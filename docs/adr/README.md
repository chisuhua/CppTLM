# ADR - 架构决策记录

> **版本**: 1.2  
> **最后更新**: 2026-05-05

---

## ADR 列表

### P1 级决策

| ADR | 议题 | 状态 |
|-----|------|------|
| [ADR-P1-TEMPLATE.md](./ADR-P1-TEMPLATE.md) | P1 级议题模板 | ✅ 模板 |

### X 级决策（交易、错误处理与 Phase 3+）

| ADR | 议题 | 状态 | 阶段 |
|-----|------|------|------|
| [ADR-X.1-transaction-id.md](./ADR-X.1-transaction-id.md) | 事务追踪 ID 分配 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.2-error-handling.md](./ADR-X.2-error-handling.md) | 错误处理策略 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.3-reset-strategy.md](./ADR-X.3-reset-strategy.md) | 复位策略 | ⏳ 待讨论 | Phase 0-2 |
| [ADR-X.4-plugin-system.md](./ADR-X.4-plugin-system.md) | 插件系统 | ⏳ 待讨论 | Phase 0-2 |
| [ADR-X.5-build-system.md](./ADR-X.5-build-system.md) | 构建系统 | ⏳ 待讨论 | Phase 0-2 |
| [ADR-X.6-transaction-integration.md](./ADR-X.6-transaction-integration.md) | TransactionContext 整合 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.7-transaction-handling.md](./ADR-X.7-transaction-handling.md) | 模块/框架职责划分 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.8-fragment-handling.md](./ADR-X.8-fragment-handling.md) | 细粒度分片处理 | ✅ 已确认 | Phase 0-2 |
| [ADR-X.9-port-type-system.md](./ADR-X.9-port-type-system.md) | Phase 3+ 端口类型系统 | 📋 待实施 | Phase 3.2 |
| [ADR-X.10-parameter-framework.md](./ADR-X.10-parameter-framework.md) | Phase 3+ 参数框架 | 📋 待实施 | Phase 3.3 |
| [ADR-X.11-config-inheritance-and-fixes.md](./ADR-X.11-config-inheritance-and-fixes.md) | 配置继承与缺陷修复策略 | ✅ 已实施 | Phase 3.1 |
| [ADR-X.12-python-config-generator.md](./ADR-X.12-python-config-generator.md) | Python 配置生成器 | 📋 提案 | Phase 3.2 |↵

### 汇总文档

| 文档 | 说明 |
|------|------|
| [ADR-X-SUMMARY.md](./ADR-X-SUMMARY.md) | X 系列决策汇总 |

---

## ADR 状态说明

| 状态 | 说明 |
|------|------|
| ✅ 已确认/已实施 | 已批准或已实施，必须遵循 |
| 📋 待实施 | 已记录，按计划实施 |
| ⏳ 待讨论 | 需要进一步讨论 |

---

**维护**: CppTLM 开发团队
