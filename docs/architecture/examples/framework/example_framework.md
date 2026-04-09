# 框架示例

> **目录**: `include/framework/`  
> **原则**: 泛型适配，类型擦除，模块注册

---

## generic_stream_adapter.hh

```cpp
// include/framework/generic_stream_adapter.hh
#ifndef GENERIC_STREAM_ADAPTER_HH
#define GENERIC_STREAM_ADAPTER_HH

#include "core/simple_port.hh"
#include "chlib/stream.h"
#include <functional>
#include <memory>
#include <vector>
#include <array>

// ========== 泛型输入适配器（类型擦除） ==========
class GenericInputStreamAdapter {
private:
    SimplePort* port_;
    void* stream_ptr_;
    std::function<void(Packet*)> decode_fn_;
    std::function<void(bool)> set_valid_fn_;
    std::function<void()> set_ready_fn_;
    
public:
    template<typename BundleT>
    GenericInputStreamAdapter(SimplePort* port, ch_stream<BundleT>& stream)
        : port_(port), stream_ptr_(&stream) {
        
        decode_fn_ = [&stream](Packet* pkt) {
            stream.payload = decode_bundle<BundleT>(pkt);
            stream.valid = true;
        };
        
        set_valid_fn_ = [&stream](bool value) {
            stream.valid = value;
        };
        
        set_ready_fn_ = [&stream]() {
            stream.ready = true;
        };
    }
    
    void tick() {
        Packet* pkt;
        if (port_->recv(pkt)) {
            decode_fn_(pkt);
        }
        set_valid_fn_(pkt != nullptr);  // 若无 pkt，设置 valid = false
        set_ready_fn_();
    }
    
private:
    template<typename BundleT>
    BundleT decode_bundle(Packet* pkt) {
        BundleT bundle;
        bundle.transaction_id = pkt->transaction_id;
        // ... 根据 Bundle 类型解码
        return bundle;
    }
};

// ========== 泛型输出适配器（类型擦除） ==========
class GenericOutputStreamAdapter {
private:
    void* stream_ptr_;
    SimplePort* port_;
    std::function<bool()> is_valid_fn_;
    std::function<Packet*()> encode_fn_;
    std::function<void()> clear_valid_fn_;
    
public:
    template<typename BundleT>
    GenericOutputStreamAdapter(ch_stream<BundleT>& stream, SimplePort* port)
        : stream_ptr_(&stream), port_(port) {
        
        auto* s = static_cast<ch_stream<BundleT>*>(stream_ptr_);
        
        is_valid_fn_ = [s]() {
            return s->valid && s->ready;
        };
        
        encode_fn_ = [s]() {
            return encode_bundle<BundleT>(s->payload);
        };
        
        clear_valid_fn_ = [s]() {
            s->valid = false;
        };
    }
    
    void tick() {
        if (is_valid_fn_()) {
            Packet* pkt = encode_fn_();
            port_->send(pkt);
            clear_valid_fn_();
        }
    }
    
private:
    template<typename BundleT>
    Packet* encode_bundle(const BundleT& bundle) {
        Packet* pkt = new Packet(...);
        pkt->transaction_id = bundle.transaction_id;
        // ... 根据 Bundle 类型编码
        return pkt;
    }
};

#endif // GENERIC_STREAM_ADAPTER_HH
```

---

## module_registry.hh

```cpp
// include/framework/module_registry.hh
#ifndef MODULE_REGISTRY_HH
#define MODULE_REGISTRY_HH

#include "core/sim_object.hh"
#include "generic_stream_adapter.hh"
#include "bundles/cache_bundles.hh"
#include "bundles/noc_bundles.hh"
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <functional>

class ModuleRegistry {
private:
    std::vector<std::unique_ptr<SimObject>> modules_;
    std::vector<std::unique_ptr<GenericInputStreamAdapter>> input_adapters_;
    std::vector<std::unique_ptr<GenericOutputStreamAdapter>> output_adapters_;
    std::map<std::string, SimplePort*> port_map_;
    
public:
    ModuleRegistry() = default;
    ~ModuleRegistry() = default;
    
    // 创建 TLM 模块（自动创建 Adapter）
    template<typename ModuleT, typename ReqBundle, typename RespBundle>
    ModuleT* create_tlm_module(const std::string& name) {
        auto* module = new ModuleT(name);
        modules_.emplace_back(module);
        
        // 自动创建 Adapter
        auto* req_port = new SimplePort();
        auto* resp_port = new SimplePort();
        
        input_adapters_.emplace_back(
            new GenericInputStreamAdapter(req_port, module->get_req_in()));
        
        output_adapters_.emplace_back(
            new GenericOutputStreamAdapter(module->get_resp_out(), resp_port));
        
        port_map_[name + "_req"] = req_port;
        port_map_[name + "_resp"] = resp_port;
        
        return module;
    }
    
    // 获取端口（用于 PortPair 连接）
    SimplePort* get_port(const std::string& name) {
        return port_map_[name];
    }
    
    // tick 所有 Adapter
    void tick_adapters() {
        for (auto& adapter : input_adapters_) {
            adapter->tick();
        }
        for (auto& adapter : output_adapters_) {
            adapter->tick();
        }
    }
    
    // 访问模块（用于 EventQueue）
    auto& modules() { return modules_; }
    
    // 获取 Adapter 数量
    size_t input_adapter_count() const { return input_adapters_.size(); }
    size_t output_adapter_count() const { return output_adapters_.size(); }
};

#endif // MODULE_REGISTRY_HH
```

---

## impl_mode.hh

```cpp
// include/framework/impl_mode.hh
#ifndef IMPL_MODE_HH
#define IMPL_MODE_HH

// 实现模式枚举
enum class ImplType {
    TLM,        // 纯 TLM 实现（快速仿真）
    RTL,        // 纯 RTL 实现（CppHDL，周期精确）
    COMPARE,    // 并行运行 TLM+RTL，对比输出
    SHADOW      // TLM 主导，RTL 影子记录
};

// 实现模式字符串转换
inline std::string impl_type_to_string(ImplType type) {
    switch (type) {
        case ImplType::TLM: return "TLM";
        case ImplType::RTL: return "RTL";
        case ImplType::COMPARE: return "COMPARE";
        case ImplType::SHADOW: return "SHADOW";
        default: return "UNKNOWN";
    }
}

#endif // IMPL_MODE_HH
```

---

## port_pair.hh

```cpp
// include/framework/port_pair.hh
#ifndef PORT_PAIR_HH
#define PORT_PAIR_HH

#include "core/simple_port.hh"

// PortPair：解耦两个 SimplePort 的驱动关系
class PortPair {
private:
    SimplePort* port_a_;
    SimplePort* port_b_;
    bool connected_;
    
public:
    // 构造函数：连接两个端口
    PortPair(SimplePort* port_a, SimplePort* port_b)
        : port_a_(port_a), port_b_(port_b), connected_(true) {
        
        if (port_a_ && port_b_) {
            port_a_->set_peer(port_b_);
            port_b_->set_peer(port_a_);
        }
    }
    
    // 析构：断开连接
    ~PortPair() {
        if (port_a_ && port_b_) {
            port_a_->set_peer(nullptr);
            port_b_->set_peer(nullptr);
        }
    }
    
    // 禁止复制
    PortPair(const PortPair&) = delete;
    PortPair& operator=(const PortPair&) = delete;
    
    // 获取端口
    SimplePort* get_port_a() const { return port_a_; }
    SimplePort* get_port_b() const { return port_b_; }
    
    // 检查连接状态
    bool is_connected() const { return connected_; }
};

#endif // PORT_PAIR_HH
```

---

## event_queue.hh (示例扩展)

```cpp
// include/framework/event_queue_extension.hh
#ifndef EVENT_QUEUE_EXTENSION_HH
#define EVENT_QUEUE_EXTENSION_HH

#include "core/event_queue.hh"
#include <vector>
#include <functional>

// EventQueue 扩展：支持自定义 tick 回调
class ModuleEventQueue : public EventQueue {
private:
    std::vector<std::function<void()>> custom_ticks_;
    
public:
    // 添加自定义 tick 回调
    void add_custom_tick(std::function<void()> callback) {
        custom_ticks_.push_back(callback);
    }
    
    // 运行仿真
    void run(int max_cycles) override {
        for (int cycle = 0; cycle < max_cycles; cycle++) {
            // 执行所有模块 tick
            for (auto* module : get_modules()) {
                module->tick();
            }
            
            // 执行自定义 tick（Adapter tick）
            for (const auto& callback : custom_ticks_) {
                callback();
            }
            
            advance_time();
        }
    }
};

#endif // EVENT_QUEUE_EXTENSION_HH
```

---

## 示例说明

- **泛型 Adapter**：支持任意 Bundle 类型，类型擦除存储
- **模块注册器**：自动创建 Adapter，统一管理模块和适配器
- **PortPair**：解耦驱动关系，保持现有连接方式
- **自定义 Tick**：EventQueue 扩展支持 Adapter tick
- **ImplType**：支持未来 COMPARE/SHADOW 模式
