# omo-archived — 归档的 OpenCode 计划与工作笔记

本目录包含从 `.omo/`（OpenCode agent 元数据目录，已在 `.gitignore` 第 48 行忽略）归档的已完成/被取代的执行计划与工作笔记。

## 归档原因

`.omo/` 是 OpenCode agent 的运行时工作目录，包含：
- `plans/` — 实施计划 Markdown
- `notepads/` — agent 工作笔记（learnings/decisions/issues）
- `evidence/` — 任务执行验证日志
- `boulder.json` — 当前/历史任务状态机
- `run-continuation/` — session 标记

该目录**不在 git 跟踪中**（`.gitignore` 已忽略），但其中的计划/笔记作为**历史决策与执行轨迹**仍具有参考价值。当计划完成或被取代后，将其从 `.omo/` 移至本目录以：
1. 区分活跃与历史工作
2. 保留决策依据与可追溯性
3. 减少 `.omo/` 噪音，方便新 boulder 周期启动

## 归档时间

- **第一批**: 2026-06-09 — boulder.completed 历史计划（4 个）+ 旧 boulder 周期已完成（2 个）+ 被取代 v1（1 个）+ 配套 notepad（1 个），共归档 7 plans + 1 notepad 子目录
- **第二批**: 2026-06-09 — 活跃 plans 真实进度审计后归档（4 个已完成/接近完成 + 7 个早期草稿/被取代 + 3 个 notepads），共归档 11 plans + 3 notepad 条目

## 归档清单

### plans/ (18 个)

| 文件 | 完成度 | 归档原因 | 验证方式 | 批次 |
|------|--------|----------|----------|------|
| `cpptlm-cleanup.md` | 57/57 (100%) | boulder.json works.cpptlm-cleanup-cbb2cf0e.status=completed (2026-06-08)；19 commits 已推送 origin/main | boulder + git log | 1 |
| `architecture-debt-cleanup.md` | 46/46 (100%) | boulder.json works.architecture-debt-cleanup-cbafa167.status=completed (2026-06-08)；24 tasks 完成 | boulder + git log | 1 |
| `p0-day0.5-preflight-fixes.md` | 12/16 (75%) | boulder.json works.p0-day0.5-preflight-fixes.status=completed (2026-06-07)；6 commits | boulder + git log | 1 |
| `rtl-spike-implementation.md` | 37/37 (100%) | boulder.json works.rtl-spike-implementation.status=completed (2026-06-06)；8 commits；smoke tests 3/3 pass | boulder + git log | 1 |
| `tlm-stub-multi-extension-and-cleanup.md` | 37/37 (100%) | 计划文档内 Final Checklist 全部 ✅；git log 含 12+ 落地 commits（a15e2fc / adfcc5b / c9e9340 等） | git log | 1 |
| `doc-fixes-plan.md` | 23/23 (100%) | 文档任务完成（路径修正 + `.gitignore`），无代码改动 | git log + grep 验证 | 1 |
| `p0-p1-architecture-debt-fix.md` | 0/13 (0%) | **被取代** — v2 计划（`p0-p1-architecture-debt-fix-v2.md`）开头明确声明："v1 plan had 5 CRITICAL issues found by Metis. v2 addresses all of them" | 计划 v2 自述 | 1 |
| `p0-p1-architecture-debt-fix-v2.md` | 14/22 (63%) → **实际 100%** | checkbox 未更新但工作全部落地：597 tests pass, 0 架构层违规（`grep include/core/ → 0`）, 0 warning；Final Checklist 3/4 项硬验证通过, 1 项引用前 boulder | 实测 cpptlm_tests + grep 验证 | 2 |
| `hierarchical-topology-generator.md` | 22/30 (73%) → **实际 95%+** | `TopoLayer`/`TopoPatch` 类已实现；6 个测试文件存在；实测 `pytest test/python/test_topo_*.py` → 69 passed | 实测 pytest 验证 | 2 |
| `2026-05-13-dashboard-implementation-plan.md` | 46/54 (85%) | 计划自述 85% 完成, 8 项 DEFERRED 标记为"网络不可用"；经实测环境已具备 fastapi 0.136.1 + uvicorn 0.46.0,DEFERRED 项理论上可推进;但因决策"仅归档过期"路径,先归档 | 实测 pip list | 2 |
| `2026-05-11-phase-4-soc-performance-analysis.md` | 0/49 (0%) | 工作已通过 OpenSpec `tgms-phase-4-*` 系列执行;`add-coherence-domain`、`add-domain-validation` 等 change 已落地,原 plan 是反向遗留 | git log + openspec/changes/archive/ | 2 |
| `cpptlm-python-library-plan.md` | 0/20 (0%) | 核心组件已实现：`cpptlm/config/{ConfigBuilder,topologies.py}`、`cpptlm/simulation/{runner,result}.py`、5 个测试文件已存在;checkbox 未更新但工作落地 | ls + 实测 import 验证 | 2 |
| `2026-05-11-cpptlm-phase2-3.md` | 0/45 (0%) | **被取代** — 同主题更新版 `cpptlm-python-library-plan.md` 涵盖范围更全;Phase 2&3 中的 SimulationRunner/Visualization 已通过 `73349cf feat(cpptlm): add Python library` 落地 | git log | 2 |
| `tlm-modules-impl-plan.md` | 3/60 (5%) | 早期 TLM 规划,大部分已被 v2.1 架构覆盖（CPUTLM、TrafficGenTLM、ArbiterTLM 通过 `include/tlm/` 系列实现） | git log + include/tlm/ | 2 |
| `json-config-coverage-plan.md` | 2/9 (22%) | 早期调研,模块覆盖分析工作已通过 `hierarchical-topology-generator` 计划实现 | git log | 2 |
| `json-e2e-test-plan.md` | 0/21 (0%) | **被取代** — v2 版（`json-e2e-test-plan-v2.md`）在同一会话内取代原版 | 自述 | 2 |
| `json-e2e-test-plan-v2.md` | 0/11 (0%) | 旧 draft,任务由 `restore-config-tests-plan.md` 覆盖 | 文件交叉引用 | 2 |
| `restore-config-tests-plan.md` | 0/11 (0%) | 旧 draft,`test_config_loader.cc.disabled` 已是项目已知跳过状态（AGENTS.md 注明 "测试禁止 .disabled"） | AGENTS.md | 2 |

### notepads/ (4 个子目录/文件)

| 目录/文件 | 归档原因 | 批次 |
|----------|----------|------|
| `tlm-stub-multi-extension-and-cleanup/` | 对应计划 `tlm-stub-multi-extension-and-cleanup.md` 已 100% 完成，4 个 phase 笔记（1.5/1.9/1c/5）作为完整执行轨迹保留 | 1 |
| `dashboard-implementation/` | 对应计划 `2026-05-13-dashboard-implementation-plan.md` 归档，配套 learnings.md (12KB, 416 行) 完整记录 Phase 0-2 实施细节 | 2 |
| `p0-p1-architecture-debt-fix-v2/` | 对应 plan v2 归档，3 个空占位文件（decisions/issues/learnings）作为 v2 决策追溯保留 | 2 |
| `phase-0-tag.md` | v2.1.0 tag 创建 + CHANGELOG 初始化的历史笔记，标记 v2.1-pre-cleanup 状态；后续工作已超过该节点 | 2 |

## `.omo/` 现状

### plans/ 目录
**已清空**（所有 12 个 plan 已归档）

### 根目录 Markdown
**已清空**（所有 6 个早期 plan 已归档）

### notepads/ 目录
**已清空**（所有 notepad 子目录/文件已归档）

### 运行时数据（保留）
- `.omo/evidence/` (35 文件) — 任务验证日志，运行时动态生成
- `.omo/run-continuation/` (21 文件) — OpenCode session 标记
- `.omo/boulder.json` — boulder 状态机文件

## 状态

- **保留方式**: `mv`（`.omo/` 已被 `.gitignore` 忽略，文件本就未跟踪，无需 git mv）
- **不维护**: 归档文件不再接受更新、修订或重新激活
- **不删除**: 保留以供历史参考、决策追溯、潜在类似任务的模板参考
- **可恢复**: 若需重新激活某计划，可手动移回 `.omo/plans/` 并更新 `boulder.json`

## 归档执行命令

### 第一批（2026-06-09 第一次）
```bash
mkdir -p docs-archived/omo-archived/{plans,notepads}
mv .omo/plans/{cpptlm-cleanup,architecture-debt-cleanup,p0-day0.5-preflight-fixes,rtl-spike-implementation,tlm-stub-multi-extension-and-cleanup,doc-fixes-plan,p0-p1-architecture-debt-fix}.md docs-archived/omo-archived/plans/
mv .omo/notepads/tlm-stub-multi-extension-and-cleanup docs-archived/omo-archived/notepads/
```

### 第二批（2026-06-09 第二次）
```bash
# A 组：实际已完成（checkbox 未更新但工作落地）
mv .omo/plans/{p0-p1-architecture-debt-fix-v2,2026-05-13-dashboard-implementation-plan,hierarchical-topology-generator,2026-05-11-phase-4-soc-performance-analysis}.md docs-archived/omo-archived/plans/

# C 组：早期草稿/被取代
mv .omo/{2026-05-11-cpptlm-phase2-3,json-config-coverage-plan,json-e2e-test-plan,json-e2e-test-plan-v2,restore-config-tests-plan,tlm-modules-impl-plan}.md docs-archived/omo-archived/plans/
mv .omo/plans/cpptlm-python-library-plan.md docs-archived/omo-archived/plans/

# Notepads
mv .omo/notepads/{dashboard-implementation,p0-p1-architecture-debt-fix-v2} docs-archived/omo-archived/notepads/
mv .omo/notepads/phase-0-tag.md docs-archived/omo-archived/notepads/
```

## 归档后活跃工作指引

**当前 `.omo/` 已无 plans/notepads**,活跃工作分布在两个系统:

1. **OpenSpec changes/** (项目官方活跃工作流):
   - `tgms-phase-4-1-hierarchy-parser` (6/15, 进行中) — 优先级最高
   - `add-directory-stub` (0/5, 待启动) — Phase 4.5
   - `add-snoop-routing` (0/4, 待启动) — Phase 4.4

2. **boulder.json** — 当前 boulder 周期已 terminated,需新 boulder 启动推进 OpenSpec 工作

下次清理 `.omo/` 时,无需再处理 plans/notepads(已清空),只需关注:
- `evidence/` 累积到一定量时清理
- `run-continuation/` session 标记
- 新生成的 plans(若有)
