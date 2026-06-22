# Archived Superpowers Plans & Specs

**Archived on**: 2026-06-22 · **Reason**: 清理 docs/superpowers/ 下过期/已完成/未执行的 plans 和 specs, 减少后续实施计划的干扰噪音

## 归档目录结构

```
docs-archived/superpowers/
├── README.md          ← 本文件
├── plans/             ← 已完成 / 已废弃 / 未执行的实施计划
└── specs/             ← 已完成 / 已被取代的设计文档
```

## 恢复方法

如需查阅历史决策/设计, 直接打开本目录对应文件即可。所有 git 历史完整保留。

如需将某个归档文件复活为 active:
1. `git mv docs-archived/superpowers/{plans,specs}/<filename> docs/superpowers/{plans,specs}/`
2. 更新 `docs/superpowers/handoffs/<date>-<topic>.md` 注明复活原因

---

## plans/ 归档清单 (13 个文件)

| 文件 | 日期 | 类型 | 备注 |
|------|------|------|------|
| `2026-05-02-e2e-test-suite.md` | 2026-05-02 | ✅ 已完成 | Phase 6 E2E test suite (15 cases) |
| `2026-05-06-phase-3-1-defect-fixes.md` | 2026-05-06 | ✅ 已完成 | Phase 3.1 缺陷修复 (DEF-03/04) |
| `2026-05-07-phase-3-3-config-enhancement.md` | 2026-05-07 | ✅ 已完成 | Phase 3.3 ParamRule JSON 序列化 |
| `2026-05-07-phase-3-remaining-tasks.md` | 2026-05-07 | ⚠️ 已被取代 | Phase 3.x 中间版, 被 revised/final 取代 |
| `2026-05-08-phase-3-4-validation-toolchain.md` | 2026-05-08 | 📋 未执行 | Phase 3.4 验证工具链 (Python 端) |
| `2026-05-08-phase-3-x-final-plan.md` | 2026-05-08 | ⚠️ 已被取代 | Phase 3.x v3.4 终版 — 实际未完整执行 |
| `2026-05-08-phase-3-x-revised-plan.md` | 2026-05-08 | ⚠️ 已被取代 | Phase 3.x v2.0 修订版, 被 final 取代 |
| `2026-05-12-unified-visualization-platform.md` | 2026-05-12 | 📋 未执行 | 统一可视化平台 (0 status markers) |
| `2026-06-02-p0-debt-remediation.md` | 2026-06-02 | ✅ 已完成 | P0 debt: PortPair 泄漏 + 死代码 Python |
| `2026-06-02-p3-debt-remediation.md` | 2026-06-02 | ✅ 已完成 | P3 debt: 循环依赖 + ASan CI |
| `2026-06-19-p0-fixes.md` | 2026-06-19 | ✅ 已完成 | P0 全套 (D.1 + CoherentXBarTLM + 死代码) |
| `2026-06-19-simmodule-complex-hierarchies.md` | 2026-06-19 | ✅ 已完成 | SimModule 5 Phase (P1-P5 全 ✅) |
| `2026-06-20-incorporate-parent-late-binding.md` | 2026-06-20 | ✅ 已完成 | ApuSoC::incorporate_parent 真实 late-binding |

---

## specs/ 归档清单 (5 个文件)

| 文件 | 日期 | 类型 | 备注 |
|------|------|------|------|
| `2026-05-29-hierarchical-topology-generator-design.md` | 2026-05-29 | ⚠️ 已被取代 | Draft v4.1, 被 simmodule spec 取代 |
| `2026-06-11-phase7a-gpu-infra-design.md` | 2026-06-11 | ✅ 已完成 | Phase 7.A GPU 基础设施设计 |
| `2026-06-19-p0-fixes-design.md` | 2026-06-19 | ✅ 已完成 | P0 全套设计 (D.1 + CoherentXBarTLM) |
| `2026-06-19-simmodule-complex-hierarchies-design.md` | 2026-06-19 | ✅ 已完成 | SimModule 复杂层次设计 (5 Phase) |
| `2026-06-20-incorporate-parent-late-binding-design.md` | 2026-06-20 | ✅ 已完成 | ApuSoC::incorporate_parent 设计 |

---

## docs/superpowers/ 当前活跃内容 (3 个文件)

仅保留作为后续实施参考:

### plans/ (2 个)
- `2026-06-11-phase7a-gpu-infra.md` — Phase 7.A 实施计划, 作为 Phase 7.B+ 上下文参考
- `2026-06-20-future-work-roadmap.md` — **唯一 active planning doc**, 完整索引 P2+ 待办

### specs/ (1 个)
- `2026-06-12-phase7-apu-fused-soc-design.md` — Phase 7 整体架构设计 (7.A 已落地, 7.B-F 待启动)

---

## 归档原因分类

| 原因 | 数量 | 文件 |
|------|:---:|------|
| ✅ 实施完成 | 12 | 所有 P0/P1/P1.5/P3/P3 debt 计划 |
| ⚠️ 已被取代 (superseded) | 4 | phase-3-remaining-tasks + revised + hierarchical-topology-generator + 已合并到 final plan |
| 📋 未执行 (deferred/superseded by other work) | 2 | phase-3-4-validation-toolchain + unified-visualization-platform |

合计: 18 个文件归档 (13 plans + 5 specs)