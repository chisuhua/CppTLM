// test/test_sm_modules.cc
// Task 17 L1: 12 SM 子模块 API 契约测试 (stub 阶段)
// 每个 submodule: 1 instantiation + get_module_type + tick() smoke 测试
// 12 子模块 × 2 assertions + namespace + 公共类型 = 36 assertions
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm::sm;

TEST_CASE("FetchUnitTLM API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    FetchUnitTLM fu("fu0", &eq);
    REQUIRE(fu.get_module_type() == "FetchUnitTLM");
    fu.tick();
    REQUIRE(true);
}

TEST_CASE("DecodeUnitTLM API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    DecodeUnitTLM du("du0", &eq);
    REQUIRE(du.get_module_type() == "DecodeUnitTLM");
    du.tick();
    REQUIRE(true);
}

TEST_CASE("IssueUnitTLM API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    IssueUnitTLM iu("iu0", &eq);
    REQUIRE(iu.get_module_type() == "IssueUnitTLM");
    iu.tick();
    REQUIRE(true);
}

TEST_CASE("ScalarALU API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    ScalarALU sa("sa0", &eq);
    REQUIRE(sa.get_module_type() == "ScalarALU");
    sa.tick();
    REQUIRE(true);
}

TEST_CASE("VectorALU API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    VectorALU va("va0", &eq);
    REQUIRE(va.get_module_type() == "VectorALU");
    va.tick();
    REQUIRE(true);
}

TEST_CASE("MatrixCore API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    MatrixCore mc("mc0", &eq);
    REQUIRE(mc.get_module_type() == "MatrixCore");
    mc.tick();
    REQUIRE(true);
}

TEST_CASE("SIMTLane API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    SIMTLane sl("sl0", &eq);
    REQUIRE(sl.get_module_type() == "SIMTLane");
    sl.tick();
    REQUIRE(true);
}

TEST_CASE("LsuGlobal API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    LsuGlobal lg("lg0", &eq);
    REQUIRE(lg.get_module_type() == "LsuGlobal");
    lg.tick();
    REQUIRE(true);
}

TEST_CASE("LsuLDS API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    LsuLDS ll("ll0", &eq);
    REQUIRE(ll.get_module_type() == "LsuLDS");
    ll.tick();
    REQUIRE(true);
}

TEST_CASE("RegFileUnit API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    RegFileUnit rf("rf0", &eq);
    REQUIRE(rf.get_module_type() == "RegFileUnit");
    rf.tick();
    REQUIRE(true);
}

TEST_CASE("WritebackUnit API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    WritebackUnit wb("wb0", &eq);
    REQUIRE(wb.get_module_type() == "WritebackUnit");
    wb.tick();
    REQUIRE(true);
}

TEST_CASE("HazardTracker API contract", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    HazardTracker ht("ht0", &eq);
    REQUIRE(ht.get_module_type() == "HazardTracker");
    ht.tick();
    REQUIRE(true);
}

TEST_CASE("12 submodule classes are accessible in tlm::sm namespace", "[sm-unit][sm-microarch]") {
    EventQueue eq;
    auto fu = std::make_unique<FetchUnitTLM>("fu", &eq);
    auto du = std::make_unique<DecodeUnitTLM>("du", &eq);
    auto iu = std::make_unique<IssueUnitTLM>("iu", &eq);
    auto sa = std::make_unique<ScalarALU>("sa", &eq);
    auto va = std::make_unique<VectorALU>("va", &eq);
    auto mc = std::make_unique<MatrixCore>("mc", &eq);
    auto sl = std::make_unique<SIMTLane>("sl", &eq);
    auto lg = std::make_unique<LsuGlobal>("lg", &eq);
    auto ll = std::make_unique<LsuLDS>("ll", &eq);
    auto rf = std::make_unique<RegFileUnit>("rf", &eq);
    auto wb = std::make_unique<WritebackUnit>("wb", &eq);
    auto ht = std::make_unique<HazardTracker>("ht", &eq);
    REQUIRE(fu != nullptr);
    REQUIRE(du != nullptr);
    REQUIRE(iu != nullptr);
    REQUIRE(sa != nullptr);
    REQUIRE(va != nullptr);
    REQUIRE(mc != nullptr);
    REQUIRE(sl != nullptr);
    REQUIRE(lg != nullptr);
    REQUIRE(ll != nullptr);
    REQUIRE(rf != nullptr);
    REQUIRE(wb != nullptr);
    REQUIRE(ht != nullptr);
}
