# HSK-1/2/3 + D1-Full 状态回复 (CppTLM → PTX-EMU)

> **日期**: 2026-07-17
> **发送方**: CppTLM Team (Sisyphus)
> **接收方**: PTX-EMU Architecture Team
> **回传目标**: `#cpptlm-integration` Slack 频道 / PTX-EMU PR comment
> **形式**: 结构化确认 + commit hash 引用
> **关联**: PTX-EMU HSK 草稿 `docs/superpowers/hsk-drafts/2026-07-16/HSK-{1,2,3}-*.md`

---

## HSK-1 闭环 (cppTLMBridge ABI) ✅ **Closed**

> **锁定 commit hash**: `73e5422` (P0 main merge, 776/776 用例 / 15562 断言 + 12/12 [f12b] tests)

### 逐项确认

| PTX-EMU 请求 | CppTLM 状态 | 证据 |
|-------------|-----------|------|
| 1. rebase feature/d1-full-impl 到 PTX-EMU `8dc000ec` | ✅ 已完成 | CppTLM main HEAD `73e5422` 含 `603bd8bc` re-vendor (PTXEMU_BRIDGE_API + cpptlm_attach_bridge + cpptlm_detach_bridge) |
| 2a. `version() == 1` (匹配 CPPTLMBRIDGE_VERSION) | ✅ 匹配 | `include/tlm/gpu/memory_bridge.hh:47` `return CPPTLMBRIDGE_VERSION` |
| 2b. `submit_kernel` 12 参数签名 | ✅ 匹配 | `memory_bridge.hh:50-54` 与 `cpptlm_bridge.h:100-106` 逐字节一致 |
| 2c. `poll_kernel` 返回 0/UINT64_MAX | ✅ 实现 | `test_memory_bridge.cc` 12 用例覆盖 (含 `poll_kernel_returns_UINT64_MAX_on_unknown_id` + `poll_kernel_returns_0_on_completed`) |
| 2d. `global_access` LD/ST timing-only | ✅ 实现 | `memory_bridge.cc::global_access` 调用 `gpu_xbar_->query_latency()`,数据由 PTX-EMU SimpleMemory 完成 |
| 3. CI 12 端点 `static_assert` | ⏳ 待 P1 实施时同步加入 | `2b28505` RFC-P1-003 已锁定 enum 值 |
| 4. 回复 commit hash + version() 返回值 | 📤 本文件 | `73e5422` + `version()=1` |

### Commit hash 回传

- **CppTLM main**: `73e5422 feat(main): --f12b-ld flag + MemoryBridge wiring + 12 C++ 单测 + 烟雾测试`
- **P0 archive**: `b94eccc chore(openspec): 归档 cpptlm-f12b-ld-impl (P0 D1-Full MemoryBridge)`
- **P2 占位**: `e69cd1d feat(tlm/gpu): AsyncCompletionAdapter placeholder (D1-Full P2 #C5)`

### 双 SHA 锁定文档

`include/cudart/AGENTS.md`:
- HSK-1 原始基线: PTX-EMU commit `8dc000ec` (2026-07-15)
- 2026-07-16 re-vendor: PTX-EMU commit `603bd8bc` (新增 cpptlm_attach_bridge + 可见性宏)
- CPPTLMBRIDGE_VERSION = 1 (类接口签名未变, version bump 未触发)

---

## HSK-2 闭环 (ANTLR4 4.13.2) ✅ **Closed (N/A for CppTLM)**

### 逐项确认

| PTX-EMU 请求 | CppTLM 状态 |
|-------------|-----------|
| 1. 接收 PTX-EMU commit `759836f0` ANTLR4 4.13.2 升级 | ✅ **确认收到** (2026-07-15 HSK response 已 lock 4.13.2,见 `2026-07-15-cpptlm-hsk-response.md`) |
| 2. CI 加入 ANTLR4 版本 `static_assert` | ❌ **不需要** — CppTLM 不使用 ANTLR4 |
| 3. CppTLM 是否需要 ANTLR4 升级 | ❌ **不需要** — CppTLM 端无 ANTLR4 引用 |
| 4. 回复确认收到 | 📤 本文件 |

### 架构理由 (不升级,不引入 static_assert)

ANTLR4 用于 PTX 解析:
- PTX-EMU 端 `src/grammar/ptxLexer.g4` / `ptxParser.g4` → 生成 C++ 解析器
- CppTLM 接收的是 PTX-EMU 已解析的 ABI 接口 (CppTLMBridge 5 虚方法),**不接触 ANTLR4**
- 若 CppTLM 端加 ANTLR4 `static_assert` 反而引入错误耦合 (依赖 PTX-EMU 私有工具)
- ANTLR4 jar (4.13.2) 完全 vendored 于 PTX-EMU 仓库 `antlr4/` 目录,无跨仓依赖

### CppTLM 端 ANTLR4 引用检查

```bash
grep -r "antlr" /workspace/project/CppTLM/include /workspace/project/CppTLM/src /workspace/project/CppTLM/external 2>/dev/null | grep -v "external/json"
# → 空 (CppTLM 端无 ANTLR4 引用)
```

---

## HSK-3 闭环 (libcpptlm_cudart.so CMake) ✅ **Closed**

### 逐项确认

| PTX-EMU 请求 | CppTLM 回复 |
|-------------|-----------|
| 1. 接受选项 1 (ExternalProject_Add) | ✅ **接受** (per `2026-07-15-cpptlm-hsk-response.md` "首选方案已确认") |
| 2. 备选偏好 (选项 2/3) | ❌ **无,选项 1 已满足需求** |
| 3. 提供 `CPPTLM_COMMIT_HASH` | ✅ 推荐 **`73e5422`** |
| 4. git remote URL 一致性 | ✅ **确认** `https://github.com/chisuhua/CppTLM.git` |

### CPPTLM_COMMIT_HASH 选择说明

**推荐 `73e5422`**,理由:
- P0 全部 10 commits 已合并到 main
- 含 MemoryBridge + KernelLaunchTLM extension + `--f12b-ld` flag + vector_add 烟雾测试
- 测试基线 776/776 用例 / 15562 断言 + 12/12 `[f12b]` 用例
- P0 change `cpptlm-f12b-ld-impl` 已归档 (`b94eccc`)
- AsyncCompletionAdapter 占位已实施 (`e69cd1d`)

**不推荐 `main`** (per cross-repo review §8.3 L1, 生产前必须固定 SHA)

**不推荐 `2b28505`** (仅为 RFC 文档提交,不含代码基线变更)

### CppTLM 端 CMake 集成状态

`src/CMakeLists.txt:46`:
```cmake
tlm/gpu/memory_bridge.cc   # D1-Full P0: CppTLMBridge 接口实现
```

`include/cudart/cpptlm_bridge.h` (vendor from PTX-EMU commit `603bd8bc`):
- `PTXEMU_BRIDGE_API` 可见性宏 (visibility default / Windows dllexport)
- `cpptlm_attach_bridge()` / `cpptlm_detach_bridge()` extern "C" 入口
- `cudaStream_t` 兼容层 (与 cudart_intrinsics.h 一致)
- `CPPTLMBRIDGE_VERSION = 1` 编译期断言
- `static_assert(sizeof(cudaStream_t) <= sizeof(uint64_t))`

### git remote URL 一致性

| 端 | URL | 状态 |
|----|-----|------|
| CppTLM remote | `https://github.com/chisuhua/CppTLM.git` | ✅ 确认 |
| PTX-EMU CMakeLists.txt 草案 | `https://github.com/chisuhua/CppTLM.git` | ✅ 一致 |

---

## D1-Full 实施状态 (informational)

> **状态截至 2026-07-17**,所有路径均在 CppTLM main 分支 (`73e5422` 为最新 P0 commit)

| 项 | 状态 | 证据 / Commit |
|----|------|--------------|
| MemoryBridge 5 虚方法 | ✅ 已实施 | `73e5422` (`include/tlm/gpu/memory_bridge.hh` 86 行 + `src/tlm/gpu/memory_bridge.cc` 147 行) |
| MemoryBridge 12 [f12b] 单测 | ✅ 12/12 PASS | `73e5422` (`test/test_memory_bridge.cc` 12 用例 / 15 断言) |
| KernelLaunchTLM P0 扩展 | ✅ 已扩展 (4 Adapter setter 预留) | `73e5422` (P0 钩子,非 Phase 8.A 旧 stub) |
| `--f12b-ld` flag wiring | ✅ 已实施 | `73e5422` (`src/main.cpp`) |
| vector_add 烟雾测试 (G-F0) | ✅ 已实施 | `73e5422` (`configs/vector_add_n1024.json` + `test/python/test_f12b_smoke.py`) |
| cpptlm-f12b-ld-impl 归档 | ✅ 已归档 | `b94eccc` |
| cpptlm-d1-p1-pipeline-scoreboard change | 🟡 Proposed (待 PTX-EMU Phase 1 接口) | `openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/` |
| AsyncCompletionAdapter 占位 (P2) | ✅ 已实施 | `e69cd1d` (`include/tlm/gpu/async_completion_adapter.hh` + 5 `[gpu][async]` 单测) |
| **3 核心模块** (Scoreboard/Pipeline/TensorCore) | ⏳ **阻塞** — 等待 PTX-EMU `cpptlm-phase8b-injection-points` Phase 1 (3 接口头文件) | RFC `2b28505` (RFC-P1-001 + RFC-P1-003 enum 值锁定) |
| **4 Adapter** (WarpScheduler + Scoreboard + Pipeline + TC) | ⏳ **阻塞** — 同上 | RFC `2b28505` (RFC-P1-002 + RFC-P1-004) |
| 12 端点 `static_assert` (PipelineId + TcPrecision) | ⏳ **阻塞** — P1 实施时同步 | RFC `2b28505` (RFC-P1-003) |
| CMake `memory_bridge` 目标 | ✅ 已集成 (`cpptlm_core` 静态库) | `73e5422` (`src/CMakeLists.txt:46`) |

### 阻塞关系图

```
PTX-EMU 端 (需先实施):
  cpptlm-phase8b-injection-points Phase 1 (3 接口头文件)
   │
   ├─ include/ptxsim/scoreboard_interface.h
   ├─ include/ptxsim/pipeline_interface.h
   └─ include/ptxsim/tensor_core_interface.h
        │
        ▼
CppTLM 端 (可启动):
  cpptlm-d1-p1-pipeline-scoreboard Phase 1 (3 核心模块)
   │
   ├─ include/tlm/gpu/scoreboard_tlm.{hh,cc}
   ├─ include/tlm/gpu/pipeline_tlm.{hh,cc}
   └─ include/tlm/gpu/tensor_core_tlm.{hh,cc}
        │
        ▼
  cpptlm-d1-p1-pipeline-scoreboard Phase 2 (4 Adapter)
   │
   ├─ include/tlm/gpu/adapter/cpptlm_warp_scheduler_adapter.{hh,cc}
   ├─ include/tlm/gpu/adapter/cpptlm_scoreboard_adapter.{hh,cc}
   ├─ include/tlm/gpu/adapter/cpptlm_pipeline_adapter.{hh,cc}
   └─ include/tlm/gpu/adapter/cpptlm_tensor_core_adapter.{hh,cc}
```

---

## 跨仓库握手链 (commit hash 互相引用)

```
CppTLM 端:                                       PTX-EMU 端:
─────────                                        ──────────
b94eccc P0 archive  ──────────┐
                             │
e69cd1d P2 AsyncCompletion ──┤   引用 →  6b367cad hsk-3 Ready to Send
                             │              (含 CPPTLM_COMMIT_HASH=73e5422)
2b28505 RFC-P1-001~004 ──────┤
                             │            df05e10b Phase 0 对齐
3d83a1e B1-B4 文档修复 ─────┤            (已锁定 PTX-0.1/0.2/0.4)
                             │
ea60cbc P0 tasks.md 勾选 ───┤
                             │
73e5422 P0 main merge ──────┘ ←── 推荐 HSK-3 锁定
```

---

## 待 PTX-EMU 端推进事项 (informational, 不阻塞 CppTLM)

1. **HSK-1/2/3 实际发出**: 草稿齐备 (`docs/superpowers/hsk-drafts/2026-07-16/`),需 PTX-EMU Architecture Team 手动复制到 `#cpptlm-integration` Slack 频道
2. **PTX-0.3 序列化协调**: `cleanup-deprecated-barrier-apis` / `god-class-refactor-thread-context-phase3` / `migrate-bar-warp-sync-to-barrier-module` 三 change 的依赖关系协调
3. **PTX-0.5 基线 worktree 建立**: Phase 1 实施前 1 分钟建立 `ptxemu-baseline-2026-07-XX`
4. **`cpptlm-phase8b-injection-points` change 状态 Proposed → Active**: 执行 `openspec change` 子命令
5. **Phase 1 实施**: 3 接口头文件 (`include/ptxsim/scoreboard_interface.h` 等)
6. **`cpptlm-d1-full` 验收门 8 项 + Phase 3.8 集成测试**: 闭合 OpenSpec change 归档前置

---

## References

- CppTLM HSK-1/2/3 历史响应: [`2026-07-15-cpptlm-hsk-response.md`](2026-07-15-cpptlm-hsk-response.md)
- CppTLM P1 RFC: [`2026-07-16-rfcs-to-ptxemu-p1-injection.md`](2026-07-16-rfcs-to-ptxemu-p1-injection.md)
- CppTLM 综合任务书: [`2026-07-14-ptxemu-comprehensive-modification-plan.md`](2026-07-14-ptxemu-comprehensive-modification-plan.md)
- CppTLM P1 计划: [`../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/`](../openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/)
- CppTLM P0 归档: [`../openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/`](../openspec/changes/archive/2026-07-16-cpptlm-f12b-ld-impl/)
- PTX-EMU HSK 草稿: `/workspace/project/PTX-EMU/docs/superpowers/hsk-drafts/2026-07-16/`
- PTX-EMU Phase 0 对齐 commit: `df05e10b`
- PTX-EMU HSK-3 Ready to Send commit: `6b367cad`

---

**最后更新**: 2026-07-17 (CppTLM 端确认 HSK-1/2/3 + D1-Full 状态)
**下次更新**: PTX-EMU Phase 1 (3 接口头文件) 完成后,启动 CppTLM P1 Phase 1 (3 核心模块)