// src/rtl/hybrid_cache_wrapper.cc
// HybridCacheWrapper PIMPL 实现（C++20）
// 功能描述：TLM↔RTL 桥接逻辑，使用 ch_stream<CacheReqBundleRTL> 接口
//           PIMPL 模式隔离 CppHDL 依赖，公共头文件保持 C++17
// 作者 CppTLM Team / 日期 2026-06-07
#include "rtl/hybrid_cache_wrapper.hh"
#include "rtl/hybrid_cache_component.hh"
#include "rtl/fragment_mapper.hh"
#include "core/packet_pool.hh"
#include "ext/transaction_context_ext.hh"

// CppHDL 头
#include "ch.hpp"
#include "chlib/stream.h"
#include "simulator.h"

#include <cstring>
#include <cstdint>

namespace cpptlm {
namespace rtl {

class HybridCacheWrapperImpl {
public:
    ch::ch_device<cpptlm::rtl::HybridCacheComponent> device_;
    ch::Simulator                                   simulator_;

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>*   req_in_  = nullptr;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>* resp_out_ = nullptr;

    Packet* pending_tx_ = nullptr;

    HybridCacheWrapperImpl()
        : device_(),
          simulator_(device_.context(), false) {}

    ~HybridCacheWrapperImpl() {
        if (pending_tx_) {
            PacketPool::get().release(pending_tx_);
            pending_tx_ = nullptr;
        }
    }

    void tick() {
        if (!req_in_ || !resp_out_) return;

        auto& io = device_.instance().io();

        // === Phase 1: TLM→RTL (Bundle → ch_stream<BundleT>) ===
        if (pending_tx_ == nullptr && req_in_->valid()) {
            const auto& src = req_in_->data();

            pending_tx_ = PacketPool::get().acquire();
            pending_tx_->payload->set_address(src.address.read());
            pending_tx_->payload->set_data_length(sizeof(uint64_t));
            if (src.data.read() != 0) {
                uint64_t d = src.data.read();
                std::memcpy(pending_tx_->payload->get_data_ptr(), &d, sizeof(uint64_t));
            }

            const TransactionContextExt* src_ext = nullptr;
            pending_tx_->payload->get_extension(src_ext);

            pending_tx_->payload->template release_extension<TransactionContextExt>();
            auto* new_ext = new TransactionContextExt();
            if (src_ext) {
                new_ext->transaction_id = src.transaction_id.read();
                new_ext->parent_id      = src.parent_id.read();
                new_ext->fragment_id    = src.fragment_id.read();
                new_ext->fragment_total = src.fragment_total.read();
            } else {
                new_ext->transaction_id = src.transaction_id.read();
                new_ext->parent_id      = src.parent_id.read();
                new_ext->fragment_id    = 0;
                new_ext->fragment_total = 1;
            }
            pending_tx_->payload->template set_extension<TransactionContextExt>(new_ext);
            pending_tx_->set_transaction_id(new_ext->transaction_id);
        }

        if (pending_tx_) {
            const TransactionContextExt* ext = get_transaction_context(pending_tx_->payload);
            auto beat = FragmentMapper::serialize_beat_at(pending_tx_, 0);

            // === Phase 2: 驱动 ch_stream<CacheReqBundleRTL> ===
            simulator_.set_value(io.req_in.payload.transaction_id, beat.tid);
            simulator_.set_value(io.req_in.payload.parent_id,      beat.parent_id);
            simulator_.set_value(io.req_in.payload.fragment_id,    beat.fragment_id);
            simulator_.set_value(io.req_in.payload.fragment_total, beat.fragment_total);
            simulator_.set_value(io.req_in.payload.address,        beat.addr);
            simulator_.set_value(io.req_in.payload.data,           beat.data);
            simulator_.set_value(io.req_in.payload.size,           beat.strb);
            simulator_.set_value(io.req_in.payload.is_write,       beat.last);
            simulator_.set_value(io.req_in.valid,                  1u);

            simulator_.tick();

            // === Phase 3: 读取 ch_stream<CacheRespBundleRTL> ===
            if (io.resp_out.valid) {
                CacheRespBeatRTL resp_beat;
                resp_beat.tid            = static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.transaction_id));
                resp_beat.parent_id      = static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.parent_id));
                resp_beat.fragment_id    = static_cast<uint8_t>(static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.fragment_id)));
                resp_beat.fragment_total = static_cast<uint8_t>(static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.fragment_total)));
                resp_beat.data           = static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.data));
                resp_beat.hit            = static_cast<bool>(static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.is_hit)));
                resp_beat.error_code     = static_cast<uint8_t>(static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.error_code)));
                resp_beat.first          = static_cast<bool>(static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.first)));
                resp_beat.last           = static_cast<bool>(static_cast<uint64_t>(simulator_.get_value(io.resp_out.payload.last)));

                // 构造响应 Packet + Extension
                Packet* resp_pkt = PacketPool::get().acquire();
                resp_pkt->payload->set_data_length(sizeof(uint64_t));
                FragmentMapper::write_resp(resp_pkt, resp_beat);

                // 构造 TLM CacheRespBundle
                bundles::CacheRespBundle resp_bundle;
                resp_bundle.transaction_id.write(resp_beat.tid);
                resp_bundle.parent_id.write(resp_beat.parent_id);
                resp_bundle.fragment_id.write(resp_beat.fragment_id);
                resp_bundle.fragment_total.write(resp_beat.fragment_total);
                resp_bundle.data.write(resp_beat.data);
                resp_bundle.is_hit.write(resp_beat.hit);
                resp_bundle.error_code.write(resp_beat.error_code);
                resp_bundle.first.write(resp_beat.first);
                resp_bundle.last.write(resp_beat.last);

                resp_out_->write(resp_bundle);
                PacketPool::get().release(resp_pkt);

                // 拉低 valid（下一拍重新接受）
                simulator_.set_value(io.resp_out.valid, 0u);
            }

            // 消费 TLM 输入
            req_in_->consume();
            PacketPool::get().release(pending_tx_);
            pending_tx_ = nullptr;
        }
    }

    void reset() {
        if (pending_tx_) {
            PacketPool::get().release(pending_tx_);
            pending_tx_ = nullptr;
        }
        simulator_.reset();
    }
};

HybridCacheWrapper::HybridCacheWrapper(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq),
      impl_(std::make_unique<HybridCacheWrapperImpl>()) {}

HybridCacheWrapper::~HybridCacheWrapper() = default;

void HybridCacheWrapper::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    adapter_ = adapter;
}

void HybridCacheWrapper::tick() {
    if (impl_) {
        impl_->req_in_ = &req_in_;
        impl_->resp_out_ = &resp_out_;
        impl_->tick();
    }
    if (adapter_) adapter_->tick();
}

void HybridCacheWrapper::do_reset(const ResetConfig& cfg) {
    if (impl_) impl_->reset();
    req_in_.reset();
    resp_out_.reset();
    SimObject::do_reset(cfg);
}

} // namespace rtl
} // namespace cpptlm
