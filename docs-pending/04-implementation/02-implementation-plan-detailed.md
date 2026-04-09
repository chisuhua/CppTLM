# CppTLM v2.0 详细实施计划

> **版本**: 2.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **基于**: 现有代码完整分析

---

## 1. 现有代码分析

### 1.1 目录结构

```
CppTLM/
├── CMakeLists.txt                 # 现有：gemsc 项目，需重构为 CppTLM
├── build.sh                       # 现有：简单构建脚本
├── include/                       # 现有：头文件
│   ├── core/                      # 核心头文件（22 个文件）
│   │   ├── sim_object.hh          # ✅ 需扩展（reset/snapshot）
│   │   ├── packet.hh              # ✅ 需扩展（transaction_id/error_code）
│   │   ├── event_queue.hh         # ✅ 保留
│   │   ├── master_port.hh         # ✅ 保留
│   │   ├── slave_port.hh          # ✅ 保留
│   │   ├── simple_port.hh         # ✅ 保留
│   │   ├── port_manager.hh        # ✅ 保留
│   │   ├── packet_pool.hh         # ✅ 保留
│   │   ├── cmd.hh                 # ✅ 保留
│   │   └── ...
│   ├── ext/                       # Extension 头文件（7 个文件）
│   │   ├── mem_exts.hh            # ✅ 现有 Extension（ReadCmdExt 等）
│   │   ├── coherence_extension.hh # ✅ 现有 Extension
│   │   └── ...
│   ├── modules/                   # 模块头文件（14 个文件）
│   │   ├── cache_sim.hh           # ✅ 需升级为 CacheV2
│   │   ├── crossbar.hh            # ✅ 需升级为 CrossbarV2
│   │   ├── memory_sim.hh          # ✅ 需升级为 MemoryV2
│   │   └── ...
│   ├── sc_core/                   # SystemC 适配层（6 个文件）
│   └── utils/                     # 工具类（6 个文件）
├── src/                           # 现有：源文件
│   ├── core/                      # 核心实现（8 个文件）
│   ├── noc/                       # NoC 实现（5 个文件）
│   ├── packet/                    # Packet 实现（1 个文件）
│   ├── main.cpp                   # 主程序
│   └── sc_main.cpp                # SystemC 主程序
├── external/                      # 外部依赖
│   ├── CppHDL -> /workspace/CppHDL  # ✅ CppHDL 符号链接
│   └── json/                      # ✅ nlohmann/json
├── test/                          # 现有：测试
│   ├── catch_amalgamated.*        # ✅ Catch2 已集成
│   ├── test_*.cc                  # ✅ 17 个测试文件
│   └── CMakeLists.txt             # 测试配置
└── samples/                       # 现有：示例
    └── simple1/                   # 简单示例
```

---

### 1.2 现有代码评估

| 组件 | 状态 | v2.0 行动 |
|------|------|----------|
| **SimObject** | ✅ 已有基础 | 扩展 reset()/save_snapshot() |
| **Packet** | ✅ 已有基础 | 扩展 transaction_id/error_code 方法 |
| **TLM Extension** | ✅ 已有机制 | 新增 TransactionContextExt/ErrorContextExt |
| **模块（Cache/Crossbar）** | ✅ 已有实现 | 升级为 V2，实现交易处理接口 |
| **构建系统** | ⚠️ 基础 CMake | 重构为 CppTLM，添加 ccache |
| **测试框架** | ✅ Catch2 已集成 | 添加新测试 |
| **CppHDL** | ✅ 符号链接已存在 | 直接使用 |

---

### 1.3 现有代码问题

| 问题 | 影响 | 解决方案 |
|------|------|---------|
| **Packet 无 transaction_id 方法** | 无法与 Extension 同步 | 添加 get/set_transaction_id() |
| **SimObject 无 reset() 接口** | 无法层次化复位 | 添加 reset()/do_reset() |
| **无统一错误码** | 错误处理分散 | 添加 ErrorCode 枚举 |
| **无 DebugTracker** | 调试能力弱 | 新建 DebugTracker 单例 |
| **无 TransactionTracker** | 交易无法追踪 | 新建 TransactionTracker 单例 |
| **CMake 无 ccache** | 编译慢 | 添加 ccache 检测 |

---

## 2. 详细实施计划

### Phase 1: 构建系统改进（1 天）

#### 任务 1.1: 重构 CMakeLists.txt

**现有**:
```cmake
project(gemsc CXX)  # 旧项目名
add_executable(sim src/main.cpp)
```

**目标**:
```cmake
project(CppTLM VERSION 2.0.0 LANGUAGES CXX)

# ccache 支持
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# SystemC 可选（使用本地头文件）
option(USE_SYSTEMC "Enable SystemC support" OFF)
if(USE_SYSTEMC)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/systemc")
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/systemc)
        add_compile_definitions(USE_SYSTEMC)
    endif()
endif()

# 核心库
add_library(cpptlm_core STATIC src/core/*.cc src/packet/*.cc)

# 可执行文件
add_executable(cpptlm_sim src/main.cpp)
target_link_libraries(cpptlm_sim cpptlm_core)
```

**文件变更**:
- `CMakeLists.txt` - 重构

**验收标准**:
- [ ] `cmake -S . -B build -G Ninja` 成功
- [ ] `cmake --build build` 成功
- [ ] ccache 自动启用

**预计**: 2 小时

---

#### 任务 1.2: 改进构建脚本

**现有**: `build.sh`（简单）

**目标**:
```bash
#!/bin/bash
# scripts/build.sh

set -e

BUILD_TYPE=${BUILD_TYPE:-Release}
USE_SYSTEMC=${USE_SYSTEMC:-OFF}

mkdir -p build
cd build

cmake .. \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DUSE_SYSTEMC=$USE_SYSTEMC \
    "$@"

ninja
```

**文件变更**:
- `scripts/build.sh` - 新建
- `scripts/test.sh` - 新建
- `scripts/format.sh` - 新建

**验收标准**:
- [ ] `./scripts/build.sh` 成功
- [ ] `./scripts/test.sh` 运行测试

**预计**: 1 小时

---

#### 任务 1.3: GitHub Actions CI

**文件**: `.github/workflows/ci.yml`（新建）

**内容**:
```yaml
name: CI
on: [push, pull_request]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install deps
        run: sudo apt-get install -y ninja-build ccache
      - name: Configure
        run: cmake -S . -B build -G Ninja
      - name: Build
        run: cmake --build build
      - name: Test
        run: ctest --test-dir build
```

**验收标准**:
- [ ] CI 自动触发
- [ ] 构建通过

**预计**: 1 小时

---

### Phase 2: 核心基础扩展（2 天）

#### 任务 2.1: SimObject 扩展

**现有**: `include/core/sim_object.hh`

**变更**:
```cpp
// 添加 ResetConfig 结构
struct ResetConfig {
    bool hierarchical = true;
    bool save_snapshot = false;
    bool preserve_errors = true;
    bool preserve_transactions = false;
};

class SimObject {
protected:
    SimObject* parent_ = nullptr;
    std::vector<SimObject*> children_;
    bool initialized_ = false;
    
public:
    // 生命周期
    virtual void init() { initialized_ = true; }
    virtual void tick() = 0;
    
    // 复位（新增）
    virtual void reset(const ResetConfig& config = ResetConfig()) {
        if (config.hierarchical) {
            for (auto* child : children_) {
                child->reset(config);
            }
        }
        do_reset(config);
    }
    
    virtual void do_reset(const ResetConfig& config) {
        // 默认空实现
    }
    
    // 快照（v2.1 预留）
    virtual void save_snapshot(json& j) {
        j["name"] = name;
        j["initialized"] = initialized_;
    }
    
    virtual void load_snapshot(const json& j) {
        if (j.contains("name")) name = j["name"];
    }
    
    // 层次化管理（新增）
    void add_child(SimObject* child) {
        children_.push_back(child);
        child->set_parent(this);
    }
    
    void set_parent(SimObject* parent) { parent_ = parent; }
    SimObject* get_parent() const { return parent_; }
    const std::vector<SimObject*>& get_children() const { return children_; }
    
    // 状态查询（新增）
    bool is_initialized() const { return initialized_; }
};
```

**文件变更**:
- `include/core/sim_object.hh` - 扩展

**验收标准**:
- [ ] 层次化复位正常工作
- [ ] 子类可重写 do_reset()

**预计**: 3 小时

---

#### 任务 2.2: Packet 扩展

**现有**: `include/core/packet.hh`

**变更**:
```cpp
// 添加 ErrorCode 枚举（新文件 include/framework/error_category.hh）
enum class ErrorCode : uint16_t {
    SUCCESS = 0x0000,
    TRANSPORT_INVALID_ADDRESS = 0x0100,
    COHERENCE_STATE_VIOLATION = 0x0300,
    // ...
};

class Packet {
public:
    // 现有字段
    tlm::tlm_generic_payload* payload;
    uint64_t stream_id = 0;
    // ...
    
    // 新增：错误处理字段
    ErrorCode error_code_ = ErrorCode::SUCCESS;
    std::string error_message_;
    
    // 新增：便捷方法
    uint64_t get_transaction_id() const {
        if (payload) {
            // 从 TransactionContextExt 获取
            // （Extension 实现后）
        }
        return stream_id;
    }
    
    void set_transaction_id(uint64_t tid) {
        stream_id = tid;
        // 同步到 Extension（Extension 实现后）
    }
    
    ErrorCode get_error_code() const { return error_code_; }
    
    void set_error_code(ErrorCode code, const std::string& msg = "") {
        error_code_ = code;
        error_message_ = msg;
    }
    
    bool has_error() const {
        return error_code_ != ErrorCode::SUCCESS;
    }
    
    std::string get_error_message() const {
        return error_message_.empty() ? "Unknown error" : error_message_;
    }
};
```

**文件变更**:
- `include/core/packet.hh` - 扩展
- `include/framework/error_category.hh` - 新建

**验收标准**:
- [ ] get/set_transaction_id() 正常
- [ ] get/set_error_code() 正常
- [ ] 向后兼容现有代码

**预计**: 3 小时

---

#### 任务 2.3: 错误码定义

**文件**: `include/framework/error_category.hh`（新建）

**内容**:
```cpp
#ifndef ERROR_CATEGORY_HH
#define ERROR_CATEGORY_HH

#include <cstdint>
#include <string>

enum class ErrorCategory : uint8_t {
    TRANSPORT = 0x01,
    RESOURCE = 0x02,
    COHERENCE = 0x03,
    PROTOCOL = 0x04,
    SECURITY = 0x05,
    PERFORMANCE = 0x06,
};

enum class ErrorCode : uint16_t {
    SUCCESS = 0x0000,
    
    // 传输层 (0x01xx)
    TRANSPORT_INVALID_ADDRESS = 0x0100,
    TRANSPORT_ACCESS_DENIED = 0x0101,
    TRANSPORT_TIMEOUT = 0x0103,
    
    // 资源层 (0x02xx)
    RESOURCE_BUFFER_FULL = 0x0200,
    RESOURCE_STARVATION = 0x0203,
    
    // 一致性层 (0x03xx)
    COHERENCE_STATE_VIOLATION = 0x0300,
    COHERENCE_DEADLOCK = 0x0301,
    COHERENCE_LIVELOCK = 0x0302,
    COHERENCE_DATA_INCONSISTENCY = 0x0303,
    COHERENCE_INVALID_TRANSITION = 0x0304,
};

inline ErrorCategory get_error_category(ErrorCode code) {
    return static_cast<ErrorCategory>((static_cast<uint16_t>(code) >> 8) & 0xFF);
}

inline std::string error_code_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::COHERENCE_STATE_VIOLATION: return "COHERENCE_STATE_VIOLATION";
        // ...
        default: return "UNKNOWN";
    }
}

inline bool is_fatal_error(ErrorCode code) {
    return code == ErrorCode::COHERENCE_DEADLOCK ||
           code == ErrorCode::COHERENCE_DATA_INCONSISTENCY;
}

#endif // ERROR_CATEGORY_HH
```

**验收标准**:
- [ ] 错误码定义完整
- [ ] 工具函数正确

**预计**: 2 小时

---

### Phase 3: 交易处理架构（2 天）

#### 任务 3.1: TransactionContextExt

**文件**: `include/ext/transaction_context_ext.hh`（新建）

**内容**:
```cpp
#ifndef TRANSACTION_CONTEXT_EXT_HH
#define TRANSACTION_CONTEXT_EXT_HH

#include <tlm>
#include <vector>
#include <string>

struct TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    uint64_t transaction_id = 0;
    uint64_t parent_id = 0;
    uint8_t fragment_id = 0;
    uint8_t fragment_total = 1;
    uint64_t create_timestamp = 0;
    std::string source_module;
    std::string type;
    uint8_t priority = 0;
    
    struct TraceEntry {
        std::string module;
        uint64_t timestamp;
        uint64_t latency;
        std::string event;
    };
    std::vector<TraceEntry> trace_log;
    
    tlm_extension* clone() const override {
        return new TransactionContextExt(*this);
    }
    
    void copy_from(const tlm_extension_base& ext) override {
        auto& e = static_cast<const TransactionContextExt&>(ext);
        transaction_id = e.transaction_id;
        parent_id = e.parent_id;
        // ...
    }
    
    bool is_root() const { return parent_id == 0 && fragment_total == 1; }
    bool is_fragmented() const { return fragment_total > 1; }
    uint64_t get_group_key() const { return parent_id != 0 ? parent_id : transaction_id; }
    
    void add_trace(const std::string& module, uint64_t ts, uint64_t lat, const std::string& event) {
        trace_log.push_back({module, ts, lat, event});
    }
};

inline TransactionContextExt* get_transaction_context(tlm_generic_payload* p) {
    TransactionContextExt* ext = nullptr;
    p->get_extension(ext);
    return ext;
}

#endif // TRANSACTION_CONTEXT_EXT_HH
```

**验收标准**:
- [ ] Extension clone/copy 正常
- [ ] 辅助方法正确

**预计**: 3 小时

---

#### 任务 3.2: TransactionTracker

**文件**: `include/framework/transaction_tracker.hh`（新建）

**内容**:
```cpp
#ifndef TRANSACTION_TRACKER_HH
#define TRANSACTION_TRACKER_HH

#include <map>
#include <vector>
#include <string>

struct TransactionRecord {
    uint64_t transaction_id;
    uint64_t parent_id;
    uint8_t fragment_id;
    uint8_t fragment_total;
    std::string source_module;
    std::string type;
    uint64_t create_timestamp;
    uint64_t complete_timestamp;
    std::vector<std::pair<std::string, uint64_t>> hop_log;
    bool is_complete = false;
};

class TransactionTracker {
private:
    std::map<uint64_t, TransactionRecord> transactions_;
    std::map<uint64_t, std::vector<uint64_t>> parent_child_map_;
    uint64_t global_timestamp_ = 0;
    uint64_t next_transaction_id_ = 1;
    
    TransactionTracker() = default;
    
public:
    static TransactionTracker& instance() {
        static TransactionTracker tracker;
        return tracker;
    }
    
    uint64_t create_transaction(tlm_generic_payload* payload,
                                 const std::string& source,
                                 const std::string& type) {
        uint64_t tid = next_transaction_id_++;
        TransactionRecord record;
        record.transaction_id = tid;
        record.source_module = source;
        record.type = type;
        record.create_timestamp = global_timestamp_;
        transactions_[tid] = record;
        return tid;
    }
    
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency) {
        if (transactions_.count(tid)) {
            transactions_[tid].hop_log.push_back({module, latency});
        }
    }
    
    void complete_transaction(uint64_t tid) {
        if (transactions_.count(tid)) {
            transactions_[tid].complete_timestamp = global_timestamp_;
            transactions_[tid].is_complete = true;
        }
    }
    
    void link_transactions(uint64_t parent_id, uint64_t child_id) {
        parent_child_map_[parent_id].push_back(child_id);
        if (transactions_.count(child_id)) {
            transactions_[child_id].parent_id = parent_id;
        }
    }
    
    void advance_time(uint64_t delta) { global_timestamp_ += delta; }
    uint64_t get_current_time() const { return global_timestamp_; }
    
    const TransactionRecord* get_transaction(uint64_t tid) const {
        auto it = transactions_.find(tid);
        return (it != transactions_.end()) ? &it->second : nullptr;
    }
    
    size_t active_count() const {
        size_t count = 0;
        for (const auto& [tid, record] : transactions_) {
            if (!record.is_complete) count++;
        }
        return count;
    }
};

#endif // TRANSACTION_TRACKER_HH
```

**验收标准**:
- [ ] 交易创建/记录/完成正常
- [ ] 父子交易关联正确

**预计**: 4 小时

---

### Phase 4: 错误处理架构（2 天）

#### 任务 4.1: ErrorContextExt

**文件**: `include/ext/error_context_ext.hh`（新建）

**内容**:
```cpp
#ifndef ERROR_CONTEXT_EXT_HH
#define ERROR_CONTEXT_EXT_HH

#include <tlm>
#include "../framework/error_category.hh"
#include <vector>
#include <map>

struct ErrorContextExt : public tlm::tlm_extension<ErrorContextExt> {
    ErrorCode error_code = ErrorCode::SUCCESS;
    ErrorCategory error_category;
    std::string error_message;
    std::string source_module;
    uint64_t timestamp = 0;
    
    struct StackFrame {
        std::string module;
        std::string function;
        std::string context;
    };
    std::vector<StackFrame> stack_trace;
    
    std::map<std::string, uint64_t> context_data;
    
    tlm_extension* clone() const override {
        return new ErrorContextExt(*this);
    }
    
    void copy_from(const tlm_extension_base& ext) override {
        auto& e = static_cast<const ErrorContextExt&>(ext);
        error_code = e.error_code;
        error_message = e.error_message;
        // ...
    }
    
    void add_stack_frame(const std::string& module, const std::string& function, const std::string& context = "") {
        stack_trace.push_back({module, function, context});
    }
    
    void set_context(const std::string& key, uint64_t value) {
        context_data[key] = value;
    }
};

#endif // ERROR_CONTEXT_EXT_HH
```

**验收标准**:
- [ ] Extension clone/copy 正常
- [ ] 堆栈追踪正常

**预计**: 3 小时

---

#### 任务 4.2: DebugTracker

**文件**: `include/framework/debug_tracker.hh`（新建）

**内容**:
```cpp
#ifndef DEBUG_TRACKER_HH
#define DEBUG_TRACKER_HH

#include "error_category.hh"
#include <map>
#include <vector>
#include <string>

struct ErrorRecord {
    uint64_t error_id;
    uint64_t timestamp;
    uint64_t transaction_id;
    ErrorCode error_code;
    ErrorCategory error_category;
    std::string source_module;
    std::string error_message;
    std::vector<std::string> stack_trace;
};

class DebugTracker {
private:
    std::map<uint64_t, ErrorRecord> errors_;
    uint64_t next_error_id_ = 1;
    
    DebugTracker() = default;
    
public:
    static DebugTracker& instance() {
        static DebugTracker tracker;
        return tracker;
    }
    
    uint64_t record_error(tlm_generic_payload* payload,
                          ErrorCode code,
                          const std::string& message,
                          const std::string& module) {
        ErrorRecord record;
        record.error_id = next_error_id_++;
        record.timestamp = TransactionTracker::instance().get_current_time();
        record.error_code = code;
        record.error_category = get_error_category(code);
        record.source_module = module;
        record.error_message = message;
        
        errors_[record.error_id] = record;
        return record.error_id;
    }
    
    const ErrorRecord* get_error(uint64_t error_id) const {
        auto it = errors_.find(error_id);
        return (it != errors_.end()) ? &it->second : nullptr;
    }
    
    std::vector<ErrorRecord> get_errors_by_category(ErrorCategory cat) const {
        std::vector<ErrorRecord> result;
        for (const auto& [id, record] : errors_) {
            if (record.error_category == cat) {
                result.push_back(record);
            }
        }
        return result;
    }
    
    size_t error_count() const { return errors_.size(); }
    
    size_t error_count(ErrorCategory cat) const {
        return get_errors_by_category(cat).size();
    }
};

#endif // DEBUG_TRACKER_HH
```

**验收标准**:
- [ ] 错误记录正常
- [ ] 查询接口正确
- [ ] 统计接口准确

**预计**: 4 小时

---

### Phase 5: 模块升级（2 天）

#### 任务 5.1: CacheV2 升级

**现有**: `include/modules/cache_sim.hh`

**变更**:
```cpp
// include/modules/cache_v2.hh
#ifndef CACHE_V2_HH
#define CACHE_V2_HH

#include "../core/sim_object.hh"
#include "../framework/transaction_tracker.hh"
#include "../framework/debug_tracker.hh"
#include <queue>
#include <map>

class CacheV2 : public SimObject {
private:
    std::queue<Packet*> req_buffer_;
    std::map<uint64_t, uint64_t> cache_lines_;
    static const int MAX_BUFFER = 4;
    
public:
    CacheV2(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    
    void tick() override {
        tryForward();
    }
    
    // 复位实现
    void do_reset(const ResetConfig& config) override {
        cache_lines_.clear();
        while (!req_buffer_.empty()) {
            PacketPool::get().release(req_buffer_.front());
            req_buffer_.pop();
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) override {
        uint64_t addr = pkt->payload->get_address();
        bool hit = (addr & 0x7) == 0;
        
        if (hit) {
            // Cache Hit
            Packet* resp = PacketPool::get().acquire();
            resp->payload = pkt->payload;
            resp->type = PKT_RESP;
            resp->original_req = pkt;
            resp->set_transaction_id(pkt->get_transaction_id());
            
            event_queue->schedule([this, resp, src_id]() {
                getPortManager().getUpstreamPorts()[src_id]->sendResp(resp);
            }, 1);
            
            PacketPool::get().release(pkt);
            
            // 记录追踪
            TransactionTracker::instance().record_hop(
                pkt->get_transaction_id(), name, 1);
        } else {
            // Cache Miss
            req_buffer_.push(pkt);
            scheduleForward(1);
        }
        return true;
    }
    
private:
    bool tryForward() {
        if (req_buffer_.empty()) return true;
        Packet* pkt = req_buffer_.front();
        // ... 转发逻辑
        return true;
    }
    
    void scheduleForward(int delay) {
        event_queue->schedule([this]() { tryForward(); }, delay);
    }
};

#endif // CACHE_V2_HH
```

**文件变更**:
- `include/modules/cache_v2.hh` - 新建（基于 cache_sim.hh 升级）

**验收标准**:
- [ ] 复位后状态清零
- [ ] 交易追踪正常
- [ ] 向后兼容

**预计**: 4 小时

---

#### 任务 5.2: CrossbarV2 升级

**类似 CacheV2，基于 crossbar.hh 升级**

**预计**: 3 小时

---

#### 任务 5.3: MemoryV2 升级

**类似 CacheV2，基于 memory_sim.hh 升级**

**预计**: 3 小时

---

### Phase 6: 测试与示例（2 天）

#### 任务 6.1: 单元测试

**现有测试**: 17 个测试文件（已使用 Catch2）

**新增测试**:
- `test_transaction_tracker.cc` - TransactionTracker 测试
- `test_debug_tracker.cc` - DebugTracker 测试
- `test_error_category.cc` - ErrorCode 测试
- `test_reset_system.cc` - 复位系统测试

**验收标准**:
- [ ] 所有测试通过
- [ ] 新增测试覆盖核心功能

**预计**: 4 小时

---

#### 任务 6.2: 示例程序

**现有**: `samples/simple1/`

**新增**:
- `examples/cache_system/` - Cache 系统示例
- `examples/noc_system/` - NoC 系统示例

**验收标准**:
- [ ] 示例可编译运行
- [ ] 输出正确结果

**预计**: 3 小时

---

## 3. 实施优先级与工期

| 阶段 | 任务数 | 工期 | 优先级 |
|------|--------|------|--------|
| **Phase 1: 构建系统** | 3 | 1 天 | P0 |
| **Phase 2: 核心基础** | 3 | 2 天 | P0 |
| **Phase 3: 交易处理** | 2 | 2 天 | P0 |
| **Phase 4: 错误处理** | 2 | 2 天 | P0 |
| **Phase 5: 模块升级** | 3 | 2 天 | P1 |
| **Phase 6: 测试示例** | 2 | 2 天 | P1 |
| **总计** | **15** | **11 天** | - |

---

## 4. 关键文件清单

### 新建文件（11 个）

| 文件 | 阶段 | 内容 |
|------|------|------|
| `scripts/build.sh` | Phase 1 | 构建脚本 |
| `scripts/test.sh` | Phase 1 | 测试脚本 |
| `scripts/format.sh` | Phase 1 | 格式化脚本 |
| `.github/workflows/ci.yml` | Phase 1 | CI 配置 |
| `include/framework/error_category.hh` | Phase 2 | 错误码定义 |
| `include/ext/transaction_context_ext.hh` | Phase 3 | TransactionContextExt |
| `include/framework/transaction_tracker.hh` | Phase 3 | TransactionTracker |
| `include/ext/error_context_ext.hh` | Phase 4 | ErrorContextExt |
| `include/framework/debug_tracker.hh` | Phase 4 | DebugTracker |
| `include/modules/cache_v2.hh` | Phase 5 | CacheV2 模块 |
| `include/modules/crossbar_v2.hh` | Phase 5 | CrossbarV2 模块 |

### 修改文件（6 个）

| 文件 | 阶段 | 变更 |
|------|------|------|
| `CMakeLists.txt` | Phase 1 | 重构为 CppTLM |
| `include/core/sim_object.hh` | Phase 2 | 扩展 reset/snapshot |
| `include/core/packet.hh` | Phase 2 | 扩展 transaction_id/error_code |
| `include/modules/cache_sim.hh` | Phase 5 | 升级为 CacheV2 |
| `include/modules/crossbar.hh` | Phase 5 | 升级为 CrossbarV2 |
| `include/modules/memory_sim.hh` | Phase 5 | 升级为 MemoryV2 |

---

## 5. 风险与缓解

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| **现有代码兼容** | 高 | 中 | 逐步迁移，保留旧接口 |
| **Extension 机制复杂** | 中 | 中 | 提前原型验证 |
| **Tracker 单例线程安全** | 中 | 低 | v2.0 单线程，v2.1 加锁 |
| **工期延误** | 中 | 中 | 优先 P0 阶段，P1 可延后 |

---

## 6. 验收标准

### v2.0 完成标准

- [ ] 所有 P0 阶段完成
- [ ] 构建系统正常工作（cmake + ninja + ccache）
- [ ] SimObject 支持层次化复位
- [ ] Packet 支持 transaction_id/error_code
- [ ] TransactionContextExt 正常工作
- [ ] TransactionTracker 正常工作
- [ ] ErrorContextExt 正常工作
- [ ] DebugTracker 正常工作
- [ ] CacheV2/CrossbarV2/MemoryV2 升级完成
- [ ] 所有单元测试通过
- [ ] 示例程序可运行

---

**下一步**: 请老板审阅详细实施计划，确认是否开始实施。
