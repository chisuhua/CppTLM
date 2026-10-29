# D-α —— MESIF 规约的**推导口径**(本 change 的核心决策)

## 决策

**MESIF 的 5 态 `M/E/S/I/F` 规约由本仓「本地推导」,每格标注依据 + 待追认状态;不等待 ArchForge 闭合 `25-nsa §4.4`。**

## 为什么是「本地推导 + 标注待追认」

| 方案 | 结论 | 理由 |
|---|---|---|
| **A′** 本地推导 + **逐格标注依据** + 逐格标注**待追认** | ✅ **采纳** | ① 队列 #9 由此**可解冻**(否则永远等一个开放问题);② **诚实**:每格都写明依据来源(ArchForge 原文 / 行业惯例 / 本仓推导)⇒ 读者能区分「有据」与「推导」;③ **可对齐**:ArchForge 闭合后逐格比对即可,差异即待追认项;④ 跨仓债**可见**(不是「假装已有规约」) |
| **B** 等 ArchForge 闭合 `25-nsa §4.4` 再做 | ❌ 否决 | `25-nsa §4.4` 是**选型开放问题**(`25-nsa:592` / `:611`),`ADR-SOC-25:283` 的返工清单尚未落地 ⇒ **无界阻塞**(per 队列 #8 同款机理) |
| **C** 直接照搬 MOESI 的 6×6 转换表,按同名字态对齐 | ❌ 否决 | 状态词表**不同**(6 态 `I/S/E/M/O/T` vs 5 态 `M/E/S/I/F`,per `32 号:213`)⇒ 同名态语义**未必同构**;MESIF 的 `F`(Forward = 单 Owner + 多 Sharer,per `25-nsa:598-600`)在 MOESI 词表里**无对应** ⇒ 照搬产出**杜撰**规约 |
| **D** 只写 5 态词表 + 一句「投影待定」 | ❌ 否决 | 词表**已在** ArchForge(`25-nsa:598`)⇒ 本仓重复它**零增量**;且队列 #9 需要的是**转换表**才有判据可写 |

## 推导口径(逐条依据)

| 规约元素 | 依据 | 依据强度 |
|---|---|---|
| 5 态 `M/E/S/I/F` 的**存在** | ArchForge `25-nsa:598` / `32 号:213` / `32 号:725` | **一手原文** |
| `F`(Forward)= 单 Owner + 多 Sharer | ArchForge `25-nsa:599-600` | **一手原文** |
| `F` → `WRITEBACK` 投影 | ArchForge `32 号:529` | **一手原文** |
| `F` 在 Owner 转移时**清空** | ArchForge `25-nsa:553` / `32 号:529` | **一手原文** |
| 目录条目含 `State(12 bits, MESIF 编码)` | ArchForge `25-nsa:539` | **一手原文** |
| `M/E/S/I` 的 4 值投影 | **本仓推导** —— 同名态与 MOESI 同义(`M`=Modified 脏独占 / `E`=Exclusive 干净独占 / `S`=Shared 多副本 / `I`=Invalid)⇒ 投影 `M→WRITEBACK` / `E→UNIQUE` / `S→READ` / `I→NONE` | **推导**(待追认) |
| 状态转换表 | **本仓推导** —— 以 MOESI 6×6(`09 号 §3.3`)为**结构模板**,按 MESIF 的 5 态与「Forward 语义」裁剪 | **推导**(待追认) |
| 写只在独占态合法 | ArchForge `25-nsa:552-554`(Remote Atomic 必须先查 Directory 确认 Owner 唯一)+ 同型先例 IHI0050 §B4.1(CHI) | **类比推导**(待追认) |

> ⚠️ **推导的边界**:上表标「推导」的每一格,在实现载体里都**必须**带 `basis` 字段(非空)+ `pending_uptick` 标记 ⇒ 判据 AC-6 逐格检查。

---

# D-β —— 可执行载体形态(与 `chi_rnf_adapter.hh` 同款)

`include/tlm/cep/mesif_state_spec.hh` 的形态:

- `enum class State { I = 0, S = 1, E = 2, M = 3, F = 4 }` —— **值序刻意**让 `E = 2` / `M = 3`(与 CHI 的 `UC=2` / `UD=3` 同款)⇒ `E + Wr → M` 的目标态恰为 **3** ⇒ 可**复用**队列 #6 的 AC-05 判据形态(输入是 POD,判据本体零改动)。
- `enum class Event { Rd, RdUnique, Wr, Upgrade, SnpShared, SnpUnique }` —— 与 CHI 的 6 事件**同构**(MESIF 是 snoop 型 + 目录型;目录查询是**另**一条路径,不属状态机转换)。
- `kStateTransition[5][6]` —— **表驱动**,每格附 `basis`。
- `perm_of(State, Perm&)` —— 5 态**全部**有投影;**瞬态无**(本规约**不声明瞬态** ⇒ AC-2 平凡成立,但载体**保留**瞬态列表以支持未来追认后扩展)。
- 三条编译期锁:① 转换表**完备**(每格非「未定义」)② 5 态投影齐备 ③ `E + Wr → M` 存在且秩升高。

---

# D-γ —— 判据实现位置(单一真源纪律)

**判据本体**放 `test/test_cep_mesif_state_spec.cc`(与 `test_cep_chi_rnf_subset.cc` 同款),**不**改 `adapter_schema_contracts.hh`(那是**实例面**的契约,不是**规约面**的)。⇒ 与队列 #8 的「判据本体零改动」纪律同构。

---

# D-δ —— 跨仓追认登记

规约载体里每格带 `basis` + `pending` 标记;另出一份**追认清单**(本 change 的交付物之一),逐格列出「ArchForge 侧需要确认/补写什么」⇒ 由 ArchForge 侧的**债务清理与规约追认**改进提案承接(`ArchForge:.rddf/improvements/cep-alignment-debt-cleanup.md`(跨仓))。

> ⚠️ **载体迁移登记**(2026-10-29,ArchForge 侧 commit `4f0f330`):原架构文档 `41 号`
> (`docs/architecture/41-cep-alignment-debt-cleanup-proposal.md`)**已被移除** —— 理由:「提案文档」不是
> 可跟踪工作项(无 owner / 无 tasks / 无 AC / 无归档记录)⇒ 跨仓债不可见。债务台账迁入
> **rdd-workflow 改进提案** `.rddf/improvements/cep-alignment-debt-cleanup.md`(已 git 跟踪)。
> ⇒ 本仓的跨仓指向**已同步改指**新载体(per Entry #98)。

> ✅ **权威归属已裁决**(2026-10-29,ArchForge 侧改进提案「权威归属裁决」段):ArchForge 采纳
> **方案 A(追认)** —— ArchForge 侧保留 `mesif` 声明作为**镜像**,口径对齐本仓权威。
> ⇒ 与本仓 `ADR-CEP-12 §13` 的「权威迁移」机制**同向**,无冲突。
> ⇒ **跨仓待办(本仓侧,登记于 tasks 4.4)**:`authority_migrations[mesif]` 的**降级 / 删除**
> 应在追认完成后由本仓人工裁决 + 台账 entry(per ArchForge AC-11 —— 本仓无权单方闭环)。

---

# D-ζ —— 判据的**口径边界**(判据锁什么 / 不锁什么)

| 判据**锁** | 判据**不锁** |
|------------|--------------|
| 转换表**完备**(30 格无空洞、无越界) | 各格**目标态**的**语义正确性** |
| 投影表**完备**(5 态各有 4 值投影) | 投影**取值**的**语义正确性** |
| 每格 `basis` **非空** + `kind`/`pending` **双向一致** | 依据**强度**判断本身是否正确 |
| 写规则 / 秩口径的**内部自洽** | — |
| 2 项语义损失**已显式登记** | — |

> ⚠️ **依据来源**:ArchForge 侧改进提案的**风险 ②** 明确 —— 「转换表 30 格中第 ②/③ 档依据是
> **推导 / 类比** ⇒ 追认可纠正 ⇒ **投影表回填前不得据此写实现判据**」。
> ⇒ 本仓的判据**只锁完备性 / 可审计性 / 待追认标记**,**不**把推导值固化为「已确认的语义」——
> 这与 D-ε.2 的 `pending` 机制是同一件事的两面:**未追认的值必须带着 `pending` 标记留在表里**,
> 而**不是**被悄悄写成「既定事实」。

---

# D-ε —— MESIF 规约推导表(Phase 1 产物;**逐格可追认**)

> **本节的地位**:本节是**推导依据与证据**(per 架构文档契约:推导 / 逐格裁决表 ⇒ 落 design,不进架构正文)。
> **规范性载体** = `include/tlm/cep/mesif_state_spec.hh`(Phase 2 产物)—— 二者的**逐格一致性**由 Phase 3 的
> `MS-05` 判据**机器校验**(解析本节的两个机器可读块,与载体逐格比对)⇒ **无双真源漂移**。
>
> **本节全部行号引用均于 2026-10-29 对 ArchForge 仓**逐条实测核验**(非记忆)。

## D-ε.1 建模约定(逐条裁决)

| # | 约定 | 内容 | 依据 |
|---|------|------|------|
| **C1** | 5 态含义 | `I` 无有效副本 · `S` 干净共享副本(他处可能还有 sharer,本节点**非** Owner) · `E` 干净独占(本节点是**唯一**持有者) · `M` 脏独占(唯一持有者且数据与内存不一致) · `F` 干净 Owner **且**存在其他 sharer(本节点被目录指定为**转发点**) | `ArchForge 25-nsa:598`(状态词表)+ `25-nsa:599`(Forward = 单 Owner + 多 Sharer) |
| **C2** | 6 事件语义 | `Rd` 本地读(不改变本节点的所有权角色) · `RdUnique` 本地取独占数据(失效他人) · `Wr` 本地写(**仅独占态合法**) · `Upgrade` 本地从共享升级为独占 · `SnpShared` 远端请求共享副本 · `SnpUnique` 远端请求独占数据(失效全部) | 事件集与 CHI 同构(per D-β);语义按 MESIF 的**目录 + 转发点**模型定义 |
| **C3** | 写规则 | `Wr` **仅**在 `{E, M}` 合法;其余态(`I`/`S`/`F`)`Wr` ⇒ **状态不变** | `ArchForge 25-nsa:553-554`(Owner 唯一性约束)+ 同型先例 IHI0050 §B4.1(CHI「非法事件 ⇒ 不变」约定,per `include/tlm/cep/chi_rnf_adapter.hh:101-105`) |
| **C4** | 远端规则 | `SnpUnique` ⇒ 任何有副本的态 → `I`;`SnpShared` ⇒ 干净 Owner(`E`)保留 Owner 身份并落入 `F`、脏 Owner(`M`)**先写回**再降为 `S`、纯 sharer(`S`)保持 `S` | `ArchForge 25-nsa:553`(Forward 在 Owner 转移时清空)+ `25-nsa:599`;`M` 的写回要求同型先例 `include/tlm/cep/chi_rnf_adapter.hh:111`(`UD+SnpShared→SC`) |
| **C5** | 目录位域边界 | ArchForge 的目录条目 State 字段是 **12 bits**(`M/E/S/I/F` **+ 子状态**,`ArchForge 25-nsa:539`)⇒ 本规约只建模 **5 个粗粒度稳定态**;**子状态**属**协议私有**,不泄漏到 4 值 `perm`(per `ADR-CEP-05` 的 `INV-CEP-05-IC1`) | `ArchForge 25-nsa:539` + `ADR-CEP-05` `INV-CEP-05-IC1` |
| **C6** | 瞬态 | 本规约**不声明瞬态**(5 态**全部**稳定)⇒ 瞬态无投影**平凡成立**;载体**保留**瞬态列表以支持未来追认后的扩展 | 与 CHI 的 `kTransientStates` 同款(per `include/tlm/cep/chi_rnf_adapter.hh:127-128`) |

## D-ε.2 依据强度分级(`basis.kind`)

| kind | 含义 | 判定标准 | `pending` |
|------|------|----------|----------|
| **`Primary`** | **一手原文**:该格结论可由 ArchForge 原文定义**直接**推出,**无需任何额外假设** | `ref` 必含 ArchForge 定位锚点(`号:` / `nsa:`) | `false` |
| **`Structural`** | **结构模板推导**:以 MOESI 6×6(`ArchForge 09 号 §3.3:148-156`)为**结构模板**按 MESIF 的 5 态 + Forward 语义裁剪 | `ref` 指向模板行 | `true` |
| **`Derived`** | **本仓推导**:依 MESIF 的目录 / 本地-远端事件语义推出 | `ref` 指向推导所依据的原文或先例 | `true` |
| **`Analogical`** | **类比推导**:依 ArchForge 的 Owner 唯一性约束 + 同型协议先例(CHI) | `ref` 同时指向 ArchForge 约束与外部先例 | `true` |

> ⚠️ **`pending` 与 `kind` 的双向约束**(Phase 3 的 `MS-03` 判据 ②③ 逐格检查):
> `kind == Primary` ⇒ `pending == false`;`kind != Primary` ⇒ `pending == true`。
> ⇒ 「一手原文」与「待追认」**不得**错配(否则跨仓债被静默放过,或已确认项被反复追认)。

## D-ε.3 5 态定义(逐态一手依据)

| 态 | 含义 | 一手依据 |
|----|------|----------|
| `I` | Invalid —— 无有效副本 | `ArchForge 25-nsa:598` |
| `S` | Shared —— 干净共享副本(非 Owner) | `ArchForge 25-nsa:598` |
| `E` | Exclusive —— 干净独占(唯一持有者) | `ArchForge 25-nsa:598` |
| `M` | Modified —— 脏独占(唯一持有者) | `ArchForge 25-nsa:598` |
| `F` | Forward —— 干净 Owner + 存在其他 sharer(转发点) | `ArchForge 25-nsa:598` + `25-nsa:599` |

## D-ε.4 6 事件定义

| 事件 | 方向 | 语义 | 依据 |
|------|------|------|------|
| `Rd` | local | 本地读 | 事件集同构于 CHI(per D-β);`ArchForge 09 号 §3.3:149` 为模板 |
| `RdUnique` | local | 本地取独占数据(失效他人) | 同上 |
| `Wr` | local | 本地写(**仅独占态合法**) | `ArchForge 25-nsa:553-554` |
| `Upgrade` | local | 本地从共享升级为独占 | 同型先例 IHI0050 §B4.1 |
| `SnpShared` | remote | 远端请求共享副本 | `ArchForge 25-nsa:599` |
| `SnpUnique` | remote | 远端请求独占数据(失效全部) | `ArchForge 25-nsa:553` |

## D-ε.5 状态转换表的**推导逻辑**(逐 kind;**逐格 basis 在载体**)

> ⚠️ **单一真源纪律**:30 格的**逐格** `basis`(kind / ref / note)**不**在本节复写 ——
> 规范性载体 `include/tlm/cep/mesif_state_spec.hh` 是**唯一真源**。
> 理由:本 change 归档后 `openspec/changes/archive/**` **不入库**(per `.gitignore`)⇒
> 若逐格表留在本节,归档即**断链**(判据将无从校验载体与推导记录的一致性)⇒ 载体与本节**不得**双写逐格表。
> 本节只承载**推导口径**与**统计**,统计由 Phase 3 的判据**逐数核对**载体(漂移即红)。

| kind | 推导规则 | 代表格 | 格数 |
|------|----------|--------|------|
| `Primary` | 结论可由 ArchForge 原文定义**直接**推出,**无需任何额外假设** | `E+SnpShared→F`(由「`F` = 单 Owner + 多 Sharer」直接得出)· `F+SnpShared→F` · `F+SnpUnique→I`(由「Forward 在 Owner 转移时清空」得出) | 3 |
| `Structural` | 以 MOESI 6×6(`ArchForge 09 号 §3.3:148-156`)为**结构模板**,按 MESIF 的 5 态 + Forward 语义裁剪;模板行号随 `kind` 逐格登记 | `I+Rd→S`(模板 `:150`)· `S+Rd→S`(`:151`)· `E+Rd→E`(`:152`)· `M+Rd→M` / `M+Wr→M`(`:153`)· `F+Rd→F`(MOESI `O` 行 `:154`) | 7 |
| `Derived` | 依 MESIF 的**目录 / 本地-远端事件语义**推出;每格的 `note` 写明所依原文或先例 | `S+Wr→S`(写非法 ⇒ 不变)· `M+SnpShared→S`(脏 Owner 先写回)· `I+SnpUnique→I` | 15 |
| `Analogical` | 依 **ArchForge 的 Owner 唯一性约束** + **同型协议先例(CHI)** 共同推出 | `E+Wr→M`(静默升级)· `F+Wr→F`(`F` 非独占)· `F+RdUnique→E` · `F+Upgrade→E` · `S+Wr→S` | 5 |

## D-ε.6 转换表的**逐类裁决示例**(便于复核,非规范)

| 格 | 裁决 | 理由 |
|----|------|------|
| `E + Wr → M` | 静默升级 | `E` 是**唯一**干净独占态 ⇒ 写 ⇒ 脏独占;**无需总线事务**(无副本可失效)⇒ 与 `moesi_gpu` 的 `E+Wr→M`(`ArchForge 09 号 §3.3:152`)同构 |
| `S + Wr → S` | 写非法 ⇒ 不变 | 写只在独占态合法(per C3);须先 `Upgrade` 到 `E` |
| `F + Wr → F` | 写非法 ⇒ 不变 | `F` 有 sharer ⇒ **非独占**;须先 `Upgrade` 到 `E` |
| `E + SnpShared → F` | 保留 Owner 身份 | MESIF 的 **Forward 语义**:唯一 Owner 收到远端共享请求 ⇒ Owner 仍单 ⇒ 落入 `F` 定义 |
| `M + SnpShared → S` | 先写回再共享 | 脏 Owner 不可直接共享 ⇒ 写回后降为 `S`(同型先例 CHI `UD+SnpShared→SC`) |
| `F + RdUnique → E` / `F + Upgrade → E` | `F` 清空 ⇒ 干净独占 | sharer 全失效 ⇒ `Forward` 清空(per `ArchForge 25-nsa:553`)⇒ 本节点成唯一干净持有者 |



## D-ε.7 投影表的**推导口径**(逐态依据;**结论在载体**)

| 强度 | 依据 | 覆盖 | 格数 |
|------|------|------|------|
| `Primary` | `ArchForge 32 号:529` —— 一手原文「`F`(Forward)态 → `perm` 投影为 `WRITEBACK`」 | `F` | 1 |
| `Structural` | `ArchForge 32 号 §7.2:582-591` 的 MOESI 6→4 表 —— `I`/`S`/`E`/`M` 四态**状态同义**,故同义迁移 | `I`/`S`/`E`/`M` | 4 |

> ⚠️ **4 值锁不破**:`F` 与 `M` 同投影 `WRITEBACK` ⇒ `perm` **仍为 4 值**(per `ADR-CEP-02` CEN-Ib11);
> **不得**为 `F` 新增第 5 个 `perm` 值(那会把「投影的表达力损失」变成「接口面扩张」)。

## D-ε.8 与 CHI / MOESI 模板的**偏离清单**(追认优先级最高)

> ⚠️ **单一真源纪律**:偏离清单的**逐条数据**(格 / 模板取值 / 本表取值 / 理由)由载体的
> `kDeviations[]` 承载(与转换表同源 ⇒ 不得脱节);本节只留**复核视角的说明**。

| # | 格 | 模板取值 | 本表取值 | 偏离理由 | 风险 |
|---|----|----------|----------|----------|------|
| **D1** | `I + SnpShared` | CHI `→SC`(`chi_rnf_adapter.hh:108`) | `→I` | CHI 该格把「请求者拿到共享副本」写进了**响应者**的行;本表按语义正确性取「无副本不因被 snoop 而获得副本」 | 低(CHI 是**本地/远端混合**的建模简化) |
| **D2** | `E + SnpShared` | CHI `UC+SnpShared→SC` | `→F` | MESIF 的 **Forward 语义**:干净 Owner 保留 Owner 身份成为转发点(CHI 的 RN 侧无转发点概念) | **高** —— 这是 MESIF 相对 CHI 的**核心差异** |
| **D3** | `S + RdUnique` | CHI `SC+RdUnique→SC` | `→E` | MESIF 的**目录**语义:取独占 ⇒ sharer 全失效 ⇒ 本节点成唯一持有者 | 中 |
| **D4** | `F` 行的 6 格中的 5 格(`RdUnique` / `Wr` / `Upgrade` / `SnpShared` / `SnpUnique`) | CHI **无对应态**(`F` 无对应) | 见载体 `kStateTransition` 的 `F` 行 | MESIF 特有(`F` = 转发点)⇒ 这 5 格是**纯本地推导**,追认价值最高(仅 `F+Rd→F` 与 MOESI 的 `O+Rd→O` 同构) | **高** |

## D-ε.9 语义损失登记(2 项;Phase 3 的 `MS-04` 判据逐项检查)

**损失 ① —— `F` 与 `M` 同投影 `WRITEBACK`**

- **后果**:「是否还有其他 sharer」在 4 值接口上**不可分辨**。
- **补偿**:**可由 Sharer Tracker 补偿** —— `mesif` 的 `sharer_tracker.impl = HierarchicalDirectory`
  (caps `SYSTEM` / `EXACT` / `GLOBAL` / `HIERARCHICAL`,per `ADR-CEP-04 §13`)⇒ EXACT + GLOBAL 目录**能**回答「谁持有」。
- ⚠️ **措辞纪律**:**不得**沿用 `chi_rnf` 的「**不可补偿**」措辞 —— `chi_rnf` 的 `sharer_tracker` 是「不参与」
  (RN-F 不维护 sharer set)⇒ 那是**协议信息拓扑的固有损失**;而 `mesif` 的损失是**投影的表达力损失**,**可补偿**。
  (对照:`moesi_gpu` 的 `O`/`M` 合并损失由 `SparseSnoopFilter`(`scope=SYSTEM`)补偿,per `ArchForge 32 号 §7.2:594-595`。)

**损失 ② —— `F → E` 的**秩回落**(本地取权路径上)**

- `F`(perm = `WRITEBACK`,秩 3)在 `RdUnique` / `Upgrade` 后落到 `E`(perm = `UNIQUE`,秩 2)⇒ 观测到的秩**下降 1**。
- **成因**:`F` **没有**独立的 `perm` 值(与 `M` 同投影)⇒ 当 `F` 退出转发点角色落到 `E` 时,秩从 `M` 的秩掉到 `E` 的秩。
- **处置**:这是损失 ① 的**次生效应**,**显式登记**为**唯一允许的秩回落例外**(per D-ε.10);**不得**静默放过,亦**不得**通过「给 `F` 新增第 5 个 `perm` 值」来消除(per `ADR-CEP-02` CEN-Ib11 的 4 值锁)。

## D-ε.10 `perm` 秩的**精确口径**(替代「秩单调」的粗口径)

令 `rank(NONE)=0 < rank(READ)=1 < rank(UNIQUE)=2 < rank(WRITEBACK)=3`。判据:

| # | 约束 | 范围 | 依据 |
|---|------|------|------|
| ① | 秩**单调不降** | **本地事件** `{Rd, RdUnique, Wr, Upgrade}` 全体 | 远端 snoop 必然降秩(失效 / 降级),不可纳入 |
| ② | `E + Wr → M` **严格升高**(2 → 3) | 单格 | `E` 是唯一干净独占态 ⇒ 写 ⇒ 脏独占 |
| ③ | **唯一允许的例外** = `F → E`(`RdUnique` / `Upgrade` 两格) | 显式登记 | 损失 ②(D-ε.9) |

> ⚠️ 粗口径「所有事件的秩都不降」**不可满足**(`M + SnpShared → S` 为 3 → 1)⇒ 口径必须限定到**本地事件**。
> 本节即该口径的**规范化来源**(Phase 3 的 `MS-02` 子条件 ④ 按此实现)。

## D-ε.11 统计(Phase 3 的 `MS-03` 判据 ③ 逐格核对)

| 项 | 转换表(30 格) | 投影表(5 格) | 合计(35 格) |
|----|---------------|---------------|--------------|
| `Primary`(一手原文,`pending=false`) | **3**(`E+SnpShared` / `F+SnpShared` / `F+SnpUnique`) | **1**(`F→WRITEBACK`) | **4** |
| `Structural` | 7 | 4 | 11 |
| `Derived` | 15 | 0 | 15 |
| `Analogical` | 5 | 0 | 5 |
| **`pending=true`(待追认)** | **27** | **4** | **31** |

> ⚠️ **本节统计 SHALL 与载体逐数一致**(Phase 3 的 `MS-03` 判据核对 kind 分布与 pending 计数)⇒ 载体改动 ⇒ 本节必须同步更新。
>
> ⚠️ **31/35 待追认是**如实登记**的结果**,不是缺陷**:ArchForge 侧本就没有 MESIF 转换表与投影表
> (per 立项证据 G1 / G2)。⇒ 该计数是**跨仓债的量化刻度**,随 ArchForge 逐格追认而下降。
