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

## 7. 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
>
> - **v1.0(2026-10-27)**:签发。draft 阶段的 3 轮 critic 评审闭环(REJECT 2 B + 10 S + 7 N → APPROVE_WITH_CONDITIONS ×3 → `mergeable: yes`)。
