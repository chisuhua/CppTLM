# ADR-X.3: 复位策略

> **版本**: 2.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **影响**: v2.0 - 模块初始化和状态管理

---

## 1. 核心问题

在混合仿真系统中，复位机制需要解决以下问题：

1. **复位粒度**: 模块级复位 / 系统级复位 / 层次化复位？
2. **复位时机**: 仿真开始前 / 异常后恢复 / 手动触发？
3. **状态保存**: 是否需要快照功能（用于调试回放）？
4. **TLM vs RTL**: 两种模块的复位语义是否一致？
5. **与交易/错误系统整合**: 复位时如何处理活跃交易和错误记录？

---

## 2. 行业调研

### 2.1 SystemC 复位机制

```cpp
// SystemC 标准复位
sc_module {
    void reset();  // 模块复位方法
    
    SC_HAS_PROCESS(ModuleName);
    ModuleName(...) {
        SC_METHOD(reset);
        sensitive << reset_signal;
    }
};
```

**特点**:
- 信号驱动复位
- 同步/异步复位可选
- 层次化复位（父模块复位触发子模块复位）

---

### 2.2 Gem5 复位机制

```cpp
// Gem5 SimObject 复位
class SimObject : public ClockedObject {
    virtual void init();      // 初始化（仿真前调用一次）
    virtual void reset();     // 复位（可多次调用）
    virtual void startup();   // 启动（复位后）
};

// 复位流程
SimObject::reset() {
    // 1. 清除状态
    // 2. 重置寄存器
    // 3. 通知子对象
    for (auto* child : children) {
        child->reset();
    }
}
```

**特点**:
- init/reset/startup 三阶段
- 层次化复位
- 支持检查点恢复

---

### 2.3 TLM-2.0 复位

```cpp
// TLM 模块复位
class tlm_module {
    void reset() {
        // 重置内部状态
        // 清除 pending 事务
        // 重置统计计数器
    }
};
```

**特点**:
- 简单复位方法
- 无标准层次化机制
- 用户自行实现

---

## 3. 方案对比

| 方案 | 设计 | 优点 | 缺点 | 适用场景 |
|------|------|------|------|---------|
| **A) 无复位** | 仅构造时初始化 | 最简单 | 无法重置状态 | 一次性仿真 |
| **B) 简单 reset()** | 虚方法，子类重写 | 简单，灵活 | 无层次化 | 小型系统 |
| **C) 层次化复位** ✅ | 父模块触发子模块复位 | 系统化，完整 | 复杂度增加 | ✅ 推荐 |
| **D) 检查点复位** | 保存/恢复完整状态 | 支持回放调试 | 内存开销大 | 高级调试 |
| **E) 混合复位** | 层次化 + 可选检查点 | 平衡灵活性与功能 | 实现复杂 | ✅ 推荐（增强版） |

---

## 4. 推荐方案：层次化复位 + 可选快照

### 4.1 核心设计

```
┌─────────────────────────────────────────────────────────────┐
│  应用层：模块复位                                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  SimObject::reset()                                  │   │
│  │  - 清除内部状态                                      │   │
│  │  - 通知子模块                                        │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  框架层：复位协调                                           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  ResetCoordinator (单例)                             │   │
│  │  - 系统级复位                                        │   │
│  │  - 快照管理（可选）                                  │   │
│  │  - 与 DebugTracker 整合                              │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  扩展层：交易/错误系统整合                                  │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  - 清除活跃交易                                      │   │
│  │  - 可选保留错误记录（用于调试）                      │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

### 4.2 SimObject 基类扩展

```cpp
// include/core/sim_object.hh (修订版)
#ifndef SIM_OBJECT_HH
#define SIM_OBJECT_HH

#include "packet.hh"
#include "../ext/transaction_context_ext.hh"
#include <vector>
#include <string>

// 复位阶段枚举
enum class ResetPhase {
    PRE_RESET,      // 复位前（保存快照）
    DO_RESET,       // 执行复位
    POST_RESET      // 复位后（恢复配置）
};

// 复位配置
struct ResetConfig {
    bool hierarchical = true;       // 层次化复位
    bool save_snapshot = false;     // 保存快照
    bool preserve_errors = true;    // 保留错误记录
    bool preserve_transactions = false;  // 保留交易记录
    std::string snapshot_path = ""; // 快照路径
};

class SimObject {
protected:
    std::string name_;
    EventQueue* event_queue;
    SimObject* parent_ = nullptr;
    std::vector<SimObject*> children_;
    
    // 状态标志
    bool initialized_ = false;
    bool reset_pending_ = false;
    
public:
    SimObject(const std::string& n, EventQueue* eq) 
        : name_(n), event_queue(eq), initialized_(false) {}
    
    virtual ~SimObject() = default;
    
    // ========== 生命周期接口 ==========
    
    // 初始化（仿真前调用一次）
    virtual void init() {
        initialized_ = true;
        // 子类可扩展
    }
    
    // 核心接口
    virtual void tick() = 0;
    
    // ========== 复位接口（可选重写） ==========
    
    // 复位方法（默认实现层次化复位）
    virtual void reset(const ResetConfig& config = ResetConfig()) {
        if (config.hierarchical) {
            // 层次化复位：先复位子模块
            for (auto* child : children_) {
                child->reset(config);
            }
        }
        
        // 执行本模块复位
        do_reset(config);
    }
    
    // 执行复位（子类重写）
    virtual void do_reset(const ResetConfig& config) {
        // 默认：清除状态标志
        reset_pending_ = false;
        
        // 子类应重写此方法清除内部状态
    }
    
    // ========== 快照接口（可选重写） ==========
    
    // 保存快照
    virtual void save_snapshot(std::ostream& os) {
        // 默认实现：保存基本信息
        os << "Module: " << name_ << "\n";
        os << "Initialized: " << initialized_ << "\n";
        // 子类可扩展
    }
    
    // 恢复快照
    virtual void load_snapshot(std::istream& is) {
        // 默认实现：恢复基本信息
        // 子类可扩展
    }
    
    // ========== 层次化管理 ==========
    
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
    
    // ========== 状态查询 ==========
    
    bool is_initialized() const { return initialized_; }
    bool is_reset_pending() const { return reset_pending_; }
    
    const std::string& name() const { return name_; }
    
    // ========== 便捷方法 ==========
    
    // 请求复位（异步）
    void request_reset() {
        reset_pending_ = true;
    }
    
    // 检查是否需要复位
    bool needs_reset() const {
        return reset_pending_;
    }
};

#endif // SIM_OBJECT_HH
```

---

### 4.3 ResetCoordinator（框架层）

```cpp
// include/framework/reset_coordinator.hh
#ifndef RESET_COORDINATOR_HH
#define RESET_COORDINATOR_HH

#include "../core/sim_object.hh"
#include "debug_tracker.hh"
#include "transaction_tracker.hh"
#include <vector>
#include <sstream>
#include <fstream>

// 复位协调器（框架层单例）
class ResetCoordinator {
private:
    std::vector<SimObject*> root_modules_;
    ResetConfig default_config_;
    
    // 快照管理
    std::map<std::string, std::string> snapshots_;  // name -> snapshot_data
    
    ResetCoordinator() = default;
    
public:
    static ResetCoordinator& instance() {
        static ResetCoordinator coordinator;
        return coordinator;
    }
    
    // ========== 初始化 ==========
    
    void register_root_module(SimObject* module) {
        root_modules_.push_back(module);
    }
    
    void set_default_config(const ResetConfig& config) {
        default_config_ = config;
    }
    
    // ========== 系统级复位 ==========
    
    // 全系统复位
    void system_reset(const ResetConfig& config = ResetConfig()) {
        ResetConfig cfg = config;
        if (config.save_snapshot) {
            // 保存快照
            save_system_snapshot(config.snapshot_path);
        }
        
        // 通知 DebugTracker
        if (!config.preserve_errors) {
            DebugTracker::instance().clear_all();
        }
        
        // 通知 TransactionTracker
        if (!config.preserve_transactions) {
            TransactionTracker::instance().clear_all();
        }
        
        // 层次化复位所有根模块
        for (auto* module : root_modules_) {
            module->reset(cfg);
        }
    }
    
    // 模块级复位
    void module_reset(SimObject* module, const ResetConfig& config = ResetConfig()) {
        module->reset(config);
    }
    
    // 异步复位请求
    void request_system_reset() {
        for (auto* module : root_modules_) {
            module->request_reset();
        }
    }
    
    // ========== 快照管理 ==========
    
    // 保存系统快照
    void save_system_snapshot(const std::string& path) {
        std::ostringstream oss;
        
        oss << "=== System Snapshot ===\n";
        oss << "Timestamp: " << TransactionTracker::instance().get_current_time() << "\n";
        oss << "\n";
        
        // 保存所有模块状态
        for (auto* module : root_modules_) {
            save_module_snapshot_recursive(module, oss);
        }
        
        // 保存到文件
        if (!path.empty()) {
            std::ofstream file(path);
            file << oss.str();
        } else {
            // 保存到内存
            snapshots_["system"] = oss.str();
        }
    }
    
    // 保存单个模块快照
    void save_module_snapshot(SimObject* module, const std::string& name = "") {
        std::ostringstream oss;
        module->save_snapshot(oss);
        
        std::string snapshot_name = name.empty() ? module->name() : name;
        snapshots_[snapshot_name] = oss.str();
    }
    
    // 恢复系统快照
    void restore_system_snapshot(const std::string& path = "") {
        std::string snapshot_data;
        
        if (!path.empty()) {
            // 从文件读取
            std::ifstream file(path);
            snapshot_data = std::string(
                std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>());
        } else {
            // 从内存读取
            snapshot_data = snapshots_["system"];
        }
        
        // 解析并恢复（简化实现）
        // ...
    }
    
    // 恢复模块快照
    void restore_module_snapshot(SimObject* module, const std::string& name = "") {
        std::string snapshot_name = name.empty() ? module->name() : name;
        
        if (snapshots_.count(snapshot_name)) {
            std::istringstream iss(snapshots_[snapshot_name]);
            module->load_snapshot(iss);
        }
    }
    
    // ========== 查询接口 ==========
    
    const std::map<std::string, std::string>& get_snapshots() const {
        return snapshots_;
    }
    
    bool has_snapshot(const std::string& name) const {
        return snapshots_.count(name) > 0;
    }
    
private:
    void save_module_snapshot_recursive(SimObject* module, std::ostream& os) {
        os << "--- Module: " << module->name() << " ---\n";
        module->save_snapshot(os);
        os << "\n";
        
        // 递归保存子模块
        for (auto* child : module->get_children()) {
            save_module_snapshot_recursive(child, os);
        }
    }
};

#endif // RESET_COORDINATOR_HH
```

---

### 4.4 TLM 模块复位示例

```cpp
// include/modules/cache_v2.hh (复位实现)
class CacheV2 : public TLMModule {
private:
    std::queue<Packet*> req_buffer_;
    std::map<uint64_t, uint64_t> cache_lines_;
    std::map<uint64_t, CoherenceState> cache_states_;
    
public:
    CacheV2(const std::string& n, EventQueue* eq) : TLMModule(n, eq) {}
    
    // ✅ 重写 do_reset：清除内部状态
    void do_reset(const ResetConfig& config) override {
        // 清除请求缓冲
        while (!req_buffer_.empty()) {
            Packet* pkt = req_buffer_.front();
            req_buffer_.pop();
            PacketPool::get().release(pkt);
        }
        
        // 清除缓存行
        cache_lines_.clear();
        
        // 清除一致性状态
        cache_states_.clear();
        
        // 调用基类
        TLMModule::do_reset(config);
    }
    
    // ✅ 重写 save_snapshot：保存状态
    void save_snapshot(std::ostream& os) override {
        os << "Cache Lines: " << cache_lines_.size() << "\n";
        os << "Pending Requests: " << req_buffer_.size() << "\n";
        
        // 保存一致性状态
        os << "Coherence States:\n";
        for (const auto& [addr, state] : cache_states_) {
            os << "  0x" << std::hex << addr << ": " 
               << coherence_state_to_string(state) << "\n";
        }
    }
    
    // ✅ 重写 load_snapshot：恢复状态
    void load_snapshot(std::istream& is) override {
        // 解析快照数据（简化实现）
        std::string line;
        while (std::getline(is, line)) {
            // 解析并恢复状态
            // ...
        }
    }
};
```

---

### 4.5 RTL 模块复位示例

```cpp
// include/core/rtl_module.hh (复位实现)
template<typename ComponentT, typename ReqBundle, typename RespBundle>
class RTLModule : public SimObject {
protected:
    std::unique_ptr<ch::ch_device<ComponentT>> device_;
    
public:
    RTLModule(const std::string& name) : SimObject(name) {
        device_ = std::make_unique<ch::ch_device<ComponentT>>();
    }
    
    // ✅ RTL 模块复位：重置 CppHDL Component
    void do_reset(const ResetConfig& config) override {
        // 重置 Component 内部状态
        device_->reset();
        
        // 清除缓冲
        // ...
    }
    
    // ✅ RTL 模块快照
    void save_snapshot(std::ostream& os) override {
        os << "RTL Component: " << device_->name() << "\n";
        // 保存 Component 状态（如果支持）
    }
};
```

---

### 4.6 与交易/错误系统整合

```cpp
// include/framework/transaction_tracker.hh (扩展)
class TransactionTracker {
public:
    // 清除所有交易
    void clear_all() {
        transactions_.clear();
        parent_child_map_.clear();
    }
    
    // 清除已完成交易（保留活跃交易）
    void clear_completed() {
        for (auto it = transactions_.begin(); it != transactions_.end();) {
            if (it->second.is_complete) {
                it = transactions_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// include/framework/debug_tracker.hh (扩展)
class DebugTracker {
public:
    // 清除所有错误
    void clear_all() {
        errors_.clear();
        state_history_.clear();
        transaction_errors_.clear();
    }
    
    // 保留错误记录（用于调试）
    void preserve_errors() {
        // 导出错误到文件
        export_errors("pre_reset_errors.csv");
    }
    
    // 导出错误
    void export_errors(const std::string& path) {
        std::ofstream file(path);
        file << "error_id,timestamp,transaction_id,error_code,category,module,message\n";
        
        for (const auto& [id, record] : errors_) {
            file << record.error_id << ","
                 << record.timestamp << ","
                 << record.transaction_id << ","
                 << static_cast<int>(record.error_code) << ","
                 << record.error_category << ","
                 << record.source_module << ","
                 << record.error_message << "\n";
        }
    }
};
```

---

## 5. 使用示例

### 5.1 初始化与注册

```cpp
// main.cpp
int sc_main() {
    // 初始化追踪器
    DebugTracker::instance().initialize(true, true, true);
    TransactionTracker::instance().initialize("trace_log.csv");
    
    // 创建模块
    auto* cpu = new CPUSim("cpu", eq);
    auto* cache = new CacheV2("cache", eq);
    auto* memory = new MemoryV2("memory", eq);
    
    // 建立层次关系
    cpu->add_child(cache);
    
    // 注册根模块
    ResetCoordinator::instance().register_root_module(cpu);
    ResetCoordinator::instance().register_root_module(memory);
    
    // 设置默认复位配置
    ResetConfig config;
    config.hierarchical = true;
    config.save_snapshot = true;
    config.preserve_errors = true;
    config.preserve_transactions = false;
    
    ResetCoordinator::instance().set_default_config(config);
    
    // 初始化（调用一次）
    cpu->init();
    memory->init();
    
    // 运行仿真
    eq.run(100000);
    
    return 0;
}
```

### 5.2 系统级复位

```cpp
// 场景 1：异常后恢复
if (fatal_error_detected) {
    // 保存快照并复位
    ResetConfig config;
    config.save_snapshot = true;
    config.snapshot_path = "pre_reset_snapshot.txt";
    config.preserve_errors = true;  // 保留错误用于调试
    
    ResetCoordinator::instance().system_reset(config);
    
    // 恢复后继续仿真
    eq.run(100000);
}

// 场景 2：手动触发复位
ResetCoordinator::instance().system_reset();

// 场景 3：模块级复位
ResetCoordinator::instance().module_reset(cache);
```

### 5.3 快照管理

```cpp
// 保存快照
ResetCoordinator::instance().save_system_snapshot("checkpoint_1000.txt");

// 保存单个模块快照
ResetCoordinator::instance().save_module_snapshot(cache, "cache_before_test");

// 恢复快照
ResetCoordinator::instance().restore_system_snapshot("checkpoint_1000.txt");

// 查询快照
if (ResetCoordinator::instance().has_snapshot("cache_before_test")) {
    // 恢复快照
    ResetCoordinator::instance().restore_module_snapshot(cache, "cache_before_test");
}
```

---

## 6. 需要确认的问题

| 问题 | 选项 | 推荐 |
|------|------|------|
| **Q1**: 复位粒度？ | A) 模块级 / B) 系统级 / C) 层次化 | **C) 层次化** |
| **Q2**: 复位时机？ | A) 仿真前 / B) 异常后 / C) 手动 / D) 全部 | **D) 全部** |
| **Q3**: 状态保存？ | A) 无 / B) 可选快照 | **B) 可选快照** |
| **Q4**: 与交易整合？ | A) 清除所有 / B) 保留活跃 / C) 可配置 | **C) 可配置** |
| **Q5**: 与错误整合？ | A) 清除所有 / B) 保留用于调试 / C) 可配置 | **C) 可配置** |

---

## 7. 与现有架构整合

| 层次 | 复位职责 | 与交易/错误整合 |
|------|---------|---------------|
| **模块层** | `do_reset()` 清除内部状态 | 通知 DebugTracker |
| **框架层** | `ResetCoordinator` 协调复位 | 清除/保留交易和错误记录 |
| **扩展层** | 快照保存/恢复 | 可选保留用于调试 |

---

## 8. 相关文档

| 文档 | 位置 |
|------|------|
| 交易处理架构 | `02-architecture/02-transaction-architecture.md` |
| 错误调试架构 | `02-architecture/03-error-debug-architecture.md` |
| SimObject 基类 | `include/core/sim_object.hh` |

---

**下一步**: 请老板确认复位策略方案细节
