// src/tlm/gpu/sdma_ring_buffer.cc
// SdmaRingBuffer 实现 (Stage 1.3a, SD-RB-1)
// 作者 CppTLM Team / 日期 2026-09-13

#include "tlm/gpu/sdma_ring_buffer.hh"

#include <algorithm>
#include <cstring>

namespace tlm::gpu {

    namespace {
        // RingSize 枚举 → bytes 映射 (per spec ring_size ∈ {4K,8K,16K,64K})
        constexpr uint32_t ring_size_bytes(SdmaRingBuffer::RingSize r) {
            switch (r) {
                case SdmaRingBuffer::RingSize::KB_4:
                    return 4u * 1024u;
                case SdmaRingBuffer::RingSize::KB_8:
                    return 8u * 1024u;
                case SdmaRingBuffer::RingSize::KB_16:
                    return 16u * 1024u;
                case SdmaRingBuffer::RingSize::KB_64:
                    return 64u * 1024u;
            }
            return 0u;  // unreachable
        }
    }  // namespace

    SdmaRingBuffer::SdmaRingBuffer(RingSize cfg_size, EntrySize entry_sz)
        : cfg_size_bytes_(ring_size_bytes(cfg_size)),
          entry_size_bytes_(static_cast<uint32_t>(entry_sz)) {
        // capacity_entries = cfg_size / entry_size, capped at kMaxEntries
        // per spec "max 1024 entries @64B" — 32B entry 也夹到 1024
        const uint32_t raw_entries = cfg_size_bytes_ / entry_size_bytes_;
        capacity_entries_ = std::min(raw_entries, kMaxEntries);
        storage_.resize(static_cast<size_t>(capacity_entries_) *
                        static_cast<size_t>(entry_size_bytes_));
    }

    bool SdmaRingBuffer::write_entry(uint32_t index, const uint8_t* data, size_t len) {
        if (index >= capacity_entries_ || data == nullptr) {
            return false;
        }
        const size_t offset = static_cast<size_t>(index) *
                              static_cast<size_t>(entry_size_bytes_);
        const size_t copy_len = std::min(len, static_cast<size_t>(entry_size_bytes_));
        std::memcpy(storage_.data() + offset, data, copy_len);
        // 若 copy_len < entry_size_bytes_, 剩余字节保留旧值 (MVP: 不 zero-pad)
        return true;
    }

    std::array<uint8_t, SdmaRingBuffer::kReturnBytes> SdmaRingBuffer::read_entry(
        uint32_t index) const {
        std::array<uint8_t, kReturnBytes> out{};
        if (index >= capacity_entries_) {
            return out;  // 越界 → 全 0
        }
        const size_t offset = static_cast<size_t>(index) *
                              static_cast<size_t>(entry_size_bytes_);
        const size_t copy_len = std::min(static_cast<size_t>(entry_size_bytes_),
                                         static_cast<size_t>(kReturnBytes));
        std::memcpy(out.data(), storage_.data() + offset, copy_len);
        // 32B entry 时 out[32..64) 保持全 0 (default-init)
        return out;
    }

}  // namespace tlm::gpu
