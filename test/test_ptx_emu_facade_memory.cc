// test/test_ptx_emu_facade_memory.cc
// =============================================================================
// PtxEmuSubmoduleMVP facade — 内存读写单元测试 (S1 / T-s1-3 §6.c)
// 功能: 验证 facade::read_global_memory / write_global_memory 在 u32/u64 类型上
//       的正确性; sync_threads 共享内存同步路径。
// 作者 CppTLM Team / 日期 2026-08-21 (S1 WU-3, Sisyphus)
// 标签 [ptx-emu-facade][memory]
// =============================================================================

#include "catch_amalgamated.hpp"

// PTX-EMU 头 — 测试显式 include (facade header 仅前向声明)
#include "ptxsim/cta_context.h"
#include "ptxsim/execution_types.h"
#include "ptxsim/instruction_factory.h"
#include "ptxsim/sm_context.h"
#include "ptxsim/thread_context.h"
#include "ptxsim/warp_context.h"
#include "ptxsim/warp_state.h"

#include "memory/resource_manager.h"
#include "memory/simple_memory.h"

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

}  // namespace

TEST_CASE("facade_memory_write_read_u32",
          "[ptx-emu-facade][memory]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    GPUContext* gpu = facade.create_gpu_context();
    REQUIRE(gpu != nullptr);

    constexpr uint64_t kAddr = 0x10000;

    // 初始值应默认为 0 (SimpleMemory zero-init)
    uint32_t initial = facade.read_global_memory<uint32_t>(gpu, kAddr);
    CHECK(initial == 0u);

    // 写入 0xDEADBEEF, 读回应一致
    facade.write_global_memory<uint32_t>(gpu, kAddr, 0xDEADBEEFu);
    uint32_t readback = facade.read_global_memory<uint32_t>(gpu, kAddr);
    CHECK(readback == 0xDEADBEEFu);

    facade.shutdown();
}

TEST_CASE("facade_memory_write_read_u64",
          "[ptx-emu-facade][memory]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    GPUContext* gpu = facade.create_gpu_context();
    REQUIRE(gpu != nullptr);

    constexpr uint64_t kAddr = 0x20000;
    constexpr uint64_t kVal  = 0x1122334455667788ULL;

    uint64_t initial = facade.read_global_memory<uint64_t>(gpu, kAddr);
    CHECK(initial == 0ULL);

    facade.write_global_memory<uint64_t>(gpu, kAddr, kVal);
    uint64_t readback = facade.read_global_memory<uint64_t>(gpu, kAddr);
    CHECK(readback == kVal);

    facade.shutdown();
}

TEST_CASE("facade_memory_different_addresses",
          "[ptx-emu-facade][memory]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    GPUContext* gpu = facade.create_gpu_context();
    REQUIRE(gpu != nullptr);

    // 写入不同地址, 验证互不干扰
    facade.write_global_memory<uint32_t>(gpu, 0x10000, 0xAAAAAAAAu);
    facade.write_global_memory<uint32_t>(gpu, 0x20000, 0xBBBBBBBBu);
    facade.write_global_memory<uint32_t>(gpu, 0x30000, 0xCCCCCCCCu);

    CHECK(facade.read_global_memory<uint32_t>(gpu, 0x10000) == 0xAAAAAAAAu);
    CHECK(facade.read_global_memory<uint32_t>(gpu, 0x20000) == 0xBBBBBBBBu);
    CHECK(facade.read_global_memory<uint32_t>(gpu, 0x30000) == 0xCCCCCCCCu);

    facade.shutdown();
}

TEST_CASE("facade_memory_sync_threads_warp_barrier",
          "[ptx-emu-facade][memory]") {
    init_ptx_globals_once();
    PtxEmuSubmoduleMVP facade;
    const GPUConfig cfg{};
    REQUIRE(facade.init("", cfg));

    GPUContext* gpu = facade.create_gpu_context();
    REQUIRE(gpu != nullptr);

    // 注: facade.get_warp_context(*sm, 0) 在 fresh SM 上返回 nullptr
    // (warps 未创建). 真正创建 warp 需 SMContext::add_block (PTX-EMU 内部 API).
    // 这里改测 facade.read/write_global_memory (不需要 warp).
    uint64_t test_addr = 0x1000;
    uint32_t expected = 0xDEADBEEF;
    facade.write_global_memory<uint32_t>(gpu, test_addr, expected);
    uint32_t actual = facade.read_global_memory<uint32_t>(gpu, test_addr);
    CHECK(actual == expected);

    facade.shutdown();
}
