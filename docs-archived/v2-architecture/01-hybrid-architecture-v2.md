# CppTLM 混合仿真架构 v2.0

> **版本**: 2.0  
> **日期**: 2026-04-09  
> **状态**: ✅ 已批准  
> **替代**: v1.0 (过度设计方案)

---

## 1. 架构愿景

**核心目标**: 一次建模，多粒度仿真 — 在同一 C++ 框架下，支持事务级 (TLM) 和周期精确级 (RTL) 建模的无缝混合仿真。

**设计原则**:
1. **模块内部统一**: TLM/RTL 模块内部都使用 `ch_stream<T>` 接口
2. **Bundle 共享**: 一份 Bundle 定义，TLM/RTL 共用
3. **连接解耦**: 模块之间使用 `PortPair` 连接，解耦驱动关系
4. **框架自动化**: Port ↔ ch_stream 转换由框架自动处理，模块设计者无感知
5. **业务纯净**: 业务模块只关心 ch_stream 逻辑，不感知适配复杂度

---

## 2. 架构分层

```
┌─────────────────────────────────────────────────────────────┐
│  应用层：用户业务模块                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  CacheTLM    │  │ CrossbarTLM  │  │   NICTLM     │       │
│  │  (纯 ch_stream)│  │ (多端口 ch_stream)│  │ (Fragment)   │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  框架层：自动适配 + 调度                                      │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  ModuleRegistry  │  StreamAdapter  │  PortPair       │   │
│  │  自动创建 Adapter │  泛型类型擦除    │  解耦连接       │   │
│  └──────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│  Mapper 层：协议/格式转换                                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ FragmentMapper│  │  AXI4Mapper  │  │  CHIMapper   │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  Bundle 层：统一共享定义                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ CacheBundle  │  │  NoCBundle   │  │ AXI4Bundle   │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 目录结构

```
include/
├── bundles/                      # 统一 Bundle 定义
│   ├── cache_bundles.hh          # Cache 请求/响应 Bundle
│   ├── noc_bundles.hh            # NoC 数据包 Bundle
│   ├── axi4_bundles.hh           # AXI4 协议 Bundle
│   └── fragment_bundles.hh       # Fragment 分片 Bundle
│
├── tlm/                          # TLM 模块实现
│   ├── cache_tlm.hh              # Cache TLM 模块
│   ├── crossbar_tlm.hh           # Crossbar TLM 模块 (多端口)
│   ├── nic_tlm.hh                # NIC TLM 模块 (Fragment)
│   └── memory_tlm.hh             # Memory TLM 模块
│
├── rtl/                          # RTL 模块实现 (CppHDL)
│   ├── cache_component.hh        # Cache RTL Component
│   ├── crossbar_component.hh     # Crossbar RTL Component
│   ├── nic_component.hh          # NIC RTL Component
│   └── wrapper/
│       └── rtl_module_wrapper.hh # 通用 RTL Wrapper
│
├── mapper/                       # Mapper 层
│   ├── fragment_mapper.hh        # Fragment 分片/重组
│   ├── axi4_mapper.hh            # AXI4 协议映射
│   └── chi_mapper.hh             # CHI 协议映射
│
├── framework/                    # 框架层
│   ├── generic_stream_adapter.hh # 泛型 Stream Adapter
│   ├── module_registry.hh        # 模块注册器
│   ├── port_pair.hh              # PortPair 连接
│   └── impl_mode.hh              # 实现模式 (TLM/RTL/COMPARE/SHADOW)
│
└── core/                         # 核心依赖
    ├── sim_object.hh             # SimObject 基类
    ├── simple_port.hh            # SimplePort
    └── event_queue.hh            # EventQueue 调度
```

---

## 4. 核心设计

### 4.1 Bundle 层（统一共享）

**原则**: 一份 Bundle 定义，TLM/RTL 共用，减少重复。

```cpp
// include/bundles/cache_bundles.hh
struct CacheReqBundle : ch::bundle_base<CacheReqBundle> {
    ch_uint<64> transaction_id;
    ch_uint<32> address;
    ch_bool is_write;
    ch_uint<32> data;
    
    CH_BUNDLE_FIELDS_T(transaction_id, address, is_write, data)
};

struct CacheRespBundle : ch::bundle_base<CacheRespBundle> {
    ch_uint<64> transaction_id;
    ch_uint<32> data;
    ch_bool is_hit;
    ch_uint<8> latency_cycles;
    
    CH_BUNDLE_FIELDS_T(transaction_id, data, is_hit, latency_cycles)
};
```

---

### 4.2 TLM 模块（纯 ch_stream）

**原则**: 业务模块只看到 `ch_stream<T>`，无 Port/Adapter 复杂度。

```cpp
// include/tlm/cache_tlm.hh
class CacheTLM : public SimObject {
private:
    ch_stream<CacheReqBundle> req_in;
    ch_stream<CacheRespBundle> resp_out;
    std::map<uint64_t, uint64_t> cache_lines;
    
public:
    CacheTLM(const std::string& name) : SimObject(name) {
        req_in.init("req_in");
        resp_out.init("resp_out");
    }
    
    // 获取 ch_stream（用于框架绑定）
    ch_stream<CacheReqBundle>& get_req_in() { return req_in; }
    ch_stream<CacheRespBundle>& get_resp_out() { return resp_out; }
    
    void tick() override {
        if (req_in.valid && req_in.ready) {
            // 纯 ch_stream 业务逻辑
        }
        req_in.ready = true;
    }
};
```

---

### 4.3 多端口支持（Crossbar）

**原则**: 数组方式声明 `ch_stream<T>`，支持任意数量端口。

```cpp
// include/tlm/crossbar_tlm.hh
class CrossbarTLM : public SimObject {
private:
    static constexpr int NUM_PORTS = 4;
    std::array<ch_stream<NoCReqBundle>, NUM_PORTS> req_in;
    std::array<ch_stream<NoCRespBundle>, NUM_PORTS> resp_out;
    
public:
    CrossbarTLM(const std::string& name) : SimObject(name) {
        for (int i = 0; i < NUM_PORTS; i++) {
            req_in[i].init("req_in_" + std::to_string(i));
            resp_out[i].init("resp_out_" + std::to_string(i));
        }
    }
    
    ch_stream<NoCReqBundle>& get_req_in(int idx) { return req_in[idx]; }
    ch_stream<NoCRespBundle>& get_resp_out(int idx) { return resp_out[idx]; }
    
    void tick() override {
        // 轮询所有输入端口
        for (int i = 0; i < NUM_PORTS; i++) {
            if (req_in[i].valid && req_in[i].ready) {
                // 路由逻辑
            }
        }
    }
};
```

---

### 4.4 框架适配层（自动处理）

**原则**: Port ↔ ch_stream 转换由框架自动处理，模块设计者无感知。

```cpp
// include/framework/generic_stream_adapter.hh
class GenericInputStreamAdapter {
private:
    SimplePort* port_;
    void* stream_ptr_;  // 类型擦除
    std::function<void(Packet*)> decode_fn_;
    
public:
    template<typename BundleT>
    GenericInputStreamAdapter(SimplePort* port, ch_stream<BundleT>& stream)
        : port_(port), stream_ptr_(&stream) {
        decode_fn_ = [&stream](Packet* pkt) {
            stream.payload = decode_bundle<BundleT>(pkt);
            stream.valid = true;
        };
    }
    
    void tick() {
        Packet* pkt;
        if (port_->recv(pkt)) {
            decode_fn_(pkt);
        }
    }
};
```

---

### 4.5 模块注册器（自动创建 Adapter）

```cpp
// include/framework/module_registry.hh
class ModuleRegistry {
private:
    std::vector<std::unique_ptr<SimObject>> modules_;
    std::vector<std::unique_ptr<GenericInputStreamAdapter>> input_adapters_;
    std::vector<std::unique_ptr<GenericOutputStreamAdapter>> output_adapters_;
    std::map<std::string, SimplePort*> port_map_;
    
public:
    template<typename ModuleT, typename ReqBundle, typename RespBundle>
    ModuleT* create_tlm_module(const std::string& name) {
        auto* module = new ModuleT(name);
        modules_.emplace_back(module);
        
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
    
    SimplePort* get_port(const std::string& name) {
        return port_map_[name];
    }
    
    void tick_adapters() {
        for (auto& adapter : input_adapters_) adapter->tick();
        for (auto& adapter : output_adapters_) adapter->tick();
    }
};
```

---

### 4.6 系统连接（PortPair 解耦）

```cpp
// src/cache_system_main.cpp
int sc_main() {
    ModuleRegistry registry;
    
    // 创建模块（自动创建 Adapter）
    auto* cpu = registry.create_tlm_module<CPUSim, CacheReqBundle, CacheRespBundle>("cpu");
    auto* l1 = registry.create_tlm_module<CacheTLM, CacheReqBundle, CacheRespBundle>("l1_cache");
    auto* l2 = registry.create_tlm_module<CacheTLM, CacheReqBundle, CacheRespBundle>("l2_cache");
    auto* mem = registry.create_tlm_module<MemorySim, CacheReqBundle, CacheRespBundle>("memory");
    
    // 连接模块（PortPair，解耦驱动关系）
    new PortPair(registry.get_port("cpu_resp"), registry.get_port("l1_cache_req"));
    new PortPair(registry.get_port("l1_cache_resp"), registry.get_port("l2_cache_req"));
    new PortPair(registry.get_port("l2_cache_resp"), registry.get_port("memory_req"));
    
    // 运行仿真
    EventQueue eq;
    for (auto& module : registry.modules()) {
        eq.add_module(module.get());
    }
    
    eq.add_custom_tick([&]() {
        registry.tick_adapters();
    });
    
    eq.run(100000);
    
    return 0;
}
```

---

## 5. 关键特性

### 5.1 Fragment 支持（Mapper 层）

```cpp
// include/mapper/fragment_mapper.hh
class FragmentMapper {
public:
    // 分片：NoCReqBundle → FragmentBundle 流
    static std::vector<FragmentBundle<uint64_t>> fragment(
        const NoCReqBundle& bundle,
        size_t beat_size = 64);
    
    // 重组：FragmentBundle 流 → NoCReqBundle
    static NoCReqBundle reassemble(
        const std::vector<FragmentBundle<uint64_t>>& fragments);
};
```

---

### 5.2 双并行模式（未来扩展）

```cpp
// include/framework/impl_mode.hh
enum class ImplType {
    TLM,        // 纯 TLM 实现（快速仿真）
    RTL,        // 纯 RTL 实现（CppHDL，周期精确）
    COMPARE,    // 并行运行 TLM+RTL，对比输出
    SHADOW      // TLM 主导，RTL 影子记录
};
```

---

## 6. 架构优势

| 特性 | v1.0 (过度设计) | v2.0 (简化方案) |
|------|----------------|----------------|
| **模块接口** | InputStream/OutputStream 复杂抽象 | 纯 ch_stream |
| **类型处理** | IRTLComponent 类型擦除三层 | 泛型模板 + 类型擦除 |
| **Port 位置** | 模块内部感知 Port | 框架层自动处理 |
| **多端口** | 端口映射表复杂查找 | 数组方式直接声明 |
| **Bundle** | TLM/RTL 各自定义 | **统一共享** |
| **代码行数** | ~500 行 | ~200 行 |
| **模块设计者** | 理解框架机制 | **只关心 ch_stream** |

---

## 7. 相关文档

| 文档 | 位置 |
|------|------|
| PRD-002 (产品需求) | `../01-product-requirements/PRD-002-final.md` |
| ADR 索引 | `../03-adr/README.md` |
| FragmentMapper 决议 | `FRAGMENT_MAPPER_DECISIONS.md` |
| P0/P1/P2 决策汇总 | `P0_P1_P2_DECISIONS.md` |

---

## 8. 附录：完整示例代码

| 示例 | 位置 | 说明 |
|------|------|------|
| **Bundle 示例** | `examples/bundles/example_bundles.md` | Cache/NoC/Fragment/AXI4/CHI Bundle 定义 |
| **TLM 模块示例** | `examples/tlm/example_tlm_modules.md` | CacheTLM/CrossbarTLM/NICTLM/MemoryTLM |
| **RTL 模块示例** | `examples/rtl/example_rtl_modules.md` | CacheComponent/CrossbarComponent/NICComponent |
| **框架示例** | `examples/framework/example_framework.md` | Adapter/Registry/PortPair/EventQueue |

---

**版本**: v2.0  
**创建日期**: 2026-04-09  
**批准状态**: ✅ 已批准  
**下一步**: ADR P1 级议题确认
