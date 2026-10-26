# ADR-CEP-19 草案(待签发):CohIf 载体与 ProtocolBridge 特化

> **状态**: 🟡 **草案 —— 未签发**
> **拟签发路径**: `docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md`
> **日期**: 2026-10-26
> **作者**: CppTLM architect
> **⚠️ 签发前置**: 本文件**故意**不落在 `docs/adr/` —— `cep_freeze_check.sh` 的 `FROZEN_GLOBS` 含 `docs/adr/ADR-CEP-*.md`,新增文件会触发 M1 `drift` ⇒ 守门红。签发须走:① `obj §11` 先有台账 Entry → ② 两步授权(commit 产物 → `--update --authorized-by "Entry #N"` → commit 清单)→ ③ `ADR-CEP-NUMBER-MAP` 清单 21 → 22。
> **依据**: [`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`](cep-cohif-carrier-architecture-review-2026-10-26.md)(逐 `file:line` 核实)

---

## 1. 背景

CEP 平台(`docs/architecture/07-coherence-exploration-platform.md`)的 6 层架构里,**Coherence Interface 层**(canonical 10 流,`ADR-CEP-02`)之下是 **Protocol Engine 插件层**(协议特化),之上是 **Stub Cache Core**。

**现状实测(2026-10-26)**:

```bash
grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/   # → 空
```

⇒ 队列 #1 ~ #5 已归档产出(`stub_cache_core` / `sharer_tracker` / `ordering_point_checker` / `channel_dependency_graph` / `collective_plane` / `collective_plane_contract`)**全部是纯 C++ 逻辑 + JSON 声明源,零 ChStream 承载体**。

**但契约要求有**:`ADR-CEP-07` 决策 3 规定 **ProtocolEngine 派生类 = `ChStreamModuleBase` + `ModuleFactory::registerObject<T>`**,示例列的就是 `ChiRnfAdapter` / `CxlCacheAdapter` / …;`ADR-CEP-07:67` 规定 **StubCacheCore = `ChStreamModuleBase`**。

⇒ **CohIf 的 10 流至今没有承载形态**;`ADR-CEP-07A` 决策 1 第 3 项指定的替代物「canonical 10 流的 Bundle」经核实**不存在**(`ADR-CEP-02` 内 `Bundle` 零命中;`07 号 §5.4` 业务 Bundle 列表无它;`include/bundles/` 17 文件 grep `coherence` 零命中)。

**同时**:四份**既有设计**齐全、却**无队列认领** ——
① `06-bis §5.2~§5.4` 的 coherence Bundle 字段定义 · ② `06 号 §4.4` 的 `ProtocolBridge` 双层类形态 · ③ `06 号 §9.3` + `module_factory.hh:313-315` 的 `register_protocol_bridge` 挂载钩子(**write-only 死钩子**) · ④ `06-bis §3.4` 的装配裁决(候选 C programmatic bridge)。

**且**:`06-bis` 顶部 `Status Update (2026-10-21) — superseded by CEP` 已把协议语义建模划归 ArchForge,并写明「**CEP 侧只提供 adapter 载体 + 能力声明 schema**」⇒ **「载体」是 CEP 的既定职责,但没有队列交付它**。

---

## 2. 决策

### 决策 1 —— 新立 **CohIf 载体队列**,排在 `moesi_gpu` 之后、`chi-rnf-subset` 之前

队列(暂名 `cpptlm-cep-cohif-carrier`)交付 **4 项**:

| # | 交付物 | 承接来源 |
|---|---|---|
| 1 | `include/bundles/cep_coherence_bundles_tlm.hh` —— `bundles::CohIfBundle` | `06-bis §5.2~§5.4` 字段基线 |
| 2 | `include/tlm/cep/protocol_engine_base.hh` —— `ChStreamModuleBase` 派生的 ProtocolEngine 基类 + ChStream 端口面 | `ADR-CEP-07` 决策 3 + `router_tlm.hh:139-142` 范式 |
| 3 | `include/framework/protocol_bridge.hh` —— `ProtocolBridgeEr` + `ProtocolBridge<InB, OutB, Ordered>` | `06 号 §4.4`(**首次落地**) |
| 4 | `register_protocol_bridge` 扩展为 `(bridge_name, factory_lambda)` | `06 号 §9.3`(死钩子 → 活钩子) |

**排序理由**:`moesi_gpu` 交付首个真实协议语义 + **特化字典**(6→3 消息类映射);载体队列**消费**该字典;`chi-rnf-subset` 则**首次在真实载体上验证**。若载体排在 `chi-rnf` 之后 ⇒ `chi-rnf` 会继承 `moesi_gpu` 的**逻辑级**装配验证(现为 mock ProtocolEngine 驱动,`stub_cache_core.hh:42`)⇒ **载体债复制一遍**。

### 决策 2 —— `CohIfBundle` 是**单一类型**,10 流以 `flow_id` 区分

**理由**:`06-bis §3.2` 实证「现有 Step 7a 装配路径不支撑 4 类 Bundle 4 端口」;但该死结的前提是「10 流 ⇒ 多类 Bundle」。**10 流共用一个 Bundle** ⇒ Bundle 类型数 = 1 ⇒ `BidirectionalPortAdapter<M, CohIfBundle, N>`(仓内已存在,`chstream_adapter_factory.hh:103`)直接够用 ⇒ **框架层零新增 StreamAdapter 子类**。

**范式已验证**:`router_tlm.hh:139-142` 用 `InputStreamAdapter<NoCFlitBundle>` + `OutputStreamAdapter<NoCFlitBundle>` + `BidirectionalPortAdapter<RouterTLM, NoCFlitBundle, NUM_PORTS>` ⇒ 载体照抄即可。

### 决策 3 —— `CohIfBundle` 归**业务语义**类,但按**网络级纪律**演进

**理由**:`06 号 §3.1` 的两条判定规则对 CohIf 给出相反答案 —— **字段维度**(无 `src_node`/`dst_node`/`flit_category`/`vc_id` ⇒ 业务)vs **owner 维度**(跨模块共享的项目级契约 ⇒ 网络)。

**裁决**:**分类 = 业务语义**(canonical 10 流是事务语义抽象);**演进纪律 = 网络级**(`BUNDLE_FIELD_WIDTH` 编译期锁 + 扩展走 OpenSpec change)。

⇒ 连带:`06 号 §3` 需**补一列「演进纪律」**,把「分类」与「纪律」解耦 —— 二者绑在一列是 `06-bis §5.6` 与本决策冲突的根因。

### 决策 4 —— 特化桥 = `ProtocolBridge` 双层,**不引入** `ProtocolBridgeFactory` 类

**理由**:`06 号` 内部两条互斥要求 —— §9.3「扩展既有 register 入口,**不重发明**」vs §11.2 的 **P3** 子项「**拒绝 ProtocolBridgeFactory 模式**」。**扩钩子签名、不新建 factory 类** ⇒ 同时满足两条,且符合 `01 号 §1.1`「当前 L3+L4 已达 Gem5 MessageBuffer 水准,**不需新增抽象层**」。

### 决策 5 —— 装配走 **programmatic**(承接 `06-bis §3.4` 候选 C)

**理由**:`06 号 §6.1`「容器端口契约 = setter 注入」;`06-bis §3.4` 的「永久豁免」条款(不与 `cpptlm-dgpu-soc-physical-ip-port-boundary` 的「全部 JSON 化」反向游泳)。

### 决策 6 —— Bundle 路径用 `include/bundles/cep_coherence_bundles_tlm.hh`

**理由**:`ADR-CEP-07A` 决策 1 第 3 项明确「**不创建** `include/bundles/coherence_bundles_tlm.hh`」⇒ 该路径不可用(ADR 不可变,只能绕)。命名风格与队列 #5 已登记的 collective 未来路径(`include/bundles/collective_bundles_tlm.hh`,`ADR-CEP-03` Status Update 2026-10-26)同构。

### 决策 7 —— `06 号 §11.2/§11.3` 的时间线转隶 CEP

**理由**:承载 seam 的代码增量在 `06 号` 口径里全部推迟到 `06 号 §11.2` 的 P3+;但 06-bis 已被 CEP supersede ⇒ `06 号` 的实施分期未执行且不再执行 ⇒ 由 CEP 排期承接(载体 = 新队列,时点 Stage 1 内、队列 #6 之后)。

### 决策 8 —— 本 ADR **不**引入新抽象层

**不引入**:StreamAdapter 新子类 · factory 抽象层 · 协议语义基类(不做 `CoherenceProtocol` 式单一 C++ 类,per `06-bis` 顶部处置表)· per-packet `tlm_extension`(per `06 号 §4.5` = P3+)。**只做**:1 个业务 Bundle 类型 + 1 个 `ChStreamModuleBase` 基类 + 1 个 seam 桥 + 1 处死钩子激活。

---

## 3. 后果

### 3.1 正面

- **`01 号 §1.1` 的 L5↔L4 seam 首次有实现** —— `ProtocolBridge<CohIfBundle, NoCFlitBundle>` = CohIf 业务语义 → 具体网络 Bundle 的转换器;换 `Axi4Bundle` 即换一种通信网络 ⇒ **「ChStream 构建的互联网络可特化到具体通信网络」闭合**;
- **`01 号 §4.2` 的「模块代码完全不知道 Port 的存在」在 CEP 落地** —— ProtocolEngine 内部只见 `Input/OutputStreamAdapter<CohIfBundle>`;
- **队列 #6 的 6→3 映射表获得消费者** —— 它是「CohIf 语义 → 网络 Bundle `flit_category`」的特化字典(目标常量**就定义在** `include/bundles/noc_bundles_tlm.hh:60-62`);
- **消除「声明有、实现无」的悬空** —— `ADR-CEP-07A` 第 3 项的替代物从「悬空引用」变为「实体路径」。

### 3.2 负面 / 代价

- **框架层 2 处改动**:`module_factory.hh:313-315` 钩子签名扩展 + Step 7 的 bridge 消费 ⇒ 触 `module_factory.cc` 核心路径 ⇒ 需 6 标签回归;
- **per-bridge 元数据只做静态**(`protocol_id_` / `bridge_id_`,per `06 号 §4.5`)⇒ per-packet 协议 flavor 仍推迟;
- **全局算术变更**:13 → **14** change / 14 季度 ⇒ 须修订冻结面 `ADR-CEP-00 §2` 决策 3 + `ADR-CEP-NUMBER-MAP` KPI 预算块 + 五处同改。

### 3.3 风险

| 风险 | 缓解 |
|---|---|
| `CohIfBundle` 单类型 ⇒ 10 流字段**并集**可能过宽(单次事务携带无关字段) | `flow_id` 决定字段语义;`flow_id` 之外字段不参与判定;若实测出现明显的带宽浪费,再拆为「请求类 / 响应类 / 数据类」三型(**不推翻单类型原则**,仅缩字段集) |
| `BUNDLE_FIELD_WIDTH` 编译期锁 ⇒ 字段集一旦定,扩展成本 = 新 Bundle 类型 | 与 `06 号 §3.4` 的「类型级切换(旧 Bundle 保留,新类型并存)」一致 |
| `register_protocol_bridge` 从死钩子变活钩子 ⇒ `get_bridges()` 的消费方此前不存在 | 队列内**必须**同时交付消费方(判据 CC-05)⇒ 只扩不消费 = 新的死钩子 |
| 载体队列把 `StubCacheCore` 拉进 ChStream 体系 ⇒ 影响队列 #3 已归档产物的形态 | `stub_cache_core.hh` 的**既有接口零改动**;ChStream 端口面作为**新增**旁挂面,原有 net_side 方法保留 |

---

## 4. 关联

- 审查依据(逐 `file:line` 核实):`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`
- 队列 #6:`openspec/changes/cpptlm-cep-moesi-gpu-adapter/`
- 架构基线:`docs/architecture/01-hybrid-architecture-v2.1.md` · `06-inter-module-network.md` · `06-bis-coherence-as-network-extension.md` · `07-coherence-exploration-platform.md`
- 契约:`ADR-CEP-{00,02,03,05,07,07A,12,13,15,16}-*.md` · `ADR-CEP-NUMBER-MAP.md`
- 偏离登记格式先例:`ADR-CEP-03` Status Update 2026-10-26
