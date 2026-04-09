# 架构设计文档

> **版本**: 1.0  
> **最后更新**: 2026-04-10

---

## 架构文档列表

### 核心架构

| 文档 | 状态 | 说明 |
|------|------|------|
| [01-hybrid-architecture-v2.md](./01-hybrid-architecture-v2.md) | ✅ 已批准 | 混合仿真架构 v2.0 |
| [02-transaction-architecture.md](./02-transaction-architecture.md) | ✅ 已批准 | 交易处理架构 |
| [03-error-debug-architecture.md](./03-error-debug-architecture.md) | ✅ 已批准 | 错误与调试架构 |
| [04-reset-checkpoint-architecture.md](./04-reset-checkpoint-architecture.md) | 📋 待确认 | 复位与快照架构 |

### 决策汇总

| 文档 | 说明 |
|------|------|
| [P0_P1_P2_DECISIONS.md](./P0_P1_P2_DECISIONS.md) | P0/P1/P2 核心决策汇总 |
| [FRAGMENT_MAPPER_DECISIONS.md](./FRAGMENT_MAPPER_DECISIONS.md) | FragmentMapper 决议 |

### 示例代码

| 目录 | 内容 |
|------|------|
| [examples/bundles/](./examples/bundles/) | Bundle 定义示例 |
| [examples/tlm/](./examples/tlm/) | TLM 模块示例 |
| [examples/rtl/](./examples/rtl/) | RTL 模块示例 |
| [examples/framework/](./examples/framework/) | 框架示例 |

---

## 推荐阅读顺序

1. 01-hybrid-architecture-v2 - 整体架构
2. P0_P1_P2_DECISIONS - 核心决策
3. 02-transaction-architecture - 交易处理
4. 03-error-debug-architecture - 错误处理

---

**维护**: CppTLM 开发团队
