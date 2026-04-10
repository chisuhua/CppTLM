// include/modules/modules_v2.hh
// CppTLM v2.0 模块实现
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
    CrossbarV2(const std::string& name, EventQueue* eq)
        : SimObject(name, eq), packets_received(0), packets_forwarded(0) {}
    
    void tick() override {
        while (!input_queue.empty()) {
            Packet* pkt = input_queue.front();
            input_queue.pop();
            pkt->add_trace(name, getCurrentCycle(), 1, "hopped");
            auto& tracker = TransactionTracker::instance();
            tracker.record_hop(pkt->get_transaction_id(), name, 1, "hopped");
            packets_forwarded++;
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override {
        input_queue.push(pkt);
        packets_received++;
        return true;
    }
    
    std::string get_module_type() const override { return "CrossbarV2"; }
    
    void do_reset(const ResetConfig&) override {
        while (!input_queue.empty()) {
            Packet* pkt = input_queue.front();
            input_queue.pop();
            PacketPool::get().release(pkt);
        }
        packets_received = 0;
        packets_forwarded = 0;
    }

private:
    std::queue<Packet*> input_queue;
    uint64_t packets_received;
    uint64_t packets_forwarded;
};

class MemoryV2 : public SimObject {
public:
    MemoryV2(const std::string& name, EventQueue* eq)
        : SimObject(name, eq), reads(0), writes(0), errors(0) {}
    
    void tick() override {
        while (!request_queue.empty()) {
            Packet* req = request_queue.front();
            request_queue.pop();
            uint64_t addr = req->payload ? req->payload->get_address() : 0;
            if (addr >= memory_size) {
                req->set_error_code(ErrorCode::TRANSPORT_INVALID_ADDRESS);
                errors++;
            } else if (req->cmd == CMD_READ) {
                reads++;
                Packet* resp = PacketPool::get().acquire();
                resp->type = PKT_RESP;
                resp->set_transaction_id(req->get_transaction_id());
                auto& tracker = TransactionTracker::instance();
                tracker.complete_transaction(req->get_transaction_id());
            }
            PacketPool::get().release(req);
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override {
        request_queue.push(pkt);
        return true;
    }
    
    std::string get_module_type() const override { return "MemoryV2"; }
    
    void do_reset(const ResetConfig&) override {
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            PacketPool::get().release(pkt);
        }
        reads = 0; writes = 0; errors = 0;
    }

private:
    std::queue<Packet*> request_queue;
    uint64_t memory_size = 0x10000000;
    uint64_t reads, writes, errors;
};

class CacheV2 : public SimObject {
public:
    CacheV2(const std::string& name, EventQueue* eq, size_t cap = 1024)
        : SimObject(name, eq), capacity(cap), hits(0), misses(0), child_transactions(0) {}
    
    void tick() override {
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            uint64_t addr = pkt->payload ? pkt->payload->get_address() : 0;
            uint64_t tag = addr >> 12;
            auto it = cache.find(tag);
            if (it != cache.end()) {
                hits++;
                pkt->add_trace(name, getCurrentCycle(), 1, "hit");
                Packet* resp = PacketPool::get().acquire();
                resp->type = PKT_RESP;
                resp->set_transaction_id(pkt->get_transaction_id());
                PacketPool::get().release(pkt);
            } else {
                misses++;
                pkt->add_trace(name, getCurrentCycle(), 10, "miss");
                Packet* child_req = PacketPool::get().acquire();
                child_req->cmd = CMD_READ;
                child_req->type = PKT_REQ;
                uint64_t child_tid = createSubTransaction(pkt, child_req);
                auto& tracker = TransactionTracker::instance();
                tracker.link_transactions(pkt->get_transaction_id(), child_tid);
                child_transactions++;
            }
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override {
        request_queue.push(pkt);
        return true;
    }
    
    std::string get_module_type() const override { return "CacheV2"; }
    
    uint64_t createSubTransaction(Packet* parent, Packet* child) {
        uint64_t parent_tid = parent->get_transaction_id();
        uint64_t child_tid = next_child_id_++;
        if (child->payload) {
            auto* ext = create_transaction_context(child->payload, child_tid, parent_tid, 0, 1);
            ext->source_module = name;
            ext->type = "READ";
        }
        return child_tid;
    }
    
    void do_reset(const ResetConfig&) override {
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            PacketPool::get().release(pkt);
        }
        cache.clear();
        hits = 0; misses = 0; child_transactions = 0;
        next_child_id_ = 1000;
    }

private:
    std::queue<Packet*> request_queue;
    std::map<uint64_t, uint64_t> cache;
    size_t capacity;
    uint64_t hits, misses, child_transactions, next_child_id_;
};

#endif
