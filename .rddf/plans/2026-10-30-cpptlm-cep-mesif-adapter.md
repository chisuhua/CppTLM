# 队列 #9 实现层 `cpptlm-cep-mesif-adapter` —— 实施计划

> **状态**: ✅ **实施完成并归档**(2026-10-30,Entry #105 / #106)· **起草**: 2026-10-30 · **双审**: oracle（`mergeable: no`,3 BLOCKER）+ metis（4 BLOCKER）· **修订**: v0.4
> **结果**: WU-0 ~ WU-8 全部完成,change 已归档(2 份 delta spec 合并进 `openspec/specs/`);`implemented` 实例 **2 → 3**;S1-3 已触发且真实义务(SLICC 备选重评)已出结论;**WU-9(H7 对拍基准)按 J-2 独立立项,不属本 change 门禁**(本季度内单独落,本文件不追踪)
> **角色**: planner 起草 → builder 实施（见 WU-0）
> **队列**: `obj-cep-platform.md §4.0` **队列 #9**（Stage 1 · 串行 14 季度中的第 9 位）
> **上一环**: 规约层 `cpptlm-cep-mesif-state-spec` 已归档（Entry #101）+ 跨仓逐格追认闭合（Entry #102 / #103）
> **本文件取代**: `/home/ubuntu/.opencode/plan/20261030-cep-mesif-adapter-implementation-order.md`（v0.1，双审后修订并移入）

---

## Goal (目标)

把 **`mesif` 变成第 3 个 `status=implemented` 的 CEP 协议实例**，并交付**第 6 种 SharerTracker 实现 `HierarchicalDirectory` 的首个行为实现**（`ADR-CEP-04 §13` 的落地义务 = R-1 阻塞项的真正销号点）。

落地即触发 **S1-3**（Stage 1 最后一个 ⛔ 未触发的准入前置）。

**次级交付**（不可当主目标，但会被审到）：
- **首个「adapter 直接消费规约层载体」的模式**（`moesi_gpu` / `chi_rnf` 都是**自带表**；`MesifAdapter` 改为**委托消费** `MesifStateSpec::kTable`）⇒ 单真源链，为队列 #10（CXL）立模板
- 为 Stage 2 的 CXL 对称铺路（`mesif` 是**目录型跨卡**架构 ⇒ 证明 Directory & Ordering Plane 能承载非 snoop 型协议）

---

## 1. 背景与阻塞状态

### 1.1 三个阻塞的清零情况

| 阻塞 | 状态 | 证据 |
|------|------|------|
| R-1 第 6 种 SharerTracker `HierarchicalDirectory` | ⚠️ **裁决已落，行为未实现** | `ADR-CEP-04 §13`（Entry #96）⇒ `sharer_tracker.hh:172` enum + `:205-207` traits 行，**无 class** |
| ArchForge 规约缺口 G1（无状态机规约）/ G2（无 5→4 投影表） | ✅ 已闭合 | 规约层已归档（`include/tlm/cep/mesif_state_spec.hh`）|
| 跨仓追认债 31/35 格 | ✅ 已闭合 | ArchForge `42 号` 35 格结论 ⇒ `pending` 0/35，`origin_kind` = **4/10/15/6** |

### 1.2 为什么不能「照 MOESI 推」

MESIF 5 态与 MOESI 6 态**词表都不同**，同名态语义未必同构（MESIF 的 `F`(Forward) = 单 Owner + 多 Sharer，MOESI 里**无对应态**）⇒ 照搬会产出「看似规约、实则杜撰」的转换表。规约层已把该坑填平；实现层**只消费、不重推**（见 AC-1）。

### 1.3 为什么规约层闭合后**仍不能**直接开写

| # | 残留 | 性质 |
|---|------|------|
| **A** | `HierarchicalDirectory` 的**行为语义**在 ArchForge 与本仓**都无规约**（只有 caps 声明 + 一句注释级描述）| ⚠️ **最大的「看似完整实则杜撰」风险**（重蹈 G1/G2 覆辙）|
| **B** | `mesif` 的 `E + Wr → M` **静默升级**在平台级**无表达**（`perm_upgrades.json` 缺 `mesif` 行；`current_state` 自述「`mesif` 无该能力」是**事实性错误**）| ⚠️ 硬钉会在实施中途爆炸 |
| **C** | `mesif.atomic.supported = true` + `intent_field = "consistency_intent"` **全仓零消费点** | ⚠️ 声明了强能力却无任何任务/判据 |
| **D** | S1-3 的**计数口径**未定（声明实例 ≥3 vs `implemented` ≥3）| 验收时会扯皮 |

---

## 2. 已确认的裁决（2026-10-30 用户采纳）

| # | 议题 | 裁决 | 理由 |
|---|------|------|------|
| **J-1** | improvement 草稿属 planner 边界（`.rddf/improvements/**`）| **切 planner 起草 → 切回 builder 实施** | 对齐队列 #6 / #8 先例 |
| **J-2** | **H7 对拍基准**（gem5 Ruby / ArchForge 参考模型 / RTL benchmark）是否进 #9 硬前置 | **不进** | #9 是**语义载体**，非性能模型 ⇒ 对拍属 Stage 2。H7 仍需**本季度内**独立落（WU-9） |
| **J-3** | `07 号` 漂移修正的时序 | **与 WU-7 归档同批**（同批 M1 两步授权）| 避免两次冻结面重算 |
| **J-4**（双审新增）| `status` 翻转的时机 | **折进 WU-7 批首**（判据与装配实测之后）| `INV-CEP-12-IC3`（`status` 变更 ⇒ 计数同步）**无机器闸** ⇒ 翻转与计数同步必须同批，否则出现「JSON 说 implemented、`07 号` 说 2 个」的无闸窗口 |

---

## 3. 关键约束（设计要点摘要）

| # | 约束 | 依据 |
|---|------|------|
| **C-1** | **`MesifAdapter` SHALL NOT 携带自有转换表 / 投影表副本** ⇒ 必须 `constexpr` 委托消费 `MesifStateSpec::kTable` + 三元组 | 规约层 `mesif_state_spec.hh:440-443` 已预埋「供未来实现层消费」；§8 风险表第 1 行只有在此形态下才**结构性不可能** |
| **C-2** | **`perm` 4 值锁**不可扩（`ADR-CEP-02` CEN-Ib11）⇒ `mesif` 5 态必须投影进现有 4 值 | `ADR-CEP-02` / `ADR-CEP-05` |
| **C-3** | **`CATEGORY_*` 仍 3 类**（链路层 flit 类别）⇒ 6 类一致性消息词表与 `CATEGORY_*` 的关系由 **adapter 处的显式映射表**承担（**非恒等**）| `ADR-CEP-12 §8.3` ② 分层纪律 + IC2 条件 ③ 补充约定 |
| **C-4** | **`status` 是本仓权威字段**，不参与 ⑥ 逐字段镜像比对（豁免**严格限于** `status` 一键，其余 12 键逐字段严格）| `ADR-CEP-12 §12 ⑥` + Entry #102（`authority_migrations` 已清空 ⇒ `mesif` 回到镜像域，12 键实测全等）|
| **C-5** | **`protocol_instance.schema.json` 零改动**（`perm_upgrades.json` / `protocol_instances.json` 均「诞生即不冻结」）| 队列 #6 / #8 先例 |
| **C-6** | **`sharer_tracker.hh` 不在 M1 冻结集** ⇒ WU-3 改它**不需**两步授权 | 冻结清单 40 件实测无它 |
| **C-7** | **判据不锁语义正确性**，只锁完备性 / 可审计性 / 待追认标记 / 内部自洽 ⇒ 语义残余风险由**登记**承担（§5）| 规约层 design `D-ζ` |
| **C-8** | **JSON 链装配**（**非** programmatic bridge）| `ADR-CEP-16` 明确排除 C++ programmatic 路径 |

---

## 4. Work Units（TDD 5 步，WU-0 ~ WU-9）

> **每个 WU 的 TDD 5 步**：① 写判据（先红）→ ② 实现载体 → ③ 跑判据（转绿）→ ④ 反向 fixture（毒化必红 + 正控）→ ⑤ 门禁回归 + 勾 `tasks.md`
> **编号映射**：v0.1 的 P0-0 ~ P0-8 → WU-0 ~ WU-8；v0.1 的 P1-3 → WU-9。

### WU-0: 角色切换（planner → builder）— 2026-10-30

- **产出**：`.rddf/state/role.json` `current_role` = `planner` → 实施前切 `builder`（`history[]` 保留最近 20 条）
- **判据**：`role.json` 记录切换轨迹；**不改**任何其他文件
- **注**：当前已完成 `planner` 切换（v0.2 起草）；`builder` 切换在 WU-1 起草完成后执行

### WU-1: improvement 5 段草稿 — 2026-10-30

- **产出**：`.rddf/improvements/cpptlm-cep-mesif-adapter.md`（**Why / What Changes / Acceptance / Capabilities / Impact**），走 `add-improve` **HARD-GATE**（brainstorm 先于写文件）
- **必含**：
  - **Why 段**：残余风险 **R-1**（按档批量 14 格证据强度弱于逐格）**显式登记为已知输入** ⇒ **不得**写成「追认闭合 ⇒ 无残余风险」
  - **Capabilities / Impact 段**：§5 的 **R-3 补偿判据形态**（`LocalView.dirty` 掩码 + 写回接线 + 反向 fixture）**显式登记**
  - **Impact 段**：残留 **A**（`HierarchicalDirectory` 行为语义无真源 ⇒ 行为决策逐条登记 + 挂跨仓追认债）**显式登记**
- **判据**：`add-improve` HARD-GATE 通过；草稿 5 段齐备

### WU-2: 建 change 目录（4 件套）— 2026-10-30 ~ 2026-10-31

- **产出**：`openspec/changes/cpptlm-cep-mesif-adapter/{proposal.md, design.md, specs/, tasks.md}`
- **硬前置**：**先实测** `openspec list --json` ⇒ CEP 内部 Proposed 并发 **< 1**（当前 0/3，per §4.7.2 step 0）
- **命令逐字写死**：`openspec validate cpptlm-cep-mesif-adapter --strict` ⇒ **rc=0**（零 WARNING）
  - ⚠️ **不得**跑 `openspec validate --all --strict` 判本 change（**全仓有预存红**：6 个 spec 的 placeholder Purpose + `vram-controller-functional-mode` 2 条 ERROR）
  - ⚠️ **长度预算**：delta spec 的 Requirement 正文 **> 500 字符触发 WARNING**（预存红里已有此型）⇒ 撰写时控制单条长度
- **design.md 必含**：§3 的 C-1 ~ C-8 关键约束 + §5 的 R-3 补偿判据形态 + §5 的 `HierarchicalDirectory` 行为决策登记表（见 WU-3）
- **判据**：`openspec validate cpptlm-cep-mesif-adapter --strict` rc=0；`tasks.md` 已建且含 TDD 5 步分组

### WU-3: Phase 0 —— 语义载体（`MesifAdapter` + `HierarchicalDirectory`）— 2026-11-01 ~ 2026-11-08

- **产出**：
  - `include/tlm/cep/mesif_adapter.hh`（header-only；**零自有表副本**，委托消费 `MesifStateSpec::kTable`）
  - `include/tlm/cep/sharer_tracker.hh` 增 `HierarchicalDirectory` class（`SharerTrackerIf` 派生）
  - 6→3 消息类映射表（`CATEGORY_*` 3 类，非恒等）
- **TDD**：
  1. 先写判据（AC-1 / AC-2 / AC-3 / AC-8）⇒ 红
  2. 实现载体
  3. 跑判据 ⇒ 绿
  4. 反向 fixture：① `mesif_adapter.hh` 内**伪造一份表** ⇒ 必红（防未来有人复制）② 删一行映射 ⇒ 必红 ③ 写错类别 ⇒ 必红
  5. 门禁回归 + 勾 `tasks.md`
- **`HierarchicalDirectory` 行为决策登记义务**（残留 A 的防护，**必做**）：
  - 凡 ArchForge 无规约的行为决策 —— 层级数（`sharer_tracker.hh:138-141` 注释级描述「L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray」）/ `ESCALATE_UP` 是否使用 / `snoop_fold` 合并规则是否与 `FullMapDirectory::join_state` 同源 / handle 满时的拒绝语义 / **F 态（转发点）在 tracker 中的表示**（`SnoopState` 只有 `I/S/E/M`，**无 F** ⇒ 「谁是转发点」对 tracker 不可见；`LocalView::forward_to` 可承载，但须显式决策）—— **逐条**在类注释 + `design.md` 以 **`basis` 式**登记（推导依据 + 是否待追认），并**显式挂跨仓追认债** ⇒ **不得**写成「已裁决」
  - 判据：类注释含决策登记表；`design.md` 含同表；**表内每条决策都有 `basis` 字段且非空**
- **判据**：见 AC-1 / AC-2 / AC-3 / AC-8

### WU-4: Phase 1 —— 插件引擎（`MesifProtocolEngine`）— 2026-11-09 ~ 2026-11-15

- **产出**：
  - `include/tlm/cep/mesif_protocol_engine.hh`（`ProtocolEngineBase<N>` 派生 + `CohIfBundle` 承载）
  - `configs/cep_mesif_pe_minimal.json`（最小装配 JSON；对齐 `configs/cep_chi_rnf_pe_minimal.json` 先例）
  - `include/chstream_register.hh` 增 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED("MesifProtocolEngine", ...)`（先例 `:133`）
- **必含三条语义**：
  1. **驱动 `MesifAdapter` 表**（非空转）
  2. **驱动 `HierarchicalDirectory`** —— 在 `M + SnpShared → F` 路径上把「写回发生」喂给 tracker（`LocalView.dirty` 由置转清）⇒ **R-3 补偿路径被接线**
  3. **atomic 路径**（残留 C）：`mesif.atomic.supported = true` ⇒ 引擎 SHALL 接受 atomic 事务并按 `vc_binding` 路由至声明的 VC（`ualink_fabric.atomic → 1`）；`consistency_intent` 的生产/消费路径 —— **要么实现，要么在 `design.md` 显式登记为「声明超前于实现」的已知缺口**（**不得沉默**）
- **TDD**：先写判据（AC-4 / AC-6 / AC-7）⇒ 红 → 实现 → 绿 → 反向 fixture（① 断掉引擎→tracker 的写回接线 ⇒ 必红 ② atomic 走错 VC ⇒ 必红 ③ atomic 被拒绝闸误杀 ⇒ 必红）→ 门禁回归 + 勾 `tasks.md`
- **判据**：见 AC-4 / AC-6 / AC-7

### WU-5: Phase 2 —— 装配实测 + 平台级声明 — 2026-11-16 ~ 2026-11-19

- **产出**：
  - `include/tlm/cep/perm_upgrades.json` 增 `{instance_id: "mesif", from: "UNIQUE", to: "WRITEBACK"}`；**修订** `derived_from.current_state`（删除「`mesif` 无该能力 ⇒ 不出现」这句**事实性错误**，改为 3 个实例均有该能力）
  - `test/test_cep_adapter_schema_contracts.cc` **重基线**（`silent_upgrades.size()` 2→**3**；`no_upgrade_instances` 1→**0**）
  - 装配实测：`INV-CEP-12-IC2`（取代后 3 条件）逐网络校验通过；`evaluate_channel_dependency_gate` 在 **3 实例共存**声明下 verdict 无环
- **⚠️ 负控见证的空转陷阱**（判据纪律第 1 条）：

  `no_upgrade_instances` 是**派生**量（`adapter_contracts_loader.hh:213-224` = instances − 已声明）。加了 `mesif` 后**无任何实例**落在其中 ⇒ 原断言「`size()==1` 且 `[0]=="mesif"`」变成**恒真空转**（无鉴别力）⇒ **必须**把负控见证从「事实断言」改造为**规则级双向判据**：
  - 判据形态改为：**「有声明 ⇒ 不在 `no_upgrade_instances`；无声明 ⇒ 在」**这条**双向规则**
  - **正控**：真实数据（3 个实例全有声明 ⇒ `no_upgrade_instances` 为空）
  - **反控（可执行反向 fixture）**：传入**合成输入**（把 `mesif` 从 `upgrades.silent_upgrades` 移除）⇒ 断言 `mesif` **必然**出现在 `no_upgrade_instances` 里 ⇒ 若派生逻辑坏了即红
  - 该形态与规约层 `deviations_are_consistent(t, list)` 的**可变异形态**同款（`mesif_state_spec.hh:638`）
- **⚠️ 反向 fixture 重设计**：原「`mesif` 注入 ⇒ 红」的毒化**不再是毒化**（mesif 合法）⇒ 必须换新毒化形态，且脚本须**响亮报红防空转**（per Entry #103 同类先例）
- **判据**：见 AC-5 / AC-9

### WU-6: Phase 3 —— 判据 + 反向 fixture — 2026-11-20 ~ 2026-11-26

- **产出**：
  - `test/test_cep_mesif_adapter.cc`（契约测试）
  - `scripts/test/cep_mesif_adapter_reverse_verify.sh`（新反向 fixture 脚本，**ctest 注册**）
- **「因正确原因红」的机器化**（v0.1 只写了口径，本轮补机制）：
  - 每条 fixture **声明期望失败模式**（`compile-lock` / `runtime-predicate`）
  - probe 输出**失败判据 id** ⇒ 脚本 **grep 断言「红在预期的判据上」**，而非仅断言「红了」
  - ⚠️ **与既有先例的差异须显式说明**：`cep_mesif_state_spec_reverse_verify.sh:94` 把「编译失败」与「运行期谓词失败」**合并计数**（「两者都算被抓到」）⇒ 本脚本**不得**沿用该合并口径（否则「正确原因」无机器区分）
  - **两层判据 id 分离**：若反向 fixture 毒化的是**规约载体**（`mesif_state_spec.hh`），规约层判据与实现层判据会**同时**红 ⇒ probe 必须把两层判据 id **分开打印** ⇒ 归因不模糊
- **既有套件零回归**（防 fixture 空转，per Entry #103 ③）：`cep_mesif_state_spec_reverse_verify.sh` 的 **11 条**毒化必须仍 **11/11**
- **判据**：见 AC-13

### WU-7: Phase 4 —— `status` 翻转 + `07 号` 漂移修正 + 完成归档 — 2026-11-27 ~ 2026-12-03

> **本 WU 是唯一触碰 M1 冻结面的 WU** ⇒ 严格按下列**批内序**执行（先例 Entry #95 / #101）。

- **批内序（写死，不得乱序）**：

  | 序 | 动作 | 说明 |
  |---|------|------|
  | ① | `protocol_instances.json` 的 `mesif.status` → **`implemented`** + `INV-CEP-12-IC3` 计数同步 | 折叠面**非**冻结（C-5）|
  | ② | **冻结面活口径修正**：`07 号` 漂移 **10 处** + `cep-sharer-tracker-base/spec.md` **2 处**（见下表）| 冻结面 ⇒ 走两步授权（**一次覆盖两件**）|
  | ③ | **commit 冻结面产物** | |
  | ④ | `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` | |
  | ⑤ | **commit 清单** | |
  | ⑥ | `openspec archive cpptlm-cep-mesif-adapter`（**完成归档**语义 ⇒ delta spec **合并**进 `openspec/specs/`）| |
  | ⑦ | **KPI / 状态列回填**（`obj §4.0` / `§5.2` / `phase-10.md` / `.rddf/roadmap.md` / `AGENTS.md`）| ⚠️ **KPI 只能在归档之后改**（连锁修 ①）|

- **`07 号` 漂移普查清单**（J-3 同批 ⇒ **一次授权覆盖全部 occurrence**，否则要二次重算）：

  | 行 | 现状 | 目标 |
  |---|------|------|
  | `:75` | 「SharerTracker 5 实现 + caps descriptor(§5.2)」| → **6** 实现 |
  | `:86` | 「SharerTracker 5 实现 + caps descriptor(token 归属见 §5.1.3)」| → **6** 实现 |
  | `:276` | 「IC3:5 个条目生命周期事件名 + 时序契约在 5 种实现一致」| → **6** 种实现 |
  | `:278` | 「IC6:5 种实现 hazard 行为契约…」| → **6** 种实现 |
  | `:323` | §5.1.1 MESIF 行状态列「📋 已排期」+「实现层 change 待建」| → **✅ 已落地** + 落点索引 |
  | `:330` | §5.1.1 状态注「`mesif`…**余 2 项缺口**未闭 ⇒ 排期 ≠ 就绪」+「`chi_rnf` = 第 **2** 个」| → 缺口**已闭** + `mesif` = 第 **3** 个 |
  | `:573` | 「`ADR-CEP-04 §2` SharerTracker 5 实现」| → **6** 实现 |
  | `:679` | §5.2「SharerTracker 5 实现 \| 🟡 **1 / 5**」+ 行号引用 `:178-195` | → 「**6** 实现 \| 🟡 **2 / 6**」+ 行号刷新 |
  | `:681` | §5.2「ProtocolEngine adapter 实例 \| 🟡 **1 个已落地**」| → **3 个**（+ `mesif` 落点）|
  | ~~`:721`~~ | ⚠️ **实施期判定纠正**:该行属 **2026-10-27 第二则 Status Update 块**(`## 附录 · Status Update 2026-10-27 第二则`)⇒ 是**历史块**,按 append-only **不回改** | 由**第十五则**(2026-10-30)登记当前态覆盖;本行**不计入**「活口径已修」处数 ⇒ 实际修 **9 处** + 新增第十五则 |

  > ⚠️ **历史 Status Update 块不可改**（append-only）：`:701` / `:702`（2026-10-25 块）、`:905`（第十二则「余 2 项缺口 ⇒ 队列 #9 仍不可启动」）⇒ 由**新的 Status Update 第十五则**覆盖，不得回改。
  > ⚠️ **invariant 计数不得变**：改 `:276` / `:278` 的「5 种实现」措辞**不改变** invariant 条数（66 / 编号位 67 / 15 组）⇒ 改后 `cep_docs_sync_check.sh` **必须** rc=0。

- **`cep-sharer-tracker-base/spec.md` 的 2 处**（WU-3 实施期**实测发现**，v0.1/v0.2 **漏项**）：

  | 位置 | 现状 | 目标 |
  |---|---|---|
  | `Purpose` 末句 | 「本 capability 交付**唯一行为实现** `FullMapDirectory`；其余 4 种实现的声明层登记随队列 #6~#9 落地」| → 交付**首个**行为实现；其余 4 种随队列 **#10 ~ #11**；第 6 种 `HierarchicalDirectory` 的**行为面**由 `cep-hierarchical-directory` 承担 |
  | `Requirement` 首句 | 「… + FullMapDirectory **唯一行为实现** 固化为**可执行契约**」| → 「… + FullMapDirectory **行为实现（首个）** 固化为**可执行契约**」|

  > **为何必须改**：该文件是**主 spec**（读者入口）。`HierarchicalDirectory` 落地后「**唯一**行为实现」是**事实性错误** ⇒ 与 `07 号` 的 `1 / 5 → 2 / 6` **同型**，属**同一批活口径漂移**。
  > **为何不提前改**：它是 **M1 冻结面** ⇒ 提前改要**第二次授权**；折进 WU-7 批 ⇒ **不增加授权次数**（J-3 的「避免两次冻结面重算」本意）。
  > ⚠️ **`deferred` Scenario 措辞保持**（4 个实现仍 deferred）⇒ `test_cep_sharer_tracker_base.cc` 的 `read_capability_spec` 断言（`只做声明层断言` / `deferred`）**不受影响**。

- **17 落点同步**（`obj §4.7.3` 收尾清单 A1-A3 / B1-B7 / C1-C3 / D1-D4）**逐项**勾
- **#9 特有移交义务**（v0.1 遗漏，本轮补）：队列 #9 行当前**持有** Stage 1 准入前置 + 孤儿前置的 **owner 义务**（per Entry #95 转指）⇒ 归档时必须**再转指**（给 #10 或宣告 Stage 1 收口）—— 对齐 Entry #95 的「移交已转出」先例
- **新脚本登记**：新 reverse-verify 脚本入 `scripts/README.md` §CEP 契约守门脚本表 ⇒ **「12 项 CEP 脚本」计数 → 13**（`obj` / `AGENTS.md` / `scripts/README.md` 三处同步）
- **判据**：见 AC-10 / AC-14 / AC-15

### WU-8: S1-3 触发后 —— SLICC 备选重评 + 承接项 2 滚动复核 — 2026-12-04 ~ 2026-12-05

- **⚠️ 口径钉死**（v0.1 遗漏，本轮补）：**S1-3 判据 = `implemented` 实例数 ≥ 3**（**不是**声明实例数 ≥ 3 —— `protocol_instances.json` 今日**已有 3 个声明实例**，`mesif.status` 仍是 `planned`）⇒ 触发证据 = `mesif.status == "implemented"` + `obj §4.0` 行改 + 台账 Entry
- **S1-3 的真实义务**（`ADR-CEP-12 §8.4`，**不是** v0.1 写的「per-network 嵌套形态复核」）：
  - 义务 = **重评 SLICC 式「协议本地虚拟网络 + 装配期绑定」备选**（`ADR-CEP-12:303-312`）—— 该备选的登记理由正是「当前为 0 个 `implemented` 实例 —— 这是将来最便宜的改点，**别让这个选项无声死掉**」
  - 产出：结论（**维持嵌套形态** / 迁移 SLICC 式）入台账 Entry + `07 号` 新 Status Update
- **承接项 2 = 滚动复核的第 2 次**（不是首次）：`obj §4.0` 队列 #6 行已标「✅ 本 change 已交付（2026-10-27 归档）」⇒ 本次是**第 2 次滚动复核**（节奏源：umbrella design D4.5）⇒ **不得**与 #6 行的「已交付」形成台账矛盾
- **判据**：见 AC-11 / AC-12

### WU-9: H7 对拍基准指定（**不进** #9 硬前置）— 2026-Q4 内独立落

> **状态**: ⏳ **未启动**(2026-10-30)· 独立立项 ⇒ **不属**本 change 的任何门禁;
> 本 change 归档时 WU-9 **未开始**,不得记作「已完成」(per J-2)。

- **产出**：gem5 Ruby / ArchForge 参考模型 / RTL benchmark **三选一**的裁决 + 对拍手段说明
- **落点**：`obj §10` Stage 0 候选 P0 行 + 对拍手段说明文档
- **约束**：**独立**立项，**不**作为 WU-2 ~ WU-8 的门禁项（per J-2）
- **理由**：R7 = Stage 1 唯一未完成的 P0；但 #9 是语义载体（表驱动状态机），非性能模型 ⇒ 硬塞会把 10 周语义交付拖成双倍

---

## 5. 残余风险义务（**不可写成「已消除」**）

| # | 风险 | 实现层的义务 | 落点 |
|---|------|-------------|------|
| **R-1** | 按档批量的 **14 格**证据强度弱于逐格（档位理由是**归纳**，逐格是**枚举**）⇒ 某格**语义判断**有误时，档位理由覆盖不到 | `proposal.md` 的 **Why 段显式登记**为已知输入；**不得**因「追认闭合」而写成「无残余风险」| WU-1 / WU-2 |
| **R-3** | 纠正 `M+SnpShared`（`S`→`F`）后，「**已写回(干净)**」与「**仍脏**」在 4 值 `perm` 上**同值不可分辨** ⇒ **写回事实在 perm 接口不可见** | ⚠️ **补偿维度 = `LocalView.dirty` 掩码 + 写回接线**（`sharer_tracker.hh:247-252` 的 `dirty` bitmask），**不是**「谁持有」—— 后者是**损失 ①**（`kPermMergeNote`，「是否还有其他 sharer」）的补偿，与损失 ③ 是**两个不同维度**（v0.1 混写，本轮修正）<br>⚠️ **补偿不会自动发生** ⇒ 引擎须在 `M+SnpShared→F` 路径上把「写回发生」喂给 tracker | WU-1 / WU-4 / WU-6 |
| **A**（新）| `HierarchicalDirectory` 行为语义**无真源** ⇒ 实现者被迫自推 ⇒ 重蹈 G1/G2 覆辙 | 行为决策**逐条 `basis` 式登记** + **挂跨仓追认债** ⇒ **不得**写成「已裁决」| WU-1 / WU-3 |
| **C**（新）| `atomic.supported = true` 全仓零消费点 | 实现 **或** 在 `design.md` 显式登记为「声明超前于实现」的已知缺口 ⇒ **不得沉默** | WU-1 / WU-4 |

---

## 6. Test Plan（验收清单 AC-1 ~ AC-15）

| # | AC | 判据形态 | 反向 fixture | 正控 |
|---|----|---------|------------|------|
| **AC-1** | `MesifAdapter` **零自有表副本**（委托消费 `MesifStateSpec::kTable` + 三元组）| 运行期 **35 格 pass-through**（`MesifAdapter::transition(s,e) == MesifStateSpec::transition(s,e)`）+ **静态扫描**（`mesif_adapter.hh` 内无 5×6 表字面量）| 在 `mesif_adapter.hh` 内**伪造一份表** ⇒ 必红 | 载体自身 35 格全通 |
| **AC-2** | 5→4 `perm` 投影 + 瞬态无投影（`INV-CEP-05-IC1`，本规约 5 态全稳定 ⇒ 平凡成立）| 逐格相等 + 瞬态列表为空断言 | 改一格投影 ⇒ 必红 | 5 态投影齐全 |
| **AC-3** | `HierarchicalDirectory` 的 **caps 4 字段 + behavior 2 字段**（`kTrackerTraits[5]` = `SYSTEM/EXACT/GLOBAL/HIERARCHICAL` + `STALL` + 无广播兜底）与实例声明**逐字段相等**（**三方**：JSON ↔ traits 行 ↔ class 成员）；且**行使多级聚合**这一与 `FullMapDirectory` 的**差异点**（否则 = 「FullMap 克隆 + 纸面 caps」，正中 `ADR-CEP-04 §13` 否决方案 D 的判词）| 逐字段断言 + STALL 行为断言 + 聚合路径断言 | 改 caps 一字段 ⇒ 必红；行为与 traits 矛盾 ⇒ 必红 | 载体三方一致 |
| **AC-4** | `MesifProtocolEngine` 可被 **JSON 链**装配；一次 CohIf 事务**穿过**引擎抵达对端；**引擎可观测点能区分「走了 adapter 表」与「空转」**（对齐 `NullProtocolEngineCounters` 的判别力口径 —— 只记调用次数的计数器不算有判别力）| 装配 E2E + 最近一次转换的 `(from,event,to)` 快照断言 | 引擎退化成 Null 换名 ⇒ 必红 | 快照非空且与输入一致 |
| **AC-5** | `perm_upgrades.json` 含 `mesif` 行（`UNIQUE → WRITEBACK`）+ `current_state` 自述已修订 + `no_upgrade_instances` 的**负控见证**改造为**双向规则**（有声明 ⇒ 不在；无声明 ⇒ 在）| 声明源装载 + 双向规则断言 | 合成输入：移除 `mesif` 声明 ⇒ `mesif` 必出现在 `no_upgrade_instances`；**旧毒化形态作废**（`mesif` 注入不再是毒化）⇒ 换新形态 | 真实数据下 3 实例全有声明 ⇒ 空集 |
| **AC-6** | atomic 路径：不被拒绝闸误杀 + 按 `vc_binding` 路由至 `ualink_fabric` 的 VC1；`consistency_intent` 生产/消费路径**实现或显式登记缺口** | VC 路由断言 + 拒绝闸行为断言 + `design.md` 登记存在性断言 | atomic 走错 VC ⇒ 必红；被误杀 ⇒ 必红 | 合法 atomic 事务被接受且落 VC1 |
| **AC-7** | **R-3 补偿可观测**：驱动 `M + SnpShared → F` 序列后 —— ① `perm` 在转换前后**同为 `WRITEBACK`**（**不可分辨的正控**）② tracker 视图 **dirty 位由置转清**（**可分辨的判据**）| 转换前后 perm 相等 + `LocalView.dirty` 前后不等 | **断掉引擎→tracker 的写回接线** ⇒ dirty 位不转 ⇒ 必红 | 接线在位时 dirty 位转清 |
| **AC-8** | 6→3 消息类映射表：完备覆盖本实例用到的消息类 + 与 `adapter_schema_contracts.hh` 的 `identity_reference_map()` 对拍 + **恒等或非恒等必须显式登记并携带理由**（`mesif` 的 `forward_data` 按 `ADR-CEP-12 §8.3` 补充约定是 **`snoop` 槽的承载实例** ⇒ 映射**是否恒等本身是设计决策**，须显式）| 完备性 + 对拍 + 登记存在性 | 删一行映射 ⇒ 必红；写错类别 ⇒ 必红 | 映射表完备且对拍一致 |
| **AC-9** | 通道依赖闸在 **3 实例共存**声明下 verdict **无环** + **逐 `(instance, network, vc)` 标注**（对齐 `test_cep_chi_rnf_subset.cc:786-806` 先例）| `evaluate_channel_dependency_gate` 断言 | 破坏 `mesif` 的 `vc_binding` ⇒ 必红 | 3 实例共存时闸绿 |
| **AC-10** | `mesif.status == "implemented"` + 其余 **12 个非 `status` 键**与 ArchForge `32 号 §6.2` **逐字段全等**（⑥ 镜像闸对 `mesif` 恢复逐字段校验，per Entry #102）+ `INV-CEP-12-IC3` 计数同步 | 声明镜像比对 + 计数断言 | 改 `mesif` 的任一非 `status` 键 ⇒ 必红 | 12 键全等 |
| **AC-11** | **S1-3 口径钉死** = **`implemented` 实例数 ≥ 3**；触发证据 = `mesif.status=implemented` + `obj §4.0` 行改 + 台账 Entry；**真实义务 = `ADR-CEP-12 §8.4` 的 SLICC 式备选重评**（不是「per-network 嵌套形态复核」）| 计数断言 + 重评结论存在性（台账 Entry + `07 号` 新 Status Update）| 结论缺失 ⇒ 必红 | 3 个 `implemented` |
| **AC-12** | 承接项 2 的**滚动复核第 2 次**（节奏源 umbrella design D4.5）—— 复核结论入台账 | 复核记录存在性 | 结论缺失 ⇒ 必红 | 复核已落 |
| **AC-13** | 每条判据**配正控** + **可执行反向 fixture** + **「因正确原因红」机器判定**（每条 fixture 声明期望失败模式 `compile-lock` / `runtime-predicate`；probe 输出**失败判据 id** ⇒ 脚本 grep 断言「红在预期判据上」）；**两层判据 id 分离**；**既有 reverse-verify 套件零回归**（`cep_mesif_state_spec_reverse_verify.sh` 仍 11/11）| 脚本自检 + ctest 注册 | 脚本命中 0 次 ⇒ **响亮报红**（非静默跳过）| 干净载体全绿 |
| **AC-14** | 收尾：`tasks.md` **全勾** → **完成归档**（delta spec **合并**进 `openspec/specs/`）+ `obj §4.7.3` **17 落点逐项**已改 + **队列 #9 行的 owner 义务已再转指**（给 #10 或宣告 Stage 1 收口）+ **P1-1 的两步授权已执行**（J-3 同批承诺的落点）+ 新脚本入 `scripts/README.md` 表（计数 **12 → 13**，三处同步）| 归档产物 + 落点勾选 | 漏任一落点 ⇒ 必红 | 全部已改 |
| **AC-15** | 门禁全绿：12→**13** 项 CEP 脚本 + `docs_sync --strict` + `openspec validate --changes --strict` + `format --check` rc=0；`ctest -R cep` 全绿；全仓 `cpptlm_tests` 全绿；工作区干净 | 脚本 rc 码 | 任一 rc≠0 ⇒ 必红 | 全 rc=0 |

---

## 7. 判据纪律（本仓已固化，4 条铁律）

1. **纯负向断言必须有正控** —— 每条判据配正控，不得裸奔
2. **每条判据须有可执行反向 fixture**，且 fixture **必须因正确原因红**
3. **编译期锁不得只以「编译红」充当 ctest 判据** ⇒ 必须有运行期探针 + **判据 id 归因**
4. **fixture 在状态变迁后会空转**（改成当前值即空操作）⇒ 字段/载体变更后必须**重新基线化**，且脚本必须**响亮报红**（命中 0 次 ≠ 通过）

> **已实证**（Entry #103）：`origin_kind` 插入 `Basis` 后 3 条毒化锚点失效 ⇒ 脚本**命中 0 次直接报红**（非静默跳过）。
> **本轮新增实证点**：AC-5 的「`mesif` 注入 ⇒ 红」毒化**因 `mesif` 变合法而失效** ⇒ 与 AC-13 的 id 归因机制配套落地。

---

## 8. 冻结面纪律

**M1 冻结集 40 件**（`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`，当前 `authorized_by` = **Entry #102**）⇒ 含 `07 号` / `ADR-CEP-*.md` / `protocol_instance.schema.json` / `fabric*.json` / 9 份 main spec / 3 个 tools。

**改动任一冻结面成员 ⇒ 两步授权**：
1. commit 产物
2. `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`
3. commit 清单

> ⚠️ **`protocol_instance.schema.json` 零改动**（C-5）⇒ `mesif.status` 翻转 / `perm_upgrades.json` 增行**均不触发**冻结面重算。
> ⚠️ **`sharer_tracker.hh` 不在冻结集**（C-6）⇒ WU-3 自由改。
> ⚠️ **触发冻结面重算的只有 WU-7 ②** —— **两件**：`07 号` 漂移 10 处 + `cep-sharer-tracker-base/spec.md` 2 处活口径 ⇒ **一次** `cep_freeze_check.sh --update` 覆盖（J-3 同批）。

---

## 9. Risk Mitigation（本 change 子集）

| 风险 | 等级 | 缓解 | 回滚 |
|------|------|------|------|
| **实现层自己拍一套转换表** | 🟡 中 | AC-1（零副本 + 静态扫描 + 伪造表 fixture）⇒ **结构性不可能** | — |
| **`HierarchicalDirectory` 行为语义自推**（残留 A） | 🟡 中 | 行为决策 `basis` 式登记 + 跨仓追认债 + AC-3 差异化行为 | 归档后若发现 ⇒ 追加台账 + 修订 main spec |
| **R-3 补偿被「声称」而未被「演示」** | 🟡 中 | AC-7（perm 同值正控 + dirty 位判据 + 断接线 fixture）⇒ 补偿**可观测** | 同上 |
| **`mesif` 静默升级在平台级无表达**（残留 B） | 🟡 中 | AC-5（声明 + 双向规则负控 + 旧毒化重设计）| `perm_upgrades.json` 诞生即不冻结 ⇒ 单文件回滚 |
| **负控见证空转**（加 mesif 后 `no_upgrade_instances` 恒空） | 🟡 中 | AC-5 改造为**双向规则** + 合成输入反控 | — |
| **`atomic` 声明与实现脱节**（残留 C） | 🟡 中 | AC-6（实现 **或** 显式登记缺口）| — |
| **冻结面误触** | 🟡 中 | `cep_freeze_check.sh` rc≠0 即拦；改则两步授权 | `git revert` + `--update` 回填 |
| **17 落点漏同步**（历史已犯） | 🟡 中 | §4.7.3 清单逐项勾 + AC-14 | 补齐 + 台账勘误 entry |
| **既有 fixture 锚点空转**（改 `sharer_tracker.hh` / `mesif_state_spec.hh` 后） | 🟡 中 | AC-13 的**既有套件零回归** + 脚本命中 0 次响亮报红 | 重新基线化 |
| **KPI 并发冲突** | 🟢 低 | `openspec list --json` 实测 0/3 ⇒ 可直接建 | — |
| **S1-3 口径扯皮** | 🟡 中 | AC-11 口径钉死 + 触发证据三件套 | — |

---

## 10. Done Definition（#9 归档前）

- [ ] WU-0 ~ WU-8 全部完成，`tasks.md` **全勾**
- [ ] AC-1 ~ AC-15 全绿
- [ ] **完成归档**语义（delta spec **合并**进 `openspec/specs/`）
- [ ] `obj §4.7.3` **17 落点逐项**已改
- [ ] 队列 #9 行的 **owner 义务已再转指**
- [ ] `07 号` 漂移 **10 处**已修 + 新 **Status Update 第十五则**已写（不改历史块）
- [ ] M1 冻结面**一次**两步授权已完成（`authorized_by` = 新 Entry 号）
- [ ] 「12 项 CEP 脚本」计数 → **13**（三处同步）
- [ ] S1-3 由 ⛔ → ✅（口径 = `implemented` ≥ 3）+ **SLICC 备选重评结论**已落
- [ ] 承接项 2 滚动复核（第 2 次）已落
- [ ] 台账 **Entry #104**（及后续）已追加
- [ ] 工作区干净；**不**主动 `git push`

---

## 11. 关联文档

- **队列 / 状态 / 流程**：`obj-cep-platform.md` §4.0（队列 #9 行）· §4.7.2（状态机）· §4.7.3（**17 落点**收尾清单）· §5.2 / §5.4（KPI）· §8.1（风险）· §9.1（**H7**）· §10 · §11（台账，末条 Entry #103）
- **规约载体**：`include/tlm/cep/mesif_state_spec.hh`（35 格 `kTable` · `origin_kind` · 9 条偏离 · 语义损失 ①②③ · 静态锁 ⑪~⑫ · **三元组 `:440-443`** · `deviations_are_consistent` 可变异形态 `:638`）
- **判据 / fixture**：`test/test_cep_mesif_state_spec.cc`（6 case / 393 assertion）· `scripts/test/cep_mesif_state_spec_reverse_verify.sh`（**11/11**，`:94` 编译/运行**合并计数**口径）· `test/test_cep_adapter_schema_contracts.cc:283-285`（**负控见证**，本轮须改造）· `scripts/test/cep_decl_reverse_verify.sh`
- **声明源 / schema**：`include/tlm/cep/protocol_instances.json` · `protocol_instance.schema.json` · `perm_upgrades.json`（**`:5` `current_state` 含事实性错误**，本轮修订）· `adapter_contracts_loader.hh:213-224`（`no_upgrade_instances` 派生）· `adapter_schema_contracts.hh`（`identity_reference_map()`）
- **实现先例**：`include/tlm/cep/moesi_gpu_adapter.hh`（自带表 + 6→3 映射）· `chi_rnf_adapter.hh` · `chi_rnf_protocol_engine.hh`（`ProtocolEngineBase<N>` + `CohIfBundle`）· `cohif_protocol_engine.hh`（`CohIfNullProtocolEngine`）· `protocol_engine_base.hh:397-403`（`NullProtocolEngineCounters` 判别力口径）· `sharer_tracker.hh`（`:128` `SnoopState` 无 F · `:138-141` 层级注释 · **`:172` enum** · **`:186-208` traits** · **`:247-252` `LocalView{dirty,forward_to}`** · **`:286` `FullMapDirectory`**）
- **装配先例**：`configs/cep_chi_rnf_pe_minimal.json` · `configs/cep_cohif_pe_minimal.json` · `include/chstream_register.hh:133`（`REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED`）· `test/test_cep_chi_rnf_subset.cc:786-806`（通道依赖闸）
- **ADR**：`ADR-CEP-04 §13`（第 6 实现 + **§8.4 之外的方案 D 否决判词**）· `ADR-CEP-02`（canonical 10 流 + CEN-Ib11）· `ADR-CEP-05 §13.1`（`perm` 形态演进）· `ADR-CEP-12 §8.3` / `§8.4`（分层纪律 + **SLICC 备选**）· `ADR-CEP-12 §12 ⑥`（`status` 豁免）· `ADR-CEP-13`（域可见写判据 (a)(b)）· `ADR-CEP-16`（排除 programmatic bridge）
- **跨仓**：ArchForge `42 号`（35 格逐格结论）· `32 号 §6.2` / `§7.4` / `§7.2.1` · `25 号 §4.2` · 收讫回执 `cross-repo-ratification-apply-ack-receipt.md`（ArchForge `f0fb57d`）
- **冻结面**：`docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`（40 件 · `authorized_by` = Entry #102）

---

## 12. 检查清单（实施者自检）

- [x] WU-0 已切 `builder`；`role.json` 轨迹完整
- [x] WU-1 草稿含 R-1 / R-3 / 残留 A / 残留 C 的**显式登记**
- [x] WU-2 `openspec list --json` 实测 < 1 **在建 change 之前**
- [x] WU-2 用的是 `validate <name> --strict`（**不是** `--all`）
- [x] WU-3 `mesif_adapter.hh` 内**无** 5×6 表字面量（去注释后零 `State::`/`Event::`/`Perm::`/`kTable`）
- [x] WU-3 `HierarchicalDirectory` 行为决策表**逐条有 basis**（`kBehaviorDecisions[5]` + 全部 `pending=true`）
- [x] WU-4 引擎→tracker 写回接线**已接**且 AC-7 可观测（断接线 ⇒ `tracker_missing` 涨 + `dirty` 不转，判据必红）
- [x] WU-4 atomic 路径**已实现**（接受 + `ualink_fabric` VC1 路由意图 + `atomic` 意图位）+ `consistency_intent` 无独立字段**已显式登记**
- [x] WU-5 `no_upgrade_instances` 负控见证**已改造为双向规则**（`derive_no_upgrade_instances()` 抽出 + 合成输入三方向反控）
- [x] WU-5 旧毒化形态**已作废并替换**（`chi_rnf` CR-05 / `moesi_gpu` Scenario 6+7 共 4 处跨 change 硬钉重基线）
- [x] WU-6 fixture 声明**期望失败模式**（16 条:`compile-lock` / `runtime-predicate`）+ probe 输出 `[spec|impl|decl] <id>`
- [x] WU-6 既有 `cep_mesif_state_spec_reverse_verify.sh` 仍 **11/11**（零回归）
- [x] WU-7 批内序 ①~⑦ **未乱序**（KPI 在归档之后改；冻结面**两次** `--update`，偏差已在 Entry #106 ④ 登记）
- [x] WU-7 `07 号` **9 处**活口径已修（`:721` 实施期判定为**历史块** ⇒ 不回改，由第十五则覆盖）；**历史块未回改**
- [x] WU-7 `cep-sharer-tracker-base/spec.md` **2 处**已修（「唯一」→「首个」+ 队列区间）；**`deferred` 措辞保持**
      ⚠️ **2026-10-30 勘误**(台账 Entry #107):当时写的「队列 #6~#9 → #10~#11」是**未核实的臆测** ⇒ **已撤回**;改为「落位**已登记**」(`TreeManager` ↔ #11 · `DcohSymmetric` ↔ #12 · `SparseSnoopFilter` 已被 #6 占用、行为待补 · `BroadcastNull` 待裁决)—— **本计划与 base spec / `07 号` / `sharer_tracker.hh` 三处同源文本已同批更正**
- [x] WU-7 invariant 计数仍 **66 / 编号位 67 / 15 组**（`cep_docs_sync_check.sh` rc=0 实证）
- [x] WU-8 S1-3 口径 = **`implemented` ≥ 3**（实测 3）；重评对象 = **SLICC 备选**；结论 = **维持嵌套形态** + 选项不注销（改绑更强触发条件）+ 承接项 2 第 2 次复核
- [x] 全仓 `cpptlm_tests`（2029 case / 874636 assertion）+ `ctest -R cep`（40/40）全绿；工作区干净

---

## 13. 维护记录

| 日期 | 版本 | 修订 |
|------|------|------|
| 2026-10-30 | **v0.5** | **归档后勘误**(台账 Entry #107):撤回本计划自身引入的一处**未核实队列区间**(`#6~#9 → #10~#11`)⇒ 改为「落位**已登记**」;同时补 `S1-6`(已闭合)的 owner 登记行;新增 §8.1 **R11**(声明超前于实现)与 §10 backlog 行。⚠️ 该区间在 3 处同源文本(base spec / `07 号` / `sharer_tracker.hh`)⇒ **同批一次**冻结面授权更正,避免重算两次。 |
| 2026-10-30 | **v0.4** | **实施收口**：① 检查清单全勾；② `:721` 的普查判定**纠正**为「历史 Status Update 块 ⇒ append-only 不回改，由第十五则覆盖」（实际修 9 处活口径）;③ 冻结面**同批两次** `--update` 的偏差登记（首次 Entry #105:07 号 9 处 + base spec 2 处；第二次 Entry #106:07 号第十六则 + ADR-04 §14 + ADR-12 Status Update）;④ 状态变迁导致 `cep_gate_selftest` 2 条 owner-活性 fixture 锚点失效 ⇒ 锚点跟随 owner #9 → #10 （判据纪律第 4 条第二次实证）;⑤ WU-9 按 J-2 独立立项，不属本 change 门禁。 |
| 2026-10-30 | **v0.3** | **实施期补漏**（WU-3 实测）：发现 **第 2 件 M1 冻结面漂移** —— `openspec/specs/cep-sharer-tracker-base/spec.md` 自称「FullMapDirectory **唯一**行为实现」+「其余 4 种随队列 #6~#9」，在 `HierarchicalDirectory` 落地后成**事实性错误**。v0.1/v0.2 均断言「冻结面仅 `07 号` 一件」⇒ 本版修正为**两件、一次授权**（WU-7 ② 合并）。**不改**任何判据口径，只补冻结面普查清单。 |
| 2026-10-30 | **v0.2** | **双审修订**（oracle `mergeable: no` 3 BLOCKER + metis 4 BLOCKER）：① 补 `perm_upgrades.json` 静默升级线 + 负控见证空转陷阱（AC-5）② R-3 补偿维度由「谁持有」修正为 **`LocalView.dirty` 掩码 + 写回接线**（AC-7）③ P0-8/AC-9 由「per-network 嵌套形态复核」修正为 **`ADR-CEP-12 §8.4` SLICC 备选重评** + S1-3 口径钉死为 `implemented ≥ 3` ④ AC-1 改为「**零自有表副本** + 伪造表 fixture」⑤ 补 `atomic` 漏项（AC-6）⑥ 补 `HierarchicalDirectory` 行为决策登记义务（残留 A）⑦ `status` 翻转折进 WU-7 批首（J-4）⑧ 补 `07 号` 漂移**10 处**普查清单 + 历史块不可改标注 ⑨ 补 6→3 映射 AC（AC-8）+ 通道依赖闸 3 实例共存 AC（AC-9）+ 引擎探针判别力 AC ⑩「因正确原因红」机器化（期望失败模式 + 判据 id grep + 两层 id 分离）⑪ 补 owner 再转指 + 脚本计数 12→13 ⑫ 补 `configs/cep_mesif_pe_minimal.json` + 注册行交付物 ⑬ P1-4 改写为时效性裁决 |
| 2026-10-30 | v0.1 | 初版（`/home/ubuntu/.opencode/plan/20261030-cep-mesif-adapter-implementation-order.md`）：P0-0~P0-8 / P1-1~P1-4 / P2 顺序 + J-1~J-3 裁决 + R-1/R-3 义务 + AC-1~AC-10 |
