---
name: cpptlm-cep-dual-port-inbound-dispatch
priority: P0
phase: phase-10
category: core-impl
type: fix
状态: ✅ 已归档(2026-11-09,per 台账 Entry #311 —— ✅ 已完成归档语义:**零 delta spec**(`.openspec.yaml` 的 `skip_specs: true`,本 change **不改任何 main spec 的契约面**)⇒ 归档时**不合并**任何 delta;归档件 = `openspec/changes/archive/2026-11-09-cpptlm-cep-dual-port-inbound-dispatch/`;tasks **30/30**)
主题: Coherence Exploration Platform(CEP)—— 双端口适配器入向按**端口组**路由 ⇒ 拓扑级回程腿闭合
依赖: ADR-CEP-06, ADR-CEP-19
来源: 台账 Entry #305 ③ R-2(残余登记)+ Entry #307(4 项 oracle 裁决 + 实施顺序)+ Entry #308(实施回执)+ Entry #309(T1 + E1)+ Entry #310(SU · 十六 冻结面授权)
生成时间: 2026-11-09
---

# cpptlm-cep-dual-port-inbound-dispatch

> **归档件**:`openspec/changes/archive/2026-11-09-cpptlm-cep-dual-port-inbound-dispatch/`(proposal + design + tasks **30/30**)
> **⛔ 零 delta spec** —— 本 change 是**框架行为修复 + 配置 + 判据**,**不改任何 main spec** ⇒ 归档**不**合并 delta(`--skip-specs`)。

## What Changes(归档摘要)

1. **框架修复**:`include/framework/dual_port_stream_adapter.hh` override **双参** `process_request_input(Packet*, unsigned)` ⇒ **按端口组**路由(组 0 = PE ⇒ `pe_req_in()` · 组 1 = Net ⇒ `net_resp_in()`)。⛔ 未新增成员(`src/core/module_factory.cc:733` 的跨实例 `static_cast` 要求布局逐字节相同)。
2. **配置**:拓扑 A/B 的 `emit_completions: false → true` + **补 `home.0 → w.1` 回程连接**(CohIf 的 egress 走 `req_out` ⇒ home 的 `req_out[0]` 原拓扑未连接 ⇒ 回程发不出去)⇒ **「双条件」实需第三条**。
3. **判据**:新 `test/test_dual_port_inbound_dispatch.cc`(5 cases · 走框架真实入口 + 反向 fixture)· SCE-09 断言翻转 · **SCE-22** 反向 fixture · emit 用例改为**实测**延迟族。
4. **同批**:ADR 结构闸(T1)· D-4 探测首次真实执行(E1)· 交接件勘误 11 项 · KPI 基线同步。

## 边界(⛔ 未达成项,不得读成达成)

- **IC3 仍 ⚪ 不适用** ⇒ 拓扑 A/B **仍不承载「全绿 run」**(只闭合**延迟族 5 键**)。
- **`B3b-2` 维持 0/4**;E1 的 8/8 槽位**全 miss** ⇒ **D-4 未对任何族触发**。
- 残余 **R-5 / R-6**(`BidirectionalPortAdapter` 2 参 override 签名不匹配 ⇒ 恒用端口 0)另立 change;**R-8**(`NUMBER-MAP` 状态列 ✅ 三义)待冻结面授权。

**关联**:台账 Entry **#311**(归档回执)· **#310**(SU · 十六 授权)· **#309**(T1 + E1)· **#308**(R-2 实施)· **#307**(4 项裁决)· **#305 ③ R-2**(残余登记)· `07 号` **SU · 十六** · `docs/adr/ADR-CEP-19` · `docs/adr/ADR-CEP-06`
