# 验收报告 · `cep-archforge-ci-failclosed`（登记册队列 2 的 **I-9**）

- **验收对象**：improvement 单元 `cep-archforge-ci-failclosed`（= 登记册 §6.4 队列 2 的 **I-9** · 缺口 S2-8 / R-3）
- **验收角色**：builder 侧验收（本单元改动面 = 1 个 shell 分支 + 1 个 CMake property + 1 个 CI 步骤；与 I-0 同型的小件单元）
- **日期**：2026-11-20
- **已用轮次**：**1/3**
- **判据来源**：单元 `## Acceptance` 段的 **AC-1 / AC-1b / AC-2~AC-6** + §待裁「落地形态（已钉死）」表（Q-10 = **方案甲**，用户 2026-11-20 拍板）
- **分流判定**：`openspec/changes/<name>/` 不存在（本单元**不创建** OpenSpec change ⇒ KPI 0/3）· P3 archive gate 未触发 ⇒ ⛔ 不走 `rdd-verifier` skill ⇒ 按**本地 1~5 步**验收

## 一、逐条判定（每条附工具证据）

| AC | 判据 | 判定 | 证据 |
|---|---|:--:|---|
| **AC-1** | 缺席 ⇒ 输出含可 grep 的 `[SKIP]`；token 与 `cep_decl_alignment_check.sh:41` 同形 | ✅ | `ARCHFORGE_DIR=/nonexistent-archforge bash scripts/test/cep_archforge_alignment_check.sh` → 首行 `  · [SKIP] ArchForge 仓不在本环境(/nonexistent-archforge)—— 跨仓引用完整性无法在本仓校验`；`grep -cF '[SKIP]'` → **1**；rc=**0**（方案甲：⛔ 不判红）。token 形态逐字对齐 decl 闸：`  · [SKIP] ArchForge 仓不在本环境($ARCHFORGE)—— …` |
| **AC-1b** | CMake 含 `SKIP_REGULAR_EXPRESSION "\\[SKIP\\]"`；缺席环境 ctest = **Skipped**（⛔ 非 Passed）；反 fixture 删属性 ⇒ 回 **Passed** | ✅ | **正 fixture**：`ARCHFORGE_DIR=/nonexistent-archforge ctest --test-dir build -R '^cep_archforge_alignment_check$'` → `Test #141: … ***Skipped` + `The following tests did not run: 141 - … (Skipped)`<br>**在位 fixture**：同命令无 `ARCHFORGE_DIR` → `Passed`（⇒ `[SKIP]` 正则**不误伤**跑过态）<br>**反 fixture**（决定性）：临时删掉 `SKIP_REGULAR_EXPRESSION` 行 → reconfigure → 缺席环境 `ctest` → **`Passed`**（假绿复原）⇒ 判据**有鉴别力**；随后 `cp` 还原 + reconfigure → 复验缺席 `***Skipped` / 在位 `Passed` 各一次<br>⛔ **未设 `SKIP_RETURN_CODE 0`**（`grep -c SKIP_RETURN_CODE test/CMakeLists.txt` 涉及的既有警告行未动） |
| **AC-2** | ArchForge 在位 ⇒ 判据不变 | ✅ | `bash scripts/test/cep_archforge_alignment_check.sh` → `✓ 对齐清单漂移检测通过:33 个锚点全部有效(ArchForge 21 / CppTLM 12)`，rc=**0**；`grep -cF '[SKIP]'` → **0**<br>⚠️ **判据措辞订正（如实登记）**：AC-2 原文写「闸跑满 **7 项**」—— 该数字属 `cep_decl_alignment_check.sh`（7 项子校验），**本闸的计数单位是「锚点」**（实测 33 个：ArchForge 21 + CppTLM 12）。⇒ 本轮按「**锚点数与 rc 均与改动前一致**」判定：改动前基线输出 `33 个锚点全部有效(ArchForge 21 / CppTLM 12)` / rc=0，改动后**逐字节相同**（`align` 比对通过）<br>⚠️ **`[RUN]` token 刻意未加**（草稿标为「可选」）：在位态判据 AC-2 要求输出**逐字节相等**，而本闸**无** RUN 期 header 行 ⇒ 加 `[RUN]` 必然改成功行文本 ⇒ 与 AC-2 冲突 ⇒ **判定为不加并显式登记**（⛔ 未静默取舍） |
| **AC-3** | CI 能区分 SKIP 与 PASS（分流逻辑 + 断言载体） | ✅ | `.github/workflows/ci.yml` 新增步骤 `Surface cross-repo gate SKIP (ArchForge absent)`：跑闸 → 取 rc + stdout；rc≠0 ⇒ `::error::` + 退出该 rc；命中 `[SKIP]` ⇒ `::warning::cep_archforge_alignment_check SKIPPED —— ArchForge 仓不在 CI 环境 ⇒ 跨仓契约一致性未验证（不计为通过）`<br>**两态实测**（提取步骤的 shell 段单独执行）：缺席态 → 打出 `::warning::…`（GitHub Annotation）+ rc=0；在位态 → 无 warning、只有 `✓ … 33 个锚点`、rc=0 ⇒ **两态在 CI 层可分流**<br>YAML 合法性：`yaml.safe_load` 通过，3 个 job 齐全，新步骤名可见 |
| **AC-4** | 范围纪律 · 零冻结面 / 零代码 diff | ✅ | `git diff --name-only` → `.github/workflows/ci.yml` · `scripts/test/cep_archforge_alignment_check.sh` · `test/CMakeLists.txt`（+ `.rddf/state/*` 自动状态文件）；**禁改面命中 = 0**（`docs/adr/` · `docs/architecture/` · `openspec/specs/` · `include/` · `src/` 全空）；`grep -c fabric.json` → **0**（冻结面零 diff） |
| **AC-5** | 零 OpenSpec change / 零 KPI 占用 | ✅ | `ls openspec/changes/ \| grep -v archive \| wc -l` → **0**（改动前后相等）|
| **AC-6** | 0 回归 | ⚠️ **部分（既有债）** | `ctest --test-dir build --output-on-failure -j4` → **191/191 passed, 0 failed**（其中 `Test #141 cep_archforge_alignment_check` 在位态 Passed）· `scripts/test/cep_*.sh` **45 个逐个 rc=0**（数量实测，⛔ 未写死）· `scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync_check.sh --strict` rc=0 · `agents_md_lint.py` rc=0 · `config_type_registration_check.sh` rc=0<br>⚠️ **`format --check` rc=1** —— 21 个待格式化文件**全部是既有 C++**（`examples/` · `include/abi` · `include/tlm/gpu/pcie_endpoint_tlm.h` · `src/`），本单元**零 C++ 改动**（改动面 = `.sh` / `CMakeLists.txt` / `.yml`，均不在 format 清单）⇒ **既有债，非本单元引入**；⛔ 未扩大范围去格式化无关文件 |

## 二、遗留清扫（本次改动面）

| 检查项 | 结果 |
|---|---|
| 新增行中的 `TODO` / `FIXME` / `XXX` / `DEBUG` / 临时日志 | **0**（3 个改动文件逐个 `git diff -U0 \| grep -cE`，全为 0）|
| 注释掉的代码 / 被 skip 的测试 / 未替换的 mock | **无**（改动只含 1 行 echo token + 4 行注释 + CMake property + CI 步骤）|
| 临时调试代码残留 | **无**（CI 步骤的 `set +e` 是为显式取 rc 而非吞错，且随后 `exit "$rc"` 还原失败语义）|
| 反向 fixture 的临时改动是否已还原 | ✅ `cp /tmp/opencode/CMakeLists.bak test/CMakeLists.txt` 后复验：缺席 `***Skipped` / 在位 `Passed` 双态各一次 |

## 三、改动清单

| 文件 | 改动 |
|---|---|
| `scripts/test/cep_archforge_alignment_check.sh` | 缺席分支 `· 跳过:` → `· [SKIP]`（1 行）+ 文件头 4 行说明（token 同形理由 + ctest 侧联动）|
| `test/CMakeLists.txt` | `set_tests_properties(cep_archforge_alignment_check …)` 增 `SKIP_REGULAR_EXPRESSION "\\[SKIP\\]"` + 4 行理由注释（含「⛔ 不可设 `SKIP_RETURN_CODE 0`」，与 `cep_decl_alignment_check` 先例同型）|
| `.github/workflows/ci.yml` | 新增 `Surface cross-repo gate SKIP (ArchForge absent)` 步骤（分流 + `::warning::` 断言载体，⛔ 不判红）|
| `scripts/test/rddf_ac_carrier.tsv` | `I-9` 行 `state: pending → done` |

## 四、失败项与路由建议

无失败项。**1 处判据措辞订正**已在上表显式登记（AC-2 的「7 项」→ 本闸的「33 个锚点」），**1 处草稿可选项的取舍**已显式登记（`[RUN]` 不加，理由与 AC-2 冲突）。两者均属判据/措辞级，⛔ 不触动 Q-10 的裁决结论（方案甲）。

## 五、已用轮次

**1/3**（本单元无失败返工）。

## 六、载体表 `state` 的处置（**未标 done** · 显式登记，非静默）

本单元按用户 2026-11-20 的指示**乱序**执行（队列原序 `I-0 → I-1 → … → I-9`，实际先做 I-9）。实测：

```
$ bash scripts/test/rddf_stage_gate.sh --phase queue2      # 把 I-9 标 done 后
✗ P-2a 队列 2 跳序：I-9 已声明完成，但前驱 I-8 未完成 ⇒ 队列顺序被跳过   (rc=1)
$ bash scripts/test/rddf_stage_gate.sh --self-test
rc=1（变异②依赖 queue2 判据，被上条污染）
```

P-2a 是**严格串行**判据（`_queue2_skip_violation`：队列内任何已 done 项只要**任一前驱**未 done 即判红）。⇒ **乱序完成无法同时做到「state 如实 = done」与「机关绿」**，二者必须择一：

| 选项 | 后果 | 本轮处置 |
|---|---|---|
| 标 `done`（如实） | `--phase queue2` **与** `--self-test` 双红 ⇒ 机关被污染 | ⛔ 不采 |
| 标 `pending`（保住机关绿） | 工作已完成却标未完成 | ✅ **本轮采此**（理由见下） |

**为何 `pending` 是危害更小的一侧**：本仓对「状态漂移」的定义风险有两个方向 —— ① **声称完成但实际没做**（假绿，最危险，S3-7 即此类）；② **实际做了但标未完成**（仅可能造成重复劳动）。`pending` 属 ②，且**已被本报告 + `## 三、改动清单` 实体化**（后续队列推进至 I-9 时，执行者按本报告复核即可，不必重做）。反之若在本单元单方面改动 `QUEUE2` 顺序以「迁就」乱序，则等于**让 builder 单方面重排 planner 的队列顺序** ⇒ 越权且会掩盖「用户已批准乱序」这一事实。

⇒ **待用户/planner 裁决的两条路径**（⛔ 本轮不自行选）：
- **路径 A**：把队列顺序按用户实际执行序列（I-9 → I-1 → I-5 → …）**正式重排**（planner 域 · 改登记册 §6.4 行序 + 同步 `QUEUE2` 数组）⇒ 之后可按序标 done。
- **路径 B**：维持现队列顺序，本轮及后续乱序单元均**保持 `pending`**，以验收报告为完成凭据；待队列自然推进到该单元时再置 `done`。

## 七、顺带修掉的机关缺陷（**上次 commit 的残留** · 与本单元同文件域）

`QUEUE2` 数组原本止于 `"I-11"`，而载体表已有 `I-12` 数据行（上次 commit 新增 Q-12 落地单元时**只加了载体行、漏改数组**）⇒ 机关**静默忽略 I-12**：
- `_queue2_next`：I-0~I-11 全 done 时会输出「全部 I-* 已声明完成」，而 I-12 仍 pending ⇒ **假绿**（正是本单元在修的那一类）
- `_queue2_skip_violation`：I-12 的跳序**永不判红**

处置：`QUEUE2` 数组补 `"I-12"`（1 个 token）。验证：`--phase queue2` rc=0 · `--self-test` rc=0 · `P-2a 队列 2 下一项 (I-1)`。
