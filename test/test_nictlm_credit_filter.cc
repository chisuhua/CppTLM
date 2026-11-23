// test/test_nictlm_credit_filter.cc
// 功能描述：`NICTLM`（通用网卡）的网络 ingress **credit flit 过滤** —— `RouterTLM`
//           把 credit 回流**以 flit 形式**走在与数据**同一通道** `resp_out` 上
//           （bundle 字段 `flit_category == CATEGORY_CREDIT`），而 `reverse_port(LOCAL)
//           == LOCAL` ⇒ 经 LOCAL 口进入 router 的 flit，其 credit 沿 `resp_out[LOCAL]`
//           回流到挂在 LOCAL 上的下游消费者。`CohIfNICTLM` 侧**已修**（change
//           `cpptlm-router-credit-flit-leak-to-consumer`）；本文件验证 **`NICTLM` 侧**
//           的同款过滤（change `cpptlm-nictlm-credit-flit-filter`）。
//
// 作者 CppTLM Team / 日期 2026-11-17
#include <catch2/catch_all.hpp>

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "metrics/stats.hh"
#include "tlm/cep/cohif_nic.hh"
#include "tlm/nic_tlm.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

using json = nlohmann::json;
using Flit = bundles::NoCFlitBundle;

namespace {

    /// 读一个 Scalar 统计的值（找不到 ⇒ -1）—— 手法照既有 `test_noc_mesh_binding_semantics.cc`
    int64_t scalar_of(tlm_stats::StatGroup& g, const std::string& name) {
        auto* s = dynamic_cast<tlm_stats::Scalar*>(g.findStat(name));
        return s ? static_cast<int64_t>(s->value()) : -1;
    }

    /// 真响应 flit（`CATEGORY_RESPONSE` · `FLIT_HEAD_TAIL`）—— 走 `reassemble` 的
    /// `is_new_packet` 分支 ⇒ 产出恰好一条 `pe_resp_out` 响应
    Flit make_resp_flit(std::uint64_t tid, std::uint64_t data) {
        Flit f;
        f.transaction_id.write(tid);
        f.src_node.write(0);
        f.dst_node.write(1);
        f.address.write(0x1000);
        f.data.write(data);
        f.size.write(8);
        f.vc_id.write(0);
        f.flit_type.write(Flit::FLIT_HEAD_TAIL);
        f.flit_index.write(0);
        f.flit_count.write(1);
        f.hops.write(0);
        f.flit_category.write(Flit::CATEGORY_RESPONSE);
        f.is_ok.write(1);
        return f;
    }

    /// credit flit —— 形状照 `BundleDirectionTraits<NoCFlitBundle>::fill_credit`
    /// （`vc_id` + `flit_category = CATEGORY_CREDIT` + `flit_type = FLIT_HEAD_TAIL`）
    Flit make_credit_flit(std::uint64_t tid, std::uint8_t flit_type) {
        Flit f;
        f.transaction_id.write(tid);
        f.vc_id.write(0);
        f.flit_category.write(Flit::CATEGORY_CREDIT);
        f.flit_type.write(flit_type);
        f.flit_index.write(0);
        f.flit_count.write(1);
        return f;
    }

    /// 向 NICTLM 的网络 ingress 注入 1 个 flit（裸模块注入路径：
    /// `InputStreamAdapter::data()` + `set_valid(true)`，随后 `tick()` 消费）
    void inject(tlm::NICTLM& nic, const Flit& f) {
        nic.net_resp_in().data() = f;
        nic.net_resp_in().set_valid(true);
    }

    /// 向 CohIfNICTLM 的网络 ingress 注入 1 个 flit（与上面同款路径）
    void inject(tlm::cep::CohIfNICTLM& nic, const Flit& f) {
        nic.net_resp_in().data() = f;
        nic.net_resp_in().set_valid(true);
    }

} // namespace

// ────────────────────────────────────────────────────────────────────────────
// AC-01 —— **带判别力**的序列：先证明「过滤器在跑」（真响应 flit 走完全程），
//          再注入同 tid 的 credit ⇒ 三面**保持** + 丢弃计数 == 1。
//          ⛔ 不得写成「注入前计数为 0 ⇒ 注入后仍为 0」（空断言陷阱）。
// ────────────────────────────────────────────────────────────────────────────
TEST_CASE("NIC-CREDIT-1: 注入 credit flit ⇒ 计数 / 重组 / 业务流三面零变化",
          "[nictlm][credit][filter]") {
    EventQueue eq;
    tlm::NICTLM nic("nic", &eq, /*node_id=*/0, /*mesh_x=*/2, /*mesh_y=*/1);

    constexpr std::uint64_t kTid = 0x1234;

    // ① 基线：1 个真响应 flit ⇒ 过滤器「在跑」且数据面正常
    inject(nic, make_resp_flit(kTid, 0xdeadbeef));
    nic.tick();

    auto* g = nic.get_stats_group();
    REQUIRE(scalar_of(*g, "flits_received") == 1);
    REQUIRE(scalar_of(*g, "packets_received") == 1);
    REQUIRE(nic.pe_resp_out().valid());
    REQUIRE(nic.pe_resp_out().data().transaction_id.read() == kTid);
    nic.pe_resp_out().clear_valid();

    // ② 注入**同 tid** 的 credit flit ⇒ 三面保持 1/1/无输出 + 丢弃计数 == 1
    inject(nic, make_credit_flit(kTid, Flit::FLIT_HEAD_TAIL));
    nic.tick();

    REQUIRE(scalar_of(*g, "flits_received") == 1);   // 不变（credit 不计入）
    REQUIRE(scalar_of(*g, "packets_received") == 1); // 不变（credit 不触发重组）
    REQUIRE_FALSE(nic.pe_resp_out().valid());        // 不产出垃圾响应
    REQUIRE(nic.credits_dropped() == 1);             // 丢弃计数

    // ③ 观测面：stat group 暴露**同名**标量（与访问器同名）
    REQUIRE(nic.get_stats_group()->stats().count("credits_dropped") == 1);
    REQUIRE(scalar_of(*g, "credits_dropped") == 1);
}

// ────────────────────────────────────────────────────────────────────────────
// T2.1c —— credit flit 的**两种形状**都必须被丢弃：
//          `FLIT_HEAD_TAIL`（`fill_credit` 的真实形状）与 `FLIT_HEAD`（默认值形状，
//          在 `reassemble` 里走**不同分支** ⇒ 若实现只处理单一分支就会漏）。
// ────────────────────────────────────────────────────────────────────────────
TEST_CASE("NIC-CREDIT-2: credit flit 的两种 flit_type 形状都被丢弃", "[nictlm][credit][filter]") {
    EventQueue eq;
    tlm::NICTLM nic("nic", &eq, 0, 2, 1);
    auto* g = nic.get_stats_group();

    SECTION("真实形状 FLIT_HEAD_TAIL") {
        inject(nic, make_credit_flit(1, Flit::FLIT_HEAD_TAIL));
        nic.tick();
        REQUIRE(nic.credits_dropped() == 1);
        REQUIRE(scalar_of(*g, "flits_received") == 0);
        REQUIRE(scalar_of(*g, "packets_received") == 0);
        REQUIRE_FALSE(nic.pe_resp_out().valid());
    }

    SECTION("默认形状 FLIT_HEAD") {
        inject(nic, make_credit_flit(2, Flit::FLIT_HEAD));
        nic.tick();
        REQUIRE(nic.credits_dropped() == 1);
        REQUIRE(scalar_of(*g, "flits_received") == 0);
        REQUIRE(scalar_of(*g, "packets_received") == 0);
        REQUIRE_FALSE(nic.pe_resp_out().valid());
    }
}

// ────────────────────────────────────────────────────────────────────────────
// AC-06 —— 同 `transaction_id` 的在途包状态**不被 credit flit 劫持**。
//
// ⚠️ 口径说明（`NICTLM::reassemble` 的既有语义）：HEAD 类 flit（`FLIT_HEAD` /
//    `FLIT_HEAD_TAIL`）单独到达时走 `is_new_packet` 分支 ⇒ **立即**产出响应并
//    `erase` 条目 ⇒ NICTLM 侧**不存在跨周期的在途包**。因此本判据用「同 tid 的
//    第二个 HEAD 是否仍能产出**恰好一条**响应」作为「`pending_packets_` 未被 credit
//    留下条目」的**机器锚点**：若 credit 未被过滤，它会新建 tid 相同的条目 ⇒ 第二个
//    HEAD 将命中该条目而不走 `is_new_packet` 分支 ⇒ 产出 **0** 条响应。
// ────────────────────────────────────────────────────────────────────────────
TEST_CASE("NIC-CREDIT-3: 同 tid 的 credit flit 不劫持在途包状态", "[nictlm][credit][filter]") {
    EventQueue eq;
    tlm::NICTLM nic("nic", &eq, 0, 2, 1);
    auto* g = nic.get_stats_group();

    constexpr std::uint64_t kTid = 0x77;

    // ① 真 HEAD flit（tid=T）⇒ 既有语义：立即完成并产出响应 #1
    Flit head = make_resp_flit(kTid, 0x1);
    head.flit_type.write(Flit::FLIT_HEAD);
    inject(nic, head);
    nic.tick();
    REQUIRE(nic.pe_resp_out().valid());
    REQUIRE(nic.pe_resp_out().data().transaction_id.read() == kTid);
    nic.pe_resp_out().clear_valid();
    const int64_t flits_after_head = scalar_of(*g, "flits_received");
    REQUIRE(flits_after_head == 1);

    // ② 同 tid 的 credit flit ⇒ 不得产出任何响应、不得改写计数
    inject(nic, make_credit_flit(kTid, Flit::FLIT_HEAD_TAIL));
    nic.tick();
    REQUIRE_FALSE(nic.pe_resp_out().valid());
    REQUIRE(nic.credits_dropped() == 1);
    REQUIRE(scalar_of(*g, "flits_received") == flits_after_head);

    // ③ 同 tid 的第二个 HEAD ⇒ **恰好一条**响应（证明 credit 未留下在途包条目）
    Flit head2 = make_resp_flit(kTid, 0x2);
    head2.flit_type.write(Flit::FLIT_HEAD);
    inject(nic, head2);
    nic.tick();
    REQUIRE(nic.pe_resp_out().valid());
    REQUIRE(nic.pe_resp_out().data().transaction_id.read() == kTid);
    nic.pe_resp_out().clear_valid();
}

// ────────────────────────────────────────────────────────────────────────────
// AC-02 —— 端到端：**测试自建**最小「NIC → Router → NIC」带流量拓扑。
//
// ⚠️ ⛔ 不用 `configs/mesh_2x2_tlm.json`：其 4 个 NIC **全无 `address_regions`**
//    且**无 memory 模块** ⇒ `lookup_node()` 恒返回 0 ⇒「真实事务数」在该拓扑下
//    **不可定义**（该 config 仅作**装载回归**）。
//
// 拓扑（1 维 2 节点，mesh_x=2 mesh_y=1）：
//     cpu0 ──► ni0 ──req_out[1]──► r0(LOCAL) ══EAST══► r1(LOCAL) ◄── ni1
//     ni0.address_regions: {base:0x1000,size:0x1000,target_node:1} ⇒ dst_node=1
//
// 判据：
//   ① `ni0.credits_dropped() > 0` —— credit **确实抵达** ni0（避免 `0==0` 空断言）
//   ② `ni0.flits_received == 0` —— ni0 只应收到 credit ⇒ 修复前 > 0、修复后 == 0
//   ③ `ni1.flits_received == ni0.packets_sent` —— 真实事务数（零 credit 污染）
// ────────────────────────────────────────────────────────────────────────────
namespace {

    json build_e2e_topology() {
        json mods = json::array();
        mods.push_back({{"name", "cpu0"}, {"type", "CPUTLM"}});
        mods.push_back(
            {{"name", "ni0"},
             {"type", "NICTLM"},
             {"params",
              {{"node_id", 0},
               {"mesh_x", 2},
               {"mesh_y", 1},
               {"address_regions",
                json::array({{{"base", "0x1000"}, {"size", "0x1000"}, {"target_node", 1}}})}}}});
        mods.push_back({{"name", "ni1"},
                        {"type", "NICTLM"},
                        {"params", {{"node_id", 1}, {"mesh_x", 2}, {"mesh_y", 1}}}});
        mods.push_back({{"name", "r0"},
                        {"type", "RouterTLM"},
                        {"params", {{"node_x", 0}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 1}}}});
        mods.push_back({{"name", "r1"},
                        {"type", "RouterTLM"},
                        {"params", {{"node_x", 1}, {"node_y", 0}, {"mesh_x", 2}, {"mesh_y", 1}}}});

        json conns = json::array();
        auto add = [&](const char* s, const char* d) {
            conns.push_back({{"src", s}, {"dst", d}, {"latency", 1}, {"bandwidth", 100}});
        };
        add("cpu0", "ni0.0");
        add("ni0.0", "cpu0");
        add("ni0.1", "r0.4");
        add("r0.4", "ni0.1");
        add("r0.1", "r1.3");
        add("r1.3", "r0.1");
        add("r1.4", "ni1.1");
        add("ni1.1", "r1.4");

        return json{{"modules", mods}, {"connections", conns}};
    }

} // namespace

TEST_CASE("NIC-CREDIT-4: 端到端 —— credit 抵达 NIC 且被丢弃，计数零污染",
          "[nictlm][credit][filter][e2e]") {
    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(build_e2e_topology()));
    factory.startAllTicks();
    eq.run(300);

    auto* ni0 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni0"));
    auto* ni1 = dynamic_cast<tlm::NICTLM*>(factory.getInstance("ni1"));
    REQUIRE(ni0 != nullptr);
    REQUIRE(ni1 != nullptr);

    auto* g0 = ni0->get_stats_group();
    auto* g1 = ni1->get_stats_group();

    const int64_t ni0_sent = scalar_of(*g0, "packets_sent");
    const int64_t ni0_flits_recv = scalar_of(*g0, "flits_received");
    const int64_t ni0_credits_dropped = ni0->credits_dropped();
    const int64_t ni1_flits_recv = scalar_of(*g1, "flits_received");

    std::cout << "[NIC-CREDIT] ni0.sent=" << ni0_sent << " ni0.flits_received=" << ni0_flits_recv
              << " ni0.credits_dropped=" << ni0_credits_dropped
              << " ni1.flits_received=" << ni1_flits_recv << "\n";

    // 真实流量存在（否则下面全是空断言）
    REQUIRE(ni0_sent > 0);

    // ① credit **确实抵达** ni0（避免 `0==0` 空断言）
    REQUIRE(ni0_credits_dropped > 0);

    // ② ni0 只应收到 credit ⇒ 修复后 `flits_received` 必须为 0
    REQUIRE(ni0_flits_recv == 0);

    // ③ ni1 收到的 flit 数 == 真实事务数（零 credit 污染）
    REQUIRE(ni1_flits_recv == ni0_sent);
}

// ────────────────────────────────────────────────────────────────────────────
// AC-04 / G-1 —— 判据形态与 `CohIfNICTLM` 侧**一致**（两侧同用 bundle 字段
//          `flit_category == CATEGORY_CREDIT`；⛔ 不用 `Packet::type`）⇒ 同一
//          credit flit 注入两侧 ⇒ 丢弃行为**可对拍**。
// ────────────────────────────────────────────────────────────────────────────
TEST_CASE("NIC-CREDIT-5: 与 CohIfNICTLM 侧同款判据可对拍", "[nictlm][credit][filter][cohif]") {
    EventQueue eq;
    tlm::NICTLM nic("nic", &eq, 0, 2, 1);
    tlm::cep::CohIfNICTLM cohif("cohif", &eq);

    const Flit credit = make_credit_flit(0x99, Flit::FLIT_HEAD_TAIL);

    inject(nic, credit);
    nic.tick();

    inject(cohif, credit);
    cohif.tick();

    // 两侧：丢弃计数都是 1（同名同义的观测面）
    REQUIRE(nic.credits_dropped() == 1);
    REQUIRE(cohif.credits_dropped() == 1);

    // 两侧：都不产出业务流
    REQUIRE_FALSE(nic.pe_resp_out().valid());
    REQUIRE_FALSE(cohif.pe_resp_out().valid());

    // 两侧：`flits_received` 都不含 credit
    REQUIRE(scalar_of(*nic.get_stats_group(), "flits_received") == 0);
    REQUIRE(cohif.flits_received() == 0);

    // NICTLM 侧额外暴露 stat group 同名标量（CohIf 侧无 stat group 标量 ⇒ 只查 NICTLM）
    REQUIRE(nic.get_stats_group()->stats().count("credits_dropped") == 1);
}
