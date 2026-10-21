# CEP ↔ ArchForge 接口对齐清单

> **副题**:M2 跨仓准入门的**可执行前置** —— ArchForge 侧 coherence 接口 PR 的范围界定
> **日期**: 2026-10-21 · **作者**: CppTLM Team(builder mode)
> **契约依据**:`ADR-CEP-00 §2 Decision 3`(跨仓准入门)/ `ADR-CEP-12`(能力声明 schema)/
> `ADR-CEP-05`(Stub Cache 协议无关性)/ `ADR-CEP-15`(CacheTLM 边界)/
> `ADR-CEP-07A`(协议泄漏通用禁令)· **真源 1** `docs/architecture/07-coherence-exploration-platform.md`
> **关联**:`openspec/changes/cpptlm-cep-umbrella/`(gate ④)· `objective-cep-platform §9.2 M2` + `§11 Entry #7`

---

## 0. 一句话

**CEP 的协议实例声明 schema 目前在 ArchForge 侧**没有对应的承载物** —— ArchForge 的
coherence 接口是 **MOESI/GPU 单协议特化**的(`CoherenceState { I,S,E,M,O,T }` 顶层枚举 +
`enum class Protocol { MESI, MOESI }` 配置入口),而 CEP 要求**协议中立**(协议状态住
Plugin 内、Stub 只暴露 4 值 `perm`、能力以**数据**声明)。本清单界定** ArchForge 侧需要
改什么**,使 M2 准入门可放行。

---

## 1. 用途与读法

- **本清单是 ArchForge 侧 PR 的范围界定**,不是 CppTLM 侧的实现计划。
- §3 的每一项 gap 都带**两侧 file:line**,可独立复核。
- §5 的 6 个问题是**两仓无法自行裁决**的(需 DRI 向 ArchForge 提问)—— **在它们有答案前,
  §4 的 PR 范围只是初稿**。
- §6 是 M2 的**可检查放行判据**。

---

## 2. 两侧接口面清单

### 2.1 ArchForge 侧(`/workspace/main/ArchForge`)

| # | 接口面 | 关键声明 | 位置 |
|---|--------|----------|------|
| A1 | `CoherenceDomain`(已实现) | `enum class Protocol { MESI, MOESI }`;`set_protocol/set_members/set_snoop_fanout`;`is_member/get_snoop_targets/lookup_home_node`;`register_bridge/has_bridge_to/get_bridge`;`tick()` **空实现** | `docs/microarchitecture/coherence-domain.md:68-92` |
| A2 | `DomainRegistry` | `register_domain / domain_exists / get_domain / clear` | `docs/microarchitecture/coherence-domain.md:94` |
| A3 | `CoherenceDomain`(**目标** API) | `enum class State { I,S,E,M,O,T }`;`enum class Event { Read,Write,Invalidate,InvalidateAck,Flush,FlushAck }`;`transition / on_snoop / invalidate_range`;**`register_cache_callback(CacheTLM*)`** | `docs/architecture/09-coherence-protocol.md:240-250` |
| A4 | `CoherenceProtocol`(抽象基类) | `handle_probe(SnoopType, CoherenceState)`;`handle_request(CoherenceState, num_sharers)`;`can_coexist(a,b)`;`translate(SnoopProbe, ...)`;`get_protocol_name()` | `docs/microarchitecture/coherence-protocol.md:133-155` |
| A5 | 协议状态 / snoop 枚举 | `CoherenceState { I,S,E,M,O,T }`;`SnoopType { INVALIDATE, DOWNGRADE_TO_S, READ_SHARED, READ_EXCLUSIVE }`;`SnoopResp { ACK, NACK, DATA, DATA_AND_ACK }` | `docs/microarchitecture/coherence-protocol.md:112-131` |
| A6 | `CoherenceBridge` | 8 端口(`domain_a/b` × `req/resp/snoop_in/snoop_out`);`set_address_mapper`;`set_bridge_latency`;`enable_snoop_forwarding` | `docs/microarchitecture/coherence-bridge.md:135+`、`§2.2 端口表` |
| A7 | `SnoopFilter` | `struct SnoopEntry { addr; sharers; shared_state }`;`record_sharer / remove_sharer / get_sharers` | `docs/architecture/09-coherence-protocol.md:311-322` |
| A8 | `CoherentXBar` | 继承 `CrossbarTLM`;每端口带 `domain_id`;snoop 广播;Flush 协议 | `docs/architecture/09-coherence-protocol.md:8` 段(§8) |
| A9 | Bundle 字段 | `addr`;`transaction_id`;`type`;`source_protocol` / `target_protocol`;`requestor_cache_id`;`data` | `docs/microarchitecture/coherence-protocol.md:181-183`、`§6` |
| A10 | **配置 Schema** | `params.protocol = "moesi_gpu"`;`phase = 7_A/7_C/7_D`;`cpu_domain_id` / `gpu_domain_id`;`enable_bridge` / `bridge_iommu`;`enable_snoop_filter` / `snoop_filter_max_entries`;`enable_coherent_xbar` / `xbar_ports`;`cache_tlm_integration = "snoop_callback"`;`state_transition_table = "switch"` | `docs/architecture/09-coherence-protocol.md §11` |
| A11 | CppTLM 侧文件映射 | `CoherenceProtocol` **待新建** `include/core/coherence_protocol.hh`;`CoherenceBridge` **待新建**;`SnoopFilter` **待新建**;`CoherenceDomain` 已存在;`CoherentXBar` 已存在 | `docs/architecture/09-coherence-protocol.md §10.1` |

### 2.2 CppTLM CEP 侧(`include/tlm/cep/protocol_instance.schema.json`)

| # | 字段组 | 属性 | 契约位置 |
|---|--------|------|----------|
| C1 | 身份状态 | `id` / `family` / `adapter` / `status` / `protocol_version` | `ADR-CEP-12:163`(IC1)、`ADR-CEP-11 §3` 锁定表 |
| C2 | 能力面 · 流 | `flow_support`(**canonical 10 流** boolean 矩阵) | `ADR-CEP-02:55`(`CEN-Ib1 ~ CEN-Ib11`) |
| C3 | 能力面 · 权限 | `perm`(**4 值锁** `NONE`/`READ`/`UNIQUE`/`WRITEBACK`) | `ADR-CEP-02:66`(`CEN-Ib11`「维持 4 值锁」) |
| C4 | 能力面 · atomic | `atomic.supported` + `atomic.intent_field` | `ADR-CEP-02:66`、`ADR-CEP-17` 决策 6 |
| C5 | 前提假设 · 一致性 | `consistency_labels.read/write` + `request_level` | `ADR-CEP-13` 域归属 |
| C6 | 前提假设 · 排序 | `ordering_assumption`(network → `p2p_ordered`/`unordered`) | `ADR-CEP-12:168`(IC2) |
| C7 | 前提假设 · VC | `vc_binding`(消息类 → `vc_id` 0..3) | `ADR-CEP-12` 决策 4 |
| C8 | Sharer Tracker | `sharer_tracker.impl`(5 实现 +「不参与」)+ `caps` 4 维(`scope`/`completeness`/`authority`/`aggregation_at`) | `ADR-CEP-04 §2` |
| C9 | 组合能力 | `combination_capabilities.concurrent/hierarchical/heterogeneous` | 调研 F16 |

---

## 3. Gap 矩阵

> **性质**:`冲突` = 两侧对同一概念有**互斥**的约定;`缺失` = CEP 有该维度而 ArchForge 侧**无对应物**;
> `口径` = 两侧都有但**粒度/词表**不同。

| # | CEP 侧 | ArchForge 侧 | 性质 | 级别 | 处置 |
|---|--------|-------------|------|------|------|
| **G1** | 协议状态**住 Plugin 内**;Stub 只暴露 4 值 `perm`(`ADR-CEP-05:170` IC1) | `CoherenceState { I,S,E,M,O,T }` 是**顶层枚举**(`coherence-protocol.md:112`);`CoherenceDomain::State`(`09:242`) | **冲突** | **P0** | 状态枚举**下沉到协议插件**;或 ArchForge 侧出 ADR 豁免 `INV-CEP-05-IC1`(需 DRI 裁决) |
| **G2** | `enum class Protocol { MESI, MOESI }` 属**协议泄漏**,已 `[[deprecated]]`(`ADR-CEP-07A:98` IC1) | `coherence-domain.md:68` 的 API **仍以 `enum class Protocol` 为配置入口**(`set_protocol`) | **冲突**(直接违规) | **P0** | 配置入口改为**协议插件标识**(对齐 CEP `id`/`family`/`adapter`);`set_protocol` 废弃 |
| **G3** | **canonical 10 流**(`ADR-CEP-02:55`) | 4 类 Bundle:`CacheReqBundle`/`CacheRespBundle`/`SnoopProbe`/`SnoopResp`(`coherence-bridge.md §2.2`) | **口径** | **P0** | 产出 **4 类 Bundle ↔ 10 流** 的**映射表**(哪类流覆盖哪几条 CEP 流);CEP 明确「**不新增端口**」(`07 号 §5.1`) |
| **G4** | `ordering_assumption` + `vc_binding`(`ADR-CEP-12:168`) | **完全缺失** —— ArchForge 无 VC / ordering 概念 | **缺失** | **P1** | 决定:VC 归属哪一侧?若 ArchForge 侧承载 → 需声明形态;若 CppTLM 侧承载 → ArchForge 需提供 fabric 命名空间 |
| **G5** | `sharer_tracker.impl` 5 实现 + `caps` 4 维(`ADR-CEP-04 §2`) | 固定 `SnoopFilter`(`09:311`),**无选型 / 无 caps** | **冲突 + 缺失** | **P1** | `SnoopFilter` → **可插拔选型抽象** + caps 语义;`BroadcastNull`(token 类协议)需 ArchForge 侧认可 |
| **G6** | `consistency_labels`(SC/TSO/RC-*/PLO,请求级 `request_level`) | **完全缺失** | **缺失** | **P2** | Stage 2+ 再议(CEP 侧已可声明) |
| **G7** | `atomic.supported` + `intent_field`(意图经 request 流**独立属性**) | 无 atomic 概念 | **缺失** | **P2** | 需确认 ArchForge 的 `CacheReqBundle` 能否承载 atomic 意图位 |
| **G8** | `combination_capabilities` 三维正交 | 无跨协议组合概念 | **缺失** | **P2** | Stage 2+ |
| **G9** | 协议逻辑住 **Plugin**,能力以**数据**声明 | 协议逻辑 = ArchForge 侧 C++ 抽象基类 `CoherenceProtocol`(`coherence-protocol.md:133`) | **冲突**(分层) | **P0** | 明确**分工**:声明在 CppTLM(schema)/ 逻辑在 ArchForge(插件实现)?—— 这决定 `CoherenceProtocol` 是否需**下沉为 CppTLM 侧 Plugin 接口** |
| **G10** | `family` ∈ {`industrial`,`ruby-slicc`,`academic`};`07 号 §5.1` 口径 = 2 族 / 14 实例 | 只有 **MOESI/GPU 一族**(`09 §3` 6×6 状态机) | **口径** | **P1** | ArchForge 侧 `family` 落 `industrial`;Ruby 族由 CppTLM 侧插件提供 —— 需明确**族边界的权威方** |
| **G11** | `CacheTLM` **不得**实现 canonical 10 流;只允许经 **adapter** 交互(`ADR-CEP-15:115` IC3) | `register_cache_callback(CacheTLM*)`(`09:250`)**直连** CacheTLM;`cache_tlm_integration = "snoop_callback"`(配置) | **冲突**(边界) | **P0** | 明确 `snoop_callback` 落在**哪一侧**:若 ArchForge 侧直连 CacheTLM → 违反 IC3;须改走 adapter |
| **G12** | `status` 三值(`planned`/`implemented`/`excluded`)= 计数口径唯一依据(`ADR-CEP-12` 决策 9) | 无 status 概念 | **缺失**(小) | **P2** | 随 capability 声明一并引入 |
| **G13** | `perm` **4 值**(`NONE`/`READ`/`UNIQUE`/`WRITEBACK`) | `CoherenceState` **6 值**(`I,S,E,M,O,T`) | **口径** | **P1** | 产出 **6 态 → 4 值 perm 的投影表**;`O`(Owned)/`T`(Transient) **无投影**(瞬态态住插件内) |

**统计**:P0 = **6 项**(G1/G2/G3/G9/G11 + 决策依赖)· P1 = 4 项(G4/G5/G10/G13)· P2 = 4 项(G6/G7/G8/G12)。

---

## 4. ArchForge 侧 PR 范围(初稿)

> ⚠️ **待 §5 的 6 问有答案后定稿**。此处是**范围界定**,不是实施排期。

### P0 —— 放行 M2 的最小集

| 项 | 目标文件(ArchForge) | 改动 |
|----|--------------------|------|
| **P0-1** | `docs/microarchitecture/coherence-domain.md` + `docs/architecture/09-coherence-protocol.md` | **协议状态归属**决议:6 态枚举下沉到协议插件,或出显式豁免 ADR(G1)。这是其余 P0 项的**前置** |
| **P0-2** | `docs/microarchitecture/coherence-domain.md:68`(API)+ `include/core/coherence_domain.hh` | `enum class Protocol { MESI, MOESI }` 配置入口 → **协议插件标识**;`set_protocol` 废弃(G2) |
| **P0-3** | 新增 `docs/architecture/`(或 microarchitecture)一份 **Bundle ↔ canonical 10 流 映射表** | 4 类 Bundle → 10 流的覆盖关系;标出**无 CEP 流对应**的 ArchForge 能力(如 `Flush` 是否落在 `writeback` / `back_invalidate`)(G3) |
| **P0-4** | `docs/architecture/09-coherence-protocol.md §5.2`(`register_cache_callback`) | `snoop_callback` 的**归属侧**决议:ArchForge 直连 CacheTLM(违反 `INV-CEP-15-IC3`)vs 改走 **adapter**(G11) |
| **P0-5** | `docs/architecture/09-coherence-protocol.md §10.1`(待新建文件清单) | **`CoherenceProtocol` 的归属**决议:留在 ArchForge(CppTLM 侧只消费声明)vs 下沉为 CppTLM 侧 **ProtocolEngine Plugin 接口**(G9) |

### P1 —— Stage 1 启动前

| 项 | 目标 | 改动 |
|----|------|------|
| **P1-1** | ArchForge 侧 `vc_binding` / `ordering_assumption` 声明形态 | 承载方裁决 + 若承载则给形态(G4) |
| **P1-2** | `SnoopFilter` → 可插拔 `sharer_tracker` 选型 + caps 4 维 | 与 CEP `ADR-CEP-04 §2` 的 5 实现对齐(G5) |
| **P1-3** | `CoherenceState` 6 态 → CEP `perm` 4 值投影表 | `O`/`T` 无投影的显式说明(G13) |
| **P1-4** | `family` / `protocol_version` 归属 | ArchForge 侧 = `industrial`;规范版本走 `ADR-CEP-11` 锁定表(G10) |

### P2 —— Stage 2+

`consistency_labels` / `atomic.intent_field` / `combination_capabilities` / `status` 的
ArchForge 侧承载(G6/G7/G8/G12)。

---

## 5. 需 DRI 向 ArchForge 澄清的 6 问

> 这 6 问**两仓无法自行裁决** —— 它们决定 §4 的 PR 范围与 §6 的放行判据。
>
> **📨 提问单已提交 ArchForge**(2026-10-21):`/workspace/main/ArchForge/docs/roadmap/cep-interface-alignment.md`
> (ArchForge commit `901cf19`)—— 含 §5 全文 + 答复表 + 引用本清单。
> 该文件**已纳入本清单的漂移锚点**(ArchForge 侧 1 项)—— 若被移动或删除,CppTLM 侧会红。

| # | 问题 | 为何必须问 ArchForge | 影响 |
|---|------|----------------------|------|
| **Q1** | 6 态 `CoherenceState` 是**下沉到协议插件**,还是**保留在 ArchForge 顶层**? | 状态归属决定 CEP 的 `INV-CEP-05-IC1` 是否需要豁免 | G1 / P0-1 |
| **Q2** | `CoherenceProtocol` 抽象基类**留在 ArchForge**,还是**下沉为 CppTLM 侧 ProtocolEngine 插件接口**? | 决定 CEP 侧「Plugin 内」的实现位置 | G9 / P0-5 |
| **Q3** | `snoop_callback` 直连 CacheTLM 的路径**保留**还是**改走 adapter**? | `INV-CEP-15-IC3` 只允许 adapter 交互 | G11 / P0-4 |
| **Q4** | VC / 排序假设的**权威方**是 CppTLM 还是 ArchForge? | `vc_id` 命名空间必须单点权威,否则 `ordering_assumption` 的 key 无法解析 | G4 / P1-1 |
| **Q5** | 协议**族边界**的权威方?ArchForge 侧 = `industrial` 族,那 Ruby 族由 CppTLM 侧插件提供,两者的 adapter 边界在哪? | `07 号 §5.1` 的「族 = 来源集合」需要跨仓一致的划分 | G10 / P1-4 |
| **Q6** | ArchForge 侧**是否接受** CEP 的 `perm` 4 值作为**跨域接口**的稳定词表(而非 6 态)? | 跨域桥接双方需同一权限词表 | G13 / P1-3 |

---

## 6. M2 放行判据(可检查)

`ADR-CEP-00 §2 Decision 3` 的准入门在**当前只有文字条款**。建议把放行条件固化为
**下列 5 条**(**全部**满足才算放行):

| # | 判据 | 检查方式 |
|---|------|----------|
| **C-1** | ArchForge 侧**存在**一个已合入的 coherence 接口 PR(有 commit hash) | `git log` 在 ArchForge 仓可见 |
| **C-2** | 该 PR 含 **§4 P0-1 ~ P0-5** 的结论(逐项可追溯到 ArchForge 文档行) | 人工核对 + 本清单 §3 状态更新 |
| **C-3** | §5 的 **6 问**各有**书面结论**(ArchForge ADR 或 issue) | 人工核对 |
| **C-4** | CppTLM 侧 `protocol_instance.schema.json` 与 ArchForge 侧声明**能双向映射**(每个 schema 字段在 ArchForge 侧有承载物,或显式标注「CppTLM 侧独占」) | **新增机器检查**(见 §7) |
| **C-5** | 复审:`docs/implementation/2026-10-21-cep-stage0-contracts.md §7` 的 **10 组反向验证** 仍全绿(M1 冻结校验 + lint --strict + ctest) | 已自动化 |

> **判据的诚实边界**:C-1 ~ C-3 依赖**外部仓的人工核对**,无法在本仓自动化 —— 这正是
> R10(独立性缺口)的具体形态。**C-4 / C-5 可自动化**,已在本清单 §7 落地。

---

## 7. 漂移检测(机器校验)

本清单引用的 ArchForge 接口名 / 字段名**会漂移**。`scripts/test/cep_archforge_alignment_check.sh`
做两件事:

1. **引用完整性**:本清单引用的**每一处** ArchForge `file:line` 仍存在且仍含预期符号;
2. **gap 状态**:§3 中每条 gap 的「CEP 侧 anchor 符号」仍在 CppTLM 侧存在(防止 CppTLM 侧改了契约、清单没跟)。

> **ArchForge 仓不在本仓 CI 环境时**:检查**优雅跳过**(打印「跳过」而非红)——
> 跨仓路径是外部条件,缺失不是本仓的缺陷。

---

## 8. 诚实边界

1. **本清单基于 2026-10-21 的 ArchForge 快照**(git `10f5e11`,6 commit)——
   ArchForge 侧任何后续改动都可能使本清单过期;§7 的机器校验只覆盖**符号存在性**,
   不覆盖**语义等价性**。
2. **ArchForge 侧的 3 份 microarchitecture 文档自述为「规划」**
   (`coherence-protocol.md:20`、`coherence-bridge.md:21` 标题含「(蓝图 → 实施)」)——
   即它们**尚未落地**。故本清单对齐的是**设计意图**,不是已实现代码。
3. **§4 的 PR 范围是初稿**,§5 的 6 问未答前不可当作排期依据。
4. **本清单不构成 M2 放行** —— 放行判据见 §6,且 C-1 ~ C-3 **无法在本仓自动化**。
5. **本清单不在 M1 冻结集内** —— 它是**工作清单**,随 gap 关闭而演进;M1 冻结的是
   **规范性契约产物**(ADR / `07 号` / schema / tools / spec)。本清单的**引用**由
   §7 的机器校验守护(锚点漂移即红),这是与「冻结」不同的一种守护形态。

---

## 9. 机器可读锚点

<!-- CEP_ALIGNMENT_ANCHORS_BEGIN -->
```json
{
  "_comment": "机器可读锚点块 —— 供 scripts/test/cep_archforge_alignment_check.sh 校验引用漂移。改动本清单的 §2 / §3 引用时,必须同步本块。",
  "archforge": [
    {
      "file": "docs/architecture/09-coherence-protocol.md",
      "line": 242,
      "symbol": "enum class State"
    },
    {
      "file": "docs/architecture/09-coherence-protocol.md",
      "line": 243,
      "symbol": "enum class Event"
    },
    {
      "file": "docs/architecture/09-coherence-protocol.md",
      "line": 250,
      "symbol": "register_cache_callback"
    },
    {
      "file": "docs/architecture/09-coherence-protocol.md",
      "line": 311,
      "symbol": "class SnoopFilter"
    },
    {
      "file": "docs/architecture/09-coherence-protocol.md",
      "line": 314,
      "symbol": "struct SnoopEntry"
    },
    {
      "file": "docs/microarchitecture/coherence-bridge.md",
      "line": 135,
      "symbol": "class CoherenceBridge"
    },
    {
      "file": "docs/microarchitecture/coherence-domain.md",
      "line": 68,
      "symbol": "enum class Protocol"
    },
    {
      "file": "docs/microarchitecture/coherence-domain.md",
      "line": 94,
      "symbol": "class DomainRegistry"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 112,
      "symbol": "enum class CoherenceState"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 117,
      "symbol": "enum class SnoopType"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 125,
      "symbol": "enum class SnoopResp"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 133,
      "symbol": "class CoherenceProtocol"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 138,
      "symbol": "handle_probe"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 141,
      "symbol": "handle_request"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 146,
      "symbol": "can_coexist"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 149,
      "symbol": "translate"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 154,
      "symbol": "get_protocol_name"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 181,
      "symbol": "source_protocol"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 182,
      "symbol": "target_protocol"
    },
    {
      "file": "docs/microarchitecture/coherence-protocol.md",
      "line": 183,
      "symbol": "requestor_cache_id"
    },
    {
      "file": "docs/roadmap/cep-interface-alignment.md",
      "line": 1,
      "symbol": "cep-interface-alignment"
    }
  ],
  "cpptlm": [
    {
      "file": "docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md",
      "line": 55,
      "symbol": "CEN-Ib1"
    },
    {
      "file": "docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md",
      "line": 66,
      "symbol": "4 值锁"
    },
    {
      "file": "docs/adr/ADR-CEP-05-stub-cache-core-interface.md",
      "line": 170,
      "symbol": "INV-CEP-05-IC1"
    },
    {
      "file": "docs/adr/ADR-CEP-12-protocol-engine-capability-map.md",
      "line": 163,
      "symbol": "INV-CEP-12-IC1"
    },
    {
      "file": "docs/adr/ADR-CEP-12-protocol-engine-capability-map.md",
      "line": 168,
      "symbol": "INV-CEP-12-IC2"
    },
    {
      "file": "docs/adr/ADR-CEP-15-cachetlm-alignment.md",
      "line": 115,
      "symbol": "INV-CEP-15-IC3"
    },
    {
      "file": "docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md",
      "line": 98,
      "symbol": "INV-CEP-07A-IC1"
    },
    {
      "file": "docs/adr/ADR-CEP-07A-06bis-artifact-disposition.md",
      "line": 99,
      "symbol": "INV-CEP-07A-IC2"
    },
    {
      "file": "include/tlm/cep/protocol_instance.schema.json",
      "symbol": "ordering_assumption"
    },
    {
      "file": "include/tlm/cep/protocol_instance.schema.json",
      "symbol": "sharer_tracker"
    },
    {
      "file": "include/tlm/cep/protocol_instance.schema.json",
      "symbol": "combination_capabilities"
    },
    {
      "file": "docs/architecture/07-coherence-exploration-platform.md",
      "line": 310,
      "symbol": "14 个 adapter 实例"
    }
  ]
}
```
<!-- CEP_ALIGNMENT_ANCHORS_END -->

---

**版本**: v0.1 · **最后更新**: 2026-10-21 · **维护者**: CppTLM Team(builder mode)
