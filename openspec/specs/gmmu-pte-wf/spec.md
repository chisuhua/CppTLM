# gmmu-pte-wf Specification

## Purpose
TBD - created by archiving change cpptlm-gmmu-pte-bus-correction. Update Purpose after archive.
## Requirements
### Requirement: gc-gmmu-wire-format-correction (Gap I 修复, 新)

`include/tlm/gpu/gmmu_tlm.hh` SHALL 将 GMMU 的 wire-format 从 `PcieTlpBundle` 切换到 `AxiMemBundle` (per §0.1 Gap 矩阵 Gap I 修复).

#### Scenario: Gap I 修复 - 头文件 wire-format 注释对齐 (per AC-GC-1.1)

- **GIVEN** 头文件 `include/tlm/gpu/gmmu_tlm.hh` 编译通过
- **WHEN** grep 检查 wire-format 注释
- **THEN** 注释明确写 `wire-format: AxiMemBundle` (chip-internal AXI 协议), 与 JSON 配置对齐
- **NOTE**: per §0.1 原则 3 + Gap I 修复; 真实 RTL 模式 (NVIDIA H100 GMMU dedicated memory client bypass mesh NoC, ARM CMN-700 + SMMUv3 DVM 独立通道)

#### Scenario: GMMU MasterPort 输出走 AxiMemBundle (per AC-GC-1.2)

- **GIVEN** GMMU PTE fetch 触发: `gmmu.translate(iova, size, &paddr)` → 当 mem_view_ 注入时走 sync path; **当 mem_view_ == nullptr 异步 path**: state IDLE → 发 AxiMemBundle MEM_READ (req_out_)
- **AND** PcieMemoryDevice::handle_slave_port(port=1) 接收 (chip-internal AXI, 保留 D-AXI v1.4 路径)
- **THEN** byte-level PTE 写入 pte_buf_, state IDLE → WAIT → COMPLETE
- **AND** translate 返回 paddr

#### Scenario: GMMU 不经 mesh SocInternalNoc (per Gap I: dedicated memory client)

- **GIVEN** `dgpu_soc_minimal_v1.json` 含 `soc_internal_noc` 节点
- **WHEN** GMMU PTE fetch 触发
- **THEN** **GMMU 不经 mesh NoC**: 走 `gmmu.0 → pcie_memory.1` 直接路径 (per H-C2 一主一辅不同路径)
- **NOTE**: per NVIDIA H100 GMMU 模式 (dedicated memory client bypass mesh NoC)

---

### Requirement: gc-sdma-translate-via-noc (Gap G1 修复, 新)

`SDMA` SHALL 改成通过 **SocInternalNoc 触发 GMMU 翻译**, 而非直接 `set_translate_cb(lambda)`.

#### Scenario: 删 set_translate_cb lambda (per Gap G1 修复)

- **GIVEN** `dgpu_board_shell.cc::bind_memory_backings` 实施完毕
- **THEN** 字符串 `"sdma->set_translate_cb"` 或 `"set_translate_cb"` **0 处**
- **AND** SDMA 调 `soc_internal_noc->send_request(dst=GMMU, ...)` 触发翻译
- **AND** translate 响应经 `soc_internal_noc->send_response` 回 SDMA
- **NOTE**: 删 `set_translate_cb` 是 breaking change; 既测试需要重连到 NOC-driven path

#### Scenario: SDMA translate 经 NOC path 0 回归 (per Phase 1.8 0 回归锁)

- **GIVEN** SDMA H2D 启动 (BAR2 mmio_write 触发)
- **AND** 需要 GMMU translate 才能确定 backing offset
- **WHEN** sim loop 跑
- **THEN** translate 经 NOC 完成 (1+ sim ticks), SDMA 收到响应后开始 DMA
- **AND** `[sdma]` 既有测试保持全绿 (路径延迟引入 1 sim tick, 既有测试 pump 多次)

---

### Requirement: gc-soc-internal-noc-data-flow (Gap G2 修复, 新)

`SocInternalNoc` SHALL 作为 `SDMA` ↔ `VRAM` 的 **DataNoC** 唯一 mesh 路径, 启用 e1 v1.1 设计的 `route_flit()` 实际推送 + `send_request/response` API + `vc_counts_[4]` 计数器.

#### Scenario: SocInternalNoc route_flit 实际推送 (per e1 v1.1 AC-E1-3.1)

- **GIVEN** `SocInternalNoc` 已 set 4 target (PCIE/SDMA/GMMU/VRAM); SDMA 通过 adapter 接 send_request
- **WHEN** SDMA H2D 触发 `soc_internal_noc->send_request(NoCFlitBundle dst=VRAM)`
- **THEN** `route_flit()` 实际推 `target->accept(flit)` (vram_noc_adapter)
- **AND** `vc_counts_[0]` (REQ) +1; `flits_dispatched_` +1
- **NOTE**: e1 v1.1 已定设计 (SocInternalNoc 4 NODE_ID + 16 字段 + vc_counts_[4] + send_request/response), 本 change 实施化

#### Scenario: SDMA→VRAM 经 NOC byte-level 数据完整性 (per AC-E1 数据完整性)

- **GIVEN** 测试用 backing = 8GB vram_storage_
- **WHEN** SDMA 发 100 batches × 4KB H2D (per `[sdma][h2d]` 既有测试)
- **THEN** vram_storage_ 字节级一致性 (driver backdoor read 验)
- **AND** 总 transactions 数 = `flits_dispatched_` (NOC 计数准确)

#### Scenario: GMMU bypass mesh NOC (per Gap I: GMMU dedicated memory client)

- **WHEN** runtime 检查 NOC traffic (per `[noc][gmmu-bypass]` test 中 instrument)
- **THEN** GMMU 不触发 NOC send_request; NOC 全程 0 GMMU traffic; GMMU 走 chip-internal AXI 独立通道
- **NOTE**: 这是 Gap I + Gap G2 的**关键边界**: 一主一辅两条路, GMMU 永远是辅路 (不经 NOC)

---

### Requirement: gc-noc-vc-counters (per e1 v1.1 AC-E1-3.5)

`SocInternalNoc` SHALL 维护 `vc_counts_[4]` 计数器按 `flit_category` 分桶 (per ARM CHI 4 VC 物理对应).

#### Scenario: 4 VC 计数器按 flit_category 分桶 (per ARM CMN-700)

- **GIVEN** soc_noc 4 target set
- **WHEN** route_flit 触发 4 个不同 flit_category:
  - REQ flit (category=0) → vc_counts_[0]++
  - RESP flit (category=1) → vc_counts_[1]++
  - DAT flit (category=2) → vc_counts_[2]++
  - CREDIT flit (category=3) → vc_counts_[3]++
- **THEN** 4 个 vc_counts_ 分别 +1
- **AND** `vc_count(unsigned vc_id)` 返回对应值

---

### Requirement: gc-frozen-surface-zero-diff (Hard Invariant)

本 change SHALL 保持 23 ABI 零 diff + 5 frozen headers 零 diff (per ADR-DGPU-08 + AGENTS.md H3). 修改范围**仅限于**: GmmuTLM wire-format, SocInternalNoc route_flit 升级, DGpuBoard bind_memory_backings lambda 删除, JSON connection 调整.

#### Scenario: 23 ABI 签名零变更

- **GIVEN** `git diff HEAD -- include/abi/` 检查
- **THEN** output 为空 (per ADR-088 §D5)

#### Scenario: 5 frozen headers 零 diff (per AGENTS.md 锁定)

- `include/abi/cpptlm_emulator.h`: 0 diff
- `include/tlm/gpu/pcie_endpoint_tlm.h`: 0 diff
- `include/tlm/gpu/pcie_display_device.hh`: 0 diff
- `include/tlm/pcie/pcie_bundles_tlm.hh`: 0 diff
- `include/tlm/pcie/pcie_endpoint_ip.hh`: 准 frozen (AddChang B 准 frozen, additive only); 本 change 不动

#### Scenario: GMMU translate 异步路径 back-compat

- **GIVEN** `gmmu.translate(iova, size, &paddr)` 公共 API
- **WHEN** mem_view_ 注入 (现有 timing-mode `translate_timing()` 路径)
- **THEN** sync 路径 (timing-mode) 保持行为不变
- **AND** mem_view_ 注入 (functional mode) 走 async chip-internal AXI PTE fetch 路径

---

### Requirement: gc-test-coverage (新)

本 change SHALL 新增 `[noc]` Catch2 测试套, 至少 18 cases / 30+ assertions.

#### Scenario: 18 cases 覆盖

```cpp
TEST_CASE("[noc][unit]") {
  // 5 cases: SocInternalNoc 基础 (route_flit + vc_counts + send_request/response + 4 target routing)
  // 1 case: unknown target → dropped
  // 1 case: 32 port cap (与 Change B 同步)
}

TEST_CASE("[noc][gmmu-bypass]") {
  // 5 cases: GMMU PTE fetch 走 chip-internal AXI, 不进 mesh NOC
}

TEST_CASE("[noc][sdma-bulk-dma]") {
  // 5 cases: SDMA→NOC→VRAM byte-level 数据完整性
}

TEST_CASE("[noc][deadlock-fuzz]") {
  // 3 cases: random flit injection, ≤ 1000 cycles resolve
}
```

#### Scenario: 既有 4 套件 0 回归锁

- `[sdma]` (既有): ✅ 保持 Pass (SDMA translate 路径改 NOC, 测试 sim pump 兼容)
- `[pcie]` (430 cases / 36672 assn): ✅ 保持 (PCIe EP 行为不变; GMMU wire-format 修订可能引起少数测试重审)
- `[minimal_dgpu_soc]` (12 cases / 66 assn): ✅ 保持 (driver 视角不变)
- `[dgpu_soc_timing]` (37 cases / 12024 assn): ✅ 保持 (timing-mode 独立)

---

