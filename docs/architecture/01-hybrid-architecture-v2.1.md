# CppTLM 混合仿真架构 v2.1 — 分层融合方案

> **文档状态**: ✅ Phase 6 完成，端到端验证通过
> **v2.1 版本**: 2.1.10 (v2.1.9 → v2.1.10: 2026-10 状态对齐)
> **更新日期**: 2026-10
> **变更摘要**: Phase 6 端到端验证完成，CI/CD 集成，测试覆盖率 367 用例

---

> **v2.1.10 状态追加（2026-10）**:
>
> 文档锁定 2026-04-22 后 10 个月演进（Phase 7-8 PCIe EP 全链路交付完成），本版本做以下**追加修订**（不破坏原 §1-§8 内容，符合 ADR 不可变先例）：
>
> 1. **§1 设计原则** 增"原则 7：框架与总线分层"（v2.1.10 增）
> 2. **§1.1** 新增"通信框架层 vs 具体总线层"分层（含 6 层 ASCII 图 + Gem5 MessageBuffer 类比 + 关键差异警告）
> 3. **§1.2** 新增"Bundle 命名重塑：未来选项而非必须"（含触发条件 4 条 + 4 项不推荐理由）
> 4. **§4.7 ImplMode 段** 修订——删除不存在的 `impl_mode.hh` 实现引用，改为指向 `simulation_mode` JSON 字段 + `VramControllerTLM::set_mode()` 实际承载
> 5. **新增 `docs/designs/dgpu-soc/communication-network.md`**——通信网络专题蓝图，含 6 层全貌 + 5-hop 数据流拆解 + 5 适配器表 + ModuleFactory 9 步装配 + 8 项关键真相 + 8 项架构债务清单
> 6. **修订 `docs/guide/CREDIT_FLOW_USER_GUIDE.md`**——顶部加"⚠️ 文档漂移警告"状态头，指向 `FcTokenBucket` + `BidirectionalPortAdapter` 为权威 credit 实现
>
> 修订依据：`openspec/changes/cpptlm-dgpu-soc-arch-truth-cleanup` §6 先例（doc-only 类变更不消耗 KPI slot）+ `docs/adr/ADR-DGPU-06` Status Update 2026-10 + `docs/adr/ADR-DGPU-11` timing-mode scope
>
> 通信基础设施当前实况:
> - L3 ChStream + L4 StreamAdapter 5 子类已 Gem5 化（协议中立）
> - L5 Bundle 命名保守（18 个全协议绑定，frozen headers 零修改）
> - Phase 7-8 PCIe EP 端到端贯通（commit `429327d`）
> - 5 标签 0 回归（`[pcie]` 36672 / `[realip]` 193 / `[pcie-memory]` 48 / `[minimal_dgpu_soc]` 66 / `[dgpu_soc_timing]` 12024）

---

## 1. 架构愿景

**核心目标**: 一次建模，多粒度仿真 — 在同一 C++ 框架下，支持事务级 (TLM) 和周期精确级 (RTL) 建模的无缝混合仿真。

**核心策略**: 两套通信模型分层协作，各取所长。

**设计原则**:
1. **模块内部统一**: 新业务模块内部使用 `ch_stream<T>` 握手协议（valid/ready 反压）
2. **Bundle 共享**: 一份轻量级 Bundle 定义（`bundles::CacheReqBundle` / `bundles::CacheRespBundle`），TLM/RTL 共用，减少重复（详见 §4.1）
3. **框架通信保持**: 模块之间使用现有 `MasterPort/SlavePort + PortPair`，保留 JSON 配置驱动
4. **StreamAdapter 桥梁**: Port ↔ ch_stream 转换由框架自动处理，模块设计者无感知
5. **业务纯净**: 新业务模块只关心 ch_stream 业务逻辑，不感知外部 Port 适配复杂度
6. **向后兼容**: 现有 Port 回调模块（Legacy）继续工作，逐步迁移
7. **框架与总线分层** (v2.1.10 增): 通信框架设施（L3 ChStream + L4 StreamAdapter，5 子类：Standalone / MultiPort / DualPort / Bidirectional / AXI4+Mapper）与具体通信总线（L5 Bundle 类型 + L6 协议字段）严格分层。模块设计只依赖 L3+L4；具体总线在后期通过 StreamAdapter 的模板参数 `BundleT` 注入，避免在架构阶段过早固化某种协议（详见 §1.1）

### 1.1 通信框架层 vs 具体总线层（v2.1.10 新增）

**核心立场**: CppTLM 通信体系按"框架设施"和"具体总线"两层组织，前者 Gem5 化（协议中立），后者允许特化。

```
┌────────────────────────────────────────────────────────────┐
│  L6 具体协议字段 (后期固化)                                  │
│  FC token bucket (PcieFlowControlTokenBucket)              │
│  ACK/NAK 12-bit seq wrap (pcie_link_layer_tlm.cc 手写)     │
│  posted-write / credit-based / single-outstanding 协议语义 │
├────────────────────────────────────────────────────────────┤
│  L5 Bundle 数据格式 (命名带协议前缀)                          │
│  PcieTlpBundle / CrBundle / DoorbellBundle / MsixBundle    │
│  CacheReqBundle / NoCFlitBundle / Axi4Bundle / Fabric2Vram │
│  ⚠️ 当前命名直接包含协议名,详见 §1.2 未来选项                  │
├────────────────────────────────────────────────────────────┤
│  L4 StreamAdapter 适配层 (协议中立, 模板只绑 BundleT)        │
│  StreamAdapter / MultiPortStreamAdapter / DualPortStream   │
│  BidirectionalPortAdapter / Axi4StreamAdapter + Axi4Mapper │
│  → 模块只与 L4 交互, 不直接依赖 L5 具体 Bundle 字段语义      │
├────────────────────────────────────────────────────────────┤
│  L3 ChStream 抽象 (Gem5 MessageBuffer 级别)                 │
│  InputStreamAdapter<BundleT> + OutputStreamAdapter<BundleT> │
│  → valid/ready 握手 + Bundle 序列化, 完全协议中立            │
├────────────────────────────────────────────────────────────┤
│  L2 MasterPort / SlavePort / PortPair (TLM 标准传输)        │
├────────────────────────────────────────────────────────────┤
│  L1 tlm_generic_payload + PacketPool                       │
└────────────────────────────────────────────────────────────┘
```

**类比 Gem5 Ruby MessageBuffer（参考坐标系, 非照搬）**:

| 维度 | Gem5 Ruby | CppTLM L3+L4 |
|------|-----------|---------------|
| 抽象层 | MessageBuffer（opaque bytes FIFO） | InputStreamAdapter/OutputStreamAdapter（valid/ready 握手） |
| 协议绑定点 | SLICC 编译期烧进 protocol controller | StreamAdapter\<M, Req, Resp\> 模板参数编译期绑定 |
| 协议切换 | 改 SLICC 文件重新生成 | 改 StreamAdapter 模板参数重新实例化 |
| 拓扑灵活性 | Ruby Topology 参数化（mesh/ring/butterfly） | MultiPortStreamAdapter\<M, Rq, Rp, N\> + BidirectionalPortAdapter |
| 协议字段表达 | MessageBuffer 装 opaque bytes, 字段由 controller 解释 | Bundle 字段命名即字段语义, 协议特定字段放 L5 Bundle 内 |

**关键差异（避免类比误导）**:
- Gem5 MessageBuffer 是真正不透明（opaque bytes），controller 自己解释字段；CppTLM Bundle 是 typed POD，模块能直接 `req.address.read()`——这是 60% Gem5 化而非 100%
- 副作用：Bundle 字段命名一旦确定，**改字段名 = 改模块代码**，无法在运行时透明切换协议
- 这是有意取舍：60% Gem5 化换 100% 编译期类型安全（`BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable` 断言）

**演进路径**:
- 当前 L3+L4 已达 Gem5 MessageBuffer 水准，**不需新增抽象层**
- L5 Bundle 命名直接含协议（PcieTlp / Cr）**是 over-spec 但非阻塞**——只在出现第二个 chip-internal 协议特化需求时才有重塑价值（详见 §1.2）

### 1.2 Bundle 命名重塑：未来选项而非必须

> **⚠️ v0.4 X-11 cross-ref**: 本节定义 Bundle 命名的"现状保守 + 重塑触发条件"真源. [`docs/architecture/06-inter-module-network.md`](06-inter-module-network.md) §3 (Bundle 二分类: 业务 vs 网络) 在本节基础上提议"业务 Bundle 可演进 + 网络 Bundle 后期协议特化时再定"的演进方案, **不破坏本节现状保守立场**. 06 §3.1 业务 Bundle = 本节"命名直接带协议前缀"现状, 06 §3.1 网络 Bundle = 本节"未来重塑触发条件"满足时的演进方向.

**当前现状**: 17 个 Bundle .hh 文件中（`include/bundles/`, per `ls include/bundles/*.hh`），命名直接带协议前缀（PcieTlp / Cr / Doorbell / Msix），这一选择**在编译期类型安全**与**协议中立**之间取了前者。

**未来可选的重塑方案**（**仅在出现第二个 chip-internal 协议时启动，不在当前阶段实施**）:

```cpp
// 当前（已实施）
struct PcieTlpBundle : public bundle_base {
    ch_uint<8>  kind;          // CFG_READ..MEM_WRITE + IRQ_DELIVERY + CPLD
    ch_uint<8>  bar_index;
    ch_uint<64> offset;
    ch_uint<32> size;
    ch_uint<64> data;
    ch_uint<16> requester_id;
    ch_uint<32> trans_id;
    // ...
};

// 假设的 TransportReqBase<> 模板（未实施, 仅示意）
template<typename ProtocolTag>  // PcieTlpTag, Axi4Tag, ChipInternalTag, ...
struct TransportReqBundle : public bundle_base {
    ch_uint<8>  cmd;           // 协议无关命令码
    ch_uint<64> addr;
    ch_uint<64> data;
    ch_uint<32> trans_id;
    // 协议特定字段通过 ProtocolTag trait 注入, 不污染通用字段
};
```

**为什么是未来选项而非必须**:
- 当前 18 个 Bundle 中, 真正需要 chip-internal 中立抽象的只有 SDMA 5 端口（`sdma_engine_tlm.hh:82-86`）——但这是**唯一**的 chip-internal 协议特化, **抽象出基类只有 1 个用户**, 不符合"≥2 用户"重构原则
- 18 Bundle 中其余 17 个都是 board-level（host↔chip）或 chip-internal 通用 cache/NOC, **命名已合理**
- 任何重塑都会触碰 frozen headers（`pcie_bundles_tlm.hh`）+ 23 ABI 冻结面, **风险高于收益**
- **`port_contract_asserts.hh:4` 已记录历史教训**: 早期计划 `ChStreamInitiatorPort<Bundle>` 模板化被 Momus REJECT, 因为"协议中立"带来的类型擦除成本 > 收益

**触发条件**（**全部满足时才启动重塑**）:
1. 出现 ≥2 个 chip-internal 协议特化需求（当前仅 SDMA 1 个）
2. KPI < 2（当前 3/3 满, 需先 archive 一个 change）
3. frozen headers 修改范围被约束在 ≤2 个文件
4. `port_contract_asserts.hh` 提供编译期断言保证"不破坏既有契约"

**当前行动**: 维持现状；L3+L4 已足够支撑"模块只与框架交互、后期注入具体总线"的设计目标；L5 命名是工程取舍而非架构问题。

---

## 2. 分层融合架构

### 2.1 整体分层

```
┌─────────────────────────────────────────────────────────────┐
│  应用层：用户业务模块                                         │
│  ┌────────────────────┐  ┌────────────────────┐             │
│  │  CacheTLM (新)     │  │  CacheSim (Legacy) │             │
│  │  内部: ch_stream   │  │  内部: Port回调     │             │
│  │  ┌──────────────┐  │  │  ┌──────────────┐ │             │
│  │  │StreamAdapter │  │  │  │ PortManager  │ │             │
│  │  │ (框架创建)   │  │  │  │ (直接回调)   │ │             │
│  │  └──────┬───────┘  │  │  └──────┬───────┘ │             │
│  └─────────┼──────────┘  └─────────┼─────────┘             │
│            │ (暴露 Port)            │ (直接 Port)            │
├────────────┼────────────────────────┼───────────────────────┤
│  框架通信层：PortPair 网络（现有）                            │
│            │                        │                        │
│  ┌─────────▼────────────────────────▼─────────┐            │
│  │  MasterPort ◄─── PortPair ───► SlavePort   │            │
│  │  (MasterPort/SlavePort 负责跨模块通信)       │            │
│  │  支持: delay, buffer, group, regex 连接     │            │
│  └─────────────────────────────────────────────┘            │
│              │ JSON配置驱动的动态装配                          │
│  ┌───────────▼────────────────────────────────┐            │
│  │  ModuleFactory (JSON驱动)                   │            │
│  │  ├─ Legacy模块: 直接通过 PortManager 工作   │            │
│  │  └─ ChStream模块: 框架自动附加 StreamAdapter │            │
│  └─────────────────────────────────────────────┘            │
├─────────────────────────────────────────────────────────────┤
│  Mapper 层（未来）：协议格式转换                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ FragmentMapper│  │  AXI4Mapper  │  │  CHIMapper   │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
├─────────────────────────────────────────────────────────────┤
│  Bundle 层：CppHDL 统一共享 (external/CppHDL)                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │ CacheBundle  │  │  NoCBundle   │  │ AXI4Bundle   │       │
│  │ (ch_stream)  │  │ (ch_stream)  │  │ (ch_stream)  │       │
│  └──────────────┘  └──────────────┘  └──────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 核心洞察：为什么分层融合可行

| 层次 | 通信模型 | 理由 |
|:---|:---|:---|
| **模块内部** | `ch_stream<T>` 握手 (valid/ready) | 轻量级 POD Bundle（`bundles::*Bundle`） + `ch_stream<T>` 基础设施，反压语义适合模块内部流水线 |
| **模块之间** | `MasterPort/SlavePort` + `PortPair` | 现有框架成熟，支持 JSON 配置、group/regex 连接、delay/buffer 管理 |
| **桥梁** | `StreamAdapter` | 在模块边界做 Packet ↔ Bundle 转换，框架自动创建 |

**Bundle 承载方式**: `tlm_generic_payload` 作为通用容器，通过 CppHDL 的 `bundle_serialization` 将 Bundle 序列化/反序列化到 payload 的 data 指针中：

```cpp
// Bundle → Packet (StreamAdapter 输出方向)
template<typename BundleT>
Packet* bundle_to_packet(const BundleT& bundle, uint64_t stream_id) {
    Packet* pkt = PacketPool::get().acquire();
    pkt->type = PKT_REQ;
    pkt->stream_id = stream_id;
    ch::core::serialize(bundle, pkt->payload->get_data_ptr(), pkt->payload->get_data_length());
    return pkt;
}

// Packet → Bundle (StreamAdapter 输入方向)
template<typename BundleT>
BundleT packet_to_bundle(Packet* pkt) {
    BundleT bundle;
    ch::core::deserialize(bundle, pkt->payload->get_data_ptr(), pkt->payload->get_data_length());
    return bundle;
}
```

---

## 3. 目录结构

```
include/
├── bundles/                      # CppTLM Bundle 定义（业务层）
│   ├── cache_bundles_tlm.hh      # Cache 请求/响应 Bundle（轻量级）
│   ├── noc_bundles_tlm.hh        # NoC 数据包 Bundle
│   ├── axi4_bundles.hh           # AXI4 协议 Bundle（未来扩展）
│   └── fragment_bundles.hh       # Fragment 分片 Bundle（未来扩展）
│
├── tlm/                          # [新增] 新式 TLM 模块实现（基于 ch_stream）
│   ├── cache_tlm.hh              # Cache TLM 模块 (ch_stream 内部)
│   ├── crossbar_tlm.hh           # Crossbar TLM 模块 (多端口 ch_stream)
│   ├── nic_tlm.hh                # NIC TLM 模块 (Fragment 分片)
│   └── memory_tlm.hh             # Memory TLM 模块
│
├── modules/legacy/               # [迁移] 旧式 Port 回调模块
│   ├── cache_sim.hh              # CacheSim (Legacy Port 回调模式)
│   ├── cpu_sim.hh                # CPUSim
│   ├── memory_sim.hh             # MemorySim
│   ├── crossbar.hh               # Crossbar
│   ├── router.hh                 # Router
│   ├── arbiter.hh                # Arbiter
│   ├── cpu_cluster.hh            # CpuCluster
│   ├── traffic_gen.hh            # TrafficGenerator
│   └── stream_consumer.hh        # StreamConsumer
│
├── framework/                    # 框架层
│   ├── stream_adapter.hh         # [新增] StreamAdapter (Port ↔ ch_stream 桥梁)
│   ├── stream_adapter_registry.hh# [新增] StreamAdapter 注册表
│   ├── transaction_tracker.hh    # 交易追踪单例
│   ├── debug_tracker.hh          # 调试追踪单例
│   └── error_category.hh         # 错误分类体系
│
├── core/                         # 核心依赖（保持不变）
│   ├── sim_object.hh             # SimObject 基类
│   ├── sim_module.hh             # SimModule 复合模块
│   ├── module_factory.hh         # 模块工厂（JSON 驱动）
│   ├── connection_resolver.hh    # 连接解析器
│   ├── port_manager.hh           # 端口管理器
│   ├── master_port.hh            # Master Port
│   ├── slave_port.hh             # Slave Port
│   ├── simple_port.hh            # SimplePort + PortPair
│   ├── packet.hh                 # Packet 数据承载
│   ├── packet_pool.hh            # Packet 池
│   ├── event_queue.hh            # 事件队列调度
│   ├── plugin_loader.hh          # 动态插件加载
│   └── ...
│
└── modules.hh                    # 注册宏（同时注册 Legacy + ChStream 模块）
```

---

## 4. 核心设计

### 4.1 Bundle 层（统一共享定义）

**原则**: 使用轻量级 POD 结构（继承空基类 `bundles::bundle_base`，无 CppHDL AST/vtable），TLM/RTL 共用同一字段语义。

> **权威说明**: 详见 `include/tlm/AGENTS.md:39`（原文："Bundle 定义见 `include/bundles/cache_bundles_tlm.hh`（轻量级，非 Ch 原生 Bundle）"）

```cpp
// include/bundles/cache_bundles_tlm.hh（实际实现 — 轻量级 TLM 侧）
namespace bundles {

struct CacheReqBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;        // 父事务 ID（0 = 根事务）
    ch_uint<8>  fragment_id;      // 当前拍序号（0-based）
    ch_uint<8>  fragment_total;   // 总拍数（1 = 不分片）
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;
    // ... 构造器、谓词方法（is_first_fragment/is_last_fragment/is_root）
};

struct CacheRespBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;
    ch_uint<8>  fragment_id;
    ch_uint<8>  fragment_total;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;
    ch_bool     first;
    ch_bool     last;
    // ... 构造器、谓词方法
};

} // namespace bundles
```

> **设计说明**: Bundle 是**轻量级 POD 结构**（继承空基类 `bundle_base`，无 CppHDL AST/vtable），字段使用 `ch_uint<N>/ch_bool` 包装（仅 `read()/write()` 方法），`std::memcpy` 序列化安全。Bundle 只定义数据形状，不绑定通信协议。

### 4.2 TLM 新模块（纯 ch_stream 内部 + StreamAdapter 暴露）

**原则**: 业务模块内部使用 `InputStreamAdapter/OutputStreamAdapter`（ch_stream 语义等价），框架层通过 StreamAdapter 自动暴露为 Port。

> **注**: 实际实现使用 `InputStreamAdapter<BundleT>` 和 `OutputStreamAdapter<BundleT>` 提供 `valid()/ready()/data()/consume()/write()` 等 ch_stream 语义方法，而非直接使用 CppHDL 的 `ch_stream<T>`。参见 `include/framework/stream_adapter.hh`。

```cpp
// include/tlm/cache_tlm.hh (Phase 1 即将实现)
#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <map>

/**
 * @brief Cache TLM 模块（新式 ch_stream 内部模型）
 * 
 * 模块内部使用 InputStreamAdapter/OutputStreamAdapter 提供 ch_stream 语义
 * 框架层通过 StreamAdapter 自动转换 Port ↔ ch_stream
 * 
 * JSON 注册名: "CacheTLM"
 */
class CacheTLM : public ChStreamModuleBase {
private:
    InputStreamAdapter<bundles::CacheReqBundle>  req_in_;
    OutputStreamAdapter<bundles::CacheRespBundle> resp_out_;
    std::map<uint64_t, uint64_t> cache_lines_;
    StreamAdapterBase* adapter_ = nullptr;

public:
    CacheTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}

    void set_stream_adapter(StreamAdapterBase* adapter) override { adapter_ = adapter; }

    void tick() override {
        if (req_in_.valid() && req_in_.ready()) {
            auto& req = req_in_.data();
            uint64_t addr = req.address.read();
            bool is_write = req.is_write.read();
            bool hit = cache_lines_.count(addr) > 0;
            if (is_write) cache_lines_[addr] = req.data.read();

            bundles::CacheRespBundle resp;
            resp.transaction_id = req.transaction_id;
            resp.data = cache_lines_[addr];
            resp.is_hit = hit ? ch_bool(true) : ch_bool(false);
            resp.error_code = ch_uint<8>(0);
            resp_out_.write(resp);
            req_in_.consume();
        }
    }

    void do_reset(const ResetConfig& config) override {
        cache_lines_.clear();
        req_in_.reset();
        resp_out_.reset();
    }
};
```
};
```

> **设计说明**: 模块代码完全不知道 Port 的存在。`StreamAdapter` 由 `ModuleFactory` 在创建模块时自动附加，负责把模块的 `ch_stream` 端映射到 `MasterPort/SlavePort`。

### 4.3 TLM 模块清单

已实现的所有 TLM 模块（位于 `include/tlm/`）：

| 模块 | 类型 | 说明 |
|------|------|------|
| `CacheTLM` | 单端口 | L1 缓存，ch_stream 内部通信 |
| `CrossbarTLM` | 多端口 | 交叉开关，支持 N 端口并发 |
| `MemoryTLM` | 单端口 | 内存端点，TLM 仿真目标 |
| `CPUTLM` | 单端口 | CPU 代理，产生流量 |
| `TrafficGenTLM` | 单端口 | 流量生成器 |
| `ArbiterTLM` | 单端口 | 仲裁器 |
| `RouterTLM` | 多端口 | 路由节点 |
| `NICTLM` | 单端口 | 网络接口控制器 |

**已注册适配器**（通过 `chstream_register.hh` 中的 `REGISTER_CHSTREAM` 宏）：

| 模块类型 | 适配器类型 | 端口模式 |
|---------|-----------|---------|
| `CacheTLM` | `StreamAdapter` | 单端口 |
| `MemoryTLM` | `StreamAdapter` | 单端口 |
| `CPUTLM` | `StreamAdapter` | 单端口 |
| `TrafficGenTLM` | `StreamAdapter` | 单端口 |
| `ArbiterTLM` | `StreamAdapter` | 单端口 |
| `RouterTLM` | `MultiPortStreamAdapter` | 多端口 |
| `NICTLM` | `StreamAdapter` | 单端口 |
| `CrossbarTLM` | `MultiPortStreamAdapter` | 多端口 |

> **实现说明**: 实际 `StreamAdapter` 使用直接模块指针（`ModuleT* module_`）而非 `std::function` 间接层，参见 `include/framework/stream_adapter.hh:210-261`。

### 4.4 ModuleFactory 扩展：支持 ChStream 模块

**核心思路**: ModuleFactory 通过 `ChStreamAdapterFactory` 自动识别 ChStreamModuleBase 派生类，无需 JSON 配置额外字段。

**JSON 配置格式（实际使用）**:

```json
{
  "modules": [
    { "name": "cpu0", "type": "CPUSim" },
    { "name": "l1", "type": "CacheTLM" },
    { "name": "xbar", "type": "CrossbarTLM", "n_ports": 4 },
    { "name": "mem", "type": "MemoryTLM" }
  ],

  "connections": [
    { "src": "cpu0", "dst": "l1", "latency": 2 },
    { "src": "l1", "dst": "xbar.0", "latency": 1 },
    { "src": "xbar.1", "dst": "mem", "latency": 10 }
  ]
}
```

> **说明**: 无需 `"mode": "chstream"` 或 `"req_bundle"/"resp_bundle"` 字段。ModuleFactory 在 Step 6 遍历所有 `object_instances`，通过 `dynamic_cast<ChStreamModuleBase*>` 自动识别 ChStream 模块，然后通过 `ChStreamAdapterFactory` 创建对应类型的 StreamAdapter。

**`instantiateAll` 实际逻辑（Step 6 - StreamAdapter 注入）**:

```cpp
// src/core/module_factory.cc (2026-04-22 Phase 6 实现)
void ModuleFactory::instantiateAll(const json& config) {
    json final_config = JsonIncluder::loadAndInclude(config);

    // ... Step 1-5: 创建模块 ...

    // === Step 6: ChStream 模块自动识别 + StreamAdapter 创建 ===
    ChStreamAdapterFactory& factory = ChStreamAdapterFactory::get();
    for (auto& [name, obj] : object_instances) {
        auto* ch_mod = dynamic_cast<ChStreamModuleBase*>(obj);
        if (!ch_mod) continue;

        const std::string& type = module_types[name];
        if (!factory.knows(type)) continue;  // 无注册适配器则跳过

        auto adapter = factory.create(type, obj);
        // 创建 N 组端口（单端口/多端口/双端口自动区分）
        // ...
        ch_mod->set_stream_adapter(adapter);
    }

    // === Step 7: 建立连接（端口索引语法支持） ===
    // ...
}
```

**自动识别机制**:
- 通过 `dynamic_cast<ChStreamModuleBase*>(obj)` 判断是否为 ChStream 模块
- 通过 `ChStreamAdapterFactory::knows(type)` 判断是否有注册的适配器工厂
- 适配器类型由模块的 `REGISTER_CHSTREAM` 宏注册时确定

**端口索引语法**: `"module.port_index"` 支持多端口模块（如 `xbar.0` 表示 xbar 的第 0 个 req_in 端口）

**ChStreamModule 基类**（✅ 已实现 — 2026-04-12 Phase 0，与文档伪代码有差异）：

```cpp
// include/core/chstream_module.hh (实际实现)
class ChStreamModuleBase : public SimObject {
public:
    ChStreamModuleBase(const std::string& n, EventQueue* eq) 
        : SimObject(n, eq) {}
    virtual ~ChStreamModuleBase() = default;
    
    virtual void set_stream_adapter(StreamAdapterBase* adapter) = 0;  // ← 文档写 set_adapter，实际为 set_stream_adapter
};
```

> **差异说明**:
> | 文档伪代码 | 实际实现 | 原因 |
> |---|---|---|
> | `set_adapter()` | `set_stream_adapter()` | 命名更明确，避免歧义 |
> | `virtual std::string get_module_type() const = 0` | 未在此处声明（从 SimObject 继承 `get_module_type()`） | SimObject 已提供虚接口 |
> | `REGISTER_CHSTREAM_MODULE` 宏 | 未定义（复用 `REGISTER_OBJECT`） | ChStream 模块注册为 SimObject，通过 `dynamic_cast` 识别 |
> | `#include "framework/stream_adapter.hh"` | 仅前向声明 `class StreamAdapterBase` | 避免循环依赖 |

#### 4.3.1 Phase 0 实施状态（2026-04-12）

实际实现见 `include/framework/stream_adapter.hh`，提供 `InputStreamAdapter<BundleT>` 和 `OutputStreamAdapter<BundleT>` 两个模板类，模拟 `ch_stream` 的 `valid()/ready()` 语义。

#### Phase 0 实施状态表

| 组件 | 文件 | 状态 | 说明 |
|:---|:---|:---|:---|
| `ChStreamModuleBase` | `include/core/chstream_module.hh` | ✅ 已实现 | 继承 SimObject + `set_stream_adapter()` 纯虚接口 |
| `StreamAdapterBase` | `include/framework/stream_adapter.hh` | ✅ 已实现 | 类型擦除基类 + `bind_ports`/`process_request_input`/`process_response_output` |
| `InputStreamAdapter<T>` | `include/framework/stream_adapter.hh` | ✅ 已实现 | 提供 `valid()/ready()/data()/consume()/reset()` 方法 |
| `OutputStreamAdapter<T>` | `include/framework/stream_adapter.hh` | ✅ 已实现 | 提供 `write()/valid()/clear_valid()/send()/reset()` 方法 |
| `CacheReqBundle` | `include/bundles/cache_bundles_tlm.hh` | ✅ 已实现 | 轻量级（TLM 用） |
| `CacheRespBundle` | `include/bundles/cache_bundles_tlm.hh` | ✅ 已实现 | 轻量级（TLM 用） |
| `NoCReqBundle/NoCRespBundle` | `include/bundles/noc_bundles_tlm.hh` | ✅ 已实现 | 轻量级（TLM 用） |
| `FragmentBundle` | `N/A` | ❌ 未实现 | 规划中，暂无代码实现 |
| `serialize_bundle`/`deserialize_bundle` | `include/bundles/bundle_serialization.hh` | ✅ 已实现 | 仿真内 `memcpy` 序列化 |
| `ModuleFactory::instantiateAll` (chstream 扩展) | `src/core/module_factory.cc` | ❌ 未实现 | Phase 1 计划扩展 |

### 4.5 Transaction/Debug 架构（完整对齐）

| 组件 | 文档要求 | 实际代码 | 对齐状态 |
|:---|:---|:---|:---|
| `TransactionTracker` | 单例，粗/细粒度追踪，父子交易，分片 | `include/framework/transaction_tracker.hh` | ✅ 完全对齐 |
| `TransactionContextExt` | tlm_extension 携带 transaction_id | `include/ext/transaction_context_ext.hh` | ✅ 完全对齐 |
| `DebugTracker` | 单例，错误记录，状态快照，查询接口 | `include/framework/debug_tracker.hh` | ✅ 完全对齐 |
| `ErrorContextExt` | tlm_extension 携带 error_code/category | `include/ext/error_context_ext.hh` | ✅ 完全对齐 |
| `ErrorCategory/ErrorCode` | 分层错误分类 | `include/core/error_category.hh` | ✅ 完全对齐 |

### 4.6 Reset/Checkpoint 架构（完整对齐）

`SimObject` 基类已提供完整能力：

| 能力 | 文档要求 | 实际代码 | 对齐状态 |
|:---|:---|:---|:---|
| 层次化复位 | `ResetConfig{hierarchical, save_snapshot}` | `sim_object.hh::reset(ResetConfig)` | ✅ 对齐 |
| 快照保存 | `save_snapshot(json&)` | `sim_object.hh::save_snapshot()` | ✅ 对齐 |
| 快照恢复 | `load_snapshot(json&)` | `sim_object.hh::load_snapshot()` | ✅ 对齐 |

### 4.7 实现模式枚举（未来扩展）

> **⚠️ v0.4 X-11 cross-ref**: 本节 (v2.1.10 修订) 是 `simulation_mode` JSON 字段的真源. [`docs/architecture/06-inter-module-network.md`](06-inter-module-network.md) §5 (仿真精度 2 模式) 在本节基础上详化 "不引入 `network.mode` 字段" + "P1 仅 per-bridge 静态" 的演进约束, **不重复声明 simulation_mode 字段定义**.

> **v2.1.10 修订（2026-10）**: 上述 `impl_mode.hh` **未实施**——`include/framework/impl_mode.hh` 不存在，意图由 `simulation_mode` JSON 字段 + `VramControllerTLM::set_mode()`（functional/timing 双模式，per ADR-DGPU-11）承载。**COMPARE / SHADOW 模式无实施计划**。详见 `docs/designs/dgpu-soc/communication-network.md` §3.4 "功能/时序双模式"。

```cpp
// 实际承担 ImplMode 等效能力的实现（v2.1.10 修订, 非 impl_mode.hh 风格 enum）
// 1) JSON config 字段: "simulation_mode": "functional" | "timing"（per dgpu_soc_minimal_v1.json）
// 2) C++ API: VramControllerTLM::set_mode(VramControllerMode) 在 DGpuBoard::bind_memory_backings() 调用
// 3) COMPARE/SHADOW: 文档不承诺, 实施路径待定
```

---

## 5. 关键迁移路径

### 5.1 Legacy → ChStream 迁移策略

```
Phase 0: 基础设施就绪
  ├─ [新建] include/bundles/*.hh (Bundle 定义)
  ├─ [新建] include/framework/stream_adapter.hh (桥梁)
  └─ [新建] include/core/chstream_module.hh (基类)

Phase 1: 单模块试点
  ├─ [新建] include/tlm/cache_tlm.hh (CacheTLM)
  └─ [验证] CacheTLM JSON 配置加载 + StreamAdapter 自动创建

Phase 2: Legacy 迁移
  ├─ [迁移] include/modules/legacy/  (旧模块移入 legacy/)
  └─ 保持 Legacy 模块正常工作（向后兼容

Phase 3: 多模块 + 连接
  ├─ [新建] include/tlm/crossbar_tlm.hh (多端口 ch_stream)
  ├─ [新建] include/tlm/memory_tlm.hh
  └─ 验证完整系统连接

Phase 4: Mapper + RTL (未来)
  ├─ [新建] include/mapper/fragment_mapper.hh
  └─ [新建] include/rtl/cache_component.hh
```

### 5.2 关键决策

| 决策点 | 选项 | 选择 | 理由 |
|:---|:---|:---|:---|
| Bundle 承载方式 | 独立 Bundle 对象 vs tlm_generic_payload 内嵌 | **内嵌到 payload** | 复用现有 Packet → Bundle 序列化/反序列化，不破坏 PortPair |
| StreamAdapter 位置 | 模块内创建 vs 工厂自动创建 | **工厂自动创建** | 模块不感知外部通信，保持业务纯净 |
| Legacy 模块处理 | 直接删除 vs 移入 legacy/ | **移入 legacy/** | 渐进式迁移，保留向后兼容 |
| 连接配置 | 新 schema vs 扩展现有 schema | **扩展现有** | 只需新增 `mode` 字段，不影响现有 JSON |

---

## 6. 相关文档

| 文档 | 位置 | 状态 |
|:---|:---|:---|
| 交易处理架构 | `02-transaction-architecture.md` | ✅ 对齐 |
| 错误调试架构 | `03-error-debug-architecture.md` | ✅ 对齐 |
| 复位检查点架构 | `04-reset-checkpoint-architecture.md` | ✅ 对齐 |
| FragmentMapper 决议 | `FRAGMENT_MAPPER_DECISIONS.md` | 📋 待实施 |
| P0/P1/P2 决策汇总 | `P0_P1_P2_DECISIONS.md` | ✅ 对齐 |
| Transaction ID 架构决策 | `docs/adr/ADR-X.1-transaction-id.md` | ✅ 对齐 |
| Transaction 整合方案 | `docs/adr/ADR-X.6-transaction-integration.md` | ✅ 对齐 |
| 端口类型系统决策 | `docs/adr/ADR-X.9-port-type-system.md` | ✅ 对齐 |
| Hybrid TLM+CppHDL v4 设计 | `docs/architecture/examples/hybrid/hybrid_tlm_cppHDL_design_v4.md` | 📋 v4 替代 v3 |

---

## 7. 与 v2.0 的差异对比

| 特性 | v2.0 | v2.1 (分层融合) |
|:---|:---|:---|
| 模块内部通信 | ch_stream ✅ | ch_stream ✅ |
| 模块之间通信 | ch_stream (文档) | **PortPair + StreamAdapter** (实际) |
| Bundle 承载 | 独立 ch_stream 对象 | **tlm_generic_payload 内嵌序列化** |
| 框架层 | ModuleRegistry (独立) | **ModuleFactory 扩展** (渐进增强) |
| Legacy 模块 | 未提及 | **legacy/ 子目录共存迁移** |
| 向后兼容性 | 无 | **JSON mode 字段开关** |
| 设计哲学 | 单范式（纯 ch_stream） | **分层融合（内 ch_stream + 外 PortPair）** |

---

**版本**: v2.1.9
**创建日期**: 2026-04-12
**最后更新**: 2026-04-22 Phase 6 完成后
**批准状态**: ✅ Phase 6 完成，CI/CD 集成，零债务验收
**创建者**: Sisyphus (AI Architect)

## 8. Phase 0 审查后更新

### 8.1 文档与实际代码的差异

| 文档伪代码 | 实际实现 | 差异原因 |
|:---|:---|:---|
| `set_adapter(StreamAdapterBase*)` | `set_stream_adapter(StreamAdapterBase*)` | 命名更明确 |
| `REGISTER_CHSTREAM_MODULE` 宏 | 复用 `REGISTER_OBJECT` | ChStream 模式注册为 SimObject 即可 |
| 模板例使用 `ch_stream<T>` | `InputStreamAdapter<T>` / `OutputStreamAdapter<T>` | CppHDL `ch_stream<T>` 依赖完整运行时，Adapter 提供更轻的仿真内等价物 |
| 序列化使用 `ch::core::serialize()` CppHDL API | `std::memcpy` | CppHDL 的 `serialize()` 返回 `ch_uint<WIDTH>`，不适合直接写入 Packet payload |

### 8.2 Phase 0 审查发现问题

| 编号 | 文件 | 问题 | 严重级别 | 处理建议 |
|:---|:---|:---|:---|:---|
| CQ-001 | `bundles/cache_bundles_tlm.hh`<br>`noc_bundles_tlm.hh` | 头文件中使用 `using namespace ch::core;`，污染全局命名空间 | 中 | Phase 1 前修复，改为显式 `ch::core::` 前缀 |
| CQ-002 | `bundle_serialization.hh` | `std::memcpy` 序列化 Bundle 类型（含 vtable/AST 节点），UB 风险 | 高 | 仅在仿真进程内使用可接受；跨进程需改用字段级序列化 |
| CQ-003 | `stream_adapter.hh` | `StreamAdapterBase` 有抽象方法 `bind_ports`、`tick` 等，但无模板子类实现 | 中 | Phase 1 创建 `CacheTLM` 时补齐具体 StreamAdapter 实现 |
| CQ-004 | `module_factory.cc` | 未扩展 `instantiateAll` 支持 `mode: "chstream"` 自动注入 | 中 | Phase 1 核心任务 |

---

### 8.3 Phase 1 关键架构调整（2026-04-12 更新）

#### 8.3.1 轻量级 Bundle 类型系统

**背景**: CppHDL 内部 include 路径不一致（`ast_nodes.h`/`lnodeimpl.h`/`logger.h` 相对路径断裂），直接 `#include "ch.hpp"` 触发编译阻塞链。

**解决方案**: 双轨 Bundle 类型系统

| 类型系统 | 依赖 | 用途 | 文件 |
|---------|------|------|------|
| **轻量级** | 仅 `<cstdint>` | TLM 仿真（当前 Phase 1-5） | `bundles/cpphdl_types.hh`<br>`bundles/cache_bundles_tlm.hh` |
| **完整版** | CppHDL `ch.hpp` | RTL 阶段（未来扩展） | `bundles/cache_bundles.hh`<br>`bundles/noc_bundles.hh`<br>`bundles/fragment_bundles.hh` |

**轻量级类型设计** (`bundles/cpphdl_types.hh`):

```cpp
// ch_uint — 字段读写包装
template<unsigned W = 64>
struct ch_uint {
    uint64_t value_;
    uint64_t read() const { return value_; }
    void write(uint64_t v) { value_ = v; }
};

// ch_bool — 布尔包装
struct ch_bool {
    bool value_;
    bool read() const { return value_; }
    void write(uint64_t v) { value_ = (v != 0); }
};

// bundle_base — 无虚函数的空基类（标准布局，memcpy 安全）
struct bundle_base {};
```

**设计原则**:
- **TLM 阶段**: 使用轻量级 Bundle（POD 字段，无 AST 节点，无 vtable） → `memcpy` 序列化安全
- **RTL 阶段**: 切换到 CppHDL Bundle（含 AST 节点，支持 Verilog 生成）
- **兼容性**: 两套 Bundle 共享相同字段名称和语义，后续可通过 trait/template 统一

#### 8.3.2 模块注册分离

**目的**: 避免每次编译都拉入 ChStream 模块依赖（包括轻量级 Bundle + StreamAdapter）

| 文件 | 职责 | 包含 |
|------|------|------|
| `modules.hh` | Legacy 模块注册 + `REGISTER_CHSTREAM` 宏定义 | 所有 `modules/` 头文件 |
| `chstream_register.hh` | ChStream 模块注册入口 | `modules.hh` + `tlm/cache_tlm.hh` + `tlm/memory_tlm.hh` |
| `main.cpp` | 主入口 | `chstream_register.hh` + `REGISTER_ALL` 宏 |

#### 8.3.3 StreamAdapter 模板与命名空间

`InputStreamAdapter` / `OutputStreamAdapter` / `StreamAdapter` 使用 `cpptlm` 命名空间，与 Bundle 的 `bundles` 命名空间分离：

```cpp
namespace cpptlm {
    class StreamAdapterBase;
    template<typename BundleT> class InputStreamAdapter;
    template<typename BundleT> class OutputStreamAdapter;
    template<typename M, typename Rq, typename Rp> class StreamAdapter;
}

namespace bundles {
    struct bundle_base;  // 轻量级
    struct CacheReqBundle;
    struct CacheRespBundle;
}
```

#### 8.3.4 ModuleFactory ChStream 注入逻辑

`src/core/module_factory.cc` 新增 **Step 7/7b**：在连接建立后，通过 `dynamic_cast<ChStreamModuleBase*>` 识别 ChStream 模块，根据 `ChStreamAdapterFactory::isMultiPort(type)` 区分单端口/多端口，为多端口模块（CrossbarTLM）创建 N=4 组端口 + 端口索引连接。

| 组件 | 文件 | 状态 |
|:---|:---|:---|
| `InputStreamAdapter<BundleT>` | `framework/stream_adapter.hh` | ✅ Phase 0 |
| `OutputStreamAdapter<BundleT>` | `framework/stream_adapter.hh` | ✅ Phase 0 |
| `StreamAdapter<ModuleT,ReqB,RespB>` | `framework/stream_adapter.hh` | ✅ Phase 0（单端口） |
| `MultiPortStreamAdapter<M,Rq,Rp,N>` | `framework/multi_port_stream_adapter.hh` | ✅ Phase 5（多端口） |
| `ChStreamAdapterFactory`（多端口注册） | `framework/chstream_adapter_factory.hh` | ✅ Phase 5 |
| ModuleFactory Step 7（多端口感知） | `module_factory.cc` | ✅ Phase 5 |
| CacheTLM | `tlm/cache_tlm.hh` | ✅ Phase 1 |
| MemoryTLM | `tlm/memory_tlm.hh` | ✅ Phase 1 |
| CrossbarTLM | `tlm/crossbar_tlm.hh` | ✅ Phase 4 |
| JSON 端口索引语法（xbar.0） | `configs/crossbar_test.json` | ✅ Phase 5 |
| 单元测试（chstream） | `test/test_*.cc` | ✅ 75 用例全通过 |

---

### 8.4 Phase 4-5 关键架构扩展（2026-04-13 更新）

#### 8.4.1 CrossbarTLM 多端口设计

```
CrossbarTLM (4 端口):
┌──────────────────────────┐
│  req_in[0] ──┐           │
│  req_in[1] ──┼─► 路由矩阵 ┤──► resp_out[0..3]
│  req_in[2] ──┤           │
│  req_in[3] ──┘           │
└──────────────────────────┘
```

**路由策略**：`dst_port = (addr >> 12) & 0x3`
- Port 0: `0x0000-0x0FFF`
- Port 1: `0x1000-0x1FFF`
- Port 2: `0x2000-0x2FFF`
- Port 3: `0x3000-0x3FFF`

#### 8.4.2 MultiPortStreamAdapter 模板

```cpp
template<typename ModuleT, typename ReqBundleT, typename RespBundleT, std::size_t N>
class MultiPortStreamAdapter : public StreamAdapterBase {
    // bind_ports_array() — 绑定 N 组 MasterPort/SlavePort
    // tick() — 遍历 N 个端口，转发 resp_out
    // process_request_input(pkt, port_idx) — 按端口反序列化
};
```

#### 8.4.3 JSON 端口索引语法

**格式**: `"module_name.port_index"`

```json
{ "connections": [
    { "src": "cache", "dst": "xbar.0", "latency": 1 },
    { "src": "xbar.1", "dst": "mem1", "latency": 2 }
]}
```
- `xbar.0` → xbar 的第 0 个 req_in 端口
- 单端口模块可省略索引（默认 0）

---



**验证链路**: CacheTLM → CrossbarTLM → MemoryTLM 全链路

```
CacheTLM (单端口) → xbar.0 (端口索引) → MemoryTLM (单端口)
```

**测试文件**: `test/test_phase6_integration.cc` — 9 测试用例，53 断言全通过

**关键验证项**:
- ModuleFactory 完整 JSON 加载 + instantiateAll 一次性实例化
- 单端口 + 多端口模块混合拓扑
- StreamAdapter 自动注入（单端口 Standalone + 多端口 MultiPort）
- JSON 端口索引语法端到端验证

### 8.5 PortManager Mirror + CoherentXBarTLM（v2.3 新增，2026-06-19 更新）

#### 8.5.1 D.1 PortManager Mirror

`SimModule::getInternalOutputPort` 对 ChStream 模块（CPUTLM/CacheTLM/MemoryTLM/CrossbarTLM）的可见性修复。

**机制**：`PortManager` 新增 `mirrorExistingDownstreamPort` / `mirrorExistingUpstreamPort` 方法。
`ModuleFactory` Step 7 创建 ChStream 端口后调用 mirror 注册到子模块 PortManager（仅入 map，不入 vector）。

**影响**：
- 解锁 P3 `connectCPU` helper
- 解锁 P5 `incorporate_parent` 真实 late-binding wiring
- 解锁 CoherentXBarTLM snoop broadcast

详见 spec: `docs-archived/superpowers/specs/2026-06-19-p0-fixes-design.md §2`

#### 8.5.2 CoherentXBarTLM

APU 顶层跨域 snoop 广播总线，继承 `CrossbarTLM`。

**当前范围（Phase 7.A/7.B）**：write-through 透传，不下场做 coherence 决策。
**未来（Phase 7.C）**：引入 6×6 state transition switch 表（per ADR-SOC-01 §3）。

详见 spec: `docs-archived/superpowers/specs/2026-06-19-p0-fixes-design.md §3`

#### §8.5.3 P1 ApuSoC::incorporate_parent 真实 Late-Binding (2026-06-19)

D.1 修复 + CoherentXBarTLM 类骨架是 P0 留下的"机制"层。本节落实"机制在 SoC 上下文中工作"。

**机制**:
- `ModuleFactory::instantiateAll` 末尾新增 Step 9: 遍历顶层 `SimModule` 调用 `incorporate_parent(nullptr)`
- `ApuSoC::incorporate_parent` 重写: 找 `xbar` (命名可配置 `coherent_xbar_name` params, 默认 `"xbar"`) → 递归遍历整棵子树 (`CpuCluster`/`GpuCluster`/`GpcCluster`/... ) → 命中 `CacheTLM` 取 D.1 修复后的 `req_out` MasterPort → `xbar->registerPeerCache(path, port)`
- `CoherentXBarTLM::registerPeerCache` 加按名去重 (双层幂等性: `ApuSoC::peer_caches_wired_` 早退 + registerPeerCache find_if 重复检查)
- 软失败策略: xbar/cache/port 缺失仅 `DPRINTF WARN` 不抛异常

**测试覆盖**:
- 4 单元测试 (`[p1]`): 基础 wiring / 深递归 / 幂等 / 无 xbar 软失败
- 1 E2E 测试 (`[e2e][p1]`): apu_soc_v1.json 加载 + 手动 snoop_broadcast 投递

**解锁**:
- 完整 APU SoC 拓扑 snoop broadcast 端到端验证
- 后续 Phase 7.C 6×6 state table 改造 CoherentXBarTLM 时, peer cache 注册路径已就绪

详见 spec: `docs-archived/superpowers/specs/2026-06-20-incorporate-parent-late-binding-design.md`

### 8.7 CI/CD 集成 + 零债务验收 (2026-04-22)

**CI/CD 工作流** (`.github/workflows/ci.yml`):
- Release + Debug 双模式构建
- ctest 测试验证（367 用例全通过）
- 代码格式检查（clang-format）
- ccache 缓存加速
- test artifacts 上传（7 天保留）

**本地提交流程**:
```bash
# 1. 本地构建验证
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure

# 2. 创建特性分支
git checkout -b feature/xxx

# 3. 提交 + 推送
git commit -m "type(scope): 描述"
git push -u origin HEAD

# 4. 创建 PR（GitHub Web UI 或 gh CLI）
# 5. 等待 CI 通过后合并
```

**历史债务清偿**:
- 清除 1 处 TODO 残留 (`module_factory.cc:333`)
- 归档 4 个 `.disabled` 测试文件至 `docs-archived/disabled-tests/`
- 修复 12 个历史失败测试 (通配符端口匹配 / MockConsumer tick 机制 / Crossbar off-by-one)
- 修复 StatGroup 路径重复拼接问题
- 消除核心模块编译器警告（Wreorder / Uninitialized / Unused parameter）
- 测试文件警告全部消除（Legacy 模块保留原样，待后续移除）

**性能基准**: CacheTLM tick 延迟 **5.27 ns/op**

**版本**: v2.1.9
**最后更新**: 2026-04-22
**批准状态**: ✅ Phase 6 完成，CI/CD 集成，零债务验收
**创建者**: Sisyphus (AI Architect)