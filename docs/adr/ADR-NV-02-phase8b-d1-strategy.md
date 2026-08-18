# ADR-NV-02: Phase 8.B D1-Lite 渐进策略——基于 PTX-EMU 真实接口

> **状态**: ✅ 已确认（D1-Lite → D1-Full 升级，见 Status Update）
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

> **2026-07-14 Status Update**: §3.2 决策**不变**，仅扩展到 4 个 Adapter（追加 Scoreboard/Pipeline/TensorCore），WarpScheduler Adapter 方案继续有效。

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

| 层次 | 阶段 A (无 PTX-EMU / D1-Lite 范围) | 阶段 B (F12b-LD 后 / D1-Full 范围) |
|------|-----------------------------------|-----------------------------------|
| **Level 1** 模块单元 | 合成输入验证 6 模块逻辑 + 4 个 Adapter mock 接口转发（`cpptlm_tests [gpu]`） | PTX-EMU 集成 subcase：4 个 Adapter 真实接口注入，enum 一致性 `static_assert` 通过 |
| **Level 2** 系统级 | 合成 workload → CppTLM NoC → bandwidth（独立模式 6 模块协同） | 真实 CUDA kernel `.cu` → PTX-EMU → exe_once() 三步注入全开（scoreboard + pipeline + TC） → MemoryBridge → bandwidth |
| **Level 3** 对照 | vs gpgpu-sim（同 workload 参数） | vs gpgpu-sim（同 `.cu` kernel）+ vs standalone PTX-EMU (±10%)——4 组件全注入下精度预期**优于** D1-Lite |

### 3.6 ❌ 反模式（明确不做）

- ❌ Adapter 直接继承 ChStreamModuleBase（不满足 PTX-EMU WarpScheduler 类型约束）
- ❌ 6 模块间产生循环依赖（各自独立可测）
- ❌ 跨仓库枚举值硬编码（必须通过 `static_assert` 编译期验证）
- ❌ Adapter 持有 PTX-EMU 资源的所有权（只持原始指针，PTX-EMU 负责生命周期）

> **2026-07-14 Status Update**: §3.6 原第一条 "❌ 强制 D1-Full" 已**移除**——PTX-EMU 团队已确认可配合改造（见 §5 R4 风险消除）。PTX-EMU 源代码修改**仅限于**改造任务书 8 项（~2.5 天），仍属 Phase 8.B scope。

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

### 5.1 原 D1-Lite 风险（部分已重新评估）

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| **R1** | 4 个 Adapter 的转发开销（从 1 个 Adapter 扩展到 4 个） | 低 | 低 | 每个 Adapter 用 `unordered_map`/直接指针，O(1)；4 个 Adapter 总开销 < 0.2% overhead |
| **R2** | PTX-EMU 未来版本接口变更破坏 4 个 Adapter | 中 | 中 | `static_assert` 扩展到 4 个 PTX-EMU 纯虚接口的全签名验证；Adapter 编译期失败即阻断 |
| **R3** | 4 组件全注入后精度仍不达 ±15%（极端 corner case） | 低 | 中 | D1-Full 注入 4 组件，精度预期**优于** D1-Lite（仅 scheduler）；如仍不达标，将精度偏差定位到具体组件（scoreboard/pipeline/TC）做局部降级 |
| **R4** | ~~PTX-EMU 团队拒绝新增 Scoreboard/Pipeline 注入点~~ | — | — | **已消除**（2026-07-14）：PTX-EMU 已出综合任务书 `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`（#1~#10 + #C1~#C5，共 15 项任务，~9 天） |
| **R5** | Adapter 引入 PTX-EMU 头文件依赖 | 中 | 中 | Adapter 头文件用 `#ifdef HAS_PTXEMU` 条件编译；独立模式不依赖 PTX-EMU headers；4 个 Adapter 全部独立编译 |

### 5.2 D1-Full 新增风险（2026-07-14 Status Update）

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| **R6** | PTX-EMU 团队 8 项改造任务（~2.5 天）延迟 → 阻塞 Phase 8.B Task 15/16 | 中 | 高 | **Adapter 解耦策略**：CppTLM 侧 4 个 Adapter 先用 mock PTX-EMU 接口开发和测试；PTX-EMU 真实接口就绪后仅替换 Adapter 实现层，Phase 8.B 主线不受阻塞 |
| **R7** | 4 个 Adapter 的跨仓库枚举值一致性（PipelineId 6 个 + TcPrecision 6 个 = 12 个端点）漂移 | 中 | 中 | (1) CppTLM CI 加入完整 12 个端点的 `static_assert`；(2) PTX-EMU 侧 CI 同步加入一致性检查；(3) ADR 引入**版本断言** `static_assert(CppTLMBRIDGE_VERSION >= V)` |
| **R8** | `exe_once()` 中 `goto skip_warp_execution` 非常规控制流 → scoreboard stall + warp 重新调度组合场景下可能引发 PTX-EMU 内部状态不一致 | 低 | 高 | (1) PTX-EMU 改造任务 #6 测试强制覆盖 `stall → re-schedule → release → re-issue` 完整循环；(2) CppTLM 集成测试 Task 16 覆盖相同路径；(3) 改造后端到端回归跑全部 PTX-EMU 原有测试，零退化 |
| **R9** | F12b-LD MemoryBridge 路径静默数据损坏（`submit_kernel` deep-copy `kernel_args` 遗漏、`global_access` 地址映射错误） | 低 | 高 | (1) G-F0 `vector_add` 输出逐元素比对 baseline，非仅验证带宽；(2) F12b-LD 完成后跑 GEMM 单 kernel 数值 diff；(3) CppTLM CI 加入 `cpptlm_tests [gpu][f12b]` 含端到端数值断言 |

> **注 — 原始 Integration Plan §10 的 R7/R8 继承关系**: 
> - 原始 **R7**（静默数据损坏）→ 本文 **R9**：原风险在 D1-Full 上下文中仍然成立。D1-Full Compute 阶段不引入新的静默数据路径，但 F12b-LD MemoryBridge 阶段未完成验证前此风险为开。
> - 原始 **R8**（无 baseline → 性能幻觉）→ 已由 ADR §6.2 G-D5（`blocked_cycles ≤ 1 cycle`，CppTLM 独立模型）和综合计划 G-D3/gpgpu-sim ±15% 双轨校准**间接解决**。MemoryBridge NoC 延迟查询的独立精度验证待 Task #C2 完成后补充。

---

## 6. 验收标准

### 6.1 原 D1-Lite 验收标准（保留）

| # | 标准 | 阶段 | 验证 |
|---|------|:---:|------|
| **G-D1** | WarpSchedulerTLM（重命名 Minimal + CGGTY）独立单元测试 pass | A | `cpptlm_tests [gpu][sched]` |
| **G-D2** | CppTLMWarpSchedulerAdapter 满足 PTX-EMU WarpScheduler 全接口 | A | 编译 + 虚函数签名一致性检查 |
| **G-D3** | SubCoreTLM 独立模式 tick() 推进 cycle，sm_ctx_=nullptr | A | `cpptlm_tests [gpu][subcore]` |
| **G-D4** | (F12b-LD 后) SMContext + Adapter 注入，exe_once() 端到端 | B | CUDA kernel 执行，CppTLM counter 观察到 warp 调度统计 |

### 6.2 D1-Full 新增验收标准（2026-07-14 Status Update）

| # | 标准 | 阶段 | 验证 |
|---|------|:---:|------|
| **G-D5** | Scoreboard / Pipeline / TensorCore 三 Adapter 注入 PTX-EMU 后，`SMContext::exe_once()` 的 `blocked_cycles_remaining` 值与同条件下 CppTLM 独立模型计算值差异 ≤ 1 cycle | B | 5 类 microbenchmark（GEMM/FlashAttn/vector_add/stencil/sparse SpMV）每类对比注入前后 blocked_cycles 序列；最大差值 ≤ 1 cycle |
| **G-D6** | PTX-EMU 改造后、未注入任何 Adapter（4 个 setter 全部为 nullptr）时，原始 PTX-EMU 测试通过率 100%，行为与改造前字节级一致 | A | PTX-EMU 全量回归测试套件（`ptxemu_tests`）全部 pass；`diff` 对比注入前/后 `InstructionLatencyTable::instance().get(stmt.type).cycles` 在所有指令类型上完全相等 |
| **G-D7** | 12 个端点（`PipelineId` 6 + `TcPrecision` 6）双向 `static_assert` 通过；CI 中任一端点漂移即编译失败 | A+B | CppTLM CI + PTX-EMU CI 双重断言；12/12 全部通过 |
| **G-D8** | `exe_once()` 中 scoreboard stall → re-schedule → release → re-issue 完整循环无状态不一致 | B | PTX-EMU 改造任务 #6 测试 + CppTLM Task 16 集成测试联合覆盖该路径；通过混沌测试（chaos test）注入随机 stall 序列，验证 warp 状态机不变量 |

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

- `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`（920 行）— **综合任务书**（F12b-LD + D1-Full + Phase 9+ Async Seam）
- `docs/superpowers/specs/PTX-EMU-README.md`（318 行）— **PTX-EMU 团队入口文档**
- `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`（332 行）— CppTLM ↔ PTX-EMU 协作规范
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

## Status Update

### 2026-07-14: D1-Lite → D1-Full 策略升级

**触发事件**：

1. **PTX-EMU 团队确认可配合**修改 SMContext 内部执行路径（`exe_once()`），为 Scoreboard / Pipeline / TensorCore 暴露注入点
2. 对 PTX-EMU 最新代码完成逐行审查（`sm_context.cpp:191-401`、`warp_context.h:278`、`thread_state.h:40`），确认 `exe_once()` 内部存在明确的三段式注入窗口：
   - **(A)** scheduler 调度后 → 指令执行前：可插入 Scoreboard hazard 检测
   - **(B)** 指令执行前 → 延迟查询点：可替换 `InstructionLatencyTable` 查询
   - **(C)** 指令执行后 → `update_state()` 前：可插入 Scoreboard 释放
3. PTX-EMU 仅需新增 2 个 API（`WarpContext::set_blocked_cycles_for_active()` + 目标寄存器提取），其余注入逻辑均通过纯虚接口解耦

**受影响的 ADR 条款变更**：

| 原条款 | 原决策 | 变更后 |
|--------|--------|--------|
| §3.1 D1-Lite vs D1-Full | Phase 8.B 实施 D1-Lite | **升级为 D1-Full**：WarpScheduler + Scoreboard + Pipeline + TensorCore 全部通过 SMContext 注入 |
| §3.3 Scoreboard/Pipeline/TC 独立 | 不注入 PTX-EMU | **纳入 8.B 注入 scope**：通过 3 个纯虚接口 + Adapter 层桥接 |
| §3.4 SubCoreTLM | 仅注入 WarpScheduler | 内部通过 `SMContext::set_scoreboard/set_pipeline_latency_provider/set_tensor_core_timing` 注入全部 4 组件 |
| §3.6 反模式 | "❌ 强制 D1-Full" | **移除**：PTX-EMU 团队已确认可配合 |
| §4.1 Task 10 拆分 | 仅 Task 10a/10b | 扩展为 Task 10a/10b + **Task 9**: ScoreboardTLM（实现 `IScoreboard`）+ **Task 11**: PipelineTLM（实现 `IPipelineLatencyProvider`）+ **Task 12**: TensorCoreTLM（实现 `ITensorCoreTiming`）+ **Task 15**: 4 个 Adapter 层 |
| §4.2 Task 14 | SubCoreTLM 独立模式 | SubCoreTLM 改为双模式：独立 + D1-Full 注入模式（通过 4 个 setter 配置） |
| §5 R4 | "PTX-EMU 团队拒绝未来新增注入点" | **风险消除**：PTX-EMU 已出改造任务书（§5 #0~#6） |
| §6 验收标准 G-D1~G-D4 | 仅 scheduler 验收 | 追加 **G-D5**: scoreboard/pipeline/TC 注入点集成测试 + **G-D6**: PTX-EMU `exe_once()` 改造后向后兼容测试 |

**PTX-EMU 侧改造任务书**：

- **`docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`** — **新版综合任务书（2026-07-14 Supersedes 旧版）**
  - **§2 F12b-LD MemoryBridge**（P0 阶段，~3 天）— 建立 CppTLM clock-of-truth 基础设施
  - **§3 D1-Full Compute 注入**（P1 阶段，~2.5 天）— Scoreboard/Pipeline/TensorCore 注入
  - **§4 Phase 9+ Async Seam**（P2 阶段，1 小时）— IAsyncCompletion 接口预留
  - **§5 集成验证**（P3 阶段，1 周）— E2E CUDA kernel + gpgpu-sim ±15%
- `docs/superpowers/specs/2026-07-03-ptxemu-modification-task.md`（**2026-07-14 已删除**，仅作历史记录）
- 3 个纯虚接口：`IScoreboard` / `IPipelineLatencyProvider` / `ITensorCoreTiming`（零 CppTLM 依赖）
- SMContext 新增 4 个 setter/4 个 getter（含 Phase 9+ `IAsyncCompletion` stub）+ exe_once() 四步注入

**关联文档同步**：

| 文档 | 状态 |
|------|:---:|
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/design.md` | ✅ 已修订 D1-Full |
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/tasks.md` | ✅ 已修订 D1-Full |
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/specs/gpu-soc-phase8b.md` | ✅ 已修订 D1-Full |
| `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md` | ✅ D1-Full 协同计划 |
| `docs/superpowers/plans/2026-06-24-gpu-soc-phase8b.md` | ✅ 已修订 D1-Full（含 Task 15a Adapter 层 + 双模式 SubCoreTLM） |
| `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md` | ✅ 已追加 §13 D1-Full 协作节 |
| `docs/adr/README.md`（ADR 索引） | ✅ 已同步：D1-Full + 状态 ✅ |
| `docs/superpowers/specs/2026-07-03-ptxemu-modification-task.md` | 🗑️ 2026-07-14 已删除（被 comprehensive-plan.md §3 子集吸收） |

**与 ADR-NV-01 的关系**：`docs/adr/ADR-NV-01-gpu-soc-architecture-target.md` §2.1 D8 原文"5+V pipeline 不分开建模"——该决策针对 Phase 8.A 时序。Phase 8.B 的 `PipelineTLM`（5+V 抽象）+ `TensorCoreTLM`（6 精度）分开建模是对 D8 的预期演进，与 Phase 8.A 决策不冲突。后续如需正式变更 D8，应在 ADR-NV-01 追加 Status Update 段（独立于本 ADR 处理）。

## Status Update

### 2026-08-18 — D1-Full 路径废止 (per ADR-X.15 v3.0 dGPU 板卡决策)

> **触发事件**: `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/` (commit `d867a5f`) 采纳 PTX-EMU 完整代理模式 — Oracle 二次审查 F-NEW-CONCERN-1/3 揭示 CppTLM 端 SM 模块仅为 timing reference stubs,非可执行 SM。

- **废止范围**: §3.2-3.6 全部 4 个 Adapter (CppTLMWarpSchedulerAdapter / CppTLMScoreboardAdapter / CppTLMPipelineAdapter / CppTLMTensorCoreAdapter) **不实施**
- **保留范围**:
  - `ScoreboardTLM` / `PipelineTLM` / `TensorCoreTLM` 作为独立 timing reference 模块(无 PTX-EMU 注入调用方)
  - 18 个 GPU 单元测试文件保留作为 legacy 覆盖(新增 `[legacy]` Catch2 标签)
- **G-D5 验收**: 因 Mode B (PTX-EMU 代理) 不注入 Adapter,本验收标准已无对应测试场景;从 `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/proposal.md` Gate #6 移除
- **关联文档**:
  - `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/tasks.md` T-P3-5
  - `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/design.md` §1.2
  - ADR-X.15 §3 + Oracle review `ses_febd64a69ffea61HXB7oilue7H`
- **决策追溯**: 本 ADR §2.4 "PTX-EMU 无独立 Scoreboard 组件" 原文即为本次决策依据 — PTX-EMU 已是自包含 SM,CppTLM 不必重复实现

---

**关联 OpenSpec 变更**：
- `openspec/changes/2026-06-24-gpu-soc-phase8b-core/` — 🟢 本 ADR 直接影响其 design.md + tasks.md 修订
- `openspec/changes/2026-06-24-gpu-soc-phase8c-advanced/` — 🔄 依赖 8.B M2 验收

**维护**: CppTLM 开发团队
**下次 review**: Phase 8.B design.md + tasks.md 修订完成后