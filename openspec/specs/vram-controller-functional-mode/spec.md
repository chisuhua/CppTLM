# vram-controller-functional-mode Specification

## Purpose
TBD - created by archiving change cpptlm-vram-controller-functional-integration. Update Purpose after archive.
## Requirements
### Requirement: vc-functional-mode-data-path (新)

`PcieMemoryDevice::handle_slave_port(port=0)` SHALL 在 `simulation_mode == "functional"` 且 `vram_controller_ != nullptr` 时, **委托 SDMA MEM_WRITE 流量给 VramControllerTLM 的 functional-mode 接口** (`VramControllerMode::FunctionalZeroDelay`), 而不是直 memcpy.

#### Scenario: SDMA H2D 经 VramController functional-mode 路径 (per §0.1 原则 4 目标态)

- **GIVEN** `dgpu_soc_minimal_v1.json` 含 `simulation_mode: "functional"` + 顶层 `vram_ctrl` 模块节点
- **AND** `DGpuBoard::bind_memory_backings()` 已注入 `vram_ctrl` (T23C 第 4 路激活) + 设置 mode = `FunctionalZeroDelay`
- **WHEN** SDMA H2D: `sdma.mem_out (AxiMemBundle MEM_WRITE)` 触发 `pcie_memory.handle_slave_port(port=0)`
- **THEN** 数据路径 = **`vram_ctrl.handle_functional_request(addr, size, is_write=true, data)`**
- **AND** 内部执行: controller-protocol-mapped path + 字节级 memcpy 到 `vram_storage_[addr..addr+size]`
- **AND** 返回 `MEM_WRITE_RESP` (resp.resp=0 OK) → `sdma.req_in[2]`
- **AND** controller latency = **0 cycles** (functional-mode 标识; 与 timing-mode `latency=kOperatingFrequencyHz × bandwidth_gbps^-1` 区分)

### Scenario: handle_request 返回 SLVERR 传播 (per D-AXI v1.4 B9)

- **GIVEN** VramController functional-mode, `sdma.mem_out` 触发 handle_slave_port(port=0)
- **AND** `vram_ctrl.handle_functional_request()` 返回 `non-zero` rc (e.g., OOB, SLVERR)
- **WHEN** `handle_slave_port` 检查返回值
- **THEN** 置 `resp.resp.write(1)` (SLVERR) 而**不**静默 OK
- **AND** SDMA 收到 SLVERR → 触发 retry driver (per D-AXI v1.4 B22)
- **NOTE**: per §X 关键风险 R3 缓解; 测试用例 `[vram_controller][functional][slverr]` 必须注入 1 case 验证

### Scenario: GMMU PTE fetch 改走 VramControllerTLM 专用 functional port (NVIDIA H100 dedicated memory client 模式, per v0.5 三锁叠加裁决)

- **GIVEN** `dgpu_soc_minimal_v1.json` functional-mode 配置 (含 `vram_ctrl`) + per §1.4.6 v0.5 修订 (GMMU dedicated memory client bus)
- **WHEN** GMMU 触发 PTE fetch: `gmmu.translate(iova, ...) → GMMU 异步状态机 → MasterPort(gmmu.req_out) → VramControllerTLM::handle_functional_request(pte_addr, 8, is_write=false, nullptr)`
- **THEN** 数据路径 = **GMMU dedicated memory client 直连 VramController functional port** (per NVIDIA H100 HBM Controller 通道); VramController 通过 `handle_functional_request` 拉 PTE 字节; 字节级 `memcpy(vram_storage_.get() + pte_addr, pte_buf, 8)`
- **NOTE**: 与 §0.1 Gap 矩阵 G2 + §1.4.3 强约束不矛盾 — GMMU 是 dedicated memory client (直连 VramController), **不经 pcie_memory, 不经 NOC mesh** (per v0.5 三锁叠加裁决)
- **NOTE**: 既有 D-AXI v1.4 PcieMemoryDevice `port=1` (PcieTlpBundle) **保留不修改** (per v1.0 原 H-B6 不变; 仅本 scope 不修改 pcie_memory), 仅 GMMU 这条路径走 VramController functional port (NEW dedicated client 通道)

---

### Requirement: vc-mode-configuration (新)

VramControllerTLM SHALL 提供 `set_mode(VramControllerMode)` + `mode()` accessor API, 用于在 runtime 切换 functional-mode 与 timing-mode 配置.

#### Scenario: 默认 mode = TimingCycleApproximate (backward compat)

- **GIVEN** 既有 `dgpu_soc_timing_v1.json` (timing-mode 配置, 未加 `simulation_mode` 字段)
- **WHEN** `VramControllerTLM` 实例化, 未调 `set_mode()`
- **THEN** `mode() == TimingCycleApproximate` (默认)
- **AND** `[dgpu_soc_timing]` 37 cases / 12024 assertions **保持全绿** (0 回归)

#### Scenario: minimal_v1 functional-mode 显式激活 FunctionalZeroDelay

- **GIVEN** `dgpu_soc_minimal_v1.json` 顶层 `simulation_mode: "functional"`
- **AND** `DGpuBoard::bind_memory_backings()` 检测 `simulation_mode == Functional` → 调 `vram_ctrl->set_mode(FunctionalZeroDelay)`
- **THEN** `vram_ctrl.mode() == FunctionalZeroDelay`
- **AND** 数据路径走 functional 数据路径 (per上一 Requirement)

#### Scenario: handle_request latency 在 functional-mode 下为 0 cycles

- **GIVEN** `vram_ctrl.mode() == FunctionalZeroDelay`
- **AND** `vram_ctrl.handle_functional_request(addr, size, is_write=true, data)` 调用
- **THEN** latency 字段为 **0 cycles** (zero-delay functional-mode)
- **AND** out_latency_seconds = 0
- **AND** 返回 rc = 0 (OK) 或 non-zero (SLVERR)
- **NOTE**: 与 TimingCycleApproximate mode 区分 (后者返回 `kOperatingFrequencyHz × bandwidth_gbps^-1` cycles latency)

---

### Requirement: vc-fallback-compat (新, backward compat)

`PcieMemoryDevice::handle_slave_port(port=0)` SHALL 在 `vram_controller_ == nullptr` (即 minimal_v1 JSON 无 `vram_ctrl` 节点) 时, **fallback 到 legacy 直 memcpy 路径**, 保持既有 D-AXI v1.4 v1.0 行为.

#### Scenario: backward compat fallback (无 vram_ctrl)

- **GIVEN** 既有 `dgpu_soc_minimal_v1.json` (未升级)
- **AND** `set_vram_controller()` 未调用 (或 vram_ctrl null)
- **WHEN** SDMA 触发 `pcie_memory.handle_slave_port(port=0)`
- **THEN** 数据路径 = `std::memcpy(vram_storage_.get() + addr, data, size)` (legacy D-AXI v1.4 行为)
- **AND** `[minimal_dgpu_soc]` 12 cases 保持全绿
- **NOTE**: per §0.2 DoD + 设计 H-B5 (fallback 路径是 mandatory, 不能因 Change A 而破坏既有测试)

---

### Requirement: vc-json-simulation-mode (新)

`configs/dgpu_soc_minimal_v1.json` 顶层 SHALL 新增 `simulation_mode: "functional"` 字段 (per ADR-DGPU-11 §6.4 schema), 缺省 = `"functional"` (向后兼容).

#### Scenario: simulation_mode 字段缺省为 functional

- **GIVEN** JSON schema validator 加载 minimal_v1 配置
- **WHEN** `simulation_mode` 字段缺失
- **THEN** validator 默认 = `"functional"` (H-B7 backward compat)
- **AND** VramController mode 自动 = `FunctionalZeroDelay`
- **NOTE**: 不破坏既有 v1.4 JSON (向后兼容)

#### Scenario: simulation_mode 显式声明 timing

- **GIVEN** JSON 含 `simulation_mode: "timing"`
- **WHEN** `DGpuBoard::init` 加载
- **THEN** Validator **必须 fail fast** (拒绝 functional-mode JSON 错误声明 timing); 仅 `dgpu_soc_timing_v1.json` 适合 (但该 JSON 不加此字段, 默认走 timing)
- **NOTE**: 防止 future regression JSON 字段误用

---

### Requirement: vc-test-coverage (新)

本 change SHALL 新增 `[vram_controller][functional]` Catch2 测试套, 至少 5 cases / 10 assertions (per §0.2 Acceptance).

#### Scenario: 5 cases 覆盖

```cpp
TEST_CASE("[vram_controller][functional] functional-mode path", "[vram_controller][functional]") {
    // 1 case: SDMA H2D 经 VramController functional-mode 路径, byte-level match
    // 1 case: SLVERR propagation (per 上一 Requirement Scenario)
    // 1 case: latency = 0 cycles 验证
    // 1 case: fallback 到 legacy memcpy (per backward compat)
    // 1 case: simulation_mode 缺省为 functional
}
```

#### Scenario: `[dgpu_soc_timing]` 0 回归锁

- **GIVEN** timing-mode 测试加载 `dgpu_soc_timing_v1.json`
- **WHEN** 既有 37 cases / 12024 assertions 全跑
- **THEN** 全部 PASS (per §0.2 DoD + 设计 R1 缓解)

#### Scenario: `[minimal_dgpu_soc]` 0 回归锁

- **GIVEN** minimal-mode 测试加载新 `dgpu_soc_minimal_v1.json` (含 `simulation_mode` + `vram_ctrl`)
- **WHEN** 既有 12 cases / 66 assertions 全跑
- **THEN** 全部 PASS (per §0.2 DoD)

---

### Requirement: vc-frozen-surface-zero-diff (新, Hard Invariant)

本 change SHALL 保持 4 frozen headers 零 diff + 23 ABI 签名零变更.

#### Scenario: frozen headers 零 diff

- **GIVEN** 本 change 实施完成 (T6 阶段)
- **WHEN** `git diff HEAD -- <4-frozen-headers>` 检查
- **THEN** 4 frozen headers 各自 **0 行变化**:
  - `include/abi/cpptlm_emulator.h` (per ADR-088 §D5)
  - `include/tlm/gpu/pcie_endpoint_tlm.h`
  - `include/tlm/gpu/pcie_display_device.hh`
  - `include/tlm/pcie/pcie_bundles_tlm.hh`
  - `include/tlm/pcie/pcie_endpoint_ip.hh`

#### Scenario: 23 ABI 签名零变更

- **GIVEN** `git diff HEAD -- include/abi/` 检查
- **THEN** output 为空
- **NOTE**: per ADR-DGPU-08 §D5 + AGENTS.md H3 不动约束; VramController 配置在 abi 层**之后** (DGpuBoard level)

---

### Requirement: vc-docs-sync (新)

本 change SHALL 同步更新 3 类文档 (per §0.2 DoD).

#### Scenario: 实施笔记 + architecture §0.2 sync + AGENTS.md KEY INVARIANTS 锁定

- **GIVEN** Phase 0 T6 文档同步阶段
- **WHEN** 实施完成 (T5 GREEN)
- **THEN** 同步更新 3 类文档:
  - 新增 `docs/implementation/2026-10-09-vram-controller-functional-integration.md` (~50 行)
  - `docs/designs/dgpu-soc/architecture.md` §0.2 Gap 矩阵 G3 行 ✅ 修订
  - `AGENTS.md` KEY INVARIANTS 加 VramController functional-mode 锁定

#### Scenario: openspec validate PASS

- **GIVEN** Phase 6 全量验证阶段
- **WHEN** `openspec validate cpptlm-vram-controller-functional-integration --strict`
- **THEN** 0 errors, ≥0 warnings (per §0.1 DoD)

---

