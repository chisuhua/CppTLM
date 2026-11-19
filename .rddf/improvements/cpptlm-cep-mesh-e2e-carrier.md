---
name: cpptlm-cep-mesh-e2e-carrier
priority: P0
phase: phase-10
category: core-impl
type: feature
状态: "🟡 **已立项**(2026-11-16 · builder 起草并实施 · 台账 Entry #448 · KPI **0/3 → 1/3**)"
created: 2026-11-16
---

# improvement: cpptlm-cep-mesh-e2e-carrier

**主题**: 让 CEP 的 coherence 流量**穿过真实互联网络**(RouterTLM XY mesh)抵达对端 —— 补上「协议在真实业务流量 + 真实 mesh 拓扑下会怎样」这一整块缺失的验证面。

> **上游**: 台账 **Entry #444** §2.2 类 B(真·功能缺口)· §3 波 2 · improvement `cpptlm-cep-stub-cache-chstream-surface` 的 **Out of Scope 明列项**「⛔ flit 级网络路径(仓内不存在)」
> **⚠️ 前置事实纠正**:G1(StubCacheCore ChStream 端口面)**已于 2026-11-09 归档完成**(Entry #305 · `StubCacheEndpointTLM` + `CohIfResponderTLM`)⇒ 本 change 的缺口**收窄**为「CohIf ↔ 网络适配层 + mesh 联合拓扑」。

---

## Why

### 第 1 条 —— CEP 至今**没有任何** coherence 流量穿过真实互联网络。

实测现状(2026-11-16):CEP 现有 e2e 判据的拓扑全为**点对点**(`cep_cohif_pe_minimal.json` 两引擎直连 / `cep_chi_rnf_e2e_a|_b.json` 点对点)· `fabric.json` 只有 2 个网络命名空间 + `vc_count`(**无节点 / 无链路 / 无路由**)· 桥**不填** `dst_node` 而 `RouterTLM` 的路由**只**从 `flit.bundle.dst_node` 取目标 ⇒ 「stub ↔ home 事务穿过真实 mesh」此前**结构性不可达**。

### 第 2 条 —— 缺的不是测试,是**一段产品代码**:CohIf ↔ 网络的适配层。

`NICTLM` 的 PE 侧是 `CacheReqBundle`(与 `CohIfBundle` 类型不通)· `CohIfNoCProtocolBridge` 是挂在 PE 上的类型转换器(无端口 ⇒ 无法插在 PE 与 Router 之间;且**不填** `dst_node`)⇒ 两者都不能承担此职责 ⇒ 新增 `CohIfNICTLM`(CEP 版 NIU)。

### 第 3 条 —— 存在**零架构裁决 + 零冻结面改动**的路径。

`CohIfBundle` 无 `dst_node`/`vc_id` ⇒ 适配层自己填 · `CohIfBundle.data` 是 1 拍载荷 ⇒ 单 flit 足够 · 垂直切片(单流 Request→Completion)下 5→3 的 `flit_category` 投影**无歧义** ⇒ **只补适配层 + 联合拓扑 + 判据**即可。

---

## What Changes

1. **新增** `include/tlm/cep/cohif_nic.hh`(**header-only** · 照仓内 CEP 惯例):`CohIfNICTLM` —— 4 端口异构 dual-port(`CohIfBundle` ↔ `NoCFlitBundle`)· 承担 `dst_node` 填充 · **地址未命中 ⇒ 显式丢弃**(⛔ 不复制 `NICTLM` 的静默投 node 0)· 观测面含 `last_recv_dst_node()` / `last_recv_hops()`(入站侧)。
2. **注册**:`registerObject` + `registerDualPortAdapter`(⛔ 不用对称宏)。
3. **新增** `configs/cep_mesh_e2e.json`:2×2 RouterTLM mesh + 2×`CohIfNICTLM` + `StubCacheEndpointTLM` + `CohIfResponderTLM` + `TrafficGenTLM`(**9 实例**)。
4. **新增** `test/test_cep_mesh_e2e.cc`(J-1~J-5)+ `test/test_noc_mesh_binding_semantics.cc`(探针**转正**为绑定语义的回归载体)。
5. **新增契约面** `openspec/specs/cep-cohif-network-adapter/spec.md`(⛔ **不 MODIFIES** 任何既有 `cep-*` spec ⇒ 零冻结面产物 ⇒ 免五步链)。

---

## Acceptance

| # | 判据 | 实测 |
|:--:|---|---|
| **AC-01** | 模块可构造 + 可注册 | ✅ |
| **AC-02** | 装配成功(9 实例) | ✅ |
| **AC-03** | 正向抵达**对端**(`last_recv_dst_node()==3`) | ✅ |
| **AC-04** | Completion 回程抵达 stub | ✅ |
| **AC-05** | **确实穿过 mesh**(`r.flits=45` ∧ `last_recv_hops()==3`) | ✅ |
| **AC-06** | 决定性变异按**正确原因**红 | ✅(`NotArrived`) |
| **AC-07** | 既有面零 diff + 闸 0 回归 | ✅ 13 文件零 diff · `[cep]` 533 用例 · `[e2e]` 144 用例 · `ctest -R cep` 100/100 |
| **AC-08** | 契约面(新增 spec · 零 MODIFIES) | ✅ |

---

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|---|
| **C-1** | coherence 流量可穿过真实互联网络 | AC-03/04/05 | ❌ 0 |
| **C-2** | 判据有牙(变异按正确原因红) | AC-06 | ❌ 0 |
| **C-3** | 既有面零侵入 | AC-07/08 | ❌ 0 |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增** | `include/tlm/cep/cohif_nic.hh` · `configs/cep_mesh_e2e.json` · `test/test_cep_mesh_e2e.cc` · `test/test_noc_mesh_binding_semantics.cc` · `openspec/specs/cep-cohif-network-adapter/spec.md` |
| **改动** | `include/chstream_register.hh` · `scripts/CMakeLists.txt`(CEP config 清单 +1)· 5 处 KPI 活口径 · 台账 |
| **⛔ 不动** | `include/bundles/**` · `include/tlm/router_tlm.hh` · `include/tlm/nic_tlm.hh` · 既有 8 个 CEP 头文件 · 既有 43 份 `cep-*` main spec · `protocol_instances.json` / `fabric.json` · `src/core/module_factory.cc` |
| **计数** | `B3b-2` 维持 **0/4** · `INV-CEP-*` 新增 **0** 条 · KPI **0/3 → 1/3** |

---

## ⚠️ 残余（登记 · 路由 architect）

| # | 残余 | 实测 | 路由 |
|:--:|---|---|---|
| **Q1** | **多流无损** —— `flit_category` 仅 3 值装不下 5 个 `flow_id` ⇒ `Completion` 与 `Snoop` 对端不可区分 | `include/bundles/noc_bundles_tlm.hh:45` | **architect**(3 条出路:① `NoCFlitBundle` 加字段(ADR-CEP-19 决策 3 允许「扩展走 OpenSpec change」)② 新增 CEP 专用网络 Bundle(⇒ RouterTLM 须支持多 Bundle 类型)③ 接受有损并把「多流混跑」排除在 CEP 可实验性之外) |
| **Q2** | **CEP 的 mesh 拓扑声明面** —— 本 change 用拓扑 config 表达 mesh,`fabric.json` 仍只有 2 个网络命名空间 | `include/tlm/cep/fabric.json` | **architect** |
| **Q3** | `NICTLM::lookup_node` 未命中**静默投 node 0** —— 本模块已显式失败,但 `NICTLM` 自身不在本 change | `include/tlm/nic_tlm.hh:44-51` | 各 owner(dGPU 侧) |
| **Q4** | **协议引擎不参与本切片的业务事务路径** —— `ADR-CEP-19` SU 2026-11-09 已登记「RN-F 不服务 request ⇒ 业务事务直连 home」⇒ 本 change 验证的是「**stub ↔ home 事务**穿过真实 mesh」,**不是**「协议语义在 mesh 环内被验证」 | `docs/adr/ADR-CEP-19*.md` SU 2026-11-09 | 边界声明已落 delta spec · **不得**对外表述为「CEP 协议已在真实 mesh 上验证」 |
| **Q5** | `CohIfNICTLM` 的 `is_write` 由 `perm` **近似**(`CohIfBundle` 无 `is_write` 字段)· `CohIfBundle` 无 `vc_id` ⇒ **单 VC** | `include/bundles/cep_coherence_bundles_tlm.hh:94-114` | 已知边界 |
