# Design — cpptlm-cep-tilelink-state-spec v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#11** 第 0 个 change(规约层)
> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/cep-tilelink-state-spec/spec.md`](specs/cep-tilelink-state-spec/spec.md)
> **口径**: 台账 **Entry #134** / **#135** / **#138**
> **basis**: `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(唯一人工审计面)
> **估时**: 4 周(规约层)· **KPI**: Proposed 并发 **0/3 → 1/3**

---

## D-α 稳定态集合 = **3 值**(`Nothing` / `Branch` / `Tip`);`Trunk` **折叠**为 `Branch`(**决策口径**)

> ⚠️ **2026-11-01 自审更正**:本节原写「`Trunk` = **过渡态**」—— 这是**推断**,且与 D1 §8.1 原文冲突:
> «Every node in the graph falls into one of **four** categories describing its position on the tree»
> ⇒ `Trunk` 是 **4 类持久节点之一**,**不是**过渡态。
> ⇒ 结论不变(**仍不进稳定态集合**),但**依据与措辞必须改**(见下三步 + 措辞纪律 + 语义损失)。

### 依据(三步推导,**全部 `direct`**)

1. **`Table 8.1`(D1 §8.1)**:`Trunk` 与 `Branch` 的**访问权限完全相同**(均仅 `Get`);
   只有 `Tip` 是 `Get, PutPartial, PutFull, Logical, Arithmetic` ⇒ 权限层面 `Trunk` ≡ `Branch`
2. **`Table 8.3`(D1 §8.1.4)**:转移记号只有 **N / B / T**,且 `Grow`/`Prune` 按规范释义必须**单调**
   (`Grow` = "permissions **upgrades** that grow the tree" · `Prune` = "permissions **downgrades**
   that shrink the tree")⇒ ⇒ `T` 的**唯一自洽读法是 `Tip`** —— 若 `T` = `Trunk`,`BtoT` 两边都只有 `Get`
   (**不构成升级**),且该表**无法表达任何进入 `Tip` 的转移** ⇒ 无法表达「取得写权限」
3. ⇒ **稳定态 = {`Nothing`, `Branch`, `Tip`}**;`Trunk` 在**权限转移模型**中**折叠**为 `Branch`

### ⚠️ 措辞纪律(硬性 —— 二审发现原稿把「决策」写成了「规范断言」)

| ❌ 禁止表述 | ✅ 正确表述 |
|---|---|
| 「`Trunk` 是**过渡态**」 | 「`Trunk` **是** D1 的 4 类持久节点之一;我们**决定**在权限转移模型中**折叠**为 `Branch`」 |
| 「依据 = 记号全集中无 `Trunk`」(**单一弱证据**) | 「依据 = `Table 8.1` 权限等价 + `Table 8.3` 单调性要求」(两条 `direct`) |
| 不登记代价 | **必须**登记 §1.2bis 三条语义损失(**副本存在性 / 脏数据 / 树位置**) |

### ⚠️ 已登记的规范内部矛盾(R-T5)

| # | 矛盾 | 处置 |
|---|---|---|
| 1 | D1 §8.1.4 正文说转移分「**six** subsets」,而同表列 **4** 类(下一段又说 "four") | 以**转移记号**为准;矛盾**显式登记** |
| 2 | D1 `Table 8.3` 表头写 `Permissions: None, Branch, **Trunk**`,而全部转移记号用 **T = Tip** | **图例行笔误** —— `Table 8.1` 证明 `Trunk` 与 `Branch` 权限相同 ⇒ 图例应为 `None, Branch, Tip` (或直接给字母 N/B/T)⇒ 以**转移记号**为准;反向 fixture:加回 `Trunk` ⇒ 必红 |
| 3 | D1 §8.1.4 正文说「**six** subsets」,而同表列 **4** 类(下一段又说 "four") | **规范内部笔误**,以 **4 类**为准;⚠️ **但同段末尾的穷尽性句成立** —— 「Table 8.3 shows **all the permissions transitions any coherence policy based on TileLink could want to express**」 ⇒ `Known-empty` 2 格可判 **`direct`**(不依赖「以记号为准」的推断) |

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
> ⇒ 本载体**不能**用「单值 + 标记」形态(见下),改用 **`kind` + 目标位掩码**。
> ⇒ **必须显式登记**,否则后人会按 #10 的模板误判。

### ⚠️ 载体形态决策(二审修订 —— 原「4 值格语义枚举」被否决)

| 形态 | 裁决 | 理由 |
|---|---|---|
| 单值 + `Multi`/`Empty` 标记 | ❌ **否决** | 允许 `single ∧ empty` 这类**非法组合**(标记与值可矛盾)⇒ 引入无法判定的状态 |
| **4 值格语义枚举**(`Known-single`/`Known-multi`/`Known-empty`/`Unknown`) | ❌ **否决** | 同上:枚举本身不阻止「`Known-single` 却存了 2 个目标位」⇒ 非法组合**靠约定**而非**靠类型** |
| **2 值 `kind ∈ {Known, Unknown}` + 3 位目标掩码**(`n`,`b`,`t`) | ✅ **采用** | **零非法组合**:`kind=Known` ⇒ 掩码任意(0/1/2/3 位都合法,分别表达 **空集 / 单值 / 多值**);`kind=Unknown` ⇒ 掩码 **MUST** 为 0 ⇒ 「规范明确空集」与「规范未覆盖」由 **`kind`** 区分、由掩码**内容**细分,二者**结构性可分** |

⇒ ⇒ **派生量**(全部可由表推出、**不单独存储**):

| 量 | 定义 | 期望值 |
|---|---|---|
| `kKnownCellCount` | `kind == Known` | **12** |
| `kUnknownCellCount` | `kind == Unknown` | **0** |
| `kEmptyCellCount` | `kind == Known` ∧ `popcount(mask) == 0` | **2** |
| `kSingleCellCount` | `kind == Known` ∧ `popcount(mask) == 1` | **5** |
| `kMultiCellCount` | `kind == Known` ∧ `popcount(mask) >= 2` | **5** |

> ⚠️ **二审抓到的算错**:原稿写 `single 6 / multi 3 / empty 2 / unknown 0`(合计 **11 ≠ 12**)。
> 正确为 **`single 5 / multi 5 / empty 2 / unknown 0`**(合计 12)—— 逐格见 D-γ。

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
⇒ ⇒ 故用 `kind == Known` ∧ 掩码 `= 0` 表达(`kind` 区分**是否规范明确**,掩码**内容**区分**空/单/多**)。

### 12 格逐格分桶(**唯一计数真源** —— 任何文档引用此数值须与此表一致)

| 事件 | 源 = `Nothing` | 源 = `Branch` | 源 = `Tip` |
|---|---|---|---|
| `Cap` | `{T,B,N}` = **3 位 → multi** | `{T,B,N}` = **3 位 → multi** | `{T,B,N}` = **3 位 → multi** |
| `Grow` | `NtoB`,`NtoT` = `{B,T}` = **2 位 → multi** | `BtoT` = `{T}` = **single** | 空集 = **empty** |
| `Prune` | 空集 = **empty** | `BtoN` = `{N}` = **single** | `TtoB`,`TtoN` = `{B,N}` = **2 位 → multi** |
| `Report` | `NtoN` = `{N}` = **single** | `BtoB` = `{B}` = **single** | `TtoT` = `{T}` = **single** |

⇒ ⇒ 合计:**`single` 5**(`Grow｜B` · `Prune｜B` · `Report`×3)· **`multi` 5**(`Cap`×3 · `Grow｜N` · `Prune｜T`)
· **`empty` 2**(`Grow｜T` · `Prune｜N`)· **`unknown` 0** ⇒ **5 + 5 + 2 + 0 = 12** ✓

⇒ ⇒ **编译期锁**:`kTransitionCellCount == 12` · `kUnknownCellCount == 0` · `kEmptyCellCount == 2`
· `kSingleCellCount == 5` · `kMultiCellCount == 5`
⇒ ⇒ **判据 MUST 五个量分别断言**(合并断言则「规范明确空集」与「规范未覆盖」**不可分辨**,
且 `single`/`multi` 混淆**不可发现**)。

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
| **把 `Trunk` 作为第 4 个稳定态** | ⚠️ **不是**因为「`Trunk` 是过渡态」(那是**推断**,且与 D1 §8.1
「`four` categories」冲突);真实理由是**决策**:`Table 8.1` 中 `Trunk` 与 `Branch` 权限**相同**
(均仅 `Get`)⇒ 权限层面**不可区分**;纳入会让 12 格变 16 格且**引入与 `Branch` 同权限的重复态**
⇒ 违背「每态可判别」目标 |
| **用 CXL 式「request 方向」判非对称** | TileLink 的 A/B **都是** request 类 ⇒ 该断言**恒假** ⇒ 形态**不可套用**(per Entry #138) |
| **扩 `perm` 值域到 5 值**(加 `TREE` 之类) | `CEN-Ib11` 的 4 值锁是**冻结契约** ⇒ 扩值域 = 击穿冻结面 ⇒ **禁止** |
| **把 12 格建成全单值矩阵** | `Cap` 的**源不定**语义**无法**用单值表达 ⇒ 会丢信息 ⇒ 需**多位掩码** |
| **空集与 `Unknown` 不区分**(不引入 `kind` 二分) | `Grow`(源=T)与 `Prune`(源=N)**规范明确**为空集 ⇒ 与 `Unknown`(规范未覆盖)**语义不同** ⇒ 混用即**丢失可判别性** |

---

## D-η ⚠️ **已知不兼容登记**:`AC-05` 判据族对 TileLink **恒红**

### 事实(代码实测,2026-11-01)

`include/tlm/cep/adapter_schema_contracts.hh` 的 `check_adapter_self_consistency`:

```cpp
if (e_write_target_state != 3) {          // 硬编码 3 = M 态
    line.verdict = AdapterContractVerdict::FAIL;
    line.detail  = "E + Write 的目标态 = ... (应为 3 = M)";
    return line;                          // ⚠️ 无 SKIP / UNKNOWN 分支
}
```

⇒ TileLink-C **无 `E` 态**(投影 3→3,`UNIQUE` 不可达)⇒ 该判据**恒红**。

### 后果(必须在规约层登记,不能留到实现层才发现)

| 若不登记 | 会发生什么 |
|---|---|
| 实现层跑 `AC-05` | **必然红** ⇒ 实施者面对「红闸」有 2 条路:(a) 诚实宣布判据不适用,(b) **填一个假的 `e_write_target_state = 3`** ⇒ **(b) 会产生「语义为假的绿灯」** |
| 平台自证 | 「`AC-05` PASS ⇒ 适配器自洽」这一结论对 TileLink **不成立** ⇒ 平台失去该维度的判别力 |

### 处置(boundary 明确)

| 面 | 归属 |
|---|---|
| **登记**「`AC-05` 族对 TileLink 恒红」+ 「**不得**通过填假值使其变绿」 | ✅ **本 change(规约层)** —— 落 `TSS-05` 子条件 ⑤ + 载体**偏离清单** |
| **族无关化**(给 `AC-05` 加 SKIP / UNKNOWN 分支,或按 caps 动态判定 `E` 态是否存在) | ⬜ **第 1 个 change(实现层)** —— 属适配器契约改造,不属规约层 |
| **两步授权** | ❌ **不需要** —— 已确认 `adapter_schema_contracts.hh` 与其 main spec **均不在 M1 冻结 40 件内** |

---

## 已排除的替代方案(范围层)

| 方案 | 排除理由 |
|---|---|
| **规约层 + 实例声明合并为一个 change** | **帕累托改进,非被迫拆分** —— ⚠️ `TSS-01` ~ `TSS-11` **无一依赖** R-T4
(规约层只锁 **3→3 投影**;R-T4 只影响**实例 `perm` 数组填几值**)⇒ 合并**不会**阻塞规约层;
拆开的真实收益 = 两段各自**独立可归档 / 独立可验收**,且规约层的 `direct` basis **不必等待**实例面裁决 |
| **只做实现层,跳过规约层** | 实现层必然**自推** `TreeManager` 语义(per `ADR-CEP-04 §5` 的「善意撒谎」告警)⇒ 重蹈 #9 覆辙 |
