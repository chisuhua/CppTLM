# ADR-CEP-22 · SU ⑥ 的 **L3** 调研报告 —— RC 的规范性一级来源与「登记载体另议」

> **定位**：`ADR-CEP-22` SU(2026-11-25) ⑥ 登记的 **L3**（「RC 的**规范性**一级来源未获取 ⇒ ④ 条件⑤ 的登记载体须另议 ⇒ 另立调研单元」）的**调研 + 决策**载体。
> **本文件的角色**：architect 产出（调研 + 双审）。⚠️ **不含过程日志**（finding 编号 / 评审轮次 / 归档流水）—— 那属治理台账（`.rddf/roadmap/objectives/obj-cep-platform.md`）。
> **落盘顺序**：oracle（先）→ critic（后）—— 顺序不可颠倒。
> **关联**：`ADR-CEP-22`（决策 3 · `:59-76` · SU(2026-11-25) ①~⑦ `:306-461`）· `ADR-CEP-12` 决策 3 · `ADR-CEP-13` SU(2026-11-24) ② ·
> `docs/research/oswald/`（论文侧锚点面）· 闸 `scripts/test/cep_oswald_provenance_check.sh`（P-1~P-7）。

---

## 0. L3 的问题陈述（可判定的形式）

SU(2026-11-25) ④ 的条件⑤ 末句（`ADR-CEP-22:424-425`）：

> ⛔ 条件⑤ 的登记载体当前绑定 **Oswald 论文**的锚点面（例外清单全部源自该论文）；若日后引入**非该论文**的语义来源
> （如 RC 的规范性一级文献 · 见 ⑥ 的 L3），登记载体**须另议** ⇒ 归 L3 调研单元处理。

⇒ **L3 = 一个条件式命题**，由两个子问题构成：

| 子问题 | 内容 | 性质 |
|---|---|---|
| **Q1（事实）** | RC 是否存在**规范性一级来源**？其中是否含**序关系（`ppo`）语义**（条件④ 的硬要求：锚点材料**不得**不含序语义）？ | 外部事实 |
| **Q2（决策）** | 若引入**非 Oswald** 的语义来源 ⇒ ④ 条件⑤ 的**登记载体**如何「另议」？ | 架构决策 |

⚠️ **L3 的真实价值不在「补一个锚点」**，而在 **SU ② 的定性隐含了一个外部前提**（`ADR-CEP-22:366-367`）：

> **性质定性（核心）**：`RC-acquire` / `RC-release` / `RC-data` 三条 `po` 授权表 = **本仓依 RC 的 acquire/release 语义所作的解释性建模**；
> ⛔ **论文未给出 RC 的 `ppo` 形式化**（F1/F2）⇒ 其正确性**由本仓负责**，**不以论文为据**。

⇒ 「**RC 的 acquire/release 语义**」是本仓三表**唯一的外部依据**（论文侧已被 F1/F2 排除）⇒
⇒ **L3 = 用 RC 的一级来源检验这个前提是否成立** ⇒ 若前提不成立 ⇒ 不是「锚点强度」问题，而是**定性本身**的问题。

> **结论指针**：**Q1 已答**（§2）⇒ **Q2 已裁决**（§3）⇒ 裁决 = **方案 C-0（不引入外部来源）· 零实施**；
> **三项登记动作** A1 措辞更正 · A2 正面确认（人工评审级）· A3 后续选项登记；⛔ **无 design_drift**（RC 三表取值零变更）。

---

## 1. 仓内取证基线（2026-11-25 · 全部仓内可机检 · ⛔ 不依赖外部事实）

### 1.1 本仓 RC 授权表的**实际取值**（决策 3 · `:66-68` + 实现）

| 标签 | 读侧 `Allow_read` | 写侧 `Allow_write` | 溯源列（原文） |
|---|---|---|---|
| `RC-acquire` | {po 边 (a→b)：**a 是读**} = **R→R ∪ R→W** | —（不在写侧词表） | ⚠️ RC 语义 · 锚点 = `ADR-CEP-12` 决策 3 ⇒ **仓内不可复核** |
| `RC-release` | —（不在读侧词表） | {po 边 (a→b)：**b 是写**} = **R→W ∪ W→W** | ⚠️ 同上 ⇒ **仓内不可复核** |
| `RC-data` | **∅** | **∅** | ⚠️ RC 的 relaxed 对应级 ⇒ **仓内不可复核** |

⇒ 实现侧唯一消费点 = `include/tlm/cep/memory_model_trace.hh` 的 `allow_read`（`:181-189`）· `allow_write`（`:198-206`）·
`domain_preserves_po`（`:209-211`，= 读侧 ∪ 写侧）⇒ 消费方 = `compound_graph_falsifier.hh` 的 `ppo_com` 派生（决策 4）。

### 1.2 仓内 RC 授权表的**实际效果**（按 8 个实例的 `consistency_labels` 实测）

> **实测口径**：本表**非推导** —— 由只读探针按**仓内实现**（`memory_model_trace.hh` 的 `allow_read` / `allow_write` /
> `domain_preserves_po`）逐实例计算得出（探针置于**仓外** `/tmp/opencode/l3probe/` ⇒ **仓内零写入**）· 判据标签取自
> `include/tlm/cep/protocol_instances.json` 的 `consistency_labels`。

| 实例 | `consistency_labels` | 该域的 `ppo` 边集合（**实测**） |
|---|---|---|
| `mesif` | read `RC-acquire` / write `RC-release` | **R→R · R→W · W→W**（**缺 W→R**） |
| `moesi_gpu` | read `RC-data` / write `RC-release` | **R→W · W→W** |
| `chi_rnf` · `cxl_2_0` · `cxl_3_0_symmetric` · `tilelink_c` · `nvlink_4` | SC / SC | 全部 po 边（R→R · R→W · W→R · W→W） |
| `ualink_4` | PLO / PLO | R→W · W→W |

⇒ **ADR 自陈的自洽性对照**（`:76`）：「`mesif`（RC-acquire/RC-release）⇒ R→R · R→W · W→W（**缺** W→R —— **恰是 RC 经典结构**）」
⇒ ⚠️ **该「恰是 RC 经典结构」是一个可被 RC 一级来源直接检验的断言** —— 本仓此前**从未**检验过（见 §1.4）。

### 1.3 ⛔ **仓内与跨仓均无 RC 经典 `ppo` 结构的任何登记** ⇒ 本仓三表的方向性前提**无法自证**

| 检索面 | 实测 |
|---|---|
| `docs/adr/ADR-CEP-*.md` | 决策 3 的 RC 行溯源 = `ADR-CEP-12` 决策 3 ⇒ **转引链**；`ADR-CEP-12:99` 的依据 = HeteroGen **§II-B** ⇒ **不含序语义** |
| `docs/research/cep-coherence-protocol-space-survey.md` | `:544` = 「RC \| release / acquire（**按方向**）」· `:557` = 「RC-release-write / RC-data-write / RC-acquire-read / RC-data-read」⇒ **仅目录式分类，无 po 语义** |
| `docs/research/oswald/`（24 条 claim） | `T-C12` / `H-C04` = 四分 taxonomy（**定义性**）· `T-C13` / `T-C14` / `H-C05` = 论文**只给 SC 与 x86-TSO** 的 `ppo` ⇒ **RC 无序语义**（F1/F2） |
| 跨仓 ArchForge `docs/architecture/32-cep-based-coherence-architecture.md` | `:263` · `:418` · `:494` · `:531` ⇒ **只用标签**（`RC-acquire` / `RC-release`），**不给 po 语义** |

⇒ **结论（仓内可证）**：本仓 RC 三表的**方向**（读侧保留以读为源的边 / 写侧保留以写为目标的边）**没有任何一级或二级来源支撑**；
其唯一表述依据是**本仓自创的解释性建模**（SU ② 的定性本身**正确**）⇒ ⇒ **L3 是真空白，不是重复劳动**。

### 1.4 ⛔ `RC-acquire` 的授权表在**唯一的操作级判定面中零覆盖**

| 检索面 | 实测 |
|---|---|
| `test/cep_memory_model_traces.json`（6 条操作级迹） | 域绑定仅 `chi_rnf`(SC) 与 `moesi_gpu`(RC-data/RC-release) ⇒ **`mesif` 零出现**（grep 命中 **0**） |
| `test/test_cep_compound_graph_falsification.cc` | `mesif` **零命中** |
| ⇒ 推论 | `RC-acquire` 的 `po` 授权表（**R→R ∪ R→W** —— 与经典 RC 差异最大的一条）**从未被任何判定断言触及** ⇒ 其正确性**完全未被检验** |

⇒ ⚠️ **若 L3 判定三表方向需修正 ⇒ 影响面极小**（`mesif` 不在任何操作级语料中 ⇒ 现有 6 条迹的判定**不变**）。

### 1.5 影响面界定（若三表需修正 ⇒ 动到什么）

| 面 | 受影响？ | 依据 |
|---|---|---|
| `include/tlm/cep/memory_model_trace.hh` 的 `allow_read` / `allow_write` | ✅ **是**（唯一实现点） | §1.1 |
| `include/tlm/cep/compound_graph_falsifier.hh` + `test/test_cep_compound_graph_falsification.cc` + 闸 `cep_compound_graph_falsification_check.sh` | ⚠️ **间接**（消费 `ppo_com`） | §1.1 |
| 现有 6 条操作级迹的**判定结论** | ❌ **否**（`mesif` 零覆盖） | §1.4 |
| `include/tlm/cep/fence_effects.hh`（G4-a 派生面） | ❌ **否**（只判**族**，不依赖 po 方向） | `fence_effects.hh:115-123` |
| `include/tlm/cep/protocol_instance.schema.json`（标签**枚举**） | ❌ **否**（标签**名**不变） | 冻结面 |
| `ADR-CEP-22` 决策 3 的**取值** | ⚠️ **是**（若方向需更正）⇒ ADR = 冻结面 ⇒ **五步链** | `AGENTS.md` ADR 条目 |
| `ADR-CEP-13` SU(2026-11-24) ② R5 修正（同文「**RC-acquire 读 / RC-release 写已自带边**」） | ⚠️ **是**（同一前提） | `ADR-CEP-13:355` |

⚠️ **仓内共有 4 处 RC 语义描述面**，且**全部回溯到 Oswald 论文**（而该论文不含 po 语义）⇒ **同一未验证前提**：
① `ADR-CEP-22` 决策 3 三表 · ② `ADR-CEP-12` 决策 3 标签词表（`:69-70`）· ③ `ADR-CEP-13` SU(2026-11-24) ② R5「标签自带边」· ④ `fence_effects.hh` 的 R5 族判定。

### 1.6 「登记载体另议」的技术面（Q2 的约束条件 · 仓内可测）

| 载体要件 | 现状 | 引入**非 Oswald** 来源的代价 |
|---|---|---|
| **schema 身份** | `cep-oswald-source-manifest/v1` ⇒ **schema 名含 `oswald`** ⇒ 身份**失真** | ⚠️ 需 **v2 改名**（`cep-source-manifest/v2`）或**另立 manifest** |
| `papers[]` | 每条含 `origin_path`（**假定**原文在 `/workspace/main/mynotes/00_Raw_Sources/papers/`）+ `materialized_path`（`.papers/`，gitignored）+ `sha256` | ⚠️ 新文献的 PDF 未必在该路径 ⇒ `origin_path` 语义要放宽（`origin_kind`） |
| `extracts[]` | 4 份，**逐字节入 git**（`oswald/AGENTS.md` 纪律 2）+ `sha256` / `bytes` / `lines` | 新文献需**抽取件入 git**（供行级锚点） |
| `claims[]` | 24 条：`id` / `file` / `lines[2]` / `must_contain` / `anchor_sha256` / `claim` / `layer` | 新 claim 指向新抽取件的行区间（**形态完全可复用** ⇒ P-3 无需改判据面） |
| **P-3 判据** | 逐条核「行区间存在 + `must_contain` 命中 + 区间 sha256 匹配」 | ✅ **判据面零改动**（只吃 `claims[]` 的 `file`/`lines`/`must_contain`/`anchor_sha256`） |
| P-4 / P-6 | claims 条数**下界 24**（防删）· claim id ↔ index 双向零 orphan | 下界 24 → 25（若新增 1 条） |
| **`claims[].layer`** | 自由文本（`背景` / `分层` / `并发` / `异构` / `异构·承重` / `异构·NG1承重` / `工具链`）⇒ **证据强度分级已在用**（未严格枚举） | 可承载「RC 一级来源」的强度标注（**无需新概念**） |

⇒ **技术结论**：载体扩面的**增量集中在「文献身份 + 抽取件 + claim」三处**，而 **P-3 的判据面零改动** ⇒ 成本**中等且可控**；
⛔ 但 `docs/research/oswald/` 在 `arch-docs.json` 的 **`exclude_paths` 明文排除** ⇒ **architect 无写权** ⇒ 实施必属 **builder** 域。

### 1.7 ⛔ 仓内处理「外部证据」有**两种既有形态** ⇒ Q2 的方案集由它们界定

| 形态 | 代表 | 锚点载体 | 机检性 | 与条件⑤ 的相容性 |
|---|---|---|---|---|
| **A · 机检面** | `docs/research/oswald/`（manifest `cep-oswald-source-manifest/v1`） | 原文 PDF **不入 git** + **抽取件逐字节入 git** + `claims[]` 行区间（`must_contain` + 区间 sha256） | ✅ **机检**（P-1~P-7） | ✅ **正相容**（条件⑤ 明文要求 P-3 守） |
| **B · 人工审计面** | `docs/research/2026-11-01-cep-cxl-nonsymmetric-anchor-summary.md` 等 4 份（队列 #10/#11/#12/#13） | **锚点摘要**（条款级定位 + 断言摘要，⛔ 不整段复制）· **强度三档** `direct` / `analogical` / `inferred`（⛔ 禁止升格）· **钉版强度** `pin=strong`（取到字节 + sha256）/ `pin=weak`（仅 URL + 抓取日） | ❌ **人工**（basis 唯一人工审计面） | ⚠️ **不相容** ⇒ 选它须**改条件⑤**（放松机检纪律） |

⇒ **由二者界定的 Q2 方案集**（**结论待 §2 定稿**）：

| 方案 | 形态 | 增量 | 对条件⑤ | 代价 |
|---|---|---|---|---|
| **方案 A** | **扩 manifest**（`schema` 改名 v2 或另立 manifest）+ 抽取件入 git + `claims[]` 追加 | 文献身份 + 抽取件 + 1~N 条 claim + P-4 下界 +1 + index 同步 | ✅ **不动条件⑤** | 中（builder 实施；⚠️ 若同时改 ADR ⇒ 五步链） |
| **方案 B** | **锚点摘要**（形态 B）+ 借用其**强度三档 + pin 分级** | 1 份摘要文档 | ⚠️ **须改条件⑤**（放弃 P-3 机检） | 低（但**放松纪律** ⇒ 预期被 critic 判 BLOCKER） |
| **方案 C** | **不引入**（L3 以「核实 + 登记否定结论」闭合）⇒ 条件⑤ 的「若」**不触发** ⇒ **载体无须另议** | 0 | ✅ **不动条件⑤** | 最低 |

⚠️ **方案 C 的可接受性受 §2 门控**：若外部事实显示**本仓三表的方向与 RC 的公认刻画相反** ⇒ 方案 C = **掩盖实质问题** ⇒ 不可接受。
⇒ ⇒ **Q2 的裁决完全被 Q1（外部事实）门控**。

---

## 2. 外部事实（Q1）—— 2026-11-25 · librarian 核查

> **核查方式**：外部文献 / 规范文本的一手核查（subagent `librarian` · 逐篇实际请求 + 状态码核验）。
> ⚠️ **每条事实标证据强度**：`一手核查`（抓到正文并逐字核对）· `索引摘录`（句子出自正文但经搜索索引获得，未逐字校对版面/页码）·
> `推导`（由更高强度的事实推出）· `否定断言`（受检索边界限制的否定）。

### 2.1 RC 的**原始定义**与**规范文本**

| # | 事实 | 证据强度 |
|:--:|---|---|
| **F-RC-1** | **RC 本身没有规范文本（normative specification）** —— 不存在任何 ISA / 标准组织文档去 normative 地定义 release consistency（对照：x86-TSO 有 Intel SDM）⇒ **「以文献作为『规范性一级来源』」这一措辞对 RC 不成立**；文献是**同行评审一手文献（primary literature）** | 一手核查 |
| **F-RC-2** | **RC 的原始定义** = K. Gharachorloo, D. Lenoski, J. Laudon, P. B. Gibbons, A. Gupta, J. L. Hennessy, "Memory Consistency and Event Ordering in Scalable Shared-Memory Multiprocessors", **ISCA 1990**（17th Int'l Symp. on Computer Architecture · ACM, Seattle WA · **pp. 15–26**）· DOI `10.1145/325164.325102`（会议录）· `10.1109/ISCA.1990.134503`（IEEE 版）· 期刊化版 *ACM SIGARCH CAN* **18(2SI):15–26** · DOI `10.1145/325096.325102` | 一手核查（DBLP 镜像目录 + Springer 参考文献 + Google Research + ACM 元数据**四源一致**） |
| **F-RC-3** | **RC 的原始定义不含 `ppo` 形式化** —— 1990 年文以「访问分类（ordinary / special: sync = acquire\|release, nsync）+ 三条 **perform** 条件」的**散文式**定义给出；`ppo` 作为**命名关系**是后来（POWER/ARM 公理化模型 · RVWMO · LKMM）才出现的 | 索引摘录（未能逐字读全文 ⇒ 见 §2.5「获取失败」） |
| **F-RC-4** | **术语 RCpc / RCsc 出自同一篇 ISCA'90** —— 按 special(sync) 访问是「processor consistent」还是「sequentially consistent」处理 ⇒ **RCpc** / **RCsc**；RCsc 的规范性表述见 F-RC-6 | 索引摘录 |

### 2.2 ⛔ **不存在**给出「RC（1990 意义下）之 `ppo` 形式化」的一手材料

| # | 事实 | 证据强度 |
|:--:|---|---|
| **F-RC-5** | **没有任何同行评审文献给出 RC（1990 意义下）的 `ppo` 形式化。** 存在的是两类：**(a) RC 家族成员**的 `ppo` 公理化（POWER / ARM → *Herding Cats* TOPLAS 2014 · Sarkar et al. PLDI 2011；RISC-V → RVWMO 规范；Linux → LKMM）· **(b) RCsc / RCpc 两个变体**的**规范性** `ppo` 规则（**RISC-V RVWMO §16.1.3 规则 5–8**） | 一手核查（对可抓取 HTML 正文做了定位）+ **否定断言**（受检索边界限制） |
| **F-RC-6** | **唯一**把 RCsc / RCpc 写成**规范条款**的文本 = **RISC-V RVWMO v2.0（Ratified）§16.1.3 "Preserved Program Order"**：注释定义段逐字 —— 「the term "RCpc" stands for release consistency with **processor-consistent** synchronization operations, and the term "RCsc" stands for release consistency with **sequentially consistent** synchronization operations … in the context of RVWMO these terms are concisely and completely defined by Preserved Program Order rules 5-7」 | 一手核查（正文逐字） |
| **F-RC-7** | **Arm** 架构文档：Load-Acquire / Store-Release → **RCsc**；Load-AcquirePC（LDAPR）→ **RCpc**（**解释性**表述，**非** `ppo` 关系式） | 一手核查 |
| **F-RC-8** | ⚠️ **RVWMO 的语义对象与本仓不同**：它按**操作注释**（`acquire-RCpc` / `release-RCsc` 等）定义 `ppo`，**不是**本仓的**按访问标签**四分（`RC-acquire` / `RC-release` / `RC-data`）⇒ ⇒ **不可**作本仓 RC 三表的序锚点 | 一手核查 + 推导 |

### 2.3 ⛔ RC 的**经典刻画** ⇒ 与本仓三表的对照（F-RC-7 的 perform 条件）

| # | 事实 | 证据强度 |
|:--:|---|---|
| **F-RC-9** | **RC 的 perform 条件**（*Adve & Gharachorloo*, "Shared Memory Consistency Models: A Tutorial", *IEEE Computer* 29(12):66–76, Dec 1996 —— **§6.4.2 "Release Consistency (RCsc/RCpc)"**；亦见 *Adve* 博士论文 **Definition 2.4**）逐字：**(1)** before an ordinary load or store access is allowed to perform …, all [preceding] **acquire** accesses must be performed, and **(2)** before a **release** access is allowed to perform …, all [preceding] ordinary load and store accesses must be performed, and **(3)** special accesses are **processor consistent** with respect to one another | 索引摘录（句子出自正文，经搜索索引获得，未逐字校对版面） |
| **F-RC-10** | **推导**（由 F-RC-9）：条件 (1) ⇒ **acquire 读**之后的 ordinary 访存必须在其之后 perform ⇒ 保留「**以 acquire 读为源**」的 po 边 = **R→R ∪ R→W**；条件 (2) ⇒ 「**先前的访存**」必须在 **release 写**之前 perform ⇒ 保留「**以 release 写为目标**」的 po 边 = **R→W ∪ W→W**；条件 (3) ⇒ special 之间**不保 SC 序** ⇒ **不新增**边 ⇒ ⇒ **RC 的 `po` 保留集合 = {R→R, R→W, W→W}，即「缺 W→R」** | 推导（基于 F-RC-9 的索引摘录 ⇒ **强度受限**） |
| **F-RC-11** | **对照结论**：F-RC-10 的 {R→R, R→W, W→W} **与本仓 `mesif` 域的实测值完全一致**（§1.2 = RR · RW · WW · **缺 WR**）⇒ ⇒ **`ADR-CEP-22:76` 的自陈「缺 W→R —— 恰是 RC 经典结构」成立** ⇒ ⛔ **无 design_drift** · **RC 三表取值零变更** · **本报告先前登记的「名实互换」疑虑不成立**（见 §3 注） | 推导（跨 §1.2 实测 + F-RC-10） |
| **F-RC-12** | **额外自洽检验**：SC 的 `po` = **全部** 4 种边 ⇒ RC ⊂ SC（RC 是 SC 的**弱化**）⇒ 与「RC 属弱一致性模型」的定位一致 | 推导 |

### 2.4 ⚠️ 三处**不存在**的引文 —— ⛔ **本仓零引用**（无污染）

| 候选引文 | 核查结论 | 本仓引用面（实测） |
|---|---|---|
| "A Comprehensive Memory Model for Coherence" (Sorin/Hill/Wood, TOCS 2009) | ⛔ **该文不存在**（多库检索无记录）⇒ 真实对应物 = **专著** *A Primer on Memory Consistency and Cache Coherence*（1st ed. 2011 / 2nd ed. 2020 · **Open Access CC BY** · DOI `10.1007/978-3-031-01764-3`） | ✅ **零引用**（`grep` 全仓无该题名） |
| "Trading Memory Barriers for Scalability" (PPoPP 2017) | ⛔ **查无此文** | ✅ **零引用** |
| "Compositionality of Shared Memory" (POPL 2019) | ⛔ **查无此文**；真实对应物 = Podkopaev, Lahav, Vafeiadis, "Bridging the Gap between Programming Languages and Hardware Weak Memory Models"（arXiv:1807.07892 · DOI `10.1145/3290382`），其形式化对象 = **IMM + POWER/ARMv7/ARMv8**，**不含 RC 的 `ppo`** | ✅ **零引用** |
| 仓内唯一相关引用 | `docs/research/cep-coherence-protocol-space-survey.md:419`「与 **Sorin primer** 完全一致」⇒ 指 **Primer 专著**（真实且 Open Access）⇒ **准确** | ✅ **正确**（无需更正） |

⇒ ⇒ **本仓无杜撰引文**；上述三条系**本次调研 prompt 的候选清单有误**（已由核查纠正），**未污染任何仓内文件**。

### 2.5 获取失败（影响 §2 的强度上限）

| 对象 | 结果 |
|---|---|
| ACM DL 正文（ISCA'90 / Tutorial 1996） | **403**（付费墙）⇒ 正文不可得，仅元数据免费 |
| `r.jina.ai` PDF→文本（Primer / ISCA'90） | **每次 30s 网关超时** ⇒ 未取到任何文本 |
| Springer 第 5 章 HTML（Primer §5 "Relaxed Memory Consistency", pp. 55–89） | 页面可读但**不含正文**（仅摘要 + 参考文献）⇒ **Primer 是否给出 `ppo_rcsc` 关系式 = 未核实** |
| `arxiv.org/html/2508.04115v1`（Weak Memory Model Formalisms 综述 · CCPE 2026） | **超时** ⇒ 仅得索引片段（其中「Definition 5.14 (PPO of TSO): `ppo_TSO = RR ∪ RW ∪ WW`」为**索引摘录**） |
| Arm ARM 正文（DDI 0487） | **未核查**（HTML 为 JS 渲染，未取到正文） |
| 否定性断言的边界 | F-RC-5 / F-RC-9 的否定部分**受检索边界限制**（不排除同名预印本 / 冷门会议版本）⇒ ⛔ **不得**表述为「学界绝对没有」 |

### 2.6 开放获取可得性（若未来要引入 ⇒ 载体可行性）

| 文献 | 可得性 | 备注 |
|---|---|---|
| **ISCA'90**（RC 原始定义） | ✅ **公开**（多份课程镜像）· ACM DL 正文 403 | ⚠️ 正文为 **PDF** ⇒ 若入仓须做**抽取件**（形态同 oswald） |
| **Adve & Gharachorloo Tutorial 1996**（**含 RC 条文式定义**）· *IEEE Computer* 29(12):66–76 | ✅ **公开**（DEC WRL-95-7 / cs.unc.edu / MIT ARIES 镜像） | ⚠️ 同上 |
| **Primer 2nd ed. 2020** | ✅ **Open Access CC BY**（Springer + 作者镜像） | ⚠️ 第 5 章是否含 `ppo_rcsc` **未核实** |
| **RVWMO v2.0** | ✅ **公开**（Ratified 规范 HTML） | ⚠️ 语义对象是 **RCsc/RCpc 变体**（F-RC-8）⇒ **不可**作本仓三表锚点 |
| **LKMM / Arm 文档 / Herding Cats / Sarkar'11 / Podkopaev'19** | ✅ 公开 | ⚠️ 语义对象同上（RC **家族成员** / 变体） |

---

## 3. 决策（Q2）—— **裁决：不引入外部来源**（方案 C-0）· 零实施

### 3.1 Q1 的答案（两句话）

> **① RC 没有规范文本，且不存在给出 RC 之 `ppo` 形式化的一手材料**（F-RC-1 / F-RC-3 / F-RC-5）⇒ **无外部 `ppo` 序锚点可得**。
> **② 但 RC 的经典刻画（perform 条件）存在**，且由此推出 RC 的 `po` 保留 = **{R→R, R→W, W→W}（缺 W→R）}**（F-RC-9 / F-RC-10）⇒
> **与本仓 `mesif` 域实测值完全一致** ⇒ **`ADR-CEP-22:76` 的自陈成立** ⇒ ⛔ **无 design_drift**（F-RC-11）。

⚠️ **措辞区分（防误读）**：「无外部序锚点」指的是 **无 `ppo` 形式化锚点**；⛔ **不是**「RC 无外部语义依据」—— RC 的 perform 条件**就是**外部语义依据（且满足条件④「含序语义」）。

### 3.2 裁决：**方案 C-0（不引入外部来源）** —— 三个理由，逐条独立成立

| # | 理由 | 依据 |
|:--:|---|---|
| **R1** | **引入不改变「派生」这一性质** —— 外部候选（*Adve & Gharachorloo* 1996 的 RC 条文 / ISCA'90）**均不含 `ppo` 形式化**（F-RC-3 / F-RC-9）⇒ 本仓的**读侧 / 写侧分列授权表**仍须**派生** ⇒ ⇒ 引入后 SU ② 的定性（「本仓依 RC 的 acquire/release 语义所作的**解释性建模**」）**一字不改** ⇒ **收益仅止于「对照」** | F-RC-3 · F-RC-9 · `ADR-CEP-22:366-367` |
| **R2** | **对照的收益已在本报告层获得**（F-RC-11 的逐项推导 + §1.2 的实测对照）⇒ ⇒ 引入外部文献**不增加**可机检性（除非把 F-RC-10 升格为锚点，那正是方案 A 的成本） | §1.2 · F-RC-11 |
| **R3** | **机制成本显著** —— 引入非-Oswald 来源 ⇒ `schema: cep-oswald-source-manifest/v1` 的**身份失真**（schema 名含 `oswald`）⇒ 需 v2 改名或另立 manifest + **抽取件逐字节入 git**（IP / 体积）+ `papers[]` 的 `origin_path` 语义放宽 + `claims[]` 追加 + **P-4 下界 24 → 25** + index 同步 + P-6 双向对账 + **builder 实施** + **五步链落盘** | §1.6 · §1.7 · `oswald/AGENTS.md` 纪律 2/4/5 |

### 3.3 ⛔ **枢轴**：为什么「不引入」⇒ **④ 条件⑤ 的登记载体无须另议**

条件⑤ 的原文（`ADR-CEP-22:411-413`）：

> ⑤ 该例外标签的**语义刻画锚点**（条件①）与「**论文未给出形式化**」的**举证锚点** ⇒ **二者均须**在 `docs/research/oswald/source-manifest.json` 的 `claims[]` 内登记 …

⇒ **枢轴在条件① 的「语义刻画来源」是什么**（不是「序规则的来源」）：

| 条件① 的「语义刻画来源」 | 指向 | 是否已满足 |
|---|---|---|
| **定义性刻画**（= 一致性接口的**请求类型分类**） | **Oswald 论文的四分 taxonomy**（`T-C12` / `H-C04`，**已在 manifest 登记**） | ✅ **已满足** |
| **序规则（`ppo`）** | 论文**未给出** ⇒ 须派生（条件② + 条件④ 末句：定义性刻画的锚点不受条件④ 限制） | ✅ **合规**（既有设计） |

⇒ ⇒ **例外清单的现状本就合规**（条件① 由 `T-C12`/`H-C04` 满足 · 条件④ 由「定义性刻画不受限」满足 · 条件⑤ 双方锚点已在 `claims[]`）
⇒ ⇒ **不需要新增任何非-Oswald 的来源** ⇒ ⇒ **条件⑤ 末句的「若」不触发** ⇒ ⇒ **登记载体无须另议** ⇒ ⇒ **L3 以「核实 + 登记」闭合，零实施**。

### 3.4 本裁决的**三项登记动作**（落盘面 = `ADR-CEP-22` 的**追加 SU 段** · 零 schema / 零实现 / 零冻结面 hash 变更以外的内容）

| # | 动作 | 内容 | 强度标注 |
|:--:|---|---|---|
| **A1** | **措辞更正**（更正 SU ⑥ 的 L3 行） | 「RC 的**规范性**一级来源**未获取**」⇒ 「**RC 无规范文本**（不存在 normative specification）⇒ 非『未获取』而是『不存在』；其**原始定义**（ISCA'90）**不含 `ppo` 形式化**；**不存在**给出 RC 之 `ppo` 形式化的一手材料 ⇒ **无外部 `ppo` 序锚点可得** ⇒ **④ 条件⑤ 的「若」不触发** ⇒ **登记载体无须另议**」 | 一手核查（F-RC-1 / F-RC-3 / F-RC-5） |
| **A2** | **正面确认登记** | 由 RC 的 perform 条件推出 RC 的 `po` 保留 = {R→R, R→W, W→W}（**缺 W→R**）⇒ 与本仓 `mesif` 域**实测**一致 ⇒ ⇒ **`ADR-CEP-22:76` 的自陈成立** ⇒ ⛔ **RC 三表取值零变更** · ⛔ **无 design_drift** · 并**撤回**本报告早期登记的「名实互换」疑虑 | ⚠️ **人工评审级**（推导基于**索引摘录** ⇒ **不可机检** · ⛔ 不得升格） |
| **A3** | **后续选项登记** | 若要把 A2 的对照**升格为可机检锚点** ⇒ 须引入外部抽取件 + 扩 manifest（**schema 身份失真**）+ `claims[]` 追加 + P-4 下界 + builder 实施 ⇒ ⇒ **另立**（方案 A），⛔ **不在本轮** | 登记项 |

⚠️ **A2 的诚实边界（⛔ 必写）**：F-RC-10 的推导基于 **F-RC-9 的索引摘录**（*Adve & Gharachorloo* / *Adve* 论文正文**未逐字校对**）⇒ ⇒ **A2 是人工评审级断言**，**不是**仓内可机检事实 ⇒ 与条件④ 的「人工评审」定位一致（⛔ 不得当作鉴别力判据）。

### 3.5 跨角色依赖（⛔ 本裁决**不由 architect 落盘**）

| 动作 | 归属 | 依据 |
|---|---|---|
| 落 `ADR-CEP-22` 的**追加 SU 段**（A1 / A2 / A3） | **planner**（步 1 台账 Entry）+ **builder**（步 2/3/4/5 五步链） | ADR = M1 冻结面（`cep_freeze_check.sh` 的 `FROZEN_GLOBS` 第 1 条）⇒ **hash 变更 1** ⇒ 须走五步链；architect **不写** `.rddf/roadmap/**`（台账） |
| manifest 扩面 / 抽取件 / `claims[]` / P-4 下界（方案 A） | **builder** | `docs/research/**` 在 `arch-docs.json` 的 `exclude_paths` 内 ⇒ architect **不可写** |
| `memory_model_trace.hh` 的授权规则 | **builder** | 本裁决**不改**授权规则（⛔ 无 design_drift）⇒ **零实施** |

---

## 4. 双审 —— 待走

> ⛔ oracle（先）→ critic（后），顺序不可颠倒。

---

## 5. 待落盘的 **SU 段草案**（⛔ **待 oracle → critic 双审闭合 + planner 台账 Entry 先行** ⇒ 方可落盘）

> **落点**：`docs/adr/ADR-CEP-22-memory-model-source-and-cross-domain-edges.md` **文末追加**（**追加式** · 正文零改动 · M1 冻结面 ⇒ 走五步链）。
> **⛔ 措辞纪律**：① 否定断言用「**经本次核查未发现**」（非「不存在」）② A2 标「**人工评审级 · 不可机检**」③ ⛔ 不出现「已获外部背书 / 已机检 / 已证明」类措辞。

```markdown
## Status Update（2026-11-25，per 台账 Entry **#N**）—— SU(2026-11-25) ⑥ 的 **L3** 闭环：RC 一级来源核实 + 「登记载体无须另议」的裁定

> **本段处置**：SU(2026-11-25) ⑥ 登记的 **L3**（「RC 的**规范性**一级来源未获取 ⇒ ④ 条件⑤ 的登记载体须另议」）的**核实 + 裁定**。
> ⛔ **决策 1~9 的结论零变更** · ⛔ **RC 三表取值零变更** · ⛔ **④ 例外清单零变更** · ⛔ **新增 invariant 数 = 0** ·
> ⛔ 不改 `protocol_instance.schema.json` / `protocol_instances.json` / `source-manifest.json` · ⛔ 不改任何实现文件。

### ① 核实结论（外部一手核查 · 2026-11-25）

| # | 结论 | 强度 |
|---|---|---|
| **N1** | **RC 无规范文本** —— 不存在任何 ISA / 标准组织文档 normative 地定义 release consistency（对照：x86-TSO 有 Intel SDM）⇒ 「**规范性**一级来源」这一措辞对 RC **不成立** | 一手核查 |
| **N2** | **RC 的原始定义** = Gharachorloo, Lenoski, Laudon, Gibbons, Gupta, Hennessy, "Memory Consistency and Event Ordering in Scalable Shared-Memory Multiprocessors", **ISCA 1990**, pp. 15–26（DOI `10.1145/325164.325102`；期刊化版 *SIGARCH CAN* 18(2SI):15–26）⇒ 该定义**不含 `ppo` 形式化**（散文式：访问分类 + 三条 perform 条件） | 一手（书目）/ 索引摘录（不含 ppo） |
| **N3** | **经本次核查未发现**给出「RC（1990 意义下）之 `ppo` 形式化」的一手材料 ⇒ ⇒ **无外部 `ppo` 序锚点可得**（⛔ 否定断言受检索边界限制） | 一手核查 + 否定断言 |
| **N4** | 存在含 `ppo` 的**规范性**文本，但语义对象**不同** = **RISC-V RVWMO v2.0 §16.1.3**（RCsc / RCpc **变体**的注释与 ppo 规则 5–8；Arm：Load-Acquire/Store-Release → RCsc、Load-AcquirePC → RCpc；LKMM / *Herding Cats* / Sarkar'11 为 RC **家族成员**的公理化）⇒ ⇒ **不可**作本仓三表（按**访问标签**四分）的序锚点 | 一手核查 |
| **N5** | ⛔ **本仓无杜撰引文** —— 三处候选引文（「A Comprehensive Memory Model for Coherence」TOCS 2009 /「Trading Memory Barriers for Scalability」PPoPP 2017 /「Compositionality of Shared Memory」POPL 2019）经核查**均不存在**，且**在本仓零引用**；`docs/research/cep-coherence-protocol-space-survey.md:419` 的「Sorin primer」指**真实专著**（*A Primer on Memory Consistency and Cache Coherence* · Open Access）⇒ **无需更正** | 一手核查 + 仓内 grep |

### ② 措辞更正（更正 SU(2026-11-25) ⑥ 的 **L3 行**）

> **原登记**：「L3 = RC 的**规范性**一级来源**未获取** ⇒ ④ 条件⑤ 的登记载体亦须随之另议」

- ⛔ **更正为**：「**RC 无规范文本**（N1）⇒ 非『未获取』而是『**不存在**该类来源』；其**原始定义**（N2）**不含 `ppo` 形式化**；**经核查未发现**给出 RC 之 `ppo` 形式化的一手材料（N3）⇒ ⇒ **无外部 `ppo` 序锚点可得** ⇒ ⇒ **④ 条件⑤ 末句的「若」不触发**（例外清单**未**新增任何非-Oswald 的语义来源）⇒ ⇒ **登记载体无须另议**」
- **「未获取」→「不存在」**：前者暗示「再找找就有」，后者是核查后的结论（N1 / N3）—— ⛔ 否定断言的措辞受检索边界限制（N3）。

### ③ ④ 条件⑤ 的「若」为何不触发（枢轴 · 本段的核心论证）

- 条件① 的「**语义刻画来源**」指向 **定义性刻画**（一致性接口的**请求类型分类**），**不是**序规则来源 ⇒ 例外清单的语义刻画锚点 = **Oswald 论文的四分 taxonomy**（`T-C12` / `H-C04`，**已在** `source-manifest.json` 的 `claims[]` 内登记）。
- 条件④ 的末句已明确：「**定义性刻画**（如请求类型分类）的锚点**不受本条限制** —— 惟其派生的序规则须按条件②**显式标注为本仓派生**」。
- ⇒ 例外清单 `{RC-acquire, RC-release, RC-data}` 的现状**本就合规**（条件① ✓ · 条件④ ✓ · 条件⑤ 双方锚点已在 `claims[]` ✓）⇒ ⇒ **不需要**新增任何非-Oswald 的语义来源 ⇒ ⇒ **条件⑤ 末句的「若」不触发** ⇒ ⇒ **登记载体无须另议**。

### ④ 正面确认（**人工评审级** · ⛔ 不可机检 · ⛔ 三表取值零变更）

- **RC 的经典刻画**（*Adve & Gharachorloo*, "Shared Memory Consistency Models: A Tutorial", *IEEE Computer* 29(12):66–76, 1996, §6.4.2；亦见 *Adve* 博士论文 Definition 2.4）的三条 perform 条件 ⇒ 推出 **RC 的 `po` 保留集合 = {R→R, R→W, W→W}**（**缺 W→R**）：条件 (1)「ordinary 访存 perform 前所有**先前 acquire** 必须 perform」⇒ 保留以 acquire 读为源的边；条件 (2)「release access perform 前所有**先前 ordinary** 必须 perform」⇒ 保留以 release 写为目标的边；条件 (3)「special accesses 之间 processor consistent」⇒ 不新增边。
- ⇒ 与本仓 `mesif` 域（`RC-acquire` / `RC-release`）的**实测** `ppo` 边集合（**R→R · R→W · W→W**）**一致** ⇒ ⇒ **§2 决策 3 的 RC 三表方向正确** ⇒ ⇒ **§2 的自洽性对照「缺 W→R —— 恰是 RC 经典结构」成立** ⇒ ⇒ ⛔ **无 design_drift** · ⛔ **RC 三表取值零变更**。
- ⚠️ **证据强度上限**：上述推导的条文来源为**索引摘录**（正文未逐字校对）⇒ ⇒ **本项是人工评审级断言，⛔ 不是仓内可机检事实**，⛔ **不得**当作鉴别力判据（与条件④ 的「人工评审」定位一致）。
- ⚠️ **与 SU(2026-11-25) ② 的并存关系**：② 说「⛔ **论文未给出** RC 的 `ppo` 形式化 ⇒ 其正确性**由本仓负责**，**不以论文为据**」；本段说「与 **RC 本身**的经典刻画一致」⇒ ⇒ **两句并存且不矛盾**：前者是**关于论文**的事实，后者是**关于 RC**的对照 ⇒ ⇒ **正确性归属不变**（仍归本仓）—— ⛔ **不得**把本段读成「RC 三表已获外部背书 / 已机检」。

### ⑤ 后续选项（登记 · 本段不处置）

- 若要把 ④ 的对照**升格为可机检锚点** ⇒ 须引入外部文献的抽取件（逐字节入 git）+ **扩 manifest**（`schema: cep-oswald-source-manifest/v1` 的身份失真 ⇒ v2 改名或另立 manifest）+ `claims[]` 追加 + **P-4 下界 24 → 25** + index 同步 + P-6 双向对账 ⇒ ⇒ **另立**（⛔ 不在本段）。
- ⚠️ 即便如此，外部候选**均不含 `ppo` 形式化**（N2）⇒ 本仓的读侧 / 写侧分列授权表**仍须派生** ⇒ ⇒ 引入**不改变**「本仓解释性建模」这一定性 ⇒ ⇒ 升格的收益仅止于「对照可机检」。

### ⑥ 本段的边界

- ⛔ **零实施**：不改 manifest / schema / 实例声明面 / 实现 / 测试 ⇒ **零 spec 变更** · **零冻结面 hash 变更**（仅本 ADR 一处 hash 变更）· **新增 invariant 数 = 0**。
- ⛔ **不构成对任何已签批 ADR 的替代**；⛔ 不改 `ADR-CEP-13` SU(2026-11-24)（其 ② R5 的族级结论不依赖本段）—— ⚠️ 若日后要把 ④ 的对照扩展到 `ADR-CEP-13`，须另立。
- ⛔ **否定断言**（N1 / N3）受检索边界限制 ⇒ 日后若出现新的 RC `ppo` 形式化材料 ⇒ 以**取代流程**处理（⛔ **不得**直接改本段）。
- **关联**：台账 Entry **#N** · 调研与决策报告 `.rddf/reviews/adr-cep22-l3-rc-ppo-source-review.md`（§1 仓内取证 · §2 外部事实 · §3 决策 · §5 SU 段草案 · §6 诚实边界）· 论文侧锚点面 `docs/research/oswald/source-manifest.json` · 闸 `scripts/test/cep_oswald_provenance_check.sh`。
```

---


## 6. 诚实边界（⛔ 本报告自身的边界）

| # | 边界 |
|:--:|---|
| 1 | **§1 的每一条都是仓内可机检事实**（带一手 `路径:行`）⇒ ⛔ **不含**任何关于「RC 的经典语义是什么」的断言。 |
| 2 | **§1 与 §2 的分工**：§1 = **仓内可机检**事实（⛔ 不含任何外部语义断言）· §2 = **外部事实**（逐条标证据强度）⇒ ⇒ §1.2 里 ADR 的自陈（「**缺** W→R —— 恰是 RC 经典结构」）在 §2 完成后**得到外部对照支持**（F-RC-11）⇒ ⛔ 但该支持的强度是**推导 + 索引摘录级**（见第 7 条）⇒ ⛔ **不得**读作「已机检」。 |
| 3 | **§3 已是裁决**（方案 C-0）⇒ 但 ⛔ **在 oracle → critic 双审闭合 + planner 台账 Entry 先行之前，本报告仍不构成落盘授权**（不改 ADR / 不改实现 / 不改载体）。 |
| 4 | **影响面界定基于 2026-11-25 的仓内状态**（`consistency_labels` 8 实例 · 6 条操作级迹 · 4 处 RC 语义描述面）⇒ 若并发变更则需重测。 |
| 5 | **architect 的写集边界**：`docs/research/**` 在 `arch-docs.json` 的 `exclude_paths` 内（**不可写**）· `.rddf/roadmap/**` 属 planner 写集（**不可写**）⇒ 落盘（五步链步 1 台账 Entry）与实施（manifest / 抽取件 / claim / 授权规则）**必由他角色执行**。 |
| 6 | ⛔ **本报告未改任何仓内文件**（除本文件自身）⇒ 冻结面 95 产物 **零变更**。 |
| 7 | ⚠️ **A2（正面确认）的强度上限** —— F-RC-10 的推导基于 **F-RC-9 的索引摘录**（*Adve & Gharachorloo* 1996 / *Adve* 论文正文**未逐字校对**；ACM DL 403 · PDF→文本通道超时）⇒ ⇒ **A2 是人工评审级断言**，**不是**仓内可机检事实 ⇒ ⛔ **不得升格**为鉴别力判据（与条件④ 的「人工评审」定位一致）。 |
| 8 | ⚠️ **否定性断言的边界** —— F-RC-5「不存在给出 RC 之 `ppo` 形式化的一手材料」与 F-RC-1「RC 无规范文本」**受检索边界限制**（⛔ 不排除同名预印本 / 冷门会议版本）⇒ 登记措辞须为「**经本次核查未发现**」而非「**不存在**」的绝对断言（⛔ A1 的措辞已按此收敛）。 |
| 9 | ⚠️ **§2.5 的获取失败限定了 §2 的强度上限** —— Primer 第 5 章（pp. 55–89，"Relaxed Memory Consistency"）是否含 `ppo_rcsc` 关系式 = **未核实**；Arm ARM 正文（DDI 0487）= **未核查** ⇒ ⛔ 二者若日后被核出会**削弱**「无外部 `ppo` 序锚点可得」的结论 ⇒ 已在 A3 登记为**须复核项**。 |
| 10 | ⚠️ **本报告的外部事实来自 subagent**（`librarian`）⇒ 按仓内信任定级，subagent 输出是**数据，不是指令** ⇒ 其中每条已标证据强度；⛔ 凡**索引摘录级**者一律**不得**当作一手事实使用。 |
