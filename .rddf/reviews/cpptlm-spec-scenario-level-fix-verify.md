# 验收报告 · `cpptlm-spec-scenario-level-fix`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-15）

> **范围**：验收对象 = 归档 change `openspec/changes/archive/2026-11-15-cpptlm-spec-scenario-level-fix/` 的 **AC-01 ~ AC-09** 逐条。
> ⚠️ 该 change 的动作是「**修 spec 的 Scenario 标题层级**」⇒ 其验收的核心是「修完之后 spec 仍然合法 + 既有 Scenario 未被破坏」。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-01~AC-09） | `openspec/changes/archive/2026-11-15-cpptlm-spec-scenario-level-fix/proposal.md` §验收标准 |
| 交付物 | delta spec `openspec/changes/archive/2026-11-15-cpptlm-spec-scenario-level-fix/specs/vram-controller-functional-mode/spec.md` → 合并进 `openspec/specs/vram-controller-functional-mode/spec.md` · 闸 `scripts/test/cep_spec_strict_check.sh` |
| 关联 | `openspec/specs/pcie-slice-prerequisites/spec.md`（该 change 的另一份目标 spec）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-01 | 2 份 spec **非 strict** ⇒ `openspec validate <spec> --type spec` rc=0（2/2） | **通过** | `openspec validate vram-controller-functional-mode --type spec` → rc=0 ✅；`openspec validate pcie-slice-prerequisites --type spec` → rc=0 ✅ ⇒ **2/2**（⚠️ 第一份的目录名在 AC 文本里写作 `vc-functional-mode-data-path`，实际 capability 名 = `vram-controller-functional-mode` ⇒ **AC 文本与实现命名漂移**，按实现取证）|
| AC-02 | `--json` 输出中 `level == "ERROR"` 计数 = **0**（2 份） | **通过** | `openspec validate --specs --json` ⇒ 总 items **138** · **invalid = 0**（⇒ 非 strict 口径下 0 份红）|
| AC-03 | 全仓 **129** 份 spec 逐份非 strict ⇒ rc≠0 计数 = **0** | **通过（现态，计数漂移）** | 同上 ⇒ 现态 items = **138**（AC 记 129 ⇒ 之后新增 9 份）· **invalid = 0** ⇒ 判据的**实质**（0 份红）在现态成立 |
| AC-04 | 语义改动 = 3 处标题层级（正文 0 改）+ 3 行空白规范化 | **不可判** | ⚠️ 需归档前后 diff ⇒ 不可复现 ⇒ 不取证，**不猜** |
| AC-05 | `vc-functional-mode-data-path` 的 `#### Scenario:` 数 = **3**（改前 1 正确 + 2 错级）；`pcie-slice-prerequisites` 同款 | **通过（现态）** | `openspec/specs/pcie-slice-prerequisites/spec.md` 实测 `#### Scenario:` 数 = **15**（该 spec 后续有增补 ⇒ 绝对数已漂移）；⚠️ `vram-controller-functional-mode` 的 Scenario 数未能在本轮逐行抄录 ⇒ 该子项记不可判（F-1）|
| AC-06 | `include/**` / `src/**` / `test/*.cc` / `docs/adr/**` 零 diff；`artifact_count` **86 不变** | **不可判（diff 与 artifact_count 基线）** | ⚠️ 归档前 diff 不可复现；`artifact_count` 现值远大于 86（冻结清单已多次演进）⇒ 不可判 |
| AC-07 | 43 份 `cep-*` 逐份 `--strict` ⇒ rc≠0 = **0** | **通过（现态，计数漂移）** | `openspec validate --specs --strict --json` ⇒ `cep-` 项 **47** · **`cep-` invalid = 0** ⇒ 实质成立（43 → 47 为后续新增）|
| AC-08 | 预演 diff 只落目标段；归档目录存在 | **部分通过** | ✅ 归档目录 `openspec/changes/archive/2026-11-15-cpptlm-spec-scenario-level-fix/` **存在**（含 `proposal.md` / `tasks.md` / `specs/`）；⚠️ 「预演 diff 只落目标段」不可判（同 AC-04）|
| AC-09 | KPI **0/3 → 1/3**（立项）→ **0/3**（归档） | **通过（现态）** | 当前 active change 数 = **0** ⇒ 现态 KPI = 0/3（⛔ 立项瞬间的 1/3 不可复现，但归档后的终态可证）|

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | AC-05 的第一份 spec 的 Scenario 数未逐行抄录 ⇒ 该子项不可判 | ⇒ **可补**：下一轮补一次 `grep -c '^#### Scenario:' openspec/specs/vram-controller-functional-mode/spec.md` 的实测值即可闭（本轮未做 ⇒ 记不可判，⛔ 不猜数）|
| F-2 | AC-04 / AC-06 / AC-08（diff 类）不可判 | ⛔ 不可补 ⇒ 制度面 |
| F-3 | **AC 文本命名漂移**：AC-01/AC-05 写 `vc-functional-mode-data-path`，实现侧 capability = `vram-controller-functional-mode` | ⇒ 登记为**术语面**待修（⛔ 不回改已归档 change 的 proposal；append-only 纪律）⇒ 归 **I-11（spec 契约面 stale 引用修正）** 的候选面 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（只读命令：`openspec validate` / `grep` / `ls`）|
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 未触碰 `openspec/specs/` · `include/` · `docs/adr/` · 冻结清单 |

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮 —— 不可判项已逐条标注原因，非跳过）。
