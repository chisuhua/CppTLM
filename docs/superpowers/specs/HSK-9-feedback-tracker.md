# HSK-9 Feedback Tracker (per plan Task 0.3 + Oracle 复审 §7)

> HSK-9 跨仓协调反馈窗口跟踪 (per `2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`).
> **Anchor strategy**: 单 owner 拓扑 (chisuhua 拥有 CppTLM + PTX-EMU 双仓),
> G14 "14 天无反馈 = 默认 ack" 重锚为 **"PR merge = ack"** (per Oracle §5).
> 14 天窗口仍作文档/审计保留, 但硬阻塞点为 PR merge commit 时间.

## 1. HSK-9 spec 镜像指针

| 端 | 路径 | commit SHA | push 日期 |
|----|------|-----------|-----------|
| CppTLM (源) | `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` | TBD (sm-mp-impl worktree commit 时回填) | 2027-02-09 (Active) |
| PTX-EMU (镜像) | `external/PTX-EMU/docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` | TBD (hsk-9-impl worktree commit 时回填) | 2026-09-06 (本次) |

## 2. 时间锚点 (PR 编号 → 窗口 → ack)

| 事件 | 日期 | SHA/编号 |
|------|------|----------|
| CppTLM 端 HSK-9 spec Active (原) | 2027-02-09 | `c656222` (feat(adr): 正式发布 HSK-9 ICOMPUTE_API_VERSION=1 SM 重构版) |
| CppTLM 端 SM 重构 §15.10 Gate 通过 | 2027-02-09 | `b369aa8` + `ac95fb7` |
| **PR 创建 (PTX-EMU 端 14d 窗口起点)** | 2026-09-06 | #21 |
| **窗口关闭 (PR 创建 + 14d)** | 2026-09-20 | — |
| **PR merge = ack (单 owner 重锚)** | TBD | TBD (PR merge commit SHA) |
| CppTLM 端 submodule bump (Task PTX-6 Step 3) | TBD (PTX-6) | TBD |

## 3. ack 记录

- **单 owner 拓扑下 (chisuhua = CppTLM + PTX-EMU 双仓 owner)**:
  - PTX-EMU owner ack = self-ack (合并 PR 行为)
  - G14 Gate "14 天无反馈 = 默认 ack" 重锚为 "PR merge = ack"
  - 14 天窗口保留作文档/审计, 不作为硬阻塞点
- **多 owner 拓扑假设 (未来)**:
  - 14 天窗口重新生效
  - 退路 A/B/C 触发条件重新启用 (per plan line 95-99)

## 4. 反馈项表 (per plan line 267-270 模板)

| 反馈 ID | 日期 | 来源 | 内容 | 状态 | 修复 commit |
|---------|------|------|------|------|-------------|
| (待 PTX-EMU owner review 时填充) | | | | | |

## 5. 退路触发条件 (per Oracle §5 重述)

| 退路 | 原 plan 触发 (多 owner) | 单 owner 拓扑重述 (用户决策) |
|------|----------------------|---------------------------|
| **退路 A** | PTX-EMU owner 反对 set_instr_descriptor_buf, 坚持 IPtxEmuDevice 12 方法不可动 | **用户决定 SM 重构挂起等 HSK-10** (per Oracle 重新解读) |
| **退路 B** | owner 部分反对, 接受 semantic change 但要求拆批 | **用户决定 HSK-9 拆批提交** |
| **退路 C** | owner 完全反对但 CppTLM 侧不愿等 → 接口改名 ICdnaComputeDevice | **用户决定接口改名绕开 HSK-9, 子波 3 仅镜像头 + PR 说明** |

**判定权**: 单 owner 下退路触发 = 用户主动决策 (非 owner 外部反对).

**无反馈到期 = ack (非退路)**: 14d 窗口内无任何反馈, 默认走 ack 路径 (即 PR merge), 不触发退路. 退路仅在用户**主动决策不走 HSK-9** 时触发. (per Oracle §5 复审 session ses_f8909c150ffeVZ7aQA0VgEaynB §5 遗漏项补充)

## 6. 状态快照时间线

| 日期 | 状态 | 事件 |
|------|------|------|
| 2027-02-09 | ✅ Active (CppTLM 端) | HSK-9 spec 发布, ICOMPUTE_API_VERSION=1, SM-owns-state 契约 |
| 2026-09-06 | 📤 已发布 (PTX-EMU 镜像) | Task 0.3.2 镜像 spec 到 PTX-EMU 仓 |
| 2026-09-06 | 📤 已发布 (PTX-EMU AGENTS.md 更新) | Task 0.3.3 HSK-9 行从 "🔵 预留" → "📤 已发布 + ICOMPUTE_API_VERSION 引入" |
| 2026-09-06 | 📤 PR 创建 (PTX-EMU #21) | Task 0.3.6 push feat/hsk-9-impl + 创建 PR |
| 2026-09-06 → 2026-09-20 | ⏳ 14d 窗口中 | PTX-EMU owner review (单 owner = self-ack at PR merge) |
| TBD | ⏳ 14d 窗口中 | PR 创建日起 14d |
| TBD | ✅ ACCEPTED | PR merge (单 owner 重锚 = ack) |
| TBD | 🔒 SM-owns-state 契约生效 | CppTLM submodule bump (Task PTX-6 Step 3) + 联合验证 (Task 4.8/4.9) |

## 7. 风险与缓解 (per Oracle §8)

| 级别 | 风险 | 缓解 |
|------|------|------|
| P0 (避免) | 误操作主 submodule checkout (detached 73a5ecee) 改 AGENTS.md → 污染主仓 submodule 指针 | Task 0.3 每步开头 `git -C ... rev-parse --abbrev-ref HEAD` 确认在 feat/hsk-9-impl |
| P1 | G14 "14 天无反馈" 形式化空转 (单 owner 下自我等待) | tracker §3 重锚为 "PR merge = ack" |
| P1 | PTX-EMU AGENTS.md HSK-9 行触发条件描述与实际内容不符 | Task 0.3.3 增强: 已明确写 "ICOMPUTE_API_VERSION=1 引入 + SM-owns-state 契约" |
| P2 | HSK 协议在单 owner 拓扑下的形式化问题 | tracker 注明即可, 不改协议文档 |

## 8. 关联文档

- 源 spec: `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` (CppTLM 端, Active)
- 镜像 spec: `external/PTX-EMU/docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` (PTX-EMU 端, Task 0.3.2)
- 计划: `docs/superpowers/plans/2027-02-10-sm-task18-impl-and-ptxemu-hsk9.md` Task 0.3 (line 215-275)
- 决策流程: 同 plan line 90-99
- Oracle 复审 session: `ses_f89127beeffeJeSPbzpCrCx6xD`
