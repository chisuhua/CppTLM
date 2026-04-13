# CppTLM 复位与检查点架构 v1.0

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **关联**: 架构 v2.0, 交易处理架构 v1.0, 错误调试架构 v1.0

---

## 1. 架构愿景

**核心目标**: 提供层次化的复位与检查点机制，支持从轻量级复位到完整检查点的渐进式需求，满足测试迭代、多配置对比、长时间仿真中断恢复等场景。

**设计原则**:
1. **分阶段实施**: v2.0 轻量级 → v2.1 状态快照 → v2.2 完整检查点
2. **层次化设计**: 模块级/系统级/检查点级
3. **JSON 格式**: 人类可读，易于调试
4. **与现有架构整合**: 交易/错误系统状态可保存/恢复

---

## 2. 使用场景

### 2.1 场景全景图

```
┌─────────────────────────────────────────────────────────────┐
│  使用场景（按优先级排序）                                    │
├─────────────────────────────────────────────────────────────┤
│  🔴 场景 1: 测试迭代复位                                      │
│     - 频率：每天数十次                                       │
│     - 需求：快速清除状态，重新初始化                          │
│     - 延迟：<1 秒                                            │
├─────────────────────────────────────────────────────────────┤
│  🔴 场景 2: 多配置对比实验                                    │
│     - 频率：每周数次                                         │
│     - 需求：从同一状态分叉，测试不同配置                       │
│     - 收益：节省 90% 仿真时间                                 │
├─────────────────────────────────────────────────────────────┤
│  🟡 场景 3: 调试特定时间点                                    │
│     - 频率：偶尔                                             │
│     - 需求：保存问题现场，反复调试                            │
│     - 整合：与 DebugTracker 联动                             │
├─────────────────────────────────────────────────────────────┤
│  🟢 场景 4: 长时间仿真中断恢复                                │
│     - 频率：罕见                                             │
│     - 需求：保存完整状态（包括事件队列）                       │
│     - 复杂度：高                                             │
├─────────────────────────────────────────────────────────────┤
│  🟢 场景 5: 异常后恢复                                        │
│     - 频率：罕见                                             │
│     - 需求：自动检测异常，恢复到最近检查点                     │
│     - 整合：与 ErrorTracker 联动                             │
└─────────────────────────────────────────────────────────────┘
```

---

### 2.2 场景详细描述

#### 场景 1: 测试迭代复位（高频）

```cpp
// 使用场景：开发过程中反复测试
class CacheTest {
    void test_cache_hit() {
        // 1. 复位到初始状态
        ResetCoordinator::instance().system_reset();
        
        // 2. 运行测试
        run_simulation(10000);
        
        // 3. 验证结果
        assert(cache->hit_rate() > 0.9);
    }
    
    void test_cache_miss() {
        // 快速复位，重新测试
        ResetCoordinator::instance().system_reset();
        run_simulation(10000);
        assert(cache->miss_rate() > 0.1);
    }
};

// 需求指标:
// - 复位延迟：<1 秒
// - 状态清除：完整
// - 不需要保存历史状态
```

---

#### 场景 2: 多配置对比实验（高频）

```cpp
// 使用场景：对比不同 Cache 配置的性能
void cache_size_sweep() {
    // 1. 仿真到 1M 周期（warmup）
    run_simulation(1000000);
    
    // 2. 保存检查点
    CheckpointManager::instance().save("cpt_warmup_1M.json");
    
    // 3. 多配置对比（从同一检查点分叉）
    std::vector<size_t> cache_sizes = {32*KB, 64*KB, 128*KB};
    
    for (size_t size : cache_sizes) {
        // 恢复检查点（跳过前 1M 周期）
        CheckpointManager::instance().load("cpt_warmup_1M.json");
        
        // 修改配置
        cache->set_size(size);
        
        // 继续仿真到 10M 周期
        run_simulation(9000000);
        
        // 记录结果
        results[size] = cache->get_stats();
    }
}

// 需求指标:
// - 检查点保存时间：<10 秒
// - 检查点恢复时间：<5 秒
// - 节省仿真时间：90%（跳过 warmup）
// - 状态一致性：所有配置从完全相同状态开始
```

---

#### 场景 3: 调试特定时间点（中频）

```cpp
// 使用场景：调试一致性违例
void debug_coherence_violation() {
    // 1. 仿真到问题点
    run_simulation(5000000);
    
    // 2. 检测到违例
    if (DebugTracker::instance().error_count() > 0) {
        // 3. 保存问题现场
        CheckpointManager::instance().save("cpt_violation_5M.json");
        
        // 4. 导出错误详情
        DebugTracker::instance().export_errors("errors_5M.csv");
        
        // 5. 回放交易历史
        auto errors = DebugTracker::instance().get_errors_by_category(
            ErrorCategory::COHERENCE);
        for (const auto& err : errors) {
            std::string replay = DebugTracker::instance().replay_transaction(
                err.transaction_id);
            DPRINTF(DEBUG, "%s\n", replay.c_str());
        }
    }
    
    // 6. 恢复检查点，添加调试日志
    CheckpointManager::instance().load("cpt_violation_5M.json");
    enable_debug_logging();
    run_simulation(100000);  // 单步调试
}

// 需求指标:
// - 与 DebugTracker 整合
// - 支持状态可视化
// - 支持单步执行
```

---

#### 场景 4: 长时间仿真中断恢复（低频）

```cpp
// 使用场景：仿真运行数小时/数天后中断恢复
void long_running_simulation() {
    // 1. 定期保存检查点
    const uint64_t CHECKPOINT_INTERVAL = 10000000;  // 每 10M 周期
    uint64_t next_checkpoint = CHECKPOINT_INTERVAL;
    
    while (current_cycle < TOTAL_CYCLES) {
        run_simulation(1000000);  // 每次运行 1M 周期
        
        // 检查是否需要保存检查点
        if (current_cycle >= next_checkpoint) {
            std::string path = "cpt_" + std::to_string(current_cycle) + "/";
            CheckpointManager::instance().save_full_checkpoint(path);
            next_checkpoint += CHECKPOINT_INTERVAL;
        }
    }
}

// 中断后恢复:
void resume_simulation() {
    // 1. 找到最近检查点
    std::string latest = CheckpointManager::instance().find_latest("cpt_*");
    
    // 2. 恢复完整状态（包括事件队列）
    CheckpointManager::instance().load_full_checkpoint(latest);
    
    // 3. 继续仿真
    long_running_simulation();
}

// 需求指标:
// - 检查点大小：<1GB（可接受）
// - 保存时间：<60 秒
// - 恢复时间：<30 秒
// - 包含事件队列、随机数状态
```

---

#### 场景 5: 异常后恢复（低频）

```cpp
// 使用场景：检测到死锁后自动恢复
void simulation_with_watchdog() {
    // 1. 启用看门狗
    Watchdog::instance().enable(1000000);  // 1M 周期超时
    
    // 2. 定期保存检查点
    CheckpointManager::instance().set_auto_save(true, 10000000);
    
    try {
        run_simulation(TOTAL_CYCLES);
    } catch (DeadlockException& e) {
        // 3. 检测到死锁
        DPRINTF(ERROR, "Deadlock detected at cycle %lu\n", current_cycle);
        
        // 4. 恢复到最近检查点
        std::string latest = CheckpointManager::instance().get_latest();
        CheckpointManager::instance().load(latest);
        
        // 5. 降低并发度，重新尝试
        reduce_concurrency();
        run_simulation(RETRY_CYCLES);
    }
}

// 需求指标:
// - 自动检测异常
// - 自动恢复检查点
// - 与 ErrorTracker 整合
```

---

## 3. 架构设计

### 3.1 层次化架构

```
┌─────────────────────────────────────────────────────────────┐
│  应用层：用户使用场景                                         │
│  - system_reset() / save() / load()                         │
├─────────────────────────────────────────────────────────────┤
│  协调层：CheckpointManager (单例)                            │
│  - 检查点管理                                                │
│  - 快照管理                                                  │
│  - 与 ResetCoordinator 整合                                  │
├─────────────────────────────────────────────────────────────┤
│  模块层：SimObject 接口                                       │
│  - reset() / save_snapshot() / load_snapshot()              │
├─────────────────────────────────────────────────────────────┤
│  存储层：序列化/反序列化                                      │
│  - JSON 格式（v2.1）                                         │
│  - 目录结构（v2.2）                                          │
└─────────────────────────────────────────────────────────────┘
```

---

### 3.2 核心类设计

#### SimObject 扩展

```cpp
// include/core/sim_object.hh
#ifndef SIM_OBJECT_HH
#define SIM_OBJECT_HH

#include <json.hpp>  // nlohmann/json
#include <string>
#include <vector>

using json = nlohmann::json;

// 复位配置
struct ResetConfig {
    bool hierarchical = true;       // 层次化复位
    bool save_snapshot = false;     // 保存快照
    bool preserve_errors = true;    // 保留错误记录
    bool preserve_transactions = false;
    std::string snapshot_path = "";
};

class SimObject {
protected:
    std::string name_;
    SimObject* parent_ = nullptr;
    std::vector<SimObject*> children_;
    bool initialized_ = false;
    
public:
    SimObject(const std::string& n) : name_(n) {}
    virtual ~SimObject() = default;
    
    // ========== 生命周期接口 ==========
    
    // 初始化（仿真前调用一次）
    virtual void init() {
        initialized_ = true;
    }
    
    // 核心接口
    virtual void tick() = 0;
    
    // ========== 复位接口（v2.0） ==========
    
    // 复位方法（默认层次化）
    virtual void reset(const ResetConfig& config = ResetConfig()) {
        if (config.hierarchical) {
            for (auto* child : children_) {
                child->reset(config);
            }
        }
        do_reset(config);
    }
    
    // 执行复位（子类重写）
    virtual void do_reset(const ResetConfig& config) {
        // 默认：清除状态标志
    }
    
    // ========== 快照接口（v2.1） ==========
    
    // 保存快照到 JSON
    virtual void save_snapshot(json& j) {
        j["name"] = name_;
        j["initialized"] = initialized_;
        // 子类可扩展
    }
    
    // 从 JSON 加载快照
    virtual void load_snapshot(const json& j) {
        if (j.contains("name")) name_ = j["name"];
        if (j.contains("initialized")) initialized_ = j["initialized"];
        // 子类可扩展
    }
    
    // ========== 层次化管理 ==========
    
    void set_parent(SimObject* parent) { parent_ = parent; }
    
    void add_child(SimObject* child) {
        children_.push_back(child);
        child->set_parent(this);
    }
    
    SimObject* get_parent() const { return parent_; }
    const std::vector<SimObject*>& get_children() const { return children_; }
    
    // ========== 状态查询 ==========
    
    bool is_initialized() const { return initialized_; }
    const std::string& name() const { return name_; }
};

#endif // SIM_OBJECT_HH
```

---

#### CheckpointManager（单例）

```cpp
// include/framework/checkpoint_manager.hh
#ifndef CHECKPOINT_MANAGER_HH
#define CHECKPOINT_MANAGER_HH

#include "../core/sim_object.hh"
#include "debug_tracker.hh"
#include "transaction_tracker.hh"
#include <json.hpp>
#include <filesystem>
#include <map>

using json = nlohmann::json;

// 检查点元数据
struct CheckpointMeta {
    uint64_t cycle;
    std::string timestamp;
    std::string version;
    std::vector<std::string> modules;
    bool has_event_queue = false;
    bool has_debug_state = false;
    bool has_transaction_state = false;
};

// 检查点管理器（框架层单例）
class CheckpointManager {
private:
    std::vector<SimObject*> root_modules_;
    std::map<std::string, CheckpointMeta> checkpoints_;
    uint64_t current_cycle_ = 0;
    
    // 配置
    bool auto_save_enabled_ = false;
    uint64_t auto_save_interval_ = 10000000;  // 10M 周期
    uint64_t next_auto_save_ = 0;
    
    CheckpointManager() = default;
    
public:
    static CheckpointManager& instance() {
        static CheckpointManager manager;
        return manager;
    }
    
    // ========== 初始化 ==========
    
    void register_root_module(SimObject* module) {
        root_modules_.push_back(module);
    }
    
    void set_current_cycle(uint64_t cycle) {
        current_cycle_ = cycle;
        
        // 检查是否需要自动保存
        if (auto_save_enabled_ && cycle >= next_auto_save_) {
            std::string path = "cpt_auto_" + std::to_string(cycle) + "/";
            save(path);
            next_auto_save_ = cycle + auto_save_interval_;
        }
    }
    
    void set_auto_save(bool enable, uint64_t interval = 10000000) {
        auto_save_enabled_ = enable;
        auto_save_interval_ = interval;
        next_auto_save_ = current_cycle_ + interval;
    }
    
    // ========== 快照管理（v2.1） ==========
    
    // 保存快照（JSON 格式）
    void save(const std::string& path) {
        json j;
        
        // 元数据
        j["meta"]["cycle"] = current_cycle_;
        j["meta"]["timestamp"] = get_current_timestamp();
        j["meta"]["version"] = "1.0";
        
        // 保存所有模块状态
        for (auto* module : root_modules_) {
            save_module_recursive(module, j["modules"][module->name()]);
        }
        
        // 保存 DebugTracker 状态（可选）
        if (DebugTracker::instance().error_count() > 0) {
            DebugTracker::instance().save_snapshot(j["debug"]);
            j["meta"]["has_debug_state"] = true;
        }
        
        // 保存 TransactionTracker 状态（可选）
        if (TransactionTracker::instance().active_count() > 0) {
            TransactionTracker::instance().save_snapshot(j["transactions"]);
            j["meta"]["has_transaction_state"] = true;
        }
        
        // 写入文件
        std::ofstream file(path);
        file << j.dump(2);  // 格式化输出
        
        // 记录检查点
        checkpoints_[path] = parse_meta(j["meta"]);
        
        DPRINTF(CHECKPOINT, "Saved checkpoint to %s (cycle=%lu)\n", 
                path.c_str(), current_cycle_);
    }
    
    // 加载快照
    void load(const std::string& path) {
        std::ifstream file(path);
        json j;
        file >> j;
        
        // 恢复元数据
        current_cycle_ = j["meta"]["cycle"];
        
        // 恢复所有模块状态
        for (auto* module : root_modules_) {
            if (j["modules"].contains(module->name())) {
                load_module_recursive(module, j["modules"][module->name()]);
            }
        }
        
        // 恢复 DebugTracker 状态
        if (j.contains("debug")) {
            DebugTracker::instance().load_snapshot(j["debug"]);
        }
        
        // 恢复 TransactionTracker 状态
        if (j.contains("transactions")) {
            TransactionTracker::instance().load_snapshot(j["transactions"]);
        }
        
        DPRINTF(CHECKPOINT, "Loaded checkpoint from %s (cycle=%lu)\n", 
                path.c_str(), current_cycle_);
    }
    
    // ========== 完整检查点（v2.2） ==========
    
    // 保存完整检查点（包括事件队列）
    void save_full_checkpoint(const std::string& dir) {
        // 创建目录
        std::filesystem::create_directories(dir);
        
        // 保存系统状态（JSON）
        save(dir + "state.json");
        
        // 保存事件队列（v2.2 实现）
        // save_event_queue(dir + "eventq.json");
        
        // 保存随机数状态
        // save_rng_state(dir + "rng.json");
        
        // 保存元数据
        CheckpointMeta meta;
        meta.cycle = current_cycle_;
        meta.has_event_queue = true;
        save_meta(dir + "meta.json", meta);
    }
    
    // 加载完整检查点
    void load_full_checkpoint(const std::string& dir) {
        // 加载系统状态
        load(dir + "state.json");
        
        // 加载事件队列（v2.2 实现）
        // load_event_queue(dir + "eventq.json");
        
        // 加载随机数状态
        // load_rng_state(dir + "rng.json");
    }
    
    // ========== 查询接口 ==========
    
    const std::map<std::string, CheckpointMeta>& get_checkpoints() const {
        return checkpoints_;
    }
    
    std::string find_latest(const std::string& pattern = "cpt_*") const {
        // 找到最近的检查点
        // ...
        return "";
    }
    
    uint64_t get_current_cycle() const {
        return current_cycle_;
    }
    
private:
    void save_module_recursive(SimObject* module, json& j) {
        // 保存当前模块
        module->save_snapshot(j);
        
        // 递归保存子模块
        for (auto* child : module->get_children()) {
            save_module_recursive(child, j["children"][child->name()]);
        }
    }
    
    void load_module_recursive(SimObject* module, const json& j) {
        // 加载当前模块
        module->load_snapshot(j);
        
        // 递归加载子模块
        for (auto* child : module->get_children()) {
            if (j.contains("children") && j["children"].contains(child->name())) {
                load_module_recursive(child, j["children"][child->name()]);
            }
        }
    }
    
    std::string get_current_timestamp() {
        // 获取当前时间戳
        // ...
        return "";
    }
    
    CheckpointMeta parse_meta(const json& j) {
        CheckpointMeta meta;
        meta.cycle = j["cycle"];
        meta.timestamp = j["timestamp"];
        meta.version = j["version"];
        return meta;
    }
    
    void save_meta(const std::string& path, const CheckpointMeta& meta) {
        json j;
        j["cycle"] = meta.cycle;
        j["timestamp"] = meta.timestamp;
        j["version"] = meta.version;
        j["has_event_queue"] = meta.has_event_queue;
        
        std::ofstream file(path);
        file << j.dump(2);
    }
};

#endif // CHECKPOINT_MANAGER_HH
```

---

#### ResetCoordinator（复位协调器）

```cpp
// include/framework/reset_coordinator.hh
#ifndef RESET_COORDINATOR_HH
#define RESET_COORDINATOR_HH

#include "../core/sim_object.hh"
#include "checkpoint_manager.hh"

class ResetCoordinator {
private:
    std::vector<SimObject*> root_modules_;
    ResetConfig default_config_;
    
    ResetCoordinator() = default;
    
public:
    static ResetCoordinator& instance() {
        static ResetCoordinator coordinator;
        return coordinator;
    }
    
    void register_root_module(SimObject* module) {
        root_modules_.push_back(module);
    }
    
    void set_default_config(const ResetConfig& config) {
        default_config_ = config;
    }
    
    // 系统级复位
    void system_reset(const ResetConfig& config = ResetConfig()) {
        ResetConfig cfg = config;
        
        // 可选保存快照
        if (cfg.save_snapshot) {
            CheckpointManager::instance().save(cfg.snapshot_path);
        }
        
        // 清除追踪器（根据配置）
        if (!cfg.preserve_errors) {
            DebugTracker::instance().clear_all();
        }
        if (!cfg.preserve_transactions) {
            TransactionTracker::instance().clear_all();
        }
        
        // 层次化复位
        for (auto* module : root_modules_) {
            module->reset(cfg);
        }
    }
    
    // 模块级复位
    void module_reset(SimObject* module, const ResetConfig& config = ResetConfig()) {
        module->reset(config);
    }
};

#endif // RESET_COORDINATOR_HH
```

---

### 3.3 模块示例

#### Cache 模块（快照实现）

```cpp
// include/modules/cache_v2.hh
class CacheV2 : public TLMModule {
private:
    std::map<uint64_t, uint64_t> cache_lines_;
    std::map<uint64_t, CoherenceState> cache_states_;
    std::queue<Packet*> req_buffer_;
    size_t size_ = 32 * 1024;  // 32KB
    
public:
    CacheV2(const std::string& n) : TLMModule(n) {}
    
    // ✅ 复位实现
    void do_reset(const ResetConfig& config) override {
        cache_lines_.clear();
        cache_states_.clear();
        
        // 清除请求缓冲
        while (!req_buffer_.empty()) {
            Packet* pkt = req_buffer_.front();
            req_buffer_.pop();
            PacketPool::get().release(pkt);
        }
    }
    
    // ✅ 快照保存（v2.1）
    void save_snapshot(json& j) override {
        TLMModule::save_snapshot(j);
        
        // 保存缓存行
        j["cache_lines"] = cache_lines_;
        
        // 保存一致性状态（序列化为字符串）
        for (const auto& [addr, state] : cache_states_) {
            j["coherence_states"][std::to_string(addr)] = 
                coherence_state_to_string(state);
        }
        
        // 保存缓冲大小
        j["pending_requests"] = req_buffer_.size();
        
        // 保存配置
        j["size"] = size_;
    }
    
    // ✅ 快照加载（v2.1）
    void load_snapshot(const json& j) override {
        TLMModule::load_snapshot(j);
        
        // 恢复缓存行
        if (j.contains("cache_lines")) {
            cache_lines_ = j["cache_lines"].get<std::map<uint64_t, uint64_t>>();
        }
        
        // 恢复一致性状态
        if (j.contains("coherence_states")) {
            for (const auto& [addr_str, state_str] : 
                 j["coherence_states"].items()) {
                uint64_t addr = std::stoull(addr_str);
                cache_states_[addr] = parse_coherence_state(state_str);
            }
        }
        
        // 恢复配置
        if (j.contains("size")) {
            size_ = j["size"];
        }
    }
};
```

---

#### Crossbar 模块（快照实现）

```cpp
// include/modules/crossbar_v2.hh
class CrossbarV2 : public TLMModule {
private:
    int num_ports_ = 4;
    std::map<uint64_t, int> routing_table_;
    
public:
    CrossbarV2(const std::string& n) : TLMModule(n) {}
    
    void do_reset(const ResetConfig& config) override {
        // 清除路由表（如果有动态条目）
        // ...
    }
    
    void save_snapshot(json& j) override {
        TLMModule::save_snapshot(j);
        
        j["num_ports"] = num_ports_;
        j["routing_table"] = routing_table_;
    }
    
    void load_snapshot(const json& j) override {
        TLMModule::load_snapshot(j);
        
        if (j.contains("num_ports")) num_ports_ = j["num_ports"];
        if (j.contains("routing_table")) routing_table_ = j["routing_table"];
    }
};
```

---

## 4. 分阶段实施方案

### 4.1 Phase 1: 轻量级复位（v2.0）

**目标**: 实现基本复位功能，满足测试迭代需求

**实现内容**:
```cpp
// SimObject 基类
class SimObject {
    virtual void reset(const ResetConfig& config);
    virtual void do_reset(const ResetConfig& config);
};

// ResetCoordinator
class ResetCoordinator {
    void system_reset();
    void module_reset(SimObject* module);
};
```

**模块实现**:
- Cache: 清除 cache_lines_, req_buffer_
- Crossbar: 清除动态路由表
- Memory: 清除 memory_ 内容

**验收标准**:
- [ ] 复位后所有模块状态清零
- [ ] 层次化复位正常工作
- [ ] 复位延迟 <1 秒

**预计工期**: 3 天

---

### 4.2 Phase 2: 状态快照（v2.1）

**目标**: 实现 JSON 格式快照，支持多配置对比

**实现内容**:
```cpp
// SimObject 扩展
class SimObject {
    virtual void save_snapshot(json& j);
    virtual void load_snapshot(const json& j);
};

// CheckpointManager
class CheckpointManager {
    void save(const std::string& path);
    void load(const std::string& path);
};
```

**模块实现**:
- Cache: 保存 cache_lines_, cache_states_, size_
- Crossbar: 保存 routing_table_, num_ports_
- Memory: 保存 memory_ 内容

**验收标准**:
- [ ] 快照保存/恢复后状态一致
- [ ] JSON 格式人类可读
- [ ] 保存时间 <10 秒，恢复时间 <5 秒
- [ ] 多配置对比实验正常工作

**预计工期**: 7 天

**依赖**: nlohmann/json 库

---

### 4.3 Phase 3: 完整检查点（v2.2）

**目标**: 实现完整检查点，支持事件队列持久化

**实现内容**:
```cpp
// CheckpointManager 扩展
class CheckpointManager {
    void save_full_checkpoint(const std::string& dir);
    void load_full_checkpoint(const std::string& dir);
    
private:
    void save_event_queue(const std::string& path);
    void load_event_queue(const std::string& path);
    void save_rng_state(const std::string& path);
    void load_rng_state(const std::string& path);
};
```

**额外内容**:
- 事件队列序列化
- 随机数生成器状态
- 统计计数器
- 目录结构组织

**验收标准**:
- [ ] 完整检查点保存/恢复
- [ ] 事件队列正确恢复
- [ ] 长时间仿真中断恢复测试通过

**预计工期**: 14 天

---

## 5. 使用示例

### 5.1 初始化

```cpp
// main.cpp
int sc_main() {
    // 创建模块
    auto* cpu = new CPUSim("cpu");
    auto* cache = new CacheV2("cache");
    auto* crossbar = new CrossbarV2("crossbar");
    auto* memory = new MemoryV2("memory");
    
    // 建立层次关系
    cpu->add_child(cache);
    
    // 注册根模块
    ResetCoordinator::instance().register_root_module(cpu);
    ResetCoordinator::instance().register_root_module(memory);
    CheckpointManager::instance().register_root_module(cpu);
    CheckpointManager::instance().register_root_module(memory);
    
    // 配置自动保存
    CheckpointManager::instance().set_auto_save(true, 10000000);  // 每 10M 周期
    
    // 初始化
    cpu->init();
    memory->init();
    
    // 运行仿真
    while (CheckpointManager::instance().get_current_cycle() < TOTAL_CYCLES) {
        run_simulation(1000000);  // 每次 1M 周期
        CheckpointManager::instance().set_current_cycle(
            CheckpointManager::instance().get_current_cycle() + 1000000);
    }
    
    return 0;
}
```

---

### 5.2 多配置对比

```cpp
void cache_size_sweep() {
    // 1. 仿真到 1M 周期
    run_simulation(1000000);
    CheckpointManager::instance().set_current_cycle(1000000);
    
    // 2. 保存检查点
    CheckpointManager::instance().save("cpt_warmup_1M.json");
    
    // 3. 多配置对比
    std::vector<size_t> sizes = {32*KB, 64*KB, 128*KB};
    
    for (size_t size : sizes) {
        // 恢复检查点
        CheckpointManager::instance().load("cpt_warmup_1M.json");
        
        // 修改配置
        cache->set_size(size);
        
        // 继续仿真
        run_simulation(9000000);
        
        // 记录结果
        results[size] = cache->get_stats();
    }
}
```

---

### 5.3 调试场景

```cpp
void debug_coherence_violation() {
    // 仿真到问题点
    run_simulation(5000000);
    
    // 检测到违例
    if (DebugTracker::instance().error_count() > 0) {
        // 保存问题现场
        CheckpointManager::instance().save("cpt_violation_5M.json");
        
        // 导出错误
        DebugTracker::instance().export_errors("errors_5M.csv");
        
        // 回放交易
        auto errors = DebugTracker::instance().get_errors_by_category(
            ErrorCategory::COHERENCE);
        for (const auto& err : errors) {
            std::string replay = DebugTracker::instance().replay_transaction(
                err.transaction_id);
            DPRINTF(DEBUG, "%s\n", replay.c_str());
        }
        
        // 恢复检查点，添加调试日志
        CheckpointManager::instance().load("cpt_violation_5M.json");
        enable_debug_logging();
        run_simulation(100000);
    }
}
```

---

## 6. JSON 格式示例

### 6.1 快照格式（v2.1）

```json
{
  "meta": {
    "cycle": 1000000,
    "timestamp": "2026-04-09T19:00:00Z",
    "version": "1.0",
    "has_debug_state": true,
    "has_transaction_state": false
  },
  "modules": {
    "cpu": {
      "name": "cpu",
      "initialized": true,
      "children": {
        "l1_cache": {
          "name": "l1_cache",
          "cache_lines": {
            "0x1000": 0xDEADBEEF,
            "0x2000": 0xCAFEBABE
          },
          "coherence_states": {
            "0x1000": "M",
            "0x2000": "S"
          },
          "pending_requests": 0,
          "size": 32768
        }
      }
    },
    "memory": {
      "name": "memory",
      "initialized": true,
      "memory_content": {
        "0x1000": 0xDEADBEEF,
        "0x2000": 0xCAFEBABE
      }
    }
  },
  "debug": {
    "error_count": 1,
    "errors": [
      {
        "error_id": 1,
        "timestamp": 5000000,
        "transaction_id": 100,
        "error_code": 768,
        "category": "COHERENCE",
        "module": "l1_cache",
        "message": "Invalid state transition"
      }
    ]
  }
}
```

---

### 6.2 完整检查点目录结构（v2.2）

```
cpt_5000000/
├── meta.json           # 检查点元数据
├── state.json          # 系统状态（JSON）
├── eventq.json         # 事件队列
├── rng.json            # 随机数状态
├── debug/
│   └── errors.csv      # 错误记录
└── transactions/
    └── active.json     # 活跃交易
```

---

## 7. 与现有架构整合

| 架构 | 整合方式 |
|------|---------|
| **交易处理** | CheckpointManager 保存/恢复 TransactionTracker 状态 |
| **错误调试** | CheckpointManager 保存/恢复 DebugTracker 状态 |
| **模块系统** | SimObject::save_snapshot/load_snapshot 接口 |

---

## 8. 需要确认的问题

| 问题 | 选项 | 推荐 |
|------|------|------|
| **Q1**: 首要场景？ | A) 中断恢复 / B) 多配置对比 / C) 调试 | **B) 多配置对比** |
| **Q2**: 快照格式？ | A) 二进制 / B) JSON / C) 自定义 | **B) JSON** |
| **Q3**: 实施策略？ | A) 一次性完整 / B) 分阶段 | **B) 分阶段** |
| **Q4**: v2.0 范围？ | A) 仅复位 / B) 复位 + 轻量快照 | **B) 复位 + 轻量快照** |
| **Q5**: 事件队列持久化？ | A) 需要 / B) 不需要 / C) 可选 | **C) 可选（v2.2）** |

---

## 9. 相关文档

| 文档 | 位置 |
|------|------|
| 交易处理架构 | `02-architecture/02-transaction-architecture.md` |
| 错误调试架构 | `02-architecture/03-error-debug-architecture.md` |
| 复位策略 ADR | `03-adr/ADR-X.3-reset-strategy.md` |

---

**版本**: v1.0  
**创建日期**: 2026-04-09  
**状态**: 📋 待确认
