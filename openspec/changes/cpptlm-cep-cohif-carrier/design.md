# cpptlm-cep-cohif-carrier — 设计(Design)

> **阶段**: planner(2026-10-27)→ **已转正**(2026-10-27,Entry #81)· **状态**: 🟡 **Proposed** · **队列**: `obj-cep-platform §4.0` #7
> **转正依据**:队列 #6 归档释放 slot(`openspec list --json` 实测 0 个 active,per Entry #80)⇒ 本目录已由 `.rddf/drafts/` 迁入 `openspec/changes/`。**本文件是 planner 阶段的裁决落点** —— 每个 `D-` 段是一次**已拍板**的语义澄清或范围界定;`tasks.md` 属 builder 阶段(已建,见同目录)。
> **上游决策**: `ADR-CEP-19`(✅ v1.0 已签发)决策 1~9 ↔ 本文件 D-1~D-9 的**映射表**见文末。

---

## D-α:Bundle 形态 —— **单一类型**,**只覆盖 5 条 RN-side 流**

**裁决**:`CohIfBundle` 是**一个** Bundle 类型,`flow_id` 区分流。

| 决策点 | 取值 | 理由 |
|--------|------|------|
| **覆盖几流?** | **5 条 RN-side 流**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`) | canonical **10** 流中,另 5 条(`forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack`)**在 ProtocolEngine 内部终结或对接 Directory & Ordering Plane**(per `07 号 §3.2` + `ADR-CEP-05` 折叠映射表 + `ADR-CEP-02` CEN-Ib8)⇒ **不跨 Stub↔PE 边界** |
| **单一类型 vs 每流一类?** | **单一类型** | ① `06-bis §3.2` 实证「现有 Step 7a 装配路径不支撑 4 类 Bundle 4 端口」—— 单一类型 ⇒ Bundle 类型数 = 1 ⇒ 该死结消解;② 范式已验证:`router_tlm.hh:136-142` 用 `Input/OutputStreamAdapter<NoCFlitBundle>` + `OutputStreamAdapter` + `BidirectionalPortAdapter<RouterTLM, NoCFlitBundle, N>`;③ 「`barrier` / `forward_data` / `back_invalidate` 对 stub 不可见」由此从**运行期纪律**升级为**类型不可表达** |
| **`flow_id` 的域** | **恰好 5 个值** | 域外值 ⇒ 编译/运行期判红(CC-01) |
| **字段集含网络层字段?** | **不含**(`src_node` / `dst_node` / `flit_category` / `vc_id` 一律无) | 网络层字段属于网络 Bundle(per `06 号 §3.1` 字段维度判定)⇒ CohIf Bundle 是**业务语义** |
| **演进纪律?** | **网络级**(`BUNDLE_FIELD_WIDTH` 锁 + 扩展走 OpenSpec change) | 它是**跨模块共享契约**(Stub / PE / Directory 三方)⇒ 取网络侧纪律(per D-1) |

---

## D-β:装配 —— **JSON 链**,**不**走 programmatic bridge

**裁决**:走既有 JSON 端口注册路径;**不做** programmatic 端口注入。

| 决策点 | 取值 | 理由 |
|--------|------|------|
| **装配路径** | **JSON 链** | ① **冻结契约排除**:`ADR-CEP-16:46`「**不**实现 programmatic bridge 的 C++ 路径」+ `:86` 决策 5 排除表同款;② **零先例**:实测 `grep -rn "new cpptlm::ChStreamInitiatorPort\|new cpptlm::ChStreamTargetPort" src/ include/ \| grep -v module_factory` → **空**(ChStream 端口只在 `module_factory.cc` Step 7 内 new);③ **JSON 链全程实测通**:`chstream_adapter_factory.hh:103` → `:108` `port_count_ = N` → `:59-61` `isMultiPort`(`port_count > 1`)→ `module_factory.cc:668-669` → **`:743-750` `is_multi` 分支 = 泛型虚分派 `bind_port_pair`**(硬编码 cast 只在 `is_dual` 分支 `:727-741`)⇒ **零新增 StreamAdapter 子类** |
| **注册宏** | 新增 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` | 与 `chstream_register.hh:175` 的 `REGISTER_CHSTREAM_DUAL` **对称**(命名 + 形态),但**不**照抄其**半截实现** —— 该宏**全仓零调用点**且**只注册 adapter、不含 `registerObject`**(NICTLM 的真实落点是内联在无参批量宏体内 `:63` + `:121-122`)⇒ 新宏**两件都做**(缺 `registerObject` ⇒ ModuleFactory 无法实例化该类型) |
| **`static_assert(n > 1)`** | **必须有** | `isMultiPort` 要求 `port_count > 1`;**N=1** 会落 single 分支调 `bind_ports` —— 而 `bidirectional_port_adapter.hh:141` 的 `bind_ports` 是**空操作 shim** ⇒ **静默不绑定** |
| **端口数 N** | **逐 PE 实例**决定 | 插件族每实例端口数不同 ⇒ 宏的 `n` 是模板参数 |

---

## D-γ:特化桥 —— `ProtocolBridge` 双层 + 钩子扩展(**显式偏离** `06 号 §11.2` 的 P3 子项)

**裁决**:采纳 `06 号 §4.4` 的类形态;扩展 `module_factory.hh:313-315` 的钩子签名。

| 决策点 | 取值 | 理由 |
|--------|------|------|
| **类形态** | `ProtocolBridgeEr`(类型擦除基类,`ChStreamModuleBase` 持 `ProtocolBridgeEr*`)+ `ProtocolBridge<InB, OutB, Ordered>`(模板子类)+ `AbstractProtocolBridge`(identity 基线) | per `06 号 §4.4` 原型,**首次落地**;`Ordered` 单 bool(gem5 `MessageBuffer::m_strict_fifo(p.ordered)` 单参数对位,per `06 号 §4.4`) |
| **钩子扩展形态** | `register_protocol_bridge(name, factory_lambda)` | per `06 号 §9.3`「扩展既有 register 入口,**不重发明**」;现为 **write-only 死钩子**(`get_bridges()` 在 `private:` 段 `:326`,全仓无人读) |
| **与 `06 号 §11.2` P3 子项(「拒绝 ProtocolBridgeFactory 模式」)** | **遵从 §9.3 + 显式偏离 P3.2** | ⚠️ v1 曾写「两条都满足」= **文字游戏**(lambda 本身就是 name→模式 的运行时解析)。**诚实表述**:P3.2 拒绝的是**独立 factory 抽象层**(与 `01 号 §1.1`「不需新增抽象层」一致);§9.3 的**钩子签名扩展**不产生抽象层 ⇒ 二者目标一致但字面冲突,**以 §9.3 为准 + 登记偏离**(已在 `06 号 §11.2` 落地) |
| **per-bridge 元数据** | `protocol_id_` / `bridge_id_`(per-bridge 静态) | per `06 号 §4.5` 的 P1 范围;per-packet `tlm_extension` = 后续阶段,**不在**本 change |
| **不引入** | 新 StreamAdapter 子类 / factory 抽象类 / 协议语义基类 | per `01 号 §1.1` + `ADR-CEP-19` 决策 8 |

---

## D-δ:Bundle 路径 —— 避开 `coherence_bundles_tlm.hh`

**裁决**:`include/bundles/cep_coherence_bundles_tlm.hh`。

| 决策点 | 取值 | 理由 |
|--------|------|------|
| **路径** | `include/bundles/cep_coherence_bundles_tlm.hh` | `ADR-CEP-07A` 决策 1 第 3 项明令「**不创建** `include/bundles/coherence_bundles_tlm.hh`」⇒ 该路径不可用(ADR 不可变,只能绕);命名风格与 `ADR-CEP-03` Status Update 2026-10-26 登记的 collective 未来路径(`include/bundles/collective_bundles_tlm.hh`)**同构** |
| **命名空间** | `bundles::` | per `06 号 §3.2` 的二分类表(网络/业务 Bundle 均在 `bundles::`)+ `include/bundles/` 既有约定 |
| **是否进 `06 号 §3.2` 归类表?** | **是**(builder 同步) | 归类表在真源 1(`06 号`),**非冻结集** ⇒ builder 阶段同步即可(第三类 = 跨模块共享的业务 Bundle) |

---

## D-ε:ProtocolEngine 端口面的形态

**裁决**:`ChStreamModuleBase` 派生 + `InputStreamAdapter<CohIfBundle>` / `OutputStreamAdapter<CohIfBundle>` 成员。

| 决策点 | 取值 | 理由 |
|--------|------|------|
| **基类** | `ChStreamModuleBase` | per `ADR-CEP-07` 决策 3(ProtocolEngine 派生类 = `ChStreamModuleBase`)+ `01 号 §4.2`「模块内部统一 ch_stream 握手」 |
| **端口成员** | `Input/OutputStreamAdapter<CohIfBundle>`(照 `router_tlm.hh:139-141` 范式) | 1 个 Bundle 类型 ⇒ N 个端口**共用**同一对适配器类型 |
| **方向语义** | `req_in[i]` = ingress / `resp_out[i]` = egress;**流的语义方向由 `flow_id` 决定**,不由端口方向决定 | `BidirectionalPortAdapter` 的 `req_in`/`resp_out` 是**通用 ingress/egress**(per critic 判定:「命名层而非功能层」);`snoop` / `back_invalidate` / `writeback` 等**单向流**的语义落在 `flow_id` + 契约测试上 |
| **5/10 流分界** | **类型层**:`CohIfBundle.flow_id` 域 = 5 ⇒ `barrier` / `forward_data` / `back_invalidate` / `address_resolve` / `snoop_completion_ack` **不可能**出现在 stub↔PE 腿上 | per `07 号 §3.2` + `ADR-CEP-05` 折叠映射表;⇒ 「对 stub 不可见」是**类型保证** |

---

## D-ζ:Stub↔PE 腿 —— **不动 `StubCacheCore`**(显式延期)

**裁决**:本 change **不**给 `StubCacheCore` 加 ChStream 端口面;该腿归 `cpptlm-cep-stub-cache-chstream-surface`。

**理由(四条互斥,逐 `file:line` 实测)**:

| # | 事实 | 出处 |
|---|------|------|
| 1 | `openspec/specs/cep-stub-cache-core/spec.md` 在 **M1 冻结集**内 | `cep_freeze_check.sh:70` |
| 2 | 该 spec:「`net_side` SHALL 由 **mock ProtocolEngine** 驱动(本 change **不**引入真实协议引擎)」+ 三个流名「SHALL **不**出现作为 stub 的**对外接口**」 | `openspec/specs/cep-stub-cache-core/spec.md` |
| 3 | 「**不**经 `ModuleFactory` 注册(本类是 **testbench 定位**的组件)」 | `include/tlm/cep/stub_cache_core.hh:39-40` |
| 4 | **应该** `registerObject` + `ChStreamAdapterFactory::create` | `ADR-CEP-07:67` / `07 号 §6.3` |

⇒ 解它需 MODIFIES 一份**冻结 main spec**(两步授权 + 台账 Entry)⇒ **不应塞进载体队列**。

**延期路径(具体,不写「以后再说」)**:排本 change 之后;其**前置** = 本 change 交付 `CohIfBundle`(提供 stub 侧需要的 5 流类型)。

---

## D-η:判据面 —— **6 条**,全部配**可执行**反向 fixture

| ID | 判据 | 归属 capability | 反向 fixture(**可执行形态**) |
|----|------|----------------|--------------------------|
| **CC-01** | `CohIfBundle` **只含 5 流**且**不含**网络层字段 | `cep-cohif-carrier` | ① 加第 6 个 `flow_id` ⇒ **静态扫描用例红**(参照 `cep-stub-cache-core` spec 的「三流名不出现」先例);② 加网络层字段 ⇒ 同上 |
| **CC-02** | `BUNDLE_FIELD_WIDTH` 锁 + `is_trivially_copyable` **生效** | `cep-cohif-carrier` | **test TU 内** `static_assert(sizeof(CohIfBundle) == 期望)` + `static_assert(std::is_trivially_copyable_v<CohIfBundle>)`;外加 `try_compile` 一个「字段宽度篡改为 +1」的**毒化副本** ⇒ 编译失败即通过(⚠️ 编译期锁**不能**只写「编译红」当 ctest 判据) |
| **CC-04** | JSON 装配后,一次 CohIf 事务可**穿过** adapter 抵达对端;**N ≥ 2** | `cep-cohif-carrier` | ① 断链 ⇒ 红;② **N=1** ⇒ 宏 `static_assert` 编译红(防 `bind_ports` 空 shim 静默不绑定) |
| **CC-03** | `ProtocolBridge` 的 `pack`/`unpack` 对 `CohIfBundle ↔ NoCFlitBundle` **双向可逆**(逐字段 round-trip 恒等) | `cep-protocol-bridge-contracts` | 篡改任一字段映射(如把 `perm` 映射到 `flit_category`)⇒ 用例红 |
| **CC-05** | `register_protocol_bridge` 从**死钩子**变**活钩子**:注册后 `get_bridges()` 可读**且**被 ModuleFactory **消费**;per-bridge 元数据齐备(`protocol_id_` 非零 / `bridge_id_` 唯一 / 与实例声明 `family` 一致) | `cep-protocol-bridge-contracts` | 只注册不消费 ⇒ 红(须**同时**交付消费方,否则是新的死钩子);重复 `bridge_id_` ⇒ 红;`protocol_id_` 与 `family` 不符 ⇒ 红 |
| **CC-06** | 6→3 映射表产出的 `CATEGORY_*` **全部落在**网络 Bundle 的 3 个常量内 | `cep-protocol-bridge-contracts` | **引用队列 #6 的 AC-04**(`openspec/changes/archive/2026-10-27-cpptlm-cep-moesi-gpu-adapter/design.md:178`「目标写成第 4 类 ⇒ 红」)—— **不重立**(避免两份判据漂移) |

> **⚠️ 判据口径:编号对齐 ≠ 定义对齐(critic 首轮 `S1`/`S2` 收口,本块是全仓唯一真源)**
>
> - **`ADR-CEP-19` 全文对 `CC-0x` 只有 3 处编号引用**(`:167` / `:168` / `:205`),**不含任何判据定义** ⇒ 本表**不是**「与 ADR 一一对应」;
> - **判据定义真源** = `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md §6.4`(`L369-374`,`docs/research/` 是**附属层**)⇒ **真源 3(ADR)不自足**,属**真源层级缺陷**,诚实登记而非掩盖。
>
> **2 处为 planner 增补(与 research §6.4 字面不符)**:
>
> | 增补 | 内容 | 上游锚点 | 分类 |
> |------|------|---------|------|
> | **CC-05 子条件 ④** | per-bridge 元数据齐备 —— `protocol_id_` 非零 / `bridge_id_` 唯一 / 与实例声明 `family` 一致 | ✅ `ADR-CEP-19 §3.2`(`:157`「per-bridge 元数据**只做静态**」)+ `06 号 §4.5`(P1 范围) | **判据化已有代价项**(非新决策) |
> | **CC-06 子条件 ②** | 映射表 SHALL **非恒等**(至少一行 ≠ 同名) | ❌ **无上游锚点** —— 仅「特化字典存在理由」的逻辑推论 | **纯 planner 推论** |
>
> **属细化、不改语义(critic 判定「不动」)**:CC-01 ③(5 个域外流名**类型不可表达**)· CC-02 ③ + 加虚函数 fixture · CC-06 ③(引用队列 #6 的 AC-04 不重立)。
>
> **追认路径(`D-κ` 步 0.5 的转正硬前置)—— ✅ 已落地(出路 ①,2026-10-27)**:architect 给 `ADR-CEP-19` 追加 `## Status Update 2026-10-27` 追认该 2 处(**§1~§7 正文零改动** ⇒ ADR 不可变纪律成立;冻结面走两步授权,per Entry #78)。⇒ 口径现状:**「判据编号」与本 ADR 一致(CC-01~CC-06),「判据定义」由本 change 的 delta spec 承载并已进入追认范围**;`ADR-CEP-19` 的追加段在归档后并入 `openspec/specs/` 时退为历史沿革(真源自足化)。
>
> **真源自足的收口**:转正归档后 `openspec archive` 把 §6.4 的判据**并入 main specs** ⇒ 判据进入真源层(首选);或由 architect 追加 ADR Status Update 固化。
>
> **本轮初稿的自查记录**:曾擅自新增 `CC-04b`(per-bridge 元数据独立成条),**已删除**,降为 CC-05 的子条件 —— 编号集合仍严格 = CC-01~CC-06。

> **纪律**(队列 #4 `Entry #65` / 队列 #5 `Entry #68`):**新断言必须有反向 fixture**;**纯负向断言必须有正控**;**判据实现退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**。

---

## D-θ:B2 贡献 —— **新增 invariant 数 = 0**

`test/test_cep_canonical_invariants.cc::expected_groups()` 把 **15 组 66 条**编号**硬编码**;`ADR-CEP-01` / `ADR-CEP-03` 在 15 组里**无组** ⇒ 新增编号即契约违反。

⇒ 本 change 的 B2 贡献 = **行为化既有条目** + **契约层加固**:

| 既有条目 | 行为化方式 |
|---------|-----------|
| `INV-CEP-05-IC1` | Stub 只暴露稳定状态 ⇒ **行为化**:`CohIfBundle` 的 5 流**不含**瞬态态信息(不泄漏 `T`) |
| `INV-CEP-12-IC1` | `protocol_version` 落在锁表内 ⇒ **行为化**:PE 端口面的 `protocol_id_` 与实例声明对齐 |
| `INV-CEP-12-IC2` | per-network 三条件校验 ⇒ **行为化**:PE 装配期逐 network 校验 `vc_binding` / `ordering_assumption` |
| `INV-CEP-12-IC3` | 仅 `implemented` 计入 adapter 实例数 ⇒ **行为化**:载体交付 ⇒ 队列 #6 的 `status` 翻转后计数 0 → 1 |

⇒ `test_cep_canonical_invariants.cc` **零 diff**;`ADR-CEP-19` 登记进 `cep_docs_sync_check.sh` 的 `NO_INV` **与** `test/cep_adr_parser.hh` 的 `no_inv_adr_files()`(**两处双写规则,必须同批改** —— 本轮已因只改脚本侧导致 ctest 2 项失败)。

---

## D-ι:范围切小 —— 本 change **不做**什么

| 不做 | 理由 |
|---|---|
| 协议语义建模(6×6 表的内容) | 归 ArchForge |
| 各协议实例的实现 | 队列 #6 / #8 / #9 |
| 跨域组合 | `ADR-CEP-13` |
| per-packet `tlm_extension` | `06 号 §4.5` = 后续阶段 |
| `coherence_bundles_tlm.hh` 路径 | `ADR-CEP-07A` |
| **动 `StubCacheCore`** | D-ζ |
| 新增 StreamAdapter 子类 / factory 抽象层 / 协议语义基类 | `01 号 §1.1` |
| 新增 invariant 编号 | D-θ |
| `06 号 §3.2` 归类表的同步 | 属 **builder** 阶段(真源 1,非冻结集) |

---

## D-κ:转正路径 —— slot 释放后的动作序列

| 步 | 动作 | 判据 |
|----|------|------|
| 0 | `openspec list --json` 实测 CEP 内部 active == **0** | ✅ **已满足**(2026-10-27 实测 0 个 active;队列 #6 已归档,Entry #80) |
| **0.5** | **判据增补追认落地** —— 走 ①(architect 追加 `## Status Update`) | ✅ **已落地**(`ADR-CEP-19` Status Update 2026-10-27;Entry #78,冻结面两步授权完成) |
| 1 | `git mv .rddf/drafts/cpptlm-cep-cohif-carrier openspec/changes/cpptlm-cep-cohif-carrier` | ✅ 目录就位 |
| 2 | 删各文件顶部的「未建 change 目录」标注段;状态行改 🟡 Proposed | ✅ 已删/已改 |
| 3 | `openspec validate cpptlm-cep-cohif-carrier --strict` | ✅ rc=0 |
| 4 | 台账追加 Entry(转正 + KPI 0/3 → 1/3) | ✅ **Entry #81** |
| 5 | `obj §4.0` 状态列 📋 Planned → 🟡 | ✅ 已改(`§4.2` 同义复述同步) |
| 6 | `phase-10.md` 对应子任务行同步 + `obj §5.2` KPI 回填 | ✅ 已同步 |

---

## D-λ:决策 ↔ `ADR-CEP-19` 编号映射

⚠️ **两侧不是同号对应**,审计时**不得**假设「决策 N ↔ D-N」:

| 本文件 | `ADR-CEP-19` 决策 | 主题 |
|--------|------------------|------|
| **D-α** | 决策 **2** | Bundle 单一类型 + 5 流界定 |
| **D-β** | 决策 **5** | 装配走 JSON 链 |
| **D-γ** | 决策 **4** | 特化桥 + 钩子扩展(显式偏离) |
| **D-δ** | 决策 **6** | Bundle 路径 |
| **D-ε** | 决策 **1**(交付面第 2 项) | ProtocolEngine 端口面形态 |
| **D-ζ** | 决策 **9** | 不动 `StubCacheCore` |
| **D-η** | 决策 **1** 的判据面 | CC-01~CC-06 |
| **D-θ** | 决策 **8** | 不引入新抽象层 / 新增 invariant 数 = 0 |
| **D-ι / D-κ** | 决策 **1** / 决策 **7** 的落地侧 | 范围切小 / 转正路径 |

> 完整映射(9 决策 ↔ 9 D)**以 `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md` §5 的表为准**;`ADR-CEP-19` §6 是决策侧的汇总。

---

## 关联

- 上游决策:`docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md`(✅ v1.0 已签发)
- 架构重审:`docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3)
- 规划输入:`.rddf/improvements/cpptlm-cep-cohif-carrier.md`
- 真源:`01 号` · `06 号` · `06-bis` · `07 号`
- 契约:`ADR-CEP-{00,02,03,05,07,07A,12,13,15,16,17,18}-*.md`
