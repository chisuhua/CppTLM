# tasks: cpptlm-cep-invariant-modeling-batch2

> **前置**：architect 裁决 Q-4 已落（`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` §二）· 用户 ratify R-2a~R-2f · litmus change 已归档（= I-6a）· Q-2 已裁 · DoD 7 措辞已订正
> **⛔ 顺序不可颠倒**（裁决 §2.6 D-5）：台账 Entry（授权）→ 改产物 → commit → `--update` → **清单单独 commit** → 无参复验

## T0 · 步骤 0（**机械件** · 缺则「已落地」是假宣称）

- [x] T0.1 新建载体 `test/test_cep_combination_contract.cc`（**两个** `INV-CARRIER` 标记 + 各自邻域断言；design.md D-3）
- [x] T0.2 新建 `test/cep_combination_contract_expected.json`（`schema: cep-combination-contract-expected/v1` · 逐实例期望值 · `unknown_count` 精确计数 · 域**双向**集合）
- [x] T0.3 从 `test/cep_invariant_deferrals.json` **删除这 2 条**登记（⇒ `deferrals == []`；⛔ 文件不得删除）
- [x] T0.4 `python3 scripts/test/cep_invariant_map_check.py --generate` 重生成 `test/cep_invariant_assertion_map.json`
- [x] T0.5 ⚠️ `--generate` **不**跑邻域校验 ⇒ 立即 `--check`（并集闸硬约束：`载体 ∪ ⭭ == 66` ∧ 交集为空）

## T1 · schema 面 + ADR 追加 SU

- [x] T1.1 `include/tlm/cep/protocol_instance.schema.json`：`properties` 13→14（新增 `combination_contract`，**逐字**取裁决 §2.2 形态）；⛔ `required` **仍 13**
- [x] T1.2 `docs/adr/ADR-CEP-12-*.md` 追加 `## Status Update`（裁决 §2.7.1 规范文本；⛔ 正文零改动；⚠️ **文末**；⚠️ 全文不得含「6 个字段组」串）
- [x] T1.3 `docs/adr/ADR-CEP-13-*.md` 追加 `## Status Update`（裁决 §2.7.2 规范文本；⛔ 正文零改动；⚠️ **文末**）

## T2 · 派生断言改形（4 断言点 / 2 用例标题）

- [x] T2.1 `test/test_cep_schema.cc:234` 用例标题 + `:252` / `:257` **拆成两个列表**
- [x] T2.2 `test/test_cep_schema.cc:418` 用例标题 + `:423` ⇒ 14
- [x] T2.3 `test/test_cep_schema.cc:447` **改写为三条**
- [x] T2.4 `:401` · `:454` · `:455` **零改动 / 保持绿**（追加 SU 落盘后须**实跑复验** `:455`）

## T3 · 实例声明项 + 条件判据 + 台账闸

- [x] T3.1 `include/tlm/cep/protocol_instances.json`：本仓权威域 **6 个**实例逐个加 `combination_contract.bridging.early_ack`（**全 6 个 = `unknown`** · design.md D-2）；⛔ **镜像域 `moesi_gpu` / `mesif` 零改动**
- [x] T3.2 条件判据（`adopted == true ⇒ edge_classes 非空 ∧ verification_scope 非空`）落载体（**合成实例**三例 · design.md D-4）
- [x] T3.3 新建 `scripts/test/cep_invariant_batch2_ledger_check.py`（照批 1 范式 · design.md D-5）+ `test/CMakeLists.txt` 注册
- [x] T3.4 台账 Entry **#514**（**以落库时实测最大值 + 1** 为准 · ⛔ 不预留编号）落 `.rddf/roadmap/objectives/obj-cep-platform.md`

## T4 · litmus 侧（语料保持为空）

- [x] T4.1 `test/cep_litmus_corpus.json` 的 `_fill_when` 改为「**待操作级模型落地**」+「通道图 ≠ 操作图」（⛔ `compound_edges` 仍 `[]`）
- [x] T4.2 `include/tlm/cep/litmus_falsifier.hh` 头注释补「通道图 ≠ 操作图」
- [x] T4.3 **golden 零改动**（`test/cep_instance_golden.json` 含其 `_note` / `_scope_rationale` 注释）

## T5 · 归档与收尾

- [x] T5.1 `openspec archive cpptlm-cep-invariant-modeling-batch2 -y`（**默认**，⛔ 不用 `--skip-specs`）⇒ delta 合并进 `openspec/specs/cep-core-contracts/spec.md`
- [x] T5.2 验收报告 `.rddf/reviews/cpptlm-cep-invariant-modeling-batch2-verify.md`
- [x] T5.3 载体表 `scripts/test/rddf_ac_carrier.tsv`：I-6 → `done`；机关 `--phase queue2` 复验
- [x] T5.4 登记册回填：§6.4 队列 2 **I-6** 行 + §1.3 R-17/R-19 相关行（若涉及）+ §6.2 S1-1 行

## T6 · 五步链（**冻结面**）

- [x] T6.1 `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #514"`（**第 1 次**）
- [x] T6.2 **commit A**（台账 + 全部改动 + 清单）
- [x] T6.3 `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #514"`（**第 2 次** ⇒ `frozen_at_commit` = commit A）
- [x] T6.4 **commit B**（**清单单独 commit** · ⛔ 台账与清单不得合并提交）
- [x] T6.5 无参复验 `bash scripts/test/cep_freeze_check.sh` rc=0 + `cep_freeze_reverse_verify` rc=0

## T7 · 0 回归

- [x] T7.1 ctest 全绿（⛔ 不写死数字）
- [x] T7.2 全部 `scripts/test/cep_*.sh` 逐个 rc=0 **且输出不含 `[SKIP]`**（⚠️ 须先确认 `$BUILD_DIR/bin/cpptlm_tests` 存在）
- [x] T7.3 `cep_adr_structural_check.py` · `rddf_stage_gate` · `docs_sync --strict` · `agents_md_lint` 全 rc=0（⚠️ `format --check` 的 **21 个既有 C++ 债** ⇒ 本单元零 C++ 改动 ⇒ 排除既有债后判 / 如实记录）

## ⛔ 本 change 不做

- **litmus change 本身**（= I-6a，已归档）· `INV-CEP-12-IC1` 的语义改动 · **边类语义定义**（`rfe`/`fr`/`ws` ⇒ backlog `cep-memory-model-operational-trace`）· **镜像域实例 + golden** · **新增实例** · `sharer_tracker.hh` / `protocol_engine_base.hh` / `docs/architecture/**`
