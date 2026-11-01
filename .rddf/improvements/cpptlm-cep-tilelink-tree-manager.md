---
name: cpptlm-cep-tilelink-tree-manager
priority: P2
phase: phase-10
依赖:
  - ADR-CEP-11
  - ADR-CEP-04
  - ADR-CEP-12
  - ADR-CEP-09
  - ADR-CEP-17
  - ADR-CEP-02
  - ADR-CEP-05
状态: 📋 **草稿待批**(2026-11-01)—— 队列 #11;**两段式**:第 0 个 change(规约层)= `cpptlm-cep-tilelink-state-spec`(尚未建)+ 第 1 个 change(实现层)= `cpptlm-cep-tilelink-tree-manager`(尚未建)
created: 2026-11-01
---

# improvement: cpptlm-cep-tilelink-tree-manager

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **状态**: 📋 草稿(2026-11-01)· **两段式**(per 队列 #9 / #10 先例:先规约层、再实现层)
> **队列**: `obj-cep-platform §4.0` **队列 #11**(`cpptlm-cep-tilelink-tree-manager`,Stage 1 / **2029-Q2**)
> · **不新增队列行**(一个队列行可承载多个 change / capability,per 队列 #3 / #4 / #8 / #9 / #10 先例)
> **两段式拆分**(per 队列 #9 的 `cpptlm-cep-mesif-state-spec` → `cpptlm-cep-mesif-adapter`、
> 队列 #10 的 `cpptlm-cep-cxl-nonsymmetric-state-spec` → `cpptlm-cep-cxl-nonsymmetric` 先例):
> · **第 0 个 change** = `cpptlm-cep-tilelink-state-spec`(**规约层**:3 稳定态 + 4 事件类别 + 12 格转移表 + `perm` 3→3 投影 + 非对称结构断言)
> · **第 1 个 change** = `cpptlm-cep-tilelink-tree-manager`(**实现层**:`TileLinkCAdapter` + `TreeManager` tracker 行为实现 + 实例声明 + 装配)
> **为何必须拆**:平台**从未**表达过「**树形一致性**」—— `LookupResult::ESCALATE_UP` 与
> `LookupHazard::CHAIN` 是 **TileLink 专属语义**,但 3 个现有实现**无一**使用它们
> ⇒ 与队列 #9 / #10 的 G1 / G2 **同型缺口** ⇒ 不先立规约层,实现层必然**自推**
> (重蹈 #9 的「看似完整实则杜撰」覆辙)。
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1,per `obj §5.4`;当前实测 **0/3**)
> **估时**: 规约层 4 周 + 实现层 8 周(共 12 周;**串行** —— 实现层以规约层为前置)

---

## 架构依据

| 依据 | 来源 | 本 change 用到的决策要点 |
|---|---|---|
| `ADR-CEP-11` | §3 锁表(**2026-11-01 原地更新**,per 台账 Entry #135) | 锁定版本 = **`tilelink-c-1.7.1`**(SiFive;PDF sha256 `30c09b5f…a2dd`)= `pin=strong` ⇒ **版本锁定无缺口**;⚠️ §3 表「关注的语义子集」列已同步为「**3 稳定态**(`Nothing`/`Branch`/`Tip`)+ **4 类权限转移**(`Cap`/`Grow`/`Prune`/`Report`)」⇒ 载体形态**已被锁表背书** |
| `ADR-CEP-11` 决策 5 | 「**不**追外部规范的**最新**版本 \| CEP 锁"**可实现的那一版**"」 | 1.7.1 **不是**最新(1.9.3 是)⇒ 本裁决**恰好落在允许范围内** ⇒ 理由**不得**写成「选最新」 |
| `ADR-CEP-04` | §2 caps 表(`TreeManager` 行 = `SUBTREE`/`EXACT`/`LOCAL`/`TREE_PARENT`)· §4 hazard 表(`TreeManager` = `Hazard_CHAIN`)· §5 lookup 同步性(`ESCALATE_UP` 时返回 `LookupResult_ESCALATE_UP + tree_parent`,由 **caller** 负责向上再发起) | `TreeManager` 是 **TileLink 专用** tracker ⇒ 队列 #11 的**实现层**须落地其**行为实现**(第 4 种行为类)⇒ ⚠️ `LocalView` 现只有 `sharers`/`dirty`/`forward_to`/`valid` **4 字段**、**无 `tree_parent`** ⇒ 实现层需**评估**是否扩字段(⚠️ `LocalView` 是**公开契约** ⇒ 改动面须登记) |
| `ADR-CEP-09` | `## Status Update(2026-11-01,per 台账 Entry #138)`(R-T2 裁决) | TileLink-C 的 **5 通道 → 4 VC** 映射 = `request`→0 · `snoop`→1 · `snoop_resp`→2 · `response`→3(**D∪E 合并**);⚠️ **3 条降维代价已登记**(优先级 5→4 / D 与 E 方向相反共 VC / `compute_noc` 边表对 TileLink 是**保守近似**)⇒ 规约层**不重复论证**,只引用 |
| `ADR-CEP-12` | `## Status Update(2026-11-01,per 台账 Entry #138)`(R-T3 裁决) | 逐消息 → 6 类归属表已登记;⚠️ **2 处降维**(`Release`/`ReleaseData`→`snoop_resp` · `GrantAck`→`response`)⇒ 规约层**不重推** |
| `ADR-CEP-17` | `## Status Update(2026-11-01,per 台账 Entry #138)`(R-T6 裁决) | TileLink-C 的「非对称」维度 = 「**权限的授予与强制撤销只由 manager 发起**」⇒ ⚠️ **与 CXL 形态不可互相套用**(CXL = request 方向 / TileLink = 权限变更发起方) |
| `ADR-CEP-02` | canonical 10 流 + `perm` 4 值锁(CEN-Ib11) | `perm` **值域**维持 4 值(不新增第 5 值);TileLink-C 的 10 流映射 = **5 true / 5 false**(载体侧只登记,归第 1 个 change 的实例声明) |
| `ADR-CEP-05` | `perm` 投影 + `INV-CEP-05-IC1` | **3→3** 投影(`Nothing→NONE` / `Branch→READ` / `Tip→WRITEBACK`);⚠️ `UNIQUE` **不可达** ⇒ 该实例**不声明**静默升级 ⇒ **不触发** `INV-CEP-08-IC2`(与 `chi_rnf`/`cxl_2_0` 形态**不同属合法差异**) |
| `ADR-CEP-08` | 决策 6 排序假设面边界 + `INV-CEP-08-IC2` | 排序假设是**协议侧声明**、逐网络 × 逐 `vc_id`;`IC2` 的触发条件 = 「声明 `UNIQUE` **且**声明静默升级」⇒ TileLink-C **两个条件都不满足** ⇒ **不触发** ⇒ 组合能力声明**不受约束** |
| 锚点摘要(唯一人工审计面) | `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md` | §1 稳定态 3 值 · §2 事件词表 4 类 · §2.3 逐格覆盖 **12 格零 `Unknown`** · §3 `perm` 3→3 · §6 非对称候选 · §9 钉版 sha256 ⇒ **basis 的唯一来源** |
| 台账 Entry #134 / #135 / #138 | Task 0 硬门 + 版本裁决 + 三项 architect 裁决 | 载体形态**已可推导**且**四项 architect 裁决全闭合** ⇒ **可起 change** |
| ArchForge | 实测**无 TileLink 状态机规约** | 与 #9 / #10 的 G1 / G2 **同型缺口** ⇒ **规约层先行**的必要性成立 |

---

## 范围

### In Scope

**第 0 个 change(`cpptlm-cep-tilelink-state-spec`,规约层)**:

1. **3 稳定态集合** —— `Nothing` / `Branch` / `Tip`;⚠️ `Trunk` **折叠**为 `Branch`,**不进**稳定态集合
   (**决策口径**,非规范断言:① `Table 8.1` 中 `Trunk`/`Branch` 权限**相同**(均仅 `Get`)
   ② `Table 8.3` 的 `Grow`/`Prune` 必须**单调** ⇒ `T` 唯一自洽读法 = `Tip` ③ 故折叠)
   ⇒ ⚠️ `Trunk` **本身存在**(D1 §8.1 的 **4 类持久节点之一**),**不得**写成「过渡态」;
   折叠的**语义损失 3 条**已登记(副本存在性 / 脏数据 / 树位置)
2. **4 类事件词表** —— `Cap` / `Grow` / `Prune` / `Report`(D1 §8.1.4 `Table 8.3` 的 4 个 Category)
3. **12 格转移表** —— 3 态 × 4 事件,**逐格零 `Unknown`**
   (9 个有序对被 `Grow`/`Prune`/`Report` 全覆盖 + `Cap` 的 3 条**源不定**通配转移)
4. **`perm` 3→3 投影** —— `Nothing→NONE` / `Branch→READ` / `Tip→WRITEBACK`
5. **非对称结构断言** —— TileLink 形态(「**权限变更的发起方**」),⚠️ **不得**套用 CXL 形态
6. **逐格 `basis` + 编译期锁**(per 队列 #10 的 `cxl_state_spec.hh` 同款载体形态)

**第 1 个 change(`cpptlm-cep-tilelink-tree-manager`,实现层)**:

7. `TileLinkCAdapter`(6→3 消息类映射 + 状态机驱动)
8. `TreeManager` tracker **行为实现**(第 4 种行为类;`Hazard_CHAIN` + `ESCALATE_UP` 语义)
9. `tilelink_c` 实例声明(13 键;`vc_binding` 按 R-T2 裁决填;`perm` 数组按 R-T4 裁决填)
10. 装配期校验接入 + 判据

### Out Scope

- ❌ **不动 `perm` 4 值锁**(CEN-Ib11)—— 值域不变,本协议族只是**用不到** `UNIQUE`
- ❌ **不改 `CATEGORY_*` 3 类**词表(传输层分类)—— 与 6 类一致性消息类**不同层**
- ❌ **不改 `CohIfBundle`** / **不改冻结 schema** / **不新增 ABI**
- ❌ **不新增 invariant 编号数**(本 change 只**消费**既有 66 条)
- ❌ **不改 `ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17`** —— 三项裁决已于 Entry #138 签发,本 change 只**引用**
- ❌ **不产出任何性能结论**(规约层只锁结构)
- ❌ **不做对拍**(对拍基线属 `docs/architecture/14-coherence-parity-baseline.md` 的独立面)
- ⚠️ **R-T4 口径**(「`perm` 4 值锁 = 值域锁 还是 每实例用满 4 值」)—— **不在第 0 个 change 内**;
  规约层只锁**投影**(3→3),实例 `perm` 数组填 3 值还是 4 值**留到第 1 个 change 前**由 architect 裁决

---

## Why

### ① 平台缺的不是「又一个 adapter」,而是**第二种一致性拓扑**

CEP 的差异化命题是**跨协议对比**;已落地的 4 个实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0`)
**全部是中心化目录**语义(SharerTracker 在 home 侧维护 sharer set)。
TileLink-C 是**树形**语义(权限沿树递归、序列化点在 Tip)⇒ **不立它,平台只覆盖了一半的一致性世界**
⇒ 平台无法回答「树形 vs 中心化目录」的**延迟 / 复杂度 / 可扩展性**对比(台账 §0 Goal 的核心命题)。

### ② 现有抽象**已经为它留好了位置**,但**零使用**

`LookupResult::ESCALATE_UP`(树形向上递归)与 `LookupHazard::CHAIN`(parent chain 加入)是
`ADR-CEP-04` 为 TileLink **专设**的两个枚举值 ⇒ 但 3 个行为实现**一个都没用**
⇒ 平台**声明了**树形语义却**从未执行**过它 ⇒ 这与 R11 的「声明超前于实现」是**同类问题的镜像**
(那次是「声明有、实现无」;这次是「**枚举有、语义无**」)。

### ③ 为什么必须**先立规约层**(而不是直接写实现)

`ADR-CEP-04 §5` 已明载:DCOH 与 TileLink 会「**被迫对返回字段善意撒谎**」——
`lookup` 假设单次同步回答全局问题,但树形语义下它**做不到**。
⇒ 实现层若不先有规约,必然**自推**一套「TreeManager 该返回什么」的语义
⇒ 落回 #9 的老路(「看似完整实则杜撰」)。
⇒ **规约层的价值 = 把「TreeManager 的语义边界」先钉死**,让实现层只做**消费**。

### ④ 为什么这一版的**证据强度显著高于队列 #10**

队列 #10(CXL)全程**禁读规范正文**(ECA §3 + AI AUP)⇒ basis 全来自**公开非规范来源**
⇒ 68 格中 **42 格 `Unknown`**。
TileLink 是 **SiFive 明示的 free and open standard** ⇒ **规范正文可读**
⇒ basis **直接来自规范**(D1/D2/D3 三份 `pin=strong`,含 sha256)
⇒ **12 格全 Known、零 `Unknown`** ⇒ 载体质量**显著优于**先例。

---

## What Changes

### 第 0 个 change(`cpptlm-cep-tilelink-state-spec`,规约层)

- **新增载体** `include/tlm/cep/tilelink_state_spec.hh`(header-only)
  - 3 态 + 4 事件 + 12 格转移表(逐格 `basis` + **格形态 = `kind ∈ {Known, Unknown}` + 3 位目标掩码**
    ⇒ 分桶 **`single 5 / multi 5 / empty 2 / unknown 0`**;⚠️ 原稿此处抄自 CXL 模板的
    「三态格语义:Known / Invalid / Unknown」**已作废** —— 本载体**无 `Invalid` 语义**)
  - `perm` 3→3 投影 + 非对称结构断言(逐消息查通道)
  - 编译期锁:`kStateCount == 3` · `kEventCount == 4` · `kTransitionCellCount == 12`
    · `kUnknownCellCount == 0`
- **新增判据** `test/test_cep_tilelink_state_spec.cc`
  - 4 铁律:① 纯负向断言 SHALL 有正控 ② 每条判据 SHALL 有可执行反向 fixture 且**因正确原因红**
    ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据 ④ fixture 状态变迁后 SHALL 重新基线化
- **新增锚点 checklist 生成器**(per `scripts/test/generate_cxl_anchor_checklist.cc` 同款)
  - 交叉核对:载体每格的 `ref` SHALL 解析到锚点摘要 §9 的**真实条目号**
  - ⇒ 手改任一侧(载体或摘要)⇒ **必须因正确原因红**
- **产出 delta spec** `openspec/changes/cpptlm-cep-tilelink-state-spec/specs/cep-tilelink-state-spec/spec.md`
  - 1 Requirement + N 个 Scenario(逐条可勾选)
- **⚠️ 零冻结面改动** ⇒ 本 change 的冻结面 diff **必须为空**

### 第 1 个 change(`cpptlm-cep-tilelink-tree-manager`,实现层)

- **新增** `include/tlm/cep/tilelink_cache_adapter.hh`(`TileLinkCAdapter`;零自有表副本)
- **新增** `TreeManager` tracker 行为实现(第 4 种行为类)
  - ⚠️ `Hazard_CHAIN` + `ESCALATE_UP` 语义的**首次使用** ⇒ 须验证 `INV-CEP-04-IC2` 幂等契约
  - ⚠️ `LocalView` 是否需扩 `tree_parent` 字段 —— 公开契约变更 ⇒ **须单独登记**(不在本 change 内默认做)
- **新增** `tilelink_c` 实例声明(13 键)+ `perm_upgrades` 登记
  - ⚠️ `UNIQUE` 不可达 ⇒ **不**声明静默升级 ⇒ 该实例进入 `no_upgrade_instances` 派生集
    ⇒ ⚠️ **既有断言 `no_upgrade_instances.empty()` 必红** ⇒ 须改为「恰含 `tilelink_c`」
    (per 队列 #10 的同款口径变更先例)
  - ⚠️ 实例计数断言(`instances.size() == 4` ⇒ 5)· 07 号 §5.1.1 行数(`== 8` ⇒ 9)⇒ 连带更新
- **KPI slot**:第 0 个 change 占 1 个(0/3 → 1/3);第 1 个 change **串行**在其后

### 明确不做(两段式内均不做)

- ❌ **不碰 `ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17`**(Entry #138 已签发)
- ❌ **不碰 `ADR-CEP-11` §3 锁表**(Entry #135 已签发)
- ❌ **不改 `INV-CEP-08-IC2` 判据**(`UNIQUE` 不可达 ⇒ 天然不触发)
- ❌ **不扩 `LocalView`**(第 1 个 change 的**评估项**,非默认动作)

---

## Acceptance

### 第 0 个 change(规约层)

- [ ] **AC-1** 规约载体的稳定态集合 SHALL **恰为 3 个**(`Nothing` / `Branch` / `Tip`),
      且 `Trunk` **SHALL NOT** 出现在稳定态集合中(反向 fixture:加回 `Trunk` ⇒ **必须因正确原因红**)
- [ ] **AC-2** 事件词表 SHALL **恰为 4 类**(`Cap` / `Grow` / `Prune` / `Report`),
      且每类 SHALL 有 D1 §8.1.4 `Table 8.3` 的**枚举转移**支撑
- [ ] **AC-3** 转移矩阵 SHALL 为 **3 × 4 = 12 格**,且 **`Unknown` 格数 SHALL 为 0**
      (编译期锁 + 运行时断言**双证**;反向 fixture:把某格改成 `Unknown` ⇒ **必须因正确原因红**)
- [ ] **AC-4** `perm` 投影 SHALL **恰为 3 行**且逐行为 `Nothing→NONE` / `Branch→READ` / `Tip→WRITEBACK`;
      `UNIQUE` **SHALL NOT** 出现在投影结果中
- [ ] **AC-5** 非对称断言 SHALL 用 **TileLink 形态**(「`Grant`/`GrantData` **只**在 D 通道、
      `Probe` **只**在 B 通道」);⚠️ **SHALL NOT** 复用队列 #10 的 CXL 形态模板
      (反向 fixture:换成 CXL 形态 ⇒ **必须因正确原因红**)
- [ ] **AC-6** 逐格 `basis` SHALL 全部指向锚点摘要的**真实条目号**;
      载体与摘要任一侧被改 ⇒ **必须因正确原因红**(交叉核对生成器)
- [ ] **AC-7** `openspec validate cpptlm-cep-tilelink-state-spec --strict` rc=0
- [ ] **AC-8** **冻结面零 diff**(`git diff --stat -- docs/adr/ docs/architecture/ openspec/specs/` = 空)

### 第 1 个 change(实现层)

- [ ] **AC-9** `TileLinkCAdapter` SHALL 有**状态机驱动**的转换表(非静态查表)，
      且 `LookupResult::ESCALATE_UP` + `LookupHazard::CHAIN` SHALL **至少各有一处真实使用**
      (⇒ 证「枚举有、语义无」的缺口**已闭合**)
- [ ] **AC-10** `TreeManager` 的 caps SHALL 逐字段等于 `ADR-CEP-04 §2` 的 `TreeManager` 行
      (`SUBTREE`/`EXACT`/`LOCAL`/`TREE_PARENT`)+ hazard `CHAIN`
- [ ] **AC-11** `tilelink_c` 实例声明 SHALL 满足 13 键 schema;`vc_binding` SHALL 逐类等于
      R-T2 裁决(`request`→0 · `snoop`→1 · `snoop_resp`→2 · `response`→3);
      ⚠️ `writeback` **SHALL NOT** 出现在 `vc_binding`(它是 10 流名,不是消息类名)
- [ ] **AC-12** 全量 `ctest` **0 回归**;既有计数断言的连带更新 SHALL **逐条登记**(不得静默改)

### 跨两段式(治理)

- [ ] **AC-13** 每处活文档的 KPI baseline 表述 SHALL 与 `openspec list --json` 实测**一致**
      (建 change 时 **0/3 → 1/3**;归档时 **1/3 → 0/3**;⚠️ **7 处**同步,含 obj §5.3)
- [ ] **AC-14** 判据 SHALL 遵守 4 铁律(正控 / 反向 fixture 因正确原因红 /
      不用编译红充当 ctest 判据 / 状态变迁后重新基线化)

---

## Capabilities

| Capability | 载体 | 状态 |
|---|---|---|
| `cep-tilelink-state-spec` | `openspec/changes/cpptlm-cep-tilelink-state-spec/` ⇒ 归档后 `openspec/specs/cep-tilelink-state-spec/spec.md` | **待建**(第 0 个 change) |
| `cep-tilelink-tree-manager` | 第 1 个 change ⇒ 归档后 main spec | **待建** |

**新增 invariant 数 = 0**(本 change 只**消费**既有 66 条)。

---

## Impact

| 面 | 影响 |
|---|---|
| **`ADR-CEP-11` §3 锁表** | **零改动**(Entry #135 已签发;本 change 只**引用** `tilelink-c-1.7.1`) |
| **`ADR-CEP-09` / `ADR-CEP-12` / `ADR-CEP-17`** | **零改动**(Entry #138 已签发;本 change 只**引用**三项裁决) |
| **`ADR-CEP-02` / `ADR-CEP-04` / `ADR-CEP-05` / `ADR-CEP-08`** | **零改动** —— 本 change **只消费**其冻结契约 |
| **`perm` 4 值锁** | **零改动** —— 值域不变;TileLink-C 只是**用不到** `UNIQUE` |
| **`CATEGORY_*` 3 类** | **零改动** |
| **`protocol_instance.schema.json`** | **零改动**(第 1 个 change 填实例时,schema 已有 `tilelink-c-1.7.1` 的合法 pattern) |
| **`protocol_instances.json`** | 第 1 个 change **+1 实例**(4 → 5)⇒ ⚠️ 连带 3 处既有断言(`instances.size()` / `no_upgrade_instances` / 07 号行数)⇒ **须逐条登记** |
| **冻结面(M1,40 件)** | 第 0 个 change **零改动**;第 1 个 change 若涉 07 号行数 ⇒ **须两步授权** |
| **KPI 并发** | 第 0 个 change 占 1 个(0/3 → 1/3)⇒ 归档释放(→ 0/3)⇒ 第 1 个 change 再占 |
| **`LocalView` 公开契约** | ⚠️ 第 1 个 change 的**评估项** —— 若需扩 `tree_parent` ⇒ 公开契约变更 ⇒ **须单独登记**,非默认动作 |
| **跨仓** | **零**(不新增 network、不改 ArchForge 声明;R-T2 裁决已把通道→VC 映射压到既有 4 VC 内) |

---

## 维护记录

| 日期 | Entry | 内容 |
|---|---|---|
| 2026-11-01 | #134 | Task 0 硬门(公开来源普查)—— 🔵 有条件通过;载体形态**已可推导**(3 态 × 4 事件 = 12 格**零 `Unknown`**);8 项残余风险登记 |
| 2026-11-01 | #135 | **版本锁定** `tilelink-c-1.3` → **`tilelink-c-1.7.1`**(SiFive)—— `ADR-CEP-11 §3` 原地更新 3 处 + 增补块 + SU(冻结面两步授权)⇒ **R-T1 闭合** |
| 2026-11-01 | #136 | R11 的 V-2 独立面(`MoesiGpuProtocolEngine` + 补偿路径判据)—— 与本 change **无交集** |
| 2026-11-01 | #137 | R11 降级 🟡 中 → 🟢 低 —— 与本 change **无交集** |
| 2026-11-01 | #138 | **R-T2 / R-T3 / R-T6 三项 architect 裁决一次性签发**(通道→VC 映射 + 6 类归属 + 非对称定义形态)⇒ **Task 0 硬门通过** ⇒ 可起 change |
| 2026-11-01 | #139 | `FullMapDirectory` 反向缺口落位裁决 —— 与本 change **无交集**(但**同属** `TreeManager` 落位体系:该 tracker 的行为实现归本 change 的第 1 个 change) |
