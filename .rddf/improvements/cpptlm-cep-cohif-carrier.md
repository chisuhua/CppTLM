# improvement: cpptlm-cep-cohif-carrier

> **状态**: 📋 **规划输入**(planner 阶段,2026-10-27)· **change 目录未建**(KPI slot 硬前置未满足,见 §「阻塞」)
> **队列**: `obj-cep-platform §4.0` 队列 **#7**(Stage 1 / 2028-Q2)· 排在 #6 `cpptlm-cep-moesi-gpu-adapter` 之后、`#8 cpptlm-cep-chi-rnf-subset` 之前
> **关联 ADR**: **`ADR-CEP-19`**(✅ v1.0 已签发 2026-10-27,per 台账 Entry #74)· `ADR-CEP-07`(集成 seam)· `ADR-CEP-12`(实例声明)· `ADR-CEP-16`(programmatic 排除)· `ADR-CEP-07A`(Bundle 路径处置)
> **架构依据**: `docs/research/cep-cohif-carrier-architecture-review-2026-10-26.md`(v3)· 三处断点 C1 / C2 / C3 全部带 `file:line`
> **估时**: 6 周(3 项交付 + 1 宏 + 1 钩子扩展;`StubCacheCore` 侧端点已延期)

---

## Why

CohIf(canonical 10 流,`ADR-CEP-02`)至今**没有承载形态** —— 而 `01 号 §1.1` 定义的机制(L3 ChStream + L4 StreamAdapter + L5 Bundle)早已完备可用。三个断点(逐 `file:line` 实测):

| # | 断点 | 证据 |
|---|------|------|
| **C1** | **CEP 零 ChStream 承载体** | `grep -rl "bundles::\|Bundle\|ChStream\|StreamAdapter" include/tlm/cep/` → **空**(7 个 `.hh`);`stub_cache_core.hh:154` 实为普通 `class StubCacheCore` —— 而 `ADR-CEP-07:64/67` + `07 号 §6.3` 明写 ProtocolEngine / StubCacheCore **都应是** `ChStreamModuleBase` |
| **C2** | **「canonical 10 流的 Bundle」是悬空引用,且同源两处** | `ADR-CEP-07A:49` + `ADR-CEP-16:45` 均写「CEP 用 canonical 10 流的 Bundle」;而 `ADR-CEP-02` 全文 `Bundle` **零命中**、`07 号 §5.4` 业务 Bundle 列表(7 项)**无它**、`include/bundles/` 17 个 `.hh` grep `coherence` **零命中** |
| **C3** | **唯一的 Bundle 化承诺绑在集合通信队列** | `ADR-CEP-03:225` 推迟至 P3(`cpptlm-cep-nvlink-sharp-adapter`),新建 `collective_bundles_tlm.hh` ⇒ **主干流无人认领** |

**为什么必须排进队列 #7,而不是留给 #6**:

- 队列 #6 `cpptlm-cep-moesi-gpu-adapter` 的「装配验证」当前**只能是逻辑级** —— `stub_cache_core.hh:42` 明写 `net_side` 由 **mock ProtocolEngine** 驱动 ⇒ 它的 `S1-2`(首个 `implemented` 实例装配后复核)**无法在真实链路上验证**;
- 若载体排在 #8 之后 ⇒ **#8 继承 #6 的无载体装配模式** ⇒ **载体债复制一遍**;
- 载体队列**消费** #6 的 6→3 特化字典(该字典的目标 `CATEGORY_*` 3 类**就定义在**网络 Bundle 自己的头文件 `noc_bundles_tlm.hh:60-62`)=> **两者互为对方的验证条件**。

**与用户两个架构目标的对应**:

| 目标 | 闭合方式 |
|---|---|
| ① 最终由 **ch_stream 承载业务类 bundle** | `CohIfBundle`(业务语义)走 `ChStreamModuleBase` 的 `Input/OutputStreamAdapter` |
| ② **互联网络可特化到具体通信网络** | `ProtocolBridge<CohIfBundle, NoCFlitBundle>` = `01 号 §1.1` 的 **L5↔L4 seam**;换 `Axi4Bundle` 即换一种网络 |

---

## What Changes

### 新增

| 文件 | 作用 |
|------|------|
| `include/bundles/cep_coherence_bundles_tlm.hh` | **`bundles::CohIfBundle`** —— 业务 Bundle,**只覆盖 5 条 RN-side 流**(`request` / `completion` / `snoop` / `snoop_resp` / `writeback`),以 `flow_id` 区分;**无网络层字段**;`bundle_base` + `ch_uint`/`ch_bool` + `BUNDLE_FIELD_WIDTH` 编译期锁 + `is_trivially_copyable` |
| `include/tlm/cep/protocol_engine_base.hh` | **ProtocolEngine 基类** —— `ChStreamModuleBase` 派生 + `InputStreamAdapter<CohIfBundle>` / `OutputStreamAdapter<CohIfBundle>`;持 `ProtocolBridgeEr*` |
| `include/framework/protocol_bridge.hh` | **特化桥** —— `ProtocolBridgeEr`(类型擦除基类)+ `ProtocolBridge<InB, OutB, Ordered>`(模板子类)+ `AbstractProtocolBridge`(identity 基线);**首次落地**(per `06 号 §4.4`) |

### 修改

| 文件 | 改什么 |
|------|--------|
| `include/chstream_register.hh` | 新增 `REGISTER_CHSTREAM_BIDIRECTIONAL(mod_type, bundle_t, n)` 宏 —— **同时**做 `ModuleFactory::registerObject<mod_type>` **与** `ChStreamAdapterFactory::registerBidirectionalPortAdapter<...>`;宏内 `static_assert(n > 1)`。⚠️ **不**照抄 `REGISTER_CHSTREAM_DUAL`(`:175`)的半截形态 —— 该宏**全仓零调用点**且**只注册 adapter、不含 `registerObject`** |
| `include/core/module_factory.hh:313-315` | `register_protocol_bridge` 签名扩展为 `(bridge_name, factory_lambda)` —— 现为 **write-only 死钩子**(`get_bridges()` 在 `private:` 段 `:326`,全仓无人读);**不新建** factory 抽象类(per `01 号 §1.1`「不需新增抽象层」) |
| `docs/architecture/06-inter-module-network.md` | **3 处修订已在本轮落地**(per `ADR-CEP-19` 决策 3 / 4 / 7):§3.3 补第三类「跨模块共享的业务 Bundle」· §4.3 `flit_category` 4→3 值 + §3.2 Bundle 数 16→17 · §11.2/§11.3 时间线转隶 + **P3** 子项的偏离登记 |

### 冻结面

**本 change 零冻结面改动** —— 载体文件(`include/bundles/` · `include/framework/` · `include/tlm/cep/`)与守门脚本**均不在** M1 冻结集(per `cep_freeze_check.sh` 的 `FROZEN_GLOBS`:含 `docs/adr/ADR-CEP-*.md` / `docs/architecture/07-*.md` / 7 份 main spec / `fabric.json` / schemas / `tools/*.py`)。⇒ **无需两步授权**。

> **但 `ADR-CEP-19` 签发本身是冻结面改动** —— 已于 2026-10-27 走完两步授权(产物 `90877fd8` → 清单 `19342b79`,`authorized_by = Entry #74`,冻结集 **39 → 40**)。本 change 引用它,**不再改**。

---

## Non-Goals

| 不做 | 理由 |
|---|---|
| 协议语义建模(6×6 状态机的**内容**) | 归 ArchForge(`06-bis` 顶部处置表「CEP 侧只提供 adapter 载体 + 能力声明 schema」) |
| `moesi_gpu` / `chi-rnf` / `mesif` 等实例的**实现** | 队列 #6 / #8 / #9 |
| 跨域组合(多协议混合) | `ADR-CEP-13`,走「人工声明 + 实验证伪」路线 |
| per-packet `tlm_extension` 元数据 | `06 号 §4.5` = 后续阶段;本 change 只做 per-bridge 静态(`protocol_id_` / `bridge_id_`) |
| `coherence_bundles_tlm.hh` 路径 | `ADR-CEP-07A` 决策 1 第 3 项明令「**不创建**」⇒ 回避(per `ADR-CEP-19` 决策 6) |
| **动 `StubCacheCore`** | 四条互斥(见下)⇒ 决策 9 显式延期到 `cpptlm-cep-stub-cache-chstream-surface` |
| 新增 StreamAdapter 子类 / factory 抽象层 / 协议语义基类 | `01 号 §1.1`「不需新增抽象层」+ `ADR-CEP-19` 决策 8 |
| **新增 invariant 编号** | `test_cep_canonical_invariants.cc` 硬编码 15 组 66 条;`ADR-CEP-01`/`ADR-CEP-03` 在 15 组里**无组** ⇒ 新增编号即契约违反。**本 change 新增 invariant 数 = 0** ⇒ `ADR-CEP-19` 已登记进 `cep_docs_sync_check.sh` 的 `NO_INV` 白名单 + `test/cep_adr_parser.hh` 的 `no_inv_adr_files()`(**两处双写规则**) |

**「不动 `StubCacheCore`」的四条互斥**(决策 9 的依据,逐 `file:line`):

1. `openspec/specs/cep-stub-cache-core/spec.md` 在 **M1 冻结集**内(`cep_freeze_check.sh:70`);
2. 该 spec 写「`net_side` SHALL 由 **mock ProtocolEngine** 驱动(本 change **不**引入真实协议引擎)」+ 三个流名「SHALL **不**出现作为 stub 的**对外接口**」;
3. `stub_cache_core.hh:39-40` 写「**不**经 `ModuleFactory` 注册(本类是 **testbench 定位**的组件)」;
4. `ADR-CEP-07:67` 写它**应该** `registerObject` + `ChStreamAdapterFactory::create`。

⇒ 解它需 MODIFIES 一份**冻结 main spec** ⇒ **不应塞进载体队列**。

---

## Acceptance

> **纪律**(队列 #4 `Entry #65` / 队列 #5 `Entry #68`):**守门/闸的新断言必须有反向 fixture**;**纯负向断言必须有正控**;**判据实现退化 = 用例红**;**判定结论为红 = 用例仍绿 + 结论作为交付物登记**。

### A. 载体行为面(`cep-cohif-carrier`,3 Scenario)

| ID | 判据 | 反向 fixture(可执行形态) |
|----|------|--------------------------|
| **CC-01** | `CohIfBundle` **只含 5 流**且**不含**网络层字段:`flow_id` 域枚举 == {request, completion, snoop, snoop_resp, writeback};字段集无 `src_node`/`dst_node`/`flit_category`/`vc_id` | ① 加第 6 个 `flow_id` ⇒ **静态扫描用例红**(参照 `cep-stub-cache-core` spec 的「三流名不出现」先例);② 加网络层字段 ⇒ 同上 |
| **CC-02** | `BUNDLE_FIELD_WIDTH` 锁 + `is_trivially_copyable` **生效** | **test TU 内** `static_assert(sizeof(CohIfBundle) == 期望)` + `static_assert(std::is_trivially_copyable_v<CohIfBundle>)`;外加 `try_compile` 一个「字段宽度篡改为 +1」的**毒化副本** ⇒ 编译失败即通过(⚠️ `BUNDLE_FIELD_WIDTH` 是**编译期锁**,不能只写「编译红」当 ctest 判据) |
| **CC-04** | JSON 装配后,一次 CohIf 事务可**穿过** adapter 抵达对端;**N ≥ 2** | ① 断链 ⇒ 红;② **N=1** ⇒ 宏 `static_assert` 编译红(防 `bind_ports` 空 shim 静默不绑定,`bidirectional_port_adapter.hh:141`) |

### B. 特化桥契约面(`cep-protocol-bridge-contracts`,3 Scenario)

| ID | 判据 | 反向 fixture |
|----|------|--------------|
| **CC-03** | `ProtocolBridge` 的 `pack`/`unpack` 对 `CohIfBundle ↔ NoCFlitBundle` **双向可逆**(逐字段 round-trip 恒等) | 篡改任一字段映射(如把 `perm` 映射到 `flit_category`)⇒ 用例红 |
| **CC-05** | `register_protocol_bridge` 从**死钩子**变**活钩子**:注册后 `get_bridges()` 可读**且**被 ModuleFactory **消费**;per-bridge 元数据齐备(`protocol_id_` 非零 / `bridge_id_` 唯一 / 与实例声明 `family` 一致) | 只注册不消费 ⇒ 红;重复 `bridge_id_` ⇒ 红;`protocol_id_` 与 `family` 不符 ⇒ 红 |
| **CC-06** | 6→3 映射表产出的 `CATEGORY_*` **全部落在**网络 Bundle 的 3 个常量内 | **引用队列 #6 的 AC-04**(`openspec/changes/cpptlm-cep-moesi-gpu-adapter/design.md:178`「目标写成第 4 类 ⇒ 红」)—— **不重立**(避免两份判据漂移) |

### C. 流程与治理

| 项 | 判据 |
|----|------|
| **判据实现单一** | 判据实现只在 `adapter_schema_contracts` / 载体基类**一份**,test 与 CLI **共用**;禁双实现 |
| **新增 invariant 数** | = **0** ⇒ `test_cep_canonical_invariants.cc` **零 diff** |
| **冻结面** | 本 change **零**冻结面改动 ⇒ 冻结集成员数**不变** |
| **KPI** | 本 change 占 1 slot ⇒ CEP 内部 Proposed 并发恒 **≤ 1** |
| **独立性缺口** | 本仓为单开发者环境,评审由 AI agent 完成 ⇒ **不等于真人外部评审**;结论登记为「残余风险 + DRI 书面接受」(per `obj §9.3` R10) |

> **⚠️ 6 条判据与 `ADR-CEP-19` 的 CC-01~CC-06 严格一一对应** —— planner **不得**给已签发的 ADR 增删判据(增删 = ADR 修订,须走冻结面两步授权)。本轮初稿曾擅自新增 `CC-04b`,**已删除**,并把 per-bridge 元数据降为 **CC-05 的子条件**。

> **判据无法在仓内验证的降级登记**:「单 `CohIfBundle` 承载 5 流的协议死锁自由度」在**无实现**时不可验证 ⇒ 登记为「残余风险 + DRI 书面接受」,**不**写成 CC 判据。

---

## Capabilities

### 新增 capability:`cep-cohif-carrier`

**载体面** —— `CohIfBundle` 的字段契约 + ProtocolEngine 的 ChStream 端口面 + JSON 装配连通性。

### 新增 capability:`cep-protocol-bridge-contracts`

**跨实例复用的平台级契约面** —— `ProtocolBridge` 的可逆性 + per-bridge 元数据 + 死钩子激活的「注册 + 消费成对」纪律。**后续 7 个 adapter change 复用**该机制,只填各自的行。

> **命名不与队列 #6 冲突**:队列 #6 已有 `cep-adapter-schema-contracts`(协议实例**声明层**的契约:`perm` / `perm_upgrade` / 6→3 映射表 / ⑥ 镜像闸)⇒ 本 change 用 `cep-protocol-bridge-contracts` 指**特化桥层**,两者**不同层、不同判据**。

---

## Impact

| 载体 | 内容 |
|------|------|
| **路线图** | 队列 #7 已在 `obj §4.0` / `§4.2` 就位(per Entry #74);本 change 的 change 目录**尚未建** ⇒ 状态列仍 **📋 Planned** |
| **架构真源** | `ADR-CEP-19`(已签发)· `06 号` 3 处修订(已落地)· `07 号 §8.1`(已落地)—— 本 change **不再改**这三处 |
| **框架层** | `chstream_register.hh` +1 宏 · `module_factory.hh` 钩子签名扩展 ⇒ 触 `module_factory.cc` 核心路径 ⇒ 需 6 标签回归 |
| **契约面** | `include/bundles/cep_coherence_bundles_tlm.hh`(新)+ `include/tlm/cep/protocol_engine_base.hh`(新)+ `include/framework/protocol_bridge.hh`(新)⇒ 需 `test_bundle_classification_contract` 归类表同步(**`06 号 §3.2` 的第三类** —— 由 builder 同步) |
| **KPI** | 占 1 slot ⇒ 需当前 slot 释放(**见「阻塞」**) |
| **延期项** | `cpptlm-cep-stub-cache-chstream-surface`(Stub↔PE 腿)—— 需 MODIFIES 冻结 main spec,排本 change 之后 |

---

## ⚠️ 阻塞:change 目录的创建受 KPI slot 硬前置约束

**约束**(`ADR-CEP-00 §2 决策 3` / `obj §5.4` / `obj §6.3 D1`):**任一时刻 CEP 系列 Proposed 并发 ≤ 1**。

**实测**(2026-10-27):`openspec list --json` = **1 个 active** = `cpptlm-cep-moesi-gpu-adapter`(队列 #6,🟡 Proposed)⇒ **slot 已被占用**。

**先例**(台账 `obj §11` 早期 entry):「补建 OpenSpec change | ❌ `cpptlm-cep-umbrella` 占**唯一** KPI slot 且 R10 未闭合 → 不 archive → 建新 change 令 CEP 内部 Proposed 并发 = 2,**违规**」。

**⇒ 处置**:
- 本轮(planner)**先出 improvement 草稿 + 4 件套草稿**(staged 于 `.rddf/drafts/cpptlm-cep-cohif-carrier/`,照 `.rddf/drafts/cpptlm-pcie-aer-ats-pri/` 先例)=> **不占 KPI slot**;
- **change 目录**(`openspec/changes/cpptlm-cep-cohif-carrier/`)的创建**必须**等队列 #6 归档释放 slot(per `obj §4.7.2` step 0「🔴 先确认 KPI slot 可用(硬前置,跳过此步必违规)」);
- **转正命令**(slot 释放后):`git mv .rddf/drafts/cpptlm-cep-cohif-carrier openspec/changes/cpptlm-cep-cohif-carrier` → 删各文件的「未建目录」标注 → `openspec validate cpptlm-cep-cohif-carrier --strict` → 台账追加 Entry。

---
