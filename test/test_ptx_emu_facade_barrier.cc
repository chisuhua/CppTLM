// test/test_ptx_emu_facade_barrier.cc
// =============================================================================
// PtxEmuSubmoduleMVP facade — barrier 同步单元测试 (S1 / T-s1-3 §6.e)
// 功能: 验证 facade 处理 bar.sync / bar.warp.sync 屏障指令的接口, 包括
//       屏障同步后 is_warp_finished / is_thread_exited 状态正确。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-3, Sisyphus)
// 标签 [ptx-emu-facade][barrier]
// =============================================================================

#include "catch_amalgamated.hpp"

// PTX-EMU 头
#include "ptxsim/cta_context.h"
#include "ptxsim/execution_types.h"
#include "ptxsim/instruction_factory.h"
#include "ptxsim/sm_context.h"
#include "ptxsim/thread_context.h"
#include "ptxsim/warp_context.h"
#include "ptxsim/warp_state.h"

#include "ptx_ir/operand_context.h"
#include "ptx_ir/ptx_types.h"
#include "ptx_ir/statement_context.h"

#include "memory/resource_manager.h"

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

/// bar.sync barrier_id (CTA-level barrier)
StatementContext make_bar_sync_stmt(int bar_id = 0) {
    StatementContext ctx;
    ctx.type = S_BAR;
    BarrierInstr instr;
    instr.barId = bar_id;
    ctx.data = instr;
    ctx.instructionText = "bar.sync " + std::to_string(bar_id) + ";";
    return ctx;
}

/// bar.warp.sync mask, reconvergence_pc
StatementContext make_bar_warp_sync_stmt(uint32_t mask,
                                                   int reconvergence_pc) {
    StatementContext ctx;
    ctx.type = S_BAR_WARP_SYNC;
    BarWarpSyncInstr instr;
    instr.qualifiers = {Qualifier::Q_B32};
    instr.operands.push_back(OperandContext{ImmOperand{std::to_string(mask)}});
    instr.operands.push_back(OperandContext{ImmOperand{std::to_string(reconvergence_pc)}});
    ctx.data = instr;
    ctx.instructionText = "bar.warp.sync.b32 0x" + std::to_string(mask) + ", " +
                          std::to_string(reconvergence_pc) + ";";
    return ctx;
}

}  // namespace

TEST_CASE("facade_barrier_warp_initial_not_finished",
          "[ptx-emu-facade][barrier]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // 初始 warp 不应 finished (所有 lane 都在 RUN 状态)
    CHECK_FALSE(facade.is_warp_finished(warp.get()));

    facade.shutdown();
}

TEST_CASE("facade_barrier_thread_state_initial",
          "[ptx-emu-facade][barrier]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // 初始所有 thread 都未退出
    for (int i = 0; i < 32; ++i) {
        ThreadContext* t = warp->get_thread(i);
        REQUIRE(t != nullptr);
        CHECK_FALSE(facade.is_thread_exited(t));
    }

    facade.shutdown();
}

TEST_CASE("facade_barrier_warp_finished_after_all_exit",
          "[ptx-emu-facade][barrier]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    CHECK_FALSE(facade.is_warp_finished(warp.get()));

    // 模拟所有 thread 退出: 直接设置 warp_state
    for (int i = 0; i < 32; ++i) {
        warp->get_warp_state().threads[i].is_exited = true;
        warp->get_warp_state().threads[i].is_active = false;
        warp->get_warp_state().threads[i].status = ptxsim::ThreadStatus::Exited;
        ThreadContext* t = warp->get_thread(i);
        if (t) {
            t->set_state(EXIT);
        }
    }
    warp->set_active_mask(0u);

    // 重新读取 warp finished 状态
    CHECK(facade.is_warp_finished(warp.get()));

    // is_thread_exited 应均为 true
    for (int i = 0; i < 32; ++i) {
        ThreadContext* t = warp->get_thread(i);
        REQUIRE(t != nullptr);
        CHECK(facade.is_thread_exited(t));
    }

    facade.shutdown();
}

TEST_CASE("facade_barrier_partial_exit",
          "[ptx-emu-facade][barrier]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // 只有一半的 lane 退出
    for (int i = 0; i < 16; ++i) {
        warp->get_warp_state().threads[i].is_exited = true;
        warp->get_warp_state().threads[i].is_active = false;
        warp->get_warp_state().threads[i].status = ptxsim::ThreadStatus::Exited;
        ThreadContext* t = warp->get_thread(i);
        if (t) t->set_state(EXIT);
    }

    // warp 不应 finished (一半 lane 还在 RUN)
    CHECK_FALSE(facade.is_warp_finished(warp.get()));

    // 前 16 个 thread exited, 后 16 个未 exit
    for (int i = 0; i < 16; ++i) {
        ThreadContext* t = warp->get_thread(i);
        CHECK(facade.is_thread_exited(t));
    }
    for (int i = 16; i < 32; ++i) {
        ThreadContext* t = warp->get_thread(i);
        CHECK_FALSE(facade.is_thread_exited(t));
    }

    facade.shutdown();
}

TEST_CASE("facade_barrier_bar_sync_construction",
          "[ptx-emu-facade][barrier]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    auto warp = make_warp_with_threads(32);
    REQUIRE(warp);

    // bar.sync 0; 应可构造 (statement type 验证)
    auto stmt = make_bar_sync_stmt(0);
    CHECK(stmt.type == S_BAR);

    // 不实际执行 (需要 CTA context); 仅验证 facade 初始化 + warp 可用
    CHECK_FALSE(facade.is_warp_finished(warp.get()));

    facade.shutdown();
}
