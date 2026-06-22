# CppTLM v2.0 测试用例编写与修复计划

> **版本**: 1.0  
> **日期**: 2026-04-10  
> **状态**: 🀄 准备执行  
> **基于**: docs/TEST_PLAN_v2.md (架构审查修订版)

---

## 一、计划概览

### 1.1 修复与编写统计

| 类型 | P0 | P1 | P2 | 总计 | 预计时间 |
|------|----|----|----|------|----------|
| **修复现有问题** | 3 | 1 | 0 | 4 | 2 小时 |
| **编写新测试** | 6 | 10 | 5 | 21 | 4 天 |
| **总计** | 9 | 11 | 5 | 25 | ~4.5 天 |

### 1.2 优先级定义

| 优先级 | 说明 | 执行时间 | 验收标准 |
|--------|------|----------|---------|
| **P0** | 核心架构验证，ADR 关键决策验证 | 1-2 天 | 100% 通过 |
| **P1** | 重要功能补充，错误传播完善 | 1-2 天 | 100% 通过 |
| **P2** | 框架层验证，性能/压力测试 | 1 天 | 无崩溃 |

---

## 二、详细计划

### Phase P0: 核心修复与关键补充（预计 4 小时）

#### BUG-001: 修复 T1.6 快照测试（30 分钟）

```cpp
// 文件: test/test_phase2_core_extensions.cc
// 问题: MockSimObject.save_snapshot 缺少 type 字段
// 修复: 添加 {"type", get_module_type()} 到 json 输出
```

#### BUG-002: 修复 T2.7 便捷函数测试（30 分钟）

```cpp
// 文件: test/test_phase3_transaction_context.cc
// 问题: payload 管理 extension 生命周期，手动 delete 导致双重释放
// 修复: 移除 delete ext/retrieved 调用
```

#### BUG-003: 恢复 test_phase4_error_context.cc（15 分钟）

```bash
# 操作: 重命名避免重复定义，恢复被禁用的测试文件
mv test/test_phase4_error_context.cc.disabled test/test_phase4_error_context.cc
# 修改重复的 TEST_CASE 名为 Phase4 前缀
```

#### T7.1: 根交易完整生命周期（1 小时）
**架构验证**: ADR-X.1 (上游分配，下游透传)

```cpp
TEST_CASE("T7.1: 根交易完整生命周期", "[transaction][lifecycle][P0]") {
    GIVEN("一个 CPU 模块（上游）和一个 Memory 模块（下游）") {
        // 1. CPU 分配 transaction_id
        // 2. Cache/Crossbar 透传 transaction_id
        // 3. Memory 终止交易
        // 4. 验证 TransactionTracker 记录完整
        
        THEN("transaction_id 由 CPU 分配，下游透传，Memory 终止") {
            REQUIRE(cpu_assigned_id == memory_received_id);
            REQUIRE(tracker.get_transaction(cpu_assigned_id)->is_complete);
        }
    }
}
```

#### T7.2: 子交易创建与关联（1 小时）
**架构验证**: ADR-X.7 (TRANSFORM 行为)

```cpp
TEST_CASE("T7.2: 子交易创建与关联", "[transaction][sub][P0]") {
    GIVEN("Cache Miss 场景") {
        // 1. 父交易中到达 Cache
        // 2. Cache Miss，创建子交易
        // 3. 链接父子交易 (link_transactions)
        // 4. 验证 parent_id 正确关联
        
        THEN("子交易 parent_id 指向父交易") {
            REQUIRE(child->get_parent() == parent->get_id());
            REQUIRE(tracker.get_children(parent->get_id()).contains(child->get_id()));
        }
    }
}
```

#### T7.3: 分片交易完整流程（1 小时）
**架构验证**: ADR-X.8 (parent_id + fragment_id/total)

```cpp
TEST_CASE("T7.3: 分片交易完整流程", "[transaction][fragment][P0]") {
    GIVEN("一个分片交易（4 分片）") {
        // 1. 创建 4 个分片，共享 parent_id
        // 2. 设置 fragment_id (0-3) 和 fragment_total (4)
        // 3. 通过 get_group_key() 验证分组
        // 4. 验证分片重组逻辑
        
        THEN("所有分片通过 parent_id 关联") {
            REQUIRE(ext1.get_group_key() == parent_id);
            REQUIRE(ext2.get_group_key() == parent_id);
        }
        THEN("fragment_id/total 正确") {
            REQUIRE(ext1.is_first_fragment());
            REQUIRE(ext4.is_last_fragment());
        }
    }
}
```

#### T8.1: 错误码设置与获取（30 分钟）
**架构验证**: ADR-X.2 (error_code + error_category)

```cpp
TEST_CASE("T8.1: 错误码设置与获取", "[error][basic][P0]") {
    GIVEN("一个 Packet 和 ErrorContextExt") {
        // 1. 设置 error_code (如 TRANSPORT_INVALID_ADDRESS)
        // 2. 验证 get_error_code() 返回正确值
        // 3. 验证 ErrorContextExt 附加到 payload
        
        THEN("错误码正确设置和获取") {
            REQUIRE(pkt->get_error_code() == ErrorCode::TRANSPORT_INVALID_ADDRESS);
            REQUIRE(ext->error_category == ErrorCategory::TRANSPORT);
        }
    }
}
```

#### T8.3: 严重错误处理（30 分钟）
**架构验证**: ADR-X.2 (fatal 错误分类)

```cpp
TEST_CASE("T8.3: 严重错误处理", "[error][fatal][P0]") {
    GIVEN("一个致命错误场景（如 COHERENCE_DEADLOCK）") {
        // 1. 设置 fatal 错误码
        // 2. 验证 is_fatal() 返回 true
        // 3. 验证 DebugTracker 记录严重错误
        
        THEN("错误被标记为 fatal") {
            REQUIRE(pkt->get_error_code() == ErrorCode::COHERENCE_DEADLOCK);
            REQUIRE(ext->is_fatal());
            REQUIRE(tracker.get_errors_by_category(ErrorCategory::COHERENCE).size() > 0);
        }
    }
}
```

#### T9.1: 完整请求-响应流程端到端（1.5 小时）
**架构验证**: 架构 01 + ADR-X.1 (全流程)

```cpp
TEST_CASE("T9.1: 完整请求-响应端到端流程", "[integration][e2e][P0]") {
    GIVEN("CPU → Cache → Crossbar → Memory → 响应 的完整链路") {
        // 1. CPU 发起请求，分配 transaction_id
        // 2. Cache Hit/Miss 处理
        // 3. Crossbar 透传
        // 4. Memory 处理并生成响应
        // 5. 响应沿路径返回
        // 6. 验证 transaction_id 全程透传
        
        THEN("transaction_id 在请求和响应中保持一致") {
            REQUIRE(resp->get_transaction_id() == req->get_transaction_id());
        }
        THEN("端到端延迟正确计算") {
            REQUIRE(resp->getDelayCycles() >= expected_latency);
        }
    }
}
```

#### T9.4: 错误场景端到端流程（1 小时）
**架构验证**: 架构 03 (错误检测 → 传播 → 处理)

```cpp
TEST_CASE("T9.4: 错误场景端到端流程", "[integration][error][P0]") {
    GIVEN("Memory 地址越界错误场景") {
        // 1. CPU 发起无效地址请求
        // 2. 链路传递到 Memory
        // 3. Memory 检测错误，设置 error_code
        // 4. 错误沿路径传播
        // 5. DebugTracker 记录错误
        
        THEN("错误被正确检测和记录") {
            REQUIRE(resp->get_error_code() == ErrorCode::TRANSPORT_INVALID_ADDRESS);
            REQUIRE(tracker.get_errors_by_category(ErrorCategory::TRANSPORT).size() > 0);
        }
    }
}
```

---

### Phase P1: 重要功能补充（预计 1-2 天）

#### T6.5: TransactionAction 验证（30 分钟）
**架构验证**: ADR-X.7 (PASSTHROUGH/TRANSFORM/TERMINATE)

#### T7.4: 交易追踪日志导出（1 小时）
**架构验证**: 架构 02 (trace_log 验证)

#### T7.5: 粗/细粒度切换（30 分钟）
**架构验证**: 架构 02 (enable_coarse_grained)

#### T8.2: 错误跨模块传播（1 小时）
**架构验证**: 架构 03 (Cache → Crossbar → Memory 错误传播)

#### T8.4: 可恢复错误处理（30 分钟）
**架构验证**: ADR-X.2 (recoverable 错误分类)

#### T8.5: 错误与交易关联（30 分钟）
**架构验证**: ADR-X.2 (共享 DebugTracker)

#### T9.2: 多模块级联测试（1.5 小时）
**架构验证**: 架构 01 (层次化)

#### T9.3: Cache Miss 子交易流程（1 小时）
**架构验证**: ADR-X.7 (TRANSFORM 行为)

#### T10.3: 内存泄漏检测（30 分钟）
**工具**: ASan

#### BUG-006: 错误码缺少 SUCCESS/RETRY/NACK（30 分钟）
**架构验证**: 架构 03 (错误码枚举)

---

### Phase P2: 框架与性能（预计 1 天）

#### T10.1: 大量并发交易（2 小时）
#### T10.2: 长时间运行稳定性（2 小时）
#### T11.1-T11.3: 框架层测试（2 小时）
#### T12.1-T12.2: Bundle 层测试（1 小时）
#### T4.6: DebugTraceExt 基础构造（30 分钟）

---

## 三、执行顺序

```
Day 1: Phase P0 (修复 4 问题 + 补充 6 核心用例)
├── BUG-001: T1.6 快照修复 (0.5h)
├── BUG-002: T2.7 便捷函数修复 (0.5h)
├── BUG-003: 恢复 Phase4 测试 (0.25h)
├── T7.1: 根交易生命周期 (1h)
├── T7.2: 子交易创建 (1h)
├── T8.1: 错误码设置与获取 (0.5h)
└── T8.3: 严重错误处理 (0.5h)
    → 验证: 核心 P0 全部通过

Day 2-3: Phase P1 (补充 10 用例)
├── Day 2: T7 剩余 + T8 剩余
│   ├── T7.3: 分片交易 (1h)
│   ├── T7.4: 日志导出 (1h)
│   ├── T8.2: 错误传播 (1h)
│   └── T8.4/8.5: 错误处理剩余 (1h)
├── Day 3: T9 + T6/T10
│   ├── T9.1: 完整请求-响应 (1.5h)
│   ├── T9.2: 多模块级联 (1h)
│   ├── T6.5: TransactionAction (0.5h)
│   └── T10.3: 内存泄漏 (0.5h)
    → 验证: P1 全部通过

Day 4-5: Phase P2 (5 用例) + 验收
├── T10.1: 并发交易 (2h)
├── T11.1-T11.3: 框架层 (2h)
├── T12.1-T12.2: Bundle 层 (1h)
└── Phase D: 回归验证 (1h)
    → 验证: 全部 63 用例通过
```

---

**维护**: CppTLM 开发团队  
**版本**: 1.0  
**最后更新**: 2026-04-10
