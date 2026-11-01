# cpptlm-cep-tilelink-tree-manager: TileLink-C(TileLink Tree Manager)—— **实现层**(`TileLinkCAdapter` + `TreeManager` 行为实现 + 实例声明)

> **状态**: 🟡 **Proposed**(2026-11-02)· **队列**: `obj-cep-platform §4.0` **#11** 的**第 1 个 change**(实现层;第 0 个 = 规约层 `cpptlm-cep-tilelink-state-spec` **✅ 已完成归档**,per 台账 **Entry #143**)
> **口径裁决**: 台账 **Entry #134** / **#135** / **#138** / **#141** / **#143** + `ADR-CEP-11` / `ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17` / `ADR-CEP-02` 的 `## Status Update`
> **规约载体(basis 唯一来源)**: `include/tlm/cep/tilelink_state_spec.hh`(**已归档**的规约层产物,main spec = `openspec/specs/cep-tilelink-state-spec/spec.md`)
> **审计面**: `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`(§2.3bis = 12 格逐格 basis 核对表)
> **规划输入**: `.rddf/improvements/cpptlm-cep-tilelink-tree-manager.md`(2026-11-01,7 段模板,HARD-GATE 通过)
> **估时**: 8 周(实现层)= **2 周** adapter + tracker 行为实现 + **1 周** 实例声明 + 装配闸 + **2 周** 判据(正向 + 8 类反向 fixture)+ **1 周** AC-05 族无关化 + **1 周** 冻结面同步 + **1 周** 验收
> **KPI**: Stage 1 = **1 slot**;Proposed 并发 **0/3 → 1/3**
> **冻结面**: **2 件** M1 成员将在 Phase 3 被改 ⇒ **两步授权** —— `openspec/specs/cep-sharer-tracker-base/spec.md`(「其余 **3** 种实现的行为实现尚未落地」⇒ 落 `TreeManager` 后变「其余 **2** 种」)· `docs/architecture/07-coherence-exploration-platform.md` §11(SharerTracker **6** 实现:`3 / 6` ⇒ `4 / 6`)⇒ 须台账 Entry 授权 → `--update --authorized-by "Entry #N"` → commit → `--update` → commit
> ⚠️ **零改动**:`protocol_instance.schema.json`(**冻结**面)**本 change 零改动** —— `perm` 的 `items.enum` 是**值级**枚举(取 4 值中任意子集皆合法,`minItems:1`)⇒ 实例填 **3 值**不触 schema(per **R-T4 裁决** = 值域锁,台账 Entry #141)

---

## Why

### ① 平台缺的不是「又一个 adapter」,而是**第二种一致性拓扑**

CEP 的差异化命题是**跨协议对比**;已落地的 4 个实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0`)**全部是中心化目录**语义(SharerTracker 在 home 侧维护 sharer set)。
TileLink-C 是**树形**语义(权限沿树递归、序列化点在 Tip)⇒ **不立它,平台只覆盖了一半的一致性世界**
⇒ 平台无法回答「树形 vs 中心化目录」的**延迟 / 复杂度 / 可扩展性**对比(台账 §0 Goal 的核心命题)。

### ② 现有抽象**已为它留好位置**,但**零使用**

`LookupResult::ESCALATE_UP`(树形向上递归)与 `LookupHazard::CHAIN`(加入同一 in-flight snoop)
是 `ADR-CEP-04` 为 TileLink **专设**的两个枚举 ⇒ 但 3 个现有行为实现**一个都没用**
⇒ 平台**声明了**树形语义却**从未执行**过它 ⇒ 这与 R11 的「声明超前于实现」是**同类问题的镜像**
(那次「声明有、实现无」;这次「**枚举有、语义无**」)。

### ③ 规约层**已就位** ⇒ 实现层**不必自推**语义

第 0 个 change(规约层)**已归档**:12 格转移表(5/5/2/0 分桶)· 3→3 `perm` 投影 · TileLink 形态非对称断言 · 逐格 `basis` 全部 `direct`。
⇒ 本 change **只消费**载体(零自有表副本)⇒ 实现层**不再有「自推 `TreeManager` 该返回什么」的空间**
⇒ 这正是两段式拆分的收益兑现点。

### ④ 平台有两处**族硬编码**必须在本 change 一次性族无关化

| 面 | 事实 | 后果 |
|---|---|---|
| `AC-05` | `check_adapter_self_consistency` 硬编码 `e_write_target_state != 3 ⇒ FAIL`(3 = M 态)且**无 SKIP 分支** | TileLink **无 `E` 态** ⇒ 实现层**必然红** ⇒ 有「填假值换绿灯」的诱因 |
| `no_upgrade_instances.empty()` | TileLink **不暴露 `UNIQUE`** ⇒ 无静默升级 ⇒ 落入派生集 | 既有断言**必红** ⇒ 派生集**首次非空** ⇒ 该断言的**恒空转前提失效** |

⇒ 这两处**必须**在本 change 内**族无关化**并**逐条登记**,否则「加一个新协议族」这条路在本仓是**走不通的**。

---

## What Changes

| 载体 | 内容 |
|---|---|
| `include/tlm/cep/tilelink_c_adapter.hh`(新 · header-only) | `TileLinkCAdapter`:**零自有表副本**(委托 `TileLinkStateSpec`)· `perm_of` 委托载体 3→3 · **6→3 消息类映射表**(按 R-T3 逐消息归属表)· `kMappingIsIdentity = false` + 恒等登记 · **不注册** 23 ABI · **不进** `ModuleFactory`(照 4 个既有 adapter 同型) |
| `include/tlm/cep/sharer_tracker.hh`(改) | ① **新增** `class TreeManager final : public SharerTrackerIf, public SnoopMechanics`(**第 4 种行为类**)—— `lookup` 返 `LookupResult::ESCALATE_UP`(**全仓首次使用**)· contention 返 `LookupHazard::CHAIN`(**全仓首次使用**)② **新增** `SnoopMechanics::snoop_join_core`(**CHAIN 的公共机械** ⇒ 禁双实现)③ caps/behavior 取 `kTrackerTraits[3]`(**单源**,不另写字面量) |
| `include/tlm/cep/protocol_instances.json`(改) | **+1 实例** `tilelink_c`(13 键)· `perm` **3 值** `["NONE","READ","WRITEBACK"]` · `vc_binding` 逐类按 R-T2(`request`→0 · `snoop`→1 · `snoop_resp`→2 · `response`→3)· `sharer_tracker.impl = "TreeManager"` + caps 逐字段等于 traits · `repo_owned_instance_ids` **+`"tilelink_c"`**(本仓域 ⇒ ⑥ 镜像义务跳过、①~⑤ 校验覆盖) |
| `include/tlm/cep/adapter_schema_contracts.hh`(改) | **AC-05 族无关化** —— 参数化「唯一干净态 + 写事件 → 目标态」三元组 + 秩单调 + **该族无 unique-clean 态 ⇒ SKIP**(非 FAIL)⇒ ⚠️ **既有 4 族的判定 SHALL 逐条不变**(回归锁) |
| `include/tlm/cep/adapter_contracts_loader.hh`(改) | `fill_adapter_inputs<TileLinkCAdapter>` + 装载层登记 |
| `include/tlm/cep/perm_upgrades.json`(改) | **不加** `tilelink_c` 行(无 `UNIQUE` ⇒ 无静默升级)⇒ ⚠️ **`current_state` 自述须改**「4 个实例全部具备该能力 ⇒ 派生量恒为空」已**失实** ⇒ 派生集**首次非空** |
| `examples/cep_adapter_contracts.cc`(改) | 加 `fill_adapter_inputs<TileLinkCAdapter>` 一段(CLI 与测试 verdict **同源** ⇒ 禁双实现) |
| `test/test_cep_tilelink_adapter.cc`(新) | adapter 契约(零副本静态扫描 + 12 格 pass-through + 3→3 投影 + 6→3 映射 + 非对称形态)+ `TreeManager` 行为断言(`ESCALATE_UP` / `CHAIN` 真实使用)+ **8 类反向 fixture** |
| `test/test_cep_tilelink_instance_decl.cc`(新) | 实例 13 键齐备 + `perm` 3 值 + `vc_binding` 逐类 + `sharer_tracker` caps + `repo_owned_instance_ids` 登记 + `no_upgrade_instances` **恰含** `tilelink_c` |
| `scripts/test/cep_tilelink_adapter_reverse_verify.sh` + `.cc`(新) | 反向验证(**毒化 ⇒ 必红**;**干净 ⇒ 必绿**)—— 照 `cep_mesif_adapter_reverse_verify.sh` 同款 |
| `openspec/changes/cpptlm-cep-tilelink-tree-manager/`(新) | 本 change 4 件套 + delta spec(capability `cep-tilelink-tree-manager`) |

### 明确不做(本 change 内)

- ❌ **不**做 **protocol engine**(`TileLinkCProtocolEngine`)—— 规约层与实现层之间**只隔 adapter + tracker**;引擎属后续(照 `mesif` / `chi_rnf` 的「engine 与 adapter 分属不同 change」形态)
- ❌ **不**扩 `LocalView`(`tree_parent` 等)—— 公开契约变更 ⇒ **非**本 change 的默认动作(per improvement 草稿);本 change **复用 `forward_to`** 承载「向上递归的父节点」(见 `design.md` D-δ)
- ❌ **不**改 `perm` **值域**(4 值锁不变)· **不**改 `protocol_instance.schema.json`(**零改动**)· **不**改 6 类消息类词表 · **不**改 `CATEGORY_*` 3 类
- ❌ **不**改 `channel_dependency_edges`(**顶层**跨实例共享,按 mesif 派生 ⇒ 对 TileLink 是**保守近似**,已在 Entry #141 登记)
- ❌ **不**加 `tilelink_c` 的 `perm_upgrades` 行(无 `UNIQUE` ⇒ 无静默升级)
- ❌ **不**产出任何性能结论 · **不**做对拍 · **不**新增 ABI · **不**新增 invariant 编号数
- ❌ **不**改 `ADR-CEP-02` / `ADR-CEP-04` / `ADR-CEP-08` / `ADR-CEP-09` / `ADR-CEP-11` / `ADR-CEP-12` / `ADR-CEP-17` 的**正文** —— 裁决均**已签发**,本 change 只**消费**

---

## Acceptance

- [ ] **AC-1** `TileLinkCAdapter` SHALL **零自有表副本** —— 其**去注释后**文本 SHALL **不**含 `State::` / `Event::` / `Perm::` **任何字面量**,SHALL **不**含任何**表名**(`kTransitions` / `kPermProjection` / `kTable`)⇒ 本类在语法上**不可能**携带自己的表(反向 fixture:植入字面量 ⇒ 必红)
- [ ] **AC-2** `TileLinkCAdapter` 的 `perm_of` SHALL **委托** `TileLinkStateSpec::perm_of` ⇒ 投影 **3→3** 且 `UNIQUE` **不出现**
- [ ] **AC-3** `TileLinkCAdapter` 的 6→3 消息类映射 SHALL 逐条等于 **R-T3 归属表**(`Acquire`→`request` · `Probe`→`snoop` · `ProbeAck`/`ProbeAckData`/`Release`/`ReleaseData`→`snoop_resp` · `Grant`/`GrantData`/`ReleaseAck`/`GrantAck`→`response`)⇒ `kMappingIsIdentity = false` + 恒等/非恒等**理由必填**
- [ ] **AC-4** `TreeManager` 的 caps/behavior SHALL **逐字段**等于 `kTrackerTraits[3]`(`Scope::SUBTREE` · `Completeness::EXACT` · `Authority::LOCAL` · `AggregationAt::TREE_PARENT` + `LookupHazard::CHAIN`),且 SHALL **从 traits 表取值**(不另写字面量)
- [ ] **AC-5** `LookupResult::ESCALATE_UP` 与 `LookupHazard::CHAIN` SHALL **至少各有一处真实使用** ⇒ 证「枚举有、语义无」的缺口**已闭合**(反向 fixture:把 `ESCALATE_UP` 换成 `ANSWER_HERE` ⇒ 必红)
- [ ] **AC-6** `tilelink_c` 实例 SHALL 满足 **13 键** schema;`perm` SHALL 恰为 **3 值** `["NONE","READ","WRITEBACK"]`;`vc_binding` SHALL 逐类等于 **R-T2**(`request`→0 · `snoop`→1 · `snoop_resp`→2 · `response`→3);⚠️ `writeback` **SHALL NOT** 出现在 `vc_binding`(它是流名,不是消息类名)
- [ ] **AC-7** `tilelink_c` SHALL 登记进 `repo_owned_instance_ids` ⇒ ⑥ 镜像义务跳过、①~⑤ 校验**覆盖**
- [ ] **AC-8** `AC-05` SHALL **族无关化** —— 对「无 unique-clean 态」的协议族给 **SKIP**(非 FAIL);且 ⚠️ **既有 4 族的判定 SHALL 逐条不变**(回归锁)
- [ ] **AC-9** `no_upgrade_instances` 派生集 SHALL **恰含** `tilelink_c`(首次非空);⚠️ `perm_upgrades.json` 的 `current_state` 自述 SHALL 同步(「派生量恒为空」已失实)
- [ ] **AC-10** **冻结面 2 件** SHALL 经**两步授权**同步(`cep-sharer-tracker-base` spec 的「其余 3 种」→「其余 **2** 种」· `07 号` §11 的 `3 / 6` → `4 / 6`)
- [ ] **AC-11** 全量 `ctest` **0 回归**;既有计数断言(`instances.size() == 4` → 5 · `07 号 §5.1.1` 行数 `== 8` → 9 · `no_upgrade_instances.empty()`)的连带更新 SHALL **逐条登记**(不得静默改)
- [ ] **AC-12** 判据 SHALL 遵守 **4 铁律**(正控 / 反向 fixture 因正确原因红 / 不用编译红充当 ctest 判据 / 状态变迁后重新基线化)

---

## Capabilities

| Capability | 类型 | 状态 |
|---|---|---|
| `cep-tilelink-tree-manager` | 新增 | ⬜ 本 change 交付(归档后并入 `openspec/specs/cep-tilelink-tree-manager/spec.md`) |

**新增 invariant 数 = 0** —— 本 change 只**消费**既有 66 条。

---

## Impact

| 面 | 影响 |
|---|---|
| **`ADR-CEP-02` / `04` / `05` / `08` / `09` / `11` / `12` / `17`** | **零改动** —— 裁决均**已签发**,本 change 只**消费** |
| **`perm` 4 值锁** | **零改动** —— 值域不变;TileLink-C 只是**用不到** `UNIQUE` ⇒ 实例填 3 值 |
| **`protocol_instance.schema.json`** | **零改动**(★ **冻结**面 —— 本 change **不触**) |
| **`protocol_instances.json`** | **+1 实例**(4 → 5)· `repo_owned_instance_ids` +1 ⇒ ⚠️ 连带 3 处既有断言 ⇒ **须逐条登记** |
| **`perm_upgrades.json`** | **不加行**;⚠️ `current_state` 自述**失实** ⇒ 须改 |
| **`sharer_tracker.hh`** | **+1 行为类**(`TreeManager`)+ **+1 公共机械**(`snoop_join_core`)⇒ 行为面扩大 ⇒ **非**冻结面,但 `cep-sharer-tracker-base` main spec 的**实现数陈述**须同步(★ 冻结面 ⇒ 两步授权) |
| **`adapter_schema_contracts.hh`** | **AC-05 族无关化** ⇒ ⚠️ **放宽**既有族 ⇒ 须以**回归锁**保证 4 族判定逐条不变;该文件**非**冻结面 ⇒ 无需两步授权 |
| **冻结面(M1,40 件)** | **2 件**被改 ⇒ **两步授权**(`cep-sharer-tracker-base` spec · `07 号` §11) |
| **KPI 并发** | **0/3 → 1/3**(本 change 占 1 个)⇒ 归档释放(→ 0/3) |
| **`LocalView` 公开契约** | ⚠️ **零改动** —— 复用 `forward_to`(语义代价已登记,见 `design.md` D-δ) |
| **跨仓** | **零** —— 不新增 network、不改 ArchForge 声明;`tilelink_c` 登记为**本仓域**实例 ⇒ ⑥ 镜像义务跳过 |
