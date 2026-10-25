# cpptlm-cep-directory-ordering-plane: Directory & Ordering Plane(SharerTrackerIf + caps + FullMapDirectory + 装配闸)

> **状态**: 🟡 **Proposed**(2026-10-25 建目录;planner 阶段产出 proposal + design + 3 个 delta capability)
> **队列**: `obj-cep-platform §4.0` 队列 **#4**(Stage 1 / 2027-Q3)· 前序 #1 `cpptlm-cep-umbrella` / #2 `cpptlm-cep-poc-spike` / #3 `cpptlm-cep-stub-cache-core` 均已 ✅ archived(完成归档语义)→ **KPI 并发 0/3 → 本 change 占 1 slot**
> **形态**: **单 change 单 Phase**(与队列 #3 的「双 Phase」不同 —— 无「声誉必须先于实现」的时效性问题);估时 12 周标称 → 实际 4-6 周当量(队列 #2/#3 实测先例)
> **裁决(2026-10-25,DRI 四问)**:**范围切小**(SharerTrackerIf 单接口 + caps + D-5 + FullMapDirectory 唯一行为实现)/ **装配闸输入源** = `include/tlm/cep/protocol_instances.json` / **双挂**(instantiateAll + validate_topology 共享判据) / **S1-7 (β) 同批补入** INV-CEP-08-IC2
> **关联 ADR**: `ADR-CEP-04`(Sharer Tracker 抽象)+ `ADR-CEP-08`(Address Resolver + Ordering Point 三判据)+ `ADR-CEP-09 Decision A`(装配期通道依赖图无环检查)+ `ADR-CEP-12`(protocol_instance 声明 schema)+ `ADR-CEP-02`(canonical 10 流)+ `ADR-CEP-15`(CacheTLM 对齐)
> **跨仓权威(只读)**: ArchForge `32 号 §6.0.1/§6.0.2`(6 类词表 + per-network VC 矩阵)/ `22-nsa §0.5.4.1`(网络清单 NS-R2 单点权威)/ `32 号 §6.1/§6.2`(两个协议实例声明)
> **承接**: 队列 #3 **⏭(孤儿) M-4 行为侧** 已由队列 #3 交付(`cep-vc-mapping-contract`)⇒ **本 change 不再染指**;S1-2 / S1-3 / S1-4(owner 重指向,per Entry #61)+ S1-7 (β) + S1-1 ②建图层
> **planning 输入**: `.rddf/improvements/cpptlm-cep-directory-ordering-plane.md`(5 段草稿)

---

## Why

Stage 1 后续队列(#6 ~ #9 各自的 CXL/TileLink/CHI-SF/NVLink 协议族)都需要消费 **Sharer Tracker** 接口与 **Channel Dependency Graph** 装配闸 —— 这两样**至今仓内零实现/零落地**:

| 缺口 | 证据 |
|---|---|
| **SharerTrackerIf 无仓内形态** | `ADR-CEP-04 §3` 已把接口族(lookup 4 路返回 + 规范 intent 4 值 + snoop begin/fold 生命周期 + observe_snoop 被动记账)、caps descriptor 4 属性 struct、`INV-CEP-04-IC1/IC2/IC3` 全部定义,但 `grep -rn "SharerTrackerIf" include/ src/ test/ examples/` **零命中** |
| **协议层 VC 分配无装配闸** | `ADR-CEP-09 Decision A` 要求「顶点 = (协议实例, network, vc_id)」的**装配期**无环检查为**必过项**(`INV-CEP-09-IC1`,「不通过 = 配置 fail」),而 S3 spike 只是**单文件可比对**的判定 —— 一不连接 `instantiateAll` 装配管线,二不输出 verdict(S1-1 ②建图层的「verdict 逐 (network, vc)」**义务未消费**) |
| **J1/J2/J3 三判据无机器化** | `ADR-CEP-08 §2 决策 3` 的可迁移性三判据**无仓内实现**,「组合必查项的 E 态静默升级检查」(决策 4)也**只存在于 ADR 文本** |
| **AddressResolver 无 owner ADR** | `ADR-CEP-08 §1.3` 三子平面分工表只写概念区分,`AddressResolverIf` **无 ADR 授权源**(canonical 流 8 的**真正 ADR 依据在 `ADR-CEP-02 §CEN-Ib8`** 是「在 ProtocolEngine 内终结」)—— 故本 change 按 `ADR-CEP-04 老路径`「**spec 首次定义**」

**为什么在同一 change**:4 个都是 Stage 1 准入前置项,单个 J1~J3 判据和 SharerTracker caps 的耦合(per `ADR-CEP-08 决策 3` J2 明确「依赖 Sharer Tracker `completeness` 非 PARTIAL」)是同一个 change 的 capability 相邻。

---

## What Changes

### 新增

| 文件/capability | 作用 |
|---|---|
| `openspec/specs/cep-sharer-tracker-base/spec.md`(delta) | **capability `cep-sharer-tracker-base`**(9 Scenario) —— SharerTrackerIf 单接口 + caps descriptor 4 属性 + D-5 全量契约 + `FullMapDirectory` 唯一行为实现 + **5×4** mapping 表数据契约 |
| `openspec/specs/cep-ordering-point-checker/spec.md`(delta) | **capability `cep-ordering-point-checker`**(6 Scenario) —— AddressResolverIf 单接口 + J1~J3 静态判据 + INV-CEP-08-IC2 E 态静默升级检查项 + **S1-7 (β) 裁决登记** |
| `openspec/specs/cep-channel-dependency-gate/spec.md`(delta) | **capability `cep-channel-dependency-gate`**(7 Scenario) —— `(实例,net,vc)` 三元组顶点 / SKIP-PASS-FAIL 三态 / verdict 逐 (network,vc) / `protocol_instances.json` 输入源契约 |
| `include/tlm/cep/sharer_tracker.hh` | **header-only** SharerTrackerIf(`lookup` / `snoop_begin` / `snoop_fold` / `observe_snoop`)+ caps descriptor struct + `FullMapDirectory` 唯一行为实现;**不注册 23 ABI**、**不进 `ModuleFactory`**(testbench 定位,队列 #3 stub_cache 先例) |
| `include/tlm/cep/address_resolver.hh` | **header-only** AddressResolverIf **单方法** `resolve(pa) → home_node_id`;与 SharerTrackerIf **正交**(per `ADR-CEP-08 §1.3`) |
| `include/tlm/cep/channel_dependency_graph.hh` | **header-only** builder + per-`(network,vc)` verdict + DFS 三色环检测(算法迁 `cep_poc_spike_s3.cc:125-151`,**不复制硬编码镜像**) |
| `include/tlm/cep/protocol_instances.json` | 本仓机器可读的协议实例声明(**诞生即不冻结**,首个 implemented 实例跑过后按 S1-4 复核口径纳入) |

### 修改

| 文件 | 改什么 |
|------|--------|
| `src/core/module_factory.cc` | `instantiateAll()` 的守卫钩子(validateConfig 之后 / 实例创建之前;**final_config 不含 CEP 协议实例 section ⇒ SKIP**) |
| `scripts/CMakeLists.txt` `validate_topology` | target 追加一次 `channel_dependency_graph` 调用(共享同一判据文件,**不复制实现**) |
| `docs/architecture/07-coherence-exploration-platform.md`(:674) | SharerTracker 行的 change 引用队列 #3 → **#4**(冻结面成员,走两步授权) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 状态列 📋 → 🟡 · §4.2 承接列 · §5.2 KPI 0/3 → **1/3** · §11 Entry #62 |
| `.rddf/roadmap/phases/phase-10.md` | queue #4 对应子任务行状态 + KPI baseline 行 |
| `.rddf/roadmap.md` · `AGENTS.md` | KPI baseline 0/3 → 1/3 |
| `.rddf/improvements/cpptlm-cep-directory-ordering-plane.md` | front-matter `状态: Proposed` + 「最后」行 |
| **`docs/adr/ADR-CEP-00-platform-charter-and-governance.md`** | **Status Update 2026-10-25**:队列 #4 已启动(KPI 0/3 → 1/3)+ 上一段「当前态」→「当时态」 |
| **`docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md`** | **Status Update**:§8 的 4 个后续 change 名(`cpptlm-cep-sharer-tracker-base/-impls/-cxl-dcoh/-tilelink-tree`)失指,与 `obj §4.0` 11-change 算术冲突(队列 #3 D11 同构处置) |

### 冻结面

**3 个成员改动**(均**仅追加** `## Status Update` 段;走 `cep_freeze_check.sh --update --authorized-by "Entry #62"` 两步授权):

| 成员 | 追加内容 |
|------|---------|
| `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | Status Update 2026-10-25 —— 队列 #4 已启动 + 上一段「当前态」改「当时态」(数值保留,仅状态措辞) |
| `docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` | §8 的 4 个 change 名失指声明(与 obj §4.0 重排取代) |
| `docs/architecture/07-coherence-exploration-platform.md` | `:674` SharerTracker 行 change 引用修正 |

**零改动**: `ADR-CEP-05` · `ADR-CEP-08` · `ADR-CEP-09` · `ADR-CEP-12` · `protocol_instance.schema.json` · `fabric.{json,schema.json}` · 2 份既有 main spec · 其余 ADR-CEP 家族成员

---

## Capabilities

### New Capabilities

- **`cep-sharer-tracker-base`** —— Sharer Tracker 的**接口签名 + caps descriptor + D-5 全量契约 + FullMapDirectory 唯一行为实现**为可执行契约:① `lookup` 4 路返回枚举 + `hazard` 3 值 + `intent` 4 值 + 生命周期(begin/fold/observe)全机检;② caps 4 属性 struct **bitmask 64 bit**(spec **首次定义**,bit→node_id 显式映射);③ `5 实现 × 4 caps` mapping 表数据级自洽(**DEV-1-9 非 FullMap 显式 deferred**);④ **IC1/IC2/IC3** 不变量的 3 条行为断言;⑤ 禁用 API 静态扫描(`snoop_complete`/`subscribe_remote_snoop`/`is_distributed` 零出现)
- **`cep-ordering-point-checker`** —— **AddressResolverIf 单接口**(resolve(pa) → home_node_id,**spec 首次定义**)+ **J1/J2/J3** 装配期静态判据 + **INV-CEP-08-IC2** E 态静默升级检查项 + **S1-7 (β) 裁决登记**(含 J3 vacuous 双形态机器判据)
- **`cep-channel-dependency-gate`** —— **装配期通道依赖图**:顶点 = `(协议实例, network, vc_id)` 三元组(per `ADR-CEP-09 §8.2`)/ 边由声明导出(**不硬编码**)/ **SKIP-PASS-FAIL 三态**(per metis 的「无实例 config 不得 crash」)/ verdict 逐 `(network,vc)` 输出(机器可 grep)/ `protocol_instances.json` 输入源契约 + **moesi_gpu fixture 与 S3 spike 对账**

### Modified Capabilities

**无** —— 队列 #3 落成 `cep-vc-mapping-contract`(8 Scenario) 与 `cep-stub-cache-core`(9 Scenario) 的 main spec(2 份冻结 spec 零改动)。

### 契约元信息(delta spec 只保留 `## ADDED Requirements` 段内内容,以下登记在此)

| 项 | 内容 |
|---|---|
| **红绿语义** | 「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论登记为交付物(per 队列 #2/#3 先例) |
| **消费者** | `cep-sharer-tracker-base` → `test/test_cep_sharer_tracker_base.cc` · `cep-ordering-point-checker` → `test/test_cep_ordering_point_checker.cc` · `cep-channel-dependency-gate` → `test/test_cep_channel_dependency_gate.cc` |
| **Requirement **首个定义** 项 | `AddressResolverIf` 的 owner ADR = `ADR-CEP-02 CEN-Ib8`(「address_resolve 在 ProtocolEngine 内终结」);canonical 流 8 不跨 CEP 引擎边界;**接口形态本 change spec 首次定义** |
| **不可验证条款** | ① 非 FullMap 4 种 tracker(SparseSF/DcohSymmetric/TreeManager/BroadcastNull)的行为**只做声明层断言**(真实行为需首真实 instance)⇒ 归队列 #6~#9 ② J3 通用形式「迁移后两点不定出冲突序」**降级为文档判据**(`ADR-CEP-08 决策 7` 显式排除;HeteroGen §VI-E E1) |


---

## Impact

### 影响范围

| 层 | 影响 |
|---|---|
| **新增头文件** | `include/tlm/cep/{sharer_tracker,address_resolver,channel_dependency_graph}.hh` —— **header-only**,不进 `ModuleFactory`、不注册 ABI |
| **新增 JSON** | `include/tlm/cep/protocol_instances.json`(**不冻结**,per `Entry #56` 先例) |
| **钩子** | `src/core/module_factory.cc` 守卫钩子;`scripts/CMakeLists.txt` `validate_topology` target 追加 |
| **测试** | 3 个新 test 文件(**22 Scenario**,锚点均指向 3 个 test 文件) |
| **冻结面** | **3 个成员**(仅追加 Status Update 段) |
| **KPI** | **0/3 → 1/3**(本 change 建目录即占用 1 slot) |
| **23 ABI / frozen headers** | **零改动** |

### Non-Goals

1. **不实现** `SparseSnoopFilter / DcohSymmetric / TreeManager / BroadcastNull` 其他 4 种 tracker 实现(消费者在队列 #6~#9 的各自协议族,per `ADR-CEP-04 §8` / `obj §4.0` 排队表)
2. **不做** runtime enforcement / runtime 死锁检测(per `ADR-CEP-09 Decision A` 先于 Decision B + 决策 7 自动判定组合可行性排除)
3. **不做** `snoop_complete` / `lookup()→sharer_bitmap` 平铺返回 / `subscribe_remote_snoop` / `is_distributed` bool flag —— per `ADR-CEP-04 §3.6` 禁止的旧 API + `INV-CEP-04-IC1` 字面值禁令
4. **不做** `AddressResolverIf` 进 SharerTrackerIf(「地址归属解析 = 树遍历 / tracker = per-node 状态」per `ADR-CEP-04 §1.3`;合并会强制 tracker 感知拓扑)
5. **不反向改** `cep-stub-cache-core.spec.md`(队列 #3 已冻结名字;S1-7 (β) 的裁决**只登记**本 change 的 design.md 而不落 main spec)
6. **不执行**「自动判定组合可行性」 —— per `ADR-CEP-08 决策 7`(HeteroGen §VI-E, E1: 学术侧未解决)
7. **不**改 `ADR-CEP-09` / `ADR-CEP-08` / `ADR-CEP-12` 正文(都是**冻结面成员**,只追加 Status Update)
8. **不**落 D-5 重构中 `ESCALATE_UP` / `FORWARD_TO` 的**行为断言**(FullMapDirectory 最多只会返回 `ANSWER_HERE` 和 `FORWARD_TO` 中 **ESCALATE_UP = 不可能**;断言「5×4 mapping 表数据级自洽」已够)

### 风险与缓解

| 风险 | 缓解 |
|------|------|
| **22 Scenario 过度承诺**(非 FullMap 的行为断言无处实证) | A-1-9 / A-2-6 显式降级登记(per 队列 #2 先例「不可验证条款不进 spec」) |
| **装配闸「离线脚本化」** | A-2-* 走**真 `instantiateAll()`** 驱动 + A-3-7 三态显式化(SKIP 打印**不是** PASS) |
| **镜像 vs 权威漂移**(Entry #55 登记的未实证面) | A-3-6 同轴化(fixture 从 `protocol_instances.json` 读 → 与 S3 spike 对账) |
| **`ResponderSet` 位宽/映射形态无权威** | A-1-2 **spec 首次定义**(bitmask 64 bit,**编译期锁**,bit→node_id 显式映射,per `ADR-CEP-04` 先例措辞) |
| **`validate_topology` 与 `instantiateAll` 双挂复制守卫** | A-* 本 change 内显式声明「**共享同一判据实现**」⇒ **禁双实现**(per metis 建议 #8) |
| **GPU config 等 46 个 configs 无 CEP section** | A-3-7 **三态显式化**(SKIP 打印 ≠ PASS,与 `cep_decl_alignment_check.sh` 的 `[SKIP]` 先例一致) |
| **单人仓无外部真人评审**(独立性问题) | 本仓的「评审」由 oracle / metis / critic subagent 承担 —— **不等价**于真人外部评审,缺口已在 `obj §8.1` R10 登记 |

---

## Acceptance

> **红绿分离**(per 队列 #2/#3 确立):「**判据实现退化**」= 用例红;「**判定结论为红**」= 用例**仍绿**且结论作为交付物登记。本节条款**不**因结论为红而失败。

### capability 1 —— `cep-sharer-tracker-base`(9 Scenario)

- [ ] **A-1-1** `SharerTrackerIf` **接口签名**:4 方法全存在(lookup / snoop_begin / snoop_fold / observe_snoop)且**命名逐字对齐** `ADR-CEP-04 §3` 的 4 段代码块
- [ ] **A-1-2** **caps descriptor 4 属性** struct 存在 + bitmask 64 bit **编译期锁**(spec **首次定义**)+ bit→node_id 显式映射
- [ ] **A-1-3** **5 实现 × 4 caps** mapping 表(`completeness/aggregation_at/hazard/broadcast_fallback`)**数据级自洽契约**(per `ADR-CEP-04 §2 Decision 3` + `§4 table` + `§3.5`)
- [ ] **A-1-4** **FullMapDirectory 行为实现**:`lookup` **ANSWER_HERE** + 精确 sharer 集 + `completeness=EXACT`;`hazard` 在普通 lookup 下返回 STALL;**断言 fullMap 的 caps 与 5×4 表一致** (per `ADR-CEP-04 §2/§4`)
- [ ] **A-1-5** **snoop_begin / snoop_fold / remaining_mask / MergedState 派生事件**(`RetireOnZeroPending` ≥ 1 组可执行断言,per `ADR-CEP-04 §3.3`)
- [ ] **A-1-6** **同地址并发 hazard**:同 addr 两次 lookup ⇒ 第二次按 caps 表返回 STALL(不返回 NONE/NONE) (per `ADR-CEP-04 §4`)
- [ ] **A-1-7** **INV-CEP-04-IC2 幂等** + **IC3 五事件名+时序**的行为断言(per `ADR-CEP-04 §2`)
- [ ] **A-1-8** **禁用 API 静态扫描**(去注释后):`snoop_complete` / `subscribe_remote_snoop` / `is_distributed` / `lookup()→sharer_bitmap` **零出现**(per `ADR-CEP-04 §3.6`)
- [ ] **A-1-9** **FullMap 之外的 4 种实现 → 声明层断言**(schema 枚举值校验);**NOT** 行为断言(per metis 建议 #5)

### capability 2 —— `cep-ordering-point-checker`(6 Scenario)

- [ ] **A-2-1** `AddressResolverIf` **存在且单方法** `resolve(pa) → home_node`(**spec 首次定义**;**不**跨 CEN-Ib8 的 `address_resolve` 流语义)
- [ ] **A-2-2** **J1 单点性** 静态路径覆盖检查(per `ADR-CEP-08 决策 3 J1`)
- [ ] **A-2-3** **J2 全序可观测性**与 caps 耦合:`completeness==PARTIAL` 无 fallback ⇒ J2 fail(per `ADR-CEP-08 决策 3 J2`)
- [ ] **A-2-4** **E 态静默升级检查项**(per `ADR-CEP-08 决策 4` + **INV-CEP-08-IC2**):对 `moesi_gpu` **实跑**检查
- [ ] **A-2-5** **S1-7 (β) 裁决登记**(per `ADR-CEP-05 §13.1`):裁决结论**作为交付物**登记
- [ ] **A-2-6** **J3 唯一性保持**:**降级登记形态**(单点 vacuous 机器判据;通用形式为**文档判据** + 「触发条件 = 队列 #6」降级条款,per 队列 #2 先例)

### capability 3 —— `cep-channel-dependency-gate`(7 Scenario)

- [ ] **A-3-1** 顶点 = `(协议实例, network, vc_id)` **三元组**(per `ADR-CEP-09 §8.2`),且 `vc_id` ∈ 0-3
- [ ] **A-3-2** 边由 `vc_binding` + `ordering_assumption` 导出(**不硬编码** — Entry #55 抗漂移)
- [ ] **A-3-3** 无环 = **PASS**:moesi_gpu fixture 由 `protocol_instances.json` 驱动,断言 8 顶点 / 8 边 / **0 跨网边**(per `cep-poc-spike-verification/spec.md` S3 结论)
- [ ] **A-3-4** **注入环 ⇒ 配置 fail**(**反向验证**):fixture 注入成环 binding,断言 `instantiateAll` 整条管线必须显式返回 false(per `INV-CEP-09-IC1`)且 verdict 逐 `(network, vc)` 标注
- [ ] **A-3-5** **verdict 输出通道**:机器可 grep 格式 `[verdict] net=<net> vc=<id>`
- [ ] **A-3-6** **顶点/边与 S3 spike 对账**(Entry #55 抗漂移**升级为已闭合**)
- [ ] **A-3-7** **SKIP/PASS/FAIL 三态显式化**:`protocol_instances.json` 缺失或空 ⇒ **SKIP**(与 PASS 可区分),per `docs_sync_check.sh` SKIP/RUN 先例

### 收尾

- [ ] **A-FIN-1** `openspec validate cpptlm-cep-directory-ordering-plane --strict` rc=0
- [ ] **A-FIN-2** 守门 7 项 rc=0;`cep_gate_selftest` 全绿;`openspec list --json` 实测 **1/3**
- [ ] **A-FIN-3** `ctest` 全绿;`[cep]` 的 case/断言数**逐项登记**
- [ ] **A-FIN-4** **归档治理 6 步**(per metis):(1) `FROZEN_GLOBS +3`(2) manifest 重生成授权串 (3) selftest 锚点迁移(4) `cep_doc_ref_check` VIRTUAL_PATHS 双态注册(5) 活文档数字一致性同步(6) `openspec archive --yes`
- [ ] **A-FIN-5** 反向验证自查:每条 fixture 变异必红 + GREEN 样本必绿
