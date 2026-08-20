# dgpu-board 微架构文档

> **类别**: GPU > dGPU Board · **状态**: 🔵 MVP 切片 (per ADR-X.17)
> **Header**: `include/tlm/gpu/dgpu_board_mvp.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`, 新增)
> **蓝图来源**: gem5 `src/dev/amdgpu/amdgpu_device.py` + `src/dev/pci/pci_host.py` + UsrLinuxEmu ADR-090 v2 §D3.3
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D5/D6
> **首版 commit**: � W3-4 实施 · **最近更新**: 2026-08-19
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - ADR: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D1-D6
> - 组件契约: [`command-processor.md`](./command-processor.md) · [`pm4-decoder.md`](./pm4-decoder.md) · [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md) · [`cuda-core-adapter.md`](./cuda-core-adapter.md) · [`ptx-emu-submodule-mvp.md`](./ptx-emu-submodule-mvp.md)
> - UsrLinuxEmu IOCTL: [`UsrLinuxEmu/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md`](../../../external/UsrLinuxEmu/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) §D3.3

---

## 1. 设计目标

`DGpuBoardTLM` 是 **dGPU 板卡的 ChStreamModuleBase 包装**,在 MVP 阶段引入,负责将 5 个内部组件(DGpuBar + Doorbell + CommandProcessor + TmuDispatchProcessor + CudaCoreAdapter + SubmissionQueue + CompletionRing)组装为一个 **JSON-driven 单芯片 dGPU 仿真目标**。

**核心特性**:
- 继承 `ChStreamModuleBase`,支持 JSON 拓扑驱动实例化(per `ModuleFactory::instantiateAll`)
- 内部 5 组件组合:DGpuBar(PCIe BAR0 MMIO)+ Doorbell + CommandProcessor + TmuDispatchProcessor + CudaCoreAdapter
- 入口方法:`install_kernel_module(vram_addr, size)` + `submit_kernel(KernelLaunchRequest)` + `write_reg(offset, value)` 模拟 UsrLinuxEmu driver 角色
- **`tick()` 由 EventQueue 调度**,内部驱动 SQ consumer + CudaCoreAdapter + CompletionRing::host_notify
- **MVP 接 UsrLinuxEmu IOCTL 0x27/0x28 stub server**(不直接 link UsrLinuxEmu)

**MVP vs v0.4 简化**:
- ✅ 保留:DGpuBar + Doorbell + SQ + CQ(per ADR-X.15 §4.2 + v0.4 design §3)
- ❌ 裁剪:TmuDispatchProcessor 32 slot(v0.4 是 256 slot)+ Scheduler Cache LRU(仅 LIFO)
- ❌ 裁剪:MMU ordering pipe 精确建模(仅延迟区间断言 250-700ns)
- ❌ 裁剪:MSI-X 中断 + DMA channel(推到 v0.5 完整版)

---

## 2. 架构概览

### 2.1 内部 5 组件层次

```
DGpuBoardTLM (ChStreamModuleBase, JSON-driven)
├── DGpuBar (PCIe BAR0 MMIO + BAR1 VRAM)
│    ├── BAR0 0x0000-0x0FFF: device regs (vendor/device ID + 控制)
│    ├── BAR0 0x1000-0x1FFF: doorbell ring MMIO space (per subchannel)
│    └── BAR1: VRAM backing (256MB, mmap'd to host)
├── Doorbell (SQ tail register, strong-ordered)
│    ├── atomic<uint64_t> sq_tail_[MAX_STREAMS=1024]
│    └── ring(stream_id, tail) → notify SQ consumer
├── CommandProcessor (CP, 5-state FSM)
│    ├── Pm4Decoder (Mesa-style TYPE3, 4 MVP opcodes)
│    ├── SubchannelContext[5] (subchannel 0-4)
│    └── CommandDispatcher (opcode → handler)
├── TmuDispatchProcessor (TMU Glue, 32 slot MVP)
│    ├── inflight_kernel_reqs_ map (32 slot + LIFO)
│    ├── dep latch: wait_on ↔ arrive_at 匹配
│    └── submit / on_complete / try_chain_dependent
├── SubmissionQueue[stream_id] (per-stream FIFO)
│    └── enqueue(KernelLaunchRequest) → tick() → dispatch
├── CompletionRing (host_notify)
│    ├── push(image_id, status) → release mutex
│    └── host_notify_() → HAL fence_signal
└── CudaCoreAdapter (per-warp step 入口)
     ├── 黑盒路径: PtxEmuSubmoduleMVP::image_execute
     └── 白盒路径: 循环 PtxEmuSubmoduleMVP::stepOneWarpInstruction (S3 启用)
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
        "tmu_lifo_evict": true,
        "enable_whitebox_path": false,
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
    // === 内部 5 组件 ===
    DGpuBar bar_;
    Doorbell doorbell_;
    CommandProcessor cp_;
    TmuDispatchProcessor tmu_;
    SubmissionQueue sq_;
    CompletionRing cq_;
    CudaCoreAdapter cuda_core_;
    PtxEmuSubmoduleMVP ptx_emu_;

    // === ChStream 端口 ===
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle> req_in_;
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle> resp_out_;

    // === 配置 ===
    std::string ptx_emu_root_;
    uint32_t vram_size_mb_ = 256;
    uint32_t max_streams_ = 4;
    uint32_t sq_depth_ = 32;
    uint32_t tmu_max_active_tasks_ = 32;
    bool tmu_lifo_evict_ = true;
    bool enable_whitebox_path_ = false;

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

    // 2. SubmissionQueue consumer tick
    sq_->tick();  // 内部调 cuda_core_.dispatch_blackbox(image, args_vram, params)

    // 3. CommandProcessor 推进 5-state FSM
    cp_.tick();  // FETCH → DECODE → DISPATCH → COMPLETE

    // 4. TmuDispatchProcessor 处理 dep chain 推进
    tmu_.tick();  // try_chain_dependent 链式推进 dep.ptr 指向的下一任务

    // 5. CompletionRing host_notify(若 push 触发)
    cq_.process_pending_host_notify();  // 释放锁后再调 hook

    // 6. Adapter tick
    if (adapter_) adapter_->tick();
}
```

---

## 5. 关键时序特性(MVP 默认配置)

| 阶段 | 延迟 | 备注 |
|------|------|------|
| `ioctl(0x27)` → H2D DMA → `install_kernel_module` | 模拟为 1 tick(0 cycle 延迟) | MVP 不仿真 H2D 物理延迟 |
| `write_reg(0x1000+stream_id)` → Doorbell ring | **250-700ns 区间断言**(per v0.4 §3.2.5) | MVP 仅断言区间,不仿真 MMU pipe |
| CP FETCH → DECODE → DISPATCH | ~1 tick(简化) | MVP 不仿真 PM4 解析 cycle 精度 |
| SQ tick → CudaCoreAdapter::dispatch_blackbox | 1 tick | 黑盒 MVP 路径 |
| CudaCoreAdapter::image_execute → PTX-EMU | variable | 自包含 GPU sim |
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

**验收标准**(per ADR-X.17 G-MVP-2):
- `cmake --build build --target validate_topology` PASS
- `ctest -R "test_dgpu_board_v1_mvp_from_config" --output-on-failure` 6 SECTION PASS
- `git grep "include.*ptxsim"` 仅命中 `ptx_emu_submodule_mvp.cc`

---

## 7. 配置参数(JSON params)

| 参数 | 类型 | 默认值 | 说明 |
|------|------|:---:|------|
| `ptx_emu_root` | string | `@PTX_EMU_ROOT@` | PTX-EMU submodule 路径(由 CMake configure_file 注入) |
| `vram_size_mb` | uint32 | 256 | BAR1 VRAM 容量 |
| `max_streams` | uint32 | 4 | SubmissionQueue 数(per-stream FIFO) |
| `sq_depth` | uint32 | 32 | 每 SQ 深度 |
| `tmu_max_active_tasks` | uint32 | 32 | TmuDispatchProcessor slot 数 |
| `tmu_lifo_evict` | bool | true | 容量满时是否 LIFO 驱逐 |
| `enable_whitebox_path` | bool | false | 是否启用 `stepOneWarpInstruction` 白盒路径(需 PTX-EMU 新 API) |
| `enable_dgpu_bar_mmio` | bool | true | 是否启用 BAR0 MMIO 路由(否则直接 API 路径) |
| `usrlxemu_ioctl_stub_mode` | string | `all` | stub 启用 IOCTL: `all` / `load` / `launch` / `unload` / `none` |

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | TmuDispatchProcessor LIFO 频繁驱逐 | 中 | 中 | 32 slot MVP 默认;溢出率 >5% 触发 review |
| R2 | Doorbell strong-order 延迟区间违反 | 中 | 中 | 测试断言 250-700ns 区间;PCIe Gen5 x16 默认 |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R4 | 5 组件组合 tick() 调度顺序冲突 | 低 | 高 | 严格顺序:SQ → CP → TMU → CQ + Adapter |
| R5 | JSON config params 注入失败 | 中 | 中 | `on_config_loaded` try-catch + 详细错误日志 |
| R6 | VRAM backing 与 MemoryTLM 端口不匹配 | 中 | 中 | DualPortStreamAdapter 验证 |

---

## 9. 实施路径

### 9.1 S2 Real-Board-Bind(W3-4)

1. 新建 `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc`(~400 LOC)
2. 复用 `include/tlm/gpu/dgpu_bar.hh`(v0.4 已实施)
3. 新建 `include/tlm/gpu/doorbell_mvp.hh` + `.cc`(~150 LOC,简化 strong-order)
4. 新建 `include/tlm/gpu/submission_queue_mvp.hh` + `.cc`(~100 LOC)
5. 新建 `include/tlm/gpu/completion_ring_mvp.hh` + `.cc`(~150 LOC,重设计 host_notify)
6. 新建 `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc`(~250 LOC)
7. 修改 `include/chstream_register.hh`:加 `REGISTER_CHSTREAM(DGpuBoardTLM)` + `UsrLinuxEmuIoctlStub`
8. 新建 `configs/dgpu_board_v1_mvp.json.in`(CMake configure_file 注入 `${PTX_EMU_ROOT}`)
9. 新建 `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION)
10. 新建 `test/test_usrlxemu_ioctl_stub.cc`(3 IOCTL)
11. 更新 `docs/soc_arch/modules/README.md` + `docs/adr/README.md`

### 9.2 估计工作量

- 设计:0.5d(本设计文档已就绪)
- 实施:5-7d(S2 阶段)
- 测试:1-2d
- 文档:0.5d
- **总计:7-10d(W3-4 内完成)**

---

## 10. 修订历史

- **2026-08-19**: 初版 — per ADR-X.17 D5/D6 切片(MVP 4 阶段 S2)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
