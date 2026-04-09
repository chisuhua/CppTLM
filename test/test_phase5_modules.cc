// test/test_phase5_modules.cc
// Phase 5: 模块升级测试（测试先行）
// 遵循 ADR: 层次化复位、交易追踪、错误处理

#include <catch2/catch_all.hpp>
#include "core/sim_object.hh"
#include "core/error_category.hh"
#include "ext/transaction_context_ext.hh"
#include "ext/error_context_ext.hh"
#include "framework/transaction_tracker.hh"
#include "framework/debug_tracker.hh"

// 前置声明：模块将在后续实现
class CacheV2;
class CrossbarV2;
class MemoryV2;

// ========== 模拟模块用于测试框架集成 ==========

class MockResettableModule : public SimObject {
public:
    int reset_count = 0;
    json last_snapshot;
    
    MockResettableModule(const std::string& n, EventQueue* eq) 
        : SimObject(n, eq) {}
    
    void tick() override {}
    
    void do_reset(const ResetConfig& config) override {
        reset_count++;
        (void)config;
    }
    
    void save_snapshot(json& j) const override {
        j = json{{"name", name}, {"reset_count", reset_count}};
    }
    
    std::string get_module_type() const override {
        return "MockResettableModule";
    }
};

// ========== SimObject 层次化复位测试 ==========

TEST_CASE("SimObject 层次化复位", "[module][reset]") {
    EventQueue eq;
    auto* parent = new MockResettableModule("parent", &eq);
    auto* child1 = new MockResettableModule("child1", &eq);
    auto* child2 = new MockResettableModule("child2", &eq);
    
    parent->add_child(child1);
    child1->add_child(child2);
    
    ResetConfig config;
    config.hierarchical = true;
    parent->reset(config);
    
    REQUIRE(parent->reset_count == 1);
    REQUIRE(child1->reset_count == 1);
    REQUIRE(child2->reset_count == 1);
    
    delete parent;
    delete child1;
    delete child2;
}

TEST_CASE("SimObject 非层次化复位", "[module][reset]") {
    EventQueue eq;
    auto* parent = new MockResettableModule("parent", &eq);
    auto* child = new MockResettableModule("child", &eq);
    
    parent->add_child(child);
    
    ResetConfig config;
    config.hierarchical = false;
    parent->reset(config);
    
    REQUIRE(parent->reset_count == 1);
    REQUIRE(child->reset_count == 0);  // 不应该被复位
    
    delete parent;
    delete child;
}

TEST_CASE("SimObject 快照功能", "[module][snapshot]") {
    EventQueue eq;
    MockResettableModule module("test", &eq);
    module.reset_count = 5;
    
    json j;
    module.save_snapshot(j);
    
    REQUIRE(j["name"] == "test");
    REQUIRE(j["reset_count"] == 5);
}

// ========== TransactionTracker 集成测试 ==========

TEST_CASE("TransactionTracker 模块集成", "[module][transaction]") {
    auto& tracker = TransactionTracker::instance();
    tracker.initialize();
    
    tlm::tlm_generic_payload payload;
    
    uint64_t tid = tracker.create_transaction(&payload, "test_module", "READ");
    REQUIRE(tid > 0);
    
    tracker.record_hop(tid, "crossbar", 1, "hopped");
    tracker.complete_transaction(tid);
    
    const auto* record = tracker.get_transaction(tid);
    REQUIRE(record != nullptr);
    REQUIRE(record->is_complete == true);
}

// ========== DebugTracker 集成测试 ==========

TEST_CASE("DebugTracker 模块集成", "[module][error]") {
    auto& tracker = DebugTracker::instance();
    tracker.reset_for_testing();
    tracker.initialize(true, true, false);
    
    tlm::tlm_generic_payload payload;
    
    uint64_t error_id = tracker.record_error(
        &payload, 
        ErrorCode::COHERENCE_STATE_VIOLATION,
        "Test error",
        "test_module"
    );
    
    REQUIRE(error_id > 0);
    
    const auto* record = tracker.get_error(error_id);
    REQUIRE(record != nullptr);
    REQUIRE(record->error_code == ErrorCode::COHERENCE_STATE_VIOLATION);
}

// ========== ErrorCategory 完整性测试 ==========

TEST_CASE("ErrorCategory 覆盖所有类别", "[module][error]") {
    REQUIRE(error_category_to_string(ErrorCategory::SUCCESS) == "SUCCESS");
    REQUIRE(error_category_to_string(ErrorCategory::TRANSPORT) == "TRANSPORT");
    REQUIRE(error_category_to_string(ErrorCategory::RESOURCE) == "RESOURCE");
    REQUIRE(error_category_to_string(ErrorCategory::COHERENCE) == "COHERENCE");
    REQUIRE(error_category_to_string(ErrorCategory::PROTOCOL) == "PROTOCOL");
    REQUIRE(error_category_to_string(ErrorCategory::SECURITY) == "SECURITY");
    REQUIRE(error_category_to_string(ErrorCategory::PERFORMANCE) == "PERFORMANCE");
}

TEST_CASE("ErrorCode 覆盖所有一致性错误", "[module][error]") {
    REQUIRE(error_code_to_string(ErrorCode::COHERENCE_STATE_VIOLATION) != "UNKNOWN_ERROR");
    REQUIRE(error_code_to_string(ErrorCode::COHERENCE_DEADLOCK) != "UNKNOWN_ERROR");
    REQUIRE(error_code_to_string(ErrorCode::COHERENCE_LIVELOCK) != "UNKNOWN_ERROR");
    REQUIRE(error_code_to_string(ErrorCode::COHERENCE_DATA_INCONSISTENCY) != "UNKNOWN_ERROR");
}

// ========== 模块类型判断测试 ==========

class MockModuleTypeA : public SimObject {
public:
    MockModuleTypeA(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    void tick() override {}
    std::string get_module_type() const override { return "ModuleTypeA"; }
};

class MockModuleTypeB : public SimObject {
public:
    MockModuleTypeB(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    void tick() override {}
    std::string get_module_type() const override { return "ModuleTypeB"; }
};

TEST_CASE("SimObject 模块类型识别", "[module][type]") {
    EventQueue eq;
    MockModuleTypeA moduleA("a", &eq);
    MockModuleTypeB moduleB("b", &eq);
    
    REQUIRE(moduleA.get_module_type() == "ModuleTypeA");
    REQUIRE(moduleB.get_module_type() == "ModuleTypeB");
}
