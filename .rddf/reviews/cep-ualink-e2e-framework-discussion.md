# 讨论纪要 · CEP 端到端可测框架（甲方案）· 决策清单与 D-1 结论

- **性质**:architect ⇄ planner 的**讨论纪要**(过程产出)—— ⛔ **不是** ADR / 决策文档;按用户要求「**所有关键决策先一一讨论,统一后再生成架构文档和决策文档**」⇒ 全部 10 条统一前**不落** `ADR-CEP-05` / `ADR-CEP-19` / `07 号` 的任何段落。
- **范围**:「甲」= 把「完备的有流量发生器和接收器的端到端仿真框架」收敛为「**CEP 端到端可测**」⇒ 复用仓内已有的 `TrafficGenTLM` / 通用域 E2E 框架 / 4 件套工具链 / 实验装置层,只补**接缝 + 拓扑 + 断言 + 产出**。
- **起点**:台账 Entry #287 的 N-2(architect 裁决 Entry #282 之上)· Entry #285 的架构文档审查。

## 一、决策清单(10 条 · 分层)

| 层 | # | 决策 | 状态 |
|---|---|---|---|
| 已裁决 | K-1 | 形态 = 组合式 wrapper(`StubCacheEndpointTLM` 持有 `StubCacheCore`) | ✅ Entry #282 |
| 已裁决 | K-2 | 映射面归属 = wrapper 内部 | ✅ Entry #282 |
| 已裁决 | K-3 | 注册与命名 = `registerObject` + `registerDualPortAdapter` / `StubCacheEndpointTLM` | ✅ Entry #282 |
| 已裁决 | K-4 | change 归属 = 收编 `cpptlm-cep-stub-cache-chstream-surface` | ✅ Entry #282 |
| **1** | **D-1** | **可观测面与 13 必纳 metric 的来源** | ✅ **已裁**(见 §二) |
| **1** | **D-3** | 引擎可观测面的扩展边界 | 🔄 **讨论中**(第 2 轮) |
| **1** | D-2 | 双向映射的对侧语义(TrafficGen 的 `resp_in_` 有回) | ⏳ 待议 |
| **2** | D-4 | 「接收器」的角色(对端引擎 vs 新建 Sink) | ⏳ 待议 |
| **2** | D-5 | 拓扑形态(直连 vs 经 `cohif_noc` 桥 + 网络) | ⏳ 待议 |
| **2** | D-6 | 冻结 spec `cep-stub-cache-core` `:56` 的 MODIFIES 精确措辞 | ⏳ 待议 |
| **2** | D-7 | TrafficGen 模式与可复现性(TRACE/SEQUENTIAL/RANDOM 种子/hotspot/stride) | ⏳ 待议 |
| **3** | D-8 | 「测 CEP 的功能」的具体判据清单 | ⏳ 待议 |
| **3** | D-9 | 实验产物规范(命名/编号/入 `experimental/` 与台账) | ⏳ 待议 |
| **3** | D-10 | 计数纪律再确认(⛔ 框架做完后 `B3b-2` 仍 0/4) | ⏳ 待议 |

**建议讨论顺序**:D-1 → **D-3** → D-2 → D-4 → D-5 → D-6 → D-7 → D-8 → D-9 → D-10

## 二、D-1 结论(用户已裁 + 取证修正)

### ① 用户裁定的 3 点

| # | 裁定 | 状态 |
|---|---|---|
| 1 | **latency 的测量点 = 发起端(wrapper)** | ✅ 与既有 `LatencyCollector` 的「issue/complete 周期戳」形态**完全同型** ⇒ **零新产出逻辑** |
| 2 | **5 个违例/停滞类 = 本轮全部产出** | ⚠ **取证后需精确化**(见 ③) |
| 3 | **「完备」= 全绿** | ⚠ **取证后需精确化**(见 ③) |
| — | **本轮产出定位 = ③ 两者都要**(判据 + 实验产物) | ✅ 已采纳 |

### ② ⚠️ 取证修正:13 键的**产出装置已存在**(本轮最重要的发现)

`test/cep_experiment_harness.hh`(2026-11-04,「CEP 实验装置的观测层(最小可信形态)」)已提供:

| 组件 | 能力 | 对应 metric |
|---|---|---|
| **`LatencyCollector`** | issue / complete **周期戳** → **13 必纳 metric 的 9 项分布** | `total_cycles` · `total_transactions` · `transactions_per_cycle` · `avg/p50/p95/p99/max_latency_cycles` |
| **`SwmrDataValueDetector`** | 运行时 **SWMR + data-value 双不变量**(`INV-CEP-06-IC5`) | `memory_violations` |
| **`ProgressWatchdog`** | 「N cycle 无事务 retire 且 in-flight 非空」**死锁判定** | `deadlock_violations` |

且 harness 内已有 `coherence_stalls` / `directory_misses` / `total_cycles` 字段。

⚠️ **两条硬事实**(决定后续所有设计的边界):
- harness **住在 `test/`**,⛔ **不**在 `include/tlm/cep/` ⇒ 其自述依据:「**实验装置不得升格为契约面**」(per `examples/cep_poc_spike_s3.cc` 的 spike 硬边界先例)⇒ ⇒ **wrapper(`include/tlm/cep/`)⛔ 不得依赖 harness**;
- harness 自述:「`include/` + `src/` 里**零产出者**(`avg_latency_cycles` / `deadlock_violations` / `vc_stall_per_vc` 均 grep 零命中)」⇒ ⇒ **metric 产出面在仓内是空白**,只有**实验装置层**填它。

### ③ 由此产生的**两个精确化问题**(需在 D-3 一并裁定)

**P-1 · 分层归属**:「latency 测发起端(wrapper)」+「harness 住在 `test/`」⇒ **wrapper 不能自己算 latency**(否则 = 实验装置升格进契约面)⇒ ⇒ **正确分层 = wrapper 只暴露原始观测事实(per-txn 的 issue / complete 周期戳 + 违例检出结果),由 harness 消费并聚合** ⇒ ⇒ ✅ **零基类改动、零既有引擎波及**。

**P-2 · 「全绿」的准确定义**:13 键中 `collective_stalls` / `directory_misses` 在 **UALink 场景下无对应载体**(无 collective plane / 无目录)⇒ ⇒ **「13 键全部有值」在 UALink 上不可达** ⇒ ⇒ 「全绿」须表述为:
> **双违例类真实为 0**(`deadlock_violations == 0` ∧ `memory_violations == 0`,per `tools/render_report.py` 的 IC3 判据)+ **其余 11 键有值或按预登记 `not-collected`**(且 `not-collected ⊆` H3 预登记集合,per `cep_prereg_drift_check.sh`)

⇒ ⇒ **「全绿」= IC3 一致性闸门绿 + run-validity 绿**,⛔ **不是**「13 键全部有值」(后者对 UALink 不可达,且会诱发 `not-collected` 滥用)。

## 三、本讨论的产出清单(⛔ 尚未落任何架构文档)

| 产出 | 状态 |
|---|---|
| 决策清单(10 条,分层 + 状态) | ✅ 本文件 |
| D-1 结论 + 3 项用户裁定 | ✅ 本文件 |
| 取证修正(产出装置已存在 / 分层约束 / 「全绿」定义) | ✅ 本文件 |
| D-3 ~ D-10 结论 | ⏳ 待逐条讨论 |
| **架构文档 + 决策文档**(`ADR-CEP-05` / `ADR-CEP-19` / `07 号` 的 SU) | ⏸ **待 10 条全部统一后**由 architect 落(需 oracle → critic) |

**关联**:台账 Entry #287(下一步任务推荐)· **#282**(4 条已裁决)· **#285**(架构文档审查)· `test/cep_experiment_harness.hh`(产出装置)· `tools/render_report.py`(IC3 判据)· `experimental/metrics.schema.json`(13 键)
