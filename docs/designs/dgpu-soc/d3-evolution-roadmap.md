# D3 演进 Roadmap (Draft)

> **版本**: v0.1 DRAFT — 2026-09-30
> **状态**: 🟡 DRAFT — 待评审（季度复审产物）
> **承接**: [D-AXI v1.4 §6-7 遗留议题](../pcie/driver-visible-minimal-soc.md) + [ADR-DGPU-07 演进 seam](../../adr/ADR-DGPU-07-minimal-soc-evolution-seam.md) + [ADR-DGPU-10 backing 字段命名](../../adr/ADR-DGPU-10-backing-naming-convention.md)
> **Owner**: CppTLM Team
> **配套**: [D-AXI v1.4 §6 遗留议题表](#6-v14-遗留议题与-d3-收编)

---

## 0. 摘要

D3 = D-AXI 完成后的下一阶段演进。围绕 3 个核心目标：

1. **真实 VRAM 控制器**: 引入 `VramControllerTLM` (HBM timing) 替代直接 memcpy
2. **GMMU PoC + SM 子系统**: 开启真实 GPU 计算路径
3. **D1 Display FB 同构**: 将 D1 32MB FB 也归 DGpuBoard 持有（冻结面解耦）

**触发条件**（AND）：
- D-AXI v1.4 已合并 + E2E 闭环 ✅ (2027-02-11 已达)
- 至少 1 个真实 driver 集成需求（UE 端 KFD/DRM 子集触发）
- 4 GMMU 7 能力设计 (ArchForge) 已冻结

**非目标**：
- ❌ 不重写冻结面 (`pcie_display_device.hh`, `pcie_endpoint_tlm.h`)
- ❌ 不引入新 ABI (per ADR-088 §D5)
- ❌ 不动 `cpptlm_emulator.h` C extern 接口

---

## 1. 触发条件 (D3 START gates)

| Gate | 判据 | 当前状态 (2026-09-30) | 来源 |
|------|------|---------------------|------|
| **G1**: D-AXI v1.4 merge | git tag + E2E pass + Oracle 评审通过 | ✅ 已达 (commit `429327d`) | `git log --grep="D-AXI v1.4"` |
| **G2**: 真实 driver 需求 | 至少 1 个 `[minimal_dgpu_soc][driver_visible]` E2E 用例覆盖 host→board 闭环 | ✅ 已达 (15 ABI + 4 BAR) | `test/test_dgpu_soc_minimal_via_abi.cc` |
| **G3**: GMMU 7 能力 SSOT | ArchForge `docs/architecture/20-gmmu-evolution-roadmap.md` 已冻结 + Oracle 评审通过 | ⏸ 未冻结（2026-09-30 季度复审后此 change 已 archive in CppTLM，归 ArchForge） | ArchForge openspec |
| **G4**: 工期预算 | 5-7 工作日（含 TDD 5 步 + Oracle 一轮） | 待定 | rdd-planner |

**Gate 决策**：
- G1+G2 ✅ → D3 可立即立项（D1 Display FB 子轨）
- G3 缺失 → GMMU/SM 子轨延后到 G3 通过
- 建议拆 D3 为 **D3a (D1 Display FB 同构)** + **D3b (GMMU/SM/VramController)**

---

## 2. D3 子轨拆分

### D3a (1-2 工作日)：D1 Display FB 同构 + MemoryTLM 容量策略

**目标**: 把 v1.4 §6 3 项遗留议题中 2 项收编。

| 子轨 | 来源议题 | 触发 |
|------|---------|------|
| **D3a-1** Display FB 收编 | §6 行 1: D1 32MB FB 同构 | D3 START gates G1+G2 满足 |
| **D3a-2** MemoryTLM capacity 同步 | §6 行 3: `capacity_gb=1` vs `vram_size_=8GB` | D3a-1 完成后 |

**D3a-1 实施要点**：
- 位置: `dgpu_board_shell.cc` 注入点（与 `pcie_memory_device` 并列）
- 接口: 复用 `set_backing_store(ptr, size)` 模式 (per ADR-DGPU-10 owner/injected 两级)
- 冻结面: `pcie_display_device.hh` 零修改（per ADR-DGPU-07 演进 seam）
- 测试: `[pcie][pcie-display]` 标签下新增 `display_fb_owner_is_board_` 测试家族

**D3a-2 实施要点**：
- 触发: D3a-1 完成后（Display FB 路径走 board 持有后）
- 内容: 评估 capacity 同步策略，输出方案选项
- 选项 1 (轻量): spec 显式声明 current behavior (memory 视角 1GB, driver 视角 8GB) 不变
- 选项 2 (中等): JSON `memory.params.capacity_gb` 自动派生 `min(vram_size_gb, default_cache_cap)`
- 选项 3 (重): 引入真 VramControllerTLM，由 controller 仲裁所有访问者容量视图

**验收**:
- `[pcie][pcie-display]` 全绿
- D-AXI v1.4 §6 行 1, 3 标记 ✅
- AGENTS.md "冻结面零 diff" 仍成立
- 新增 ADR: `ADR-DGPU-12-display-fb-ownership.md` (D1 归 board)

---

### D3b (5-7 工作日)：GMMU PoC + SM 子系统 + VramController

**目标**: 真实 GPU 计算路径 + HBM timing。

| 子轨 | 模块 | 依赖 |
|------|------|------|
| **D3b-1** VramControllerTLM | 继承 `MemoryTLM`, 加行缓冲 + bandwidth 上限 | D-AXI v1.4 seam (`handle_slave_port ↔ backing_ptr_`) |
| **D3b-2** MemoryClusterTLM | 多通道 HBM 控制器 | D3b-1 |
| **D3b-3** GMMU PoC | 7 能力最小实现 (per ArchForge GMMU SSOT) | G3 gate 通过 |
| **D3b-4** StreamingMultiprocessor | SM 完整实现 (supersede 当前 stub) | D3b-3 (GMMU 提供地址翻译) |

**D3b-1 实施要点**:
- 位置: `pcie_memory_device.cc::handle_slave_port()` ↔ `backing_ptr_` 之间 (per D-AXI §7 table)
- 接口: **零 API 变更** (controller 替代直接 memcpy; 端口/adapter/注册全不动)
- 行为: 添加 row buffer hit/miss 模型 + bandwidth cap (e.g., 800 GB/s)
- 测试: `[vram-controller][bandwidth]` 标签 + `[pcie]` 全绿无回归

**D3b-3 实施要点** (假定 G3 已通过):
- SSOT 来自 ArchForge, CppTLM 仓仅实施
- 7 能力: L1 PTE walk / L2 PTE walk / fault injection / cache coherence / permission check / range check / error reporting
- 接口: 跨仓契约 per HSK-9 (IComputeDevice 15 方法)

**验收**:
- 新增测试 ≥ 100 assertions
- `[minimal_dgpu_soc][gmmu]` E2E 通过
- PTX-EMU HSK-9 contract 验证通过
- `[pcie]` 0 regressions (从 79046 baseline 验证)

---

## 3. v1.4 §6 遗留议题收编映射

| §6 议题 | D3 收编计划 (本草案) | 实际子轨 |
|---------|---------------------|----------|
| **D1 PcieDisplayDevice 32MB FB 同构** | ✅ D3a-1 实施 | D3a-1 |
| **BAR1 doorbell offset 0x10010000 > BAR1 16MB 实窗** | ❌ **维持现状，不修** | — |
| **MemoryTLM `capacity_gb=1` vs `vram_size_=8GB` 交互** | ✅ D3a-2 评估 | D3a-2 (选项 1/2) |

**doorbell 议题结论** (2026-09-30 复审):
- 现 `dgpu_board_shell.cc:306-447` 在 BAR1 fast-path bound 检查**之前**先匹配 doorbell 路径
- `kBar1DoorbellOffset` 常量 (`dgpu_board_shell.hh:171`) 是 driver 测试虚拟地址合成
- 改常量 = 弄断 `[sdma][doorbell]` 既有测试
- **决策**: 维持现状。后续若真 driver 集成时 doorbell 物理地址 ≠ 0x10010000 触发, 才回头修。

---

## 4. 风险 + 缓解

| 风险 | 影响 | 概率 | 缓解 |
|------|------|------|------|
| 冻结面被无意修改 | ABI 破坏 (per ADR-DGPU-08) | 低 | pre-commit hook + `[freeze-surface]` 测试家族 |
| G3 gate 长时间未达 | D3b 子轨延后 | 中 | D3a 独立启动, 不阻塞 D3a |
| VramController 性能回归 | SoC 仿真变慢 | 中 | bandwidth cap 默认禁用, opt-in |
| GMMU 7 能力范围蔓延 | D3 工期失控 | 高 | Oracle 评审 + rdd-planner 工作量估算 |
| CppTLM vs ArchForge 边界模糊 | 重复实施 | 中 | 仅在 ArchForge GMMU SSOT 已冻结后才启动 D3b-3 |

---

## 5. 依赖 + 时序

```
D-AXI v1.4 ✅  ─────┐
                   ├─→ D3a-1 (Display FB 同构) [1-2d]
                   │
G3 (ArchForge)  ⏸  ├─→ D3b-3 (GMMU PoC) [2-3d]
                   │
D3b-3 完成        ├─→ D3b-4 (SM 完整) [2-3d]
                   │
D3a-1 完成        └─→ D3a-2 (MemoryTLM capacity) [1d]
```

**最短路径** (假定 G3 已达): D3a-1 → D3b-3 → D3b-4 → D3a-2 → D3b-1 → D3b-2 (顺序可调整)
**最长路径** (G3 未达): D3a-1 → D3a-2 + (等 G3) → D3b-1 → D3b-2 → D3b-3 → D3b-4

---

## 6. 开放问题 (需用户决策)

1. **D3 拆分**: D3a (立即启动, 1-2d) + D3b (等 G3, 5-7d) — 还是合并为单 D3？
2. **D3a-2 选项**: capacity 同步走选项 1 (不变) / 选项 2 (auto-derive) / 选项 3 (VramController)？
3. **D3b 工期**: 5-7 工作日估算是否合理？(Oracle 评审后可调整)
4. **D3 change 在哪仓?** D3a + D3b 都在 CppTLM 仓 (per 项目愿景: CppTLM = 框架 + 集成验证), 还是 D3b (GMMU/SM) 拆到 ArchForge?
5. **v2.0 全 TLM 化**: D-AXI v2.0 是否作为远期目标立项？详见 [soc-internal-tlm-design-target.md](soc-internal-tlm-design-target.md)（季度复审 2026-09-30 产物）

---

## 7. 下一步行动

- [ ] 季度复审产物：本草案归档至 `docs/designs/dgpu-soc/d3-evolution-roadmap.md`
- [ ] 用户评审 D3 拆分 + D3a-2 选项 + D3b 工期 + D3 仓位置
- [ ] 若批准 → 启动 rdd-arch → rdd-planner → rdd-builder 流程
- [ ] D3a-1 (Display FB 同构) 作为第一个 change 启动 (per §1 G1+G2 已达)

---

## 8. 参考文档

- D-AXI v1.4 §6-7 (遗留议题 + D3 演进 seam) — [docs/pcie/driver-visible-minimal-soc.md](../pcie/driver-visible-minimal-soc.md)
- ADR-DGPU-07 (演进 seam) — [docs/adr/ADR-DGPU-07-minimal-soc-evolution-seam.md](../../adr/ADR-DGPU-07-minimal-soc-evolution-seam.md)
- ADR-DGPU-10 (backing 字段命名 owner/injected) — [docs/adr/ADR-DGPU-10-backing-naming-convention.md](../../adr/ADR-DGPU-10-backing-naming-convention.md)
- ArchForge GMMU SSOT — `ArchForge/docs/architecture/20-gmmu-evolution-roadmap.md` (跨仓镜像)
- ArchForge SoC 架构 — `ArchForge/docs/architecture/` (跨仓镜像)

---

**维护**: CppTLM Team · **状态**: 🟡 DRAFT 待评审 · **评审入口**: §6 4 个开放问题