// test/test_vram_controller_tlm.cc
// VramControllerTLM 单元测试 (Phase 4 cpptlm-dgpu-soc-timing-mvp T4)
// 验证 design.md §5 + spec.md "VramControllerTLM":
//   1. 行缓冲 hit (同 row 连续访问)
//   2. 行缓冲 miss + row buffer 更新
//   3. bandwidth 上限 → 128B req 累加 4 cyc wait
//   4. invalidate_row_buffer() 后下次访问 row miss
//   5. on_config_loaded() 读 vram_size_bytes
#include <cstdint>
#include <cstring>
#include <vector>
#include "event_queue.hh"
#include "tlm/vram_controller_tlm.hh"
#include <catch2/catch_all.hpp>

namespace {

void inject_req(VramControllerTLM* vram, uint64_t tid, uint64_t addr, bool wr, uint64_t data = 0,
                uint8_t size = 8) {
    bundles::CacheReqBundle req;
    req.transaction_id.write(tid);
    req.address.write(addr);
    req.is_write.write(wr ? 1 : 0);
    req.data.write(data);
    req.size.write(size);
    vram->req_in().consume();
    std::memcpy(&vram->req_in().data(), &req, sizeof(req));
    vram->req_in().set_valid(true);
}

} // namespace

TEST_CASE("vram_ctrl: 行缓冲 hit (同 row 连续访问)", "[dgpu_soc_timing][vram_ctrl][timing]") {
    EventQueue eq;
    VramControllerTLM vram("vram", &eq);

    std::vector<uint8_t> backing(8192, 0);
    vram.set_backing_view(backing.data(), backing.size());
    // 首个访问 row miss (row buffer 初始 invalid), 后续同 row hit
    vram.set_timing_params(100, 200, 120, /*use_zero=*/false);
    vram.set_vram_params(/*row_hit=*/30, /*row_miss=*/80, /*bw_gbps=*/32);

    // access 1 @0x100 (row 0x0): row miss (initial invalid)
    inject_req(&vram, 1, 0x100, false, 0, 8);
    vram.tick();
    REQUIRE(vram.vram_row_misses() == 1);
    REQUIRE(vram.vram_row_hits() == 0);
    REQUIRE(vram.inflight_resp_count() == 1);
    // row buffer 更新到 row 0x0
    REQUIRE(vram.row_hit(0x200) == true);  // 同 row (0x0) hit

    // 推进到 resp 发出 (latency=80 miss)
    for (uint64_t c = 0; c < 79; ++c) {
        vram.advance_cycle();
        vram.tick();
    }
    vram.advance_cycle();  // cycle 80
    vram.tick();
    REQUIRE(vram.resp_out().valid());
    REQUIRE(vram.resp_out().data().transaction_id.read() == 1);
    vram.resp_out().clear_valid();

    // access 2 @0x200 (同 row 0x0): row hit → 30 cyc
    inject_req(&vram, 2, 0x200, false, 0, 8);
    vram.tick();
    REQUIRE(vram.vram_row_hits() == 1);
    for (uint64_t c = 0; c < 29; ++c) {
        vram.advance_cycle();
        vram.tick();
    }
    vram.advance_cycle();  // cycle 110 (80+30)
    vram.tick();
    REQUIRE(vram.resp_out().valid());
    REQUIRE(vram.resp_out().data().transaction_id.read() == 2);
}

TEST_CASE("vram_ctrl: 行缓冲 miss + row buffer 更新", "[dgpu_soc_timing][vram_ctrl][timing]") {
    EventQueue eq;
    VramControllerTLM vram("vram", &eq);

    std::vector<uint8_t> backing(8192, 0);
    vram.set_backing_view(backing.data(), backing.size());
    vram.set_timing_params(100, 200, 120, false);
    vram.set_vram_params(30, 80, 32);

    // row 0x0 miss → row buffer 更新
    inject_req(&vram, 1, 0x100, false, 0, 8);
    vram.tick();
    REQUIRE(vram.vram_row_misses() == 1);
    REQUIRE(vram.row_hit(0x100) == true);  // row buffer 现在指向 row 0x0

    // 不同 row 0x2000 → miss + 更新
    inject_req(&vram, 2, 0x2000, false, 0, 8);
    vram.tick();
    REQUIRE(vram.vram_row_misses() == 2);
    REQUIRE(vram.row_hit(0x2100) == true);   // 新 row 0x2000
    REQUIRE(vram.row_hit(0x100) == false);   // 旧 row 0x0 被替换
}

TEST_CASE("vram_ctrl: bandwidth 上限 → 128B req 累加 4 cyc wait", "[dgpu_soc_timing][vram_ctrl][timing]") {
    EventQueue eq;
    VramControllerTLM vram("vram", &eq);

    std::vector<uint8_t> backing(8192, 0);
    vram.set_backing_view(backing.data(), backing.size());
    vram.set_timing_params(100, 200, 120, false);
    vram.set_vram_params(30, 80, 32);  // 32 GB/s

    // 128B read: cycles_needed = ceil(128/32) = 4 → bandwidth wait 累加 1
    inject_req(&vram, 1, 0x100, false, 0, 128);
    vram.tick();
    REQUIRE(vram.bandwidth_limit_waits() == 1);
    // 1 个 req 产生 4 cyc 等待 (bw 上限: 128B/32B-per-cyc)
    // v0.1 简化: 仅计数 (后续 D4 加专门调度), 延迟本身由父类 latency 建模
    REQUIRE(vram.vram_row_misses() == 1);

    // 多个大 req 累加
    inject_req(&vram, 2, 0x200, false, 0, 128);
    vram.tick();
    REQUIRE(vram.bandwidth_limit_waits() == 2);
}

TEST_CASE("vram_ctrl: invalidate_row_buffer() 后下次访问 row miss", "[dgpu_soc_timing][vram_ctrl][timing]") {
    EventQueue eq;
    VramControllerTLM vram("vram", &eq);

    std::vector<uint8_t> backing(8192, 0);
    vram.set_backing_view(backing.data(), backing.size());
    vram.set_timing_params(100, 200, 120, false);
    vram.set_vram_params(30, 80, 32);

    // 首次访问 → miss + row buffer valid
    inject_req(&vram, 1, 0x100, false, 0, 8);
    vram.tick();
    REQUIRE(vram.vram_row_misses() == 1);
    REQUIRE(vram.row_hit(0x100) == true);

    // invalidate → 同 row 变成 miss
    vram.invalidate_row_buffer();
    REQUIRE(vram.row_hit(0x100) == false);

    // 再次访问同地址 → 重新 miss
    inject_req(&vram, 2, 0x100, false, 0, 8);
    vram.tick();
    REQUIRE(vram.vram_row_misses() == 2);
}

TEST_CASE("vram_ctrl: on_config_loaded() 读 vram_size_bytes", "[dgpu_soc_timing][vram_ctrl][timing]") {
    EventQueue eq;
    VramControllerTLM vram("vram", &eq);

    // config: vram_size_bytes=16MB → set_size_bytes 触发
    nlohmann::json cfg = {{"vram_size_bytes", 16ULL * 1024 * 1024}};
    vram.set_config(cfg);  // 触发 on_config_loaded

    // 验证 size_cap 生效: backing 2KB, size_cap 16MB → 读 addr 0x10000 (64KB) 仍在 cap 内
    std::vector<uint8_t> backing(2048, 0);
    vram.set_backing_view(backing.data(), backing.size());
    vram.set_timing_params(100, 200, 120, false);
    vram.set_vram_params(30, 80, 32);

    // 0x10000 < 16MB cap → 合法 (非 OUT_OF_RANGE)
    inject_req(&vram, 1, 0x10000, false, 0, 8);
    vram.tick();
    REQUIRE(vram.inflight_resp_count() == 1);
}
