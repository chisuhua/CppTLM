# 验收报告 · **G-15**（决策块覆盖义务的**射程去形态化** · 登记册 §6.2 新缺口 · 实施单元 `cep-registry-stale-cleanup`）

- **角色**：builder（**实施侧自验** —— 本单元由同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **日期**：2026-11-22 · **基线 commit**：`6d0ffcbe`（W3-b 收口 · 台账 Entry #524）
- **授权**：architect **追加裁决 §11**（`.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` · commit `302465d8` · 审查链 = oracle 1 轮 `PASS-WITH-NOTES` + critic 1 轮 `APPROVE_WITH_CONDITIONS` · **0 BLOCKER**）+ 台账 **Entry #524**（快照变更授权）

## 判据来源

| 来源 | 位置 |
|---|---|
| **G-15 的判据语义（D3 ①~⑤）** | 追加裁决 §11.1 的 **D3**（⛔ architect 只给语义，不规定实现） |
| 缺口定性 / 时限 / 归属 | 同上 **D2**（S3 治理流程债 · **须在 `ADR-CEP-21` 签发 / 归档前完成**）/ **D5**（归 `cep-registry-stale-cleanup`；写盘面不触其 AC-6 六个禁止路径） |
| 快照变更授权 | 台账 **Entry #524** ②（`authorized_by` 须指向该 Entry，⛔ 不得沿用 `Entry #431`） |
| 附带订正（D4） | 同上 **D4** —— 台账 `obj:22` / `:158` 的「21 份」→「**24 份（其中决策类 21 份）**」+ 括注枚举同步补 19/20/21 + 同名异义消歧（**已在 W3-b 收口 commit `6d0ffcbe` 落盘**） |

## 逐条判定

| # | 判据 | 实测 | 结论 |
|:--:|---|---|:--:|
| **D3 ①** | **覆盖义务谓词** = 「**有决策章**」（`## 2 …` 与 `## 决策` 两形态都算） | `check_coverage()` 的谓词复用 `section2(strip_fences(...))` ⇒ 章定位扩展后**自动覆盖两形态**（⛔ 未新写正则）· 消息措辞同步泛化为「有决策章(`## 2 …` 或 `## 决策`)」 | ✅ |
| **D3 ②** | **章定位函数 `section2()` 须同步扩展**（⛔ 只改谓词不改章定位 ⇒ **登记即红**） | `SEC2_RE` 改为 `^(?:## 2\b.*\|##\s*决策\s*)$`（**编号形态在前**）；**围栏感知的 `section2_line_range()` 同步改用共享常量 `DECISION_CH_HEADING_RE`**（⛔ 两处不同口径 ⇒ `danger_fences` 射程会与 `section2()` 错位）⇒ **变异 M2 实测**：只回退章定位 ⇒ 「§2 章缺失(快照期望 0 块)」**恒红** ⇒ 裁决的警告**成立且被验证** | ✅ |
| **D3 ③** | 快照补 `ADR-CEP-21`：**块数 = 0** ⇒ `count` **23 → 24** · `total` **不变（+0）** | `test/cep_decision_block_snapshot.json`：`files` 增 `"ADR-CEP-21-sharer-bitmap-expansion.md": 0` · `count` = **24** · `total` = **131**（**不变** ✓）· `authorized_by` = **`Entry #524`**（⛔ 未沿用 `Entry #431`）· 块数 0 的依据 = 现有 `DECISION_RE` 口径下其决策章范围内**无** `### 决策` 形态子标题（实测 `grep -c '^### 决策' docs/adr/ADR-CEP-21-*.md` == **0**） | ✅ |
| **D3 ④** | ⛔ **不改 `DECISION_RE`**（决策块的形态定义） | `DECISION_RE` **零改动**（`git diff` 该行无变化）⇒ 其余 23 份既有 ADR 的计数口径**零变化**（闸实测 24 份逐份计数 == 快照） | ✅ |
| **D3 ⑤** | ⛔ **不靠**「把 `ADR-CEP-21` 的标题改成 `## 2. 决策`」达成 | `git diff --name-only` **不含** `docs/adr/**` ⇒ **零 ADR 正文改动** | ✅ |
| **D2 时限** | 须在 `ADR-CEP-21` 签发 / 归档前完成 | `ADR-CEP-21` 状态实测 = 🟢 **Proposed**（2026-11-14 起草 · 双审通过 · **未签发**）⇒ 缺口**前瞻性**成立 ⇒ 本次在其签发前完成 | ✅ |
| **D5 归属** | 写盘面不触 AC-6 六个禁止路径 | `git diff --name-only`（本次）= `scripts/test/cep_decision_block_check.py` · `scripts/test/cep_decision_block_check.sh` · `test/cep_decision_block_snapshot.json` · 载体表 · 登记册 · 台账 ⇒ **不含** `docs/adr/` · `docs/architecture/` · `openspec/specs/` · `include/tlm/cep/` · `experimental/` · `tools/` ⇒ **AC-6 不被违反** | ✅ |

## 决定性变异（4 条 · 全部按签名红）

| # | 变异 | 实测输出 | 鉴别力 |
|:--:|---|---|:--:|
| **M1** | 影子树里把 `ADR-CEP-21` 从快照删掉 | 「❌ 决策块覆盖缺失: `ADR-CEP-21-sharer-bitmap-expansion.md`: 有决策章(`## 2 …` 或 `## 决策`)但未登记决策块快照 ⇒ 登记义务…」 | ✅ **覆盖义务对 ADR-CEP-21 真的开火**（这正是 G-15 的闭合点） |
| **M2** | 只回退**章定位**（`SEC2_RE` 不再认 `## 决策`），快照仍含 ADR-CEP-21 | 「❌ 决策块计数快照偏离: `ADR-CEP-21-…`: §2 章缺失(快照期望 0 块)」 | ✅ **裁决 D3② 的「登记即红」警告成立** ⇒ 两处必须同时改 |
| **M3** | 给 `ADR-CEP-21` 的决策章加一个 `### 决策 1` | 「❌ 决策块计数快照偏离: 期望 0 块, 实得 1 块, 差值 +1」 | ✅ 快照的「0 块」是**自钉住**的（改 ADR ⇒ 快照必须同步） |
| **M4** | 驱动壳的**变异⑥ 语义反转**（旧断言「`## 决策` 形态**不**误报」⇒ 与新口径相反） | 改后：探针 ADR（`## 决策` 形态）**必须**按 `决策块覆盖缺失` 变红 ⇒ 闸实测「✅ R-d 变异⑥(无编号决策章·开火) 按签名变红:决策块覆盖缺失」 | ✅ **变异⑥ 从「不误报」翻转为「开火」** ⇒ 两形态都被钉住 |

> ⚠️ **变异⑥ 的翻转是本次的必然连带**（裁决未逐字提及，但它是旧口径的**实现物**）：⛔ 若保留旧断言「`## 决策` 形态不触发覆盖判据」，新谓词会与该断言**直接矛盾** ⇒ 闸自测必红 ⇒ 故一并翻转，并在驱动壳头注登记该反转。

## 一处与裁决**字面**的偏差（透明登记）

| 项 | 裁决字面 | 实际落法 | 理由 |
|---|---|---|---|
| 「「0 块」是**已知限制**，须在**快照注释**里写明」 | 快照 JSON 内有注释位 | **快照 schema 升 v2 并新增必填 `notes` 键**（承载该口径说明：① 块形定义 ② `ADR-CEP-21` = 0 的成因与「⛔ 不是『计数适用于它』」③ 两形态都在射程 ④ `count` / `total` 语义） | 快照 v1 **无注释位**（固定 5 键 + `files` 值域为整数 ⇒ `total` 由求和导出 ⇒ 无法承载文本）⇒ 扩一个注释位 = **格式变更** ⇒ 按「v1 形状已变」的诚实做法**升 v2**（`SNAPSHOT_SCHEMA` 与校验器同步：v2 下 `notes` 缺失即红）⇒ 只改 schema 字符串（**仅 2 处引用**：闸脚本 + 快照文件本身） |

## 回归记录

| 检查 | 命令 | 结果 |
|---|---|---|
| 决策块判据闸 | `bash scripts/test/cep_decision_block_check.sh` | ✅ rc=0（**20 项通过 / 0 项失败** · 含基线 + 10 条决定性变异） |
| ADR 结构闸 | `bash scripts/test/cep_adr_structural_check.sh` | ✅ rc=0 |
| 文档同步闸 | `bash scripts/test/cep_docs_sync_check.sh` | ✅ rc=0 |
| 顺序机关 | `bash scripts/test/rddf_stage_gate.sh` | ✅ rc=0 |
| 活基线闸（W2-a） | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0 |
| 旧口径残留闸（W2-b） | `bash scripts/test/cep_registry_stale_refs_check.sh` | ✅ rc=0 |
| 冻结面校验 | `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0（零冻结面成员改动） |
| 文档路径同步闸 | `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次改动 | ✅ 2 个闸脚本（判据 + 驱动壳）+ 快照（schema v2 / 补录 / `authorized_by`）+ 载体表 + 登记册 + 台账 + 本报告 |
| 临时诊断代码 / 临时文件 | ✅ **无**（变异全在 `/tmp/opencode/` 影子内；仓内零残留） |
| TODO / 半截实现 / 被 skip 的测试 | ✅ 无新增 |
| 未替换的 mock / 临时 fallback | ✅ 无 |
| 范围纪律 | ✅ **零 ADR 正文改动** · 零 `docs/architecture/` · 零 `openspec/specs/` · 零 C++/test 改动 ⇒ `cep-registry-stale-cleanup` 的 AC-6 未被违反 |
| 判据语义边界 | ✅ `DECISION_RE` **零改动**（D3④）⇒ 其余 23 份既有 ADR 的计数口径零变化 |
| 载体表 `state` 纪律 | ✅ `G-15` 行与本报告**同 commit** 落 `done` |
| 遗留缺口（不在本单元范围） | ⚠️ **G-16**（`ADR_LIST` 缺反向完整性检查）已登记 ⇒ **未处置**（per 裁决 D7「本裁决不处置」） |
| 历史件里的旧 schema 字符串 | ⚠️ `openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/design.md:115` 仍写 `"schema": "cep-decision-block-snapshot-v1"` ⇒ **归档 change 的历史快照**，⛔ **不改**（归档件不可变；且 `cep_doc_ref_check.sh` 的排除集含 `openspec/changes/archive/**/*.md` ⇒ 不被任何闸扫描） |

## 已用轮次

**1/3**（G-15 实施侧自验第 1 轮 —— D3 ①~⑤ 一次到位 + 4 条变异全按签名红；无返工轮）。
