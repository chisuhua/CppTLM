# Tasks — cpptlm-cep-canonical-flow-parser-scope v1.0(契约 / 判据基础设施修复,TDD 5-step)

> **状态**: 🟡 **Proposed**(2026-11-02)· **台账**: `obj-cep-platform.md` **Entry #149**
> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/cep-canonical-flow-parser-scope/spec.md`](specs/cep-canonical-flow-parser-scope/spec.md)
> **规划输入**: `.rddf/drafts/cpptlm-cep-canonical-flow-parser-scope/improvement.md`(**草稿已就绪,待 planner 落** `.rddf/improvements/`)
> **估时**: **1 周** · **KPI**: Proposed 并发 **0/3 → 1/3**

---

## 执行进度

| Phase | 状态 | 说明 |
|---|---|---|
| 0 治理前置 | 🟡 **进行中** | 台账 Entry #149 已落 + 4 件套已建 + improvement 草稿 staged ⇒ **待 planner 落** `.rddf/improvements/`(0.5);KPI 回填 ✅(0.7)· 冻结面实测 ✅(0.6 = **0 件**) |
| 1 修法 | ✅ **已完成**(2026-11-02) | `parse_canonical_flow_names` 范围限定(§2 段)—— TDD **红 ⇒ 绿** |
| 2 判据 | ✅ **已完成**(2026-11-02) | 4 个合成 ADR 文本用例(CFP-01 ~ CFP-04)—— **TDD 红已见证**(11 == 10 + 注入名计数 1) |
| 3 连带 | ✅ **已完成**(2026-11-02) | B5「闸边表 ≠ SU 边表」显式登记(台账 Entry #150)· 冻结面实测 = **0 件** ⇒ **无需两步授权** |
| 4 验收 + 归档 | ⬜ 待实施 | 门禁 + `ctest` + KPI 1/3 → 0/3 + 归档 |

---

## Phase 0 — 治理前置

- [x] 0.1 **台账裁决**(per §4.7.2 转正路径 step 0 ~ 3):KPI slot 实测 **0 个 active** ⇒ 可用;DRI 裁决 **① 建 change** ⇒ 台账 **Entry #149**
- [x] 0.2 **陷阱条件精确化 + 修法验证矩阵**(PoC 实测,ASCII 判据逐字对齐 C++)⇒ 台账 Entry #149 ② ③
- [x] 0.3 **4 件套建档**(`proposal.md` + `design.md` + delta spec + `tasks.md`)
- [x] 0.4 **improvement 草稿起草** → staged 于 `.rddf/drafts/cpptlm-cep-canonical-flow-parser-scope/`
- [ ] 0.5 ⛔ **planner 落 improvement 草稿** → `.rddf/improvements/cpptlm-cep-canonical-flow-parser-scope.md`(**planner 域**;`add-improve` HARD-GATE)
- [x] 0.6 **冻结面实测确认**:`include/tlm/cep/collective_plane_contract.hh` 是否在 M1 冻结集内 ⇒ 若在 ⇒ Phase 3 走两步授权
- [x] 0.7 **KPI baseline 回填**:7 处(`obj §5.2` / `obj §5.3` / `phase-10.md` / `.rddf/roadmap.md` / `AGENTS.md` / umbrella improvement / `obj §10` 行)

---

## Phase 1 — 修法(TDD:红 ⇒ 绿)

- [x] 1.1 **红**:先写 CFP-01 ~ CFP-03 的 3 个合成用例 ⇒ 在**现行实现**上必红(SU 注入 ⇒ 11 条 / §3 注入 ⇒ 11 条)
- [x] 1.2 **绿**:`parse_canonical_flow_names` 加「截取 `## 2.` 段」辅助 ⇒ 3 个用例转绿
- [x] 1.3 **重构**:辅助函数独立命名 + 注释说明**为何**限定范围(per `design.md` §3 的安全失败论证)
- [x] 1.4 **零回归**:`evaluate_canonical_flows` 的 collective 侧扫描逻辑**零改动**

---

## Phase 2 — 判据(CFP-01 ~ CFP-04)

- [x] 2.1 CFP-01 解析范围用例(§2 段外不入解析集)
- [x] 2.2 CFP-02 SU 注入 + §3 注入用例(各断言 == 10 且**不含**注入名 ⇒ 归因命中)
- [x] 2.3 CFP-03 结构异常用例(0 条 ⇒ FAIL + detail 含「权威源格式已变」)
- [x] 2.4 CFP-04 真实 ADR 零回归(**既有断言零改动**,只加一条「§2 段切片后仍 10 条」断言)
- [x] 2.5 **正控**:每个毒化用例的**基线**(干净文本)必须绿 ⇒ 铁律 ①

---

## Phase 3 — 连带

- [x] 3.1 **B5 登记**:「闸边表 ≠ SU 边表」差异(`protocol_instances.json` 的 `compute_noc` 边集 vs `ADR-CEP-09` SU 的 4 条边)⇒ 台账 §8.1 显式登记(不改代码)
- [x] 3.2 **冻结面**:若 Phase 0 的 0.6 实测 `collective_plane_contract.hh` **在**冻结集内 ⇒ 台账 Entry + `--update --authorized-by` 两步授权
- [x] 3.3 **反向验证形态说明**入台账(「毒化 ⇒ 仍绿」是**回归防护**,不是鉴别力 ⇒ 与既有 `*_reverse_verify.sh` 的差异须显式登记)

---

## Phase 4 — 验收 + 归档

- [ ] 4.1 `ctest` 全绿(含 `[cep][collective-contract]`)
- [ ] 4.2 `openspec validate cpptlm-cep-canonical-flow-parser-scope --strict` rc=0
- [ ] 4.3 门禁全 rc=0(`cep_docs_sync_check --strict` / `cep_gate_selftest` / `cep_doc_ref_check --strict` / `docs_sync_check --strict` / `cep_freeze_check` / `cep_decl_alignment_check`)+ `format.sh --check` rc=0
- [ ] 4.4 KPI **1/3 → 0/3** + 7 处 baseline 同步
- [ ] 4.5 **完成归档**(`openspec archive` ⇒ delta capability 合并进 `openspec/specs/cep-canonical-flow-parser-scope/spec.md`)
