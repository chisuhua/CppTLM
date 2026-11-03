# cpptlm-cep-nvlink-sharp — NVLink SHARP 实现层(载体 + 声明 + 判据)

> **状态**: 🟡 **Proposed**(2026-11-03)· **队列**: `obj-cep-platform §4.0` **#13** 第 1 个 change(**实现层**)
> **配套**: [`design.md`](design.md) · [`tasks.md`](tasks.md) · [`specs/cep-nvlink-sharp/spec.md`](specs/cep-nvlink-sharp/spec.md) · [`specs/cep-collective-plane-contract/spec.md`](specs/cep-collective-plane-contract/spec.md)
> **口径**: 台账 **Entry #186**(规约层归档 + 实现层移交)· **#185 / #184 / #183 / #182** · **#175**(规约层建档)· **#179 / #178**(ADR 归因 id 唯一化)· **#171**(Decision D)· **#169 / #170**(三则 ADR 定稿)
> **前置**: `ADR-CEP-01` / `02` / `03` **定稿签发 v1.0** ✅(Entry #169 / #170)· VC 空间裁决 ✅(Entry #171 · Decision D)· Task 0 公开来源普查 ✅(Entry #173)· **规约层已归档** ✅(Entry #186)
> **审计面**: `docs/research/2026-11-03-cep-nvlink-sharp-anchor-summary.md`(basis 唯一人工审计面)
> **估时**: **8 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 change = `cpptlm-cep-nvlink-sharp-state-spec`(**规约层**,✅ 已归档)· **本 change = 第 1 个 change(实现层)**

---

## Why

### ① 平台缺的是「coherence × collective × 网内归约」组合,而非又一个 adapter

已落地的 6 个协议实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c`)
**零个**覆盖「网内归约与 coherence 共存」⇒ 平台答不了「SHARP 在网内做归约时该区域的 coherence 可见性怎么办」
—— 而这正是 dGPU SoC「NVSwitch 内嵌 SHARP」的直接对应(per 07 号 §5.1.1 的 NVLink 行)。

### ② 集体面载体**已在、零使用** ⇒ 与 R11 同型的「声明超前于实现」

队列 #6 已交付 `CollectivePlaneIf`(7 方法,含 `group_register_region`)+ `NullCollectivePlane`
+ `LocalCollectivePlane` + `collective_planes.json`;但 `plane_binding` 全 null、`registered_regions` 空
⇒ **零使用** ⇒ 本 change 是它的**首个消费者**(与队列 #10 `SparseSnoopFilter`、队列 #12 `DcohSymmetric` 同型)。

### ③ `ADR-CEP-03 §3` 的 R1~R5 + A~D 已成文、**零行为面** ⇒ 纸面契约

规约层(已归档)只给了**纯函数判定**;**区域表运行时状态、tracker 行为、实例声明、装配期校验**全部缺位
⇒ 平台仍无法回答「归约发生时 coherence 侧到底发生什么」。

### ④ `BroadcastNull` 是**唯一**零目录边的选择(顺带闭合一条已登记缺口)

`ADR-CEP-01 §3.3` 抓取点 1 要求 Collective Plane → Directory 调用边**必须为零**;
`BroadcastNull`(`scope=NONE` / `completeness=EXACT(degenerate)` / `authority=NONE` / `aggregation_at=NONE`)
是语义上唯一贴切者 ⇒ 本 change **首次兑现**其行为面(traits 行早已登记)⇒ 闭合 07 号 §11 的
「`BroadcastNull` 仅声明层 traits」缺口。

### ⑤ ADR 已把 **3 条判据**指派给本层,载体不落地它们就一直**悬空**

`ADR-CEP-01` 附录 V3(barrier 分类 litmus)· `ADR-CEP-01` SU D.4 的 **V6-c**(`CP-03-VC-PRECONDITION`)·
`ADR-CEP-03` F.3 的 **partial failure 显式建模** ⇒ 三者的载体 owner 均为「队列 #13 **实现层**」
⇒ 悬空判据禁令(per ADR-CEP-01 附录)要求载体**必须**在此交付。

---

## What Changes

| 面 | 交付 |
|---|---|
| **载体 ①** | `include/tlm/cep/nvlink_adapter.hh`(新 · header-only · **独立头**)—— **零自有表副本** · **无 `transition_*`** · **不暴露**区域表类型 · **委托** `nvlink_sharp_state_spec.hh`(已归档规约载体,唯一真源) |
| **载体 ②** | `include/tlm/cep/nvlink_broadcast_null_tracker.hh`(新 · header-only)—— `BroadcastNull` **行为实现**;caps / behavior **逐字段取自 `kTrackerTraits[4]`**(不另写 caps 字面量);`scope=NONE` ⇒ 恒 `BROADCAST` + **零目录边** ⇒ 满足 `ADR-CEP-01 §3.3` 抓取点 1 |
| **载体 ③** | `include/tlm/cep/protocol_instances.json` —— **`nvlink_4` 实例声明**(schema `required` **实读** 13 键 · `protocol_version = nvlink-4.0` · `sharer_tracker.impl = BroadcastNull`)+ `repo_owned_instance_ids` **+1** |
| **载体 ④** | `include/tlm/cep/collective_planes.json` —— `plane_binding` 加 `nvlink_4` → **本地模型** + `registered_regions` 增 1 条(`mode = nc_drop_cacheable`,per R3 区域互斥)⇒ 集体面**首个消费者** |
| **载体 ⑤** | `include/tlm/cep/collective_plane_contract.hh` —— **V6-c 前提约束**(`CP-03-VC-PRECONDITION`)+ **CP-05 标识清单补 `collective_region_table`** + coherence 侧扫描集合扩展(**复用 `CP-05`,不新造 id**) |
| **判据** | 正向判据(每条**负向断言配正控**)+ **8 组反向 fixture**(毒化 ⇒ **必红**且归因 id 命中)+ 基线正控 + `ctest` 注册 + 装配期校验 |
| **文档** | 07 号 §5.1.1 NVLink 行状态列 + §11 两行计数(SharerTracker `5/6` → `6/6`;adapter `6 个` → `7 个`)· `collective_plane.hh` 的**失指 change 名**订正(§10 登记项) |
| **零改动** | `ADR-CEP-01` / `02` / `03` **正文** · canonical **10 流**清单 · `protocol_instance.schema.json` · `collective_planes.schema.json` · `sharer_tracker.hh` **接口面** · `nvlink_sharp_state_spec.hh`(**只读消费**) |

### 反向 fixture 形态

**「毒化 ⇒ 判据必红」= 鉴别力**(与队列 #11 修**误报**的「毒化 ⇒ 判据仍绿」形态**相反**)
⇒ **两组都须登记**,避免后人混用(per improvement §判据纪律)。

### 4 铁律继承(自队列 #12)

① 纯负向断言须配正控 ② 每条判据须有可执行反向 fixture 且**因正确原因红** ③ 编译期锁不得只以「编译红」充当 ctest 判据
④ fixture 状态变迁后须**重新基线化**(锚点**动态定位**,不得硬编码会迁移的 change 名 / 队列号)。

---

## Capabilities

| 能力 | 内容 | 载体 |
|---|---|---|
| **`cep-nvlink-sharp`** | NVLink SHARP 的**实现层**载体与判据(adapter + tracker + 实例声明 + 集体面绑定 + 装配期校验) | 本 change |
| **`cep-collective-plane-contract`**(delta) | 声明最小集**第四键** `vc_reservation_declaration` + 「空预留缺声明 ⇒ FAIL」场景 + **V6-c** 前提约束场景 + **CP-05** 标识清单扩展场景 —— ⚠️ 该 main spec **在 M1 冻结集内** ⇒ 归档时须走**两步授权** | 本 change(对已冻结 spec 的 delta) |

---

## Impact

| 面 | 影响 |
|---|---|
| **本仓实现** | 新增 **2 个头** + 改 **3 个文件**(`protocol_instances.json` / `collective_planes.json` / `collective_plane_contract.hh`)+ 1 处注释订正;**冻结面零 diff**(除归档时**已授权**的 main spec delta) |
| **平台能力** | 首次覆盖「coherence × collective × 网内归约」组合;集体面**首次被消费**;`BroadcastNull` 从「无消费者」⇒「有消费者」;**3 条悬空判据**(V3 / V6-c / partial failure)落载体 |
| **文档** | 07 号 §5.1.1 NVLink 行 + §11 两行计数;`collective_plane.hh` 失指 change 名 ⇒ 指向本 change |
| **KPI** | **0/3 → 1/3**(本 change 占 1 slot;规约层归档已释放前一个 slot) |
| **残余风险** | 公开来源受限 ⇒ 9 项 `pending` 语义**不得**被实现层推理填空(逐格 basis 在规约层载体)⇒ 本层只实现**已定稿**的结构性语义 |
| **可逆性** | 新增面 + 声明面新增条目 ⇒ 无既有实例行为变更 ⇒ 可整体回退(声明面回退 = 删 `nvlink_4` 条目 + 还原 `plane_binding` / `registered_regions`) |

---

## Out of Scope

- NVSwitch **硬件模型** / SHARP 的**硬件 opcode 与指令编码**(只写枚举名)
- **UALink INC**(队列 #14 —— 本 change 的 collective 规约层是其前置先例)
- `ADR-CEP-01` / `02` / `03` **正文改写**;canonical **10 流清单**;`protocol_instance.schema.json`;`collective_planes.schema.json`
- **reduce 算子族的新增**(唯一真源 `tlm::cep::ReduceOp`,恰 6 值 ⇒ 本层只**断言**基数,不新增枚举)
- 真实硬件的**性能 / 带宽**结论(本层只出功能正确性结论)
- **queue #14 之后的** collective 面扩展(如需真实 fabric 集合流量 ⇒ 先命中 `vc_reservation_declaration.revisit_trigger`)

---

## Acceptance

- [ ] **载体形态定稿**:`nvlink_adapter.hh` 零自有表副本(静态扫描 + 鉴别力自测)· 无 `transition_*` · 不暴露区域表类型;`nvlink_broadcast_null_tracker.hh` 的 caps **逐字段取自 `kTrackerTraits[4]`**
- [ ] **声明面**:`nvlink_4` **13 键齐备**(schema `required` 实读)· `protocol_version = nvlink-4.0` · `repo_owned_instance_ids` +1 · `collective_planes.json` `plane_binding` / `registered_regions` / `collective_vc_reservation`(空)/ `vc_reservation_declaration`(四字段)齐备
- [ ] **3 条悬空判据落载体**:V3(barrier 分类 litmus 双向)· V6-c(`CP-03-VC-PRECONDITION`)· partial failure 显式建模 ⇒ **归因 id 唯一登记**于 `design §7`
- [ ] **反向 fixture**:**8 组**毒化 ⇒ 必红 + 归因命中 + 基线正控 ⇒ **全过**
- [ ] **零改动核验**:`ADR-CEP-01/02/03` 正文 · 10 流清单 · 两个 schema · `sharer_tracker.hh` 接口面 · `nvlink_sharp_state_spec.hh` ⇒ **零 diff**
- [ ] **收口**:`ctest` 全绿 + 门禁 **7** 脚本 rc=0 + `openspec validate --strict` rc=0 + `format.sh --check` rc=0 + **冻结面两步授权**(`cep_freeze_check.sh --update --authorized-by "Entry #N"`)· 07 号 §5.1.1 / §11 同步 + 台账 Entry(归档记录)

---

## ⚠️ 与 improvement 草稿的**一处数字订正**

improvement(`cpptlm-cep-nvlink-sharp.md` §Acceptance)写「第 1 个 change 的反向 fixture:**5 组**」
⇒ **实测订正为 8 组**:ADR 指派给本层的义务共 **8 条**(每条一个独立归因 id)⇒ 铁律 ② 要求
**每条**判据各有**因正确原因红**的反向 fixture ⇒ **5 组不足以逐条归因**。
⇒ improvement 草稿随本 change 同步订正(台账 Entry 登记)。
