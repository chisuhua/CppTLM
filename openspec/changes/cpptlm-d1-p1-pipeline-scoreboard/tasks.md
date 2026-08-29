# Tasks: cpptlm-d1-p1-pipeline-scoreboard - D1-Full Compute 注入 + Async Seam

> **Status**: Proposed（2026-07-15 从 `cpptlm-f12b-ld-impl` P1/P2/P3 剥离）
> **Design Revision 1**: 2026-07-17（HSK-4/5 后简化 -- vendor 头文件 + 去掉 Internal/空壳 Adapter）
> **Design Revision 2**: 2026-07-18（Metis + Oracle 双审后修复 4 架构 P0 + 4 文档 P0）
> **Design Revision 3**: 2026-07-18（Oracle P0 协同仿真架构设计 -- IPtxEmuDriver 窄接口 + 9 步实施计划）
> **Design Revision 4**: 2026-07-18（Oracle P0 审查修复实施 — unordered_map + CAPACITY=2048 + AdvanceResult + unique_ptr + reset() + VERSION=2）
> **Parent**: `proposal.md` + `design.md` (cpptlm-d1-p1-pipeline-scoreboard)
> **前置**: P0 `cpptlm-f12b-ld-impl` 归档 + PTX-EMU 交付 P1 接口
> **总工时**: ~3.5d（Phase 1 已完成）+ ~2.5d（Phase 4 Oracle P0 修复 + 端到端协同仿真）

## 启动条件

- [x] **P0 已归档**: `cpptlm-f12b-ld-impl` -> `openspec/changes/archive/`（commit `b94eccc`）
- [x] **PTX-EMU P1 接口已提交**: HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) + HSK-5 (`367fd6a5`) 已交付
- [x] **PTX-EMU P1 测试已通过**: 210/210 PASS（PTX-7a 7 Mock + PTX-7b 4 集成，2026-07-18 committed `7df7d560`）；注入点 change 已归档 (`e3070360`)

> **注意**: 2026-07-18 Oracle P0 架构审查发现原 Co-sim seam 未闭合（无 `libcpptlm_cudart.so`、bridge path 不生成 PTX 任务、`poll_kernel` 立即标记完成）。Phase 4 已重新设计为 9 步计划（P0-2 → P0-1 → P0-3），基于 `IPtxEmuDriver` 窄接口跨仓库驱动 PTX-EMU 执行。

---

## Deferred Tasks Summary（2026-08-26 审计）

本 change 当前 60 tasks 进度 53/60 (88%)。**剩余 7 个任务全部为显式 deferred**，非遗漏：

| Task | 描述 | 延迟原因 | 关联 Wave |
|------|------|----------|----------|
| **4.6** | PipelineTLM + TensorCoreTLM latency 精确对齐 | 需 gpgpu-sim 参考数据 | Phase 4 Wave 2 |
| **4.7** | `test_kernel_launch_ptx_integration.cc`（G-D2/G-D3/G-D8） | 需 PTX-EMU 真实 exe_once 链路 | Phase 4 Wave 2 |
| **4.9** | `test_gpgpu_sim_comparison.py`（G-D5 ±15%） | 需 gpgpu-sim reference 数据 | Phase 4 Wave 2 |
| **G-D2** | `set_blocked_cycles_for_active()` 双端验收 | 依赖 4.7 | Phase 4 Wave 2 |
| **G-D3** | `blocked_cycles_remaining` cycle 契约 ≤ 1 cycle | 需统一 cycle 契约 | Phase 4 Wave 2 |
| **G-D5** | 5 类 microbenchmark vs gpgpu-sim ±15% | 依赖 4.9 | Phase 4 Wave 2 |
| **G-D8** | exe_once stall → re-issue 完整循环 | 需真实 SMContext::exe_once() 链路 | Phase 4 Wave 2 |

**全部 7 个 tasks 均标注 `Phase 4 Wave 2`** — 待 gpgpu-sim reference data + 真实 PTX-EMU exe_once 链路测试可用时执行。

### 前置状态更新（2026-08-30 D15 修复后）

| 前置 | 状态 | 证据 |
|------|------|------|
| PTX-EMU 真实 exe_once 链路（4.7/G-D8 前置） | ✅ **已解除** | `./build/bin/cpptlm_tests "[timing]"` 7/7 PASS + `"[gpu]"` 131/131 PASS（含 `test_cuda_core_adapter_timing.cc` 实证 `facade->sm_exe_once()` 经注入后正常驱动） |
| attach_timing 激活（P0 任务1, 4.7 前置） | ✅ 已完成 | `cuda_core_adapter_mvp.cc:67` 3→1 聚合注入 + `ptx_emu_submodule_mvp.cc:157` attach_timing 实现 |
| gpgpu-sim reference data（4.6/4.9/G-D5 前置） | ❌ **仍阻塞** | `which gpgpu-sim` 空,本地无 reference 数据 |
| 统一 cycle 契约（G-D3 前置） | ⚠️ 部分就绪 | 1 CppTLM tick = 1 GPUContext::exe_once() 契约已定义,待 4.7 落地实证 |

**结论**: 4.7/G-D2/G-D3/G-D8 的前置已大部分解除（PTX-EMU 链可用）,但 G-D3 的 cycle 契约实证与 4.6/4.9/G-D5 仍需 gpgpu-sim reference 数据。**Wave 2 启动时 4.7 可直接实施**,4.9/G-D5 仍需外部数据。

**rdd-doctor 状态**: 这些任务触发 1 个 WARNING（tasks-checkbox 53/60 88%），属设计意图延迟，**不**算功能缺陷。可绕过方案：`SKIP_TASKS_GATE=yes` 归档。

**Roadmap 对应**: 这些任务对应 `.rddf/roadmap/phases/phase-7.md` 子任务 7.C（Coherence Protocol 集成，最高风险子任务，缓存协议对齐依赖 gpgpu-sim reference）。

---

## Phase 1: Vendor 头文件 + 3 核心模块 + 12 端点 static_assert（#C4, ~1.5d）

### 1.0 Vendor 3 接口头文件（~0.2d）

- [x] **1.0.1** `include/cudart/scoreboard_interface.h`（vendor from PTX-EMU `8acfd2d1`，16 行，SHA-256: `4935b50fdea76e8ceb3d37374fd5e453b8d40419a02effd4ffb75c5c3ed63f15` ✅ 2026-07-18）
- [x] **1.0.2** `include/cudart/pipeline_interface.h`（vendor from PTX-EMU `9e7361b9`，29 行，SHA-256: `14c4b8209fc22de24e86312b3a77596491e66e5981dd97e51a676efdb30dd681` ✅ 2026-07-18）
- [x] **1.0.3** `include/cudart/tensor_core_interface.h`（vendor from PTX-EMU `463038e0`，31 行，SHA-256: `6ff8351d01feea3c2b783e047738df5ddf91a4dfbc8e492b9e3cfcb93d11e4a5` ✅ 2026-07-18）
- [x] **1.0.4** 更新 `include/cudart/AGENTS.md`：记录 3 vendor 头文件 provenance（SHA-256 + commit hash + 同步策略 + G-D4 PASS 证据） ✅ 2026-07-18
  - **同步策略**: PTX-EMU 接口 commit 后 24h 内手动同步（`git show <commit>:<path>`）+ SHA-256 校验 + 12 端点 static_assert 回归
  - **提取方式**: 必须用 `git show <commit>:<path>`，**不复制 working tree**（避免未提交修改污染）

### 1.1 ScoreboardTLM（~0.3d）

- [x] **1.1.1** `include/tlm/gpu/scoreboard_tlm.hh` ✅ 2026-07-18 (Design Revision 4: unordered_map+CAPACITY=2048)
  - `class ScoreboardTLM : public IScoreboard`（直接实现 PTX-EMU 接口，无 Internal 层）
  - `#include "cudart/scoreboard_interface.h"`
  - `CAPACITY = 2048`（**global scoreboard**，覆盖 64 warps × 8 in-flight × 3 dest × 1.33 margin, **Oracle P0 修复: 原 MAX_ENTRIES=512 → 2048**）+ `std::unordered_map<uint64_t, bool>` O(1) 查找
  - 4 override: `has_free_entry()` / `allocate(reg_id, warp_id)` / `release(reg_id, warp_id)` / `tick()`
  - 新增 `reset()` 非虚方法（跨 kernel 清空 entries）
  - `make_key(reg_id, warp_id)` = `(warp_id << 32) | reg_id` 复合键
- [x] **1.1.2** `src/tlm/gpu/scoreboard_tlm.cc` ✅ 2026-07-18 (Design Revision 4)
  - 构造函数: `entries_.reserve(CAPACITY)` 预分配防碎片
  - `allocate`: `emplace(make_key, true)` — `.second == false` = duplicate rejects
  - `release`: `erase(make_key)` — O(1) 删除
  - `reset()`: `clear()` + `reserve(CAPACITY)` 保留预分配（Phase 4 PTX-EMU kernel launch 前调用）
  - `tick()`: P1 no-op（死锁缓解: 见 design.md §2.1）
- [x] **1.1.3** `test/test_scoreboard_tlm.cc`（9 单测，tag `[gpu][d1p1]`） ✅ 2026-07-18 — 536 assertions PASS
  - [x] `has_free_entry_initial_returns_true`
  - [x] `allocate_fills_entries_and_returns_false_when_full`（**2048 entries 满**）
  - [x] `release_frees_entry_for_reuse`
  - [x] `release_unknown_reg_returns_false`
  - [x] `duplicate_allocate_returns_false`（**rejects 决议**）
  - [x] `multi_warp_allocate_independent`
  - [x] `tick_is_noop_in_phase1`
  - [x] `reset_clears_all_entries`（**Oracle P0 新增**）
  - [x] `o1_lookup_many_entries`（**Oracle P0 新增: 验证 O(1) 性能**）

### 1.2 PipelineTLM（~0.2d）

- [x] **1.2.1** `include/tlm/gpu/pipeline_tlm.hh` ✅ 2026-07-18
- [x] **1.2.2** `src/tlm/gpu/pipeline_tlm.cc` ✅ 2026-07-18
- [x] **1.2.3** `test/test_pipeline_tlm.cc`（4 单测，tag `[gpu][d1p1]`） ✅ 2026-07-18 — PASS

### 1.3 TensorCoreTLM（~0.2d）

- [x] **1.3.1** `include/tlm/gpu/tensor_core_tlm.hh` ✅ 2026-07-18
- [x] **1.3.2** `src/tlm/gpu/tensor_core_tlm.cc` ✅ 2026-07-18
- [x] **1.3.3** `test/test_tensor_core_tlm.cc`（5 单测，tag `[gpu][d1p1]`） ✅ 2026-07-18 — PASS

### 1.4 12 端点 static_assert + 签名级验证（~0.1d）

- [x] **1.4.1** `test/test_12_endpoint_static_assert.cc` ✅ 2026-07-18 — 已通过 `cpptlm_bridge.h` 末尾 `abi_guards_g_d4` 命名空间实现（16 static_assert, 编译期验证, negative test 证实生效）。无需单独测试文件 -- `cpptlm_bridge.h` 由 `memory_bridge.hh` → `src/main.cpp` 编译链确保每次 build 都执行断言。

### 1.5 CMake 集成（~0.1d）

- [x] **1.5.1** `src/CMakeLists.txt`：已新增 `tlm/gpu/scoreboard_tlm.cc` ✅ 2026-07-18
- [x] **1.5.2** `test/CMakeLists.txt`：GLOB 自动发现 `test_scoreboard_tlm.cc`（无需手动注册） ✅ 2026-07-18

**Commit**:
```bash
git add include/cudart/{scoreboard,pipeline,tensor_core}_interface.h \
        include/cudart/AGENTS.md \
        include/tlm/gpu/{scoreboard,pipeline,tensor_core}_tlm.hh \
        src/tlm/gpu/{scoreboard,pipeline,tensor_core}_tlm.cc \
        src/CMakeLists.txt \
        test/test_{scoreboard,pipeline,tensor_core}_tlm.cc \
        test/test_12_endpoint_static_assert.cc
git commit -m "feat(tlm/gpu): 3 核心模块 + 12 端点 static_assert (D1-Full P1 #C4)

ScoreboardTLM + PipelineTLM + TensorCoreTLM 直接实现 PTX-EMU 纯虚接口
(vendor 头文件 from HSK-4)。12 端点 PipelineId+TcPrecision static_assert
+ 签名级 decltype 验证 编译期拦截。Phase 1 占位 latency = 1.0
(G-D5 精确对齐留给 Phase 4)。MAX_ENTRIES=512 (global scoreboard,
覆盖 64 warps × 8 in-flight)。duplicate allocate rejects 决议。

Refs:
- 综合计划 §3 Task #C4
- HSK-4: PTX-EMU 8acfd2d1/9e7361b9/463038e0
- HSK-5: PTX-EMU 367fd6a5
- Design Revision 1 (2026-07-17): vendor + 去掉 Internal/空壳 Adapter
- Design Revision 2 (2026-07-18): Metis + Oracle 双审修复 4 架构 P0"
```

---

## Phase 2: WarpScheduler Adapter（**SKIPPED** — 2026-07-18 Oracle P0 审查确认不需要）

> PTX-EMU Step A 已直接使用 `WarpContext::get_physical_warp_id()`（`sm_context.cpp:30-54`），Step B 在 PTX-EMU 内部调用 `set_blocked_cycles_for_active()`。无需 WarpScheduler Adapter 做 `WarpContext*` ↔ `uint32_t warp_id` 转换。**不创建新文件**。

- [x] **2.1** 评估 WarpScheduler Adapter 必要性 — 结论: **SKIP**

---

## Phase 3: IAsyncCompletion 占位（#C5, ~1h）

> ✅ **状态（2026-07-17 验证）**:
> - `include/tlm/gpu/async_completion_adapter.hh` 已落地（97 行, header-only）
> - 测试: `test/test_async_completion_adapter.cc`（5 TEST_CASE 全部通过）
> - commit: `e69cd1d feat(tlm/gpu): AsyncCompletionAdapter placeholder (D1-Full P2 #C5)`
>
> **2026-07-18 Oracle 审查**: `IAsyncCompletion` 是 CppTLM 自定义接口（非 PTX-EMU vendor）。当前为投机性脚手架，Phase 9+ 确认是否需要。

- [x] **3.1** `include/tlm/gpu/async_completion_adapter.hh`（header-only） ✅
- [x] **3.2** 编译通过 + 单测 5 case 全 pass ✅

---

## Phase 4: 端到端协同仿真 — Oracle P0 修复计划（~2.5d）

> **2026-07-18 Oracle P0 架构审查 (ses_08c578035ffe5hUb6qhVcT51GH)**:
> 原 tasks.md Phase 4 设计过于简化（`static_cast<GPUContext*>(ptx_emu_context_)->exe_once(warp_id)` 无法编译），且 3 个 P0 阻塞项未被识别：
> 1. Co-sim seam 未闭合（无 `libcpptlm_cudart.so`、bridge path 不生成 PTX 任务、`poll_kernel()` 立即标记完成）
> 2. 跨仓库耦合（`cpptlm_core` 不能依赖 PTX-EMU C++20 头文件）
> 3. Scoreboard 多 SM 语义（需 per-SM 实例，非全局单例）
>
> 替代方案: **`IPtxEmuDriver` 窄接口 + `PtxEmuDriverShim`**。详见 design.md §8。

### Wave 0: PTX-EMU 修复（强前置，~1d）— ✅ 已完成 (PTX-EMU commit `13325cd8`+)

> PTX-EMU 侧 Wave 0 已实施并归档（`openspec/changes/cpptlm-p1-ptxemu-shim/` archived）。
> CppTLM 侧跟进修复见 Wave 0a。

- [x] **4.0.1** 修复 `cudart_sim.cpp` bridge path: 同时 enqueue 到 `GPUContext` + 设置 `on_complete` callback ✅ PTX-EMU `13325cd8`
- [x] **4.0.2** PTX-EMU 新增 `src/cudart/cpptlm_bridge/PtxEmuDriverShim` ✅ PTX-EMU `13325cd8`
- [x] **4.0.3** 构建修复: CppTLM PIC + pin commit + add_subdirectory ✅ PTX-EMU `35ffeba8`
- [x] **4.0.4** PTX-EMU 侧新增 `cpptlm_set_driver` ABI 入口（weak default + strong override） ✅ PTX-EMU `13325cd8`

### Wave 0a: CppTLM 跟进修复 — vtable 对齐 + 构建适配（~1h）

> PTX-EMU 实现与 CppTLM 初始设计存在差异: PtxEmuDriverApi vtable 在 `cpptlm_bridge.h` (ABI 真值源), 字段顺序 + 类型需完全一致。

- [x] **4.0a.1** Vendor PTX-EMU `cpptlm_bridge.h` rebase — 新增 `PtxEmuDriverApi` struct + `cpptlm_set_driver` 声明 ✅ 2026-07-18
- [x] **4.0a.2** `ptx_emu_driver.hh` 删除本地 `PtxEmuDriverApi`, include vendored `cpptlm_bridge.h` ✅
- [x] **4.0a.3** `DriverWrapper` 适配 PTX-EMU vtable: `inject_*` 用 `void*` payload, `is_kernel_complete` 返回 `int` ✅
- [x] **4.0a.4** `ptx_emu_driver_shim.cc` 修复: `PtxEmuDriverApi` 全局命名空间 (非 `tlm::`) ✅
- [x] **4.0a.5** 编译期 static_assert: `sizeof(PtxEmuDriverApi) == 64` ✅
- [x] **4.0a.6** 全量回归: 808/808 PASS, 18829 assertions ✅

### Wave 1: CppTLM Phase 4 实现（~1d）

- [x] **4.1** 新增 `include/tlm/gpu/ptx_emu_driver.hh` — `IPtxEmuDriver` 纯虚接口（7 方法: `AdvanceResult advance(max, &actual)` / `is_kernel_complete` / `inject_scoreboard(unique_ptr)` / `inject_pipeline(unique_ptr)` / `inject_tensor_core(unique_ptr)` / `num_sms`） ✅ 2026-07-18
- [x] **4.1a** 新增 `AdvanceResult` enum: `Executed / NoOp / KernelComplete / Error` ✅ 2026-07-18
- [x] **4.1b** `inject_*()` 改为 `std::unique_ptr` 所有权转移 ✅ 2026-07-18
- [x] **4.2** 修改 `kernel_launch_tlm.hh`：`void* ptx_emu_context_` → `IPtxEmuDriver* driver_` + setter `set_ptx_emu_driver()` ✅ 2026-07-18
- [x] **4.3** 修改 `kernel_launch_tlm.cc::tick()`: `result = driver_->advance(N, actual)` → switch on `AdvanceResult` → `Error` 记录日志 + 终止 tick ✅ 2026-07-18
- [x] **4.4** 修改 `kernel_launch_tlm.cc`: 删除 `call_ptx_emu_exe_once_()` 空函数 + 移除 10000 次循环（1 tick = 1 advance） ✅ 2026-07-18
- [x] **4.5** 修改 `main.cpp` f12b-ld 路径: 创建 `vector<unique_ptr<ScoreboardTLM>>` per-SM → `driver->inject_scoreboard(sm_id, std::move(sb))` 转移所有权 ✅ 2026-07-18
- [ ] **4.6** Latency 精确对齐：PipelineTLM + TensorCoreTLM 占位值 → gpgpu-sim 精确值 + `is_placeholder_` = false + TC delegation 规则（Pipeline 对 TC 指令返回 0）— **Deferred: 需 gpgpu-sim 参考数据, Phase 4 Wave 2**

### Wave 2: 验证（~0.5d）

- [ ] **4.7** `test/test_kernel_launch_ptx_integration.cc`（条件链接 PTX-EMU，覆盖 G-D2/G-D3/G-D8）— **Deferred: Phase 4 Wave 2**
- [x] **4.8** `test/test_kernel_launch_tlm_ext.cc` 更新: fake driver mock 注入 + per-SM scoreboard 隔离测试 ✅ 2026-07-18（9 新测试用例, 含 MockPtxEmuDriver + AdvanceResult 全覆盖）
- [ ] **4.9** `test/python/test_gpgpu_sim_comparison.py`（G-D5 5 类 microbenchmark vs gpgpu-sim ±15%）— **Deferred: Phase 4 Wave 2**

### [CppTLM 端] 3 核心模块实例化方式

**Nota bene**: `main.cpp` 中 `IPtxEmuDriver::num_sms()` 查询 SM 数量 → 创建 N 个 `ScoreboardTLM` `std::unique_ptr` 实例 → 通过 `driver->inject_scoreboard(sm_id, sb)` 注入到 PTX-EMU 的每个 `SMContext`。PipelineTLM/TensorCoreTLM 无状态，可跨 SM 共享或 per-SM（成本相同）。**不参与 `ModuleFactory::instantiateAll()` JSON 拓扑解析**。

---

## P1 验收门

> **2026-07-18 Oracle P0 审查后更新归属**:
> - **CppTLM 端可独立验证**: G-D1, G-D4
> - **PTX-EMU 端已验证** ✅: G-D6, G-D7（PTX-7a 7 Mock + PTX-7b 4 集成 PASS, 210/210）
> - **双端联合验收**（需 Phase 4 Wave 0+1 完成后）: G-D2, G-D3, G-D5, G-D8

- [x] **G-D1** [CppTLM 端] 3 纯虚接口编译通过 ✅ 2026-07-18（16 单测 567 assertions PASS）
- [ ] **G-D2** [双端] `set_blocked_cycles_for_active()` 对 warp 内活跃线程正确设置延迟（需 Phase 4 Wave 2 `test_kernel_launch_ptx_integration.cc`）
- [ ] **G-D3** [双端] `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle（需统一 cycle 契约：1 CppTLM tick = 1 `GPUContext::exe_once()`）— **Deferred: Phase 4 Wave 2**
- [x] **G-D4** [CppTLM 端] 12 端点 `static_assert` + 签名级 `decltype` 验证 ✅ 2026-07-18 (commit `09b64b6`, negative test 证实断言生效)
- [ ] **G-D5** [双端] 5 类 microbenchmark vs gpgpu-sim ±15%（Phase 4 Wave 2）
- [x] **G-D6** [PTX-EMU 端] 3 setter 全 nullptr 零退化 ✅ 2026-07-18（PTX-7a `test_smcontext_injection.cpp` 已验证, `sm_context.cpp:32` nullptr guard）
- [x] **G-D7** [PTX-EMU 端] pipeline/TC nullptr → 回退 InstructionLatencyTable ✅ 2026-07-18（`test_step_b_set_blocked_cycles.cpp` 4-branch 回归锁, `sm_context.cpp:328`）
- [ ] **G-D8** [双端] exe_once stall → re-schedule → release → re-issue 完整循环（需 Phase 4 Wave 2 真实 `SMContext::exe_once()` 链路测试）

## P1 依赖关系图（Oracle P0 修订后）

```
P0 归档 + HSK-4/5 交付 + PTX-7a/7b 完成 ✅
                |
                v
┌───────────────────────────────────────────┐
│ Phase 1: Vendor + 3 核心模块 + 12 assert  │  ← ✅ 已完成 (2026-07-18)
│  G-D1, G-D4 验证                          │
└───────────────────┬───────────────────────┘
                    |
                    v
┌───────────────────────────────────────────┐
│ Phase 2: WarpScheduler Adapter            │  ← ✅ SKIPPED
└───────────────────┬───────────────────────┘
                    |
                    v
┌───────────────────────────────────────────┐
│ Phase 3: Async Seam → ✅ 已完成 (e69cd1d) │
└───────────────────┬───────────────────────┘
                    |
                    v
┌───────────────────────────────────────────┐
│ Phase 4: Oracle P0 修复 (~2.5d)           │
│  Wave 0: PTX-EMU 修复 (4.0.1-4.0.4)      │  ← 强前置
│  Wave 1: CppTLM 实现 (4.1-4.6)            │  ← P0-2→P0-1→P0-3 顺序
│  Wave 2: 验证 (4.7-4.9)                   │
│  G-D2, G-D3, G-D5, G-D8 验证              │
└───────────────────────────────────────────┘
                    |
                    v
                P1 归档
```

## 关键依赖

- **P0 归档**: ✅
- **PTX-EMU P1 接口 + 测试**: HSK-4/5 已交付 ✅ + PTX-7a/7b 已完成 ✅（210/210 PASS, committed `7df7d560`）
- **PTX-EMU 注入点 change 已归档**: ✅ (`e3070360`, ADR-0020 Active)
- **HSK-1/2/3**: 已在 P0 验证通过 ✅
- **Phase 4 新发现**: P0 Co-sim seam 未闭合（`poll_kernel` 立即完成、bridge path 不生成 PTX 任务）。Oracle 设计方案见 design.md §8。
- **后续**: P1 归档后 → 双端全量 D1-Full 验收（P0+P1 联合）
