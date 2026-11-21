# tasks: cpptlm-doorbell-contract-alignment

> **前置**：architect 裁决 **Q-12** 已落（`.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`）· openspec 可行性已验证（3 处主断言 + 2 处连带断言**均在 Requirement 块内** ⇒ 可经 delta 改写；per I-11 AC-0 spike「delta 必须至少 1 个 `### Requirement:` 块」）· 立项时 `openspec/changes/` active == **0**。

## T1 · delta 落盘（3 份 · C-1 ~ C-5）

- [x] T1.1 `specs/minimal-dgpu-soc-architecture/spec.md` —— MODIFIED **两个** Requirement（「BAR 布局与代码真相对齐」= C-1 + C-2；「doorbell 合成偏移是已裁定的现状」= C-3）
- [x] T1.2 `specs/dgpu-soc-timing/spec.md` —— MODIFIED 「BAR 路由优先级」（C-4：正文追加偏移识别口径，⛔ 3 个 Scenario 行不动）
- [x] T1.3 `specs/lwc-bus-hub-mvp/spec.md` —— MODIFIED 「sdma-doorbell-in-handling」（C-5：同 C-4 形态）
- [x] T1.4 **C-6 核对**：`dgpu-soc-control-hub` 的 `0x10010000\|kBar1DoorbellOffset` 命中 == **0** ⇒ **不含，无需改** ⇒ ⛔ 显式记录于 proposal 的 What Changes 表 + design.md D-3 + 验收报告

## T2 · 骨架与校验

- [x] T2.1 `proposal.md`（含 `## 验收标准`）· `design.md`（D-1~D-7）· `tasks.md`
- [x] T2.2 `openspec validate cpptlm-doorbell-contract-alignment --strict` rc=0

## T3 · 决定性变异（裁决 §3.2 · M-1 / M-2 / M-3）

- [x] T3.0 **前置**：先证明 doorbell 相关测试**当前绿**（M-1 无此项则变异无意义）
- [x] T3.1 **M-1**：影子副本临时删除 doorbell 优先匹配分支 ⇒ **doorbell 路由断言**变红（⛔ 编译失败不算红因）⇒ 还原 ⇒ 绿
- [x] T3.2 **M-2**：三份 shipped 配置**维持 16MB**（`git diff --name-only` 不含 `configs/**` ∧ `examples/**`）⇒ 新条款仍被满足
- [x] T3.3 **M-3**：影子副本删掉 C-3 的新措辞 ⇒ AC-7 的 `grep -cF "pcie_ep.params.bar_sizes[1] >= 268435464"` 从 1 变 **0** ⇒ 还原 ⇒ 1

## T4 · 归档前取证

- [x] T4.1 影子预演：`/tmp` 副本 `openspec archive -y` ⇒ 取合并后 diff，确认只落目标段
- [x] T4.2 AC-1 / AC-2 / AC-7 的 6 项 `grep` 判据（main spec 侧）
- [x] T4.3 AC-8 范围纪律：`git diff --name-only` 不含 `include/**` · `src/**` · `configs/**` · `examples/**` · `docs/designs/dgpu-driver/architecture.md` · `scripts/test/param_effect_inventory.py`

## T5 · 归档与收尾

- [x] T5.1 `openspec archive cpptlm-doorbell-contract-alignment -y`（**默认**，⛔ 不用 `--skip-specs`）
- [x] T5.2 **C-7**：向 `docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md` **追加** `## Status Update` 段（⛔ 正文零改动 · ⛔ 既有行 diff 行数 == 0 · 新增行含「Q-12」）
- [x] T5.3 归档后复跑 T4.2 的 6 项 `grep`（main spec 侧）+ `openspec validate --specs --strict` 红数不增加
- [x] T5.4 验收报告 `.rddf/reviews/cpptlm-doorbell-contract-alignment-verify.md`
- [x] T5.5 载体表 `scripts/test/rddf_ac_carrier.tsv`：I-12 → `done`；机关 `--phase queue2` 复验
- [x] T5.6 登记册回填：§6.4 队列 2 **I-12** 行 + §6.4 队列 1 **Q-12** 行（补「✅ 已裁」+ **修正 `:49` 错锚**）+ §10.4 指针

## T6 · 0 回归

- [x] T6.1 `ctest` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `rddf_stage_gate` 全 rc=0
- [x] T6.2 ⚠️ `format --check` 的 **21 个既有 C++ 债** ⇒ 本单元零 C++ 改动 ⇒ 须**排除既有债**后判，或如实记录为既有红

## ⛔ 本 change 不做

- **代码取值来源 / 配置 / driver ABI 表 / `docs/designs/dgpu-driver/architecture.md`**（N-1 ~ N-6）
- **`docs/designs/dgpu-soc/d3-evolution-roadmap.md`**（🟡 DRAFT，只引用不修改）
- **`lwc-bus-hub-mvp` NOTE 的历史行号锚点**（`dgpu_board_shell.cc:537`）—— ⛔ 记录不改（design.md D-3）
