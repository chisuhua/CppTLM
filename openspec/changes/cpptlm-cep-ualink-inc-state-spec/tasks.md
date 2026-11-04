# Tasks — cpptlm-cep-ualink-inc-state-spec v1.0(Stage 3 · 第 0 个 change · **规约层** · TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-04)· **队列**: `obj-cep-platform §4.0` **#14** 第 0 个 change(**规约层**)
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-ualink-inc-state-spec/spec.md`](specs/cep-ualink-inc-state-spec/spec.md)
> **口径**: 台账 **Entry #194**(improvement)· **#193**(三则 ADR SU + 冻结面授权)
> **审计面**: `docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md`
> **估时**: **4 周** · **KPI**: Proposed 并发 **0/3 → 1/3**
> **两段式**: 第 0 个 change = 本 change(**规约层**)· 第 1 个 change = `cpptlm-cep-ualink-inc`(**实现层**)

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | ✅ **已完成**(2026-11-04) | 转正登记 + KPI 回填 + Task 0 产物登记 |
| 1 载体形态定稿 | ✅ **已完成**(2026-11-04) | `ualink_inc_state_spec.hh` 自足载体 + 逐格 basis + 9 项 pending + 编译期断言 |
| 2 判据面 | ⬜ 待实施 | 正向测试 + **9 组**反向 fixture + 归因 id 元测试 + 探针/脚本 + ctest 注册 |
| 3 证书规约面 | ⬜ 待实施 | 四条证书的逐边依据 + 参照域 + `IC1` 鉴别力边界登记 |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + `validate` + KPI **1/3 → 0/3** + 归档 + 台账 Entry |

---

## Phase 0 — 治理前置

- [x] 0.1 **转正裁决** ⇒ 台账 Entry 登记
- [x] 0.2 **Task 0 产物登记**:`docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md`(✅ 已产出,2026-11-04)
      ⇒ **5 条公开来源**(`A-1` ~ `A-5`,sha256 钉版)· **9 项 pending**(`N-1` ~ `N-9`)· **9 个契约锚点**(`C-1` ~ `C-9`)
- [x] 0.3 **KPI baseline 回填**:**0/3 → 1/3**,**8 处**
- [x] 0.4 ⚠️ **合规偏差登记**:UALink Common 2.0 的规范正文受 **Evaluation Copy Agreement** 约束(实测)
      ⇒ 与本仓 **CXL ECA §3** 同型 ⇒ **禁投喂技术正文** ⇒ 与用户 2026-11-03 的裁决有偏差 ⇒ **登记不改裁决**
      (⚠️ **对本 change 的实质影响 = 零** —— 公开来源本就撑不住 9 项语义)

## Phase 1 — 载体形态定稿

- [x] 1.1 **`include/tlm/cep/ualink_inc_state_spec.hh`**(新 · header-only · **自足**)
      ⇒ ① **只 include 标准库** ② ⛔ 无平台标识 / 无项目内头 / 无区域表 / 无 `transition_*`(禁项清单,per `design §2.1`)
      ③ INC 四原语 id + 映射(per `design §2.2`)④ **逐格 basis**(每格有 `ref`)⑤ **9 项 pending**(逐项可报)
- [x] 1.2 **编译期结构断言**(带归因 id 文案):禁项清单的可判部分 + 原语映射 + pending 完备性
- [x] 1.3 **零改动核验**(实测:本阶段 `git diff --stat` 仅新增 1 头 + 1 测试,平台头 **零改动**):不碰 `collective_plane.hh` / `sharer_tracker.hh` / `protocol_instance*.json` /
      `fabric.json` / `collective_planes.*` / `ReduceOp` / canonical 10 流

## Phase 2 — 判据面

- [ ] 2.1 **正向测试**(`test/test_cep_ualink_inc_state_spec.cc`):每条**负向断言配正控**
- [ ] 2.2 **9 组反向 fixture**(每条**必红** + 归因 id **精确相等**):F-1 ~ F-9(per `design §4`)
- [ ] 2.3 **基线正控**(干净载体必绿)+ **鉴别力自测**(毒化载体 ⇒ 必红)
- [ ] 2.4 **归因 id 元测试**:fixture id ⊆ 本层登记集合(**双向**)+ 每个本层新建 id ≥ 1 条 fixture
- [ ] 2.5 **跨载体一致性元测试**(per `design §2.4`):X-1 原语覆盖 ⊆ `CollectiveOp` · X-2 pending 集合**逐项相等** · X-3 `self-derived` 格的 `ref` **不得**指 `A-n`
- [ ] 2.6 **探针 + 驱动脚本** + **ctest 注册**

## Phase 3 — 证书规约面

- [ ] 3.1 四条证书的**逐边依据 + 参照域**(两域并集)落载体
- [ ] 3.2 「该网映射不一致 ⇒ 不得用 M-2」的**可判定形式**
- [ ] 3.3 **`IC1` 鉴别力边界**登记(⛔ **不得**把「重跑 DAG」当证书)
- [ ] 3.4 **U-1 / U-2** 的登记形态(与 `N-n` 的关系写清)

## Phase 4 — 验收 + 归档

- [ ] 4.1 门禁全 rc=0 + `format.sh --check` rc=0
- [ ] 4.2 `ctest` 全绿 + **专项** `ctest -R cep_ualink_inc` + 记录 **case / assertion 数**
- [ ] 4.3 `openspec validate cpptlm-cep-ualink-inc-state-spec --strict` rc=0
- [ ] 4.4 KPI **1/3 → 0/3** 回填(**8 处**)
- [ ] 4.5 **归档**(delta 合并进 `openspec/specs/cep-ualink-inc-state-spec/spec.md`)+ `VIRTUAL_PATHS` 登记 + 台账 Entry

---

## ⚠️ 归档前必须闭合的**登记债**

| 项 | 位置 | 处置 |
|---|---|---|
| 9 项 pending 的转正路径 | 锚点摘要 §3 | ⛔ 本层**不**转正;需**新**的 Task 0 轮次(且需用户**明确**授权 Evaluation Copy 正文) |
| 载体名与 `CollectiveOp` 的跨载体一致性 | 元测试 X-1 | 本层**只**判「覆盖 ⊆」;**等值**由实现层的断言承担(载体自足 ⇒ 不能引用平台头) |
| 合规裁决的偏差 | 用户 2026-11-03 裁决 vs 实测 Evaluation Copy | **登记不改裁决**(台账);⇒ 本层按保守侧执行 |
