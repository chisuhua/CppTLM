# HSK-8: PTX-EMU 端公共设备 API (CppTLM → PTX-EMU 发起)

> **状态**: ✅ **Acked** — PTX-EMU owner ack commit `738b412c` (origin/main, 2026-08-22 23:27:15 +0800) · **原始 Draft**: 2026-08-21 · **Acked**: 2026-08-22
> **发起方**: CppTLM Team (Sisyphus, `docs/hsk-8` 草拟 `f2b8aa0`, review fixes `4cdedc5` + `2043b28`)
> **接收方**: PTX-EMU Architecture Team
> **PTX-EMU ack**: [`738b412c`](https://github.com/chisuhua/PTX-EMU/commit/738b412c82a11068c1286a611a30593dcc1d1afc) (commit message `docs(hsk-8): PTX-EMU owner ack public device API contract (issue #22)`)
> **issue #22 ack comment**: [comment 5381166580](https://github.com/chisuhua/CppTLM/issues/22#issuecomment-5381166580)
> **关联变更**: [`openspec/changes/cpptlm-ptxemu-public-device-api/`](../../changes/cpptlm-ptxemu-public-device-api/)
> **HSK 历史**: HSK-1 (`8dc000ec`) · HSK-2 (ANTLR4 4.13.2) · HSK-3 (ExternalProject_Add, ⚠️ 已被 HSK-6 废止方向) · HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) · HSK-5 (`advance()`, 🟥 CANCELLED by HSK-6) · HSK-6 (PTX-EMU `25e36f60` · CppTLM ack `369cf71` · 实施 refs `585e4ff`/`5d9473a`/`fa2b3ec`) · HSK-7 (🔵 预留 — 仅 `CPPTLMBRIDGE_VERSION` 解冻时触发, 截至 2026-08-21 未签发, 编号保留) · **HSK-8 (本协议 — CppTLM 草拟 `f2b8aa0`/`4cdedc5`/`2043b28`, PTX-EMU ack `738b412c`, issue #22 ack comment `5381166580`)**

> **PTX-EMU ack 详情** (per `738b412c` commit message):
>
> **Ack 时间窗**: Ack 截止 2026-09-05 (14 天窗口, per HSK-6 precedent); Phase 2 PR 开工 ETD 2026-09-19
>
> **4 决策点答复** (含 Oracle 闭包审计):
> 1. **`StatementContext` 公共化 → 路径 (a) 选**（晋升 `ptxemu/ir/statement.h`）, 但有 **CONDITIONAL Phase 0 净化**（路径 (a) 选定的 2 原因: CppTLM Decision 5 锁定 'sizeof visibility is mandatory' 与路径 (b) opaque handle 冲突; 路径 (b) 题面描述含内在矛盾 — PtxirReader 仍暴露 `StatementContext`）:
>    - **污染点 1**: `ptx_ir/operand_context.h:59` `mutable void *operand_phy_addr` (运行时指针混入值类型)
>    - **污染点 2**: `ptx_ir/statement_context.h:310` `InstructionState state` (执行态嵌入 IR, 违反 §1 教训)
>    - **闭包实际**: 5 文件 ~1053 LOC, 含 2 个 `.def` X-Macro 公共化
> 2. **CI 集成**: 本期 `drift_check` (新 workflow); 下期 `consumer_smoke` (HSK-9 准入)
> 3. **`PROJECT_IS_TOP_LEVEL` 隔离**: 接受 + `option(PTXEMU_BUILD_TESTING OFF)` 默认值
> 4. **Phase 2 PR 排期**: `origin/main` base, **12-15d** (Oracle 上调 50% over spec 1-2d 估算); 目标 2026-09-19 前合入

> **HSK-6 commit 角色澄清** (避免审查时的 hash 混淆):
> - `25e36f60` (PTX-EMU 端): HSK-6 公告 commit (PTX-EMU 仓 `chisuhua/PTX-EMU`, 不在 CppTLM git history — CppTLM submodule 仅镜像 PTX-EMU 仓内容快照, 验证需 PTX-EMU 仓 `git log 25e36f60`)
> - `369cf71` (CppTLM 端): CppTLM 对 HSK-6 的 ack commit — 本 spec HSK 历史段的 ack 引用 (`docs(hsk-6): ack PTX-EMU HSK-6 bridge deprecation announcement`)
> - `585e4ff` (CppTLM 端): Phase 2a 实施 commit (origin/main 祖先, HSK-6 ack 前; S1/MVP 工作线承接 — `feat(phase2a): inject PipelineTLM/ScoreboardTLM/TensorCoreTLM into PTX-EMU`)
> - `5d9473a` (CppTLM 端): HSK-6 openspec change `cpptlm-v3-dgpu-extract` 启动 commit (`docs(openspec): start cpptlm-v3-dgpu-extract change (per #19 v3.0 RFC)`)
> - `fa2b3ec` (CppTLM 端): HSK-6 P0-1 实施 commit (17 条 static_assert 迁移至 `abi_guards.h` — `feat(abi-guards): migrate 17 G-D4 static_asserts to abi_guards.h (HSK-6 P0-1)`)
> - `738b412c` (PTX-EMU 端): HSK-8 ack commit (本 spec 接收方签发, 含 4 决策点答复 + CONDITIONAL Phase 0 净化)

> **HSK-7 预留语义** (审计可追溯):
> HSK-6 response (`docs-archived/superpowers/specs/2026-08-18-hsk-6-response.md` 第 30 行) 显式声明:
> "CPPTLMBRIDGE_VERSION 冻结于 2 (任何解冻触发 HSK-7)"
> 即 HSK-7 协议号已**锁定给** ABI 解冻事件, 编号保留, 不被任何其他变更占用。

---

## 背景

HSK-6 (`25e36f60`) 已确立 CppTLM 端**不再 vendored** PTX-EMU 桥接代码（消费关系废止）。HSK-3 的 `ExternalProject_Add` 决策约束的是已废止方向（PTX-EMU 消费 CppTLM），对当前方向（CppTLM 消费 PTX-EMU）不构成约束。

PTX-EMU 远端 `origin/main` 已主动完成 `cleanup-cudart-cpptlm-bridge-coupling` Phase 1-4（1018 行删除），`include/cudart/cpptlm_bridge.h` / `cudart/cpptlm_bridge/PtxEmuDriverShim.h/cpp` / `cudart/stub_bridge.h` 均已删除。

CppTLM 端完成 S1 (`openspec/changes/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`，850+891 测试 PASS) 后，需推进"**CppTLM 仅 include PTX-EMU 公共头，cpp 实现封装在 PTX-EMU 端 .a 库内**"的方向。本 HSK-8 锁定该方向的集成契约。

---

## 请求

PTX-EMU 团队承担以下契约：

### 1. 公共 API 头维护

| 项 | 契约 |
|----|------|
| 文件路径 | PTX-EMU 仓 `include/ptxemu/device_api.h` |
| 命名空间 | `ptxemu::` |
| 版本守卫宏 | `PTXEMU_API_VERSION` (初始值 `1`) |
| 兼容 C++ 标准 | C++17 (因 CppTLM 全局 C++17) |
| 公共头暴露范围 | `ptxemu::IPtxEmuDevice` 抽象接口 + `DeviceConfig`/`WarpStatus`/`LaneStatus`/`ThreadState` DTO + `create_device/destroy_device` 工厂 + `decode_ptxir` 字节流解码 |

### 2. PTXEMU_API_VERSION 语义

```cpp
#define PTXEMU_API_VERSION 1
```

**冻结规则**:
- 不修改公共头签名（不删除/不修改/不改语义）→ MUST NOT bump
- 新增可选接口 → MAY bump `PTXEMU_API_VERSION`
- ABI break → MUST bump + 触发新 HSK
- `IPtxEmuDevice::api_version()` 运行时返回值 MUST == `PTXEMU_API_VERSION`

### 3. CMake 库目标

PTX-EMU 仓 `CMakeLists.txt` MUST 新增 `add_library(ptxemu_core STATIC ${PTXEMU_CORE_SOURCES})`，显式列源（禁 GLOB），并设置：

```cmake
target_include_directories(ptxemu_core
    PUBLIC  include/ptxemu
    PRIVATE ${PTXEMU_SRC})           # ptxsim/ ptx_ir/ memory/ register/ cudart/ 等
```

并新增 `option(PTXEMU_BUILD_TESTING OFF)` 默认值 + `if(PROJECT_IS_TOP_LEVEL)` 隔离 PTX-EMU 自身 tests/tools。

### 4. 内部头封装

PTX-EMU 内部头（`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/`register/*.h`/`cudart/*.h`/`utils/*.h`）通过 `PRIVATE` include 封装，CppTLM 编译时**不可见**。CppTLM include 防火墙 grep 门禁将强制此约束（任何 .cc include `ptxsim/`/`ptx_ir/`/`memory/`/`register/` 即 CMake FATAL_ERROR）。

### 5. HSK-4 接口注入复用

`IPtxEmuDevice::attach_timing(uint32_t sm, IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)` 复用 HSK-4 已 vendored 的 3 个纯虚接口（IScoreboard 来自 `8acfd2d1`、IPipelineLatencyProvider 来自 `9e7361b9`、ITensorCoreTiming 来自 `463038e0`）。PTX-EMU 端不需要重新定义这些接口。

### 6. PTX-EMU 端契约测试

PTX-EMU 仓 MUST 在 CI 中跑：
- `tests/build_cpptlm_consume/consumer_smoke` — 最小 consumer 可执行，仅 include `ptxemu/device_api.h` + 调用 `create_device/destroy_device`
- `tests/build_cpptlm_consume/drift_check.cmake` — GLOB drift 门禁（`ptxemu_core` 源文件 vs `git ls-files`）

### 7. `StatementContext` 公共化

PTX-EMU 仓 MUST 在 `include/ptxemu/ir/statement.h` 提供 `ptxemu::Statement`（晋升自内部 `StatementContext`）。该头 MUST 是纯数据 IR 类型，不引入 PTX-EMU 内部实现头。若 `StatementContext` 传递 include 闭包无法公开，PTX-EMU 端降级方案：改为不透明 `StatementHandle` + `decode_ptxir` 直接提交字节流。

---

## PTX-EMU 端实施工作量

| 内容 | 工作量 |
|------|--------|
| `include/ptxemu/device_api.h` (~200 行公共头) | Short |
| `include/ptxemu/ir/statement.h`（晋升 `StatementContext`） | Short |
| `src/ptxemu/device_api_impl.cc` (~400 行薄适配层) | Medium |
| `add_library(ptxemu_core STATIC ...)` + PUBLIC/PRIVATE include 拆分 | Short |
| `if(PROJECT_IS_TOP_LEVEL)` 隔离 + `option(PTXEMU_BUILD_TESTING OFF)` | Short |
| `tests/build_cpptlm_consume/consumer_smoke.cc` + `drift_check.cmake` | Short |
| 内部 `EXE_STATE` ↔ 公共 `ThreadState` `static_assert` 锁 | Quick |
| **总计** | **Short~Medium (1-2d)** |

---

## 跨仓协调顺序

| 步骤 | 责任方 | 依赖 |
|------|--------|------|
| 1. PTX-EMU HSK-8 ack commit | PTX-EMU | 本 spec |
| 2. PTX-EMU 端 PR（基于 `origin/main` post-deletion） | PTX-EMU | 步骤 1 |
| 3. PTX-EMU CI 全绿（consumer_smoke + drift_check + 自身测试） | PTX-EMU | 步骤 2 |
| 4. PTX-EMU PR 合入 main | PTX-EMU | 步骤 3 |
| 5. CppTLM bump PR（submodule pin + add_subdirectory + 桥接残留簇删除） | CppTLM | 步骤 4 |

**禁止跨级**:
- ❌ PTX-EMU PR 基于 `c2038a93` 或更早 commit（仍引用 `g_cpptlm_bridge`，库目标无法独立链接）
- ❌ CppTLM bump PR 在 PTX-EMU PR 合入前提交（submodule pin 解析失败）

---

## CppTLM 端接受条件（仅 PTX-EMU 满足以下即生效）

- [ ] PTX-EMU 仓 `include/ptxemu/device_api.h` 已新增（含 `IPtxEmuDevice` + 工厂 + `PTXEMU_API_VERSION=1`）
- [ ] PTX-EMU 仓 `add_library(ptxemu_core STATIC ...)` 可被 `add_subdirectory(external/PTX-EMU)` 消费
- [ ] PTX-EMU 端 `consumer_smoke` 测试 PASS
- [ ] PTX-EMU 端 `drift_check` 通过
- [ ] PTX-EMU maintainer 在本 PR 评论 +1 ack

---

## CppTLM 端回传计划（PTX-EMU ack 后执行）

| 步骤 | 内容 |
|------|------|
| 1 | submodule bump 到 PTX-EMU PR 合入 commit |
| 2 | 根 `CMakeLists.txt`: 删除 `include(cmake/PTXEmuCore.cmake)`, 改 `add_subdirectory(external/PTX-EMU)` |
| 3 | 删除 `cmake/PTXEmuCore.cmake` (246 行) + `src/tlm/gpu/ptx_emu_bridge_stub.cc` + `include/cudart/cpptlm_bridge.h` + `MemoryBridge`/`PtxEmuDriverShim` 桥接残留簇 5 项 |
| 4 | 重写 `ptx_emu_submodule_mvp.cc/.hh` + `cuda_core_adapter_mvp.cc/.hh`（12+5 include → 1，模板→重载，指针→`uint32_t warp_id` 句柄）|
| 5 | `abi_guards.h` 拆分（per HSK-6 P0-1 `fa2b3ec`）: (a) 16 条 enum 断言（`PipelineId` 6 条 + `TcPrecision` 6 条 + `is_same_v` 4 条，from `cpptlm_bridge.h:243-306`）迁至新建 `include/cudart/hsk4_abi_guards.h`; (b) 删除 `sizeof(PtxEmuDriverApi)==64` 这条布局锁（理由：`PtxEmuDriverApi` 在 HSK-6 已废止，布局锁失去意义）；拆分后 `abi_guards.h` 自身清空（H3 review fix: 16+1=17 总数与 `fa2b3ec` 当前内容一致） |
| 6 | 12 测试改写（facade fixture 改用 PTX 源码字符串）|
| 7 | 新增 include 防火墙 grep 门禁（除 facade/adapter 外任何 .cc include `ptxsim/`/`ptx_ir/`/`memory/`/`register/` 即 FATAL_ERROR）|
| 8 | 跑 8 条验证标准（OFF 850 + ON 891 + grep 归零 + 版本守卫 + 反向故意失败）|

---

## 关联变更 / 引用

- **CppTLM openspec change**: [`openspec/changes/cpptlm-ptxemu-public-device-api/`](../../changes/cpptlm-ptxemu-public-device-api/) (4/4 artifacts, proposed)
- **HSK-6 CppTLM ack**: [`docs-archived/superpowers/specs/2026-08-18-hsk-6-response.md`](../../../docs-archived/superpowers/specs/2026-08-18-hsk-6-response.md)
- **HSK-4 vendored 接口头** (CppTLM 端): `include/cudart/{scoreboard,pipeline,tensor_core}_interface.h`
- **PTX-EMU HSK-4 端**: `https://github.com/chisuhua/PTX-EMU/blob/main/include/ptxsim/scoreboard_interface.h` (`8acfd2d1`) / `pipeline_interface.h` (`9e7361b9`) / `tensor_core_interface.h` (`463038e0`)
- **PTX-EMU 远端 cleanup commits** (Phase 1-4): `a9a14e1d` / `292022a3` / `e4d7e369` / `09786635`
- **ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) (D2/D3)
- **Oracle 4 轮咨询**: session `ses_fdb70164bffe2vBN71uiaV90aY`

---

## PTX-EMU 团队查找路径（TL;DR）

| 角色 | 路径 |
|------|------|
| PTX-EMU 团队查找本 spec | CppTLM 仓 `docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md`（GitHub: `https://github.com/chisuhua/CppTLM/blob/main/docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md`）|
| PTX-EMU 团队回传 ack | PTX-EMU 仓 `docs/superpowers/specs/2026-08-21-hsk-8-cpptlm-response.md`（mirror）或 PTX-EMU PR 评论 |
| CppTLM 团队跟进 | 本 spec + 关联 `openspec/changes/cpptlm-ptxemu-public-device-api/` (proposal.md §跨仓协调) |

---

**Cc**: @ptx_emu_owner · @ptx_emu_architecture_team · @usr_linux_emu_architecture_team

**Refs**:
- HSK-1 (`8dc000ec`) · HSK-2 (ANTLR4 4.13.2) · HSK-3 (ExternalProject_Add, HSK-6 废止方向) · HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) · HSK-5 (🟥 CANCELLED by HSK-6) · HSK-6 (PTX-EMU `25e36f60` / CppTLM ack `369cf71` / 实施 `585e4ff`·`5d9473a`·`fa2b3ec`) · HSK-7 (🔵 预留 — 仅 ABI 解冻时签发, 未使用) · **HSK-8 (本 spec)**
- PTX-EMU cleanup Phase 1-4: `a9a14e1d` / `292022a3` / `e4d7e369` / `09786635`
- CppTLM openspec change `cpptlm-ptxemu-public-device-api` (proposal + design + tasks + 3 specs = 7 artifacts)
- Oracle session `ses_fdb70164bffe2vBN71uiaV90aY`

---

**起草**: CppTLM Team (Sisyphus) · 2026-08-21
**Owner**: PTX-EMU Architecture Team (本协议接收方)
**状态**: 📤 Draft — 待 PTX-EMU ack