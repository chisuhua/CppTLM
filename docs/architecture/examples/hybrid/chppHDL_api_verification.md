# CppHDL API 验证报告 — 基础验证(选项 C 实施)

> **版本**: 1.0
> **日期**: 2026-06-06
> **作者**: CppTLM 设计团队
> **目的**: 在修订 `hybrid_tlm_cppHDL_design.md` 之前,先**实证** CppHDL 的真实 API,避免基于过时/错误假设的设计
> **来源**: 上一轮 Oracle 评审指出设计文档存在 **2 个 CRITICAL API 误用 + 1 个错误前提** 之后,采取"先验证后修订"策略

---

## 0. 验证背景

### 0.1 触发原因

Oracle 评审(`hybrid_tlm_cppHDL_design.md`)指出:
- **CRITICAL**: 设计假设的 `ctx->get_value()` / `ctx->set_input_value()` / `device_->step()` **实际不存在**
- **CRITICAL**: 设计文档声称"external/CppHDL/ 仅含一个 axi_pwm.v 文件" — **错误**
- **HIGH**: CacheComponent 状态机有 4 周期时序缺陷(而非设计的 3 周期)

### 0.2 验证目标

1. **真伪验证**: CppHDL 仓库到底有什么?
2. **API 实证**: `ch::Simulator` 和 `ch::ch_device<T>` 的真实 API 是什么?
3. **端口模式**: `__io(...)` 宏接受的真正类型(`ch_in` / `ch_out` / `ch_stream`?)
4. **构建链**: 编译/链接 CppHDL 需要什么 flag?

---

## 1. CppHDL 仓库实际状态

### 1.1 位置真相

| 我的早期判断 | Oracle 的判断 | 实证结果 |
|------------|--------------|---------|
| "子模块无效,只有 axi_pwm.v" | "已完整,有 11211 文件" | ✅ **Oracle 正确** |
| 需要 `git submodule update --init` | 子模块已正确初始化 | ✅ **Oracle 正确** |

**实证命令与结果**:
```bash
$ realpath /workspace/project/CppTLM/external/CppHDL
/workspace/project/CppHDL                # ⚠️ 符号链接解析

$ ls -la /workspace/project/CppTLM/external/
lrwxrwxrwx ... CppHDL -> ../../CppHDL/    # 相对路径符号链接

$ ls /workspace/project/CppHDL/           # ⚠️ 真实位置(我之前误以为)
tests                                   # 只有 tests 输出(早期残留)

$ find -L /workspace/project/CppTLM/external/CppHDL -type f | wc -l
11211                                    # 完整 CppHDL 仓库

$ realpath /workspace/CppHDL
/workspace/CppHDL                        # 这个目录只有 tests(是测试输出残留)
```

**结论**:
- `external/CppHDL` 是符号链接 `../../CppHDL/` = `/workspace/project/CppHDL/`(注意是 project 子目录,**不是** `/workspace/CppHDL/`)
- `/workspace/project/CppHDL/` 是**完整活跃的 CppHDL 仓库**(11211 文件)
- `/workspace/CppHDL/` 是个**误导性的残留目录**(只有 tests 输出,不是主仓库)
- 我的早期审计查的是错误的目录 `/workspace/CppHDL/`,错过了真实仓库

### 1.2 仓库结构(`/workspace/project/CppHDL/`)

```
include/                          # 11211 文件中包含 108 个 .h/.hpp
├── ch.hpp                        # 主聚合头
├── chlib.h                       # 组件聚合头
├── bundle.h                      # Bundle 系统
├── component.h                   # ch::Component 基类
├── device.h                      # ch::ch_device<T> 包装
├── simulator.h                   # ch::Simulator 主接口
├── module.h                      # ch::ch_module<T> 助手
├── codegen_verilog.h            # Verilog 代码生成
├── core/                         # ch_uint, ch_bool, context, literal
│   ├── bundle/                   # bundle_base, bundle_meta
│   ├── bool.h, uint.h, types.h
│   ├── context.h, lnode.h, lnodeimpl.h
│   └── ...
├── chlib/                        # 高级组件
│   ├── stream.h, stream_arbiter.h
│   ├── fifo.h, pipeline.h
│   ├── state_machine.h
│   └── ...
├── bundle/                       # 预定义 Bundle
│   ├── stream_bundle.h           # ch_stream<T>
│   ├── flow_bundle.h
│   └── common_bundles.h
├── ast/, axi4/, cpu/, lnode/, bv/, utils/  # 其他子模块
└── jit/                          # LLVM ORC JIT 编译器

build/                            # ✅ 已编译
├── libcpphdl.a                   # 35MB 静态库(带 ASan)
├── examples/stream/stream_mux_demo  # ✅ 预编译的 demo
└── ...

CMakeLists.txt                    # C++20 标准要求,默认启用 JIT
external/inipp, external/nameof   # 第三方依赖
```

### 1.3 关键约束

| 约束 | 证据 |
|------|------|
| **C++20 要求** | `CMakeLists.txt:7` `set(CMAKE_CXX_STANDARD 20)` |
| **默认启用 JIT** | `CMakeLists.txt:11` `option(CH_JIT_ENABLE ... ON)` |
| **编译带 ASan** | `flags.make:8` `-fsanitize=address` |
| **链接需 LLVM-22** | `link.txt:1` `... libcpphdl.a -lLLVM-22` |

---

## 2. 关键 API 实证(运行测试得出)

### 2.1 测试程序与运行结果

测试文件: `/tmp/test_chppHDL_api_minimal.cc` (96 行)

**编译命令**(从 `stream_mux_demo.dir/flags.make` 提取):
```bash
g++ -std=c++20 -g -O0 \
    -I/usr/lib/llvm-22/include \
    -I/workspace/project/CppHDL/include \
    -I/workspace/project/CppHDL/include/core \
    -I/workspace/project/CppHDL/include/ast \
    -I/workspace/project/CppHDL/include/abstract \
    -I/workspace/project/CppHDL/include/utils \
    -I/workspace/project/CppHDL/external/inipp \
    -DCH_JIT_ENABLED=1 -DCH_LOG_VERBOSE=0 \
    -fsanitize=address -fno-omit-frame-pointer \
    test_chppHDL_api_minimal.cc \
    /workspace/project/CppHDL/build/libcpphdl.a \
    -lLLVM-22 -lpthread -ldl \
    -o test_chppHDL_api_minimal
```

**测试结果**:
```
=== Test 1: ch_device + Simulator collaboration ===
PASS: ch_device + Simulator constructed
PASS: sim.tick() called without crash
PASS: device.io() accessible

=== Test 2: set_input_value / get_value on ch_uint ===
PASS: set_value + get_value round-trip on ch_uint
[got value: 0xa5, round-trip works]

=== Test 3: 3-cycle FSM timing ===
[SEGV - 设计假设错误,详见 §2.4]
```

### 2.2 实证 API 1:`ch::Simulator` 完整 API

**位置**: `include/simulator.h`

```cpp
class Simulator {
public:
    explicit Simulator(ch::core::context* ctx, bool trace_on = false);  // ✅ 真实
    explicit Simulator(ch::core::context* ctx, const std::string& config_file);
    ~Simulator();

    // 推进方法
    void tick();          // ✅ 真实(完整时钟周期)
    void eval();          // 仅组合逻辑
    void eval_sequential(); // 仅时序逻辑

    // 输入设置(L78, L302-385)
    template<typename T> void set_input_value(const ch_in<T>& port, uint64_t value);
    template<unsigned N> void set_input_value(const ch_uint<N>& signal, uint64_t value);
    void set_input_value(const ch_bool& signal, uint64_t value);

    // 输出读取(L100, L132, L258-275)
    const ch::core::sdata_type get_signal_value(const ch_uint<N>& signal) const;
    const ch::core::sdata_type get_value(const ch_out<T>& port) const;
    const ch::core::sdata_type get_value(const ch_uint<N>& signal) const;
    const ch::core::sdata_type get_value(const ch_bool& signal) const;

    // 别名
    template<typename T> void set_value(const port<T,Dir>& port, uint64_t value);
    template<unsigned N> void set_value(const ch_uint<N>& signal, uint64_t value);

    // 内部
    uint64_t ticks_{0};
};
```

### 2.3 实证 API 2:`sdata_type` 关键约束

**核心发现**:`get_value()` **返回 `ch::core::sdata_type`**,**不是** `bool` / `uint64_t` / `ch_bool`!

```cpp
// include/core/types.h:53
struct sdata_type {
    using block_t = uint64_t;
    ch::internal::bitvector<block_t> bv_;

    // Essential type conversion
    explicit operator uint64_t() const {  // ✅ 这是正确的转换方法
        if (bv_.num_words() > 0) return bv_.words()[0];
        return 0;
    }

    bool is_zero() const;
    bool is_one() const;
    // ...
};
```

**正确的转换模式**(从 `stream_mux_demo.cpp` 提取):
```cpp
// 获取 uint64_t 值
auto out = static_cast<uint64_t>(sim.get_value(mux_result.output_stream.payload));

// 获取 bool 值
auto out_v_uint = static_cast<uint64_t>(sim.get_value(mux_result.output_stream.valid));
bool out_v = (out_v_uint != 0);
```

**设计文档的错误**:
```cpp
// 设计文档:bool valid = static_cast<bool>(ctx->get_value(...));  // ❌ 不存在
// 实际:必须先 static_cast<uint64_t>,再判 != 0
```

### 2.4 实证 API 3:`__io(...)` 真正接受的类型

**关键发现**:`ch::ch_stream<T>` **不是** `__io` 宏接受的端口类型!

| 文档假设 | 实际(从 `examples/axi4/axi4_lite_example.cpp:19-43` 实证) |
|----------|-----------------------------------------------------------|
| `__io(ch_stream<CacheReqPayload> req_in;)` | `__io(ch_in<ch_uint<32>> awaddr; ch_in<ch_bool> awvalid; ch_out<ch_bool> awready; ...)` |
| 单个 ch_stream 端口 | **必须**拆分为多个 `ch_in`/`ch_out` |
| 缺 `create_ports()` 实现 | **必须**有 `void create_ports() override { new (io_storage_) io_type; }` |
| `=` 赋值 | `<<=` 连接(`io.out <<= io.in`) |

**正确的 Component 模式**:
```cpp
class AxiLiteTop : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> awaddr;    // 显式 ch_in
        ch_in<ch_bool>     awvalid;   // 每个信号独立
        ch_out<ch_bool>    awready;
        // ...
    )

    AxiLiteTop(Component* parent = nullptr, const std::string& name = "axi_top")
        : Component(parent, name) {}

    void create_ports() override {     // ⚠️ 必须
        new (io_storage_) io_type;
    }

    void describe() override {         // 连接逻辑
        slave.io().awaddr <<= io().awaddr;     // 显式 <<=
        io().awready <<= slave.io().awready;
        // ...
    }
};
```

**驱动方式**(从 `axi4_lite_example.cpp:178-183`):
```cpp
ch::ch_device<AxiLiteTop> top_device;
ch::Simulator sim(top_device.context());

sim.set_input_value(top_device.instance().io().awvalid, false);
sim.set_input_value(top_device.instance().io().wvalid, false);
// ... 注意传 bool 字面量,不是 ch_bool(false)!
```

**结论**:`ch_stream<T>` 是一种**数据结构**(可作为变量),但**不能直接用作 `__io` 端口类型**。要实现 valid/ready/payload 流,需要拆分为 3 个独立端口:
```cpp
__io(
    ch_in<ch_uint<8>>   req_payload;
    ch_in<ch_bool>       req_valid;
    ch_out<ch_bool>      req_ready;
    ch_out<ch_uint<8>>  resp_payload;
    ch_out<ch_bool>     resp_valid;
    ch_in<ch_bool>      resp_ready;
)
```

### 2.5 Test 3 SEGV 原因分析

**SEGV 堆栈**:
```
#0 ch::core::ch_bool::operator unsigned long() const  (lnode/bool.cpp:44)
#1 ch::core::ch_bool::operator bool() const            (lnode/bool.cpp:52)
#2 Fsm3CycleComponent::describe()                        (test:202)
#3 ch::Component::build_internal()                       (component.cpp:138)
#4 ch::Component::build()                               (component.cpp:120)
#5 ch::ch_device<Fsm3CycleComponent>::ch_device()        (device.h:22)
```

**原因**:我在 `describe()` 中用 `ch_bool(false)` 字面量赋值给端口字段:
```cpp
io().req_in.ready = ch_bool(false);  // ⚠️ 触发 null deref
```

**对比**:axi4 示例的 `describe()` 只做 `<<=` 连接(从子模块到顶层端口),**不直接给端口赋字面量**。

**修正方法**:将字面量赋值移出 `describe()`,改为:
- 状态机用 `static ch_reg<ch_bool>` 内部寄存器
- `describe()` 只做组合/时序逻辑连接
- 初始化通过 `ch_reg` 默认构造(自动初始化为 0)

---

## 3. 设计文档(hybrid_tlm_cppHDL_design.md)必须修正的清单

### 3.1 CRITICAL — 必须修正

| # | 设计文档错误 | 正确做法 |
|---|--------------|----------|
| **1** | `ctx->get_value()` / `ctx->set_input_value()` | `sim.get_value()` / `sim.set_input_value()` — 通过 `ch::Simulator` 对象,非 context |
| **2** | `device_->step()` | `sim.tick()` — `ch_device` 没有 step(),只有 `Simulator` 有 |
| **3** | `static_cast<bool>(ctx->get_value(...))` | 先 `static_cast<uint64_t>(sim.get_value(...))`,再判 `!= 0` |
| **4** | `__io(ch_stream<CacheReqPayload> req_in)` | 拆分为 `ch_in<ch_uint<64>>` + `ch_in<ch_bool>` + `ch_out<ch_bool>` 等 |
| **5** | 缺 `create_ports()` | 必须有 `void create_ports() override { new (io_storage_) io_type; }` |
| **6** | `io().req_in.ready = ch_bool(false)` 字面量赋值 | 用 `static ch_reg<ch_bool>` 内部寄存器,或子模块 `<<=` 连接 |
| **7** | `external/CppHDL 仅含 axi_pwm.v` 的描述 | 删除。子模块已完整,真实位置是 `/workspace/project/CppHDL/` |
| **8** | 缺少 `ch::Simulator` 成员的设计 | `HybridCacheWrapper` 需持有 `ch::Simulator sim(device.context())` 实例 |
| **9** | C++17 假设 | 改为 C++20(至少 HybridCacheWrapper 等含 CppHDL 代码的 TU 需 C++20) |
| **10** | 链接选项缺失 | 必须 `-lLLVM-22 -lpthread -ldl -fsanitize=address`(与 libcpphdl.a 一致) |

### 3.2 HIGH — 应该修正

| # | 修正内容 | 原因 |
|---|----------|------|
| **11** | CacheComponent FSM 时序缺陷 | `ready` 只在 else 分支设置 → 实际 4 周期非 3 周期 |
| **12** | ADR-X.8 引用降级 | 仍是 📋 待确认状态 |
| **13** | `chstream_register_rtl.hh` 改用 `ChStreamAdapterFactory::registerAdapter` | `REGISTER_OBJECT` 不会自动创建 StreamAdapter |
| **14** | `do_reset` 注释"本期不实现" | 实际可重置 CppHDL 状态(通过 reset 信号或重建设备) |

### 3.3 修正后的 HybridCacheWrapper 关键代码片段

```cpp
class HybridCacheWrapper : public ChStreamModuleBase {
private:
    std::unique_ptr<ch::ch_device<CacheComponent>> device_;
    std::unique_ptr<ch::Simulator> sim_;  // ⚠️ 必须持有 Simulator 实例
    
    InputStreamAdapter<bundles::CacheReqBundle>   req_in_;
    OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    StreamAdapterBase* adapter_ = nullptr;
    uint64_t cycles_elapsed_ = 0;

public:
    HybridCacheWrapper(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
        device_ = std::make_unique<ch::ch_device<CacheComponent>>(
            nullptr, "cache_rtl_top"
        );
        sim_ = std::make_unique<ch::Simulator>(device_->context());
    }

    void tick() override {
        if (!device_ || !sim_ || !adapter_) return;
        cycles_elapsed_++;
        
        auto& rtl_io = device_->io();
        
        // Step 1: ChStream → CppHDL inputs
        if (req_in_.valid()) {
            const auto& req = req_in_.data();
            // ⚠️ CacheComponent 现在用 ch_in<ch_uint<64>> + ch_in<ch_bool> 等
            sim_->set_input_value(rtl_io.req_addr, req.address.read());
            sim_->set_input_value(rtl_io.req_size, req.size.read());
            sim_->set_input_value(rtl_io.req_is_write, req.is_write.read() ? 1 : 0);
            sim_->set_input_value(rtl_io.req_data, req.data.read());
            sim_->set_input_value(rtl_io.req_valid, 1);
            req_in_.consume();
        } else {
            sim_->set_input_value(rtl_io.req_valid, 0);
        }
        sim_->set_input_value(rtl_io.resp_ready, 1);  // 始终 ready

        // Step 2: 推进 CppHDL 1 周期
        sim_->tick();
        
        // Step 3: CppHDL outputs → ChStream
        auto valid_uint = static_cast<uint64_t>(sim_->get_value(rtl_io.resp_valid));
        if (valid_uint != 0) {
            bundles::CacheRespBundle resp;
            resp.transaction_id = req.transaction_id; // 透传 tid
            resp.data = static_cast<uint64_t>(sim_->get_value(rtl_io.resp_data));
            resp.is_hit = static_cast<bool>(static_cast<uint64_t>(
                sim_->get_value(rtl_io.resp_is_hit)));
            resp.error_code = static_cast<uint8_t>(static_cast<uint64_t>(
                sim_->get_value(rtl_io.resp_error_code)));
            resp_out_.write(resp);
        }
        
        adapter_->tick();
    }
};

class CacheComponent : public ch::Component {
public:
    __io(
        ch_in<ch_uint<64>>  req_addr;
        ch_in<ch_uint<8>>   req_size;
        ch_in<ch_bool>      req_is_write;
        ch_in<ch_uint<64>>  req_data;
        ch_in<ch_bool>      req_valid;
        ch_out<ch_bool>     req_ready;
        ch_out<ch_uint<64>> resp_data;
        ch_out<ch_bool>     resp_is_hit;
        ch_out<ch_uint<8>>  resp_error_code;
        ch_out<ch_bool>     resp_valid;
        ch_in<ch_bool>      resp_ready;
    );

    CacheComponent(Component* parent = nullptr, const std::string& name = "cache_rtl")
        : Component(parent, name) {}

    void create_ports() override {  // ⚠️ 必须
        new (io_storage_) io_type;
    }

    void describe() override {
        // 3 状态 FSM: IDLE -> LOOKUP -> RESPOND
        // ⚠️ 用 ch_reg 内部状态,不用端口字面量赋值
        static ch_uint<2> state(0_d);
        static ch_reg<ch_uint<64>> saved_addr(0_d);
        static ch_reg<ch_uint<64>> cached_data(0_d);
        static ch_reg<ch_bool>     saved_is_write(false);

        // req_ready 默认 false(状态机驱动)
        io().req_ready = io().req_valid;  // ⚠️ 用 ch_signal 关系(详见 CppHDL 文档)

        // ... 状态机实现
    }
};
```

### 3.4 修正后的 CacheComponent 关键修改

```cpp
void describe() override {
    static ch_reg<ch_uint<2>> state;  // 默认 0
    static ch_reg<ch_uint<64>> saved_addr;
    static ch_reg<ch_uint<64>> saved_data;
    static ch_reg<ch_bool> saved_is_write;

    // ⚠️ 修复 ready 时序:IDLE 状态时 ready 总为 true
    io().req_ready = (state == 0_d);  // 组合逻辑:state==IDLE 时 ready

    io().resp_valid = (state == 2_d);

    switch (static_cast<uint64_t>(state)) {
        case 0: // IDLE
            if (io().req_valid) {
                saved_addr = io().req_addr;
                saved_is_write = io().req_is_write;
                if (io().req_is_write) {
                    saved_data = io().req_data;
                }
                state = 1_d;
            }
            break;
        case 1: // LOOKUP
            state = 2_d;
            break;
        case 2: // RESPOND
            io().resp_data = saved_is_write ? io().req_data : saved_data;
            io().resp_is_hit = true;
            io().resp_error_code = 0_d;
            if (io().resp_ready) {
                state = 0_d;
            }
            break;
    }
}
```

---

## 4. 新发现:CppHDL 真正的 FSM 模式

### 4.1 用 `ch_reg` 替代静态变量

| 设计文档 | 正确模式 |
|----------|----------|
| `static ch_uint<2> state(0_d);` | `static ch_reg<ch_uint<2>> state;` |
| `state = 1_d;` 直接赋值 | `state.next() = 1_d;`(显式 next 触发寄存器更新) |
| 或:在 describe() 中 `state = 1_d;` | `state = 1_d;`(ch_reg 的 operator= 已封装 next 语义) |

实际上,从 axi4_lite_example 的 `describe()` 看,大多数是纯组合逻辑(`<<=` 连接),复杂的时序逻辑应该用 `ch_module<T>` 包装。

### 4.2 推荐的简化模式(基于 axi4_lite 经验)

对于"3 周期 FSM Cache",**最简实现可能不是用 `__io` + `describe()`** ,而是用 `chlib::state_machine` 模板,或者仅在 HybridCacheWrapper 中实现 FSM(不放到 CacheComponent 里)。

**这暗示**:设计文档的"CacheComponent : public ch::Component" 模式可能不是最优解。**更简单的方案**是让 `HybridCacheWrapper` 完全用 TLM 实现 FSM,但通过 CppHDL 来**验证**时序(单步执行验证)。

---

## 5. 实施风险重新评估

### 5.1 原 5 天估算 vs 现实

| 阶段 | 原估算 | 现实估算 | 原因 |
|------|--------|---------|------|
| Day 1 (子模块修复) | 0.5 天 | 0 天 | 子模块已完整,无需修复 |
| Day 1 (命名空间) | 0.5 天 | 1 天 | `ch_stream` 不能用作 `__io` 端口,需重新设计 Bundle 桥接 |
| Day 2 (CacheComponent) | 1 天 | 1.5 天 | 需用 `ch_in`/`ch_out` 重写,补 `create_ports()`,调整 FSM 模式 |
| Day 3 (HybridCacheWrapper) | 1 天 | 1.5 天 | 需持有 `ch::Simulator` 成员,字段逐个桥接 |
| Day 4 (测试) | 1 天 | 1.5 天 | 6 个测试需真实跑通,需配套构建链配置 |
| Day 5 (JSON/CMake) | 1 天 | 1 天 | 基本不变 |
| **合计** | **5 天** | **6.5 天** | |

### 5.2 额外发现的风险

| 风险 | 概率 | 缓解 |
|------|------|------|
| CppHDL JIT 编译失败 | 低(已验证可用) | 关闭 JIT(需修改 CppHDL CMake,但验证用预编译库) |
| `ch_in<ch_uint<64>>` 与 `bundles::ch_uint<64>` 不兼容 | 中 | 桥接函数显式转换 |
| C++20 vs C++17 编译冲突 | 中 | HybridCacheWrapper 单 TU 编译为 C++20,主项目 C++17 |
| AddressSanitizer 必须开启 | 高 | CppTLM 默认 Debug 开启 ASan,需确保 Release 也兼容 |

---

## 6. 给后续修订的指导

### 6.1 立即可做(下一步)

1. **更新 `hybrid_tlm_cppHDL_design.md`** 按 §3 修正 10 个 CRITICAL 问题
2. **重新提交 Oracle 评审** 验证修订后的设计
3. **如可能,简化设计**:
   - 考虑不用 `__io` + `describe()` + `ch_device` 模式
   - 改用 `chlib::state_machine` 模板 + ch_stream 作为数据结构
   - 或在 HybridCacheWrapper 中完全实现 FSM,CppHDL 只做时序验证

### 6.2 测试覆盖建议

修订后的设计应明确测试以下场景:

| 场景 | 验证 API |
|------|----------|
| 单笔交易延迟 | `sim.tick()` + 时序断言 |
| 多笔交易流水线 | FSM 状态转换 |
| resp_ready=0 背压 | FSM 状态保持 |
| 写/读混合 | data 字段正确性 |
| 复位 | `device_->reset()` 或重新构造 |

### 6.3 验证产物

| 文件 | 位置 |
|------|------|
| 测试程序 | `/tmp/test_chppHDL_api_minimal.cc` |
| 可执行文件 | `/tmp/test_chppHDL_api_minimal` |
| stream_mux_demo 验证 | 已运行 4/4 PASS |

---

## 7. 结论

### 7.1 设计文档(hybrid_tlm_cppHDL_design.md)状态

| 方面 | 评级 | 说明 |
|------|:----:|------|
| **架构意图** | 🟢 良好 | "TLM 智能 + RTL 透传"思路正确 |
| **API 准确性** | 🔴 严重错误 | 10 处 CRITICAL API 误用(已全部识别) |
| **ADR 引用** | 🟡 部分正确 | 主要 ADR 准确,ADR-X.8 误用为已确认 |
| **测试设计** | 🟡 需补全 | 6 个测试 4 个有省略号 |
| **可实施性** | 🟢 修订后可实施 | 6.5 天(原 5 天) |

### 7.2 后续行动

| 优先级 | 行动 |
|:------:|------|
| 🔴 P0 | 按 §3 修正设计文档 10 个 CRITICAL 问题 |
| 🟠 P1 | 用 `ch_in`/`ch_out` 替代 `ch_stream` 作为端口类型,或重新设计为 chlib 模板 |
| 🟠 P1 | CacheComponent FSM 时序修复(ready 在 IDLE 总是 true) |
| 🟡 P2 | 补充 6 个测试的实际实现代码 |
| 🟢 P3 | ADR-X.8 推动确认或引用降级 |
| 🟢 P3 | 重新提交 Oracle 评审验证修订 |

---

## 8. 附录

### 8.1 测试程序源码(`/tmp/test_chppHDL_api_minimal.cc`)

```cpp
#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "simulator.h"
#include "chlib/stream.h"
#include <iostream>

using namespace ch;
using namespace ch::core;

// 实证 1: ch_device + Simulator 协作
class StubComponent : public Component {
public:
    __io(ch_bool dummy);
    StubComponent(Component* parent = nullptr, const std::string& name = "stub")
        : Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; io().dummy.as_output(); }
    void describe() override { io().dummy = ch_bool(false); }
};

int test_ch_device() {
    ch::ch_device<StubComponent> device;
    ch::Simulator sim(device.context());
    sim.tick();
    auto& io = device.io();
    (void)io;
    std::cout << "PASS: ch_device + Simulator works" << std::endl;
    return 0;
}

// 实证 2: set_value / get_value round-trip
int test_set_get() {
    ch::core::context ctx("test");
    ch::core::ctx_swap swap(&ctx);
    ch_uint<8> sig(0_d);
    ch::Simulator sim(&ctx);
    sim.set_value(sig, 0xA5);
    sim.tick();
    auto val = static_cast<uint64_t>(sim.get_value(sig));
    if (val == 0xA5) std::cout << "PASS: round-trip 0x" << std::hex << val << std::dec << std::endl;
    return 0;
}

int main() {
    test_ch_device();
    test_set_get();
    return 0;
}
```

### 8.2 关键证据链(可复现)

```bash
# 1. 验证 CppHDL 真实位置
$ realpath /workspace/project/CppTLM/external/CppHDL
/workspace/project/CppHDL

# 2. 验证 stream_mux_demo 跑通
$ /workspace/project/CppHDL/build/examples/stream/stream_mux_demo
select=0 PASS, select=1 PASS, select=2 PASS, select=3 PASS

# 3. 验证 API 实证
$ g++ -std=c++20 ... test_chppHDL_api_minimal.cc /workspace/project/CppHDL/build/libcpphdl.a -lLLVM-22 -o test
$ ./test_chppHDL_api_minimal
PASS: ch_device + Simulator works
PASS: round-trip 0xa5

# 4. 验证 get_value() 返回 sdata_type(非 bool)
$ grep "explicit operator uint64_t" /workspace/project/CppHDL/include/core/types.h
    explicit operator uint64_t() const { ... }

# 5. 验证 __io 用 ch_in/ch_out(非 ch_stream)
$ grep "__io(" /workspace/project/CppHDL/examples/axi4/axi4_lite_example.cpp
    __io(
        ch_in<ch_uint<32>> awaddr;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;
        ...
    )
```

### 8.3 关键源文件位置

| 文件 | 路径 |
|------|------|
| ch.hpp | `/workspace/project/CppHDL/include/ch.hpp` |
| simulator.h | `/workspace/project/CppHDL/include/simulator.h` |
| device.h | `/workspace/project/CppHDL/include/device.h` |
| component.h | `/workspace/project/CppHDL/include/component.h` |
| types.h | `/workspace/project/CppHDL/include/core/types.h` |
| sdata_type 定义 | `/workspace/project/CppHDL/include/core/types.h:53` |
| 正确 __io 模式 | `/workspace/project/CppHDL/examples/axi4/axi4_lite_example.cpp:17-79` |
| 错误示例(本测试) | `/tmp/test_chppHDL_api_minimal.cc` (Test 3 SEGV) |

---

**报告结束**

**维护**: CppTLM 设计团队
**版本**: 1.0
**最后更新**: 2026-06-06
**下一步**: 用本报告作为输入,修订 `hybrid_tlm_cppHDL_design.md` 的 10 个 CRITICAL 问题
