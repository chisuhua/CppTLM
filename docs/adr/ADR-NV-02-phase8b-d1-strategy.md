# ADR-NV-02: Phase 8.B D1-Lite 渐进策略——基于 PTX-EMU 真实接口

> **状态**: ⏳ 待审批
> **日期**: 2026-07-03
> **影响**: Phase 8.B 全部 6 模块接口设计 + 与 PTX-EMU 集成路径
> **类别**: NVIDIA GPU 仿真 — PTX-EMU 集成接口对齐
> **来源**: 审查 PTX-EMU 8 个关键头文件 + F12b-LD 设计文档 + Phase 8.B OpenSpec change

---

## 1. 背景

### 1.1 Phase 8.B 原假设

`openspec/changes/2026-06-24-gpu-soc-phase8b-core/` 定义了 6 个核心模块：

| 模块 | 原假设 |
|------|-------|
| WarpSchedulerTLM | standalone round-robin + CGGTY 查表 |
| ScoreboardTLM | standalone 12-entry RAW hazard 检测 |
| PipelineTLM | standalone 5+V 指令管线查表 |
| TensorCoreTLM | standalone 6 precision TC timing |
| L2PartitionTLM | standalone multi-slice 延迟查表 |
| SubCoreTLM | 内部包裹以上 5 模块，对外 black-box |

原 proposal 假定了**手写 JSON workload pattern**（GEMM/FlashAttn/vector_add/stencil/sparse），不与 PTX-EMU 耦合。

### 1.2 F12 集成路径揭示的真实目标

F12a (已完成 `af13e55`..`7ff067e`) + F12b-LD 设计文档定义了明确的 PTX-EMU 集成路线：

```
F12a: standalone GPU core modules (✅ DONE)
  └→ MinimalWarpSchedulerTLM — 接口名与 PTX-EMU WarpScheduler 对齐

F12b-LD: D2 → Memory-timing bridge
  └→ CppTLMBridge + MemoryBridge + libcpptlm_cudart.so
     └→ GLOBAL memory access 经 CppTLM NoC 路由

Phase 8.B: D1 → 替换 PTX-EMU 内部组件
  └→ 预期：WarpSchedulerTLM 替换 PTX-EMU WarpScheduler
```

### 1.3 触发审查的事件

2026-07-03 对 PTX-EMU 仓库 8 个关键头文件进行逐一审查（`ptxsim/warp_scheduler.h`、`ptxsim/sm_context.h`、`ptxsim/gpu_context.h`、`ptxsim/warp_state.h`、`ptxsim/warp_context.h`、`ptx_ir/instruction_latency_table.h`、`ptx_ir/instruction_latency.h`、`memory/hardware_memory_manager.h`），发现原 proposal 假设与实 PTE-EMU API 存在显著偏差。

---

## 2. PTX-EMU 真实 API 约束

### 2.1 WarpScheduler 接口——MinimalWarpSchedulerTLM 名义对齐但实质不兼容

PTX-EMU `WarpScheduler` 是纯虚基类（`include/ptxsim/warp_scheduler.h:27-40`）：

```cpp
class WarpScheduler {
    virtual void add_warp(WarpContext* warp) = 0;         // ❌ CppTLM: uint32_t
    virtual void remove_warp(WarpContext* warp) = 0;      // ❌ CppTLM: uint32_t
    virtual WarpContext* schedule_next() = 0;             // ❌ CppTLM: optional<uint32_t>
    virtual void update_state() = 0;                      // ❌ CppTLM: (uint32_t, bool, uint32_t)
    virtual bool all_warps_finished() const = 0;          // ✅ 一致
    virtual void set_execution_mode(...) = 0;             // ❌ CppTLM: 缺失
    virtual bool schedule_with_migration(...) = 0;        // ❌ CppTLM: 缺失
};
```

`MinimalWarpSchedulerTLM` (`include/tlm/gpu/minimal_warp_scheduler_tlm.hh`) 的注释写"接口名与 PTX-EMU WarpScheduler 对齐"，但 **6 个方法中仅 1 个签名完全一致**。D1 直接注入不可行——必须通过 Adapter 层桥接。

### 2.2 SMContext 注入点——仅 WarpScheduler 可用

```cpp
class SMContext {
    void set_warp_scheduler(std::unique_ptr<WarpScheduler> scheduler);  // ✅ 唯一可注入外部组件
    // ❌ 不存在: set_scoreboard()
    // ❌ 不存在: set_pipeline()
    // ❌ 不存在: set_tensor_core()
    // ❌ 不存在: set_latency_table()
};
```

SMContext **只有一个**可注入的外部组件接口。ScoreboardTLM / PipelineTLM / TensorCoreTLM 无法通过 SMContext 注入。

### 2.3 InstructionLatencyTable——全局单例，不可替换

```cpp
class InstructionLatencyTable {
    static InstructionLatencyTable& instance();  // 全局单例
    InstructionLatency get(StatementType type) const;
    void load(const InstructionLatencyConfig& cfg);  // JSON 覆盖入口
};
```

不是 per-SM 实例。唯一可行的 CppTLM 集成路径是通过 `load()` 注入管线感知延迟值。

### 2.4 PTX-EMU 无独立 Scoreboard 组件

PTX-EMU 使用 `WarpState::threads[lane].blocked_cycles_remaining` 按 warp 隐式管理阻塞周期，无独立 scoreboard。Phase 8.B ScoreboardTLM 将是**全新添加**的组件，需要修改 PTX-EMU 指令发射路径。

---

## 3. 决策

### 3.1 ✅ D1-Lite 定义为 Phase 8.B 目标——非 D1-Full 全栈替换

| 决策 | 结论 | 原因 |
|------|------|------|
| **D1-Lite vs D1-Full** | Phase 8.B 实施 **D1-Lite**（仅 WarpScheduler 替换），D1-Full（全栈 Scheduler+Scoreboard+Pipeline+TC 替换）推迟到 Phase 9+ | SMContext 仅暴 scheduler 注入点；Scoreboard/Pipeline/TC 需 PTX-EMU 团队配合新增注入点 |

### 3.2 ✅ WarpSchedulerTLM 通过 Adapter 层注入——`CppTLMWarpSchedulerAdapter`

```cpp
class CppTLMWarpSchedulerAdapter : public WarpScheduler {
    // 核心: WarpContext* ↔ uint32_t 转换
    WarpSchedulerTLM* tlm_scheduler_;
    std::unordered_map<uint32_t, WarpContext*> warp_map_;
};
```

- Adapter 桥接 `uint32_t`（CppTLM 内部）与 `WarpContext*`（PTX-EMU）
- Adapter 继承 PTX-EMU 纯虚基类 `WarpScheduler`，满足 `SMContext::set_warp_scheduler()` 类型约束
- 新增的 3 个方法（`set_execution_mode` / `get_execution_mode` / `schedule_with_migration`）提供默认实现
- WarpSchedulerTLM 内部保留 `uint32_t` 接口，独立模式下仍可使用合成数据测试

### 3.3 ✅ ScoreboardTLM / PipelineTLM / TensorCoreTLM 作为独立模型——Phase 8.B 不注入 PTX-EMU

| 模块 | 8.B 定位 | 集成方式 | 注入时机 |
|------|---------|---------|:--------:|
| **ScoreboardTLM** | 独立 RAW hazard 检测库 | 暂不注入 PTX-EMU；预留 `ScoreboardObserver` 接口 | Phase 9+ |
| **PipelineTLM** | 独立 5+V 查表模型 | 输出 `InstructionLatencyConfig` → PTX-EMU `InstructionLatencyTable::load()` | F12b-LD |
| **TensorCoreTLM** | 独立 TC timing 模型 | Timing-only，PTX-EMU 负责功能执行 | F12b-LD |

### 3.4 ✅ SubCoreTLM 包裹 SMContext——仅注入 WarpScheduler

```cpp
class SubCoreTLM : public ChStreamModuleBase {
    void set_sm_context(SMContext* sm_ctx);  // D1-Lite: 包裹真实 SMContext
    // Phase 8.B 阶段 A (独立): sm_ctx_ = nullptr, 内部用 standalone 组件
    // F12b-LD 阶段 B (集成): sm_ctx_ 非空, tick() 调用 sm_ctx_->exe_once()
};
```

- Phase 8.B 阶段 A: standalone 合成测试
- F12b-LD 完成后: 注入真实 SMContext + CppTLMWarpSchedulerAdapter

### 3.5 ✅ 验证双轨制

| 层次 | 阶段 A (无 PTX-EMU) | 阶段 B (F12b-LD 后) |
|------|--------------------|---------------------|
| **Level 1** 模块单元 | 合成输入验证 6 模块逻辑 | PTX-EMU 集成 subcase |
| **Level 2** 系统级 | 合成 workload → CppTLM NoC → bandwidth | 真实 CUDA kernel `.cu` → PTX-EMU → MemoryBridge → bandwidth |
| **Level 3** 对照 | vs gpgpu-sim（同 workload 参数） | vs gpgpu-sim（同 `.cu` kernel）+ vs standalone PTX-EMU (±10%) |

### 3.6 ❌ 反模式（明确不做）

- ❌ 强制 D1-Full 全栈替换（PTX-EMU 当前 API 不支持）
- ❌ 修改 PTX-EMU 源代码（Phase 8.B scope 外）
- ❌ Adapter 直接继承 ChStreamModuleBase（不满足 PTX-EMU WarpScheduler 类型约束）
- ❌ 6 模块间产生循环依赖（各自独立可测）

---

## 4. 实施影响

### 4.1 Phase 8.B Task 10 拆分

原 `Task 10: WarpSchedulerTLM` 拆分为：

| 子任务 | 产出 | 依赖 |
|:------:|------|:---:|
| **Task 10a** | `WarpSchedulerTLM`（重命名 MinimalWarpSchedulerTLM + 加 CGGTY + priority；保留 `uint32_t` 接口） | 无 |
| **Task 10b** | `CppTLMWarpSchedulerAdapter : public WarpScheduler`（内含 WarpSchedulerTLM*，转发 PTX-EMU 调用） | Task 10a |

### 4.2 Phase 8.B Task 14 修订

SubCoreTLM 不再"内部包含 4 scheduler + scoreboard + pipeline + TC + black-box pipe"，改为：
- **Phase 8.B 独立模式**: `sm_ctx_ = nullptr`，内部使用独立 `WarpSchedulerTLM` + `ScoreboardTLM` + `PipelineTLM`（合成测试用）
- **F12b-LD 集成模式**: `sm_ctx_` 非空，tick 调用 `sm_ctx_->exe_once()`，注入 `CppTLMWarpSchedulerAdapter`

### 4.3 文档影响

| 文档 | 需要修改 |
|------|---------|
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/design.md` | §2 6 模块设计中 WarpSchedulerTLM 加 Adapter 层，SubCoreTLM 加 SMContext 包裹 |
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/tasks.md` | Task 10 → 10a + 10b |
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/specs/gpu-soc-phase8b.md` | REQ-GPU-8B-2/6 修订 |
| `docs/superpowers/plans/2026-06-24-gpu-soc-phase8b.md` | 同步修订 |

---

## 5. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| **R1** | Adapter 转发开销降低 D1-Lite 性能 | 低 | 低 | `warp_map_` 用 `unordered_map`，查找 O(1)；`uint32_t` ↔ `WarpContext*` 为简单映射，< 0.1% overhead |
| **R2** | PTX-EMU 未来版本接口变更破坏 Adapter | 中 | 中 | `static_assert(CppTLMBRIDGE_VERSION >= V)` 版本检查扩展到 WarpScheduler 签名 |
| **R3** | D1-Lite 仅替换 scheduler、其余仍是 standalone 模型，端到端精度不达 ±15% | 中 | 中 | Level 2 system test 覆盖双轨验证；如精度不足则标记为已知限制 |
| **R4** | PTX-EMU 团队拒绝未来新增 Scoreboard/Pipeline 注入点 | 低 | 高 | D1-Full 可降级为 D1-Lite + observer mode，Scoreboard/Pipeline 做统计观测不替换 |
| **R5** | `CppTLMWarpSchedulerAdapter` 需要 `#include <ptxsim/warp_scheduler.h>`，引入 PTX-EMU 头文件依赖 | 中 | 中 | Adapter 头文件用 `#ifdef HAS_PTXEMU` 条件编译；独立模式不依赖 PTX-EMU headers |

---

## 6. 验收标准

| # | 标准 | 阶段 | 验证 |
|---|------|:---:|------|
| **G-D1** | WarpSchedulerTLM（重命名 Minimal + CGGTY）独立单元测试 pass | A | `cpptlm_tests [gpu][sched]` |
| **G-D2** | CppTLMWarpSchedulerAdapter 满足 PTX-EMU WarpScheduler 全接口 | A | 编译 + 虚函数签名一致性检查 |
| **G-D3** | SubCoreTLM 独立模式 tick() 推进 cycle，sm_ctx_=nullptr | A | `cpptlm_tests [gpu][subcore]` |
| **G-D4** | (F12b-LD 后) SMContext + Adapter 注入，exe_once() 端到端 | B | CUDA kernel 执行，CppTLM counter 观察到 warp 调度统计 |

---

## 7. 参考文献

### PTE-EMU 侧（审查的 8 个关键头文件）

- `include/ptxsim/warp_scheduler.h`（82 行）— WarpScheduler 纯虚基类 + RoundRobinWarpScheduler / GreedyWarpScheduler 实现
- `include/ptxsim/sm_context.h`（193 行）— SMContext = WarpContext + CTAContext 容器 + `set_warp_scheduler()` 注入点
- `include/ptxsim/gpu_context.h`（176 行）— GPUContext = SMContext[] 容器 + KernelLaunchRequest + `exe_once()`
- `include/ptxsim/warp_state.h`（60 行）— WarpState = ThreadState[32] + exec_mask
- `include/ptxsim/warp_context.h`（283 行）— WarpContext = ThreadContext[32] + SIMT stack + `decrement_blocked_cycles()`
- `include/ptx_ir/instruction_latency_table.h`（55 行）— InstructionLatencyTable 全局单例 + `load(InstructionLatencyConfig&)`
- `include/ptx_ir/instruction_latency.h`（21 行）— InstructionLatency {cycles, is_long_delay} 结构体
- `include/memory/hardware_memory_manager.h`（59 行）— HardwareMemoryManager 全局单例 + GLOBAL 空间拦截点（D2 路径）

### CppTLM 侧

- `include/tlm/gpu/minimal_warp_scheduler_tlm.hh`（57 行）— 现有 MinimalWarpSchedulerTLM = `uint32_t` 接口（非 `WarpContext*`）
- `include/tlm/gpu/sub_core_slot.hh`（40 行）— SubCoreSlot helper = `occupy/release/tick`
- `include/tlm/gpu/gpu_compute_unit_tlm.hh`（Phase 8.A 产物，将被 SubCoreTLM 替代）

### 设计文档

- `docs/superpowers/specs/2026-06-30-f12-ptxemu-ldpreload-design.md`（360 行）— CppTLMBridge / MemoryBridge / KernelLaunchTLM 扩展设计
- `docs/superpowers/plans/f12-ptxemu-ldpreload-integration.md`（310 行）— D1/D2/D3 定义 + F12a → F12b-LD → 8.B 路线图
- `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`（273 行）— CppTLM ↔ PTX-EMU 协作同步文档
- `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` — Phase 8.B OpenSpec change（proposal + design + specs + tasks）
- `docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` — 本文档的前置决策：gpu_soc 独立 SoC 目标 + 借鉴 gpgpu-sim 不集成

---

## 8. 修订历史

- **2026-07-03 v1.0 (本文)** — 初版签发
  - 审查 PTX-EMU 8 个关键头文件后识别接口实质性偏差
  - 定义 D1-Lite（仅 WarpScheduler 替换）vs D1-Full（全栈替换）分阶
  - 决策 Adapter 层桥接方案（`CppTLMWarpSchedulerAdapter`）
  - Scoreboard/Pipeline/TensorCore 暂作独立模型
  - SubCoreTLM 改为包裹 SMContext + 注入 WarpScheduler

---

**关联 OpenSpec 变更**：
- `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` — 🟢 本 ADR 直接影响其 design.md + tasks.md 修订
- `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/` — 🔄 依赖 8.B M2 验收

**维护**: CppTLM 开发团队
**下次 review**: Phase 8.B design.md + tasks.md 修订完成后