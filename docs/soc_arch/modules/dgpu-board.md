# dgpu-board 微架构文档

> **类别**: GPU > dGPU Board · **状态**: 🔵 MVP 切片 (per ADR-SOC-06)
> **Header**: `include/tlm/gpu/dgpu_board_mvp.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`, 新增)
> **蓝图来源**: gem5 `src/dev/amdgpu/amdgpu_device.py` + `src/dev/pci/pci_host.py` + UsrLinuxEmu ADR-090 v2 §D3.3
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5/D6
> **首版 commit**: � W3-4 实施 · **最近更新**: 2026-08-19
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - ADR: [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D1-D6
> - 组件契约: [`command-processor.md`](./command-processor.md) · [`pm4-decoder.md`](./pm4-decoder.md) · [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md) · [`cuda-core-adapter.md`](./cuda-core-adapter.md) · [`ptx-emu-submodule-mvp.md`](./ptx-emu-submodule-mvp.md)
> - UsrLinuxEmu IOCTL: [`UsrLinuxEmu/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md`](../../../external/UsrLinuxEmu/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) §D3.3

---

## 1. 设计目标

`DGpuBoardTLM` 是 **dGPU 板卡的 ChStreamModuleBase 包装**,在 MVP 阶段引入,负责将 8 个内部组件(DGpuBar + Doorbell + CommandProcessor + TmuDispatchProcessor + SubmitQueue + CudaCoreAdapter + PtxEmuSubmoduleMVP + CompletionRing)组装为一个 **JSON-driven 单芯片 dGPU 仿真目标**。

**核心特性**:
- 继承 `ChStreamModuleBase`,支持 JSON 拓扑驱动实例化(per `ModuleFactory::instantiateAll`)
- 内部 8 组件组合:DGpuBar(PCIe BAR0 MMIO)+ Doorbell + CommandProcessor + TmuDispatchProcessor + SubmitQueue + CudaCoreAdapter + PtxEmuSubmoduleMVP + CompletionRing
- 入口方法:`install_kernel_module(vram_addr, size)` + `submit_kernel(KernelLaunchRequest)` + `write_reg(offset, value)` 模拟 UsrLinuxEmu driver 角色
- **`tick()` 由 EventQueue 调度**,内部驱动 CP→TMU→SQ→CudaCore 4 阶段串联(per Phase F-H.2)
- **MVP 接 UsrLinuxEmu IOCTL 0x27/0x29/0x01 + 0x28 永久 -ENOSYS**(per Phase F-H.3)

**MVP vs v0.4 简化**:
- ✅ 保留:DGpuBar + Doorbell + SubmitQueue(WDU 分发网络)+ CQ(per ADR-X.15 §4.2 + Phase F-H.5)
- ✅ 保留:4 个已有 TLM 模块(`ScoreboardTLM`/`PipelineTLM`/`TensorCoreTLM`/`MinimalWarpSchedulerTLM`)集成(CudaCoreAdapter 注入)
- ❌ 裁剪:TmuDispatchProcessor 32 slot(MVP 简化,v0.5 完整版 256 slot)+ 反压停 fetch(非 LIFO)
- ❌ 裁剪:MMU ordering pipe 精确建模(仅延迟区间断言 250-700ns)
- ❌ 裁剪:MSI-X 中断 + DMA channel(推到 v0.5 完整版)

---

## 2. 架构概览

### 2.1 内部组件层次(per Phase F-H.2 + Phase I.1/I.2,8 组件)

```
DGpuBoardTLM (ChStreamModuleBase, JSON-driven)
├── DGpuBar (PCIe BAR0 MMIO + BAR1 VRAM)
│    ├── BAR0 0x0000-0x0FFF: device regs (vendor/device ID + 控制)
│    ├── BAR0 0x1000-0x1FFF: doorbell ring MMIO space (per subchannel)
│    └── BAR1: VRAM backing (256MB, mmap'd to host + pushbuffer_ring)
├── Doorbell (SQ tail register, strong-ordered)
│    ├── atomic<uint64_t> sq_tail_[MAX_STREAMS=1024]
│    └── ring(stream_id, tail) → notify CP consumer
├── CommandProcessor (CP, 5-state FSM)
│    ├── Pm4Decoder (NVIDIA method packet, 4 method_addr ranges)
│    ├── SubchannelContext[8] (subchannel 0-7,per Phase F-B.2 H2)
│    └── CommandDispatcher (method_addr → handler)
├── TmuDispatchProcessor (TMU Glue, 32 slot MVP)
│    ├── inflight_kernel_reqs_ map (32 slot + 反压停 fetch,per Phase F-D.2 H5)
│    ├── dep latch: wait_on ↔ arrive_at 匹配
│    └── submit / on_complete / try_chain_dependent
├── SubmitQueue (WDU 分发网络,per Phase F-H.5)
│    ├── per-cluster pending FIFO(32 槽)+ per-core active(4 槽)
│    └── enqueue(cta_descriptor) → tick() → dispatch_to_core
├── CudaCoreAdapter (★ SM 微架构探索器,per Phase I.2)
│    ├── tick() → sm->exe_once()(驱动 PTX-EMU 3-Step 注入)
│    ├── 4 timing 模块注入(ScoreboardTLM/PipelineTLM/TensorCoreTLM/MinimalWarpSchedulerTLM)
│    └── WarpState 镜像(cycle/exec_mask/blocked,不含 PC)
├── PtxEmuSubmoduleMVP (★ PTX functional facade,per Phase I.1)
│    ├── functional_execute_warp / decode_ptxir / create_gpu_context
│    └── 唯一 include PTX-EMU 头的 .cc(编译防火墙)
└── CompletionRing (host_notify)
     ├── push(task_id, status) → release mutex
     └── host_notify_() → HAL fence_signal
```

### 2.2 ChStream 端口

| 端口 | 类型 | 数量 | 角色 |
|------|------|:---:|------|
| `req_in_` | `InputStreamAdapter<ComputeReqBundle>` | 1 | 接收 host 请求(预留) |
| `resp_out_` | `OutputStreamAdapter<ComputeRespBundle>` | 1 | 返回响应(预留) |
| `vram_port_` | `MemoryTLM` ChStreamPort | 1 | VRAM backing(对接 `MemoryTLM` H2D DMA + L2 backing) |

### 2.3 JSON 配置示例(`configs/dgpu_board_v1_mvp.json`)

```json
{
  "name": "dgpu_board_v0.5-MVP",
  "modules": [
    {
      "name": "dgpu_board0",
      "type": "DGpuBoardTLM",
      "params": {
        "ptx_emu_root": "@PTX_EMU_ROOT@",
        "vram_size_mb": 256,
        "max_streams": 4,
        "sq_depth": 32,
        "tmu_max_active_tasks": 32,
        "tmu_max_active_tasks": 32,
        "tmu_enable_backpressure": true,
        "enable_dgpu_bar_mmio": true
      }
    },
    {
      "name": "vram",
      "type": "MemoryTLM",
      "params": {
        "size_mb": 256,
        "latency_rd_cyc": 100,
        "latency_wr_cyc": 120
      }
    },
    {
      "name": "usrlxemu_ioctl_stub",
      "type": "UsrLinuxEmuIoctlStub",
      "params": {
        "stub_mode": "load_kernel_module|launch_kernel_module|unload_kernel_module",
        "bind_dgpu": "dgpu_board0"
      }
    }
  ],
  "connections": [
    { "src": "dgpu_board0.vram_port", "dst": "vram.req_in" },
    { "src": "usrlxemu_ioctl_stub.dgpu_board0", "dst": "dgpu_board0" }
  ]
}
```

### 2.3.1 UsrLinuxEmu IOCTL → CppTLM DGpuBoardTLM 链路图(per UsrLinuxEmu `adr-090` §D3.3 Mode B 替代路径)

> **关键澄清**(per Phase F-H.2 架构重定义,2026-08-20):UsrLinuxEmu `GPU_IOCTL_LAUNCH_KERNEL_MODULE` (0x28) handler **永久锁定返回 -ENOSYS**(per UsrLinuxEmu ADR-090 v2 §D2.2 + ADR-023 §D4)。**真实 launch 由 CppTLM DGpuBoardTLM 通过 0x01 PUSHBUFFER_SUBMIT_BATCH 承载**,driver 通过 pushbuffer 把 NVIDIA GPFIFO entries 写到 CppTLM VRAM 的 pushbuffer ring,CP fetch 解析,逐方法包转发。
>
> **MVP dispatch 链路(per Phase F-H 修订,参考 NVIDIA Hopper 蓝图 `docs/research/WDUtoSM/overview.md`)**:
> ```
> host pushbuffer → CP → TMU → SQ(分发网络)→ CudaCore(SM)
> ```

```
┌────────────────────────────────────────────────────────────────────────────┐
│ driver (TaskRunner/UMD 或移植版 nvidia/amd 真实 driver)                 │
│   ↓ 调用 cuLaunchKernel(grid, block, args, ...)                          │
│ UsrLinuxEmu gpgpu_device::ioctl(0x01 GPU_IOCTL_PUSHBUFFER_SUBMIT_BATCH)  │
│   ↓ handler = 把 gpfifo_entries[] 写到 CppTLM VRAM.pushbuffer_ring 内存   │
│   ↓ (MMIO write GPFIFO entry + doorbell ring trigger)                     │
│ UsrLinuxEmu gpgpu_device::ioctl(0x28 GPU_IOCTL_LAUNCH_KERNEL_MODULE)     │
│   ↓ handler 永久返回 -ENOSYS(per UsrLinuxEmu ADR-090 §D2.2)               │
└─────────────────────────────────────────────────────────────────────────────┘
                                │
                                ▼ (CppTLM CP tick fetch from pushbuffer ring)
┌─────────────────────────────────────────────────────────────────────────────┐
│ CppTLM DGpuBoardTLM::tick()                                                 │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────────┐         │
│   │ cp_.tick() — Command Processor                              │         │
│   │   ├─ FETCH:mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry)) │         │
│   │   ├─ DECODE:parse_method(payload[0])                        │         │
│   │   │       → Pm4MethodDispatch {                            │         │
│   │   │           method_addr, subchannel, data_count,          │         │
│   │   │           args_vram_addr, grid, block, shared_mem, ...  │         │
│   │   │         }                                                │         │
│   │   └─ DISPATCH: tmu_.submit(dispatch_packet)                  │         │
│   └─────────────────────────────────────────────────────────────┘         │
│                              │                                              │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────┐         │
│   │ tmu_.tick() — Task Management Unit                          │         │
│   │   ├─ submit_kernel(record)                                  │         │
│   │   │     ├─ 1. select_cluster(stream_id) → cluster_id       │         │
│   │   │     ├─ 2. inflight_kernel_reqs_[task_id] = record       │         │
│   │   │     ├─ 3. dep_latch 校验(wait_on / arrive_at)          │         │
│   │   │     └─ 4. pre_dispatch:                                  │         │
│   │   │           sq_[cluster_id].enqueue(cta_descriptor)        │         │
│   │   │     ├─ 5. 反压停 fetch(per Phase F-D.2 H5)             │         │
│   │   │     │     容量满 → BACKPRESSURED → CP 反压              │         │
│   │   └─ try_chain_dependent 推进 dep chain(同链路内)          │         │
│   └─────────────────────────────────────────────────────────────┘         │
│                              │                                              │
│                              ▼ (分发网络:WDU + Work Distribution Crossbar) │
│   ┌─────────────────────────────────────────────────────────────┐         │
│   │ sq_.tick() — Submission Queue (WDU 分发网络)                │         │
│   │   ├─ dispatch_to_core(cta_desc)                              │         │
│   │   │     路由选择:cluster_id → target_core_id                │         │
│   │   │     (MVP 单 SM 路由;v0.5 完整版可扩展为 Work Dist Xbar  │         │
│   │   │      per `docs/research/WDUtoSM/overview.md` §NVIDIA WDU)│        │
│   │   └─ cuda_core_[target_core_id].on_cta_arrival(cta_desc)    │         │
│   └─────────────────────────────────────────────────────────────┘         │
│                              │                                              │
│                              ▼ (驱动式 warp 执行,深度集成 PTX-EMU)         │
│   ┌─────────────────────────────────────────────────────────────┐         │
│   │ cuda_core_.tick() — Cuda Core (SM)                           │         │
│   │   ├─ 1. sm->exe_once()(per sm_context.h:59)                 │         │
│   │   │       PTX-EMU 内部:WarpScheduler 选 warp                │         │
│   │   │       → WarpContext::execute_warp_instruction × N       │         │
│   │   ├─ 2. 镜像 PC + cycle:WarpState[warp_id]={                │         │
│   │   │       pc = w->get_thread_pc(0),                          │         │
│   │   │       cycle_count = sm->get_cycle_count()                │         │
│   │   │     }                                                    │         │
│   │   └─ 3. on_warp_complete(task_id, status)                   │         │
│   │         ├─ sm->release_resources()                          │         │
│   │         ├─ sq_->on_warp_complete(task_id)                   │         │
│   │         │     ├─ tmu_->on_complete(task_id)                 │         │
│   │         │     └─ cq_.push(task_id, status) → host_notify    │         │
│   │         └─ gpu_ctx_->clear_requests()                       │         │
│   └─────────────────────────────────────────────────────────────┘         │
│                              │                                              │
│                              ▼                                              │
│   ┌─────────────────────────────────────────────────────────────┐         │
│   │ cq_.process_pending_host_notify() → HAL fence_signal        │         │
│   │     └─ driver cuStreamSynchronize() 返回                    │         │
│   └─────────────────────────────────────────────────────────────┘         │
└─────────────────────────────────────────────────────────────────────────────┘
```

**链路要点**(per Phase F-H.2 修订):
1. **CP 是真启动点**(per `command-processor.md` §4.2 fetch_packet):MVP 通过 0x01 PUSHBUFFER 把 gpfifo entries 写到 VRAM,CP 从 VRAM pushbuffer ring 读,`mem_read_vram(GPU VA)` 而非 BAR0 MMIO
2. **TMU 是依赖解耦层**(per `tmu-dispatch-processor.md` §1):inflight_kernel_reqs_[32 slot] + dep_latch + 反压停 fetch
3. **SQ 是 WDU 分发网络**(per `submit-queue.md` §1):CTA 路由到 target_core_id(MVP 单 SM 即可,v0.5 完整版可扩展为 Work Distribution Crossbar)
4. **CudaCore 深度集成 PTX-EMU**(per `cuda-core-adapter.md` §2):不调 ABI `ptxemu_image_execute`,改用内部 C++ 实例方法驱动 PC

**对比表**:

| 路径 | UsrLinuxEmu 端 | CppTLM 端 |
|------|----------------|-----------|
| **IOCTL 0x01** (PUSHBUFFER SUBMIT BATCH) | handler = 写 gpfifo entries 到 CppTLM VRAM.pushbuffer_ring | CP::tick() fetch + decode + dispatch |
| **IOCTL 0x27** (LOAD) | handler = HAL #66 → H2D DMA 写 CppTLM VRAM | `DGpuBoardTLM::install_kernel_module` 接收 |
| **IOCTL 0x28** (LAUNCH) | handler = -ENOSYS(永久锁定) | **MVP 不直接承载**,0x01 pushbuffer 才是真 launch 入口 |
| **IOCTL 0x29** (UNLOAD) | handler = 走 FREE_BO 路径 → `DGpuBoardTLM::uninstall_kernel_module(vram_addr)`(真实工作,per ADR-SOC-06 D4) | `DGpuBoardTLM::uninstall_kernel_module` 接收(vram_addr 释放) |

**MVP stub 模式测试覆盖**: `test_usrlxemu_ioctl_stub.cc` 验证 4 IOCTL 端到端链路(per ADR-SOC-06 G-MVP-5)。

---

## 3. 接口(Public API)

```cpp
class DGpuBoardTLM : public ChStreamModuleBase {
public:
    // === 构造 + JSON 注入 ===
    explicit DGpuBoardTLM(const std::string& name, EventQueue* eq);
    ~DGpuBoardTLM() override = default;

    std::string get_module_type() const override { return "DGpuBoardTLM"; }

    // === ChStreamModuleBase ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;

    // === MVP Host Entry API(模拟 UsrLinuxEmu driver 角色) ===

    /// IOCTL 0x27 equivalent: H2D DMA image_bytes → VRAM
    /// 返回 vram_addr(VRAM 偏移,非 host pointer)
    uint64_t install_kernel_module(const uint8_t* image_bytes, size_t size);

    /// IOCTL 0x28 equivalent: 构造 KernelLaunchRequest + 触发 CP
    /// 返回 image_id(>=0=成功, -1=失败)
    int32_t submit_kernel(const KernelLaunchRequest& req);

    /// IOCTL 0x29 equivalent: 释放 VRAM backing + CQ push status
    int32_t uninstall_kernel_module(uint64_t vram_addr);

    /// PCIe BAR0 MMIO 写入(Doorbell ring 触发)
    void write_reg(uint32_t offset, uint32_t value);
    uint32_t read_reg(uint32_t offset) const;

    // === 测试 / 监控用 ===
    DGpuBar& bar() { return bar_; }
    Doorbell& doorbell() { return doorbell_; }
    CommandProcessor& command_processor() { return cp_; }
    TmuDispatchProcessor& tmu() { return tmu_; }
    CudaCoreAdapter& cuda_core() { return cuda_core_; }
    CompletionRing& completion_ring() { return cq_; }
    PtxEmuSubmoduleMVP& ptx_emu() { return ptx_emu_; }

    /// JSON params 注入(per ModuleFactory instantiateAll)
    void on_config_loaded() override;

private:
    // === 内部 6 组件(per Phase F-H.2 架构重定义: CP→TMU→SQ→CudaCore) ===
    DGpuBar bar_;
    Doorbell doorbell_;
    CommandProcessor cp_;
    TmuDispatchProcessor tmu_;
    SubmitQueue sq_;            // 分发网络(per submit-queue.md)
    CompletionRing cq_;
    CudaCoreAdapter cuda_core_;
    PtxEmuSubmoduleMVP ptx_emu_;  // 深度集成 adapter(per ptx-emu-submodule-mvp.md)

    // === ChStream 端口 ===
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle> req_in_;
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle> resp_out_;

    // === 配置 ===
    std::string ptx_emu_root_;
    uint32_t vram_size_mb_ = 256;
    uint32_t max_streams_ = 4;
    uint32_t sq_depth_ = 32;                  // SubmitQueue 深度
    uint32_t tmu_max_active_tasks_ = 32;      // TMU inflight slot 数
    bool tmu_enable_backpressure_ = true;     // 反压停 fetch(per Phase F-D.2 H5)
    // ❌ 移除(per Phase F-H.1 修订):
    // - tmu_lifo_evict_: 反压替代 LIFO
    // - enable_whitebox_path_: 改为唯一路径(白盒 + 深度集成)

    // === 统计 ===
    uint64_t kernels_submitted_ = 0;
    uint64_t kernels_completed_ = 0;
    uint64_t doorbell_rings_ = 0;
    uint64_t cycles_since_reset_ = 0;
};
```

---

## 4. tick() 行为流程

```cpp
void DGpuBoardTLM::tick() {
    cycles_since_reset_++;

    // 1. Doorbell strong-ordered write(per v0.4 design §3.2.5,延迟 250-700ns 区间)
    //    Doorbell 内部已 schedule_after_cycles 处理延迟

    // 2. **CP→TMU→SQ→CudaCore 单 dispatch 链路**(per Phase F-H.2 架构重定义):
    //    cp_.tick() → tmu_.tick() → sq_.tick() → cuda_core_.tick()(对齐 §2.3.1 图 + ADR D5 + S1)
    //
    //    **MVP 范围**:全链路激活
    //    - **CP**:fetch gpfifo entry from VRAM.pushbuffer_ring → parse_method → DECODE → DISPATCH
    //      → 构造 Pm4MethodDispatch packet → tmu_.submit(...)
    //    - **TMU**:inflight_kernel_reqs_[32 slot] + dep_latch 校验 + pre_dispatch → sq_.enqueue(cta)
    //    - **SQ**:WDU 分发网络(per submit-queue.md)→ cuda_core_[target].on_cta_arrival(cta)
    //    - **CudaCore**:深度集成 PTX-EMU(per cuda-core-adapter.md §2.1)→ per-tick sm->exe_once()
    //      + 镜像 PC + cycle 到 WarpState + on_warp_complete → SQ → TMU → CQ
    cp_.tick();
    tmu_.tick();   // dep chain advance + pre_dispatch
    sq_.tick();    // 分发网络:CTA → target_core_id
    cuda_core_.tick();  // 驱动 SM cycle + 镜像 WarpState

    // 3. CompletionRing host_notify(若 push 触发)
    cq_.process_pending_host_notify();  // 释放锁后再调 hook

    // 4. Adapter tick
    if (adapter_) adapter_->tick();

    // **MVP 单链路说明**(per Phase F-H.2 修订):
    //   host pushbuffer → CP.fetch → CP.decode(Pm4MethodDispatch) → TMU.submit → SQ.enqueue
    //   → SQ.dispatch → CudaCore.on_cta_arrival → per-tick exe_once → on_warp_complete
    //   → SQ.on_warp_complete → TMU.on_complete → CQ.push → host_notify
    //   - 输入:UsrLinuxEmu driver 经 ioctl(0x01 PUSHBUFFER_SUBMIT_BATCH) 把 gpfifo entries 写到 VRAM
    //   - 输出:driver cuStreamSynchronize() 经 host_notify 收到 fence
    //   - **无独立 dispatch 路径**:cp_.tick() 是唯一从 host 输入入口,sq_.tick() 是唯一中间分发节点
    //   - **历史回溯**:Phase F-C.2 早期修订误把 cp_.tick() 移出,Phase F-G 恢复 sq_+tmu_,Phase F-H 补全 cp_+sq_+tmu_+cuda_core_ 全链路
}
```

---

## 5. 关键时序特性(MVP 默认配置)

| 阶段 | 延迟 | 备注 |
|------|------|------|
| `ioctl(0x27)` → H2D DMA → `install_kernel_module` | 模拟为 1 tick(0 cycle 延迟) | MVP 不仿真 H2D 物理延迟 |
| `write_reg(0x1000+stream_id)` → Doorbell ring | **250-700ns 区间断言**(per v0.4 §3.2.5 + [`docs/research/PCIe/PCIe_上的保序write.md`](../../research/PCIe/PCIe_上的保序write.md) §4 non-posted read flush 延迟 Gen5 250-350ns / 多级 switch 400-600ns / 跨 RC >700ns,per Phase F-E.2 L1) | MVP 仅断言区间,**不仿真 MMU ordering pipe**(stub 模式单线程 tick 不可能真发生 TLP 乱序) |
| CP FETCH → DECODE → DISPATCH | ~1 tick(简化) | MVP 不仿真 PM4 解析 cycle 精度 |
| SQ tick → SubmitQueue.dispatch_to_core → CudaCoreAdapter::on_cta_arrival | 1 tick | 深度集成路径(per Phase I.2) |
| CudaCoreAdapter::tick() → sm->exe_once() | variable | 驱动 SM cycle + WarpState 镜像 |
| CompletionRing::push + host_notify | ~50ns 模拟 | signal hook |

**总 cycle budget(MVP 默认)**:`500ns-2us + kernel exec`

---

## 6. 测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_dgpu_board_v1_mvp_from_config.cc` | `[dgpu-board][mvp][e2e]` | 6 SECTION: validate_topology / instantiateAll / H2D / launch / host_notify / 负面路径 |
| `test_dgpu_board_mvp_unit.cc` | `[dgpu-board][mvp][unit]` | 单元测试:install_kernel_module / submit_kernel / write_reg / Doorbell ring strong-order |
| `test_usrlxemu_ioctl_stub.cc` | `[usrlxemu-ioctl][stub]` | IOCTL 0x27/0x28/0x29 stub 端到端 |
| `test_dgpu_bar_strong_order_mvp.cc` | `[dgpu-bar][mvp]` | Doorbell strong-order 延迟区间断言 250-700ns |

**验收标准**(per ADR-SOC-06 G-MVP-2):
- `cmake --build build --target validate_topology` PASS
- `ctest -R "test_dgpu_board_v1_mvp_from_config" --output-on-failure` 6 SECTION PASS
- `git grep "include.*ptxsim"` 仅命中 `ptx_emu_submodule_mvp.cc`

---

## 7. 配置参数(JSON params)

| 参数 | 类型 | 默认值 | 说明 |
|------|------|:---:|------|
| `ptx_emu_root` | string | `@PTX_EMU_ROOT@` | PTX-EMU submodule 路径(由 CMake configure_file 注入) |
| `vram_size_mb` | uint32 | 256 | BAR1 VRAM 容量 |
| `max_streams` | uint32 | 4 | SubmitQueue 流数(per-stream FIFO) |
| `sq_depth` | uint32 | 32 | 每 SubmitQueue pending 深度 |
| `tmu_max_active_tasks` | uint32 | 32 | TmuDispatchProcessor slot 数 |
| `tmu_enable_backpressure` | bool | true | 容量满时反压停 fetch(per Phase F-D.2 H5) |
| `enable_dgpu_bar_mmio` | bool | true | 是否启用 BAR0 MMIO 路由(否则直接 API 路径) |
| `usrlxemu_ioctl_stub_mode` | string | `all` | stub 启用 IOCTL: `all` / `load` / `launch` / `unload` / `none` |
| ❌ 删除(已废弃) | — | — | `tmu_lifo_evict`(反压替代 LIFO,per Phase F-D.2 H5); `enable_whitebox_path`(深度集成替代白盒,per Phase I.1) |

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| ~~R1 | TmuDispatchProcessor LIFO 频繁驱逐~~ | — | — | **🗑️ 风险已消除(per Phase F-D.2 H5)**:反压停 fetch,容量满拒绝不驱逐 |
| R2 | Doorbell strong-order 延迟区间违反 | 中 | 中 | 测试断言 250-700ns 区间;PCIe Gen5 x16 默认;**数字依据** per [`docs/research/PCIe/PCIe_上的保序write.md`](../../research/PCIe/PCIe_上的保序write.md) §4(per Phase F-E.2 L1) |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R4 | 8 组件组合 tick() 调度顺序冲突 | 低 | 高 | 严格顺序:cp_.tick() → tmu_.tick() → sq_.tick() → cuda_core_.tick() |
| R5 | JSON config params 注入失败 | 中 | 中 | `on_config_loaded` try-catch + 详细错误日志 |
| R6 | VRAM backing 与 MemoryTLM 端口不匹配 | 中 | 中 | DualPortStreamAdapter 验证 |

---

## 9. 实施路径

### 9.1 S2 Real-Board-Bind(W3-4)

1. 新建 `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc`(~500 LOC)
2. 复用 `include/tlm/gpu/dgpu_bar.hh`(v0.4 已实施)
3. 新建 `include/tlm/gpu/doorbell_mvp.hh` + `.cc`(~150 LOC,简化 strong-order)
4. 新建 `include/tlm/gpu/submit_queue_mvp.hh` + `.cc`(~150 LOC,WDU 分发网络,per Phase F-H.5)
5. 新建 `include/tlm/gpu/completion_ring_mvp.hh` + `.cc`(~150 LOC,重设计 host_notify)
6. 新建 `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc`(~300 LOC)
7. 修改 `include/chstream_register.hh`:加 `REGISTER_CHSTREAM(DGpuBoardTLM)` + `UsrLinuxEmuIoctlStub`
8. 新建 `configs/dgpu_board_v1_mvp.json.in`(CMake configure_file 注入 `${PTX_EMU_ROOT}`)
9. 新建 `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION)
10. 新建 `test/test_usrlxemu_ioctl_stub.cc`(**4 IOCTL**:0x27/0x28-ENOSYS/0x29/0x01,per Phase F-H.3)
11. 更新 `docs/soc_arch/modules/README.md` + `docs/soc_arch/adr/README.md`

### 9.2 估计工作量

- 设计:0.5d(本设计文档已就绪)
- 实施:5-7d(S2 阶段)
- 测试:1-2d
- 文档:0.5d
- **总计:7-10d(W3-4 内完成)**

---

## 10. 修订历史

- **2026-08-19**: 初版 — per ADR-SOC-06 D5/D6 切片(MVP 4 阶段 S2)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
