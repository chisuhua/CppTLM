// submit_queue_mvp.hh
// SubmitQueue MVP: WDU (Work Distribution Unit) 分发网络
// Author: CppTLM Team
// Date: 2026-08-26
//
// 依据 docs/research/WDUtoSM/overview.md NVIDIA Hopper:
//   - per-cluster pending FIFO: 32 槽
//   - per-core active 槽: 4 槽
//   - MVP 阶段: 单 SM 路由 (select_target_core 始终返回 0)
//   - 容量满拒绝不驱逐 (per Oracle M3 + Phase F-D.2 H5)
#ifndef CPPTLM_SUBMIT_QUEUE_MVP_H
#define CPPTLM_SUBMIT_QUEUE_MVP_H

#include <array>
#include <cstddef>
#include <cstdint>

namespace tlm::gpu {

    // CTA (Cooperative Thread Array) 描述符
    struct CtaDescriptor {
        uint32_t task_id = 0;
        uint64_t vram_image_addr = 0;
        uint32_t grid_x = 0;
        uint32_t grid_y = 0;
        uint32_t grid_z = 0;
        uint32_t block_x = 0;
        uint32_t block_y = 0;
        uint32_t block_z = 0;
        uint32_t shared_mem_bytes = 0;
        uint64_t args_vram_addr = 0;
    };

    // WDU 分发网络 - 简化版 (单 cluster + 单 SM)
    // v0.5 完整版: 升级为多 cluster crossbar 逐周期仲裁
    class SubmitQueue {
    public:
        static constexpr size_t PENDING_FIFO_SIZE = 32;       // per-cluster pending 容量
        static constexpr size_t ACTIVE_SLOTS_PER_CORE = 4;    // per-core active 容量
        static constexpr uint8_t TARGET_CORE_MVP = 0;        // 单 SM MVP 路由

        SubmitQueue();
        ~SubmitQueue();

        SubmitQueue(const SubmitQueue&) = delete;
        SubmitQueue& operator=(const SubmitQueue&) = delete;

        // 入队 WDU (Work Distribution Unit)
        // @return true = 入队成功; false = pending FIFO 满 (反压信号,不驱逐)
        bool enqueue(const CtaDescriptor& cta);

        // 派发: 从 pending 移动最多 ACTIVE_SLOTS_PER_CORE 个 entry 到 active
        void tick();

        // 反向流: warp 完成时释放 active 槽
        // 未知的 task_id 静默忽略 (防御性)
        void on_warp_complete(uint32_t task_id, int32_t status);

        // MVP: 始终返回 0 (单 SM 路由)
        uint8_t select_target_core(const CtaDescriptor& cta) const;

        // 状态查询
        size_t pending_count() const { return pending_count_; }
        size_t active_count() const { return active_count_; }
        size_t inflight_count() const { return pending_count_ + active_count_; }
        bool is_full() const { return pending_count_ >= PENDING_FIFO_SIZE; }

        // 诊断统计
        uint64_t backpressure_count() const { return backpressure_count_; }
        uint64_t completed_count() const { return completed_count_; }

    private:
        // 循环 FIFO (head/tail 索引 + count)
        std::array<CtaDescriptor, PENDING_FIFO_SIZE> pending_;
        size_t pending_head_ = 0;
        size_t pending_tail_ = 0;
        size_t pending_count_ = 0;

        // Active slots (compact array)
        std::array<CtaDescriptor, ACTIVE_SLOTS_PER_CORE> active_;
        size_t active_count_ = 0;

        uint64_t backpressure_count_ = 0;
        uint64_t completed_count_ = 0;
    };

} // namespace tlm::gpu

#endif // CPPTLM_SUBMIT_QUEUE_MVP_H
