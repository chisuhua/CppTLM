# proposal: cep-ssp-cp-q1b-landing —— 用户自定义协议的装配期入口（contract 面 · `family` 4 值域 + `protocol_version` 两分支）

> **来源**：objective `.rddf/roadmap/objectives/obj-cep-custom-protocol.md` 的 **`CP-Q-1b`**（✅ 已 ratify：用户批准 R1~R4 · oracle 2 轮〔首审 `REJECT` → 8/8 闭合 → 复审 `APPROVE`〕· critic 1 轮 `APPROVE_WITH_CONDITIONS`〔**0 BLOCKER**〕）
> **架构依据**：`ADR-CEP-24` SU 第三则（决策 3.2 的 Q1/Q2/Q3 三项已裁 + §6 **V-24-13 取值域 3 → 4**）· `ADR-CEP-11` SU(2026-11-26)（决策 3 + `INV-CEP-11-IC1` + 决策 5 三处两分支化）· `ADR-CEP-12` SU(2026-11-26)（`family` 域 **3 → 4 值** + 13 键的用户可写划分）
> **形态**：**contract 面扩值域**（⛔ 零解释器 / 零装配集成 / 零掩码格 / 零实验 4 件套 —— 那 4 项属 `cep-ssp-batch2`）
> **优先级**：P1 | **阶段**：phase-10 | **分类**：core-impl | **类型**：feature
> **⚠️ 硬前置**：本 change 是 `cep-ssp-batch2` 的**唯一硬前置** —— 不落它，批 2 的 ⑦⑧⑨⑩ **全部无法验证**（用户的自定义实例在装配期**必然 fail**）

## Why

批 1 交付了「**能声明**」，但用户的自定义实例**过不了装配期** —— `family` 无法解析、`protocol_version` 不在锁表。⇒ 声明面**无法被任何装配路径消费** ⇒ 批 2 的 ⑦⑧⑨⑩ **全部无法验证**。

本 change 把「声明面」接到「装配期」的门口：让 `family` 有第 4 个可解析取值、让 `protocol_version` 有第二种可机判的合法形态。

`CP-Q-1b` 裁决（`.rddf/reviews/cep-CP-Q-1b-adjudication.md`，**已 ratify** 用户批准 R1~R4 · oracle 2 轮〔首审 `REJECT` → 8/8 闭合 → 复审 `APPROVE`〕· critic 1 轮 `APPROVE_WITH_CONDITIONS`〔**0 BLOCKER**〕）+ 其落盘的 3 份 ADR Status Update：

| ADR SU | 内容 |
|---|---|
| `ADR-CEP-24` SU 第三则 | 决策 3.2 的 Q1/Q2/Q3 **三项已裁** + §6 **V-24-13 取值域 3 → 4** |
| `ADR-CEP-11` SU(2026-11-26) | **决策 3 + `INV-CEP-11-IC1` + 决策 5** 三处**两分支化**（`user:<ssp-id>` 合法） |
| `ADR-CEP-12` SU(2026-11-26) | `family` 域 **3 → 4 值**（加 `user-defined`）+ 13 键的用户可写划分 |

**动因（为什么必须做）**：用户自定义协议在**装配期必然 fail** ——
- `family` 不属于既有 3 值域 ⇒ `[IAG-03]`「family 必须存在且可解析」红（`instance_assembly_gate.hh:141-148`）；
- `protocol_version` 不在 `ADR-CEP-11` §3 的锁定表内 ⇒ 决策 3 / `INV-CEP-11-IC1` 红；
- `protocol_version` 的 schema `pattern` = `^[a-z0-9]+(-[a-z0-9.]+)*$` **拒绝 `:`** ⇒ `user:<ssp-id>` 在 schema 层即被拒。

⇒ 本 change 是 `cep-ssp-batch2` 的**唯一硬前置**：不落它，批 2 的功能面**全部无法验证**。

## What Changes

**In Scope**（6 项 · contract 面）：

| # | 改动 | 落点 |
|:--:|---|---|
| 1 | `family` enum 加 `user-defined`；`protocol_version` 的 `pattern` 改**两分支** | `include/tlm/cep/protocol_instance.schema.json`（**M1 冻结面**）|
| 2 | `protocol_family.hh` **6 处**：`enum class ProtocolFamily` 加 `UserDefined = 4` · `kProtocolFamilyCount` 3→4 · `static_assert(kProtocolFamilyCount == 3)` → 4 · `kProtocolFamilies[]` 加条目 · `protocol_family_name` 的 `switch` 加 case · **新增** `static_assert(protocol_family_id(UserDefined) == 4)` | `include/tlm/cep/protocol_family.hh:43-53,56,79-89,106-113` |
| 3 | **IC1 通用闸两分支化** + pattern 容纳性用例纳入 `user:` 形态 | `test/test_cep_schema.cc:573`（IC1 通用闸）· `:512`（pattern 用例） |
| 4 | ⭐ **IC1 Scenario 两分支化** | `openspec/specs/cep-core-contracts/spec.md:335-339`（**M1 冻结面**）|
| 5 | ⭐ **共享 helper**：两分支谓词落**单一** helper（⛔ 禁在 IAG 另写一份） | 新/既有 helper（per `test_cep_schema.cc:552` 的 Entry #109 G-2）|
| 6 | 头文件注释「per `ADR-CEP-12` 的 `family` 域 —— 恰好 3 值」→ 4 值 | `protocol_family.hh:53` |

**Out of Scope**：

- ⛔ 解释器 / 装配集成 / 掩码格扩展 / 实验 4 件套（= `cep-ssp-batch2`）
- ⛔ `combination_contract` 的用户可写性（留 `cep-ssp-batch2` 的 open item）
- ⛔ **改动锁定表本身**（`ADR-CEP-11` §3 · `INV-CEP-11-IC2` 零触碰）
- ⛔ 任何 engine 行为 / 时序面
- ⛔ `INV-CEP-12-IC1` 的 13 必含**个数**

## Capabilities

### New Capabilities

- ⛔ **无**（本 change **零**新增能力面 ⇒ 新增 invariant 数 = **0**，IC1 号不变，只扩取值域）

### Modified Capabilities

- `cep-core-contracts`: `cep-protocol-version-locking-contract` 的 **IC1 Scenario 两分支化** —— `protocol_version` **必须**满足 **① 在 `ADR-CEP-11` §3 锁定表内** **或** **② 匹配 `^user:([a-z][a-z0-9_]*)$` 且 `<ssp-id>` ∈ `ssps.json` 已登记 `ssp.id`**；两者皆不满足 ⇒ **装配期 fail**（⛔ 不得静默降级）

## Impact

| 面 | 影响 |
|---|---|
| **冻结面** | `protocol_instance.schema.json`（enum + pattern）· `openspec/specs/cep-core-contracts/spec.md`（IC1 Scenario）⇒ **两处五步链** |
| 既有 8 实例 | **零行为变更**（`family` 取值不变，`protocol_version` 仍在锁表） |
| 新增 invariant | **0**（IC1 号不变，只扩取值域） |
| `ADR-CEP-11` §3 锁定表 | **零改动**（`INV-CEP-11-IC2` 保持） |
| **解锁** | `cep-ssp-batch2`（⑦⑧⑨⑩）—— ⚠️ 本 change 是它的**唯一硬前置** |
| ⚠️ 表述纪律 | 本 change 完成后 **仍不得**说「CEP 已支持用户自定义协议」—— 那只在 `cep-ssp-batch2` 验收后才成立 |

## 验收标准

- [ ] `family` enum 含 **4 值**；`protocol_family_from_name("user-defined")` 为 `true`，`protocol_family_id(UserDefined) == 4`
- [ ] `protocol_version` **接受** `user:<已登记 ssp-id>`；**拒绝** `user:<未登记>` / `chi-r3`（拼错的官方版本）/ `user:`（空 id）—— 4 条反向 fixture 各自按**正确原因**红
- [ ] **既有 8 实例全绿**（`protocol_version` 全在锁表 ⇒ 零回归）；批 1 的 `mesif_ssp` 不受影响（SSP 不绑实例）
- [ ] `openspec/specs/cep-core-contracts/spec.md` 的 IC1 Scenario 已两分支化（`grep "user:" <spec>` ≥ 1）
- [ ] 两分支谓词是**唯一**实现（共享 helper；⛔ 无第二份拷贝）
- [ ] 五步链完成：schema + spec **两处**冻结面 ⇒ `cep_freeze_check.sh` 无参 **rc=0**
- [ ] `test_cep_schema.cc` 的 `[cep][schema][ic1]` 用例全绿 + 新增 `user:` 用例

