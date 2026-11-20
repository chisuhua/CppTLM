// test/test_cep_invariant_cen_ib8.cc
// `CEN-Ib8` 的判定级载体 —— 「address_resolve 的终结点**确实**在 engine 内」
//
// per `ADR-CEP-02 CEN-Ib8` + 判据文件
// `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md`
//     §2.4（Q-4 裁决）+ §8.2 C-1 / §9.2 SF-1（表达路径：复用既有 `Kind`/`Origin`，⛔ 不新增字段）
// 作者: CppTLM Team / 日期: 2026-11-20
//
// ── 判据纪律（本文件）────────────────────────────────────────────────
// ① **判定在 test 侧**（沿用 `stub_cache_endpoint.hh` 先例：实现侧只出原始事实）；
// ② 正控 + 负控**同款驱动、单差异**；违例数用**精确值**（⛔ 不用「> 0」）；
// ③ 负控必须命中「不经 engine 的 address_resolve」⇒ 该事务**在观测面无对应记录**。
#include "catch_amalgamated.hpp"

#include "core/event_queue.hh"
#include "tlm/cep/mesif_protocol_engine.hh"
#include "tlm/cep/protocol_engine_base.hh"
#include "tlm/cep/sharer_tracker.hh"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

    namespace cep = tlm::cep;
    using cep::EventObservation;

    constexpr std::uint64_t kAddrBase = 0x4000;

    /// 只开 AddressResolve 一条流（默认 `Support::flow` 全 false ⇒ `gate()` 会拒）
    void open_resolve_flow(cep::MesifProtocolEngine& e) {
        cep::MesifProtocolEngine::Support s{};
        s.flow[static_cast<std::size_t>(cep::CanonicalFlow::AddressResolve)] = true;
        e.set_support(s);
    }

    /// 判定结果（`CEN-Ib8` 的可断言量）
    struct ResolveVerdict {
        std::size_t resolves = 0;   ///< 被检查的 address_resolve 次数
        std::size_t recorded = 0;   ///< 其中「engine 内有恰一条合法终结点记录」的次数
        std::size_t violations = 0; ///< 违例：无记录 或 记录多于一条 或 Origin 非法
    };

    /// 逐次 `address_resolve` 断言：**engine 内有恰一条** `Kind = Transition` 且
    /// `Origin ∈ {LocalRequest, CompletionFill}` 的记录（per §2.4 判据写法）。
    ///
    /// ⚠️ 「无记录」与「记录重复」**都算违例** —— 后者会让「恰在 engine 内终结一次」
    ///    退化成「终结多次」，同样是 CEN-Ib8 的违例形态。
    ResolveVerdict check_resolves(const std::vector<EventObservation>& obs,
                                  const std::vector<std::uint64_t>& resolved) {
        ResolveVerdict v;
        for (const std::uint64_t addr : resolved) {
            ++v.resolves;
            std::size_t hits = 0;
            for (const auto& o : obs) {
                if (o.addr != addr || o.kind != EventObservation::Kind::Transition)
                    continue;
                if (o.origin == EventObservation::Origin::LocalRequest ||
                    o.origin == EventObservation::Origin::CompletionFill)
                    ++hits;
            }
            if (hits == 1u)
                ++v.recorded;
            else
                ++v.violations;
        }
        return v;
    }

} // namespace

// INV-CARRIER 标记置于**断言块**紧上方（判据 ⑤ 要求标记邻域 ±8 行含断言宏）
TEST_CASE("CEP invariant 批 1: CEN-Ib8 —— address_resolve 终结点在 engine 内可观测",
          "[cep][invariant-batch1][cen-ib8]") {
    EventQueue eq;
    cep::HierarchicalDirectory dir;
    cep::MesifProtocolEngine e("mesif_ib8", &eq);
    e.set_tracker(&dir, cep::kNodeIdSdma, /*level=*/0);
    open_resolve_flow(e);
    e.enable_observations(true);

    // ── 正控：3 次都在 engine 内终结 ⇒ 3 条终结点记录、0 违例 ──────────────
    std::vector<std::uint64_t> resolved;
    for (std::uint64_t i = 0; i < 3; ++i) {
        const std::uint64_t addr = kAddrBase + i * 0x40;
        cep::AddressResolveResult out{};
        e.on_address_resolve(addr, cep::kNodeIdVram, out);
        // 解析确实发生了（否则「有记录」可能只是恰好有别的记录）
        CHECK(out.home_node == cep::kNodeIdVram);
        CHECK(out.coherent == false);
        resolved.push_back(addr);
    }
    // INV-CARRIER: CEN-Ib8
    const ResolveVerdict ok = check_resolves(e.observations(), resolved);
    CHECK(ok.resolves == 3u);
    CHECK(ok.recorded == 3u);
    CHECK(ok.violations == 0u);

    // ── 负控（同款判定器 · 单差异）：追加一次**不经 engine** 的解析 ──────────
    //    该地址没有 engine 记录 ⇒ 违例**精确 == 1**（⛔ 不是「整体不全」）
    std::vector<std::uint64_t> with_ghost = resolved;
    with_ghost.push_back(kAddrBase + 0x1000); // 未调用 on_address_resolve
    const ResolveVerdict bad = check_resolves(e.observations(), with_ghost);
    CHECK(bad.resolves == 4u);
    CHECK(bad.recorded == 3u);
    CHECK(bad.violations == 1u);
}
