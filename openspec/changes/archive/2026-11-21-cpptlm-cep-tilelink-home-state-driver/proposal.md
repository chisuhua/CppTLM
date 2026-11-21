# proposal: cpptlm-cep-tilelink-home-state-driver —— 档 2 第二族（`tilelink_c`）home 侧状态驱动

> **来源**：`.rddf/improvements/cep-tilelink-home-side-state-driver.md`（登记册 §6.4 队列 2 的 **I-7b**）
> **形态来源**：architect 裁决 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §四.4（I-7b 的**形态**）+ **`.rddf/reviews/cep-i7b-adjudication-2026-11-21.md`**（**实施形态** · Q-1 逐行 basis 纪律 + Q-2 契约面主体 · oracle APPROVE + critic APPROVE_WITH_CONDITIONS，**2 条条件 BLOCKER 复审已闭合**）· **用户 ratify**（2026-11-21）
> **前置**：✅ I-7a 已交付（`TreeManager::view_of`，commit `a2a4c7ae`）⇒ 单元 AC-13「前置可机判」**已满足**
> **⛔ 实施侧不得自决语义**（裁决 §二 已定纪律；⛔ 行内容归本 change 的 `design.md`，由 **C-1 ~ C-5** 锁死）

## Why

档 2（协议级）观测面**当前只有 1 族**（`mesif`，engine 侧）。要扩到第 2 族（`tilelink_c`），缺的不是断言而是**产生点**：

- `TreeManager` 的构造点全仓仅 1 处（`test/test_cep_cxl_dcoh_tracker.cc:279`，作 CHAIN 对照）⇒ 档 2 判据所需的「写者获取独占写权限时目录里还有别人」**没有产生点**；
- tilelink 引擎受 `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界行 + **TLE-05** 约束 ⇒ 驱动**不能**放进引擎（裁决 §四.4 + I-7b 交办包 §六）。

⇒ 缺口 = **缺一个 home 侧消息簿记层**（消息流 → 树形 sharer 分布 + 驱动边界观测）。

## What Changes（**6 项**）

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **home 侧消息簿记层**（新 `include/tlm/cep/tilelink_home_state_driver.hh` · header-only）| 组件定位 = **消息簿记层**（**被动**：把输入流翻译为树表更新;⛔ **不自主发** `Probe`/`Grant` · 不做超时与 transient 处理）⇒ 附**逐消息安装语义表**（**10 行** · 形态见 `design.md` §D-1）|
| 2 | **观测点 = 驱动边界** | 快照取 `TreeManager::view_of`（const 纯查询 · I-7a 提供）⇒ ⛔ **不得**用 `lookup`（有副作用）⇒ ⛔ **不得**把观测点放进 `TreeManager` 内部（同义反复 ⇒ 必绿）⇒ 快照时点 = **安装前** |
| 3 | **档 2 第二族条目** | `carrier_kind = "tracker-observation"` · `run_produced = false` · `has_verdict = false` · `tier = 2`；⚠️ `carrier_kind` **无取值域校验**（`cep-evidence-tiers/v1` 全仓只有 tag 字符串，**无 schema 文件**）⇒ 取值域由**新判据 T9** 封闭 |
| 4 | **读法措辞** | 「**两族 / 两种载体形态** / **单子树 / 单 tracker 实例** / **无通过态**」；⚠️ `cannot_claim` 必含两条：① 与 `mesif-observation` **不同型**（tracker 侧，无引擎态迁移）② **不覆盖 data-value 维**（TileLink 侧 `Trunk` 折叠为 `Branch` ⇒ `dirty` 掩码丢失）⇒ ⛔ 禁「同强度」「全族达成」「检测力已证」|
| 5 | **T4/T5 + 新增 T8/T9** | T8 = `evidence_tiers` ↔ `coverage_map` **双向**跨表一致（⚠️ **现无任何判据把两张表绑起来** ⇒ 真缺口）· T9 = `carrier_kind` **封闭词表** + 两值互异；⚠️ 变异基座 **+2 条** |
| 6 | **「本子树内」限定** | `coverage_map.families.tilelink_c.reason` 追加「**本子树内**」（⛔ `ESCALATE_UP` 使兄弟/祖先子树的持有者在本 tracker 上**恒不可见**）· 新条目 `can_claim` **不继承**无限定版本 |

## Out-of-Scope

⛔ 4 件套 run / `hypothesis_id` / verdict · ⛔ 版本维 / 权限维 / 标签维 · ⛔ `moesi_gpu` / `cxl_*` / `nvlink_4` / `chi_rnf` / `ualink_4` · ⛔ `include/tlm/cep/sharer_tracker.hh` · ⛔ `include/tlm/cep/protocol_engine_base.hh`（族 1 基底面 · `EventObservation` **不复用**）· ⛔ `include/tlm/cep/tilelink_protocol_engine.hh`（引擎状态面）· ⛔ `include/tlm/cep/tilelink_state_spec.hh`（规约层真源 · **只读消费**）· ⛔ `include/tlm/cep/tilelink_tree_manager.hh`（I-7a 写集）· ⛔ `openspec/specs/cep-tilelink-runtime-engine/spec.md` · ⛔ `test/test_cep_tier2_observation.cc`（族 1 零回归的实证）· ⛔ **TileLink-C 规范一致性**（本组件是 home 侧**仿真模型**）

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **⚠️ 本 change 有 delta spec** ⇒ **触冻结面五步链**（台账 Entry → 改产物 → commit A → `--update` → **清单单独 commit** → 无参复验）。

### 裁决判据（**逐条可机检** · 编号沿用裁决 §2.3）

- [ ] **AC-1（C-1 行宇宙锁）**：安装表**恰 10 行**、行序 == 消息枚举值、消息名集合 == `kProtocolMessageMap` 的 10 词表；⚠️ **反 fixture**：删 / 增 / 换序任一行 ⇒ 按签名红；驱动收到**表外消息**被静默吞掉 ⇒ 按签名红（须计入 unknown/invalid）
- [ ] **AC-2（C-2 basis ↔ 锚点一致性）**：逐行谓词 —— `Direct` ⇒ `event_ref` **且** `payload_ref` **均**已注册为 `RefKind::SpecClause`；`SelfDerived` ⇒ `event_ref` 为 SpecClause **且** `payload_ref` 为 `RepoDerived`；`Pending` ⇒ 动作恒 NoOp；⚠️ **反 fixture**：把任一行 basis 改掉而 ref 不变 ⇒ 按签名红；把某行 ref 指向**未注册 id** ⇒ 按签名红；把某行 note 锚到 **improvement 自身** ⇒ 按签名红（**循环自证**）
- [ ] **AC-3（C-3 Pending ⇒ fail-closed 锁）**：`Pending` 行 ⇒ 动作恒 NoOp **且** 消费出口把该消息计入「不可判」而**非**「零违例」；⚠️ **反 fixture**：把某行改 `Pending`，若该消息**仍产生树变更** ⇒ 按签名红
- [ ] **AC-4（C-4 决定性变异）**：影子拷贝改某行 basis / 删某行 ⇒ 按签名红 ⇒ 复原后 hash 一致
- [ ] **AC-5（C-5 越权消费锁）**：驱动头文件**不得**出现规约层映射表的自有字面量副本（只经规约层访问器消费）⇒ 静态扫描判红
- [ ] **AC-6（D-1 单调性反向 fixture）**：N 组 `Grant` → `Release` 配对 ⇒ 终态 `view_of(addr).sharers == 0` **精确**成立；⚠️ **反 fixture**：删驱动的清表调用 ⇒ 按签名红（⛔ 这抓的是「装错表」的**单调假阳**方向）
- [ ] **AC-7（D-2 方向混淆反 fixture）**：同一「保留 vs 放弃」对**单差异** ⇒ 终态 sharer 位**精确**在 / 不在 ⇒ 违例计数**精确** `== 1` vs `== 0`（⛔ 非 `> 0`）
- [ ] **AC-8（D-3 载荷折叠试金石）**：一条「放弃」载荷 ⇒ 清表生效；⚠️ 裸消息键实现（无载荷）在此**必红**
- [ ] **AC-9（构造流自检 · 源=Tip 收 Grant）**：源 = `Tip` 的节点再收到 `Grant` 类消息 ⇒ 依 `TLC-06`（源=Tip **无** `Grow` 转移 ⇒ 目标掩码**空集**）⇒ 该消息**必须落「不可判」**，⛔ **不得**被装表
- [ ] **AC-10（族 2 判据载体 · 三段分离）**：判定**全在 test 侧**（族 1 先例）· 正控（合法流 ⇒ 违例数 **0**）· 负控（对抗流 ⇒ 违例数**精确 == 1**）· 判定行只取「**Tip 获取**」事件 ⇒ 纯非 Tip 事件的判定行增量 == **0**
- [ ] **AC-11（溯源两级 + 反向 fixture）**：① 每条观测的「他人 sharer」中每位都能映射到**输入流的某条消息 id**（经安装台账）② **反向 fixture**：**绕过驱动**直接 `set_sharer` 播种一位 ⇒ 该位必须被判 `provenance_broken` 且**不得出现在任何观测快照里**
- [ ] **AC-12（装配期硬断言 · 4 条 · 各配反 fixture + 同流正控）**：① `node` 未映射（`responder_mask_of == 0`）② `tips` 已含他人（多 Tip）③ 观测面关闭 / 空流 ④ 丢弃计数 > 0；每条 ⇒ 判「**不可判**」（⛔ **不得**记 0）· ⚠️ `TreeManager::set_sharer(addr, node, State)` **无 level 参数** ⇒ ⛔ **不可照抄**族 1 的 level 前提
- [ ] **AC-13（观测面有界 · 默认关闭）**：观测面**默认关闭** ⇒ 零记录；缓冲有上限 ⇒ 溢出递增**丢弃计数** ⇒ 丢弃非零 ⇒ 判**不可判**
- [ ] **AC-14（档 2 条目 · T8/T9 + 决定性变异）**：新增 `tilelink_c` 档 2 条目（`carrier_kind = "tracker-observation"`）+ `coverage_map.families.tilelink_c.reason` 追加「**本子树内**」限定；T8 双向闭合 + T9 封闭词表全绿；⚠️ **3 条变异按签名红**：① 新条目 `family` 改成 `moesi_gpu` ⇒ T8 红 ② `carrier_kind` 撞值成 `protocol-engine-observation` ⇒ T9 红 ③ 驱动头文件删掉 `set_sharer` 调用 ⇒ 载体内容判据红
- [ ] **AC-15（规格主体变更 · delta）**：delta spec 含 ① Requirement 主体改写（**逐字取裁决 §3.1**）② 新 Scenario「tracker 侧载体 SHALL 在驱动边界产生 per-installation 观测」（**逐字取裁决 §3.2** · 含族 1 的四条 disciplines）③ 族 1 的 7 个 Scenario **逐字保留** ④ `openspec validate <change> --strict` rc=0；⚠️ **只加一行 JSON 字段不构成完成**
- [ ] **AC-16（冻结面五步链）**：① `artifact_count` 前后**逐字节不变** ② `--update --authorized-by "Entry #N"` 按链执行 ③ 清单**单独 commit** ④ 无参复验 rc=0；⚠️ 归档后 main spec diff 须**重新实测**（⛔ **不假设** `+8/−1`）
- [ ] **AC-17（文档读法 SU · append-only）**：`docs/architecture/07-coherence-exploration-platform.md` 追加 SU（既有 SU **逐字不改** · `git diff` 只增行）登记档 2 的两族 / 两载体形态；⚠️ 该 SU 须加入 reconcile 的 `DOC_UNITS` ⇒ **T6 变异的定位随之更新**（否则新 SU 完全不受 T6 保护）
- [ ] **AC-18（诚实边界 · 禁夸大）**：新增产出物内 grep 禁 `检测力已证` / `全族达成` / `所有族` / `档 2 强于档 1` / `通过态`；⚠️ 须照 T5a 的**否定语境豁免**口径（裸 grep 会在「无通过态」这类**被鼓励**的措辞上假红）；「新增产出物」清单 = 驱动头 + 新测试 + `cep_evidence_tiers.json` 新增字段 + spec delta + 新 SU（五项）
- [ ] **AC-19（范围纪律 · 零越权）**：`git diff --name-only` 不含 `docs/adr/**` · `docs/architecture/**` **既有段落** · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `include/tlm/cep/tilelink_protocol_engine.hh` · `include/tlm/cep/tilelink_tree_manager.hh` · `include/tlm/cep/tilelink_state_spec.hh` · `include/tlm/cep/source_registry.hh` · `include/tlm/cep/sources.json` · `openspec/specs/cep-tilelink-runtime-engine/spec.md` · `test/test_cep_tier2_observation.cc`
- [ ] **AC-20（0 回归）**：ctest 全绿（⛔ **不写死数字**）· 全部 `scripts/test/cep_*.sh` 逐个 rc=0 **且输出不含 `[SKIP]`**（⚠️ 须先确认 `build/bin/cpptlm_tests` 存在）· `bash scripts/test/rddf_stage_gate.sh --phase queue2` rc=0 ∧ `--self-test` rc=0（⚠️ ctest 注册的是**裸调用** ⇒ **不覆盖**队列语义）· `bash scripts/test/cep_e2e_reconcile_check.sh` rc=0
