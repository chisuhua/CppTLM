# CppTLM v2.0 Phase 3 测试计划（架构审查修订版）

> **版本**: 1.0  
> **日期**: 2026-04-10  
> **状态**: 📋 准备执行  
> **基于**: Oracle 架构审查报告 + Explore 代码检查

---

## 一、P3 阶段目标

根据 Oracle 审查发现的 **5 个严重风险** 和 **7 个中等问题**，P3 阶段的核心目标是：

1. **风险消除**: 修复内存泄漏、错误处理失效、协议状态缺失等关键问题
2. **框架完善**: 实现 TransactionAction、TLMModule 基类、ResetCoordinator
3. **架构一致性**: 确保代码实现与 docs/ 下的架构文档 100% 一致
4. **系统整合**: 验证 Tracker、PacketPool、Module 之间的完整集成

---

## 二、P3 修复与测试矩阵

### 阶段 P3.1: 严重风险修复（Priority 0）

| 风险 ID | 风险描述 | 修复文件 | 测试用例 | 预计时间 |
|---------|---------|---------|---------|---------|
| **R1** | **PacketPool 内存泄漏**<br>`release()` 未 `clear_extensions()` | `include/core/packet_pool.hh` | T11.1: 内存清理验证 | 30 min |
| **R2** | **错误处理失效**<br>`Packet::set_error_code` 为 Stub | `include/core/packet.hh` | T11.2: 错误集成验证 | 30 min |
| **R3** | **一致性状态缺失**<br>缺少 OWNED/TRANSIENT 状态 | `include/ext/error_context_ext.hh` | T11.3: 状态完整性 | 15 min |
| **R4** | **调试追踪缺失**<br>DebugTraceExt 未实现 | `include/ext/debug_trace_ext.hh` | T11.4: 调试扩展验证 | 30 min |
| **R5** | **快照递归缺失**<br>save_snapshot 不支持子模块 | `include/core/sim_object.hh` | T11.5: 递归快照验证 | 30 min |

### 阶段 P3.2: 基础架构实现（Priority 1）

| 任务 ID | 任务描述 | 架构依据 | 修复文件 | 测试用例 | 预计时间 |
|---------|---------|---------|---------|---------|---------|
| **T12.1** | TransactionAction 枚举 | ADR-X.7 | `include/core/sim_object.hh` | T12.1: 动作枚举 | 15 min |
| **T12.2** | TransactionInfo 结构体 | ADR-X.7 | `include/core/sim_object.hh` | T12.2: 交易信息结构 | 15 min |
| **T12.3** | TLMModule 基类 | 02-transaction-architecture.md | `include/core/tlm_module.hh` | T12.3: 虚接口验证 | 1 h |
| **T12.4** | reset() 递归行为 | 04-reset-checkpoint-architecture.md | `include/core/sim_object.hh` | T12.4: 递归复位 | 30 min |
| **T12.5** | Replay 接口 | ADR-X.2 | `include/framework/debug_tracker.hh` | T12.5: 回放功能 | 30 min |

### 阶段 P3.3: 框架层实现（Priority 2）

| 任务 ID | 任务描述 | 架构依据 | 修复文件 | 测试用例 | 预计时间 |
|---------|---------|---------|---------|---------|---------|
| **T13.1** | ModuleRegistry | 01-hybrid-architecture-v2.md | `include/framework/module_registry.hh` | T13.1: 注册与实例化 | 1 h |
| **T13.2** | impl_mode.hh | 01-hybrid-architecture-v2.md | `include/framework/impl_mode.hh` | T13.2: 模式枚举 | 30 min |
| **T13.3** | PortPair 增强 | 01-hybrid-architecture-v2.md | `include/core/simple_port.hh` | T13.3: 解耦连接 | 30 min |
| **T13.4** | ResetCoordinator | 04-reset-checkpoint-architecture.md | `include/framework/reset_coordinator.hh` | T13.4: 系统复位 | 1 h |
| **T13.5** | CheckpointManager | 04-reset-checkpoint-architecture.md | `include/framework/checkpoint_manager.hh` | T13.5: 快照管理 | 1 h |

---

## 三、测试用例详情

### P3.1: 严重风险修复测试

#### T11.1: PacketPool 内存清理验证 (修复 R1)

```cpp
TEST_CASE("T11.1: PacketPool 内存清理验证", "[memory][pool][P3.1]") {
    GIVEN("一个带有 Extension 的 Packet") {
        Packet* pkt = PacketPool::get().acquire();
        create_transaction_context(pkt->payload, 100, 0, 0, 1); // 附加 Extension
        REQUIRE(get_transaction_context(pkt->payload) != nullptr);
        
        WHEN("Packet 被释放回 Pool") {
            PacketPool::get().release(pkt);
            
            THEN("Extension 应被正确清理而非泄漏") {
                // 验证 Pool 内部队列是否有可用的 Packet
                // 且该 Packet 的 payload 不应残留旧的 Extension 数据
                // (如果有 ASan，应无泄漏报错)
                REQUIRE_NOTHROW(PacketPool::get().acquire());
            }
        }
    }
}
```

#### T11.2: 错误集成验证 (修复 R2)

```cpp
TEST_CASE("T11.2: Packet 错误处理集成", "[error][packet][P3.1]") {
    GIVEN("一个 Packet") {
        Packet* pkt = PacketPool::get().acquire();
        
        WHEN("设置错误码") {
            pkt->set_error_code(ErrorCode::TRANSPORT_INVALID_ADDRESS);
            
            THEN("错误码应正确存储在 ErrorContextExt 中") {
                REQUIRE(pkt->get_error_code() == ErrorCode::TRANSPORT_INVALID_ADDRESS);
                
                auto* err_ext = get_error_context(pkt->payload);
                REQUIRE(err_ext != nullptr);
                REQUIRE(err_ext->error_code == ErrorCode::TRANSPORT_INVALID_ADDRESS);
                REQUIRE(err_ext->error_category == ErrorCategory::TRANSPORT);
            }
        }
        
        // 清理
        PacketPool::get().release(pkt);
    }
}
```

#### T11.3: CoherenceState 完整性 (修复 R3)

```cpp
TEST_CASE("T11.3: CoherenceState 状态完整性", "[error][state][P3.1]") {
    THEN("应包含所有 6 种协议状态") {
        REQUIRE(static_cast<int>(CoherenceState::INVALID) == 0);
        REQUIRE(static_cast<int>(CoherenceState::SHARED) == 1);
        REQUIRE(static_cast<int>(CoherenceState::EXCLUSIVE) == 2);
        REQUIRE(static_cast<int>(CoherenceState::MODIFIED) == 3);
        REQUIRE(static_cast<int>(CoherenceState::OWNED) == 4);
        REQUIRE(static_cast<int>(CoherenceState::TRANSIENT) == 0x10);
    }
    
    THEN("应有字符串转换函数") {
        REQUIRE(coherence_state_to_string(CoherenceState::INVALID) == "INVALID");
        REQUIRE(coherence_state_to_string(CoherenceState::OWNED) == "OWNED");
        REQUIRE(coherence_state_to_string(CoherenceState::TRANSIENT) == "TRANSIENT");
    }
}
```

#### T11.4: DebugTraceExt 验证 (修复 R4)

```cpp
TEST_CASE("T11.4: DebugTraceExt 功能验证", "[debug][trace][P3.1]") {
    GIVEN("一个带有 DebugTraceExt 的 Payload") {
        tlm::tlm_generic_payload payload;
        auto* trace_ext = add_debug_trace(&payload); // 辅助函数创建
        
        WHEN("记录追踪日志") {
            trace_ext->add_log("cache", 100, "hit");
            trace_ext->add_log("crossbar", 105, "hop");
            
            THEN("日志应完整保存") {
                REQUIRE(trace_ext->log_entries.size() == 2);
                REQUIRE(trace_ext->log_entries[0].module == "cache");
                REQUIRE(trace_ext->log_entries[1].timestamp == 105);
            }
        }
    }
}
```

#### T11.5: 递归快照验证 (修复 R5)

```cpp
TEST_CASE("T11.5: 递归快照验证", "[snapshot][hierarchy][P3.1]") {
    GIVEN("具有层次化结构的模块") {
        EventQueue eq;
        auto* parent = new MockResettableModule("parent", &eq);
        auto* child = new MockResettableModule("child", &eq);
        parent->add_child(child);
        
        WHEN("递归保存快照") {
            json snapshot;
            save_recursive_snapshot(parent, snapshot);
            
            THEN("子模块快照应被包含") {
                REQUIRE(snapshot.contains("name"));
                REQUIRE(snapshot.contains("children"));
                REQUIRE(snapshot["children"].contains("child"));
            }
        }
        
        delete parent;
    }
}
```

### P3.2: 基础架构测试

#### T12.1: TransactionAction 验证

```cpp
TEST_CASE("T12.1: TransactionAction 枚举", "[transaction][action][P3.2]") {
    THEN("枚举值应正确定义") {
        REQUIRE(static_cast<int>(TransactionAction::PASSTHROUGH) == 0);
        REQUIRE(static_cast<int>(TransactionAction::TRANSFORM) == 1);
        REQUIRE(static_cast<int>(TransactionAction::TERMINATE) == 2);
        REQUIRE(static_cast<int>(TransactionAction::BLOCK) == 3);
    }
}
```

#### T12.3: TLMModule 基类验证

```cpp
TEST_CASE("T12.3: TLMModule 虚接口验证", "[transaction][base_class][P3.2]") {
    GIVEN("一个从 TLMModule 派生的模块") {
        // 验证是否继承了 onTransactionStart/Hop/End 等虚方法
        
        THEN("应能正确处理交易生命周期") {
            // 验证虚调用链
            SUCCEED("TLMModule 虚接口验证通过");
        }
    }
}
```

---

## 四、执行计划

| 阶段 | 内容 | 预计时间 | 关键里程碑 |
|------|------|---------|-----------|
| **P3.1** | 修复 5 个严重风险 | 2.5 小时 | 100% 通过 T11.1-T11.5 |
| **P3.2** | 实现 5 个基础组件 | 3.5 小时 | 100% 通过 T12.1-T12.5 |
| **P3.3** | 框架层实现 | 4 小时 | 100% 通过 T13.1-T13.5 |
| **验收** | 完整回归测试 | 1 小时 | 100% 新增测试通过 |
| **总计** | | **11 小时** | 覆盖率 100% |

---

## 五、成功标准

### 代码覆盖率目标

| 模块 | 行覆盖率 | 分支覆盖率 |
|------|---------|-----------|
| `include/core/packet_pool.hh` | 95% | 90% |
| `include/core/packet.hh` | 95% | 90% |
| `include/ext/` (所有扩展) | 95% | 90% |
| `include/framework/` (Trackers) | 90% | 85% |

### 架构一致性标准

- [ ] 所有 ADR-X.1~X.8 中的决策在代码中有对应实现
- [ ] docs/ 中描述的接口在代码中存在且功能正确
- [ ] 内存泄漏风险消除（ASan 验证通过）
- [ ] 错误处理系统端到端可用

---

**维护**: CppTLM 开发团队  
**版本**: 1.0  
**最后更新**: 2026-04-10  
**下次审查**: P3.1 完成后
