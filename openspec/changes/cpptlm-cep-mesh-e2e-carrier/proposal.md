# change: cpptlm-cep-mesh-e2e-carrier

**主题**: 让 CEP 的 coherence 流量**穿过真实互联网络**(RouterTLM XY mesh)抵达对端 —— 补上「协议在真实 mesh 下会怎样」这一整块缺失的验证面。

> **上游**: 台账 **Entry #444** §2.2 类 B(真·功能缺口)· §3 波 2 · improvement `cpptlm-cep-stub-cache-chstream-surface` 的 **Out of Scope 明列项**「⛔ flit 级网络路径(仓内不存在)」
> **本 change 的边界**: **只做垂直切片**(单流 Request→Completion 往返)· **零架构裁决** · **零冻结面改动**

---

## Why

### 第 1 条 —— CEP 至今**没有任何** coherence 流量穿过真实互联网络。

实测现状(2026-11-16):

| 面 | 事实 | 位置 |
|---|---|---|
| CEP e2e 判据 | 拓扑 = `pe0 ↔ pe1` **两个引擎点对点直连** · 无 Router · 无 NIC · 引擎是 `NullProtocolEngine` | `configs/cep_cohif_pe_minimal.json` · `test/cep_cohif_assembly_helper.hh:56-99` |
| 已归档的 stub-cache e2e | 拓扑 A/B = `tg → stub → home` 与 `stub ↔ home` + `chi → home` · **点对点** · 无 router | `configs/cep_chi_rnf_e2e_a.json` / `_b.json` |
| CEP 的「互联网络」声明面 | `fabric.json` 只有 **2 个网络命名空间** + `vc_count` · **无节点 / 无链路 / 无路由** | `include/tlm/cep/fabric.json` |
| CEP 实验产出 | `trace.h5` 是 **run 级摘要** · 脚本自注「本模型不产逐周期信号」 | `scripts/test/cep_experiment_run.sh:emit_trace()` |

⇒ 后果: `07 号 SU · 七 ④` 诚实登记的后果至今成立 —— 现有**全部**事务级判据都是「引擎↔引擎」同构对等 ⇒ **不能**回答「该协议在**真实业务流量 + 真实 mesh 拓扑**下会怎样」。而这正是 CEP 平台存在的理由(跨协议取舍问题的可实验性)。

### 第 2 条 —— 缺口不在「写测试」,在**缺一段产品代码**:CohIf ↔ 网络的适配层不存在。

仓内已有的三块积木**全部到位**,但**接不上**:

| 积木 | 状态 | 位置 |
|---|---|---|
| stub cache 端点(带 CohIf 端口面) | ✅ 已归档(2026-11-09,Entry #305) | `include/tlm/cep/stub_cache_endpoint.hh`(`StubCacheEndpointTLM` · 4 端口 · CohIf 侧) |
| home 简化模型 | ✅ 同上 | `include/tlm/cep/cohif_responder.hh`(`CohIfResponderTLM`) |
| CohIf↔NoCFlit 特化桥 | ✅ 已落地(层 1 投影 + 层 2 字节等价) | `include/tlm/cep/cohif_noc_bridge.hh` |
| **CohIf → 网络的适配模块(填 `dst_node` + 发 flit)** | ❌ **不存在** | — |

⛔ **桥不能替代它**:`CohIfNoCProtocolBridge::pack` 明确「**不清零 `out` 的其余字段 —— 桥无权改写路由器负责的字段**」⇒ 桥**不填** `dst_node`;而 `RouterTLM` 的路由**只**从 `flit.bundle.dst_node.read()` 取目标(`src/tlm/router_tlm.cc:179`)⇒ **必须有模块填 `dst_node`**。仓内填 `dst_node` 的是 `NICTLM::packetize`(`lookup_node(address)`,`src/tlm/nic_tlm.cc:98,108`)—— 但 `NICTLM` 的 PE 侧端口是 `CacheReqBundle`,**不是** `CohIfBundle` ⇒ 类型不通。

### 第 3 条 —— 垂直切片可做到 **零架构裁决 + 零冻结面改动**。

三条硬约束(实测,非推断):

1. `CohIfBundle` 的 12 个字段**无** `dst_node` / `src_node` / `vc_id`(ADR-CEP-19 决策 3 **刻意**设计:分类=业务语义)⇒ 适配层必须自己填。
2. `CohIfBundle.data` 是 **1 拍**载荷(ADR-CEP-02)⇒ **单 flit 足够**(`flit_count = 1`)⇒ 不需要分片。
3. `NoCFlitBundle` **无 payload 字段**(只有 8 字节 `data`)⇒ 桥的层 2「完整语义不透损」在 NoC 上**不可行**;层 1 投影可用,但 `flit_category` 只有 3 值(REQUEST/RESPONSE/CREDIT)装不下 CohIf 的 5 个 `flow_id` ⇒ **单流场景下无歧义**(场景内只有 Request 与 Completion 两种)⇒ **垂直切片可成立**。

⇒ 本 change 只补这段适配层 + 联合拓扑 + 判据 ⇒ **不改任何既有 Bundle / 既有 CEP 模块 / 既有冻结面 spec**。

---

## What Changes

1. **新增** `include/tlm/cep/cohif_nic.hh` + `src/tlm/cep/cohif_nic.cc`:`CohIfNICTLM` —— CEP 版网络接口(CohIf ↔ NoCFlit)
   - 4 端口(异构 dual-port):`pe_req_in(): CohIfBundle` · `pe_resp_out(): CohIfBundle` · `net_req_out(): NoCFlitBundle` · `net_resp_in(): NoCFlitBundle`
   - 正向:CohIf → NoCFlit(`transaction_id` / `address` / `data` / `size` 映射 + **`dst_node` 由地址映射填** + `src_node` + `vc_id` + `flit_type=HEAD_TAIL` + `flit_count=1` + `hops=0` + `flit_category` 三值映射)
   - 反向:NoCFlit → CohIf(反向映射)
   - 地址映射:`address_regions` 配置(`base` / `size` / `target_node`)· **未命中 ⇒ 显式丢弃 + 计数**（⛔ **不**学 `NICTLM::lookup_node` 的静默投 node 0,`include/tlm/nic_tlm.hh:44-51`）
2. **注册**:`ModuleFactory::registerObject` + `ChStreamAdapterFactory::registerDualPortAdapter<CohIfNICTLM, CohIfBundle, CohIfBundle, NoCFlitBundle, NoCFlitBundle>`(⛔ 不用对称 `REGISTER_CHSTREAM_BIDIRECTIONAL`,它要求两侧同 BundleT)
3. **新增** `configs/cep_mesh_e2e.json`:2×2 `RouterTLM` XY mesh + 2×`CohIfNICTLM` + `StubCacheEndpointTLM` + `ChiRnfProtocolEngine` + `CohIfResponderTLM` + `TrafficGenTLM`
4. **新增** `test/test_cep_mesh_e2e.cc`(标签 `[cep][e2e][mesh]`):J-1 ~ J-6 判据
5. **新增契约面** `openspec/specs/cep-cohif-network-adapter/spec.md`(delta · `## ADDED Requirements`)
6. **1 份 improvement** + change 四件套 + 台账 Entry

---

## Acceptance

| # | 判据 | 可验收点(机器可判) |
|:--:|---|---|
| **AC-01** | 模块可构造 + 可注册 | `CohIfNICTLM` 两模块可构造;`ModuleFactory` 注册表含 `CohIfNICTLM`;`registerDualPortAdapter` 的 4 个 BundleT 逐类型断言 |
| **AC-02** | **装配成功** | `ModuleFactory::instantiateAll(configs/cep_mesh_e2e.json)` rc=0;**9 个实例**(tg / w / home / nic_a / nic_b / r00 / r01 / r10 / r11)全部在位;⛔ **不含协议引擎**(per ADR-CEP-19 SU 2026-11-09:RN-F 不服务 request ⇒ 业务事务直连 home) |
| **AC-03** | **正向抵达** | Request 经 mesh 抵达**对端** NIC 的**入向**(`net_resp_in` 侧)· **且** `nic_b.last_recv_dst_node() == 3`(观测**收到的** flit 的 `dst_node`;⛔ 不是本模块出站查表结果)· ⛔ 只断言「计数>0」会被「回环到自己」骗过 |
| **AC-04** | **反向抵达** | Completion 经 mesh 回到 stub 侧(`StubCacheEndpointTLM` 的 per-txn 观测:complete 周期戳存在 + in-flight 归零) |
| **AC-05** | **确实穿过了 mesh(而非直达)** | 参与路径的 `RouterTLM` 的 `flits_forwarded` > 0 **且** `packets_forwarded` > 0 **且** `nic_b.last_recv_hops() >= 1`(router 转发时 `hops += 1` ⇒ 直连 ⇒ `hops == 0`)⇒ ⛔ 不得只靠「抵达」断言(直达也能抵达) |
| **AC-06** | **决定性变异(按正确原因红)** | 断开 mesh 一条 router-router 连接 ⇒ 判据 SHALL 红且失败原因 SHALL 是「未抵达」(⛔ **不是**「装配失败」—— 装配失败会让变异「因错误的原因红」) |
| **AC-07** | **既有面零 diff + 闸 0 回归** | `include/tlm/cep/stub_cache_core.hh` / `stub_cache_endpoint.hh` / `cohif_responder.hh` / `cohif_noc_bridge.hh` / `protocol_engine_base.hh` / `include/bundles/**` / `include/tlm/router_tlm.hh` / `include/tlm/nic_tlm.hh` **全部零 diff**;`ctest -R cep` 与 `[cep]` / `[e2e]` 回归全绿;既有 **45** 闸数量不变 |
| **AC-08** | **契约面** | 新增 `cep-cohif-network-adapter` main spec;⛔ **不 MODIFIES** 任何既有 `cep-*` spec(⇒ 零冻结面产物 ⇒ 免五步链) |

---

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|---|
| **C-1** | **coherence 流量可穿过真实互联网络** | AC-03 / AC-04 / AC-05 | ❌ 0 |
| **C-2** | **判据有牙**(变异必红 + 按正确原因红) | AC-06 | ❌ 0 |
| **C-3** | **既有面零侵入**(不改 Bundle / 不改既有模块 / 不改既有冻结面) | AC-07 / AC-08 | ❌ 0 |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `include/tlm/cep/cohif_nic.hh` · `src/tlm/cep/cohif_nic.cc` · `configs/cep_mesh_e2e.json` · `test/test_cep_mesh_e2e.cc` · `openspec/specs/cep-cohif-network-adapter/spec.md` |
| **改动文件** | `include/chstream_register.hh`(+2 行注册)· `CMakeLists.txt`(+1 源文件)· change 四件套 · improvement · 台账 · **`src/core/module_factory.cc`(⚠️ 条件项 —— 仅当 T1.3 探针证实 `is_dual` 分支的硬编码 `DualPortStreamAdapter` cast 对本模块不可用时,才做最小化修改;默认零 diff)** |
| **⛔ 不动** | `include/bundles/**`(含 `NoCFlitBundle` / `CohIfBundle`)· `include/tlm/router_tlm.hh` · `include/tlm/nic_tlm.hh` · `include/tlm/cep/stub_cache_core.hh` / `stub_cache_endpoint.hh` / `cohif_responder.hh` / `cohif_noc_bridge.hh` / `protocol_engine_base.hh` · 既有 43 份 `cep-*` main spec · `protocol_instances.json` / `protocol_instance.schema.json` / `fabric.json` |
| **契约影响** | 新增**第五类 CEP 模块**(adapter / engine / bridge / home 模型 / **network adapter**)· 两侧边界句随 delta 落盘 |
| **计数影响** | `B3b-2` 维持 **0/4** · `INV-CEP-*` 新增 **0** 条 · KPI **0/3 → 1/3**(建 change)→ **0/3**(归档) |

---

## ⚠️ 本 change 明确**不**解决的(登记 · 路由 architect)

| # | 残余 | 实测 | 路由 |
|:--:|---|---|---|
| **Q1** | **多流无损** —— `flit_category` 只有 3 值(REQUEST/RESPONSE/CREDIT)装不下 CohIf 的 5 个 `flow_id` ⇒ 层 1 投影**有损** ⇒ `Completion` 与 `Snoop` 在对端**不可区分** | `include/bundles/noc_bundles_tlm.hh:45` · `include/tlm/cep/cohif_noc_bridge.hh` 的 5→3 映射 | **architect**(3 条出路:① `NoCFlitBundle` 加字段(ADR-CEP-19 决策 3 明确「扩展走 OpenSpec change」⇒ 有合法路径,但触 dGPU 侧 `BUNDLE_FIELD_WIDTH` 纪律)② 新增 CEP 专用网络 Bundle(⇒ RouterTLM 须支持多 Bundle 类型 ⇒ 侵入 Router)③ 接受有损并把「多流混跑」明确排除在 CEP 可实验性之外) |
| **Q2** | **CEP 的 mesh 拓扑声明面** —— 本 change 用**拓扑 config** 表达 mesh,`fabric.json` 仍只有 2 个网络命名空间 ⇒ **声明面与实现面分离** | `include/tlm/cep/fabric.json` | **architect**(是否要 `fabric.json` 承接节点/链路/路由 ⇒ 改派生副本 ⇒ 触 ArchForge 镜像纪律) |
| **Q3** | **`NICTLM::lookup_node` 未命中静默投 node 0** —— 本 change 的 `CohIfNICTLM` **显式失败**(改进),但 `NICTLM` 自身的静默失败**不在本 change** | `include/tlm/nic_tlm.hh:44-51` | 各 owner(dGPU 侧) |

> **本 change 的正当性不依赖 Q1/Q2 的裁决** —— 垂直切片(单流 Request→Completion)在有损投影下**语义无歧义** ⇒ Q1 的裁决结果只影响「能否扩到多流混跑」,不影响本 change 的 AC。
