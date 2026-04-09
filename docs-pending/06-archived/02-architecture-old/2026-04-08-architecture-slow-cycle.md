# CppTLM 架构慢循环讨论记录

**讨论时间**: 2026-04-08  
**参与者**: 老板 + DevMate  
**状态**: 进行中  
**参考文档**: `多层次混合仿真.md` (v1.0, 2026-04-03)

---

## 1. 项目目标确认

**一句话目标**:  
提供统一的 C++ 建模范式，让用户在同一仿真环境中自由组合事务级（TLM）、行为级和周期精确级（RTL）建模。

**核心原则**: 一次建模，多粒度仿真

**关键约束**:
- C++17 (SystemC 2.3.4 兼容)
- 无 CDC/多频率需求（简化 GVT 设计）
- 复用现有 CppHDL 框架
- 渐进式过渡策略

---

## 2. 架构决策议题 (P0-P3)

### P0 阶段：核心接口设计

| 编号 | 议题 | 决策要点 | 选项 |
|------|------|---------|------|
| **P0.1** | TransactionContext 设计 | TLM Extension 存储方案 | A) Extension 方案（与现有 CoherenceExtension 一致）B) 独立对象方案 |
| **P0.2** | 传播规则分类 | 透传型/转换型/终止型 + SimObject 辅助函数 | A) propagate() 虚函数 B) 模板方法模式 |
| **P0.3** | 双并行实现模式 | tlm/rtl/compare/shadow 四种 impl_type | A) enum 化，确认四种模式 B) 仅 tlm/rtl |
| **P0.4** | 时间归一化策略 | 简化版 GVT | A) 简化版 GVT（无 CDC） B) 完整 CDC 支持 |

### P1 阶段：适配层实现

| 编号 | 议题 | 决策要点 | 选项 |
|------|------|---------|------|
| **P1.0** | Port\<T\> 兼容策略 | backward compatible vs 完全重构 | A) 沿用 simple_port，新增 Port\<T\> B) 完全重构 |
| **P1.1** | CppHDL 集成方案 | wrapper vs 直接集成 | A) HybridComponentWrapper B) 直接集成 |
| **P1.2** | TLMToStreamAdapter 状态机 | valid/ready vs 简化版 | A) 完整状态机 B) 简化版，可配置 |
| **P1.3** | 协议适配器范围 | AXI4/CHI/TileLink 优先级 | A) 先 AXI4 B) 先 TileLink C) 同时 |

### P2 阶段：验证与置信度

| 编号 | 议题 | 决策要点 | 选项 |
|------|------|---------|------|
| **P2.1** | 置信度评分机制 | 高/中/低 三档 | A) 基于参数来源 B) 基于误差范围 |
| **P2.2** | 回归测试框架 | 契约测试 vs 功能测试 | A) 契约测试框架 + 自动化对比 B) 仅功能测试 |
| **P2.3** | 验收标准 | 每个阶段的 DoD | A) 采纳提案 §9.6 B) 调整阈值 |

### P3 阶段：实施计划

| 编号 | 议题 | 决策要点 | 选项 |
|------|------|---------|------|
| **P3.1** | Phase 划分 | 5 个 Phase | A) 维持 5 个 Phase B) 压缩为 3 个 |
| **P3.2** | 时间估计 | 20 周 total | A) 维持 20 周 B) 重新评估 |
| **P3.3** | 风险缓解 | 6 大风险 + 应对 | A) 采纳提案 §10 B) 调整缓解措施 |

---

## 3. 已知已确认决策 (ADR)

| ADR | 内容 | 确认时间 | 状态 |
|-----|------|---------|------|
| ADR-001 | 五维适配层：Port\<T\>、TLMToStreamAdapter、MemoryProxy、TemporalMapper、TransactionTracker | 2026-01-28 | ✅ 已批准 |
| ADR-002 | TransactionContext 生命周期：透传/转换/终止三种模块行为 | 2026-04-03 | ✅ 已批准 |
| ADR-003 | 双并行策略：impl_type = tlm/rtl/compare/shadow + GVT 统一时钟 | 2026-04-03 | ✅ 已批准 |
| ADR-004 | 多总线协议：AXI4/CHI/TileLink + ReorderBuffer | 2026-04-03 | ✅ 已批准 |

---

## 4. 当前项目状态

| 组件 | 状态 | 说明 |
|------|------|------|
| 核心仿真引擎 | ✅ 完成 | SimObject、EventQueue、PortPair、Packet |
| 配置系统 | ✅ 完成 | JSON 驱动模块注册与连接 |
| 测试框架 | ✅ 完成 | 21 个 Catch2 测试用例 |
| 混合建模适配层 | 📋 设计完成 | 五维适配层方案已评审 |
| CppHDL 集成 | ⚠️ 待实施 | external/CppHDL 已链接但未验证 |

---

## 5. 待确认问题

### Q1. P0.1 TransactionContext 存储方案
**问题**: TransactionContext 应该作为 TLM Extension 还是独立对象？

**选项**:
- A) **Extension 方案** — 与现有 CoherenceExtension 一致，通过 TLM 扩展传递
- B) **独立对象方案** — TransactionContext 作为独立对象，随交易对象传递

**DevMate 推荐**: A) Extension 方案（保持一致性）

### Q2. P1.0 Port\<T\> 兼容策略
**问题**: 如何处理现有 simple_port 与新 Port\<T\> 的关系？

**选项**:
- A) **沿用 + 新增** — 保持 simple_port 兼容，新增 Port\<T\> 接口
- B) **完全重构** — 废弃 simple_port，全部迁移到 Port\<T\>

**DevMate 推荐**: A) 沿用 + 新增（backward compatible）

### Q3. P1.3 协议适配器优先级
**问题**: 三个协议适配器先做哪个？

**选项**:
- A) **先 AXI4** — 最广泛使用，生态最完善
- B) **先 TileLink** — 更简单，适合入门
- C) **同时做** — 并行开发，但资源分散

**DevMate 推荐**: A) 先 AXI4（商业价值最大）

---

## 6. 讨论记录

| 时间 | 议题 | 结论 | 确认人 |
|------|------|------|--------|
| 2026-04-08 | 启动架构慢循环 | ACF-Workflow 模式 | 老板 |

---

## 7. 下一步行动

- [ ] 老板回答 Q1-Q3 的选择题
- [ ] DevMate 记录最终决策
- [ ] 更新本文档为最终版
- [ ] 进入 ACF-Workflow 阶段 2（DevMate + OpenCode）

---

**文档状态**: 🔴 待确认
