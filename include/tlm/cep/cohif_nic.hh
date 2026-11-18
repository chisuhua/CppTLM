// include/tlm/cep/cohif_nic.hh
// CohIfNICTLM: CEP 的网络接口模块（CohIf ↔ NoCFlit 双向转换 + 地址→dst_node 映射）
//
// 功能描述：把 `bundles::CohIfBundle`（L5 业务语义，5 条跨 Stub↔PE 边界的流）
//           翻译成 `bundles::NoCFlitBundle`（L4 网络传输载体），并**承担 `dst_node`
//           的填充职责** —— 使 CEP 的 coherence 流量可经 `RouterTLM` 构成的
//           XY mesh **抵达对端**并带回响应。
//
// 作者 CppTLM Team / 日期 2026-11-16
//
// ── 为什么必须有本模块（而非复用 `NICTLM` / `CohIfNoCProtocolBridge`）────
//
// ① `NICTLM` 的 PE 侧端口是 `CacheReqBundle` ⇒ 与 `CohIfBundle` **类型不通**。
// ② `CohIfNoCProtocolBridge` 是**挂在 PE 上的类型转换器**，不是 ChStream 模块
//    ⇒ 没有自己的端口 ⇒ 无法插在 PE 与 Router 之间；且它明确「**无权改写
//    路由器负责的字段**」⇒ **不填 `dst_node`**，而 `RouterTLM` 的路由**只**从
//    `flit.bundle.dst_node.read()` 取目标 ⇒ 路由不了。
// ③ `dst_node` 在仓内的既有填法是 NIU 层职责（`NICTLM::packetize` 的
//    `lookup_node(address)`）⇒ 本模块即 CEP 版 NIU。
//
// ── 已知边界（如实登记，不掩饰）────────────────────────────────────────
//
// ⚠️ **`flit_category` 投影有损**：`NoCFlitBundle.flit_category` 仅 3 值
//    （REQUEST / RESPONSE / CREDIT），装不下 CohIf 的 5 个 `flow_id`
//    ⇒ `Completion` 与 `Snoop` 在对端**不可区分** ⇒ 本模块**不**承诺多流无损
//    （待 architect 裁决，见 change `cpptlm-cep-mesh-e2e-carrier` 的 Q1）。
// ⚠️ **地址映射未命中 ⇒ 显式丢弃**（`stats_dropped_unmapped_` ++）
//    ⇒ **不**复制 `NICTLM::lookup_node` 的静默投 node 0（`include/tlm/nic_tlm.hh:44-51`）。
// ⚠️ `CohIfBundle` **无** `vc_id` ⇒ 单 VC。
// ⚠️ 本模块**不**承载协议语义（credit / retry / ordering point / 多拍聚合 /
//    Perm 规则 / 容量与队列模型）。
// ⚠️ **credit flit 一律丢弃**：`RouterTLM` 的 credit 回流以 **flit 形式**走在
//    `resp_out` 通道上（`BundleDirectionTraits<NoCFlitBundle>::fill_credit` 写
//    `flit_category = CATEGORY_CREDIT`；`reverse_port(LOCAL) == LOCAL` ⇒ 经 LOCAL
//    口进入 router 的 flit，其 credit 沿 `resp_out[LOCAL]` 回流到本模块）。本模块
//    不辨 credit 时会把它当数据：计入 `flits_received`、覆写 `last_recv_*`、
//    并向 PE 侧投影出**全零伪 Completion** ⇒ 故在 ingress 显式丢弃并计数
//    `stats_credits_dropped_`。
// ⚠️ **LOCAL 口的 credit 耗尽不在本模块解决**：router 对 LOCAL 输出同样记账
//    `BUFFER_DEPTH = 8`，而本模块无归还通道 ⇒ 每 (LOCAL, vc) 8 个 flit 后本地投递
//    停滞（既有行为，见 `.rddf/improvements/cpptlm-router-local-port-credit-exhaustion.md`）。
#ifndef TLM_CEP_COHIF_NIC_HH
#define TLM_CEP_COHIF_NIC_HH

#include "bundles/cep_coherence_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/dual_port_stream_adapter.hh"
#include "metrics/stats.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace tlm::cep {

/// CEP 网络接口：PE 侧 `CohIfBundle` ↔ 网络侧 `NoCFlitBundle`
class CohIfNICTLM : public ChStreamModuleBase {
public:
    using CohIf = bundles::CohIfBundle;
    using Flit = bundles::NoCFlitBundle;

    /// 地址区间 → 目标 mesh 节点
    struct AddressRegion {
        std::uint64_t base_addr = 0;
        std::uint64_t size = 0;
        std::uint32_t target_node = 0;
    };

    CohIfNICTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq), stat_group_("cohif_nic") {}
    ~CohIfNICTLM() override = default;

    std::string get_module_type() const override { return "CohIfNICTLM"; }
    unsigned num_ports() const override { return 4; }

    // ── 端口访问器（异构 dual-port；照 `NICTLM` / `StubCacheEndpointTLM` 范式）──
    cpptlm::InputStreamAdapter<CohIf>& pe_req_in() { return pe_req_in_; }
    cpptlm::OutputStreamAdapter<CohIf>& pe_resp_out() { return pe_resp_out_; }
    cpptlm::OutputStreamAdapter<Flit>& net_req_out() { return net_req_out_; }
    cpptlm::InputStreamAdapter<Flit>& net_resp_in() { return net_resp_in_; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override { adapter_ = adapter; }

    /// 装配期配置解析（`node_id` / `mesh_x` / `mesh_y` / `address_regions[]`）
    void on_config_loaded() override;

    // ── 观测面（原始事实；⛔ 不做 latency 聚合 / 指标组装）──────────────
    std::uint64_t flits_sent() const { return stats_flits_sent_; }
    std::uint64_t flits_received() const { return stats_flits_received_; }
    std::uint64_t dropped_unmapped() const { return stats_dropped_unmapped_; }
    /// 网络 ingress 丢弃的 credit flit 数（`flit_category == CATEGORY_CREDIT`）
    std::uint64_t credits_dropped() const { return stats_credits_dropped_; }
    /// 本模块**出站** flit 的查表结果（`lookup_node` 的返回值）
    std::uint32_t last_dst_node() const { return last_dst_node_; }
    /// 本模块**收到**的 flit 的 `dst_node` ⇒ 「确实投给对端」的可观测量
    std::uint32_t last_recv_dst_node() const { return last_recv_dst_node_; }
    /// 本模块**收到**的 flit 的 `hops` ⇒ 「确实穿过了 mesh」的可观测量
    /// （router 转发时 `hops += 1` ⇒ 直连的 flit `hops == 0`）
    std::uint32_t last_recv_hops() const { return last_recv_hops_; }

    tlm_stats::StatGroup* get_stats_group() override { return &stat_group_; }
    std::string get_stats_path() const override { return getName(); }

    std::uint32_t node_id() const { return node_id_; }

    // ── 周期推进 ────────────────────────────────────────────────────────
    void tick() override;

private:
    /// 地址 → 目标节点；未命中 ⇒ `std::nullopt`（**显式失败**，⛔ 不静默投 0）
    bool lookup_node(std::uint64_t addr, std::uint32_t& node) const;

    /// CohIf → NoCFlit（正向）。返回 false ⇒ 丢弃（地址未命中）
    bool pack_flit(const CohIf& in, Flit& out);
    /// NoCFlit → CohIf（反向）
    static CohIf unpack_flit(const Flit& in);

    void handle_pe_ingress();
    void handle_net_ingress();
    void drain_net_egress();

    // ── 端口 ────────────────────────────────────────────────────────────
    cpptlm::InputStreamAdapter<CohIf> pe_req_in_{};
    cpptlm::OutputStreamAdapter<CohIf> pe_resp_out_{};
    cpptlm::OutputStreamAdapter<Flit> net_req_out_{};
    cpptlm::InputStreamAdapter<Flit> net_resp_in_{};
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // ── 待发队列（出站 flit）───────────────────────────────────────────
    std::deque<Flit> pending_flits_;

    // ── 配置 ────────────────────────────────────────────────────────────
    std::uint32_t node_id_ = 0;
    std::uint32_t mesh_x_ = 4;
    std::uint32_t mesh_y_ = 4;
    std::vector<AddressRegion> regions_;

    // ── 观测 ────────────────────────────────────────────────────────────
    std::uint64_t stats_flits_sent_ = 0;
    std::uint64_t stats_flits_received_ = 0;
    std::uint64_t stats_dropped_unmapped_ = 0;
    std::uint64_t stats_credits_dropped_ = 0;
    std::uint32_t last_dst_node_ = 0;
    std::uint32_t last_recv_dst_node_ = 0;
    std::uint32_t last_recv_hops_ = 0;

    tlm_stats::StatGroup stat_group_;
};

// ════════════════════════════════════════════════════════════════════════
// 实现（**header-only** —— 照仓内 CEP 惯例：43 个 CEP 头文件均无对应 .cc）
// ════════════════════════════════════════════════════════════════════════

namespace detail {

/// `base` / `size` 的宽松解析：数字或「0x」十六进制字符串皆可
inline std::uint64_t parse_u64(const nlohmann::json& obj, const char* key) {
    if (!obj.contains(key)) {
        return 0ULL;
    }
    const auto& v = obj.at(key);
    if (v.is_string()) {
        return std::stoull(v.get<std::string>(), nullptr, 0);
    }
    return v.get<std::uint64_t>();
}

}  // namespace detail

inline void CohIfNICTLM::on_config_loaded() {
    // ── 声明即读取（`ADR-X.17` D-1）────────────────────────────────────────
    // 三键**可选**（默认值 = 成员初值）⇒ `load_param_opt`;类型不符 ⇒ 装配期失败
    // （⛔ 旧实现 `cfg["x"].get<uint32_t>()` 在类型不符时抛的是 nlohmann 的
    //   `type_error`，错误信息不含「参数名 + 期望类型」⇒ 不满足 D-3 的可诊断要求）。
    load_param_opt("node_id", node_id_);
    load_param_opt("mesh_x", mesh_x_);
    load_param_opt("mesh_y", mesh_y_);

    // `address_regions` 是**非标量**（对象数组）⇒ `ADR-X.17` D-2 态②
    // （`load_param` 只承载标量）⇒ 模块内解析 + 显式登记「已消费」。
    ignore_param("address_regions");
    const auto& cfg = get_config();
    regions_.clear();
    if (cfg.contains("address_regions")) {
        for (const auto& reg : cfg["address_regions"]) {
            AddressRegion r;
            r.base_addr = detail::parse_u64(reg, "base");
            r.size = detail::parse_u64(reg, "size");
            r.target_node = reg.value("target_node", 0u);
            regions_.push_back(r);
        }
    }
}

inline bool CohIfNICTLM::lookup_node(std::uint64_t addr, std::uint32_t& node) const {
    for (const auto& r : regions_) {
        if (r.size != 0 && addr >= r.base_addr && addr < r.base_addr + r.size) {
            node = r.target_node;
            return true;
        }
    }
    return false;
}

inline bool CohIfNICTLM::pack_flit(const CohIf& in, Flit& out) {
    std::uint32_t dst = 0;
    if (!lookup_node(in.address.read(), dst)) {
        // ⚠️ **显式失败** —— ⛔ 不静默投 node 0（`NICTLM::lookup_node` 的缺陷不复制）
        return false;
    }

    out.transaction_id.write(in.transaction_id.read());
    out.address.write(in.address.read());
    out.data.write(in.data.read());
    out.size.write(in.size.read());

    out.src_node.write(node_id_);
    out.dst_node.write(dst);
    out.vc_id.write(0);  // ⚠️ `CohIfBundle` 无 `vc_id` ⇒ 单 VC

    // `CohIfBundle.data` 是 **1 拍**载荷 ⇒ 单 flit 足够
    out.flit_type.write(Flit::FLIT_HEAD_TAIL);
    out.flit_index.write(0);
    out.flit_count.write(1);
    out.hops.write(0);  // ⚠️ router 转发时递增 ⇒ 本模块不写

    // ⚠️ 有损投影：5 个 flow_id → 3 值 flit_category（见文件头「已知边界」）
    out.flit_category.write(in.flow_id == bundles::CohIfFlowId::Request
                                ? Flit::CATEGORY_REQUEST
                                : Flit::CATEGORY_RESPONSE);

    // 写意图的可观测代理（⚠️ 近似 —— `CohIfBundle` 无 `is_write` 字段）
    const std::uint8_t perm = in.perm.read();
    out.is_write.write((perm == CohIf::PERM_UNIQUE || perm == CohIf::PERM_WRITEBACK) ? 1 : 0);

    // `src_port` / `is_ok` / `error_code` 由 router / 响应侧负责 ⇒ 本模块不写
    out.error_code.write(0);

    last_dst_node_ = dst;
    return true;
}

inline CohIfNICTLM::CohIf CohIfNICTLM::unpack_flit(const Flit& in) {
    CohIf c;
    // ⚠️ `flow_id` **不可从 flit 恢复**（5→3 有损）⇒ 由 `flit_category` 恢复二值
    c.flow_id = (in.flit_category.read() == Flit::CATEGORY_REQUEST)
                    ? bundles::CohIfFlowId::Request
                    : bundles::CohIfFlowId::Completion;
    c.transaction_id.write(in.transaction_id.read());
    c.address.write(in.address.read());
    c.data.write(in.data.read());
    c.size.write(in.size.read());
    return c;
}

inline void CohIfNICTLM::handle_pe_ingress() {
    if (!pe_req_in_.valid() || !pe_req_in_.ready()) {
        return;
    }
    const CohIf in = pe_req_in_.data();
    pe_req_in_.consume();

    Flit flit;
    if (!pack_flit(in, flit)) {
        ++stats_dropped_unmapped_;
        return;
    }
    pending_flits_.push_back(flit);
}

inline void CohIfNICTLM::handle_net_ingress() {
    if (!net_resp_in_.valid() || !net_resp_in_.ready()) {
        return;
    }
    const Flit flit = net_resp_in_.data();
    net_resp_in_.consume();

    // credit flit 与数据 flit 共用同一通道（见文件头「已知边界」）⇒ 显式丢弃：
    // 不计数、不写观测、不投影 PE 侧（否则产出全零伪 Completion）。
    if (flit.flit_category.read() == Flit::CATEGORY_CREDIT) {
        ++stats_credits_dropped_;
        return;
    }

    ++stats_flits_received_;
    last_recv_dst_node_ = flit.dst_node.read();
    last_recv_hops_ = flit.hops.read();

    pe_resp_out_.write(unpack_flit(flit));
}

inline void CohIfNICTLM::drain_net_egress() {
    // ⚠️ `OutputStreamAdapter::valid()` 的语义 =「**有数据待发**」(等价 `ch_stream.valid`)
    //    ⇒ **可写**的条件是 `!valid()`;已有待发数据时须等 adapter 消费后再写。
    //    (照 `NICTLM::tick()` 的既有判据 `if (!net_req_out_.valid() && !pending.empty())`)
    if (pending_flits_.empty() || net_req_out_.valid()) {
        return;
    }
    net_req_out_.write(pending_flits_.front());
    pending_flits_.pop_front();
    ++stats_flits_sent_;
}

inline void CohIfNICTLM::tick() {
    handle_pe_ingress();
    handle_net_ingress();
    drain_net_egress();
    if (adapter_) {
        adapter_->tick();
    }
}

}  // namespace tlm::cep

#endif  // TLM_CEP_COHIF_NIC_HH
