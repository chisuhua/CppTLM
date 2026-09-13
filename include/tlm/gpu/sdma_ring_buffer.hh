// include/tlm/gpu/sdma_ring_buffer.hh
// SdmaRingBuffer: SDMA Ring Buffer (Stage 1.3a, SD-RB-1)
// 功能描述：cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}, entry_size ∈ {32B, 64B},
//           32-bit RPTR/WPTR 原子，max 1024 entries @64B。
//           物理位置：SdmaEngineTLM 内部存储 (与 backdoor 注入模式同构)，
//           数据通路走 BAR1 窗口经 PcieBarRouter 转发。
// 作者 CppTLM Team / 日期 2026-09-13
// 参考: openspec/changes/2026-09-10-cpptlm-stage-1-3-sdma/spec.md
//       Scenario "Ring Buffer 4 config sizes"
//       Scenario "RPTR/WPTR atomic 32-bit"
//       Scenario "Doorbell write triggers"
#ifndef CPPTLM_TLM_GPU_SDMA_RING_BUFFER_HH
#define CPPTLM_TLM_GPU_SDMA_RING_BUFFER_HH

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace tlm::gpu {

    /**
     * @brief SDMA Ring Buffer（Stage 1.3a 新增类）
     *
     * 设计原则 (per spec.md):
     *   - cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}
     *   - entry_size ∈ {32B, 64B}
     *   - entry_count = cfg_size / entry_size, capped at 1024
     *   - RPTR/WPTR: std::atomic<uint32_t>, wptr ≥ rptr invariant
     *   - 物理位置：SdmaEngineTLM 内部存储 (单写者 WPTR + tick 消费 RPTR)
     *   - Doorbell 触发：WPTR 写入 BAR1+0x10010000 → ring[RPTR..WPTR] 被消费
     *
     * Wire-format (Stage 1.3a 1.3a):
     *   - 每个 entry 64B (与 32B entry 不冲突; read_entry 始终返回 64B,
     *     32B entry 时仅前 32B 有效, 后 32B zero-pad)
     *   - Ring 物理位置 = entry_size × capacity_entries (≤ cfg_size)
     */
    class SdmaRingBuffer {
    public:
        enum class RingSize : uint8_t {
            KB_4 = 0,
            KB_8 = 1,
            KB_16 = 2,
            KB_64 = 3,
        };

        enum class EntrySize : uint8_t {
            B_32 = 32,
            B_64 = 64,
        };

        // spec: "max 1024 entries @64B" — 硬上限 (即使 ring_size=64KB + entry_size=32B
        //      算出来是 2048 entries, 也夹到 1024)
        static constexpr uint32_t kMaxEntries = 1024;

        // 固定返回大小 (per test_sdma_ring_buffer.cc read_entry.size() == 64u)
        static constexpr size_t kReturnBytes = 64;

        SdmaRingBuffer(RingSize cfg_size, EntrySize entry_sz);
        ~SdmaRingBuffer() = default;

        SdmaRingBuffer(const SdmaRingBuffer&) = delete;
        SdmaRingBuffer& operator=(const SdmaRingBuffer&) = delete;

        // 容量查询
        uint32_t capacity_bytes() const {
            return cfg_size_bytes_;
        }
        uint32_t capacity_entries() const {
            return capacity_entries_;
        }
        uint32_t entry_size_bytes() const {
            return entry_size_bytes_;
        }

        // RPTR/WPTR 原子访问 (per spec: std::atomic<uint32_t>)
        // 注意: 返回非 const 引用 (原子操作需要 mutable, 不提供 const overload)
        std::atomic<uint32_t>& rptr() {
            return rptr_;
        }
        std::atomic<uint32_t>& wptr() {
            return wptr_;
        }

        // 推进指针 (单步 fetch_add, 内存序 acq_rel 保证可见性)
        void advance_wptr(uint32_t n) {
            wptr_.fetch_add(n, std::memory_order_acq_rel);
        }
        void advance_rptr(uint32_t n) {
            rptr_.fetch_add(n, std::memory_order_acq_rel);
        }

        // ready_count = wptr - rptr (per spec "wptr ≥ rptr always")
        uint32_t ready_count() const {
            return wptr_.load(std::memory_order_acquire) -
                   rptr_.load(std::memory_order_acquire);
        }

        // 写 entry: memcpy min(len, entry_size_bytes_) 到 storage_[idx*entry_size]
        //   越界 index 返回 false (MVP: 仅 assert + no-op, 不抛错)
        bool write_entry(uint32_t index, const uint8_t* data, size_t len = kReturnBytes);

        // 读 entry: 返回 std::array<uint8_t, 64>, 32B entry 时后 32B zero-pad
        std::array<uint8_t, kReturnBytes> read_entry(uint32_t index) const;

    private:
        uint32_t cfg_size_bytes_;       // 配置的 ring_size (bytes)
        uint32_t entry_size_bytes_;     // 每个 entry 大小 (32 or 64)
        uint32_t capacity_entries_;     // 实际可用 entry 数 (≤ cfg/entry_size, ≤ kMaxEntries)
        std::vector<uint8_t> storage_;  // size = capacity_entries_ * entry_size_bytes_
        std::atomic<uint32_t> rptr_{0};
        std::atomic<uint32_t> wptr_{0};
    };

}  // namespace tlm::gpu

#endif  // CPPTLM_TLM_GPU_SDMA_RING_BUFFER_HH
