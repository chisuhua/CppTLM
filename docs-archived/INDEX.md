# docs-archived/ 索引

> 仅添加，不删除 | 总文件数: 126

## 1. 目录速览

| 目录/文件 | 内容 | 废弃原因 |
|-----------|------|----------|
| v2-architecture/ | v2.0 架构文档 (8 文件) | 被 docs/architecture/ v2.1 替代 |
| dead-code-2026-04-14/ | 2026-04-14 死代码清理 | 代码已从 src/include 删除 |
| dead-code-headers-2026-04-14/ | 已删除的头文件 (24 文件) | v2.0→v2.1 迁移 |
| dead-code-headers-2026-q2/ | 已删除的桥接头文件 (2 文件) | packet_to_payload 路径修正 |
| dead-code-sources-2026-04-14/ | 已删除的源文件 (7 文件) | v2.0→v2.1 迁移 |
| disabled-tests/ | 已禁用的测试 (5 文件) | 测试框架 GTest→Catch2 迁移 |
| hybrid-iterations/ | 混合架构设计迭代 (2 文件) | v2.1 架构定稿 |
| implementation-plans/ | 旧版实施计划 (3 文件) | 被 .omo/plans/ 替代 |
| v2-plans/ | v2.0 实施计划 (3 文件) | 被 .omo/plans/ 替代 |
| stream_producer_legacy.hh | v1 流生产者头文件 | 被 cache_bundles_tlm.hh 替代，不应被引用 |
| 01-ARCHITECTURE/ | 架构讨论记录 (16 文件) | 被 docs/architecture/ 替代 |
| 05-LEGACY/ | v1/v2 遗留文档 (10 文件) | 被 docs/architecture/ 替代 |

## 2. 当前推荐文档

| 主题 | 当前路径 |
|------|---------|
| 混合架构 v2.1 | docs/architecture/01-hybrid-architecture-v2.1.md |
| 注册宏体系 | include/AGENTS.md |
| 实施计划 | .omo/plans/architecture-debt-cleanup.md |
| 项目概览 | AGENTS.md (根目录) |

## 3. 迁移历史

| 日期 | 事件 |
|------|------|
| 2026-Q2 | cmd_exts.hh 简化为宏库 (只读迁移) |
| 2026-04-14 | 死代码清理 #1 (头文件/源文件归档) |
| 2026-04-10 | v2.0 冻结 → v2.1 启动 |
| 2025 | v1 → v2 重构 |

## 维护规则

1. **添加**：PR 提交，注明废弃原因
2. **不删除**：永久保留
3. **索引更新**：同步本文件