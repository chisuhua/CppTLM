---
name: cep-fence-consistency-effect-derivation
priority: P1
phase: phase-10
主题: G4 —— fence 一致性效果的派生声明面（由一致性标签 + barrier 支持推导）
依赖:
  - ADR-CEP-12
  - ADR-CEP-13
状态: "🟡 **待审批**(2026-11-24 · per gap 分析 §2.3.1 素材盘点结论：素材齐全 · 零 schema 变更形态可行；⛔ 若裁决要写回 schema ⇒ 转 architect + 五步链)"
created: 2026-11-24
---

# cep-fence-consistency-effect-derivation: fence 一致性效果的派生声明面

> **来源**：gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md` **G4**（fence 无一致性标签 · 论文一致性接口第三元缺失）
> + **§2.3.1 素材盘点结论**（2026-11-24 · planner 一次性调研 · 零写入）⇒ **素材齐全** · **存在零 schema 变更的实现形态**
> **形态**：**照抄 G10**（派生声明面 + 闸 + 反向 fixture）⇒ 零发明
> **定位**：**只登记现状 + 让缺口可检出** ⛔ **不改** schema ⛔ **不改** instances ⛔ **不改** ADR 正文 ⛔ 零 invariant 新增

## 架构依据

### 1. 论文侧（G4 的语义来源）

- 一致性接口 = read / write / **fence**（HeteroGen HPCA2022 p.3 §II-B）⇒ fence 是**一等操作**。
- **fence 效果按一致性模型而异**：SC 无 / TSO 全屏障 / RC release·acquire / PLO 只补 W→W 与 R→W。
- **论文没有「fence 标签」**：标签是 read·write 方向的**模型标签**（Table I）⇒ fence 是**组合效果**，不是第三个标签值。

### 2. 本仓侧（现状 · 2026-11-24 实测）

- canonical 流 9 `barrier` **存在**（requester-scoped · per ADR-CEP-02 决策 3）⇒ **无需新增 canonical 流**。
- `protocol_instances.json` 的 `consistency_labels` = read / write（**无 fence**）。
- 8 实例的 `flow_support.barrier`：`mesif` / `moesi_gpu` / `nvlink_4` = true，其余 5 个 = false。

### 3. 不做会怎样

fence 的效果既不被声明面表达、也不被闸校验 ⇒ 任何人新增 barrier 流或改写一致性标签时，
**无判据要求同步 fence 效果** ⇒ 与 G10 实施前的状态同型（散文）⇒ **缺口不可检出**；
且「fence 无标签」易被误读为「fence 无关」⇒ **假绿**。

## 范围

### 做

1. **派生声明面**（新增通用层头 · 协议中立）：由 `consistency_labels`（read/write）+ `flow_support.barrier`
   **推导**每个实例的 fence 效果，枚举取值域 = 论文四行（none / full-barrier / release-acquire / partial），
   ⛔ **不新增标签枚举值**（⛔ 那是 architect 判权域）。
2. **闸**：派生值与论文表**逐实例一致**（机器可检）· 标签/barrier 任一被改 ⇒ 派生值随之变 ⇒ **不误报**。
3. **反向 fixture**：注入越界标签组合 ⇒ 必红（红因有签名）。

### 不做（⛔ 边界）

| 不做项 | 理由 | 归属 |
|---|---|---|
| 改 `protocol_instance.schema.json`（加 `fence` 子字段） | 冻结面 ⇒ 五步链；且**语义上**是把「组合效果」误当「标签」 | architect |
| 改 `protocol_instances.json` 实例声明面 | 同上（冻结面镜像比对面） | architect |
| 改 `ADR-CEP-13` 正文 | 冻结面 ⇒ 五步链 | architect |
| 实现 fence 的**协议语义** | 属协议 FSM 语义设计，归 ArchForge | ArchForge |

## Why

| 事实 | 一手出处 |
|---|---|
| 论文的 fence 效果表（SC / TSO / RC / PLO 四行）与标签表**已在仓内**调研文档 | `docs/research/cep-coherence-protocol-space-survey.md` §4.3.1 / §4.3.2 · :961 |
| 本仓已有推导所需二维：一致性标签 + barrier 支持 | `include/tlm/cep/protocol_instances.json`（`consistency_labels` · `flow_support.barrier`） |
| fence 效果**可**零 schema 变更地表达 ⇒ 不必等 architect 就能让缺口可检出 | §2.3.1 盘点结论（实例 × 标签 × barrier ⇒ 派生值表） |

**不做的代价**：fence 效果继续以**散文**存在 ⇒ 标签 / barrier 漂移无人发现 ⇒ 且「fence 无标签」易被误读。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **派生声明面**（新头） | 通用层 · 协议中立 · 零协议名；8 实例 × fence 效果派生值 + 逐行 `basis` + 机器可读边界常量 |
| 2 | **闸**（新脚本） | 派生值 ⇔ 论文表一致（逐实例）+ 标签/barrier 漂移检测 + 反向 fixture（各自红因签名）+ `--root` / `--selftest` |
| 3 | **测试**（新 cc） | 运行期复述派生值 + 边界常量断言 + 变异副本负控 |
| 4 | **main spec**（新建） | **冻结面新增 1** ⇒ 五步链 1 次（93 → 94） |
| 5 | **计数同步面** | 闸 +1 · ctest +2 · test_cep_*.cc +1 · 主 spec +1 · README + 登记册 §0.1 + cep/AGENTS.md 索引 |

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。

- [ ] **AC-1**：派生声明面**零协议类名** / 零 include 协议头（闸判红）。
- [ ] **AC-2**：实例集**双向零 orphan**（派生表 id 集 == `protocol_instances.json` id 集）。
- [ ] **AC-3**：**派生正确性** —— 每个实例的派生 fence 效果与论文四行表**逐实例一致**（机器可检）。
- [ ] **AC-4**：**推导依据可追溯** —— 每行有非空 `basis`（指向该实例的标签 + barrier + 论文行）。
- [ ] **AC-5**：**边界诚实** —— 明确「派生值 ≠ fence 协议语义」+「无 barrier 流 ⇒ 效果 none」+「SC 标签 ⇒ fence 无额外效果」。
- [ ] **AC-6**：**反向 fixture** —— 注入越界标签组合 ⇒ 必红（命中自己的红因签名）。
- [ ] **AC-7**：**漂移检测** —— 标签 / barrier 任一被改 ⇒ 派生值**随之变化**（闸不误报为红，但派生表须同步 ⇒ 由反向 fixture 覆盖）。
- [ ] **AC-8**：**零冻结面变更**（除新建 main spec）—— schema / instances / ADR 正文 **零 diff**。
- [ ] **AC-9**：**零回归**（全量口径）：52+ 闸全量 + 全量 ctest（⛔ 不按 `-R` 过滤）+ 非 cep 只读闸 + `docs_sync --strict`。
- [ ] **AC-10**：**计数同步面** + `cep_registry_baseline_check.py` rc=0。

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-fence-consistency-effect-derivation` | **ADDED** | 新能力：fence 一致性效果的**派生声明面**（8 实例）⇒ 与 G1 / G10 **正交**（G10 = hop / 时序；本单元 = fence 效果） |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | 派生声明面头 · 对应测试 · 契约闸 · main spec |
| **修改文件** | `test/CMakeLists.txt` · `scripts/README.md` · 登记册 §0.1 · `include/tlm/cep/AGENTS.md` |
| **零改动（硬查）** | `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/protocol_instances.json` · 任何 `docs/adr/*.md` 正文 · `test/cep_invariant_assertion_map.json` |
| **冻结面** | **1 处**（新建 main spec）⇒ 五步链 **1 次** |
| **风险** | ① 派生值被误读为「fence 协议语义」⇒ 假绿（⇒ AC-5 双处诚实边界）② 论文四行表的「PLO / TSO」在本仓无实例 ⇒ 枚举取值域仍保留（供未来实例）③ 派生依赖标签 ⇒ 标签漂移 ⇒ 派生漂移（⇒ AC-7 由 fixture 覆盖） |

## 关联

gap 分析 **G4** + §2.3.1 素材盘点结论 · **G10** `cep-hop-timing-capability-declaration`（形态模板）· **G1** `cep-protocol-engine-capability-declaration`（正交参照）· `ADR-CEP-12`（能力映射表）· `ADR-CEP-13`（一致性域归属）· `ADR-CEP-02` 决策 3（barrier = requester-scoped）· 论文侧调研真源 `docs/research/cep-coherence-protocol-space-survey.md` §4.3.1 / §4.3.2
