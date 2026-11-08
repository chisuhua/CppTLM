# cpptlm-cep-stub-cache-chstream-surface: 真实业务流量可进入 CEP 互联面(G1~G4′ 收编)

> **状态**: 🟡 **Proposed**(2026-11-08 立项 —— 用户口令「按你建议进入下一步」;KPI 并发 **0/3 → 1/3**)
> **立项依据**:`ADR-CEP-19` **决策 9**(显式延期 → 实施态,具名绑定本 change)· 台账 **Entry #288**(延期项转实施态)/ **#291~#296**(收编 scope + 甲/乙 + F-12 + retention + 路径图)· `.rddf/improvements/cpptlm-cep-stub-cache-chstream-surface.md`
> **估时**: ~8.5d(A1~A9) + 收尾 ~0.7d

## Why

`07 号` **SU · 七 ④** 诚实登记:CEP 现有**全部**事务级判据都是「**引擎 ↔ 引擎**同构对等」⇒ ⛔ **不能回答**「该协议在**真实业务流量**下会怎样」。而 `obj §2.1` 的愿景正是把跨协议取舍从**人脑推演**变成**可跑实验出数据** ⇒ ⇒ 该缺口必须补到**可测**(⛔ 不承诺条款级协议语义)。

`ADR-CEP-19` 决策 9 已把「`StubCacheCore` 的 ChStream 端口面」判为**显式延期**并**具名绑定**本 change;Entry #288 已转为**实施态** ⇒ ⇒ 本 change = **履行已签发承诺**。

## What Changes

| # | 变更 | 落点 |
|---|---|---|
| 1 | `StubCacheEndpointTLM`(组合式 wrapper:异构双端口 + 观测 API + 映射 + `attach_protocol_bridge` override + 只读桥指针) | `include/tlm/cep/stub_cache_endpoint.hh`(新,header-only) |
| 2 | `CohIfResponderTLM`(**真实 home 的简化模型**:最小应答语义) | `include/tlm/cep/cohif_responder.hh`(新,header-only) |
| 3 | 注册(`registerObject` × 2 + `registerDualPortAdapter` × 1 + `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED` × 1) | `include/chstream_register.hh` |
| 4 | `TrafficGenTLM` 的 **seed / 地址段 / mode setter** | `include/tlm/traffic_gen_tlm.hh` |
| 5 | 拓扑 **A** / **B**(两份独立 config) | `configs/cep_chi_rnf_e2e_a.json` / `_b.json`(新) |
| 6 | 判据 **J-1 ~ J-9**(Catch2,含 J-5 决定性变异) | `test/test_cep_stub_cache_e2e.cc`(新) |
| 7 | 台账元数据 **7 字段**(两处并存)+ **双向闸** + 首批 h3 run 入库 | `test/cep_e2e_runs.jsonl`(新)· `obj §11` · `.gitignore` |
| 8 | 契约面:**:56 单条 MODIFIED** + ADDED capability | `openspec/specs/cep-stub-cache-core/spec.md`(两步授权)· 本 change 的 delta |

## 验收标准(Acceptance Criteria)

- [ ] **AC-1(授权)** 另立台账 Entry 授权 `:56` 单条 MODIFIES,且冻结面两步授权链绿(provenance 复验通过)
- [ ] **AC-2(端口面)** 两模块可构造;`StubCacheEndpointTLM` 两侧 Bundle **确为异构**;`stub_cache_core.hh` **零 diff**,`INV-SC1`~`INV-SC6` 与 14 个既有单测全绿
- [ ] **AC-3(注册)** `ModuleFactory::instantiateAll` 对 A / B 两份 config 均成功
- [ ] **AC-4(复现)** setter 存在 + 快照含种子与 **git rev**;同种子两次 run 的计数**逐字段相等**
- [ ] **AC-5(分档 J-9)** A ⇒ 闸 `SKIP` ∧ 无 `cep.bridges` ∧ 无 `latency>0`;B ⇒ 闸 `PASS` ∧ 1 entry / 3 modules / ≥1 条 `latency>0` ∧ 三模块桥指针同 `bridge_id`
- [ ] **AC-6(判据)** J-1 ~ J-9 逐条绿;**J-5 变异按正确原因红**(⛔ 非「因错误原因红」)
- [ ] **AC-7(可达)** 13 键**各有结论**(实测值 **或** `not-collected`+reason)+ IC3 **✓ 通过** + 双违例类真实 0;三键测量归属**显式落字**
- [ ] **AC-8(对账)** 台账条目 7 字段齐备 + verdict 三态 + **双向闸绿**(零 orphan)+ 首批 h3 run 仅含 2 件
- [ ] **AC-9(契约)** `:56` MODIFIED 合并;delta 含豁免登记 4 项 / 「登记 ≠ 可复算」边界句 / 7 字段 / 「⛔ 不作为 T1–T4 条款级证据」声明句
- [ ] **AC-10(计数)** `B3b-2` 仍 **0/4**;`test/cep_b3b2_deferrals.json` 零 diff;`protocol_instances.json` 与 `protocol_instance.schema.json` 零 diff

## Out of Scope

- ⛔ 家族级 / 条款级协议语义(credit · retry · ordering point · 多拍聚合 · Perm 规则 · 容量与队列模型)
- ⛔ `trace.h5` / `report.html` 入库(retention = 丙)· ⛔ 历史 127 run 回填(grandfather)
- ⛔ 改 `B3b-2` 计数 · ⛔ flit 级网络路径(仓内不存在)· ⛔ `TRACE` 流量模式
- ⛔ 把 `CohIfResponderTLM` 写进 `protocol_instances.json`(已裁**甲**)

## 关联

`ADR-CEP-19` 决策 9 + SU 2026-11-08 · 二 / 三 · `ADR-CEP-05` 决策 2 · `ADR-CEP-07` 决策 3 · `ADR-CEP-12` SU 2026-11-08 · `ADR-CEP-06` SU 2026-11-08 · 二 · `07 号` SU · 七 / 十一 ~ 十四 · 台账 Entry #288 / #291 ~ #296
