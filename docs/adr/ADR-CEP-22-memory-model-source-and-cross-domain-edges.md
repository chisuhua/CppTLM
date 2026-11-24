# ADR-CEP-22: 每域模型来源与域间边构造规则（操作级复合一致性判据的可判定前置）

> **状态**: 🟡 **Proposed** ✅ **已签发**（v1.0, 2026-11-24 —— per **用户 ratify「A」** + **oracle 1+1 轮**（首审 REJECT → 复审 APPROVE）+ **critic 1 轮**（`mergeable: yes` · **0 BLOCKER**）双审闭合；详见文末 **Status Update**）
> **日期**: 2026-11-24 · **作者**: CppTLM build agent(architect mode)
> **作用域**: **「每域模型来源」+「域间边构造规则」** 的裁定 —— `ADR-CEP-13` 决策 2 的复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 在**操作级**的**可判定来源**与**构造规则**
> **Parent**: **无** —— 新建决策，⛔ **不取代**任何既有 ADR
> **关联真源**: 真源 1 `docs/architecture/07-coherence-exploration-platform.md` §2.2 + §5.1.4（实体 `docs/architecture/cep/protocol-engine.md` §5.1.4）· 真源 3 `ADR-CEP-13`（决策 1/2/5②）· `ADR-CEP-12`（决策 3 = 标签的**用途** = 跨域组合判据 · 决策 4 = 请求级标签）· `ADR-CEP-08`（决策 1 = Ordering Point）· `ADR-CEP-20`（决策 6 = 能力阶梯 + 档位声明义务）· `ADR-CEP-17`（决策 7 = 5 语义）· `ADR-CEP-02`
> **一手锚点**（实测，逐 `file:line` 核实）:
> - `include/tlm/cep/protocol_instance.schema.json` —— `consistency_labels`：`read` = `SC|TSO|RC-acquire|RC-data|PLO`；`write` = `SC|TSO|RC-release|RC-data|PLO`；另有 `request_level`
> - `include/tlm/cep/protocol_instances.json` —— 实测 **8 个实例**：`moesi_gpu` = RC-data/RC-release · `mesif` = RC-acquire/RC-release · `ualink_4` = **PLO/PLO** · `chi_rnf`/`tilelink_c`/`cxl_2_0`/`cxl_3_0_symmetric`/`nvlink_4` = SC/SC ⇒ ⚠️ **`TSO` 本仓 0 实例；`PLO` 有 1 个现役实例**（`ualink_4`，`status: implemented`；同记 `ADR-CEP-12` 的「8 实例」SU 段）
> - `docs/architecture/cep/protocol-engine.md:100-104` —— 5 语义表；**仓内唯一的 per-label 序陈述**在 `:104`（PLO-CC「**只强制 W→W 与 R→W 序**」）· `:101` TSO 段**只谈 sharer tracking、不谈序** · `:100` SC = SWMR + writer-initiated invalidation；`docs/adr/ADR-CEP-17-ruby-slicc-protocol-family.md:155-158` 同表决策版（含指针 `HeteroGen_HPCA2022-draft.md:1266-1276` Table I + `:276-299` 标签定义）
> - ⚠️ **论文原文不在本仓**（`find -iname "*hetergen*" -o -iname "*oswald*"` 实测 **0 命中**）⇒ 一手锚点只能**转引** ⇒ 决策 3 的 RC 家族规则标注「**仓内不可复核**」
> - `include/tlm/cep/compound_graph_falsifier.hh` —— 现役工装图顶点 = `(instance, network, vc)`（**通道依赖图**）⇒ 与公理要求的操作图**不同型**；`test/cep_compound_graph_corpus.json` —— `compound_edges` = `[]`，`_status` 写明「边类无定义 ⇒ 复合公理**当前不可证伪**」
> - `scripts/test/cep_freeze_check.sh:47` —— `FROZEN_GLOBS` 含 `docs/adr/ADR-CEP-*.md` ⇒ 本 ADR 命中 M1 冻结面；`test/cep_adr_structural_deferrals.json` —— `decision_adrs` = **21** 份（闸「未登记即红」）；`scripts/test/docs_adr_index_check.sh` + `scripts/docs/gen_adr_index.py` —— 索引三列判据面
> **审稿报告**: `.rddf/reviews/ADR-CEP-22-memory-model-source-and-cross-domain-edges-review.md`（**metis 1 轮** gap 分析 · **oracle 1+1 轮**（首审 REJECT → 复审 APPROVE）· **critic 1 轮**（`mergeable: yes` · 0 BLOCKER）⇒ 判定行原文与**逐项处置**全录；⚠️ 过程日志只在此处，**不进本文正文**）
> **关联 change**: `backlog: cep-memory-model-operational-trace`（**尚未立项** ⇒ AC 编号以 backlog 草稿的 `AC-N` 为准；立项后**回填** change 指针与 AC 映射）· **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md` Entry **#541**（落盘授权 · 用户 ratify）· **AC 映射**: 见 §Verification 的「AC 映射」列

---

## 1. 背景与问题

### 1.1 问题：三层缺口，逐层可测

`ADR-CEP-13` 决策 2 已把跨域组合判据定为**操作级**公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)`（`ppo_com(t) ≡ ppo_i`）。要能判定，需三样同时到位：**① 域的模型来源**（`ppo_i` 从哪来）· **② 操作级载体** · **③ 域间边构造规则**。本仓**三样都没有**：

- **G-a 无操作级模型** —— 现役工装 `compound_graph_falsifier.hh` 的图顶点 = `(instance, network, vc)` = **通道依赖图** ⇒ 与公理要求的**操作图**不同型 ⇒ 声明项派生的边**不能**证伪该公理（假绿）；
- **G-b 无域身份面** —— schema 的 13 个顶层属性**无** domain / ordering-point 字段（`ordering_assumption` = 逐网络 × 逐 VC 的排序假设；`family` = 来源族）⇒ 「线程 → 域」**无处承载**；
- **G-c 边类语义无处承载** —— `rfe`/`fr`/`ws` **两仓均无定义**（`ADR-CEP-12` schema：「⛔ 边类语义不在本仓定义」）⇒ 语料为空 ⇒ 「**不可证伪**」这一真结果（per 决策 5 ②）。

### 1.2 本 ADR 的定位与硬边界

本 ADR 是 G2 单元（`cep-memory-model-operational-trace`）的**开工前置**：裁定「每域模型来源 + 域间边构造规则」⇒ 产出**可直译**的规则。⛔ **不做**：不改任何**已签批 ADR 正文** · 不改 `protocol_instance.schema.json`（改字段类型/取值域须**重开 R-2 + 用户 ratify** —— per I-6 裁决记录 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §五（R-2e 本体所在节））· **新增 invariant 数 = 0** · 不立项实施 · 不产 run · v1 轨迹 = **手写 litmus 迹**（运行期采集另立 backlog）。

### 1.3 两条不可违背的上游约束

- **决策 2 的动机**：采用复合模型**正因**「所有域模型相同」**过严**（异构现实 = 各域模型不同，如 CPU 簇 SC + GPU 簇 RC）⇒ ⛔ 不得裁定「统一选一个全局家族」；
- **决策 5 ②**：平台**不提供无违例保证** ⇒ 判定面**只能**产出「证伪 / 未证伪 / 不可判」，⛔ 不得产出「已证明」。

---

## 2. 决策

### 决策 1 —— 域身份 = **操作迹侧的域标识**（⛔ 不进 schema）

**裁定**：域身份**只存在于操作迹数据面** —— 迹中每线程声明一个**域标识**（字符串），该标识**必须**解析到一个**已声明的协议实例 `id`**（`protocol_instances.json`）⇒ 域模型（决策 2）由此解析而得。⛔ 域标识**不进** schema ⇒ **零 schema 改动**。

- **解析失败**：线程未声明域 ⇒ `invalid` + `unbound-domain`；域标识解析不到实例 ⇒ `invalid` + `no-domain-instance`（⛔ **不得**静默降级为「未证伪」· backlog AC-6）；
- **多域 ↔ 单实例**：**允许** —— 域边界 = Ordering Point 作用域（`ADR-CEP-13` 决策 1 / `ADR-CEP-08` 决策 1）⇒ 同一实例可被多个域标识引用 ⇒ 这些域**共享**同一模型标签（决策 2 的直接后果；⛔ 不引入「域自声明模型」的冲突裁决）；
- ⚠️ **本决策的承重假设（须显式登记）**：「迹中的域标识**忠实代表**该线程所属的 Ordering Point 作用域」是**迹作者承担的假设** —— 域不进 schema ⇒ **配置面无任何机器判据**可校验它（⇒ 见 §4.2 代价）。

**排除**：「域 ⟺ 协议实例」（违反决策 1 的作用域定义）与「域 = schema 独立实体」（触 R-2e）—— 理由见 §3 选项 A/B。

### 决策 2 —— 每域模型来源 = **域绑定的协议实例的 `consistency_labels`**（`read`/`write` 二元组）

**裁定**：域 *i* 的模型来源 = 域 *i* 绑定的协议实例声明的 `consistency_labels` ⇒ **零新词表**。依据：`ADR-CEP-12` 决策 3 **明文**把标签的**用途**定为「`ADR-CEP-13` 的跨域组合判据」；决策 4 要求标签能到**请求级**（博士论文 §7.6.6：RCC 的普通 read 与 acquire 同发 `GetV` ⇒ 改名 `GetV_R`/`GetV_A`）；`ADR-CEP-13` 决策 1 定「域内用自己的 `ppo_i`」。

⚠️ **诚实边界（须逐字随引用传播）**：`consistency_labels` 是协议的**能力面**（能保证什么），⛔ **不是**运行期**实际**行为 ⇒ v1 取**声明面**；运行期采集另立 backlog。⛔ 不得把「标签 = 能力」升格为「标签 = 该域实际行为」。

### 决策 3 —— `ppo_i` 的派生 = **访问模式两层**：标签 → 访问模式 → 保留的程序序边

**裁定**：`ppo_i` = 域 *i* 内**被其模型保留**的程序序边集合。授权分**读侧 / 写侧**两张表；边 (a→b) 指同线程内 a 在 b 之前。

| 标签 L | 读侧授权 `Allow_read(L)` | 写侧授权 `Allow_write(L)` | 溯源 |
|---|---|---|---|
| **SC** | **全部** po 边（R→R · R→W · W→R · W→W） | **全部** po 边 | ✅ **定义级**（SC 的定义即「全部程序序被尊重」）⇒ **仓内可复核**（`protocol-engine.md:100` + `ADR-CEP-12` 决策 3） |
| **RC-acquire** | {po 边 (a→b)：**a** 是读} = R→R ∪ R→W（acquire 之后的操作被其定序） | —（该标签不在写侧词表） | ⚠️ RC 语义 · 锚点 = `ADR-CEP-12` 决策 3（RC 分 release-write/data-write/acquire-read/data-read，per HeteroGen §II-B）⇒ **仓内不可复核** |
| **RC-release** | —（该标签不在读侧词表） | {po 边 (a→b)：**b** 是写} = R→W ∪ W→W（release 之前的操作被其定序） | ⚠️ 同上 ⇒ **仓内不可复核** |
| **RC-data** | **∅**（relaxed 访问**不保留任何**程序序） | **∅** | ⚠️ RC 的 relaxed 对应级（`ADR-CEP-12` 决策 3 的两级划分：data-read / data-write）⇒ **仓内不可复核**；⛔ **不得**把同地址序（`po_loc`）塞进来 —— 那是**过严** ⇒ 会把 RC 合法的迹误判为 `falsified`（假阳性） |
| **PLO** | **∅**（PLO 不强制 R→R） | {po 边 (a→b)：**b** 是写} = R→W ∪ W→W（**只强制 W→W 与 R→W 序**） | ✅ **仓内可复核**（`docs/architecture/cep/protocol-engine.md:104` 逐字：「PLO-CC \| **PLO**(partial-load-order) … 只强制 W→W 与 R→W 序」）· ⛔ 该规则**只对写侧生效** ⇒ 读侧 ∅ |
| **TSO** | ⛔ **v1 不登记** | ⛔ **v1 不登记** | 本仓 **0 实例**；仓内**无** TSO 的**序**规则（`protocol-engine.md:101` 只谈 sharer tracking）⇒ 域绑定到声明 TSO 的实例 ⇒ `undecided` + `model-semantics-unavailable`（⛔ **不得**静默当 SC ⇒ 假绿）。⚠️ 其候选规则已知（enforced = po 去掉 W→R）⇒ 留待**有实例时**登记 |

**组合规则**：域 *i* 的 `ppo_i` = `Allow_read(read_label_i) ∪ Allow_write(write_label_i)`（**并集**，非交集；语义 = 任一端点的标签授权该边 ⇒ 保留）。

**溯源纪律（硬）**：每条规则标一手锚点 + **两档标注**（`仓内可复核` / `仓内不可复核`）；⛔ **无锚点 ⇒ 不登记**（该标签的域一律 `undecided`）⇒ **不得编值**（沿 schema `early_ack` 的 `unknown` 先例）。

**自洽性对照**（决策 2 来源的自检）：SC/SC 域 ⇒ 全部边；`mesif`（RC-acquire/RC-release）⇒ R→R · R→W · W→W（**缺** W→R —— 恰是 RC 经典结构）；`moesi_gpu`（RC-data/RC-release）⇒ R→W · W→W；`ualink_4`（PLO/PLO）⇒ R→W · W→W。

### 决策 4 —— 域间边构造规则（`rfe`/`fr`/`ws` **全部派生**；⛔ 不得作输入）

**裁定**：公理面的边**全部由「操作 + 读自映射 + 同地址写序候选 + 域绑定」派生**；⛔ 语料**不得**直接给出 `fr`/`ws`（⇒ `invalid` + `derived-edge-as-input`· backlog AC-3①）。

**输入面**（迹必须提供）：`ops`（读/写 + 地址 + 值 + 线程）· `rf`（读 → 它读到的写）· `co_candidates`（**逐地址**写的候选全序，可为空）· 线程内 op 序列（⇒ `po`）· 每线程的域标识（决策 1）。

| 边 | 构造规则（可直译） |
|---|---|
| **`ppo_com`** | `ppo_com` = `⋃_i ppo_i`；边 (a→b) ∈ `ppo_com` ⟺ ∃域 i：a、b 同属域 i 且 (a→b) ∈ `ppo_i`（决策 3） |
| **`rfe`**（read-from external） | `rfe(w, r) ⟺ rf(w, r) ∧ dom(w) ≠ dom(r)`（**域标识**比较 ⇒ **同实例跨域也算 `rfe`**，⛔ 非矛盾输入 —— 域 = Ordering Point 作用域，同实例的两个域 = 两个作用域） |
| **`ws`**（写序列化 = **逐地址** `co`） | 给定同地址写序候选 `co`：`ws(w1, w2) ⟺ co(w1, w2)`（**同地址**写的全序）。⚠️ **不取全局写序**（含跨地址）—— 跨地址写序**永不改变判定**（它只是候选序的冗余拷贝）⇒ 取之只会误导实现者维护无关边 |
| **`fr`**（from-read） | 给定 `co`：`fr(r, w2) ⟺ ∃w1: rf(w1, r) ∧ ws(w1, w2)`（**读 → 写**；`ws` 逐地址 ⇒ 同址自动成立；**静态** —— 不依赖候选全局序） |

**公理面图** `G_ax = (ops, ppo_com ∪ rfe ∪ fr ∪ ws)`；⛔ `fr`/`ws`/`rfe` **只入公理面图**，不作输入。

⚠️ **`fr` 的约定依赖（须显式登记）**：另一常见形态是**写 → 写**（`fr(w', w) ⟺ ∃r: rf(w, r) ∧ ws(w, w')`，cat 模型的经典写法）⇒ 两种形态的**图不同**（前者含读→写边，后者含写→写边）⇒ 本 ADR 取**读 → 写**形态：它**静态**（⇒ 候选面收窄为 `co` 候选，见决策 5）、**非冗余**（不被 `ws` 吸收），且对「读读到被覆写值」这一 coherence 压力与经典形态**同样可检出**。⇒ 实施 change 须以一手锚点复核所取形态并**显式登记**。

**跨域面的归属**：`fr(r, w2)` 中 `r` 与 `w2` **恒同址**（`co` 逐地址）⇒ `fr` **不产生跨地址边**；但二者**可不同域** ⇒ `fr` **可跨域**（跨域的 coherence 压力）。`ws` = 逐地址 `co` ⇒ 同址写之间（含**跨域**写）有 `ws` 边。⇒ **跨域的承重边 = `rfe`（跨域读自）+ 跨域的 `fr` / `ws`**；**域内关系** = `ppo_com`。⚠️ **口径**：上游「域间关系在**全局内存序**中统一处理」中的「全局内存序」是**无环性判据的语义**（存在一个与全部边相容的全局全序），⛔ **不是**第四条边类 ⇒ 本 ADR 的 `ws` 取逐地址 `co`（= cat 模型的经典形态）⇒ 与上游**无违例**。

### 决策 5 —— 候选 = **同地址写序候选** + 静态图判环 + **∃/∀ 聚合语义** + 截断 ⇒ `undecided`

**① 候选空间**：候选 = **逐地址写序候选** `co`（同地址写的全序；可为空 = 无约束）⇒ 组合空间 = 各地址候选的**笛卡尔积**。⛔ **不再枚举全局内存序**（`fr`/`ws` 已静态，见决策 4 ⇒ 图不再随全局序变化）。

**② 逐候选判定**：对每个 `co` 候选组合，按决策 4 构造**静态图** `G_ax` ⇒ 该候选「**可行**」（无环）/「**成环**」（有环）。

**③ 聚合语义（⛔ 关键，不可反）**：迹的判定 = **∃ 可行候选 ⇒ `not_falsified`**（「本次语料范围内**未证伪**」，**短路**：找到一个即可停）；**∀ 候选成环 ⇒ `falsified`**（「**已证伪**」，**需穷举**；见证 = 候选枚举摘要 + **一个代表性成环候选**，含其写序候选标识如 `co_order == co1`）。

⚠️ **理由**：公理是「合规执行必须无环」⇒ **能**找到无环实现 ⇒ 该迹**不**反驳公理 ⇒ **未证伪**；只有**全部**候选都成环 ⇒ 该迹**没有**合规实现 ⇒ **证伪**。⇒ 把「某候选成环」直接当「证伪」是**假阳性**（模型其实允许另一候选）⇒ ⛔ 禁止。

**④ 截断**：候选组合总数 > 上限（`co_enumeration_cap`，默认值由实施 change 定）⇒ 若**尚未**找到可行候选 ⇒ `undecided` + 原因码 `search-truncated` + `search_truncated: true`（⛔ **不得**判 `falsified` —— 未穷举；⛔ **不得**判 `not_falsified` —— 尚有未查候选）。

**⑤ 实现口径（供实现参考）**：实现按「**co 候选 × 静态图判环**」即可，⛔ 无需构造全局 mo ⇒ 枚举面从「全局序」收窄到「同地址写序候选」。⚠️ **well-formedness 前置**：该口径要求迹满足「**读不会读到自己程序序更晚的写**」（`rf` 与 `ppo_com` 不构成自反环）；违反者（如 `ppo(r→w) ∧ rf(w→r)`）**不在** AC-10 的畸形闭集内 ⇒ 其 `not_falsified` 判定**不可**由本口径推出 ⇒ 实施 change 须把该形态**纳入畸形集**或**显式登记为限制**。

### 决策 6 —— **双图折衷**：公理面**不含**域内 `rf`；观测面**报告**域内 `rf`

| 面 | 边类 | 用途 |
|---|---|---|
| **公理面**（证伪图输入） | `ppo_com` · `rfe` · `fr` · `ws` | 决策 2 公式**逐字**四类；⛔ **不含**域内 `rf`（`rf` 不在公式里 ⇒ 域内读由**域自身模型**约束，不由复合公理约束 —— ⚠️ 该「域内一致性不被本公理检查」的缺口见 §4.2） |
| **观测面**（判定产物报告） | `rf`（域内）· `rfe`（跨域）· `fr` · `ws` · `ppo_com` | 供人/闸观察「哪些读读到哪些写」⇒ backlog AC-8 的「同域 ⇒ `{rf}` · 跨域 ⇒ `{rfe}`」落**观测面** |

⇒ ⛔ **两面不得混用**：证伪函数只接受公理面图。

### 决策 7 —— 判定面：**四态** + 原因码**闭集**

| 态 | 语义 | 触发 |
|---|---|---|
| `falsified` | **已证伪** | ∀ 候选成环（决策 5 ③） |
| `not_falsified` | **未证伪** | ∃ 可行候选（⛔ **≠**「无违例」≠「已证明」） |
| `undecided` | **不可判** | 截断（`search-truncated`）· 域标签无规则（`model-semantics-unavailable`） |
| `invalid` | **输入非法（拒收）** | ⛔ **不是**判定态：与「不可判」**必须区分**（前者输入错，后者判不出） |

**原因码闭集**（v1，每码一条反 fixture）：`derived-edge-as-input`（AC-3①）· `channel-edge-not-op-edge`（AC-11）· `unbound-domain` / `no-domain-instance`（AC-6）· `model-semantics-unavailable`（决策 3）· `search-truncated`（AC-9）· `malformed-trace` 及细分 `rf-target-missing` / `duplicate-op-id` / `self-ppo-edge` / `rf-self-loop`（AC-10）。

**术语纪律（逐字复用）**：操作 / 执行迹 / 见证（witness）/ **未证伪** / **不可判** —— 「未证伪」≠「无违例」≠「已证明」（per `ADR-CEP-13` 抓取点 5）。

### 决策 8 —— `V-13-4` 退场链路 = **(b) 显式 Out-of-Scope**；⛔ 不改既有 ADR 正文

**裁定**：本单元 v1 **不提供运行期载体**（轨迹 = **手写** litmus 迹）⇒ `ADR-CEP-13` 的 `V-13-4`（复合一致性模型的可判定性）判读**不被本单元取代** ⇒ **显式 Out-of-Scope**，指向 I-6 后续单元 ⇒ ⛔ **不得沉默**（两个单元都以为对方做 = 已发生过的失效模式）。⚠️ 若后续单元提供运行期载体 ⇒ 走**追加 SU** 声明判读被取代（**正文不动**）—— 属**实施 change**，⛔ 不在本 ADR。

### 决策 9 —— 登记与冻结纪律（三件套 + 落盘纪律）

1. **登记**：本 ADR 进 `test/cep_adr_structural_deferrals.json` 的 `decision_adrs`（闸「未登记即红」）⇒ 分母 21 → 22；
2. **冻结链**：命中 `docs/adr/ADR-CEP-*.md` glob ⇒ 台账 Entry（含**新增声明行**）+ `cep_freeze_check.sh --update --authorized-by "Entry #N"` + 清单**单独** commit + 无参复验；
3. **索引 + 编号表**：`scripts/docs/gen_adr_index.py --write`（`docs/adr/README.md` 索引行 + §1 统计表 CEP 份数 **24 → 25**）· `docs/adr/ADR-CEP-NUMBER-MAP.md` §2 决策 1 表新增 `CEP-22` 行（编号表 = 唯一真源· `openspec/specs/cep-core-contracts/spec.md:184`）⇒ NUMBER-MAP 属 M1 冻结面 ⇒ **同链**。

4. **回滚路径**：本 ADR 处于 `Proposed` 且**未走冻结链** ⇒ 回滚 = **不 ratify**（零成本）或 **追加 SU 修订**（`ADR-CEP-00 §2 Decision 3` 的「原地更正 + 标注增补块 + 台账 entry」形态）⇒ **无数据迁移 / 无 schema 变更 / 无实现回退负担**（决策 1~8 的产物全部**未落地**）。

⚠️ **落盘纪律**：本 ADR 草案在 architect 阶段**落于仓外**（`/tmp/opencode/adr/`）—— `docs/adr/ADR-CEP-*.md` 命中冻结 glob ⇒ **文件一落工作区**，`cep_freeze_check.sh`（无参）即报「新增未登记」⇒ 仓内落盘须由 **builder** 在台账 Entry 授权下与决策 9 三件套**同批**完成。

---

## 3. 选项对比表

| 选项 | 内容 | 结论 | 理由 |
|---|---|---|---|
| **A** | 域 ⟺ 协议实例（域 = 实例 `id`，零新面） | ⛔ 不采纳 | 违反 `ADR-CEP-13` 决策 1 —— 域边界由 **Ordering Point 作用域**界定，⛔ 不是协议实例边界 ⇒ 一实例可承载多域 ⇒ **静默推翻**决策 1 前半 ⇒ 须改已签批 ADR（⛔ 本单元禁改） |
| **B** | 域 = schema 独立实体（新增域字段） | ⛔ 不采纳 | 承载面只能是 `protocol_instance.schema.json`（13 属性无域字段）⇒ 触 **R-2e**（须重开 R-2 + 用户 ratify）⇒ 本单元 Out-of-Scope ⇒ 属**另一个单元** |
| **C** | 域只存在于迹数据面（域标识 + 解析到实例） | ✅ **采纳** | 同时满足 ⛔ 不触 R-2e · ⛔ 不改既有 ADR 正文 · ✅ 承载 AC-6 的「线程解析到一个域」；代价 = 域身份不在配置面（§4.2） |
| **D** | 模型来源 = 实例 `consistency_labels` | ✅ **采纳** | `ADR-CEP-12` 决策 3 **明文**把标签用途定为跨域组合判据 ⇒ 零新词表 + 8 实例已声明（SC/RC×3/PLO 在用）⇒ v1 即有非空实例集 |
| **E** | 模型来源 = 域显式声明自己的模型 | ⛔ 不采纳 | 与 C **耦合**（一实例被多域引用 ⇒ 域声明 vs 实例能力可能冲突 ⇒ 须新增裁决）⇒ scope creep；且标签已被定位为**实例**属性 ⇒ 域级声明 = 第二真源 |
| **F** | 模型来源 = 学术锚点直接给定（ADR 正文写「该域 = RC」） | ⛔ 不采纳（作运行期真源） | ADR **不可被代码读取** ⇒ 只能作**佐证**（决策 3 已把 HeteroGen §II-B / Table I 列为锚点）⇒ 运行期真源须机器可读 ⇒ 归 D |
| **G** | `fr`/`ws` 由**候选全局序**派生（v0.1 形态）· 或语料直接给出 `fr`/`ws` / 一个全局序字段 | ⛔ 不采纳 | v0.1 的 `fr(w1,w2) ⟺ ∃r: rf(w1,r) ∧ w1≺r ∧ r≺w2` **被 `ws` 完全吸收**（两端皆写 ⇒ `fr ⊆ ws`）⇒ 并集里 `fr` 是**冗余边**、公理实际只跑三类 ⇒ 且方向非 cat 标准；语料直接给出则是**让语料断言结论** ⇒ 判定退化为「读语料」⇒ 假绿（AC-3①）；给全局序字段则引入**第三个未定义概念**（谁定全局序？⇒ 与公理同深度 ⇒ 循环）并使「写序非唯一」这一真实难点消失 ⇒ 判据失去鉴别力 ⇒ 归决策 4 的**静态**形态 |
| **H** | 候选 **co**（同地址写序）+ **静态图**判环 + ∃/∀ 聚合 | ✅ **采纳** | 与 litmus 语义一致（模型允许该迹 ⟺ **∃** 合规实现）⇒ 证伪 = 无合规实现（∀ 成环）⇒ 结构性排除「某候选成环即证伪」的假阳性；且因 `fr`/`ws` 静态 ⇒ 枚举面从「全局序」收窄到「同地址写序候选」⇒ 候选爆炸面大幅收窄（代价：候选组合 = 逐地址候选的笛卡尔积 ⇒ 仍需上限 + 截断兜底） |
| **I** | 公理面**加**域内 `rf` | ⛔ 不采纳 | 决策 2 的公式**逐字**是 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` ⇒ 加边 = 改公理 = 改已签批 ADR（⛔ 禁）⇒ 归决策 6 的双图折衷（观测面可报告，**公理面不可加**） |
| **J** | 只验域内（退回「通道图 + 域内序」） | ⛔ 不采纳 | 公理的**跨域部分**（`rfe`）恰是它相对「所有域模型相同」的**增量** ⇒ 退回域内 = 退回**过严**假设 ⇒ 与决策 2 的动机相悖；且 AC-2 是**否定式**判据 ⇒ 无正向鉴别力 |
| **K** | 域身份放 schema 但**复用**既有属性（如 `family`） | ⛔ 不采纳 | `family` = **来源族**（`industrial|ruby-slicc|academic`）⇒ 语义不同源；复用 = 语义污染 ⇒ 同样触 R-2e（改取值域）⇒ 归 C |

---

## 4. 后果 / 代价

### 4.1 正面

- **`INV-CEP-13-IC1` 从「声明」走向「可判」**：G-a/G-b/G-c 三层缺口各有落点 ⇒ 后续 change 可逐条自证；
- **零新词表、零 schema 改动** ⇒ ⛔ 不触 R-2e ⇒ 冻结面**仅新增本 ADR 一件**（+ NUMBER-MAP 行）；
- **假阳性被结构性排除**：∃/∀ 聚合语义 + 派生边纪律 + 原因码闭集 ⇒ 「已证伪」= 真的没有合规实现；**枚举面收窄**：`fr`/`ws` 静态 ⇒ 只枚举**同地址写序候选**（决策 5 ⑤）；**诚实边界不被稀释**：三态 + `invalid` 分离 + 「未证伪 ≠ 无违例 ≠ 已证明」逐字在场；
- **审计链完整**：决策 9 三件套把新 ADR 纳入结构闸 / 冻结链 / 索引闸 ⇒ 不产生「文档在仓外、闸看不见」的僵尸。

### 4.2 负面（代价）—— 含**两条必须登记的缺口**

- **⚠️ 缺口①：域内一致性（值面 / SWMR / reads-see-latest）不由本公理检查** —— 反例（单 SC 域迹：`w1,w2(X); r` 读 `w1`，另一线程 `r2` 读 `w2` 且 `r2→r` 程序序）：SC 的违例环**依赖域内 `rf` 边**闭环，而 `rf` 不在公理并集里 ⇒ 复合公理**查不出** ⇒ ⇒ **域内一致性是各域自身的验证义务**，⛔ 不在本 ADR 的保证范围内（决策 6 的双图折衷**不掩盖**该缺口）；
- **⚠️ 缺口②：「迹域标识忠实代表 Ordering Point 作用域」是**迹作者承担的假设** —— 域不进 schema ⇒ 配置面**无任何机器判据**可校验 ⇒ 该假设失效 ⇒ `rfe` 判定失真（域内跨域误判）；
- **域身份不在配置面**（决策 1）⇒ 域划分**只存在于迹里** ⇒ ⛔ 无法在 config 层做域拓扑检查；**域模型 = 能力面，非运行期行为**（决策 2）⇒ 与「实现真的按该模型运行」之间**仍有缺口**（运行期采集另立 backlog）；
- **RC 家族规则「仓内不可复核」**（决策 3）⇒ 下游 change 须以可复核锚点复核或**显式标注** ⇒ **已登记的**证据强度缺口（⛔ 不是遗漏）；⚠️ `TSO` 亦**不可复核**且**无实例** ⇒ v1 不登记 ⇒ 相关域一律 `undecided`；
- **`fr` 的约定依赖**（决策 4）⇒ 读→写 / 写→写两形态的图不同 ⇒ 实施 change 须以一手锚点复核并**显式登记**所取形态；
- **枚举爆炸（残余）**（决策 5）⇒ 候选组合 = 逐地址候选的**笛卡尔积** ⇒ 必须有上限 ⇒ 触上限即 `undecided` ⇒ **大迹可能判不出**；
- **双图折衷需持续纪律**（决策 6）⇒ 两套边类集合 ⇒ 实现混用 ⇒ 假绿/假红 ⇒ 须单测钉死（V-22-5）。

### 4.3 风险

| # | 风险 | 缓解 |
|:--:|---|---|
| R-1 | **聚合语义被实现反**（把「某候选成环」当证伪）⇒ 假阳性 | 决策 5 ③ + 决策 7 闭集 + V-22-3 反向 fixture |
| R-2 | **公理面混入域内 `rf`** ⇒ 公式被静默改写 ⇒ 假绿 | 决策 6 + 单测钉死边类集合（V-22-5） |
| R-3 | **RC 规则不可复核** ⇒ 结论不可审计 | 决策 3 两档标注 + 下游 change 复核义务（V-22-2） |
| R-4 | **`undecided` 被降级为「未证伪」** ⇒ 假绿 | 决策 7（`undecided` / `invalid` 分离）+ 闭集 + V-22-4 |
| R-5 | **手写迹与实际实现脱节** ⇒ 判定与仿真无关 | v1 定位 = 「按声明模型判」⇒ 诚实边界登记（决策 2）+ 运行期采集另立 backlog |
| R-6 | **`fr` 约定被下游改写** ⇒ 判定口径漂移 | 决策 4 的约定依赖段 + 实施 change 的复核义务 |
| R-7 | **backlog AC-7 / AC-8 字面不可满足**（§5.1）⇒ 实施 change 照抄 ⇒ 判据自相矛盾 | §5.1 登记 + **须 planner 修订**（本 ADR ⛔ 不代改 · planner 写集） |

---

## 5. 边界与抓取点（新增 invariant 数 = 0）

### 5.1 两处 backlog AC 字面缺陷（登记 + 路由 planner 修订；⛔ 本 ADR 不代改）

| # | 缺陷 | 正确形态（本裁定） |
|:--:|---|---|
| **D-1** | **AC-7** 写「三写迹 ⇒ `co1` 成环 / `co2` 不成环 ⇒ 判定分别为 `falsified=true` / `falsified=false`」—— 若二者是**同一条迹**的两个写序候选，按 ∃/∀ 聚合语义该迹判定应为 **`not_falsified`**（`co2` 可行 ⇒ 模型允许该迹）⇒ 字面**与正确语义冲突** | 改为**逐候选判定 + 聚合语义**（决策 5 ②③）：`co1` 成环 ⇒ 该候选「成环」；`co2` 可行 ⇒ 该迹 `not_falsified`；`falsified` 需**全部**候选成环。「见证含 `co_order == co1`」的表述**保留**（作成环候选标识） |
| **D-2** | **AC-8** 写「跨域边类集合**精确等于** schema `edge_classes` 枚举」—— 枚举是**全集**，任何具体迹的边类集合只能是**子集** ⇒ 字面**不可满足** | 改为：**① 迹中每条边的类型 ∈ schema `edge_classes` 枚举**（⛔ 运行时从 `protocol_instance.schema.json` 读，禁写字面量）**② 跨域读自边类型 == `rfe`**，域内 == `rf`（决策 6 观测面） |

### 5.2 抓取点

| # | 抓取点 | 载体 | 状态 |
|:--:|---|---|---|
| 1 | 域标识 → 实例 → `consistency_labels` 解析链（决策 1/2） | 实施 change 的解析器 + 单测 | ⭭ 待落地 |
| 2 | `ppo_i` 逐标签授权表（决策 3） | 本 ADR §2 决策 3 | ⚠️ **文档层**（RC/TSO **仓内不可复核**；PLO/SC 可复核）⇒ 人工评审 |
| 3 | `rfe`/`fr`/`ws` 派生规则 + `fr` 约定依赖（决策 4） | 本 ADR §2 决策 4 | ⚠️ **文档层** ⇒ 人工评审 + 实施 change 的锚点复核 |
| 4 | ∃/∀ 聚合 + 截断 ⇒ `undecided`（决策 5） | 本 ADR §2 决策 5 + 实施 change | ⚠️ 文档层 · ⭭ 机器面待落地 |
| 5 | 公理面 / 观测面边类分离（决策 6）· 原因码闭集 + 四态（决策 7） | 本 ADR §2 决策 6/7 + 单测 | ⭭ 待落地 |
| 6 | **缺口①（域内一致性不被本公理检查）+ 缺口②（域标识忠实性假设）** | 本 ADR §4.2 | ✅ 文档层（本 ADR 即载体） |
| 7 | `V-13-4` 的显式 Out-of-Scope（决策 8） | 本 ADR §2 决策 8 | ✅ 文档层（本 ADR 即载体） |
| 8 | 登记三件套（决策 9） | 决策 9 三行 | ⭭ 待落地（builder 阶段） |

⇒ ⛔ **新增 invariant 数 = 0** ⇒ 本 ADR 归 `scripts/test/cep_docs_sync_check.sh` 的 `NO_INV` 白名单（⛔ 禁新造平行 id 体系；Verification 判据一律**复用** `INV-CEP-13-IC1`）。

---

## 6. 关联 ADR / 后续

- `ADR-CEP-13` —— 决策 1（域定义）· 决策 2（**本 ADR 的对象**）· 决策 5 ②（不提供无违例保证）· 抓取点 5（术语）；⛔ 本 ADR **不改**其正文；
- `ADR-CEP-12` —— 决策 3（标签的**用途** = 跨域组合判据）· 决策 4（请求级标签）· R-2e（⛔ 不改字段类型/取值域，per I-6 裁决记录 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §五（R-2e 本体所在节））；其 `:1163` 的旧语料路径悬空**已在**台账 Entry #540 登记，⛔ 本 ADR 不处理；
- `ADR-CEP-08` 决策 1（Ordering Point = 域的界定点）· `ADR-CEP-20` 决策 6（能力阶梯 + 档位声明义务 ⇒ 本 ADR 的判定产物**须**带档位限定，⛔ 不得跨档比较强度）· `ADR-CEP-17` 决策 7（5 语义 + 标签硬前置）· `ADR-CEP-02`（canonical 10 流）；

- **后续**：① `backlog: cep-memory-model-operational-trace` —— 本 ADR 是其**开工前置**（立项后回填 change 指针 + AC 映射）· ② **运行期轨迹采集** —— 另立 backlog（决策 2）· ③ **`TSO` 规则表** —— 待有实例时登记（决策 3；候选规则 = enforced = po 去掉 W→R）· ④ **backlog AC-7 / AC-8 修订** —— planner 域（§5.1）。

---

## 7. 决策汇总

| 决策 | 结论 |
|---|---|
| 1 | 域身份 = **迹侧域标识**（解析到实例 `id`）· ⛔ 不进 schema；⚠️ 缺口②：域标识的**忠实性假设**由迹作者承担 |
| 2 | 模型来源 = **实例的 `consistency_labels`**（`read`/`write` 二元组）· ⚠️ **能力面**，非运行期行为 |
| 3 | `ppo_i` = 读侧/写侧授权表的**并集**；v1 覆盖 **SC / RC-acquire / RC-release / RC-data / PLO**（**RC-data = ∅**）；**TSO 不登记** ⇒ `undecided`；RC 家族标注**仓内不可复核** |
| 4 | `rfe`/`fr`/`ws` **全部派生**（⛔ 不得作输入）· `ws` = **逐地址** `co` · `fr` = **静态读→写**形态（⚠️ 约定依赖，须锚点复核）· 域间承重边 = `rfe` |
| 5 | 候选 = **同地址写序候选**（⛔ 不枚举全局序）+ **静态图**判环 + **∃/∀ 聚合语义**（∃ 可行 ⇒ 未证伪；∀ 成环 ⇒ 证伪）+ 截断 ⇒ `undecided` |
| 6 | **双图折衷**：公理面**不含**域内 `rf`；观测面**报告**域内 `rf`；⚠️ **缺口①：域内一致性不由本公理检查** |
| 7 | 判定面**四态**（`falsified`/`not_falsified`/`undecided`/`invalid`）+ 原因码**闭集**（10 码） |
| 8 | `V-13-4` 退场 = **(b) 显式 Out-of-Scope**；⛔ 不改既有 ADR 正文 |
| 9 | 登记三件套（`decision_adrs` + 五步链 + 索引/编号表）+ 落盘纪律（仓外起草 ⇒ builder 落盘） |

---

## Verification（前瞻判据 · 可勾选 —— **不是**已跑完的实测日志）

> **口径**：本章是**签发前置**。每条给**载体** + **反向 fixture（毒化 ⇒ 必红 + 归因 id）**；归因 id **一律复用**既有体系（`INV-CEP-13-IC1`）⇒ ⛔ 零新造。**载体状态**逐条标注：✅ 可执行 / ⚠️ **文档层**（人工评审）/ ⭭ 待落地。⚠️ **文档层判据 ⛔ 禁**当作鉴别力判据（per `ADR-CEP-20` V-20-1 同款纪律）。

| # | 判据 | 载体 | 反向 fixture（毒化 ⇒ 必红） | 归因 id | AC 映射（backlog `cep-memory-model-operational-trace`） |
|---|---|---|---|---|---|
| **V-22-1** | 「哪条公理依赖哪个域模型语义」**依赖表**齐备（公理子句 → 域模型语义 → 依赖落点），⛔ 不得只写结论 | 本 ADR §2 决策 2/3/4 | ⚠️ **文档层** ⇒ 人工评审（architect/oracle 签核）；反向 fixture **无** ⇒ ⛔ 禁当鉴别力判据 | `INV-CEP-13-IC1` | **AC-1** |
| **V-22-2** | `ppo_i` 逐标签授权表**带溯源两档标注**；⛔ 无锚点的标签**未**登记（TSO） | 本 ADR §2 决策 3 | ⚠️ **文档层**（RC/TSO **仓内不可复核**）⇒ 人工评审 + 下游 change 复核义务 | `INV-CEP-13-IC1` | **AC-1** / AC-5（人工部分） |
| **V-22-3** | 聚合语义 = **∃ 可行 ⇒ 未证伪 / ∀ 成环 ⇒ 证伪**；⛔ 不得把「某候选成环」当证伪 | 本 ADR §2 决策 5 ③ | ⭭ **待落地** ⇒ 反向 fixture：一条「co1 成环 + co2 可行」的迹 ⇒ 判定**必须** `not_falsified`（毒化：改成「任一成环即证伪」⇒ 必红） | `INV-CEP-13-IC1` | **AC-7**（含 §5.1 D-1 修订） |
| **V-22-4** | 四态 + 原因码闭集；`undecided` 与 `invalid` **分离**；⛔ `undecided` **不得**降级为 `not_falsified` | 本 ADR §2 决策 7 | ⭭ **待落地** ⇒ 反向 fixture：每码一条（10 码）⇒ 毒化：把 `undecided` 判成 `not_falsified` ⇒ 必红 | `INV-CEP-13-IC1` | **AC-3①** / **AC-6** / **AC-9** / **AC-10** / **AC-11** |
| **V-22-5** | 公理面边类集合 == `{ppo_com, rfe, fr, ws}`（⛔ 不含域内 `rf`）；观测面**报告**域内 `rf` | 本 ADR §2 决策 6 | ⭭ **待落地** ⇒ 反向 fixture：往公理面塞一条域内 `rf` ⇒ 必红 | `INV-CEP-13-IC1` | **AC-8**（含 §5.1 D-2 修订） |
| **V-22-6** | 域标签解析链可机判：域标识 → 实例 → `consistency_labels`；未绑定 / 解析不到 ⇒ 拒收 | 本 ADR §2 决策 1 | ⭭ **待落地** ⇒ 反向 fixture：① 线程无域字段 ⇒ `invalid` + `unbound-domain` ② 域标识写错 ⇒ `invalid` + `no-domain-instance` | `INV-CEP-13-IC1` | **AC-6** |
| **V-22-7** | **金标准违例 case**（跨域 `rfe` 拉环）：域 A `r1(X)`→`w2(Y)` · 域 B `r2(Y)`→`w1(X)`，`rf(r1←w1)`、`rf(r2←w2)` ⇒ 环 `w1→r1→w2→r2→w1`（**在 `ppo_com ∪ rfe` 内 ⇒ 与 `co` 候选无关**）⇒ 判定 `falsified` 且给出见证 | 本 ADR §2 决策 4/5 | ⭭ **待落地** ⇒ 正向测例的毒化 = **关掉 `rfe` 派生** ⇒ 环断 ⇒ 判定**必须变** `not_falsified` ⇒ 证明该 case 真正依赖跨域边（⛔ 不可用「关掉 `fr`」作毒化 —— 该环不依赖 `fr`） | `INV-CEP-13-IC1` | **AC-4**（正向部分） |
| **V-22-8** | `V-13-4` 的 (b) 退场选择**在场**（⛔ 不得沉默） | 本 ADR §2 决策 8 | ✅ **可执行**（本 ADR 即载体）⇒ 反向 fixture：grep 本 ADR 须命中 `V-13-4` 与所选标签 `(b)` 共现 | `INV-CEP-13-IC1` | **AC-15** |
| **V-22-9** | 本 ADR **零 diff** 既有判据面（`protocol_instance.schema.json` · `protocol_instances.json` · `combination_contract` · 既有 `docs/adr/ADR-CEP-*.md` 正文） | `cep_freeze_check.sh` + `docs_sync_check.sh` | ✅ **可执行** ⇒ 毒化：改其一 ⇒ 必红 | `INV-CEP-12-IC1`（实例声明契约 owner `ADR-CEP-12`） | **AC-13③** |
| **V-22-10** | 登记三件套齐备（`decision_adrs` 含本 ADR · 冻结链跑通 · 索引/编号表含 `CEP-22`） | 决策 9 三行 | ⭭ **待落地**（builder 阶段）⇒ 反向 fixture：把本 ADR 从 `decision_adrs` 移除 ⇒ `cep_adr_structural_check.sh` 必红（未登记即红） | `INV-CEP-13-IC1` | **AC-13** |
| **V-22-11** | **缺口① / 缺口② 显式在场**（域内一致性不被本公理检查 · 域标识忠实性假设由迹作者承担） | 本 ADR §4.2 | ✅ **可执行**（本 ADR 即载体）⇒ 反向 fixture：grep 本 ADR 须命中两条缺口的措辞 | `INV-CEP-13-IC1` | **AC-1**（人工部分） |

**签发前置自检**：① 本章位于**正文**（首个 `## Status Update` 之前）✅ ② 每条判据均有**载体 + 归因 id + 反向 fixture**（文档层 / 待落地项已**逐条如实标注**「无载体 / 禁当鉴别力判据」）✅ ③ 归因 id **全部复用**既有体系（⛔ 零新造）✅ ④ 新 ADR 登记进 `decision_adrs` ⭭ **待办**（决策 9 第 1 项，须与**落盘同批**完成）⑤ ⛔ 未签发前**不得**进入「已签发」状态、**不得**走冻结面授权链、⛔ 不得据本 ADR 立项 G2 的实施。

---

## 8. 修订记录

| 版本 | 日期 | 变更 | 依据 |
|---|---|---|---|
| v0.3 | 2026-11-24 | 吸收 **oracle 复审 APPROVE**（8/8 高影响 + 4/4 中低全闭 · 无新 BLOCKER）与 **critic APPROVE_WITH_CONDITIONS**（0 BLOCKER · `mergeable: yes`）的 3 SHOULD_FIX + 4 NITS：① **修 v0.1 残留**：决策 4「跨域面归属」段的「`fr` 跨地址部分」**误写**（`co` 逐地址 ⇒ `r` 与 `w2` 恒同址 ⇒ 无跨地址 `fr`；跨域性来自**域**不同而非地址不同）并补「全局内存序 = 无环性判据的语义、⛔ 非第四条边类」口径 ② 元数据状态行更正（oracle 复审已完成）③ 决策 9 补**显式回滚路径** ④ 选项表字母补 `H`（原 A~G 后跳 I）⑤ 决策 5 ⑤ 由「⟺ 等价」弱化为**实现口径**并加 **well-formedness 前置**（`rf` 与 `ppo_com` 构成自反环的病态迹不在畸形闭集内） | oracle 复审 + critic 首审 |
| v0.2 | 2026-11-24 | 吸收 **oracle 首审 REJECT** 的 8 项：① **事实修正** —— `ualink_4` = PLO/PLO（现役实例）⇒ 事实陈述由「TSO/PLO 无实例」改为「TSO 0 实例、PLO 1 实例」；② **PLO 登记**（`Allow_write(PLO)` = R→W ∪ W→W，锚点 `protocol-engine.md:104` 仓内可复核）；③ **RC-data 改为 ∅**（relaxed 不保留任何 po；v0.1 的 `po_loc` 属过严 ⇒ 假阳性）；④ **`fr` 改静态读→写形态**（v0.1 的 ≺-派生形态被 `ws` 完全吸收 ⇒ 冗余）；⑤ **`ws` 改逐地址 `co`**（跨地址写序永不改变判定 ⇒ 无意义且误导）；⑥ 决策 5 候选面收窄为**同地址写序候选** + 静态图判环 + 等价性注记；⑦ **补登缺口①**（域内一致性不被本公理检查，附反例）与**缺口②**（域标识忠实性假设由迹作者承担）；⑧ **V-22-7 换真环迹**（跨域 `rfe` 拉环；毒化改为「关掉 `rfe`」）+ 修判定态名拼写（漏 `i`） + R-2e 指针改引 I-6 裁决记录 | oracle 首审 |
| v0.1 | 2026-11-24 | 初稿（🟡 Proposed）：决策 1~9 + 选项 A~L + §5.1 两处 backlog AC 字面缺陷登记 | 用户口令「同意你的建议，请继续推进」· 上游 = 台账 Entry #540 后的 G2 开工前置（`backlog: cep-memory-model-operational-trace`）· metis 1 轮（gap 分析）· oracle 1 轮 **REJECT** |

> ⛔ **本 ADR 未签发**：oracle + critic 双审完成且 BLOCKER 全闭 + **用户 ratify** 前，⛔ 不得进入「已签发」状态、⛔ 不得走冻结面授权链、⛔ 不得据本 ADR 立项 G2 的实施。

---

## Status Update 2026-11-24（per 台账 **Entry #541**）—— **签发**（v1.0）

> **签发依据（三者齐备，缺一不可）**：
> ① **oracle 首审 REJECT**（4 项高影响 + 4 项中低影响）⇒ v0.2 逐条闭合 ⇒ **oracle 复审 APPROVE**（8/8 + 4/4 全闭，**无新 BLOCKER**）；
> ② **critic 1 轮 APPROVE_WITH_CONDITIONS**（`review_profile: arch` · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER** · 3 SHOULD_FIX + 4 NITS）⇒ v0.3 全部修；
> ③ **用户 ratify「A」**（2026-11-24）⇒ 授权签发 + 仓内落盘 + 后续立项。
>
> **审稿报告**：` .rddf/reviews/ADR-CEP-22-memory-model-source-and-cross-domain-edges-review.md`（判定行原文 + 逐项闭合证据 + 「ADR 完成的标志」5 条核对）。
>
> **本轮修正中登记的实质缺陷（如实留档，⛔ 正文不改）**：
> - **oracle 首审**：① 我原称「TSO/PLO 本仓无实例」**是错的** —— `ualink_4` = **PLO/PLO**（`status: implemented`）⇒ PLO 改为 v1 登记；② 我原写的 `fr` 规则**是 vacuous 的**（两端皆写 ⇒ 被 `ws` 完全吸收 ⇒ 公理实际只跑三类边）⇒ 改**静态读→写**形态；③ `RC-data` 原写 `po_loc` 属**过严 ⇒ 假阳性** ⇒ 改 **∅**；④ v0.1 的 `fr` 由**候选全局序**派生 ⇒ 候选面收窄为**同地址写序候选** + 静态图判环；⑤ **须补登缺口①/②**；⑥ V-22-7 的金标准迹**不可达** ⇒ 换**跨域 `rfe` 拉环**真环迹。
> - **critic**：⑦ 决策 4「跨域面归属」段残留 v0.1 的「`fr` **跨地址**部分」—— `co` 逐地址 ⇒ `r` 与 `w2` **恒同址**；跨域性来自**域**不同而非地址不同 ⇒ 已改写并补「全局内存序 = 无环性判据的语义、⛔ 非第四条边类」口径；⑧ 元数据「复审未完成」与事实不符 ⇒ 已更正；⑨ 补**显式回滚路径**；⑩ 选项表字母补 `H`；⑪ 决策 5 ⑤ 由「⟺ 等价」弱化为**实现口径** + 加 **well-formedness 前置**。
>
> **签发后的边界（逐字）**：
> - ⛔ 本 ADR **只**裁定「每域模型来源 + 域间边构造规则」；**不**改任何已签批 ADR 正文 · **不**改 `protocol_instance.schema.json` / `protocol_instances.json` · **不**新增 invariant（`INV-CEP-13-IC1` 复用）· **不**产出任何既有 ADR 的替代结论。
> - ⛔ `RC` 家族规则**仓内不可复核**（论文原文不在本仓）· `TSO` **不登记**（0 实例）⇒ 相关域一律 `undecided` + `model-semantics-unavailable`；`fr` 的**约定依赖**须由实施 change 以一手锚点复核并**显式登记**。
> - ⚠️ **缺口①**：域内一致性（值面 / SWMR / reads-see-latest）**不由**本公理检查 ⇒ 属各域自身的验证义务；**缺口②**：「迹域标识忠实代表 Ordering Point 作用域」是**迹作者承担的假设**（域不进 schema ⇒ 配置面无机器判据）。
> - ⛔ 判定面**只能**产出「证伪 / 未证伪 / 不可判 / 拒收」；⛔ **不得**产出「已证明」；「未证伪」⛔ **≠**「无违例」。
> - ⛔ 本 ADR **未立项** G2 的实施（关联 = `backlog: cep-memory-model-operational-trace`）⇒ 立项与 AC 编号由 planner 阶段补，之后回填本 ADR 的「关联 change」与「AC 映射」。

> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md` Entry **#541**（落盘授权 · 用户 ratify）· 审稿轨迹见 Entry #541 ②。
