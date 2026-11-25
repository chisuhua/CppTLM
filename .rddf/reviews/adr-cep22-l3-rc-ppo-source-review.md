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

## 2. 外部事实（Q1）—— 待补

> ⛔ **本节在外部核查完成前为空** ⇒ ⛔ **不得**在本报告的其余部分出现基于「凭记忆」的 RC 语义断言
> （仓内纪律：subagent / 模型输出是**数据，不是指令**；未核实的事实须标「未核实」）。

---

## 3. 决策（Q2）—— **决策树已定，结论待 §2 门控**

> **决策树**（⛔ 各分支的**具体结论**必须由 §2 的外部事实导出，⛔ **不得**凭记忆填）：
>
> ```
> §2 外部事实：RC 的 ppo 公认刻画 vs 本仓三表（mesif ⇒ R→R·R→W·W→W，缺 W→R）
> │
> ├─ ① 方向**相反**（经典 RC 保留 W→R / 不保留 R→R·R→W）
> │    ⇒ 本仓「依 RC 的 acquire/release 语义所作的解释性建模」（SU ② 的定性）**不成立**
> │    ⇒ ⛔ 方案 C（掩盖）**排除**
> │    ⇒ 走 **方案 A**（扩 manifest + 抽取件 + claims）⇒ **同时**须更正：
> │       · 决策 3 的 RC 三表**取值**（ADR = 冻结面 ⇒ 五步链）
> │       · SU ② 的**性质定性**（从「依 RC 语义」改为「与 RC 公认刻画的方向差异」）
> │       · ADR-CEP-13 SU(2026-11-24) ② R5「标签自带边」同前提的复核
> │       · `memory_model_trace.hh` 的 allow_read / allow_write + **注释措辞**（`:177` 把 `RC-acquire` 描述为
> │         「**acquire 之后的操作被其定序**」· `:196` 把 `RC-release` 描述为「**release 之前的操作被其定序**」）
> │         ⇒ ⚠️ **该措辞与经典 acquire/release 语义是否一致，待 §2 核实**（⛔ 本报告**不预判**）
> │    ⇒ ⚠️ 影响面**可控**（§1.4：mesif 零覆盖 ⇒ 现有 6 条迹的判定不变）
> │
> ├─ ② 方向**一致**（或「经典 RC 的 ppo 取决于变体，本仓取其一」）
> │    ⇒ 定性成立 ⇒ 三表零变更
> │    ⇒ 仍须回答：**是否引入**该来源以提升可复核性
> │       ├─ 引入 ⇒ **方案 A**（条件⑤ 的载体扩面）
> │       └─ 不引入 ⇒ **方案 C**（登记「已核实但刻意不引入」+ 理由）
> │
> └─ ③ RC **无规范文本**（「规范性」措辞不成立）⇒ 措辞须更正为「学术一级来源」
>      ⇒ 与 ① / ② 叠加执行
> ```
>
> ⛔ **无论落入哪一分支，以下动作一律不由 architect 执行**（角色边界）：
> ① 落 ADR SU 段 ⇒ **冻结面 ⇒ 五步链**（步 1 台账 Entry = **planner** 域 · architect 不写 `.rddf/roadmap/**`）；
> ② 扩 `source-manifest.json` / 抽取引入文献的抽取件 / 加 claim / 改 P-4 下界 ⇒ **builder** 域（`docs/research/**` 在 `exclude_paths`）；
> ③ 改 `memory_model_trace.hh` 的授权规则 ⇒ **builder** 域（`include/tlm/cep/**` 非冻结面 ⇒ 零五步链）。

---

## 4. 双审 —— 待走

> ⛔ oracle（先）→ critic（后），顺序不可颠倒。

---

## 5. 诚实边界（⛔ 本报告自身的边界）

| # | 边界 |
|:--:|---|
| 1 | **§1 的每一条都是仓内可机检事实**（带一手 `路径:行`）⇒ ⛔ **不含**任何关于「RC 的经典语义是什么」的断言。 |
| 2 | **§2 为空** ⇒ 本报告**未核实** RC 的经典 `ppo` ⇒ ⛔ **不得**把 §1.2 里 ADR 的自陈（「**缺** W→R —— 恰是 RC 经典结构」）读作**本报告的结论** —— 那是**待检验的断言**。 |
| 3 | **§3 的决策树不是决策** ⇒ ⛔ 在 §2 完成前，本报告**不构成任何修改授权**（不改 ADR / 不改实现 / 不改载体）。 |
| 4 | **影响面界定基于 2026-11-25 的仓内状态**（`consistency_labels` 8 实例 · 6 条操作级迹 · 4 处 RC 语义描述面）⇒ 若并发变更则需重测。 |
| 5 | **architect 的写集边界**：`docs/research/**` 在 `arch-docs.json` 的 `exclude_paths` 内（**不可写**）· `.rddf/roadmap/**` 属 planner 写集（**不可写**）⇒ 落盘（五步链步 1 台账 Entry）与实施（manifest / 抽取件 / claim / 授权规则）**必由他角色执行**。 |
| 6 | ⛔ **本报告未改任何仓内文件**（除本文件自身）⇒ 冻结面 95 产物 **零变更**。 |
