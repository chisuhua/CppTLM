# sriov-vf-pool Spec — Phase 4 (SR-IOV VF Pool + FLR + Completion + ARI)

> **配套**: [`proposal.md`](../proposal.md)
> **父 spec**: [`pcie-ip-microarch/spec.md`](../../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/specs/pcie-ip-microarch/spec.md) (umbrella)
> **Oracle 评审 C1-C5 补交**: Phase 4 原交付缺 spec.md,本文件补齐 7 个 ADDED Requirement 的 delta 格式

## Purpose

本 spec 定义 Phase 4 交付的 7 个 ADDED Requirement—— `sriov-vf-pool-routing` (17 端口 stream_id 分发)、`per-vf-config-space` (per-VF Config Space 独立)、`per-vf-msix-table` (per-VF MSI-X table 独立)、`ari-routing` (8-bit Function routing-id 解析)、`flr-semantics` (FLR PF 全复位 + VFx 仅对应状态)、`completion-tracking` (NP↔CplD trans_id 关联)、`endpoint-ip-17ports` (新类 PcieEndpointIP 17 端口 composition 集成)。

## ADDED Requirements

### Requirement: sriov-vf-pool-routing

PcieSriovVfPool **SHALL** 将 17 个 stream_id(0..16) 分发到对应 PF/VF 内部状态：
- stream_id=0  → PF (slot 0)
- stream_id=1..16 → VF0..VF15 (slot 1..16)
错误 stream_id(≥17) 拒绝(dispatch 返 false)。

理由:Oracle C1/C3 评审要求 VF Pool 路由表明确定义 stream_id↔PF/VF 映射,且拒绝越界 ID。

#### Scenario: 17 端口 stream_id 正确路由到对应 PF/VF

- **WHEN** 对 stream_id=0(PF),1(VF0),16(VF15) 调用 `dispatch_tlp(sid, CFG_WRITE)`
- **THEN** `dispatch_tlp` 返回 true 且各自写入对应 `config_of(sid)`
- **AND** stream_id=17(越界) 调用 `dispatch_tlp` 返回 false,无副作用

#### Scenario: 多端口并发路由互不干扰

- **WHEN** 并发写 stream_id=0,1,2,16 不同 config offset
- **THEN** 各 PF/VF 独立保存,互不覆盖

### Requirement: per-vf-config-space

PcieSriovVfPool **SHALL** 为每个 VF(1..16)和 PF(0)提供独立的 4KB Config Space。PF/VF 间 Config Space 写入互不影响。

理由:PCIe SR-IOV spec 要求每个 VF 拥有独立 Config Space(包括 Command/Status/Capabilities/BAR/ARI Capability 等)。

#### Scenario: VF0 写 Config Space 不影响 VF1/PF

- **WHEN** `dispatch_tlp(1, CFG_WRITE offset=0x04 value=0xCAFE)` 写 VF0
- **THEN** `config_of(1).read(0x04) == 0xCAFE`
- **AND** `config_of(0).read(0x04) == 0x10`(PF 默认 Command 位)
- **AND** `config_of(2).read(0x04) == 0x10`(VF1 默认)

### Requirement: per-vf-msix-table

PcieSriovVfPool **SHALL** 为每个 VF(1..16)和 PF(0)提供独立的 MSI-X Table。MSI-X pending 状态 per-VF 隔离。

理由:PCIe SR-IOV spec 要求每个 VF 独立 MSI-X 中断向量表(向量数可配),pending 状态需独立管理。

#### Scenario: VF0 MSI-X pending 独立

- **WHEN** `dispatch_msix(1, 3)` 置 VF0 vector 3 pending
- **THEN** `msix_pending(1, 3) == true`
- **AND** `msix_pending(0, 3) == false`(PF 不受影响)
- **AND** `msix_pending(2, 3) == false`(VF1 不受影响)

### Requirement: ari-routing

AriRouter **SHALL** 支持 ARI(Alternative Routing-ID Interpretation)8-bit Function Number 解析：
- ARI enabled: routing-id[7:0] = Function Number, 0=PF, 1..16=VF0..VF15
- ARI disabled: 仅 Function 0(PF)有效,其余路由到 PF
越界 Function(>16)保守 fallback 到 PF。

理由:PCIe ARI spec 要求 SR-IOV 设备支持 8-bit Function Number,替代传统 16-bit BDF,支持 >8 个 Function。

#### Scenario: ARI enabled 时 routing-id 低 8 位解析 Function Number

- **WHEN** `set_ari_enabled(true)` 后调用 `route_id_to_vf(0x0001)`(fn=1)
- **THEN** 返回 1(VF0 slot)
- **AND** `route_id_to_vf(0x0010)`(fn=16) 返回 16(VF15 slot)
- **AND** `route_id_to_vf(0x0000)`(fn=0) 返回 0(PF slot)

#### Scenario: ARI disabled 时仅 Function 0 命中 PF

- **WHEN** `set_ari_enabled(false)` 后调用 `route_id_to_vf(0x1234)`(任意 fn)
- **THEN** 总是返回 0(PF slot)

### Requirement: flr-semantics

PcieSriovVfPool **SHALL** 实现简化 FLR(Function Level Reset,per Q10)：
- `flr_pf()`: 全 17 slot(PF+16 VF)Config Space/MSI-X/FC/seq# 全部复位,并清零 ARI `ari_enabled`
- `flr_vf(vf_id)`: 仅复位对应 VF(slot vf_id,合法范围 1..16),PF + 其他 VF 不受影响
- `flr_vf(0)` 必须拒绝(vf_id=0 是 PF,非 VF),PF 状态保持不变

理由:Oracle C1 发现 `flr_vf(0)` 误复位 PF; Oracle C2 发现 `flr_pf()` 未复位 ARI 状态。FLR 语义需严格匹配 PCIe spec:PF FLR 复位所有 VF,VF FLR 仅复位自身。

#### Scenario: flr_pf() 全 17 slot 复位 + ARI 清零

- **WHEN** 17 slot 全写入非默认值 + 开启 ARI `set_ari_enabled(true)` → `flr_pf()`
- **THEN** 17 slot Config Space 均回默认(Command=0x10)
- **AND** 17 slot MSI-X pending 均清零
- **AND** 17 slot FC token bucket 均回 capacity
- **AND** 17 slot seq# 均归 0
- **AND** `ari_router().ari_enabled() == false`

#### Scenario: flr_vf(x) 仅复位对应 VF,PF+其他 VF 不动

- **WHEN** VF0(1)写 Config 0xDEAD,VF1(2)写 0xBEEF,PF(0)写 0xCAFE → `flr_vf(1)`
- **THEN** `config_of(1).read() == 0x10`(VF0 复位)
- **AND** `config_of(2).read() == 0xBEEF`(VF1 保持)
- **AND** `config_of(0).read() == 0xCAFE`(PF 保持)

#### Scenario: flr_vf(0) 拒绝,PF 状态保持

- **WHEN** PF(0) Config 写 0xDEAD + seq#=5 + MSI-X pending → `flr_vf(0)`
- **THEN** `config_of(0) == 0xDEAD`(未复位)
- **AND** `seq_of(0) == 5`
- **AND** `msix_pending(0) == true`

### Requirement: completion-tracking

CompletionTracker **SHALL** 实现 NP 请求(trans_id)与 CplD 匹配：
- `register_np(vf_id, trans_id)`: 登记 outstanding NP 请求(CFG_READ/MMIO_READ/MEM_READ)
- `complete(vf_id, trans_id, cpl_data)`: CplD 到达时匹配并返回数据
- 多 outstanding NP 请求各自独立匹配(乱序完成)
- 溢出策略:per-VF outstanding map 容量上限(DEFAULT_CAPACITY=64),N+1 拒绝新发出

理由:PCIe 要求 Non-Posted 请求(Memory/Cfg Read)有对应 Completion。Oracle Q12 要求 trans_id 关联匹配,溢出保护防内存泄漏。

#### Scenario: NP 请求 register + CplD complete 返回数据

- **WHEN** `register_np(1, 0x100)` → `complete(1, 0x100, CplData{0xCAFEBABE, 4})`
- **THEN** `complete` 返回 true,`completed_count(1) == 1`,`outstanding_count(1) == 0`

#### Scenario: 多 outstanding NP 乱序完成独立匹配

- **WHEN** `register_np(1, 0x100)`,`register_np(1, 0x101)`,`register_np(1, 0x102)` → `complete(1, 0x102)`,`complete(1, 0x100)`,`complete(1, 0x101)`
- **THEN** 每次 `complete` 返回 true,最终 `outstanding_count(1) == 0`,`completed_count(1) == 3`

#### Scenario: 溢出 N+1 拒绝新发出

- **WHEN** 填满 per-VF outstanding 到 capacity(64) → `register_np(1, 0xFFFF)`
- **THEN** 返回 false,`outstanding_count(1) == 64`
- **AND** 完成一个后 `register_np(1, 0xFFFF)` 再次返回 true

### Requirement: endpoint-ip-17ports

PcieEndpointIP **SHALL** 作为新类(独立于 PcieEndpointTLM),提供 17 端口 SR-IOV Endpoint：
- 17 端口:port[0]=PF,port[1..16]=VF0..VF15
- 内置 PcieSriovVfPool + CompletionTracker
- 通过 ChStreamModuleBase 多端口注入 17 个 StreamAdapter
- on_config_loaded 挂接 PcieLinkLayer/PciePhyDigitalCtrl/PcieBypassMux
- `flr_pf()`/`flr_vf()` 转发到 pool + completions
- ModuleFactory 可通过 `"PcieEndpointIP"` 实例化

理由:Oracle Top-3 要求不破坏 PcieEndpointTLM 的 4 端口冻结布局。新类实现 SR-IOV 完整功能。

#### Scenario: JSON 实例化 PcieEndpointIP + 17 端口 adapter 注入

- **WHEN** JSON 配置 `module_type: "PcieEndpointIP"` + `link_layer.enabled: true`
- **THEN** ModuleFactory 创建实例,`num_ports() == 17`
- **AND** `set_stream_adapter(adapters[17])` 注入 17 个非空 adapter
- **AND** `all_ports_have_adapter() == true`
- **AND** `vf_pool()` 暴露 pool 访问,`completions()` 暴露 completion tracker

#### Scenario: on_config_loaded 挂接 LinkLayer/Phy/Mux

- **WHEN** JSON 含 `link_layer.enabled: true` → `on_config_loaded()`
- **THEN** `PcieLinkLayer::for_endpoint(name)` 非空
- **AND** `PciePhyDigitalCtrl::for_endpoint(name)` 非空
- **AND** `PcieBypassMux::for_endpoint(name)` 非空

Incremental: 0 files updated, 0 nodes, 0 edges (postprocess=minimal)