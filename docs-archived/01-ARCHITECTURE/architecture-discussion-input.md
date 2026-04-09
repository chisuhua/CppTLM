# 架构讨论输入文档

**日期**: 2026-04-07  
**模式**: 慢循环 - 架构讨论  
**参与者**: DevMate (技术合伙人) + 老板 (CTO)

---

## 1. 讨论目标

本次架构讨论的目标是：
1. 确认混合建模架构设计的关键决策点
2. 确认 CppHDL 集成方案
3. 确认 Phase 1 实施优先级
4. 识别架构风险并制定缓解措施

---

## 2. 架构决策点（需讨论确认）

### 决策点 1: TransactionContext 设计

**方案 A: 嵌入 Packet 内部**（推荐）
```cpp
class Packet {
    TransactionContext context;  // 值语义，直接嵌入
};
```
- ✅ 内存连续，缓存友好
- ✅ 无需额外生命周期管理
- ⚠️ Packet 体积增大（约 200 字节）

**方案 B: 指针引用**
```cpp
class Packet {
    TransactionContext* context;  // 指针，需管理生命周期
};
```
- ✅ Packet 体积小
- ⚠️ 需额外生命周期管理
- ⚠️ 间接访问，缓存不友好

**推荐**: 方案 A

**需确认**:
- [ ] 是否同意方案 A？
- [ ] 是否需要支持可选 Context（编译时开关）？

---

### 决策点 2: Port<T> 与现有 SimplePort 的兼容

**方案 A: 完全替换**
- 新代码使用 `Port<T>`
- 现有 `SimplePort` 标记为 deprecated
- 提供迁移指南

**方案 B: 并行共存**（推荐）
- `PacketPort<T>` 包装 `SimplePort`
- 现有代码零修改
- 新代码使用 `Port<T>`

**推荐**: 方案 B

**需确认**:
- [ ] 是否同意并行共存策略？
- [ ] SimplePort 何时标记 deprecated？

---

### 决策点 3: TLMToStreamAdapter 状态机

**状态定义**:
```
IDLE ──trySend()──> TRANSMITTING ──last_beat──> IDLE
   ↑                    │
   └────ready=0────────┘ (BACKPRESSURED)
```

**关键设计**:
- FIFO 深度：可配置（默认 8）
- valid/ready 时序：符合 AXI4 规范
- 背压传播：RTL ready=0 → TLM trySend()=false

**需确认**:
- [ ] 状态机设计是否合理？
- [ ] FIFO 深度默认值是否合适？
- [ ] 是否需要支持乱序完成？

---

### 决策点 4: 双并行实现模式

**impl_type 配置**:
| 模式 | 行为 | 用途 |
|------|------|------|
| **tlm** | 仅 TLM 实现 | 快速仿真 |
| **rtl** | 仅 RTL 实现 | 周期精确 |
| **compare** | 并行运行两种 | 功能验证 |
| **shadow** | RTL 影子模式 | 行为记录 |

**需确认**:
- [ ] 四种模式是否满足需求？
- [ ] compare 模式的输出对比策略？
- [ ] shadow 模式是否必要？

---

### 决策点 5: CppHDL 集成方案

**方案对比**:
| 方案 | 优点 | 缺点 | 推荐阶段 |
|------|------|------|---------|
| **符号链接** | 简单、即时生效 | 无版本控制 | 开发阶段 |
| **Git Submodule** | 版本锁定、CI/CD 友好 | 需额外 git 命令 | 生产阶段 |
| **ExternalProject** | 完全自动化 | 构建时间增加 | 库依赖管理 |

**推荐**: 开发阶段用符号链接，生产阶段迁移到 submodule

**需确认**:
- [ ] 是否同意推荐方案？
- [ ] CppHDL 是否有稳定 API/版本号？
- [ ] 是否需要记录 CppHDL commit hash？

---

## 3. 架构风险识别

### 风险 1: TransactionContext 性能开销

**风险描述**: Context 嵌入 Packet 可能增加内存占用，影响仿真速度

**影响评估**:
- 内存增加：约 200 字节/Packet
- 仿真速度影响：预计 < 5%（待基准测试验证）

**缓解措施**:
- 编译时开关 `ENABLE_TRANSACTION_CONTEXT`
- 基准测试验证性能影响
- 优化 Context 内存布局（对齐、压缩）

**需讨论**: 是否接受潜在的性能开销？

---

### 风险 2: CppHDL 接口变更

**风险描述**: CppHDL 项目可能变更 API，导致适配器需重新设计

**影响评估**:
- 适配器代码需修改
- 测试用例需更新

**缓解措施**:
- 早期锁定接口契约（编写接口测试）
- 记录 CppHDL commit hash
- 设计适配器抽象层，隔离变化

**需讨论**: 如何与 CppHDL 团队协调接口稳定性？

---

### 风险 3: 命名空间冲突

**风险描述**: GemSc 与 CppHDL 可能有命名冲突（如 Packet、Port、Module）

**影响评估**:
- 编译错误
- 代码可读性下降

**缓解措施**:
- 明确命名空间封装（`gemsc::` vs `ch::`）
- 适配器代码使用独立命名空间（`gemsc::adapter::`）
- 代码审查检查命名冲突

**需讨论**: 命名空间约定是否需要文档化？

---

### 风险 4: 时间归一化复杂度

**风险描述**: TLM 事件（毫秒级）与 RTL 周期（纳秒级）的时间映射可能复杂

**影响评估**:
- 实现复杂度高
- 调试困难

**缓解措施**:
- 简化方案：TLM 事件在 RTL 周期边界触发
- 全局虚拟时间 (GVT) 统一表示
- 提供调试工具（时间线可视化）

**需讨论**: 时间归一化精度要求是多少？

---

## 4. 架构原则确认

以下架构原则是否需要调整或补充？

| 原则 | 说明 | 确认 |
|------|------|------|
| **交易穿透** | TransactionContext 端到端穿透所有模块 | [ ] |
| **接口不变性** | 模块级交易接口在不同抽象层级间保持一致 | [ ] |
| **协议无关性** | 支持多总线协议 (AXI/CHI/TileLink) | [ ] |
| **双并行实现** | TLM/RTL 并行，通过 transaction_id 关联 | [ ] |
| **渐进式过渡** | 支持混合运行模式，增量验证 | [ ] |

**补充原则**: _______________

---

## 5. 实施优先级确认

### Phase 0: 环境准备（本周）
- [ ] 确认 systemc-3.0.1 目录状态
- [ ] 验证构建系统（cmake && make）
- [ ] 运行 21 个测试用例
- [ ] 创建 CppHDL 符号链接

### Phase 1: 交易上下文与基础接口（第 2-4 周）
- [ ] TransactionContext 实现
- [ ] Port<T> 模板实现
- [ ] 传播规则实现
- [ ] 单元测试

### Phase 2: 适配层核心（第 5-8 周）
- [ ] TLMToStreamAdapter 实现
- [ ] ProtocolAdapter (AXI4) 实现
- [ ] ReorderBuffer 实现
- [ ] 集成测试

### Phase 3: 双并行实现（第 9-12 周）
- [ ] impl_type 配置框架
- [ ] 时间归一化层
- [ ] compare/shadow 模式
- [ ] 状态映射验证

**需确认**:
- [ ] Phase 划分是否合理？
- [ ] 时间估计是否准确？
- [ ] 资源需求是否明确？

---

## 6. 验收标准确认

### Phase 1 验收标准
- [ ] 100+ 交易完整上下文传播验证通过
- [ ] trace_log 完整性验证通过
- [ ] 跨交易干扰事件正确记录
- [ ] Port<T> 编译通过，类型安全
- [ ] PacketPort 无缝替换 SimplePort
- [ ] 单元测试覆盖率 > 90%

### Phase 2 验收标准
- [ ] TLM 到 RTL 数据流零数据损坏
- [ ] 背压正确传播
- [ ] transaction_id 端到端关联验证通过
- [ ] AXI4 ProtocolAdapter 通过一致性测试

### Phase 3 验收标准
- [ ] compare 模式显示功能等价
- [ ] 生成时间归一化的延迟对比报告
- [ ] 状态映射验证通过

**需确认**: 验收标准是否需要调整？

---

## 7. 文档重组状态

✅ 已完成:
- `docs/00-START_HERE.md` — 入口文档
- `docs/01-ARCHITECTURE/hybrid-modeling-overview.md` — 架构概览
- `docs/01-ARCHITECTURE/transaction-context.md` — 交易上下文设计
- `docs/01-ARCHITECTURE/cpphdl-integration-analysis.md` — CppHDL 集成分析
- `docs/01-ARCHITECTURE/architecture-discussion-input.md` — 本文档

📋 待创建:
- `docs/01-ARCHITECTURE/adapter-design.md` — 适配器 API 设计
- `docs/01-ARCHITECTURE/protocol-adapters.md` — 协议适配器规范
- `docs/02-IMPLEMENTATION/phase-0-setup.md` — Phase 0 实施指南
- `docs/02-IMPLEMENTATION/phase-1-transaction-context.md` — Phase 1 实施指南

---

## 8. 讨论输出

### 决策记录

| 决策点 | 决策结果 | 备注 |
|--------|---------|------|
| TransactionContext 设计 | | |
| Port<T> 兼容策略 | | |
| TLMToStreamAdapter 状态机 | | |
| 双并行实现模式 | | |
| CppHDL 集成方案 | | |

### 风险缓解计划

| 风险 | 缓解措施 | 负责人 |
|------|---------|--------|
| | | |

### 行动计划

| 任务 | 优先级 | 预计开始 | 负责人 |
|------|--------|---------|--------|
| | | | |

---

## 9. 附录：参考文档

| 文档 | 位置 |
|------|------|
| 项目审计报告 | `PROJECT_AUDIT_AND_NEXT_STEPS.md` |
| 架构概览 | `docs/01-ARCHITECTURE/hybrid-modeling-overview.md` |
| 交易上下文设计 | `docs/01-ARCHITECTURE/transaction-context.md` |
| CppHDL 集成分析 | `docs/01-ARCHITECTURE/cpphdl-integration-analysis.md` |
| 历史设计 (v1) | `docs/05-LEGACY/v1-hybrid-design/` |
| 历史设计 (v2) | `docs/05-LEGACY/v2-improvements/` |
| 提案文档 | `/workspace/mynotes/CppTLM/docs/proposal/多层次混合仿真.md` |

---

**准备就绪**: 等待架构讨论开始
