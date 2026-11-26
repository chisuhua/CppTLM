---
name: cep-ssp-batch2
priority: P1
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖: ADR-CEP-24, ADR-CEP-06, ADR-CEP-14, ADR-CEP-05, ADR-CEP-20, cep-ssp-cp-q1b-landing
来源: 批 1 的 cep-ssp-declaration.md:57（⑦⑧⑨⑩）+ ADR-CEP-24 决策 6 · CP-Q-1b 的 open item
生成时间: 2026-11-26
状态: 🟢 **已批准·待立项**(2026-11-26 · 用户批准 5 段设计 · ⚠️ **硬前置 = cep-ssp-cp-q1b-landing** ·
  ⑦ 解释器取 functional-only)
roadmap_ref:
  project_id: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
  phase: phase-10
---

# cep-ssp-batch2

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
**优先级**: P1 | **来源**: 批 1 的 `cep-ssp-declaration.md:57` + `ADR-CEP-24` 决策 6
**阶段**: phase-10 | **分类**: core-impl | **类型**: feature

## 架构依据

| 依据 | 内容 |
|---|---|
| `ADR-CEP-24` 决策 6 | **交付终点（可判定）**：用户自定义协议实例能被**装配**、能被**实验**（产出 `ADR-CEP-06` 的 4 件套） |
| `ADR-CEP-24` 决策 2.3 | 校验深度 = **形态 + 引用闭环**；⛔ **不承诺**语义（活性 / 无死锁 / 一致性 / 兼容性）|
| `ADR-CEP-24` 决策 6.1 | **洞命中**（运行期事件落 `0xFF`）⇒ 显式违例计数 + 该次实验判「**不可判**」（⛔ 不静默跳过、⛔ 不 panic）|
| `ADR-CEP-24` 决策 6.2 | 双真源收敛：过渡期以手写面为真源，SSP 面经逐格 diff 闸对齐 |
| `INV-CEP-05-IC1` | Stub **只暴露稳定态**；瞬态态与协议私有状态住插件内部 ⇒ ⛔ 生成/解释的瞬态态**不得泄漏**到 Stub 面 |
| `ADR-CEP-20` 决策 6 | 证据**档位声明**义务 + **跨档聚合禁令**（⛔ 档 0 活性禁冒充满档）|
| `ADR-CEP-14` | Stub 可验证性合同测试体系（C1~C10）⇒ SSP 协议须过该体系 |
| 批 1 `cep-ssp-declaration.md:57` | 批 2 的四项：**⑦ 通用解释器** · **⑧ 装配集成** · **⑨ 掩码格扩展** · **⑩ 实验 4 件套** |
| `CP-Q-1b` NIT④ | `combination_contract`（第 14 可选键）的用户可写性**未定** ⇒ 本 change 的 open item |

**依赖（硬前置）**：`cep-ssp-cp-q1b-landing` —— 不落它，本 change 的 ⑧ 装配集成在第一步就红（`family` 不可解析 · `protocol_version` 不在锁表）。

## 范围

**In Scope（4 项 + 1 open item）**

| # | 改动 | 说明 |
|:--:|---|---|
| **⑦** | **functional-only 解释器** `include/tlm/cep/ssp_interpreter.hh` | 把 SSP 声明表跑成**状态迁移 + perm 投影**；**零时序**（不含 hop / 消息级延迟）；洞命中 ⇒ 违例计数 + 不可判（决策 6.1）|
| **⑧** | **装配集成**（SSP → engine 的 seam）| 用户实例的 `id` + `adapter` + **SSP 绑定**三者解析到同一插件；含 **`combination_contract` 的处置**（open item · 倾向**受限用户声明** + CEP 校验，与 `combination_capabilities` 同型）|
| **⑨** | **掩码格扩展** | `ssp.schema.json` 的格形状支持 `target_mask`（TileLink 的 `Cap` = 「源不定、目标为集合」）—— **contract 面扩展** ⇒ 独立评审 |
| **⑩** | **实验 4 件套端到端** | `ADR-CEP-06` 的 `simulation_config.json` + `trace.h5` + `metrics.json` + `report.html` 在 SSP 协议上跑通 |
| + | **合同测试** | SSP 协议过 `ADR-CEP-14` 的 C1~C10 体系 |

**Out of Scope**

- ⛔ **时序 / hop / 消息级延迟** ⇒ S2（`ADR-CEP-25` 待起草；撞台账 Entry #543 + J-3/J-4 翻转）
- ⛔ **瞬态态生成 / 生成器** ⇒ S3（`ADR-CEP-26` 待起草；须 amendment `ADR-CEP-18` + 补 MSD 缺件）
- ⛔ **6 份手写 `*_state_spec.hh` 的迁移**（`ADR-CEP-24` §8 F3）
- ⛔ **档 2 证据扩面**（F4）
- ⛔ `family` / `protocol_version` 的 contract 面（= 前置 `cep-ssp-cp-q1b-landing`）

## Why

批 1 只到「**能声明**」—— SSP 是一张通过校验的表，但**不能被任何装配路径消费、不能产出实验产物**。objective `obj-cep-custom-protocol` 的**必达 DoD** 是「用户可在不写 C++ 的前提下定义协议；该协议能**装配**、能**实验**；数据面与手写面**逐格对账相等**（V-24-2）」—— 前两项（装配 / 实验）**只在本 change 交付**。

⇒ 本 change 是把「声明面」变成「可运行、可实验的协议」的那一步。

## What Changes

1. **新增 `ssp_interpreter.hh`**（header-only，per `include/tlm/cep/AGENTS.md` 铁律 1）：读 `ssps.json` 的声明表 → 状态迁移 + perm 投影；洞命中 ⇒ 违例计数（⛔ 不静默跳过）。
2. **装配 seam**：SSP ↔ `protocol_instance` ↔ adapter/engine 的三方绑定；`combination_contract` 的处置落定。
3. **`ssp.schema.json` 掩码格**：`Transition{kind, target_mask, basis, pending, ref, note}` 形态（对齐 `tilelink_state_spec.hh:131-139` 的既有实现）—— 冻结面。
4. **runner 集成**：`ADR-CEP-06` 的 4 件套产出路径接上 SSP 协议。
5. **合同测试**：`ADR-CEP-14` C1~C10 在 SSP 协议上的实例化。

## Acceptance

- [ ] 用户自定义 SSP 协议**可装配**（实例 `id` + `adapter` + SSP 绑定三者解析成功）
- [ ] 该协议可产出 `ADR-CEP-06` 的 **4 件套**（`simulation_config.json` + `trace.h5` + `metrics.json` + `report.html`）
- [ ] **洞命中** ⇒ 显式**违例计数** + 该次实验判「**不可判**」（⛔ 不静默跳过、⛔ 不 panic）
- [ ] **掩码格**可表达 TileLink 形态（`target_mask` 3 位掩码；含 1 正 1 负）
- [ ] **瞬态态零泄漏**到 Stub 面（`INV-CEP-05-IC1` 回归；⛔ 解释器的瞬态态只住插件内）
- [ ] 实验证据**声明档位**（`ADR-CEP-20` 决策 6；⛔ 档 0 活性禁冒充满档 · ⛔ 跨档聚合）
- [ ] **合同测试**覆盖（`ADR-CEP-14` C1~C10 在 SSP 协议上的实例化）
- [ ] **既有 8 实例 + 批 1 的 `mesif_ssp` 零回归**（全量 ctest + 全部 `cep_*.sh` 绿）

## Capabilities

- 用户自定义协议**端到端可用**：声明 → 装配 → 实验 → 4 件套
- 支持**两种格形态**：单目标 `0xFF`（MESIF / CXL 形）+ **掩码** `target_mask`（TileLink 形）
- 洞命中**可检出**且**不静默**（违例计数 + 不可判）

## Impact

| 面 | 影响 |
|---|---|
| 新增文件 | `include/tlm/cep/ssp_interpreter.hh`（header-only）· 装配 seam · runner 集成 · 合同测试 |
| **冻结面** | `include/tlm/cep/ssp.schema.json`（**掩码格扩展**）⇒ 五步链 |
| **objective 必达 DoD** | ✅ 可达（「能装配 + 能实验」由本 change 交付）|
| 既有实现 | 8 实例 + `mesif_ssp` **零行为变更** |
| ⚠️ 时序面 | **零改动** —— 瞬态态**无时长**（时长 = 链路时延 + 对端处理，属 S2）⇒ ⛔ 不得把本 change 的实验结论表述为「协议语义已经真实链路验证」 |
| ⚠️ 表述纪律 | 本 change 验收后**方可**说「CEP 已支持用户自定义协议」；⛔ 在任何 S2 / S3 表述上仍须遵守 `ADR-CEP-20` 的档位纪律 |
