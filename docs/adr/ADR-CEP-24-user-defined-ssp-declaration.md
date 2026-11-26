# ADR-CEP-24: 用户自定义稳定态协议（SSP）声明面与自定义接入路径

> **状态**: 🟢 **Proposed** ✅ **已签发**（v1.0 · 2026-11-26 —— **metis 1 轮已处置** + **oracle 1 轮 `APPROVE`**（3 项 must-fix 全落）+ **critic 1 轮 `APPROVE_WITH_CONDITIONS`**（`mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER** · 2 SHOULD_FIX + 4 NIT 全处置）双审闭合；⛔ **签发 ≠ 可实施** —— 决策 3 两维度未定义 ⇒ S1 待 §8 F5 补齐；详见文末 **Status Update**）
> **日期**: 2026-11-26 · **作者**: CppTLM build agent（architect mode）
> **作用域**: 「**用户自定义稳定态协议（SSP）**」的 ①**表达面**（数据声明 + 通用解释器）· ②**能力边界** · ③**归口** · ④**手写接入路径**
> ⛔ **本 ADR 不含**: 「从 SSP **生成**瞬态态 + 在飞事务」⇒ **ADR-CEP-26**（另立 + `ADR-CEP-18` 追加 SU 声明取代范围）；「协议语义层 hop / 时序 + 引擎 egress 接 fabric」⇒ **ADR-CEP-25**（另立）
> **父决策**: **无**（新决策；⛔ 不取代任何既有 ADR）
> **关联真源**: `docs/architecture/07-coherence-exploration-platform.md` §5 · `ADR-CEP-00` 决策 1 / 4 + SU 2026-11-11 / 12 · `-02`（10 流 + perm 锁）· `-05`（`INV-CEP-05-IC1`）· `-07` 决策 3 · `-09`（`INV-CEP-09-IC1`）· `-12` · `-13` 决策 5/6 · `-17` 决策 3/5/6 · `-18` 决策 1/4/5/6 · `-20` 决策 5/6 · `-23` 决策 2
> **一手锚点**（实测，逐 `file:line` 核实）：
> - **SSP 表达面已在仓内，但逐协议手写**：`include/tlm/cep/mesif_state_spec.hh:80-145`（`kStateCount=5` / `kEventCount=6` / `kPermCount=4` · `transition[][]`（`0xFF` = 空洞）· 逐格 `Basis{kind,origin_kind,pending,ref,note}` · 5→4 `perm` 投影 + 逐格依据 · `Deviation` 偏离清单 · `:142-143`「**数据只存在这一处** ⇒ 无双份数据」）；同型 **6** 份：`mesif_state_spec.hh`(937 行) · `tilelink_state_spec.hh`(945 行 · 3 态/4 事件) · `cxl_state_spec.hh`(682 行 · 4 态) · `cxl_dcoh_state_spec.hh`(516 行) · `nvlink_sharp_state_spec.hh`(673 行) · `ualink_inc_state_spec.hh`(822 行)
> - ⭐ **事件词表是「逐协议」的，仓内无跨协议 canonical 事件词表**：`cxl_state_spec.hh:69`（事件词表段）· `mesif_protocol_engine.hh:189`（「规约的 **6 事件词表**(Rd/RdUnique/Wr/Upgrade/SnpShared/SnpUnique)没有『降级为 S』的…」）⇒ **`ADR-CEP-02` 的 canonical 10 流是「流」不是「事件」**（`flow_support` = 10 流覆盖矩阵，⛔ 非引擎事件字母表）
> - ⭐ **`source_registry.hh` 三档 `RefKind` 对「用户自定义」无对应档**：`:28-32`（`SpecClause` / `PublicNonnormative` / `RepoDerived`）+ `:97-100`（未登记 id ⇒ `registered=false` ⇒ **fail-closed**）⇒ 用户自创协议**无来源可引**；**实例声明面**：`protocol_instance.schema.json` —— `properties` **14** = **13 必含**（`required`）+ **1 可选**（`combination_contract`），⚠️ `test/test_cep_schema.cc:443`（`REQUIRE(props.size() == 14)`）+ `:480`（**机器锁定** `ADR-CEP-12` 含「13 个顶层属性」）⇒ **「13」是 `required` 口径**；`family` 枚举 = `protocol_family.hh:43-47` 的 **3** 值
> - **「新声明面不进 schema」先例**：`engine_capabilities.hh` + `hop_timing_capabilities.hh` —— 均为 C++ constexpr 声明表 + 独立判据 + 反向存在性闸，⛔ 均未扩实例面 13 键
> - **归口**：`ADR-CEP-00:70-94`（决策 4 gem5 Ruby 豁免）· `:32-40`（决策 1 代码归口 + 设计权威分层）· ⭐ `ADR-CEP-20:88`（「仅当需要**新协议语义的设计**(而非实现既有设计)时,才按 `ADR-CEP-00` 的单向门取 ArchForge 的设计权威」）· **反向先例三条**：Entry #543 归档裁决「协议 FSM 语义设计归 ArchForge」· gap 台账 G3 协议侧归 ArchForge · `ADR-CEP-20` 决策 5 单向门
> - **缺口台账**：`.rddf/roadmap/cep-gen-family-gap-closure.md` §2.2 F1~F8（**瞬态态合计 = 1** · 6/8 零发射 · 3/8 无引擎）· §2.3 G1~G10 · §5.1 T1~T4 复评（结论「全未达」）
> - **约束源**：`ADR-CEP-02:62`（`CEN-Ib11` `perm` **4 值锁**）· `ADR-CEP-05:164`（`INV-CEP-05-IC1` Stub **只暴露稳定态**）· `ADR-CEP-09:125`（`INV-CEP-09-IC1` 装配期无环为**必过项**）· `ADR-CEP-17:112-118`（决策 5 **HTM 排除**）· `:99`（决策 3 token / HTM 私有状态归插件）· `ADR-CEP-18:86-101`（决策 4 演进位「SSP 描述 → 插件生成」+ *Gen 族层 ↔ CEP 对应物*：SSC → `ADR-CEP-14` ✅ · **MSD 消息停滞检测 → 暂无对应**（唯一缺件）· 最小化 → `ADR-CEP-12` ✅）· `:103-114`（决策 5 T1~T4）· `ADR-CEP-23:43`（决策 2 不承载 belief 模型）· `:117-131`（Verification 章形态 + 「归因 id 零新造」先例）
> - **治理面**：`ADR-CEP-00` SU 2026-11-11（Entry #338）「一决策一文档化」残余 + **3 条可判定触发条件** · SU 2026-11-12（Entry #363）**决策块计数口径归一** + R-c/R-d（快照 `test/cep_decision_block_snapshot.json` = **26 份 / 145 块**）· `ADR-CEP-NUMBER-MAP:123-147`（KPI 预算 **10 / 14 / 14**）· `docs/guide/cep-usage.md:144-170`（§12.6 五步授权链）· `include/tlm/cep/AGENTS.md:41/42/45`（铁律 2 / 3 / 6）
> **审稿报告**: `.rddf/reviews/ADR-CEP-24-user-defined-ssp-declaration-review.md`（metis §1 · oracle §2 · critic §3 · 收口清单 §4）—— **metis 1 轮**（✅ 18 条 findings 逐条处置）· **oracle 1 轮**（✅ 判定 `APPROVE`，3 项 must-fix 全落）· **critic 1 轮**（✅ 判定 `APPROVE_WITH_CONDITIONS`，`mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**，2 SHOULD_FIX + 4 NIT 全处置）
> **关联 change**: `backlog: cep-user-defined-ssp-declaration`（**尚未立项** ⇒ AC 编号以 backlog 草稿的 `AC-N` 为准；立项后**回填** change 指针与 AC 映射）

---

## 1. 背景与问题

### 1.1 问题陈述

CEP **不能**让用户自定义一致性协议 —— 不是「没有协议引擎机制」（`ProtocolEngineBase<N>` + `ModuleFactory` 注册机制俱全，`ADR-CEP-07` 决策 3），也不是「没有稳定态协议表达」（仓内有 **6** 份可执行状态机），而是**两者之间没有可承载「用户自己写的协议」的那一层**：

| 现状 | 事实 | 后果 |
|---|---|---|
| 稳定态表达 | 6 份手写 C++ 头，各自定义 `State`/`Event` | 用户要自定义 ⇒ 必须写 C++ ⇒ 门槛 = 编译期 |
| **事件词表** | **逐协议**定义（`cxl_state_spec.hh:69` / `mesif_protocol_engine.hh:189`）⇒ 无跨协议 canonical 词表 | 数据面**缺一个确定的消费集合** ⇒ ⛔ 不可实施（决策 3.1）|
| 实例声明面 | 13 必含 + 1 可选（M1 冻结面）· `family` 仅 3 值 | 内嵌 SSP ⇒ 破契约 + 用户协议无处归族 |
| **来源登记** + 协议中立 | `RefKind` 三档 · 未登记 id **fail-closed**；通用层禁协议名（`INV-CEP-07A-IC2`）| 用户自创协议**无来源可引** ⇒ `Basis.ref` 无处登记；SSP 面须落在「数据值」例外内 |

⇒ **问题**：缺一个**数据驱动**的稳定态表达面，使「用户自定义协议」从「写 C++ 插件」降为「填声明 + 写适配器」，且**不新增实例面契约**。

### 1.2 与 `ADR-CEP-18` 的区别（常被混为一谈）+ 三段路线的拆分

决策 1 排除的是**生成器**（DSL / 解析器 / IR / 生成器），**未排除「手写路径的数据化」** ⇒ 本 ADR 描述协议**怎么被描述**，`ADR-CEP-18` 管协议**怎么被生成** ⇒ 本 ADR **不动** `ADR-CEP-18` 决策 1 / 6（逐条对照见决策 1 子项 1.3）。
**依赖链**（依赖单向 + 决策各自独立）：① SSP 声明面（本 ADR）──► ② 生成器（ADR-CEP-26 · 撞 CEP-18）──► ③ 时序 + egress 接 fabric（ADR-CEP-25）
⚠️ **②③ 之间的硬顺序**：瞬态态时长 = 等对端响应 = **链路往返时延 + 对端处理时间** ⇒ ⛔ 不建模时延 ⇒ 瞬态态零时长 ⇒ 在飞事务退化为计数器 ⇒ **③ 的时序语义是 ② 仿真价值成立的前置**（但 ② 的**算法**可先做）⇒ 交付顺序 ≠ 依赖顺序。

### 1.3 与既有裁决 / 缺口台账的对齐（**必读** —— 不写此节即与台账归属列静默打架）

| 既有件 | 关系 | 本 ADR 处置 |
|---|---|---|
| gap **G1**（协议 FSM 承载能力现状面）| ✅ 已闭（`engine_capabilities.hh`）| 不重做；但 S1 产出**可执行语义面**（⛔ 非现状登记表）—— 决策 1.2 |
| gap **G3**（分层协议组合 = 零）| ⛔ 归属列明写「**跨仓 ArchForge**」| **不触及** |
| gap **G5** / **G10** + **Entry #543 归档裁决**（hop/时序 Out-of-Scope）| G5 ⛔ 论文侧亦未解决 ⇒ **抄不到**；G10 ✅ 本仓可做部分已闭 · ⛔ 补模型归 ArchForge；#543 同 | 均**不触及**；ADR-CEP-25 须显式处理 G10 归属与 #543 的冲突 |
| gap **T1~T4 复评**（§5.1，结论「全未达」）| 本 ADR 的正当性**不是**「T1 已触发」| **不引用** T1~T4 作正当性；ADR-CEP-26 须**显式 supersede** 该复评结论 |
| `ADR-CEP-00` SU 2026-11-11「一决策一文档化」残余 + 3 触发条件 | 触发条件针对**既有** ADR 的**拆出**（取代范围单元素 / 台账裁决改单块 / invariant owner 迁移）| 本 ADR 为**新件**且 ⛔ 不拆既有 ADR ⇒ **不触发**；但 ⛔ **不得**写成「1 份装三段的 umbrella」（正是该治理方向要防的形态）|

---

## 2. 决策

### 决策 1 —— 主决策：CEP 引入「用户自定义稳定态协议（SSP）」能力

**裁定**：CEP 引入用户自定义 SSP 能力，形态 = **独立 SSP 声明面（数据）+ 通用解释器**，归 **CppTLM**。
**子项 1.1 · 归口：机制归 CppTLM / 语义内容归 ArchForge**

`ADR-CEP-00` 决策 4 的 gem5 Ruby 豁免支持「**可插拔机制** = 框架能力」；但仓内已有**三条反向先例**划出更细的线 ⇒ 站得住的表述是二段式：

| 归属 | 内容 | 依据 |
|---|---|---|
| ✅ **CppTLM**（本仓可实施）| ① SSP 声明面 + 通用解释器（**机制**）· ② 手写 adapter / engine 的**接入路径**（**实现既有设计**）| `ADR-CEP-00` 决策 4 + `ADR-CEP-20:88` |
| ⛔ **ArchForge**（须先取设计权威）| **某个具体协议的语义内容**（该协议状态 / 事件 / 序语义**本身**的设计）| `ADR-CEP-00` 决策 1 单向门 + `ADR-CEP-20:88` + Entry #543 / G3 先例 |

⚠️ **适用条件**：若诉求实为「把**某个具体协议**建模」⇒ ⛔ **不适用**本决策 ⇒ 走 ArchForge 单向门。
**子项 1.2 · 产出形态 = 可执行语义面（⛔ 非现状登记表）**

⚠️ 仓内已验证的 G1 / G10 范式产出的是**现状登记表**（声明「有没有」）；本 ADR 要的是**可执行语义面**（**执行**该数据面的稳定态语义）⇒ **新物种**，⛔ 不得按声明面的估时 / 风险写。
**子项 1.3 · 实现形态钉死：数据 + 通用解释器（逐条不触及 `ADR-CEP-18` 决策 1 的四件排除物）**

| 排除物 | 本 ADR 的处置（**逐条**）|
|---|---|
| **DSL** | ⛔ **不引入**。SSP 面是 **JSON 数据**，⛔ 不是一门语言；⛔ 无语法定义 / 无类型系统 / 无语言级扩展点 |
| **解析器** | ⛔ **不引入**。消费方是**既有** JSON loader（`instance_assembly_loader.hh` 同型）；⛔ 无词法/语法分析、⛔ **不生成**解析器代码（⛔ 无 ANTLR / 无 .g4 / 无自举前端）|
| **IR** | ⛔ **不引入**。⛔ 无独立中间层：JSON →（既有 loader）→ C++ 值；⛔ **不**定义 SSP 的 AST / IR 类型 |
| **代码生成** | ⛔ **不引入**。产出是**声明面 + 解释器**；⛔ **不**从声明面**生成** adapter / engine 源码（属 ADR-CEP-26）。⚠️ **用 `ADR-CEP-18` 决策 6 的原文词「生成器」**（⛔ 非本稿早前转述的「代码生成」—— 那更窄，恰好给「烘焙」留辩称空间）|
| **烘焙 / 常量表编译**（**第三条路径 · 本轮新增排除**）| ⛔ **不引入**。「SSP JSON → `*_state_spec.hh` 式 **constexpr 常量表**」的脚本**不生成 adapter / engine 源码** ⇒ 字面上不撞上表四件，**但实质是把「解释器」架空为「生成器 + 数据编译」** ⇒ 撞 `ADR-CEP-18` 决策 1 / 6 的**实质**且**不触发重新评审**。⚠️ **仓内诱惑极强**（`mesif_state_spec.hh` 的 `kTable` 恰是 constexpr 表形态，实施者极可能「生成同款表以复用既有载体与测试面」）⇒ **本 ADR 连「把声明面编译为任何代码产物」的变体一并排除**（产物**不是** adapter / engine 源码也禁止）。守护载体 = V-24-5 的字面清单 |

⇒ **与四件排除物 + 烘焙路径零交集** ⇒ 本 ADR **不需要** `ADR-CEP-18` amendment。

⚠️ **判别力上限的显式继承**（oracle 指出本文未声明）：上表五条排除**全部是「否定存在性」命题** ⇒ 机器闸只能扫关键词 ⇒ 判别力上限 = **措辞 + 评审**。这与既有 `ADR-CEP-18` 的 `V-18-1`（**文档层判据**，其 §4 已明写「机器载体不存在 ⇒ 人工评审 + 台账裁决」）**同源** ⇒ ⛔ **不是新债**，但本文**显式继承**该性质（不做「已机器化」的误称）。

### 决策 2 —— SSP 声明面 = **独立面**（数据驱动）；⛔ 不扩实例面

**裁定**：新增**独立的** SSP 声明面（JSON + **同名** `.schema.json`，per `AGENTS.md:42` 铁律 3），⛔ **不进** `protocol_instance.schema.json`（13 必含 + 1 可选，M1 冻结面）· ⛔ **不扩** `perm` 值域 · ⛔ **不新增** canonical 流 · ⛔ **不新增** `family` 枚举值（决策 3.2）。

**字段组**（⛔ 具体字段名待实施阶段定稿）：**身份**（`id` **全局唯一** + 指向 adapter / engine 的绑定，per `ADR-CEP-12` 的 `adapter` 口径）· **状态集**（稳定态枚举，⛔ 不写协议名）· **事件集**（⛔ 来源见决策 3.1）· **转移表**（`transition[state][event]`，`0xFF` = 空洞 + **逐格 `basis`**，per `mesif_state_spec.hh` 的 `Basis` 五元组）· **perm 投影**（**4 值锁**）· **排序假设**（逐 network × vc，per `ADR-CEP-12`）。
**子项 2.1 · 铁律 2 的落点：SSP 数据文件 = 协议名的**第四类允许面**

`AGENTS.md:41` 铁律 2 规定通用层禁协议名、协议差异只允许出现在 `*_adapter.hh` / `*_state_spec.hh` / `*_protocol_engine.hh` **三类**文件。⚠️ **SSP 数据文件必然携带协议名**（每协议一份声明，其 `id` 即协议标识）⇒ 构成**第四类**允许面。**裁定**：SSP 声明面**数据文件**登记为铁律 2 的第四类允许面（⛔ 扩的是「允许出现协议名」的文件类型清单，⛔ **不是**放宽「通用层禁协议名」）；配套扫描的**允许面须逐字枚举**（⛔ 非开放集，per `ADR-CEP-23` V-23-5 同型纪律）。
**子项 2.2 · 前向兼容：本面同时是「手写解释器」与「（未来）生成器」的**共同输入面**

`ADR-CEP-18` 决策 4 把 SSP 描述面定义为生成器的**输入** ⇒ 本面须预留**机器可序列化**（JSON）· **可逐格 diff**（V-24-2）· **来源可归因**（决策 4 的第 4 档）三项性质 ⇒ 否则 ADR-CEP-26 落地时要么重做、要么产生第二真源。
**子项 2.3 · 校验深度声明：loader 只保「形态 + 引用闭环」，⛔ 不承诺语义**

**裁定**：loader 校验深度 = **形态**（schema / shape / fail-closed）+ **引用闭环**（状态 / 事件 / `Basis.ref` id / perm 值 / flow 引用均在允许集内）；⛔ **不承诺** 活性 / 无死锁 / 一致性 / 兼容性 —— 逐字继承 `ADR-CEP-13` 决策 5 / 6。⚠️ **诚实后果**：一个**有死锁**的 SSP 在 loader 侧**不会被拒绝**（形态合法）⇒ 该缺口**不得**表述为「loader 已把关」⇒ 由实验侧违例闸承担（`ADR-CEP-06`）⇒ V-24-10 以**措辞禁令 + 反向 fixture**钉住。

### 决策 3 —— **两个维度在本 ADR 中冻结为「未定义」**，且**禁止实现阶段自创**

⚠️ 本决策是**「冻结未定义」**（⇒ 实施前置义务），**不是**「留白」。二者若在实施阶段由实现者自决，将分别退化为「自造 mini-DSL」与「改冻结面 schema」。
**子项 3.1 —— 事件字母表的来源（**未定义**）**

**事实**：仓内**无**跨协议 canonical 事件词表（事件集**逐协议**定义）。⚠️ **概念澄清（⛔ 易错点）**：`ADR-CEP-02` 的 canonical **10 流**是「**流**」，`flow_support` 是 10 流覆盖矩阵；⛔ **它不是引擎事件字母表** ⇒ ⛔ **不得**把「事件集 ⊆ 10 流」当作已成立的约束。**裁定**：事件字母表的**来源与扩权方**冻结为未定义 ⇒ 实施前置 = 先补齐来源与扩权规则（新建词表 or 复用既有引擎事件集之一），该补齐**须另行决策**（⛔ 不得由实现阶段自创）。
**子项 3.2 —— 用户可写键子集 + `family` 枚举（**未定义**）**

**事实**：实例面 13 必含键中**哪些用户可写**未裁定；`family` 枚举**仅 3 值** ⇒ 用户协议填哪个？加第 4 值 = **三层同批变更**（oracle NIT③ 核实）—— ① `protocol_instance.schema.json` 的 enum（**M1 冻结面**）② `protocol_family.hh:43-47` 的枚举 + `:53` `kProtocolFamilyCount = 3` + `:113` `static_assert(kProtocolFamilyCount == 3)` ③ `test_cep_protocol_bridge_contracts.cc` 对该枚举的核对 ⇒ **必然触五步链 + 冻结面授权**。**裁定**：冻结为未定义 ⇒ 实施前置 = 逐键裁定 + **显式决策** `family` 是否加值（**代价须与取舍一并亮出**，不得只说「加个枚举值」）。**候选划分（供裁决取用；⛔ 本 ADR 不裁定）**：`transition` + `protocol_version` + `status` 倾向用户可写；`flow_support` / `perm` / `consistency_labels` / `ordering_assumption` / `vc_binding` / `sharer_tracker` / `combination_capabilities` 倾向框架固定（承载跨 ADR 契约面）。**额外未定点**：用户协议的版本语义与 `ADR-CEP-11`（版本锁定表 = 唯一真源）的关系未定义 ⇒ 归入本子项的实施前置。

### 决策 4 —— 来源登记对「用户自定义」的处置：新增 `RefKind::UserDefined`（第 4 档）

**问题**：`RefKind` 三档对用户自创协议**无对应档**（无 `SpecClause`、无 `PublicNonnormative`、也不是 `RepoDerived`）⇒ 未登记 id **fail-closed 判红** ⇒ 任何用户协议的 `Basis.ref` 都无处登记 ⇒ S1 不可用。**裁定**：新增第 4 档 **`UserDefined`**（用户自定义声明 · 无外部来源）：⛔ 不计入 `Direct` · ⛔ 不计入 `B3b`（与 `PublicNonnormative` 同）· **未登记 id 保持 fail-closed** · `Basis.ref` **可空**（无外部来源可引）但其余四字段（`kind`/`origin_kind`/`pending`/`note`）**仍必填** · 须**显式声明**「本声明为用户自定义」（⛔ 不得隐式落 `RepoDerived`）。⚠️ **不改**三档既有语义（⛔ 单向增档 ⇒ 既有 `cep_refkind_coverage` / `cep_basis_source_kind` 判据**不失效**）。

⚠️ **档位语义的精确澄清**（oracle 指出本文未排除的误读）：`UserDefined` 是「**无锚点格**的**兜底档**」，**不是**「用户协议的协议级统一档」。⇒ **逐格 basis 按内容选档**：用户自创协议**不等于**每格都无锚点 —— 用户**可以**基于公开论文设计自有协议，**有外部锚点的格仍应挂既有三档**（`PublicNonnormative` 等）；⛔ **不得协议级统一挂 `UserDefined` 并强制全部格 `ref` 为空**。

⭐ **本决策的诚实性依据（仓内现状佐证）**：`mesif_state_spec.hh:103-108` 的 `resolve_ref` 把**空 `ref` ⇒ 隐式 `resolve("repo")`**（`RepoDerived`）⇒ **仓内现状恰恰是本文选项 G 所否决的伪装**（「无来源」被隐式落成「本仓推导」）。⇒ `UserDefined` 档把这一隐式伪装**显式化** ⇒ **诚实性净改善**（不是新增一层掩盖）。

**两案成本对比（被否方案的代价须留痕）**：**(a) 新增 `UserDefined` 档**（本决策）—— 触碰 `source_registry.hh` 的 `RefKind` 语义（**该头非冻结面** ⇒ 无五步链）· 需同步 `sources.json` + 两 spec + 载体自足白名单 | **(b) 免 `ref` 约定 + 闸白名单** —— 零 schema 变更 · 五步链零次，但把「用户自创」**伪装成**「已签推导」（落 `RepoDerived`）⇒ 违反 fail-closed 的诚实原则，且白名单成为「无来源」的常驻开口。⇒ (a) 成本落在**语义准确**、(b) 成本落在**诚实性** ⇒ **选 (a)**；若实施成本过高可回退 (b)，但须**显式登记**该回退及其诚实性代价（⛔ 不得默默改口径）。

### 决策 5 —— 能力边界（继承式约束，⛔ 不新增契约）

| # | 约束 | 继承自 | 对自定义协议的实际含义 |
|---|---|---|---|
| B1 | canonical **10 流**一次定义（**流**，⛔ 非事件词表）| `ADR-CEP-02`（`CEN-Ib1`）| ⛔ 不得发明新**流**；其**事件**如何映射到流 ⛔ 待决策 3.1 补齐后裁定 |
| B2 | `perm` **4 值锁** | `ADR-CEP-02`（`CEN-Ib11`）| ⛔ 无第 5 个 perm 值（用户可写**子集**）；`atomic` 走 `request` 的**独立属性** |
| B3 | 排序假设**逐网络**声明 | `ADR-CEP-12` | ⛔ 不得用单值表达多网络异序 |
| B4 | **HTM / 事务内存排除** | `ADR-CEP-17` 决策 5 | ⛔ 不得引入 read-set / write-set / 事务中止语义 |
| B5 | **token 权限携带**归插件内部 | `ADR-CEP-17` 决策 3 | ⛔ 声明面**不表达** token（此类协议形态为 `BroadcastNull` 或「不参与」）|
| B6 | **原子 RMW 实现细节不由声明面推断** | `ADR-CEP-17` 决策 6（ProtoGen 局限 L2）| ⛔ 声明面**不**描述 atomic 的实现 |
| B7 | **协议中立** | `INV-CEP-07A-IC2`（例外含 schema 的**数据值**）| SSP 面**是数据** ⇒ 合法；通用层**不得**出现协议名 |
| B8 | **瞬态态与私有状态不得泄漏到 Stub 面** | `ADR-CEP-05`（`INV-CEP-05-IC1`）| ⛔ 声明面**只描述稳定态**；⛔ 不得声明瞬态态 |

### 决策 6 —— 接入路径：**SSP 声明 + 手写 adapter + 手写 engine**

**裁定**：走**既有**插件机制接入 —— SSP 面提供稳定态语义，**adapter + engine 仍手写**（per `ADR-CEP-17` 决策 3 + `INV-CEP-05-IC1`），注册走 `ProtocolEngineBase<N>`（`protocol_engine_base.hh:220`）+ `ModuleFactory::registerObject<T>` / `REGISTER_MODULE(T)`（per `ADR-CEP-07` 决策 3，⛔ **不发明**新宏形式）。**交付终点（可判定）**：一个用户自定义协议实例能被**装配**、能被**实验**（产出 `ADR-CEP-06` 的 4 件套）、其稳定态语义**逐格可审**、且**可与既有手写 `*_state_spec.hh` 逐格对账**（V-24-2 的 killer 判据）。⚠️ 交付终点**不含**自动生成瞬态态 / 在飞事务模型 / 自动兼容性判定。
**子项 6.1 · 运行期「未定义转移」的失败语义**（从声明面升级到**执行面**后新出现的失败类）
⚠️ 手写 spec 靠 C++ 默认行为兜住「事件落在 `0xFF` 洞上」；**数据面没有默认行为可兜**。⇒ **裁定**：洞命中 ⇒ **显式违例计数上报 + 该次实验判「不可判」**（⛔ **不**静默跳过、⛔ **不** panic）—— 与档 0「0 被读成结论」同型（`ADR-CEP-20` 决策 6）。
**子项 6.2 · 双真源的收敛判据**
⚠️ 6 份手写 spec 与 SSP 数据面并存，若无收敛判据 ⇒ 永久双轨 ⇒ 漂移温床。⇒ **裁定**：**过渡期以手写面为真源**、SSP 数据面经 V-24-2 的**逐格 diff 闸**对齐；**收敛时点判据 = 该协议的 SSP 数据面被 diff 闸判为「逐格相等」且其实例声明绑定到该 SSP 数据面** ⇒ 满足即视为已迁移（迁移本身**不在**本 ADR 范围，见 §8 F3）。

### 决策 7 —— 显式排除 + 后续 ADR 编号预留

| 排除项 | 理由 | 归属 |
|---|---|---|
| **从 SSP 生成瞬态态 + 在飞事务**（生成器）| `ADR-CEP-18` 决策 1 / 6 排除；T1~T4 **实测全未达**（T2 对 token / HTM 结构性不可达 ⇒ 该路线**只可能覆盖部分协议**）；⚠️ 前置层 **MSD 消息停滞检测缺位**（决策 4 表内唯一「暂无对应」）| **ADR-CEP-26**（另立 + `ADR-CEP-18` 追加 SU **声明取代范围**；⛔ SU 只能承接 / 标注 supersede，**不改写**正文）|
| **协议语义层 hop / 时序 + 引擎 egress 接 fabric** | 撞 `hop_timing_capabilities.hh` J-3 / J-4 现状锁 + **Entry #543 归档裁决**；⚠️ 实际规模远小于措辞（**仅 `chi_rnf` 有 egress + 在飞探针** ⇒ 6/8 零发射、3/8 无独立引擎头 ⇒ 须**收敛到具体实例清单**）| **ADR-CEP-25**（另立；须显式处理与 Entry #543 的冲突）|
| **belief 模型 / 不完整目录知识** | 沿 `ADR-CEP-23` 决策 2 | 已决 |
| **新增 canonical 流 / 新 perm 值 / 新 SharerTracker 种类 / 新 `family` 值** | 决策 5 + 决策 3.2 | 已排除（`family` 的**是否**加值 ⏳ 待决策 3.2）|
| **自动协议兼容性判定** | `ADR-CEP-13` 决策 5 / 6 | 已排除 |
| **具体协议的语义建模** | 决策 1.1 的适用条件 | ArchForge 单向门 |
| **「Stage 1~3」的结束判据** | ⛔ 仓内**无**该判据 ⇒ `ADR-CEP-18` 决策 1/6 的 amendment 生效条件不可判定 ⇒ **改用条件触发措辞**（不依赖 stage 边界）| **ADR-CEP-26** 的义务 |

---

## 3. 选项对比

| 选项 | 结论 | 理由 |
|---|:--:|---|
| **A. 独立 SSP 声明面 + 通用解释器**（⛔ 不扩实例面；adapter / engine 仍手写；**四件排除物零交集**）| ✅ **选中** | 零实例面契约变更（13 必含 + 1 可选不动）⇒ **不动** `test_cep_schema.cc` 的属性数断言；沿两个新声明面先例；「自定义」门槛从「写 C++ 插件」降到「填声明 + 写适配器」；逐格 `basis` 复用现有来源可追溯机制；与 ADR-CEP-26 共用同一 SSP 格式 ⇒ ⛔ 不产生第二真源；**不需要** `ADR-CEP-18` amendment |
| B. 扩 `protocol_instance.schema.json` 加 SSP 内嵌段 | ❌ | 破「13 必含 + 1 可选」契约（`test_cep_schema.cc:443` 机器锁定 `properties == 14`）⇒ 必然触五步链 + 改机器断言；且 SSP（协议描述）与实例（协议实例）语义**正交**，内嵌即耦合 |
| C. 沿用 `*_state_spec.hh` 的 C++ 形态（现状延长）| ❌ | 「自定义」= 写 C++ 头 ⇒ 门槛不变、**问题未解**；逐协议枚举 ⇒ 协议中立与逐格 basis 的机器可审性无法统一 |
| D. 双形态并存（C++ 头 + JSON 声明都当真源）| ❌ | **双真源** ⇒ 漂移风险极高（`ADR-CEP-23`「无双份数据」纪律 + `mesif_state_spec.hh:142-143`「数据只存在这一处」先例）|
| E. 让用户直接写 SSP 的 DSL / 解析器 | ❌ | 撞 `ADR-CEP-18` 决策 1 / 6 的四件排除物；T2 对 token / HTM 结构性不可达 |
| F. 事件词表就地由实现者自创 | ❌ | 「自造 mini 事件 DSL」⇒ 决策 3.1 已冻结该路径 ⇒ 须先补齐来源与扩权规则 |
| G. 来源登记沿用 `RepoDerived` 兜底用户协议 | ❌ | `RepoDerived` 语义 =「本仓 / 跨仓已签锚点或本仓推导」⇒ 用户协议**不是**本仓推导 ⇒ 语义误标；且把「用户自创」伪装成「已签推导」⇒ 违反 fail-closed 的诚实原则 ⇒ 决策 4 取 `UserDefined` 档 |

---

## 4. 后果 / 代价

**正面** — ①「用户自定义协议」从「写 C++ 插件」降为「**填声明 + 写适配器**」⇒ 打开 CEP 的自定义语义；②**零实例面契约变更** ⇒ 不动 `protocol_instance.schema.json` 与其机器断言；③逐格 `basis` 复用现有 `RefKind`（**非冻结面**）⇒ 用户协议同受「来源效力可机判」约束且**不被伪装成已签推导**；④与 ADR-CEP-26 **共用 SSP 格式** ⇒ ⛔ 不产生第二真源；⑤归口论证完整（机制/语义二段式 + 三条反向先例已处理）⇒ 不与台账归属列打架。

**负面 / 代价** — ①**表达力上限被明确**：终点是「稳定态语义可声明 + 可执行」，⛔ 不覆盖瞬态态 / 在飞事务（ADR-CEP-26 的事）；②逐格 `basis` 对用户是负担 ⇒ 决策 4 的 `UserDefined` 档 + 「`ref` 可空、其余四字段必填」是**部分**缓解（⛔ 非全部）；③**两个维度冻结为未定义** ⇒ **S1 在其补齐前不可实施** ⇒ 排期须把它们当**前置**而非并行项；④**两份 SSP 表达面并存期**：6 份手写 spec **不迁**（迁移 = 6 协议 × 逐格 basis 重导 ⇒ 大工程 + 冻结面风险）⇒ 须显式定位（前者 = 实现侧载体，后者 = 声明侧）+ 逐格 diff 闸（决策 6.2）；⑤新增一处需同步的载体（声明面 + schema + spec + 闸 + 快照登记）⇒ 漂移面 +1；⑥**「可执行语义面」是新物种** ⇒ G1 / G10 声明面范式**不适用** ⇒ 估时/风险不得按声明面写（决策 1.2）。
**边界情况**（⛔ 须在实施阶段以判据覆盖，不得静默通过）

| # | 边界 | 期望处置 |
|---|---|---|
| E1 | **空 / 退化 SSP**（0 状态 / 0 事件 / 全 `0xFF` 洞表）| loader **fail-closed**（带红因签名）；⛔ 不得静默通过 |
| E2 | **id 冲突**（撞既有 8 实例 id，或两用户协议互撞）| schema pattern 只管形态不管唯一性 ⇒ 须**装配期唯一性检查** |
| E3 | **无源协议** | 决策 4 的 `UserDefined` 档解 fail-closed 死锁；⛔ 不得落回 `RepoDerived` 伪装（⇒ 诚实性依据见决策 4 的 `mesif_state_spec.hh:103-108` 佐证）|
| E4 | **规模上限**（state × event 表增长）| 须声明上限 + 触发式扩容判据（先例：LWC 32-port cap · sharer 位图 64-bit 只映射 4 bit + `ADR-CEP-21`）|
| E5 | **双写窗口**（6 份手写 spec 与 SSP 面并存）/ **多用户版本并存**（两份 SSP 声明同一协议的不同版本）| 须配**逐格 diff 闸**（V-24-2）+ 显式真源定位声明（决策 6.2）；版本语义与 `ADR-CEP-11` 的关系 ⏳ 待决策 3.2 |
| E6 | **新文件冻结面归属** | `FROZEN_GLOBS` 是**显式文件名清单**（仅 `docs/architecture/cep/*.md` 有「新文件自动入集」口径）⇒ SSP schema / spec **不会自动入集** ⇒ 本 ADR 取「**显式入集**」（新 spec 承载契约语义 ⇒ 走五步链）⇒ ⛔ 沉默 = 裸奔或意外膨胀 |
| E7 | **解释器错误路径**（数据合法但运行期事件落在 `0xFF` 洞）| 决策 6.1（违例计数 + 不可判）|

**风险** — **R1 冻结面膨胀**（每新增声明面/spec 进 M1 集 ⇒ 五步链成本上升）· **R2 双形态漂移**（改 SSP 面却以为改了已实现协议语义 ⇒ **静默假绿** ⇒ 由 V-24-2 兜住）· **R3 归口判权被挑战**（诉求实为「具体协议建模」⇒ 由决策 1.1 适用条件显式兜住）· **R4 用户预期落差**（期望「填完声明就得到带瞬态态的并发协议」⇒ 终点达不到 ⇒ 须在 backlog 草稿与交付说明中显式声明终点）· **R5 翻转自己埋的闸**（新增 ADR ⇒ 决策块快照须增行否则 R-d 红；新 schema/spec 入冻结集 ⇒ manifest `artifact_count` 变 ⇒ 五步链 ⇒ **两项须与本 ADR 同批落地**）。

---

## 5. 与上游不变量 / 判据的关系（本 ADR **不新造** invariant 编号）

> **口径**：同一约束挂两套编号 = 第二套判据标准 ⇒ ⛔ 零新造 `INV-CEP-24-*`。

| 本 ADR 的约束 | 复用编号 | 关系 |
|---|---|---|
| SSP 面**协议中立**（通用层无协议名；声明面作为**数据值**合法）| `INV-CEP-07A-IC2`（例外明文含「capability schema 的数据值」）| 本 ADR 把例外**同型**扩展到 SSP 数据文件（决策 2.1）⇒ ⛔ **owner ADR（`ADR-CEP-07A`）须追加 Status Update 留痕**（读 owner ADR 的人须知道例外已扩）⇒ 登记为 §7 C11 |
| 实例声明面（**13 必含 + 1 可选** = `properties` 14）**不动** | `INV-CEP-12-IC1` + `test/test_cep_schema.cc:443` | ⛔ 不碰该面 ⇒ 该不变量的取值与断言均不变 |
| canonical **10 流**一次定义 | `CEN-Ib1` | 决策 5 B1 的继承源 |
| `perm` **4 值锁** + atomic 走 `request` 独立属性 | `CEN-Ib11` | 决策 5 B2 的继承源 |
| Stub **只暴露稳定态**，瞬态态与私有状态住插件内部 | `INV-CEP-05-IC1`（`ADR-CEP-05:164`）| 决策 5 B8 的继承源 |
| 装配期通道依赖图无环为必过项 | `INV-CEP-09-IC1`（`ADR-CEP-09:125`）| 与 ADR-CEP-25 相关；本 ADR ⛔ 不新增依赖边 |
| **HTM 排除** + token / HTM 私有状态归**插件内部** | `ADR-CEP-17` 决策 5（**V 级判据**，无独立 INV 编号）+ 决策 3 | 决策 5 B4 / B5 + 决策 6 的依据 ⇒ ⛔ 不新造编号 |
| 协议**生成器**不在 Stage 1~3 | `ADR-CEP-18` 决策 1 / 6（无独立 INV 编号；其 §3 写死约束 + V-18-1）| 本 ADR **不动**该决策 |

---

## 6. Verification（前瞻判据 · 可勾选 —— **不是**已跑完的实测日志）

> **口径**：每条给**载体** + **反向 fixture（毒化 ⇒ 必红）** + **归因 id**；归因 id 一律**复用 owner 编号**（§5）—— **零新造 id**。⛔ 载体尚未存在者**如实标注**「待落地」。

| # | 判据 | 载体 | 反向 fixture（毒化 ⇒ 必红）|
|---|---|---|---|
| **V-24-1** | SSP 面**与** `protocol_instance.schema.json` **零耦合**（实例面 `properties` 仍 == 14 / `required` 仍 == 13；SSP 面**不进**实例面）| `test/test_cep_schema.cc:443`（既有断言）| 把 SSP 字段塞进实例面 schema ⇒ `properties == 14` 必红 · 归因 `INV-CEP-12-IC1` |
| **V-24-2** | ⭐ **往返等价（killer 判据）**：用 SSP **数据面**表达一个既有协议（建议 MESIF），机判其 transition 表与 `mesif_state_spec.hh` **逐格相等**（含 `0xFF` 空洞与逐格 `Basis`）⇒ 证明「数据面表达力 ≥ 手写面」| 待落地（loader + 逐格 diff 用例）· 既有对照面 = `mesif_state_spec.hh` 的 `kTable` | 改 SSP 数据面一格 ⇒ 必红；⛔ **反向**：手写面改一格而数据面未改 ⇒ 亦必红（⇒ **双向**）· 归因 **无**（新判据，owner = 本 ADR）|
| **V-24-3** | SSP 面**协议中立**（通用层无协议标识符；数据文件作为第四类允许面被**逐字枚举**）| 待落地（字面清单扫描）· 既有第二事实载体 `test/test_cep_contracts_crosscheck.cc` | 通用层出现协议名 ⇒ 必红 · 归因 `INV-CEP-07A-IC2` |
| **V-24-4** | `perm` 投影取值 ⊆ **4 值锁**（⛔ 无第 5 值；用户可写**子集**）| 待落地（SSP 面校验 + 装配闸）| 出现第 5 个 perm 值 ⇒ 必红 · 归因 `CEN-Ib11` |
| **V-24-5** | 声明面**不表达** belief / token / atomic 实现细节 / **瞬态态**（决策 5 B4/B5/B6/B8）· ⛔ **无烘焙脚本 / 无生成产物 / 格内无条件表达式串**（决策 1.3 的第五条排除）| 待落地（字面清单扫描，同 V-24-3 形态）| 出现 belief / token / 瞬态态字段 ⇒ 必红；出现条件表达式 / 烘焙产物 ⇒ 必红 · 归因 **无**（守门判据）|
| **V-24-6** | **声明侧 ≠ 实现侧**（R2 防线）+ **id 全局唯一**（E2）| 待落地（双向零 orphan **或** 显式「未绑定」登记 + 装配期 id 唯一性检查）| 误绑已实现实例 / id 撞既有实例 ⇒ 必红 · 归因 **无**（守门判据）|
| **V-24-7** | **来源登记**：`UserDefined` 档未登记 id ⇒ **fail-closed**；`ref` 可空但其余四字段必填 | 待落地（`source_registry.hh` 第 4 档 + `sources.json` + 载体自足白名单）· 既有 fail-closed 面 = `source_registry.hh:97-100` | 移除一个 `UserDefined` id ⇒ 必红；`note` 置空 ⇒ 必红 · 归因 **无**（守门判据）|
| **V-24-8** | **空 / 退化 SSP** loader **fail-closed**（E1）| 待落地（loader 红因签名 + 反向 fixture）| 提交 0 状态 / 全洞 SSP ⇒ 按**正确原因**红（⛔ 非「rc≠0」）· 归因 **无**（守门判据）|
| **V-24-9** | **治理同步**：决策块数登记进 R-d 快照；SSP 新 spec / schema 的冻结归属为**显式入集** | `test/cep_decision_block_snapshot.json`（现 26 份 / 145 块）+ `scripts/test/cep_decision_block_check.py` · `scripts/test/cep_freeze_check.sh` | 快照漏登本 ADR ⇒ R-d 必红；新 spec 未入冻结集 ⇒ freeze 必红 · 归因 **无**（治理判据）|
| **V-24-10** | ⭐ **措辞禁令**（决策 2.3 的机器面）：SSP 面 / loader / ADR **不得**出现「保证无死锁」·「保证无违例」·「保证兼容」类断言 | 待落地（措辞扫描面 + **字面清单**）| 写入该类断言 ⇒ 必红 · 归因 **无**（守门判据）|
| **V-24-11** | **洞命中 = 不可判**（决策 6.1）：运行期事件落在 `0xFF` 洞 ⇒ 产出**违例计数**且该次实验判「不可判」，⛔ 不得静默跳过 | 待落地（违例计数载体 + 实验判定面）| 静默跳过 ⇒ 必红（违例计数不增）· 归因 **无**（守门判据）|
| **V-24-12** | **回归**：既有 8 实例行为**逐字不变**；全量 ctest + `scripts/test/cep_*.sh` 零红 | 既有 `scripts/test/cep_*.sh`（55 个）+ `test/cep_*.py`（14 个）+ ctest | 任一既有实例行为变更 ⇒ 必红 · 归因 **无**（回归判据）|
| **V-24-13** | ⭐ **守护决策 3 的冻结**（oracle 指出本文的唯一断点：12 条判据对决策 3 **零守护**）：SSP 面的**事件集 ⊆ 已登记的事件词表来源**（⛔ 不得出现未登记的事件词表）· **`family` ⊆ 3 值域**（⛔ 不得出现第 4 个族值）| 待落地（字面清单扫描 · ⛔ 非开放集表述）· 既有对照面 = `protocol_family.hh:53`（`kProtocolFamilyCount = 3`）+ `:113`（`static_assert(kProtocolFamilyCount == 3)`）| 出现**未登记的事件词表** ⇒ 必红；出现**第 4 个 `family` 值** ⇒ 必红 · 归因 **无**（守门判据；⛔ 不挂 invariant 编号）|

> **诚实边界**：V-24-2 ~ V-24-12 的载体**在本 ADR 签发时全部尚未落地**（⛔ 属实施阶段义务）⇒ ⛔ 本章**不得**被读成「已具备判别力」；其当前价值是**边界声明**（钉死「自定义协议能做什么 / 不能做什么」），由评审 + 后续实施落地。⚠️ V-24-2 是**唯一**能证明「数据面表达力 ≥ 手写面」的判别性证据；⛔ 其余判据只证明「边界未被越过」。

---

## 7. 签发前置 checklist（治理同步面 · 逐条勾）

> **形态**：照抄 `ADR-CEP-18` SU ① 的「签发前置」形态。⛔ 本清单是**签发前**义务，⛔ 不是实施期可选项。

| # | 义务 | 载体 / 命令 | 状态 |
|---|---|---|---|
| C1 | metis 1 轮 gap 分析已处置 | 本稿（18 条 findings 逐条落点 · 审稿报告 §1.2）| ✅ 已处置 |
| C2 | **oracle** 审查 + **签发凭据显式声明**（⛔ **签发 ≠ 可实施**：决策 3 两维度未定义 ⇒ S1 待 §8 F5 补齐，形态照 `ADR-CEP-18` SU ③「签发凭据显式声明」）| 审稿报告 §2（判定行原文 `判定: APPROVE`）· 本 ADR 文末 `## Status Update` 的「签发凭据」段 | ✅ **oracle 已处置**（3 项 must-fix 全落）· ⏳ **签发凭据段待落** |
| C3 | **critic** 审查（与既有 ADR / 契约冲突）| 审稿报告 §3（判定行原文 `APPROVE_WITH_CONDITIONS` · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**）| ✅ **已处置**（2 SHOULD_FIX + 4 NIT 全落）|
| C4 | `ADR-CEP-NUMBER-MAP` 增 **4 行**（**CEP-23 补登记** —— 其签发 commit 未带该行 + **CEP-24** + **CEP-25 / CEP-26** 预留 `🟡 待起草`，先例 = `CEP-10` 无文件亦登记）+ ⭕ **KPI 预算块不变**（**10 / 14 / 14** 计的是 OpenSpec change 数 ⇒ 新增 ADR 不占 change slot）| `ADR-CEP-NUMBER-MAP.md` §2 决策 1 表 + 文末 SU 2026-11-26 | ✅ **已落**（architect 域 · `docs/adr/`；随产物 commit 同批）|
| C5 | 决策块数登记进 **R-d 快照**（26 份 / 145 块 ⇒ 27 份 / 152 块）+ 登记进 `decision_adrs`（未登记即红）| `test/cep_decision_block_snapshot.json` · `test/cep_adr_structural_deferrals.json`（均 builder 域）| ⏳ 待办 |
| C6 | 冻结面五步链（ADR 在 `FROZEN_GLOBS` 内 ⇒ **新文件自动入集**）| 台账 Entry → 产物 commit → `--update --authorized-by "Entry #N"` → 清单单独 commit → 无参复验 rc=0（`docs/guide/cep-usage.md §12.6`）| ⏳ 待办（**台账 Entry 属 planner 域**）|
| C7 | `openspec validate --specs --strict` ⇒ **CEP 前缀 spec 失败数 == 0**（实测基线：141 份中 55 份红**全为非 CEP** ⇒ `cep-*` 50 份全绿）· `openspec validate --changes --strict` ⇒ 无 active change | `openspec validate --specs --strict`（⛔ **裸** `openspec validate --strict` 恒为 `Nothing to validate` ⇒ **空转**，不可作验收证据，per `ADR-CEP-22` SU 口径）| ⏳ 待办 |
| C8 | 关联 change 登记 | `backlog: cep-user-defined-ssp-declaration`（本文元数据已写）| ✅ 已登记 |
| **C9** | ⭐ **owner ADR 留痕**（critic SHOULD_FIX ①）：决策 2.1 扩展了 `INV-CEP-07A-IC2` 的**例外集**（「例外**仅** Plugin 内部与 capability schema 的数据值」→ 增 SSP 声明面数据文件）⇒ 该句是**冻结面内**的签发 invariant 文本 ⇒ ⛔ 须为 **`ADR-CEP-07A` 追加 Status Update** 标注「例外扩展，**非新增**」（先例 = `ADR-CEP-05:164` 的 `INV-CEP-05-IC1` **补落**三段式：来源 + 一手依据 + 落位说明）⇒ ⛔ 否则读 owner ADR 的人不会知道例外已扩 | `docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md` 文末 SU（**同批**走 C6 五步链）| ✅ **已落**（architect 域 · `docs/adr/`；随产物 commit 同批）|
| **C10** | **载体文本同步**（oracle NIT①② + critic NIT）：① `include/tlm/cep/source_registry.hh:13`（兜底注释）+ `:27-28`（「三档唯一真源」表述）随第 4 档同步 · ② `include/tlm/cep/AGENTS.md:41` 铁律 2 文本增「SSP 声明面数据文件 = 第四类允许面」· ③ 根 `AGENTS.md:7` 的「23 份决策 ADR」**本 ADR 签发后变 24** ⇒ 须更新 · ④ 决策 4 的 `cep_refkind_coverage` / `cep_basis_source_kind` 两 spec 文本同步（实施期义务）| 三个载体文件（⛔ **均不在 `FROZEN_GLOBS`** ⇒ 无五步链；但 `include/tlm/cep/AGENTS.md` 在根 `AGENTS.md` ARCHIVES 子目录清单内 ⇒ 须同步该清单）| ⏳ 待办 |

---

## 8. 关联 ADR / 后续

**关联（依赖）**：`ADR-CEP-00`（决策 1.1 + SU 2026-11-11/12）· `-02`（决策 5 B1/B2）· `-05`（决策 5 B8 + 决策 6）· `-07`（决策 6）· `-09`（`INV-CEP-09-IC1`）· `-12`（决策 2 + 决策 5 B3）· `-13`（决策 6 / 7）· `-17`（决策 5 B4/B5/B6 + 决策 6）· `-18`（§1.2 + 决策 1.3 + 决策 7）· `-20`（决策 1.1 + 决策 7）· `-23`（决策 7 + §6 判据形态先例）

**后续（⛔ 本 ADR 不含，另立）**
| # | 议题 | 触发条件 / 阻塞 | 归属 |
|---|---|---|---|
| **F1** | **ADR-CEP-25**：协议语义层 hop / 时序 + 引擎 egress 接 fabric | 撞 J-3/J-4 现状锁 + **Entry #543 归档裁决** ⇒ 须显式处理该冲突；⚠️ **须收敛到具体实例清单**（仅 `chi_rnf` 有 egress + 在飞探针；6/8 零发射、3/8 无独立引擎头）；⚠️ 证据**档位声明**（禁档 0 活性冒充满档，per `ADR-CEP-20` 决策 6）· J-9 的**第三轴**（「引擎是否参与」）落位 · `INV-CEP-09-IC1` 新边无环论证 · `engine_capabilities` egress 字段翻转 + J-04 配套机检 | architect |
| **F2** | **ADR-CEP-26**：SSP → 插件生成器（瞬态态 + 在飞事务）| 须以「新 ADR 收窄 + `ADR-CEP-18` 追加 SU **声明取代范围**」形态 amendment 决策 1 / 6（⛔ SU 只能承接 / 标注 supersede，**不改写**正文）；须**显式 supersede** gap 台账 §5.1 的 T1~T4 复评结论；**T2 的结构性排除（HTM / token 永远手写）原样继承**；⛔ 前置层 **MSD 缺位** ⇒ 缺位时**不立项**；⭐ 四元组（引擎状态集合 + perm 投影 + 在飞事务不变式 + 转发序区分）须显式裁决 **T-C05（State Set 对偶性）** 的承载元素（现有四元组**无**承载）+ 对接既有 `kTransientStateCount` seam + 写入 `INV-CEP-05-IC1` 不泄漏约束 | architect |
| **F3** | **6 份 `*_state_spec.hh` → 声明面形态的迁移** | ⛔ 本 ADR **不迁**；过渡期真源定位 + 收敛判据见决策 6.2 | planner 排期 · architect 判权 |
| **F4** | **档 2（协议级证据面）扩面** | 与 F1 强相关；既有 `cep-tier2-second-family-tilelink` 已拆分承接第 2 族 | planner / builder |
| **F5** | **事件字母表来源 + 用户可写键子集 / `family` / `protocol_version`**（决策 3 的两个冻结未定项）| ⛔ **S1 的实施前置**（不可由实现阶段自创）；裁决须**显式回答两案对比**：① 事件词表 = **新建跨协议词表** vs **复用既有引擎事件集**（`mesif_protocol_engine.hh:189` 的 6 事件词表 / `cxl_state_spec.hh:69` 之一）② `family` 若加值 = **三层同批变更**（schema enum + `protocol_family.hh:43-53` 枚举与常量 + `test_cep_protocol_bridge_contracts.cc` 核对）⇒ **五步链成本**须在裁决中亮出 | architect |

---

## 9. 修订记录

> **变更史**: 见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账（授权记录单一真源）+ `.rddf/reviews/ADR-CEP-24-user-defined-ssp-declaration-review.md`（审稿过程）。

---

## Status Update（2026-11-26）—— **签发**（v1.0）

> **性质**：追加式（per `AGENTS.md`「**ADR 不可变 + Status Update**」先例）—— ⛔ **§1 ~ §9 正文一律不改**；唯一例外是文首 `> **状态**:` 元数据行（唯一豁免行）+ 本段。
> **依据**: **用户指令**「自主推进：先做第 0 步，再切 architect 做第 1 步，最后切 planner 做第 2 步，最后切 builder 实施」· **metis 1 轮**（✅ 18 条 findings 逐条处置）· **oracle 1 轮**（✅ 判定 `APPROVE`，3 项 must-fix 全落）· **critic 1 轮**（✅ 判定 `APPROVE_WITH_CONDITIONS`，`mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**，2 SHOULD_FIX + 4 NIT 全处置）
> **审稿报告**: `.rddf/reviews/ADR-CEP-24-user-defined-ssp-declaration-review.md`（metis §1 + oracle §2 + critic §3 + 收口清单 §4；**过程日志只在此处**，不进本文正文）

### ① 签发凭据（**显式声明** —— per `ADR-CEP-18` SU ③ 同型形态）

| # | 凭据 | 状态 |
|---|---|---|
| 1 | metis 1 轮 gap 分析 | ✅ 已处置（18 条 findings 逐条落点，审稿报告 §1.2）|
| 2 | **oracle** 1 轮（目标场景会怎么坏）| ✅ 判定 `APPROVE`（3 项 must-fix 全落）|
| 3 | **critic** 1 轮（与既有 ADR / 契约冲突）| ✅ 判定 `APPROVE_WITH_CONDITIONS`（**0 BLOCKER** · 2 SHOULD_FIX + 4 NIT 全处置）|
| 4 | 四要素齐（元数据 / 选项对比 ≥3 行 / 独立后果章 / 前瞻 Verification）| ✅ |
| 5 | 归因 id **零新造**（§5 全部复用 owner 编号）| ✅ |
| 6 | 关联 change 已登记 | ✅ `backlog: cep-user-defined-ssp-declaration` |
| 7 | 审稿报告已落 + 本文指针已写 | ✅ |
| 8 | ⭐ **签发 ≠ 可实施** | ✅ **显式声明**：**决策 3 的两个维度（事件字母表来源 / 用户可写键子集 + `family` + `protocol_version`）仍未定义** ⇒ **S1 待 §8 F5 补齐后方可实施** ⇒ ⛔ 任何「CEP 已支持用户自定义协议」的表述**在 F5 闭合前均不成立**；守护判据 = **V-24-13** |

### ② 本次双审的**实质 delta**（相对 v0.1 初稿）

| 来源 | delta |
|---|---|
| **metis** | ① 新增 **§1.3 与既有裁决 / 缺口台账的对齐**（7 行映射表）② 决策 1.1 归口改**二段式**（机制 / 语义内容）③ 决策 1.3 **四件排除物逐条声明** ④ **决策 2.1** 铁律 2「第四类允许面」⑤ **决策 2.3 校验深度声明**（形态 + 引用闭环；⛔ 不承诺语义）⑥ **决策 3** 两维度**冻结为未定义** ⑦ **决策 4** `RefKind::UserDefined` 第 4 档 ⑧ **决策 6.1** 洞命中 = 违例计数 + 不可判 ⑨ **决策 6.2** 双真源收敛判据 ⑩ **V-24-2** 往返等价 killer 判据 ⑪ **V-24-10** 措辞禁令 ⑫ **V-24-11** 洞命中不可判 ⑬ **V-24-12** 回归 ⑭ **§7 签发前置 checklist**（9 项）|
| **oracle** | ① **V-24-13**（守护决策 3 —— 此前 12 条判据对决策 3 **零守护**）② 决策 1.3 改用 `ADR-CEP-18` 决策 6 **原文词「生成器」** + **第五条「烘焙 / 常量表编译」排除** + 显式继承 `V-18-1` 的文档层判别力上限 ③ 决策 4 增「`UserDefined` = **无锚点格的兜底档**」+ 引 `mesif_state_spec.hh:103-108`（空 ref ⇒ 隐式 `RepoDerived` = 仓内现状恰是本文否决的伪装 ⇒ 本决策是把它**显式化**）④ **C2** 签发凭据并入「签发 ≠ 可实施」|
| **critic** | ① **C9** owner ADR（`ADR-CEP-07A`）**Status Update 留痕**（`INV-CEP-07A-IC2` 例外集扩展；先例 = `ADR-CEP-05:164` 的补落三段式）② **C2 / 元数据 / 审稿报告** 三处审查状态对齐 ③ **C10** 载体文本同步（`source_registry.hh` 注释 + `AGENTS.md` 铁律 2 + 根 `AGENTS.md` 计数 + 决策 4 两 spec 指针）④ §7 编号连续化（C9 补位）⑤ 审稿报告 §0 快照刷新 |

### ③ 签发后的**未决义务**（⛔ 不得当作已完成）

| 义务 | 归属 | 落点 |
|---|---|---|
| 台账 Entry（冻结面授权记录）| **planner** | `.rddf/roadmap/objectives/obj-cep-platform.md` §11 |
| 产物 commit + `--update --authorized-by "Entry #N"` + 清单单独 commit + 无参复验 rc=0 | **builder** | §7 C6（五步链）|
| `ADR-CEP-NUMBER-MAP` 4 行 + KPI 预算块 | **builder**（`docs/adr/` 随产物同批）| §7 C4 |
| `ADR-CEP-07A` owner SU 留痕 | **builder**（同批）| §7 C9 |
| R-d 快照（27 份 / 152 块）+ `decision_adrs` | **builder**（`test/`）| §7 C5 |
| 载体文本同步 4 项 | **builder** | §7 C10 |
| **决策 3 两维度的裁决**（事件字母表来源 / 用户可写键子集 / `family` / `protocol_version`）| **architect** | §8 F5（⛔ **S1 的实施前置**）|
| 本 ADR 的**实施**（SSP 声明面 + 通用解释器 + loader + 闸）| **builder**（经 planner 立项）| §8 F5 闭合后 |

---
