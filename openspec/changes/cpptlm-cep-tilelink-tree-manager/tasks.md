# Tasks — cpptlm-cep-tilelink-tree-manager v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-02)· **队列**: `obj-cep-platform §4.0` **#11** 第 1 个 change(实现层)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-tilelink-tree-manager/spec.md`](specs/cep-tilelink-tree-manager/spec.md)
> **口径**: 台账 **Entry #134** / **#135** / **#138** / **#141** / **#143**
> **规约载体**: `include/tlm/cep/tilelink_state_spec.hh`(第 0 个 change 已归档)
> **估时**: 8 周 · **KPI**: Proposed 并发 **0/3 → 1/3**

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ✅ **已完成**(2026-11-02) | 版本锁定(#135)· 三项 architect 裁决(#138)· **R-T4 裁决**(#141)· 第 0 个 change 已归档(#143)· improvement 草稿(HARD-GATE 通过)· change 目录 4 件套已建 |
| 1 载体形态定稿 | ✅ **已完成**(2026-11-02) | adapter = **零自有表副本**· `TreeManager` = **第 4 种行为类**· `LocalView` **零改动** · `AC-05` **族无关化** · 冻结面 **2 件**清单 —— 详见 `design.md` D-α ~ D-⑨ |
| 2 adapter + tracker 行为实现 | ✅ **已完成**(2026-11-02) | `tilelink_c_adapter.hh`(新)+ `tilelink_tree_manager.hh`(新)+ `sharer_tracker.hh`(+1 通用机械 `snoop_join_core`);TDD 5-step 走完(红 ⇒ 绿 ⇒ 重构);`[cep][tilelink][adapter]` **7 cases / 165 assertions** 全绿;全量 `ctest` **133/133** ⇒ **0 回归**;⚠️ **3 处实施期修订**已回写 `design.md` / delta spec(per 台账 Entry #145) |
| 3 实例声明 + 判据收口 | ✅ **已完成**(2026-11-02) | `protocol_instances.json` + `perm_upgrades.json` + `adapter_contracts_loader.hh` + `adapter_schema_contracts.hh`(AC-05 族无关化)+ `examples/cep_adapter_contracts.cc` + 判据 + 反向验证脚本 + **冻结面 2 件两步授权** |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + 全量 ctest 0 回归 + KPI 1/3 → 0/3 + 7 处 baseline + 归档 |

---

## Phase 0 — 治理前置 ✅

- [x] 0.1 **版本锁定**:`ADR-CEP-11 §3` 锁表 `tilelink-c-1.7.1`(SiFive)⇒ 台账 **Entry #135**
- [x] 0.2 **Task 0 硬门**:公开来源普查 ⇒ `docs/research/2026-11-01-cep-tilelink-tree-manager-anchor-summary.md`
      ⇒ **8 项残余风险全部闭合**(R-T1 ~ R-T8,per Entry #138 / #141)
- [x] 0.3 **三项 architect 裁决**(per **Entry #138**):**R-T2**(5 通道 → 4 VC)· **R-T3**(6 类归属表)· **R-T6**(非对称形态)
- [x] 0.4 **R-T4 裁决**(per **Entry #141**)= `perm` 4 值锁 = **值域锁** ⇒ 实例填 **3 值**
- [x] 0.5 **第 0 个 change(规约层)已归档**(per **Entry #143**)⇒ main spec = `openspec/specs/cep-tilelink-state-spec/spec.md`
- [x] 0.6 improvement 草稿 ⇒ `.rddf/improvements/cpptlm-cep-tilelink-tree-manager.md`(HARD-GATE rc=0)
- [x] 0.7 change 目录 4 件套已建 + `openspec validate --strict` rc=0
- [x] 0.8 **KPI slot 占用**:**0/3 → 1/3** + **7 处**活文档 baseline 同步
      ⇒ ⚠️ **第 7 处(obj §5.3)** 是队列 #10 的前例被门禁抓出来的漏项 ⇒ 勿漏

---

## Phase 1 — 载体形态定稿 ✅

> **状态**:✅ **已完成**(2026-11-02)—— 详见 `design.md` D-α ~ D-⑨。

- [x] 1.1 **adapter 形态 = 零自有表副本**(委托 `TileLinkStateSpec`)⇒ 照 **mesif / cxl 新形态**,**不**照 moesi_gpu / chi_rnf 旧形态
- [x] 1.2 **结构性保证**:adapter 去注释后文本 SHALL 不含 `State::` / `Event::` / `Perm::` 字面量、不含表名 ⇒ 由**静态扫描**兜底
- [x] 1.3 `TreeManager::lookup` = **3 态语义**(`Tip`→`ANSWER_HERE` · `Branch`/`Nothing`→`ESCALATE_UP`+`forward_to`=父节点)⇒ **永不** `BROADCAST`
- [x] 1.4 **`CHAIN` 机械** = 新增 `SnoopMechanics::snoop_join_core`(公共机械 ⇒ 禁双实现)
- [x] 1.5 **`LocalView` 零改动** ⇒ 复用 `forward_to`;⚠️ 双重语义**已登记**
- [x] 1.6 **`AC-05` 族无关化** = 参数化三元组 + **SKIP** 分支 + **回归锁**(既有 4 族判定逐条不变)
- [x] 1.7 **实例 13 键**口径 + `repo_owned_instance_ids` 登记 + `perm` **3 值** + `vc_binding` 按 R-T2
- [x] 1.8 **冻结面 2 件**清单(`cep-sharer-tracker-base` spec · `07 号` §11)⇒ **两步授权**;⚠️ `protocol_instance.schema.json` **零改动**

---

## Phase 2 — adapter + tracker 行为实现(TDD 5-step)

### 2.1 `include/tlm/cep/tilelink_c_adapter.hh`(新 · header-only)

- [x] 2.1.1 `kAdapterName = "TileLinkCAdapter"` · `kInstanceId = "tilelink_c"` · 状态/事件类型别名取 `TileLinkStateSpec`
- [x] 2.1.2 `transition()` **委托** `TileLinkStateSpec::transition`;`perm_of()` **委托** `TileLinkStateSpec::perm_of`
      ⇒ ⚠️ **实施期补正**:第 0 个 change 的载体只有 `perm_of(State) → Perm`(**返回值**形态)
      ⇒ 与平台装载契约(`perm_of(state, out) → bool`)**不兼容** ⇒ 已在载体**补入同源**的出参 overload
      ⇒ 载体**行为**不变、**签名面**补齐;main spec `TSS-04` **零改动**
- [x] 2.1.3 **6→3 消息类映射表** `kMessageClassMap`(**4 行**:消息类 → `CATEGORY_*`)+ `kMappingIsIdentity = false`
      + `kIdentityReason`(非恒等成因 = **词表子集**,**不是**方向不同)
- [x] 2.1.4 **逐消息 → 6 类**归属表 `kProtocolMessageMap`(**10 行**,per **R-T3**)+ `kMessageClassDowngradeNote`(2 处降维登记)
      ⇒ ⚠️ **与 2.1.3 是两张方向不同的表** ⇒ 判据**分别**断言
- [x] 2.1.5 **瞬态态**声明(`Trunk` = 折叠 ⇒ **无瞬态态可泄漏**)+ `kPermMergeNote`(语义损失登记)
- [x] 2.1.6 **零注册**:不注册 23 ABI · 不进 `ModuleFactory` · 不改 frozen header

### 2.2 `sharer_tracker.hh`(+1 通用机械)与 `tilelink_tree_manager.hh`(新 · `TreeManager`)

> ⚠️ **实施期修订**:`TreeManager` 由「并入 `sharer_tracker.hh`」改为**独立头**
> ⇒ 通用抽象层**不**反向依赖协议族(详见 `design.md` D-α 的「`TreeManager` 的落位」)。

- [x] 2.2.1 **新增** `SnoopMechanics::snoop_join_core(addr, extra_responders, handle)`(4 步语义,见 `design.md` D-γ)
      ⇒ ⚠️ **不产生 IC3 生命周期事件**(join **不是**一次新的 snoop begin)⇒ 事件流语义不变
- [x] 2.2.2 **新增** `class TreeManager final : public SharerTrackerIf, public SnoopMechanics`
      (落 `include/tlm/cep/tilelink_tree_manager.hh`)
      —— caps/behavior **取** `kTrackerTraits[3]`(**单源**,不另写字面量)
- [x] 2.2.3 `lookup` = 3 态语义(1.3)· `set_sharer` / `clear_sharer`(照 `FullMapDirectory` 的**非虚公开方法**形态)
      ⇒ ⚠️ `set_sharer` 的第 3 参 = **树位置**(`TileLinkStateSpec::State`),**非** `SnoopState`
      ⇒ 位置**派生**(`tips != 0 ⇒ Tip` / `sharers != 0 ⇒ Branch` / 否则 `Nothing`)⇒ **无双真源**
- [x] 2.2.4 `snoop_begin` / `snoop_fold` / `observe_snoop` **委托** `*_core`(禁双实现)
- [x] 2.2.5 `static_assert` 锁:`TreeManager` 的 caps/behavior == `kTrackerTraits[3]`(照 `HierarchicalDirectory` 形态)

### 2.3 TDD 5-step(每步:红 → 绿 → 重构)

- [x] 2.3.1 **步 1(红)**:先写 `test/test_cep_tilelink_adapter.cc` 的 AC-1 ~ AC-5 断言 ⇒ 编译失败
      ⇒ **实测红**:`fatal error: tlm/cep/tilelink_c_adapter.hh: No such file or directory`
- [x] 2.3.2 **步 2(绿)**:实现 adapter + `TreeManager` 使 AC-1 ~ AC-5 全绿
      ⇒ **实测绿**:`[cep][tilelink][adapter]` **7 cases / 165 assertions** 全绿
- [x] 2.3.3 **步 3(重构)**:零副本静态扫描口径与 `cxl_cache_adapter.hh` 对齐;命名与既有 adapter 对齐
      ⇒ 全量 `ctest` **133/133** ⇒ **0 回归**

---

## Phase 3 — 实例声明 + 判据收口

### 3.1 实例声明

- [x] 3.1.1 `protocol_instances.json` **+1 实例** `tilelink_c`(13 键,逐值按 `design.md` D-ζ)
      ⇒ ⚠️ `perm` **3 值** · `vc_binding` 逐类按 R-T2 · ⚠️ **不得**写 `writeback`
- [x] 3.1.2 `repo_owned_instance_ids` **+`"tilelink_c"`** ⇒ ⑥ 镜像跳过、①~⑤ 覆盖
- [x] 3.1.3 `ordering_assumption`:`compute_noc` 的 **vc2 / vc3**(响应向)**必声明** `p2p_ordered`(**J1 判据**)
- [x] 3.1.4 `perm_upgrades.json` **不加行** + ⚠️ `current_state` 自述**失实**须改
- [x] 3.1.5 `adapter_contracts_loader.hh` 加 `fill_adapter_inputs<TileLinkCAdapter>` + 装载层登记
- [x] 3.1.6 `examples/cep_adapter_contracts.cc` 加一段(CLI 与测试 verdict **同源** ⇒ 禁双实现)

### 3.2 `AC-05` 族无关化

- [x] 3.2.1 参数化「唯一干净态 + 写事件 → 目标态」三元组(不再硬编码 3)
- [x] 3.2.2 **SKIP 分支**:该族**无** unique-clean 态 ⇒ SKIP(非 FAIL)
- [x] 3.2.3 保留秩单调判据(按传入秩比较)
- [x] 3.2.4 ⚠️ **回归锁**:既有 4 族 verdict **逐条不变**(既有测试覆盖)

### 3.3 判据

- [x] 3.3.1 `test/test_cep_tilelink_adapter.cc` —— 零副本静态扫描 + 12 格 pass-through + 3→3 投影 + 6→3 映射 + 非对称形态 + `ESCALATE_UP` / `CHAIN` 真实使用(⇒ **Phase 2 的 TDD 步 1 已建**,**7 cases / 165 assertions** 全绿;Phase 3 追加实例面断言)
- [x] 3.3.2 `test/test_cep_tilelink_instance_decl.cc` —— 13 键齐备 + `perm` 3 值 + `vc_binding` 逐类 + `sharer_tracker` caps + `repo_owned_instance_ids` + `no_upgrade_instances` **恰含** `tilelink_c`
- [x] 3.3.3 **反向 fixture**(每条 SHALL **因正确原因红**):
      ① adapter 植入 `State::` 字面量 ⇒ AC-1 红 ② adapter 自持表 ⇒ AC-1 红
      ③ `ESCALATE_UP` 换成 `ANSWER_HERE` ⇒ AC-5 红 ④ `CHAIN` 换成 `STALL` ⇒ AC-5 红
      ⑤ `snoop_join_core` 不并入 `remaining` ⇒ 归零逻辑红 ⑥ 实例 `perm` 加回 `UNIQUE` ⇒ AC-6 红
      ⑦ `vc_binding` 写 `writeback` ⇒ 装配期 fail ⑧ `AC-05` 传假 `unique_clean_state` ⇒ 回归锁红
- [x] 3.3.4 **反向验证脚本** `scripts/test/cep_tilelink_adapter_reverse_verify.sh` + `.cc`
      (照 `cep_mesif_adapter_reverse_verify.sh`:毒化 ⇒ 必红 + **干净 ⇒ 必绿** + 期望失败模式须匹配)
- [x] 3.3.5 **ctest 注册**:`cep_tilelink_adapter` + `cep_tilelink_adapter_reverse_verify`

### 3.4 冻结面同步(**两步授权**)

- [x] 3.4.1 台账 Entry(授权记录)⇒ `--update --authorized-by "Entry #146"` → commit 产物 → `--update`(provenance 对齐)→ commit
- [x] 3.4.2 `openspec/specs/cep-sharer-tracker-base/spec.md` —— 「其余 **3** 种」→「其余 **2** 种」+ 登记 `TreeManager` 落位
- [x] 3.4.3 `docs/architecture/07-coherence-exploration-platform.md` §11 —— `3 / 6` → `4 / 6` + 「其余 3 个」→「其余 **2** 个」
- [x] 3.4.4 ⚠️ **既有计数断言的连带更新逐条登记**(`instances.size() == 4` → 5 · 07 号 §5.1.1 行数 `== 8` → 9 · `no_upgrade_instances.empty()` → 「恰含 `tilelink_c`」)—— **不得静默改**

---

## Phase 4 — 验收 + 归档

- [ ] 4.1 门禁全绿:docs_sync --strict / doc_ref --strict / cep_docs_sync / gate_selftest /
      freeze / decl_alignment / format --check · `openspec validate --strict` rc=0
- [ ] 4.2 全量 `ctest` **0 回归**;新增 ctest 条目全绿
- [ ] 4.3 **冻结面零 diff**(除 3.4 已授权的 2 件外)
- [ ] 4.4 **KPI 1/3 → 0/3** + **7 处**baseline 同步(⚠️ 同 0.8 的清单)
- [ ] 4.5 **归档语义 = 「✅ 已完成归档」** ⇒ delta spec **合并**进
      `openspec/specs/cep-tilelink-tree-manager/spec.md`
- [ ] 4.6 台账 entry(归档记录)+ §4.0 队列 #11 行状态更新(两段式 ⇒ **全部完成**)

---

## 非目标(Non-Goals)

- ❌ **不**做 **protocol engine**(`TileLinkCProtocolEngine`)—— 与 adapter **分属不同 change**
- ❌ **不**扩 `LocalView`(`tree_parent` 等)—— 公开契约变更 ⇒ **另立 change**;本 change 复用 `forward_to`
- ❌ **不**改 `perm` 值域 · **不**改 `protocol_instance.schema.json`(**零改动**)· **不**改 6 类消息类词表 · **不**改 `CATEGORY_*` 3 类
- ❌ **不**改 `channel_dependency_edges`(顶层跨实例共享;保守近似**已登记**)
- ❌ **不**加 `tilelink_c` 的 `perm_upgrades` 行(无 `UNIQUE` ⇒ 无静默升级)
- ❌ **不**产出任何性能结论 · **不**做对拍 · **不**新增 ABI · **不**新增 invariant 编号数
- ❌ **不**改任何 ADR 的**正文** —— 裁决均**已签发**,本 change 只**消费**
