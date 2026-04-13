# ADR 索引 (Architecture Decision Records)

> **版本**: 4.0 ✅  
> **最后更新**: 2026-04-09  
> **状态**: ✅ P0/P1/X 已确认，⏳ 剩余待讨论

---

## 📋 ADR 列表

### P0 级决策（已确认）

| ADR 编号 | 议题 | 状态 | 位置 |
|---------|------|------|------|
| **ADR-P0.1** | TransactionContext 设计 | ✅ 已确认 | `../02-architecture/P0_P1_P2_DECISIONS.md` |
| **ADR-P0.2** | 传播规则分类 | ✅ 已确认 | `../02-architecture/P0_P1_P2_DECISIONS.md` |
| **ADR-P0.3** | 双并行实现模式 | ✅ 已确认 | `../02-architecture/P0_P1_P2_DECISIONS.md` |
| **ADR-P0.4** | 时间归一化策略 | ✅ 已确认 | `../02-architecture/P0_P1_P2_DECISIONS.md` |

---

### P1 级决策（已确认）

| ADR 编号 | 议题 | 状态 | 位置 |
|---------|------|------|------|
| **ADR-P1.1** | Bundle 共享策略 | ✅ 已确认 | `ADR-P1-TEMPLATE.md` |
| **ADR-P1.2** | 多端口声明方式 | ✅ 已确认 | `ADR-P1-TEMPLATE.md` |
| **ADR-P1.3** | Adapter 泛型设计 | ✅ 已确认 | `ADR-P1-TEMPLATE.md` |
| **ADR-P1.4** | Fragment/Mapper 层 | ✅ 已确认 | `ADR-P1-TEMPLATE.md` |
| **ADR-P1.5** | 双并行模式策略 | ✅ 已确认 | `ADR-P1-TEMPLATE.md` |

---

### X 级决策（交易与错误处理）

| ADR 编号 | 议题 | 状态 | 位置 |
|---------|------|------|------|
| **ADR-X.1** | 事务追踪 ID 分配 | ✅ 已确认 | `ADR-X.6-transaction-integration.md` |
| **ADR-X.2** | 错误处理策略 | ✅ 已确认 | `ADR-X.2-error-handling.md` |
| **ADR-X.3** | 复位策略 | ⏳ 待讨论 | `ADR-X.3-reset-strategy.md` |
| **ADR-X.4** | 插件系统 | ⏳ 待讨论 | `ADR-X.4-plugin-system.md` |
| **ADR-X.5** | 构建系统 | ⏳ 待讨论 | `ADR-X.5-build-system.md` |
| **ADR-X.6** | TransactionContext 整合 | ✅ 已确认 | `ADR-X.6-transaction-integration.md` |
| **ADR-X.7** | 模块/框架职责划分 | ✅ 已确认 | `ADR-X.7-transaction-handling.md` |
| **ADR-X.8** | 细粒度分片处理 | ✅ 已确认 | `ADR-X.8-fragment-handling.md` |

---

### P2 级决策（待创建）

| ADR 编号 | 议题 | 状态 | 位置 |
|---------|------|------|------|
| **ADR-P2.1** | 置信度评分机制 | ⏳ 待创建 | - |
| **ADR-P2.2** | 回归测试框架 | ⏳ 待创建 | - |
| **ADR-P2.3** | 验收标准 | ⏳ 待创建 | - |

---

### P3 级决策（未来）

| ADR 编号 | 议题 | 状态 | 位置 |
|---------|------|------|------|
| **ADR-P3.1** | 配置文件格式 | ⏳ 未来 | - |
| **ADR-P3.2** | 调试基础设施 | ⏳ 未来 | - |
| **ADR-P3.3** | 性能分析工具 | ⏳ 未来 | - |

---

## 📁 文档结构

```
03-adr/
├── README.md                          # 本索引文件
├── ADR-P1-TEMPLATE.md                 # P1 级议题（已确认）
├── ADR-X-SUMMARY.md                   # X 系列汇总（已确认）
├── ADR-X.2-error-handling.md          # 错误处理（已确认）
├── ADR-X.3-reset-strategy.md          # 复位策略（待讨论）
├── ADR-X.4-plugin-system.md           # 插件系统（待讨论）
├── ADR-X.5-build-system.md            # 构建系统（待讨论）
├── ADR-X.6-transaction-integration.md # 交易整合（已确认）
├── ADR-X.7-transaction-handling.md    # 交易处理（已确认）
├── ADR-X.8-fragment-handling.md       # 分片处理（已确认）
└── archived/                          # 归档的旧 ADR
    └── README.md                      # 旧版索引
```

---

## 🔗 相关文档

| 文档 | 位置 |
|------|------|
| PRD-002 (产品需求) | `../01-product-requirements/PRD-002-final.md` |
| 架构 v2.0 | `../02-architecture/01-hybrid-architecture-v2.md` |
| 交易处理架构 | `../02-architecture/02-transaction-architecture.md` |
| 错误调试架构 | `../02-architecture/03-error-debug-architecture.md` |
| P0/P1/P2 决策汇总 | `../02-architecture/P0_P1_P2_DECISIONS.md` |
| FragmentMapper 决议 | `../02-architecture/FRAGMENT_MAPPER_DECISIONS.md` |
| 示例代码 | `../02-architecture/examples/` |

---

## 📊 ADR 状态统计

| 级别 | 总数 | 已确认 | 待确认 | 待创建 | 未来 | 完成率 |
|------|------|--------|--------|--------|------|--------|
| **P0** | 4 | 4 | 0 | 0 | 0 | 100% |
| **P1** | 5 | 5 | 0 | 0 | 0 | 100% |
| **X** | 8 | 6 | 3 | 0 | 0 | 67% |
| **P2** | 3 | 0 | 0 | 3 | 0 | 0% |
| **P3** | 3 | 0 | 0 | 0 | 3 | 0% |
| **总计** | **23** | **15** | **3** | **3** | **3** | **65%** |

---

## 🎯 待讨论议题（v2.0 影响）

以下议题已创建但需要讨论确认：

| ADR | 议题 | 重要性 | 推荐方案 | 说明 |
|-----|------|--------|---------|------|
| **ADR-X.3** | 复位策略 | 🟡 中 | 独立 reset() 方法 | 模块级/系统级/层次化 |
| **ADR-X.4** | 插件系统 | 🟢 低 | v2.0 无插件 | 编译时链接 vs 运行时加载 |
| **ADR-X.5** | 构建系统 | 🟡 中 | Ninja + CMake | SystemC 支持，测试框架 |

---

## ✅ 核心架构决策汇总

### 交易处理架构

| 决策 | 方案 |
|------|------|
| ID 分配 | 上游分配 + 分层 ID |
| 分片标识 | parent_id + fragment_id |
| 模块职责 | TLM 智能 + RTL 透传 |
| 追踪方式 | TransactionTracker 单例 |
| 粒度配置 | 粗/细可配置 |

### 错误处理架构

| 决策 | 方案 |
|------|------|
| 错误分类 | 分层（传输/资源/一致性/...） |
| Extension | ErrorContext + DebugTrace |
| 追踪器 | DebugTracker 内存索引 |
| 回放支持 | replay_transaction / replay_address |
| 与交易整合 | 共享框架 |

---

**维护**: DevMate  
**版本**: v4.0 ✅  
**最后更新**: 2026-04-09
