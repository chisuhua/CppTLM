# 验收报告 · **W4 批 1**（一揽子文本订正 · 零授权子项 W4-b / W4-c / W4-d / W4-f · 登记册 §6.4）

- **角色**：builder（**实施侧自验** —— 本批由同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **日期**：2026-11-23 · **基线 commit**：`084c04fa`（G-2 / G-12 记账遗漏修复）
- **承载缺口**：**G-4**（W4-b）· **G-5**（W4-c）· **G-6**（W4-d）· **G-11**（W4-f）
- **同批附带闭合**：**G-10**（W2-b 已落地，记账遗漏）· **G-14**（W2-e 已落地，记账遗漏）

## 判据来源

| 来源 | 位置 |
|---|---|
| **W4-b ~ W4-f 的逐条判据** | 登记册 `.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6.4 的 **W4** 段（子项表）+ §6.2 各缺口行 |
| W4 的**单 commit 纪律** | 同上 §6.9 阶段 ④（「⚠️ **刻意单 commit**」⇒ 登记册 §6.2 / §6.3 / §6.4 + TSV 锚点 + 追加式台账 Entry + 审稿记录**同一 commit**） |
| 台账 append-only 纪律 | 同上 §6.3（W3-c / W4-a 的「**追加式**」形态）+ 台账各 Entry 的「⛔ 台账 append-only ⇒ 不回改历史 Entry」 |

## 逐条判定

| 子项 | 缺口 | 判据 | 实测 | 结论 |
|:--:|:--:|---|---|:--:|
| **W4-b** | G-4 | 订正行**同时**含「伪缺口 / 已订正」标记 + 载明证伪证据；⛔ 不删原文 | §6.4 的 **I-11 行追加**（原行逐字保留、TSV anchor「**spec 契约面 stale 引用修正**」未动）· 订正含标记 + 三项证据：`test/test_pcie_slice_wire_format_snapshot.cc:62-71` 的 **29 条 `static_assert`** · `openspec/specs/pcie-slice-prerequisites/spec.md:21` 的 Scenario 点名该文件 · `test/CMakeLists.txt:96` 已注册 ctest | ✅ |
| **W4-c** | G-5 | 状态行回填 + §6.8 自查命令口径订正 | **7 份**单元状态行「🟡 已立项 / 待立项」→「✅ **已落地**」（I-5 / I-7a / I-7b / I-8 / I-9 / I-10 / I-11）；⚠️ 其中 **4 份**的队列行**未记**归档 change 名 ⇒ 状态行**如实**写「登记册该行未记归档 change 名」⛔ 不臆造；§6.8 的白名单**补入「已落地」**（本仓通用完成态 token，旧白名单只认四种 ⇒ 漏它）⇒ 现行命令实测输出 = **1 份**（`cep-freeze-lexicon-pointers` · 真实活义务） | ✅ |
| **W4-d** | G-6 | §6.3 **显式列行**登记 `cep-memory-model-operational-trace`（⛔ 不落兜底行） | §6.3 归属表**新增该单元的独立行**（状态 token「🟡 待立项」· ⏳ 待用户 ratify）⇒ ⛔ 原先只在 G-6 行里被提到 ⇒ **提到 ≠ 有归属行**（旧对账口径只查「是否被提到」⇒ 会假绿） | ✅ |
| **W4-f** | G-11 | R-19 旧串 `handle_cr_transaction` `grep -c` == **0** + R-17 的来源 change 按**正确形态**可达 | R-19：`AGENTS.md:423` **同行替换**为真实方法名 `cr_write()` / `doorbell_write()`（per `include/tlm/lwc/lwc_bus_hub.hh:94/107`）⇒ 旧串实测 **0**；⛔ **行数不变** ⇒ P-5 活口径锚点 `AGENTS.md:313` **零位移**。<br>R-17：**归档形态从未进过 git**（`.gitignore:64` 排除整个 `openspec/changes/archive/`）⇒ 正确判据 = **`git-history-proposal` 形态**（台账 Entry 的 M-3 已订正）⇒ 实测 `git show 0345d283:…/cpptlm-dgpu-soc-timing-mvp/proposal.md`（**168 行**）与 `git show 689752ef:…/cpptlm-driver-visible-minimal-soc/proposal.md`（**228 行**）**可达** ⇒ §6.3 的完成定义形态**同步订正**（⛔ **不要求**归档目录入库 ⇒ **无需 force-add**） | ✅ |

## 顺带闭合的两处记账遗漏（非本批实施，是登记册回填滞后）

| 缺口 | 实际闭合于 | 证据 |
|---|---|---|
| **G-10**（剩余 grep 判据未接闸） | **W2-b**（2026-11-22 · commit `40fe6b69`） | 新增 `scripts/test/cep_registry_stale_refs_check.sh`（ctest **#154**）⇒ 实测「**40 项全部为 0**」；⛔ §6.3.2 的「尚未接入闸」自述**同步订正**为「已接入闸」 |
| **G-14**（`--phase A\|B\|C` 恒红） | **W2-e**（2026-11-22 · commit `40fe6b69`） | 闸内退役判定 ⇒ `--phase A` / `B` / `C` 三条**实测均 rc=0** |

## 两处**原计划的事实错误**（实施期实测发现，已订正）

| # | 登记册原述 | 实测 | 处置 |
|:--:|---|---|---|
| **E-1** | W4-c 要回填「**8** 份」状态行 | 真 stale = **7** 份（I-5 / I-7a / I-7b / I-8 / I-9 / I-10 / I-11）—— 第 8 份的成因是 **2026-11-22 新建的 `cep-freeze-lexicon-pointers`**（状态「🟡 已立项」⇒ **真实活义务**，非 stale） | 回填 **7** 份 + §6.8 口径改为「**1 份活义务**」⇒ ⛔ 不把活义务当 stale 回填成「已落地」 |
| **E-2** | §6.8 的白名单 = `已归档\|已实施\|已拆分\|已作废` | 漏了**「已落地」**（本仓通用完成态 token，登记册与台账里到处都是「✅ 已落地」）⇒ 7 份回填成「已落地」后**仍会被判 stale** | 白名单**补入「已落地」**；⚠️ 另记录口径的**域 = 文件全文**（⛔ 非仅 `状态:` 行）⇒ `cep-memory-model-operational-trace` 因文内他处提及而落进白名单 |

## 失败项与路由建议

| # | 项 | 路由 |
|---|---|---|
| **F-1** | **W4-a / W4-e（`obj-cep-platform.md:474`）未处置** —— 该行 = 「当前 **3 个 active**」（实测 active = **0** ⇒ G-3 的 stale 散文）**且** = 唯一超阈行（**2964** 字符）⇒ 两个缺口落在**同一行** | ⏸ **待授权**（见下）。⚠️ **实测发现第三条路**：该行属 `### 5.2 当前真实 baseline`（**living 现状节** · 自述「只承载活口径 ⇒ 归档流水应移到别处」）⇒ 其约 **30** 条 slot 流水**每条都带已存在的台账 Entry**（抽样 20 条全命中）⇒ **删去零信息损失** ⇒ 该行 2964 → 约 90 字符 ⇒ **超阈自解** ⇒ ⛔ **不需要**「扩 `KPI_LINE_MAX_FILES` / 显式接受」的二选一裁决 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次改动 | ✅ 7 份 improvement 状态行 · `AGENTS.md:423` · 登记册（§6.2 / §6.3 / §6.3.2 / §6.4 / §6.8 / §6.9）· 载体表 · 本报告 |
| 临时诊断代码 / 临时文件 | ✅ **无**（全在 `/tmp/opencode/`；仓内零残留） |
| TODO / 半截实现 / 被 skip 的测试 | ✅ 无新增 |
| 未替换的 mock / 临时 fallback | ✅ 无 |
| 范围纪律 | ✅ 零 C++ / spec / ADR 改动 · 零 `openspec/changes/**` 改动 · **零 force-add**（⛔ 不需要）· ⛔ 未动任何已归档件 |
| `AGENTS.md` 改动的影响面 | ⛔ **同行替换** ⇒ 行数不变 ⇒ P-5 活口径锚点 `AGENTS.md:313` **零位移**（实测 P-5 仍「锚定 7 处，逐处取值 == 实测」） |
| 台账 append-only | ✅ 本批**未改台账正文**（台账回执 Entry 与本报告同 commit 追加） |

## 回归记录

| 检查 | 命令 | 结果 |
|---|---|---|
| 顺序机关 | `bash scripts/test/rddf_stage_gate.sh` | ✅ rc=0 |
| 活基线闸 | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0 |
| 旧口径残留闸 | `bash scripts/test/cep_registry_stale_refs_check.sh` | ✅ rc=0 |
| 文档路径同步闸 | `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |
| 冻结面校验 | `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0 |
| R-19 旧串清零 | `grep -c 'handle_cr_transaction' AGENTS.md` | ✅ **0** |
| R-17 的 git-history 判据 | `git show 0345d283:…` / `git show 689752ef:…` | ✅ 均可达（168 / 228 行） |

## 已用轮次

**1/3**（W4 批 1 实施侧自验第 1 轮 —— 4 个子项 + 2 处记账遗漏 + 2 处原计划事实错误一次到位；无返工轮）。
