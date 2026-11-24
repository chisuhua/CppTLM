# proposal: cep-hop-timing-capability-declaration —— 协议语义层的「hop / 时序模型」承载能力声明面

> **来源**：gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md` **G10**（无网络 hop / 时序模型 · 依赖 **G1** · G1 已闭）· 台账 Entry **#543**（立项授权）· 用户指令「继续推进任务1」
> **形态**：**照抄 I1（G1）现成范式**（声明面 + 编译期反射 + 闸 + 反向 fixture）⇒ **零发明**
> **定位**：把「协议语义层**有没有** hop / 时序模型」从**散文缺口**变成**机器可检的现状面**。⛔ **只登记现状** · ⛔ **不补** hop / 时序模型（属协议 FSM 语义设计，归 ArchForge）· ⛔ 零 schema 变更 · ⛔ 零 ADR 正文变更 · ⛔ 零 invariant 新增。

## Why

| 事实 | 一手出处 |
|---|---|
| **协议语义层零 hop / 时序 API**：5 个实例引擎 + 1 个 CohIf Null 外壳，去注释后**零** hop / latency / travel-time / arrival-cycle 类 API | `include/tlm/cep/*_protocol_engine.hh`（实测 grep） |
| 引擎**有**步进钩子 `tick()` ⇒ ⛔ **全局步进**，**不是**消息级时序模型 | `protocol_engine_base.hh:319` · `chi_rnf_protocol_engine.hh:520` · `mesif_protocol_engine.hh:647` |
| **载体 / 器件层已有** hop / 时延概念 ⇒ 「无 hop / 时序」**仅对协议语义层成立** | CohIf flit `hops`（`cohif_nic.hh:233` + `:103`）· CohIf responder `completion_latency_cycles`（`:53`）· 桩缓存核 `snoop_latency_cycles`（`:158`） |
| 3 个实例**无独立引擎头** ⇒ 与 G1 同型：**编译期飞地** | `protocol_instances.json` · I1 的 `kEngineCapabilities` |
| G10 的**唯一前置**（G1）已闭 ⇒ 可开工 | `openspec/changes/archive/2026-11-23-cep-protocol-engine-capability-declaration` |

**不做的代价**：任何人新增协议语义层的 hop / 时序建模时，**无判据要求同步任何声明面** ⇒ 与 G1 实施前的状态同型（引擎头里的散文）⇒ 缺口**不可检出**；且「无 hop / 时序」若不限定层，会被误读为「全平台无」（载体层**已有**）⇒ **假绿**。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **声明面**（新头 `include/tlm/cep/hop_timing_capabilities.hh`） | 协议中立 · 通用层 · 零协议**类名** · 零 include 协议头；`kHopTimingCapabilities[8]`（逐行 `basis`）+ SFINAE 探针模板（`HasHopApi` / `HasTimingApi`） |
| 2 | **层的区分** | 声明只覆盖**协议语义层**（`protocol_hop` / `protocol_timing` 恒 `None`）；载体 / 器件层的既有 hop / 时延概念在**头文件诚实边界**里逐条登记（3 处）⇒ ⛔ 防「全平台无」的误读 |
| 3 | **现状锁**（J-3） | 任一行 `protocol_hop` / `protocol_timing` ≠ `None` ⇒ 必红 ⇒ 有人实现了协议语义层 hop / 时序 ⇒ **必须走决策 + 更新声明**（不得静默长出） |
| 4 | **反向存在性**（J-4 · 核心「缺口可检出」机制） | `*_protocol_engine.hh` **去注释后**出现 hop / 时序 API token ⇒ 必红 ⇒ 强制更新声明 + 走决策 |
| 5 | **编译期否定断言**（J-5） | 5 个实例引擎类型上 `static_assert(!HasHopApi<E>)` ∧ `static_assert(!HasTimingApi<E>)` ⇒ 引擎新增该类 API ⇒ **编译失败**（⛔ **零协议头改动** ⇒ 探针以否定断言形态放在测试里） |
| 6 | **载体层存在性**（J-6） | CohIf `hops` + responder / stub cache 的 `*_latency_cycles` 仍在 ⇒ 任一被删 ⇒ 必红（防声明失真） |
| 7 | **诚实边界双处登记**（J-8 / AC-9 / AC-10） | 头文件 + main spec 的 Purpose **双处**声明：`tick()` ≠ 时序模型 · 载体层已有 hop / 时延 ⇒ 仅协议语义层为无 |
| 8 | **闸**（新 `scripts/test/cep_hop_timing_capabilities_check.sh`） | 六判据臂 + **三条反向 fixture**（① 声明表加一行 ⇒ J-1 红 ② 引擎头注入 `last_hops()` ⇒ J-4 红 ③ 声明 `protocol_hop = CounterOnly` ⇒ J-3 红）⇒ 每条**按正确原因红**（红因有签名） |
| 9 | **main spec**（新建 `openspec/specs/cep-hop-timing-capability-declaration/spec.md`） | **冻结面新增 1** ⇒ 五步链 1 次（92 → 93） |
| 10 | **计数同步面** | 闸 51→52 · ctest 203→204 · `test_cep_*.cc` 78→79 · spec 139→140（`cep-*` 48→49）· `scripts/README.md` · 登记册 §0.1 · `include/tlm/cep/AGENTS.md` 文件索引（**并补登 I1 漏掉的 `engine_capabilities.hh`**） |

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **⚠️ 回归口径（per 台账 Entry #542 ⑪⑫ 教训）**：**零回归证据 = 全量 ctest（⛔ 不按 `-R` 过滤）+ 全部 `cep_*.sh` + 非 `cep` 只读闸 + `docs_sync --strict`**。

- [ ] **AC-1（通用层零协议名 / 零 include 协议头）**：声明头不得出现协议**类名**、不得 `#include` 协议引擎 / adapter 头 ⇒ 闸判红。
- [ ] **AC-2（实例集双向零 orphan）**：声明表 id 集 == `protocol_instances.json` 的 id 集（**8**）⇒ 双向均红。
- [ ] **AC-3（协议语义层 hop / 时序 恒 None · 现状锁）**：任一行 `protocol_hop` / `protocol_timing` ≠ `None` ⇒ 必红。
- [ ] **AC-4（引擎层 hop / 时序 API 反向存在性）**：`*_protocol_engine.hh` **去注释后**出现 hop / 时序 API token ⇒ 必红（**核心「缺口可检出」机制**）。
- [ ] **AC-5（编译期否定断言）**：5 个实例引擎类型上 `static_assert(!HasHopApi<E>)` ∧ `static_assert(!HasTimingApi<E>)` ⇒ 引擎新增该类 API ⇒ **编译失败**。
- [ ] **AC-6（载体层 hop / 时序 存在性）**：CohIf flit `hops` + responder `completion_latency_cycles` + stub cache `snoop_latency_cycles` 仍在 ⇒ 任一被删 ⇒ 必红。
- [ ] **AC-7（`basis` 非空 + id 唯一）**。
- [ ] **AC-8（与 G1 正交 · 不重复真源）**：声明表区段内不得出现 `egress` / `in_flight` 字样 ⇒ 必红。
- [ ] **AC-9（`tick()` ≠ 时序模型 · 诚实边界）**：头文件须显式登记「`tick()` 是全局步进钩子，⛔ 不是消息级时序模型」⇒ 闸 grep 命中该说明。
- [ ] **AC-10（载体层 ≠ 协议语义层 · 防误读）**：头文件须显式登记「载体 / 器件层已有 hop / 时延概念（3 处）⇒『无 hop / 时序』仅对**协议语义层**成立」⇒ 闸 grep 命中。
- [ ] **AC-11（三条反向 fixture · 按正确原因红）**：① 声明表加一行 ⇒ AC-2 红 ② 引擎头注入 `last_hops()` ⇒ AC-4 红 ③ 声明 `protocol_hop = CounterOnly` ⇒ AC-3 红 ⇒ 每条须**命中自己的红因签名**。
- [ ] **AC-12（main spec + 五步链）**：新建 main spec ⇒ 冻结面**新增 1** ⇒ `--update` 期望「**新增 1 · 移出 0 · hash 变更 0**」· `artifact_count` **92 → 93** · 清单单独 commit · 无参复验 93 产物 hash 一致；`openspec validate --strict` rc=0。
- [ ] **AC-13（计数同步面）**：闸 **51 → 52** · ctest **203 → 204** · `scripts/README.md` 守门表新增行 · 登记册 §0.1「闸族 **52 + 14 = 66**」· `include/tlm/cep/AGENTS.md` 文件索引新增本头（**并补登 I1 漏掉的 `engine_capabilities.hh`**）⇒ `cep_registry_baseline_check.py` rc=0。
- [ ] **AC-14（零回归 · 全量口径）**：**全量** `ctest --test-dir build`（⛔ **不按 `-R` 过滤**）· 全部 `cep_*.sh` · 非 `cep` 只读闸 · `docs_sync --strict` ⇒ 全绿。
- [ ] **AC-15（零决策变更 · 零 schema 变更）**：`protocol_instance.schema.json` / `protocol_instances.json` / 任何 `docs/adr/*.md` 正文 ⇒ **零 diff**（`git diff --name-only` 实测）。
- [ ] **AC-16（不新增 invariant 编号）**：invariant 集合**逐字不变**（`cep_invariant_map_check.py` rc=0）。
- [ ] **AC-17（诚实边界双处登记）**：本单元**只登记现状**、⛔ 不补 hop / 时序模型（归 ArchForge）⇒ 头文件 + main spec 的 Purpose **双处**声明。

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-hop-timing-capability-declaration` | **ADDED** | 新能力：协议语义层的 hop / 时序建模承载能力**声明面**（8 实例）+ 反向存在性判据 ⇒ 与 G1 的 `cep-protocol-engine-capability-declaration` **正交**（G1 = 引擎存在性 / egress / 在飞；本能力 = hop / 时序） |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `include/tlm/cep/hop_timing_capabilities.hh` · `test/test_cep_hop_timing_capabilities.cc` · `scripts/test/cep_hop_timing_capabilities_check.sh` · `openspec/specs/cep-hop-timing-capability-declaration/spec.md` |
| **修改文件** | `test/CMakeLists.txt`（注册 ctest）· `scripts/README.md`（守门表）· `.rddf/roadmap/objectives/obj-cep-platform-remaining.md`（§0.1 闸族计数）· `include/tlm/cep/AGENTS.md`（文件索引）· 台账 `obj-cep-platform.md` |
| **零改动（硬查）** | `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/protocol_instances.json` · `include/tlm/cep/engine_capabilities.hh` · `include/tlm/cep/*_protocol_engine.hh` · 任何 `docs/adr/*.md` 正文 · `test/cep_invariant_assertion_map.json` |
| **冻结面** | **1 处**（新建 main spec）⇒ 五步链 **1 次** |
| **闸 / 测试 / spec** | 闸 **+1**（51→52）· ctest **+1**（203→204）· `test_cep_*.cc` **+1**（78→79）· 主 spec **+1**（139→140 · `cep-*` 48→49） |
| **KPI** | 本单元占 1 slot（活口径 1/3）⇒ 归档后释放回 0/3 |
| **风险** | ① 「全平台无 hop / 时序」的误读 ⇒ 假绿（⇒ AC-10 + 双处诚实边界）② 把 `tick()` 称作时序模型 ⇒ 假绿（⇒ AC-9）③ 判定退化为「检查一张全 None 的表」⇒ 无牙（⇒ AC-4 / AC-5 / AC-11 三条**有牙**判据）④ 与 G1 的真源重叠 ⇒ 漂移面（⇒ AC-8） |

## 关联

gap 分析 **G10**（+ 依赖 **G1**）· I1 `cep-protocol-engine-capability-declaration`（形态模板）· `ADR-CEP-19`（CohIf 载体形态 —— 载体层 hop / 时延的来源）· `ADR-CEP-13`（一致性公理 —— 时序维度的最终消费者）· `ADR-CEP-20`（能力阶梯）· 台账 Entry **#543**（立项授权）· backlog `.rddf/improvements/cep-hop-timing-capability-declaration.md`
