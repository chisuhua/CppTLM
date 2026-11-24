---
name: cep-protocol-engine-capability-declaration
priority: P1
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-12
  - ADR-CEP-13
  - ADR-CEP-18
状态: "✅ **已实施**（builder 2026-11-23/24 · Stage B 闭环）—— 声明面 `include/tlm/cep/engine_capabilities.hh`（8 行 `kEngineCapabilities{id, egress, basis}`）+ 5 个 `EngineTypeTraits<T>` 特化 + `HasInFlightApi<T>` SFINAE 探针 + `static_assert` 交叉核对 + 闸 `scripts/test/cep_engine_capabilities_check.sh` + main spec `openspec/specs/cep-protocol-engine-capability-declaration/spec.md`（台账 Entry #537）· 验收报告 `.rddf/reviews/cep-protocol-engine-capability-declaration-verify.md`"
created: 2026-11-23
---

# cep-protocol-engine-capability-declaration: 协议引擎承载能力声明面（现状可判化）

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不补任何缺口**（不补瞬态态 · 不补在飞事务 · 不加 egress · 不改引擎行为）——
> 本单元**只做「让现状可检出」**。⛔ **不改** `include/tlm/cep/protocol_instance.schema.json`（13 键冻结面）·
> ⛔ **不改** `include/tlm/cep/protocol_instances.json`（实例面）· ⛔ **不改** `docs/architecture/cep/protocol-engine.md`（冻结面 + architect 判权）·
> ⛔ **新增 invariant 数 = 0** · ⛔ **不改** `include/tlm/cep/litmus_falsifier.hh`（I2 单元）·
> ⛔ **不定义** `rfe` / `fr` / `ws` 语义（既有 backlog `cep-memory-model-operational-trace`）。

---

## 架构依据

### 1. 论文侧：ProtoGen 的核心产出是什么

Oswald 博士论文（178 页，一手 PDF）的核心产出是**从原子 SSP 生成非原子并发协议 FSM**：
per-block **瞬态态格**（`IMAD` / `IMA` / `IS` / `SI` / `SMAD` / `SMA` / `MI`）+ **转发请求改名** + **State Set 对偶性**。
论文 §2.4.2 明确：MSI 一个协议的**三个稳定态**对应**十七个瞬态态**。

### 2. 本仓侧：这些能力当前只以「引擎头文件里的散文」存在

| 事实 | 一手出处 |
|---|---|
| MESIF 引擎自陈「**本引擎不建模在飞的 snoop 集合**与屏障域(Stage 2)」 | `include/tlm/cep/mesif_protocol_engine.hh`（决策 `E-5`） |
| `moesi_gpu` 决策 M-6「canonical 10 流的 `on_*` **只记账**」 | `include/tlm/cep/moesi_gpu_protocol_engine.hh` |
| `ualink` 「⛔ **零发射**、**零状态**（计数器自增）」 | `include/tlm/cep/ualink_protocol_engine.hh` |
| `chi_rnf` 独有 `in_flight_` / `txn_in_flight` / `pending_egress` | `include/tlm/cep/chi_rnf_protocol_engine.hh` |
| `tilelink` 引擎是「事件计数 / 上报器」，明确「不建 PE 本地树状态机」 | `include/tlm/cep/tilelink_protocol_engine.hh` |
| 8 实例合计瞬态态 = 1（仅 `moesi_gpu` 的 `T`） | `include/tlm/cep/moesi_gpu_adapter.hh` |
| ⚠️ **`ualink_adapter.hh` / `nvlink_adapter.hh` 无 `kTransientStateCount` 字段**（= 字段不存在，非「值为 0」） | 该二文件（metis 1 轮实测订正） |

⇒ **这些差异目前没有任何机器可查的载体**（它们是注释，不是判据）。

### 3. 不做会怎样

| # | 后果 | 严重度 |
|:--:|---|:--:|
| **C1** | 读者 / 评审**无法从声明面区分**「有真协议 FSM 的实例」与「零发射记账层」⇒ 每次都要人工考古 5 个引擎头 | 🟡 中 |
| **C2** | 任何「补齐在飞事务建模」的改动**无法被检出** ⇒ 不补也不红 ⇒ Stage 2 义务**永远不会自动触发** | 🔴 高 |
| **C3** | `docs/architecture/cep/protocol-engine.md` 的断言（「真实协议的瞬态复杂度全部归 Protocol Engine 插件」）与实现背离**不可机检** | 🔴 高 |
| **C4** | `ADR-CEP-18` 决策 5 的 **T2 判据**（「目标协议的**全部协议私有状态**可被 SSP 表达」）在仓内**从未被测过** | 🟡 中 |

> **⚠️ 本单元的真实目的是「把 Stage 2 义务变成一张挂在代码里的欠条」**（C2）⇒ 欠条要有效，前提是「**补了必红**」这条牙是真的。
> **边界**：「只做检出、不做补齐」——补齐属协议 FSM 语义设计，归 architect + ArchForge。

### 4. 仓内既有可复用范式（本单元照抄，不发明）

| 范式 | 先例 | 本单元如何用 |
|---|---|---|
| **结构化的「本仓自推决策」登记表**（`id` / `basis`） | `MesifProtocolEngine::kEngineDecisions[]`（`E-1`~`E-6`）+ `HierarchicalDirectory::kBehaviorDecisions` | 声明面每行带 **`basis`（非空 ⇒ 缺失即判据必红）** |
| **可变异形态的自洽谓词** | `MesifProtocolEngine::engine_decisions_are_consistent(list, n)` | 判据本体接收 `(list, n)` ⇒ 测试可传**变异副本**做负控 |
| **协议差异只许出现在协议头** | `include/tlm/cep/AGENTS.md` 铁律 2 | 类型级能力声明放在**各引擎头**；通用层只放枚举 + 判据 + 实例级表 |
| **反射探测优于文本匹配** | 仓内 `static_assert` 锁值序（如 `cxl_state_spec.hh` 的 `kStateCount == 4`） | 用 **SFINAE 探测公开 API**，⛔ 不用 grep 类名 |
| **编译失败型反向验证 = probe TU 模式** | `scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh` | AC-3/AC-4 的反向 fixture **必须**沿用该模式（probe TU + 失败输出命中签名），⛔ 不接受裸 `rc≠0` |

---

## 范围

### In-Scope

1. **通用层**（新）：`include/tlm/cep/engine_capabilities.hh` —— 枚举 + 实例级声明表 + 判据本体 + SFINAE 反射检测（**零协议名**）
2. **协议层**（5 个引擎头各一处）：`EngineTypeTraits<T>` 显式特化（**仅能力位 + `basis`，⛔ 不带 `instance_id`** —— 避免把实例命名权烧进类型系统）
3. **闸**（新）：`scripts/test/cep_engine_capabilities_check.sh` —— 实例集双向零 orphan + 引擎头文件集反向存在性 + 反向 fixture
4. **测试**（新）：`test/test_cep_engine_capabilities.cc` —— 编译期 `static_assert` 交叉核对 + 瞬态策略 + 运行期判据
5. **门禁挂点**：`scripts/test/cep_spec_strict_check.sh` 的覆盖对账（新增 spec 时数量自动跟随）

### Out-of-Scope（⛔ 逐条列明）

| 排除项 | 理由 |
|---|---|
| **补瞬态态格 / 补在飞 snoop 集合 / 加 egress** | ⛔ 本单元只做「让缺口可检出」。补齐 = 协议 FSM 语义设计 ⇒ 属 architect + ArchForge |
| **扩 `include/tlm/cep/protocol_instance.schema.json` 的 13 键** | 冻结面 ⇒ 五步链；且 `ADR-CEP-12` 决策 1 管「什么协议存在」，本单元管「谁承载语义」⇒ **正交** |
| **扩 `include/tlm/cep/protocol_instances.json` 的实例面** | 同上；该文件的 ⑥ 镜像比对闸要求逐字段对齐 ArchForge ⇒ 新增键会污染镜像义务 |
| **新建 JSON 声明源 + schema + loader** | **本轮裁决 = 不建**（D-3 修订，见 What Changes 末）：`kEngineDecisions` / `kTrackerTraits` 证明 **C++ constexpr 表即可作唯一真源**；JSON 会引入 schema + loader + 闸 + 镜像四件套 + **第二真源** |
| **改 `docs/architecture/cep/protocol-engine.md` 的措辞** | 冻结面 + architect 判权 ⇒ planner 不代裁 ⇒ 登记为 Stage C 待裁项 |
| **新增 invariant 编号** | 现状登记，不引入新架构约束 |
| **定义 `rfe` / `fr` / `ws` 语义 + 操作级模型** | 既有 backlog `cep-memory-model-operational-trace` |
| **改 `include/tlm/cep/litmus_falsifier.hh`** | 属 I2 单元 |

---

## Why

- **不做的代价**：C2 与 C3 使「Stage 2 义务」不可触发 ⇒ 每次 review 重复人工考古，缺口可长期静默存在。
- **为什么不是「补缺口」而是「先声明」**：补缺口需 architect 决策 + ArchForge 语义授权（协议语义设计权威在 ArchForge，per `ADR-CEP-00`）⇒ planner 越权；**把缺口变成可检出的声明面，是 planner 域内可做且不越界的事**。
- **为什么用反射而不是 grep 类名**：`grep "txn_in_flight"` 会命中**注释**与**历史行** ⇒ 假绿；SFINAE 探测的是**类型真实暴露的公开 API** ⇒ 引擎删掉该方法 ⇒ 编译直接失败。
- **为什么声明与事实要「两份」**：声明（引擎说「我建模了在飞事务」）与事实（类型真的暴露 `txn_in_flight`）是**两个独立来源** ⇒ 交叉核对才有鉴别力；若只有一份 ⇒ 恒真、零判别力。
- **为什么 `egress` 不做机检**：5 个引擎的发射面**没有统一的公开可观察 API**（`chi_rnf` 是 `emit(b, ch)` / `mesif` 是 `emit_snoop_resp(...)`，其余三个**零发射**）⇒ 强行 grep 会命中注释 ⇒ 假绿。
  ⇒ **记录但不伪装成有牙判据**：头内单条常量 `kEgressHasMachineCheck = false`（**⛔ 不做 per-row `pending` 字段** —— 那会是恒真字段、零判别力）。
- **为什么不建 JSON**：见 What Changes 末的 D-3 修订说明。

---

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| **1** | **通用层** `include/tlm/cep/engine_capabilities.hh`（新建） | ① `enum class EgressMode { ProtocolMessages, None }` ② `enum class InFlightModeling { Modeled, NotModeled }` ③ `struct EngineCapability { const char* id; EgressMode egress; const char* basis; };`（**⛔ 无 `pending` 字段** —— 诚实性由文件级常量承载）④ `inline constexpr std::array<EngineCapability, 8> kEngineCapabilities`（8 行，见下方 egress 取值表）⑤ `inline constexpr bool kEgressHasMachineCheck = false;`（**本轮无机器判据的显式登记**）⑥ `template <class T> struct EngineTypeTraits { static_assert(!sizeof(T*), "EngineTypeTraits: <T> 无能力特化；若无引擎实例请走 kEngineCapabilities 行级声明"); };`（dependent-false ⇒ 保留「未特化即错」+ 可读归因）⑦ SFINAE `HasInFlightApi<T>`（探测 `T::txn_in_flight`）⑧ `engine_capabilities_are_consistent(list, n)` 自洽谓词（`basis` 非空 + `id` 唯一）<br>⚠️ **零协议名**：本头**不得**出现任何具体引擎类名 / adapter 类名（铁律 2），且**不得 `#include` 任何 `*_protocol_engine.hh` / `*_adapter.hh`**（防经 include 绕过类名检查形成隐式耦合） |
| **2** | **协议层** 5 个引擎头各追加 `EngineTypeTraits<T>` 特化 | 特化内容 = `static constexpr InFlightModeling in_flight` + `static constexpr const char* basis`（**⛔ 不含 `instance_id`**）<br>⚠️ 特化必须**逐字**落在**各引擎头**（协议差异留在协议层）<br>⚠️ **1 引擎类型 : 1 实例**（现状不变式；若未来一类型服务多实例 ⇒ 须改设计并走评审）<br>⚠️ 3 个**无引擎**实例（`cxl_2_0` / `cxl_3_0_symmetric` / `nvlink_4`）**无特化** |
| **3** | **判据本体**（置于 #1 的头内，零 JSON 依赖） | **J-1** 在飞建模一致（**编译期**）：`EngineTypeTraits<T>::in_flight == Modeled` ⇔ `HasInFlightApi<T>::value == true`<br>**J-2** 实例集完整（**运行期 + 闸**）：`kEngineCapabilities` 的 `id` 集 **==** `protocol_instances.json` 的 `id` 集（**双向零 orphan**）<br>**J-2b** 声明自洽：每行 `basis` 非空 ∧ `id` 唯一<br>**J-3** 瞬态态策略（**编译期**）：**仅对有 `kTransientStateCount` 的 adapter**（`chi_rnf` / `mesif` / `moesi_gpu` / `tilelink_c` / `cxl_cache`）`static_assert(kTransientStateCount <= kMaxTransientStatesPerInstance)`；⚠️ `ualink_adapter` / `nvlink_adapter` **无该字段** ⇒ **不在 J-3 范围**<br>⚠️ **`kMaxTransientStatesPerInstance = 1` 是「现状锁」而非「设计容量」** —— 提升该阈值 = **architect 裁决事项**（衔接路径文档 Stage C）<br>⛔ **不把各实例的瞬态态计数复制进通用层表**（adapter 头是唯一真源） |
| **4** | **闸** `scripts/test/cep_engine_capabilities_check.sh`（新建） | ① 从 `kEngineCapabilities[]` 的**初始化区段**抽取 `id` 集（grep **数据行**，⛔ 不 grep API / 类名）② 双向零 orphan（↔ `protocol_instances.json` 的 `id`）③ **引擎头文件集反向存在性**：`include/tlm/cep/*_protocol_engine.hh` 的集合 **恰为** 5 个已登记引擎头；出现第 6 个 ⇒ 必红（红因码 `engine-cap-new-engine-detected`）④ 反向 fixture（毒化 ⇒ 按固定签名红 ⇒ 还原 ⇒ 必须绿）<br>⚠️ ③ 的**已知局限**：无法区分同 spec 的两个实例（`cxl_2_0` vs `cxl_3_0_symmetric` 共用 `cxl_*` 命名空间）⇒ ③ 只守「**新引擎出现**」这一个真实风险，**不假装能逐实例绑定** |
| **5** | **测试** `test/test_cep_engine_capabilities.cc`（新建） | ① **编译期** `static_assert` 逐实例核对 J-1（**失败签名须含实例 id 字面量**）② **编译期** `static_assert` 核对 J-3（5 个持字段的 adapter）③ 运行期核对 J-2 / J-2b + 自洽谓词的**变异副本**负控 |
| **6** | **门禁挂点** | `scripts/test/cep_spec_strict_check.sh` 的「items 数 == specs 目录数」对账自动跟随 ⇒ **⛔ 不写死数字** |

### egress 取值表（8 行 · 逐行给依据）

| `id`（= JSON 键值） | 引擎 | `egress` | 依据 |
|---|---|---|---|
| `chi_rnf` | `ChiRnfProtocolEngine` | `ProtocolMessages` | `emit(b, ChiChannel)` + `emit_comp_ack` + `emit_snoop_resp` |
| `mesif` | `MesifProtocolEngine` | `ProtocolMessages` | `emit_snoop_resp(...)` + 请求装填（`request_emitted` 递增） |
| `moesi_gpu` | `MoesiGpuProtocolEngine` | `None` | 决策 M-6「`on_*` 只记账」；`emit` 零命中 |
| `tilelink_c` | `TileLinkProtocolEngine` | `None` | 事件计数 / 上报器（`note_atomic_intent` / `consume`），不建 PE 本地树状态机；`emit` 零命中 |
| `ualink_4` | `UalinkProtocolEngine` | `None` | 「零发射、零状态」 |
| `cxl_2_0` | **（无引擎）** | `None` | 无对应 `*_protocol_engine.hh` |
| `cxl_3_0_symmetric` | **（无引擎）** | `None` | 同上 |
| `nvlink_4` | **（无引擎）** | `None` | 同上 |

> ⚠️ **实例名逐字对齐**：JSON 键 = `id`（⛔ 非 `instance_id`）；TileLink 实例 id = `tilelink_c`（⛔ 非 `tilelink`）。

### D-3 修订说明（相对 2026-11-23 初次设计 —— 已向用户报备 + metis 已确认方向正确）

初次设计为「新建 `engine_modes.json` + 同名 schema + loader」。本轮**改为零 JSON**：

1. 仓内先例证明 **C++ constexpr 表即可作唯一真源**（`kEngineDecisions[]` / `kTrackerTraits`）⇒ JSON 镜像 = **第二真源**（需额外闸逐字段比对）；
2. JSON 方案额外引入 **schema + loader + 闸 + 镜像** 四件套，收益不抵成本（本单元只需 8 行 × 3 字段）；
3. **零 JSON ⇒ 零冻结面触碰** ⇒ 无五步链成本。

⇒ **目标不变**（避开冻结的 13 键 schema），**路径更省**。

---

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；
> ⛔ 「rc≠0」不构成红因；⛔ 「声明 ≠ 判定」—— 声明面成立**不等于**协议语义已被检验。
> **编译失败型判据（AC-3 / AC-4）的实现范式**：沿用 `scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh` 的 **probe TU 模式**（写一个临时 TU + 断言编译失败 **且** 输出命中该 `static_assert` 的**字面量签名**）；⛔ **不接受**「只看 `rc≠0`」。

- [ ] **AC-1（通用层零协议名 · 机检）**：`include/tlm/cep/engine_capabilities.hh` 内**不出现**任何具体引擎类名（`ChiRnfProtocolEngine` / `MesifProtocolEngine` / `MoesiGpuProtocolEngine` / `UalinkProtocolEngine` / `TileLinkProtocolEngine`）、adapter 类名、**且不 `#include` 任何 `*_protocol_engine.hh` / `*_adapter.hh`**；反 fixture：① 插入其一类名 ⇒ 红因码 `generic-layer-protocol-name` ② 插入任一 `#include "..._protocol_engine.hh"` ⇒ 红因码 `generic-layer-protocol-include`<br>⚠️ **豁免**：`HasInFlightApi` 自身含 `InFlight` 字样 ⇒ 不在 AC-9 的字符扫描范围（见 AC-9）
- [ ] **AC-2（实例集双向零 orphan · 行为判据）**：`kEngineCapabilities` 的 `id` 集 **==** `protocol_instances.json` 的 `id` 集 ⇒ ① 删一行 ⇒ 必红（红因码 `engine-cap-orphan-missing`）② 加多余一行 ⇒ 必红（`engine-cap-orphan-extra`）③ 还原 ⇒ 必须绿；⚠️ 本条**同时承担「删一行」的负控**（谓词不承担，见 AC-6）
- [ ] **AC-3（在飞建模一致 · 编译期）**：逐实例 `static_assert(EngineTypeTraits<T>::in_flight == Modeled ⇔ HasInFlightApi<T>::value)`；**反 fixture（probe TU）**：① 把 `chi_rnf` 特化改成 `NotModeled` ⇒ 编译失败 **且输出命中 `chi_rnf` 字面量** ② 把 `mesif` 特化改成 `Modeled`（但它无 `txn_in_flight`）⇒ 编译失败 **且输出命中 `mesif` 字面量**
- [ ] **AC-4（瞬态态策略 · 编译期）**：对**持有 `kTransientStateCount` 的 5 个 adapter**（`chi_rnf` / `mesif` / `moesi_gpu` / `tilelink_c` / `cxl_cache`）逐 `static_assert(kTransientStateCount <= kMaxTransientStatesPerInstance)`；**反 fixture（probe TU）**：给 `mesif` 的 `kTransientStates` 加一项 ⇒ 编译失败且输出命中 `mesif`；⚠️ `ualink_adapter` / `nvlink_adapter` **不在本条范围**（无该字段 ⇒ 引用即编译错）；⛔ **不得**把计数复制进 `kEngineCapabilities`（AC-9 覆盖）
- [ ] **AC-5（`basis` 非空 + `kEgressHasMachineCheck` 诚实登记 · 机检）**：① 每行 `basis` 非空（缺失 ⇒ 红因码 `engine-cap-empty-basis`）② `kEgressHasMachineCheck == false`（**本轮的诚实登记**；若将来引入 egress 机检 ⇒ 必须翻为 `true` 并同时补该判据，否则红因码 `engine-cap-egress-claim-without-check`）③ 反 fixture：`basis` 置空 ⇒ 红；`kEgressHasMachineCheck` 改 `true` 而仓内无 egress 判据 ⇒ 红
- [ ] **AC-6（自洽谓词可变异 · 负控 · 限两项）**：`engine_capabilities_are_consistent(list, n)` 必须能接收**变异副本**并返回假 ⇒ 负控**仅两个变体**：① `basis` 置空 ② `id` 重复；⛔ **「删一行」不在谓词负控范围**（谓词无行数期望源 ⇒ 归 AC-2 的 J-2 闸）；⛔ 谓词**不得**读取全局状态（纯函数）
- [ ] **AC-7（SKIP 与 FAIL 可区分 · 闸侧）**：`cep_engine_capabilities_check.sh` 在「从初始化区解析出 **0 行**」时判 `SKIP`（自检未就绪），在「行集不符」判 `FAIL` ⇒ `SKIP` 与 `FAIL` **必须可区分**（⛔ 不得把「解析不到」当「通过」）；⚠️ 零 JSON 设计下**不存在**「声明源不可读」态（constexpr 表编译进二进制）⇒ 本条只覆盖闸的解析退化
- [ ] **AC-8（引擎头文件集反向存在性 · 机检）**：`include/tlm/cep/*_protocol_engine.hh` 的文件集 **恰为** 5 个已登记引擎头 ⇒ ① 正控：恰 5 个 ⇒ 绿 ② 反 fixture：`touch include/tlm/cep/fake_protocol_engine.hh` ⇒ 必红（红因码 `engine-cap-new-engine-detected`）③ 删除其一 ⇒ 必红；⚠️ **已知局限**：无法区分共用命名的两个 `cxl` 实例 ⇒ 本条只守「新引擎出现」这一个风险，**⛔ 不假装能逐实例绑定**
- [ ] **AC-9（单真源 · 机检 · 范围收窄）**：检查目标**限定为** `struct EngineCapability` 的字段声明区 与 `kEngineCapabilities` 的初始化区段 ⇒ 其间**不得**出现 `transient` / `in_flight` 字段名；反 fixture：在 `EngineCapability` 内加 `int transient_state_count;` ⇒ 必红（红因码 `engine-cap-duplicate-source`）；⚠️ **`HasInFlightApi` 的定义区不在扫描范围**（AC-1 豁免）
- [ ] **AC-10（零回归）**：`ctest` 全绿（⛔ 不写死断言数）· 全部 `scripts/test/cep_*.sh` rc=0 · `bash scripts/test/cep_docs_sync_check.sh --strict` rc=0 · `bash scripts/test/cep_doc_ref_check.sh` rc=0
- [ ] **AC-11（既有冻结面零变更 · 机检）**：`git diff --name-only` 中的 `openspec/specs/cep-*` 路径 **全部为本 change 新增**（即：与「既有 spec 集合」求交 == 空）· 且不含任何 `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` 的**已存在**文件 ⇒ 违反即本单元失败（⇒ **本单元不需要五步链**）<br>⚠️ 本 change **会新增** `openspec/specs/cep-engine-capability-declaration/spec.md` ⇒ 该新增**不算**违反
- [ ] **AC-12（不新增 invariant 编号 · 机检）**：`cep_invariant_map_check.py` rc=0 且本 change **未新增**任何 `INV-*` 编号（⛔ 不写死「66」这类会腐化的快照值）
- [ ] **AC-13（表述纪律 · 人工评审项 · ⛔ 不写成验收 checkbox）**：产出物中**不得**出现「协议语义已验证 / 无违例 / 已证明」类断言；本单元只登记**承载能力现状** ⇒ 由 verifier 在报告中以**人工评审**口径确认（并入 `.rddf/reviews/<change>-verify.md` 的「表述纪律」段），⛔ 不进入自动判据清单

---

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-engine-capability-declaration` | **ADDED** | 新能力：**协议实例的承载能力声明面** —— ① 引擎存在性 ② egress 形态（诚实标注无机器判据）③ 在飞事务建模（编译期反射交叉核对）④ 瞬态态策略（编译期，仅对有字段的 adapter）⑤ 实例集双向零 orphan ⑥ 引擎头文件集反向存在性 |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `include/tlm/cep/engine_capabilities.hh`（通用层 · header-only）· `scripts/test/cep_engine_capabilities_check.sh` · `test/test_cep_engine_capabilities.cc` · `openspec/specs/cep-engine-capability-declaration/spec.md`（主 spec） |
| **修改文件** | 5 个引擎头各**追加** `EngineTypeTraits<T>` 特化（`include/tlm/cep/chi_rnf_protocol_engine.hh` · `include/tlm/cep/mesif_protocol_engine.hh` · `include/tlm/cep/moesi_gpu_protocol_engine.hh` · `include/tlm/cep/ualink_protocol_engine.hh` · `include/tlm/cep/tilelink_protocol_engine.hh`）· `test/CMakeLists.txt`（挂测试） |
| **零改动（硬查 · AC-11）** | `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/protocol_instances.json` · `include/tlm/cep/perm_upgrades.json` · `include/tlm/cep/litmus_falsifier.hh` · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · **既有** `docs/architecture/cep/*.md` · **既有** `docs/adr/ADR-CEP-*.md` · **既有** `openspec/specs/cep-*` |
| **冻结面** | **无**（新建文件 + 引擎头的**追加**特化 ⇒ 引擎头不在 M1 冻结清单）⇒ ⛔ 不需要五步链 |
| **闸** | ctest +1 · `scripts/test/cep_engine_capabilities_check.sh` 新增 1 个（⛔ 数量**不是**鉴别力指标）· `scripts/test/cep_spec_strict_check.sh` 覆盖对账自动跟随 |
| **KPI** | 本单元占 1 slot（当前 Proposed = 0/3 ⇒ 峰值 1/3）⇒ 归档后释放；⛔ 不写死 `0/3 → 1/3 → 0/3`（会腐化） |
| **风险** | ① 反射探测点选错 ⇒ 缓解：探测点必须**公开可观察**，探测名写进判据并登记 ⇒ 改名即触发复核 ② 3 个无引擎实例是**编译期飞地**（其行只被运行期/闸看，无编译期绑定）⇒ 缓解：AC-8 的引擎头文件集反向存在性守「新引擎出现」这一个真实风险；⚠️ **已知局限** = 无法逐实例绑定共用命名的 cxl 两实例 ③ `egress` 无机器判据 ⇒ 缓解：`kEgressHasMachineCheck = false` **诚实登记**（⛔ 不伪装成有牙）+ AC-5 ② 强制「引入机检必须同时翻标志」④ 声明面被误读为「能力承诺」⇒ 缓解：AC-13 表述纪律（人工评审项）⑤ `EngineTypeTraits` 主模板 dependent-false 的报错可读性 ⇒ 缓解：自定义 `static_assert` 消息含「若无引擎实例请走 kEngineCapabilities 行级声明」 |

---

## 审查轨迹

| 轮次 | 角色 | 判定 | 本单元受影响处 |
|:--:|---|:--:|---|
| — | **architect** | ⛔ 未审 | 本单元**不触碰**架构决策面（不改冻结面 · 不改 ADR · 不新增 invariant）；「瞬态态归属措辞」与「证据天花板登记」另登记为 Stage C 待 architect 裁项（见 `.rddf/roadmap/cep-gen-family-gap-closure.md` §3 Stage C） |
| 1 | **metis** | **提出 P0 × 5 + P1 × 7 + P2 × 5** | ✅ **全部处置**：P0-1 roadmap Stage B 与 D-3 脱节 ⇒ 已改写 roadmap §3/§6（`engine_modes` 零残留）· P0-2 AC-11 与 Impact 矛盾 ⇒ AC-11 改为「既有 spec 零变更，新增除外」· P0-3 AC-4 对 ualink/nvlink 不可编译 ⇒ AC-4 显式枚举 5 个持字段 adapter + roadmap F1 措辞订正 · P0-4 AC-6「删一行」与谓词不自洽 ⇒ 拆：谓词只留 `basis` 置空 / `id` 重复，删一行归 AC-2 · P0-5 AC-8 探测机制未定义 ⇒ 定为**引擎头文件集反向存在性** + 诚实标注已知局限 · P1-6 AC-9 范围收窄 + `HasInFlightApi` 豁免 · P1-7 AC-3/4 锚定 probe-TU 先例 + 失败签名含实例 id · P1-9 AC-7 重定义为「闸解析 0 行 ⇒ SKIP」· P1-10 补 egress 8 行取值表 + 键名对齐 `id` / `tilelink_c` · P2-13 J-3 阈值 = 现状锁 + 提升归 architect · P2-14 AC-12 删「66」快照值 · P2-15 主模板 dependent-false + 1 引擎类型:1 实例 · P2-16 AC-1 增补禁 include 协议头。**另采纳两项 metis 主动发现**：① `EngineTypeTraits` **移除 `instance_id`**（实例命名权不烧进类型系统）② 去掉 per-row `pending` 字段（恒真、零判别力）⇒ 改为文件级常量 `kEgressHasMachineCheck` |
| — | **（metis 发现的事实订正）** | 已核实 | `ualink_adapter.hh` / `nvlink_adapter.hh` **无 `kTransientStateCount`**（实测 `grep -c` = 0）；JSON 键 = `id`；TileLink 实例 id = `tilelink_c` |
