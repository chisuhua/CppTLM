---
name: cep-hop-timing-capability-declaration
priority: P2
phase: phase-10
主题: G10 缺口可检出面 —— 协议语义层的「hop / 时序模型」承载能力声明
依赖:
  - ADR-CEP-13
  - ADR-CEP-19
  - ADR-CEP-20
状态: "🟢 **已批准·待立项**(2026-11-24 · per 用户指令「继续推进任务1」⇒ gap 分析 **G10** 的本仓可做部分 = **让缺口可检出**；⛔ **不补 hop / 时序模型**（属协议 FSM 语义设计，归 ArchForge）)"
created: 2026-11-24
---

# cep-hop-timing-capability-declaration: 协议语义层的「hop / 时序模型」承载能力声明面

> **来源**：gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md` **G10**（无网络 hop / 时序模型 · 🟡 P2 · 「需先有协议 FSM 语义」· **依赖 G1**）· G1 已于 2026-11-23 闭合（`cep-protocol-engine-capability-declaration` 归档）⇒ **依赖已解冻**。
> **形态**：**照抄 I1（G1）的现成范式**（声明面 + 编译期反射 + 闸 + 反向 fixture）· ⛔ 不发明新机制。

## 架构依据

### 1. 论文侧（G10 的语义来源）

Oswald 博士论文（*HeteroGen*）的协议语义产出 = **非原子多跳并发 FSM**（per-block 瞬态态 + **转发请求改名** + State Set 对偶性）⇒ 协议的**时序行为**建立在「消息穿越网络」的**跳数（hops）**与**时延**之上 ⇒ 没有 hop / 时序模型 ⇒ 协议 FSM 的**时间维度**无处落地。

### 2. 本仓侧（缺口的事实面 · 2026-11-24 实测）

| # | 事实 | 一手出处 |
|:--:|---|---|
| F1 | **协议语义层（8 个实例的引擎）零 hop / 时序 API**：5 个实例引擎（`chi_rnf` / `mesif` / `moesi_gpu` / `tilelink_c` / `ualink_4`）+ 1 个 CohIf Null 外壳中，去注释后**零** hop / latency / travel-time / arrival-cycle 类 API | `include/tlm/cep/*_protocol_engine.hh`（实测 grep） |
| F2 | 引擎**有**步进钩子 `tick()`（驱动入站分派 + egress 发送）—— ⛔ 但那是**全局步进**，**不是**消息级时序模型 | `include/tlm/cep/protocol_engine_base.hh:319` · `chi_rnf_protocol_engine.hh:520` · `mesif_protocol_engine.hh:647` |
| F3 | **载体 / 器件层已有** hop / 时序概念：CohIf flit 的 `hops` 字段（router 转发时递增）· CohIf responder 的 `completion_latency_cycles` · 桩缓存核的 `snoop_latency_cycles` | `include/tlm/cep/cohif_nic.hh:233` + `:103` · `cohif_responder.hh:53` · `stub_cache_core.hh:158` |
| F4 | 3 个实例**无独立引擎头**（`cxl_2_0` / `cxl_3_0_symmetric` / `nvlink_4`）⇒ 与 G1 同型：**编译期飞地** | `include/tlm/cep/protocol_instances.json` · I1 的 `kEngineCapabilities` |

⇒ **G10 的精确语义（必须逐字守住）**：**「无 hop / 时序模型」只对「协议语义层」成立** —— 载体 / 器件层**已有** hop 与时延概念（F3）⇒ ⛔ **不得**表述为「全平台无 hop / 时序」。

### 3. 不做会怎样

G10 继续以「散文缺口」存在 ⇒ 任何人新增协议语义层的 hop / 时序建模时，**无任何判据要求同步声明面** ⇒ 与 G1 实施前的状态同型（引擎头里的散文）⇒ 缺口**不可检出** ⇒ 论文侧的时间维度落地时，本仓**无法回答「哪个实例已经有 hop / 时序模型」**。

### 4. 仓内既有可复用范式（本单元照抄，不发明）

- **I1（G1）**：`include/tlm/cep/engine_capabilities.hh`（协议中立 constexpr 声明表 + `basis` 字段 + 诚实边界）+ `scripts/test/cep_engine_capabilities_check.sh`（双向零 orphan + 反向存在性 + probe TU 反向 fixture）+ `test/test_cep_engine_capabilities.cc`（编译期交叉核对）。
- **协议中立铁律**（`include/tlm/cep/AGENTS.md` 铁律 2）：通用层**不得**出现协议**类名**、**不得** include 协议头 ⇒ 协议差异只经由各协议头对 traits 的**特化**注入。
- **单真源纪律**：G1 的表**不含**瞬态态 / 在飞字段（那两项的真源在 adapter 头与 traits 特化）⇒ 本单元的表**不含** G1 的 `egress` / `in_flight` 字段（**正交**，不重复）。

## 范围

### In-Scope

1. 新建 `include/tlm/cep/hop_timing_capabilities.hh`（协议中立 · 通用层 · 零协议名）：协议语义层的 hop / 时序建模声明（8 行）+ `basis` + SFINAE 探针模板（否定断言用）+ 诚实边界登记（`tick()` ≠ 时序模型 · 载体层已有 hop/时序）。
2. 新建 `test/test_cep_hop_timing_capabilities.cc`：编译期否定断言（5 个实例引擎 × hop / 时序）+ 运行期 J-1/J-2 复述 + 载体层事实复述。
3. 新建 `scripts/test/cep_hop_timing_capabilities_check.sh`：六条判据臂 + 三条反向 fixture（每条**按正确原因红**）。
4. 新建 main spec `openspec/specs/cep-hop-timing-capability-declaration/spec.md`（**冻结面新增 1** ⇒ 五步链 1 次）。
5. 计数同步面（闸 51→52 · ctest 203→204 · `scripts/README.md` · 登记册 §0.1 · `include/tlm/cep/AGENTS.md` 文件索引）。

### Out-of-Scope（⛔ 逐条列明）

| 不做项 | 理由 |
|---|---|
| **补齐 hop / 时序模型** | 属协议 FSM 语义设计 ⇒ per 宪章归 **ArchForge**；本单元**只登记现状**（与 G1 同纪律） |
| 扩 `protocol_instance.schema.json` 的 13 键 | 冻结面 + `ADR-CEP-12` R-2e ⇒ 须重开 R-2 + 用户 ratify ⇒ **零 schema 变更** |
| 改 `protocol_instances.json` | 同上（R-2e）⇒ 声明面是 **C++ constexpr 表**（同 I1） |
| 改任何 ADR 正文 | 零决策变更 |
| 把 `tick()` 称作「时序模型」 | ⛔ `tick()` 是**全局步进钩子**（F2）⇒ 表述错误 = 假绿 |
| 给载体层（`cohif_*`）加 hop / 时序能力 | 载体层**已有**（F3）⇒ 本单元只**登记**其存在（防声明失真），不扩展 |

## Why

| 事实 | 一手出处 |
|---|---|
| 协议语义层零 hop / 时序 API（F1） | `include/tlm/cep/*_protocol_engine.hh` 实测 |
| G10 的依赖（G1）已闭 ⇒ 本单元可开工 | `openspec/changes/archive/2026-11-23-cep-protocol-engine-capability-declaration` |
| 「无 hop / 时序」若不限定层 = **误读**（载体层已有，F3） | `cohif_nic.hh:233` · `cohif_responder.hh:53` · `stub_cache_core.hh:158` |
| I1 的现成范式可直接照抄（零发明） | `include/tlm/cep/engine_capabilities.hh` + `scripts/test/cep_engine_capabilities_check.sh` |

**为什么现在做**：G10 的**唯一前置**是 G1，已闭；本单元是「让缺口可检出」的**低成本**部分（⛔ 不补能力）⇒ 无待决架构问题 ⇒ 可直接实施。

## What Changes

### 声明面字段（8 行 · 逐行给依据）

| 实例 | `protocol_hop` | `protocol_timing` | 依据（basis） |
|---|:--:|:--:|---|
| `chi_rnf` | None | None | `chi_rnf_protocol_engine.hh` 公开面 = `tick()` 步进钩子 + `emit*` 三处发射；零 hop / 时序 API |
| `mesif` | None | None | `mesif_protocol_engine.hh` 同型（`tick()` + `emit_snoop_resp`） |
| `moesi_gpu` | None | None | 决策 M-6：canonical 10 流 `on_*` 只记账；零 hop / 时序 API |
| `tilelink_c` | None | None | 事件计数 / 上报器；零 hop / 时序 API |
| `ualink_4` | None | None | 零发射、零状态；零 hop / 时序 API |
| `cxl_2_0` | None | None | 无独立引擎头 ⇒ 协议语义层零（编译期飞地） |
| `cxl_3_0_symmetric` | None | None | 同上 |
| `nvlink_4` | None | None | 同上 |

### 判据骨架

| 判据 | 形态 | 落点 |
|---|---|---|
| **J-1** 实例集双向零 orphan | 运行期 + 闸 | 声明表 id 集 == `protocol_instances.json` id 集 |
| **J-2** 声明自洽 | 运行期 + 闸 | `basis` 非空 ∧ id 唯一 |
| **J-3** 协议语义层 hop / 时序 **恒 None**（现状锁） | 闸 | 任一行 ≠ None ⇒ 必红 |
| **J-4** 引擎层 hop / 时序 API **反向存在性** | 闸（去注释扫描） | 任一引擎头出现 hop / 时序 API ⇒ 必红 ⇒ 强制更新声明 + 走决策 |
| **J-5** 编译期**否定断言** | 编译期 | 5 个实例引擎类型上 `static_assert(!HasHopApi<E>)` ∧ `!HasTimingApi<E>` |
| **J-6** 载体层 hop / 时序 **存在性** | 闸 | CohIf `hops` + responder / stub cache 的 `*_latency_cycles` 仍在 ⇒ 被删必红（防声明失真） |
| **J-7** 与 G1 **正交** | 闸 | 声明表区段内不得出现 `egress` / `in_flight` 字样 |
| **J-8** 诚实边界**双处登记** | 闸 | 头文件须显式登记「`tick()` ≠ 时序模型」+「载体层已有 hop / 时序 ⇒ 仅协议语义层为无」 |

## Acceptance

- [ ] **AC-1（通用层零协议名 / 零 include 协议头）**：声明头不得出现协议**类名**、不得 `#include` 协议引擎 / adapter 头 ⇒ 闸判红。
- [ ] **AC-2（实例集双向零 orphan）**：声明表 id 集 == `protocol_instances.json` 的 id 集（**8**）⇒ 双向均红。
- [ ] **AC-3（协议语义层 hop / 时序 恒 None · 现状锁）**：任一行 `protocol_hop` / `protocol_timing` ≠ `None` ⇒ 必红（⇒ 有人实现了协议语义层 hop / 时序 ⇒ 必须走决策 + 更新声明）。
- [ ] **AC-4（引擎层 hop / 时序 API 反向存在性）**：`*_protocol_engine.hh` **去注释后**出现 hop / 时序 API token ⇒ 必红（**核心「缺口可检出」机制**）。
- [ ] **AC-5（编译期否定断言）**：5 个实例引擎类型上 `static_assert(!HasHopApi<E>)` ∧ `static_assert(!HasTimingApi<E>)` ⇒ 引擎新增该类 API ⇒ **编译失败**。
- [ ] **AC-6（载体层 hop / 时序 存在性）**：CohIf flit `hops` + responder `completion_latency_cycles` + stub cache `snoop_latency_cycles` 仍在 ⇒ 任一被删 ⇒ 必红（防声明失真）。
- [ ] **AC-7（`basis` 非空 + id 唯一）**。
- [ ] **AC-8（与 G1 正交 · 不重复真源）**：声明表区段内不得出现 `egress` / `in_flight` 字样 ⇒ 必红。
- [ ] **AC-9（`tick()` ≠ 时序模型 · 诚实边界）**：头文件须显式登记「`tick()` 是全局步进钩子，⛔ 不是消息级时序模型」⇒ 闸 grep 命中该说明。
- [ ] **AC-10（载体层 ≠ 协议语义层 · 防误读）**：头文件须显式登记「载体 / 器件层已有 hop / 时延概念（3 处）⇒『无 hop / 时序』仅对**协议语义层**成立」⇒ 闸 grep 命中。
- [ ] **AC-11（三条反向 fixture · 按正确原因红）**：① 声明表加一行 ⇒ AC-2 红 ② 引擎头注入 `last_hops()` ⇒ AC-4 红 ③ 声明 `protocol_hop = CounterOnly` ⇒ AC-3 红 ⇒ 每条须**命中自己的红因签名**（⛔ 「rc≠0」不构成红因）。
- [ ] **AC-12（main spec + 五步链）**：新建 `openspec/specs/cep-hop-timing-capability-declaration/spec.md` ⇒ 冻结面**新增 1** ⇒ `--update` 期望「**新增 1 · 移出 0 · hash 变更 0**」· `artifact_count` 92 → **93** · 清单单独 commit · 无参复验 93 产物 hash 一致；`openspec validate --strict` rc=0。
- [ ] **AC-13（计数同步面）**：闸 **51 → 52** · ctest **203 → 204** · `scripts/README.md` 守门表新增行 · 登记册 §0.1「闸族 **52 + 14 = 66**」· `include/tlm/cep/AGENTS.md` 文件索引新增本头（**并补登 I1 漏掉的 `engine_capabilities.hh`**）⇒ `cep_registry_baseline_check.py` rc=0。
- [ ] **AC-14（零回归 · **全量**口径）**：**全量** `ctest --test-dir build`（⛔ **不按 `-R` 过滤** —— 过滤会按名字排除判据面，per 台账 Entry #542 ⑪⑫ 教训）· 全部 `cep_*.sh` · 非 `cep` 只读闸 · `docs_sync --strict` ⇒ 全绿。
- [ ] **AC-15（零决策变更 · 零 schema 变更）**：`protocol_instance.schema.json` / `protocol_instances.json` / 任何 `docs/adr/*.md` 正文 ⇒ **零 diff**（`git diff --name-only` 实测）。
- [ ] **AC-16（不新增 invariant 编号）**：invariant 集合**逐字不变**（`cep_invariant_map_check.py` rc=0）⇒ 本单元**不引入**任何 invariant（纯现状声明面）。
- [ ] **AC-17（诚实边界双处登记）**：本单元**只登记现状**、⛔ 不补 hop / 时序模型（属协议 FSM 语义设计，归 ArchForge）⇒ 头文件 + main spec 的 Purpose **双处**声明。

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-hop-timing-capability-declaration` | **ADDED** | 新能力：协议语义层的 hop / 时序建模承载能力**声明面**（8 实例）+ 反向存在性判据 ⇒ 与 G1 的 `cep-protocol-engine-capability-declaration` **正交**（前者 = 引擎存在性 / egress / 在飞；本能力 = hop / 时序） |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `include/tlm/cep/hop_timing_capabilities.hh` · `test/test_cep_hop_timing_capabilities.cc` · `scripts/test/cep_hop_timing_capabilities_check.sh` · `openspec/specs/cep-hop-timing-capability-declaration/spec.md` |
| **修改文件** | `test/CMakeLists.txt`（注册 ctest）· `scripts/README.md`（守门表）· `.rddf/roadmap/objectives/obj-cep-platform-remaining.md`（§0.1 闸族计数）· `include/tlm/cep/AGENTS.md`（文件索引）· 台账 `obj-cep-platform.md`（Entry + 计数） |
| **零改动（硬查）** | `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/protocol_instances.json` · `include/tlm/cep/engine_capabilities.hh` · `include/tlm/cep/*_protocol_engine.hh`（**零协议头改动** ⇒ 编译期探针以**否定断言**形态放在测试里）· 任何 `docs/adr/*.md` 正文 · `test/cep_invariant_assertion_map.json` |
| **冻结面** | **1 处**（新建 main spec ⇒ **新增 1**）⇒ 五步链 **1 次** |
| **闸** | **+1**（51 → 52）· ctest **+1**（203 → 204） |
| **测试** | `test_cep_*.cc` **+1**（78 → 79） |
| **KPI** | 本单元占 1 slot（活口径 1/3）⇒ 归档后释放回 0/3 |
| **风险** | ① 表述误读 ⇒ 把载体层已有 hop / 时序说成「全平台无」⇒ **假绿**（⇒ AC-10 + 双处诚实边界）② 把 `tick()` 称作时序模型 ⇒ 假绿（⇒ AC-9）③ 判定退化为「检查一个全 None 的表」⇒ 无牙（⇒ AC-4/AC-5/AC-11 三条**有牙**判据）④ 与 G1 的真源重叠 ⇒ 漂移面（⇒ AC-8） |

## 关联

gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md` **G10**（+ 依赖 **G1**）· I1 `cep-protocol-engine-capability-declaration`（**本单元的形态模板**）· `ADR-CEP-19`（CohIf 载体形态 —— 载体层 hop / 时延的来源）· `ADR-CEP-13`（一致性公理 —— 时序维度的最终消费者）· `ADR-CEP-20`（能力阶梯）· 台账 Entry **#543**（立项授权）
