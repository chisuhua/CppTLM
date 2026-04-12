// include/core/chstream_port.hh
// ChStream 模块专用端口实现：不依赖 PortManager 的轻量级端口
// 功能描述：提供 ChStream 模块使用的具体端口类，实现 MasterPort 和 SlavePort 接口
// 作者：CppTLM Team
// 日期：2026-04-12
#ifndef CHSTREAM_PORT_HH
#define CHSTREAM_PORT_HH

#pragma once
#include "simple_port.hh"
#include "packet.hh"
#include "packet_pool.hh"
#include "event_queue.hh"
#include <queue>

namespace cpptlm { class StreamAdapterBase; }

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

    SimObject* getOwner() const override {
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

public:
    ChStreamTargetPort(std::string name, StreamAdapterBase* adapter, EventQueue* eq)
        : SlavePort(name), adapter_(adapter), eq_(eq) {}

    bool recvReq(Packet* pkt) override {
        if (adapter_) {
            adapter_->process_request_input(pkt);
        }
        PacketPool::get().release(pkt);
        return true;
    }

    void tick() override {
    }

    uint64_t getCurrentCycle() const override {
        return eq_->getCurrentCycle();
    }

    SimObject* getOwner() const override {
        return nullptr;
    }
};

}

#endif // CHSTREAM_PORT_HH