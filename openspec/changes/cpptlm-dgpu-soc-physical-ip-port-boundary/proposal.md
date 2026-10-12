# cpptlm-dgpu-soc-physical-ip-port-boundary: SoC 内部 IP 端口按物理 IP 类型切型 (修复物理 IP 边界漂移)

> **状态**: 🟡 Proposed Draft v0.5.0 (草稿, 2026-10-12 起草, 待评审)
> **作者**: CppTLM Team
> **版本**: v0.5.0 (初始草稿, P0 架构级)
> **关联 OpenSpec**:
>   - ✅ `cpptlm-vram-controller-functional-integration` archived (Change A, Phase 0)
>   - ✅ `cpptlm-lwc-bus-hub-mvp` archived (Change B, Phase 0)
>   - ✅ `cpptlm-gmmu-pte-bus-correction` archived (Change C, Phase 1)
>   - ✅ `cpptlm-dgpu-soc-internal-noc-e1` v1.1 archived (e1 v1.1, 2026-10-09)
>   - ✅ `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 archived (2026-10-10, doc-only cleanup)
>   - **业务定位锁定**: `docs/designs/dgpu-soc/architecture.md` §0.1 原则 3 (SoC 内部模块接口贴近真实物理 IP 边界)
>
> **🔗 Cross-Reference (per 根 AGENTS.md "SoC 内部 Bundle 类型分层" P0-revision 2026-10-12)**:
> 旧版"SimModule 无 ChStream 端口"误归纳已修正(2026-10-12 dual-agent review),根 AGENTS.md §KEY INVARIANTS + `include/tlm/AGENTS.md` + `include/tlm/cluster/AGENTS.md` 三处同步。本 change 推动**真正落地**物理 IP 端口边界,不再用 MVP 临时方案混用 wire-format。
> **修复范围**: SoC 内部 IP 端口类型切型 + 新增 wire-format 转换 adapter + `SocInternalNoc` ChStream 化
> **优先级**: **P0** (架构根因级, 对齐真实 dGPU 物理 IP 边界)
> **估时**: **14-22d** (5 模块改造 + 2 adapter + SocInternalNoc ChStream 化 + 回归测试)
> **依赖**: 全部 archived changes 已是 ✅;**frozen headers (pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh / cpptlm_emulator.h) 零 diff** (per ADR-DGPU-08 §2.3)
> **目标 tag**: `cpptlm-dgpu-soc-physical-ip-port-boundary-completion`

---

## Why

### 1. 背景:旧版 MVP 临时方案把全部 SoC 内部 IP 端口用 `PcieTlpBundle`

旧版 `SdmaEngineTLM` 5 端口(per `include/tlm/gpu/sdma_engine_tlm.hh:84-99`):

```
0. desc_in   (ingress) → PcieTlpBundle   (kind=DMA_DESC 编码 DmaDescriptor)
1. mem_in    (ingress) → PcieTlpBundle   VRAM 读响应 (D2H 路径)
2. mem_out   (egress)  → PcieTlpBundle   VRAM 读/写
3. host_out  (egress)  → PcieTlpBundle   upstream DMA 事务
4. done_out  (egress)  → PcieTlpBundle   (kind=DMA_DONE 编码 CompletionBundle)
```

**关键设计注释** (per `sdma_engine_tlm.hh:92-99`):

> **Wire-format 选择(MVP)**:所有 5 端口共享 `PcieTlpBundle` 作为底层 wire-format
> (参考 `PcieEndpointTLM` 复用 `PcieTlpBundle` 承载 `IRQ_DELIVERY` 的设计)。
> **升级路径**:若未来引入专用 `DmaDescBundle` / `DoneBundle` 作为 wire-format,
> 仅需修改 `SdmaEngineTLM` 内部转换函数 + `ChStreamAdapterFactory` 注册;
> 外部 API 保持向后兼容。

**`VramControllerTLM`** (per `include/tlm/vram_controller_tlm.hh:58`):继承 `MemoryTLM`,端口 `CacheReqBundle`。

**`GpuInternalFabric`** (per `include/tlm/cluster/gpu_internal_fabric.hh:37`):走 `set_vram_target/set_fabric_source` programmatic bridge,内部 `Fabric2VramCtrlBundle` 8 字段。

### 2. 物理 IP 边界漂移:违背真实 dGPU 设计

| 真实 dGPU 物理 IP 边界 | 当前 CppTLM 实现 | 偏离程度 |
|--------------------|------------------|---------|
| **NVIDIA H100 SDMA** 出口 → mesh NoC (AXI4-like wire) | SDMA 出口用 PcieTlpBundle | ❌ **严重偏离** |
| **ARM CMN-700 mesh NoC** → HBM Controller (CHI/AXI4) | NoC 内部用 NoCFlitBundle,出口用 CacheReqBundle (继承 MemoryTLM) | ✅ 部分对齐 |
| **HBM3 Controller** 上游 → AXI4 (不是 PCIe TLP) | VramController 端口用 CacheReqBundle (继承 MemoryTLM) | ⚠️ 偏离 (CacheReqBundle 不严格是 AXI4) |
| **NVIDIA H100 GMMU** dedicated memory client → HBM (AXI4-like) | GMMU 走 programmatic bridge, PcieTlpBundle | ❌ 严重偏离 |
| **PCIe EP** ↔ host board-level wire (PCIe TLP) | PcieEndpointIP 内部用 PcieTlpBundle | ✅ 对齐 |

### 3. 根因:这是 ADR-DGPU-06 + D-AXI v1.5 B25 反复修订未根治的病

**ADR-DGPU-06** 标题就是"axi-mem-bundle-boundary" (chip-internal AXI vs board-level PCIe 边界严格分离),但**实装时**:`SdmaEngineTLM` / `VramControllerTLM` / `GpuInternalFabric` 端口**全用 PcieTlpBundle** (MVP 临时方案),违背该 ADR。

**D-AXI v1.5 B25** (per AGENTS.md KEY INVARIANTS):"撤回 v1.3 B2 SDMA mem_in/mem_out → AxiMemBundle 切型决策 (避免异构 multi-port adapter 框架限制);D3+ 演进路径待 Change C/D3 协同设计"——**B25 只是撤回切型决策,并未真正落地 AXI/NoC 类型**。

### 4. 后果(已观察到的 3 类问题)

1. **e1 v1.1 反复修订** (per `cpptlm-dgpu-soc-internal-noc-e1` v1.1):4 P0 + 3 CRITICAL + 3 MAJOR 大量工作是因为"SDMA 发 PcieTlpBundle vs NoC 内部 NoCFlitBundle vs VramCtrl 收 CacheReqBundle"三方类型不一致,需要复杂 adapter 兜底
2. **Change C Gap G1 反复挣扎** (per `docs/implementation/2026-10-09-gmmu-pte-bus-correction.md` §6 Deviation 1):GMMU programmatic bridge + VramController functional port 是 workaround,**真正解法是端口类型对齐**
3. **`sdma.2 → soc_noc.0` JSON 直连走不通** (per本会话 earlier discussion):即便 SocInternalNoc ChStream 化,`PcieTlpBundle` ↔ `NoCFlitBundle` 仍要 wire-format 转换层

### 5. 物理 IP 对应表(本 change 推动的真实对齐)

| 物理 IP | 真实 wire 类型 | 当前 CppTLM 端口 | 目标 CppTLM 端口 |
|---------|-------------|------------------|------------------|
| **NVIDIA H100 SDMA 下游 (NoC 侧)** | AXI4-like / NoC flit | `mem_in/mem_out` (PcieTlpBundle) ❌ | **`mem_in/mem_out` (NoCFlitBundle)** ✅ |
| **NVIDIA H100 SDMA 上游 (PCIe/host 侧)** | PCIe TLP (DMA descriptor + completion) | `desc_in/host_out/done_out` (PcieTlpBundle) ✅ | **`desc_in/host_out/done_out` (PcieTlpBundle)** ✅ (保留) |
| **HBM3 Controller 上游 (NoC 侧)** | AXI4 / NoC flit | `req_in_/req_out_/resp_in_/resp_out_` (CacheReqBundle) ⚠️ | **`req_*_` (NoCFlitBundle)` + 内部 AXI4 协议层** ✅ |
| **NVIDIA H100 GMMU dedicated memory client** | AXI4-like / NoC flit | programmatic bridge (PcieTlpBundle) ❌ | **`gmmu.req_out_` (NoCFlitBundle) → `vram_ctrl.req_in_`** ✅ |
| **PCIe EP ↔ host board-level** | PCIe TLP | `PcieTlpBundle` (PcieEndpointIP) ✅ | **保留** (frozen header) |
| **SoC 内部 IP 互连 (寄存器访问, non-NoC bulk)** | AXI4 / AXI-Lite | 多用 PcieTlpBundle ❌ | **AxiBundle / AxiStreamBundle 系列** ✅ |

### 6. 4 原则 (per `docs/designs/dgpu-soc/architecture.md` §0.1 锁定)

- **原则 1** driver-visible SoC 拓扑:本 change 推动**真实物理 IP 边界**,driver 视角闭环不变
- **原则 2** chip-level main memory by DGpuBoard:`vram_storage_` 所有权不动,本 change 只改 wire-format
- **原则 3** SoC 内部模块接口贴近真实物理 IP 边界 + 双重无注入约束:**本 change 直接落地此原则**
- **原则 4** VramControllerTLM 单独支持 functional-mode + backdoor:`set_mode(FunctionalZeroDelay)` API 不动,本 change 只改 wire-format

---

## What Changes

### 切型 ① — `SdmaEngineTLM` 端口按"上游 PCIe / 下游 NoC"二分

| 端口 | 当前类型 | 目标类型 | 理由 |
|------|---------|---------|------|
| `desc_in` (PORT 0) | `PcieTlpBundle` | **`PcieTlpBundle`** (保留) | 上游 PCIe 侧, host 端 driver 写入 DMA descriptor,wire 是 PCIe TLP |
| `mem_in` (PORT 1) | `PcieTlpBundle` | **`NoCFlitBundle`** | 下游 NoC 侧, VRAM 读响应 (D2H 路径) 经 SocInternalNoc mesh,wire 应对齐 NoC |
| `mem_out` (PORT 2) | `PcieTlpBundle` | **`NoCFlitBundle`** | 下游 NoC 侧, VRAM 读/写经 SocInternalNoc mesh |
| `host_out` (PORT 3) | `PcieTlpBundle` | **`PcieTlpBundle`** (保留) | 上游 PCIe 侧, upstream DMA 事务,wire 是 PCIe TLP |
| `done_out` (PORT 4) | `PcieTlpBundle` | **`PcieTlpBundle`** (保留) | 上游 PCIe 侧, DMA completion,wire 是 PCIe TLP |

**新增 wire-format 转换层**:
- `SDMA_PcieTlp_To_NocAdapter` (PcieTlpBundle → NoCFlitBundle, 用于 `mem_out` 出口侧)
- `SDMA_Noc_To_PcieTlpAdapter` (NoCFlitBundle → PcieTlpBundle, 用于 `mem_in` 入口侧)
- 注册到 `ChStreamAdapterFactory`

### 切型 ② — `VramControllerTLM` 上游 NoC 化

| 端口 | 当前类型 | 目标类型 | 理由 |
|------|---------|---------|------|
| `req_in_` (上游) | `CacheReqBundle` (继承 MemoryTLM) | **`NoCFlitBundle`** | 上游 NoC 侧, 接 SDMA 经 SocInternalNoc + GMMU dedicated client + fabric |
| `req_out_` (内部) | `CacheReqBundle` | **保留** (内部 AXI 协议真实) | MemoryTLM 内部行缓冲 + 协议状态机 |
| `resp_in_` (内部) | `CacheRespBundle` | **保留** | 同上 |
| `resp_out_` (上游) | `CacheRespBundle` | **`NoCRespBundle` (= `NoCFlitBundle{flit_category=RESPONSE}`)** | 上游 NoC 侧, 响应回 SocInternalNoc |

**新增 wire-format 转换层**:
- `VramCtrl_Noc_To_AxiAdapter` (NoCFlitBundle → CacheReqBundle, 入口侧)
- `VramCtrl_Axi_To_NocAdapter` (CacheRespBundle → NoCFlitBundle, 出口侧)
- 内部 `req_out_/resp_in_` 保留 `CacheReqBundle/CacheRespBundle`(MemoryTLM 协议真实)

### 切型 ③ — `GpuInternalFabric` ChStream 化(取消 callback)

| 项 | 当前 | 目标 |
|----|------|------|
| 基类 | `SimModule` | **`SimModule` (加 ChStream 端口字段)** |
| 端口字段 | 0 ChStreamPort | **N 组 ChStreamPort (req_in + req_out + resp_in + resp_out)** |
| 接收方式 | `set_vram_target/set_fabric_source` programmatic bridge | **ChStream 端口 + `set_stream_adapter()` override** |
| `Fabric2VramCtrlBundle` | 8 字段,内部 | **保留** (内部 IP-间 wire) |
| JSON 声明 | programmatic (C++ `bind_memory_backings()`) | **JSON `connections[]` 可声明** `fabric.X → vram_ctrl.Y` |

### 切型 ④ — `SocInternalNoc` ChStream 化(取消 callback)

| 项 | 当前 | 目标 |
|----|------|------|
| 基类 | `SimModule` | **`SimModule` (加 ChStream 端口字段)** |
| 端口字段 | 0 ChStreamPort | **N 组 ChStreamPort (req_in + req_out + resp_in + resp_out) + NoCFlitBundle 类型** |
| 接收方式 | `set_*_target()` programmatic bridge | **ChStream 端口 + `set_stream_adapter()` override** |
| 路由 | `route_flit()` 内部推送 | **保留** (NoC 内部 mesh 路由逻辑) |
| JSON 声明 | programmatic (C++ `bind_memory_backings()`) | **JSON `connections[]` 可声明** `sdma.2 → soc_noc.0` 等 |

### 切型 ⑤ — `PcieMemoryDevice` 端口类型微调(per Change A 实证)

| 端口 | 当前 | 目标 |
|------|------|------|
| `req_in_` (port 0, SDMA bulk DMA) | `CacheReqBundle` (继承 MemoryTLM) | **保留** (PcieMemoryDevice 本身就是 protocol adapter) |
| `req_in_` (port 1, GMMU PTE fetch) | `CacheReqBundle` | **改 NoCFlitBundle** (GMMU dedicated client 经 NoC) |

注: 实际上 Gap I 修复后, GMMU 不再走 PcieMemoryDevice,**port 1 已在 Change C T4 退役**;`PcieMemoryDevice` 当前只保留 `port 0` (SDMA bulk DMA 委托 VramController)。本 change 确认这一点。

### 切型 ⑥ — `GmmuTLM` 端口类型切型

| 端口 | 当前 | 目标 |
|------|------|------|
| `req_out_` (PTE fetch) | `PcieTlpBundle` | **`NoCFlitBundle`** |
| `resp_in_` (PTE fetch 响应) | `PcieTlpBundle` | **`NoCRespBundle`** |
| `mem_view_` (programmatic bridge) | (callback) | **保留** (timing-mode H-C6 不动) |
| `translate_target_` (programmatic) | (callback, Gap G1 修复后) | **保留** (per Change C T6) |

### 切型 ⑦ — SoC 内部 IP 互连(寄存器访问, non-NoC bulk)

| 场景 | 当前 | 目标 |
|------|------|------|
| `lwc_hub ↔ GMMU` (CR-Bus) | `CrBundle` (8 字段 APB-lite) | **保留** (per Change B) |
| `lwc_hub ↔ SDMA` (Doorbell) | `DoorbellBundle` (4 字段 posted-write) | **保留** (per Change B) |
| `lwc_hub ↔ MsixGeneratorIP` (MSI-X) | `MsixBundle` | **保留** (per Change E) |
| `gpu_internal_fabric ↔ SDMA` 等 | 多用 PcieTlpBundle | **改 AxiBundle 系列** |

### 不在范围内

- **23 ABI 函数** (`include/abi/cpptlm_emulator.h`):零 diff (frozen)
- **4 frozen headers**:零 diff (per ADR-DGPU-08)
- **PcieEndpointIP 内部 17 端口**:保留 PcieTlpBundle (frozen by Phase 4 桥接)
- **PCIe Controller ↔ host 边界**:保留 PcieTlpBundle (board-level)
- **timing-mode** (dgpu_soc_timing_v1.json):保持 `SimulationMode::TimingCycleApproximate` + 当前 wire-format (本 change 只动 functional-mode, timing-mode H-C6 走 mem_view_/translate_timing TLB 路径,不动)

---

## Acceptance Criteria

- [ ] `SdmaEngineTLM` 端口类型切型完成 (3 端口 NoCFlitBundle + 2 端口 PcieTlpBundle)
- [ ] `VramControllerTLM` 端口类型切型完成 (上游 NoCFlitBundle + 内部 CacheReqBundle)
- [ ] `GpuInternalFabric` ChStream 化 (取消 callback, JSON `connections[]` 可写)
- [ ] `SocInternalNoc` ChStream 化 (取消 callback, JSON `connections[]` 可写)
- [ ] `GmmuTLM` 端口类型切型完成
- [ ] 4 个新 wire-format adapter 实现 + 注册 (SDMA_PcieTlp_To_Noc, SDMA_Noc_To_PcieTlp, VramCtrl_Noc_To_Axi, VramCtrl_Axi_To_Noc)
- [ ] JSON `sdma.2 → soc_noc.0` 直连走通(per Phase 0 D-AXI 评审需求)
- [ ] JSON `soc_noc.0 → vram_ctrl.0` 直连走通(per Phase 0 D-AXI 评审需求)
- [ ] 0 回归 (per Phase 0+1 实测基线):[pcie] 430/36672 + [gmmu] 25/5273 + [noc] 64/304 + [sdma] 68/12945 + [minimal_dgpu_soc] 24/170 + [dgpu_soc_timing] 37/12024 + [driver_visible] 4/40
- [ ] frozen headers 零 diff + 23 ABI 零变更
- [ ] 实施笔记 (`docs/implementation/2026-XX-XX-...md`) 完整 + post-flight 验证
- [ ] 同步更新 `dgpu_soc_minimal_v1.json` JSON connections (从 1 条 → 4-5 条)

---

## Capabilities

### New Capabilities

- `dgpu-soc-physical-ip-port-boundary`: SoC 内部 IP 端口按真实物理 IP 边界切型
  - 新增 wire-format adapter 4 个 (SDMA PcieTlp↔NoC, VramCtrl Noc↔Axi)
  - 新增 SimModule ChStream 化模式 (2 个: SocInternalNoc + GpuInternalFabric)
  - 新增端口类型约束: 内部 NoC 接口 → NoCFlitBundle, 内部 AXI 接口 → AxiBundle 系列, PCIe 板级 → PcieTlpBundle

### Modified Capabilities

- `dgpu-board-abi-hygiene`: 不动 (23 ABI 冻结)
- `driver-visible-minimal-soc`: 同步更新 JSON 拓扑(从 1 条 connection → 4-5 条) + DGpuBoard::bind_memory_backings() 装配简化(programmatic bridge 改为 ChStream 自动化)
- `soc-internal-noc`: 从 callback 化 → ChStream 化(per e1 v1.1 archived)
- `sdma-engine-tlm`: 端口切型(3 端口 NoC + 2 端口 PCIe)
- `vram-controller-functional-mode`: 端口切型(上游 NoC + 内部 Cache)
- `gmmu-tlm`: 端口切型(req_out_/resp_in_ 改 NoCFlitBundle)
- `pcie-ip-microarch`: 不动 (PcieEndpointIP 内部 frozen)

### Removed Capabilities

- `dgpu-soc-programmatic-bridge-pattern`: 移除 4 处 programmatic bridge (SocInternalNoc 4-target + GpuInternalFabric 双桥) → 改为 ChStream JSON 自动化

---

## Impact

### Affected Code

- `include/tlm/gpu/sdma_engine_tlm.{hh,cc}`: 端口切型 + 4 端口类型变更
- `include/tlm/vram_controller_tlm.{hh,cc}`: 端口切型 + 2 端口类型变更
- `include/tlm/gpu/gmmu_tlm.{hh,cc}`: 端口切型 + 2 端口类型变更
- `include/tlm/cluster/soc_internal_noc.{hh,cc}`: ChStream 化
- `include/tlm/cluster/gpu_internal_fabric.{hh,cc}`: ChStream 化
- `include/tlm/gpu/pcie_memory_device.{hh,cc}`: 端口调整 (port 1 退役确认)
- `src/tlm/gpu/dgpu_board_shell.cc`: bind_memory_backings() 简化 (移除 4 处 programmatic bridge)
- `configs/dgpu_soc_minimal_v1.json`: connections 从 1 条 → 4-5 条
- `chstream_register.hh`: 4 个新 adapter 注册
- `bundles/`: 确认 NoCFlitBundle / CacheReqBundle / PcieTlpBundle / AxiBundle 各 Bundle 类型

### Affected Tests

- `[sdma]` 68 cases: 0 回归(per port 切型)
- `[vram]` (新) 0 cases: 待新增 (VramController 端口切型)
- `[gmmu]` 25 cases: 0 回归
- `[noc]` 64 cases: 0 回归
- `[minimal_dgpu_soc]` 24 cases: 0 回归 + 新增 E2E 直连 case (sdma → soc_noc → vram_ctrl)
- `[dgpu_soc_timing]` 37 cases: 0 回归 (timing-mode 不动)
- `[driver_visible]` 4 cases: 0 回归
- `[pcie]` 430 cases: 0 回归 (PcieEndpointIP frozen)
- `[pcie-memory]` 17 cases: 0 回归
- 新增 `[port-typing]` 标签: 6+ cases (验证 4 个 adapter 双向)

### Affected Specs

- `openspec/specs/dgpu-soc-physical-ip-port-boundary/spec.md` (新增, 本 change 起草)
- `openspec/specs/sdma-engine-tlm/spec.md` (修改, 端口类型约束)
- `openspec/specs/vram-controller-functional-mode/spec.md` (修改, 端口切型)
- `openspec/specs/gmmu-pte-wf/spec.md` (修改, 端口切型)
- `openspec/specs/soc-internal-noc/spec.md` (修改, ChStream 化)

### Affected ADRs

- ADR-DGPU-06 (axi-mem-bundle-boundary): Status Update 2026-10-12 追加, 标记"已落地"
- ADR-DGPU-10 (backing-naming-convention): 不动
- ADR-DGPU-11 (timing-mode-soc-scope): 不动 (timing-mode H-C6 不动)

### Risk Assessment

- **High Risk**: 5 模块端口类型切型,影响面广(7 标签 1100+ cases)
- **Mitigation**:
  - TDD 5-step 严格 RED→GREEN→E2E→Notes
  - 0 回归锁(per Phase 0+1 实测基线)
  - frozen headers 零 diff + 23 ABI 零变更 (per ADR-DGPU-08)
  - 实施期保留 programmatic bridge fallback (per Change C §6 Deviation 4 set_translate_cb 保留先例)
  - 拆分 5 sub-task (per tasks.md),每 sub-task 独立 TDD + commit
  - 实施后 doc-only cleanup (per cpptlm-dgpu-soc-arch-truth-cleanup v1.3 §6 先例, 不消耗 KPI)

### KPI Status (起草时)

- **当前 baseline (2026-10-12)**: 3/3 (e1 v1.1 Draft + flr-rebar v1.1 Draft + p2-integration-unblock Proposed),**触顶**
- **本 change 起草状态**: 🟡 Proposed Draft (per AGENTS.md "Draft 不计入 KPI 上限" 复审规则),**不消耗 KPI**
- **activate 时机**: Change D (cpptlm-pcie-tlp-entry-layer) 或 Change E (cpptlm-msix-generator-ip) archive 后,KPI 释放 1 slot,再 activate 本 change
- **归档时**: 1 slot 释放, 给 Change F (Error/CDC) 等 backlog 留位
