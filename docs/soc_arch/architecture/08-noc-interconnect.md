# 08. dGPU SoC v1.0 NoC Interconnect 架构 — Mesh NoC + Router + NIC + 跨 Cluster 通信

> **类别**: SoC Architecture > 子系统架构 (L7 Memory — NoC Interconnect)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.0 PASS（§3.5 L5 WDU 互连 + §3.7 L7 Memory 互连）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/gpu-noc-mesh.md`](../modules/gpu-noc-mesh.md)
> - [`docs/soc_arch/modules/noc-router.md`](../modules/noc-router.md)
> - [`docs/soc_arch/modules/noc-nic.md`](../modules/noc-nic.md)
> - [`docs/soc_arch/modules/interconnect-crossbar.md`](../modules/interconnect-crossbar.md)
> - [`docs/soc_arch/modules/interconnect-arbiter.md`](../modules/interconnect-arbiter.md)
> - [`docs/soc_arch/modules/interconnect-bridge.md`](../modules/interconnect-bridge.md)
> **关联研究综述**:
> - [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md)（gem5 Garnet NoC 合成流量 + 配置示例）
> - [`docs/research/US20240356866A1_交叉开关动态目的选择_解析.md`](../../research/US20240356866A1_交叉开关动态目的选择_解析.md)（DDS Crossbar 用于 NoC）
> **关联 ADR**: ADR-SOC-09（v1.0 NVIDIA+AMD dual vendor）/ ADR-SOC-10（v1.0 ModuleFactory 拓扑层）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.5/§3.7 中的**NoC 互连子系统**详细化文档。

- 想快速理解 NoC 范围 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 Mesh NoC 拓扑 → 读 §3
- 想理解 Router 路由算法 → 读 §4
- 想理解 NIC 网络接口 → 读 §5
- 想理解 L5 WDU Crossbar vs L7 NoC Mesh → 读 §6
- 想理解 跨 Cluster 通信(DSMEM/GPCARB) → 读 §7
- 想理解 Gem5 Garnet 参照 → 读 §8
- 想理解 v1.0 战略对齐 → 读 §9
- 想理解 CppTLM 实施 → 读 §10
- 想理解配置 Schema → 读 §11
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §12
- 想评估风险 → 读 §13

---

## 1. 范围与目标

### 1.1 NoC Interconnect 子系统定位

**NoC(Network-on-Chip)Interconnect** = dGPU SoC v1.0 系统拓扑的**片上网络层**,负责:

- **Mesh NoC 拓扑**:SM/CU/TPC/GPC 间的多对多通信
- **Router**:XY/OE/Turn Model 路由算法
- **NIC(Network Interface Controller)**:SM 与 NoC 之间的协议转换
- **L5 WDU Crossbar** vs **L7 Mesh NoC**:两层互连的分工
- **跨 Cluster 通信**(DSMEM/GPCARB/GXBAR,per US20250173152A1)

### 1.2 v1.0 战略关键决策(per `00-overview` §1.3)

| 互连层 | 角色 | 关系 |
|--------|------|------|
| **L5 WDU Crossbar** | SM 内部 / GPC 之间 | 一对一低延迟,**DDS v1.1** + Round-Robin v1.0 |
| **L7 Mesh NoC** | SM/TPC/GPC/Memory 间多对多 | 高吞吐 + 多路径 |
| **L7 DSMEM/GPCARB/GXBAR** | GPC 内跨 SM | 180 clocks SM-to-SM(per arXiv 2402.13499)|

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.0 PASS 的 §3.5 + §3.7 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

```
                ┌─────────────────────────────────────────────────────────────┐
                │  ComputeCluster(per SM)                                    │
                │  ┌────────────────────┐                                   │
                │  │ SM                 │ ←→ NIC(Network Interface)         │
                │  │  - RegFile         │     - Flit 打包/解包               │
                │  │  - SMEM/TMEM       │     - Credit-based flow control    │
                │  │  - Tensor Core     │     - Link traversal              │
                │  └─────────┬──────────┘                                   │
                └────────────┼──────────────────────────────────────────────┘
                             │
                             ▼
                  ┌──────────────────────────────────────────┐
                  │    Router(per GPC/TPC)                 │
                  │  - XY routing 算法                     │
                  │  - 5-port(本地 N/S/E/W)                │
                  │  - Virtual Channel(VC)分配             │
                  │  - 仲裁调度(per US20240356866A1 思想) │
                  └─────────┬────────────────────┬─────────┘
                            │                    │
                  ┌─────────▼──────┐    ┌─────────▼──────────┐
                  │ 邻居 Router    │    │ 邻居 Router        │
                  │ (XY direction) │    │ (XY direction)     │
                  └─────────┬──────┘    └──────────┬─────────┘
                            │                      │
                            └──────────┬───────────┘
                                       │
                                       ▼
                  ┌──────────────────────────────────────────┐
                  │      Memory Subsystem                   │
                  │  - HBM3e Controller                     │
                  │  - L2 Cache Slice(per GPC)              │
                  │  - Memory Cluster                      │
                  └──────────────────────────────────────────┘

   ──────────────────  L5 WDU Crossbar (per GPC)  ──────────────────

                  ┌──────────────────────────────────────────┐
                  │  Work Distribution Crossbar             │
                  │  - v1.0: Round-Robin 简化              │
                  │  - v1.1: DDS(per US20240356866A1)     │
                  │  - 把 CTA 路由到具体 SM                │
                  └──────────────────────────────────────────┘

   ──────────────────  L7 GPCARB/GXBAR (DSMEM, v1.1)  ──────────────────

                  ┌──────────────────────────────────────────┐
                  │  GPCARB/GXBAR 专用 SM2SM 网络           │
                  │  - 跨 SM 协作(CGA cluster)             │
                  │  - 180 clocks SM-to-SM                  │
                  │  - 与 MXBAR/L2 路径物理隔离            │
                  └──────────────────────────────────────────┘
```

---

## 3. Mesh NoC 拓扑

### 3.1 NVIDIA 多层互连

NVIDIA GPU 多层互连结构(per `docs/research/SM/overview.md`):

```
L5: Work Distribution Crossbar(per US20240356866A1)
  ↓
L5: GPC(TPC 集合)
  ↓
L5: SM(单物理核)
  ↓
L7: Mesh NoC 跨 GPC/SM/Memory
  ↓
L7: GPCARB/GXBAR 跨 SM(CGA cluster 共享)
```

### 3.2 AMD CDNA 3 多层互连

AMD CDNA 3 多层互连:

```
L5: SPI 两级仲裁(per US10579388B2)
  ↓
L5: SE(Shader Engine) per XCD
  ↓
L5: CU(单计算单元)
  ↓
L5: SIMD32/64 wavefront
  ↓
L7: Infinity Fabric 跨 XCD
  ↓
L7: Infinity Cache(256MB L3 级)
```

### 3.3 Mesh NoC 拓扑设计

**CppTLM Mesh NoC 拓扑**(per `gpu-noc-mesh.md`):

- **2D Mesh**:M × N 网格
- **每个节点 = 一个 Router**
- **每个 Router = 1 个 local processor**(SM/CU/TPC)
- **5-port router**:N/S/E/W + Local
- **XY routing**:先 X 方向再 Y 方向(避免死锁)

### 3.4 Mesh 规模示例

| GPU | SM/CU 数 | 估计 Mesh 规模 |
|-----|----------|----------------|
| NVIDIA H100 | ~132 SM | 12×11 网格 |
| NVIDIA B200 | ~144-160 SM | 12×13 网格 |
| NVIDIA GB203 | ~84 SM | 10×9 网格(消费级)|
| AMD MI300X | 304 CU | ~18×17 网格(8 XCD × 38 CU) |

---

## 4. Router 路由算法

### 4.1 XY 路由(默认,无死锁)

**XY 路由算法**:
- 先沿 **X 方向** 路由(横向)
- 再沿 **Y 方向** 路由(纵向)
- **优点**:实现简单 + **避免死锁**(per `docs/research/gem5-soc-survey.md` §2.4)
- **缺点**:热点路径可能拥塞

### 4.2 端口结构

```
5-port Router:
  ┌─────────┐
  │   N     │
  │         │
W ── router ── E
  │         │
  │   S     │
  └────L────┘
```

### 4.3 Virtual Channel(VC)

**VC 分配**(避免协议死锁):
- VC0:Request 流量
- VC1:Response 流量
- VC2:Forward 流量
- VC3:Reply 流量
- 4 VC 足以避免请求-响应协议死锁

### 4.4 仲裁调度

**经典仲裁**(per US20240356866A1 思想,但简化):
- **Round-Robin**:循环调度每个输入端口
- **优先级**:Request > Response
- **VC 仲裁**:VC0 > VC1 > VC2 > VC3

### 4.5 CppTLM 实施

- **`noc-router.md` 模块**:5-port XY router
- **`noc-nic.md` 模块**:NIC 接口

---

## 5. NIC(Network Interface Controller)

### 5.1 NIC 角色

**NIC** 位于 SM 与 NoC 之间,负责:
- **Flit 打包/解包**:把 SM 请求打包为 NoC flit
- **协议转换**:TLM 事务 ↔ NoC flit
- **Credit-based flow control**:跟踪下游 router 缓冲可用性
- **Link traversal**:实现 wire delay(per cycle)

### 5.2 Flit 结构

```cpp
struct Flit {
    FlitType type;        // HEAD/BODY/TAIL
    uint16_t src_id;       // 源 Router ID
    uint16_t dst_id;       // 目的 Router ID
    uint8_t vc;            // Virtual Channel ID
    uint32_t txn_id;       // 事务 ID
    uint64_t addr;         // 物理地址
    uint32_t size;         // 数据大小
    uint8_t data[16];      // flit payload(典型 16B flit)
};
```

### 5.3 CppTLM 实施

- **`noc-nic.md` 模块**:NIC 接口
- **Credit-based 信用管理**:避免 flit 丢失

---

## 6. L5 WDU Crossbar vs L7 Mesh NoC 分工

### 6.1 两层互连对比

| 维度 | L5 WDU Crossbar | L7 Mesh NoC |
|------|-----------------|-------------|
| 用途 | CTA/wavefront 分发 | 通用内存访问 + 数据传输 |
| 范围 | per-GPC/per-Cluster | 整个 dGPU SoC |
| 延迟 | **极低**(单 cycle) | **较高**(多 cycle + 多 hop) |
| 拓扑 | 1-to-N(分发) | N-to-N(多对多) |
| 路由 | DDS / Round-Robin | XY / Turn Model |
| 协议 | Work Distribution | L2 Cache ↔ L1/SMEM ↔ Memory |

### 6.2 数据流路径选择

- **L5**:TMU 输出 TMD → WDU/SubmitQueue → Crossbar → SM/CU(单 cycle 分发)
- **L7**:SM/CU 输出请求 → NIC → Mesh NoC → Memory Cluster → HBM(多 cycle 数据传输)

### 6.3 CppTLM 实施差异

- **L5 WDU Crossbar**:`submit_queue.md` + `noc-router.md`(简化,1 cycle)
- **L7 Mesh NoC**:`noc-nic.md` + `noc-router.md`(完整,多 cycle)

---

## 7. 跨 Cluster 通信(DSMEM/GPCARB/GXBAR,v1.1)

### 7.1 GPCARB/GXBAR 专用 SM2SM 网络

**per US20250173152A1**(per `00-overview` §3.6 L6.1 + §3.7 L7.4):

- **GPCARB**(GPU Processing Cluster Arbitration Router Bus)
- **GXBAR**(GPU Crossbar)
- **专用 SM-to-SM 网络**,与 MXBAR/L2 路径**物理隔离**
- 用于 **CGA**(cluster)各 SM 的本地共享内存互访

### 7.2 性能参数(per arXiv 2402.13499)

| 参数 | 值 |
|------|-----|
| SM-to-SM 延迟 | **180 clocks** |
| 相对 L2 | **低于 L2 32%** |
| 网络 | 专用 GPCARB/GXBAR(物理隔离)|

### 7.3 配套协议

- **CGABAR.ARRIVE/WAIT**:集群屏障与 CGA 退出/flush 协议
- **uTLB 地址 hash**:地址翻译
- **phase 翻转 MEMBAR**:内存屏障
- **合并写确认**:reduce 写流量

### 7.4 CppTLM 实施

- **v1.0 MVP 不实施**(per `00-overview` §4-bis R21)
- **v1.1 完整版**:`DSMEM` 模块 + GPCARB/GXBAR + CGABAR

---

## 8. Gem5 Garnet NoC 参照

### 8.1 Gem5 Garnet NoC 模型

**Garnet NoC**(per `docs/research/gem5-soc-survey.md` §2.4):

- Gem5 标准库集成(`configs/example/garnet_synth_traffic.py`)
- **配置**:`garnet_synth_traffic.py` 合成流量测试
- **真实模型**:`ruby_random_test.py` 包含 MOESI Hammer 随机测试
- **集成示例**:`ruby_mem_test.py` 包含 Ruby + MemTest + DMA

### 8.2 Gem5 学习示例(NoC 集成)

| 路径 | 模式 | 拓扑 |
|------|------|------|
| `configs/learning_gem5/part1/two_level.py` | [SE] | L1 I/D + L2 共享 + MemBus + DDR3 |
| `configs/example/garnet_synth_traffic.py` | [SE] | Garnet 合成流量 |
| `configs/example/ruby_mem_test.py` | [SE] | Ruby + MemTest + DMA |

### 8.3 CppTLM NoC 简化

CppTLM NoC 实现相比 Gem5 Garnet:
- **简化**:无真实 wire delay(per cycle 跳数)
- **简化**:VC 数量可配(默认 4)
- **简化**:Router 仲裁算法可配(Round-Robin / 优先级)

---

## 9. v1.0 战略对齐

### 9.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| L5 WDU Crossbar | ✅ Round-Robin v1.0;✅DDS v1.1 | ✅ §6 |
| L7 Mesh NoC | ✅ 2D mesh + XY routing | ✅ §3-4 |
| L5/L7 分工 | ✅ L5 分发 + L7 数据传输 | ✅ §6 |
| 跨 Cluster 通信 | 🔵 DSMEM v1.1 | ✅ §7 |

### 9.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| 2D Mesh 拓扑 | ✅ | ✅ |
| XY 路由 | ✅ | ✅ |
| 5-port Router | ✅ | ✅ |
| VC 4 档 | ✅ | ✅ |
| Round-Robin 仲裁 | ✅ | ✅ |
| NIC + Flit 打包 | ✅ | ✅ |
| Credit-based flow control | ✅ | ✅ |
| Turn Model 路由 | ❌ | ✅(可选)|
| L5 Crossbar Round-Robin | ✅ | ✅ |
| L5 Crossbar DDS | ❌ | ✅ per US20240356866A1 |
| GPCARB/GXBAR DSMEM | ❌ | ✅ per US20250173152A1 |

### 9.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor 战略 |
| ADR-SOC-10（Proposed） | v1.0 ModuleFactory 拓扑层 |

---

## 10. CppTLM 实施

### 10.1 模块清单

| 模块 | 路径 | 角色 |
|------|------|------|
| **GpuNoC** | `include/tlm/cluster/gpu_noc_cluster.hh` | Mesh NoC 顶层 |
| **noc-router** | `include/tlm/router_tlm.hh` | 5-port XY router |
| **noc-nic** | `include/tlm/nic_tlm.hh` | NIC 接口 |
| **interconnect-crossbar** | `include/tlm/crossbar_tlm.hh` | 通用 Crossbar |
| **interconnect-arbiter** | `include/tlm/arbiter_tlm.hh` | 仲裁器 |
| **interconnect-bridge** | `include/tlm/interconnect/bridge_tlm.hh`（规划中） | 桥接 |

### 10.2 关键共享方法

```cpp
class Router {
public:
    void tick();  // 路由 + 仲裁
    void flit_in(PortId port, const Flit& f);
    Flit flit_out(PortId port);
};

class NIC {
public:
    void tick();  // Flit 打包/解包 + credit 管理
    void request(const Bundle& req);
    Bundle response();
};

class GpuNoC {
public:
    void set_topology(int rows, int cols);
    void route(uint16_t src, uint16_t dst, const Flit& f);
};
```

---

## 11. 配置 Schema

```json
{
  "name": "gpu_noc_0",
  "type": "GpuNoC",
  "params": {
    "topology": "mesh_2d",          // "mesh_2d" | "ring" | "torus"
    "rows": 12,
    "cols": 12,
    "router_ports": 5,              // N/S/E/W/Local
    "vc_count": 4,                   // Virtual Channels
    "flit_size_bytes": 16,
    "routing_algorithm": "xy",      // "xy" | "turn_model" (v1.1)
    "arbitration": "round_robin",   // "round_robin" | "priority" | "dds" (L5)
    
    "wire_delay_cycles": 1,         // per hop
    "router_pipeline_cycles": 4,    // RC/VA/SA/ST/EB
    
    "credit_buffer_size": 16,       // per VC
    "max_packet_size_flits": 16,
    
    // v1.1
    "enable_dsmem": false,
    "enable_dds_crossbar": false
  }
}
```

---

## 12. ADR/微架构/OpenSpec 引用矩阵

### 12.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor 战略 |
| ADR-SOC-10（Proposed） | v1.0 ModuleFactory 拓扑层 |

### 12.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **GpuNoC** | [`docs/soc_arch/modules/gpu-noc-mesh.md`](../modules/gpu-noc-mesh.md) |
| **noc-router** | [`docs/soc_arch/modules/noc-router.md`](../modules/noc-router.md) |
| **noc-nic** | [`docs/soc_arch/modules/noc-nic.md`](../modules/noc-nic.md) |
| **noc.common** | [`docs/soc_arch/modules/noc.common.md`](../modules/noc.common.md) |
| **interconnect-crossbar** | [`docs/soc_arch/modules/interconnect-crossbar.md`](../modules/interconnect-crossbar.md) |
| **interconnect-arbiter** | [`docs/soc_arch/modules/interconnect-arbiter.md`](../modules/interconnect-arbiter.md) |
| **interconnect-bridge** | [`docs/soc_arch/modules/interconnect-bridge.md`](../modules/interconnect-bridge.md) |

### 12.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/gem5-soc-survey.md`](../../research/gem5-soc-survey.md) | Gem5 Garnet NoC 合成流量 + ruby_mem_test |
| [`docs/research/US20240356866A1_交叉开关动态目的选择_解析.md`](../../research/US20240356866A1_交叉开关动态目的选择_解析.md) | DDS Crossbar 思想(用于 L5)|

---

## 13. 风险与缓解 R1-R5

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | NoC 多周期 wire delay 建模准确性 | 🟡 中 | v1.0 MVP 简化(per hop 1 cycle);v1.1 完整版根据真实硬件校准 |
| **R2** | XY 路由在非对称拓扑(Mesh 大小不一)可能热点 | 🟢 低 | XY 路由在 mesh 拓扑下无死锁;热点可通过 VC 仲裁缓解 |
| **R3** | Credit-based flow control 死锁 | 🟡 中 | 4 VC 足以避免请求-响应协议死锁 |
| **R4** | GPCARB/GXBAR 跨 SM 通信延迟建模(180 clocks) | 🟡 中 | v1.0 MVP 不实施;v1.1 基于 arXiv 2402.13499 实测 |
| **R5** | Mesh 规模与 SM 数量不匹配(GB203 消费级) | 🟢 低 | Mesh 配置(rows × cols)灵活可调 |

---

## 14. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(NoC Interconnect 子系统架构,基于 Mesh + XY Router + NIC + 跨 Cluster 通信 + Gem5 Garnet 参照) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS