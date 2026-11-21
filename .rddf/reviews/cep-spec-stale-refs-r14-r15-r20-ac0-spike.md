# AC-0 · openspec 可行性 spike 结论（I-11）

- **角色**：builder · **日期**：2026-11-21
- **验收对象**：`.rddf/improvements/cep-spec-stale-refs-r14-r15-r20.md` §Acceptance **AC-0**（**开工前置**）
- **判据**：影子 change 跑通 `openspec validate` + 试归档 ⇒ ①② 各给「可行 / 不可行 + 原因」

## 结论摘要

| 问 | 结论 | 依据 |
|:--:|---|---|
| **①** `MODIFIED` delta 能否**只改文档 trailer 的状态行**（`pcie-ip-microarch/spec.md:296` 这类位于 `---` 之后、文件末的 `**维护**/**状态**`）？ | ⛔ **不可行** | 影子 change `zz-spike-r13`（delta 只含 trailer 两行）⇒ `openspec validate zz-spike-r13 --strict`：<br>· ⚠ `WARNING`：`Text before the first requirement in "## MODIFIED Requirements" is **not carried into the main spec by archive**`（⇒ 即使写了也**不会**生效）<br>· ✗ `ERROR`：`## MODIFIED Requirements were found, but no requirement entries parsed`（⇒ delta 必须至少 1 个 `### Requirement:` 块）|
| **②** `MODIFIED` delta 能否**只删 Requirement 内一行引用**（`pcie-slice-prerequisites/spec.md:14` 的 `abi_guards.h` 行）？ | ✅ **可行（但须重述整个 Requirement）** | 影子 change `zz-spike-r12`：把 32 行的 Requirement 块重述为 31 行（删该行）⇒ `openspec validate --strict` → `Change 'zz-spike-r12' is valid`；`openspec archive zz-spike-r12 -y` → `pcie-slice-prerequisites: update · ~ 1 modified` ⇒ 合并后 `grep -c abi_guards` == **0** |

## ①② 的现场取证

**① 的 delta（不合法）**

```markdown
## MODIFIED Requirements

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — SPIKE-已更新
```

⇒ 两条诊断（见上表）：`MODIFIED` 段内**无 `### Requirement:` 块** ⇒ ERROR；且该段首的散文**不被归档带进 main spec** ⇒ 即使不报错也无效。

**② 的 delta（合法）** = 完整重述 `### Requirement: pcie-slice-wire-format-in-repo-guarded` 块（标题 + 正文 + blockquote note + 全部 `#### Scenario:`），仅把含 `include/cudart/abi_guards.h` 的那一行删除。

**归档合并的实际 diff（影子内）**

```diff
 ## Purpose
 固化 dGPU 板级 SoC 切片落地前必须成立的前置条件：...
+
 ## Requirements
+
 ### Requirement: pcie-slice-wire-format-in-repo-guarded
@@
 > - UsrLinuxEmu 经 23 ABI C 符号消费,从不 memcpy 这些 TLM Bundle
-> - 真正跨仓 ABI 由 `include/cudart/abi_guards.h` G-D4 17 条静态断言承担 (per AGENTS.md CROSS-PROJECT)
@@
 **Maintenance**: CppTLM Team (Sisyphus)
 **Status**: 📝 Proposed — 待评审后启动 T-prereq-1~5 实施
-
  ---
```

⇒ 目标行被删 ✅；**附带两处格式变动**（`## Purpose` / `## Requirements` 之后各插入 1 空行；文件末空行被移除）⇒ 属 openspec 合并器的**固有行为**（⛔ 非本单元改动，diff 里必须预期它）。

## ⚠️ 同批发现（影响 R-13 的修法判断）

- `pcie-ip-microarch:296` 与 `pcie-ip-integration:53` 的 `**维护**/**状态**` 位于**文档 trailer**（最后一个 `---` 之后），**不属于任何 Requirement** ⇒ ① 的结论直接适用于它们 ⇒ **trailer 无法被 delta 直接修改**。
- ⚠️ **但**：**2026-11-21 的 dry-run 实测修正了一条本记录首版的判断** —— 合并器对 trailer 的处理**取决于其键名**：
  - **英文** trailer（`**Maintenance**` / `**Status**`）⇒ **原样保留**（见 spike② 的 `pcie-slice-prerequisites` 合并 diff）
  - **中文** trailer（`**维护**` / `**状态**`）⇒ **被丢弃**（见 dry-run：`pcie-ip-integration` 的 `@@ -46,9 +48,3 @@` 删掉 `-**维护**` + `-**状态**: 📋 Spec — Phase 8 最终整合交付中`；`v1-architecture` 同理删掉 `-**状态**: 📋 V1.0 … （00-overview.md 已交付 + Oracle PASS）`）
  ⇒ **⇒ 任何触到该 capability 的 archive 都会把中文 trailer 整段清掉** ⇒ ⇒ **R-13 的后半（`pcie-ip-integration:53`）会被本 change 的归档自动闭合**；而 **R-13a（`pcie-ip-microarch:296`）不在本 change 的触碰范围内 ⇒ 仍然 stale**。
- ⚠️ 该行为是 openspec 的**格式规范化**（canonical spec = `## Purpose` + `## Requirements`，无 trailer 字段）⇒ ⛔ 不是本 change 的副作用，但 **diff 里必须预期它**（否则会被读成「误删」）。
- 另一处相关事实：`v1-architecture` 的 trailer 现为 `**状态**: 📋 Spec — v1.0 总架构蓝图（00-overview.md 已交付 + Oracle PASS）` ⇒ 与 R-15 的「该文件不存在」**直接矛盾**（同一文件内自相矛盾）⇒ ⇒ 该矛盾**随本 change 的归档自动消失**（trailer 被丢弃）

## 对本单元的路径判定（per AC-0 的 escape clause）

AC-0 原文：「任一为否 ⇒ 本单元**改走「architect 裁 + 主管契约面直接修订」路径，⛔ 不得硬套 delta**」

| 项 | 落点 | delta 可行性 | 建议路径 |
|:--:|---|:--:|---|
| **R-12** | `pcie-slice-prerequisites` Requirement 内 blockquote | ✅ 可行（重述该 Requirement） | **delta** |
| **R-14** | `pcie-ip-integration` Requirement 内（范围/实现位置） | ✅ 可行（重述 `integration-delivery`） | **delta** |
| **R-15** | `v1-architecture` Requirement 正文 + Scenario | ✅ 可行（重述该 Requirement） | **delta**（⚠️ 但 trailer 的「已交付」串留下 ⇒ 见下） |
| **R-20** | `dgpu-board-adapter-info` Requirement 内 Scenario | ✅ 可行（重述 `First-touch Handle 生命周期管理`） | **delta** |
| **R-13** | `pcie-ip-microarch:296` + `pcie-ip-integration:53` 的 **trailer** | ⛔ **不可经 delta 直接改**，但 **`pcie-ip-integration` 的会随本 change 归档自动清除**（dry-run 实测） | **拆开**：`pcie-ip-integration` ⇒ 本 change 顺带闭合；**`pcie-ip-microarch` ⇒ 须 architect 裁**（触到它只能靠「重述其某个 Requirement」⇒ 与「只改状态行」冲突；或「直改 main spec」；或放弃） |

⇒ **5 项中 4 项走 delta、R-13 的后半随归档自动闭合、R-13 的前半（`pcie-ip-microarch`）须 architect 裁**。⚠️ 本结论**不自行决定** R-13a 的路径（直改 main spec 绕过 delta 流程 ⇒ 与 improvement 的 范围纪律冲突 ⇒ 只有 architect 能授权）。

## 失败项与路由建议

| # | 项 | 路由 |
|:--:|---|---|
| F-1 | **R-13（trailer 状态行）delta 不可行** | ⇒ **交 architect 裁**（二选一：(a) 授权「直接修订 main spec 的 trailer」并给出新的状态串形态；(b) 放弃 R-13 并把该项移出本单元范围）⇒ ⛔ builder 不自决 |
| F-2 | R-15 的 trailer 亦含「00-overview.md **已交付** + Oracle PASS」⇒ 与 R-15 的缺口矛盾 | ⇒ 与 F-1 同源（trailer 不可经 delta 改）⇒ 并入 same architect 裁决 |
| F-3 | 影子 spike 的产物（`zz-spike-r12` / `zz-spike-r13`）**只在 `/tmp/opencode/spike/`**，未进仓 | ✅ 已隔离（`openspec/changes/` 的 active 计数**未受影响** ⇒ AC-9 的 active ≤ 0 仍成立）|

## 遗留清扫

| 项 | 结论 |
|---|---|
| 是否改动仓内 `openspec/specs/**` | ✅ **零改动**（spike 全部在 `/tmp/opencode/spike/` 的副本内；`git status` 无 `openspec/specs` 变更）|
| 是否新建仓内 change | ✅ **否**（shadow change 在 /tmp）⇒ KPI 计数未动 |
| 临时目录 | ⏳ `/tmp/opencode/spike/` 保留至本单元收口（复核用）⇒ 收口时删除 |

## 已用轮次

**1/1**（AC-0 是**一次性前置**，无返工轮）。
