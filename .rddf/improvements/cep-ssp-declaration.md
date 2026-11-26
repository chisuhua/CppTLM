---
name: cep-ssp-declaration
priority: P1
phase: phase-10
主题: CEP S1 —— 用户自定义稳定态协议（SSP）声明面 + 通用解释器（`CP-I-2`）
依赖:
  - ADR-CEP-24
  - ADR-CEP-02
  - ADR-CEP-05
  - ADR-CEP-12
状态: "🟢 **已批准·待立项**(2026-11-26 · per 用户指令「继续 CP-I-2 立项 → 实施」· 前置 `CP-Q-1a` 已于同日 ratify ⟹ 裁决记录 `.rddf/reviews/cep-CP-Q-1a-adjudication.md` §6.1)"
created: 2026-11-26
---

# cep-ssp-declaration: CEP 用户自定义稳定态协议（SSP）声明面 + 通用解释器

> **来源**：objective `.rddf/roadmap/objectives/obj-cep-custom-protocol.md` 的 **`CP-I-2`**（S1 实施）。
> **架构依据**：`ADR-CEP-24`（✅ 已签发 2026-11-26）· **`CP-Q-1a` 裁决**（✅ 已 ratify：事件字母表 = **新建** + 14 核心锚 + 扩权方 = architect + 先例库摊销）。
> **形态**：**数据声明面 + 通用解释器**（⛔ 不是 DSL / 解析器 / IR / 生成器 / 烘焙 —— `ADR-CEP-24` 决策 1.3 的五件排除物零交集）。

## 架构依据

### 1. 为什么现在做（前置已全部就位）

| # | 前置 | 状态 |
|:--:|---|---|
| 1 | `ADR-CEP-24` 签发（双审 0 BLOCKER）| ✅ 2026-11-26 · commit `4134ed89` |
| 2 | `CP-Q-1a` 事件字母表裁决（用户 ratify R1~R4）| ✅ 2026-11-26 · commit `41e92f1b` · 台账 Entry #552 |
| 3 | 锚集 / 扩权方 / 私有事件规则 | ✅ 已定（14 核心锚 · architect 批准 · 先例库摊销）|
| 4 | 表达力事实登记（3/6 体裁 / 2/6 干净覆盖）| ✅ 已登记（`ADR-CEP-24` SU 第二则）|

### 2. 本仓侧现状（实测 · 2026-11-26）

| # | 事实 | 出处 |
|:--:|---|---|
| F1 | 6 份 `*_state_spec.hh` 中 **3 份**是事件式转移表（`mesif` 6 / `tilelink_c` 4 / `cxl_2_0` 17）| `mesif_state_spec.hh:60-67` · `tilelink_state_spec.hh:71-76` · `cxl_state_spec.hh:76-98` |
| F2 | MESIF / CXL 的格 = `uint8_t` 单目标 + `0xFF` 空洞；**TileLink 的格 = `Transition{kind, target_mask, basis, pending, ref, note}`**（`target_mask` **3 位掩码**）| `mesif_state_spec.hh:132` · `tilelink_state_spec.hh:131-139` |
| F3 | 既有「每协议词表 + flow 映射」先例 = `kChiOpcodeTable`（**25** 条 opcode → `CanonicalFlow`）| `chi_rnf_protocol_engine.hh:139-167` |
| F4 | 事件名逐字断言先例 = `kEventNames`（防 `Tip`→`Trunk` 漂移不可分辨）| `tilelink_state_spec.hh:119-120` |
| F5 | 逐格 `Basis{kind, origin_kind, pending, ref, note}` 五元组 + 「数据只存在这一处」纪律 | `mesif_state_spec.hh:87-99` · `:142-143` |

### 3. 不做会怎样

`ADR-CEP-24` 已签发但**不可实施** ⇒ 「用户自定义协议」仍 = 写 C++ 插件 ⇒ 本 objective 的 S1 必达 DoD 永远挂起 ⇒ `CP-Q-1a` 的裁决胜利用不上。

## Why

把「用户自定义稳定态协议」从「**写 C++ 插件**」降为「**填一份 JSON 声明 + 写适配器**」，且该声明面能**逐格复现**既有手写 `*_state_spec.hh` 的语义（V-24-2 的 `killer` 判据）。

## What Changes

### 交付**增量**（⚠️ 本改善**分两批**交付 —— ⛔ 不一次做全，避免"半截实现"）

| 批 | 内容 | 满足的 DoD |
|:--:|---|---|
| **批 1（本 change）** | ① **事件字母表契约**（`event_alphabet.hh`：14 核心锚 + 私有事件规则 + 原子性 + 逐字断言）② **SSP 声明面**（`ssp.schema.json` + `ssps.json` 数据）③ **loader**（fail-closed）④ **V-24-2 killer 判据**（SSP 数据面表达 MESIF，与 `mesif_state_spec.hh` 的 `kTable` **逐格** diff 相等）⑤ 闸 `cep_ssp_check.sh`（覆盖裁决 §8 的 8 条 AC）⑥ 主 spec | **必达-1 的 ①（可声明）+ ③（逐格对账）** |
| **批 2（另立 change）** | ⑦ **通用解释器**（`ssp_interpreter.hh`：把声明表跑成协议行为）⑧ **装配集成**（SSP → engine 的 seam）⑨ **掩码格扩展**（TileLink 的 `target_mask`）⑩ 「能实验（4 件套）」的端到端判据 | **必达-1 的 ②（装配 + 实验）** |

⚠️ **批 2 必须另立** 的三条理由：① 解释器要**驱动 engine 行为**（时序/在飞语义）⇒ 与 S2（`CP-Q-2`）耦合 ② 掩码格是 **contract 面扩展**（`ssp.schema.json` 的格形状）⇒ 须独立评审 ③ 「实验 4 件套」需要 runner 集成，改动面远超声明面。

## Acceptance（可机判 · 逐条给载体）

> 来源 = `ADR-CEP-24` §6 的 **V-24-1~13** + `CP-Q-1a` 裁决 §8 的 **8 条 AC**。⛔ 只列**批 1 可判**的；批 2 的判据随批 2 立项。

| # | AC | 载体（判据形态） |
|:--:|---|---|
| **A-1** | ✱ **V-24-2 killer**：`ssps.json` 中 MESIF 的 SSP 与 `mesif_state_spec.hh` 的 `kTable` **逐格相等**（含 `0xFF` 空洞 + 逐格 `Basis` 五元组）；**双向**（任一侧改一格 ⇒ 红） | `test/test_cep_ssp_declaration.cc`（逐格 diff）+ 反向 fixture |
| **A-2** | loader **fail-closed**：未知状态 / 未知事件 / 未登记 `Basis.ref` / 第 5 个 `perm` 值 / 非 10 流的 flow 引用 ⇒ 按**正确原因**红（带红因签名） | loader 红因码 + 反向 fixture |
| **A-3** | 事件的 `flow` ∈ canonical 10 流（裁决 AC-1） | schema `enum` + 集合校验 |
| **A-4** | 事件的「核心锚」引用 ∈ **已登记的 14 锚集**（裁决 AC-2） | `event_alphabet.hh` 的常量集 + 集合校验 |
| **A-5** | 私有事件「无等价锚」⇒ 必须有 **architect 批准记录**（裁决 AC-3） | 批准记录存在性校验 |
| **A-6** | 事件名**逐字断言**（裁决 AC-4；先例 `kEventNames`） | 字面清单 + 毒化 |
| **A-7** | 核心锚集**封闭**（新增须改登记常量 ⇒ 触发复核）（裁决 AC-5） | 规模常量 + 反向存在性闸 |
| **A-8** | ⭐ **私有事件原子性**（裁决 AC-7）：⛔ 禁止私有事件引用/组合其他私有事件 | 字面清单 + 反向 fixture |
| **A-9** | ⭐ **「无等价」占比可观测**（裁决 AC-8）：占比 > 半数 ⇒ 触发锚集复审 | 占比统计 + 阈值 |
| **A-10** | **V-24-1**：实例面 `properties` 仍 == 14（⛔ SSP 不进实例面） | `test_cep_schema.cc:443`（既有断言） |
| **A-11** | **V-24-3**：SSP 数据文件是**协议名唯一允许面**，通用层代码零协议名 | 字面清单扫描 + `INV-CEP-07A-IC2` |
| **A-12** | **V-24-5 / V-24-10 / V-24-13**：声明面不表达 belief/token/atomic/瞬态态 · 无「保证无死锁」类措辞 · 事件集 ⊆ 已登记词表 / `family` ⊆ 3 值 | 字面清单 + 反 fixture |
| **A-13** | **V-24-12 回归**：既有 8 实例行为逐字不变（全量 ctest + 55 闸零红） | ctest + `scripts/test/cep_*.sh` |
| **A-14** | 表达力表述用「**3/6 体裁 / 2/6 干净覆盖**」，⛔ 不得写 6/6 | 措辞禁令 + 反向 fixture |

## Capabilities

| 能力 | 形态 |
|---|---|
| `cep-event-alphabet` | 14 核心锚 + 私有事件规则 + 原子性 + 占比阈值 |
| `cep-ssp-declaration` | SSP JSON schema（状态集 / 事件集 / 转移表 / perm 投影 / 逐格 basis）+ 数据 |
| `cep-ssp-loader` | fail-closed 装载 + 引用闭环校验 |

## Impact

| 面 | 影响 |
|---|---|
| 新增头文件 | `event_alphabet.hh`（契约）· `ssp_loader.hh`（装载）—— ⛔ 均不含协议名（协议中立） |
| 新增数据源 | `ssp.schema.json` + `ssps.json` —— ⛔ **不进** `protocol_instance.schema.json`（实例面 14 属性不动）|
| 新增测试 / 闸 | `test/test_cep_ssp_declaration.cc` · `scripts/test/cep_ssp_check.sh` |
| 新增主 spec | `openspec/specs/cep-ssp-declaration/spec.md` ⇒ **冻结面新增 1**（96 → 97）⇒ **五步链 1 次** |
| 登记载体 | 决策块快照不适用（非 ADR）；`decision_adrs` 不变；新 spec 入冻结集 ⇒ 五步链 |
| 零变更面 | ⛔ `protocol_instance.schema.json` · ⛔ 既有 6 份 `*_state_spec.hh`（不迁）· ⛔ 既有 8 实例行为 |
| KPI | 占 **1** 个 openspec slot（0/3 → 1/3）|
