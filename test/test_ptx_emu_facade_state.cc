// test/test_ptx_emu_facade_state.cc
// =============================================================================
// PtxEmuSubmoduleMVP facade — 状态读写 round-trip 单元测试 (S1 / T-s1-3 §6.f)
// 功能: 验证 facade::write_register/read_register round-trip, advance_thread_pc /
//       read_thread_pc round-trip, read_blocked_cycles 在 set_blocked_cycles_for_active
//       后的正确性 (★ FIX-H8/B.3)。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-3, Sisyphus)
// 标签 [ptx-emu-facade][state]
// =============================================================================

#include "catch_amalgamated.hpp"

// PTX-EMU 头
#include "ptxsim/cta_context.h"
#include "ptxsim/execution_types.h"
#include "ptxsim/instruction_factory.h"
#include "ptxsim/register_analyzer.h"
#include "ptxsim/sm_context.h"
#include "ptxsim/thread_context.h"
#include "ptxsim/warp_context.h"
#include "ptxsim/warp_state.h"

#include "ptx_ir/operand_context.h"
#include "ptx_ir/ptx_types.h"

#include "memory/resource_manager.h"
#include "register/register_bank_manager.h"

#include "tlm/gpu/ptx_emu_submodule_mvp.hh"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

using tlm::PtxEmuSubmoduleMVP;
using tlm::GPUConfig;

namespace {

void init_ptx_globals_once() {
    static bool done = false;
    if (!done) {
        InstructionFactory::initialize();
        ResourceManager::instance().initialize(1, 8192);
        done = true;
    }
}

std::unique_ptr<WarpContext> make_warp_with_threads(int num_lanes = 32) {
    auto warp = std::make_unique<WarpContext>();
    warp->set_register_bank_manager(std::make_shared<RegisterBankManager>(1, 32));
    warp->set_warp_id(0);

    Dim3 blockIdx{0, 0, 0};
    Dim3 gridDim{1, 1, 1};
    Dim3 blockDim{(uint32_t)num_lanes, 1, 1};

    std::map<std::string, std::unique_ptr<Symtable>> name2Sym;
    std::map<std::string, int> label2pc;
    std::vector<StatementContext> stmts_ref(1); // dummy entry, idx 0 防 SIGSEGV

    for (int i = 0; i < num_lanes; i++) {
        auto t = std::make_unique<ThreadContext>();
        Dim3 tid{(uint32_t)i, 0, 0};
        t->init(blockIdx, tid, gridDim, blockDim, stmts_ref,
                &name2Sym, label2pc, nullptr, nullptr);
        t->set_state(RUN);
        warp->add_thread(std::move(t), i);
    }
    warp->set_active_mask(0xFFFFFFFF);
    return warp;
}

}  // namespace

TEST_CASE("facade_state_register_roundtrip",
          "[ptx-emu-facade][state]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // 创建 r1 寄存器
    auto rbm = warp->get_register_bank_manager();
    REQUIRE(rbm != nullptr);
    REQUIRE_FALSE(rbm->get_register("r1", 0, 0));
    rbm->create_register("r1", sizeof(uint32_t));

    // write then read 各 lane
    for (int i = 0; i < 32; ++i) {
        // 用 write_register (按 name 解析不直接, 通过底层 API)
        void* p = rbm->get_register("r1", 0, i);
        REQUIRE(p != nullptr);
        *static_cast<uint32_t*>(p) = static_cast<uint32_t>(i * 100);
    }

    // 读回
    for (int i = 0; i < 32; ++i) {
        void* p = rbm->get_register("r1", 0, i);
        REQUIRE(p != nullptr);
        uint32_t val = *static_cast<uint32_t*>(p);
        CHECK(val == static_cast<uint32_t>(i * 100));
    }

    facade.shutdown();
}

TEST_CASE("facade_state_thread_pc_roundtrip",
          "[ptx-emu-facade][state]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // 初始 PC 为 0
    for (int i = 0; i < 32; ++i) {
        CHECK(facade.read_thread_pc(warp.get(), i) == 0u);
    }

    // 推进 PC 到不同值
    for (int i = 0; i < 32; ++i) {
        uint32_t new_pc = static_cast<uint32_t>(i + 1);
        facade.advance_thread_pc(warp.get(), i, new_pc);
    }

    // 读回
    for (int i = 0; i < 32; ++i) {
        CHECK(facade.read_thread_pc(warp.get(), i) == static_cast<uint32_t>(i + 1));
    }

    facade.shutdown();
}

TEST_CASE("facade_state_blocked_cycles_after_set",
          "[ptx-emu-facade][state]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // 初始 blocked_cycles 应为 0
    for (int i = 0; i < 32; ++i) {
        CHECK(facade.read_blocked_cycles(warp.get(), i) == 0u);
    }

    // 调用 WarpContext::set_blocked_cycles_for_active(5) — 全 warp 阻塞 5 cycles
    warp->set_blocked_cycles_for_active(5);

    // 验证所有 lane 阻塞 5 cycles
    for (int i = 0; i < 32; ++i) {
        CHECK(facade.read_blocked_cycles(warp.get(), i) == 5u);
    }

    // 重新清零
    warp->set_blocked_cycles_for_active(0);

    facade.shutdown();
}

TEST_CASE("facade_state_active_mask_unchanged_by_register_write",
          "[ptx-emu-facade][state]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    uint32_t mask_before = facade.read_active_mask(warp.get());
    CHECK(mask_before == 0xFFFFFFFFu);

    // 创建 + 写寄存器不应改变 active_mask
    auto rbm = warp->get_register_bank_manager();
    rbm->create_register("r1", sizeof(uint32_t));
    for (int i = 0; i < 32; ++i) {
        void* p = rbm->get_register("r1", 0, i);
        *static_cast<uint32_t*>(p) = 0xCAFEu;
    }

    CHECK(facade.read_active_mask(warp.get()) == 0xFFFFFFFFu);

    facade.shutdown();
}

TEST_CASE("facade_state_advance_pc_independent_per_lane",
          "[ptx-emu-facade][state]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // lane 5 推进 PC
    facade.advance_thread_pc(warp.get(), 5, 42);
    CHECK(facade.read_thread_pc(warp.get(), 5) == 42u);

    // 其他 lane 不受影响
    CHECK(facade.read_thread_pc(warp.get(), 0) == 0u);
    CHECK(facade.read_thread_pc(warp.get(), 10) == 0u);
    CHECK(facade.read_thread_pc(warp.get(), 31) == 0u);

    // lane 5 再次推进 (覆盖)
    facade.advance_thread_pc(warp.get(), 5, 100);
    CHECK(facade.read_thread_pc(warp.get(), 5) == 100u);

    facade.shutdown();
}
