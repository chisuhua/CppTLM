// src/tlm/pcie/pcie_sriov_vf_pool_tlm.cc
// PcieSriovVfPool 实现：dispatch_tlp / dispatch_msix / next_seq / FLR (T-P4-4/5)
// 作者 CppTLM Team / 日期 2026-10-13
#include "tlm/pcie/pcie_sriov_vf_pool_tlm.hh"
#include "tlm/pcie/pcie_completer_engine.hh"
#include "tlm/pcie/pcie_link_layer_tlm.hh"

namespace tlm::pcie {

    namespace {
        constexpr uint16_t kSeqMask = 0x0FFFu; // 12-bit wrap
    }

    bool PcieSriovVfPool::dispatch_tlp(uint16_t stream_id, const bundles::PcieTlpBundle& tlp) {
        if (!is_valid_stream_id(stream_id)) {
            return false;
        }
        auto& cfg = config_pool_.config_of(stream_id);
        const uint8_t kind = tlp.kind.read();
        const uint16_t cfg_offset = static_cast<uint16_t>(tlp.offset.read());
        const uint32_t trans_id = static_cast<uint32_t>(tlp.trans_id.read());
        switch (kind) {
        case bundles::PcieTlpBundle::CFG_READ:
            // NP 请求: 登记 outstanding Completion 关联 (per Q12)
            completions_.register_np(stream_id, trans_id);
            return true;
        case bundles::PcieTlpBundle::CFG_WRITE:
            cfg.write(cfg_offset, static_cast<uint32_t>(tlp.data.read()));
            return true;
        default:
            // T-P10-1: 非 CFG 事务 → 委托 PcieCompleterEngine 处理
            // (MMIO_READ/WRITE, MEM_READ/WRITE 等)
            // 注意: completer_engine 当前持有自身 config space 副本,
            // 实际 MMIO/MEM 路由需后续 T-P10-3 接入 dispatch_tlp_entry 三态分派
            completer_engine_.handle_tlp(tlp);
            return true;
        }
    }

    bool PcieSriovVfPool::dispatch_completion(uint16_t stream_id, uint32_t trans_id,
                                              const CompletionTracker::CplData& cpl) {
        return completions_.complete(stream_id, trans_id, cpl);
    }

    bool PcieSriovVfPool::dispatch_msix(uint16_t stream_id, uint16_t vector) {
        if (!is_valid_stream_id(stream_id)) {
            return false;
        }
        // 先调 update_pending 置 PBA bit + 若未 masked 入 irq_out 队列
        const bool accepted = msix_pool_.update_pending(stream_id, vector);
        if (!accepted) {
            return false;
        }
        // T-P11-3: 从 MSI-X table 读取 vector entry → 入 delivery_queue_
        // (不依赖 irq_out 队列, 直接读 table 保证 dispatch 语义: 
        //  dispatch_msix 后 tick() 投递 MWr)
        uint64_t msg_addr = 0;
        uint32_t msg_data = 0;
        uint32_t control = 0;
        if (msix_pool_.table_of(stream_id).get_vector_entry(vector, msg_addr, msg_data, control)) {
            // 跳过 masked vector (不投递 MWr)
            if (control & 0x1u) {
                return true;
            }
            MsixDelivery d;
            d.stream_id = stream_id;
            d.vector = vector;
            d.msg_addr = msg_addr;
            d.msg_data = msg_data;
            d.delivered = false;
            delivery_queue_.push_back(d);
        }
        return true;
    }

    bool PcieSriovVfPool::msix_configure_vector(uint16_t stream_id, uint16_t vector,
                                                   uint64_t msg_addr, uint32_t msg_data,
                                                   uint32_t control) {
        if (!is_valid_stream_id(stream_id)) {
            return false;
        }
        return msix_pool_.table_of(stream_id).configure_vector(vector, msg_addr, msg_data, control);
    }

    void PcieSriovVfPool::tick(uint64_t /*elapsed_ns*/) {
        if (!link_layer_) {
            return;
        }
        // 遍历 delivery_queue_ 投递所有 pending MWr
        for (auto& d : delivery_queue_) {
            if (d.delivered) {
                continue;
            }
            emit_mwr_for_msix(d);
        }
    }

    void PcieSriovVfPool::emit_mwr_for_msix(const MsixDelivery& d) {
        // 构造 MWr TLP (MMIO_WRITE, addr=msg_addr, data=msg_data)
        bundles::PcieTlpBundle mwr(
            bundles::PcieTlpBundle::MMIO_WRITE,  // kind
            0,                                    // bar_index
            d.msg_addr,                           // offset (target addr)
            4,                                    // size (4 bytes data per MSI-X spec)
            d.msg_data,                           // data
            0,                                    // requester_id (简化)
            d.vector                              // trans_id (用 vector 编号区分)
        );

        const bool sent = link_layer_->tx_tlp(mwr, d.stream_id);
        if (sent) {
            // 标记已投递
            // 使用 const_cast 来标记 delivered (delivery_queue_ 内修改)
            const_cast<MsixDelivery&>(d).delivered = true;
            // 投递完成 → 调 ABI 回调
            if (intr_delivered_cb_) {
                intr_delivered_cb_(d.vector);
            }
        }
        // 若发送失败, 下个 tick 重试
    }

    uint16_t PcieSriovVfPool::next_seq(uint16_t stream_id) noexcept {
        if (!is_valid_stream_id(stream_id)) {
            return 0;
        }
        const uint16_t s = tlp_seq_[stream_id];
        tlp_seq_[stream_id] = (s + 1u) & kSeqMask;
        return s;
    }

    uint16_t PcieSriovVfPool::seq_of(uint16_t stream_id) const noexcept {
        if (!is_valid_stream_id(stream_id)) {
            return 0;
        }
        return tlp_seq_[stream_id];
    }

    void PcieSriovVfPool::flr_pf() noexcept {
        config_pool_.init_all();
        msix_pool_.init_all();
        completions_.flr_pf();
        tlp_seq_.fill(0);
        ari_router_.set_ari_enabled(false); // ARI Forwarding Enable 是 PF 属性, FLR 后回默认
        for (uint16_t sid = 0; sid < NUM_PORTS; ++sid) {
            fc_engine_.install_bucket(sid, FcTokenBucket());
        }
    }

    void PcieSriovVfPool::flr_vf(uint16_t vf_id) noexcept {
        // vf_id=0 是 PF, 拒绝; 合法 VF 范围 1..16
        if (vf_id < 1 || vf_id >= NUM_PORTS) {
            return;
        }
        config_pool_.config_of(vf_id).init();
        msix_pool_.table_of(vf_id).init();
        completions_.flr_vf(vf_id);
        tlp_seq_[vf_id] = 0;
        fc_engine_.install_bucket(vf_id, FcTokenBucket());
    }

} // namespace tlm::pcie
