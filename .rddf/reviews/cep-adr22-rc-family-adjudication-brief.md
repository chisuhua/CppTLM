# 裁决简报 · `ADR-CEP-22` 决策 3 的 RC 家族 `po` 授权表定性（NG1 + NG2）

> **交付对象**：**architect**（判权域）· 需**用户 ratify**方可落盘
> **提出方**：build agent（2026-11-25 复审）· **证据全部仓内可机检**（`scripts/test/cep_oswald_provenance_check.sh` 全绿 · 21/21 锚点命中）
> **性质**：**纯裁决输入** —— 本简报**不代改**已签批 ADR（`ADR-CEP-22` 正文不可变；修订只能走追加 `## Status Update` 段 + 冻结面五步链）
> **关联**：gap 总账 `.rddf/roadmap/cep-gen-family-gap-closure.md`（NG1 / NG2）· 论文侧锚点面 `docs/research/oswald/`

---

## 1. 一句话结论

> **ADR-CEP-22 决策 3 的三条 RC `po` 授权表（`RC-acquire` / `RC-release` / `RC-data`）在 *Gen 族* 论文中找不到形式化对应**：
> 论文只给出了 **SC** 与 **x86-TSO** 两个 `ppo` 关系（`T-C13` / `T-C14`），RC 在论文里**只被语义刻画**（`T-C12`）；
> 而该 ADR 所引锚点 `HeteroGen_HPCA2022-draft.md:1266-1276` 实为 **Table I「case study 协议清单表」**（`H-C01` / `H-C02`），**不含任何序语义**。
> ⇒ **锚点与承重件错配**，且该表是当前 6 条操作级迹中 **5 条跨域迹的判定承重件** ⇒ 属**必须裁定**项，非文档洁癖。

---

## 2. 事实基线（逐条可复核）

| # | 事实 | 锚点 | 复核方式 |
|---|---|---|---|
| F1 | 论文 §8.5.2 给出 `ppo` 形式化的**只有两个**：`SC: ppo ≜ po` · `x86-TSO: ppo ≜ po \ (st(x);po;ld(y))` | `T-C13` / `T-C14` | `extracts/thesis-text-extract.md:7098-7110` |
| F2 | RC 在论文里是**语义**刻画（release/acquire 操作），**从未给出 RC 的 `ppo` 关系** | `T-C12` | 同文件 `:6887` |
| F3 | ADR-CEP-22 决策 3 的 RC 三表标注「⚠️ RC 语义 · 锚点 = `ADR-CEP-12` 决策 3 … ⇒ **仓内不可复核**」 | `docs/adr/ADR-CEP-22-…md` 决策 3 | 直接读 |
| F4 | ADR-CEP-22 决策 3 自定硬纪律：「⛔ **无锚点 ⇒ 不登记**（该标签的域一律 `undecided`）⇒ **不得编值**」 | 同上 | 直接读 |
| F5 | ADR-CEP-22 决策 3 所引锚点 = **Table I**（七个**同构**协议清单：SC→MSI/MESI；TSO→TSO-CC；RC→RCC-O/RCC/GPU；PLO→PLO-CC） | `H-C01` / `H-C02` | `extracts/HeteroGen_HPCA2022-draft.md:1243` / `:1275-1276` |
| F6 | 该抽取件全文 grep `ppo` / `preserved program order` = **0 命中** | — | `grep -c` |
| F7 | **对照**：ADR-CEP-22 的 **PLO 行是对的**且有论文字面支撑 —— 论文原文「PLO-CC … enforce[s] … partial-load-order that **enforces the W→W and the R→W** orderings **but not the other two**」⇒ PLO 写侧 {R→W, W→W} + 读侧 ∅ **逐字吻合** | — | `extracts/HeteroGen_HPCA2022-draft.md:1253-1256` |
| F8 | RC 的 **acquire/release 语义**（词面）在仓内可查：「Acquire = 后续内存引用必须能看到此前所有释放操作」·「Release = 此前的所有内存引用必须在释放前完成」 | — | mynotes 仓 `01_Wiki/concepts/memory-consistency-protocols.md:70-79`（**跨仓**） |

---

## 3. 问题陈述：三重错位

| 错位 | 内容 |
|---|---|
| **① 锚点错配** | 决策 3 的 RC 三表被溯源到 Table I（协议清单表）⇒ 锚点**不支撑**所锚的**序关系断言** |
| **② 纪律张力** | 决策 3 自定「无锚点 ⇒ 不登记」，但 RC 家族既无仓内可复核锚点、又无论文 `ppo` 形式化，却**已登记并被采纳为规则** ⇒ 该纪律要么为 RC 家族开**显式例外**，要么 RC 家族应降为 `undecided` |
| **③ 表述不精确** | 「仓内不可复核」的真实含义是「**论文未给出可援引的形式化**」⇒ 二者对读者的行动含义不同（前者=去找；后者=**本仓自创，须自行负责**） |

---

## 4. 影响面量化（为何必须裁定）

**`moesi_gpu`（`read=RC-data` / `write=RC-release`）绑定了当前 6 条操作级迹中的 5 条**：

| 迹 | family | 域绑定 |
|---|---|---|
| `xdomain-sb` | hetero-sb | `cpuA→chi_rnf` · `gpuB→moesi_gpu` |
| `xdomain-sb-companion` | hetero-sb | 同上 |
| `co-two-candidates` | hetero-co | 同上 |
| `rf-cross-domain` | rf-class | 同上 |
| `co-single` | hetero-co | 同上 |
| `rf-intra-domain` | rf-class | 仅 `chi_rnf`（SC ⇒ 不受影响） |

⇒ **`ppo_i(moesi_gpu)` = {R→W, W→W}**（`Allow_read(RC-data)=∅` ∪ `Allow_write(RC-release)={R→W,W→W}`）**直接决定**跨域环是否成形。
⇒ **若走选项 (b)（RC → `undecided`）**：这 5 条迹全部落 `undecided` + `model-semantics-unavailable`
⇒ 闸 `cep_compound_graph_falsification_check.sh` 臂①（金标准违例迹 ⇒ `falsified` + 见证）**必然失败**
⇒ **G2 的现有实现需整体重做**。**故此裁定不可回避。**

---

## 5. 三个选项

| 选项 | 内容 | 结论 | 理由 |
|:--:|---|---|---|
| **A. 保留现状** | 维持「仓内不可复核」标注，锚点仍指 Table I | ❌ **不采纳** | 锚点**明确不支撑**承重件；且纪律张力未解 ⇒ 留下一条**已知的错误指针** |
| **B. RC 家族降为 `undecided`** | 与 `TSO` 同等待遇（0 实例 + 无规则 ⇒ `undecided` + `model-semantics-unavailable`） | 🟡 **可行但代价大** | 语义最保守、零编值风险；但按 §4 ⇒ **G2 现有实现整体重做**（5/6 迹作废） |
| **C. 重新锚定 + 显式定性**（**推荐**） | ① 锚点从 Table I 改为「**RC 的 acquire/release 语义**」（论文对 RC 的**唯一**刻画层级，论文 §II-B）+ 仓内可查的 RC 语义表述；② 措辞从「仓内不可复核」改为「**论文未给出 RC 的 `ppo` 形式化；本表是本仓依 RC acquire/release 语义的解释性建模**」；③ 为决策 3 的「无锚点 ⇒ 不登记」纪律补一条**显式例外条款**（例外对象 = 「论文未给出形式化、但存在公认语义刻画」的标签），并把该例外**登记为口径**（⛔ 不新造 invariant 编号） | ✅ **推荐** | ① 语义**不需重做**（表本身合理且与 RC 标准刻画吻合）；② 消除错误指针与纪律张力；③ 把「本仓解释性建模」这一事实**显式化**，读者可据其复核与追责 |

### 5.1 选项 C 的合理性论证（供核验）

| RC 家族表 | 本仓取值 | 与 RC 的 acquire/release 语义是否自洽 | 论文/文献支撑 |
|---|---|---|---|
| `Allow_read(RC-acquire)` = {a 是读} = R→R ∪ R→W | acquire **定序其后**的操作 | ✅ 一致（F8：「后续内存引用必须能看到此前所有释放操作」⇒ acquire 之后的读/写被其定序） | 论文**只给语义**（F2） |
| `Allow_write(RC-release)` = {b 是写} = R→W ∪ W→W | release **定序其前**的操作 | ✅ 一致（F8：「此前的所有内存引用必须在释放前完成」） | 同上 |
| `Allow_read(RC-data)` = ∅ | relaxed 访问**不保留任何**程序序 | ✅ 一致（RC 的 relaxed 级不保序）；⚠️ 决策 3 已自陈「⛔ 不得把 `po_loc` 塞进来 ⇒ 那是**过严** ⇒ 会把 RC 合法迹误判为 `falsified`（假阳性）」 | 同上 |
| **合取自洽性**（ADR 决策 3 自检段） | `mesif`（RC-acquire/RC-release）⇒ {R→R, R→W, W→W}（**缺 W→R**） | ✅ 与 RC 的经典形态一致（RC 允许 W→R 重排） | — |
| `moesi_gpu`（RC-data/RC-release）⇒ {R→W, W→W} | ✅ 自洽 | — | — |

⇒ **表本身是合理的解释性建模**；问题**不在表**，而在**锚点与措辞**。⇒ 选项 C 是「修指针 + 显式定性」，**不是「改语义」**。

---

## 6. 拟用 `## Status Update` 段文本草案（供 architect 直接采纳或改写）

> **拟追加位置**：`docs/adr/ADR-CEP-22-memory-model-source-and-cross-domain-edges.md` 文末（正文**不动**）
> **拟登记**：台账 Entry（编号待 architect / 台账分配）· 冻结面五步链（该 ADR 命中 `docs/adr/ADR-CEP-*.md` glob ⇒ hash 变更 ⇒ 清单单独 commit）

```markdown
## Status Update（<日期>，per 台账 Entry #N）—— 决策 3 的 RC 家族 `po` 授权表重新锚定 + 定性澄清（零决策变更）

> **性质**：追加式修订（per `ADR-CEP-00 §2 Decision 3` 的「原地更正 + 标注增补块 + 台账 entry」形态）——
> §2 决策 3 / §4.2 / §5.2 抓取点 2 / §7 决策汇总 —— **原文不改**；**自本段起以本段文本为准**。

### ① 事实更正（本段的事实依据，全部仓内可机检）

- 论文 §8.5.2 给出 `ppo` 形式化的**只有两个**：`SC: ppo ≜ po` 与 `x86-TSO: ppo ≜ po \ (st(x);po;ld(y))`
  （锚点 `docs/research/oswald/extracts/thesis-text-extract.md:7098-7110` · 锚点 id `T-C13`/`T-C14`）；
  RC 在论文中**仅被语义刻画**（release/acquire 操作），**未给出 RC 的 `ppo` 关系**（同文件 `:6887` · `T-C12`）。
- 本 ADR 决策 3 原文所引锚点 `HeteroGen_HPCA2022-draft.md:1266-1276` 实为 **Table I「case study 协议清单表」**
  （七个同构协议；`extracts/HeteroGen_HPCA2022-draft.md:1243` / `:1275-1276` · `H-C01`/`H-C02`）
  ⇒ **该表不含任何程序序 / 一致性序语义** ⇒ **锚点不支撑所锚的序关系断言**。
- ⛔ 对照：决策 3 的 **PLO 行无误** —— 论文原文「PLO-CC … enforce[s] … partial-load-order that enforces the
  **W→W and the R→W** orderings but not the other two」（`extracts/HeteroGen_HPCA2022-draft.md:1253-1256`）。

### ② 定性（决策 3 的 RC 三表的性质）

`RC-acquire` / `RC-release` / `RC-data` 三条 `po` 授权表 = **本仓依 RC 的 acquire/release 语义所作的解释性建模**；
⛔ **论文未给出 RC 的 `ppo` 形式化**（事实依据见 ①）⇒ 其正确性由**本仓**负责，不以论文为据。
⇒ 「⚠️ **仓内不可复核**」的措辞更正为「**论文未给出可援引的形式化**」—— 二者对读者的行动含义不同
（前者 ⇒ 去仓内找；后者 ⇒ 本仓自创，须自行复核与追责）。

### ③ 溯源纪律的显式例外（决策 3「无锚点 ⇒ 不登记」的例外条款）

决策 3 的硬纪律「⛔ 无锚点 ⇒ 不登记（该标签的域一律 `undecided`）⇒ 不得编值」**增补一条例外**：
> **例外对象** = 「论文**未给出形式化**、但存在**公认语义刻画**」的标签（如 RC 的 acquire/release 语义）；
> **登记条件** = ① 语义刻画来源可指名可复核 ② 派生规则逐条写明 ③ 派生结果附**自洽性对照**
> ④ ⛔ 该标签的锚点**不得**指向不含序语义的一手材料 ⑤ 该例外清单本身**受闸守**（⛔ 不新造 invariant 编号）

⇒ 本例外清单当前 = `{RC-acquire, RC-release, RC-data}`；`TSO` **不在**例外清单（0 实例 ⇒ 维持 `undecided`）。

### ④ 连带更正

- §4.2 的「RC 家族规则『仓内不可复核*』」⇒ 按 ② 更正措辞；
- §7 决策汇总「v1 覆盖 SC / RC-acquire / RC-release / RC-data / PLO」**不变**（规则集合不变，**只改性质标注与锚点**）；
- **决策 1~9 的结论零变更**；`INV-CEP-13-IC1` 复用不变；**新增 invariant 数 = 0**。

### ⑤ 边界

- 本段**不改**任何已签批 ADR 正文 · **不改** `protocol_instance.schema.json` / `protocol_instances.json`；
- 本段**不**引入任何新的程序序语义 —— 只**更正锚点 + 显式定性**；
- 若 ArchForge 给出 RC 的形式化 `ppo` 定义 ⇒ 以**取代流程**修正本段（⛔ 不得直接改本段）。
```

---

## 7. 连带项（建议与本裁决**同批**处理，避免二次改写）

| # | 连带项 | 处置建议 |
|---|---|---|
| L1 | **原因码计数**：ADR 决策 7 写「**10 码**闭集」，代码 `compound_graph_falsifier.hh:87-99` 实为 **11 个**（`malformed-trace` 在代码里是独立码，ADR 里写成「及细分 4 个」的父类） | 与本裁决**同批**澄清口径：`malformed-trace` 是否独立成码 ⇒ 二选一（代码删该码 / ADR 改述为 11 码） |
| L2 | **G2 实施面**：ADR 的 V-22-3/4/5/6/7/10 六条判据仍标「⭭ 待落地」，而实现 change 已归档（验收 PASS 17/17） | 建议 planner 补一次**判据载体回填**（与本裁决同批，否则 RC 语义再改一次要重跑两遍） |
| L3 | **运行期轨迹**：v1 的迹是**手写**（`ADR-CEP-22` 决策 2 的诚实边界 + R-5）⇒ 判定与仿真无关 | 另立 backlog（ADR 已如此登记）；⛔ **不得**在本裁决里悄悄扩大范围 |

---

## 8. 不做什么（明确排除）

- ❌ **不**改 `ADR-CEP-22` 的**任何决策结论**（决策 1~9 零变更）—— 本简报只争「锚点 + 性质标注 + 纪律例外」；
- ❌ **不**改 RC 三表的**取值**（语义无异议，见 §5.1 自洽性论证）—— 本简报**不主张**改表；
- ❌ **不**新增 canonical 流 / 不改 `protocol_instance.schema.json` / 不动 `protocol_instances.json`；
- ❌ **不**代替 architect 拍板 —— §5 的三个选项由 **architect** 选，落地需 **用户 ratify**；
- ❌ **不**在无台账 Entry 授权前落盘 ADR 改动（冻结面五步链）。

---

## 9. 诚实边界

| # | 边界 |
|---|---|
| B1 | 本简报的**论文侧事实**（F1/F2/F5/F7）已逐条落到**仓内可机检**的锚点（21/21 命中，`scripts/test/cep_oswald_provenance_check.sh` 全绿）；**RC 语义来源**（F8）引自**跨仓** mynotes 知识库，**不是**规范性来源 |
| B2 | §5.1 的「与 RC 经典形态一致」是**工程自洽性论证**，非**规范引用** —— 若需规范级锚点，应引 RC/C11 的一手文献（本轮**未**获取） |
| B3 | 「论文未给出 RC 的 `ppo` 形式化」是基于**博士论文**（178 页）的否定性推断；若 **HPCA 2022 会议版**另有 RC 序规则而博士论文未收录，本结论需修正 —— 本轮**未**逐字核对会议版 PDF |
| B4 | 本简报**未**运行任何测试/闸（闸的结论取自 2026-11-25 的实跑记录） |
| B5 | 本简报**不构成**对 `ADR-CEP-22` 的取代 —— 落盘形式只能是追加 `## Status Update` 段 |

---

**建议下一步**：architect 在 §5 三选项中择一（**推荐 C**）→ 用户 ratify → 台账 Entry 授权 → builder 走五步链落盘 + 与 L1/L2 同批回填。
