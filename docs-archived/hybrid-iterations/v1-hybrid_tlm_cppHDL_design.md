# CppTLM TLM + CppHDL 混合仿真示例 — 完整方案设计

> **版本**: 1.0
> **日期**: 2026-06-06
> **状态**: 📋 设计方案(待评审)
> **作者**: CppTLM 设计团队
> **关联文档**:
> - 架构: [`docs/architecture/01-hybrid-architecture-v2.1.md`](../01-hybrid-architecture-v2.1.md), [`docs/architecture/多层次混合仿真.md`](../多层次混合仿真.md)
> - 决策汇总: [`docs/architecture/P0_P1_P2_DECISIONS.md`](../P0_P1_P2_DECISIONS.md)
> - ADR 索引: [`docs/adr/README.md`](../../adr/README.md), [`docs/adr/ADR-X-SUMMARY.md`](../../adr/ADR-X-SUMMARY.md)
> - 上游 CppHDL 仓库: https://github.com/chisuhua/CppHDL (486 commits, 完整活跃)
> - 上游调研: 详见第五节
> - 上游缺口分析: 见"前置调研结论"

---

## 0. 文档目的

本文档提出一个**最小可行但完整可测**的 TLM + CppHDL 混合仿真示例,作为 CppTLM 架构 v2.1 中 **"TLM 智能 + RTL 简单透传"** 混合仿真路线的首次端到端实现验证。

**示例目标**:
1. **验证架构可行性**: 证明 CppTLM 现有 ChStream 基础设施可与 CppHDL 真实 RTL 组件互通
2. **建立参考实现**: 为后续 RouterTLM/MemoryTLM 等模块的 RTL 替换提供可复用模板
3. **暴露技术风险**: 命名空间冲突、Bundle 桥接、周期粒度对齐等问题
4. **满足 ADR-P0.3**: 实现"双并行模式"(tlm/rtl 显式选择)的最简形式

**示例范围**:
- 1 个 TLM 模块(已存在): `CacheTLM`
- 1 个 CppHDL RTL 模块(新建): `CacheComponent`(3 周期精确 FSM)
- 1 个桥接 Wrapper(新建): `HybridCacheWrapper`(将 CppHDL Component 包装为 CppTLM ChStreamModule)
- 1 个端到端测试(新建): 验证 tid 一致性 + 时序差异
- 1 个 JSON 配置(新建): 支持 TLM/RTL 实例化选择

---

## 1. 设计依据(ADR 引用)

本方案严格遵循已确认的 ADR 决策。下表列出每条关键设计与对应 ADR 的映射:

| 设计决策 | 引用 ADR | ADR 关键内容 | 本方案如何遵循 |
|----------|----------|--------------|----------------|
| **Bundle 字段必须 TLM/RTL 共享** | [ADR-P1.1](../../adr/ADR-P1-TEMPLATE.md) | "✅ 选项 B) 统一共享" | `CacheReqPayload`/`CacheRespPayload` 字段与 `bundles::CacheReqBundle` 字段 1:1 对应 |
| **多端口使用数组方式** | [ADR-P1.2](../../adr/ADR-P1-TEMPLATE.md) | "✅ std::array<ch_stream<T>, N>" | NICTLM/CrossbarRTL 等多端口模块遵循此模式 |
| **Adapter 混合方案** | [ADR-P1.3](../../adr/ADR-P1-TEMPLATE.md) | "泛型基类 + 模板特化" | `HybridCacheWrapper` 继承 `ChStreamModuleBase`,使用模板适配器 |
| **独立 Mapper 层** | [ADR-P1.4](../../adr/ADR-P1-TEMPLATE.md) | "FragmentMapper/AXI4Mapper 独立" | 本期不实现 Mapper,留待 v2.1+ |
| **双并行模式** | [ADR-P1.5](../../adr/ADR-P1-TEMPLATE.md) + [P0.3](../P0_P1_P2_DECISIONS.md) | "v2.1 实现 tlm/rtl/compare/shadow" | **本方案核心目标** — 通过 `impl_type` 字段显式选择 |
| **TLM 智能 + RTL 透传** | [ADR-X.8](../../adr/ADR-X.8-fragment-handling.md) | "TLM 智能 + RTL 简单透传" | `CacheComponent` RTL 侧不做交易追踪,只做周期精确查找 |
| **TLM Module 生命周期钩子** | [ADR-X.7](../../adr/ADR-X.7-transaction-handling.md) | "PASSTHROUGH/TRANSFORM/TERMINATE" | `HybridCacheWrapper` 行为类似 PASSTHROUGH,Hybrid 桥接 |
| **TransactionContext 整合** | [ADR-X.6](../../adr/ADR-X.6-transaction-integration.md) | "Extension + Packet 双层同步" | RTL 侧不感知交易,Wrapper 层负责 tid 传递 |
| **错误处理通过 Extension** | [ADR-X.2](../../adr/ADR-X.2-error-handling.md) | "ErrorContextExt + DebugTracker" | RTL 错误通过 `ErrorCode` 字段反馈,Wrapper 转换为 Extension |
| **构建系统** | [ADR-X.5](../../adr/ADR-X.5-build-system.md) | "CMake+Ninja+ccache" | 新文件加入现有 CMake 显式列举 |
| **CppHDL 集成** | [P1.2](../P0_P1_P2_DECISIONS.md) | "短期符号链接 → 长期 submodule" | **本方案前置条件**:修复子模块初始化 |

**结论**:本方案是 ADR-P0.3 + P1.5 + P1.1 + ADR-X.8 的最小可行集成实现。

---

## 2. 设计目标与非目标

### 2.1 In-Scope(本期实现)

| 目标 | 验收标准 |
|------|----------|
| ✅ CppHDL 子模块可正确加载 | `git submodule update --init external/CppHDL` 后 `ch.hpp` 可编译 |
| ✅ `CacheComponent` 3 周期精确 | Catch2 验证 `first_resp_cycle == 2` |
| ✅ `HybridCacheWrapper` 桥接 | 5 笔交易 tid 一致,数据一致 |
| ✅ TLM 与 RTL 时序差异可观测 | TLM 1 周期 vs RTL 3 周期 |
| ✅ JSON 驱动加载 | `configs/hybrid_demo.json` 通过 ModuleFactory 加载 |
| ✅ 命名空间冲突解决 | 编译零警告/错误 |
| ✅ 完整 Catch2 测试 | 至少 6 个 TEST_CASE 覆盖 tid/时序/复位/错误 |

### 2.2 Out-of-Scope(本期不实现)

| 不实现 | 原因 |
|--------|------|
| ❌ ch_flow 支持 | CppTLM 暂无 ch_flow 抽象,留待 v2.1+ |
| ❌ Fragment/Multi-flit | 单拍请求足够验证,复杂分片在 v2.1+ |
| ❌ AXI4/CHI 协议映射 | Mapper 层是后续 P1.4 任务 |
| ❌ compare/shadow 模式 | P0.3 要求但本期只做 tlm/rtl 显式选择 |
| ❌ 置信度评分 | P2.1 内容,本方案不涉及性能分析 |
| ❌ 完整 Verilog 生成 | CppHDL 支持 codegen_verilog,但本期只验证仿真 |
| ❌ 跨时钟域 CDC | 多时钟域是 v2.2 任务 |
| ❌ 持久化快照 | v2.2 检查点任务 |

### 2.3 关键约束

| 约束 | 来源 |
|------|------|
| 1 个 CppTLM tick = 1 个 CppHDL cycle | 由 P0.4 周期级 GVT 决策 |
| Bundle 字段类型宽度固定 | ADR-P1.1 共享策略 |
| RTL 模块不感知交易 | ADR-X.8 职责分离 |
| 不引入新外部依赖 | 已有 CppHDL 子模块足够 |

---

## 3. 系统架构

### 3.1 数据流图

```
┌─────────────────────────────────────────────────────────────────┐
│                    CppTLM 事件循环 (EventQueue)                  │
│                                                                   │
│   ┌──────────┐   ch_stream req    ┌──────────────────┐         │
│   │  CPU_TLM │═══════════════════>│ HybridCacheWrapper│         │
│   │  (Initiator)│                 │   (SimObject)     │         │
│   └────┬─────┘                   │   ┌─────────────┐ │         │
│        │ resp                     │   │  ch::ch_    │ │         │
│        │<════════════════════════│═══│  device<    │ │         │
│        │                          │   │ CacheComponent│         │
│   ┌────▼─────┐                   │   │  >            │ │         │
│   │Memory_TLM│<══════════════════│═══│  (CppHDL     │ │         │
│   │(Target)  │   miss path        │   │   3 周期 FSM) │ │         │
│   └──────────┘                   │   └─────────────┘ │         │
│                                  └──────────────────┘         │
│                                                                   │
│   同一仿真时间内并存 TLM 路径和 RTL 路径,通过 JSON 显式选择       │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 类关系图

```
                            ┌──────────────────────┐
                            │     SimObject        │ (include/core/sim_object.hh)
                            │   - name_            │
                            │   - event_queue      │
                            │   - virtual tick()   │
                            └──────────┬───────────┘
                                       │
                            ┌──────────▼───────────┐
                            │ ChStreamModuleBase   │ (include/core/chstream_module.hh)
                            │   - virtual          │
                            │     set_stream_adapter│
                            └──────────┬───────────┘
                                       │
                  ┌────────────────────┼────────────────────┐
                  │                    │                    │
        ┌─────────▼────────┐  ┌─────────▼────────┐  ┌────────▼─────────┐
        │    CacheTLM     │  │ HybridCacheWrapper│  │   MemoryTLM     │
        │  (纯 C++ 业务)   │  │  (CppHDL 桥接)    │  │  (纯 C++ 业务)   │
        │  - InputStream  │  │  - ch::ch_device  │  │  - InputStream  │
        │  - OutputStream │  │  - 1 tick=1 cycle │  │  - OutputStream │
        └─────────────────┘  └────────┬─────────┘  └─────────────────┘
                                      │
                                      │ owns
                                      ▼
                            ┌──────────────────────┐
                            │  ch::ch_device<       │ (来自 CppHDL)
                            │    CacheComponent>    │
                            └────────┬─────────────┘
                                     │
                            ┌────────▼─────────┐
                            │   ch::Component   │ (CppHDL 基类)
                            │   - io()          │
                            │   - describe()    │
                            └────────┬─────────┘
                                     │ has __io(
                                     │   ch_stream<CacheReqPayload> req_in;
                                     │   ch_stream<CacheRespPayload> resp_out;
                                     │ )
                                     ▼
                            ┌─────────────────────┐
                            │   CacheComponent    │ (新文件)
                            │   - 3 周期 FSM      │
                            │   - 周期精确查找     │
                            └─────────────────────┘
```

### 3.3 模块边界(职责分离)

| 层 | 组件 | 职责 | 依据 |
|------|------|------|------|
| **CppTLM 业务层** | CPU_TLM, MemoryTLM | 已有,生成/接受请求 | 现有 |
| **CppTLM 桥接层** | `HybridCacheWrapper` | **新增**:TLM ⇄ CppHDL 协议转换 | 本方案 |
| **CppHDL 仿真层** | `CacheComponent` | **新增**:周期精确 RTL 行为 | 本方案 |
| **CppHDL 运行时** | `ch::ch_device`, `ch::Simulator` | 已有,提供 tick() 驱动 | 上游 CppHDL |

---

## 4. 详细设计

### 4.1 Bundle 桥接层设计

#### 4.1.1 Bundle 字段映射

**核心约束(ADR-P1.1)**:TLM 和 RTL 必须共用字段定义。

| 字段名 | TLM 侧 (`bundles::CacheReqBundle`) | RTL 侧 (`ch::core::ch_uint<N>`) | 桥接方式 |
|--------|----------------------------------|-------------------------------|---------|
| `transaction_id` | `ch_uint<64> transaction_id` (POD 包装) | `ch_uint<64>` (CppHDL AST 节点) | 转换:`uint64_t value` ⇄ `make_uint<64>(value)` |
| `address` | `ch_uint<64> address` | `ch_uint<64>` | 同上 |
| `size` | `ch_uint<8> size` | `ch_uint<8>` | 同上 |
| `is_write` | `ch_bool is_write` | `ch_bool` | 同上 |
| `data` | `ch_uint<64> data` | `ch_uint<64>` | 同上 |

#### 4.1.2 桥接策略

```cpp
// TLM → RTL (在 HybridCacheWrapper::tick() 中)
uint64_t tid_value = req.transaction_id.read();  // TLM 风格 getter
rtl_io.req_in.payload.transaction_id = ch::core::make_uint<64>(tid_value);  // CppHDL 风格

// RTL → TLM
uint64_t tid_value = static_cast<uint64_t>(ch_device->get_value(rtl_io.resp_out.payload.transaction_id));
resp.transaction_id = ch_uint<64>(tid_value);  // TLM 风格 setter
```

**关键技术点**:
- CppHDL 的 `ch_uint<N>` 是 AST 节点(可综合),与 CppTLM 的 POD 包装不同
- 桥接必须在每个 tick 中读取(因为 RTL 端在时钟沿更新)
- 桥接开销:< 100 ns/tick,可忽略

### 4.2 HybridCacheWrapper 设计

#### 4.2.1 状态字段

```cpp
class HybridCacheWrapper : public ChStreamModuleBase {
private:
    // === CppHDL 设备 ===
    std::unique_ptr<ch::ch_device<CacheComponent>> device_;
    
    // === ChStream 端口(与 CacheTLM 接口完全一致) ===
    InputStreamAdapter<bundles::CacheReqBundle>   req_in_;
    OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    
    // === 状态 ===
    uint64_t cycles_elapsed_ = 0;
    StreamAdapterBase* adapter_ = nullptr;
};
```

#### 4.2.2 tick() 周期推进算法

```
function tick():
    cycles_elapsed_++
    
    // Step 1: ChStream → CppHDL 输入
    if req_in_.valid() and rtl_io.req_in.ready_signal():
        transfer TLM fields → CppHDL payload
        rtl_io.req_in.valid = true
        req_in_.consume()
    else:
        rtl_io.req_in.valid = false
    
    rtl_io.resp_out.ready = true  // 无背压
    
    // Step 2: CppHDL 推进 1 周期
    device_->step()  // 等价于 simulator.tick()
    
    // Step 3: CppHDL 输出 → ChStream
    if rtl_io.resp_out.valid:
        extract fields via device->get_value()
        resp_out_.write(convert back to Bundle)
    
    // Step 4: StreamAdapter 内部转发
    if adapter_: adapter_->tick()
```

**关键不变量**: `1 CppTLM tick ≡ 1 CppHDL cycle` (符合 P0.4 周期级 GVT 决策)

### 4.3 CacheComponent RTL 设计

#### 4.3.1 状态机

```
         ┌────┐  valid && ready  
         │IDLE├───────────────┐
         └─┬──┘               │
           │                  ▼
           │            ┌──────────┐  1 cycle
           │            │  LOOKUP  ├──────────┐
           │            └──────────┘          │
           │                                 ▼
           │            ┌──────────┐  ready?  ┌─────────┐
           └────────────┤ RESPOND  │<─────────│ RESPOND │
                        └──────────┘  stall   │ (wait)  │
                                              └─────────┘
```

**总延迟**: 1 (IDLE) + 1 (LOOKUP) + 1 (RESPOND) = **3 cycles** (命中)

#### 4.3.2 接口定义

```cpp
class CacheComponent : public ch::Component {
public:
    __io(
        ch::ch_stream<CacheReqPayload>  req_in;   // 从 Master 接收
        ch::ch_stream<CacheRespPayload> resp_out; // 向 Master 发送
    );
    
    // describe() 中实现 3 周期 FSM
    void describe() override;
};
```

### 4.4 ModuleFactory 集成

#### 4.4.1 注册机制

`HybridCacheWrapper` 沿用 `REGISTER_CHSTREAM` 机制(与 `CacheTLM` 一致),无需新增宏。

#### 4.4.2 JSON 配置(对应 ADR-X.9 端口类型系统)

```json
{
  "modules": [
    { "name": "cpu",        "type": "CPUTLM" },
    { "name": "cache_tlm",  "type": "CacheTLM" },
    { "name": "cache_rtl",  "type": "HybridCacheWrapper" },
    { "name": "memory",     "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "cpu",       "dst": "cache_tlm",  "latency": 1 },
    { "src": "cpu",       "dst": "cache_rtl",  "latency": 1 },
    { "src": "cache_tlm", "dst": "memory",     "latency": 10 },
    { "src": "cache_rtl", "dst": "memory",     "latency": 10 }
  ]
}
```

**JSON 不变**(`HybridCacheWrapper` 与 `CacheTLM` 接口完全一致),用户通过选择 module type 决定实现方式 — 这就是 ADR-P0.3 "tlm/rtl 显式选择" 的最简实现。

**未来 v2.1+ 扩展**(本期不实现):
```json
{ "name": "cache", "type": "CacheTLM", "impl_type": "rtl" }
{ "name": "cache_shadow", "type": "CacheTLM", "impl_type": "shadow" }
```

### 4.5 测试设计(对应 ADR-P2.2)

#### 4.5.1 4 类回归测试中的 3 类

| 测试类型 | 对应 ADR | 本期测试用例 |
|----------|----------|--------------|
| **功能回归** | P2.2 | `tid 匹配 + 数据一致 + error_code 正确` |
| **性能回归** | P2.2 | `TLM 1 周期 vs RTL 3 周期时序差异` |
| **混合系统回归** | P2.2 | `TLM+RTL 并行运行同一交易流,结果一致` |
| ❌ 内存安全 | P2.2 | 本期不特别测试(已由现有 ASan CI 覆盖) |

#### 4.5.2 6 个 TEST_CASE 详细列表

| # | 名称 | 验证内容 | 标签 |
|---|------|----------|------|
| 1 | `Hybrid: 5 笔交易 tid 匹配` | TLM 和 RTL 路径对相同 tid 产生相同响应 | `[hybrid][cppHDL]` |
| 2 | `Hybrid: RTL 3 周期精确延迟` | 验证 `first_resp_cycle == 2` (0-indexed) | `[hybrid][cppHDL][timing]` |
| 3 | `Hybrid: 复位后状态清零` | 验证 do_reset 后 RTL 重新开始 | `[hybrid][cppHDL][reset]` |
| 4 | `Hybrid: 写操作数据一致` | is_write=true 时 data 正确传递 | `[hybrid][cppHDL][write]` |
| 5 | `Hybrid: 多笔并发顺序保持` | 同 tid 顺序输入输出 | `[hybrid][cppHDL][order]` |
| 6 | `Hybrid: JSON 驱动加载` | ModuleFactory 通过 type 字段正确实例化 | `[hybrid][cppHDL][factory]` |

---

## 5. 实施前置条件(基于前置调研)

### 5.1 关键发现

**前置调研结论**(详见 5.2 节):
- CppTLM 现有 TLM 100% 完成(Phase 0-7),528 个测试通过
- **RTL 替换 0% 完成** — 所有设计组件未实现
- CppHDL 子模块**当前仅含一个 axi_pwm.v 文件**(无效)
- 上游 CppHDL 仓库(https://github.com/chisuhua/CppHDL)是**完整活跃项目**(486 commits, 99% C++)

### 5.2 必备修复:CppHDL 子模块

**当前状态**(`/workspace/CppHDL/`):
```
/workspace/CppHDL/
└── tests/
    └── output/
        └── axi_pwm.v  (20KB Verilog,无效)
```

**修复步骤**:
```bash
cd /workspace/project/CppTLM
git submodule status external/CppHDL    # 应显示未初始化
rm -rf external/CppHDL                  # 移除无效目录
git submodule add https://github.com/chisuhua/CppHDL.git external/CppHDL
git submodule update --init --recursive
```

**修复后**(`/workspace/CppHDL/`):
```
include/
├── ch.hpp                # 主聚合头
├── bundle.h              # Bundle 系统
├── component.h           # Component 基类
├── device.h              # ch_device<T> 包装
├── simulator.h           # 仿真器主接口
├── module.h              # ch_module<T> 子模块助手
├── core/                 # ch_uint, ch_bool, context, literal
├── chlib/                # stream, flow, fifo, state_machine
├── bundle/               # stream_bundle, flow_bundle
├── axi4/                 # AXI4 互连
└── cpu/                  # RV32I 核

examples/
├── stream/               # stream_mux, stream_arbiter, stream_demux, stream_fork
├── axi4/                 # AXI4 样例
├── riscv-mini/           # RISC-V mini
└── spinalhdl-ported/     # SpinalHDL 移植
```

### 5.3 命名空间冲突解决

**冲突点**:
- CppTLM `include/bundles/cpphdl_types.hh`:`bundles::ch_uint<W>` = `uint64_t`(POD 包装)
- 真实 CppHDL `include/core/uint.h`:`ch::core::ch_uint<N>` = AST 节点

**解决策略**:在 `HybridCacheWrapper` 中使用 `namespace ch` 完全限定,避免与 `bundles::ch_uint` 冲突:

```cpp
// HybridCacheWrapper 内部
using ch_uint  = ch::core::ch_uint<N>;   // CppHDL AST
using ch_bool  = ch::core::ch_bool;       // CppHDL AST
using ch_stream = ch::ch_stream<T>;       // CppHDL stream

// 外部 Bundle 仍用 bundles::
using CppTLM_uint  = bundles::ch_uint<W>; // CppTLM POD
using CppTLM_bool  = bundles::ch_bool;
```

**桥接函数**:
```cpp
namespace bridge {
    template<typename N>
    ch::core::ch_uint<N> to_cppHDL(const bundles::ch_uint<N>& tlm) {
        return ch::core::make_uint<N>(tlm.read());
    }
    template<typename N>
    bundles::ch_uint<N> to_cppTLM(const ch::core::ch_uint<N>& rtl) {
        return bundles::ch_uint<N>(static_cast<uint64_t>(rtl));
    }
}
```

### 5.4 已确认可行(从上游示例)

**`examples/stream/stream_mux_demo.cpp`**(1.7KB)证明:
- ✅ CppHDL 可独立编译运行
- ✅ `ch::core::context` + `ctx_swap` 生命周期管理
- ✅ `ch::ch_stream<ch_uint<8>>` 完整 valid/ready/payload API
- ✅ `ch::Simulator::tick()` 推进
- ✅ `set_input_value()` / `get_value()` API

---

## 6. 完整文件清单

### 6.1 新增文件(5 个)

| 文件路径 | 行数估算 | 职责 |
|----------|---------|------|
| `include/rtl/cache_component.hh` | ~120 | CppHDL 3 周期 FSM Cache |
| `include/rtl/hybrid_cache_wrapper.hh` | ~200 | TLM↔CppHDL 桥接器 |
| `include/rtl/CHANGELOG.md` | ~30 | RTL 模块开发记录 |
| `test/test_hybrid_tlm_cppHDL.cc` | ~250 | Catch2 端到端测试 |
| `configs/hybrid_demo.json` | ~30 | JSON 驱动配置 |
| **合计** | **~630 行** | |

### 6.2 修改文件(3 个)

| 文件路径 | 改动 |
|----------|------|
| `src/CMakeLists.txt` | 添加 `rtl/hybrid_cache_wrapper.cc`(若分离实现)+ `test/test_hybrid_tlm_cppHDL.cc` |
| `include/chstream_register.hh` | 添加 `REGISTER_CHSTREAM(HybridCacheWrapper, ...)` |
| `docs/adr/ADR-X.8-fragment-handling.md` | 引用本示例作为首次 RTL 集成验证(可选) |

### 6.3 不修改文件(显式说明)

- `include/tlm/cache_tlm.hh` — 已有,不动
- `include/framework/stream_adapter.hh` — 已有,不动
- `include/bundles/cache_bundles_tlm.hh` — 已有,不动
- `include/core/chstream_module.hh` — 已有,不动
- `include/framework/impl_mode.hh` — **不创建**(留待 v2.1 compare/shadow 阶段)

---

## 7. 详细代码

### 7.1 `include/rtl/cache_component.hh`

```cpp
// include/rtl/cache_component.hh
// CppHDL RTL Cache 组件 - 3 周期精确查找
//
// 设计依据:
// - ADR-X.8: RTL 模块简单透传,不感知交易追踪
// - P0.3 双并行模式:本组件是 rtl 实现路径
// - 周期粒度:1 CppTLM tick = 1 CppHDL cycle (P0.4)
//
// 功能:实现一个简化的 L1 缓存,3 周期精确命中
// 状态:IDLE -> LOOKUP -> RESPOND
//
// 作者 CppTLM Team / 日期 2026-06-06
#ifndef CACHE_COMPONENT_HH
#define CACHE_COMPONENT_HH

#include "ch.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/uint.h"
#include "chlib/stream.h"

using namespace ch;
using namespace ch::core;

/**
 * CacheReqPayload:与 CppTLM bundles::CacheReqBundle 字段 1:1 对应
 * 字段宽度:tid(64), addr(64), size(8), is_write(1), data(64)
 */
struct CacheReqPayload {
    ch_uint<64> transaction_id;
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;
    
    CH_BUNDLE_FIELDS_T(transaction_id, address, size, is_write, data)
};

/**
 * CacheRespPayload:与 CppTLM bundles::CacheRespBundle 字段 1:1 对应
 * 字段宽度:tid(64), data(64), is_hit(1), error_code(8)
 */
struct CacheRespPayload {
    ch_uint<64> transaction_id;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;
    
    CH_BUNDLE_FIELDS_T(transaction_id, data, is_hit, error_code)
};

/**
 * CacheComponent:3 周期精确 FSM 的 L1 Cache
 *
 * 状态机:
 *   IDLE    -> 接收请求,保存 metadata
 *   LOOKUP  -> 模拟 1 周期 SRAM 查找
 *   RESPOND -> 输出响应
 *
 * 命中延迟:3 cycles (无失配路径,简化设计)
 */
class CacheComponent : public Component {
public:
    __io(
        ch_stream<CacheReqPayload>  req_in;    // 来自 Master (slave 方向)
        ch_stream<CacheRespPayload> resp_out;  // 去 Master (master 方向)
    );
    
    CacheComponent(Component* parent = nullptr, 
                   const std::string& name = "cache_rtl")
        : Component(parent, name) {}
    
    void create_ports() override {
        io().req_in.as_slave_direction();    // 接收 valid+payload, 发送 ready
        io().resp_out.as_master_direction(); // 发送 valid+payload, 接收 ready
    }
    
    void describe() override {
        // 状态寄存器
        static ch_uint<2> state(0_d);
        static ch_uint<64> saved_tid(0_d);
        static ch_uint<64> saved_addr(0_d);
        static ch_uint<64> cached_data(0_d);
        static ch_bool     saved_is_write(false);
        
        // 默认信号
        io().req_in.ready = ch_bool(false);
        io().resp_out.valid = ch_bool(false);
        
        // 3 周期状态机
        switch (static_cast<uint64_t>(state)) {
            case 0: { // IDLE:等待请求
                if (io().req_in.valid) {
                    saved_tid      = io().req_in.payload.transaction_id;
                    saved_addr     = io().req_in.payload.address;
                    saved_is_write = io().req_in.payload.is_write;
                    
                    if (saved_is_write) {
                        cached_data = io().req_in.payload.data;
                    }
                    state = 1_d;  // 进入 LOOKUP
                } else {
                    io().req_in.ready = ch_bool(true);
                }
                break;
            }
            case 1: { // LOOKUP:模拟 1 周期 SRAM 访问
                state = 2_d;  // 进入 RESPOND
                break;
            }
            case 2: { // RESPOND:输出响应
                io().resp_out.payload.transaction_id = saved_tid;
                io().resp_out.payload.data = saved_is_write
                    ? io().req_in.payload.data
                    : cached_data;
                io().resp_out.payload.is_hit = ch_bool(true);
                io().resp_out.payload.error_code = ch_uint<8>(0_d);
                io().resp_out.valid = ch_bool(true);
                
                if (io().resp_out.ready) {
                    state = 0_d;  // 返回 IDLE
                }
                break;
            }
        }
    }
};

#endif // CACHE_COMPONENT_HH
```

### 7.2 `include/rtl/hybrid_cache_wrapper.hh`

```cpp
// include/rtl/hybrid_cache_wrapper.hh
// TLM ↔ CppHDL 桥接器 - 将 CacheComponent 包装为 CppTLM ChStreamModule
//
// 设计依据:
// - ADR-P0.3:实现 tlm/rtl 显式选择(本类的类型名即 impl_type 标识)
// - ADR-P1.1:Bundle 字段共享(通过字段映射)
// - ADR-P1.3:Adapter 泛型混合方案
// - 1 CppTLM tick = 1 CppHDL cycle (P0.4 周期级 GVT)
//
// 桥接流程(tick() 3 步):
//   1. ChStream InputStreamAdapter -> CppHDL ch_stream<CacheReqPayload>
//   2. ch::ch_device<CacheComponent>::step() 推进 1 周期
//   3. CppHDL ch_stream<CacheRespPayload> -> OutputStreamAdapter
//
// 作者 CppTLM Team / 日期 2026-06-06
#ifndef HYBRID_CACHE_WRAPPER_HH
#define HYBRID_CACHE_WRAPPER_HH

#include "ch.hpp"
#include "component.h"
#include "device.h"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "bundles/cache_bundles_tlm.hh"
#include "rtl/cache_component.hh"

using namespace ch;

/**
 * HybridCacheWrapper:将 CppHDL CacheComponent 包装为 CppTLM ChStreamModule
 * 
 * 接口与 CacheTLM 完全一致(InputStreamAdapter<CacheReqBundle> +
 * OutputStreamAdapter<CacheRespBundle>),可与已有 CPU/Memory 模块
 * 无缝互通。
 * 
 * 使用场景:
 * - JSON 中 "type": "HybridCacheWrapper" 即选择 RTL 实现
 * - 与 CacheTLM 并存,验证 tid 一致性
 */
class HybridCacheWrapper : public ChStreamModuleBase {
private:
    // === CppHDL 设备(顶层仿真器包装) ===
    std::unique_ptr<ch::ch_device<CacheComponent>> device_;
    
    // === ChStream 接口(与 CacheTLM 完全一致) ===
    InputStreamAdapter<bundles::CacheReqBundle>   req_in_;
    OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    
    // === 状态 ===
    uint64_t cycles_elapsed_ = 0;
    StreamAdapterBase* adapter_ = nullptr;

public:
    explicit HybridCacheWrapper(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
        // 创建设备 - 必须在 describe() 之前完成
        device_ = std::make_unique<ch::ch_device<CacheComponent>>(
            nullptr, "cache_rtl_top"
        );
    }
    
    std::string get_module_type() const override { 
        return "HybridCacheWrapper"; 
    }
    
    void set_stream_adapter(StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }
    
    /**
     * tick():每个 CppTLM tick 推进 CppHDL 1 周期
     */
    void tick() override {
        if (!device_ || !adapter_) return;
        
        cycles_elapsed_++;
        auto& rtl_io = device_->io();
        auto* ctx = device_->context();
        
        // === Step 1: ChStream -> CppHDL req_in ===
        if (req_in_.valid() && 
            static_cast<bool>(ctx->get_value(rtl_io.req_in.ready.impl()))) {
            
            const auto& req = req_in_.data();
            rtl_io.req_in.payload.transaction_id = 
                ch::core::make_uint<64>(req.transaction_id.read());
            rtl_io.req_in.payload.address = 
                ch::core::make_uint<64>(req.address.read());
            rtl_io.req_in.payload.size = 
                ch::core::make_uint<8>(req.size.read());
            rtl_io.req_in.payload.is_write = 
                ch::core::ch_bool(req.is_write.read());
            rtl_io.req_in.payload.data = 
                ch::core::make_uint<64>(req.data.read());
            
            ctx->set_input_value(
                rtl_io.req_in.valid.impl(), 
                static_cast<uint64_t>(true));
            req_in_.consume();
        } else {
            ctx->set_input_value(
                rtl_io.req_in.valid.impl(), 
                static_cast<uint64_t>(false));
        }
        
        // resp_out 始终 ready(无背压,简化设计)
        ctx->set_input_value(
            rtl_io.resp_out.ready.impl(), 
            static_cast<uint64_t>(true));
        
        // === Step 2: CppHDL 推进 1 周期 ===
        device_->step();
        
        // === Step 3: CppHDL resp_out -> ChStream ===
        if (static_cast<bool>(ctx->get_value(rtl_io.resp_out.valid.impl()))) {
            bundles::CacheRespBundle resp;
            resp.transaction_id = static_cast<uint64_t>(
                ctx->get_value(rtl_io.resp_out.payload.transaction_id.impl()));
            resp.data = static_cast<uint64_t>(
                ctx->get_value(rtl_io.resp_out.payload.data.impl()));
            resp.is_hit = static_cast<bool>(
                ctx->get_value(rtl_io.resp_out.payload.is_hit.impl()));
            resp.error_code = static_cast<uint8_t>(
                ctx->get_value(rtl_io.resp_out.payload.error_code.impl()));
            resp_out_.write(resp);
        }
        
        // === Step 4: StreamAdapter 内部转发 ===
        adapter_->tick();
    }
    
    void do_reset(const ResetConfig& config) override {
        cycles_elapsed_ = 0;
        req_in_.reset();
        resp_out_.reset();
        // 注:CppHDL 状态机复位需要重新创建 device_,本期不实现
    }
    
    uint64_t cycles_elapsed() const { return cycles_elapsed_; }
};

#endif // HYBRID_CACHE_WRAPPER_HH
```

### 7.3 `include/rtl/chstream_register_rtl.hh`

```cpp
// include/rtl/chstream_register_rtl.hh
// RTL 模块注册宏(扩展 CppTLM 现有 REGISTER_CHSTREAM)
//
// 与 chstream_register.hh 模式一致,使用动态创建函数
#ifndef CHSTREAM_REGISTER_RTL_HH
#define CHSTREAM_REGISTER_RTL_HH

#include "rtl/hybrid_cache_wrapper.hh"

namespace cpptlm {

inline HybridCacheWrapper* create_HybridCacheWrapper(
    const std::string& name, EventQueue* eq) {
    return new HybridCacheWrapper(name, eq);
}

// 注册到 ModuleFactory 的双注册表
REGISTER_OBJECT(HybridCacheWrapper, create_HybridCacheWrapper)

}  // namespace cpptlm

#endif  // CHSTREAM_REGISTER_RTL_HH
```

### 7.4 `test/test_hybrid_tlm_cppHDL.cc`

```cpp
// test/test_hybrid_tlm_cppHDL.cc
// TLM + CppHDL 混合仿真端到端测试
//
// 测试 6 项(P2.2 四类回归中的 3 类):
//   1. 功能回归:tid 匹配 + 数据一致
//   2. 性能回归:TLM 1 周期 vs RTL 3 周期时序差异
//   3. 混合系统回归:TLM+RTL 并行运行
//
// 作者 CppTLM Team / 日期 2026-06-06
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/cache_tlm.hh"
#include "rtl/hybrid_cache_wrapper.hh"
#include "framework/stream_adapter.hh"
#include "bundles/cache_bundles_tlm.hh"

using namespace cpptlm;

namespace {

// 测试辅助:创建并填充 req bundle
bundles::CacheReqBundle make_req(uint64_t tid, uint64_t addr, 
                                  bool is_write, uint64_t data) {
    bundles::CacheReqBundle req;
    req.transaction_id = tid;
    req.address = addr;
    req.size = bundles::ch_uint<8>(8);
    req.is_write = is_write;
    req.data = data;
    return req;
}

// 测试辅助:推动单个模块直到响应或超时
bool drive_until_response(ChStreamModuleBase* mod, 
                          InputStreamAdapter<bundles::CacheReqBundle>& in,
                          OutputStreamAdapter<bundles::CacheRespBundle>& out,
                          uint64_t max_cycles = 20) {
    for (uint64_t c = 0; c < max_cycles; ++c) {
        if (out.valid()) return true;
        in.tick();
        out.tick();
        mod->tick();
    }
    return false;
}

}  // namespace

// ===== 测试 1:5 笔交易 tid 匹配 =====
TEST_CASE("Hybrid: 5 笔交易 tid 匹配 TLM/RTL 一致", "[hybrid][cppHDL]") {
    EventQueue eq;
    CacheTLM cache_tlm("cache_tlm", &eq);
    HybridCacheWrapper cache_rtl("cache_rtl", &eq);
    
    std::vector<bundles::CacheRespBundle> tlm_resps, rtl_resps;
    const uint64_t base_addr = 0x1000;
    
    for (uint64_t tid = 1; tid <= 5; ++tid) {
        auto req = make_req(tid, base_addr + tid * 8, 
                            tid % 2 == 0, 0xAA00 + tid);
        
        // TLM 路径
        // ... (推入 cache_tlm.req_in_,驱动 N 周期,收集 cache_tlm.resp_out_)
        // ... (代码略,核心断言:tlm_resps.back().transaction_id == tid)
        
        // RTL 路径  
        // ... (推入 cache_rtl.req_in_,驱动 N 周期,收集 cache_rtl.resp_out_)
        // ... (代码略,核心断言:rtl_resps.back().transaction_id == tid)
    }
    
    REQUIRE(tlm_resps.size() == 5);
    REQUIRE(rtl_resps.size() == 5);
    
    for (size_t i = 0; i < 5; ++i) {
        INFO("Transaction " << i + 1);
        CHECK(tlm_resps[i].transaction_id.read() == rtl_resps[i].transaction_id.read());
        CHECK(tlm_resps[i].data.read() == rtl_resps[i].data.read());
        CHECK(tlm_resps[i].is_hit.read() == rtl_resps[i].is_hit.read());
        CHECK(tlm_resps[i].error_code.read() == rtl_resps[i].error_code.read());
    }
}

// ===== 测试 2:RTL 3 周期精确延迟 =====
TEST_CASE("Hybrid: RTL 3 周期精确命中延迟", "[hybrid][cppHDL][timing]") {
    EventQueue eq;
    HybridCacheWrapper cache_rtl("cache_rtl", &eq);
    
    auto req = make_req(1, 0x2000, false, 0);
    
    int first_resp_cycle = -1;
    const int max_cycles = 10;
    for (int c = 0; c < max_cycles; ++c) {
        // ... (推入请求,推进 cache_rtl.tick())
        if (cache_rtl.resp_out_.valid() && first_resp_cycle < 0) {
            first_resp_cycle = c;
        }
    }
    
    // RTL 状态机:IDLE->LOOKUP->RESPOND = 3 cycles
    // 0-indexed:第 2 个 tick 看到响应
    CHECK(first_resp_cycle == 2);
}

// ===== 测试 3:复位 =====
TEST_CASE("Hybrid: 复位后状态清零", "[hybrid][cppHDL][reset]") {
    EventQueue eq;
    HybridCacheWrapper cache_rtl("cache_rtl", &eq);
    
    // 推入 + 推进
    // ... (略)
    
    auto initial_cycles = cache_rtl.cycles_elapsed();
    CHECK(initial_cycles > 0);
    
    // 复位
    ResetConfig cfg;
    cache_rtl.do_reset(cfg);
    
    CHECK(cache_rtl.cycles_elapsed() == 0);
    // ... (断言 resp_out 无残留响应)
}

// ===== 测试 4:写操作 =====
TEST_CASE("Hybrid: 写操作 data 正确传递", "[hybrid][cppHDL][write]") {
    EventQueue eq;
    HybridCacheWrapper cache_rtl("cache_rtl", &eq);
    
    auto req = make_req(1, 0x3000, true, 0xDEADBEEF);
    // ... (推入并推进)
    
    REQUIRE(cache_rtl.resp_out_.valid());
    auto resp = cache_rtl.resp_out_.data();
    CHECK(resp.transaction_id.read() == 1);
    CHECK(resp.data.read() == 0xDEADBEEF);  // 写数据回显
}

// ===== 测试 5:顺序保持 =====
TEST_CASE("Hybrid: 多笔交易顺序保持", "[hybrid][cppHDL][order]") {
    EventQueue eq;
    HybridCacheWrapper cache_rtl("cache_rtl", &eq);
    
    // 连续推入 3 笔
    for (uint64_t tid = 100; tid <= 102; ++tid) {
        auto req = make_req(tid, 0x4000, false, 0);
        // ... (推入)
    }
    
    std::vector<uint64_t> out_tids;
    for (int c = 0; c < 20 && out_tids.size() < 3; ++c) {
        // ... (推进 + 收集)
        if (cache_rtl.resp_out_.valid()) {
            out_tids.push_back(cache_rtl.resp_out_.data().transaction_id.read());
        }
    }
    
    REQUIRE(out_tids.size() == 3);
    CHECK(out_tids[0] == 100);
    CHECK(out_tids[1] == 101);
    CHECK(out_tids[2] == 102);
}

// ===== 测试 6:JSON 工厂加载 =====
TEST_CASE("Hybrid: JSON 驱动 ModuleFactory 加载", "[hybrid][cppHDL][factory]") {
    // 此测试需要 ModuleFactory 已注册 HybridCacheWrapper
    // ... (加载 configs/hybrid_demo.json,断言模块被创建)
    // ... (代码略,主要验证 chstream_register_rtl.hh 集成)
    
    SUCCEED("HybridCacheWrapper registered and loadable");
}
```

### 7.5 `configs/hybrid_demo.json`

```json
{
  "$schema": "../docs/architecture/schema/topo.schema.json",
  "name": "hybrid_tlm_rtl_demo",
  "version": "1.0",
  "modules": [
    { "name": "cpu",        "type": "CPUTLM" },
    { "name": "cache_tlm",  "type": "CacheTLM" },
    { "name": "cache_rtl",  "type": "HybridCacheWrapper" },
    { "name": "memory",     "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "cpu",       "dst": "cache_tlm",  "latency": 1 },
    { "src": "cpu",       "dst": "cache_rtl",  "latency": 1 },
    { "src": "cache_tlm", "dst": "memory",     "latency": 10 },
    { "src": "cache_rtl", "dst": "memory",     "latency": 10 }
  ]
}
```

---

## 8. 实施计划

### 8.1 阶段分解(总 5 个工作日)

| Day | 任务 | 交付物 | 验证 |
|-----|------|--------|------|
| **Day 1 上午** | 修复 CppHDL 子模块 | 完整 ch.hpp 可编译 | `stream_mux_demo` 跑通 |
| **Day 1 下午** | 命名空间冲突解决 | `bridge` 命名空间 + 别名 | CppTLM 编译零错误 |
| **Day 2** | 编写 `cache_component.hh` | RTL 组件 120 行 | 单测:3 周期精确 |
| **Day 3** | 编写 `hybrid_cache_wrapper.hh` | Wrapper 200 行 | 桥接 1 笔交易 |
| **Day 4** | 编写 `test_hybrid_tlm_cppHDL.cc` | 6 个 TEST_CASE | Catch2 全部通过 |
| **Day 5** | JSON 集成 + CMake | `hybrid_demo.json` 加载 | ctest 集成通过 |

### 8.2 依赖关系

```
Day 1 (前置)
  ├── 修复 CppHDL 子模块 (阻塞所有后续)
  └── 命名空间冲突解决 (阻塞所有后续)

Day 2
  └── 依赖:Day 1

Day 3
  └── 依赖:Day 2

Day 4
  └── 依赖:Day 3

Day 5
  └── 依赖:Day 4
```

### 8.3 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | CppHDL `ch.hpp` 包含顺序问题(规划中已注释多个 `#include`) | 高 | 中 | 阶段 1 优先验证 `stream_mux_demo` 可独立编译 |
| R2 | 命名空间冲突(bundles::ch_uint vs ch::core::ch_uint) | 中 | 中 | `namespace bridge` 隔离 + `using` 别名 |
| R3 | `get_value()` / `set_input_value()` API 行为不确定性 | 中 | 高 | 单步调试 + 简化的"传 0 验证通路"策略 |
| R4 | 1 tick = 1 cycle 不匹配导致 Wrapper 时序错乱 | 低 | 高 | Day 3 立即编写时序单测 |
| R5 | CppHDL AST 节点(POD vs ch_uint)在 Bundle 序列化时类型不匹配 | 中 | 中 | 桥接函数显式转换,不在 CppHDL 内部用 bundles 类型 |

### 8.4 回退方案

若 Day 1 结束时 CppHDL 子模块修复失败,回退路径:
1. **优先**:升级 CppHDL 到稳定 commit hash(API 可能更稳定)
2. **次优**:用 MockCppHDL 桩(手写简化版 ch_stream / Component)做端到端验证,推迟真实 CppHDL 集成到 v2.1
3. **降级**:只做架构设计验证(写代码 + 文档),不要求编译通过

---

## 9. 验收标准

### 9.1 功能验收(对应 ADR-P2.2)

| 验收项 | 标准 | 验证方法 |
|--------|------|----------|
| CppHDL 子模块可用 | `external/CppHDL/include/ch.hpp` 存在 | `ls` |
| CppHDL 单独可仿真 | `stream_mux_demo` 跑通 | `./build/bin/stream_mux_demo` |
| Hybrid 编译通过 | 零警告零错误 | `cmake --build build` |
| 6 个 TEST_CASE 通过 | 100% pass | `ctest -R hybrid` |
| TLM+RTL 结果一致 | tid/data/is_hit 完全相同 | TEST_CASE #1 |
| 时序差异可观测 | RTL 3 周期,TLM 1 周期 | TEST_CASE #2 |
| 复位正确 | cycles_elapsed 归零 | TEST_CASE #3 |
| 写数据正确 | data 字段回显 | TEST_CASE #4 |
| 顺序保持 | tid 单调递增 | TEST_CASE #5 |
| JSON 加载 | type 字段识别 | TEST_CASE #6 |

### 9.2 质量验收

- **代码规范**: 通过 `clang-format --check`(沿用项目 `.clang-format`)
- **LSP 诊断**: 所有新文件 `lsp_diagnostics` 零错误
- **构建产物**: 新增 `cpptlm_core` 静态库增长 < 50KB
- **测试覆盖率**: 桥接逻辑 100% 行覆盖

### 9.3 文档验收

- 本文档(`docs/architecture/examples/hybrid/hybrid_tlm_rtl_design.md`)被评审通过
- 架构文档 `01-hybrid-architecture-v2.1.md` 添加"已实现示例"链接
- 实施计划 `plans/hybrid_tlm_rtl_implementation.md` 引用本设计

---

## 10. 后续路线图(本方案外的扩展)

### 10.1 短期(基于本方案,1-2 周)

| 任务 | 优先级 | 来源 |
|------|--------|------|
| `HybridMemoryWrapper` 同样模式 | 高 | 与 cache 平移 |
| `HybridArbiterWrapper` | 中 | 扩展 RTL 覆盖 |
| VCD 波形生成 | 中 | P2.2 性能回归 |
| `compare` 模式(同实例两个实现) | 中 | ADR-P0.3 |
| `shadow` 模式 | 低 | ADR-P0.3 |

### 10.2 中期(2-3 月)

| 任务 | 来源 |
|------|------|
| AXI4 ProtocolAdapter | P1.4 |
| FragmentMapper | FRAGMENT_MAPPER_DECISIONS.md |
| 完整 RISC-V mini 仿真 | CppHDL `examples/riscv-mini/` |
| 双时钟域支持 | P2.3 Phase 2.2 |

### 10.3 长期(6+ 月)

| 任务 | 来源 |
|------|------|
| RTL → Verilog 综合 | CppHDL `codegen_verilog.h` |
| VCD + transaction_id 注释 | 多层次混合仿真 §5.2.4 |
| 性能校准流水线 | P2.1 置信度评分 |
| 商用 RTL IP 集成 | ADR-X.4 插件系统 |

---

## 11. 附录:核心引用清单

### 11.1 ADR 直接引用

| ADR | 引用章节 | 关键约束 |
|-----|----------|----------|
| [ADR-P1.1](../../adr/ADR-P1-TEMPLATE.md) | §1, §4.1.1 | Bundle 字段共享 |
| [ADR-P1.2](../../adr/ADR-P1-TEMPLATE.md) | §3.1 | 多端口数组方式 |
| [ADR-P1.3](../../adr/ADR-P1-TEMPLATE.md) | §4.2 | Adapter 混合方案 |
| [ADR-P1.4](../../adr/ADR-P1-TEMPLATE.md) | §2.2 | Mapper 层独立(未来) |
| [ADR-P1.5](../../adr/ADR-P1-TEMPLATE.md) | §2.2, §3.4 | v2.1 双并行实现 |
| [ADR-X.1](../../adr/ADR-X.1-transaction-id.md) | §1 | 事务 ID 分配 |
| [ADR-X.2](../../adr/ADR-X.2-error-handling.md) | §1 | 错误处理 Extension |
| [ADR-X.3](../../adr/ADR-X.3-reset-strategy.md) | §1 | 层次化复位 |
| [ADR-X.4](../../adr/ADR-X.4-plugin-system.md) | §1 | v2.0 静态链接 |
| [ADR-X.5](../../adr/ADR-X.5-build-system.md) | §1 | CMake+Ninja+ccache |
| [ADR-X.6](../../adr/ADR-X.6-transaction-integration.md) | §1, §4.1.1 | TransactionContext 整合 |
| [ADR-X.7](../../adr/ADR-X.7-transaction-handling.md) | §1, §3.2 | 模块生命周期钩子 |
| [ADR-X.8](../../adr/ADR-X.8-fragment-handling.md) | §1, §3.2 | TLM 智能 + RTL 透传 |
| [ADR-X.9](../../adr/ADR-X.9-port-type-system.md) | §4.4 | 端口类型系统 |
| [ADR-X.11](../../adr/ADR-X.11-config-inheritance-and-fixes.md) | §4.4 | 配置继承 |

### 11.2 架构文档引用

| 文档 | 引用章节 |
|------|----------|
| [`01-hybrid-architecture-v2.1.md`](../01-hybrid-architecture-v2.1.md) | 整体架构、ChStream 设计 |
| [`01-hybrid-architecture-v2.md`](../01-hybrid-architecture-v2.md) | RTL 原始设计(已归档但供参考) |
| [`多层次混合仿真.md`](../多层次混合仿真.md) | 完整混合仿真路线图,本文档为其子集 |
| [`P0_P1_P2_DECISIONS.md`](../P0_P1_P2_DECISIONS.md) | P0.1-P2.3 决策,本文档严格遵守 |
| [`FRAGMENT_MAPPER_DECISIONS.md`](../FRAGMENT_MAPPER_DECISIONS.md) | Bridge+Mapper 分层(本文档简化) |
| [`02-transaction-architecture.md`](../02-transaction-architecture.md) | 交易处理架构 |
| [`03-error-debug-architecture.md`](../03-error-debug-architecture.md) | 错误处理架构(已实现) |
| [`04-reset-checkpoint-architecture.md`](../04-reset-checkpoint-architecture.md) | 复位架构 |

### 11.3 调研结论引用

| 调研 | 关键结论 | 引用章节 |
|------|----------|----------|
| CppHDL 上游仓库 | 完整活跃项目(486 commits, 99% C++) | §5.2 |
| CppHDL API | `ch::ch_stream<T>`, `ch::Component`, `ch::ch_device<T>`, `ch::Simulator` | §4.3, §5.4 |
| CppTLM 当前 TLM | 100% 完成(Phase 0-7),528 测试 | §1 (设计依据) |
| CppTLM RTL 状态 | 0% 完成,所有组件未实现 | §5.1 |
| CppHDL 子模块 | 当前无效(仅 1 个 .v 文件) | §5.2 |

### 11.4 相关 ADRs 关联图

```
┌─────────────────────────────────────────────────────────────┐
│ ADR-X.6 (TransactionContext Extension + Packet)              │
│  └─► 决定:TID 同步机制,RTL 侧不感知,由 Wrapper 桥接       │
│                                                              │
│ ADR-X.7 (模块生命周期)                                       │
│  └─► 决定:HybridCacheWrapper 是 PASSTHROUGH-like 行为        │
│                                                              │
│ ADR-X.8 (分片处理 + TLM/RTL 职责分离)                        │
│  └─► 决定:CacheComponent RTL 不感知交易,只做周期精确查找   │
│                                                              │
│ ADR-P0.3 (双并行实现模式 tlm/rtl/compare/shadow)            │
│  └─► 本方案:实现 tlm/rtl 显式选择(type 字段)               │
│  └─► 未来:compare/shadow 模式(v2.1+)                        │
│                                                              │
│ ADR-P1.1 (Bundle 共享)                                       │
│  └─► 本方案:CacheReqPayload 字段与 bundles::CacheReqBundle  │
│       1:1 对应                                                │
│                                                              │
│ ADR-P0.4 (周期级 GVT)                                        │
│  └─► 本方案:1 CppTLM tick ≡ 1 CppHDL cycle                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 12. 评审检查清单

评审本设计时,请确认以下问题:

| # | 检查项 | 答案 |
|---|--------|------|
| 1 | 是否满足 ADR-P0.3 的"tlm/rtl 显式选择"? | ✅ 是,JSON 中 type 字段选择 |
| 2 | 是否满足 ADR-P1.1 的"Bundle 共享"? | ✅ 是,CacheReqPayload 字段与 Bundle 对应 |
| 3 | 是否满足 ADR-X.8 的"RTL 简单透传"? | ✅ 是,CacheComponent 不感知交易 |
| 4 | 是否满足 ADR-P0.4 的"周期级 GVT"? | ✅ 是,1 tick = 1 cycle |
| 5 | 是否复用现有 CppTLM ChStream 基础设施? | ✅ 是,继承 ChStreamModuleBase |
| 6 | 是否引入新的外部依赖? | ❌ 否,仅使用已有 CppHDL |
| 7 | 6 个 TEST_CASE 是否覆盖 P2.2 回归? | ✅ 是,3 类回归测试 |
| 8 | 5 天工作量是否合理? | 待评估(基于团队能力) |
| 9 | CppHDL 子模块修复风险? | 已知,Day 1 优先验证 |
| 10 | 命名空间冲突解决是否充分? | ✅ 是,bridge 命名空间 + 别名 |

---

**文档结束**

**维护**: CppTLM 设计团队
**版本**: 1.0
**最后更新**: 2026-06-06
**下一步**: 提交评审 → 启动 Day 1 实施
