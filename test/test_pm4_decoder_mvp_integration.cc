// test_pm4_decoder_mvp_integration.cc
// CP + Pm4Decoder 集成测试 (per Oracle P1-b 修复 2026-08-28)
// Author: CppTLM Team
// Date: 2026-08-28
//
// 集成测试定义(per tasks L54 Oracle P1-b):
//   CommandProcessor + 真 Pm4Decoder + mock vram_reader + mock dispatch_target
//   **不**依赖 DGpuBoardTLM 装配,这样 T-s3-2 创建并跑通该测试。
//   T-s3-3 仅做 ctest 回归(per L101)。
//   DGpuBoardTLM 端到端 E2E 由 T-s3-3 的 test_tmu_dispatch_processor_tlm.cc + S3SubmitQueueHandler
//   覆盖。
//
// TDD: Step 1 (FAIL) - 预期编译失败 (set_vram_reader/set_dispatch_target 未实现)
#include "catch_amalgamated.hpp"
#include "tlm/gpu/command_processor_mvp.hh"
#include "tlm/gpu/pm4_decoder_mvp.hh"
#include "tlm/gpu/pm4_types_mvp.hh"
#include "tlm/gpu/tmu_types_mvp.hh"

#include <cstring>
#include <vector>

using tlm::gpu::CommandProcessor;
using tlm::gpu::Pm4Decoder;
using tlm::gpu::Pm4MethodType;
using tlm::gpu::TmuDispatchRecord;
using tlm::gpu::TmuSubmitResult;

namespace {

    uint32_t pack_header(uint32_t inc, uint32_t method_addr, uint32_t subchannel,
                         uint32_t data_count, uint32_t reserved) {
        return (inc & 0x1u) | ((method_addr & 0x7FFFu) << 1) | ((subchannel & 0xFu) << 16) |
               ((data_count & 0xFu) << 20) | ((reserved & 0xFFu) << 24);
    }

} // namespace

// CP + 真 Pm4Decoder: 验证 CP 经 DECODE 真实解析 Pm4MethodDispatch 并调 dispatch_target
TEST_CASE("CP+Pm4Decoder integration: DISPATCH_DIRECT fully resolves through DECODE",
          "[pm4-decoder-integration]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    // VRAM mock: 预置 DISPATCH_DIRECT (0x4001) header
    uint32_t packed = pack_header(0, 0x4001, 0, 3, 0);
    cp.set_vram_reader([packed](uint64_t va, void* out, size_t sz) {
        if (sz < sizeof(uint32_t))
            return -22;
        std::memcpy(out, &packed, sizeof(uint32_t));
        return 0;
    });

    // dispatch_target spy: 记录收到的 TmuDispatchRecord
    std::vector<TmuDispatchRecord> records;
    cp.set_dispatch_target([&records](const TmuDispatchRecord& rec) {
        records.push_back(rec);
        return TmuSubmitResult::SUBMITTED;
    });

    // 跑 5 transition
    cp.wake(); // IDLE → FETCH
    REQUIRE(cp.state() == CommandProcessor::State::FETCH);
    cp.tick(); // FETCH → DECODE (calls vram_reader)
    cp.tick(); // DECODE → DISPATCH (transition only)

    REQUIRE(records.size() == 0); // dispatch_target not yet called (next tick)

    cp.tick(); // DISPATCH → COMPLETE (calls dispatch_target)
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].task_id != 0);

    cp.tick(); // COMPLETE → IDLE
    REQUIRE(cp.state() == CommandProcessor::State::IDLE);

    REQUIRE(cp.cp_backoff_count() == 0);
    REQUIRE(cp.degraded() == false);
}

// CP + 真 Pm4Decoder: EVENT_WRITE 也走通完整路径
TEST_CASE("CP+Pm4Decoder integration: EVENT_WRITE passes through DECODE→DISPATCH",
          "[pm4-decoder-integration]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    uint32_t packed = pack_header(0, 0x4201, 1, 1, 0);
    cp.set_vram_reader([packed](uint64_t, void* out, size_t sz) {
        if (sz < sizeof(uint32_t))
            return -22;
        std::memcpy(out, &packed, sizeof(uint32_t));
        return 0;
    });

    int call_count = 0;
    cp.set_dispatch_target([&call_count](const TmuDispatchRecord&) {
        ++call_count;
        return TmuSubmitResult::SUBMITTED;
    });

    cp.wake();
    cp.tick();                // FETCH → DECODE
    cp.tick();                // DECODE → DISPATCH
    REQUIRE(call_count == 0); // dispatch_target not yet called
    cp.tick();                // DISPATCH → COMPLETE (calls dispatch_target)

    REQUIRE(call_count == 1);
}

// CP + 真 Pm4Decoder: UNKNOWN method_addr 跳过 DISPATCH
TEST_CASE("CP+Pm4Decoder integration: UNKNOWN method_addr skips dispatch",
          "[pm4-decoder-integration]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    uint32_t packed = pack_header(0, 0x5FFF, 0, 0, 0); // UNKNOWN range
    cp.set_vram_reader([packed](uint64_t, void* out, size_t sz) {
        if (sz < sizeof(uint32_t))
            return -22;
        std::memcpy(out, &packed, sizeof(uint32_t));
        return 0;
    });

    int call_count = 0;
    cp.set_dispatch_target([&call_count](const TmuDispatchRecord&) {
        ++call_count;
        return TmuSubmitResult::SUBMITTED;
    });

    cp.wake();
    cp.tick(); // FETCH → DECODE
    cp.tick(); // DECODE → COMPLETE (UNKNOWN → skip DISPATCH)
    REQUIRE(cp.state() == CommandProcessor::State::COMPLETE);

    REQUIRE(call_count == 0);
    REQUIRE(cp.cp_backoff_count() == 0);
}
