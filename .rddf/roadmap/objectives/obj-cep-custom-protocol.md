# Objective: CEP 用户自定义协议能力（SSP 声明面 → 瞬态态生成 → 真实物理链路）

> **性质**：**planner 产出**（目标叙事 + 实施路径面）—— 回答「按什么次序、闭什么缺口、谁来做、门禁是什么」。
> **⛔ 不是真源**：判据细节以 `docs/adr/ADR-CEP-*.md` 为准；实测状态以 §0.1 的**当场重跑**为准。
> **状态更新（2026-11-26 · 新建 · metis 1 轮已处置）**：本 objective 是**第二个跨 sprint 复杂目标**（触发依据见 §1.3）⇒ 与已 ⏸ 暂存归档的 `obj-cep-platform`（其终态 DoD Go 仍因 `B3b-2 = 0/4` 挂起）**并列**，⛔ 不取代、不合并。
> **编号空间（⛔ 必读）**：本 objective 的一切单元 id 以 **`CP-`** 为前缀（`CP-P-*` / `CP-Q-*` / `CP-I-*` / `CP-W-*`）。⚠️ **禁止**在本 objective 内使用裸 `Q-n` / `I-n` / `P-n` / `W-n` —— 该四个前缀与号段**已被 `obj-cep-platform-remaining.md` 占用**（其 `Q-1~Q-13` / `I-0~I-12` / `P-1~P-12` / `W1~W4` 各有不同含义，metis 实测撞车）⇒ 复用即制造「同一个 `I-1` 指两件事」的寻址歧义（remaining 册 §6.4 的 E-1 缺口形态）。
> **关联 ADR（架构锚点）**：`ADR-CEP-24`（**S1 · ✅ 已签发 2026-11-26**）· `ADR-CEP-25`（**S2 · 🟡 待起草**）· `ADR-CEP-26`（**S3 · 🟡 待起草**）
> **关联台账**：`obj-cep-platform.md` §11 **Entry #551**（ADR-CEP-24 落盘授权 · 五步链第 1 步）· **§11 = CEP 全域台账**（⛔ **不随任何 objective 归档封存** —— 该文件虽为 ⏸ 暂存归档，其 §11 仍是 append-only 的授权记录真源）

---

## §0 Goal

> **一句话**：让 CEP 从「**只运行既有 8 个协议实例**」走向「**用户可自定义协议**」—— 用户以**数据声明**（稳定态协议 SSP）描述协议，CEP 提供通用解释器执行之；在条件具备时，进一步支持**从 SSP 生成瞬态态 + 在飞事务**，并让协议语义层**真实穿过 Router + flit 产业链路**。

### 0.1 DoD（**分层** —— 必达 / 条件达成）

| 层 | 判据 | 落点 |
|---|---|---|
| **必达 · S1** | 用户可在**不写 C++** 的前提下定义一份稳定态协议；该协议能**装配**、能**实验**（`ADR-CEP-06` 4 件套）；其数据面与既有手写 `*_state_spec.hh` **逐格对账相等**（**V-24-2 killer 判据**）| `ADR-CEP-24` §6 |
| **必达 · S1** | **`ADR-CEP-24` §8 F5 全部闭合** —— ✅ 事件字母表（`CP-Q-1a` 2026-11-26）+ ✅ 键子集/`family`/`protocol_version`（`CP-Q-1b` 2026-11-26）| `.rddf/reviews/cep-CP-Q-1a-*.md` · `cep-CP-Q-1b-*.md` · 台账 Entry #552 / #554 |
| **条件达成 · S2** | 引擎 egress 消息**真实穿过** `CohIfNICTLM` + `RouterTLM`（`last_recv_hops ≥ 1`，**且须声明档位**）；协议语义层 hop / 时序**可声明**并被观测 | `ADR-CEP-25`（**待起草**）|
| **条件达成 · S3** | `SSP → 插件`生成器可产出**带瞬态态 + 在飞事务**的 CEP 插件；且 **MSD 消息停滞检测**缺件已补 | `ADR-CEP-26`（**待起草**）|

> ⚠️ **「条件达成」的机判载体（⛔ 不得只写散文 —— 否则 S1 一绿即宣称成功）**：S2 / S3 各须一条登记，载体 = **`test/cep_custom_protocol_deferrals.json` + `scripts/test/cep_custom_protocol_deferral_check.sh`**（**形态照抄** `test/cep_b3b2_deferrals.json` + `scripts/test/cep_b3b2_deferral_check.sh`，即 `obj-cep-platform` 的 **B3b-2 = 0/4 ⭭** 先例）。⇒ 每条含 **`status`** ∈ {`achieved`, `not_achieved(reason)`, `actively_not_promised`} + **复审触发条件**。⛔ **未登记 ⇒ 视为未达成**（⛔ 不得默认通过）。

### 0.2 ⛔ 本 objective 的**表述纪律**（硬）

- ⛔ **`CP-Q-1a` 闭合前**：任何「CEP 已支持用户自定义协议」的表述**均不成立**（`ADR-CEP-24` §Status Update ① 第 8 条）。
  > ✅ **解禁 YYYY-MM-DD 说明**：`CP-Q-1a` **裁定已达成**（2026-11-26）⇒ **本条禁令调整为「`CP-I-2` 交付前」**（形态已定 ≠ 能力已交付）⇒ ⛔ 仍不得在 `CP-I-2` 验收前宣称「已支持」。
  > ✅ **解禁 2026-11-26（条件 = `CP-Q-1a` · 证据 = `.rddf/reviews/cep-CP-Q-1a-adjudication.md` §6.1 + 台账 Entry #552）** —— ⛔ 原禁令行保留（审计链），本行只追加。
- ⛔ **S3 签发前**：任何「CEP 能自动生成瞬态态」的表述**均不成立**。
- ⛔ **S2 签发前**：任何「协议语义已接真实物理链路」的表述**均不成立**（含「mesh 上跑出 flit 活性」这类**档 0 冒充档 2** 的形态，per `ADR-CEP-20` 决策 6）。

> **解禁登记（⛔ 必须有人做，否则禁令永久化）**：禁令解除由 **planner** 在本节「追加式」登记 —— 每条禁令对应一个**解禁条件**（`CP-Q-1a` 裁定 / S2 签发 / S3 签发），达成后追加一行 `> ✅ 解禁 YYYY-MM-DD（条件 = <单元 id> · 证据 = <路径>）` + 台账 Entry。⛔ 不得删除原禁令行（保留审计链）。

---

## §1 Background

### 1.1 现状（可实测的事实）

| # | 事实 | 出处 |
|:--:|---|---|
| F1 | SSP 表达面**已在仓内**，但**逐协议手写 C++ 头**（6 份 `*_state_spec.hh`，各自定义 `State` / `Event` 枚举）⇒ 用户要自定义 = 写 C++ | `include/tlm/cep/*_state_spec.hh` |
| F2 | **无跨协议 canonical 事件词表**（事件集逐协议定义）| `cxl_state_spec.hh:69` · `mesif_protocol_engine.hh:189` |
| F3 | `RefKind` 三档对「用户自创协议」**无对应档** ⇒ 未登记 id fail-closed | `source_registry.hh:28-32,97-100` |
| F4 | 实例面 `properties` = 14（13 必含 + 1 可选）；`family` 枚举 **3** 值 + `kProtocolFamilyCount = 3` | `protocol_instance.schema.json` · `protocol_family.hh:53` |
| F5 | 协议语义层 hop / 时序 = **零**（J-3/J-4 反向存在性闸锁死）| `hop_timing_capabilities.hh:31` |
| F6 | **引擎不参与 mesh 业务路径**（mesh E2E 测的是 stub ↔ home 事务）| `docs/architecture/cep/fabric-transport.md §6.3.1` |
| F7 | 8 实例中**仅 `chi_rnf`** 有 egress + 在飞探针；6/8 零发射、3/8 无独立引擎头 | `engine_capabilities.hh:99-116` |
| F8 | **瞬态态合计 = 1**（仅 `moesi_gpu` 的 `T`）| gap 台账 §2.2 F1 |

> ⚠️ **行号漂移风险**：上表 `file:line` 无漂移守门（remaining 册 G-17 同型）⇒ 引用时**以 `grep` 当场复核为准**；本 objective **无** `cep_registry_baseline_check.py` 的等价物 ⇒ 顶栏事实只作**导航**，⛔ 不作判据。

### 1.2 核心矛盾

**「有机制、无入口」**：CEP 有协议引擎机制、有稳定态表达（6 份 state_spec）、有证据治理（55 闸 + 13 判据 + 3 档证据），但**这三者之间没有「用户自己写的协议」的那一层** ⇒ **自定义协议的门槛 = 写 C++ 插件**。

### 1.3 立项依据（触发条件 · 逐条可判定）

| # | 触发条件 | 实测 |
|:--:|---|---|
| 1 | 出现**第二个跨 sprint 复杂目标**（`roadmap.md` 的 objective 触发依据）| ✅ 本目标跨 S1/S2/S3 三段，各有独立 ADR |
| 2 | 既有 `obj-cep-platform` 已 ⏸ 暂存归档（`active_objectives: []`）⇒ 规划层为空 | ✅ `.rddf/state/.planner-handoff.json` |
| 3 | 用户明确要求「实现自定义一致性协议 … 并接真实物理链路仿真」 | ✅ 用户 2026-11-26 指令 |
| 4 | 架构前提 **部分** 具备：`ADR-CEP-24` 已签发（双审 0 BLOCKER），但 **`CP-Q-1a` 未裁 ⇒ S1 仍不可实施** | ⚠️ **部分**（⛔ 不写成「已具备」—— metis 指出该措辞会被单独引用）|

> ⚠️ **⛔ 本 objective 的正当性不是「`ADR-CEP-18` 的 T1 已触发」** —— T1 是**需求侧**判据（同族实例 > 8 且跨 ≥3 族），实测**未达**；本 objective 的正当性是「**用户自定义需求是 T1~T4 未覆盖的新需求类**」（`ADR-CEP-24` §1.3 + §8 F2）。⇒ `ADR-CEP-26` 的 amendment 须**显式 supersede** 台账 §5.1 的 T1~T4 复评结论。

---

## §2 范围

### 2.1 目标愿景

把「**用户想用 CEP 跑一个 CEP 还没内置的协议**」从「**改 C++ 源码 + 重编译**」变成「**填一份声明 + 写一个适配器（或由生成器产出）**」，且其结论**可与既有 8 实例的方法论对齐**（同闸、同判据、同档位纪律）。

### 2.2 In-Scope

- **S1（`ADR-CEP-24`）**：独立 SSP 声明面（数据）+ 通用解释器 + 手写 adapter/engine 接入路径 + `RefKind::UserDefined` 第 4 档 + 能力边界
- **S2（`ADR-CEP-25` 待起草）**：协议语义层 hop / 时序 + 引擎 egress 接 fabric（**须收敛到具体实例清单** —— 仅 `chi_rnf` 有 egress）
- **S3（`ADR-CEP-26` 待起草）**：SSP → 插件生成器（瞬态态 + 在飞事务）

### 2.3 Out-of-Scope

- ⛔ **具体某协议的语义建模** ⇒ ArchForge 单向门（`ADR-CEP-00` 决策 1 + `ADR-CEP-20:88`）
- ⛔ **HTM / 事务内存**（`ADR-CEP-17` 决策 5）· **token 权限携带**（决策 3 归插件内）
- ⛔ **6 份 `*_state_spec.hh` 的迁移**（`ADR-CEP-24` §8 F3）· ⛔ **并存期双写维护成本**（metis 指出：无队列项承接；**显式登记为已知代价**，per `ADR-CEP-24` 决策 6.2 的逐格 diff 闸）
- ⛔ **自动协议兼容性判定**（`ADR-CEP-13` 决策 5/6）
- ⛔ **补齐 4 个受限族运行时引擎**（`B3b-2 = 0/4` 属 `obj-cep-platform` 的既有残余，与本 objective 无关）

---

## §3 架构依据

| 段 | ADR | 状态 | 关键判据 |
|---|---|---|---|
| **S1** | `ADR-CEP-24` | ✅ **已签发**（2026-11-26 · metis 1 + oracle `APPROVE` + critic `APPROVE_WITH_CONDITIONS` · 0 BLOCKER）| V-24-2（往返等价 killer）· V-24-13（守护决策 3）· ✅ **F5 的 `CP-Q-1a` 已裁（2026-11-26）⇒ S1 可立项**；✅ **`CP-Q-1b` 亦已裁（2026-11-26）⇒ `ADR-CEP-24` §8 **F5 全部闭合**；批 2 可立项 |
| **S2** | `ADR-CEP-25` | 🟡 **待起草**（编号已预留）| 须处理与 **Entry #543** 的冲突 + ⭐ **J-3/J-4 现状锁翻转**（见 §8 R2）· 须收敛到具体实例清单 · 证据档位声明 |
| **S3** | `ADR-CEP-26` | 🟡 **待起草**（编号已预留）| 须以「新 ADR 收窄 + `ADR-CEP-18` 追加 SU **声明取代范围**」形态 amendment 决策 1/6；须 **supersede** T1~T4 复评；**T2 carve-out 原样继承**；前置层 **MSD 缺件** |

**继承的既有约束（⛔ 零新造）**：`CEN-Ib1` · `CEN-Ib11` · `INV-CEP-05-IC1` · `INV-CEP-07A-IC2`（例外集已扩至 4 类，见 `ADR-CEP-07A` SU 2026-11-26）· `INV-CEP-09-IC1` · `INV-CEP-12-IC1` · `ADR-CEP-13` 决策 5/6 · `ADR-CEP-17` 决策 3/5/6 · `ADR-CEP-20` 决策 5/6 · `ADR-CEP-18` 决策 1/4/5/6

---

## §4 Phases & Changes

### 4.0 排队总表（**权威** · 三队列形态，照抄 `obj-cep-platform-remaining` 册 §6.4）

> **排序纪律（critic SF-1 同型）**：每条排序边 = **一条可机判的「前置命中命令」**，且该命令**必须在「未达成」时返回「不命中」**（⛔ 恒真命令 = 零判别力，metis 已指出本文 v1 有三条恒真）。「是否触冻结面 / 是否占 KPI slot」**降为约束注解**，⛔ 不作排序依据。
> **并发（⚠️ 修正 metis 指出的错误表述）**：`.rddf/state/role-claims.json` 是 **per `(work_root, role)`** 的 claim（实测 general / builder / planner **同时 active**）⇒ **跨 role 可并行**。⇒ 本 objective 的并发规则 = ① **同 role 串行**（每 role 唯一 active claim）② **跨 role 可并行**（例：`CP-Q-1a`（architect）与 `CP-I-1`（builder）**可同时进行** —— 后者只依赖台账 Entry #551，不依赖任何裁决）③ **openspec change 并发 ≤ 1**（`docs/guide/cep-usage.md`）⇒ `CP-I-2/3/4` 三者串行。
> **载体表**：⚠️ 本 objective 的 `CP-P-*` 与 `CP-Q-*` 行**不**进 `scripts/test/rddf_ac_carrier.tsv`（该表是 **OpenSpec change 的 AC→载体** 映射，`CP-P/Q` 无 AC）⇒ **本条显式声明，⛔ 不静默复刻 remaining 册的「散文队列」缺陷**；`CP-I-*` 行**各随其 openspec change 的 AC 进该表**（anchor 纪律同 remaining 册 §6.4 队列 2 头注）。

#### 队列 0 · PROC（流程动作 · 零代码）

| 序 | 立项 | 完成判据（可机判） | 状态 |
|:--:|---|---|---|
| **CP-P-1** | 本 objective 落盘 + `roadmap.md` Objectives 段登记 | 本文件存在 · `grep -c "obj-cep-custom-protocol" .rddf/roadmap.md` ≥ 1 | ✅ 本次 |
| **CP-P-2** | `.planner-handoff.json` 的 stale `awaiting_builder` 清理 | `grep -c "cep-litmus-falsifier-naming-disambiguation" .rddf/state/.planner-handoff.json` == 0 | ✅ 本次 |

> ⚠️ **原 v1 的 `P-3` 已并入 `CP-I-1`**（metis 指出 P-3 是 I-1 的 PROC 镜像、判据以 I-1 为准 ⇒ 同一工作两行登记）。

#### 队列 1 · 决策（`needs` = architect / user）

| 序 | 立项 | 前置命中命令（**未达成 ⇒ 不命中**） | 完成判据（可机判） | 归属 |
|:--:|---|---|---|---|
| **CP-Q-1a** | ⭐ **事件字母表来源裁决**（**只此项阻塞 `CP-I-2`**）：**新建跨协议词表** vs **复用既有引擎事件集** —— 裁决须含**两案的表达力对比证据** | `ls .rddf/reviews/cep-CP-Q-1a-adjudication.md 2>/dev/null \| wc -l` == 0（**未产出 ⇒ 不命中**）| 裁决记录存在 + 含「来源 = 新建/复用 + 依据」+「表达力证据（谁产出、在哪）」| **architect** | ✅ **已闭合 2026-11-26**（用户 ratify R1~R4 · 新建 + 14 核心锚 + 扩权方 = architect · oracle `APPROVE` · ADR SU + Entry #552）|
| **CP-Q-1b** | **键子集 + `family` + `protocol_version` 裁决**（⚠️ **不阻塞 S1 开工** —— metis 指出四子项成本差一个数量级，打包会「最贵子项卡死全部」）：13 键划分 + `family` 是否加第 4 值（**三层同批**：schema enum + `protocol_family.hh:43-53` + `test_cep_protocol_bridge_contracts.cc`）+ `protocol_version` 与 `ADR-CEP-11` 的关系 | `ls .rddf/reviews/cep-CP-Q-1b-adjudication.md 2>/dev/null \| wc -l` == 0 | 裁决记录存在 + **若 `family` 不加值 ⇒ 须含「用户协议 `family` 填什么」的分支**（metis 边界点）+ 若加值 ⇒ 五步链成本已亮出 | **architect** | ✅ **已闭合 2026-11-26**（用户 ratify R1~R4〔R4 经 §7.3 澄清选「甲」〕· oracle 2 轮〔REJECT→APPROVE〕· critic `APPROVE_WITH_CONDITIONS` 0 BLOCKER · 3 份 ADR SU + Entry #554）|
| **CP-Q-2** | **`ADR-CEP-25` 起草**（hop / 时序 + egress 接 fabric）| `ls docs/adr/ADR-CEP-25-*.md 2>/dev/null \| wc -l` == 0 | ADR 双审闭合（报告含 `判定: APPROVE` 或 `APPROVE_WITH_CONDITIONS` 原文）+ `grep -c "已签发" ADR-CEP-25` ≥ 1 · ⭐ **须显式处置 ① Entry #543 冲突（见下「两候选」）② J-3/J-4 现状锁翻转 ③ 收敛到具体实例清单 ④ 证据档位声明** | **architect** |
| **CP-Q-3** | **`ADR-CEP-26` 起草**（SSP → 插件生成器）| `ls docs/adr/ADR-CEP-26-*.md 2>/dev/null \| wc -l` == 0 | 同上双审形态 + **须 amendment `ADR-CEP-18` 决策 1/6**（追加 SU 声明取代范围）+ **supersede** T1~T4 复评 + **MSD 缺件登记** | **architect** |
| **CP-Q-4** | （条件）若 `CP-Q-3` 判定「生成器路线不可行」⇒ **明确登记为主动不承诺** | `grep -c "actively_not_promised" test/cep_custom_protocol_deferrals.json` == 0 | 在 **`test/cep_custom_protocol_deferrals.json`** 的 S3 条目写 `status: actively_not_promised` + `reason`（⚠️ **落点 = 本 objective 自建的载体文件**，⛔ 不再悬空引用 remaining 册的 S5 节）| architect |

> **`CP-Q-2` 的 Entry #543 冲突 —— 两条候选（裁决须二选一，⛔ 不得含糊）**
> - **(a) 缩窄式**：S2 拆为「**时序声明面 / 管道**（机制，归 CppTLM）」+「**时序语义值**（协议 FSM 设计，归 ArchForge）」⇒ 本 ADR 只在 CppTLM 侧落**声明面与管道**，语义值走单向门（沿 `ADR-CEP-24` 决策 1.1 的二段式先例，⛔ 无需重新裁决 Entry #543）
> - **(b) 重裁式**：追加一条台账 Entry，**收窄 Entry #543 的 Out-of-Scope 边界**（从「补齐 hop/时序一律归 ArchForge」收窄为「语义设计归 ArchForge / 声明面与机制归 CppTLM」）⇒ 须用户参与（改已归档裁决的边界）
> - ⚠️ **⚠️ 两案都须补登**：**J-3/J-4 现状锁翻转** = 改写 `hop_timing_capabilities.hh:31` 的「任一行 ≠ `None` ⇒ 必红」为「须在登记集内」⇒ 该闸的**反向 fixture 全部要重写**（属 `CP-I-3` 的同批义务，见 §8 R2） + **五步链**（该文件**不在** `FROZEN_GLOBS`，但 `openspec/specs/cep-hop-timing-capability-declaration/spec.md` 是 ⇒ 改判据语义须改 spec ⇒ 走五步链）

#### 队列 2 · 实施（`needs` = builder · 逐项串行；`CP-I-1` 例外 —— 不占 change slot）

| 序 | 立项 | 前置命中命令（**未达成 ⇒ 不命中**） | 完成判据（可机判） | openspec change |
|:--:|---|---|---|---|
| **CP-I-1** | **`ADR-CEP-24` 落盘三件 + 两处 `test/` 登记 + 载体文本同步**（五步链 2~5）| `grep -c "^### Entry #551" .rddf/roadmap/objectives/obj-cep-platform.md` == 1 ✅ | `cep_freeze_check.sh` 无参 **rc=0**（96 产物）· `cep_adr_structural_check.sh` **4/4** · `cep_decision_block_check.py` 无「覆盖缺失」红（**27 份 / 152 块**）· 根 `AGENTS.md` 计数 **24** · `agents_md_lint --all` + `docs_sync_check --strict` 双绿 | ⛔ 无需 change（纯登记 + 落盘 + 文本）|
| **CP-I-2** | **S1 实施**：SSP 声明面（JSON + 同名 schema）+ **通用解释器** + loader + 闸（含 V-24-2 逐格 diff 闸）| ✅ **`CP-Q-1a` 已裁 2026-11-26**（裁决 §6.1 + Entry #552）| 必达 DoD §0.1 的两条 S1 判据全绿 | ✅ 需 change |
| **CP-I-3** | **S2 实施**（egress 接 fabric + 协议语义层 hop/时序 + **J-3/J-4 闸改写**）| **`CP-Q-2` 已签发**（`grep -c "已签发" docs/adr/ADR-CEP-25-*.md` ≥ 1）| `last_recv_hops ≥ 1` 端到端判据 + **档位声明落点 = 实验产物元数据（`ADR-CEP-20` 决策 6(i)）** + J-3/J-4 反向 fixture 重写后全绿 | ✅ 需 change |
| **CP-I-4** | **S3 实施**（生成器）| **`CP-Q-3` 已签发** **且** `grep -c "MSD" docs/adr/ADR-CEP-26-*.md` ≥ 1 **且** MSD 缺件已补 | 生成插件 ↔ 手写插件一致性判据（`ADR-CEP-14` 体系）| ✅ 需 change |

#### 工作流 `CP-W-1`（横切 · 可后置）

| 序 | 立项 | 完成判据 |
|:--:|---|---|
| **CP-W-1** | **载体文本同步剩余 3 项**（⚠️ 第 4 项「根 `AGENTS.md` 计数 24」**归 `CP-I-1`**，metis 指出双登记）：① `source_registry.hh:13` + `:27-28` 注释（三档 → 四档）② `include/tlm/cep/AGENTS.md:41` 铁律 2（三类 → 四类允许面）③ 决策 4 的 `cep_refkind_coverage` / `cep_basis_source_kind` 两 spec 文本（实施期义务）| 3 项逐条 `grep` 命中 · `agents_md_lint --all` + `docs_sync_check --strict` 双绿 |

### 4.1 ⚠️ 交付顺序 ≠ 依赖顺序

```
S1（声明面）──► S3（生成器）──► S2（时序）的「仿真价值」成立
```
⚠️ **瞬态态时长 = 等对端响应 = 链路往返时延 + 对端处理时间** ⇒ ⛔ 不建模链路时延 ⇒ 瞬态态零时长 ⇒ 在飞事务退化为计数器 ⇒ **S2 的时序语义是 S3 仿真价值成立的前置**（但 S3 的**算法**可先做）。⇒ 按「**先 S1（入口）→ 再 S2（时序）→ 后 S3（生成）**」排。

---

## §5 KPI 预算与治理

| 面 | 值 | 说明 |
|---|---|---|
| OpenSpec Proposed 并发 | **≤ 1**（本 objective 内）· 全仓 **≤ 3** | `docs/guide/cep-usage.md` |
| 本 objective 的 change 数 | **`CP-I-2` / `CP-I-3` / `CP-I-4` 共 3 个**（`CP-I-1` 与 `CP-W-1` 不占 slot）| 串行 |
| 冻结面 | `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` · `openspec/specs/cep-*` · `include/tlm/cep/*.schema.json` · `fabric.json` | 改动走五步链（`§12.6`）|
| 新增 invariant | **0**（`ADR-CEP-24` §5 零新造；S2/S3 的 ADR 须各自声明）| — |

---

## §6 验收（分层 · 逐行强制）

| # | 判据 | 当前 |
|:--:|---|---|
| 必达-1 | §0.1 的 S1 判据（数据面 + 装配 + 实验 + V-24-2 逐格对账）| ⏳ 待 `CP-I-2` |
| 必达-2 | `CP-Q-1a` 已裁（裁决记录存在）| ⏳ |
| **强制-3** | ⭐ **S2 状态已登记**（`test/cep_custom_protocol_deferrals.json` 的 S2 条目 `status` ∈ {`achieved`, `not_achieved(reason)`, `actively_not_promised`}）| ⏳ 待建载体文件 |
| **强制-4** | ⭐ **S3 状态已登记**（同上）| ⏳ |
| **强制-5** | ⭐ **终止/Pivot 的登记形态**：若 §8.2 触发，objective 状态写为 `terminated(<原因>)` 或 `pivoted(<收窄范围>)`，⛔ **不得**写「完成」 | ✅ 条款在 |
| 表述纪律 | §0.2 三条禁令 + 解禁登记 | ✅ 已登记 |

> ⚠️ **强制-3/4 是 DoD 分层「不开后门」的关键**：没有这两行，S1 一绿即可宣布 objective 成功（metis 指出的后门）⇒ 新增载体文件 `test/cep_custom_protocol_deferrals.json` + `scripts/test/cep_custom_protocol_deferral_check.sh`（形态照抄 `cep_b3b2_deferrals.json` + `cep_b3b2_deferral_check.sh`）**须在 `CP-I-1` 同批落**（零代码：纯登记 + 闸）。

---

## §7 Reviewers

| 角色 | 指定 | 职责 |
|---|---|---|
| **规划审查（本 objective）** | **metis**（✅ 1 轮已处置 —— 见下表）| 隐藏意图 / 需求歧义 / AI slop / 缺失 AC / 边界 |
| **平台验收方 / 消费者** | 同 `obj-cep-platform` §9.2 | ⚠️ **独立性缺口**（R10）**继承** |

### 7.1 metis 1 轮 findings 处置（**12 条 · 全落** · ⚠️ metis 输出是材料非判据）

| # | finding | 处置 |
|:--:|---|---|
| **P0-1** | `CP-I-1` 前置命令 `grep -c "Entry #551"` 实测 = **3**，写 `== 1` ⇒ **恒红** | ✅ 改用唯一锚点 `^### Entry #551` == 1 |
| **P0-2** | 编号与 `remaining` 册**全部**撞车（`Q-1~4` / `I-1~4` / `P-1~3` 均同号不同义）| ✅ 全部加 **`CP-`** 前缀 + 顶栏「编号空间」段 |
| **P0-3** | 「条件达成」**无机判载体** ⇒ S1 一绿即宣称成功（**后门**）；与 `B3b-2 ⭭` 先例**不同型**（后者有 `cep_b3b2_deferrals.json` + 闸）| ✅ §0.1 增机判载体段（`cep_custom_protocol_deferrals.json` + 闸，**`CP-I-1` 同批建**）+ §6 强制-3/4/5 |
| **P0-4** | 「实质串行」与机制不符：`role-claims` 是 **per `(work_root, role)`** ⇒ 跨 role 可并行 | ✅ §4.0 改三段式并发规则 + §10 增「可并行」列 |
| **P1-1** | `CP-Q-1` 打包四子项 ⇒ 最贵子项（`family` 三层 + 五步链）卡死全部 | ✅ **拆 `CP-Q-1a`（阻塞 S1）/ `CP-Q-1b`（不阻塞 S1）** |
| **P1-2** | `CP-Q-2` 只写「处置 Entry #543」不够；**漏记 J-3/J-4 现状锁翻转成本** | ✅ §4.0 增**两候选（缩窄式 / 重裁式）**+「两案都须补登 J-3/J-4」；§8 R2 改写为「双重冲突」；`CP-I-3` 判据列同批义务 |
| **P1-3** | `CP-Q-1~3` 的裁决记录**无载体** ⇒ 存在性不可机判 | ✅ 载体 = `.rddf/reviews/cep-CP-Q{1a,1b,2,3}-adjudication.md`；前置命令改 `ls ... \| wc -l == 0`（非恒真）|
| **P1-4** | `CP-Q-4` 悬空引用 remaining 册的「S5」节 | ✅ 落点改 `cep_custom_protocol_deferrals.json` 的 S3 条目 |
| **P1-5** | `CP-H3` 观测值来源未盘点：① 新 metric 键触 `metrics.schema.json`（**M1 冻结面**）② 复用 `last_recv_hops` = **档 0 冒充协议语义层** | ✅ §9 `CP-H3` 列两路 + 要求 `CP-Q-2` 裁决 + 禁冒充 |
| **S-1** | slop：`H1`/`H2` 复读 DoD | ✅ `CP-H1` 改真问题（表达力**在哪类协议触顶**）；`H2` 标注为 V-24-2 的独立复述；§9 顶注禁令 |
| **S-2** | §1.3 触发条件 4「✅ 架构前提已具备」过度陈述 | ✅ 改「**部分**具备…`CP-Q-1a` 未裁 ⇒ S1 仍不可实施」|
| **S-3~S-12** | Replan 无 baseline · 解禁无认领 · `P-3` 重复 · `W1/W-1` 编号不一 · 根 `AGENTS.md` 双登记 · 载体表未声明 · §11 未挂 backlog / 未声明全域台账 · Kill 无 ADR 侧善后 · 行号无守门 · 词表表达力证据无归属 | ✅ **全落**（§8.2 baseline 2026-11-26 · §0.2 解禁登记 · 删 `P-3` 并入 `CP-I-1` · 统一 `CP-W-1` · 计数归 `CP-I-1` · §4.0 载体表声明 · §11 补 backlog + 全域台账声明 · Kill 增「ADR 追加 SU」· §1.1 增行号漂移风险 · `CP-Q-1a` 增「表达力评估证据」）|

> **未采纳**：metis 建议「建编号空间**登记表**」⇒ 降级为 `CP-` 前缀约定（建表 = 新增需维护的登记面，正是 remaining 册 §6.5 要防的形态；`CP-` 前缀已足以消歧且 `grep` 可一眼识别）。

---

## §8 Risks & Kill Criteria

### 8.1 风险

| # | 风险 | 缓解 |
|:--:|---|---|
| **R1** | **`CP-Q-1a` 长期不裁** ⇒ S1 永久不可实施 | `CP-Q-1a` 是 S1 的**唯一入口**；⚠️ 但它**不阻塞** `CP-I-1`（跨 role 可并行）⇒ 排期风险已被 §4.0 的并行声明缓解 |
| **R2** | **S2 双重冲突**：① Entry #543（补 hop/时序归 ArchForge）② ⭐ **J-3/J-4 现状锁**（`hop_timing_capabilities.hh:31`「任一行 ≠ `None` ⇒ 必红」）⇒ 翻转 = **改写既有闸语义 + 重写全部反向 fixture + 改 spec（五步链）** | `CP-Q-2` 须**显式**处置两条（§4.0 的两候选 + J-3/J-4 成本）；该成本**已在 `CP-I-3` 判据中列为同批义务** |
| **R3** | **S3 的前置层 MSD 缺件** | `CP-Q-3` 须登记；⛔ 缺位时**不立项** `CP-I-4`（判据含 `grep "MSD"`）|
| **R4** | **「烘焙」静返**：`SSP JSON → constexpr 表` 脚本绕过解释器 ⇒ 静返 `ADR-CEP-18` 决策 1 | `ADR-CEP-24` 决策 1.3 第五条排除 + V-24-5 |
| **R5** | **档 0 冒充档 2** | S2 的证据**档位声明**义务（`ADR-CEP-20` 决策 6）+ `CP-I-3` 判据点名落点 |
| **R6** | **表述超前**（S1 达成即宣称完整）| §0.2 三条禁令 + **解禁登记**（§0.2 末）+ §6 强制-3/4 |
| **R7** | **双登记面**：本 objective 与 `remaining` 册若登记同一工作项 | 编号前缀 `CP-`（顶栏「编号空间」段）+ §4.0 载体表声明 |

### 8.2 Kill / Pivot / Replan

| 触发 | 判据 | 处置 |
|---|---|---|
| **Kill** | `CP-Q-1a` 裁定「**事件字母表无法建立**」（两案均不可行）| **终止本 objective**：① objective 状态写 `terminated(event-alphabet-infeasible)` ② **`ADR-CEP-24` 追加 SU 声明本 objective 终止**（ADR 不可变 ⇒ 只能追加，metis 边界点）③ §0.2 禁令**永久化**（⛔ 不得解禁）④ 台账 Entry |
| **Pivot** | `CP-Q-3` 裁定「生成器路线不可行」| S3 转 `actively_not_promised`（`CP-Q-4`）⇒ objective **收窄为 S1 + S2**（⛔ 不是失败）|
| **Replan** | 任一 `CP-Q` 项**自 2026-11-26 起**延期 > 2 季度，或 openspec Proposed 并发连续 2 季度无法为本 objective 腾 slot | `deferral-rationale` Entry + 重排 §4.0 |

---

## §9 研究问题清单（**防止交付工具但不产出洞见**）

> ⚠️ **不得复读 DoD**（metis 指出 v1 的 H1/H2 是 §0.1 判据的同义改写）⇒ 下列每条是**实施期可能失败、失败会改变结论**的问题。

| # | 假设（**可证伪**）| 判据 | 观测面 / 档位 | 关联 |
|:--:|---|---|---|---|
| **CP-H1** | 数据面表达力**在哪类协议上会触顶**（6 份 state_spec 中哪些可被 SSP 完整表达、哪些不可）| 对 6 份逐一判定「可表达 / 部分 / 不可」并给出**触顶原因**（如 0xFF 空洞语义、`Basis` 五元组不可表达）| 契约级（逐格 diff + 人工判定）| `CP-I-2` |
| **CP-H2** | ⭐ 数据面与手写面**逐格等价**（V-24-2 的独立复述）| SSP 表达 MESIF ⇒ 与 `kTable` 逐格相等（含空洞 + `Basis`）· **双向** | 契约级（逐格 diff 闸）| `CP-I-2` |
| **CP-H3** | 协议语义层的 hop / 时序**可声明并被观测** | 声明值 == 观测值 | ⚠️ **观测面须先裁决（两条路，`CP-Q-2` 定）**：① **新增 metric 键**（`metrics.schema.json` 是 **M1 冻结面** ⇒ 五步链 + 预登记授权 Entry）② **复用载体层**（`last_recv_hops`（`cohif_nic.hh:103`）/ flit `hops`（`:233`））⇒ **须声明档位，⛔ 禁冒充协议语义层观测**（R5 同型）| `CP-I-3` |
| **CP-H4** | 引擎 egress 消息**真实穿过** Router + flit | `last_recv_hops ≥ 1` + 一条真实协议事务完成往返 + **档位声明**（`ADR-CEP-20` 决策 6(i)，落实验产物元数据）| 档 2 观测面 | `CP-I-3` |
| **CP-H5** | 生成器能产出与手写插件**行为等价**的瞬态态 | 生成 ↔ 手写的一致性判据（`ADR-CEP-14` 体系）| 契约级 | `CP-I-4` |

⚠️ **verdict 三态**（同 `obj-cep-platform` §9.1 ③）：✅ 成立 / ⏭ 未测量 / ❌ 不成立 ⇒ ⛔「实测 0 ≠ 未测量」。

---

## §10 Next Sprint（**并行声明**）

| 序 | 候选 | 前置 | 归属 | 可并行 |
|:--:|---|---|---|---|
| 1 | ~~**CP-Q-1a** / **CP-Q-1b**~~ ✅ **均已完成 2026-11-26** ⇒ **`ADR-CEP-24` §8 F5 全部闭合** ⇒ **下一入口 = 批 2 立项**（解释器 + 装配 + 掩码格 + 4 件套）| 无 | planner→builder | — |
| 2 | **CP-I-1**（ADR-CEP-24 落盘五步链 2~5 + 两处 test 登记 + 载体文本）| 台账 Entry #551 ✅ 已落 | builder | ✅ 与 #1 可并行 |
| 2b | **CP-I-2 立项**（S1 实施）| ✅ `CP-Q-1a` 已裁 | planner→builder | ⚠️ 受 openspec 并发 ≤ 1 |
| 3 | **CP-W-1**（载体文本剩余 3 项）| 无 | builder | ⚠️ 同 role 与 #2 串行（每 role 唯一 claim）|

---

## §11 台账（**指针** · ⛔ 不复述）

| 项 | 落点 |
|---|---|
| ADR-CEP-24 落盘授权（五步链第 1 步）| `obj-cep-platform.md` §11 **Entry #551** |
| **关联 change（未立项）** | `backlog: cep-user-defined-ssp-declaration`（`ADR-CEP-24` 元数据已登记；⚠️ **本 objective 的 `CP-I-2` 立项时须把它提升为 openspec change**）|
| ADR-CEP-24 审稿报告 | `.rddf/reviews/ADR-CEP-24-user-defined-ssp-declaration-review.md` |
| 编号登记 | `docs/adr/ADR-CEP-NUMBER-MAP.md` §2 决策 1 + SU 2026-11-26 |
| 例外集扩展留痕 | `docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md` SU 2026-11-26 |
| gap 台账（缺口事实）| `.rddf/roadmap/cep-gen-family-gap-closure.md` |
| 论文侧证据锚点 | `docs/research/oswald/paper-claims-index.md` |
| **S2/S3 状态登记载体（待建）** | `test/cep_custom_protocol_deferrals.json` + `scripts/test/cep_custom_protocol_deferral_check.sh`（`CP-I-1` 同批落）|

> **§11 = CEP 全域台账**：本 objective 的后续 Entry **一律追加到 `obj-cep-platform.md` §11**（该文件的 ⏸ 暂存归档语义**不封存 §11** —— §11 是 append-only 的授权记录真源，Entry #543 / #551 均落于归档后，形态一致）⇒ ⛔ 本文件不另立台账。
