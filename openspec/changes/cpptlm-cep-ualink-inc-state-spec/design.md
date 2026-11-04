# design — cpptlm-cep-ualink-inc-state-spec(规约层)

> **口径**:本 design 是**规约层**的实施指导(how);**当前态**与**架构意图**在 `docs/designs/` 与
> `docs/adr/ADR-CEP-0{1,3,4,9,11}`(真源 3)·**不重推语义**,只把裁决**机器化**。
> **唯一真源链**:`ADR-CEP-01` SU#4(路径 D + 四条证书)→ `ADR-CEP-03`(原语集 / 跨平面契约)
> → `ADR-CEP-04` SU(tracker 选型)→ 本层载体 → 判据。

---

## §1 交付面与载体形态决定

### §1.1 复用的既有裁决(**不重开**)

| # | 裁决 | 登记处 |
|---|---|---|
| ① | **VC 路径 = M-2**(共享 VC + 四条证书);M-1(隔离)在本拓扑**不可用**(VC 饱和) | `ADR-CEP-01` SU#4 的 E.2 |
| ② | **原语最小集 = 恰 4**;`AllReduce` = REDUCE + BROADCAST 的**复合**,⛔ **不是**第 5 原语 | `ADR-CEP-01:94` / `ADR-CEP-03:75` |
| ③ | **reduce 算子 = 复用既有 `ReduceOp`(恰 6 值)**;载体**不**定义算子族 | `ADR-CEP-03` SU(#2) F.1-b |
| ④ | **partial failure** ⇒ `CollectiveEvt::Fault`(不得静默) | `ADR-CEP-01` §7 V5 |
| ⑤ | **tracker 选型 = 候选集(7 值)+ 逐实例理由登记**;⛔ **无** `flow_support` 派生规则 | `ADR-CEP-04` SU 2026-11-03 |
| ⑥ | 集合平面**继承**该 network 既有实例的 VC 空间(**不声明**自己的绑定) | `ADR-CEP-01` SU#4 的 E.3.2 |

### §1.2 本层**新**定的 4 项形态决定

| # | 决定项 | 结论 | 依据 |
|---|---|---|---|
| **N-1** | 载体的**自足性** | **只 include 标准库**;⛔ 不引用 `collective_plane.hh` / `sharer_tracker.hh` 等平台头 | 队列 #13 规约层载体的先例(自足 ⇒ 不受平台表变动影响 ⇒ basis 不会因实现漂移而失效) |
| **N-2** | 「不新增原语」如何**可判定** | 载体**自持** INC 四原语 id + 它们到 `REDUCE`/`BROADCAST`/`MULTICAST` 的**映射**(按**名字**);**跨载体一致性**由**元测试**断言(载体名集 == `tlm::cep::CollectiveOp` 的**覆盖集**)⇒ 载体仍自足,判据仍可执行 | 载体自足 + 判据可执行**两者兼得**的唯一形态(在载体里 include 平台头会破自足;只写散文会不可判) |
| **N-3** | **9 项 pending** 的载体形态 | 每项 pending 是一个**具名条目**,带 `ref`(= 锚点摘要的 `N-n` id)+ **禁推声明**;载体**不得**含任何 `pending = false` 的「由公开来源支撑」格(公开来源撑不住 ⇒ 全部 `self-derived` 或 `pending`) | 锚点摘要 §3 · 队列 #13 规约层的「无 `ref` 的 pending 即红」先例 |
| **N-4** | **证书规约面**的位置 | 载体**内**给出四条证书的**逐边依据 + 参照域**;⛔ **不**在载体里实现判据(判据属 `cep-collective-plane-contract`,由**实现层**的 delta 承载)⇒ 本层只提供**规约面** | `ADR-CEP-01` SU#4 的 E.3.1(判据面在契约头)· 队列 #13 的「载体 ≠ 判据」分层 |

---

## §2 载体形态(`include/tlm/cep/ualink_inc_state_spec.hh`)

### §2.1 ⭐ 自足性(禁项清单)

本文件的**去注释后**文本 SHALL **不**含:

| 类别 | 禁项 |
|---|---|
| **平台标识** | `CollectiveOp` · `ReduceOp` · `CollectiveEvt` · `Status::` · `LookupResult` · `TrackerImpl` · `CollectivePlaneIf` · `LocalCollectivePlane` · `kCollectivePlaneRegistry` · `tlm::cep::` |
| **项目内头** | 任何 `#include "tlm/..."` / `#include "cep/..."` ⇒ **只允许** `#include <...>` |
| **区域表** | `collective_region_table` |
| **转移判定** | `transition_*` · `is_silent_upgrade` |

⇒ 语法上**不可能**携带平台状态机 / 投影表 / basis 表 ⇒ 只能承载**本协议自身的**语义与 basis。

### §2.2 INC 四原语(自持 id + 映射)

| INC 原语 | 映射到的既有原语 | 性质 |
|---|---|---|
| `ReadReduce` | `REDUCE` | 一原语 |
| `WriteMulticast` | `MULTICAST` | 一原语 |
| `Broadcast` | `BROADCAST` | 一原语 |
| `AllReduce` | `REDUCE` + `BROADCAST` | **复合**(⛔ 不是第 5 原语) |

⇒ **覆盖集 = {REDUCE, BROADCAST, MULTICAST}** ⊆ `CollectiveOp` 的 4 原语 ⇒ **不新增原语**
(跨载体一致性由 §2.4 的元测试断言)。

### §2.3 逐格 `basis` + 9 项 `pending`

- 每格 `basis` 必有 `ref`(指向锚点摘要的 `A-n` / `C-n` / `N-n` id)⇒ **无 `ref` 的格即红**。
- 每格 `basis` 有 `kind ∈ {self-derived, pending}`:
  - `self-derived` ⇒ `ref` 必指 `C-n`(**仓内已签锚点**)⇒ 公开来源**零支撑**;
  - `pending` ⇒ `ref` 必指 `N-n`。
- **9 项 pending**(`N-1` ~ `N-9`,与锚点摘要 §3 **逐项对应**)⇒ 载体 SHALL 能**逐项**报出其 id 与 `ref`。

### §2.4 跨载体一致性的**元测试**形态

| 断言 | 内容 |
|---|---|
| **X-1 原语覆盖** | 载体的 INC 四原语的**覆盖集** ⊆ `tlm::cep::CollectiveOp` 的取值集;且 `AllReduce` 的映射**恰为 2 个**(REDUCE + BROADCAST)⇒ **不是**第 5 原语 |
| **X-2 pending 完备** | 载体的 pending 条目集合 == 锚点摘要的 `N-1` ~ `N-9`(**逐项相等**,不多不少) |
| **X-3 basis 完整** | 每格 basis 有 `ref`;且 `self-derived` 格的 `ref` **不得**指 `A-n`(公开来源撑不住 ⇒ 禁止冒充) |

---

## §3 证书的规约面(四条)

| # | 证书 | 规约面提供的**可判定形式** |
|---|---|---|
| **C-1′** | 槽位语义继承 | 参照域 = **两域并集**(ArchForge 派生域 + `repo_owned_instance_ids[]`);判定 = 该 network 上**全部有绑定的**既有实例的映射**逐类相等**;**不一致 ⇒ 该网不得用 M-2** |
| **C-2** | VC 覆盖完整 | 覆盖集 SHALL ⊇ 该 network 的 `vc_count` 全部取值(直接对 `fabric.json` 判)⇒ ⛔ **不**依赖「边表是否引用某 VC」 |
| **C-3** | 控制面与数据面互斥 | 某 group 有 op in-flight 时 SHALL **拒绝** `group_register_region` / `group_destroy` |
| **C-4** | 未检验项登记 | **U-1**(H6 隔离假设)与 **U-2**(同 VC 内两类流量的相互阻塞**不被图模型表达**)SHALL 登记为**未检验** |

⚠️ **证书的判据面**属 `cep-collective-plane-contract`(实现层的 delta 承载)⇒ 本层**只**给规约面。

### §3.1 ⛔ 不得把「重跑 DAG」当证书(实测,写进规约面)

`evaluate_channel_dependency_gate` 的边**按 network 声明、不按 instance** ⇒ 任一实例的图 = 声明边表的
**诱导子图** ⇒ **DAG 的任一子图仍是 DAG** ⇒ 该闸**在结构上不可能**报出由 `vc_binding` 引起的环
(实测:把 `ualink_fabric` 的 `request` 映到 `response` 的槽 ⇒ 仍 **PASS**,`cycles=0`)⇒
「重跑 DAG」**恒真** ⇒ ⛔ **不得**作为证书的判据面。
⇒ 语义部分由 **C-1′ / C-3** 承担;**C-2** 只承担覆盖完整性。

---

## §4 判据面(9 组反向 fixture ↔ 9 个 Scenario)

| 组 | Scenario | 义务 | 归因 id | 毒化锚点 | 红形态 |
|---|---|---|---|---|---|
| **F-1** | `UIS-01` | 原语集**恰 4** 且**不新增**原语 | `UIS-01` | 影子副本加第 5 个 INC 原语 | 运行期 |
| **F-2** | `UIS-02` | reduce 算子**只引用**既有 `ReduceOp`(恰 6 值) | `UIS-02` | 影子副本自带算子枚举 | 运行期 |
| **F-3** | `UIS-03` | partial failure ⇒ `Fault`(不静默) | `UIS-03` | 影子副本把失败路径改为静默 | 运行期 |
| **F-4** | `UIS-04` | `ReadReduce` 的 coherence 可见性**边界**(pending 不得被当作已定稿) | `UIS-04` | 影子副本把该格 `pending` 改成 `self-derived` | 运行期 |
| **F-5** | `UIS-05` | 排序 / 原子性要求(同 F-4 的形态) | `UIS-05` | 同上 | 运行期 |
| **F-6** | `UIS-06` | 控制面归属(pending 边界) | `UIS-06` | 同上 | 运行期 |
| **F-7** | `UIS-07` | 集合 VC **继承**(不新造映射)+ 「不一致 ⇒ 不得用 M-2」 | `UIS-07` | 影子副本给集合平面加独立映射 | 运行期 |
| **F-8** | `UIS-08` | `ualink_4` 的 `flow_support` **待定**(不得先填后证) | `UIS-08` | 影子副本把该格填成具体值 | 运行期 |
| **F-9** | `UIS-09` | `perm` 子集 **待定** | `UIS-09` | 同上 | 运行期 |

⇒ **9 组**,1:1 对应 `UIS-01` ~ `UIS-09`;每组**必红** + 归因 **精确相等**;**基线正控**(干净载体必绿)。

---

## §5 归因 id 登记(**唯一**登记处 = 本 design §5)

| 义务 | 归因 id | 状态 | 代码登记点 |
|---|---|---|---|
| 原语集恰 4 / 不新增原语 | `UIS-01` | 本层新建 | 载体 + 测试用例名 |
| reduce 算子复用 | `UIS-02` | 本层新建 | 同上 |
| partial failure ⇒ `Fault` | `UIS-03` | 本层新建 | 同上 |
| `ReadReduce` 可见性边界 | `UIS-04` | 本层新建 | 同上 |
| 排序 / 原子性边界 | `UIS-05` | 本层新建 | 同上 |
| 控制面归属边界 | `UIS-06` | 本层新建 | 同上 |
| 集合 VC 继承 + 不一致即禁用 | `UIS-07` | 本层新建 | 同上 |
| `ualink_4` 的 `flow_support` 待定 | `UIS-08` | 本层新建 | 同上 |
| `ualink_4` 的 `perm` 待定 | `UIS-09` | 本层新建 | 同上 |
| 四条证书(C-1′~C-4) | `CP-03-VC-CERTIFICATE` / `CP-09-IC1` | **复用**(已签) | `collective_plane_contract.hh`(实现层) |

⇒ ⛔ **不新造 `CP-*` 体系**(per `ADR-CEP-01` 附录「禁在本仓新造平行 id 体系」)⇒ 本层用 `UIS-xx` 作归因键
(照队列 #13 规约层的 `NSS-xx` 先例)。

---

## §6 pending 边界(9 项 ⇒ 本层纪律)

`N-1` ~ `N-9`(锚点摘要 §3,逐项对应)⇒ **不得**被本层或实现层**推理填空**。
⇒ 本层只实现**结构性**语义;凡落到这 9 项 ⇒ **一律** `pending` + 登记 `ref`。
⇒ ⚠️ **合规注记**:UALink Common 2.0 的规范正文受 **Evaluation Copy Agreement** 约束(实测,锚点摘要 §0)
⇒ 与本仓 **CXL ECA §3** 同型 ⇒ 技术正文**禁投喂 AI** ⇒ `N-n` 的转正需**新**的 Task 0 轮次(不得就地扩写)。

---

## §7 文档同步面 + 冻结面

| 面 | 动作 | 时点 |
|---|---|---|
| 台账 `obj §10` | 登记本 change(规约层) | 建档时 |
| `scripts/README.md` | 守门表新增行 + **反向 fixture 登记**新增节 | Phase 3 |
| 冻结面 | **零 diff**(本层不动任何 M1 产物)⇒ 归档时**无**需两步授权 | — |

---

## §8 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| **公开来源受限** ⇒ 9 项 pending ⇒ 本层的语义覆盖面窄 | 🟡 中 | 逐格 basis 全部指向 `C-n`;pending 逐项登记;⛔ 实现层不得推理填空 |
| **禁项清单**漏检(载体偷偷引用平台头)⇒ basis 随实现漂移 | 🟡 中 | 静态扫描 + **鉴别力自测**(毒化载体 ⇒ 必红) |
| **`IC1` 恒真** ⇒ 证书退化 | 🟢 低(已处置) | §3.1 写进规约面 + ⛔ 禁止作判据 |
