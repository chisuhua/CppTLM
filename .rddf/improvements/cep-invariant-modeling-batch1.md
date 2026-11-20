---
name: cep-invariant-modeling-batch1
priority: P1
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-05
  - ADR-CEP-08
  - ADR-CEP-09
  - ADR-CEP-04
状态: "🟡 **已立项**(2026-11-20 · planner 起草 · 经 metis 1 轮 + oracle 1 轮 + critic 3 轮审查(critic 第 3 轮 APPROVE_WITH_CONDITIONS)· 关联登记册 §6.3 归属表 I-5 · 前置 = Q-4 architect 判据裁决)"
created: 2026-11-20
---

# cep-invariant-modeling-batch1: 3 条守门 invariant 的建模（批 1）

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文** · ⛔ **新增 invariant 数 = 0**（本单元把 **已定义** 的 3 条 `⭭-unmodelled` 变成**有判定级载体** ⇒ 定义数仍 **66**，编号位仍 **67**）· ⛔ **不缩 66 的分母** · ⛔ **`INV-CEP-04-IC4` 永久跳空**（编号位 67，⛔ 新增 carrier 不得占用）· ⛔ **判据语义由 Q-4 architect 裁决给定，builder 只实现**（⛔ 不得自行发明 invariant 语义 = 越权）· ⛔ **不改 `test/cep_invariant_deferrals.json` 的手写性质**（⛔ 只能**移除**已建模条目的登记，⛔ 不得新增条目）· ⛔ **不改 `include/tlm/cep/sharer_tracker.hh`** · ⛔ **不改 `B3b-2` 计数**。

## 架构依据

`test/cep_invariant_deferrals.json` 登记了 **5** 条 `⭭-unmodelled`（⚠️ 登记册 §1.1 B-1a 写「**3 条**」⇒ **stale**（§1 是 2026-11-16 历史快照）⇒ 本单元以 JSON 实测的 5 条为准；📌 **现行口径见登记册 §6.2 S1-1**（5 条：3 条批 1 + 2 条批 2）—— per critic NIT）：

| id | 缺口 | 触发条件（`trigger_condition` 原文） |
|---|---|---|
| `CEN-Ib8` | 「address_resolve『在 ProtocolEngine 内终结』需 engine 的 egress 可观测面；当前无该面 ⇒ 无可断言对象」 | 「engine 侧 egress 可观测面落地（per `ADR-CEP-05 §6` 折叠表）」 |
| `INV-CEP-08-IC1` | 「『恰在一个 Ordering Point 序列化』= J3 的通用形式；checker 头明写『通用形式不可静态判定』⇒ 当前仅单点 vacuous 判据」 | 「J3 通用形式获得可判定载体（per `ADR-CEP-08` 决策 3）」 |
| `INV-CEP-09-IC2` | 「仲裁器未建模（`ADR-CEP-09` 自记『vc3 内 E 不被 D 饿死』未建模；多 VC 仲裁属 Stage 4+）⇒ 无可断言对象」 | 「多 VC 仲裁器落地（Stage 4+,per `ADR-CEP-09`）」 |

**本单元 = 批 1（前 3 条）**；批 2（`INV-CEP-13-IC1` / `INV-CEP-13-IC3`）是**跨 change 规格修订**（schema 13→14 + `ADR-CEP-12` 联动 + litmus change）⇒ 另立 `cep-invariant-modeling-batch2`。

**闸的判据面**（`scripts/test/cep_invariant_map_check.py`）：① ⭭ id ⊆ 66 集合 ② `reason`/`trigger_condition`/`ledger_entry` 非空 ③ **有牙载体 id ∪ 授权⭕ id == 66 集合**且交集为空 ④ `ledger_entry` 须解析到**真实存在的台账 Entry**且该 Entry 正文含该 id ⑤ 每个载体标记的**邻域**（±8 行）须含断言宏（C/C++）或非零退出式（shell）⇒ 「恒真占位」判红。
⇒ 建模 = 为该 id 提供一个**带 `// INV-CARRIER: <id>` 刚性标记 + 同址断言**的载体 ⇒ 映射表 `test/cep_invariant_assertion_map.json` 由脚本**重生成**（⛔ **不得手改**）。

**为什么批 1 可独立于批 2**：批 1 三条的触发条件都是「**载体面**落地」，不涉及 schema / ADR 修订；批 2 的两条明确要求「schema 13→14 + `INV-CEP-12-IC1` 联动修订 + `ADR-CEP-12` 追加式修订」⇒ **两条路径的审批链不同** ⇒ 混在一批会把「可做的」拖进「需 user ratify 的」。

## 范围

- **In-Scope**：
  - 为 `CEN-Ib8` / `INV-CEP-08-IC1` / `INV-CEP-09-IC2` 各提供 **≥1 个带断言的判定级载体**
  - `test/cep_invariant_deferrals.json` —— **移除**这 3 条登记（⛔ 不得改动其余 2 条的 `reason`/`trigger_condition`/`ledger_entry`）
  - `test/cep_invariant_assertion_map.json` —— 由脚本**重生成**（⛔ 不得手改）
  - 台账追加一条 Entry（⛔ append-only，不改历史 Entry 原文）
- **Out-Scope**：
  - ⛔ `INV-CEP-13-IC1` / `INV-CEP-13-IC3`（⇒ 批 2）
  - ⛔ 任何已签批 ADR 的正文 / 新增 invariant 定义
  - ⛔ `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` 的既有虚函数签名
  - ⛔ 跨 change 规格修订（schema / `ADR-CEP-12` 联动）⇒ 批 2
  - ⛔ `B3b-2` 计数 / 档 2 扩面（⇒ `cep-tier2-second-family-tilelink`）

## Why

**不做的代价**：`66` 条 invariant 的「有判定级载体」比例被这 3 条**结构性锁死**（当前 61 有牙 + 5 授权⭕）⇒ 平台**无法**声称「66 条全部有机器载体」⇒ 这是 §6.2 唯一被 oracle / critic 双方都认定为「真功能缺口」的 S1 级项之一。

**为什么 `CEN-Ib8` 优先**：`CEN-Ib8` 的触发条件「engine 侧 egress 可观测面落地」**同时**是档 2 扩面（`cep-tier2-second-family-tilelink`）的硬前置 ⇒ 它的载体面一旦落地，**两件事同时解锁** ⇒ 投入产出比最高。

**为什么必须 architect 先给判据**：登记册 §1.1 B-1a 已写明「⚠️ **判据须 architect 先定义**（否则 builder 自行发明 invariant 语义 = 越权）」⇒ 本单元**不预判** `INV-CEP-08-IC1`（J3 通用形式）该怎么表达、`INV-CEP-09-IC2`（多 VC 仲裁器）该怎么建模 ⇒ 那些是**架构取舍**（例如：多 VC 仲裁器是否要引入一个可运行组件？），不是实现细节。

## What Changes

| # | 变更 | 形态 / 约束（**具体判据由 Q-4 裁决给定**）|
|:--:|---|---|
| 1 | **`CEN-Ib8` 载体** | 「address_resolve 在 ProtocolEngine 内终结」需 **engine egress 可观测面** ⇒ 载体须能观测「一次 address_resolve 的终结点确实在 engine 内」；⛔ 判据形态由 Q-4 定 |
| 2 | **`INV-CEP-08-IC1` 载体** | 「恰在一个 Ordering Point 序列化」= J3 **通用形式** ⇒ 需一个**可静态判定**的载体；⚠️ 既有 checker 明写「通用形式不可静态判定」⇒ 本条要么**给出可判定形式**（由 Q-4 裁），要么**降级为「单点非 vacuous 判据」**并登记偏离 |
| 3 | **`INV-CEP-09-IC2` 载体** | 「多 VC 仲裁器」⇒ 需仲裁器可断言对象；⚠️ `ADR-CEP-09` 自记「vc3 内 E 不被 D 饿死」未建模 ⇒ 本条要么**引入最小仲裁模型**（Stage 4+ 范围，⛔ 须 Q-4 裁是否属本仓范围），要么**显式登记为「超出本仓范围」并给出 Stage 4+ 的移交口径** |
| 4 | **deferrals 移除 3 条** | `test/cep_invariant_deferrals.json` —— 移除这 3 条；⚠️ **其余 2 条（`INV-CEP-13-IC1` / `INV-CEP-13-IC3`）逐字不动** |
| 5 | **映射表重生成** | `test/cep_invariant_assertion_map.json` 由 `cep_invariant_map_check.py` **生成**；⛔ **不得手改** |
| 6 | **台账 Entry** | 追加一条：列出 3 条 id + 各自载体文件 + 锚点 + 判据来源（Q-4 裁决指针）⇒ 满足闸判据 ④「`ledger_entry` 须解析到真实台账 Entry 且正文含该 id」 |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因；⛔ 载体须满足闸判据 ⑤（邻域 ±8 行含断言宏）⇒ 「恒真占位」必红。

- [ ] **AC-1（3 条**处置**落地 —— ⛔ 不是「3 条建模落地」）**：`python3 scripts/test/cep_invariant_map_check.py` rc=0 且输出「`⭭-unmodelled` 数由 **5 降为 2**」；`test/cep_invariant_deferrals.json` 的 `deferrals` 长度 == **2** 且其 2 个 id == `INV-CEP-13-IC1` / `INV-CEP-13-IC3`。<br>⚠️ **处置 ≠ 建模**（per metis B4）：3 条的终态**不同质** —— `CEN-Ib8` = **真建模** · `INV-CEP-08-IC1` = **部分覆盖**（AC-10：偏离落点在注释 + 台账 ⇒ **不构成「完全达成」**）· `INV-CEP-09-IC2` = **移出 ⭕ 登记但语义未达成**（AC-12：⭕ 登记已移除、grep 断言恒成立 ⇒ **不是恒真占位**，但 **⛔ 不得表述为「已建模」**）⇒ ⛔ **三者都不得在台账/文档里写成「已建模」**；⚠️ 判据 ③ 要求「载体 ∩ ⭕ = ∅」⇒ 移出登记与加载体**不可并存**，这是**闸的硬约束**而非口径选择 |
- [ ] **AC-2（映射表是生成物）**：`git diff` 对 `test/cep_invariant_assertion_map.json` 的改动**等于**脚本重生成后的输出（判据 = 重跑脚本后 `git diff --exit-code` 该文件为空）⇒ 证明非手改
- [ ] **AC-3（载体有牙 · 反 fixture）**：对 3 个新载体**各**做 1 条决定性变异（破坏断言 ⇒ 按签名红 ⇒ 还原 ⇒ 绿）；⛔ 变异须命中闸判据 ⑤ 的邻域断言，⛔ 不得用「删掉整个载体」冒充（那会命中判据 ③ 而非 ⑤）
- [ ] **AC-4（台账可解析）**：闸判据 ④ 绿 ⇒ 新增 Entry **真实存在** 且其正文**含** 3 条 id；⚠️ 反 fixture：把 Entry 正文里的 id 改掉 ⇒ 判红 ⇒ 还原绿
- [ ] **AC-5（零新增 invariant · 零跳空占用）**：`grep -c "INV-CEP-"` 新增载体中的 id 数 == **3**（恰为建模的 3 条）；66 集合成员数仍 **66**；`INV-CEP-04-IC4` **仍不在**任何载体标记中；⚠️ 编号位仍 **67**
- [ ] **AC-6（其余 deferrals 逐字不动）**：`git diff` 对 `test/cep_invariant_deferrals.json` 的改动**只含 3 条删除**（⛔ 无任何字段修改行）⇒ 判据 = diff 中非删除行数 == **0**
- [ ] **AC-7（范围纪律 · 零越权）**：`git diff --name-only` **不含** `docs/adr/` · `docs/architecture/` · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh`；⚠️ 若 Q-4 裁「`INV-CEP-09-IC2` 引入最小仲裁模型」⇒ 该文件**允许**改动，但须在 Q-4 裁决中**逐文件列白名单**
- [ ] **AC-8（0 回归）**：`ctest` 全绿（⛔ **不写死数字**，以 `ctest -N` 实测为准）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**
- [ ] **AC-9（裁决吸收 · Q-5 · 13 键仓内 golden）**：新建 `test/cep_instance_golden.json`，结构 = 实例 id → 13 键 → 权威取值（**从 `include/tlm/cep/protocol_instances.json` 的已声明值派生**，⛔ **不得**另造值）；⚠️ **双源禁令** = golden 是**派生副本** ⇒ 闸须**双向比对**（沿用 `cep_decl_alignment_check.sh` 范式）⇒ **单改一侧即红**；判据 = ① golden 内每键值 == `protocol_instances.json` 同键值（正向）② 改 golden 一侧 ⇒ 闸红（反向）；⚠️ **canonical = 仓内 golden**（per architect 裁决 §2.5：二者**不在** `repo_owned_instance_ids` ⇒ 权威在本仓 ⇒ golden 是**唯一可对账锚**，⛔ 不是第二真源）
- [ ] **AC-10（裁决吸收 · M-1 · `INV-CEP-08-IC1` 偏离落点）**：⛔ **不改 `scripts/test/cep_invariant_map_check.py`**（⛔ **不引入 `⭭-partial` 第三态** —— disposition 词表只有 `⭭-unmodelled` / `⭭-pending` 两态，per `:135`）；⇒ 「集合外不被覆盖」这个偏离**不进 `test/cep_invariant_deferrals.json`**（加载体后必须移除登记，per 判据 ③「载体 ∩ ⭕ = ∅」，实现见 `cep_invariant_map_check.py:152-154`）⇒ **偏离落点 = IC1 载体断言的注释 + 台账 Entry 正文字面量**；判据 = 台账 Entry 正文含「集合外不被覆盖」+ **枚举集合的完整清单**（`grep -F` 命中）；⚠️ **该偏离使本条不构成「完全达成」** ⇒ ⛔ **不得**表述为「J3 通用形式已覆盖」
- [ ] **AC-11（裁决吸收 · C-1 · `CEN-Ib8` 不新增字段）**：⛔ **不向 `include/tlm/cep/protocol_engine_base.hh` 的 `EventObservation` 添加任何字段**（字段表已定死 per 裁决 R5：per `protocol_engine_base.hh:166-187`）⇒ 表达路径 = **既有 `Kind` / `Origin` 枚举取值**（`Origin` 共 **6** 值：`LocalRequest` / `CompletionFill` / `RemoteSnoop` / **`Writeback`** / **`BackInvalidate`** / `DirectInject`，per `:177-184`；⚠️ `DirectInject` ⛔ **不计入**任何生产流判据，per R5 §c）⇒ 一次 `address_resolve` 若在 engine 内终结 ⇒ 事件以 **`Kind = Transition`** + **`Origin = LocalRequest`** 被记录 ⇒ 判据 = 「每一次 `address_resolve` 都有记录 ∧ 其 `Origin` ∈ `{LocalRequest, CompletionFill}`」；负控 = 构造**不经 engine** 的 `address_resolve` ⇒ 该事务**在观测面无对应记录** ⇒ 判红（违例数**精确 == 1**）；⚠️ 表达路径已写进判据文件（裁决记录 §8.2 C-1 + §9.2 SF-1）⇒ ⛔ builder 不得自行 invention
- [ ] **AC-12（裁决吸收 · C-2 · `INV-CEP-09-IC2` 的 grep 模式写死）**：判定模式 = `grep -rn "多 VC 仲裁\|vc_arbitrat\|VcArbitrat" include/ src/ test/` 命中数 == **0**；⚠️ **⛔ 不得用 `Arbiter` 作模式词** —— 仓内 `ArbiterTLM2` / `ArbiterTLM4` 已占用该词（`include/chstream_register.hh:118-119`）⇒ 用之即**永恒误红**；**反 fixture** = 影子副本中新增一个含 `vc_arbitrat` 的桩实现 ⇒ 该断言**必须转红** ⇒ 证明「一旦出现即强制重评」**不是恒真占位**；⚠️ 本条**不算达成**（`⭭-unmodelled` 仍登记）⇒ ⛔ 不得表述为「已建模」

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无新增 invariant） | — | ⛔ **新增 invariant 数 = 0** —— 本单元**不定义**新 invariant，只为**已定义**的 3 条提供判定级载体 ⇒ **不产 delta spec**、**不合并 main spec** ⇒ 归档时**零契约面变化** |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | 3 个载体文件（路径由 Q-4 裁决给定；若 Q-4 指向既有文件则不新增）· 台账 Entry（追加） |
| **修改文件** | `test/cep_invariant_deferrals.json`（**只删 3 条**）· `test/cep_invariant_assertion_map.json`（脚本重生成） |
| **零改动（硬查）** | `docs/adr/**` · `docs/architecture/**` · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `openspec/specs/**` · `experimental/**` · `tools/**` · `scripts/**` |
| **冻结面** | **零**（载体若落在 `include/tlm/cep/**` 的 `.hh` ⇒ ⛔ **不在** `FROZEN_GLOBS`（该 glob 只含 `protocol_instance.schema.json` / `fabric.schema.json` / `fabric.json`）⇒ 不触五步链；⚠️ 但若载体落在 `docs/` ⇒ 触五步链）|
| **闸** | `cep_invariant_map_check.sh` 无需改（判据已就位）· `cep_invariant_deferrals` 条目数 5 → 2 |
| **KPI** | **0/3 → 1/3** ⇒ 归档 ⇒ **0/3** |
| **风险** | ① `INV-CEP-08-IC1` 的 J3 通用形式可能**结构上不可判定** ⇒ 无载体 ⇒ 须 Q-4 裁「降级 + 偏离登记」② `INV-CEP-09-IC2` 可能**超出本仓范围**（Stage 4+ 仲裁器）⇒ 须 Q-4 裁「移交口径」③ `CEN-Ib8` 的载体可能顺带成为档 2 扩面的前置 ⇒ 与 `cep-tier2-second-family-tilelink` 的依赖须显式登记 ④ 载体写成恒真占位 ⇒ 闸判据 ⑤ 必红（AC-3 兜底） |

## 待裁

⛔ **本单元有 architect 硬前置（Q-4）** —— 登记册 §1.1 B-1a 明写「判据须 architect 先定义（否则 builder 自行发明 invariant 语义 = 越权）」。

| 问 | 内容 |
|:--:|---|
| **Q-a** | `CEN-Ib8` 的「engine egress 可观测面」具体形态 = 复用 `protocol_engine_base.hh` 的 `EventObservation`（档 2 已建的观测面），还是另建？⇒ **建议复用**（档 2 观测面已在 `include/tlm/cep/protocol_engine_base.hh` 落地 ⇒ 复用 ⇒ 不新增基类面） |
| **Q-b** | `INV-CEP-08-IC1`（J3 通用形式）：给出可判定形式，还是降级为「单点非 vacuous 判据」并登记偏离？ |
| **Q-c** | `INV-CEP-09-IC2`（多 VC 仲裁器）：在本仓引入最小仲裁模型，还是登记为 Stage 4+ 移交？ |
| **Q-d** | 3 个载体的落点白名单（哪些文件允许改）？ |

⇒ **Q-a ~ Q-d 合并为登记册 §6.4 的 Q-4 一次裁决**（architect 域，产出 `.rddf/reviews/` 下的判据文件）⇒ 裁决须**逐条给出「可断言对象 + 载体形态 + 判据写法」三要素**（§6.4 Q-4 完成判据）⇒ 本单元在该裁决落盘后方可开工（前置命中判据 = 裁决文件存在且 `grep -F` 命中 3 条 id）。

## 审查轨迹（planner 起草 · 2026-11-20）

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|---|---|
| 1 | metis | 指出「5 条 vs 3 条」口径需核 + 判据须 architect 先定 | ⇒ 本单元按 JSON 实测 **5 条**拆批，批 1 = 3 条 |
| 2 | oracle | **REJECT** · 修法 1「I-5 前置挂错 + 队列 1 缺项」 | ⇒ v3 增补 **Q-4**（architect 判据立项）并显式定义批次构成 |
| 3 | critic（v2） | **REJECT** · B-1 假事实 + B-2 越权 + B-3 重复认领 | ⇒ 本单元不碰 B2 行；批次归属唯一 |
| 4 | critic（v3） | **REJECT** · BLOCKER-1 | ⇒ 本单元不含 KPI 活口径面（归 `cep-stage-gate-p5-hardening`） |
| 5 | critic（v4） | **APPROVE_WITH_CONDITIONS** | ⇒ 本单元的「7 处全锚 / 闸头转指」两条 SHOULD_FIX 不涉及本单元 |

⇒ **本单元须待 Q-4 architect 裁决后开工**。
