# Tasks — cpptlm-cep-umbrella v1.0(Stage 0,TDD 5-step)

> **状态**: 📋 **Proposed**(2026-10-22)—— 跨仓准入门(M2)**已放行**(per `ADR-CEP-00 §8` + 台账 Entry #9/#10;取证见 `proposal.md` 前置 gate ④)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-core-contracts/spec.md`](specs/cep-core-contracts/spec.md)
> **上游**: `phase-10.md` Stage 0 子任务 10.B~10.G + `objective-cep-platform §4.1`
> **估时**: 28 人日(Stage 0 剩余;已消耗 13 人日 = 10.A 8 + 10.E 5)
> **KPI**: Stage 0 = **1 slot**(per `ADR-CEP-00 §2 Decision 3`)


---

## 执行进度(2026-10-21,builder 阶段)

| Phase | 状态 | 说明 |
|-------|------|------|
| 1 ADR 解析器 + 契约测试 | ✅ 完成 | 7 cases / 238 assertions;修 2 处 parser 缺陷(`^` 缺 `multiline`、未限定 §4 节内) |
| 2 文档层 | ✅ 完成 | 06-bis Status Update 段(正文 0 diff)+ 07 号复核;组名以真源为准 |
| 3 capability schema | ✅ 完成 | 12 cases / 95 assertions;**C9 负向断言抓到 2 个真问题**(子串假阳性 + `token` 泄漏) |
| 4 Null 覆盖矩阵契约 | ✅ 完成 | 6 cases(契约层;实现属 Stage 1) |
| 5 seam 契约 + 冻结面守门 | ✅ 完成 | 7 cases;冻结面用 `git diff --name-only HEAD` 实测 |
| 6 实验输出契约 | ✅ 完成 | 8 cases / 54 assertions;`network.mode` 否定语境判定(否定词常在出现点**之前**) |
| 7 tools + lint | ✅ 完成 | 6 cases + lint 8/8;lint 的**范围**比规则更易出错(收窄到 38 个 CEP 专属文档) |
| 8 收敛 + 回归 + 文档 | ✅ 完成(验收前置留 1 项) | GREEN 收敛 ✅ / 全量 **0 回归**(81145 assertions / 1856 cases)✅ / 实施笔记 ✅ / **独立 AI 复审 2 轮已完成**(6 BLOCKER 全处置)✅ / ✅ **跨仓准入门(M2)已放行**(2026-10-22,台账 Entry #9/#10)→ 但 Stage 0 仍不 archive(R10 未闭合) |

> **Task 8.4 全部 6 步已完成(2026-10-21)**:独立 AI 复审 2 轮(`general` + `explore`,替代失效的 `oracle` / `critic`)、M1 产物冻结(28 产物 SHA-256 + 机器校验)、M2 跨仓准入门**取证未放行**、R10 登记为「部分独立」、台账 Entry #7。
> **✅ M2 准入门放行(2026-10-22)**:ArchForge 侧接口**规约文档**已合入(`6020f42` + `8290b4c` + `ac956b8`);本仓侧完成**契约收敛批 1**(per-network schema delta + 4 份 ADR 的 Status Update + umbrella spec IC2 改写 + fabric 载体 + **第 9 组检查器**,反向验证 **12/12 全绿**)—— 台账 Entry #9(含 RUN 证据)/ Entry #10(放行裁决)。
> **⛔ Stage 0 仍不 archive**:**仅**因 R10 独立验收未闭合(M2 已放行)。`cpptlm-cep-umbrella` 状态 🟡 **Proposed**(baseline **1/3**)。
> **Commit**:见下方 `git log`(2026-10-21 三次提交:`7195f7f7` / `e1d706a5` / `824bd26e` + 本轮)。

---

## 总体实施时间线(8 Phase,~28 人日)

| Phase | 内容 | 估时 | 依赖 |
|-------|------|:----:|------|
| 1 | TDD RED — 契约测试骨架 + 解析器 | 2d | — |
| 2 | 文档层 — 07 号复核 + 06-bis Status Update | 1d | Phase 1 |
| 3 | capability schema(JSON)+ `INV-CEP-12-IC1~3` | 3d | Phase 1 |
| 4 | `NullProtocolEngine` / `NullCollectivePlane` 10 流覆盖矩阵 | 3d | Phase 1 |
| 5 | ModuleFactory seam 契约 + 冻结面 0 diff 守门 | 4d | Phase 1 |
| 6 | `simulation_mode` + `verification_mode` 锁 + 4 件套契约 | 3d | Phase 1 |
| 7 | `tools/` × 3 + `experimental/` 约定 + lint 脚本 | 6d | Phase 2~6 |
| 8 | GREEN 收敛 + 全量 0 回归 + 文档同步 + 验证建议复核 | 6d | Phase 7 |

---

## Phase 1: TDD RED — 契约测试骨架 + ADR 解析器(2d)

### Task 1.1: 搭 ADR 不变量解析器 + RED 骨架

**Files**:
- 新建 `test/test_cep_canonical_invariants.cc`(~260 行)
- 新建 `test/cep_adr_parser.hh`(test 内部工具头,~120 行)

**步骤**:
- [x] Step 1: 写 `cep_adr_parser.hh` —— 解析 `docs/adr/ADR-CEP-*.md` 的 `§3 不变量` 节,抽出 invariant id 列表
- [x] Step 2: 写 RED 测试:期望解析出 **51 个编号位 / 50 条实际**(IC4 跳空)
- [x] Step 3: 写 RED 测试:期望 `CEN-Ib1`~`CEN-Ib11` 连续无跳号
- [x] Step 4: 写 RED 测试:期望 `INV-SC1`~`INV-SC6` + `INV-CEP-05-IC1` 共 7 条
- [x] Step 5: 写 RED 测试:期望 `INV-CEP-04-IC1/2/3/5/6/7` 共 6 条(**IC4 跳空**)
- [x] Step 6: 写 RED 测试:期望 `INV-CEP-06-IC1`~`IC5` / `INV-CEP-07-IC1`~`IC7` / `INV-CEP-08-IC1`~`IC3` / `INV-CEP-09-IC1`~`IC3` / `INV-CEP-12-IC1`~`IC3` / `INV-CEP-13-IC1`~`IC3` / `INV-CEP-NUM-IC1`~`IC3`
- [x] Step 7: 写 RED 测试:期望 `07 号 §4` 总表的**分组数 = 10** 且各组条数与解析结果一致
- [x] Step 8: 期望编译失败 / 测试 RED

**验证**: `cmake --build build` 退出码 0 但 `./build/bin/cpptlm_tests "[cep][canonical-invariant]"` 全 RED

**Commit**: `test(cep): RED — ADR 不变量解析器 + 50 条 invariant 契约骨架(Phase 1.1)`

### Task 1.2: 验证既有全量 0 回归(RED 阶段基线)

**Files**: 无

**步骤**:
- [x] Step 1: 记录既有全量 case 数与基线(per `objective §0` 约 1241 cases)
- [x] Step 2: 验证既有全部标签全绿,建立回归基线快照

**验证**: `ctest --test-dir build` 全绿 + 基线快照已记录

**Commit**: `test(cep): 记录 Stage 0 回归基线(Phase 1.2)`

---

## Phase 2: 文档层 — 07 号复核 + 06-bis Status Update(1d)

### Task 2.1: 06-bis supersede Status Update 段

**Files**:
- 修改 `docs/architecture/06-bis-coherence-as-network-extension.md`(**仅顶部加段,正文 0 diff**)

**步骤**:
- [x] Step 1: 顶部加 `## Status Update` 段,写明:superseded by `docs/architecture/07-coherence-exploration-platform.md`(CEP),日期 2026-10-21,原阶段范围如何映射到 CEP Stage
- [x] Step 2: 验证**正文 diff = 0**(`git diff --numstat` 仅显示顶部新增行)
- [x] Step 3: 验证 `06-bis` 中被 CEP 承接的接口清单与 `ADR-CEP-00A §3.6` 一致

**验证**: `git diff docs/architecture/06-bis-*.md` 仅顶部段 + `ADR-CEP-00A §3.6` 对照通过

**Commit**: `docs(cep): 06-bis supersede Status Update 段(Phase 2.1)`

### Task 2.2: 07 号真源复核

**Files**:
- 复核 `docs/architecture/07-coherence-exploration-platform.md`(657 行)

**步骤**:
- [x] Step 1: 复核 §3.1 流清单 ↔ `ADR-CEP-02 §2` 逐名一致
- [x] Step 2: 复核 §4 总表 10 组的条数 ↔ 各 owner ADR 的 `§3 不变量` 节一致
- [x] Step 3: 复核 §5.1 adapter 计数口径(2 来源族 / 16 实例 + 5 语义索引)与 §4 计数口径无冲突
- [x] Step 4: 复核 §9 Primary ADRs 覆盖 **16 份** ADR
- [x] Step 5: 复核 §11 Gap → 实现状态映射已含新 ADR 的 4 行

**验证**: 复核清单逐项打勾(无 diff 亦可,复核结论记入实施笔记)

**Commit**: `docs(cep): 07 号真源复核 — 16 份 ADR 覆盖 + 10 组计数口径(Phase 2.2)`

---

## Phase 3: capability schema + `INV-CEP-12-IC1~3`(3d)

### Task 3.1: 写 RED schema 契约测试

**Files**:
- 新建 `test/test_cep_schema.cc`(~140 行)

**步骤**:
- [x] Step 1: 写 RED 测试:schema 文件存在且可解析
- [x] Step 2: 写 RED 测试:schema 必含 **8 个字段组**(`id`/`family`/`adapter`/`status`/`protocol_version`/`flow_support`/`perm`+`atomic`/`consistency_labels`/`ordering_assumption`+`vc_binding`/`sharer_tracker`/`combination_capabilities`)
- [x] Step 3: 写 RED 测试:`flow_support` **恰好 10 项**且与 canonical 10 流逐名对齐
- [x] Step 4: 写 RED 测试:`perm` 值域**恰好 4 值**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`),且**不含** `ATOMIC`
- [x] Step 5: 写 RED 测试:`combination_capabilities` **恰好 3 维**且**正交于** `family`
- [x] Step 6: 写 RED 测试:`consistency_labels` 必含 `read`/`write` 两级 **+** `request_level`
- [x] Step 7: 写 RED 测试:含 `named_network` key 的配置解析不到唯一 `vc_id` ⇒ **fail**(装配期 fail 语义)
- [x] Step 8: 写 RED 测试:`status` 只有 `implemented` 计入 adapter 计数

> ⚠️ **口径取代(2026-10-22,per `ADR-CEP-12 §8` Status Update)** —— 本节 Step 2 的「**8 个字段组**」
> 与 Step 7 的「`named_network` → `vc_id` 解析」**已被取代**:
> 字段组口径为 **7 组 / 13 属性**(per `INV-CEP-12-IC1`);`ordering_assumption` / `vc_binding`
> 改为 **per-network 嵌套形态** —— 「`named_network` 解析到唯一 `vc_id`」这一谓词**整体消失**。
> 上述步骤保留为**当时的 TDD 记录**,不改写;现行判据见 `specs/cep-core-contracts/spec.md` 的 IC2 scenario
> 与 `ADR-CEP-12 §8.3`。

**验证**: `./build/bin/cpptlm_tests "[cep][schema]"` 全 RED

**Commit**: `test(cep): RED — capability schema 8 字段组 + vc_id 解析契约(Phase 3.1)`

### Task 3.2: 实现 schema JSON

**Files**:
- 新建 `include/tlm/cep/protocol_instance.schema.json`(~120 行)

**步骤**:
- [x] Step 1: 写 JSON Schema draft 2020-12,定义 8 个字段组
- [x] Step 2: `flow_support` 用 `properties` 显式列 10 流名 + `additionalProperties: false`
- [x] Step 3: `perm` 用 `enum` 列 4 值;`atomic` 子对象含 `supported` + `intent_field`
- [x] Step 4: `consistency_labels` 含 `read`/`write` enum + `request_level` 附加属性表
- [x] Step 5: `ordering_assumption` 为 `additionalProperties` 的 `p2p_ordered`/`unordered` enum;`vc_binding` 为 `vc_id` 映射
- [x] Step 6: `sharer_tracker` 含 `impl` enum(5 实现 + `不参与`)+ `caps` 四属性
- [x] Step 7: `combination_capabilities` 3 维布尔
- [x] Step 8: 验证 Task 3.1 的测试全 GREEN

**验证**: `./build/bin/cpptlm_tests "[cep][schema]"` 全绿

**Commit**: `feat(cep): protocol_instance.schema.json — 8 字段组(ADR-CEP-12 决策 1)(Phase 3.2)`

---

## Phase 4: Null 实现 10 流覆盖矩阵(3d)

### Task 4.1: 写 RED 覆盖矩阵测试

**Files**:
- 新建 `test/test_cep_dummy_signals.cc`(~140 行)

**步骤**:
- [x] Step 1: 写 RED 测试:`NullProtocolEngine` 的 **10 条流全部**可被观测到(逐流打点)
- [x] Step 2: 写 RED 测试:`NullCollectivePlane` 的 4 个原语(`REDUCE`/`BROADCAST`/`MULTICAST`/`BARRIER`)均返回"不支持"
- [x] Step 3: 写 RED 测试:`NullProtocolEngine` 的 10 流方法名与 canonical 10 流**逐名对齐**

**验证**: `./build/bin/cpptlm_tests "[cep][dummy-signals]"` 全 RED

**Commit**: `test(cep): RED — Null 实现 10 流覆盖矩阵(Phase 4.1)`

### Task 4.2: 实现 Null 覆盖矩阵契约

**Files**:
- 修改 `test/test_cep_dummy_signals.cc`
- 新建 `include/tlm/cep/.gitkeep`(占位)

**步骤**:
- [x] Step 1: 定义 10 流的可观测契约(每流一个计数点)
- [x] Step 2: 断言 canonical 10 流名与 `ADR-CEP-02 §2` 一致
- [x] Step 3: 验证 Task 4.1 测试全 GREEN
- [x] Step 4: 验证既有全量 0 回归

**验证**: `[cep][dummy-signals]` 全绿 + 全量全绿

**Commit**: `test(cep): GREEN — Null 实现 10 流覆盖矩阵(Phase 4.2)`

---

## Phase 5: ModuleFactory seam + 冻结面 0 diff(4d)

### Task 5.1: 写 RED seam 契约测试

**Files**:
- 新建 `test/test_cep_module_factory_seam.cc`(~220 行)

**步骤**:
- [x] Step 1: 写 RED 测试:CEP 模块注册路径**不含**任何新发明宏(grep `include/`)
- [x] Step 2: 写 RED 测试:ProtocolEngine 派生类走 `ModuleFactory::registerObject<T>`
- [x] Step 3: 写 RED 测试:SharerTracker / CollectivePlane 派生类走 `REGISTER_MODULE(T)`
- [x] Step 4: 写 RED 测试:`simulation_mode` 是唯一模式维度(配置中**不存在** `network.mode`)
- [x] Step 5: 写 RED 测试:functional 模式 `avg_latency_cycles == 0`
- [x] Step 6: 写 RED 测试:23 ABI + 4 frozen headers 的 **0 diff** 守门(读 git 或编译期静态断言)

**验证**: `[cep][module-factory-seam]` 全 RED

**Commit**: `test(cep): RED — ModuleFactory seam + 冻结面 0 diff 守门(Phase 5.1)`

### Task 5.2: 实现 seam 契约断言

**Files**:
- 修改 `test/test_cep_module_factory_seam.cc`

**步骤**:
- [x] Step 1: 实现注册路径的静态检查(源码 grep,零运行时依赖)
- [x] Step 2: 实现 `simulation_mode` 单一维度检查(配置 schema 层)
- [x] Step 3: 实现冻结面 0 diff 检查(读 `git diff --name-only` 或编译期 include 断言)
- [x] Step 4: 验证 Task 5.1 测试全 GREEN
- [x] Step 5: 验证既有全量 0 回归

**验证**: `[cep][module-factory-seam]` 全绿 + 全量全绿

**Commit**: `test(cep): GREEN — ModuleFactory seam + 冻结面 0 diff 守门(Phase 5.2)`

---

## Phase 6: `simulation_mode` + `verification_mode` 锁 + 4 件套(3d)

### Task 6.1: 写 RED 实验输出契约测试

**Files**:
- 新建 `test/test_cep_simulation_mode_lock.cc`(~90 行)

**步骤**:
- [x] Step 1: 写 RED 测试:4 件套**恰好 4 个**,缺一件 = fail
- [x] Step 2: 写 RED 测试:`metrics.json` 含 **13** 个必纳字段
- [x] Step 3: 写 RED 测试:不可观测 metric 标 `not-collected` + `_not_collected_reason`,**禁止**省略 / 0 / -1
- [x] Step 4: 写 RED 测试:`verification_mode` ∈ {`exhaustive`, `bounded_random`, `sampled`} **四档**(含"必须声明"这一档)
- [x] Step 5: 写 RED 测试:`bounded_random` / `sampled` 的结果**不得**表述为"已证明无死锁 / 无违例"
- [x] Step 6: 写 RED 测试:`memory_violations` 的判定覆盖 **SWMR + data-value** 双不变量,且报告能区分违例类型

**验证**: `[cep][simulation-mode]` 全 RED

**Commit**: `test(cep): RED — 4 件套 + 13 metric + verification_mode 契约(Phase 6.1)`

### Task 6.2: 实现实验输出契约 + `experimental/metrics.schema.json`

**Files**:
- 修改 `test/test_cep_simulation_mode_lock.cc`
- 新建 `experimental/metrics.schema.json`(~60 行)
- 新建 `experimental/.gitkeep`

**步骤**:
- [x] Step 1: 写 `metrics.schema.json`(13 必纳 + 4 可选 + `not-collected` 契约)
- [x] Step 2: 实现 4 件套完备性断言
- [x] Step 3: 实现 `verification_mode` / `verification_scope` 断言(per design D7:落在 `simulation_config.json`)
- [x] Step 4: 实现"不得越界表述"的**文档级**断言(扫 `docs/architecture/07-*.md` 中的违规措辞)
- [x] Step 5: 验证 Task 6.1 测试全 GREEN
- [x] Step 6: 验证既有全量 0 回归

**验证**: `[cep][simulation-mode]` 全绿 + 全量全绿

**Commit**: `test(cep): GREEN — 4 件套 + verification_mode + experimental 约定(Phase 6.2)`

---

## Phase 7: tools + lint 脚本(6d)

### Task 7.1: 写 RED tools 契约测试

**Files**:
- 新建 `test/test_cep_tools_contract.cc`(~80 行)

**步骤**:
- [x] Step 1: 写 RED 测试:3 个 tools 存在且 `--help` 可运行
- [x] Step 2: 写 RED 测试:tools **不 import** `cpptlm` 包(源码 grep,per design D4)
- [x] Step 3: 写 RED 测试:tools 输入为 4 件套目录,输出文件符合 `experimental/metrics.schema.json`
- [x] Step 4: 写 RED 测试:`ab_compare.py` 输出的 `delta_metrics.json` 可被 `metrics.schema.json` 校验

**验证**: `[cep][tools]` 全 RED

**Commit**: `test(cep): RED — tools 契约(零项目内依赖 + 4 件套输入输出)(Phase 7.1)`

### Task 7.2: 实现 3 个 tools

**Files**:
- 新建 `tools/render_report.py`(~200 行)
- 新建 `tools/run_sweep.py`(~150 行)
- 新建 `tools/ab_compare.py`(~150 行)

**步骤**:
- [x] Step 1: `render_report.py`:`metrics.json` + `trace.h5`(可选)→ `report.html`;`h5py` 缺失时**明确降级提示**,不静默失败
- [x] Step 2: `run_sweep.py`:维度组合 sweep → N 个 4 件套 + `sweep_summary.csv`
- [x] Step 3: `ab_compare.py`:A/B 对比 → `delta_metrics.json` + CDF overlay
- [x] Step 4: 验证 Task 7.1 测试全 GREEN

**验证**: `[cep][tools]` 全绿 + 手工跑一次 demo 输入

**Commit**: `feat(tools): render_report / run_sweep / ab_compare — 纯标准库实现(Phase 7.2)`

### Task 7.3: 写 RED lint 脚本测试 + 实现 `cep_docs_sync_check.sh`

**Files**:
- 新建 `scripts/test/cep_docs_sync_check.sh`(~180 行)

**步骤**:
- [x] Step 1: 写 RED:脚本存在但**未实现**关键检查(锚点 + 计数)
- [x] Step 2: 实现 16 份 ADR 的章节锚点检查
- [x] Step 3: 实现 `07 号 §4` 总表 ↔ 各 owner ADR 的**条数一致性**检查(4 处文档口径一致)
- [x] Step 4: 实现 `07 号 §5.1` adapter 计数口径检查
- [x] Step 5: 实现 `INV-CEP-04-IC4` 跳空的**显式断言**(跳空是设计,不是遗漏)
- [x] Step 6: 验证**既有** `scripts/test/docs_sync_check.sh --strict` **未被改动**且仍 PASS(per design D5)
- [x] Step 7: 验证 lint 全绿

**验证**: `bash scripts/test/cep_docs_sync_check.sh` 全绿 + 既有 `docs_sync_check.sh --strict` 仍 PASS

**Commit**: `feat(ci): cep_docs_sync_check.sh — 16 ADR 锚点 + invariant 计数单一真源(Phase 7.3)`

---

## Phase 8: GREEN 收敛 + 全量回归 + 文档同步(6d)

### Task 8.1: 契约测试全绿收敛

**Files**:
- 修改 `test/test_cep_canonical_invariants.cc`

**步骤**:
- [x] Step 1: Phase 1 的 RED 全部转 GREEN(66 条 invariant 断言)
- [x] Step 2: 加 token 级一致性检查(per design D9):`CEN-Ib11` 必须含 `perm` / `4` / `ATOMIC` / `独立属性` 等关键 token
- [x] Step 3: 验证 `[cep][canonical-invariant]` ~66 cases 全绿
- [x] Step 4: 验证 `[cep]` 全标签合计 46 cases / 519 assertions 全绿

**验证**: `./build/bin/cpptlm_tests "[cep]"` 全绿

**Commit**: `test(cep): GREEN — 50 条 invariant 契约全绿 + token 一致性(Phase 8.1)`

### Task 8.2: 全量回归 + 冻结面终检

**Files**: 无

**步骤**:
- [x] Step 1: `ctest --test-dir build` 全量全绿(**0 回归**:80437 assertions / 1827 cases)
- [x] Step 2: `git diff HEAD -- include/abi/` 为空(23 ABI)
- [x] Step 3: 4 个 frozen headers `git diff` 为空
- [x] Step 4: `openspec validate cpptlm-cep-umbrella --strict` PASS
- [x] Step 5: 验证 `openspec/specs/` 未被污染(Stage 0 未归档,delta spec 未合并)

**验证**: 上述 5 项全过

**Commit**: `test(cep): 全量回归 0 回归 + 冻结面终检(Phase 8.2)`

### Task 8.3: 文档同步 + 实施笔记

**Files**:
- 新建 `docs/implementation/2026-10-21-cep-stage0-contracts.md`(~120 行)
- 复核 `docs/architecture/README.md` Doc Map 行
- 复核 `openspec/changes/cpptlm-cep-umbrella/proposal.md` 文件表(13 文件,per design D8 修正)

**步骤**:
- [x] Step 1: 写实施笔记(TDD 5-step 记录 + commits 索引 + 已知局限)
- [x] Step 2: 复核 README Doc Map(invariant 计数 / adapter 计数)
- [x] Step 3: 复核 proposal 文件表与实际落地一致(14 文件)
- [x] Step 4: 复核 `objective-cep-platform §9.2` 三项角色状态(✅ 2026-10-21 已指定;独立性缺口登记 R10)

**验证**: 复核清单逐项打勾 + `cep_docs_sync_check.sh` 全绿

**Commit**: `docs(implementation): CEP Stage 0 契约层实施笔记 + README/proposal 复核(Phase 8.3)`

### Task 8.4: Stage 0 验收前置确认 + 独立复审(不实施)

**Files**: `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(新)· `scripts/test/cep_freeze_check.sh`(新)· `scripts/test/cep_docs_sync_check.sh`(+第 7 组)· `test/CMakeLists.txt`(+ctest)· `openspec/changes/cpptlm-cep-umbrella/{proposal.md,tasks.md}`(状态)· `.rddf/roadmap/objectives/obj-cep-platform.md`(§5.2 + §8.1 + §11 Entry #7)

**步骤**:
- [x] Step 1: 确认 `objective-cep-platform §9.2` 三项角色**已指定**(✅ 2026-10-21 已完成,形式 gate 解除)
- [x] Step 2: **执行独立 AI 复审**(机制 M4)—— ✅ 2026-10-21 **2 轮**:`general`(架构 6 问)+ `explore`(代码对抗式 6 问,立场 REJECT)。**发现清单**:6 个 BLOCKER(B1 冻结面 CI 恒绿 / B2 一致性闸门三工具全不生效 / B3「全仓」名不副实 / B4 07 号第 3 列未校验 / B5 tools 从不执行 / B6 `ab_compare` 缺 `import html`)+ 15 项 SHOULD_FIX + 架构复审抓出的解析器形态盲区 / 编号重号 / 文件名拼写 / 字段组口径 4 处不一致 / C10 零覆盖等 —— **全部逐条处置**,台账见 `docs/implementation/2026-10-21-cep-stage0-contracts.md §7`。**10 组反向验证**(故意破坏 → 必须红)全部有牙。⚠️ `oracle` / `critic` 已从 agent 注册表消失 → 通道强度弱于原生 oracle,已如实登记
- [x] Step 3: 确认**产物冻结先于解读**(机制 M1)—— ✅ Stage 0 尚无实验 4 件套 → M1 对应物 = **规范性契约产物**冻结:**28 个产物**(21 ADR + `07 号` + 2 schema + 3 tools + umbrella `spec.md`)。清单 `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`(逐件 SHA-256 + 冻结基线 commit);校验器 `scripts/test/cep_freeze_check.sh`(`--update` 重新生成),已接入 `cep_docs_sync_check.sh` **第 7 组** + 独立 ctest `cep_freeze_check`。**反向验证**:改一份冻结 ADR → freeze checker 红 + lint `--strict` exit=1;恢复 → 绿 ✅。⚠️ 两个 checker 脚本自身**不冻结**(checker 必须可修)
- [x] Step 4: 确认**跨仓准入门**满足(机制 M2)—— ✅ **已满足**(2026-10-22):**C-1** ArchForge 侧接口**规约文档**已合入(`6020f42` + `8290b4c` + `ac956b8`;措辞按 `ADR-CEP-00 §8` 修订为「接口规约文档」);**C-2/C-3** P0-1~P0-5 与 6 问结论已由我方逐条签收(回执 §3/§4);**C-4** 已**机器化** —— `scripts/test/cep_decl_alignment_check.sh`(5 项子校验(① `jsonschema` 全量 + ②~⑤ 手写语义),**反向验证 12/12 全绿**,RUN 证据见台账 Entry #9);**C-5** 全量回归 `ctest` **94/94**、`ctest -R cep` **16/16**、CEP lint **9/9 组**、M1 冻结 **29/29**、锚点 **33/33**。**连带**:本 change 状态 🟡 **Proposed Draft** → 🟡 **Proposed**(`proposal.md` + `tasks.md` 同步);objective §5.2 baseline **0/3 → 1/3**;台账 Entry #10 记录放行裁决。⚠️ **Stage 0 仍不 archive**(R10 未闭合,与 M2 无关)
- [x] Step 5: 记录**独立性残余缺口**(R10)—— ✅ Stage 0 验收等级**降级为「部分独立」**(M1 ✅ / M2 ⛔ / M3 ⏳ / M4 ✅);objective §8.1 R10 缓解栏已写明**不得表述为「完全独立验收」**,M4 不可替代真人外部评审
- [x] Step 6: 在 `§11 决策台账` 追加 `sprint-review` entry 记录 Stage 0 结论 —— ✅ **Entry #7**(append-only,置于 Entry #6 之后)

**验证**: 台账新增 entry ✅(`objective-cep-platform §11 Entry #7`)

**⛔ 结论**:Stage 0 **仍不 archive** —— M2 准入门**已于 2026-10-22 放行**(台账 Entry #9/#10),**唯一**未闭合项是 R10 独立验收(真人外部评审仍缺)。台账 Entry #7 记录了当时的未放行结论,Entry #9/#10 记录放行依据。

---

## Stage 1 准入前置(2026-10-22 契约收敛批 1 遗留的 provisional 债)

> 以下 4 项由 `ADR-CEP-12 §8` 的 **provisional 标注**引入 —— 嵌套形态是 **Stage 0 跨仓对齐形态**,
> 采纳自 ArchForge `32 号 §6.4`(其 2 份声明 `status` 均为 `planned`、版本不在锁表)。
> **Stage 1 必须承接**,否则 provisional 变成无声的永久债。
>
> ⚠️ 实际为 **5 项**(S1-1 ~ S1-5)—— 表头「4 项」是初稿遗留,2026-10-22 修正。

- [ ] **S1-1**:`ADR-CEP-09` 的通道依赖图顶点按 **`(协议实例, network, vc_id)`** 建立 —— **在此之前不得用其无环判据放行任何协议实例**(两张网共享 vc_id 值域 0-3,顶点不带 network ⇒ 跨网依赖边被合并 ⇒ 可能给错误 verdict)
- [ ] **S1-2**:N-1 / N-2 的**实现侧复核** —— 首个 `implemented` 实例装配后复核 per-network 嵌套形态
- [ ] **S1-3**:**SLICC 式「协议本地 vnet + 装配期绑定」**备选的重评 —— 触发条件:出现**第 3 张网**或**第 3 个协议实例**(`ADR-CEP-12 §8.4` 已登记,别让它无声死掉)
- [ ] **S1-4**:`include/tlm/cep/fabric.{schema.json,json}` 在首个 `implemented` 实例跑过后**再纳 M1 冻结集**(零实例新产物不诞生即冻结)
- [ ] **S1-5**:**adapter 消息类 → `CATEGORY_*` 的显式映射表(非恒等)** —— 6 类一致性消息类落在 Coherence Interface 层,`CATEGORY_*` 保持**链路层 3 类**不变;映射表由 adapter 承担。**验收**:映射表存在且经 contract test 断言(per `ADR-CEP-12 §8.3` IC2 ③ 的分层纪律)

## Stage 1 准入前置(2026-10-22 A4 批 2 引入的限定债)

> **A6**(`ADR-CEP-05 §7` 判据 2 状态收敛)—— per 台账 **Entry #12** [adr-amendment] 的**路线 B**:
> A4 先行(`moesi_gpu` / `mesif` 登记进 `07 号 §5.1.1`)+ `ADR-CEP-05` 判据 2 的**限定注记**,
> 把「已触发 / 仍未触发」的判定**显式化为 Stage 1 前置**,而不是靠「不登记」来回避。
> **债以限定注记的形式留在契约里,不是被消除。**

- [ ] **S1-6**:**A6** —— 依 `ADR-CEP-13` `INV-CEP-13-IC2` 判据 **(b)** 的结论,重写 `ADR-CEP-05 §7` 判据 2 的「当前不触发」陈述;若 (b) **不成立** → R-2 降级为 P1,`ADR-CEP-12 §8` 的 provisional 标注范围同步收窄。
  **输入**:ArchForge **Q-3**(已 2 轮未交付;第 3 轮催问见回执 §8.5 的 **Q-3a~e**,
  含 **Q-3e**「是否接受以 PoC 的 S2 场景**实验判定**替代 (b) 的纸面评估」)
  **或** `cpptlm-cep-poc-spike` 的 **S2 观测值**(读到旧值 / 新值)——
  **二者取其一即可闭合**;标注侧由 **Q-3c** 裁定(ArchForge 侧 `ADR-SOC-25` vs 本仓 `ADR-CEP-05` 的 Status Update)。
  **验收**:`ADR-CEP-05` 与 `ADR-CEP-12` 的 IC 口径一致;无「已触发 / 未触发」矛盾(per 计划 §6 AC-9)

---

## 验收对照表(proposal Acceptance ↔ tasks)

| Acceptance 项 | 落点 |
|---------------|------|
| TDD 5-step 全 GREEN | Phase 1~8 |
| 50 条 invariant 机器覆盖(10 组) | Task 1.1 / 8.1 |
| `[cep][canonical-invariant]` ~50 cases | Task 1.1 / 8.1 |
| `[cep][dummy-signals]` ~10 cases | Task 4.1 / 4.2 |
| `[cep][module-factory-seam]` ~8 cases | Task 5.1 / 5.2 |
| `[cep][simulation-mode]` ~6 cases | Task 6.1 / 6.2 |
| `[cep][schema]` ~6 cases | Task 3.1 / 3.2 |
| `cep_docs_sync_check.sh` PASS | Task 7.3 |
| 23 ABI 0 diff + 4 frozen headers 0 diff | Task 5.2 / 8.2 |
| 既有全量 0 回归 | Task 1.2 / 4.2 / 5.2 / 6.2 / 8.2 |
| `openspec validate --strict` PASS | Task 8.2 |
| 文档同步 | Task 2.1 / 2.2 / 8.3 |
| ⛔ §9.2 三项角色已指定 | Task 8.4 |
| ⛔ M1 产物冻结(28 产物 hash + 机器校验) | Task 8.4 Step 3 |
| ⛔ M4 独立 AI 复审(2 轮 / 6 BLOCKER 全处置) | Task 8.4 Step 2 |
| ⛔ M2 跨仓准入门(⛔ 未放行 → Proposed Draft) | Task 8.4 Step 4 |
| ⛔ R10 登记为「部分独立」 | Task 8.4 Step 5 |

---

**版本**: v1.0
**最后更新**: 2026-10-21
**维护者**: CppTLM Team(builder mode)
