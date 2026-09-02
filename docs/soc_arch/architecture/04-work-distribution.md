# 04. dGPU SoC v1.0 Work Distribution (WDU) 架构 — Crossbar + CGA Cluster + 双 Vendor

> **类别**: SoC Architecture > 子系统架构 (L5 WDU → SM/CU 分发)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.0 PASS（§3.5 L5 WDU → SM/CU 分发层）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/submit-queue.md`](../modules/submit-queue.md)（v0.5 MVP 单 SM 简化路由）
> - [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../modules/tmu-dispatch-processor.md)
> **关联研究综述**:
> - [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md)（NVIDIA WDU + AMD SPI/SQ 综述 12 件专利）
> - [`docs/research/US20240356866A1_交叉开关动态目的选择_解析.md`](../../research/US20240356866A1_交叉开关动态目的选择_解析.md)（DDS Crossbar）
> - [`docs/research/US12333311B2_协作群组阵列_解析.md`](../../research/US12333311B2_协作群组阵列_解析.md)（CGA Cluster）
> **关联 ADR**: ADR-SOC-06 D5（SubmitQueue + WDU）/ ADR-SOC-09 D2（L5 WDU→SM 双 vendor）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.5 L5 WDU → SM/CU 分发层的**子系统架构**详细化文档。

- 想快速理解 L5 层结构 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 NVIDIA 路径 → 读 §3(WDU + Crossbar + CGA Cluster)
- 想理解 AMD 路径 → 读 §4(SPI + SQ + split workgroup + load-rating)
- 想理解双 vendor 共享 SubmitQueue → 读 §5
- 想理解 DDS Crossbar 动态目的选择 → 读 §6
- 想理解 CGA Cluster 协作群组 → 读 §7
- 想理解 v1.0 战略对齐 → 读 §8
- 想理解配置 Schema → 读 §9
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §10
- 想评估风险 → 读 §11

---

## 1. 范围与目标

### 1.1 L5 WDU 分发层定位

**L5 WDU 分发层** = dGPU SoC v1.0 系统拓扑的**工作单元分发层**,负责:

- **NVIDIA 路径**:TMU → WDU → Work Distribution Crossbar → GPC (Pipeline Manager) → SM
  - **DDS Crossbar**(US20240356866A1):super destination group + 二维仲裁 + 逐周期动态目的选择
  - **CGA Cluster**(US12333311B2):thread block cluster 跨 SM 协作 + all-or-none 并发
  - **两级 WD**:GPU2GPC WD 420a → GPC2SM WD 420b(per US12333311B2)
- **AMD 路径**:CP → SPI (Shader Processor Input) → SQ → CU / SIMD wavefront
  - **SPI 两级仲裁**(US10579388B2):HQD 内队列级 + HQD 间 pipe 级
  - **split workgroup**(EP3785113B1):workgroup 拆分 + load-rating 反馈
  - **Wavefront 重组**(US9135077B2):阈值判定 + fill/steal/share 原子重分配
  - **多内核波前调度器**(EP3803583B1):双阈值迟滞节流

**L5 层模块清单**(per `00-overview` §3.5):

| 模块 | 角色 | v1.0 状态 |
|------|------|---------|
| **WDU** | NVIDIA Work Distribution Unit | ✅ v0.5 MVP(per SubmitQueue 简化) |
| **SubmitQueue** | WDU 分发网络(双 vendor 共享) | ✅ v0.5 MVP(per-cluster pending 32 + per-core active 4) |
| **Crossbar (Round-Robin)** | v1.0 MVP 简化分发 | ✅ v1.0 MVP |
| **DDS Crossbar** | 动态目的选择(v1.1 完整版) | 🔵 v1.1 完整版 |
| **CGA Cluster** | 跨 SM 协作群组 | 🔵 v1.1 完整版 |
| **AMD SPI** | Shader Processor Input(per US10579388B2) | 🔵 v1.0 MVP 新增 |
| **AMD SQ** | Sequencer(wavefront 调度) | 🔵 v1.0 MVP 新增 |

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R13-R17)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| WDU 基础 | ✅ SubmitQueue + WDU | 同 v1.0 | 00-overview D6 / ADR-SOC-09 D2 |
| Crossbar Round-Robin | ✅ v1.0 MVP | 同 v1.0 | D6 |
| Crossbar DDS 动态目的 | ❌ 推迟 | ✅ per US20240356866A1 | D6 |
| CGA Cluster | ❌ 推迟 | ✅ per US12333311B2 | D6 |
| AMD SPI→SQ | ✅ 完整路径(per US10579388B2 + EP3785113B1 + US9135077B2 + EP3803583B1) | 同 v1.0 | D6 |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.0 PASS 的 §3.5 L5 WDU → SM/CU 分发层 + §4-bis 范围矩阵 R13-R17 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

### 2.1 L5 层数据流总览(双 vendor)

```
                    ┌─────────────────────────────────────────────┐
                    │              L4 TMU/TMD                      │
                    │  - Scheduler Table(优先级分层)               │
                    │  - TMD Cache(依赖预取)                       │
                    └───────────────────┬─────────────────────────┘
                                        │ TMD dispatched
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │           WDU + SubmitQueue               │
                  │  (双 vendor 共享分发网络)                 │
                  │  - per-cluster pending FIFO(32 slot)     │
                  │  - per-core active(4 slot)                │
                  │  - 反压停 fetch                            │
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ NVIDIA 路径        │    │ AMD 路径               │
              │ (nv_mode)          │    │ (amd_mode)             │
              └─────────┬──────────┘    └────────────┬───────────┘
                        │                            │
        ┌───────────────▼────────────┐    ┌──────────▼──────────────┐
        │ Work Distribution Crossbar│    │ SPI 两级仲裁            │
        │ v1.0: Round-Robin 简化    │    │ (HQD 内队列级 +         │
        │ v1.1: DDS 动态目的选择    │    │  HQD 间 pipe 级)        │
        │ (per US20240356866A1)     │    │ (per US10579388B2)      │
        └─────────┬─────────────────┘    └──────────┬──────────────┘
                  │                                 │
        ┌─────────▼─────────────────┐    ┌──────────▼──────────────┐
        │ CGA Cluster all-or-none  │    │ split workgroup         │
        │ admission(v1.1)           │    │ + load-rating 反馈     │
        │ (per US12333311B2)        │    │ (per EP3785113B1)      │
        └─────────┬─────────────────┘    └──────────┬──────────────┘
                  │                                 │
        ┌─────────▼─────────────────┐    ┌──────────▼──────────────┐
        │ GPC 2-level WD:           │    │ Wavefront 重组          │
        │ - GPU2GPC WD 420a         │    │ + 多内核波前调度器      │
        │ - GPC2SM WD 420b          │    │ (per US9135077B2 +      │
        │ + speculative launch      │    │  EP3803583B1)          │
        └─────────┬─────────────────┘    └──────────┬──────────────┘
                  │                                 │
                  └─────────────┬───────────────────┘
                                │
                                ▼
                  ┌──────────────────────────────────────────┐
                  │           L6 SM/CU 内核                  │
                  │  - NVIDIA SM_90/100 + Warp调度          │
                  │  - AMD SIMD32/64 + Wavefront 调度       │
                  └──────────────────────────────────────────┘
```

### 2.2 关键交互注释

- **L4 → L5**:TMU 输出 TMD → WDU/SubmitQueue 接收,按 vendor_mode 路由
- **双 vendor 共用**:SubmitQueue 作为分发网络,NVIDIA/AMD 共享 pending FIFO + active slot
- **NVIDIA 分层**:WDU → Crossbar → CGA → GPC2SM WD → SM(per US20240356866A1 / US12333311B2)
- **AMD 分层**:SPI(两级仲裁)→ SQ(Wavefront 调度 + split workgroup + 重组)→ CU(per US10579388B2 / EP3785113B1 / US9135077B2 / EP3803583B1)

---

## 3. NVIDIA 路径 — WDU + Crossbar + CGA Cluster

### 3.1 NVIDIA WDU(Work Distribution Unit)

**WDU 定位**(per `docs/research/WDUtoSM/overview.md` §三):
- 接收 TMU 234 的 TMD
- 维护 **Task Table 345**(CTA 槽位,支持高优先级驱逐)
- 与 TMU 300 协作组成 Task/Work Unit 207(per US20130198760A1)

**CppTLM v0.5 实现**:通过 SubmitQueue 简化(per ADR-SOC-06 D5):
- per-cluster pending FIFO(32 slot)
- per-core active(4 slot)
- 反压停 fetch(non-LIFO eviction)

### 3.2 Work Distribution Crossbar

#### 3.2.1 v1.0 MVP: Round-Robin 简化

**简化版本**(per `00-overview` D6):
- **Round-Robin 仲裁**:按 cluster/SM 顺序轮询分配 CTA
- **不跟踪信用/端口可用性**:v1.0 MVP 简化
- **放弃 DDS 动态目的选择**:v1.1 完整版追加

#### 3.2.2 v1.1 完整版: DDS Crossbar(per US20240356866A1)

**DDS(Dynamic Destination Selection)Crossbar** 核心机制(per `docs/research/US20240356866A1_交叉开关动态目的选择_解析.md`):

| 组件 | 职责 |
|------|------|
| **Receiver 308** | 内含 source request queue(s) 312(per-source + per-super destination group + 虚拟输出排队) |
| **Mapper 316** | 把"源—super destination group"展开为多条"源—目的"选项 |
| **S-D Arbiter 320** | source-to-destination 仲裁(源选目的) |
| **D-S Arbiter 324** | destination-to-source 仲裁(目的选源) |
| **Remapper 328** | 把目的级 grant 按 super destination group 做 OR 归并,触发队列弹出 |

**核心创新**:
- **分发时动态目的选择**(at-dispatch adaptive routing):在分发包之时跟踪 destination credit 与 port availability
- **逐周期动态选择**:信用与端口可用性按 per cycle basis 重新判定
- **super destination group**:多目的端口分组,组内动态路由
- **适配 GPU 小包环境**(per 专利原文):"much better suited for the small packet environment of a Graphics Processing Unit (GPU) crossbar"

### 3.3 CGA Cluster(per US12333311B2)

**Cooperative Group Array (CGA / Cluster)**(per `docs/research/US12333311B2_协作群组阵列_解析.md`):

**核心机制**:
- **Grid 与 CTA 之间**新增 CGA 层级(Hopper thread block cluster)
- **两级 work distributor**:
  - **GPU2GPC WD 420a**(per cluster)
  - **GPC2SM WD 420b**(per GPC)
- **speculative launch**:按 GPC/µGPU/GPU 域亲和协同共驻
- **all-or-none 并发保证**:CGA 内全部 CTA 必须并发准入
- **DSMEM 配套**:GXBAR SM-to-SM 网络(per US20250173152A1)
- **CGABAR.ARRIVE/WAIT**:集群屏障与 CGA 退出/flush 协议

**CppTLM v1.1 完整版实施**:
- CGA Cluster 模块(per `00-overview` §3.5 L5.1)
- DSMEM 跨 SM 通信(per arXiv 2402.13499,180 clocks SM-to-SM,低于 L2 32%)
- CGABAR 屏障协议

### 3.4 两级 WD(GPU2GPC + GPC2SM)

**两级 WD** 实施(per US12333311B2 §三):
- **GPU2GPC WD**:把 CTA 路由到具体 GPC(per cluster 亲和)
- **GPC2SM WD**:把 CTA 路由到具体 SM(per free slots 查询)
- **speculative launch**:mock-GPC 影子状态投机启动
- **all-or-none 准入**:CGA 内全部 CTA 必须找到有效 SM 槽位

### 3.5 US20240036952A1(per `docs/research/US20240036952A1_线程块调度策略_解析.md`)

**block cluster 调度策略 API**:
- spread/balance 放置 hint
- cluster 维度 set/get
- 硬件并发上限查询
- 披露硬件链路:front-end 5210 → scheduler 5212 → WDU 5214(每 GPC pending 32 / active 4 槽)→ XBar 5220 → GPC 5218(MPC 5310/DPC 5306)→ SM 5314

---

## 4. AMD 路径 — SPI + SQ + Split Workgroup

### 4.1 SPI 两级仲裁(per US10579388B2)

**SPI(Shader Processor Input 202)** 核心机制(per `docs/research/WDUtoSM/overview.md` §四):

**两级仲裁**:
1. **HQD 内队列级**:4 bit 优先级 + quantum + 六种上下文切换条件 + MQD 保存/恢复
2. **HQD 间 pipe 级**:CS_HIGH/MEDIUM/LOW + HP3D/GFX 五级优先级 + totem pole 公平电路

**关键概念**:
- SPI 决定哪条流水线的 wavefront 可提交 shader core
- 抢占点在"shader 资源分配之前"

### 4.2 Split Workgroup(per EP3785113B1)

**Feedback Guided Split Workgroup Dispatch for GPUs**(per `docs/research/WDUtoSM/overview.md` §四):

**核心机制**:
- 打破"workgroup 必须整体装入单个 CU"约束
- 资源不足时把 workgroup **拆分为单个 wavefront** 分发到多个 CU
- 用 **性能计数器反馈**计算的 **load-rating** 引导 CU 选择
- **集中式 split workgroup scoreboard**(CU 掩码 + barrier 计数)完成跨 CU barrier 同步

**DOE PathForward 资助项目**(per EP3785113B1 公开信息)。

### 4.3 Wavefront 重组(per US9135077B2)

**GPU Compute Optimization via Wavefront Reforming**(per `docs/research/WDUtoSM/amd/US9135077B2_波前重组计算优化_解析.md`):

**核心机制**:
- work-item 分发到 CU 之后,用**每处理器队列 + 阈值判定 + fill/steal/share 原子**重分配
- 把多个**部分填充的 wavefront** 重组为**满负荷 wavefront**
- 回收 SIMD 尾部空闲算力

### 4.4 多内核波前调度器(per EP3803583B1)

**Multi-kernel Wavefront Scheduler**(per `docs/research/EP3803583B1_多内核波前调度器_解析.md`):

**核心机制**:
- CU 内"按 kernel 优先级分组的两级波前调度器 + 去调度队列"
- 多内核 wavefront 以**调度组**为粒度仲裁发射
- 按**停顿周期等资源竞争度量**做**双阈值迟滞节流**
- 实现 CU 上多内核并发、干扰抑制与前向进展保障

### 4.5 动态工作组分发(per US20230206382A1)

**Dynamic Dispatch for Workgroup Distribution**:
- CP 内 dispatch controller 闭环动态分发
- 依据各 SE 物理参数(active CU 数,反映制造 harvesting 不对称)
- SPI 经 compute dispatch bus 上报的进度/槽位状态
- workgroup 负载均衡到各 SE

### 4.6 AMD vs NVIDIA 术语对照

| 功能 | NVIDIA 术语 | AMD 术语 |
|------|------------|---------|
| 分发目的地动态选择 | US20240356866A1 Crossbar | US20230206382A1 SE 级策略 |
| 分发单元层次结构 | US12333311B2 两级 WD | EP3785113B1 workgroup 拆分 |
| 基本工作单元定义 | US8112614B2 CTA(奠基) | workgroup→wavefront 粒度转换 |
| 接收侧资源准入/策略 | US20240036952A1 pending/active pool | US10579388B2 两级仲裁 |
| 执行侧线程组织 | US9921847B2 TDT 树形分歧线程 | EP3803583B1 多内核调度 |
| 波前调度 | TBD | US9135077B2 重组 + EP3803583B1 调度器 |

---

## 5. 双 Vendor 共享 SubmitQueue

### 5.1 SubmitQueue 设计(per ADR-SOC-06 D5)

**CppTLM SubmitQueue** 作为**双 vendor 共享的分发网络**:

```cpp
class SubmitQueue {
public:
    // 共享 pending/active 资源(双 vendor 共用)
    std::array<FIFO<CTA>, MAX_CLUSTERS> pending_;        // per-cluster 32 slot
    std::array<Slot<CTA>, MAX_CORES> active_;            // per-core 4 slot
    
    // NVIDIA 分发
    void enqueue_nvidia(const TMD& tmd, uint16_t gpc_id, uint16_t sm_id);
    
    // AMD 分发
    void enqueue_amd(const TMD& tmd, uint16_t se_id, uint16_t cu_id);
    
    // 反压:fetch 暂停机制
    void tick();
};
```

### 5.2 反压停 fetch(non-LIFO eviction)

**反压机制**(per ADR-SOC-06 D5):
- 当 active slot 已满,SubmitQueue **暂停 fetch** 而非 LIFO eviction
- 保证正在执行的 wavefront/CTA 不被抢占
- 牺牲公平性换取执行连续性(per Phase F-D.2 H5)

### 5.3 双 vendor 切换

**通过 JSON config param 切换**(per `00-overview` §6.1):

```json
{
  "name": "submit_queue_0",
  "type": "SubmitQueue",
  "params": {
    "vendor_mode": "nv_mode",       // "nv_mode" | "amd_mode"
    "submit_queue_depth": 32,        // per-cluster
    "active_slot_per_core": 4,          // per-core
    "backpressure_fetch": true      // 反压停 fetch(默认)
  }
}
```

---

## 6. DDS Crossbar 动态目的选择详解

### 6.1 核心问题(per US20240356866A1 §2)

**现有技术的不足**:
- 既有的 **pre-dispatch adaptive routing**:分发前用 port credit availability、load、RTT 等选定目的
- 缺点:目的选择在 crossbar dispatcher 中**不随网络状态变化**,无法跟踪瞬态 crossbar contention

### 6.2 本专利创新

**at-dispatch adaptive routing**(与 dispatcher 紧耦合):
- 利用**仲裁时刻的瞬时目的可用性**选择输出目的
- 选择可随网络状态**逐周期(cycle to cycle)** 改变
- "much better suited for the small packet environment of a Graphics Processing Unit (GPU) crossbar"(per 说明书唯一明确点名 GPU 之处)

### 6.3 关键数据结构

| 结构 | 字段 | 用途 |
|------|------|------|
| **super_destination_group** | `group_id` + `member_destinations[]` | 多目的端口分组 |
| **packet_type** | `static_destination` / `dynamic_destination` | 包路由类型 |
| **destination_credit** | `posted_credits` + `np_credits` + `cpl_credits` | 目的端信用 |
| **port_availability** | `lane_idle` + `valid_signal` | 端口可用性 |

### 6.4 流程(per US20240356866A1 FIG. 4)

```
1. 包入队 → source request queue 312(VOQ)
2. Mapper 316 展开 → super_destination group → 多 source-destination 选项
3. S-D Arbiter 320:源选目的(逐周期)
4. D-S Arbiter 324:目的选源(逐周期)
5. Remapper 328:合并 grant → 触发队列弹出
7. 输出到具体目的 SM/GPC
```

### 6.5 v1.0 MVP 简化 vs v1.1 完整版

| 维度 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| 仲裁方式 | Round-Robin(per cluster 顺序) | DDS(逐周期信用/可用性) |
| 信用跟踪 | ❌ | ✅ |
| 端口可用性跟踪 | ❌ | ✅ |
| super_destination_group | ❌ | ✅ |
| at-dispatch 动态目的选择 | ❌（仅静态） | ✅（per cycle 信用/可用性） |

---

## 7. CGA Cluster 协作群组

### 7.1 CGA 引入背景(per US12333311B2)

**CTA 局限性**:
- CTA 必须**在单个核内完成**(per US8112614B2)
- 跨 CTA 通信需 global memory(高延迟)

**CGA 创新**:
- Grid 与 CTA 之间**新增 CGA 层级**
- **多 CTA 跨 SM 协作**(共享操作数)
- **all-or-none 并发保证**

### 7.2 两级 Work Distributor

**GPU2GPC WD 420a**(per cluster):
- 整个 CGA 选择目标 GPC
- 按 GPC/µGPU/GPU 域亲和协同共驻

**GPC2SM WD 420b**(per GPC):
- CGA 内 CTA 选择目标 SM
- 按 free slots 查询 + mock-GPC 影子状态投机启动

### 7.3 配套基础设施

- **DSMEM**(per US20250173152A1,Hopper SM-to-SM 网络):GPCARB/GXBAR,与 MXBAR/L2 路径物理隔离
- **CGABAR.ARRIVE/WAIT**:集群屏障与 CGA 退出/flush 协议
- **4GB/256×16MB CTAID 分段地址窗口**:地址分段

### 7.4 CppTLM 实施(v1.1 完整版)

- ✅ v1.1 完整版实施 CGA Cluster
- ❌ v1.0 MVP 不实施
- 关联模块:DSMEM(GPU-to-SM 通信,per US20250173152A1)+ L1.5 cluster 互连(per `00-overview` §3.7 L7.4)

---

## 8. v1.0 战略对齐

### 8.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| L5 层 WDU 基础 | ✅ SubmitQueue + WDU | ✅ §5 |
| NVIDIA Crossbar v1.0 | ✅ Round-Robin 简化 | ✅ §3.2.1 |
| NVIDIA Crossbar v1.1 | 🔵 DDS 动态目的 | ✅ §3.2.2 + §6 |
| NVIDIA CGA Cluster v1.1 | 🔵 all-or-none 并发 | ✅ §3.3 + §7 |
| AMD SPI→SQ v1.0 | ✅ 完整路径 | ✅ §4.1-4.5 |

### 8.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| WDU 基础 | ✅ SubmitQueue + WDU | ✅ |
| Crossbar Round-Robin | ✅ | ✅ |
| Crossbar DDS | ❌ | ✅ per US20240356866A1 |
| CGA Cluster | ❌ | ✅ per US12333311B2 |
| AMD SPI→SQ 完整 | ✅(4 件专利: US10579388B2 + EP3785113B1 + US9135077B2 + EP3803583B1) | ✅ |

### 8.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-06 D5 | SubmitQueue + WDU(单 SM 简化路由 + 反压停 fetch) |
| ADR-SOC-09 D2 | v1.0 NVIDIA+AMD dual vendor 战略,DDS v1.1 推迟 |

---

## 9. 配置 Schema

### 9.1 顶层 JSON Schema

```json
{
  "name": "wdu_0",
  "type": "WDU",
  "params": {
    "vendor_mode": "nv_mode",         // "nv_mode" (默认) | "amd_mode"
    "submit_queue_depth": 32,         // per-cluster pending FIFO
    "active_slot_per_core": 4,        // per-core active
    "backpressure_fetch": true,       // 反压停 fetch
    "crossbar_mode": "round_robin",   // "round_robin" (v1.0) | "dds" (v1.1)
    "enable_cga_cluster": false,      // v1.1 完整版
    "amd_spi_two_level_arb": true,    // AMD SPI 两级仲裁
    "amd_split_workgroup": true,      // split workgroup + load-rating
    "amd_wavefront_reform": true,     // Wavefront 重组
    "amd_multi_kernel_wf_sched": true  // 多内核波前调度器
  }
}
```

### 9.2 连接(connection)示例

```json
{
  "connections": [
    {
      "src": "tmu_0.tmd_dispatched",
      "dst": "wdu_0.tmd_in"
    },
    {
      "src": "wdu_0.cta_out_nv",
      "dst": "submit_queue_0.cta_in"
    },
    {
      "src": "wdu_0.cta_out_amd",
      "dst": "submit_queue_0.cta_in"
    },
    {
      "src": "submit_queue_0.dispatch_to_core",
      "dst": "compute_cluster_0.cta_in"
    }
  ]
}
```

---

## 10. ADR/微架构/OpenSpec 引用矩阵

### 10.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-06 D5 | SubmitQueue + WDU(单 SM 简化路由 + 反压停 fetch) |
| ADR-SOC-09 D2 | v1.0 NVIDIA+AMD dual vendor 战略,DDS v1.1 推迟 |

### 10.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **SubmitQueue** | [`docs/soc_arch/modules/submit-queue.md`](../modules/submit-queue.md) |
| **TMU Dispatch Processor** | [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../modules/tmu-dispatch-processor.md) |
| **ComputeUnitTLM** | [`docs/soc_arch/modules/gpu-compute_unit.md`](../modules/gpu-compute_unit.md) |
| **CudaCoreAdapter** | [`docs/soc_arch/modules/cuda-core-adapter.md`](../modules/cuda-core-adapter.md) |

### 10.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md) | NVIDIA WDU + AMD SPI/SQ 综述 12 件专利 |
| [`docs/research/US20240356866A1_交叉开关动态目的选择_解析.md`](../../research/US20240356866A1_交叉开关动态目的选择_解析.md) | DDS Crossbar |
| [`docs/research/US12333311B2_协作群组阵列_解析.md`](../../research/US12333311B2_协作群组阵列_解析.md) | CGA Cluster |
| [`docs/research/US20240036952A1_线程块调度策略_解析.md`](../../research/US20240036952A1_线程块调度策略_解析.md) | block cluster 调度策略 API |
| [`docs/research/US10579388B2_着色器核心资源分配策略_解析.md`](../../research/WDUtoSM/amd/US10579388B2_着色器核心资源分配策略_解析.md) | AMD SPI 两级仲裁 |
| [`docs/research/EP3785113B1_反馈引导工作组拆分分发_解析.md`](../../research/WDUtoSM/amd/EP3785113B1_反馈引导工作组拆分分发_解析.md) | split workgroup |
| [`docs/research/US9135077B2_波前重组计算优化_解析.md`](../../research/WDUtoSM/amd/US9135077B2_波前重组计算优化_解析.md) | Wavefront 重组 |
| [`docs/research/EP3803583B1_多内核波前调度器_解析.md`](../../research/WDUtoSM/amd/EP3803583B1_多内核波前调度器_解析.md) | 多内核波前调度器 |
| [`docs/research/US20230206382A1_动态工作组分发_解析.md`](../../research/WDUtoSM/amd/US20230206382A1_动态工作组分发_解析.md) | SE 级策略 |

---

## 11. 风险与缓解 R1-R5

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | DDS Crossbar v1.0 不实施,v1.0 MVP 性能可能不足 | 🟡 中 | v1.0 MVP 简化已足够功能验证;v1.1 完整版追加 DDS 性能优化 |
| **R2** | CGA Cluster 跨 SM 通信延迟建模不准(per arXiv 2402.13499 实测 180 clocks SM-to-SM) | 🟡 中 | v1.0 MVP 不实施 CGA;v1.1 完整版基于实测延迟建模 |
| **R3** | AMD SPI 物理参数差异(SE 数 / CU 数 per XCD) | 🟢 低 | per gem5-soc-survey 与 MI300X 实际拓扑(8 XCD × 38 CU) |
| **R4** | split workgroup 跨 CU barrier 同步复杂 | 🟡 中 | EP3785113B1 集中式 split workgroup scoreboard 设计参考 |
| **R5** | SubmitQueue 反压可能导致 TDG 队列溢出 | 🟢 低 | 32 slot pending + 4 slot active 已 v0.5 验证 |

---

## 12. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L5 WDU 子系统架构,基于 NVIDIA WDU+DDS Crossbar+CGA Cluster vs AMD SPI+SQ+split workgroup 双 vendor 路径) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS