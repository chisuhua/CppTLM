# Phase 2: 核心基础扩展 - 详细实施计划

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **工期**: 2 天

---

## 1. 现有代码分析

### 1.1 SimObject（现有）

**文件**: `include/core/sim_object.hh`

**现有结构**:
```cpp
class SimObject {
protected:
    std::string name;
    EventQueue* event_queue;
    std::unique_ptr<PortManager> port_manager;
    LayoutInfo layout;

public:
    SimObject(const std::string& n, EventQueue* eq);
    virtual void tick() = 0;
    
    // 现有方法
    const std::string& getName() const;
    EventQueue* getEventQueue() const;
    PortManager& getPortManager();
    uint64_t getCurrentCycle() const;
    
    virtual bool handleDownstreamResponse(Packet* pkt, int src_id, const std::string& src_label);
    virtual bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label);
};
```

**需要扩展**:
- [ ] 层次化管理（parent_/children_）
- [ ] 复位接口（reset()/do_reset()）
- [ ] 快照接口（save_snapshot()/load_snapshot()）
- [ ] 初始化标志（initialized_）

---

### 1.2 Packet（现有）

**文件**: `include/core/packet.hh`

**现有结构**:
```cpp
class Packet {
public:
    tlm::tlm_generic_payload* payload;
    uint64_t stream_id = 0;  // ← 可作为 transaction_id 基础
    uint64_t seq_num = 0;
    CmdType cmd;
    PacketType type;
    
    uint64_t src_cycle;
    uint64_t dst_cycle;
    
    Packet* original_req = nullptr;
    std::vector<Packet*> dependents;
    
    std::vector<std::string> route_path;
    int hop_count = 0;
    uint8_t priority = 0;
    uint64_t flow_id = 0;
    int vc_id = 0;
};
```

**需要扩展**:
- [ ] 错误码字段（error_code_）
- [ ] 错误消息字段（error_message_）
- [ ] get/set_transaction_id() 方法
- [ ] get/set_error_code() 方法
- [ ] has_error() 方法

---

## 2. 详细实施任务

### 任务 2.1: SimObject 扩展（4 小时）

#### 2.1.1 新增结构定义

**文件**: `include/core/sim_object.hh`

**新增内容**:
```cpp
// ========== ResetConfig 结构 ==========
struct ResetConfig {
    bool hierarchical = true;       // 层次化复位
    bool save_snapshot = false;     // 复位前保存快照
    bool preserve_errors = true;    // 保留错误记录
    bool preserve_transactions = false;  // 保留交易记录
    std::string snapshot_path = ""; // 快照保存路径
};
```

#### 2.1.2 SimObject 类扩展

**变更**:
```cpp
class SimObject {
protected:
    // ===== 现有字段 =====
    std::string name;
    EventQueue* event_queue;
    std::unique_ptr<PortManager> port_manager;
    LayoutInfo layout;
    
    // ===== 新增字段 =====
    SimObject* parent_ = nullptr;                    // 父模块
    std::vector<SimObject*> children_;               // 子模块列表
    bool initialized_ = false;                       // 初始化标志
    bool reset_pending_ = false;                     // 复位等待标志

public:
    SimObject(const std::string& n, EventQueue* eq);
    virtual ~SimObject() = default;

    // ===== 生命周期接口 =====
    
    // 初始化（仿真前调用一次）
    virtual void init() {
        initialized_ = true;
    }
    
    // 核心 tick 接口
    virtual void tick() = 0;

    // ===== 复位接口 =====
    
    // 复位方法（默认层次化复位）
    virtual void reset(const ResetConfig& config = ResetConfig()) {
        if (config.hierarchical) {
            // 先复位所有子模块
            for (auto* child : children_) {
                child->reset(config);
            }
        }
        do_reset(config);
    }
    
    // 执行复位（子类重写）
    virtual void do_reset(const ResetConfig& config) {
        // 默认实现：清除状态标志
        reset_pending_ = false;
    }

    // ===== 快照接口（v2.1 实现，预留） =====
    
    virtual void save_snapshot(std::ostream& os) {
        os << "Module: " << name << "\n";
        os << "Initialized: " << initialized_ << "\n";
    }
    
    virtual void load_snapshot(std::istream& is) {
        // 默认空实现
    }

    // ===== 层次化管理 =====
    
    void set_parent(SimObject* parent) {
        parent_ = parent;
    }
    
    void add_child(SimObject* child) {
        children_.push_back(child);
        child->set_parent(this);
    }
    
    SimObject* get_parent() const { return parent_; }
    
    const std::vector<SimObject*>& get_children() const {
        return children_;
    }

    // ===== 状态查询 =====
    
    bool is_initialized() const { return initialized_; }
    bool is_reset_pending() const { return reset_pending_; }
    
    const std::string& getName() const { return name; }
    EventQueue* getEventQueue() const { return event_queue; }
    
    // ===== 便捷方法 =====
    
    void request_reset() {
        reset_pending_ = true;
    }

    // ===== 现有方法（保持不变） =====
    PortManager& getPortManager();
    bool hasPortManager() const;
    void setLayout(double x, double y);
    const LayoutInfo& getLayout() const;
    uint64_t getCurrentCycle() const;
    
    virtual bool handleDownstreamResponse(Packet* pkt, int src_id, const std::string& src_label);
    virtual bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label);
};
```

**验收标准**:
- [ ] 编译通过，无警告
- [ ] 层次化复位正常工作
- [ ] 子类可重写 do_reset()
- [ ] 向后兼容现有代码

---

### 任务 2.2: ErrorCode 定义（2 小时）

**文件**: `include/framework/error_category.hh`（新建）

**内容**:
```cpp
// include/framework/error_category.hh
#ifndef ERROR_CATEGORY_HH
#define ERROR_CATEGORY_HH

#include <cstdint>
#include <string>

// ========== 错误类别枚举 ==========
enum class ErrorCategory : uint8_t {
    TRANSPORT   = 0x01,  // 传输层错误
    RESOURCE    = 0x02,  // 资源层错误
    COHERENCE   = 0x03,  // 一致性层错误
    PROTOCOL    = 0x04,  // 协议层错误
    SECURITY    = 0x05,  // 安全层错误
    PERFORMANCE = 0x06,  // 性能层错误
    CUSTOM      = 0x10,  // 用户自定义
};

// ========== 错误码枚举（按类别组织） ==========
enum class ErrorCode : uint16_t {
    // 成功
    SUCCESS = 0x0000,
    RETRY   = 0x0001,
    NACK    = 0x0002,
    
    // 传输层错误 (0x01xx)
    TRANSPORT_INVALID_ADDRESS   = 0x0100,
    TRANSPORT_ACCESS_DENIED     = 0x0101,
    TRANSPORT_MEMORY_NOT_MAPPED = 0x0102,
    TRANSPORT_ALIGNMENT_FAULT   = 0x0103,
    TRANSPORT_TIMEOUT           = 0x0104,
    TRANSPORT_PROTOCOL_VIOLATION = 0x0105,
    
    // 资源层错误 (0x02xx)
    RESOURCE_BUFFER_FULL        = 0x0200,
    RESOURCE_QUEUE_OVERFLOW     = 0x0201,
    RESOURCE_CONFLICT           = 0x0202,
    RESOURCE_STARVATION         = 0x0203,
    
    // 一致性层错误 (0x03xx)
    COHERENCE_STATE_VIOLATION   = 0x0300,
    COHERENCE_DEADLOCK          = 0x0301,
    COHERENCE_LIVELOCK          = 0x0302,
    COHERENCE_DATA_INCONSISTENCY = 0x0303,
    COHERENCE_INVALID_TRANSITION = 0x0304,
    COHERENCE_SNOOP_CONFLICT    = 0x0305,
    COHERENCE_DIRECTORY_OVERFLOW = 0x0306,
    COHERENCE_SHARERS_LIMIT     = 0x0307,
    
    // 协议层错误 (0x04xx)
    PROTOCOL_ID_CONFLICT        = 0x0400,
    PROTOCOL_OUT_OF_ORDER       = 0x0401,
    PROTOCOL_SEQUENCE_ERROR     = 0x0402,
    PROTOCOL_INVALID_COMMAND    = 0x0403,
    
    // 安全层错误 (0x05xx)
    SECURITY_PERMISSION_DENIED  = 0x0500,
    SECURITY_ENCRYPTION_ERROR   = 0x0501,
    SECURITY_AUTHENTICATION_FAILED = 0x0502,
    
    // 性能层错误 (0x06xx)
    PERFORMANCE_THRESHOLD_EXCEEDED = 0x0600,
    PERFORMANCE_HOTSPOT_DETECTED   = 0x0601,
};

// ========== 工具函数 ==========

inline ErrorCategory get_error_category(ErrorCode code) {
    uint16_t v = static_cast<uint16_t>(code);
    if (v <= 0x0002) return ErrorCategory::TRANSPORT;  // SUCCESS/RETRY/NACK
    return static_cast<ErrorCategory>((v >> 8) & 0xFF);
}

inline std::string error_category_to_string(ErrorCategory cat) {
    switch (cat) {
        case ErrorCategory::TRANSPORT: return "TRANSPORT";
        case ErrorCategory::RESOURCE: return "RESOURCE";
        case ErrorCategory::COHERENCE: return "COHERENCE";
        case ErrorCategory::PROTOCOL: return "PROTOCOL";
        case ErrorCategory::SECURITY: return "SECURITY";
        case ErrorCategory::PERFORMANCE: return "PERFORMANCE";
        case ErrorCategory::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

inline std::string error_code_to_string(ErrorCode code) {
    uint16_t v = static_cast<uint16_t>(code);
    std::string cat = error_category_to_string(get_error_category(code));
    return cat + "_" + std::to_string(v & 0xFF);
}

inline bool is_error(ErrorCode code) {
    return code != ErrorCode::SUCCESS && code != ErrorCode::RETRY && code != ErrorCode::NACK;
}

inline bool is_fatal_error(ErrorCode code) {
    return code == ErrorCode::COHERENCE_DEADLOCK ||
           code == ErrorCode::COHERENCE_LIVELOCK ||
           code == ErrorCode::COHERENCE_DATA_INCONSISTENCY;
}

#endif // ERROR_CATEGORY_HH
```

**验收标准**:
- [ ] 编译通过
- [ ] 工具函数正确
- [ ] 错误码分类清晰

---

### 任务 2.3: Packet 扩展（3 小时）

#### 2.3.1 新增字段和方法

**文件**: `include/core/packet.hh`

**变更**:
```cpp
// 在文件开头添加
#include "../framework/error_category.hh"

class Packet {
    friend class PacketPool;
public:
    // ===== 现有字段（保持不变） =====
    tlm::tlm_generic_payload* payload;
    uint64_t stream_id = 0;
    uint64_t seq_num = 0;
    CmdType cmd;
    PacketType type;

    uint64_t src_cycle;
    uint64_t dst_cycle;

    Packet* original_req = nullptr;
    std::vector<Packet*> dependents;

    std::vector<std::string> route_path;
    int hop_count = 0;
    uint8_t priority = 0;
    uint64_t flow_id = 0;
    int vc_id = 0;

    // ===== 新增字段 =====
    ErrorCode error_code_ = ErrorCode::SUCCESS;
    std::string error_message_;

    // ===== 现有方法（保持不变） =====
    bool isRequest() const { return type == PKT_REQ; }
    bool isResponse() const { return type == PKT_RESP; }
    bool isStream() const { return type == PKT_STREAM_DATA; }
    bool isCredit() const { return type == PKT_CREDIT_RETURN; }

    uint64_t getDelayCycles() const {
        return (dst_cycle >= src_cycle) ? (dst_cycle - src_cycle) : 0;
    }

    uint64_t getEnd2EndCycles() const {
        return original_req ? (dst_cycle - original_req->src_cycle) : getDelayCycles();
    }

    // ===== 新增方法 =====
    
    // Transaction ID 管理
    uint64_t get_transaction_id() const {
        // v2.0: 直接返回 stream_id
        // v2.1: 可从 TransactionContextExt 获取
        return stream_id;
    }
    
    void set_transaction_id(uint64_t tid) {
        stream_id = tid;
        // v2.1: 同步到 TransactionContextExt
    }
    
    // Error Code 管理
    ErrorCode get_error_code() const {
        return error_code_;
    }
    
    void set_error_code(ErrorCode code, const std::string& msg = "") {
        error_code_ = code;
        if (!msg.empty()) {
            error_message_ = msg;
        }
    }
    
    bool has_error() const {
        return is_error(error_code_);
    }
    
    std::string get_error_message() const {
        if (!error_message_.empty()) {
            return error_message_;
        }
        return error_code_to_string(error_code_);
    }
    
    // 添加追踪日志（v2.1 实现，预留接口）
    void add_trace(const std::string& module, uint64_t latency, const std::string& event = "") {
        // v2.1: 添加到 TransactionContextExt 或 DebugTraceExt
        (void)module; (void)latency; (void)event;  // 避免未使用警告
    }

private:
    int ref_count = 0;

    void reset() {
        if (payload && !isCredit()) {
            delete payload;
        }
        payload = nullptr;
        stream_id = 0;
        seq_num = 0;
        cmd = CMD_INVALID;
        type = PKT_REQ;
        src_cycle = 0;
        dst_cycle = 0;
        original_req = nullptr;
        dependents.clear();
        route_path.clear();
        hop_count = 0;
        priority = 0;
        flow_id = 0;
        vc_id = 0;
        ref_count = 0;
        
        // 重置错误状态
        error_code_ = ErrorCode::SUCCESS;
        error_message_.clear();
    }

    Packet(tlm::tlm_generic_payload* p, uint64_t cycle, PacketType t)
        : payload(p), src_cycle(cycle), type(t), ref_count(0) {}

    ~Packet() {
        // PacketPool 负责清理
    }
    
    friend class PacketPool;
};
```

**验收标准**:
- [ ] 编译通过，无警告
- [ ] get/set_transaction_id() 正常工作
- [ ] get/set_error_code() 正常工作
- [ ] has_error() 正确判断
- [ ] 向后兼容现有代码

---

## 3. 文件变更清单

### 新建文件（1 个）

| 文件 | 内容 | 预计大小 |
|------|------|---------|
| `include/framework/error_category.hh` | ErrorCode 定义 | ~100 行 |

### 修改文件（2 个）

| 文件 | 变更内容 | 变更量 |
|------|---------|--------|
| `include/core/sim_object.hh` | 添加 ResetConfig、层次化管理、复位/快照接口 | +60 行 |
| `include/core/packet.hh` | 添加 error_code_、transaction_id 方法 | +40 行 |

---

## 4. 实施步骤

### 步骤 1: 创建 error_category.hh（30 分钟）

```bash
# 创建目录
mkdir -p include/framework

# 创建文件
touch include/framework/error_category.hh
```

**验收**: 编译通过

---

### 步骤 2: 扩展 sim_object.hh（2 小时）

**顺序**:
1. 添加 `#include <vector>` 和 `#include <iosfwd>`
2. 添加 `ResetConfig` 结构
3. 添加保护成员：`parent_`, `children_`, `initialized_`, `reset_pending_`
4. 添加 `init()` 方法
5. 添加 `reset()` / `do_reset()` 方法
6. 添加 `save_snapshot()` / `load_snapshot()` 方法
7. 添加层次化管理方法
8. 添加状态查询方法

**验收**: 编译通过，现有模块无需修改即可编译

---

### 步骤 3: 扩展 packet.hh（1.5 小时）

**顺序**:
1. 添加 `#include "../framework/error_category.hh"`
2. 添加 `error_code_` 和 `error_message_` 字段
3. 添加 `get/set_transaction_id()` 方法
4. 添加 `get/set_error_code()` 方法
5. 添加 `has_error()` 方法
6. 添加 `get_error_message()` 方法
7. 添加 `add_trace()` 方法（预留）
8. 更新 `reset()` 方法，清除错误状态

**验收**: 编译通过，现有测试通过

---

### 步骤 4: 验证向后兼容性（1 小时）

```bash
# 编译项目
cd build
cmake ..
make -j$(nproc)

# 运行现有测试
ctest --output-on-failure
```

**验收**:
- [ ] 所有现有测试通过
- [ ] 无编译警告
- [ ] 无运行时错误

---

## 5. 验收标准

### 编译验收
- [ ] `cmake -S . -B build -G Ninja` 成功
- [ ] `cmake --build build` 成功
- [ ] 无编译警告（-Wall -Wextra -Wpedantic）

### 功能验收
- [ ] SimObject 层次化复位正常工作
- [ ] Packet get/set_transaction_id() 正常
- [ ] Packet get/set_error_code() 正常
- [ ] ErrorCode 工具函数正确

### 兼容性验收
- [ ] 现有 17 个测试全部通过
- [ ] 现有模块无需修改即可编译
- [ ] 现有示例程序可运行

---

## 6. 风险与缓解

| 风险 | 影响 | 概率 | 缓解措施 |
|------|------|------|---------|
| **向后兼容破坏** | 高 | 中 | 渐进式修改，保留所有现有接口 |
| **编译错误** | 中 | 低 | 逐步验证，每步编译检查 |
| **测试失败** | 中 | 低 | 现有测试不应受影响 |

---

## 7. 预计工期

| 任务 | 预计时间 | 实际时间 |
|------|---------|---------|
| 任务 2.1: SimObject 扩展 | 4 小时 | - |
| 任务 2.2: ErrorCode 定义 | 2 小时 | - |
| 任务 2.3: Packet 扩展 | 3 小时 | - |
| 步骤 4: 验证兼容性 | 1 小时 | - |
| **总计** | **10 小时** | - |

---

**下一步**: 请老板确认 Phase 2 实施计划，确认后开始实施。
