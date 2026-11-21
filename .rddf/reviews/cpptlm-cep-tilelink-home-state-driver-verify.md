# 验收报告 — `cpptlm-cep-tilelink-home-state-driver`（登记册队列 2 的 **I-7b**）

> **角色**：builder（验收 = 角色内建环节 per `prompts/build.md` §审查配对 的 builder 行）· **已用轮次**：1/3
> **基线（base）**：`bc66bce0`（I-7b 裁决 commit）→ 实施 commit A `ff1dd8bd` → 冻结清单 commit `e97ca964`
> **判定**：**PASS**（AC-1 ~ AC-20 逐条见下；0 项失败 · 0 项残留半截）

## 判据来源

1. `openspec/changes/archive/2026-11-21-cpptlm-cep-tilelink-home-state-driver/proposal.md` 的 `## 验收标准`（AC-1 ~ AC-20）
2. `.rddf/improvements/cep-tilelink-home-side-state-driver.md`（单元 AC-1 ~ AC-14）
3. 实施形态裁决 `.rddf/reviews/cep-i7b-adjudication-2026-11-21.md`（Q-1 逐行 basis 纪律 C-1 ~ C-5 · Q-2 契约面主体 D-1 ~ D-3）
4. `openspec/specs/cep-evidence-tier2-observation/spec.md`（归档后的 main spec · 本单元新增 1 个 Scenario）

## 逐条判定 + 证据

| AC | 判定 | 证据（命令 / 位置） |
|---|---|---|
| **AC-1** C-1 行宇宙锁 | PASS | `test/test_cep_tilelink_home_driver.cc:215` `CHECK(row_universe_ok(kTlHomeInstallRows))` + 3 条反 fixture（删 / 加 / 换序）；**真变异 M-1**（换序）⇒ `CHECK(row_universe_ok(...))` **false** |
| **AC-2** C-2 basis ↔ 锚点 | PASS | `:245` 逐行 `REQUIRE(tl_home_basis_ref_kind_consistent(row))` + 未注册 id / 未改动 anchor 反 fixture（`:252` 用**非 `TLC-` 前缀**的 `UNREGISTERED-ID` —— 前缀 resolver 会把 `TLC-*` 一律判为 SpecClause） |
| **AC-3** C-3 `Pending` ⇒ fail-closed | PASS | `:285` `Pending ⇒ action == NoOp` + 反 fixture（带装表动作的 Pending 行 ⇒ 谓词红） |
| **AC-4** C-4 决定性变异 | PASS | `:302` 影子表把 `ProbeAckData`（`payload_ref = repo`）改成 `Direct` ⇒ 谓词红；harness 内 3 条影子变异全有鉴别力 |
| **AC-5** C-5 越权消费锁 | PASS | `:316` 驱动头（去注释）不含 `kT3MappingCells` / `kTransitions` / `kProtocolMessageMap` / `kMessageClassMap`，且含 `t3_consumable_event_mask` / `TileLinkStateSpec::transition` |
| **AC-6** D-1 单调性 | PASS | `:459` `Grant → Release` 后 `view_of(addr).sharers == 0` **精确**；**真变异 M-2**（删 `set_sharer`）⇒ `0 == 1` 红 |
| **AC-7** D-2 方向混淆 | PASS | `:377` 「保留 vs 放弃」单差异 ⇒ `Branch` vs `Nothing`（`ProbeAck` + `Prune` 12 格行） |
| **AC-8** D-3 载荷折叠 | PASS | `:390` 需要载荷的行给 `0` 载荷 ⇒ `Undecidable`（裸消息键实现必红） |
| **AC-9** 源=Tip 收 Grant | PASS | `:358` `tl_home_resolve_install(Grant, Tip, src=Tip)` ⇒ `Undecidable`（`TLC-06` 空集） |
| **AC-10** 族 2 判据载体 | PASS | `:424` 正控 `swmr_violations == 0`；`:440` 负控（peer `Branch` → self `Tip`）`swmr_violations == 1` **精确** 且 `invalid == false`；`:544` 纯非 Tip 事件判定行增量 `== 0` |
| **AC-11** 溯源两级 | PASS | `:470` 绕过驱动 `set_sharer(..., Branch)` ⇒ `provenance_broken == true` 且 `untraceable_bits != 0` |
| **AC-12** 装配期硬断言 4 条 | PASS | `:486` node 未映射（`invalid` 且违例数 0，⛔ 不记 0 违例）· `:502` 多 Tip（**不装表**：`sharers` 不变）· `:514` 观测面关闭 · `:527` 溢出（`dropped_observations() == 1`） |
| **AC-13** 观测面有界 + 默认关闭 | PASS | `:514` `CHECK_FALSE(drv.observations_enabled())` + `observations().empty()`；`:527` 容量 2 ⇒ 3 条输入丢 1 条 |
| **AC-14** 档 2 条目 + T8/T9 + 3 条变异 | PASS | `test/cep_evidence_tiers.json` 新增 `tilelink_c-observation`（`carrier_kind = tracker-observation`）· `bash scripts/test/cep_e2e_reconcile_check.sh --mutations` ⇒ **11 条**变异全按签名红（`j` ⇒ 「档 2 条目家族在覆盖地图标记不适用」= **T8**；`k` ⇒ 「载体形态撞值」= **T9**）· ③ `:330` 载体内容判据（**真变异 M-2** ⇒ `CHECK(installs_via_set_sharer(code))` **false**） |
| **AC-15** delta（主体 + 新 Scenario） | PASS | `openspec validate <change> --strict` rc=0（归档前）· 归档合并实测 `+12/−1`（= 主体首句 1 行替换 + 新 Scenario 11 行）· main spec `#### Scenario:` 计数 **8**（族 1 的 7 逐字保留 + 新 1） |
| **AC-16** 冻结面五步链 | PASS | `artifact_count` **90 → 90**（逐字节不变）· `--update --authorized-by "Entry #516"` ×2（`hash 变更 2` → `hash 变更 0`）· 清单**单独 commit** `e97ca964`（1 文件 / +1 −1）· 无参复验 rc=0「provenance 自检通过：90 个产物的 hash 与 commit `ff1dd8bdc062` 的内容一致」 |
| **AC-17** 文档 SU append-only | PASS | `git diff --numstat ff1dd8bd^ ff1dd8bd -- docs/architecture/07-…md` = **+42/−4**，其中 SU 插入段 `@@ -860,0 +861,36 @@` 为**纯增**；4 处删除**全部**落在**生成索引段**（`35 个一级节 / 22 则 SU` 计数与 1 行行号）；既有正文与既有 SU 零改动 · 新 SU 已登记进 `DOC_UNITS`（T6 覆盖） |
| **AC-18** 诚实边界 | PASS | 5 项新增产出物 grep `检测力已证` / `全族达成` / `所有族` / `强于档 1` / `通过态` ⇒ 命中项**全部**处于否定语境（`无通过态` / `⛔ 不构成「检测力已证」` / `不得宣称…通过态`） |
| **AC-19** 范围纪律 | PASS | `git diff --name-only ff1dd8bd^ ff1dd8bd` 不含 `docs/adr/**` / `sharer_tracker.hh` / `protocol_engine_base.hh` / `tilelink_protocol_engine.hh` / `tilelink_tree_manager.hh` / `tilelink_state_spec.hh` / `source_registry.hh` / `sources.json` / `cep-tilelink-runtime-engine/spec.md` / `test_cep_tier2_observation.cc` |
| **AC-20** 0 回归 | PASS | ctest **198/198**（见下「终局复跑」）· `scripts/test/cep_*.sh` **47 个逐个 rc=0 且 0 处 `[SKIP]`** · `rddf_stage_gate.sh --phase queue2` rc=0 ∧ `--self-test` rc=0 · `cep_e2e_reconcile_check.sh --mutations` rc=0 · `docs_sync_check.sh --strict` rc=0 · `agents_md_lint.py --all` rc=0 · `format.sh --check` 仅剩 **21 个既有债**（本轮新增 2 文件 0 债） |

## 决定性变异（真变异 · 按签名红 + 还原后 sha 一致）

- **M-1** 安装表前两行换序 ⇒ `CHECK(row_universe_ok(...))` = `false`；还原后 `sha256` 与变异前一致、`[cep][tilelink][home-driver]` 全绿（110 assertions / 5 cases）
- **M-2** 删 `tracker_->set_sharer(addr, node, target);` ⇒ 4 处按预期红：AC-14③ 载体内容判据 `false` · AC-10 正控 `CHECK_FALSE(v.invalid)` · AC-10 负控 `v.swmr_violations == 1` 变 `0 == 1` · AC-6 `sharers == mSelf` 变 `0 == 1`；还原后 sha 一致、全绿
- 闸内：`j`（family → `moesi_gpu`）⇒ T8 红 · `k`（`carrier_kind` 撞值）⇒ T9 红

## 失败项与路由建议

**无。**（过程态曾出现 3 类红 —— `KPI baseline`（P-5 · 活跃 change = 1）／冻结清单 hash 漂移（未 `--update`）／`cep_doc_ref_check --strict`（SU 引用归档后已移动的活路径）—— 均已在本单元内按正确机制收敛：归档 / 五步链 / 台账 + SU 引用的死路径豁免登记，均非实现缺陷。）

## 遗留清扫（本次 scope 内）

| 项 | 处置 |
|---|---|
| TODO / 半截实现 | **无**（`grep -n "TODO\|FIXME" include/tlm/cep/tilelink_home_state_driver.hh test/test_cep_tilelink_home_driver.cc` = 0 命中） |
| 注释掉的代码 | **无** |
| 临时调试日志 | **无**（真变异用完即还原并核 sha） |
| 被 skip 的测试 | **无**（`[cep][tilelink][home-driver]` 5 cases 全跑；不含 `[!shouldfail]` / `[.]`） |
| 未替换的 mock | **无**（无 mock·驱动直连 `TreeManager` 具体类型） |
| 临时 fallback 分支 | **无**（`Undecidable` 是**契约出口**，非临时兜底） |
| 范围外历史遗留 | 仅**记录不清理**：`format --check` 的 21 个既有 C++ 债 · `R-13a`（main spec trailer）· I-10 两处偏离 —— 均已有 backlog / 待裁记录 |

## 终局复跑（本报告落盘后）

```
ctest --test-dir build --output-on-failure -j4   → 198/198 passed
./build/bin/cpptlm_tests "[cep]"                 → 12740 assertions in 554 test cases（I-7a 时 12614/549 ⇒ +126 / +5）
./build/bin/cpptlm_tests "[cep][tilelink][home-driver]" → 110 assertions in 5 test cases
```
