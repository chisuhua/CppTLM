# docs-archived/ 索引

> 仅添加，不删除 | 总文件数: 183

## 1. 目录速览

| 目录/文件 | 内容 | 废弃原因 |
|-----------|------|----------|
| v2-architecture/ | v2.0 架构文档 (6 文件) | 被 docs/architecture/ v2.1 替代 |
| dead-code-2026-04-14/ | 2026-04-14 死代码清理 | 代码已从 src/include 删除 |
| dead-code-headers-2026-04-14/ | 已删除的头文件 (24 文件) | v2.0→v2.1 迁移 |
| dead-code-headers-2026-q2/ | 已删除的桥接头文件 (**3 文件**) | packet_to_payload 路径修正 + cmd_exts.hh 宏库无消费者 |
| dead-code-sources-2026-04-14/ | 已删除的源文件 (7 文件) | v2.0→v2.1 迁移 |
| dead-configs-2026-q2/ | **2026-Q2 失效 JSON 配置** (13 文件) | example_hier2/hierarchy/layout (legacy 类型 no-op) + root-stale (requirements-visualization.txt, build.sh) |
| disabled-tests/ | 已禁用的测试 (5 文件) | 测试框架 GTest→Catch2 迁移 |
| hybrid-iterations/ | 混合架构设计迭代 (4 文件) | v2.1 架构定稿 |
| implementation-plans/ | 旧版实施计划 (3 文件) | 被 .omo/plans/ 替代 |
| v2-plans/ | v2.0 实施计划 (1 文件) | `phase2-detailed-plan.md` 根级副本已于 2026-06-15 去重 |
| completed-plans-2026-q2/ | **2026-Q2 已完成 plans** (6 文件) | debt-remediation 完成 + 5 个验证 plans + phase7-noc-nic 重复版 |
| stream_producer_legacy.hh | v1 流生产者头文件 | 被 cache_bundles_tlm.hh 替代，不应被引用 |
| 01-ARCHITECTURE/ | 架构讨论记录 (16 文件) | 被 docs/architecture/ 替代 |
| 05-LEGACY/ | v1/v2 遗留文档 (10 文件) | 被 docs/architecture/ 替代 |
| omo-archived/ | 已归档的 OpenCode plans + notepads (25 文件) | 从 .omo/ 迁移，保留历史决策与执行轨迹 (p0-p1-architecture-debt-fix-v2/ 3 个 0 字节占位已清理) |
| `findings_architecture_review.md` (根级) | **2026-04-25 Phase 7 P0 调研报告** | 5 个 P0 问题已在 v2.1.0 修复，零引用 |
| `workflow-state.md` (根级) | **2026-05-29 OpenSpec 工作流状态** | 内容已全部完成归档，零引用 |

## 2. 当前推荐文档

| 主题 | 当前路径 |
|------|---------|
| 混合架构 v2.1 | docs/architecture/01-hybrid-architecture-v2.1.md |
| 注册宏体系 | include/AGENTS.md |
| 实施计划 | .omo/plans/architecture-debt-cleanup.md |
| 项目概览 | AGENTS.md (根目录) |
| 清理 boulder 历史 | docs-archived/omo-archived/INDEX.md |
| 死配置恢复 | docs-archived/dead-configs-2026-q2/README.md |

## 3. 迁移历史

| 日期 | 事件 |
|------|------|
| 2026-06-15 | **全面债务清理 #2**（113→183 文件）— configs/mesh_4x4.json CPUSim→CPUTLM 迁移；3 个 example configs 目录归档；cmd_exts.hh 宏库归档；6 个 plans 归档；2 个根级过期 .md 归档；2 个根级冗余文件归档；4 个 AI 配置 md5 完全相同文件删除；3 个 0 字节 omo notepad 占位删除；8 对归档内 MD5 重复去重 (136,407 字节节省) |
| 2026-06-10 | v2-plans/ 内部去重（2 文件与 implementation-plans/ 字节相同已删） |
| 2026-06-09 | OpenCode plans 归档至 omo-archived/ (第一批 7 plans + 第二批 11 plans + 4 notepads) |
| 2026-06-09 | 根目录 21 项 cleanup（备份/孤儿头/openspec 残留/过期 notes 已清理） |
| 2026-06-08 | v2.1.0 发布（hybrid v2.1 架构 + tlm_stub 多扩展） |
| 2026-06-08 | samples/simple1 + samples/simple_hier 归档至 samples-orphaned/ |
| 2026-Q2 | cmd_exts.hh 简化为宏库 (只读迁移) |
| 2026-04-14 | 死代码清理 #1 (头文件/源文件归档) |
| 2026-04-10 | v2.0 冻结 → v2.1 启动 |
| 2025 | v1 → v2 重构 |

## 维护规则

1. **添加**：PR 提交，注明废弃原因
2. **不删除**：永久保留
3. **索引更新**：同步本文件
4. **去重例外**：归档目录内部允许去除字节级重复（保留一份），须在 INDEX.md 记录
5. **完整性约束**：openspec/archive/ 归档流程需验证 tasks.md 100% 完成（2026-06-15 发现 7/20 不达标）