# ADR 索引 — 架构决策记录

> **项目**: CppTLM 多层次混合仿真平台  
> **创建日期**: 2026-04-09  
> **状态**: Active

---

## ADR 列表

| 编号 | 标题 | 状态 | 日期 |
|------|------|------|------|
| **ADR-001** | [采用 TLM Extension 存储 TransactionContext](./ADR-001-transaction-context-storage.md) | ✅ Approved | 2026-04-07 |
| **ADR-002** | [传播规则分类：透传型/转换型/终止型](./ADR-002-propagation-rules.md) | ✅ Approved | 2026-04-07 |
| **ADR-003** | [双并行实现模式：tlm/rtl/compare/shadow](./ADR-003-dual-implementation.md) | ✅ Approved | 2026-04-07 |
| **ADR-004** | [简化版 GVT 时间归一化（周期级）](./ADR-004-time-normalization.md) | ✅ Approved | 2026-04-07 |
| **ADR-005** | [Port\<T\> 泛型模板 + PacketPort 包装](./ADR-005-port-template.md) | ✅ Approved | 2026-04-07 |
| **ADR-006** | [CppHDL 集成：符号链接 → git submodule](./ADR-006-cpphdl-integration.md) | ✅ Approved | 2026-04-07 |
| **ADR-007** | [适配器库分层：Bridge + Mapper](./ADR-007-adapter-layering.md) | ✅ Approved | 2026-04-07 |
| **ADR-008** | [协议适配器范围：首期 AXI4](./ADR-008-protocol-scope.md) | ✅ Approved | 2026-04-07 |
| **ADR-009** | [置信度评分机制：HIGH/MEDIUM/LOW](./ADR-009-confidence-scoring.md) | ✅ Approved | 2026-04-07 |
| **ADR-010** | [回归测试框架：四类回归](./ADR-010-regression-testing.md) | ✅ Approved | 2026-04-07 |
| **ADR-011** | [Phase 分级验收标准](./ADR-011-acceptance-criteria.md) | ✅ Approved | 2026-04-07 |
| **ADR-012** | [5 Phase 实施路线图（20 周）](./ADR-012-implementation-roadmap.md) | ⏳ Draft | TBD |
| **ADR-013** | [风险登记册与缓解措施](./ADR-013-risk-register.md) | ⏳ Draft | TBD |
| **ADR-014** | [文档结构与组织](./ADR-014-document-structure.md) | ⏳ Draft | TBD |

---

## 待创建 ADR

以下 ADR 需要从讨论记录中整理创建：

| 优先级 | ADR | 来源 |
|--------|-----|------|
| **P0** | ADR-001 ~ ADR-004 | `EXTRACTED_P0.*_FULL.md` |
| **P0** | ADR-005 ~ ADR-008 | `EXTRACTED_P1.*_FULL.md` |
| **P1** | ADR-009 ~ ADR-011 | `EXTRACTED_P2.1_FULL.md` |
| **P2** | ADR-012 ~ ADR-014 | `EXTRACTED_P3.1_FULL.md` + 讨论汇总 |

---

## ADR 模板

```markdown
# ADR-XXX: [标题]

> **状态**: Proposed | Approved | Deprecated | Superseded  
> **日期**: YYYY-MM-DD  
> **相关 PRD**: PRD-001  
> **相关文档**: [链接]

---

## 1. 背景与问题

[描述需要做出的决策及其背景]

## 2. 决策

[清晰陈述决策内容]

## 3. 备选方案

| 方案 | 描述 | 优点 | 缺点 |
|------|------|------|------|
| A | ... | ... | ... |
| B | ... | ... | ... |

## 4. 决策理由

[为什么选择这个方案]

## 5. 影响

### 5.1 正面影响
- ...

### 5.2 负面影响（已接受的风险）
- ...

## 6. 验收标准

- [ ] ...
- [ ] ...

## 7. 相关文档

- [链接]

---

**审批**:
- [ ] 架构师
- [ ] 技术负责人
```

---

## 文档处理流程

```
1. 收集原始文档 → docs-pending/05-legacy/
2. 提取讨论记录 → docs-pending/02-architecture/
3. 从 PRD 推导需求 → docs-pending/01-product-requirements/
4. 创建 ADR 文档 → docs-pending/03-adr/
5. 整理架构规范 → docs-pending/04-implementation/
6. 评审 → 批准 → 移动到正式 docs/ 目录
```

---

**最后更新**: 2026-04-09
