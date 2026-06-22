# Archived Plans (项目根 `plans/` 目录归档)

**Archived on**: 2026-06-22 · **Reason**: 清理项目根 `plans/` 目录下的过期/已完成/未执行/一次性报告文件, 减少后续实施计划的干扰噪音

## 归档目录结构

```
docs-archived/plans/
├── README.md          ← 本文件
├── P3_TEST_PLAN.md
├── TEST_EXECUTION_PLAN.md
├── commit-review-26b2d6c4-ae78969f.md
├── doc_alignment_findings.md
├── implementation_plan_v2.1.md
├── issue-chstream-register-namespace.md
├── p0-alignment-remediation-plan.md
├── performance-metrics-refined-plan.md
├── phase7-completion-plan.md
└── tgms-v4-dev-plan.md
```

注: `plans/archive/` 子目录保留 (tgms-dev-plan.md + tgms-handoff.md), 那是历史手工归档区。

## 恢复方法

如需查阅历史决策/设计, 直接打开本目录对应文件即可。所有 git 历史完整保留。

如需将某个归档文件复活为 active:
1. `git mv docs-archived/plans/<filename> plans/`
2. 更新 `docs/superpowers/handoffs/<date>-<topic>.md` 注明复活原因

---

## plans/ 归档清单 (10 个文件)

| 文件 | 日期 | 类型 | 备注 |
|------|------|------|------|
| `P3_TEST_PLAN.md` | 2026-04-10 | ✅ 已完成 | CppTLM v2.0 Phase 3 测试计划 (P3.1+P3.2+P3.3 全部完成) |
| `TEST_EXECUTION_PLAN.md` | 2026-04-10 | ✅ 已完成 | 测试用例编写与修复计划 (690/690 tests pass) |
| `commit-review-26b2d6c4-ae78969f.md` | 2026-06-07 | ⚠️ 一次性报告 | Commit review 报告 (非 plan, 是 Sisyphus 审查产出) |
| `doc_alignment_findings.md` | 2026-04-27 | ⚠️ 一次性报告 | 文档对齐检查发现 (非 plan, 是 audit findings) |
| `implementation_plan_v2.1.md` | 2026-04-13 | ✅ 已完成 | v2.1 分层融合架构实施计划 (2026-04-12 100% 完成) |
| `issue-chstream-register-namespace.md` | 2026-06-07 | ✅ 已完成 | chstream_register.hh namespace 修复 (fix 已 commit 2026-06-10) |
| `p0-alignment-remediation-plan.md` | 2026-06-07 | 🟡 重新审计 | P0 对齐修复 (P0-#1/#2/#3 已落地, P0-#4 移至 F11) |
| `performance-metrics-refined-plan.md` | 2026-04-15 | ✅ 已完成 | 性能指标框架 v2.0 (P8-0 Phase 0 已落地 2026-04-16) |
| `phase7-completion-plan.md` | 2026-05-09 | ✅ 已完成 | Phase 7 NoC/NIC TLM 实施 (95% 完成, 专项测试 → F1) |
| `tgms-v4-dev-plan.md` | 2026-05-27 | ⚠️ 已被取代 | TGMS v4.0 计划 (被 future-work-roadmap F4+F6 取代) |

---

## docs/superpowers/ 当前活跃规划 (1 个文件)

仅保留作为后续实施参考:

- `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` — **唯一 active planning doc**, 完整索引 P2+ 待办 (含新增的 F11: P0-#4 Packet::reset release_extension)

---

## 归档原因分类

| 原因 | 数量 | 文件 |
|------|:---:|------|
| ✅ 实施完成 | 7 | P3_TEST_PLAN + TEST_EXECUTION + implementation_v2.1 + phase7-completion + performance-metrics + issue-chstream + p0-alignment |
| ⚠️ 已被取代 (superseded) | 1 | tgms-v4-dev-plan |
| ⚠️ 一次性报告 (非 plan) | 2 | commit-review + doc_alignment-findings |

合计: 10 个文件归档

---

## 后续行动

- P0-#4 (Packet::reset release_extension) 已移至 `docs/superpowers/plans/2026-06-20-future-work-roadmap.md` F11
- 新风险#1 (test_phase6_integration.cc 直接驱动) 也已纳入 F11
- 后续 contributor 应直接读 future-work-roadmap 获取 active 任务列表, 不必扫本归档