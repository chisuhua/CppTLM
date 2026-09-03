# 03. dGPU SoC v1.0 Task Management Unit (TMU) 架构 — TMU + TMD + 依赖预取 + PDL

> **类别**: SoC Architecture > 子系统架构 (L4 TMU/TMD)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（§3.4 L4 TMU/TMD 层）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../modules/tmu-dispatch-processor.md)（v0.5 MVP 简化）
> - [`docs/soc_arch/modules/gpu-memory-cluster.md`](../modules/gpu-memory-cluster.md)（TMD 关联）
> **关联研究综述**:
> - [`docs/research/TMU/overview.md`](../../research/TMU/overview.md)（NVIDIA TMU 专利综述:8 件 + 3 代形态）
> - [`docs/research/TMU/TMD.md`](../../research/TMU/TMD.md)（TMD 完整字段布局 7 区）
> **关联 ADR**: ADR-SOC-06 D5（TMU 简化 MVP）/ ADR-SOC-09 D2(共享 L4 TMU/TMD)

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.4 L4 TMU/TMD 层的**子系统架构**详细化文档。

- 想快速理解 L4 层结构 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 TMU 三种形态 → 读 §3(Kepler → Volta → Hopper 演进)
- 想理解 TMD 完整字段布局 → 读 §4(7 区 Init/Sched/Exec/QueueState/HW-only/Dep/Queue)
- 想理解依赖任务自动启动 → 读 §5(per US20130198760A1)
- 想理解依赖预取 → 读 §6(per US11182207B2)
- 想理解 PDL vs CDP 区分 → 读 §7(关键概念澄清)
- 想理解嵌套流事件 → 读 §8(per US9928109B2)
- 想理解 v1.0 战略对齐 → 读 §9
- 想理解配置 Schema → 读 §10
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §11
- 想评估风险 → 读 §12

---

## 1. 范围与目标

### 1.1 L4 TMU/TMD 层定位

**L4 TMU/TMD 层** = dGPU SoC v1.0 系统拓扑的**任务调度层**,负责:

- **接收** L3 CommandProcessor 解码后的 TMD 描述符
- **优先级分层** Scheduler Table 组织(per task priority)
- **依赖管理** Dependent TMD enable/pointer + Consumer Flags(per US11182207B2)
- **依赖预取** Dependency/Prefetch Unit 410 在生产者执行期间并行预取消费者描述符
- **依赖自动启动** Dependent TMD auto-launch,无 CPU 介入(per US20130198760A1)
- **嵌套流事件** Wait/Signal Event 跨 stream 同步(per US9928109B2, v1.1)
- **PDL 设备端依赖启动**(per US20230236878A1, v1.1)

**L4 层模块清单**(per `00-overview` §3.4):

| 模块 | 角色 | v1.0 状态 |
|------|------|---------|
| **TMU** | 任务管理单元(优先级分层 + 依赖管理 + 预取) | ✅ v0.5 MVP 简化(per `tmu-dispatch-processor.md`) |
| **TMD Cache** | TMD 描述符缓存(预取加速) | 🔵 v1.0 MVP 新增基础(per US11182207B2) |
| **Scheduler Table** | 优先级分层任务表 | ✅ v0.5 简化 |
| **Stream Event Queue** | 嵌套流事件队列 | 🔵 v1.1 完整版追加 |
| **PDL Hardware Hook** | 设备端依赖启动硬件基础 | ❌ v1.1 完整版追加(per US20230236878A1) |

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R10-R12)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| TMU 基础(TMD 调度) | ✅ 优先级分层 + TMD prefetch(per US11182207B2) | 同 v1.0 | D5(ADR-SOC-06) |
| TMU 嵌套流事件 | ❌ 推迟 | ✅ per US9928109B2 | D5 |
| TMU PDL | ❌ 推迟 | ✅ per US20230236878A1 WSDU/pre-exit | D5 |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.1 PASS 的 §3.4 L4 TMU/TMD 层 + §4-bis 范围矩阵 R10-R12 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

### 2.1 L4 层数据流总览

```
                    ┌─────────────────────────────────────────────┐
                    │           L3 Command Protocol               │
                    │  - CommandProcessor 5-state FSM             │
                    │  - Pm4Decoder-NV / Pm4Decoder-AMD           │
                    │  - 输出标准 TMD 描述符                       │
                    └───────────────────┬─────────────────────────┘
                                        │ TMD(Init/Sched/Exec/QueueState/HW-only/Dep/Queue)
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │        TMU(Task Management Unit)         │
                  │  ┌────────────────────────────────────┐ │
                  │  │ Scheduler Table (优先级分层)         │ │
                  │  │  - 高优先级链表                       │ │
                  │  │  - 中优先级链表                       │ │
                  │  │  - 低优先级链表                       │ │
                  │  └────────────────────────────────────┘ │
                  │  ┌────────────────────────────────────┐ │
                  │  │ TMD Cache (依赖预取加速)             │ │
                  │  │  - Dependency/Prefetch Unit 410     │ │
                  │  │  - Consumer Flags (462/464/466)     │ │
                  │  └────────────────────────────────────┘ │
                  │  ┌────────────────────────────────────┐ │
                  │  │ Stream Event Queue (v1.1 完整版)    │ │
                  │  │  - Wait Event / Signal Event        │ │
                  │  └────────────────────────────────────┘ │
                  └─────────┬────────────────────┬────────────┘
                            │                    │
                  ┌─────────▼──────┐    ┌─────────▼──────────────┐
                  │ Dependent TMD  │    │ Independent TMD       │
                  │ (等依赖完成)   │    │ (立即就绪)             │
                  └─────────┬──────┘    └────────────┬───────────┘
                            │                        │
                            └────────────┬───────────┘
                                         │
                                         ▼
                  ┌──────────────────────────────────────────┐
                  │           L5 WDU → SM/CU 分发             │
                  │  - SubmitQueue.enqueue + dispatch_to_core│
                  └──────────────────────────────────────────┘

   ──────────────────  PDL 路径 (v1.1 完整版)  ──────────────────

   SM/CU 内部直接发起 child task
       │
       ▼
   WSDU(pre-exit / WSDU 230) 直接接收 → Scheduler Table
       │
       ▼
   (绕过 L2/L3 pushbuffer + front end)
```

### 2.2 关键交互注释

- **L3 → L4**:CommandProcessor 解码后输出标准 TMD;TMU 接收并放入 Scheduler Table
- **依赖预取**:生产者 TMD 包含 Consumer Flags,TMU 在生产者执行期间并行预取消费者描述符
- **依赖自动启动**:消费者 TMD 含 Dependent TMD enable + pointer;TMU 自动启动,无 CPU 介入
- **PDL(v1.1)**:SM/CU 内部直接通过 WSDU(pre-exit)提交 child task,绕过 pushbuffer + front end

---

## 3. TMU 三种形态(per `docs/research/TMU/overview.md` §2)

### 3.1 Kepler 时代 — Task/Work Unit 207(US20130198760A1)

**形态**:`Task/Work Unit 207 = TMU 300 + WDU 340`

| 组成 | 职责 |
|------|------|
| **TMU 300** | 维护 Scheduler Table 321(按优先级的 TMD 分组链表)+ TMD Cache 350 |
| **WDU 340** | 维护 Task Table 345(CTA 槽位,支持高优先级驱逐) |

**关键创新**:把依赖任务的使能标志(Dependent TMD Enable)、描述符指针(Dependent TMD Pointer)、字段拷贝使能编码进前驱任务的 TMD,任务完成时 TMU 自动启动依赖任务,取代信号量同步。

### 3.2 Volta 时代 — 独立 TMU 单元 215/234(US9535815B2 / US11182207B2)

**形态**:独立 TMU 单元 215/234

**职责**:
- 组织 pending/active grid 池
- 管理依赖计数、描述符预取
- US9535815B2 明确称其为 "macro-scheduler",与 SM 内 scheduler unit("micro-scheduler")构成两级调度

### 3.3 Hopper 时代 — WSDU 230 合并(US20230236878A1)

**形态**:`Work Scheduler/Distribution Unit 230`(WSDU,Hopper 合并 TMU + WDU)

| 组成 | 职责 |
|------|------|
| **Scheduling Dependency Logic 232** | 调度/数据依赖解耦与提前启动 |
| **Task Dependency Table 240** | 依赖锁存器表(片上) |

**关键创新**:TMU 与 WDU 合并为单一单元,实现调度/数据依赖解耦与提前启动,并接收 SM 端直接提交的消费者任务命令(per US20230236878A1)。

### 3.4 三种形态对比(per `00-overview` §3.4 L4.1)

```
Kepler: Task/Work Unit 207 = TMU 300 + WDU 340
Volta: 独立 TMU 单元 215/234(pending/active grid pool)
Hopper: WSDU 230 = TMU + WDU 合并 + Task Dependency Table 240
       + 接收 SM 直接提交(PDL 硬件基础)
```

**CppTLM v1.0 实施**:
- **v0.5 MVP 简化**(per `tmu-dispatch-processor.md`):独立 TMU 单元 + 简化 TMD 调度
- **v1.0 MVP**:TMD prefetch 基础(per US11182207B2)+ 优先级分层
- **v1.1 完整版**:WSDU 合并 + PDL 接收 SM 直接提交(per US20230236878A1)

---

## 4. TMD 完整字段布局(per `docs/research/TMU/TMD.md`)

### 4.1 TMD 7 区字段布局

TMD(Task Metadata Descriptor)是 TMU 调度的基本对象,编码 grid 参数、优先级、依赖字段等(per `docs/research/TMU/TMD.md`)。

| 区 | 字段 | 用途 |
|----|------|------|
| **Init 区** | grid_dim / block_dim / kernel_id / shared_mem_size | 初始参数(per kernel launch) |
| **Sched 区** | priority / queue_id / sched_policy | 调度策略(优先级分层) |
| **Exec 区** | entry_point / kernel_args / arg_count | 执行入口与参数 |
| **QueueState 区** | state / active_count / pending_count | 队列状态(per queue) |
| **HW-only 区** | hardware_flags / fence_mask | 硬件独占标志(per US20130198760A1) |
| **Dep 区** | dependent_tmd_enable / dependent_tmd_pointer / consumer_flags(462/464/466) | 依赖管理(per US11182207B2) |
| **Queue 区** | queue_tail / queue_head / ring_pos | 队列指针(per HQD 模型) |

### 4.2 关键字段语义

- **Dependent TMD enable flag**:1-bit 标志,标识此 TMD 完成后自动启动依赖 TMD
- **Dependent TMD pointer**:依赖 TMD 的描述符地址(per US20130198760A1)
- **Consumer Flags (462/464/466)**:per US11182207B2,标识依赖任务预取范围
- **priority**:用于 Scheduler Table 优先级分层(高/中/低三档)
- **hardware_flags**:硬件独占标志,如 fence_mask(避免并发冲突)

### 4.3 TMD 构造(来自 Pm4Decoder)

```cpp
struct TMD {
    // Init 区
    uint32_t grid_dim[3];
    uint32_t block_dim[3];
    uint32_t kernel_id;
    uint32_t shared_mem_size;
    
    // Sched 区
    uint8_t priority;  // 0=high, 1=mid, 2=low
    uint16_t queue_id;
    
    // Exec 区
    uint64_t entry_point;
    uint64_t kernel_args_ptr;
    uint32_t arg_count;
    
    // QueueState 区
    enum State { PENDING, ACTIVE, COMPLETE };
    State state;
    uint32_t active_count;
    
    // HW-only 区
    uint32_t hardware_flags;
    uint64_t fence_mask;
    
    // Dep 区
    bool dependent_tmd_enable;
    uint64_t dependent_tmd_pointer;
    uint32_t consumer_flags;  // 462/464/466 mask
    
    // Queue 区
    uint64_t queue_tail;
    uint64_t queue_head;
    uint32_t ring_pos;
};
```

---

## 5. 依赖任务自动启动(per US20130198760A1)

### 5.1 核心机制

**Dependent TMD Auto-Launch**(per `docs/research/TMU/overview.md` §3.1):

- 前驱任务的 TMD 包含 **Dependent TMD enable flag** + **Dependent TMD pointer** + **字段拷贝使能**
- 前驱任务完成时,TMU **自动启动依赖任务**,**取代信号量同步**
- **零 CPU 介入**:无需 CPU 轮询或中断处理

### 5.2 流程

```
Producer TMD (Dependent enable=true, pointer=&consumer_tmd)
    ↓
Producer 执行完成
    ↓
TMU 收到 Producer 完成事件
    ↓
TMU 验证 Dependent enable flag
    ↓
TMU 解析 Consumer TMD 字段(根据字段拷贝使能)
    ↓
TMU 把 Consumer TMD 加入 Scheduler Table(自动)
    ↓
Consumer 调度执行
```

### 5.3 优势

- **降低 CPU-GPU 同步开销**:无中断/无轮询
- **提高 grid 间依赖执行效率**:Producer → Consumer 自动流水线
- **简化应用编程模型**:声明式依赖(类似 CUDA Stream)

### 5.4 CppTLM 实施(v1.0 MVP)

- **依赖关系编码**:TMD::dependent_tmd_enable + dependent_tmd_pointer 字段
- **自动启动**:TMU 在 producer 完成时验证 + 解析 consumer
- **测试覆盖**:`test_tmu_dispatch_processor_tlm.cc`(per v0.5 MVP 测试)

---

## 6. 依赖预取(per US11182207B2)

### 6.1 Dependency/Prefetch Unit 410

**核心机制**(per `docs/research/TMU/overview.md` §3.2):

- TMU 内置 **Dependency/Prefetch Unit 410**
- 依据生产者 TMD 中的 **Consumer Flags**(462/464/466)在生产者执行期间**并行预取**消费者的描述符/指令/常量
- 预取内容存入 **Consumer Task Descriptor Cache 480**
- flush 完成后递减消费者 **Current Count 444**,归零进入 **Active List 420**
- **Self-Reset Flag 446** 支持循环依赖免软件复位

### 6.2 性能提升

**官方专利数据**:文本称可将**依赖解析延迟削减一半以上**(per US11182207B2)。

### 6.3 CppTLM 实施(v1.0 MVP 新增基础)

- **v1.0 MVP 新增 TMD Cache**:`TMU::tmd_cache_`(基础缓存,~64 entry)
- **Dependency/Prefetch Unit 简化**:仅 prefetch dependent_tmd_pointer 指向的 TMD
- **Consumer Flags 简化**:仅支持标志 462(per US11182207B2)
- **v1.1 完整版**:Self-Reset Flag + 多 Consumer Flags(462/464/466)+ Cache 优化

---

## 7. PDL vs CDP 关键概念澄清

### 7.1 PDL(Programmatic Dependent Launch,CUDA 11.8+)

- **CUDA 版本**:CUDA 11.8+ 起(Hopper 时代)
- **硬件基础**:**US20230236878A1**(WSDU/pre-exit,Hopper 2022 申请,2023 公开)
- **机制**:SM/CU 内部可直接发起 child task,**绕过 pushbuffer + front end + CPU**
- **子任务优先级**:子任务优先级链表**高于停在屏障的父任务**,避免死锁

### 7.2 CDP(CUDA Dynamic Parallelism,CUDA 5.0+)

- **CUDA 版本**:CUDA 5.0+(Kepler 时代)
- **硬件基础**:**US20210349763A1**(嵌套并行计算,2021 公开)
- **机制**:父线程经 `A<<<B>>>C` 发射子 grid;`cudaThreadSynchronize()` 时父 CTA 状态存入 continuation state buffer 并释放 SM
- **子任务提交**:子 grid 经 MMU 的 PCAS(posted compare-and-swap)入队;子任务由 processing cluster array **直接提交给 TMU**

### 7.3 PDL 与 CDP 的关键差异

| 维度 | CDP(CUDA 5.0+) | PDL(CUDA 11.8+) |
|------|-----------------|-------------------|
| **CUDA 版本** | 5.0+ | 11.8+ |
| **专利号** | **US20210349763A1** | **US20230236878A1** |
| **GPU 架构** | Kepler+ | Hopper+ |
| **子任务粒度** | 子 grid 完整启动 | 子任务可更细粒度 |
| **绕过 pushbuffer** | 否 | 是 |
| **绕过 front end** | 否 | 是 |
| **绕过 CPU** | 部分 | 全部 |

### 7.4 CppTLM 实施

- **v0.5 MVP + v1.0 MVP**:**均不实施** PDL/CDP(v0.5 仅占位 ScoreboardTLM/PipelineTLM)
- **v1.1 完整版**:PDL 完整实施(per US20230236878A1)+ CDP 基础支持(per US20210349763A1)
- **专利号不可混用**:`docs/research/TMU/overview.md` 已澄清;`00-overview` v3.0 §1.4 + §3.4 L4.4 已区分

---

## 8. 嵌套流事件(per US9928109B2,v1.1)

### 8.1 流事件核心机制

**跨 stream 依赖无 CPU 介入**(per `docs/research/TMU/overview.md` §3.6):

- stream 映射为 GPU 预分配的 **TMDQ**(Task Metadata Descriptor Queue)
- 跨流依赖经图变换为 **wait event / signal event** 对
- 原子递减 dependency count,归零即推进流,全程**无锁、无 CPU 介入**
- **OOM 退化为单流串行化**:GPU 内存不足时退化为单流串行化保正确性
- **事件逻辑由 AtExit 调度** 的 scheduler kernel 执行

### 8.2 关键概念

- **Wait Event**:consumer 流等待 producer 流信号
- **Signal Event**:producer 流完成后发信号
- **Stream 取消语义**:stream 内任务顺序保证,跨 stream 通过 event 同步

### 8.3 CppTLM 实施

- **v0.5 + v1.0 MVP**:**均不实施** 嵌套流事件
- **v1.1 完整版**:Stream Event Queue + Wait/Signal Event pair + 图变换原子操作

---

## 9. v1.0 战略对齐

### 9.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| L4 层 TMU 基础 | ✅ 优先级分层 + TMD prefetch | ✅ §3 + §6 |
| L4 层 PDL | ❌ v1.0 推迟;v1.1 完整版 | ✅ §7 实施策略 |
| L4 层 嵌套流 | ❌ v1.0 推迟;v1.1 完整版 | ✅ §8 实施策略 |
| PDL vs CDP 区分 | ✅ CUDA 11.8+ / US20230236878A1 | ✅ §7 严格区分 |

### 9.2 v1.0 MVP / v1.1 范围矩阵

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| TMU 基础调度 | ✅ | ✅ |
| TMD Cache(prefetch) | ✅ 基础 64 entry | + 多 Consumer Flags |
| 优先级分层 | ✅ 3 档(高/中/低) | + 5 档 |
| Dependent TMD Auto-Launch | ✅ 基础 | + Self-Reset Flag |
| 嵌套流事件 | ❌ | ✅ per US9928109B2 |
| PDL | ❌ | ✅ per US20230236878A1 |
| CDP | ❌ | ✅ 基础 per US20210349763A1 |

### 9.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-06 D5 | TMU 简化(v0.5)+ v1.0 MVP prefetch + v1.1 PDL/嵌套流 |
| ADR-SOC-09 D2(共享分层) | v1.0 NVIDIA+AMD dual vendor 战略,TMU 共享(不区分 vendor) |

---

## 10. 配置 Schema

### 10.1 顶层 JSON Schema

```json
{
  "name": "tmu_0",
  "type": "TMU",
  "params": {
    "priority_levels": 3,             // 3 档(高/中/低)
    "tmd_cache_size": 64,             // TMD Cache 64 entry
    "max_outstanding_tmds": 1024,     // 最大 pending TMD 数
    "dependent_tmd_auto_launch": true, // 自动启动依赖
    "enable_tmd_prefetch": true,      // 启用依赖预取
    "consumer_flags_mask": "0x1",     // 仅支持 Consumer Flag 462(per US11182207B2)
    "enable_nested_stream": false,     // v1.0 MVP 关闭;v1.1 开启
    "enable_pdl": false               // v1.0 MVP 关闭;v1.1 开启
  }
}
```

### 10.2 连接(connection)示例

```json
{
  "connections": [
    {
      "src": "command_processor_0.tmd_out",
      "dst": "tmu_0.tmd_in"
    },
    {
      "src": "tmu_0.tmd_dispatched",
      "dst": "wdu_0.tmd_in"
    }
  ]
}
```

---

## 11. ADR/微架构/OpenSpec 引用矩阵

### 11.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-06 D5 | TMU 简化 MVP + Pm4Decoder + CommandProcessor 链路 |
| ADR-SOC-09 D2(共享分层) | v1.0 NVIDIA+AMD dual vendor 战略,TMU 共享 |

### 11.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **TMU Dispatch Processor** | [`docs/soc_arch/modules/tmu-dispatch-processor.md`](../modules/tmu-dispatch-processor.md) |
| **CommandProcessor** | [`docs/soc_arch/modules/command-processor.md`](../modules/command-processor.md) |
| **Pm4Decoder** | [`docs/soc_arch/modules/pm4-decoder.md`](../modules/pm4-decoder.md) |
| **Memory Cluster** | [`docs/soc_arch/modules/gpu-memory-cluster.md`](../modules/gpu-memory-cluster.md)(TMD 关联) |

### 11.3 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/TMU/overview.md`](../../research/TMU/overview.md) | NVIDIA TMU 专利综述(8 件 + 3 代形态) |
| [`docs/research/TMU/TMD.md`](../../research/TMU/TMD.md) | TMD 完整字段布局 7 区 |
| [`docs/research/TMU/TMU专利专题整理.md`](../../research/TMU/TMU专利专题整理.md) | 23 件专利族整理(3 代形态) |
| [`docs/research/TMU/US11182207B2_依赖任务描述符预取_解析.md`](../../research/TMU/US11182207B2_依赖任务描述符预取_解析.md) | 依赖预取专利解析 |
| [`docs/research/TMU/US20130198760A1_依赖任务自动启动_解析.md`](../../research/TMU/US20130198760A1_依赖任务自动启动_解析.md) | Dependent TMD auto-launch |
| [`docs/research/TMU/US20230236878A1Efficiently_Launch_task_on_processor.md`](../../research/TMU/US20230236878A1Efficiently_Launch_task_on_processor.md) | WSDU/pre-exit(PDL 硬件基础) |
| [`docs/research/TMU/US20210349763A1_嵌套并行计算_解析.md`](../../research/TMU/US20210349763A1_嵌套并行计算_解析.md) | CDP 嵌套并行 |
| [`docs/research/TMU/US9928109B2_嵌套流事件处理_解析.md`](../../research/TMU/US9928109B2_嵌套流事件处理_解析.md) | 嵌套流事件 |

---

## 12. 风险与缓解 R1-R5

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | PDL 与 CDP 专利号混用 | 🟢 低 | `00-overview` v3.0 §7 已区分;本文 §7 再次区分;后续 ADR/微架构文档遵循 |
| **R2** | TMD 7 区字段实施复杂 | 🟡 中 | v1.0 MVP 实施核心 4 区(Init/Sched/Exec/Dep);v1.1 完整版追加 QueueState/HW-only/Queue |
| **R3** | 依赖预取 Cache 大小与命中率平衡 | 🟡 中 | v1.0 MVP 基础 64 entry;v1.1 完整版根据 L2/L3 cache 容量调整 |
| **R4** | 嵌套流事件 v1.0 推迟,可能影响部分应用 | 🟡 中 | v1.1 完整版追加;v1.0 MVP 应用可绕过嵌套流用单流实现 |
| **R5** | Self-Reset Flag(per US11182207B2 446)实施复杂 | 🟢 低 | v1.1 完整版追加,v0.5 + v1.0 MVP 不实施 |

---

## 13. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L4 TMU/TMD 子系统架构,基于 NVIDIA TMU 三代形态 + TMD 7 区 + 依赖预取 + PDL/CDP 区分) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS