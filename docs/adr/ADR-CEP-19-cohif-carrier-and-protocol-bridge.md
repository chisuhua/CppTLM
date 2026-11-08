# ADR-CEP-19: CohIf 载体(业务 Bundle + ChStream 端口面 + ProtocolBridge 特化)

> **状态**: ✅ **已签发**(v1.0,2026-10-27;per 台账 `obj-cep-platform §11` **Entry #74** 的冻结面授权)
> **日期**: 2026-10-27
> **作者**: CppTLM build agent(architect mode)
> **作用域**: **CohIf 的承载形态** —— 业务 Bundle(`CohIfBundle`)/ ProtocolEngine 的 ChStream 端口面 / `ProtocolBridge` 特化桥 / 装配路径(JSON 链)/ 与既有冻结 ADR 的边界
> **关联真源**:
> - 真源 1:`docs/architecture/01-hybrid-architecture-v2.1.md` v2.1.10 §1.1(L1~L6 分层 + 原则 7「模块设计只依赖 L3+L4」)+ §1.2(Bundle 命名重塑的触发条件)+ §4.2(纯 ch_stream 内部 + StreamAdapter 暴露)
> - 真源 1:`docs/architecture/06-inter-module-network.md` v0.4 §3(Bundle 二分类)+ §4.1(seam 在 L5↔L4 之间)+ §4.4(`ProtocolBridge` 双层类形态)+ §4.5(per-bridge 元数据)+ §9.3(`register_protocol_bridge` 死钩子)+ §6.1(容器端口契约 = setter 注入)
> - 真源 1:`docs/architecture/07-coherence-exploration-platform.md` v0.1 §2.1(6 层 CEP 架构)+ §3.1(canonical 10 流)+ §3.2(Stub 只感知 5 条流)+ §5.4(Bundle 约束)+ §6.3(ModuleFactory 集成 seam)
> - 真源 1:`docs/architecture/06-bis-coherence-as-network-extension.md` v0.3 §3.2(8 端口装配实证)+ §3.4(候选 C programmatic)+ §5.2/§5.3/§5.4(Bundle 字段基线)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## 5. 关联 ADR / 后续」节
>
> **一手锚点**(全部实测,逐 `file:line` 核实):
> - `include/tlm/cep/stub_cache_core.hh:154` — `class StubCacheCore {`(**非** `ChStreamModuleBase`)
> - `docs/adr/ADR-CEP-07-simulation-and-integration-semantics.md:64` / `:67` — ProtocolEngine / StubCacheCore **应当**是 `ChStreamModuleBase`
> - `docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md:49` 与 `docs/adr/ADR-CEP-16-06-evolution-route-mapping.md:45` — 「CEP 用 **canonical 10 流的 Bundle**」**两处**悬空引用
> - `docs/adr/ADR-CEP-16-06-evolution-route-mapping.md:46` 与 `:86` — **不**实现 programmatic bridge 的 C++ 路径
> - `include/framework/chstream_adapter_factory.hh:103`(`registerBidirectionalPortAdapter`)/ `:108`(`port_count_ = N`)/ `:59-61`(`isMultiPort` = `port_count > 1`)
> - `src/core/module_factory.cc:668-669`(判定)· `:743-750`(`is_multi` 分支 = **泛型虚分派** `bind_port_pair`,**无硬编码 cast**;硬编码 cast 只在 `is_dual` 分支 `:727-741`)
> - `include/framework/bidirectional_port_adapter.hh:128`(`bind_port_pair` override)· `:141`(`bind_ports` = **空操作 shim** ⇒ `N=1` 静默不绑定)
> - `include/chstream_register.hh:63`(`registerObject<tlm::NICTLM>`)/ `:119`(RouterTLM 的 Bidir 注册)/ `:175`(`REGISTER_CHSTREAM_DUAL` = **零调用点的死宏**,宏体只注册 adapter)
> - `include/core/module_factory.hh:313-315` — `register_protocol_bridge` = **write-only 死钩子**(`get_bridges()` 在 `private:` 段 `:326`)
> - `include/tlm/router_tlm.hh:136-142` — `InputStreamAdapter<NoCFlitBundle>` + `OutputStreamAdapter<NoCFlitBundle>` + `BidirectionalPortAdapter<RouterTLM, NoCFlitBundle, NUM_PORTS>`(范式先例)
> - `include/bundles/noc_bundles_tlm.hh:60-62` — `CATEGORY_REQUEST=0` / `CATEGORY_RESPONSE=1` / `CATEGORY_CREDIT=2`(**3 类**;`CREDIT_RESPONSE` 全仓零命中)
> - `scripts/test/cep_freeze_check.sh:70` — `openspec/specs/cep-stub-cache-core/spec.md` 在 M1 冻结集内
>
> **⚠️ 签发前置(已由 Entry #74 满足)**:① `obj §11` 台账 Entry #74 已登记本 ADR + 7 份冻结成员的 Status Update 授权;② 两步授权:先 commit 产物 → `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #74"` → 再 commit 清单;③ `ADR-CEP-NUMBER-MAP` 清单 21 → 22 + KPI 预算 13 → 14。
>
> **⚠️ 顺序依赖**:`docs/architecture/06-inter-module-network.md` 的 3 处修订(决策 3 的落点)**必须在本 ADR 签发之后** —— 否则构成「用未签发 ADR 草案为改真源 1 背书」的循环论证(见决策 3)。

## 1. 背景

CEP 6 层架构里,**Coherence Interface** 层(canonical 10 流,`ADR-CEP-02`)之下是 **Protocol Engine 插件层**(协议特化),之上是 **Stub Cache Core**。

**现状实测(2026-10-26)**:

```bash
grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/   # → 空
find include/tlm/cep -name "*.hh" | wc -l                               # → 7
```

⇒ 队列 #1 ~ #5 已归档产出**全部是纯 C++ 逻辑 + JSON 声明源,零 ChStream 承载体**。

**但契约要求有**:`ADR-CEP-07` 决策 3 规定 **ProtocolEngine 派生类 = `ChStreamModuleBase` + `ModuleFactory::registerObject<T>`**(示例列的就是 `ChiRnfAdapter` / `CxlCacheAdapter` / …);`ADR-CEP-07:67` 规定 **StubCacheCore = `ChStreamModuleBase`**。

**同时,「canonical 10 流的 Bundle」在**两处** ADR 里都是悬空引用**:
- `ADR-CEP-07A:49`:「CEP 用 **canonical 10 流的 Bundle**(per `ADR-CEP-02` / `07 号 §5.4`)」
- `ADR-CEP-16:45`(P2.0 行):同一表述

经核实:`ADR-CEP-02` 全文 `Bundle` **零命中**;`07 号 §5.4` 业务 Bundle 列表(7 项)**无它**;`include/bundles/` 17 个 `.hh` grep `coherence` **零命中**。

**四份既有设计齐全、0 个队列认领**:
① `06-bis §5.2~§5.4` 的 coherence Bundle 字段定义 · ② `06 号 §4.4` 的 `ProtocolBridge` 双层类形态 · ③ `06 号 §9.3` + `module_factory.hh:313-315` 的 `register_protocol_bridge`(**write-only 死钩子**;`get_bridges()` 在 `private:` 段 `:326`) · ④ 装配路径: `06-bis §3.4` 候选 C(programmatic)被 `ADR-CEP-16:46/:86` **反向排除** ⇒ 净结果 = 无路径。

---

## 2. 决策

### 决策 1 —— 新立 **CohIf 载体队列**,排在 `moesi_gpu` 之后、`chi-rnf-subset` 之前

队列(暂名 `cpptlm-cep-cohif-carrier`)交付 **3 项 + 1 项显式延期**:

| # | 交付物 | 承接来源 |
|---|---|---|
| 1 | `include/bundles/cep_coherence_bundles_tlm.hh` —— `bundles::CohIfBundle` | `06-bis §5.2~§5.4` 字段基线 |
| 2 | `include/tlm/cep/protocol_engine_base.hh` —— `ChStreamModuleBase` 派生 + ChStream 端口面 | `ADR-CEP-07` 决策 3 + `router_tlm.hh:136-142` 范式 |
| 3 | `include/framework/protocol_bridge.hh` —— `ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>` | `06 号 §4.4`(首次落地) |
| 4 | `chstream_register.hh` 新增 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)`(**`registerObject` + `registerBidirectionalPortAdapter` 两件都做**)+ `module_factory.hh:313-315` 钩子扩展 | `06 号 §9.3` + `chstream_register.hh:63` + `:119` |
| **延期** | `StubCacheCore` 的 ChStream 端口面 ⇒ `cpptlm-cep-stub-cache-chstream-surface` | 决策 9 |

**排序理由**:`moesi_gpu` 交付首个真实协议语义 + **特化字典**(6→3);载体队列**消费**该字典;`chi-rnf-subset` 则**首次在真实载体上验证**。若载体排在 `chi-rnf` 之后 ⇒ `chi-rnf` 继承**逻辑级**装配验证(`stub_cache_core.hh:42` 明写 `net_side` 由 mock ProtocolEngine 驱动)⇒ 载体债复制一遍。

### 决策 2 —— `CohIfBundle` 是**单一类型**,**只覆盖 5 条 RN-side 流**

**关键界定**:canonical **10 流**中,**只有 5 条跨 Stub↔PE 边界**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`);另 5 条在 ProtocolEngine 内部终结或对接 Directory & Ordering Plane(`07 号 §3.2`、`ADR-CEP-05` 折叠映射表、`ADR-CEP-02` CEN-Ib8)。

⇒ 本 ADR 的 `CohIfBundle` **只表达这 5 流**;`forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack` **不在其 `flow_id` 域内** ⇒ 「对 stub 不可见」是**类型不可表达**的(不是运行期纪律)。

**理由(消解 `06-bis §3.2` 死结)**:`06-bis` 实证「现有 Step 7a 装配路径不支撑 4 类 Bundle 4 端口」。Bundle 类型数收敛为 **1** ⇒ `BidirectionalPortAdapter<M, CohIfBundle, N>`(仓内已存在,`chstream_adapter_factory.hh:103`)直接够用 ⇒ **框架层零新增 StreamAdapter 子类**。

**范式已验证**:`router_tlm.hh:136-142` 用 `InputStreamAdapter<NoCFlitBundle>` + `OutputStreamAdapter<NoCFlitBundle>` + `BidirectionalPortAdapter<RouterTLM, NoCFlitBundle, NUM_PORTS>`。

### 决策 3 —— `CohIfBundle` 归**业务语义**类,但按**网络级纪律**演进

**理由**:`06 号 §3.1` 的两条判定规则对 CohIf 给出相反答案 —— **字段维度**(无 `src_node`/`dst_node`/`flit_category`/`vc_id` ⇒ 业务)vs **owner 维度**(跨模块共享 ⇒ 网络)。

**裁决**:**分类 = 业务语义**;**演进纪律 = 网络级**(`BUNDLE_FIELD_WIDTH` 锁 + 扩展走 OpenSpec change)。

**落点**:`06 号 §3.3` 首行「模块 owner 可改?」**加第三类「跨模块共享的业务 Bundle」**(不是「补一列」—— `§3.1` 已有「演进粒度」行,真正绑纪律的是 `§3.3`)。

**顺序依赖**:`06 号` 虽非 M1 冻结集,但属真源 1 ⇒ 变更需 Oracle 评审 or ADR 触发 ⇒ **本 ADR 必须先签发**。

### 决策 4 —— 特化桥 = `ProtocolBridge` 双层 + 钩子扩展(**显式偏离** `06 号 §11.2` 的 P3 子项)

**裁决**:采纳 `06 号 §4.4` 的 `ProtocolBridgeEr` + `ProtocolBridge<InB, OutB, Ordered>`;扩展 `module_factory.hh:313-315` 为 `(bridge_name, factory_lambda)`。

**与 `06 号` 的两条要求**:
- **遵从** `§9.3`(「扩展既有入口,不重发明」)
- **显式偏离** `§11.2` 的 P3 子项(「拒绝 ProtocolBridgeFactory 模式」)—— **偏离理据**:该条拒绝的是**独立 factory 抽象层**(与 `01 号 §1.1`「不需新增抽象层」一致);`§9.3` 的**钩子签名扩展**不产生抽象层。二者目标一致但字面冲突 ⇒ 以 `§9.3` 为准 + **登记偏离**(不写成「两条都满足」)。

### 决策 5 —— 装配走 **JSON 链**,**不**走 programmatic bridge

**裁决**:
1. 注册:`REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` —— **宏体做两件事**:① `ModuleFactory::registerObject<mod_type>("#mod_type")`(对象注册是 adapter 注册的前提,缺它 ModuleFactory 实例化不了 PE)② `ChStreamAdapterFactory::get().registerBidirectionalPortAdapter<mod_type, bundle_t, n>("#mod_type")`;宏内 `static_assert(n > 1)`。
   - **不照抄 `REGISTER_CHSTREAM_DUAL`(`:175`)**:实测该宏**零调用点**且**只注册 adapter、不含 `registerObject`**(NICTLM 的真实落点是内联在 `REGISTER_CHSTREAM` 体内 —— `:63` + `:121-122`)⇒「与 DUAL 对称」是语法真、语义虚;
2. 装配:走既有 JSON 端口路径(`registerBidirectionalPortAdapter` → `port_count_` → `isMultiPort` → `module_factory.cc` 的 `is_multi` 分支 → **虚分派** `bind_port_pair`);
3. **不做** programmatic 端口注入。

**理由(实测)**:
- **冻结契约排除**:`ADR-CEP-16:46`「**不做** programmatic bridge 的 C++ 实施」+ `:86`「**不**实现 programmatic bridge 的 C++ 路径」;
- **零先例**:`grep -rn "new cpptlm::ChStreamInitiatorPort\|new cpptlm::ChStreamTargetPort" src/ include/ | grep -v module_factory` → **空**(ChStream 端口只在 `module_factory.cc` Step 7 内 new);
- **JSON 链全程实测通**:`chstream_adapter_factory.hh:103` → `:108` `port_count_=N` → `:61-63` `isMultiPort` → `module_factory.cc:668-669` → `:743-750` `bind_port_pair`(泛型虚分派,**无硬编码 cast**;硬编码 cast 只在 `is_dual` 分支 `:727-741`);
- **`static_assert(n > 1)` 的必要性**:`isMultiPort` 要求 `port_count > 1`;`N=1` 会落 single 分支调 `bind_ports`,而 `BidirectionalPortAdapter::bind_ports` 是**空操作 shim**(`bidirectional_port_adapter.hh:141`)⇒ 静默不绑定。

### 决策 6 —— Bundle 路径用 `include/bundles/cep_coherence_bundles_tlm.hh`

**理由**:`ADR-CEP-07A:49` 明确「**不创建** `include/bundles/coherence_bundles_tlm.hh`」⇒ 该路径不可用(ADR 不可变,只能绕)。命名风格与 `ADR-CEP-03:225` 的 `collective_bundles_tlm.hh` 同构。

### 决策 7 —— `06 号 §11.2/§11.3` 的时间线转隶 CEP

**理由(实测)**:`06 号 §11.2` 承诺的 3 个测试(`test_bundle_classification_contract.cc` / `test_simulation_mode_schema.cc` / `test_seam_documentation_locator.cc`)与 `include/core/coherence_bridge.hh` / `src/core/coherence_bridge.cc` **全仓均不存在**;`openspec/changes/` 与 `archive/` **无**对应 change ⇒ P1/P2 从未实施 ⇒ 由 CEP 排期承接。

### 决策 8 —— 本 ADR **不**引入新抽象层

**不引入**:StreamAdapter 新子类(复用 `BidirectionalPortAdapter`)· factory 抽象层(只扩钩子签名)· 协议语义基类(不做 `CoherenceProtocol` 式单一 C++ 类,per `06-bis` 顶部处置表)· per-packet `tlm_extension`(per `06 号 §4.5` = 后续阶段)。

**只做**:1 个业务 Bundle 类型(5 流)+ 1 个 `ChStreamModuleBase` 基类 + 1 个 seam 桥 + 1 个注册宏 + 1 处死钩子激活。

### 决策 9 —— **不动 `StubCacheCore`**(显式延期)

**裁决**:本队列**不**给 `StubCacheCore` 加 ChStream 端口面;该腿归 `cpptlm-cep-stub-cache-chstream-surface`(新,排本队列之后)。

**理由(四条互斥,实测)**:
1. `openspec/specs/cep-stub-cache-core/spec.md` 在 **FROZEN_GLOBS** 内(`cep_freeze_check.sh:70`;`:69` 是 `cep-vc-mapping-contract`);
2. 该 spec:「`net_side` SHALL 由 **mock ProtocolEngine** 驱动(本 change **不**引入真实协议引擎)」+ 三个流名「SHALL **不**出现作为 stub 的**对外接口**」;
3. `stub_cache_core.hh:39-40`:「**不**经 `ModuleFactory` 注册(本类是 **testbench 定位** 的组件)」;
4. `ADR-CEP-07:67` / `07 号 §6.3`:它**应该** `registerObject` + `ChStreamAdapterFactory::create`。

⇒ 解它需 MODIFIES 一份**冻结 main spec**(两步授权 + 台账 Entry),**不应塞进载体队列**。延期项的**前置** = 本队列交付 `CohIfBundle`(提供 stub 侧需要的 5 流类型)。

---

## 3. 后果

### 3.1 正面

- **`01 号 §1.1` 的 L5↔L4 seam 首次有实现** —— `ProtocolBridge<CohIfBundle, NoCFlitBundle>`;换 `Axi4Bundle` 即换一种通信网络 ⇒ **「ChStream 构建的互联网络可特化到具体通信网络」闭合**;
- **`01 号 §4.2`「模块代码完全不知道 Port 的存在」在 CEP 落地** —— ProtocolEngine 内部只见 `Input/OutputStreamAdapter<CohIfBundle>`;
- **队列 #6 的 6→3 映射表获得消费者** —— 它是「CohIf 语义 → 网络 Bundle `flit_category`」的特化字典(目标常量**就定义在** `include/bundles/noc_bundles_tlm.hh:60-62`);
- **消除「声明有、实现无」的悬空** —— `ADR-CEP-07A:49` 与 `ADR-CEP-16:45` 的替代物从「悬空引用」变为「实体路径」;
- **「对 stub 不可见」升级为类型保证** —— 5 流界定使 `barrier` / `forward_data` / `back_invalidate` 在 `CohIfBundle` 的 `flow_id` 域外。

### 3.2 代价

- **框架层 3 处**:`chstream_register.hh` 新增 1 个宏 · `module_factory.hh:313-315` 钩子签名扩展 · `protocol_bridge.hh` 新文件 ⇒ 触 `module_factory.cc` 核心路径 ⇒ 需 6 标签回归;
- **per-bridge 元数据只做静态**(`protocol_id_` / `bridge_id_`,per `06 号 §4.5`)⇒ per-packet 协议 flavor 仍推迟;
- **全局算术**:13 → **14** change / 14 季度 ⇒ 须修订 `ADR-CEP-00 §2 决策 3` + `07 号 §8.1` + `ADR-CEP-NUMBER-MAP` + 五处同改;
- **一处延期**:Stub↔PE 腿(决策 9)⇒ 「三明治」只闭合 PE↔网络 两段中的后一段 + PE 的输入侧。

### 3.3 风险

| 风险 | 缓解 |
|---|---|
| `CohIfBundle` 字段集按 5 流定,若将来某流的字段需求扩张 ⇒ 需扩展 Bundle | 按**网络级纪律**走 OpenSpec change(D-1);字段宽度一改即等同新 Bundle(per `06 号 §3.4`) |
| `BUNDLE_FIELD_WIDTH` 编译期锁 ⇒ 扩展成本高 | 与 `06 号 §3.4` 的「类型级切换(旧 Bundle 保留,新类型并存)」一致 |
| `register_protocol_bridge` 从死钩子变活钩子 ⇒ 消费方必须同时交付 | 判据 CC-05 **强制**「注册 + 消费」成对;只扩不消费 = 新的死钩子 |
| `N=1` 落 single 分支 ⇒ `bind_ports` 空 shim 静默不绑定 | 宏内 `static_assert(n > 1)`;判据 CC-04 覆盖 |
| 载体队列只闭合 PE↔网络 ⇒ 「ChStream 承载业务 bundle」的**业务侧**证明仍不完整 | 决策 9 的延期项**有 owner 与前置**,不是「以后再说」;在延期 change 交付前,「业务侧完整闭环」**不作承诺** |
| 六个已归档 change 的产物形态(纯 C++ 逻辑)与新载体并存 ⇒ 两套范式 | 只有 ProtocolEngine 走 ChStream;StubCache / Tracker / Plane 维持现状直到各自的 MODIFIES change |

---

## 4. 关联

- 审查依据(逐 `file:line` 核实):`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3;三轮 critic 评审全落地)
- 队列 #6:`openspec/changes/cpptlm-cep-moesi-gpu-adapter/`
- 架构基线:`01 号` · `06 号` · `06-bis` · `07 号`
- 契约:`ADR-CEP-{00,02,03,05,07,07A,12,13,15,16,17,18}-*.md` · `ADR-CEP-NUMBER-MAP.md`
- 偏离登记格式先例:`ADR-CEP-03` Status Update 2026-10-26
- critic 第 1 轮评审(REJECT:2 B + 10 S + 7 N)⇒ 全部落地
- critic 第 2 轮复审判定 **APPROVE_WITH_CONDITIONS**(无 BLOCKER:2 个 BLOCKER 经逐行读码确认真修、10 S + 7 N 全部落地;新增 3 S + 2 N)⇒ 本轮已修完
- **决策 ↔ 审查报告 D 编号的映射表**:见审查报告 §5 开头(**不是**同号对应:决策 1↔D-5 / 决策 3↔D-1 / 决策 4↔D-3 / 决策 5↔D-2 / 决策 6↔D-4 / 决策 7↔D-6 / 决策 9↔D-9;决策 2 / 8 是横贯约束;D-7 / D-8 是治理动作、不进本 ADR)

---

## 5. 关联 ADR / 后续

| ADR | 关系 |
|-----|------|
| `ADR-CEP-00` | **§2 决策 3** 的排队算术(Stage 1 名单 / 总数 / 串行季度)—— 本 ADR 落地其**队列 #7** 一项 |
| `ADR-CEP-02` | canonical 10 流的**权威定义**;本 ADR 决策 2 据其 §3 + `07 号 §3.2` 把**跨 Stub↔PE 边界**的流收敛为 **5 条** |
| `ADR-CEP-03` | `ProtocolBridge` 未来路径先例(集合通信的 Bundle 化推迟登记)—— 本 ADR 是**主干流**侧的对位物 |
| `ADR-CEP-07` | 决策 3(ModuleFactory 集成 seam)+ 决策 9(ProtocolEngine 派生类 = `ChStreamModuleBase` 的落地排期) |
| `ADR-CEP-07A` | 决策 6 的**路径偏离**(第 3 项的替代物一直是悬空引用) |
| `ADR-CEP-12` | `vc_binding` / `ordering_assumption` 声明 —— 本 ADR 的 PE 端口面消费它们,**不新立** VC 纪律 |
| `ADR-CEP-13` | (b) 成立后的首个可执行载体 —— 本 ADR 是其**声明侧**投影的载体 |
| `ADR-CEP-15` | Stub Cache Core 与 `CacheTLM` 的边界 —— 决策 9 **不改** stub,故本 ADR 与其无冲突 |
| `ADR-CEP-16` | 决策 5(**不**走 programmatic bridge 的 C++ 路径)与 `:45` 的第二处悬空引用 |

**后续**:

| 项 | 归属 | 状态 |
|---|------|------|
| `cpptlm-cep-cohif-carrier` 队列的 builder 阶段(4 项交付面 + 6 条判据 CC-01~CC-06) | 队列 #7 | 🟡 **待启动** |
| `cpptlm-cep-stub-cache-chstream-surface`(Stub↔PE 腿的 ChStream 化;需 MODIFIES 冻结 main spec) | 队列 #7 之后 | ⏸ **已登记,未排期** |
| `06 号` 的 3 处修订(决策 3 的落点 / 漂移订正 / 时间线转隶) | 本 ADR 签发**之后** | ⏳ **待做**(顺序依赖) |
| 队列 #6 补对 `ADR-CEP-07 决策 3` / `07 号 §6.3` 的偏离登记 | 队列 #6 | 🟡 **待做** |

---

## 6. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| **队列位置** | 新立 `cpptlm-cep-cohif-carrier`,插在 `moesi_gpu`(#6)与 `chi-rnf-subset`(#7→#8)之间 | ✅ 已拍板 |
| **Bundle 形态** | **单一类型** `CohIfBundle`,**只覆盖 5 条 RN-side 流**,`flow_id` 区分,无网络层字段 | ✅ 已拍板 |
| **Bundle 分类** | **业务语义** + **网络级演进纪律**(`06 号 §3.3` 加第三类) | ✅ 已拍板 |
| **特化桥** | `ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>`;钩子签名扩展、**不新建 factory 类** | ✅ 已拍板 |
| **装配路径** | **JSON 链**(`REGISTER_CHSTREAM_BIDIRECTIONAL` + `is_multi` 泛型虚分派);**不**走 programmatic | ✅ 已拍板 |
| **Bundle 路径** | `include/bundles/cep_coherence_bundles_tlm.hh`(**避开** `coherence_bundles_tlm.hh`) | ✅ 已拍板 |
| **时间线** | `06 号 §11.2/§11.3` 的 P1/P2 时间线转隶 CEP(实测从未实施) | ✅ 已拍板 |
| **抽象层** | **不**引入新 StreamAdapter 子类 / factory 抽象层 / 协议语义基类 | ✅ 已拍板 |
| **stub 边界** | **不动** `StubCacheCore`;延期到 `cpptlm-cep-stub-cache-chstream-surface` | ✅ 已拍板 |

---

## Status Update 2026-10-28(第二则)—— Phase 3 落地 + **B′ 方案**裁决登记 + 「Bundle 级无损不可达」的架构事实

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记状态变化与实施形态。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #86**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #86)。

> **登记 ① —— 决策 3/4 的落地形态与 3 处**有意的差异**(`06 号 §4.4` 原型 → 落地)**:
> 落地于 `include/framework/protocol_bridge.hh`(`ProtocolBridgeEr` + `ProtocolBridge<InB,OutB,Ordered>` + `AbstractProtocolBridge<FlitT>`)+ `include/tlm/cep/cohif_noc_bridge.hh`(CohIf ↔ NoC 特化桥)。
> 差异 ① **pack/unpack 拆两层**(见登记 ②);差异 ② per-bridge 元数据**要求非零**(让「未分配」可测);差异 ③ 挂载用 `ChStreamModuleBase` 的**可选**虚函数 `attach_protocol_bridge`(默认 false ⇒ 零波及)—— 理由:`ProtocolEngineBase<N>` 的 `N` 是模板参数 ⇒ ModuleFactory 无法 `dynamic_cast` 到具体类型;独立 mixin 与 `SimObject` 无继承关系 ⇒ `dynamic_cast` 编译不过,让 mixin 继承 `SimObject` 则引入菱形继承 ⇒ 更糟。

> **登记 ② —— 「Bundle 级逐字段 round-trip 恒等」在数学上不可达(架构事实,本则核心)**:
> `CohIfBundle.flow_id` 的域是 **5 值**(`request`/`completion`/`snoop`/`snoop_resp`/`writeback`);`NoCFlitBundle.flit_category` 的域是 **3 值**(`REQUEST`/`RESPONSE`/`CREDIT`);而队列 #6 的 6→3 消息类映射表是**刻意多对一**的(CC-06 ② 要求它「非恒等」)⇒ **5 值 → 3 值是多对一有损映射** ⇒ **仅靠网络 flit 的字段空间不可能唯一恢复 `flow_id`**(无论怎么压宽度、怎么按流复用槽位都一样)。
> ⇒ 决策 3 的「`pack`/`unpack` 双向可逆」因此**必须分层**:① **消息内容层**(`pack`/`unpack`)= 共有字段 4 个(`transaction_id`/`address`/`data`/`size`)**双向无损** + 协议语义字段显式登记丢弃;② **端到端层**(`pack_with_packet`/`unpack_with_packet`,原型**已有**这两个虚函数)= `Packet` payload 数据区承载 `InB` 的完整字节 ⇒ **完整 12 字段逐字段恒等**。
> ⇒ **净效果**:「CohIf 业务语义跨网络不透损」这条主张**由端到端层保证**(比原单层判据**更强** —— 覆盖跨真实网络的往返),而非被削弱。
> ⚠️ **本登记不是对决策 3 的「变更」**,而是其**载体分层**的落地口径;决策 3 的「双向可逆」在**端到端层**成立。

> **登记 ③ —— 协议语义的落点选择(2026-10-28 DRI 裁决,B′ 方案)**:
> 备选 5 条:① 新增专用网络 Bundle 类型 ② Packet payload 承载 ③ 收窄 `CohIfBundle` 宽度 ④ 改 `NoCFlitBundle` 加 sideband ⑤ 放宽判据到「可承载子集」。
> **裁决 ②**,理由:① 新增 Bundle 类型 ⇒ 与 `CohIfBundle` 构成「重复类型」观感 + `06 号 §3.2` 归类表需增行;③ 违反 `06 号 §3.4` 第 ③ 项「字段宽度改:**不可**」;④ 破 `NoCFlitBundle` 的 16 字段编译期锁 + 全 consumer 重编;⑤ 把判据阉割成平凡可满足。
> ② 的额外优势:`06 号 §4.4` 原型**已有** `pack_with_packet`/`unpack_with_packet` ⇒ **零新增抽象、零新增类型**;且**不**使用 `tlm_extension` ⇒ **不触** `06 号 §4.5` 把 per-packet 承载标为「P3+」的排除项(与 design `D-γ` 一致)。

> **登记 ④ —— 决策 4 的钩子扩展已实施(死钩子 → 活钩子)**:
> `register_protocol_bridge(bridge_name, factory_lambda)`(per `06 号 §9.3`,**不**引入独立 factory 抽象类 ⇒ §11.2 的 P3 子项偏离依旧成立);`get_bridges()` 转 **public 可读**;消费路径 = `ModuleFactory::instantiateAll` 的 **Step 7.5**(per `06 号 §4.5` 的「per-bridge 配置时机 = ModuleFactory Step 7c(构建期)」)读 `config["cep"]["bridges"]` → 建桥 → 校验元数据 → 注入目标模块。
> per-bridge 元数据口径(per 2026-10-28 DRI 裁决):**`protocol_id_` = 协议来源族的数字编号**(`family` → 编号的唯一真源在 `include/tlm/cep/protocol_family.hh`;`family` 为 `industrial`/`ruby-slicc`/`academic` 三值);**实例级唯一性由 `bridge_id_` 负责**。
> ⚠️ **「只注册不消费」不判装配失败**:多拓扑共用进程级注册表时某个拓扑不引用全部桥是正常的 ⇒ 强制失败会误伤 ⇒ 以**死桥清单**(`unconsumed_bridges()`)暴露,由判据测试断言其非空。

> **登记 ⑤ —— 行号漂移(`module_factory.hh` / `chstream_module.hh`)**:
> Phase 3 给这两个头文件加了内容 ⇒ 既有行号引用语义漂移(符号仍在,行号变了)。**本文件正文的 `module_factory.hh:313-315` / `:326` 引用按「ADR 不可变」不改**;新行号对照表见 `06 号` 文末「附录 · 行号对照表」(2026-10-28):`register_protocol_bridge` 313-315 → **336-343**;`get_bridges` 327(`private:`) → **346**(**`public:`**);`registerObject<T>` 122 → **128**;`registerModule` 的 `static_assert` 135 → **141**;`validate_domain_boundary` 307 → **313**;`getParamRulesRegistry` 81 → **87**;`chstream_module.hh` `num_ports` 54 → **55**、`advance_cycle` 73 → **74**。

## Status Update 2026-10-28(第三则)—— 决策 4 的装配宏**已落地** + `BidirectionalPortAdapter` 泛化登记 + 行号漂移

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记状态变化。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #87**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #87)。

> **登记 ① —— 决策 4 的 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` 已落地**(`include/chstream_register.hh`):
> ① **两件都做** —— `ModuleFactory::registerObject<T>`(缺它 ⇒ ModuleFactory 无法实例化该类型)+ `ChStreamAdapterFactory::registerBidirectionalPortAdapter<T,B,N>`;
> ② **不**照抄 `REGISTER_CHSTREAM_DUAL` 的**半截实现**(它**只**注册 adapter、**不含** `registerObject`,且全仓零调用点 ⇒ 不构成形态基准);
> ③ 宏内 `static_assert(n > 1)`(判据 CC-04 ③ —— N=1 落 `bind_ports` **空操作 shim** ⇒ 静默不绑定)。
> ⇒ **反向 fixture 实测有牙**:N=1 ⇒ 编译失败且失败原因**精确命中**该断言;N=2 ⇒ 编译成功(正控)。已注册 ctest `cep_chstream_bidirectional_macro_reverse_verify`(3/3)。

> **登记 ② —— 前置的必要泛化:`BidirectionalPortAdapter` 与 Bundle 解耦**:
> 该 adapter 原本**硬编码** `NoCFlitBundle` 语义(读 `flit_category` 决定 `PKT_REQ`/`PKT_RESP`;credit 用 `NoCFlitBundle` 构造)⇒ 承载 `CohIfBundle` **编译不过**。⇒ 抽成 `cpptlm::BundleDirectionTraits<BundleT>`(方向判定 + credit 载体),**框架层不认识任何具体 Bundle**;各 Bundle 在**自己的层**提供特化(CEP 侧:`include/tlm/cep/cohif_direction_traits.hh`)。⚠️ **未特化的 BundleT ⇒ 编译期报错**(不静默用错方向)。⇒ **`NoCFlitBundle` 特化逐行为等价**于泛化前的硬编码实现(全量回归 **0 变化**)。
> ⚠️ **登记本 ADR 的决策面影响**:本登记**不改变**决策 4 的任何决策内容 —— 决策 4 规定的是「**宏** + 钩子扩展」两条;泛化只是让「宏能用于 CohIfBundle」成为可能的**实现前提**,不是新增决策。

> **登记 ③ —— 行号漂移**:`include/framework/bidirectional_port_adapter.hh` 因泛化整体下移 —— `bind_port_pair` 128 → **171**、`bind_ports`(**空操作 shim**)141 → **184**、`pending_credits_` 105 → **148**、`receive_credit` 调用 184 → **225**、`send_credit` 216 → **257**、`send_credit_impl` 228 → **269**;**`CATEGORY_CREDIT` 写入点** 235 → **109**(迁入 `BundleDirectionTraits<NoCFlitBundle>::fill_credit`,**语义不变**)。
> ⚠️ 本文件正文的 `bidirectional_port_adapter.hh:128` / `:141` 引用按「ADR 不可变」**不改**;`chstream_register.hh` 的既有行号(**50 / 119 / 121 / 175 / 177**)**零漂移**。

## Status Update 2026-10-28(第四则)—— 决策 4/5 的装配路径**已落地** + `INV-CEP-07-IC4` 闭合 + egress 通道语义登记 + 行号漂移

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记状态变化。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #88**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #88)。

> **登记 ① —— 决策 4 的装配宏 + 决策 5 的 JSON 端口注册路径,已端到端跑通**:
> - 交付 `CohIfProtocolEngine`(`CohIfNullProtocolEngine` 的**非模板可注册外壳**,10 流行为**完全继承** `NullProtocolEngine`,**无新增协议语义**)⇒ 由 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED` 注册(短名 `CohIfProtocolEngine`,N=2)。
> - 协议桥 `cohif_noc` 由 `ModuleFactory::register_protocol_bridge` **全局注册**;其**消费**由 `config["cep"]["bridges"]` 驱动(Step 7.5)⇒ **注册与消费成对**的判据(CC-05 ②)已可执行。
> - 端到端判据形态:一次 CohIf 事务**穿过** adapter 抵达对端 ⇒ 断言对端 canonical 流**计数器递增** + 发起侧**未**收到自己的事务 + 其它流**未**被误触发(**非仅「绑定成功」**)。
> - ⇒ **`INV-CEP-07-IC4`(CEP 模块注册走既有宏体系)** 由「无载体」转「**已闭合**」。

> **登记 ② —— egress 通道语义(新增决策面,此前 ADR 未涉及)**:
> `BidirectionalPortAdapter` 的 egress 走 `req_out` 还是 `resp_out`,由 **Bundle 专属 trait** 声明:`cpptlm::BundleDirectionTraits<BundleT>::egress_channel()`(`Resp` = 默认 = `RouterTLM` 既有行为;`Req` = `CohIf` 语义)。
> ⚠️ **一条实测纠偏**(避免留下错误口径):**两种通道都能把数据送到对端 ingress** —— 框架的 `ChStreamTargetPort::recvReq` 对 `req_in` / `resp_in` **一视同仁** ⇒ 「走 `resp_out` 会丢数据」的推断**不成立**。⇒ `CohIf` 选 `req_out` 的**真实理由**是:① **方向语义正确**(egress 借用「对方→我」那条连接的响应通道会**倒置方向**);② **判据有牙**(egress 走 `req_out` 时,「断开我→对方的连接」**确实切得断** egress —— 实测改成 `Resp` 会让反向 fixture **假通过**)。
> ⚠️ **决策面影响**:本登记**不改变**决策 1 ~ 7 的任何决策内容 —— 它是**实现层的方向语义澄清**,已由具名判据钉住(`test/test_cep_cohif_json_assembly.cc`)。

> **登记 ③ —— 行号漂移**:
> `include/chstream_register.hh` ⇒ `REGISTER_CHSTREAM` 50 → **107**、`registerObject<tlm::RouterTLM>` 62 → **119**、`registerObject<tlm::NICTLM>` 63 → **120**、`registerBidirectionalPortAdapter<tlm::RouterTLM>` 119 → **188**、`registerDualPortAdapter<tlm::NICTLM>` 121 → **190**、`REGISTER_CHSTREAM_DUAL` 175 → **244**、其宏内 `registerDualPortAdapter` 177 → **246**;
> `include/framework/bidirectional_port_adapter.hh` ⇒ `CATEGORY_CREDIT` 写入点 109 → **133**、`pending_credits_` 148 → **174**、`bind_port_pair` 171 → **197**、`bind_ports`(**空操作 shim**)184 → **210**、`receive_credit` 调用 225 → **260**、`send_credit` 257 → **292**、`send_credit_impl` 269 → **304**。
> ⚠️ **一处「零漂移」结论已失效**:第三则 ③ 登记的「`chstream_register.hh` 既有行号零漂移」是**当时**口径,已被 Phase 5 推翻。⚠️ 本文件**正文**的行号引用(`chstream_register.hh:63` / `:119` / `:175`、`module_factory.hh:313-315` 等)按「ADR 不可变」**不改**;完整对照表见 `docs/architecture/06-inter-module-network.md` 文末「附录 · 行号对照表(Entry #88)」。

## Status Update 2026-10-28(第五则)—— 判据收口(Phase 6)+ 反向 fixture 登记(Phase 7)+ 新增 invariant 数 = 0

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记状态变化。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #89**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #89)。

> **登记 ① —— 6 条判据全部有可执行判据 + 反向 fixture,且「登记位置」可查**:
> - `test/test_cep_cohif_carrier.cc`(新建,17 case / 52 assertion):**CC-01 / CC-02 / CC-04**;
> - `test/test_cep_protocol_bridge_contracts.cc`(Phase 3 交付,14 case / 104 assertion):**CC-03 / CC-05 / CC-06**;
> - **CC-06 引用**队列 #6 的 AC-04 实现(`tlm::cep::category_value_domain()`)⇒ **不重立第二份判据**。
> - ⇒ 新增 **`scripts/README.md` §反向 fixture 登记**表(判据 ID · fixture 形态 · **登记位置**,共 **13** 条)+ `docs/ONBOARDING.md §5.5` 补登记 + ctest 显式登记 3 条 ⇒ 「无登记 ⇒ CI 跑不到 ⇒ 等于没有」的缺口闭合。

> **登记 ② —— 编译期锁的 ctest 形态(per 判据纪律)**:编译期锁**不得**只以「编译红」充当 ctest 判据 ⇒
> `test/test_cep_cohif_carrier.cc` 在 **ctest 内**调编译器,断言「编译**确实**失败」**且**
> 「失败原因**命中**预期的那条 `static_assert`」⇒ 防「因错误的原因红」。
> ⚠️ **反向 fixture 实测有牙**:临时删掉**头文件 + 编译探针**两处 `is_trivially_copyable` 锁
> ⇒ 「CC-02 反向 fixture ④」**报红**;恢复后全绿。

> **登记 ③ —— 新增 invariant 数 = 0(per 本 ADR `D-θ`)**:实测
> ① `test/test_cep_canonical_invariants.cc` 在**全 change 范围零 diff**;
> ② 全部提交**未新增**任何 `INV-CEP-*` **定义**(diff 中只出现既有 invariant 的引用/登记:
> `INV-CEP-07-IC2` 载体落地 · `INV-CEP-07-IC4` 闭合);
> ③ `[cep][canonical-invariant]` 13 case / 687 assertion 全绿。

> **登记 ④ —— 一处口径偏离(诚实登记)**:Phase 7 的 task 7.1 字面指向
> `scripts/test/cep_decl_reverse_verify.sh`,但该脚本守的是「**协议实例声明 ↔ schema 一致性**」域
> (`fabric.json` / `protocol_instances.json` / ArchForge 声明),与本 change 的判据域
> (CohIf 载体 / ProtocolEngine 端口面 / ProtocolBridge / JSON 装配)**零交集**
> ⇒ 在其中加本 change 的 fixture **等于测另一个闸**(违反「不重发明」)
> ⇒ **未改动该脚本**,改按「登记位置补齐」落地。
> ⚠️ **决策面影响**:**不改变**本 ADR 决策 1 ~ 9 的任何决策内容。

> **登记 ⑤ —— 门禁实测**:9 项 CEP 脚本 + `docs_sync_check --strict` + `openspec validate --changes --strict` 全 rc=0;
> `cep_gate_selftest` rc=0;`ctest -R cep` **35/35**;全仓 **1983 case / 873322 assertion** 全绿(0 回归)。

## 7. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
>
> - **v1.0(2026-10-27)**:签发。draft 阶段的 3 轮 critic 评审闭环(REJECT 2 B + 10 S + 7 N → APPROVE_WITH_CONDITIONS ×3 → `mergeable: yes`)。

## Status Update 2026-10-27 —— 判据子条件追认(CC-05 ④ / CC-06 ②)与判据真源层级登记

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §7 正文一律不改**,仅登记追认与口径。
>
>
> **追认 ① —— CC-05 子条件 ④(per-bridge 元数据齐备)**: **追认成立**。依据本 ADR `§3.2 代价` 已声明「**per-bridge 元数据只做静态**(`protocol_id_` / `bridge_id_`,per `docs/architecture/06-inter-module-network.md` §4.5)」⇒ 该元数据**本就在本 ADR 的决策面之内**;子条件 ④ 只是把**已声明的代价项**判据化(机器可检),**不引入新决策、不改 §2 决策 5**。
>
> **追认 ② —— CC-06 子条件 ②(映射表 SHALL 非恒等)**: **追认成立**,但**依据标注为逻辑推论**(无对应上游条款)—— 依据是「特化字典的存在理由」:若 6→3 映射**恒等**,则「CohIf 语义 → 网络 Bundle 链路层类别」无需字典,队列 #6 的映射表机制本身**失去存在意义**。该子条件是**反向护栏**(防字典退化),**不改变** CC-06 的主判据(产出的 `CATEGORY_*` 必须落在网络 Bundle 的 3 个常量内)。
>
> **判据集合的最终形态**: 编号仍为 **CC-01 ~ CC-06 共 6 条** —— 与本 ADR `§5` 后续表所述「6 条判据 CC-01~CC-06」**一致**;本段只固化**子条件**,**不新增判据编号**。
>
> **判据口径(编号对齐 ≠ 定义对齐)**: 「判据**编号**」以本 ADR 为准;「判据**定义**」此前只在附属层调研文档 `§6.4` ⇒ **本 ADR 不自足**,属**真源层级缺陷**,在此诚实登记。**治本路径** = 队列 #7 转正归档后 `openspec archive` 把 delta spec 并入 `openspec/specs/` ⇒ 判据进入真源层(自足),本段届时退为历史沿革。
>
> **追认的边界**: 本段**不**新增决策、**不**改判据**编号集合**、**不**引入新的架构取舍、**不**触碰 §2 决策 9(`StubCacheCore` 延期)。若实现阶段证明子条件 ④ / ② **不可实现**,须**新开**一段 Status Update 登记其失效,**不得**回改本段(per ADR 不可变纪律)。
>
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #78**
>
> **冻结面**: 本文件属 M1 冻结面 **40 成员**;本次改动已走授权闸 `cep_freeze_check.sh --update --authorized-by "Entry #78"`。

---

## Status Update(2026-11-08,per 台账 Entry #288)—— 决策 9 的**延期项解除条件已满足** + 收编进具名 change

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文与既有 SU(尤其**决策 9** 及其**四条理由**—— **逐字保留不回改**);⛔ 不改决策 1 ~ 8 与决策 10+ 的任何一条。
> **触发**:台账 **Entry #269**(SU · 七:显式延期项的事实复核)· **#280**(SU · 七)· **#282**(4 条 architect 裁决)· **#284**(SU · 九 路径更正)· **#285**(架构文档审查)· **#287**(下一步任务推荐)· **#288**(本轮 10 条决策统一)。
> **审查**:`oracle`(10 条决策的独立架构复核)→ `critic`(本段 + `07 号` 对应段的架构审稿),顺序不可颠倒。

### ① 决策 9 的延期项:**解除条件已满足**(延期态 → 实施态)

**决策 9**(`:130-140`)把「`StubCacheCore` 的 ChStream 端口面」判为**显式延期**,并给出**四条互斥理由**:① spec 在 `FROZEN_GLOBS` 内;② spec 的「`net_side` SHALL 由 **mock** ProtocolEngine 驱动」+「三个流名 SHALL 不出现作为 stub 的对外接口」;③ `stub_cache_core.hh:39-40`「**不**经 `ModuleFactory` 注册(本类是 **testbench 定位** 的组件)」;④ `ADR-CEP-07:67` / `07 号 §6.3` 它**应该** `registerObject` + `ChStreamAdapterFactory::create`;⇒ 并指明「该腿归 `cpptlm-cep-stub-cache-chstream-surface`(新,排本队列之后)」+「**前置** = 本队列交付 `CohIfBundle`」。

**解除条件已满足的逐条核对**:

| 理由 | 现状 | 判定 |
|---|---|---|
| ① spec 在 `FROZEN_GLOBS` 内 | **仍成立** ⇒ ⇒ 解它**必** MODIFIES 一份冻结 main spec(**两步授权** + 台账 Entry) | ⇒ **未被绕过**;改为**显式 MODIFIES**(见 ③) |
| ② spec 的 mock / 三词禁约束 | 前者**将**由本轮 MODIFIES 解除(见 ③,⛔ **仅 `:56` 一条**);后者**仍成立**(`forward_data` / `back_invalidate` / `barrier` 三词**继续**不在 stub 对外接口出现) | ⇒ **部分解除**,不整体作废 |
| ③ stub 是 testbench 定位的**纯类** | **仍成立且被刻意保全** —— 本轮**不**给 `StubCacheCore` 加端口面;改为**组合式 wrapper**(`StubCacheEndpointTLM` 持有 stub 实例)⇒ ⇒ **stub 纯类零 diff** | ⇒ **未被推翻** |
| ④ 它**应该** `registerObject` | 由 wrapper 侧的**真实注册机制**(`registerObject` + `registerDualPortAdapter`)达成 ⇒ ⇒ 缺口**闭合** | ⇒ ✅ **已满足** |

**前置条件**:本队列交付 `CohIfBundle` —— ✅ **已交付**(2026-10-28,Entry #90)⇒ ⇒ **决策 9 的延期项进入实施态**,且**不新建** change 名字 ⇒ **收编**进既有具名 change **`cpptlm-cep-stub-cache-chstream-surface`**。

### ② 实施形态的裁决登记(per 台账 Entry #282 的 4 条裁决)

| 项 | 裁决 |
|---|---|
| **形态** | **组合式 wrapper** `StubCacheEndpointTLM`(`ChStreamModuleBase` 派生)内部**持有** `StubCacheCore` 实例;注册 = `registerObject` + `registerDualPortAdapter<..., CacheReqBundle, CacheRespBundle, CohIfBundle, CohIfBundle>`(异构双端口;先例 `NICTLM`)⇒ ⇒ **`StubCacheCore` 纯类零改动**(`INV-SC1`~`INV-SC6` 与 14 个既有单测**零变化**) |
| **映射面归属** | **wrapper 内部**(G-0e 两侧):op 编码 / `evict_line` 缺口(**保留 core-side 方法 + 装配夹具显式调用**,⛔ 不伪造 Bundle 字段)/ 同步 `AccessResult` ⇒ 异步 `CacheRespBundle` 的在途关联(直读 stub 既有观测面)/ net_side `CohIfBundle` ↔ 功能 5 流编码;⛔ **`cohif_noc` 桥的 `pack`/`unpack` 冻结不动** |
| **语义分层红线** | **正向圈定**:映射实现**仅许**存在于 wrapper 自身 `.hh`/`.cc`,⛔ **不得**另立 mapper / 翻译 / 桥接模块;**负向禁令**:禁入 `include/framework/` 与 `include/tlm/cep/cohif_noc_bridge.hh`;`flow_id` → canonical 流的解释权在**类型层** ⇒ wrapper 只**产生**合法 `flow_id`,⛔ **不解释**其含义 |
| **change 归属** | 收编进 `cpptlm-cep-stub-cache-chstream-surface`(**不改名**);⚠️ 其名只承诺「端口面」而收编后承载 **G1 端口面 + G2 拓扑 + G3 断言 + G4' metric 产出**四面 ⇒ ⇒ **proposal 须显式声明收编 scope** |

### ③ 冻结 main spec `openspec/specs/cep-stub-cache-core/spec.md` 的 MODIFIES 预告(**2 条**)

| 条 | 现状 | 将改为 |
|---|---|---|
| `:56` | 「`net_side` SHALL 由 **mock ProtocolEngine** 驱动(本 change **不**引入真实协议引擎)」 | 「`net_side` SHALL 由**经 `CohIfBundle` 端口面**的**对端协议引擎**驱动(本 change **不**引入 mock ProtocolEngine)」+ 主语写「**端口面 wrapper 持有的** stub 的 `net_side`」 |
| `:71` | 「**WHEN** cpu_side **命中** ⇒ **THEN** `net_side` SHALL **不**产生任何 flit」 | ⛔ **不 MODIFIES**(2026-11-08 勘误)—— 该条的 WHEN **限定在命中路径**;拓扑 B 的 flit 只产生于 **miss / request**(及 snoop / writeback)路径 ⇒ **不冲突** ⇒ ⇒ 若要让「桥 flit 路径」入规范,正确载具是 change delta 的 **ADDED Scenario**(自带授权),⛔ **不改**冻结的命中路径 Scenario(否则削弱 `INV-SC6` 的可执行锚) |

⛔ `:63` **不动**(`forward_data` / `back_invalidate` / `barrier` 三词**继续**不在 stub 对外接口出现)。
⚠️ **勘误溯源**:本轮初稿曾把 `:71` 列入 MODIFIES(沿 Entry #282 的记录);实测其 WHEN 范围后**撤销** ⇒ ⇒ **冻结授权面由 2 条收缩为 1 条**(`:56`)。
⚠️ **两条授权链分离**:本 SU 只授权**架构文档**(`ADR-CEP-19` + `07 号`);spec 的 MODIFIES 属**实施期**动作 ⇒ ⇒ 须由 **builder 在其 change 内**执行,并**另立台账 Entry** 走两步授权(per 决策 9 理由 ①)⇒ ⇒ ⛔ 本段**不**预先授权 spec 改动。

### ④ 两个拓扑(CHI RN-F 族;**实施前**不宣称已落地)

| 拓扑 | 形态 | 承载的判据 | 边界 |
|---|---|---|---|
| **A(直连)** | `TrafficGenTLM → StubCacheEndpointTLM → ChiRnfProtocolEngine ⇄ ChiRnfProtocolEngine` ⇄ **`CohIfResponderTLM`(完成者,D-0)** | **全绿载体**:13 键全有值 + IC3 闸门绿 + 端到端响应完整 | — |
| **B(带延迟 + 投影登记)** | Stub↔引擎 与 引擎↔完成者 均走**带 `latency` 的 ChStream `connections`**;三个 CohIf 端点**共挂一个** `cohif_noc` bridge entry(同一**投影语境**) | **互联可达**(经带延迟的连接,⛔ **不经**桥)+ **桥投影**(测试级 `pack`/`unpack` 断言) | ⛔ **有损投影**(flow_id 5→3)⇒ ⛔ **不承载** canonical 10 流的完整语义 ⇒ ⛔ **不承载全绿**;⚠️ **如实命名**:「网络」= ChStream 连接延迟,**非** Router/flit 路径(仓内不存在)⇒ ⛔ **不得**宣称 flit 级路径 |

### ④-a 「完成者」角色的裁决(用户裁定,2026-11-08)与**拓扑准入判据**

**用户裁定的拓扑准入判据(治理纪律,逐字)**:

> 「**JSON 拓扑代表真实物理存在的**;如果响应者元素是**物理上不存在**、只是为了测试而创建 ⇒ 那就**不进 JSON 拓扑**;否则 ⇒ **就要进 JSON 拓扑**。」

⇒ **据此**:「完成者」元素扮演的是 **home(应答端)** —— home 在真实系统里**物理存在** ⇒ ⇒ **它进 JSON 拓扑** ⇒ ⇒ 落点 = **契约面**(`include/tlm/cep/`,**可注册**的、**协议中立**的 ChStream 模块)⇒ ⇒ **⛔ 不进 `protocol_instances.json`**(它**不是**协议实例 ⇒ 由此**自然**不产生任何协议语义声明 / **不计入 B3b**)⇒ ⇒ **装配闸不判红**(闸只要求 `bridges[].modules` 的成员**存在于拓扑**,⛔ **不**要求其为协议实例)。

⚠️ **连带(实施期须处理)**:桥的装配期挂载点 `attach_protocol_bridge` 当前**仅**由 `ProtocolEngineBase` override ⇒ ⇒ **wrapper 与完成者均需新增该 override** 才能进入 `cep.bridges[]` ⇒ ⇒ **拓扑 B 的「桥挂载结构」待裁**(B-1 三端点共挂一桥 / B-2 桥仅连 Stub↔引擎、完成者走 `connections`)。

⚠️ **⚠️ 未闭合的前置(2026-11-08 oracle 复核发现)——拓扑 A/B 的「全绿」缺「完成者」角色**:
stub 是 RN ⇒ miss 时发起 `request` 并**挂起等 `completion`**;而**全仓无任何模块**给 `flow_id` 赋 `Completion` / `Snoop`(各引擎的 `on_request` **只记账**)⇒ ⇒ stub 的 miss 请求**无任何回流** ⇒ ⇒ `ProgressWatchdog` 必触发 ⇒ `deadlock_violations > 0` ⇒ **IC3 红** ⇒ ⇒ **J-3 / J-4 结构性不可达** ⇒ ⇒ **该角色落定前,本段的「全绿」目标不可开工** ⇒ ⇒ **待裁(新增决策点 D-0)**。

### ④-b ⚠️ 「经网络」的定义钉死(与 `07 号` SU · 十一 ①-b 同款)

「网络」= ChStream `connections`(可带 `latency`);**桥是装配期投影登记器,不在数据路径上** ⇒ ⇒ **「事务穿过桥抵达对端」是假绿表述** ⇒ ⇒ 拓扑 B 的两个验证目标必须**拆开**:**互联可达**(经带 latency 的连接,⛔ 不经桥)+ **桥投影**(测试级 `pack`/`unpack` 断言,⛔ 非「事务穿过桥」)。

### ⑤ ⚠️ 计数纪律(**不得**用框架跑通改判 B3b)

> 本框架**只**提升**可实验性**(真实业务流量下的行为面可测);⛔ **不**构成 `B3b-2` 的解除证据 —— `B3b-2` 的计数依据是**规范条款级** T1–T4(per `07 号` SU · 四 D-1),而本框架的**链路跑通**⛔ **不是**条款级依据 ⇒ ⇒ **`B3b-2` 维持 0/4**;且本框架**不改变** 4 受限族(CXL 2.0 / CXL 3.0 / NVLink / UALink)的任何状态或 deferral 登记(⛔ `test/cep_b3b2_deferrals.json` **零 diff**)。

### ⑥ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且本 ADR 正文与**全部**既有 SU **逐字未变**
- [ ] 决策 9 的**四条理由**逐条核对已登记(① 仍成立⇒改显式 MODIFIES;② 部分解除;③ **刻意保全**;④ **已闭合**)
- [ ] 决策 9 的**延期项状态**已由「显式延期」更新为「**进入实施态**」;具名 change **收编**(不改名)
- [ ] 冻结 main spec 的 **1 条** MODIFIES 预告已登记(`:56`);`:71` **已撤销**(勘误);`:63` **不动**;⚠️ spec 改动**另立 Entry + 两步授权**(本段**不**预授权)
- [ ] 两个拓扑(A 直连 / B 经网络)的形态与**承载边界**已登记;⛔ B **不承载全绿**
- [ ] ⑤ 的计数纪律禁令句已逐字登记;`test/cep_b3b2_deferrals.json` **零 diff**
- [ ] ⚠️ **实施前**的文档面**无**任何「已具备端到端可测能力」类宣称(⛔ 决策登记 ≠ 落地)
- [ ] 冻结面两步授权已执行:台账 **Entry #288** 授权 → **commit 本段 + `07 号` 对应段** → `cep_freeze_check.sh --update --authorized-by "Entry #288"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #288**(本轮 10 条决策 + 授权)· **#287**(下一步任务推荐)· **#285**(架构文档审查)· **#284**(`07 号` SU · 九 路径更正)· **#282**(4 条 architect 裁决)· **#280**(`07 号` SU · 七)· **#269**(`ADR-CEP-19` 决策 9 + 显式延期)· `ADR-CEP-05` 决策 2(三明治挂载)· `ADR-CEP-07` 决策 3(真实注册机制)· `openspec/specs/cep-stub-cache-core/spec.md`(`:56` / `:63` / `:71`)· `include/tlm/cep/stub_cache_core.hh` · `include/tlm/cep/cohif_noc_bridge.hh`

---

## Status Update(2026-11-08 · 二,per 台账 Entry #292)—— ① 决策 9 与本 SU 的**状态追认**(D-0 / 桥挂载结构 = 已裁 B-1)② `:71` 撤销**理由**勘误 ③ 「完成者」定位改写 = **真实 home 的简化模型** ④ 拓扑 B **独立保留** + 拓扑分档判据 **J-9**

> **性质**:纯**追加 SU**(append-only)。⛔ 不改本 ADR 正文 §1 ~ §7 与**任何**既有 SU(尤其 **Entry #288 SU** 的 ① ~ ⑥ —— **逐字保留不回改**;⛔ 其 §④-a 的两处「**待裁**」措辞**由本段追认取代**,原文保留);⛔ 不改决策 1 ~ 8 与决策 10+ 的任何一条;⛔ 不改 `07 号` 既有段。
> **触发**:台账 **Entry #291**(两条用户裁决 · F-05 / F-11)· 第二轮审查 `.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md`(**F-01 / F-04 / F-05 / F-11**)· `07 号` **SU · 十一** ①-b / ①-c。
> **审查**:`oracle`(4 项勘误的**理由是否站得住** + 新增边界是否引入新的越界宣称)→ `critic`(本段 + `07 号` SU · 十二 的架构审稿),顺序不可颠倒。
> **关联**:`07 号` SU · 十二 · `ADR-CEP-12` SU 2026-11-08 · `ADR-CEP-05` 决策 2 · `ADR-CEP-07` 决策 3 · `openspec/specs/cep-stub-cache-core/spec.md`(`:56` / `:63` / `:71`)· `include/tlm/cep/protocol_instance.schema.json`

### ① 状态追认(Entry #288 SU §④-a 的两处「待裁」**已过时**)

- **原文(⛔ 保留不回改)**:本 SU §④-a 末段写「⇒ ⇒ **拓扑 B 的「桥挂载结构」待裁**(B-1 三端点共挂一桥 / B-2 桥仅连 Stub↔引擎、完成者走 `connections`)」;同段末写「⇒ ⇒ **待裁(新增决策点 D-0)**」。
- **追认**:用户 **2026-11-08** 裁定 **D-0 已裁**(落点 = `07 号` SU · 十一 ① 的 D-0 行)· **桥挂载结构 = B-1 + 显式语义降级**(落点 = 同 SU ①-c)⇒ ⇒ **该两处状态 = 已裁**;⇒ 本段为**现行状态真源**。
- ⚠️ **追认的范围**:仅**状态**(待裁 → 已裁)+ **理由链**;⛔ **不**追认任何**新的**架构取舍 ⇒ ①-c 的「B-1 + 语义降级」原文**逐字有效**,本段**不改**其内容。
- ⚠️ **附带措辞精确化**:`07 号` SU · 十一 ①-c 称 B-1 裁定「**零代码、零新机制**」;而本 SU §④-a 明写「wrapper 与完成者**均需新增** `attach_protocol_bridge` override」⇒ ⇒ **精确化**为:「零代码」= **不新增桥机制 / 拓扑机制**(两处 override 属**已排期**的实施侧 A1 / A2 范围)⇒ ⛔ **不得**读作「本次改动不需要写代码」。

### ② `:71` 撤销的**理由**勘误(结论不变、且 `:71` **今日有牙**)

- **原文(⛔ 保留不回改)**:本 SU §③ 的 `:71` 行写「⛔ **不 MODIFIES**(2026-11-08 勘误)—— 该条的 WHEN **限定在命中路径**;拓扑 B 的 **flit 只产生于 miss / request**(及 snoop / writeback)路径 ⇒ **不冲突**」;同表「若要让『桥 flit 路径』入规范,正确载具是 change delta 的 **ADDED Scenario**」。
- **该理由的问题(仅一处在「命名」)**:其**内核**(WHEN 限定命中路径 / miss-request 路径才有流量)**成立**;但它把该流量挂在「**拓扑 B 的 flit 路径**」名下;而 `07 号` SU · 十一 ①-b / ①-c **已钉死**:桥 = **装配期投影登记器**(无状态投影器;`pack` / `unpack` 只搬共有字段 + `flow_id → flit_category` 有损投影;`src/` 中**零调用点**)⇒ ⇒ 仓内**不存在** Router + flit 路径 ⇒ ⇒ **「拓扑 B 的 flit 路径」这一命名作废**(⛔ 不得被读成「拓扑 B 有 flit 行为」)。
- ⇒ ⇒ **勘误后的理由**(取代原措辞):
  1. `:71` 的 WHEN **限定在命中路径**;
  2. 命中路径下 stub 的行为(本地状态更新 + 回 `cpu_side` 响应 + `net_requests` **不增**)与 **net_side 的驱动者是谁**(mock 引擎 vs 真实引擎)**无关** ⇒ ⇒ `:56` 的改动(mock → 真实引擎)**不改变** `:71` 的成立性;
  3. 拓扑 B 只增加 **miss / request** 路径流量 ⇒ ⇒ 与 `:71` **无冲突**;
  4. ⇒ ⇒ `:71` **保持**,且**继续由 `INV-SC6` 的计数断言执行**(锚点 `test_cep_stub_cache_core.cc`;实测 `ctest` 164/164 绿)⇒ ⇒ **冻结授权面仍 = `:56` 单条**。
  5. 「若要让『桥 flit 路径』入规范,正确载具是 change delta 的 **ADDED Scenario**」这半句**继续有效**(与「桥不在数据路径上」并存,per `07 号` SU · 十一 ①-b + 本 ADR Entry #288 SU ④-b;⚠️ 「D-11」这一**编号**仅见于讨论纪要,⛔ 不得当作已签发文档的引用锚点)。
- ⚠️ **防误读句(逐字)**:
  > ⛔ **不得**据「命中路径限定」推出 `:71` 可删除、可降级或已空转 —— 它是**现行执行**的冻结锚(`INV-SC6` 计数断言);⚠️ 「仓内无 flit 数据路径」只支撑本行的**后半句**(桥 flit 路径入规范须走 ADDED Scenario),⛔ **不得**用作 `:71` 不动的前台理由。
- ⚠️ **结论不变**:`:71` 仍**不 MODIFIES**;`:56` 仍是**唯一** MODIFIES 条目;`:63` 仍**不动**。

### ③ 「完成者」定位改写 = **真实 home 的简化模型**(承载最小协议语义)

- **原文(⛔ 保留不回改)**:本 SU §④-a 写「⇒ **据此**:「完成者」元素扮演的是 **home(应答端)** —— home 在真实系统里**物理存在** ⇒ ⇒ **它进 JSON 拓扑** ⇒ ⇒ 落点 = **契约面**(`include/tlm/cep/`,**可注册**的、**协议中立**的 ChStream 模块)⇒ ⇒ **⛔ 不进 `protocol_instances.json`**(它**不是**协议实例 ⇒ 由此**自然**不产生任何协议语义声明 / **不计入 B3b**)⇒ ⇒ **装配闸不判红**(闸只要求 `bridges[].modules` 的成员**存在于拓扑**,⛔ **不**要求其为协议实例)」。
- **改写后的表述**(逐项):
  - 元素 = **真实 home 的简化模型** —— home 在真实系统里物理存在 ⇒ 其**建模替身**代表真实物理元素 ⇒ ⇒(**用户裁决**,per 台账 Entry #291 ①)按用户自裁的**拓扑准入判据**(本 SU §④-a 逐字引用)⇒ ⇒ **进 JSON 拓扑**;
    - ⚠️ **如实登记**:「角色真实性 ⇒ 实例真实性」的滑移是**被用户裁决消解**的,⛔ **不是**逻辑蕴含 ⇒ ⛔ 不得把该推导链当**通用**规则复用;
  - **承载**第一族(**CHI RN-F**)的**最小应答语义** = ① 消费 `Request` ⇒ 生成 `Completion`;② 按读请求**可配发** `Snoop`;③ 消费 `Writeback` ⇒ **接收即消费、零响应**(fire-and-forget);
  - ⛔ **不**承载家族级 / 条款级语义:**credit · retry · ordering point · 多拍(`CompData` / `DataSepResp`)聚合 · Perm 规则 · 容量与队列模型**;
  - ⛔ **「协议中立」措辞作废** —— 该模块是**协议相关实现**(协议中立即在**平台层**成立:CohIfBundle 端口 + canonical 10 流);⇒ 归入契约面**与 adapter / engine / bridge 并列的独立类目:home 模型**(⛔ **不是**第二个 engine、⛔ **不是** adapter、⛔ **不是** protocol instance);
  - ⇒ ⇒ **不进** `protocol_instances.json`(它**不是**协议实例 ⇒ 由此**自然**不产生任何协议语义声明 / **不计入 B3b**)⇒ ⇒ **装配闸不判红**(闸只要求 `bridges[].modules` 的成员**存在于拓扑**,⛔ **不**要求其为协议实例)—— **此四项落点结论逐字不变**;⚠️ 但被取代的**不止**「协议中立」限定词,还有 §④-a 推理链里「由此**自然**不产生任何协议语义声明」半句(承载面由「零语义」变为「最小应答语义」= 读者最需识别的实质变化)⇒ ⇒ **变化面以本段 ③ 与 `07 号` SU · 十二 ② 的对照表为准**。
- ⚠️ **命名**:类名 / 注册短名 `CohIfResponderTLM` **维持不变**(per `07 号` SU · 十一 ①-c 的 B-1 模块清单 + 台账 Entry #291 裁决 A);⛔ `07 号` SU · 十一 ① 的 D-0 行原文写「⚠️ **命名待裁**」—— 该措辞**由本段追认取代**(同 ① 的追认机制)⇒ ⇒ ⚠️ **不得**据类名中的 `CohIf` 推断其行为面为协议中立;类目以本段为准(**home 模型 · 协议相关**)。
- ⚠️ **防误读句(逐字;须同时进契约面 delta 与台账)**:
  > home 简化模型承载**最小应答语义** ⇒ ⛔ **不**构成任何家族的 **T1–T4 条款级**证据(计数依据 = 规范条款级)⇒ ⇒ **`B3b-2` 维持 0/4**;⛔ **不得**以「CEP 有了 home 模型」推出协议语义达成。
- ⚠️ **待裁(本段不预裁)· 声明面落点二选一**:

  | 路线 | 内容 | 冻结面代价 |
  |---|---|---|
  | **甲(建议)** | **不进** `protocol_instances.json` + 契约面 delta 登记**明示豁免**(「声明 ↔ 实现对齐」的例外登记) | **零** —— `include/tlm/cep/protocol_instances.json` **不在** M1 冻结集;`protocol_instance.schema.json` 不动 |
  | **乙** | 给实例加**第 14 键**(如 `home_model`) | ⚠️ **触及 M1 冻结件** `include/tlm/cep/protocol_instance.schema.json`(实测 `additionalProperties: false` ⇒ 加键即违 schema)⇒ 须**另立台账 Entry + 两步授权** |

  ⇒ ⇒ 裁定归 **architect**;本段**只登记约束**,⛔ 不预裁。

### ④ 拓扑 B **独立保留** + 拓扑分档判据 **J-9**

- **裁定**:**两个拓扑并存维持**(A 直连 = 全绿载体;B = **独立拓扑**)⇒ ⇒ ⚠️ 代价 = **两套 config + 两套判据分档** ⇒ ⇒ 用**机器可判**的分档判据对冲。
- **「A 必须零延迟」的判据级理由**(B-1)—— ⛔ **因果序不可颠倒**(防后人只引半句):
  - **主理由 = 判别力**:若 A 也带 `latency` ⇒ B 的「**互联可达**」面**退化为 A 的子集** ⇒ ⇒ B 的独立价值收窄到只剩「桥投影装配面」⇒ ⇒ 「两拓扑并存」失去**主要**依据 ⇒ ⇒ 故 **A 的 `connections` 一律 `latency = 0`**(⚠️ 「桥投影」面因 A 按 J-9 定义**永不带** bridge entry ⇒ 不会因 A 带 latency 而退化 ⇒ ⛔ 不得把该面算进退化项);
  - **次理由 = latency 断言纯度**:**A 是全绿载体** ⇒ 其 latency 断言必须**只**反映**模块 / 协议处理延迟** ⇒ ⛔ **不**混入**连接延迟**;
  - ⇒ ⇒ 两拓扑**不重叠**(B 专门承载「**带连接延迟时事务仍抵达对端**」这一面)。
- **J-9 拓扑分档判据(机器判)**:

  | 拓扑 | 判据 |
  |---|---|
  | **A** | 装配闸 `SKIP`(⇒ `cep.bridges` 键**缺省**)∧ `cep.bridges` **不存在** ∧ 所有 `connections` **无 `latency > 0`** |
  | **B** | 装配闸 `PASS` ∧ `cep.bridges` **恰好 1 条** ∧ 该条 `modules` **恰好 3** ∧ **至少 1 条** `connections` 带 `latency > 0` ∧ 三个模块的**桥指针**指向**同一 `bridge_id`** |

  - ⚠️ **两条实施契约(缺一 ⇒ J-9 落空)**:① B 的 bridge entry **必须**带 `protocol_instance` 且命名一个**已声明**实例(实测 `[IAG-02]`:缺失或未声明 ⇒ 判红)⇒ ⇒ config 作者若不知此约束 ⇒ B **永远 FAIL**;② `ChStreamModuleBase` **无 `bridge()` 访问器**(实测仅有 `attach_protocol_bridge` 虚函数;`bridge()` 只存在于 `ProtocolEngineBase`)⇒ ⇒ 「逐模块桥指针指向同一 `bridge_id`」预设了**当前不存在**的能力 ⇒ ⇒ 契约须写明:**wrapper / home 模型自行存储桥指针并暴露只读访问器**(⛔ **不动基类**,per D-1「零基类改动」)。
- **B 的独立价值(3 条可判项)**:
  1. **互联可达** = 一次事务经**带 `latency` 的** ChStream `connections` 抵达对端(⛔ **不经**桥);
  2. **三端点共挂一 entry** 的装配面(单 entry / 3 模块 / 同一 `bridge_id`);
  3. **桥投影** = **测试级** `pack` / `unpack` 断言(⛔ **非**「事务穿过桥」—— 假绿表述,per `07 号` SU · 十一 ①-b)。
- ⚠️ **B 仍不承载全绿**(沿用 Entry #288 SU ④ 的边界)⇒ ⛔ 不得把 B 的连通性读成「13 键全绿」。

### ⑤ Verification(可勾选验收判据 · 前瞻)

- [ ] 本段 SU 已追加,且本 ADR 正文与**全部**既有 SU **逐字未变**
- [ ] ① 的状态追认已登记(两处「待裁」→ 已裁 B-1 / D-0),原文保留未回改;「零代码」的精确化已登记
- [ ] ② 的 `:71` 撤销**理由**已勘误为「命中路径限定 + 驱动者无关性」,**结论未变**(`:71` 仍不 MODIFIES;冻结授权面仍 = `:56` 单条);且已登记「`:71` **今日有牙**(`INV-SC6` 计数断言)」与防误读句
- [ ] ③ 的定位已改写为「真实 home 的简化模型」+ 最小应答语义边界(含 `Writeback` 的「接收即消费、零响应」)+ 「协议中立」措辞作废 + 独立类目归类;「进 JSON / 不进 `protocol_instances.json` / 装配闸不判红 / 不计入 B3b」四项结论**逐字不变**
- [ ] ③ 的**防误读句**已逐字登记,且**要求**同时进契约面 delta 与台账
- [ ] ③ 的声明面二选一(甲 / 乙)已登记为**待裁**,⛔ **未预裁**
- [ ] ④ 的「A 零延迟」理由(**判别力为主 / 断言纯度为次**) + **J-9** 判据 + 两条实施契约(`[IAG-02]` 的 `protocol_instance` / 只读桥指针访问器)已登记;B 不承载全绿的边界保留
- [ ] ⛔ 本段**无**任何落地宣称(⛔ 决策登记 ≠ 落地);`test/cep_b3b2_deferrals.json` **零 diff**;**`B3b-2` 维持 0/4**
- [ ] 冻结面两步授权已执行:台账 **Entry #292** 授权 → **commit 本段 + `07 号` SU · 十二 + `ADR-CEP-12` SU** → `cep_freeze_check.sh --update --authorized-by "Entry #292"` → **清单单独 commit** → 无参复验 provenance 通过

**关联**:台账 **Entry #292**(授权)· **#291**(两条用户裁决 + 落地义务清单)· **#288**(12 条决策 + 授权)· **#287**(下一步任务推荐)· **#285**(架构文档审查)· `.rddf/reviews/cep-arch-planner-review-findings-2026-11-08.md`(F-01 / F-04 / F-05 / F-11)· `07 号` **SU · 十一** / **SU · 十二** · `ADR-CEP-12` SU 2026-11-08 · `ADR-CEP-05` 决策 2 · `ADR-CEP-07` 决策 3 · `openspec/specs/cep-stub-cache-core/spec.md`(`:56` / `:63` / `:71`)· `test/test_cep_stub_cache_core.cc`(`INV-SC6` 计数断言)· `include/tlm/cep/protocol_instance.schema.json`

