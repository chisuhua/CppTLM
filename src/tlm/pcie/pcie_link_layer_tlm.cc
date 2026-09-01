// src/tlm/pcie/pcie_link_layer_tlm.cc
// PcieLinkLayer 实现：DLLP gen/parse/dispatch + FC + ACK/NAK retry + 双向 Rx
// 作者 CppTLM Team / 日期 2026-09-08
// 参考: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §4

#include "tlm/pcie/pcie_link_layer_tlm.hh"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace tlm::pcie {

namespace {

// PcieEndpointTLM ↔ PcieLinkLayer composition 注册表（按 EP 模块名）
// 冻结 .h 布局不能加成员 → EP 侧通过 on_config_loaded() 挂接，这里持有生命周期。
std::unordered_map<std::string, std::unique_ptr<PcieLinkLayer>>& endpoint_registry() {
    static std::unordered_map<std::string, std::unique_ptr<PcieLinkLayer>> registry;
    return registry;
}

} // namespace

PcieLinkLayer* PcieLinkLayer::attach_to_endpoint(const std::string& endpoint_name,
                                                 EventQueue* eq,
                                                 const PcieLinkLayerConfig& cfg) {
    auto& reg = endpoint_registry();
    auto it = reg.find(endpoint_name);
    if (it != reg.end()) {
        it->second = std::make_unique<PcieLinkLayer>(eq, cfg);
        return it->second.get();
    }
    auto [new_it, _] = reg.emplace(endpoint_name,
                                   std::make_unique<PcieLinkLayer>(eq, cfg));
    return new_it->second.get();
}

PcieLinkLayer* PcieLinkLayer::for_endpoint(const std::string& endpoint_name) noexcept {
    auto& reg = endpoint_registry();
    auto it = reg.find(endpoint_name);
    return (it != reg.end()) ? it->second.get() : nullptr;
}

void PcieLinkLayer::detach_from_endpoint(const std::string& endpoint_name) noexcept {
    auto& reg = endpoint_registry();
    reg.erase(endpoint_name);
}

std::size_t PcieLinkLayer::endpoint_count() noexcept {
    return endpoint_registry().size();
}

namespace {

// 12-bit seq 比较：(a - b) mod 4096 == 0 视为相等优先级相当
// 用于 ACK 累积确认中判断 entry 是否 ≤ ack_seq（含 wrap）。
bool seq_le(uint16_t entry, uint16_t ack_seq) {
    // entry ≤ ack_seq (mod 4096): 从 entry 到 ack 的正向距离 < 2048
    // (即 entry 在 ack 之前不超过半个 seq 空间)
    const uint16_t dist = static_cast<uint16_t>(
        (ack_seq - entry) & PcieLinkLayer::SEQ_MASK);
    return dist < 2048;
}

} // namespace

PcieLinkLayer::PcieLinkLayer(EventQueue* eq)
    : PcieLinkLayer(eq, PcieLinkLayerConfig{}) {}

PcieLinkLayer::PcieLinkLayer(EventQueue* eq, const PcieLinkLayerConfig& cfg)
    : name_("pcie_link_layer"), eq_(eq), cfg_(cfg), fc_(eq) {
    err_.enabled = cfg_.link_error_injection_enabled;
    // 初始 credit：InitFC 未到达时使用 cfg 默认（PCIe 链路训练后由 InitFC 设定上限）
    fc_.install_bucket(0, FcTokenBucket(cfg_.fc_capacity));
    // 设初始 token = cfg 初始 credit（bruteforce 一致：FcTokenBucket 构造已填 capacity）
    fc_.bucket(0).update(FcTokenBucket::Type::Posted, cfg_.fc_init_p);
    fc_.bucket(0).update(FcTokenBucket::Type::NonPosted, cfg_.fc_init_np);
    fc_.bucket(0).update(FcTokenBucket::Type::Completion, cfg_.fc_init_cpl);
}

// ========== DLLP 生成 ==========

bundles::PcieDllpBundle PcieLinkLayer::make_ack(uint16_t ack_seq) {
    bundles::PcieDllpBundle dllp;
    dllp.kind.write(bundles::PcieDllpBundle::ACK);
    dllp.vc_id.write(0u);  // 单 VC0 (per Q11)
    dllp.seq_num_ack.write(ack_seq);
    return dllp;
}

bundles::PcieDllpBundle PcieLinkLayer::make_nak(uint16_t nak_seq) {
    bundles::PcieDllpBundle dllp;
    dllp.kind.write(bundles::PcieDllpBundle::NAK);
    dllp.vc_id.write(0u);
    dllp.seq_num_ack.write(nak_seq);
    return dllp;
}

bundles::PcieDllpBundle PcieLinkLayer::make_init_fc1(uint16_t p, uint16_t np,
                                                      uint16_t cpl) {
    bundles::PcieDllpBundle dllp;
    dllp.kind.write(bundles::PcieDllpBundle::INIT_FC1);
    dllp.vc_id.write(0u);
    dllp.credit_P.write(p);
    dllp.credit_NP.write(np);
    dllp.credit_Cpl.write(cpl);
    return dllp;
}

bundles::PcieDllpBundle PcieLinkLayer::make_init_fc2(uint16_t p, uint16_t np,
                                                      uint16_t cpl) {
    bundles::PcieDllpBundle dllp;
    dllp.kind.write(bundles::PcieDllpBundle::INIT_FC2);
    dllp.vc_id.write(0u);
    dllp.credit_P.write(p);
    dllp.credit_NP.write(np);
    dllp.credit_Cpl.write(cpl);
    return dllp;
}

bundles::PcieDllpBundle PcieLinkLayer::make_update_fc(uint16_t p, uint16_t np,
                                                       uint16_t cpl) {
    bundles::PcieDllpBundle dllp;
    dllp.kind.write(bundles::PcieDllpBundle::UPDATE_FC);
    dllp.vc_id.write(0u);
    dllp.credit_P.write(p);
    dllp.credit_NP.write(np);
    dllp.credit_Cpl.write(cpl);
    return dllp;
}

bundles::PcieDllpBundle PcieLinkLayer::make_nop() {
    bundles::PcieDllpBundle dllp;
    dllp.kind.write(bundles::PcieDllpBundle::NOP);
    dllp.vc_id.write(0u);
    return dllp;
}

bundles::PcieDllpBundle PcieLinkLayer::make_vendor() {
    bundles::PcieDllpBundle dllp;
    dllp.kind.write(bundles::PcieDllpBundle::VENDOR);
    dllp.vc_id.write(0u);
    return dllp;
}

// ========== DLLP parse / dispatch ==========

bool PcieLinkLayer::parse_dllp(const bundles::PcieDllpBundle& dllp,
                               ParsedDllp& out) const {
    out.vc_id = static_cast<uint8_t>(dllp.vc_id.read());
    out.credit_P = static_cast<uint16_t>(dllp.credit_P.read());
    out.credit_NP = static_cast<uint16_t>(dllp.credit_NP.read());
    out.credit_Cpl = static_cast<uint16_t>(dllp.credit_Cpl.read());
    out.seq_num = static_cast<uint16_t>(dllp.seq_num.read());
    out.seq_num_ack = static_cast<uint16_t>(dllp.seq_num_ack.read());
    out.trans_id = static_cast<uint32_t>(dllp.trans_id.read());

    switch (dllp.kind.read()) {
    case bundles::PcieDllpBundle::ACK:       out.kind = Dispatch::ACK; break;
    case bundles::PcieDllpBundle::NAK:       out.kind = Dispatch::NAK; break;
    case bundles::PcieDllpBundle::INIT_FC1:  out.kind = Dispatch::INIT_FC1; break;
    case bundles::PcieDllpBundle::INIT_FC2:  out.kind = Dispatch::INIT_FC2; break;
    case bundles::PcieDllpBundle::UPDATE_FC: out.kind = Dispatch::UPDATE_FC; break;
    case bundles::PcieDllpBundle::NOP:       out.kind = Dispatch::NOP; break;
    case bundles::PcieDllpBundle::VENDOR:    out.kind = Dispatch::VENDOR; break;
    default:                                 out.kind = Dispatch::UNKNOWN; break;
    }
    return true;
}

PcieLinkLayer::Dispatch PcieLinkLayer::rx_dllp(
    const bundles::PcieDllpBundle& dllp) {
    ParsedDllp parsed;
    parse_dllp(dllp, parsed);

    switch (parsed.kind) {
    case Dispatch::ACK:
        on_ack_received(parsed.seq_num_ack);
        break;
    case Dispatch::NAK:
        on_nak_received(parsed.seq_num_ack);
        break;
    case Dispatch::INIT_FC1:
    case Dispatch::INIT_FC2:
        // InitFC1/InitFC2：设定 FC credit 上限（per §3.4.1）
        apply_initial_fc(dllp);
        break;
    case Dispatch::UPDATE_FC:
        // UpdateFC：增加 credit（唯一补充路径，per Q2）
        fc_.update(0, FcTokenBucket::Type::Posted, dllp.credit_P.read());
        fc_.update(0, FcTokenBucket::Type::NonPosted, dllp.credit_NP.read());
        fc_.update(0, FcTokenBucket::Type::Completion, dllp.credit_Cpl.read());
        break;
    case Dispatch::NOP:
    case Dispatch::VENDOR:
        // NOP/Vendor：无副作用（保留字段解析已完成）
        break;
    default:
        break;
    }
    return parsed.kind;
}

void PcieLinkLayer::apply_initial_fc(const bundles::PcieDllpBundle& fc1) {
    // 重建 bucket：InitFC1/InitFC2 设定 capacity 上限与各桶初始 credit
    const uint16_t p = static_cast<uint16_t>(fc1.credit_P.read());
    const uint16_t np = static_cast<uint16_t>(fc1.credit_NP.read());
    const uint16_t cpl = static_cast<uint16_t>(fc1.credit_Cpl.read());
    FcTokenBucket bucket(cfg_.fc_capacity);
    bucket.init_fc(cfg_.fc_capacity, p, np, cpl);
    fc_.install_bucket(0, std::move(bucket));
}

// ========== FC delegates ==========

bool PcieLinkLayer::can_send_fc(FcTokenBucket::Type t, uint32_t vf) {
    return fc_.can_send(vf, t);
}

bool PcieLinkLayer::consume_fc(FcTokenBucket::Type t, uint32_t vf) {
    return fc_.consume(vf, t);
}

void PcieLinkLayer::update_fc(FcTokenBucket::Type t, uint32_t credit,
                              uint32_t vf) {
    fc_.update(vf, t, credit);
}

// ========== Tx path ==========

FcTokenBucket::Type PcieLinkLayer::fc_type_for_kind(uint8_t kind) {
    switch (kind) {
    case bundles::PcieTlpBundle::CFG_READ:
    case bundles::PcieTlpBundle::MMIO_READ:
    case bundles::PcieTlpBundle::MEM_READ:
        return FcTokenBucket::Type::NonPosted;
    case bundles::PcieTlpBundle::IRQ_DELIVERY:
    default:
        return FcTokenBucket::Type::Posted;  // 写 / MSI-X 中断投递视为 Posted
    }
}

bool PcieLinkLayer::tx_tlp(const bundles::PcieTlpBundle& tlp, uint32_t vf) {
    const FcTokenBucket::Type fc_type = fc_type_for_kind(
        static_cast<uint8_t>(tlp.kind.read()));
    if (!fc_.can_send(vf, fc_type)) {
        return false;
    }
    fc_.consume(vf, fc_type);

    // 分配 12-bit seq
    const uint16_t seq = next_tx_seq_;
    next_tx_seq_ = static_cast<uint16_t>((next_tx_seq_ + 1) & SEQ_MASK);

    // 入 retry buffer（retry buffer 深度限制）
    if (retry_buf_.size() < cfg_.retry_buffer_size) {
        retry_buf_.emplace(seq, tlp);
    }

    // 入 wire 输出队列（携带 seq 供错误注入丢包判定）
    tx_tlp_out_.emplace_back(seq, tlp);
    return true;
}

void PcieLinkLayer::tx_dllp(const bundles::PcieDllpBundle& dllp) {
    tx_dllp_out_.push_back(dllp);
}

bool PcieLinkLayer::try_pop_tx_tlp(bundles::PcieTlpBundle& out) {
    while (!tx_tlp_out_.empty()) {
        const auto& [seq, tlp] = tx_tlp_out_.front();
        // 错误注入 TLP 丢包（若启用）：指定 seq 的 TLP 从 wire 消失
        // （retry buffer 中保留 → 对端 NAK 后重传），此处直接丢弃
        if (err_.enabled && err_.tlp_loss_seqs.count(seq) > 0) {
            err_.tlp_loss_seqs.erase(seq);
            ++tlp_dropped_;
            tx_tlp_out_.pop_front();
            continue;
        }
        out = tlp;
        tx_tlp_out_.pop_front();
        return true;
    }
    return false;
}

bool PcieLinkLayer::try_pop_tx_dllp(bundles::PcieDllpBundle& out) {
    while (!tx_dllp_out_.empty()) {
        const bundles::PcieDllpBundle& front = tx_dllp_out_.front();
        if (err_.enabled && err_.drop_next_dllp) {
            err_.drop_next_dllp = false;
            ++dllp_dropped_;
            tx_dllp_out_.pop_front();
            continue;  // DLLP 丢失：不输出
        }
        out = front;
        tx_dllp_out_.pop_front();
        return true;
    }
    return false;
}

// ========== Rx path（下行，Q17）==========

bool PcieLinkLayer::rx_tlp_from_host(const bundles::PcieTlpBundle& tlp,
                                     uint32_t vf) {
    const FcTokenBucket::Type fc_type = fc_type_for_kind(
        static_cast<uint8_t>(tlp.kind.read()));
    if (!fc_.can_send(vf, fc_type)) {
        return false;  // FC 不足：反压，不消费
    }
    fc_.consume(vf, fc_type);

    // 分配下行 seq + 记录到下行 retry buffer（与上行独立，per Q17）
    const uint16_t rx_seq = next_rx_seq_;
    next_rx_seq_ = static_cast<uint16_t>((next_rx_seq_ + 1) & SEQ_MASK);
    downstream_rx_buf_.push_back(tlp);

    // 生成 ACK DLLP 发回 host（累积确认）
    tx_dllp(make_ack(rx_seq));

    // 送事务层
    if (tlp_sink_) {
        tlp_sink_(tlp);
    }
    return true;
}

PcieLinkLayer::Dispatch PcieLinkLayer::rx_dllp_from_host(
    const bundles::PcieDllpBundle& dllp) {
    // 下行 DLLP 与上行共享同一处理引擎（ACK/NAK/InitFC/UpdateFC/NOP）
    if (dllp_sink_) {
        dllp_sink_(dllp);
    }
    return rx_dllp(dllp);
}

// ========== ACK/NAK retry（累积确认，per §3.6）==========

void PcieLinkLayer::on_ack_received(uint16_t ack_seq) {
    // 累积确认（per §3.6）：ACK(seq=AckSeq) 清 retry buffer 中所有
    // 在 (last_acked, AckSeq] 区间内的条目（含 12-bit wrap 单调推进）
    const uint16_t delta = seq_dist(ack_seq, last_acked_seq_);
    if (delta == 0)
        return;  // 重复/旧 ACK：无新确认
    for (auto it = retry_buf_.begin(); it != retry_buf_.end();) {
        const uint16_t d = seq_dist(it->first, last_acked_seq_);
        if (d != 0 && d <= delta) {
            it = retry_buf_.erase(it);
        } else {
            ++it;
        }
    }
    last_acked_seq_ = ack_seq;
}

void PcieLinkLayer::on_nak_received(uint16_t nak_seq) {
    // 重发所有 seq ≥ nak_seq 的 TLP（含 wrap：seq 在 nak_seq 之后半个空间内）
    std::vector<std::pair<uint16_t, bundles::PcieTlpBundle>> retransmit;
    for (const auto& [seq, tlp] : retry_buf_) {
        if (seq_le(nak_seq, seq)) {
            retransmit.emplace_back(seq, tlp);
        }
    }
    // 按序重发（seq 升序）：push_front 会反转顺序 → 必须降序遍历
    // 才能得到 wire 输出升序（per Oracle Issue #1）
    for (auto it = retransmit.rbegin(); it != retransmit.rend(); ++it) {
        tx_tlp_out_.push_front(std::move(*it));
    }
}

// ========== Error Injector / tick ==========

void PcieLinkLayer::tick() {
    if (!err_.enabled)
        return;
    // inject_nak(seq) 注入 → 等效于收到 NAK DLLP → 触发重传
    if (err_.has_pending_nak) {
        err_.has_pending_nak = false;
        on_nak_received(err_.nak_seq);
    }
}

} // namespace tlm::pcie