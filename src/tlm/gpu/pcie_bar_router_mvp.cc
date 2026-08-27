// src/tlm/gpu/pcie_bar_router_mvp.cc
// PcieBarRouter 实现
// 作者 CppTLM Team / 日期 2026-08-26

#include "tlm/gpu/pcie_bar_router_mvp.hh"

#include <stdexcept>

namespace tlm::gpu {

    PcieBarRouter::PcieBarRouter() = default;

    void PcieBarRouter::init(uint64_t cycle_ns) {
        regs_.clear();
        pending_doorbell_out_.clear();
        doorbell_.init(cycle_ns);
        now_cycles_ = 0;
    }

    bool PcieBarRouter::add_register(uint32_t offset, const std::string& name,
                                      Access access, SideEffect side_effect,
                                      uint32_t doorbell_stream_id) {
        if ((offset & 0x3) != 0) return false;  // 必须 4-byte 对齐
        if (regs_.count(offset) > 0) return false;
        regs_[offset] = RegisterEntry{offset, name, access, side_effect,
                                       /*value=*/0, doorbell_stream_id};
        return true;
    }

    uint32_t PcieBarRouter::mmio_read(uint32_t offset) const {
        if ((offset & 0x3) != 0) return 0xFFFFFFFFu;
        auto it = regs_.find(offset);
        if (it == regs_.end()) return 0xFFFFFFFFu;
        // WO 寄存器 read 返回 0xFFFFFFFF（PCIe 行为：未实现）
        if (it->second.access == Access::WO) return 0xFFFFFFFFu;
        return it->second.value;
    }

    bool PcieBarRouter::mmio_write(uint32_t offset, uint32_t value, uint32_t trans_id) {
        if ((offset & 0x3) != 0) return false;
        auto it = regs_.find(offset);
        if (it == regs_.end()) return false;
        // RO 寄存器 write 静默拒绝
        if (it->second.access == Access::RO) return false;

        it->second.value = value;

        // 处理 side_effect
        if (it->second.side_effect == SideEffect::DOORBELL) {
            const uint32_t stream_id = it->second.doorbell_stream_id;
            // 调用 Doorbell.ring() 启动强序写
            if (doorbell_.ring(stream_id, value)) {
                enqueue_doorbell_out(it->second, value, trans_id);
            }
        }
        return true;
    }

    void PcieBarRouter::enqueue_doorbell_out(const RegisterEntry& entry, uint32_t value,
                                              uint32_t trans_id) {
        // 计算完成周期：当前 now_cycles_ + 门铃延迟（来自 Doorbell）
        // 由于 Doorbell 内部已经维护 FIFO 完成时间，这里用 stream_id=0 查 sq_tail 不可行，
        // 改用保守估计（按 250-700ns 区间映射到 cycles）。
        // 实际测试通过 doorbell_is_pending()/sq_tail() 配合 tick 推进断言
        // 此处记录 now_cycles_ + MIN_LATENCY（最保守），测试可基于实际延迟断言。
        const uint64_t latency_cycles = Doorbell::MIN_LATENCY_NS;  // 250 cycles (cycle_ns=1)
        DoorbellOutEvent evt;
        evt.stream_id = entry.doorbell_stream_id;
        evt.wdu_offset = value;
        evt.trans_id = trans_id;
        evt.complete_cycle = now_cycles_ + latency_cycles;
        pending_doorbell_out_.push_back(evt);
    }

    void PcieBarRouter::tick() {
        now_cycles_ += 1;
        doorbell_.tick();

        // 弹出到期 doorbell 事件（FIFO 顺序，与 Doorbell 强序对齐）
        while (!pending_doorbell_out_.empty()
               && pending_doorbell_out_.front().complete_cycle <= now_cycles_) {
            // 不弹出 — 由 try_pop_doorbell_out() 在 module tick() 中拉取
            // 但保留完整队列以便测试 pending_doorbell_count()
            break;  // 让外部 API 控制弹出时机（测试可观察 in-flight）
        }
    }

    const PcieBarRouter::DoorbellOutEvent* PcieBarRouter::try_pop_doorbell_out() {
        if (pending_doorbell_out_.empty()) return nullptr;
        if (pending_doorbell_out_.front().complete_cycle > now_cycles_) {
            return nullptr;  // 未到期
        }
        const DoorbellOutEvent* evt = &pending_doorbell_out_.front();
        return evt;  // 返回前端的 const ptr；调用方 consume() 后 pop
    }

    void PcieBarRouter::consume_doorbell_out() {
        if (!pending_doorbell_out_.empty()
            && pending_doorbell_out_.front().complete_cycle <= now_cycles_) {
            pending_doorbell_out_.pop_front();
        }
    }

    std::size_t PcieBarRouter::pending_doorbell_count() const {
        return pending_doorbell_out_.size();
    }

    bool PcieBarRouter::doorbell_is_pending(uint32_t stream_id) const {
        return doorbell_.is_pending(stream_id);
    }

    uint64_t PcieBarRouter::doorbell_sq_tail(uint32_t stream_id) const {
        return doorbell_.sq_tail(stream_id);
    }

    uint64_t PcieBarRouter::doorbell_now_cycles() const {
        return doorbell_.now_cycles();
    }

} // namespace tlm::gpu