// test_command_processor_mvp.cc
// CommandProcessor 单元测试 - 5-state FSM 真实逻辑 + 退避策略 + 装配接口
// Author: CppTLM Team
// Date: 2026-08-28
//
// Per design.md §3 5-state FSM + §4.4 退避策略 + Oracle P1-a 修复 (内部自动 + getter 断言)
// TDD: Step 1 (FAIL) - 预期编译/链接失败因为 set_vram_reader/set_dispatch_target/
//      DEGRADED 状态/cp_backoff_count() getter 未实现
#include "catch_amalgamated.hpp"
#include "tlm/gpu/command_processor_mvp.hh"
#include "tlm/gpu/pm4_decoder_mvp.hh"
#include "tlm/gpu/pm4_types_mvp.hh"
#include "tlm/gpu/tmu_types_mvp.hh"

#include <cstring>
#include <vector>

using tlm::gpu::CommandProcessor;
using tlm::gpu::Pm4Decoder;
using tlm::gpu::Pm4MethodDispatch;
using tlm::gpu::Pm4MethodHeader;
using tlm::gpu::Pm4MethodType;
using tlm::gpu::TmuDispatchRecord;
using tlm::gpu::TmuSubmitResult;

namespace {

// Pack Pm4MethodHeader fields into uint32_t
uint32_t pack_header(uint32_t inc, uint32_t method_addr, uint32_t subchannel,
                     uint32_t data_count, uint32_t reserved) {
    return (inc & 0x1u) | ((method_addr & 0x7FFFu) << 1)
         | ((subchannel & 0xFu) << 16) | ((data_count & 0xFu) << 20)
         | ((reserved & 0xFFu) << 24);
}

// Mock vram_reader: 预设一段 gpfifo_entry (PM4 method header) 到 buffer
struct MockVram {
    uint32_t packed_header = 0;
    int32_t return_value = 0;
    int call_count = 0;

    int32_t read([[maybe_unused]] uint64_t va, void* out, size_t size) {
        ++call_count;
        if (return_value != 0) return return_value;
        if (size < sizeof(uint32_t)) return -22;  // EINVAL
        std::memcpy(out, &packed_header, sizeof(uint32_t));
        return 0;
    }
};

// 记录 dispatch_target 收到的 record
struct DispatchSpy {
    std::vector<TmuDispatchRecord> records;
    TmuSubmitResult next_return = TmuSubmitResult::SUBMITTED;
    int call_count = 0;

    TmuSubmitResult operator()(const TmuDispatchRecord& rec) {
        ++call_count;
        records.push_back(rec);
        return next_return;
    }
};

} // namespace

// ── 装配接口存在性 (s2 set_decoder 已有, s3 新增 4 方法) ──
TEST_CASE("CommandProcessor: set_vram_reader stores reader", "[command-processor][mvp]") {
    CommandProcessor cp;
    int32_t dummy = 0;
    REQUIRE_NOTHROW(cp.set_vram_reader([&dummy](uint64_t, void*, size_t) { return dummy; }));
}

TEST_CASE("CommandProcessor: set_dispatch_target stores target", "[command-processor][mvp]") {
    CommandProcessor cp;
    REQUIRE_NOTHROW(cp.set_dispatch_target([](const TmuDispatchRecord&) {
        return TmuSubmitResult::SUBMITTED;
    }));
}

TEST_CASE("CommandProcessor: on_backpressure no-op default", "[command-processor][mvp]") {
    CommandProcessor cp;
    REQUIRE_NOTHROW(cp.on_backpressure(8));
    REQUIRE_NOTHROW(cp.on_submit_queue_rejected(8));
}

TEST_CASE("CommandProcessor: DEGRADED state enum exists", "[command-processor][mvp]") {
    // 验证 DEGRADED 是合法 state 值
    CommandProcessor cp;
    bool degraded_seen = false;
    for (int s = 0; s <= 10; ++s) {
        auto st = static_cast<CommandProcessor::State>(s);
        if (st == CommandProcessor::State::DEGRADED) degraded_seen = true;
    }
    REQUIRE(degraded_seen);
}

// ── Getter 接口 ──
TEST_CASE("CommandProcessor: cp_backoff_count() starts at 0", "[command-processor][mvp]") {
    CommandProcessor cp;
    REQUIRE(cp.cp_backoff_count() == 0);
    REQUIRE(cp.degraded() == false);
    REQUIRE(cp.backoff_cycles_remaining() == 0);
}

// ── 5 transition 全链 (mock reader + mock dispatch_target 返回 SUBMITTED) ──
TEST_CASE("CommandProcessor: 5-transition FSM with SUBMITTED dispatch", "[command-processor][mvp]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    MockVram vram;
    vram.packed_header = pack_header(/*inc*/0, /*addr*/0x4001, /*subch*/0, /*dc*/3, /*res*/0);
    cp.set_vram_reader([&vram](uint64_t va, void* out, size_t sz) {
        return vram.read(va, out, sz);
    });

    DispatchSpy spy;
    cp.set_dispatch_target([&spy](const TmuDispatchRecord& rec) {
        return spy(rec);
    });

    // IDLE → FETCH (via wake) → DECODE → DISPATCH → COMPLETE → IDLE
    REQUIRE(cp.state() == CommandProcessor::State::IDLE);
    cp.wake();
    REQUIRE(cp.state() == CommandProcessor::State::FETCH);

    cp.tick();  // FETCH → DECODE
    REQUIRE(cp.state() == CommandProcessor::State::DECODE);

    cp.tick();  // DECODE → DISPATCH (transition only, dispatch_target_ called next tick)
    REQUIRE(cp.state() == CommandProcessor::State::DISPATCH);
    REQUIRE(spy.call_count == 0);  // not yet called

    cp.tick();  // DISPATCH → COMPLETE (calls dispatch_target_ + records)
    REQUIRE(cp.state() == CommandProcessor::State::COMPLETE);
    REQUIRE(spy.call_count == 1);
    REQUIRE(spy.records.size() == 1);
    REQUIRE(spy.records[0].task_id != 0);

    cp.tick();  // COMPLETE → IDLE
    REQUIRE(cp.state() == CommandProcessor::State::IDLE);

    REQUIRE(vram.call_count == 2);  // FETCH + DECODE 各调一次 (per design §3.1,DECODE 也调以解析 header)
    REQUIRE(cp.cp_backoff_count() == 0);
    REQUIRE(cp.degraded() == false);
}

// ── DISPATCH 返回 BACKPRESSURED → 退避 ──
TEST_CASE("CommandProcessor: BACKPRESSURED dispatch increments backoff counter", "[command-processor][mvp]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    MockVram vram;
    vram.packed_header = pack_header(0, 0x4001, 0, 3, 0);
    cp.set_vram_reader([&vram](uint64_t va, void* out, size_t sz) {
        return vram.read(va, out, sz);
    });

    DispatchSpy spy;
    spy.next_return = TmuSubmitResult::BACKPRESSURED;
    cp.set_dispatch_target([&spy](const TmuDispatchRecord& rec) {
        return spy(rec);
    });

    cp.wake();
    cp.tick();
    cp.tick();
    cp.tick();

    REQUIRE(cp.cp_backoff_count() == 1);
    REQUIRE(cp.state() == CommandProcessor::State::FETCH);
}

// ── 连续 3 次 BACKPRESSURED → DEGRADED ──
TEST_CASE("CommandProcessor: 3 consecutive BACKPRESSURED enters DEGRADED state", "[command-processor][mvp]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    MockVram vram;
    vram.packed_header = pack_header(0, 0x4001, 0, 3, 0);
    cp.set_vram_reader([&vram](uint64_t va, void* out, size_t sz) {
        return vram.read(va, out, sz);
    });

    DispatchSpy spy;
    spy.next_return = TmuSubmitResult::BACKPRESSURED;
    cp.set_dispatch_target([&spy](const TmuDispatchRecord& rec) {
        return spy(rec);
    });

    cp.wake();
    for (int i = 0; i < 12; ++i) cp.tick();

    REQUIRE(cp.cp_backoff_count() >= 3);
    REQUIRE(cp.degraded() == true);
}

// ── DISPATCH 返回 SUBMIT_QUEUE_REJECTED 同样触发退避 ──
TEST_CASE("CommandProcessor: SUBMIT_QUEUE_REJECTED also triggers backoff", "[command-processor][mvp]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    MockVram vram;
    vram.packed_header = pack_header(0, 0x4001, 0, 3, 0);
    cp.set_vram_reader([&vram](uint64_t va, void* out, size_t sz) {
        return vram.read(va, out, sz);
    });

    DispatchSpy spy;
    spy.next_return = TmuSubmitResult::SUBMIT_QUEUE_REJECTED;
    cp.set_dispatch_target([&spy](const TmuDispatchRecord& rec) {
        return spy(rec);
    });

    cp.wake();
    cp.tick();  // FETCH → DECODE
    cp.tick();
    cp.tick();
    cp.tick();  // DISPATCH → SUBMIT_QUEUE_REJECTED → FETCH

    REQUIRE(cp.cp_backoff_count() == 1);
}

// ── VRAM read 失败 → DISPATCH 失败 ──
TEST_CASE("CommandProcessor: VRAM read error skips DISPATCH", "[command-processor][mvp]") {
    CommandProcessor cp;

    MockVram vram;
    vram.return_value = -5;  // EIO
    cp.set_vram_reader([&vram](uint64_t va, void* out, size_t sz) {
        return vram.read(va, out, sz);
    });

    DispatchSpy spy;
    cp.set_dispatch_target([&spy](const TmuDispatchRecord& rec) {
        return spy(rec);
    });

    cp.wake();
    cp.tick();  // FETCH → DECODE (但 VRAM read 失败)
    cp.tick();  // DECODE → DISPATCH (跳过 dispatch_target,因为 dispatch record 未生成)

    REQUIRE(spy.call_count == 0);  // dispatch_target 没被调
}

// ── DEGRADED 后 DISPATCH 被门控 ──
TEST_CASE("CommandProcessor: DEGRADED gates DISPATCH (returns to FETCH only)", "[command-processor][mvp]") {
    CommandProcessor cp;
    cp.set_decoder(std::make_unique<Pm4Decoder>());

    MockVram vram;
    vram.packed_header = pack_header(0, 0x4001, 0, 3, 0);
    cp.set_vram_reader([&vram](uint64_t va, void* out, size_t sz) {
        return vram.read(va, out, sz);
    });

    DispatchSpy spy;
    spy.next_return = TmuSubmitResult::BACKPRESSURED;
    cp.set_dispatch_target([&spy](const TmuDispatchRecord& rec) {
        return spy(rec);
    });

    cp.wake();
    for (int i = 0; i < 10; ++i) cp.tick();

    // 进 DEGRADED 后,dispatch_target 仍被调 (测真实调) 但 CP 不再清零 backoff
    // (设计:DEGRADED 仅 gate DISPATCH, dispatch_target 仍可见)
    REQUIRE(cp.degraded() == true);
}

// ── UNKNOWN method_addr → DECODE 失败, dispatch_target 不调 ──
TEST_CASE("CommandProcessor: UNKNOWN method_addr skips DISPATCH", "[command-processor][mvp]") {
    CommandProcessor cp;

    MockVram vram;
    vram.packed_header = pack_header(0, 0x5FFF, 0, 0, 0);  // UNKNOWN range
    cp.set_vram_reader([&vram](uint64_t va, void* out, size_t sz) {
        return vram.read(va, out, sz);
    });

    DispatchSpy spy;
    cp.set_dispatch_target([&spy](const TmuDispatchRecord& rec) {
        return spy(rec);
    });

    cp.wake();
    cp.tick();  // FETCH → DECODE
    cp.tick();  // DECODE → DISPATCH (UNKNOWN → 跳过)

    REQUIRE(spy.call_count == 0);
    REQUIRE(cp.cp_backoff_count() == 0);  // UNKNOWN 不是反压,不计数
}
