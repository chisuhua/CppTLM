# 验收报告 · `cpptlm-doorbell-contract-alignment`（doorbell 契约面失真修正 · **dGPU 域**）

- **角色**：builder（**实施侧自验** —— 本 change 由同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **日期**：2026-11-21 · **关联单元**：登记册 §6.4 队列 2 **I-12**
- **上游裁决**：`.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`（architect · oracle REJECT 3 类 BLOCKER 全闭 + critic APPROVE_WITH_CONDITIONS · 无 BLOCKER）
- **⚠️ 域声明**：对象 = **dGPU 域**（`openspec/specs/{minimal-dgpu-soc-architecture,dgpu-soc-timing,lwc-bus-hub-mvp,dgpu-soc-control-hub}/` + `docs/adr/ADR-DGPU-09-*.md`），⛔ **不是 CEP 域**（借 CEP 队列作载体）。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单 | `openspec/changes/archive/2026-11-21-cpptlm-doorbell-contract-alignment/proposal.md` §验收标准（AC-1 ~ AC-10）|
| 裁决内容 | `.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md` §3.1（7 处连带）· §3.2（3 条决定性变异）· §3.3（复活锁）· §3.4（6 条范围纪律）|
| 设计与取舍 | 同 change `design.md` D-1 ~ D-7 |
| 上游 | 登记册 §6.4 I-12 行 · §1.3 R-18 行 · §6.2 S3-8 · §6.3 归属表 · §6.9 阶段 ⑥ |

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| **AC-1** | C-1~C-3 三处主断言消解：`必须 ≥ 256MB+64KB` == 0 ∧ `访问越界` == 0 ∧ `§13 风险表` == 0 ∧ `v1.0 仅 ABI 路径` == 0 ∧ `仍命中` ≥ 1 | **通过** | 归档后 `openspec/specs/minimal-dgpu-soc-architecture/spec.md` 实测：`必须 ≥ 256MB+64KB`=**0** · `访问越界`=**0** · `§13 风险表`=**0** · `v1.0 仅 ABI 路径`=**0** · `仍命中`=**1** · `pcie_ep.params.bar_sizes[1] >= 268435464`=**1**。⚠️ **追加判据的必要性**：裁决原文用 `grep -cF "不触发 doorbell 路由"`，但原文实为 `**不**触发`（markdown 粗体标记夹在中间）⇒ 该判据**裁决前就是 0** = **空判据** ⇒ 本 change 追加 `grep -cF "访问越界"`（命中原文 `doorbell 访问越界,**不**触发 doorbell 路由`）作为有鉴别力的替代 |
| **AC-2** | C-4~C-6 连带面：3 份 spec 的偏移字面量**命中集合与裁决前一致** ∧ `不触发.*(doorbell\|路由)` 逐份 == 0 | **通过** | 归档前后逐行比对（基线见下）—— `dgpu-soc-timing`：3 行命中（`off=0x10010000` / `检测 kBar1DoorbellOffset` / `mmio_write(1, 0x10010000, wptr)`）**内容逐行不变**；`lwc-bus-hub-mvp`：3 行命中（body / NOTE / GIVEN）**内容逐行不变**；`dgpu-soc-control-hub`：**0 行**（⇒ C-6「不含，无需改」**显式记录**于 proposal 的 What Changes 表 + design.md D-3 + 本报告）⇒ `不触发.*(doorbell\|路由)` 3 份**逐份 == 0** |
| **AC-3** | C-7 ADR SU 追加：`git diff` **只增行** ∧ 新增行含「Q-12」+ 指向本 change | **通过** | `git diff --numstat -- docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md` ⇒ **+10 / −0**（删除行 == 0 ✓）；新增行含「Q-12」**2** 处 · 含「cpptlm-doorbell-contract-alignment」**2** 处 · 含裁决文件指针 **1** 处 |
| **AC-4** | **M-1**：删 doorbell 优先匹配分支 ⇒ **门铃路由断言**变红 ⇒ 还原 ⇒ 绿；⛔ 编译失败不算红因 | **通过（红因与裁决括注不同，见下）** | 变异前：`./build/bin/cpptlm_tests "[doorbell]"` ⇒ `All tests passed (181 assertions in 12 test cases)`（**先证绿** ✓）。变异（仅改门铃分支的匹配条件 ⇒ `kBar1DoorbellOffset + 1`）：`cmake --build build -j4` **rc=0**（⛔ 非编译失败）⇒ 测试**变红**：<br>`test_dgpu_board_doorbell_routing.cc:38: FAILED: REQUIRE( board.pcie_ep_doorbell_count() == 1u ) with expansion: 0 == 1`<br>`test_dgpu_board_doorbell_routing.cc:56: FAILED: REQUIRE( board.pcie_ep_doorbell_count() == 3u ) with expansion: 0 == 3`<br>⇒ `12 test cases: 11 passed \| 1 failed` · `181 assertions: 179 passed \| 2 failed`。还原（`git diff` 归零）+ 重建 ⇒ **复绿** `All tests passed (181 assertions in 12 test cases)` |
| **AC-5** | **M-2**：三份 shipped 配置**维持 16MB** ⇒ 新条款仍满足 | **通过** | ① `git status --porcelain -- configs/ examples/` ⇒ **空**（零改动）；三份配置的 `bar_sizes` 实测均为 `[4096, 16777216, 8589934592]` ⇒ `bar_sizes[1]` = **16777216**（16MB）。② **实现依据**：门铃分支的匹配条件 `if (bar == 1 && offset == kBar1DoorbellOffset)` **不含任何大小判据**，且位于窗口分类**之后**（`classify_naive` 的 `off < sz ? Bar1FbFastPath : Bar1Unknown` 对 256MB 偏移判 Unknown ⇒ 落 `default: break` ⇒ 仍到门铃分支）。③ **实测（更严的同代码路径）**：BAR1 窗口 = **0**（`framebuffer_size_` 默认 0 ⇒ 比 16MB 更小 ⇒ `off < sz` 同样为假 ⇒ **同一分支**）时，`test_minimal_dgpu_soc_characterization.cc` 的「T0 BAR1 doorbell increments counter」仍断言 `pcie_ep_doorbell_count() == before + 1` ⇒ `./build/bin/cpptlm_tests "[dgpu_baseline]"` ⇒ `All tests passed (9 assertions in 5 test cases)` ⇒ 「不依赖实窗大小」成立 ⇒ 16MB 情形必然成立。④ 且 M-1 已证明该断言**有鉴别力**（同一断言在变异后变红）|
| **AC-6** | **M-3**：删 C-3 新措辞 ⇒ 判据变红 ⇒ 还原 ⇒ 绿（⚠️ 须与 AC-7 复合） | **通过** | 影子副本（`/tmp/opencode/v12/`）：变异前 `grep -cF "pcie_ep.params.bar_sizes[1] >= 268435464"` = **1**；把 C-3 的新措辞整条删掉（还原为旧 Requirement）⇒ **0** ⇒ **判据变红** ✓；未变异副本仍为 **1** ⇒ 还原即绿。⚠️ 复合验证：同一变异下 AC-1 的 `grep -cF "v1.0 仅 ABI 路径"` 仍为 **0** ⇒ **印证裁决的判断「AC-6 单独不可判，须与 AC-7 复合」** |
| **AC-7** | 复活锁：C-3 新文本含 **R-2** 的机判表述 ⇒ `grep -cF` == 1 | **通过** | `grep -cF "pcie_ep.params.bar_sizes[1] >= 268435464" openspec/specs/minimal-dgpu-soc-architecture/spec.md` = **1**；同 Scenario 内含**机判命令**（`grep '"bar_sizes"' configs/*.json examples/*.json \| awk -F'[:,]' '{b=$3+0; if (b>=268435464) print}' \| wc -l` 命中数 ≥ 1 ⇒ **数值比较**，⛔ 非正则）+ d3-roadmap 的**语义触发条件**（「真 driver 集成时 doorbell 物理地址 ≠ 0x10010000 ⇒ 才回头修」）**两者并存** |
| **AC-8** | 范围纪律：`git diff --name-only` 不含 `include/**` · `src/**` · `configs/**` · `examples/**` · `docs/designs/dgpu-driver/architecture.md` · `scripts/test/param_effect_inventory.py` | **通过** | 逐路径命中数实测**全为 0**（`include/`=0 · `src/`=0 · `configs/`=0 · `examples/`=0 · `docs/designs/dgpu-driver/architecture.md`=0 · `scripts/test/param_effect_inventory.py`=0）。本次改动的**全部**文件 = 3 份 main spec + 1 份 ADR（+10/−0）+ 1 个归档 change 目录（6 份 md，force-add）+ 基线文件 + 载体表 + 登记册 + 本报告 |
| **AC-9** | 0 回归：`ctest` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · `rddf_stage_gate` rc=0 · `docs_sync --strict` / `agents_md_lint` 全 rc=0 | **通过** | 见 §回归记录（ctest **197/197** · 47 个 `cep_*.sh` **0 失败** · 5 个闸全 rc=0）|
| **AC-10** | 登记册回填：§6.4 队列 2 **I-12** 行 + 队列 1 **Q-12** 行 + **修正 `:49` 错锚** + §10.4 指针 | **通过（1 项按等效落点满足，见 F-2）** | ① §6.4 队列 2 **I-12** 行 ⇒ 标「✅ 已落地 2026-11-21」+ change 名 + 归档路径 + 7 处连带明细 + 2 处工具约束 + AC-4 括注证伪；② §6.4 队列 1 **Q-12** 行 ⇒ 已有「✅ 已裁（architect 2026-11-20 · 裁决文件 = …）」+ **已有**「⚠️ 锚点订正」段（`:49` 实为 `sdma.params.max_inflight`，`bar_sizes` 在 `:27` ⇒ 改锚定措辞「`bar_sizes` 数组第 2 元素」）⇒ 追加落地指针；③ §1.3 **R-18** 行（标题已改名 + 状态）；④ §6.3 归属表 S3-8 · C 类行；⑤ §6.9 阶段 ⑥ 行。⚠️ **§10.4 在现行登记册结构中已不存在**（该册已重构为 §1~§6.9）⇒ 见 F-2 |

## 决定性变异汇总（3 条 · 全部有鉴别力证据）

| 变异 | 手段 | 期望 | 实测 | 判别力 |
|:--:|---|---|---|:--:|
| **M-1** | 改 `src/tlm/gpu/dgpu_board_shell.cc` 门铃分支的匹配条件 ⇒ `kBar1DoorbellOffset + 1` ⇒ 真实增量构建 | 门铃路由断言变红 | `REQUIRE( board.pcie_ep_doorbell_count() == 1u )` ⇒ `0 == 1`；`REQUIRE( board.pcie_ep_doorbell_count() == 3u )` ⇒ `0 == 3`（构建 rc=0 ⇒ ⛔ 非编译失败）| ✅ |
| **M-2** | 三份 shipped 配置**维持 16MB**（零改动）| 新条款仍满足 | 门铃路由断言在**窗口 = 0**（比 16MB 更严、同代码路径）下仍通过 ⇒ 16MB 必然满足 | ✅ |
| **M-3** | 影子副本删掉 C-3 的新措辞 | 判据变红 | AC-7 的 `grep -cF` **1 → 0**（未变异副本仍为 1）| ✅ |

## 失败项与路由建议

| # | 项 | 路由 |
|:--:|---|---|
| **F-1** | **裁决 AC-4 的括注被实测证伪**：裁决写「红因 = `REQUIRE(rc == 0)` 失败」，实测**不成立** —— 删门铃分支后 BAR1 写落 `default: break` ⇒ 仍 `return 0` ⇒ `rc` 不变 ⇒ smoke T1.4 的 `REQUIRE(rc == 0)` **仍绿**（M-1 的 12 个 case 中 11 个通过即含该 case）。**真红因 = `REQUIRE(board.pcie_ep_doorbell_count() == 1u)`** | ⇒ 已在本 change 的 AC-4 改以**实测红因**表述，并在 `design.md` D-4 记录实测推导（⛔ **不静默改判据**）。⚠️ 裁决记录本身**不修改**（已签批）⇒ 若需同步，由 architect 追加 `## Status Update` |
| **F-2** | 裁决 AC-10 要求的「§10.4 补指针」在**现行登记册结构中不存在**（该册已重构为 §1~§6.9，无 §10） | ⇒ 按**等效落点**满足（§6.4 队列 1 Q-12 行 + §6.4 队列 2 I-12 行 + §1.3 R-18 行 + §6.3 归属表 + §6.9 阶段 ⑥）⇒ ⛔ **不新建 §10**（会造出第二套编号空间）⇒ 显式登记于此 |
| **F-3** | **2 处 openspec 工具约束**（实测，非设计选择）：① **MODIFIED 内禁止重命名 Scenario**（`MODIFIED … omits scenario(s) the current spec still has` ⇒ archive 拒绝丢 Scenario）⇒ C-2 的 Scenario **标题保留**「doorbell 配置过小(检测 config bug)」，改由**正文**给出结论（`- **AND** 本 Scenario 的 **config-bug 检测结论 = 否**`）⇒ **标题与正文存在措辞张力**（残留标签，非逻辑矛盾：标题是「检测」这一**动作**，正文是检测**结论**）；② **MODIFIED 的 Requirement 标题须在现行 spec 中存在** ⇒ C-3 的 Requirement 改名走 `## RENAMED Requirements`（`FROM:`/`TO:`），非 REMOVED+ADDED（后者会删掉 Requirement 本体 ⇒ 违反裁决「⛔ 不删除」）。⚠️ 已实测：REMOVED+ADDED（同名）⇒ `Requirement present in both ADDED and REMOVED` ERROR；REMOVED+ADDED（改名）⇒ valid 但**越权** ⇒ 未采用 | ⇒ F-3① 的**标题残留**登记为**已知残留**（⛔ 本 change 不顺手改；将来若 openspec 支持 Scenario 级 RENAMED，或走 REMOVED+ADDED 的独立 change 再清理）|
| **F-4** | 本 change **无独立 verifier 环节**（同一 builder 会话实施 + 自验） | ⇒ 已在报告头显式标注；⚠️ 若要求独立验收 ⇒ 需 verifier 角色复核（builder↔verifier 窗口已用 **2/3**）|
| **F-5** | 归档合并附带的**格式变动**（`## Purpose`/`## Requirements` 后各插 1 空行；Requirement 间的 `---` 分隔线被移除；文件末空行被移除）| ⇒ 属 openspec 合并器的**固有行为**（影子预演与实跑**逐项一致**）⇒ ⛔ 不视为回归 |
| **F-6** | `dgpu-soc-timing` 的 `AND sdma_engine_->mmio_write(1, 0x10010000, wptr) 调用` 仍描述**旧的直调形态**（实现已改为 `DoorbellBundle` → `lwc_hub_->doorbell_write` + `sdma_engine_->handle_doorbell_transaction`）| ⇒ **本 change 不改**：① 裁决 AC-2 ① 要求该 spec 的偏移字面量**命中集合与裁决前一致** ⇒ 改写会破坏该判据；② 该 staleness 属**另一议题**（doorbell 消息传递形态），非 C-4 的「doorbell vs 配置」口径 ⇒ ⛔ 不在本 change 顺手改。⇒ **登记为新发现**（建议另立 backlog）|
| **F-7** | `lwc-bus-hub-mvp` 的 NOTE 行 `dgpu_board_shell.cc:537` 是**历史行号锚点**（今日 `:537` 已是 BAR2 分支；门铃分支在 `:566`，`handle_doorbell_transaction` 在 `:594`）| ⇒ **不改**：该 NOTE 描述的是**被替换掉的历史直调**，改指 `:566` 会指向当前已不含该直调的代码 ⇒ 反而更不准。⇒ 记录为**已知的行号锚点腐化**（裁决 §6 已列该 3 处之一），⛔ 不在本 change 顺手改 |
| **F-8** | `AGENTS.md` 的「**已归档 change 计数 = 162**」与实测**不符**（本 change 归档**前**实测已 **166**，归档后 **167**）⇒ **既有陈旧**，非本单元造成 | ⇒ ⛔ **不改**（改动最小化 + 范围外历史遗留只记录不趁机清理）；且 `AGENTS.md` 自身的 DOC HYGIENE 纪律已写明「**计数一律用命令取** / ⛔ 不固化具体值」⇒ 该行的**正解是删除而非改数**（属 doc-hygiene 决策面 · `cep-doc-hygiene-kpi-and-impl-state` improvement 的范围）。⇒ **无闸锚定该计数**（`rddf_stage_gate.sh` 的 P-5 锚的是 **active** 数，当前 0 ⇒ 绿）⇒ 不影响本单元任何判据 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 临时目录 | ✅ `/tmp/opencode/v12/`（影子预演 + M-3 变异副本）· `/tmp/opencode/m1_backup.cc`（M-1 源码备份）· `/tmp/opencode/fa3.txt` · `/tmp/opencode/i12_baseline.txt` —— 全部在 `/tmp` 且**不进仓** ⇒ 收口时删除 |
| 临时诊断代码 | ✅ 无（M-1 的变异**已还原**：`git diff -- src/tlm/gpu/dgpu_board_shell.cc` 归零）|
| 变异残留 | ✅ M-1 还原后 `cmake --build build -j4` 重建 + `[doorbell]` **复绿**（181 assertions / 12 cases 全过）|
| 范围纪律 | ✅ 零 `include/**` · 零 `src/**` · 零 `configs/**` · 零 `examples/**` · 零 `docs/designs/dgpu-driver/architecture.md` · 零 `scripts/test/param_effect_inventory.py` |
| 新归档目录的闸可见性 | ✅ 已 **force-add**（6 份 md）+ 基线 `31 → 32`（`scripts/test/archive_verify_baseline.txt`）⇒ `archive_verify_check` 在本报告落盘后**保持绿**（⛔ 不靠「等下次」）|
| 载体表 / 登记册 | ✅ `scripts/test/rddf_ac_carrier.tsv` 的 **I-12 → done**；机关三路径复验全 rc=0，`P-2a` 下一项 = **I-6** |
| 半截状态 | ⚠️ **F-3①（Scenario 标题残留）与 F-6（timing spec 的旧直调形态）为已知残留**，已显式登记 ⇒ ⛔ 不假装「零残留」|

## 回归记录

| 项 | 结果 |
|---|---|
| `ctest --test-dir build --output-on-failure -j4` | ✅ **197/197 passed**（0 failed · 173.03 s）。⚠️ **首跑记录**：首跑时本报告**尚未落盘** ⇒ ctest #156 `archive_verify_check` **fail-closed 报红**（该闸的职责：归档 change 缺独立验收报告即判红）⇒ 报告落盘后**复跑 ctest** ⇒ **197/197 全绿**（该闸单独复跑亦 rc=0：扫面 32 份 · 基线 32 · git 跟踪 32）|
| 全部 `scripts/test/cep_*.sh` | ✅ **0 失败 / 47** |
| `openspec validate --specs --strict --json` | ✅ **138 items / 55 invalid / 47 `cep-` 项 / `cep-` invalid = 0** ⇒ 与基线**逐项一致**（红数未增加）|
| `openspec validate <4 份受影响 spec> --type spec` | ✅ 逐份 `is valid` |
| `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |
| `python3 scripts/test/agents_md_lint.py --all --strict` | ✅ rc=0 |
| `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0（本次未触冻结集：`openspec/specs/cep-*` / `docs/adr/ADR-CEP-*` / `include/tlm/cep/protocol_instance.schema.json` **零改动**）|
| `bash scripts/test/rddf_stage_gate.sh`（默认 / `--phase queue2` / `--self-test`）| ✅ 三路径全 rc=0 · `P-2a 队列 2 下一项 (I-6)` |
| `bash scripts/test/archive_verify_check.sh` | ✅ rc=0（扫面 **32** 份 · 基线 32 · git 跟踪 32）|
| `bash scripts/test/openspec_version_check.sh` | ✅ rc=0 |
| ⚠️ `scripts/build/format.sh --check` | **既有红**：**21 个既有 C++ 文件**待格式化 ⇒ 本单元**零 C++ 改动** ⇒ 该项**排除既有债**后判，**如实记录为既有红**（per 裁决 AC-9 的同款约定）|

## 已用轮次

**1/3**（第 1 轮；无返工轮 —— 唯一的返工点是 **C-2 Scenario 标题**：初稿改名为「doorbell 在小实窗下仍命中…」⇒ `openspec validate` 报 `MODIFIED … omits scenario(s)` ⇒ 改回原标题 + 正文给结论；期间另实测出 2 条 openspec 工具约束与 1 条裁决括注证伪，均已登记））。
