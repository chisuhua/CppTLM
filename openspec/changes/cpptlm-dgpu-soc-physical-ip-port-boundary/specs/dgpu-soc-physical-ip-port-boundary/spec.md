# Spec — cpptlm-dgpu-soc-physical-ip-port-boundary

> **Change**: `cpptlm-dgpu-soc-physical-ip-port-boundary` (Phase 0+, P0 架构级)
> **配套**: [`../../proposal.md`](../../proposal.md) · [`../../design.md`](../../design.md) · [`../../tasks.md`](../../tasks.md)
> **修复**: SoC 内部 IP 端口按真实 dGPU 物理 IP 边界切型 (PcieTlpBundle ↔ NoCFlitBundle ↔ CacheReqBundle)
> **估时**: 14-22d

---

## ADDED Requirements

### Requirement: dgpu-soc-port-typing-physical-ip-boundary

SoC 内部 IP 端口类型**SHALL**按真实 dGPU 物理 IP 边界切型,不可继续使用 MVP 临时方案的"全 PcieTlpBundle"。PcieTlpBundle 只存在于 PCIe Controller ↔ 板级(host)边界。

**Bundle 类型分层**:
- **PCIe 板级接口** (host ↔ chip): `PcieTlpBundle` (frozen by PcieEndpointIP 17 端口)
- **SoC 内部 NoC 接口** (SDMA ↔ SocInternalNoc ↔ VramController, GMMU ↔ VramController, fabric ↔ VRAM): `NoCFlitBundle` (`include/bundles/noc_bundles_tlm.hh:26-50`, v2.1 决策 16 字段)
- **SoC 内部 AXI 接口** (其他 SoC 内部 IP 互连,寄存器访问,non-NoC bulk): `AxiBundle` / `AxiStreamBundle` 系列
- **LWC 总线接口**: `CrBundle` / `DoorbellBundle` / `MsixBundle` (per Change B + Change E)

**端口切型表** (per proposal.md "What Changes"):

| 模块 | 端口 | 旧类型 | 新类型 |
|------|------|--------|--------|
| `SdmaEngineTLM` | `mem_in` (port 1) | `PcieTlpBundle` | `NoCFlitBundle` |
| `SdmaEngineTLM` | `mem_out` (port 2) | `PcieTlpBundle` | `NoCFlitBundle` |
| `SdmaEngineTLM` | `desc_in/host_out/done_out` (port 0/3/4) | `PcieTlpBundle` | `PcieTlpBundle` (保留) |
| `VramControllerTLM` | `req_in_/req_out_/resp_in_/resp_out_` (上游) | `CacheReqBundle/CacheRespBundle` | `NoCFlitBundle` |
| `VramControllerTLM` | `req_internal/resp_internal` (内部) | `CacheReqBundle/CacheRespBundle` | `CacheReqBundle/CacheRespBundle` (保留) |
| `GmmuTLM` | `req_out_/resp_in_` (PTE fetch) | `PcieTlpBundle` | `NoCFlitBundle` |
| `GpuInternalFabric` | ChStream 端口字段 | 0 (callback) | N 组 ChStreamPort |
| `SocInternalNoc` | ChStream 端口字段 | 0 (callback) | N 组 ChStreamPort (NoCFlitBundle) |

#### Scenario: SDMA 端口切型

- **WHEN** `SdmaEngineTLM::mem_out` 发出 PcieTlpBundle (SDMA → VramController bulk DMA)
- **THEN** StreamAdapter `SDMA_PcieTlp_To_NocAdapter` 自动转换为 NoCFlitBundle
- **AND** 字段映射: `pcie.tag → noc.transaction_id`, `pcie.addr → noc.address`, `pcie.data → noc.data`, `pcie.length_bytes → noc.size`, `pcie.kind = MEM_WRITE → noc.is_write = true`
- **AND** 推送给 `SocInternalNoc::req_in_`
- **AND** 经过 mesh 路由到 `VramControllerTLM::req_in_`
- **AND** 反向响应经 `SDMA_Noc_To_PcieTlpAdapter` 转回 PcieTlpBundle → `sdma.mem_in`

#### Scenario: VramController 上游 NoC 化

- **WHEN** `VramControllerTLM::req_in_` 收到 NoCFlitBundle (从 SocInternalNoc mesh 路由过来)
- **THEN** StreamAdapter `VramCtrl_Noc_To_AxiAdapter` 自动转换为 CacheReqBundle
- **AND** 字段映射: `noc.address → cache.addr`, `noc.data (低 32 位) → cache.data`, `noc.vc_id → cache.vc_id`, `noc.is_write → cache.is_write`
- **AND** 内部 `MemoryTLM::req_internal` 协议真实处理 (行缓冲 + 协议状态机)
- **AND** 响应经 `VramCtrl_Axi_To_NocAdapter` 转回 NoCFlitBundle (flit_category=RESPONSE, vc_id=1) → `resp_out_`

#### Scenario: GmmuTLM 端口切型

- **WHEN** `GmmuTLM::translate(iova)` 需要 fetch PTE (per Change C T2 既有)
- **THEN** `gmmu.req_out_` (NoCFlitBundle) 推送给 `SocInternalNoc::req_in_` (NoCFlitBundle)
- **AND** 经 mesh 路由到 `VramControllerTLM::req_in_`
- **AND** `vram_ctrl_->handle_functional_request(pte_addr, 8, false, nullptr)` 直 memcpy pte_buf_ → COMPLETE → paddr
- **AND** 响应经 `gmmu.resp_in_` (NoCFlitBundle) 收回

#### Scenario: 保留 programmatic bridge fallback

- **WHEN** 实施期某测试依赖 programmatic bridge (per Change C §6 Deviation 4 先例)
- **THEN** `SocInternalNoc` + `GpuInternalFabric` 的 `set_*_target` 方法**保留**作 fallback
- **AND** 标记 `[[deprecated("use ChStream JSON connection")]]`
- **AND** 不影响 [pcie-memory] 17 cases + [realip][fabric] 等 characterization tests

---

### Requirement: dgpu-soc-simmodule-chstream-ization

`SocInternalNoc` + `GpuInternalFabric` **SHALL** ChStream 化,允许 JSON `connections[]` 声明,取消 programmatic bridge。

**关键实现** (per `include/core/sim_module.hh:132-161` + `include/tlm/AGENTS.md` "SimModule vs SimObject 端口能力对照表"):

- **WHEN** SimModule 派生类要 ChStream 化
- **THEN** 在类中加 N 组 ChStreamPort 字段 (req_in + req_out + resp_in + resp_out)
- **AND** `set_stream_adapter()` override (单端口 + 多端口 overload)
- **AND** `num_ports() const override` 返回 N
- **AND** JSON `outputs[]` / `inputs[]` 暴露内部子模块端口(可选, 顶层用)
- **AND** JSON `connections[]` 可写 `src: "sim_module.port_idx"`, `dst: "chstream_module.port_idx"`

#### Scenario: SocInternalNoc ChStream 化

- **WHEN** `SocInternalNoc` 完成 ChStream 化改造
- **THEN** 类内有 N 组 `cpptlm::InputStreamAdapter<NoCFlitBundle> req_in_[NUM_PORTS]` 等字段
- **AND** `set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override` 实现
- **AND** `unsigned num_ports() const override { return NUM_PORTS; }`
- **AND** JSON `connections[]` 可写 `{ "src": "sdma.2", "dst": "soc_noc.0" }` (per Phase 0 D-AXI 评审需求)
- **AND** 取消 4 个 `set_pcie_target/set_sdma_target/set_gmmu_target/set_vram_target` programmatic bridge (保留作 fallback)
- **AND** 保留 `route_flit()` mesh 路由 + `vc_counts_[4]` + `flits_dispatched_` 计数 + `send_request/response` API

#### Scenario: GpuInternalFabric ChStream 化

- **WHEN** `GpuInternalFabric` 完成 ChStream 化改造
- **THEN** 类内有 4 组 ChStreamPort 字段
- **AND** `Fabric2VramCtrlBundle` 8 字段**保留** (内部 IP-间 wire, 不动 BUNDLE_FIELD_WIDTH 约束)
- **AND** `set_stream_adapter()` override
- **AND** `num_ports() const override { return 4; }`
- **AND** JSON `connections[]` 可写 `{ "src": "fabric.0", "dst": "soc_noc.2" }`
- **AND** 取消 `set_vram_target/set_fabric_source` programmatic bridge (保留作 fallback)

---

### Requirement: dgpu-soc-wire-format-adapter

本 change SHALL 新增 4 个 wire-format adapter,处理 SoC 内部 NoC 化过程中的 Bundle 类型转换:
- `SDMA_PcieTlp_To_NocAdapter` (PcieTlpBundle → NoCFlitBundle, SdmaEngineTLM `mem_out` 出口侧)
- `SDMA_Noc_To_PcieTlpAdapter` (NoCFlitBundle → PcieTlpBundle, SdmaEngineTLM `mem_in` 入口侧, **deprecated** by 后续 [cpptlm-dgpu-soc-control-hub](../../../cpptlm-dgpu-soc-control-hub/) change — 该 change 提议 SDMA 8 端口切型, PcieTlpBundle 端口删除)
- `VramCtrl_Noc_To_AxiAdapter` (NoCFlitBundle → CacheReqBundle, VramControllerTLM 入口侧)
- `VramCtrl_Axi_To_NocAdapter` (CacheRespBundle → NoCFlitBundle, VramControllerTLM 出口侧)

**超集关系** (per [cpptlm-dgpu-soc-control-hub/proposal.md §6](../proposal.md)): 本 change (5 模块端口切型 + 4 wire-format adapter + 2 SimModule ChStream 化) **SHALL 被 superset by** `cpptlm-dgpu-soc-control-hub` (新增 ControlHub + 移除 PcieMemoryDevice + SDMA 独立 translate + 8 端口切型, 含本 change 全部内容 + 3 个新 Bundle + 13 条 JSON connection)。实施前建议合并 (per [control-hub design.md §10.3](../design.md))。

- **`SDMA_PcieTlp_To_NocAdapter`**: PcieTlpBundle → NoCFlitBundle (SDMA mem_out 出口侧)
- **`SDMA_Noc_To_PcieTlpAdapter`**: NoCFlitBundle → PcieTlpBundle (SDMA mem_in 入口侧)
- **`VramCtrl_Noc_To_AxiAdapter`**: NoCFlitBundle → CacheReqBundle (VramController req_in_ 入口侧)
- **`VramCtrl_Axi_To_NocAdapter`**: CacheRespBundle → NoCFlitBundle (VramController resp_out_ 出口侧)

**注册位置**: `include/chstream_register.hh` `ChStreamAdapterFactory`

**字段映射契约** (per `design.md` §3 详细):
- `transaction_id` 16-bit/64-bit 双向映射 (PCIe tag ↔ NoC transaction_id)
- `address` 直接透传
- `data` 64-bit 透传 (CacheReqBundle 32-bit 截断, MSB 损失需 per-trace 验证)
- `is_write/is_read` 互转
- `flit_category` REQ/RESP 区分
- `vc_id` 0=REQ, 1=RSP, 2=DAT, 3=SNP

#### Scenario: SDMA 出口 PcieTlp → NoC 转换

- **WHEN** `SDMA_PcieTlp_To_NocAdapter::convert()` 被调用 (PcieTlpBundle mem_write 输入)
- **THEN** 输出 NoCFlitBundle:
  - `transaction_id = pcie.tag` (直接映射, 16-bit 透传)
  - `src_node = SDMA_NODE_ID` (硬编码 SDMA 节点号)
  - `dst_node = (pcie.addr >= VRAM_BASE) ? VRAM_NODE_ID : PCIE_NODE_ID` (路由决策)
  - `address = pcie.addr` (透传)
  - `data = pcie.data` (64-bit 透传)
  - `size = pcie.length_bytes`
  - `vc_id = 0` (REQ → VC0)
  - `flit_type = (pcie.length_bytes > 8) ? FLIT_HEAD_TAIL : FLIT_HEAD` (multi-beat 拆分)
  - `flit_count = (pcie.length_bytes + 7) / 8` (8-byte flit 对齐)
  - `flit_category = CATEGORY_REQUEST`
  - `is_write = true` (mem_write)
  - `error_code = 0`

#### Scenario: VramController 入口 NoC → Cache 转换

- **WHEN** `VramCtrl_Noc_To_AxiAdapter::convert()` 被调用 (NoCFlitBundle REQ 输入)
- **THEN** 输出 CacheReqBundle:
  - `addr = noc.address`
  - `data = static_cast<uint32_t>(noc.data & 0xFFFFFFFF)` (低 32 位截断, MSB 损失 documented)
  - `vc_id = noc.vc_id`
  - `is_write = noc.is_write`
  - `size = noc.size`
- **AND** 内部 MemoryTLM 协议真实处理

#### Scenario: Adapter 注册

- **WHEN** `ChStreamAdapterFactory::registerAdapter()` 被调用
- **THEN** 4 个 adapter 注册到 factory (per `chstream_register.hh`)
- **AND** JSON `connections[]` 声明 `src: "sdma.2", dst: "soc_noc.0"` 时, factory 自动选择 `SDMA_PcieTlp_To_NocAdapter` (per source bundle type + dest bundle type 匹配)
- **AND** 编译期 `BUNDLE_FIELD_WIDTH` assert 锁字段宽度

---

### Requirement: dgpu-soc-json-topology-direct-connect

`configs/dgpu_soc_minimal_v1.json` 顶层 `soc.connections[]` **SHALL**从 1 条扩展到 4-5 条,真正反映完整 wire 拓扑,符合 `dgpu_board_v1.json:80-94` 模式。

**超集关系** (per [cpptlm-dgpu-soc-control-hub/proposal.md §6](../proposal.md)): 本 change 提议 4-5 条 connection, **SHALL 被 superset by** `cpptlm-dgpu-soc-control-hub` 提议 13 条 connection (1 条 → 4-5 条 → 13 条 superset 关系)。

**目标 JSON 拓扑** (per `design.md` §4.2):

```json
"connections": [
  { "src": "sdma.2",       "dst": "soc_noc.0",   "latency": 1 },   // SDMA → NoC
  { "src": "soc_noc.1",    "dst": "vram_ctrl.0", "latency": 1 },   // NoC → VRAM
  { "src": "fabric.0",     "dst": "soc_noc.2",   "latency": 1 },   // fabric → NoC
  { "src": "gmmu.0",       "dst": "soc_noc.3",   "latency": 1 },   // GMMU → NoC (GMMU dedicated client)
  { "src": "soc_noc.4",    "dst": "pcie_memory.0", "latency": 1 }  // NoC → PCIeMemory (backward compat)
]
```

#### Scenario: JSON 拓扑扩展

- **WHEN** `ModuleFactory::instantiateAll()` 加载 `dgpu_soc_minimal_v1.json`
- **THEN** 5 条 `connections[]` 全部解析成功
- **AND** 每条 connection 触发 `ChStreamAdapterFactory::createAdapter()` 自动选 adapter
- **AND** 5 个 StreamAdapter 注入到对应模块 (per `set_stream_adapter()` override)
- **AND** 与 `dgpu_board_shell.cc::bind_memory_backings()` 装配结果**一致**(cross-validation)

#### Scenario: programmatic bridge 退役

- **WHEN** `DGpuBoard::bind_memory_backings()` 完成本 change 装配
- **THEN** 取消 4 个 `soc_noc->set_*_target()` 调用
- **AND** 取消 `fabric->set_vram_target()` + `vram_ctrl->set_fabric_source()` 调用
- **AND** 保留 `pcie_mem->set_vram_controller()` (PcieMemoryDevice 仍作 PCIe 外观层)
- **AND** 保留 `gmmu_->set_vram_target()` (programmatic fallback, Gap I 兜底)
- **AND** `bind_memory_backings()` 从 ~30 行简化到 ~5 行

#### Scenario: 0 回归锁

- **WHEN** 实施期所有 module 端口切型 + adapter 注入 + ChStream 化完成
- **THEN** 0 回归 (per Phase 0+1 实测基线):
  - `[pcie]` 430/36672 0 失败
  - `[gmmu]` 25/5273 0 失败
  - `[noc]` 64/304 0 失败
  - `[sdma]` 68/12945 0 失败
  - `[minimal_dgpu_soc]` 24→28/170→190 (新增 4 E2E 直连 cases) 0 失败
  - `[dgpu_soc_timing]` 37/12024 0 失败 (timing-mode 不动)
  - `[driver_visible]` 4/40 0 失败
  - `[pcie-memory]` 17/48 0 失败
- **AND** 新增 22 cases (6 adapter + 5 端口 + 4 E2E + 7 misc) 全过
- **AND** frozen headers 0 diff + 23 ABI 0 变更 (per ADR-DGPU-08)
- **AND** `docs_sync_check.sh --strict` exit 0

---

### Requirement: dgpu-soc-physical-ip-port-boundary-deferred

本 change SHALL 明确锁定以下 NOT in scope 项 (per proposal.md "What Changes" 末 + design.md §8):

- **timing-mode** (`dgpu_soc_timing_v1.json`): SHALL 不动 (per H-C6 + ADR-DGPU-11, 范围锁定 minimal_v1 functional-mode, timing-mode 走 dgpu_soc_timing_v1 独立 SoC 拓扑, 与本 change 正交)
- **23 ABI 函数** (`include/abi/cpptlm_emulator.h`): 零 diff (frozen)

- **23 ABI 函数** (`include/abi/cpptlm_emulator.h`): 零 diff (frozen)
- **4 frozen headers** (`pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h`): 零 diff
- **PcieEndpointIP 内部 17 端口**: 保留 PcieTlpBundle (frozen by Phase 4 桥接)
- **PCIe Controller ↔ host 边界**: 保留 PcieTlpBundle (board-level)
- **timing-mode** (`dgpu_soc_timing_v1.json`): 不动 (per H-C6 `mem_view_/translate_timing` TLB 路径)
- **LWC 总线** (`CrBundle` / `DoorbellBundle` / `MsixBundle`): 不动 (per Change B + Change E)
- **PCI 板级 PcieEndpointIP ↔ host 通信**: 不动

#### Scenario: deferred scope 严格隔离

- **WHEN** 实施期本 change 触及 deferred scope
- **THEN** 自动触发 `frozen headers 0 diff` 检查失败
- **AND** `git diff include/abi/cpptlm_emulator.h` 必须为 0 行
- **AND** `git diff include/tlm/gpu/pcie_endpoint_tlm.h` 必须为 0 行
- **AND** `git diff include/bundles/pcie_bundles_tlm.hh` 必须为 0 行
- **AND** timing-mode 测试 `[dgpu_soc_timing]` 37/12024 必须 0 失败

---

## 修改的现有 Specs

- `openspec/specs/sdma-engine-tlm/spec.md`: 端口类型约束 (3 端口 NoC + 2 端口 PCIe)
- `openspec/specs/vram-controller-functional-mode/spec.md`: 上游 NoC 化 (per Change A 既有 + 本 change 扩展)
- `openspec/specs/gmmu-pte-wf/spec.md`: 端口切型 (per Change C 既有 + 本 change 扩展)
- `openspec/specs/soc-internal-noc/spec.md`: ChStream 化 (per e1 v1.1 archived + 本 change 扩展)

## 修改的现有 ADRs

- ADR-DGPU-06 (axi-mem-bundle-boundary): Status Update 2026-10-12 追加, 标记"已落地"
- 其他 ADR: 不动
