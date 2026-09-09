# PCIe Endpoint 实施入口文档（双仓 SSOT）

> **定位**: 本文档是 UsrLinuxEmu ↔ CppTLM 双仓 **PCIe EP 驱动到硬件链路**所有实施工作的**集中入口**（Single Source of Truth Entry Point）。
> **状态**: Draft v0.1 (2026-09-09)
> **维护**: CppTLM + UsrLinuxEmu 架构组（跨仓同步）
> **目的**: 让任何进入 PCIe EP / dGPU E2E 主线工作的工程师，能够**从这里找到所有需要的文档、openspec change、实施路径、同步点、验证清单**，而不需要在双仓搜索
> **关联索引**:
> - 本文档**不是**架构 SSOT——架构 SSOT 见 [`pcie-endpoint-architecture.md`](pcie-endpoint-architecture.md)（硬件侧）+ [`docs/02_architecture/pcie-endpoint-architecture.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/02_architecture/pcie-endpoint-architecture.md)（**驱动侧同步**；v0.2 修订：原文本误用 CppTLM 自仓路径 `docs/soc_arch/architecture/16-...`，正确目标为 UsrLinuxEmu 仓 `docs/02_architecture/` 无编号风格）
> - 本文档**不是**roadmap——roadmap 见 [`pcie-ep-cpptlm-collaboration-roadmap.md`](../roadmap/pcie-ep-cpptlm-collaboration-roadmap.md)
> - 本文档**不是**SDMA 内部设计——见 [`sdma-engine-design.md`](sdma-engine-design.md)
> - 本文档**是**索引 + 路径图 + 同步点 + 验证清单的"导航器"

---

## §1 双仓文档地图

### §1.1 CppTLM 仓文档（5 核心文档）

| 文档 | 路径 | 角色 | 何时读 |
|------|------|------|-------|
| **pcie-endpoint-entry.md**（本文档）| `docs/soc_arch/architecture/18-pcie-endpoint-entry.md` | **入口（SSOT for navigation）** | 任何 PCIe EP 工作的**起点** |
| **pcie-endpoint-architecture.md** | `docs/soc_arch/architecture/16-pcie-endpoint-architecture.md` | **架构 SSOT（硬件侧）** | 了解跨仓架构 + 数据流/控制流时 |
| **sdma-engine-design.md** | `docs/soc_arch/architecture/17-sdma-engine-design.md` | **SDMA 内部设计**（§1-§14 全 14 章节）| 阶段 1.3a-1.3d 实施时 |
| **pcie-ep-cpptlm-collaboration-roadmap.md** | `docs/roadmap/pcie-ep-cpptlm-collaboration-roadmap.md` | **5+4 步实施 roadmap** | 路径规划 + 工期估算时 |
| **openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/** | `openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/` | **openspec change**（proposal + design + tasks + spec）| change 提案 + 13 ADDED Requirements 时 |

### §1.2 UsrLinuxEmu 仓文档（5 条目）

| 文档 | 路径 | 角色 | 何时读 |
|------|------|------|-------|
| **pcie-endpoint-entry.md**（本文档同源）| [`docs/02_architecture/pcie-endpoint-entry.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/02_architecture/pcie-endpoint-entry.md) | **入口（SSOT for navigation）** | 任何 PCIe EP 工作的**起点** |
| **pcie-endpoint-architecture.md** | [`docs/02_architecture/pcie-endpoint-architecture.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/02_architecture/pcie-endpoint-architecture.md) | **架构 SSOT（驱动侧）** | 了解驱动/HAL/bridge 架构时 |
| **pcie-bus-bridge-roadmap.md** | `docs/roadmap/pcie-bus-bridge-roadmap.md` | **总 roadmap**（v0.2.3 战略调整）| 阶段关系 + 跨轨道依赖时 |
| **openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/** | `openspec/changes/2026-09-09-5-5-7-cpptlm-cp-real-ification/` | **CP 真实化 change**（Deferred 待 CppTLM 5 步完成）| 5.5.7 重启时 |
| **openspec/changes/2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/** | `openspec/changes/2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/` | **kernel dispatch + DMA change**（Deferred 待 CppTLM 5 步完成）| 5.5.8 重启时 |

### §1.3 跨仓引用关系

```
┌─────────────────────────────────────────────────────────┐
│ 入口（SSOT for navigation）                                │
│ CppTLM/docs/soc_arch/architecture/18-pcie-endpoint-entry.md         │
│ UsrLinuxEmu/docs/02_architecture/pcie-endpoint-entry.md              │
└────────┬───────────────────────────────────────────────────┘
         │
         ├─→ CppTLM 架构 SSOT ─→ 跨仓引用 ─→ UsrLinuxEmu 架构 SSOT
         │   pcie-endpoint-architecture.md (硬件侧)            │
         │                                  ↓                  │
         │                            pcie-endpoint-architecture.md (驱动侧) │
         │
         ├─→ CppTLM SDMA 内部设计（阶段 1.3 实施）
         │   sdma-engine-design.md
         │
         ├─→ CppTLM 5+4 步 roadmap
         │   pcie-ep-cpptlm-collaboration-roadmap.md
         │
         ├─→ UsrLinuxEmu 总 roadmap
         │   pcie-bus-bridge-roadmap.md (v0.2.3)
         │
         ├─→ CppTLM openspec change (13 ADDED Requirements)
         │   2026-09-09-cpptlm-pcie-ep-foundation/
         │
         ├─→ UsrLinuxEmu openspec change (CP + DMA)
         │   2026-09-09-5-5-7-cpptlm-cp-real-ification/
         │   2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/
         │
         └─→ 5.5.6 P4.NEW-A/B/C/D + B.5 已 ship 代码（接线真实）
             sim_hardware/src/cpptlm/ (backdoor_endpoint.cpp + bridge.cpp + endpoint.cpp) + sim_hardware/src/pcie/ (host_bridge.cpp)
```

---

## §2 实施路径图（5+4 步）

### §2.1 总览（来自 roadmap）

```
阶段 1 基础必备（4.0-5.0 周，跨仓 blocker；= 0.5-1+0.5+2.5-3+0.5 求和）        阶段 2 性能增强（1 周）
┌─────────────────┬─────────────────┬─────────────────┐
│  阶段 1.1 PCIe EP │  阶段 1.2 MSI-X  │  阶段 1.3 DMA  │
│  基础（0.5-1 周）│  中断（0.5 周）  │  引擎（2.5-3 周）│
│                  │                  │  ┌────┬────┬───┐│
│  修复 #3 #5 #6 #7 │  修复 #4        │  │1.3a│1.3b│1.3│
│                  │                  │  │    │    │c/d│
│                  │                  │  └────┴────┴───┘│
│                  │                  │                 │
│  阶段 1.4 电源管理│  阶段 2.1 P2P   │                 │
│  （0.5 周）       │  + Resizable    │                 │
│                  │  BAR（1 周）    │                 │
└─────────────────┴─────────────────┴─────────────────┘
```

### §2.2 5+4 步详细映射

| 步骤 | 标题 | 工期 | 执行仓（实施→验证）| 对应 openspec change | 关键模块（5 端口 + Ring + Doorbell）| 当前状态 |
|------|------|:---:|------------------|----------------------|----------------------------------------|----------|
| **阶段 1.1** | PCIe EP 基础 | 0.5-1 周 | CppTLM → UsrLinuxEmu | `2026-09-09-cpptlm-pcie-ep-foundation` §1.1 | config space + 4 data path + race 修复 | 🔄 Proposed |
| **阶段 1.2** | MSI-X 中断 | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.2 | intr_cb 真实触发 + trigger_irq_async 接线 | 🔄 Proposed |
| **阶段 1.3a** | PCIe SDMA 基础 | 1 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §2-§6` | Ring Buffer + RPTR/WPTR + Doorbell + SG | 🔄 Proposed |
| **阶段 1.3b** | D2D SDMA 路径 | 0.5-1 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §10` | NoC 数据面 + 显存控制器 bypass | 🔄 Proposed |
| **阶段 1.3c** | dma_translate_cb + GART/IOMMU + CP→SDMA | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §8+§11` | 地址翻译链 + PM4 DMA opcode 0x4600-0x4900 | 🔄 Proposed |
| **阶段 1.3d** | SDMA 完成通知 | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.3 + `sdma-engine-design.md §9` | Fence + MSI-X 接线（#4）| 🔄 Proposed |
| **阶段 1.4** | 电源管理 | 0.5 周 | CppTLM → UsrLinuxEmu | 同上 §1.4 | D0/D3 + ASPM | 🔄 Proposed |
| **阶段 2.1** | P2P + Resizable BAR | 1 周 | CppTLM → UsrLinuxEmu | 同上 §2.1 | ARI 路由 + Resizable BAR Cap | 🔄 Proposed |
| **总计** | | **5.0-6.0 周** | | | |

### §2.3 关键路径（5+4 → UsrLinuxEmu 5.5.7/8/9 重启）

```
CppTLM 5+4 步 (5.0-6.0 周)
  ├─→ 阶段 1.3a 完成后: UsrLinuxEmu profile 测试升级 (data assertion)
  ├─→ 阶段 1.3c 完成后: dma_translate_cb 真实化（#2 修复）
  ├─→ 阶段 1.3d 完成后: MSI-X 中断链真实（#4 修复）
  └─→ 全 5+4 步完成后:
        ├─→ UsrLinuxEmu 5.5.7 重启 (CommandProcessor 真实化)
        ├─→ UsrLinuxEmu 5.5.8 重启 (kernel dispatch + DMA)
        └─→ UsrLinuxEmu 5.5.9 真机双轨验证
```

---

## §3 openspec change 全景

### §3.1 CppTLM 仓 change（当前 1 个，13 ADDED Requirements）

| Change | 范围 | 阶段覆盖 | 状态 |
|--------|------|---------|------|
| [`2026-09-09-cpptlm-pcie-ep-foundation`](https://github.com/chisuhua/CppTLM/openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/) | CppTLM PCIe EP 基础必备 + 性能增强 + 电源管理 + 完成通知 | **§1.1-1.4 + §2.1**（8 步全覆盖）| 🔄 Proposed |

**关键文档**：
- `proposal.md` — Why / What / Capabilities / Impact（含 7 修复 + 5 步建议）
- `design.md` — 技术设计（含 7 错误定位 + 阶段 1.3 细分 4 子阶段）
- `tasks.md` — TDD 5 步结构（8 个阶段任务）
- `specs/cpptlm-pcie-ep-foundation/spec.md` — 13 ADDED Requirements（覆盖 7 修复）

### §3.2 UsrLinuxEmu 仓 change（当前 3 个，已 ship 1 个 + Deferred 2 个）

| Change | 范围 | 状态 |
|--------|------|------|
| [`2026-09-08-5-5-6-cpptlm-ep-binding`](../archive/2026-09-08-2026-09-08-5-5-6-cpptlm-ep-binding/) | 5.5.6 dGPU E2E 主线 #1（背门端点 + bridge dlopen 23 ABI + hal_cpptlm 3 op + backend 选择）| ✅ Archived（接线真实，Oracle 9.5/10）|
| [`2026-09-09-5-5-7-cpptlm-cp-real-ification`](../2026-09-09-5-5-7-cpptlm-cp-real-ification/) | 5.5.7 dGPU E2E 主线 #2（CommandProcessor 真实化）| ⏸️ Deferred（待 CppTLM 5+4 步完成）|
| [`2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma`](../2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/) | 5.5.8 dGPU E2E 主线 #3（kernel dispatch + DMA）| ⏸️ Deferred（同上）|
| **5.5.7.1 P5.NEW-A**（已在 `2026-09-09-5-5-7-cpptlm-cp-real-ification/specs/`）| Profile 真实化验证（独立 P5.NEW-A 任务）| ✅ Oracle 9.4/10 |

### §3.3 5.5.6-5.5.9 状态（来自 UsrLinuxEmu roadmap v0.2.3）

| 阶段 | 标题 | 状态 | 阻塞条件 |
|------|------|------|---------|
| **5.5.6** | dGPU E2E 主线 #1 — 真实 CppTLM EP | ✅ Archived | — |
| **5.5.7** | dGPU E2E 主线 #2 — CommandProcessor | ⏸️ Deferred | CppTLM 5+4 步完成 |
| **5.5.8** | dGPU E2E 主线 #3 — kernel dispatch + DMA | ⏸️ Deferred | CppTLM 5+4 步完成 |
| **5.5.9** | dGPU E2E 主线 #4 — 真机双轨验证 | 📋 待启动 | 5.5.7 + 5.5.8 完成 |

---

## §4 架构核心概念（5 分钟理解）

### §4.1 双层 DMA 架构

```
┌─────────────────────────────────────────┐
│ CommandBuffer (PM4/AQL) — 调度层        │
│ 驱动构造命令序列，提交到 CP 解析分发      │
└──────────┬──────────────────────────────┘
           │ PM4 命令包
           ▼
┌──────────────────────────────────────────────┐
│ Command Processor (CP) — 翻译官              │
│ Fetch PM4 → Decode → Dispatch 到各引擎     │
│ dma_req[2] 端口转发 DMA 类到 SDMA          │
└──────────┬───────────────────────────────┘
           │ SdmaRingEntry (CP→SDMA)
           ▼
┌──────────────────────────────────────────────┐
│ SDMA Engine — 执行层                          │
│ Ring Buffer + RPTR/WPTR + Doorbell + FSM      │
│  5 端口: desc_in/mem_in/mem_out/host_out/done │
└──────────┬───────────────────────────────┘

> **⚠️ 目标态 vs 现状**：上图为 5+4 步**完成后**的目标架构。**现状**（2026-09-09）：SDMA 为 descriptor 直投（`sdma_engine_tlm.cc`，Ring Buffer/RPTR/WPTR/Doorbell **尚不存在**，阶段 1.3a 待建）；CP 无 DMA 类分发（阶段 1.3c 待建）。找符号/测试 target 前先看 §11.3 代码事实与 §2.2 状态列。
           │ PCIe TLP (H2D/D2H) / NoC (D2D)
           ▼
┌──────────────────────────────────────────────┐
│ GPU 内部 NoC → 显存控制器 → VRAM            │
│ 或 PCIe Controller → 系统内存               │
└──────────────────────────────────────────────┘
```

### §4.2 4 层 PCIe 能力框架

| 层级 | 范围 | 本 roadmap 实施 |
|------|------|----------------|
| **基础必备** | PCIe EP + MSI-X + DMA + 电源 | ✅ §2 5+4 步 |
| **性能增强** | P2P + Resizable BAR + 原子操作 + 带宽优化 | ✅ 阶段 2.1 |
| **虚拟化必备** | SR-IOV + VF 配置 + VF 中断隔离 | ❌ 排除（移交 VFIO） |
| **高级可选** | CXL/NTB/TPH/ATS/PRI/PASID | ❌ 排除（后续阶段） |

### §4.3 跨仓边界（23 ABI 契约 / 22 = 绑定子集）

```
UsrLinuxEmu (driver)                  CppTLM (hardware 仿真)
  GpgpuDevice (drv/ioctl)              23 ABI functions
    ↓                                     ↑
  HAL struct (71 fn-ptrs)              cpptlm_emulator_*
    ↓                                     ↑
  CpptlmBridge (23 ABI dlopen；         DGpuBoard / PcieEndpointIP / SDMA / CmdProc
                  22 = 绑定子集)         ↑
                              dlopen("libcpptlm_emulator.so")
```

**核心约束**：23 ABI 函数签名不变（5 端口 wire-format 冻结，HAL append-only）

> **ABI 数字三口径说明**（mirror 自 UsrLinuxEmu 仓 `pcie-endpoint-entry.md §4.3`，per [ADR-088 §D5](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) 冻结契约：基础 6 + callback typedef 4 + register 1 + 板卡扩展 8 + MSI-X 3 + DMA translate 1 = 23）：
> - **23** = 契约（19 forward functions + 4 callback typedefs；per `include/abi/cpptlm_emulator.h` 文件头注释）
> - **22** = 5.5.6 `bridge.cpp` dlsym 实际绑定数（19 契约函数 + 3 adapter 扩展 `open/close/get_adapter_info`，见 ADR-092）；经 `nm -D libcpptlm_emulator.so` 实测，`.so` 实际导出亦为 22——契约 23 中 4 个 callback typedef 非函数符号，不参与 dlsym/导出计数
> - **71** = `struct gpu_hal_ops` 函数指针数（去重 unique 符号；canonical SSOT 由 UsrLinuxEmu `tools/docs-audit.sh §1.5` 强制执行）。v0.3 修订：原 7f4d26a4 commit 误用 73（`grep -c "(\*"` 错算，含 `(*callback)`/`(*handler)` 两个嵌套参数名），已更正。
> - **结论**：23 = 契约；22 = 绑定数 = 导出数；71 = HAL fn-ptrs（unique 计数）。原"24"/"73"口径经核对实测均不成立。

> **🚧 可改 / 禁改边界（全局，非仅验证项）**：
> - ❌ **禁改**：本仓 `src/abi/cpptlm_emulator.h`（声明冻结，per §7.3）；23 ABI 签名；5 端口 wire-format；UsrLinuxEmu 仓 `plugins/gpu_driver/drv/`（ADR-036 三区分 + §7.3 零修改约束）
> - ✅ **可改**：`src/tlm/`（实现 + 仿真）；`test/`（新增测试）

---

## §5 跨仓同步点（实施时序）

### §5.1 同步检查清单

| 时机 | UsrLinuxEmu 端 | CppTLM 端 | 验证命令 |
|------|----------------|----------|---------|
| **阶段 1.1 完成后** | 测试断言升级（CHECK → REQUIRE + buf）| cfg space + 4 data path + race 修复 | `ctest -R profile_real` |
| **阶段 1.2 完成后** | — | msix intr_cb 触发验证（200ms 内 ≥1） | `test_msix_*` |
| **阶段 1.3a 完成后** | — | Ring Buffer + RPTR/WPTR + SG | `test_sdma_ring_rptr_wptr` |
| **阶段 1.3b 完成后** | D2D 不走 host_out 断言 | NoC 数据面 + 显存控制器 | `test_d2d_noc_path` |
| **阶段 1.3c 完成后** | dma_translate identity 断言 | translate_cb 真实化（#2）| `test_dma_translate_iommu` |
| **阶段 1.3d 完成后** | msix 触发后 driver ISR | Fence + MSI-X 接线（#4）| `test_sdma_fence` |
| **阶段 1.4 完成后** | reset path 验证 | D0/D3 + ASPM | `test_pm_state` |
| **阶段 2.1 完成后** | P2P 接入点 + Resizable BAR | ARI + Resizable BAR Cap | `test_p2p_dma` |
| **全 5+4 步完成后** | 5.5.7 重启 + 5.5.8 重启 + 5.5.9 启动 | openspec change archive | Oracle 终审 ≥9.0 |

### §5.2 实施同步约束

- **CppTLM 端先行**（架构补完 + 代码实施）
- **UsrLinuxEmu 端同步**（每子阶段完成后升级断言）
- **commit 节奏**：每个子阶段一个 commit（CppTLM）+ 一个 commit（UsrLinuxEmu）
- **Oracle 复审点**：每个阶段完成后触发

---

## §6 关键决策（已固化）

> **选择列图例**：单字母为该决策在 Oracle/设计评审中的**选项代号**——**X** = 选项 X（接受现状语义 + 对方仓修复）；**P** = 选项 P（TaskRunner 直调，不经 drv/）；**D** = 选项 D（保持既有 opt-in 默认）；其余行为文字描述即最终选择。§7.2 的"（X/P/D）"引用同此。

| ID | 决策 | 选择 | 影响 |
|----|------|------|------|
| **D.1** | -ETIMEDOUT 语义 | X（接受 + CppTLM 修复 race）| 5.5.7 CP attach 不变 |
| **D.2** | adapter op 接入点 | P（TaskRunner 直调 HAL adapter_*）| 零 drv/ 改动 |
| **D.3** | ctest WORKING_DIRECTORY | D（保持 opt-in 模式）| 169 baseline 不变 |
| **D.4** | 5 端口 wire-format | 冻结（5 端口索引顺序锁定）| 23 ABI 兼容 |
| **D.5** | Ring Buffer 引入 | 作为 desc_in 前端（不变 wire-format）| 阶段 1.3a 双轨过渡 |
| **D.6** | SDMA 完成通知 | MSI-X + Fence 双轨 | 阶段 1.3d |

---

## §7 验证清单（用户必查）

### §7.1 阶段完成验证

- [ ] 阶段 1.1 完成：4 数据通路 roundtrip + config space 真实化
- [ ] 阶段 1.2 完成：msix_update_pending 触发 intr_cb（200ms 内 ≥1 次）
- [ ] 阶段 1.3a 完成：Ring Buffer + RPTR/WPTR + SG + Doorbell 绑定
- [ ] 阶段 1.3b 完成：D2D 路径不经过 PCIe（host_out 零事务断言）
- [ ] 阶段 1.3c 完成：dma_translate_cb 真实调用（identity mapping）
- [ ] 阶段 1.3d 完成：Fence + MSI-X 接线（#4 修复）
- [ ] 阶段 1.4 完成：D0 ↔ D3 切换 + ASPM
- [ ] 阶段 2.1 完成：P2P + Resizable BAR
- [ ] 全 5+4 步 Oracle 审查 ≥ 9.0/10
- [ ] UsrLinuxEmu 5.5.7 P5.NEW-A profile 测试稳定（mmio ret==0 + buf 真实数据）

### §7.2 跨仓集成验证

- [ ] CppTLM `test/test_cpptlm_emulator_abi.cc` 全 PASS（单数 `test/`，无 `abi/` 子目录）
- [ ] UsrLinuxEmu `test_bridge_kcpptlm_profile_real_standalone` 5/5 PASS + data assertion
- [ ] `ctest` 双向全绿
- [ ] docs-audit 双仓 PASS
- [ ] D.1/D2/D3 决策不变（X/P/D）

### §7.3 架构约束保持

- [ ] UsrLinuxEmu `drv/` 零修改（5+4 步实施期间 + 完成后）
- [ ] UsrLinuxEmu HAL append-only（ADR-023 §D4 不变）
- [ ] 23 ABI 函数签名不变（仅行为从 stub → 真实）
- [ ] CppTLM `src/abi/cpptlm_emulator.h` 不变
- [ ] 5 端口 wire-format 冻结（D.4）

---

## §8 工作场景速查（"我在做 X，应该看哪些文档"）

### §8.0 我是新人，第一次进入 PCIe EP 工作

1. **读序**（约 40 分钟）：§1 文档地图 → §4 核心概念（注意 §4.1 目标态注记）→ §2.3 关键路径 → §7.3 架构约束
2. **当前能做什么**（全部 5+4 步 🔄 Proposed 期间）：
   - ✅ 阅读 + 挑错：双仓文档/spec 评审（本文档 §11、UsrLinuxEmu spec 13 ADDED Requirements）
   - ✅ 跑现状验证：`ctest -R profile_real`（UsrLinuxEmu 仓）、`test/test_cpptlm_emulator_abi.cc`（CppTLM 仓）
   - ✅ 认领预备任务：§8.3 测试断言升级预案的 dry-run（不改代码，先写 diff 草稿）
   - ⏸️ 不可认领：阶段 1.1-2.1 实施（待 CppTLM change 批准启动）；5.5.7/5.5.8（⏸️ Deferred）
3. **可动 / 禁动区域**：见 §4.3 末尾边界框（SSOT）；新人请先读该节再动手
4. **第一联系人**：跨仓同步问题 → 见 §5 同步点负责人（架构组）

### §8.1 我是 CppTLM 开发者，要实施阶段 1.1（PCIe EP 基础）

1. **先读**：[`pcie-endpoint-architecture.md`](pcie-endpoint-architecture.md) §2.1-2.2（数据流）
2. **再读**：[`openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/spec.md`](../../openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/specs/cpptlm-pcie-ep-foundation/spec.md)（13 ADDED Requirements 中关于 PCIe EP 基础的部分）
3. **实施**：`src/tlm/gpu/dgpu_board_shell.cc`（修复 #3 + #5 + #6 + #7）
4. **测试**：新建 `test/test_cpptlm_emulator_abi_*.cc`（单数 `test/`，无 `abi/` 子目录）
5. **验证**：从 UsrLinuxEmu 仓跑 `ctest -R profile_real`

### §8.2 我是 CppTLM 开发者，要实施阶段 1.3a（SDMA Ring Buffer）

1. **必读**：[`sdma-engine-design.md`](sdma-engine-design.md) §2-§6（核心 5 章节）
2. **对照**：[`openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md`](../../openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md) §3.3（阶段 1.3 细分）
3. **实施**：`src/tlm/gpu/sdma_engine_tlm.cc` 扩展 + 新建 `sdma_ring_buffer.h/cc`
4. **测试**：新建 `test/test_sdma_ring_rptr_wptr.cc`（单数 `test/`，无 `sim_hardware/`，扩展 `.cc`）
5. **验证**：5 端口 wire-format 不变 + Ring→descriptor 转换内部完成

### §8.3 我是 UsrLinuxEmu 开发者，要升级测试断言

1. **必读**：[`pcie-endpoint-architecture.md`](pcie-endpoint-architecture.md) §2.4.1（SDMA 接入点）
2. **必读**：[CppTLM/docs/soc_arch/architecture/17-sdma-engine-design.md](https://github.com/chisuhua/CppTLM/blob/main/docs/soc_arch/architecture/17-sdma-engine-design.md) §6（Packet 格式）
3. **修改**：`tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp`：
   - `CHECK(ret != -ENOSYS)` → `REQUIRE(ret == 0)` + `INFO("ret=" << ret)`
   - 加 buf 内容断言（写入 0xDEADBEEF → 读回相等）
4. **验证**：从 UsrLinuxEmu 仓跑（`cd /workspace/project/UsrLinuxEmu && ./build/bin/test_bridge_kcpptlm_profile_real_standalone`；该二进制是 UsrLinuxEmu Catch2 产物，CppTLM 仓无此文件）

### §8.4 我是架构师，要做跨仓对齐

1. **入口**：本文档（`pcie-endpoint-entry.md`）
2. **双仓架构 SSOT**：
   - CppTLM: `docs/soc_arch/architecture/16-pcie-endpoint-architecture.md`（硬件侧）
   - UsrLinuxEmu: [`docs/02_architecture/pcie-endpoint-architecture.md`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/02_architecture/pcie-endpoint-architecture.md)（驱动侧；v0.2 修订：原文本误用 CppTLM 自仓路径）
3. **跨仓引用**：两个 SSOT 互相 cross-reference
4. **变更影响评估**：任何 PCIe EP 改动需同步更新两个 SSOT

### §8.5 我是 PM，要看进度

1. **总 roadmap**：[`pcie-ep-cpptlm-collaboration-roadmap.md`](../roadmap/pcie-ep-cpptlm-collaboration-roadmap.md)（5+4 步 + 5.0-6.0 周工期）
2. **当前状态**：
   - 阶段 1.1-1.4 + 2.1：🔄 Proposed（待 CppTLM 实施）
   - 阶段 1.3 已细分 4 子阶段（1.3a/1.3b/1.3c/1.3d）
   - UsrLinuxEmu 5.5.7/5.5.8：⏸️ Deferred（待 CppTLM 5+4 步完成）
3. **里程碑**（**累计周** = 阶段完工时点；M6-M8 是启动时点而非"+X 周"）：
   - M1 (阶段 1.1): 0.5-1 周
   - M2 (阶段 1.2): 1.0-1.5 周
   - M3 (阶段 1.3 4 子步完成): 3.5-4.5 周（= M2 + 1.3a 1 + 1.3b 0.5-1 + 1.3c 0.5 + 1.3d 0.5 = 2.5-3.0）
   - M4 (阶段 1.4): 4.0-5.0 周（= M3 + 0.5）
   - M5 (5+4 步全部完成 / 阶段 2.1): **5.0-6.0 周**（5.5.7 启动门）
   - M6 (5.5.7 启动): M5 完成时
   - M7 (5.5.7 完成 / 5.5.8 启动): M5 + 1-2 周（per 5.5.7 proposal 工期）= **6.0-8.0 周**
   - M8 (5.5.8 完成 / 5.5.9 启动): M7 + 2-3 周（per 5.5.8 proposal 工期）= **8.0-11.0 周**
   - M9 (5.5.9 真机双轨验证完成): M8 + 4-6 周 = **12.0-17.0 周**
   - M8 (5.5.9 启动): +4 周

---

## §9 风险与回退

| 风险 | 等级 | 回退 |
|------|:----:|------|
| 阶段 1.1 mmio race 修复影响 5.5.7.1 profile 测试 | 中 | timeout 延长 + sim_loop tick 频率提升 |
| 阶段 1.3 SDMA 实施量大（现有 descriptor 直投 → Ring Buffer 重构）| 高 | 阶段 1.3c 过渡期保留 5 端口 wire-format |
| 阶段 1.3c dma_translate_cb 真实调用触发 UsrLinuxEmu cb 错误处理 | 中 | cb 失败 fallback（phys = iova identity）|
| 阶段 1.3d 中断链修复后 msix 测试不稳定 | 中 | 测试用 100ms 超时 + retry 1 |
| 阶段 1.4 电源管理状态切换影响 profile 加载 | 低 | profile 加载时强制 D0 |
| 跨仓协调延迟（CppTLM 实施 → UsrLinuxEmu 升级断言）| 中 | 每子阶段独立 commit，可异步同步 |
| Oracle 复审不通过 | 中 | 两条评审线分轨：5.5.x line 已过 9.0+（5.5.6 9.5 + 5.5.7.1 9.4），PCIe EP 协同 line 当前最佳 8.7/10 < 9.0（评审 3.5 + 3.5 + 8.7）；如 5+4 步后仍未达 9.0，需 AD-hoc 加 Oracle 评审轮次 |

---

## §10 文档维护规则

### §10.1 新增 PCIe EP 文档时

1. **先更新本文档 §1**（文档地图）
2. **再更新对应 SSOT 文档**（架构 / SDMA 设计 / roadmap / change）
3. **commit 节奏**：本文档 + 受影响文档同 commit
4. **命名规范**：
   - `pcie-*.md` — PCIe EP 相关
   - `sdma-*.md` — SDMA 相关
   - 跨仓引用用完整 URL（`https://github.com/chisuhua/CppTLM/blob/main/docs/...`），owner = `chisuhua`（与 git remote 一致）

### §10.2 修改现有 PCIe EP 文档时

1. **更新文档地图**：在本文档 §1 标记版本变化
2. **更新修订记录**：在文档自身 §修订记录
3. **跨仓同步**：涉及双仓 API 改动时同步两个仓的文档

### §10.3 归档已废弃 PCIe EP 文档时

1. 移到 `archive/` 目录
2. 在本文档 §1 标记 "已归档"
3. 保留链接（Git 历史可访问）

---

## §11 关联资源

### §11.1 ADR（架构决策）

| ADR | 内容 | 关联 |
|-----|------|------|
| [ADR-023 HAL append-only](../../00_adr/adr-023-hal-interface.md) | HAL 71 fn-ptrs append-only（v0.3 修订：原 73 来自 `grep -c "(\*"` 错算，含 `(*callback)`/`(*handler)` 嵌套参数名；以 UsrLinuxEmu `tools/docs-audit.sh §1.5` 强制值 71 为准）| §4 §6 D.4 |
| [ADR-088 dGPU 完整仿真](../../00_adr/adr-088-dgpu-complete-simulation.md) | dGPU 仿真边界 + 23 ABI | §4 §1 |
| [ADR-091 4 象限布局](../../00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) | 4 象限 + PCIe tier | §4.2 |
| [ADR-092 HAL adapter + bypass binding](../../00_adr/adr-092-hal-adapter-and-bypass-binding.md) | 3 个后端 + bypass | §4.3 |

### §11.2 spec（功能规范）

| spec | 内容 |
|------|------|
| [`cpptlm-pcie-ep-foundation/spec.md`](../../openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/specs/cpptlm-pcie-ep-foundation/spec.md) | 13 ADDED Requirements |
| [`sdma-engine-tlm/spec.md`](../../openspec/specs/sdma-engine-tlm/spec.md) | SDMA 组件 spec |
| [`dgpu-board-adapter-info/spec.md`](../../openspec/specs/dgpu-board-adapter-info/spec.md) | DGpuBoard adapter 信息通道（per ADR-092）|

### §11.3 代码事实

| 模块 | 文件 | 行数 | 说明 |
|------|------|------|------|
| SDMA 引擎（现有）| `src/tlm/gpu/sdma_engine_tlm.cc` | 420 | descriptor 直投，阶段 1.3a 待扩展 |
| Command Processor（现有）| `src/tlm/gpu/command_processor_mvp.cc` | 149 | 5-state FSM，阶段 1.3c 待扩 DMA 类 |
| DGpuBoard（现有）| `src/tlm/gpu/dgpu_board_shell.cc` | 445 | 7 错误修复点 |
| CpptlmBridge（UsrLinuxEmu 已 ship）| `sim_hardware/src/cpptlm/bridge.cpp` | 405 | 23 ABI dlopen + 4 data path |
| BackdoorEndpoint（UsrLinuxEmu 已 ship）| `sim_hardware/src/cpptlm/backdoor_endpoint.cpp` | 390 | 5 ule_dgpu_* functions |
| HAL cpptlm（UsrLinuxEmu 已 ship）| `plugins/gpu_driver/hal/hal_cpptlm.cpp` | 79 | 3 adapter op 真化 |
| GpgpuDevice（UsrLinuxEmu 已 ship）| `plugins/gpu_driver/drv/gpgpu_device.cpp` | 1137 | ioctl 派发表（**41** IOCTL，per `gpgpu_device.h:27 kNumIoctls = 41`）|

---

## §12 修订记录

- **v0.1** (2026-09-09, Draft): 初版,基于 2026-09-08 文档重命名 + 2026-09-09 战略调整 + 5+4 步 roadmap + SDMA 内部设计
  - §1 双仓文档地图（5 CppTLM + 4 UsrLinuxEmu 核心文档）
  - §2 实施路径图（5+4 步 + 4.5-6.5 周）
  - §3 openspec change 全景（CppTLM 1 + UsrLinuxEmu 3 已 ship + Deferred）
  - §4 架构核心概念（双层 DMA + 4 层 PCIe + 22 ABI 边界）
  - §5 跨仓同步点（9 个时序检查清单）
  - §6 关键决策（6 条固化决策）
  - §7 验证清单（阶段完成 + 跨仓集成 + 架构约束）
  - §8 工作场景速查（5 个常见场景）
  - §9 风险与回退（7 条）
  - §10 文档维护规则
  - §11 关联资源（4 ADR + 3 spec + 7 代码模块）

- **v0.2** (2026-09-09, 7f4d26a4 commit): 跨仓镜像 UsrLinuxEmu 端 entry.md v0.2 同步
  - §1.2 标题 4 核心文档 → 5 条目
  - §2.2/§2.3 总工期 4.5-6.5 → 5.0-6.0 周（阶段求和正确值）
  - §3.2/§4.3/§6/§7/§11.3 ABI 口径 22 → 23（**含未在 commit message 声明的副作用**，且未补 §4.3 ABI 三口径脚注）
  - §4.3 图 HAL 71 → 73 fn-ptrs（**错误**：73 来自 `grep -c "(\*"` 错算嵌套参数名；canonical SSOT = 71）
  - §11.1 ADR-023 HAL 71 → 73（同上错误）
  - §12 v0.1 描述项被改写为新数字（**违反 append-only 纪律**，本 commit v0.3 还原）

- **v0.3** (2026-09-09, post-Oracle 复审): 修正 7f4d26a4 引入的 4 类错误
  - **§4.3 ABI 口径正确化**:
    - 图内 HAL 73 → **71 fn-ptrs**（对齐 UsrLinuxEmu `tools/docs-audit.sh §1.5` 强制 SSOT；73 错算已说明）
    - §11.1 ADR-023 行 73 → **71 fn-ptrs**
    - **补 §4.3 ABI 三口径脚注**（mirror UsrLinuxEmu 仓 `pcie-endpoint-entry.md §4.3`）：23 = 契约 / 22 = 绑定子集 / 71 = HAL fn-ptrs，避免"23 ABI dlopen"在无限定下字面错误
  - **仓内一致性修复**（7f4d26a4 编辑 roadmap/17-doc 但未统一 ABI 口径，遗留 5 处 "22 ABI"）：
    - roadmap L61/L102/L228/L251 "22 ABI" → "23 ABI（22 = 5.5.6 dlsym 绑定子集）"
    - 17-doc L765 §12.1 "22 ABI" → 同上
  - **changelog 还原**:
    - §12 v0.1 描述项还原为 7f4d26a4 篡改前的真实数字（4 UsrLinuxEmu 文档 / 4.5-6.5 周 / 22 ABI / 7 代码模块）
    - 追加 v0.2 条目如实记录 7f4d26a4 的全部变更（含副作用与错误）
  - **已知遗留**: UsrLinuxEmu 仓 `570b977` commit 同样把 entry.md 图内 71→73，是本次错误的源头；该仓 v0.3+ 修正应先于 CppTLM 后续镜像动作，否则再 sync 又会把 73 拉进来

- **待 v0.4**: 阶段 1.3a 实施后追加（实际 Ring Buffer wire-format 验证 + 性能基准）

- **v0.3.1** (2026-09-09, post-Oracle 闭环登记, Oracle session `ses_f79101668ffeNaDC6a2mvarbOm`): 双仓 73→71 错误链闭环
  - **UsrLinuxEmu 仓 `1145540` (v0.2.2) 已完成** `71 fn-ptrs` 全面回滚 + changelog discipline 修正（§11.1 ADR-023 行、§12 v0.2 历史还原、v0.2.1/v0.2.2 独立条目、header v0.2.2 升级）
  - **本仓 300ddc45 (v0.3)** 已完成图内 71 + §11.1 ADR-023 行 71 + §4.3 ABI 三口径脚注 + 仓内一致性（roadmap/17-doc 5 处 22→23）
  - **撤销**: 本文档 §12 v0.3 "已知遗留" 条目中"UsrLinuxEmu 仓 `570b977` commit 同样把 entry.md 图内 71→73，该仓 v0.3+ 修正应先于 CppTLM 后续镜像动作"——**已过时**, UsrLinuxEmu 仓已闭环完成（`1145540`）。本 v0.3.1 闭环条目按 append-only 纪律保留 v0.3 原文, 仅追加闭环登记。
  - **新发现**: §11.2 引用死链 `cpptlm-emulator-abi-contract-extension/spec.md` —— 该 spec 在本仓 `openspec/specs/` **不存在**; ABI 契约范围已被 `cpptlm-pcie-ep-foundation/spec.md` 13 ADDED Requirements 完整覆盖, 单独 spec 已无存在必要。已替换为 `dgpu-board-adapter-info/spec.md` (per ADR-092 adapter 信息通道)。
  - **跨仓 SSOT**: UsrLinuxEmu `tools/docs-audit.sh §1.5` 权威方法 `grep -oE "\(\*[a-z_]+\)" | grep -vE "callback|handler" | sort -u | wc -l = **71**; ADR-023 §D4 + ADR-092 §D1 累计 68→71 与之一致。

---

**入口文档使用提示**：
- **首次进入 PCIe EP 工作**：从 §1 文档地图开始，依次读 §4 核心概念 → §2 实施路径 → §8 工作场景速查
- **跨仓协调**：以本文档 §5 同步点为准，每子阶段 commit 前检查同步清单
- **问题排查**：先看 §6 决策 → 再看 §9 风险 → 最后看具体文档章节

**SSOT 边界**：
- **本文档** = 入口 + 索引 + 同步点（不存架构/设计 SSOT 内容）
- **pcie-endpoint-architecture.md**（双仓）= 架构 SSOT
- **sdma-engine-design.md** = SDMA 内部设计 SSOT
- **pcie-ep-cpptlm-collaboration-roadmap.md** = roadmap SSOT
- **openspec change** = change 提案 + 13 ADDED Requirements