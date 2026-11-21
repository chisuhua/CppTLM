# cep-dod-reading-consistency Specification (delta)

## Purpose

CEP 的 **DoD 读法**（B3b-2 何时可登记为达成 · Stage 4+ 何时算进入）此前分散在三处载体上，
且**同一禁令有两个互斥读法** ⇒ N = 1 之日无人可裁。本 capability 把「**唯一权威声明处 +
其余载体只写指针**」这一形态**契约化**，并使其实例由**单一闸**机器守住。

本 capability **只约束形态**（声明落在哪、指针怎么写、判据怎么咬），⛔ 不改变任何计数、
分母或口径的 ratify 状态；权威声明的**正文**由 `docs/architecture/07-coherence-exploration-platform.md`
的 `## Status Update 2026-11-13` 承载，本 spec **只写指针**。

> **演进方式**：本 spec 经由 OpenSpec delta（**ADDED / MODIFIED / REMOVED / RENAMED**）演进，
> **不直接改写**本文件
> **归档前形态**（被评审与冻结的对象）：`openspec/changes/cpptlm-cep-dod-reading-consistency/specs/cep-dod-reading-consistency/spec.md`
> **规范载体**：`docs/architecture/07-coherence-exploration-platform.md` 的 `## Status Update 2026-11-13`
> **canonical 实现**：`scripts/test/cep_dod_reading_check.{sh,py}`

## ADDED Requirements

### Requirement: cep-dod-reading-single-authority

CEP 的 **DoD 读法口径** SHALL 有**唯一权威声明处**，且该处的**形态**与该处**指针的闭合性**
SHALL 由机器判据守住。

#### Scenario: 唯一权威声明处 SHALL 落在冻结件的新 Status Update

- **WHEN** 需要判定「B3b-2 何时可登记为 ✅」或「Stage 4+ 准入依据」
- **THEN** 权威声明 SHALL 是 `docs/architecture/07-coherence-exploration-platform.md` 的 `## Status Update 2026-11-13`
- **AND** 其余载体（`docs/guide/cep-usage.md` 的「四条禁写」第 3 条、台账 `objective-cep-platform` 的 §6.2 / §4.5 / §4.6）SHALL **只写指针**
- **AND** ⛔ 该等载体 SHALL NOT 复写权威声明的作用域正文（复写即产生第二个权威句 ⇒ 判据立刻判红）

#### Scenario: 声明与指针 SHALL 由单一闸守门

- **WHEN** 提交触及上述载体
- **THEN** 闸 `scripts/test/cep_dod_reading_check.sh` SHALL 以**闸内常量**（逐字整行锚）判定：
      新文本**整行唯一**、旧文本**零命中**、作用域**要素齐备**、指针**目标存在**
- **AND** 判据 SHALL 为「**整行全等** + 唯一承载文件 + 该文件内行次数 == 1」⇒ ⛔ 子串命中 SHALL NOT 被接受
- **AND** 每条判定 SHALL 有**稳定红因签名**；⛔ `rc ≠ 0` SHALL NOT 单独构成红因
- **AND** 判据 SHALL 配**决定性变异**（影子树内破坏 ⇒ 按签名红 ⇒ 还原 ⇒ 绿），且变异 SHALL NOT 改动真实仓

#### Scenario: 指针链 SHALL 单向闭合且目标存在

- **WHEN** 校验 `docs/guide/cep-usage.md` 与台账侧的指针
- **THEN** 每个指针 SHALL 解析到**存在**的目标（文件存在 + 目标标题在该文件内**恰 1 命中** + 该标题段内含必含 token）
- **AND** 三处指针 SHALL 指认**同一**权威声明处 ⇒ ⛔ 并列的第二条权威链 SHALL NOT 存在
- **AND** 目标段内出现「口径已生效」类越权表述 ⇒ SHALL 判 **FAIL**

#### Scenario: 本 capability SHALL NOT 改变任何计数或生效状态

- **WHEN** 本 capability 落地
- **THEN** B3b-2 的计数 SHALL **保持不变**（仍为 **0/4 ⏭**），分母 SHALL **不缩**，口径 ④ 的 ratify 状态 SHALL **不变**
- **AND** Stage 4+ 准入 SHALL 按**事件**认定，季度值 SHALL 仅作**算术派生值**
- **AND** ⛔ 本 capability SHALL NOT 把「读法已唯一化」表述为「DoD 已达成」

#### Scenario: 判据 SHALL 声明其不可约残余

- **WHEN** 交付本能力
- **THEN** SHALL 显式记录：① 闸只验**文本形态 + 出现次数 + 指针目标存在**，**不验**读者是否按该读法理解；
      ② 「整行唯一」只保证唯一性、**不保证正确性**；③ 手册该条自此**受 CI 锁定**（长期维护税）；
      ④ 本闸为**单根语义**（不解析被指物存在性）⇒ 覆盖不到「指针指向存在但语义错的节」
- **AND** ⛔ SHALL NOT 把「不可判 / 不空转」表述为「已覆盖 / 已证明」

> **锚点**：`docs/architecture/07-coherence-exploration-platform.md` 的 `## Status Update 2026-11-13` ·
> `docs/guide/cep-usage.md` 的「四条禁写」第 3 条 · `scripts/test/cep_dod_reading_check.py` 头注
