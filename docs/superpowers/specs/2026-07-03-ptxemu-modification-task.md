# PTX-EMU 改造任务书：Phase 8.B D1-Full 协同注入点

> **文档类型**: PTX-EMU 团队改造任务书
> **日期**: 2026-07-03
> **作者**: CppTLM Team
> **依赖**: PTX-EMU 团队确认可配合修改
> **关联**: `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`（完整双方协同计划）

---

## 1. 背景

### 1.1 我们在做什么

CppTLM 正在实施 Phase 8.B——GPU sub-core 微架构仿真。目标是用 CppTLM 的 WarpScheduler / Scoreboard / Pipeline / TensorCore 组件**注入到 PTX-EMU**，替代 PTX-EMU 当前的内置实现，实现统一的 CppTLM 驱动的 timing 模型。

换句话说：用户的 CUDA 程序仍通过 `LD_PRELOAD libcudart.so` 运行，PTX-EMU 仍负责 PTX 指令功能执行，但**指令调度、scoreboard hazard 检测、管线延迟、TC timing 全部由 CppTLM 组件决定**。

### 1.2 为什么需要 PTX-EMU 配合

审查 PTX-EMU 代码后发现，当前 `SMContext` 只暴露了**一个**外部注入点：

```cpp
void SMContext::set_warp_scheduler(std::unique_ptr<WarpScheduler> scheduler);  // ✅ 已有
```

但 Scoreboard、Pipeline 延迟、TensorCore timing 的注入点**不存在**。这意味着：

| 组件 | CppTLM 能否注入？ | 现状 |
|------|:---:|------|
| WarpScheduler | ✅ | `SMContext::set_warp_scheduler()` 已存在 |
| Scoreboard | ❌ | PTX-EMU 无独立 scoreboard 类，用 `blocked_cycles_remaining` 隐式管理 |
| Pipeline 延迟 | ❌ | `InstructionLatencyTable` 是全局单例，无法 per-SM 替换 |
| TensorCore timing | ❌ | TC 延迟硬编码或来自 JSON config，无外部注入接口 |

### 1.3 这次改造要达成什么

PTX-EMU 团队只需做一件事：**在 `SMContext` 上新增 3 个注入点 + 3 个接口定义**。CppTLM 负责所有注入实现。双方通过 **统一的纯虚接口** 解耦——PTX-EMU 不需要知道 CppTLM 的任何内部类型。

---

## 2. 设计依据

### 2.1 为什么用纯虚接口而不是直接 include CppTLM 头文件

```
❌ 方案 A: CppTLM 类型直接注入
   SMContext::set_scoreboard(CppTLM::ScoreboardTLM*)
   → PTX-EMU 必须依赖 CppTLM 头文件 → 循环依赖, 无法独立编译

✅ 方案 B: 纯虚接口 + Adapter
   PTX-EMU 定义 IScoreboard (纯虚基类, 零外部依赖)
   CppTLM 提供 ScoreboardAdapter : IScoreboard (桥接 CppTLM ScoreboardTLM)
   → PTX-EMU 零 CppTLM 依赖, 各自独立编译
```

选择方案 B 的额外理由：
- PTX-EMU 的 Mock 测试可以完全独立编写
- CppTLM Phase 8.B 模块也可以完全独立测试（不需要 PTX-EMU 环境）
- 接口变更只影响 Adapter 层，不影响核心模块

### 2.2 为什么注入点放在 SMContext::exe_once() 内部而不是外部

```
❌ 外部驱动模式:
   while (!done) {
       sm_ctx->tick_scheduler();       // CppTLM 调用
       sm_ctx->check_scoreboard();     // CppTLM 调用
       sm_ctx->get_pipeline_latency(); // CppTLM 调用
       sm_ctx->execute_instruction();
   }
   → PTX-EMU 需要大量重构, 破坏现有封装

✅ 内部注入模式:
   SMContext::exe_once() {
       warp = scheduler_->schedule_next();     // ← 已可注入
       if (scoreboard_)                        // ← 新增
           check_hazard();
       if (pipeline_provider_)                 // ← 新增
           latency = provider->get_cycles();
       warp->execute_instruction();
       if (scoreboard_)
           release();
   }
   → 最小侵入, exe_once() 语义不变
```

### 2.3 为什么 Scoreboard 做成独立接口而不是扩展 WarpScheduler

WarpScheduler 的职责是**选 warp**，Scoreboard 的职责是**检测 hazard 并 stall**。两者是正交的：
- Scheduler 回答"下一个该跑哪个 warp"
- Scoreboard 回答"这个 warp 能不能发射这条指令"

把它们合并会破坏单一职责，且 PTX-EMU 现有 `WarpScheduler` 基类已经稳定——不应为了加 scoreboard 而改它的接口。

### 2.4 为什么 Pipeline 延迟接口用 double 而非 uint32_t

SM_120 paper 的 microbenchmark 数据显示很多指令的延迟不是整数 cycle：

| 指令 | 延迟 (cycles) |
|------|:---:|
| IADD3 | 2.22 |
| FFMA | 4.22 |
| DFMA | 64.13 |
| MUFU.RCP | 44.28 |

用 `uint32_t` 会丢失精度，累积误差会导致仿真结果偏离 gpgpu-sim baseline。CppTLM 的 PipelineTLM 模型核心价值就是**分数 cycle 精度**。

---

## 3. 改造内容（3 步）

### 第 1 步：新增 3 个纯虚接口头文件

每个接口文件零外部依赖（只依赖 `<cstdint>` 和 `<string>`），放在 `include/ptxsim/` 下。

#### 3.1 `include/ptxsim/scoreboard_interface.h`（新增）

```cpp
#ifndef PTXSIM_SCOREBOARD_INTERFACE_H
#define PTXSIM_SCOREBOARD_INTERFACE_H

#include <cstdint>

class IScoreboard {
public:
    virtual ~IScoreboard() = default;

    /// 检查是否有空闲 entry（false = stall 当前 warp）
    virtual bool has_free_entry() const = 0;

    /// 为 warp 的指令分配一个 scoreboard entry（跟踪目标寄存器）
    /// @return true = 分配成功, false = 无空闲 (RAW hazard, stall warp)
    virtual bool allocate(uint32_t reg_id, uint32_t warp_id) = 0;

    /// 指令执行完后释放 entry
    virtual bool release(uint32_t reg_id, uint32_t warp_id) = 0;

    /// 每 tick 推进内部计数器
    virtual void tick() = 0;
};

#endif
```

**设计说明**:
- `reg_id` 是目标寄存器编号（`uint32_t`），不绑定 PTX-EMU 的寄存器类型枚举
- `warp_id` 由 PTX-EMU 传入（`WarpContext::get_physical_warp_id()`），CppTLM Adapter 用来映射回内部 warp
- `has_free_entry()` 在 issue 前调用——比 allocate 更轻量，先检查再分配

#### 3.2 `include/ptxsim/pipeline_interface.h`（新增）

```cpp
#ifndef PTXSIM_PIPELINE_INTERFACE_H
#define PTXSIM_PIPELINE_INTERFACE_H

#include <cstdint>
#include <string>

/// 管线 ID 枚举 —— 值必须与 CppTLM tlm::PipelineId 保持一致
enum class PipelineId : uint32_t {
    P0_INT_FP32 = 0,
    V_SIMD     = 1,
    P1_FP64    = 2,
    P2_SFU     = 3,
    P3_LSU     = 4,
    P4_TC      = 5
};

class IPipelineLatencyProvider {
public:
    virtual ~IPipelineLatencyProvider() = default;

    /// 按指令名 + 管线 ID 查询分数 cycle 延迟
    /// @return 分数 cycle 延迟 (如 4.22), 返回 0.0 表示查表未命中
    virtual double get_fractional_cycles(
        const std::string& instruction,
        PipelineId pipe_id) const = 0;

    /// 按 PTX StatementType 查询（用于 exe_once 内集成）
    /// @param statement_type 来自 ptx_ir/ptx_types.h 的 StatementType 枚举值
    /// @return 分数 cycle 延迟, 返回 0.0 表示未命中 → 退回 InstructionLatencyTable
    virtual double get_fractional_cycles_by_type(
        int statement_type,
        PipelineId pipe_id) const = 0;
};

#endif
```

**设计说明**:
- `PipelineId` 枚举值**必须**与 CppTLM 的 `tlm::PipelineId` 保持 0-5 一致（双方通过 Adapter 静态转换）
- `get_fractional_cycles_by_type()` 接收 `int` 而非 `StatementType`——避免接口文件依赖 `ptx_ir/ptx_types.h`，保持零外部依赖
- 返回类型 `double` 支持分数 cycle（如 FFMA 4.22），PTX-EMU 内部取 `ceil()` 或累加分数部分

#### 3.3 `include/ptxsim/tensor_core_interface.h`（新增）

```cpp
#ifndef PTXSIM_TENSOR_CORE_INTERFACE_H
#define PTXSIM_TENSOR_CORE_INTERFACE_H

#include <cstdint>

/// TC 精度枚举 —— 值必须与 CppTLM tlm::TcPrecision 保持一致
enum class TcPrecision : uint32_t {
    FP4  = 0,
    FP6  = 1,
    FP8  = 2,
    FP16 = 3,
    BF16 = 4,
    TF32 = 5
};

class ITensorCoreTiming {
public:
    virtual ~ITensorCoreTiming() = default;

    /// 指定精度的 TC 延迟（cycles）
    virtual uint32_t get_latency(TcPrecision prec) const = 0;

    /// 指定精度的 TC 吞吐（cycles per instruction）
    virtual uint32_t get_throughput_cycles(TcPrecision prec) const = 0;

    /// 按矩阵维度查询延迟（预留接口，Phase 8.B 暂不实现 M/N/K 敏感延迟）
    virtual uint32_t get_latency_mnk(
        TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const {
        (void)M; (void)N; (void)K;
        return get_latency(prec);  // 默认退化到精度统一延迟
    }
};

#endif
```

**设计说明**:
- 首期 6 精度（FP4/FP6/FP8/FP16/BF16/TF32），所有精度统一返回 29 cyc 延迟（SM_120 paper 发现 12 种非 FP64 精度共享同一管线）
- `get_latency_mnk()` 预留矩阵维度参数，Phase 9+ 可实现 M/N/K 敏感延迟
- `TcPrecision` 枚举值**必须**与 CppTLM `tlm::TcPrecision` 保持一致

---

### 第 2 步：修改 SMContext 头文件

#### 修改文件: `include/ptxsim/sm_context.h`

在文件顶部 `#include` 区追加：

```cpp
#include "ptxsim/scoreboard_interface.h"
#include "ptxsim/pipeline_interface.h"
#include "ptxsim/tensor_core_interface.h"
```

在 `SMContext` 类的 public 区追加 3 个 setter：

```cpp
class SMContext {
public:
    // ... 现有方法 ...

    // === 新增: CppTLM 注入点 ===

    /// 注入外部 Scoreboard。nullptr = 不检查 hazard（默认行为）
    /// 所有权不转移——调用方负责 IScoreboard 生命周期
    void set_scoreboard(IScoreboard* scoreboard) { scoreboard_ = scoreboard; }

    /// 注入外部管线延迟提供者。nullptr = 使用内置 InstructionLatencyTable
    void set_pipeline_latency_provider(IPipelineLatencyProvider* provider) {
        pipeline_provider_ = provider;
    }

    /// 注入外部 TC timing。nullptr = 使用内置延迟
    void set_tensor_core_timing(ITensorCoreTiming* tc) {
        tensor_core_timing_ = tc;
    }

    // === 新增: 查询方法（便于调试） ===
    IScoreboard*             get_scoreboard()              const { return scoreboard_; }
    IPipelineLatencyProvider* get_pipeline_latency_provider() const { return pipeline_provider_; }
    ITensorCoreTiming*       get_tensor_core_timing()      const { return tensor_core_timing_; }

private:
    // ... 现有私有成员 ...

    // === 新增私有成员 ===
    IScoreboard*              scoreboard_           = nullptr;
    IPipelineLatencyProvider* pipeline_provider_    = nullptr;
    ITensorCoreTiming*        tensor_core_timing_   = nullptr;
};
```

**设计说明**:
- setter 使用裸指针而非 `unique_ptr`——所有权归 CppTLM（通过 `libcpptlm_cudart.so` 持有），PTX-EMU 不负责释放
- `nullptr` 语义 = "禁用注入，回退到 PTX-EMU 内置行为"，保证向后兼容
- 也暴露 getter，方便测试和调试

---

### 第 3 步：修改 SMContext::exe_once() 实现

#### 修改文件: `src/ptxsim/core/sm_context.cpp`

**当前 `exe_once()` 的简化流程：**

```
1. 从 warp_scheduler 调度下一个 warp
2. 获取 warp 要执行的下一条指令 (StatementContext)
3. 调用 warp->execute_warp_instruction(stmt)
4. update_state()
```

**改造后的流程（+3 处注入，用 `// === NEW ===` 标注）：**

```
1. 从 warp_scheduler 调度下一个 warp
   ↓
2. 获取 warp 要执行的下一条指令 (StatementContext) + 目标寄存器列表
   ↓
   // === NEW: Scoreboard 检查 ===
   if (scoreboard_) {
       scoreboard_->tick();  // 推进 scoreboard 内部计数器

       if (!scoreboard_->has_free_entry()) {
           // scoreboard 满 → stall 当前 warp，跳到下一个 warp
           continue;  // 或标记 warp 为 blocked
       }

       // 对每个目标寄存器做 RAW hazard 检测
       for (auto& dest_reg : stmt.dest_registers()) {
           if (!scoreboard_->allocate(dest_reg, warp->get_physical_warp_id())) {
               // RAW hazard → stall
               goto stall_warp;
           }
       }
   }
   ↓
   // === NEW: Pipeline 延迟查询 ===
   uint32_t instr_latency = 0;
   if (pipeline_provider_) {
       double fractional = pipeline_provider_->get_fractional_cycles_by_type(
           static_cast<int>(stmt.type),   // StatementType → int
           map_instruction_to_pipeline(stmt)  // 指令 → 管线映射
       );
       if (fractional > 0.0) {
           instr_latency = static_cast<uint32_t>(std::ceil(fractional));
       }
       // 如果 pipeline_provider_ 返回 0.0（未命中），退回到 InstructionLatencyTable
   }
   if (instr_latency == 0) {
       // === NEW: TensorCore 延迟查询 ===
       if (tensor_core_timing_ && is_tensor_core_instruction(stmt)) {
           TcPrecision prec = map_instruction_to_tc_precision(stmt);
           instr_latency = tensor_core_timing_->get_latency(prec);
       }
   }
   if (instr_latency == 0) {
       // 所有提供者都未命中 → 退回内置 InstructionLatencyTable
       auto latency_entry = InstructionLatencyTable::instance().get(stmt.type);
       instr_latency = latency_entry.cycles;
   }

   // 将延迟注入 warp（blocked_cycles_remaining）
   warp->set_blocked_cycles(instr_latency);
   ↓
3. 调用 warp->execute_warp_instruction(stmt)  // 功能执行不变
   ↓
   // === NEW: Scoreboard 释放 ===
   if (scoreboard_) {
       for (auto& dest_reg : stmt.dest_registers()) {
           scoreboard_->release(dest_reg, warp->get_physical_warp_id());
       }
   }
   ↓
4. update_state()
```

**关键约束**：

1. **`nullptr` 回退**：如果 scoreboard_/pipeline_provider_/tensor_core_timing_ 为 nullptr，行为与原 exe_once() 完全一致——不做任何额外操作。

2. **Pipeline 优先级高于 TensorCore**：pipeline_provider_ 先查询——如果是 TC 指令且 pipeline_provider_ 不支持，返回 0.0，然后走 tensor_core_timing_ 分支。这允许 PipelineTLM 完整接管所有指令延迟（包括 TC），或者只接管非 TC 指令而让 TC 走独立路径。

3. **Pipeline 优先于 InstructionLatencyTable**：如果 pipeline_provider_ 返回 >0 的值，**直接使用该值**，不再查询 InstructionLatencyTable。这是"替换"语义，不是"叠加"。

4. **延迟取 ceil**：`double` → `uint32_t` 用 `std::ceil()`。PTX-EMU 内部 `blocked_cycles_remaining` 是整数，分数部分向上取整。CppTLM PipelineTLM 累积分数部分，确保长期仿真精度不丢失。

5. **辅助函数需要 PTX-EMU 实现**：
   - `map_instruction_to_pipeline(StatementContext&) → PipelineId` —— 根据指令类型/opcode 映射到管线
   - `is_tensor_core_instruction(StatementContext&) → bool` —— 判断是否 TC 指令
   - `map_instruction_to_tc_precision(StatementContext&) → TcPrecision` —— 提取 TC 指令的精度
   这三个函数实现在 `sm_context.cpp` 内部或提取到独立的 `pipeline_mapping.cpp`。

---

## 4. 改造清单汇总

| 编号 | 动作 | 文件 | 内容 | 预估工时 |
|:----:|:---:|------|------|:---:|
| **#1** | 新增 | `include/ptxsim/scoreboard_interface.h` | `IScoreboard` 纯虚基类 (4 方法) | 15 min |
| **#2** | 新增 | `include/ptxsim/pipeline_interface.h` | `PipelineId` 枚举 + `IPipelineLatencyProvider` 纯虚基类 (2 方法) | 15 min |
| **#3** | 新增 | `include/ptxsim/tensor_core_interface.h` | `TcPrecision` 枚举 + `ITensorCoreTiming` 纯虚基类 (3 方法) | 15 min |
| **#4** | 修改 | `include/ptxsim/sm_context.h` | `#include` 3 接口 + 3 setter + 3 getter + 3 私有成员 | 15 min |
| **#5** | 修改 | `src/ptxsim/core/sm_context.cpp` | `exe_once()` 内三步注入（scoreboard 检查 → pipeline/TC 延迟查询 → scoreboard 释放）+ 3 个辅助映射函数 | 1.5 day |
| **#6** | 新增 | `test/test_scoreboard_injection.cpp` | Mock 测试: scoreboard + pipeline + TC + nullptr 回退 | 0.5 day |
| | | | **合计** | **~2.5 天** |

---

## 5. 测试要求

### 5.1 Mock 实现（PTX-EMU 侧，仅用于验证注入点）

```cpp
// test/test_scoreboard_injection.cpp

// Mock Scoreboard: 限制 12 entries
class MockScoreboardLimited : public IScoreboard {
    uint32_t max_entries_;
    uint32_t used_ = 0;
public:
    explicit MockScoreboardLimited(uint32_t max) : max_entries_(max) {}
    bool has_free_entry() const override { return used_ < max_entries_; }
    bool allocate(uint32_t, uint32_t) override {
        if (used_ >= max_entries_) return false;
        ++used_; return true;
    }
    bool release(uint32_t, uint32_t) override { if (used_ > 0) --used_; return true; }
    void tick() override {}
};

// Mock Pipeline: 固定延迟
class MockPipelineFixed : public IPipelineLatencyProvider {
    double latency_;
public:
    explicit MockPipelineFixed(double cyc) : latency_(cyc) {}
    double get_fractional_cycles(const std::string&, PipelineId) const override {
        return latency_;
    }
    double get_fractional_cycles_by_type(int, PipelineId) const override {
        return latency_;
    }
};
```

### 5.2 最小测试用例

```cpp
TEST_CASE("SMContext: nullptr injection = no-op (backward compat)") {
    SMContext sm(64, 2048, 64*1024, 0);
    REQUIRE(sm.get_scoreboard() == nullptr);
    REQUIRE(sm.get_pipeline_latency_provider() == nullptr);
    REQUIRE(sm.get_tensor_core_timing() == nullptr);

    // 创建 warp, 执行指令
    // 验证: 行为与注入前完全一致, blocked_cycles 来自 InstructionLatencyTable
}

TEST_CASE("SMContext: scoreboard limits concurrent operations") {
    SMContext sm(64, 2048, 64*1024, 0);
    MockScoreboardLimited sb(12);
    sm.set_scoreboard(&sb);

    // 提交 13 个相同 dest_reg 的指令
    // 验证: 前 12 条通过, 第 13 条 warps 被 stall
}

TEST_CASE("SMContext: pipeline overrides InstructionLatencyTable") {
    SMContext sm(64, 2048, 64*1024, 0);
    MockPipelineFixed pipe(4.22);  // FFMA 真实延迟
    sm.set_pipeline_latency_provider(&pipe);

    // 执行 FFMA 指令
    // 验证: blocked_cycles == 5 (ceil of 4.22)
    // 验证: 不是 InstructionLatencyTable 的默认值
}
```

---

## 6. 向后兼容性保证

| 保证 | 验证方式 |
|------|---------|
| 不注入时行为不变 | `nullptr` 测试 → 现有 PTX-EMU 测试全绿 |
| SMContext 构造函数不破坏 | 3 个新成员 = nullptr, 不修改构造函数 |
| 现有 WarpScheduler 注入继续工作 | `set_warp_scheduler()` 完全不被修改 |
| 头文件不引入新依赖 | 3 个接口头文件只依赖 `<cstdint>` + `<string>` |

---

## 7. 协作流程

```
PTX-EMU Team                          CppTLM Team
     │                                      │
     ├─ #1~#4: 接口 + SMContext 头文件 ─────→│ (审查接口签名)
     │      (0.5 天)                         │
     │                                      ├─ 开始 Phase 8.B 模块实现
     │                                      │  (2 周, 基于已稳定的接口)
     │                                      │
     ├─ #5: exe_once 注入 ──────────────→   │
     │   + #6: Mock 测试 (2 天)              │
     │                                      │
     │←── 联调: Adapter → SMContext ────────┤
     │   端到端 CUDA kernel 验证             │
```

**关键承诺**：PTX-EMU 完成 #1~#4（接口定义）后 CppTLM 即可启动 Phase 8.B，不需要等 #5~#6 完成。

---

## 8. 联系人

- **CppTLM Team**: CppTLM 仓库 `main` 分支
- **本文档**: `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`
- **ADR**: `docs/adr/ADR-NV-02-phase8b-d1-strategy.md`
- **协作同步**: `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`

---

*本文档由 CppTLM Team 生成。如有接口签名疑问，优先在 #1~#4 阶段澄清。*