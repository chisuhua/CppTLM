# 11. CDNA 真实 ISA 集成架构（双轨前端 + 统一 Timing 宿主）

> **版本**: v1.0-draft (2027-02-09)
> **状态**: 📋 Proposed
> **作者**: CppTLM Team (Sisyphus)
> **关联 ADR**:
> - [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md)（NVIDIA + AMD 双 Vendor 战略）
> - [`ADR-SOC-11-pcie-endpoint-ip.md`](../adr/ADR-SOC-11-pcie-endpoint-ip.md)（17 ports PcieEndpointIP 整合）
> - [`ADR-SOC-15-cdna-real-isa-roadmap.md`](../adr/ADR-SOC-15-cdna-real-isa-roadmap.md)（演进路线图）
> **关联研究**:
> - Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP`
> - MGPUSim 公开实现（AMD GCN3 emulator + timing）
> - gem5 多 ISA 架构（StaticInst / DynInst 描述符）
> - accel-sim SASS 前端（trace-driven functional）
> **目的**: 文档化 CppTLM 从虚拟 PTX 演进到类 AMD CDNA 真实物理 ISA 的**架构范式、Seam 接口设计、与 CDNA 微架构适配**。路线图与阶段门禁见 ADR-SOC-15。

---

## 11.1 顶层架构范式

### 11.1.1 双轨前端 + 统一 Timing 宿主

为实现 **“前期 PTX 快速起跑，后期平滑换装 CDNA”**，系统采用 **“前端指令执行器 (Functional Execution) + 统一微架构时序宿主 (Timing Core)”** 的解耦架构，参考业界已验证范式：

| 业界参考 | 关键做法 | 对本项目启示 |
 |---|---|---|
| **gem5 多 ISA** | 各 ISA 独立 decode/execute，共享 O3/Minor CPU timing；decode 出口统一为 `StaticInst`/`DynInst` 描述符 | **ISA 中立 timing core 范式成立**；描述符是关键抽象 |
| **MGPUSim** | AMD GCN3 emulator + timing 分离，emulator 维护 `s_waitcnt` 计数器，timing 负责 Cache/NoC/DRAM | **与本项目结构最相似**；证明 AMD emulator + 外部 timing 闭环可行 |
| **accel-sim** | NVbit trace-driven SASS + 独立 timing core | **timing core 完全 ISA-agnostic**；前端可降级为 trace replay |

### 11.1.2 整体架构图

```
              ┌──────────────────────────────┐        ┌──────────────────────────────┐
              │ Frontend A: PTX-EMU         │        │ Frontend B: CDNA-EMU        │
              │ (虚拟 ISA / 解释执行 / 阶段1) │        │ (物理 ISA / GFX942 解码 / 阶段3)│
              └──────────────┬──────────────┘        └──────────────┬───────────────┘
                             │ 产出 InstrDescriptor                │ 产出 InstrDescriptor
                             └───────────────────┬──────────────────┘
                                                 ▼
                             ┌──────────────────────────────────────┐
                             │   标准化指令描述符 (InstrDescriptor) │
                             │   + 调度控制元数据 (CtrlBits)        │
                             └───────────────────┬──────────────────┘
                                                 │
                      ┌──────────────────────────┴──────────────────────────┐
                      ▼                                                     ▼
        ┌───────────────────────────┐                         ┌───────────────────────────┐
        │  IComputeDevice 步进驱动 │                         │   IMemoryPort 异步内存    │
        │ (1 tick = 1 cycle step) │                         │   (NoC / Cache / HBM)   │
        └─────────────┬─────────────┘                         └─────────────┬─────────────┘
                      │                                                     │
══════════════════════╪═════════════════════════════════════════════════════╪══════════════════════════════
                      ▼                                                     ▼
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                          CppTLM Microarchitecture Timing Host                          │
│                                                                                             │
│  ┌─────────────────────────┐  ┌─────────────────────────┐  ┌─────────────────────────────┐  │
│  │  PipelineTLM (查表)   │  │ MatrixCoreTLM (MFMA)  │  │  HazardTracker (waitcnt/sb)│  │
│  │  (LatencyClass 枚举) │  │ (CDNA3 FP16/BF16/INT8) │  │  (显式计数器与屏障仲裁)     │  │
│  └─────────────────────────┘  └─────────────────────────┘  └─────────────────────────────┘  │
│                                                                                             │
│  ┌───────────────────────────────────────────────────────────────────────────────────────┐  │
│  │ NoC Interconnect + L1/L2 CacheCluster + MemoryCluster (HBM3e)（周期精确数据网）   │  │
│  └───────────────────────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
```

### 11.1.3 与现有架构的关系

- **保留**：HSK-6 ACCEPTED 的 `IPtxEmuDevice` 12 方法骨架（生命周期、单步推进、状态查询）
- **重构**：`set_scoreboard` + `attach_timing` 中的 `IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming` 三个 vendored 接口
- **新增**：`IMemoryPort` 异步内存事务接口（真实 ISA 前置条件）
- **保留不变**：23 ABI 冻结（`include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h`）+ PcieEndpointIP 17 端口布局

---

## 11.2 核心 Seam 接口定义

### 11.2.1 指令级 Seam：`InstrDescriptor`（标准化指令描述符）

**目的**：完全废弃 `PipelineTLM` 原有的 `has(instr, "fma")` 字符串子串查表机制。在执行器的 Decode 阶段产出无 ISA 依赖的 POD 描述符。

**位置**：`include/tlm/gpu/instruction_descriptor.hh`（阶段 A 新增）

```cpp
namespace cpptlm::gpu {

// 处理器管道分类（跨 ISA 通用）
enum class PipeClass : uint8_t {
    kScalarALU,      // AMD: SALU (s_add, s_mov...)
    kVectorALU,      // AMD: VALU (v_add_f32, v_fma_f32...)
    kMatrixCore,     // AMD: MFMA (v_mfma_f32_16x16x16_fp16...)
    kBranch,         // AMD: s_branch, s_cbranch...
    kLSU_Global,     // AMD: global_load, global_store
    kLSU_LDS,        // AMD: ds_read, ds_write
    kSpecialSync     // AMD: s_waitcnt, s_barrier
};

// 延迟类（数值查表，CppTLM 维护 LatencyClass → cycles 映射）
enum class LatencyClass : uint8_t {
    kFixed1Cycle,
    kFixed4Cycles,
    kFixed8Cycles,
    kFixed16Cycles,
    kMatrixHeavy,
    kMemoryVariable  // 必须通过 IMemoryPort 由 NoC 实测返回
};

// 调度控制位（CDNA s_waitcnt 显式计数器示例，PTX 阶段全部 0）
struct CtrlBits {
    uint8_t vmcnt_req   = 0;  // 发射时增加的未完成 Vector 内存计数
    uint8_t lgkmcnt_req = 0;  // 发射时增加的 LDS/GDS/Constant 计数
    uint8_t expcnt_req  = 0;  // 导出计数
    uint8_t wait_vmcnt   = 0xFF;  // s_waitcnt 要求的 vmcnt 阈值 (0xFF = 不等待)
    uint8_t wait_lgkmcnt = 0xFF;
    uint8_t wait_expcnt  = 0xFF;
};

// 标准化指令描述符（CppTLM 仅消费此结构）
struct InstrDescriptor {
    PipeClass    pipe;
    LatencyClass latency_class;
    CtrlBits     ctrl;

    uint16_t dst_regs[4];
    uint16_t src_regs[4];
    uint8_t  num_dst = 0;
    uint8_t  num_src = 0;

    bool     is_memory = false;
    uint64_t target_vaddr = 0;
    uint32_t mem_size     = 0;
};

} // namespace cpptlm::gpu
```

### 11.2.2 计算设备 Seam：`IComputeDevice`（泛化 `IPtxEmuDevice`）

**目的**：将带有 PTX 命名印记的 `IPtxEmuDevice` 升级为通用计算设备接口，支持 PTX-EMU / CDNA-EMU / 未来 RVV-EMU 等多前端实现。

**位置**：`include/tlm/gpu/compute_device_interface.hh`（阶段 C 新增）

```cpp
class IComputeDevice {
public:
    virtual ~IComputeDevice() = default;

    // 生命周期
    virtual bool initialize(const DeviceConfig& cfg) = 0;
    virtual void shutdown() = 0;

    // 单步驱动：CppTLM tick() 推进时调用，推进 1 个 SM 核心周期
    virtual int  exe_once() = 0;
    virtual int  sm_exe_once(uint32_t sm_id) = 0;
    virtual int  wave_exe_once(uint32_t sm_id, uint32_t wave_id) = 0;

    // 指令感知钩子：Decode 完成后立即通知 Timing Core 登记 hazard/counter 状态
    virtual bool peek_next_instruction(uint32_t sm_id, uint32_t wave_id,
                                       cpptlm::gpu::InstrDescriptor& out_desc) = 0;

    // 状态查询
    virtual bool is_wave_stalled(uint32_t sm_id, uint32_t wave_id) = 0;
    virtual bool is_finished() = 0;
};
```

### 11.2.3 内存系统 Seam：`IMemoryPort` 异步内存事务接口

**目的**：彻底推翻“查表得 200 cycles”的假内存模型。Load/Store 指令转变为向 CppTLM 注入的真实总线事务，由 NoC + CacheCluster + MemoryCluster 提供周期精确延迟。

**位置**：`include/tlm/gpu/memory_port_interface.hh`（阶段 B 新增）

```cpp
struct MemRequest {
    uint64_t vaddr;
    uint32_t size;
    bool     is_write;
    uint32_t sm_id;
    uint32_t wave_id;
    uint64_t tag;       // 用于异步匹配的回执 ID
};

class IMemoryPort {
public:
    virtual ~IMemoryPort() = default;

    // 前端执行器发射内存请求进 CppTLM 互联层（非阻塞，队列满返回 false）
    virtual bool send_request(const MemRequest& req) = 0;

    // CppTLM 内存系统向前端执行器回报已完成事务（异步回调）
    virtual void set_response_callback(
        std::function<void(uint64_t tag, uint32_t sm_id, uint32_t wave_id)> cb) = 0;
};
```

**CppTLM 端实现**：`GpuMemoryPortAdapter`（阶段 B 新增）将 `MemRequest` 打包为 `CacheReqBundle` / `Axi4Bundle`，直连 `L1/L2 CacheCluster`，处理完经回调回写。

---

## 11.3 类 AMD CDNA 微架构适配设计

### 11.3.1 线程束拓扑（Wave64 & SIMD4）

**CDNA 特性**：
- 每个 CU 含 4 个 SIMD16 单元（或分时 SIMD32）
- 默认执行 **Wave64**（64 线程/Wavefront，执行 4 拍或 2 拍）
- 每 Wave 内活跃线程通过 **EXEC mask**（AMD 64-bit）控制

**CppTLM 适配**：
- 将 `active_mask` 状态字段由 32-bit 升级为 64-bit `uint64_t exec_mask`
- 支持 `EXEC_LO`（低 32 bit）+ `EXEC_HI`（高 32 bit）双寄存器模型
- `Warp` 抽象更名为 `Wavefront`，但保持 17 端口命名兼容（per `IComputeDevice::wave_exe_once`）

### 11.3.2 显式计数器危险检测（`CDNAHazardTracker`）

**CDNA 模型**：编译器显式插入 `s_waitcnt` 指令维护三个硬件计数器：

| 计数器 | 监控对象 | 每发射 `global_load` | 每回调完成 |
|--------|----------|---------------------|-----------|
| `vmcnt` | 未完成 Vector Memory（全局/缓存读） | `vmcnt++` | `vmcnt--` |
| `lgkmcnt` | LDS/GDS/Constant 读写 | `lgkmcnt++` | `lgkmcnt--` |
| `expcnt` | Export 操作 | `expcnt_req` | `expcnt--` |

**挂起/就绪逻辑**（`HazardTracker` 实现）：

```
front-end 执行 s_waitcnt vmcnt(0):
    if (CDNAHazardTracker.vmcnt() > 0):
        mark wavefront as Stalled_Memory
        skip in exe_once  # 不推进该 wavefront
    else:
        allow wavefront to proceed

# CppTLM 端 IMemoryPort 响应回调:
    on response(tag, sm_id, wave_id):
        CDNAHazardTracker.decrement(sm_id, wave_id)
        unblock wavefront if all waitcnt satisfied
```

**位置**：`include/tlm/gpu/cdna_hazard_tracker.hh`（阶段 C 新增）

### 11.3.3 张量核心映射（`MatrixCoreTLM`）

**重命名与扩面**：
- 原 `TensorCoreTLM` 重构为 `MatrixCoreTLM`，覆盖 CDNA 3 MFMA 指令集
- A100 MMA 数据仍保留为 `kMatrixHeavy` LatencyClass，CDNA MFMA 单独定义 latency class

**CDNA 3 主流 MFMA 指令覆盖**（阶段 C 必交付）：

| 指令 | Throughput (cycles) | TensorCoreTLM 映射 |
|------|---------------------|---------------------|
| `V_MFMA_F32_32X32X8_FP16` | 64 | kMatrixHeavy_MFMA_Large |
| `V_MFMA_F32_16X16X16_FP16` | 32 | kMatrixHeavy_MFMA_Medium |
| `V_MFMA_F32_32X32X16_BF16` | 64 | 同上 |
| `V_MFMA_I32_16X16X32_INT8` | 32 | kMatrixHeavy_MFMA_INT8 |
| `V_MFMA_F16_16X16X16_FP16` | 32 | kMatrixHeavy_MFMA_F16 |

**TC delegation**：复用现有 PTX 阶段的"P4_TC 返回 0.0"语义（per `pipeline_tlm.cc:107`），由 `MatrixCoreTLM` 单独计算 latency。

---

## 11.4 实施约束与不变量

### 11.4.1 严格禁止（Anti-patterns）

- ❌ CppTLM 核心库禁止包含 PTX-EMU 或 CDNA-EMU 的内部头文件
- ❌ CppTLM 禁止维护字符串形式的指令查表（`has(instr, "fma")` 等）
- ❌ CppTLM 禁止做 ISA 语义推测（推断收敛点、控制流发散合并等）
- ❌ 禁止在 PR 中增量加 `IComputeDevice` 方法（必须通过 HSK-N bump VERSION）

### 11.4.2 严格保护（Invariants）

- 23 ABI 冻结（`include/abi/cpptlm_emulator.h`）—— **零修改**
- `PcieEndpointTLM` 4 端口布局冻结 —— **仅加 `[[deprecated]]` 标注**
- `PcieEndpointIP` 17 端口布局 —— 保留
- `PTXEMU_API_VERSION=1` 冻结 —— **变更为 `ICOMPUTE_API_VERSION=1` 需 HSK-9**

### 11.4.3 CppTLM Clean Room 原则

- PTX-EMU / CDNA-EMU / RVV-EMU 均作为 `external/` 子模块或动态库注入
- CppTLM 仅感知：`InstrDescriptor` + `IComputeDevice` + `IMemoryPort`
- 各执行器自治维护 ISA 私有状态（PTX SIMT stack / CDNA EXEC mask / RVV vector length）

---

## 11.5 验收门禁（与 ADR-SOC-15 路线图对齐）

| 阶段 | 验收门禁 | 对应架构层 |
|------|----------|------------|
| 阶段 A（中立化） | `[pcie][axi][e2e][wave2]` 全绿；PTX-EMU 输出与改造前 bit-identical | §11.2.1 InstrDescriptor |
| 阶段 B（内存 Seam） | 内存延迟动态波动；G-D8 chaos test 闭环 | §11.2.3 IMemoryPort |
| 阶段 C（CDNA 接入） | SGEMM/Vector Add CDNA kernel 通过；周期数与官方白皮书 ±15% | §11.3 全套 |
| 阶段 D（双轨切换） | 切换 `"isa_type": "ptx"` / `"cdna3"` 两种模式各自完整回归 | §11.1 整体架构 |

---

## 11.6 已知限制与未来工作

### 已知限制
- **Phase A 之前的 PTX 路径仍依赖字符串查表**：当前 `PipelineTLM::get_fractional_cycles(instr, pipe)` 内部含 `has(instr, "fma")` 等 6 类子串匹配（`pipeline_tlm.cc:21-34`）。Phase A 之前不修复（避免 scope creep）
- **多 SM 拓扑支持未完成**：当前 `attach_timing` hardcode `sm_id=0`（per PTX-EMU AGENTS.md 注释），阶段 C 需扩展
- **PRF bank conflict 模型缺失**：阶段 D 后续增强

### 未来工作（不在本版本范围）
1. 内存回调接口向 **NoC + HBM3e 实测** 切换（阶段 B 档位 A → 档位 B）
2. RVV 接入时 `IComputeDevice` 去 wave 化（`wave_exe_once` → `sched_unit_exe_once`）
3. NVIDIA SASS 接入（不推荐为第一目标，per Oracle 评估 — 无公开 ISA 手册）

---

## 11.7 参考文献

| 文献 | 关键内容 |
|------|----------|
| Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP` | gpgpu-sim / gem5 / MGPUSim 范式对比 + PTX→真实 ISA 演进路径 |
| Oracle 演进分析 `ses_f982f1597ffejYGzVek5F7zBfP` follow-up | 4 阶段路线图 + 选型 + 风险 |
| HSK-6 (`369cf71`) | CppTLM-PTX-EMU 桥接关系废止 + 14 PtxEmuDriverShim 符号物理消除 |
| HSK-8 ACCEPTED | `IPtxEmuDevice` 12/12 wired + drift_check 6 invariants PASS |
| `external/PTX-EMU/include/ptxemu/device_api.h` | 12 方法接口定义（升级前的 `IPtxEmuDevice` 蓝本） |
| MGPUSim（accel-sim/gpgpu-sim_distribution） | AMD GCN3 emulator + timing 分离参考实现 |
| gem5 多 ISA | `StaticInst` / `DynInst` 描述符参考 |