# Entry 请求 · 台账 Entry #537（CEP 架构文档对齐批次）

> **本文件是「请求」不是台账**。台账唯一写入方 = **rdd-planner**（`.rddf/roadmap/objectives/obj-cep-platform.md` · append-only）。
> **本回合零台账写入**；architect 不代写台账。
> 请求日期：2026-11-23 · 请求方：architect · 台账最新 Entry = **#536** ⇒ 本次应为 **#537**
> **配套逐字执行稿**：[`cep-arch-alignment-batch-b-2026-11-23.md`](./cep-arch-alignment-batch-b-2026-11-23.md)（355 行 · 11 节）—— **落盘时按该稿逐字执行**，⛔ 不临场改写。

---

## 1. 请求事项（一句话）

授权 architect 对 **4 个 M1 冻结面文件**做 **9 项**文档层订正（**零 ADR 正文改动 · 零代码改动 · 零契约面改动**），并走「台账 Entry → 冻结清单 `--update` → 清单单独 commit → 无参复验」五步链。

**背景**（审查结论摘要）：以 `docs/adr/CEP-DECISION-DIGEST.md` 为基准审查 `docs/architecture/` 的 CEP 文档 ⇒ **24 份 ADR 中 21 份有正文落点**；但存在 **3 项结构性缺陷 + 7 项文档漂移 + 2 项正确性问题**。其中 **1 项已在本批之前完成**（手册 `docs/guide/cep-usage.md` §12.9 订正，已提交 `202b7973`，⛔ 不在冻结面 ⇒ 无需本 Entry）。

---

## 2. 授权范围（逐文件 · 逐项）

| # | 文件 | 改动 | 位移约束 | 性质 |
|:--:|---|---|:--:|---|
| 1 | `docs/architecture/07-coherence-exploration-platform.md` | **新增「附录 A · 证据档位 × 6 层架构 归属映射」**（`## ` 级 · 追加在最后一条 SU 之后、文末索引之前） | 追加（活锚点最高 `:743` ⇒ 安全） | 新增正文节 |
| 2 | 同上 | **新增 1 则 SU**：档 0 空洞性的**双根因**补全（装置语义面 + 拓扑分档） | 追加 | 追加 SU（append-only） |
| 3 | 同上 | **§8.3 表等长替换**（19 行 → 19 行）：合并 5 组重复 ID · 修 10 行「4 格 vs 表头 3 列」· 补 `CEP-19`/`CEP-20`/`CEP-21` · 加 2 行注记 | **零位移** | 正文就地订正 |
| 4 | 同上 | **§9「Primary ADRs」表追加 3 行**（`19`/`20`/`21`） | 追加 | 正文就地订正 |
| 5 | 同上 | **计数订正 3 处**（`:10` / `:113` / `:434`：「10 份 ADR」→「24 份」） | 等行数替换 | 正文就地订正 |
| 6 | 同上 | **新增 1 行订正**（附录 SU 区）：`06-bis` **不在** M1 冻结集 ⇒ 上一则 SU 的「冻结面」标签不准确（结论不变：superseded 历史快照不改） | 追加 | 追加 SU |
| 7 | `docs/architecture/cep/fabric-transport.md` | **文件头 1 行订正**：删除与同文件 §6.3.1 + 手册 §12.2 自相矛盾的「本层无 CEP 自有运行时实现」（mesh 载体实测全在） | **零位移**（单行等长替换） | 正文就地订正 |
| 8 | `docs/architecture/cep/parity-baseline.md` | **§4.7 产物路径订正 2 处** + **§8 表编号修正**（重复 `11` + 错位 `12`）+ **「档位」→「判据档」术语消歧 4 处** + **§3.1 补可达性限定注记 1 处** | **零位移** | 正文就地订正 |
| 9 | `docs/architecture/cep/parity-baseline.md` | **§8 表按编号重排行序**（1..18 升序） | **零位移**（表在文末 · 外部无行号锚点，实测 `grep "parity-baseline\.md:[0-9]"` = 空） | 正文就地订正 |

---

## 3. ⛔ 本 Entry **不**授权的事（范围隔离）

| ⛔ 不做 | 理由 / 归属 |
|---|---|
| 改任何 `docs/adr/ADR-CEP-*.md` 正文 | 真源 3 不可变；本批零 ADR 改动 |
| 改 `openspec/specs/**`（含 tier2-observation spec 的陈旧 Purpose、`tier2_applicable` 入契约面） | **契约面 = planner 域**；architect 不写 |
| 写 `scripts/test/**`（T10 机判实现） | **实施面 = builder 域**；本批只给**判据规格** |
| 新建第三个登记面 / 扩 `ic3_verdict` 词表 | `ADR-CEP-20` 决策 6 禁令 |
| 让档 0 承载一致性语义 / 跨档比较或聚合 | `ADR-CEP-20` 决策 1 / 6(i) |
| 「拓扑 B 产档 0 run」（甲′） | oracle 已否决原表述；改写版未起草 ⇒ 建议先做 B′ 负控见证（`ADR-CEP-20:71` backlog） |
| 改 `docs/architecture/06-bis-*.md` / `06-inter-module-network.md` | superseded / 框架面，本批不涉及 |
| 改 `ADR-CEP-21`（阶段 1 机制化 / `kResponderMappedBits` 现值 4） | 该 ADR = 🟢 **Proposed**（未签发）⇒ 阶段 1「成立即生效」尚未生效 |

⛔ **E2E 修订甲′**（`ADR-CEP-20` SU 五节 / `07 号` 指针 / `spec` MODIFIES）**不在本批** ⇒ 需**独立** Entry（ADR SU append + spec delta 两步授权链）。

---

## 4. 落盘后必须满足的验收判据（可机判）

| # | 判据 | 命令 / 断言 |
|:--:|---|---|
| 1 | 冻结面 hash 与 manifest 一致（新值） | `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #537"` 后**无参复验** rc=0 |
| 2 | **活锚点零位移** | `07 号` 的 `:138` / `:316` / `:590` / `:582-600` / `:696` / `:743` **内容不变** ⇒ 落盘前后逐行比对 |
| 3 | 索引一致 | `python3 scripts/cep/generate_doc_index.py --write` 后 `bash scripts/test/cep_index_position_check.sh` rc=0（自述 **37** 一级节 / **23** 则 SU） |
| 4 | ADR 索引未腐化 | `bash scripts/test/docs_adr_index_check.sh` rc=0 · `python3 scripts/docs/gen_adr_index.py --check` rc=0 |
| 5 | 文档闸全绿 | `cep_doc_ref_check.sh` · `cep_docs_sync_check.sh --strict` · `docs_sync_check.sh --strict` · `cep_e2e_reconcile_check.sh` · `cep_dod_reading_check.sh` · `cep_dod_reading_check.py` 全部 rc=0 |
| 6 | ADR 结构 / 决策块未动 | `cep_adr_structural_check.py` · `cep_decision_block_check.py` rc=0（**决策块快照 `count=24` / `total=131` 不得变**） |
| 7 | 表格列数 | `07 号` §8.3 表全部数据行 **3 格**；§9 表全部行 **2 格** |
| 8 | ⛔ 禁词零命中 | 附录 A 内不得出现「强于 / 不弱于 / 优于 / 等同 / 并列引用 / 跨档比较 / 跨档聚合 / 合计 / 总判定」；手册 §12.9 不出现「检测力已证」 |

---

## 5. ⚠️ 四个必知的坑（planner / 执行者）

### 坑 1 · 台账 Entry **不可逐个列 invariant id**（Entry #536 刚踩过）

`Entry #536` 记录了：**invariant 台账锚闸按「本批锚 = 编号最大的、同时含本批 id 的 Entry」定位** ⇒ 任何后续 Entry 只要提及那些 id，就**劫持本批锚** ⇒ 闸红。
⇒ **本 Entry 正文请用批次级陈述**（如「4 个文件的文档层订正」），⛔ **不要**逐个罗列 `INV-CEP-*` / `CEN-Ib*` id。

### 坑 2 · `git commit` 必须**单独成段**

本仓 `commit-gate` 判据：命令串含 commit 段 **且** 出现**引号外**的 `;` / `&&` / `||` / `|` / `&` / 换行 ⇒ **整串拒绝**。
⇒ **必须用 shell 工具的 `workdir` 参数**，⛔ **不要**内联 `cd <dir> && git commit …`（那会被判为复合命令）；`git add` 与 `git commit` **分两次调用**。

### 坑 3 · SU 是 append-only，⛔ 不回改

`07 号` 的既有 SU（23 则）**逐字保留**；任何订正都以**新 SU** 表达（本次 = 第 24 则 + 第 25 则）。
⇒ `07:590`（「23 份 ADR … 23 键」已陈旧）**⛔ 不可编辑**，由新 SU 顺带订正。

### 坑 4 · token 码位

台账 / 文档 token：**`⏭` = U+23ED**（降级 / 授权）· **`⛔` = U+26D4**（禁止）· **⛔ 不是 U+2B55**（`⭔` 是另一个字符，部分读取工具会渲染成它）。落盘前逐字节核对。

---

## 6. 给 planner 的 Entry 正文骨架（**由 planner 落笔**，⛔ architect 不代写）

```markdown
### Entry #537 — 2026-11-23 [planner](**架构文档对齐批次授权** —— CEP 架构文档 ↔ ADR 决策面的 9 项订正)

**① 授权范围**(architect 起草 · 逐字执行稿 = `.rddf/reviews/cep-arch-alignment-batch-b-2026-11-23.md`)：
- **4 个 M1 冻结面文件**的 **9 项文档层订正** —— ① `07 号` 新增「附录 A · 证据档位 × 6 层架构 归属映射」② `07 号` 新增 1 则 SU(档 0 空洞性双根因)③ `07 号` §8.3 表等长替换(19→19 行)④ `07 号` §9 表追加 3 行 ⑤ `07 号` 计数订正 3 处 ⑥ `07 号` 新增 1 行标签订正 ⑦ `fabric-transport.md` 文件头 1 行 ⑧ `parity-baseline.md` §4.7 路径 2 处 + §8 编号 + 术语消歧 4 处 + §3.1 注记 1 处 ⑨ `parity-baseline.md` §8 表重排行序。

**② 硬约束**：活锚点(`07 号` 的 `:138` / `:316` / `:590` / `:582-600` / `:696` / `:743`)**零位移**;零 ADR 正文改动;零代码改动;零契约面改动;⛔ 既有 SU 逐字保留;⛔ 不引入跨档比较或聚合措辞。

**③ 执行链**：architect 按执行稿逐项编辑 → `python3 scripts/cep/generate_doc_index.py --write` → `cep_freeze_check.sh --update --authorized-by "Entry #537"` → **清单单独 commit** → 无参复验。

**④ 验收**：见本请求稿 §4 的 8 条可机判判据。

**⑤ 范围外**(⛔ 本 Entry 不授权)：ADR 正文 / openspec specs / scripts 实现 / 第三个登记面 / `ic3_verdict` 词表 / 档 0 承载语义 / 跨档比较 / 拓扑 B 产 run / E2E 修订甲′(另立)。

**⑥ 关联**：`.rddf/roadmap/objectives/obj-cep-platform-remaining.md` §6 · `ADR-CEP-20` 决策 6 · `docs/adr/CEP-DECISION-DIGEST.md` · `docs/guide/cep-usage.md` §12.9~§12.10 · `.rddf/reviews/cep-arch-alignment-batch-b-2026-11-23.md`。
```

---

## 7. 回滚

- 冻结面改动若在 commit 前出错 ⇒ `git checkout -- <4 个文件>` 即可（⛔ 不涉及台账与冻结清单）；
- 冻结清单 `--update` 后若需回退 ⇒ 用**上一个** `authorized_by` 重新 `--update`（`cep_freeze_check.sh` 记录 `authorized_by` + `frozen_at_commit` ⇒ 可寻址）；
- 台账 Entry **append-only** ⇒ ⛔ 不回改；回滚走**新 Entry**（per 台账既有形态）。

---

## 8. 本请求的待确认项

| # | 项 | 状态 |
|:--:|---|---|
| 1 | Entry 号 = **#537** | 需 planner 复核（台账最新 = #536） |
| 2 | 执行稿落点 = `.rddf/reviews/cep-arch-alignment-batch-b-2026-11-23.md` | ✅ 已落盘（architect 写集内 · ⛔ 非冻结面） |
| 3 | 审稿报告 | 本批**未走 oracle→critic 双审** —— 理由：产物是**文档层订正**（非 ADR / 非架构决策），判据 = 「是否与既有 ADR 决策一致 + 是否零位移」⇒ 已由本请求 §4 的 8 条可机判判据覆盖；⛔ 若 planner 认为需双审，本批**暂缓落盘** |
| 4 | 方向甲（拓扑 B 产 run） | oracle 已否决原表述；改写版（甲′）**未起草** ⇒ 建议先做 B′ 负控见证 |
