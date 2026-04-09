# CppTLM 文档中心

> **版本**: 1.0  
> **最后更新**: 2026-04-10  
> **状态**: ✅ 整理完成

---

## 文档目录

| 目录 | 内容 | 文档数 |
|------|------|--------|
| **[guide/](./guide/)** | 开发指南 | 1 |
| **[skills/](./skills/)** | 技能清单 | 1 |
| **[requirements/](./requirements/)** | 产品需求 | 3 |
| **[architecture/](./architecture/)** | 架构设计 | 8+ |
| **[adr/](./adr/)** | 架构决策记录 | 10+ |
| **[implementation/](./implementation/)** | 实施计划 | 3 |
| **[archived/](./archived/)** | 归档文档 | - |

---

## 快速导航

### 新手入门

1. [开发者指南](./guide/DEVELOPER_GUIDE.md) - 快速开始、核心概念、开发规范
2. [技能清单](./skills/SKILLS_SUMMARY.md) - 核心技能、进阶路径
3. [PRD-002](./requirements/PRD-002-final.md) - 产品需求终版

### 架构设计

1. [混合架构 v2.0](./architecture/01-hybrid-architecture-v2.md) - 核心架构
2. [交易处理架构](./architecture/02-transaction-architecture.md) - 交易追踪
3. [错误处理架构](./architecture/03-error-debug-architecture.md) - 错误与调试

### 决策记录

1. [ADR 索引](./adr/README.md) - 所有 ADR 列表
2. [P0/P1/P2 决策](./architecture/P0_P1_P2_DECISIONS.md) - 核心决策汇总
3. [错误处理决策](./adr/ADR-X.2-error-handling.md) - 错误处理策略

### 实施计划

1. [实施计划 v2.0](../plans/implementation_plan_v2.md) - 详细实施计划
2. [Phase 2 计划](./implementation/phase2-detailed-plan.md) - Phase 2 详细计划

---

## 文档结构

```
docs/
├── README.md                    # 本文件
├── guide/                       # 开发指南
│   └── DEVELOPER_GUIDE.md       # 开发者指南
├── skills/                      # 技能清单
│   └── SKILLS_SUMMARY.md        # 技能总结
├── requirements/                # 产品需求
│   ├── PRD-001-*.md
│   ├── PRD-002-final.md
│   └── PRD-DRAFT-issues.md
├── architecture/                # 架构设计
│   ├── 01-hybrid-architecture-v2.md
│   ├── 02-transaction-architecture.md
│   ├── 03-error-debug-architecture.md
│   ├── 04-reset-checkpoint-architecture.md
│   ├── P0_P1_P2_DECISIONS.md
│   ├── FRAGMENT_MAPPER_DECISIONS.md
│   └── examples/                # 示例代码
├── adr/                         # 架构决策记录
│   ├── README.md
│   ├── ADR-P1-TEMPLATE.md
│   ├── ADR-X.1 ~ ADR-X.8
│   └── ADR-X-SUMMARY.md
├── implementation/              # 实施计划
│   ├── 01-implementation-plan.md
│   ├── 02-implementation-plan-detailed.md
│   └── phase2-detailed-plan.md
└── archived/                    # 归档文档
```

---

## 文档维护

### 文档更新流程

1. 修改文档
2. 更新版本号
3. 更新最后修改日期
4. 提交时包含文档变更说明

### 文档命名规范

| 类型 | 命名格式 | 示例 |
|------|---------|------|
| **需求** | PRD-XXX-*.md | PRD-002-final.md |
| **架构** | XX-topic-name.md | 01-hybrid-architecture-v2.md |
| **ADR** | ADR-X.Y-topic.md | ADR-X.2-error-handling.md |
| **计划** | XX-plan-name.md | 01-implementation-plan.md |

### 版本控制

- 主版本：重大架构变更（v1.0 → v2.0）
- 次版本：内容更新（v1.0 → v1.1）
- 修订版：勘误修正（v1.0.1 → v1.0.2）

---

## 相关资源

| 资源 | 位置 |
|------|------|
| 实施计划 | `plans/implementation_plan_v2.md` |
| 技能文档 | `docs/skills/SKILLS_SUMMARY.md` |
| 开发者指南 | `docs/guide/DEVELOPER_GUIDE.md` |

---

**维护**: CppTLM 开发团队  
**版本**: 1.0  
**最后更新**: 2026-04-10
