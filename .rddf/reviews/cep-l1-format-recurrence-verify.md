# 验收报告 · **G-19**（L1「CI 红线」第三次复发 —— 21 文件 `clang-format` + O-3 观察项处置）

- **角色**：builder（实施侧自验）
- **日期**：2026-11-23 · **基线 commit**：`8b7f74ae`（G-18）
- **承载缺口**：**G-19** —— `scripts/build/format.sh --check` 实测 **rc=1 / 21 个文件需格式化** ⇒ CI `code-format` job 会红
- **授权来源**：用户 **「B」** —— 承接上一轮选项表「**B · 登记 + 一起修**」

## 判据来源

| 来源 | 位置 |
|---|---|
| **用户拍板** | 上一轮选项表「B · 登记 + 一起修」 |
| **观察项 O-3** | 登记册 `:409` —— 「`obj-cep-platform.md:970` 的 L1『CI 红线』与 `:16457` 的『已清』互斥」 |
| **CI 判据** | `.github/workflows/ci.yml:180-204` —— `code-format` job 真跑 `./scripts/build/format.sh --check` |
| **豁免口径** | 本仓纪律「阻断不排期」仅适用于非阻断项；CI 红线**阻塞一切合并** ⇒ 不属「仅登记不排期」 |
| **形态先例** | Entry #275（2026-11-07 清 13 个）· Entry #360（2026-11-12 清 10 个）⇒ **第三次复发**，处置形态成文 |

## G-19 逐条判定

| # | 判据 | 实测 | 结论 |
|:--:|---|---|:--:|
| **1** | **O-3 的两行不互斥，是时间序** | `:970` = §10 的 **2026-11-07 规划快照**（L1 当时待办）；`:16457` = **Entry #275 同日稍后**（13 文件已清）⇒ 时间序自洽，⛔ 不是记录矛盾 | ✅ |
| **2** | **但两者都不描述当下** | `bash scripts/build/format.sh --check` ⇒ **rc=1**，输出「❌ **21 个文件需要格式化**」 | ✅ |
| **3** | **红不是版本漂移造成的** | 本地 `clang-format 22.1.6` == CI pin 的 `clang-format-22`（`ci.yml:186-199`） | ✅ |
| **4** | **修完 check 归零** | 修复后 `format.sh --check` ⇒ **rc=0** · 「✅ 格式化完成！」 | ✅ |
| **5** | **只动该动的文件** | 改动文件集合 == 修复前 `--check` 报出的 21 个（`diff` 集合一致）· 共 **21 files · +892/−856** | ✅ |
| **6** | **代码体零语义变更** | 去注释 / 去空白 / 剔除 include 后逐文件比对：**21/21 完全相同** | ✅ |
| **7** | **include 集合零变更** | 去重后 include 多重集逐文件比对：**21/21 相同**；唯一差异 = `test_cep_mesh_e2e.cc` 的**重复 `#include <cstdio>` 被去重**（20→19，clang-format `SortIncludes` 的 dedup 行为） | ✅ |
| **8** | **注释正文零语义变更** | 去注释标记 / 去空白后逐文件比对：**21/21 完全相同**（唯二非空白差异 = 两处注释折行新增的 `//` 续行标记） | ✅ |
| **9** | **行为零变更（回归）** | 重编译 `ninja: no work to do`（`verify_test_seam_fix.sh` 已用格式化后源码重编）· 9 核心标签 **603 cases / 49535 assertions 全过** · `ctest` 全量见下 | ✅ |
| **10** | **不触 CEP 冻结面** | 21 文件全在 `src/**` 与 `test/**`；`cep_freeze_check.sh` rc=0（90 产物 hash 不变） | ✅ |

## 行数 / sha256 全表（21 文件）

| 文件 | 行数（前 → 后） | sha256 前 12 → 后 12 |
|---|---|---|
| `src/core/module_factory.cc` | 1607 → 1610 | `72fdec5fcc30` → `7123962fd29c` |
| `src/main.cpp` | 203 → 206 | `a574d1295372` → `b0b574a6f301` |
| `src/tlm/gpu/dgpu_board_shell.cc` | 1292 → 1292 | `a4837333ff26` → `70cfbc120964` |
| `src/tlm/gpu/sdma_engine_tlm.cc` | 808 → 808 | `9855e0226135` → `deffe6d5e4f7` |
| `src/tlm/nic_tlm.cc` | 206 → 206 | `73fe90c2e22d` → `1e73c54bb9ad` |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | 849 → 847 | `da7cfeac8f72` → `55b418616a0c` |
| `test/test_cep_instance_declaration_contract.cc` | 743 → 746 | `95d34cddc628` → `57d6d065cd8c` |
| `test/test_cep_mesh_e2e.cc` | 533 → 536 | `3bf0c55b136c` → `d3142f7146ae` |
| `test/test_cep_sharer_tracker_base.cc` | 1010 → 1010 | `cd0e95bc4af2` → `f8992530c6a8` |
| `test/test_config_type_registration.cc` | 133 → 138 | `e67156768556` → `c369b08a5ac6` |
| `test/test_dgpu_msix_cap.cc` | 138 → 136 | `2060187ee8d9` → `f9ab3b4671de` |
| `test/test_memory_tlm_backing.cc` | 194 → 194 | `e3d17766e43d` → `5226a8eb3dcf` |
| `test/test_nested_failure_propagation.cc` | 122 → 120 | `931e86732028` → `b609b259456e` |
| `test/test_nictlm_credit_filter.cc` | 321 → 322 | `4e0928214a98` → `d1491d88686d` |
| `test/test_noc_mesh_binding_semantics.cc` | 330 → 336 | `ff9720a5c970` → `c15d1850516e` |
| `test/test_param_migrate_batch2.cc` | 1111 → 1113 | `46a82fc2dff8` → `013f67db18f8` |
| `test/test_realip_facade_wiring.cc` | 184 → 183 | `789f707fccbe` → `d4362c7c8418` |
| `test/test_router_buffer_size_param.cc` | 196 → 197 | `3236cc99f6cd` → `5a7133ca6204` |
| `test/test_router_local_port_credit.cc` | 160 → 159 | `87e2619279f3` → `e5f71c7471a9` |
| `test/test_sdma_engine_timing.cc` | 271 → 271 | `6e708f72bac0` → `39ae79a9b74c` |
| `test/test_simmodule_exposed_port_wiring.cc` | 343 → 360 | `7ed510c1dfaf` → `ce5965daa657` |

## 附带发现（本轮验证时撞到，均**预存在**且**不在 CI / ctest 内**）

| # | 发现 | 取证 | 判定 |
|:--:|---|---|:--:|
| **a** | `check_pcie_ip_facade.sh` **rc=1** —— `pcie_completer_engine.cc:118-119` 访问 `PcieEndpointIP` internals | 该文件**不在本轮 21 个改动内**（`git status` 空）· HEAD 版本同内容 · 违规自 **`d1a2239f`「2026-09-17 feat(completer)」** 引入 ⇒ 与本轮格式化无关 | 预存在 · 非 CI |
| **b** | `verify_test_seam_fix.sh` STEP 5 **rc=1** —— 期望 `601 cases / 49525 assertions`，实测 `603 / 49535` | 期望值**硬编码**于该脚本 `:269`（脚本最后改动 = `1acd7f91` · 2026-10-12）；两脚本均**未注册 ctest**、**不在 `ci.yml`** ⇒ 期望随新增测试静默陈旧 | 预存在 · 非 CI |

> 两项已登记进登记册 §6.2 的「仓级」段（**仅登记不排期**）；⛔ 本轮不修 —— 修它们要改闸判据面（含 `clang-format` 之外的门禁口径），属范围外。

## 失败项与路由建议

| # | 项 | 路由 |
|:--:|---|---|
| — | 本缺口无失败项 | — |
| a / b | 预存在的两个闸假红 | 已登记「仓级」段 · 建议后续单独立项（改闸判据） |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 临时诊断代码 / 临时文件 | ✅ 零仓内残留（比对脚本与备份均在 `/tmp/opencode/`） |
| TODO / 半截实现 / 被 skip 的测试 | ✅ 无新增 |
| 未替换的 mock / 临时 fallback | ✅ 无 |
| 范围纪律 | ✅ 只改 `--check` 报出的 21 个文件 · 零 `include/**` · 零 `openspec/**` · 零 ADR 正文 · 零冻结面 |
| 行号漂移 | ⚠️ 15 个文件行数变化（−2 ~ +17）⇒ 仓内历史 `path:line` 引用（多见于 `.rddf/reviews/` 与 `docs/archive/`）**指向的行会漂移 1~17 行**；代码与注释正文逐字未变 ⇒ 按行读取的内容不变，仅编号偏移 |

## 回归记录

| 检查 | 命令 | 结果 |
|---|---|---|
| 格式化闸 | `./scripts/build/format.sh --check` | ✅ rc=0（修复前 rc=1 / 21 文件） |
| 构建 | `cmake --build build -j4` | ✅ `ninja: no work to do`（已由 verify 脚本以格式化后源码重编） |
| 9 核心标签 | `bash scripts/test/verify_test_seam_fix.sh` STEP 4 | ✅ 9/9 标签全过（`[pcie]` 452 cases / 36773 assertions 等） |
| 全量 ctest | `ctest --test-dir build` | ✅ **200/200 Passed**（365 sec） |
| 顺序机关 | `bash scripts/test/rddf_stage_gate.sh` | ✅ rc=0 |
| 活基线闸 | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0 |
| 旧口径残留闸 | `bash scripts/test/cep_registry_stale_refs_check.sh` | ✅ rc=0 |
| 文档路径同步 | `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |
| 冻结面 | `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0 |
| CEP 契约同步 | `bash scripts/test/cep_docs_sync_check.sh --strict` | ✅ rc=0 |
| 配置类型闸 | `bash scripts/test/config_type_registration_check.sh` | ✅ rc=0 |
| IAG 对账闸 | `bash scripts/test/cep_iag_gate_reconcile_check.sh` | ✅ rc=0 |

## 已用轮次

**1/3**（实施侧自验第 1 轮 · 零返工）。
