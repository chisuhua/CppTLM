// submit_queue_mvp.cc
// SubmitQueue MVP 实现 - WDU 分发网络
// Author: CppTLM Team
// Date: 2026-08-26

#include "tlm/gpu/submit_queue_mvp.hh"

namespace tlm::gpu {

    SubmitQueue::SubmitQueue() = default;
    SubmitQueue::~SubmitQueue() = default;

    bool SubmitQueue::enqueue(const CtaDescriptor& cta) {
        // 容量检查 (无驱逐,反压信号)
        if (pending_count_ >= PENDING_FIFO_SIZE) {
            backpressure_count_++;
            return false;
        }

        pending_[pending_tail_] = cta;
        pending_tail_ = (pending_tail_ + 1) % PENDING_FIFO_SIZE;
        pending_count_++;
        return true;
    }

    void SubmitQueue::tick() {
        // 派发: 从 pending 移动最多 ACTIVE_SLOTS_PER_CORE 个到 active
        while (active_count_ < ACTIVE_SLOTS_PER_CORE && pending_count_ > 0) {
            active_[active_count_] = pending_[pending_head_];
            active_count_++;

            pending_head_ = (pending_head_ + 1) % PENDING_FIFO_SIZE;
            pending_count_--;
        }
    }

    void SubmitQueue::on_warp_complete(uint32_t task_id, int32_t /*status*/) {
        // 查找 active slot 中匹配 task_id
        for (size_t i = 0; i < active_count_; i++) {
            if (active_[i].task_id == task_id) {
                // Compact: 移动后续元素前移
                for (size_t j = i; j + 1 < active_count_; j++) {
                    active_[j] = active_[j + 1];
                }
                active_count_--;
                completed_count_++;
                return;
            }
        }
        // 未找到: 静默忽略 (防御性 — TMU 可能对已完成任务调用 on_warp_complete)
    }

    uint8_t SubmitQueue::select_target_core(const CtaDescriptor& /*cta*/) const {
        return TARGET_CORE_MVP;  // 单 SM MVP 路由
    }

} // namespace tlm::gpu
