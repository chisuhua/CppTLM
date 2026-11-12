# 验收报告 —— `cpptlm-cep-invariant-gate-teeth`

> **作者**: build agent(verifier mode) · **日期**: 2026-11-12
> **被验对象**: change `cpptlm-cep-invariant-gate-teeth`(已归档:`openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/`)
> **角色边界**: verifier **只验不改**;⛔ 本报告不修改任何实现。

---

## ① 判据来源(按优先级)

| 序 | 来源 | 内容 |
|:--:|---|---|
| 1 | **用户本次明确给出的验收条件** | 「审查配对:builder 的产出由 verifier 验;不要自己宣布通过」+ C3 链任务书(P0a → P0b → C3a → C3b → C3-F) |
| 2 | `openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/proposal.md` 的 `## 验收标准(Acceptance Criteria)` 段 | **AC-1 ~ AC-22** |
| 3 | 本次变更自述 | `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md` 的 22 条 AC + `design.md` D-1 ~ D-11 + `specs/cep-invariant-gate/spec.md` 的 7 个 Scenario |
| 4 | ADR 的 Verification 章 | ⛔ 本 change **不新建 ADR**;`ADR-CEP-00` 的 SU 2026-11-12 为**取代声明 + 换算表 + 重述块**,⛔ 无独立 Verification 段 ⇒ 不作判据来源 |

> **分流判定**:`rdd-verifier` skill 的 canonical 前置条件 **3 条均不满足**(① 无 active change 目录(已归档)· ② `.rddf/state/iteration.json` 不存在 ⇒ P3 archive gate 未触发 · ③ AC 段标题形态 `## 验收标准(Acceptance Criteria)` 不符 skill 期望的整行形态)⇒ **走本地 1–5 步验收**,判据取自上表。

---

## ② 验证顺序与结论

| 步 | 内容 | 结果 |
|:--:|---|---|
| 1 | 静态检查(lint / 格式) | ✅ `agents_md_lint.py` rc=0 · `format.sh --check` rc=0 |
| 2 | 单元 / 闸自测 | ✅ 新闸 **16/16** · 38 个 `cep_*.sh` **38/38 rc=0** |
| 3 | 集成 / 端到端 | ❌ `ctest --test-dir build -j4` ⇒ **170/171**(`cep_doc_ref_check` 红)⇒ **FAIL** |
| 4 | 边界与错误路径 | ✅ 7 条决定性变异**各按固定签名红** + 逐条还原即绿(影子树 ⇒ 真实仓零改动) |
| 5 | 遗留清扫 | ❌ 2 项未闭(见 ⑤)⇒ **随 AC-21 一并路由** |

---

## ③ 逐条判定(AC-1 ~ AC-22)

| AC | 判据 | 判定 | 证据(工具取证) |
|:--:|---|:--:|---|
| **AC-1** | 基线正控 rc=0(变异前) | ✅ | `bash scripts/test/cep_decision_block_check.sh` ⇒ `══ ① 基线(无变异) ══ ✅ 基线通过` |
| **AC-2** | `ADR-CEP-00` 按 D-1 四步计数 = **4** | ✅ | 独立复算脚本(不走闸):`ADR-CEP-00 计数 = 4 (期望 4)`;`awk '/^## Status Update 2026-11-12/,0' ADR-CEP-00 \| grep -c '^###[[:space:]]*决策'` = **0** ⇒ 新 SU 未新增决策块 |
| **AC-3** | 22 份**逐份** == 快照,`total` = **125** | ✅ | 独立复算:`逐份比对: 22 份, 相等 22 份, 不等 0 份`;`实测合计 = 125 \| 快照 total = 125 \| 相等 = True`;快照 `authorized_by = Entry #363` 且台账存在该 Entry = True |
| **AC-4** | 变异① 围栏含行首 `## ` ⇒ 签名 `围栏内含行首 ##` | ✅ | 闸自测:`✅ R-d 变异① 按签名变红:围栏内含行首 ##` |
| **AC-5** | 变异② 非 `###` 级 ⇒ 签名 `决策块标题非 ### 级` | ✅ | `✅ R-d 变异② 按签名变红:决策块标题非 ### 级` |
| **AC-6** | 变异③ 非 ASCII 十进制 ⇒ 签名 `决策块编号非法` | ✅ | `✅ R-d 变异③ 按签名变红:决策块编号非法` |
| **AC-7** | 变异④ 删决策段 ⇒ 签名 `决策块计数快照偏离` | ✅ | `✅ R-d 变异④ 按签名变红:决策块计数快照偏离` |
| **AC-8** | R-d 4 条变异逐一还原 ⇒ 绿 | ✅ | 闸自测 4 条 `✅ R-d 变异N 还原后绿` |
| **AC-9** | R-c 正向:三份存在 + 无同号拆出件 | ✅ | 闸基线绿 + 变异② 反证(造 `ADR-CEP-02a-*` ⇒ 红)⇒ 判据有鉴别力 |
| **AC-10** | R-c 反向:`### ⑥` 块恰 3 bullet + 三 token 各 ≥1 | ✅ | 闸基线绿 + 变异① 反证(删一行 ⇒ `重述块缺项`) |
| **AC-11** | 触发条件表:恰 3 行 + 4 类引用可解析 | ✅ | 闸基线绿 + 变异③ 反证(清空「判定方式」⇒ `引用解析缺失`) |
| **AC-12** | 变异① 删重述块一行 ⇒ `重述块缺项` | ✅ | `✅ R-c 变异① 按签名变红:重述块缺项` |
| **AC-13** | 变异② 同号拆出件 ⇒ 新闸红 **且** 结构闸① 红(**双闸各报各的签名**) | ✅ | `✅ R-c 变异② 按签名变红:存在同号拆出件` + `✅ R-c 变异② 结构闸按签名变红:未归类的 ADR 文件`(**两条闸、两个不同签名**) |
| **AC-14** | 变异③ 判定方式清空 ⇒ `引用解析缺失` | ✅ | `✅ R-c 变异③ 按签名变红:引用解析缺失` |
| **AC-15** | R-c 3 条变异逐一还原 ⇒ 绿 | ✅ | 闸自测 3 条 `✅ R-c 变异N 还原后绿` |
| **AC-16** | `ADR-CEP-00` = **4**;22 份合计 = **125**(不扰动 R-d) | ✅ | 同 AC-2 / AC-3 的独立复算 |
| **AC-17** | ctest 见新闸名 + 用例数 **170 → 171** | ✅ | `ctest -N` ⇒ `Total Tests: 171`;`grep -c cep_decision_block_check` = **1**(#92) |
| **AC-18** | `07 号` SU **插在索引之前** + 索引重生成 + 索引闸 rc=0 + **29/16** | ✅ | `grep -n` ⇒ SU 在 **:569**、索引在 **:600** ⇒ SU 在前;`cep_index_position_check.sh` ⇒ `索引行 600 · 一级节 A=29/自述29/实际29 · SU B=16/自述16/实际16 · 问题 0`,rc=**0** |
| **AC-19** | 五步链:同一次 `--update` + 精确 `authorized_by` + 无参复验 rc=0 | ✅ | manifest `authorized_by = Entry #368` 且台账存在该 Entry = **True**;`cep_freeze_check.sh` 无参 ⇒ `✓ M1 产物冻结校验通过:79 个产物 hash 全部一致` + `✓ provenance 自检通过`,rc=**0** |
| **AC-20** | 归档 + **无条件**第二次 `--update` ⇒ `artifact_count` **78 → 79** | ✅ | manifest `artifact_count = 79`;`openspec list` ⇒ `No active changes found.`;归档件 + `openspec/specs/cep-invariant-gate/spec.md` 均在;`openspec validate cep-invariant-gate --type spec --strict` ⇒ **rc=0** |
| **AC-21** | 0 回归:ctest **171/171** + 38 闸 rc=0 + format + docs_sync | ❌ | **ctest 实测 170/171** ⇒ `cep_doc_ref_check` Failed(38 闸 / format / docs_sync / lint 均 ✅)。⇒ **FAIL** |
| **AC-22** | 数字预登记:守门表 +1 行(32→33);磁盘 36→38;`AGENTS.md` 数字不动 | ✅ | 守门表实测 **33 行**;磁盘 `cep_*.sh` = **38**;`AGENTS.md` 3 处均写「以 `ls` 为准」⇒ 无数字改动 |

**判定汇总**:✅ **21 / 22**;❌ **1**(AC-21)⇒ **不通过**(verifier 第 1 轮)。

---

## ④ 失败项与路由建议

### ❌ FAIL-1(AC-21)—— `implementation_gap`

| 项 | 内容 |
|---|---|
| **现象** | `ctest --test-dir build -j4` ⇒ **170/171**;失败用例 = **`cep_doc_ref_check`**(#145) |
| **红因(实测)** | `✗ obj-cep-platform.md: 引用不存在的文件 openspec/changes/cpptlm-cep-invariant-gate-teeth/`(3 处:目录 / `design.md` / `specs/cep-invariant-gate/spec.md`) |
| **归因** | 台账 **Entry #367 / #368**(本 change 自己的归档登记)引用了**归档前的 active 路径**;该目录在归档时已迁至 `openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/` ⇒ 引用指向**已不存在的路径** ⇒ **闸判红是正确的** |
| **⛔ 非闸缺陷** | 闸的判据本身无误(路径确实不存在);同型先例 = `cep_doc_ref_check.sh` 的 VIRTUAL_PATHS 已有 **3 条**同构登记(`cpptlm-cep-stub-cache-core` / `cpptlm-cep-poc-spike` / `cpptlm-cep-cxl-nonsymmetric-state-spec` —— 均为「change 归档后其 active 路径仍被 append-only 台账引述」)⇒ **本次是第 4 条同型**,机制已存在,只是**未登记本 change 的那一条** |
| **建议修法** | 在 `scripts/test/cep_doc_ref_check.sh` 的 `VIRTUAL_PATHS` 追加 1 条(`openspec/changes/cpptlm-cep-invariant-gate-teeth`),注释写明「**归档后 active 路径已迁至 `openspec/changes/archive/2026-11-12-...`**」⇒ 与既有 3 条先例**同型同机制**;⛔ **不改闸的判据逻辑**,⛔ **不改台账已提交 Entry**(append-only) |
| **复现命令** | `bash scripts/test/cep_doc_ref_check.sh` ⇒ 修前 rc=1(3 处)⇒ 修后期望 rc=0;`ctest --test-dir build -R cep_doc_ref_check` ⇒ 期望 Passed |
| **路由** | **builder**(implementation_gap)—— ⛔ verifier 不改 |

### 遗留清扫 2 项(随 FAIL-1 一并路由)

| # | 发现 | 事实 | 建议 |
|:--:|---|---|---|
| **S-1** | **归档件 tasks.md 的 6 项未勾选** | `A6.2`(commit 2 件冻结件)+ `A11.1` ~ `A11.5` = **6 项**仍为 `- [ ]` ⇒ 事实已完成(commit `f0faa178` / 归档已完成 / 第二次 `--update` 已跑 / manifest 已 commit / validate 已过),**载体未回填** | 回填 6 项勾选 + 附实测回执(⚠️ 上次回填脚本因 `**A11.1**` 与实际文本 `A11.1` 不匹配而 **assert 中止 ⇒ 整体未生效** ⇒ 本次须逐项核对文本后再写) |
| **S-2** | 台账 `#367` 的 KPI 回填第 7 项(已归档 change 计数) | `AGENTS.md` 的「已归档 change **135** 份」在本次归档后变为 **136** 份,但该数字**不在** KPI 当前态口径内(闸不抓)⇒ 已在 Entry #367 ③ 显式登记「由 **T3** 统一订正」 | ⛔ 本 change **不改**(已登记;避免与 T3 重复改同一行) |

> **S-2 已如实登记** ⇒ 非遗留 ⇒ 不路由。
> **S-1 + FAIL-1** ⇒ **路由回 builder**,修完由 verifier 复验(第 2 轮)。

---

## ⑤ 已用轮次与后续

| 项 | 值 |
|---|---|
| **已用轮次** | **1 / 3**(上限 3,per `RDDF_VERIFIER_MAX_LOOPS=3`) |
| **判定** | **不通过**(AC-21 FAIL) |
| **失败分类** | `implementation_gap`(闸的豁免登记缺 1 条 + 归档件 tasks 回填未生效)—— ⛔ **不是** `proposal_drift`(判据与实现一致,22 条 AC 中 21 条已过) |
| **下一步** | 回 **builder**:① 登记 VIRTUAL_PATHS 第 4 条同型豁免 ② 回填归档件 tasks 的 6 项 ③ commit ⇒ 切 **verifier** 第 2 轮复验 `cep_doc_ref_check` + `ctest` |

---

## ⑥ 关联

台账 **Entry #368**(归档终态)· **#367**(归档授权)· **#366**(闸侧占位路径归一)· **#365**(C3b)· **#364**(C3a)· **#363**(C3 开工授权)· `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md`(22 条 AC)· `openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/`(proposal / design / tasks / delta)
