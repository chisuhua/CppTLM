// include/tlm/cep/cohif_noc_bridge.hh
// CohIf ↔ NoCFlit 的**特化桥**(per `cpptlm-cep-cohif-carrier` Phase 3)
//
// 功能:把 `bundles::CohIfBundle`(L5 业务语义)翻译成 `bundles::NoCFlitBundle`
//       (L4 网络传输载体),并按 **B′ 方案**提供「端到端完整语义」通道。
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── **两层**的映射(per CC-03 ① 的分层判据)─────────────────────────
//
// 【层 1 · 消息内容投影】`pack(const CohIfBundle&, NoCFlitBundle&)` /
//                        `unpack(const NoCFlitBundle&, CohIfBundle&)`
//   · **共有字段**(4 个)双向映射:`transaction_id` / `address` / `data` / `size`
//   · `flit_category` 由 `flow_id` 经**桥自己的 5→3 映射**写入;
//     **不可逆** ⇒ `flow_id` 属**丢弃字段**
//   · **网络封装字段**(`src_node` / `dst_node` / `vc_id` / `flit_type` /
//     `flit_index` / `flit_count` / `hops` / `src_port` / `is_write` /
//     `is_ok` / `error_code`)**既不写也不读** ⇒ per CC-03 ②「网络层语义只在
//     `NoCFlitBundle` 侧」的职责边界。
//     ⚠️ `pack` **不清零** `out` 的其余字段 —— 桥**无权**改写路由器负责的字段。
//
// 【层 2 · 端到端完整语义】`pack_with_packet(Packet*)` /
//                          `unpack_with_packet(Packet*)`
//   · `Packet` 的 payload 数据区承载 **`CohIfBundle` 的完整 12 字段字节**
//     (`CohIfBundle` 是 `trivially_copyable` ⇒ 字节等价 ⇔ 逐字段相等)
//   · 这是「CohIf 业务语义跨网络**不透损**」的**唯一**承载路径
//     (per B′ 方案的架构论证:`flow_id` 5 值 → `flit_category` 3 值是
//      **多对一**有损映射 ⇒ 仅靠网络 flit 字段空间**不可能**唯一恢复)
//
// ── **丢弃字段清单**(`dropped_fields()`)—— 显式、可测、不藏 ─────────
//   层 1 丢弃的 8 个协议语义字段:`flow_id` / `perm` / `snoop_type` /
//   `line_state` / `hit` / `dirty` / `error` / `atomic`
//   ⇒ 判据「**丢弃清单 ↔ 实际行为**一致性」可机器核对:
//     若某字段声称被丢弃、实际却被写进了 `NoCFlitBundle` ⇒ 红。
#ifndef TLM_CEP_COHIF_NOC_BRIDGE_HH
#define TLM_CEP_COHIF_NOC_BRIDGE_HH

#include "bundles/cep_coherence_bundles_tlm.hh"
#include "bundles/noc_bundles_tlm.hh"
#include "framework/protocol_bridge.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace tlm::cep {

/// CohIf → NoCFlit 的特化桥
class CohIfNoCProtocolBridge
    : public cpptlm::ProtocolBridge<bundles::CohIfBundle, bundles::NoCFlitBundle, false> {
public:
    using InBundle = bundles::CohIfBundle;
    using OutBundle = bundles::NoCFlitBundle;
    using Base = cpptlm::ProtocolBridge<InBundle, OutBundle, false>;

    // ── 层 1:消息内容投影 ──────────────────────────────────────────
    void pack(const InBundle& in, OutBundle& out) override {
        // ⚠️ **不清零** `out` 的其余字段 —— 桥无权改写路由器负责的传输封装字段
        out.transaction_id.write(in.transaction_id.read());
        out.address.write(in.address.read());
        out.data.write(in.data.read());
        out.size.write(in.size.read());
        out.flit_category.write(category_of(in.flow_id));
        ++this->stats_.pack_cycles;
        ++this->stats_.conversions;
    }

    void unpack(const OutBundle& in, InBundle& out) override {
        // 协议语义字段**不在层 1 的承载范围** ⇒ 复位为默认值(明确「未携带」)
        out = InBundle();
        out.transaction_id.write(in.transaction_id.read());
        out.address.write(in.address.read());
        out.data.write(in.data.read());
        out.size.write(in.size.read());
        ++this->stats_.unpack_cycles;
        ++this->stats_.conversions;
    }

    // ── 层 2:端到端完整语义 ────────────────────────────────────────
    //
    // 调用约定:payload 数据区**已**承载 `InB` 的原始字节(调用方负责放入);
    // 本方法负责**规范化**并写回。规范化内容:校验 `flow_id` 落在 5 值域内 ——
    // 越界说明上游写坏了 ⇒ **不静默放行**(计数 + 不写回)。
    void pack_with_packet(Packet* pkt) override {
        InBundle b{};
        if (!read_payload_inb(pkt, b))
            return;
        if (!is_valid_flow_id(b.flow_id)) {
            ++semantic_errors_;
            return;
        }
        write_payload_inb(pkt, b);
        ++this->stats_.pack_cycles;
        ++this->stats_.conversions;
    }

    void unpack_with_packet(Packet* pkt) override {
        InBundle b{};
        if (!read_payload_inb(pkt, b))
            return;
        if (!is_valid_flow_id(b.flow_id)) {
            ++semantic_errors_;
            return;
        }
        write_payload_inb(pkt, b);
        ++this->stats_.unpack_cycles;
        ++this->stats_.conversions;
    }

    // ── 字段清单(显式、可测)───────────────────────────────────────
    /// 层 1 **共有**字段(4 个)—— 双向**无损**映射
    static const std::vector<std::string>& shared_fields() {
        static const std::vector<std::string> kFields = {"transaction_id", "address", "data", "size"};
        return kFields;
    }

    /// 层 1 **完全丢弃**字段(7 个)—— 协议语义,层 1 **完全不进**网络 flit
    ///
    /// ⚠️ **不含 `flow_id`** —— `flow_id` 是**降维映射**(见
    ///    `lossily_mapped_fields()`),不是丢弃:它**确实**影响网络 flit
    ///    (`flit_category`),只是 5 值 → 3 值**不可逆**。
    ///    把两者混为一谈会让「丢弃清单 ↔ 实际行为」判据**假红**(实测踩过)。
    static const std::vector<std::string>& dropped_field_names() {
        static const std::vector<std::string> kFields = {"perm",     "snoop_type", "line_state",
                                                         "hit",      "dirty",      "error",
                                                         "atomic"};
        return kFields;
    }

    std::vector<std::string> dropped_fields() const override { return dropped_field_names(); }

    /// 层 1 **降维映射**字段(1 个)—— 映射到网络 flit 但**不可逆**
    ///
    /// `flow_id`(5 值)→ `flit_category`(3 值)是**多对一** ⇒ 同类别的不同流
    /// (如 `Request` 与 `Snoop` 都映射到 `CATEGORY_REQUEST`)**在网络上不可区分**
    /// ⇒ 完整流语义只能走**层 2** 的 payload 通道。
    static const std::vector<std::string>& lossily_mapped_fields() {
        static const std::vector<std::string> kFields = {"flow_id"};
        return kFields;
    }

    /// 层 1 **不携带**的全部字段(丢弃 7 + 降维 1 = 8 个)—— 走层 2 的 payload 通道
    static std::vector<std::string> layer1_uncarried_fields() {
        std::vector<std::string> all = dropped_field_names();
        for (const auto& f : lossily_mapped_fields())
            all.push_back(f);
        return all;
    }

    std::string name() const override { return "cohif_noc"; }

    /// 语义规范化失败计数(越界 `flow_id` ⇒ 累加)
    std::uint64_t semantic_errors() const { return semantic_errors_; }

    // ── `flow_id` → `flit_category` 的 5→3 映射 ─────────────────────
    //
    // ⚠️ **多对一 ⇒ 不可逆**(per B′ 方案的架构论证)⇒ `flow_id` 属丢弃字段。
    //    映射的**值域**复用队列 #6 的平台真源
    //    (`adapter_schema_contracts.hh::category_value_domain()`)——
    //    **不重立第二份** 3 类值域(per CC-06 ③)。
    static std::uint8_t category_of(bundles::CohIfFlowId f) {
        switch (f) {
            case bundles::CohIfFlowId::Request:
            case bundles::CohIfFlowId::Snoop:
            case bundles::CohIfFlowId::Writeback:
                return bundles::NoCFlitBundle::CATEGORY_REQUEST;
            case bundles::CohIfFlowId::Completion:
            case bundles::CohIfFlowId::SnoopResp:
                return bundles::NoCFlitBundle::CATEGORY_RESPONSE;
        }
        return bundles::NoCFlitBundle::CATEGORY_REQUEST;  // 越界兜底(防御性)
    }

    /// `flow_id` 是否落在合法域内(5 值)
    static bool is_valid_flow_id(bundles::CohIfFlowId f) {
        switch (f) {
            case bundles::CohIfFlowId::Request:
            case bundles::CohIfFlowId::Completion:
            case bundles::CohIfFlowId::Snoop:
            case bundles::CohIfFlowId::SnoopResp:
            case bundles::CohIfFlowId::Writeback:
                return true;
        }
        return false;
    }

private:
    std::uint64_t semantic_errors_ = 0;
};

}  // namespace tlm::cep

#endif  // TLM_CEP_COHIF_NOC_BRIDGE_HH
