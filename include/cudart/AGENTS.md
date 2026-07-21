# cudart/ Vendored Headers

## cpptlm_bridge.h (ABI 头文件)

- **Source**: `/workspace/project/PTX-EMU` 本地仓库 @ commit `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d`
- **Public repo**: github.com/chisuhua/PTX-EMU @ commit `8dc000eca9f78e8ee017eafcb305eb4ca62ffd6d`
- **Vendor method**: 从 PTX-EMU 本地仓库 commit 显式提取（`git show 8dc000ec:include/cudart/cpptlm_bridge.h`），不复制 working tree
- **ABI 真值源**: PTX-EMU 是 ABI 提供方（CppTLM 通过 HSK-3 选项 1 `ExternalProject_Add` 消费）
- **SHA-256 (commit 8dc000ec)**: `c19e66a32de398e6bba2042f3f19923ff89dbc02f10bbf310c073ad3a8ff3dbe`
- **Last verified**: 2026-07-15（vendor 与 PTX-EMU commit 8dc000ec 字节级一致）
- **Sync policy**: HSK-1 每次重发时手动同步（`git show <new-hash>:include/cudart/cpptlm_bridge.h`）
- **Replaced by**: 未来 HSK-3 选项 1（ExternalProject_Add）实施后，此 vendor 改为动态拉取（无需手动同步）

## Phase 8.B HSK-4 纯虚接口头文件（PTX-1/2/3, vendored 2026-07-18）

### scoreboard_interface.h

- **Source**: github.com/chisuhua/PTX-EMU @ commit `8acfd2d1` (HSK-4 PTX-1)
- **Source path**: `include/ptxsim/scoreboard_interface.h` (16 lines)
- **Vendor method**: `git show 8acfd2d1:include/ptxsim/scoreboard_interface.h` + 添加 vendor 头注释（不改原文）
- **SHA-256 (commit 8acfd2d1)**: `4935b50fdea76e8ceb3d37374fd5e453b8d40419a02effd4ffb75c5c3ed63f15`
- **ABI 真值类**: `IScoreboard`（无依赖, 仅 include `<cstdint>`）

### pipeline_interface.h

- **Source**: github.com/chisuhua/PTX-EMU @ commit `9e7361b9` (HSK-4 PTX-2)
- **Source path**: `include/ptxsim/pipeline_interface.h` (29 lines)
- **SHA-256 (commit 9e7361b9)**: `14c4b8209fc22de24e86312b3a77596491e66e5981dd97e51a676efdb30dd681`
- **ABI 真值类**: `IPipelineLatencyProvider` + enum class `PipelineId` (6 值, P0_INT_FP32=0 ... P4_TC=5)

### tensor_core_interface.h

- **Source**: github.com/chisuhua/PTX-EMU @ commit `463038e0` (HSK-4 PTX-3)
- **Source path**: `include/ptxsim/tensor_core_interface.h` (31 lines)
- **SHA-256 (commit 463038e0)**: `6ff8351d01feea3c2b783e047738df5ddf91a4dfbc8e492b9e3cfcb93d11e4a5`
- **ABI 真值类**: `ITensorCoreTiming` + enum class `TcPrecision` (6 值, FP4=0 ... TF32=5)

### 共同策略

- **Sync policy**: HSK-4 每次重发时手动同步（`git show <new-hash>:include/ptxsim/<file>` + 更新 SHA-256）
- **G-D4 验证门**: 由 `cpptlm_bridge.h` 末尾 `namespace abi_guards_g_d4` 内的 12 端点 + 4 签名级 `static_assert` 强制，编译失败即 ABI drift
- **Replaced by**: 未来 HSK-3 选项 1（ExternalProject_Add）实施后，3 个 vendor 改为动态拉取（无需手动同步）

## 验收检查

- [x] 2026-07-15 — `cpptlm_bridge.h` 与 PTX-EMU commit 8dc000ec 字节级一致
- [x] 2026-07-16 — `cpptlm_bridge.h` re-vendor from 603bd8bc (B1 cpptlm_attach_bridge + PTXEMU_BRIDGE_API)
  - SHA-256: `ca716a8179841da6de76e0c54406c76d21e42ca3cb8e08a8cd48907f865fe5e7`
  - 变更: 新增 `cpptlm_attach_bridge()` / `cpptlm_detach_bridge()` extern "C" 入口 + `PTXEMU_BRIDGE_API` 可见性宏 + `g_cpptlm_bridge` 全局指针声明 + cudaStream_t 兼容层
- [x] 2026-07-18 — HSK-4 三接口头 vendored from PTX-EMU stable commits 8acfd2d1 / 9e7361b9 / 463038e0
  - SHA-256: 上述 3 文件 vendor SHA-256 验证（vendor body 字节级一致，去除 vendor 头注释后）
- [x] 2026-07-18 — **G-D4 编译期验证门 PASS** (16/16 static_assert)
  - CppTLM cmake build PASS（含 cpptlm_sim / cpptlm_tests / 多个 demo target）
  - Negative test 验证断言生效：故意 `TcPrecision::FP4 = 1` 触发 `cpptlm_bridge.h:213` 编译错误 `static assertion failed: G-D4 ABI drift: TcPrecision::FP4 != 0 (expected 0)`
  - 还原后再次 build PASS（无副作用）
  - docs_sync_check --strict PASS (366/366 路径有效)

- [x] 2026-07-21 — **S0 HSK-4/5 Rebase + CI 验证门通过** (808/808 PASS, 18829 assertions)
  - PTX-EMU 3 接口头文件 commit 无变化: `8acfd2d1`/`9e7361b9`/`463038e0` 保持锁定
  - `cpptlm_bridge.h`: PTX-EMU `97539fdb` 仅 `__attribute__((weak))` 从声明移至定义，CppTLM vendored 声明已对齐（无 weak）
  - `cpptlm_bridge.h` 函数签名无变化 (`PtxEmuDriverApi` struct 布局不变, `sizeof==64` 保持)
  - G-D4 16/16 static_assert 编译通过 (29 GPU/D1P1 tests, 3256 assertions PASS)
  - S0→S2 门禁: `static_assert` 16/16 绿 ✅ — 可推进 Phase 1.1 poll_kernel 修复

## 文件名一致性验证（2026-07-18）

通过 `grep -rn "pipeline_latency_provider_interface\|tensor_core_timing_interface" openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/` 确认：
- spec.md / proposal.md / design.md / tasks.md 全部使用正确文件名 `pipeline_interface.h` 和 `tensor_core_interface.h`
- 无误命名残留需要 sweep

（先前"已知修正"段为初次审计时未经验证的推测，与实际 openspec 文档不符；本次 grep 验证消除该虚警。）
