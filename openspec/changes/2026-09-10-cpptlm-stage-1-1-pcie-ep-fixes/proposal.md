# Proposal: cpptlm-stage-1-1-pcie-ep-fixes — 4 Bug 修复聚焦 Change

> **状态**: 🔄 Proposed v1.0（2026-09-10）
> **优先级**: P0（前置 UsrLinuxEmu 5.5.7 dGPU E2E 主线解锁）
> **工期**: 0.5-1 周（按修复顺序 #3 → #6 → #5 → #7 文档）
> **关联 change**:
> - [2026-09-09-cpptlm-pcie-ep-foundation](../2026-09-09-cpptlm-pcie-ep-foundation/) — 父 change（5 步综合实施）；本 change 是阶段 1.1 聚焦子集
> **关联 ADR**:
> - [ADR-088](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md) ✅ Accepted — dGPU 仿真边界
> - [ADR-091](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-091-pci-driver-architecture-and-four-quadrant.md) ✅ Accepted v0.2 — 4 象限
> **前置基线**:
> - 探索报告完整：`explore session ses_f76db853affeOKwc99O4walT80` 给出 4 bug 详细代码位置
> - Oracle Gate D 4 项 checklist 验证（已 ship `bab64dd5` adapter 扩展）
> **下游**: UsrLinuxEmu `2026-09-10-ue-stage-1-1-bridge-sync` change 解锁；5.5.7+ 启动 gate 解锁

---

## Why

`2026-09-09-cpptlm-pcie-ep-foundation` change 暴露的 7 个根本性错误中，**阶段 1.1 必须修复的 4 个 bug**：

| # | Bug | 实际行为 | 阻塞影响 |
|---|-----|---------|---------|
| **#3** | `pcie_config_read/write` -ENOSYS | 纯 stub | 5.5.6 profile_real 测试断言 `ret != -ENOSYS` 假通过 |
| **#5** | `mmio_read` 数据缺口 | `TODO T-bs-3c` 未填 buf | 5.5.7 dGPU E2E 数据通路空转 |
| **#6** | `backdoor_read` 语义错位 | miss 返 `len` 伪装成功 | 5.5.7 VRAM backdoor 路径不可信 |
| **#7** | `mmio_write` 文档矛盾 | design.md §3.1 说 blocking，architecture §2.1 说 async | 实施 vs 文档歧义 |

**Why 单独 change**：
1. **聚焦**：父 change `2026-09-09-cpptlm-pcie-ep-foundation` 涵盖 5 步（2-3 周），阶段 1.1 独立推进可加速 5.5.7 解锁
2. **TDD 友好**：4 个 bug 各自独立单元测试，可在 worktree 中并行 TDD 5 步（write failing test → implement → verify pass → commit）
3. **风险隔离**：4 修复互不依赖（实施顺序 #3 → #6 → #5 → #7），独立 commit 便于 Oracle 逐步复审

## What Changes

本 change 实施 `src/tlm/gpu/dgpu_board_shell.cc` 4 个修复点（探索报告确认所有 4 bug 实际都在 dgpu_board_shell.cc 而非 cpptlm_emulator.cc）。

### 修复清单

#### 修复 #3: `pcie_config_read/write` 转发
- **当前代码**：`dgpu_board_shell.cc:151-164` 纯 stub，返 `-ENOSYS`
- **实施**：转发到 `ep_->cfg_space_->read(offset, width, *val)` + `ep_->cfg_space_->write(offset, width, val)`
- **依赖**：`soc_->getInternalInstance("pcie_ep")` → `dynamic_cast<PcieEndpointTLM*>` → `cfg_space_->read/write`
- **测试**：`test/test_dgpu_pcie_config.cc::test_pcie_config_read_vendor_id_returns_0x10DE`

#### 修复 #6: `backdoor_read` miss 返 -ENOENT
- **当前代码**：`dgpu_board_shell.cc:168-190` miss 初始化 `rc = len` 后不变
- **实施**：miss 返 `-ENOENT`（-38），hit 返 `0` + `memcpy` buf
- **测试**：`test/test_dgpu_backdoor.cc::test_backdoor_read_miss_returns_enoent_not_len`

#### 修复 #5: `mmio_read` 数据拷贝
- **当前代码**：`dgpu_board_shell.cc:106-133` `TODO T-bs-3c` 未填 buf
- **实施**：`std::memcpy(buf, data.data(), std::min(len, data.size()))` + 返 byte count
- **依赖**：sim_loop drain 真实化（per design.md §2.5 同步等待）
- **测试**：`test/test_dgpu_mmio.cc::test_mmio_read_real_data_not_garbage`

#### 修复 #7: 文档矛盾澄清
- **当前**：design.md §3.1 说 "mmio_write blocks synchronously"，architecture §2.1 说 "MMIO Write 是异步"
- **裁决**：**保持 async**（当前代码正确，per architecture §2.1）
- **实施**：修改 design.md §3.1 与 architecture §2.1 一致，明确 mmio_write 异步语义
- **测试**：N/A（仅文档修订）

### 修改文件清单

- `src/tlm/gpu/dgpu_board_shell.cc` — 4 修复点
- `src/tlm/pcie/pcie_endpoint_ip.cc` — 修复 #5 涉及的 sim_loop drain
- `docs/soc_arch/architecture/18-pcie-endpoint-entry.md` — 修复 #7 文档修订
- `openspec/changes/2026-09-09-cpptlm-pcie-ep-foundation/design.md` — 修复 #7 design.md §3.1 修订
- `test/test_dgpu_pcie_config.cc` (新建)
- `test/test_dgpu_backdoor.cc` (新建)
- `test/test_dgpu_mmio.cc` (新建)

## Impact

### 影响的 specs
- 新建 `cpptlm-stage-1-1-fixes` spec — 4 bug 修复 ADDED Requirements
- 不影响父 change `cpptlm-pcie-ep-foundation` spec（仍为 Proposed，父 change 综合实施跟踪）

### 影响的代码
- **修改**：`dgpu_board_shell.cc` (4 修复点) + `pcie_endpoint_ip.cc` (sim_loop drain)
- **新建**：3 个测试文件
- **不改**：
  - ❌ `include/abi/cpptlm_emulator.h`（23 ABI 冻结，per UE entry §7.3）
  - ❌ `wire-format-snapshot.json`（5 ports 冻结）
  - ❌ `src/abi/cpptlm_emulator.cc`（已 ship adapter 扩展，bab64dd5 保持）

### 影响的下游
- **UsrLinuxEmu `2026-09-10-ue-stage-1-1-bridge-sync` change**：本 change 完成后 UE 侧桥接测试可启动
- **5.5.7 启动 gate 解锁**：5.5.7 dGPU E2E 主线 #2 CommandProcessor 解锁条件 = 阶段 1.1 + 1.2 + 1.3a 全部 ship
- **CppTLM 父 change 进度**：阶段 1.1 标记 ship 后父 change 任务清单可对应勾选

### 风险评估

- **低风险**：4 修复点都在 `dgpu_board_shell.cc` 局部代码，独立 TDD 测试
- **中等风险**：修复 #5 涉及 sim_loop drain 同步等待，需要时序验证
- **需澄清**：修复 #7 文档修订需 Oracle 1 次轻量复审确认裁决方向

### 不在范围内（范围声明 v1.2 修订，2026-09-10 方案 B 拆分）

> **v1.2 修订**：本 change **仅覆盖 §1-§6（4 bug 修复 + 集成测试 + Oracle Gate E）**，**不再承担唯一实施 tracker 角色**。阶段 1.2-2.1 已拆分为独立 change：
> - 阶段 1.2 MSI-X → [`2026-09-10-cpptlm-stage-1-2-msix`](../2026-09-10-cpptlm-stage-1-2-msix/)
> - 阶段 1.3 SDMA 4 子阶段 → [`2026-09-10-cpptlm-stage-1-3-sdma`](../2026-09-10-cpptlm-stage-1-3-sdma/)
> - 阶段 1.4+2.1 电源+P2P → [`2026-09-10-cpptlm-stage-1-4-2-1`](../2026-09-10-cpptlm-stage-1-4-2-1/)
>
> 以下仅为**父 change 残余增强任务**（不重复）：

- LTSSM Link 管理（父 change tasks.md 任务 1.1.5）— 阶段 1.1 链路状态机增强，非 4 bug 修复范围
- MSI-X Capability + 向量表（父 change 任务 1.2.2）+ 中断节流（任务 1.2.3）— 中断增强，非修复 #4 接线范围（修复 #4 在 stage-1-2-msix change 跟踪）

---

## 下一步

1. 启动 worktree：`git worktree add .rddf/wt/2026-09-10-stage-1-1-fixes -b feat/2026-09-10-stage-1-1-fixes`
2. TDD 5 步实施（per commit）：
   - Commit 1: 修复 #3 (pcie_config_read/write) + 测试
   - Commit 2: 修复 #6 (backdoor_read miss) + 测试
   - Commit 3: 修复 #5 (mmio_read 数据) + 测试
   - Commit 4: 修复 #7 文档矛盾修订
3. 每 commit 跑 `./build/bin/cpptlm_tests "[dgpu][pcie]"` 验证
4. 实施完成触发 Oracle Gate E 复审（per ADR-035 §R2 v0.1 → v0.2 升档流程）
5. CppTLM 父 change `2026-09-09-cpptlm-pcie-ep-foundation` tasks.md §2 阶段 1.1 任务对应勾选

## Oracle 探索 session

- `ses_f76db853affeOKwc99O4walT80` — 探索报告，4 bug 代码位置 + 测试基础设施 + 实施顺序建议

## refs

- ADR-088 dGPU 仿真边界
- ADR-091 4 象限 + PcieBypassController 3 态
- 父 change `2026-09-09-cpptlm-pcie-ep-foundation` §2 阶段 1.1
- `docs/soc_arch/architecture/18-pcie-endpoint-entry.md` §4.3 23 ABI 边界
