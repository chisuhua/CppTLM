// test/test_vector_regfile_tlm.cc
// VectorRegFileTLM 单元测试
// 作者 CppTLM Team / 日期 2026-06-30
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "modules.hh"
#include "tlm/gpu/vector_regfile_tlm.hh"
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

TEST_CASE("VectorRegFileTLM.Defaults", "[vector_regfile][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    VectorRegFileTLM vrf("vrf0", &eq);

    REQUIRE(vrf.get_num_regs() == 64);
    REQUIRE(vrf.get_num_banks() == 4);
    REQUIRE(vrf.get_module_type() == "VectorRegFileTLM");
}

TEST_CASE("VectorRegFileTLM.Setters", "[vector_regfile][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    VectorRegFileTLM vrf("vrf0", &eq);

    vrf.set_num_regs(128);
    vrf.set_num_banks(8);

    REQUIRE(vrf.get_num_regs() == 128);
    REQUIRE(vrf.get_num_banks() == 8);
}

TEST_CASE("VectorRegFileTLM.ReadWriteRoundTrip", "[vector_regfile][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    VectorRegFileTLM vrf("vrf0", &eq);

    vrf.write(0, 0, 0xDEADBEEF);
    REQUIRE(vrf.read(0, 0) == 0xDEADBEEF);

    vrf.write(1, 7, 0xCAFEBABE);
    REQUIRE(vrf.read(1, 7) == 0xCAFEBABE);
}

TEST_CASE("VectorRegFileTLM.BankConflict", "[vector_regfile][gpu][phase7b]") {
    registerChStreamModules();
    EventQueue eq;
    VectorRegFileTLM vrf("vrf0", &eq);

    // 4 banks, reg 0/4/8/12 map to bank 0 -> 4-way conflict
    std::vector<uint32_t> regs = {0, 4, 8, 12};
    REQUIRE(vrf.bank_conflict_cycles(regs) == 4);

    // different bank -> 1 cycle
    std::vector<uint32_t> regs2 = {0, 1, 2, 3};
    REQUIRE(vrf.bank_conflict_cycles(regs2) == 1);
}
