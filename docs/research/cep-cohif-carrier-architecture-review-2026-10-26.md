# CEP CohIf 载体队列 —— 架构重审与设计

> **状态**: 🟡 **architect 产出**(调研 + 决策草案)· **未签发**
> **日期**: 2026-10-26
> **角色**: architect(per 用户裁决「切 architect 重新审视前面完成的内容」)
> **独立性说明**: 本仓为**单开发者**环境;本评审由 AI agent 完成,**不等于真人外部评审**(per `obj-cep-platform §9.3` R10 先例 ⇒ 结论应登记为「残余风险 + DRI 书面接受」,而非「已闭环」)。
> **本文档定位**: **附属 1(调研层)** —— 不进架构评审;引用真源, 不重发明。真源变更走 §9 的清单。

---

## 0. 摘要

**三条结论**:

1. **队列 #6 的 planner 产出与 `01 号` 核心立场一致** —— 6→3 消息类映射表**就是** L5↔L4 seam 的数据形态,且其目标 `CATEGORY_*` 3 类**就定义在网络 Bundle 自己的头文件里**(`include/bundles/noc_bundles_tlm.hh:60-62`)=> 特化字典已锚定到真实网络 Bundle,**不是孤岛**。

2. **但 CohIf 至今没有承载形态** —— 三处断点(§4)均有 `file:line` 证据:
   - **C1** CEP 全 11 个头文件**零** `Bundle`/`ChStream`/`StreamAdapter` 引用;`StubCacheCore` 实为普通 class(`stub_cache_core.hh:154`),**非** `ChStreamModuleBase` —— 而 `ADR-CEP-07:64/67` 与 `07 号 §6.3` 明写它应该是。
   - **C2** `ADR-CEP-07A:49` 决策「CEP 用 **canonical 10 流的 Bundle**」—— 该替代物**不存在**(`ADR-CEP-02` grep `Bundle` 零命中;`07 号 §5.4` 业务 Bundle 列表无它;`include/bundles/` 17 文件 grep `coherence` 零命中)=> **悬空引用**。
   - **C3** 唯一的「Bundle 化」承诺绑在**集合通信**队列(`ADR-CEP-03:225` 推迟至 `cpptlm-cep-nvlink-sharp-adapter`),**主干 10 流无人认领**。

3. **根因不是缺设计,而是「既有设计从未被一个队列认领」** —— 四份设计齐全、0 个队列:
   | 既有设计 | 位置 | 归属现状 |
   |---|---|---|
   | coherence Bundle 字段定义(`SnoopProbe`/`SnoopRespBundle`/`CoherenceMsg`) | `06-bis §5.2~§5.4` | 被 `ADR-CEP-07A` 「supersede」但**无替代物** |
   | `ProtocolBridge` 双层类形态 | `06 号 §4.4` | 「§15 演进 seam 才实施」⇒ 永久推迟 |
   | `register_protocol_bridge` 挂载钩子 | `06 号 §9.3` + `module_factory.hh:313-315` | **write-only 死钩子**,无人读 |
   | 装配路径裁决(候选 C programmatic) | `06-bis §3.4` | 随 06-bis 被 supersede |

**本轮最重要的新发现(消解 06-bis §3.2 认定的死结)**: canonical 10 流**可以共用一个 `CohIfBundle`**(以 `flow_id` 区分 10 流)⇒ **「多 Bundle 类型跨端口」这个装配死结不存在** ⇒ 框架层**零新增 StreamAdapter 子类**(照抄 `RouterTLM` 的 `BidirectionalPortAdapter<M, BundleT, N>` 范式即可,`router_tlm.hh:139-142` 已实证)。

---

## 1. 审查范围与证据基准

| 项 | 内容 |
|---|---|
| **被审对象** | 队列 #6 `cpptlm-cep-moesi-gpu-adapter` 的 planner 四件套(proposal / design / 2 份 delta spec)+ 队列 #1~#5 已归档产出 |
| **架构基线** | `docs/architecture/01-hybrid-architecture-v2.1.md` v2.1.10(`01 号`)· `docs/architecture/06-inter-module-network.md` v0.4(`06 号`)· `docs/architecture/06-bis-coherence-as-network-extension.md` v0.3(`06-bis`)· `docs/architecture/07-coherence-exploration-platform.md` v0.1(`07 号`) |
| **契约基线** | `docs/adr/ADR-CEP-{00,00A,02,05,07,07A,12,13,16}-*.md` + `ADR-CEP-NUMBER-MAP.md` |
| **代码基线** | `include/tlm/cep/*`(11 头文件)· `include/bundles/*`(17 文件)· `include/framework/*` · `src/core/module_factory.cc` |
| **方法** | 逐 `file:line` 核实,**不接受文档转述的二手结论**(B-1 教训:假测量) |
| **本次新增实测** | 装配能力矩阵(§2.2)· `RouterTLM` 范式实证 · `register_protocol_bridge` 死钩子复核 · `flit_category` 值域复核 · `06 号` 内部自相矛盾复核 |

---

## 2. `06-bis` 兼容性审查

`06-bis` 顶部 `Status Update (2026-10-21) — superseded by CEP` 已把它的实施路线逐项转隶给 CEP。**逐段核对如下**:

| `06-bis` 段 | 内容 | 对新队列(载体)的处置 |
|---|---|---|
| 顶部处置表 ·「§7 P3 `MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop`」 | **归 ArchForge**(协议语义建模);「**CEP 侧只提供 adapter 载体 + 能力声明 schema**」 | ✅ **这就是新队列的语义定义** —— 「adapter 载体」= 本队列要建的东西;协议语义仍归 ArchForge,本仓只做载体 + 声明 |
| 顶部处置表 ·「§3.4 / P2.0 programmatic bridge 路径」 | **supersede** → CEP 的 ProtocolEngine 插件 + canonical 10 流;**不做**单一 `CoherenceProtocol` C++ 类 | ✅ **不重发明**:`CoherenceProtocol` 抽象基类不做;但**「programmatic bridge 装配路径」本身是框架层事实,不被 supersede**(见下) |
| 顶部处置表 ·「§3.4 / P2.1 `CoherenceBridge` C++ 实施」 | **supersede** → 跨域组合由 `ADR-CEP-13` 承接 | ✅ 载体 = **canonical 10 流的端口面**,不是「双域 8 端口的 CoherenceBridge」 |
| **§3.2 8 端口装配实证** | Step 7a **不支撑** 4 类 Bundle / 4 端口;`module_factory.cc` dual 分支硬编码 cast;`REGISTER_CHSTREAM` 形态不匹配 | 🔴 **硬约束 —— 但本轮实测发现可消解**(§2.2) |
| **§3.4 选 C(programmatic bridge)** | `ChStreamModuleBase` + programmatic 端口 setter;「永久豁免」条款(不与「全部 JSON 化」反向游泳) | ✅ **承接**为新队列的装配路径 |
| **§5.2 / §5.3 / §5.4** | `SnoopProbe` / `SnoopRespBundle` / `CoherenceMsg` 字段定义(`bundle_base` + `ch_uint` + `BUNDLE_FIELD_WIDTH` 编译期锁) | ✅ **承接为 `CohIfBundle` 的字段基线**,但**按 canonical 10 流重排**(10 流 ≠ 4 类 Bundle) |
| **§5.6** | 「SnoopProbe / SnoopRespBundle / CoherenceMsg 是**网络 Bundle**(per `06 §3.2` 判定)」 | ⚠️ **与「CohIf 业务 Bundle」基线冲突** ⇒ 需架构裁决(**D-1**,§5) |
| **§3.5 / §7 阶段表** | JSON 路径打通推迟;Python 工厂改判为 capability schema + `tools/` 3 脚本 | ✅ 不冲突 |

### 2.1 `06-bis §3.2` 硬约束的**今日复核**(行号已漂移,结论未变)

| `06-bis` 记载 | **今日实测** | 判定 |
|---|---|---|
| `chstream_adapter_factory.hh:33-110` 4 个模板方法 | `:34` `registerAdapter` · `:42` `registerMultiPortAdapter` · `:84` `registerDualPortAdapter` · `:103` `registerBidirectionalPortAdapter` | ✅ 成立 |
| `port_count_[type]` 仅 Multi/Dual 赋值;Dual **固定 = 2** | `:47` Multi 赋 N · `:91` Dual 赋 **2** · `:108` Bidir 赋 N | ✅ 成立 |
| `module_factory.cc:644-645` `is_multi`/`is_dual` 二选一 | **`:668-669`**(行号漂移) | ✅ 成立,行号已变 |
| `module_factory.cc:707-709` dual 硬编码 cast | **`:729-731`** `static_cast<DualPortStreamAdapter<...>>`(行号漂移) | ✅ 成立,行号已变 |
| 「`REGISTER_CHSTREAM` 是 3 参数 tuple」 | ❌ **不准确** —— `chstream_register.hh:50` 的 `REGISTER_CHSTREAM` 是**无参**批量宏;带 Bundle 模板的是 **`:175` `REGISTER_CHSTREAM_DUAL(mod_type, pe_req_t, pe_resp_t, net_req_t, net_resp_t)`**(5 参数) | ⚠️ **06-bis 此处表述有误**,新文档不得沿用 |

### 2.2 ★ 本轮新发现:**硬约束可被「单 Bundle + flow_id」消解**

把 `06-bis §3.2` 的装配能力矩阵按**今日源码**重建:

| 适配器 | Bundle 类型参数 | 端口数 | 能否表达「多对不同 Bundle 类型」 |
|---|---|---|---|
| `StreamAdapter`(single) | `Req, Resp` | 1 | ❌ 1 对 |
| `MultiPortStreamAdapter` | `Req, Resp, N` | N | ❌ **N 端口共用同一对** |
| `DualPortStreamAdapter` | `PE_Req, PE_Resp, Net_Req, Net_Resp` | 固定 2 | ⚠️ 2 对,但语义被固定为「PE 侧 / Net 侧」 |
| `BidirectionalPortAdapter` | `BundleT, N` | N | ❌ 1 个类型,但 **N 端口 × (req_in + resp_out)** |

⇒ **没有任何变体支持「多对不同 Bundle 类型」** —— 这正是 `06-bis §3.2` 判定的死结。

**但死结的前提是「10 流 ⇒ 多类 Bundle」**。若 **canonical 10 流共用一个 `CohIfBundle`**(以 `flow_id` 区分流号),则:
- Bundle 类型数 = **1** ⇒ `BidirectionalPortAdapter<M, CohIfBundle, N>` 直接够用 ⇒ **框架层零新增 StreamAdapter 子类**;
- 范式**仓内已验证**:`router_tlm.hh:139-142` 用 `InputStreamAdapter<NoCFlitBundle>` + `OutputStreamAdapter<NoCFlitBundle>` + `BidirectionalPortAdapter<RouterTLM, NoCFlitBundle, NUM_PORTS>` ⇒ **载体照抄即可**;
- 而 `RouterTLM` 正是 `06 号 §6.2` 标注的 **Network router** 角色 ⇒ **「ChStream 构建的互联网络」的现成范例**。

⇒ **D-2 由此成立**:载体的框架依赖 = `ChStreamModuleBase` + `BidirectionalPortAdapter`(已存在)+ `ProtocolBridge`(本队列落地)+ `register_protocol_bridge`(死钩子扩展)。

---

## 3. `06 号` 兼容性审查

`06 号` 是**真源 1**,**不在 M1 冻结集**(`cep_freeze_check.sh` 的 `FROZEN_GLOBS` 只含 `docs/architecture/07-coherence-exploration-platform.md`)=> architect 可直接修订,但仍受 AGENTS.md 真源门槛约束(需 ADR 触发 / Oracle 评审)。

| `06 号` 段 | 内容 | 判定 |
|---|---|---|
| **§1.1 / §2.1** | L1~L6 分层,严格对齐 `01 号 §1.1` | ✅ 载体落在 **L4/L5**,不动 L6 |
| **§4.1** | seam 在 **L5 Bundle ↔ L4 StreamAdapter 之间** | ✅ **新队列的架构定位**:`CohIfBundle`(L5 业务)↔ `NoCFlitBundle`(L5 网络)的转换器 = seam 实现 |
| **§4.4** | `ProtocolBridgeEr`(类型擦除)+ `ProtocolBridge<InB, OutB, Ordered>`(模板)+ `AbstractProtocolBridge` identity 基线 | ✅ **直接承接**为特化桥的类形态(**本队列首次落地**) |
| **§4.5** | 元数据承载:per-bridge 静态(`protocol_id_`/`bridge_id_`)= P1 范围;per-packet `tlm_extension` = P3+ | ✅ 承接 **per-bridge** 部分 |
| **§9.3** | `register_protocol_bridge` 是 **write-only 死钩子**;「当引入 `ProtocolBridgeFactory` 时,**扩展**为 `(bridge_name, factory_lambda)`,**不重发明**」 | ✅ **挂载点已有**;⚠️ 与 §11.2 的 **P3** 子项「**拒绝 ProtocolBridgeFactory 模式**」**自相矛盾** ⇒ 见 **D-3** |
| **§6.1 / §6.2** | 互联网络 = SimModule 容器;容器端口契约 = **setter 注入**(`PORT_IS_INITIATOR` 已废弃) | ✅ 与 programmatic 装配一致 |
| **§6.3** | 三种用法:A flat + name_prefix / B SimModule 嵌套 JSON / **C programmatic bridge** | ✅ 载体走 **C** |
| **§3.1 / §3.2** | Bundle 二分类判定规则:**字段维度**(是否网络层字段) vs **owner 维度**(项目级 vs 业务模块) | ⚠️ **二义性** ⇒ 见 **D-1** |
| **§3.4** | 业务 Bundle 可演进(字段级加减 / 类型级并存 / 字段宽度不可改) | ⚠️ 与「CohIf Bundle 按网络级纪律管」冲突 ⇒ 见 **D-1** |
| **§4.3** | `flit_category` 列 **4** 值(REQUEST/RESPONSE/CREDIT/CREDIT_RESPONSE) | 🟡 **文档漂移** —— 仓内 `noc_bundles_tlm.hh:60-62` 只有 **3** 个 `CATEGORY_*`;`CREDIT_RESPONSE` 全仓**零命中** ⇒ 见 **D-7** |
| **§11.2 P1** | v0.4 方案 A:**纯测试 + 文档,0 代码**;所有代码增量推到 **P3+** | 🔴 见 **D-6** |
| **§11.3 P2** | `CoherenceBridge` 落地 + Python 多 dGPU 跨域桥接 | 🔴 见 **D-6** |
| **§15.1 / §15.4** | 「06 不实施 coherence 协议,仅演进 seam 引用 ArchForge 既有」;列出 ArchForge `coherence-bridge.md` 的 `CoherenceBridge : ChStreamModuleBase` 8 端口 + `SnoopProbe`/`SnoopResp` Bundle | ✅ **不重发明**的边界来源;CEP 侧只做载体 + 声明 |

### 3.1 `06 号` 内部自相矛盾(本轮新发现)

- `06 号 §9.3`:「当 P3+ 引入 `ProtocolBridgeFactory` 时,**扩展** `register_protocol_bridge` 为 `(bridge_name, factory_lambda)`」
- `06 号 §11.2` 的 **P3** 子项:「ModuleFactory Step 7c 精简版(per-bridge 静态分配,**拒绝 ProtocolBridgeFactory 模式**)」

⇒ **同一份真源内两条互斥要求**。**D-3** 给出兼容裁决。

---

## 4. 三个断点(证据链)

### C1 —— CEP 零 ChStream 承载体

```bash
grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/   # → 空
```

| 模块 | 真源要求的基类 | 实测基类 | 出处 |
|---|---|---|---|
| `StubCacheCore` | `ChStreamModuleBase` | **普通 class** | 要求:`ADR-CEP-07:67` / `07 号 §6.3` / `ADR-CEP-15:35`;实测:`stub_cache_core.hh:154` |
| `SharerTracker` 派生 | `SimModule` + `REGISTER_MODULE` | `SharerTrackerIf` / `FullMapDirectory` | 要求:`ADR-CEP-07` 决策 3;实测:`sharer_tracker.hh:249/273` |
| `FabricCollectivePlane` 派生 | `SimModule` | `CollectivePlaneIf` | 要求:同上;实测:`collective_plane.hh:325` |
| **ProtocolEngine 派生** | `ChStreamModuleBase` + `registerObject<T>` | **不存在**(队列 #6 的 `MoesiGpuAdapter` 声明为 header-only / 不进 `ModuleFactory`) | 要求:`ADR-CEP-07:64`;实测:`proposal.md` 新增表 |

**偏离登记链断裂**:队列 #5 登记过(「`07 号 §6.3` 读作**最终形态**口径」,`obj §11` Entry #66 ②);**队列 #6 未登记**。

### C2 —— canonical 10 流没有 Bundle(`ADR-CEP-07A:49` 悬空引用)

| 检查 | 结果 |
|---|---|
| `ADR-CEP-02` 内 `Bundle` | **零命中** |
| `07 号 §5.4` 业务 Bundle 列表 | `CacheReqBundle`/`CacheRespBundle`/`GmmuPteBundle`/`SdmaCmdBundle`/`DoorbellBundle`/`CrBundle`/`MsixBundle` —— **无 10 流 Bundle** |
| `include/bundles/` 17 文件 grep `coherence` | **零命中** |
| 仓内 10 流类型 | 只有 **5 条**有类型(`stub_cache_core.hh:90/100/111/119` 的 `SnoopResp`/`WritebackOut`/`RequestOut`/`AccessResult`,均为 `tlm::cep` 普通 struct,**非** `bundles::` POD);另 5 条在 ProtocolEngine 内部,**连类型都没有**(`07 号 §3.2`) |

⇒ `ADR-CEP-07A` 「不创建 `coherence_bundles_tlm.hh`」的**决策本身正确**(理由是抢 06-bis P2 文件),但其指定的**替代物不存在** ⇒ CohIf 至今**无承载形态**。

### C3 —— Bundle 化承诺绑在集合通信队列

`ADR-CEP-03:225`:
> **Bundle 化的未来路径(具体,不写「以后再说」)**: 推迟至 **P3**(`cpptlm-cep-nvlink-sharp-adapter`)—— 届时有真实传输通道,新建 `include/bundles/collective_bundles_tlm.hh`

⇒ 那是 **collective** 的 Bundle;**主干 10 流**在 `obj §4.2~§4.5` 的 13 个 change 里**无人认领**。

---

## 5. 架构裁决(D-1 ~ D-8)

### D-1 —— `CohIfBundle` 的分类:**业务语义 + 共享契约纪律**(第三类处理)

**问题**:`06-bis §5.6` 判 coherence Bundle 为「**网络** Bundle」;`06 号 §3.1` 的两条判定规则给出**相反**答案:

| 判定维度 | `SnoopProbe`/`CohIf` 字段的实际形态 | 结论 |
|---|---|---|
| **字段维度**(是否有网络层字段) | `addr`/`transaction_id`/`type`/`perm`/`data` —— **无** `src_node`/`dst_node`/`flit_category`/`vc_id` | ⇒ **业务** |
| **owner 维度**(项目级 vs 业务模块 owner) | CEP 平台层定义,跨 Stub Cache + ProtocolEngine + Directory Plane 三方共享 | ⇒ **网络** |

**裁决**:
1. **分类 = 业务语义** —— canonical 10 流是「一致性事务语义抽象」(`07 号 §3.1`),字段无网络层特征 ⇒ 按**字段维度**判为业务 Bundle;
2. **演进纪律 = 网络级** —— 它是**跨模块共享契约**,扩展须走 OpenSpec change + `BUNDLE_FIELD_WIDTH` 编译期锁(`06 号 §3.3` 网络 Bundle 列);
3. ⇒ **`06 号 §3` 需补一列「演进纪律」与「分类」解耦**(分类回答「是什么」,纪律回答「谁能改」)。`06 号` 非冻结集 ⇒ 可直接修订(需 ADR 触发)。

**理由**:现有二分类把「分类」与「纪律」**绑在一列**(`06 号 §3.3` 表),遇到「跨模块共享但非网络字段」的场景就二义。CEP 的 CohIf 是**第一个**触发该二义的实例 ⇒ 必须解耦,否则后续每个跨模块共享 Bundle 都要重新吵一遍。

### D-2 —— 装配:**programmatic**(承接 `06-bis §3.4` 候选 C),但 Bundle 类型**收敛为 1 个**

**裁决**:
1. `CohIfBundle` **单一类型**,10 流以 `flow_id` 区分 ⇒ 消解 `06-bis §3.2` 的「多 Bundle 类型」死结(§2.2);
2. 端口面用 `BidirectionalPortAdapter<M, CohIfBundle, N>`(仓内已存在,`chstream_adapter_factory.hh:103`)⇒ **框架层零新增 StreamAdapter 子类**;
3. 装配走 **programmatic**(setter 注入)—— 与 `06 号 §6.1`「容器端口契约 = setter 注入」一致,并落在 `06-bis §3.4` 的**永久豁免**条款内(不与 `cpptlm-dgpu-soc-physical-ip-port-boundary` 的「全部 JSON 化」反向游泳)。

### D-3 —— 特化桥:`ProtocolBridge` 双层 + 钩子扩展(**不引入 `ProtocolBridgeFactory` 类**)

**裁决**:采纳 `06 号 §4.4` 的 `ProtocolBridgeEr` + `ProtocolBridge<InB, OutB, Ordered>`;**扩展** `module_factory.hh:313-315` 的 `register_protocol_bridge` 为 `(bridge_name, factory_lambda)` 形态。

**同时满足 `06 号` 的两条互斥要求**:
- `§9.3`「**扩展**既有 register 入口,**不重发明**」✅(扩同一个钩子)
- `§11.2` 的 **P3** 子项「**拒绝 ProtocolBridgeFactory 模式**」✅(只扩钩子签名,**不新建** factory 类)

**与 `01 号 §1.1`「当前 L3+L4 已达 Gem5 MessageBuffer 水准,**不需新增抽象层**」一致** ⇒ 不引入第 6 个 StreamAdapter 变体、不引入 factory 抽象层。

### D-4 —— `CohIfBundle` 的路径:**避开** `coherence_bundles_tlm.hh`

**裁决**:`include/bundles/cep_coherence_bundles_tlm.hh`。

**理由**:`ADR-CEP-07A:49` 明确「**不创建** `include/bundles/coherence_bundles_tlm.hh`」⇒ 该路径**不可用**(ADR 不可变,只能绕)。命名风格与队列 #5 已登记的 collective 未来路径(`include/bundles/collective_bundles_tlm.hh`,`ADR-CEP-03:225`)**同构**。

**须登记的偏离**:对 `ADR-CEP-07A` 第 3 项的偏离 —— 其「不创建」的理由(抢 06-bis P2 文件)**已失效**(06-bis P2 随 06-bis 被 CEP supersede,永不实施),但**路径约束本身仍有效** ⇒ 新路径 + 偏离登记。

### D-5 —— 排期位置:**插在队列 #6 与 #7 之间**(per 用户裁决 2026-10-26)

| # | Change | 交付语义 |
|---|---|---|
| **6** | `cpptlm-cep-moesi-gpu-adapter` | 首个真实协议语义 + **特化字典**(6→3 映射表)+ `perm` 形态声明 |
| **7(新)** | `cpptlm-cep-cohif-carrier`(暂名) | **载体**:CohIf Bundle + ProtocolEngine 的 ChStream 端口面 + `ProtocolBridge` 特化桥 |
| **8(原 7)** | `cpptlm-cep-chi-rnf-subset` | 第二个实例,**首次在真实载体上验证** |

**为什么必须插在 #6 之后、#7 之前**:
- #6 的「装配验证」当前**只能是逻辑级**(`stub_cache_core.hh:42` 明写 `net_side` 由 **mock ProtocolEngine** 驱动);
- 若载体队列排在 #7 之后 ⇒ **#7 会继承 #6 的无载体装配模式** ⇒ **载体债复制一遍**;
- 载体队列消费 #6 的特化字典 ⇒ **顺序上 #6 必须先行**。

### D-6 —— `06 号 §11.2/§11.3` 的时间线**转隶 CEP**

**事实**:`06 号 §11.2` v0.4 方案 A 把承载 seam 的**全部代码增量**推到 **P3+**;`§11.3` P2 = `CoherenceBridge` 落地。**但 06-bis 已被 CEP supersede ⇒ `06 号` 的 P1/P2 未实施且不再实施**。

**裁决**:`06 号 §11.2/§11.3` 的**时间线**作废,改由 CEP 排期承接;载体落 **CEP 队列 #7(新)**,时点 = Stage 1 内、#6 之后。`06 号` 加 `## Status Update` 登记这处**时间线转隶**(非冻结集 ⇒ 无需两步授权)。

### D-7 —— `06 号 §4.3` 的 `flit_category` 值域漂移

**事实**:`06 号 §4.3` 列 4 值;仓内 `noc_bundles_tlm.hh:60-62` 只有 3 个 `CATEGORY_*`;`CREDIT_RESPONSE` 全仓零命中。

**裁决**:**正面结论** = CEP 的 3 类与网络 Bundle 的 3 类**精确 1:1** ⇒ `06 号 §4.3`「网络 Bundle 不需要新加字段」**成立**;**订正** `06 号 §4.3` 的 4 值表述为 3 值(与 `06 号 §15.2` 的「`NoCFlitBundle.vc_id` 4-bucket(4 vnet per CHI)」**不冲突** —— 后者说的是 **VC 数**,不是 **flit_category 值数**)。

### D-8 —— 队列 #6 的偏离登记**必须补**

`proposal.md` 把「header-only / 不进 `ModuleFactory`」当**事实**陈述,但它是对 `ADR-CEP-07 决策 3` + `07 号 §6.3` 的**偏离** ⇒ 须照 `ADR-CEP-03:219-225` 的格式登记,并指向本队列(具体路径,不写「以后再说」)。

---

## 6. 新队列设计(`cpptlm-cep-cohif-carrier`)

### 6.1 交付面(4 项)

| # | 交付物 | 承接来源 | 关键约束 |
|---|---|---|---|
| **1** | `include/bundles/cep_coherence_bundles_tlm.hh` —— `bundles::CohIfBundle` | 字段基线:`06-bis §5.2/§5.3/§5.4`(`SnoopProbe`/`SnoopRespBundle`/`CoherenceMsg`) | `bundle_base` 空基类 + `ch_uint`/`ch_bool` + `BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable`;**单一类型**,10 流以 `flow_id` 区分;**不**含网络层字段(D-1) |
| **2** | `include/tlm/cep/protocol_engine_base.hh` —— `ChStreamModuleBase` 派生的 ProtocolEngine 基类 + ChStream 端口面 | `ADR-CEP-07:64` + `router_tlm.hh:139-142` 范式 + `06-bis §3.4` 候选 C | `InputStreamAdapter<CohIfBundle>` / `OutputStreamAdapter<CohIfBundle>`;**programmatic** 注入;**持** `ProtocolBridgeEr*` |
| **3** | `include/framework/protocol_bridge.hh` —— `ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>` + `AbstractProtocolBridge` | `06 号 §4.4`(**首次落地**) | 双层:类型擦除基类 + 模板子类;`Ordered` 单 bool(不引入三分类,per `06 号 §4.4`) |
| **4** | `module_factory.hh:313-315` `register_protocol_bridge` 扩展为 `(bridge_name, factory_lambda)` | `06 号 §9.3` | **不新建** factory 类(D-3);**死钩子 → 活钩子** |

### 6.2 端到端数据流(闭合用户两个目标)

```
        ┌──────────────── 业务侧 ────────────────┐
Traffic Generator ──ChStream──> Stub Cache Core
                                   │  CohIfBundle (业务 Bundle, 10 流 + flow_id)
                                   │  ★ 载体队列交付 ① + ②
                                   ▼
                        ProtocolEngine 插件 (MoesiGpuAdapter / ChiRnfAdapter / ...)
                                   │  协议特化:6 态×6 事件 / 6→4 perm 投影 / 6→3 消息类映射
                                   │  ★ 载体队列交付 ③: ProtocolBridge<CohIfBundle, NoCFlitBundle>
                                   ▼
                        ┌──────── 互联网络(ChStream 构建)────────┐
                        │ RouterTLM / CrossbarTLM                │
                        │   BidirectionalPortAdapter<NoCFlitBundle, N> │
                        │   NoCFlitBundle(网络 Bundle,16 字段,编译期锁)│
                        └──────────────┬───────────────────────────┘
                                       ▼
                            VramControllerTLM / 对端 adapter
```

| 用户目标 | 闭合方式 | 交付物 |
|---|---|---|
| **「ch_stream 承载业务类 bundle」** | `CohIfBundle`(业务语义)走 `ChStreamModuleBase` 的 `Input/OutputStreamAdapter` | ① + ② |
| **「互联网络可特化到具体通信网络」** | `ProtocolBridge<CohIfBundle, NoCFlitBundle>` = `01 号 §1.1` 的 **L5↔L4 seam**;换 `Axi4Bundle` 即换一种网络 | ③ + ④ |

**特化的可插拔性**:桥是**模板类** ⇒ 「CohIf → NoC」/「CohIf → Axi4」/「CohIf → PCIe TLP」是**同一 seam 的不同实例**,由 `protocol_id_` 区分(`06 号 §4.5` per-bridge 静态)。

### 6.3 队列边界(明确不做)

| 不做 | 理由 |
|---|---|
| 协议语义建模(6×6 表的**内容**) | 归 ArchForge(`06-bis` 顶部处置表「§7 P3 `MesiGpuProtocol` … 归 ArchForge」);本仓只做载体 + 声明 |
| `moesi_gpu` 的具体实现 | 属队列 #6 |
| `chi-rnf` / `mesif` 等其余实例 | 属队列 #8+ |
| 跨域组合(多协议混合) | 属 `ADR-CEP-13`,走「人工声明 + 实验证伪」路线 |
| per-packet `tlm_extension` 元数据 | `06 号 §4.5` = P3+;本队列只做 per-bridge 静态 |
| `coherence_bundles_tlm.hh` 路径 | `ADR-CEP-07A:49` 明确「不创建」(D-4) |
| 新增 StreamAdapter 子类 / factory 抽象层 | `01 号 §1.1`「不需新增抽象层」+ D-3 |

### 6.4 判据面(验收面,builder 阶段兑现)

| ID | 判据 | 反向 fixture |
|---|---|---|
| **CC-01** | `CohIfBundle` **单一类型**:10 流均可由 `flow_id` 表达;**不含**网络层字段(无 `src_node`/`dst_node`/`flit_category`/`vc_id`) | 注入网络层字段 ⇒ 红 |
| **CC-02** | `BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable` 断言生效 | 改字段宽度 / 加虚函数 ⇒ 编译红 |
| **CC-03** | `ProtocolBridge` 的 `pack`/`unpack` 对 `CohIfBundle → NoCFlitBundle` **双向可逆**(round-trip 恒等) | 注入不可逆转换 ⇒ 红 |
| **CC-04** | 载体端口面:`BidirectionalPortAdapter` 装配后,一次 CohIf 事务可**穿过** adapter 抵达对端 | 断链 ⇒ 红 |
| **CC-05** | `register_protocol_bridge` 从**死钩子**变**活钩子**:注册后 `get_bridges()` 可读且被 ModuleFactory 消费 | 只注册不消费 ⇒ 红 |
| **CC-06** | 6→3 映射表产出的 `CATEGORY_*` **全部落在** `NoCFlitBundle` 的 3 个 `CATEGORY_*` 常量内(D-7) | 产出第 4 类 ⇒ 红 |

> **纪律**(per 队列 #4 `Entry #65` / 队列 #5 `Entry #68`):**守门/闸的新断言必须有反向 fixture**;**纯负向断言必须有正控**;**判据实现退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**。

---

## 7. 对既有真源的影响清单

| 真源 | 动作 | 门槛 | 归属 |
|---|---|---|---|
| `docs/architecture/06-inter-module-network.md` §3 | **补「演进纪律」列 + 分类判定优先级**(D-1) | ADR 触发 / Oracle 评审 | architect 可起草(非冻结集) |
| `docs/architecture/06-inter-module-network.md` §4.3 | **订正** `flit_category` 4 值 → 3 值(D-7) | 同上 | 同上 |
| `docs/architecture/06-inter-module-network.md` §11.2/§11.3 | **追加 Status Update**:时间线转隶 CEP(D-6) | 同上 | 同上 |
| `docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md` | **追加 Status Update**:登记对第 3 项替代物的偏离 + 新路径(D-4) | **冻结面** ⇒ 两步授权 | planner/builder |
| `docs/adr/ADR-CEP-07-simulation-and-integration-semantics.md` | **追加 Status Update**:ProtocolEngine 基类落地排期(队列 #7 新) | **冻结面** ⇒ 两步授权 | planner/builder |
| `docs/adr/ADR-CEP-15-cachetlm-alignment.md` | **追加 Status Update**:`§1.2` 表格「基类 = `ChStreamModuleBase`」与代码不符(`stub_cache_core.hh:154`) | **冻结面** ⇒ 两步授权 | planner/builder |
| `docs/adr/ADR-CEP-NUMBER-MAP.md` | 新增 ADR 后更新清单(21 → 22)+ KPI 预算(13 → 14) | **冻结面** ⇒ 两步授权 | planner/builder |
| **新** `docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md` | 承载 D-1 ~ D-8 | 签发前**不得**落 `docs/adr/`(会触发 M1 冻结 drift ⇒ 守门红) | architect 起草 → DRI 签发 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` §4.0 / §4.2 / §4.6 / §11 | 队列插入(13 → 14)+ 台账 Entry | 五处同改 + 冻结 ADR 同步 | **planner** |

---

## 8. 待 DRI 裁决

| # | 裁决点 | 推荐 |
|---|---|---|
| **1** | 新队列名 | `cpptlm-cep-cohif-carrier`(承载 CohIf Bundle + ProtocolEngine 端口面 + ProtocolBridge) |
| **2** | 全局算术 | 13 → **14** change / 14 季度;终态 2029-Q4 → **2030-Q1** |
| **3** | 新 ADR 编号 | `ADR-CEP-19`(现有最大 `ADR-CEP-18`;`ADR-CEP-NUMBER-MAP` 清单 21 → 22) |
| **4** | `CohIfBundle` 分类 | **业务语义 + 网络级纪律**(D-1)⇒ 需 `06 号 §3` 补列 |
| **5** | 载体队列的估时 | 8 周(4 项交付物,含框架层 2 处小改 + 1 处死钩子激活) |

---

## 9. 签发前置条件(本 ADR 草案落地为 `docs/adr/ADR-CEP-19-*.md` 的前置)

1. `obj §11` **先有对应台账 Entry**(否则 `cep_freeze_check.sh --update --authorized-by` 会因「台账无对应 entry」拒跑);
2. **两步授权**:先 commit 产物 → `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` → 再 commit 清单(新 ADR 文件会触发 M1 `drift` ⇒ 必须重生成清单);
3. `ADR-CEP-NUMBER-MAP` 清单与 KPI 预算同步(21 → 22;13 → 14);
4. `docs/architecture/06-inter-module-network.md` 的 3 处修订走「真源 1 变更门槛」(ADR 触发 / Oracle 评审)—— **本轮 architect 产出只提供草案与依据,不代签**。

---

## 10. 关联

- 队列 #6 planner 四件套:`openspec/changes/cpptlm-cep-moesi-gpu-adapter/`(proposal / design / 2 份 delta spec)
- 排期真源:`.rddf/roadmap/objectives/obj-cep-platform.md`(§4.0 排队总表 / §11 台账)
- 架构基线:`docs/architecture/01-hybrid-architecture-v2.1.md` · `06-inter-module-network.md` · `06-bis-coherence-as-network-extension.md` · `07-coherence-exploration-platform.md`
- 契约:`docs/adr/ADR-CEP-{00,02,03,05,07,07A,12,13,15,16}-*.md`
- 既有同类先例:`ADR-CEP-03` Status Update 2026-10-26(Bundle 路径偏离登记 + 未来路径 P3)—— 本队列的**偏离登记格式**照此
