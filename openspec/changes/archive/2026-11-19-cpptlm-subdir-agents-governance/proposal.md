# cpptlm-subdir-agents-governance

> **状态**: 📋 Proposed（2026-11-19）
> **类型**: 文档 + 测试工具（`scripts/test/`）· **无代码/构建/冻结面改动**
> **KPI**: ⚠️ **不计入 Proposed 上限（3）** —— `skip_specs: true`，无能力变更（per `cpptlm-dgpu-soc-arch-truth-cleanup` / `cpptlm-agents-md-slimming` 先例）
> **delta spec**: 无

## Why

15 份子目录 `AGENTS.md`（合计 66,066 B）此前**零闸覆盖**，实测出 3 类问题：

| # | 问题 | 实测证据 |
|:--:|---|---|
| **W-1** | **闸从来没牙** | `docs_sync_check.sh:372` 调 `agents_md_lint.py` **没传 `--strict`** ⇒ lint 有问题仍返回 0 ⇒ 根 AGENTS.md 一路漂到 **98.3% 体量**也没人拦 |
| **W-2** | **子文件与根文件直接矛盾 + 失效引用 6 处** | `include/core/AGENTS.md` 类树仍画 `CPUSim/CacheSim/MemorySim (include/modules/legacy/)`，而 `include/modules/` **整个目录不存在**、根文件 ANTI-PATTERNS 明写已删除；`core/ext/cmd_exts.hh` 已于 2026-06-10 删除；`configs/AGENTS.md` 三份拓扑配置实际已改名 `*_tlm.json`；`configs` / `cpptlm` / `cpptlm_config` 把 **2026-06-17 已归档**的 change 当活指针 |
| **W-3** | **主线目录零指引** | `include/tlm/{cep(45 .hh), pcie(26), gpu(28+21)}` 三个主线目录**没有** AGENTS.md，而 `include/bundles`（8 文件）反而有 |

附带发现：**`cep_freeze_check` 的授权 id 提取过宽** —— `grep "^### "` 会把台账 Entry 正文里的 `###` 级子标题（「授权明细」「验收」…）当作合法 id ⇒ 实测 **682 条 id 中 177 条是伪造入口**，可用子标题冒充冻结面授权。

## What Changes

| # | 动作 | 落点 |
|:--:|---|---|
| **G1** | `agents_md_lint.py` 增 `--all`：18 份子目录 AGENTS.md 的 **体量（≤10 KiB）/ 单行（≤1000）/ 未标注失效引用 / 根文件登记清单对账**；增 `--root`（拷贝根）与 `--selftest`（6 变异） | `scripts/test/agents_md_lint.py` |
| **G2** | **修调用点**：`docs_sync_check.sh` 传 `--all --strict`（W-1 的根因） | `scripts/test/docs_sync_check.sh` |
| **G3** | **修 6 处失效/矛盾**（`include/core` 类树与 `core/ext` 段、`configs` 拓扑表、「已归档 change」引用 ×3、`src/core` 的「暂不编译」、`test` 的「文档待清理」） | 6 份子 AGENTS.md |
| **G4** | **补 3 份主线 AGENTS.md**（CEP / PCIe / GPU），按 6 段模板，只写本目录特有铁律 | `include/tlm/{cep,pcie,gpu}/AGENTS.md`（新） |
| **G5** | 根文件：登记清单 15→18 + **分工纪律** 5 条 + `external/PTX-EMU` 不属本仓治理声明 | `AGENTS.md` |
| **G6** | **收紧冻结面授权校验**：`grep "^### "` → `grep "^### Entry #"` | `scripts/test/cep_freeze_check.sh` |
| **G7** | ctest 注册 `agents_md_lint_all` / `agents_md_lint_selftest` | `test/CMakeLists.txt` |

## Acceptance

| # | 判据 | 手段 |
|:--:|---|---|
| AC-1 | `agents_md_lint.py --all --strict` rc=0（18 份全过） | 脚本 |
| AC-2 | **判据有牙**：`--selftest` 6 变异 + 基线共 **7/7** 符合预期（含「标注后放行」这条**反向**变异） | 脚本 |
| AC-3 | `docs_sync_check.sh --strict` rc=0，且**确实传入** `--all --strict` | grep + 执行 |
| AC-4 | 6 处失效/矛盾闭合（复跑探针：未标注失效引用 = 0） | 探针脚本 |
| AC-5 | 3 份新文件存在、≤10 KiB、已登记 | 闸 |
| AC-6 | **授权校验不能被子标题冒充**：`--update --authorized-by "<子标题>"` 必拒且**不写盘** | 实测 + `git status` |
| AC-7 | 合法授权不受影响：`cep_freeze_check.sh`（无参）rc=0，89 件 hash 一致 | 脚本 |
| AC-8 | **零回归**：全部文档闸 + `ctest` 全绿 | 闸套件 |

## 明确排除

| 项 | 理由 |
|---|---|
| 给 `examples/` / `src/tlm/*` 也补 AGENTS.md | 信息收益率低（跟着头文件走）；补了就要维护，且无独立判据 |
| 治理 `external/PTX-EMU/` 的 15 份 | submodule 自有，不属本仓 |
| 拆分 `DOC HYGIENE` / 合并 `COMMANDS` | 上一轮已明确留后续 |
| 放宽任何体量阈值 | 放宽阈值 = 判据退化 |
