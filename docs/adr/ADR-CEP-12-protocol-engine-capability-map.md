# ADR-CEP-12: ProtocolEngine 插件能力映射表 Schema

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **协议实例的能力 / 前提假设 / 组合能力的结构化声明 schema**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §5.1(协议族 → 实例 → adapter 映射)+ §5.1.4(协议语义索引)
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
> - 真源 3:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流 + `CEN-Ib11` atomic 承载)
> - 真源 3:`ADR-CEP-04-sharer-tracker-abstraction-redesign.md`(Sharer Tracker 5 实现 + caps descriptor)
> - 真源 3:`ADR-CEP-07-simulation-and-integration-semantics.md`(插件接口可组合性,决策 9)
> - 真源 3:`ADR-CEP-08-ordering-point-portability.md`(Ordering Point 可迁移性 + 排序假设面,本批起草)
> - 真源 3:`ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md`(多 VC 仲裁 + 死锁自由性,本批起草)
> - 真源 3:`ADR-CEP-11-protocol-version-locking.md`(协议版本锁定,待起草)
> - 真源 3:`ADR-CEP-13-coherence-domain-ownership.md`(一致性域归属,本批起草)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
> - 调研:`docs/research/cep-coherence-protocol-space-survey.md` v0.4 §5.7(证据类型归类)+ §5.6 F16/F24/F25
>
> **一手锚点**:
> - 源码 `ProtoGen/MSI_Proto.pcc:3-6`:`Network { Ordered fwd; Unordered resp; Unordered req; }` —— 排序假设是**协议规格的一部分**(E1)
> - ProtoGen ISCA 2018 **p.7** 局限 L3 + L4:网络序假设是输入;VC 分配不由协议生成器负责(E1)
> - HeteroGen HPCA 2022 **p.3** §II-B + Table I:一致性标签体系(E1)
> - 博士论文 §7.6.6:请求级标签(RCC 的 `GetV` 无法区分 read vs acquire → `GetV_R` / `GetV_A`)(E1)
> - 源码 `include/bundles/noc_bundles_tlm.hh:38`:`ch_uint<8> vc_id;` —— 排序是 VC 的静态属性,flit 不带 ordering 字段(E1)

---

## 1. 背景

### 1.1 问题

`ADR-CEP-10`(可观测性归属)、`ADR-CEP-11`(版本锁定)、`ADR-CEP-08` / `09` / `13` 都需要引用"某个协议实例**能做什么 / 前提是什么**"的结构化事实。当前只有 `07 号 §5.1` 的三张**人工维护的 Markdown 表格**,存在三个问题:

| 问题 | 表现 |
|------|------|
| **不可机器检查** | 三张表是散文;ADR 之间的引用只能引"§5.1",无法引用字段 |
| **维度缺失** | 三张表**隐含**了多个维度(10 流支持、perm、一致性模型、Sharer Tracker 选型、组合能力),但**无 schema 承载** |
| **计数会漂移** | 表内 `adapter 归属 = 新增(若选为实现目标)` 与"16 adapter 实例"两处数字并存,实现推进即失真 |

### 1.2 本 ADR 的定位

本 ADR 定义**协议实例的能力声明 schema**。它是 `ADR-CEP-08` / `09` / `13` / `18` 的**共同数据源**,也是 `07 号 §5.1` 三张表的**机器可读对应物**。

> **先落地 schema 草案再签 ADR 的理由**:`ADR-CEP-08` / `09` / `13` 都要**引用字段名**。若 schema 在这些 ADR 之后才定,三者会各自发明字段。→ schema 草案已于 2026-10-20 先行落在 `ADR-CEP-17 §3.4`,本 ADR 正式采纳。

---

## 2. 决策

### 决策 1:schema 全量(协议实例为 key)

```yaml
protocol_instance:
  # ── 身份与状态 ──────────────────────────────────
  id: <stable id>                       # 稳定身份;计数与交叉引用依赖
  family: <industrial | ruby-slicc | academic>   # 来源族(per 07 号 §5.1 口径)
  adapter: <adapter 名>                 # 1 协议实例 = 1 adapter
  status: planned | implemented | excluded       # 计数口径:仅 implemented 计入
  protocol_version: <ver>               # 版本锁(取值策略待 ADR-CEP-11)

  # ── 能力面 ──────────────────────────────────────
  flow_support:                         # canonical 10 流覆盖矩阵
    request: yes|no;            completion: yes|no
    snoop: yes|no;              snoop_resp: yes|no
    writeback: yes|no;          forward_data: yes|no
    back_invalidate: yes|no;    address_resolve: yes|no
    barrier: yes|no;            snoop_completion_ack: yes|no

  perm: [NONE, READ, UNIQUE, WRITEBACK]        # 4 值锁(per ADR-CEP-02 / CEN-Ib11)
  atomic:                                        # atomic 意图承载(per 决策 2)
    supported: yes|no
    intent_field: <name>                          # request 流中承载 atomic 意图的属性名

  # ── 前提假设面 ──────────────────────────────────
  consistency_labels:                   # 决策 3
    read:  SC | TSO | RC-acquire | RC-data | PLO
    write: SC | TSO | RC-release | RC-data | PLO
    request_level: { <request_class>: <label> }   # 决策 4

  ordering_assumption:                  # 决策 5
    <vc_id | named_network>: p2p_ordered | unordered

  vc_binding:                           # 决策 6
    <flow_or_message_class>: <vc_id>

  sharer_tracker:                       # 决策 7
    impl: FullMapDirectory | SparseSnoopFilter | DcohSymmetric | TreeManager | BroadcastNull | 不参与
    caps: { scope, completeness, authority, aggregation_at }

  # ── 组合能力 ────────────────────────────────────
  combination_capabilities:             # 决策 8
    concurrent: yes|no
    hierarchical: yes|no
    heterogeneous: yes|no
```

### 决策 2:`perm` 4 值锁 + atomic 独立属性

- `perm` **维持 4 值**(`NONE` / `READ` / `UNIQUE` / `WRITEBACK`)—— **不新增 `"ATOMIC"` 值**(per `ADR-CEP-02` `CEN-Ib11`)
- atomic 意图作为 `request` 流的**独立属性**承载,属性名由 `atomic.intent_field` 声明
- **依据**:ProtoGen ISCA 2018 **p.7** 局限 L2 ——「ProtoGen **cannot generate new protocol actions not explicitly specified in the SSP**. For example, **it cannot infer how atomic read-modify-writes must be implemented without it being specified in the SSP**」(E1)→ atomic RMW 是**协议动作**,语义归插件,意图经独立属性传递

### 决策 3:`consistency_labels` 的读 / 写两级

- `read` / `write` 各取一个标签
- **依据**:HeteroGen **p.3** §II-B ——「a conventional writer-initiated SWMR-enforcing protocol is said to enforce sequential consistency (SC). Consequently such a protocol is associated with **SC-read-requests and SC-write-requests**」;RC 分 **release-write / data-write** 与 **acquire-read / data-read** 两级(E1)
- **用途**:`ADR-CEP-13` 的跨域组合判据;同一 `barrier` 在不同标签下效果不同(F7)

### 决策 4:请求级标签(决策 3 的必要细化)

`consistency_labels` 须支持**请求级**映射,而非只到"读 / 写"两级。

**依据(一手)**:博士论文 §7.6.6 —— 在 RCC 协议中,**普通 read 与 acquire 都发同一种 `GetV` 请求**,目录**无法区分**二者;HieraGen/HeteroGen 的解法是把请求**改名**为 `GetV_R`(read)/ `GetV_A`(acquire)(E1)。

**后果**:若 schema 只标到"读 / 写"两级,则「**同一 wire 请求在不同标签下语义不同**」这一情形**无法表达** ⇒ 跨域桥接无法正确推断访问类型。

### 决策 5:`ordering_assumption` 逐网络 / 逐 `vc_id`,key 必须可解析

- **粒度**:逐网络(不是一个协议一个单值)
- **key**:`<vc_id | named_network>`,且 `named_network` **必须在 fabric config 中解析到唯一 `vc_id`**,否则**配置加载 fail**
- **理由**:排序是 **VC 的静态属性**,不是 flit 的字段 —— `NoCFlitBundle` 只有 `vc_id` / `flit_type` / `flit_category` / `is_write` / `is_ok` / `error_code`,**不带 ordering**(per `include/bundles/noc_bundles_tlm.hh:38` 及 `:60-62`,E1)。不绑定 `vc_id` ⇒ `ordering_assumption` 与 `flit.vc_id` 无法关联 ⇒ `ADR-CEP-09` 的死锁自由论证拿不到可计算输入
- **依据**:源码 `MSI_Proto.pcc:3-6` 的 `Network { Ordered fwd; Unordered resp; Unordered req; }` —— 排序假设是**协议规格的一部分**(E1);ProtoGen 局限 L3「cannot automatically deduce the protocol for an unordered network given an SSP for an ordered network」(E1,网络序假设是**输入**)
- **单网络协议**:可退化为单值(简化,不强制逐条列举)

### 决策 6:`vc_binding` 是消息类 → `vc_id` 的映射

- **一手依据**:ProtoGen 局限 L4 ——「ProtoGen **does not specify how protocol actions must interact with the interconnect**; it requires the user to **manually define virtual channels and assign messages to channels**」(E1)→ 消息→VC 绑定**不由协议生成器负责**,是**协议侧声明**
- **消费方**:`ADR-CEP-09` Decision A(通道依赖图无环判据)
- **不承诺**:CEP **不自动生成** `vc_binding`

### 决策 7:`sharer_tracker` 逐实例选型 + caps

- `impl` 取 `ADR-CEP-04` 的 5 实现之一,或字面值 `不参与`
- `caps` 复用 `ADR-CEP-04 §2` 的 caps descriptor 四属性(`scope` / `completeness` / `authority` / `aggregation_at`)
- **修正记录**:TSO-CC / RCC / RCC-O / GPU(Spandex)/ PLO-CC 这 5 个**自失效 / write-through 类**语义**不做 sharer tracking 式中心目录**,其 `impl` 应为 `BroadcastNull` 或 `不参与` —— **`SparseSnoopFilter` 本身即一种 sharer tracker**,配它与协议特征自相矛盾(per Oracle 独立复审 N5;一手:HeteroGen §VI-B + `ADR-CEP-17` 决策 3 的 token 同原则)

### 决策 8:`combination_capabilities` 正交于族

- `concurrent` / `hierarchical` / `heterogeneous` 三维,彼此**正交**,且**正交于 `family`**
- **依据**:调研 F16 —— *Gen 族的 Table 1.3 展示了"**算法阶段 × 工具**"的二维能力矩阵**,且每个已发布工具都缺一段(E1)。**该矩阵是"工具的算法阶段",CEP 矩阵是"协议实例的组合能力" —— 形似而非同构**(E3),但"二维"这一**结构**可借鉴
- **为何必须正交**:`family` 回答"这个协议来自哪里",`combination_capabilities` 回答"这个协议能与什么组合" —— gem5 族的 MESI 同时支持 hierarchical,是两个独立维度

### 决策 9:`status` 是计数口径的唯一依据

- `implemented` 才计入"已实现 adapter 实例"计数;`planned` / `excluded` 不计
- **理由**:`07 号 §5.1` 的"16 adapter 实例"是**下界**(§5.1.4 的 5 个语义行标"新增(若选为实现目标)"),落地后变 20。若计数仍散落在 Markdown 表格里,实现推进即引发口径争议(per Oracle 独立复审 N3)
- **`excluded`** 用于显式排除项(如 `MESI_Three_Level_HTM`,per `ADR-CEP-17` 决策 5)

### 决策 10:显式排除

| 排除项 | 理由 | 证据 |
|--------|------|------|
| **不提供**自动协议兼容性判定器 | 学术侧亦未解决 | HeteroGen §VI-E(E1) |
| **不承诺**自动生成 `vc_binding` | CEP 不做协议生成器 | `ADR-CEP-17` 决策 10 |
| **不新增** canonical 流 / **不扩** `perm` 枚举 | 零流数变更 + 4 值锁 | `ADR-CEP-02` |
| **不承诺** `consistency_labels` 覆盖全部学术协议 | 仅覆盖 §5.1.4 的 5 个语义 | `07 号 §5.1.4` |

---

## 3. 不变量

- **INV-CEP-12-IC1**:每个协议实例记录**必须**包含 **7 个字段组(共 13 个顶层属性)**:`id`/`family`/`adapter`/`status`/`protocol_version`、`flow_support`、`perm` + `atomic`、`consistency_labels`、`ordering_assumption` + `vc_binding`、`sharer_tracker`、`combination_capabilities`
  > ⚠️ **口径修订(2026-10-21,per 代码复审)**:本 invariant 原写「6 组」但同句枚举了 **7** 组;
  > 同 ADR 决策汇总写「8 个」、`ADR-CEP-17 §3.4` 写「6 个」、umbrella spec 写「8 个」——
  > **四处互不一致且无人断言**。现统一为 **7 个字段组 / 13 个顶层属性**(= schema 实际属性数),
  > 并由 `test/test_cep_schema.cc` 的「schema 字段组完整性(7 组 / 13 属性)」用例**机器断言**。
- **INV-CEP-12-IC2**:`ordering_assumption` 的 `named_network` key **必须**解析到唯一 `vc_id`,否则**配置加载 fail**
- **INV-CEP-12-IC3**:`status` 变更时,**adapter 计数口径**须同步更新(`implemented` 才计入)

> **编号位说明**:`INV-CEP-12-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **schema 字段组完整性**:每条协议实例记录**必须**含 **7 个字段组 / 13 个顶层属性**(缺组 = fail)
2. **`vc_id` 解析**:`named_network` 解析不到唯一 `vc_id` ⇒ 配置加载 fail
3. **`perm` 值域**:只允许 4 个字面值(机器可检查,per `ADR-CEP-02` `CEN-Ib11`)
4. **`flow_support` 覆盖**:`NullProtocolEngine` 的 10 流覆盖矩阵须与本 schema 的 `flow_support` 对齐(per `ADR-CEP-07` 决策 4)
5. **计数同步**:`status` 变更 ⇒ 计数同步(per `INV-CEP-12-IC3`)
6. **`sharer_tracker` 一致性**:自失效类协议不得填 `SparseSnoopFilter`(per 决策 7)

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-02`(canonical 10 流)| ✅ v0.3 | `perm` 4 值锁 + `CEN-Ib11` atomic 承载 |
| `ADR-CEP-04`(Sharer Tracker)| ✅ v0.6 | 提供 5 实现 + caps descriptor(决策 7) |
| `ADR-CEP-07`(仿真语义 + seam)| ✅ v0.7 | 决策 9:插件**声明** `ordering_assumption` + `vc_binding` |
| `ADR-CEP-08`(Ordering Point)| 🟡 **本批起草** | 排序假设面的**语义**边界(本 ADR 提供字段载体) |
| `ADR-CEP-09`(多 VC 仲裁 + 死锁自由)| 🟡 **本批起草** | **消费** `ordering_assumption` + `vc_binding` → 通道依赖图 |
| `ADR-CEP-11`(协议版本锁定)| 🟡 待起草 | `protocol_version` 的取值策略 |
| `ADR-CEP-13`(一致性域归属)| 🟡 **本批起草** | **消费** `consistency_labels` + `combination_capabilities` |
| `ADR-CEP-14`(Stub 合同测试)| 🟡 待起草 | schema 字段的机器检查落地位置 |
| `ADR-CEP-17`(协议族接入)| ✅ v0.3 | 决策 8:本 schema 的**规范来源**;§3.4 草案转由本 ADR 承接 |
| `ADR-CEP-18`(协议生成路线)| 🟡 **本批起草** | 依赖本 schema 作为"什么协议存在"的真源 |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| schema 全量 | **7 个字段组 / 13 个顶层属性**,协议实例为 key | ✅ 已拍板 |
| `perm` + atomic | 4 值锁;atomic 走独立属性 | ✅ 已拍板 |
| `consistency_labels` 读 / 写两级 | SC/TSO/RC/PLO | ✅ 已拍板 |
| 请求级标签 | 支持 `request_level` 映射 | ✅ 已拍板 |
| `ordering_assumption` 逐 `vc_id` | key 必须解析到唯一 `vc_id` | ✅ 已拍板 |
| `vc_binding` | 消息类 → `vc_id`;不自动生成 | ✅ 已拍板 |
| `sharer_tracker` + caps | 逐实例选型;自失效类不得填 `SparseSnoopFilter` | ✅ 已拍板 |
| `combination_capabilities` | 三维,正交于 `family` | ✅ 已拍板 |
| `status` 计数口径 | 仅 `implemented` 计入 | ✅ 已拍板 |
| 显式排除 | 自动兼容性判定 / 自动生成 `vc_binding` / 新增流 / 扩 `perm` | ✅ 已拍板 |

---

## 7. 修订记录

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | 调研 v0.4 §5.6 F16/F24/F25 + §5.7 证据类型归类 + Oracle 独立复审 Q5/Q6/Q7 | schema 全量 + 8 项决策 + 3 条 invariant |
| 决策 1 的 5 个新增维度 | Oracle 独立复审 Q7 | `id`/`status`/`protocol_version`、`vc_binding`、`sharer_tracker`、`flow_support`、`atomic` |
| 决策 5 | Oracle 独立复审 N8 + 一手 `noc_bundles_tlm.hh:38` | key 绑定 `vc_id`;`named_network` 解析失败即 config fail |
| 决策 7 | Oracle 独立复审 N5 | 自失效类协议的 Sharer Tracker 选型修正 |
| 决策 8 | 调研 F16(E3,形似非同构) | `combination_capabilities` 正交于 `family` |
| **§9 Status Update**(2026-10-22,台账 **Entry #13**) | ArchForge 应答(`4ff2fa6`):判据 (b) **成立** → **R-2 已触发** | **§9**(新增**独立** Status Update 节):R-2 状态 + `perm` 形态约束(Q-3b 补充)+ 路径标签澄清(决策 4 **不适用**)+ 未变更项。**§8 主体(per-network 嵌套形态)零改动** |

---

## 8. Status Update(2026-10-22):`ordering_assumption` / `vc_binding` 升 per-network 嵌套形态

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)——
> §2 决策 5 / 决策 6 与 §3 `INV-CEP-12-IC2` 的**正文不改**,**自本段起以本段文本为准**。
> **触发**: ArchForge 侧 coherence 接口规约合入(commit `8290b4c`)后的跨仓对齐 ——
> 其 N-1/N-2 裁决把 VC 分配从**扁平**改为 **per-network 矩阵**,并给出 diff 级 schema delta(`32 号 §6.4`)。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` Entry #9(`adr-amendment`)
> **落地载体**: `include/tlm/cep/protocol_instance.schema.json` + `include/tlm/cep/fabric.{schema.json,json}`
> + `scripts/test/cep_decl_alignment_check.sh`(判据 C-4 的机器化)

### 8.1 取代范围

| 项 | 位置 | 处置 |
|----|------|------|
| **决策 5** | §2 `:118-124` | **取代** —— 见 §8.2 |
| **决策 6** | §2 `:126-130` | **取代** —— 见 §8.2 |
| **§2 前置 schema 草图** | §2 `:82` | **取代**(草图里的 `<vc_id \| named_network>` 旧 key 形态) |
| **§4 抓取点 2** | §4 `:178` | **取代**(「`named_network` 解析不到唯一 `vc_id` ⇒ 配置加载 fail」旧判据) |
| **§6 决策汇总行** | §6 `:211` | **取代**(「`ordering_assumption` 逐 `vc_id` / key 必须解析到唯一 `vc_id`」旧判据) |
| **§7 修订记录行** | §7 `:226` | **取代**(「key 绑定 `vc_id`;`named_network` 解析失败即 config fail」旧判据) |
| **`INV-CEP-12-IC2`** | §3 `:168` | **取代** —— 见 §8.3 |
| `INV-CEP-12-IC1` / `IC3` | §3 `:163` / `:169` | **不变** |

> **兜底声明**:本 ADR 的 **§2 / §3 / §4 / §6 / §7 全文**中,凡出现「`<vc_id | named_network>`」
> 「解析到唯一 `vc_id`」「解析失败即 config fail」「key 绑定 `vc_id`」等**扁平形态旧谓词**者,
> **一律以本段为准** —— 该谓词在 per-network 嵌套形态下**整体消失**。
> (2026-10-22 复审修正:初版兜底只写 §2 / §3,漏了 §4 抓取点 / §6 决策汇总 / §7 修订记录三处)

> **旧文留痕说明**:正文保留 2026-10-21 的原始表述(不可变约定)。
> 旧文与本段冲突时,**以本段为准** —— 旧文中的「`named_network` 解析到**唯一** `vc_id`」这一
> **谓词在嵌套形态下整体消失**(network 即 key,其下 vc_id 是**显式**给出的,不存在多义)。

### 8.2 取代后文本(规范)

**决策 5(取代后)**:`ordering_assumption` 逐网络 × 逐 `vc_id`

- **粒度**:逐网络 × 逐 `vc_id` —— **key = network**,其下每个 key 是一个 `vc_id`
- **key 语义**:network 必须在 **fabric config 的网络命名空间**中登记
  (per `INV-CEP-12-IC2` 条件 ② 与 ArchForge `22-nsa §0.5.4.1` NS-R1)
- **值的语义**:`{vc_id → ordering}`;每个 `vc_id` 必须落在该网络的**槽位上界**内
  (`vc_id < fabric_config.networks[n].vc_count`)
- **理由**:排序是**协议对所用 `(network, vc_id)` 的保序需求 + fabric 侧担保**,一致性由**装配期校验**承担
  —— 不绑定 `vc_id` ⇒ `ordering_assumption` 与 `flit.vc_id` 无法关联 ⇒ `ADR-CEP-09` 的死锁自由论证拿不到可计算输入
  > ⚠️ **措辞修订(2026-10-22,per oracle 独立架构复审)**:原措辞「排序是 **VC 的静态属性**」易被误读为
  > 「排序归属 Fabric 层」。该论证的**实质**是「key 应是 `vc_id`、flit 不带 ordering 字段」
  > (`NoCFlitBundle` 无 ordering 字段);而**归属**的一手依据是 `.pcc` 的
  > `Network { Ordered fwd; Unordered resp; Unordered req; }` —— 排序假设写在**协议规格文件**里,
  > 是协议生成器的**输入** ⇒ 归**协议侧声明**(per `07 号:145`)。
- **依据**:`MSI_Proto.pcc:3-6`(同上);ProtoGen 局限 L3(同上)
- **单网络协议**:可退化为单 network(简化,不强制逐条列举)
- **⚠️ provisional 标注**:本形态是 **Stage 0 跨仓对齐形态(provisional)** —— 采纳自 ArchForge `32 号 §6.4`
  的 schema delta,而其 2 份声明 `status` 均为 `planned`、规范版本**不在** `ADR-CEP-11 §3` 锁表内、
  `32 号 §6.0.3` 明说死锁自由性「归 CEP 侧机器校验(ArchForge 侧不背书)」。
  **首个 `implemented` 实例装配后,Stage 1 必须复核本形态**。
  **provisional ≠ 免台账** —— 每次演进照常走 §11 台账 `adr-amendment` + 冻结清单重生成。

**决策 6(取代后)**:`vc_binding` 是**逐网络**的消息类 → `vc_id` 的映射

- **粒度**:逐网络 —— **key = network**,其下每个 key 是一个消息类
- **一手依据**:ProtoGen 局限 L4(同上)→ 消息→VC 绑定**不由协议生成器负责**,是**协议侧声明**
- **消费方**:`ADR-CEP-09` Decision A(通道依赖图无环判据)——
  ⚠️ **图顶点已同步升为 `(协议实例, network, vc_id)`**,见 `ADR-CEP-09 §8 Status Update`
  (这是**数学对象的变更**,不是措辞:两张网共享 `vc_id` 值域 0-3,顶点不带 network ⇒ 跨网依赖边被合并)
- **不承诺**:CEP **不自动生成** `vc_binding`
- **同一 `vc_id` 可绑定多个消息类**(per ArchForge `32 号 §6.0.2` ualink VC3 = `response` + `snoop_resp` + `forward_data`)
  → **不做唯一性约束**

### 8.3 不变量(取代后 + 新增)

- **`INV-CEP-12-IC2`(取代后)**: **逐网络**校验,对每个 network `n` ∈ `ordering_assumption` ∪ `vc_binding`:
  ① `ordering_assumption[n]` 的每个 `vc_id` ∈ `vc_binding[n].values()`;
  ② `vc_binding[n].values()` 中每个 `vc_id` **<** `fabric_config.networks[n].vc_count`(槽位上界);
  ③ `vc_binding[n]` 的每个消息类 ∈ **6 类规范词表** `{request,response,snoop,snoop_resp,atomic,query}`;
  否则 → **装配期 fail**(沿用 `ADR-CEP-11` 决策 3 的「不静默降级」处置)

  > ⚠️ **指针修正(2026-10-22)**:ArchForge `32 号 §6.4.2` 末句写「沿用 `ADR-CEP-12` **决策 3** 的『不静默降级』处置」
  > 是**错误指针** —— 「不得静默降级」出在 **`ADR-CEP-11:134/106`**;`ADR-CEP-12` 决策 3 是 `consistency_labels`。
  > 本仓引用一律以 `ADR-CEP-11` 为准。

  > ⚠️ **旧文(已被取代)**:`ordering_assumption` 的 `named_network` key 必须解析到唯一 `vc_id`,否则配置加载 fail
  > —— 嵌套形态下 network 即 key、vc_id 显式给出,**「唯一性」这一谓词整体消失**。

  > **6 类词表的补充约定**(IC2 条件 ③ 的展开,**不另立编号**):
  > - **明确排除**:`CATEGORY_CREDIT`(流控类别,非一致性消息类)/
  >   `forward_data`(canonical 流 6,是 `snoop` 槽的**承载实例**)/
  >   `snoop_completion_ack`(canonical 流 10,是 `snoop_resp` 槽的**承载实例**)
  >   → 一手依据:ArchForge `32 号 §6.0.1`
  > - **分层纪律**:6 类词表落在 **Coherence Interface 层**。
  >   `CATEGORY_*`(`include/bundles/noc_bundles_tlm.hh:60-62`)是**链路层 flit 类别**,
  >   **不得**为承载本词表而扩成 6 类 —— 那会把一致性语义搬进传输层(词表混层),
  >   且 `soc_internal_noc.hh:118` 的 `++vc_counts_[vc]` 分桶下标与「VC 0-3」值域语义会冲突。
  >   两者关系由 **adapter 处的显式映射表**承担(**非恒等**,Stage 1 交付)。
  > - **为何不另立新编号**:6 类词表是 IC2 条件 ③ 的**判据内容**,不是独立的判据族;
  >   若单列为 `INV-CEP-12-IC4`,会连带触发 **4 处 invariant 计数同步**
  >   (`07 号 §4` 总表 + `objective-cep-platform` DoD 7 的 15 组分解 + `improvement A2` + `phase-10` 交付预期)
  >   + canonical invariant 用例数 —— 超出本批「让判据 C-4 机器可验收」的范围,且会污染 A4/A6 批次的 diff。
  >   词表约束**照样机器可验收**:`scripts/test/cep_decl_alignment_check.sh` 第 ④ 项。

> **编号位说明**:`INV-CEP-12-IC1` ~ `IC3` 共 **3** 条(**IC2 文本已修订,数量不变**)。


### 8.4 登记备选(不实施):SLICC 式「协议本地虚拟网络 + 装配期绑定」

- **形态**:协议声明只携带**协议本地虚拟网络**(如 `fwd`/`resp`/`req`,各带 ordering 属性 ——
  正是 `.pcc` 的 `Network {}` 块);另设**装配绑定产物**承载 `(协议, vnet) → (network, vc_id)`
- **优点**:排序只声明一次(协议本地)、跨协议实例零重复、`ADR-CEP-09` 前提 3 从弱校验变强校验、
  C-4 的双向映射判据更干净(声明 vnet 集 ↔ 绑定表 ↔ fabric 网络集,**三段各管一段**)
- **代价**:推翻 ArchForge N-2 已交付的嵌套形态 → 跨仓谈判重开
- **触发条件**:Stage 1 出现**第 3 张网**或**第 3 个协议实例**时重评
- **为何现在登记**:当前为 **0 个 `implemented` 实例** —— 这是将来最便宜的改点,
  别让这个选项**无声死掉**

### 8.5 跨仓依据

| 项 | 值 |
|----|-----|
| 触发提交 | ArchForge `8290b4c`(N-1~N-4 裁决)+ `ac956b8`(D-9 批 1) |
| schema delta | ArchForge `32 号 §6.4.1`(:844-871 diff 原文) |
| 判据伪码 | ArchForge `32 号 §6.4.2`(:875-883) |
| per-network 矩阵 | ArchForge `32 号 §6.0.2`(:322) |
| 6 类词表 + 排除项 | ArchForge `32 号 §6.0.1`(:299) |
| 网络清单 + `vc_count` | ArchForge `22-nsa §0.5.4.1`(:144-152)+ NS-R1(:156)/ NS-R2(:157) |
| 本仓落地载体 | `include/tlm/cep/protocol_instance.schema.json` + `include/tlm/cep/fabric.{schema.json,json}` |
| 机器校验 | `scripts/test/cep_decl_alignment_check.sh` —— 5 项子校验:① 声明 ↔ schema **全 13 个 required 属性**(依赖 `jsonschema` 做全量校验;**刻意不降级运行** —— 缺依赖即报错退出,不静默跳过)②~⑤ per-network 语义规则(手写);**反向验证 12/12** |

---

## 9. Status Update(2026-10-22):R-2 已触发 + `perm` 形态约束(per 台账 Entry #13)

> **授权**:台账 `objective-cep-platform §11` **Entry #13** [adr-amendment];
> 触发 = ArchForge 答复回执(2026-10-22)§1.1 Q-3a(判据 (b) **成立**)→ R-2 **升格为「已触发」**。
> **本节不改 §2 决策 2 的 4 值锁** —— `perm` 的**值域**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`)
> 与 `ADR-CEP-02 CEN-Ib11` 的禁扩展不变量**零改动**;变的是**字段形态**,落 **Stage 1**。

### 9.1 R-2 状态

| 项 | 内容 |
|----|------|
| **R-2 需求本体** | `perm` 增「可静默升级」判别位 —— **不撤回** |
| **旧状态** | 「潜在表达力缺口,尚未证实触发」(R-2 采纳需求 / 论证待补) |
| **新状态** | ✅ **已触发** —— 判据 (b) 成立 → `moesi_gpu` 的静默 `E→M` 是域可见写 → 4 值 `perm` 确实无法表达「这是静默升级」 |
| **论证来源** | ArchForge 答复回执(2026-10-22)§1.1 Q-3a + Q-3b;宿主 ADR 状态见 `ADR-CEP-05 §13.1` |
| **本批处置** | **不改** `protocol_instance.schema.json`(`perm` 仍为 4 值枚举数组)。形态变更 = **Stage 1** 的 CEP 侧 schema 演进,登记为 `S1-7` |

### 9.2 `perm` 形态约束(per 对方 Q-3b 补充,本仓采纳)

新字段**必须**表达「**本行的 `UNIQUE` 投影可静默升为 `WRITEBACK`**」这一**具体**语义,
**不是**一个泛化的「可升级」布尔。理由:

| 转换 | 性质 | 对跨域传播的触发条件 |
|------|------|-------------------|
| `E → M` | **静默**升级(无需通知目录) | 写**跨域可见而未被传播** → 正是 (b) 成立的那一类 |
| `O → M` | **显式**升级(需 `INVALIDATE→others`,**发外部通信**) | 传播路径**已存在** → 不需要靠判别位补救 |

⇒ 泛化布尔会把二者混同 → 丢掉的正是判据 (b) 所依赖的那个区分。

### 9.3 路径标签澄清(本仓独立判定,与 ArchForge 的**可观测后果一致**)

ArchForge 在答复回执 `cep-interface-alignment-response-2026-10-22.md` §1.4 判定为 `ADR-CEP-11 决策 4` **路径 A**(不升版本号 / adapter 代码不动),
并请本仓确认。本仓**独立**核对后:

- **✅ 接受实质结论**:`protocol_version` **不升版**(`moesi-gpu-1.0` 保持)。
  `perm` 是 **CEP 侧** Crossbar Interface 投影词汇表(`ADR-CEP-02 CEN-Ib11`),
  **不是**协议规范的一部分;`protocol_version` 按 `ADR-CEP-11 决策 1` **只承载规范版本**;
  `moesi-gpu-1.0` 声明的协议语义(6 态 × 6 事件 / snoop 型)**完全未变**。
- **⚠️ 但不采用「路径 A」标签**:`ADR-CEP-11 决策 4` 的三条路径是「**规范版本升级**的处置路径」;
  R-2 落地**不构成规范版本升级** → **决策 4 不适用**(既非 A 也非 B),其「决策登记」义务**不触发**。
  若强行套用路径 A 的**动作**判据(「adapter 代码不动」)则**不成立** ——
  `perm` 产出方**必须**能区分静默/显式升级 → adapter/plugin 侧**必然有代码变更**
  (现状:`perm` **仅在 schema 中定义,无任何 C++ 实现** → 该变更整体属 Stage 1)。
- **净结论**:**可观测后果与对方完全一致**(版本号不动 / adapter 语义子集不动 / 不占 adapter change slot),
  差异**仅在标签与登记方式** → 记为「**决策 4 不适用;R-2 归 CEP 侧 schema 演进,落 Stage 1**」。
  详细论证见台账 `Entry #13 ②`。

### 9.4 未变更项(明示)

- **§2 决策 2** 的 `perm` **4 值锁** —— **零改动**(禁 `ATOMIC` 第 5 值;`CEN-Ib12` 仍不存在)
- **§2 决策 1** 的 schema 全量结构 —— **零改动**
- **`protocol_instance.schema.json`** —— **零改动**(本批)
- 本 ADR 的其余全部正文 —— **零改动**

---

**版本**: v0.2 草案 + Status Update 2026-10-22(§8 / §9)
**最后更新**: 2026-10-22
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
