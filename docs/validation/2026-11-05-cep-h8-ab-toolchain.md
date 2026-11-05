# CEP 实验：H8（4 件套 + 13 必纳 metric 足以让跨协议取舍决策可复现）

**日期**:2026-11-05 · **角色**:architect（A/B 维度降级裁决）· builder（装置参数化 + 产出 + 工具链修复）· planner（表落）
**性质**:判定工具链级假设（`obj-cep-platform.md §9.1` 的 H8）
**装置**:既有 3 个实验装置各一对配置（`h6` / `h4` / `h2`，经 `CEP_EXP_VARIANT` 参数化，不新增 `run_hN` 入口）
**关联**:台账 **Entry #224**（H8-A/B-1/2/3 降级裁决 + Task C 路径 + 冻结面授权链）· **#218**（§9.1 改造 + 预登记载体）
· **#222**（H4 形态裁决）· `ADR-CEP-06` **SU#1**（哨兵纪律 / 判据④）· **SU#2**（预登记载体裁决）· **SU#3**（报告层三态）
· `docs/validation/2026-11-05-cep-h4-completion-domain-experiment.md`（同族先例）

---

## 0. 一句话结论

**H8 判 ✅ 成立**（工具链级 / 模型级）—— 3 组 A/B（`h6` 隔离 vs 共享 · `h4` 正确 vs 错标 · `h2` FullMap vs BroadcastNull）
各产出**可读结论**：`ab_compare.py` 给出逐 metric delta 表，`recommend.py` 给出 3 条**建议**（均在**实测面**上成立，
未实测面按 SU#3 显式标注「闸不适用，建议不含该面安全断言」）⇒ **4 件套 + 13 必纳 metric 的取舍决策链可复现**。

⚠️ **本结论是工具链级**（见 §5 口径限定），⛔ **不是**「跨协议取舍已被证明安全」——A/B 两侧是**同一装置的两个配置**，
**不是**两个真协议族（本仓无协议级实验装置，per Entry #224 ⑤ A/B-1）。

---

## 1. 判据来源（逐字）

| 假设 | 验证手段 | 成功判据 |
|---|---|---|
| **H8** | 4 件套 + 13 必纳 metric + `tools/recommend.py` 输出建议 | **3 组 A/B 对比产出可读结论** |

> ⚠️ 原表未写明 A/B 两侧是什么 ⇒ 由 Entry #224 ⑤ 的降级裁决补齐：
> **A/B-1** A/B = 同一装置的两个配置（⛔ 非两个协议族）· **A/B-2** 3 组 = 3 装置各一对（见 §2）·
> **A/B-3** 变体**不进 `exp_id`**（`CEP_EXP_VARIANT` 环境变量）⇒ drift 闸白名单正则不变。

---

## 2. A/B 形态与装置参数化

| 组 | 装置 | A 侧（baseline） | B 侧（target） | 取舍维度 |
|---|---|---|---|---|
| **1** | `h6`（VC 模型） | `iso` = VC 隔离（默认 = 既有行为） | `shared` = VC 共享 | **死锁风险 vs 带宽效率** |
| **2** | `h4`（完成域模型） | `correct` = requester-scoped（默认 = 既有行为） | `mislabel` = 错标 group-scoped | **误分类的代价** |
| **3** | `h2`（目录策略） | `fullmap` = FullMap（集中） | `broadcast` = BroadcastNull | **目录开销 vs 一致性覆盖** |

**实现方式**（per Entry #224 ⑤ A/B-3）：`CEP_EXP_VARIANT` 由 dispatch 读取后**以参数**传入 `run_hN`，
默认参数 = 既有行为（**不设变体 ⇒ 产出与改动前逐字节一致**）：
- 变体只切换**代表值归属**（metrics 槽位 = 「本 run 的被试配置」），双模型装置本身不变；
- `hypothesis_id` 仍由 `CEP_EXP_ID` 承载 ⇒ 变体 run 绑**本组装置**的预登记行（`H6` / `H4` / `H2`）⇒
  `not-collected ⊆ 预登记 out-of-scope` 自动绿（per Entry #224 ⑤ A/B-1「共享该假设的预登记 ⇒ 无需新增登记」）。
- ⚠️ dispatch 保持 `r = run_hN(...)` 字面形态 —— drift 闸按 `=\s*run_(h\d+)\s*\(` 提取白名单，
  三元表达式会**静默漏检**（Entry #224 ⑤ 已警示的失败模式）。

---

## 3. 三组结果

### 3.1 组 1 —— `h6`：VC 隔离 vs 共享

| metric | A（iso） | B（shared） | 判读 |
|---|---:|---:|---|
| `deadlock_violations` | **0** | **1** | B 实测死锁 ⇒ 被淘汰 |
| `total_transactions` | 4000 | 2000 | B 服务吞吐减半 |
| `transactions_per_cycle` | 2 | 1 | 同上 |
| `collective_stalls` | 0 | **2 001 000** | B 的 collective 持续占 coherence VC 等待 |
| 延迟族 / `memory_violations` | `not-collected` | `not-collected` | 装置不采（per 预登记） |

**`recommend.py`**：`推荐 A(baseline) —— target 存在实测硬性违例(deadlock_violations=1) ⚠️ 硬性闸未实测(memory_violations) —— 闸不适用,建议不含该面安全断言`（exit 0）

### 3.2 组 2 —— `h4`：完成域正确 vs 错标

| metric | A（correct） | B（mislabel） | 判读 |
|---|---:|---:|---|
| `deadlock_violations` | **0** | **1** | 错标 ⇒ 循环等待 ⇒ 死锁（`ADR-CEP-01 §6.2` 代价 3） |
| `total_transactions` | 16 | 16 | 两侧发起量相同（差异在**完成**而非发起） |
| `collective_stalls` | 0 | 0 | 隔离 VC（带宽充足）下仍死锁 ⇒ **排除带宽竞争解释** |

**`recommend.py`**：`推荐 A(baseline) —— target 存在实测硬性违例(deadlock_violations=1) ⚠️ 硬性闸未实测(memory_violations) —— 闸不适用,建议不含该面安全断言`（exit 0）

### 3.3 组 3 —— `h2`：FullMap vs BroadcastNull

| metric | A（fullmap） | B（broadcast） | 判读 |
|---|---:|---:|---|
| `avg_latency_cycles` | **4** | **18** | B 退化 **350%**（> 10% 阈值）⇒ B 被淘汰 |
| `total_transactions` | 200 | 200 | 同工作量 |
| 违例类（deadlock / memory） | `not-collected` | `not-collected` | 装置不采（per 预登记） |

**`recommend.py`**：`推荐 A(baseline) —— target 延迟退化 350.0% > 10.0% ⚠️ 硬性闸未实测(deadlock_violations, memory_violations) —— 闸不适用,建议不含该面安全断言`（exit 0）

### 3.4 报告层三态（SU#3）在 6 个 run 上的实测

| run | `render_report --check` | 渲染态 | 说明 |
|---|---|---|---|
| g1a / g2a / g3a / g3b | 绿 | ⚪ **一致性闸门不适用**（g1a/g2a）；⚪（g3a/g3b） | 违例类含 `not-collected` ⇒ 闸不适用（⛔ 非「通过」） |
| g1b / g2b | **红（预期）** | ❌ **一致性违例(实验 fail)** | 实测死锁=1 = 被淘汰配置的实测证据，**不是**装置缺陷 |

⇒ 「**预期红因正确原因**」由脚本断言（输出须含「死锁违例 / 一致性违例」字样，见 §6）。

---

## 4. 工具链层修复（本轮实质改动，均在 Entry #224 ⑥ 授权链内）

### 4.1 `tools/recommend.py` 两处**哨兵值缺陷**（per SU#1 §⑤）

| # | 缺陷 | 触发 | 修复 |
|---|---|---|---|
| 1 | `hard_ok()` 直接 `v <= max` —— `"not-collected"` 与 int 比较 ⇒ **TypeError**（exit 1） | 任一侧 `deadlock_violations` / `memory_violations` 未实测（h6 / h4 / h2 装置**全部**命中） | `_gate_state()` 3 态分解：**违例**（实测超阈）> **未实测**（`not-collected`，闸不适用）> **通过**（实测达标） |
| 2 | 延迟面 `b_lat - a_lat` —— `avg_latency_cycles == "not-collected"` ⇒ **TypeError** | h6 / h4 变体（不采延迟） | `_num()` 归一：非数值 ⇒ 视为未实测 ⇒ 不显著 + 注脚 |

**语义（per SU#3 勘误：违例 > 不适用 > 通过）**：
- 实测违例 ⇒ 该侧**不合格**，推荐另一侧（其余未实测面在文本中**显式标注不背书**）；
- 双侧均无实测违例 ⇒ 按实测延迟面比较；未实测面**不构成**肯定断言（文本强制带「闸不适用」注脚）。

### 4.2 装置参数化（`test/test_cep_experiment_emit.cc`）

- `run_h2/run_h4/run_h6` 增 `variant` 参数（默认 = 既有行为）；dispatch 读 `CEP_EXP_VARIANT` + **逐装置白名单**
  （越界 `FAIL`，⛔ 不静默忽略）。
- 新增 `[cep][exp][variant]` 单测：3 组 × 双变体，含「默认路径零变化」断言（不设变体 ⇒ scope 无变体标签、
  `deadlock=0`、h2 代表值 = dcoh 14）。

---

## 5. 口径限定（⚠️ 必须随任何引用重复）

1. **工具链级 / 模型级**，非协议级 —— 是**工具链**在**模型装置**上的取舍决策链，⛔ 不得表述为「跨协议已验证」。
2. **A/B 两侧 = 同一装置的两个配置** —— 不是两个真协议族（本仓无协议级实验装置，per Entry #224 ⑤ A/B-1）。
3. **共 3 组、3 个装置** —— 组 1（VC）、组 2（完成域）、组 3（目录）各一对；**不覆盖**带宽 1:4 / 注入倾斜等其它维度
   （工具链不依赖具体维度，但本结论的**证据面**仅此 3 组）。
4. **`recommend.py` 的 exit 0 = 「有明确推荐」**，⛔ **不等于**「已证明安全」—— 未实测面（memory / 延迟等）
   在建议文本中显式标注「闸不适用」；`bounded_random` 的结论不构成「已证明无死锁」（per `ADR-CEP-06 §3`）。
5. **`g1b` / `g2b` 的 `--check` 红是预期 verdict** —— B 侧就是**被淘汰配置**，其 4 件套完整产出、
   违例横幅正确渲染；该红**不是**装置缺陷，也**不是**闸失效。
6. **`trace.h5` 是 run 级摘要**（`grain = "run-summary"` + `exp_variant` 标注），⛔ 不冒充 cycle-level trace。

---

## 6. 反向 fixture（判据「因正确原因红」的实测）

| # | fixture | 实测 |
|---|---|---|
| 1 | 双侧 hard 闸 `not-collected` + 延迟未实测（h6/h4 形态） | 修复前 **TypeError exit 1**；修复后 **exit 3** + 「闸不适用」注脚（`[cep][tools]` SECTION） |
| 2 | 双侧 hard 闸 `not-collected` + 延迟 4 vs 18（h2 形态） | **exit 0** + 「推荐 A」+ 注脚（哨兵不得把未实测当违例） |
| 3 | B 侧实测 `deadlock_violations=1` + A 侧未实测 | **exit 0** + 「推荐 A」+ 「实测硬性违例(deadlock_violations=1)」+ 注脚 |
| 4 | `g1b` / `g2b` 的 `--check` | **红因含「死锁违例 / 一致性违例」字样**（脚本断言）；若红因变为其它（如文件缺失）⇒ 脚本判失败 |
| 5 | 变体白名单越界（如 `CEP_EXP_VARIANT=shared` + `CEP_EXP_ID=h2`） | dispatch **FAIL**（⛔ 不静默忽略） |

---

## 7. 4 件套链路与复现

```bash
cmake --build build -j4 --target cpptlm_tests
./build/bin/cpptlm_tests "[cep][exp][variant]"          # 变体单测(3 组 × 双变体 + 默认路径零变化)
./build/bin/cpptlm_tests "[cep][tools]"                 # 含 3 条哨兵 fixture
bash scripts/test/cep_prereg_drift_check.sh             # 预登记两闸(§9.1 ② ↔ JSON)
bash scripts/test/cep_h8_ab_runs.sh                     # 6 run + 3 组 ab_compare + recommend
# → experimental/run-<ts>-h8-ab/{g1a,g1b,g2a,g2b,g3a,g3b}/ 各 4 件套
#   + compare-g1a-vs-g1b/ / compare-g2a-vs-g2b/ / compare-g3a-vs-g3b/(delta_metrics.json + ab_compare.html)
#   + conclusions.txt(3 条建议原文)
```

---

## 8. Verdict

| 假设 | verdict | 计数影响 |
|---|---|---|
| **H8** | ✅ **成立**（工具链级 / 模型级，per §5 口径限定） | §6.2 **B7** 计数 **+1** |

⇒ **B7 计数由 7/8 → 8/8**（H1 / H2 / H3 / H4 / H5 / H6 / H7 + 本次 H8）。

⚠️ **B4 仍未达成**（详情见 §9）—— ⛔ 本实验**不构成** B4 的达成证据：B4 明文要求「违例类
（`deadlock_violations` / `memory_violations`）**实测为 0**」，而本组 6 run 的违例类存在
`not-collected` 面（h6/h4 侧 `memory_violations`、h2 侧两违例类均未采集）⇒ **不得**计入 B4。

---

## 9. 本轮顺带暴露的结构性发现（B4 的可达成条件当前不可达）

**事实（三层实测）**：

| 装置族 | `deadlock_violations` | `memory_violations` | 违例类双实测 |
|---|---|---|---|
| `h2`（目录策略） | `not-collected` | `not-collected` | ❌ |
| `h3`（CONSERVATIVE 完备性） | `not-collected` | **实测**（`swmr` + `data_value` 双分桶） | ❌ |
| `h4`（完成域） | **实测** | `not-collected`（模型无内存一致性语义） | ❌ |
| `h6`（VC 隔离） | **实测** | `not-collected`（同上） | ❌ |

⇒ **没有任何装置族同时实测两个违例类** ⇒ 按 B4 的现行口径
（「违例类必须实测为 0」「违例类为 `not-collected` 的 run 不计入」），**B4 当前不可达成** ——
这**不是**「未开始」，而是**结构性不可达**。

**已排除的错误处置**（⛔ 不得静默二选一）：
- ⛔ 「给 h6/h4 挂 `SwmrDataValueDetector` 凑 0」—— 两装置**无内存一致性语义**，挂检测器 = 恒真的假绿
  （在无 SWMR 状态的模型上「检出 0 违例」= 未检验任何东西，与 H3 已排除的空检查同族）；
- ⛔ 「援引 h3 的 `memory=0` + h6 的 `deadlock=0` 拼接成『两者都实测 0』」—— 违反 B4
  「**不继承 pass**」的明文（跨 run 继承 = 把两个 run 的证据面拼成第三个未产出的 run）。

**路由**：需 **architect** 裁决二选一（builder 不自行拍板）——
① 新增一个**同时具备两种违例语义**的实验装置族（新装置 = 决策层，须预登记 + 入口白名单）；或
② 重裁 B4 口径（如按「每违例类各由一族实测为 0」计 —— 属 B4 语义修改，须 ADR/objective 评审）。

⇒ 本发现登记于台账 **Entry #225**；在裁决前 **B4 维持 ⏳ 未达成**。
