// include/core/chstream_port.hh
// ChStream 模块专用端口实现：不依赖 PortManager 的轻量级端口
// 功能描述：提供 ChStream 模块使用的具体端口类，实现 MasterPort 和 SlavePort 接口
// 作者：CppTLM Team
// 日期：2026-04-12
#ifndef CHSTREAM_PORT_HH
#define CHSTREAM_PORT_HH

#include "core/stream_adapter_base.hh"
#include "packet.hh"
#include "packet_pool.hh"
#include "event_queue.hh"
#include <queue>

namespace cpptlm {

class ChStreamInitiatorPort : public MasterPort {
private:
    EventQueue* eq_;
    std::queue<Packet*> queue_;

public:
    explicit ChStreamInitiatorPort(std::string name, EventQueue* eq)
        : MasterPort(name), eq_(eq) {}

    bool recvResp(Packet* pkt) override {
        queue_.push(pkt);
        return true;
    }

    void tick() override {
    }

    uint64_t getCurrentCycle() const override {
        return eq_->getCurrentCycle();
    }

    SimObject* getOwner() override {
        return nullptr;
    }

    Packet* drainResponse() {
        if (queue_.empty()) {
            return nullptr;
        }
        Packet* pkt = queue_.front();
        queue_.pop();
        return pkt;
    }

    bool hasResponse() const {
        return !queue_.empty();
    }
};

class ChStreamTargetPort : public SlavePort {
private:
    StreamAdapterBase* adapter_;
    EventQueue* eq_;
    unsigned port_idx_ = 0;  // P0-5b fix: 多端口路由必须知道自己的端口索引

public:
    ChStreamTargetPort(std::string name, StreamAdapterBase* adapter, EventQueue* eq,
                       unsigned port_idx = 0)
        : SlavePort(name), adapter_(adapter), eq_(eq), port_idx_(port_idx) {}

    bool recvReq(Packet* pkt) override {
        if (adapter_) {
            // P0-5b fix: 多端口 adapter 需要知道请求来自哪个端口
            adapter_->process_request_input(pkt, port_idx_);
        }
        PacketPool::get().release(pkt);
        return true;
    }

    void tick() override {
    }

    uint64_t getCurrentCycle() const override {
        return eq_->getCurrentCycle();
    }

    SimObject* getOwner() override {
        return nullptr;
    }
};

}

#endif // CHSTREAM_PORT_HH