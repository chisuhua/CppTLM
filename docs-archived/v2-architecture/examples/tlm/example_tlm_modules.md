# TLM 模块示例

> **目录**: `include/tlm/`  
> **原则**: 纯 ch_stream 接口，无 Port/Adapter 复杂度

---

## cache_tlm.hh

```cpp
// include/tlm/cache_tlm.hh
#ifndef CACHE_TLM_HH
#define CACHE_TLM_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "core/sim_object.hh"
#include "bundles/cache_bundles.hh"

// TLM Cache 模块（纯 ch_stream，无 Port）
class CacheTLM : public SimObject {
private:
    // 统一 ch_stream 接口（无 Port）
    ch_stream<CacheReqBundle> req_in;
    ch_stream<CacheRespBundle> resp_out;
    
    // Cache 状态
    std::map<uint64_t, uint64_t> cache_lines;
    size_t cache_size_;
    
public:
    CacheTLM(const std::string& name, size_t cache_size = 1024)
        : SimObject(name), cache_size_(cache_size) {
        req_in.init("req_in");
        resp_out.init("resp_out");
    }
    
    // 获取 ch_stream（用于框架绑定）
    ch_stream<CacheReqBundle>& get_req_in() { return req_in; }
    ch_stream<CacheRespBundle>& get_resp_out() { return resp_out; }
    
    void tick() override {
        // 接收请求（ch_stream 语义）
        if (req_in.valid && req_in.ready) {
            auto& req = req_in.payload;
            
            // 查找 Cache
            uint64_t addr = static_cast<uint64_t>(req.address);
            bool hit = cache_lines.count(addr);
            
            // 构建响应
            resp_out.payload.transaction_id = req.transaction_id;
            resp_out.payload.data = hit ? cache_lines[addr] : 0;
            resp_out.payload.is_hit = hit;
            resp_out.payload.latency_cycles = hit ? 1 : 100;
            resp_out.valid = true;
            
            // 未命中时填充 Cache
            if (!hit && cache_lines.size() < cache_size_) {
                cache_lines[addr] = 0xDEADBEEF;  // 简化：假数据
            }
        } else {
            resp_out.valid = false;
        }
        
        // 背压处理：TLM 总是 ready
        req_in.ready = true;
    }
};

#endif // CACHE_TLM_HH
```

---

## crossbar_tlm.hh

```cpp
// include/tlm/crossbar_tlm.hh
#ifndef CROSSBAR_TLM_HH
#define CROSSBAR_TLM_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "core/sim_object.hh"
#include "bundles/noc_bundles.hh"
#include <array>

// Crossbar TLM 模块（4 输入 × 4 输出）
class CrossbarTLM : public SimObject {
private:
    static constexpr int NUM_PORTS = 4;
    
    // 多端口声明（数组方式）
    std::array<ch_stream<NoCReqBundle>, NUM_PORTS> req_in;
    std::array<ch_stream<NoCRespBundle>, NUM_PORTS> resp_out;
    
    // 路由表
    std::map<uint64_t, int> routing_table_;
    
public:
    CrossbarTLM(const std::string& name) : SimObject(name) {
        for (int i = 0; i < NUM_PORTS; i++) {
            req_in[i].init("req_in_" + std::to_string(i));
            resp_out[i].init("resp_out_" + std::to_string(i));
        }
        
        // 初始化路由表
        for (int i = 0; i < NUM_PORTS; i++) {
            routing_table_[i] = i;
        }
    }
    
    // 获取端口（用于框架绑定）
    ch_stream<NoCReqBundle>& get_req_in(int idx) { return req_in[idx]; }
    ch_stream<NoCRespBundle>& get_resp_out(int idx) { return resp_out[idx]; }
    
    void tick() override {
        // 轮询所有输入端口
        for (int i = 0; i < NUM_PORTS; i++) {
            if (req_in[i].valid && req_in[i].ready) {
                auto& req = req_in[i].payload;
                
                // 路由决策
                int dst_port = route_by_dst(req.dst_id);
                
                // 背压处理
                if (resp_out[dst_port].ready) {
                    resp_out[dst_port].payload = req;
                    resp_out[dst_port].valid = true;
                }
                // 否则：背压，保持 valid 等待下一周期
            }
        }
        
        // 设置背压：总是 ready
        for (int i = 0; i < NUM_PORTS; i++) {
            req_in[i].ready = true;
        }
    }
    
private:
    int route_by_dst(uint64_t dst_id) {
        return (dst_id >> 24) & 0x3;  // 高 8 位表示目标端口
    }
};

#endif // CROSSBAR_TLM_HH
```

---

## nic_tlm.hh

```cpp
// include/tlm/nic_tlm.hh
#ifndef NIC_TLM_HH
#define NIC_TLM_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "core/sim_object.hh"
#include "bundles/noc_bundles.hh"
#include "bundles/fragment_bundles.hh"
#include "mapper/fragment_mapper.hh"

// NIC TLM 模块（NoC 接口控制器）
class NICTLM : public SimObject {
private:
    // CPU 侧：简单请求/响应
    ch_stream<CacheReqBundle> cpu_req_in;
    ch_stream<CacheRespBundle> cpu_resp_out;
    
    // NoC 侧：NoC 数据包（可能分片）
    ch_stream<NoCReqBundle> noc_req_out;
    ch_stream<NoCRespBundle> noc_resp_in;
    
    // Fragment 缓冲
    std::vector<FragmentBundle<uint64_t>> pending_fragments;
    size_t fragment_beat_size_ = 64;
    
    uint64_t my_node_id_;
    
public:
    NICTLM(const std::string& name, uint64_t node_id)
        : SimObject(name), my_node_id_(node_id) {
        cpu_req_in.init("cpu_req_in");
        cpu_resp_out.init("cpu_resp_out");
        noc_req_out.init("noc_req_out");
        noc_resp_in.init("noc_resp_in");
    }
    
    // 获取端口
    ch_stream<CacheReqBundle>& get_cpu_req_in() { return cpu_req_in; }
    ch_stream<CacheRespBundle>& get_cpu_resp_out() { return cpu_resp_out; }
    ch_stream<NoCReqBundle>& get_noc_req_out() { return noc_req_out; }
    ch_stream<NoCRespBundle>& get_noc_resp_in() { return noc_resp_in; }
    
    void tick() override {
        // ========== CPU 侧 → NoC 侧 ==========
        if (cpu_req_in.valid && cpu_req_in.ready) {
            auto& cpu_req = cpu_req_in.payload;
            
            // 打包为 NoC 请求
            noc_req_out.payload.transaction_id = cpu_req.transaction_id;
            noc_req_out.payload.src_id = my_node_id_;
            noc_req_out.payload.dst_id = route_by_addr(cpu_req.address);
            noc_req_out.payload.address = cpu_req.address;
            noc_req_out.payload.payload = cpu_req.data;
            noc_req_out.payload.packet_type = cpu_req.is_write ? 1 : 0;
            noc_req_out.valid = true;
        } else {
            noc_req_out.valid = false;
        }
        
        // ========== NoC 侧 → CPU 侧 ==========
        if (noc_resp_in.valid && noc_resp_in.ready) {
            auto& noc_resp = noc_resp_in.payload;
            
            cpu_resp_out.payload.transaction_id = noc_resp.transaction_id;
            cpu_resp_out.payload.data = noc_resp.payload;
            cpu_resp_out.payload.is_hit = (noc_resp.status == 0);
            cpu_resp_out.payload.latency_cycles = 10;
            cpu_resp_out.valid = true;
        } else {
            cpu_resp_out.valid = false;
        }
        
        // 背压
        cpu_req_in.ready = true;
        noc_resp_in.ready = true;
    }
    
private:
    uint64_t route_by_addr(uint64_t addr) {
        return (addr >> 24) & 0xFF;  // 高 8 位表示目标节点
    }
};

#endif // NIC_TLM_HH
```

---

## memory_tlm.hh

```cpp
// include/tlm/memory_tlm.hh
#ifndef MEMORY_TLM_HH
#define MEMORY_TLM_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "core/sim_object.hh"
#include "bundles/cache_bundles.hh"

// Memory TLM 模块（简单内存模型）
class MemoryTLM : public SimObject {
private:
    ch_stream<CacheReqBundle> req_in;
    ch_stream<CacheRespBundle> resp_out;
    
    std::map<uint64_t, uint64_t> memory_;
    
public:
    MemoryTLM(const std::string& name) : SimObject(name) {
        req_in.init("req_in");
        resp_out.init("resp_out");
    }
    
    ch_stream<CacheReqBundle>& get_req_in() { return req_in; }
    ch_stream<CacheRespBundle>& get_resp_out() { return resp_out; }
    
    void tick() override {
        if (req_in.valid && req_in.ready) {
            auto& req = req_in.payload;
            
            // 写操作
            if (req.is_write) {
                memory_[static_cast<uint64_t>(req.address)] = req.data;
            }
            
            // 读操作
            uint64_t data = memory_[static_cast<uint64_t>(req.address)];
            
            // 构建响应
            resp_out.payload.transaction_id = req.transaction_id;
            resp_out.payload.data = data;
            resp_out.payload.is_hit = true;  // Memory 总是命中
            resp_out.payload.latency_cycles = 100;
            resp_out.valid = true;
        } else {
            resp_out.valid = false;
        }
        
        req_in.ready = true;
    }
};

#endif // MEMORY_TLM_HH
```

---

## 示例说明

- **Ch_stream 接口**：所有模块只定义 `ch_stream<T>`，无 Port
- **多端口支持**：Crossbar 使用 `std::array` 声明多端口
- **背压处理**：TLM 模块 `ready = true`，RTL 模块根据状态设置
- **端口获取**：通过 `get_*()` 方法暴露 ch_stream 给框架绑定
