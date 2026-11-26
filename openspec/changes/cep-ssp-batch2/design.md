# design: cep-ssp-batch2 —— 用户自定义协议的运行时（解释器 + 装配集成 + 掩码格 + 实验 4 件套）

> **Change name**: `cep-ssp-batch2`（slug，**不带日期前缀**）
> **配套**: `openspec/changes/cep-ssp-batch2/`（proposal / design / specs×3 / tasks）
> **来源**: `.rddf/improvements/cep-ssp-batch2.md`（✅ 已批准·待立项 · 用户批准 5 段设计 · ⑦ 取 functional-only）
> **架构依据**: `ADR-CEP-24` 决策 6（交付终点）/ 2.3（只保形态 + 引用闭环）/ 6.1（洞命中 ⇒ 违例计数 + 不可判）/ 6.2（双真源收敛）· `ADR-CEP-06`（4 件套）· `ADR-CEP-14`（C1~C10）· `ADR-CEP-20` 决策 6（证据档位声明）· `INV-CEP-05-IC1`（瞬态态零泄漏）
> **生成时间**: 2026-11-27 · **生成方式**: builder 手写（design.md ⛔ **不由 rdd-builder 生成**）
> **授权**: 台账 **Entry #557**（立项 + 硬前置已解除 + 五步链步 1）

---

## 0. 决策摘要（一句话）

把批 ① 交付的「**能声明**」接到「**能装配 + 能实验**」：新增 functional-only 解释器、把 **SSP 绑定**作为装配闸的新一维、把声明面的格形状扩为**两形态**（单目标 + 掩码），并让用户自定义协议跑通 `ADR-CEP-06` 的 4 件套 ⇒ objective `obj-cep-custom-protocol` 的 **S1 必达 DoD 全达成**。

## 1. 背景：批 ① 之后还差什么

批 ①（`cep-ssp-declaration`，已归档）交付的是**一张通过校验的表**。它**不能**被任何装配路径消费，也**产不出**实验产物 ⇒ objective 的 S1 必达 DoD「能装配 + 能实验」两项**未交付**。

| 面 | 现状（实测） | 缺口 |
|---|---|---|
| 声明面 | ✅ `ssp.schema.json` + `ssps.json` + fail-closed loader + V-24-2 逐格对账 | — |
| 装配面 | `instance_assembly_gate.hh` 判 `id` / `family` / `bridge.modules` / `bridge_id` / plane 绑定 | ⛔ **不判 SSP 绑定**（用户协议无从挂载） |
| 运行面 | 无解释器 | ⛔ 声明表**不可执行** |
| 格形态 | `transition` 单元 = **目标 state 名 或 null**（单目标） | ⛔ 表达不了 TileLink 的 `Cap`（源不定 ⇒ 目标为集合）|
| 实验面 | 4 件套产自手写协议 | ⛔ 用户自定义协议**跑不出** 4 件套 |

## 2. 关键决策

### D-1：解释器 **functional-only**（零时序）

**决策**：解释器只做**状态迁移 + perm 投影**，**不产出任何时长 / 延迟 / hop 数**。

**理由**：`ADR-CEP-24` 决策 2.3（只保形态 + 引用闭环 · ⛔ 不承诺语义）+ 改进的 ⑦ 明写「**零时序**（不含 hop / 消息级延迟）」。

**代价**：实验结论**只能**支撑「形态 + 引用闭环 + 路径延迟常数」档 ⇒ **不得**表述为「协议语义已真实链路验证」（`ADR-CEP-20` 决策 6 的档位纪律 + `ADR-CEP-25` 待起草）。⇒ 本 change 的 4 件套**必带档位声明**。

### D-2：洞命中 ⇒ **显式违例计数 + 该次实验判「不可判」**（⛔ 不静默跳过 / 不 panic）

**决策**：事件的目标状态落在声明表 `0xFF` 空洞 ⇒ 解释器**显式计数**该次违例，且承载该次运行的实验判「**不可判**」。

**理由**：`ADR-CEP-24` 决策 6.1（洞命中 ⇒ 显式违例计数 + 该次实验判「不可判」）+ 脚本自带的归因纪律（「不可判 / 不空转」SHALL NOT 被表述为「已覆盖 / 已证明」）。

**代价**：洞命中**不是**错误路径而是**可观测结果** ⇒ 解释器接口须把「违例计数」作为**一等输出**（⛔ 不能只返回 bool）。

### D-3：掩码格扩展 = **声明面格形状扩为两形态**（`transition` 单元可为「目标 state 名」**或**「掩码对象」）

**决策**：`ssp.schema.json` 的 `transition` 单元类型由 `string | null` 扩为 `string | null | {target_mask: int}`（掩码形），形态对齐 `tilelink_state_spec.hh:131-139` 的既有 `Transition{kind, target_mask, basis, pending, ref, note}`。

**理由**：TileLink 的 `Cap` 语义 = 「源不定 ⇒ 目标为集合」⇒ 单目标格**表达不了**；`ADR-CEP-24` 决策 6.2（双真源收敛：过渡期以手写面为真源，SSP 面经逐格 diff 闸对齐）。

**约束**：
- ⛔ **不新增** `transition` 之外的字段（`states` / `events` / `perm` / `basis_*` 零改动）；
- ⛔ 掩码形**只表达目标集合**，⛔ **不**引入时序/hop；
- ⚛� `target_mask` 位宽 = **3**（对齐手写面 `tilelink_state_spec.hh:134`）；
- ⚛� **V-24-2 killer 判据扩为覆盖两形态**（`cep-ssp-declaration` 的 MODIFIED delta 已落）⇒ 掩码形也须与手写面**逐位**相等。

### D-4：装配闸新增「**SSP 绑定**」一维（归因 tag `[IAG-09]`）

**决策**：装配闸在既有判据之外，新增「实例声明的 SSP 绑定」这一维：`id` + `adapter` + SSP **三方解析到同一插件**；任一环缺失 ⇒ 判红。

**理由**：`ADR-CEP-24` 决策 6（能装配）+ 批 ① 之后的直接缺口（`instance_assembly_gate.hh` 只判 `id`/`family`/`bridge.*`/`plane`）。

**约束**：
- ⛔ **不复用 `[IAG-08]`**（已被既有用例「装配闭环与既有闸一致」占用 —— 先例 `2026-11-10-cpptlm-cep-bridge-reattach-guard` 的显式禁令）⇒ 新 tag = **`[IAG-09]`**；
- ⛔ 装配路径臂与离线 CLI 臂**共用同一份判据实现**（既有 `两臂 SHALL 共用同一份判据实现与加载规则` Scenario 零改动）；
- ⛔ SSP 绑定只覆盖**形态 + 引用闭环**（⛔ 不承诺语义）—— 已写入 MODIFIED delta 的「不可约残余」④。

### D-5：实验 4 件套复用**既有**产出路径（⛔ 不新建平行管线）

**决策**：用户自定义协议的 4 件套走**既有** `ADR-CEP-06` 的产出路径（`simulation_config.json` → `trace.h5` → `metrics.json` → `report.html`），⛔ **不新建**平行实验管线。

**理由**：4 件套的**判据**（`ADR-CEP-06`）与产出**路径**（`tools/run_sweep.py` / `render_report.py` / `ab_compare.py`）已存在且**在 M1 冻结集内** ⇒ 新建平行管线 = 第二份真源 + 额外冻结面改动。

**代价**：若既有路径**只**认手写协议 ⇒ 需在其上加一个「协议来源 = SSP」的分支（⛔ **不得**复制整条管线）⇒ **以 P1 实测为准**（见 plan Task 4）。

### D-6：合同测试沿用 `ADR-CEP-14` C1~C10（⛔ 不新建合同体系）

**决策**：SSP 协议的合同测试 = 在既有 `ADR-CEP-14` C1~C10 体系上**实例化**，⛔ 不新建体系。

**理由**：合同体系是**跨协议**的（Stub 可验证性）⇒ 新建体系 = 与既有体系语义重叠且制造「哪套是准」的歧义。

### D-7：`combination_contract` 的用户可写性 = ⑧ 内的**受限用户声明**（倾向）

**决策**：`combination_contract`（第 14 可选键）走「**受限用户声明** + CEP 校验」，形态与 `combination_capabilities` 同型。

**理由**：`CP-Q-1b` NIT④ 留下的 open item；「受限 + 校验」与 `combination_capabilities` 的既有处置同型 ⇒ **不新造**第三种自由度形态。

**⚠️ 裁时的边界**：⛔ **不改** `INV-CEP-12-IC1` 的 13 必含个数（`combination_contract` 仍是**可选**声明项）· ⛔ **不**放开为自由可写。

### D-8：⚠️ **⑨ 的 contract 面落在未冻结件上**（architect 待裁）

**决策**：本 change **照实施**（⑨ 掩码格扩展），**不擅自**扩 M1 冻结集；同时把「`ssp.schema.json` / `ssps.json` 是否纳入 M1」**登记为 architect 域待裁**（台账 Entry #557 ⑥）。

**理由**：批 ① 新建这两个声明源时**未入 M1 集**（实测清单无此二件）⇒ 掩码格形状（contract 面）目前**可被静默改写**；而**冻结集成员资格**是 M1 机制的 architect 裁决面（先例：Entry #310 提到「另立 Entry + 两步授权」）。

**代价**：批 ② 期间该 contract 面无 M1 保护 ⇒ **缓解**：本 change 的掩码形态须由**合同测试 + 逐位对账闸**覆盖（⛔ 不依赖冻结清单）。

## 3. 影响面

| 面 | 影响 | 冻结面 |
|---|---|:--:|
| `include/tlm/cep/ssp_interpreter.hh`（**新头** · header-only）| ⑦ 状态迁移 + perm 投影 + 违例计数 | — |
| `include/tlm/cep/ssp.schema.json` | ⑨ `transition` 单元扩为两形态 | ⚠️ **不在 M1 集**（见 D-8）|
| `include/tlm/cep/instance_assembly_gate.hh` | ⑧ 新增 SSP 绑定一维（`[IAG-09]`）| — |
| `include/tlm/cep/ssp_loader.hh` | 掩码形校验（E5 维度一致性扩为「单元可为掩码对象」）| — |
| `openspec/specs/cep-ssp-declaration/spec.md` | ⑨ 逐格对账扩为两形态 | ✅ **M1 五步链** |
| `openspec/specs/cep-instance-assembly/spec.md` | ⑧ 装配判据扩 SSP 绑定维 | ✅ **M1 五步链** |
| `openspec/specs/cep-ssp-runtime/spec.md`（**新 main spec**）| ⑦⑩ 运行时契约 | ✅ **M1 新增**（glob 自动扩）|
| `experimental/preregistration.json` / `tools/*.py` / `experimental/metrics.schema.json` / `docs/architecture/cep/simulation-and-experiment.md` | ⑩ 4 件套路径（**若触及**）| ✅ **M1 五步链**（命中即走）|
| `include/tlm/cep/protocol_instance.schema.json` | ⑧ 若扩实例声明面 | ✅ **M1 五步链**（命中即走）|
| 既有 8 实例 + 批 ① `mesif_ssp` | **零行为变更** | — |
| 时序面 | **零改动** | — |

## 4. 风险与缓解

| # | 风险 | 缓解 |
|:--:|---|---|
| R-1 | **洞命中被静默吞掉**（解释器跳过 ⇒ 实验假绿）| D-2：违例计数为一等输出 + 该次实验判「不可判」+ 反向 fixture（洞命中 ⇒ 计数 == 1 且实验判不可判）|
| R-2 | **掩码格与手写面漂移**（两形态对账漏检）| D-3：V-24-2 killer 判据扩为**两形态逐位**对账 + 双向（任一侧改一格 ⇒ 必红）|
| R-3 | **装配闸两臂不同源**（执行臂漏检 ⇒ 「只测不拦」）| D-4：沿用既有「两臂共用同一份判据实现」Scenario（零改动）+ 执行臂反向 fixture |
| R-4 | **时序被偷偷引入**（解释器带出延迟 ⇒ 档位纪律被破）| D-1：接口层**不产出**任何时长/hop + 「零时序」反向断言 + 表述纪律写入 spec |
| R-5 | **⑨ 的 contract 面无 M1 保护** | D-8：登记 architect 待裁 + 本 change 用**合同测试 + 逐位对账**兜底（不依赖冻结清单）|
| R-6 | **瞬态态泄漏到 Stub 面** | D-2 + `INV-CEP-05-IC1` 回归用例 |
| R-7 | **既有 8 实例 / 批 ① `mesif_ssp` 回归** | 零行为变更（只扩取值域 + 增一维判据）· 全量 ctest + 全部 `cep_*.sh` 绿 |

## 5. Verification（可勾选验收判据 · 与 proposal §验收标准 一致）

- [ ] 用户自定义 SSP 协议**可装配**（实例 `id` + `adapter` + SSP 绑定三者解析成功）
- [ ] 该协议可产出 `ADR-CEP-06` 的 **4 件套**（`simulation_config.json` + `trace.h5` + `metrics.json` + `report.html`）
- [ ] **洞命中** ⇒ 显式**违例计数** + 该次实验判「**不可判**」（⛔ 不静默跳过、⛔ 不 panic）
- [ ] **掩码格**可表达 TileLink 形态（`target_mask` 3 位掩码；含 1 正 1 负）
- [ ] **瞬态态零泄漏**到 Stub 面（`INV-CEP-05-IC1` 回归；⛔ 解释器的瞬态态只住插件内）
- [ ] 实验证据**声明档位**（`ADR-CEP-20` 决策 6；⛔ 档 0 活性禁冒充满档 · ⛔ 跨档聚合）
- [ ] **合同测试**覆盖（`ADR-CEP-14` C1~C10 在 SSP 协议上的实例化）
- [ ] **既有 8 实例 + 批 1 的 `mesif_ssp` 零回归**（全量 ctest + 全部 `cep_*.sh` 绿）

## 6. 表述纪律（⛔ 交付时必须遵守）

- ⛔ 本 change 验收后**方可**说「CEP 已支持用户自定义协议」；在此之前任何表述**不得**含此断言
- ⛔ 本 change 的实验结论**只**支撑「形态 + 引用闭环 + 路径延迟常数」档 ⇒ ⛔ **不得**表述为「协议语义已经真实链路验证」（`ADR-CEP-20` 决策 6 · 档 0 禁冒充满档）
- ⛔ 洞命中的那次实验**不得**记为「一致」或「通过」
