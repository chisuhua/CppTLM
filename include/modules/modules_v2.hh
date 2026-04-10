// include/modules/modules_v2.hh
#ifndef MODULES_V2_HH
#define MODULES_V2_HH

#include "core/sim_object.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "ext/transaction_context_ext.hh"
#include "framework/transaction_tracker.hh"
#include <queue>
#include <map>

class CrossbarV2 : public SimObject {
public:
    CrossbarV2(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    void tick() override { while (!input_queue.empty()) { PacketPool::get().release(input_queue.front()); input_queue.pop(); } }
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override { input_queue.push(pkt); return true; }
    std::string get_module_type() const override { return "CrossbarV2"; }
    void do_reset(const ResetConfig&) override { while (!input_queue.empty()) { PacketPool::get().release(input_queue.front()); input_queue.pop(); } }
private:
    std::queue<Packet*> input_queue;
};

class MemoryV2 : public SimObject {
public:
    MemoryV2(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    void tick() override { while (!request_queue.empty()) { PacketPool::get().release(request_queue.front()); request_queue.pop(); } }
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override { request_queue.push(pkt); return true; }
    std::string get_module_type() const override { return "MemoryV2"; }
    void do_reset(const ResetConfig&) override { while (!request_queue.empty()) { PacketPool::get().release(request_queue.front()); request_queue.pop(); } }
private:
    std::queue<Packet*> request_queue;
};

class CacheV2 : public SimObject {
public:
    CacheV2(const std::string& n, EventQueue* eq, size_t = 1024) : SimObject(n, eq) {}
    void tick() override { while (!request_queue.empty()) { PacketPool::get().release(request_queue.front()); request_queue.pop(); } }
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override { request_queue.push(pkt); return true; }
    std::string get_module_type() const override { return "CacheV2"; }
    uint64_t createSubTransaction(Packet* parent, Packet*) { return parent->get_transaction_id() + 1000; }
    void do_reset(const ResetConfig&) override { while (!request_queue.empty()) { PacketPool::get().release(request_queue.front()); request_queue.pop(); } }
private:
    std::queue<Packet*> request_queue;
};

#endif
