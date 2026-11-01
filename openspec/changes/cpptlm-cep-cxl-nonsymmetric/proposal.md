# cpptlm-cep-cxl-nonsymmetric: CXL 2.0 非对称 —— **实现层**(adapter + `SparseSnoopFilter` 行为实现 + 实例声明)

> **状态**: 🟡 **Proposed**(2026-11-01)· **队列**: `obj-cep-platform §4.0` **#10** 的**第 1 个 change**(两段式的后半段)
> **上游**: 第 0 个 change `cpptlm-cep-cxl-nonsymmetric-state-spec` **已完成归档**(2026-11-01,Entry #127)
> ⇒ 规约层 main spec = `openspec/specs/cep-cxl-nonsymmetric-state-spec/spec.md`(1 Requirement + CSS-01 ~ CSS-06)
> **口径裁决**: 台账 **Entry #122**(7 条)+ `ADR-CEP-11` 的 `## Status Update(2026-11-01)`
> **规划输入**: `.rddf/improvements/cpptlm-cep-cxl-nonsymmetric.md`(2026-11-01 修订版)
> **KPI**: Stage 1 = **1 slot**;Proposed 并发 **0/3 → 1/3**
> **冻结面**: **零正文改动** —— 仅 `07 号`(§5.1.1 CXL 行措辞对齐)+ `ADR-CEP-04`(第 3 个行为实现登记)
> **各自追加 `## Status Update`**,分别经台账 entry **两步授权**

---

## Why

### ① 规约层已闭合,但**实现面仍是空的**

第 0 个 change 交付了**可执行规约载体**(`include/tlm/cep/cxl_state_spec.hh`):
4 态 · 4→4 投影 · **三态 68 格矩阵** · 非对称结构断言 · 逐格 `basis`。
⇒ 但**没有任何生产代码消费它**:全仓零 include 点。

### ② #9 的教训:缺规约就实现 ⇒ 实现必然自推

`MesifAdapter`(`mesif_adapter.hh`)与 `MoesiGpuAdapter`(`moesi_gpu_adapter.hh`)**形态不一致**:
前者**委托**规约载体(零自有表副本),后者**自带 6×6 表**。
⇒ 规约改一格,自带表的 adapter **不会红** ⇒ 双真源。
⇒ 本 change 的 `CxlCacheAdapter` SHALL **委托** `CxlStateSpec`(照 `MesifAdapter` 先例)。

### ③ R11:`SparseSnoopFilter` 的声明面承诺**至今未兑现**

`moesi_gpu` 的实例声明写 `sharer_tracker.impl = "SparseSnoopFilter"`
(`protocol_instances.json`),而该实现**只有 traits 行、无行为类**
⇒ 台账 §8.1 **R11** 登记为 🟡 中;归属经 **Entry #112**(去重)· **Entry #124**(复核维持)
落在**本 change**(同载体 `sharer_tracker.hh`)。
⚠️ 且 `chi_rnf_adapter.hh:172` 明载:`moesi_gpu` 的 `O→WRITEBACK` 投影损失
**由** `SparseSnoopFilter`(`scope=SYSTEM`)**补偿** ⇒ 该补偿**当前无行为实现**。

### ④ CXL 实例的 peer 集**平凡为空** ⇒ 实例声明须诚实

按口径 ③(host-centric / 单一一致性点):CXL 2.0 每 root port 只 1 个 cache device
⇒ **设备侧 sharer 集平凡为空** ⇒ CXL 实例的 `sharer_tracker` SHALL 声明为
**「不参与」**(照 `chi_rnf` 先例)⇒ **不**给 CXL 实例挂 SF。

---

## What Changes

| # | 交付物 | 落点 | 性质 |
|---|---|---|---|
| 1 | **`CxlCacheAdapter`** | `include/tlm/cep/cxl_cache_adapter.hh`(新) | header-only · **不进 `ModuleFactory`** · 委托 `CxlStateSpec` |
| 2 | **`SparseSnoopFilter` 行为实现** | `include/tlm/cep/sharer_tracker.hh`(追加) | `public SharerTrackerIf, public SnoopMechanics` · 复用 `kTrackerTraits[1]` |
| 3 | **CXL 实例声明** | `include/tlm/cep/protocol_instances.json` | 新实例 `cxl_2_0` + `repo_owned_instance_ids` |
| 4 | **静默升级登记** | `include/tlm/cep/perm_upgrades.json` | 新增一行 `UNIQUE → WRITEBACK` |
| 5 | **冻结面 2 件**(追加 `## Status Update`) | `docs/architecture/07-coherence-exploration-platform.md` · `docs/adr/ADR-CEP-04-*.md` | 两步授权 |

### ⚠️ `Unknown` 格在实现层的**消费纪律**(本 change 的核心判据)

规约层的 68 格是**三态**:`Known` 26 / `Illegal` 0 / `Unknown` 42。
⇒ **实现层消费 `Unknown` 格时 SHALL 走显式降级路径**(保守行为 + 事件登记),
**SHALL NOT** 静默当作 `Illegal`、**SHALL NOT** 臆断目标态。
⚠️ 这是 MESIF 先例的**反面**:mesif 用 `0xFF = 未定义(空洞)` ⇒ 消费者会把「未知」读成「非法」
⇒ 本载体**显式区分**三态(per 规约层 `design.md D-γbis`)。

---

## Acceptance Criteria

| # | 判据 | 可执行证据 |
|---|---|---|
| **AC-1** | `CxlCacheAdapter` **零自有表副本**:去注释后文本 SHALL NOT 含 `State::`/`Event::`/`Perm::` 字面量 | 静态扫描器 + 扫描器**鉴别力**自测(注入一个字面量 ⇒ 必红) |
| **AC-2** | adapter 的转换查询 SHALL 区分 `Known`/`Illegal`/`Unknown` 三态,`Unknown` SHALL NOT 被折叠成目标态 | 68 格逐格 pass-through + `Unknown` 格抽样 |
| **AC-3** | `SparseSnoopFilter` 的 caps/behavior SHALL **逐字段**等于 `ADR-CEP-04 §2`/`§4`(`SYSTEM`/`CONSERVATIVE`/`GLOBAL`/`HOME` + `Hazard_STALL` + `broadcast_fallback=true`) | 三方相等断言(traits / 类 / ADR 引用) |
| **AC-4** | `SparseSnoopFilter` 的 **CONSERVATIVE 语义**:SHALL **不**给出「无其他 sharer」的**精确**结论;只能给「**可能**还有其他 sharer」的保守近似 | 反向 fixture:构造「filter 说独占但实际有其他 sharer」⇒ **必红**(漏探 = 正确性失败) |
| **AC-5** | `moesi_gpu` 的 `O→WRITEBACK` 补偿路径 SHALL 有**可执行**判据(消费 SF 的保守查询) | 端到端:SF 返回保守结论 ⇒ 补偿路径被触发并登记 |
| **AC-6** | CXL 实例声明 SHALL 通过 schema(`13` required 键)+ 声明闸;`sharer_tracker.impl` SHALL 为「不参与」 | `openspec/specs` 校验 + 声明面对照测试 |
| **AC-7** | `perm_upgrades.json` SHALL 新增 `cxl_2_0` 的 `UNIQUE → WRITEBACK` 一行,SHALL NOT 引入第 5 个 `perm` 值 | 跨 schema `$ref` 校验(机制上不可能) |
| **AC-8** | **冻结面 2 件** SHALL 仅追加 `## Status Update`(正文零改动),provenance 对齐 | `cep_freeze_check` 两步授权闭环 |
| **AC-9** | 门禁全绿:CEP 脚本族 + `docs_sync --strict` + `doc_ref --strict` + `gate_selftest` + `format --check` + 全量 `ctest` | 退出码 |

---

## Capabilities

| Capability | 状态 | 内容 |
|---|---|---|
| `cep-cxl-nonsymmetric` | **ADDED** | `CxlCacheAdapter`(委托规约载体)+ CXL 实例声明 + 静默升级登记 + 三态消费纪律 |
| `cep-sparse-snoop-filter` | **ADDED** | `SparseSnoopFilter` 行为实现(caps/hazard 逐字段对齐 `ADR-CEP-04`)+ CONSERVATIVE 语义 + R11 补偿路径 |

---

## Impact

| 面 | 影响 |
|---|---|
| **队列 / KPI** | #10 实现层占 **1 slot**(0/3 → 1/3)⇒ 归档后回 0/3 |
| **协议实例** | `implemented` 实例 **3 → 4**(新增 `cxl_2_0`);⚠️ `moesi_gpu` 的声明**不变**(SF 落地后其声明**兑现**) |
| **SharerTracker** | 行为实现 **2 → 3**;`SparseSnoopFilter` 补上行为面 ⇒ **R11 可降级**(须 V-1∧V-2∧V-3 全达成) |
| **冻结面** | `07 号`(§5.1.1 CXL 行措辞对齐裁决 ③⑦)+ `ADR-CEP-04`(第 3 个行为实现登记)⇒ **两步授权** |
| **跨仓债** | **无** —— 不镜像进 ArchForge `32 号 §6.1`(产品侧注册表);CXL 实例**只在本仓**声明 |
| **残余风险** | ① 42 格 `Unknown` 在实现层的**降级行为**未被规范正文确定 ⇒ 只能按**保守**方向实现 ② 许可待人工确认(D2/D4/D14;**未**被本 change 的 `pending=false` 格引用)③ `SparseSnoopFilter` 的 **CONSERVATIVE 精度**无规范原文可依 ⇒ 只作**协议骨架**,不作精度依据 ④ **不产出任何性能结论**(CXL 非对称 workload 在 H7 声明范围之外) |
| **对后续队列的影响** | #12(`cxl-dcoh-symmetric`)可复用本 change 的 adapter + SF ⇒ 非对称 → 对称的差异面被**隔离**在 #12 |
