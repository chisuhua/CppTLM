# tasks — cpptlm-agents-md-slimming

> doc-only change。每项完成时勾选并附验证命令的结果。

## T1 外移 STRUCTURE 逐文件清单

- [x] 1.1 新建 `docs/REPO-MAP.md`，把 `AGENTS.md` §STRUCTURE 的原树**逐条原样**搬入（含 ★ 标注与行内注释）
- [x] 1.2 顶部写明分工：「本文件 = 逐文件清单（详版）；`AGENTS.md §STRUCTURE` = 主线导航（简版）」
- [x] 1.3 验证 AC-2：原树中被 lint 校验过的路径集合 ⊆ `docs/REPO-MAP.md` 的路径集合

## T2 瘦身 AGENTS.md §STRUCTURE

- [x] 2.1 重写为 ≤ 50 行：★ 主线（`include/tlm/{pcie,cep,cluster,gpu}` / `src/tlm/...` / `test/` / `docs/{adr,architecture,designs}` / `openspec/` / `scripts/`）
- [x] 2.2 保留 `## STRUCTURE` 后紧跟代码围栏的形态（`agents_md_lint.py` 的解析前提）
- [x] 2.3 节末加一行指针 → `docs/REPO-MAP.md`

## T3 外移 PHASE STATE 交付流水

- [x] 3.1 新建 `docs/archive/process/PHASE-LOG.md`：搬入 PCIe Phase 1-8 流水（commit 区间 / Oracle 结论）、D-AXI 版本演进、已归档 dGPU SoC 提案、GPGPU 既有交付明细、已知问题
- [x] 3.2 验证 AC-3：原 `PHASE STATE` 的 42 行表格行逐条出现

## T4 重写 AGENTS.md §PHASE STATE

- [x] 4.1 压到 ≤ 18 行：一张**终态表**（CEP / PCIe EP / D-AXI / GPGPU / 框架）+「当前主线」一行
- [x] 4.2 节末指针 → `docs/archive/process/PHASE-LOG.md`

## T5 精简 WHERE TO LOOK

- [x] 5.1 压到 ≤ 24 行：只留高频「任务 → 入口」
- [x] 5.2 完整索引改为指针：`docs/README.md §3`（目录索引）/ `docs/adr/README.md`（63 份 ADR）/ `docs/designs/README.md`

## T6 MCP 段 + 计数 + 登记

- [x] 6.1 `MCP TOOLS` 段改为指针 + 条件使用（⛔ 不出现废止工具名、⛔ 不无条件「优先于 Grep」）
- [x] 6.2 硬编码计数改为「实测值 + 取值命令」（修正 `370` → 实测值）
- [x] 6.3 `docs/README.md §3.6` 登记 `docs/REPO-MAP.md`

## T7 验收

- [x] 7.1 `AGENTS.md` ≤ 45,000 B
- [x] 7.2 `python3 scripts/test/agents_md_lint.py --strict` rc=0
- [x] 7.3 `bash scripts/test/docs_sync_check.sh --strict` rc=0
- [x] 7.4 `bash scripts/test/docs_index_consistency_check.sh` rc=0
- [x] 7.5 `ctest --test-dir build -j4` 全绿
- [x] 7.6 `bash scripts/test/cep_freeze_check.sh` rc=0（未触碰冻结面）

## 验收记录（2026-11-19）

| 判据 | 结果 |
|---|---|
| AC-1 体量 | **41,333 B** ≤ 45,000（原 64,448 B ⇒ **−23,115 B / −35.9%**）；`agents_md_lint --strict` rc=0 |
| AC-2 信息不丢（STRUCTURE） | 原树 201 个 token → `docs/REPO-MAP.md` **命中 201 / 缺失 0** |
| AC-3 信息不丢（PHASE STATE） | 原表格 **42 行全部**在 `docs/archive/process/PHASE-LOG.md` |
| AC-4 计数 | `370` 已随树移除；`WHAT THIS IS` 的 `46 份 main spec` / `45 个契约闸` → **改为取值命令** |
| AC-5 MCP | 废止工具名 **零残留**；⚠️「何时用」为条件句（不含无条件「优先于 Grep/Log/Read」） |
| AC-6 零回归 | 13 个文档闸 rc=0；`ctest` 全绿；`cep_freeze_check` rc=0（未触碰冻结面） |
| AC-7 可达性 | `docs_sync_check --strict` rc=0（新增两文件被覆盖且路径全部存在） |

**实施期发现**：新建本 change 会让 `openspec list` 的 active 从 0→1 ⇒ `cep_docs_sync_check` 的 KPI baseline 断言与 `rddf_stage_gate` 的 P-5 **同时转红**。这是**预期行为**（活口径必须同步），处置 = 完成后立即归档（`--skip-specs`，doc-only）恢复 0/3。
