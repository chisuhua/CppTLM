# Tasks — cpptlm-cep-chi-rnf-subset v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-10-28 转正,Entry #91 —— change 目录已建;KPI 并发 **0/3 → 1/3**)· **队列**: `obj-cep-platform §4.0` #8(Stage 1 / 2028-Q3)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-chi-rnf-subset/spec.md`](specs/cep-chi-rnf-subset/spec.md) · [`specs/cep-instance-authority-partition/spec.md`](specs/cep-instance-authority-partition/spec.md)
> **上游决策**: `ADR-CEP-12`(+ 本 change 追加 Status Update)· `ADR-CEP-11 §3`(`chi-r2`)· `ADR-CEP-02`(canonical 10 流)· `ADR-CEP-05 §13.1`(静默升级判据)
> **估时**: 8 周(8 个 Phase)
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1)

---

## 执行进度(待 builder 阶段回填)

| Phase | 状态 | 说明 |
|---|------|------|
| 0 治理前置 | ⬜ **未开始** | 台账勘误 entry(F-1 双轴澄清)· `ADR-CEP-12` Status Update(architect)· `07 号` §5.1.1 状态 · 冻结清单重算(**2 件冻结面**) |
| 1 adapter 载体面 | ✅ **已完成**(2026-10-28) | `ChiRnfAdapter`(状态模型 + 6→4 投影 + 6→3 映射 + merge note + `identity_reason`)+ **E1 一手锚**(IHI0050) |
| 2 实例声明落地 | ✅ **已完成**(2026-10-28) | `protocol_instances.json` 的 `chi_rnf` + `repo_owned_instance_ids[]` + `perm_upgrades.json` 加行与自述改写 |
| 3 权威分区改造 | ✅ **已完成**(2026-10-28) | ⑥ 收窄 + ①~⑤ 扩面 + 3 条新反向 fixture + 装载层参数化 |
| 4 真插件引擎 | ✅ **已完成**(2026-10-28) | `ChiRnfProtocolEngine`(6 通道观测点 + Credit + TxnID + CompAck) |
| 5 JSON 装配实测 | ✅ **已完成**(2026-10-28) | 装配期校验实测 + CC-05 ④ family 编号对齐 |
| 6 判据收口 | ✅ **已完成**(2026-10-28) | CR-01 ~ CR-14 + 反向 fixture 登记 + 零 diff 复核 |
| 7 真源同步 / 归档 | ⬜ **未开始** | 台账 / 活文档 / 归档 |

---

## 逐任务清单(TDD 5-step:判据 → 实现 → 夹具 → 验证 → 登记)

### Phase 0 —— 治理前置(architect 权限 + 台账勘误)

> **为什么单列 Phase 0**:本 change 有 **2 项**冻结面改动(`ADR-CEP-12` + `07 号`)⇒ 走**两步授权序列**;
> 另需 **1 条台账勘误**(Entry #71 的 F-1 引证失锚)。⇒ 这三件事**先于**任何 JSON / checker 改动 ——
> 否则会在错误前提上实施(Oracle 地雷 1)。

- [ ] **0.1** **台账勘误 entry**(F-1 双轴澄清)—— 追加 `obj-cep-platform.md §11` entry:
      F-1 的**引证失锚**(`07 号 §5.1.1` CHI 行无「显式/静默升级」字样)+ **两轴澄清**
      (轴 1 获取独占 = 显式;轴 2 独占干净→脏 = **静默**)+ F-1 的**排期结论不受影响**
      (perm 演进跟唯一真实消费者走,机制已由队列 #6 交付)⇒ **原文不改**(append-only)
- [ ] **0.2** **`ADR-CEP-12` 追加 `## Status Update`**(architect)—— 五要素:
      原前提 / 新例外 / 不变项 / 触发 / 机器化;并**顺带追认**队列 #6 的 `status` 字段级豁免。
      **正文 §2 ~ §10 逐字零改动**
- [ ] **0.3** **`07 号 §5.1.1` CHI 行状态更新**(冻结面)—— 「已落地」(队列 #8)+ 口径注记
- [ ] **0.4** **冻结清单重算**(两步授权序列)—— ① commit 产物 → ②
      `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` → ③ commit 清单
      (⚠️ **先跑 `--update` 会导致 provenance 失败**)

### Phase 1 —— adapter 载体面(`ChiRnfAdapter`,判据 CR-03 / CR-04)

- [x] **1.1** **E1 一手锚**:引 **IHI0050**(AMBA CHI r2,`ADR-CEP-11 §3` 的锁定锚)的 cache-state
      transition 章节,钉死「`UC` 上 store 命中 ⇒ `UD`,**零总线事务**」——
      ⚠️ **引不到则本 change 停在此处**(不得以 gem5 的二手佐证冒充一手锚;
      二手佐证 = `docs/research/gem5-ruby-protocol-bridge-survey.md:532` 的 `UC`/`UD`/`SC`/`SD` 状态族)
- [x] **1.2** `include/tlm/cep/chi_rnf_adapter.hh` —— `tlm::cep::ChiRnfAdapter`:
      6 通道枚举 + 5 稳定态枚举(`I` / `SC` / `UC` / `UD` / `SD`)+ 瞬态(**无投影**)+
      `perm_of(State, Perm&)` 投影 + 状态转换表 + 6→3 消息类映射(4 行)+
      `kMappingIsIdentity = true` + `kIdentityReason` + `kPermMergeNote`
- [x] **1.3** **瞬态无投影**的编译期锁 —— 瞬态态**不得**出现在 `perm_of` 的投影表内
      (per `INV-CEP-05-IC1`)
- [x] **1.4** **6→3 映射 4 行** + `identity_reason`(登记「4 行全部与方向默认表一致」)
- [x] **1.5** **`kPermMergeNote`** —— `SD → WRITEBACK` 的语义损失登记,
      措辞须写明「**协议信息拓扑的固有损失,RN 侧不可补偿**」(与 `moesi_gpu` 的 `O→WRITEBACK`
      补偿句**措辞不同** —— 后者由 `SparseSnoopFilter` 补偿,本实例 `sharer_tracker = 不参与`)
- [ ] **1.6** **反向 fixture**(CR-03 / CR-04)—— 删 `snoop` 映射行 ⇒ 红;目标写第 4 类 ⇒ 红;
      恒等无 reason ⇒ 红;让瞬态有投影 ⇒ 红;改掉 `UC+Store→UD` ⇒ 红
      (**每条 fixture 必须因正确原因红** —— 断言命中具体的判据子条件,不接受 `!= PASS` 形态)

### Phase 2 —— 实例声明落地(CR-01 / CR-05 / CR-06)

- [x] **2.1** `protocol_instances.json` 的 `chi_rnf` 实例 —— 13 属性齐全:
      `family=industrial` · `adapter=ChiRnfAdapter` · `status=planned`(落地后翻 `implemented`)·
      `protocol_version=chi-r2` · `flow_support`(7 true / 3 false)· `perm`(全 4 值)·
      `atomic={supported:false}` · `consistency_labels={read:SC, write:SC}` ·
      `ordering_assumption` · `vc_binding` · `sharer_tracker` · `combination_capabilities`
- [x] **2.2** **顶层 `repo_owned_instance_ids: ["chi_rnf"]`** —— 登记键(envelope 层;
      ⚠️ `protocol_instance.schema.json` **零改动** —— 它校验**单个实例对象**,不校验 envelope)
- [x] **2.3** `perm_upgrades.json` 加行 `{chi_rnf, UNIQUE, WRITEBACK}` + **`current_state` 自述改写**
      (该字段是本仓自持声明源的自述 ⇒ 加行后原文即过期 ⇒ 失真即 drift)
- [ ] **2.4** **反向 fixture**(CR-05 / CR-06)—— 引用不存在的 id ⇒ 红;`UNIQUE→READ` ⇒ 红;
      `不参与` + `scope=SYSTEM` ⇒ 红

### Phase 3 —— 权威分区改造(CR-07 / CR-08 / CR-09)

- [x] **3.1** `cep_decl_alignment_check.sh` 的 **⑥ 收窄** —— 3 条规则:
      R1 镜像比对只覆盖 `ArchForge ids ∩ repo ids`;R2 `extra_ids − repo_owned` 才红;
      R3 登记 id 撞 ArchForge id ⇒ 红
- [x] **3.2** **①~⑤ 扩面** —— repo-owned 实例同样接受 schema / `vc_count` / 词表 / 网络集 /
      边端点绑定校验(**增加**受检对象,不是**减少**约束)
- [x] **3.3** **3 条新反向 fixture**(登记进 `cep_decl_reverse_verify.sh`)——
      未登记的额外实例 ⇒ 红;登记实例违 schema ⇒ 红;撞名 ⇒ 红
      (⚠️ **既有 fixture 的索引假设**:现只变异 `silent_upgrades[0]`(moesi 行)⇒ 加 chi 行后
      chi 行**无 fixture 覆盖** ⇒ **补**一条变异 `[1]` 的用例)
- [x] **3.4** **装载层参数化**(`adapter_contracts_loader.hh`)—— 拆「全局输入」(AC-01/02/03)与
      「逐 adapter 输入」(AC-04/05);逐 adapter 用**模板参数分派**(编译期类型,非运行期字符串分发);
      **判据本体 `adapter_schema_contracts.hh` 零改动**
- [x] **3.5** **正控**:既有「破坏 10 / 11」fixture 仍 PASS(12 键穷举 + `status` 正控)

### Phase 4 —— 真插件引擎(`ChiRnfProtocolEngine`,CR-10 ~ CR-14)

- [x] **4.1** `include/tlm/cep/chi_rnf_protocol_engine.hh` —— 派生 `ProtocolEngineBase<CohIfBundle>`;
      **6 通道观测点**(逐通道计数)+ Credit 记账 + TxnID 分配/释放 + CompAck 发射
- [x] **4.2** **真插件 vs Null 的判别力** —— 判据钉在 Null **结构上不可能**有的行为上
      (`protocol_engine_base.hh:420-455`:全部 `on_*` 把参数 `(void)` 丢弃、**零发射**)
- [x] **4.3** **subset 排除项的机器断言** —— 非 CopyBack 写(`NCBWrData`)/ `atomic` /
      DVM / Retry 事务**注入即拒绝**(静默收下 ⇒ 红)
- [x] **4.4** **反向 fixture** —— 引擎换成 `CohIfNullProtocolEngine` ⇒ 必红;屏蔽任一通道 ⇒ 必红;
      旁路 Credit 记账 ⇒ 必红;强制在途重复 TxnID ⇒ 必红;抑制 CompAck ⇒ 必红

### Phase 5 —— JSON 装配实测(CR-10 ③)

- [x] **5.1** JSON config —— `cep.protocol_instances` + `cep.bridges`(含 `chi_rnf` 桥条目)
- [x] **5.2** **装配期校验实测** —— 装配闸对绑定 `chi_rnf` 的 config 出逐 `(network, vc)` verdict
      **全 PASS** + per-instance 统计行含 `chi_rnf`
- [x] **5.3** **CC-05 ④ family 编号对齐** —— 桥的 `protocol_id_` = `protocol_family_id("industrial")`
      (per 队列 #7 的既有机制,**不重立判据**)

### Phase 6 —— 判据收口(CR-01 ~ CR-14)

- [x] **6.1** `test/test_cep_chi_rnf_subset.cc`(新建)—— CR-01 ~ CR-14 全部断言 + 正控
- [x] **6.2** **反向 fixture 登记** —— `scripts/README.md` §反向 fixture 登记表(逐条:判据 ID ·
      fixture 形态 · 登记位置)+ `docs/ONBOARDING.md §5.5` 补登记
- [x] **6.3** **ctest 显式登记** —— `test/CMakeLists.txt`
- [x] **6.4** **零 diff 复核** —— `test/test_cep_canonical_invariants.cc` **零 diff** +
      全 change 范围**未新增** `INV-CEP-*` 定义 ⇒ **新增 invariant 数 = 0**;
      `StubCacheCore` **零 diff**;`adapter_schema_contracts.hh` **零 diff**;
      `protocol_instance.schema.json` **零 diff**;`CohIfBundle` **零 diff**
- [x] **6.5** **CLI 与测试 verdict 逐条相同** —— `examples/cep_adapter_contracts.cc` 与 Catch2
      **共用**判据实现 ⇒ 逐条相同(单一真源纪律)

### Phase 7 —— 真源同步 / 归档

- [ ] **7.1** 台账 Entry(收尾)+ §4.0 行 #8 状态 + `phase-10.md` + `.rddf/roadmap.md` +
      `AGENTS.md` KPI 基线 + improvement front-matter
- [ ] **7.2** **归档** —— tasks 全勾 ⇒ `openspec archive cpptlm-cep-chi-rnf-subset -y`
      (**「已完成归档」语义** ⇒ 合并 delta spec 进 `openspec/specs/`)
- [ ] **7.3** **归档引发的守门连锁修** —— 按队列 #7 Entry #90 的 4 类先例逐一排查:
      KPI baseline / 移交链断言 ⑦ owner 转指 / 变异 fixture 锚点跟随 / 对外引用可达性
