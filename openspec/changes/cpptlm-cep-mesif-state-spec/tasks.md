# cpptlm-cep-mesif-state-spec: 任务清单(TDD 5-step:判据 → 实现 → 夹具 → 验证 → 登记)

## 执行进度(待 builder 阶段回填)

| Phase | 内容 | 状态 |
|-------|------|------|
| 0 治理前置 | 台账 entry + 队列 #9 行登记 + KPI 五处同改(0/3 → 1/3) | ✅ **已完成**(2026-10-29,Entry #97 —— commit `934289d3`) |
| 1 规约推导 | 5 态 × 6 事件转换表 + 5→4 投影表 + **逐格 basis** | ✅ **已完成**(2026-10-29 —— design `D-ε.1` ~ `D-ε.11`;**逐格 basis 归载体**,design 只留口径与统计) |
| 2 可执行载体 | `mesif_state_spec.hh`(表驱动 + 编译期锁) | ⬜ **未开始** |
| 3 判据收口 | AC-1 ~ AC-8 + 反向 fixture | ⬜ **未开始** |
| 4 追认清单 + 跨仓对齐 | 逐格追认清单 + ArchForge 侧提案联动 | ⬜ **未开始** |
| 5 归档 | tasks 全勾 ⇒ `openspec archive`(**已完成归档**语义 ⇒ 合并 delta spec) | ⬜ **未开始** |

---

## 逐任务清单

### Phase 0 —— 治理前置

- [x] **0.1** 台账 entry —— `obj-cep-platform.md §11` 追加(立项依据 · 裁决 · 冻结面授权)
  —— ✅ **已完成**:台账 **Entry #97**(2026-10-29;立项依据 + 立项形态的 4 方案对比 + 交付物 + spec 债 + 队列/KPI + 冻结面零改动)
- [x] **0.2** 排队表登记 —— **队列 #9 行**登记「第 0 个 change(规约层)」+ 状态列(🟡 Proposed)+ §4.2 估时列(规约层 4 周 + 实现层 10 周)—— **不新增队列行** ⇒ 顺序 / 季度口径不变
  —— ✅ **已完成**:`obj §4.0` 行 #9 状态列 = 🟡 Proposed(第 0 个 change 已建)+ §4.2 估时列 = **4 + 10 周**
- [x] **0.3** **五处同改**(per 排期调整规则)—— `phase-10.md` 依赖图与 KPI 行 · `.rddf/roadmap.md` · `AGENTS.md` KPI 当前态 **0/3 → 1/3** · `obj §5.2` · `improvements/cpptlm-cep-mesif-state-spec.md` 状态行
  —— ✅ **已完成**:KPI **0/3 → 1/3**(实测 `openspec list --json` = **1** active);`obj`(`§5.2` + `§5.4`)· `phase-10.md` · `.rddf/roadmap.md` · `AGENTS.md` · `improvements/cpptlm-cep-umbrella.md` **五处同改**
- [x] **0.4** **CR-06 ③ 措辞债**的处置确认 —— 已归档 main spec `cep-chi-rnf-subset` 的 CR-06 ③ 写「不新增 SharerTracker 实现」,与 R-1 裁决**冲突** ⇒ 本 change 以 **MODIFIES** 修正(不改已归档件本体)
  —— ✅ **已完成**:本 change 的 `specs/cep-chi-rnf-subset/spec.md` 已写 **MODIFIED**(携带全部 11 个既有 Scenario,仅改 CR-06 ③ 措辞);`openspec validate --strict` rc=0

### Phase 1 —— 规约推导

- [x] **1.1** **5 态定义** —— `M`(Modified)/ `E`(Exclusive)/ `S`(Shared)/ `I`(Invalid)/ `F`(Forward);每态带**一手依据**锚点(per design `D-α` 推导口径表)
  —— ✅ **已完成**:design `D-ε.3`(逐态一手依据:`ArchForge 25-nsa:598` + `25-nsa:599`)
- [x] **1.2** **事件集合** —— 6 事件(与 CHI 同构):`Rd` / `RdUnique` / `Wr` / `Upgrade` / `SnpShared` / `SnpUnique`
  —— ✅ **已完成**:design `D-ε.4`(逐事件语义 + 依据)
- [x] **1.3** **转换表**(5 × 6 = 30 格)—— **每格**带 `basis`(依据来源)+ `pending`(待追认标记)⇒ 规约**可审计**
  —— ✅ **已完成**:30 格在 design `D-ε.5`(逐 kind 推导逻辑)+ `D-ε.6`(逐类裁决示例)完成推导;
  **逐格 `basis` 由载体承载**(design `D-ε.5` 的单一真源注记:归档后 `archive/**` 不入库 ⇒ 逐格表不得留在 design)
- [x] **1.4** **5→4 投影表** —— `I→NONE` / `S→READ` / `E→UNIQUE` / `M→WRITEBACK` / `F→WRITEBACK`;`F` 的投影有 ArchForge 一手依据(`32 号:529`),其余 4 态标注**推导 + 待追认**
  —— ✅ **已完成**:design `D-ε.7`(`F` = `Primary` / `32 号:529`;其余 4 态 = `Structural` / `32 号 §7.2:582-591` 同义迁移);**4 值锁不破**
- [x] **1.5** **`F` 的语义损失登记** —— `F`(单 Owner + 多 Sharer)与 `M` 同投影 `WRITEBACK` ⇒ 「是否还有 sharer」在 4 值下**不可分辨** ⇒ 登记(对齐 `moesi_gpu` 的 `O`/`M` 合并损失登记;但 `chi_rnf` 的 `SD`/`UD` 合并损失是**不可补偿**的,`mesif` 因 `sharer_tracker = HierarchicalDirectory`(EXACT + GLOBAL)⇒ 该损失**由 sharer tracker 补偿** ⇒ 措辞必须写准)
  —— ✅ **已完成**:design `D-ε.9` 损失 ①(**可由 Sharer Tracker 补偿**,措辞与 `chi_rnf` 的「不可补偿」**明确区分**)+ 损失 ②(`F → E` 秩回落)
- [x] **1.6** **写只在独占态合法**的语义 —— `S` 上的写**状态不变**(须先 `Upgrade`)⇒ 与 CHI 的 IHI0050 §B4.1 同型
  —— ✅ **已完成**:design `D-ε.1` 约定 **C3**(写仅在 `{E, M}` 合法;`I`/`S`/`F` 上写 ⇒ 状态不变)+ `D-ε.10` 秩口径(**唯一例外** = `F → E`,已登记)

### Phase 2 —— 可执行载体

- [ ] **2.1** `include/tlm/cep/mesif_state_spec.hh` —— `State` / `Event` 枚举(**值序** `E=2` / `M=3` ⇒ `E+Wr→M` 目标态 = 3)+ `perm_of` + `kStateTransition[5][6]` + 逐格 `basis`
- [ ] **2.2** 三条**编译期锁** —— ① 转换表完备(无空洞)② 5 态投影齐备 ③ `E + Wr → M` 存在且秩升高
- [ ] **2.3** `basis` / `pending` 的**可执行性** —— 每格的 `basis` 非空(`const char*`)⇒ 编译期即可断言;`pending` 计数可查(供「追认进度」观测)

### Phase 3 —— 判据收口

- [ ] **3.1** `test/test_cep_mesif_state_spec.cc` —— **AC-1** 5 态各有投影 · **AC-2** 瞬态无投影 · **AC-3** 转换表完备 · **AC-4** 共享态写状态不变 · **AC-5** 秩单调 · **AC-6** 每格 `basis` 非空
- [ ] **3.2** **反向 fixture** —— ① 给瞬态加投影 ⇒ 红 ② 挖一格转换 ⇒ 红 ③ 共享态写突变 ⇒ 红 ④ 秩下降 ⇒ 红 ⑤ 清空某格 `basis` ⇒ 红(**每条都须「因正确原因红」**)
- [ ] **3.3** **正控** —— `chi_rnf` / `moesi_gpu` 的既有投影**零回归**
- [ ] **3.4** **CR-06 ③ 措辞修正**的 spec 断言 + `openspec validate --strict` rc=0

### Phase 4 —— 追认清单 + 跨仓对齐

- [ ] **4.1** **逐格追认清单** —— 列出「ArchForge 侧需确认/补写什么」(按 `basis` 分组:一手原文 / 推导 / 类比)
- [ ] **4.2** **ArchForge 侧提案联动** —— 对齐提案 `ArchForge:.rddf/improvements/cep-alignment-debt-cleanup.md`(跨仓)(ArchForge 仓)登记本 change 的追认项 ⇒ **跨仓债可见**
  —— ⚠️ **载体已迁移**(2026-10-29,ArchForge 侧 commit `4f0f330`):原架构文档 `41 号` 已移除 ⇒ 本仓指向**已改指**新载体(per Entry #98)
- [ ] **4.3** **队列 #9 的销号条件** —— 明确「本 change 归档 ⇒ 队列 #9 的『余 2 项缺口』登记项可销号」的判据
- [ ] **4.4** **`authority_migrations[mesif]` 降级/删除的**条件登记**(per ArchForge 侧改进提案 AC-11)—— ArchForge 已裁决**方案 A(追认)** ⇒ 两端口径同向;但**追认未完成**前**不得**降级 ⇒ 本条登记**触发条件 + 执行主体 + 台账锚**(本仓无权单方闭环,per `ADR-CEP-12 §13` 的跨仓边界)

### Phase 5 —— 归档

- [ ] **5.1** 台账 entry(收尾)+ §4.0 行 #9 的「第 0 个 change」状态 + **五处同改**同步 + KPI 回 0/3
- [ ] **5.2** `openspec archive cpptlm-cep-mesif-state-spec -y`(**已完成归档**语义 ⇒ **合并** delta spec 进 `openspec/specs/`;**不**用 `--skip-specs`)
- [ ] **5.3** 归档引发的守门连锁修 —— 按队列 #7 Entry #90 / 队列 #8 Entry #95 的先例逐一排查(KPI baseline / 移交链 owner 转指 / 变异 fixture 锚点 / 路径可达性)
