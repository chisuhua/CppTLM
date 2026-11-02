# cpptlm-cep-cxl-dcoh-symmetric: CXL 3.0 对称(DCOH)—— **实现层**(`CxlDcohAdapter` + `DcohSymmetric` 行为实现 + 实例声明)

> **状态**: 🟡 **Proposed**(2026-11-02)· **队列**: `obj-cep-platform §4.0` **#12** 的**第 1 个 change**(实现层)
> **口径**: 台账 **Entry #159**(第 0 个 change 归档)· **#154**(4 项 architect 裁决)· **#153**(Task 0 普查)· **#122**(CXL 口径裁决)
> **规划输入**: `.rddf/improvements/cpptlm-cep-cxl-dcoh-symmetric.md`(2026-11-02,HARD-GATE rc=0)
> **规约载体**: `include/tlm/cep/cxl_dcoh_state_spec.hh`(**已归档**的规约层产物;main spec = `openspec/specs/cep-cxl-dcoh-symmetric-state-spec/spec.md`)
> **审计面**: `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`(**basis 唯一人工审计面**)
> **估时**: 8 周 · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 = **规约层**(`cpptlm-cep-cxl-dcoh-symmetric-state-spec`,**已归档**)· 第 1 个 = **本 change**(实现层)
> **冻结面**: 预期 **0 件**(新增载体;`protocol_instance.schema.json` 值级枚举**零改动**)—— **须 Phase 0 实测确认**

---

## Why

### ① `DcohSymmetric` 的 traits **已承诺、实现面零兑现**

`ADR-CEP-04 §13` 已登记第 6 种 tracker;`AggregationAt` 枚举已含 `REQUESTER`(裁决后改为 `HOME` 语义);
而 `scope=LOCAL_MEMORY_REGION` / `authority=LOCAL_HINT` **只有 CXL 3.0 的设备侧代理**用得上
⇒ **声明面已承诺、实现面零兑现** ⇒ 与 R11 同型(队列 #10 的 `SparseSnoopFilter` 已同款先例)
⇒ 本 change 是该缺口的**首次兑现** ⇒ 闭合 07 号「残留 A:行为语义无基源」。

### ② 规约层已就位 ⇒ 实现层**不必自推**

第 0 个 change(规约层)**已归档**:3 套状态空间 · 6+3 通道 · `{BISnp}` 判别式 · `perm` 4→4 · 窄义 5 谓词 ·
**28 格**逐格 `basis` + **10 项**语义 pending。
⇒ 本 change **只消费**载体 ⇒ **零语义自推**空间。

### ③ 对称模式**必须**是**独立 adapter** —— 扩既有 adapter 会破坏零副本

`CxlCacheAdapter` 是 **`cxl_2_0` 专用**载体(零自有表副本 + **单一委托** `cxl_state_spec.hh`)。
对称模式的**状态空间完全不同**(3 套 vs 1 套)⇒ 支持双载体将迫使它引入**载体选择逻辑**
⇒ **破坏「去注释后文本不含成员字面量」的结构性保证** ⇒ 裁决为**独立 `CxlDcohAdapter`**(per architect 裁决 #4)。

### ④ 平台需要它才能回答**第四种一致性拓扑**的取舍

已落地 5 个实例全部是 host-centric / 中心化目录(仅 `tilelink_c` 为树形)
⇒ 不覆盖「设备侧代理 + 反向 snoop host」,就答不了「中心化目录 vs 设备侧代理」这一档(§0 Goal 核心命题)。

---

## What Changes

| 面 | 改动 |
|---|---|
| **adapter(新增)** | `include/tlm/cep/cxl_dcoh_adapter.hh`(header-only · **独立** · **零自有表副本** · 委托消费规约载体)—— ⚠️ **不暴露** transition API(载体**无转移表**) |
| **tracker(新增行为类)** | **独立头** `include/tlm/cep/cxl_dcoh_tracker.hh` 的 `DcohSymmetric` **行为实现**(traits 已登记)—— 保守型(宁多探不漏探)+ `STALL` hazard + `LOCAL_HINT` 权威强度;⚠️ **Phase 2.3 订正落点**(`sharer_tracker.hh` ⇒ 独立头,理由见 design §3.1) |
| **实例声明** | `protocol_instances.json` **+1** `cxl_3_0_symmetric`(13 键 · `perm` **4 值** · `family=industrial` · `sharer_tracker.impl=DcohSymmetric`)+ `repo_owned_instance_ids` **+1** |
| **`perm_upgrades.json`** | **不声明**静默升级(E→M 在本族**未确证** ⇒ 加了即**假声明**)⇒ `no_upgrade_instances` 变 **2** 个 ⇒ 既有 2 处断言**同步更新** |
| **判据** | 正向 + **反向 fixture**(形态 = 「毒化 ⇒ 必红」)· `AC-05` 族无关化**复用**(本族无 unique-clean 态 ⇒ SKIP)⇒ **零改动** |
| **零改动** | `ADR-CEP-*` 正文 · `protocol_instance.schema.json`(值级枚举已含 `REQUESTER` ⇒ 零改动)· `CxlCacheAdapter` · `cxl_state_spec.hh` · `cxl_dcoh_state_spec.hh`(**规约层,只读**) · canonical 10 流清单 · **0 新增** ABI · **0 新增** invariant 编号数 |

---

## Acceptance

| # | 判据 | 形式 |
|---|---|---|
| **CDX-01** | `CxlDcohAdapter` SHALL **零自有表副本** | 去注释后文本 **SHALL NOT** 含 `DevCacheState::` / `DcohViewState::` / `HomeDirState::` / `CxlMemChannel::` / `CxlCacheChannel::` / `ChannelDirection::` / `Perm::` **任何成员字面量** · **SHALL NOT** 含表名(`kPermProjection` / `kDeviceInitiatedSnoopSet` / `kCxlMemChannelDirection` / `kBasis*`)· **配正控**(同机制必须能发现确实存在的标识符) |
| **CDX-02** | adapter SHALL **委托**规约载体 | 逐项委托:`perm_of` / 通道词表 / 判别式 / 基数常量 ⇒ **不得**自有副本 |
| **CDX-03** | adapter SHALL **不暴露** transition API | ⛔ 载体**无转移表** ⇒ adapter **不得**提供 `transition_*`(否则等于**自造转移表** ⇒ 推理填空) |
| **CDX-04** | `DcohSymmetric` 行为 SHALL 符合 traits | `scope=LOCAL_MEMORY_REGION` / `completeness=CONSERVATIVE` / `authority=LOCAL_HINT` / `aggregation_at=HOME` / `hazard=STALL` / `broadcast_fallback=true` —— 行为**逐字段**可核对 |
| **CDX-05** | `LOCAL_HINT` 的**语义代价** SHALL **显式登记** | tracker 答案**只是 hint**,**不可当权威** ⇒ `LocalView` **零改动** ⇒ 双重语义(须先读 `result` 再解释 `forward_to`)**登记** |
| **CDX-06** | 实例声明 SHALL 齐 **13** 键且 `perm` 为 **4 值** | 机器驱动(照 `protocol_instance.schema.json` 的 `required`)· `perm` = NONE/READ/UNIQUE/WRITEBACK · `family=industrial` |
| **CDX-07** | 实例 SHALL **不声明**静默升级 | `perm_upgrades.json` **无**本族行 ⇒ `no_upgrade_instances` **恰含 2** 个(`tilelink_c` + 本族)⇒ **E→M 静默升级未确证** ⇒ 登记为 pending |
| **CDX-08** | `AC-05` 对本族 SHALL 走 **SKIP** | 本族无 unique-clean 态 ⇒ SKIP(不是 FAIL,不得记 PASS)⇒ **复用**既有族无关化 ⇒ 零改动 |

⇒ `ctest` 全绿 · 门禁全 rc=0 · `openspec validate --strict` rc=0 · `format.sh --check` rc=0。

---

## Impact

| 面 | 影响 |
|---|---|
| **代码(新增)** | `include/tlm/cep/cxl_dcoh_adapter.hh` · `include/tlm/cep/cxl_dcoh_tracker.hh`(新 · 独立头 · `DcohSymmetric` 行为类) |
| **代码(既有,零改动)** | `CxlCacheAdapter` · `cxl_state_spec.hh` · `cxl_dcoh_state_spec.hh` · `protocol_instance.schema.json` · `adapter_schema_contracts.hh`(AC-05 复用) |
| **代码(声明面)** | `protocol_instances.json`(+1 实例 · +1 `repo_owned`)· `perm_upgrades.json`(**零行**)· **2 处**既有计数断言更新 |
| **测试** | 新增 adapter + tracker + 实例声明三组用例;反向 fixture |
| **冻结面** | 预期 **0 件**(Phase 0 实测确认) |
| **契约** | **0 新增** ABI · **0 新增** invariant 编号数 |
| **风险 ①** | ⚠️ **`broadcast_fallback=true` 无公开依据** ⇒ 行为按 traits 实现但**登记 pending**;**不改 traits**(改需另一次裁决) |
| **风险 ②** | ⚠️ **`LOCAL_HINT` 与「DCOH 是区域内最终仲裁者」的公开表述有张力** ⇒ 行为**保守**(只给 hint)⇒ 语义代价登记(CDX-05) |
| **风险 ③** | ⚠️ **不得自造转移表** ⇒ adapter 无 `transition_*`(CDX-03) |
| **风险 ④** | ⚠️ **合规** —— 任何时候不得把 CXL 规范正文投喂 AI;pending 项走「人类成员条款摘要」 |
