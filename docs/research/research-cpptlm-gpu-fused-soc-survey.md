# CppTLM → CPU+GPGPU Fused SoC 调研报告

> **调研日期**: 2026-06-11
> **调研目标**: 为 CppTLM（C++ TLM 2.0 混合仿真框架）规划 CPU+GPGPU 融合 SoC 仿真能力的建设路径。
> **蓝本参考**: gem5 `develop` 分支的 `configs/example/apu_se.py`（APU 形态，shared DDR）+ `configs/example/gem5_library/x86-mi300x-gpu.py`（dGPU 形态，disjoint network + HBM2）。
> **用户决策**（2026-06-11）:
> - **形态选择**: APU-first（共享 single RubySystem + 单 DDR）；dGPU 作为 APU 完成后的 Phase 2 备选
> - **D1 协议策略**: 分步走 — Phase0-1 用简化 I/S/M 三态 + write-through bypass；Phase2+ 升级为 MOESI + GPU_VIPER 简化版（用 C++ `switch` 表，不复制 gem5 slicc 代码）
> - **D2 CU 粒度**: 黑盒优先（只发请求，不模拟 5-stage pipeline / ISA / SIMD）
> - **D3 Wavefront/Coalescing**: 抽象（用 `coalescing_factor` 参数代替精确模拟）
> - **D4 HSAPP/CP/Dispatcher**: 简化到极致（用 `KernelLaunchTLM` ~150 行代替 HSA 三件套 3000+ 行）
> - **D5 目录结构**: 新建 `include/tlm/gpu/` 子目录（`compute_unit_tlm.hh` / `tcc_tlm.hh` / `kernel_launch_tlm.hh` / `pcie_bridge_tlm.hh`）
> **范围约束**: CPU 建模不在范围内（沿用现有 `TrafficGenTLM` / `CPUTLM`）；零新增外部依赖。

---

## 0. 阅读引导

本文档结构：

1. **§1 现状底图** — CppTLM v2.1 已有 TLM 模块 + GPU 资产盘点
2. **§2 gem5 CPU+GPGPU 融合 SoC 全景** — APU + dGPU 双形态深度调研
3. **§3 关键架构决策**（D1-D5 完整论证 + 已采纳建议）
4. **§4 6 阶段执行路径**（Phase0-5）+ Phase 2 备选 dGPU
5. **§5 最高风险与缓解** + **§6 MVP 定义** + **§7 Anti-Recommendations**
6. **§8 关键 permalink 索引**

读者若关心"做不做"，直接读 §3 §4 §7；若关心"为什么"，读 §1 §2 §8。

---

## 1. CppTLM v2.1 现状底图

### 1.1 已有的 TLM 模块（10 个头文件）

| 模块 | Header | 端口形态 | 角色 |
|------|--------|----------|------|
| `CacheTLM` | `include/tlm/cache_tlm.hh` | 单端口 req/resp | 简化 L1（hit 5cy / miss 50cy，std::map 查表） |
| `MemoryTLM` | `include/tlm/memory_tlm.hh` | 单端口 | 简化 DRAM 桩（rd 100cy / wr 120cy + 行缓冲） |
| `CrossbarTLM` | `include/tlm/crossbar_tlm.hh` | 4 端口 | 按 `addr>>12&3` 路由 + 冲突检测 |
| `CPUTLM` | `include/tlm/cpu_tlm.hh` | 单端口 initiator | 0x1000–0x1100 线性只读，10cy 间隔 |
| `TrafficGenTLM` | `include/tlm/traffic_gen_tlm.hh` | 单端口 initiator | 3 基础 + 4 压力模式（SEQUENTIAL/RANDOM/TRACE + HOTSPOT/STRIDED/NEIGHBOR/TORNADO） |
| `ArbiterTLM<N>` | `include/tlm/arbiter_tlm.hh` | N→1 + 1 出 | Round-Robin，txn→port 表回送响应 |
| `tlm::RouterTLM` | `include/tlm/router_tlm.hh` | 5 端口双向 | 六阶段流水线 + Credit 控流 + XY 路由 |
| `tlm::NICTLM` | `include/tlm/nic_tlm.hh` | PE 侧 + Network 侧非对称双端口 | `FLITS_PER_PACKET=4` packetize/reassemble |
| `tlm::LinkTLM` | `include/tlm/link_tlm.hh` | 单端口 NoC flit | 物理链路延迟 + Credit 传递 |
| `cpptlm::rtl::HybridCacheWrapper` | `include/rtl/hybrid_cache_wrapper.hh` | PIMPL | TLM↔RTL 桥接（CppHDL HybridCacheComponent） |

**基类层级**: `SimObject → ChStreamModuleBase → 具体类`
**注册宏**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:29-65`）一次性注册 9 个模块 + 对应 StreamAdapter
**Bundle 体系**: `CacheReqBundle` / `CacheRespBundle`（含 `fragment_*` 字段）+ `NoCFlitBundle`（REQUEST/RESPONSE/CREDIT 统一）
**StreamAdapter 4 种形态**: 单端口 / 多端口 / 双向（Credit 队列）/ 双端口非对称（NIC 用）

### 1.2 GPU 资产盘点 — **零**

通过全仓 grep（`gpu`, `GPU`, `shader`, `Shader`, `wavefront`, `Wavefront`, `SIMD`, `kernel`, `Kernel`, `HSA`, `compute`, `Compute`, `accelerator`, `apu`, `APU`, `dgpu`, `dGPU`, `HBM`, `hbm`）+ glob（`**/*gpu*.*` / `**/*shader*` / `**/*compute*` / `**/*hbm*` 等）+ `git log --all --grep='gpu|GPU|shader'` 三重验证：

**真实 GPU C++ 代码**: **0 个文件**。

**字符串提及均为假阳性**:
- `configs/cpu_group.json` 中 `gpu0` 实际是 `TrafficGenTLM`（CPU 流量发生器），命名意图未实现
- `test/test_domain_boundary.cc` 中 `cpu_domain`/`gpu_domain`/`bridge_cpu_gpu` 是 CoherenceDomain API 字符串测试夹具
- `test/python/test_topo_*.py` 中 `add_module('gpu0', 'GPUTLM')` 是 Python DSL 测试输入，**C++ 注册表中无 GPUTLM**
- `roadmap.md` Phase 6.1 / 6.4 任务条目 🟡 Pending
- `docs/research-gem5-soc-survey.md` 是 gem5 调研参考材料

**git 历史**: 无任何 `gpu/shader/compute/hbm/wavefront` 关键词的 commit/PR/分支。**GPU 工作从未启动**。

### 1.3 已具备的 GPU 桥接基础设施

虽然 CppTLM 没有 GPU 模块，但其**通用基础设施已具备 GPU 桥接的全部形态学准备**：

| Adapter / Bundle | 现有位置 | GPU 适配评估 |
|-----------------|----------|--------------|
| `DualPortStreamAdapter<M, PE_R, PE_RR, Net_R, Net_RR>` | `include/framework/dual_port_stream_adapter.hh` | **CPU↔GPU 桥接关键模板**（PE 侧 CacheBundle + Net 侧 NoCFlitBundle） |
| `BidirectionalPortAdapter<M, BundleT, N>` | `include/framework/bidirectional_port_adapter.hh` | **GPU 内部 Mesh/Crossbar**（N 端口双向 wormhole + Credit） |
| `MultiPortStreamAdapter<M, Req, Resp, N>` | `include/framework/multi_port_stream_adapter.hh` | GPU Memory Controller 多通道扇出 |
| `CoherenceDomain` | `include/core/coherence_domain.hh` | MOESI + snoop fanout + home node + 跨域 bridge — **MOESI_AMD_Base / GPU_VIPER 协议底座** |
| `CoherenceState` 枚举 | `include/ext/error_context_ext.hh` | I/S/E/M/O/T — MOESI 完整状态机 |
| `CacheReqBundle/CacheRespBundle/NoCFlitBundle` | `include/bundles/` | 加 `kernel_id/workgroup_id/wavefront_id` 字段即可扩展为 `ComputeReqBundle` |
| `REGISTER_CHSTREAM_DUAL` 宏 | `include/chstream_register.hh` | 双端口注册（已用于 NICTLM，可直接套用 GPU 桥模块） |
| `validate_domain_boundary` | `src/core/module_factory_validate.cc:445` | CPU↔GPU 域边界 API 已就绪 |

**结论**: CppTLM 是一个 **CPU Cache/NoC 框架，GPU 资产为零**，但桥接基础设施已完备。预计 Phase 0-1（GPU Bundle + MemoryBridge）可在 2-4 周内完成，仅需新增 GPU Bundle、ComputeUnitTLM 类、GPU Domain DSL 扩展。

---

## 2. gem5 CPU+GPGPU 融合 SoC 全景

### 2.1 APU 形态（Integrated GPU）

**主配置**: `configs/example/apu_se.py`（Raven Ridge gfx902 风格，可通过 `--dgpu` flag 切到 dGPU 形态）

APU = CPU die + GPU die **共享**同一物理 DDR 内存池，由 `MOESI_AMD_Base`（目录）+ `GPU_VIPER`（GPU 一侧状态机）通过单一 `RubySystem` + 单一 `mem_ranges` 提供一致性。

#### 拓扑骨架

```
[CPU1..N] (X86TimingSimpleCPU / X86O3CPU)
 └── icache_port/dcache_port → ruby_port.in_ports
 └── MMU walker → ruby_port.in_ports
 └── interrupts[0].pio → piobus
[Shader] (ClockedObject, gfx-version = "gfx902")
 ├── CUs[0..n_cu-1] (ComputeUnit)
 │ ├── n_wf × SIMD × Wavefront
 │ ├── VectorRegisterFile + ScalarRegisterFile + RegisterFileCache
 │ ├── RegisterManager (DynPoolManager × SIMD)
 │ ├── LdsState (banks=32, size=65536)
 │ ├── ldsBus ←→ ldsPort
 │ ├── memory_port[0..63] (wavefront-size mem ports)
 │ ├── gmTokenPort
 │ ├── sqc_port / sqc_tlb_port (I-cache + ITLB)
 │ ├── scalar_port / scalar_tlb_port
 │ └── translation_port[0..63] (per-lane D-TLB)
 ├── GPUDispatcher
 ├── GPUCommandProcessor
 └── HSAPacketProcessor (PIO @0xE00000000 base, numHWQueues=10)

[System]
 ├── piobus = IOXBar(width=32)
 ├── Ruby.create_system(...) → 单一 GPU_VIPER Ruby 网络
 ├── dma_cntrl* (与 HSAPP/CP 同频)
 └── mem_ranges = [AddrRange(mem_size)] (DDR4 host memory, 无独立 VRAM)
```

#### Shader/CU 创建（`apu_se.py:439-498` 摘要）

```python
shader = Shader(n_wf=args.wfs_per_simd, cu_per_sqc=args.cu_per_sqc, ...)
# 协议一致性 acquire/release 设置
if buildEnv["PROTOCOL"] == "GPU_VIPER":
    shader.impl_kern_launch_acq = True
    shader.impl_kern_end_rel = False  # write-through 协议无需 release

compute_units.append(ComputeUnit(
    cu_id=i, perLaneTLB=per_lane, num_SIMDs=args.simds_per_cu,
    wf_size=args.wf_size,
    localDataStore=LdsState(banks=args.numLdsBanks,
                             bankConflictPenalty=args.ldsBankConflictPenalty,
                             size=args.lds_size)))
```

#### HSAPP/CP/Dispatcher 装配（`apu_se.py:599-617`）

```python
gpu_hsapp = HSAPacketProcessor(pioAddr=hsapp_gpu_map_paddr, numHWQueues=args.num_hw_queues)
dispatcher = GPUDispatcher(kernel_exit_events=True)
gpu_cmd_proc = GPUCommandProcessor(hsapp=gpu_hsapp, dispatcher=dispatcher)
gpu_cmd_proc.pio = system.piobus.mem_side_ports
gpu_hsapp.pio = system.piobus.mem_side_ports
shader.dispatcher = dispatcher
shader.gpu_cmd_proc = gpu_cmd_proc
```

### 2.2 dGPU 形态（Discrete GPU + HBM2）

**主配置**: `configs/example/gem5_library/x86-mi300x-gpu.py`（MI300X / 派生 MI355X）

#### 与 APU 关键差异

| 维度 | APU | dGPU |
|------|-----|------|
| GPU 设备内存 | 共享 host DDR | **独立 HBM2**（`HBM2Stack(size="16GiB")`） |
| 网络拓扑 | 单一 RubySystem | **Disjoint Network**（CPU 一套 + GPU 一套独立 RubySystem） |
| CPU↔GPU 桥接 | TCC（共享目录） | **PCIe**（`AMDGPUDevice.upstream → board.get_pci_host()`）+ 一组 DMA ports |
| 协议 | MOESI_AMD_Base + GPU_VIPER（共享） | CPU=MOESI_AMD_Base / GPU=GPU_VIPER（双独立协议） |
| 目录归属 | TCC + 共享 directory | CPU 目录 + GPU 目录（`CPUonly=False, GPUonly=True`） |

#### MI300X 默认配置（`src/python/gem5/components/devices/gpus/amdgpu.py:171-203`）

```python
class MI300X(BaseViperGPU):
    def __init__(self, gpu_memory, num_cus=40, cu_per_sqc=4,
                 tcp_size="16KiB", tcp_assoc=16,
                 sqc_size="32KiB", sqc_assoc=8,
                 scalar_size="32KiB", scalar_assoc=8,
                 tcc_size="256KiB", tcc_assoc=16,
                 tcc_count=16,           # 16 个 TCC (≈2 per XCD)
                 cache_line_size=64):
        ...
        self.device.device_name = "MI300X"
        self.device.DeviceID = 0x74A1
        self.device.BAR5 = PciMemBar(size="2MiB")
```

#### dGPU 内存路径（`viper_shader.py:124-165`）

```python
device.cp = self.gpu_cmd_proc
device.device_ih = AMDGPUInterruptHandler()
device.memory_manager = AMDGPUMemoryManager(cache_line_size=self._cache_line_size)
device.dma = ...                              # CPU-side DMA, 走 PCIe
device.PXCAPBaseOffset = 0x80
device.PXCAPDevCtrl2 = 0x0040                 # ← 启用 PCIe atomic requestor
device.BAR0 = PciMemBar(size=f"{self._vram_size}B")  # BAR0 = VRAM
```

`AMDGPUDevice` (`dev/amdgpu/amdgpu_device.hh:55-65`) 按 BAR 解码：
- BAR0 → VRAM frame (HBM2)
- BAR2 → doorbell (MMIO 到 CP/SDMA/PM4)
- BAR5 → MMIO 控制寄存器

**关键限制**（`x86-mi300x-gpu.py` 注释）:
> # Note: Only KVM and ATOMIC work due to buggy MOESI_AMD_Base protocol.

### 2.3 GPU 内部微架构（Shader → CU → SIMD/WF/LDS/SQC/TCC）

```
Shader (=1 GPU 软件实例)
├── n_cu × ComputeUnit
│ ├── N_SIMD × VectorRegisterFile (vreg_file_size regs)
│ ├── N_SIMD × ScalarRegisterFile (sreg_file_size regs)
│ ├── N_SIMD × RegisterFileCache (可选)
│ ├── N_SIMD × n_wf × Wavefront (per-SIMD per-slot)
│ ├── LdsState (banks × bankConflictPenalty cycle/冲突)
│ │ └── LdsChunk[dispatchId][wgId]  ← per-WG 切片，refCounter 计数
│ ├── RegisterManager (DynPoolManager × N_SIMD)
│ ├── Memory Ports:
│ │   memPort[wf_size=64]         → VIPERCoalescer → TCP (L1, 16KB)
│ │   scalarDataPort              → Scalar cache (32KB)
│ │   sqcPort                     → SQC (L1 Icache, 32KB)
│ │   ldsPort                     → ldsBus → LdsState.cuPort
│ │   gmTokenPort                 → TCP (back-pressure)
│ │   tlbPort[wf_size]            → l1_coalescer → L1 TLB → L2 TLB → L3 TLB
│ ├── Pipeline Stages (per cycle):
│ │   FetchStage → ScoreboardCheckStage → ScheduleStage → ExecStage
│ │     ↘ GlobalMemPipeline ↗
│ │     ↘ LocalMemPipeline ↗
│ │     ↘ ScalarMemPipeline ↗
│ └── ComputeUnitParams
├── GPUDispatcher (HSA task → WG pool → CUs)
├── GPUCommandProcessor (HSAPP ↔ kernel code)
└── HSAPacketProcessor (PIO 监听 doorbell, AQL ring)
```

#### ComputeUnit 关键字段（`src/gpu-compute/compute_unit.hh:185-225`）

```cpp
class ComputeUnit : public ClockedObject {
public:
    FetchStage fetchStage;
    ScoreboardCheckStage scoreboardCheckStage;
    ScheduleStage scheduleStage;
    ExecStage execStage;
    GlobalMemPipeline globalMemoryPipe;
    LocalMemPipeline localMemoryPipe;
    ScalarMemPipeline scalarMemoryPipe;

    std::vector<std::vector<Wavefront *>> wfList;
    int cu_id;
    std::vector<VectorRegisterFile *> vrf;
    std::vector<ScalarRegisterFile *> srf;
    std::vector<RegisterFileCache *> rfc;

    int simdWidth;
    Cycles issuePeriod;
    // ...
    LDSPort ldsPort;
    std::vector<DataPort> memPort;
    std::vector<DTLBPort> tlbPort;
    ScalarDataPort scalarDataPort;
    SQCPort sqcPort;
    GMTokenPort gmTokenPort;
};
```

#### Pipeline stage 接口（`compute_unit.hh:280-290`）

```cpp
// SCB→SCH: readyList
// SCH→EX: dispatchList + readyInst
ScoreboardCheckToSchedule scoreboardCheckToSchedule;
ScheduleToExecute scheduleToExecute;

class ScoreboardCheckToSchedule : public PipeStageIFace {
    void markWFReady(Wavefront *wf, int func_unit_id);
    std::vector<Wavefront *> &readyWFs(int func_unit_id);
};

class ScheduleToExecute : public PipeStageIFace {
    GPUDynInstPtr &readyInst(int func_unit_id);
    void dispatchTransition(const GPUDynInstPtr &gpu_dyn_inst,
                            int func_unit_id, DISPATCH_STATUS disp_status);
};
```

#### Wavefront 状态机（`src/gpu-compute/wavefront.hh:78-118`）

```cpp
class Wavefront : public SimObject {
public:
    enum status_e {
        S_STOPPED, S_RETURNING, S_RUNNING, S_STALLED,
        S_STALLED_SLEEP, S_WAITCNT, S_BARRIER };
    GfxVersion gfxVersion;
    const int wfSlotId;
    const int simdId;
    int kernId;
    std::deque<GPUDynInstPtr> instructionBuffer;
    // outstanding memory req tracking
    int outstandingReqs, outstandingReqsWrGm, outstandingReqsWrLm,
        outstandingReqsRdGm, outstandingReqsRdLm,
        scalarOutstandingReqsRdGm, scalarOutstandingReqsWrGm;
    // waitcnt counters
    int vmWaitCnt, expWaitCnt, lgkmCnt;
    // scratch flat Addr
    archFlatScratchAddr = 0;  // ← MI300+
    // LDS chunk (per-WG)
    LdsChunk *ldsChunk;
};
```

#### LDS (`src/gpu-compute/lds_state.hh:80-90`)

```cpp
class LdsState : public ClockedObject {
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, int32_t>> refCounter;
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, LdsChunk>> chunkMap;
    int bytesAllocated, maximumSize, banks, bankConflictPenalty;
    // LdsChunk = simple std::vector<uint8_t>
    // 支持 read<T>/write<T>/atomic<T>
    // bank conflict 计算在 countBankConflicts
};
```

LDS **按 WG 切片**: `reserveSpace(dispId, wgId, size)` 分配一个 `LdsChunk`。

#### SQC (`src/mem/ruby/protocol/GPU_VIPER-SQC.sm` 摘要)

```slicc
machine(MachineType:SQC, "GPU SQC (L1 I Cache)")
  : Sequencer* sequencer;
    CacheMemory * L1cache;
    int TCC_select_num_bits;
    Cycles issue_latency := 80;
    Cycles l2_hit_latency := 18;
// 状态: I / V (只读 L1 ICache，多 CU 共享 --cu-per-sqc=4)
// TCC 请求路由: out_msg.Destination.add(mapAddressToRange(address, MachineType:TCC, ...))
```

#### TCP (`src/mem/ruby/protocol/GPU_VIPER-TCP.sm` 摘要)

```slicc
machine(MachineType:TCP, "GPU TCP (L1 Data Cache)")
  : VIPERCoalescer* coalescer;
    Sequencer* sequencer;
    CacheMemory * L1cache;
    bool WB; bool disableL1;
    int TCC_select_num_bits;
    Cycles issue_latency := 40;
    Cycles l2_hit_latency := 18;
// 状态: I/V/A/F
// GLC/SLC 控制 bypass
// 写策略: write-through to TCC (wt_writeThrough action)
```

#### TCC (`src/mem/ruby/protocol/GPU_VIPER-TCC.sm` 摘要)

```slicc
machine(MachineType:TCC, "TCC Cache")
  : CacheMemory * L2cache;
    bool WB;
    Cycles l2_request_latency := 50;
    Cycles l2_response_latency := 20;
// 状态: M/W/V/I/IV/WI/WIB/A
// APU 上 tcc.WB = False（默认），写策略 write-through → directory → DDR
// dGPU 上 tcc.WB = False 也是默认
// 接 TCP/SQC 请求 + NB (Northbridge=directory) probe
```

**关键洞察**: TCC 收到 TCP 上行请求时，向下请求 `requestToNB_out`：
```slicc
action(rd_requestData, "r", desc="Miss in L2, pass on") {
    peek(coreRequestNetwork_in, CPURequestMsg) {
        enqueue(requestToNB_out, CPURequestMsg, l2_request_latency) {
            out_msg.addr := address;
            out_msg.Destination.add(mapAddressToMachine(address, MachineType:Directory));
            out_msg.CURequestor := in_msg.Requestor;  // ← 转发发起方以支持并发
```

TCC 接 NB probe（来自 directory 的 invalidate/downgrade）：
```slicc
in_port(probeNetwork_in, NBProbeRequestMsg, probeFromNB) {
    if (in_msg.Type == ProbeRequestType:PrbInv) {
        trigger(Event:PrbInv, in_msg.addr, cache_entry, tbe);
    } else {
        trigger(Event:PrbDowngrade, in_msg.addr, cache_entry, tbe);
    }
}
```

### 2.4 CPU-GPU 一致性（VIPER + MOESI_AMD_Base + TCC）

#### 协议构成（`src/mem/ruby/protocol/GPU_VIPER.slicc`）

```slicc
protocol "GPU_VIPER";
include "MOESI_AMD_Base-msg.sm";
include "MOESI_AMD_Base-dir.sm";
include "MOESI_AMD_Base-dma.sm";
include "MOESI_AMD_Base-CorePair.sm";
include "GPU_VIPER-msg.sm";
include "GPU_VIPER-TCP.sm";
include "GPU_VIPER-SQC.sm";
include "GPU_VIPER-TCC.sm";
include "MOESI_AMD_Base-L3cache.sm";  // unused in GPU_VIPER; TCC 取代 L3
```

**关键观察**:
- `dir.sm` 和 `msg.sm` **继承自 MOESI_AMD_Base** — CPU 和 GPU 共享同一消息协议与目录控制器实现
- `L3cache.sm` 在 GPU_VIPER 注释为 `unused`，**TCC 取代 L3** 在 GPU 一侧
- 主机一致性由 `MOESI_AMD_Base-dir` 提供；GPU 一侧仅 TCP/SQC/TCC 是 GPU 专属状态机

#### TCC 作为"GPU 的 L3"桥接 host DDR

在 APU 上，TCC 是 GPU 子系统和 CPU MOESI_AMD_Base 目录之间的"网关"——CU 写到 TCC 后立即 wb 到 DDR，CPU 这边目录保有这条线的 cache state；CPU 修改通过 probe 推到 TCC。

#### Acquire/Release 语义（`apu_se.py`）

```python
if buildEnv["PROTOCOL"] == "GPU_VIPER":
    shader.impl_kern_launch_acq = True   # ← 启动时 acquire
    shader.impl_kern_end_rel = False     # ← write-through，末尾不需要 release
else:
    shader.impl_kern_launch_acq = True
    shader.impl_kern_end_rel = True
```

#### Mtype 缓存一致性模式（`apu_se.py:170-180`）

```
--111 C_RW_S (Cached-ReadWrite-Shared)  -- APU 默认
--01x UC_L2 (Uncached_GL2)              -- dGPU 默认 (m-type=6)
--00x UC_All (Uncached_All_Load)
```

`apu_se.py:544-549`:
```python
if args.dgpu:
    args.m_type = 6  # ← dGPU: write-back GL2 + 系统级 directory coherence
```

### 2.5 HSA Runtime + Dispatcher

#### 数据通路总览

```
[Host CPU User Process]
 └── roc runtime → doorbell write (MMIO BAR2)
   ↓ [PCIe BAR2 / IOBus] ──→ AMDGPUDevice.readDoorbell
   ↓ 路由到对应 CP/SDMA/PM4
   [PM4PacketProcessor]  ← graphics pipe
   [HSAPacketProcessor]  ← HSA queue doorbell
   ↓ 拉 AQL packets
   [HSAPacketProcessor.AQLRingBuffer] ──→ (DMA host memory via dma)
   ↓
   [GPUCommandProcessor]  ← 处理 dispatch/agent/vendor packets
   ├── MQD DMA (host_amd_queue_addr → task->amdQueue)
   ├── scratch 检查
   └── dispatchPkt → dispatcher
   [GPUDispatcher]
   └── hsaQueueEntries[disp_id] = task
   └── exec() → ComputeUnit.dispWorkgroup(task, num_wfs)
   [ComputeUnit.dispWorkgroup]
   └── for each WF: startWavefront → fillKernelState → fetch
   [Wavefront.exec] in FetchStage → ExecStage loop
```

#### AQL 包结构（`src/dev/hsa/hsa_packet.hh:28-72`）— 所有包 64 字节

```cpp
struct _hsa_dispatch_packet_t {
    uint16_t header;
    uint16_t setup;
    uint16_t workgroup_size_x, workgroup_size_y, workgroup_size_z;
    uint32_t grid_size_x, grid_size_y, grid_size_z;
    uint32_t private_segment_size;
    uint32_t group_segment_size;  // ← LDS size per WG
    uint64_t kernel_object;       // ← device code addr
    uint64_t kernarg_address;     // ← kernel args addr
    uint64_t completion_signal;
};

struct _hsa_barrier_and_packet_t {
    uint16_t header;
    uint64_t dep_signal[5];       // ← HSA 限制 ≤5 dep signals per barrier
    uint64_t completion_signal;
};
```

#### HSAQueueDescriptor（`src/dev/hsa/hsa_packet_processor.hh:73-130`）

```cpp
class HSAQueueDescriptor {
    uint64_t basePointer;        // AQL ring buffer base
    uint64_t doorbellPointer;    // doorbell MMIO addr
    uint64_t writeIndex;
    uint64_t readIndex;
    uint32_t numElts;
    uint64_t hostReadIndexPtr;   // host 端 read pointer (DMA 读取)
    GfxVersion gfxVersion;
    uint64_t ptr(uint64_t ix) {
        return basePointer + ((ix % numElts) * objSize());
    }
};
```

#### GPUCommandProcessor 核心流程（`src/dev/hsa/gpu_command_processor.hh:218-247`）

```cpp
class GPUCommandProcessor : public DmaVirtDevice {
    void dispatchPkt(HSAQueueEntry *task);  // 主入口
    void MQDDmaEvent(HSAQueueEntry *task) {  // MQD 读回调
        if ((task->privMemPerItem() * 64) >
            task->amdQueue.compute_tmpring_size_wavesize * 1024) {
            updateHsaSignal(queue_inactive_signal, 1, ...);  // 要求 runtime 扩 scratch
        } else {
            dispatchPkt(task);
        }
    }
    void dispatchPkt(HSAQueueEntry *task) {
        dispatcher->dispatch(task);
    }
};
```

**scratch lazy allocation** 是关键：dGPU 在 ROCm 2.2+ 上 kernel 启动时若 scratch 不够，会主动 poll `queue_inactive_signal` 等 runtime 扩容后再启动。

#### Kernel 启动路径（11 步）

```
1. CPU roc runtime 写 doorbell (PIO)
   → AMDGPUDevice.writeDoorbell → queue_inactive_signal / PM4
2. HSAPacketProcessor.write(Pkt)
   → schedules QueueProcessEvent
3. QueueProcessEvent.process → getCommandsFromHost (DMA AQL packets to _aqlBuf)
   → processPkt → submitDispatchPkt
4. submitDispatchPkt → GPUCommandProcessor.submitDispatchPkt
5. GPUCommandProcessor.performTimingRead (DMA MQD via ReadDispIdOffsetDmaEvent)
   → MQDDmaEvent → dispatchPkt
6. dispatcher.dispatch(task)
   → hsaQueueEntries[disp_id] = task; execIds.push(disp_id); scheduleDispatch
7. dispatcher.exec → for each CU with hasDispResources:
   → ComputeUnit.dispWorkgroup(task, num_wfs_in_wg)
8. ComputeUnit.dispWorkgroup → for each WF: startWavefront(wf, waveId, ldsChunk, task, bar_id)
9. startWavefront → fillKernelState (init SGPR/VGPR from AQL + MQD)
   → reserveSpace for LDS → wf.start(...)
10. Wavefront.exec (in pipeline loop)
    → FetchStage.fetch → SQC miss → TCC → directory → DDR
    → ScheduleStage/ExecStage
    → globalMemoryPipe.issueRequest → DTLB → TCP → TCC → ...
11. WF 完成 → dispatcher.notifyWgCompl
12. 所有 WG 完成 → dispatcher.exec marks dispatchComplete
13. CP.sendCompletionSignal → 更新 host 完成信号 (DMA)
```

### 2.6 PCIe / Disjoint Network（dGPU）

#### Disjoint Network 的本质

`x86-mi300x-gpu.py` 创建的是**两个独立 RubySystem**:

```
[CPU Side RubySystem] ────── (ViperCPUCacheHierarchy) ──────
 └── L1/L2/L3 + MOESI_AMD_Base dir ←→ Host DDR (DDR4)

[GPU Side RubySystem] ────── (ViperGPUCacheHierarchy) ──────
 └── TCP/SQC/TCC + GPU_VIPER dir ←→ HBM2
 ↑ 两边仅通过 PCI 桥 (device.upstream = board.get_pci_host()) 连
```

`src/python/gem5/prebuilt/viper/gpu_cache_hierarchy.py:31-45` 注释确认:
> Make an independent RubySystem class containing only the GPU caches with no network connection to the CPU cache hierarchy.

#### GPU 网络拓扑（`viper_network.py`）

```python
l2_xbar_types = ("GPU_VIPER_TCP_Controller", "GPU_VIPER_SQC_Controller", "GPU_VIPER_TCC_Controller")
soc_xbar_types = ("GPU_VIPER_DMA_Controller", "GPU_VIPER_Directory_Controller")
# 两个交叉开关：L2 xbar (TCP/SQC/TCC) + SoC xbar (DMA/Directory)
# 两个 xbar 双向互联
```

物理拓扑：
```
TCP/SQC/TCC (per CU) ⇄ L2_CROSSBAR ⇄ SOC_CROSSBAR ⇄ DMA/Directory/HBM2
```

#### CPU↔GPU 桥：`ViperShader.get_cpu_dma_ports`

`viper_shader.py` 把以下 DMA port 接到 host 的 `iobus`/`pci_bus`:
- `gpu_cmd_proc.hsapp.dma`
- `gpu_cmd_proc.dma`
- `system_hub.dma`
- `device.device_ih.dma`
- `device.dma` ← 主 PCIe DMA
- 每个 PM4 的 `pm4_proc.dma`
- 每个 SDMA 的 `sdma.dma`

---

## 3. 关键架构决策（D1-D5）

### D1 协议策略

| 选项 | 工作量 | 灵活性 | 评价 |
|------|--------|--------|------|
| (A) **简化状态机**: I→S→M→I 三态 + write-through 直写（GPU 不缓存） | Low | 低 | ✅ Phase 0-1 |
| (B) **完整 MOESI + GPU_VIPER 扩展**: I/S/E/M/O + TCP/TCC/SQC 子状态 | High | 高 | ✅ Phase 2+ |

**采纳**: 分步走。
- (A) 在 Phase 0-1：GPU 请求走 `write-through`（CacheTLM 不用变），验证 GPU 请求能走到 Memory。
- (B) 在 Phase 2 才把 CacheTLM 改为 protocol-aware。

**不复制 gem5 的 GPU_VIPER slicc 文件**——slicc 是 gem5 特有的 DSL，5000+ 行不可读。CppTLM 用 C++ `switch` 表驱动状态转换（`uint8_t state_transition[6][6]` 表，6 状态 × 6 事件）。

### D2 CU 粒度

| 选项 | 工作量 | 评价 |
|------|--------|------|
| **黑盒**: CU 就是 `tick()` 循环，按参数化 `issue_per_cycle` 发请求 | Low | ✅ 推荐 |
| 5-stage pipeline（Fetch→Scoreboard→Schedule→Exec→Mem）+ 精确 wavefront 调度 | Very High | ❌ 推迟 |

**采纳**: **黑盒优先**。CU 在 Phase 1 就是一个 `tick()` 循环，按参数化的 `kernel_issue_interval_` 发出 `ComputeReqBundle`。不做 SIMD/wavefront/LDS model。

### D3 Wavefront / Coalescing

| 选项 | 工作量 | 评价 |
|------|--------|------|
| **抽象**: 用 `coalescing_factor` 参数（如 4：4 个独立请求在 CU 层面合并为 1 个大请求） | Low | ✅ 推荐 |
| 精确模拟 wavefront 内 memory coalescing（同一 warp 的 64 个线程的访存合并） | Very High | ❌ 不做 |

**采纳**: **抽象掉**。CppTLM 作为 TLM 框架，关注 NoC/cache/memory 层级的行为，不是 GPU ISA 行为。

### D4 HSAPP / CP / Dispatcher

| 选项 | 工作量 | 评价 |
|------|--------|------|
| **简化**: 一个 `KernelLaunchTLM` 模块（~150 行） | Low | ✅ 推荐 |
| 完整 HSAPPacketProcessor + GPUCommandProcessor + GPUDispatcher + GPUComputeDriver | Very High (3000+ 行) | ❌ 推迟 |

**采纳**: **简化到极致**。在 Phase 1-4，用一个 `KernelLaunchTLM` 模块代替：
```json
{
  "name": "kernel_launcher",
  "type": "KernelLaunchTLM",
  "params": {
    "num_workgroups": 8,
    "workgroup_size": 64,
    "kernel_duration": 100
  }
}
```

`KernelLaunchTLM` 在 `tick()` 中定期向 `ComputeUnitTLM` 发 kernel launch 命令。不需要 PIO、doorbell、AQL 解析。

### D5 目录结构

| 选项 | 工作量 | 评价 |
|------|--------|------|
| **新建 `include/tlm/gpu/` 子目录** | Low | ✅ 推荐 |
| 扩展现有 `include/tlm/` | Low | ⚠️ 会污染通用 TLM 目录 |

**采纳**:
```
include/tlm/
├── cache_tlm.hh              (扩展现有 — 支持 coherence protocol)
├── gpu/
│   ├── compute_unit_tlm.hh   (ComputeUnitTLM)
│   ├── tcc_tlm.hh            (TCC_TLM — GPU last-level cache)
│   ├── kernel_launch_tlm.hh  (KernelLaunchTLM)
│   └── pcie_bridge_tlm.hh    (Phase 2 dGPU)
├── bundles/
│   └── compute_bundles_tlm.hh (ComputeReqBundle/ComputeRespBundle)
```

`include/tlm/gpu/` 子目录清晰隔离 GPU 特定代码，避免污染通用 TLM 目录。

---

## 4. 6 阶段执行路径（APU-first）

### 🟢 Phase 0 — GPU 基础设施（2-3 周，Medium）

**Scope**:
- 新建 `include/bundles/compute_bundles_tlm.hh`：`ComputeReqBundle` / `ComputeRespBundle`（在 CacheReqBundle 基础上加 `kernel_id`, `workgroup_id`, `wavefront_id`, `global_mem_addr`, `local_mem_addr` 字段，~50 行）
- 新建 `include/tlm/gpu/gpu_tlm.hh`：`GPUTLM` v0（ChStreamModuleBase 派生，tick() 中发读取请求验证 Bundle 通路）
- 注册 `REGISTER_CHSTREAM(GPUTLM, ComputeReqBundle, ComputeRespBundle)` + ChStreamAdapterFactory 注册
- `configs/gpu_standalone.json`：1× GPUTLM → 1× MemoryTLM（先不经过 CacheTLM，验证 Bundle 路径）

**为什么先做**: 验证 GPU Bundle 类型能在 StreamAdapter 管线中流通。这是所有后续工作的前提。

**Acceptance criteria**:
```bash
./build/bin/cpptlm_tests "[gpu]"  # 新测试: GPUTLM standalone send/receive
./cpptlm --config configs/gpu_standalone.json  # 可执行，0 错误
```

**gem5 参考**：`src/gpu-compute/ComputeUnit.py`（仅接口，不读 pipeline 实现细节）

---

### 🟡 Phase 1 — ComputeUnit 黑盒（3-4 周，Medium-High）

**Scope**:
- `ComputeUnitTLM`：从 ChStreamModuleBase 派生，核心 loop：
  - tick() 中按 `kernel_issue_interval_` 发出 `ComputeReqBundle`（read/write）
  - 维护 `inflight_kernel_reqs_` map（类似 CPUTLM 的 `inflight_txns_`，但有 `workgroup_id` 维度）
  - 跟踪 `workgroup_progress_`（多少 work-item 已完成）
- **不建模**：ISA、SIMD pipeline、wavefront scheduling、register file、LDS
- CrossbarTLM 扩展：支持 GPU 类请求的地址路由（高位地址映射到 GPU memory region）
- CacheTLM 扩展（可选 Phase 1）：在 CacheReqBundle 中识别 `is_gpu_request = true` → 绕过 CPU 侧 cache 一致性（GPU 请求走 bypass 路径）

**为什么先做**: 这是"CPU+GPGPU SoC"的 GPU 核心。黑盒 CU 能在 Phase 1 就交付端到端演示 — CPU module (TrafficGenTLM) 发请求 → CacheTLM → CrossbarTLM，同时 GPUTLM 发 kernel 请求 → CrossbarTLM → MemoryTLM，验证总线共享。

**Acceptance criteria**:
```json
// configs/apu_demo_v1.json: 2 TrafficGenTLM + 1 ComputeUnitTLM + 1 CrossbarTLM + 1 MemoryTLM
// 验证：两个源都能访问 MemoryTLM，地址不冲突
```
```bash
./build/bin/cpptlm_tests "[gpu][phase6]"  # ComputeUnitTLM 黑盒测试 + Phase 6 集成
# 统计输出显示 kernel 请求完成计数
```

**gem5 参考**：`src/gpu-compute/ComputeUnit.py`（1000+ 行，只读构造参数和接口，不读 pipeline 逻辑）

---

### 🔴 Phase 2 — Coherence Protocol 集成（4-5 周，High — **最高风险**）

**Scope**（核心难点）:
- **CacheTLM protocol-aware 改造**（这步是最高风险项）:
  - 当前 `CacheTLM` 只有 `std::map<uint64_t, uint64_t> cache_lines_`（地址→数据）
  - 扩展为 `std::map<uint64_t, CacheLine>` 其中 `CacheLine = {data, CoherenceState, sharers_bitmask}`
  - tick() 中加入 coherence state machine：收到 req → lookup → state transition → 决定 hit/miss/snoop
  - 实现 `get_snoop_targets()` 和 `handle_snoop()` 接口
- **CoherenceDomain 与 CacheTLM 集成**: `CoherenceDomain` 目前独立于 `CacheTLM`，需要 bridge
  - 在 ModuleFactory 中：创建 CacheTLM 时，查找所属 CoherenceDomain → 注册 snoop callback
  - CacheTLM::tick() 中：miss 时调用 `domain->get_snoop_targets()` → 发 snoop 请求给同域其他 CacheTLM
  - CoherenceDomain 的 `lookup_home_node()` → CacheTLM 的 directory 查询
- **GPU_VIPER 简化协议**：不需要完整 slicc 代码。CppTLM 用状态枚举 + `switch` 表
  - 状态机：`I → S`(read), `I → M`(write), `S → M`(upgrade), `M → I`(invalidation), `S → I`(eviction)
  - GPU 特有：`I_TCP` (TCP miss, pending to TCC), `S_TCP` (TCP hit, stale 可能)
  - 用 `uint8_t state_transition[6][6]` 表驱动（6 个状态 × 6 个事件）

**为什么这个阶段做**: Phase 1 的 CU 黑盒可以 bypass 一致性（直写 memory）。Phase 2 才真正把 coherence 串联起来 — 这是 APU 的核心区别（CPU+GPU 共享一致性域）。也是 CppTLM 从"玩具 cache"走向"协议级 cache"的质变。

**Acceptance criteria**:
```json
// configs/apu_demo_v2.json:
// cpu_domain { TrafficGenTLM + CacheTLM + CacheTLM } + gpu_domain { ComputeUnitTLM + CacheTLM }
// 跨域通过 ProtocolBridge
// 验证：CPU write → GPU snoop → GPU Cache invalidate → GPU read miss → 从 memory 重读
```
```bash
./build/bin/cpptlm_tests "[coherence][gpu]"  # MESI state transitions
```

**gem5 参考**（仅参考 message 类型）:
- `src/mem/ruby/protocol/GPU_VIPER.slicc`（理解协议构成）
- `src/mem/ruby/protocol/MOESI_AMD_Base-{msg,dir,dma}.sm`（消息格式）
- **不要**: 复制 `.sm` 文件内容；用 C++ 重写简化状态机

**触发升级条件**: 如果 Phase 2 测试发现 CacheTLM 改造导致 5+ 测试回归，暂停 coherence 方案，回到 Phase 1 的 write-through bypass 策略。

---

### 🟡 Phase 3 — TCC Bridge + 内存层次（3-4 周，Medium）

**Scope**:
- **TCC（Last-Level GPU Cache）模块**: `TCC_TLM` 继承 ChStreamModuleBase
  - 位于 `ComputeUnitTLM ↔ MemoryTLM` 路径中间
  - 功能: aggregate GPU 请求，写合并（write coalescing），coherent probe fan-in
  - 使用 `DualPortStreamAdapter`（TCP 侧 ↔ Directory/Memory 侧）— **这是 DualPortAdapter 的完美应用场景**
  - TCC 从 ComputeUnit 收 write-through，批量发往 MemoryTLM；从 CoherenceDomain 收 probe，转发给 ComputeUnit
- **GPU memory controller**: `MemoryTLM` 扩展支持 `hbm_mode` 参数（高带宽、高延迟）

**为什么这个阶段做**: TCC 是 APU 架构中 CPU↔GPU coherence 的枢纽。gem5 的 TCC 是 TCP/SQC 和目录之间的唯一桥梁。Phase 2 有了 CacheTLM coherence，Phase 3 才需要 TCC 做 aggregation。

**Acceptance criteria**:
```json
// configs/apu_demo_v3.json: ComputeUnitTLM → TCC_TLM → MemoryTLM (HBM)
// 验证: write coalescing (同一 address 的连续 write 在 TCC 合并)
```
```bash
./build/bin/cpptlm_tests "[gpu][tcc]"  # TCC write coalescing 测试
```

**gem5 参考**: `src/mem/ruby/protocol/GPU_VIPER-TCC-{L1,L2}.sm`（只读 TCC state machine 消息类型和 probe 转发逻辑）

---

### 🟢 Phase 4 — Multi-CU + NoC Integration（2-3 周，Medium）

**Scope**:
- `ComputeUnitTLM` 扩展为数组模式：JSON 参数 `num_cus=4` 创建多个 CU 实例
- 内部用 `BidirectionalPortAdapter<N>` 连接 CU ↔ GPU Crossbar
- `GPUNoC`（复用 `tlm::RouterTLM` + `tlm::LinkTLM`）+ NoCFlitBundle（已存在）
- 验证：4 CU 同时发 kernel 请求，通过 GPU NoC 到 TCC→Memory

**为什么这个阶段做**: 前面的 Phase 都是单 CU。多 CU + NoC 是"真实 GPU"的门槛。因为有现成的 `RouterTLM`/`LinkTLM`/`NoCFlitBundle`，这个 Phase 主要是 JSON 配置工作。

**Acceptance criteria**:
```json
// configs/apu_demo_v4.json: 4× ComputeUnit + 1× GPUCrossbar + 1× TCC + 1× Memory
// 验证: 4 CU 并行 kernel 请求，NoC 路由正确
```

**gem5 参考**: `configs/example/apu_se.py` 的 Shader/CU 创建循环（`CUs = [ComputeUnit(...) for _ in range(num_cu)]`）

---

### 🟢 Phase 5 — Full APU SoC Demo（2-3 周，Low-Medium）

**Scope**:
- JSON 配置文件 `configs/apu_full_soc.json`：组装所有组件
- CPU cluster: `2× TrafficGenTLM → 2× CacheTLM → CPU Crossbar`
- GPU cluster: `4× ComputeUnitTLM → GPU Crossbar → TCC_TLM → MemoryTLM`
- CPU↔GPU coherence: `CoherenceDomain("apu_domain", Protocol::MOESI)` 包含所有 CacheTLM + TCC
- Domain boundary: `ProtocolBridge` (DualPortAdapter) 处理 CPU↔GPU 跨域流量
- Python 完整测试套件: `test/python/test_apu_soc.py` 端到端验证
- 统计收集: GPU utilization, kernel completion time, cache hit/miss, NoC latency

**gem5 参考**:
- `configs/example/apu_se.py` 完整的 Shader/CU/HSAPP 设置
- `configs/example/gem5_library/x86-mi300x-gpu.py` 的 ViperBoard 设置

---

### 🟡 Phase 2 备选 — dGPU: Disjoint Coherence + PCIe Bridge（4-6 周，High）

如果 APU 完成后需要 dGPU 扩展:

**Scope**:
- 两个独立的 `CoherenceDomain`: `cpu_domain` (MOESI) + `gpu_domain` (GPU_VIPER)
- `PCIBridge_TLM`: 基于 `DualPortStreamAdapter` 实现, latency=100-500 cycles
- GPU 独立 memory controller: `HBM_TLM`（比 MemoryTLM 更高带宽，bandwidth 参数化）
- 两组独立拓扑: CPU cluster + GPU cluster，物理上分离
- JSON config `configs/dgpu_demo.json`：CPU → [PCIe] → GPU

**gem5 参考**: `configs/example/gpufs/Disjoint_VIPER.py`（11 个 virtual network + 分离的 CPU/GPU 控制器）+ `src/dev/AMDGPUDevice.py`（PCIe device）

---

## 5. 最高风险与缓解

### 最高风险: Coherence Protocol 集成（Phase 2）

**具体风险**: 当前 `CacheTLM` 是 protocol-unaware（只有 address→data 映射，没有 state machine）。改造为 protocol-aware 需要：

1. 扩展 `CacheLine` 结构，加入 coherence state + sharers bitmask
2. 实现状态转换表（至少 6×6=36 种转换）
3. 引入 snoop protocol：miss→snoop→respond→transition 多周期交易
4. 处理 deadlock/livelock（snoop filter, transient state）

**缓解**:
- 不直接复用 gem5 的 slicc 自动机。用 C++ `switch` 表驱动
- CacheTLM 的 coherence 改造分两小步：先加 `CoherenceState` 标签（但不做 snoop），再加 snoop callback
- 充分测试：`[coherence]` 标签下每个状态转换独立单元测试

**触发升级条件**: 如果 Phase 2 测试发现 CacheTLM 改造导致 5+ 测试回归，暂停 coherence 方案，回到 Phase 1 的 write-through bypass 策略。

### 可 defer 的内容（不影响 MVP）

| 内容 | defer 到 | 原因 |
|------|----------|------|
| 完整 wavefront 调度 | Phase 4+ | 黑盒 CU 足以验证 NoC/cache/memory |
| LDS (local data share) 建模 | Phase 5+ | LDS 是 CU 内部行为，不涉及 NoC |
| PCIe bridge (dGPU) | Phase 2 (分离项目) | APU 不需要 PCIe |
| HSA runtime / ROCm KFD | Post MVP | CppTLM 是 TLM 仿真，不是 ISA 仿真 |
| Full GPU_VIPER slicc state machine | 永远不做 | CppTLM 用简化状态表，不做 slicc 代码生成 |
| DMA engine | Post MVP | CPU-GPU 数据搬移用 MemoryTLM 模拟 |

---

## 6. MVP 定义（Phase 1+2 完成后即可演示）

```json
{
  "modules": [
    {"name": "cpu0", "type": "TrafficGenTLM"},
    {"name": "cpu1", "type": "TrafficGenTLM"},
    {"name": "cache_cpu0", "type": "CacheTLM"},
    {"name": "cache_cpu1", "type": "CacheTLM"},
    {"name": "cu0", "type": "ComputeUnitTLM", "params": {"num_workgroups": 4, "wg_size": 64}},
    {"name": "tcc", "type": "TCC_TLM"},
    {"name": "xbar", "type": "CrossbarTLM"},
    {"name": "mem", "type": "MemoryTLM", "params": {"mode": "hbm"}}
  ],
  "connections": [
    {"src": "cpu0", "dst": "cache_cpu0", "latency": 1},
    {"src": "cpu1", "dst": "cache_cpu1", "latency": 1},
    {"src": "cache_cpu0", "dst": "xbar", "latency": 2},
    {"src": "cache_cpu1", "dst": "xbar", "latency": 2},
    {"src": "cu0", "dst": "tcc", "latency": 1},
    {"src": "tcc", "dst": "xbar", "latency": 2},
    {"src": "xbar", "dst": "mem", "latency": 50}
  ],
  "coherence_domains": [
    {"name": "apu_domain", "protocol": "MOESI", "members": ["cache_cpu0", "cache_cpu1", "tcc"]}
  ]
}
```

**验证流**: CPU 写 addr A → cache 失效 → TCC snoop → 跨 cache 一致性 → GPU 重读正确数据。

---

## 7. Anti-Recommendations ⚠️

### ❌ 不要做的事

1. **不要尝试通过 gem5 slicc 代码理解 coherence 协议**
   - gem5 的 GPU_VIPER 协议分布在 15+ `.sm` 文件中，总计 5000+ 行 slicc 代码
   - slicc 是 gem5 特有的 DSL，既不通用也不可读
   - 正确做法：读 `MOESI_AMD_Base-msg.sm` 理解消息类型，然后用 C++ 重写简化状态机
   - Phase 2 需要的只是一张 `state_transition[6][6]` 表，不是 slicc

2. **不要建模 GPU ISA（GCN/RDNA/CDNA 指令集）**
   - CppTLM 是 Transaction-Level Modeling 框架，不是架构模拟器
   - ISA 建模需要完整解码器、寄存器文件、执行单元 — 这是 gem5/GPGPU-Sim 的领域
   - CU 作为 "blackbox request issuer" 就足够了

3. **不要复制 gem5 的 Shader/ComputeUnit 类层次**
   - gem5 的 `Shader.py` → `ComputeUnit.py` → `SIMD.py` → `Wavefront.py` 层次深达 5 级
   - 这是 gem5 的 Python 配置层特性，不是 CppTLM 需要的
   - CppTLM 只需要：`ComputeUnitTLM`（内部有 tick loop 发请求）和 `KernelLaunchTLM`（驱动 CU），总共 2 个类

4. **不要在 Phase 1 尝试完整 HSA 运行时**
   - `HSAPacketProcessor` + `GPUCommandProcessor` + `GPUDispatcher` + `GPUComputeDriver` = 3000+ 行
   - HSA runtime 包括：AQL 包解析、doorbell PIO、queue management、signal tracking、VM fault handling
   - Phase 1-4 用 `KernelLaunchTLM` 代替（~150 行）

5. **不要同时做 APU 和 dGPU**
   - 两者架构差异大（APU=share all, dGPU=separate all）
   - 共享的只有 `ComputeUnitTLM` 和 `ComputeReqBundle`
   - APU 完成后评估 ROI 再决定是否做 dGPU

6. **不要引入新的外部依赖**
   - sleigh/llvm 解码器？不需要（不建模 ISA）
   - ROCm runtime？不需要（不建模 HSA）
   - GPU 仿真库 (gpgpu-sim)？不需要（CppTLM 是 TLM 框架）
   - **当前零外部依赖的编译模型不能打破**

### ✅ 值得做的（但不属于核心路径）

- `scripts/gpu_kernel_trace_gen.py`：生成 GPU 访存 trace，供 TrafficGenTLM 回放（替代完整的 HSA runtime）
- `configs/benchmark/`：固定的 benchmark 拓扑（类似 gem5 的 `x86-mi300x-gpu.py`），便于回归测试
- 统计 dashboard 扩展：GPU utilization（CU 忙碌 vs 等待）、kernel turnaround time、TCC coalescing ratio

---

## 8. 关键 permalink 索引

| 主题 | URL |
|------|-----|
| APU 配置 | https://github.com/gem5/gem5/blob/develop/configs/example/apu_se.py |
| dGPU (MI300X) 配置 | https://github.com/gem5/gem5/blob/develop/configs/example/gem5_library/x86-mi300x-gpu.py |
| MI300X/MI210/MI355X Python 类 | https://github.com/gem5/gem5/blob/develop/src/python/gem5/components/devices/gpus/amdgpu.py |
| ViperShader (Python) | https://github.com/gem5/gem5/blob/develop/src/python/gem5/components/devices/gpus/viper_shader.py |
| GPU cache hierarchy (Python) | https://github.com/gem5/gem5/blob/develop/src/python/gem5/prebuilt/viper/gpu_cache_hierarchy.py |
| Viper network | https://github.com/gem5/gem5/blob/develop/src/python/gem5/prebuilt/viper/viper_network.py |
| Shader (C++) | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/shader.hh |
| ComputeUnit (C++) | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/compute_unit.hh |
| Wavefront (C++) | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/wavefront.hh |
| LDS (C++) | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/lds_state.hh |
| Dispatcher (C++) | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/dispatcher.hh |
| HSAQueueEntry (C++) | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/hsa_queue_entry.hh |
| HSA Packet Processor | https://github.com/gem5/gem5/blob/develop/src/dev/hsa/hsa_packet_processor.hh |
| GPU Command Processor | https://github.com/gem5/gem5/blob/develop/src/dev/hsa/gpu_command_processor.hh |
| HSA Packet 定义 | https://github.com/gem5/gem5/blob/develop/src/dev/hsa/hsa_packet.hh |
| AMDGPU Device (PCIe) | https://github.com/gem5/gem5/blob/develop/src/dev/amdgpu/amdgpu_device.hh |
| GPU_VIPER 协议 (slicc) | https://github.com/gem5/gem5/blob/develop/src/mem/ruby/protocol/GPU_VIPER.slicc |
| MOESI_AMD_Base 协议 | https://github.com/gem5/gem5/blob/develop/src/mem/ruby/protocol/MOESI_AMD_Base.slicc |
| SQC 状态机 | https://github.com/gem5/gem5/blob/develop/src/mem/ruby/protocol/GPU_VIPER-SQC.sm |
| TCP 状态机 | https://github.com/gem5/gem5/blob/develop/src/mem/ruby/protocol/GPU_VIPER-TCP.sm |
| TCC 状态机 | https://github.com/gem5/gem5/blob/develop/src/mem/ruby/protocol/GPU_VIPER-TCC.sm |
| Pipeline stage 接口 | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/comm.hh |
| Global memory pipeline | https://github.com/gem5/gem5/blob/develop/src/gpu-compute/global_memory_pipeline.hh |

---

## 9. 相关 CppTLM 内部资源

| 主题 | 路径 |
|------|------|
| CoherenceDomain (MESI/MOESI enum + snoop fanout) | `include/core/coherence_domain.hh` |
| MOESI 状态枚举 | `include/ext/error_context_ext.hh` |
| Domain boundary validation | `src/core/module_factory_validate.cc:445` |
| DualPortStreamAdapter（GPU MemoryBridge 模板） | `include/framework/dual_port_stream_adapter.hh` |
| BidirectionalPortAdapter（CU 内部互联模板） | `include/framework/bidirectional_port_adapter.hh` |
| MultiPortStreamAdapter（GPU memory fan-out） | `include/framework/multi_port_stream_adapter.hh` |
| CacheReqBundle/RespBundle（GPU 字段扩展基础） | `include/bundles/cache_bundles_tlm.hh` |
| NoCFlitBundle（GPU NoC flit 基础） | `include/bundles/noc_bundles_tlm.hh` |
| REGISTER_CHSTREAM_DUAL（GPU 双端口注册宏） | `include/chstream_register.hh` |
| Roadmap（待同步） | `roadmap.md` |
| 文档同步检查 | `scripts/test/docs_sync_check.sh` |
| 文档审计报告 | `docs/docs_audit_report.md` |
| gem5 SoC 调研报告（前序工作） | `docs/research-gem5-soc-survey.md` |

---

**报告状态**: ✅ 完成
**下一步**: 用户决策（a + b 已执行；Phase 0 brainstorm 设计等用户触发）