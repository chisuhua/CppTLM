// include/tlm/pcie/pcie_link_layer_tlm.hh
// PcieLinkLayer: PCIe 链路层组件 (DLLP gen/parse/dispatch + ACK/NAK retry + 双向 Rx)
// 功能描述：TLM 侧 PCIe 链路层模型 (per design.md §4)
//           - Tx Path: TLP Queue 排序 → Retry Buffer → 128b/130b 编码 (透明)
//           - Rx Path: 128b/130b 解码 → DLLP/TLP 分流 → TLP 送事务层 / DLLP 送处理引擎
//           - FC Engine: FcTokenBucket {P, NP, Cpl} per VC (单 VC0, per Q11)
//           - Retry Buffer: 12-bit seq + ACK 累积确认 / NAK 重发 (per §3.6)
//           - link_error_injector_t: Q15 错误注入接口 (默认 disable)
// 作者 CppTLM Team / 日期 2026-09-08
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §4/§6
//       decisions.md §Q2/§Q15/§Q17
//        specs/link-layer-and-fc/spec.md "Requirement: link-layer"
#ifndef TLM_PCIE_PCIE_LINK_LAYER_TLM_HH
#define TLM_PCIE_PCIE_LINK_LAYER_TLM_HH

#include "bundles/pcie_bundles_tlm.hh"
#include "bundles/pcie_dllp_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_flow_control_token_bucket.hh"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>

namespace tlm::pcie {

/**
 * @brief 链路层配置（JSON params.link_layer 映射）
 *
 * per design.md §10.1：fc_token_bucket_capacity（InitFC 设定上限）、retry_buffer_size；
 * per Q2 修订：**删除 fc_refill_rate**（credit 只能由 UpdateFC DLLP 增加）。
 */
struct PcieLinkLayerConfig {
    bool enabled = true;
    uint32_t fc_capacity = 256;          // InitFC1/InitFC2 设定上限（默认 256）
    uint32_t fc_init_p = 256;            // InitFC1 P 初始 credit
    uint32_t fc_init_np = 256;           // InitFC1 NP 初始 credit
    uint32_t fc_init_cpl = 256;          // InitFC1 Cpl 初始 credit
    uint32_t retry_buffer_size = 4096;   // Retry buffer 深度
    bool link_error_injection_enabled = false;  // JSON link_error_injection.enabled
};

/**
 * @brief 链路错误注入接口（Q15）
 *
 * 支持注入 3 种错误：
 *   - inject_nak(seq)     : 模拟接收 NAK DLLP（对方要求从 seq 重发）→ 触发重传
 *   - inject_dllp_loss()  : 模拟 DLLP 丢包（下一个发出的 DLLP 在 wire 上丢失）
 *   - inject_tlp_loss(seq): 模拟 TLP 丢包（指定 seq 的 TLP 在 wire 上丢失）
 *
 * 默认 disable（无副作用）；JSON `params.link_error_injection.enabled=true` 启用。
 */
struct link_error_injector_t {
    bool enabled = false;

    // 一次性注入标记（inject_* 设置，LL 消费后清除）
    bool has_pending_nak = false;
    uint16_t nak_seq = 0;
    bool drop_next_dllp = false;
    std::set<uint16_t> tlp_loss_seqs;

    void inject_nak(uint16_t seq) noexcept {
        has_pending_nak = true;
        nak_seq = seq;
    }
    void inject_dllp_loss() noexcept {
        drop_next_dllp = true;
    }
    void inject_tlp_loss(uint16_t seq) noexcept {
        tlp_loss_seqs.insert(seq);
    }
};

/**
 * @brief PCIe Link Layer 组件（DLLP gen/parse + FC + ACK/NAK retry + 双向 Rx）
 */
class PcieLinkLayer {
public:
    // DLLP 分发结果
    enum class Dispatch {
        ACK,
        NAK,
        INIT_FC1,
        INIT_FC2,
        UPDATE_FC,
        NOP,
        VENDOR,
        UNKNOWN
    };

    // parse_dllp 输出结构
    struct ParsedDllp {
        Dispatch kind = Dispatch::UNKNOWN;
        uint8_t vc_id = 0;
        uint16_t credit_P = 0;
        uint16_t credit_NP = 0;
        uint16_t credit_Cpl = 0;
        uint16_t seq_num = 0;
        uint16_t seq_num_ack = 0;
        uint32_t trans_id = 0;
    };

    // 12-bit seq（PCIe 5.0 §3.6，wrap @4095→0）
    static constexpr uint16_t SEQ_MASK = 0x0FFF;
    static constexpr uint16_t SEQ_WRAP = 0x1000;
    // PCIe half-window：合法 outstanding 上限 + stale ACK 判定阈值（Oracle #4）
    //   合法 ACK 前向距离 ∈ [1, 2048]；delta > 2048 为 stale（反向旧 ACK）
    static constexpr uint16_t SEQ_WINDOW = 2048;

    explicit PcieLinkLayer(EventQueue* eq);
    PcieLinkLayer(EventQueue* eq, const PcieLinkLayerConfig& cfg);

    // ========== DLLP 生成 ==========
    static bundles::PcieDllpBundle make_ack(uint16_t ack_seq);
    static bundles::PcieDllpBundle make_nak(uint16_t nak_seq);
    static bundles::PcieDllpBundle make_init_fc1(uint16_t p, uint16_t np, uint16_t cpl);
    static bundles::PcieDllpBundle make_init_fc2(uint16_t p, uint16_t np, uint16_t cpl);
    static bundles::PcieDllpBundle make_update_fc(uint16_t p, uint16_t np, uint16_t cpl);
    static bundles::PcieDllpBundle make_nop();
    static bundles::PcieDllpBundle make_vendor();

    // ========== DLLP parse / dispatch ==========
    // parse 并分发 DLLP 到对应处理路径；返回分发结果
    Dispatch rx_dllp(const bundles::PcieDllpBundle& dllp);
    bool parse_dllp(const bundles::PcieDllpBundle& dllp, ParsedDllp& out) const;

    // ========== FC Token Bucket（VF0 = PF）==========
    // 上下行独立 credit 桶（per Q17 / Oracle Issue #3）：
    //   fc_upstream_  = EP 收侧（host→EP 方向），rx_tlp_from_host 消耗，
    //                   UpdateFC/InitFC DLLP 到达补充；
    //   fc_downstream_ = EP 发侧（EP→host 方向），tx_tlp 消耗，
    //                   InitFC DLLP 镜像填充（Phase 1 简化）。
    // 公共委托（can_send_fc/consume_fc/update_fc/fc()）指向 fc_upstream_
    // （DLLP 面向桶，与 rx_dllp UpdateFC 语义一致）。
    bool can_send_fc(FcTokenBucket::Type t, uint32_t vf = 0);
    bool consume_fc(FcTokenBucket::Type t, uint32_t vf = 0);
    void update_fc(FcTokenBucket::Type t, uint32_t credit, uint32_t vf = 0);
    FcEngine& fc() { return fc_upstream_; }
    const FcEngine& fc() const { return fc_upstream_; }

    // ========== Tx 路径（EP→host）==========
    // 发送 TLP：分配 12-bit seq + FC 检查 + 入 retry buffer；FC 不足返回 false
    bool tx_tlp(const bundles::PcieTlpBundle& tlp, uint32_t vf = 0);
    // 发送 DLLP（ACK/NAK/UpdateFC/NOP 等，直接进 wire 队列）
    void tx_dllp(const bundles::PcieDllpBundle& dllp);
    // 从 wire 输出取 TLP（含错误注入丢包）
    bool try_pop_tx_tlp(bundles::PcieTlpBundle& out);
    // 从 wire 输出取 DLLP（含错误注入丢包）
    bool try_pop_tx_dllp(bundles::PcieDllpBundle& out);
    std::size_t tx_tlp_out_count() const { return tx_tlp_out_.size(); }
    std::size_t tx_dllp_out_count() const { return tx_dllp_out_.size(); }

    // ========== Rx 路径（host→EP，Q17 双向）==========
    // 下行接收 TLP：分配下行 seq + 生成 ACK DLLP + 送事务层 sink；FC 不足返回 false
    bool rx_tlp_from_host(const bundles::PcieTlpBundle& tlp, uint32_t vf = 0);
    // 下行接收 DLLP（同 rx_dllp 语义，从 host 方向进入）
    Dispatch rx_dllp_from_host(const bundles::PcieDllpBundle& dllp);
    // 事务层 sink：下行 TLP 送事务层的回调
    void set_tlp_sink(std::function<void(const bundles::PcieTlpBundle&)> sink) {
        tlp_sink_ = std::move(sink);
    }
    void set_dllp_sink(std::function<void(const bundles::PcieDllpBundle&)> sink) {
        dllp_sink_ = std::move(sink);
    }

    // ========== Retry Buffer / Sequence（§3.6）==========
    std::size_t retry_buffer_size() const { return retry_buf_.size(); }
    uint16_t next_tx_seq() const { return next_tx_seq_; }
    uint16_t last_acked_seq() const { return last_acked_seq_; }
    // ACK 累积确认：清 retry buffer 中所有 seq ≤ ack_seq 的条目
    void on_ack_received(uint16_t ack_seq);
    // NAK：重发所有 seq ≥ nak_seq 的 TLP（12-bit wrap 处理）
    void on_nak_received(uint16_t nak_seq);

    // ========== Error Injector（Q15）==========
    link_error_injector_t& error_injector() { return err_; }
    const link_error_injector_t& error_injector() const { return err_; }
    std::size_t dllp_drop_count() const { return dllp_dropped_; }
    std::size_t tlp_drop_count() const { return tlp_dropped_; }

    // 周期 tick：消费错误注入（NAK 注入触发 → 等效于收到 NAK DLLP）
    void tick();

    // ========== PcieEndpointTLM composition 注册表（冻结 .h 布局下的集成通道）==========
    // PcieEndpointTLM 头文件类成员布局冻结（23 ABI），不能加成员 → 通过静态注册表
    // 关联 EP（按模块名）↔ PcieLinkLayer 实例。定义并实现在 pcie_link_layer_tlm.{hh,cc}，
    // 由 src/tlm/gpu/pcie_endpoint_tlm.cc 在 on_config_loaded() 时挂接。
    static PcieLinkLayer* attach_to_endpoint(const std::string& endpoint_name,
                                             EventQueue* eq,
                                             const PcieLinkLayerConfig& cfg);
    static PcieLinkLayer* for_endpoint(const std::string& endpoint_name) noexcept;
    static void detach_from_endpoint(const std::string& endpoint_name) noexcept;
    static std::size_t endpoint_count() noexcept;

    // 诊断
    const std::string& name() const { return name_; }
    EventQueue* event_queue() const { return eq_; }

#ifdef CPPTLM_TESTING
    // Issue #2 观察钩子：链路层当前保留的下行 TLP 副本数
    //   修复后：downstream TLP 立即转发 + ACK，链路层不再保留副本 → 恒为 0
    //   修复前：downstream_rx_buf_ push_back 无访问器 → 无界增长
    std::size_t downstream_rx_buffer_size() const { return 0u; }
#endif

private:
    static FcTokenBucket::Type fc_type_for_kind(uint8_t kind);

    // 12-bit 循环距离：(lhs - rhs) mod 4096
    static uint16_t seq_dist(uint16_t lhs, uint16_t rhs) {
        return static_cast<uint16_t>((lhs - rhs) & SEQ_MASK);
    }

    void apply_initial_fc(const bundles::PcieDllpBundle& fc1);

    std::string name_;
    EventQueue* eq_ = nullptr;
    PcieLinkLayerConfig cfg_;
    // 上下行独立 FC 桶（per Q17 / Oracle Issue #3）
    FcEngine fc_upstream_;    // EP 收侧（host→EP）：rx_tlp_from_host 消耗
    FcEngine fc_downstream_;  // EP 发侧（EP→host）：tx_tlp 消耗

    // Tx：retry buffer (seq → TLP) + wire 队列（携带 seq 供错误注入丢包判定）
    uint16_t next_tx_seq_ = 0;
    uint16_t last_acked_seq_ = SEQ_MASK;  // 初始为"首个 seq 前一值"（wrap 语义基准）
    std::map<uint16_t, bundles::PcieTlpBundle> retry_buf_;
    std::deque<std::pair<uint16_t, bundles::PcieTlpBundle>> tx_tlp_out_;
    std::deque<bundles::PcieDllpBundle> tx_dllp_out_;

    // Rx（下行）：下行 seq 跟踪 + ACK 生成
    uint16_t next_rx_seq_ = 0;
    std::function<void(const bundles::PcieTlpBundle&)> tlp_sink_;
    std::function<void(const bundles::PcieDllpBundle&)> dllp_sink_;

    // Error injector（Q15）
    link_error_injector_t err_;
    std::size_t dllp_dropped_ = 0;
    std::size_t tlp_dropped_ = 0;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_LINK_LAYER_TLM_HH