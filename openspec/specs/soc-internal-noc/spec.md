# soc-internal-noc Specification

## Purpose
TBD - created by archiving change cpptlm-dgpu-soc-internal-noc-minimal. Update Purpose after archive.
## Requirements
### Requirement: soc-internal-noc-module-identity SocInternalNoc SimModule 定义

`SocInternalNoc` SHALL be a `SimModule` 派生类, 命名空间 `tlm::cluster`, 提供 4 target 程序化桥接 + 路由功能。

**Module Identity**:
- 名称: `SocInternalNoc`
- 命名空间: `tlm::cluster`
- 基类: `::SimModule`
- 物理对应: Arteris FlexNoC / Custom Mesh IP (per architecture.md §2.4)
- 构造: `SocInternalNoc(const std::string& name, EventQueue* eq)`

#### Scenario: 模块注册可发现 (AC-1)

- **GIVEN** `ModuleFactory::getModuleRegistry()` 包含 `"SocInternalNoc"` 键
- **WHEN** 任何 TU 在全局作用域触发 `REGISTER_MODULE(tlm::cluster::SocInternalNoc)`
- **THEN** `registry.find("SocInternalNoc") != registry.end()`
- **AND** 该注册表项 create lambda 可调用 `new SocInternalNoc(name, eq)`

#### Scenario: 短名可发现 (per PcieControllerIP precedent)

- **GIVEN** `modules_cluster.hh` 已 `#include` + 注册 `tlm::cluster::SocInternalNoc`
- **WHEN** `ModuleFactory::getModuleRegistry()` 查询
- **THEN** 同时存在 FQ 键 `"tlm::cluster::SocInternalNoc"` 与短名键 `"SocInternalNoc"` (幂等 ODR-safe)
- **NOTE**: 短名用于 JSON `"type": "SocInternalNoc"` (per dgpu_soc_v2_realip_p2.json 模式)

---

### Requirement: soc-internal-noc-4-target-bridge 4 Target Programmatic Bridge

`SocInternalNoc` SHALL 提供 4 个独立 target setter + accessor, 类型分别为:
- `tlm::pcie::PcieControllerIP*` (PcieControllerIP facade)
- `tlm::gpu::SdmaEngineTLM*` (SDMA engine)
- `tlm::gpu::GmmuTLM*` (GMMU page walker)
- `::VramControllerTLM*` (VRAM controller)

#### Scenario: set/get 双向 (AC-3)

- **GIVEN** `SocInternalNoc soc_noc("soc_noc", &eq)`
- **WHEN** `soc_noc.set_pcie_target(pcie_ptr)` 然后 `soc_noc.pcie_target()`
- **THEN** 第二次调用返回 `pcie_ptr` (相同指针)
- **AND** sdma/gmmu/vram_target() 返回 `nullptr` (未设置, 默认)

#### Scenario: 独立 4 setter 互不干扰

- **GIVEN** 仅 set `pcie_target` + `gmmu_target`
- **WHEN** 查 `sdma_target()` / `vram_target()`
- **THEN** 两者都返回 `nullptr`
- **NOTE**: 4 setter 互不依赖, 允许 partial 装配 (测试场景)

#### Scenario: nullptr safety (AC-3)

- **GIVEN** soc_noc 所有 4 target 都未设置 (默认 nullptr)
- **WHEN** 任何 route 操作触发
- **THEN** `soc_noc.flit_routed()` 仍可读 (返回 0)
- **AND** 不 crash / segfault

---

### Requirement: soc-internal-noc-4-node-id 4 Node ID 常量 (Mesh 寻址)

`SocInternalNoc` SHALL 暴露 4 个 `static constexpr uint32_t` 节点 ID 常量, 与 `architecture.md §2.4` 4-IP 表格顺序对齐:

| 常量 | 值 | 物理对应 |
|------|----|---------|
| `NODE_ID_PCIE` | 0 | PCIe Controller (DWC i5122) |
| `NODE_ID_SDMA` | 1 | SDMA engine (NVIDIA copy engine) |
| `GMMU` | 2 | GMMU (per dGPU 页表) |
| `VRAM` | 3 | VRAM controller (NVIDIA FB / AMD MC) |

#### Scenario: 节点 ID 唯一 + 单调

- **GIVEN** 4 NODE_ID_* 常量
- **WHEN** 静态断言
- **THEN** 4 值互不相等
- **AND** 顺序与 architecture.md §2.4 IP 表格顺序一致 (PCIE/SDMA/GMMU/VRAM)

---

### Requirement: soc-internal-noc-route-flit route_flit() 路由功能 (AC-2)

`SocInternalNoc::route_flit(const NoCFlitBundle& flit)` SHALL 读 `flit.dst_node`, switch 4 cases, 增相应统计:
- 已知 dst_node (0-3) → 增 `flits_routed_`
- 未知 dst_node (>3) → 增 `unknown_dst_count_`, 不 crash

#### Scenario: dst=NODE_ID_PCIE 路由

- **GIVEN** soc_noc 已 set_pcie_target(&pcie)
- **AND** flit.dst_node = NODE_ID_PCIE
- **WHEN** `soc_noc.route_flit(flit)`
- **THEN** soc_noc.flit_routed() == 1
- **AND** soc_noc.unknown_dst_count() == 0
- **AND** pcie 收到 flit (per Phase 1 桩的内部 queue 推送)

#### Scenario: dst=NODE_ID_SDMA/GMMU/VRAM 路由

- **GIVEN** 3 个 target 均 set
- **WHEN** 连续 route 3 个不同 dst_node flit (SDMA/GMMU/VRAM)
- **THEN** 每次都增 `flits_routed_` 1
- **AND** 3 次后 `flits_routed()` == 3
- **AND** 各自 target 收到 1 个 flit

#### Scenario: dst=99 (未知) graceful skip (AC-2)

- **GIVEN** soc_noc 4 target 均 set
- **WHEN** `route_flit(flit_with_dst_99)`
- **THEN** `unknown_dst_count()` == 1
- **AND** `flits_routed()` == 0 (不变)
- **AND** 不 crash / 不 assert

#### Scenario: nullptr target + 路由触发 (AC-3)

- **GIVEN** soc_noc 所有 4 target = nullptr (默认)
- **WHEN** 任何 dst_node 0-3 触发 route
- **THEN** 不 crash (graceful, 类似 D.2 优雅降级)
- **AND** 统计按需增 (flits_routed 或 unknown_dst_count)

---

### Requirement: soc-internal-noc-4ip-doc-sync 4-IP 边界映射 (文档同步, AC-1 间接)

`SocInternalNoc` SHALL 在 `architecture.md §2.4` 4-IP 表格中新增一行, 物理 IP 对应 Arteris FlexNoC / Custom Mesh:

| IP 边界 | 真实芯片对应 | CppTLM 类 | 端口契约 | State ownership |
|---------|--------------|-----------|----------|------------------|
| **`SocInternalNoc`** (新增) | Arteris FlexNoC / Custom Mesh IP | `include/tlm/cluster/soc_internal_noc.hh` (新) | `NoCFlitBundle` (per `noc_bundles_tlm.hh:26`, 18 字段) | stateless 路由表 (4 raw 指针 + 2 计数器) |

#### Scenario: 文档同步可发现

- **GIVEN** `docs/designs/dgpu-soc/architecture.md §2.4` 4-IP 表格
- **WHEN** grep `SocInternalNoc`
- **THEN** 1 命中 (新增行)
- **AND** 引用 `include/tlm/cluster/soc_internal_noc.hh` 路径真实存在

---

### Requirement: soc-internal-noc-json-wiring JSON 装配路径 (AC-4)

`DGpuBoard::bind_memory_backings()` SHALL 在 4 target 可用时, 通过 `soc_->getInternalInstance("soc_noc")` 装配 4 setter。

#### Scenario: 完整装配 (4 target 均存在)

- **GIVEN** `dgpu_soc_v2_realip_p2.json` 含 `soc_noc` 节点 (type=SocInternalNoc)
- **AND** DGpuBoard::init 已 inject pcie_ep / sdma / gmmu / vram
- **WHEN** `bind_memory_backings()` 执行
- **THEN** 4 setter 全部调用: `soc_noc->set_pcie_target/set_sdma_target/set_gmmu_target/set_vram_target`
- **AND** 5 桥接完成 (1 vram via GpuInternalFabric 既有 + 4 new via SocInternalNoc)

#### Scenario: 部分装配 (partial target)

- **GIVEN** soc_noc 已 inject, 但 4 target 中部分为 nullptr
- **WHEN** `bind_memory_backings()` 执行
- **THEN** nullptr 检查: 仅 set 非 nullptr 的 target, nullptr 跳过 (避免 crash)
- **AND** `if (target) soc_noc->set_target(target)` 模式

#### Scenario: dynamic_cast 失败 (soc_noc 缺失)

- **GIVEN** JSON 无 soc_noc 节点
- **WHEN** `bind_memory_backings()` 调 `soc_->getInternalInstance("soc_noc")` 返回 nullptr
- **AND** `dynamic_cast<tlm::cluster::SocInternalNoc*>(nullptr)` 返 nullptr
- **THEN** 整个 if 块跳过 (no crash, 4 setter 全部跳过)
- **AND** 测试可验证 (E2E `[minimal_dgpu_soc]` 0 回归)

---

### Requirement: soc-internal-noc-test-coverage 测试覆盖 (AC-1 间接, AC-2, AC-3)

`test/test_soc_internal_noc.cc` SHALL 提供 5+ 测试用例, tag `[realip][pin-contract][noc]`, 覆盖:

| 场景 | 期望 |
|------|------|
| 4 NODE_ID_* 唯一性 + 顺序 | `STATIC_REQUIRE` 4 互不相等 + 与 architecture.md 对齐 |
| 4 setter + accessor 双向 | 4 set/get 互不干扰 + nullptr 默认 |
| dst=PCIE/SDMA/GMMU/VRAM 路由 | 4 dst 路径 + flits_routed++ |
| dst=99 未知 | unknown_dst_count++ + 0 crash |
| nullptr target + 路由 | 0 crash + 统计正确 |

#### Scenario: 5 测试用例全绿

- **GIVEN** 编译 + 运行 `./build/bin/cpptlm_tests "[realip][pin-contract][noc]"`
- **THEN** 5+ test cases / ≥10 assertions 全绿
- **AND** 双 tag `[realip][pin-contract]` 防 vacuous pass (per handoff §9.2 #10)

---

### Requirement: soc-internal-noc-zero-regression E2E 0 回归 (AC-6, AC-7)

本 change SHALL 不引入任何现有测试套件回归。

#### Scenario: 7 核心套件全绿

- **GIVEN** 修复后 build
- **WHEN** 跑 7 套件
- **THEN** 全绿, 0 回归:
  - `[pcie]` 430/430 / 36672 assertions
  - `[pcie-memory]` 17/17 / 48 assertions
  - `[minimal_dgpu_soc]` 12/12 / 66 assertions
  - `[realip]` 24/24 / 223 assertions (含 [realip][pin-contract][noc] sub-tag)
  - `[dgpu_soc_timing]` 37/37 / 12024 assertions
  - `[driver_visible]` 2/2 / 29 assertions
  - `[tmu]` 11/11 / 103 assertions

#### Scenario: openspec validate --strict

- **WHEN** `openspec validate cpptlm-dgpu-soc-internal-noc-minimal --strict`
- **THEN** 退出码 0, 输出 "Change is valid"

#### Scenario: docs_sync_check --strict

- **WHEN** `./scripts/test/docs_sync_check.sh --strict`
- **THEN** 退出码 0, 输出 "所有路径引用都有效"
- **NOTE**: 新 design doc `soc-internal-noc-design.md` 路径在 AGENTS.md / architecture.md 引用, 必须可解析

