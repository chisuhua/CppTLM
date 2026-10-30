# Tasks — cpptlm-cep-mesif-adapter v1.0(Stage 1,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-10-30 转正 —— change 目录已建;KPI 并发 **0/3 → 1/3**)· **队列**: `obj-cep-platform §4.0` **#9**(Stage 1 / 2028-Q4)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-mesif-adapter/spec.md`](specs/cep-mesif-adapter/spec.md) · [`specs/cep-hierarchical-directory/spec.md`](specs/cep-hierarchical-directory/spec.md)
> **上游决策**: `ADR-CEP-04 §13`(第 6 种实现)· `ADR-CEP-12 §8.3`/`§8.4`/`§12`/`§13` · `ADR-CEP-05 §13.1` · `ADR-CEP-02`(CEN-Ib11) · `ADR-CEP-13` 决策 3 · `ADR-CEP-16`
> **上游产物**: 规约层 `cpptlm-cep-mesif-state-spec`(**已归档**)—— `include/tlm/cep/mesif_state_spec.hh`(35 格 `kTable` + 三元组)
> **实施计划**: `.rddf/plans/2026-10-30-cpptlm-cep-mesif-adapter.md`(v0.2)
> **估时**: 10 周(7 个 Phase)
> **KPI**: Stage 1 = **1 slot**(per `ADR-CEP-00 §2 决策 3`;并发上限 1)

---

## 执行进度(待 builder 阶段回填)

| Phase | 状态 | 说明 |
|---|------|------|
| 0 治理前置 | ✅ **已完成**(2026-10-30) | change 目录 4 件套已建 + `openspec validate --strict` rc=0 + KPI **0/3 → 1/3** + 队列 #9 行状态列更新 |
| 1 语义载体 | ✅ **已完成**(2026-10-30) | `MesifAdapter`(零副本委托消费)+ `HierarchicalDirectory` 行为实现 + 行为决策登记;`[cep][mesif-adapter]` 3 case / 96 assertion + `[cep][sharer-tracker]` 16 case / 312 assertion 绿 |
| 2 真插件引擎 | ✅ **已完成**(2026-10-30) | `MesifProtocolEngine` + atomic 最小可用路径 + R-3 写回接线 + 最小装配 JSON + 注册行;`[cep][mesif-engine]` 5 case / 98 assertion 绿 |
| 3 平台级声明 + 装配 | ⬜ **未开始** | `perm_upgrades.json` 增行 + 负控见证双向规则化 + `status` 翻转 + 装配实测 + 通道依赖闸 |
| 4 判据收口 | ⬜ **未开始** | 契约测试 + 反向 fixture 脚本(「因正确原因红」机器判定)+ 既有套件零回归 |
| 5 冻结面 + 归档 | ⬜ **未开始** | `07 号` 漂移 10 处 + Status Update 第十五则 + 两步授权 + 完成归档 + 17 落点 + owner 转指 |
| 6 S1-3 触发后 | ⬜ **未开始** | SLICC 备选重评(ADR-CEP-12 §8.4)+ 承接项 2 滚动复核 |

---

## Phase 0 — 治理前置 ✅

- [x] 0.1 角色切换(planner 起草 → builder 实施);`role.json` 轨迹完整
- [x] 0.2 improvement 5 段草稿(`.rddf/improvements/cpptlm-cep-mesif-adapter.md`)—— **HARD-GATE 通过**
      (7 段齐备 + YAML frontmatter + `**主题**` 匹配 phase-10 + 15 条 AC 复选框)
- [x] 0.3 实施计划双审修订(`.rddf/plans/2026-10-30-cpptlm-cep-mesif-adapter.md` v0.2 —— oracle `mergeable: no` 3 BLOCKER + metis 4 BLOCKER 全闭合)
- [x] 0.4 change 目录 4 件套已建(proposal + design + 2 份 delta spec + tasks)
- [x] 0.5 `openspec validate cpptlm-cep-mesif-adapter --strict` **rc=0**
- [x] 0.6 KPI 并发回填 **0/3 → 1/3**(五处同改:`obj §4.0` / `obj §5.2` / `phase-10.md` / `.rddf/roadmap.md` / `AGENTS.md`)
- [x] 0.7 `obj §4.0` 队列 #9 行状态列更新(🟡 Proposed —— 实现层 change 目录已建)
- [x] 0.8 台账 Entry 追加(附录 #104)

---

## Phase 1 — 语义载体(`MesifAdapter` + `HierarchicalDirectory`)

> **TDD 5 步**:① 判据先红 → ② 实现载体 → ③ 跑判据转绿 → ④ 反向 fixture(毒化必红 + 正控)→ ⑤ 门禁回归 + 勾选

### 1.A `MesifAdapter`(MA-01 / MA-02 / MA-03)

- [x] 1.A.1 **判据先红**:`test/test_cep_mesif_adapter.cc` 建骨架,写 MA-01 / MA-02 / MA-03 的断言(尚无实现 ⇒ 红)
- [x] 1.A.2 实现 `include/tlm/cep/mesif_adapter.hh`:
      - **零自有表副本** —— `constexpr` 委托消费 `MesifStateSpec::kTable` + `perm_of()` + 三元组(design `D-α`)
      - 5→4 `perm` 投影(委托 + 瞬态无投影)
      - 6→3 消息类映射表 `kMessageClassMap` + `kMappingIsIdentity` + `kIdentityReason`(design `D-ε`)
      - 语义损失 ① 的措辞(与 `chi_rnf` 同类损失**可机器区分**)
- [x] 1.A.3 跑判据 ⇒ 转绿(3 case / 96 assertion)
- [x] 1.A.4 **反向 fixture**:① 在 `mesif_adapter.hh` 内伪造一份表 ⇒ 必红 ② 删一行映射 ⇒ 必红 ③ 写错类别 ⇒ 必红;**正控**:载体 35 格全通
      （本 Phase 落地**检测器灵敏度**断言:伪造串必须被扫描器命中 + 缺行/错类别的鉴别力断言;**真实载体的毒化执行版**由 Phase 4 的 `4.1` 脚本承担 —— 那里才有「因正确原因红」的判据 id 归因）
- [x] 1.A.5 门禁回归 + 勾选

### 1.B `HierarchicalDirectory`(HD-01 ~ HD-05)

- [x] 1.B.1 **判据先红**:写 HD-01(caps+behavior 三方相等)/ HD-02(多级聚合差异化)/ HD-03(STALL)/ HD-04(basis 登记)/ HD-05(F 态)断言
- [x] 1.B.2 实现 `include/tlm/cep/sharer_tracker.hh` 的 `HierarchicalDirectory` class:
      - caps = `SYSTEM`/`EXACT`/`GLOBAL`/`HIERARCHICAL`;behavior = `STALL` + 无广播兜底
      - **多级聚合**行为(非 `FullMapDirectory` 克隆)
      - contention ⇒ `STALL` + `valid = false`
- [x] 1.B.3 **行为决策登记表**(design `D-β` B-1 ~ B-5)逐条填「依据」+ 标 `pending = true` ⇒ **跨仓追认债**挂账
      （机器可读形态:`kBehaviorDecisions[5]` + `behavior_decisions_are_consistent()` 可变异谓词 + `kForwardPointRoleRepresentable = false`）
- [x] 1.B.4 跑判据 ⇒ 转绿(16 case / 312 assertion)
- [x] 1.B.5 **反向 fixture**:① 改 caps 任一字段 ⇒ 必红 ② 行为与 traits 行矛盾 ⇒ 必红 ③ 聚合退化为单级 ⇒ 必红 ④ 删任一决策的 `basis` ⇒ 必红;**正控**:三方相等 + 聚合可观测
      （同 1.A.4:真实载体毒化版由 `4.1` 脚本执行;本 Phase 落地可变异谓词的判别力断言 —— `std::array` 可变异副本是必需的,直接 `auto` 拷贝会 decay 成 `const *` 导致 fixture 恒红/恒绿）
- [x] 1.B.6 门禁回归 + 勾选

---

## Phase 2 — 真插件引擎(`MesifProtocolEngine`)

- [x] 2.1 **判据先红**:写 MA-06 / MA-07 断言(引擎面 + atomic + R-3 补偿可观测)
- [x] 2.2 实现 `include/tlm/cep/mesif_protocol_engine.hh`:
      （引擎侧无真源的建模决策 **E-1 ~ E-6** 逐条登记(design `D-λ` + 头注释),并挂跨仓债 #3）
      - 派生 `ProtocolEngineBase<2>`;实现**全部 10 条** canonical 流的 `on_*`
      - 驱动 `MesifAdapter` 表(**可观测点**能区分「走了表」与「空转」)
      - **写回接线**:`M + SnpShared → F` 路径把「写回发生」喂给 tracker(design `D-γ`)
      - **atomic 最小可用路径**:接受 + 按 `vc_binding` 路由 VC1 + `CohIfBundle::atomic` 承载(design `D-δ`)
- [x] 2.3 `configs/cep_mesif_pe_minimal.json`(对齐 `configs/cep_chi_rnf_pe_minimal.json`)
- [x] 2.4 `include/chstream_register.hh` 增 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED("MesifProtocolEngine", ...)`
- [x] 2.5 跑判据 ⇒ 转绿(5 case / 98 assertion;全仓 `cpptlm_tests` 2024 case / 874527 assertion 全绿)
- [x] 2.6 **反向 fixture**:① 断掉引擎→tracker 写回接线 ⇒ dirty 位不转 ⇒ 必红 ② atomic 走错 VC ⇒ 必红 ③ atomic 被拒绝闸误杀 ⇒ 必红;**正控**:接线在位时 dirty 转清 + 合法 atomic 落 VC1
      （本 Phase 为**可变异谓词/实例级**反控;真实载体毒化版由 `4.1` 脚本执行并做判据 id 归因）
- [x] 2.7 门禁回归 + 勾选

---

## Phase 3 — 平台级声明 + 装配实测

- [ ] 3.1 **判据先红**:写 MA-04 / MA-05 / MA-08 断言
- [ ] 3.2 `perm_upgrades.json` 增 `{instance_id: "mesif", from: "UNIQUE", to: "WRITEBACK"}`;**修订** `derived_from.current_state`(删「`mesif` 无该能力」)
- [ ] 3.3 `test/test_cep_adapter_schema_contracts.cc` **重基线** + 负控见证**双向规则化**(design `D-θ`):
      - 规则:**有声明 ⇒ 不在 `no_upgrade_instances`;无声明 ⇒ 在**
      - **反控**:合成输入(移除 `mesif` 声明)⇒ `mesif` 必出现在集合里
      - 原「`mesif` 注入 ⇒ 红」毒化**作废并替换**
- [ ] 3.4 `protocol_instances.json` 的 `mesif.status` → `implemented`;其余 12 键**逐字不动**
- [ ] 3.5 装配实测:`INV-CEP-12-IC2`(取代后 3 条件)逐网络校验通过 + 一次 CohIf 事务**穿过**引擎抵达对端
- [ ] 3.6 `evaluate_channel_dependency_gate` 在 **3 实例共存**下 verdict **无环** + 逐 `(instance, network, vc)` 标注
- [ ] 3.7 跑判据 ⇒ 转绿
- [ ] 3.8 **反向 fixture**:破坏 `mesif` 的 `vc_binding` ⇒ 必红;破坏声明源派生逻辑 ⇒ 必红;**正控**:3 实例共存闸绿
- [ ] 3.9 门禁回归 + 勾选

---

## Phase 4 — 判据收口

- [ ] 4.1 `scripts/test/cep_mesif_adapter_reverse_verify.sh`:
      - 每条 fixture **声明期望失败模式**(`compile-lock` / `runtime-predicate`)
      - probe 输出**失败判据 id** ⇒ 脚本 **grep 断言「红在预期的判据上」**(design `D-η`)
      - **不**沿用「编译 / 运行合并计数」口径 ⇒ 差异已在本文件显式说明
      - 毒化**规约载体**时**两层判据 id 分开打印**
      - 命中 0 次 ⇒ **响亮报红**(非静默跳过)
- [ ] 4.2 ctest 注册新脚本;`scripts/README.md` §CEP 守门脚本表新增行 ⇒「**12 项 CEP 脚本**」计数 → **13**
- [ ] 4.3 **既有套件零回归**:`cep_mesif_state_spec_reverse_verify.sh` 仍 **11/11**;若锚点因载体变更而失效 ⇒ **重新基线化**并**响亮报红**
- [ ] 4.4 全量门禁:13 项 CEP 脚本 + `docs_sync --strict` + `openspec validate --changes --strict` + `format --check` rc=0;`ctest -R cep` 全绿;全仓 `cpptlm_tests` 全绿
- [ ] 4.5 勾选

---

## Phase 5 — 冻结面 + 完成归档

> **批内序写死**(design `D-ι`):① `status` 翻转 + 计数同步 → ② `07 号` 漂移 → ③ commit 产物 → ④ `--update` → ⑤ commit 清单 → ⑥ `openspec archive` → ⑦ KPI / 状态列回填

- [ ] 5.1 `07 号` 漂移修正 **10 处**(design `D-ι` 普查清单:`:75` / `:86` / `:276` / `:278` / `:323` / `:330` / `:573` / `:679` / `:681` / `:721`);**历史 Status Update 块不回改**
- [ ] 5.1b `openspec/specs/cep-sharer-tracker-base/spec.md` **2 处**活口径修正(`Purpose` 末句「唯一行为实现」→「首个」+ 队列 #6~#9 → #10~#11;`Requirement` 首句同改);**`deferred` Scenario 措辞保持**(判据依赖它)
- [ ] 5.2 `07 号` 新增 `## Status Update` 第十五则(覆盖 `:905` 的过时结论)
- [ ] 5.3 invariant 计数**未变**(66 / 编号位 67 / 15 组);`cep_docs_sync_check.sh` rc=0
- [ ] 5.4 commit 冻结面产物
- [ ] 5.5 `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`
- [ ] 5.6 commit 清单
- [ ] 5.7 `openspec archive cpptlm-cep-mesif-adapter`(**完成归档**语义 ⇒ delta spec **合并**进 `openspec/specs/`)
- [ ] 5.8 **17 落点逐项**同步(`obj §4.7.3`:A1-A3 / B1-B7 / C1-C3 / D1-D4)
- [ ] 5.9 队列 #9 行的**准入前置 / 孤儿前置 owner 义务再转指**(给 #10 或宣告 Stage 1 收口)
- [ ] 5.10 KPI **1/3 → 0/3** 回填(**归档之后**才改)
- [ ] 5.11 台账 Entry(完成归档结论 + 落点索引)

---

## Phase 6 — S1-3 触发后

- [ ] 6.1 **S1-3 口径** = `implemented` 实例数 ≥ 3;触发证据三件套齐(`status` + `obj §4.0` 行 + 台账 Entry)
- [ ] 6.2 **SLICC 式备选重评**(design `D-κ` / `ADR-CEP-12 §8.4`)⇒ 结论(维持嵌套形态 / 迁移)入台账 + `07 号` Status Update
- [ ] 6.3 承接项 2 的**滚动复核第 2 次**(节奏源 umbrella design D4.5)⇒ 复核结论入台账
- [ ] 6.4 `obj §4.0` 队列 #9 行 S1-3 由 ⛔ → ✅

---

## 收尾核对(归档前)

- [ ] `tasks.md` **全勾**
- [ ] AC-1 ~ AC-15 全绿
- [ ] **完成归档**语义(delta spec 已合并进 `openspec/specs/`)
- [ ] `obj §4.7.3` **17 落点逐项**已改
- [ ] owner 义务已再转指
- [ ] M1 冻结面**一次**两步授权已完成(`authorized_by` = 新 Entry 号)
- [ ] 「12 项 CEP 脚本」计数 → **13**(三处同步)
- [ ] 残余风险 R-1 / R-3 / A **未**被写成「已消除」
- [ ] 工作区干净;**不**主动 `git push`
