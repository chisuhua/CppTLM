// test/test_minimal_warp_scheduler_tlm.cc
// MinimalWarpSchedulerTLM 单元测试
// 作者 CppTLM Team / 日期 2026-06-30
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/minimal_warp_scheduler_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;

namespace {
    void registerChStreamModules() {
        static bool registered = false;
        if (!registered) {
            REGISTER_OBJECT;
            REGISTER_CHSTREAM;
            registered = true;
        }
    }
} // namespace

TEST_CASE("MinimalWarpSchedulerTLM.Defaults", "[warp_scheduler][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    MinimalWarpSchedulerTLM sched("sched0", &eq);

    REQUIRE(sched.get_module_type() == "MinimalWarpSchedulerTLM");
    REQUIRE(sched.all_warps_finished() == true);
}

TEST_CASE("MinimalWarpSchedulerTLM.AddWarp", "[warp_scheduler][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    MinimalWarpSchedulerTLM sched("sched0", &eq);

    sched.add_warp(0);
    sched.add_warp(1);
    sched.add_warp(2);

    REQUIRE(sched.all_warps_finished() == false);
}

TEST_CASE("MinimalWarpSchedulerTLM.ScheduleNext_RoundRobin", "[warp_scheduler][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    MinimalWarpSchedulerTLM sched("sched0", &eq);

    sched.add_warp(0);
    sched.add_warp(1);
    sched.add_warp(2);

    REQUIRE(sched.schedule_next() == 0);
    REQUIRE(sched.schedule_next() == 1);
    REQUIRE(sched.schedule_next() == 2);
    REQUIRE(sched.schedule_next() == 0); // wrap around
}

TEST_CASE("MinimalWarpSchedulerTLM.UpdateState_Blocked", "[warp_scheduler][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    MinimalWarpSchedulerTLM sched("sched0", &eq);

    sched.add_warp(0);
    sched.add_warp(1);

    sched.update_state(0, true, 2); // warp 0 blocked for 2 cycles

    // warp 0 blocked, should skip to warp 1
    REQUIRE(sched.schedule_next() == 1);
    REQUIRE(sched.schedule_next() == 1);

    sched.tick(); // decrement blocked cycles
    sched.tick();

    // warp 0 unblocked now
    REQUIRE(sched.schedule_next() == 0);
}

TEST_CASE("MinimalWarpSchedulerTLM.RemoveWarp", "[warp_scheduler][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    MinimalWarpSchedulerTLM sched("sched0", &eq);

    sched.add_warp(0);
    sched.add_warp(1);
    sched.remove_warp(0);

    REQUIRE(sched.schedule_next() == 1);
    REQUIRE(sched.schedule_next() == 1);
}