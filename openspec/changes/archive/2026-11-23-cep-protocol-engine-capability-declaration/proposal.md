# proposal: cep-protocol-engine-capability-declaration —— 协议引擎承载能力声明面（现状可判化）

> **来源**：`.rddf/improvements/cep-protocol-engine-capability-declaration.md`（planner 2026-11-23 · 用户批准「I1 完整闭环」· metis 1 轮已处置）
> **上游分析**：`.rddf/roadmap/cep-gen-family-gap-closure.md`（Oswald 博士论文 178 页一手 PDF × 本仓 CEP 实现面对照 ⇒ 缺口 G1）
> **定位**：把「协议引擎到底承载到哪一层」从**引擎头里的散文**变成**编译期 + 闸双重保证的声明面**。⛔ **不补缺口**，只让缺口**可检出**。

## Why

### 论文侧的事实

Oswald 博士论文的核心产出是**从原子 SSP 生成非原子并发协议 FSM**：per-block **瞬态态格**（`IMAD` / `IMA` / `IS` / …）+ 转发请求改名 + State Set 对偶性。论文 §2.4.2 明确：MSI 一个协议的**三个稳定态**对应**十七个瞬态态**。

### 本仓侧的现状：这些差异只存在于注释里

| 事实 | 一手出处 |
|---|---|
| 8 个协议实例合计瞬态态 = **1**（仅 `moesi_gpu` 的 `T`） | `include/tlm/cep/moesi_gpu_adapter.hh` |
| MESIF 引擎自陈「**不建模在飞的 snoop 集合**与屏障域(Stage 2)」 | `include/tlm/cep/mesif_protocol_engine.hh`（决策 `E-5`） |
| `moesi_gpu` 决策 M-6「`on_*` **只记账**」（零发射） | `include/tlm/cep/moesi_gpu_protocol_engine.hh` |
| `ualink` 「零发射、零状态（计数器自增）」 | `include/tlm/cep/ualink_protocol_engine.hh` |
| `tilelink` 引擎是「事件计数 / 上报器」 | `include/tlm/cep/tilelink_protocol_engine.hh` |
| ⚠️ `ualink_adapter.hh` / `nvlink_adapter.hh` **无** `kTransientStateCount` 字段（= 不存在，非「为 0」） | 该二文件（metis 实测） |
| 3 个实例**无引擎**（`cxl_2_0` / `cxl_3_0_symmetric` / `nvlink_4`） | `include/tlm/cep/` 无对应 `*_protocol_engine.hh` |

⇒ **没有任何机器可查的载体**。

### 不做的代价

| # | 后果 | 严重度 |
|:--:|---|:--:|
| **C1** | 无法从声明面区分「有真协议 FSM 的实例」与「零发射记账层」⇒ 每次 review 人工考古 5 个引擎头 | 🟡 中 |
| **C2** | 任何「补齐在飞事务建模」的改动**无法被检出** ⇒ 不补也不红 ⇒ Stage 2 义务**永远不会自动触发** | 🔴 高 |
| **C3** | `docs/architecture/cep/protocol-engine.md` 的断言与实现背离**不可机检** | 🔴 高 |
| **C4** | `ADR-CEP-18` 决策 5 的 **T2 判据**在仓内**从未被测过** | 🟡 中 |

> **⚠️ 本单元的真实目的是「把 Stage 2 义务变成一张挂在代码里的欠条」**（C2）⇒ 欠条要有效，前提是「**补了必红**」这条牙是真的。
> **边界**：「只做检出、不做补齐」——补齐属协议 FSM 语义设计，归 architect + ArchForge（per `ADR-CEP-00` 的设计权威分层）。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **通用层** `include/tlm/cep/engine_capabilities.hh`（新建） | `EngineCapability { id; egress; basis; }` + `kEngineCapabilities[8]` + `kEgressHasMachineCheck = false` + `EngineTypeTraits<T>` 主模板（dependent-false）+ SFINAE `HasInFlightApi<T>` + 自洽谓词；**零协议名 · 零 include 协议头** |
| 2 | **协议层** 5 个引擎头各**追加** `EngineTypeTraits<T>` 特化 | 仅 `in_flight` + `basis`（⛔ 不带 `instance_id`）；1 引擎类型 : 1 实例 |
| 3 | **判据本体** | J-1 在飞建模一致（编译期）· J-2 实例集双向零 orphan · J-2b 声明自洽 · J-3 瞬态态策略（仅对持字段的 5 个 adapter；阈值 1 = **现状锁**） |
| 4 | **闸** `scripts/test/cep_engine_capabilities_check.sh`（新建） | 实例集双向零 orphan + 引擎头文件集**反向存在性** + 反向 fixture |
| 5 | **测试** `test/test_cep_engine_capabilities.cc`（新建） | 编译期 `static_assert` 交叉核对 + 运行期判据 + 变异副本负控 |
| 6 | **门禁挂点** | `scripts/test/cep_spec_strict_check.sh` 覆盖对账自动跟随（⛔ 不写死数字） |

**egress 取值表（8 行，逐行给依据）**：

| `id` | 引擎 | `egress` | 依据 |
|---|---|---|---|
| `chi_rnf` | `ChiRnfProtocolEngine` | `ProtocolMessages` | `emit(b, ChiChannel)` + `emit_comp_ack` + `emit_snoop_resp` |
| `mesif` | `MesifProtocolEngine` | `ProtocolMessages` | `emit_snoop_resp(...)` + 请求装填（`request_emitted`） |
| `moesi_gpu` | `MoesiGpuProtocolEngine` | `None` | 决策 M-6；`emit` 零命中 |
| `tilelink_c` | `TileLinkProtocolEngine` | `None` | 事件计数 / 上报器；`emit` 零命中 |
| `ualink_4` | `UalinkProtocolEngine` | `None` | 「零发射、零状态」 |
| `cxl_2_0` | **（无引擎）** | `None` | 无对应 `*_protocol_engine.hh` |
| `cxl_3_0_symmetric` | **（无引擎）** | `None` | 同上 |
| `nvlink_4` | **（无引擎）** | `None` | 同上 |

> ⚠️ 实例名逐字对齐：JSON 键 = `id`（⛔ 非 `instance_id`）；TileLink 实例 id = `tilelink_c`。

## Out of Scope

- ⛔ **补瞬态态格 / 补在飞 snoop 集合 / 加 egress** —— 补齐 = 协议 FSM 语义设计 ⇒ architect + ArchForge
- ⛔ **扩** `include/tlm/cep/protocol_instance.schema.json` 的 13 键（冻结面 ⇒ 五步链）
- ⛔ **扩** `include/tlm/cep/protocol_instances.json` 的实例面（⑥ 镜像比对义务）
- ⛔ **新建 JSON 声明源 + schema + loader**（D-3 修订：C++ constexpr 表即可作唯一真源，JSON 会造第二真源）
- ⛔ **改** `docs/architecture/cep/protocol-engine.md`（冻结面 + architect 判权 ⇒ Stage C 待裁）
- ⛔ **新增** invariant 编号 · ⛔ **定义** `rfe` / `fr` / `ws` 语义（既有 backlog）· ⛔ **改** `include/tlm/cep/litmus_falsifier.hh`（I2 单元）
- ⛔ 任何 `docs/adr/ADR-CEP-*.md` · 任何**既有** `openspec/specs/cep-*` · 任何协议引擎的**行为**变更

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **编译失败型判据（AC-3 / AC-4）的实现范式**：沿用 `scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh` 的 **probe TU 模式**（临时 TU + 断言编译失败 **且** 输出命中该 `static_assert` 的字面量签名）；⛔ 不接受「只看 `rc≠0`」。

- [ ] **AC-1（通用层零协议名 · 机检）**：`engine_capabilities.hh` 内不出现任何具体引擎类名 / adapter 类名，且不 `#include` 任何 `*_protocol_engine.hh` / `*_adapter.hh`；反 fixture：插类名 ⇒ `generic-layer-protocol-name`；插 include ⇒ `generic-layer-protocol-include`
- [ ] **AC-2（实例集双向零 orphan · 行为判据）**：`kEngineCapabilities` 的 `id` 集 == `protocol_instances.json` 的 `id` 集；反 fixture：删一行 ⇒ `engine-cap-orphan-missing`；加多余行 ⇒ `engine-cap-orphan-extra`；还原 ⇒ 绿。⚠️ 本条**同时承担「删一行」负控**
- [ ] **AC-3（在飞建模一致 · 编译期）**：逐实例 `static_assert(EngineTypeTraits<T>::in_flight == Modeled ⇔ HasInFlightApi<T>::value)`；probe TU 反 fixture：① `chi_rnf` 改 `NotModeled` ⇒ 编译失败且命中 `chi_rnf` 字面量 ② `mesif` 改 `Modeled` ⇒ 编译失败且命中 `mesif` 字面量
- [ ] **AC-4（瞬态态策略 · 编译期）**：对**持有 `kTransientStateCount` 的 5 个 adapter**（`chi_rnf` / `mesif` / `moesi_gpu` / `tilelink_c` / `cxl_cache`）逐 `static_assert(kTransientStateCount <= kMaxTransientStatesPerInstance)`；probe TU 反 fixture：给 `mesif` 加一项 ⇒ 编译失败且命中 `mesif`；⚠️ `ualink_adapter` / `nvlink_adapter` 不在范围
- [ ] **AC-5（`basis` 非空 + `kEgressHasMachineCheck` 诚实登记 · 机检）**：① 每行 `basis` 非空 ⇒ 否则 `engine-cap-empty-basis` ② `kEgressHasMachineCheck == false` ⇒ 改 `true` 而无 egress 判据则 `engine-cap-egress-claim-without-check`
- [ ] **AC-6（自洽谓词可变异 · 负控 · 限两项）**：`engine_capabilities_are_consistent(list, n)` 对**变异副本**返回假 —— 仅两个变体：① `basis` 置空 ② `id` 重复；⛔ 「删一行」不在谓词范围（归 AC-2）；⛔ 谓词不得读全局状态
- [ ] **AC-7（SKIP 与 FAIL 可区分 · 闸侧）**：闸在「解析出 0 行」判 `SKIP`、「行集不符」判 `FAIL` ⇒ 两者必须可区分（⛔ 不得把「解析不到」当「通过」）
- [ ] **AC-8（引擎头文件集反向存在性 · 机检）**：`include/tlm/cep/*_protocol_engine.hh` 恰为**已登记的 6 个**（5 个实例引擎 + 1 个 `cohif_protocol_engine.hh` = `NullProtocolEngine<2>` 的可注册外壳，不承载协议语义、无实例特化）；反 fixture：`touch` 假引擎头 ⇒ `engine-cap-new-engine-detected`；删其一 ⇒ 红。⚠️ 已知局限：无法区分共用命名的两个 `cxl` 实例
- [ ] **AC-9（单真源 · 机检 · 范围收窄）**：`struct EngineCapability` 字段区 与 `kEngineCapabilities` 初始化区段内不得出现 `transient` / `in_flight` 字段名；反 fixture：加 `int transient_state_count;` ⇒ `engine-cap-duplicate-source`；⚠️ `HasInFlightApi` 定义区不在扫描范围
- [ ] **AC-10（零回归）**：`ctest` 全绿（⛔ 不写死断言数）· 全部 `scripts/test/cep_*.sh` rc=0 · `cep_docs_sync_check.sh --strict` rc=0 · `cep_doc_ref_check.sh` rc=0
- [ ] **AC-11（既有冻结面零变更 · 机检）**：`git diff --name-only` 中的 `openspec/specs/cep-*` 路径**全部为本 change 新增**（与既有 spec 集合求交 == 空）· 且不含**已存在**的 `docs/adr/ADR-CEP-*.md` / `docs/architecture/cep/*.md`（⇒ 本单元无需五步链；本 change **会新增** spec ⇒ 不算违反）
- [ ] **AC-12（不新增 invariant 编号 · 机检）**：`cep_invariant_map_check.py` rc=0 且本 change 未新增任何 `INV-*` 编号（⛔ 不写死「66」这类会腐化的快照值）
- [ ] **AC-13（表述纪律 · 人工评审项）**：产出物不得出现「协议语义已验证 / 无违例 / 已证明」类断言 ⇒ 由 verifier 在报告「表述纪律」段人工确认，⛔ 不进入自动判据清单

## 影响面

| 面 | 内容 |
|---|---|
| **新增** | `include/tlm/cep/engine_capabilities.hh` · `scripts/test/cep_engine_capabilities_check.sh` · `test/test_cep_engine_capabilities.cc` · 本 change 的 spec delta |
| **修改** | 5 个引擎头**追加**特化 · `test/CMakeLists.txt` |
| **零改动** | `protocol_instance.schema.json` · `protocol_instances.json` · `perm_upgrades.json` · `litmus_falsifier.hh` · `sharer_tracker.hh` · `protocol_engine_base.hh` · 既有 ADR / 既有 CEP spec |
| **冻结面** | 无 ⇒ 无需五步链 |
| **KPI** | 占 1 slot（归档释放） |
