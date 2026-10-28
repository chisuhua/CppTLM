# cpptlm-cep-chi-rnf-subset: CHI RN-F 子集 —— 工业协议族首个「非 ArchForge 贡献」实例 + 实例级权威分区 + 真实载体首验

> **状态**: 🟡 **Proposed**(2026-10-28 转正 —— change 目录已建;KPI 并发 **0/3 → 1/3**,per Entry #91)
> **转正依据**:队列 #7 `cpptlm-cep-cohif-carrier` 已完成归档释放 KPI slot(`openspec list --json` 实测 **0** 个 active,per Entry #90)⇒ 满足 `obj §4.7.2` step 0 的硬前置。转正动作见 `design.md` **`D-κ`**(0 ~ 6 步全部执行,Entry #91)。
> **队列**: `obj-cep-platform §4.0` 队列 **#8**(Stage 1 / 2028-Q3)· 排在 #7 之后、#9 `cpptlm-cep-mesif-adapter` 之前
> **关联 ADR**: `ADR-CEP-01`(6 层架构 + 旁路平面)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)· **`ADR-CEP-12`**(实例声明 schema + 权威分区裁决)· `ADR-CEP-11`(规范版本锁定 `chi-r2`)· `ADR-CEP-07A`(协议私有状态边界)· `ADR-CEP-05`(Stub 只暴露稳定态)· `ADR-CEP-13`(域可见写判据 (a)(b))
> **规划输入**: `.rddf/improvements/cpptlm-cep-chi-rnf-subset.md`(5 段草稿)
> **冻结面**: **`ADR-CEP-12`** 在 M1 冻结集内 ⇒ 追加 Status Update 走**两步授权序列**;`protocol_instance.schema.json` **零改动**;`protocol_instances.json` / `perm_upgrades.json` / 守门脚本**不在**冻结集
> **估时**: 8 周(实例 adapter + 实例声明 + 权威分区改造 + 真实载体首验)

---

## Why

队列 #8 是 CEP 平台的**第 2 个**协议实例(第 1 个 = 队列 #6 `moesi_gpu`),也是**第 1 个**「**不是 ArchForge 贡献**」的工业协议实例。三个断点(逐 `file:line` 实测):

| # | 断点 | 证据 |
|---|------|------|
| **D1** | **CHI RN-F 无处安放** | `protocol_instances.json` 是 ArchForge 32 号 §6.1/§6.2 的**逐字段派生副本**;⑥ 镜像闸(`scripts/test/cep_decl_alignment_check.sh:466-468`)对「派生副本含 ArchForge 未声明的实例」**报红**;而 ArchForge 侧**完全没有 CHI 声明**(grep `32-cep-based-coherence-architecture.md` / `09-coherence-protocol.md`:CHI 零命中,仅 `08-noc-interconnect.md` 的物理模型引用)⇒ **队列 #8 交付面要求的「实例声明」当前无处落地** |
| **D2** | **实例全集相等前提首次不成立** | ⑥ 闸建立在「`instances[]` 全集 = ArchForge 声明全集」这一**隐含前提**上;既有的 `status` 字段豁免(`:453-461`)只是**字段级**先例。CHI 属**本仓 `07 号 §5.1.1` 工业族口径**(ArchForge 只贡献 `moesi_gpu` / `mesif`)⇒ 前提在**实例粒度**上首次破 |
| **D3** | **真实载体首验仍是空话** | 队列 #6 的装配验证只能是**逻辑级**(`include/tlm/cep/stub_cache_core.hh:42` 明写 `net_side` 由 **mock ProtocolEngine** 驱动);队列 #7 交付了载体(`CohIfBundle` + `ProtocolBridge` + JSON 装配链)但引擎侧仍是 `CohIfNullProtocolEngine`(`include/tlm/cep/protocol_engine_base.hh:420-455` 全部 `on_*` 把参数 `(void)` 丢弃、**零发射**)⇒ 「CohIf 承载业务 bundle」在**协议语义侧**从未跑通过一次 |

**为什么必须排在 #7 之后**:队列 #7 的载体是本 change 的**前置**(前置 = #6 的 6→3 字典 + #7 的 `CohIfBundle` / `ProtocolBridge`)。反序 ⇒ 本 change 继承无载体装配模式 ⇒ 载体债复制一遍(per Entry #71 同款理由)。

**为什么不是「补 ArchForge 声明」就完事**:ArchForge 侧声明的是**它贡献的**协议;CHI 是**框架级工业族口径**(07 号 §5.1.1)的协议,归**本仓权威**。⇒ 治理上需要一个**「实例级权威分区」**机制,而不是「让本仓冒充 ArchForge」。

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `include/tlm/cep/chi_rnf_adapter.hh` | `tlm::cep::ChiRnfAdapter` —— CHI RN-F 子集 adapter:6 通道 + 状态模型 + 6→4 `perm` 投影 + 6→3 消息类映射(4 行)+ 恒等理由 + 语义损失登记 |
| `include/tlm/cep/chi_rnf_protocol_engine.hh` | `ChiRnfProtocolEngine` —— 可注册的 **真插件** ProtocolEngine(替换队列 #7 的 Null 外壳),驱动 6 通道观测点 + Credit + TxnID + CompAck |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/tlm/cep/protocol_instances.json` | 新增 `chi_rnf` 实例声明(13 属性)+ **顶层 `repo_owned_instance_ids[]`** 登记键 |
| `include/tlm/cep/perm_upgrades.json` | 新增 `chi_rnf` 的 `UNIQUE → WRITEBACK` 静默升级行 + `current_state` 自述同步 |
| `include/tlm/cep/adapter_contracts_loader.hh` | **参数化装载层**(全局输入 AC-01/02/03 + 逐 adapter 输入 AC-04/05)—— 判据本体 `adapter_schema_contracts.hh` **零改动** |
| `scripts/test/cep_decl_alignment_check.sh` | ⑥ 镜像闸**收窄**(镜像比对只覆盖 `ArchForge ids ∩ repo ids`;`extra_ids − repo_owned` 才红)+ **①~⑤ 扩面**到 repo-owned 实例(**不放松**)+ 新增「登记 id 撞 ArchForge id ⇒ 红」 |
| `scripts/test/cep_decl_reverse_verify.sh` | 新增 **3 条**反向 fixture(未登记额外实例 ⇒ 红 / 登记实例违 schema ⇒ 红 / 撞名 ⇒ 红) |
| `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` | 追加 `## Status Update`(**实例级权威分区**裁决 —— architect 权限,两步授权) |
| `docs/architecture/07-coherence-exploration-platform.md` | §5.1.1 CHI 行状态 → 「已落地」+ 队列指针(per `07 号` 冻结面 ⇒ 两步授权) |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | §4.0 行 #8 状态 + §11 台账 Entry |

### 冻结面

**2 件**:`ADR-CEP-12` + `07 号` ⇒ 走**两步授权序列**(先 commit 产物 → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → commit 清单)。

`protocol_instance.schema.json` **零改动**(冻结面 + A′ 方案不需要);`protocol_instances.json` / `perm_upgrades.json` **不在**冻结集(per `ADR-CEP-12 §10` 裁决 ①「诞生即不冻结」)。

---

## Non-Goals

| 不做 | 理由 |
|---|---|
| **完整 CHI** | per `ADR-CEP-16 §4` 改判:Stage 1.4 只做 RN-F subset,完整 CHI → Stage 4+ 维护态 |
| **扩 `CohIfBundle` 字段** | 队列 #7 的 CC-01(5 流域锁)/ CC-02(宽度锁表)已归档 ⇒ subset 的 opcode 白名单应**收窄 subset**,不是扩载体 |
| **扩 `CATEGORY_*` 词表** | 恒为 **3 类**(per `ADR-CEP-12 §8.3` ② + ArchForge 32 号 §6.0.1 的维度正交性补注) |
| **扩 `perm` 4 值锁** | per `ADR-CEP-02` CEN-Ib11 |
| **改 AC-05 机制** | 判据本体 `adapter_schema_contracts.hh` **零改动** ⇒ 不 MODIFIES `cep-adapter-schema-contracts` main spec ⇒ 不挤占 KPI slot |
| **改 AC-04 的恒等语义** | AC-04 的恒等判定按**全 6 类**整表比较(机制层语义缺口)⇒ 本 change **只登记**、不修(修归独立 change) |
| **`StubCacheCore` 改动** | 归 `cpptlm-cep-stub-cache-chstream-surface` |
| **非 CopyBack 写 / atomic / DVM / Retry 语义** | subset 显式排除 + 机器断言(静默收下 ⇒ 红) |
| **新增 invariant 编号** | `test/test_cep_canonical_invariants.cc` 硬编码 15 组 66 条 ⇒ **新增 invariant 数 = 0** |

---

## Capabilities

### 新增 capability:`cep-chi-rnf-subset`

**实例面** —— CHI RN-F 子集 adapter(6 通道 / 状态模型 / 6→4 `perm` 投影 / 6→3 消息类映射)+ `chi_rnf` 实例声明 + 静默升级声明 + 真实载体首验。

### 新增 capability:`cep-instance-authority-partition`

**平台级治理面** —— 「实例级权威分区」:本仓自持实例经顶层 `repo_owned_instance_ids[]` 登记进入 `instances[]`;⑥ 镜像闸收窄到 `ArchForge ids ∩ repo ids`;①~⑤ 语义校验扩面到 repo-owned 实例。**后续 5 个非 ArchForge adapter**(cxl-nonsymmetric / tilelink / cxl-dcoh / nvlink / ualink)**复用**该机制,只填各自的行。

> **与队列 #6 `cep-adapter-schema-contracts` 不同层**:后者是**实例声明内容**的契约(`perm` / `perm_upgrade` / 6→3 映射表);本 capability 是**声明源权威归属**的契约(谁有权声明哪些实例)。⇒ 两者 capability 名不同、判据不重叠。

---

## Acceptance

见 2 份 delta spec 的 Scenario 清单(共 **9** 条 = 6 + 3,编号 `CR-01` ~ `CR-13` 中的 9 条判据;完整清单见 `design.md` 的 `D-η`)。**判据退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**(per 队列 #5 `Entry #68` 的红绿分离纪律)。

**DoD 引用**:`obj-cep-platform §6.2` B1~B8 + §6.3 D1~D3 + `ADR-CEP-00 §2 决策 3` + `ADR-CEP-12` 决策 1~10。

---

## 依赖与风险

| 项 | 类型 | 缓解 |
|---|---|---|
| ~~KPI slot 未释放~~ | **已解除** | 2026-10-28 队列 #7 归档 ⇒ slot 释放(Entry #90)⇒ 实测 `openspec list --json` = 0 active |
| **实例声明落点**(D1/D2) | **架构** | A′ 方案:同文件 + 顶层 `repo_owned_instance_ids[]` 登记 + ⑥ 收窄 + ①~⑤ 扩面;ADR-CEP-12 Status Update 走 architect 车道(**Phase 0**) |
| **台账 Entry #71 的 F-1 引证失锚** | 一致性 | F-1 说「CHI 是显式请求升级 ⇒ 不需要静默升级演进」,但其引证的 `07 号 §5.1.1` CHI 行**无**「显式/静默升级」字样;F-1 混淆了**两条升级轴** ⇒ 追加**勘误 entry**(双轴澄清),不改原文(append-only) |
| **AC-04 恒等语义缺口** | 契约 | CHI 只用 4 类 ⇒ 机械判「非恒等」;**登记**为已知缺口(修归独立 change),不静默放过 |
| **AC-05 无 SKIP 路径** | 机制 | 任何未来无静默升级的 adapter 套 AC-05 必假红 ⇒ 登记为缺口,`#8` 不动机制 |
| **CHI 状态机一手锚缺失** | 一致性 | UC→UD 静默性质目前只有**间接**佐证(gem5 `CHI-cache.sm` 的 `UC`/`UD`/`SC`/`SD` 状态族,per `docs/research/gem5-ruby-protocol-bridge-survey.md:532`)⇒ design 阶段须引 **IHI0050**(AMBA CHI r2,`ADR-CEP-11 §3` 的锁定锚)的一手 cache-state transition 章节作 E1 锚;**引不到则本 change 停在 Phase 1** |
| **两层映射必须两张表** | 正确性 | CohIf 流层(`SnoopResp` 可带 data)与 6 类 / VC 层(`snoop_resp`→VC3 **无数据** / 数据走 `forward_data` 载体→VC2,VC-R2)⇒ 合并成一张表必自相矛盾 |
| **`TXREQ` 双流承载** | 覆盖度 | 6 通道覆盖判据必须按**通道**写观测点;按流写会漏掉「TXREQ 同时承载 Request + Writeback」 |

---

## 关联

- 台账:`.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #71**(DRI 三问裁决 + 范围收窄)· **Entry #80** / **#90**(队列 #6 / #7 归档释放 slot)· **Entry #90**(owner 转指队列 #8)
- 决策:`ADR-CEP-12`(实例声明 schema;本 change 追加 Status Update)· `ADR-CEP-11 §3`(`chi-r2` 锁定)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)· `ADR-CEP-05 §13.1`(静默升级判据 (a)(b))
- 变更件:`openspec/changes/cpptlm-cep-chi-rnf-subset/`(proposal / design / tasks / 2 份 delta spec)
- 规划输入:`.rddf/improvements/cpptlm-cep-chi-rnf-subset.md`
