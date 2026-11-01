# Design — cpptlm-cep-tilelink-state-spec v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#11** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/cep-tilelink-state-spec/spec.md`](specs/cep-tilelink-state-spec/spec.md)
> **口径**: 台账 **Entry #134** / **#135** / **#138**
> **basis**: `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(唯一人工审计面)
> **估时**: 4 周(规约层)· **KPI**: Proposed 并发 **0/3 → 1/3**

---

## D-α 稳定态集合 = **3 值**(`Nothing` / `Branch` / `Tip`);`Trunk` = **过渡态**

### 依据

D1(1.7.1)§8.1.4 `Table 8.3` 的转移记号全集 = **N / B / T** ⇒ **无任何 `Trunk` 参与的转移**
(`Cap` = `toT`/`toB`/`toN` · `Grow` = `NtoB`/`NtoT`/`BtoT` · `Prune` = `TtoB`/`TtoN`/`BtoN` ·
`Report` = `TtoT`/`BtoB`/`NtoN`)⇒ **`Trunk` 不参与任何转移**。
D1 §8.1.1 的「`Acquire` 操作**长树**,或**延长 trunk** 或从既有 branch/tip **新增 branch**」
⇒ `Trunk` 是**生长过程**中的路径节点 ⇒ **过渡态**。

### ⚠️ 已登记的规范内部矛盾(R-T5)

| # | 矛盾 | 处置 |
|---|---|---|
| 1 | D1 §8.1.4 正文说转移分「**six** subsets」,而同表列 **4** 类(下一段又说 "four") | 以**转移记号**为准;矛盾**显式登记** |
| 2 | D1 `Table 8.3` 表头写 `Permissions: None, Branch, **Trunk**`,而全部转移记号用 **T = Tip** | **表头疑为笔误** ⇒ 以**转移记号**为准;若规范本意含 `Trunk` ⇒ 态数变 4 ⇒ **判据须能抓住**(反向 fixture:加回 `Trunk` ⇒ 必红) |

⇒ **判据设计要求**:稳定态集合的**断言必须逐字列出 3 个名字**(而非「恰为 3 个」)
⇒ 这样才**有鉴别力**(换掉任一个名字都红),否则「加回 `Trunk` 仍 3 个」的漂移**不可分辨**。

---

## D-β 事件词表 = **4 类**(`Cap` / `Grow` / `Prune` / `Report`)

### 依据

D1 §8.1.4 `Table 8.3` 的 4 个 Category 行 + 其后的逐类释义:

| 类别 | 释义要点(据 D1 原文) |
|---|---|
| `Cap` | 权限变化**不指定原权限**,只指定**应变成什么**(`toT`/`toB`/`toN`) |
| `Grow` | 权限**升级**,**长树**,记录原权限与新权限 |
| `Prune` | 权限**降级**,**缩树**,记录原权限与新权限 |
| `Report` | **空操作**,权限不变,但**报告当前权限** |

⇒ **比 CXL 的 17 事件、MESIF 的 6 事件都更紧** ⇒ 载体可产出 `pending=false` 格。

### ⚠️ `Cap` 的**源不定**语义 ⇒ 载体形态的关键设计

`Cap` 的 3 条转移(`toT`/`toB`/`toN`)**不指定源态** ⇒ 在 3 态 × 4 事件矩阵里,
`Cap` 的 **3 个格**(源 = N / B / T)**每格的目标是 3 元集合**,而非单值。

⇒ ⇒ **载体不能把 12 格都建成「单值」** —— 否则 `Cap` 的格**无法表达**。
⇒ ⇒ **载体形态决策**:每格存**目标态集合**(bitmask 或「源不定」标记)
⇒ ⇒ **12 格仍全 Known**(每格都有规范枚举的转移支撑),但 `Cap` 的 3 格是**多目标**。

> ⚠️ **这一条与队列 #10 的载体形态不同** —— #10 的 68 格是**单值**(目标态或 `Unknown`/`Illegal`)。
> ⇒ 本载体引入**第三种格语义**:`Known-single` / `Known-multi` / `Unknown`。
> ⇒ **必须显式登记**,否则后人会按 #10 的模板误判。

---

## D-γ 逐格覆盖 ⇒ **12 格全 Known、零 `Unknown`**

### 依据

| 事件 | 规范枚举的转移 | 覆盖的有序对 |
|---|---|---|
| `Grow` | `NtoB` · `NtoT` · `BtoT` | (N,B) (N,T) (B,T) |
| `Prune` | `TtoB` · `TtoN` · `BtoN` | (T,B) (T,N) (B,N) |
| `Report` | `TtoT` · `BtoB` · `NtoN` | (T,T) (B,B) (N,N) |
| `Cap` | `toT` · `toB` · `toN` | **源不定** ⇒ 3 格 × 3 目标 |

⇒ `Grow` + `Prune` + `Report` = **9 条**,恰好覆盖 {N,B,T} 上的**全部 9 个有序对**
⇒ `Cap` 另给 3 条**通配**转移 ⇒ ⇒ **12 格全部有规范枚举支撑 ⇒ 零 `Unknown`**。

### ⚠️ `Grow` / `Prune` 的**部分格为空集**

`Grow` 的 3 条覆盖 (N,B) (N,T) (B,T) ⇒ **源 = T 的 `Grow` 格为空集**(Tip 已是最高权限,无从再「长」)
⇒ 同理 **源 = N 的 `Prune` 格为空集**(Nothing 无从「缩」)。
⇒ ⇒ **载体必须能表达「空集」这一合法态** ⇒ **不是** `Unknown`(「规范未覆盖」)
⇒ ⇒ **第三种语义再次必需**:`Known-empty`(= 规范**明确**该转移不存在)

⇒ ⇒ **格语义共 4 种**:`Known-single` / `Known-multi` / `Known-empty` / `Unknown`
⇒ ⇒ **编译期锁**:`kUnknownCellCount == 0` + `kEmptyCellCount == 2`
⇒ ⇒ **判据须分别断言「空集格恰为 2 个」与「`Unknown` 格为 0」** —— 合并断言则**不可分辨**。

---

## D-δ `perm` 投影 = **3→3**;`UNIQUE` **不可达**

| 稳定态 | 副本权限 | 独占? | 可能脏? | 投影(**保守**) | 强度 |
|---|---|---|---|---|---|
| `Nothing` | None | — | — | **`NONE`** | direct |
| `Branch` | Read | 否 | 否(只读副本,非写者) | **`READ`** | direct |
| `Tip` | Read+Write | **是**(唯一写者) | **是** | **`WRITEBACK`** | direct(独占)+ pending(干净性) |

### 依据

D1 §8.1「The fundamental permissions it is possible for a particular agent's copy of a block to have are
**None, Read, or Read+Write**」+ §8.1 的节点类别描述(`Tip` = 「the **point of memory access serialization**」
+ Read/Write 权限 + **may contain dirty data**;`Branch` = 只读副本)。

### ⚠️ `UNIQUE` 不可达 ⇒ 三条结构性结论

1. **保守降级方向唯一** —— 规范**只说**副本「**may** contain dirty data」⇒ **不承诺**干净/脏
   ⇒ ⇒ **`UNIQUE`(干净独占)不可确证** ⇒ ⇒ **永不出现**
2. ⇒ ⇒ **该实例不声明 `UNIQUE → WRITEBACK` 静默升级**
   ⇒ ⇒ **不触发** `INV-CEP-08-IC2`(该 invariant 的触发条件 = 「声明 `UNIQUE` **且**声明静默升级」)
   ⇒ ⚠️ 与 `chi_rnf` / `cxl_2_0`(均声明了静默升级)**形态不同** ⇒ 属**合法差异**,非不一致
   ⇒ ⚠️ **规约层不做这个判定** —— 它属**实例声明**(第 1 个 change);规约层只**记录**「`UNIQUE` 不可达」这一事实
3. **「`perm` 4 值锁」是值域锁** —— schema 的 `perm` 是 `array` + `minItems: 1` + 枚举项
   ⇒ **无**「必须列全 4 值」的闸 ⇒ 本协议族的**投影**只用 3 值 **不违反**值域锁
   ⇒ ⚠️ 但「实例 `perm` 数组填 3 值还是 4 值」是 **R-T4**,**不在本 change 内**(见 D-⑥)

---

## D-ε 非对称结构断言 = **TileLink 形态**(权限变更的发起方)

### 依据(per `ADR-CEP-17` 的 `## Status Update(2026-11-01,per 台账 Entry #138)`)

| 维度 | TileLink-C 的表述 | 可判形式 |
|---|---|---|
| ① 权限**授予**方向 | **只有 slave 能授予** | `Grant` / `GrantData` **只**出现在 **D 通道** |
| ② 权限**强制撤销**发起方 | **只有 slave 能发起** | `Probe` **只**出现在 **B 通道** |
| ③ request 类消息方向 | **双向**(A 与 B)⇒ **不可**用 CXL 式定义 | ❌ 不作为判据 |

### ⚠️ **不得复用队列 #10 的 CXL 形态**

| 协议族 | 非对称维度 | 判据形态 |
|---|---|---|
| **CXL 2.0** | 「request 类消息的**方向**」 | `H2D Req` **只含 snoop** ∧ `D2H Req` **只含 local** |
| **TileLink-C** | 「**权限变更的发起方**」 | `Grant` ∪ `Probe` **只**出现在 slave→client 的通道 |

⇒ ⇒ 若实现者把 #10 的模板直接搬过来 ⇒ 会断言「request 类消息方向单向」⇒ **在 TileLink 上恒假**
⇒ ⇒ **反向 fixture 必须覆盖这一条**:把断言换成 CXL 形态 ⇒ **必须因正确原因红**。

---

## D-ζ ⛔ 合规边界:TileLink **无** CXL 类禁令(但仍守 IP 边界)

| 项 | 内容 |
|---|---|
| **合规** | SiFive 明示 TileLink 是「a **free and open standard**」(D1/D2 §1)⇒ **规范正文可自由投喂 AI** ⇒ 本 change 的 basis **直接来自规范** ⇒ 与队列 #10 **完全相反** |
| **⚠️ IP 边界** | 规范 © SiFive Inc.(D1: © 2018;D2: © 2023)**非** CC-BY ⇒ 载体与摘要仍**只记条款级定位 + 断言摘要**,**不整段复制**原文段落 |
| **强度三档** | `direct` / `analogical` / `inferred` ⇒ **禁止升格**;本 change 的逐格 `basis` **全部 `direct`** |
| **钉版** | `pin=strong` 三份(D1 sha256 `30c09b5f…a2dd` · D2 `0d28c6f7…` · D3 `8caab6cc…`)⇒ **载体 `public-direct` 只接受 `pin=strong`** |

---

## D-⑦ 载体**零自有表副本**的消费方向预留(为第 1 个 change)

`cxl_state_spec.hh` 的先例:载体**只**提供**结构**(态 / 事件 / 投影 / 断言 / `basis` 自洽),
**不**持有 adapter 的实现状态 ⇒ 第 1 个 change 的 `TileLinkCAdapter` **消费**载体,
**不复制**其转移表。

⇒ ⇒ 本载体**不得**引入任何**运行期状态**(纯 `static constexpr`)⇒ 便于第 1 个 change 直接复用。

---

## D-⑧ 已排除的替代方案

| 方案 | 排除理由 |
|---|---|
| **把 `Trunk` 作为第 4 个稳定态** | 转移记号全集中**无** `Trunk` ⇒ 它是**过渡态**;纳入会让 12 格变 16 格且**引入无规范支撑的格** ⇒ 违背「零 `Unknown`」目标 |
| **用 CXL 式「request 方向」判非对称** | TileLink 的 A/B **都是** request 类 ⇒ 该断言**恒假** ⇒ 形态**不可套用**(per Entry #138) |
| **扩 `perm` 值域到 5 值**(加 `TREE` 之类) | `CEN-Ib11` 的 4 值锁是**冻结契约** ⇒ 扩值域 = 击穿冻结面 ⇒ **禁止** |
| **把 12 格建成全单值矩阵** | `Cap` 的**源不定**语义**无法**用单值表达 ⇒ 会丢信息 ⇒ 需 `Known-multi` 格语义 |
| **不引入 `Known-empty` 格语义** | `Grow`(源=T)与 `Prune`(源=N)**规范明确**为空集 ⇒ 与 `Unknown`(规范未覆盖)**语义不同** ⇒ 混用即**丢失可判别性** |

---

## 已排除的替代方案(范围层)

| 方案 | 排除理由 |
|---|---|
| **规约层 + 实例声明合并为一个 change** | 实例声明依赖 **R-T4** 裁决(未闭合)⇒ 合并 ⇒ 规约层被阻塞 ⇒ **拆开** |
| **只做实现层,跳过规约层** | 实现层必然**自推** `TreeManager` 语义(per `ADR-CEP-04 §5` 的「善意撒谎」告警)⇒ 重蹈 #9 覆辙 |
