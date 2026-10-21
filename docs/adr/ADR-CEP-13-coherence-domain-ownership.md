# ADR-CEP-13: 一致性域归属与跨域组合判据

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **一致性域的划界 + 跨域组合的可判定判据 + 组合承诺边界**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §2.2(Directory & Ordering Plane)+ §5.1.4(协议语义索引)+ §8.1(Stage 划分)
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4
> - 真源 3:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
> - 真源 3:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流 + `barrier` 语义)
> - 真源 3:`ADR-CEP-04-sharer-tracker-abstraction-redesign.md`(Sharer Tracker caps + `INV-CEP-04-IC7` token 不跟踪 sharer set)
> - 真源 3:`ADR-CEP-06-experiment-protocol.md`(`verification_mode` 契约 —— 实验**证伪**而非保证)
> - 真源 3:`ADR-CEP-07-simulation-and-integration-semantics.md`(决策 9:插件接口可组合性)
> - 真源 3:`ADR-CEP-08-ordering-point-portability.md`(Ordering Point + J1/J2/J3 判据,本批起草)
> - 真源 3:`ADR-CEP-09-multi-vc-arbitration-and-deadlock-freedom.md`(多 VC 仲裁 + 死锁自由性,本批起草)
> - 真源 3:`ADR-CEP-12-protocol-engine-capability-map.md`(`consistency_labels` + `combination_capabilities`,本批起草)
> - 真源 3:`ADR-CEP-17-ruby-slicc-protocol-family.md`(决策 7 / 决策 9 —— 组合承诺边界)
> - 真源 3:`ADR-CEP-NUMBER-MAP.md`(编号预分配)
> - 调研:`docs/research/cep-coherence-protocol-space-survey.md` v0.4 §4.3.3 / §4.3.4 / §4.3.5 / §4.3.6 / §4.3.7 + §5.3 F8/F9/F10/F13
>
> **一手锚点**:
> - HeteroGen HPCA 2022 §V-B:复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)`,`ppo_com(t) ≡ ppo_i`(E1)
> - HeteroGen §VI-D-1:"全局可见写"静态判据 (a)+(b)(E1)
> - HeteroGen §VI-C:proxy-cache + 一致性模型翻译(ArMOR)(E1)
> - HeteroGen §VI-D-2:保守 processor-centric vs 激进 memory-centric 两种桥接并发设计(E1)
> - HeteroGen §VI-E:兼容性自动判定「beyond the scope of this work」(E1)
> - 博士论文 §7.6.6:目录无法区分 read vs acquire → 请求改名(E1)

---

## 1. 背景

### 1.1 问题

`07 号 §2.2` 的 Directory & Ordering Plane 含 `barrier_domain` / 一致性域概念,但 CEP 从未定义:

- **一致性域的边界**怎么划 —— 一个域包含哪些节点、由什么界定
- **跨域边**的可见性规则是什么 —— 域 A 的写何时、如何对域 B 可见
- **跨域组合**的正确性由什么保证 —— 判定条件是什么
- **CEP 承诺到什么程度** —— 是"自动组合"还是"人工声明 + 实验证伪"

### 1.2 学术侧已给出的判据(可直接转写)

HeteroGen(HPCA 2022)把"异构组合"形式化为**复合一致性模型**,给出三条可转写的判据:

| 判据 | 内容 | 证据 |
|------|------|------|
| **复合一致性公理** | 复合一致性 ⟺ `acyclic( ppo_com ∪ rfe ∪ fr ∪ ws )`,其中 `ppo_com(t) ≡ ppo_i`(t 属于域 i) | §V-B(E1) |
| **全局可见写判据** | 请求 `r` 是域可见写 ⟺ (a) 终态 `s1` 允许本地写命中(可能转 `s2`)且 (b) `s1`/`s2` 接受导致**数据值响应**的转发 | §VI-D-1(E1) |
| **桥接并发粒度** | 保守(全串行)/ 激进(同地址串行)由"对端协议是否允许提前 ack"决定 | §VI-D-2(E1) |

### 1.3 范围事实(必须诚实面对)

HeteroGen 论文 §9.2 自陈:

> 「The HieraGen and HeteroGen tool are publicly available as two stand alone tools with **limited compatibility**. Automatically generating a heterogeneous and hierarchical cache coherence protocol from given input SSPs is **not possible with these tools without manual intervention**.」(E1)

但同文 §1.3 明示**算法层可串联**:"the lower-level directory controller SSP input of HieraGen **can be** the atomic heterogeneous directory-controller output of the HeteroGen stage"(E1)。

→ **两层必须分开**(per `ADR-CEP-17` 决策 9 的精化):
- **工具集成负担** —— 两个独立工具的 IR 不统一。**CEP 不继承**(CEP 不做自动合成)
- **语义组合负担** —— 组合判据须人工套用,且判据本身**尚未被自动化验证过**。**CEP 完全继承**

---

## 2. 决策

### 决策 1:一致性域的定义

**一致性域** = **一个 Ordering Point 生效 + 一套自己的 preserved program order(`ppo`)生效**的范围。

| 属性 | 内容 |
|------|------|
| **边界** | 由 `Ordering Point` 的作用域界定(per `ADR-CEP-08` 决策 1)—— 域内所有冲突事务必经该点 |
| **内部序** | 域内用自己的 `ppo_i`(per `ADR-CEP-02` 的请求序) |
| **跨域** | 域间**不共享** `ppo`;跨域关系进全局内存序 |
| **与 `barrier_domain` 的关系** | `barrier_domain` 是**作用域标签**(用于流 9 `barrier` 的 requester-scoped 语义),**不等于**一致性域边界 —— 一个 `barrier_domain` 可跨越多个一致性域 |

### 决策 2:复合一致性判据

**不变量 `INV-CEP-13-IC1`**:跨域组合的正确性判据采用**复合一致性模型**:

```
acyclic( ppo_com ∪ rfe ∪ fr ∪ ws )
其中  ppo_com(t) ≡ ppo_i   (线程 t 属于域 i)
```

**语义**:复合一致性保证 **每个域的操作仍遵守该域自己的模型**;域间关系在**全局内存序**中统一处理(`fr` 边跨域也算全局序的一部分)。

**为什么采用它而非"所有域模型相同"**:后者过严 —— 异构系统的现实就是各域模型不同(如 CPU 簇 SC + GPU 簇 RC)。`ppo_com` 形式既准确又可实现。

**编程侧推论(记录,不展开)**:复合一致性使**编译器映射可复用** —— 线程映射到哪个域,就用该域已知的编译器映射。

**依据**:HeteroGen §V-B(E1)。

### 决策 3:跨域写传播的触发条件

**不变量 `INV-CEP-13-IC2`**:一次跨域写传播**必须**由"域可见写"触发,判据为:

> 请求 `r` 是**域可见写** ⟺
> **(a)** 终态 `s1` 允许 store **命中且无需外部通信**(可能转到 `s2`)
> **且 (b)** `s1` 或 `s2` 接受"导致**数据值响应**的转发请求"

**为什么需要这条**:不是所有写都需要跨域传播。RC 协议中,store 命中 invalid line 时其实发的是**读请求**(取数据),但终态 valid **允许后续写**(写先本地缓冲)→ (a) 成立而 (b) 不成立(valid 态不接受转发请求)⇒ **不得**误判为域可见写。

**传播机制**(HeteroGen §VI-C 的两条,可转写):
1. **一致性模型翻译** —— 在目标域找出"SC 等价的访问序列"(SC 等价的写**保证**触发一次全局传播的写)
2. **proxy-cache** —— 在桥接点内嵌一个目标域协议状态机实例,由它发起目标域的请求

**依据**:HeteroGen §VI-D-1 / §VI-C(E1)。

### 决策 4:跨域桥接的并发粒度

**不变量 `INV-CEP-13-IC3`**:跨域桥接的并发粒度**由对端协议是否允许提前 ack 决定**,不由框架固定:

| 对端协议 | 粒度 | 说明 |
|---------|------|------|
| 允许提前写 ack | **保守**:全部串行 | 保守解的代价可控 |
| 不允许提前完成 | **激进**:同地址串行,跨地址可并发 | 常规一致性控制器行为 |

**与 Sharer Tracker 的同构性**(调研 F13,**E3 设计类比**):保守 ↔ `CONSERVATIVE`;激进 ↔ 精确。二者是**同构选择**,但**不是同一物** —— 引用时须标注为类比。

**依据**:HeteroGen §VI-D-2(E1)。

### 决策 5:组合承诺边界(与 `ADR-CEP-17` 决策 9 对齐)

**CEP 承诺三件更窄、更可交付的事**:

| # | 承诺 | 落点 |
|---|------|------|
| **①** | **判据可复用** —— 把 5 条组合判据(唯一序列化点 / E 态静默升级 / 复合一致性公理 / 全局可见写判据 / proxy-cache 模板)转写成 CEP 的**结构化声明项**,使"人工声明"从口头承诺变成**有固定字段、可机器检查**的声明 | `ADR-CEP-12` schema + 本 ADR 的判据条目 |
| **②** | **逐组合实验证伪** —— 正确性由**随机 / litmus 实验在声明的 `verification_scope` 内尝试证伪**;**平台不提供无违例保证** | `ADR-CEP-06 §3` 的 `verification_mode` 契约 |
| **③** | **接口预留组合接缝** —— 插件接口从第一版为组合维度留扩展点 | `ADR-CEP-07` 决策 9 |

**明确不承诺**:

| 不承诺 | 理由 | 证据 |
|--------|------|------|
| **自动化组合生成** | CEP 不做协议生成器 | `ADR-CEP-17` 决策 10 |
| **自动兼容性判定器** | 学术侧亦未解决 | HeteroGen §VI-E(E1) |
| **无违例保证** | 实验只能证伪 | `ADR-CEP-06 §3` |
| **任意配置穷尽验证** | 规模事实(极端配置需 1TB RAM + 数小时) | 调研 F27(E1) |

### 决策 6:显式排除

| 排除项 | 理由 |
|--------|------|
| **不提供**自动协议兼容性判定器 | 见决策 5 |
| **不承诺**自动化组合 | 见决策 5 |
| **不承诺**无违例保证 | 见决策 5 |
| **不做**"分层 + 异构"的自动组合 | 工具集成负担不继承,语义组合负担完全继承(见 §1.3) |
| **不引入**新的 canonical 流 | 跨域可见性由既有 10 流 + 插件语义表达 |

---

## 3. 不变量

- **INV-CEP-13-IC1**:跨域组合的判据采用**复合一致性模型** `acyclic( ppo_com ∪ rfe ∪ fr ∪ ws )`,`ppo_com(t) ≡ ppo_i`(t 属域 i)
- **INV-CEP-13-IC2**:跨域写传播**必须**由"域可见写"判据 (a)+(b) 触发;不得以"所有写都传播"或"任何写都不传播"简化
- **INV-CEP-13-IC3**:跨域桥接的并发粒度由**对端协议是否允许提前 ack** 决定(保守全串行 / 激进同地址串行)

> **编号位说明**:`INV-CEP-13-IC1` ~ `IC3` 共 **3** 条。

---

## 4. 抓取点

1. **复合公理可判定**:跨域组合的声明项须能映射到 `ppo_com` / `rfe` / `fr` / `ws` 四个关系(per `INV-CEP-13-IC1`)
2. **域可见写判据强制**:`INV-CEP-13-IC2` 的 (a)(b) 两条件**必须**同时评估,缺 (b) 的实现会把 RC 的读请求误判为写
3. **桥接粒度选择有据**:并发粒度的选择**必须**记录所依据的对端协议"是否允许提前 ack"判定(per `INV-CEP-13-IC3`)
4. **E 态检查并行**:`ADR-CEP-08` 的 `INV-CEP-08-IC2`(E 态静默升级)是**跨域组合必查项**,与本 ADR 的判据并列
5. **不得承诺无违例**:任何文档/输出**不得**把 `deadlock_violations == 0` / `memory_violations == 0` 表述为"已证明"(per `ADR-CEP-06 §3`)

---

## 5. 关联 ADR / 后续

| 项 | 状态 | 关系 |
|----|------|------|
| `ADR-CEP-02`(canonical 10 流)| ✅ v0.3 | 域内 `ppo` 与流语义 |
| `ADR-CEP-04`(Sharer Tracker)| ✅ v0.6 | `INV-CEP-04-IC7`:token 类协议 tracker 不跟踪 sharer set —— 与本 ADR 的"协议自带一致性语义时中心目录可退化"同源 |
| `ADR-CEP-06`(实验输出)| ✅ v0.3 | `verification_mode` 契约 = 决策 5 ② 的载体 |
| `ADR-CEP-07`(仿真语义 + seam)| ✅ v0.7 | 决策 9:接口预留组合接缝 = 决策 5 ③ 的载体 |
| `ADR-CEP-08`(Ordering Point)| 🟡 **本批起草** | 域边界由 Ordering Point 界定;`INV-CEP-08-IC2` E 态检查并行 |
| `ADR-CEP-09`(多 VC + 死锁自由)| 🟡 **本批起草** | 跨域桥接引入的新 VC 须纳入通道依赖图 |
| `ADR-CEP-12`(能力映射表)| 🟡 **本批起草** | `consistency_labels` + `combination_capabilities` = 决策 5 ① 的载体 |
| `ADR-CEP-17`(协议族接入)| ✅ v0.3 | 决策 7 / 决策 9:承诺边界的上游 |
| `ADR-CEP-14`(Stub 合同测试)| 🟡 待起草 | 判据 (a)(b) 的机器检查落地位置 |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 一致性域定义 | 一个 Ordering Point + 一套 `ppo` 的范围;`barrier_domain` ≠ 域边界 | ✅ 已拍板 |
| 复合一致性判据 | `INV-CEP-13-IC1` | ✅ 已拍板 |
| 域可见写触发 | `INV-CEP-13-IC2` (a)+(b) | ✅ 已拍板 |
| 桥接并发粒度 | `INV-CEP-13-IC3` | ✅ 已拍板 |
| 组合承诺边界 | 承诺 3 件(判据可复用 / 实验证伪 / 接口预留);不承诺 4 件 | ✅ 已拍板 |
| 显式排除 | 自动兼容性判定 / 自动组合 / 无违例保证 / 穷尽验证 / 新增流 | ✅ 已拍板 |

---

## 7. 修订记录

| 修订 | 来源 | 内容 |
|------|------|------|
| 全文起草 | 调研 v0.4 §4.3.3~§4.3.7 + §5.3 F8/F9/F10/F13 + Oracle 独立复审 Q7 | 域定义 + 3 条判据 invariant + 承诺边界 |
| 决策 2 | HeteroGen §V-B(E1) | 复合一致性公理;拒绝"所有域模型相同"的过严形式 |
| 决策 3 | HeteroGen §VI-D-1(E1) | 域可见写 (a)(b) 双条件;记录 RC 反例 |
| 决策 5 | `ADR-CEP-17` 决策 9 精化 + HeteroGen §VI-E(E1) | 承诺 3 件 / 不承诺 4 件;"实验证伪"而非"实验保证" |
| §1.3 | 调研 F19 修正(per oracle 复审 R5) | **工具集成负担不继承;语义组合负担完全继承** —— 两层分开 |

---

**版本**: v0.1 草案
**最后更新**: 2026-10-21
**批准状态**: 🟡 待 Oracle 复审
**维护者**: CppTLM build agent(architect mode)
