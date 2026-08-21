# cpptlm-ptxemu-public-device-api: Design

## Context

S1 (`openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`) 在 `c2038a93` pin 下交付了 PTX-EMU 深度集成基础设施，通过 `cmake/PTXEmuCore.cmake` shim + `ptx_emu_submodule_mvp.cc` / `cuda_core_adapter_mvp.cc` 编译防火墙实现了 OFF 850/850 + ON 891/891 测试全 PASS。但 S1 当前**不满足**用户要求的封装级别：
- facade.cc 直接 include 12 个 PTX-EMU 实现头，完整持有 `GPUContext`/`SMContext`/`WarpContext`/`ThreadContext` 类型
- adapter.cc 调 `sm->exe_once()`/`set_scoreboard()`/`thread->get_state()` 等内部 API
- 编译防火墙只是约定级封装，头文件漂移照样炸 CppTLM

PTX-EMU 远端 main 已主动清理 `cleanup-cudart-cpptlm-bridge-coupling` Phase 1-4（1018 行删除），HSK-6 已接受桥梁废弃。HSK-3 (ExternalProject_Add) 约束的"PTX-EMU 消费 CppTLM"方向已被 HSK-6 废止，新方向需要新 HSK-8 handshake。

Oracle 4 轮咨询（session `ses_fdb70164bffe2vBN71uiaV90aY`）输出**选项 X**（PTX-EMU 端公共设备 API `IPtxEmuDevice`）作为唯一忠实落地方案。本 change 实施该方案。

## Goals / Non-Goals

**Goals:**
- PTX-EMU 端维护 `ptxemu/device_api.h` 公共契约（CppTLM 唯一 include 入口）
- PTX-EMU 内部头通过 CMake `target_include_directories(ptxemu_core PRIVATE ...)` 封装，CppTLM 编译路径中不可见
- CppTLM 端 facade.cc / adapter.cc 仅 include `ptxemu/device_api.h`，调用抽象方法
- 跨仓协调：PTX-EMU PR（独立）+ CppTLM bump PR（绑定）+ HSK-8 handshake（书面锁定）
- 桥接残留簇（stub.cc / vendored cpptlm_bridge.h / MemoryBridge / PtxEmuDriverShim）一次性清除
- 测试 fixture 化：facade 测试改用 PTX 源码字符串而非程序化 IR 构建

**Non-Goals:**
- 不修改 PTX-EMU 内部实现（仅新增 `device_api.h` + 薄适配层）
- 不修改 CppTLM 上层 API（`PtxEmuSubmoduleMVP` 和 `CudaCoreAdapter` 对上层接口不变）
- 不引入新的 ABI 面（HSK-4 vendored 3 接口 IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 已存在，本 change 复用其位置）
- 不重写 S1 已验证的测试逻辑（仅改 fixture 构建方式 + 断言类型）
- 不在本 change 内执行 cpptlm-v3-dgpu-extract 的其他 P0-P4 任务
- 不重做 S1（保持 S1 已 archive 状态，本 change 是后续工作线）

## Decisions

### Decision 1: PTX-EMU 端新增 `ptxemu/device_api.h` 公共头（唯一入口）

**Why**: 用户要求 "CppTLM 只需要 header 的源代码，cpp 代码可以不用暴露给 CppTLM"。这是 PIMPL 模式的标准实现：公共头声明抽象接口，实现细节封装在 .so/.a 内。

**Alternatives considered**:
- **W (编译防火墙)**：约定级封装，头文件漂移照样炸 CppTLM。与用户诉求"程度差异不是本质满足"。
- **Z (PTX-EMU 内部类改纯虚基类)**：让 PTX-EMU 为自己的内部热路径类付虚调用税 + 全仓重构，侵入性远超收益。
- **Y (dlopen + dlsym)**：解决部署期解耦，引入符号解析失败只能在运行时发现等问题；与 add_subdirectory 构建模型冲突。

**Chosen**: PTX-EMU 端新增 `ptxemu/device_api.h` (~200 行) + `src/ptxemu/device_api_impl.cc` (~400 行薄适配层)。

### Decision 2: CMake `add_subdirectory` + PUBLIC/PRIVATE include 拆分

**Why**: A3 (add_subdirectory) 是新方向下源码级紧耦合（facade.cc 直接 include 头）的唯一自然选择。PUBLIC/PRIVATE include 拆分是"cpp 不暴露"的强制机制——`target_include_directories(ptxemu_core PRIVATE ${PTXEMU_SRC})` 让 CppTLM 编译时看不到 PTX-EMU 内部头。

**Alternatives considered**:
- **A1 (find_package)**: install/export/package config 是为二进制分发设计的，增加 CI 安装步骤和前缀管理成本，换来的版本隔离用 submodule pin 已实现。
- **A2 (ExternalProject_Add)**: build 时而非 configure 时构建，目标对 `target_link_libraries` 不可见，需手工维护 imported library 路径，对静态库场景脆弱。HSK-3 选它是因为旧方向下 CppTLM 对 PTX-EMU 是外部产物；新方向双方源码级紧耦合。

**Chosen**: `add_subdirectory(external/PTX-EMU)` + `target_link_libraries(cpptlm_core PUBLIC ptxemu_core)` + `ptxemu_core` 的 `target_include_directories` 拆分 PUBLIC/PRIVATE。

### Decision 3: warp 句柄从指针改为 `uint32_t warp_id`

**Why**: `WarpContext*`/`SMContext*` 是 PTX-EMU 内部类的完整类型指针，跨抽象接口传递意味着 CppTLM 必须看到这些类型。改为 `uint32_t warp_id` 句柄后，CppTLM 只看到整数，内部寻址在 impl 内完成。

**Alternatives considered**:
- **不透明指针** (`struct WarpHandle*` + `delete` 走工厂)：增加一次指针解引用开销，调试时看不到内部状态。

**Chosen**: `using WarpHandle = uint32_t;` + IPtxEmuDevice 方法参数带 `warp_id` + `int lane`。

### Decision 4: 模板方法改为显式重载

**Why**: S1 facade `read_register<T>` 是模板，跨抽象接口（虚函数）不能传递模板参数——虚函数表不含模板实例化。必须改为非模板的显式重载（`read_register_u32`/`read_register_u64`）。

**Chosen**: `IPtxEmuDevice::read_register_u32` + `read_register_u64` + `read_register_f32` + `read_register_f64`（或 `void* + size` 通用接口）。

### Decision 5: `StatementContext` 晋升为公共 IR 头

**Why**: `StatementContext` 是 S1 facade.hh 的公开值类型，`sizeof` 可见性要求无法绕过。如果不晋升，要么 (a) 改为不透明句柄（增加解引用 + 调试不便），要么 (b) 接受模板限制。晋升为 `ptxemu/ir/statement.h` 是最小侵入方案。

**Alternatives considered**:
- **不透明 `StatementHandle`**: 失去值类型语义，facade.hh 公开 API 需改写。
- **保留程序化 IR 构建**: S1 facade_decode 测试可用，但 fixture 测试无法用 PTX 源码字符串。

**Chosen**: `ptxemu/ir/statement.h` 晋升为公共 IR 头（纯数据，不含实现）。**前置硬校验**: PTX-EMU 需确认 `StatementContext` 的传递 include 闭包全部可公开（若闭包含实现头，降级到不透明句柄方案）。

### Decision 6: PTX-EMU 内部 `EXE_STATE` ↔ 公共 `ThreadState` static_assert 锁

**Why**: PTX-EMU 内部 `EXE_STATE{IDLE,RUN,EXIT,BAR_SYNC}` 演进后公共 `ThreadState` 失真，DTO 映射漂移风险。沿用 `fa2b3ec` abi_guards 模式，在 PTX-EMU 内部 impl 内做 `static_assert` 强制两枚举同构，**不进公共 ABI**。

**Chosen**: impl 内的 `static_assert(static_cast<uint32_t>(ptxemu::ThreadState::kIdle) == static_cast<uint32_t>(ptxsim::EXE_STATE::IDLE))` 系列。

### Decision 7: 桥接残留簇一次性清除

**Why**: stub.cc / vendored cpptlm_bridge.h / MemoryBridge / PtxEmuDriverShim 在 S1 是过渡产物。升级到 PTX-EMU origin/main 后：
- `ptx_emu_submodule_mvp.cc` 不再直接 include `memory.cpp` 相关的实现头
- PTX-EMU 端 `cudart_sim.cpp` 不再调用 `g_cpptlm_bridge` / `cpptlm_set_driver`
- `MemoryBridge` 类继承的 `CppTLMBridge` 基类随 vendored bridge.h 消失而消失

这意味着删除从"可选清理"变成"编译必需"。一次性清除避免半状态。

**Chosen**: 在 CppTLM bump PR 内**单 PR 三件事**：(1) submodule bump；(2) `add_subdirectory` + 删除 246 行 cmake shim；(3) 删除桥接残留簇 5 项。

### Decision 8: 跨仓协调顺序（PTX-EMU PR → CppTLM bump PR）

**Why**: CppTLM bump PR 依赖 PTX-EMU PR 提供的 `ptxemu_core` target。若 PTX-EMU PR 排期延迟，CppTLM 阻塞。

**Mitigation**:
- HSK-8 先书面锁定接口（`ptxemu_core` target 名 + `PTXEMU_API_VERSION` + PUBLIC include 路径）
- CppTLM 侧可先在本地用 PTX-EMU 分支 commit 验证 bump PR（不必等主分支合入）
- PTX-EMU PR 应基于 `origin/main`（post-deletion），不能基于 c2038a93（仍引用 g_cpptlm_bridge，库目标无法独立链接）

## Risks / Trade-offs

### Risk 1: API 首版遗漏（DTO 粒度与 S1 行为有偏差）

**Severity**: Medium
**Mitigation**: API 面严格从 S1 facade.cc / adapter.cc 现有调用点**机械抽取**（禁止设计性发挥）；迁移后 891 套件逐条对拍行为；缺失字段通过反向故意失败测试暴露。

### Risk 2: DTO 映射漂移（PTX-EMU 内部 enum 演进后公共 DTO 失真）

**Severity**: Medium
**Mitigation**: Decision 6 的 `static_assert` 锁 + PTX-EMU 侧契约测试覆盖每个枚举值。

### Risk 3: 跨仓时序死锁（CppTLM bump 依赖 PTX-EMU PR 先合入）

**Severity**: Medium
**Mitigation**: Decision 8 的 HSK-8 锁定 + 本地分支 commit 验证。

### Risk 4: pin 回滑（bump 后若退回 `09786635` 之前，`ptxemu_core` 不存在）

**Severity**: Low
**Mitigation**: 顶层 CMakeLists.txt 加版本守卫：
```cmake
if(NOT TARGET ptxemu_core)
    message(FATAL_ERROR "submodule 过旧, 需 bump 到 >= HSK-8 commit (ptxemu_core target 不存在)")
endif()
```

### Risk 5: CMake 目标污染（add_subdirectory 把 PTX-EMU 的 options/tests 带入 CppTLM 构建）

**Severity**: Low
**Mitigation**: PTX-EMU 端强制 `if(PROJECT_IS_TOP_LEVEL)` 隔离自身 tests/tools + `option(PTXEMU_BUILD_TESTING OFF)` 默认值 + CppTLM 端 configure 时断言 `cpptlm_tests` 编译时间不回归。

### Risk 6: 双层 facade 调试断层（出问题要穿两层才能到 PTX-EMU 内部）

**Severity**: Low
**Mitigation**: `IPtxEmuDevice` 预留 `debug_dump_state()` 逃生口（debug-only，不进 ABI 锁）。

### Risk 7: `StatementContext` 晋升受阻（闭包含实现头）

**Severity**: Low
**Mitigation**: 执行前硬校验（参见 Decision 5）。降级方案：不透明 `StatementHandle` + `decode` 直提交字节流，facade_decode 测试随之简化。

## Migration Plan

### Phase 1: HSK-8 Handshake（CppTLM 发起，~1h）

- 新建 `docs/superpowers/specs/2026-XX-XX-hsk-8-ptxemu-public-api.md`
- 锁定：`ptxemu_core` target 名 + PUBLIC include 路径 + `PTXEMU_API_VERSION` 语义 + "PTX-EMU 内部重构不影响 device_api.h"承诺
- CppTLM 侧 commit，PTX-EMU 侧 ack commit

### Phase 2: PTX-EMU PR（独立仓库，~Short~Medium 1-2d）

- 基于 `origin/main`（post-deletion，commit `09786635` 之后）
- 新增 `add_library(ptxemu_core STATIC ...)` 显式源清单
- `target_include_directories(ptxemu_core PUBLIC include/ptxemu)` + `PRIVATE ${PTXEMU_SRC}`
- 新增 `include/ptxemu/device_api.h` + `include/ptxemu/ir/statement.h` + `src/ptxemu/device_api_impl.cc`
- 新增 `tests/build_cpptlm_consume/consumer_smoke.cc` + `drift_check.cmake`
- `if(PROJECT_IS_TOP_LEVEL)` 隔离自身 tests + `option(PTXEMU_BUILD_TESTING OFF)`
- PTX-EMU CI 全绿后合入

### Phase 3: CppTLM bump PR（~Short 1-2d）

- submodule bump 到 Phase 2 合入 commit
- 根 `CMakeLists.txt`：`add_subdirectory(external/PTX-EMU)` + 版本守卫 + `target_link_libraries(cpptlm_core PUBLIC ptxemu_core)`
- 删除 `cmake/PTXEmuCore.cmake`（246 行）
- 删除 `src/tlm/gpu/ptx_emu_bridge_stub.cc` + `include/cudart/cpptlm_bridge.h` + `include/tlm/gpu/memory_bridge.hh` + `src/tlm/gpu/memory_bridge.cc` + `src/tlm/gpu/ptx_emu_driver_shim.cc` + `test/ptxemu_link_smoke.cc`
- `src/tlm/gpu/ptx_emu_submodule_mvp.cc/.hh` 重写：12 include → 1，指针→uint32 句柄
- `src/tlm/gpu/cuda_core_adapter_mvp.cc/.hh` 重写：5 include → 1，调用点替换
- `include/cudart/abi_guards.h` 拆分：16 条 enum 断言迁至 `include/cudart/hsk4_abi_guards.h`，删除 `sizeof(PtxEmuDriverApi)==64`
- 12 测试改写
- 新增 include 防火墙 grep 门禁（除 facade/adapter .cc 外任何 `ptxsim/|ptx_ir/|memory/|register/` 即 configure 失败）

### Phase 4: 验证（~30min）

跑 [验证标准](../proposal.md#验证标准-bump-pr-合入前必跑) 8 条。

### Phase 5: 文档同步（~30min）

- `AGENTS.md` HSK 链路段追加 HSK-8
- `docs/superpowers/specs/2026-08-18-hsk-6-response.md` 状态段追加：桥接清理提前到本 change（替代原 v3.0 P4 计划）
- `openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/` 加注 "本 change 为过渡阶段产物，未来 HSK-8 + bump PR 接管"
- `roadmap-meta` 更新
- `scripts/test/docs_sync_check.sh --strict` PASS

## Open Questions

- **Q1: PTX-EMU 端是否愿意接受步骤 2 PR**? 这是触发条件（PTX-EMU 开发者接受 HSK-8 并愿意维护库目标）。若拒绝，整个 change 不执行，fallback 到 S1 现状。
- **Q2: `StatementContext` 晋升公共 IR 头是否可行**? 需 PTX-EMU 端确认其传递 include 闭包全部可公开。若不可，降级到 `StatementHandle` 不透明句柄方案。
- **Q3: 测试 fixture 化后，CppTLM 是否接受 facade_decode roundtrip 测试迁至 PTX-EMU 端**? 这意味着 CppTLM 端该测试改为 golden-bytes decode 测试。如不接受，需在 PTX-EMU 端保留 roundtrip + CppTLM 端用 PTX 字节流端到端测试。
- **Q4: HSK-8 handshake 文档的最终签发人**? CppTLM 发起 + PTX-EMU ack，需双方 maintainer 签字（commit author + reviewer）。
- **Q5: PTX-EMU 仓库是否已有 `if(PROJECT_IS_TOP_LEVEL)` 隔离模式**? 需调研 PTX-EMU 自身 CMakeLists.txt 是否已支持被 add_subdirectory 调用而不污染调用方。