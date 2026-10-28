---
name: cpptlm-cep-chi-rnf-subset
priority: P1
phase: phase-10
主题: CEP CHI RN-F 子集 —— 工业协议族首个「非 ArchForge 贡献」实例 + 实例级权威分区 + 真实载体首验
依赖:
  - ADR-CEP-01
  - ADR-CEP-02
  - ADR-CEP-12
  - ADR-CEP-11
  - ADR-CEP-07A
状态: ✅ **已归档**(2026-10-28,Entry #95 —— 「**已完成归档**」语义:2 份 delta spec 已合并进 `openspec/specs/cep-chi-rnf-subset/` + `openspec/specs/cep-instance-authority-partition/`;Phase 1 ~ 6 全完成;KPI 并发 **0/3 → 1/3 → 0/3**;⚠️ 转正时的活跃路径 `openspec/changes/cpptlm-cep-chi-rnf-subset/` 已于归档时失效 ⇒ 现为 `openspec/changes/archive/2026-10-28-cpptlm-cep-chi-rnf-subset/`(**不入库**,per `.gitignore`))
created: 2026-10-28
---

# improvement: cpptlm-cep-chi-rnf-subset

> **状态**: ✅ **已归档**(2026-10-28,Entry #95)· **实现已落地、契约已进 main specs**
> **时态锚定**:2026-10-28 转正时登记的活跃路径 `openspec/changes/cpptlm-cep-chi-rnf-subset/` **已于同日归档时失效**(归档件不入库)
> **队列**: `obj-cep-platform §4.0` 队列 **#8**(Stage 1 / 2028-Q3)· 排在 #7 `cpptlm-cep-cohif-carrier` 之后、#9 `cpptlm-cep-mesif-adapter` 之前
> **关联 ADR**: `ADR-CEP-01`(6 层架构 + 旁路平面)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)· `ADR-CEP-12`(实例声明 schema)· `ADR-CEP-11`(规范版本锁定 `chi-r2`)· `ADR-CEP-07A`(协议私有状态边界)
> **估时**: 8 周(实例 adapter + 实例声明 + 权威分区改造 + 真实载体首验)
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1,per `obj §5.4`)

---

## Why

队列 #8 是 CEP 平台的**第 2 个**协议实例(第 1 个 = 队列 #6 `moesi_gpu`),也是**第 1 个**「**不是 ArchForge 贡献**」的工业协议实例。三个断点:

| # | 断点 | 证据 |
|---|------|------|
| **D1** | **CHI RN-F 无处安放** | `protocol_instances.json` 是 ArchForge 32 号 §6.1/§6.2 的**逐字段派生副本**;⑥ 镜像闸(`cep_decl_alignment_check.sh:466-468`)对「派生副本含 ArchForge 未声明的实例」**报红**;而 ArchForge 侧**完全没有 CHI 声明**(grep `32 号` / `09 号`:零命中)⇒ **队列 #8 交付面要求的「实例声明」当前无处落地** |
| **D2** | **实例全集相等前提首次不成立** | ⑥ 闸建立在「`instances[]` 全集 = ArchForge 声明全集」这一**隐含前提**上(`status` 字段豁免只是字段级先例,`cep_decl_alignment_check.sh:453-461`);CHI 属**本仓 07 号 §5.1.1 工业族口径** ⇒ 前提在**实例粒度**上首次破 |
| **D3** | **真实载体首验仍是空话** | 队列 #6 的装配验证只能是**逻辑级**(`stub_cache_core.hh:42` 明写 `net_side` 由 mock ProtocolEngine 驱动);队列 #7 交付了载体(`CohIfBundle` + `ProtocolBridge` + JSON 装配链)但引擎侧仍是 `CohIfNullProtocolEngine`(**零发射**)⇒ 「CohIf 承载业务 bundle」在**协议语义侧**从未跑通过一次 |

**为什么必须排在 #7 之后**:队列 #7 的载体是队列 #8 的**前置**(前置 = #6 的 6→3 字典 + #7 的 `CohIfBundle` / `ProtocolBridge`)。反序 ⇒ #8 继承无载体装配模式 ⇒ 载体债复制一遍(per Entry #71 同款理由)。

**为什么不是「补 ArchForge 声明」就完事**:ArchForge 侧声明的是**它贡献的**协议(`moesi_gpu` / `mesif`);CHI 是**框架级工业族口径**(07 号 §5.1.1)的协议,归本仓权威。⇒ 治理上需要一个**「实例级权威分区」**机制,而不是「让本仓冒充 ArchForge」。

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `include/tlm/cep/chi_rnf_adapter.hh` | `tlm::cep::ChiRnfAdapter` —— CHI RN-F 子集 adapter:6 通道 × 状态模型 + 6→4 `perm` 投影 + 6→3 消息类映射(4 行)+ `identity_reason` + 语义损失登记 |
| `include/tlm/cep/chi_rnf_protocol_engine.hh` | `ChiRnfProtocolEngine` —— 可注册的 **真插件** ProtocolEngine(替换队列 #7 的 Null 外壳),驱动 6 通道观测点 + Credit + TxnID + CompAck |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/tlm/cep/protocol_instances.json` | 新增 `chi_rnf` 实例声明(13 属性)+ **顶层 `repo_owned_instance_ids[]`** 登记键 |
| `include/tlm/cep/perm_upgrades.json` | 新增 `chi_rnf` 的 `UNIQUE → WRITEBACK` 静默升级行 + `current_state` 自述同步 |
| `include/tlm/cep/adapter_contracts_loader.hh` | **参数化装载层**(全局输入 AC-01/02/03 + 逐 adapter 输入 AC-04/05)—— 判据本体 `adapter_schema_contracts.hh` **零改动** |
| `scripts/test/cep_decl_alignment_check.sh` | ⑥ 镜像闸**收窄**(镜像比对只覆盖 `ArchForge ids ∩ repo ids`;`extra_ids − repo_owned` 才红)+ **①~⑤ 扩面**到 repo-owned 实例(不放松)+ 新增「登记 id 撞 ArchForge id ⇒ 红」 |
| `scripts/test/cep_decl_reverse_verify.sh` | 新增 **3 条**反向 fixture(未登记额外实例 ⇒ 红 / 登记实例违 schema ⇒ 红 / 撞名 ⇒ 红) |
| `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | 追加 `## Status Update`(**实例级权威分区**裁决 —— architect 权限) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 行 #8 状态 + §11 台账 Entry |

### 冻结面

**`ADR-CEP-12-protocol-engine-capability-map.md`** 在 M1 冻结集内(40 产物之一)⇒ 追加 Status Update 走**两步授权序列**(先 commit 产物 → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → commit 清单)。

`protocol_instance.schema.json` **零改动**(冻结面 + A′ 方案不需要);`protocol_instances.json` / `perm_upgrades.json` **不在**冻结集(per `ADR-CEP-12 §10` 裁决 ①「诞生即不冻结」)。

---

## Non-Goals

| 不做 | 理由 |
|---|---|
| **完整 CHI** | per `ADR-CEP-16 §4` 改判:Stage 1.4 只做 RN-F subset,完整 CHI → Stage 4+ 维护态 |
| **扩 `CohIfBundle` 字段** | 队列 #7 的 CC-01(5 流域锁)/ CC-02(宽度锁表)已归档;subset 的 opcode 白名单应**收窄 subset**,不是扩载体 |
| **扩 `CATEGORY_*` 词表** | 恒为 **3 类**(per `ADR-CEP-12 §8.3` ② + ArchForge 32 号 §6.0.1 维度正交性补注) |
| **扩 `perm` 4 值锁** | per `ADR-CEP-02` CEN-Ib11 |
| **改 AC-05 机制** | 判据本体 `adapter_schema_contracts.hh` **零改动** ⇒ 不 MODIFIES `cep-adapter-schema-contracts` main spec ⇒ 不挤占 KPI slot |
| **`StubCacheCore` 改动** | 归 `cpptlm-cep-stub-cache-chstream-surface` |
| **非 CopyBack 写 / atomic / DVM / Retry 语义** | subset 显式排除 + 机器断言(静默收下 ⇒ 红) |
| **新增 invariant 编号** | `test_cep_canonical_invariants.cc` 硬编码 15 组 66 条 ⇒ **新增 invariant 数 = 0** |

---

## Acceptance

> **纪律**(队列 #4 `Entry #65` / 队列 #5 `Entry #68`):**守门/闸的新断言必须有反向 fixture**;**纯负向断言必须有正控**;**判据实现退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**。

### A. 实例声明面(`cep-chi-rnf-subset`)

| ID | 判据 | 反向 fixture |
|----|------|--------------|
| **CR-01** | `chi_rnf` 实例声明符合**冻结** schema 全 13 属性;`perm` = 4 值全含;`protocol_version` = `chi-r2`(per `ADR-CEP-11 §3` 锁表);`family` = `industrial` | 删任一 required 键 ⇒ 红;`protocol_version` 写表外值 ⇒ 红 |
| **CR-02** | `ordering_assumption` ⊆ `vc_binding`(逐网络);`vc_binding` 消息类 ∈ 6 类词表;键对称;`vc_id < vc_count` | 破坏任一子条件 ⇒ 红(逐条) |
| **CR-03** | 6→3 映射表完备(每个使用中消息类恰好一行)+ 目标 ∈ 3 类值域 + 恒等须带 `identity_reason` | 删行 ⇒ 红;目标写第 4 类 ⇒ 红;恒等无 reason ⇒ 红 |
| **CR-04** | 6→4 `perm` 投影完备(每个稳定态有投影)+ 瞬态无投影 + `UC+Store→UD` 存在 + perm 秩 `UNIQUE→WRITEBACK` 升高 | 让瞬态有投影 ⇒ 红;改掉该转换 ⇒ 红 |
| **CR-05** | `chi_rnf` 在 `perm_upgrades.json` 声明 `UNIQUE→WRITEBACK`,且被引用完整性 + 单调性判据 PASS | 引用不存在的 id ⇒ 红;`UNIQUE→READ` ⇒ 红 |
| **CR-06** | `sharer_tracker.impl` = `不参与` ⇒ caps 必须是退化行(`scope=NONE` / `completeness=EXACT` / `authority=NONE` / `aggregation_at=NONE`) | `不参与` + `scope=SYSTEM` ⇒ 红 |

### B. 权威分区面(`cep-instance-authority-partition`)

| ID | 判据 | 反向 fixture |
|----|------|--------------|
| **CR-07** | ⑥ 镜像闸:ArchForge 声明的实例 **12 键逐字段严格相等**(镜像漂移 ⇒ 红);`extra_ids − repo_owned` ⇒ 红;登记 id 撞 ArchForge id ⇒ 红 | 改任一非 `status` 键 ⇒ 红(穷举);加未登记实例 ⇒ 红;撞名 ⇒ 红 |
| **CR-08** | ①~⑤ 语义校验**扩面**覆盖 repo-owned 实例(schema / vc_count / 词表 / 网络集 / 边端点绑定) | 登记实例违 schema ⇒ 红;违词表 ⇒ 红 |

### C. 真实载体首验(队列 #8 相对 #7 的核心增量)

| ID | 判据 | 反向 fixture |
|----|------|--------------|
| **CR-09** | **真插件驱动**:一次 CHI 事务经 `ProtocolBridge` **穿过** JSON 装配抵达对端,且由 `ChiRnfProtocolEngine`(**非 Null**)驱动 —— 判据钉在 Null **结构上不可能**有的行为上 | 把引擎换成 `CohIfNullProtocolEngine` ⇒ 必红(Null 零发射) |
| **CR-10** | 6 通道**各有**观测点事务(TXREQ/RXRSP/RXDAT/RXSNP/TXRSP/TXDAT) | 屏蔽任一通道 ⇒ 红 |
| **CR-11** | Credit 流控:credit 耗尽 ⇒ 停发;补 credit ⇒ 恢复(**正控**) | 旁路 credit 记账 ⇒ 红 |
| **CR-12** | TxnID 管理:在途唯一 + 完成后释放可复用 | 强制在途重复 TxnID ⇒ 红 |
| **CR-13** | CompAck:需 CompAck 的读类事务恰好发 1 个;**正控** = 不需者零发送 | 抑制 CompAck ⇒ 红 |

### D. 流程与治理

| 项 | 判据 |
|----|------|
| **判据实现单一** | 判据本体 `adapter_schema_contracts.hh` **零改动**;test 与 CLI 共用 |
| **新增 invariant 数** | = **0** ⇒ `test_cep_canonical_invariants.cc` **零 diff** |
| **冻结面** | `ADR-CEP-12` 追加 Status Update ⇒ 走**两步授权序列**;冻结集成员数**不变**(40) |
| **台账勘误** | Entry #71 的 F-1「CHI 显式请求升级 ⇒ 不需要该演进」**引证失锚** ⇒ 追加勘误 entry(双轴澄清) |
| **KPI** | 占 1 slot ⇒ CEP 内部 Proposed 并发恒 **≤ 1** |
| **独立性缺口** | 本仓为单开发者环境,评审由 AI agent 完成 ⇒ **不等于真人外部评审**;登记为「残余风险 + DRI 书面接受」(per `obj §9.3` R10) |

---

## Capabilities

### 新增 capability:`cep-chi-rnf-subset`

**实例面** —— CHI RN-F 子集 adapter(6 通道 / 状态模型 / 6→4 `perm` 投影 / 6→3 消息类映射)+ `chi_rnf` 实例声明 + 静默升级声明 + 真实载体首验。

### 新增 capability:`cep-instance-authority-partition`

**平台级治理面** —— 「实例级权威分区」:本仓自持实例经顶层 `repo_owned_instance_ids[]` 登记进入 `instances[]`;⑥ 镜像闸收窄到 `ArchForge ids ∩ repo ids`;①~⑤ 语义校验扩面到 repo-owned 实例。**后续 5 个非 ArchForge adapter**(cxl-nonsymmetric / tilelink / cxl-dcoh / nvlink / ualink)**复用**该机制,只填各自的行。

> **与队列 #6 `cep-adapter-schema-contracts` 不同层**:后者是**实例声明内容**的契约(`perm` / `perm_upgrade` / 6→3 映射表);本 capability 是**声明源权威归属**的契约(谁有权声明哪些实例)。⇒ 两者 capability 名不同、判据不重叠。

---

## Impact

| 载体 | 内容 |
|------|------|
| **路线图** | 队列 #8 已在 `obj §4.0` / `§4.2` 就位;本 change 的 change 目录**尚未建** ⇒ 状态列仍 **📋 Planned** |
| **架构真源** | `ADR-CEP-12` 追加 Status Update(**实例级权威分区** —— architect 权限,走两步授权);`07 号 §5.1.1` CHI 行状态更新;**不改** `ADR-CEP-02` / `ADR-CEP-11` / `ADR-CEP-01` / `ADR-CEP-07A` 正文 |
| **契约面** | `protocol_instances.json` + `perm_upgrades.json`(本仓自持,非冻结)+ `adapter_contracts_loader.hh` 参数化(**判据本体零改动**) |
| **框架层** | `cep_decl_alignment_check.sh` ⑥ 收窄 + ①~⑤ 扩面 ⇒ 守门脚本改造(非冻结) |
| **KPI** | 占 1 slot ⇒ 当前 slot 释放(实测 **0 active**,已满足) |
| **延后项** | 完整 CHI(Stage 4+)· `StubCacheCore` ChStream 端口面(`cpptlm-cep-stub-cache-chstream-surface`)· 非 CopyBack 写 / atomic / DVM / Retry 语义 |

---

## ✅ 阻塞:已解除(2026-10-28 转正,Entry #91)

**约束**(`ADR-CEP-00 §2 决策 3` / `obj §5.4`):**任一时刻 CEP 系列 Proposed 并发 ≤ 1**。

**实测**(转正前,2026-10-28):`openspec list --json` = **0 个 active**(队列 #7 已归档,per Entry #90)⇒ **硬前置满足** ⇒ change 目录已建。

**转正后**:KPI 并发 **0/3 → 1/3**(`openspec list --json` 实测 active = `cpptlm-cep-chi-rnf-subset`)。

**治理前置(不阻塞建目录,但阻塞实施)**:ADR-CEP-12 的**实例级权威分区**裁决属 architect 权限 ⇒ 排在 **Phase 0**(台账勘误 entry + ADR Status Update 追加 + 冻结清单重算),builder 阶段才动 JSON / checker。
