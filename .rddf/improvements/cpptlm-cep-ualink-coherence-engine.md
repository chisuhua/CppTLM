---
name: cpptlm-cep-ualink-coherence-engine
priority: P2
phase: phase-10
依赖:
  - ADR-CEP-12
  - ADR-CEP-04
  - ADR-CEP-01
状态: ✅ **已完成并归档**(2026-11-07,per 台账 Entry #268 立项 → **#269** architect 裁决(用户裁「引擎强度 = **甲**」+ oracle→critic)→ **#270** 实施 → **#271** 归档;delta 合并进 `openspec/specs/cep-ualink-coherence-engine/spec.md`)
created: 2026-11-07
---

# improvement: cpptlm-cep-ualink-coherence-engine

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

## 架构依据

- `07 号` **SU 2026-11-06 · 四** D-1(来源效力):公开非规范来源 ⇒ **不构成** `Direct`、**不计入** B3b;
- 台账 **Entry #265** 的**锚点摘要** `docs/research/2026-11-07-cep-ualink-flow-semantics-anchor-summary.md`:4 个互相独立的公开非规范来源 + 6 条一步推断(I-1 `request`↔UPLI Request · I-2 `completion`↔UPLI Response · I-3 `snoop=false` · I-4 `query`↔ReadReduce · I-5 `atomic`↔AtomicR/NR · I-6 `response`↔归约结果回传(**最弱**));
- Entry #258:「**装配**已闭合」,但承载者是 **Null** 实现 ⇒ 「**流语义**未闭合」;
- Entry #265 §④ 的两条硬边界:① J1 的 `p2p_ordered` 是**建模假设**而非协议保证 ⇒ ⛔ **completion 记账不得依赖有序性假设**;② `snoop_resp` **无语义依据**(该类语义仍待定)⇒ ⛔ 不消费。

## 范围

**In-Scope**:

1. **新引擎** `include/tlm/cep/ualink_protocol_engine.hh`(`ProtocolEngineBase<2>` 派生):
   - `on_request` / `on_completion` = **canonical 流记账**(与 `MoesiGpuProtocolEngine` 同型的「只记账」形态);
   - 其余 8 流(`snoop` / `snoop_resp` / `writeback` / `forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack`)= **显式 Unsupported 记账**(`unsupported_flow_rejected` 计数)⇒ ⛔ 零语义发明;
   - `snoop_resp` 与 I-6(collective 结果回传)**显式 pending / 不消费**;
   - ⛔ **不实现** UPLI 协议语义(ReqCmd 编码、4 通道时序、transaction tag 配对)—— 那是**条款级依据**才允许的强度;
2. **注册**(短名 `UalinkProtocolEngine`,N=2,`bundles::CohIfBundle`)+ **装配拓扑** `configs/cep_ualink_engine_pe_minimal.json`(bridge `protocol_instance = "ualink_4"`);
3. **L4 判据**:10 流逐条记账 + Unsupported 计数 + **「completion 记账不依赖有序性」**的判据 + 装配闸 PASS。

**Out of Scope(明确不做)**:

- ⛔ 不改 `ualink_4` 的声明(`flow_support` / `perm` / `vc_binding` / `ordering_assumption`)**一字不改**;
- ⛔ 不实现 UPLI 通道级语义(需条款级依据 ⇒ 当前**无**);
- ⛔ 不把 `ualink_4` 计入 B3b(**仍 0/4**)⇒ 本条只闭合「**流语义**(弱档建模)」,⛔ 不动计数;
- ⛔ 不改 `07 号` §5.1.1 的措辞(除「流语义」一句的**精确化**,需 architect 授权 + 两步冻结面)。

## Why

装配已闭合但**语义仍缺** ⇒ `ualink_4` 的 `request`/`completion` 在仓内**没有承载者** ⇒ 任何依赖 UALink 语义的实验都无法做 ⇒ 这正是「**声明 ≠ 装配 ≠ 语义**」三层的最后一层。
而依据现状(公开非规范 + 一步推断)只支持**弱档建模声明** ⇒ 引擎必须是「**记账 + 建模声明**」,**不是**协议实现。

## What Changes

| 变更 | 文件 |
|---|---|
| 引擎头 | `include/tlm/cep/ualink_protocol_engine.hh`(新) |
| 注册 | `include/chstream_register.hh` |
| 装配拓扑 | `configs/cep_ualink_engine_pe_minimal.json`(新) |
| 判据 | `test/test_cep_ualink_engine.cc`(新) |
| 契约 | delta spec → `openspec/specs/cep-ualink-coherence-engine/spec.md` |
| 文档 | `07 号` §5.1.1 UALink 行(architect 域,冻结面两步授权)· `ADR-CEP-12` SU |

## Acceptance

- [ ] **AC-1** `on_request` / `on_completion` 逐条记账;8 条未支持流**显式**计入 `unsupported_flow_rejected` ⇒ **零语义发明**
- [ ] **AC-2** **completion 记账不依赖有序性假设**(⛔ 不得因乱序到达而改变计数/结论)—— 有**反向 fixture**(乱序投递 ⇒ 断言不变)
- [ ] **AC-3** `snoop_resp` 与 I-6 路径**显式 pending / 不消费**;`perm` 恒 `NONE`
- [ ] **AC-4** 装配闭环:拓扑挂载 + 装配闸 PASS(`ualink_4` **required** plane 绑定)⇒ 与 Entry #258 的装配面一致
- [ ] **AC-5** 0 回归:`ctest` 全绿 + 4 闸 + 冻结面授权
- [ ] **AC-6** **B3b-2 仍 0/4** 的断言显式存在(本条不改计数)⇒ 与 `07 号` SU · 六 / Entry #264 一致

## Capabilities

- **第三层闭环**:`ualink_4` 从「已声明 + 已装配」推进到「**有流语义承载者(弱档建模声明)**」;
- **实验可做性**:依赖 UALink `request`/`completion` 记账的实验(如 collective plane 的 INC 原语路径)有了可执行的承载点。

## Impact

- **公共面**:新引擎头 + 注册表 +1;
- **契约**:新增 1 份 main spec;
- **测试**:新增 1 个测试文件;
- **冻结面**:1 处(`07 号` §5.1.1 精确化)+ `ADR-CEP-12` SU ⇒ 两步授权;
- **KPI**:占 1 slot(归档后回 0/3);
- **⚠️ 不改 B3b 计数**:B3b-2 仍 **0/4**。

## 待 architect 裁决(实施前必须先裁)——「引擎强度」三选项

| 选项 | 引擎强度 | 依据要求 | 代价 |
|---|---|---|---|
| **甲(建议)** | **纯记账 + 弱档建模声明**(canonical 流 ↔ UPLI 概念的一次推断,显式标注) | 现有锚点摘要即足 | ⚠️ **不**是协议实现 ⇒ 不可据此声称「UALink 语义已实现」;须在契约与 `07 号` 双处写明 |
| 乙 | **实现 UPLI 通道级语义**(ReqCmd / 4 通道 / tag 配对) | 需**条款级**依据(人类成员条款摘要,per D-4 ②)⇒ **当前无** | ⛔ 现在做 = 推理填空 ⇒ **禁止** |
| 丙 | 不做 | — | 「流语义」缺口保留 |

⇒ **建议甲**,但须同时明确:① 契约标注「弱档建模声明,非协议实现」;② `07 号` §5.1.1 的措辞**不得**升级为「语义已实现」。
