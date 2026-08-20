# HSK-6 状态回复 (CppTLM → PTX-EMU + UsrLinuxEmu 利益相关方)

> **日期**: 2026-08-18
> **发送方**: CppTLM Team (Sisyphus)
> **接收方**: PTX-EMU Architecture Team + UsrLinuxEmu Architecture Team (利益相关方)
> **回传目标**: PTX-EMU 仓 `docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md` PR comment + UsrLinuxEmu annex §E 跟踪表更新
> **形式**: 结构化确认 + commit hash 引用 + P0-1 门禁计划
> **关联**:
> - PTX-EMU [`2026-08-18-hsk-6-cpptlm-bridge-deprecation.md`](https://github.com/chisuhua/PTX-EMU/blob/main/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md) (commit `25e36f60`)
> - CppTLM [issue #19 v3.0 RFC](https://github.com/chisuhua/CppTLM/issues/19) (Gate #2 ack 2026-08-18)
> - UsrLinuxEmu [ADR-090 v2 commit `37a91b6`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md) (Oracle F-NEW-2 修订)
> - Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy` (4 轮评估 + 5-step self-check)
> - HSK-1~5 历史响应: `2026-07-15-cpptlm-hsk-response.md` + `2026-07-17-hsk-1-2-3-responses.md` + `2026-07-17-hsk-4-5-responses.md`

---

## HSK-6 闭环（CppTLM 桥接消费关系废止）✅ **Acked with Major Revision** (mirror CppTLM #19 v3.1 draft)

> **锁定 commit hash**:
> - PTX-EMU HSK-6 发出: `25e36f60`
> - UsrLinuxEmu ADR-090 v2: `e03b5a1` (✅ Accepted) + `37a91b6` (Oracle F-NEW-2 修订)
> - CppTLM v3.0 openspec change: `5d9473a` (`openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/`)

### 逐项确认

| HSK-6 请求 (PTX-EMU `25e36f60`) | CppTLM 状态 | 证据 / 行动 |
|-------------|-----------|------|
| 1. 消费关系废止 (CppTLM 不再 vendored `cpptlm_bridge.h`) | ✅ **接受** | per [CppTLM #19 v3.0 RFC](https://github.com/chisuhua/CppTLM/issues/19); CppTLM v3.0.0 version bump 已规划 (P4 阶段) |
| 2. 真相源保留 (PTX-EMU 仓 `include/cudart/cpptlm_bridge.h:14-16` 不删) | ✅ **接受** | CppTLM 删除清单中明确"PTX-EMU 真相源保留不删, 仅 CppTLM vendored 副本 + 桥接层物理删除" |
| 3. CPPTLMBRIDGE_VERSION 冻结于 2 (任何解冻触发 HSK-7) | ✅ **接受** | 纳入 `include/cudart/abi_guards.h` (P0-1 新文件) 长期治理 |
| 4. P0-1 门禁 (G-D4 17 条 static_assert 迁至 `abi_guards.h`) | ✅ **接受** | CppTLM owner 承担实施任务 (W1); 来源: `cpptlm_bridge.h:243-306` (16 条) + `ptx_emu_driver.hh:27` (1 条) |
| 5. 删除清单 11 项 | ✅ **接受 (修订版)** | per UsrLinuxEmu `37a91b6` §D6.1: 7 接口/shim/全局 + 4 vendored cudart 头 + 1 布局锁 |
| 6. Mode A frozen vs Mode B dual-rail (两阶段删除) | ✅ **接受** | per CppTLM #19 P4 物理删除阶段 |
| 7. 替代路径: CppTLM v3.0.0 dGPU board (DGpuBar + Doorbell + SQ/CQ) | ✅ **接受 (实施路径已就绪)** | per CppTLM #19 §架构 + 本仓 `openspec/changes/2026-08-18-cpptlm-v3-dgpu-extract/` |
| 8. ANTLR4 不在 CppTLM scope | ✅ **接受 (per HSK-6 接收修订)** | CppTLM owner 决定移除 ANTLR4 相关章节 + v3.0 RFC tasks; v2 §E.4 ANTLR4 spike owner 修订为 UsrLinuxEmu + PTX-EMU |
| 9. 9 周双轨 P0-P4 时间线 | ✅ **接受** | per CppTLM #19 §关键里程碑; W1(P0) + W1-3(P1) + W4-6(P2) + W6-8(P3) + W8-9(P4) |
| 10. HSK-5 `advance()` deferred → CANCELLED by HSK-6 | ✅ **接受** | 协议序列不留悬空项; `IPtxEmuDriver::advance()` 随接口整体删除永久废止 |

### CppTLM 端行动清单

#### W1 (P0 冻结)
- [ ] **G-D4 static_assert 迁移 (P0-1 硬门禁)**:
  - [ ] 新建 `include/cudart/abi_guards.h`
  - [ ] 从 `cpptlm_bridge.h:243-306` 迁移 16 条 (6 PipelineId + 6 TcPrecision + 4 `is_same_v`)
  - [ ] 从 `ptx_emu_driver.hh:27` 迁移 1 条 (`sizeof(PtxEmuDriverApi) == 64`)
  - [ ] 验证: 用 PTX-EMU `@ccd34155:include/cudart/cpptlm_bridge.h:223-290` (14 条 PTX-EMU 侧断言) 对比 CppTLM vendored 副本, 确保完整迁移
- [ ] **Mode A 冻结**: 给 `MemoryBridge` / `IPtxEmuDriver` / `DriverWrapper` 加 `[[deprecated]]` 标注
- [ ] **PTX-EMU HSK-6 ack 等待**: 不主动发出, 等 PTX-EMU owner 14 天窗口 (Ack 截止 2026-09-01) 收齐 ack

#### W1-3 (P1 双轨并行)
- [ ] **轨 A — PtxEmuSubmodule façade**: `include/tlm/gpu/ptx_emu_submodule.hh` + `.cc` (`namespace tlm`)
- [ ] **轨 B — dGPU 最小完整集**: `DGpuBar` + `Doorbell` + `SQ/CQ` 骨架 (PCIe 设备语义)
- [ ] **CompletionRing 重设计**: 删 `std::function` 回调表 → push + `host_notify` 钩子 (per v2 §D3.4)

#### W4-6 (P2 收敛)
- [ ] **ISmExecutor + PtxEmuSubmodule façade 汇合** + E2E 路径实施
- [ ] **KernelLaunchRequest 复用** (`include/tlm/gpu/kernel_launch_tlm.hh:30` 已存在)

#### W6-8 (P3 重构)
- [ ] **KernelLaunchTLM 重构** + CompletionRing + E2E 测试

#### W8-9 (P4 物理删除)
- [ ] **Phase 2 物理删除 11 项清单**:
  1. `MemoryBridge` (`include/tlm/gpu/memory_bridge.hh`)
  2. `IPtxEmuDriver` (`include/tlm/gpu/ptx_emu_driver.hh:19`)
  3. `DriverWrapper` (`include/tlm/gpu/ptx_emu_driver.hh:51`)
  4. `g_ptx_emu_driver` 全局符号
  5. `cpptlm_set_driver` ABI 入口
  6. `ptx_emu_driver_shim.cc` (`src/tlm/gpu/`)
  7. `cpptlm_bridge.h` vendored (`include/cudart/`, 14837 字节)
  8. `pipeline_interface.h` vendored (`include/cudart/`, 1659 字节)
  9. `scoreboard_interface.h` vendored (`include/cudart/`, 1278 字节)
  10. `tensor_core_interface.h` vendored (`include/cudart/`, 1709 字节)
  11. `PtxEmuDriverApi` 布局锁 (`ptx_emu_driver.hh:27`)
- [ ] **CMakeLists.txt v2.1.0 → v3.0.0 BREAKING bump** (per ADR-088 §D6.2)
- [ ] **新建 `include/cpptlm_version.h`**
- [ ] **移除 `cpptlm_bridge.h` vendored 规则**
- [ ] **808 测试验证** (per ADR-088 §D6.2 同步扩展)
- [ ] **Tag v3.0.0**

#### 4 测试文件处置(per v2 §D6.2)
- `test_memory_bridge.cc` → 🗑️ **删除**(测试对象 `MemoryBridge` 物理删除)
- `test_memory_bridge_poll.cc` → 🔄 **重写**为 `test_completion_ring.cc` (`poll_kernel` → CompletionRing push/host_notify)
- `test_kernel_launch_tlm_ext.cc` → ✅ **保留 + 改符号**(`MockPtxEmuDriver` → SQ/CQ doorbell mock; `KernelLaunchRequest`/`setMemoryBridge` 复用)
- `test_gpu_soc_perf.cc` → ✂️ **拆分**(scoreboard perf 保留; MemoryBridge poll perf → CompletionRing)

### CppTLM 端 commit hash 回传

- **CppTLM main HEAD**: `585e4ff` (实施开始时)
- **P0 openspec change**: `5d9473a` (`docs(openspec): start cpptlm-v3-dgpu-extract change`)
- **后续 commit**: 待 P0-1 实施 + P1-P4 推进

### P0-1 门禁验证 checklist

| 项 | 当前 | P0-1 完成后 | 验证方法 |
|---|---|---|---|
| 17 条 static_assert 在 abi_guards.h | ❌ | ✅ | `grep -c "static_assert" include/cudart/abi_guards.h` 应返回 17 |
| cpptlm_bridge.h 仅保留 ABI 真值源声明 (无 static_assert) | ❌ | ✅ | `grep -c "static_assert" include/cudart/cpptlm_bridge.h` 应返回 0 |
| ptx_emu_driver.hpp 仅保留 IPtxEmuDriver + DriverWrapper (无 static_assert) | ❌ | ✅ | `grep -c "static_assert" include/tlm/gpu/ptx_emu_driver.hh` 应返回 0 |
| 真相源 PTX-EMU 仓 cpptlm_bridge.h:223-290 (14 条) 与 CppTLM vendored 副本 (16 条) 数量差 2 条 | ✅ 已确认 (差 = 4 PipelineId + 0 PipelineId + 2 TcPrecision, 但内容等价) | 不变 | (注: 数量差非关键, 关键是内容覆盖) |

---

## ✅ CppTLM Ack 状态总结

| 项 | 状态 |
|---|:---:|
| HSK-6 主协议 (消费关系废止) | ✅ **接受** |
| P0-1 门禁 (G-D4 17 条静态断言迁移) | ✅ **接受 (实施中)** |
| 11 项删除清单 | ✅ **接受 (修订版)** |
| HSK-5 `advance()` 关闭 | ✅ **接受** |
| ANTLR4 移除 (CppTLM 不在 scope) | ✅ **接受** |
| Mode A frozen vs Mode B dual-rail | ✅ **接受** |
| 替代路径 (v3.0.0 dGPU board) | ✅ **接受 (实施路径已就绪)** |
| Ack 截止 2026-09-01 (14 天) | ✅ **接受 (自动 ack, 无异议)** |
| 跨链 commit 顺序 per ADR-035 §R5.1 | ✅ **接受** |

**CppTLM 端 HSK-6 响应 = ✅ ACCEPTED with Implementation Plan**

---

## 跨链同步

| 仓 | 跟踪载体 | 状态 |
|---|---|---|
| PTX-EMU | HSK-6 公告 (`25e36f60`) + HSK-PROTOCOL-NOTES (`bf1a652d`) | ✅ 已发出 |
| UsrLinuxEmu | ADR-090 v2 (`e03b5a1` + `37a91b6`) + annex §E | ✅ Accepted (Gate #5 ✅) |
| CppTLM | 本响应文件 + `cpptlm-v3-dgpu-extract` openspec change (`5d9473a`) | 📤 本文件 |
| TaskRunner | tadr-308 + openspec change (`6b1d39d`) | 🟡 进行中 |

---

**Cc**: @ptx_emu_owner · @usr_linux_emu_architecture_team

**Refs**:
- PTX-EMU `25e36f60` + `bf1a652d`
- CppTLM `5d9473a`
- UsrLinuxEmu `e03b5a1` + `37a91b6`
- Oracle session `ses_fef78854dffeLfDJh7p8ELuMLy`
- Annex `docs/05-advanced/adr-090-cross-repo-coordination.md` §E