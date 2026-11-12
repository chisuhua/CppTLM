# 验收报告 —— `cpptlm-cep-invariant-gate-teeth`

> **作者**: build agent(verifier mode) · **日期**: 2026-11-12 · **轮次**: **2 / 3**
> **被验对象**: change `cpptlm-cep-invariant-gate-teeth`(已归档:`openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/`)
> **角色边界**: verifier **只验不改**;⛔ 第 1 轮的失败项由 **builder** 修(Entry #369),verifier **只复验**。
> **终态**: ✅ **通过** —— **AC-1 ~ AC-22 全 PASS(22/22)**。

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

## ② 验证顺序与结论(终态)

| 步 | 内容 | 结果 |
|:--:|---|---|
| 1 | 静态检查(lint / 格式) | ✅ `agents_md_lint.py` rc=0 · `format.sh --check` rc=0 |
| 2 | 单元 / 闸自测 | ✅ 新闸 **16/16** · 38 个 `cep_*.sh` **38/38 rc=0** |
| 3 | 集成 / 端到端 | ✅ `ctest --test-dir build -j4` ⇒ **171/171**(第 2 轮) |
| 4 | 边界与错误路径 | ✅ 7 条决定性变异**各按固定签名红** + 逐条还原即绿(影子树 ⇒ 真实仓零改动) |
| 5 | 遗留清扫 | ✅ **2 项全闭**(S-1 归档件 tasks 6 项回填;S-2 已如实登记 ⇒ 非遗留) |

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
| **AC-21** | 0 回归:ctest **171/171** + 38 闸 rc=0 + format + docs_sync | ✅ | **第 2 轮**:`ctest --test-dir build -j4` ⇒ `100% tests passed, 0 tests failed out of 171`(162.59s);`cep_doc_ref_check` 单跑 rc=0 / `100% tests passed, 0 tests failed out of 1`;38 个 `cep_*.sh` **38/38 rc=0**;`format.sh --check` rc=0;`docs_sync_check.sh --strict` rc=0;`agents_md_lint.py` rc=0 |
| **AC-22** | 数字预登记:守门表 +1 行(32→33);磁盘 36→38;`AGENTS.md` 数字不动 | ✅ | 守门表实测 **33 行**;磁盘 `cep_*.sh` = **38**;`AGENTS.md` 3 处均写「以 `ls` 为准」⇒ 无数字改动 |

**判定汇总**:✅ **22 / 22 PASS** ⇒ **验收通过**。

---

## ④ 轮次记录

### 第 1 轮 —— ❌ 不通过(21/22)

| 项 | 内容 |
|---|---|
| **FAIL-1(AC-21)** | `ctest` = **170/171** ⇒ `cep_doc_ref_check` 红:`✗ obj-cep-platform.md: 引用不存在的文件 openspec/changes/cpptlm-cep-invariant-gate-teeth/`(3 处)⇒ 台账 **Entry #367 / #368** 引用**归档前的 active 路径**(已迁至 `openspec/changes/archive/2026-11-12-...`)⇒ **闸判红正确**,⛔ 非闸缺陷 |
| **分类** | `implementation_gap`(豁免登记缺 1 条 + 归档件 tasks 回填未生效)—— ⛔ **非** `proposal_drift`(22 条 AC 中 21 条已过) |
| **路由** | **builder**(⛔ verifier 不改实现)⇒ 台账 **Entry #369** |
| **遗留 S-1** | 归档件 tasks.md 的 `A6.2` + `A11.1`~`A11.5` = **6 项**仍 `- [ ]` ⇒ 根因 = 上次回填脚本 `old` 文本写成 `**A11.1**` 而实际为 `A11.1` ⇒ **assert 中止 ⇒ 整体未生效** ⇒ 第 2 轮逐项按实际文本回填 ⇒ 实测 **0** 项未勾 |
| **遗留 S-2** | `AGENTS.md` 的「已归档 change **135** 份」⇒ 本次归档后为 **136** 份,但该数字**不在** KPI 当前态口径内 ⇒ **已在 Entry #367 ③ 显式登记**「由 T3 统一订正」⇒ **如实登记 ⇒ 非遗留** ⇒ 不路由 |

### 第 2 轮 —— ✅ 通过(22/22)

| 项 | 内容 |
|---|---|
| **FAIL-1 复验** | `bash scripts/test/cep_doc_ref_check.sh` ⇒ **rc=0**;`ctest -R cep_doc_ref_check` ⇒ `100% tests passed, 0 tests failed out of 1`;修法 = `VIRTUAL_PATHS` 追加第 4+ 条**同型**登记(`openspec/changes/cpptlm-cep-invariant-gate-teeth`)⇒ ⛔ 该子串**不匹配**归档后路径 ⇒ 归档件本身仍被逐条检查;闸的判据逻辑 / 语义断言 42 条 / 负向断言 4 条 / 自动检测 755 处**均未改** |
| **S-1 复验** | `grep -c '^- \\[ \\]' <归档件>/tasks.md` = **0** ⇒ 全勾 |
| **全量复验** | `ctest --test-dir build -j4` ⇒ **171/171**(162.59s) |
| **已用轮次** | **2 / 3** ⇒ 未触上限 |

---

## ⑤ 遗留清扫结果

| # | 对象 | 结果 |
|:--:|---|---|
| 1 | TODO / FIXME / XXX / HACK(本 change 的 5 个新产物) | **0** |
| 2 | 被 skip 的测试 / 未替换的 mock | **0** |
| 3 | 临时调试残留(`static FILE* diag` 等) | **0**(全仓 `scripts/test/` + `include/` 无命中) |
| 4 | 归档件 tasks.md 的未勾选项 | **0**(第 2 轮回填后) |
| 5 | 闸内 `print`/`echo` | **正常判据输出**(闸的存在目的),⛔ 非调试残留 |
| 6 | `AGENTS.md` 已归档 change 计数(135 → 136) | **已如实登记**(Entry #367 ③)⇒ 交 **T3** 统一订正,⛔ 非本 change 遗留 |
| 7 | **本 change 引入的已知债** | **0**(全链无 TODO 残留、无半截实现、无临时 fallback) |

---

## ⑥ 验收结论

> ✅ **通过** —— `cpptlm-cep-invariant-gate-teeth` 的 **AC-1 ~ AC-22 全 PASS(22/22)**;
> 新闸自测 **16/16**、`ctest` **171/171**、38 个 CEP 闸 **全 rc=0**、冻结面 **79 产物** 无参复验 rc=0。
> ⇒ 治理线 G5「机制有牙齿」**闭合**(R-c / R-d 两项残留由「有登记」升级为「有牙齿的机器判据」);
> **D1 槽位已释放** ⇒ **C4**(MUST-FIX-1/2)现在可立项(per §13.9 W7 行)。

**后续队列(⛔ 本验收不代做)**:**C4**(MUST-FIX-1/2)→ **T3**(Q2 残 / README 自述数字 + 三表划分说明)⇒ **T6**(推送;当前本地 **ahead 12**)。

---

## ⑦ 关联

台账 **Entry #369**(第 1 轮修复)· **#368**(归档终态)· **#367**(归档授权)· **#366**(闸侧占位路径归一)· **#365**(C3b)· **#364**(C3a)· **#363**(C3 开工授权)· **#362**(P0a)· `.rddf/improvements/cpptlm-cep-invariant-gate-teeth.md`(22 条 AC)· `openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/`(proposal / design / tasks / delta)· `openspec/specs/cep-invariant-gate/spec.md`
