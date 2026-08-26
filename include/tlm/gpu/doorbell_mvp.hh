// doorbell_mvp.hh
// Doorbell MVP: SQ tail register 写入 + 强序写语义
// Author: CppTLM Team
// Date: 2026-08-26
//
// 依据 docs/research/PCIe/PCIe_上的保序write.md §4:
//   PCIe doorbell MMIO 写延迟区间为 250-700ns。
// 语义:
//   - ring(stream_id, wdu_offset): 发起一次 SQ tail 写入(强序)
//   - 同一 stream_id 上的多次 ring 按调用顺序对 SQ 可见(strong-order)
//   - tick(): 推进内部周期计数器, 完成到期写入并更新可见 tail
#ifndef CPPTLM_DOORBELL_MVP_H
#define CPPTLM_DOORBELL_MVP_H

#include <cstdint>
#include <deque>
#include <unordered_map>

namespace tlm::gpu {

    class Doorbell {
    public:
        // PCIe doorbell 写延迟区间 (ns), per PCIe_上的保序write.md §4
        static constexpr uint64_t MIN_LATENCY_NS = 250;
        static constexpr uint64_t MAX_LATENCY_NS = 700;
        // 每 stream 最大在途写深度
        static constexpr size_t MAX_PENDING_PER_STREAM = 64;

        Doorbell();
        ~Doorbell();

        Doorbell(const Doorbell&) = delete;
        Doorbell& operator=(const Doorbell&) = delete;
        Doorbell(Doorbell&&) = delete;
        Doorbell& operator=(Doorbell&&) = delete;

        // 初始化: cycle_ns 为每个周期对应的纳秒数 (默认 1ns/cycle)
        void init(uint64_t cycle_ns = 1);

        // 发起一次 SQ tail 写入; 返回 false 表示该 stream 在途队列已满
        bool ring(uint32_t stream_id, uint64_t wdu_offset);

        // 推进一个周期, 完成到期写入
        void tick();

        // 该 stream 是否仍有在途写
        bool is_pending(uint32_t stream_id) const;

        // 原子可见的 SQ tail 指针 (最近一次已完成写入的 wdu_offset)
        uint64_t sq_tail(uint32_t stream_id) const;

        // 当前仿真周期数
        uint64_t now_cycles() const;

    private:
        struct PendingWrite {
            uint64_t wdu_offset;      // 写入值 (新 tail)
            uint64_t complete_cycle;  // 完成周期
        };

        struct StreamState {
            std::deque<PendingWrite> pending;  // FIFO, 保证同 stream 强序
            uint64_t visible_tail = 0;         // 最近完成的 tail
        };

        // 由 (stream_id, wdu_offset) 确定性推导 [250,700]ns 内的延迟
        uint64_t latency_ns(uint32_t stream_id, uint64_t wdu_offset) const;

        std::unordered_map<uint32_t, StreamState> streams_;
        uint64_t cycle_ns_ = 1;
        uint64_t now_ = 0;
    };

} // namespace tlm::gpu

#endif // CPPTLM_DOORBELL_MVP_H
