---
name: cpptlm-cep-basis-source-kind-and-cell-free
priority: P1
phase: phase-10
依赖:
  - ADR-CEP-12
  - ADR-CEP-04
  - ADR-CEP-11
状态: 🟡 **已批准实施**(2026-11-06,per 台账 Entry #251 —— 用户裁决「P1-M1 + P1-M2 全量」)
created: 2026-11-06
---

# improvement: cpptlm-cep-basis-source-kind-and-cell-free

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

## 架构依据

`07 号` **SU 2026-11-06 · 四**（D-1 来源效力裁决，台账 Entry #249）要求：`Direct` = **规范正文条款级**，公开非规范来源不得冒充；但该 SU 同时诚实声明「**效力不可机器自动判**（载体无来源类型字段）⇒ 只能人工抽样」。

本条就是把这个「不可机器判」补成**可机器判**的载体：

- `ADR-CEP-12` SU 2026-11-06（认识论三档：`ualink_4` 13 键的 `public-nonnormative-derived` / `self-derived` 登记形态）—— 三档已有语义，缺**执行载体**；
- `ADR-CEP-04` SU 2026-11-06（`ualink_4` 取值裁决的 4 个公开来源）—— 来源清单已存在，缺**注册表化**；
- `ADR-CEP-11`（协议版本锁定）—— 来源必须与规范版本绑定（`sha256` + 日期）。

现状实测（4 套互不兼容的 basis 枚举 + 2 套 CellKind）：

| 载体 | basis 枚举 | CellKind | ref 类型 |
|---|---|---|---|
| `tilelink_state_spec.hh` | `Direct / Analogical / Inferred` | `Known / Unknown`（2 值） | `const char*` 自由串 |
| `cxl_state_spec.hh` | `PublicDirect / PublicAnalogical / ArchforgeAnchor / SelfDerived` | `Known / Illegal / Unknown`（3 值） | `const char*` 自由串 |
| `mesif_state_spec.hh` | `Primary / Structural / Derived / Analogical` | — | `const char*` 自由串 |
| `ualink_inc_state_spec.hh` | `SelfDerived / Pending` | — | 键级锚点 |

⇒ 同一「证据强度」概念有 4 个 token 名；任何载体改个枚举名即可**形式上合规**（命名绕过）。

## 范围

**In-Scope**：

1. **`RefKind` 三档唯一真源**（新头 `include/tlm/cep/source_registry.hh`）：
   `spec_clause`（规范正文 §/Table/Figure 级，可计 `Direct`）· `public_nonnormative`（论文 / 内核文档 / 厂商补丁 / webinar，**不得**计 `Direct`）· `repo_derived`（本仓推导）。
2. **来源注册表**：`include/tlm/cep/sources.json`（真源，人类可审计）+ 头文件内 `kSources[]`（编译期查找表），二者由闸脚本校验**逐条一致**。每条含 `id / kind / title / locator / sha256 / retrieved / license`。
3. **`ref_kind_of(ref)` 解析入口**：按注册表 source id 前缀（`D1` / `TLC-` / `UIS-` 等）解析来源类型，**逐格判定**，零逐格编辑。
4. **`basis` ↔ `ref_kind` 映射断言**：`Direct` / `Primary` ⇒ `spec_clause`；`PublicDirect` ⇒ `public_nonnormative`；`SelfDerived` / `Inferred` / `Derived` / `Pending` ⇒ `repo_derived`（或 `public_nonnormative` 且 `pending=true`）。违规 ⇒ 编译期或测试红。
5. **`CellKind::Free`**（tilelink 2→3 值、cxl 3→4 值）：语义 = 规范**明确**「实现定义 / 自由选择」，与 `Unknown`（规范未覆盖）严格区分；消费端 SHALL 走**合法**降级路径。
6. **决定性变异 3 条 + 闸脚本**（`scripts/test/cep_source_effect_check.sh`，影子树三态：clean / 删条目 / 改 kind）。
7. **现有 `[cep]` 测试适配**：4 载体相关断言按新入口改写；0 回归。

**Out-of-Scope（明确不做）**：

- ⛔ 不改任何引擎的转移逻辑（`*_protocol_engine.hh` 的消费掩码语义不变）；
- ⛔ 不重写既有格的 `basis` 取值（P0 carve-out：CXL 22 格 `PublicDirect` / MESIF `Primary` 维持原状，只加**类型校验**）；
- ⛔ 不改 `fabric.json` / `protocol_instance.schema.json`（M1 冻结面：41 产物，零 diff）；
- ⛔ 不改 `07 号` / ADR（append-only ⇒ D-1「不可机器判」的诚实声明更新交 architect 追加 SU，见 Impact）；
- ⛔ 不处理 4 族规约普查（B3b-2 的 T1/T4 缺口，本条只做载体机制）。

## Why

**DoD 压力下的唯一解法是让「效力」可判，而不是放宽标准。**

- 若继续人工抽样 ⇒ B3b-2 的 4 族永远「不可审计」，DoD 拆分（戊）后 B3b-2 的 ⏭ 也没有强度保证；
- 若直接放宽（允许公开来源计 `Direct`）⇒ 违反 `07 号` SU · 四 D-1，且矩阵格错会让引擎**静默实现错误转移**（`cxl_state_spec` 的零副本契约正是防这个）；
- `CellKind::Free` 缺失则「规范明确 implementation defined」被迫塞进 `Unknown`，导致 T4 完备性**低估**（明明规范已定义却算未覆盖）。

## What Changes

| 变更 | 文件 | 性质 |
|---|---|---|
| 新头 `source_registry.hh`（`RefKind` / `SourceEntry` / `kSources[]` / `ref_kind_of`） | `include/tlm/cep/` | 新增，header-only |
| 来源注册表真源 | `include/tlm/cep/sources.json` | 新增 |
| `CellKind::Free` + 消费端降级 | `tilelink_state_spec.hh` / `cxl_state_spec.hh`（+ 消费方 adapter 的 switch） | 枚举扩展 |
| `ref_kind` 一致性断言 | `test/test_cep_source_registry.cc`（新） | 新增测试 |
| 决定性变异闸 | `scripts/test/cep_source_effect_check.sh` | 新增脚本 |
| 现有测试适配 | `test/test_cep_tilelink_*.cc` / `test_cep_cxl_*.cc` / `test_cep_mesif_*.cc` / `test_cep_ualink_*.cc` | 断言改写 |

## Acceptance

- [ ] **AC-1**：`RefKind` 三档 + `sources.json` + `kSources[]` 落地；**4 载体的每一格 `ref` 均可解析到已登记来源**（未登记 = 0）；`sources.json` ↔ `kSources[]` 逐条一致（闸脚本比对）
- [ ] **AC-2**：`basis` ↔ `ref_kind` 映射断言 0 违规 —— **按载体**映射（⛔ 不得按 token 名跨载体套用）：tilelink 主矩阵/T3 `Direct` ⇒ `spec_clause`；cxl `PublicDirect` ⇒ `public_nonnormative`（⛔ 不得标 `spec_clause`）；mesif `Primary`（ArchForge 原文）⇒ `repo_derived` 且 `ref` 非空；ualink `SelfDerived` ⇒ `repo_derived` / `Pending` ⇒ `public_nonnormative`（此项是 D-1 的机器化落点）
- [ ] **AC-3**：`CellKind::Free` 在 tilelink + cxl 落地，且四态（`Known` / `Free` / `Unknown` / `Illegal`(仅 cxl)）**互斥可判**；消费端新增显式 `transition_is_free` 分支（`Free` **不得**被 `Known`/`Unknown` 吞掉）；⚠️ 当前真实 `Free` 格 = **0**（规范可读化后填充）⇒ 判据 = 枚举 + 互斥断言 + 消费端分支 + 零计数自洽，⛔ 不假装有真实格数据
- [ ] **AC-4**：决定性变异 3 条全部按正确原因红 —— ① 删 `sources.json` 一条目 ⇒ 闸红（指出缺失 id）② 把 `spec_clause` 来源改成 `public_nonnormative` 而 basis 仍 `Direct` ⇒ 闸红（指出格）③ `Free` 误当 `Unknown` ⇒ 测试红
- [ ] **AC-5**：0 回归 —— `[cep]` 全量绿（`ctest --test-dir build -j4` ≥154 项）+ M1 冻结面 41 产物零 diff + `cep_docs_sync_check.sh` / `cep_doc_ref_check.sh` / `cep_decl_alignment_check.sh` 全绿
- [ ] **AC-6**：遗留交接 —— 台账登记「`07 号` SU · 四 D-1 的『不可机器判』声明需 architect 追加 SU 更新」（builder 不改 ADR）

## Capabilities

- **来源效力可机器判**：每格证据的来源类型由注册表单一真源决定，改枚举名无法绕过；
- **DoD 审计可复算**：B3b-1 的 2 族（CHI/TileLink）与任何未来族的"是否满足 `Direct`"可由脚本复算，无需人工抽样；
- **规范自由语义可表达**：`CellKind::Free` 使 T4 完备性判定不再把「规范明确的实现自由」误算成「未覆盖」；
- **注册表可审计**：每条来源带 `sha256` + 日期 + 许可，满足 `ADR-CEP-11` 版本锁定要求。

## Impact

| 面 | 影响 |
|---|---|
| 公共头文件 | 新增 2 文件 + 2 载体枚举扩展（`CellKind`）⇒ **API 变更**，须走 OpenSpec change |
| 引擎 | 零逻辑变更；仅 `CellKind::Free` 的分支降级（若有 switch 需补 case） |
| M1 冻结面 | **零 diff**（`sources.json` 是新文件，不在 41 产物内） |
| 测试 | 4 载体相关断言适配；新增 1 个测试文件 + 1 个闸脚本；0 回归 |
| KPI | 占 1 个 OpenSpec slot（当前 0/3） |
| 遗留 | `07 号` SU · 四的「不可机器判」需 architect 追加 SU 更新（P1 完成后） |
| 风险 | 4 套 basis 枚举的映射若写错，会把合法格误判为违规 ⇒ AC-2 需逐载体核对映射表；`sources.json` 与 `kSources[]` 双载体有漂移风险 ⇒ AC-1 的闸比对兜底 |
| 估时 | **3-5d**（M1: 2d / M2: 1.5-2d） |
