# tasks: cpptlm-cep-gate-robustness

> **AC 映射**: proposal AC-01 ~ AC-15 · design D-1 ~ D-7
> **T0 基线（AC-12 对比用，⛔ 不写死进 AC）**：`ctest -R cep` = **100** · ctest 总 = **180** · `[cep]` = **529 用例 / 12288 断言** · 闸 = **45** · 冻结产物 = **86**

---

## Phase 0 · 立项态（planner 已完成）

- [x] **T0.1** `openspec validate cpptlm-cep-gate-robustness --strict` rc=0
- [x] **T0.2** improvement v2（metis 7 MUST + 5 SHOULD + 5 NIT 全处置）+ proposal + **1 份 delta**
- [x] **T0.3** 台账 **Entry #436**（含 R-5 关闭 / R-7 新发现 / R-9）· KPI **0/3 → 1/3**
- [x] **T0.4** `design.md` + `tasks.md`
- [x] **T0.5** T0 基线记录（见上）

## Phase 1 · W1 影子路径 mktemp 化（11 个闸）

- [x] **T1.1** 5 个 helper 用户：`cep_adr_structural_check` / `cep_b3b2_archive_form_check` / `cep_decision_block_check` / `cep_dod_reading_check` / `cep_iag_gate_reconcile_check`
- [x] **T1.2** 4 个自建：`cep_b3b2_d4_probe_check` / `cep_b3b2_d4_quarterly_review` / `cep_source_effect_check` / `cep_tilelink_engine_failclosed`（后两者变量名是 `SHADOW_ROOT` ⇒ 补 `SHADOW_ROOT="$SHADOW"`）
- [x] **T1.3** `cep_dod_reading_check.sh:31`：`LOG` 默认 → `"$SHADOW/last.log"`（`SHADOW_LOG` 覆盖保留）
- [x] **T1.4** `cep_iag_gate_reconcile_check.sh:142` 与 `cep_freeze_reverse_verify.sh:30`：`mktemp -t` → `mktemp -d "${TMPDIR:-/tmp/opencode}/..."` + 清理
- [x] **T1.5** 每闸加 `echo "[shadow] $SHADOW"`

## Phase 2 · W2 覆盖义务判据

- [x] **T2.1** `cep_decision_block_check.py`：新增正向覆盖判据（签名 `决策块覆盖缺失`），谓词复用 `section2(strip_fences(...))`
- [x] **T2.2** 同文件 docstring「职责边界」段改写（原文说「不在本段职责内」）
- [x] **T2.3** `--list` 输出补该判据
- [x] **T2.4** `cep_decision_block_check.sh`：fixture **A**（`ADR-CEP-30` 开火）+ **B**（`ADR-CEP-31` 不误报），命名理由写进注释
- [x] **T2.5** 真实仓复验：打印两集合并 `diff` 为空；`check.py` rc=0

## Phase 3 · W3 指代订正（8 处）

- [x] **T3.1** 快照键数 6 处 → 23（`.py:11/:237/:504`、`.sh:10`、`README:73`、`CMakeLists:286`）
- [x] **T3.2** `cep_freeze_check.sh:10` → 24（磁盘文件）
- [x] **T3.3** `cep_adr_structural_check.py:8` → 21 / 24（`decision_adrs` 分母）
- [x] **T3.4** `cep_docs_sync_check.sh:4` → 以数组实际条数（W4 后 23）
- [x] **T3.5** 复验：`grep -rn '22 份' --include=*.py --include=*.sh --include=*.md --include=*.txt scripts/ test/ CMakeLists.txt` == 0

## Phase 4 · W4 `ADR_LIST` 补齐

- [x] **T4.1** `cep_docs_sync_check.sh:50` 补 `-20`/`-21` ⇒ **24** 条（⚠️ **判据订正**：初稿写「补 `-19`/`-20`/`-21` ⇒ 23」，实施实测原为 **22** 条且 `-19` 已在 Entry #74 登记 ⇒ 只需补 2 条；`-19` 已登记是初稿误读数组所致，见 improvement W4 订正段）
- [x] **T4.2** `docs_sync --strict` rc=0（三份都含「决策」节 ⇒ 应绿）

## Phase 5 · 0 回归 + 自测

- [x] **T5.1** 逐闸 `bash -n` + 实跑（45 闸）
- [x] **T5.2** 同闸**并行 2 实例**：rc 都 0 且两行 `[shadow]` 路径不同
- [x] **T5.3** `SHADOW_ROOT` 显式覆盖：路径被使用且**跑完仍存在**
- [x] **T5.4** 默认路径跑完无残留（trap 生效）
- [x] **T5.5** `ctest -R cep` == **T0 100** 全绿；`[cep]` == **T0 529**；45 闸全 rc=0

## Phase 6 · critic（arch + build 档，1 轮）

- [x] **T6.1** 送审：1 份 delta（语义）+ 11 闸改动（可靠性）+ 判据新增（有牙？）—— critic `APPROVE`（`arch, build` 双档；0 BLOCKER）
- [x] **T6.2** 无 BLOCKER ⇒ 不需复审（critic `APPROVE` / mergeable: yes / 0 SHOULD_FIX / 6 NIT 全处置：3 修 + 3 记录采纳）

## Phase 7 · 归档（五步链 · **1 份**冻结产物）

- [x] **T7.1** 影子根**预演** `openspec archive`（`~1 modified`；Scenario 集合相等；diff 只落目标段）
- [x] **T7.2** 真仓 `openspec archive --yes` + 复验 `cep-invariant-gate --strict` rc=0
- [x] **T7.3** 产物 commit
- [x] **T7.4** `cep_freeze_check.sh --update --authorized-by "Entry #436"` ⇒ 清单 diff = **1 份** hash + 元数据；`artifact_count` 86 不变　〔**补勾 2026-11-15**：verifier Entry #440 实测确认 —— 清单 commit `28e795a7` diff = 1 file/3±3 · `artifact_count` 86→86 · 无参复验 rc=0〕
- [x] **T7.5** 清单单独 commit + 无参复验 rc=0 + `frozen_at_commit` == 产物 commit　〔**补勾 2026-11-15**：verifier Entry #440 实测 —— `frozen_at_commit` == `git rev-parse 08a67003` ⇒ 相等〕

## Phase 8 · 收口

- [x] **T8.1** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 体量数字（归档 147 → **148**）　〔**补勾 2026-11-15**：verifier Entry #440 实测 —— KPI 已回 0/3（`docs_sync --strict` 判「实测 0/3 = 所有活文档的当前态表述」· 闸全绿）〕
- [x] **T8.2** 台账闭合回执 Entry　〔**补勾 2026-11-15**：verifier Entry #440 实测 —— 台账 Entry **#437** 在位〕
- [x] **T8.3** verifier 独立验收　〔**补勾 2026-11-15**：verifier 第 1 轮 **PASS（15/15 · 0 未取证）** · 报告 `.rddf/reviews/cpptlm-cep-gate-robustness-verify.md` · 台账 Entry **#440**〕
