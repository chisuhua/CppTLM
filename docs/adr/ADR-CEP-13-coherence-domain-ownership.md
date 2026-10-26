# ADR-CEP-13: 一致性域归属与跨域组合判据

> **状态**: 🟡 起草中(v0.1)
> **日期**: 2026-10-21
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **一致性域的划界 + 跨域组合的可判定判据 + 组合承诺边界**
> **关联真源**:
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §2.2(Directory & Ordering Plane)+ §5.1.4(协议语义索引)+ §8.1(Stage 划分)
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
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

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 8. Status Update(2026-10-22):`INV-CEP-13-IC2` 的**应用口径**注记

> **授权**:台账 `objective-cep-platform §11` **Entry #13** [adr-amendment]。
> **触发**:ArchForge 答复回执(2026-10-22)**§1.1 Q-3a** —— 判据 (b) **首次被评估**且**成立**。
> **本节不改判据定义** —— §2 决策 3 与 §3 `INV-CEP-13-IC2` 的**正文零改动**;
> 本节仅记「**怎么用**这条判据」的应用口径(判据本身自 v0.1 起未变)。

### 8.1 为什么需要口径注记

判据 (b) 首次被实际评估(2026-10-22),评估中暴露**三处易被误用的读法**。
ArchForge 的评估过程(答复回执 `cep-interface-alignment-response-2026-10-22.md` §1.1)对这三点均有明确处理,本节据以固化,避免后续重复踩坑。

### 8.2 三条应用口径(判据**定义不变**)

| # | 口径 | 内容 | 依据 |
|---|------|------|------|
| **①** | **(b) 是析取,不是合取** | 判据 (b) 的原文是「`s1` **或** `s2` 接受『导致数据值响应的转发请求』」→ **任一**成立即 (b) 成立。<br>⚠️ 常见误读:把它当成「`s1` 与 `s2` 都须成立」。`moesi_gpu` 的 `E→M` 即为**反例**:`E` 不成立(干净独占,snoop 响应 `ACK` 不带数据),但 `s2 = M` 成立 → (b) 仍成立 | ArchForge 答复回执 `cep-interface-alignment-response-2026-10-22.md` §1.1(3) |
| **②** | **(b) 落在协议状态机的响应语义上,不在状态名的直觉上** | 判据问的是「该状态**是否以数据响应转发请求**」,**不是**「该状态名听起来是否像可写」。<br>例:`M` 成立是因为它的 snoop 响应是 `DATA_AND_ACK`(**携带数据值**),不因为「M = Modified」这个名字 | ArchForge 答复回执 `cep-interface-alignment-response-2026-10-22.md` §1.1(6) |
| **③** | **(b) 必须逐协议实例评估,不可跨实例外推** | 换一个协议的 `E` 语义(若其 `E` 也带数据响应),(b) 的结论会**不同**。<br>⚠️ 本次结论**仅对 `moesi_gpu` 成立** | ArchForge 答复回执 `cep-interface-alignment-response-2026-10-22.md` §1.1(6) |

### 8.3 本次评估的落点与层级(如实声明)

- **评估结果**:`moesi_gpu` 的 `E→M` → 判据 (b) **成立** → (a) ∧ (b) 成立 → **是域可见写**。
- **论证来源**:ArchForge 答复回执(2026-10-22)**§1.1 Q-3a** + 其仓 `32 号 §5.2.1`。
- **层级**:**声明层** —— `moesi_gpu` 插件**尚无实现**(本仓 `07 号 §5.1.1` 状态 = 未排期;
  ArchForge `09 号 §5.3` 亦为规划态);(b) 的评估依据是对方仓的**文档级** snoop 响应语义。
  **实测确认**依赖 PoC **S2**(per `ADR-CEP-05 §13.1` 同表记录)。
- **判据 2 的状态**(宿主 ADR):**已触发**(实质层)/ 组合检查落地后方可执行(程序层)——
  详见 `ADR-CEP-05 §13.1`。**标注侧**由 ArchForge Q-3c 裁定 = **CEP 侧**
  (本 ADR **不**复制该状态值,防双真源)。

### 8.4 未变更项(明示)

> §2 决策 3 的判据原文 · §3 `INV-CEP-13-IC2` 的 (a)(b) 定义 · §4 抓取点第 2 项(本轮是**按它执行**,不是改它)·
> §6 决策汇总 · 本 ADR 其余全部正文 —— **全部零改动**。

## 9. Status Update(2026-10-26,per 台账 Entry #71)—— (b) 成立后的**首个可执行载体**

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §8 正文一律不改,仅登记状态变化。
> **触发**: 2026-10-26 队列 #6 `cpptlm-cep-moesi-gpu-adapter` 建 change 目录(Stage 1 / 2028-Q1)⇒ 该 change 是 `moesi_gpu` 的**首个 `implemented` adapter**,也是 `INV-CEP-13-IC2` 判据 (a)(b) 的**首个可执行载体**。
> **载体**:本 ADR 决策 3 的 (a)(b) 判据,在 `moesi_gpu` 上得到**声明侧**的首次落地 —— (a) = `E` 态 store **命中且无需外部通信**(静默升级);(b) = `M` 态的 snoop 响应**携带数据值**(`DATA_AND_ACK`)。该载体以 `perm_upgrade.silent_upgrades[]`(声明 `UNIQUE → WRITEBACK`)+ adapter 状态机表(`E+Write→M`)双向核对的形式落地(per 队列 #6 的契约判据 **AC-02** / **AC-05**)。
> **边界**:本 change 只落地**声明侧**投影与**语义关键点**断言;**跨域写传播机制**(决策 3 的两条:一致性模型翻译 / proxy-cache)仍**未实施** ⇒ 判据 (a)(b) 的**运行期**验证属后续 change,不因本载体而声称已闭合。
> **未变更项(明示)**:§2 决策 1 ~ 5 的正文 · §3 `INV-CEP-13-IC1` ~ `IC3` 的**编号与定义** · §8.4 的既有所列 —— **全部零改动**。
> **台账**:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #71**
> **冻结面**:本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #71)。
