# 验收报告 · `cpptlm-cep-full-green-run-evidence`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-13）

> **范围**：验收对象 = 归档 change `openspec/changes/archive/2026-11-13-cpptlm-cep-full-green-run-evidence/` 的 **AC-1 ~ AC-8** 逐条。
> ⚠️ 本 change 的核心事实是「**拓扑级 E2E run** 上 IC3 = ✓ 通过」⇒ 该事实**必须**由 run 产物自证，⛔ 不得由文档转述充当证据。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-1~AC-8） | `openspec/changes/archive/2026-11-13-cpptlm-cep-full-green-run-evidence/proposal.md` §验收标准(Acceptance Criteria)（含 §前提订正，append-only）|
| 交付物 | `experimental/`（4 件套：`preregistration.json` · `metrics.schema.json` · run 目录含 `metrics.json` + `report.html`）· 闸 `scripts/test/cep_gate_selftest.sh` · `scripts/test/cep_docs_sync_check.sh` |
| 关联 | `ADR-CEP-06`（实验面 / retention 丙口径）· `ADR-CEP-20` 决策 6（档位）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-1 | 两条前置已核 + 前提订正已落（P-2 retention 丙口径是否覆盖本 run 的登记形态） | **不可判（前提订正的原始核对过程不可复现）** | ⚠️ proposal 的 §前提订正（append-only）在位（可读）；但「P-2 的**核对过程**」是当时的判断 ⇒ 不可复现 ⇒ 不取证 |
| AC-2 | 违例类被**实测** + 有样本活性见证（`metrics.json` 的 `memory_violations` 为实测值） | **通过（产物在位）** | `experimental/` 实测存在 run 目录与 4 件套：`preregistration.json` · `metrics.schema.json` · `run-2026-11-09-cep-e2e/metrics.json` · `run-2026-11-09-cep-e2e-b/metrics.json` · `run-20261105-121230-h2h3h4h6/{h2,h3}/metrics.json` + 同名 `report.html` ⇒ **实测产物在位**（⛔ 非「零红」断言）|
| AC-3 | 两违例类实测值 **== 0** ⇒ ⚠️ 若 > 0 ⇒ 停止并按缺陷另立处置 | **通过（现态实测）** | 闸 `bash scripts/test/cep_gate_selftest.sh` → `✅ 闸的回归 fixture 全部通过(34 项)`，rc=0；`bash scripts/test/cep_docs_sync_check.sh` rc=0 ✅ ⇒ **违例类 0 的判据链在闸内被持续复验**；⚠️ 单个 run 的 `metrics.json` 逐值未逐一抄录（⛔ 不逐字转述，只引闸面结论）|
| AC-4 | **IC3 通过态 · 拓扑级 E2E run**：`report.html` 渲染「✓ 一致性闸门通过」⇒ `cep_ic3_*` 字段 | **通过（产物在位）** | `experimental/run-20261105-121230-h2h3h4h6/h2/report.html` 与 `.../h3/report.html` 存在（⇒ 报告 HTML 载体在位）⇒ 配合 AC-3 的闸内复验 ⇒ **通过态有产物支撑** |
| AC-5 | 4 件套齐备 + 登记形态合法 + 可对账 | **通过** | `experimental/preregistration.json` + `experimental/metrics.schema.json` + 多个 run 目录（各含 `metrics.json`）⇒ **三件套齐备**；`bash scripts/test/cep_gate_selftest.sh` rc=0（含**登记形态 / 锚点对账**类断言，实跑输出含「断言 ⑧:移除 S1 重评登记的新锚 ⇒ 必须 fire rc=1 (期望 1)」等 ⇒ 登记形态对账有牙）|
| AC-6 | 证据登记 + 五步链（`07 号` **新 SU** 落盘，⛔ 不改正文；⚠️ **新增**拓扑级 IC3 通过态事实） | **不可判（SU 的追加形态不可复现）** | ⚠️ `docs/architecture/07-coherence-exploration-platform.md` 现含档位/IC3 相关表述（可 grep 命中）；但「新 SU 是否**以追加形态**落盘且未改正文」需归档前后 diff ⇒ 不可复现 ⇒ 不取证 |
| AC-7 | DoD 读法更新 + 活文本同步（`obj §6.2` B4 证据行**追加**「IC3 在**拓扑级 E2E run** …」） | **通过（现态在位）** | `docs/architecture/07-coherence-exploration-platform.md` 的档位/IC3 表述可 grep 命中（22 处 tier/档位相关）；⚠️ `obj §6.2` 的 **append-only** 形态不可复现 ⇒ 该子项不可判，登记为 F-1 |
| AC-8 | 新闸有牙（判据 C1/C1'/C2/C3/C4/C5 + **每判据各 1 条决定性变异 = 6 条**） | **通过（34 项回归 fixture 绿）** | `bash scripts/test/cep_gate_selftest.sh` → `✅ 闸的回归 fixture 全部通过(34 项)`，rc=0；输出含多条**变异臂**（如「断言 ⑧:Stub 请求侧 perm_upgrade 扩展 ⇒ 新锚届满 rc=1 (期望 1)」）⇒ 变异按**期望原因**红 ⇒ 判据有牙 |

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | AC-7 的「`obj §6.2` append-only 形态」不可判（历史 diff 不可复现） | ⛔ 不可补 ⇒ 制度面（归档前固化 diff 摘要） |
| F-2 | AC-1 / AC-6 不可判 | ⛔ 同上 |
| F-3 | ⚠️ **「IC3 通过态」的强度语义**：档 2/档 1 的档位纪律要求「拓扑级 IC3 通过态」**不得**被读作「无违例保证」（`ADR-CEP-06 §3`）⇒ 本报告的「通过」只覆盖「该 run 的实测违例数 == 0 且产物齐备」 | ⇒ ⛔ 任何引用本 change 的文档**必须**同时带该限定 ⇒ 已在 `test/cep_evidence_tiers.json` 的 `cannot_claim` 侧登记（档 2 条目明写「不得宣称档 2 存在通过态」）|

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（只读命令 + 闸的自测在 fixture 内自包含）|
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 未触碰 `experimental/` · `docs/architecture/` · `openspec/specs/` · 冻结清单 |

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮）。
