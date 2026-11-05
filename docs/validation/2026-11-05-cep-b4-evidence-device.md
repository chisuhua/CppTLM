# CEP 实验：H3 v2 装置（「一致性 × 完成域」复合）—— B4 达成证据载体

**日期**:2026-11-05 · **角色**:builder（装置实现 + 产出 + 工具链）
**性质**:装置级验证 + B4 达成证据形态（`ADR-CEP-06` **SU#4** 的落地）
**装置**:新建 `test/cep_consistency_completion_model.hh`（复用 h4 完成域引擎 + h3 一致性件 + 既有 harness 检测器）
**关联**:台账 **Entry #226**（`scope-change`：预登记收缩 + 冻结授权 + 授权先行）· **#225 ⑧**（B4 结构性不可达的发现与路由）
· improvement `cep-b4-evidence-device`（AC-1a ~ AC-10）· `ADR-CEP-06` **SU#4**（裁决 + 六条非空洞性约束）· **SU#1 §④**（B4 读取约束）· **SU#3**（报告层三态）
· `docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md`（v1 证据，本文的重证对象）

---

## 0. 一句话结论

**H3 v2 装置成立** —— 单个 run 内 **`deadlock_violations` 与 `memory_violations` 同时实测**（均 = 0），
且装置合格性四条件全绿（死锁检测力 / 内存检测力 / 同一事务流 / watchdog 非空洞）；
`report.html` 渲染 **「✓ 一致性闸门通过」** ⇒ **该态的首个真实实例**（此前仅由合成 fixture F1 覆盖）。

⚠️ **B4 的回填仍待一道闸**：**AC-9 architect 实现评审**（装置含真实阻塞机制、非「恒假 watchdog + 默认 0」）——
本结论**不预先宣告** B4 达成（per SU#4 §⑤ 末项 + improvement AC-9）。

---

## 1. 判据来源（逐字）

| 来源 | 判据 |
|---|---|
| `ADR-CEP-06` SU#4 §② ①~⑥ | 在途窗口真实存在 · 双检测器各自被负控证明 · 0 是实测 0 · 不得以空检查充数 · **同一事务流**（实例级观测键）· **耦合探针**（阻塞开/关） |
| SU#4 §① | B4 达成证据形态 = 单 run 两违例类实测 0 + run-validity 绿 + `--check` 绿 + 渲染「✓ 通过」 |
| improvement AC-1a/1b ~ AC-10 | 逐条机器锚（见 §6 映射表） |

---

## 2. 装置（新建 1 文件 + 复用 3 件既有测试件）

| 件 | 复用 | 作用 |
|---|---|---|
| `cep_cd::CompletionDomainModel` | h4 装置（**已验证的循环等待**） | 完成域语义 + 死锁机理（错标 ⇒ 循环等待） |
| `SwmrDataValueDetector` / `DcohSymmetric` / `HomeNodeModel` | h3 装置同款 | SWMR + data-value 不变量 + 漏探(superset)判定 |
| `cep_exp::ProgressWatchdog` | harness | 活性判定（in-flight 非空 且 连续 N cycle 无 retire） |
| **新建** `cep_cc::ConsistencyCompletionModel` | — | **编排 + 身份仪表化**:周期化事务流 + 真实在途窗口 + 实例级 `(txn_id, addr, node)` 身份 + 观测键导出 + 合格性判定 |

⚠️ **不是「把两件实验穿一件风衣」**：承载内存不变量检查的事务**同时**受完成域阻塞规则支配 ⇒ 约束 5 以
**实例级**观测键交集判定（内存操作实例 ⊆ 受阻塞支配实例 ⇒ 覆盖率 = 1.0）；退化形态（内存操作挂独立实例流）
⇒ 覆盖率 = 0 ⇒ **拒绝产出**。

---

## 3. 实测结果

### 3.1 正控（1200 coherence 事务）

| 指标 | 实测 | 判据 |
|---|---:|---|
| `deadlock_violations` | **0** | ✅ **实测**（非 `not-collected`） |
| `memory_violations` | **0**（`swmr=0` / `data_value=0`） | ✅ **实测** |
| 漏探(superset) | **0** | ✅ 分支 1 重证（v2 同参数） |
| `total_transactions` / `total_cycles` | 1200 / 2397 | — |
| `transactions_per_cycle` | 0.500626 | — |
| 在途峰值 / 停滞上限 | **3** / 2 | ✅ 真实在途窗口（`max_in_flight > 1`）；停滞 < 阈值 200 ⇒ 未误判 |
| 同一事务流覆盖率 | **1.000000**（1200/1200 实例） | ✅ 实例级交集成立 |
| 地址级重叠 | **是** | ✅ 同流同地址空间 |
| 延迟族（avg/p50/p95/p99/max） | 2 / 2 / 3 / 3 / 3 | ⚠️ **语义 = 在途窗口**（v2 口径,v1 是 lookup 成本,⛔ 不可比） |

### 3.2 三负控（检测力证明）

| 负控 | 注入 | 实测 |
|---|---|---|
| ① 错标完成域 | coherence = group-scoped | **死锁 = 1**（watchdog 与完成域模型同步检出）；coherence 退休数 = 0 ⇒ 内存检查未执行 ⇒ 负控**不提供** memory 证据 |
| ② SWMR 注入 | 未先失效既有 sharer 的写 | `swmr = 1197` |
| ③ 过期读注入 | 读观察到过期版本 | `data_value = 1199` |

⇒ 装置合格性四条件：**①死锁检测力 OK · ②内存检测力 OK · ③同一事务流 OK · ④watchdog 非空洞 OK**。

### 3.3 耦合探针（阻塞开 / 关）

| 桶 | 阻塞开 | 阻塞关 | Δ | 预测 |
|---|---:|---:|---:|---:|
| `swmr` | 0 | 0 | **+0** | +0 ✅ |
| `data_value` | 0 | 0 | **+0** | +0 ✅ |

⇒ 预测锚 `experimental/predictions/h3_coupling_v2.json`（时间戳 + 实现 commit `a3384a86`；探针校验其为 HEAD 祖先且锚 mtime ≤ run mtime ⇒ **事后补写预测不成立**）。
⇒ **灵敏度**（防「双 0 空过」）：SWMR 注入在阻塞开/关**两配置下均被检出 > 0**（`[cep][b4]` 单测）。

### 3.4 既有证据未被扰动

| run | 与 v2 落地前的 13 必纳 + 声明集对比 |
|---|---|
| `h2` / `h4` / `h6` | **逐项不变** ✅（`cep_completion_domain_model.hh` 新增的阻塞开关**默认 true** ⇒ h4 证据链零影响） |
| `h3` | 变更即设计意图：`deadlock_violations` 由 `not-collected` → **实测 0**;延迟语义改为在途窗口;`_not_collected` 由 4 项收缩为 3 项（与预登记收缩一致） |

---

## 4. 与既有 H3 结论的关系（分支 1 的重证）

- `§9.1 ①` H3 **分支 1**（`memory_violations == 0`,1200 事务,swmr=0 / data_value=0）与 **分支 2**（`deadlock_violations == 0`）
  由 **v2 run 一次性重证**（分支 1 数值不变:swmr=0 / data_value=0 / 漏探=0;分支 2 首次实测 0）。
- ⚠️ **v1 装置形态不保留为可跑变体**（其 `deadlock=not-collected` 与收缩后的预登记互斥,per SU#4 §① 流程闸）⇒
  可追溯性由**重证**承担（不靠归档旧产物）;`docs/validation/2026-11-04-cep-h2-h3-h6-experiments.md` **只勘误不重跑**。

---

## 5. 口径限定（⚠️ 必须随任何引用重复）

1. **模型级**，非协议级 —— 是**模型**上的一致性 × 完成域交互结论，⛔ 不得表述为「已证明协议层无死锁 / 无违例」。
2. **正控的 0 是「本模型无该形态」的 0** —— 负控证明检测力活着（错标 ⇒ 死锁=1;注入 ⇒ 违例>0），
   但 ⛔ 不代表真实硬件不会死锁。
3. **延迟语义 = 在途窗口**，与 v1（lookup 成本）⛔ **不可比**;跨版本比较 latency 无效。
4. **`verification_scope` 属自证面家族**（SU#2 §①）—— 机器锚 = `[cep][b4]` 单测 + `cep_b4_observation_keys_check` /
   `cep_b4_coupling_probe` 两道 ctest 闸;散文仅指针。
5. **观测键是本装置特有**（附加键 `observation_keys`）—— ⛔ 不在 13 必纳 metric 契约面;13 键语义零变。
6. **B4 尚未回填** —— AC-9 architect 实现评审是前置 gate;本结论⛔ 不宣告 B4 达成。

---

## 6. AC ↔ 实测映射

| AC | 实测证据 |
|---|---|
| AC-1a | 预登记双源收缩 + 冻结 `--update --authorized-by "Entry #226"`(commit `380208f0` / `3f5e99ad`);drift rc=0 |
| AC-1b | h3 v2 run 的 `not-collected` = {coherence_stalls, collective_stalls, directory_misses} ⊆ H3 预登记 ✅ |
| AC-2 | `deadlock_violations=0` / `memory_violations=0` 且两者在 `_measured` 内 ✅ |
| AC-3 | 三负控各自红过（死锁=1 / swmr=1197 / data_value=1199）;负控不红 ⇒ `EmitResult.fatal` ⇒ 拒绝产出 ✅ |
| AC-4 | 正控 `max_in_flight=3 > 1` 且 `max_idle_run=2 < 200` ✅ |
| AC-5 | 实例级覆盖率 **1.0**(1200/1200);落 metrics.json **且** trace.h5;退化形态 ⇒ 覆盖率 0 ⇒ 拒绝产出 ✅ |
| AC-6 | 探针 Δ=0/Δ=0 与预测一致;锚前置(commit 祖先 + mtime 序);灵敏度单测 ✅ |
| AC-7 | `--check` 绿 + `report.html` 含「✓ 一致性闸门通过」(首个真实实例)✅ |
| AC-8 | `device=v2` 标签在 scope;v2 同参数重证分支 1(swmr=0 / data_value=0 / 漏探=0)✅;2026-11-04 文档只勘误 ✅ |
| **AC-9** | ⏳ **未完成** —— architect 实现评审(需切 architect);**B4 回填的前置 gate** |
| AC-10 | 退化形态 fixture:`degenerate_expect_fatal` ⇒ emit fatal 且**未写任何文件** ✅ |

---

## 7. 复现

```bash
cmake --build build -j4 --target cpptlm_tests
./build/bin/cpptlm_tests "[cep][b4]"                 # 5 cases / 45 assertions
bash scripts/test/cep_experiment_run.sh              # 官方 4 组 + h3 文件层观测键断言
ctest --test-dir build -R 'cep_b4'                   # 观测键闸 + 耦合探针闸
bash scripts/test/cep_b4_coupling_probe.sh           # 探针(锚前置校验)
```

---

## 8. Verdict

| 项 | 状态 |
|---|---|
| H3 **分支 2**（`deadlock_violations == 0`） | ✅ **实测 0**（v2 run;此前 ⏭ 未测量）—— 表落待 planner |
| H3 **分支 1** | ✅ v2 同参数**重证**(swmr=0 / data_value=0 / 漏探=0) |
| IC3 三态「✓ 通过」 | ✅ 获得**首个真实 run 实例** |
| **B4** | ⏳ **未回填** —— 待 **AC-9 architect 实现评审** ⇒ 之后由 planner 落表 |
