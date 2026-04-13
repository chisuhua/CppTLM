# ADR-X 系列议题决策汇总

> **版本**: 3.0 ✅  
> **日期**: 2026-04-09  
> **状态**: ✅ 全部完成  
> **关联**: 架构 v2.0, 交易处理架构 v1.0, 错误调试架构 v1.0, 复位检查点架构 v1.0

---

## 📊 议题总览

| ADR 编号 | 议题 | 状态 | 决策 |
|---------|------|------|------|
| **ADR-X.1** | 事务追踪 ID 分配 | ✅ 已确认 | 上游分配 + 分层 ID |
| **ADR-X.2** | 错误处理策略 | ✅ 已确认 | 分层错误码 + TLM Extension + DebugTracker |
| **ADR-X.3** | 复位策略 | ✅ 已确认 | 层次化复位 + 可选快照（分阶段） |
| **ADR-X.4** | 插件系统 | ✅ 已确认 | v2.0 静态链接 + v2.1 可选动态库 |
| **ADR-X.5** | 构建系统 | ✅ 已确认 | CMake+Ninja + ccache + 本地 SystemC |
| **ADR-X.6** | TransactionContext 整合 | ✅ 已确认 | Extension 机制 + Packet 共存 |
| **ADR-X.7** | 模块/框架职责划分 | ✅ 已确认 | 声明式虚方法 + 框架自动追踪 |
| **ADR-X.8** | 细粒度分片处理 | ✅ 已确认 | TLM 智能处理 + RTL 简单透传 |

---

## ✅ 已确认议题详情

### ADR-X.1: 事务追踪 ID 分配

**决策**: 上游分配 + 分层 ID

```cpp
// Initiator 模块分配 ID
req.transaction_id = (node_id << 32) | (local_counter++ & 0xFFFFFFFF);
```

**理由**:
- 简单，无需协调
- 支持多 Initiator 并发
- 与 AXI/CHI 协议一致

**文档**: `ADR-X.6-transaction-integration.md`

---

### ADR-X.2: 错误处理策略

**决策**: 分层错误码 + TLM Extension + DebugTracker

| 问题 | 决策 |
|------|------|
| Q1: 错误分类 | 分层（传输/资源/一致性/协议/安全/性能） |
| Q2: Extension 设计 | 多 Extension（ErrorContext + DebugTrace） |
| Q3: 追踪方式 | 内存索引 + 查询接口（非 CSV） |
| Q4: 回放支持 | replay_transaction / replay_address_history |
| Q5: 与交易整合 | 共享 DebugTracker 框架 |

**核心架构**:
```
ErrorContextExt (错误上下文)
DebugTraceExt (调试追踪)
TransactionContextExt (交易上下文)
    ↓
DebugTracker (统一追踪器)
    - 错误记录（按 transaction_id 索引）
    - 状态快照（按地址索引）
    - 查询/回放接口
```

**文档**: `ADR-X.2-error-handling.md`, `02-architecture/03-error-debug-architecture.md`

---

### ADR-X.3: 复位策略

**决策**: 层次化复位 + 可选快照（分阶段实施）

| 问题 | 决策 |
|------|------|
| Q1: 复位粒度 | 层次化（父→子） |
| Q2: 复位时机 | 全部支持（仿真前/异常后/手动） |
| Q3: 状态保存 | 可选快照（JSON 格式） |
| Q4: 与交易整合 | 可配置（保留/清除） |
| Q5: 与错误整合 | 可配置（保留用于调试） |

**分阶段实施**:
- **v2.0**: 轻量级复位（`reset()` 接口）
- **v2.1**: 状态快照（JSON 格式，多配置对比）
- **v2.2**: 完整检查点（事件队列持久化）

**文档**: `ADR-X.3-reset-strategy.md`, `02-architecture/04-reset-checkpoint-architecture.md`

---

### ADR-X.4: 插件系统

**决策**: v2.0 静态链接 + v2.1 可选动态库

| 问题 | 决策 |
|------|------|
| Q1: v2.0 是否需要插件？ | 不需要（静态链接） |
| Q2: v2.1 是否添加动态库？ | 可选（按需实现） |
| Q3: 插件接口复杂度？ | 中等 |
| Q4: 第三方 IP 需求？ | 未来（v2.1） |
| Q5: 实施时机？ | v2.1 |

**v2.0 实现**:
```cpp
REGISTER_MODULE(cache_v2, CacheV2)
SimObject* module = ModuleRegistry::instance().create_module(type, name);
```

**文档**: `ADR-X.4-plugin-system.md`

---

### ADR-X.5: 构建系统

**决策**: CMake+Ninja + ccache + 本地 SystemC

| 问题 | 决策 |
|------|------|
| Q1: 构建工具？ | CMake + Ninja |
| Q2: ccache 支持？ | 自动检测并启用 |
| Q3: SystemC 支持？ | 可选启用（USE_SYSTEMC 选项） |
| Q4: SystemC 来源？ | 项目内头文件（`external/systemc/`） |
| Q5: 测试框架？ | Catch2 |
| Q6: 依赖管理？ | FetchContent |
| Q7: CI/CD? | GitHub Actions |

**构建命令**:
```bash
# 标准构建（自动使用 ccache）
./scripts/build.sh

# 启用 SystemC
./scripts/build.sh -DUSE_SYSTEMC=ON

# 运行测试
./scripts/test.sh

# 查看 ccache 统计
ccache -s
```

**文档**: `ADR-X.5-build-system.md`

---

### ADR-X.6: TransactionContext 整合

**决策**: Extension 机制 + Packet 共存

```
Packet::stream_id ←→ TransactionContextExt::transaction_id（同步）
```

**理由**:
- 复用现有机制
- 向后兼容
- 模块层简化（只调用 get/set_transaction_id()）

**文档**: `ADR-X.6-transaction-integration.md`

---

### ADR-X.7: 模块/框架职责划分

**决策**: 声明式虚方法 + 框架自动追踪

**三种模块类型**:
| 类型 | 重写方法 | action | 示例 |
|------|---------|--------|------|
| 透传型 | `onTransactionHop()` | PASSTHROUGH | Crossbar |
| 转换型 | `createSubTransaction()` | TRANSFORM | Cache |
| 终止型 | `onTransactionEnd()` | TERMINATE | Memory |

**文档**: `ADR-X.7-transaction-handling.md`

---

### ADR-X.8: 细粒度分片处理

**决策**: TLM 智能处理 + RTL 简单透传

| 特性 | TLM 模块 | RTL 模块 |
|------|---------|---------|
| 分片识别 | ✅ parent_id + fragment_id | ❌ 不感知 |
| 分片重组 | ✅ 目的模块可重组 | ❌ 不支持 |
| 交易追踪 | ✅ 可记录 trace_log | ❌ 透传 |

**分片标识**:
```cpp
struct TransactionContextExt {
    uint64_t transaction_id;   // 分片 ID
    uint64_t parent_id;        // 父交易 ID（所有分片共享）
    uint8_t  fragment_id;      // 分片序号
    uint8_t  fragment_total;   // 总分片数
};
```

**文档**: `ADR-X.8-fragment-handling.md`

---

## 📁 文档位置

| 文档 | 位置 |
|------|------|
| 架构 v2.0 | `02-architecture/01-hybrid-architecture-v2.md` |
| 交易处理架构 | `02-architecture/02-transaction-architecture.md` |
| 错误调试架构 | `02-architecture/03-error-debug-architecture.md` |
| 复位检查点架构 | `02-architecture/04-reset-checkpoint-architecture.md` |
| ADR-X.2 错误处理 | `ADR-X.2-error-handling.md` |
| ADR-X.3 复位策略 | `ADR-X.3-reset-strategy.md` |
| ADR-X.4 插件系统 | `ADR-X.4-plugin-system.md` |
| ADR-X.5 构建系统 | `ADR-X.5-build-system.md` |
| ADR-X.6 交易整合 | `ADR-X.6-transaction-integration.md` |
| ADR-X.7 交易处理 | `ADR-X.7-transaction-handling.md` |
| ADR-X.8 分片处理 | `ADR-X.8-fragment-handling.md` |
| 本汇总 | `ADR-X-SUMMARY.md` |

---

## 📊 架构文档结构

```
docs-pending/02-architecture/
├── 01-hybrid-architecture-v2.md         # 架构 v2.0 ✅
├── 02-transaction-architecture.md       # 交易处理架构 ✅
├── 03-error-debug-architecture.md       # 错误调试架构 ✅
├── 04-reset-checkpoint-architecture.md  # 复位检查点架构 ✅
└── examples/                            # 示例代码
    ├── bundles/
    ├── tlm/
    ├── rtl/
    └── framework/

docs-pending/03-adr/
├── README.md                            # ADR 索引 ✅
├── ADR-P1-TEMPLATE.md                   # P1 级议题 ✅
├── ADR-X-SUMMARY.md                     # X 系列汇总 ✅
├── ADR-X.2-error-handling.md            # 错误处理 ✅
├── ADR-X.3-reset-strategy.md            # 复位策略 ✅
├── ADR-X.4-plugin-system.md             # 插件系统 ✅
├── ADR-X.5-build-system.md              # 构建系统 ✅
├── ADR-X.6-transaction-integration.md   # 交易整合 ✅
├── ADR-X.7-transaction-handling.md      # 交易处理 ✅
└── ADR-X.8-fragment-handling.md         # 分片处理 ✅
```

---

## 📈 完成度统计

| 类别 | 总数 | 已确认 | 待确认 | 完成率 |
|------|------|--------|--------|--------|
| **P0 级** | 4 | 4 | 0 | 100% |
| **P1 级** | 5 | 5 | 0 | 100% |
| **X 级** | 8 | 8 | 0 | **100%** ✅ |
| **总计** | **17** | **17** | **0** | **100%** ✅ |

---

## 🎯 核心架构决策汇总

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
| 错误分类 | 分层（传输/资源/一致性/协议/安全/性能） |
| Extension | ErrorContext + DebugTrace |
| 追踪器 | DebugTracker 内存索引 |
| 回放支持 | replay_transaction / replay_address |
| 与交易整合 | 共享框架 |

### 复位与检查点架构

| 决策 | 方案 |
|------|------|
| 复位粒度 | 层次化（父→子） |
| 快照格式 | JSON（人类可读） |
| 实施策略 | 分阶段（v2.0→v2.1→v2.2） |
| 与交易整合 | 可配置保留/清除 |

### 插件系统

| 决策 | 方案 |
|------|------|
| v2.0 | 静态链接（ModuleRegistry） |
| v2.1 | 可选动态库（按需实现） |
| 模块注册 | 编译时注册 + 配置创建 |

### 构建系统

| 决策 | 方案 |
|------|------|
| 构建工具 | CMake 3.16+ + Ninja |
| 编译加速 | ccache 自动检测 |
| SystemC | 可选启用 + 本地头文件 |
| 测试框架 | Catch2 |
| 依赖管理 | FetchContent |
| CI/CD | GitHub Actions |

---

## 🚀 下一步行动

### 阶段 1: 实施准备（1 天）

- [ ] 确认所有架构文档
- [ ] 创建实施计划
- [ ] 分配任务（如需要）

### 阶段 2: 核心架构实施（v2.0, 5-7 天）

- [ ] 交易处理架构（TransactionContextExt, TransactionTracker）
- [ ] 错误处理架构（ErrorContextExt, DebugTracker）
- [ ] 复位系统（ResetCoordinator, SimObject::reset）
- [ ] 构建系统（CMakeLists.txt, CI 配置）
- [ ] 模块注册机制（ModuleRegistry）

### 阶段 3: 示例与测试（2-3 天）

- [ ] 创建示例程序（3 个）
- [ ] 编写单元测试
- [ ] 集成测试

### 阶段 4: 文档完善（1 天）

- [ ] API 文档
- [ ] 用户指南
- [ ] 开发者指南

---

## 📋 实施优先级

| 优先级 | 模块 | 依赖 | 工期 |
|--------|------|------|------|
| **P0** | 构建系统 | 无 | 1 天 |
| **P0** | 交易处理 | 无 | 2 天 |
| **P0** | 错误处理 | 交易处理 | 2 天 |
| **P1** | 复位系统 | 交易处理 + 错误处理 | 2 天 |
| **P1** | 模块注册 | 无 | 1 天 |
| **P2** | 示例程序 | 以上全部 | 2 天 |
| **P2** | 测试 | 以上全部 | 2 天 |

---

**维护**: DevMate  
**版本**: v3.0 ✅  
**最后更新**: 2026-04-09  
**状态**: **ADR-X 系列全部完成，准备进入实施阶段**
