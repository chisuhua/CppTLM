# 裁决记录 · **I-7b**（档 2 第二族 · `tilelink_c` home 侧状态驱动）

> **状态**：🟢 **形态已裁 · ratify 后可开工**（Q-1 / Q-2 均已裁定；⏳ **待用户 ratify** —— ratify 前 builder **不得开工**）
> **日期**：2026-11-21 · **裁决人**：architect（build agent 会话，`role-switch.sh architect --by user`）
> **对象**：登记册 §6.4 队列 2 的 **I-7b**（I-7 的**第二个子单元**）· 单元 = `.rddf/improvements/cep-tilelink-home-side-state-driver.md`
> **上游**：`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四.4（I-7b 的**形态**：两种载体形态 / 溯源纪律 / `carrier_kind = "tracker-observation"` / 档 2 读法）· 本记录 = 其**实施形态**的补充裁定（⛔ **不重开**已裁内容）
> **前置**：I-7a 已交付（`TreeManager::view_of`，commit `a2a4c7ae`）⇒ 单元 AC-13「前置可机判」**已满足**
> **输入测绘**：`.rddf/reviews/cep-i7b-preflight-2026-11-21.md`（builder 的 5 项技术探测；⚠️ 其 §三 的 10 行动作表是**草案**、⛔ **不是**裁决 —— 见 §七）
> **关联 change**：⏳ 待 builder 立项（`openspec/changes/` 尚无 I-7b 的 change）
> **AC 映射**：本记录的裁定 ↔ I-7b 单元 AC 编号的对应见 §五末（供 builder 在 change 的 proposal 中引用）
> **审查**：oracle **APPROVE（条件式）**（§六）· critic **APPROVE_WITH_CONDITIONS**（mergeable: yes · scope_exceeded: false · **2 条落盘时的条件 BLOCKER** + 5 条 SHOULD_FIX + 2 条 NITS）⇒ 全部落盘闭合，见 §六
> **⛔ 本记录不是 ADR** ⇒ 不适用「ADR 完成的标志」5 条（⛔ **不新建** `docs/adr/` —— 本单元是**实施形态**裁定，不是新的不可逆架构决策）

---

## 一、裁决摘要

| # | 问题 | 裁定 |
|:--:|---|---|
| **Q-1** | home 侧簿记层的「**规约事件 → 树表安装动作**」这一跳，其**语义来源**受什么纪律约束？ | **「逐行 basis 纪律」** —— 键 = **(消息, 载荷)** 组合（⛔ 非裸消息）· 每行带**必填** `basis`（三档，复用 `T3BasisKind`）· 三档各绑定**允许的动作类别** · `Pending` 行 ⇒ **fail-closed**（不装表 + 计入「不可判」）· 纪律由 **C-1 ~ C-5** 五条机械判据锁死（⛔ 不得退化为文档承诺） |
| **Q-2** | 档 2 spec 的 Requirement 主体如何覆盖「引擎侧 + tracker 侧」两种载体形态？ | **主体首句**从「协议级**引擎**」改为「协议级**观测面**（两载体形态）」+ **追加 1 个 tracker 侧 Scenario**（**含族 1 的四条 disciplines**，⛔ 不让族 2「同档不同权」）+ **族 1 的 7 个 Scenario 逐字保留** + **spec Purpose 段不动**（= 动机叙事层；⚠️ 显式裁定，避免半已决） |

---

## 二、Q-1 裁决 · 逐行 basis 纪律

### 2.1 根因订正（⛔ **两条上游表述在此被推翻**，须先记）

| # | 上游表述 | 实测 | 订正 |
|:--:|---|---|---|
| **R-1** | oracle 建议：`Grant`/`GrantData`「按 `toT`/`toB`/`toN` 装表（`TLC-01~03`）」 | `TLC-01~03` 是 **`Cap`** 的锚点（`tilelink_state_spec.hh:172-179`，「**源不定** ⇒ 目标 3 元集合」）；而 **T3 表**（`:747-777`）的 `TLC-M-02` 说 `Grant`/`GrantData` ⇒ **`Grow`**（记号 `NtoB`/`NtoT`/`BtoT`）| ⇒ **`Cap` 行（源不定）不可直接安装**；`Grant` 类按 **`Grow`** 行安装 ⇒ 目标由**源态（树表可查）+ 消息载荷**共同决定 |
| **R-2** | preflight §三 的「G-1：`Grow`/`Prune` 在消息层无逐消息锚点 ⇒ 仓内不可判定」 | **T3 表存在且 11 行全部 `Direct`**（`TLC-M-01~11`，`:747-777`）：Request/`Acquire` ⇒ **0（无事件）** · Completion/`Grant`+`GrantData` ⇒ **`Grow`** · Snoop/`Probe` ⇒ `Prune` · SnoopResp/`ProbeAck`+`ProbeAckData` ⇒ `Prune`（**potentially**）· Writeback/`Release`+`ReleaseData` ⇒ `Prune` | ⇒ **G-1 撤销**；真实缺口 = 「**规约事件 → 树表动作**」这一跳（= oracle 的诊断）|

⇒ ⚠️ **本单元的规约层真源是 3 张表**（驱动**只读**消费，⛔ 不得复制为自有字面量）：
`tilelink_state_spec.hh:171-208`（12 格 State×Event 转移表）· `:747-777`（T3 11 行意图→事件映射）· `:259-265`（5 条消息→通道/方向）+ `tilelink_c_adapter.hh:196-203`（10 条消息→6 类角色）+ `:227-235`（2 处降维登记）。

### 2.2 纪律（**逐行** · 每行 = 一条消息 × 一个载荷组合）

| 要素 | 裁定 | 依据 |
|---|---|---|
| **键** | **(消息, 载荷)** 组合 —— ⛔ **非裸消息**。载荷取值域封闭：目标副本状态 ∈ `TileLinkStateSpec::State`（3 值）· **复用规约载体的别名**（`tilelink_tree_manager.hh:62-63` 已 `using State = TileLinkStateSpec::State`）⇒ ⛔ **不新建第二套词表**；方向 = **单向**（规约 ⇒ 消息字段 ⇒ 规约），⛔ 不反向 | critic SHOULD_FIX 2 ③ + oracle 已排除方案 ④ + **复审 SHOULD_FIX ①** |<br>⚠️ **行身份的消歧（复审 SHOULD_FIX ① 的闭合）**：**行 = 消息**（**恰 10 行**、行序 == 消息枚举值 ⇒ C-1 成立）；**载荷是该行声明的「合法域」内的运行时参数**（⛔ **不按载荷展开行**）⇒ 「**未登记组合**」的精确定义 = **载荷超出该行声明的合法域**（⇒ fail-closed）⇒ 「键 = (消息, 载荷)」与「C-1 行数恒 10」**同时自洽** |
| **basis（必填 · 三档）** | 复用 `T3BasisKind{Direct, SelfDerived, Pending}`（`tilelink_state_spec.hh:735-744` 已有谓词 `t3_basis_ref_kind_consistent`）—— ⛔ **不新造词表**。**basis 字段无默认值** ⇒ 省略即编译错 | critic SHOULD_FIX 2 ② |
| **basis ↔ 动作类别的绑定** | `Direct` ⇒ 锚点已注册为 `RefKind::SpecClause`（如 `TLC-M-02`）· `SelfDerived` ⇒ 锚点为 `RefKind::RepoDerived` **且显式登记**（先例 `:738-739`）· `Pending` ⇒ **动作恒为 NoOp**（⛔ 不装表）| critic C-2 / C-3 |
| **fail-closed** | 未登记的 (消息, 载荷) 组合 ⇒ **不静默 coerce 成最接近合法动作** ⇒ 计入「不可判」（先例 `t3_consumable_event_mask`，`:782-787`：其注释明禁「据此推断该意图无副作用」——那只是 `Direct` + 空掩码才有的含义）| oracle 建议 2 ① + critic C-3 |
| **消费出口纪律** | 驱动 SHALL **只读消费**规约层表（⛔ 不得复制 T3/映射表为自有字面量）—— 否则产生**第二份会漂移的映射表**（先例：runtime spec 的 TMA-01 同型纪律）| critic SHOULD_FIX 1 末 |
| **「不可判」≠「零违例」** | 前提不成立 / 丢弃计数非零 / 消费 `Pending` 行 ⇒ 该 episode **无结论**，⛔ **不计入违例数**；「装错表」= 实现 bug ⇒ 违例计数**精确不符**且红因签名可区分 ⇒ 两通道在 test 侧**分离断言** | oracle 建议 2 ③ + 族 1 先例（`openspec/specs/cep-evidence-tier2-observation/spec.md:40-45`）|

⚠️ **关键取舍（为何不是「只实现有锚点的动作」）**：若只装 `Grant` 侧而不装 `Release`/`ProbeAck` 侧 ⇒ `TreeManager::clear_sharer` 是**唯一**清表入口（`tilelink_tree_manager.hh:106-115`）而驱动不存在 ⇒ `entries_` **单调累积** ⇒ SWMR 判据（`Tip` 获取时 `sharers & ~self ≠ 0`）随流长**无界假阳**，且与已登记的 `coverage_map.tilelink_c.completeness = EXACT`（语义 = 「能回答**没有**其他 sharer」，`test/cep_evidence_tiers.json:89-96`）**无声矛盾**。
⇒ **建模决定式给出 + 逐行标注 basis**，其误差**有界、可审计、可登记**；静默跳过则误差**单调增长且以「数据」面貌出现** ⇒ ⛔ **不接受**纯保守降级（先例：`kMessageClassDowngradeNote` 的 2 处降维是**显式登记**而非静默跳过）。

### 2.3 机械判据（**C-1 ~ C-5** · ⛔ 缺任一条 ⇒ 本纪律退化为文档承诺 ⇒ 判 BLOCKER）

| # | 判据 | **红条件（签名）** | 先例 |
|:--:|---|---|---|
| **C-1** | **行宇宙锁**：安装表**恰 10 行**、行序 == 消息枚举值、消息名集合 == `kProtocolMessageMap` 的 10 词表 | 删 / 增 / 换序任一行 ⇒ 红；驱动收到**表外消息**被静默吞掉 ⇒ 红（须计入 unknown/invalid）| `t3_row_count_is_exact` / `t3_intents_are_unique_and_complete`（`:843-856`）|
| **C-2** | **basis ↔ 锚点一致性**：逐行谓词（`Direct` ⇒ 锚点已注册为 `SpecClause`；`SelfDerived` ⇒ `RepoDerived` **且显式登记**；`Pending` ⇒ NoOp）| 某行标 `Direct` 但锚点 id **未注册** ⇒ 红；某行锚到 **improvement 自身** ⇒ 红（**循环自证**，见 §七）| `t3_basis_ref_kind_consistent`（`:735-744`）|
| **C-3** | **Pending ⇒ fail-closed 锁**：`Pending` 行 ⇒ 动作恒 NoOp **且** 消费出口把该消息计入「不可判」而**非**「零违例」| 毒化测试：把某行改 `Pending`，若该消息**仍产生树变更** ⇒ 红；或该消息被判「零违例」⇒ 红 | `t3_pending_rows_declare_no_event`（`:870-875`）+ `t3_consumable_event_mask`（`:782-787`）|
| **C-4** | **决定性变异**（TLE-04 同型）：影子拷贝改某行 basis / 删某行 ⇒ **按签名红** ⇒ 复原后 hash 一致 | 变异未触发红 ⇒ 判据无鉴别力 ⇒ 红 | 仓内 TLE-04 / I-6a 的 4 条变异范式 |
| **C-5** | **越权消费锁**：驱动头文件**不得**出现规约层映射表的自有字面量副本（只经规约层访问器消费）| 出现第二份映射表字面量 ⇒ 红 | TMA-01 ③ 的静态扫描范式 |

⚠️ **形态订正（critic 指出我给它的前提有偏）**：仓内 basis 一致性的**最强先例不是 static_assert**，而是「**静态行宇宙锁 + 运行时逐行一致性断言**」这对组合（`tilelink_state_spec.hh:905-906` 明写「编译期锁 SHALL 同时有运行时断言」）⇒ C-1 用静态锁，C-2/C-3 用运行时逐行断言。⚠️ 若新头文件**不可 constexpr** 见到 source registry ⇒ **落运行时 REQUIRE 即可**（同 T3 先例），⛔ 不得强上 static_assert。

⚠️ **另加 1 条失真机检（oracle 建议 2 ⑤）· D-1 ~ D-3**：不靠文档承诺，把「装错表」变成可检 ——

| # | 判据 | 红条件 |
|:--:|---|---|
| **D-1**（单调性反向 fixture）| N 组 `Grant` → `Release` 配对 ⇒ 终态 `view_of(addr).sharers == 0` **精确**成立 | 删驱动的 `clear_sharer` 调用 ⇒ 按签名红 |
| **D-2**（方向混淆反 fixture）| 同一「保留 vs 放弃」对，**单差异** ⇒ 终态 sharer 位**精确**在 / 不在 | 违例计数**精确** `== 1` vs `== 0`（⛔ 非 `> 0`）|
| **D-3**（载荷折叠试金石）| 一条「放弃」载荷 ⇒ 清表生效 | 裸消息键实现（无载荷）在此**必红** |

⚠️ **构造流自检（critic 验证建议的实例化红测）**：源 = `Tip` 的节点再收到 `Grant` 类消息 ⇒ 依 `TLC-06`（源=Tip **无** `Grow` 转移 ⇒ 目标掩码**空集**）⇒ 该消息**必须落「不可判」**，⛔ **不得**被装表。

### 2.4 被否方案 · 取舍 · 回滚

| 被否方案 | 排除理由 | — |
|---|---|---|
| **纯保守降级**（不实现无锚点动作 / 一律 invalid 化）| 引入**单调假阳**（§2.2 末）+ 与已登记 `EXACT` 无声矛盾 + 「真实流几乎恒不可判」⇒ **交付物自我掏空**；且与仓内「降维**显式登记**而非静默跳过」文化相悖 | oracle 已排除 ① + critic 已排除 |
| **允许全部按规范常识推导**（无逐行标注）| I-6 裁决 P-5「编值」同型失败 —— 看似可信的无源声明面、闸永远绿、后人无法分辨哪条是真锚点；且仓内合规纪律是「规范 © SiFive ⇒ 只记条款级定位 + 断言摘要」⇒ 无锚即 `SelfDerived`/`Pending` | oracle 已排除 ② |
| **族 2 复用族 1 的 `EventObservation`** | 禁改族 1 基底面（`protocol_engine_base.hh:169-199`，字段表定死）· tracker 侧**无迁移/数据语义** ⇒ 硬套产生**假字段**（假绿温床）· 两形态区分正是 §四.4 的载体分型本意 | oracle 已排除 ③ |
| **新建独立权限枚举** | 违反 `perm` 4 值锁 + 第二词表 = 双真源（`TreeManager` 已别名规约载体） | oracle 已排除 ④ |
| **簿记层放进引擎** | TLE-05 逐字禁（裁决记录 §四.1 已引两处原文）· 「源不定」是规范事实 ⇒ 推翻 = 重开已签批契约 | oracle 已排除 ⑤ |

**取舍一句话**：**要覆盖度 + 要可审计 ⇒ 建模决定式给出 + 逐行 basis 标注 + 五条机械锁**；⚠️ 不用「宁可少覆盖也不推断」换安全 —— 那会引入**更危险**的单调失真。

**回滚路径**：删驱动文件 + 回滚 `test/cep_evidence_tiers.json` 条目 ⇒ **无 ABI 面 / 无 ADR 面 / 无冻结面残留**（⛔ 契约面若已改 ⇒ 走五步链**单向**，须另起 Entry 反向合并）。

---

## 三、Q-2 裁决 · 契约面措辞

### 3.1 Requirement 主体首句（**逐字落 delta** · `MODIFIED` 只改这一句 + 追加 1 Scenario）

> CEP 的**协议级观测面** SHALL 以 **per-event** 粒度暴露原始观测事实，观测面 SHALL 由**两种载体形态**承载 —— ① **引擎侧**：协议级引擎在状态迁移与行数据装入落点暴露观测（地址 / 事件 / 迁移前后态 / 数据前后值 / sharer 快照 / 归因）;② **tracker 侧**：home 侧消息簿记层在**驱动边界**暴露观测（地址 / 消息 / 安装动作 / 安装前后副本分布 / sharer 快照 / 归因）—— 两种形态 SHALL **各自满足**本 Requirement 的判据口径、归因边界与不可判语义;**族 1 的 Scenario 实例化「引擎侧」载体形态;本 Requirement 的观测面 = 两形态之并**。

⚠️ **主体只列纪律级字段**（两形态的 carrier 形态 / `run_produced=false` / `has_verdict=false` / 默认关 / 有界 / invalid 记账）—— ⛔ **不列**族 2 的协议语义字段表（否则等于替 home 侧**仿真模型**承诺规范一致，违反单元 Out-Scope）。

### 3.2 新增 Scenario（名含 `tracker` ⇒ 满足单元 AC-7 ②）

> #### Scenario: tracker 侧载体 SHALL 在驱动边界产生 per-installation 观测
>
> - **WHEN** home 侧消息簿记层将一条 TileLink-C 消息翻译为树表安装动作（`set_sharer` / `clear_sharer`）
> - **THEN** 每次安装 SHALL 在**驱动边界**产生**恰好 1 条**观测记录，含消息 id / 地址 / 节点 / 安装动作 / **安装前**副本分布快照 / 归因来源（⛔ 记录 SHALL **NOT** 含**安装后**副本分布快照 —— 观测点在安装**前**发出 ⇒ 记录内只有前值;⛔ 记录 SHALL **NOT** 含迁移前后态 / 数据前后值 —— 簿记层无引擎态迁移）
> - **AND** 快照 SHALL 取自 `TreeManager::view_of` 的 const 纯查询（⛔ SHALL **NOT** 用有副作用的 `lookup`;⛔ SHALL **NOT** 把观测点放进 `TreeManager` 内部 —— 观测它自己维持的不变量 ⇒ 同义反复 ⇒ 必绿）
> - **AND** 快照时点 SHALL 为**安装前**视图
> - **AND** 观测面 SHALL **默认关闭**且**有界**，丢弃计数非零 ⇒ 该次观测**不可判**（⛔ SHALL **NOT** 读作零违例）
> - **AND** 装配前提不成立（node 未映射 / 多 Tip / 观测面关闭）SHALL 递增**观测面失效计数**并判**不可判**（⛔ 不可判 SHALL **NOT** 计为零违例）
> - **AND** **未登记**的（消息, 载荷）组合 SHALL **fail-closed**（不装表 ⇒ 计入不可判），⛔ SHALL **NOT** 被静默解释为最接近的合法动作
> - **AND** 每条观测的 sharer 位 SHALL 可溯源到输入流消息 id;**绕过驱动**直接安装产生的位 SHALL 判 `provenance_broken` 且 SHALL **NOT** 出现在任何观测快照中

⚠️ **上段最后 4 条 = 族 1 的四条 disciplines 在族 2 的对齐**（默认关闭且有界 / invalid ≠ 零违例 / fail-closed / 装配前提不成立）⇒ ⛔ **不让族 2「同档不同权」**（critic 条件 BLOCKER ② 的闭合点）。

### 3.3 spec `## Purpose` 段 —— **显式裁定：不动**

`openspec/specs/cep-evidence-tier2-observation/spec.md:12,16` 的「协议引擎暴露 per-event 原始观测事实」与 canonical 实现行**保持不动**。
**理由**：`Purpose` = **动机叙事层**，`Requirement` = **规范层** ⇒ 只改规范层 ⇒ **最小冻结面 diff**（已实测 **+8/−1**）· 族 1 **零回归**。
⚠️ **若后续要连 `Purpose` 一起改** ⇒ 须**重新实测 diff 并重走**本记录的探测（builder 的 D-1）⇒ ⛔ 本记录**不授权**该扩展。

### 3.4 被否方案 · 取舍 · 回滚

| 被否方案 | 排除理由 |
|---|---|
| **改写族 1 的 7 个 Scenario 的 WHEN 子句**（把「协议引擎」改成「观测面」）| 与改写后的主体**不矛盾**（族 1 的 Scenario 是「引擎侧」形态的**实例化**）· 且会**扩大冻结面 churn**、无收益 ⇒ **逐字保留**（critic 已核实 D-1 的 +8/−1）|
| **只加 1 行 JSON 字段就宣称完成** | 单元 AC-7 明禁（「只加一行 JSON 字段**不构成**完成」）· 契约面必须真改（主体 + Scenario）|
| **主体里列全族 2 的协议语义字段表** | 等于替 home 侧仿真模型**承诺规范一致**（违反 Out-Scope）⇒ 只列纪律级字段（§3.1）|

**取舍一句话**：**改规范层、不改叙事层；加 1 个 Scenario 承载族 2 形态且对齐族 1 的四条 disciplines；族 1 的 7 个 Scenario 逐字不动。**

**回滚路径**：冻结面**单向**（五步链）⇒ 若需回退 ⇒ **另起台账 Entry** 做反向合并（⛔ 不得手改 main spec）。

---

## 四、诚实边界（⛔ **不得**说 · 实施侧必登记）

| # | 禁述 |
|:--:|---|
| ① | 不得宣称 TileLink-C **规范一致性 / 规范忠实**（home 侧是**仿真模型**，保真度不在判据面）|
| ② | 不得把 `SelfDerived` 行表述为「规范锚定 / 规范明确」 |
| ③ | 不得宣称两族「同强度 / 全族达成 / 所有族 / 检测力已证 / 档 2 强于档 1 / 通过态」 |
| ④ | 不得宣称 tracker 侧覆盖 **data-value 维**（`Trunk` 折叠 ⇒ `dirty` 掩码丢失 ⇒ 连负控形态都没有）|
| ⑤ | 不得宣称覆盖**跨子树** sharer（`ESCALATE_UP` 之外的持有者恒不可见 ⇒ 「**本子树内**」限定必现）|
| ⑥ | 不得称 tracker 侧记录为「引擎状态迁移观测」（`event`/`from`/`to` 字段不适用）|
| ⑦ | 不得把「观测面存在 / 判据可机判」表述为「协议语义已被检验」（同 I-6 的「声明 ≠ 判定」）|
| ⑧ | `can_claim` 不得含「协议语义全覆盖」「跨子树可见性」 |
| ⑨ | 不得把「写 episode 完整」表述为「协议完整性」（`entries_` 无容量上限是已登记未定义面）|

---

## 五、交办（builder · 阶段序列）

| 步 | 内容 | 判据形态 |
|:--:|---|---|
| **S1** | 起 openspec change（proposal / design / tasks + **delta spec** = §3.1 主体 + §3.2 Scenario）· 台账 Entry（授权冻结面变更）| `openspec validate <change> --strict` rc=0；`openspec archive` 后 main spec diff **+8/−1**（**重新实测**，⛔ 不假设）|
| **S2** | **安装语义表**（10 行 × (消息, 载荷) × basis × 动作）+ 驱动头文件（只读消费规约层）| C-1 / C-2 / C-3 / C-5 全绿 |
| **S3** | 驱动边界观测载体（tracker 侧 · **不复用** `EventObservation`）+ 观测点（`view_of` 安装前快照）| C-4 变异按签名红；同一流重跑逐字相等 |
| **S4** | 判据载体（test 侧判定器 + 正控 / 负控 / 反 fixture + 4 条装配期硬断言 + provenance 反 fixture + 有界性）| D-1 / D-2 / D-3 全绿；**精确** `== 0` / `== 1`（⛔ 非 `> 0`）；「源=Tip 收 Grant ⇒ 不可判」红测绿 |
| **S5** | 登记面：`test/cep_evidence_tiers.json` 新增 `tilelink_c` 档 2 条目（`carrier_kind = "tracker-observation"`）+ `coverage_map.families.tilelink_c.reason` 追加「**本子树内**」限定；reconcile 闸加 **T8**（两表双向闭合）+ **T9**（`carrier_kind` 封闭词表）+ 变异基座 +2 条 | T8/T9 全绿 + 3 条决定性变异按签名红 |
| **S6** | **冻结面五步链**（台账 Entry → 改产物 → commit A → `--update` → 清单单独 commit → 无参复验）· `docs/architecture/07-coherence-exploration-platform.md` 追加 SU（append-only）· `reconcile` 的 `DOC_UNITS` 追加新 SU（T6 变异定位随之更新）| `cep_freeze_check` 无参 rc=0；`artifact_count` 不变；**只 1 个 hash 变**（`cep-evidence-tier2-observation/spec.md`）|
| **S7** | 验收报告 + 载体表（**父编号 `I-7`** ⇒ I-7a 的 8 行 + I-7b 的 N 行**全 done** 才算 I-7 完成）+ 登记册回填 + 归档（force-add + 基线 +1）| 队列 2 下一项仍 = **I-7**（I-7b 完成前）|

⛔ **不要做**：动引擎（`tilelink_protocol_engine.hh` 零 diff）· 改族 1 基底面（`protocol_engine_base.hh` / `sharer_tracker.hh`）· 改 `tilelink_state_spec.hh`（规约层真源 ⇒ **零 diff 硬查**）· 复用 `EventObservation` · 新建权限枚举 · 抄 T3/映射表为自有字面量 · 新建 ADR · 写死 KPI 计数 · 提前扩档 2 之外的族。

**AC 映射（本记录裁定 ↔ I-7b 单元 AC 编号）**：Q-1 纪律 ⇒ 单元 **AC-1/2/3/4/5/6/10**（机制面 / 内容面 / 溯源 / 术语拆分 / 判定行钉死 / 装配期硬断言 / 诚实边界）· Q-2 措辞 ⇒ **AC-7**（规格主体变更）+ **AC-9**（文档读法 SU）· 机械判据 C-1~C-5 + D-1~D-3 ⇒ **AC-2**（决定性变异）+ **AC-3**（反向 fixture）· 载体表与登记面 ⇒ **AC-1** + **AC-13**（单元口径）· 冻结面 ⇒ **AC-8** · 零越权 ⇒ **AC-11** · 前置门 ⇒ **AC-13**（已满足）· 单子树限定 ⇒ **AC-14**。

---

## 六、审查轨迹（**2 条条件 BLOCKER 的闭合说明**）

| 轮 | 角色 | 判定 | 处置 |
|:--:|---|---|---|
| 1 | **oracle**（先行审查）| **APPROVE（条件式）**：条件 = Q-1 采用「逐行 basis 纪律」+ Q-2 采用其给定文本 | ✅ 采纳其**纪律形态**与 Q-2 文本；⚠️ 其 `Grant`⇒`Cap` 的**细节被 §2.1 R-1 订正**（依据 T3 表 `TLC-M-02`）；⚠️ 其「G-1 不可判定」的表述被 §2.1 R-2 撤销 |
| 1 | **critic**（第二审）| **APPROVE_WITH_CONDITIONS**（mergeable: yes · scope_exceeded: false）· **2 条落盘时的条件 BLOCKER** + 5 条 SHOULD_FIX + 2 条 NITS |
| 2 | **critic**（**复审** · 只验闭合）| **APPROVE_WITH_CONDITIONS** —— **2 条条件 BLOCKER 均闭合**（🔴① C-1~C-5 + D-1~D-3 可机检 · 🔴② disciplines 四条对齐族 1）· **无新 BLOCKER**（mergeable: yes · scope_exceeded: false）· 2 条 SHOULD_FIX（行身份二义 / 快照时点措辞张力）+ 1 条 NITS | ✅ **SHOULD_FIX ①**（行身份二义）⇒ §2.2「键」行加消歧（**行 = 消息 · 恰 10 行** + 载荷作该行合法域内的运行时参数 ⇒ 「未登记组合」= 载荷超域）；✅ **SHOULD_FIX ②**（快照时点措辞）⇒ §3.2 THEN 改为「**安装前**副本分布快照」+ 显式禁「安装后」快照（⛔ 不让措辞张力进冻结面）；✅ **NITS ①** ⇒ 记录头改「ratify 后可开工」；✅ NITS ②（过程日志可迁台账）⇒ 记录内保留（可接受）| ✅ **条件 BLOCKER ①**（Q-1 必须含机械判据）⇒ §2.3 **C-1 ~ C-5** + **D-1 ~ D-3**；✅ **条件 BLOCKER ②**（Q-2 须补 disciplines）⇒ §3.2 末 4 条；✅ SHOULD_FIX 1（TLE-05 作用域核实 + 防搬移守卫 + 只读消费纪律）⇒ §2.2 消费出口纪律；✅ SHOULD_FIX 2（basis 必填 + 行宇宙锁 + C-1~C-5）⇒ §2.2 / §2.3；✅ SHOULD_FIX 3（族 1 Scenario 逐字保留 + 主体只列纪律级字段）⇒ §3.1 / §3.4；✅ SHOULD_FIX 4（**Purpose 段显式裁定**）⇒ §3.3；✅ SHOULD_FIX 5（记录形式要求：file:line / 被否方案 / 取舍 / 回滚 / 订正项登记 / **不抄 10 行全文**）⇒ §2.1 / §2.4 / §3.4 / §七；✅ NITS 1（`tilelink_state_spec.hh` 入零 diff 硬查）⇒ §五；✅ NITS 2（不可 constexpr ⇒ 运行时 REQUIRE）⇒ §2.3 末 |

⚠️ **critic 的条件 BLOCKER 是「落盘时的」** ⇒ 本记录的落盘文本已按其要求闭合 ⇒ **复审只验闭合**（§审查配对：BLOCKER → 允许 1 次复审，只提交闭合证据 + 本轮增量）。

### 6.1 TLE-05 冲突面 —— **不成立**（critic 实测结论，采纳）

`test/test_cep_tilelink_engine.cc:84-86` 的 `engine_source_text()` **只读 `tilelink_protocol_engine.hh` 单文件**；其 lint（`:377-384`）禁的是**该文件内**的 `TileLinkStateSpec::State` / `Nothing`/`Branch`/`Tip` / `transition` token。
⇒ 驱动引用规约层表 **不被** TLE-05 抓到（前提 = **引擎文件零 diff**（单元 AC-11 已含）+ 驱动代码物理上不进引擎文件）；且该 lint 顺带充当「防搬移守卫」⇒ 裁决写明：**TLE-05 的单文件作用域是既成事实**，驱动引用规约层表 = 「运行面只读」的合法消费（与 runtime spec `Purpose:14`「运行面只读」同向，⛔ 不构成冲突）。

---

## 七、被订正项登记（⛔ 上游表述在此显式被推翻/订正）

| # | 出处 | 原表述 | 订正 | 依据 |
|:--:|---|---|---|---|
| **1** | oracle 建议 2 ① | `Grant`/`GrantData`「按 `toT`/`toB`/`toN` 装表（`TLC-01~03`）」| ⇒ **`Cap` 行（源不定）不可直接安装**；`Grant` 类按 **`Grow`** 行安装（目标 = 源态 + 消息载荷）| T3 表 `TLC-M-02`（`:752-753`）+ 12 格表 `TLC-06`（`:187-188`，源=Tip 无 Grow 转移 ⇒ 目标空集）|
| **2** | preflight §三 G-1 | 「`Grow`/`Prune` 在消息层无逐消息锚点 ⇒ 仓内不可判定」| ⇒ **撤销**：T3 表 11 行全部 `Direct`（`TLC-M-01~11`）⇒ 消息→事件层**有锚点** | `tilelink_state_spec.hh:747-777` |
| **3** | preflight §三 的 10 行动作表 | 「`ProbeAck` = 保留降级副本」等逐行动作 | ⇒ **不照单接受**：该行锚「improvement What Changes #1 明文」= **循环自证** ⇒ 该类行只能是 `SelfDerived` 或 `Pending`；且**记录不抄 10 行全文**（否则 adapter / improvement / 记录三份真源）⇒ 行内容归 change 的 `design.md`，由 C-1~C-5 锁死 | critic SHOULD_FIX 5 ④ + C-2 |
| **4** | architect 给 critic 的输入 | 「`:735-744` 的 basis 一致性是**编译期锁**先例」| ⇒ 实为**运行时逐格 REQUIRE**（`test_cep_source_registry.cc:58,117`）⇒ 最强先例 = 「**静态行宇宙锁 + 运行时逐行一致性断言**」组合（`:905-906` 明写「编译期锁 SHALL 同时有运行时断言」）⇒ §2.3 末已按此订正 | critic 指出 |

---

## 八、遗留与交办

| # | 对象 | 内容 |
|:--:|---|---|
| **H-1** | **用户** | ratify Q-1（逐行 basis 纪律）与 Q-2（主体 + Scenario + disciplines + Purpose 不动）|
| **H-2** | **builder** | 按 §五 的 S1 ~ S7 实施；⚠️ S1 须**重新实测** main spec diff（⛔ 不假设 +8/−1）|
| **H-3** | **builder** | 载体表 `I-7` 追加 I-7b 的行（**父编号** ⇒ I-7a 8 行 + I-7b N 行全 done 才算 I-7 完成）|
| **H-4** | **builder** | 台账 Entry 落 `.rddf/roadmap/objectives/obj-cep-platform.md`（编号 = 落库时实测最大值 + 1）· 冻结面授权 |
| **H-5** | **builder** | 登记册 §6.4 的 **I-7** 行回填（I-7a ✅ + I-7b 落地）|
