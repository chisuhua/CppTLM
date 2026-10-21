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
| **计数会漂移** | 表内 `adapter 归属 = 新增(若选为实现目标)` 与"14 adapter 实例"两处数字并存,实现推进即失真 |

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
- **理由**:`07 号 §5.1` 的"14 adapter 实例"是**下界**(§5.1.4 的 5 个语义行标"新增(若选为实现目标)"),落地后变 18。若计数仍散落在 Markdown 表格里,实现推进即引发口径争议(per Oracle 独立复审 N3)
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

---

**版本**: v0.1 草案
**最后更新**: 2026-10-21
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
