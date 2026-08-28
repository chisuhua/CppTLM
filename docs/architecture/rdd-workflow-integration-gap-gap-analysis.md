# 架构差距分析: rdd-workflow-integration-gap

> **生成日期**: 2026-08-15
> **状态**: 草案
> **关联 ADR**: (待补充 — 建议新建 ADR-XXXX-rdd-workflow-integration)
> **关联 change**: (待补充)
> **生成工具**: rdd-workflow `guide-arch` Phase 3 `arch_gap_analysis.sh::generate_gap_analysis`

---

## 1. 目标架构

CppTLM 项目集成 rdd-workflow 流程后达到的状态：

- 在 CppTLM 项目根可调用 `/rdd-env-check`、`/rdd-doctor`、`/guide` 等 rdd-workflow 命令
- CppTLM 自身的 OpenSpec workflow（`openspec/changes/<name>/`）与 rdd-workflow 兼容
- CppTLM ADR / Roadmap / Architecture gap-analysis 等工件被 rdd-workflow 自动发现并校验
- 工作树清理、文档同步等纪律门控生效

依据：rdd-workflow 设计文档 `docs/adr/ADR-0003-three-phase-architecture.md`、`ADR-0016-arch-artifact-discovery.md`、`ADR-0017-rddf-session.md`、`ADR-0027-orchestrator-integration.md`。

## 2. 当前架构

**实际状态**（基于 2026-08-15 在 `/workspace/project/CppTLM/` 上的诊断）：

| 项 | 状态 | 证据 |
|---|---|---|
| `openspec/changes/<name>/` | ✅ 存在 | 3 active changes |
| `docs/adr/ADR-*.md` | ✅ 存在 | 21 个 ADR（含 `ADR-0000-template`） |
| `roadmap.md` | ✅ 存在 | 项目根 |
| `docs/architecture/` | ✅ 存在 | 22 项文档，1 个 gap-analysis |
| `_lib/` (rdd-workflow) | ❌ 缺失 | rdd-workflow 项目内部目录 |
| `.rddf/state/` | ⚠️ 部分 | 仅在调用 rdd-env-check / rdd-doctor 时创建 |
| `proposal-suggestions.md` / `proposal-approved.md` | ❌ 缺失 | rdd-workflow design 阶段工件 |
| `.opencode/skills/cpptlm-debug/` | ✅ 存在 | 项目级 skill（与 rdd-workflow 共存） |
| `skills/rdd-workflow/` | ❌ 缺失 | 工作树本地 rdd-workflow 副本 |
| `~/.agents/skills/` | ✅ 全局安装 | rdd-workflow 全局 fallback 可用 |
| AGENTS.md rdd-workflow section | ❌ 缺失 | 根 AGENTS.md 没注册 rdd-workflow skills |
| pre-commit rdd-workflow hook | ❌ 缺失 | `scripts/build/format.sh` 不调用 rdd-doctor |

## 3. 差距清单

| # | 差距项 | 严重程度 | 优先级 | 关联 change |
|---|--------|---------|--------|------------|
| 1 | rdd-workflow `skills/` / `_lib/` 工作树副本未安装 | 高 | P0 | TBD |
| 2 | AGENTS.md 缺 rdd-workflow section（注册 skills + 命令） | 高 | P0 | TBD |
| 3 | pre-commit 未集成 `rdd-doctor` / `docs_sync_check` 双重门控 | 中 | P1 | TBD |
| 4 | `proposal-suggestions.md` / `proposal-approved.md` 未初始化 | 中 | P1 | TBD |
| 5 | `.rddf/state/` 默认 gitignore 未确认 | 低 | P2 | TBD |
| 6 | CppTLM 项目类型检测启发式（arch-env-check 把 C++ 误判 Python）| 低 | P2 | TBD |
| 7 | `guide-arch` 在 CppTLM 上跑 fallback（`~/.agents/skills/`）而工作树本地 | 低 | P2 | TBD |
| 8 | 缺 ADR：是否正式启用 rdd-workflow 流程作为 CppTLM 的二级工作流 | 中 | P1 | TBD |

## 4. 补齐路径

### Phase A: 决策（arch 阶段 P0）

1. 编写 `ADR-XXXX-rdd-workflow-integration.md`，明确：
   - rdd-workflow 与现有 OpenSpec workflow 的关系（并存/替代/桥接）
   - 全局安装（`~/.agents/skills/`）vs 工作树副本（`skills/rdd-workflow/`）的选择
   - rdd-doctor 报告是否接入 CI（`STRICT_ARCH_GATE=yes`）
2. arch-done 门控：ADR ≥ 1 + roadmap.md → 已满足（21 ADR + roadmap.md 存在）
3. 写 `.rddf/state/.arch-handoff.json`（仅当决策通过）

### Phase B: 集成（plan 阶段 P1）

1. 创建 `openspec/changes/rdd-workflow-integration/` 含 proposal.md / design.md / tasks.md
2. tasks.md 包含：
   - 安装 rdd-workflow 工作树副本（`bash /workspace/project/rdd-workflow/install.sh --local`）
   - 初始化 `.rddf/state/` 与 `.rddf/.gitignore`
   - 创建 `proposal-suggestions.md` / `proposal-approved.md` 模板
   - 更新 `AGENTS.md` 添加 rdd-workflow section
   - 集成 pre-commit hook（`.git/hooks/pre-commit` 调用 `rdd-doctor --category state`）

### Phase C: 验证（ship 阶段 P2）

1. 跑 `/rdd-env-check` 在 CppTLM 上 → 期望所有 15 字段非空
2. 跑 `/rdd-doctor --json` → 期望 0 CRITICAL findings
3. 提交一个空 test change → 跑完整 guide → 归档 → 验证 `.rddf/state/.plan-handoff.json` 正确写入

## 5. 参考资料

- 相关 ADR（待创建）：`ADR-XXXX-rdd-workflow-integration.md`
- 相关 change（待创建）：`openspec/changes/rdd-workflow-integration/`
- rdd-workflow 内部参考：
  - `ADR-0003-three-phase-architecture.md` (arch → plan → ship 三阶段)
  - `ADR-0016-arch-artifact-discovery.md` (artifact 自动发现)
  - `ADR-0017-rddf-session.md` (session 生命周期)
  - `ADR-0027-orchestrator-integration.md` (orchestrator 集成)
- CppTLM 内部参考：
  - `AGENTS.md` (项目主索引)
  - `docs/superpowers/specs/PTX-EMU-README.md` (跨项目集成参考)
  - `openspec/changes/<name>/` (现有 OpenSpec workflow 实例)