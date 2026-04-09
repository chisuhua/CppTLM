# ADR-X.8: 细粒度分片交易处理与 TLM/RTL 职责分离

> **版本**: 1.0  
> **日期**: 2026-04-09  
> **状态**: 📋 待确认  
> **影响**: v2.0 - 分片交易架构设计

---

## 1. 核心问题

老板提出的问题：

1. **细粒度分片场景**：每个分片独立进入下游模块，下游如何识别它们属于同一个交易？

2. **TLM vs RTL 职责分离**：
   - TLM 模块：是否应该有交易处理和追踪逻辑？
   - RTL 模块：是否保持简单透传？

---

## 2. 核心设计原则

```
TLM 模块：智能处理（可识别分片、重组、追踪）
RTL 模块：简单透传（周期精确，不感知交易）
框架层：统一追踪（通过 Extension 关联分片）
```

---

## 3. 分片标识设计

### 3.1 TransactionContextExt 扩展

```cpp
// include/ext/transaction_context_ext.hh
struct TransactionContextExt : public tlm::tlm_extension<TransactionContextExt> {
    // ========== 核心标识字段 ==========
    uint64_t transaction_id;      // 当前交易 ID（分片使用 sub_id）
    uint64_t parent_id;           // 父交易 ID（0 表示根交易）
    uint8_t  fragment_id;         // 分片序号（0~N-1）
    uint8_t  fragment_total;      // 总分片数（1 表示不分片）
    
    // ========== 辅助方法 ==========
    bool is_root() const { return parent_id == 0 && fragment_total == 1; }
    bool is_fragmented() const { return fragment_total > 1; }
    bool is_last_fragment() const { return fragment_id == fragment_total - 1; }
    bool is_first_fragment() const { return fragment_id == 0; }
    
    // ========== 分组键（用于重组） ==========
    uint64_t get_group_key() const {
        // 父交易 ID 作为分组键（所有分片共享同一个 parent_id）
        return parent_id != 0 ? parent_id : transaction_id;
    }
};
```

### 3.2 分片标识示例

```
根交易（粗粒度）:
  transaction_id = 100
  parent_id = 0
  fragment_id = 0
  fragment_total = 1

分片交易（细粒度）:
  分片 0: transaction_id = 201, parent_id = 200, fragment_id = 0, fragment_total = 4
  分片 1: transaction_id = 202, parent_id = 200, fragment_id = 1, fragment_total = 4
  分片 2: transaction_id = 203, parent_id = 200, fragment_id = 2, fragment_total = 4
  分片 3: transaction_id = 204, parent_id = 200, fragment_id = 3, fragment_total = 4
  
  ✅ 所有分片通过 parent_id = 200 关联
  ✅ 通过 fragment_id 区分序号
  ✅ 通过 fragment_total 知道何时完成
```

---

## 4. TLM 模块设计（智能处理）

### 4.1 TLM 模块基类扩展

```cpp
// include/core/tlm_module.hh
#ifndef TLM_MODULE_HH
#define TLM_MODULE_HH

#include "sim_object.hh"
#include "../ext/transaction_context_ext.hh"
#include <map>
#include <vector>

// 分片重组缓冲
struct FragmentBuffer {
    uint64_t parent_id;
    uint8_t fragment_total;
    std::map<uint8_t, Packet*> fragments;  // fragment_id -> Packet
    uint64_t first_arrival_time;
    
    bool is_complete() const {
        return fragments.size() == fragment_total;
    }
    
    bool has_fragment(uint8_t id) const {
        return fragments.count(id) > 0;
    }
};

// TLM 模块基类（支持分片处理）
class TLMModule : public SimObject {
protected:
    // 分片重组缓冲（按 parent_id 分组）
    std::map<uint64_t, FragmentBuffer> fragment_buffers_;
    
    // 配置
    bool enable_fragment_reassembly_ = false;  // 是否启用分片重组
    
public:
    TLMModule(const std::string& n, EventQueue* eq) : SimObject(n, eq) {}
    
    // ========== 分片处理接口（可选重写） ==========
    
    // 接收分片（框架层调用）
    virtual void onFragmentReceived(Packet* pkt) {
        if (!enable_fragment_reassembly_) {
            // 不启用重组：直接处理
            handlePacket(pkt);
            return;
        }
        
        // 获取分片信息
        TransactionContextExt* ext = get_transaction_context(pkt->payload);
        if (!ext || !ext->is_fragmented()) {
            // 非分片交易：直接处理
            handlePacket(pkt);
            return;
        }
        
        // 分片交易：缓冲重组
        uint64_t group_key = ext->get_group_key();
        
        // 初始化缓冲
        if (fragment_buffers_.find(group_key) == fragment_buffers_.end()) {
            fragment_buffers_[group_key] = {
                .parent_id = group_key,
                .fragment_total = ext->fragment_total,
                .fragments = {},
                .first_arrival_time = event_queue->getCurrentCycle()
            };
        }
        
        // 添加分片
        auto& buffer = fragment_buffers_[group_key];
        if (!buffer.has_fragment(ext->fragment_id)) {
            buffer.fragments[ext->fragment_id] = pkt;
        }
        
        // 检查是否完成
        if (buffer.is_complete()) {
            // 所有分片到达：重组后处理
            onFragmentGroupComplete(buffer);
            fragment_buffers_.erase(group_key);
        }
    }
    
    // 分片组完成（可选重写）
    virtual void onFragmentGroupComplete(FragmentBuffer& buffer) {
        // 默认：按顺序处理每个分片
        for (auto& [id, pkt] : buffer.fragments) {
            handlePacket(pkt);
        }
    }
    
    // 处理单个包（子类实现）
    virtual void handlePacket(Packet* pkt) = 0;
    
    // ========== 便捷方法 ==========
    
    // 获取父交易的所有分片
    std::vector<Packet*> get_fragments(uint64_t parent_id) {
        std::vector<Packet*> fragments;
        if (fragment_buffers_.count(parent_id)) {
            auto& buffer = fragment_buffers_[parent_id];
            for (uint8_t i = 0; i < buffer.fragment_total; i++) {
                if (buffer.fragments.count(i)) {
                    fragments.push_back(buffer.fragments[i]);
                }
            }
        }
        return fragments;
    }
    
    // 清除缓冲（异常处理）
    void clear_fragment_buffer(uint64_t parent_id) {
        if (fragment_buffers_.count(parent_id)) {
            for (auto& [id, pkt] : fragment_buffers_[parent_id].fragments) {
                PacketPool::get().release(pkt);
            }
            fragment_buffers_.erase(parent_id);
        }
    }
};

#endif // TLM_MODULE_HH
```

---

### 4.2 TLM 模块示例：Memory（支持分片重组）

```cpp
// include/modules/memory_tlm_v2.hh
#ifndef MEMORY_TLM_V2_HH
#define MEMORY_TLM_V2_HH

#include "../core/tlm_module.hh"
#include <map>

class MemoryTLMV2 : public TLMModule {
private:
    std::map<uint64_t, uint64_t> memory_;
    
public:
    MemoryTLMV2(const std::string& n, EventQueue* eq)
        : TLMModule(n, eq) {
        enable_fragment_reassembly_ = true;  // ✅ 启用分片重组
    }
    
    // ✅ 重写 onFragmentGroupComplete：等待所有分片到达后处理
    void onFragmentGroupComplete(FragmentBuffer& buffer) override {
        uint64_t parent_id = buffer.parent_id;
        
        // 重组所有分片
        uint64_t base_addr = 0;
        for (auto& [id, pkt] : buffer.fragments) {
            TransactionContextExt* ext = get_transaction_context(pkt->payload);
            
            // 执行内存操作
            uint64_t addr = pkt->payload->get_address() + (id * 8);  // 假设每分片 8 字节
            memory_[addr] = 0xDEADBEEF;  // 简化
            
            // 创建响应
            Packet* resp = PacketPool::get().acquire();
            resp->payload = pkt->payload;
            resp->type = PKT_RESP;
            resp->original_req = pkt;
            
            // 响应包使用父交易 ID（粗粒度）
            resp->set_transaction_id(parent_id);
            
            // 设置分片信息
            if (TransactionContextExt* resp_ext = get_transaction_context(resp->payload)) {
                resp_ext->parent_id = parent_id;
                resp_ext->fragment_id = ext->fragment_id;
                resp_ext->fragment_total = ext->fragment_total;
            }
            
            // 发送响应
            event_queue->schedule([this, resp]() {
                getPortManager().getUpstreamPorts()[0]->sendResp(resp);
            }, 100);  // Memory 延迟
            
            PacketPool::get().release(pkt);
        }
        
        // 框架层记录交易完成
        TransactionTracker::instance().complete_transaction(parent_id);
    }
    
    // 处理单个包（非分片场景）
    void handlePacket(Packet* pkt) override {
        uint64_t addr = pkt->payload->get_address();
        bool is_write = pkt->cmd == CMD_WRITE;
        
        if (is_write) {
            memory_[addr] = 0xDEADBEEF;
        }
        
        Packet* resp = PacketPool::get().acquire();
        resp->payload = pkt->payload;
        resp->type = PKT_RESP;
        resp->original_req = pkt;
        resp->set_transaction_id(pkt->get_transaction_id());
        
        event_queue->schedule([this, resp]() {
            getPortManager().getUpstreamPorts()[0]->sendResp(resp);
        }, 100);
        
        PacketPool::get().release(pkt);
    }
    
    void tick() override {
        // Memory 无缓冲，无需 tick
    }
};

#endif // MEMORY_TLM_V2_HH
```

---

### 4.3 TLM 模块示例：Crossbar（透传 + 分片感知）

```cpp
// include/modules/crossbar_tlm_v2.hh
#ifndef CROSSBAR_TLM_V2_HH
#define CROSSBAR_TLM_V2_HH

#include "../core/tlm_module.hh"

class CrossbarTLMV2 : public TLMModule {
public:
    CrossbarTLMV2(const std::string& n, EventQueue* eq)
        : TLMModule(n, eq) {
        enable_fragment_reassembly_ = false;  // ❌ 不启用重组，直接透传
    }
    
    // ✅ 重写 onFragmentReceived：透传分片
    void onFragmentReceived(Packet* pkt) override {
        TransactionContextExt* ext = get_transaction_context(pkt->payload);
        
        // 获取分片信息（用于路由决策）
        uint64_t parent_id = ext ? ext->parent_id : 0;
        uint8_t fragment_id = ext ? ext->fragment_id : 0;
        
        // 路由决策（基于地址，与分片无关）
        int dst_port = route_by_dst(pkt->payload->get_address());
        
        // 框架层记录（细粒度：每个分片独立记录）
        if (ext && ext->is_fragmented()) {
            TransactionTracker::instance().record_hop(
                pkt->get_transaction_id(), name_, 1, "fragment_hopped");
            
            // 同时记录父交易（粗粒度追踪）
            if (ext->parent_id != 0) {
                TransactionTracker::instance().record_hop(
                    ext->parent_id, name_, 1, "parent_fragment_hopped");
            }
        } else {
            TransactionTracker::instance().record_hop(
                pkt->get_transaction_id(), name_, 1, "hopped");
        }
        
        // 转发到下游
        forward_to_port(dst_port, pkt);
    }
    
    void handlePacket(Packet* pkt) override {
        // 非分片场景
        int dst_port = route_by_dst(pkt->payload->get_address());
        forward_to_port(dst_port, pkt);
    }
    
    void tick() override {
        // 处理缓冲
    }
    
private:
    int route_by_dst(uint64_t addr) {
        return (addr >> 24) & 0x3;
    }
    
    void forward_to_port(int dst_port, Packet* pkt) {
        // ... 发送逻辑
    }
};

#endif // CROSSBAR_TLM_V2_HH
```

---

## 5. RTL 模块设计（简单透传）

### 5.1 RTL 模块基类

```cpp
// include/core/rtl_module.hh
#ifndef RTL_MODULE_HH
#define RTL_MODULE_HH

#include "sim_object.hh"
#include "ch.hpp"
#include "chlib/stream.h"

// RTL 模块基类（简单透传，不感知交易）
template<typename ComponentT, typename ReqBundle, typename RespBundle>
class RTLModule : public SimObject {
protected:
    std::unique_ptr<ch::ch_device<ComponentT>> device_;
    
    // 外部 SimplePort（用于 PortPair 连接）
    SimplePort* req_port;
    SimplePort* resp_port;
    
    // 内部 ch_stream 缓冲
    std::optional<ReqBundle> pending_req;
    std::optional<RespBundle> pending_resp;
    
public:
    RTLModule(const std::string& name) : SimObject(name) {
        device_ = std::make_unique<ch::ch_device<ComponentT>>();
        req_port = new SimplePort();
        resp_port = new SimplePort();
    }
    
    SimplePort* get_req_port() { return req_port; }
    SimplePort* get_resp_port() { return resp_port; }
    
    // ✅ RTL 模块不处理交易上下文，只透传 ch_stream 数据
    void tick() override {
        // ========== 外部 Port → 内部 ch_stream ==========
        Packet* req_pkt;
        if (req_port->recv(req_pkt) && !pending_req.has_value()) {
            pending_req = decode_req(req_pkt);
        }
        
        // ========== 驱动 CppHDL Component ==========
        auto& io = device_->instance().io();
        
        if (pending_req.has_value() && io.req_in.ready) {
            io.req_in.payload = pending_req.value();
            io.req_in.valid = true;
            pending_req.reset();
        } else {
            io.req_in.valid = false;
        }
        
        if (io.resp_out.valid && io.resp_out.ready) {
            pending_resp = io.resp_out.payload;
        }
        
        device_->tick();
        
        // ========== 内部 ch_stream → 外部 Port ==========
        if (pending_resp.has_value()) {
            Packet* resp_pkt = encode_resp(pending_resp.value());
            
            // ✅ 透传 transaction_id（从请求包复制）
            if (req_pkt) {
                resp_pkt->set_transaction_id(req_pkt->get_transaction_id());
                
                // ✅ 透传 Extension（如果存在）
                if (TransactionContextExt* req_ext = get_transaction_context(req_pkt->payload)) {
                    TransactionContextExt* resp_ext = new TransactionContextExt(*req_ext);
                    resp_pkt->payload->set_extension(resp_ext);
                }
            }
            
            resp_port->send(resp_pkt);
            pending_resp.reset();
        }
    }
    
private:
    ReqBundle decode_req(Packet* pkt) {
        ReqBundle bundle;
        // ... 解码逻辑
        return bundle;
    }
    
    Packet* encode_resp(const RespBundle& bundle) {
        Packet* pkt = new Packet(...);
        // ... 编码逻辑
        return pkt;
    }
};

#endif // RTL_MODULE_HH
```

---

### 5.2 RTL 模块示例：CacheComponent（纯 CppHDL）

```cpp
// include/rtl/cache_component.hh
#ifndef CACHE_COMPONENT_HH
#define CACHE_COMPONENT_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "bundles/cache_bundles.hh"

// RTL Cache 模块（纯 ch_stream，可综合为 Verilog）
class CacheComponent : public ch::Component {
public:
    __io(
        ch_stream<CacheReqBundle> req_in;
        ch_stream<CacheRespBundle> resp_out;
    );
    
    CacheComponent(ch::Component* parent = nullptr, const std::string& name = "cache")
        : Component(parent, name) {}
    
    void create_ports() override {
        new (this->io_storage_) io_type;
    }
    
    void describe() override {
        // 周期精确的 Cache 行为
        static ch_uint<2> state(0_d);
        static ch_uint<64> saved_tid(0_d);
        
        switch (static_cast<uint64_t>(state)) {
            case 0: // IDLE
                if (io().req_in.valid && io().req_in.ready) {
                    saved_tid = io().req_in.payload.transaction_id;
                    state = 1;
                }
                io().req_in.ready = 1;
                break;
            case 1: // LOOKUP
                state = 2;
                io().req_in.ready = 0;
                break;
            case 2: // RESPOND
                io().resp_out.payload.transaction_id = saved_tid;
                io().resp_out.payload.is_hit = 1;
                io().resp_out.payload.latency_cycles = 1;
                io().resp_out.valid = 1;
                
                if (io().resp_out.ready) {
                    state = 0;
                }
                io().req_in.ready = 0;
                break;
        }
    }
};

#endif // CACHE_COMPONENT_HH
```

---

## 6. 框架层设计（统一追踪）

### 6.1 粗/细粒度可配置

```cpp
// include/framework/transaction_tracker.hh
class TransactionTracker {
private:
    bool enable_coarse_grained_ = true;   // 粗粒度追踪（父交易）
    bool enable_fine_grained_ = true;     // 细粒度追踪（分片）
    
public:
    // 记录经过（自动判断粗/细粒度）
    void record_hop(uint64_t tid, const std::string& module, uint64_t latency, const std::string& event) {
        TransactionRecord* record = get_transaction(tid);
        if (!record) return;
        
        // 细粒度：记录分片
        if (enable_fine_grained_ && record->is_fragmented()) {
            record->fragment_hop_log[record->fragment_id].push_back({module, latency});
        }
        
        // 粗粒度：记录父交易
        if (enable_coarse_grained_) {
            uint64_t parent_id = record->parent_id != 0 ? record->parent_id : tid;
            TransactionRecord* parent_record = get_transaction(parent_id);
            if (parent_record) {
                parent_record->hop_log.push_back({module, latency});
            }
        }
    }
    
    // 配置方法
    void enable_coarse_grained(bool enable) { enable_coarse_grained_ = enable; }
    void enable_fine_grained(bool enable) { enable_fine_grained_ = enable; }
};
```

---

## 7. 分片流转示例

```
CPU (创建分片)
  ↓
  transaction_id = 201, parent_id = 200, fragment_id = 0, fragment_total = 4
  transaction_id = 202, parent_id = 200, fragment_id = 1, fragment_total = 4
  transaction_id = 203, parent_id = 200, fragment_id = 2, fragment_total = 4
  transaction_id = 204, parent_id = 200, fragment_id = 3, fragment_total = 4
  ↓
Crossbar (TLM，透传)
  - 识别分片（通过 parent_id）
  - 路由决策（基于地址）
  - 框架层记录（粗 + 细粒度）
  ↓
Cache (TLM/RTL)
  - TLM 模式：可重组分片，处理 Cache 逻辑
  - RTL 模式：简单透传 ch_stream
  ↓
Memory (TLM，重组)
  - 等待所有 4 个分片到达
  - 重组后执行内存操作
  - 发送响应（使用 parent_id = 200）
```

---

## 8. 需要确认的问题

| 问题 | 选项 | 推荐 |
|------|------|------|
| **Q1**: 分片标识方式？ | A) parent_id + fragment_id | ✅ **A** |
| **Q2**: TLM 模块职责？ | A) 智能处理 / B) 简单透传 | ✅ **A) 智能处理** |
| **Q3**: RTL 模块职责？ | A) 智能处理 / B) 简单透传 | ✅ **B) 简单透传** |
| **Q4**: 框架层追踪粒度？ | A) 粗 / B) 细 / C) 两者可配 | ✅ **C) 两者可配** |
| **Q5**: 分片重组位置？ | A) 所有模块 / B) 目的模块 | ✅ **B) 目的模块** |

---

**下一步**: 请老板确认上述设计细节
