# 09. dGPU SoC v1.0 Coherence Protocol 架构 — MOESI/GPU 6×6 状态机 + 跨域桥接 + Snoop Filter

> **类别**: SoC Architecture > 子系统架构 (L7 Coherence)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.0 PASS（§3.7 L7.6 Coherence）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/coherence-protocol.md`](../modules/coherence-protocol.md)
> - [`docs/soc_arch/modules/coherence-domain.md`](../modules/coherence-domain.md)
> - [`docs/soc_arch/modules/coherence-bridge.md`](../modules/coherence-bridge.md)
> - [`docs/soc_arch/modules/snoop_filter.md`](../modules/snoop_filter.md)
> - [`docs/soc_arch/modules/coherent_xbar.md`](../modules/coherent_xbar.md)
> - [`docs/soc_arch/modules/cache-protocol.md`](../modules/cache-protocol.md)
> - [`docs/soc_arch/modules/cache-replacement.md`](../modules/cache-replacement.md)
> - [`docs/soc_arch/modules/cache.common.md`](../modules/cache.common.md)
> **关联研究综述**:
> - [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md)（Ruby MSI + MOESI Hammer 随机测试）
> **关联 ADR**: ADR-SOC-01（Coherence 协议分步走策略）/ 00-overview D8（v1.0 Coherence 完整实施）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.7 L7.6 Coherence 子系统的**详细化文档**。

- 想快速理解 Coherence 范围 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 MOESI/GPU 6×6 状态机 → 读 §3
- 想理解分步走策略 → 读 §4(Phase 7.A → 7.B → 7.C → 7.D)
- 想理解 CoherenceDomain → 读 §5
- 想理解 CoherenceBridge → 读 §6
- 想理解 SnoopFilter → 读 §7
- 想理解 CoherentXBar → 读 §8
- 想理解 v1.0 战略对齐 → 读 §9
- 想理解 CppTLM 实施 → 读 §10
- 想理解配置 Schema → 读 §11
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §12
- 想评估风险 → 读 §13

---

## 1. 范围与目标

### 1.1 L7 Coherence 子系统定位

**L7 Coherence** = dGPU SoC v1.0 系统拓扑的**一致性层**,负责:

- **MOESI/GPU 6 状态 × 6 事件**转换(per ADR-SOC-01)
- **CPU↔GPU 跨域 coherence**(APU + dGPU 必需)
- **跨域桥接**:CPU coherence ↔ GPU coherence
- **Snoop Filter 优化**:减少跨域 snoop 流量

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R23-R24)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| Coherence MOESI/GPU | ✅ 6 状态 × 6 事件(per ADR-SOC-01)| 同 v1.0 | D8 |
| Coherence 跨域桥接 | ✅ 基础 | ✅ 完整 + snoop filter 优化 | D8 |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.0 PASS 的 §3.7 L7.6 Coherence + §4-bis 范围矩阵 R23-R24 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

```
                ┌─────────────────────────────────────────────┐
                │            CPU 端                            │
                │  - 缓存(L1/L2/L3)                          │
                │  - coherence domain: COH-CPU                │
                └───────────────────┬─────────────────────────┘
                                        │ snoop 请求 / 数据请求
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │     CoherenceBridge (per ADR-SOC-01)    │
                  │  - 桥接 CPU↔GPU coherence 域              │
                  │  - 转发 snoop 请求                       │
                  │  - 维护跨域地址映射                       │
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ SnoopFilter         │    │ CoherentXBar          │
              │ (减少跨域 snoop 流量)│    │ (per GPU coherence 域)│
              └─────────┬──────────┘    └────────────┬───────────┘
                        │                            │
                        └─────────────┬──────────────┘
                                      │
                                      ▼
                  ┌──────────────────────────────────────────┐
                  │            GPU 端                          │
                  │  - 缓存(L1/L2) + SMEM                   │
                  │  - coherence domain: COH-GPU(MOESI/GPU) │
                  └──────────────────────────────────────────┘

   ──────────────────  GPU 内部 MOESI/GPU 状态机  ──────────────────

   ┌──────────────────────────────────────────────────┐
   │ GPU Coherence Domain(per CoherenceDomain 模块)  │
   │ ┌─────────────────────────────────────────────┐ │
   │ │ 6 状态:I/S/E/M/O/T                         │ │
   │ │ 6 事件:Read/Write/Invalidate/InvalidateAck/Flush/FlushAck│
   │ │ state_transition[6][6] 转换表(C++ switch)   │ │
   │ └─────────────────────────────────────────────┘ │
   │ ┌─────────────────────────────────────────────┐ │
   │ │ snoop callback → CacheTLM 集成              │ │
   │ └─────────────────────────────────────────────┘ │
   └──────────────────────────────────────────────────┘
```

---

## 3. MOESI/GPU 6 状态 × 6 事件状态机(per ADR-SOC-01)

### 3.1 6 状态定义

**MOESI 协议**(Modified / Owner / Exclusive / Shared / Invalid):

| 状态 | 缩写 | 含义 |
|------|------|------|
| **Invalid** | I | 该 cache line 无效 |
| **Shared** | S | 共享读,可能有多个 cache 持有 |
| **Exclusive** | E | 独占读,内存与 cache 一致 |
| **Modified** | M | 已修改,独占写 |
| **Owned** | O | 已拥有,可能与其他 cache 共享(脏)|
| **Temporary** | T | 临时状态(过渡) |

### 3.2 6 事件定义

| 事件 | 缩写 | 含义 |
|------|------|------|
| **Read** | Rd | 读请求 |
| **Write** | Wr | 写请求 |
| **Invalidate** | Inv | 使其他 cache line 无效 |
| **Invalidate Ack** | InvAck | 无效确认 |
| **Flush** | Fl | 把脏数据写回内存 |
| **Flush Ack** | FlAck | Flush 确认 |

### 3.3 状态转换表(per ADR-SOC-01 §2（决策）)

**`uint8_t state_transition[6][6]`**(6 状态 × 6 事件):

```cpp
// 状态枚举: 0=I, 1=S, 2=E, 3=M, 4=O, 5=T
// 事件枚举: 0=Rd, 1=Wr, 2=Inv, 3=InvAck, 4=Fl, 5=FlAck

uint8_t state_transition[6][6] = {
    // Rd      Wr      Inv     InvAck  Fl      FlAck
    { 1,      3,      0,      0,      0,      0  },   // I + event
    { 1,      3,      0,      1,      4,      1  },   // S + event
    { 2,      3,      0,      2,      4,      2  },   // E + event
    { 3,      3,      3,      3,      3,      3  },   // M + event
    { 4,      3,      0,      4,      4,      4  },   // O + event
    { 1,      3,      0,      1,      4,      1  },   // T + event (过渡回 S/M)
};
```

**CppTLM 实施**:`C++ switch` 表驱动状态转换(per ADR-SOC-01 §2（决策）)

### 3.4 与 gem5 Ruby MOESI Hammer 对照

**Gem5 Ruby MOESI Hammer** 是 CppTLM CoherenceDomain 的设计参考(per `docs/research/gem5-soc-survey.md` §2.5):

- `configs/example/ruby_random_test.py`:MOESI Hammer 随机测试
- `configs/example/ruby_mem_test.py`:Ruby + MemTest + DMA
- `configs/example/ruby_direct_test.py`:Ruby Directed Tester

**关键差异**:
- **CppTLM 简化**:状态转换用 `switch` 表(per ADR-SOC-01);不复制 gem5 slicc DSL(5000+ 行不可读)
- **CppTLM 集成**:`CoherenceDomain` 与 `CacheTLM` 通过 snoop callback 集成

---

## 4. 分步走策略(per ADR-SOC-01)

### 4.1 Phase 7.A–7.B:黑盒 GPU 阶段

**Phase 7.A–7.B 策略**(per ADR-SOC-01 §2（决策）):

- GPU 请求走 **write-through 直写策略**
- `CacheTLM` **不需要 protocol-aware 改造**
- GPU 请求**不缓存在 L1/L2**,直接穿透至 `MemoryTLM`

**优势**:
- 实施简单
- 避免 coherence 复杂度
- 性能足够 APU 形态

**劣势**:
- GPU 写延迟高(L1 cache 未命中)
- CPU↔GPU coherence 较弱

### 4.2 Phase 7.C:Coherence 集成阶段

**Phase 7.C 策略**(per ADR-SOC-01 §2（决策）):

- 将 `CacheTLM` 升级为 **protocol-aware**
- 引入 **6 状态 × 6 事件**转换表(`uint8_t state_transition[6][6]`)
- `CoherenceDomain` 与 `CacheTLM` 通过 **snoop callback** 集成

**实施要点**:
- MOESI/GPU 协议在 GPU 端 L1/L2 启用
- CPU 端保留原有 MOESI(MESI 简化或 MOESI)
- 跨域桥接(`CoherenceBridge`)负责 CPU↔GPU 协议转换

### 4.3 Phase 7.D:Cache Protocol 升级(推迟)

**Phase 7.D 计划**(per ADR-SOC-01):

- 完整 Cache Protocol 升级
- 推断 Phase 7.D 涉及:Per-line state transition + directory-based + advanced prefetch
- **当前状态**:v1.0 推迟(per ADR-SOC-01 Status Update + `00-overview` §4-bis R23/R24)

**注**:`00-overview` v3.0 §4-bis R23-R24 中,Phase 7.D 不显式列入 v1.0 范围,推迟至后续版本（由 ADR-SOC-01 Status Update 跟踪）。

### 4.4 永不复制 gem5 slicc DSL

**per ADR-SOC-01 §2（决策）**:
- **slicc 是 gem5 特定语言、5000+ 行不可读**
- **CppTLM 用 C++ `switch` 表驱动状态转换**
- **避免维护负担**(不学习 slicc DSL 语法)

---

## 5. CoherenceDomain

### 5.1 角色

**CoherenceDomain** 模块(per `coherence-domain.md`):

- 维护 cache line 状态表(per address)
- 响应 snoop 请求
- 触发 cache line 状态转换
- 与 `CacheTLM` 通过 snoop callback 集成

### 5.2 关键 API

```cpp
class CoherenceDomain {  // 目标 API；当前实现接口见 include/core/coherence_domain.hh
public:
    enum class State { I, S, E, M, O, T };
    enum class Event { Read, Write, Invalidate, InvalidateAck, Flush, FlushAck };
    
    State transition(State current, Event event);
    void on_snoop(uint64_t addr, Event event, TransactionId txn_id);
    void invalidate_range(uint64_t addr, size_t len);
    
    // 集成 CacheTLM
    void register_cache_callback(CacheTLM* cache);
};
```

### 5.3 CppTLM 集成(per ADR-SOC-01 §2（决策）)

- **Phase 7.C 实施**:snoop callback → CacheTLM 集成
- **6 状态 × 6 事件** 转换表(`uint8_t state_transition[6][6]`)
- **永不复制 gem5 slicc DSL**

---

## 6. CoherenceBridge(跨域桥接)

### 6.1 角色

**CoherenceBridge** 模块(per `coherence-bridge.md`):

- 桥接 CPU coherence domain 与 GPU coherence domain
- 转发 snoop 请求跨域
- 维护跨域地址映射(可能涉及 IOMMU)
- 处理跨域 Flush

### 6.2 跨域协议

**典型流程**:
1. CPU 读 GPU 数据
2. Bridge 转发 snoop 请求
3. GPU CoherenceDomain 执行状态转换
4. GPU 返回响应
5. CPU 收到 GPU 数据并完成

**典型流程**(GPU 写):
1. GPU 写 → GPU CacheTLM 状态变更(M)
2. Bridge 收到 Invalid/Flush 请求
3. Bridge 转发至 CPU CoherenceDomain
4. CPU CacheTLM 执行 snoop
5. CPU CacheTLM 状态变更(I)
6. CPU 返回 Invalidate/Flush Ack
7. Bridge 返回 Ack

### 6.3 v1.0 MVP / v1.1 范围

- **v1.0 MVP 基础**:Bridge 跨域转发 + 简单地址映射
- **v1.1 完整版**:完整 snoop 协议 + IOMMU 集成 + 性能优化

---

## 7. SnoopFilter

### 7.1 角色

**SnoopFilter** 模块(per `snoop_filter.md`):

- per-domain snoop 请求记录
- 减少跨域 snoop 流量(避免每次 snoop 广播所有 cache)
- 跟踪哪些 cache 持有某 line 的拷贝

### 7.2 关键数据结构

```cpp
class SnoopFilter {
public:
    // per-address snoop 跟踪
    struct SnoopEntry {
        uint64_t addr;
        std::vector<uint16_t> sharers;  // cache ID list
        State shared_state;
    };
    
    void record_sharer(uint64_t addr, uint16_t cache_id);
    void remove_sharer(uint64_t addr, uint16_t cache_id);
    std::vector<uint16_t> get_sharers(uint64_t addr);
};
```

### 7.3 v1.0 MVP / v1.1 范围

- **v1.0 MVP 基础**:per-domain snoop 跟踪
- **v1.1 完整版**:跨域 snoop 优化 + directory-based coherence(可选)

---

## 8. CoherentXBar

### 8.1 角色

**CoherentXBar** 模块(per `coherent_xbar.md`):

- 继承 `CrossbarTLM`
- 扩展支持 coherence 协议(每端口带 domain ID)
- 处理 coherence 请求的路由

### 8.2 与 CrossbarTLM 差异

| 维度 | CrossbarTLM | CoherentXBar |
|------|-------------|--------------|
| coherence 支持 | ❌ | ✅ |
| Domain ID | 无 | 每端口 domain_id |
| Snoop 路由 | ❌ | ✅ 自动 snoop 广播 |
| Flush 协议 | 无 | FlushAck 时机 |

### 8.3 CppTLM 集成

- **Phase 7.C 实施**:CacheTLM 升级为协议感知(per ADR-SOC-01 §2（决策）)
- **v1.0 MVP 基础**:CoherentXBar + CoherenceDomain + Bridge
- **v1.1 完整版**:完整协议 + SnoopFilter 优化

---

## 9. v1.0 战略对齐

### 9.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| MOESI/GPU 6×6 状态机 | ✅ per ADR-SOC-01 | ✅ §3 |
| 分步走 Phase 7.A→7.C | ✅ write-through → protocol-aware | ✅ §4 |
| CoherenceDomain | ✅ | ✅ §5 |
| CoherenceBridge | ✅ 基础 → 完整 v1.1 | ✅ §6 |
| SnoopFilter | ✅ 基础 → 优化 v1.1 | ✅ §7 |
| CoherentXBar | ✅ | ✅ §8 |

### 9.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| MOESI/GPU 6×6 状态机 | ✅ | ✅ |
| CoherenceDomain | ✅ | ✅ + 优化 |
| CoherenceBridge | ✅ 基础 | ✅ 完整 + IOMMU |
| SnoopFilter | ✅ 基础 | ✅ 完整 + directory-based |
| CoherentXBar | ✅ | ✅ + 性能优化 |
| Phase 7.D(Cache Protocol 升级)| ❌ 推迟 | ✅ 完整 |

### 9.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-01 | Coherence 协议分步走策略 |
| 00-overview D8 | v1.0 Coherence 完整实施 |

---

## 10. CppTLM 实施与目标接口

### 10.1 模块清单

| 模块 | 路径 | 角色 |
|------|------|------|
| **CoherenceProtocol** | （待新建）`include/core/coherence_protocol.hh` | MOESI/GPU 6×6 目标状态机 |
| **CoherenceDomain** | `include/core/coherence_domain.hh` | per-cache domain |
| **CoherenceBridge** | （待新建）`include/core/coherence_bridge.hh` | CPU↔GPU 跨域桥接目标模块 |
| **SnoopFilter** | （待新建）`include/core/snoop_filter.hh` | snoop 过滤目标模块 |
| **CoherentXBar** | `include/tlm/coherent_xbar_tlm.hh` | 相干 Crossbar |

### 10.2 关键共享方法

```cpp
class CoherenceProtocol {
public:
    static State transition(State current, Event event);
    static uint8_t state_transition[6][6];
};

class CoherenceDomain {
public:
    void on_snoop(uint64_t addr, Event event);
    void invalidate_range(uint64_t addr, size_t len);
};

class CoherenceBridge {
public:
    void forward_cpu_to_gpu(uint64_t addr, Event event);
    void forward_gpu_to_cpu(uint64_t addr, Event event);
};
```

### 10.3 v1.0 MVP 简化路径

- **MOESI/GPU 6×6 状态机**:`CoherenceProtocol::transition()`
- **CacheTLM protocol-aware**:`on_snoop` callback
- **CoherenceBridge**:`forward_cpu_to_gpu / forward_gpu_to_cpu`
- **SnoopFilter 基础**:`record_sharer / remove_sharer`
- **CoherentXBar**:`CrossbarTLM` + `domain_id` 扩展

---

## 11. 配置 Schema

```json
{
  "name": "coherence_system_0",
  "type": "CoherenceDomain",
  "params": {
    "protocol": "moesi_gpu",         // MOESI/GPU 6 状态 × 6 事件
    "phase": "7_C",                 // 7_A (write-through) | 7_C (protocol-aware) | 7_D (完整)
    
    "cpu_domain_id": 0,
    "gpu_domain_id": 1,
    
    "enable_bridge": true,
    "bridge_iommu": false,          // v1.1 完整版
    
    "enable_snoop_filter": true,
    "snoop_filter_max_entries": 8192,
    
    "enable_coherent_xbar": true,
    "xbar_ports": 4,
    
    "cache_tlm_integration": "snoop_callback",  // per ADR-SOC-01
    "state_transition_table": "switch"          // per ADR-SOC-01(非 gem5 slicc)
  }
}
```

---

## 12. ADR/微架构/OpenSpec 引用矩阵

### 12.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-01 | Coherence 协议分步走策略 + MOESI/GPU 6×6 |
| 00-overview D8 | v1.0 Coherence 完整实施 |

### 12.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **CoherenceProtocol** | [`docs/soc_arch/modules/coherence-protocol.md`](../modules/coherence-protocol.md) |
| **CoherenceDomain** | [`docs/soc_arch/modules/coherence-domain.md`](../modules/coherence-domain.md) |
| **CoherenceBridge** | [`docs/soc_arch/modules/coherence-bridge.md`](../modules/coherence-bridge.md) |
| **SnoopFilter** | [`docs/soc_arch/modules/snoop_filter.md`](../modules/snoop_filter.md) |
| **CoherentXBar** | [`docs/soc_arch/modules/coherent_xbar.md`](../modules/coherent_xbar.md) |
| **CacheProtocol** | [`docs/soc_arch/modules/cache-protocol.md`](../modules/cache-protocol.md) |
| **CacheReplacement** | [`docs/soc_arch/modules/cache-replacement.md`](../modules/cache-replacement.md) |
| **CacheCommon** | [`docs/soc_arch/modules/cache.common.md`](../modules/cache.common.md) |

### 12.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md) | Gem5 Ruby MSI + MOESI Hammer 随机测试 |

---

## 13. 风险与缓解 R1-R5

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | MOESI/GPU 状态转换表 `state_transition[6][6]` 与真实硬件一致性 | 🟡 中 | v1.0 MVP 简化版;v1.1 完整版根据硬件验证 |
| **R2** | 跨域 snoop 性能开销(CPU↔GPU 同步) | 🟡 中 | v1.0 MVP 基础;v1.1 SnoopFilter 优化 |
| **R3** | CoherenceBridge 复杂状态机(CPU 端 MOESI + GPU 端 MOESI/GPU) | 🟡 中 | C++ switch 表驱动(per ADR-SOC-01)|
| **R4** | Phase 7.D Cache Protocol 升级工作量 | 🟢 低 | 推迟至 v1.1 完整版;v1.0 Phase 7.C 足够 |
| **R5** | IOMMU 集成(per AMD ROCm + NVIDIA HMM) | 🟡 中 | v1.1 完整版追加;v1.0 MVP 基础 |

---

## 14. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L7 Coherence 子系统架构,基于 MOESI/GPU 6×6 + CoherenceDomain + Bridge + SnoopFilter + CoherentXBar + gem5 Ruby MOESI 参照) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS