# tasks: cpptlm-spec-stale-refs-b-class

> **前置**：AC-0 spike 已完成 ⇒ `.rddf/reviews/cep-spec-stale-refs-r14-r15-r20-ac0-spike.md`（结论：trailer 类不可经 delta ⇒ R-13 不并入本 change）

## T1 · delta 落盘（4 项）

- [x] T1.1 `specs/pcie-slice-prerequisites/spec.md` —— MODIFIED `pcie-slice-wire-format-in-repo-guarded`（删 `include/cudart/abi_guards.h` 行）
- [x] T1.2 `specs/pcie-ip-integration/spec.md` —— MODIFIED `integration-delivery`（3 处 `docs/soc_arch/architecture/19-…` → ArchForge 归属）
- [x] T1.3 `specs/v1-architecture/spec.md` —— MODIFIED `v1-architecture-blueprint`（`SHALL 交付` → `SHALL NOT 在本仓重复交付` + 2 处 Scenario）
- [x] T1.4 `specs/dgpu-board-adapter-info/spec.md` —— MODIFIED **两个** Requirement（`get_adapter_info` → `get_device_info`，含「无句柄依赖」说明）

## T2 · 骨架与校验

- [x] T2.1 `proposal.md`（含 `## 验收标准`）· `design.md`（D-1 的 (a)/(b) 对比表 + D-2/D-3 依据）· `tasks.md`
- [ ] T2.2 `openspec validate cpptlm-spec-stale-refs-b-class --strict` rc=0

## T3 · 归档前取证

- [ ] T3.1 影子预演：在 `/tmp` 副本上 `openspec archive -y` ⇒ 取合并后 diff，确认只落目标段
- [ ] T3.2 四项 `grep -c` == 0（R-12 / R-14 / R-15 / R-20 的旧引用）
- [ ] T3.3 `git diff --name-only` 不含 `include/**` · `src/**` · `openspec/specs/cep-*` · `docs/adr/**` · `docs/architecture/**`

## T4 · 归档与收尾

- [ ] T4.1 `openspec archive cpptlm-spec-stale-refs-b-class -y`（**默认**，⛔ 不用 `--skip-specs`）
- [ ] T4.2 归档后复跑 T3.2 的四项 `grep -c`（main spec 侧）+ `openspec validate --specs --strict` 红数不增加
- [ ] T4.3 验收报告 `.rddf/reviews/cpptlm-spec-stale-refs-b-class-verify.md`（存档验收闸会查）
- [ ] T4.4 载体表 `scripts/test/rddf_ac_carrier.tsv`：I-11 → `done`；机关 `--phase queue2` 复验
- [ ] T4.5 登记册 §6.4 I-11 行回填「已落地 2026-11-21」（⚠️ **R-13 未做** ⇒ 须显式标注「R-13 待 architect 裁」）

## T5 · 0 回归

- [ ] T5.1 `ctest` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` 全 rc=0 · 工作区干净

## ⛔ 本 change 不做（须单独处置）

- **R-13**（`pcie-ip-microarch:296` + `pcie-ip-integration:53` 的 trailer 状态行）—— delta 不可行（AC-0 spike）⇒ **待 architect 裁**
- `PcieTlpBundle` 的布局编译期断言缺口（design.md D-2 实测：该 bundle 无 `static_assert` / 宽度锁）⇒ **另立 backlog**
