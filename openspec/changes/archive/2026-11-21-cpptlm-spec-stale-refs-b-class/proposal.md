# proposal: cpptlm-spec-stale-refs-b-class —— 5 项 B 类 spec 契约面 stale 文本修正

> **来源**：`.rddf/improvements/cep-spec-stale-refs-r14-r15-r20.md`（登记册 §1.3 终态表 B 类 · 用户 2026-11-20 拍板 Q-9「B → openspec change」）
> **AC-0 spike 结论**：`.rddf/reviews/cep-spec-stale-refs-r14-r15-r20-ac0-spike.md` ⇒ **trailer 类（R-13）delta 不可行** ⇒ 本 change 只做 **4 项**（R-12 / R-14 / R-15 / R-20）；R-13 待 architect 裁

## Why

这 4 条 spec **都在说「你要交付 / 调用一个不存在的东西」**：

| # | spec | 失效内容（实测） |
|:--:|---|---|
| **R-12** | `pcie-slice-prerequisites` | 引用 `include/cudart/abi_guards.h` —— **该路径在本仓不存在**（`ls include/cudart/` 无输出）|
| **R-14** | `pcie-ip-integration` | 要求交付 `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` —— 该目录**已迁 ArchForge**（仓内仅占位 README）|
| **R-15** | `v1-architecture` | Requirement 写 `SHALL 交付 docs/soc_arch/architecture/00-overview.md` —— **该文件不存在** |
| **R-20** | `dgpu-board-adapter-info` | 引用**已删除**的 ABI `cpptlm_emulator_get_adapter_info`（`include/abi/cpptlm_emulator.h:108` 的注释明写「已删除 per `ADR-SOC-20 §2.1`」）|

`openspec validate --specs` 对它们**是绿的**（只查格式与长度，不查引用是否可达）⇒ 这是「假绿」在**契约层**的形态。

## What Changes

| # | 落点 | 变更为 |
|:--:|---|---|
| 1 | `pcie-slice-prerequisites`（MODIFIED `pcie-slice-wire-format-in-repo-guarded`）| 删除悬空引用行 `include/cudart/abi_guards.h` + 该行的「17 条 G-D4 静态断言」断言（**等价物交代见 design.md**）|
| 2 | `pcie-ip-integration`（MODIFIED `integration-delivery`）| 三处 `docs/soc_arch/architecture/19-…` 改为「**ArchForge 仓维护 · 本仓不重复交付**」（含 1 处 Scenario 的 WHEN）|
| 3 | `v1-architecture`（MODIFIED `v1-architecture-blueprint`）| `SHALL 交付 00-overview.md` → `SHALL NOT 在本仓重复交付`（文档归 ArchForge）+ 2 处 Scenario 的读取对象改为「ArchForge 仓的 `00-overview.md`」|
| 4 | `dgpu-board-adapter-info`（MODIFIED **两个** Requirement）| 2 处 `cpptlm_emulator_get_adapter_info` 改为 `cpptlm_emulator_get_device_info`（按 `dev_id`，**无句柄依赖** —— 与 `ADR-SOC-20 §2.1` 的删除理由一致：与 `get_device_info` 重叠）|

## Out of Scope

- ⛔ **R-13**（`pcie-ip-microarch:296` + `pcie-ip-integration:53` 的**文档 trailer 状态行**）—— AC-0 spike 证明 **delta 无法修改 trailer**（`openspec validate` ERROR「no requirement entries parsed」+ WARNING「not carried into the main spec」）⇒ 待 architect 裁（直接修订 main spec 的 trailer vs 放弃该项）
- ⛔ R-17 / R-19（A 类归 `cep-doc-hygiene-kpi-and-impl-state`）· ⛔ R-18（C 类归 architect Q-12）
- ⛔ 任何 CEP spec · 任何 ADR 正文 · 已删 ABI 的复活动作 · `pcie-ip-microarch` 的任何改动（其失效内容只在 trailer）

## 验收标准

- [ ] **AC-1**：`openspec validate cpptlm-spec-stale-refs-b-class --strict` rc=0；本 proposal 含 `## 验收标准` 段；`design.md` 给出 R-14 / R-15 的 (a) 跨仓指针 vs (b) 本仓不承诺 对比表 + 取舍理由
- [ ] **AC-2（R-12）**：归档合并后 `grep -c "include/cudart/abi_guards.h" openspec/specs/pcie-slice-prerequisites/spec.md` == **0**；design.md 说明「该守卫面现由何处承担」
- [ ] **AC-4（R-14）**：归档合并后 `grep -c "docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md" openspec/specs/pcie-ip-integration/spec.md` == **0**
- [ ] **AC-5（R-15）**：归档合并后 `grep -c "docs/soc_arch/architecture/00-overview.md" openspec/specs/v1-architecture/spec.md` == **0**，且该 Requirement 的 `SHALL` 与「本仓不交付」**语义自洽**（⛔ 不得留「SHALL 交付一个不存在且不承诺交付的文件」）
- [ ] **AC-6（R-20）**：归档合并后 `grep -c "cpptlm_emulator_get_adapter_info" openspec/specs/dgpu-board-adapter-info/spec.md` == **0**；且新引用 `cpptlm_emulator_get_device_info` **在现行 ABI 内**（`include/abi/cpptlm_emulator.h:71`）
- [ ] **AC-7（未复活血缘）**：`git diff --name-only` **不含** `include/abi/**` · `include/tlm/**` · `include/bundles/**` · `src/**`
- [ ] **AC-8（CEP 零 diff）**：`git diff --name-only` **不含** `openspec/specs/cep-*` · `docs/adr/**` · `docs/architecture/**`
- [ ] **AC-9（KPI + 归档形态）**：立项时 `openspec/changes/` active **≤ 0**（本项目立项后 = 1）；归档用**默认**（⛔ 不用 `--skip-specs`）
- [ ] **AC-10（0 回归）**：`openspec validate --specs --strict` 的 `cep-` invalid **仍为 0** 且**红数不增加**（以实测为准，⛔ 不写死）· `ctest` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` 全 rc=0
