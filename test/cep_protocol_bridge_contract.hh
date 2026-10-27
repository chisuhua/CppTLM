// test/cep_protocol_bridge_contract.hh
// ProtocolBridge 契约的**可复用判据函数**(per `cpptlm-cep-cohif-carrier` Phase 3)
//
// 功能:把 CC-03 / CC-05 的判据实现成**独立函数**,使反向 fixture 能够:
//       ① 在**不**跑完整 `instantiateAll` 的前提下断言「契约违反 ⇒ 判红」
//       ② 断言判红是**因正确原因**红(返回的 `mismatched` / `violations`
//          精确指出被篡改的字段)
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 与 Phase 1 / Phase 2 的既有模式一致 ──────────────────────────────
//   `test/cep_cohif_contract.hh`(Phase 1)+ `test/cep_protocol_engine_contract.hh`
//   (Phase 2)都是「共享判据 + 毒化副本生成器」;本文件是同一模式的第三份。
#ifndef CEP_PROTOCOL_BRIDGE_CONTRACT_HH
#define CEP_PROTOCOL_BRIDGE_CONTRACT_HH

#include "bundles/cep_coherence_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "core/packet_pool.hh"

#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpptlm {
namespace cep {

// ══════════════════════════════════════════════════════════════════════
//  毒化副本生成器(reverse fixture 的载体)
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ **锚点缺失必须抛异常**(不得静默返回原文)—— 否则反向验证会**假绿**
//    (毒化没生效 ⇒ 判据仍绿 ⇒ 脚本误判为「毒化有效」)。
enum class BridgePoison {
    PermToCategory,  ///< 把 `perm` 映射到 `flit_category`(task 3.9 的例子)
    SwapAddrData,    ///< 把 `address` 与 `data` 对调
};

inline const char* bridge_poison_name(BridgePoison p) {
    switch (p) {
        case BridgePoison::PermToCategory: return "perm_to_category";
        case BridgePoison::SwapAddrData: return "swap_addr_data";
    }
    return "unknown";
}

/// 生成毒化副本 —— 锚点缺失**抛异常**
inline std::string poison_cohif_bridge(const std::string& src, BridgePoison which) {
    std::string out = src;
    // ⚠️ 用**子串查找**而非 regex —— 锚点含 `(` `)` 等 regex 元字符,
    //    当 regex 用会失配(实测踩过:锚点「找不到」)。
    const std::string anchor = "out.flit_category.write(category_of(in.flow_id));";
    const std::size_t pos = out.find(anchor);
    if (pos == std::string::npos) {
        throw std::runtime_error("毒化副本生成失败 —— 锚点未找到:" + anchor);
    }
    const std::size_t len = anchor.size();
    std::string injected;
    switch (which) {
        case BridgePoison::PermToCategory:
            injected = anchor + "\n        out.flit_category.write(in.perm.read() & 0x3);  // 毒化";
            break;
        case BridgePoison::SwapAddrData:
            injected = anchor +
                       "\n        out.address.write(in.data.read());  // 毒化\n"
                       "        out.data.write(in.address.read());  // 毒化";
            break;
    }
    out.replace(pos, len, injected);
    return out;
}

/// 全部毒化种类(供脚本遍历)
inline std::vector<BridgePoison> bridge_all_poisons() {
    return {BridgePoison::PermToCategory, BridgePoison::SwapAddrData};
}

// ══════════════════════════════════════════════════════════════════════
//  通用比较辅助
// ══════════════════════════════════════════════════════════════════════

/// `NoCFlitBundle` 的**逐字段**相等(不用 `memcmp` —— `ch_uint` 有 padding,
/// `memcmp` 可能因未初始化 padding 误判)
inline bool noc_flit_equal(const bundles::NoCFlitBundle& a, const bundles::NoCFlitBundle& b) {
    return a.transaction_id.read() == b.transaction_id.read() &&
           a.src_node.read() == b.src_node.read() && a.dst_node.read() == b.dst_node.read() &&
           a.address.read() == b.address.read() && a.data.read() == b.data.read() &&
           a.size.read() == b.size.read() && a.vc_id.read() == b.vc_id.read() &&
           a.flit_type.read() == b.flit_type.read() && a.flit_index.read() == b.flit_index.read() &&
           a.flit_count.read() == b.flit_count.read() && a.hops.read() == b.hops.read() &&
           a.flit_category.read() == b.flit_category.read() &&
           a.src_port.read() == b.src_port.read() && a.is_write.read() == b.is_write.read() &&
           a.is_ok.read() == b.is_ok.read() && a.error_code.read() == b.error_code.read();
}

/// `CohIfBundle` 的**逐字段**相等(12 字段全覆盖)
inline bool cohif_equal(const bundles::CohIfBundle& a, const bundles::CohIfBundle& b) {
    return a.flow_id == b.flow_id && a.transaction_id.read() == b.transaction_id.read() &&
           a.address.read() == b.address.read() && a.data.read() == b.data.read() &&
           a.size.read() == b.size.read() && a.perm.read() == b.perm.read() &&
           a.snoop_type.read() == b.snoop_type.read() &&
           a.line_state.read() == b.line_state.read() && a.hit.read() == b.hit.read() &&
           a.dirty.read() == b.dirty.read() && a.error.read() == b.error.read() &&
           a.atomic.read() == b.atomic.read();
}

/// `CohIfBundle` 的**默认态**(NSDMI)—— 「层 1 丢弃字段复位」的期望值
inline bundles::CohIfBundle cohif_default() { return bundles::CohIfBundle(); }

// ══════════════════════════════════════════════════════════════════════
//  判据 ①:内容层 round-trip(CC-03 ① 的**内容层**)
// ══════════════════════════════════════════════════════════════════════
//
// 【层 1】`unpack(pack(x))` 的期望形态(per B′ 方案):
//   · **共有字段**(4 个)逐字段相等
//   · **丢弃字段**(8 个)复位为默认值 —— 明确「层 1 不携带协议语义」
//   · **网络封装字段**不被桥改写(职责边界,per CC-03 ②)
struct ContentRoundTrip {
    bool ok = true;
    std::vector<std::string> mismatched;  ///< 共有字段中不相等的字段名
    std::vector<std::string> not_reset;    ///< 未复位为默认值的丢弃字段名
    std::vector<std::string> touched_net;  ///< 被桥越权改写的网络封装字段名
};

template <typename BridgeT>
ContentRoundTrip check_content_round_trip(BridgeT& bridge, const bundles::CohIfBundle& in) {
    ContentRoundTrip r;

    bundles::NoCFlitBundle flit;
    // 预置网络封装字段 —— 桥**不得**改写它们
    flit.src_node.write(0x1122);
    flit.dst_node.write(0x3344);
    flit.vc_id.write(7);
    flit.flit_type.write(bundles::NoCFlitBundle::FLIT_HEAD);
    flit.flit_index.write(2);
    flit.flit_count.write(5);
    flit.hops.write(3);
    flit.src_port.write(4);
    flit.is_write.write(1);
    flit.is_ok.write(0);
    flit.error_code.write(9);

    bridge.pack(in, flit);

    // 职责边界:网络封装字段必须原样
    if (flit.src_node.read() != 0x1122)
        r.touched_net.push_back("src_node");
    if (flit.dst_node.read() != 0x3344)
        r.touched_net.push_back("dst_node");
    if (flit.vc_id.read() != 7)
        r.touched_net.push_back("vc_id");
    if (flit.flit_type.read() != bundles::NoCFlitBundle::FLIT_HEAD)
        r.touched_net.push_back("flit_type");
    if (flit.flit_index.read() != 2)
        r.touched_net.push_back("flit_index");
    if (flit.flit_count.read() != 5)
        r.touched_net.push_back("flit_count");
    if (flit.hops.read() != 3)
        r.touched_net.push_back("hops");
    if (flit.src_port.read() != 4)
        r.touched_net.push_back("src_port");
    if (flit.is_write.read() != 1)
        r.touched_net.push_back("is_write");
    if (flit.is_ok.read() != 0)
        r.touched_net.push_back("is_ok");
    if (flit.error_code.read() != 9)
        r.touched_net.push_back("error_code");

    bundles::CohIfBundle out;
    bridge.unpack(flit, out);

    // 共有字段逐字段相等
    if (out.transaction_id.read() != in.transaction_id.read())
        r.mismatched.push_back("transaction_id");
    if (out.address.read() != in.address.read())
        r.mismatched.push_back("address");
    if (out.data.read() != in.data.read())
        r.mismatched.push_back("data");
    if (out.size.read() != in.size.read())
        r.mismatched.push_back("size");

    // 丢弃字段复位为默认值
    const bundles::CohIfBundle d = cohif_default();
    if (out.flow_id != d.flow_id)
        r.not_reset.push_back("flow_id");
    if (out.perm.read() != d.perm.read())
        r.not_reset.push_back("perm");
    if (out.snoop_type.read() != d.snoop_type.read())
        r.not_reset.push_back("snoop_type");
    if (out.line_state.read() != d.line_state.read())
        r.not_reset.push_back("line_state");
    if (out.hit.read() != d.hit.read())
        r.not_reset.push_back("hit");
    if (out.dirty.read() != d.dirty.read())
        r.not_reset.push_back("dirty");
    if (out.error.read() != d.error.read())
        r.not_reset.push_back("error");
    if (out.atomic.read() != d.atomic.read())
        r.not_reset.push_back("atomic");

    r.ok = r.mismatched.empty() && r.not_reset.empty() && r.touched_net.empty();
    return r;
}

// ══════════════════════════════════════════════════════════════════════
//  判据 ②:端到端 round-trip(CC-03 ① 的**端到端层**)
// ══════════════════════════════════════════════════════════════════════
//
// 【层 2】`Packet` 的 payload 数据区承载**完整** `CohIfBundle` ⇒ 12 字段
//         逐字段相等(端到端不透损 —— 这才是「CohIf 语义跨网络无损」的主张)
struct EndToEndRoundTrip {
    bool ok = true;
    std::vector<std::string> mismatched;  ///< 不相等的字段名
    bool payload_usable = false;          ///< payload 是否可用于层 2 往返
};

template <typename BridgeT>
EndToEndRoundTrip check_end_to_end_round_trip(BridgeT& bridge,
                                               const bundles::CohIfBundle& in) {
    EndToEndRoundTrip r;
    Packet* pkt = PacketPool::get().acquire_with_min_size(sizeof(bundles::CohIfBundle));
    if (!pkt || !pkt->payload) {
        PacketPool::get().release(pkt);
        return r;
    }
    if (!BridgeT::write_payload_inb(pkt, in)) {
        PacketPool::get().release(pkt);
        return r;
    }
    r.payload_usable = true;

    bridge.pack_with_packet(pkt);
    bridge.unpack_with_packet(pkt);

    bundles::CohIfBundle out;
    if (!BridgeT::read_payload_inb(pkt, out)) {
        r.mismatched.push_back("<payload 读取失败>");
        PacketPool::get().release(pkt);
        return r;
    }
    PacketPool::get().release(pkt);

    if (out.flow_id != in.flow_id)
        r.mismatched.push_back("flow_id");
    if (out.transaction_id.read() != in.transaction_id.read())
        r.mismatched.push_back("transaction_id");
    if (out.address.read() != in.address.read())
        r.mismatched.push_back("address");
    if (out.data.read() != in.data.read())
        r.mismatched.push_back("data");
    if (out.size.read() != in.size.read())
        r.mismatched.push_back("size");
    if (out.perm.read() != in.perm.read())
        r.mismatched.push_back("perm");
    if (out.snoop_type.read() != in.snoop_type.read())
        r.mismatched.push_back("snoop_type");
    if (out.line_state.read() != in.line_state.read())
        r.mismatched.push_back("line_state");
    if (out.hit.read() != in.hit.read())
        r.mismatched.push_back("hit");
    if (out.dirty.read() != in.dirty.read())
        r.mismatched.push_back("dirty");
    if (out.error.read() != in.error.read())
        r.mismatched.push_back("error");
    if (out.atomic.read() != in.atomic.read())
        r.mismatched.push_back("atomic");

    r.ok = r.mismatched.empty();
    return r;
}

// ══════════════════════════════════════════════════════════════════════
//  判据 ③:「丢弃清单 ↔ 实际行为」一致性(CC-03 ③ 的反向 fixture 载体)
// ══════════════════════════════════════════════════════════════════════
//
// ⚠️ **行为式**判定(不是清单比对)—— 逐个变更字段,看网络 flit 是否**跟着变**:
//   · 变更**共有字段** ⇒ 网络 flit **必须**变(否则该字段根本没被映射)
//   · 变更**丢弃字段** ⇒ 网络 flit **必须不变**(否则它其实没被丢弃 ——
//     这正是 3.9 的 fixture「把 `perm` 映射到 `flit_category`」要抓的形态)
struct FieldMappingProbe {
    bool ok = true;
    std::vector<std::string> not_mapped;   ///< 声称共有、实际未影响网络 flit的字段
    std::vector<std::string> leaked;       ///< 声称丢弃、实际影响了网络 flit 的字段
};

template <typename BridgeT>
FieldMappingProbe check_field_mapping(BridgeT& bridge, const bundles::CohIfBundle& base) {
    FieldMappingProbe r;

    bundles::NoCFlitBundle base_flit;
    bridge.pack(base, base_flit);

    auto probe = [&](const char* name, bool should_change, auto&& mutate) {
        bundles::CohIfBundle v = base;
        mutate(v);
        bundles::NoCFlitBundle f;
        bridge.pack(v, f);
        const bool changed = !noc_flit_equal(base_flit, f);
        if (should_change && !changed)
            r.not_mapped.push_back(name);
        if (!should_change && changed)
            r.leaked.push_back(name);
    };

    // 共有字段(4 个)—— 必须影响网络 flit
    probe("transaction_id", true,
          [](bundles::CohIfBundle& v) { v.transaction_id.write(0xA5A5A5A5A5A5A5A5ULL); });
    probe("address", true, [](bundles::CohIfBundle& v) { v.address.write(0x1234567890ABCDEFULL); });
    probe("data", true, [](bundles::CohIfBundle& v) { v.data.write(0xFEDCBA0987654321ULL); });
    probe("size", true, [](bundles::CohIfBundle& v) { v.size.write(77); });

    // 丢弃字段(7 个)—— 必须**不**影响网络 flit
    probe("perm", false, [](bundles::CohIfBundle& v) { v.perm.write(bundles::CohIfBundle::PERM_UNIQUE); });
    probe("snoop_type", false,
          [](bundles::CohIfBundle& v) { v.snoop_type.write(bundles::CohIfBundle::SNOOP_FORWARD); });
    probe("line_state", false,
          [](bundles::CohIfBundle& v) { v.line_state.write(bundles::CohIfBundle::STATE_M); });
    probe("hit", false, [](bundles::CohIfBundle& v) { v.hit.write(1); });
    probe("dirty", false, [](bundles::CohIfBundle& v) { v.dirty.write(1); });
    probe("error", false, [](bundles::CohIfBundle& v) { v.error.write(1); });
    probe("atomic", false, [](bundles::CohIfBundle& v) { v.atomic.write(1); });

    // ── `flow_id`:**降维映射**(5 值 → 3 值,多对一 ⇒ 不可逆)──────────
    //
    // ⚠️ 同类别/跨类别的流**动态选取** —— 硬编码会随 base 的 `flow_id` 变化而
    //    失效(实测踩过:base 是 `SnoopResp` 而硬编码用了 `Request` ⇒ 两条探针
    //    语义颠倒 ⇒ **正控假红**)。
    {
        const std::uint8_t base_cat = BridgeT::category_of(base.flow_id);
        const bundles::CohIfFlowId all[] = {
            bundles::CohIfFlowId::Request,    bundles::CohIfFlowId::Completion,
            bundles::CohIfFlowId::Snoop,      bundles::CohIfFlowId::SnoopResp,
            bundles::CohIfFlowId::Writeback};
        bundles::CohIfFlowId same_flow = base.flow_id;
        bundles::CohIfFlowId diff_flow = base.flow_id;
        bool have_same = false;
        bool have_diff = false;
        for (auto f : all) {
            if (f == base.flow_id)
                continue;
            if (BridgeT::category_of(f) == base_cat) {
                if (!have_same) {
                    same_flow = f;
                    have_same = true;
                }
            } else if (!have_diff) {
                diff_flow = f;
                have_diff = true;
            }
        }
        // ① 同类别的**另一个**流 ⇒ 网络 flit **必须不变**(信息丢失的直接证据)
        if (have_same) {
            bundles::CohIfBundle v = base;
            v.flow_id = same_flow;
            bundles::NoCFlitBundle f;
            bridge.pack(v, f);
            if (!noc_flit_equal(base_flit, f))
                r.leaked.push_back("flow_id(同类别流被区分 —— 降维映射不成立)");
        }
        // ② 跨类别的流 ⇒ 网络 flit **必须变**(证明 `flow_id` 确实被映射)
        if (have_diff) {
            bundles::CohIfBundle v = base;
            v.flow_id = diff_flow;
            bundles::NoCFlitBundle f;
            bridge.pack(v, f);
            if (noc_flit_equal(base_flit, f))
                r.not_mapped.push_back("flow_id(跨类别的流未改变网络 flit)");
        }
    }

    r.ok = r.not_mapped.empty() && r.leaked.empty();
    return r;
}

}  // namespace cep
}  // namespace cpptlm

#endif  // CEP_PROTOCOL_BRIDGE_CONTRACT_HH
