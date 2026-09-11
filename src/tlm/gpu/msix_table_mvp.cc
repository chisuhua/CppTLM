// src/tlm/gpu/msix_table_mvp.cc
// MsiXTable 实现
// 作者 CppTLM Team / 日期 2026-08-26

#include "tlm/gpu/msix_table_mvp.hh"

#include <algorithm>
#include <stdexcept>

namespace tlm::gpu {

    MsiXTable::MsiXTable(uint16_t num_vectors) : num_vectors_(num_vectors) {
        if (num_vectors == 0) {
            throw std::invalid_argument("MsiXTable: num_vectors must be > 0");
        }
        entries_.resize(num_vectors);
        pba_.resize(num_vectors, 0); // PBA bits per vector, init = 0
    }

    void MsiXTable::init() {
        for (auto& e : entries_) {
            e.msg_addr = 0;
            e.msg_data = 0;
            e.control = 0; // mask bit = 0 (unmasked by default)
        }
        std::fill(pba_.begin(), pba_.end(), 0); // clear PBA bits
        pending_irq_out_.clear();
        did_deliver_last_update_ = false; // 重置投递标志 (隔离跨操作状态)
    }

    bool MsiXTable::resize(uint16_t new_n) {
        if (new_n == 0)
            return false; // 保留构造器 > 0 不变式

        if (new_n < num_vectors_) {
            // 缩表: 丢弃 vector >= new_n 的 pending IRQ 事件 (PBA/entries 截断同步)
            std::deque<IrqOutEvent> kept;
            for (const auto& evt : pending_irq_out_) {
                if (evt.vector < new_n) {
                    kept.push_back(evt);
                }
            }
            pending_irq_out_.swap(kept);
        }
        // 扩表: std::vector::resize 对新 VectorEntry 槽位 value-initialize
        // (msg_addr/msg_data/control 默认清零); pba_ 显式 0 填充新槽位
        entries_.resize(new_n);
        pba_.resize(new_n, 0);
        num_vectors_ = new_n;
        // did_deliver_last_update_ 语义跨 resize 不变 (仅 update_pending/init/
        // clear_pending 维护), 保持既有调用方 (DGpuBoard wrapper) 的判定一致
        return true;
    }

    bool MsiXTable::configure_vector(uint16_t vector, uint64_t msg_addr, uint32_t msg_data,
                                     uint32_t control) {
        if (vector >= num_vectors_)
            return false;
        entries_[vector].msg_addr = msg_addr;
        entries_[vector].msg_data = msg_data;
        entries_[vector].control = control;
        return true;
    }

    bool MsiXTable::set_mask(uint16_t vector, bool masked) {
        if (vector >= num_vectors_)
            return false;
        const bool was_masked = (entries_[vector].control & 0x1u) != 0;
        if (masked) {
            entries_[vector].control |= 0x1u; // bit 0 = mask
        } else {
            entries_[vector].control &= ~0x1u;
            // PBA semantics: unmask 后, 若 PBA 已置位, 投递累积 IRQ
            // (per PCI-SIG MSI-X ECN + T-prereq-3 spec Scenario "mask 期间 pending 不丢失")
            if (was_masked) {
                try_deliver_pending_on_unmask(vector);
            }
        }
        return true;
    }

    bool MsiXTable::clear_mask(uint16_t vector) {
        return set_mask(vector, false);
    }

    bool MsiXTable::is_masked(uint16_t vector) const {
        if (vector >= num_vectors_)
            return false;
        return (entries_[vector].control & 0x1u) != 0;
    }

    bool MsiXTable::update_pending(uint16_t vector, uint32_t trans_id) {
        did_deliver_last_update_ = false; // 每次 update_pending 重置投递标志
        if (vector >= num_vectors_)
            return false;

        // PBA semantics (per T-prereq-3): 无论 mask 状态都置 PBA bit
        // → masked 时不丢失 pending, unmask 后自动投递
        pba_[vector] = 1;

        // 触发前判断 mask：masked vector 仅置 PBA, 不入队 (per PCI-SIG ECN)
        if (is_masked(vector))
            return true; // accepted (PBA set); not yet delivered to queue

        // 未 mask: 同步入队 IRQ 事件
        const auto& entry = entries_[vector];
        IrqOutEvent evt;
        evt.vector = vector;
        evt.msg_data = entry.msg_data;
        evt.msg_addr = entry.msg_addr;
        evt.trans_id = trans_id;
        pending_irq_out_.push_back(evt);
        did_deliver_last_update_ = true; // 真正投递到出向队列
        return true;
    }

    bool MsiXTable::clear_pending(uint16_t vector) {
        did_deliver_last_update_ = false; // 重置投递标志 (隔离跨操作状态)
        if (vector >= num_vectors_)
            return false;
        bool cleared = false;
        // 清 PBA bit (PBA semantics)
        if (pba_[vector] != 0) {
            pba_[vector] = 0;
            cleared = true;
        }
        // pending_irq_out_ 是 FIFO，按 vector 匹配并删除第一个匹配项
        for (auto it = pending_irq_out_.begin(); it != pending_irq_out_.end(); ++it) {
            if (it->vector == vector) {
                pending_irq_out_.erase(it);
                cleared = true;
                break;
            }
        }
        return cleared;
    }

    bool MsiXTable::is_pending(uint16_t vector) const {
        if (vector >= num_vectors_)
            return false;
        // 兼容旧 API: 检查 irq_out 队列 + PBA (PBA 包含累计待处理)
        if (pba_[vector] != 0)
            return true;
        for (const auto& evt : pending_irq_out_) {
            if (evt.vector == vector)
                return true;
        }
        return false;
    }

    bool MsiXTable::is_pba_set(uint16_t vector) const {
        if (vector >= num_vectors_)
            return false;
        return pba_[vector] != 0;
    }

    void MsiXTable::try_deliver_pending_on_unmask(uint16_t vector) {
        // 若 PBA 位置位且 vector 未 mask, 投递累积 IRQ
        if (vector >= num_vectors_)
            return;
        if (pba_[vector] == 0)
            return;
        if (is_masked(vector)) // 防御性检查: 已被设为 mask
            return;

        const auto& entry = entries_[vector];
        IrqOutEvent evt;
        evt.vector = vector;
        evt.msg_data = entry.msg_data;
        evt.msg_addr = entry.msg_addr;
        evt.trans_id = 0;
        pending_irq_out_.push_back(evt);
        // 注意: 不清 PBA bit; 由 driver EOI (clear_pending) 清除
    }

    const MsiXTable::IrqOutEvent* MsiXTable::try_pop_irq_out() {
        if (pending_irq_out_.empty())
            return nullptr;
        return &pending_irq_out_.front();
    }

    void MsiXTable::consume_irq_out() {
        if (!pending_irq_out_.empty()) {
            pending_irq_out_.pop_front();
        }
    }

    std::size_t MsiXTable::pending_count() const {
        return pending_irq_out_.size();
    }

    std::size_t MsiXTable::pba_count() const {
        return static_cast<std::size_t>(std::count(pba_.begin(), pba_.end(), 1));
    }

} // namespace tlm::gpu