# cpptlm-cep-ualink-inc-state-spec — UALink INC 规约层(载体 + 逐格 basis + 判据)

> **状态**: 🟡 **Proposed**(2026-11-04)· **队列**: `obj-cep-platform §4.0` **#14** 第 0 个 change(**规约层**)
> **配套**: [`design.md`](design.md) · [`tasks.md`](tasks.md) · [`specs/cep-ualink-inc-state-spec/spec.md`](specs/cep-ualink-inc-state-spec/spec.md)
> **口径**: 台账 **Entry #194**(improvement 草稿)· **#193**(三则 ADR SU + 冻结面授权)· **#191 / #192**(遗留清扫)
> **前置**: `ADR-CEP-01` / `04` / `09` 的 2026-11-03/04 三则 SU ⇒ ✅ **已满足**(Entry #193)
> **审计面**: `docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md`(basis 唯一人工审计面)
> **估时**: **4 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 change = 本 change(**规约层**)+ 第 1 个 change = `cpptlm-cep-ualink-inc`(**实现层**)

---

## Why

### ① 平台答不了「网内归约时该区域的 coherence 可见性怎么办」

已落地的 6 个协议实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0` / `cxl_3_0_symmetric` / `tilelink_c`)
+ 队列 #13 的 `nvlink_4` **全部是 coherence-only** ⇒ 「coherence × collective × **网内**归约」的交叉
**零覆盖**。UALink INC(`ReadReduce` / `WriteMulticast` / `Broadcast` / `AllReduce`,跑在 `ualink_fabric` 上)
是平台**唯一**能覆盖该交叉的协议面(per `ADR-CEP-03:70`「UALink → **全集合族**」)。

### ② UALink 是**全集合族**,与队列 #13 的 SHARP **不重叠**

队列 #13 的 NVLink SHARP 只覆盖 **SHARP op 集**(`ADR-CEP-03:70` 的并列句)⇒ 两个队列的 collective 语义面
**不重叠** ⇒ 队列 #13 的载体与判据**不能**替代本队列。

### ③ 公开来源**撑不住**细粒度语义 ⇒ 必须有规约层把「已定稿的」与「待定的」**分开**

Task 0 普查(锚点摘要 §2 / §3)实测:公开来源只有 **5 条高层断言**(`A-1` ~ `A-5`),**连 INC 四原语的名字都没有**
⇒ **9 项**细粒度语义(`N-1` ~ `N-9`)是 `pending`。
⇒ 若无规约层把这些 `pending` **显式登记 + 逐格绑定**,实现层就会**顺手推理填空** ⇒ **纸面契约**。

### ④ 路径 D 的**证书**必须先有规约面

`ADR-CEP-01` SU#4(Entry #193)把抓取点 3 改为**两种满足模式**,并规定 M-2 的**四条证书**
(C-1′ / C-2 / C-3 / C-4)⇒ 但证书的**逐边依据 + 参照域 + pending 边界**尚无规约载体
⇒ 本层提供之(实现层只负责落载体)。

---

## What Changes

| 面 | 交付 |
|---|---|
| **载体 ①** | `include/tlm/cep/ualink_inc_state_spec.hh`(新 · header-only · **自足**)—— 只 include **标准库**;⛔ 无平台词表副本 · ⛔ 无区域表类型 · ⛔ 无 `transition_*`;承载 INC **四原语**的**可判定**语义 + **逐格 basis** + **9 项 pending** + 编译期断言(带归因 id) |
| **判据** | 正向判据(每条**负向断言配正控**)+ **9 组反向 fixture**(毒化 ⇒ **必红**且归因 id **精确相等**)+ 基线正控 + **归因 id 元测试**(双向覆盖)+ `ctest` 注册 |
| **证书规约面** | 四条证书的**逐边依据 + 参照域**(两域并集)+ 「该网映射不一致 ⇒ 不得用 M-2」的**可判定形式** + `IC1` 的**鉴别力边界**登记(⛔ **不得**把「重跑 DAG」当证书) |
| **文档** | 台账 §10 登记 · `scripts/README.md` 守门表 + 反向 fixture 登记 |
| **零改动** | `ADR-CEP-01` / `02` / `03` / `04` / `09` / `11` / `12` **正文** · `protocol_instance*.json` · `fabric.json` · `collective_planes.*` · canonical **10 流**清单 · `ReduceOp` 枚举 · `CollectivePlaneIf` 的 **7 方法**面 · **0 新增** ABI · **0 新增** `INV-` 编号 |

---

## Capabilities

| 能力 | 内容 | 载体 |
|---|---|---|
| **`cep-ualink-inc-state-spec`**(新) | UALink INC 的**规约层**契约:四原语的可判定语义 · 逐格 `basis` · 9 项 `pending` · 四条证书的规约面 | 本 change |

---

## Impact

| 面 | 影响 |
|---|---|
| **本仓实现** | 新增 **1 个头** + 1 份测试 + 1 个探针/脚本;**冻结面零 diff** |
| **平台能力** | 首次把「coherence × collective × 网内归约」的**语义边界**固定下来(哪些已定稿、哪些待定)⇒ 实现层不再需要自行推理 |
| **文档** | 台账登记 · `scripts/README.md` |
| **KPI** | **0/3 → 1/3** |
| **残余风险** | 公开来源受限 ⇒ **9 项 pending** ⇒ 本层只实现**结构性**语义;实现层**不得**推理填空(逐格 basis 在本层载体) |
| **可逆性** | 高:删 1 头 + 撤 delta spec ⇒ 零既有行为变更 |

---

## Out of Scope

- 硬件 **opcode / 指令编码** · 真实 **带宽 / 延迟**结论 · UALink 的**一致性状态机**(它没有)
- **扩 `vc_count`**(路径 A)—— 4 VC 是硬件事实 ⇒ 本层零改动
- **不**新增 reduce 算子族(唯一真源 = 既有 `ReduceOp` 恰 6 值)· **不**新增 canonical 流
- **不**改 `CollectivePlaneIf` 的 **7 方法**面 · **不**改 `collective_planes.json`(那是实现层的声明面)
- 实现层载体(`UalinkAdapter` / `FabricCollectivePlane` / `ualink_4`)—— 属**第 1 个 change**

---

## Acceptance

- [ ] **载体形态定稿**:`ualink_inc_state_spec.hh` **自足**(只 include 标准库 · 无平台词表副本 · 无区域表类型 · 无 `transition_*`)+ **逐格 basis**(每格必有 `ref`)+ **9 项 pending** + 编译期断言(带归因 id)
- [ ] **9 组反向 fixture**:毒化任一格 ⇒ **必红**且归因 id **精确相等**命中 + **基线正控**(干净载体必绿)
- [ ] **归因 id 元测试**:fixture 的 id 集合 ⊆ 本 change 的 id 登记集合(**双向**)+ 每个本层新建 id 至少一条 fixture
- [ ] **证书规约面**:四条证书的**逐边依据 + 参照域**齐备 + 「该网映射不一致 ⇒ 不得用 M-2」**可判定** + `IC1` 鉴别力边界登记
- [ ] **零改动核验**:`ADR-CEP-01/02/03/04/09/11/12` 正文 · `protocol_instance*.json` · `fabric.json` · `collective_planes.*` · `ReduceOp` · canonical 10 流 ⇒ **零 diff**
- [ ] **收口**:`ctest` 全绿 + 门禁 rc=0 + `openspec validate --strict` rc=0 + `format.sh --check` rc=0 + KPI 回填
