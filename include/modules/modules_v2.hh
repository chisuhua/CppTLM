// include/modules/modules_v2.hh
// CppTLM v2.0 模块实现
#ifndef MODULES_V2_HH
#define MODULES_V2_HH

#include <queue>
#include <map>
#include <atomic>

#include "core/sim_object.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "ext/transaction_context_ext.hh"
#include "framework/transaction_tracker.hh"
#include "core/tlm_module.hh" // P3.2 Wave 3: Use new TLMModule base class

// =========================================================================
// CrossbarV2
// =========================================================================
class CrossbarV2 : public TLMModule {
public:
    CrossbarV2(const std::string& name, EventQueue* eq)
        : TLMModule(name, eq), packets_received(0), packets_forwarded(0) {}
    
    void tick() override {
        while (!input_queue.empty()) {
            Packet* pkt = input_queue.front();
            input_queue.pop();
            
            // P3.2 Wave 3: Use lifecycle hook
            TransactionInfo info = onTransactionHop(pkt);
            if (info.action == TransactionAction::PASSTHROUGH) {
                pkt->add_trace(name, getCurrentCycle(), 1, "hopped");
                packets_forwarded++;
            }
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override {
        input_queue.push(pkt);
        packets_received++;
        return true;
    }
    
    std::string get_module_type() const override { return "CrossbarV2"; }
    
    void do_reset(const ResetConfig& config) override {
        while (!input_queue.empty()) {
            Packet* pkt = input_queue.front();
            input_queue.pop();
            PacketPool::get().release(pkt);
        }
        packets_received = 0;
        packets_forwarded = 0;
        TLMModule::do_reset(config); // Important: Call base reset for fragment buffers
    }

private:
    std::queue<Packet*> input_queue;
    uint64_t packets_received;
    uint64_t packets_forwarded;
};

// =========================================================================
// MemoryV2
// =========================================================================
class MemoryV2 : public TLMModule {
public:
    MemoryV2(const std::string& name, EventQueue* eq)
        : TLMModule(name, eq), reads(0), writes(0), errors(0) {}
    
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
                // P3.2 Wave 3: Mark as Terminating
                onTransactionEnd(req);
                
                Packet* resp = PacketPool::get().acquire();
                resp->type = PKT_RESP;
                resp->set_transaction_id(req->get_transaction_id());
                auto& tracker = TransactionTracker::instance();
                tracker.complete_transaction(req->get_transaction_id());
                PacketPool::get().release(resp);
            }
            PacketPool::get().release(req);
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override {
        request_queue.push(pkt);
        return true;
    }
    
    std::string get_module_type() const override { return "MemoryV2"; }
    
    void do_reset(const ResetConfig& config) override {
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            PacketPool::get().release(pkt);
        }
        reads = 0; writes = 0; errors = 0;
        TLMModule::do_reset(config);
    }

private:
    std::queue<Packet*> request_queue;
    uint64_t memory_size = 0x10000000;
    uint64_t reads, writes, errors;
};

// =========================================================================
// CacheV2
// =========================================================================
class CacheV2 : public TLMModule {
public:
    CacheV2(const std::string& name, EventQueue* eq, size_t cap = 1024)
        : TLMModule(name, eq), capacity(cap), hits(0), misses(0), child_transactions(0) {}
    
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
                
                TransactionInfo info = onTransactionHop(pkt); // Passthrough
                (void)info;

                Packet* resp = PacketPool::get().acquire();
                resp->type = PKT_RESP;
                resp->set_transaction_id(pkt->get_transaction_id());
                PacketPool::get().release(pkt);
            } else {
                misses++;
                pkt->add_trace(name, getCurrentCycle(), 10, "miss");
                
                // P3.2 Wave 3: Transform Action (Create Child)
                uint64_t child_tid = createSubTransaction(pkt, nullptr); // Use base class implementation
                child_transactions++;
                // Note: In a real TLM module, we would forward the child packet downstream here.
                // Since this is a stub, we just track the creation.
            }
        }
    }
    
    bool handleUpstreamRequest(Packet* pkt, int, const std::string&) override {
        request_queue.push(pkt);
        // P3.2 Wave 3: Start Transaction
        onTransactionStart(pkt); 
        return true;
    }
    
    std::string get_module_type() const override { return "CacheV2"; }
    
    // createSubTransaction is inherited from TLMModule, implementing the fix for ID collision and lifecycle.
    // We can remove the local override if the base class one is sufficient (which it is now).
    
    void do_reset(const ResetConfig& config) override {
        // Fix 3 (Reset Order): Clear cache BEFORE releasing packets
        cache.clear();
        
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            PacketPool::get().release(pkt);
        }
        hits = 0; misses = 0; child_transactions = 0;
        // Call base to clear fragment_buffers_
        TLMModule::do_reset(config);
    }

    // TLMModule provides fragment_buffers_, but CacheV2 has its own working queue
    std::queue<Packet*> request_queue; 
    std::map<uint64_t, uint64_t> cache;
    size_t capacity;
    uint64_t hits = 0;
    uint64_t misses = 0;
    uint64_t child_transactions = 0;
};

#endif
