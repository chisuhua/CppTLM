// submit_queue_mvp.hh
// SubmitQueueTLM - ChStream 组件 (SQ tail + 32-slot pending FIFO + done_in 释放)
// Per board-soc-split design §3.5 ports table + ADR-SOC-07 D1
// Author: CppTLM Team
// Date: 2026-08-26 (updated 2026-08-29: promote to ChStreamModuleBase)
//
// 依据 docs/research/WDUtoSM/overview.md NVIDIA Hopper:
//   - per-cluster pending FIFO: 32 槽
//   - per-core active 槽: 4 槽
//   - MVP 阶段: 单 SM 路由 (select_target_core 始终返回 0)
//   - 容量满拒绝不驱逐 (per Oracle M3 + Phase F-D.2 H5)
#ifndef CPPTLM_SUBMIT_QUEUE_MVP_H
#define CPPTLM_SUBMIT_QUEUE_MVP_H

#include "bundles/cpphdl_types.hh"
#include "bundles/dma_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace bundles {

struct CtaDescriptorBundle : public bundle_base {
    ch_uint<32> task_id;
    ch_uint<64> vram_image_addr;
    ch_uint<32> grid_x;
    ch_uint<32> grid_y;
    ch_uint<32> grid_z;
    ch_uint<32> block_x;
    ch_uint<32> block_y;
    ch_uint<32> block_z;
    ch_uint<32> shared_mem_bytes;
    ch_uint<64> args_vram_addr;

    CtaDescriptorBundle() = default;
};

} // namespace bundles

namespace tlm::gpu {

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

    class SubmitQueueTLM : public ChStreamModuleBase {
    public:
        static constexpr size_t PENDING_FIFO_SIZE = 32;
        static constexpr size_t ACTIVE_SLOTS_PER_CORE = 4;
        static constexpr uint8_t TARGET_CORE_MVP = 0;

        explicit SubmitQueueTLM(const std::string& n, EventQueue* eq);
        ~SubmitQueueTLM() override = default;

        SubmitQueueTLM(const SubmitQueueTLM&) = delete;
        SubmitQueueTLM& operator=(const SubmitQueueTLM&) = delete;

        std::string get_module_type() const override { return "SubmitQueueTLM"; }
        void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
            adapter_ = adapter;
        }

        bool enqueue(const CtaDescriptor& cta);
        bool dequeue(CtaDescriptor* out);

        void tick() override;

        void on_warp_complete(uint32_t task_id, int32_t status);

        uint8_t select_target_core(const CtaDescriptor& cta) const;

        size_t pending_count() const { return pending_count_; }
        size_t active_count() const { return active_count_; }
        size_t inflight_count() const { return pending_count_ + active_count_; }
        bool is_full() const { return pending_count_ >= PENDING_FIFO_SIZE; }

        uint64_t backpressure_count() const { return backpressure_count_; }
        uint64_t completed_count() const { return completed_count_; }

    private:
        cpptlm::InputStreamAdapter<bundles::CtaDescriptorBundle> cta_in_;
        cpptlm::InputStreamAdapter<bundles::CompletionBundle> done_in_;
        cpptlm::StreamAdapterBase* adapter_ = nullptr;

        std::array<CtaDescriptor, PENDING_FIFO_SIZE> pending_;
        size_t pending_head_ = 0;
        size_t pending_tail_ = 0;
        size_t pending_count_ = 0;

        std::array<CtaDescriptor, ACTIVE_SLOTS_PER_CORE> active_;
        size_t active_count_ = 0;

        uint64_t backpressure_count_ = 0;
        uint64_t completed_count_ = 0;
    };

    using SubmitQueue = SubmitQueueTLM;

} // namespace tlm::gpu

#endif // CPPTLM_SUBMIT_QUEUE_MVP_H
