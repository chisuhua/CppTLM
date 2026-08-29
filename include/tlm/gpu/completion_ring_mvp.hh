// completion_ring_mvp.hh
// CompletionRingTLM - ChStream 组件 (CQ ring + 4 端口: done_in[0/1]/done_out/irq_out)
// Per board-soc-split design §3.5 ports table + ADR-SOC-07 D1
// Author: CppTLM Team
// Date: 2026-08-31
//
// 语义:
//   - on_warp_complete(task_id, status): 推送一条完成 entry 到环形缓冲
//   - tick(): 每周期消费一条 entry 并触发 host_notify (exactly-once)
//   - host_notify 每次 on_warp_complete 恰好触发一次 (无丢失, 无重复)
//   - done_in[0/1]: CompletionBundle ingress (多源汇聚)
//   - done_out[2]: CompletionBundle egress (dep chain 释放)
//   - irq_out[3]: MsiXDeliveryBundle egress (→ pcie_ep.irq_out 转发)
#ifndef CPPTLM_COMPLETION_RING_TLM_H
#define CPPTLM_COMPLETION_RING_TLM_H

#include "core/chstream_module.hh"
#include "bundles/dma_bundles_tlm.hh"   // CompletionBundle (sdma-engine deliverable)
#include "bundles/pcie_bundles_tlm.hh" // MsiXDeliveryBundle
#include "framework/stream_adapter.hh"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>

// 提供 CompletionEntry 别名，方便内部使用
namespace bundles {
    using CompletionEntry = CompletionBundle;
} // namespace bundles

namespace tlm::gpu {

    class CompletionRingTLM : public ChStreamModuleBase {
    public:
        // 默认环形缓冲容量
        static constexpr size_t DEFAULT_CAPACITY = 64;

        using HostNotifyFn = std::function<void(uint32_t task_id, int32_t status)>;

        explicit CompletionRingTLM(const std::string& n, EventQueue* eq);
        CompletionRingTLM() : CompletionRingTLM("", nullptr) {}  // 向后兼容 (s2 测试)
        explicit CompletionRingTLM(size_t capacity) : CompletionRingTLM() { capacity_ = capacity; }
        ~CompletionRingTLM() override = default;

        CompletionRingTLM(const CompletionRingTLM&) = delete;
        CompletionRingTLM& operator=(const CompletionRingTLM&) = delete;
        CompletionRingTLM(CompletionRingTLM&&) = delete;
        CompletionRingTLM& operator=(CompletionRingTLM&&) = delete;

        // ChStreamModuleBase interface
        std::string get_module_type() const override { return "CompletionRingTLM"; }
        void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
            adapter_ = adapter;
        }
        void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override {
            if (adapters) {
                adapter_ = adapters[0];
            }
        }
        unsigned num_ports() const override { return 4; }  // done_in[0/1] + done_out + irq_out

        // 核心 API (保留 s2 逻辑)
        void push(const bundles::CompletionEntry& entry);
        bool pop(bundles::CompletionEntry* out);
        size_t pending_count() const;

        // 兼容旧 API (s2 测试使用)
        bool on_warp_complete(uint32_t task_id, int32_t status);
        void tick() override;
        void set_host_notify(HostNotifyFn fn);
        size_t inflight_count() const;
        size_t capacity() const;

        // 端口相关占位 (后续 T-bs-5 实现)
        // done_in[0] 和 done_in[1] 通过适配器接收 CompletionBundle
        // done_out 和 irq_out 通过适配器发送

    private:
        std::deque<bundles::CompletionEntry> ring_;
        static constexpr size_t kRingSize = 4096;  // per NVIDIA spec, MVP 暂用 1024
        size_t capacity_ = DEFAULT_CAPACITY;
        HostNotifyFn host_notify_;
        cpptlm::StreamAdapterBase* adapter_ = nullptr;
    };

    // 向后兼容别名
    using CompletionRing = CompletionRingTLM;

} // namespace tlm::gpu

#endif // CPPTLM_COMPLETION_RING_TLM_H