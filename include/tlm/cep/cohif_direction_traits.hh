// include/tlm/cep/cohif_direction_traits.hh
// `CohIfBundle` 的**方向 / credit 载体**特化
//
// 功能:为 `cpptlm::BundleDirectionTraits` 提供 `bundles::CohIfBundle` 的特化,
//       使 `BidirectionalPortAdapter` 能承载 `CohIfBundle`(而非只承载
//       `NoCFlitBundle`)—— per `cpptlm-cep-cohif-carrier` Phase 4(判据 CC-04 ③)。
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 为什么特化落在 CEP 侧(层次纪律)────────────────────────────────
// `BidirectionalPortAdapter` 原本**硬编码** `NoCFlitBundle` 语义
// (读 `flit_category` 决定 `PKT_REQ`/`PKT_RESP`;credit 用 `NoCFlitBundle` 构造)
// ⇒ 承载 `CohIfBundle` 会**编译不过**(`CohIfBundle` 无 `flit_category` / `vc_id`)。
// ⇒ 泛化抽成 `cpptlm::BundleDirectionTraits` —— **框架层不认识任何具体 Bundle**;
//    各 Bundle 在**自己的层**提供特化 ⇒ `include/bundles/` 保持**不依赖** `core/`
//    (bundles 是最底层,不能反向依赖 framework/core)。
//
// ── 方向口径(per `ADR-CEP-19` 决策 2 的 CohIf 5 流)──────────────────
//   `Completion` / `SnoopResp` = **响应** ⇒ `PKT_RESP`
//   `Request` / `Snoop` / `Writeback` = **请求** ⇒ `PKT_REQ`
//   ⚠️ 该口径与 `CohIfNoCProtocolBridge::category_of()` 的 5→3 映射**同源**
//      (Completion/SnoopResp → `CATEGORY_RESPONSE`;其余 → `CATEGORY_REQUEST`)
//      —— 两处若漂移则「网络类别」与「Packet 方向」会不一致 ⇒ 由
//      `test/test_cep_protocol_bridge_contracts.cc` 的交叉断言钉住。
#ifndef TLM_CEP_COHIF_DIRECTION_TRAITS_HH
#define TLM_CEP_COHIF_DIRECTION_TRAITS_HH

#include "bundles/cep_coherence_bundles_tlm.hh"
#include "framework/bidirectional_port_adapter.hh"

namespace cpptlm {

template <>
struct BundleDirectionTraits<bundles::CohIfBundle> {
    static PacketType packet_type(const bundles::CohIfBundle& b) {
        return (b.flow_id == bundles::CohIfFlowId::Completion ||
                b.flow_id == bundles::CohIfFlowId::SnoopResp)
                   ? PKT_RESP
                   : PKT_REQ;
    }

    /// credit 载体 —— `CohIfBundle` **无网络层字段**(per CC-01)
    /// ⇒ credit 的 VC 由 **Packet 字段**(`pkt->vc_id`)承载 ⇒ 此处**不**写任何字段
    static void fill_credit(bundles::CohIfBundle&, unsigned) {}

    /// egress 通道 = **`req_out`**(本方向的 request 通道)
    ///
    /// ⚠️ **为什么是 `req_out`**(per Phase 5 实测,非推断):
    ///    框架的 `ChStreamTargetPort::recvReq` 无论挂在 `req_in` 还是 `resp_in`
    ///    上,都投递到 `process_request_input` ⇒ **走 `resp_out` 也能把数据送到
    ///    对端的 ingress**。但 `resp_out` 装的是「**对方→我**」那条 JSON 连接的
    ///    **响应**通道 ⇒ egress 借用它会**倒置连接方向**,并让「**断开某条连接
    ///    ⇒ 事务不再抵达**」这条判据**失去判别力**
    ///    (实测:反向 fixture「dst 指向不存在的端口索引」在 `Resp` 下**假通过**
    ///    —— 因为另一条 JSON 连接仍把 `resp_out` 接到对端)。
    ///    ⇒ egress 走 `req_out` = 「用我自己发起的这条连接」⇒ 断链即断达。
    static constexpr EgressChannel egress_channel() { return EgressChannel::Req; }
};

}  // namespace cpptlm

#endif  // TLM_CEP_COHIF_DIRECTION_TRAITS_HH
