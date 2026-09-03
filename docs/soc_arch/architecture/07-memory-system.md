# 07. dGPU SoC v1.0 Memory System 架构 — HBM3e + L1/L2 + SMEM + DSMEM + TMEM + Coherence

> **类别**: SoC Architecture > 子系统架构 (L7 Memory Hierarchy + Coherence)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（§3.7 L7 Memory Hierarchy + Coherence 层）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/cache-l1.md`](../modules/cache-l1.md)/[`cache-l2.md`](../modules/cache-l2.md)/[`cache-protocol.md`](../modules/cache-protocol.md)
> - [`docs/soc_arch/modules/gpu-shared-memory.md`](../modules/gpu-shared-memory.md)
> - [`docs/soc_arch/modules/gpu-memory-cluster.md`](../modules/gpu-memory-cluster.md)
> - [`docs/soc_arch/modules/memory-hbm.md`](../modules/memory-hbm.md)/[`memory-dram.md`](../modules/memory-dram.md)
> - [`docs/soc_arch/modules/coherence-protocol.md`](../modules/coherence-protocol.md)/[`coherence-domain.md`](../modules/coherence-domain.md)/[`coherence-bridge.md`](../modules/coherence-bridge.md)/[`snoop_filter.md`](../modules/snoop_filter.md)
> **关联研究综述**:
> - [`docs/research/SM/overview.md`](../../research/SM/overview.md)（NVIDIA Hopper/Blackwell DSMEM/TMEM）
> - [`docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md`](../../research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md)
> - [`docs/research/SM/CudaCore/arXiv2402.13499_Hopper微基准剖析_解析.md`](../../research/SM/CudaCore/arXiv2402.13499_Hopper微基准剖析_解析.md)
> **关联 ADR**: ADR-SOC-01（Coherence 协议策略）/ 00-overview D8（v1.0 Coherence 完整）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.7 L7 Memory Hierarchy + Coherence 层的**子系统架构**详细化文档。

- 想快速理解 L7 层结构 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 HBM3e 控制器 → 读 §3
- 想理解 L2 共享 Cache → 读 §4
- 想理解 L1 + Shared Memory / LDS → 读 §5
- 想理解 DSMEM 分布式共享 → 读 §6
- 想理解 TMEM 张量内存 → 读 §7
- 想理解 Coherence 协议 → 读 §8
- 想理解 v1.0 战略对齐 → 读 §9
- 想理解 CppTLM 实施 → 读 §10
- 想理解配置 Schema → 读 §11
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §12
- 想评估风险 → 读 §13

---

## 1. 范围与目标

### 1.1 L7 Memory Hierarchy + Coherence 层定位

**L7 层** = dGPU SoC v1.0 系统拓扑的**存储与一致性层**,负责:

- **HBM3e 控制器**:多通道 HBM 管理 + RAS
- **L2 共享 Cache**:跨 SM/CU 共享 L2
- **L1 Cache + Shared Memory / LDS**:每 SM/CU 本地高速缓存
- **DSMEM**(Hopper/Blackwell 独有,v1.1):跨 SM 协作共享内存
- **TMEM**(Blackwell 独有,v1.1):张量运算中间结果
- **Coherence**:MOESI/GPU 协议 + 跨域桥接 + SnoopFilter

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R18-R24)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| HBM3e Controller | ✅ 简化(192-270GB NVIDIA / 192GB AMD) | + 多通道 HBM + RAS | D4 |
| L2 Cache | ✅ 共享 + 跨域桥接基础 | + 优化 + snoop filter | D4 / D8 |
| L1 + Shared Memory | ✅ 基础 | + 优化 | D4 |
| DSMEM | ❌ 推迟 | ✅ per US20250173152A1 | D4 |
| TMEM | ❌ 推迟 | ✅ per arXiv 2512.02189(256KB/SM)| D4 |
| Coherence MOESI/GPU | ✅ 6 状态 × 6 事件(per ADR-SOC-01)| 同 v1.0 | D8 |
| Coherence 跨域桥接 | ✅ 基础 | ✅ 完整 + snoop filter 优化 | D8 |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.1 PASS 的 §3.7 L7 + §4-bis 范围矩阵 R18-R24 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

```
                    ┌─────────────────────────────────────────────┐
                    │            L6 SM/CU 内核                   │
                    │  - Warp × 4 / Wavefront                     │
                    │  - RegFile(256KB/SM)                       │
                    │  - Tensor Core                              │
                    └───────────────────┬─────────────────────────┘
                                        │
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │  L1 + Shared Memory / LDS                 │
                  │  - 228KB Hopper / 99KB Blackwell SM_120  │
                  │  - 64KB LDS(AMD MI300X)                  │
                  │  - Unified L1/SMEM Cache                  │
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ SM-to-SM 通信    │    │ DSMEM (v1.1)          │
              │ (warp 屏障等)    │    │ 跨 SM 共享 + CGABAR   │
              │                  │    │ (per US20250173152A1) │
              └─────────┬──────────┘    └────────────┬───────────┘
                        │                            │
                        └─────────────┬──────────────┘
                                      │
                                      ▼
                  ┌──────────────────────────────────────────┐
                  │  L2 共享 Cache                            │
                  │  - NVIDIA B200: ~60MB/die                │
                  │  - AMD MI300X: ~8MB/XCD + Infinity Cache 256MB(L3)│
                  │  - Crossbar 路由                          │
                  │  - Coherence MOESI/GPU                    │
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ Snoop Filter       │    │ Coherence Bridge      │
              │ (per ADR-SOC-01)   │    │ (CPU↔GPU coherence)   │
              └─────────┬──────────┘    └────────────┬───────────┘
                        │                            │
                        └─────────────┬──────────────┘
                                      │
                                      ▼
                  ┌──────────────────────────────────────────┐
                  │  HBM3e Controller                        │
                  │  - NVIDIA: 192-270GB, 7.7-8 TB/s         │
                  │  - AMD MI300X: 192GB HBM3, 5.3 TB/s      │
                  │  - Multi-channel + RAS                   │
                  └──────────────────────────────────────────┘

   ──────────────────  TMEM 张量内存 (v1.1)  ──────────────────

   Blackwell B200:
   SM TMEM 256KB(512×128 lane, 16 TB/s 读)
       ↑↓ per-tile
   Tensor Core(tcgen05.mma)
```

---

## 3. HBM3e Controller

### 3.1 NVIDIA Blackwell HBM3e

**关键参数**(per `docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md` Table 3):

| 维度 | HGX B300(Blackwell Ultra)| HGX B200(Blackwell)|
|------|-------------------------|---------------------|
| 显存 | **270 GB HBM3e @ 7.7 TB/s** | 最高 **192 GB HBM3e @ 7.7 TB/s** |
| 最大 TDP | 1100 W | 1000 W |
| 互连 | NVLink 5 + PCIe Gen6 | NVLink 5 + PCIe Gen5 |

**双 die 架构**:
- GB100/B200:两片 die 均达光罩尺寸上限
- 经 10 TB/s 片间 NV-HBI(NVIDIA High-Bandwidth Interface)
- "providing one fully coherent chip"

### 3.2 AMD CDNA 3 MI300X

**关键参数**(per `00-overview` §3.7 L7.1):

| 维度 | MI300X |
|------|--------|
| HBM | 192GB HBM3 |
| 带宽 | 5.3 TB/s |
| 拓扑 | 8 XCD chiplets × 38 CU = 304 CU |
| Infinity Cache | 256MB(L3 级,全卡共享)|

### 3.3 CppTLM 实施

- **`MemoryCluster` 模块**:多通道 HBM 控制器(per `gpu-memory-cluster.md`)
- **`MemoryTLM` 模块**:DRAM 简化模型(per `memory-dram.md`)
- **HBM3e 控制器**:v1.0 MVP 简化;v1.1 完整版追加 RAS + 多通道优化

---

## 4. L2 共享 Cache

### 4.1 NVIDIA Blackwell B200 L2

**关键参数**:
- **~60MB L2 per die**(公开口径)
- 跨 die 共享(per NV-HBI 10 TB/s)
- L2 分区:per S3 多 cache slice(per arXiv 2512.02189)

### 4.2 AMD MI300X L2/L3

**关键参数**(per `00-overview` §3.7 L7.2):
- **~8MB L2 per XCD**(per XCD chiplet)
- **Infinity Cache 256MB(L3 级,全卡共享)**
- L2 与 Infinity Cache 分层表述(避免混淆)

### 4.3 CppTLM 实施

- **`CacheCluster` 模块**:L2 聚合
- **`CoherentXBar` 模块**:L2 ↔ L1/SMEM 路由
- **`crossbar_tlm`**(in `include/tlm/crossbar_tlm.hh`)

### 4.4 跨域桥接

- **CPU↔GPU coherence**(per ADR-SOC-01)
- `CoherenceDomain` + `CoherenceBridge`
- v1.0 MVP 基础;v1.1 完整版追加完整跨域 + snoop filter

---

## 5. L1 + Shared Memory / LDS

### 5.1 NVIDIA L1/SMEM 统一缓存

| 架构 | 共享内存 |
|------|---------|
| Hopper SM_90 | 228KB/SM(L1/SMEM 统一)|
| Blackwell SM_100(B200 数据中心)| 228KB/SM |
| Blackwell SM_120(GB203 消费级)| **≈99KB/SM**(L1 容量减半)|
| AMD MI300X CDNA 3 | **64KB LDS per CU** |

**L1 Cache 容量**:
- Hopper/B200:与 SMEM 统一,共 228KB
- GB203 消费级:L1/SMEM 统一缓存容量减半

### 5.2 AMD LDS(Local Data Share)

**关键参数**:
- 64KB LDS per CU
- 由 MFMA 等张量运算共享输入数据
- 类似 NVIDIA SMEM 但容量较小

### 5.3 CppTLM 实施

- **`CacheTLM` 模块**:L1 + SMEM/LDS 统一
- **`SharedMemoryTLM` 模块**:NVIDIA SMEM
- **`gpu-shared-memory.md`**:完整模块微架构

---

## 6. DSMEM 分布式共享(per US20250173152A1,v1.1)

### 6.1 关键机制

**Distributed Shared Memory (DSMEM)** 是 Hopper/Blackwell 独有的跨 SM 协作共享内存:

- **GPC 内 CGA(cluster)各 SM**的本地共享内存经**专用 SM2SM 网络**(GPCARB/GXBAR,与 MXBAR/L2 路径物理隔离)互访
- **4GB/256×16MB CTAID 分段地址窗口**
- 源侧两级路由表 + 目的侧 ShMemBase CAM
- **uTLB 地址 hash**
- **phase 翻转 MEMBAR** 与合并写确认
- **CGABAR.ARRIVE/WAIT**:集群屏障与 CGA 退出/flush 协议

### 6.2 性能参数(per arXiv 2402.13499 Hopper 实测)

| 参数 | 值 |
|------|-----|
| SM-to-SM 延迟 | **180 clocks** |
| 相对 L2 | **低于 L2 32%** |
| 网络 | 专用 GPCARB/GXBAR(物理隔离)|

### 6.3 CppTLM 实施

- **v1.0 MVP 不实施**(per `00-overview` §4-bis R21)
- **v1.1 完整版**:`DSMEM` 模块 + ShMemBase CAM + uTLB + CGABAR 协议

---

## 7. TMEM 张量内存(per arXiv 2512.02189,v1.1)

### 7.1 关键参数(B200 实测)

**TMEM(Tensor Memory)** 是 Blackwell SM 新设的张量内存:

| 参数 | 值 |
|------|-----|
| 容量 | **256KB/SM** |
| 组织 | **512 列 × 128 lane** |
| 读带宽 | **~16 TB/s** |
| 最优 tile | 64×64 |
| 操作码 | `tcgen05.ld/st/cp` 显式管理 |

### 7.2 角色与优势

- **张量中间结果存储**:MMA 计算中间结果无需写回通用寄存器
- **CTA pair 共享**:相邻 rank CTA 通过 TMEM 共享操作数
- **warp 取消**:per-thread 调度不需要 warp 级协调
- **延迟恒定 ≈11 cycles**(per arXiv 2512.02189 实测)

### 7.3 CppTLM 实施

- **v1.0 MVP 不实施**(per `00-overview` §4-bis R22)
- **v1.1 完整版**:`TMEM` 模块 + 256KB/SM + 16 TB/s 读带宽 + tcgen05.ld/st/cp API

---

## 8. Coherence 协议

### 8.1 MOESI/GPU 6 状态 × 6 事件(per ADR-SOC-01)

**Coherence Protocol Stepwise Strategy**(per `docs/soc_arch/adr/ADR-SOC-01-coherence-protocol-strategy.md`):

- **Phase 7.A–7.B(黑盒 GPU 阶段)**:GPU 请求走 write-through 直写策略,CacheTLM 不需要 protocol-aware 改造。GPU 请求不缓存在 L1/L2,直接穿透至 MemoryTLM。
- **Phase 7.C(Coherence 集成阶段)**:将 CacheTLM 升级为 protocol-aware,引入 **6 状态 × 6 事件**转换表(`uint8_t state_transition[6][6]`)。`CoherenceDomain` 与 CacheTLM 通过 snoop callback 集成。

**永不复制 gem5 slicc DSL**(per ADR-SOC-01):slicc 是 gem5 特定语言、5000+ 行不可读。CppTLM 用 C++ `switch` 表驱动状态转换。

### 8.2 6 状态 × 6 事件转换

**6 状态**:I / S / E / M / O / T(per `coherence-protocol.md`)
**6 事件**:Read / Write / Invalidate / InvalidateAck / Flush / FlushAck

### 8.3 跨域桥接

**CoherenceBridge** 负责:
- CPU coherence 域 ↔ GPU coherence 域
- snoop 请求跨域转发
- v1.0 MVP 基础;v1.1 完整版追加完整 snoop filter

**SnoopFilter**:
- per-domain snoop 请求记录
- 减少跨域 snoop 流量
- v1.0 MVP 基础;v1.1 完整版优化

### 8.4 CppTLM 实施

- **`CoherenceDomain` 模块**(per `coherence-domain.md`)
- **`CoherenceBridge` 模块**(per `coherence-bridge.md`)
- **`CoherenceProtocol` 模块**(per `coherence-protocol.md`)
- **`SnoopFilter` 模块**(per `snoop_filter.md`)
- **`CoherentXBar` 模块**(per `coherent_xbar.md`)

---

## 9. v1.0 战略对齐

### 9.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| HBM3e Controller v1.0 | ✅ 简化(192-270GB NVIDIA / 192GB AMD)| ✅ §3 |
| L2 Cache | ✅ 共享 + 跨域桥接基础 | ✅ §4 |
| L1 + Shared Memory | ✅ 基础 | ✅ §5 |
| DSMEM v1.1 | 🔵 US20250173152A1 | ✅ §6 |
| TMEM v1.1 | 🔵 arXiv 2512.02189 | ✅ §7 |
| Coherence MOESI/GPU | ✅ 6 状态 × 6 事件 | ✅ §8 |
| Coherence 跨域桥接 | ✅ 基础 → 完整 v1.1 | ✅ §8.3 |

### 9.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| HBM3e Controller | ✅ 简化 | + 多通道 + RAS |
| L2 Cache | ✅ 共享 | + 优化 |
| L1 + SMEM/LDS | ✅ 基础 | + 优化 |
| DSMEM | ❌ | ✅ per US20250173152A1 |
| TMEM | ❌ | ✅ 256KB/SM, 16 TB/s 读 |
| MOESI/GPU 6×6 | ✅ | ✅ |
| Coherence 跨域桥接 | ✅ 基础 | ✅ 完整 |
| Snoop Filter | ✅ 基础 | + 优化 |

### 9.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-01 | Coherence 协议分步走策略(Phase 7.A→7.B→7.C)|
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor 战略,Coherence 跨域桥接 |

---

## 10. CppTLM 实施

### 10.1 模块清单

| 模块 | 路径 | 角色 |
|------|------|------|
| **MemoryCluster** | `include/tlm/gpu/memory_cluster_tlm.hh` | 多通道 HBM 控制器 |
| **MemoryTLM** | `include/tlm/memory_tlm.hh` | DRAM 简化模型 |
| **CacheCluster** | `include/tlm/cluster/cache_cluster.hh` | L2 聚合 |
| **CacheTLM** | `include/tlm/cache_tlm.hh` | L1 + SMEM/LDS |
| **SharedMemoryTLM** | `include/tlm/gpu/shared_memory_tlm.hh` | NVIDIA SMEM |
| **CoherenceDomain** | `include/core/coherence_domain.hh` | 协议域 |
| **CoherenceBridge** | (待新建)`include/core/coherence_bridge.hh` | 跨域桥接(v1.0 目标接口) |
| **CoherenceProtocol** | (待新建)`include/core/coherence_protocol.hh` | MOESI/GPU 协议 |
| **SnoopFilter** | (待新建)`include/core/snoop_filter.hh` | snoop 过滤 |
| **CoherentXBar** | `include/tlm/coherent_xbar_tlm.hh` | 相干路由器 |
| **DSMEM**(v1.1) | (待新建) | 分布式共享内存 |
| **TMEM**(v1.1) | (待新建) | 张量内存 |

### 10.2 关键共享方法

```cpp
class MemoryCluster {
public:
    void tick();  // HBM 控制器调度
    bool read(uint64_t addr, void* buf, size_t len);
    bool write(uint64_t addr, const void* buf, size_t len);
};

class CoherenceDomain {
public:
    State transition(State current, Event event);  // 6 状态 × 6 事件
    void snoop(TransactionId txn_id);
    void invalidate_range(uint64_t addr, size_t len);
};

class CoherenceBridge {
public:
    void forward_to_cpu(TransactionId txn_id, const Bundle& req);
    void forward_to_gpu(TransactionId txn_id, const Bundle& req);
};
```

---

## 11. 配置 Schema

```json
{
  "name": "memory_system_0",
  "type": "MemoryCluster",
  "params": {
    "vendor_mode": "nv_mode",          // "nv_mode" | "amd_mode"
    
    // HBM3e 参数
    "hbm3e_capacity_gb": 192,           // NVIDIA: 192-270; AMD: 192
    "hbm3e_bandwidth_tbps": 7.7,        // TB/s
    "hbm3e_channels": 12,               // HBM 通道数
    "hbm3e_ras_enabled": false,         // v1.1 完整版
    
    // L2 Cache
    "l2_size_mb": 60,                   // NVIDIA B200 ~60MB/die; AMD ~8MB/XCD
    "l2_associativity": 16,
    "l2_line_size": 128,                // bytes
    
    // L1 + SMEM
    "l1_smem_kb": 228,                  // Hopper/B200: 228; SM_120: 99
    "amd_lds_kb": 64,                   // AMD MI300X LDS per CU
    
    // Coherence
    "coherence_protocol": "moesi_gpu",  // MOESI/GPU 6 状态 × 6 事件
    "snoop_filter_enabled": true,
    "coherence_bridge_enabled": true,
    
    // v1.1 推迟项
    "enable_dsmem": false,
    "enable_tmem": false
  }
}
```

---

## 12. ADR/微架构/OpenSpec 引用矩阵

### 12.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-01 | Coherence 协议分步走策略 |
| 00-overview D8 / ADR-SOC-09 D4 | v1.0 Coherence 完整实施 |

### 12.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **MemoryCluster** | [`docs/soc_arch/modules/gpu-memory-cluster.md`](../modules/gpu-memory-cluster.md) |
| **MemoryTLM** | [`docs/soc_arch/modules/memory-memtlm.md`](../modules/memory-memtlm.md) |
| **HBM** | [`docs/soc_arch/modules/memory-hbm.md`](../modules/memory-hbm.md) |
| **DRAM** | [`docs/soc_arch/modules/memory-dram.md`](../modules/memory-dram.md) |
| **CacheCluster** | [`docs/soc_arch/modules/cache-l1.md`](../modules/cache-l1.md)/[`cache-l2.md`](../modules/cache-l2.md) |
| **CacheProtocol** | [`docs/soc_arch/modules/cache-protocol.md`](../modules/cache-protocol.md) |
| **SharedMemory** | [`docs/soc_arch/modules/gpu-shared-memory.md`](../modules/gpu-shared-memory.md) |
| **CoherenceProtocol** | [`docs/soc_arch/modules/coherence-protocol.md`](../modules/coherence-protocol.md) |
| **CoherenceDomain** | [`docs/soc_arch/modules/coherence-domain.md`](../modules/coherence-domain.md) |
| **CoherenceBridge** | [`docs/soc_arch/modules/coherence-bridge.md`](../modules/coherence-bridge.md) |
| **SnoopFilter** | [`docs/soc_arch/modules/snoop_filter.md`](../modules/snoop_filter.md) |
| **CoherentXBar** | [`docs/soc_arch/modules/coherent_xbar.md`](../modules/coherent_xbar.md) |

### 12.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/SM/overview.md`](../../research/SM/overview.md) | NVIDIA Hopper/Blackwell DSMEM/TMEM |
| [`docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md`](../../research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md) | Blackwell 显存规格 |
| [`docs/research/SM/CudaCore/arXiv2402.13499_Hopper微基准剖析_解析.md`](../../research/SM/CudaCore/arXiv2402.13499_Hopper微基准剖析_解析.md) | DSMEM 180 clocks SM-to-SM |
| [`docs/research/SM/CudaCore/arXiv2512.02189_Blackwell微基准深入_解析.md`](../../research/SM/CudaCore/arXiv2512.02189_Blackwell微基准深入_解析.md) | TMEM 256KB/SM, 16 TB/s |
| [`docs/research/SM/TMA/US20250173152A1_分布式共享内存_解析.md`](../../research/SM/TMA/US20250173152A1_分布式共享内存_解析.md) | DSMEM 专利 |
| [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md) | gem5 Memory 集成(DRAMSim2, sc_main, hmc)|

---

## 13. 风险与缓解 R1-R6

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | B200 L2 容量不同口径(60MB per die vs 120MB total) | 🟢 低 | v1.0 MVP 取 60MB per die;v1.1 完整版根据公开口径调整 |
| **R2** | GB203 消费级 SMEM 减半(≈99KB vs Hopper 228KB) | 🟡 中 | v1.0 MVP 默认数据中心 B200;消费级 v1.1 完整版 |
| **R3** | DSMEM SM-to-SM 180 clocks 延迟建模 | 🟡 中 | v1.0 MVP 不实施;v1.1 基于 arXiv 2402.13499 实测 |
| **R4** | TMEM 256KB/SM 显式管理 API(tcgen05.ld/st/cp) | 🟡 中 | v1.0 MVP 不实施;v1.1 完整版 |
| **R5** | MOESI/GPU 6×6 状态机与真实 GPU 一致性 | 🟡 中 | per ADR-SOC-01 Phase 7.C 简化版;C++ switch 表驱动 |
| **R6** | Coherence 跨域桥接(CPU↔GPU)的性能瓶颈 | 🟡 中 | v1.0 MVP 基础;v1.1 完整版追加 snoop filter 优化 |

---

## 14. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L7 Memory System + Coherence 子系统架构,基于 HBM3e + L1/L2 + SMEM + DSMEM + TMEM + Coherence) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS