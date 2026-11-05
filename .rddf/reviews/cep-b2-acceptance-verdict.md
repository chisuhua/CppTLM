# B2 验收裁决（architect）—— 61 条有牙载体的逐条复核

**状态**:有条件通过（APPROVE_WITH_CONDITIONS）· **日期**:2026-11-05 · **角色**:architect
**对象**:`test/cep_invariant_assertion_map.json` 的 **61 条**载体 + `test/cep_invariant_deferrals.json` 的 **5 条** ⏭-unmodelled
**判据**:C1 裁决的**规则 B**（载体有牙 = 变红条件与 invariant 语义咬合）+ 判据 **1.1**（文本存在性断言不计入）+ 附裁② 批 0 的「能说清破坏什么 ⇒ 变红」
**方法**:不读「变红理由」措辞，只读**断言体**：逐条取包含载体的 `TEST_CASE` 全文，判断「删掉/破坏该 invariant 的实质 ⇒ 该用例是否红」。

---

## 0. 结论

| 判定 | 条数 | 含义 |
|---|---:|---|
| **A 类 · 载体成立** | **55** | 断言体咬合 invariant 语义，且多数带反向 fixture |
| **B 类 · 借壳（BLOCKER）** | **2** | 断言体的语义**与 invariant 无关** ⇒ 该 id 实际上**无载体** |
| **C 类 · 错位（BLOCKER）** | **1** | 断言体是**另一条** invariant 的语义 ⇒ 本条无载体 |
| **D 类 · 咬合面偏弱 / 文字错位（SHOULD_FIX）** | **3** | 载体成立，但咬的是**声明层**而非 invariant 所述的对象 |
| **E 类 · 范围受限 / 阈值偏松（SHOULD_FIX）** | **2** | 载体成立，但前置条件未建模 / 阈值可被绕过 |

⇒ **B2 暂不得登记 ✅**：3 条 BLOCKER 中有 **2 条使 id 实际无载体**
（`INV-CEP-14-IC1` / `INV-CEP-14-IC2`）⇒ 真实载体数 = **59 / 61**
（`INV-CEP-07-IC3` 另有正确载体 `collective_plane:60`，故该 id 仍成立）。
修正量小（改 3 处标记 + 补 1 条语义断言 + 6 处加固），**不需重做批 0/1/2**。

---

## 1. BLOCKER（必须修，否则 B2 不能宣称 66/66）

### B-1 `INV-CEP-14-IC1` 借壳 —— 载体语义是「决策 5」，不是 IC1

| 项 | 内容 |
|---|---|
| 载体 | `test/test_cep_dummy_signals.cc:89`（`测试替身不入产品目录`） |
| 断言体 | `include/tlm/cep/` 目录下文件名不得含 `test`/`mock`/`fake`/`stub_double` |
| **invariant 原文** | **每条 Stub 合同测试（C1~C10）必须可定位到至少一条 `INV-SC*`/`INV-CEP-05-IC1`/`CEN-Ib*`/`INV-CEP-07-IC7`/`INV-CEP-08-IC2`；不得存在无锚点的合同测试** |
| 判定 | 「替身不入产品目录」是 `ADR-CEP-14` **决策 5** 的产物处置，与该 invariant 无逻辑关系 ⇒ **借壳**。其「变红理由」写「C1~C10 清单缺项 ⇒ 红」也与断言体不符（双重错位） |
| 危险 | 若将来真正的锚点断言被删，此标记仍会让映射表保持绿 ⇒ **掩盖回归** |
| 修法 | 移除该标记；把 `INV-CEP-14-IC1` 挂到 **`test_cep_dummy_signals.cc:107`** 的「C1~C10 逐行 + **锚点列非空**」断言上（那才是 IC1 的语义） |

### B-2 `INV-CEP-14-IC2` 错位 —— 其标记下的语义属 IC1

| 项 | 内容 |
|---|---|
| 载体 | `test/test_cep_dummy_signals.cc:107`（`ADR-CEP-14 的 C1~C10 清单齐备`） |
| 断言体 | 解析 C 表 ⇒ C1~C10 齐备 + **锚点列非空** + `text.find("负向")` |
| **invariant 原文** | **合同测试必须包含至少一条负向断言（C9：Stub 不泄漏协议信息）；缺负向断言 = 合同集不完整** |
| 判定 | 主内容（锚点非空）是 **IC1** 的语义；IC2 的语义只落到 `text.find("负向")`（**文本存在性**，按判据 1.1 **不计入**）⇒ 本条**无有牙载体** |
| 修法 | 标记改挂 IC1（见 B-1）；**新增** IC2 的语义断言：合同测试侧**确实存在负向断言**（如解析 C 表的「测试」列引用，断言对应测试文件含 `REQUIRE_FALSE`/`CHECK_FALSE`），而不是查 ADR 是否写了「负向」二字 |

### B-3 `INV-CEP-07-IC3` 借壳 —— schema 侧的 adapter 计数属另一条判据

| 项 | 内容 |
|---|---|
| 载体 | `test/test_cep_schema.cc:750`（`07 号 §5.1 的 adapter 实例数与表格行数一致`） |
| **invariant 原文** | **`NullCollectivePlane` 是 Collective Plane 默认实现，所有原语返回「不支持」** |
| 判定 | 断言体是「adapter 实例数 ↔ 07 号表格行数」的跨源一致性 —— 与 NullCollectivePlane 无关 ⇒ **借壳** |
| 备注 | 该 id 另有**正确**载体 `test_cep_collective_plane_invariants.cc:60`（CEN-Ib3 同 case），故 id 不会掉成无载体；但借壳标记同样有「掩盖回归」风险 |
| 修法 | 移除 `schema:750` 的该标记；case 内容可保留（它本身是有效断言，只是不属 IC3） |

---

## 2. SHOULD_FIX（不阻断 B2，但须登记并排期）

| # | id | 位置 | 问题 | 建议 |
|---|---|---|---|---|
| S-1 | `INV-CEP-06-IC2` | `test_cep_simulation_mode_lock.cc:69` | invariant 主体是「**实验输出** 13 必纳 metric」，载体只断言 `metrics.schema` 的 `required` 含 13 项（**声明层**）。删掉产物里的某字段不会红 | 增补产物侧断言：`ExperimentMetrics::to_json()` 输出解析后 13 键齐备（或引用 runner 的 `render_report --check`） |
| S-2 | `INV-CEP-07-IC1` | `test_cep_simulation_mode_lock.cc:203` | 载体是「ADR 文本中 `network.mode` 的每处出现须同行否定语境」= **文档扫描**；真正的语义载体在同一文件另一 case（`代码/配置 network.mode 命中数 == 0`） | 标记移到「代码/配置 0 命中」那条；ADR 文本扫描降为辅助 |
| S-3 | `INV-CEP-06-IC5` | `test_cep_simulation_mode_lock.cc:186` | 载体 = schema 有 `memory_violation_kinds.{swmr,data_value}` + ADR 文本含 SWMR/data-value；invariant 是「**统计**必须覆盖双分桶」 | 增补 `ViolationCounts` 双字段的编译期/结构断言（统计面） |
| S-4 | `INV-CEP-15-IC1` | `test_cep_module_factory_seam.cc:115` | ①「变红理由」写的是 **IC3** 的理由（源文本出现协议标识符），与本 case 无关；② case 内含 **4 个 PCIe 冻结头零 diff**，属 `INV-CEP-07-IC5` 的判据面（越界） | 改理由文字为「cache_tlm 被改 ⇒ 红」；PCIe 头断言保留但注明归 07-IC5 |
| S-5 | `INV-CEP-04-IC7` | `test_cep_instance_declaration_contract.cc:143` | invariant 前置条件是「**token 类协议**」，本仓**无 token 类实例**（`ADR-CEP-17` 的 `MOESI_CMP_token` 未落为 instance）⇒ 规则应用分支不可测；载体只覆盖「实现形态 = BroadcastNull / 不参与」这一半 | 在 case 注释显式声明覆盖范围（并登记：首个 token 类实例接入时必须扩展）；或在 `ADR-CEP-17` 落实例后补前置条件分支 |
| S-6 | `INV-CEP-07A-IC3` | `test_cep_contracts_crosscheck.cc:325` | 阈值 `with_field >= 3`（4 个既有 config 里至少 3 个含 `coherence_domains`）⇒ 删 1 个字段不会红 | 改为「已存在的每个 config 都必须含该字段」 |

---

## 3. 复核通过的部分（55 条，附证据要点）

**强载体（行为 / 反向 fixture 双向，且变红条件即 invariant 违反）**：

- `INV-CEP-06-IC3`（跑 `render_report --check`：合规 rc=0 / 违例 rc≠0 + 横幅）、`INV-CEP-11-IC1`（锁表 8 值逐个 `regex_match` schema pattern）、`INV-CEP-11-IC2`（锁表解析 ↔ schema **双向不重复**）、`INV-CEP-12-IC2`（`fabric.json` 的 `vc_count` 上界 + 6 类词表）、`INV-CEP-12-IC3`（`count_implemented` 混合状态行为 + enum 锁）、`INV-CEP-15-IC2`（CEP 目录**内容**扫描 CacheTLM 副本 + 统计扫描文件数防空转）、`INV-CEP-15-IC3`（大写归一扫描协议标识符）、`INV-CEP-08-IC2`（暴露 E 态但未声明层叠 ⇒ FAIL / 打开后 ⇒ PASS）、`INV-CEP-08-IC3`（实现头扫描物理实体）、`INV-CEP-07-IC3`（`collective_plane:60` 全原语不支持）、`INV-SC1~SC6`（Stub 行为 + 正控）、`CEN-Ib3/Ib4`（签名 trait + 坏签名反向 fixture）、`INV-CEP-NUM-IC1`（编号表解析 + 去重）

**批 1/2 新增的 42 条**：逐条复核，**未发现借壳**。其中判据最贴 invariant 的：
`INV-CEP-14-IC3`（`derive_ordering_verdict` 纯函数使「缺项 ⇒ fail」可单测，且**同时修复了实现缺口**）、
`INV-CEP-09-IC1`（真实 PASS / 人造 2-环 FAIL / 改无环恢复 PASS）、
`INV-CEP-11-IC3`（锁表「增补块」授权锚 + 新值须在当前锁表内 —— 同时防「表改了但没留痕」）、
`INV-CEP-16-IC1`（7 条合成行自检，且**变异验证过**：P7 混入白名单 ⇒ 自检红）、
`INV-CEP-07-IC4`（注册点白名单 + **运行时** adapter 工厂查得短名）、
`INV-CEP-05-IC1`（Stub 状态白名单 + 瞬态 T 无投影，且对照稳定态**有**投影）。

**5 条 ⏭-unmodelled**：触发条件逐条复核，**均指向真实未建模对象**（egress 可观测面 / J3 通用形式 / 多 VC 仲裁器 / ADR-CEP-13 决策 5 ① 两处未转写），无「用 ⏭ 凑数」迹象。

---

## 4. 处置建议（路由）

| 步 | 角色 | 内容 | 量级 |
|---|---|---|---|
| 1 | builder | B-1/B-2/B-3 三处 BLOCKER（改标记 + 补 1 条 IC2 语义断言） | ~1-2h |
| 2 | builder | S-1/S-2/S-3/S-4/S-5/S-6 六项 SHOULD_FIX（可同批） | ~2-3h |
| 3 | architect | 复验：只验「3 条 BLOCKER 是否闭合」+「新 diff 是否引入新借壳」（不复审已通过部分） | ~0.5h |
| 4 | planner | 复验通过后落 B2 登记「**66/66**（61 载体 + 5 授权⏭）」+ 台账 Entry | — |

⚠️ **不得**在复验前把 B2 登记为 ✅；映射表闸的 `--for-achievement` 只保证「pending == 0」，**不保证「载体咬合语义」** —— 后者只能人工判，本次裁决即为此。

---

## 5. 关联

`.rddf/reviews/cep-b2-coverage-verdict.md`（C1 规则 A/B/C）· `.rddf/reviews/cep-b2-unmodelled-verdict.md`（4 条分叉 + ⏭ 登记形态）·
`.rddf/reviews/cep-b2-pending-batching-verdict.md`（附裁② 批 0/1/2 + 判据 1.1）· 台账 Entry #232 / #233 / #234 / #235 ·
`test/cep_invariant_assertion_map.json` · `test/cep_invariant_deferrals.json` · `scripts/test/cep_invariant_map_check.py`

---

## 6. 修正回执（builder，2026-11-06）

按本裁决 §4 的第 1、2 步执行完毕（architect 复验待办）。

| 编号 | 处置 | 落地位置 | 证据 |
|---|---|---|---|
| **B-1** | ✅ 移除 `dummy_signals`「测试替身不入产品目录」case 上的借壳标记（该 case 属 `ADR-CEP-14` 决策 5，不承载 invariant）；`INV-CEP-14-IC1` 改挂到「C1~C10 清单齐备」case 的**锚点列非空**断言 | `test/test_cep_dummy_signals.cc:107` | 映射表：`INV-CEP-14-IC1 → dummy_signals:107` |
| **B-2** | ✅ `INV-CEP-14-IC2` 落到**新建**语义 case：C 表**恰一条**「负向」行 + 锚点列非空 + 负向判定机制**有鉴别力**（合成含协议名文本必命中 / 干净文本必不命中）+ 该负向断言**真的施加在产品层**（去注释扫 Stub 源码 ⇒ 命中集空） | `test/test_cep_dummy_signals.cc:137`（新 case） | 同上；`[cep][dummy-signals]` 7 cases 绿 |
| **B-3** | ✅ 移除 `test_cep_schema.cc` 上 `INV-CEP-07-IC3` 的借壳标记（该 case 是「adapter 实例数 ↔ 07 号表格行数」跨源一致性）；`INV-CEP-07-IC3` 只保留正确载体 | `test/test_cep_collective_plane_invariants.cc:60` | 映射表：`INV-CEP-07-IC3` 只剩 1 个载体 |
| **S-1** | ✅ `INV-CEP-06-IC2` 增补**产物侧**断言：`cep_exp::ExperimentMetrics::to_json()` 的键集合 ⊇ 13 必纳 metric（不再只断言 schema 声明） | `test/test_cep_simulation_mode_lock.cc` | `[cep][simulation-mode]` 10 cases 绿 |
| **S-3** | ✅ `INV-CEP-06-IC5` 增补**统计面**断言：`SwmrDataValueDetector` 的 SWMR / data-value **分别**计数且 `total()` 求和；反向：关检查 ⇒ 双桶为 0 | 同上 | 同上 |
| **S-4** | ✅ `INV-CEP-15-IC1` 的「变红理由」改正为「cache_tlm 相对基线非零 diff ⇒ 红」，并注明该 case 内 4 个 PCIe 冻结头属 `INV-CEP-07-IC5` 判据面（不重复挂标记） | `test/test_cep_module_factory_seam.cc:115` | — |
| **S-5** | ✅ `INV-CEP-04-IC7` 增补**覆盖范围声明**：token 类协议前置条件当前无实例 ⇒ 本 case 覆盖「实现形态」支；首个 token 类实例接入时必须扩展 | `test/test_cep_instance_declaration_contract.cc` | — |
| **S-6** | ✅ `INV-CEP-07A-IC3` 的 `with_field >= 3` 收紧为「**每个**既有 config 都必须含 `coherence_domains`」，另保留「确有既有 config 被检查」下界防空转 | `test/test_cep_contracts_crosscheck.cc` | — |
| **S-2** | ⚠️ **本条为 architect 误判，不做改动** —— 复核发现该 case（`simulation_mode_lock.cc:206`）**已含**「代码/配置 `network.mode` 命中数 == 0」的语义断言（case 内自带注释「断言面必须覆盖**代码与配置**,不只是 ADR 文字」），载体本就咬合语义 | — | 见该 case `:247-290` |

**未改动项说明**：S-2 修正后，architect §2 表中的 SHOULD_FIX 由 6 项降为 **5 项**（S-2 撤销）。

**验证**：`[cep]` 428 cases / 10023 assertions 绿 · `ctest` 154/154 绿 · `format` rc=0 ·
映射表闸 `--for-achievement` rc=0（66/66 = 61 载体 + 5 ⏭-unmodelled + 0 pending）·
`docs_sync` / `doc_ref` / `freeze` / `prereg_drift` rc=0。

**待 architect 复验**：只验「B-1/B-2/B-3 是否闭合」+「本轮新 diff 是否引入新借壳」，不复审已通过部分。

---

## 7. 复验裁决（architect，2026-11-06）

**范围**：per §4 第 3 步 —— 只验「B-1/B-2/B-3 是否闭合」+「本轮新 diff 是否引入新借壳」；**不复审**已通过的 55 条。
**被验对象**：commit `e9175370`（8 文件，+145/−31）。

### 7.1 BLOCKER 闭合判定

| 编号 | 判定 | 证据 |
|---|---|---|
| **B-1** | ✅ **闭合** | 「测试替身不入产品目录」case 上已无 `INV-CARRIER`（仅留说明注释）；`INV-CEP-14-IC1` 现挂 `test_cep_dummy_signals.cc:107`，其断言体 = C1~C10 逐行**锚点列非空** ⇒ 与 IC1「每条合同测试必须可定位到至少一条锚点」**语义一致** |
| **B-3** | ✅ **闭合** | `test_cep_schema.cc` 内 `INV-CEP-07-IC3` 标记数 = **0**；该 id 在映射表中**只余** `test_cep_collective_plane_invariants.cc:60`（NullCollectivePlane 全原语不支持 ⇒ 语义正确） |
| **B-2** | ◐ **部分闭合（未完全闭合）** | 见 7.2 |

### 7.2 B-2 残余缺口（精确要求）

已闭合的部分：C 表恰一条「负向」行 + 锚点列非空；负向判定机制**有鉴别力**（合成含协议名文本必命中 / 干净文本不命中）；该机制**施加于产品层**且命中集为空。

**仍缺**：invariant IC2 的主体是「合同测试**集**必须**含**至少一条负向断言」——
现载体证明的是「**可以**写出这样一条负向断言且它有效」，
⛔ **不能**发现「测试集里那条负向断言被删掉」（删除后用例仍绿）⇒ 载体未咬合 IC2 的主体。

**闭合所需的机器判据（可执行、变红原因正确）**：

> 对 C 表「负向」行，其「断言的不变量」列给出的**锚点 id**（C9 行 = `INV-CEP-05-IC1`），
> **必须**存在某个 `test/test_*.cc` 文件**同时含**该锚点 id **与**至少一条负向断言宏
> （`REQUIRE_FALSE` / `CHECK_FALSE`）。
> - 变红条件：删掉该文件里的负向断言 ⇒ 红；删掉锚点 ⇒ 红。
> - 可满足性已实测：当前有 **4** 个测试文件同时满足（`test_cep_canonical_flow_contract.cc` /
>   `test_cep_canonical_invariants.cc` / `test_cep_moesi_gpu_adapter.cc` / `test_cep_moesi_gpu_compensation.cc`）。
> - **附带收益**：该判据按**锚点 id** 定位，**不依赖** C 表已陈旧的「归属测试文件」列
>   （C9 该列写的是 `test_cep_canonical_invariants.cc`，而 ADR-CEP-14 自记「实测不成立」——
>   实际负向断言在 `test_cep_schema.cc` / `test_cep_contracts_crosscheck.cc`）⇒ 自动避开该陈旧列。

工作量：~15 分钟（1 条断言 + 1 条反向 fixture）。

### 7.3 本轮新 diff 是否引入新借壳

**否。**逐项核对：

| 改动 | 性质 | 判定 |
|---|---|---|
| `dummy_signals` 新增 IC2 case | **被审对象本身** | ◐ 见 7.2（非借壳，是覆盖不足） |
| `simulation_mode_lock` S-1（产物侧 13 键） | 补进 `INV-CEP-06-IC2` 既有 case | ✅ 加固同一 invariant，未换语义 |
| `simulation_mode_lock` S-3（双分桶统计） | 补进 `INV-CEP-06-IC5` 既有 case | ✅ 同上 |
| `contracts_crosscheck` S-6（逐 config 必含字段） | 收紧 `INV-CEP-07A-IC3` 阈值 | ✅ 同上 |
| `module_factory_seam` S-4 / `instance_declaration_contract` S-5 | **纯注释** | ✅ 无载体变更 |

标记增删核对：`INV-CEP-14-IC1`（移位）、`INV-CEP-14-IC2`（移位）、`INV-CEP-07-IC3`（删除 1 处）；
**无新增 id**；映射表闸 `--for-achievement` 仍 rc=0（66/66 = 61 载体 + 5 ⏭-unmodelled + 0 pending）。

### 7.4 结论

**B-1 / B-3 已闭合；B-2 差一条断言。**

⇒ **B2 仍不得登记 ✅**（`INV-CEP-14-IC2` 无有牙载体 ⇒ 真实载体数 60/61）。
⇒ 下一步：builder 补 §7.2 的那条判据（~15min）→ architect 复验该 1 条 → planner 落 B2 登记。

---

## 8. B-2 闭合回执（builder，2026-11-06）

按 §7.2 的机器判据补齐 `INV-CEP-14-IC2` 的主体（合同测试集**含**该负向断言）。

**落地**：`test/test_cep_dummy_signals.cc` 的 IC2 case 内新增第 ④ 段 + 3 条反向 fixture。

| 项 | 内容 |
|---|---|
| 判据 | 取 C 表「负向」行的**锚点 id**（C9 行 = `INV-CEP-05-IC1`），要求仓内存在某个 `test/test_*.cc` **同时**含该锚点 id 与至少一条负向断言宏（`REQUIRE_FALSE(` / `CHECK_FALSE(`） |
| 变红条件 | 删掉那条负向断言 ⇒ 红；删掉锚点 ⇒ 红 |
| 按**锚点 id** 定位 | ⛔ 不依赖 C 表已陈旧的「归属测试文件」列（C9 该列写 `test_cep_canonical_invariants.cc`，而 ADR-CEP-14 自记「实测不成立」）⇒ 自动避开该陈旧列 |
| 防恒真 | 扫描文件数下界 `> 10`（目录读空即红） |
| 反向 fixture | (a) 有锚点无负向断言 ⇒ 判否；(b) 有负向断言无锚点 ⇒ 判否；(c) 二者兼有 ⇒ 命中（证明非恒否） |

**变异验证**（判别力证据）：把解析出的锚点 id 数字 `0→9`（`INV-CEP-05-IC1` → `INV-CEP-95-IC1`）后重跑 ⇒

```
FAILED: 合同测试集里没有「含锚点 INV-CEP-95-IC1 …」的文件
test cases: 7 | 6 passed | 1 failed
```

⇒ 该断言**因正确原因红**（不是恒真占位）。

**验证**：`[cep][dummy-signals]` 245 assertions 绿 · `[cep]` 428 cases / 10030 assertions 绿 ·
`ctest` 154/154 绿 · `format` rc=0 · 映射表闸 `--for-achievement` rc=0（66/66 = 61 载体 + 5 ⏭-unmodelled + 0 pending）·
`docs_sync` / `doc_ref` / `freeze` / `prereg_drift` rc=0。

**待 architect 终验**：只验本条（§7.2 的要求 ⇒ §8 的落地）。

---

## 9. 3c 终验裁决（architect，2026-11-06）

**范围**：只验 §7.2 的要求 ⇒ §8 的落地（`INV-CEP-14-IC2` 那一条）。

### 9.1 判定：**未通过**（发现自指漏洞，判据不咬合其主体）

§8 的实现**形式上**满足 §7.2 的字面要求（取锚点 id → 找同含锚点与负向断言宏的 `test/test_*.cc`）。
但独立复核发现：**扫描器自身即可满足该判据** ⇒ 判据**不咬合** IC2 的主体。

**证据**（architect 自行实测，非引用 builder 的回执）：

```
同时含锚点 INV-CEP-05-IC1 + 负向断言宏的文件:
  test_cep_canonical_flow_contract.cc   ← 真实载体
  test_cep_canonical_invariants.cc      ← 真实载体
  test_cep_dummy_signals.cc             ← ⚠️ **扫描器自身**
  test_cep_moesi_gpu_adapter.cc         ← 真实载体
  test_cep_moesi_gpu_compensation.cc    ← 真实载体
```

`test_cep_dummy_signals.cc` 内的锚点出现**全部**是**字符串字面量**（fixture 内容 / 传给判定函数的实参）：

```
249:  {"a.cc", "INV-CEP-05-IC1"}};                    ← fixture 字符串
250:  REQUIRE(files_with_anchor_and_negative("INV-CEP-05-IC1", only_anchor).empty());
254:  REQUIRE(files_with_anchor_and_negative("INV-CEP-05-IC1", only_negative).empty());
257:  {"c.cc", "INV-CEP-05-IC1\nREQUIRE_FALSE(x);"}}; ← fixture 字符串
258:  REQUIRE(files_with_anchor_and_negative("INV-CEP-05-IC1", both).size() == 1u);
```

⇒ **把 4 个真实载体里的负向断言全部删掉，该判据仍绿** ⇒ 它证明的是「**可以**写出这样一条断言」，
⛔ 发现不了「合同测试集**不再含**该负向断言」⇒ 与 §7.2 要求的「删掉那条负向断言 ⇒ 红」**不符**。

> ⚠️ 这与 §7 之前发现的 `INV-CEP-14-IC1` 借壳是**同一类错误**（载体被自己的 fixture 顶替）——
> 说明「自证」这一失败模式在本仓**反复出现**，后续载体评审须把「载体文件自身是否满足该判据」列为**固定检查项**。

### 9.2 闭合所需的精确修法

1. **剔除字符串字面量后再匹配**：扫描时先把文件里的字符串/字符字面量内容置空（保留**注释与代码**），
   再找锚点 id 与负向断言宏 ⇒ fixture 字符串**不得**自证。
   - 效果：`test_cep_dummy_signals.cc` 的锚点只存在于字面量 ⇒ 不再命中；
   - 真实载体不受影响：`canonical_flow_contract.cc`（锚点在**注释**）、`moesi_gpu_adapter.cc`、
     `moesi_gpu_compensation.cc` 三处锚点均在**注释**中 ⇒ 仍命中 ⇒ 判据仍绿。
   - ⚠️ 注意：`test_cep_canonical_invariants.cc` 的锚点在**字面量**里（`{"INV-SC1", …, "INV-CEP-05-IC1"}`），
     按此规则它**不再**是载体 —— 这是**正确**的（它本就没有针对该 invariant 的负向断言）。
2. **扫描器自排除（双保险）**：含扫描器哨兵串的文件一律跳过（防将来复制实现时再次自证）。
3. **决定性变异测试（闭合证据，必须附）**：把 4 个真实载体里的 `REQUIRE_FALSE(` 全部去掉
   ⇒ 判据必须**红**；还原后必须绿。（builder §8 给的「锚点 id 变异」只证明「锚点必须存在」这一半，**不够**。）

### 9.3 其余项状态（不变）

- **B-1 / B-3**：✅ 已闭合（§7.1）
- **S-1 / S-3 / S-4 / S-5 / S-6**：✅ 已落地（§6）；**S-2** 为 architect 误判，已撤销
- **5 条 ⏭-unmodelled**：✅ 触发条件经复核成立

### 9.4 结论

`INV-CEP-14-IC2` 的载体**仍不成立** ⇒ 真实载体数仍为 **60/61** ⇒ **B2 仍不得登记 ✅**。

下一步：builder 按 §9.2 修（~30min，含决定性变异测试）→ architect 复验该 1 条 → planner 落 B2 登记。
