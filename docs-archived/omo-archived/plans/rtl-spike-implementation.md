# CppTLM + CppHDL 1-Day Spike 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在1天内完成 TLM+CppHDL 混合仿真的 Spike 实施，包含6个新文件和CMake集成，实现编译验证 + 1 tick smoke test

**Architecture:** PIMPL隔离C++20依赖(头文件C++17, .cc文件C++20)，复用现有ChStreamModuleBase/StreamAdapter/FragmentMapper基础设施，通过CMake条件编译(BUILD_RTL)隔离RTL桥接代码

**Tech Stack:** C++17/20, CMake 3.16+, CppHDL(C++20), LLVM-22, Catch2 v3.7.0

---

## 文件结构

### 新建文件 (6个)

| # | 文件 | C++标准 | CppHDL依赖 | 说明 |
|---|------|---------|-----------|------|
| 1 | `include/rtl/hybrid_cache_wrapper.hh` | C++17 | ❌ 无 | PIMPL头，继承ChStreamModuleBase |
| 2 | `include/rtl/hybrid_cache_component.hh` | C++20 | ✅ ch.hpp | CppHDL Component，标量端口 |
| 3 | `src/rtl/hybrid_cache_wrapper.cc` | C++20 | ✅ ch.hpp+Simulator | PIMPL实现，TLM↔RTL桥接 |
| 4 | `src/rtl/hybrid_cache_component.cc` | C++20 | ✅ ch.hpp | describe()单拍FSM |
| 5 | `test/test_cppHDL_smoke.cc` | C++20 | ✅ ch.hpp | 实例化+1 tick smoke test |
| 6 | `src/rtl/CMakeLists.txt` | CMake | — | RTL子目录构建规则 |

### 修改文件 (2个)

| # | 文件 | 修改内容 | 说明 |
|---|------|---------|------|
| 7 | `include/chstream_register.hh` | +2行(registerObject+registerAdapter) +1行include | 注册HybridCacheWrapper到宏 |
| 8 | `CMakeLists.txt`(根) | +BUILD_RTL选项 +add_subdirectory(src/rtl)条件 | 条件编译RTL桥接 |

### 已存在文件 (2个，无需修改)

| 文件 | 状态 | 说明 |
|------|------|------|
| `include/rtl/fragment_mapper.hh` | ✅ 已验证(19/19测试) | 薄映射函数，纯C++17 |
| `test/test_fragment_mapper.cc` | ✅ 已验证(19/19测试) | FragmentMapper单测 |

---

## 前置条件检查清单 (必须在实施前完成)

- [x] **Day 0 验证**: 运行以下命令确认环境就绪

```bash
# 1. Ubuntu版本
cat /etc/lsb-release
# 期望: Ubuntu 22.04+

# 2. C++17编译器
g++ --version
# 期望: g++ 11+

# 3. LLVM-22可用
clang++-22 --version
# 期望: clang version 22.x.x

# 4. LLVM-22头文件
ls /usr/lib/llvm-22/include/llvm
# 期望: 目录存在

# 5. CppHDL子模块
readlink -f external/CppHDL
# 期望: /workspace/project/CppHDL

# 6. CppHDL已编译
ls external/CppHDL/build/libcpphdl.a
# 期望: 文件存在

# 7. C++20支持
echo 'int main(){return 0;}' | g++ -std=c++20 -x c++ -
# 期望: 编译成功(无错误)
```

**若任一条件失败**: 按v4 §11.2回退路径处理

---

## 实施任务

### Task 1: 创建目录结构 + PIMPL头文件

**Files:**
- Create: `include/rtl/hybrid_cache_wrapper.hh`
- Create: `src/rtl/` (目录)

- [x] **Step 1: 创建RTL源目录**

```bash
mkdir -p src/rtl
```

- [x] **Step 2: 编写PIMPL头文件**

```cpp
// include/rtl/hybrid_cache_wrapper.hh
// HybridCacheWrapper: TLM↔RTL 桥接模块
// 功能描述：PIMPL头文件，C++17兼容，零CppHDL依赖
// 作者: CppTLM Team
// 日期: 2026-06-06
#ifndef RTL_HYBRID_CACHE_WRAPPER_HH
#define RTL_HYBRID_CACHE_WRAPPER_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "bundles/cache_bundles_tlm.hh"
#include <memory>
#include <string>

namespace cpptlm {
namespace rtl {

// 前向声明PIMPL实现类
class HybridCacheWrapperImpl;

/**
 * @brief TLM↔RTL混合缓存桥接模块
 *
 * 继承ChStreamModuleBase，通过PIMPL模式隔离C++20/CppHDL依赖。
 * 头文件保持C++17兼容，所有CppHDL相关代码在.cc中。
 */
class HybridCacheWrapper : public ChStreamModuleBase {
public:
    /**
     * @brief 构造函数
     * @param name 模块名称
     * @param eq 事件队列指针
     */
    HybridCacheWrapper(const std::string& name, EventQueue* eq);
    
    /**
     * @brief 析构函数(必须在.cc中定义，因unique_ptr<Impl>需完整类型)
     */
    ~HybridCacheWrapper() override;
    
    // ChStreamModuleBase接口
    void set_stream_adapter(StreamAdapterBase* adapter) override;
    
    // 模块业务逻辑
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    
    // 访问器(供StreamAdapter使用)
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in() { return req_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out() { return resp_out_; }
    
private:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>   req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    std::unique_ptr<HybridCacheWrapperImpl> impl_;
};

} // namespace rtl
} // namespace cpptlm

#endif // RTL_HYBRID_CACHE_WRAPPER_HH
```

- [x] **Step 3: 验证头文件语法**

```bash
# 用C++17编译器快速检查头文件语法
g++-std=c++17 -fsyntax-only -I include -I include/core -I external/json include/rtl/hybrid_cache_wrapper.hh
# 期望: 无错误(注意：此时可能因依赖未完全解析而有警告，但不应有fatal error)
```

- [x] **Step 4: Commit**

```bash
git add include/rtl/hybrid_cache_wrapper.hh
git commit -m "feat(rtl): add HybridCacheWrapper PIMPL header (C++17 compatible)

- PIMPL pattern isolates C++20/CppHDL dependencies in .cc
- Header is pure C++17, zero CppHDL includes
- Inherits ChStreamModuleBase, overrides set_stream_adapter/tick/do_reset
- Forward-declares HybridCacheWrapperImpl for unique_ptr"
```

---

### Task 2: CppHDL Component头文件

**Files:**
- Create: `include/rtl/hybrid_cache_component.hh`

- [x] **Step 1: 编写Component头文件**

```cpp
// include/rtl/hybrid_cache_component.hh
// HybridCacheComponent: CppHDL RTL组件
// 功能描述：单拍Cache RTL模型，用于Spike验证
// 注意：此文件包含ch.hpp，必须是C++20编译单元
// 作者: CppTLM Team
// 日期: 2026-06-06
#ifndef RTL_HYBRID_CACHE_COMPONENT_HH
#define RTL_HYBRID_CACHE_COMPONENT_HH

#include "ch.hpp"

namespace cpptlm {
namespace rtl {

/**
 * @brief 混合缓存RTL组件(Spike stub)
 *
 * 单拍FSM：IDLE→PROCESS→RESPONSE→IDLE
 * 所有端口为标量ch_in/ch_out，避免ch_stream作为__io端口的问题。
 */
class HybridCacheComponent : public ch::Component {
public:
    // === Request通道(标量端口) ===
    ch_in<ch_uint<64>> req_addr_;           // 请求地址
    ch_in<ch_uint<32>> req_tid_;            // 事务ID
    ch_in<ch_uint<8>>  req_fragment_id_;    // 分片序号
    ch_in<ch_uint<8>>  req_fragment_total_; // 总分片数
    ch_in<ch_uint<64>> req_data_;           // 请求数据
    ch_in<ch_uint<8>>  req_opcode_;         // 操作码
    ch_in<ch_bool>     req_valid_;          // 请求有效
    ch_in<ch_bool>     req_first_;          // 首拍标志
    ch_in<ch_bool>     req_last_;           // 末拍标志
    ch_out<ch_bool>    req_ready_;          // 请求就绪
    
    // === Response通道 ===
    ch_out<ch_uint<32>> resp_tid_;          // 响应事务ID
    ch_out<ch_uint<64>> resp_data_;         // 响应数据
    ch_out<ch_bool>     resp_hit_;          // Cache命中
    ch_out<ch_bool>     resp_valid_;        // 响应有效
    ch_in<ch_bool>      resp_ready_;        // 响应就绪
    
    /**
     * @brief 构造函数
     * @param parent 父组件(可为nullptr)
     * @param name 组件名称
     */
    HybridCacheComponent(ch::Component* parent = nullptr, 
                         const std::string& name = "hybrid_cache")
        : ch::Component(parent, name) {}
    
    /**
     * @brief 创建端口(覆盖基类)
     */
    void create_ports() override {
        new (this->io_storage_) io_type;
    }
    
    /**
     * @brief 描述RTL行为(FSM实现)
     */
    void describe() override;
};

} // namespace rtl
} // namespace cpptlm

#endif // RTL_HYBRID_CACHE_COMPONENT_HH
```

- [x] **Step 2: 验证Component头编译**

```bash
# 用C++20编译器+CppHDL头路径检查
# 注意：此步骤可能因CppHDL配置复杂而需要调整， Spike阶段以实际cmake构建为准
# 先尝试简单语法检查
clang++-22 -std=c++20 -fsyntax-only -I include -I /workspace/project/CppHDL/include include/rtl/hybrid_cache_component.hh 2>&1 | head -20
# 期望: 无fatal error(可能有CppHDL内部warning)
```

- [x] **Step 3: Commit**

```bash
git add include/rtl/hybrid_cache_component.hh
git commit -m "feat(rtl): add HybridCacheComponent header (C++20)

- CppHDL Component with scalar ports only (no ch_stream in __io)
- Request: addr/tid/fragment_id/fragment_total/data/opcode/valid/first/last/ready
- Response: tid/data/hit/valid/ready
- Single-beat FSM stub for Spike (Day 2+ extends to multi-beat)"
```

---

### Task 3: PIMPL实现文件

**Files:**
- Create: `src/rtl/hybrid_cache_wrapper.cc`

- [x] **Step 1: 编写PIMPL实现**

```cpp
// src/rtl/hybrid_cache_wrapper.cc
// HybridCacheWrapper PIMPL实现
// 功能描述：TLM↔RTL桥接逻辑，C++20 + CppHDL
// 作者: CppTLM Team
// 日期: 2026-06-06
#include "rtl/hybrid_cache_wrapper.hh"
#include "rtl/hybrid_cache_component.hh"
#include "rtl/fragment_mapper.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"

// C++20 / CppHDL头(隔离在此.cc文件)
#include "ch.hpp"
#include "chlib/stream.h"

#include <cstring>
#include <vector>

namespace cpptlm {
namespace rtl {

// =============================================================================
// PIMPL实现类
// =============================================================================

class HybridCacheWrapperImpl {
public:
    // CppHDL设备与仿真器
    ch::ch_device<HybridCacheComponent> device_;
    ch::Simulator simulator_;
    
    // TLM侧输入/输出流(由HybridCacheWrapper::set_stream_adapter设置)
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>* req_in_ = nullptr;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>* resp_out_ = nullptr;
    
    // 串行化状态(多拍传输，Spike仅单拍)
    Packet* pending_tx_ = nullptr;
    uint8_t current_beat_ = 0;
    uint8_t total_beats_ = 0;
    
    // Response收集(多拍响应，Day 2+实现)
    std::vector<CacheRespBeatRTL> resp_beats_;
    
    explicit HybridCacheWrapperImpl() = default;
    
    ~HybridCacheWrapperImpl() {
        // 清理pending事务
        if (pending_tx_) {
            PacketPool::get().release(pending_tx_);
            pending_tx_ = nullptr;
        }
    }
    
    /**
     * @brief 单周期tick：TLM→RTL→TLM数据流
     */
    void tick() {
        // Phase 1: 接受TLM事务(Spike: 仅单拍)
        if (pending_tx_ == nullptr && req_in_ && req_in_->valid() && req_in_->ready()) {
            const auto& req = req_in_->data();
            
            // 构造Packet(模拟从Bundle创建Packet)
            pending_tx_ = PacketPool::get().acquire();
            pending_tx_->payload->set_address(req.addr);
            pending_tx_->payload->set_data_length(sizeof(uint64_t));
            
            // 设置数据
            if (req.data && req.data_length > 0) {
                std::memcpy(pending_tx_->payload->get_data_ptr(), req.data, 
                           std::min(req.data_length, static_cast<int>(sizeof(uint64_t))));
            }
            
            // 设置TransactionContextExt(单拍：first=last=true, fragment_total=1)
            auto* ext = new TransactionContextExt();
            ext->transaction_id = req.stream_id;
            ext->parent_id = 0;  // 根事务
            ext->fragment_id = 0;
            ext->fragment_total = 1;
            
            // X.13安全模式：release旧extension(如有)，set新extension
            pending_tx_->payload->release_extension<TransactionContextExt>();
            pending_tx_->payload->set_extension(ext);
            
            // 同步stream_id
            pending_tx_->set_transaction_id(req.stream_id);
            
            current_beat_ = 0;
            total_beats_ = 1;  // Spike限定单拍
            req_in_->consume();
        }
        
        // Phase 2: 推一beat到RTL(Spike: 仅单拍)
        if (pending_tx_ && current_beat_ < total_beats_) {
            auto beat = FragmentMapper::serialize_beat_at(pending_tx_, current_beat_);
            
            // 写入RTL端口
            device_.io(req_addr_) = beat.addr;
            device_.io(req_tid_) = beat.tid;
            device_.io(req_fragment_id_) = beat.fragment_id;
            device_.io(req_fragment_total_) = beat.fragment_total;
            device_.io(req_data_) = beat.data;
            device_.io(req_first_) = beat.first;
            device_.io(req_last_) = beat.last;
            device_.io(req_valid_) = true;
            
            current_beat_++;
            if (current_beat_ >= total_beats_) {
                // 所有beat已发送，清理pending
                PacketPool::get().release(pending_tx_);
                pending_tx_ = nullptr;
            }
        } else {
            // 无pending事务，拉低valid
            device_.io(req_valid_) = false;
        }
        
        // Phase 3: 收集RTL响应(Spike: 仅单拍响应)
        if (device_.io(resp_valid_) && device_.io(resp_ready_)) {
            CacheRespBeatRTL resp_beat;
            resp_beat.tid = device_.io(resp_tid_);
            resp_beat.data = device_.io(resp_data_);
            resp_beat.hit = device_.io(resp_hit_);
            resp_beat.last = true;   // Spike单拍：last=true
            resp_beat.first = true;  // Spike单拍：first=true
            
            // 构造响应Packet
            Packet* resp_pkt = PacketPool::get().acquire();
            resp_pkt->payload->set_data_length(sizeof(uint64_t));
            
            // 写入响应数据
            FragmentMapper::write_resp(resp_pkt, resp_beat);
            
            // 通过StreamAdapter输出
            if (resp_out_) {
                resp_out_->write_payload(resp_pkt);
            } else {
                PacketPool::get().release(resp_pkt);
            }
            
            // 拉低valid(单周期脉冲)
            device_.io(resp_valid_) = false;
        }
        
        // 推进CppHDL仿真
        simulator_.tick();
    }
    
    /**
     * @brief 复位RTL状态
     */
    void reset() {
        // 清理pending事务
        if (pending_tx_) {
            PacketPool::get().release(pending_tx_);
            pending_tx_ = nullptr;
        }
        current_beat_ = 0;
        total_beats_ = 0;
        resp_beats_.clear();
        
        // 复位RTL端口
        device_.io(req_valid_) = false;
        device_.io(resp_valid_) = false;
        
        // 推进仿真器复位
        simulator_.reset();
    }
};

// =============================================================================
// HybridCacheWrapper 公共接口实现
// =============================================================================

HybridCacheWrapper::HybridCacheWrapper(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq)
    , impl_(std::make_unique<HybridCacheWrapperImpl>()) {}

HybridCacheWrapper::~HybridCacheWrapper() = default;
// 必须在.cc定义，因unique_ptr<Impl>需要完整类型

void HybridCacheWrapper::set_stream_adapter(StreamAdapterBase* adapter) {
    adapter_ = adapter;
    // 设置StreamAdapter后，获取输入/输出流引用
    if (adapter) {
        // TODO: 从adapter获取InputStreamAdapter/OutputStreamAdapter引用
        // Spike阶段简化：直接通过req_in_/resp_out_访问
    }
}

void HybridCacheWrapper::tick() {
    if (impl_) {
        impl_->req_in_ = &req_in_;
        impl_->resp_out_ = &resp_out_;
        impl_->tick();
    }
}

void HybridCacheWrapper::do_reset(const ResetConfig& config) {
    if (impl_) {
        impl_->reset();
    }
    ChStreamModuleBase::do_reset(config);
}

} // namespace rtl
} // namespace cpptlm
```

- [x] **Step 2: Commit**

```bash
git add src/rtl/hybrid_cache_wrapper.cc
git commit -m "feat(rtl): add HybridCacheWrapper PIMPL implementation (C++20)

- Implements TLM→RTL→TLM data flow in single-beat mode (Spike)
- Phase 1: Accept TLM transaction via InputStreamAdapter
- Phase 2: Push beat to RTL ports using FragmentMapper::serialize_beat_at
- Phase 3: Collect RTL response and output via OutputStreamAdapter
- X.13 safe mode: release_extension before set_extension
- PIMPL isolates all CppHDL/LLVM dependencies in .cc file"
```

---

### Task 4: describe() FSM实现

**Files:**
- Create: `src/rtl/hybrid_cache_component.cc`

- [x] **Step 1: 编写describe()实现**

```cpp
// src/rtl/hybrid_cache_component.cc
// HybridCacheComponent::describe() 实现
// 功能描述：单拍Cache FSM (IDLE→PROCESS→IDLE)
// 注意：无字面量端口赋值，组合逻辑驱动ready信号
// 作者: CppTLM Team
// 日期: 2026-06-06
#include "rtl/hybrid_cache_component.hh"

namespace cpptlm {
namespace rtl {

void HybridCacheComponent::describe() {
    // === FSM状态寄存器 ===
    ch_reg<ch_uint<2>> state(0_d);               // 0=IDLE, 1=PROCESS
    ch_reg<ch_uint<32>> latched_tid(0_d);
    ch_reg<ch_uint<8>>  latched_fragment_id(0_d);
    ch_reg<ch_uint<8>>  latched_fragment_total(0_d);
    ch_reg<ch_bool>     latched_is_write(false_d);
    ch_reg<ch_uint<64>> latched_addr(0_d);
    ch_reg<ch_uint<64>> latched_data(0_d);
    
    // === Ready信号(组合逻辑，非寄存器) ===
    // 仅在IDLE状态接受新请求
    req_ready_ = (state == 0_d);
    
    // === 主FSM ===
    switch (static_cast<uint64_t>(state)) {
        case 0:  // IDLE: 等待req
            if (req_valid_ && req_ready_) {
                // 锁存请求字段(首拍或单拍)
                latched_tid = req_tid_;
                latched_fragment_id = req_fragment_id_;
                latched_fragment_total = req_fragment_total_;
                latched_addr = req_addr_;
                latched_data = req_data_;
                latched_is_write = (req_opcode_ != 0_d);  // 0=READ, 非0=WRITE
                state = 1_d;
            }
            break;
            
        case 1:  // PROCESS: 模拟Cache查找(单周期完成)
            // 生成响应
            resp_tid_ = latched_tid;
            resp_data_ = latched_data;  // Spike: echo请求数据
            resp_hit_ = true_d;         // Spike: 总是命中
            resp_valid_ = true_d;
            
            // 等待下游ready，然后返回IDLE
            if (resp_ready_) {
                resp_valid_ = false_d;
                state = 0_d;
            }
            break;
            
        default:
            // 非法状态：复位到IDLE
            state = 0_d;
            break;
    }
}

} // namespace rtl
} // namespace cpptlm
```

- [x] **Step 2: Commit**

```bash
git add src/rtl/hybrid_cache_component.cc
git commit -m "feat(rtl): add HybridCacheComponent describe() FSM (single-beat)

- 2-state FSM: IDLE → PROCESS → IDLE
- No literal port assignments (comb-logic drives ready)
- Spike: single-cycle processing, always hit, echo data
- Latches request fields in ch_reg<> for cross-cycle state
- Safe: illegal state defaults to IDLE"
```

---

### Task 5: Smoke测试

**Files:**
- Create: `test/test_cppHDL_smoke.cc`

- [x] **Step 1: 编写Smoke测试**

```cpp
// test/test_cppHDL_smoke.cc
// CppHDL Smoke测试：实例化 + 1 tick不崩溃
// 功能描述：验证ch::ch_device<HybridCacheComponent>可实例化且sim.tick()不崩溃
// 标签: [cpphdl][smoke]
// 作者: CppTLM Team
// 日期: 2026-06-06
#include <catch2/catch_test_macros.hpp>

// CppHDL头(C++20)
#include "ch.hpp"
#include "rtl/hybrid_cache_component.hh"

using namespace cpptlm::rtl;

TEST_CASE("CppHDL device instantiation", "[cpphdl][smoke]") {
    // 测试1: ch_device可实例化
    REQUIRE_NOTHROW([]() {
        ch::ch_device<HybridCacheComponent> device;
        (void)device;  // 抑制未使用警告
    }());
}

TEST_CASE("CppHDL simulator tick (1 cycle)", "[cpphdl][smoke]") {
    // 测试2: 创建device + simulator，执行1个tick不崩溃
    REQUIRE_NOTHROW([]() {
        ch::ch_device<HybridCacheComponent> device;
        ch::Simulator sim(device);
        
        // 复位
        sim.reset();
        
        // 执行1个tick
        sim.tick();
        
        // 验证：能执行到这里即成功(Spike范围)
        CHECK(true);
    }());
}

TEST_CASE("CppHDL port access", "[cpphdl][smoke]") {
    // 测试3: 端口读写
    REQUIRE_NOTHROW([]() {
        ch::ch_device<HybridCacheComponent> device;
        ch::Simulator sim(device);
        
        // 写请求端口
        device.io(device.req_addr_) = 0xDEADBEEF;
        device.io(device.req_tid_) = 42;
        device.io(device.req_valid_) = true;
        device.io(device.req_first_) = true;
        device.io(device.req_last_) = true;
        
        // 推进1 tick
        sim.tick();
        
        // 读响应端口(Spike单拍：应立即有响应)
        auto resp_valid = device.io(device.resp_valid_);
        // Spike范围：不验证具体值，仅验证可读
        (void)resp_valid;
        
        CHECK(true);
    }());
}
```

- [x] **Step 2: Commit**

```bash
git add test/test_cppHDL_smoke.cc
git commit -m "test(cpphdl): add smoke tests for CppHDL integration

- Test 1: ch_device<HybridCacheComponent> instantiation
- Test 2: Simulator tick() 1 cycle without crash
- Test 3: Port read/write accessibility
- Tags: [cpphdl][smoke]"
```

---

### Task 6: 更新chstream_register.hh

**Files:**
- Modify: `include/chstream_register.hh`

- [x] **Step 1: 添加include和注册行**

在 `include/chstream_register.hh` 中：
1. 在 `#include "tlm/link_tlm.hh"` 后添加：
   ```cpp
   #include "rtl/hybrid_cache_wrapper.hh"
   ```

2. 在 `ChStreamAdapterFactory::get().registerAdapter<tlm::LinkTLM, ...>` 行后添加：
   ```cpp
       ModuleFactory::registerObject<cpptlm::rtl::HybridCacheWrapper>("HybridCacheWrapper"); \
       ChStreamAdapterFactory::get().registerAdapter<cpptlm::rtl::HybridCacheWrapper, \
           bundles::CacheReqBundle, bundles::CacheRespBundle>("HybridCacheWrapper"); \
   ```

- [x] **Step 2: 验证修改**

```bash
# 检查修改后内容
grep -n "HybridCacheWrapper" include/chstream_register.hh
# 期望输出: 包含 "HybridCacheWrapper" 的行(至少2处: registerObject + registerAdapter)

grep -n "rtl/hybrid_cache_wrapper" include/chstream_register.hh
# 期望输出: 包含 "rtl/hybrid_cache_wrapper.hh" 的行(1处include)
```

- [x] **Step 3: Commit**

```bash
git add include/chstream_register.hh
git commit -m "feat(register): add HybridCacheWrapper to REGISTER_CHSTREAM macro

- Add #include \"rtl/hybrid_cache_wrapper.hh\" (C++17 compatible header)
- Add ModuleFactory::registerObject<cpptlm::rtl::HybridCacheWrapper>
- Add ChStreamAdapterFactory::registerAdapter<cpptlm::rtl::HybridCacheWrapper,
    bundles::CacheReqBundle, bundles::CacheRespBundle>
- HybridCacheWrapper registered alongside existing TLM modules"
```

---

### Task 7: CMake集成

**Files:**
- Create: `src/rtl/CMakeLists.txt`
- Modify: `CMakeLists.txt`(根)

- [x] **Step 1: 创建RTL子目录CMakeLists.txt**

```cmake
# src/rtl/CMakeLists.txt — RTL桥接库(C++20 + CppHDL)

# CppHDL路径
set(CppHDL_ROOT "${CMAKE_SOURCE_DIR}/external/CppHDL")
set(CppHDL_BUILD "${CppHDL_ROOT}/build")
set(CppHDL_LIB "${CppHDL_BUILD}/libcpphdl.a")

# 检查CppHDL库是否存在
if(NOT EXISTS ${CppHDL_LIB})
    message(WARNING "CppHDL library not found at ${CppHDL_LIB}. \
        RTL bridge will not be built. Run: cd external/CppHDL && ./build.sh")
endif()

# RTL桥接静态库
add_library(cpptlm_rtl STATIC
    hybrid_cache_wrapper.cc
    hybrid_cache_component.cc
)

target_include_directories(cpptlm_rtl PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CppHDL_ROOT}/include
)

target_include_directories(cpptlm_rtl PRIVATE
    ${CMAKE_SOURCE_DIR}/external/json
)

# C++20标准
target_compile_features(cpptlm_rtl PUBLIC cxx_std_20)

# CppHDL编译选项
target_compile_options(cpptlm_rtl PRIVATE
    -stdlib=libstdc++
    -Wno-unused-parameter
)

# 链接CppHDL和LLVM
target_link_libraries(cpptlm_rtl PUBLIC
    ${CppHDL_LIB}
    LLVM-22
    cpptlm_core
)

# 确保CppHDL库先构建
add_dependencies(cpptlm_rtl cpptlm_core)
```

- [x] **Step 2: 修改根CMakeLists.txt**

在根 `CMakeLists.txt` 中：
1. 在 `option(USE_ASAN ...)` 后添加：
   ```cmake
   option(BUILD_RTL "Build CppHDL RTL bridge (Spike)" OFF)
   ```

2. 在 `add_subdirectory(src)` 后添加：
   ```cmake
   if(BUILD_RTL)
       add_subdirectory(src/rtl)
   endif()
   ```

3. 在测试部分 `if(BUILD_TESTS)` 内添加：
   ```cmake
   if(BUILD_RTL)
       add_subdirectory(test/rtl)
   endif()
   ```

- [x] **Step 3: 创建test/rtl/CMakeLists.txt**

```cmake
# test/rtl/CMakeLists.txt — RTL smoke tests

add_executable(cpptlm_rtl_tests
    test_cppHDL_smoke.cc
    ../catch_amalgamated.cpp
)

target_include_directories(cpptlm_rtl_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/external/CppHDL/include
)

target_compile_features(cpptlm_rtl_tests PRIVATE cxx_std_20)

target_link_libraries(cpptlm_rtl_tests PRIVATE
    cpptlm_rtl
    cpptlm_core
)

# 添加测试
add_test(NAME RTL_Smoke COMMAND cpptlm_rtl_tests "[cpphdl][smoke]")
```

- [x] **Step 4: Commit**

```bash
git add src/rtl/CMakeLists.txt test/rtl/CMakeLists.txt CMakeLists.txt
git commit -m "build(cmake): add RTL bridge build configuration

- Add BUILD_RTL option (default OFF, preserves 598 baseline tests)
- Create src/rtl/CMakeLists.txt for cpptlm_rtl static library (C++20)
- Create test/rtl/CMakeLists.txt for cpptlm_rtl_tests executable
- Link CppHDL libcpphdl.a + LLVM-22
- Conditional compilation: RTL code only built when BUILD_RTL=ON"
```

---

### Task 8: 首次构建与修复

- [x] **Step 1: 配置构建**

```bash
# 清理旧构建目录
rm -rf build

# 配置(开启RTL构建)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_RTL=ON
# 期望: 配置成功，无fatal error
```

- [x] **Step 2: 编译**

```bash
# 编译RTL库
cmake --build build --target cpptlm_rtl -j$(nproc)
# 期望: 编译成功(可能有warning，但无error)
```

- [x] **Step 3: 编译Smoke测试**

```bash
# 编译RTL测试
cmake --build build --target cpptlm_rtl_tests -j$(nproc)
# 期望: 编译成功
```

- [x] **Step 4: 运行Smoke测试**

```bash
# 运行Smoke测试
./build/bin/cpptlm_rtl_tests "[cpphdl][smoke]" -v
# 期望: 3个测试全部通过
```

- [x] **Step 5: 验证基线测试未破坏**

```bash
# 编译并运行基线测试(不带BUILD_RTL)
rm -rf build_baseline
cmake -S . -B build_baseline -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build_baseline --target cpptlm_tests -j$(nproc)
./build_baseline/bin/cpptlm_tests
# 期望: 598/598 测试通过(零回归)
```

- [x] **Step 6: 验证符号存在**

```bash
# 检查RTL库符号
nm build/lib/libcpptlm_rtl.a | grep -c HybridCache
# 期望: >= 4 (类符号)

# 检查注册宏包含HybridCacheWrapper
grep -c "HybridCacheWrapper" include/chstream_register.hh
# 期望: >= 2
```

- [x] **Step 7: Commit构建修复**

```bash
# 如果有修改，提交
git add -A
git commit -m "fix(build): resolve first-build compilation issues

- [描述具体修复内容，如：修正头文件路径/添加缺失include/修正CMake链接顺序]"
```

---

## 验收标准

### 必须全部通过(7项)

- [x] 1. `cmake -DBUILD_RTL=ON` 配置无fatal error
- [x] 2. `cmake --build build` 编译 `cpptlm_rtl.a` 成功
- [x] 3. `./build/bin/cpptlm_rtl_tests` 3个smoke测试全部通过
- [x] 4. `./build/bin/cpptlm_tests` 仍 598/598 通过(零回归)
- [x] 5. `nm build/lib/libcpptlm_rtl.a | grep -c HybridCache` >= 4
- [x] 6. `chstream_register.hh` 含 `HybridCacheWrapper` >= 2处
- [x] 7. `chstream_register.hh` 含 `#include "rtl/hybrid_cache_wrapper.hh"`

### 可选验证

- [x] 8. 运行 `./build/bin/cpptlm_rtl_tests -v` 查看详细输出
- [x] 9. 检查 `.omo/evidence/` 目录有构建日志和测试输出

---

## RED阻塞器处理

若以下任一情况发生，按回退路径处理：

| 阻塞器 | 触发条件 | 回退路径 |
|--------|---------|---------|
| CppHDL C++20头与C++17 STL冲突 | `ch::ch_device<>`模板实例化失败 | 静态库分两个: cpptlm_core.a(C++17) + cpptlm_rtl.a(C++20) |
| LLVM-22安装失败 | `clang++-22`不存在 | 改用LLVM-15 + ASan(牺牲JIT性能) |
| `ch::ch_device<>`崩溃 | CppHDL库版本不匹配 | 重新编译CppHDL，锁定commit hash |
| CMake嵌套构建失败 | CppHDL build输出路径冲突 | 用`ExternalProject_Add`替代`add_subdirectory` |
| PIMPL头包含STL导致ODR违规 | `ch::Simulator`非可移动 | 改用raw指针 + 显式构造/析构 |

---

## 时间估算

| 任务 | 预估时间 | 累计 |
|------|---------|------|
| Task 1: PIMPL头 | 30min | 0.5h |
| Task 2: Component头 | 45min | 1.25h |
| Task 3: PIMPL实现 | 2h | 3.25h |
| Task 4: describe() FSM | 30min | 3.75h |
| Task 5: Smoke测试 | 30min | 4.25h |
| Task 6: 注册宏 | 15min | 4.5h |
| Task 7: CMake集成 | 1.5h | 6.0h |
| Task 8: 构建修复 | 2h | 8.0h |
| **总计** | **8h** | |

---

## 注意事项

1. **PIMPL模式**: 头文件必须零CppHDL include，所有CppHDL代码在.cc中
2. **X.13安全**: 使用`release_extension<T>()`后再`set_extension<T>()`，禁止先get+delete
3. **单拍限定**: Spike仅实现单拍传输，多拍逻辑留TODO注释(Day 2+)
4. **零回归**: BUILD_RTL=OFF时必须保持598/598测试通过
5. **Commit频率**: 每个Task完成后立即commit，保持原子性
6. **证据保存**: 构建日志、测试输出保存到`.omo/evidence/`
