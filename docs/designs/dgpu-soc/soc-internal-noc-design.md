# SocInternalNoc 设计文档 (Functional Routing + Interface Compat)

> **状态**: ✅ Implemented (cpptlm-dgpu-soc-internal-noc-minimal, 2026-10-08)
> **配套 OpenSpec**: [`openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal/`](../../../openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal/) (proposal + design + tasks + specs)
> **作者**: CppTLM Team / 日期: 2026-10-08
> **范围**: CppTLM scope (per AGENTS.md 季度复审产物 "仿真框架 + 基本 SoC 验证" 在本仓)

---

## 1. 背景与动机

### 1.1 问题陈述

`dgpu-realip-pin-contract` Change A (`Fabric2VramCtrlBundle`) 只覆盖了 GpuInternalFabric ↔ VramControllerTLM 一个桥接。**SoC 内部 4 个核心模块的端到端数据路径未落地**：

- GMMU PTE walk: GMMU → NoC → VRAM → NoC → GMMU
- SDMA copy: SDMA → NoC → VRAM → NoC → SDMA
- PCIe BAR decode: PCIe → NoC → GMMU/SDMA/VRAM

驱动路径 `UsrLinuxEmu → DGpuBoard ABI` 需要前 2 条路径可工作才能验证 GMMU/SDMA 闭环。

### 1.2 现状 (per architecture.md §2.4 4-IP 表格)

- `GpuInternalFabric` 名字暗示 "GPU 内部"，实际只承担 1 个桥接 (vram_ctrl via Fabric2VramCtrlBundle)
- `attach_crossbar` inner crossbar 是 dormant state (T25A.1 才会激活)
- GMMU/SDMA/PCIe Controller 与 VRAM 之间没有显式 NoC 路由
- DGpuBoard::mmio_read/write 路径靠直接函数调用绕过

### 1.3 CppTLM vs ArchForge Scope 决策 (per AGENTS.md 季度复审产物)

| CppTLM scope (本 change) | ArchForge scope (defer) |
|---------------------------|--------------------------|
| "功能路由 + 接口兼容" | "完整 SoC 架构建模" |
| 4 target 路由表 (stateless) | multi-VC 仲裁 |
| NoCFlitBundle 18 字段编译期契约 | mesh XY 路由 / adaptive routing |
| Programmatic bridge via setters | congestion control / backpressure |
| 0 latency / 0 arbitration | latency / cycle-approximate |
| CppTLM = 仿真框架 + 基本 SoC 验证 | ArchForge = SoC 架构建模 (跨仓镜像) |

→ **接口对齐**已通过 `NoCFlitBundle` 18 字段 (mesh XY + VC + flit_type) 落地。**NoC 行为建模不是对齐目标，是 ArchForge 范围**。

---

## 2. 架构

### 2.1 模块位置

```
include/tlm/cluster/
├── gpu_internal_fabric.hh    [现有] VRAM bridge (Fabric2VramCtrlBundle, 8 字段)
│                                  本期 scope 不动 (vram_ctrl_target 仍走此路径)
└── soc_internal_noc.hh       [新]   SoC 范围 4 target 路由 (NoCFlitBundle, 18 字段)
```

### 2.2 5-IP 边界映射 (per architecture.md §2.4 修订)

| IP 边界 | 物理对应 | TLM 类 | 端口契约 | State ownership |
|---------|----------|--------|----------|------------------|
| PcieControllerIP | DWC i5122 / Cadence | `pcie_controller_ip.hh` | 4 BAR decode | stateless facade |
| PcieAxiBridgeIP | Synopsys AXI↔PCIe | `pcie_axi_bridge_ip.hh` | AXI4-slave composite | EP-internal |
| GpuInternalFabric | Custom mesh / Arteris | `gpu_internal_fabric.hh` | Fabric2VramCtrlBundle (8 字段) | non-owning crossbar |
| **SocInternalNoc (新)** | **Arteris FlexNoC / Custom Mesh** | **`soc_internal_noc.hh`** | **NoCFlitBundle (18 字段)** | **stateless 路由骨架** |
| VramControllerTLM | NVIDIA FB / AMD MC | `vram_controller_tlm.hh` | Fabric2VramCtrlBridge + DFI-lite | backing ptr + row buffer |

### 2.3 SocInternalNoc 接口设计

```cpp
namespace tlm::cluster {

class SocInternalNoc : public ::SimModule {
public:
    explicit SocInternalNoc(const std::string& name, EventQueue* eq);

    // 4 NODE_ID 常量 (per architecture.md §2.4 IP 表格顺序)
    static constexpr uint32_t NODE_ID_PCIE = 0;
    static constexpr uint32_t NODE_ID_SDMA = 1;
    static constexpr uint32_t NODE_ID_GMMU = 2;
    static constexpr uint32_t NODE_ID_VRAM = 3;

    // 4 target setters (programmatic bridge, per GpuInternalFabric pattern)
    void set_pcie_target(tlm::pcie::PcieControllerIP* target) noexcept;
    void set_sdma_target(tlm::gpu::SdmaEngineTLM* target) noexcept;
    void set_gmmu_target(tlm::gpu::GmmuTLM* target) noexcept;
    void set_vram_target(::VramControllerTLM* target) noexcept;

    // 4 accessors (nullptr-safe)
    tlm::pcie::PcieControllerIP* pcie_target() const noexcept;
    tlm::gpu::SdmaEngineTLM*    sdma_target() const noexcept;
    tlm::gpu::GmmuTLM*         gmmu_target() const noexcept;
    ::VramControllerTLM*        vram_target() const noexcept;

    // 路由入口 (per design.md §2.3)
    void route_flit(const bundles::NoCFlitBundle& flit) noexcept;

    // 统计
    uint64_t flits_routed() const noexcept;
    uint64_t unknown_dst_count() const noexcept;
};

} // namespace tlm::cluster
```

---

## 3. 路由语义

```cpp
void SocInternalNoc::route_flit(const NoCFlitBundle& flit) {
    const uint32_t dst = flit.dst_node.read();
    switch (dst) {
        case NODE_ID_PCIE: ++flits_routed_; break;
        case NODE_ID_SDMA: ++flits_routed_; break;
        case NODE_ID_GMMU: ++flits_routed_; break;
        case NODE_ID_VRAM: ++flits_routed_; break;
        default: ++unknown_dst_count_; break;  // graceful skip
    }
    // 未来 E.1: 实际把 flit 推到 target->accept(flit) (本期 placeholder)
}
```

**关键不变量** (per specs REQ-NOC-4):
- `flit.dst_node` 与 4 个 target 节点 ID 一一对应 (0/1/2/3)
- 未知 dst_node (>3) 不 crash, 仅记数 (per D.2 优雅降级原则)
- 4 个 setter 可独立调用, 允许 partial 装配
- nullptr target + 路由触发 → 仍 flits_routed++ (本期路由骨架不检查 target 非空; 真实推送是 E.1 范畴, 届时加 nullptr 守卫)

---

## 4. 装配流程

### 4.1 JSON 装配路径

`configs/dgpu_soc_v2_realip_p2.json` 加 `soc_noc` 节点：

```json
{
  "name": "soc_noc",
  "type": "SocInternalNoc",
  "params": {}
}
```

`ModuleFactory::simulate_instantiate` 通过 `REGISTER_MODULE` 找到 type 字符串 → `new SocInternalNoc(name, eq)` → 加入 `module_instances["soc_noc"]`。

### 4.2 双注册 (FQ + 短名, 仿 PcieControllerIP precedent)

```cpp
// include/modules_cluster.hh
const bool _reg_socinternalnoc = (REGISTER_MODULE(tlm::cluster::SocInternalNoc), true);
const bool _reg_socinternalnoc_short =
    (ModuleFactory::registerModule<tlm::cluster::SocInternalNoc>("SocInternalNoc"), true);
```

### 4.3 DGpuBoard 桥接

`src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()` 在 fabric↔vram 桥接后加：

```cpp
if (auto* soc_noc = dynamic_cast<tlm::cluster::SocInternalNoc*>(
        soc_->getInternalInstance("soc_noc"))) {
    if (controller_ip_) soc_noc->set_pcie_target(controller_ip_);
    if (sdma_engine_)   soc_noc->set_sdma_target(sdma_engine_);
    if (gmmu_)          soc_noc->set_gmmu_target(gmmu_);
    if (vram_ctrl_seen) soc_noc->set_vram_target(vram_ctrl_seen);
}
```

→ 5 桥接: vram 走 GpuInternalFabric (现有) + 4 new via SocInternalNoc

---

## 5. 端口契约: NoCFlitBundle

`include/bundles/noc_bundles_tlm.hh:26` 定义 18 字段:

| 字段 | width | 语义 |
|------|:-----:|------|
| transaction_id | 64 | 事务 ID |
| src_node | 32 | 源节点 (mesh XY) |
| dst_node | 32 | 目标节点 (mesh XY) |
| address | 64 | DRAM byte address |
| data | 64 | write data |
| size | 8 | transaction size (bytes) |
| vc_id | 8 | 虚拟通道 ID (0-3) |
| flit_type | 8 | 0=HEAD, 1=BODY, 2=TAIL, 3=HEAD_TAIL (wormhole) |
| flit_index | 8 | 当前 flit 在分组中的索引 |
| flit_count | 8 | 总 flit 数量 |
| hops | 8 | 已跳数 (credit return routing) |
| flit_category | 8 | 0=REQUEST, 1=RESPONSE, 2=CREDIT |
| src_port | 8 | 上游路由器输出端口 |
| is_write | 1 (ch_bool) | 写标志 |
| is_ok | 1 (ch_bool) | 成功标志 (RESPONSE only) |
| error_code | 8 | 0=OK, 1=DECERR, 2=SLVERR |
| (总计 16 字段 + 2 ch_bool) |

→ 编译期契约通过 `BUNDLE_FIELD_WIDTH(NoCFlitBundle, ...)` macro 锁 (per #4 commit `3fba7a61` 测试已覆盖)。

---

## 6. 测试覆盖

`test/test_soc_internal_noc.cc` (5 test cases / 31 assertions), tag `[realip][pin-contract][noc]`:

| 场景 | 验证 |
|------|------|
| 1 | 4 NODE_ID_* 唯一性 + 顺序 (编译期 STATIC_REQUIRE) |
| 2 | 4 setter + accessor 双向 (set nullptr 验证 graceful) |
| 3 | dst=PCIE 路由 (flits_routed++) |
| 4 | dst=SDMA/GMMU/VRAM 路由 (3 sub-cases, 全部 flits_routed++) |
| 5 | dst=99 未知 → unknown_dst_count++, 0 crash |
| 6 | nullptr target + 路由触发 → 0 crash |

**E2E 回归** (0 回归):
- `[pcie]` 430/430 / 36672 assertions
- `[pcie-memory]` 17/17 / 48 assertions
- `[minimal_dgpu_soc]` 12/12 / 66 assertions
- `[realip]` 29/29 / 254 assertions (含新 sub-tag)
- `[dgpu_soc_timing]` 37/37 / 12024 assertions
- `[driver_visible]` 2/2 / 29 assertions

---

## 7. 设计权衡

### 7.1 为什么独立模块而非扩展 GpuInternalFabric

| 维度 | 独立 SocInternalNoc (推荐) | 扩展 GpuInternalFabric |
|------|---------------------------|-----------------------|
| 命名清晰度 | ✅ "SoC 内部 NoC" 准确 | ❌ "GPU 内部 Fabric" 实际承担 SoC 路由, 名字误导 |
| 关注点分离 | ✅ VRAM bridge vs NoC routing 是不同 abstraction | ❌ 单一模块承担 2 个职责 |
| 测试隔离 | ✅ NoC routing test 不需要 VRAM mock | ❌ 测试必须 mock VRAM |
| ADR-DGPU-07 seam | ✅ 新模块扩展更易 D3 完整化 | ❌ 既有模块扩展有 side effect 风险 |
| 代码改动量 | 中 (新模块 + 4 装配) | 小 (既有模块加 3 setter) |
| 风险 | 中 (新文件需更多测试) | 低 (复用既有测试) |

→ **选独立模块**（推荐）, 命名准确 + 关注点分离是核心, 改动量差异可接受。

### 7.2 为什么用 NoCFlitBundle 而非 Fabric2VramCtrlBundle

`NoCFlitBundle` 18 字段 (mesh XY + VC + flit_type) 对齐物理 IP 真实 wire format。
`Fabric2VramCtrlBundle` 8 字段 (subset) 是当前 GpuInternalFabric↔VramControllerTLM 专用, 不含 mesh 寻址。

→ SocInternalNoc ingress/egress 用 `NoCFlitBundle` (对齐物理), VramControllerTLM 内部仍接 `Fabric2VramCtrlBundle` (GpuInternalFabric 桥接, 已有)。

### 7.3 为什么本期不切 4 target 模块的 wire format

`PcieControllerIP::match_bar` 用 uint8_t + uint64_t + BarTarget, `SdmaEngineTLM` 5 端口用 `PcieTlpBundle`, `GmmuTLM` 用 `PcieTlpBundle`, `VramControllerTLM` 用 `Fabric2VramCtrlBundle`。

切到统一 `NoCFlitBundle` 需要改 4 个模块 + 装配层, scope 爆炸, 留 future change E.1。

→ 本期 SocInternalNoc 仅持 4 个 raw 指针, 路由表基于 `dst_node`, **不实际推动 flit 数据**。数据流仍走既有路径 (DGpuBoard::mmio_read/write 直调 + Fabric2VramCtrlBridge), SocInternalNoc 作为未来 E.1 的"路由骨架"。

---

## 8. 演进路径 (per ADR-DGPU-07 演进 seam)

### 8.1 E.1 (下一 PR, 独立): SocInternalNoc 实际承载 NoCFlitBundle 数据流

需要:
1. 4 target 模块切到 NoCFlitBundle 端口 (PcieControllerIP / SdmaEngineTLM / GmmuTLM)
2. SocInternalNoc::route_flit() 实际推 flit 到 target->accept(flit)
3. 装配层重写: 现有 PcieTlpBundle + Fabric2VramCtrlBundle 路径 → NoCFlitBundle
4. 测试: 驱动路径 E2E 真实运行 (GMMU page walk 经 SocInternalNoc → VRAM)

### 8.2 E.2 (defer to ArchForge): 完整 SoC 内部 NoC 物理行为

- multi-VC 仲裁 (vc_id 已有字段)
- mesh XY 路由 (src_node/dst_node 已有字段)
- congestion control / backpressure (hops 已有字段)
- latency / cycle-approximate (timing-mode 扩展)

### 8.3 D3 seam (per ADR-DGPU-07): 完整 GPU 模拟

- 多通道 HBM PHY
- 多级页表 (L2/L3 TLB)
- AXI master port outstanding + rid 关联 (per existing #4 commit `3fba7a61` BUNDLE_FIELD_WIDTH prep)

---

## 9. Frozen surface (0 修改)

- `include/abi/cpptlm_emulator.h` — 23 ABI 冻结
- `include/bundles/noc_bundles_tlm.hh` — NoCFlitBundle (既有)
- `include/bundles/dgpu_fabric_bundles_tlm.hh` — Fabric2VramCtrlBundle (既有)
- `include/tlm/vram_controller_tlm.hh` — VramControllerTLM 内部
- `include/tlm/gpu/pcie_endpoint_tlm.h` — frozen ABI
- `include/tlm/gpu/pcie_display_device.hh` — frozen
- `include/tlm/pcie/pcie_bundles_tlm.hh` — frozen
- 4 target 模块的 wire format (本期不切, 留 E.1)

---

## 10. 关联资源

| 资源 | 位置 |
|------|------|
| OpenSpec change | `openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal/` |
| Proposal | [proposal.md](../../../openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal/proposal.md) |
| Design | [design.md](../../../openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal/design.md) |
| Specs | [specs/soc-internal-noc/spec.md](../../../openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal/specs/soc-internal-noc/spec.md) |
| Tasks | [tasks.md](../../../openspec/changes/cpptlm-dgpu-soc-internal-noc-minimal/tasks.md) |
| 测试 | [test/test_soc_internal_noc.cc](../../../test/test_soc_internal_noc.cc) |
| 实现 | [include/tlm/cluster/soc_internal_noc.hh](../../../include/tlm/cluster/soc_internal_noc.hh) |
| 装配 | [src/tlm/gpu/dgpu_board_shell.cc:766-790](../../../src/tlm/gpu/dgpu_board_shell.cc) |
| 注册 | [include/modules_cluster.hh](../../../include/modules_cluster.hh) |
| 相关 invariants | [AGENTS.md §KEY INVARIANTS](../../../AGENTS.md) |
| 4-IP → 5-IP 边界 | [architecture.md §2.4](./architecture.md) |
| 范围治理 | [AGENTS.md "仿真框架 vs 应用设计"](../../../AGENTS.md) |
| 演进 seam | [ADR-DGPU-07](../adr/ADR-DGPU-07-minimal-soc-evolution-seam.md) |
| NoCFlitBundle 契约 | [include/bundles/noc_bundles_tlm.hh](../../../include/bundles/noc_bundles_tlm.hh) + #4 commit `3fba7a61` |
