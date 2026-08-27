// src/tlm/gpu/msix_table_mvp.cc
// MsiXTable 实现
// 作者 CppTLM Team / 日期 2026-08-26

#include "tlm/gpu/msix_table_mvp.hh"

#include <stdexcept>

namespace tlm::gpu {

    MsiXTable::MsiXTable(uint16_t num_vectors) : num_vectors_(num_vectors) {
        if (num_vectors == 0) {
            throw std::invalid_argument("MsiXTable: num_vectors must be > 0");
        }
        entries_.resize(num_vectors);
    }

    void MsiXTable::init() {
        for (auto& e : entries_) {
            e.msg_addr = 0;
            e.msg_data = 0;
            e.control = 0; // mask bit = 0 (unmasked by default)
        }
        pending_irq_out_.clear();
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
        if (masked) {
            entries_[vector].control |= 0x1u; // bit 0 = mask
        } else {
            entries_[vector].control &= ~0x1u;
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
        if (vector >= num_vectors_)
            return false;

        // 触发前判断 mask：masked vector 不投递
        if (is_masked(vector))
            return false;

        const auto& entry = entries_[vector];
        IrqOutEvent evt;
        evt.vector = vector;
        evt.msg_data = entry.msg_data;
        evt.msg_addr = entry.msg_addr;
        evt.trans_id = trans_id;
        pending_irq_out_.push_back(evt);
        return true;
    }

    bool MsiXTable::clear_pending(uint16_t vector) {
        if (vector >= num_vectors_)
            return false;
        // pending_irq_out_ 是 FIFO，按 vector 匹配并删除第一个匹配项
        for (auto it = pending_irq_out_.begin(); it != pending_irq_out_.end(); ++it) {
            if (it->vector == vector) {
                pending_irq_out_.erase(it);
                return true;
            }
        }
        return false;
    }

    bool MsiXTable::is_pending(uint16_t vector) const {
        if (vector >= num_vectors_)
            return false;
        for (const auto& evt : pending_irq_out_) {
            if (evt.vector == vector)
                return true;
        }
        return false;
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

} // namespace tlm::gpu