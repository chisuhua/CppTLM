# design: cep-hop-timing-capability-declaration —— 协议语义层的 hop / 时序建模声明面

> **来源**：`proposal.md`（同目录）· **gap**：G10 · **形态模板**：I1（`cep-protocol-engine-capability-declaration`）· **台账**：Entry **#543**
> **范围纪律**：⛔ **只登记现状** · ⛔ **不补** hop / 时序模型（归 ArchForge）· ⛔ 零 schema / 实例声明变更（R-2e）· ⛔ 零 ADR 正文变更 · ⛔ 零 invariant 新增 · ⛔ **零协议头改动** · ⛔ 与 G1 正交

---

## D-1 落点（2 个新代码面 + 2 个文档面）

| 文件 | 角色 | 内容 |
|---|---|---|
| `include/tlm/cep/hop_timing_capabilities.hh`（**新增**） | **声明面**（协议中立 · 零协议名） | `HopModel` / `TimingModel` 枚举 + `HopTimingCapability` struct + `kHopTimingCapabilities[8]`（逐行 `basis`）+ 自洽谓词 + **SFINAE 探针**（`has_hop_api_v` / `has_timing_api_v`）+ **3 个诚实边界常量** |
| `test/test_cep_hop_timing_capabilities.cc`（**新增**） | **判据测试** | 编译期否定断言（5 实例引擎 × 2）+ 探针**正控**（合成类型）+ 运行期 J-1 / J-2 / J-3 + 载体层事实复述 + 谓词变异副本负控 |
| `scripts/test/cep_hop_timing_capabilities_check.sh`（**新增**） | **契约闸** | 12 条判据 + **3 条反向 fixture**（各按自己的红因签名）+ `--root` / `--selftest` |
| `openspec/specs/cep-hop-timing-capability-declaration/spec.md`（**新增**） | **main spec**（**冻结面新增 1**） | 6 个 Requirement / 10 个 Scenario；delta 由主 spec **提取生成**（零漂移） |

⛔ **不改** `engine_capabilities.hh`（G1 的表）· ⛔ **不改**任何 `*_protocol_engine.hh` · ⛔ **不改** `protocol_instances.json` / `.schema.json`。

## D-2 声明面形态（8 行 · 逐行 `basis`）

- `protocol_hop` / `protocol_timing`：**8 行恒 `None`**（现状锁 ⇒ J-3）
- `basis`：逐行指向**实测位置**（引擎公开面 / 无引擎头的编译期飞地）
- 枚举值 `CounterOnly` / `ParameterizedDelay`：**当前仅**用于「载体层 / 器件层」的语义命名与反向 fixture ⇒ 协议语义层**现状恒 None**

## D-3 判据骨架

| 判据 | 形态 | 红因码 |
|---|---|---|
| **J-1** 实例集双向零 orphan | 闸 + 测试 | `hop-timing-orphan-missing` / `-extra` |
| **J-2** 声明自洽（`basis` 非空 ∧ `id` 唯一） | 闸 + 测试 | `hop-timing-empty-basis` / `-duplicate-id` |
| **J-3** 协议语义层 hop / 时序**恒 None**（现状锁） | 闸 | `hop-timing-protocol-model-present` |
| **J-4** 引擎层 hop / 时序 API **反向存在性**（去注释扫描） | 闸 | `hop-timing-engine-api-detected` |
| **J-5** 编译期**否定断言** | 编译期 | 失败签名含**实例 id 字面量** |
| **J-6** 载体 / 器件层 hop / 时延**存在性** | 闸 + 测试 | `hop-timing-carrier-fact-missing` |
| **J-7** 与 G1 **正交**（无 `egress` / `in_flight`） | 闸 | `hop-timing-duplicate-source` |
| **J-8** 诚实边界**机器可读**（3 常量 + 2 处措辞） | 闸 | `hop-timing-boundary-constant-missing` / `-wording-missing` |

### D-3.1 「层」的三分（**本单元的核心语义**）

| 层 | hop | 时序 | 判据归属 |
|---|:--:|:--:|---|
| **协议语义层**（8 实例的引擎） | ❌ | ❌ | 本声明表（J-3 现状锁 + J-4 反向存在性 + J-5 编译期否定断言） |
| **步进钩子** `tick()` | — | ⚠️ 有 | ⛔ **不是**消息级时序模型 ⇒ `kTickIsSteppingHookNotTimingModel` |
| **载体 / 器件层** | ✅ | ✅ | 事实**在**载体 / 器件头文件（J-6 守其不失真）⇒ `kHopTimingAbsenceIsProtocolSemanticsOnly` |

⚠️ J-4 的模式词表**刻意不含** `tick` —— 它是全局步进钩子，计入即误报。

## D-4 编译期否定断言 + 探针正控

- **零协议头改动** ⇒ 探针以**模板**形态放在通用头（`has_hop_api_v<T>` / `has_timing_api_v<T>`，优先级重载实现），**应用**在测试里（测试可 include 协议头）。
- ⚠️ **零实例特化是刻意的**：现状**无任何**实例有该能力 ⇒ 特化集为**空** ⇒ ⛔ 不写「假的空特化」（那会让声明面假装有编译期绑定）。
- **探针正控**（合成类型 `WithHop{last_hops()}` / `WithTiming{latency_cycles()}` / `WithNothing{}`）⇒ 探针无牙 ⇒ 判据假绿 ⇒ **必须**能被抓出来。

## D-5 载体层事实为何**不进**通用头

`cohif_nic.hh` / `cohif_responder.hh` / `stub_cache_core.hh` 的 hop / 时延事实**涉及具体载体名** ⇒ 通用头受 `include/tlm/cep/AGENTS.md` 铁律 2 约束（⛔ 不得出现协议名）⇒
⇒ 通用头只按**角色**登记「载体 / 器件层已有 hop 与时延」这一**件事实**（机器可读常量）；**具体落点**（3 个文件 + token）由**闸**与**测试**守（⛔ 闸与测试不受铁律 2 约束）。

## D-6 闸的 3 条反向 fixture（各按**自己的**红因签名）

| # | 变异 | 期望红因 |
|:--:|---|---|
| 1 | 声明表加一行 | `hop-timing-orphan-extra` |
| 2 | 引擎头注入 `inline int last_hops()` | `hop-timing-engine-api-detected` |
| 3 | 声明 `protocol_hop = CounterOnly` | `hop-timing-protocol-model-present` |

⇒ fixture 跑在**拷贝根**（`--root`）⇒ **零工作树污染**；每个 fixture 的**前置**（锚点命中）失败 ⇒ **显式失败**（⛔ 不得静默跳过 ⇒ 退化为「空变异必绿」）。

## D-7 计数同步面（AC-13）

| 面 | 变更 |
|---|---|
| `scripts/test/cep_*.sh` | **51 → 52** |
| ctest 条目 | **203 → 204**（新增 `cep_hop_timing_capabilities` + `cep_hop_timing_capabilities_check`） |
| `test/test_cep_*.cc` | **78 → 79** |
| 主 spec | **139 → 140**（`cep-*` **48 → 49**） |
| `scripts/README.md` | 守门表新增行 |
| 登记册 `obj-cep-platform-remaining.md` §0.1 | 闸族 **52 + 14 = 66** |
| `include/tlm/cep/AGENTS.md` | 文件索引新增本头（**并补登 I1 漏掉的 `engine_capabilities.hh`**） |

## D-8 冻结面（**新增 1** ⇒ 五步链 1 次）

- 新建 main spec ⇒ 命中 `openspec/specs/cep-*` 冻结 glob ⇒ `--update` 期望「**新增 1 · 移出 0 · hash 变更 0**」· `artifact_count` **92 → 93**。
- ⛔ 其余变更面（`include/` / `test/` / `scripts/`）**不在**冻结集 ⇒ 零 hash 变更。

---

## 判据落点（17 条 AC → 机检面）

| AC | 落点 |
|:--:|---|
| AC-1 | 闸（零协议类名 / 零 include 协议头） |
| AC-2 | 闸 + 测试（双向零 orphan） |
| AC-3 | 闸（现状锁）+ 测试（谓词负控） |
| AC-4 | 闸（去注释扫描）+ fixture ② |
| AC-5 | 测试（编译期否定断言 + 探针正控） |
| AC-6 | 闸 + 测试（载体层事实） |
| AC-7 | 闸 + 测试（basis / id 唯一） |
| AC-8 | 闸（区段内无 `egress` / `in_flight`） |
| AC-9 / AC-10 | 闸（3 常量 + 2 处措辞）+ 测试（常量断言） |
| AC-11 | 闸 `--selftest`（3 条 fixture · 各自红因签名） |
| AC-12 | main spec + delta（提取生成）+ 五步链 |
| AC-13 | 计数同步面（§D-7）+ `cep_registry_baseline_check.py` rc=0 |
| AC-14 | **全量** ctest（⛔ 不按 `-R` 过滤）+ 全部 `cep_*.sh` + 非 `cep` 只读闸 + `docs_sync --strict` |
| AC-15 | `git diff --name-only` 实测零 schema / 零 ADR 正文 |
| AC-16 | `cep_invariant_map_check.py` rc=0（零 invariant 新增） |
| AC-17 | 头文件 + main spec 的 Purpose **双处**诚实边界 |
