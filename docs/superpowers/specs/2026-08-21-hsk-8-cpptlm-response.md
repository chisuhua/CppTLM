# HSK-8 状态回复 (PTX-EMU → CppTLM)

> **日期**: 2026-XX-XX (PTX-EMU 端 ack 时填写)
> **发送方**: PTX-EMU Architecture Team (@ptx_emu_owner)
> **接收方**: CppTLM Team (Sisyphus)
> **回传目标**: CppTLM 仓 issue/PR 评论 + CppTLM `docs/superpowers/specs/2026-08-21-hsk-8-cpptlm-response.md` (mirror)
> **形式**: 结构化确认 + commit hash 引用 + 验收 checklist
> **关联**:
> - CppTLM HSK-8 spec: [`docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md`](https://github.com/chisuhua/CppTLM/blob/main/docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md) (commit `3d7898b`)
> - HSK-6 历史: [`docs-archived/superpowers/specs/2026-08-18-hsk-6-response.md`](../docs-archived/superpowers/specs/2026-08-18-hsk-6-response.md) (CppTLM ack commit `369cf71`)
> - CppTLM openspec change: `openspec/changes/cpptlm-ptxemu-public-device-api/`
> - Oracle session `ses_fdb70164bffe2vBN71uiaV90aY` (4 轮咨询)
> - HSK 历史: HSK-1 (`8dc000ec`) · HSK-2 (ANTLR4 4.13.2) · HSK-3 (ExternalProject_Add, ⚠️ 已被 HSK-6 废止方向) · HSK-4 (`8acfd2d1`/`9e7361b9`/`463038e0`) · HSK-5 (`advance()`, 🟥 CANCELLED by HSK-6) · HSK-6 (PTX-EMU `25e36f60` / CppTLM ack `369cf71` / 实施 `585e4ff`·`5d9473a`·`fa2b3ec`) · HSK-7 (🔵 预留 — 仅 ABI 解冻触发) · **HSK-8 (本 ack)**

---

## HSK-8 闭环（PTX-EMU 端公共设备 API 契约）⏳ **PTX-EMU ACK 待填写**

> **锁定 commit hash** (PTX-EMU 端 ack 时回填):
> - PTX-EMU HSK-8 公告 commit: `<PTX-EMU 待填, 类似 25e36f60>`
> - CppTLM HSK-8 spec commit: `3d7898b` (本仓 `openspec/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration` 分支)

### 逐项确认（参考 HSK-8 spec §"CppTLM 端接受条件"）

| HSK-8 请求（CppTLM `3d7898b`） | PTX-EMU 状态 | 证据 / 行动 |
|-------------|-----------|------|
| 1. PTX-EMU 仓 `include/ptxemu/device_api.h` 新增（含 `IPtxEmuDevice` + 工厂 + `PTXEMU_API_VERSION=1`） | ⏳ 待 PTX-EMU 验证 | PTX-EMU owner review + commit |
| 2. PTX-EMU 仓 `add_library(ptxemu_core STATIC ...)` 可被 `add_subdirectory(external/PTX-EMU)` 消费 | ⏳ 待 PTX-EMU 验证 | PTX-EMU CMakeLists.txt + consumer_smoke 测试 |
| 3. PTX-EMU 端 `consumer_smoke` 测试 PASS | ⏳ 待 PTX-EMU 验证 | PTX-EMU CI run 输出 |
| 4. PTX-EMU 端 `drift_check` 通过 | ⏳ 待 PTX-EMU 验证 | PTX-EMU CI run 输出 |
| 5. PTX-EMU maintainer 在本 PR 评论 +1 ack | ⏳ 待 PTX-EMU 验证 | GitHub PR 评论 |

### HSK-8 关键契约重述（PTX-EMU owner 复审参考）

| 契约 | 详情 |
|------|------|
| 公共头路径 | `ptxemu/device_api.h` (PTX-EMU 端 `include/ptxemu/`) |
| 命名空间 | `ptxemu::` |
| 版本守卫宏 | `PTXEMU_API_VERSION` (初始值 `1`) |
| C++ 标准 | C++17 (CppTLM 全局 C++17) |
| 公共头暴露范围 | `IPtxEmuDevice` 抽象接口 + `DeviceConfig`/`WarpStatus`/`LaneStatus`/`ThreadState` DTO + `create_device/destroy_device` 工厂 + `decode_ptxir` 字节流解码 |
| CMake 库目标 | `ptxemu_core` (STATIC, 显式列源, PUBLIC `include/ptxemu` + PRIVATE `${PTXEMU_SRC}`) |
| HSK-4 接口复用 | `IPtxEmuDevice::attach_timing()` 接收 `IScoreboard*` + `IPipelineLatencyProvider*` + `ITensorCoreTiming*` |
| Statement 公共化 | `ptxemu/ir/statement.h` 晋升自内部 `StatementContext`；降级方案 `StatementHandle` |
| 静态断言锁 | impl 层 `static_assert(static_cast<uint32_t>(ptxemu::ThreadState::kIdle) == static_cast<uint32_t>(ptxsim::EXE_STATE::IDLE))` 系列 |

### PTX-EMU 端实施工作量（HSK-8 spec 估算 vs 实际）

| 内容 | spec 估算 | 实际 (PTX-EMU owner 填写) | 备注 |
|------|---------|---------|------|
| `include/ptxemu/device_api.h` (~200 行公共头) | Short | _____ | |
| `include/ptxemu/ir/statement.h` (晋升 `StatementContext`) | Short | _____ | 含传递闭包审计 (1-2d) |
| `src/ptxemu/device_api_impl.cc` (~400 行薄适配层) | Medium | _____ | |
| `add_library(ptxemu_core STATIC ...)` + PUBLIC/PRIVATE include 拆分 | Short | _____ | |
| `if(PROJECT_IS_TOP_LEVEL)` 隔离 + `option(PTXEMU_BUILD_TESTING OFF)` | Short | _____ | |
| `tests/build_cpptlm_consume/consumer_smoke.cc` + `drift_check.cmake` | Short | _____ | |
| 内部 `EXE_STATE` ↔ 公共 `ThreadState` `static_assert` 锁 | Quick | _____ | |
| **总计** | **Short~Medium (1-2d)** | _____ | 实际可能 3-5d (含 StatementContext 闭包审计) |

---

## ✅ PTX-EMU Ack 状态总结

| 项 | 状态 |
|---|:---:|
| HSK-8 主协议（公共设备 API 契约） | ⏳ 待 PTX-EMU owner 验证 |
| 5 条接受条件逐项确认 | ⏳ 待 PTX-EMU 验证 |
| StatementContext 公共化路径（晋升 vs 降级） | ⏳ 待 PTX-EMU 选定 |
| 跨仓协调顺序 per spec §"跨仓协调顺序" 5 步 | ⏳ 待 PTX-EMU 排期 |
| Ack 截止 2026-XX-XX (14 天窗口) | ⏳ 待 PTX-EMU 选定时 |

**PTX-EMU 端 HSK-8 响应 = ⏳ PENDING OWNER ACK**

---

## 跨链同步

| 仓 | 跟踪载体 | 状态 |
|---|---|---|
| PTX-EMU | HSK-8 公告 commit + 本文件 mirror | ⏳ 待 PTX-EMU 落地 |
| CppTLM | HSK-8 spec (`3d7898b`) + 本文件 mirror + openspec change `cpptlm-ptxemu-public-device-api` | ✅ 已发出 |
| UsrLinuxEmu | (Cc, 无需 ack) | 🔵 仅通知 |
| TaskRunner | (Cc, 无需 ack) | 🔵 仅通知 |

---

## 🔵 PTX-EMU owner 决策点（请在 ack 时明确）

1. **`StatementContext` 公共化路径**: (a) 晋升为 `ptxemu/ir/statement.h` 公共 IR 头; (b) 降级为不透明 `StatementHandle` + `decode_ptxir` 直接提交字节流
2. **PTX-EMU 端 CI 集成策略**: (a) `consumer_smoke` + `drift_check` 加入 PTX-EMU 自身 CI; (b) 仅作为新 PR 的 smoke gate
3. **`option(PTXEMU_BUILD_TESTING OFF)` 默认值**: 确认接受 OFF 默认 + `if(PROJECT_IS_TOP_LEVEL)` 隔离模式
4. **Phase 2 PR 排期**: 目标合入 commit hash (待 PTX-EMU 排期后填写)

---

**Cc**: @ptx_emu_owner · @ptx_emu_architecture_team · @usr_linux_emu_architecture_team · @cpp_tlm_owner

**Refs**:
- CppTLM HSK-8 spec: `3d7898b` (https://github.com/chisuhua/CppTLM/blob/openspec/2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md)
- CppTLM HSK-8 proposal: `openspec/changes/cpptlm-ptxemu-public-device-api/proposal.md`
- HSK-6 CppTLM ack: `369cf71`
- HSK-6 PTX-EMU 公告: `25e36f60`
- Oracle session: `ses_fdb70164bffe2vBN71uiaV90aY`

---

**起草**: PTX-EMU Architecture Team (本协议接收方) · 2026-XX-XX (PTX-EMU 端 ack 时回填)
**Owner**: CppTLM Team (Sisyphus) (本协议发起方)
**状态**: ⏳ Pending PTX-EMU Owner ACK

---

## Status Update (2026-08-31) — HSK-8 Phase 2 Step 4 落地后回归验证

**触发**: CppTLM 2026-08-31 全量回归审计（HEAD=`1d16e5f`）发现 HSK-8 Phase 2 Step 4 (commit `738b412c`) 落地后 2 项测试基础设施漂移。

**修复 change**: `cpptlm-2026-08-31-fix-test-infra-drift`（已归档，commit 序列 `5a460d4`→`641c663`）

| 项 | 结果 | 关联 commit |
|----|------|-------------|
| `--f12b-ld` observability 日志恢复 | ✅ `src/main.cpp` else 分支恢复 `MemoryBridge disabled`，`test_f12b_default_off` 2/2 PASS | `5a460d4` |
| `test_f12b_enabled_no_crash` 移除 | ✅ 与 HSK-8 永久禁用决策矛盾的用例删除（无 "enabled" 合法语义） | `6085cbd` |
| PTX-EMU `bench/cute/` ctest 隔离 | ✅ wrapper-level `ctest -E cute`（cmake `set_tests_properties` 跨子目录 scope 不可行） | `2bfea64` |

**回归基线**（2026-08-31 复验）:
- `cpptlm_tests`: **998 test cases / 32600 assertions / ALL PASSED**
- Python pytest: **255 passed / 2 PASS**（f12b_smoke 从 2 fail 改为 2/2 PASS）
- ctest gate: **20/21 PASS + 1 Not Run**（`test_cpptlm_emulator_dlopen` 因 BUILD_RTL=OFF，pre-existing）
- E2E: **12/12 PASS**

**遗留 follow-up**:
- [ ] PTX-EMU 上游 PR: `external/PTX-EMU/CMakeLists.txt:138-139` 加 `if(PTXEMU_BUILD_TESTING OR PROJECT_IS_TOP_LEVEL)` 守卫（与 `tests/` 守卫 line 130-137 对称）— 根治 `bench/cute/` leak
- [ ] AGENTS.md § KEY INVARIANTS: `cpptlm_tests 764/764 (2026-07-03)` → `998/998 (2026-08-31)`

**HSK-8 主协议状态**: 不变（仍 ⏳ 待 PTX-EMU owner ack 公共设备 API 契约 `3d7898b`）
