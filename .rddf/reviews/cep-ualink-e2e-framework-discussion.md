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
| **1** | D-2 | 双向映射的对侧语义(TrafficGen 的 `resp_in_` 有回) | ✅ **已裁**(见 §五) |
| **2** | D-4 | 「接收器」的角色(对端引擎 vs 新建 Sink) | ⏳ 待议 |
| **2** | D-5 | 拓扑形态(直连 vs 经 `cohif_noc` 桥 + 网络) | ✅ **已裁**(见 §六) |
| **2** | D-6 | 冻结 spec `cep-stub-cache-core` `:56` 的 MODIFIES 精确措辞 | ✅ **已裁**(见 §六) |
| **2** | D-7 | TrafficGen 模式与可复现性(TRACE/SEQUENTIAL/RANDOM 种子/hotspot/stride) | ✅ **已裁**(见 §六) |
| **3** | D-8 | 「测 CEP 的功能」的具体判据清单 | 🔄 **讨论中**(第 5 轮 · 验收层打包) |
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

## 三、D-3 结论(用户已裁 · 第 2 轮)

| # | 裁定 | 状态 |
|---|---|---|
| 1 | **wrapper 暴露公开观测 API**(原始事实,**不做统计**) | ✅ 已裁 ⇒ **零基类改动、零既有引擎波及** |
| 2 | 违例类在**不适用**场景 ⇒ `not-collected` + **显式登记降格**(⛔ 不得默许) | ✅ 已裁 |
| 3 | 第一批「全绿」实验目标族 = **CHI RN-F**(有回响应 + 有并发写);**UALink 留后续** | ✅ 已裁 |

**裁定 1 的边界(由 D-1 §② P-1 推导)**:wrapper 的公开面只多**原始事实**(per-txn issue/complete 周期戳 · in-flight 计数 · SWMR 可见的写序/写值)⇒ **不做** latency 聚合、不做 metric 组装 ⇒ ⇒ **不违反**「实验装置不得升格为契约面」红线(`cep_experiment_harness.hh` 自述依据)⇒ ⇒ 聚合仍由 harness(`test/`)消费。

**裁定 2 的强制项**:降格须**显式登记**(预登记 out-of-scope 集 `experimental/preregistration.json` + `cep_prereg_drift_check.sh` 校验)⇒ ⛔ **不得**用 `not-collected` 掩盖「本可测但没测」。

**裁定 3 的连带结论**:⚠ `deadlock_violations` 的 `ProgressWatchdog` 判据在**零发射**引擎上会**假阳性触发**(请求永不完成 ⇒ 「N cycle 无 retire 且 in-flight 非空」成立)⇒ ⇒ **零发射引擎的链路 ⇒ 该键 `not-collected`** ⇒ ⇒ **第一批选 CHI RN-F 正是为了避开该陷阱**(CHI 有 completion 路径 ⇒ 请求会 retire)。

## 四、D-2 的取证底料(第 3 轮讨论用)

```
① CacheReqBundle  :transaction_id · parent_id · fragment_id/total · address · size · is_write · data
   CacheRespBundle :transaction_id · parent_id · fragment_id/total · data
   ⚠ **无 status / 无 hit-miss 标志 / 无 error 标志**
② TrafficGen 循环 :「resp_in_ 有则收;completed_<N 且 issued_<N ⇒ 继续发」⇒ **不等响应**也能发满 N 个
   ⇒ ⛔ hit 路径无响应**不会卡住**它;但 completed_ 少计 ⇒ **latency 样本会缺命中项**
③ CHI 响应面      :ChiOpcode::Comp / CompData / DataSepResp ⇒ CanonicalFlow::Completion ⇒ **有回响应路径** ✅
④ stub hit 路径   :cpu_read 返回 AccessResult{hit,data,txn_id,pending} ⇒ **hit 时无 net-side request**(INV-SC6 零网络流量)
```

> ⚠️ **勘误(2026-11-08 critic NIT)**:§五 ④「TrafficGen 默认 `fragment_total=1`」**有误** —— `issueRequest()` **从不写** fragment 字段,`ch_uint` 存储默认 `value_ = 0` ⇒ **实发 `fragment_total = 0`** ⇒ ⇒ D-2 ④ 的「透传」前提为「**`fragment_total ≤ 1`**」(⛔ 不依赖 =1)。

## 五、D-2 结论(用户已裁 · 第 3 轮 · 四点全同意)

| # | 裁定 | 关键边界 |
|---|---|---|
| ① | **命中路径也回一个「CPU 侧响应」** | wrapper 在 `cpu_read` 返回 `hit=true` 时**就地生成** `CacheRespBundle` ⇒ 「一请求一响应」成立 ⇒ latency 样本完整;✅ **零网络流量保持**(零**网络** ≠ 零 **CPU 侧**响应)⇒ 不违反 `INV-SC6` |
| ② | **响应侧映射归 wrapper**(与 K-2 一致) | `CohIfBundle(completion) → CacheRespBundle{transaction_id ← 同 txn_id, data ← completion.data}`;wrapper 的 CohIf 侧**有 egress**(回 NoD 响应)⇒ ⚠ **必须显式写明边界**:「**承载层回 NoD 响应 ≠ 协议引擎发射**」⇒ 否则会被读成「引擎开始发射」而违反 M-6 |
| ③ | **`evict_line` 由装配夹具显式调用** | 与 G-0e ② 同向(「保留 core-side 功能方法 + 测试/装配夹具显式调用」)⇒ ✅ **不改通用域 `CacheReqBundle`**;⚠ 本轮 `INV-SC*` 的 eviction 约束**靠夹具驱动**测,不由 TrafficGen 驱动 |
| ④ | **分片只透传、不聚合** | TrafficGen 默认 `fragment_total=1` ⇒ wrapper 透传;CHI 多拍响应(`CompData`/`DataSepResp`)的**聚合语义本轮不做** ⇒ 须在契约面**登记该限制** |

**① 的直接收益**:`CacheRespBundle` 无 status/hit-miss 标志 ⇒ TrafficGen 只认「一请求一响应」配对 ⇒ 命中也回 ⇒ 配对恒成立 ⇒ latency 分布**不系统性偏慢**。

## 六、D-4 ~ D-7 结论(用户已裁 · 第 4 轮 · 形态层打包)

| # | 裁定 | 关键边界 |
|---|---|---|
| **D-4** | **不新建 Sink**(后置为 `tools/ab_compare.py` 的 baseline 侧)+ wrapper **单一模块双角色**(CohIf 接缝 + CPU 侧 NoD 响应) | 接收语义已由「对端 CHI 引擎 + wrapper」闭合 ⇒ 新增 Sink 无判据增量;wrapper 两角色共享在途 txn 表 ⇒ 拆分必逼出状态共享 |
| **D-5** | **两个拓扑并存**:**A(直连)** = 全绿载体;**B(经网络)** = 互联可达 + 桥投影 + flit 路径验证,**⛔ 不承载全绿** | 「网络」在本仓 = ChStream `connections`(带 latency)+ `cohif_noc` 桥(既有先例 `cep_chi_rnf_pe_minimal.json`)= 2 引擎 + 双向 connections + 1 桥;⛔ 非 Router+flit 层 ⇒ 范围增量小 |
| **D-6** | 冻结 spec `cep-stub-cache-core` 改 **2 条**:`:56`(mock → 真实引擎 + 端口面 wrapper)与 `:71`(不产生 flit → 经桥可产生);⛔ `:63` 不动 | `:63` 禁的 `forward_data`/`back_invalidate`/`barrier` 三词**仍**不在 stub 对外接口出现 ⇒ 不触发;⚠️ 连带 `ADR-CEP-19` 决策 9 的**延期依据② 正是引用这两条** ⇒ ⇒ 需 `ADR-CEP-19` 追加 SU |
| **D-7** | 流量模式:`SEQUENTIAL`(基线)+ `hotspot`(局部性压力);⛔ `RANDOM`(除非固定种子并入 config 快照)/ `TRACE`(需人工 trace) | ⚠️ 纪律:`RANDOM` 的**种子必须进实验产物的 config 快照** ⇒ 否则实验不可复现 ⇒ 违反 `ADR-CEP-06` 的可审计性 |

### ⚠️ 连锁发现:降格**在本轮不触发** ⇒ 冻结面从 5 处回落至 3 处

D-3 裁定 2(不适用 ⇒ `not-collected` + 显式登记降格)**在本轮不触发**,理由:

- `collective_stalls`:CHI 场景**无 collective plane** ⇒ 「collective 停顿 **0** 次」是**真实采集的数值**(⛔ **不是** `not-collected`,更不是谎报)⇒ ⇒ **13 键可全部有值** ⇒ ⇒ 「全绿」**无需降格** ⇒ ⇒ ⛔ **不必**新增假设(新增假设会触及 `ADR-CEP-06` + `obj §9.1` + 冻结的 `preregistration.json` ⇒ 冻结面 5 处)⇒ ⇒ 回落;
- `deadlock_violations` / `memory_violations`:CHI **有** completion 路径 ⇒ 请求会 retire ⇒ `ProgressWatchdog` 的「N cycle 无 retire 且 in-flight 非空」**不成立** ⇒ 真实 0 ✅;
- ⇒ ⚠️ **但 `memory_violations = 0` 有前提**:harness 的 `SwmrDataValueDetector` 需 `checks_enabled = true` **且存在并发写** ⇒ 「空检查检出 0」是**假的 0**(harness 自述的反向 fixture 纪律)⇒ ⇒ ⚠️ **本轮负载必须产生真实并发写** ⇒ ⇒ **D-7 的模式选择需连带确认**(见 §七 D-8 ④)。

⇒ **本轮冻结面清单(3 处)**:① `openspec/specs/cep-stub-cache-core/spec.md`(`:56` + `:71` MODIFIES,两步授权)② `ADR-CEP-19`(追加 SU:决策 9 的延期依据变更 + 延期项解除条件)③ `07 号`(若需登记本次架构决策;⛔ 具体段落待 D-8/D-9 定稿后确定)。

## 七、本讨论的产出清单(⛔ 尚未落任何架构文档)

| 产出 | 状态 |
|---|---|
| 决策清单(10 条,分层 + 状态) | ✅ 本文件 |
| D-1 结论 + 3 项用户裁定 | ✅ 本文件 |
| 取证修正(产出装置已存在 / 分层约束 / 「全绿」定义) | ✅ 本文件 |
| D-3 ~ D-10 结论 | ⏳ 待逐条讨论 |
| **架构文档 + 决策文档**(`ADR-CEP-05` / `ADR-CEP-19` / `07 号` 的 SU) | ⏸ **待 10 条全部统一后**由 architect 落(需 oracle → critic) |

**关联**:台账 Entry #287(下一步任务推荐)· **#282**(4 条已裁决)· **#285**(架构文档审查)· `test/cep_experiment_harness.hh`(产出装置)· `tools/render_report.py`(IC3 判据)· `experimental/metrics.schema.json`(13 键)

---

## 八、回填(2026-11-08,per 台账 **Entry #291**)—— 12 条决策的**最终状态** + 对 §一 / §五 / §六 的**更正登记**

> **性质**:末尾追加段(append-only)。⛔ **不回改** §一 ~ §七 任何原文;本段以「**更正登记**」形式逐条指明旧行与最终事实的差异(per `07 号` SU · 九 的更正登记范式)。
> **触发**:第二轮审查报出本文件作为「12 条决策依据面」**严重过期**(F-03)⇒ + 用户对 **F-05 / F-11** 的两条裁决。

### ① 决策状态回填(§一 表的**现行值**;⛔ §一 原文保留)

| 决策 | §一 原状态 | **最终状态** | 落点 |
|---|---|---|---|
| **D-1** | ✅ 已裁 | ✅ 已裁(不变) | `07 号` SU · 十一 ① |
| **D-2** | ✅ 已裁 | ✅ 已裁(不变);⚠️ §五 ④ 的「`fragment_total=1`」**已被本文件 §四 勘误为 0** ⇒ 以「**`≤ 1`**」为准(见 ② C-1/C-2) | `07 号` SU · 十一 ①(D-2 行含勘误) |
| **D-3** | 🔄 **讨论中** | ✅ **已裁**(本文件 §三 的 3 条裁定即为最终) | `07 号` SU · 十一 ①(D-3 行) |
| **D-4** | ⏳ **待议** | ✅ **已裁**(本文件 §六 D-4 行即为最终) | `07 号` SU · 十一 ①(D-4 行) |
| **D-5** | ✅ 已裁 | ✅ 已裁(两拓扑并存**维持** —— 用户 2026-11-08 裁决 F-11);⚠️ 但 §六 D-5 行的「**flit 路径验证**」**作废**(见 ② C-3) | `07 号` SU · 十一 ①(D-5 行)+ ①-b / ①-c |
| **D-6** | ✅ 已裁 | ✅ 已裁;⚠️ §六 D-6 行「改 **2 条**」**作废** ⇒ 最终 = **`:56` 单条**(`:71` **已撤销**) | `07 号` SU · 十一 ①(D-6 行)· `ADR-CEP-19` §③ |
| **D-7** | ✅ 已裁 | ✅ 已裁;⚠️ 本文件 §六 D-7 行写「`hotspot`」⇒ 最终为 **`RANDOM`**(固定种子入 config 快照)⇒ `hotspot` **移除** | `07 号` SU · 十一 ①(D-7 行) |
| **D-8** | 🔄 **讨论中** | ✅ **已裁**(J-1 ~ J-8) | `07 号` SU · 十一 ①(D-8 行) |
| **D-9** | ⏳ **待议** | ✅ **已裁**(产物不入库 + 台账记元数据) | `07 号` SU · 十一 ①(D-9 行) |
| **D-10** | ⏳ **待议** | ✅ **已裁**(计数纪律禁令句) | `07 号` SU · 十一 ③ |
| **D-0** | ⛔ **本文件未收录** | ✅ **已裁**(用户 2026-11-08):完成者 = **真实 home 的简化模型**(承载**最小协议语义**)⇒ 进 JSON 拓扑 / 落契约面 / ⛔ 不进 `protocol_instances.json` / ⛔ 不计入 B3b | `07 号` SU · 十一 ①(D-0 行)· `ADR-CEP-19` ④-a |
| **D-11** | ⛔ **本文件未收录** | ✅ **已裁**:「网络」= ChStream `connections`(可带 `latency`);**桥不在数据路径上**(装配期投影登记器)⇒ ⛔ 「事务穿过桥」是**假绿**表述 | `07 号` SU · 十一 ①-b · `ADR-CEP-19` ④-b |

⇒ **共 12 条**(§一 的「10 条」是 D-0 / D-11 落定**之前**的清单)⇒ **§一 表已过期**,本表为**现行状态**。

### ② 对本文件旧行的**更正登记**(⛔ 不回改原文)

| # | 旧行(位置) | 原文 | **更正** |
|---|---|---|---|
| **C-1** | §四 末(既有勘误段) | 「§五 ④『TrafficGen 默认 `fragment_total=1`』**有误** … 实发 `fragment_total = 0`」 | ✅ 勘误**成立**(实测 `cache_bundles_tlm.hh:42` 无默认成员初值 + `traffic_gen_tlm.hh:163` 用**默认构造**)⇒ ⚠️ 但 **§五 ④ 未回灌** ⇒ 生效口径 = 「**`fragment_total ≤ 1`**,⛔ 不依赖 = 1」 |
| **C-2** | §五 ④ | 「TrafficGen 默认 `fragment_total=1` ⇒ wrapper 透传」 | ⛔ **作废** ⇒ 见 C-1 |
| **C-3** | §六 D-5 | 「B(经网络) = 互联可达 + 桥投影 + **flit 路径验证**」 | ⛔ **flit 路径验证作废** —— D-11 钉死:桥是**装配期投影登记器**、`src/` **零调用点** ⇒ 仓内**不存在** Router+flit 路径 ⇒ B 的桥侧只能是**测试级** `pack`/`unpack` 断言 |
| **C-4** | §六 D-6 | 「冻结 spec 改 **2 条**(`:56` + `:71`)」 | ⛔ **作废** ⇒ 最终 = **`:56` 单条**;`:71` **撤销**(其 WHEN 限定在**命中**路径);`:63` 不动 |
| **C-5** | §六 D-7 | 「`SEQUENTIAL`(基线)+ `hotspot` … ⛔ `RANDOM`」 | ⛔ **作废** ⇒ 最终 = `SEQUENTIAL` + **`RANDOM`**(固定种子并入 config 快照)⇒ `hotspot` **移除**;⚠️ 机制缺口:`TrafficGenTLM` **无** seed setter(`rng_(42)` 硬编码)⇒ 「种子入快照」**今无落点**(未闭合项 U-2) |
| **C-6** | §七 | 「⛔ **尚未落任何架构文档**」「D-3 ~ D-10 结论 ⏳ **待逐条讨论**」 | ⛔ **作废** ⇒ 12 条**已全部统一并落盘**(`07 号` SU · 十一 + `ADR-CEP-19` SU 2026-11-08,per Entry #288) |
| **C-7** | §一 表 | D-3「讨论中」· D-4「待议」· D-8「讨论中」· D-9 / D-10「待议」 | ⛔ **作废** ⇒ 见 §① 回填表 |

### ③ 本文件**未**覆盖的依据面(诚实登记)

⇒ 本文件覆盖 **D-1 ~ D-7**(7 条)的讨论与取证;**D-8 / D-9 / D-10 / D-0 / D-11** 的讨论依据**不在本文件** ⇒ ⚠️ 审查材料包 §一 P1 第 5 项称本文件是「12 条决策的**逐条讨论依据**」**不成立** ⇒ **依据面实况 = 7/12 条**;其余 5 条的**裁定文本**见 `07 号` SU · 十一 与 `ADR-CEP-19` SU 2026-11-08(决策文本自足),但其**讨论过程未留档** ⇒ 登记为**证据面缺口**(⛔ 非阻塞;若要补齐 ⇒ 另立 Entry **回溯补记**,⛔ 不得事后伪造讨论记录)。

**关联**:台账 **Entry #291** · **#288**(12 条决策落盘)· **#290**(审查材料包)· `.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md`(第二轮审查 · F-03)· `07 号` SU · 十一 · `ADR-CEP-19` SU 2026-11-08
