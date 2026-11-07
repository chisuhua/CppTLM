# cpptlm-cep-ualink-coherence-engine: `ualink_4` 的**记账级专用承载**引擎

> **状态**: 🟡 **Proposed**(2026-11-07 立项 —— 用户裁「引擎强度 = **甲(纯记账 + 弱档建模声明)**)」,per 台账 Entry #269;KPI 并发 **0/3 → 1/3**)
> **立项依据**:台账 Entry #268(第 2 项立项 + 三选项待裁)· **#265**(来源检索 + ⚠️ 合规事件 + 两条硬边界)· `ADR-CEP-12` **SU 2026-11-07**(Entry #269)的强度裁决
> **关联**:`.rddf/improvements/cpptlm-cep-ualink-coherence-engine.md`
> **估时**: ~1d

## Why

`ualink_4` 的**声明 ≠ 装配 ≠ 语义**三层中,前两层已闭合(Entry #258),第三层缺:「`request`/`completion` 的**专用承载引擎**仍缺,承载者现为 Null 实现」(per `ADR-CEP-12` SU 2026-11-06 ②)⇒ **任何依赖 UALink 流语义的实验仍不可做**。

但**依据现状**(4 个公开非规范来源 + 6 条一步推断,证据档位 `public_nonnormative`)只支持**弱档建模声明** ⇒ 引擎必须是「**记账 + 建模声明**」,**不是**协议实现 ⇒ ⛔ 禁 UPLI 通道级语义(ReqCmd / 4 通道 / tag 配对)。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | **新引擎** `UalinkProtocolEngine`(`ProtocolEngineBase<2>` 派生):**三分法**观测面(记账 2 / 拒绝 7 / pending 1)+ order-free 在途集合(`txn_id` 恒等) | `include/tlm/cep/ualink_protocol_engine.hh`(新) |
| 2 | **注册**(短名 `UalinkProtocolEngine`,N=2,`bundles::CohIfBundle`)+ 强度标签注释 | `include/chstream_register.hh` |
| 3 | **装配拓扑**(⛔ **不改**已归档 change 的 `configs/cep_ualink_pe_minimal.json`) | `configs/cep_ualink_engine_pe_minimal.json`(新) |
| 4 | **判据**(6 cases):三分法 + 4 条不变量 / completion 不依赖有序性(**中途探针 + 端态 + 决定性变异**)/ `snoop_resp` pending 终态 / 装配闭环(负控 2 条)/ **B3b-2 仍 0/4** | `test/test_cep_ualink_engine.cc`(新) |
| 5 | **契约** | delta spec → `openspec/specs/cep-ualink-coherence-engine/spec.md` |

**Out of Scope**:
- ⛔ **不改** `ualink_4` 的 13 键声明(`flow_support` / `perm` / `vc_binding` / `ordering_assumption`)**一字不改**;
- ⛔ **不实现** UPLI 通道级语义(需条款级依据 ⇒ 当前无);
- ⛔ **不改** B3b 计数(**B3b-2 仍 0/4**)⇒ 本条只闭合「**流语义**(记账级建模声明)」;
- ⛔ **不改** `ProtocolEngineBase` / `MoesiGpuProtocolEngine` / `MesifProtocolEngine` / 已归档配置;
- ⛔ **不得**为 `response` 消息类 / collective 结果回传路径新增任何 handler(= 锚点摘要 I-6 路径)。

## Acceptance

- [ ] **AC-1** 三分法观测面 + **4 条机器可判不变量**全部成立(UGE-02)
- [ ] **AC-2** **completion 记账不依赖有序性** —— 中途探针(**中间 id** ⇒ 在途集合逐格相等)+ 端态(反序 ⇒ 集合空 / unmatched==0 / 计数与正序逐字段相等)+ **决定性变异**(证明判据有牙,非恒真)(UGE-03)
- [ ] **AC-3** `snoop_resp` **pending 终态**:恰好一次登记 / 零「拒绝」计数 / **恒不消费**(对在途集合零影响)/ 对 request-completion 零影响 / 零发射(UGE-04)
- [ ] **AC-4** 装配闭环:`ualink_4` **required** plane 绑定 ⇒ 闸 PASS;负控 2 条(`[IAG-04]` / `[IAG-05]`)**按正确原因红**(UGE-05)
- [ ] **AC-5** **零发射**:引擎从不写 egress(`egress_emitted == 0` + 全部端口 `resp_out()` 无 valid)(UGE-02 / UGE-04)
- [ ] **AC-6** **B3b-2 仍 0/4** 的断言**显式存在**:4 族 deferral 登记完整 + `ualink_4` 声明逐字未变 + 依据档位**不升格**(`public_nonnormative`)(UGE-06)
- [ ] **AC-7** 0 回归:`ctest` 全绿 + 4 闸 + 冻结面授权
- [ ] **AC-8** `07 号` §5.1.1 UALink 行**精确化**(architect 域 · 冻结面两步授权,per Entry #269 §5)
