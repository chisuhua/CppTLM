# PTX-EMU 团队入口文档

> **用途**: PTX-EMU 团队从零开始理解 CppTLM ↔ PTX-EMU 协同仿真需求时的**单一入口**
> **更新日期**: 2026-07-14
> **CppTLM 维护者**: CppTLM Team (main branch)

---

## 0. 一句话总结

PTX-EMU 需做 **§2 MemoryBridge (F12b-LD)** + **§3 Compute Timing (D1-Full)** 共 **15 项协同改造任务（~9 人天）**，使 CppTLM 成为唯一时钟真相源，通过 3 注入点（Scoreboard/Pipeline/TC）接管 Compute timing，通过 MemoryBridge 接管 GLOBAL 访存 timing。

---

## 1. 必读文档（按顺序）

| 顺序 | 文档 | 行数 | 用途 | 优先级 |
|:---:|------|:---:|------|:---:|
| **①** | [`2026-07-14-ptxemu-comprehensive-modification-plan.md`](./2026-07-14-ptxemu-comprehensive-modification-plan.md) | 920 | **顶层任务书** — 9 个改造任务的完整规格、代码示例、验收标准 | 🔴 必读 |
| **②** | [`2026-07-03-ptxemu-phase8b-d1full-plan.md`](./2026-07-03-ptxemu-phase8b-d1full-plan.md) | 439 | D1-Full 接口对齐详细设计 — 3 注入点 + 6 模块接口 + Adapter Pattern（作为 §3 任务 #6~#9 的补充参考） | 🟡 参考 |
| **③** | [`2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`](./2026-07-01-f12b-ld-ptxemu-collaboration-sync.md) | 332 | 协作规范 — Timing Model §7、§13 D1-Full 协作节、双方协作流程 | 🟡 参考 |
| **④** | [`docs/adr/ADR-NV-02-phase8b-d1-strategy.md`](../../adr/ADR-NV-02-phase8b-d1-strategy.md) | 337 | 决策依据 — G-D1~G-D8 验收标准 + 风险 R1~R8 | 🟡 参考 |

**总阅读量**: 920 + 439 + 332 + 337 = **~2028 行**（~4 小时阅读 + 笔记时间）

**已弃用文档（不要读）**：
- ~~`2026-07-03-ptxemu-modification-task.md`~~ — 🗑️ 2026-07-14 已删除（被 comprehensive-plan.md §3 子集吸收）
- ~~`2026-06-30-f12-ptxemu-ldpreload-design.md`~~ — 🗑️ 2026-07-14 已删除（设计已被 comprehensive-plan.md §2.2/§3 吸收并扩展）
- ~~`f12-ptxemu-ldpreload-integration.md`~~ — 🗑️ 2026-07-14 已删除（计划已被 comprehensive-plan.md §4 取代）

---

## 2. 协同推进任务路径规划

### 2.1 阶段划分（4 个 P0~P3 阶段）

```
P0 (Week 1)        P1 (Week 2)         P2 (Phase 9+)     P3 (Week 3-4)
─────────────      ─────────────       ─────────────     ──────────────
F12b-LD            D1-Full             Async Seam        E2E Validation
MemoryBridge       Compute Timing      IAsyncCompletion  CUDA Kernel
(5.5 天)           (2.5 天)            (1 小时)          (1 周)

§2 MemoryBridge    §3 Compute          §4 Phase 9+       §5 Integration
#1-#5 + #C1-#C2    #6-#10 + #C3-#C5    #10 (in #7)       (E2E tests)
```

### 2.2 P0 阶段：F12b-LD MemoryBridge（5.5 天，🔴 关键路径）

**目标**：建立 CppTLM 作为 clock-of-truth 的基础设施

**PTX-EMU 端任务（5 项）**：

| 任务 | 文件 | 内容 | 工时 |
|:---:|------|------|:---:|
| **#1** | `include/cudart/cpptlm_bridge.h` (新) | CppTLMBridge 接口定义（含 `submit_kernel`/`poll_kernel`/`synchronize_stream`/`global_access`/`version` + `CPPTLMBRIDGE_VERSION` 宏 + `cudaStream_t` 宽度断言） | 45 min |
| **#2** | `src/cudart/cudart_sim.cpp` | `cudaLaunchKernel` 异步路径（完整参数传递 + PendingKernel 注册） | 1 day |
| **#3** | `src/cudart/cudart_sim.cpp` | `cudaStreamSynchronize`（按 `stream_id` 过滤 + 迭代器失效修复）+ `cudaDeviceSynchronize` + `cudaStreamCreate` | 1 day |
| **#4** | `src/ptxsim/instructions/memory.cpp` | `LdHandler`/`StHandler` 走 `global_access()` (timing-only)；数据仍读写 SimpleMemory | 0.5 day |
| **#5** | `CMakeLists.txt` | `libcpptlm_cudart.so` 集成构建（含 `CPPTLMBRIDGE_VERSION` 断言） | 0.5 day |

**CppTLM 端任务（2 项）**：

| 任务 | 文件 | 内容 | 工时 |
|:---:|------|------|:---:|
| **#C1** | `src/tlm/gpu/memory_bridge.{hh,cc}` (新) | MemoryBridge 实现（`version` + `submit/poll/synchronize/global_access` + `kernel_args` deep-copy） | 1 day |
| **#C2** | `src/tlm/gpu/kernel_launch_tlm.cc` | EventQueue 集成 + PTX-EMU 驱动 + KernelLaunchRequest + FIFO 调度 | 1 day |

**并行加速**：#1 可与 #C1 并行；#2/#3/#4/#5 可与 #C2 并行

### 2.3 P1 阶段：D1-Full Compute 注入（2.5 天）

**目标**：在 P0 clock-of-truth 基础上，注入 Scoreboard/Pipeline/TC 计算指令 timing

**PTX-EMU 端任务（5 项）**：

| 任务 | 文件 | 内容 | 工时 |
|:---:|------|------|:---:|
| **#6** | `include/ptxsim/{scoreboard,pipeline,tensor_core}_interface.h` (新) | 3 个纯虚接口（零 CppTLM 依赖） | 45 min |
| **#7** | `include/ptxsim/{sm_context,warp_context,async_completion_interface}.h` | 3 setter + 1 async_completion setter + get_dest_registers helper | 1 hr |
| **#8** | `src/ptxsim/core/warp_context.cpp` | `set_blocked_cycles_for_active()` 实现 | 30 min |
| **#9** | `src/ptxsim/core/sm_context.cpp` | `exe_once()` 四步注入 (Step A/B/C/D) | 1.5 day |
| **#10** | (已包含在 #7) | IAsyncCompletion stub | 0 |

**CppTLM 端任务（3 项）**：

| 任务 | 文件 | 内容 | 工时 |
|:---:|------|------|:---:|
| **#C3** | `include/tlm/gpu/*_adapter.hh` (新) | 4 个 Adapter (WarpScheduler/Scoreboard/Pipeline/TC) | 0.5 day |
| **#C4** | `include/tlm/gpu/{scoreboard,pipeline,tensorcore}_tlm.hh` | 3 核心模块 + `tlm::I*Internal` 接口 | 1 day |
| **#C5** | `include/tlm/gpu/async_completion_adapter.hh` (新) | AsyncCompletion 占位 Adapter | 15 min |

**并行约束**：
- ✅ **可并行**：#6/#7/#8 + C3/C4（接口头文件与 Adapter 独立开发）
- ❌ **不可并行**：#9 必须等 P0 的 `KernelLaunchTLM::tick()` 模型稳定后才能完整验证

### 2.4 P2 阶段：Phase 9+ Async Seam（1 小时）

**目标**：预埋 TMA async 完成回调 seam，避免 Phase 9+ 架构重构

**任务**：#10 已在 P1 #7 中完成（IAsyncCompletion 接口头文件 + SMContext stub）

**Phase 9+ 启用条件**：
- Phase 8.B 独立模式：`async_completion_ = nullptr`，无影响
- Phase 9+ TMA：替换 `AsyncCompletionAdapter` 占位实现为真实 DMA 引擎即可

### 2.5 P3 阶段：集成验证（1 周）

**目标**：E2E CUDA kernel 验证 + gpgpu-sim 对照

**测试场景**：

| 场景 | baseline | 验收 |
|------|:---:|:---:|
| GEMM (FP16, M=N=K=4096) | gpgpu-sim 700 GB/s | ±15% |
| FlashAttn (b=8, h=16, seq=512) | 470 GB/s | ±15% |
| vector_add (n=1024²) | 1176 GB/s | ±15% |
| stencil (3D 7-point, N=512³) | 940 GB/s | ±15% |
| sparse SpMV (10k×10k, 0.01) | 230 GB/s | ±15% |

---

## 3. 关键架构决策（必须理解）

### 3.1 控制流方向：CppTLM 主动驱动

```
CppTLM EventQueue ─→ KernelLaunchTLM::tick() ─→ PTX-EMU::exe_once()
                                          └─→ CrossbarTLM/CacheTLM/MemoryTLM::tick()
```

**PTX-EMU 不调用 `bridge->tick()`**。PTX-EMU 的 `cudaDeviceSynchronize`/`cudaStreamSynchronize` 通过 host-side 事件循环触发 CppTLM 推进。

### 3.2 时钟同步

```
CppTLM cur_cycle (唯一时钟真相)
    │
    ├─→ PTX-EMU SMContext::cycle_counter_ (相对计数器，与 cur_cycle 同步)
    └─→ PTX-EMU GPUContext::gpu_clock (同步)
```

### 3.3 数据所有权

```
PTX-EMU SimpleMemory ─── 数据存储（功能正确性）
CppTLM NoC/Cache       ─── 延迟查询（timing-only）
```

**Phase 8.B 语义**：LD/ST 数据在 SimpleMemory 立即可用；CppTLM `global_access()` 返回的 latency 仅用于设置 `blocked_cycles_remaining`。

### 3.4 Phase 9+ 演进路径

`IAsyncCompletion` 接口已在 SMContext stub 中预留，Phase 9+ TMA 实现时只需替换 Adapter 实现，**无需修改 PTX-EMU 任何接口**。

---

## 4. 任务依赖关系图

```
              PTX-EMU                              CppTLM
              ────────                            ────────

              ┌──── #1: CppTLMBridge 接口 ──────────────────┐
              │                                            │
              ↓  (并行启动)                              ↓  (并行启动)
                                                 ┌──── #C1: MemoryBridge ─────┐
                                                 └──────────────────────────────┘
              ┌──── #2: cudaLaunchKernel 异步路径 ────┐
              ├──── #3: cudaStreamSynchronize ─────────┤
              ├──── #4: memory.cpp LdHandler ──────────┤
              ├──── #5: libcpptlm_cudart.so 构建 ──────┤  ┌── #C2: KernelLaunchTLM ─┐
              └─────────────────────────────┘  └────────────────┘
                          │                          │
                          ▼                          ▼
              ╔═══════════════════════════════════════════════╗
              ║ §2 验证 (G-F1 ~ G-F5)：F12b-LD MemoryBridge  ║
              ╚═══════════════════════════════════════════════╝
                          │                          │
              ┌──── #6: 3 个纯虚接口 ────┐  ┌── #C3: 4 Adapter ──┐
              ├──── #7: SMContext setter ──┤  ├──── #C4: 3 核心模块 ─┤
              ├──── #8: set_blocked_cycles ─┤  └──── #C5: Async 占位 ─┘
              └──── #9: exe_once() 四步注入 ─┘
                          │                          │
                          ▼                          ▼
              ╔═══════════════════════════════════════════════╗
              ║ §3 验证 (G-D1 ~ G-D8)：D1-Full Compute 注入  ║
              ╚═══════════════════════════════════════════════╝
                          │
                          ▼
              ╔═══════════════════════════════════════════════╗
              ║ §5 验证：E2E CUDA kernel vs gpgpu-sim ±15%   ║
              ╚═══════════════════════════════════════════════╝
```

---

## 5. 验收标准速查

### 5.1 §2 F12b-LD MemoryBridge

| 编号 | 标准 | 验证 |
|:---:|------|------|
| G-F1 | `g_cpptlm_bridge == nullptr` 时 PTX-EMU 零退化 | `ptxemu_tests` 全 pass |
| G-F2 | 有 bridge 时 `cudaLaunchKernel` 立即返回（异步） | 单测 < 100μs |
| G-F3 | `global_access()` 延迟与 CppTLM NoC 路由延迟一致（误差 ≤ 5%） | 集成测试 |
| G-F4 | `cudaDeviceSynchronize` 正确等待所有 kernel 完成 | 单测 |
| G-F5 | F12b-LD 集成测试：GEMM kernel 经 CppTLM NoC 路由完成 | `cpptlm_tests [gpu][f12b]` |

### 5.2 §3 D1-Full Compute

| 编号 | 标准 | 验证 |
|:---:|------|------|
| G-D1 | 3 纯虚接口编译通过，无 CppTLM 头文件污染 | `ptxemu_tests` 全 pass |
| G-D2 | `set_blocked_cycles_for_active()` 正确设置延迟 | 单测 |
| G-D3 | `blocked_cycles_remaining` 与 CppTLM 独立模型差值 ≤ 1 cycle | 集成测试 |
| G-D4 | 4 Adapter `static_assert` 12 个端点 0-5 双向一致 | 编译期 |
| G-D5 | 5 类 microbenchmark vs gpgpu-sim ±15% | GEMM/FlashAttn/vector_add/stencil/sparse |
| G-D6 | 4 setter 全 nullptr 时 PTX-EMU 零退化 | 全量回归 |
| G-D7 | scoreboard/pipeline/TC 任意 nullptr 时回退到 InstructionLatencyTable | 单测 |
| G-D8 | exe_once() scoreboard stall → re-schedule → release → re-issue 完整循环无状态不一致 | PTX-EMU #6 + CppTLM Task 16 联合覆盖 |

### 5.3 Phase 9+ Async Seam

- `IAsyncCompletion` 接口已定义 + SMContext stub 已注入
- Phase 9+ TMA 实现时仅替换 `AsyncCompletionAdapter` 占位实现即可
- **Phase 8.B 无 Phase 9+ 相关验收标准**（独立验证 IAsyncCompletion 触发逻辑属于 Phase 9+ 范围）

---

## 6. 风险登记表（8 项，详见 ADR-NV-02 §5）

| # | 风险 | 状态 |
|:--:|------|:---:|
| R1 | 4 Adapter 转发开销 | 低（< 0.2% overhead） |
| R2 | PTX-EMU 版本变更破坏 Adapter | 中（`static_assert` 编译期拦截） |
| R3 | D1-Full 精度不达 ±15% | 低（D1-Full 优于 D1-Lite） |
| R4 | ~~PTX-EMU 拒绝新增注入点~~ | **已消除**（PTX-EMU 已确认） |
| R5 | Adapter 引入头文件依赖 | 中（`#ifdef HAS_PTXEMU` 条件编译） |
| R6 | PTX-EMU 8 项改造任务延迟阻塞 P1 | 中（**Adapter 解耦策略**：CppTLM 侧 Adapter 用 mock 接口先开发） |
| R7 | 跨仓库枚举值一致性（12 端点）漂移 | 中（CppTLM CI + PTX-EMU CI 双重 `static_assert`） |
| R8 | `exe_once()` `goto skip_warp_execution` 控制流状态不一致 | 低（PTX-EMU #6 + CppTLM Task 16 联合覆盖 + 混沌测试） |

---

## 7. 协作流程图

```
PTX-EMU Team                              CppTLM Team
     │                                          │
     ├─ #0 (30 min): 接口对齐确认 ──────────────→│ (审阅 #1, 确认枚举映射)
     │   - 枚举值 0-5 双向一致                     │   - 接受 4 Adapter 接口
     │   - kernel_id 唯一性保证                    │
     │                                          │
     ├─ #1 (45 min): 接口定义 ──────────────────→│ (审阅签名)
     │                                          ├─ #C1 (1 day): MemoryBridge ───→
     │                                          │
     ├─ #2-#5 (3 days): cudaLaunchKernel + ──→   │
     │   cudaStreamSynchronize + memory.cpp +   │
     │   CMakeLists.txt                          ├─ #C2 (1 day): KernelLaunchTLM ─→
     │                                          │
     │←── #5 集成完成 ───────────────────────────┤
     │                                          │
     ╔════ §2 验证 G-F1 ~ G-F5 ════════════════╗
     ║  P0 阶段交付：clock-of-truth 基础     ║
     ╚════════════════════════════════════════╝
     │                                          │
     ├─ #6-#9 (2 days): 3 接口 + SMContext ───→ │
     │   setter + warp_context + exe_once()    │
     │                                          ├─ #C3-C5 (1.5 days): 4 Adapter + 3 模块
     │                                          │   + Async 占位
     │                                          │
     │←── #9 注入完成 ──────────────────────────┤
     │                                          │
     ╔════ §3 验证 G-D1 ~ G-D8 ════════════════╗
     ║  P1 阶段交付：D1-Full Compute 注入    ║
     ╚════════════════════════════════════════╝
     │                                          │
     ├──── §5 联合验证 (1 周) ─────────────────→│
     │                                          │
     ╔════ 5 类 microbenchmark ±15% ═══════════╗
     ║  P3 阶段交付：E2E CUDA kernel 验证    ║
     ╚════════════════════════════════════════╝
```

---

## 8. 联系人

- **CppTLM Team**: CppTLM 仓库 `main` 分支
- **本文档**: `docs/superpowers/specs/PTX-EMU-README.md`
- **综合任务书**: `docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md`
- **D1-Full 设计**: `docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md`
- **协作规范**: `docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md`
- **ADR**: `docs/adr/ADR-NV-02-phase8b-d1-strategy.md`

---

## 9. 快速命令参考

```bash
# 阅读顺序（4 个核心文档，按此顺序）
cat docs/superpowers/specs/2026-07-14-ptxemu-comprehensive-modification-plan.md  # 920 行
cat docs/superpowers/specs/2026-07-03-ptxemu-phase8b-d1full-plan.md             # 439 行
cat docs/superpowers/specs/2026-07-01-f12b-ld-ptxemu-collaboration-sync.md      # 332 行
cat docs/adr/ADR-NV-02-phase8b-d1-strategy.md                                   # 337 行

# PTX-EMU 验证（完成 #5 后）
./build/bin/ptxemu_tests                              # 全 pass（G-F1 验收）
./build/bin/cpptlm_tests [gpu][f12b]                  # 全 pass（G-F5 验收）

# CppTLM 验证（完成 §2 + §3 后）
./build/bin/cpptlm_tests [gpu]                        # 全 pass
python -m pytest test/python/test_gpgpu_sim_comparison.py  # 5 类 ±15%
```

---

*入口文档最后更新: 2026-07-14 · 综合任务书 v1.1 · D1-Full 策略已确认*