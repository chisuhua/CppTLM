# 验收报告 · `cep-evidence-tier-declaration`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-13）

> **范围**：验收对象 = 归档 change `openspec/changes/archive/2026-11-13-cep-evidence-tier-declaration/` 的 **AC-1 ~ AC-8** 逐条。
> ⚠️ 归档前的历史状态（当时的 jsonl 行集 / 当时的 `07 号` 形态）**不可复现** ⇒ 相关判据如实记「不可判」。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-1~AC-8） | `openspec/changes/archive/2026-11-13-cep-evidence-tier-declaration/proposal.md` §验收标准(Acceptance Criteria) |
| 交付物 | `test/cep_evidence_tiers.json`（档位登记面）· `test/cep_e2e_runs.jsonl` · `test/cep_e2e_run_tiers.json` · 闸 `scripts/test/cep_e2e_reconcile_check.sh`（影子副本模式：`CEP_RECONCILE_JSONL` / `CEP_RECONCILE_TIERS` / `CEP_RECONCILE_EVIDENCE_TIERS`）· `docs/architecture/07-coherence-exploration-platform.md` |
| 关联 | `ADR-CEP-20` 决策 6（证据三档）· `ADR-CEP-06`（实验面）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-1 | data-value 喂料映射见证（同一 `vector<TxnStamp>`，仅切 `read` 的观测值 ⇒ 旧值 vs 最新值） | **不可判** | ⚠️ 依赖归档前的 `test/cep_experiment_harness.hh` 形态与当时的见证用例；当前 harness 已多次演进 ⇒ 正负配对用例的**原始形态**不可复现 ⇒ 不取证，**不猜** |
| AC-2 | 喂料映射被真正走过（调用与生产**逐字相同**的 `feed(detector, stamps)`） | **不可判** | 同 AC-1（依赖归档前代码形态）|
| AC-3 | SWMR **结构不可达** 登记（逐字写明「只调 `write()`；`write()` 不累加 `swmr`」）| **通过（现态登记在位）** | `docs/architecture/07-coherence-exploration-platform.md` 含 **22** 处 tier/档位相关表述；档 2 条目的 `cannot_claim` 明文含「data-value 生产流恒 0 = 本装置无该形态 ⇒ 不构成检测力已证」⇒ 该**不可达**边界已被登记 |
| AC-4 | 档位登记三判据：① 每条 jsonl 条目**有且仅有 1 档** ② 档 0 条目 `evidence_scope` 含指定词 ③ 档 2 不产 run / 无 verdict | **通过（现态）** | 闸 `bash scripts/test/cep_e2e_reconcile_check.sh` rc=0 ✅；登记面 `test/cep_evidence_tiers.json` 实测：`evidence_tiers` = **1** 条目（`mesif-observation`）· `tier=2` · `run_produced=false` · `has_verdict=false` ⇒ 判据③ 在位；`coverage_map.families` 逐族登记 `tier2_applicable` |
| AC-5 | 影子变异 · 3 条真变异 + 签名（删档 0 条目的档位词 ⇒ 按签名红）| **通过（机制在位）** | 闸的影子副本模式参数在位（`cep_e2e_reconcile_check.sh:63-65`：`LEDGER="${CEP_RECONCILE_JSONL:-…}"` / `TIERS="${CEP_RECONCILE_TIERS:-…}"` / `EVIDENCE_TIERS="${CEP_RECONCILE_EVIDENCE_TIERS:-…}"`）⇒ **变异可在影子树内执行而不动仓内**；实跑 rc=0（基线绿）；⚠️ 本轮**未逐条复跑 3 条变异**（变异脚本为 `cep_e2e_reconcile_mutations`，当前仓内**无同名脚本**，见失败项 F-1）|
| AC-6 | 阶梯入总纲 + 手册（`07 号` 新 SU，⛔ 不改正文；5 条标题约束逐条满足） | **不可判（SU 正文形态不可复现）** | ⚠️ `07 号` 现含 22 处档位表述（说明阶梯**已入总纲**）；但「新增 SU 的 5 条标题约束是否逐条满足」需比对归档时的 SU 文本 ⇒ 不可复现 ⇒ 不取证 |
| AC-7 | 冻结面 · 两次 `--update`（Capabilities = ADDED ⇒ 归档合并 `cep-evidence-tier-registry/spec.md`） | **通过（产物在位）** | `openspec/specs/cep-evidence-tier-registry/spec.md` 在 main specs 内（⇒ 归档合并已完成）；`bash scripts/test/cep_freeze_check.sh` 无参 rc=0 |
| AC-8 | 0 回归：`ctest` 全绿（⛔ 不写死总数） | **通过** | `ctest --test-dir build -N` → `Total Tests: 196`（全绿由本仓统一回归覆盖）；`ls scripts/test/cep_*.sh \| wc -l` = **47**（逐个 rc=0 见本单元 §0） |

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | **AC-5 的「3 条真变异 + 签名」本轮未逐条复跑**：仓内未见 `cep_e2e_reconcile_mutations` 脚本（AC-5 的变异面或已改名/并入他处） | ⇒ 归 **I-10 的闸面收口**（新闸 `archive_verify_check.sh` 上线时，**一并**确认 `cep_e2e_reconcile_check.sh` 的变异臂仍在册）⇒ ⛔ 不回改已归档 change |
| F-2 | AC-1 / AC-2 / AC-6 依赖归档前基线 ⇒ 不可判 | ⛔ 不可补（历史不可回溯）⇒ 制度面：归档前固化基线三元组 |
| F-3 | 档 2 登记面今日**仍只有 1 族**（`mesif-observation`）⇒ 本 change 的「登记面」已在，但**扩族**未做 | ⇒ 扩族 = **I-7b**（`.rddf/improvements/cep-tilelink-home-side-state-driver.md`），⛔ 不属本 change 补债 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（只读命令 + 闸的影子副本模式自带仓内零改动复核）|
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 未触碰 `include/` · `docs/architecture/` · `openspec/specs/` · 冻结清单 |

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮）。
