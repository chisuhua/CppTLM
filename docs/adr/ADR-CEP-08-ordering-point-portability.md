# ADR-CEP-08: Ordering Point 可迁移性规则

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **Ordering Point 的定义 + 可迁移性判据 + 排序假设面边界 + 组合兼容性检查项**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §2.2(Directory & Ordering Plane 三子平面)+ §5.2(Sharer Tracker)
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
> - 真源 3:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流 + `barrier` 语义)
> - 真源 3:`ADR-CEP-04-sharer-tracker-abstraction-redesign.md`(Sharer Tracker caps)
> - 真源 3:`ADR-CEP-12-protocol-engine-capability-map.md`(能力映射表 schema,本批起草;提供 `ordering_assumption` 字段)
> - 真源 3:`ADR-CEP-13-coherence-domain-ownership.md`(一致性域归属,本批起草)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
> - 调研:`docs/research/cep-coherence-protocol-space-survey.md` v0.4 §4.2(HieraGen 深度)+ §5.3 F3/F4/F5/F17
>
> **一手锚点**:
> - HieraGen ISCA 2020 **p.2**:「any two racing coherence transactions will serialize at exactly one of the directories」(E1)
> - HieraGen §V-D:E 态静默升级破坏全局 SWMR(E1)
> - HieraGen §VII-D/E:非包容共享 cache / 跨层非树通信**不支持**(E1)
> - 博士论文 §9.3.4:「the bus in the snooping protocol acting as the point of serialization」(**E2 — Future Work,未实现**)

---

## 1. 背景

### 1.1 问题

`ADR-CEP-00` 把 CEP 定为框架级一致性探索平台,`07 号 §2.2` 的 **Directory & Ordering Plane** 声明含三个子平面:**Address Resolver + Sharer Tracker + Ordering Point**。但:

- **Ordering Point 从未被定义** —— 它是什么、在 CEP 的哪一层、承担什么语义
- **"可迁移性"从未被定义** —— 什么条件下 Ordering Point 可以从一种物理实现(目录节点)迁移到另一种(总线 / 树形 manager / in-network 排序点),而**不破坏全局 SWMR**
- **排序假设没有归属** —— 协议在"某条网络/VC 上消息是否保序"这个前提,既不是框架开关,也不是协议内部状态

### 1.2 为什么现在必须定义

调研 F4 / F5 给出了**两个方向相反**的一手事实:

| 事实 | 含义 |
|------|------|
| 目录是序列化点(ProtoGen 核心洞察,E1) | 目录型协议的 Ordering Point = 目录 |
| 总线亦可为序列化点(论文 §9.3.4,**E2 未实现**) | Ordering Point **不绑定物理实体** |
| 冲突事务恰在一个目录序列化(HieraGen,E1) | 唯一性是**可判定**的,不是修辞 |
| E 态静默升级破坏全局 SWMR(HieraGen §V-D,E1) | 存在**具名的**组合失败机制 |

→ CEP 要支持多个协议族(目录型 / 广播 snooping 型 / 树形),**必须**有一个跨协议通用的 Ordering Point 抽象,以及把"能否迁移"变成**可判定的规则**而非口号。

### 1.3 与 `ADR-CEP-04`(Sharer Tracker)的分工

| 子平面 | 回答的问题 | 本 ADR |
|--------|-----------|--------|
| **Address Resolver** | 这个地址归谁 | — |
| **Sharer Tracker** | 谁持有这个地址 | — |
| **Ordering Point** | **冲突事务在哪里被定序** | ✅ 本 ADR |

Sharer Tracker 决定**谁**要参与,Ordering Point 决定**序**从哪来。二者**正交但耦合**:Sharer Tracker 的 `completeness` 决定"漏掉一个 sharer"的后果(破坏 J2 全序可观测性)。

---

## 2. 决策

### 决策 1:Ordering Point 的定义

**定义**:Ordering Point 是**同一一致性域内,任意两条冲突事务必须经过、且在此获得全序**的**唯一逻辑点**。

| 属性 | 内容 |
|------|------|
| **逻辑性** | 它是**逻辑中心**,不绑定物理实体。目录节点 / 总线 / 树形 manager / in-network 排序点都可承载 |
| **唯一性** | 域内**恰有一个** Ordering Point 生效(否则冲突事务可能获得两个不一致的序) |
| **与流的关系** | Ordering Point **不新增 canonical 流** —— 它是 canonical 10 流的**定序语义**,发生在这些流之上(per `ADR-CEP-02` 零流数变更承诺) |
| **与 Sharer Tracker 的关系** | Sharer Tracker 提供"谁参与",Ordering Point 提供"序";两者组合才构成一次可判定的 snooping/失效 |

### 决策 2:唯一序列化点不变量

**不变量 `INV-CEP-08-IC1`**:在同一一致性域内,任意两条冲突事务**恰在一个** Ordering Point 序列化。

- **一手依据**:HieraGen ISCA 2020 p.2 —— 「in a hierarchical SSP that correctly enforces coherence globally, **any two racing coherence transactions will serialize at exactly one of the directories**」(E1)
- **树结构证明**(HieraGen §7.5):全局 SWMR ⇒ 每 block 恰有一个 owner;两条冲突事务的路径指向同一 owner;**树上两条路径恰有一个公共目录节点**;后到者据此推断自己输了,不再前进 → 该公共节点即唯一序列化点
- **可迁移性的守恒量**:迁移 Ordering Point **不得**改变本不变量。若迁移后同一冲突事务对可被两个点定出**冲突**的序,本不变量即被破坏

### 决策 3:可迁移性三判据

一个候选 Ordering Point `p` 合法,当且仅当同时满足:

| 判据 | 内容 | 检验方式 |
|------|------|---------|
| **J1 单点性** | 所有冲突事务**必须经过** `p` | 静态:消息路径覆盖检查 |
| **J2 全序可观测性** | `p` 给出的序对域内**所有 sharer 一致可见** | 依赖 `ordering_assumption` 声明 + Sharer Tracker `completeness` 非 `PARTIAL` |
| **J3 唯一性保持** | 迁移后,任一冲突事务对**不得**被两个点定出冲突的序 | 静态:与现存点的序一致性检查 |

**J2 与 Sharer Tracker 的耦合**:若 Sharer Tracker `completeness == PARTIAL` 且无 broadcast fallback,`J2` 不成立 —— 因为漏掉的 sharer 观察不到同一个序。→ **组合检查必须同时核对 caps**。

### 决策 4:E 态静默升级兼容性检查(组合必查项)

**不变量 `INV-CEP-08-IC2`**:任何**跨层 / 跨域组合**的可行性检查**必须包含 "E 态静默升级" 项**;未通过 ⇒ 该组合**非法**。

**机制**(一手,HieraGen §V-D,E1):
- E(Exclusive)态**可静默升级到 M**(read-write),且**无需通知目录**
- 若层 L 与层 H **都**允许静默升级,则:cache-L 在 E 态静默升级写入的同时,cache-H 可取到 S 权限 ⇒ **全局 SWMR 被破坏**
- HieraGen 的检测判据:**SSP 是否允许静默权限升级** —— 若允许,则推断"E 态是可写的"

**对 CEP 的具体影响**:`ADR-CEP-05` 的 Stub Cache **明确暴露 `E`**(`perm` 枚举 4 值)。因此 CEP 做组合时**该检查项在结构上必然存在**(不是"可能触发")。

**两条解法**(HieraGen 给出的):
1. 保守:高层改用**显式请求升级**的协议(如 MOESI 而非允许静默升级的组合)
2. 精确:在协议描述层**区分请求类型** —— HieraGen 的做法是把 `GetV` 重命名为 `GetV_R` / `GetV_A`(read vs acquire)使目录能推断访问类型

**与 `ADR-CEP-05 §7` α 失效判据 2 的关系**:`INV-CEP-05-IC1` 说"Stub 只暴露稳定状态";本决策指出 **Stub 可见的 4 值 `perm` 可能不足以支撑组合判断** ⇒ 若组合需要区分"静默 E→M"与"显式请求升级",需回到 `ADR-CEP-05 §7` 的 α 失效判据 2 重新评估(β) 独立接口方案。

### 决策 5:Ordering Point 不绑定物理实体

**不变量 `INV-CEP-08-IC3`**:Ordering Point **不绑定**任何特定物理实体;目录 / 总线 / 树形 manager / in-network 排序点**皆可**承载。

**一手依据与其证据等级(重要)**:
> 「One of key insights of ProtoGen is that the directory is the point of serialization for transactions. **This insight can be extended to the bus in the snooping protocol acting as the point of serialization** for all transactions. A cache can infer whether its own transaction has been serialized, by observing the request initiating this transaction on the bus.」
> —— 博士论文 §9.3.4(**E2 —— 属 Future Work,作者尚未实现**)

⚠️ **引用纪律**:本决策的**方向性**有 E2 支撑,**具体机制**尚未被实现验证。落盘时必须保留"**未实现**"标注,禁止写成"文献已证明 snooping 协议可用总线作序列化点"。

**对 gem5 Ruby 的意义**:gem5 的广播型协议(`MOESI_AMD_Base` / `MOESI_hammer` 的 probe filter + 广播)正是"总线即序列化点"的形态 —— 故本决策使 CEP 能以**同一抽象**覆盖目录型与广播型协议族(per 调研 F17)。

### 决策 6:排序假设面的边界

**排序假设**(`ordering_assumption`)是**协议侧的声明**,不是框架开关。

| 项 | 归属 |
|----|------|
| **声明什么** | 某个网络 / VC 上消息是否保序(`p2p_ordered` / `unordered`) |
| **谁声明** | ProtocolEngine 插件(per `ADR-CEP-07` 决策 9 + `ADR-CEP-12` schema) |
| **key 粒度** | **逐网络 / 逐 `vc_id`** —— 必须能解析到唯一 `vc_id`,否则配置加载 fail(per 调研 F8 / `NoCFlitBundle` 无 ordering 字段,排序是 VC 的**静态属性**) |
| **消费方** | 本 ADR 的 `J2` 判据 + `ADR-CEP-09` 的通道依赖图无环判据 |
| **不属谁** | **不**属 Fabric & Transport Plane 的可调参数;**不**属 Sharer Tracker |

**依据**:一手 —— ProtoGen 局限 L3「cannot automatically deduce the protocol for an unordered network given an SSP for an ordered network」(E1,网络序假设是**输入**);源码 `.pcc` 的 `Network { Ordered fwd; Unordered resp; Unordered req; }`(E1,排序假设是**协议规格的一部分**)。

### 决策 7:显式排除

| 排除项 | 理由 | 证据 |
|--------|------|------|
| **不承诺**自动判定组合可行性 | 该问题在学术侧亦未解决 | HeteroGen §VI-E:「We believe this is a challenging problem that is beyond the scope of this work」(E1) |
| **不支持**非包容共享 cache | HieraGen 不支持;要支持须放弃"不改 SSP"的锐利分界线 | HieraGen §VII-D(E1)+ 博士论文 §9.3.2(E2) |
| **不支持**跨层非树通信 | HieraGen 不支持,原文称该假设"critical to the current design" | HieraGen §VII-E(E1) |
| **不新增** canonical 流 | Ordering Point 是既有流的**定序语义** | `ADR-CEP-02` 零流数变更承诺 |
| **不承诺**snooping 协议的序列化点机制**已验证** | §9.3.4 属 Future Work | E2 标注 |

---

## 3. 不变量

- **INV-CEP-08-IC1**:同一一致性域内,任意两条冲突事务**恰在一个** Ordering Point 序列化
- **INV-CEP-08-IC2**:任何跨层 / 跨域组合的可行性检查**必须包含 "E 态静默升级" 项**;未通过 ⇒ 该组合非法
- **INV-CEP-08-IC3**:Ordering Point **不绑定**物理实体(目录 / 总线 / 树形 manager / in-network 皆可)

> **编号位说明**:`INV-CEP-08-IC1` ~ `IC3` 共 **3** 条,`IC4` **跳空**。

---

## 4. 抓取点

1. **唯一性检查**:对每个协议实例组合,验证任一冲突事务对的可序列化点数 == 1
2. **E 态升级检查**:组合检查器**必须**执行 E 态静默升级项(缺项 = fail,不得静默跳过)
3. **J2 依赖 caps**:Sharer Tracker `completeness == PARTIAL` 且无 broadcast fallback ⇒ `J2` 不成立 ⇒ 组合非法
4. **`ordering_assumption` key 解析**:必须解析到唯一 `vc_id`,否则配置加载 fail
5. **§9.3.4 引用纪律**:任何引用"总线即序列化点"的文档**必须**标注"Future Work,未实现"

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-04`(Sharer Tracker)| ✅ v0.6 | 提供 caps(`J2` 依赖 `completeness`) |
| `ADR-CEP-12`(能力映射表 schema)| 🟡 **本批起草** | 提供 `ordering_assumption` 字段(决策 6 的载体) |
| `ADR-CEP-09`(多 VC 仲裁 + 死锁自由性)| 🟡 **本批起草** | 消费 `ordering_assumption` + `vc_binding` → 通道依赖图 |
| `ADR-CEP-13`(一致性域归属)| 🟡 **本批起草** | 跨域组合的域边界由本 ADR 的 `J1`/`J3` 判据支撑 |
| `ADR-CEP-05`(Stub Cache)| ✅ v0.7 | `INV-CEP-05-IC1` + §7 α 失效判据 2(与决策 4 联动) |
| `ADR-CEP-07`(仿真语义 + seam)| ✅ v0.7 | 决策 9:插件声明排序假设 + VC 绑定 |
| `ADR-CEP-11`(协议版本锁定)| 🟡 待起草 | 版本锁定与排序假设的版本兼容性 |
| `ADR-CEP-14`(Stub 合同测试)| 🟡 待起草 | `INV-CEP-08-IC2` 的检查项落地位置 |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| Ordering Point 定义 | 域内冲突事务必经且在此定序的**唯一逻辑点** | ✅ 已拍板 |
| 唯一序列化点 | `INV-CEP-08-IC1` | ✅ 已拍板 |
| 可迁移性三判据 | J1 单点性 / J2 全序可观测性 / J3 唯一性保持 | ✅ 已拍板 |
| E 态静默升级检查 | `INV-CEP-08-IC2`,组合必查项 | ✅ 已拍板 |
| 不绑定物理实体 | `INV-CEP-08-IC3`;⚠️ 依据为 E2(Future Work) | ✅ 已拍板(附证据等级标注) |
| 排序假设面边界 | 协议侧声明、逐 `vc_id`、消费方 = J2 + CEP-09 | ✅ 已拍板 |
| 显式排除 | 自动组合判定 / 非包容共享 cache / 跨层非树通信 / 新增流 | ✅ 已拍板 |

---

## 7. 修订记录

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | 调研 v0.4 §4.2 + §5.3 F3/F4/F5/F17 + Oracle 独立复审 | Ordering Point 定义 + 可迁移性三判据 + E 态检查 + 排序假设面 |
| 决策 2 | HieraGen p.2(E1) | 唯一序列化点不变量 + 树结构证明 |
| 决策 4 | HieraGen §V-D(E1) | E 态静默升级检查项 + 两条解法 |
| 决策 5 | 博士论文 §9.3.4(**E2**) | 不绑定物理实体;**明确标注 Future Work** |
| 决策 6 | ProtoGen L3(E1)+ `.pcc`(E1) | 排序假设面的归属与粒度 |

---

**版本**: v0.1 草案
**最后更新**: 2026-10-21
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
