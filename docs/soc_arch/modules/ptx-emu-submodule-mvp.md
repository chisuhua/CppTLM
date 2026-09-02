# ptx-emu-submodule-mvp 微架构文档(per Phase I.1 重构)

> **类别**: GPU > PTX-EMU **Functional Model Facade** · **状态**: 🔵 MVP 切片 (per ADR-SOC-06) + 📋 v1.0 dGPU SoC 战略补充(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md))
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **Header**: `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + **`.cc` 为唯一 PTX-EMU 头 include 位置(编译防火墙)**
> **位置**: DGpuBoardTLM 内部组件
> **蓝图来源**: PTX-EMU 内部 C++ 实例接口(`ptxsim/gpu_context.h` + `ptxsim/sm_context.h` + `ptxsim/warp_context.h` + `ptx_ir/ptxir_reader.h`) + gpgpu-sim `cuda-sim/` 功能模拟分层模式
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-SOC-06-cpptlm-v05-mvp`](../../soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D3
> **关联模块**: [`cuda-core-adapter.md`](./cuda-core-adapter.md)(SM 微架构模拟器,与本模块严格分离)
> **首版 commit**: 🔵 W1-2 实施 · **最近更新**: 2026-08-20(Phase I 重构)
> **维护者**: CppTLM Team (Sisyphus)

> **关联调研**: PTX-EMU submodule `external/PTX-EMU@87820951`(per DP1=B 决策,2026-08-13 audit commit)。架构参照 gpgpu-sim `cuda-sim/` 模块:负责**功能正确性**,不关心 timing/pipeline/stall。`CudaCoreAdapter` 才是 timing model 责任人。

---

## 1. 设计目标(per Phase I.1 重构)

`PtxEmuSubmoduleMVP` 是 **PTX 指令功能正确性保证的 facade**,严格遵循 **gpgpu-sim functional/timing 分离原则**:

| 维度 | PtxEmuSubmoduleMVP(本模块) | CudaCoreAdapter(对偶模块) |
|------|---------------------------|---------------------------|
| **职责** | PTX 指令**功能正确性** | SM **微架构行为**(timing) |
| **关心** | 寄存器值、内存值、计算结果、PC 推进、SIMT 分支 | cycle、stall、pipeline delay、scoreboard hazard、warp 调度 |
| **不关心** | cycle 数、stall 原因、调度策略、流水线延迟 | 单条指令具体计算什么 |
| **接口风格** | 单指令功能调用 + 状态读/写 + 构造 | per-tick 推进 + timing 注入接口 |
| **类比 gpgpu-sim** | `cuda-sim/`(functional) | `gpgpu-sim/shader_core/`(timing) |

### 1.1 编译防火墙(per ADR-SOC-06 D3)

```
CppTLM
├── include/tlm/gpu/
│   ├── ptx_emu_submodule_mvp.hh       ← 仅前向声明 PTX-EMU 内部类
│   ├── cuda_core_adapter_mvp.hh       ← 不 include PTX-EMU 头
│   ├── command_processor_mvp.hh      ← 不 include PTX-EMU 头
│   └── ...
├── src/tlm/gpu/
│   ├── ptx_emu_submodule_mvp.cc       ← ⭐ 唯一 include PTX-EMU 头的位置
│   │                                    #include <ptxsim/gpu_context.h>
│   │                                    #include <ptxsim/sm_context.h>
│   │                                    #include <ptxsim/warp_context.h>
│   │                                    #include <ptx_ir/ptxir_reader.h>
│   │                                    #include <ptx_ir/statement_context.h>
│   │                                    #include <memory/simple_memory.h>
│   │                                    #include <register/register_bank_manager.h>
│   ├── cuda_core_adapter_mvp.cc       ← 不 include PTX-EMU 头(只调 facade)
│   └── ...
└── external/PTX-EMU/                  ← git submodule(per DP1=B)
    ├── include/ptxsim/
    ├── include/ptx_ir/
    └── ...
```

**验证命令**:
```bash
git grep "include.*ptxsim\|include.*ptx_ir\|include.*memory/simple_memory\|include.*register/" \
  -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
# 预期: 仅命中 src/tlm/gpu/ptx_emu_submodule_mvp.cc
```

### 1.2 职责严格分离(per Phase I.1)

**本模块(PtxEmuSubmoduleMVP)负责**:
1. PTX IR 解码(`PtxirReader::read()`)
2. PTX-EMU 内部对象构造(GPUContext / SMContext / WarpContext 实例化)
3. **单条 PTX 指令的功能执行**(`WarpContext::execute_warp_instruction(stmt, target_pc)`)
4. **功能状态读写**(寄存器值、内存值、PC、exec mask)
5. PTX-EMU 内部子模块代理(scoreboard、pipeline、TC)实例化接口的 getter

**本模块不负责**(由 CudaCoreAdapter 处理):
1. ❌ cycle 推进(`SMContext::exe_once()`)— 这是 timing model 的入口
2. ❌ WarpScheduler(per cycle 选哪个 warp)— 时序调度
3. ❌ Scoreboard hazard 检查的时机判断— timing 关心
4. ❌ Pipeline latency 注入 — timing 关心
5. ❌ TensorCore timing — timing 关心

---

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────────────┐
│ CudaCoreAdapter(微架构侧,见 cuda-core-adapter.md)                  │
│   tick() {                                                          │
│     sm->exe_once();        ← Timing model 入口(由 CudaCoreAdapter驱动)│
│       │                                                              │
│       ├─ Step A: scoreboard.check()       ← Timing 问 hazard         │
│       ├─ functional_execute_warp(...)     ← ★ 通过本 facade 转发     │
│       ├─ Step B: pipeline.get_latency()   ← Timing 注入延迟          │
│       └─ Step C: scoreboard.release()     ← Timing 释放资源          │
│   }                                                                  │
│       │                                                              │
│       ▼ 转发到本 facade                                               │
└─────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────────────────┐
│ PtxEmuSubmoduleMVP(本模块,functional facade)                         │
│                                                                      │
│   ── Functional Construction ──                                      │
│   create_gpu_context()  → std::unique_ptr<ptxsim::GPUContext>      │
│   create_sm_context()   → ptxsim::SMContext*                        │
│   create_warp_context() → ptxsim::WarpContext*                      │
│   decode_ptxir()        → std::vector<ptx_ir::StatementContext>     │
│   submit_kernel_request(gpu, req)                                    │
│                                                                      │
│   ── Functional Execute(★ 本模块核心) ──                            │
│   functional_execute_warp(warp, stmt, target_pc)                     │
│     → 转发到 ptxsim::WarpContext::execute_warp_instruction           │
│     → 仅关心"这条 PTX 指令计算什么"                                │
│     → 不关心 cycle、不关心 stall、不关心 hazard                       │
│                                                                      │
│   ── Functional State(状态读/写) ──                                │
│   read_register<T>(warp, lane, reg_name)    → T                      │
│   write_register<T>(warp, lane, reg_name, val)                      │
│   read_global_memory<T>(gpu, addr)          → T                      │
│   write_global_memory<T>(gpu, addr, val)                             │
│   read_thread_pc(warp, lane)                → uint32_t               │
│   advance_thread_pc(warp, lane, new_pc)                              │
│   read_active_mask(warp)                    → uint32_t               │
│   is_warp_finished(warp)                    → bool                   │
│   is_thread_exited(warp, lane)              → bool                   │
│                                                                      │
│   ── Functional Module Getters(供 CudaCoreAdapter 注入) ──         │
│   scoreboard_factory()        → IScoreboard*(默认 nullptr = 走 PTX-EMU)│
│   pipeline_provider_factory() → IPipelineLatencyProvider*           │
│   tensor_core_factory()       → ITensorCoreTiming*                  │
└─────────────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────────────────┐
│ PTX-EMU internal(git submodule)                                      │
│   ptxsim::GPUContext / SMContext / WarpContext / ThreadContext       │
│   ptx_ir::PtxirReader / StatementContext                              │
│   memory::SimpleMemory / register::RegisterBankManager               │
│   scoreboard::IScoreboard / pipeline::IPipelineLatencyProvider /     │
│   tensor::ITensorTiming                                             │
└─────────────────────────────────────────────────────────────────────┘
```

**关键设计契约**(per Phase I.1):

1. **Functional 永远不知 cycle 为何物**:本模块接口中没有 cycle/stall/hazard 概念
2. **Timing 不直接调用 functional**:CudaCoreAdapter 通过本 facade 转发,**自己不直接调** PTX-EMU 内部
3. **State 读写是双向的**:测试可以读寄存器验证功能正确性,不污染 timing
4. **Module getters 只暴露接口不创建对象**:让 CudaCoreAdapter 决定用什么实现注入(默认 nullptr)

---

## 3. 接口(Public API)(per Phase I.1 重构)

```cpp
// include/tlm/gpu/ptx_emu_submodule_mvp.hh
// 仅前向声明 PTX-EMU 内部类,不 include 头
namespace ptxsim { class GPUContext; class SMContext; class WarpContext; }
namespace ptx_ir { class PtxirReader; struct StatementContext; struct KernelEntry; }
namespace memory { class SimpleMemory; }
namespace register_bank { class RegisterBankManager; }
class IScoreboard;
class IPipelineLatencyProvider;
class ITensorCoreTiming;

class PtxEmuSubmoduleMVP {
public:
    /// PTX-EMU submodule 路径(per ADR-SOC-06 D3 锁定 PTX-EMU@87820951)
    explicit PtxEmuSubmoduleMVP();
    ~PtxEmuSubmoduleMVP();

    /// 初始化(从 JSON params 读取 PTX-EMU 路径 + config)
    /// @param config GPU 配置(per ptxsim::GPUConfig:num_sms/max_warps_per_sm/...)
    void init(const std::string& ptx_emu_root, const GPUConfig& config);
    void shutdown();

    // ===================================================================
    // Functional Construction(构造 PTX-EMU 内部对象)
    // ===================================================================

    /// 创建 GPUContext 实例(持有 N 个 SMContext)
    /// MVP 默认 num_sms=1(per ptxsim::GPUConfig::num_sms 默认值)
    std::unique_ptr<ptxsim::GPUContext> create_gpu_context();

    /// 获取 SMContext(从 GPUContext)
    /// MVP 仅 sm_idx=0(单 SM 路由)
    ptxsim::SMContext* get_sm_context(ptxsim::GPUContext& gpu, uint32_t sm_idx = 0);

    /// 获取 WarpContext(从 SMContext)
    /// warp_idx 范围 0..max_warps_per_sm-1
    ptxsim::WarpContext* get_warp_context(ptxsim::SMContext& sm, uint32_t warp_idx);

    /// 解码 PTX IR 二进制(per ptx_ir::PtxirReader::read)
    /// @param image_bytes PTXIR 字节流(per ptxir_format.h PTXIR_MAGIC + PTXIR_VERSION=4)
    /// @param size 字节数
    /// @return std::vector<ptx_ir::StatementContext> 解码后指令序列
    /// @throw std::runtime_error PTXIR magic/version 校验失败
    std::vector<ptx_ir::StatementContext> decode_ptxir(
        const uint8_t* image_bytes, size_t size);

    /// 提交 kernel launch 请求(per ptxsim::GPUContext::submit_kernel_request)
    void submit_kernel_request(ptxsim::GPUContext& gpu,
                               ptxsim::KernelLaunchRequest&& req);

    // ===================================================================
    // Functional Execute(★ 本模块核心:单条 PTX 指令功能执行)
    // ===================================================================

    /// 单 warp 单指令功能执行
    /// @param warp 目标 warp(由调用方从 SMContext 获取)
    /// @param stmt 单条已解码 PTX 指令(由 decode_ptxir() 返回)
    /// @param target_pc 该指令的目标 PC(用于 SIMT 重汇聚)
    /// @post warp 的寄存器、内存、PC 状态按指令语义更新
    /// @note **不增加任何 cycle** — 这是 functional model,不是 timing model
    void functional_execute_warp(ptxsim::WarpContext& warp,
                                  ptx_ir::StatementContext& stmt,
                                  int target_pc);

    /// 等待所有活跃 lane 完成(barrier sync)
    /// 触发后阻塞所有 lane,直到 barrier 计数器归零
    void functional_barrier_sync(ptxsim::WarpContext& warp, int barrier_id);

    /// Warp 退出(ret instruction)
    void functional_exit_warp(ptxsim::WarpContext& warp);

    // ===================================================================
    // Functional State(状态读/写,用于测试与观测)
    // ===================================================================

    /// 读 lane 寄存器值
    template <typename T>
    T read_register(const ptxsim::WarpContext& warp, int lane_id,
                    const std::string& reg_name);

    /// 写 lane 寄存器值
    template <typename T>
    void write_register(ptxsim::WarpContext& warp, int lane_id,
                        const std::string& reg_name, T value);

    /// 读全局内存(per SimpleMemory::direct_access)
    template <typename T>
    T read_global_memory(const ptxsim::GPUContext& gpu, uint64_t address);

    /// 写全局内存
    template <typename T>
    void write_global_memory(const ptxsim::GPUContext& gpu, uint64_t address, T value);

    /// 读 lane 指令 PC
    uint32_t read_thread_pc(const ptxsim::WarpContext& warp, int lane_id);

    /// 读 warp blocked cycles 剩余(per `WarpState::blocked_cycles_remaining`,
    /// FIX-H8/B.3 补缺 — CudaCoreAdapter 镜像 WarpState.blocked_cycles 需此读口,
    /// 避免直接调 PTX-EMU `WarpContext::get_blocked_cycles_remaining` 破坏编译防火墙)
    uint32_t read_blocked_cycles(const ptxsim::WarpContext& warp);

    /// 推进 lane PC(per WarpContext::advance_thread_pc)
    void advance_thread_pc(ptxsim::WarpContext& warp, int lane_id, int new_pc);

    /// 读 warp 活跃掩码(per WarpContext::get_active_mask)
    uint32_t read_active_mask(const ptxsim::WarpContext& warp);

    /// Warp 是否完成(所有 lane 退出,per WarpContext::is_finished)
    bool is_warp_finished(const ptxsim::WarpContext& warp);

    /// Lane 是否退出(per WarpContext::is_all_threads_exited)
    bool is_thread_exited(const ptxsim::WarpContext& warp, int lane_id);

    // ===================================================================
    // Functional Module Getters(供 CudaCoreAdapter 注入 timing 模块)
    // ===================================================================

    /// 创建 Scoreboard(per IScoreboard 纯虚接口)
    /// MVP 默认 nullptr → PTX-EMU 内部默认 Scoreboard
    /// v0.5 完整版返回 ScoreboardTLM 实例
    IScoreboard* create_scoreboard();

    /// 创建 Pipeline latency provider(per IPipelineLatencyProvider)
    /// MVP 默认 nullptr → PTX-EMU 内部 InstructionLatencyTable
    IPipelineLatencyProvider* create_pipeline_latency_provider();

    /// 创建 TensorCore timing provider(per ITensorCoreTiming)
    /// MVP 默认 nullptr → PTX-EMU 默认 TC 延迟
    ITensorCoreTiming* create_tensor_core_timing();

    // === 测试/统计 ===
    bool is_initialized() const { return initialized_; }
    uint64_t functional_execute_count() const { return functional_execute_count_; }

private:
    bool initialized_ = false;
    std::string ptx_emu_root_;
    GPUConfig config_;
    uint64_t functional_execute_count_ = 0;
};
```

---

## 4. 数据流(Functional Model 视角)

### 4.1 PTX IR 解码 + 提交

```cpp
// 1. 调用方从 VRAM 读 PTX IR 字节
const uint8_t* image_bytes = vram_.read(cta.vram_image_addr, cta.image_size);

// 2. ★ 本 facade 解码 PTX IR
std::vector<ptx_ir::StatementContext> stmts =
    ptx_emu_facade_.decode_ptxir(image_bytes, cta.image_size);
// 校验 PTXIR_MAGIC + PTXIR_VERSION=4(per ptxir_format.h)
// 失败抛 std::runtime_error

// 3. ★ 本 facade 创建 GPUContext
auto gpu_ctx = ptx_emu_facade_.create_gpu_context();

// 4. 构造 KernelLaunchRequest(per gpu_context.h:54)
ptxsim::KernelLaunchRequest req {
    .args = cta.kernel_args,
    .gridDim = {cta.grid_x, cta.grid_y, cta.grid_z},
    .blockDim = {cta.block_x, cta.block_y, cta.block_z},
    .statements = &stmts,
    .name2Sym = ...,
    .label2pc = ...,
    .request_id = cta.task_id,
    .on_complete = callback_lambda,
    .shared_mem_size = cta.shared_mem_bytes
};

// 5. ★ 本 facade 提交 kernel
ptx_emu_facade_.submit_kernel_request(*gpu_ctx, std::move(req));
//   → 内部转 ptxsim::GPUContext::submit_kernel_request()
//   → 内部添加 CTA 到 SMContext (add_block)
```

### 4.2 单指令 Functional 执行

```cpp
// 由 CudaCoreAdapter::tick() 在 exe_once() 内部调用
// (本 facade 仅转发,不知道 cycle 计数)

void functional_execute_warp(ptxsim::WarpContext& warp,
                              ptx_ir::StatementContext& stmt,
                              int target_pc) {
    // 校验:仅在 lane 活跃 + PC 匹配时执行(per warp_dispatch::execute_warp_instruction 实现)
    // lane 非活跃 → 跳过,functional 结果不变
    // lane PC 不匹配 → 跳过,functional 结果不变

    // 转发到 PTX-EMU internal
    ptxsim::warp_dispatch::execute_warp_instruction(&warp, stmt, target_pc);

    // PTX-EMU 内部:
    //   for each active lane in warp:
    //     thread->sync_from_warp_state()
    //     handler->ExecPipe(thread, stmt)  ← 功能计算
    //     thread->sync_to_warp_state()
    //     warp_state.threads[lane].pc++     ← PC 推进

    functional_execute_count_++;
}
```

**注意**:此调用**不增加任何 cycle 计数**。Cycle 是 timing model(CudaCoreAdapter)关心的事。

### 4.3 功能正确性测试模式

```cpp
TEST_CASE("PTX ADD instruction functional correctness") {
    PtxEmuSubmoduleMVP facade;
    facade.init("/path/to/PTX-EMU", default_config);

    auto gpu = facade.create_gpu_context();
    auto* sm = facade.get_sm_context(*gpu);
    auto* warp = facade.get_warp_context(*sm, 0);

    // 1. 构造 ADD 指令(per ptx_ir::StatementContext)
    ptx_ir::StatementContext add_stmt(
        ptx_ir::StatementType::ADD,
        /* 编码字段:dst=%r1, src1=%r2, src2=%r3 */
    );

    // 2. 写初值:src2=10, src3=20
    facade.write_register<int32_t>(*warp, /*lane=*/0, "%r2", 10);
    facade.write_register<int32_t>(*warp, /*lane=*/0, "%r3", 20);

    // 3. ★ Functional 执行(不关心 cycle)
    facade.functional_execute_warp(*warp, add_stmt, /*target_pc=*/0);

    // 4. 验证结果:dst = 10 + 20 = 30(功能正确性)
    int32_t result = facade.read_register<int32_t>(*warp, 0, "%r1");
    CHECK(result == 30);

    // 5. 验证 PC 推进
    CHECK(facade.read_thread_pc(*warp, 0) == 1);
}
```

---

## 5. Functional vs Timing 分离矩阵

| 操作 | Functional facade 负责 | CudaCoreAdapter(timing)负责 |
|------|-----------------------|---------------------------|
| 解码 PTX IR | ✅ `decode_ptxir()` | ❌ |
| 创建 GPUContext | ✅ `create_gpu_context()` | ❌ |
| 创建 SMContext | ✅ `get_sm_context()` | ❌ |
| 创建 WarpContext | ✅ `get_warp_context()` | ❌ |
| 提交 kernel | ✅ `submit_kernel_request()` | ❌ |
| **推进一个 SM cycle** | ❌ | ✅ `tick()` → `sm->exe_once()` |
| **WarpScheduler 选 warp** | ❌ | ✅(由 sm->exe_once() 内部) |
| **单指令 functional 执行** | ✅ `functional_execute_warp()` | ❌(只转发) |
| **Scoreboard hazard 检查** | ❌ | ✅ Step A(注入 IScoreboard) |
| **Pipeline latency 注入** | ❌ | ✅ Step B(注入 IPipelineLatencyProvider) |
| **TC timing 注入** | ❌ | ✅ Step C(注入 ITensorCoreTiming) |
| 读 lane 寄存器值 | ✅ `read_register<T>()` | ❌(WarpState 不含) |
| 写 lane 寄存器值 | ✅ `write_register<T>()` | ❌ |
| 读全局内存 | ✅ `read_global_memory<T>()` | ❌ |
| 写全局内存 | ✅ `write_global_memory<T>()` | ❌ |
| 读 lane PC | ✅ `read_thread_pc()` | ❌(WarpState 不含 PC) |
| 推进 lane PC | ✅ `advance_thread_pc()` | ❌ |
| 读 active mask | ✅ `read_active_mask()` | ❌(WarpState 不含) |
| Warp 完成判断 | ✅ `is_warp_finished()` | ❌ |
| Lane 退出判断 | ✅ `is_thread_exited()` | ❌ |
| 镜像 cycle / exec_mask / blocked_cycles | ❌ | ✅ WarpState[warp_id] |
| 镜像 WarpScheduler 状态 | ❌ | ✅ WarpSchedulerStats |
| 创建 Scoreboard 实例 | ✅ `create_scoreboard()`(返回给 timing) | ❌ |
| 创建 Pipeline 实例 | ✅ `create_pipeline_latency_provider()` | ❌ |
| 创建 TC 实例 | ✅ `create_tensor_core_timing()` | ❌ |

---

## 6. 单元测试覆盖(Functional Correctness 验证)

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_ptx_emu_facade_decode.cc` | `[ptx-emu-facade][decode]` | PTXIR 格式校验 + magic/version 异常路径 |
| `test_ptx_emu_facade_arith.cc` | `[ptx-emu-facade][arith]` | ADD/SUB/MUL/DIV 寄存器结果正确性 |
| `test_ptx_emu_facade_memory.cc` | `[ptx-emu-facade][memory]` | LD/ST 共享/全局内存读写正确性 |
| `test_ptx_emu_facade_branch.cc` | `[ptx-emu-facade][branch]` | SIMT 分支/active mask 正确性 |
| `test_ptx_emu_facade_barrier.cc` | `[ptx-emu-facade][barrier]` | `bar.sync` 多 warp 同步正确性 |
| `test_ptx_emu_facade_state.cc` | `[ptx-emu-facade][state]` | register/PC/active mask 读写 round-trip |

**验收标准**(per ADR-SOC-06 G-MVP-3):
- [ ] 6 类 PTX 指令(arith/memory/branch/barrier/io/misc)功能执行结果与 PTX 规范一致
- [ ] 注册/内存/PC/active mask 状态读写 round-trip 无误差
- [ ] PTXIR 格式 magic/version 校验失败抛异常
- [ ] **Functional 调用不增加 cycle 计数**(per `cycle_counter_` 不变验证)— 与 timing model 严格分离

---

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | PTX-EMU 头文件 API 变更(`ptxsim::WarpContext::execute_warp_instruction` 等接口不稳定) | 中 | 高 | submodule pin `PTX-EMU@87820951`(per ADR-SOC-06 §7.5);`abi_guards.h` 17 条静态断言 |
| R2 | PTX-EMU 内部对象生命周期管理混乱(GPUContext/SMContext/WarpContext 谁负责 delete) | 中 | 中 | 本 facade 用 `std::unique_ptr<GPUContext>` RAII;SM/Warp 由 GPUContext 持有 |
| R3 | Functional 误调 timing-only API(如 `exe_once`) | 低 | 高 | 编译期隔离:本 facade 接口**禁止** `exe_once` / `set_blocked_cycles` 等 timing API;通过代码评审保证 |
| R4 | Functional 误读 PC 推到 WarpState(混淆两层职责) | 低 | 中 | 文档与接口表明确分离;测试 `read_thread_pc` 与 `CudaCoreAdapter::WarpState` 是不同来源 |
| R5 | WarpContext::advance_thread_pc 与 ThreadContext 双 PC 不同步 | 低 | 中 | per `warp_context.h:106-110` 注释,只调 `advance_thread_pc`(统一路径) |

---

## 8. 修订历史

| 日期 | 修订 |
|------|------|
| 2026-08-19 | 初版 — per ADR-SOC-06 D3 切片(8 ABI 透传) |
| 2026-08-20 | Phase A 修订(S5 cpptlm_attach_bridge)+ DP4=C 白盒永久禁用 |
| 2026-08-20 | Phase F-H.6 重构:删除 8 ABI 黑盒,改为深度集成接口(本 facade) |
| 2026-08-20 | **Phase I.1 重构(本次)**:严格按 gpgpu-sim functional/timing 分离原则重写。本模块定位为 **PTX 指令功能正确性保证的 facade**,不关心 cycle/stall/调度/hazard 任何 timing 概念。接口严格分类为 Functional Construction / Functional Execute / Functional State / Functional Module Getters 4 类。WarpState 不含 PC(由本模块 read_thread_pc 读取);CudaCoreAdapter 的 WarpState 只含 cycle / exec_mask / blocked_cycles(timing 状态)。`functional_execute_warp` 是本模块唯一核心入口,**不增加 cycle 计数**。CudaCoreAdapter 通过本 facade 转发 functional 调用,自己负责 timing 推进。|

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-20*