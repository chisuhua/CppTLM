# CP-Q-1a 裁决记录 —— 事件字母表的**来源**与**扩权方**

> **性质**：**架构裁决记录**（`architect` 产出）—— 落实 `ADR-CEP-24` 决策 3.1 的**实施前置**（「事件字母表的来源与扩权方在本 ADR 中冻结为未定义 ⇒ 须另行决策」）。
> **⛔ 不是 ADR**：本裁决**不新增** contract 面（无新 schema / 无新 invariant）⇒ 其 contract 化落在 `CP-I-2`（SSP 声明面实施）⇒ 届时须走该 change 的常规审查。
> **⛔ 状态**：✅ **已 ratify**（2026-11-26 · 用户批准 R1~R4 全部建议，见 §6.1 的裁决记录）。
> **开工门**：✅ **已解除** —— R1~R4 现已 ratify ⇒ `CP-I-2`（S1 实施）**可立项**（仍受 objective §4.0 的排队与 openspec 并发 ≤ 1 约束）。
> **前置**：`ADR-CEP-24`（已签发 2026-11-26）· 台账 Entry #551 · objective `obj-cep-custom-protocol` 的 `CP-Q-1a`
> **审查**：⏳ oracle（裁决草案的失败模式）· ⛔ critic 不适用于本件（非 ADR / 无 contract 变更；其 contract 面由 `CP-I-2` 承担）

---

## 1. 裁决对象（`ADR-CEP-24` 决策 3.1 的两个待定项）

| # | 待定项 | `ADR-CEP-24` 的原话 |
|:--:|---|---|
| **Q1** | **事件字母表的来源** | 「**新建跨协议词表** vs **复用既有引擎事件集**（`mesif_protocol_engine.hh:189` 的 6 事件 / `cxl_state_spec.hh:69` 之一）」|
| **Q2** | **扩权方**（谁能新增事件）| 「该补齐**须另行决策**」|

⚠️ **Q1 的选项表述本身有一个前提需要先证伪**：「复用既有引擎事件集」隐含「既有事件集**可以**被复用」——**下表证明该前提不成立**。

---

## 2. 证据（实测 · 逐 `file:line`）

### 2.1 六份 `*_state_spec.hh` 的**三种建模风格**

| 文件 | 事件表达 | 规模 | 风格 |
|---|---|---|---|
| `mesif_state_spec.hh:60-67` | `enum class Event` | **6**（`Rd`/`RdUnique`/`Wr`/`Upgrade`/`SnpShared`/`SnpUnique`）| **请求式**（local / remote 二分）|
| `tilelink_state_spec.hh:71-76` | `enum class Event` | **4**（`Cap`/`Grow`/`Prune`/`Report`）| **权限转移式**（`Cap` = 只给**目标**权限，**源不定**，`:72` 逐字）|
| `cxl_state_spec.hh:76-98` | `enum class Event` | **17** = `kLocalEventCount` **14**（D2H Req 11 + Evict 3）+ `kRemoteEventCount` **3**（H2D snoop），`kEventCount = 14+3`（`:115-118`），转换矩阵 **68 格** = 17×4（`:119`）| **操作码式**（CXL 协议 opcode 直译）|
| `cxl_dcoh_state_spec.hh` | **无** `Event` 枚举 | — | **三视图状态空间**（`DevCacheState` / `DcohViewState` / `HomeDirState`，`:72/76/79`）+ 通道模型 |
| `nvlink_sharp_state_spec.hh` | **无** `Event` 枚举 | — | **聚合/集合语义**（`IngressDecision` / `RegisterResult` / `EpochState` / `OrderingRule`，`:223-329`）|
| `ualink_inc_state_spec.hh` | **无** `Event` 枚举 | — | **网内计算绑定**（`IncOp` / `Obligation` / `PendingId` / `CertificateId`，`:103-582`）|

### 2.2 三个决定性事实

**事实 1 —— 结构性一致、**词表**互不兼容**：三份事件式 state_spec 的**转移表形状相同**（`transition[state][event]` + `perm` 投影 + 逐格 `Basis`；TileLink 的 `kTransitionCellCount = kEventCount × kStateCount = 12`，`tilelink_state_spec.hh:111`）⇒ **差别只在词表**：6 / 4 / 17，且语义风格三者不同（请求式 / 权限转移式 / 操作码式）。

**事实 2 —— 「复用某一既有事件集」⇒ 表达力丢失**（逐条）：
- 复用 MESIF 的 6 ⇒ ⛔ 表达不出 TileLink 的 `Cap`/`Grow`/`Prune`/`Report`（权限转移）与 CXL 的 17 个操作码；
- 复用 TileLink 的 4 ⇒ ⛔ 表达不出 MESIF 的 `local/remote` 二分与 `SnpShared`/`SnpUnique` 区分；
- 复用 CXL 的 17 ⇒ ⛔ **最大的一份反而最不可复用**：⚠️ 它们**大多不是品牌名**（仅 **2/17** 带 CXL/Cache 味：`CLFlush` / `CacheFlushed`）⇒ ⛔ **不能**用「破坏协议中立」立论（该论证不成立）。**真问题 = 操作码粒度**：这 17 个编码了 **CXL 特定的 opcode 细分** —— ① `RdAny` 与 `RdOwn` 的**关系无公开来源**（`cxl_state_spec.hh:74-76` 逐字：「与 `RdOwn` 的**关系无公开来源** —— 摘要 §2 只列名不释义」）⇒ **该粒度本身语义不可确证、不可移植** ② `CLFlush` / `CacheFlushed` / `ItoMWr` / `WOWrInv` / `WrInv` 是 CXL 特定变体 ⇒ 复用 = 把 CXL 的 opcode 细分**强加**给所有用户协议（其中一部分语义还**不可确证**）。⚠️ **B7 的正确推理链**（oracle 指出本裁决未写全）：B7 的**字面**（`ADR-CEP-24:133`）只禁「通用层**代码**出现协议名」，而 SSP **数据**含协议名**合法** ⇒ ⛔ **不能**说「复用 CXL 破坏 B7」。真正的链是：**强制共享**的词表会被**通用解释器**消费 ⇒ 该词表**事实上进入通用消费路径** ⇒ 单一协议的 opcode 语义被强加给全部用户协议 ⇒ 撞 B7 的**立法意图**（协议中立）。裁决否的是「**强制共享**」形态，⛔ 不是「数据里出现协议名」。
⇒ **「复用」在表达力与协议中立两条上都不成立。**

**事实 3 —— 六份里**只有 3 份**是「稳定态转移表」**：另 3 份（DCoh / SHARP / UALink INC）**没有 `Event` 枚举**、不是转移表 ⇒ ⚠️ **SSP 声明面（= 稳定态转移表）在结构上只覆盖 6 份中的 3 份**。这是 `ADR-CEP-24` 未显式登记的事实，本裁决**必须**登记（否则「表达力 ≥ 手写面」会被误读为「6/6」）。

### 2.3 既有先例（说明「新建」不是新发明）

| 先例 | 形态 | 对本裁决的意义 |
|---|---|---|
| `chi_rnf_protocol_engine.hh:139-167` 的 `kChiOpcodeTable` | **25** 条（16 接受 + 9 拒收） `(ChiOpcode, name, ChiChannel, CanonicalFlow, accepted, reject, has_data)` | ⭐ **每协议自带词表 + 逐条映射到 canonical 10 流** 的**现存模式** ⇒ 「新建词表」= 把这个模式**泛化到 SSP 面**，不是发明新东西 |
| `tilelink_state_spec.hh:119-120` 的 `kEventNames` | 逐字名字表（「必须逐字断言…否则 `Tip`→`Trunk` 的漂移不可分辨」）| 词表漂移的**机器判据形态**先例 |
| `ADR-CEP-02` 的 canonical **10 流** | `Request` / `Completion` / `Snoop` / `SnoopResp` / `Writeback` / `ForwardData` / `BackInvalidate` / `AddressResolve` / `Barrier` / `SnoopCompletionAck` | 可作**语义锚**的公共层（但「事件→流」**不闭合**，见 §4 的 R2）|

---

## 3. 选项对比

| 案 | 形态 | 表达力 | 可机判性 | DSL 风险 | 判定 |
|---|---|---|---|---|---|
| **A. 新建**统一**词表**（一套事件名，全 SSP 必用）| 设计一个覆盖 6+4+17 语义的封闭词表（**~27** 事件）| 🟡 能覆盖已知 3 份，但**新协议须扩表** | 🟢 高（封闭集 + 逐字断言）| 🔴 **高** —— 表越扩越像 DSL 词表；且 `Cap` 的「源不定」这类参数化语义难以塞进固定事件名 | ❌ 不选（DSL 风险 + 增长失控）|
| **B. 复用**某一既有事件集 | 全 SSP 必用 MESIF 6 / TL 4 / CXL 17 之一 | 🔴 **低**（事实 2 已证伪）| 🟢 高 | 🟢 无 | ❌ **明确否决**（表达力丢失 + CXL 的 opcode 粒度不可移植；⚠️ **不用**「破坏协议中立」立论 —— 见事实 2 第 3 条）|
| **C. 每 SSP 自带事件集 + 强制 flow 映射** | 事件名自由（用户定），但**每事件必须**映射到 canonical 10 流之一 | 🟢 **高**（无上限）| 🟡 中（可判「有映射」，不可判「映射**语义正确**」）| 🟡 中 —— 「事件集 = open set」⇒ `ADR-CEP-24` 决策 3.1 要的「**确定**的事件集」落空 ⇒ 退回「自造 mini 词表」 | 🟡 可作**基线**，但不够 |
| **D（第四条路，oracle 指出本裁决漏列）. 以 canonical 10 流本身作事件集** | 事件 = 10 流 | 🔴 **低**（流 ≠ 迁移事件：`Rd` 与 `RdUnique` **同映射** `Request` 流，`chi_rnf_protocol_engine.hh:141-142` 实测 ⇒ 读共享/读独占的区分**坍缩**）| 🟢 高 | 🟢 无 | ❌ **显式否决**（= 案 C 的退化形态，表达力丢失已由事实 2 证）|
| **C′. 封闭核心锚 + 开放私有事件（须映射 + 须批准）** | ① **核心语义锚**（architect 定义、封闭、⛔ 实现者不得改）② SSP 可声明**协议私有事件**，但每个**必须**：(a) 映射到 canonical 10 流（先例 `kChiOpcodeTable`）(b) 指明**等价核心锚**或声明「无等价」⇒ **须 architect 批准** | 🟢 **高**（核心锚保底 + 私有事件扩面）| 🟢 **高**（三层判据：锚集封闭 + 每事件有映射 + 逐字断言）| 🟢 **低**（核心锚封闭 ⇒ 不是 open DSL；私有事件须批准 ⇒ 扩权受控）| ✅ **推荐** |

---

## 4. 裁决

### 4.1 Q1（来源）—— **新建**，形态取 **C′**

**裁定**：**新建**事件词表；⛔ **不复用**任何既有事件集（事实 2）。

**形态** = **封闭核心锚 + 开放私有事件**（C′）：
- **核心锚集**（architect 定义 · 封闭 · ⛔ 实现者/用户不得增删）—— 初版**建议**（⛔ 须 §6 ratify 后才定稿，草案仅供参考）：`ReadShared` · `ReadUnique` · `Write` · `WriteNoAlloc` · `Upgrade` · `SnoopShared` · `SnoopUnique` · `SnoopInvalidate` · `EvictDirty` · `EvictClean` · `Cap` · `Grow` · `Prune` · `Report`
- ⚠️ **实测覆盖缺口（oracle S2）**：14 锚对 MESIF 6 + TileLink 4 **全覆盖**；对 **CXL-2.0** 约 **8-9** 个事件（`RdCurr` / `RdAny` / `RdOwnNoData` / `CLFlush` / `CacheFlushed` / `WrCur` / `WrInv` / `WOWrInv` / `SnpCurr`）**无干净锚可指** ⇒ 走私有事件 ⇒ 这把 R3 的瓶颈从定性变**定量**（CXL 类协议每份约 8-9 个待批）。
- **协议私有事件**：SSP 可声明，但每个**必须**：① 映射到 canonical 10 流之一（**机器判据**）② 指明**等价核心锚**（或声明「无等价」⇒ **architect 批准**）③ 名字**逐字断言**（先例 `kEventNames`）
- ⇒ 这样「确定的事件集」= **核心锚的封闭集**；私有事件是**受控扩展**，⛔ 不是 open set。

### 4.2 Q2（扩权方）—— **architect**

**裁定**：新增**核心锚** = **architect** 裁决（新增语义 = `ADR-CEP-20:88` 的「新协议语义的设计」⇒ 走单向门）；新增**协议私有事件** = SSP 作者声明 + **architect 批准**（若声明「无等价核心锚」时）。

⛔ **实现者（builder）不得自创核心锚**；⛔ 用户在 SSP 里只能使用**已登记**的核心锚 + 经批准的私有事件。

### 4.3 依据

1. **事实 2** 直接否决「复用」（表达力 + 协议中立双失）。
2. **事实 1**（结构一致、词表不同）说明问题**恰在词表层** ⇒ 裁决必须落在词表上，而转移表结构无需动。
3. **先例 `kChiOpcodeTable`**（**25** 条）证明「每协议词表 + flow 映射」「逐字断言」是**现存模式** ⇒ C′ 是把已有模式泛化，⛔ 不是新发明。
4. **`ADR-CEP-24` 决策 1.3** 的排除（无 DSL / 解析器 / IR / 生成器 / 烘焙）⇒ 词表必须**封闭可枚举**（核心锚）而非 open DSL；C′ 满足。
5. **`ADR-CEP-24:32`** 逐字：「数据面**缺一个确定的消费集合** ⇒ ⛔ 不可实施（决策 3.1）」⇒ 只有核心锚封闭才提供「确定的消费集合」；案 C（open set）与其**D**（10 流作集）均使其退化为逐实例动态集 ⇒ 直接撞该句。

---

## 5. 登记的事实（`ADR-CEP-24` 未显式写的）

⭐ **SSP 声明面（稳定态转移表）在结构上只覆盖 6 份 state_spec 中的 3 份**（MESIF / TileLink / CXL-2.0）。另 3 份（**DCoh 三视图** / **SHARP 聚合** / **UALink INC**）**没有 `Event` 枚举**，不是转移表 ⇒ ⛔ **「表达力 ≥ 手写面」（`ADR-CEP-24` 的 V-24-2）只对这 3 份可判**；对另 3 份**不适用**。

⇒ 本裁决**登记**该事实，并建议在 `CP-I-2` 的验收里把「6/6」改为「**3/6（事件式转移表类）**」，⛔ 不得以「6/6」表述。

⚠️ **格载荷差异（oracle S3 · 补注）**：「3/6」是**体裁**判断（= 有 `State × Event` 转移表）。但**格**的载荷不同：MESIF / CXL 的格 = `uint8_t` 单目标 + `0xFF` 空洞（`mesif_state_spec.hh:132`），而 **TileLink 的格 = `Transition{kind, target_mask, basis, pending, ref, note}`**（`tilelink_state_spec.hh:131-139`，`target_mask` 是 **3 位掩码** 以承载 `Cap` 的「源不定」）⇒ 按 `ADR-CEP-24` 决策 2 已声明的格形态（`transition[state][event]` + `0xFF` 空洞 + 逐格 `basis`），**干净覆盖的只有 2/6（MESIF + CXL）**；**TileLink 需「掩码格」扩展**（属 contract 面 ⇒ 留 `CP-I-2`）。⇒ ⛔ 不得把 §2.2 事实 1 的「形状相同」读成**格级同构**。

---

## 6. 用户 ratify 记录

### 6.1 裁决记录（✅ 2026-11-26 · 用户「批准你的裁决建议」）

| # | 待 ratify 项 | **裁决** | 决定 |
|:--:|---|---|---|
| **R1** | 形态 | ✅ **C′**（封闭核心锚 + 受控私有事件）| 采纳建议 —— ⛔ 不取 A（统一词表 ≈27，DSL 风险）/ C（无锚，撞 `ADR-CEP-24:32`「确定的消费集合」）/ D（10 流即事件集，`Rd`/`RdUnique` 坍缩）|
| **R2** | 核心锚初版清单 | ✅ **14 个**（`ReadShared` `ReadUnique` `Write` `WriteNoAlloc` `Upgrade` `SnoopShared` `SnoopUnique` `SnoopInvalidate` `EvictDirty` `EvictClean` `Cap` `Grow` `Prune` `Report`）| 采纳建议；`CP-I-2` 时按实际协议补（⚠️ CXL 约 8-9 事件走私有事件，见 §4.1）|
| **R3** | 「无等价锚」私有事件的批准路径 | ✅ **须 architect 批准** + **「已批准私有事件先例库」摊销**（已批准者入库，后续 SSP 复用**免批**）| 采纳建议 |
| **R4** | 「3/6 体裁 / 2/6 干净覆盖」的登记 | ✅ **接受**；并向 `ADR-CEP-24` **追加 SU 镜像登记** | 采纳建议 —— ADR SU 随本 ratify 同批落（五步链，见 §6.2）|

### 6.2 ratify 后的落盘动作（本条 + ADR SU **双登记**）

| # | 动作 | 归属 | 状态 |
|:--:|---|---|---|
| 1 | 本裁决记录 → 已 ratify + §6.1 记录 | architect | ✅ 本次 |
| 2 | **`ADR-CEP-24` 追加 SU**（镜像「3/6 体裁 + 2/6 干净覆盖 + 格载荷差异留 `CP-I-2` + 指向本裁决」）| architect（`docs/adr/` · **冻结面五步链**）| ⏳ 同一批 |
| 3 | 台账 Entry（ADR SU 的冻结面授权）| **planner** | ⏳ |
| 4 | 产物 commit + `--update --authorized-by` + 清单单独 commit + 无参复验 | **builder** | ⏳ |
| 5 | objective `CP-Q-1a` → ✅ **闭合** + `CP-I-2` **解除阻塞** | **planner** | ⏳ |

⚠️ **锚集的实际边界（ratify 时已知晓）**：14 锚对 MESIF 6 + TileLink 4 **全覆盖**；对 **CXL-2.0** 约 8-9 事件无干净锚 ⇒ 走私有事件（R3 的摊销机制针对此项）。
⚠️ **Contract 化留 `CP-I-2`**：锚集常量、私有事件 schema、`target_mask` 掩码格扩展（TileLink）、`flow` 映射校验 —— 均属 `CP-I-2` 的 design/AC 范畴，⛔ 本裁决不落 contract。

---

## 7. 证据来源（objective `CP-Q-1a` 的完成判据要求「谁产出、在哪」）

| 证据 | 产出者 | 落点 |
|---|---|---|
| 6 份 state_spec 的建模风格分类 | **本裁决（architect）实测** | §2.1 的 `file:line`（逐份核）|
| 三份事件式 state_spec 的事件数与事件名 | 同上 | `mesif_state_spec.hh:60-67`（6）· `tilelink_state_spec.hh:71-76`（4）+ `:119-120`（`kEventNames`）· `cxl_state_spec.hh:69+`（17）|
| 另 3 份无 `Event` 枚举的证明 | 同上 | `cxl_dcoh_state_spec.hh:72-79` · `nvlink_sharp_state_spec.hh:223-329` · `ualink_inc_state_spec.hh:103-582`（`grep "enum class Event"` = 0 命中）|
| 先例 `kChiOpcodeTable`（**25** 条 opcode → flow）| 同上 | `chi_rnf_protocol_engine.hh:139-167` |
| canonical 10 流清单 | 同上 | `ADR-CEP-02 §2`（`CanonicalFlow` 枚举：AddressResolve / BackInvalidate / Barrier / Completion / ForwardData / Request / Snoop / SnoopCompletionAck / SnoopResp / Writeback）|
| **词表表达力的逐案对比** | 本裁决（architect）**推理 + 实测** | §2.2 事实 2（逐条列举「复用某一份会丢什么」）|

⚠️ **诚实边界**：§2.2 事实 2 是**结构级推理**（由事件名 + 语义注释判断），⛔ **未做逐格实验**（无人实际用 MESIF 的 6 事件去表达 TileLink 的表）。⇒ 若需升级为**实测级**证据，须在 `CP-I-2` 做「用核心锚表达 3 份 state_spec 的逐格 diff」（即 V-24-2 扩展到 TileLink）。

---

## 8. 对 `CP-I-2` 的约束（AC 草案）

| # | 约束 | 判据形态 |
|:--:|---|---|
| 1 | SSP 事件的 `flow` 字段 ∈ canonical 10 流 | 枚举校验 + 反向 fixture |
| 2 | SSP 事件的「核心锚」引用 ∈ **已登记核心锚集**（⛔ 不得引用未登记锚）| 集合校验 + 反向 fixture |
| 3 | 私有事件「无等价锚」⇒ 必须有 **architect 批准记录**（按路径可查）| 存在性校验 |
| 4 | 事件名**逐字断言**（先例 `kEventNames`）| 字面清单 + 毒化 |
| 5 | 核心锚集**封闭**（新增须改登记常量 ⇒ 触发复核）| 规模常量 + 反向存在性闸 |
| 6 | 表达力表述用「**3/6（事件式转移表类）**」，⛔ 不得写「6/6」（「干净覆盖」= **2/6**，见 §5）| 措辞禁令 + 反向 fixture |
| **7** | ⭐ **私有事件原子性**（oracle S1）：⛔ 禁止私有事件**引用/组合**其他私有事件（组合机制 = DSL 的胚胎）| 字面清单 + 反向 fixture |
| **8** | ⭐ **「无等价」占比可观测**（oracle S1）：私有事件中声明「无等价核心锚」的**占比 > 半数** ⇒ 触发**核心锚集复审**（防锚集沦为装饰）| 占比统计 + 阈值触发 + 反向 fixture |

---

## 9. 关联

`ADR-CEP-24` 决策 3.1 / §8 F5（原文锚 `:32`「确定的消费集合」）· objective `obj-cep-custom-protocol` 的 `CP-Q-1a` / `CP-I-2` / **`CP-H1`**（`:245`「对 6 份逐一判定可表达性」—— **§5 即其首付**，⛔ 不重复造分类） · 台账 Entry #551 · `.rddf/roadmap/cep-gen-family-gap-closure.md` §2.2（瞬态态 = 1 的缺口事实）· `docs/research/oswald/paper-claims-index.md`（`T-C04` 转发请求改名 = 事件**区分度**的论文侧依据）
