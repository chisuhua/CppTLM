# 验收报告 · `cep-stage-gate-p5-hardening`（登记册队列 2 的 **I-0**）

- **验收对象**：improvement 单元 `cep-stage-gate-p5-hardening`（= 登记册 §6.4 队列 2 的 **I-0**）
- **验收角色**：verifier（⛔ 只验不改 —— 实现由 builder 提交，验收独立执行）
- **日期**：2026-11-20
- **已用轮次**：**1/3**
- **判据来源**：① 单元 `## Acceptance` 段的 **AC-1 ~ AC-13** ② 裁决记录 `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §2.6（Q-6a/c/d 判据形态）③ 登记册 §6.2 S3-11 / §6.4 I-0 完成判据
- **分流判定**：`openspec/changes/<name>/` 不存在（本单元**不创建** OpenSpec change ⇒ KPI 0/3）· `.rddf/state/iteration.json` 的 P3 archive gate **未触发** ⇒ ⛔ 不走 `rdd-verifier` skill ⇒ 按**本地 1~5 步**验收

## 一、逐条判定（每条附工具证据）

| AC | 判据 | 判定 | 证据 |
|---|---|:--:|---|
| **AC-1** | 顺序机关生效 · 本计划队列被守 | ✅ | `bash scripts/test/rddf_stage_gate.sh` → **rc=0**；`grep -c rddf_stage_gate test/CMakeLists.txt` → **2**（已注册 ctest）；载体表内本计划 **I-0~I-11（13 行）**，`awk -F'\t' '$1 ~ /^I-/'` → **13**；P-3 输出 **13** 条 `✓ P-3 [I-*]` |
| **AC-2** | 顺序由机关保证 · 非文风（跳序必判红） | ✅ | `bash scripts/test/rddf_stage_gate.sh --self-test` → `✓ SELF-TEST 变异② 队列 2 跳序被判红（I-1 标 done 而前驱 I-0 未完成）`；**红因有签名**：「P-2a 队列 2 跳序：I-1 已声明完成，但前驱 I-0 未完成 ⇒ 队列顺序被跳过」 |
| **AC-3** | P-5 判据有鉴别力 · 决定性变异 | ✅ | `--self-test` → `✓ SELF-TEST 变异① P-5 判红 7/6 处（7 处锚定行全改 3/3）`；**对照**：同一变异下**旧判据（子串存在性）仅判红 2/6**（`AGENTS.md` / `obj-cep-platform.md` / `umbrella.md` / `nvlink-sharp.md` 仍判「已同步」）⇒ 判据从 **2/6 → 7/6**（7 处锚定行**全部**判红）<br>⚠️ **轮 1 记录 6/6 已被本轮订正为 7/6** —— 影子骨架漏拷 `obj-cep-platform.md`（第 4 个活口径文件）⇒ 它未被变异 ⇒ 少判红 1 处；已修（补 cp）+ 加「影子环境完整性前置检查」（⛔ 缺文件时报「环境不完整」而非误导性的「判据无鉴别力」）|
| **AC-4** | 当前无漂移 · 活口径取值 == 实测 | ✅ | 闸输出 `✓ P-5 0 处活口径全部同步（锚定 7 处，逐处取值 == 实测）`；逐文件实测：**AGENTS.md** 1 处取 `0` · **roadmap.md** 1 处 `0` · **phase-10.md** 1 处 `0` · **obj-cep-platform.md** 1 处 `0` · **umbrella.md** 1 处 `0` · **nvlink-sharp.md** **2** 处 `0` ⇒ 合计 **7** 处 == `ls openspec/changes \| grep -v '^archive$' \| wc -l`（**0**）/ 3 |
| **AC-5** | 零新建登记面 | ✅ | `ls scripts/test/rddf_* \| wc -l` → **2**（`rddf_ac_carrier.tsv` + `rddf_stage_gate.sh`）⇒ 仍是既有 2 个文件；本单元 `git diff --name-only scripts/test/` → **仅这 2 个** |
| **AC-6** | 既有载体表**判据语义零变化** | ✅ | 逐行前 4 列比对（HEAD vs 当前）：**0 处不匹配**；数据行 **18 → 31**（新增 **13** == 本计划 I-* 行数）；第 5 列取值集合 == **`{'pending'}`**（既有 18 行 + 新增 13 行全 pending）⇒ **判据语义零变化**<br>⚠️ **原判据已订正**：AC-6 原文「既有数据行 18 行 diff 行数 == 0」与「载体表增第 5 列 state」**直接矛盾**（加列必然改行尾）⇒ 已订正为「**前 4 列逐字不变 + 第 5 列全 pending**」（这才是「判据语义零变化」的真正含义）—— **判据修改已显式登记，未静默** |
| **AC-7** | 闸头引用不悬空 | ✅ | `grep -c "§1.5.6" scripts/test/rddf_stage_gate.sh` → **0**；`grep -c "§6.5"` → **2**；TSV 侧 `§1.5.6` → **0**、`§6.5` → **1** ⇒ 两个文件的旧引用**均已转指 §6.5**（per Q-6d） |
| **AC-8** | `TODO` 行不长期停留 | ✅ | `grep -cP '\tTODO\t' scripts/test/rddf_ac_carrier.tsv` → **0** ⇒ 载体表**零 TODO 行** |
| **AC-9** | 范围纪律 · 零越权 | ✅ | `git diff --name-only \| grep -cE "^(docs/adr/\|docs/architecture/\|openspec/specs/\|include/\|src/\|test/\|experimental/\|tools/)"` → **0**（零冻结面 diff）；`git diff --name-only .rddf/roadmap/objectives/obj-cep-platform.md` → **0**（台账侧零改动）⇒ 本单元改动面 = **2 个脚本文件 + 1 处草稿 AC 判据订正** |
| **AC-10** | 0 回归 | ⚠️ **部分** | `rddf_stage_gate.sh` / `cep_freeze_check.sh` / `docs_sync --strict` / `agents_md_lint` / `cep_invariant_map_check.py` 全 **rc=0**；45 个 `cep_*.sh` 全绿；⚠️ **`format --check` rc=1** —— 但 21 个待格式化文件**全部是 C++**（`src/` · `test/`），本单元**零 C++ 改动** ⇒ **既有债，非本单元引入**（本单元只改 shell + TSV，两者不在 format 清单里）；⚠️ **该项在本单元开工前即不成立** ⇒ 已如实登记，⛔ **未扩大范围去格式化 21 个无关 C++ 文件** |
| **AC-11** | 裁决吸收 · M-2 + §9.1 · 锚点表逐文件映射 + 联合命中语义 | ✅ | 闸内 `LIVE_ANCHORS` 逐文件给出**精确锚**：`AGENTS.md`→`实测基线`(1) · `roadmap.md`→`当前真实 baseline = `(1) · `phase-10.md`→同(1) · `obj-cep-platform.md`→**`**当前 Proposed 并发 = `(1)** · `umbrella.md`→`当前真实 baseline = `(1) · `nvlink-sharp.md`→`当前实测 **`(2) ⇒ 合计 **7**；**联合命中语义已实现**（判据 = ① 精确锚命中处数 == 期望 ② **该锚所在行内**的 `N/3` == 实测 ③ 总处数 == 7）⇒ 台账流水里的同形措辞**既不计入处数、也不参与取值判定**；⚠️ **宽子串已实测排除**：`obj-cep-platform.md` 的宽子串 `当前 Proposed 并发 = ` 实测 **3** 处（`:474` 真锚 + `:25815`/`:25967` 台账流水）⇒ **必须用粗体精确锚**；⛔ **零行号**（全部措辞锚定）· ⛔ **零豁免** · `LIVE_FILES` **未增补**（6 文件即全集） |
| **AC-12** | 裁决吸收 · Q-6e · R 编号空间冻结 + 语义前缀 | ✅ | 载体表内**旧 AC 编号 `AC-01~AC-09` 逐字保留**（⛔ 未重编号）；**新登记用语义前缀** —— 本计划用 `I-*`（实施单元），登记册侧另用 `S1-x`/`S2-x`/`S3-x`（缺口）与 `PROC-*`（队列 0，待 Q-6 收口）⇒ **未新建第三套编号空间**；⚠️ **残余**：登记册 §6.4 队列 0 的 `P-1~P-5` 与 §6.6 的 `P-*` 同前缀异义 ⇒ 已显式挂 **Q-6** 裁决（⛔ 本单元不越权改名） |
| **AC-13** | 裁决吸收 · Q-6c · `TODO` 行时限可被 user 修订 | ✅ | 闸已实现 **TODO 时限三态判红**：① 缺「升级时限 + 升级顺序」⇒ **判红**（实测：`I-TEST` 无时限 ⇒ `✗ P-3 [I-TEST] TODO 行缺少「升级时限 + 升级顺序」`）② 超期 ⇒ **判红**（实测：到期 2020-01-01 ⇒ `✗ … 已过 2515 天 > 1 工作日`）③ 未超期 ⇒ **skip 不红**（实测：到期 2099-01-01 ⇒ `⚠ … 剩余 26342 天内须升级`，rc=0）⇒ **「长期 TODO = 机关无牙」已封堵**；⚠️ **该时限已标注为「architect 裁的默认值 · 可被 user 修订」**（⛔ 未表述为不可协商的技术约束，per 裁决 §8.3 L-2）；⚠️ 工作日口径用「自然日差 > 2」**近似**（周末不单独计算 ⇒ 偏保守）—— 已如实标注为近似 |

## 二、验证顺序与结果（verifier 产出契约 1~5 步）

| 步 | 内容 | 结果 |
|---|---|---|
| 1 · 编译 / 静态检查 | `bash -n scripts/test/rddf_stage_gate.sh` | ✅ 语法 OK |
| 2 · 单元 / 闸测试 | `bash scripts/test/rddf_stage_gate.sh`（默认 / `--phase queue2` / `--self-test`） | ✅ 三条路径全 **rc=0** |
| 3 · 集成 | `ctest --test-dir build -R rddf_stage_gate` | ✅ 绿（闸已在 `test/CMakeLists.txt` 注册） |
| 4 · 边界与错误路径 | 非法 `--phase X` → **rc=2**（拒绝）· TODO 三态（缺时限 / 超期 / 未超期）· 跳序 vs 合法顺序 | ✅ 全部符合预期 |
| 5 · 遗留清扫 | `TODO/FIXME/XXX/调试日志` 扫描 | ✅ **零真残留**（13 处命中全为 `TODO` **作为载体类型字面量**的机制代码，非未完成实现）；临时文件残留 **0** |

**0 回归全量**：9 个仓内闸全 **rc=0**；45 个 `scripts/test/cep_*.sh` 逐个 **rc=0**；`bash scripts/test/rddf_stage_gate.sh --self-test` → **rc=0**（3 项决定性变异 + 字节指纹守恒）。

## 三、失败项与路由建议

| # | 项 | 分类 | 建议路由 |
|:--:|---|---|---|
| **F-1** | AC-10 的 `format --check` rc=1（21 个 C++ 文件待格式化） | **既有债**（⛔ 非本单元引入 —— 本单元零 C++ 改动；`git diff` 中无任何 `src/`·`test/` 文件） | ⛔ **不归本单元**；建议**另立**仓级格式债单元（或由 `pre-commit` 的既有闸承接）⇒ 属本轮计划范围外，**登记为 backlog，不在本单元处理** |
| **F-2** | AC-6 判据自身与「载体表增第 5 列」矛盾 | **判据表述缺陷**（已在单元内**显式订正**并记录理由） | ✅ 已闭合（订正文本落 `cep-stage-gate-p5-hardening.md` AC-6）⇒ **需 planner 知悉**（判据归 planner 域） |
| **F-3** | 登记册 §6.4 队列 0 的 `P-*` 与 §6.6 的 `P-*` 同前缀异义 | **既有缺口**（per metis B8，已显式挂 **Q-6**） | ⛔ **不归本单元**；Q-6 收口时统一改名 |
| **F-4** | `--phase A` / `--phase B` 在三阶段全归档后仍 rc=1 | **既有行为**（实测 **HEAD 版同样 rc=1** —— 不是本单元引入） | ⛔ **不归本单元**；建议后续把 A/B/C 分支标为「历史规划，已完成」或直接移除（属 Q-6 收口范围） |

**判定**：**PASS_WITH_NOTES** —— AC-1 ~ AC-13 全部达成（F-1/F-3/F-4 为范围外既有债；F-2 已显式订正）。

## 四、遗留清扫结果

| 项 | 结果 |
|---|---|
| TODO / 半截实现 / 注释掉的代码 | **0**（13 处 grep 命中全为 `TODO` 作为**载体类型字面量**的机制代码 —— 是被实现的机制，不是未完成的意图） |
| 临时调试日志 / `diag` 文件 | **0** |
| 被 skip 的测试 | **0**（`--self-test` 是**主动的变异验证路径**，⛔ 按草稿 What Changes #6 **刻意不注册 ctest**） |
| 未替换的 mock / 临时 fallback | **0** |
| 临时文件残留 | **0**（`scripts/test/.gate_old_tmp.sh` 已清理；影子副本用 `mktemp -d` + `trap` 自动回收） |
| ⚠️ **超出本单元范围的改动** | **0**（本单元仅改 `scripts/test/rddf_stage_gate.sh` + `scripts/test/rddf_ac_carrier.tsv` + 1 处草稿 AC 判据订正） |

## 五、已用轮次

**1/3**（本轮为第 1 轮；本单元无失败项 ⇒ 无需重试）。
⚠️ **重试上限 3 轮**（per `RDDF_VERIFIER_MAX_LOOPS`）；若后续 F-2 的判据订正需再验 ⇒ 轮 2。

## 六、下一步建议

1. **切 builder 提交本单元**（2 个脚本文件 + 1 处判据订正）⇒ `git commit` 由工具层验证门放行。
2. **登记册 §6.4 队列 2 的 I-0 行**把「完成判据」更新为「已由 verifier 验收（报告：`.rddf/reviews/cep-stage-gate-p5-hardening-verify.md`）」⇒ 随后**把载体表内 I-0 行的 state 改为 `done`**（⚠️ 必须与验收证据同 commit ⇒ 否则「声明完成」与「实际完成」脱钩）。
3. **下一个可开工单元**：I-9（ArchForge 闸假绿，≈3 行）· I-1 / I-2 / I-3（纯文档债）· I-5（invariant 批 1）。
4. ⚠️ **F-1 / F-3 / F-4 建议登记为 backlog**（不在本单元处理）。
