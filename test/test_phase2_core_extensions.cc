// test/test_phase2_core_extensions.cc
// Phase 2: SimObject 扩展 + ErrorCode 测试

#include <catch2/catch_all.hpp>
#include "core/sim_object.hh"
#include "core/error_category.hh"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ========== ResetConfig 测试 ==========

TEST_CASE("ResetConfig 默认值", "[sim_object][reset]") {
    ResetConfig config;
    REQUIRE(config.hierarchical == true);
    REQUIRE(config.save_snapshot == false);
    REQUIRE(config.preserve_errors == true);
}

TEST_CASE("ResetConfig 构造函数", "[sim_object][reset]") {
    ResetConfig config(true, true, false);
    REQUIRE(config.hierarchical == true);
    REQUIRE(config.save_snapshot == true);
    REQUIRE(config.preserve_errors == false);
}

// ========== Mock SimObject 用于测试 ==========

class MockSimObject : public SimObject {
public:
    int reset_called = 0;
    json last_snapshot;
    
    MockSimObject(const std::string& n, EventQueue* eq) 
        : SimObject(n, eq) {}
    
    void tick() override {
        // Mock 实现
    }
    
    void do_reset(const ResetConfig& config) override {
        reset_called++;
        SimObject::do_reset(config);
    }
    
    void save_snapshot(json& j) const override {
        j = json{{"name", name}, {"initialized", initialized_}, {"type", get_module_type()}, {"custom", "data"}};
    }
    
    void load_snapshot(const json& j) override {
        SimObject::load_snapshot(j);
        if (j.contains("custom")) {
            // 自定义字段处理（mock）
        }
    }
    
    std::string get_module_type() const override {
        return "MockSimObject";
    }
};

// ========== SimObject 层次化测试 ==========

TEST_CASE("SimObject 基础功能", "[sim_object]") {
    EventQueue eq;
    MockSimObject obj("test_obj", &eq);
    
    SECTION("名称查询") {
        REQUIRE(obj.getName() == "test_obj");
    }
    
    SECTION("EventQueue 查询") {
        REQUIRE(obj.getEventQueue() == &eq);
    }
    
    SECTION("初始化状态") {
        REQUIRE(obj.is_initialized() == false);
        obj.init();
        REQUIRE(obj.is_initialized() == true);
    }
    
    SECTION("模块类型") {
        REQUIRE(obj.get_module_type() == "MockSimObject");
    }
}

TEST_CASE("SimObject 层次化管理", "[sim_object][hierarchy]") {
    EventQueue eq;
    auto* parent = new MockSimObject("parent", &eq);
    auto* child1 = new MockSimObject("child1", &eq);
    auto* child2 = new MockSimObject("child2", &eq);
    
    SECTION("添加子模块") {
        parent->add_child(child1);
        parent->add_child(child2);
        
        REQUIRE(parent->has_children() == true);
        REQUIRE(parent->get_children().size() == 2);
        REQUIRE(child1->get_parent() == parent);
        REQUIRE(child2->get_parent() == parent);
    }
    
    SECTION("层次化复位") {
        parent->add_child(child1);
        child1->add_child(child2);
        
        ResetConfig config;
        parent->reset(config);
        
        REQUIRE(parent->reset_called == 1);
        REQUIRE(child1->reset_called == 1);
        REQUIRE(child2->reset_called == 1);
    }
    
    delete parent;
    delete child1;
    delete child2;
}

TEST_CASE("SimObject 快照功能", "[sim_object][snapshot]") {
    EventQueue eq;
    MockSimObject obj("snapshot_test", &eq);
    
    SECTION("保存快照") {
        json j;
        obj.save_snapshot(j);
        
        REQUIRE(j.contains("name"));
        REQUIRE(j.contains("initialized"));
        REQUIRE(j["name"] == "snapshot_test");
    }
    
    SECTION("加载快照") {
        json j = {{"name", "restored_obj"}, {"initialized", true}};
        obj.load_snapshot(j);
        
        REQUIRE(obj.getName() == "restored_obj");
        REQUIRE(obj.is_initialized() == true);
    }
}

TEST_CASE("SimObject 非层次化复位", "[sim_object][reset]") {
    EventQueue eq;
    auto* parent = new MockSimObject("parent", &eq);
    auto* child = new MockSimObject("child", &eq);
    
    parent->add_child(child);
    
    ResetConfig config;
    config.hierarchical = false;  // 禁用层次化
    
    parent->reset(config);
    
    REQUIRE(parent->reset_called == 1);
    REQUIRE(child->reset_called == 0);  // 子模块不应被复位
    
    delete parent;
    delete child;
}

// ========== ErrorCode 测试 ==========

TEST_CASE("ErrorCode 默认值", "[error][category]") {
    ErrorCode code = ErrorCode::SUCCESS;
    REQUIRE(get_error_category(code) == ErrorCategory::SUCCESS);
    REQUIRE(is_success(code) == true);
    REQUIRE(is_fatal_error(code) == false);
}

TEST_CASE("错误类别提取", "[error][category]") {
    REQUIRE(get_error_category(ErrorCode::TRANSPORT_INVALID_ADDRESS) == ErrorCategory::TRANSPORT);
    REQUIRE(get_error_category(ErrorCode::COHERENCE_DEADLOCK) == ErrorCategory::COHERENCE);
    REQUIRE(get_error_category(ErrorCode::RESOURCE_BUFFER_FULL) == ErrorCategory::RESOURCE);
}

TEST_CASE("Phase2 错误码转字符串", "[error][category]") {
    REQUIRE(error_code_to_string(ErrorCode::SUCCESS) == "SUCCESS");
    REQUIRE(error_code_to_string(ErrorCode::COHERENCE_DEADLOCK) == "COHERENCE_DEADLOCK");
    REQUIRE(error_code_to_string(ErrorCode::TRANSPORT_TIMEOUT) == "TRANSPORT_TIMEOUT");
    REQUIRE(error_code_to_string(static_cast<ErrorCode>(0x9999)).substr(0, 7) == "UNKNOWN");
}

TEST_CASE("错误类别转字符串", "[error][category]") {
    REQUIRE(error_category_to_string(ErrorCategory::TRANSPORT) == "TRANSPORT");
    REQUIRE(error_category_to_string(ErrorCategory::COHERENCE) == "COHERENCE");
    REQUIRE(error_category_to_string(ErrorCategory::SECURITY) == "SECURITY");
}

TEST_CASE("Phase2 严重错误检测", "[error][category]") {
    REQUIRE(is_fatal_error(ErrorCode::COHERENCE_DEADLOCK) == true);
    REQUIRE(is_fatal_error(ErrorCode::COHERENCE_DATA_INCONSISTENCY) == true);
    REQUIRE(is_fatal_error(ErrorCode::SECURITY_TAMPER_DETECTED) == true);
    REQUIRE(is_fatal_error(ErrorCode::RESOURCE_BUFFER_FULL) == false);
}

TEST_CASE("Phase2 可恢复错误检测", "[error][category]") {
    REQUIRE(is_recoverable_error(ErrorCode::RESOURCE_BUFFER_FULL) == true);
    REQUIRE(is_recoverable_error(ErrorCode::TRANSPORT_TIMEOUT) == true);
    REQUIRE(is_recoverable_error(ErrorCode::RESOURCE_STARVATION) == true);
    REQUIRE(is_recoverable_error(ErrorCode::COHERENCE_DEADLOCK) == false);
}

TEST_CASE("错误码枚举值", "[error][category]") {
    // 验证错误码格式：高字节为类别
    REQUIRE(static_cast<uint16_t>(ErrorCode::TRANSPORT_INVALID_ADDRESS) >> 8 == 0x01);
    REQUIRE(static_cast<uint16_t>(ErrorCode::COHERENCE_STATE_VIOLATION) >> 8 == 0x03);
    REQUIRE(static_cast<uint16_t>(ErrorCode::PROTOCOL_ID_CONFLICT) >> 8 == 0x04);
}

// ========== 集成测试 ==========

TEST_CASE("SimObject + ErrorCode 集成", "[sim_object][error]") {
    EventQueue eq;
    MockSimObject obj("test", &eq);
    
    SECTION("复位成功场景") {
        ResetConfig config;
        obj.reset(config);
        REQUIRE(obj.reset_called == 1);
        REQUIRE(obj.is_reset_pending() == false);
    }
    
    SECTION("快照包含模块类型") {
        json j;
        obj.save_snapshot(j);
        REQUIRE(j["name"] == "test");
        REQUIRE(j["type"] == "MockSimObject");
    }
}
