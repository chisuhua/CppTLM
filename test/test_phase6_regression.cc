// test/test_phase6_regression.cc
// Phase 6: 回归测试套件（简化版）

#include <catch2/catch_all.hpp>
#include "core/sim_object.hh"
#include "core/error_category.hh"
#include "framework/transaction_tracker.hh"
#include "framework/debug_tracker.hh"
#include "modules/modules_v2.hh"

TEST_CASE("REGRESSION: ErrorCode 类别提取", "[regression][error]") {
    REQUIRE(get_error_category(ErrorCode::SUCCESS) == ErrorCategory::SUCCESS);
    REQUIRE(get_error_category(ErrorCode::COHERENCE_DEADLOCK) == ErrorCategory::COHERENCE);
}

TEST_CASE("REGRESSION: DebugTracker 单例", "[regression][debug]") {
    auto& tracker = DebugTracker::instance();
    tracker.reset_for_testing();
    REQUIRE(&tracker == &DebugTracker::instance());
}

TEST_CASE("REGRESSION: SimObject 层次化复位", "[regression][sim]") {
    class TestModule : public SimObject {
    public:
        int reset_count = 0;
        TestModule(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
        void tick() override {}
        void do_reset(const ResetConfig& c) override { reset_count++; (void)c; }
    };
    
    EventQueue eq;
    auto* parent = new TestModule("parent", &eq);
    auto* child = new TestModule("child", &eq);
    parent->add_child(child);
    parent->reset(ResetConfig(true));
    REQUIRE(parent->reset_count == 1);
    REQUIRE(child->reset_count == 1);
    delete parent;
    delete child;
}

TEST_CASE("REGRESSION: CacheV2 模块类型", "[regression][module]") {
    EventQueue eq;
    CacheV2 cache("cache", &eq, 1024);
    REQUIRE(cache.get_module_type() == "CacheV2");
}

TEST_CASE("REGRESSION: CrossbarV2 透传", "[regression][module]") {
    EventQueue eq;
    CrossbarV2 xbar("xbar", &eq);
    xbar.init();
    REQUIRE(xbar.is_initialized() == true);
}

TEST_CASE("REGRESSION: MemoryV2 终止", "[regression][module]") {
    EventQueue eq;
    MemoryV2 mem("mem", &eq);
    REQUIRE(mem.get_module_type() == "MemoryV2");
}
