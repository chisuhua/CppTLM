# CppTLM v2.0 迁移指南

> **版本**: 1.0  
> **创建日期**: 2026-04-10  
> **适用对象**: 从 v1.0 迁移到 v2.0 的开发者

---

## 一、v2.0 新特性概览

### 核心功能

| 特性 | v1.0 | v2.0 | 迁移影响 |
|------|------|------|---------|
| **SimObject** | 基础 tick | + reset/snapshot/层次化 | 向后兼容 |
| **Packet** | 基础字段 | + transaction_id/error_code | 向后兼容 |
| **错误处理** | 无 | ErrorContextExt + DebugTracker | 新增 API |
| **交易追踪** | 无 | TransactionContextExt + TransactionTracker | 新增 API |
| **模块** | CacheSim等 | CacheV2/CrossbarV2/MemoryV2 | 新增类 |

---

## 二、代码迁移

### 2.1 SimObject 迁移

**v1.0 代码**:
```cpp
class MyModule : public SimObject {
    void tick() override {
        // 业务逻辑
    }
};
```

**v2.0 代码** (向后兼容，无修改必要):
```cpp
class MyModule : public SimObject {
    void tick() override {
        // 业务逻辑
    }
    
    // 可选：添加复位逻辑
    void do_reset(const ResetConfig& config) override {
        // 复位时的清理工作
        buffer_.clear();
    }
};
```

### 2.2 Packet 迁移

**v1.0 代码**:
```cpp
pkt->stream_id = 100;
```

**v2.0 代码** (推荐使用新 API):
```cpp
pkt->set_transaction_id(100);  // 推荐：自动同步 Extension
// pkt->stream_id 自动更新为 100
```

**查询交易 ID**:
```cpp
// v1.0
uint64_t id = pkt->stream_id;

// v2.0 (推荐)
uint64_t id = pkt->get_transaction_id();
// 从 Extension 获取，回退到 stream_id
```

### 2.3 添加交易追踪

**新增代码**:
```cpp
#include "ext/transaction_context_ext.hh"
#include "framework/transaction_tracker.hh"

// 1. 初始化 Tracker
auto& tracker = TransactionTracker::instance();
tracker.initialize();

// 2. 创建交易
tlm::tlm_generic_payload payload;
uint64_t tid = tracker.create_transaction(&payload, "cpu_0", "READ");

// 3. 设置 Packet
Packet* pkt = new Packet(&payload, 0, PKT_REQ);
pkt->set_transaction_id(tid);

// 4. 记录 hop
tracker.record_hop(tid, "crossbar", 1, "hopped");

// 5. 完成交易
tracker.complete_transaction(tid);
```

### 2.4 添加错误处理

**新增代码**:
```cpp
#include "ext/error_context_ext.hh"
#include "framework/debug_tracker.hh"

if (error_condition) {
    // 1. 设置错误码
    pkt->set_error_code(ErrorCode::COHERENCE_STATE_VIOLATION);
    
    // 2. 附加 ErrorContextExt
    ErrorContextExt* ext = nullptr;
    pkt->payload->get_extension(ext);
    if (!ext) {
        ext = create_error_context(
            pkt->payload,
            ErrorCode::COHERENCE_STATE_VIOLATION,
            "State mismatch",
            get_name()
        );
    }
    
    // 3. 记录到 DebugTracker
    DebugTracker::instance().record_error(
        pkt->payload,
        ErrorCode::COHERENCE_STATE_VIOLATION,
        "State mismatch detected",
        get_name()
    );
}
```

---

## 三、模块升级

### 3.1 CacheV2 替代 CacheSim

**v1.0**:
```cpp
#include "modules/cache_sim.hh"
CacheSim cache("l1", eq, 1024);
```

**v2.0**:
```cpp
#include "modules/modules_v2.hh"
CacheV2 cache("l1", eq, 1024);

// 新特性:
// - 支持层次化复位
// - 自动创建子交易 (miss 时)
// - 集成交易追踪
```

### 3.2 CrossbarV2 替代 Crossbar

**v1.0**:
```cpp
#include "modules/crossbar.hh"
Crossbar xbar("xbar", eq, 4);
```

**v2.0**:
```cpp
#include "modules/modules_v2.hh"
CrossbarV2 xbar("xbar", eq);

// 新特性:
// - 透传 transaction_id
// - 自动记录 hop 延迟
// - 支持层次化复位
```

### 3.3 MemoryV2 替代 MemorySim

**v1.0**:
```cpp
#include "modules/memory_sim.hh"
MemorySim mem("mem", eq, 0x10000000);
```

**v2.0**:
```cpp
#include "modules/modules_v2.hh"
MemoryV2 mem("mem", eq);

// 新特性:
// - 错误检测 (地址越界)
// - 自动完成交易
// - 集成 DebugTracker
```

---

## 四、构建系统

### 4.1 CMakeLists.txt

**v1.0**:
```cmake
add_library(cpptlm STATIC ${SOURCES})
```

**v2.0** (向后兼容):
```cmake
add_library(cpptlm_core STATIC ${CORE_SOURCES})

# 新增: 包含目录
target_include_directories(cpptlm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/core
)
```

### 4.2 编译选项

**v2.0 新增选项**:
```cmake
option(USE_SYSTEMC "Enable SystemC" OFF)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
```

---

## 五、测试迁移

### 5.1 添加新测试

**v2.0 测试模板**:
```cpp
#include <catch2/catch_all.hpp>
#include "core/sim_object.hh"
#include "framework/transaction_tracker.hh"

TEST_CASE("MyFeature", "[feature]") {
    GIVEN("前置条件") {
        WHEN("执行操作") {
            THEN("验证结果") {
                REQUIRE(...);
            }
        }
    }
}
```

### 5.2 运行测试

```bash
# 配置
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON

# 构建
cmake --build . -j4

# 运行
ctest --output-on-failure

# 运行特定测试
ctest -R transaction  # 只运行包含 "transaction" 的测试
```

---

## 六、API 参考

### 6.1 SimObject 新 API

| 方法 | 说明 | 默认实现 |
|------|------|---------|
| `init()` | 初始化模块 | 设置 initialized_=true |
| `do_reset(config)` | 复位逻辑 | 空 |
| `save_snapshot(j)` | 保存快照 | 保存 name/initialized |
| `load_snapshot(j)` | 加载快照 | 恢复 name/initialized |
| `add_child(obj)` | 添加子模块 | - |
| `get_parent()` | 获取父模块 | - |

### 6.2 Packet 新 API

| 方法 | 说明 |
|------|------|
| `get_transaction_id()` | 获取交易 ID |
| `set_transaction_id(tid)` | 设置交易 ID |
| `get_parent_id()` | 获取父交易 ID |
| `set_fragment_info(id, total)` | 设置分片信息 |
| `is_fragmented()` | 检查是否分片 |
| `get_group_key()` | 获取分组键 |
| `add_trace(...)` | 添加追踪日志 |
| `get_error_code()` | 获取错误码 |
| `set_error_code(code)` | 设置错误码 |

### 6.3 Tracker API

| Tracker | 方法 | 说明 |
|---------|------|------|
| **TransactionTracker** | `create_transaction(...)` | 创建交易 |
| | `record_hop(...)` | 记录 hop |
| | `complete_transaction(...)` | 完成交易 |
| | `link_transactions(...)` | 链接父子 |
| | `get_transaction(tid)` | 查询记录 |
| **DebugTracker** | `record_error(...)` | 记录错误 |
| | `record_state_transition(...)` | 记录状态 |
| | `get_error(id)` | 查询错误 |
| | `get_state_history(addr)` | 状态历史 |

---

## 七、常见问题

### Q1: 迁移需要修改现有代码吗？

**A**: 不需要。v2.0 完全向后兼容，现有代码无需修改即可编译运行。

### Q2: 何时使用新 API？

**A**: 
- **立即使用**: `set_transaction_id()` 替代直接修改 `stream_id`
- **按需使用**: 交易追踪、错误处理（新功能）

### Q3: 如何启用交易追踪？

**A**:
```cpp
// 1. 初始化
auto& tracker = TransactionTracker::instance();
tracker.initialize();

// 2. 创建交易时自动追踪
uint64_t tid = tracker.create_transaction(&payload, "cpu", "READ");
```

### Q4: 如何调试错误？

**A**:
```cpp
// 1. 启用 DebugTracker
DebugTracker::instance().initialize(true, true, true);  // stop_on_fatal=true

// 2. 查询错误
auto errors = DebugTracker::instance().get_errors_by_category(ErrorCategory::COHERENCE);
```

---

## 八、升级检查清单

- [ ] 编译通过
- [ ] 现有测试通过
- [ ] 新测试通过
- [ ] 性能无下降
- [ ] 文档更新

---

## 九、相关资源

| 资源 | 位置 |
|------|------|
| 开发者指南 | `docs/guide/DEVELOPER_GUIDE.md` |
| 技能清单 | `docs/skills/SKILLS_SUMMARY.md` |
| 架构文档 | `docs/architecture/` |
| 示例代码 | `examples/` |

---

**维护**: CppTLM 开发团队  
**版本**: 1.0  
**最后更新**: 2026-04-10
