# HSK-4/5 + Phase 1 状态回复 (CppTLM -> PTX-EMU)

> **日期**: 2026-07-17
> **发送方**: CppTLM Team (Sisyphus)
> **接收方**: PTX-EMU Architecture Team
> **回传目标**: `#cpptlm-integration` Slack 频道 / PTX-EMU PR comment
> **形式**: 结构化确认 + commit hash 引用
> **关联**: PTX-EMU HSK `openspec/changes/cpptlm-phase8b-injection-points/hsk-{4,5}.md`
> **本回复 commit**: 待回填 (本文件提交后)

---

## HSK-4 闭环 (3 纯虚接口头文件) 🟡 **Ack - 待 rebase 编译验证**

> **PTX-EMU 锁定 commit hash**:
> - `8acfd2d1` (IScoreboard)
> - `9e7361b9` (IPipelineLatencyProvider)
> - `463038e0` (ITensorCoreTiming)
> - HSK-4 actual send: `34620770`

### 逐项确认

| PTX-EMU 请求 | CppTLM 状态 | 证据 |
|-------------|-----------|------|
| 1. 接收 3 接口头文件首发 commit hash | ✅ **确认收到** (2026-07-17) | PTX-EMU `include/ptxsim/{scoreboard,pipeline,tensor_core}_interface.h` 3 文件已读完整 |
| 2a. `IScoreboard` 4 纯虚方法签名 | ✅ **字节级一致** | `has_free_entry() const` / `allocate(uint32_t,uint32_t)` / `release(uint32_t,uint32_t)` / `tick()` 与 CppTLM RFC-P1-001 §3.1 锁定签名匹配 |
| 2b. `IPipelineLatencyProvider` 2 纯虚方法签名 | ✅ **字节级一致** | `get_fractional_cycles(string,PipelineId)` + `get_fractional_cycles_by_type(int,PipelineId)` 与 RFC-P1-001 §3.2 匹配 |
| 2c. `ITensorCoreTiming` 2 纯虚 + 1 默认实现 | ✅ **字节级一致** | `get_latency(TcPrecision)` + `get_throughput_cycles(TcPrecision)` + `get_latency_mnk(prec,M,N,K)` 默认实现退化到 `get_latency(prec)`,与 RFC-P1-001 §3.3 匹配 |
| 3a. `PipelineId` enum 值 0-5 | ✅ **完全匹配** | PTX-EMU `pipeline_interface.h:9-16` 与 CppTLM RFC-P1-003 锁定值一致: `P0_INT_FP32=0, V_SIMD=1, P1_FP64=2, P2_SFU=3, P3_LSU=4, P4_TC=5` |
| 3b. `TcPrecision` enum 值 0-5 | ✅ **完全匹配** | PTX-EMU `tensor_core_interface.h:8-15` 与 CppTLM RFC-P1-003 锁定值一致: `FP4=0, FP6=1, FP8=2, FP16=3, BF16=4, TF32=5` |
| 4. CppTLM 端 12 端点 `static_assert` 编译期验证 | ⏳ **待 rebase 验证** (todo I, 非阻塞) | CppTLM CI rebase 到 `463038e0` 后即可编译期断言;当前 RFC-P1-003 enum 值已锁定 (`2b28505`) |
| 5. CppTLM 端实现 3 接口 (ScoreboardTLM/PipelineTLM/TensorCoreTLM) | ⏳ **待 P1 Phase 1 实施** (todo J) | `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md` Phase 1, 0/4 完成 |
| 6. 回复 commit hash + 接口签名确认 | 📤 本文件 | 见下 |

### Commit hash 回传

- **PTX-EMU 3 接口 commit**: `8acfd2d1` / `9e7361b9` / `463038e0` (CppTLM 端已确认收到)
- **CppTLM P1 RFC 锁定**: `2b28505 docs(specs): RFC-P1-001~004 to PTX-EMU 团队` (enum 值锁定源头)
- **CppTLM 待 rebase 目标**: PTX-EMU `463038e0` (3 接口齐全后的 latest)

### 双 SHA 锁定文档

`include/cudart/AGENTS.md` (待 H 步同步后):
- HSK-1 原始基线: PTX-EMU commit `8dc000ec` (2026-07-15)
- HSK-1 re-vendor: PTX-EMU commit `603bd8bc` (2026-07-16)
- **HSK-4 Phase 1 接口基线**: PTX-EMU commit `463038e0` (2026-07-17)  ← NEW
- **HSK-5 exe_once 注入基线**: PTX-EMU commit `367fd6a5` (2026-07-17)  ← NEW
- CPPTLMBRIDGE_VERSION = 1 (类接口签名未变, version bump 未触发)

### 接口签名快照 (字节级对齐确认)

```cpp
// === IScoreboard (scoreboard_interface.h, 16 行) ===
class IScoreboard {
public:
    virtual ~IScoreboard() = default;
    virtual bool has_free_entry() const = 0;
    virtual bool allocate(uint32_t reg_id, uint32_t warp_id) = 0;
    virtual bool release(uint32_t reg_id, uint32_t warp_id) = 0;
    virtual void tick() = 0;
};

// === IPipelineLatencyProvider (pipeline_interface.h, 29 行) ===
enum class PipelineId : uint32_t {
    P0_INT_FP32 = 0, V_SIMD = 1, P1_FP64 = 2, P2_SFU = 3, P3_LSU = 4, P4_TC = 5
};
class IPipelineLatencyProvider {
public:
    virtual ~IPipelineLatencyProvider() = default;
    virtual double get_fractional_cycles(
        const std::string& instruction, PipelineId pipe_id) const = 0;
    virtual double get_fractional_cycles_by_type(
        int statement_type, PipelineId pipe_id) const = 0;
};

// === ITensorCoreTiming (tensor_core_interface.h, 31 行) ===
enum class TcPrecision : uint32_t {
    FP4 = 0, FP6 = 1, FP8 = 2, FP16 = 3, BF16 = 4, TF32 = 5
};
class ITensorCoreTiming {
public:
    virtual ~ITensorCoreTiming() = default;
    virtual uint32_t get_latency(TcPrecision prec) const = 0;
    virtual uint32_t get_throughput_cycles(TcPrecision prec) const = 0;
    virtual uint32_t get_latency_mnk(
        TcPrecision prec, uint32_t M, uint32_t N, uint32_t K) const {
        return get_latency(prec);  // default impl
    }
};
```

---

## HSK-5 闭环 (exe_once 3-step 注入) 🟡 **Ack - 待 rebase + 集成测试验证**

> **PTX-EMU 锁定 commit hash**:
> - `367fd6a5` (sm_context.cpp impl - Step A/B/C + 3 BUG 修复)
> - `921b4542` (tasks.md done)
> - HSK-5 actual send: `488d840d`

### 逐项确认

| PTX-EMU 请求 | CppTLM 状态 | 证据 |
|-------------|-----------|------|
| 1. 接收 `367fd6a5` (sm_context.cpp) | ✅ **确认收到** (2026-07-17) | commit message + hsk-5.md 内容已读完整 |
| 2a. Step A: Scoreboard hazard check | ✅ **设计对齐** | `tick + has_free_entry + allocate(dest_regs, warp_id) + rollback on failure + goto warp_done` 与 CppTLM `cpptlm-d1-p1-pipeline-scoreboard/design.md` §3 Step A 匹配 |
| 2b. Step B: Latency query 优先级链 | ✅ **设计对齐** | `pipeline_provider_ -> tensor_core_timing_ -> ptxsim::getLatency()` 优先级链与 RFC-P1-002 §2 匹配; `next_warp->set_blocked_cycles_for_active(latency)` 是 CppTLM 端 WarpContext 扩展点 |
| 2c. Step C: Scoreboard release (gated by `warp_executed`) | ✅ **设计对齐** | `scoreboard_->release(dest_reg, warp_id)` for all dest regs,守卫防止 Step A 失败时释放未分配 regs - Oracle BUG-3 fix |
| 3. 3 个 Oracle BUG 修复确认 | ✅ **确认** | BUG-1 (goto label 位置) / BUG-2 (Step B in skip path) / BUG-3 (Step C in skip path 释放未分配) - CppTLM 端设计假设已隐含这些守卫,PTX-EMU 端修复后与 CppTLM 期望一致 |
| 4. 3 public static helpers | ✅ **确认** | `is_tensor_core_instruction` / `map_instruction_to_pipeline` / `map_instruction_to_tc_precision` - CppTLM Adapter 实施时将复用这些 helper (避免重复实现 PTX 指令分类逻辑) |
| 5. 3 file-local helpers (anonymous namespace) | ✅ **确认** | `step_a_scoreboard_check` / `step_b_set_blocked_cycles` / `step_c_release_scoreboard` - PTX-EMU 内部实现细节,CppTLM 端不直接依赖 |
| 6. PTX-EMU 端验证: 27/27 helpers + 13/13 barrier PASS | ✅ **确认收到** | 0 回归 + nullptr fallback 字节级兼容 - 满足 CppTLM P1 启动条件 3 的核心测试部分 |
| 7. CppTLM 端 rebase + 编译验证 | ⏳ **待 todo I** (非阻塞) | 需 CppTLM CI rebase 到 `367fd6a5` 后跑 12 端点 `static_assert` |
| 8. CppTLM 端 e2e 集成测试 | ⏳ **待 PTX-7a/7b** (PTX-EMU 端) | CppTLM P1 Phase 4 集成验证 (`tasks.md` §Phase 4) 需 PTX-EMU 端 7 Mock + 4 集成 tests 完成 |

### Commit hash 回传

- **PTX-EMU exe_once 主体**: `367fd6a5` (CppTLM 端已确认收到)
- **PTX-EMU tasks.md done**: `921b4542`
- **CppTLM 待 rebase 目标**: PTX-EMU `367fd6a5` (含 3 BUG 修复后的稳定态)

### Step A/B/C 控制流确认

```
exe_once(stmt) 控制流 (PTX-EMU 端, commit 367fd6a5):
  ┌─────────────────────────────────────────────────────────┐
  │ Step A: Scoreboard hazard check                         │
  │   - scoreboard_->tick()                                 │
  │   - if (!scoreboard_->has_free_entry()) goto warp_done  │
  │   - for each dest_reg: scoreboard_->allocate(reg, warp) │
  │   - on failure: rollback allocated_so_far + goto warp_done │
  │   - nullptr scoreboard_ = skip (byte-identical)         │
  ├─────────────────────────────────────────────────────────┤
  │ [execution path only - NOT skip path]                   │
  │ Step B: Latency query (priority chain)                  │
  │   - latency = pipeline_provider_->get_fractional_cycles_by_type(...) │
  │   - if (is_tensor_core_instruction(stmt))               │
  │       latency = tensor_core_timing_->get_latency(...)   │
  │   - fallback: ptxsim::getLatency(stmt.type).cycles      │
  │   - next_warp->set_blocked_cycles_for_active(latency)   │
  ├─────────────────────────────────────────────────────────┤
  │ [gated by warp_executed - NOT in skip path]             │
  │ Step C: Scoreboard release                              │
  │   - for each dest_reg: scoreboard_->release(reg, warp)  │
  │   - Guard prevents releasing unallocated entries        │
  └─────────────────────────────────────────────────────────┘

  warp_done: (Oracle BUG-1 fix: BEFORE set_scheduled(false))
    next_warp->set_scheduled(false)
    ...

CppTLM 端期望 (cpptlm-d1-p1-pipeline-scoreboard/design.md §3):
  - ScoreboardTLM 实现 IScoreboard 4 方法
  - PipelineTLM 实现 IPipelineLatencyProvider 2 方法
  - TensorCoreTLM 实现 ITensorCoreTiming 2+1 方法
  - 4 Adapter 将 CppTLM 实现注入 PTX-EMU SMContext 3 setter
```

---

## P1 启动条件状态更新 (vs 2026-07-17-hsk-1-2-3-responses.md)

> **状态截至 2026-07-17 (HSK-5 后)**,`cpptlm-d1-p1-pipeline-scoreboard` 仍为 `state: proposed`

| 启动条件 | 之前状态 (HSK-1/2/3 时) | 当前状态 (HSK-4/5 后) | 变化 |
|---------|:---:|:---:|---|
| 1. P0 `cpptlm-f12b-ld-impl` 已归档 | ✅ `b94eccc` | ✅ `b94eccc` | - |
| 2. PTX-EMU P1 接口已提交 (3 接口 + SMContext setter + exe_once 注入) | ❌ **阻塞** | 🟡 **实质解锁** | HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) + HSK-5 (`367fd6a5`) 已交付 |
| 3. PTX-EMU P1 测试已通过 (Mock + 集成 + 回归) | ❌ **阻塞** | 🟡 **部分完成** | 21/21 ABI + 27/27 helpers + 13/13 barrier PASS;⏳ PTX-7a (7 Mock) + PTX-7b (4 集成) 待 |

### P1 Phase 推进度

| Phase | 之前 | HSK-4/5 后可推进度 |
|-------|:---:|:---:|
| Phase 1: 3 核心模块 (`scoreboard_tlm`/`pipeline_tlm`/`tensor_core_tlm`) | 0/4 | 🟡 **可启动** (接口签名锁定) |
| Phase 2: 4 Adapter | 0/6 | 🟡 **可启动** (依赖 Phase 1) |
| Phase 3: AsyncCompletion 占位 | ✅ 1/2 (`e69cd1d`) | ✅ |
| Phase 4: KernelLaunchTLM 激活 | 0/5 | ⏳ 等 Phase 1+2 |
| **G-D4: 12 端点 `static_assert`** | ❌ | 🟡 **可立即编译期验证** (todo I) |

### 阻塞关系图 (HSK-4/5 后更新)

```
PTX-EMU 端:
  ✅ HSK-4: 3 接口头文件 (8acfd2d1/9e7361b9/463038e0)
  ✅ HSK-5: exe_once 3-step 注入 (367fd6a5)
  ⏳ PTX-7a: 7 Mock 单元测试
  ⏳ PTX-7b: 4 集成测试
       │
       ▼
CppTLM 端 (可立即启动):
  🟡 todo I: rebase + 12 端点 static_assert 编译验证 (G-D4)
       │
       ▼
  🟡 todo J: P1 Phase 1 (3 核心模块, ~600 LOC)
       │
       ├── include/tlm/gpu/scoreboard_tlm.{hh,cc}      (IScoreboard impl)
       ├── include/tlm/gpu/pipeline_tlm.{hh,cc}        (IPipelineLatencyProvider impl)
       └── include/tlm/gpu/tensor_core_tlm.{hh,cc}     (ITensorCoreTiming impl)
            │
            ▼
       todo J (cont): P1 Phase 2 (4 Adapter, ~400 LOC)
            │
            ├── include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}
            ├── include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}
            ├── include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}
            └── (WarpScheduler adapter - 已在 Phase 8.A 部分实施)
                 │
                 ▼
            Phase 4: KernelLaunchTLM 激活 (待 PTX-7a/7b 完成)
```

---

## 跨仓库握手链 (commit hash 互相引用, HSK-4/5 后)

```
CppTLM 端:                                       PTX-EMU 端:
─────────                                        ──────────
b94eccc P0 archive  ──────────┐
                              │
e69cd1d P2 AsyncCompletion ──┤   引用 ->  6b367cad hsk-3 Ready to Send
                              │              (含 CPPTLM_COMMIT_HASH=73e5422)
2b28505 RFC-P1-001~004 ──────┤
                              │            df05e10b Phase 0 对齐
3d83a1e B1-B4 文档修复 ─────┤            (锁定 PTX-0.1/0.2/0.4)
                              │
ea60cbc P0 tasks.md 勾选 ───┤            8acfd2d1 HSK-4 IScoreboard ────┐
                              │            9e7361b9 HSK-4 IPipelineLatency ─┤ HSK-4
cafb466 P1 §3.1+§3.2 勾选 ──┤            463038e0 HSK-4 ITensorCoreTiming ─┘
                              │
8b4462c AGENTS.md 路径修正 ──┤            367fd6a5 HSK-5 exe_once 3-step ──┐
                              │            921b4542 HSK-5 tasks.md done ────┤ HSK-5
73e5422 P0 main merge ──────┘ ←── HSK-3 锁定  488d840d HSK-5 actual send ──┘
                              │
                              └─── HSK-4/5 响应 (本文件) ───>  PTX-EMU 确认
```

---

## 待 PTX-EMU 端推进事项 (informational, 不阻塞 CppTLM todo I)

1. **PTX-7a**: 7 Mock 单元测试 (`test_scoreboard_injection` / `test_pipeline_injection` / `test_nullptr_fallback` 等) - 解锁 CppTLM P1 Phase 4 完整验收
2. **PTX-7b**: 4 集成测试 - 解锁 CppTLM P1 G-D5 (5 类 microbenchmark vs gpgpu-sim ±15%)
3. **HSK-4/5 actual Slack 发送**: 草稿在 `openspec/changes/cpptlm-phase8b-injection-points/hsk-{4,5}.md`,需 PTX-EMU Architecture Team 手动复制到 `#cpptlm-integration` Slack 频道 (本响应文档同步发出)
4. **`cpptlm-phase8b-injection-points` change 状态**: Proposed -> Active (执行 `openspec change` 子命令)
5. **Phase 5 完整闭合**: tasks.md 当前 37/62 完成,25 待 (主要为 PTX-7a/7b 测试)

---

## CppTLM 端下一步 (todo I + J, 等 PTX-7a/7b)

| todo | 动作 | 风险 | 时机 |
|------|------|:---:|------|
| **I** | CppTLM CI rebase 到 PTX-EMU `367fd6a5` + 跑 12 端点 `static_assert` 编译验证 | 中 | 用户授权后 1-2 天 |
| **J** | P1 Phase 1 实施 (3 核心模块 ~600 LOC) + Phase 2 (4 Adapter ~400 LOC) | 中-高 | todo I 通过 + 等 PTX-7a/7b |
| **K** | P1 Phase 4 集成验证 + G-D5 microbenchmark | 高 | PTX-7a/7b 完成 + Phase 1+2 实施完毕 |

---

## References

- CppTLM HSK-1/2/3 历史响应: [`2026-07-17-hsk-1-2-3-responses.md`](2026-07-17-hsk-1-2-3-responses.md)
- CppTLM HSK-1/2/3 初次响应: [`2026-07-15-cpptlm-hsk-response.md`](2026-07-15-cpptlm-hsk-response.md)
- CppTLM P1 RFC: [`2026-07-16-rfcs-to-ptxemu-p1-injection.md`](2026-07-16-rfcs-to-ptxemu-p1-injection.md)
- CppTLM 综合任务书: [`2026-07-14-ptxemu-comprehensive-modification-plan.md`](2026-07-14-ptxemu-comprehensive-modification-plan.md)
- CppTLM P1 计划: [`../../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`](../../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/)
- CppTLM P0 归档: [`../../openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/`](../../openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/)
- PTX-EMU HSK-4: `/workspace/project/PTX-EMU/openspec/changes/cpptlm-phase8b-injection-points/hsk-4.md`
- PTX-EMU HSK-5: `/workspace/project/PTX-EMU/openspec/changes/cpptlm-phase8b-injection-points/hsk-5.md`
- PTX-EMU ADR-0020: `/workspace/project/PTX-EMU/docs/adr/0020-cpptlm-injection-points.md`
- PTX-EMU 3 接口头文件: `/workspace/project/PTX-EMU/include/ptxsim/{scoreboard,pipeline,tensor_core}_interface.h`
- PTX-EMU SMContext 注入: `/workspace/project/PTX-EMU/include/ptxsim/sm_context.h` (set_scoreboard / set_pipeline_latency_provider / set_tensor_core_timing + 3 static helpers)

---

**最后更新**: 2026-07-17 (CppTLM 端确认 HSK-4/5 收到 + Phase 1 实质解锁 2/3)
**下次更新**: (a) todo I (rebase + 12 端点 static_assert) 完成后,或 (b) PTX-7a/7b 完成后启动 P1 Phase 1 实施
