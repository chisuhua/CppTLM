// include/modules/modules_v2.hh
// CppTLM v2.0 模块实现
// 演示：层次化复位、交易追踪、错误处理

#ifndef MODULES_V2_HH
#define MODULES_V2_HH

#include "core/sim_object.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "ext/transaction_context_ext.hh"
#include "ext/error_context_ext.hh"
#include "framework/transaction_tracker.hh"
#include "framework/debug_tracker.hh"
#include <map>
#include <queue>

/**
 * @brief CrossbarV2 - 透传型模块
 * 
 * 特点：
 * - 透传 transaction_id
 * - 记录 hop 延迟
 * - 支持层次化复位
 */
class CrossbarV2 : public SimObject {
public:
    CrossbarV2(const std::string& name, EventQueue* eq)
        : SimObject(name, eq), packets_received(0), packets_forwarded(0) {}
    
    void tick() override {
        // 处理输入队列中的包
        while (!input_queue.empty()) {
            Packet* pkt = input_queue.front();
            input_queue.pop();
            
            // 记录 hop
            pkt->add_trace(name, getCurrentCycle(), 1, "hopped");
            
            auto& tracker = TransactionTracker::instance();
            tracker.record_hop(pkt->get_transaction_id(), name, 1, "hopped");
            
            // 转发
            if (output_port) {
                output_port->send(pkt);
                packets_forwarded++;
            }
        }
    }
    
    void handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) override {
        input_queue.push(pkt);
        packets_received++;
    }
    
    std::string get_module_type() const override {
        return "CrossbarV2";
    }
    
    void do_reset(const ResetConfig& config) override {
        // 清空队列
        while (!input_queue.empty()) {
            Packet* pkt = input_queue.front();
            input_queue.pop();
            PacketPool::get().release(pkt);
        }
        packets_received = 0;
        packets_forwarded = 0;
        
        SimObject::do_reset(config);
    }
    
private:
    std::queue<Packet*> input_queue;
    void* output_port = nullptr;  // 简化：实际应为 SimplePort*
    uint64_t packets_received;
    uint64_t packets_forwarded;
};

/**
 * @brief MemoryV2 - 终止型模块
 * 
 * 特点：
 * - 终止交易
 * - 发送响应
 * - 错误检测与处理
 */
class MemoryV2 : public SimObject {
public:
    MemoryV2(const std::string& name, EventQueue* eq)
        : SimObject(name, eq), reads(0), writes(0), errors(0) {}
    
    void tick() override {
        while (!request_queue.empty()) {
            Packet* req = request_queue.front();
            request_queue.pop();
            
            // 检查地址有效性
            uint64_t addr = req->payload ? req->payload->get_address() : 0;
            if (addr >= memory_size) {
                // 记录错误
                req->set_error_code(ErrorCode::TRANSPORT_INVALID_ADDRESS);
                
                ErrorContextExt* ext = nullptr;
                req->payload->get_extension(ext);
                if (!ext) {
                    ext = create_error_context(
                        req->payload,
                        ErrorCode::TRANSPORT_INVALID_ADDRESS,
                        "Address out of range",
                        name
                    );
                }
                
                auto& tracker = DebugTracker::instance();
                tracker.record_error(req->payload, ErrorCode::TRANSPORT_INVALID_ADDRESS,
                                    "Address out of range", name);
                errors++;
                
                continue;
            }
            
            // 处理请求
            if (req->cmd == CMD_READ) {
                reads++;
                Packet* resp = PacketPool::get().acquire();
                resp->type = PKT_RESP;
                resp->set_transaction_id(req->get_transaction_id());
                
                // 完成交易
                auto& tracker = TransactionTracker::instance();
                tracker.complete_transaction(req->get_transaction_id());
                
                // 发送响应
                if (response_port) {
                    response_port->send(resp);
                }
            } else if (req->cmd == CMD_WRITE) {
                writes++;
                // 类似处理...
                
                auto& tracker = TransactionTracker::instance();
                tracker.complete_transaction(req->get_transaction_id());
            }
            
            PacketPool::get().release(req);
        }
    }
    
    void handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) override {
        request_queue.push(pkt);
    }
    
    std::string get_module_type() const override {
        return "MemoryV2";
    }
    
    void do_reset(const ResetConfig& config) override {
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            PacketPool::get().release(pkt);
        }
        reads = 0;
        writes = 0;
        errors = 0;
        
        SimObject::do_reset(config);
    }
    
private:
    std::queue<Packet*> request_queue;
    void* response_port = nullptr;  // 简化
    uint64_t memory_size = 0x10000000;  // 256MB
    uint64_t reads;
    uint64_t writes;
    uint64_t errors;
};

/**
 * @brief CacheV2 - 转换型模块
 * 
 * 特点：
 * - Cache miss 时创建子交易
 * - 关联父子交易
 * - 支持层次化复位
 */
class CacheV2 : public SimObject {
public:
    CacheV2(const std::string& name, EventQueue* eq, size_t capacity = 1024)
        : SimObject(name, eq), capacity(capacity), hits(0), misses(0), child_transactions(0) {}
    
    void tick() override {
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            
            uint64_t addr = pkt->payload ? pkt->payload->get_address() : 0;
            uint64_t tag = addr >> 12;  // 简化：4KB page
            
            auto it = cache.find(tag);
            if (it != cache.end()) {
                // Cache hit
                hits++;
                pkt->add_trace(name, getCurrentCycle(), 1, "hit");
                
                Packet* resp = PacketPool::get().acquire();
                resp->type = PKT_RESP;
                resp->set_transaction_id(pkt->get_transaction_id());
                
                if (response_port) {
                    response_port->send(resp);
                }
                
                PacketPool::get().release(pkt);
            } else {
                // Cache miss
                misses++;
                pkt->add_trace(name, getCurrentCycle(), 10, "miss");
                
                // 创建子交易
                Packet* child_req = PacketPool::get().acquire();
                child_req->cmd = CMD_READ;
                child_req->type = PKT_REQ;
                child_req->payload = pkt->payload;  // 共享 payload
                
                uint64_t child_tid = createSubTransaction(pkt, child_req);
                
                // 链接父子交易
                auto& tracker = TransactionTracker::instance();
                tracker.link_transactions(pkt->get_transaction_id(), child_tid);
                child_transactions++;
                
                // 发送到下一级
                if (downstream_port) {
                    downstream_port->send(child_req);
                }
            }
        }
    }
    
    void handleUpstreamRequest(Packet* pkt, int src_id, const std::string& src_label) override {
        request_queue.push(pkt);
    }
    
    std::string get_module_type() const override {
        return "CacheV2";
    }
    
    uint64_t createSubTransaction(Packet* parent, Packet* child) {
        // 分配新交易 ID
        uint64_t parent_tid = parent->get_transaction_id();
        uint64_t child_tid = next_child_id_++;
        
        // 设置子交易 Extension
        if (child->payload) {
            auto* ext = create_transaction_context(child->payload, child_tid, parent_tid, 0, 1);
            ext->source_module = name;
            ext->type = "READ";
        }
        
        return child_tid;
    }
    
    void do_reset(const ResetConfig& config) override {
        while (!request_queue.empty()) {
            Packet* pkt = request_queue.front();
            request_queue.pop();
            PacketPool::get().release(pkt);
        }
        cache.clear();
        hits = 0;
        misses = 0;
        child_transactions = 0;
        next_child_id_ = 1000;
        
        SimObject::do_reset(config);
    }
    
private:
    std::queue<Packet*> request_queue;
    void* response_port = nullptr;
    void* downstream_port = nullptr;
    std::map<uint64_t, uint64_t> cache;  // tag -> data (简化)
    size_t capacity;
    uint64_t hits;
    uint64_t misses;
    uint64_t child_transactions;
    uint64_t next_child_id_ = 1000;
};

#endif // MODULES_V2_HH
