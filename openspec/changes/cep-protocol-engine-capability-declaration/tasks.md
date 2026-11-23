## Tasks

> 来源：`.rddf/plans/cep-protocol-engine-capability-declaration.md`（TDD 5 步 · 8 个 Work Unit）
> 判据：proposal `## 验收标准` AC-1 ~ AC-13。
> 实施者：builder · 2026-11-23 · worktree `.rddf/wt/cep-protocol-engine-capability-declaration/`

- [x] Task 1「通用层骨架 + 自洽谓词」：`include/tlm/cep/engine_capabilities.hh` —— `EgressMode` / `InFlightModeling` / `EngineCapability` / `kEngineCapabilities[8]` / `engine_capabilities_are_consistent`（纯函数）
- [x] Task 2「8 行声明表填实值」：8 行 + 每行 `basis` 非空 + `kEgressHasMachineCheck = false` + `kMaxTransientStatesPerInstance = 1`（现状锁）
- [x] Task 3「SFINAE 检测 + 主模板 dependent-false」：`HasInFlightApi<T>`（探测 `T::txn_in_flight`）+ `EngineTypeTraits<T>` 主模板 `static_assert(!sizeof(T*), ...)` + `in_flight_declaration_matches<T>()`
- [x] Task 4「5 个引擎头 `EngineTypeTraits<T>` 特化 + J-1」：`chi_rnf` = `Modeled`；`mesif` / `moesi_gpu` / `ualink_4` / `tilelink_c` = `NotModeled`；测试逐实例 `static_assert`（消息含实例 id 字面量）
- [x] Task 5「J-3 瞬态态策略（编译期）」：对 5 个持 `kTransientStateCount` 的 adapter 逐 `static_assert`（`ualink_adapter` / `nvlink_adapter` 无该字段 ⇒ 显式排除）
- [x] Task 6「闸 `scripts/test/cep_engine_capabilities_check.sh`」：双向零 orphan + 引擎头文件集反向存在性（6 个 = 5 实例引擎 + 1 Null 外壳）+ `basis` 非空 + 单真源扫描 + 三态 SKIP/PASS/FAIL
- [x] Task 7「probe TU 反向验证（AC-3 / AC-4）」：基线编译通过 + 变异编译失败且命中 `chi_rnf` / `mesif` 字面量签名
- [x] Task 8「CMake 挂测试 + 全量回归」：`test/CMakeLists.txt` 加 2 条 `add_test`（`cep_engine_capabilities` / `cep_engine_capabilities_check`）+ 全量构建 + 回归

## 实施证据（builder 自测 · 2026-11-23）

| 项 | 命令 | 结果 |
|---|---|---|
| 新测试 | `ctest -R ^cep_engine_capabilities$` | ✅ 87 assertions / 5 cases 全过 |
| 新闸 | `bash scripts/test/cep_engine_capabilities_check.sh` | ✅ 全部判据通过（含 4 条 probe） |
| 闸反向 fixture | `touch include/tlm/cep/fake_protocol_engine.hh` ⇒ 重跑 ⇒ `rm` | ✅ 必红 → 还原必绿 |
| `openspec validate --strict` | 对 change | ✅ valid |
| `cep_docs_sync_check.sh --strict` | 全量 | ✅ rc=0 |
| `cep_doc_ref_check.sh` | 全量 | ✅ rc=0 |
| 全量构建 | `ninja -j4`（全部 target） | ✅ FULL_RC=0 / 0 error |
| ctest -R cep（worktree） | 111 项 | ⚠️ 5 项红 —— **全部为 worktree 环境产物**（见下） |

### worktree ctest 5 项红的归因（⛔ 不是本 change 引入）

| # | 测试 | 红因 | 判据 |
|:--:|---|---|---|
| 88 | `cep_stub_cache_core` | 读 `openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core/...`（worktree **缺失**） | 同一测试在**主仓** `./build/bin/cpptlm_tests "[cep][stub-cache-core]"` = **224 assertions 全过** |
| 89 | `cep_scenario_anchor_reconciliation` | 同上（同一归档路径缺失） | 实测 `MISSING openspec/changes/archive/2026-10-25-cpptlm-cep-stub-cache-core` |
| 90 | `cep_all` | 聚合 `[cep]` ⇒ 含 88/89 | 传递 |
| 155 | `cep_registry_baseline_check` | **仅**两项归档计数：文档 `170/54` ≠ 实测 `69/30` | worktree 归档目录数实测 = **69**；主仓 = **170** |
| 183 | `cep_full_green_run_check` | 需既有 experiment `report.html` 产物（无） | `未渲染「✓ 一致性闸门通过」` |

**根因**：worktree 是干净 checkout，`openspec/changes/archive/` 的绝大部分目录**不在库内**（主仓 170 vs worktree 69）⇒ 读归档路径的判据在 worktree 恒红。
**⇒ 权威验收应在合并后的主仓执行**（届时归档计数应为 171/55，须同步 §0.1）。

## 本 change 引入的计数漂移（已同步）

| 面 | 原 | 现 | 落点 |
|---|:--:|:--:|---|
| `cep_*.sh` 闸数 | 49 | **50** | `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §0.1 |
| 闸族合计 | 63 | **64** | 同上 |
| `test_cep_*.cc` | 77 | **78** | 同上 |
| OpenSpec active | 0 | **1** | 同上 + KPI 活口径 6 处（`AGENTS.md` / `roadmap.md` / `phase-10.md` / `obj §5.2` / 2 个 improvement） |
