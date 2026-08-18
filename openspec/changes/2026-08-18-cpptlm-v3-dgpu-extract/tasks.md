# cpptlm-v3-dgpu-extract: Tasks (P0-P4 Phase-Organized)

> **结构**: Phase 化任务清单 (P0 冻结 → P1 双轨 → P2 收敛 → P3 重构 → P4 物理删除)
> **配套**: [proposal.md](proposal.md)
> **Owner**: CppTLM Team
> **实施指南**: `/tmp/cpptlm-action-plan.md`（22 个操作步骤完整代码骨架 + commit 模板 + 验证命令）

---

## P0 冻结 (W1)

### T-P0-1: G-D4 static_assert 迁移至 `include/cudart/abi_guards.h` ✅ **已完成 (commit `fa2b3ec`)**

> ✅ **本任务已于 commit `fa2b3ec` 完成** (per ADR-X.15 §3 + 当前 HEAD 双重验证通过)。
> 保留任务记录作为审计追溯;W1 不再作为活跃工作项。

**Acceptance** (历史审计):
- [x] 新建 `include/cudart/abi_guards.h` (112 行)
- [x] 包含 17 条 static_assert (16 条 from `cpptlm_bridge.h:243-306` + 1 条 from `ptx_emu_driver.hh:27`)
- [x] 包含 CPPTLMBRIDGE_VERSION 锁 (值 = 2)
- [x] `include/cudart/cpptlm_bridge.h` 改为 `#include "cudart/abi_guards.h"` 并删除 16 条原 static_assert (保留 line 225 cudaStream_t 断言)
- [x] `include/tlm/gpu/ptx_emu_driver.hh` 改为 `#include "cudart/abi_guards.h"` 并删除 1 条原 static_assert

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

### T-P0-4: 路径与扩展名约定修正 [🔴 BLOCKING — 阻断构建]

> **触发**: Oracle 评审 C-NEW-1 [HIGH] — 现有 proposal/specs/tasks 引用 `tests/` 与 `.cpp`,与本仓 `test/` + `.cc` 惯例冲突;`.cpp` 文件不会被 `file(GLOB test_*.cc)` 捕获,静默不编译。

**Acceptance**:
- [ ] 全量修正 `tests/` → `test/`:
  - proposal.md:65-68 (4 处)
  - tasks.md:122 (`test_cpptlm_submodule_e2e_standalone.cpp` → `.cc`)
  - tasks.md:200-203 (4 个测试文件路径)
  - sm-executor.md:262-263 (`tests/mock_libptxemu_device.so` / `tests/mock_smock_pcie_device.so`)
- [ ] 全量修正 `.cpp` → `.cc`:
  - tasks.md:122 (`test_cpptlm_submodule_e2e_standalone.cpp` → `.cc`)
- [ ] 删除 `mock_smock_pcie_device.so` 引用 (per Oracle B.3 — DGpuBar/Doorbell/SQ 是纯 C++ 类,header mock 足够,做 .so 是过度工程)

**验证命令**:
```bash
grep -rn "tests/" openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/ 2>&1 | wc -l   # 必须 0
grep -rn "\.cpp" openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/ 2>&1 | wc -l  # 必须 0
```

**Commit**:
```bash
git commit -am "docs(cpptlm-v3-dgpu-extract): fix test path conventions (test/ not tests/, .cc not .cpp)

Per Oracle C-NEW-1:
- tests/ → test/ (CppTLM convention per test/AGENTS.md)
- .cpp → .cc (root CMakeLists file(GLOB test_*.cc) skips .cpp silently)
- Remove mock_smock_pcie_device.so reference (over-engineering; header mock is enough for DGpuBar/Doorbell/SQ)

Refs: ADR-X.15 §3, Oracle review C-NEW-1"
```

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

### T-P1-A4: mock_libptxemu_device.so 构建基础设施 [🟡 MEDIUM — 阻塞 P1 单测]

> **触发**: Oracle C-NEW-5 [MEDIUM] — `dlopen("libptxemu_device.so")` 裸名只搜 LD_LIBRARY_PATH,build 树内找不到;所有 SmExecutorImpl 测试间接依赖 mock .so 构建顺序。

**Acceptance**:
- [ ] 新建 `test/mock/mock_libptxemu_device.cc`(提供 8 个 dlsym 函数,返回可预测值 + `module_version()=2`)
- [ ] 新建 `test/mock/CMakeLists.txt`,声明 SHARED target `mock_libptxemu_device`(link `${CMAKE_DL_LIBS}`)
- [ ] 根 `CMakeLists.txt` 把 `test/mock/` 添加为 `add_subdirectory`
- [ ] `test/CMakeLists.txt`:`target_compile_definitions(cpptlm_tests PRIVATE MOCK_PTXEMU_SO="$<TARGET_FILE:mock_libptxemu_device>")`
- [ ] 测试代码用 `dlopen(MOCK_PTXEMU_SO, RTLD_NOW | RTLD_LOCAL)`(加 RTLD_LOCAL 防符号污染)
- [ ] 添加 `add_dependencies(cpptlm_tests mock_libptxemu_device)` 保证构建顺序

**验证**:
```bash
ls $<TARGET_FILE:mock_libptxemu_device>     # 存在
nm -D $<TARGET_FILE:mock_libptxemu_device> | grep -E "image_load|image_execute"  # 8 个符号
ctest -R "test_ptx_emu_submodule"           # PASS
```

---

## P2 收敛 (W4-6)

### T-P2-4: DGpuBoardTLM 包装模块 [🔴 CRITICAL — P1 交付物的仿真时间入口]

> **触发**: Oracle C-NEW-2 [CRITICAL] — DGpuBar/Doorbell/SQ/CompletionRing/SmExecutorImpl 按 spec 都是普通 C++ 类,既不能被 ModuleFactory::instantiateAll() 通过 JSON 实例化,也不能被 EventQueue 自动 tick 调度;同时 Oracle C-NEW-1 路径问题与 C-2/C-3/C-4(原评)的根因。
>
> **本任务前置**: P2 阶段首任务,必须在 T-P2-1 ISmExecutor 之前完成 — 否则 P3 阶段没有任何 JSON-driven 实例化路径,T-P3-4 JSON E2E 无法实施。

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/dgpu_board_tlm.hh` + `src/tlm/gpu/dgpu_board_tlm.cc`
- [ ] `DGpuBoardTLM : public ChStreamModuleBase`(对照 `KernelLaunchTLM` 模式,`chstream_register.hh:57`)
- [ ] 内部成员持有: `DGpuBar bar_` + `Doorbell doorbell_` + `SubmissionQueue sq_` + `CompletionRing cq_` + `SmExecutorImpl exec_` + `PtxEmuSubmodule ptx_emu_`
- [ ] 构造函数 `DGpuBoardTLM(name, EventQueue* eq, const DGpuBoardParams& params)`(params 含 `ptx_emu_dso`, `vram_size_mb`, `max_streams`, `sq_depth`)
- [ ] `tick() override` 由 EventQueue 调度,内部调 `sq_->tick()` 推进 SQ consumer
- [ ] `set_param()` 处理 JSON params(经 `ModuleFactory` 注入的 params schema 见 `dgpu-board.md` §7)
- [ ] Doorbell **集成到 DGpuBar 的 BAR0 MMIO offset 表**(per Oracle A.5 真实硬件语义): `BAR0 offset 0x1000-0x1FFF` = doorbell ring space,`DGpuBar::write_reg(0x1000+stream_id, tail)` → `Doorbell::ring`
- [ ] ChStream 端口连接 MemoryTLM/MemoryCluster(H2D DMA + VRAM backing)
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(DGpuBoardTLM)`(在 `KernelLaunchTLM` 附近)
- [ ] host 侧 helper 方法 `installKernelModule(vram_addr, size)` + `submitKernel(req)` + `write_reg(offset, value)`(模拟 UsrLinuxEmu driver 角色)

**验证**:
```bash
cmake --build build -j8
ctest -R "test_dgpu_board_tlm"  # 单元测试,确认 ChStreamModuleBase 派生与 register OK
./build/bin/cpptlm_tests --list-tests | grep -i dgpu   # 出现 [dgpu-board-tlm]
```

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
- [ ] 新建 `test/test_cpptlm_submodule_e2e_standalone.cc`
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

### T-P3-3: Mode A/B dual-rail E2E (Gate 4.7) [⚠️ **改写** — 见 proposal.md Gate #6]

**Acceptance**:
- [ ] 5 类 microbenchmark（per CppTLM #19 v3.0 RFC）：
  - GEMM
  - vector_add
  - FlashAttention
  - stencil
  - SpMV
- [ ] **功能等价 blocking** + **cycle 偏差 informational**(per Oracle A.4 — ±15% 无科学依据,应为 sanity bound 而非 equivalence)
- [ ] baseline 报告 `median/p95 cycle` 分布(每个 benchmark N≥30 runs,固定 seed),超 ±15% 触发人工 review 而非自动 fail
- [ ] 与 UsrLinuxEmu 联合测试（submodule 集成）

### T-P3-4: JSON-config E2E 测试 [🔴 HIGH — 用户主需求]

> **触发**: 用户要求 + Oracle Section C。**唯一能验证 ModuleFactory 参数解析 + StreamAdapter 注入 + 端口绑定的层**;单元/集成层做不到。

**前置**: T-P2-4 DGpuBoardTLM 必须完成。

**Acceptance**:
- [ ] 新建 `configs/dgpu_board_v1.json`(schema 见 `dgpu-board.md` §7):
  - 1 个 `DGpuBoardTLM` 模块(`ptx_emu_dso` 用 `@MOCK_PTXEMU_SO@` placeholder)
  - 1 个 `MemoryTLM` 模块作为 H2D DMA + VRAM backing
  - 1 个 `connections` 数组
- [ ] 根 CMakeLists 配置 `configure_file(configs/dgpu_board_v1.json.in configs/dgpu_board_v1.json @ONLY)` 注入 `${MOCK_PTXEMU_SO}`
- [ ] 纳入 `validate_topology` CMake target 扫描(`scripts/CMakeLists.txt:36`)
- [ ] 新建 `test/test_dgpu_board_from_config.cc`(完全照 `test_apu_soc_from_config.cc` 骨架)
- [ ] Catch2 标签 `[e2e][dgpu][from-config][P3]`
- [ ] 6 条验收 (per Oracle C.4):
  1. `validate_topology` 通过该 JSON
  2. `instantiateAll` 返回 true,`getInstance("dgpu0")->get_module_type() == "DGpuBoardTLM"`,StreamAdapter 已注入 — **JSON 层独有验证**
  3. H2D: 写 VRAM → `installImage` 返回 0 + image id ≠ 0
  4. Launch: N=4 stream 各 1 次 doorbell ring → `eq.run(budget)` 内 CompletionRing 收到 N 个 entry、status 全 0、`pending_count()` 归零、无死锁
  5. host_notify 触发 ≥1 次且最终 drain 完全(不断言 ==N,因 spurious wakeup 合法)
  6. 负面 SECTION: `ptx_emu_dso` 指向不存在路径 → `instantiateAll` 或模块构造按契约失败

**验证**:
```bash
cmake --build build --target validate_topology            # JSON 合法
ctest -R "test_dgpu_board_from_config" --output-on-failure  # 6 条 SECTION PASS
```

**Commit**:
```bash
git commit -am "feat(dgpu): JSON-config E2E test for dGPU board submodule (Oracle C)

Per Oracle review Section C + user requirement:
- configs/dgpu_board_v1.json: minimal DGpuBoardTLM + MemoryTLM topology
- test/test_dgpu_board_from_config.cc: 6-section Catch2 E2E
- Follows test_apu_soc_from_config.cc pattern
- Validates ModuleFactory params + StreamAdapter injection + port binding

Refs: Oracle C.4 acceptance criteria, ADR-X.15 §3"
```

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
- [ ] 删除 `test/test_memory_bridge.cc`
- [ ] 重写 `test/test_memory_bridge_poll.cc` → `test/test_completion_ring.cc`
- [ ] 保留 `test/test_kernel_launch_tlm_ext.cc` + 改符号（MockPtxEmuDriver → SQ/CQ doorbell mock）
- [ ] 拆分 `test/test_gpu_soc_perf.cc`（scoreboard perf 保留 + MemoryBridge poll perf → CompletionRing）

### T-P4-4: 808 测试验证 + Tag v3.0.0 [⚠️ **baseline 指标改写** — 见 proposal.md Gate #7]

**Acceptance**:
- [ ] `ctest -j8` 达到 baseline(Catch2 test cases 计数:`./build/bin/cpptlm_tests --list-tests | wc -l`)
- [ ] **baseline 指标明确定义**:Catch2 test cases 数(消除 proposal.md 808、tasks.md:231 ~148、AGENTS.md 764、实际 846 四处数字不一致)
- [ ] Git tag `v3.0.0` with commit message
- [ ] Push tag to remote

**Tag 命令**:
```bash
git tag -a v3.0.0 -m "cpptlm-v3-dgpu-extract P4 完成

- 11 项物理删除完成
- 4 测试文件处置完成
- v2.1.0 → v3.0.0 BREAKING bump
- baseline 测试验证 (Catch2 test cases)
- Mode A 彻底退役（Mode B 唯一路径）"

git push origin v3.0.0
```

### T-P4-5: AGENTS.md 全栈同步 [🔴 HIGH — DOC HYGIENE 硬规则违反]

> **触发**: Oracle 评级 C-6 [原评 MEDIUM → 升 HIGH] — 项目根 AGENTS.md "DOC HYGIENE (硬性)" 规定结构调整 PR 必含 AGENTS.md 同步,但提案新增 6 个 .hh 文件 + 1 个包装模块 + 1 个 mock .so 均未列入同步任务。

**Acceptance**:
- [ ] `include/tlm/AGENTS.md` 模块列表追加: `DGpuBar / Doorbell / SubmissionQueue / CompletionRing / PtxEmuSubmodule / ISmExecutor / DGpuBoardTLM / KernelLaunchTLM (重构)`
- [ ] `include/AGENTS.md` 注册宏体系表追加 `DGpuBoardTLM` 行(REGISTER_CHSTREAM 入口)
- [ ] `test/AGENTS.md` 标签表追加新测试标签:`[dgpu-bar] / [doorbell] / [submission-queue] / [completion-ring] / [ptx-emu-submodule] / [sm-executor] / [e2e][dgpu][from-config]`
- [ ] 根 `AGENTS.md` STRUCTURE 节追加: `include/tlm/gpu/dgpu_board_tlm.hh` + `src/tlm/gpu/dgpu_board_tlm.cc` + `configs/dgpu_board_v1.json` + `test/test_dgpu_board_from_config.cc` + `test/mock/mock_libptxemu_device.so`
- [ ] 根 `AGENTS.md` 命名空间惯例节(新增): `tlm/gpu/*.hh` → `namespace tlm`;`tlm/cluster/*.hh` → `namespace cpptlm::tlm`(per Oracle C-1 + include/tlm/AGENTS.md)
- [ ] `./scripts/test/docs_sync_check.sh --strict` PASS(0/365 不一致)

**验证**:
```bash
./scripts/test/docs_sync_check.sh --strict  # PASS
grep "DGpuBoardTLM" include/tlm/AGENTS.md include/AGENTS.md test/AGENTS.md   # 三处都有
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
| R5 | baseline 指标定义模糊(808/846/764/~148 四处不一致) | 中 | 中 | T-P4-4 改写为 Catch2 test cases 数统一 |
| R6 | HSK-6 ack 超时 | 中 | 中 | 14 天 + 超时无异议兜底 |
| R7 | TaskRunner tadr-308 实施延期（独立） | 低 | 低 | 不阻塞 CppTLM P0-P3 |
| R8 | CompletionRing host_notify 与 FenceRegistry 集成 | 中 | 中 | W6-8 联合测试 |
| **R9** | **非 SimObject 组件无仿真时间调度,P1 交付物无法 run** | **高** | **高** | **T-P2-4 DGpuBoardTLM 包装前置到 P2 首任务(per Oracle C-NEW-2)** |
| **R10** | **CompletionRing push/hook/pop 自死锁(spec 矛盾 C-NEW-3)** | **中** | **高** | **spec 裁决(push 释放锁后再调 hook)+ TSan 用例** |
| **R11** | **mock .so 构建顺序导致单测随机失败** | **中** | **中** | **T-P1-A4 target 依赖 + 双层 mock(header + .so)** |

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