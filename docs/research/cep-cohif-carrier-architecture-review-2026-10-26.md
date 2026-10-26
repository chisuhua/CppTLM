# CEP CohIf 载体队列 —— 架构重审与设计(v2)

> **状态**: 🟡 **architect 产出**(调研 + 决策草案)· **未签发**
> **日期**: 2026-10-26 · **修订**: v2(per critic 第 1 轮 REJECT:2 BLOCKER + 10 SHOULD_FIX + 7 NIT 全落地)
> **角色**: architect(per 用户裁决「切 architect 重新审视前面完成的内容」)
> **独立性说明**: 本仓为**单开发者**环境;本评审由 AI agent 完成,**不等于真人外部评审**(per `obj-cep-platform §9.3` R10 先例 ⇒ 结论应登记为「残余风险 + DRI 书面接受」,而非「已闭环」)。
> **本文档定位**: **附属 1(调研层)**。⇒ **签发前,任何文档不得引用本文档的「裁决」作为依据**;裁决的生效形态是 `ADR-CEP-19`(见 §9 签发前置)。

---

## 0. 摘要

**四条结论**:

1. **队列 #6 的 planner 产出与 `01 号` 核心立场一致** —— 6→3 消息类映射表**就是** L5↔L4 seam 的数据形态,其目标 `CATEGORY_*` 3 类**就定义在网络 Bundle 自己的头文件里**(`include/bundles/noc_bundles_tlm.hh:60-62`)。

2. **CohIf 至今没有承载形态** —— 三处断点(§4)均有 `file:line` 证据:
   - **C1** CEP 的 **7** 个 `.hh`(另有 6 个 json/schema,共 13 文件)**零** `Bundle`/`ChStream`/`StreamAdapter` 引用;`StubCacheCore` 实为普通 class(`stub_cache_core.hh:154`),**非** `ChStreamModuleBase` —— 而 `ADR-CEP-07:64/67` 与 `07 号 §6.3` 明写它应该是。
   - **C2** 「CEP 用 **canonical 10 流的 Bundle**」是**悬空引用**,且**同源两处**:`ADR-CEP-07A:49` + `ADR-CEP-16:45`。该替代物**不存在**(`ADR-CEP-02` grep `Bundle` 零命中;`07 号 §5.4` 业务 Bundle 列表无它;`include/bundles/` 17 个 `.hh` grep `coherence` 零命中)。
   - **C3** 唯一的「Bundle 化」承诺绑在**集合通信**队列(`ADR-CEP-03:225` 推迟至 `cpptlm-cep-nvlink-sharp-adapter`),**主干流无人认领**。

3. **根因不是缺设计,而是「既有设计从未被一个队列认领」** —— 四份设计齐全、0 个队列:
   | 既有设计 | 位置 | 归属现状 |
   |---|---|---|
   | coherence Bundle 字段定义(`SnoopProbe`/`SnoopRespBundle`/`CoherenceMsg`) | `06-bis §5.2~§5.4` | 被 `ADR-CEP-07A` supersede 但**无替代物** |
   | `ProtocolBridge` 双层类形态 | `06 号 §4.4` | 「§15 演进 seam 才实施」⇒ 永久推迟 |
   | `register_protocol_bridge` 挂载钩子 | `06 号 §9.3` + `module_factory.hh:313-315` | **write-only 死钩子**,无人读 |
   | 装配路径裁决 | `06-bis §3.4`(候选 C) + `ADR-CEP-16:46`(**排除** programmatic) | 两条互相抵消,净结果 = 无路径 |

4. **本轮最重要的更正(v2 新增,per critic S1)**:v1 说「canonical 10 流共用一个 `CohIfBundle`」**是概念错误** —— **只有 5 流跨 Stub↔PE 边界**,另 5 流在 ProtocolEngine 内部终结或对接 Directory Plane(`07 号 §3.2`、`ADR-CEP-05` 折叠映射表)。
   ⇒ **`CohIfBundle` 只需表达 5 流** ⇒ 字段集小得多、单类型**更**成立,且「`barrier` / `forward_data` / `back_invalidate` 对 stub 不可见」从**运行期纪律**回到**类型不可表达**(比 v1 方案更强)。

**装配路径(v2 更正)**:v1 选 **programmatic** —— **与冻结 `ADR-CEP-16:46/:86` 直接冲突且仓内零先例**。v2 改为 **JSON 链**(`registerBidirectionalPortAdapter` + `is_multi` 分支的**虚分派** `bind_port_pair`),**已实测全程通**,且只需 **1 行注册 + 1 个新宏**。

---

## 1. 审查范围与证据基准

| 项 | 内容 |
|---|---|
| **被审对象** | 队列 #6 `cpptlm-cep-moesi-gpu-adapter` 的 planner 四件套 + 队列 #1~#5 已归档产出 |
| **架构基线** | `01 号`(v2.1.10)· `06 号`(v0.4)· `06-bis`(v0.3)· `07 号`(v0.1) |
| **契约基线** | `ADR-CEP-{00,00A,02,03,05,07,07A,12,13,15,16,17,18}-*.md` + `ADR-CEP-NUMBER-MAP.md` |
| **代码基线** | `include/tlm/cep/*`(7 `.hh`)· `include/bundles/*`(17 `.hh`)· `include/framework/*` · `src/core/module_factory.cc` · `include/chstream_register.hh` |
| **方法** | 逐 `file:line` 核实,**不接受文档转述**(B-1 教训:假测量) |
| **v2 新增实测** | 装配链全程(§2.2)· `ADR-CEP-16` 三行 · programmatic 零先例 · 文件计数 · `06 号 §3.1/§3.3` 落点 · `07 号 §8.1` 旧数 |

---

## 2. `06-bis` 兼容性审查

`06-bis` 顶部 `Status Update (2026-10-21) — superseded by CEP` 已把实施路线逐项转隶 CEP。

| `06-bis` 段 | 内容 | 对新队列的处置 |
|---|---|---|
| 顶部处置表 ·「§7 P3 `MesiGpuProtocol` / `MesiThreeLevelProtocol` / `translate_snoop`」 | **归 ArchForge**;**「CEP 侧只提供 adapter 载体 + 能力声明 schema」** | ✅ **这就是新队列的语义定义** |
| 顶部处置表 ·「§3.4 / P2.0 programmatic bridge 路径」 | **supersede** → CEP 的 ProtocolEngine 插件 + canonical 10 流 | 🔴 **v1 在此断言不实** —— 见 §2.3 |
| 顶部处置表 ·「§3.4 / P2.1 `CoherenceBridge` C++ 实施」 | **supersede** → 跨域组合由 `ADR-CEP-13` 承接 | ✅ 载体 ≠ 双域 8 端口的 `CoherenceBridge` |
| **§3.2 8 端口装配实证** | Step 7a 不支撑 4 类 Bundle / 4 端口 | 🔴 硬约束 —— **v2 以「5 流单 Bundle + JSON 链」消解**(§2.2) |
| **§5.2 / §5.3 / §5.4** | `SnoopProbe` / `SnoopRespBundle` / `CoherenceMsg` 字段定义 | ✅ **承接为 `CohIfBundle` 字段基线**(仅 5 流用到的字段) |
| **§5.6** | 「SnoopProbe / SnoopRespBundle / CoherenceMsg 是**网络 Bundle**」 | ⚠️ 与「CohIf 业务 Bundle」基线冲突 ⇒ **D-1** |
| **§3.5 / §7 阶段表** | JSON 路径打通推迟;Python 工厂改判为 capability schema + `tools/` 3 脚本 | ✅ 不冲突 |

### 2.1 `06-bis §3.2` 硬约束的**今日复核**(行号已漂移;两处记载有误)

| `06-bis` 记载 | **今日实测** | 判定 |
|---|---|---|
| `chstream_adapter_factory.hh:33-110` 4 个模板方法 | `:34` `registerAdapter` · `:42` `registerMultiPortAdapter` · `:84` `registerDualPortAdapter` · `:103` `registerBidirectionalPortAdapter` | ✅ 成立 |
| `port_count_[type]` **仅** Multi/Dual 赋值;Dual **固定 = 2** | `:47` Multi 赋 N · `:91` Dual 赋 **2** · **`:108` Bidir 也赋 N** | ❌ **06-bis 此处有误** —— Bidir 也赋 `port_count_`,**这正是 v2 装配链的支点**(Bidir ⇒ `isMultiPort` 为真 ⇒ 走 `is_multi` 分支 ⇒ 虚分派 `bind_port_pair`) |
| `module_factory.cc:644-645` `is_multi`/`is_dual` 二选一 | **`:668-669`**(行号漂移) | ✅ 成立 |
| `module_factory.cc:707-709` dual 硬编码 cast | **`:727-741`**(行号漂移) | ✅ 成立 |
| 「`REGISTER_CHSTREAM` 是 **3 参数 tuple**」 | ❌ **不准确** —— `chstream_register.hh:50` 是无参批量宏;带 Bundle 的是 **`:175` `REGISTER_CHSTREAM_DUAL(mod_type, pe_req_t, pe_resp_t, net_req_t, net_resp_t)`**(5 参数);**不存在** `REGISTER_CHSTREAM_BIDIRECTIONAL` | ⚠️ 06-bis 表述有误;新文档不沿用 |

### 2.2 ★ 消解死结的两步(全部实测)

**第一步:Bundle 类型数收敛为 1 —— 但只在 5 流范围内**

`07 号 §3.2` 与 `ADR-CEP-05:76/83`:Stub 的 `net_side` **只感知 5 条流**(request / completion / snoop / snoop_resp / writeback),另 5 条在 ProtocolEngine 内部终结或对接 Directory Plane。

⇒ **跨 Stub↔PE 边界的只有 5 流** ⇒ `CohIfBundle` 只需覆盖这 5 流的字段(addr / txn_id / perm / data / size / is_write / snoop_type / dirty / error)。
⇒ 而 `forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack` **天然不在该 Bundle 的 `flow_id` 域内** ⇒ 「对 stub 不可见」是**类型不可表达**的,不是纪律。

**第二步:JSON 装配链全程实测通过**

| 环节 | 实证 |
|---|---|
| 注册 | `chstream_adapter_factory.hh:103` `registerBidirectionalPortAdapter<M,B,N>` → `:108` `port_count_[type] = N` |
| 判定 | `:61-63` `isMultiPort()` = `port_count_ > 1` ⇒ Bidir(N≥2)**命中** |
| 分支 | `module_factory.cc:668-669` 取 `is_multi`;`:743-750` 走 **`adapter->bind_port_pair(i, req_out, resp_in, resp_out, req_in)`** |
| 虚分派 | `bidirectional_port_adapter.hh:128` `bind_port_pair(...) override` —— 填 `req_out_port_/resp_in_port_/resp_out_port_/req_in_port_` |
| 先例 | `chstream_register.hh:119` `registerBidirectionalPortAdapter<tlm::RouterTLM, bundles::NoCFlitBundle, 5>("RouterTLM")` |

⇒ **`is_multi` 分支是泛型虚分派,无硬编码 cast**(硬编码 cast 只在 `is_dual` 分支 `:727-741`)⇒ **新模块类型走 JSON 链是通的**,只需 1 行注册。

> **v1 vs v2 的差别**:v1 用「RouterTLM 范式」证明的是 **JSON 链**,决策却选了 **programmatic** —— 论证链与决策链不是同一条。v2 把两者对齐。

### 2.3 与 `ADR-CEP-16` 的关系(v1 的**不实断言**已订正)

| 文件 | 原文 | v1 的处理 |
|---|---|---|
| `ADR-CEP-16:45`(P2.0 行) | 「CEP 用 **ProtocolEngine 插件** + **canonical 10 流** 的 Bundle(per `ADR-CEP-02` / `07 号 §5.4`)」 | **漏报** —— 与 C2 所报 `ADR-CEP-07A:49` **同源**,是第 2 处悬空引用 |
| `ADR-CEP-16:46`(P2.1 行) | 「**不做** programmatic bridge 的 C++ 实施」 | **未处理** |
| `ADR-CEP-16:86`(决策 5 排除表) | 「**不**实现 programmatic bridge 的 C++ 路径;CEP 走人工声明路线」 | **未处理**;v1 却写「programmatic bridge 装配路径**本身是框架层事实,不被 supersede**」= **对本仓 Standing 文本的不实陈述** |

**v2 订正**:
1. **删掉**「不被 supersede」这句断言(它是错的);
2. **D-2 改选 JSON 链** ⇒ 与 `ADR-CEP-16:46/:86` **不冲突**(它们排除的是 programmatic bridge 的 C++ 路径,本队列走的是 JSON 端口注册,`RouterTLM` 同款既有路径);
3. **C2 清单补第 2 处**(`ADR-CEP-16:45`);
4. `ADR-CEP-16` 进入 §7 影响清单 —— 但属性是「**引用依据**」(证明不冲突),**不是**修改对象。

---

## 3. `06 号` 兼容性审查

`06 号` 是**真源 1**,**不在 M1 冻结集**(`FROZEN_GLOBS` 只含 `docs/architecture/07-coherence-exploration-platform.md`)⇒ 可直接修订,但受 AGENTS.md 真源门槛约束(需 Oracle 评审 or ADR 触发)。

| `06 号` 段 | 内容 | 判定 |
|---|---|---|
| **§4.1** | seam 在 **L5 Bundle ↔ L4 StreamAdapter 之间** | ✅ **新队列的架构定位** |
| **§4.4** | `ProtocolBridgeEr` + `ProtocolBridge<InB, OutB, Ordered>` | ✅ **直接承接**(本队列首次落地) |
| **§4.5** | per-bridge 静态(`protocol_id_`/`bridge_id_`)= P1 范围;per-packet = P3+ | ✅ 承接 **per-bridge** |
| **§9.3** | `register_protocol_bridge` 是 write-only 死钩子(实测 `:313-315` + `get_bridges()` 在 `private:` 段 `:326`);「扩展为 `(bridge_name, factory_lambda)`,**不重发明**」 | ✅ **挂载点已有**;⚠️ 与 §11.2 P3 子项冲突 ⇒ **D-3** |
| **§6.1 / §6.2** | 互联网络 = SimModule 容器;**容器**端口契约 = setter 注入 | ✅ 不冲突 —— 载体是**叶子模块**,走 JSON;setter 注入是**容器**(`SocInternalNoc`/`GpuInternalFabric`/`VramControllerTLM`)专属 |
| **§6.3** | 三种用法:A flat / B SimModule 嵌套 JSON / C programmatic bridge(跨域桥) | ✅ 载体走 **A/B**(JSON),**不**走 C |
| **§3.1 / §3.2** | Bundle 二分类:**字段维度** vs **owner 维度** | ⚠️ **二义性** ⇒ **D-1**(且 §3.1 **已有**「演进粒度」行,见 D-1) |
| **§3.3** | 「模块 owner 可改?」行 = 真正把**纪律**绑到分类的地方 | ⚠️ D-1 的**精确落点**在此,不是「补一列」 |
| **§4.3** | `flit_category` 列 **4** 值(含 `CREDIT_RESPONSE`) | 🟡 **漂移** —— 实测 `noc_bundles_tlm.hh:60-62` 只有 **3** 个 `CATEGORY_*`,`CREDIT_RESPONSE` 全仓零命中 ⇒ **D-7** |
| **§3.2 标题** | 「per `include/bundles/` **16** 个 .hh」 | 🟡 **漂移** —— 实测 **17** 个 `.hh`(另 1 个 `AGENTS.md`)⇒ 并入 **D-7** |
| **§11.2 P1** | v0.4 方案 A:**纯测试 + 文档,0 代码** | 🔴 见 **D-6** |
| **§11.3 P2** | `CoherenceBridge` 落地 | 🔴 见 **D-6** |
| **§15.1** | 「06 不实施 coherence 协议,仅演进 seam 引用 ArchForge 既有」 | ✅ 不重发明的边界来源 |

### 3.1 `06 号` 内部自相矛盾(D-3 的由来)

- `:781`(§9.3):「当引入 `ProtocolBridgeFactory` 时,**扩展** `register_protocol_bridge` 为 `(bridge_name, factory_lambda)`」
- `:925`(§11.2 P3 子项):「ModuleFactory Step 7c 精简版(per-bridge 静态分配,**拒绝 ProtocolBridgeFactory 模式**)」

**v1 的处置「扩钩子签名、不新建 factory 类 ⇒ 两条都满足」是文字游戏**(critic S6):`(bridge_name, factory_lambda)` 的 name→lambda 映射**就是**模式名解析的运行时机制,只是没叫 factory 类。
**v2 改为诚实表述**:采纳 §9.3 的扩展路径,**显式偏离** §11.2 P3 子项的拒绝条款,偏离理由 = 「§11.2 拒绝的是**独立 factory 抽象层**(与 `01 号 §1.1`「不需新增抽象层」一致);§9.3 的**钩子签名扩展**不是抽象层」,并登记进 §7 影响清单。

---

## 4. 三个断点(证据链)

### C1 —— CEP 零 ChStream 承载体

```bash
grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/   # → 空
find include/tlm/cep -name "*.hh" | wc -l                               # → 7
```

| 模块 | 真源要求的基类 | 实测 | 出处 |
|---|---|---|---|
| `StubCacheCore` | `ChStreamModuleBase` | **普通 class** | 要求:`ADR-CEP-07:67` / `07 号 §6.3` / `ADR-CEP-15:35`;实测:`stub_cache_core.hh:154` |
| `SharerTracker` 派生 | `SimModule` + `REGISTER_MODULE` | `SharerTrackerIf` / `FullMapDirectory` | 实测:`sharer_tracker.hh:249/273` |
| `FabricCollectivePlane` 派生 | `SimModule` | `CollectivePlaneIf` | 实测:`collective_plane.hh:325` |
| **ProtocolEngine 派生** | `ChStreamModuleBase` + `registerObject<T>` | **不存在** | 要求:`ADR-CEP-07:64`;实测:队列 #6 `proposal.md` 新增表 |

**偏离登记链断裂**:队列 #5 登记过(`obj §11` Entry #66 ②「`07 号 §6.3` 读作**最终形态**口径」);**队列 #6 未登记**。

### C2 —— 悬空引用,**同源两处**

| 检查 | 结果 |
|---|---|
| `ADR-CEP-07A:49` | 「CEP 用 **canonical 10 流的 Bundle**(per `ADR-CEP-02` / `07 号 §5.4`)」 |
| `ADR-CEP-16:45` | 「CEP 用 **ProtocolEngine 插件** + **canonical 10 流** 的 Bundle(per `ADR-CEP-02` / `07 号 §5.4`)」 |
| `ADR-CEP-02` 内 `Bundle` | **零命中** |
| `07 号 §5.4` 业务 Bundle 列表(7 项) | `CacheReqBundle`/`CacheRespBundle`/`GmmuPteBundle`/`SdmaCmdBundle`/`DoorbellBundle`/`CrBundle`/`MsixBundle` —— **无它** |
| `include/bundles/` 17 `.hh` grep `coherence` | **零命中** |
| 仓内流承载类型 | 仅 **3** 个 struct 承载流(`stub_cache_core.hh:90` `SnoopResp` / `:100` `WritebackOut` / `:111` `RequestOut`);`:119` `AccessResult` 是 **cpu-side** 结果,不承载流;均为 `tlm::cep` 普通 struct,**非** `bundles::` POD |

⇒ 「不创建 `coherence_bundles_tlm.hh`」的**决策正确**(避免抢 06-bis P2 文件),但其**替代物在两处 ADR 里都是悬空引用**。

### C3 —— Bundle 化承诺绑在集合通信队列

`ADR-CEP-03:225`:
> **Bundle 化的未来路径(具体,不写「以后再说」)**: 推迟至 **P3**(`cpptlm-cep-nvlink-sharp-adapter`)—— 届时有真实传输通道,新建 `include/bundles/collective_bundles_tlm.hh`

⇒ 那是 **collective** 的 Bundle;**主干流**在 13 个 change 里无人认领。

---

## 5. 架构裁决(D-1 ~ D-9)

### D-1 —— `CohIfBundle` 分类:**业务语义 + 网络级纪律**

**问题**:`06-bis §5.6` 判 coherence Bundle 为「网络」;`06 号 §3.1` 两条判定规则对 CohIf 给出**相反**答案 —— **字段维度**(无 `src_node`/`dst_node`/`flit_category`/`vc_id` ⇒ 业务)vs **owner 维度**(跨模块共享 ⇒ 网络)。

**裁决**:**分类 = 业务语义**;**演进纪律 = 网络级**(`BUNDLE_FIELD_WIDTH` 锁 + 扩展走 OpenSpec)。

**精确落点(v2 订正,v1 的「补一列」不准确)**:
- `06 号 §3.1` **已有**「演进粒度」行(`:261`),**已**把两类的纪律分开写 ⇒ **不需要补列**;
- 真正把纪律绑到分类的是 **`§3.3` 首行「模块 owner 可改?」**(`:297` —— 业务「✅ 允许」/ 网络「❌ 编译期锁」);
- ⇒ **D-1 的落点是 `§3.3` 加**一行「**跨模块共享的业务 Bundle**」的第三类:**分类=业务 / 纪律=网络**,并注明唯一实例 = `CohIfBundle`。

**顺序依赖(v2 新增,per critic S4)**:「`06 号` 非冻结集 ⇒ 可直接修订」**不等于**「无需门槛」。AGENTS.md 要求真源 1 变更需 Oracle 评审 or ADR 触发 ⇒ **必须先签发 `ADR-CEP-19`,再动 `06 号`**。用未签发的 ADR 草案为改真源背书 = 循环论证。

### D-2 —— 装配:**JSON 链**(`REGISTER_CHSTREAM_BIDIRECTIONAL` 宏 + `is_multi` 虚分派)

**裁决**(v2 从 programmatic 改为 JSON):
1. `CohIfBundle` **单一类型**,**只覆盖 5 条 RN-side 流**(§2.2 第一步)⇒ 消解 `06-bis §3.2` 的「多 Bundle 类型」死结;
2. 端口面用 `BidirectionalPortAdapter<M, CohIfBundle, N>`(仓内已存在,`chstream_adapter_factory.hh:103`);
3. **装配走 JSON 链**(实测全程通,§2.2 第二步)⇒ 与冻结 `ADR-CEP-16:46/:86`(排除 programmatic bridge)**天然不冲突**;
4. 新增宏 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)`(与既有 `chstream_register.hh:175` `REGISTER_CHSTREAM_DUAL` 对称),内含 `static_assert(n > 1)` —— 理由:`isMultiPort` 要求 `port_count > 1`(`:61-63`),**N=1 会落 single 分支调 `bind_ports`(空 shim,`bidirectional_port_adapter.hh:141`)** ⇒ 静默不绑定。

**为什么不选 programmatic(v1 的错误)**:
- `ADR-CEP-16:46/:86` **明写排除**;
- 实测 **零先例**:`grep -rn "new cpptlm::ChStreamInitiatorPort\|new cpptlm::ChStreamTargetPort" src/ include/ | grep -v module_factory` → **空**(ChStream 端口只在 `module_factory.cc` Step 7 内 new);
- `06-bis §3.4` 的候选 C 本身**未实施**且已被 supersede。

### D-3 —— 特化桥:`ProtocolBridge` 双层 + 钩子扩展(**显式偏离** §11.2 P3 子项)

**裁决**:采纳 `06 号 §4.4` 的 `ProtocolBridgeEr` + `ProtocolBridge<InB, OutB, Ordered>`;**扩展** `module_factory.hh:313-315` 的 `register_protocol_bridge` 为 `(bridge_name, factory_lambda)`。

**与 `06 号` 的两条要求的关系(v2 诚实表述,per critic S6)**:
- **遵从** §9.3(「扩展既有入口,不重发明」)✅
- **显式偏离** §11.2 P3 子项(「拒绝 ProtocolBridgeFactory 模式」)—— **偏离理据**:§11.2 拒绝的是**独立 factory 抽象层**(与 `01 号 §1.1`「不需新增抽象层」一致);§9.3 的**钩子签名扩展**不产生抽象层 ⇒ 二者目标一致但字面冲突,**以 §9.3 为准 + 登记偏离**。
- ⇒ 该偏离进 §7 影响清单(`06 号 §11.2` 加注),而非「两条都满足」的文字游戏。

### D-4 —— `CohIfBundle` 路径:**避开** `coherence_bundles_tlm.hh`

**裁决**:`include/bundles/cep_coherence_bundles_tlm.hh`。
**理由**:`ADR-CEP-07A:49` 明确「**不创建** `include/bundles/coherence_bundles_tlm.hh`」⇒ 该路径不可用。命名风格与 `ADR-CEP-03:225` 的 `collective_bundles_tlm.hh` 同构。

### D-5 —— 排期位置:**插在 #6 与 #7 之间**(per 用户裁决)

| # | Change | 交付语义 |
|---|---|---|
| **6** | `cpptlm-cep-moesi-gpu-adapter` | 首个真实协议语义 + **特化字典**(6→3)+ `perm` 形态声明 |
| **7(新)** | `cpptlm-cep-cohif-carrier` | **载体**:CohIf Bundle + PE 的 ChStream 端口面 + `ProtocolBridge` 特化桥 |
| **8(原 7)** | `cpptlm-cep-chi-rnf-subset` | 第二个实例,**首次在真实载体上验证** |
| **9(原 8)** | `cpptlm-cep-mesif-adapter` | 保持 R-1 阻塞登记 |
| **10 ~ 14(原 9 ~ 13)** | `cxl-nonsymmetric` / `tilelink` / `cxl-dcoh-symmetric` / `nvlink-sharp` / `ualink-inc` | 顺延 |

**全局算术**:13 → **14** change / 14 季度;终态 2029-Q4 → **2030-Q1**(具体季度值以 `obj §4.0` 为唯一真源,本文档不复制)。

**为什么必须插在 #6 之后、#7 之前**:#6 的装配验证当前**只能是逻辑级**(`stub_cache_core.hh:42` 明写 `net_side` 由 **mock ProtocolEngine** 驱动);若载体排在 #7 之后 ⇒ **#7 继承无载体装配 ⇒ 载体债复制一遍**。

### D-6 —— `06 号 §11.2/§11.3` 的时间线**转隶 CEP**

**事实(v2 已核实,per critic A5)**:`06 号 §11.2` 承诺的 3 个测试(`test_bundle_classification_contract.cc` / `test_simulation_mode_schema.cc` / `test_seam_documentation_locator.cc`)与 `include/core/coherence_bridge.hh` / `src/core/coherence_bridge.cc` **全仓均不存在**;`openspec/changes/` 与 `archive/` **无**对应 change ⇒ **P1/P2 从未实施**。

**裁决**:`06 号 §11.2/§11.3` 的时间线作废,改由 CEP 排期承接;载体落 **CEP 队列 #7(新)**。`06 号` 加 `## Status Update` 登记时间线转隶。

### D-7 —— `06 号` 两处漂移:值域 + 计数

| 漂移 | 原文 | 实测 | 订正 |
|---|---|---|---|
| `flit_category` 值数 | `§4.3`(`:358`)列 **4** 值(含 `CREDIT_RESPONSE`) | `noc_bundles_tlm.hh:60-62` 只有 **3** 个 `CATEGORY_*`;`CREDIT_RESPONSE` 全仓零命中 | 改 3 值 |
| Bundle 文件数 | `§3.2` 标题(`:266`)「**16** 个 .hh」 | **17** 个 `.hh`(+1 `AGENTS.md`) | 改 17 |

**正面结论**:CEP 的 3 类与网络 Bundle 的 3 类**精确 1:1** ⇒ `06 号 §4.3`「网络 Bundle 不需要新加字段」**成立**;与 `§15.2` 的「`vc_id` 4-bucket(4 vnet)」**不冲突**(后者是 **VC 数**,不是 `flit_category` 值数)。

### D-8 —— 队列 #6 的偏离登记**必须补**

`proposal.md` 把「header-only / 不进 `ModuleFactory`」当**事实**陈述,但它是对 `ADR-CEP-07 决策 3` + `07 号 §6.3` 的**偏离** ⇒ 须照 `ADR-CEP-03:219-225` 格式登记,并指向本队列(具体路径)。

### D-9 —— **不动 `StubCacheCore`**(v2 新增,per critic S7)

**裁决**:本队列**不**给 `StubCacheCore` 加 ChStream 端口面。

**理由(实测三叉矛盾)**:
1. `openspec/specs/cep-stub-cache-core/spec.md` 在 **FROZEN_GLOBS** 内(`cep_freeze_check.sh:69`);
2. 该 spec 写「`net_side` SHALL 由 **mock ProtocolEngine** 驱动(本 change **不**引入真实协议引擎)」+ 三个流名「SHALL **不**出现作为 stub 的**对外接口**」;
3. `stub_cache_core.hh:39-40` 写「**不**经 `ModuleFactory` 注册(本类是 **testbench 定位** 的组件)」;
4. 而 `ADR-CEP-07:67` / `07 号 §6.3` 说它**应该** `registerObject` + `ChStreamAdapterFactory::create`。

⇒ **四条互斥**。解它需要 MODIFIES 一份**冻结 main spec**(两步授权 + 台账 Entry),**不应塞进载体队列**。

**延期路径(具体,不写「以后再说」)**:Stub↔PE 那条腿的 ChStream 化归入 **`cep-stub-cache-core` main spec 的下一次 MODIFIES change**(暂名 `cpptlm-cep-stub-cache-chstream-surface`),排在载体队列之后;其前置 = 本队列交付 `CohIfBundle`(提供 stub 侧需要的 5 流类型)。

---

## 6. 新队列设计(`cpptlm-cep-cohif-carrier`)

### 6.1 交付面(3 项 + 1 显式延期)

| # | 交付物 | 承接来源 | 关键约束 |
|---|---|---|---|
| **1** | `include/bundles/cep_coherence_bundles_tlm.hh` —— `bundles::CohIfBundle` | `06-bis §5.2/§5.3/§5.4` 字段基线 | 只覆盖 **5 条 RN-side 流**;`bundle_base` + `ch_uint`/`ch_bool` + `BUNDLE_FIELD_WIDTH` 锁 + `is_trivially_copyable`;**无网络层字段**(D-1) |
| **2** | `include/tlm/cep/protocol_engine_base.hh` —— `ChStreamModuleBase` 派生 + `InputStreamAdapter<CohIfBundle>`/`OutputStreamAdapter<CohIfBundle>` | `ADR-CEP-07:64` + `router_tlm.hh:136-142` 范式 | **JSON 装配**(D-2);持 `ProtocolBridgeEr*` |
| **3** | `include/framework/protocol_bridge.hh` —— `ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>` + `AbstractProtocolBridge` | `06 号 §4.4`(**首次落地**) | 双层:类型擦除基类 + 模板子类;`Ordered` 单 bool |
| **4** | `chstream_register.hh` 新增 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` + `module_factory.hh:313-315` 钩子扩展 | `06 号 §9.3` + `chstream_register.hh:175` 对称 | 宏内 `static_assert(n > 1)`;钩子**扩展**不新建类(D-3) |

> **显式延期(D-9)**:`StubCacheCore` 的 ChStream 端口面 → `cpptlm-cep-stub-cache-chstream-surface`(所有权明确,非「以后再说」)。

### 6.2 端到端数据流(v2:标方向 + 标 5/10 分界)

```
   [业务侧 · 测试夹具装配]
Traffic Generator ──(cpu_side 功能调用,非 ChStream)──> Stub Cache Core
                                                            │
                        CohIfBundle · 5 流 (request / completion /
                        snoop / snoop_resp / writeback)     │ ★ 交付 1
                        ⇒ forward_data / back_invalidate / address_resolve /
                          barrier / snoop_completion_ack **不在该 Bundle 域内**
                                                            ▼
                        ProtocolEngine (MoesiGpuAdapter / ChiRnfAdapter / ...)
                          6 态×6 事件 · 6→4 perm 投影 · 6→3 消息类映射
                          另 5 流在此终结或对接 Directory & Ordering Plane
                                                            │
                        ProtocolBridge<CohIfBundle, NoCFlitBundle>  ★ 交付 3
                        (L5↔L4 seam 实例;换 Axi4Bundle = 换一种网络)
                                                            ▼
   [互联网络 · ChStream 构建]  RouterTLM / CrossbarTLM
                        BidirectionalPortAdapter<NoCFlitBundle, N>  ★ 交付 4 装配
                        NoCFlitBundle 16 字段(已有,编译期锁)
                                                            ▼
                        VramControllerTLM / 对端 adapter
```

**方向语义(v2 新增,per critic S3)**:

| 流 | 方向 | 落点 |
|---|---|---|
| `request` / `writeback` | node → center | PE 出向(经桥 → 网络 REQUEST) |
| `completion` / `snoop` / `snoop_resp` | center → node / node → center | PE 双向 |
| `forward_data` | node ↔ node(经中心调度) | **PE 内部**,不经 `CohIfBundle` |
| `back_invalidate` | center → node | **PE 内部**,不经 `CohIfBundle` |
| `address_resolve` | node ↔ center | **PE 内部终结**(`ADR-CEP-02` CEN-Ib8) |
| `barrier` | 双向 | **PE 内部** + Directory/Collective Plane |
| `snoop_completion_ack` | node → center | **PE 内部**,配对 `snoop_resp` |

**VC / 死锁纪律的挂载点(v2 新增)**:`cep-channel-dependency-gate`(冻结 main spec)给出的通道依赖图纪律挂在 **`ProtocolEngine` 的 `vc_binding` 声明**(`ADR-CEP-12` 的 13 键之一)⇒ 本队列**不新立** VC 纪律,只要求 PE 的 `protocol_id_`/`bridge_id_` 与既有 `vc_binding` 对齐;6→3 映射表的 `CATEGORY_*` 即 VC 分桶的输入(REQ→VC0 / RSP→VC1 / CREDIT→VC2,per `ADR-CEP-09`)。

### 6.3 队列边界(明确不做)

| 不做 | 理由 |
|---|---|
| 协议语义建模(6×6 表内容) | 归 ArchForge(`06-bis` 顶部处置表) |
| `moesi_gpu` 具体实现 | 队列 #6 |
| `chi-rnf` / `mesif` 等实例 | 队列 #8+ |
| 跨域组合(多协议混合) | `ADR-CEP-13`,人工声明 + 实验证伪 |
| per-packet `tlm_extension` | `06 号 §4.5` = P3+ |
| `coherence_bundles_tlm.hh` 路径 | `ADR-CEP-07A:49`(D-4) |
| **`StubCacheCore` 的 ChStream 端口面** | **D-9** —— 需 MODIFIES 冻结 main spec,另立 change |
| 新增 StreamAdapter 子类 / factory 抽象层 | `01 号 §1.1`「不需新增抽象层」(D-2/D-3) |

### 6.4 判据面(6 条,全部配**可执行**反向 fixture)

| ID | 判据 | 反向 fixture(**可执行形态**) |
|---|---|---|
| **CC-01** | `CohIfBundle` **只含 5 流**且**不含**网络层字段:`flow_id` 域枚举 == {request, completion, snoop, snoop_resp, writeback};字段集无 `src_node`/`dst_node`/`flit_category`/`vc_id` | ① 加第 6 个 `flow_id` ⇒ 静态扫描**用例红**(参照 `cep-stub-cache-core` spec 的「三流名不出现」先例);② 加网络层字段 ⇒ 同上 |
| **CC-02** | `BUNDLE_FIELD_WIDTH` 锁 + `is_trivially_copyable` **生效** | **test TU 内** `static_assert(sizeof(CohIfBundle) == 期望)` + `static_assert(std::is_trivially_copyable_v<CohIfBundle>)`;外加 `try_compile` 一个「字段宽度篡改为 +1」的**毒化副本** ⇒ 编译失败即通过(per critic S9:`BUNDLE_FIELD_WIDTH` 是编译期锁,**不能**只写「编译红」当 ctest 判据) |
| **CC-03** | `ProtocolBridge` 的 `pack`/`unpack` 对 `CohIfBundle ↔ NoCFlitBundle` **双向可逆**(round-trip 恒等,逐字段) | 篡改任一字段映射(如把 `perm` 映射到 `flit_category`)⇒ 用例红 |
| **CC-04** | JSON 装配后,一次 CohIf 事务可**穿过** adapter 抵达对端;**N ≥ 2** | ① 断链 ⇒ 红;② **N=1** ⇒ 宏 `static_assert` 编译红(防 `bind_ports` 空 shim 静默不绑定,`bidirectional_port_adapter.hh:141`) |
| **CC-05** | `register_protocol_bridge` 从**死钩子**变**活钩子**:注册后 `get_bridges()` 可读且被 ModuleFactory **消费**(非只写) | 只注册不消费 ⇒ 红(须同时交付消费方,否则是新的死钩子) |
| **CC-06** | 6→3 映射表产出的 `CATEGORY_*` **全部落在**网络 Bundle 的 3 个常量内 | **引用队列 #6 的 AC-04**(`design.md:178`「目标写成第 4 类 ⇒ 红」),**不重立**(per critic S10:避免两份判据漂移) |

> **纪律**(队列 #4 `Entry #65` / 队列 #5 `Entry #68`):**新断言必须有反向 fixture**;**纯负向断言必须有正控**;**判据实现退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**。

> **判据无法在仓内验证的降级登记**:「单 `CohIfBundle` 承载 5 流的协议死锁自由度」在**无实现**时不可验证 ⇒ 登记为「残余风险 + DRI 书面接受」(比照 `obj §9.3` R10),**不**写成 CC 判据。

---

## 7. 对既有真源的影响清单(v2 补全,per critic S5/S7)

| 真源 | 动作 | 门槛 | 归属 |
|---|---|---|---|
| **`ADR-CEP-00 §2 决策 3`**(`:53-56` 逐名枚举 Stage 1 的 9 change + 总数 13) | 插新队列 ⇒ Stage 1 → 10 / 总数 → 14 | **冻结面** ⇒ 两步授权 | planner/builder |
| **`07 号 §8.1`**(`:543-556` Stage 表:Stage 1 = **7** / 合计 **11**) | ⚠️ **已是旧数**(连 13 都没跟上)⇒ 修订为插入后口径 | **冻结面** ⇒ 两步授权 | planner/builder |
| `docs/architecture/06-inter-module-network.md §3.3` | 加第三类「跨模块共享的业务 Bundle」(D-1) | 真源 1 ⇒ **ADR-19 先签发**,再改 | architect |
| `docs/architecture/06-inter-module-network.md §4.3 / §3.2` | `flit_category` 4→3 值;Bundle 文件数 16→17(D-7) | 同上 | architect |
| `docs/architecture/06-inter-module-network.md §11.2 / §11.3` | Status Update:时间线转隶 CEP(D-6)+ P3 子项的拒绝条款**显式偏离登记**(D-3) | 同上 | architect |
| `docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md` | Status Update:第 3 项替代物偏离 + 新路径(D-4) | **冻结面** | planner/builder |
| `docs/adr/ADR-CEP-16-06-evolution-route-mapping.md` | Status Update:**C2 第 2 处悬空引用**(`:45`)+ 澄清 `:46/:86` 与 D-2(JSON 链)不冲突 | **冻结面** | planner/builder |
| `docs/adr/ADR-CEP-07-simulation-and-integration-semantics.md` | Status Update:ProtocolEngine 基类落地排期 | **冻结面** | planner/builder |
| `docs/adr/ADR-CEP-15-cachetlm-alignment.md` | Status Update:`§1.2` 的「基类 = `ChStreamModuleBase`」与 `stub_cache_core.hh:154` 不符 | **冻结面** | planner/builder |
| **`openspec/specs/cep-stub-cache-core/spec.md`** | **本队列不动**(D-9);延期项归 `cpptlm-cep-stub-cache-chstream-surface` | **冻结面** | 后续 change |
| `docs/adr/ADR-CEP-NUMBER-MAP.md` | ADR 清单 21 → 22 + KPI 预算 13 → 14 | **冻结面** | planner/builder |
| **新** `docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md` | 承载 D-1 ~ D-9 | **签发前不得落 `docs/adr/`**(会触发 M1 drift ⇒ 守门红) | architect 起草 → DRI 签发 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` §4.0 / §4.2 / §4.6 / §11 | 队列插入(13 → 14)+ 台账 Entry | 五处同改 | **planner** |

---

## 8. 待 DRI 裁决

| # | 裁决点 | 推荐 |
|---|---|---|
| **1** | 新队列名 | `cpptlm-cep-cohif-carrier` |
| **2** | 全局算术 | 13 → **14** change / 14 季度;终态 2030-Q1 |
| **3** | 新 ADR 编号 | `ADR-CEP-19`(现有最大 `ADR-CEP-18`;NUMBER-MAP 21 → 22) |
| **4** | `CohIfBundle` 分类 | 业务语义 + 网络级纪律(D-1) |
| **5** | 估时 | 6 周(3 项交付 + 1 个宏;D-9 已延期,较 v1 减 2 周) |
| **6** | D-9 延期项的 owner | `cpptlm-cep-stub-cache-chstream-surface`(新,排载体队列之后) |

---

## 9. 签发前置条件(**v2 补第 0 项**,per critic S8)

| # | 前置 | 说明 |
|---|---|---|
| **0** | **`obj §11` 追加本轮 architect 重审的台账 Entry** | v1 只写「签发前需 Entry」却**没给本轮自己登记** —— 与 D-8 批队列 #6 的标准不一致。本轮是「新立队列 + 9 条裁决」的同级决策事件 ⇒ **必须先登记** |
| 1 | 台账 Entry 覆盖范围 | 须同时覆盖:本轮重审 · `ADR-CEP-16` Status Update · `ADR-CEP-00` / `07 号 §8.1` 计数修订 · `ADR-CEP-07A/07/15` Status Update · `NUMBER-MAP`(21→22 / 13→14) |
| 2 | 两步授权 | 先 commit 产物 → `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` → 再 commit 清单 |
| 3 | 顺序依赖 | **先签发 `ADR-CEP-19` → 再改 `06 号`**(D-1;否则是「用未签发 ADR 为改真源背书」) |
| 4 | 真源 1 门槛 | `06 号` 的 3 处修订走 Oracle 评审 or ADR 触发 |

---

## 10. 关联

- 队列 #6 planner 四件套:`openspec/changes/cpptlm-cep-moesi-gpu-adapter/`
- 排期真源:`.rddf/roadmap/objectives/obj-cep-platform.md`(§4.0 / §11)
- 架构基线:`01 号` · `06 号` · `06-bis` · `07 号`
- 契约:`ADR-CEP-{00,02,03,05,07,07A,12,13,15,16,17,18}-*.md`
- 偏离登记格式先例:`ADR-CEP-03` Status Update 2026-10-26
- **critic 第 1 轮评审**(REJECT:2 BLOCKER + 10 SHOULD_FIX + 7 NIT)⇒ v2 全部落地
