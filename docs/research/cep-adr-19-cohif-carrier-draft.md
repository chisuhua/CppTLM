# ADR-CEP-19 草案(待签发):CohIf 载体与 ProtocolBridge 特化

> **状态**: 🟡 **草案 —— 未签发**
> **版本**: v2(2026-10-26;per critic 第 1 轮 REJECT 全落地 —— 装配路径从 programmatic 改为 JSON 链 + 5 流界定 + 显式偏离登记 + 不动 StubCacheCore)
> **拟签发路径**: `docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md`
> **作者**: CppTLM architect
> **⚠️ 签发前置**:
> ① `obj §11` **先有**台账 Entry(本轮 architect 重审 + 本 ADR)
> ② 两步授权(commit 产物 → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → commit 清单)—— 因 `FROZEN_GLOBS` 含 `docs/adr/ADR-CEP-*.md`,新增文件即 M1 `drift`
> ③ `ADR-CEP-NUMBER-MAP` 清单 21 → 22 + KPI 预算 13 → 14
> ④ **先签发本 ADR,再改 `06 号`**(否则构成「用未签发 ADR 为改真源背书」的循环)
> **依据**: [`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`](cep-cohif-carrier-architecture-review-2026-10-26.md)(逐 `file:line` 核实)

---

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

- 审查依据(逐 `file:line` 核实):`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v2)
- 队列 #6:`openspec/changes/cpptlm-cep-moesi-gpu-adapter/`
- 架构基线:`01 号` · `06 号` · `06-bis` · `07 号`
- 契约:`ADR-CEP-{00,02,03,05,07,07A,12,13,15,16,17,18}-*.md` · `ADR-CEP-NUMBER-MAP.md`
- 偏离登记格式先例:`ADR-CEP-03` Status Update 2026-10-26
- critic 第 1 轮评审(REJECT:2 B + 10 S + 7 N)⇒ 全部落地
- critic 第 2 轮复审判定 **APPROVE_WITH_CONDITIONS**(无 BLOCKER:2 个 BLOCKER 经逐行读码确认真修、10 S + 7 N 全部落地;新增 3 S + 2 N)⇒ 本轮已修完
- **决策 ↔ 审查报告 D 编号的映射表**:见审查报告 §5 开头(**不是**同号对应:决策 1↔D-5 / 决策 3↔D-1 / 决策 4↔D-3 / 决策 5↔D-2 / 决策 6↔D-4 / 决策 7↔D-6 / 决策 9↔D-9;决策 2 / 8 是横贯约束;D-7 / D-8 是治理动作、不进本 ADR)
