# verify: cep-ssp-batch2 —— 实施验收（AC 8/8）

> **验收对象**：`cep-ssp-batch2` 的**实现**（S1 批 ②：⑦ functional-only 解释器 + ⑧ 装配 SSP 绑定维 + ⑨ 掩码格两形态 + ⑩ 档 0 实验 4 件套 + C1~C10 实例化）
> **验收时间**：2026-11-27
> **执行角色**：builder（角色内建 verifier 契约 · `rdd-verifier` v2.0 内联 LLM 协议）
> **实施分支**：`openspec/cep-ssp-batch2` @ `c3d80b3f` · 已由 P3 合入 main @ `4a72b705`（归档 `2026-11-27-cep-ssp-batch2`）
> **verdict cache**：`.rddf/state/verifier/.ac-verdict-cep-ssp-batch2.json`（`codebase_commit = c3d80b3f` · `verification_state = passed` · `rdd rdd-verify` 报 `states: ['passed']`）
> **授权**：台账 Entry #558（P2 实施 + 4 处冻结面五步链）· Entry #559（AC-7 补齐 + 技术误判纠正）

---

## 判据来源（优先级 1 → 4）

1. 用户本次明确指令：P2.5 裁决 = **路径 A（本 change 内补齐 AC-7）** + **merge（合入并进 P3）**
2. `openspec/changes/cep-ssp-batch2/proposal.md` 的 `## 验收标准`（8 条）
3. 本批 commit 承诺的行为（6 个 commit 的 message）
4. `ADR-CEP-24` 决策 6 的交付终点 / `ADR-CEP-14` C1~C10 / `ADR-CEP-20` 决策 6

---

## 逐条判定（8/8 PASS · 每条 ≥1 条工具证据）

| AC | 判定 | 证据 |
|---|---|---|
| **AC-1** 可装配（id + adapter + SSP 三方解析）| **PASS** (0.95) | `[cep][assembly]` **109 assertions / 10 cases** 全绿；IAG-09 含 5 条 fixture（未登记 / 分支不一致 / 非 `user:` 形态 / adapter 缺 / `ssps_doc=nullptr` 零变更见证）· `instance_assembly_gate.hh` 第 ⑦ 维 |
| **AC-2** 4 件套 | **PASS** (0.95) | `cep_ssp_runtime_check.sh` **7 项全绿**（A-1 断言 4 件齐备）· `cep_experiment_run.sh --ssp` 复用既有 `emit_trace`/`render_report`（零复制）· `render_report --check` 通过 |
| **AC-3** 洞命中 ⇒ 违例计数 + 不可判 | **PASS** (0.97) | `[cep][ssp][interp]`：`ok==true`（不 panic）· `violation_count==1` · `judgeable==false`；A-4 反向 fixture：`CEP_SSP_HOLE=1` ⇒ **拒绝产出**（无 metrics/simulation_config）|
| **AC-4** 掩码格（1 正 1 负）| **PASS** (0.95) | `[cep][ssp][mask]`：`tilelink_cap_ssp` **12 格与 `kTransitions` 逐位相等**（转置对齐）+ 反向改一位必红 + E14 五条非法形态（缺键/多键/非整数/负/越位宽）|
| **AC-5** 瞬态态零泄漏 | **PASS** (0.90) | `ssp_interpreter.hh` 编译期零时序锁 `sizeof(SspStep)==3*sizeof(std::string)`；解释器只读声明面、无瞬态态生成路径；C9 用例断言每步目标态必为声明面稳定态 |
| **AC-6** 证据声明档位 | **PASS** (0.95) | A-3：`simulation_mode==functional`（⛔ 非 timing）· 13 项全 `not-collected` · `_measured` 空 · scope 逐字含「零时序」「不承诺语义」；`render_report` 的 IC3 判「不适用」|
| **AC-7** C1~C10 在 SSP 上实例化 | **PASS** (0.85) | `[cep][ssp][contract]` **6 cases / 76 assertions** 全绿：**C1/C2/C3/C9 PASS** · **C4~C8/C10 SKIP**（Stub 行为纪律 / 组合接缝 ⇒ SSP 声明面无此面）· 闸 A-6 断言该组全绿 |
| **AC-8** 零回归 | **PASS** (0.98) | `[cep]` **599 cases / 13923 assertions** 全绿（主仓基线 585/13719 ⇒ +14 全为本批新增）· `ctest` **211/211 passed**（3 parity 环境 Skipped 非失败）· `scripts/test/cep_*.sh` **57/57 rc=0** |

---

## ⚠️ 两处必须随引用传播的限定

1. **AC-7 的 SKIP 不得记为 PASS**：C4~C8（outstanding / snoop 延迟 / completion 配对 / writeback 载荷 / hit 路径）与 C10（组合接缝）**属 Stub / 组合器侧**，SSP 声明面**无此面** ⇒ 判 **SKIP**（per `AdapterSchemaContracts` 三态纪律）。⛔ 不得宣称「C1~C10 **全部** PASS」。
2. **技术误判纠正（Entry #559）**：Entry #558 ⑥ 曾判「AC-7 需 SSP adapter + engine，约 500~800 行」—— **实测错误**。`ADR-CEP-14` 决策 1 逐字写着「合同测试断言的是 Stub 对 Coherence Interface 的**投影行为**是否符合契约，**不**断言实现满足契约」，其载体自身也只断言「契约存在且自洽」⇒ 实际 **~150 行测试**。

---

## 遗留清扫（第 5 步）

| 检查项 | 结果 |
|---|---|
| 本批新文件 TODO / FIXME | **0** |
| 临时调试日志 | **0** |
| 注释掉的代码 | **0** |
| 被 skip 的测试（`[!mayfail]` / `SKIP()`）| **0** |
| 新增 `.cc.disabled` | **0** |
| `tasks.md` 未勾项 | **0**（T0~T8 全勾）|

---

## 已知环境坑（不影响判定，记录供后续）

1. **worktree 缺 `openspec/changes/archive/` 归档件**（`.gitignore` 排除 ⇒ 178 目录中仅 77 被检入）⇒ 7 个测试在全新 worktree 假红。处置：以**主仓基线 585/585 全绿** + **修复 worktree 环境后 593/593 全绿** 双重对照排除。
2. **`experimental/run-20261113-151310-cep-full-green/` 缺 `report.html` + `trace.h5`**（同样 gitignore）⇒ `cep_full_green_run_check` 假红。处置：从主仓补件后全绿。
3. **`.rddf/state/iteration.json` 从未被创建**（P0 要求文件已存在才登记）⇒ `rddf rdd-verify` 看不到任何 change ⇒ 归档门阻塞。处置：用官方 `skills._lib.iteration.store` API（`create_empty` + `add_or_update_change` + `save`）补建并登记（最接近单写入口）。
4. **P3 的 `commit_archive_moves: git add failed`**：`.gitignore` 排除 `openspec/changes/archive/` ⇒ 归档件须 `git add -f`（批 ① 先例），且 `archive_verify_baseline.txt` 43 → 44。

---

## 已用轮次

- 本 change 的 verifier 轮次：**1**（首轮全 PASS，无重试）
- `rddf rdd-verify` aggregate exit **0**（`states: ['passed']`）

---

## 结论

**AC 8/8 PASS** ⇒ `route = archive-ready` ⇒ 已归档（`openspec/changes/archive/2026-11-27-cep-ssp-batch2`）。

⛔ **表述纪律（交付后仍适用）**：
- ✅ 可以说「CEP 已支持用户自定义协议」的**形态 + 引用闭环**档（objective `obj-cep-custom-protocol` 的 S1 必达 DoD 达成）
- ⛔ **不得**表述为「协议语义已真实链路验证」（零时序 · 时序面属 S2 `ADR-CEP-25`）
- ⛔ **不得**宣称「C1~C10 全部 PASS」（C4~C8/C10 = SKIP）
- ⛔ **不得**宣称 ⑨ 的 `ssp.schema.json` / `ssps.json` 的 contract 面已受 M1 保护（**不在** M1 冻结集 · architect 域待裁 · Entry #557 ⑥）
