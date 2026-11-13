# 档 2 评估 —— 协议引擎不变量观测面的工程量级与最小切片

> **性质**:architect **评估**(⛔ 不是决策、⛔ 不是 ADR、⛔ 不含实施)。决策依据 = `ADR-CEP-20`(v1.0)决策 5(**强度上限在本仓的协议引擎面**)+ 其「后续 ②」。
> **授权**:台账 **Entry #403** ⑥(本评估属 A→B→C 的 C)
> **结论先行**:**档 2 本仓可达;最小切片 ≈ 5-8 人日,全族推广 ≈ 12-20 人日**(粗估,须由实施前读码校正)。**关键正面发现**:协议引擎侧**已有 sharer tracker 且已注入 2 个引擎** ⇒ sharer 侧不是从零建。
> ⚠️ **本评估不裁「做不做」** —— 立项与排期属 planner;若涉**新协议语义定义**,依 `ADR-CEP-20` 决策 4 主理由 ② 须先取 ArchForge 设计权威。

---

## 1 · 现状实测(全部逐 `file:line`)

| # | 事实 | 落点 |
|:--:|---|---|
| 1 | **6 个协议引擎** + 基类 | `chi_rnf_protocol_engine.hh`(648 行)· `mesif_protocol_engine.hh`(667)· `moesi_gpu_protocol_engine.hh`(306)· `tilelink_protocol_engine.hh`(209)· `ualink_protocol_engine.hh`(185)· `cohif_protocol_engine.hh`(**43**)· `protocol_engine_base.hh`(529) |
| 2 | **不变量观测面 = 不存在** | 全族 `grep swmr|version|detector` ⇒ **0 命中**;`protocol_engine_base.hh` 只有 `:414 counts()`(聚合计数器),**无 per-txn 观测钩子** |
| 3 | ✅ **sharer 侧已有基建** | `include/tlm/cep/sharer_tracker.hh`(接口 + **5 实现**:`FullMapDirectory` / `SparseSnoopFilter` / `HierarchicalDirectory` / `DcohSymmetric` / `BroadcastNullTracker`)· `mesif_protocol_engine.hh:49` 已 `#include` · `moesi_gpu_protocol_engine.hh:146-147` 已**注入** `SharerTrackerIf*`(`set_sharer_tracker` / `sharer_tracker()`) |
| 4 | ⚠️ **版本语义 = 不存在** | 引擎族 `version` **0 命中**;`include/bundles/cache_bundles_tlm.hh:39-46` 的 Req/Resp **无 version 字段** ⇒ 与档 0 同款缺口 |
| 5 | 规约层载体**已成熟** | 6 份 `*_state_spec.hh`(`cxl` / `cxl_dcoh` / `mesif` / `nvlink_sharp` / `tilelink` / `ualink_inc`)· 8 份 `*_adapter.hh` · **15 个** `cep_*reverse_verify*.sh`(毒化 fixture 已机械守) |
| 6 | 实例声明面 | `ADR-CEP-12`(已签发 v1.0)的 `protocol_instances.json`:`perm` 4 值 + `consistency_labels` 读/写两级 + `ordering_assumption` per-network + `status`(`registered` / `pending` / `implemented`) |

⇒ ⇒ **档 2 的两个「语义要件」状态不同**:sharer **已有基建**(事实 3),版本**完全没有**(事实 4)⇒ 工程量因此**不对称**。

---

## 2 · 档 2 需要什么(adar-CEP-20 决策 6 的「协议语义」档)

| 要件 | 内容 | 现状 | 需新增 |
|---|---|---|---|
| **观测面** | per-txn 的 `(addr, is_write, state_before, state_after, version, sharer_set_snapshot)` 原始事实(⛔ 不做聚合) | ⛔ 无(`counters()` 只聚合) | ✅ 基类观测接口 + 逐引擎接线(**做法可复用 R-3 的 `TxnStamp` 同型**) |
| **sharer 可见性** | 每次读/写时该地址的 sharer 集合(判「是否先失效既有 sharer」) | ✅ `SharerTrackerIf` + 5 实现,**2 引擎已注入** | ⚠️ 其余 **4 引擎**需注入(mesif/moesi_gpu 之外) |
| **版本语义** | 每次写产生可区分的版本;读带回观测版本 | ⛔ **无**(Req/Resp 无 version 字段) | ✅ **需新增**(⇒ ⚠️ **bundle 语义变更**,影响面须评估) |
| **检测器** | SWMR + data-value(可复用 `test/cep_experiment_harness.hh` 的 `SwmrDataValueDetector` 逻辑) | ✅ 逻辑存在(test 侧) | ⚠️ 需**搬到实现侧或用观测回调**(⛔ 实现不得依赖 test 头) |
| **负控** | 注入式:错标完成域 / SWMR 注入 / 过期读 | ✅ 概念先例:`test_cep_b4_evidence_device.cc` 三负控 | ✅ 逐族重做(6 族 × 3 负控) |
| **六条非空洞性约束** | `ADR-CEP-06` SU#4 ② 原样适用(档 2 是**语义档**) | ✅ 规则已签发 | ✅ 逐条落证据 |

---

## 3 · 工程量级(粗估 · ⚠️ 须实施前读码校正)

| 切片 | 内容 | 估时 |
|---|---|---|
| **S1** 基类观测面 | `protocol_engine_base.hh` 加 per-txn 观测结构 + 访问器(⛔ 不改既有虚函数签名) | **1-2 人日** |
| **S2** 单族接线(建议 **mesif** —— 已有 sharer tracker + state_spec + 反向闸) | 在 `mesif_protocol_engine.hh` 落观测点(读/写/失效) | **1-2 人日** |
| **S3** 版本语义 | ⚠️ **最大不确定项** —— 若只做 **run 内单调写序**(引擎内部计数)⇒ 小;若要求 **协议级版本字段**(改 bundle)⇒ **波及面大**(须评估 `CacheReqBundle` 的 24 处消费方) | **1 人日(内部写序)/ 3-5 人日(bundle 级)** |
| **S4** 检测器接入 | 观测回调 → 检测器(实现零 test 依赖) | **1 人日** |
| **S5** 负控 + 六条约束 | 3 负控 × 1 族 + 六条逐条证据 | **2-3 人日** |
| **S6** E2E + 4 件套 + IC3 | 协议级 run 落 `experimental/` + 档 2 条目 + 档位表 + 新闸判据 | **1-2 人日** |
| **最小切片合计**(S1+S2+S3-内部写序+S4+S5+S6) | | **≈ 7-11 人日** |
| **全族推广**(其余 5 引擎 × S2/S3/S5 的增量) | | **+8-12 人日** |
| **全族合计** | | **≈ 15-23 人日** |

⚠️ **三处会推翻估时的未知(⛔ 不许假设,须在实施前读码确认)**:

| # | 未知 | 若不成立 |
|:--:|---|---|
| U-1 | `mesif_protocol_engine.hh` 的**事务边界**是否足以承载 per-txn 观测(是否有清晰的 read/write/invalidate 三入口) | ⇒ S2 上浮(需先在引擎内引入事务抽象) |
| U-2 | `SharerTrackerIf` 是否**可查询当前 sharer 集合**(而不只「接受/拒绝」) | ⇒ 需扩接口 ⇒ **波及 5 实现 + 2 已注入引擎** |
| U-3 | `perm` 4 值 与 `consistency_labels` 读/写两级(`ADR-CEP-12`)是否足以判定**权限违例** | ⇒ 缺维度 ⇒ 需先补 `ADR-CEP-12` 的 schema(⇒ **改已签批 ADR** ⇒ 走取代流程) |

---

## 4 · 关键风险

| # | 风险 | 缓解 |
|:--:|---|---|
| R-1 | **设计权威边界**:若新增「版本」语义属**新协议语义设计** ⇒ 依 `ADR-CEP-20` 决策 4 主理由 ② 须先取 **ArchForge 设计权威** | ⇒ **立项前**先裁:① 用**引擎内部写序**(不引入协议语义 ⇒ 本仓可做)② 引入**协议级版本字段**(⇒ 走 ArchForge 单向门) |
| R-2 | **bundle 语义变更的波及面**:若选 ②,`CacheReqBundle` / `CacheRespBundle` 加字段 ⇒ 需评估全部消费方(实测 `test/` + `src/` + `include/` 多处) | ⇒ 优先选 ①(内部写序)以避开 |
| R-3 | **实现零 test 依赖**(与 R-3 change 的 D-3 同款纪律):检测器在 `test/` ⇒ 实现侧只能**暴露原始事实**,⛔ 不得 `#include` test 头 | ⇒ 用观测回调/访问器;⛔ 不搬 test 代码进 `include/` |
| R-4 | **6 族成熟度不均**(`cohif` 仅 43 行 = 薄壳;`chi_rnf` 是子集)⇒ 全族都做「协议语义观测」可能**语义上不可比** | ⇒ 逐族裁「该族是否具备可观测的协议语义」;⛔ 不为凑覆盖率强行统一 |
| R-5 | **档位表的档 2 条目**一旦出现,`cep-evidence-tier-declaration`(C8)的判据须支持档 2 | ⇒ C8 的 tiers 表已支持任意档位值;⚠️ 但「档 0 才必须含档位词」的判据须扩到「档 2 必须含其**族名 + 版本口径**」⇒ **C8 之后的小增量** |

---

## 5 · 建议(按影响排序)

1. **先裁 U-3 与 R-1**(两个「会不会改写已签批契约」的前置)⇒ 若答案为「要改 `ADR-CEP-12` schema」或「要引入协议级版本」⇒ ⛔ **不立项**,先走 ADR 取代 / ArchForge 单向门。
2. **最小切片选 `mesif`**(事实 3:已有 sharer tracker 注入 + state_spec + 反向闸)⇒ 用它验证「观测面 + 检测器 + 负控 + 六条约束」的**全链可行性**,再谈推广。
3. **版本语义取「引擎内部写序」**(不引入协议语义)⇒ 规避 R-1 / R-2;⚠️ 须逐字登记「该版本是**观测侧代理版本**,⛔ 非协议语义的版本」—— 与 `ADR-CEP-20` 决策 3a 对「测试侧代理版本」的否定**不冲突**(那里否定的是**把它当装置语义判据**;这里它是**观测维度**,且判别力由 sharer 侧提供)。
4. **不追全族覆盖**:逐族裁「有无可观测协议语义」⇒ 覆盖率**按可实现集**报,⛔ 不用「6/6」当指标(与 `B3b-2` 的逐族纪律同族)。
5. **C8 落地后再做**(避免两个 change 同时动 `test/cep_e2e_run_tiers.json` 与档位判据)。

---

## 6 · 与既有决策的关系

- **`ADR-CEP-20` 决策 5**:本评估**确认**其结论(强度上限在本仓的协议引擎面)—— 并给出**正面证据**:sharer 侧基建已在(事实 3),故「本仓可达」不是纸面判断。
- **`ADR-CEP-20` 决策 3a**:⛔ 不冲突 —— 决策 3a 否定的是「在**拓扑装置**上代理版本并**当装置语义判据**」;本评估的「内部写序」是**观测维度**,判别力来自 **sharer 侧**(真实可见性),⛔ 不是代理产物。
- **`ADR-CEP-06` SU#4 ②**:档 2 是**语义档** ⇒ 六条非空洞性约束**原样适用**(⛔ 不得豁免)。
- **`ADR-CEP-12`**:`status` 只有 `implemented` 的实例才可能有观测面 ⇒ 覆盖口径须与该字段对齐。

---

## 7 · 不可约残余

1. 本评估的估时是**粗估**,三处未知(U-1~U-3)任一不成立即上浮 ⇒ ⛔ 不得直接作为排期承诺;
2. 「协议语义级判别力」的**语义正确性**仍须人工评审(⛔ 同 V-20-1/2/3 的文档层纪律);
3. 全族覆盖**不可比**(族成熟度不均)⇒ ⛔ 不得用「N/6」当强度指标;
4. 本评估**不裁决**「做不做」—— 立项属 planner,若涉新语义设计属 ArchForge 单向门。

**关联**:台账 **Entry #403**(授权)· `ADR-CEP-20` 决策 5 / 后续 ② · `ADR-CEP-12` · `ADR-CEP-06` SU#4 ② · `include/tlm/cep/*_protocol_engine.hh` · `include/tlm/cep/sharer_tracker.hh` · `docs/architecture/cep/protocol-engine.md`
