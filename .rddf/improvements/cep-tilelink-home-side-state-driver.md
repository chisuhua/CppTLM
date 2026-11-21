---
name: cep-tilelink-home-side-state-driver
priority: P2
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-20
  - ADR-CEP-04
  - ADR-CEP-12
状态: "🟡 **待立项**(2026-11-21 · planner 起草 · 架构裁决已落 = architect 裁决记录 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四(登记册 §6.4 队列 2 的 **I-7 决策 3** · 本单元 = **I-7b** sub-id)· 前置 = **I-7a**(`TreeManager::view_of`)落地 · 经 **metis 1 轮**(P0/P1/P2 逐条处置见 §审查轨迹)· ⏳ 待立项)"
created: 2026-11-21
---

# cep-tilelink-home-side-state-driver: TileLink-C home 侧状态驱动 + 档 2 第二族

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不改** `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界与 **TLE-05**(事件面只声明事件类别 / SHALL NOT 预设 from-state / grep 0 处 `TileLinkStateSpec::State` / transition matrix)· ⛔ **不产 4 件套 run** · ⛔ **新增 invariant 数 = 0** · ⛔ **不扩** `perm` 4 值锁 / canonical 10 流词表 · ⛔ **不宣称** TileLink-C 规范一致性(本组件是**home 侧仿真模型**)

## 架构依据

档 2 = 证据阶梯的**天花板档**(协议语义级,per `ADR-CEP-20` 决策 6 的三档表)。现行载体**只有 1 族**(`mesif`,engine 侧观测面)。

⛔ **现状缺口**(architect 2026-11-21 实测):
- `tilelink_c` 在 `test/cep_evidence_tiers.json` 的 `coverage_map.families` 已登记 `tier2_applicable = true`
  (理由:`TreeManager` 的 completeness = EXACT 且无广播兜底 ⇒ 可回答「没有其他 sharer」)—— **该理由仍成立**,⚠️ 但需补「**本子树内**」限定(见 What Changes #6);
- 而 **`TreeManager` 的构造点全仓仅 1 处**(`test/test_cep_cxl_dcoh_tracker.cc:279`,作 CHAIN 对照实现)—— `DcohSymmetric` 侧虽有循环驱动(`test/cep_consistency_completion_model.hh:228-254`),但那是 `DcohTracker` **不是** `TreeManager`
  ⇒ 档 2 判据所需的「写者获取独占写权限时目录里还有别人」**没有产生点**;
- 而 tilelink 引擎受 `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界行 + **TLE-05** 约束
  ⇒ 驱动**不能**放进引擎。

⇒ 缺口 = **缺一个 home 侧状态驱动组件**(消费 TileLink 消息流 → 维护树形 sharer 分布)。

**架构裁决已定**(per `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四):
I-7 拆 **I-7a**(=`TreeManager::view_of`,档 2 仍单族)与 **I-7b**(= 本单元)。

## 范围

- **In-Scope**:TileLink-C home 侧**消息簿记层**(输入消息流 → `TreeManager` 的 `set_sharer`/`clear_sharer` 调用 + 观测记录)· 驱动边界快照(观测点)· `test/cep_evidence_tiers.json` 新增 `tilelink_c` 档 2 条目 · `scripts/test/cep_e2e_reconcile_check.sh` 的 T4/T5(+ 新增 T8/T9)· `openspec/specs/cep-evidence-tier2-observation/spec.md` 的 delta(含 tracker 侧载体 requirement)· `docs/architecture/07-coherence-exploration-platform.md` 的档 2 载体表追加式 SU
- **Out-Scope**:⛔ 4 件套 run / `hypothesis_id` / verdict · ⛔ 版本维 / 权限维 / 标签维 · ⛔ `moesi_gpu` / `cxl_*` / `nvlink_4` / `chi_rnf` / `ualink_4` · ⛔ `include/tlm/cep/sharer_tracker.hh` · ⛔ `include/tlm/cep/tilelink_protocol_engine.hh` 的引擎状态面 · ⛔ `openspec/specs/cep-tilelink-runtime-engine/spec.md` · ⛔ I-7a 的 `view_of` 实现(独立交付)· ⛔ **TileLink-C 规范一致性**(本组件是 home 侧**仿真模型**,⛔ 不得宣称规范一致)

## Why

**不做的代价**:档 2 停在**单族单点** ⇒ 「协议语义级证据」在本仓实质上只是 `mesif` 一族的一次观测 ⇒ 扩族价值归零。

**为什么必须先有 I-7a**:档 2 的快照取法必须是 **const 纯查询**(`view_of`)⇒ `TreeManager` 当前**没有**该面 ⇒ I-7a 先落地。

**为什么不放进引擎**:TLE-05 是**已签批主契约**,**不重开** ⇒ 驱动不进引擎。⚠️ 注意论证边界:**这不等于「驱动不可能掌握源态」**——链路层消息(`Acquire` / `Probe` / `ProbeAck` / `Grant`)本身携带 param(toT/toB/toN)⇒ from-state 在**消息层**可得;排除引擎的理由是**契约约束**,不是技术必然。

**为什么不并进 I-7 原单元**:I-7 原改善稿假设「引擎已有 tracker 同步、只差两处观测落点」⇒ 实测**引擎既无状态也无 tracker 同步** ⇒ 原单元的真实缺口是「缺一个协议侧状态驱动组件」(体量差一个数量级)⇒ 拆 a/b 后 I-7a 可独立交付,本单元独立审批。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **home 侧消息簿记层** | 组件定位 = **消息簿记层**(被动:把输入流翻译为树表更新;⛔ **不自主发** `Probe`/`Grant`,不做超时与 transient 处理)⇒ 附**逐消息安装语义表**(10 条:`Acquire`/`Probe`/`ProbeAck`/`ProbeAckData`/`Release`/`ReleaseData`/`Grant`/`GrantData`/`ReleaseAck`/`GrantAck` → 对树表的动作);⚠️ `ProbeAck` = 客户端**保留**降级副本(`set_sharer(Branch)`)· `ProbeAckData` = 交回数据并放弃(`clear_sharer`)—— 两者**方向相反**,混淆 ⇒ 正控/负控互为反例 |
| 2 | **观测点 = 驱动边界** | 快照取 `TreeManager::view_of`(const 纯查询,I-7a 提供)⇒ ⛔ **不得**用 `lookup`(有副作用)⇒ ⛔ **不得**把观测点放进 `TreeManager` 内部(那会观测它自己维持的不变量 ⇒ 同义反复 ⇒ 必绿)⇒ 快照时点 = **安装前**视图 |
| 3 | **档 2 第二族条目** | `carrier_kind = "tracker-observation"` · `run_produced = false` · `has_verdict = false` · `tier = 2`;⚠️ `carrier_kind` **无取值域校验**(`cep-evidence-tiers/v1` 全仓只有 tag 字符串,**无 schema 文件**)⇒ 取值域须由新判据 AC-S4 封闭 |
| 4 | **读法措辞** | 「**两族 / 两种载体形态** / **单子树 / 单 tracker 实例** / **无通过态**」;⚠️ `cannot_claim` 必含两条:①「与 `mesif-observation` **不同型**(tracker 侧,无引擎态迁移)」②「**不覆盖 data-value 维**」(TileLink 侧 `Trunk` 折叠为 `Branch` ⇒ `dirty` 掩码丢失 ⇒ 该维**连负控形态都没有**;与 mesif 的「恒 0 但可注入陈旧副本」明确区别)⇒ ⛔ 禁「同强度」「全族达成」「检测力已证」 |
| 5 | **T4/T5 + 新增 T8/T9** | T4(字段齐备 + 载体文件存在 + `run_produced==false` + `has_verdict==false`)· T5a(禁肯定式通过态,**含否定语境豁免**)· T5b(禁夸大短语)· T5c(禁跨档 run 引用)· **T8** = `evidence_tiers` ↔ `coverage_map` 跨表一致(现无任何判据把两张表绑起来)· **T9** = `carrier_kind` 封闭词表 + 两值互异 |
| 6 | **「本子树内」限定** | `coverage_map.families.tilelink_c.reason` 追加「**本子树内**」限定(⛔ `ESCALATE_UP` 使兄弟/祖先子树的持有者在本 tracker 上**恒不可见**)· 新条目 `can_claim` **不继承**无限定版本 |

## Acceptance

- [ ] **AC-1(档 2 覆盖 1 族 → 2 族 · 机制面)**:`test/cep_evidence_tiers.json` 档 2 条目数 ≥ 2;`cep_e2e_reconcile_check.sh` T4/T5 全绿
- [ ] **AC-2(档 2 内容面 · T8/T9 + 决定性变异)**:T8/T9 全绿 **且** 3 条变异**按签名红**:① 新条目 `family` 改成 `moesi_gpu` ⇒ T8 红 ② `carrier_kind` 撞值成 `protocol-engine-observation` ⇒ T9 红 ③ 驱动头文件删掉 `set_sharer` 调用 ⇒ 载体内容判据红
- [ ] **AC-3(溯源 · 两级 + 反向 fixture)**:① 每条观测的 `others` 中每位都能映射到**输入流的某条消息 id**(经安装记录)② **反向 fixture**:**绕过驱动**直接 `set_sharer` 播种一位 ⇒ 该位必须被判 `provenance_broken` 且**不得出现在任何观测快照里**(后者同时证明观测点在驱动边界、不在 `TreeManager` 内部);⚠️ 「可溯源到驱动方 `set_sharer` 调用」单写是**由构造恒真**(同义反复,per `test/cep_consistency_completion_model.hh:268-270` 的既有登记)
- [ ] **AC-4(术语拆分 · 三个独立判据)**:① **正控** = 合法流 ⇒ 违例数 **0**(照 mesif 先例)② **非空洞见证** = 观测面确实记录过 `others ≠ 0` ③ **反 fixture**(对抗流)⇒ 违例数**精确 == 1**;⚠️ 「写 episode 完整」须给等式:`episode 键 = (addr, node, episode_id)`,`完整` = 该 episode 的记录数 == **期望值常量**(⛔ 不是 `> 0`);⚠️ 同一 episode 的重复 `Grant` / 重试须去重(否则精确计数变 2)
- [ ] **AC-5(判定行钉死 + 负向)**:判定行只取「**Tip 获取**」事件;**负向**:喂一条纯非 Tip 事件(`ProbeAck` / `Release` / `Report`)⇒ 判定行计数增量 == **0**(同流对照);⚠️ 若由 producer 自行决定产什么行 ⇒ 该判据退化为恒真 ⇒ 负向是唯一有牙齿的部分
- [ ] **AC-6(装配期硬断言 · 4 条 · 各配反 fixture + 正控)**:① `node` 未映射(`responder_mask_of == 0`)② `tips` 已含他人(多 Tip;⚠️ `set_sharer(Tip)` 只做 `|=`,「至多 1 个」是**约定不是判据**)③ 观测面关闭 / 空流 ④ 丢弃计数 > 0;每条 ⇒ 置 `observation_invalid` ⇒ 判「**不可判**」(⛔ 不得记 0);⚠️ 每条须有**同流正控**(否则「把一切判 invalid」也能过);⚠️ 现状 mesif 的三条是「node 已映射 / **level 合法** / tracker 非空」—— `TreeManager::set_sharer(addr, node, State)` **无 level 参数** ⇒ ⛔ 不可照抄
- [ ] **AC-7(规格主体变更)**:delta spec 须含 ① Requirement 主体改写(现主体是「CEP 的**协议级引擎**」,本单元**不是**引擎)或新增 Scenario「tracker 侧载体 SHALL …」② Scenario 名含 `tracker` ③ `openspec validate <change> --strict` rc=0;⚠️ 只加一行 JSON 字段**不构成**完成
- [ ] **AC-8(冻结面五步链)**:① `artifact_count` 前后**逐字节不变** ② `--update --authorized-by "Entry #N"` 恰好执行一次 ③ 清单**单独 commit** ④ 无参复验 rc=0
- [ ] **AC-9(文档读法 SU · append-only)**:在 `docs/architecture/07-coherence-exploration-platform.md` 追加 SU(既有 SU **逐字不改**,`git diff` 只增行)登记档 2 的两族 / 两载体形态;⚠️ 该 SU 须加入 reconcile 的 `DOC_UNITS` 且 T6 变异的定位随之更新 —— 否则**新 SU 完全不受 T6 保护**
- [ ] **AC-10(诚实边界 · 引用 T5a 豁免口径)**:新增产出物内 grep 禁 `检测力已证` / `全族达成` / `所有族` / `档 2 强于档 1` / `通过态`;⚠️ 须照 T5a 的**否定语境豁免**口径(裸 grep 会在 spec delta 的「无通过态」这类**被鼓励**的措辞上假红);「新增产出物」清单 = 驱动头 + 新测试 + `cep_evidence_tiers.json` 新增字段 + spec delta + 新 SU(五项)
- [ ] **AC-11(范围纪律 · 零越权)**:`git diff --name-only` 不含 `docs/adr/**` · `docs/architecture/**` **既有段落** · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh`(`EventObservation` 载体,第二族若复用它即侵入族 1 基底面)· `include/tlm/cep/tilelink_protocol_engine.hh` · `include/tlm/cep/tilelink_tree_manager.hh`(属 I-7a 写集)· `openspec/specs/cep-tilelink-runtime-engine/spec.md` · `test/test_cep_tier2_observation.cc`(族 1 零回归的实证)
- [ ] **AC-12(0 回归)**:ctest 全绿(⛔ 不写死数字)· 全部 `scripts/test/cep_*.sh` rc=0(数量以 `ls scripts/test/cep_*.sh | wc -l` 实测);⚠️ 判定时点 = **归档前**
- [ ] **AC-13(前置可机判)**:开工门 = `tilelink_tree_manager.hh` 中存在 `LocalView view_of(...) const` 且与 `FullMapDirectory::view_of`(`sharer_tracker.hh:681`)**同签名** ⇒ 编译期可用 + grep 命中
- [ ] **AC-14(单子树限定必须出现)**:新条目的 `evidence_scope` / `cannot_claim` 必含「本子树内」/「单 tracker 实例」措辞,且 `cannot_claim` 必含一条「不覆盖跨子树(`ESCALATE_UP` 之外的 sharer 不可见)」

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-evidence-tier2-observation` | **MODIFIED** | 档 2 观测面覆盖族从 `mesif` 扩到 `mesif` + `tilelink_c`;⚠️ ⭔ 仍不产 run / ⭔ 仍无 verdict / ⚠️ **两种载体形态**(engine 侧 vs tracker 侧)· ⚠️ Requirement 主体须改写(见 AC-7) |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | TileLink-C home 侧消息簿记层(header-only)· 档 2 观测测试 |
| **修改文件** | `test/cep_evidence_tiers.json`(+1 条目 + `coverage_map` reason 限定)· `scripts/test/cep_e2e_reconcile_check.sh`(+T8/T9)· `openspec/specs/cep-evidence-tier2-observation/spec.md`(**冻结面** ⇒ 五步链)· `docs/architecture/07-coherence-exploration-platform.md`(追加式 SU) |
| **零改动(硬查)** | 见 AC-11 清单 |
| **冻结面** | `openspec/specs/cep-evidence-tier2-observation/spec.md` 已在 `FROZEN_GLOBS` ⇒ 归档须走一次 `--update --authorized-by` |
| **KPI** | 本单元占 1 slot(⇒ 与 I-6 同时在窗口内时峰值 **2/3**)⇒ 归档后释放;⛔ 不写死 `0/3 → 1/3 → 0/3` |
| **风险** | ① 观测点误置 `TreeManager` 内部 ⇒ 同义反复假绿(⇒ AC-3 反向 fixture)② 未映射节点静默 no-op ⇒ 假绿(⇒ AC-6①)③ 多 Tip 不被结构性拒绝 ⇒ 目录不一致被读成 1 次违例(⇒ AC-6②)④ `ProbeAck` / `ProbeAckData` 方向混淆 ⇒ 违例被自己洗掉(⇒ What Changes #1)⑤ 读法被读成「同强度两族」(⇒ AC-10/AC-14) |
| **已知未定义面(登记不阻塞)** | ① `entries_` 无容量上限(长流下「观测完整」与「目录完整」不同步)② 同址 `CHAIN` in-flight 期间安装 sharer 的快照语义未定义(⇒ 交 I-7a 的 `view_of` AC 锁)③ 簿记层保真度**不在**本单元判据面(⛔ 不得宣称规范一致) |

## 审查轨迹

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|:--:|---|
| 1 | **architect** | **形态已裁**(2026-11-21) | 决策 3 = I-7 拆 a/b;Q-a = 具体类上补 `view_of`(⛔ 不动 `SharerTrackerIf`) |
| — | **oracle**(裁决记录内) | APPROVE | 处置已并入 §四 与附录 B |
| — | **critic**(裁决记录内) | APPROVE_WITH_CONDITIONS | 处置已并入 §四 与附录 B(TLE-05 逐字引用 / I-7a 口径 / sub-id 登记) |
| 1 | **metis** | **提出 P0 × 7 + P1 × 7 + P2 × 3** | ✅ **全部处置**:P0-1 组件定位 = **消息簿记层** + 逐消息安装语义表(What Changes #1)· P0-2 AC-3 重写为两级溯源 + 反向 fixture(AC-3)· P0-3 AC-6 重写为 4 条 + 各配正控(AC-6)· P0-4 加 AC-7 规格主体变更 + AC-8 冻结五步链 · P0-5 删「源不定 ⇒ 驱动不能进引擎」论证(Why 段边界澄清)· P0-6 旧单元取代关系 + `obj-cep-platform-remaining.md` S1-2 口径同步(⏳ 待用户 ratify 后与 H-4 合并执行)· P0-7 「单子树」写进 evidence_scope / cannot_claim + coverage_map reason(What Changes #6 + AC-14)· P1-8 AC-S3/S4 ⇒ T8/T9 + 3 条决定性变异(AC-2)· P1-9 AC-2 术语拆分 + episode 等式(AC-4)· P1-10 AC-5 加负向(AC-5)· P1-11 AC-8 照 T5a 豁免 + 5 项产出物清单(AC-10)· P1-12 AC-9 补齐零 diff(AC-11)· P1-13 AC-1 拆机制/内容两条(AC-1 + AC-2)· P1-14 依赖加 ADR-CEP-04 + I-7a(front-matter + AC-13)· P2-15/16/17 保真度降级 + 「唯一构造点」措辞 + 两条无界/未定义面登记(Impact) |
