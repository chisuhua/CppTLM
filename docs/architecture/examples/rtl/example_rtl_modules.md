# RTL 模块示例

> **目录**: `include/rtl/`  
> **原则**: 纯 CppHDL Component，可综合为 Verilog

---

## cache_component.hh

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
        // 周期精确的 Cache 行为（纯 CppHDL）
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
            case 1: // LOOKUP (1 周期)
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

## crossbar_component.hh

```cpp
// include/rtl/crossbar_component.hh
#ifndef CROSSBAR_COMPONENT_HH
#define CROSSBAR_COMPONENT_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "bundles/noc_bundles.hh"

// RTL Crossbar 模块（4 输入 × 4 输出，周期精确）
class CrossbarComponent : public ch::Component {
private:
    static constexpr int NUM_PORTS = 4;
    
    // 路由表（可配置）
    std::array<uint64_t, NUM_PORTS> routing_table_;
    std::array<ch_uint<4>, NUM_PORTS> state_;  // 0=IDLE, 1=ACTIVE, 2=RESPOND
    
public:
    __io(
        std::array<ch_stream<NoCReqBundle>, NUM_PORTS> req_in;
        std::array<ch_stream<NoCRespBundle>, NUM_PORTS> resp_out;
    );
    
    CrossbarComponent(ch::Component* parent = nullptr, const std::string& name = "crossbar")
        : Component(parent, name) {
        for (int i = 0; i < NUM_PORTS; i++) {
            state_[i] = 0_d;
        }
    }
    
    void create_ports() override {
        new (this->io_storage_) io_type;
    }
    
    void describe() override {
        for (int i = 0; i < NUM_PORTS; i++) {
            switch (static_cast<uint64_t>(state_[i])) {
                case 0: // IDLE
                    if (io().req_in[i].valid && io().req_in[i].ready) {
                        io().req_in[i].ready = 0;
                        state_[i] = 1;
                    } else {
                        io().req_in[i].ready = 1;
                    }
                    break;
                
                case 1: // ROUTING (1 周期延迟)
                    state_[i] = 2;
                    break;
                
                case 2: // RESPOND
                    int dst = route_by_dst(io().req_in[i].payload.dst_id);
                    
                    io().resp_out[dst].payload = io().req_in[i].payload;
                    io().resp_out[dst].valid = 1;
                    
                    if (io().resp_out[dst].ready) {
                        state_[i] = 0;
                    }
                    break;
            }
        }
    }
    
private:
    int route_by_dst(uint64_t dst_id) {
        return (dst_id >> 24) & 0x3;  // 高 8 位表示目标端口
    }
};

#endif // CROSSBAR_COMPONENT_HH
```

---

## nic_component.hh

```cpp
// include/rtl/nic_component.hh
#ifndef NIC_COMPONENT_HH
#define NIC_COMPONENT_HH

#include "ch.hpp"
#include "chlib/stream.h"
#include "bundles/noc_bundles.hh"
#include "bundles/cache_bundles.hh"

// RTL NIC 模块（NoC 接口控制器，周期精确）
class NICComponent : public ch::Component {
private:
    static constexpr int NUM_PORTS = 4;
    
    ch_uint<32> my_node_id_;
    
    __io(
        ch_stream<CacheReqBundle> cpu_req_in;
        ch_stream<CacheRespBundle> cpu_resp_out;
        ch_stream<NoCReqBundle> noc_req_out;
        ch_stream<NoCRespBundle> noc_resp_in;
    );
    
public:
    NICComponent(
        ch::Component* parent = nullptr,
        const std::string& name = "nic",
        uint64_t node_id = 0)
        : Component(parent, name) {
        my_node_id_ = static_cast<uint32_t>(node_id);
    }
    
    void create_ports() override {
        new (this->io_storage_) io_type;
    }
    
    void describe() override {
        static ch_uint<2> cpu_state(0_d);
        static ch_uint<2> noc_state(0_d);
        static ch_uint<64> saved_tid(0_d);
        
        // ========== CPU 侧状态机 ==========
        switch (static_cast<uint64_t>(cpu_state)) {
            case 0: // IDLE
                if (io().cpu_req_in.valid && io().cpu_req_in.ready) {
                    saved_tid = io().cpu_req_in.payload.transaction_id;
                    cpu_state = 1;
                }
                io().cpu_req_in.ready = 1;
                break;
            
            case 1: // 包装为 NoC 请求
                // preparation logic
                cpu_state = 2;
                break;
        }
        
        // ========== NoC 侧状态机 ==========
        switch (static_cast<uint64_t>(noc_state)) {
            case 0: // IDLE
                if (io().noc_req_out.ready) {
                    noc_state = 1;
                }
                break;
            
            case 1: // 发送响应
                if (io().noc_resp_in.valid && io().noc_resp_in.ready) {
                    io().cpu_resp_out.payload.transaction_id = saved_tid;
                    io().cpu_resp_out.payload.data = io().noc_resp_in.payload.payload;
                    io().cpu_resp_out.payload.is_hit = 1;
                    io().cpu_resp_out.valid = 1;
                    
                    if (io().cpu_resp_out.ready) {
                        noc_state = 0;
                    }
                }
                break;
        }
        
        // ========== 背压处理 ==========
        if (cpu_state == 0) {
            io().cpu_req_in.ready = 1;
        } else {
            io().cpu_req_in.ready = 0;
        }
        
        if (noc_state == 1) {
            io().noc_req_out.valid = 1;
            if (io().noc_req_out.ready) {
                io().noc_req_out.valid = 0;
            }
        } else {
            io().noc_req_out.valid = 0;
        }
        
        io().noc_resp_in.ready = 1;
    }
};

#endif // NIC_COMPONENT_HH
```

---

## wrapper 示例

```cpp
// include/rtl/wrapper/rtl_module_wrapper.hh
#ifndef RTL_MODULE_WRAPPER_HH
#define RTL_MODULE_WRAPPER_HH

#include "core/sim_object.hh"
#include "core/simple_port.hh"
#include "ch.hpp"
#include "chlib/stream.h"

// RTL 模块包装器（CppHDL Component + SimplePort 适配）
template<typename ComponentT, typename ReqBundle, typename RespBundle>
class RTLModuleWrapper : public SimObject {
private:
    std::unique_ptr<ch::ch_device<ComponentT>> device_;
    
    // 外部 SimplePort（用于 PortPair 连接）
    SimplePort* req_port;
    SimplePort* resp_port;
    
    // 内部 ch_stream 缓冲
    std::optional<ReqBundle> pending_req;
    std::optional<RespBundle> pending_resp;
    
public:
    RTLModuleWrapper(const std::string& name) : SimObject(name) {
        device_ = std::make_unique<ch::ch_device<ComponentT>>();
        
        req_port = new SimplePort();
        resp_port = new SimplePort();
    }
    
    // 获取外部端口（用于 PortPair 连接）
    SimplePort* get_req_port() { return req_port; }
    SimplePort* get_resp_port() { return resp_port; }
    
    // tick 入口
    void tick() override {
        // ========== 外部 Port → 内部 ch_stream ==========
        Packet* req_pkt;
        if (req_port->recv(req_pkt) && !pending_req.has_value()) {
            pending_req = decode_req(req_pkt);
        }
        
        // ========== 驱动 CppHDL Component ==========
        auto& io = device_->instance().io();
        
        // 请求路径：缓冲 → ch_stream → Component
        if (pending_req.has_value() && io.req_in.ready) {
            io.req_in.payload = pending_req.value();
            io.req_in.valid = true;
            pending_req.reset();
        } else {
            io.req_in.valid = false;
        }
        
        // 响应路径：Component → ch_stream → 缓冲
        if (io.resp_out.valid && io.resp_out.ready) {
            pending_resp = io.resp_out.payload;
        }
        
        // 驱动 Component tick
        device_->tick();
        
        // ========== 内部 ch_stream → 外部 Port ==========
        if (pending_resp.has_value()) {
            Packet* resp_pkt = encode_resp(pending_resp.value());
            resp_port->send(resp_pkt);
            pending_resp.reset();
        }
    }
    
private:
    ReqBundle decode_req(Packet* pkt) {
        ReqBundle bundle;
        bundle.transaction_id = pkt->transaction_id;
        // ... 根据 Bundle 类型解码
        return bundle;
    }
    
    Packet* encode_resp(const RespBundle& bundle) {
        Packet* pkt = new Packet(...);
        pkt->transaction_id = bundle.transaction_id;
        // ... 根据 Bundle 类型编码
        return pkt;
    }
};

#endif // RTL_MODULE_WRAPPER_HH
```

---

## 示例说明

- **CppHDL Component**：使用 `ch::Component` 基类，定义 `describe()` 方法
- **__io 宏**：声明 ch_stream 接口
- **周期精确**：使用 `static ch_uint<N> state(0_d)` 实现状态机
- **可综合**：代码符合 CppHDL 语法，可综合为 Verilog
- **Wrapper 适配**：RTL 模块通过 Wrapper 包装，提供 SimplePort 接口
