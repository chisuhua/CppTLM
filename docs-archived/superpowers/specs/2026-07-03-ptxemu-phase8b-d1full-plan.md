# CppTLM ↔ PTX-EMU 协同计划：Phase 8.B D1-Full 接口对齐

> **Date**: 2026-07-03
> **Scope**: PTX-EMU 侧新增 3 个注入点 + CppTLM 侧 6 模块接口对齐 + 集成路径
> **决策基础**: PTX-EMU 团队确认可配合修改

---

## 0. 总体策略：Adapter Pattern 解耦

```
Phase 8.B 模块 (无 PTX-EMU 依赖)
       │
       │  通过 Adapter 桥接
       ↓
PTX-EMU 注入接口 (PTX-EMU 定义纯虚类)
       │
       │  SMContext::set_xxx()
       ↓
PTX-EMU SMContext::exe_once() (注入点调用)
```

**设计原则**: CppTLM Phase 8.B 所有模块**不依赖 PTX-EMU 头文件**。PTX-EMU 定义注入接口（纯虚基类），CppTLM 通过 Adapter 层实现。Phase 8.B 独立测试用 CppTLM 内部接口；F12b-LD 集成时 Adapter 桥接两套接口。

---

## 第 1 部分：PTX-EMU 侧改造

### 1.1 新增 3 个注入接口（纯虚基类，零外部依赖）

#### 文件 1: `include/ptxsim/scoreboard_interface.h`

```cpp
#ifndef PTXSIM_SCOREBOARD_INTERFACE_H
#define PTXSIM_SCOREBOARD_INTERFACE_H

#include <cstdint>

class IScoreboard {
public:
    virtual ~IScoreboard() = default;

    /// 检查指定 warp 是否有空闲 scoreboard entry
    virtual bool has_free_entry() const = 0;

    /// 分配一个 entry（用于 RAW hazard 跟踪）
    /// @param reg_id  目标寄存器 ID
    /// @param warp_id 请求分配的 warp
    /// @return true = 分配成功, false = 无空闲 entry (stall warp)
    virtual bool allocate(uint32_t reg_id, uint32_t warp_id) = 0;

    /// 释放已分配的 entry
    virtual bool release(uint32_t reg_id, uint32_t warp_id) = 0;

    /// 每个 tick 推进内部状态
    virtual void tick() = 0;
};

#endif
```

#### 文件 2: `include/ptxsim/pipeline_interface.h`

```cpp
#ifndef PTXSIM_PIPELINE_INTERFACE_H
#define PTXSIM_PIPELINE_INTERFACE_H

#include <cstdint>
#include <string>

/// 管线 ID（5+V 抽象）
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

    /// 查询给定指令在指定管线上的分数 cycle 延迟
    /// @param instruction  指令名 (如 "FFMA", "LDG", "HMMA")
    /// @param pipe_id      执行管线
    /// @return 分数 cycle 延迟 (如 4.22)
    virtual double get_fractional_cycles(
        const std::string& instruction,
        PipelineId pipe_id) const = 0;

    /// 按 PTX StatementType 查询（用于 InstructionLatencyTable 集成）
    /// @param type  来自 ptx_ir/ptx_types.h
    /// @param pipe_id 执行管线
    /// @return 分数 cycle 延迟，返回 0.0 表示查表未命中
    virtual double get_fractional_cycles_by_type(
        int statement_type,
        PipelineId pipe_id) const = 0;
};

#endif
```

#### 文件 3: `include/ptxsim/tensor_core_interface.h`

```cpp
#ifndef PTXSIM_TENSOR_CORE_INTERFACE_H
#define PTXSIM_TENSOR_CORE_INTERFACE_H

#include <cstdint>

/// TC 精度枚举
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

    /// 查询指定精度的 TC 延迟（cycles）
    virtual uint32_t get_latency(TcPrecision prec) const = 0;

    /// 查询指定精度的 TC 吞吐（cycles per instruction）
    virtual uint32_t get_throughput_cycles(TcPrecision prec) const = 0;

    /// 按矩阵维度查询延迟（未来支持 M/N/K 敏感延迟）
    virtual uint32_t get_latency_mnk(
        TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const {
        return get_latency(prec);  // 默认退化到精度统一延迟
    }
};

#endif
```

### 1.2 SMContext 新增 3 个 setter

#### 修改: `include/ptxsim/sm_context.h`

```cpp
// 新增 include
#include "ptxsim/scoreboard_interface.h"
#include "ptxsim/pipeline_interface.h"
#include "ptxsim/tensor_core_interface.h"

class SMContext {
    // === 新增公开方法 ===

    /// 注入外部 Scoreboard（nullptr=禁用，默认)
    void set_scoreboard(IScoreboard* scoreboard);

    /// 注入外部管线延迟提供者（nullptr=使用内置 InstructionLatencyTable）
    void set_pipeline_latency_provider(IPipelineLatencyProvider* provider);

    /// 注入外部 TC timing（nullptr=使用内置延迟）
    void set_tensor_core_timing(ITensorCoreTiming* tc);

    // === 新增私有成员 ===
private:
    IScoreboard* scoreboard_ = nullptr;
    IPipelineLatencyProvider* pipeline_provider_ = nullptr;
    ITensorCoreTiming* tensor_core_timing_ = nullptr;
};
```

### 1.3 SMContext::exe_once() 集成点

#### 修改: `src/ptxsim/core/sm_context.cpp` (exe_once 方法内)

```
当前 exe_once() 流程:
  1. warp_scheduler_->schedule_next()  → warp
  2. warp->execute_warp_instruction(stmt)
  3. update_state()

注入后流程 (+3 个钩子):
  1. warp_scheduler_->schedule_next()  → warp
  1.5 [NEW] if scoreboard_:
         if !scoreboard_->has_free_entry() → stall warp, skip to next
         for each dest reg in stmt:
           if !scoreboard_->allocate(reg_id, warp_id) → stall
  2. [MODIFIED] if pipeline_provider_:
         latency = pipeline_provider_->get_fractional_cycles_by_type(stmt.type, pipe_id)
         warp->set_blocked_cycles(static_cast<uint32_t>(latency))
       elif tensor_core_timing_ && is_tc_instruction(stmt):
         latency = tensor_core_timing_->get_latency(precision)
         warp->set_blocked_cycles(latency)
  2.5 [NEW] warp->execute_warp_instruction(stmt)   ← 原执行逻辑
  3. [NEW] if scoreboard_:
         for each dest reg in stmt: scoreboard_->release(reg_id, warp_id)
  4. scoreboard_->tick()  (推进内部计数器)
  5. update_state()
```

**关键约束**:
- 步骤 1.5~2 之间的 stall 逻辑**不改变** PTX-EMU 原有执行语义——仅在原有基础上增加 scoreboard 检查
- `pipeline_provider_` 返回值**覆盖** `InstructionLatencyTable` 查表结果（而非叠加）
- 如果 `pipeline_provider_` 和 `tensor_core_timing_` 都为 nullptr，退化为原有 `InstructionLatencyTable` 查表

### 1.4 PTX-EMU 测试（Mock 验证）

新增测试文件验证注入点行为：

```cpp
// test/test_scoreboard_injection.cpp
TEST_CASE("Scoreboard injection: stall on 13th allocate") {
    SMContext sm(64, 2048, 64*1024, 0);
    MockScoreboardLimited sb(12);  // 12 entries, 13th fails
    sm.set_scoreboard(&sb);

    // 创建 warp + CTA, 提交 13 个相同 reg_id 的指令
    // 前 12 条通过，第 13 条 stall
    // 验证 warp blocked_cycles > 0
}

TEST_CASE("Pipeline injection: override FFMA latency") {
    SMContext sm(...);
    MockPipelineLatencyProvider plp;
    plp.set_latency("FFMA", PipelineId::P0_INT_FP32, 4.22);
    sm.set_pipeline_latency_provider(&plp);

    // 执行 FFMA 指令
    // 验证 warp blocked_cycles == 4 (ceiling of 4.22)
}

TEST_CASE("Injection points: nullptr fallback to defaults") {
    SMContext sm(...);
    // scoreboard_ == nullptr → 不检查 hazard
    // pipeline_provider_ == nullptr → 用 InstructionLatencyTable
    // 行为与注入前完全一致
}
```

### 1.5 PTX-EMU 侧变更清单

| 编号 | 类型 | 文件 | 内容 | 工时 |
|:----:|------|------|------|:---:|
| PTX-1 | 新增 | `include/ptxsim/scoreboard_interface.h` | IScoreboard 纯虚基类 | 0.1d |
| PTX-2 | 新增 | `include/ptxsim/pipeline_interface.h` | IPipelineLatencyProvider + PipelineId 枚举 | 0.1d |
| PTX-3 | 新增 | `include/ptxsim/tensor_core_interface.h` | ITensorCoreTiming + TcPrecision 枚举 | 0.1d |
| PTX-4 | 修改 | `include/ptxsim/sm_context.h` | +3 setter + 3 私有成员 | 0.1d |
| PTX-5 | 修改 | `src/ptxsim/core/sm_context.cpp` | exe_once() 三步注入 + nullptr fallback | 1.0d |
| PTX-6 | 新增 | `test/test_scoreboard_injection.cpp` | Mock 测试: scoreboard + pipeline + nullptr fallback | 0.5d |

**PTX-EMU 总工时: 2 天**（3 个接口 0.3d + SMContext 改造 1.1d + 测试 0.5d）

---

## 第 2 部分：CppTLM 侧 Phase 8.B

### 2.1 模块接口（CppTLM 内部接口 → 无 PTX-EMU 依赖）

CppTLM 定义自己的内部接口（位于 `include/tlm/gpu/`），Phase 8.B 模块实现这些接口。

```
include/tlm/gpu/
├── scoreboard_interface.hh     ← 新增: tlm::IScoreboardInternal (仅 CppTLM 内部)
├── pipeline_interface.hh       ← 新增: tlm::IPipelineLatencyInternal
├── tensor_core_interface.hh    ← 新增: tlm::ITensorCoreTimingInternal
├── scoreboard_tlm.hh           ← ScoreboardTLM : IScoreboardInternal
├── warp_scheduler_tlm.hh       ← WarpSchedulerTLM (保留 uint32_t 接口)
├── pipeline_tlm.hh             ← PipelineTLM : IPipelineLatencyInternal
├── tensor_core_tlm.hh          ← TensorCoreTLM : ITensorCoreTimingInternal
├── l2_partition_tlm.hh         ← L2PartitionTLM (独立)
└── subcore_tlm.hh              ← SubCoreTLM (组合 + SMContext 包裹)
```

### 2.2 任务修正（Task 9-14 → 9-16）

| 任务 | 原 scope | 修正后 scope | 变化 |
|:----:|---------|-------------|------|
| **Task 9** | ScoreboardTLM ≥12 entries | ScoreboardTLM ≥12 entries **实现 `tlm::IScoreboardInternal`** | +接口继承 |
| **Task 10a** | WarpSchedulerTLM CGGTY | 重命名 MinimalWarpSchedulerTLM → WarpSchedulerTLM + CGGTY + priority | 不变 |
| **Task 10b** | (不存在) | **新增 `CppTLMWarpSchedulerAdapter : public WarpScheduler`** | 新增 |
| **Task 11** | PipelineTLM 5+V | PipelineTLM 5+V **实现 `tlm::IPipelineLatencyInternal`** | +接口继承 |
| **Task 12** | TensorCoreTLM 6 精度 | TensorCoreTLM 6 精度 **实现 `tlm::ITensorCoreTimingInternal`** | +接口继承 |
| **Task 13** | L2PartitionTLM multi-slice | 不变 | — |
| **Task 14** | SubCoreTLM black-box | SubCoreTLM: 内部组合 4 模块 + `set_sm_context()` 预留, **tick() 双模式** | +SMContext |
| **Task 15** | (不存在） | **新增 PTX-EMU Adapter 层**: ScoreboardAdapter, PipelineAdapter, TensorCoreAdapter, WarpSchedulerAdapter | 新增 |
| **Task 16** | 5 microbenchmark + docs | 合成 workload (Level 1) + 真实 CUDA kernel (Level 2, F12b 后) + docs | 双轨验证 |

### 2.3 Task 15: Adapter 层详解

```
include/tlm/gpu/adapter/          (新目录, 仅 F12b-LD 集成时编译)
├── cpptlm_warp_scheduler_adapter.hh/.cc
│   └→ : public WarpScheduler, 内部转 WarpSchedulerTLM
├── cpptlm_scoreboard_adapter.hh/.cc
│   └→ : public IScoreboard (PTX-EMU), 内部转 ScoreboardTLM
├── cpptlm_pipeline_adapter.hh/.cc
│   └→ : public IPipelineLatencyProvider (PTX-EMU), 内部转 PipelineTLM
└── cpptlm_tensor_core_adapter.hh/.cc
    └→ : public ITensorCoreTiming (PTX-EMU), 内部转 TensorCoreTLM
```

每个 Adapter 模板（以 Scoreboard 为例）：

```cpp
// include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.hh
#include "ptxsim/scoreboard_interface.h"   // PTX-EMU 接口
#include "tlm/gpu/scoreboard_tlm.hh"        // CppTLM 实现

class CppTLMScoreboardAdapter : public IScoreboard {
public:
    explicit CppTLMScoreboardAdapter(tlm::ScoreboardTLM* impl)
        : impl_(impl) {}

    bool has_free_entry() const override {
        return impl_->has_free_entry();
    }
    bool allocate(uint32_t reg_id, uint32_t warp_id) override {
        return impl_->allocate(reg_id);  // warp_id 暂存于内部 map
    }
    bool release(uint32_t reg_id, uint32_t warp_id) override {
        return impl_->release(reg_id);
    }
    void tick() override {
        impl_->tick();
    }

private:
    tlm::ScoreboardTLM* impl_;
};
```

### 2.4 完整实施时间线

```
Week 0     PTX-EMU Team: 新增 3 接口 + SMContext 修改 + Mock 测试 (2d)
           └→ 产出: PTX-1..6 全部完成, 现有 PTX-EMU 测试不破坏

Week 1-2   CppTLM Team (6 人并行):
           Task 9  ScoreboardTLM (0.5d)
           Task 10a WarpSchedulerTLM (1d)
           Task 10b WarpSchedulerAdapter (0.5d)
           Task 11  PipelineTLM (1.5d)
           Task 12  TensorCoreTLM (0.5d)
           Task 13  L2PartitionTLM (0.5d)
           Task 14  SubCoreTLM (1d)
           └→ 产出: 6 modules + 6 unit tests + G1 验收

Week 3     CppTLM Team:
           Task 15  Adapter 层 (1d)
           Task 16  Level 1 合成 workload 集成测试 (0.5d)
           └→ 产出: Level 1 验证通过, G2 验收

Week 4     (F12b-LD 就绪后)
           集成测试: SMContext 注入 → exe_once() → MemoryBridge
           Level 2: 真实 CUDA kernel → PTX-EMU → CppTLM NoC → bandwidth
           Level 3: vs gpgpu-sim (±15%) + vs standalone PTX-EMU (±10%)
           M2 验收 + Oracle 审查 + OpenSpec 归档
```

### 2.5 CppTLM 侧变更清单

| 编号 | 类型 | 文件 | 内容 | 阶段 |
|:----:|------|------|------|:---:|
| **CPP-1** | 新增 | `include/tlm/gpu/scoreboard_interface.hh` | `tlm::IScoreboardInternal` 纯虚基类 | A |
| **CPP-2** | 新增 | `include/tlm/gpu/pipeline_interface.hh` | `tlm::IPipelineLatencyInternal` + `tlm::PipelineId` | A |
| **CPP-3** | 新增 | `include/tlm/gpu/tensor_core_interface.hh` | `tlm::ITensorCoreTimingInternal` + `tlm::TcPrecision` | A |
| **CPP-4** | 新增 | `include/tlm/gpu/scoreboard_tlm.hh/.cc` | ScoreboardTLM : IScoreboardInternal | A |
| **CPP-5** | 重命名 | `include/tlm/gpu/warp_scheduler_tlm.hh/.cc` | MinimalWarpSchedulerTLM → WarpSchedulerTLM + CGGTY | A |
| **CPP-6** | 新增 | `include/tlm/gpu/pipeline_tlm.hh/.cc` | PipelineTLM : IPipelineLatencyInternal | A |
| **CPP-7** | 新增 | `include/tlm/gpu/tensor_core_tlm.hh/.cc` | TensorCoreTLM : ITensorCoreTimingInternal | A |
| **CPP-8** | 新增 | `include/tlm/gpu/l2_partition_tlm.hh/.cc` | L2PartitionTLM | A |
| **CPP-9** | 新增 | `include/tlm/gpu/subcore_tlm.hh/.cc` | SubCoreTLM + set_sm_context() 双模式 | A |
| **CPP-10** | 新增 | `include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.hh/.cc` | Adapter: WarpScheduler | B |
| **CPP-11** | 新增 | `include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.hh/.cc` | Adapter: IScoreboard | B |
| **CPP-12** | 新增 | `include/tlm/gpu/adapter/cpptlm_pipeline_adapter.hh/.cc` | Adapter: IPipelineLatencyProvider | B |
| **CPP-13** | 新增 | `include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.hh/.cc` | Adapter: ITensorCoreTiming | B |
| **CPP-14** | 新增 | `test/test_{scoreboard...l2_partition,subcore}_tlm.cc` × 7 | 单元测试 | A |
| **CPP-15** | 新增 | `test/test_gpu_soc_phase8b.cc` | Level 1 合成 workload 集成 | A |
| **CPP-16** | 修改 | `include/chstream_register.hh` | +6 行注册新模块 | A |
| **CPP-17** | 修改 | `include/tlm/cluster/compute_cluster.hh` | 集成 SubCoreTLM | A |
| **CPP-18** | 修改 | `include/tlm/cluster/gpu_cluster.hh` | 集成 L2PartitionTLM | A |

---

## 第 3 部分：接口契约对照

### 3.1 CppTLM 内部接口 ↔ PTX-EMU 注入接口 类型映射

| CppTLM internal | PTX-EMU interface | Adapter 桥接逻辑 |
|----------------|-------------------|-----------------|
| `tlm::IScoreboardInternal::has_free_entry()` | `IScoreboard::has_free_entry()` | 直接转发 (签名一致) |
| `tlm::IScoreboardInternal::allocate(reg_id)` | `IScoreboard::allocate(reg_id, warp_id)` | `warp_id` 存 Adapter 内部 map, 转发 `reg_id` |
| `tlm::IScoreboardInternal::release(reg_id)` | `IScoreboard::release(reg_id, warp_id)` | 从 Adapter map 查 `warp_id`, 转发 |
| `tlm::IPipelineLatencyInternal::get_fractional_cycles(instr, pipe)` | `IPipelineLatencyProvider::get_fractional_cycles(instr, pipe)` | 直接转发 (`tlm::PipelineId` ↔ `PipelineId` 枚举值相同) |
| `tlm::ITensorCoreTimingInternal::get_latency(prec)` | `ITensorCoreTiming::get_latency(prec)` | 直接转发 (`tlm::TcPrecision` ↔ `TcPrecision` 枚举值相同) |

### 3.2 枚举一致性保障

```cpp
// PTX-EMU: include/ptxsim/pipeline_interface.h
enum class PipelineId : uint32_t { P0_INT_FP32=0, V_SIMD=1, P1_FP64=2, P2_SFU=3, P3_LSU=4, P4_TC=5 };

// CppTLM: include/tlm/gpu/pipeline_interface.hh
enum class PipelineId : uint32_t { P0_INT_FP32=0, V_SIMD=1, P1_FP64=2, P2_SFU=3, P3_LSU=4, P4_TC=5 };
// ⚠️  两个命名空间不同 (::PipelineId vs tlm::PipelineId), 但枚举值完全相同
//     Adapter 用 static_cast<PipelineId>(tlm_pipe) 转换
```

---

## 第 4 部分：验收标准

| 编号 | 标准 | 阶段 | 验证方式 |
|:----:|------|:---:|---------|
| **G0** | PTX-EMU 3 接口编译通过 + Mock 测试全部 pass | PTX-EMU Week 0 | `ctest` 全绿 |
| **G1** | CppTLM Phase 8.B 6 模块单元测试全部 pass | CppTLM Week 2 | `cpptlm_tests [gpu][sb][sched][pipe][tc][l2][subcore]` |
| **G2** | CppTLM Phase 8.B Level 1 合成 workload 集成测试 pass | CppTLM Week 3 | `test_gpu_soc_phase8b.cc` |
| **G3** | Adapter 编译通过（与 PTX-EMU 头文件联编） | CppTLM Week 3 | `#include <ptxsim/scoreboard_interface.h>` 编译无错误 |
| **G4** | F12b-LD 集成: SMContext 注入 → exe_once → CppTLM counter | F12b-LD Week 4 | 真实 CUDA kernel 端到端执行 |
| **G5** | vs gpgpu-sim bandwidth ±15% (5 类) + vs standalone PTX-EMU timing ±10% | F12b-LD Week 4 | `test_gpgpu_sim_comparison.py` |
| **G6** | apu_soc 兼容性全绿（不破坏 Phase 8.A） | 全阶段 | `cpptlm_tests [phase7][apu_soc]` |
| **G7** | docs_sync 0 missing + format clean | 全阶段 | `docs_sync_check.sh --strict` |

---

## 附录 A: 需要同步更新的文档

| 文档 | 更新内容 | 优先级 |
|------|---------|:---:|
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/design.md` | §2 模块设计反映 PTX-EMU 接口 + Adapter | 🔴 |
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/tasks.md` | Task 9→16 修正 | 🔴 |
| `openspec/changes/2026-06-24-gpu-soc-phase8b-core/specs/gpu-soc-phase8b.md` | REQ-GPU-8B-1~6 修订 | 🔴 |
| `docs/superpowers/plans/2026-06-24-gpu-soc-phase8b.md` | 同步修订 | 🟡 |
| `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md` | 追加 Phase 8.B D1-Full 协作计划 | 🟡 |
| `docs/adr/ADR-NV-02-phase8b-d1-strategy.md` | D1-Lite → D1-Full 状态更新 | 🟡 |

---

*本计划待 PTX-EMU 团队确认后执行。*