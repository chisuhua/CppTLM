# PCIe EP + CppTLM 协同 Roadmap

> **目的**:打通 CppTLM dGPU PCIe EP 协同流程,为 UsrLinuxEmu 5.5.7+ dGPU E2E 主线 (CommandProcessor / kernel dispatch / DMA) 解锁硬件仿真基础
> **状态**: Draft v0.1 (2026-09-09, 基于 Oracle 三轮审查 + 用户战略调整)
> **维护者**: CppTLM + UsrLinuxEmu 架构组 (跨仓协调)
> **关联**: [pcie-endpoint-architecture.md](../02_architecture/pcie-endpoint-architecture.md) — 跨仓架构 SSOT

---

## §0 战略背景

### §0.0 用户决策（2026-09-09）

> **先打通 CppTLM PCIe EP 协同流程,再考虑 CommandProcessor。在完全实现 PCIe EP 协同的情况下,再启动 5.5.7。**

### §0.1 Oracle 三轮审查揭示的 7 个根本性错误（2026-09-09）

| # | 错误 | 实际行为 | 影响 |
|---|------|---------|------|
| 1 | `cpptlm_emulator_register_backdoor_cb` | `(void)cb` NO-OP | cb 永不入 DGpuBoard |
| 2 | `cpptlm_emulator_register_dma_translate_cb` | 硬编码 `return 0` (pa=0) | pa ≠ identity |
| 3 | `cpptlm_emulator_pcie_config_read/write` | `return -ENOSYS` | 死路 |
| 4 | `cpptlm_emulator_msix_update_pending` | intr_cb 接线但 `trigger_irq_async` 无调用方 | 中断链断裂 |
| 5 | `cpptlm_emulator_mmio_read` | ret=0 但 buf 未填充（T-bs-3c）| 数据缺口 |
| 6 | `cpptlm_emulator_backdoor_read` | 未命中 vram_segments_ 返 `len`（如 4）| 伪装成功 |
| 7 | `cpptlm_emulator_mmio_write` | 立即返 0，数据丢弃 | 写入假成功 |

**根本原因**：5.5.6 ship 时 5 个新测试仅断言 `ret != -ENOSYS`,对数据正确性零覆盖;UsrLinuxEmu 端"接线真实但语义空转"。

---

## §1 4 层 PCIe 能力框架 + 5 步实施路线图

### §1.1 4 层 PCIe 能力框架

| 层级 | 状态 | 包含 |
|------|------|------|
| **基础必备**（阶段一必须）| 🎯 **本 roadmap 实施** | PCIe Endpoint 基础 + MSI-X 中断 + DMA 引擎 + 电源管理 |
| **性能增强**（阶段一后期或阶段二）| 🎯 阶段 2.1 | P2P + Resizable BAR + 原子操作 + 带宽优化 |
| **虚拟化必备**（阶段二/三）| ❌ 排除 | SR-IOV + VF 配置空间 + VF 中断隔离 |
| **高级可选**（后续阶段）| ❌ 排除 | CXL / NTB / TPH / ATS / PRI / PASID |

### §1.2 5 步实施路线图（阶段 1 基础必备 + 阶段 2 性能增强）

| 步骤 | 标题 | 工期 | 对应 openspec change proposal | 状态 |
|------|------|:---:|------------------------------|:----:|
| **阶段 1.1** | PCIe EP 基础 | 0.5-1 周 | [`2026-09-09-cpptlm-pcie-ep-foundation` §阶段 1.1](../../openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/tasks.md) | 🔄 Proposed |
| **阶段 1.2** | MSI-X 中断 | 0.5 周 | 同上 §阶段 1.2 | 🔄 Proposed |
| **阶段 1.3a** | PCIe SDMA 基础: Ring Buffer + RPTR/WPTR + Doorbell + SG | 1 周 | 同上 §阶段 1.3 + [`docs/soc_arch/architecture/17-sdma-engine-design.md` §2-§6](../../02_architecture/sdma-engine-design.md) | 🔄 Proposed |
| **阶段 1.3b** | D2D SDMA 路径: NoC 数据面 + 显存控制器 bypass | 0.5-1 周 | 同上 §阶段 1.3 + [`sdma-engine-design.md` §10](../../02_architecture/sdma-engine-design.md) | 🔄 Proposed |
| **阶段 1.3c** | dma_translate_cb 真实化 + GART/IOMMU + CP→SDMA DMA 转发 | 0.5 周 | 同上 §阶段 1.3 + [`sdma-engine-design.md` §8+§11](../../02_architecture/sdma-engine-design.md) | 🔄 Proposed |
| **阶段 1.3d** | SDMA 完成通知: Fence + MSI-X 接线 | 0.5 周 | 同上 §阶段 1.3 + [`sdma-engine-design.md` §9](../../02_architecture/sdma-engine-design.md) | 🔄 Proposed |
| **阶段 1.4** | 电源管理 | 0.5 周 | 同上 §阶段 1.4 | 🔄 Proposed |
| **阶段 2.1** | P2P + Resizable BAR | 1 周 | 同上 §阶段 2.1 | 🔄 Proposed |
| **总计** | | **5.0-6.0 周** | | |

**v0.1 → v0.2 修订**（Oracle 2026-09-09 审查触发）：
- 阶段 1.3 从单一步骤拆为 4 子阶段（1.3a/1.3b/1.3c/1.3d），原 0.5 周 → 2.5-3 周
- 总工期 2.5-3.5 周 → **4.5-6.5 周**（v0.3 还原：原 7f4d26a4 commit 把此数字改写为 5.0-6.0 周，与 v0.2 实际值不符；正确 v0.2 数字应为 4.5-6.5 周——阶段求和验证为 5.0-6.0 周，故实际从 v0.3 起为 5.0-6.0 周，溯源见 v0.3 条目）
- 新增 SDMA 内部设计文档 [`docs/soc_arch/architecture/17-sdma-engine-design.md`](../../02_architecture/sdma-engine-design.md)（11 章节，作为 1.3 实施的设计基础）
- **不变项**：阶段 1.1/1.2/1.4/2.1 工期不变；22 ABI 签名不变；5 端口 wire-format 冻结（v0.3 还原：原 7f4d26a4 commit 把此 22 改写为 23，但**未在 roadmap 内同步统一所有 22 引用**，且未补 §4.3 ABI 三口径脚注）

### §1.3 双层 DMA 架构（PCIe SDMA 硬件 + CommandBuffer 软件）

```
┌─────────────────────────────────────────────────────┐
│                    驱动层（软件）                      │
│                                                     │
│  ┌───────────────┐     ┌───────────────────────┐    │
│  │ CommandBuffer  │     │ SDMA Ring Buffer      │    │
│  │ (PM4/AQL)     │     │ (DMA 描述符)           │    │
│  └───────┬───────┘     └───────────┬───────────┘    │
└──────────┼─────────────────────────┼────────────────┘
           ▼ Doorbell                ▼ Doorbell
┌──────────────────┐     ┌───────────────────────┐
│ Command Processor │     │ SDMA 引擎（硬件）      │
│ (CmdProc)        │     │ (PCIe SDMA)            │
│ - 解析 PM4/AQL   │     │ - Ring + RPTR/WPTR     │
│ - 分发到各引擎    │     │ - 直接 PCIe TLP        │
└────────┬─────────┘     └───────────┬───────────┘
         │                           │ DMA translate cb
         ▼                           ▼
┌─────────────────────────────────────────────────────┐
│          GPU 内部 + PCIe EP + 显存控制器              │
└─────────────────────────────────────────────────────┘
```

**关系**：CommandBuffer 是"指挥官"(编排任务),SDMA 引擎是"搬运工"(执行 PCIe DMA 传输);CmdProc 解析 CommandBuffer 中 DMA 指令包后**调度 SDMA 引擎**执行。

**阶段一开发建议**：SDMA 引擎先行（数据通路基础设施）→ CommandBuffer 同步实现（计算任务入口）。

---

## §2 跨仓协调路径

### §2.1 仓库边界

```
┌─────────────────────────┐                ┌─────────────────────────┐
│  UsrLinuxEmu (driver)   │  ←─dlopen─→  │   CppTLM (hardware)    │
│                         │                │                         │
│  • GpgpuDevice ioctl   │  openspec/changes│  • 23 ABI functions  │
│  • HAL struct          │  2026-09-09-...   │  • DGpuBoard SoC       │
│  • bridge.cpp kCpptlm  │                │  • PcieEndpointIP      │
│  • backdoor_endpoint   │                │  • SDMA / CmdProc      │
│  • plugin + hal_user   │                │  • MSIX / PcieCfgSpc   │
└─────────────────────────┘                └─────────────────────────┘
              ↕                                            ↕
    openspec/changes/                            openspec/changes/
    2026-09-09-5-5-7-cpptlm-cp-real-ification/      2026-09-09-cpptlm-pcie-ep-foundation/
    2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma/
```

### §2.2 实施同步点

| 时机 | UsrLinuxEmu 端 | CppTLM 端 |
|------|----------------|----------|
| 阶段 1.1 完成后 | 回归测试 (`test_bridge_kcpptlm_profile_real`) | API 行为稳定 |
| 阶段 1.2 完成后 | MSI-X 中断测试 (`test_msix_*)` | 中断投递链验证 |
| 阶段 1.3 完成后 | DMA translate 测试 | `dma_translate_cb` 真实调用 |
| 阶段 1.4 完成后 | 电源状态测试 | PM state machine 验证 |
| 阶段 2.1 完成后 | P2P / Resizable BAR 测试 | P2P routing 验证 |
| **全 5 步完成后** | **5.5.7 重启 + 测试断言升级** | **openspec change archive** |

---

## §3 步骤详细映射（每步骤 → openspec change + 代码修改）

### §3.1 阶段 1.1: PCIe EP 基础

**对应 openspec change**: [`2026-09-09-cpptlm-pcie-ep-foundation` §阶段 1.1](../../openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/tasks.md)

**修复 4 个错误**：
- #3 pcie_config_read/write → 转发 `ep_->cfg_space_->read/write`
- #5 mmio_read 数据缺口 → TLP 注入 TODO T-bs-3c + race 修复
- #6 backdoor_read 返 len → 未命中返 -ENOENT
- #7 mmio_write 数据丢弃 → 同步阻塞至 sim_loop drain

**关键代码文件**（CppTLM 端）：
- `src/tlm/gpu/dgpu_board_shell.cc` (line 106-220) — 4 个函数真实化
- `src/tlm/pcie/pcie_endpoint_ip.cc` — config space 接线
- `src/tlm/pcie/pcie_link_layer_tlm.cc` — LTSSM 补全

**UsrLinuxEmu 端同步**：
- 测试断言升级 `CHECK(ret != -ENOSYS)` → `REQUIRE(ret == 0) + buf 内容断言`
- `tests/sim_hardware/test_bridge_kcpptlm_profile_real_standalone.cpp` 加 4 数据通路 roundtrip 断言

### §3.2 阶段 1.2: MSI-X 中断

**对应 openspec change**: §阶段 1.2

**修复 1 个错误**：
- #4 msix_update_pending 中断链 → `pcie_ep.irq_out` → `board->trigger_irq_async` 接线

**关键代码文件**：
- `src/abi/cpptlm_emulator.cc` — `register_callbacks` 5 cb 全部真实接线
- `src/tlm/pcie/pcie_endpoint_ip.cc` — IRQ delegate 接线
- `src/tlm/pcie/pcie_msix_per_vf_tlm.cc` — 4-8 向量表

### §3.3 阶段 1.3: DMA 引擎

**对应 openspec change**: §阶段 1.3

**修复 1 个错误**：
- #2 dma_translate_cb 硬编码 pa=0 → 真实调用 UsrLinuxEmu cb（identity mapping）

**关键代码文件**：
- `src/abi/cpptlm_emulator.cc` (line 443-460) — lambda 内移除 `(void)cb`
- `src/tlm/gpu/sdma_engine_tlm.cc` — Scatter-Gather 描述符
- `src/tlm/pcie/pcie_endpoint_ip.cc` — IOMMU 兼容接口

### §3.4 阶段 1.4: 电源管理

**对应 openspec change**: §阶段 1.4

**关键代码文件**：
- `src/tlm/pcie/pcie_config_space_per_vf_tlm.cc` — PM Capability 寄存器
- `src/tlm/pcie/pcie_endpoint_ip.cc` — D0/D3 状态机
- `src/tlm/gpu/dgpu_soc.cc` — 电源切换逻辑

### §3.5 阶段 2.1: P2P + Resizable BAR

**对应 openspec change**: §阶段 2.1

**关键代码文件**：
- `src/tlm/pcie/pcie_ari_router_tlm.cc` — ARI 路由
- `src/tlm/pcie/pcie_bypass_mux.cc` — P2P 路由
- `src/tlm/pcie/pcie_config_space_per_vf_tlm.cc` — Resizable BAR Capability

---

## §4 UsrLinuxEmu 端后续行动（5 步完成后）

| 序号 | 行动 | 依赖 | 对应 openspec change |
|------|------|------|---------------------|
| 1 | 测试断言升级（5.5.7.1 P5.NEW-A + 5.5.8 P5.NEW-X.1）| 阶段 1.1-1.3 完成 | 新增独立 follow-up change |
| 2 | 删除 5.5.8 阶段 1 cp_attach（Oracle 验证根因错位）| 阶段 1.2 完成 | 修改 `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma` |
| 3 | **5.5.7 dGPU E2E 主线 #2 重启**（CommandProcessor）| 5 步全完成 | `2026-09-09-5-5-7-cpptlm-cp-real-ification` (现 Accepted 等启动) |
| 4 | **5.5.8 dGPU E2E 主线 #3 重启**（kernel dispatch + DMA）| 5.5.7 重启后 | `2026-09-09-5-5-8-cpptlm-kernel-dispatch-dma` |
| 5 | **5.5.9 真机双轨验证** | 5.5.8 重启后 | 待立项 |

---

## §5 验证清单

### §5.1 阶段完成验证

- [ ] 阶段 1.1 完成：4 数据通路 roundtrip + config space 真实化
- [ ] 阶段 1.2 完成：msix_update_pending 触发 intr_cb（200ms 内 ≥1 次）
- [ ] 阶段 1.3 完成：dma_translate_cb 真实调用（pa == iova）
- [ ] 阶段 1.4 完成：D0 ↔ D3 状态切换 + ASPM L0s/L1
- [ ] 阶段 2.1 完成：P2P DMA + Resizable BAR
- [ ] 全 5 步 Oracle 审查 ≥ 9.0/10
- [ ] UsrLinuxEmu 5.5.7 P5.NEW-A profile 测试稳定（mmio ret==0 + buf 真实数据）

### §5.2 跨仓集成验证

- [ ] CppTLM `tests/abi/test_cpptlm_emulator_abi.cc` 全 PASS
- [ ] UsrLinuxEmu `test_bridge_kcpptlm_profile_real_standalone` 5/5 PASS + data assertion
- [ ] `ctest` 双向全绿
- [ ] docs-audit 双仓 PASS
- [ ] D.1/D2/D3 决策不变（X/P/D）

### §5.3 架构约束保持

- [ ] UsrLinuxEmu `drv/` 零修改（5 步实施期间 + 完成后）
- [ ] UsrLinuxEmu HAL append-only（ADR-023 §D4 不变）
- [ ] 23 ABI 函数签名不变（22 = 5.5.6 dlsym 绑定子集；仅行为从 stub 变为真实）
- [ ] CppTLM `src/abi/cpptlm_emulator.h` 不变

---

## §6 风险与回退

| 风险 | 等级 | 缓解 |
|------|:----:|------|
| 步骤 1.1 mmio race 修复影响 5.5.7.1 profile 测试 | 中 | timeout 延长到 10ms + sim_loop tick 频率提升 |
| 步骤 1.3 dma_translate_cb 真实调用触发 UsrLinuxEmu cb 错误处理 | 中 | cb 失败返 pa=iova（identity）fallback |
| 步骤 1.2 中断链修复后 msix 测试不稳定 | 中 | 测试用 100ms 超时 + retry 1 次 |
| 步骤 1.4 电源管理状态切换影响 profile 加载 | 低 | profile 加载时强制 D0 |
| 跨仓协调延迟 | 低 | CppTLM 与 UsrLinuxEmu 同步 commit |

---

## §7 修订记录

- **v0.1** (2026-09-09, Draft): 初版,基于 Oracle 三轮审查 + 用户战略调整 + CppTLM 5 步实施框架

- **v0.2** (2026-09-09, 7f4d26a4 commit): 跨仓镜像 UsrLinuxEmu 端 entry.md v0.2 同步
  - 总工期数字 v0.2 实际值为 **4.5-6.5 周**（本表 v0.2 原写），但与阶段 1.1+1.2+1.3(a-d)+1.4+2.1 求和（min=5.0, max=6.0）不一致——正确值应为 5.0-6.0 周
  - §4.3 跨仓边界图与 18-doc 同步：22→23 ABI、71→73 fn-ptrs（**73 错**，未补 ABI 三口径脚注）
  - §1.2 标题 "4 核心文档 → 5 条目"
  - §2.2 总计 + §2.3 关键路径图 + §8.5 PM 行 总工期改为 5.0-6.0 周（**未声明副作用**：§2.3/§8.5 未列在 commit message）
  - §3.2 5.5.6 行 "bridge dlopen 22 ABI → 23 ABI"（**未声明副作用** + 概念混淆：22 = dlsym 绑定子集 ≠ 23 契约）
  - §6 D.4 决策表 "22 ABI 兼容 → 23 ABI 兼容"（**未声明副作用**）
  - §7.3 验证清单 "22 ABI 函数签名不变 → 23 ABI"（**未声明副作用**）
  - §11.1 ADR-023 行 "HAL 71 fn-ptrs → HAL 73 fn-ptrs"（**未声明副作用** + 73 错）
  - §11.3 bridge.cpp 行 "22 ABI dlopen → 23 ABI dlopen"（**未声明副作用** + 22 = 绑定数，不是 23）
  - §12 v0.1 描述项被改写为新数字（**违反 append-only 纪律**）
  - **17-doc ADDED 14 → 13 同步**（spec.md 实测）

- **v0.3** (2026-09-09, post-Oracle 复审): 修正 7f4d26a4 引入的 4 类错误
  - **工期数字修正**:
    - 总表 v0.2 历史项中的"5.0-6.0 周"改回"4.5-6.5 周"（保留 v0.2 实际历史）；同时 v0.3 当前真实值改为 **5.0-6.0 周**（8 阶段求和正确值，min=0.5+0.5+1+0.5+0.5+0.5+0.5+1, max=1+0.5+1+1+0.5+0.5+0.5+1）
  - **ABI 口径正确化**:
    - 4 处 "22 ABI" → "23 ABI 契约签名（22 = 5.5.6 dlsym 绑定子集）"（L61/L102/L228/L251）
    - 与 18-doc §4.3 ABI 三口径脚注同步
  - **17-doc §12.1 同步**: "22 ABI" → "23 ABI（22 = 绑定子集）"，并补 18-doc 脚注 cross-ref
  - **changelog 还原**: 本表 v0.2 历史项 "总工期 → 5.0-6.0 周"改回"4.5-6.5 周"（v0.2 真实值）；"22 ABI 签名"还原为"22 ABI 签名"（v0.2 实际，未统一为 23）
  - **已知遗留**: UsrLinuxEmu 仓 570b977 commit 同样把 entry.md 图内 71→73；该仓 v0.3+ 修正应先于 CppTLM 后续镜像动作，否则再 sync 又会把 73 拉进来

- **待 v0.4**: 阶段 1.3a 实施后追加（实际 wire-format 验证 + 性能基准）

- **v0.3.1** (2026-09-09, post-Oracle 闭环登记, Oracle session `ses_f79101668ffeNaDC6a2mvarbOm`): 双仓 73→71 错误链闭环
  - **UsrLinuxEmu 仓 `1145540` (v0.2.2) 已完成** 71 fn-ptrs 全面回滚 + changelog discipline 修正
  - **本仓 300ddc45 (v0.3)** 已完成 5 处 22→23 + 1 处 71→73→71 自愈
  - **撤销**: 本表 v0.3 "已知遗留" 中"UsrLinuxEmu 仓 570b977 commit 同样把 entry.md 图内 71→73，该仓 v0.3+ 修正应先于 CppTLM 后续镜像动作"——**已过时**, UE 已闭环
  - **跨仓 SSOT**: UsrLinuxEmu `tools/docs-audit.sh §1.5` = **71** fn-ptrs