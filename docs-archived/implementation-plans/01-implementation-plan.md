# CppTLM v2.0 实施计划

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待讨论  
> **目标**: 基于已确认的架构决策，实施 v2.0 核心功能

---

## 1. 实施总览

### 1.1 实施范围

基于已确认的 ADR 决策，实施以下内容：

| 架构 | ADR 参考 | 实施优先级 |
|------|---------|-----------|
| 交易处理架构 | ADR-X.1/X.6/X.7/X.8 | P0 |
| 错误处理架构 | ADR-X.2 | P0 |
| 复位系统 | ADR-X.3 | P1 |
| 构建系统 | ADR-X.5 | P0 |
| 模块注册 | ADR-X.4 | P1 |

**不在 v2.0 范围**:
- ❌ 完整检查点（v2.2）
- ❌ 动态库插件（v2.1 可选）
- ❌ 事件队列持久化（v2.2）

---

### 1.2 实施阶段

```
┌─────────────────────────────────────────────────────────────┐
│  Phase 1: 构建系统（1 天）                                   │
│  - CMakeLists.txt + ccache + CI                             │
├─────────────────────────────────────────────────────────────┤
│  Phase 2: 核心基础（2 天）                                   │
│  - SimObject 扩展 + ModuleRegistry + Packet 扩展            │
├─────────────────────────────────────────────────────────────┤
│  Phase 3: 交易处理（2 天）                                   │
│  - TransactionContextExt + TransactionTracker + 分片支持    │
├─────────────────────────────────────────────────────────────┤
│  Phase 4: 错误处理（2 天）                                   │
│  - ErrorContextExt + DebugTracker + 一致性追踪              │
├─────────────────────────────────────────────────────────────┤
│  Phase 5: 复位系统（1 天）                                   │
│  - ResetCoordinator + 快照（JSON）                          │
├─────────────────────────────────────────────────────────────┤
│  Phase 6: 示例与测试（2 天）                                 │
│  - 3 个示例程序 + 单元测试                                  │
└─────────────────────────────────────────────────────────────┘

总计：10 天
```

---

## 2. 详细实施计划

### Phase 1: 构建系统（1 天）

**目标**: 建立现代化构建系统，支持快速迭代

#### 任务 1.1: CMakeLists.txt 配置

**文件**:
- `CMakeLists.txt`（根配置）
- `cmake/modules/FindSystemC.cmake`
- `src/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `examples/CMakeLists.txt`

**内容**:
```cmake
# 核心配置
- ccache 自动检测
- C++17 标准
- SystemC 可选启用（本地头文件）
- 编译选项（-Wall -Wextra -O2 -g）

# 子目录
- src/ (核心库)
- tests/ (单元测试)
- examples/ (示例程序)
```

**验收标准**:
- [ ] `cmake -S . -B build -G Ninja` 成功
- [ ] `cmake --build build` 成功
- [ ] ccache 自动启用
- [ ] SystemC 可选启用（`-DUSE_SYSTEMC=ON`）

**预计**: 2 小时

---

#### 任务 1.2: 构建脚本

**文件**:
- `scripts/build.sh`
- `scripts/test.sh`
- `scripts/format.sh`
- `scripts/clean.sh`

**内容**:
```bash
# build.sh
- 自动创建 build 目录
- 支持 CMake 参数透传
- 支持 BUILD_TYPE, USE_SYSTEMC 等选项

# test.sh
- 运行 ctest
- 支持参数透传

# format.sh
- clang-format 代码格式化

# clean.sh
- 清理构建目录
- 清理 ccache 缓存（可选）
```

**验收标准**:
- [ ] `./scripts/build.sh` 成功
- [ ] `./scripts/test.sh` 成功
- [ ] `./scripts/format.sh` 成功

**预计**: 1 小时

---

#### 任务 1.3: GitHub Actions CI

**文件**:
- `.github/workflows/ci.yml`

**内容**:
```yaml
# CI 流程
- 触发：push/PR
- 矩阵：Release/Debug × SystemC ON/OFF
- 步骤：checkout → install deps → configure → build → test
- 缓存：ccache 缓存
- 产物：测试结果上传
```

**验收标准**:
- [ ] CI 自动触发
- [ ] 所有矩阵构建通过
- [ ] 测试结果可见

**预计**: 1 小时

---

#### 任务 1.4: 依赖集成

**内容**:
- Catch2（FetchContent）
- nlohmann/json（FetchContent）
- SystemC 头文件（本地 `external/systemc/`）

**文件**:
- `cmake/deps/Catch2.cmake`
- `cmake/deps/json.cmake`

**验收标准**:
- [ ] Catch2 自动下载
- [ ] nlohmann/json 自动下载
- [ ] SystemC 头文件正确引用

**预计**: 1 小时

---

### Phase 2: 核心基础（2 天）

**目标**: 建立核心基础架构，支持交易/错误/复位系统

#### 任务 2.1: SimObject 扩展

**文件**:
- `include/core/sim_object.hh`
- `src/core/sim_object.cc`

**内容**:
```cpp
class SimObject {
    // 生命周期
    virtual void init();
    virtual void tick() = 0;
    
    // 复位（v2.0）
    virtual void reset(const ResetConfig& config);
    virtual void do_reset(const ResetConfig& config);
    
    // 快照（v2.1）
    virtual void save_snapshot(json& j);
    virtual void load_snapshot(const json& j);
    
    // 层次化管理
    void add_child(SimObject* child);
    const std::vector<SimObject*>& get_children() const;
    
    // 状态查询
    bool is_initialized() const;
    const std::string& name() const;
};
```

**验收标准**:
- [ ] 层次化复位正常工作
- [ ] 子类可重写 `do_reset()`
- [ ] 快照接口（预留 v2.1）

**预计**: 3 小时

---

#### 任务 2.2: Packet 扩展

**文件**:
- `include/core/packet.hh`

**内容**:
```cpp
class Packet {
    // 现有字段
    tlm_generic_payload* payload;
    uint64_t stream_id;
    // ...
    
    // 新增：错误处理
    ErrorCode error_code_;
    std::string error_message_;
    
    // 新增：便捷方法
    uint64_t get_transaction_id() const;
    void set_transaction_id(uint64_t tid);
    ErrorCode get_error_code() const;
    void set_error_code(ErrorCode code, const std::string& msg = "");
    bool has_error() const;
    void add_trace(const std::string& module, ...);
};
```

**验收标准**:
- [ ] `get/set_transaction_id()` 与 Extension 同步
- [ ] `get/set_error_code()` 正常工作
- [ ] 向后兼容现有代码

**预计**: 2 小时

---

#### 任务 2.3: ModuleRegistry

**文件**:
- `include/core/module_registry.hh`
- `src/core/module_registry.cc`

**内容**:
```cpp
// 模块注册宏
#define REGISTER_MODULE(type_name, class_name)

// 注册器
class ModuleRegistry {
    void register_module(const std::string& name, ModuleFactoryFunc factory);
    SimObject* create_module(const std::string& type, const std::string& name);
    std::vector<std::string> list_modules() const;
};
```

**验收标准**:
- [ ] 模块注册成功
- [ ] 通过类型创建模块
- [ ] 列出可用模块

**预计**: 2 小时

---

#### 任务 2.4: Extension 基础

**文件**:
- `include/ext/extension_base.hh`

**内容**:
```cpp
// Extension 基类（复用现有 tlm_extension）
// 提供便捷函数
get_transaction_context(payload)
set_transaction_context(payload, ext)
get_error_context(payload)
set_error_context(payload, ext)
```

**验收标准**:
- [ ] Extension 创建/获取正常
- [ ] 与 TLM payload 集成

**预计**: 1 小时

---

### Phase 3: 交易处理（2 天）

**目标**: 实现完整交易处理架构

#### 任务 3.1: TransactionContextExt

**文件**:
- `include/ext/transaction_context_ext.hh`

**内容**:
```cpp
struct TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    // 核心字段
    uint64_t transaction_id;
    uint64_t parent_id;
    uint8_t fragment_id;
    uint8_t fragment_total;
    
    // 调试字段
    uint64_t create_timestamp;
    std::string source_module;
    std::string type;
    uint8_t priority;
    
    // 追踪日志
    struct TraceEntry { ... };
    std::vector<TraceEntry> trace_log;
    
    // 辅助方法
    bool is_root() const;
    bool is_fragmented() const;
    uint64_t get_group_key() const;
    void add_trace(...);
};
```

**验收标准**:
- [ ] Extension clone/copy 正常
- [ ] 辅助方法正确
- [ ] 与 Packet 同步

**预计**: 3 小时

---

#### 任务 3.2: TransactionTracker

**文件**:
- `include/framework/transaction_tracker.hh`

**内容**:
```cpp
class TransactionTracker {
    // 交易生命周期
    uint64_t create_transaction(payload, source, type);
    void record_hop(tid, module, latency, event);
    void complete_transaction(tid);
    void link_transactions(parent_id, child_id);
    void set_fragment_info(tid, fragment_id, fragment_total);
    
    // 查询接口
    const TransactionRecord* get_transaction(tid) const;
    std::vector<uint64_t> get_children(parent_id) const;
    std::vector<uint64_t> get_active_transactions() const;
    
    // 配置
    void enable_coarse_grained(bool);
    void enable_fine_grained(bool);
};
```

**验收标准**:
- [ ] 交易创建/记录/完成正常
- [ ] 父子交易关联正确
- [ ] 分片信息设置正确
- [ ] 粗/细粒度可配置

**预计**: 4 小时

---

#### 任务 3.3: TLM 模块基类

**文件**:
- `include/core/tlm_module.hh`

**内容**:
```cpp
class TLMModule : public SimObject {
protected:
    // 分片重组缓冲
    std::map<uint64_t, FragmentBuffer> fragment_buffers_;
    bool enable_fragment_reassembly_ = false;
    
public:
    // 交易处理接口
    virtual TransactionInfo onTransactionStart(Packet* pkt);
    virtual TransactionInfo onTransactionHop(Packet* pkt);
    virtual TransactionInfo onTransactionEnd(Packet* pkt);
    virtual uint64_t createSubTransaction(Packet* parent, Packet* child);
    
    // 分片处理接口
    virtual void onFragmentReceived(Packet* pkt);
    virtual void onFragmentGroupComplete(FragmentBuffer& buffer);
    virtual void handlePacket(Packet* pkt) = 0;
};
```

**验收标准**:
- [ ] 交易处理接口可重写
- [ ] 分片重组缓冲正常
- [ ] 默认实现正确

**预计**: 3 小时

---

#### 任务 3.4: RTL 模块基类

**文件**:
- `include/core/rtl_module.hh`

**内容**:
```cpp
template<typename ComponentT, typename ReqBundle, typename RespBundle>
class RTLModule : public SimObject {
protected:
    std::unique_ptr<ch::ch_device<ComponentT>> device_;
    SimplePort* req_port;
    SimplePort* resp_port;
    
public:
    void tick() override {
        // Port → ch_stream → Component → ch_stream → Port
        // 透传 transaction_id 和 Extension
    }
};
```

**验收标准**:
- [ ] RTL 模块透传正常
- [ ] CppHDL Component 集成正确

**预计**: 2 小时

---

#### 任务 3.5: 模块实现（示例）

**文件**:
- `include/modules/cache_v2.hh`
- `include/modules/crossbar_v2.hh`
- `include/modules/memory_v2.hh`

**内容**:
```cpp
// CacheV2: 转换型模块
class CacheV2 : public TLMModule {
    TransactionInfo onTransactionHop() override;
    uint64_t createSubTransaction() override;
    void handlePacket(Packet* pkt) override;
};

// CrossbarV2: 透传型模块
class CrossbarV2 : public TLMModule {
    TransactionInfo onTransactionHop() override;
    void handlePacket(Packet* pkt) override;
};

// MemoryV2: 终止型模块
class MemoryV2 : public TLMModule {
    TransactionInfo onTransactionEnd() override;
    void handlePacket(Packet* pkt) override;
};
```

**验收标准**:
- [ ] 三种模块类型正确实现
- [ ] 交易处理逻辑正确
- [ ] 单元测试通过

**预计**: 4 小时

---

### Phase 4: 错误处理（2 天）

**目标**: 实现完整错误处理与调试架构

#### 任务 4.1: ErrorContextExt

**文件**:
- `include/ext/error_context_ext.hh`

**内容**:
```cpp
struct ErrorContextExt : public tlm::tlm_extension<ErrorContextExt> {
    ErrorCode error_code;
    ErrorCategory error_category;
    std::string error_message;
    std::string source_module;
    uint64_t timestamp;
    
    // 堆栈追踪
    struct StackFrame { ... };
    std::vector<StackFrame> stack_trace;
    
    // 上下文数据
    std::map<std::string, uint64_t> context_data;
    
    // 一致性特定字段
    CoherenceState expected_state;
    CoherenceState actual_state;
    std::vector<uint32_t> sharers;
};
```

**验收标准**:
- [ ] Extension clone/copy 正常
- [ ] 堆栈追踪正常
- [ ] 上下文数据可设置/获取

**预计**: 3 小时

---

#### 任务 4.2: DebugTraceExt

**文件**:
- `include/ext/debug_trace_ext.hh`

**内容**:
```cpp
struct DebugTraceExt : public tlm::tlm_extension<DebugTraceExt> {
    struct TraceEvent {
        uint64_t timestamp;
        std::string event_type;  // "ERROR", "STATE_CHANGE"
        std::string description;
        uint64_t data;
    };
    std::vector<TraceEvent> events;
    
    void add_event(type, desc, data);
    void add_error_event(code, desc);
    void add_state_event(from, to, trigger);
};
```

**验收标准**:
- [ ] 事件添加正常
- [ ] 与 ErrorContextExt 互补

**预计**: 2 小时

---

#### 任务 4.3: ErrorCategory & ErrorCode

**文件**:
- `include/framework/error_category.hh`
- `include/framework/coherence_state.hh`

**内容**:
```cpp
// 错误类别
enum class ErrorCategory : uint8_t {
    TRANSPORT = 0x01,
    RESOURCE = 0x02,
    COHERENCE = 0x03,
    PROTOCOL = 0x04,
    SECURITY = 0x05,
    PERFORMANCE = 0x06,
};

// 错误码（按类别组织）
enum class ErrorCode : uint16_t {
    TRANSPORT_INVALID_ADDRESS = 0x0100,
    COHERENCE_STATE_VIOLATION = 0x0300,
    COHERENCE_DEADLOCK = 0x0301,
    // ...
};

// 一致性状态
enum class CoherenceState : uint8_t {
    INVALID, SHARED, EXCLUSIVE, MODIFIED, OWNED, TRANSIENT
};
```

**验收标准**:
- [ ] 错误类别/码定义完整
- [ ] 工具函数正确（`is_error()`, `is_fatal_error()`）
- [ ] 一致性状态枚举完整

**预计**: 2 小时

---

#### 任务 4.4: DebugTracker

**文件**:
- `include/framework/debug_tracker.hh`

**内容**:
```cpp
class DebugTracker {
    // 记录错误
    uint64_t record_error(payload, code, message, module);
    
    // 记录状态转换
    void record_state_transition(addr, from, to, event, tid);
    
    // 查询接口
    const ErrorRecord* get_error(error_id);
    std::vector<ErrorRecord> get_errors_by_transaction(tid);
    std::vector<ErrorRecord> get_errors_by_category(cat);
    std::vector<StateSnapshot> get_state_history(addr);
    
    // 回放接口
    std::string replay_transaction(tid);
    std::string replay_address_history(addr);
    
    // 统计接口
    size_t error_count();
    std::map<ErrorCategory, size_t> errors_by_category();
};
```

**验收标准**:
- [ ] 错误记录正常
- [ ] 查询接口正确
- [ ] 回放接口输出可读
- [ ] 统计接口准确

**预计**: 5 小时

---

#### 任务 4.5: 模块错误处理集成

**文件**:
- `include/modules/cache_v2.hh`（扩展）
- `include/modules/crossbar_v2.hh`（扩展）
- `include/modules/memory_v2.hh`（扩展）

**内容**:
```cpp
// Cache: 检测一致性违例
if (current == INVALID && is_write) {
    pkt->set_error_code(ErrorCode::COHERENCE_INVALID_TRANSITION);
    
    if (ErrorContextExt* ext = get_error_context(payload)) {
        ext->expected_state = EXCLUSIVE;
        ext->actual_state = MODIFIED;
    }
    
    DebugTracker::instance().record_error(...);
}
```

**验收标准**:
- [ ] 模块错误检测正常
- [ ] 错误上下文设置正确
- [ ] DebugTracker 记录正确

**预计**: 3 小时

---

### Phase 5: 复位系统（1 天）

**目标**: 实现层次化复位与快照功能

#### 任务 5.1: ResetCoordinator

**文件**:
- `include/framework/reset_coordinator.hh`

**内容**:
```cpp
class ResetCoordinator {
    void register_root_module(SimObject* module);
    void set_default_config(const ResetConfig& config);
    
    // 系统级复位
    void system_reset(const ResetConfig& config);
    
    // 模块级复位
    void module_reset(SimObject* module, const ResetConfig& config);
};
```

**验收标准**:
- [ ] 系统级复位正常
- [ ] 模块级复位正常
- [ ] 层次化复位正确

**预计**: 2 小时

---

#### 任务 5.2: CheckpointManager（v2.1 预留）

**文件**:
- `include/framework/checkpoint_manager.hh`

**内容**:
```cpp
class CheckpointManager {
    void save(const std::string& path);  // JSON 快照
    void load(const std::string& path);
    
    void save_full_checkpoint(const std::string& dir);  // v2.2
    void load_full_checkpoint(const std::string& dir);  // v2.2
    
    void set_auto_save(bool enable, uint64_t interval);
};
```

**验收标准**:
- [ ] 快照保存/加载接口（v2.1 实现）
- [ ] 自动保存配置（v2.1 实现）

**预计**: 3 小时（预留接口，v2.1 实现）

---

#### 任务 5.3: 模块复位实现

**文件**:
- `include/modules/cache_v2.hh`（扩展）
- `include/modules/crossbar_v2.hh`（扩展）
- `include/modules/memory_v2.hh`（扩展）

**内容**:
```cpp
void do_reset(const ResetConfig& config) override {
    cache_lines_.clear();
    cache_states_.clear();
    // 清除请求缓冲
    while (!req_buffer_.empty()) {
        PacketPool::get().release(req_buffer_.front());
        req_buffer_.pop();
    }
}
```

**验收标准**:
- [ ] 模块复位后状态清零
- [ ] 资源正确释放

**预计**: 2 小时

---

### Phase 6: 示例与测试（2 天）

**目标**: 提供示例程序和测试用例

#### 任务 6.1: 示例程序

**文件**:
- `examples/cache_system/main.cpp`
- `examples/noc_system/main.cpp`
- `examples/coherence_system/main.cpp`

**内容**:
```cpp
// Cache 系统示例
int sc_main() {
    // 初始化追踪器
    DebugTracker::instance().initialize(true, true, true);
    
    // 创建模块
    auto* cpu = new CPUSim("cpu");
    auto* cache = new CacheV2("cache");
    auto* memory = new MemoryV2("memory");
    
    // 运行仿真
    eq.run(100000);
    
    // 查询统计
    auto stats = DebugTracker::instance().errors_by_category();
    
    return 0;
}
```

**验收标准**:
- [ ] 3 个示例程序可编译
- [ ] 示例程序可运行
- [ ] 输出正确结果

**预计**: 4 小时

---

#### 任务 6.2: 单元测试

**文件**:
- `tests/unit/test_cache.cpp`
- `tests/unit/test_crossbar.cpp`
- `tests/unit/test_memory.cpp`
- `tests/unit/test_transaction.cpp`
- `tests/unit/test_error.cpp`

**内容**:
```cpp
// Catch2 测试
TEST_CASE("Cache hit", "[cache]") {
    CacheV2 cache("test_cache");
    // ...
    REQUIRE(cache.hit_rate() > 0.9);
}

TEST_CASE("Transaction tracking", "[transaction]") {
    uint64_t tid = TransactionTracker::instance().create_transaction(...);
    REQUIRE(tid > 0);
}
```

**验收标准**:
- [ ] 所有单元测试通过
- [ ] 覆盖率 >80%（可选）

**预计**: 4 小时

---

#### 任务 6.3: 集成测试

**文件**:
- `tests/integration/test_coherence.cpp`
- `tests/integration/test_deadlock.cpp`

**内容**:
```cpp
// 一致性违例检测测试
TEST_CASE("Coherence violation detection", "[integration][coherence]") {
    // 设置场景：两个核心同时写入同一地址
    // 验证：DebugTracker 捕获违例
    REQUIRE(DebugTracker::instance().error_count() > 0);
}
```

**验收标准**:
- [ ] 集成测试通过
- [ ] 错误场景正确检测

**预计**: 2 小时

---

## 3. 实施议题列表

以下是需要逐一讨论确认的实施议题：

### 议题 1: 文件组织结构

**讨论点**:
- 头文件是否全部放在 `include/`？
- 源文件是否全部放在 `src/`？
- 模板类是否 header-only？

**建议**:
- 头文件：`include/`
- 源文件：`src/`（非模板类）
- 模板类：header-only（`include/`）

---

### 议题 2: 命名空间设计

**讨论点**:
- 是否需要命名空间？
- 命名空间层次如何设计？

**建议**:
```cpp
namespace cpptlm {
    namespace core { }      // SimObject, Packet
    namespace modules { }   // CacheV2, CrossbarV2
    namespace framework { } // Tracker, Coordinator
    namespace ext { }       // Extension
}
```

---

### 议题 3: 日志系统

**讨论点**:
- 使用什么日志系统？
- 日志级别如何定义？

**建议**:
- 简单实现：`DPRINTF(module, format, ...)` 宏
- 日志级别：DEBUG, INFO, WARNING, ERROR, FATAL

---

### 议题 4: 配置系统

**讨论点**:
- 配置文件格式？
- 配置加载方式？

**建议**:
- 格式：JSON（nlohmann/json）
- 加载：`load_config("system.json")`

---

### 议题 5: 内存管理

**讨论点**:
- Packet 生命周期如何管理？
- 是否使用智能指针？

**建议**:
- PacketPool 管理 Packet 生命周期
- 模块使用 `std::unique_ptr` 管理子模块

---

### 议题 6: 线程安全

**讨论点**:
- 是否需要线程安全？
- Tracker 单例是否加锁？

**建议**:
- v2.0: 单线程仿真，不加锁
- v2.1+: 可选线程安全（加锁）

---

### 议题 7: 向后兼容

**讨论点**:
- 现有代码如何迁移？
- 是否需要兼容层？

**建议**:
- 提供迁移指南
- 关键接口保持兼容
- 旧模块可逐步迁移

---

### 议题 8: 文档生成

**讨论点**:
- 是否生成 API 文档？
- 使用什么工具？

**建议**:
- Doxygen 生成 API 文档
- Markdown 编写用户指南

---

## 4. 实施优先级

| 优先级 | 阶段 | 依赖 | 工期 | 风险 |
|--------|------|------|------|------|
| **P0** | Phase 1: 构建系统 | 无 | 1 天 | 低 |
| **P0** | Phase 2: 核心基础 | Phase 1 | 2 天 | 低 |
| **P0** | Phase 3: 交易处理 | Phase 2 | 2 天 | 中 |
| **P0** | Phase 4: 错误处理 | Phase 3 | 2 天 | 中 |
| **P1** | Phase 5: 复位系统 | Phase 4 | 1 天 | 低 |
| **P1** | Phase 6: 示例与测试 | Phase 5 | 2 天 | 低 |

---

## 5. 风险与缓解

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| **Extension 机制复杂** | 高 | 中 | 提前原型验证 |
| **分片重组逻辑复杂** | 中 | 中 | 简化 v2.0 范围 |
| **DebugTracker 性能** | 中 | 低 | 可选启用 |
| **向后兼容问题** | 高 | 中 | 提前测试现有代码 |

---

## 6. 验收标准

### v2.0 完成标准

- [ ] 所有 P0 阶段完成
- [ ] 构建系统正常工作
- [ ] 交易处理架构完整
- [ ] 错误处理架构完整
- [ ] 复位系统基础完成
- [ ] 3 个示例程序可运行
- [ ] 单元测试通过率 100%
- [ ] 集成测试通过率 100%
- [ ] 文档完整

---

**下一步**: 请老板审阅实施计划，并逐一讨论上述 8 个议题。
