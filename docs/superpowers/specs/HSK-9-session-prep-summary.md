# 会话准备阶段总结 (子波 0, Task 0.1-0.5) + 子波 1 放行决议

> **执行日期**: 2026-09-06
> **Oracle final verdict**: APPROVE (session `ses_f88ec48cdffeqIHpez0aJ7d2ed`)
> **下次启动**: 子波 1 (CppTLM 18a, Task 1.1-1.6) — **放行**, 推荐选项 B (reading-log P0 必读深度阅读 ~2h 后启动)

## 1. 5 task 完成度表

| Task | 判定 | 关键证据 | commit |
|------|------|----------|--------|
| **0.1** 跨仓 worktree + baseline | **PASS** | 双仓 baseline 实测锁定 (44498/1232 OFF; 254/254 PTX-EMU) | `7b96313` (CppTLM) + e7aa69d6/80911163 (PTX-EMU) |
| **0.2** PTX-EMU submodule 状态 | **PASS** | `PTXEMU_API_VERSION=1` 冻结确认; HSK-9 🔵 预留为预期前置 | (无 commit, read-only) |
| **0.3** HSK-9 反馈窗口 + G12 重做 | **PASS** | PR #21 MERGED `d5a58cf5` (squash merge, 2026-09-06T13:47:56Z); 4 commits + 1 P2 修补 | `0a5d276` `a6fc671` `e0c88bf` `a766474` (CppTLM tracker); `80911163` `e7aa69d6` (PTX-EMU) |
| **0.4** 必读文档清单强制落地 | **PASS** | 11 P0/P1/P2 文档结构梳理 (87 行 reading-log + 章节标题 grep) | `75ee2b2` |
| **0.5** 跨仓 build 拓扑验证 | **PASS** | ON 模式 44498/1232 PASS (与 OFF 一致); detached SHA 完美复位; superproject 无污染 | `1e1e96d` |

**整体: PASS, 子波 1 放行.**

## 2. 跨仓 commit 链

### CppTLM 仓 (`feat/sm-mp-impl` 分支, 1e1e96d HEAD)
```
1e1e96d docs(specs): Task 0.5 build 拓扑 + Step 1-3 实测记录
75ee2b2 docs(specs): Task 0.4 reading-log — 11 P0/P1/P2 文档结构梳理
a766474 docs(specs): Task 0.3 P2 修补 — tracker §5 加 '无反馈到期=ack' 注释
e0c88bf docs(specs): Task 0.3.6e tracker §2 fix — PR #21 + 14d window close 2026-09-20
a6fc671 docs(specs): Task 0.3.6 tracker update — fill PR #21 + 14d window
0a5d276 docs(specs): Task 0.3 HSK-9 feedback tracker (PR 窗口跟踪 + 退路重述)
7b96313 docs(plans): Task 0.1 baseline tracker + AGENTS.md 44498 assertions 修订
5cd6fb4 docs(plans): v3.1 修订 SM Task 18 + PTX-EMU HSK-9 (信息级修正 7 处)
```

### PTX-EMU 仓 (`feat/hsk-9-impl` 分支, e7aa69d6 HEAD + main d5a58cf5)
```
feat/hsk-9-impl:
  e7aa69d6 docs(agents): HSK-9 row status 🔵 预留 → 📤 已发布 + 触发条件修正
  80911163 docs(superspecs): mirror HSK-9 spec (ICOMPUTE_API_VERSION=1 SM rewrite)
  73a5ecee chore(openspec): archive phase-1-5-namespace-migration + promote specs

main:
  d5a58cf5 PR #21 squash merge (Task 0.3.7 merge, 含 2 HSK-9 commits)
  + 73a5ecee ...
```

## 3. Oracle 累计 6 次调用摘要

| # | Session | 阶段 | Verdict | 关键修补/建议 |
|---|---------|------|---------|---------------|
| 1 | `ses_f8c579906ffeoKlnK0jEEDwrdS` | v2 复审 | APPROVE-WITH-FIXES | v3.1 信息级 patch (F-1 perl 多行 + F-2 Task 2.2 + F-3 工期统一 + F-4/F-5 commit msg) |
| 2 | `ses_f89271863ffe7kyqKT2ZpvXkTw` | Task 0.1 | APPROVE-WITH-FIXES | PTX-EMU OOM 修复 (-j2); Step 4 cwd/remote 修正 |
| 3 | `ses_f89127beeffeJeSPbzpCrCx6xD` | Task 0.3 详细 | APPROVE-WITH-FIXES | push + PR 路径; G14 重锚 "PR merge = ack" (单 owner) |
| 4 | `ses_f8909c150ffeVZ7aQA0VgEaynB` | Task 0.3 完成 | APPROVE-WITH-FIXES | P2-1 PR body 勾 ctest; P2-2 tracker §5 加 "无反馈到期=ack" |
| 5 | `ses_f8900b940ffejDtHduCBR47v7J` | Task 0.5 预审 | APPROVE-WITH-FIXES | 3 项 P1/P2 修复 (-j1 替换 -j2, 补测试运行, fallback d5a58cf5); Step 2 裁剪 |
| 6 | `ses_f88ec48cdffeqIHpez0aJ7d2ed` | **最终复审** | **APPROVE** | 会话准备 5/5 PASS; 子波 1 放行; 信息级修补 -j1 约束固化 |

## 4. 子波 1 (CppTLM 18a, Task 1.1-1.6) 放行条件核对表

| 条件 | 状态 | 证据 |
|------|------|------|
| worktree `feat/sm-mp-impl` tip + 干净 | ✅ | HEAD=1e1e96d, status 空 |
| HSK-9 spec 已镜像 + PR 已 merge | ✅ | PR #21 MERGED d5a58cf5; 4 tracker commits 闭环 |
| reading-log 已建立 (子波 1 启动前必读) | ✅ | commit 75ee2b2, 11 文档结构梳理 |
| 44498 断言基线双模式 (OFF/ON) 锁定 | ✅ | OFF 44498/1232 + ON 44498/1232 + PTX-EMU 254/254 |
| 计划 Task 1.1-1.6 步骤已复审 | ✅ | TDD 5 步结构 + v2 P0-1 GPUTLM 4-getter 范式 + Step 3 提前声明 |
| PTX-EMU submodule pin 73a5ecee (不可 bump 到 e7aa69d6) | ✅ | 等 Task PTX-6 Step 3 submodule bump |
| 环境约束固化 | ✅ | PTX-EMU -j2 / CppTLM ON -j1 / 15GB no-swap / ccache 命中 |

**全部满足, 子波 1 无条件放行.**

## 5. 风险闭环状态

| 级别 | 风险 | 当前状态 |
|------|------|----------|
| **P0** 误操作主 submodule checkout | ✅ **已闭环** | Task 0.5 Step 3 detached 演练 + `git status --porcelain` 空 + superproject 无 `+` 前缀三重验证 |
| **P1** G14 形式化空转 | ✅ **已闭环** | tracker §3 重锚 "PR merge = ack" (Oracle §5 重述); 14d 降级审计保留 |
| **P1** AGENTS.md HSK-9 行触发条件描述不符 | ✅ **已闭环** | Task 0.3.3 修订 "ICOMPUTE_API_VERSION=1 引入 + SM-owns-state 契约" |
| **P2** HSK 协议单 owner 形式化 | ⚠️ **保留** | tracker 注明即可, 不改协议文档 (符合 Oracle §8 原判定) |

## 6. 后续跟踪项 (per Oracle final §6)

| 项 | 状态 | 处理时机 |
|----|------|----------|
| PR #21 d5a58cf5 在 PTX-EMU main | ✅ 已 merge | Task PTX-6 Step 3 submodule bump 时消费 |
| 14d 窗口 (2026-09-06 → 2026-09-20) | 📤 文档/审计保留 | 单 owner 下 PR merge = ack, 14d 是形式化窗口 |
| AGENTS.md line 370 (44498) 在 feat/sm-mp-impl 分支内 | ⏳ 等 merge 回 main | 子波 1-4 完成或 PR 形式合并时 |
| baseline tracker "CppTLM ON build -j1" 硬约束 (Oracle 信息级修补) | ⏳ 本总结 commit 同步 | 紧随本 commit |

## 7. 环境约束固化 (per Oracle §3 + §8)

| 约束 | 值 | 适用范围 |
|------|----|---------|
| PTX-EMU build `-j` 数 | **2** | 15GB/no-swap + nvcc 重型 .cu (e2e_flashattention/tcgen05 等); OOM 实测验证 (-j4 失败, -j2 通过) |
| CppTLM ON build `-j` 数 | **1** | 15GB/no-swap + ANTLR TU 内存压力 (-j2 失败, -j1 通过); 仅 ON 模式适用 |
| CppTLM OFF build `-j` 数 | `$(nproc)` (=4) | 纯 C++ compile, 无 nvcc, 无 ANTLR TU 重压, `-j4` 安全 |
| ccache | /usr/bin/ccache | CppTLM 命中可显著缩短 rebuild (实测 -j1 build 496s vs 首次 810s) |
| CUDA toolkit | 13.0.88 (/workspace/project/opt/cuda) | ON 模式必需 (含 cuobjdump); nvcc 已用 |
| cmake | 3.28.3 | — |
| g++ | 13.3.0 | — |
| nproc | 4 | CppTLM OFF `-j$(nproc)`; PTX-EMU / CppTLM ON `-j2` 或 `-j1` |

## 8. 子波 1 启动推荐 (per Oracle final §9)

**选项 B (推荐)**: 先做 reading-log P0 必读深度阅读 (~2h), 再启动子波 1.

理由:
- reading-log §"子波 1 启动前必读" 标 P0-3 (SM 顶层 stub) ⏳ 待深度阅读 — 是 Task 1.1 直接修改目标
- P0-1 §15.5-15.6 (IComputeDevice 契约 + 23 ABI 不变量) 是 Task 1.5 `set_instr_descriptor_buf` 的语义源头
- 2h 成本换取 Task 1.3-1.5 真值实现不因契约理解偏差返工 — 划算

**选项 A (备选)**: 立即启动 Task 1.1 (跳过深度阅读), 风险承担 = Task 1.3-1.5 返工概率.

**选项 C**: 其他准备 (e.g. 修订 baseline tracker, 跑 ON 模式全套测试) — 已包含在子波 1 启动流程.

## 9. Oracle Final Verdict (for 会话 1 启动)

```
会话准备阶段 (子波 0, Task 0.1-0.5): PASS
子波 1 (CppTLM 18a, Task 1.1-1.6): 放行
前置条件: 完成 P0 必读深度阅读并在 reading-log 打卡 (选项 B)
信息级修补 (非阻断): baseline tracker 追加 "CppTLM ON build 一律 -j1" 硬约束
Baseline 锁定: 44498 assertions / 1232 cases, OFF=ON 双模式, 子波 1-4 断言地板

— Oracle, 2026-09-06, 第 6 次复审 (final)
```

## 10. 关联文档

- 计划: `docs/superpowers/plans/2027-02-10-sm-task18-impl-and-ptxemu-hsk9.md` (v3.1, 2040 行)
- baseline tracker: `docs/superpowers/specs/HSK-9-baseline-tracker.md` (commit 1e1e96d)
- HSK-9 feedback tracker: `docs/superpowers/specs/HSK-9-feedback-tracker.md` (commit a766474)
- HSK-9 reading-log: `docs/superpowers/specs/HSK-9-reading-log.md` (commit 75ee2b2)
- CppTLM worktree: `/workspace/project/CppTLM/.worktrees/sm-mp-impl` [feat/sm-mp-impl, 1e1e96d]
- PTX-EMU worktree: `/workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl` [feat/hsk-9-impl, e7aa69d6]
- PR #21: https://github.com/chisuhua/PTX-EMU/pull/21 (MERGED d5a58cf5)
