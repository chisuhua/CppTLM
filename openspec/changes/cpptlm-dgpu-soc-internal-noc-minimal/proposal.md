# cpptlm-dgpu-soc-internal-noc-minimal: SoC 内部 NoC (功能路由 + 接口兼容)

> **状态**: 📋 Proposed — 2026-10-08
> **作者**: CppTLM Team
> **前置**: `dgpu-realip-pin-contract` Change A/B/C/D ✅ 完成 (`dgpu-realip-pin-contract-change-d-completion` tag 落)
> **关联设计**: `docs/designs/dgpu-soc/soc-internal-noc-design.md` (本文档写完后生成)
> **关联 ADR**: ADR-DGPU-07 (Minimal → 完整 GPU 演进 seam) + AGENTS.md "仿真框架 vs 应用设计" 范围治理

---

## Why

`dgpu-realip-pin-contract` Change A (`Fabric2VramCtrlBundle`) 只覆盖了 GpuInternalFabric ↔ VramControllerTLM 一个桥接。**SoC 内部 4 个核心模块的端到端数据路径未落地**：

- GMMU PTE walk: GMMU → NoC → VRAM → NoC → GMMU (page table fetch + response)
- SDMA copy: SDMA → NoC → VRAM → NoC → SDMA (data path + completion)
- PCIe BAR decode: PCIe → NoC → GMMU/SDMA/VRAM (BAR0/1/2 routing)

驱动路径 `UsrLinuxEmu → DGpuBoard ABI` 需要前 2 条路径可工作才能验证 GMMU/SDMA 闭环。

**当前状态** (`include/tlm/cluster/gpu_internal_fabric.hh`):
- `GpuInternalFabric` 名字暗示 "GPU 内部"，实际只承担 1 个桥接 (vram_ctrl)
- `attach_crossbar` inner crossbar 是 dormant state (T25A.1 才会激活)
- **GMMU/SDMA/PCIe Controller 与 VRAM 之间没有显式 NoC 路由** — DGpuBoard::mmio_read/write 路径靠直接函数调用绕过

**问题**：
1. 物理 RTL 真实存在 SoC 内部 NoC (Arteris FlexNoC / Custom Mesh), TLM 必须有对应模块满足"接口对齐"目标
2. SDMA 切到 NoCFlitBundle 端口契约的迁移 (#4) 缺目标模块 (NoC) 接收方
3. GMMU page walk 与 SDMA copy 是 driver path 的 critical path, 必须可在 E2E 测试中验证

**主因 (按 ROI 排序)**:

1. **驱动路径闭环**: UsrLinuxEmu driver 通过 GMMU pt_base / SDMA 提交 / BAR 读写访问 SoC 资源。NoC 是这些访问的必经路径, 但当前是 "函数调用 bypass", 不可观测, 不可注入故障。
2. **物理 IP 接口对齐**: 本分支陈述目标 "TLM ↔ 物理 IP 边界对齐"。`architecture.md §2.4` 的 4-IP 表格已映射 Arteris FlexNoC → 真实物理 IP, 但 TLM 侧无对应实现。
3. **Future #4 (SDMA/GMMU 迁移 NoCFlitBundle) 缺接收方**: 已落地的 BUNDLE_FIELD_WIDTH 检查 (3fba7a61) 锁住 NoCFlitBundle 字段宽度, 但缺模块用 NoCFlitBundle 真正路由。
4. **GpuInternalFabric 名字与职责不匹配**: 名字说 "GPU 内部" 但只桥接 vram, 实际是 VRAM bridge。建议独立 `SocInternalNoc` 承担 SoC 范围路由。

**预期收益**:

- Driver path E2E: GMMU page walk + SDMA copy 可在 `[minimal_dgpu_soc][driver_visible]` 测试中真实运行 (而非函数调用 bypass)
- NoC 接口契约: `NoCFlitBundle` 18 字段 (mesh XY + VC + flit_type) 编译期检查 + 运行时路由
- 关注点分离: `GpuInternalFabric` 继续承担 VRAM bridge (现有), `SocInternalNoc` 承担 GMMU/SDMA/PCIe/VRAM 4 目标路由
- ArchForge scope 隔离: 完整 NoC 物理行为 (multi-VC / mesh / QoS) 留 ArchForge, CppTLM 只做"功能路由 + 接口兼容"

---

## What Changes

### 新模块 `SocInternalNoc`

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/tlm/cluster/soc_internal_noc.hh` | **新** (~80 行) | `SocInternalNoc` SimModule 类, 4 target setters + 路由表 + JSON 装配 |
| `src/tlm/cluster/soc_internal_noc.cc` | **新** (~100 行) | tick() 实现: 读 ingress NoCFlitBundle → 按 dst_node 路由 → 推到 target 队列 |
| `test/test_soc_internal_noc.cc` | **新** (~150 行) | 编译期契约 (BUNDLE_FIELD_WIDTH) + 运行时路由正确性 + E2E 与 GMMU/SDMA/PCIe 协作 |
| `include/modules_cluster.hh` | 改 (加 ~10 行) | `REGISTER_MODULE(tlm::cluster::SocInternalNoc)` 短名注册 (仿 PcieControllerIP precedent) |
| `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()` | 改 (加 ~15 行) | 装配 `soc_noc->set_pcie_target()` / `set_sdma_target()` / `set_gmmu_target()` / `set_vram_target()` |
| `docs/designs/dgpu-soc/architecture.md §2.4` | 改 (加 1 行) | 4-IP 表格新增 `SocInternalNoc` 行 (映射 Arteris FlexNoC) |
| `AGENTS.md` KEY INVARIANTS | 改 (加 1 条) | SocInternalNoc 接口契约 invariant |
| `docs/designs/dgpu-soc/soc-internal-noc-design.md` | **新** | 完整设计文档 (per openspec/changes/ 设计要求) |

### Frozen surface (0 修改)

- `include/abi/cpptlm_emulator.h` — 23 ABI 签名不动
- `include/tlm/gpu/pcie_endpoint_tlm.h` — 不动
- `include/tlm/gpu/pcie_display_device.hh` — 不动
- `include/tlm/pcie/pcie_bundles_tlm.hh` — 不动
- `include/tlm/vram_controller_tlm.hh` — 不动 (VramControllerTLM 继续由 GpuInternalFabric 桥接, 路径不变)
- `Fabric2VramCtrlBundle` (dgpu_fabric_bundles_tlm.hh) — 不动
- `NoCFlitBundle` (noc_bundles_tlm.hh) — 不动 (作为 wire format)

---

## Non-Goals

| # | 排除项 | 理由 |
|---|--------|------|
| 1 | 完整 SoC 内部 NoC 物理行为 (multi-VC 仲裁, mesh XY 路由细节, adaptive routing, congestion, QoS) | 属 ArchForge "SoC 架构建模" 范畴 (per AGENTS.md 季度复审产物, TLM 框架 + 基本 SoC 验证在本仓, 完整 SoC 建模在 ArchForge) |
| 2 | Cycle-approximate timing (latency / backpressure 传播) | 属 timing-mode SoC 范围, 当前 minimal_v1 不做; 留 D-AXI v2.x timing 扩展 |
| 3 | Crossbar vs Ring vs Torus 拓扑选择 | CppTLM 锁定 mesh 单拓扑; 拓扑选择属物理 IP 实现决策 |
| 4 | 网络功耗建模 | 与 TLM 功能验证无关, 属 RTL 仿真 |
| 5 | 修改 GpuInternalFabric (VRAM bridge) 现有职责 | GpuInternalFabric 继续只承担 VRAM bridge; 新模块独立承担 SoC 范围路由 (per 决策 #2 推荐选项 B) |
| 6 | 修改 4 个目标模块 (PcieControllerIP / SdmaEngineTLM / GmmuTLM / VramControllerTLM) 现有 wire format | 暂不切到 NoCFlitBundle 端口 (留给 #4 后续 PR); SocInternalNoc 通过 setters 持有 raw 指针, 不改目标模块接口 |

---

## Success Criteria

| AC | 描述 | 验证方法 |
|----|------|---------|
| AC-1 | 编译期: `BUNDLE_FIELD_WIDTH(NoCFlitBundle, ...)` 13 字段宽度锁 (per #4 commit 3fba7a61) | `static_assert` PASS, 测试断言通过 |
| AC-2 | 运行时: `SocInternalNoc::tick()` 按 `flit.dst_node` 正确路由到 4 target 之一 | 单元测试覆盖所有 4 路由 + 未知 dst_node graceful skip |
| AC-3 | 程序化桥接: 4 setters (`set_pcie_target` / `set_sdma_target` / `set_gmmu_target` / `set_vram_target`) + 4 accessors | 单元测试覆盖 set/get 双向 + nullptr 安全 |
| AC-4 | JSON 装配: `bind_memory_backings()` 通过 `soc_->getInternalInstance("soc_noc")` 装配 4 桥接 | `[minimal_dgpu_soc]` E2E 不回归 |
| AC-5 | Driver path 闭环: `[driver_visible]` 测试通过 GMMU/SDMA 真实可访问 (经 SocInternalNoc) | `[driver_visible]` 2/2 + `[minimal_dgpu_soc]` 12/12 全绿 |
| AC-6 | 0 回归: `[pcie]` / `[pcie-memory]` / `[realip]` / `[dgpu_soc_timing]` / `[tmu]` / `[router]` / `[phase6]` 全绿 | 7 套件 0 回归 |
| AC-7 | `openspec validate cpptlm-dgpu-soc-internal-noc-minimal --strict` PASS | validate 命令 |
| AC-8 | `scripts/test/docs_sync_check.sh --strict` PASS (新 soc-internal-noc-design.md 路径在文档中引用) | docs sync check |

---

## Risk & Mitigation

| 风险 | 概率 | 影响 | 缓解 |
|------|:----:|:----:|------|
| DGpuBoard::bind_memory_backings 装配顺序错乱 | 中 | 中 | SocInternalNoc 装配后必须验证 `[pcie-memory]` 不回归; 若 0 回归则视为兼容 |
| 4 target 模块接受 raw 指针但不接受 NoCFlitBundle 流 | 中 | 中 | 暂不切 wire format (Non-Goal #6); SocInternalNoc 持有指针但 tick() 仅做目标记录, 实际数据流不通过 NoC 节点 (本期); 等 #4 后续 PR 切到 NoCFlitBundle 流 |
| PcieControllerIP 是 SimModule, SdmaEngineTLM/GmmuTLM 是 ChStreamModuleBase, VramControllerTLM 是 MemoryTLM | 高 | 中 | raw 指针不依赖基类, 4 setter 类型各异但同模块 API; 测试覆盖 nullptr + 4 种类型 setter |
| 性能开销: 4 个 set/get 调用 + 路由查表 | 低 | 低 | minimal_v1 scope, 不在 critical path; 测验证 < 1% 性能影响 |

---

## Out-of-Scope (defer to future)

- **Change E.1**: SocInternalNoc 实际承载 NoCFlitBundle 数据流 (取代函数调用 bypass) — 需要 4 target 模块也切到 NoCFlitBundle 流, scope 较大
- **Change E.2**: Crossbar vs Ring 拓扑配置化 (per-port routing table) — 属 ArchForge SoC 架构建模
- **Change E.3**: multi-VC + congestion + QoS — 属 ArchForge 物理精度
- **Future #4 PR**: SDMA/GMMU 实际 wire format 切到 NoCFlitBundle — 与本 change 正交, 可并行
