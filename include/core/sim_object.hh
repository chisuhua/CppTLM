// include/core/sim_object.hh
// SimObject - 所有仿真模块的基类
// 扩展：reset(), snapshot(), 层次化管理
#ifndef SIM_OBJECT_HH
#define SIM_OBJECT_HH

#include "event_queue.hh"
#include "port_manager.hh"
#include <memory>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstdint>
#include <map>

using json = nlohmann::json;

class Packet; // Forward declaration

struct LayoutInfo {
    double x = -1, y = -1;
    bool valid() const { return x >= 0 && y >= 0; }
};

// P3.2 Wave 1: Core Transaction Types
enum class TransactionAction {
    PASSTHROUGH = 0,  // Crossbar routing
    TRANSFORM = 1,    // Cache creating sub-transactions
    TERMINATE = 2,    // Memory ending transaction
    BLOCK = 3         // Stall for resources
};

struct TransactionInfo {
    uint64_t transaction_id = 0;
    uint64_t parent_id = 0;
    uint8_t  fragment_id = 0;
    uint8_t  fragment_total = 1;
    TransactionAction action = TransactionAction::PASSTHROUGH;
    
    bool is_root() const { return parent_id == 0 && fragment_total == 1; }
    bool is_fragmented() const { return fragment_total > 1; }
};

struct FragmentBuffer {
    uint64_t parent_id = 0;
    uint8_t fragment_total = 0;
    std::map<uint8_t, Packet*> fragments;
    uint64_t first_arrival_time = 0;
    
    bool is_complete() const { return fragment_total > 0 && fragments.size() == fragment_total; }
    bool has_fragment(uint8_t id) const { return fragments.count(id) > 0; }
};

/**
 * @brief 复位配置
 * 
 * 控制层次化复位行为
 */
struct ResetConfig {
    bool hierarchical = true;      ///< 是否层次化复位（递归复位子模块）
    bool save_snapshot = false;    ///< 是否在复位前保存快照
    bool preserve_errors = true;   ///< 是否保留错误状态
    bool preserve_transactions = false; ///< 是否保留交易追踪
    
    ResetConfig() = default;
    ResetConfig(bool hier, bool snapshot = false, bool preserve_err = true)
        : hierarchical(hier), save_snapshot(snapshot), preserve_errors(preserve_err) {}
};

class SimObject {
protected:
    std::string name;
    EventQueue* event_queue;
    std::unique_ptr<PortManager> port_manager;
    LayoutInfo layout;
    
    // 层次化管理
    SimObject* parent_ = nullptr;
    std::vector<SimObject*> children_;
    
    // 状态跟踪
    bool initialized_ = false;
    bool reset_pending_ = false;

public:
    SimObject(const std::string& n, EventQueue* eq) 
        : name(n), event_queue(eq) {}
    
    virtual ~SimObject() = default;

    // ========== 生命周期管理 ==========
    
    /**
     * @brief 初始化模块
     * 可在子类中重写以执行初始化逻辑
     */
    virtual void init() { 
        initialized_ = true; 
    }
    
    /**
     * @brief 仿真 Tick（纯虚函数，子类必须实现）
     */
    virtual void tick() = 0;
    
    /**
     * @brief 启动 Tick 循环（框架调用）
     */
    void initiate_tick() {
        event_queue->schedule(new TickEvent(this), 1);
    }

    // ========== 层次化复位 ==========
    
    /**
     * @brief 执行复位操作
     * 
     * 框架方法，处理层次化逻辑后调用 do_reset()
     * 
     * @param config 复位配置
     */
    virtual void reset(const ResetConfig& config = ResetConfig()) {
        reset_pending_ = true;
        
        // 可选：保存快照
        if (config.save_snapshot) {
            json snapshot;
            save_snapshot(snapshot);
            // 快照可由用户处理或存储
        }
        
        // 层次化复位：先复位子模块
        if (config.hierarchical) {
            for (auto* child : children_) {
                if (child) {
                    child->reset(config);
                }
            }
        }
        
        // 调用子类实现的复位逻辑
        do_reset(config);
        
        reset_pending_ = false;
    }
    
    /**
     * @brief 子类重写此方法实现具体复位逻辑
     * 
     * 默认实现：空（保持向后兼容）
     * 
     * @param config 复位配置
     */
    virtual void do_reset(const ResetConfig& config) {
        // 默认空实现，子类可选择性重写
        (void)config; // 避免未使用警告
    }

    // ========== 快照（Checkpoint）支持 ==========
    
    /**
     * @brief 保存状态快照到 JSON
     * 
     * 子类可重写以保存自定义状态
     * 
     * @param j JSON 对象（输出）
     */
    virtual void save_snapshot(json& j) const {
        j["name"] = name;
        j["initialized"] = initialized_;
        j["type"] = get_module_type();
    }
    
    /**
     * @brief 从 JSON 加载状态快照
     * 
     * 子类可重写以恢复自定义状态
     * 
     * @param j JSON 对象（输入）
     */
    virtual void load_snapshot(const json& j) {
        if (j.contains("name")) {
            name = j["name"];
        }
        if (j.contains("initialized")) {
            initialized_ = j["initialized"];
        }
    }

    // ========== 层次化管理 ==========
    
    /**
     * @brief 添加子模块
     * 
     * @param child 子模块指针
     */
    void add_child(SimObject* child) {
        if (child) {
            children_.push_back(child);
            child->set_parent(this);
        }
    }
    
    /**
     * @brief 设置父模块
     * 
     * @param parent 父模块指针
     */
    void set_parent(SimObject* parent) {
        parent_ = parent;
    }
    
    /**
     * @brief 获取父模块
     * @return 父模块指针（无父模块返回 nullptr）
     */
    SimObject* get_parent() const { 
        return parent_; 
    }
    
    /**
     * @brief 获取所有子模块
     * @return 子模块引用向量
     */
    const std::vector<SimObject*>& get_children() const { 
        return children_; 
    }
    
    /**
     * @brief 检查是否有子模块
     * @return true 如果有子模块
     */
    bool has_children() const {
        return !children_.empty();
    }

    // ========== 状态查询 ==========
    
    const std::string& getName() const { return name; }
    EventQueue* getEventQueue() const { return event_queue; }
    
    PortManager& getPortManager() {
        if (!port_manager) port_manager = std::make_unique<PortManager>();
        return *port_manager;
    }
    bool hasPortManager() const { return port_manager != nullptr; }
    
    void setLayout(double x, double y) {
        layout.x = x; layout.y = y;
    }
    const LayoutInfo& getLayout() const { return layout; }
    
    uint64_t getCurrentCycle() const {
        return event_queue->getCurrentCycle();
    }
    
    /**
     * @brief 检查模块是否已初始化
     * @return true 如果已初始化
     */
    bool is_initialized() const {
        return initialized_;
    }
    
    /**
     * @brief 检查是否正在复位
     * @return true 如果复位进行中
     */
    bool is_reset_pending() const {
        return reset_pending_;
    }
    
    /**
     * @brief 获取模块类型名称（用于调试）
     * @return 类型名称字符串
     */
    virtual std::string get_module_type() const {
        return "SimObject";
    }

    // ========== 配置管理（Phase 1: 选项A + 预留选项B扩展点）==========

    /**
     * @brief 配置参数描述符（选项B扩展点）
     * 
     * 派生类可重写以声明参数 Schema，用于验证和自动生成文档。
     * 默认返回空 vector，表示使用选项A的简单存储模式（无验证）。
     */
    struct ParamDesc {
        std::string name;       ///< 参数名称
        enum Type { INT, UINT, STRING, BOOL, ARRAY } type;  ///< 参数类型
        bool required = false;  ///< 是否必填
        json default_value;     ///< 默认值

        ParamDesc() = default;
        ParamDesc(const std::string& n, Type t, bool req, const json& def)
            : name(n), type(t), required(req), default_value(def) {}
    };

    /**
     * @brief 获取参数 Schema 描述
     * @return 参数描述向量（空=选项A兼容模式）
     */
    virtual std::vector<ParamDesc> get_param_schema() const {
        return {};
    }

    /**
     * @brief 设置配置参数（选项A基础实现）
     * 
     * 基类存储原始 JSON，子类可重写 on_config_loaded() 做解析和验证。
     * 
     * @param params JSON 配置对象
     */
    virtual void set_config(const json& params) {
        config_ = params;
        on_config_loaded();
    }

    /**
     * @brief 获取已存储的配置参数
     * @return JSON 配置对象的引用
     */
    const json& get_config() const { return config_; }

    /**
     * @brief 配置加载后回调（子类可重写）
     * 
     * 选项A：子类在此解析 config_ 中的字段
     * 选项B：基类在此自动验证 schema（如果已实现）
     * 
     * 默认空实现，保持向后兼容。
     */
    virtual void on_config_loaded() {}

    /**
     * @brief 验证配置是否合法
     * 
     * 当 get_param_schema() 返回非空时，基类可自动验证。
     * 默认实现：schema 为空时返回 true，否则使用验证器检查。
     * 
     * @return true 如果配置合法或无 schema
     */
    virtual bool validate_config() const {
        auto schema = get_param_schema();
        if (schema.empty()) {
            return true;  // 选项A兼容模式
        }
        // 选项B：使用 schema 验证（未来实现）
        for (const auto& desc : schema) {
            if (desc.required && !config_.contains(desc.name)) {
                DPRINTF(MODULE, "[CONFIG] %s missing required param: %s\n", name.c_str(), desc.name.c_str());
                return false;
            }
        }
        return true;
    }

protected:
    /// 存储原始 JSON 配置（选项A基础实现）
    json config_;

public:
    
    virtual bool handleDownstreamResponse(Packet* pkt, int src_id, const std::string& src_label) {
        DPRINTF(MODULE, "[WARNING] Unhandled downstream response in %s\n", name.c_str());
        (void)pkt; (void)src_id; (void)src_label; // 避免未使用警告
        return false;
    }
    
    virtual bool handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) {
        DPRINTF(MODULE, "[WARNING] Unhandled upstream request in %s\n", name.c_str());
        (void)pkt; (void)src_id; (void)src_label;
        return false;
    }
};

inline void TickEvent::process() {
    obj->tick();
    obj->initiate_tick();
}

#endif // SIM_OBJECT_HH
