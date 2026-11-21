# 验收报告 · `cpptlm-spec-stale-refs-b-class`（改 `openspec/specs` 的 4 项 B 类 stale 文本）

- **角色**：builder（**实施侧自验** —— 本 change 由同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **原始验收 · 2026-11-21**（归档后即时补记；⛔ 本报告**不**声称「原归档未验」）

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单 | `openspec/changes/archive/2026-11-21-cpptlm-spec-stale-refs-b-class/proposal.md` §验收标准（AC-1~AC-10）|
| AC-0 前置结论 | `.rddf/reviews/cep-spec-stale-refs-r14-r15-r20-ac0-spike.md`（trailer 不可经 delta 改；中文 trailer 会被归档丢弃）|
| 设计取舍 | 同 change `design.md` D-1（(a)/(b) 对比 ⇒ 选 (b)）· D-2（R-12 等价物）· D-3（R-20 ABI 依据）|
| 上游 | `.rddf/improvements/cep-spec-stale-refs-r14-r15-r20.md`（登记册 §1.3 B 类 · 用户 2026-11-20 Q-9）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-0 | 影子 change 跑通 `validate` + 试归档，①② 各给书面结论 | **通过** | ✅ 已落 `.rddf/reviews/cep-spec-stale-refs-r14-r15-r20-ac0-spike.md`：① trailer 不可经 delta 改（`ERROR: no requirement entries parsed` + `WARNING: not carried into the main spec`）② Requirement 内行删除**可行**（重述整块；归档 `~ 1 modified`）|
| AC-1 | `openspec validate <name> --strict` rc=0 · proposal 含 `## 验收标准` · design.md 有 R-14/R-15 的 (a)/(b) 对比表 | **通过** | `openspec validate cpptlm-spec-stale-refs-b-class --strict` → `Change … is valid`（rc=0，**归档前**实测）；`proposal.md` 含 `## 验收标准`；`design.md` D-1 为三列对比表（形态 / 收益 / 代价风险）+ 三条裁决理由 |
| AC-2 | R-12：`grep -c "include/cudart/abi_guards.h"` == 0 + design.md 交代等价物 | **通过** | 归档后 `grep -cF "include/cudart/abi_guards.h" openspec/specs/pcie-slice-prerequisites/spec.md` → **0**；design.md D-2 逐条交代（该路径**从未在本仓存在** · 「G-D4 17 条断言」无处取证 · `AGENTS.md CROSS-PROJECT` 段不存在 · 本仓现有 `BUNDLE_FIELD_WIDTH`/`static_assert` 共 58 处 · ⚠️ **`PcieTlpBundle` 本身无布局断言 = 既有缺口**）|
| AC-3 | R-13 状态行旧串归零（⚠️ 本 change 只覆盖 `pcie-ip-integration` 侧，见 F-1） | **部分通过（后半闭合 / 前半待裁）** | `grep -c "Phase 8 最终整合交付中" openspec/specs/pcie-ip-integration/spec.md` → **0** ✅（原因：该文件的 `**维护**/**状态**` 是**中文 trailer** ⇒ 归档合并时**整段被丢弃** —— dry-run + 实跑双向实测）；⚠️ `grep -c "等待评审 + Phase 1 启动实施" openspec/specs/pcie-ip-microarch/spec.md` → **1** ⇒ **未闭合**（本 change 未触该 capability ⇒ 见 F-1）|
| AC-4 | R-14：`grep -c "docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md"` == 0 | **通过** | 归档后 → **0**（3 处全部改为「ArchForge 仓维护 · 本仓不重复交付」）|
| AC-5 | R-15：`00-overview.md` 的仓内路径归零 **且 `SHALL` 自洽** | **通过** | `grep -cF "docs/soc_arch/architecture/00-overview.md" openspec/specs/v1-architecture/spec.md` → **0**；`SHALL` 改为 `SHALL NOT 在本仓重复交付 …（由 ArchForge 仓维护）;本仓 SHALL 保证实现与该蓝图一致` ⇒ **无「SHALL 交付一个不存在的东西」** 的自相矛盾；另：该文件的 trailer 旧串「（00-overview.md 已交付 + Oracle PASS）」随归档**被丢弃**（`grep -c` 实测 0）|
| AC-6 | R-20：`grep -c "cpptlm_emulator_get_adapter_info"` == 0 且新引用在现行 ABI 内 | **通过** | 归档后 → **0**（**两个** Requirement 的 2 处全部改为 `cpptlm_emulator_get_device_info`）；新引用实测在现行 ABI：`include/abi/cpptlm_emulator.h:71` `int cpptlm_emulator_get_device_info(uint32_t dev_id, cpptlm_device_info_t*)`；改写同时写明「按 `dev_id`、无句柄依赖」并锚到 `ADR-SOC-20 §2.1` 的删除理由 |
| AC-7 | 未复活血缘：`git diff` 不含 `include/abi/**` · `include/tlm/**` · `include/bundles/**` · `src/**` | **通过** | 本 change 的改动面 = 4 份 main spec + 1 个 openspec change 目录（已归档）+ 基线文件 ⇒ 上述路径**零 diff**（实测见 §遗留清扫）|
| AC-8 | CEP 零 diff：不含 `openspec/specs/cep-*` · `docs/adr/**` · `docs/architecture/**` | **通过** | 同上（4 份被改 spec 的 capability 名均为非 `cep-` 前缀）|
| AC-9 | KPI + 归档形态：立项时 active ≤ 0；归档用**默认**（⛔ 不用 `--skip-specs`） | **通过** | 立项前 `openspec/changes/` 的 active = **0**；归档命令为 `openspec archive cpptlm-spec-stale-refs-b-class -y`（**无** `--skip-specs`）⇒ 输出 `Totals: + 0, ~ 5, - 0` ⇒ delta 已合并 |
| AC-10 | 0 回归：`openspec validate --specs --strict` 红数**不增加** · `ctest` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · 文档闸全绿 | **通过** | `openspec validate --specs --strict --json` → **138 items / 55 invalid / 47 cep- 项 / cep- invalid = 0** ⇒ 与基线（138 / 55 / 47 / 0）**逐项一致**（⇒ 红数未增加）· `ctest` 与 47 个 `cep_*.sh` 见 §回归记录 · `docs_sync --strict` / `agents_md_lint --all --strict` 全 rc=0 |

## 失败项与路由建议

| # | 失败 / 未闭合项 | 路由 |
|:--:|---|---|
| F-1 | **R-13a 未闭合**：`openspec/specs/pcie-ip-microarch/spec.md:296` 的 `**状态**: 📋 Spec — 等待评审 + Phase 1 启动实施` **仍在** | ⇒ **交 architect 裁**（二选一）：(a) 授权「直接修订 main spec 的 trailer」（直改，绕过 delta —— 与 improvement 的范围纪律冲突 ⇒ 只有 architect 能授权）；(b) 放弃该项（把 R-13a 从 B 类移入「已知不修」）。⚠️ **第三条路已实测排除**：靠 delta「触到该 capability 让它被丢掉」需重述其某个 Requirement ⇒ 与「只改状态行，不动 Requirement 正文」冲突；且 dry-run 显示**无 delta 就无法触到** |
| F-2 | **`PcieTlpBundle` 无布局编译期断言**（R-12 关联的既有缺口）：`include/bundles/pcie_bundles_tlm.hh` 内 `grep -c "static_assert\|BUNDLE_FIELD_WIDTH"` == **0**，而 `pcie-slice-prerequisites` 的 Requirement 写 `MUST enforce … via compile-time static_assert` | ⇒ **另立 backlog**（实现面变更，⛔ 本 change 的 AC-7 禁 `include/bundles/**`）⇒ 建议登记为 improvement（`cep-pcie-tlp-layout-guard`），⛔ 不在本单元顺手加 |
| F-3 | 本 change **无独立 verifier 环节**（同一 builder 会话实施+自验） | ⇒ 已在报告头显式标注；⚠️ 若要求独立验收 ⇒ 需 verifier 角色复核（窗口已用 2/3）|
| F-4 | 归档合并附带的**格式变动**（`## Purpose`/`## Requirements` 后各插 1 空行；中文 trailer 被丢弃；文件末空行被移除） | ⇒ 属 openspec 合并器的**固有行为**（dry-run 已预演、实跑一致）⇒ ⛔ 不视为回归；已在本报告与 spike 记录中登记 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 临时目录 | ✅ `/tmp/opencode/spike/` 与 `/tmp/opencode/dryrun/` 为 **/tmp 副本**（⛔ 不进仓）⇒ 本回合结束前删除 |
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 零 `include/**` · 零 `src/**` · 零 `docs/adr/**` · 零 `docs/architecture/**` · 零 `openspec/specs/cep-*` |
| 新归档目录的闸可见性 | ✅ 已 force-add（7 文件）+ 基线 `30 → 31`（`scripts/test/archive_verify_baseline.txt`）⇒ 闸在新归档落地后**保持绿**（⛔ 不靠「等下次」）|
| 半截状态 | ⚠️ **R-13a 是本单元的唯一未闭合项**（已显式登记 + 待裁）⇒ ⛔ 不假装完成 |
| 载体表 / 登记册 | ✅ `scripts/test/rddf_ac_carrier.tsv` 的 I-11 → `done`；登记册 §6.4 I-11 行回填（含「R-13a 待裁」标注）|

## 已用轮次

**1/3**（第 1 轮；无返工轮 —— 唯一返工点是 **AC-0 spike 首版的 trailer 判断**：首版写「合并器不会自动规范化 trailer」⇒ dry-run 实测**中文 trailer 会被丢弃** ⇒ 已更正 spike 记录，未产生代码返工）。

---

## 补记（2026-11-21 · 台账 Entry #517）—— **F-1 / R-13a 已闭合**

> ⚠️ 追加段：⛔ 不改上文判定。

| 项 | 原状态 | 现状态 |
|---|---|---|
| **F-1**（R-13a 未闭合） | `openspec/specs/pcie-ip-microarch/spec.md:296` 仍写 `**状态**: 📋 Spec — 等待评审 + Phase 1 启动实施`，**待 architect 裁** | ✅ **已闭合**：用户 2026-11-21 **授权直改**（无 delta 路径，理由见 AC-0 spike）⇒ 新值 `✅ 已实施 —— Phase 1-8 全部交付(链路层 / 编码 / PHY 数字控制 / SR-IOV / AXI Stream Adapter / AXI4Mapper / Host Bypass + RC / 整合交付)`（与 `AGENTS.md` §PHASE STATE 同源） |

取证：无闸/测试断言该字面量（`grep -rn "等待评审 + Phase 1 启动实施" scripts/test/ test/` = 0）· `openspec validate pcie-ip-microarch --type spec` 仅剩**存量** WARNING · `cep_doc_ref_check.sh --strict` / `docs_sync_check.sh --strict` rc=0。

⚠️ **同根因残留（未授权）**：另 5 份 PCIe spec 的状态行同样陈旧（`link-layer-and-fc:92` / `130b-encoding:56` / `axi-stream-adapter:72` / `axi4-mapper:51` / `host-bypass-and-rc:49`）—— 待单独授权，⛔ 不随本次扩大。
