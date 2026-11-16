// test/test_router_local_port_credit.cc
// 功能描述：RouterTLM 的 LOCAL 端口终端语义（ADR-X.16 D1：LOCAL 不参与 downstream
//           credit 流控）+ mesh 侧 credit 停滞可观测（D4）。对应 change
//           `cpptlm-router-local-port-credit-exhaustion` 的 AC-01 / AC-02 / AC-03 / AC-07。
// 作者 CppTLM Team / 日期 2026-11-17
#include "bundles/noc_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "tlm/router_tlm.hh"
#include <catch2/catch_all.hpp>
#include <cstdint>

using namespace tlm;
using FlitBundle = bundles::NoCFlitBundle;

namespace {

constexpr unsigned kLocal = static_cast<unsigned>(RouterPort::LOCAL);
constexpr unsigned kEast = static_cast<unsigned>(RouterPort::EAST);
constexpr unsigned kWest = static_cast<unsigned>(RouterPort::WEST);

/// 单 flit packet（HEAD_TAIL）—— 每 packet 唯一 transaction_id（`routing_table_` 以其为键）
FlitBundle make_flit(std::uint64_t txid, std::uint32_t dst, unsigned vc) {
    FlitBundle f;
    f.transaction_id.write(txid);
    f.src_node.write(0);
    f.dst_node.write(dst);
    f.vc_id.write(vc);
    f.flit_type.write(FlitBundle::FLIT_HEAD_TAIL);
    f.flit_index.write(0);
    f.flit_count.write(1);
    f.flit_category.write(FlitBundle::CATEGORY_REQUEST);
    return f;
}

/// 读 `credit_stall_cycles` 标量（跨 (port, vc) 累计的停滞周期数）
tlm_stats::Counter stall_cycles(RouterTLM& r) {
    for (auto& [n, stat] : r.stats().stats()) {
        if (n == "credit_stall_cycles") {
            if (auto* s = dynamic_cast<tlm_stats::Scalar*>(stat.get()))
                return s->value();
        }
    }
    return 0;
}

/// 每周期最多注入 1 个 flit（全局 1 flit/cycle）⇒ 输入 buffer 不会因超速而静默丢弃
bool try_inject(RouterTLM& r, unsigned port, unsigned& injected, unsigned total,
                std::uint64_t tx_base, std::uint32_t dst, unsigned vc) {
    if (injected >= total || r.req_in()[port].valid())
        return false;
    r.req_in()[port].data() = make_flit(tx_base + injected, dst, vc);
    r.req_in()[port].set_valid(true);
    ++injected;
    return true;
}

/// 读出一个出向 flit（读后立即 clear_valid —— `OutputStreamAdapter::write` 不清 valid）
bool try_drain(RouterTLM& r, unsigned port) {
    if (!r.resp_out()[port].valid())
        return false;
    r.resp_out()[port].clear_valid();
    return true;
}

} // namespace

TEST_CASE("RLC-1 (AC-01): LOCAL 端口 40 flit 全到 —— 无 8-flit 平台期",
          "[router][credit][local]") {
    // ADR-X.16 D1：LOCAL 不参与 downstream credit 流控 ⇒ 连续投递不受 BUFFER_DEPTH 限制。
    // 旧行为下第 9 个 flit 会永停（credit 不归还），本用例即为该缺陷的判别力载体。
    EventQueue eq;
    RouterTLM router("r00", &eq, 0, 0, 2, 2);

    constexpr unsigned kTotal = 40;
    unsigned injected = 0;
    unsigned drained = 0;
    for (unsigned cyc = 0; cyc < 400 && drained < kTotal; ++cyc) {
        try_inject(router, kLocal, injected, kTotal, 1000, /*dst=*/0, /*vc=*/0);
        router.tick();
        if (try_drain(router, kLocal))
            ++drained;
    }

    REQUIRE(injected == kTotal);
    REQUIRE(drained == kTotal);
    // AC-07：LOCAL 侧不误报 —— 全程无 credit 停滞
    REQUIRE(stall_cycles(router) == 0);
}

TEST_CASE("RLC-2 (AC-02): 4 条 VC 共 80 flit 全到", "[router][credit][local]") {
    // 4 个 in_vc 同时在飞（手写 vc_id 0..3）；out_vc 由 allocate_vc 决定。
    EventQueue eq;
    RouterTLM router("r00", &eq, 0, 0, 2, 2);

    constexpr unsigned kTotal = 80;
    unsigned injected = 0;
    unsigned drained = 0;
    for (unsigned cyc = 0; cyc < 600 && drained < kTotal; ++cyc) {
        try_inject(router, kLocal, injected, kTotal, 5000, /*dst=*/0,
                   /*vc=*/injected % RouterTLM::NUM_VCS);
        router.tick();
        if (try_drain(router, kLocal))
            ++drained;
    }

    REQUIRE(injected == kTotal);
    REQUIRE(drained == kTotal);
    // AC-07：4 VC 下 LOCAL 同样不产生 credit 停滞
    REQUIRE(stall_cycles(router) == 0);
}

TEST_CASE("RLC-3 (AC-03): mesh 向 credit 停滞可观测，且归还 credit 后停止增长",
          "[router][credit][local]") {
    // 裸 RouterTLM（adapter_ == nullptr ⇒ credit 永不归还）向 mesh 向（WEST → EAST）
    // 注入 ⇒ 8 个 flit 后 credit 耗尽 ⇒ `credit_stall_cycles` 累积。
    // 反向判据：每周期归还 1 个 credit ⇒ 停滞解除 ⇒ 计数停止增长（防「无脑 +1」的假实现）。
    EventQueue eq;
    RouterTLM router("r00", &eq, 0, 0, 2, 2);

    constexpr unsigned kTotal = 24;
    unsigned injected = 0;
    unsigned drained = 0;

    // ── 阶段 A：不归还 credit ────────────────────────────────────────
    for (unsigned cyc = 0; cyc < 80; ++cyc) {
        try_inject(router, kWest, injected, kTotal, 2000, /*dst=*/1, /*vc=*/0);
        router.tick();
        if (try_drain(router, kEast))
            ++drained;
    }
    const auto stalled = stall_cycles(router);
    REQUIRE(stalled >= 16); // AC-03 阈值（ADR-X.16 D4 / V-3）

    // ── 阶段 B：每周期归还 1 个 credit ───────────────────────────────
    const auto before = stall_cycles(router);
    for (unsigned cyc = 0; cyc < 40; ++cyc) {
        try_inject(router, kWest, injected, kTotal, 2000, /*dst=*/1, /*vc=*/0);
        router.receive_credit(kEast, 0);
        router.tick();
        if (try_drain(router, kEast))
            ++drained;
    }
    // 归还后停滞解除 ⇒ 计数不增长（严格相等，非「增速下降」）
    REQUIRE(stall_cycles(router) == before);
}

TEST_CASE("RLC-4: LOCAL 恒真 credit 的结构前提未被破坏", "[router][credit][local]") {
    // G-2 的结构契约：端口枚举与反向映射不变（LOCAL 是终端端口，reverse_port 自环）
    REQUIRE(static_cast<unsigned>(RouterPort::LOCAL) == 4u);
    REQUIRE(RouterTLM::NUM_PORTS == 5u);
    REQUIRE(RouterTLM::NUM_VCS == 4u);

    // `downstream_credits_` 的死状态处置是**注释性**的（方案 (a)）⇒ 此处只做可达性冒烟：
    // 即便 LOCAL 的 credit 被反复归还（超发也不越界），行为不受影响。
    EventQueue eq;
    RouterTLM router("r00", &eq, 0, 0, 2, 2);
    for (unsigned i = 0; i < 4u * RouterTLM::NUM_VCS; ++i)
        router.receive_credit(kLocal, i % RouterTLM::NUM_VCS);
    REQUIRE(stall_cycles(router) == 0);
}
