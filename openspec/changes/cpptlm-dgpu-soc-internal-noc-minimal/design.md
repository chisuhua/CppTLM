# Design — cpptlm-dgpu-soc-internal-noc-minimal

> **状态**: 📐 Draft (与 proposal.md 配套)
> **日期**: 2026-10-08

---

## 1. 设计目标

落地 SoC 内部 NoC 的"功能路由 + 接口兼容"最小实现，作为 UsrLinuxEmu driver path 闭环的必经节点。

### 1.1 CppTLM scope（在本仓实现）

| 维度 | 实现 |
|------|------|
| 模块 | `SocInternalNoc` (新) — SimModule 派生 |
| 路由功能 | tick() 读 ingress NoCFlitBundle, 按 `flit.dst_node` 路由到 4 target 之一 |
| 端口契约 | `NoCFlitBundle` (已有, 18 字段) 编译期 + 运行时契约 |
| 桥接模式 | 4 个 setter + accessor, 与 GpuInternalFabric::set_vram_target 同模式 |
| 装配 | `DGpuBoard::bind_memory_backings()` 通过 `soc_->getInternalInstance("soc_noc")` 调 setters |
| 测试 | `[realip][pin-contract][noc]` sub-tag, 与现有 A.5 测试共存 |

### 1.2 ArchForge scope（不在本仓，留 ArchForge）

- multi-VC 仲裁
- mesh XY 路由 / adaptive routing
- congestion control / backpressure
- latency / cycle-approximate
- QoS 调度
- 网络功耗建模
- 拓扑选择（crossbar vs ring vs torus）

→ ArchForge 范围（per AGENTS.md 季度复审产物：SoC 架构建模在 ArchForge 仓）

---

## 2. 架构

### 2.1 模块位置

```
include/tlm/cluster/
├── gpu_internal_fabric.hh    [现有] VRAM bridge（不动）
└── soc_internal_noc.hh       [新]   SoC 内部 NoC（4 target 路由）
```

### 2.2 SocInternalNoc 接口设计

```cpp
namespace tlm::cluster {

class SocInternalNoc : public SimModule {
public:
    explicit SocInternalNoc(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "SocInternalNoc"; }

    // 4 target setters (per GpuInternalFabric::set_vram_target pattern)
    void set_pcie_target(tlm::pcie::PcieControllerIP* target) noexcept;
    void set_sdma_target(tlm::gpu::SdmaEngineTLM* target) noexcept;
    void set_gmmu_target(tlm::gpu::GmmuTLM* target) noexcept;
    void set_vram_target(::VramControllerTLM* target) noexcept;

    // 4 accessors (nullptr-safe)
    tlm::pcie::PcieControllerIP* pcie_target() const noexcept;
    tlm::gpu::SdmaEngineTLM*    sdma_target() const noexcept;
    tlm::gpu::GmmuTLM*         gmmu_target() const noexcept;
    ::VramControllerTLM*        vram_target() const noexcept;

    // 4 node_id 常量 (mesh 寻址, flit.dst_node 范围)
    static constexpr uint32_t NODE_ID_PCIE = 0;
    static constexpr uint32_t NODE_ID_SDMA = 1;
    static constexpr uint32_t NODE_ID_GMMU = 2;
    static constexpr uint32_t NODE_ID_VRAM = 3;

    // Routing 入口 (test 入口 / future data path)
    void route_flit(const NoCFlitBundle& flit);

    // 统计
    uint64_t flits_routed() const noexcept { return flits_routed_; }
    uint64_t unknown_dst_count() const noexcept { return unknown_dst_count_; }

private:
    tlm::pcie::PcieControllerIP* pcie_ = nullptr;
    tlm::gpu::SdmaEngineTLM*    sdma_ = nullptr;
    tlm::gpu::GmmuTLM*         gmmu_ = nullptr;
    ::VramControllerTLM*        vram_ = nullptr;

    uint64_t flits_routed_ = 0;
    uint64_t unknown_dst_count_ = 0;
};

} // namespace tlm::cluster
```

### 2.3 Routing 语义

```
route_flit(flit):
    1. 读 flit.dst_node
    2. switch (flit.dst_node) {
        case NODE_ID_PCIE: assert(pcie_); ++flits_routed_; break;
        case NODE_ID_SDMA: assert(sdma_); ++flits_routed_; break;
        case NODE_ID_GMMU: assert(gmmu_); ++flits_routed_; break;
        case NODE_ID_VRAM: assert(vram_); ++flits_routed_; break;
        default: ++unknown_dst_count_; break;  // graceful skip
    }
```

**关键不变量**：
- `flit.dst_node` 与 4 个 target 节点 ID 一一对应
- 未知 dst_node 不 crash, 仅记数 (per D.2 优雅降级原则)
- 4 个 setter 可独立调用, 允许 partial 装配 (测试用)
- nullptr target + 路由触发 → `assert` 触发 (Release build 不 crash, 仅统计)

### 2.4 Node ID 分配

| 模块 | NODE_ID | 物理对应 |
|------|---------|---------|
| PcieControllerIP | 0 | PCIe Controller (DWC i5122) |
| SdmaEngineTLM | 1 | SDMA engine (NVIDIA copy engine) |
| GmmuTLM | 2 | GMMU (per dGPU 页表) |
| VramControllerTLM | 3 | VRAM controller (NVIDIA FB / AMD MC) |

→ 节点 ID 与 `architecture.md §2.4` 4-IP 表顺序对齐, 保持 RTL 团队 1:1 对照。

---

## 3. 装配流程

### 3.1 JSON 装配路径

`configs/dgpu_soc_v2_realip_p2.json` 加 `soc_noc` 节点：

```json
{
  "name": "soc_noc",
  "type": "SocInternalNoc",
  "params": {}
}
```

`ModuleFactory::simulate_instantiate` 通过 `REGISTER_MODULE` 找到 type 字符串 → `new SocInternalNoc(name, eq)` → 加入 `module_instances["soc_noc"]`。

### 3.2 DGpuBoard 桥接

`src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()` 末尾加：

```cpp
// E.1 (cpptlm-dgpu-soc-internal-noc-minimal): SoC 内部 NoC 桥接
if (auto* soc_noc = dynamic_cast<tlm::cluster::SocInternalNoc*>(
        soc_->getInternalInstance("soc_noc"))) {
    if (pcie_ctrl) soc_noc->set_pcie_target(pcie_ctrl);
    if (sdma)     soc_noc->set_sdma_target(sdma);
    if (gmmu)     soc_noc->set_gmmu_target(gmmu);
    if (vram)     soc_noc->set_vram_target(vram);
}
```

→ 5 桥接: vram 走 GpuInternalFabric 现有路径, 4 新桥接走 SocInternalNoc

---

## 4. 测试设计

### 4.1 编译期契约（`BUNDLE_FIELD_WIDTH` 复用）

参考 `dgpu-realip-pin-contract` Change A.2 (commit `e9ccadf2`) + #4 (commit `3fba7a61`)：

```cpp
TEST_CASE("SocInternalNoc tick() routes flit by dst_node", "[noc][realip][pin-contract]") {
    NoCFlitBundle flit;
    flit.dst_node = SocInternalNoc::NODE_ID_VRAM;
    BUNDLE_FIELD_WIDTH(NoCFlitBundle, dst_node, 32);
    // ... runtime routing check
}
```

→ 已由 #4 commit `3fba7a61` 覆盖, 本 change **不重复**添加 BUNDLE_FIELD_WIDTH 测试, 改引用既有。

### 4.2 运行时路由（新增）

| 场景 | 期望 | Tag |
|------|------|-----|
| dst=PCIE → 路由到 pcie_target | pcie_target() 收到 | `[realip][pin-contract][noc]` |
| dst=SDMA → 路由到 sdma_target | sdma_target() 收到 | `[realip][pin-contract][noc]` |
| dst=GMMU → 路由到 gmmu_target | gmmu_target() 收到 | `[realip][pin-contract][noc]` |
| dst=VRAM → 路由到 vram_target | vram_target() 收到 | `[realip][pin-contract][noc]` |
| dst=99 (未知) → graceful skip | unknown_dst_count++ | `[realip][pin-contract][noc]` |
| target=nullptr + 路由触发 | 不 crash, 计数不变 | `[realip][pin-contract][noc]` |

### 4.3 E2E 回归

| 套件 | 状态 | 期望 |
|------|------|------|
| `[pcie]` | 既有 | 0 回归 (430/430) |
| `[pcie-memory]` | 既有 | 0 回归 (17/17) |
| `[minimal_dgpu_soc]` | 既有 | 0 回归 (12/12) — 验证 JSON 装配 + bind_memory_backings 4 桥接 |
| `[realip]` | 既有 | 0 回归 (24/24) |
| `[driver_visible]` | 既有 | 0 回归 (2/2) — driver path 经 SocInternalNoc |

---

## 5. 设计权衡

### 5.1 为什么独立模块而非扩展 GpuInternalFabric

| 维度 | 独立 SocInternalNoc (推荐) | 扩展 GpuInternalFabric |
|------|---------------------------|-----------------------|
| 命名清晰度 | ✅ "SoC 内部 NoC" 准确 | ❌ "GPU 内部 Fabric" 实际承担 SoC 路由, 名字误导 |
| 关注点分离 | ✅ VRAM bridge vs NoC routing 是不同 abstraction | ❌ 单一模块承担 2 个职责 |
| 测试隔离 | ✅ NoC routing test 不需要 VRAM mock | ❌ 测试必须 mock VRAM |
| ADR-DGPU-07 seam | ✅ 新模块扩展更易 D3 完整化 | ❌ 既有模块扩展有 side effect 风险 |
| 代码改动量 | 中 (新模块 + 4 装配) | 小 (既有模块加 3 setter) |
| 风险 | 中 (新文件需更多测试) | 低 (复用既有测试) |

→ **选独立模块**（推荐）, 命名准确 + 关注点分离是核心, 改动量差异可接受。

### 5.2 为什么用 NoCFlitBundle 而非 Fabric2VramCtrlBundle

`NoCFlitBundle` 18 字段 (mesh XY + VC + flit_type) 对齐物理 IP 真实 wire format。
`Fabric2VramCtrlBundle` 8 字段 (subset) 是当前 GpuInternalFabric↔VramControllerTLM 专用, 不含 mesh 寻址。

→ SocInternalNoc ingress/egress 用 `NoCFlitBundle` (对齐物理), VramControllerTLM 内部仍接 `Fabric2VramCtrlBundle` (GpuInternalFabric 桥接, 已有)。

### 5.3 为什么本期不切 4 target 模块的 wire format

`PcieControllerIP::match_bar` 用 uint8_t + uint64_t + BarTarget, `SdmaEngineTLM` 5 端口用 `PcieTlpBundle`, `GmmuTLM` 用 `PcieTlpBundle`, `VramControllerTLM` 用 `Fabric2VramCtrlBundle`。

切到统一 `NoCFlitBundle` 需要改 4 个模块 + 装配层, scope 爆炸, 留 future change E.1。

→ 本期 SocInternalNoc 仅持 4 个 raw 指针, 路由表基于 `dst_node`, **不实际推动 flit 数据**。数据流仍走既有路径 (DGpuBoard::mmio_read/write 直调 + Fabric2VramCtrlBridge), SocInternalNoc 作为未来 E.1 的"路由骨架"。

---

## 6. 后续步骤

| 步骤 | 内容 | 估时 |
|------|------|------|
| 1 | 实现 `SocInternalNoc` (header + impl) | 1h |
| 2 | 注册 `REGISTER_MODULE` + 装配 `bind_memory_backings` | 30min |
| 3 | 写测试 (5 场景 + E2E 回归) | 1h |
| 4 | 更新 `architecture.md §2.4` + `AGENTS.md` KEY INVARIANTS | 30min |
| 5 | 写 `docs/designs/dgpu-soc/soc-internal-noc-design.md` | 30min |
| 6 | Build + openspec validate + docs_sync + 12 套件全绿 | 30min |
| 7 | Commit + tag `cpptlm-dgpu-soc-internal-noc-minimal-completion` | 15min |
| **总计** | | **4-5h** |
