# cpptlm-ptxemu-public-device-api: Tasks

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`specs/`](../specs/)
> **结构**: 跨 2 仓 2 PR 任务清单 · **Owner**: CppTLM Team (Sisyphus) + PTX-EMU Team
> **关联**: S1 change (`openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`) 必须先 archive
> **Handshake**: HSK-8 (CppTLM 发起 + PTX-EMU ack)

---

## 0. 前置条件

- [ ] 0.1 **S1 已 archive**（commit 聚合 13 新文件 + 6 修改文件 @ c2038a93, OFF 850/850 + ON 891/891 全 PASS）
- [ ] 0.2 **PTX-EMU 开发者接受 HSK-8 契约**（书面 ack commit）
- [ ] 0.3 **`StatementContext` 传递闭包审计通过**（PTX-EMU 端确认可晋升为公共 IR 头；或确认降级到 `StatementHandle` 不透明句柄方案）

---

## 1. HSK-8 Handshake（CppTLM 发起, ~1h）

- [x] 1.1 新建 `docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md` 锁定：`ptxemu_core` target 名 + PUBLIC include 路径 + `PTXEMU_API_VERSION` 语义（初始 1）+ "PTX-EMU 内部重构不影响 device_api.h"承诺 + 实施工作量（Short~Medium 1-2d）+ 跨仓协调顺序（PTX-EMU PR 合入 → CppTLM bump PR）
- [x] 1.2 CppTLM 侧 commit（spec 文档签发 — pending 当前 commit）
- [ ] 1.3 PTX-EMU 侧 ack commit（PTX-EMU maintainer 在 PR 中 +1 同意）

---

## 2. PTX-EMU 仓库 PR（独立仓库, ~Short~Medium 1-2d）

**前置**: PTX-EMU 仓库基于 `origin/main`（commit `09786635` 或更新）开分支

- [ ] 2.1 新建 `include/ptxemu/device_api.h`（~200 行，定义 `IPtxEmuDevice` 抽象接口 + `DeviceConfig`/`WarpStatus`/`LaneStatus`/`ThreadState` + 工厂函数 `create_device/destroy_device` + `PTXEMU_API_VERSION` 宏）
- [ ] 2.2 新建 `include/ptxemu/ir/statement.h`（晋升 `StatementContext` 为公共 IR 头，纯数据，C++17 兼容）
- [ ] 2.3 新建 `src/ptxemu/device_api_impl.cc`（~400 行薄适配层，封装 `GPUContext`/`SMContext`/`WarpContext`/`ThreadContext`/`HardwareMemoryManager`/`RegisterBankManager`，做 DTO 映射）
- [ ] 2.4 在 `src/ptxemu/device_api_impl.cc` 加 `static_assert` 锁：`ptxemu::ThreadState` 与 `ptxsim::EXE_STATE` 同构
- [ ] 2.5 新建 `tests/build_cpptlm_consume/consumer_smoke.cc`（最小 consumer exe，仅 include `ptxemu/device_api.h` + 调用 `create_device/destroy_device`）
- [ ] 2.6 新建 `tests/build_cpptlm_consume/drift_check.cmake`（GLOB drift 门禁，比较 `ptxemu_core` target 源文件 vs `git ls-files`）
- [ ] 2.7 PTX-EMU 端 `CMakeLists.txt` 新增 `add_library(ptxemu_core STATIC ${PTXEMU_CORE_SOURCES})`（显式列源，禁 GLOB）
- [ ] 2.8 PTX-EMU 端 `CMakeLists.txt` 设置 `target_include_directories(ptxemu_core PUBLIC include/ptxemu)` + `PRIVATE ${PTXEMU_SRC}`（内部头封装）
- [ ] 2.9 PTX-EMU 端 `CMakeLists.txt` 在 `if(PROJECT_IS_TOP_LEVEL)` 块内放置自身 tests/tools（避免 add_subdirectory 调用时污染调用方）
- [ ] 2.10 PTX-EMU 端 `CMakeLists.txt` 新增 `option(PTXEMU_BUILD_TESTING OFF)`（默认 OFF）
- [ ] 2.11 PTX-EMU CI 配置：跑 `consumer_smoke` + `drift_check` + 既有 PTX-EMU tests（确保 `PTXEMU_BUILD_TESTING=ON` 时 PTX-EMU 自身 CI 不回归）
- [ ] 2.12 PTX-EMU PR 提交 + 评审 + 合入（PTX-EMU maintainer 决策）

---

## 3. CppTLM bump PR（~Short 1-2d, 单 PR 三件事）

**前置**: Phase 2 PTX-EMU PR 已合入 main

- [ ] 3.1 submodule bump：`external/PTX-EMU` 升级到 Phase 2 合入 commit
- [ ] 3.2 根 `CMakeLists.txt`：删除 `include(cmake/PTXEmuCore.cmake)`，新增 `if(CPPTLM_WITH_PTX_EMU) ... add_subdirectory(external/PTX-EMU) ... endif()`
- [ ] 3.3 根 `CMakeLists.txt`：新增版本守卫 `if(NOT TARGET ptxemu_core) message(FATAL_ERROR "submodule 过旧, 需 bump 到 >= HSK-8 commit") ... endif()`
- [ ] 3.4 根 `CMakeLists.txt`：`target_link_libraries(cpptlm_core PUBLIC ptxemu_core)`（保留原 PUBLIC 链接语义）
- [ ] 3.5 根 `CMakeLists.txt`：新增 include 防火墙 grep 门禁（扫描 `src/` 与 `test/` 下所有 `.cc/.hh`，除 `ptx_emu_submodule_mvp.cc` + `cuda_core_adapter_mvp.cc` 外任何 include `ptxsim/`/`ptx_ir/`/`memory/`/`register/` 即 FATAL_ERROR）
- [ ] 3.6 删除 `cmake/PTXEmuCore.cmake`（全 246 行）
- [ ] 3.7 删除 `src/tlm/gpu/ptx_emu_bridge_stub.cc`
- [ ] 3.8 删除 `include/cudart/cpptlm_bridge.h`
- [ ] 3.9 删除 `include/tlm/gpu/memory_bridge.hh` + `src/tlm/gpu/memory_bridge.cc` + `src/tlm/gpu/ptx_emu_driver_shim.cc`
- [ ] 3.10 删除 `test/ptxemu_link_smoke.cc`
- [ ] 3.11 删除 vendored HSK-4 三接口头：`include/cudart/scoreboard_interface.h` + `include/cudart/pipeline_interface.h` + `include/cudart/tensor_core_interface.h`（改由 PTX-EMU PUBLIC include 传递）
- [ ] 3.12 `src/tlm/gpu/ptx_emu_submodule_mvp.cc/.hh` 重写：12 include → 1（`ptxemu/device_api.h`），内部类指针 → `uint32_t warp_id` 句柄，模板方法 → 显式重载
- [ ] 3.13 `src/tlm/gpu/cuda_core_adapter_mvp.cc/.hh` 重写：5 include → 1（`ptxemu/device_api.h`），调用点替换（`sm->exe_once()` → `device->step_once()`，3 个 `set_*` → `attach_timing()`，指针 → 句柄）
- [ ] 3.14 `include/cudart/abi_guards.h` 拆分（per HSK-6 P0-1 `fa2b3ec`）: (a) 16 条 enum 断言（`PipelineId` 6 + `TcPrecision` 6 + `is_same_v` 4，from `cpptlm_bridge.h:243-306`）迁至新建 `include/cudart/hsk4_abi_guards.h`; (b) 删除 `sizeof(PtxEmuDriverApi)==64` 布局锁（理由：`PtxEmuDriverApi` 在 HSK-6 已废止）; 验证: `grep -c "static_assert" include/cudart/abi_guards.h` 应返回 0; `grep -c "static_assert" include/cudart/hsk4_abi_guards.h` 应返回 16
- [ ] 3.15 `include/cudart/AGENTS.md` 更新：移除已删除的 `cpptlm_bridge.h` 章节，添加 HSK-8 章节
- [ ] 3.16 `src/main.cpp:131` 清理：`g_ptx_emu_driver` 相关注释引用
- [ ] 3.17 `src/CMakeLists.txt` 调整：移除 `ptx_emu_bridge_stub.cc`/`memory_bridge.cc`/`ptx_emu_driver_shim.cc` 引用
- [ ] 3.18 `test/CMakeLists.txt` 调整：移除 `ptxemu_link_smoke` target

---

## 4. 测试改造（~Medium~Large, 1d）

### 4.1 PTX-EMU 端（独立仓库）

- [ ] 4.1.1 `tests/unit/test_device_api_create_destroy.cc` 新建：验证 `create_device/destroy_device` 生命周期
- [ ] 4.1.2 `tests/unit/test_device_api_warp_status.cc` 新建：验证 `warp_status()` DTO 映射正确性
- [ ] 4.1.3 `tests/unit/test_thread_state_mapping.cc` 新建：验证 `ThreadState` ↔ `EXE_STATE` static_assert 锁（同构验证）
- [ ] 4.1.4 `tests/unit/test_load_ptx_source.cc` 新建：验证 `load_ptx_source()` 端到端路径（解析 PTX 源码 → IR → execute_warp）

### 4.2 CppTLM 端 facade 测试 ×6（~1d）

- [ ] 4.2.1 `test/test_ptx_emu_facade_decode.cc` 改写：roundtrip 测试迁至 PTX-EMU 端（4.1.4 + 4.1.5），CppTLM 保留 golden-bytes decode 测试
- [ ] 4.2.2 `test/test_ptx_emu_facade_arith.cc` (271 行, 19 内部引用) 改写：fixture 改用 PTX 源码字符串（`device_->load_ptx_source()`），断言改为 `read_register_u32/u64` + `warp_status()`
- [ ] 4.2.3 `test/test_ptx_emu_facade_branch.cc` (243 行, 10 内部引用) 改写：同上（PTX 分支源码 + `read_thread_pc` 断言）
- [ ] 4.2.4 `test/test_ptx_emu_facade_barrier.cc` (228 行, 15 内部引用) 改写：同上（PTX `bar.sync` 源码）
- [ ] 4.2.5 `test/test_ptx_emu_facade_memory.cc` (137 行, 8 内部引用) 改写：PTX `ld/st.global` 源码 + `read/write_global_u32/u64` 断言
- [ ] 4.2.6 `test/test_ptx_emu_facade_state.cc` (221 行, 6 内部引用) 改写：`warp_status()` DTO 断言

### 4.3 CppTLM 端 adapter 测试 ×6（~Short）

- [ ] 4.3.1 `test/test_cuda_core_adapter_mvp_tick.cc` (46 行) 改写：保留，仅类型名调整（`uint32_t warp_id` 替换指针）
- [ ] 4.3.2 `test/test_cuda_core_adapter_mvp_scoreboard.cc` (71 行) 改写：保留（`attach_timing` 注入验证）
- [ ] 4.3.3 `test/test_cuda_core_adapter_mvp_pipeline.cc` (77 行) 改写：保留
- [ ] 4.3.4 `test/test_cuda_core_adapter_mvp_dispatch.cc` (89 行) 改写：保留（`CtaDescriptor` 不变）
- [ ] 4.3.5 `test/test_cuda_core_adapter_mvp_warp_state.cc` (92 行, 13 不透明指针用法) 改写：指针 → `WarpHandle` 机械替换
- [ ] 4.3.6 `test/test_cuda_core_adapter_mvp_injection.cc` (97 行) 改写：断言改为验证 `attach_timing` 被调（Mock device）

---

## 5. 验证（~30min, 8 条勾选必跑）

- [ ] 5.1 OFF 路径 850/850 全 PASS
- [ ] 5.2 ON 路径 891/891 全 PASS
- [ ] 5.3 facade.cc / adapter.cc 对 PTX-EMU 内部头 include 数量 = 0（仅 include `ptxemu/device_api.h`）
- [ ] 5.4 `grep -r "g_cpptlm_bridge\|cpptlm_set_driver\|cpptlm_attach_bridge\|cpptlm_detach_bridge\|cpptlm_bridge.h" src/ include/` **归零**
- [ ] 5.5 `grep -r "ptxsim/\|ptx_ir/\|memory/simple_memory\|register/register_bank" src/ include/` **归零**（除 `device_api.h` 外零 PTX-EMU 头）
- [ ] 5.6 `PTXEMU_API_VERSION` 版本守卫断言生效（手动回退 submodule 到 c2038a93 触发 FATAL_ERROR）
- [ ] 5.7 include 防火墙 grep 门禁生效（手动在某 .cc 加 `#include "ptxsim/sm_context.h"` 触发 FATAL_ERROR）
- [ ] 5.8 abi_guards 反向故意失败测试触发编译错误（故意将 `PipelineId::P0_INT_FP32 = 1` 验证）

---

## 6. 文档同步（~30min）

- [ ] 6.1 `AGENTS.md` HSK 链路段追加 HSK-8（ptxemu-public-api）
- [ ] 6.2 `docs/superpowers/specs/2026-08-18-hsk-6-response.md` 状态段追加：桥接清理提前到本 change（替代原 v3.0 P4 计划）
- [ ] 6.3 `openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/` 加注 "本 change 为过渡阶段产物，未来 HSK-8 + bump PR 接管"
- [ ] 6.4 `roadmap.md` 更新：MVP 状态标注本 change 完成
- [ ] 6.5 `roadmap-meta` 更新（如适用）
- [ ] 6.6 跑 `scripts/test/docs_sync_check.sh --strict` 必须 PASS（AGENTS.md / ONBOARDING.md / roadmap.md / scripts/README.md 路径全有效）

---

## 风险登记（本 change 子集）

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | API 首版遗漏（DTO 粒度与 S1 行为有偏差） | 中 | 中 | API 面严格从 S1 调用点机械抽取；迁移后 891 套件逐条对拍行为 |
| R2 | DTO 映射漂移（PTX-EMU 内部 enum 演进后公共 DTO 失真） | 中 | 中 | impl 内 `static_assert` 锁 + 契约测试覆盖每个枚举值 |
| R3 | 跨仓时序死锁（CppTLM bump 依赖 PTX-EMU PR 先合入） | 中 | 高 | HSK-8 先书面锁定；本地分支 commit 验证 |
| R4 | pin 回滑风险 | 低 | 中 | 顶层 CMakeLists 版本守卫给出明确错误 |
| R5 | CMake 目标污染（add_subdirectory 把 PTX-EMU options/tests 带入 CppTLM） | 低 | 中 | `PROJECT_IS_TOP_LEVEL` 守卫 + `PTXEMU_BUILD_TESTING=OFF` 默认 |
| R6 | 双层 facade 调试断层 | 低 | 低 | `IPtxEmuDevice::debug_dump_state()` 逃生口（debug-only） |
| R7 | `StatementContext` 传递闭包含实现头 | 中 | 中 | 执行前硬校验；降级方案：`StatementHandle` 不透明句柄 |
| R8 | HSK-4 接口 vendored 头删除后 CppTLM 编译失败（如果 PTX-EMU 端 PUBLIC include 路径变化） | 低 | 中 | Phase 2 PTX-EMU 端 `include/ptxemu/device_api.h` 明确 `#include "ptxemu/ir/statement.h"` 等公共头路径 |

---

## 验收检查表

最终本 change archive 前：
- [ ] 阶段 0-6 全部完成（2026-XX-XX 全部 acceptance 勾选）
- [ ] PTX-EMU 端 PR 已合入（CI 全绿 + PTX-EMU maintainer 决策）
- [ ] CppTLM 端 bump PR 已合入（OFF 850 + ON 891 全 PASS）
- [ ] 8 条验证标准全部 PASS
- [ ] `docs_sync_check --strict` PASS
- [ ] HSK-8 handshake 文档双方签字

**最终统计**:
- PTX-EMU 端: 1 公共头（~200 行）+ 1 IR 头 + 1 impl（~400 行）+ 2 测试 + 2 cmake 改动 ≈ Short~Medium (1-2d)
- CppTLM 端: facade.cc/.hh 重写 + adapter.cc/.hh 重写 + 桥接残留簇 5 项删除 + abi_guards 拆分 + 12 测试改写 + CMake 重构 ≈ Medium~Large (2-3d)
- 跨仓协调: HSK-8 handshake + PR 评审 + 同步 ≈ Short

---

**Cc**: CppTLM Team · PTX-EMU Team · UsrLinuxEmu (PTX-EMU 下游)
**Refs**:
- [`proposal.md`](../proposal.md)
- [`design.md`](../design.md)
- [`specs/ptxemu-public-device-api/spec.md`](../specs/ptxemu-public-device-api/spec.md)
- [`specs/ptxemu-internal-impl-hiding/spec.md`](../specs/ptxemu-internal-impl-hiding/spec.md)
- [`specs/cpptlm-facade-public-api-only/spec.md`](../specs/cpptlm-facade-public-api-only/spec.md)
- S1 change: `openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`
- HSK-6: `docs/superpowers/specs/2026-08-18-hsk-6-response.md`

---

**起草**: Sisyphus (2026-08-21, per Oracle ses_fdb70164bffe2vBN71uiaV90aY 4 轮咨询 + 用户方向"cpp 不暴露")
**Owner**: CppTLM Team + PTX-EMU Team
**状态**: 📋 Proposed — 2026-08-21, 待 archive 阶段统一聚合 commit