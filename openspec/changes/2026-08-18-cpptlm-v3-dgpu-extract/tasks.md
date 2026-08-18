# cpptlm-v3-dgpu-extract: Tasks (P0-P4 Phase-Organized)

> **结构**: Phase 化任务清单 (P0 冻结 → P1 双轨 → P2 收敛 → P3 重构 → P4 物理删除)
> **配套**: [proposal.md](proposal.md)
> **Owner**: CppTLM Team
> **实施指南**: `/tmp/cpptlm-action-plan.md`（22 个操作步骤完整代码骨架 + commit 模板 + 验证命令）

---

## P0 冻结 (W1)

### T-P0-1: G-D4 static_assert 迁移至 `include/cudart/abi_guards.h` [🔴 BLOCKING for P4]

**Acceptance**:
- [ ] 新建 `include/cudart/abi_guards.h`
- [ ] 包含 17 条 static_assert（16 from `cpptlm_bridge.h:243-306` + 1 from `ptx_emu_driver.hh:27`）
- [ ] 包含 CPPTLMBRIDGE_VERSION 锁（值 = 2）
- [ ] `include/cudart/cpptlm_bridge.h` 改为 `#include "cudart/abi_guards.h"` 并删除 16 条原 static_assert
- [ ] `include/tlm/gpu/ptx_emu_driver.hh` 改为 `#include "cudart/abi_guards.h"` 并删除 1 条原 static_assert

**验证命令**:
```bash
grep -c "static_assert" include/cudart/abi_guards.h      # 必须 17
grep -c "static_assert" include/cudart/cpptlm_bridge.h   # 必须 0
grep -c "static_assert" include/tlm/gpu/ptx_emu_driver.hh # 必须 0
cmake --build build -j8 && ctest -j8  # baseline PASS 数相同
```

**Commit**:
```bash
git commit -am "feat(abi-guards): migrate 17 static_asserts from cpptlm_bridge.h:243-306 + ptx_emu_driver.hh:27 (HSK-6 P0-1)

Per HSK-6 commit 25e36f60 §P0-1 硬门禁:
- 新建 include/cudart/abi_guards.h 集中所有 static_assert
- cpptlm_bridge.h + ptx_emu_driver.hh 改为 include abi_guards.h (保留 ABI 真值源)
- 验证: grep -c static_assert 在 abi_guards.h = 17, 在源文件 = 0

Refs: PTX-EMU@ccd34155 (真相源), ADR-090 v2 §D5.2"
```

### T-P0-2: Mode A 冻结 — `[[deprecated]]` 标注

**Acceptance**:
- [ ] `MemoryBridge` 类（`memory_bridge.hh`）加 `[[deprecated("Mode A frozen, P4 物理删除 (HSK-6 ack 后)")]]`
- [ ] `IPtxEmuDriver` 接口（`ptx_emu_driver.hh:19`）加 `[[deprecated("同 MemoryBridge")]]`
- [ ] `DriverWrapper` 类（`ptx_emu_driver.hh:51`）加 `[[deprecated("由 PtxEmuSubmodule 替代")]]`

**验证**:
```bash
grep -c "\[\[deprecated" include/tlm/gpu/memory_bridge.hh     # 必须 1
grep -c "\[\[deprecated" include/tlm/gpu/ptx_emu_driver.hh   # 必须 2
cmake --build build -j8  # 现有调用方应触发 deprecation warning 而非 error
```

### T-P0-3: HSK-6 ack 跟踪 + weekly-checklist.md

**Acceptance**:
- [ ] 新建 `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/weekly-checklist.md`
- [ ] 列出 W1-W9 任务进度（checkboxes）
- [ ] 跟踪 PTX-EMU HSK-6 ack 收齐（deadline 2026-09-01）

---

## P1 双轨并行 (W1-3)

### T-P1-A1: PtxEmuSubmodule façade（轨 A）

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/ptx_emu_submodule.hh`（封装 8 ABI 函数签名）
- [ ] 新建 `src/tlm/gpu/ptx_emu_submodule.cc`（dlsym `libptxemu_device.so`）
- [ ] 实现 8 方法：`image_load / image_kernel_name / image_execute / image_unload / module_version / image_kernel_count / image_kernel_name_at / image_execute_named`
- [ ] 启动时 dlopen + 8 dlsym；任一失败抛 `std::runtime_error`
- [ ] `CMakeLists.txt` 注册 `cpptlm_ptx_emu_submodule` target（link `${CMAKE_DL_LIBS}`）

**验证**:
```bash
cmake --build build -j8
ctest -R "test_ptx_emu_submodule"  # 新增单元测试
# 启动时若 libptxemu_device.so 不可用，runtime_error 应抛出
```

### T-P1-A2: DGpuBar + Doorbell + SQ/CQ 骨架（轨 B）

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/dgpu_bar.hh`（PCIe BAR0 MMIO + BAR1 VRAM backing）
- [ ] 新建 `include/tlm/gpu/doorbell.hh`（SQ tail register）
- [ ] 新建 `include/tlm/gpu/submission_queue.hh`（NVMe 模型 SQ）
- [ ] 新建 `include/tlm/gpu/completion_ring.hh`（**v2 §D3.4 重设计**）
- [ ] `CompletionRing::push` + `set_host_notify` 替代 `AsyncCompletionAdapter::setCompletionCallback`
- [ ] 复用 `MemoryCluster` + `GpuNoC`（per v2 §D3.3，不新写）

### T-P1-A3: CMakeLists.txt 更新（target 注册）

**Acceptance**:
- [ ] 注册 `cpptlm_dgpu_board` 库（包含 DGpuBar/Doorbell/SQ/CQ）
- [ ] 注册 `cpptlm_ptx_emu_submodule` 库（独立 dlsym）
- [ ] link `${CMAKE_DL_LIBS}` 给 `cpptlm_ptx_emu_submodule`

---

## P2 收敛 (W4-6)

### T-P2-1: `ISmExecutor` 接口

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/is_m_executor.hh`
- [ ] 3 个纯虚方法：`installImage / dispatch / setCompletionCallback`
- [ ] `setCompletionCallback` 内部转调 `CompletionRing::set_host_notify`
- [ ] 命名空间 `tlm`

### T-P2-2: `SmExecutorImpl` 集成 `PtxEmuSubmodule`

**Acceptance**:
- [ ] 新建 `src/tlm/gpu/sm_executor_impl.cc`
- [ ] 持有 `PtxEmuSubmodule` 实例
- [ ] `installImage(vram_addr, size, ...)` 通过 `map_vram_to_host` 解 H2D DMA 后调 `image_load`
- [ ] `dispatch(image, args_vram_addr, ...)` 调 `image_execute`/`image_execute_named`

### T-P2-3: E2E 路径

**Acceptance**:
- [ ] 新建 `tests/test_cpptlm_submodule_e2e_standalone.cpp`
- [ ] 完整链路：cuModuleLoadData → IOCTL 0x27 → HAL #66 → CppTLM PtxEmuSubmodule → SQ enqueue → dispatch → CompletionRing push → host_notify
- [ ] 5 SECTIONs：image_load/dispatch/CompletionRing/Mode A 兼容/错误注入

---

## P3 重构 (W6-8)

### T-P3-1: `KernelLaunchTLM` 重构适配 `ISmExecutor`

**Acceptance**:
- [ ] `include/tlm/gpu/kernel_launch_tlm.hh` 持有 `ISmExecutor*`
- [ ] 旧接口保留（向后兼容到 P4）
- [ ] 新接口 `dispatch(DispatchParams)` 直接转发到 ISmExecutor
- [ ] 复用 `KernelLaunchRequest`（`kernel_launch_tlm.hh:30` 已存在）

### T-P3-2: CompletionRing push/host_notify 实施

**Acceptance**:
- [ ] 删除 `AsyncCompletionAdapter::setCompletionCallback` 入口
- [ ] 替换为 `SmExecutorImpl::dispatch` 完成时调 `CompletionRing::push + host_notify`
- [ ] host_notify hook 接入 UsrLinuxEmu FenceRegistry（跨仓集成点）

### T-P3-3: Mode A/B dual-rail E2E (Gate 4.7)

**Acceptance**:
- [ ] 5 类 microbenchmark（per CppTLM #19 v3.0 RFC）：
  - GEMM
  - vector_add
  - FlashAttention
  - stencil
  - SpMV
- [ ] cycle 数 ±15% tolerance（Mode A vs Mode B）
- [ ] 与 UsrLinuxEmu 联合测试（submodule 集成）

---

## P4 物理删除 (W8-9)

**前置条件**（全部必须 ✅）:
- [ ] T-P0-1 完成（grep -c 验证）
- [ ] T-P3-3 完成（dual-rail E2E PASS）
- [ ] PTX-EMU HSK-6 ack 收齐（commit hash 记录）
- [ ] UsrLinuxEmu Mode B 集成测试通过
- [ ] 808 测试 baseline 验证

### T-P4-1: 11 项物理删除

**Acceptance**（per UsrLinuxEmu `37a91b6` §D6.1）:
- [ ] 删除 `MemoryBridge` 类（`memory_bridge.{hh,cc}`）
- [ ] 删除 `IPtxEmuDriver` 接口（`ptx_emu_driver.hh:19`）
- [ ] 删除 `DriverWrapper` 类（`ptx_emu_driver.hh:51`）
- [ ] 删除 `g_ptx_emu_driver` 全局符号
- [ ] 删除 `cpptlm_set_driver` ABI 入口
- [ ] 删除 `ptx_emu_driver_shim.cc`
- [ ] 删除 vendored `cpptlm_bridge.h`
- [ ] 删除 vendored `pipeline_interface.h`
- [ ] 删除 vendored `scoreboard_interface.h`
- [ ] 删除 vendored `tensor_core_interface.h`
- [ ] 删除 `PtxEmuDriverApi` 布局锁（已在 P0-1 迁移）

**验证**:
```bash
grep -rn "MemoryBridge\|IPtxEmuDriver\|DriverWrapper\|cpptlm_bridge\|ptx_emu_driver_shim" include/ src/ 2>&1 | wc -l  # 必须 0
grep -rn "g_ptx_emu_driver\|cpptlm_set_driver" include/ src/ 2>&1 | wc -l  # 必须 0
cmake --build build -j8 && ctest -j8
```

### T-P4-2: CMakeLists.txt v2.1.0 → v3.0.0 BREAKING bump

**Acceptance**:
- [ ] `CMakeLists.txt` `project(cpptlm VERSION 3.0.0)`
- [ ] 新建 `include/cpptlm_version.h`（CPPTLM_VERSION_MAJOR=3 MINOR=0 PATCH=0）
- [ ] per ADR-088 §D6.2 BREAKING 流程记录

### T-P4-3: 4 测试文件处置

**Acceptance**（per UsrLinuxEmu `37a91b6` §D6.2）:
- [ ] 删除 `tests/test_memory_bridge.cc`
- [ ] 重写 `tests/test_memory_bridge_poll.cc` → `tests/test_completion_ring.cc`
- [ ] 保留 `tests/test_kernel_launch_tlm_ext.cc` + 改符号（MockPtxEmuDriver → SQ/CQ doorbell mock）
- [ ] 拆分 `tests/test_gpu_soc_perf.cc`（scoreboard perf 保留 + MemoryBridge poll perf → CompletionRing）

### T-P4-4: 808 测试验证 + Tag v3.0.0

**Acceptance**:
- [ ] `ctest -j8` 达到 808 测试 baseline
- [ ] Git tag `v3.0.0` with commit message
- [ ] Push tag to remote

**Tag 命令**:
```bash
git tag -a v3.0.0 -m "cpptlm-v3-dgpu-extract P4 完成

- 11 项物理删除完成
- 4 测试文件处置完成
- v2.1.0 → v3.0.0 BREAKING bump
- 808 测试 baseline 验证
- Mode A 彻底退役（Mode B 唯一路径）"

git push origin v3.0.0
```

---

## 测试目标

| 测试类型 | 数量（per ADR-088 §D6.2 同步扩展） |
|---|---|
| 现有 baseline | ~148 |
| + P0 新增（abi_guards 单元测试） | ~5 |
| + P1 新增（PtxEmuSubmodule + DGpu board 单元测试） | ~30 |
| + P2 新增（ISmExecutor + E2E） | ~25 |
| + P3 新增（KernelLaunchTLM + CompletionRing + dual-rail） | ~50 |
| + P4 新增（v3.0.0 集成测试 + tag） | ~50 |
| **总计** | **~808** |

---

## 风险登记表（per openspec change `5d9473a`）

| # | 风险 | 概率 | 影响 | 缓解 |
|---|---|:---:|:---:|---|
| R1 | P0-1 static_assert 迁移遗漏 | 中 | 高 | grep -c 自动化验证; PTX-EMU 真相源 8dc000ec 行数对比 |
| R2 | ANTLR4 误解 | 低 | 中 | HSK-6 §1.8 + Oracle session 确认 CppTLM 不在 ANTLR4 scope |
| R3 | PtxEmuSubmodule dlsym 失败 | 中 | 中 | 启动时 dlerror() 报错; Mode A 兜底 |
| R4 | DGpu board 实现复杂度高估 | 中 | 中 | W1-3 骨架先实现, P3 重构完善 |
| R5 | 808 测试 baseline 难达到 | 中 | 中 | P0-P4 累计 ~600 新测试, ADR-088 §D6.2 同步扩展 |
| R6 | HSK-6 ack 超时 | 中 | 中 | 14 天 + 超时无异议兜底 |
| R7 | TaskRunner tadr-308 实施延期（独立） | 低 | 低 | 不阻塞 CppTLM P0-P3 |
| R8 | CompletionRing host_notify 与 FenceRegistry 集成 | 中 | 中 | W6-8 联合测试 |

---

## 验收检查表

最终 v3.0.0 tag 前:

- [ ] T-P0-1, T-P0-2, T-P0-3 完成
- [ ] T-P1-A1, T-P1-A2, T-P1-A3 完成
- [ ] T-P2-1, T-P2-2, T-P2-3 完成
- [ ] T-P3-1, T-P3-2, T-P3-3 完成
- [ ] T-P4-1, T-P4-2, T-P4-3, T-P4-4 完成
- [ ] 全部 grep -c 验证 PASS
- [ ] 全部 ctest PASS（808 baseline）
- [ ] 跨仓协调：PTX-EMU HSK-6 ack 收齐 + UsrLinuxEmu submodule bump + Mode B E2E

---

**Cc**: CppTLM Team · UsrLinuxEmu Architecture Team · PTX-EMU Architecture Team

**Refs**:
- [proposal.md](proposal.md)
- [`/tmp/cpptlm-action-plan.md`](/tmp/cpptlm-action-plan.md)（22 操作步骤完整骨架）
- HSK-6 ack commit `369cf71`
- CppTLM openspec change `5d9473a`（起始 commit）

---

**起草**: Sisyphus (2026-08-18)
**Owner**: CppTLM Team