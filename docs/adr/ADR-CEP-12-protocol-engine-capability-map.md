# ADR-CEP-12: ProtocolEngine 插件能力映射表 Schema

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **协议实例的能力 / 前提假设 / 组合能力的结构化声明 schema**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §5.1(协议族 → 实例 → adapter 映射)+ §5.1.4(协议语义索引)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
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

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 8. Status Update(2026-10-22):`ordering_assumption` / `vc_binding` 升 per-network 嵌套形态

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)——
> §2 决策 5 / 决策 6 与 §3 `INV-CEP-12-IC2` 的**正文不改**,**自本段起以本段文本为准**。
> 其 N-1/N-2 裁决把 VC 分配从**扁平**改为 **per-network 矩阵**,并给出 diff 级 schema delta(`32 号 §6.4`)。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` Entry #9(`adr-amendment`)
> **落地载体**: `include/tlm/cep/protocol_instance.schema.json` + `include/tlm/cep/fabric.{schema.json,json}`
> + `scripts/test/cep_decl_alignment_check.sh`(判据 C-4 的机器化)

### 8.1 取代范围

> **取代范围**:§2 决策 5 / 决策 6 / 前置 schema 草图、§4 抓取点 2、§6 决策汇总行、§7 修订记录行、
> §3 `INV-CEP-12-IC2` —— **取代后文本**见 §8.2 / §8.3;`IC1` / `IC3` **不变**。
>
> **兜底声明**:本 ADR 的 §2 / §3 / §4 / §6 / §7 全文中,凡出现「`<vc_id | named_network>`」
> 「解析到唯一 `vc_id`」「解析失败即 config fail」「key 绑定 `vc_id`」等**扁平形态旧谓词**者,
> **一律以本段为准** —— 该谓词在 per-network 嵌套形态下**整体消失**。
>
> **旧文留痕说明**:正文保留 2026-10-21 的原始表述(不可变约定);旧文与本段冲突时**以本段为准**。
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

> §2 决策 2 的 `perm` **4 值锁**(禁 `ATOMIC` 第 5 值;`CEN-Ib12` 仍不存在)· §2 决策 1 的 schema 全量结构 ·
> `protocol_instance.schema.json` · 本 ADR 其余全部正文 —— **全部零改动**。

## 11. Status Update(2026-10-28)—— 实例声明的**权威分区**(镜像域 vs 本仓域)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §10 正文一律不改,仅登记**一项决策**。
> **决策 —— `protocol_instances.json` 承载**两类**协议实例,权威来源不同**:① **镜像域实例** = ArchForge 32 号 §6.1/§6.2 声明者(权威 = ArchForge,逐字段镜像);② **本仓域实例** = 本仓自有口径者(权威 = 本仓,如 `07 号 §5.1.1` 工业族中的 CHI RN-F —— ArchForge 未声明该协议)。**分区由 `repo_owned_instance_ids[]` 显式登记**,不靠推断。
> **配套的闸门改造(两条,方向相反)**:① **镜像比对收窄** —— 逐字段比对只覆盖 `ArchForge ids ∩ repo ids`;本仓域实例**不再**被要求与 ArchForge 逐字段相等;② **语义校验扩面** —— ①~⑤(schema 13 属性 / `vc_count` 上界 / 词表 / 网络集 / 边端点绑定)**同样**覆盖本仓域实例。⇒ 收窄的是**镜像义务**,不是**校验义务**。
> **不变项(明示)**:§2 决策 1 ~ 10 的正文 · §3 `INV-CEP-12-IC1` ~ `IC3` 的**编号与数量**(3 条不变)· §8 的 per-network 嵌套形态 · §9.4 的 `perm` 4 值锁 · `protocol_instance.schema.json` **零改动**(分区键落**信封层**,不落实例层)—— 全部不变。**新增 invariant 数 = 0**。
> **为何不能走「另立声明文件」**:声明有 **6 处消费方**(装配闸 / 桥消费路径 / 装载器 / VC 占用快照 / 判据输入 / 测试),另立文件需逐处发明合并层,且会使本仓域实例**绕过**装配闸 ⇒ 违反「装配期校验实测」与 `INV-CEP-12-IC2`。
> **为何不能走「等 ArchForge 补声明」**:ArchForge 声明的是**它贡献的**协议;本仓域协议由 `07 号 §5.1.1` 的口径定义 ⇒ 权威归属须在本仓解决。
> **关联**:台账 `obj-cep-platform §11` **Entry #91**(变更依据与机器化判据登记)。

## 10. Status Update(2026-10-26,per 台账 Entry #71)—— `perm` 形态演进的**落点裁决** + 6→3 映射表的首个载体

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §2 ~ §9 正文一律不改,仅登记状态变化。
> **裁决 ① —— `perm_upgrade` 的落点 = 独立声明源,`protocol_instance.schema.json` **零改动****:新字段 `perm_upgrade.silent_upgrades[]`(`from`/`to` **跨 schema `$ref`** 取自 §9.4 所述的 `perm` **4 值锁**)落**新**文件 `include/tlm/cep/perm_upgrades.json` + 配套 schema,**诞生即不冻结**。**理由**:① `scripts/test/cep_decl_alignment_check.sh` ⑥ 项要求本仓 `protocol_instances.json` 与 ArchForge 声明**逐字段严格相等**(含「键只存在于一侧」判红)⇒ 该文件是 ArchForge **单点权威的派生副本**,不得承载本仓单方字段;② `perm` 的 4 值锁权威在本仓(`CEN-Ib11`),不在 ArchForge ⇒ 「协议**是什么**」与「协议**怎么投影到 stub cache**」本就分层;③ `protocol_instance.schema.json` 的既有 **13 属性断言零回归**(per `test/test_cep_schema.cc:234`)。
> **裁决 ② —— 6→3 消息类映射表 = adapter 的属性**:落 `MoesiGpuAdapter` 内(逐 adapter 不同 ⇒ 不建独立 JSON);**方向唯一**(6 类 → 链路层 `CATEGORY_*` 3 类);**非恒等**为存在性前提(至少一行 ≠ 恒等;若某实例确实恒等 ⇒ 须**显式声明 + 给出理由**);**`CATEGORY_*` 不扩**(§8.3 ② 的分层纪律维持)。
> **未变更项(明示)**:§2 决策 1 ~ 9 的正文 · §3 `INV-CEP-12-IC1` ~ `IC3` 的**编号与数量**(3 条不变)· §8 的 per-network 嵌套形态 · §9.4 的 `perm` 4 值锁 —— **全部零改动**。**新增 invariant 数 = 0**。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #71**
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #71)。

## 12. Status Update(2026-10-28)—— §11 的**五要素**补足 + 队列 #6 `status` 豁免的**追认**

> **性质**:追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §11 一律不改,仅**补足** §11 的表述形态,并追认一项**既已生效但当年未登记**的实践。

> **① 原前提**:§2 决策 1 + §10 裁决 ① 的隐含前提是「`instances[]` 的**全集** = ArchForge 声明全集」⇒ 本仓副本是**纯派生副本**,逐字段相等在结构上必然成立,⑥ 镜像闸据此要求「全集相等」。

> **② 新例外(实例级,不是字段级)**:`chi_rnf`(`07 号 §5.1.1` 工业族的 CHI RN-F 子集)由**本仓**声明 —— ArchForge **未**声明该协议 ⇒ 上述前提在**实例粒度**上首次不成立。分区键 = 顶层 `repo_owned_instance_ids[]`(**信封层**,不是实例字段 ⇒ 实例 schema 零改动)。

> **③ 不变项**:§2 决策 1 ~ 10 · §3 的 3 条 `INV-CEP-12-IC1` ~ `IC3` 的**编号与数量** · §8 的 per-network 嵌套形态 · §9.4 的 `perm` 4 值锁 · §10 / §11 的裁决 · `protocol_instance.schema.json` —— **全部不变**。**新增 invariant 数 = 0**。

> **④ 触发**:`cpptlm-cep-chi-rnf-subset` 交付**第 1 个**非 ArchForge 贡献的工业协议实例 ⇒ ⑥ 的「全集相等」前提失效,必须在不**放松**语义校验的前提下表达权威分区。

> **⑤ 机器化**:分区键由 `scripts/test/cep_decl_alignment_check.sh` 读取并执行**两条方向相反**的闸 —— 镜像比对**收窄**到 `ArchForge ids ∩ repo ids`(未登记的额外实例仍判红、登记 id 与 ArchForge 撞名判红),①~⑤ 的语义校验**扩面**到本仓域实例(增加受检对象,不减少约束)。

> **⑥ 追认(队列 #6 的**字段级**豁免)**:§2 决策 9 已定「`status` 是**计数口径**的唯一依据」⇒ 它是**本仓权威字段**,不是 ArchForge 的镜像面。⑥ 的逐字段比对因此**跳过** `status` —— 该实践自队列 #6 起生效但当年**未**追加本 ADR,本次一并追认。**豁免范围严格限于 `status` 一个键**,其余 **12** 键仍逐字段严格相等(类型严格、递归比较、键只存在于一侧即判红)。

> **关联**:§11(实例级权威分区的裁决正文)· 台账 `obj-cep-platform §11` Entry #91 / #92 / #94。

## 13. Status Update(2026-10-29,per 台账 Entry #96)—— 权威**迁移**子分类:`mesif` 的声明权威由 ArchForge 迁到本仓

> **性质**:追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §12 正文一律不改,仅登记**一项决策**。
> **触发**:队列 #9 `cpptlm-cep-mesif-adapter` 的 **R-1** 阻塞 —— ArchForge `32 号 §6.2:519-523` 声明 `mesif` 的 `sharer_tracker.impl = "SparseSnoopFilter"`(caps `SYSTEM/CONSERVATIVE/GLOBAL/HOME`),与同文件 `§9.1:716-741` 自陈的**三级目录**并存且**不自洽**;且 `SparseSnoopFilter` 已被 `moesi_gpu` 占用(ArchForge `docs/microarchitecture/snoop_filter.md:45-47` 明说「本模块**就是** CEP 声明中的 `SparseSnoopFilter` 实现」)⇒ `mesif` 继续声明它是**语义冲突**。
> **用户裁决**(2026-10-29):`mesif` 的声明权威**由本仓接管**(不走 ArchForge 侧撤回声明的跨仓路径)。
> **关联**:台账 `obj-cep-platform §11` **Entry #96**;`ADR-CEP-04 §13`(R-1 本体裁决:第 6 种实现 `HierarchicalDirectory`)。

### 13.1 决策 —— §11 的「两类」不变,新增**权威迁移**子分类 + 强制审计痕迹

§11 的两类划分(**镜像域** / **本仓域**)**不变**;本决策在其上给「本仓域」增加一个**可审计的子类**:**权威迁移** —— 即「该实例 id **曾**由 ArchForge 声明,现权威**已迁到**本仓」。

**选项对比**:

| 方案 | 结论 | 理由 |
|---|---|---|
| **A′** 新增顶层 `authority_migrations[]`(逐条:`instance_id` / `reason` / `ledger_entry` / `archforge_snapshot`);守门从它派生「迁移名单」;⑥ 对迁移名单**同样不适用**逐字段镜像,①~⑤ **同样覆盖**;**`reason` 必填 + 台账锚必填 + ArchForge 快照锚必填** | ✅ **采纳** | ① **审计可区分**「本仓自有协议」与「从 ArchForge 接管来的协议」—— 二者的**跨仓义务**完全不同(前者无,后者有追认义务);② **强制痕迹** ⇒ 接管**不是随手动作**:无理由 / 无台账锚 ⇒ 守门报红;③ ⑥ 的跳过集合 = `repo_owned` ∪ `迁移名单` ⇒ 守门改动面最小(与 §11 的两条方向相反的闸**同构**);④ `protocol_instance.schema.json` **零改动**(登记键落在**信封层**) |
| **B** 直接把 `mesif` 塞进 `repo_owned_instance_ids[]` + 放宽「撞名 ⇒ 红」 | ❌ 否决 | ① **稀释** §11 的语义:`repo_owned` 承载两类**跨仓义务完全不同**的实例(自有 vs 接管)⇒ 审计时无法区分;② 迁移**不可追溯**(看不出是接管、何时接管、为何接管);③ 「随手接管」无痕 ⇒ 守门失守 |
| **C** 不改机制,等 ArchForge 撤回或修正其 `mesif` 声明 | ❌ 否决 | 跨仓**无界阻塞** —— D-FABRIC 域永久阻塞(per ArchForge `32 号 §13` RK-3);与队列 #8 的 A′ 先例**方向相反**但机理相同(那里是「本仓新增协议、ArchForge 未声明」,这里是「本仓要修 ArchForge 已声明的协议」) |
| **D** 把 §11 的 `repo_owned` 语义**放宽**为「本仓权威(含接管)」,并用注释字段标注 | ❌ 否决 | 同 B 的稀释问题 + 追加式修订**自我否定**已签发条款的语义边界(不可变原则下应**新增**子类而非**改写**原义) |

**采纳方案的具体形态**:

| 项 | 值 | 落点 |
|---|---|---|
| 新增信封键 | `authority_migrations[]` = `{instance_id, reason, ledger_entry, archforge_snapshot}` | `protocol_instances.json` 顶层(envelope 层)⇒ `protocol_instance.schema.json` **零改动** |
| 迁移名单 | = `authority_migrations[].instance_id` 集合 | 守门脚本派生(不要求调用方维护第二份名单) |
| ⑥ 镜像比对 | 对 `repo_owned` ∪ `迁移名单` **同样不适用**;其余实例**仍逐字段镜像** | `cep_decl_alignment_check.sh` ⑥ |
| ①~⑤ 语义校验 | 对 `repo_owned` ∪ `迁移名单` **同样覆盖**(不放松) | 同上 ①~⑤ |
| 撞名闸 | `repo_owned` id 与 ArchForge **撞名且未登记迁移** ⇒ 红;**已登记迁移** ⇒ 绿 | 同上 |
| 新增判据 | ① 迁移 id 必须存在于 `instances[]` ② `reason` **非空** ③ `ledger_entry` 非空且台账可寻址 ④ `archforge_snapshot` 非空 ⑤ 同一 id 不得同时出现在 `repo_owned` 与迁移名单 | 同上 |

### 13.2 后果 / 代价

**正面**:① `mesif` 的声明可在本仓演进 ⇒ R-1(`ADR-CEP-04 §13`)的落地**不再被跨仓阻塞**;② ⑥ 不再对 `mesif` 误报(它本仓已声明为准);③ ①~⑤ 仍覆盖 ⇒ 本仓域实例**不是** schema 盲区;④ 迁移带**理由 + 台账锚 + ArchForge 快照锚** ⇒ 接管**可追溯**。

**负面(代价)**:① 本仓与 ArchForge 的 `mesif` 声明会**分叉**(ArchForge `32 号 §6.2` 仍写 `SparseSnoopFilter` + `SYSTEM/CONSERVATIVE/GLOBAL/HOME`;本仓将写 `HierarchicalDirectory` + `SYSTEM/EXACT/GLOBAL/HIERARCHICAL`)⇒ 该分叉**对 ⑥ 不可见**(⑥ 对迁移名单不适用)⇒ 只能靠**迁移登记 + 台账 + 跨仓追认**维持;② 守门新增 5 条判据(其中 3 条是「必填字段」);③ `protocol_instances.json` 的 envelope 多 1 个键 ⇒ 6 处消费方**零改动**即看见(per §11 的扩面纪律)。

**风险 / 已知残留**:
- ⚠️ **迁移机制本身是「接管通道」** ⇒ 若「理由必填」被弱化,就会退化为随手接管 ⇒ 该条已列为 V2/V6 的**反向 fixture**。
- ⚠️ **ArchForge 侧若将来撤回/修正 `mesif` 声明** ⇒ 迁移名单应**降级**回 `repo_owned`(或直接删除迁移登记)⇒ **该降级动作未机器化**(需人工裁决 + 台账 entry)⇒ 登记为**已知残留**。
- ⚠️ **跨仓追认是 ArchForge 侧的动作**(把 `32 号 §6.2` 的 `mesif` 声明对齐到本仓口径)⇒ 本仓**无权**修改 ⇒ 追认**未完成前**,两端声明**持续分叉** ⇒ 登记为**跨仓债**(非本仓可闭环项)。

### 13.3 Verification(前瞻判据,未实施)

- [ ] **V1** `protocol_instances.json` 顶层含 `authority_migrations[]`,且含 `mesif` 条目。
- [ ] **V2** 每条迁移记录含**非空** `reason` + `ledger_entry` + `archforge_snapshot`。
- [ ] **V3** 迁移名单 id 存在于 `instances[]`;且与 `repo_owned_instance_ids[]` **不重叠**。
- [ ] **V4** ⑥ 对迁移名单 id **不**要求逐字段镜像(与 `repo_owned` 同待遇);①~⑤ **仍覆盖**。
- [ ] **V5** 撞名闸:未登记迁移的 `repo_owned` id 与 ArchForge 撞名 ⇒ 红;登记迁移后 ⇒ 绿。
- [ ] **V6** **反向 fixture**:迁移 `reason` 为空 ⇒ 红;迁移 id 不存在 ⇒ 红;同一 id 同时在两个名单 ⇒ 红。
- [ ] **V7** **正控**:`moesi_gpu` / `mesif`(镜像域,**未**登记迁移)仍**逐字段镜像** ⇒ ⑥ 绿。

---

## Status Update(2026-10-30,per 台账 Entry #106)—— §8.4 的 SLICC 式备选**重评结论**

> **正文不改**(ADR 不可变;§8.4 的「登记备选(不实施)」原文保留)。本条登记**触发后的重评结论**。

### 触发

§8.4 的触发条件:「Stage 1 出现**第 3 张网**或**第 3 个协议实例**时重评」。
`implemented` 实例数已达 **3**(`moesi_gpu` / `chi_rnf` / `mesif`,队列 #9 归档后)⇒ **已触发**。

### 结论:**维持现行嵌套形态**(不迁移 SLICC 式)

| 维度 | SLICC 式(协议本地 vnet + 装配绑定) | 现行嵌套形态 | 判定 |
|---|---|---|---|
| 排序声明次数 | 1 次(协议本地) | 每实例每网络一次 | SLICC 优 |
| 重复是否已产生缺陷 | — | 机械重复(`mesif` 与 `moesi_gpu` 的 `compute_noc` 绑定逐字相同)| **否**(被 `INV-CEP-12-IC2` 三条件 + `cep_decl_alignment_check.sh` 锁住)|
| 装配期校验强度 | 前提 3 强校验 | 逐网络三条件 + 跨仓镜像闸,**已机器化** | 持平 |
| 代价 | **推翻 ArchForge `N-2` 已交付形态** ⇒ 跨仓谈判重开;需新增**装配绑定产物** | 无 | 嵌套优(Stage 1) |
| 消费者 | 装配绑定产物在 Stage 1 **无消费者**(fabric 侧尚无绑定面) | 队列 #4 已消费 | 嵌套优 |

### 选项**不注销**,改绑更强触发条件

在下列任一情形出现时**再评**:
1. 出现**第 3 张网**;
2. 任一实例需要**跨网络的联合排序**(即单实例无法用 per-network 绑定表达);
3. Stage 2 引入 fabric 侧**绑定面**消费者(届时 SLICC 式的收益才成立)。

> 当前 3 实例**均**可用 per-network 绑定完整表达:`mesif` 的 `atomic` / `query` 只在
> `ualink_fabric`、`snoop` 只在 `compute_noc` —— 属**网络特化**,不是联合排序。

### 关联

- 「承接项 2」滚动复核**第 2 次**:嵌套形态仍**充分**(per-network 即可表达);
  下次复核触发 = 第 4 个 `implemented` 实例;
- 完整评估表与残余风险见 `docs/architecture/07-coherence-exploration-platform.md`
  **Status Update 2026-10-30 第十六则** + `obj-cep-platform §9.4`。

---

## Status Update(2026-11-01,per 台账 Entry #138)—— **TileLink-C 的逐消息 → 6 类消息类归属表**(R-T3)

> **授权**:台账 `objective-cep-platform §11` **Entry #138** [adr-amendment] —— **冻结面授权记录**。
> **形态**:**纯追加**(本 ADR **正文不改**;决策 1 ~ 10 · §2 schema 全部保留)。
> ⚠️ **6 类消息类词表**与 **`protocol_instance.schema.json`** **零改动** ⇒ 本段只登记**归属表**与**降维**。

### ① 触发

R-T3:本 ADR 决策 6 的 6 类消息类词表 `{request, response, snoop, snoop_resp, atomic, query}`
**无**「对 completion 的 ack」这一档 ⇒ TileLink-C 的 `GrantAck` **无处可放**;
且 `writeback` 是 **canonical 10 流名**、**不是**消息类名 ⇒ **两套词表被混用**的风险须正面处置。

### ② 裁决:逐消息 → 6 类归属表(**零词表改动**)

| TileLink 通道 | 消息 | 归属类 | 理由 |
|---|---|---|---|
| **A** | `Acquire` | **`request`** | 语义即「请求获得权限」 |
| **B** | `Probe` | **`snoop`** | 语义即「查询 / 撤销副本权限」 |
| **C** | `ProbeAck` / `ProbeAckData` | **`snoop_resp`** | 对 `snoop` 的应答(方向 client→manager) |
| **C** | `Release` / `ReleaseData` | **`snoop_resp`** | ⚠️ **降维** —— 语义是「**自愿放弃**」,**不是**「应答」;但它与 `ProbeAck` **同方向同通道** ⇒ 归同类以保持「通道 ↔ VC」一一 |
| **D** | `Grant` / `GrantData` / `ReleaseAck` | **`response`** | 对 `request` 的响应(方向 manager→client) |
| **E** | `GrantAck` | **`response`** | 对 `response` 的确认;词表**无**「completion 的 ack」档 ⇒ 与 `Grant` **同类同 VC** ⇒ VC 内有序 ⇒ **不引入**跨 VC 依赖 |

⇒ ⇒ **`vc_binding` 逐 network 填写**:`request`→0 · `snoop`→1 · `snoop_resp`→2 · `response`→3
(配套的无环性论证见 `ADR-CEP-09` 的 `## Status Update(2026-11-01,per 台账 Entry #138)`)。

### ③ ⚠️ 2 处降维(**登记**,不得静默)

| # | 降维 | 说明 |
|---|---|---|
| 1 | `Release` / `ReleaseData` 归 `snoop_resp` | 语义是「自愿放弃权限」⇒ 词表**无**对应档 ⇒ 借 `snoop_resp`(同方向同通道) |
| 2 | `GrantAck` 归 `response` | 词表**无**「对 completion 的 ack」档 ⇒ 与 `Grant` 同类 ⇒ 同 VC ⇒ VC 内有序 |

⚠️ **「两套词表混用」的处置口径**:**canonical 10 流名**(`request` / `completion` / `snoop` /
`snoop_resp` / `writeback` / …)与**消息类名**(`request` / `response` / `snoop` / `snoop_resp` /
`atomic` / `query`)是**两套不同词表** ⇒ `writeback` **只**是流名,**不是**消息类名
⇒ **填 `vc_binding` 时不得写 `writeback`**(schema 的 `propertyNames` enum 也不含它 ⇒ 写了即装配期 fail)。

### ④ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR** | **零改动**(仅追加本段);**6 类消息类词表**与 **§2 schema** **全部保留** ⇒ `protocol_instance.schema.json` **零改动** |
| **队列 #11** | **R-T3 闭合** ⇒ `tilelink_c` 实例的 `vc_binding` 可逐类填写 |
| **平台自证能力** | ⚠️ 「5 通道 → 4 VC」的降维**仍然存在**(见 `ADR-CEP-09` 同 Entry 的 ⑤)⇒ 本段只解决**类归属** |

**关联**: 台账 `obj-cep-platform.md` **Entry #138**(授权记录)· #134 · `ADR-CEP-09` SU(同 Entry)
· `ADR-CEP-02`(canonical 10 流)· `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md` §4 / §7


---

## Status Update(2026-11-06,per 台账 Entry #239)—— `ualink_4` 实例声明的 **9 键取值裁决** + **路径 A 采纳**(声明实例)

> **性质**:追加式(正文 §1 ~ §13 **逐字不改**)—— 本段裁 `ualink_4` 实例声明中**上一轮未覆盖的 9 键**,
> 并裁「声明 vs ⏭ 登记」的路径。**上一轮已裁的 4 键**(`flow_support.snoop`/`snoop_resp` · `perm` ·
> `sharer_tracker.impl` · `consistency_labels`)**不在本段重复**,见 `ADR-CEP-04` **SU 2026-11-06**。
> **触发**:台账 **Entry #237**(事实底稿 + 三个裁决请求)· **#238**(D-1/D-2/D-3 裁决 + 冻结面授权)。
> **审查**:`.rddf/reviews/cep-b3a-b3b-ruling.md` 体系(**oracle → critic**,顺序不可颠倒);
> critic 结论 = **APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` ·
> **三档均无 BLOCKER** ⇒ 无需复审;其 **5 条 SHOULD_FIX** 已全部落进本段。
> **关联 change**:`backlog: cpptlm-cep-b3a-b3b-advancement`(占位,立项后回填)。

### ① 建模前提(**显式**,⛔ 不是脚注)

`ualink_4` 是「**无硬件一致性的 memory-semantic fabric**」——
⚠️ **该前提本身是 `C-10` 建模声明**(一步推断),⛔ **不是**事实(per 锚点摘要 `C-10`:
支撑 = 4 个互相独立的**公开非规范**来源;公开可钉版文本**无字面 "no snoop"**;
⛔ 本仓**未**读取任何 UALink 规范 PDF 技术正文,Evaluation Copy 同型约束照旧)。

### ② 9 键取值 + **认识论分级**(三档)

| # | 键 | 取值 | **认识论分级** | 锚 / 依据 |
|---|---|---|---|---|
| 1 | `family` | `industrial` | **锚定建模声明**(来源族判定) | `07 号` §5.1 口径 + `channel_dependency_graph.hh` 用 `family` 推导 per-bridge `protocol_id_`(CC-05 ④)⇒ `industrial` 是让该链闭合的唯一取值 + UALink Consortium = 产业联盟(对照 `tilelink_c` = `academic`:UC Berkeley 研究起源 / Chips Alliance 开源标准) |
| 2 | `flow_support.request` / `.completion` | `true` / `true` | **来源锚定(正向)** | `07 号` §5.1.1「TL Load / Store / Atomic」+ 4 个公开来源;⚠️ **≠ spec-pinned**(4 来源均为公开**非规范**) |
| 3 | `flow_support.snoop` / `.snoop_resp` / `.snoop_completion_ack` | `false` | **锚定建模声明**(负向 · 一步推断) | `C-10`;且三者**成套**(CEN-Ib3 / Ib4 / Ib10)⇒ 同真同假 |
| 4 | `flow_support.writeback` / `.forward_data` / `.back_invalidate` / `.address_resolve` | `false` | **锚定建模声明**(负向) | `C-10` + 无缓存写回 / 无目录转发 / 无反失效 / 无地址解析面(CEN-Ib5~Ib8) |
| 5 | `flow_support.barrier` | `false` | **锚定建模声明**(负向) | **依据已换**(见 ③ ③) |
| 6 | `atomic` | `{supported: true}`(省略 `intent_field`) | **来源锚定(正向)** | `07 号` §5.1.1 + `ADR-CEP-11 §3` 锁表;`intent_field` = 「request 流中承载 atomic 意图的**独立属性名**」⇒ UALink 由 **opcode** 区分(无独立属性)⇒ 省略(先例 `nvlink_4` 同样省略);⚠️ **≠ spec-pinned** |
| 7 | `ordering_assumption` | `{ualink_fabric: {0:unordered, 1:unordered, 2:unordered, 3:p2p_ordered}}` | **规则执行**(逐字拷贝) | `ADR-CEP-12` 决策 5(排序是 (network, vc_id) 的**静态属性**) + `C-8` 的锁定 VC 语义 + `moesi_gpu` / `mesif` 的 `ualink_fabric` 既有登记(**逐字相同**) |
| 8 | `vc_binding` | `{ualink_fabric: {request:0, atomic:1, query:2, response:3}}`(⛔ **去** `snoop_resp`) | **规则执行 + 负向删减** | 槽位与取值 = `C-8` + 既有登记(规则执行);**删减** `snoop_resp` 的依据 = `C-10` 的 `snoop=false`(⚠️ 该删减是**负向**成分,⛔ 不是既有登记本身) |
| 9 | `combination_capabilities` | `{concurrent:true, hierarchical:false, heterogeneous:true}` | **锚定建模声明**(**维度借用**) | `C-10` + ⚠️ **维度借用**显式登记(`ADR-CEP-12` 决策 8 的三维来自调研 F16,面向 **coherence 协议** ⇒ 用于无一致性 fabric 属**类比延伸**) |

**其余 3 键(机械可推导,非建模选择)**:`adapter` = `UalinkAdapter` · `status` = `implemented` · `protocol_version` = `ualink-2.0`。

> **新增 invariant 数 = 0** —— 本段裁的是**实例取值**,不动 `INV-CEP-12-IC1`~`IC3` 的编号与数量(照 §11 / §12 / §13 先例句式)。

### ③ 逐项依据要点(争议项的改写)

**③-1 `family`** —— ⛔ **不是装饰**:装配期用它推导 per-bridge 的 `protocol_id_`
⇒ 取值错误会**静默**影响 bridge 绑定链;`industrial` 是该链闭合的唯一取值。

**③-2 `flow_support` 无漏项** —— atomic **不需要**独立流:`ADR-CEP-17` CEN-Ib11 规定 atomic 意图经
`request` 流的**独立属性**承载,`vc_binding` 的 `atomic` 消息类负责 VC 分配
(先例:`nvlink_4` = atomic supported 而**无** `intent_field`、**无**独立流 ⇒ 已闭环)。

**③-3 `barrier = false` 的依据(⚠️ **本轮已换** —— 换之前的理由是错的)**

| | 内容 |
|---|---|
| ⛔ **作废的理由** | 「INC 的同步是**集合级** ⇒ 归 `collective_barrier` 平面 ⇒ canonical `barrier` 流 false」—— **范畴错误**:两平面**正交**(per `ADR-CEP-02` §2 决策 B),且被**先例反证**:`nvlink_4` 声明 `barrier: true`,而 NVLink SHARP 的 rendezvous barrier **同样**被判进 `collective_barrier` 平面 ⇒ 「有集合级同步平面」与「canonical `barrier` = true」在既有登记中**共存** |
| ✅ **现行理由** | canonical `barrier` 是 **requester-scoped ordering point**(drain / visibility,per `ADR-CEP-02` 决策 3);4 个公开来源对 UALink 有无 fence/drain 原语**完全沉默**;在 ① 的前提(software coherency)下协议层**无** barrier 消息 ⇒ **负向建模声明**挂 `C-10`,认识论等级 = 一步推断 |
| **残余弱点(诚实登记)** | 「**沉默非证据**」—— 该论证的强度**不**高于同批的其它负向声明 ⇒ 已按同一分级登记,⛔ 不升格 |

**③-4 `combination_capabilities` 的安全论证(⚠️ **依据已改正**)**

- ⛔ **作废的引用**:`INV-CEP-13-IC2` 是**跨域写传播**判据(域可见写 (a)+(b)),与 `hierarchical` 维度**无直接关系**
  ⇒ 不得用它论证本键的安全。
- ✅ **现行依据**:`hierarchical` 维度**当前无已落地的机器判据**消费它(`07 号` §7 的跨域组合判据为**人工声明**,
  尚未落地)⇒ 取 `false` **不产生任何闸效应**;且该维度的**实例级评估集为空** ⇒ 无回归面。
- ⚠️ **鉴别力声明**:存量 7 实例该三维**恒为** `{true, true, true}` ⇒ 本取值是**首个** discriminating 值;
  它与 `sharer_tracker.impl = 不参与` / `perm = ["NONE"]` 形成**三重一致**的「无一致性」声明
  ⇒ 登记为「**声明入口 + 待实验证伪**」姿态(照 Entry #215 的同族 posture),⛔ 不宣称已由实验证实。

### ④ 路径裁决 = **路径 A**(声明实例)

| 方案 | 结论 | 理由 |
|---|---|---|
| **A** 声明 `ualink_4` + 计数链 7→8 | ✅ **采纳** | ① schema 的 **13 键全部 required** ⇒ 「留白」**无合法表达**,路径 B 只是把留白从 JSON 挪进台账;② 路径 B 的原前提「取值不可判定」已被 4 个公开来源 + `C-10` **部分证伪**;③ 残余弱键(`barrier` / `combination_capabilities`)两条路都躲不掉,路径 A 至少让它们以**带认识论标签的声明形态**入库,可被未来实验证伪 |
| **B** 不声明 + 登记第 8 实例 ⏭ + 「8 实例」→「7 + 1 ⏭」 | ❌ 否决 | 使 B3a 以「⏭ 阻塞」成为**终态**(DoD 永不闭合);「6 族 / 8 实例」与 JSON 实例数**维持**既有不一致(⛔ 该不一致**已存在**,路径 A 是**修复**它);登记理由「语义无对应物」在 `C-10` 已签后**半过时** |
| **C** 声明但 `status="planned"` | ❌ 排除 | schema 允许,但**不能**避免计数 churn(存量断言数的是**实例总数**而非 `implemented` 数)且**低报**事实(`UalinkAdapter` 已落地、装配可达已成立) |
| **D** schema 加 `unknown` / `null` 值 | ❌ 排除 | 真正理由 = **为 2 个弱键把 boolean 改 nullable 的全局语义收益为负**(不是「不能动冻结面」—— 冻结面**可以**走 `--update --authorized-by` 授权修改);且违背「悬空判据禁令」先例 |

**回滚路径**(显式):整体回退 = 删 `ualink_4` 实例条目 + `repo_owned_instance_ids` 除名 +
`sharer_tracker_rationales` 除名 + **计数链全回退**(5 个测试文件的存量断言 + `07 号` 两处 + objective 台账) +
`C-10` 的 `N-8` / `N-9` 转正**不随之回退**(它是**事实登记**,与实例声明解耦)⇒ 回滚**不产生新的不一致**。

### ⑤ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR 正文 §1 ~ §13** | **零改动**;`protocol_instance.schema.json` **零改动**(13 键全 required + 枚举已含全部候选取值 ⇒ 不触发冻结面授权) |
| **`protocol_instances.json`** | **非** M1 冻结件(但受 ⑥ 镜像闸约束)⇒ 实例 **7 → 8** · `implemented` **7 → 8** · `repo_owned_instance_ids` **增员**(⛔ **同 commit** 必须增员,否则 ⑥ 报「未登记的额外实例」)· `sharer_tracker_rationales` 增 `ualink_4` 条目 · `no_upgrade_instances` **3 → 4** |
| **`no_upgrade_instances` 的派生序** | `derive_no_upgrade_instances` 按 **instances 数组顺序**过滤 ⇒ `ualink_4` **追加到数组末尾** ⇒ 派生序 = `[cxl_3_0_symmetric, tilelink_c, nvlink_4, ualink_4]` ⇒ **`ualink_4` 在 `at(3)`**(⚠️ 现有断言的 `at(0..2)` **非** JSON 顺序,勿凭直觉推) |
| **ArchForge 侧** | **零义务** —— ⑥ 镜像比对只覆盖 `ArchForge ids ∩ repo ids`,`repo_owned` 实例**豁免**;跨仓 grep 实测 ArchForge **无** `ualink` 声明 ⇒ 无撞名 |
| **装配面** | `snoop=false` **首实例**(现存 7 个全 `true`)⇒ 装配闸首次在 snoop-less 声明上运行;实测:装配闸 ①~⑤ **不消费** `flow_support.snoop` ⇒ **无新代码路径**;`perm=["NONE"]` ⇒ `exposes_unique_perm=false` ⇒ `INV-CEP-08-IC2` 走 **vacuous PASS**(detail 文字**已实测准确**) |
| **台账 / 文档同步** | `obj-cep-platform.md` 的 B3a 提及位 **≥ 6 处**(§6.2 B3a 行 / §4.5 推进状态 / 规划前提 / §6.2 终态表 / 待办指针 / 规划段)—— ⚛️ **终态表「仍阻塞 = B3b + B3a」句必须同批改写**,否则 `Entry #238` 刚闭合的 **D-3 双记账立即复发**;`07 号` 的计数 / 状态字样行 **≥ 7 处**(§5.1.1 标题「6 族 / 8 实例」/ §5.1.1 UALink 行 / §5.1.1 计数脚注 / §5.1.1 状态脚注 / §5.1.1 adapter 实例行 / §11 adapter 实例行 / §11 schema 行)⇒ 追加 `ualink_4` 实例是**修复**既有不一致 |
| **「声明 ≠ 装配」** | 实例**声明**落地 ≠ **实例级装配**闭环(装配仍需把 `ualink_4` 挂进拓扑 + collective plane 绑定)⇒ `07 号` §5.1.1 UALink 行**须保留**「实例级装配待 backlog」措辞,⛔ 不得被读成「已闭环」 |
| **代价** | ① **5 个测试文件**的存量断言翻转;② 计数链 3 处计数 + 2 处文档;③ `ualink_4` 的**证据强度低于**其余 7 实例(3 键为一步推断 + 1 键为维度借用)⇒ 须**显式登记**;④ `no_upgrade_instances` 3→4 须回查与 `ADR-CEP-09` VC 依赖集的**隐性冲突**(排除理由必须可审计) |

### ⑥ Verification(前瞻判据 · 可勾选)

- [ ] `ADR-CEP-04` **SU 2026-11-06** 已加**双向**交叉引用(本段 → 该段 4 键;该段 → 本段 9 键)⇒ 读者不会误以为 13 键全裁于一处
- [ ] `protocol_instance.schema.json` **零改动**(冻结面未触发 `--update --authorized-by`)
- [ ] `ualink_4` 实例已声明(13 键)· `repo_owned_instance_ids` **同 commit** 增员 · `sharer_tracker_rationales["ualink_4"]` 已落
- [ ] **决定性变异**:`cep_decl_alignment_check.sh` 在「含 `ualink_4` 且已登记 `repo_owned`」下 **⑥ 段不红**
  (同时证明镜像豁免生效 + ArchForge 零义务 + 计数链一致)
- [ ] **反向 fixture**:从 JSON 删掉 `ualink_4` ⇒ ⑥ 的「未登记的额外实例」检查**按正确原因红**
- [ ] **UIA-07 翻转**(`test_cep_ualink_inc.cc` 的「`ualink_4` 不声明」负向 fixture ⇒ 正控)⇒ 同 commit 内**保留一条「负向 witness」注释**(登记「本断言 2026-11-06 由『不声明』翻转为『声明』,系**有意**变异点」)
- [ ] 5 个测试文件的存量断言已翻转(`test_cep_tilelink_instance_decl.cc` · `test_cep_mesif_instance_decl.cc` · `test_cep_cxl_dcoh_instance_decl.cc` · `test_cep_moesi_gpu_adapter.cc` · `test_cep_ualink_inc.cc`)⇒ `no_upgrade_instances` **size 4** 且 `at(3) == "ualink_4"`
- [ ] `obj-cep-platform.md` 的 **≥ 6 处** B3a 提及位已同批改写(⚛️ **含终态表「仍阻塞」句**)
- [ ] `07 号` 的 **≥ 7 处**计数 / 状态行已同步,且 §5.1.1 UALink 行**保留**「实例级装配待 backlog」措辞
- [ ] `ADR-CEP-09` 的 `no_upgrade_instances` 3→4 回查结论已登记(排除理由可审计)
- [ ] `cep_decl_alignment_check.sh --strict` 绿 · `cep_freeze_check.sh --strict` 绿 · `cep_prereg_drift_check.sh --strict` 绿 · `cep_docs_sync_check.sh --strict` 绿 · `cep_invariant_map_check.py --check --for-achievement` 绿(66/66 = 61 + 5 + 0,⚠️ **确认 ADR-CEP-12 的不变量计数未漂移**)· `[cep]` 全绿

**关联**:台账 **Entry #239**(本裁决 + 路径 A)· **#238**(D-1/D-2/D-3 裁决 + 冻结面授权)· **#237**(事实底稿)· **#233** · **#231** · **#215**(声明入口 + 实验证伪 posture)·
`ADR-CEP-04` **SU 2026-11-03**(候选集 + 理由登记)+ **SU 2026-11-06**(`ualink_4` 4 键 + O1/O2)·
`ADR-CEP-02` §2 决策 3 / 决策 B + **SU 2026-11-02 R-T4** + **SU 2026-11-06**(`perm` 语义)·
`ADR-CEP-11 §3`(锁表)· `ADR-CEP-17` CEN-Ib11 · `ADR-CEP-09`(VC 依赖集)·
`docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md` §0 / §2bis(`C-10`) / §3 ·
`docs/architecture/07-coherence-exploration-platform.md` §5.1.1 / §7 / §11 ·
`.rddf/reviews/cep-b3a-b3b-ruling.md`(审稿报告体系)


---

## Status Update(2026-11-06,per 台账 Entry #239 —— **订正 #1**)—— `ualink_4` 的 `vc_binding` **改为逐字全取**(含 `snoop_resp`)

> **性质**:追加式**订正**(per ADR 不可变 —— 上一段 SU 2026-11-06 **逐字保留不改**)。
> **触发**:builder 实施阶段的**决定性实测** —— 声明 `ualink_4` 后,`cep_collective_plane_contract` 的
> **CP-03** 与 **V6-d 四条证书**判红:
> `[CP-03-VC-CERTIFICATE/C-1′] 网络 `ualink_fabric` 的实例映射**类集合不一致** ⇒ 继承不良定义`。
> **审查**:同段的 oracle → critic 链**未覆盖**该点 ⇒ 本订正经 **critic 复核**(判据同 §② 分档纪律)。

### ① 订正内容

| 项 | 上一段 SU 的值 | **订正后的值** |
|---|---|---|
| `ualink_4.vc_binding.ualink_fabric` | `{request:0, atomic:1, query:2, response:3}`(**去** `snoop_resp`) | **`{request:0, atomic:1, query:2, response:3, snoop_resp:3}`**（**逐字全取** `moesi_gpu` / `mesif` 的既有登记) |
| 该键的**认识论分级** | 「规则执行 **+ 负向删减**」(删减依据 = `C-10`) | **纯「规则执行」** —— ⛔ **无**负向删减 ⇒ 分档**变简** |

### ② 订正依据(实测 + 判据原文)

- **判据原文**(`include/tlm/cep/collective_plane_contract.hh`,**C-1′「槽位语义继承」**):
  同网络上**所有有 `vc_binding` 的实例**,其**类集合必须完全一致** ——
  `other.size() != first.size()` ⇒ FAIL(「实例映射**类集合不一致** ⇒ 继承不良定义」);
  且每个类的 `vc` 取值也必须相同。
  ⇒ 上一段 SU 的「去 `snoop_resp`」使 `ualink_4` 的类集合 = 4 ≠ `moesi_gpu` / `mesif` 的 5 ⇒ **判红**。
- **语义澄清(本次订正的认知收获)**:`vc_binding` = 「**消息类 → `vc_id` 的映射**」(per 本 ADR §2 决策 6)——
  它描述的是「**若**该消息类出现,它走哪个 VC」,⛔ **不是**「该实例承载哪些流」
  (那是 `flow_support` 的职责,per 本 ADR §2 决策 1)。
  ⇒ **列出本实例不承载的流类是允许且必要的** —— 否则 C-1′ 的「继承良定义」被破坏。
  ⇒ `ualink_4` 的 `flow_support.snoop_resp = false` 与 `vc_binding.snoop_resp = 3` **并存不矛盾**:
  前者说「本实例**不承载**该流」,后者说「该流在本网的 **VC 归属**」。
- ⛔ **本订正不改变** `flow_support` 的任何取值(`snoop_resp = false` **维持**)、**不改变** `perm` / `impl` /
  `consistency_labels` / `family` / `atomic` / `ordering_assumption` / `combination_capabilities`。

### ③ 后果 / 代价

| 面 | 后果 |
|---|---|
| **上一段 SU** | **逐字不改**(append-only);本段为**订正**,冲突处以本段为准 |
| **`vc_binding` 的可执行性** | `ordering_assumption` 的 4 个 VC(0..3)**全覆盖** ⇒ 「ordering ⊆ vc_binding(逐网络)」(CP-03 的 ②)仍成立;类集合与同网实例**逐字相同** ⇒ C-1′ **绿** |
| **代价** | `ualink_4` 的声明里出现一个「本实例不承载」的 VC 归属 ⇒ ⛔ **不得**把它读成「UALink 承载 snoop_resp 流」⇒ 该语义由 `flow_support` 独占;**此点须在实例旁的理由登记里写明**(per 载体 `sharer_tracker_rationales` 的同族纪律) |
| **新增 invariant 数** | **0**(同上一段) |

### ④ Verification(前瞻判据 · 可勾选)

- [ ] `ualink_4.vc_binding.ualink_fabric` = `{request:0, atomic:1, query:2, response:3, snoop_resp:3}`(与 `moesi_gpu` / `mesif` **逐字相同**)
- [ ] `cep_collective_plane_contract` 的 **CP-03** 与 **CP-06** **绿**;**V6-d 四条证书**(`C-1′` / `C-2` / `C-3` / `C-4`)**全绿**
- [ ] `flow_support.snoop_resp = false` **维持不变**(与 `vc_binding.snoop_resp = 3` 并存不矛盾,见 §②)
- [ ] `ordering_assumption` 的 VC 覆盖 ⊆ `vc_binding` 的 VC 覆盖(逐网络)
- [ ] 实例旁的理由登记写明「`vc_binding` 是 **VC 分配映射**,⛔ 不是 `flow_support` 的重复声明」

**关联**:本 ADR **SU 2026-11-06**(被订正的那一段)· 台账 **Entry #239** · `ADR-CEP-01` **SU#4 E.3.1 / E.6**(C-1′ 与 V6-d)·
`include/tlm/cep/collective_plane_contract.hh`(`C-1′`「槽位语义继承」判据本体)· `ADR-CEP-04` **SU 2026-11-06**


---

## Status Update(2026-11-06 · 四,per 台账 Entry #250)—— `backlog:` 占位**回填**(⛔ 不改任何决策内容)

> **性质**:纯**指针回填**(append-only)。⛔ 不 reopen 任何决策、⛔ 不改正文、⛔ 不新增 invariant 编号。
> **原占位**:`backlog: cpptlm-cep-b3a-b3b-advancement`(占位,立项后回填)
> **回填**:该 backlog 条目已交付 —— `openspec/changes/archive/2026-11-06-cpptlm-cep-tilelink-t3-mapping-survey/`(T3 映射锚点普查,✅ 已完成归档)+ `openspec/changes/archive/2026-11-06-cpptlm-cep-tilelink-runtime-engine/`(TileLink-C 运行时引擎,✅ 已完成归档);剩余 4 族处置见 `07 号` **SU 2026-11-06 · 四** + 台账 **Entry #249**。


---

## Status Update(2026-11-06,per 台账 Entry #258)—— `ualink_4` 的**实例级装配**闭合(「声明 ≠ 装配」缺口)

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文与既有 SU;⛔ 不改 §5.1.1 之外的契约。
> **触发**:台账 **Entry #240** 登记的 backlog(「声明 ≠ 装配」+ 本 ADR **SU 2026-11-06 ④** 后果表的「实例级装配待 backlog」措辞)+ 用户裁决「按建议顺序实施」。
> ⚠️ **与本 ADR 既有 SU ④ 后果表的关系**:该表要求「`07 号` §5.1.1 UALink 行**须保留**『实例级装配待 backlog』措辞」—— 本 SU 记录**该措辞已被后续授权更新**(台账 **Entry #258**,2026-11-06)⇒ 两段**并存不矛盾**(append-only 语义:后段登记状态推进,⛔ 不回改前段)。
> **关联 change**:`openspec/changes/cpptlm-cep-ualink-instance-assembly/`(归档后指向 archive 路径)。

### ① 「声明 ≠ 装配」的三层证据(判据形状)

| 层 | 证据 | 缺失 ⇒ 归因 |
|---|---|---|
| ① 声明层 | `protocol_instances.json` 的 `instances[]` 有该 id + `family` 可解析 | `[IAG-02]` / `[IAG-03]` |
| ② 拓扑层 | 配置 `cep.bridges[].protocol_instance` 命中 + bridge 的 `modules` **确实在拓扑中** + `bridge_id` 唯一 | `[IAG-06]` / `[IAG-07]` |
| ③ 平面层 | `collective_planes.json` 的 `plane_binding[instance]` **非空** 且 plane 名**已登记** | `[IAG-04]` / `[IAG-05]` |

⇒ 装配闸 = `include/tlm/cep/instance_assembly_gate.hh`(新公共头,`GateReport` 同型)。

### ② `ualink_4` 的装配事实

- 拓扑:`configs/cep_ualink_pe_minimal.json`(2 个 `CohIfProtocolEngine` + `cep.bridges[].protocol_instance = "ualink_4"`,`bridge_id = 3`);
- 平面:`plane_binding["ualink_4"] = "FabricCollectivePlane"`(承载真实 fabric 集合流量);
- ⚠️ **流语义未闭环**:`request` / `completion` 的**专用承载引擎**仍缺(现由 Null 实现承载拓扑层)⇒ 本 SU 只闭合「装配」,⛔ 不宣称协议能力完整。

### ③ 遗留

- `nvlink_4` 有 plane 绑定但**无拓扑挂载** ⇒ 不在本闸职责内(闸只对「配置中挂载了的实例」提要求)⇒ 登记为**独立遗留**;
- UALink 专用 coherence 引擎(承载 `request`/`completion`)⇒ **后续 change**。


---

## Status Update(2026-11-07,per 台账 Entry #261)—— 装配闸 ③ 平面层判据的**语义修正**(契约对齐)

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文与既有 SU(尤其 **SU 2026-11-06**(Entry #258)的 ① 三层证据表 —— **不回改**)。
> **触发**:critic 对 P2/P3 的补复核 **REJECT**(1 BLOCKER)—— P3 在 P2 归档后改判了 ③ 的语义,但冻结契约未同步 ⇒ 契约与实现/测试**自相矛盾**。
> **审查**:`.rddf/reviews/cep-instance-assembly-and-l4-e2e-verify.md`(验收)· 本轮 critic 复核(REJECT → 本 change 闭合)。

### ① 修正内容(③ 平面层)

本 ADR **SU 2026-11-06**(Entry #258)① 的 ③ 写「`plane_binding` 中的绑定**非空**且**已在 plane 注册表登记**」——
该表述是**无条件**的。实际实现(per 台账 **Entry #259 §⑤** 的「顺带修正」)为:

- `plane_binding` 含该实例键且值**非空** ⇒ 必须**已登记**(否则 `[IAG-05]`);
- 值为 **null / 空** ⇒ 该实例**不启用 collective**(**合法状态**,如 `moesi_gpu` / `mesif`)⇒ **不判红**;
- **仅当**该实例被调用方经 `required_plane_instances` **显式要求绑定**时,空 / 缺失绑定 ⇒ `[IAG-04]`。

### ② 修正理由(假阴性)

CXL / MOESI-GPU / MESIF 走**软件一致性 / 无 collective 能力** ⇒ 强制非空绑定会把**合法**的「不启用 collective」误判为装配失败(假阴性)⇒ 修正方向成立。

### ③ 「required 清单」的权威归属

该清单 SHALL 由**调用方**显式传入,闸**不自行推断**(那是协议语义判断,归 ADR / 设计文档)⇒ 避免「判据可被调用方静默稀释」。
当前唯一权威调用点:`test/test_cep_instance_assembly.cc` 的 IAG-01 正控(`ualink_4` **required**)与 `test/test_cep_moesi_gpu_engine.cc` 的 MGE-05(`moesi_gpu` **不** required)。

### ④ 契约面

`openspec/specs/cep-instance-assembly/spec.md` 的 Requirement ③ 与 IAG-01/02 已按本 SU **改写**(MODIFIED,per change `cpptlm-cep-instance-assembly-contract-alignment`)⇒ 与实现/测试逐条一致。

---

## Status Update(2026-11-07,per 台账 Entry #269)—— UALink 专用 coherence 引擎的**强度裁决** = **记账级专用承载**(bookkeeping-only)

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文 §1 ~ §13 与**任何**既有 SU(尤其 **SU 2026-11-06**(Entry #239)的 9 键取值 + 订正 #1 · **SU 2026-11-06**(Entry #258)的装配三层证据 · **SU 2026-11-07**(Entry #261)的 ③ 平面层修正 —— **逐字保留不回改**)。
> **触发**:台账 **Entry #268**(第 2 项立项 + 三选项待裁)· 用户裁「引擎强度 = **甲**」· **oracle** 架构咨询(4 问,本轮)。
> **审查**:`oracle`(强度边界 + 判据有牙性咨询)⇒ 采纳其 4 个落点;`critic`(本段 + `07 号` §5.1.1 UALink 行精确化 的架构审稿)。
> **关联 change**:`openspec/changes/cpptlm-cep-ualink-coherence-engine/`(实施面;本段只裁**边界**,不裁实现细节)。

### ① 建模前提(回指,**不重复论证**)

`ualink_4` 是「**无硬件一致性的 memory-semantic fabric**」—— ⚠️ 该前提本身是 `C-10` **建模声明**(一步推断),⛔ **不是**事实(per **SU 2026-11-06**(Entry #239)①)。

⚠️ **本段的依据档位 = `public_nonnormative`**(4 个互相独立的公开非规范来源 + 6 条一步推断,per 锚点摘要 `docs/research/2026-11-07-cep-ualink-flow-semantics-anchor-summary.md`)⇒ ⛔ **不构成 `Direct`**、⛔ **不计入 B3b**(per `07 号` **SU · 四 D-1**)⇒ **B3b-2 维持 0/4,本段不改任何计数**。

### ② 引擎强度裁决

`UalinkProtocolEngine` = **记账级专用承载**(bookkeeping-only dedicated carrier)。其对 canonical 10 流的全部行为 = **计数器自增**;⛔ **零发射**(zero egress)、⛔ **零协议状态机**、⛔ **零 UPLI 通道级语义**(`ReqCmd` 编码 / 4 通道时序 / `transaction tag` 配对 **均不实现**)。

⚠️ **本引擎不构成「UALink 协议语义已实现」的任何证据**;**B3b 的 UALink 行维持 ⏭ 受限,计数不变(仍 0/4)**。

> **术语纪律**:「**弱档**」**不得单独出现** —— 必须全称「**公开非规范来源 + pending + fail-closed**」。「纯记账」不得写作「简化实现」「基础实现」「部分实现」。

### ③ 字面禁令清单(必须写进凡本引擎出现的文档面)

| ⛔ **不得使用** | ✅ **只许使用** |
|---|---|
| 「实现 / 承载 UALink 语义」 | 「记账」 |
| 「`request` / `completion` 语义已落地」 | 「登记」 |
| 「已支持 UALink」 | 「可观测」 |
| 「UALink 协议引擎(实现 …)」 | 「专用承载(记账级)」 |
| 「引擎处理 UALink 事务」 | 「装配闭合」 |

**两条触发式禁令**(出现即越界):

- **J1 相关**:任何「completion 有序」表述必须全称「J1 的 `p2p_ordered` 是**建模假设**(假设 completion 有序以便定位排序点),**非协议保证**」(per 锚点摘要 §三 `ordering_assumption` 行口径);
- **`vc_binding` 相关**:`snoop_resp` 走 VC3 的任何表述必须连带「`vc_binding` 是**消息类 → VC 的分配映射**,⛔ **不是** `flow_support` 的重复声明」(per 本 ADR **决策 6**)。

### ④ 观测面形状(**三分法**,非 M-6「一律记账」)

⚠️ **本段不复用 `MoesiGpuProtocolEngine` 的 M-6「10 流一律只记账」形态** —— 理由:那会让引擎观测面与 `flow_support` 声明**矛盾**(8 条声明 `false` 的流被正常记账)⇒ 复犯 `07 号` **SU · 四 D-1** 点名的「静默 … 且看起来权威」失败模式。
✅ **既有范式**:`MesifProtocolEngine::gate()`(「声明 `false` ⇒ 拒绝计数,不静默降级」)⇒ **三分法 = 与声明对齐,引入的语义维度为零**。

| 状态 | 适用流 | 计数器 |
|---|---|---|
| **记账**(声明 `true`) | `request` · `completion` | `flow_events[10]`(逐流抵达;⚠️ **`flow_events` 对**全部 10 流**计抵达**,不限于本行的 2 条 —— 否则不变量 2 / 3 在抵达时不可满足) |
| **拒绝**(声明 `false`) | `snoop` · `writeback` · `forward_data` · `back_invalidate` · `address_resolve` · `barrier` · `snoop_completion_ack`(7 条) | `unsupported_rejected[10]`(**per-flow**,⛔ 非共享标量 ⇒ per-flow 归因) |
| **pending 不消费**(语义待定) | `snoop_resp`(1 条) | `snoop_resp_pending_not_consumed`(标量;**名字自带否定语义**) |

**机器可判不变量**(4 条,随契约面落):

1. `unsupported_rejected[Request] == 0 ∧ unsupported_rejected[Completion] == 0` —— 被声明支持的流**永不被拒**;
2. `∀f ∉ {request, completion, snoop_resp}: flow_events[f] == unsupported_rejected[f]` —— 声明不支持的流**每次抵达都被拒绝**,且**零其他可观测效果**(外加**零发射**断言);
3. `flow_events[SnoopResp] == snoop_resp_pending_not_consumed` —— 每次抵达**恰好一次** pending 登记,零其他效果;
4. `completion_unmatched_ == 0`(正常运行路径)—— completion 的 `txn_id` 恒等匹配(见 ⑤)。

### ⑤ completion 记账**不依赖有序性**(判据须**有牙**)

⚠️ **判据退化警告**:若引擎是**纯计数器**,则「乱序投递 ⇒ 断言不变」**恒真**(整数自增可交换)⇒ **判据无牙** ⇒ verifier 按字面执行即「测了个寂寞」。
✅ **本段要求**:引擎维护**最小 order-free per-txn 结构** —— `on_request` 按 `txn_id` **插入**在途集合,`on_completion` 按 `txn_id` **恒等擦除**。

**依据 = 平台自有相关性字段**:`dispatch_cohif` 把同一个 `b.transaction_id.read()` 传给两条流 ⇒ 按恒等匹配是**协议中立**的**记账决定**,**零 UALink 宣称**。
⚠️ **「零 TxnID 配对」的边界(消解 oracle 咨询的内部张力)**:「零 TxnID 配对」= **零 UPLI 协议层配对语义**(零 tag 配对规则 / 零 request↔response 配对协议 / 零超时 / 零重试 / 零排序 / 零资源门控);**平台自有 `transaction_id` 字段的恒等记账**属**平台层**,⛔ **不是**协议层 ⇒ **允许**,且是判据具备鉴别力的**必要条件**。

**鉴别力构造(两段 · 端态 + 中途)**:① **中途探针**:request `t1,t2` → 仅 completion `t1` ⇒ 断言在途集合 **== `{t2}`}**(⛔ 非「仅剩 1 个」—— 集合**逐格相等**)⇒ **插入序回退**类 mutant(pop 最旧且**不比对** `txn_id`)在此**必红**(其集合为空);② **端态**:request `t1,t2,t3` → completion **反序** `t3,t1,t2` ⇒ 断言在途集合**空** / `completion_unmatched_ == 0` / 计数与正序运行**逐字段相等** ⇒ **任何 order-coupling 类配对 mutant**(completion 处理**依赖请求到达序**)在反序下必红。

⚠️ **鉴别力边界(诚实登记)**:上述 fixture **抓不到**两类 **order-oblivious** 逃逸 mutant —— (a) **插入序回退且带 `txn_id` 恒等比对**(端态与中途均正确,但**匹配逻辑**非恒等);(b) **以地址而非 `txn_id` 作键**(若 fixture 中地址与 `txn_id` 同步变化,端态断言全通过)⇒ ⇒ **4 条不变量只 pin 端态与计数,不 pin 在途集合的键型** ⇒ 键型 SHALL 由**代码评审 + 头文件注释**(见 ⑤ 的键型约束)保证,⛔ **不得**声称本 fixture 已覆盖键型变异。
⚠️ **键型约束**:在途集合 SHALL 以 **`txn_id` 为唯一键**(⛔ 不得以地址 / 到达序 / 任何派生量作键)⇒ 「零 TxnID 配对」的边界见上条。

⛔ **文档禁用「TxnID 管理」** —— 那是 CHI 真插件的词汇(per `openspec/specs/cep-chi-rnf-subset` **CR-13**;⚠️ **CR-12 = Credit**,勿错引)⇒ 会构成越界宣称。

### ⑥ `snoop_resp` 的**不对称**(四句齐,防误读)

1. **为何是第三态**:声明给了它 `vc_binding`(VC3)却给 `flow_support = false`(per `protocol_instances.json`),而其余 7 条流**无任何正向陈述** ⇒ per 本 ADR **决策 6**,`mapping ≠ support` ⇒ 它处在「有邻近误导性陈述」的**不同认识论位置**;
2. **`pending` 是终态处置,不是暂态** —— 字面:「`pending`(语义待定,**恒不消费**);⛔ **不得**读作『已支持、待完成 / 待处理』」;
3. **机器绑定**:每次 `snoop_resp` 抵达 ⇒ **恰好一次**计数 + **零发射** + **零状态变更** + 对 `request`/`completion` 记账**零影响**;
4. **计数器名自带否定语义**:`snoop_resp_pending_not_consumed`。

⚠️ **collective 结果回传路径**:canonical 10 流里**没有** collective-result 流(`response` 是 `vc_binding` 的**消息类**,⛔ **非** canonical 流)⇒ 引擎对它**无事可做**,承载者仍是 `FabricCollectivePlane`(per `07 号` §11)⇒ 契约 SHALL 显式写「⛔ 引擎**不得**为 `response` 消息类 / collective 结果回传路径**新增任何 handler**(= 锚点摘要 **I-6** 路径)」以防后人填空(⚠️ 契约面**不**以研究文档的推断编号 **I-6** 为锚 ⇒ 该表述自解释)。

### ⑦ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR 正文 §1 ~ §13** | **零改动**;**新增 invariant 数 = 0** |
| **`protocol_instances.json`** | **零改动**(`ualink_4` 的 13 键取值**逐字不动**) |
| **计数** | **B3b-2 维持 0/4**;B3b-1 **维持 2/2** ⇒ **零变化** |
| **代价 ①** | 引擎**不提供**任何 UALink 协议语义 ⇒ 依赖 UALink **协议**语义的实验**仍不可做**;本条只把「承载者缺位」补到**记账级** |
| **代价 ②** | 观测面为**三分法** ⇒ 与既有 `MoesiGpuProtocolEngine`(M-6 一律记账)**形态不同** ⇒ 仓内出现**两种引擎观测约定** ⇒ 须靠本文档显式区分(⛔ 不得合并为「统一形态」) |
| **代价 ③** | 在途集合 = 引擎持有的**唯一**状态 ⇒ 引擎**不是**纯无状态载体 ⇒ 与 M-6「零状态机」的对照**仅在协议语义层**成立(平台层记账状态仍存在) |
| **代价 ④** | `snoop_resp` 的第三态使引擎观测面**比声明更细** ⇒ 未来若 `ualink_4` 的 `flow_support.snoop_resp` 转 `true`,该分支 SHALL 被**替换**而非**并存**(避免同一流两个状态) |

### ⑧ Verification(可勾选验收判据 · 前瞻)

- [ ] `ADR-CEP-12` 已追加本段 SU,且 §1 ~ §13 与**全部**既有 SU **逐字未变**
- [ ] `07 号` §5.1.1 UALink 行已**精确化**(实施**后**才改):措辞含「**记账级**」+ 「⛔ 不得读成『UALink 协议语义已实现』」+ 「**B3b-2 维持 0/4**」
- [ ] 凡本引擎出现的文档面**逐处**核过 ③ 的禁令清单(⛔ 无「弱档」单独出现)
- [ ] 命名核过:类名 / 注册短名 / 配置名**无** `Upli` / `Transaction` / `ReqRsp` 等语义载荷词;注册块注释**带**强度标签
- [ ] 引擎观测面实现 ④ 的**三分法** + **4 条机器可判不变量**
- [ ] ⑤ 的**反向 fixture 存在**(两段):① **中途探针** —— request `t1,t2` → 仅 completion `t1` ⇒ 在途集合 **== `{t2}`}**(逐格相等,⛔ 非「仅剩 1 个」);② **端态** —— 反序 `t3,t1,t2` ⇒ 在途集合空 / `completion_unmatched_ == 0` / 计数与正序逐字段相等;**且**该 fixture 对**order-coupling 类**配对 mutant **必红**(判据有牙,非恒真)
- [ ] ⑤ 的**鉴别力边界已诚实登记**:fixture **不覆盖** order-oblivious 逃逸 mutant(插入序回退**且带**恒等比对 / 以地址作键)⇒ 契约面 **⛔ 不得**声称覆盖键型变异;⇒ 在途集合**键型 = `txn_id`** 由**代码评审 + 头文件注释**保证
- [ ] **头文件注释写明「零 TxnID 配对」的边界**:「零 TxnID 配对 = 零 **UPLI 协议层**配对语义(零 tag 配对规则 / 零 request↔response 配对协议 / 零超时 / 零重试 / 零排序 / 零资源门控);平台自有 `transaction_id` 字段的**恒等记账**属**平台层** ⇒ **允许**」(per 台账 Entry #269 ③ 的**第一道文本防线**)
- [ ] `snoop_resp` 的 ⑥ 四句**齐备**且**机器绑定**断言存在;契约含「⛔ 不得为 `response` 消息类 / collective 结果回传路径新增任何 handler」
- [ ] 契约面(main spec)含头部 note:「依据档位 `public_nonnormative`;⛔ 不得作为 UALink 协议语义实现的证据;⛔ 不得计入 B3b」
- [ ] **B3b-2 仍 0/4** 的断言**显式存在**于判据中;`test/cep_b3b2_deferrals.json` **未被改动**
- [ ] 冻结面两步授权已执行:台账 **Entry #269** 授权 → **commit 本 SU 与 `07 号` 精确化** → `cep_freeze_check.sh --update --authorized-by "Entry #269"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #269**(授权 + oracle 咨询落点)· **#268**(立项 + 三选项)· **#265**(来源检索 + ⚠️ 合规登记 + 两条硬边界)· **#258**(`ADR-CEP-12` SU 追加先例 · 装配面)· **#261**(SU 追加先例 · 契约对齐)· `docs/research/2026-11-07-cep-ualink-flow-semantics-anchor-summary.md` · `include/tlm/cep/mesif_protocol_engine.hh`(`gate()` 范式先例)· `include/tlm/cep/moesi_gpu_protocol_engine.hh`(M-6 对照)· `openspec/changes/cpptlm-cep-ualink-coherence-engine/`

---

## Status Update(2026-11-08,per 台账 Entry #292)—— 「home 简化模型」的应答行为**不**作为能力映射 basis 分档的输入(边界句)

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文 §1 ~ §13 与**任何**既有 SU(尤其 **SU 2026-11-07**(Entry #269)的强度裁决 / 三分法 / 4 条不变量 / 鉴别力边界 —— **逐字保留不回改**);⛔ 不改任何家族实例的 13 键取值(⛔ `ualink_4` / `chi_rnf` 等**零改动**);⛔ **零**计数变化。
> **触发**:台账 **Entry #291**(用户裁决:完成者 = **真实 home 的简化模型**,承载**最小协议语义**)· `07 号` **SU · 十二** ② 的定位改写。
> **审查**:`oracle`(本边界句是否与本 ADR 的 basis 定义自洽)→ `critic`(本段 + `07 号` SU · 十二 + `ADR-CEP-19` SU · 二 的架构审稿),顺序不可颠倒。

### ① 边界句(逐字)

> `CohIfResponderTLM`(契约面独立类目 = **home 简化模型**,承载第一族 **CHI RN-F** 的**最小应答语义**)⇒ ⛔ **不**作为本 ADR 能力映射 / basis 分档的**输入**;⛔ **不**计入任何家族的 **T1–T4**;⛔ **不得**据其存在推出任何家族的协议语义达成 ⇒ ⇒ **`B3b-2` 维持 0/4**。

⇒ **落点性质**:本段是**审查包 U-5**(可后置项)的 **home 模型半边**的**首次落地**;同源纪律 = 本 ADR **决策 6**(`mapping ≠ support`)+ `07 号` **SU · 四 D-1**(计数依据 = 规范条款级)。
⚠️ **范围边界**:⛔ 本段**不**夹带 U-5 的 **wrapper 半边**(那超出 Entry #292 的授权面 ⇒ 须**另立 Entry**)。

### ② 理由(为什么它不能进 basis)

- 本 ADR 的 **basis = 公开来源档位 + `protocol_instances.json` 的 13 键声明**(id / family / adapter / status / `protocol_version` / `flow_support` / `perm` / `atomic` / `consistency_labels` / `ordering_assumption` / `vc_binding` / `sharer_tracker` / `combination_capabilities`)⇒ ⇒ **分档的输入是「声明」,不是「实现」**。
- home 简化模型 ⛔ **不在** `protocol_instances.json`(它**不是**协议实例)⇒ ⇒ **无 basis 声明** ⇒ ⇒ 若允许它进入分档,会破坏「**声明 ↔ 能力档位**」的双向对齐(实现侧元素倒灌进声明侧判据)⇒ ⇒ 与本 ADR **决策 6** 同源的纪律问题。
- ⇒ ⇒ 故:它**可以**在契约面承载**最小应答语义**,但**不得**出现在能力映射的输入侧 ⇒ 两个面**互不越界**。

### ③ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且本 ADR 正文与**全部**既有 SU **逐字未变**
- [ ] ① 的边界句已**逐字**登记;并已登记「落实 **U-5** 的 home 模型半边 + 与 **决策 6** / SU · 四 D-1 同源」的说明
- [ ] ① 的**范围边界**已登记(⛔ 未夹带 U-5 的 wrapper 半边)
- [ ] ② 的理由已登记(basis = 声明面 13 键,⛔ 非实现侧元素)
- [ ] ⛔ 契约面 delta 与台账中**均**含 ① 的**逐字**句(防误读)—— ⚠️ **本条随实施期 change 复验**(delta 是实施期产物 ⇒ 本次授权链复验时**不可**勾选)
- [ ] ⛔ `protocol_instances.json` **零 diff**;**`B3b-2` 维持 0/4**;`test/cep_b3b2_deferrals.json` **零 diff**
- [ ] 冻结面两步授权已执行:台账 **Entry #292** 授权 → **commit 本段 + `07 号` SU · 十二 + `ADR-CEP-19` SU · 二** → `cep_freeze_check.sh --update --authorized-by "Entry #292"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #292**(授权)· **#291**(两条用户裁决 + 落地义务清单)· **#269**(Entry #269 强度裁决)· **#285**(架构文档审查 · U-5 的来源)· `07 号` **SU · 十二** · `ADR-CEP-19` SU 2026-11-08 · 二 · `include/tlm/cep/protocol_instances.json` · `include/tlm/cep/protocol_instance.schema.json`

