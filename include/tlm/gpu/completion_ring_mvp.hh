// completion_ring_mvp.hh
// CompletionRing MVP: warp 完成事件推送 + host_notify 钩子
// Author: CppTLM Team
// Date: 2026-08-26
//
// 语义:
//   - on_warp_complete(task_id, status): 推送一条完成 entry 到环形缓冲
//   - tick(): 每周期消费一条 entry 并触发 host_notify (exactly-once)
//   - host_notify 每次 on_warp_complete 恰好触发一次 (无丢失, 无重复)
#ifndef CPPTLM_COMPLETION_RING_MVP_H
#define CPPTLM_COMPLETION_RING_MVP_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>

namespace tlm::gpu {

    class CompletionRing {
    public:
        // 默认环形缓冲容量
        static constexpr size_t DEFAULT_CAPACITY = 64;

        using HostNotifyFn = std::function<void(uint32_t task_id, int32_t status)>;

        CompletionRing();
        explicit CompletionRing(size_t capacity);
        ~CompletionRing();

        CompletionRing(const CompletionRing&) = delete;
        CompletionRing& operator=(const CompletionRing&) = delete;
        CompletionRing(CompletionRing&&) = delete;
        CompletionRing& operator=(CompletionRing&&) = delete;

        // 推送完成 entry; 返回 false 表示环形缓冲已满 (不覆盖, 不丢失语义由调用方保证)
        bool on_warp_complete(uint32_t task_id, int32_t status);

        // 推进一个周期: 消费一条 entry 并触发 host_notify (exactly-once)
        void tick();

        // 设置 host 通知回调
        void set_host_notify(HostNotifyFn fn);

        // 在途 (已推送未通知) entry 数, 调试用
        size_t inflight_count() const;

        size_t capacity() const;

    private:
        struct Entry {
            uint32_t task_id;
            int32_t status;
        };

        std::deque<Entry> ring_;  // FIFO 环形缓冲 (MVP 用 deque 实现)
        size_t capacity_ = DEFAULT_CAPACITY;
        HostNotifyFn host_notify_;
    };

} // namespace tlm::gpu

#endif // CPPTLM_COMPLETION_RING_MVP_H
