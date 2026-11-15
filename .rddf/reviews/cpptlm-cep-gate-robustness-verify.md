# 验收报告：`cpptlm-cep-gate-robustness`

> **角色**: verifier（角色内建验收 · **独立于实施者**）
> **被验对象**: change `cpptlm-cep-gate-robustness`（台账 Entry #436 立项 / #437 收口）
> **归档目录**: `openspec/changes/archive/2026-11-15-cpptlm-cep-gate-robustness/`
> **验收日期**: 2026-11-15 · **轮次**: 第 1 轮
> **结论**: ✅ **PASS**（15/15 判据通过 · 0 不通过 · 0 未取证）

---

## 判据来源（按优先级）

| 优先级 | 来源 | 取用 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 「切 verifier 做 T8 独立验收」⇒ 目标 = 上一 change `gate-robustness` |
| 2 | `proposal.md` 的 `## 验收标准` 段 | **AC-01 ~ AC-15**（本报告的主判据） |
| 3 | 本次变更自己声明的行为 | commit `08a67003` / `28e795a7` / `5d7eb64f` 的承诺 |
| 4 | ADR 的 Verification 章 | 本 change 未新增 ADR ⇒ 不适用 |

**分流判定**（是否走 `rdd-verifier` skill）：① change 有 `## 验收标准` 段 ✓ · ② P3 archive gate 信号 —— `.rddf/state/iteration.json` **不存在**（本会话未跑 `rdd-builder` skill ⇒ 无状态机文件）✗ · ③ skill 可加载 ✓
⇒ **条件 ② 不满足 ⇒ 不走 skill**，走**本地验收**（5 步）。
**本地验收的额外门**（`iteration.json` 的 `tasks_done == tasks_total`）—— 该文件不存在 ⇒ **该门不适用**（无 iteration 状态可读）。
⚠️ 该文件不存在时**不得**据此判 pass —— 本报告的 pass 结论**完全由下方逐条工具证据支撑**，与 iteration 状态无关。

---

## 逐条判定（每条 ≥1 条工具证据）

| # | 判据（proposal 原文） | 判定 | 证据（工具调用 + 实际输出） |
|:--:|---|:--:|---|
| **AC-01** | 9 闸默认分支为 `mktemp -d`，无固定字面量残留 | ✅ | `grep -c 'mktemp -d' <9 闸>` 全部 **2**；`grep -rn 'SHADOW_ROOT:-/tmp/opencode' scripts/ test/` = **0**；`grep -l 'echo "\[shadow\] \$SHADOW"' scripts/test/cep_*.sh \| wc -l` = **9** |
| **AC-02** | 每闸输出 `[shadow] <path>`；同闸并行 2 实例 ⇒ 两 path 不同且两 rc 都为 0 | ✅ | 脚本 `/tmp/opencode/verify_ac02.sh` 对 **9/9** 闸并行跑双实例：**全部 rc=0/0 且两路径不同**（如 `cep-decision-block.EmWO2i` vs `cep-decision-block.1dJpOI`） |
| **AC-03** | 显式 `SHADOW_ROOT` 被使用，跑完**仍存在** | ✅ | `SHADOW_ROOT=/tmp/opencode/verify_ac03 bash …decision_block_check.sh` ⇒ rc=0、`[shadow] /tmp/opencode/verify_ac03`、`test -d` ⇒ **目录仍存在**（未删调用方目录） |
| **AC-04** | 默认路径跑完**无残留**（trap 生效） | ✅ | 跑完 `ls -d /tmp/opencode/cep-decision-block.* \| wc -l` = **0** |
| **AC-05** | 覆盖判据**开火**且归因唯一 | ✅ | `/tmp/opencode/verify_ac050607.py`：影子根加 `ADR-CEP-30-verify-probe.md`（`## 2` + `### 决策 1`）⇒ **rc=1** · `决策块覆盖缺失` 命中=**True** · 文件名命中=**True** |
| **AC-06** | 覆盖判据**不误报**（`## 决策` 无 `## 2`） | ✅ | 同脚本：加 `ADR-CEP-31-verify-probe.md` ⇒ **rc=0** · 覆盖判据命中次数 = **0** |
| **AC-07** | 真实仓一致且**可复算**；`check.py` rc=0 | ✅ | 同脚本实测：语料（含 §2 章）= **23** · 快照 keys = **23** · `语料\快照=[]` · `快照\语料=[]` · `check.py` rc=**0** |
| **AC-08** | `--list` 输出含新判据 | ✅ | `python3 …check.py --list` 第 **7** 行：`[R-d] 覆盖义务(语料 → 快照):有 §2 章的 ADR-CEP-*.md 必须登记(决策块覆盖缺失)` |
| **AC-09** | 契约自陈**不互斥**；含覆盖义务 bullet + 残余第 ④ 条；spec `--strict` rc=0 | ✅ | `openspec/specs/cep-invariant-gate/spec.md`：「不提供「覆盖完整性」判据」残留 = **0**；覆盖义务 3 处（`:30` 自陈 / `:44` bullet / `:123` 残余④）；`openspec validate cep-invariant-gate --type spec --strict` rc=**0** |
| **AC-10** | 「22 份」清零且**指代正确**（8 处逐一核对，排除 `__pycache__`） | ✅ | `grep -rn '22 份' --include=*.py --include=*.sh --include=*.md --include=*.txt scripts/ test/ CMakeLists.txt`（排除 `__pycache__`）= **0**；**指代正确性实测**：快照 keys = **23**（注释称 23 ✓）/ total = **131**（✓）· 磁盘 `ADR-CEP-*.md` = **24**（✓）· `decision_adrs` = **21**（✓，且 24 − 3 = 21 自洽）· `cep_freeze_check.sh` 改为「全量，不写数字」· `cep_docs_sync_check.sh:4` 改为「以数组实际为准」 |
| **AC-11** | `ADR_LIST` 条数 == **24**（== 磁盘全量）且含 `-20`/`-21`；`docs_sync --strict` rc=0 | ✅ | 实测 `ADR_LIST` 条数 = **24** · 含 `-20` = True · 含 `-21` = True · **集合 == 磁盘全量(24) = True**；`cep_docs_sync_check.sh --strict` rc=**0** 且打印「✓ **24 份** CEP ADR 全部存在」 |
| **AC-12** | 0 回归：45 闸全 rc=0（ctest 同参数）· `ctest -R cep` 条目数 == T0 · `[cep]` 用例数 == T0 | ✅ | T0 取自 `tasks.md:4`（`ctest -R cep`=**100** · 总=**180** · `[cep]`=**529 用例/12288 断言** · 闸=**45** · 冻结=**86**）。实测：45 闸全 rc=**0**（`docs_sync`/`doc_ref` 带 `--strict`）· `ctest -R cep` = **100/100 通过** · 全量 **180/180 通过** · `[cep]` = **529 用例 / 12288 断言** ⇒ **3 项 == T0** |
| **AC-13** | 范围零 diff（闸数 45 不变、新增 `INV-CEP-*` = 0） | ✅ | 基线 `2f530450`（本 change 前）→ HEAD：`include/`=**0** · `src/`=**0** · `test/*.cc`=**0** · `docs/adr/`=**0**；闸数基线 **45** = 当前 **45**；新增 `INV-CEP-*` = **0** |
| **AC-14** | 五步链（1 份冻结产物）：Entry → 产物 commit → `--update --authorized-by`（diff = 1 份 hash + 元数据；`artifact_count` 86 不变）→ 清单单独 commit → 无参复验 rc=0 | ✅ | ① `Entry #436` 存在（台账 grep = 1）② 产物 commit = `08a67003` ③ 清单 commit = `28e795a7`，其 diff = **1 file changed, 3 insertions(+), 3 deletions(-)**，内容 = **1 份 hash**（`cep-invariant-gate/spec.md`）+ `authorized_by`(Entry #434→**#436**) + `frozen_at_commit` ④ `artifact_count` 基线 **86** = 当前 **86** ⑤ `cep_freeze_check.sh` 无参 rc=**0**（「provenance 自检通过:86 个产物的 hash 与 commit 08a67003e9f3 的内容一致」） |
| **AC-15** | `manifest.frozen_at_commit` == 产物 commit sha | ✅ | `manifest.frozen_at_commit` = `08a67003e9f36f06f01df2f5088fb2ab21d8bfbd` == `git rev-parse 08a67003` ⇒ **相等 = True** |

**判定汇总**：**15/15 通过 · 0 不通过 · 0 未取证**。

---

## 验证顺序（早失败早停，未跳步）

| 步 | 内容 | 结果 |
|:--:|---|---|
| **1 · 静态检查** | 45 闸 `bash -n`（代表性 5 闸逐一执行全过）· `python3 -m py_compile`（`cep_decision_block_check.py` OK）· `agents_md_lint` rc=**0** · `openspec validate --changes --strict` rc=**0** | ✅ |
| **2 · 单元测试** | `[cep]` = **529 用例 / 12288 断言** 全绿 | ✅ |
| **3 · 集成 / 端到端** | `ctest -R cep` = **100/100**；全量 = **180/180**；45 闸逐个实跑 rc=0 | ✅ |
| **4 · 边界与错误路径** | ① `SHADOW_ROOT` 指向**不存在**路径 ⇒ **rc=0**（闸自建目录，行为自愈，无崩溃）② `check.py --root` 指向**空目录** ⇒ **rc=2** + 明确报错「文档语料根缺少 docs/adr」⇒ **可控失败，无未捕获异常（无 Traceback）** | ✅ |
| **5 · 遗留清扫复核** | 见下 | ✅ |

### 第 5 步 · 遗留清扫（本次范围）

| 类别 | 检查 | 结果 |
|---|---|---|
| **TODO / 半截实现** | 13 个被触碰脚本 `grep -nE '\b(TODO\|FIXME\|HACK)'` | **0 残留** |
| **注释掉的代码 / 临时调试日志** | `printf` / `echo "DEBUG"` / `static FILE* diag` / `print("DEBUG"` | 命中 **1** 处 = `cep_decision_block_check.sh:299` 的 `printf '# ADR-CEP-02a: 变异体…'` ⇒ **非遗留**：`git log -S` 追溯 = `cba5e31c`（**本 change 之前**引入）⇒ 属 R-c 变异② 的**影子树变异体构造器**（闸的机制，非调试日志） |
| **被 skip 的测试 / `.disabled`** | `test/CMakeLists.txt` + 闸脚本 | **0 残留** |
| **未替换的 mock** | 本 change 零 mock 触碰（闸脚本自建影子树，非 mock 框架） | **N/A** |
| **临时 fallback** | 逐条登记：① `${TMPDIR:-/tmp/opencode}` 兜底（CI 无该目录时退到系统 `/tmp`）② `mkdir -p … \|\| true`（base 不可写时不阻断，mktemp 报错间接失败）③ `_own_shadow` 守卫（仅清理自建目录） | **3 项已登记，均为设计内兜底**（critic 第 4 节已确认第 ② 项为「装饰性」但可接受） |
| **资源释放** | `grep -l 'trap .*EXIT'`（9 闸 + `cep_freeze_reverse_verify`）= **10** ⇒ trap 覆盖全部；AC-04 实测默认路径残留 **0** | ✅ |

---

## 失败项与路由建议

**无失败项** ⇒ 无需路由（`implementation_gap` / `proposal_drift` 分类不适用）。

---

## 本次验收的**发现**（不构成失败，但须知会）

| # | 发现 | 性质 | 建议 |
|:--:|---|---|---|
| **F-1** | **`--list` 路径的 trap 泄漏已被实施方在 critic NIT 阶段修掉**（3 闸把 `--list` 分支前置到创建影子目录之前）—— 本验收顺带实测：6 次 `--list` 后残留 **0** | 已闭合 | 记录即可 |
| **F-2** | `check_coverage` 的**语料 → 快照**方向依赖 `section2(strip_fences(...))` 谓词；实测 AC-05/AC-06 双向成立（开火 + 不误报）⇒ **有牙且不越界** | 已验证 | 无 |
| **F-3** | 本 change 之后**又发生了一个 change**（`cpptlm-spec-scenario-level-fix`，Entry #438/#439）⇒ 其改了 **2 份非 `cep-*` 主 spec**，**不在** `cep-invariant-gate` ⇒ **对本 change 的判据无影响**（AC-09 实测仍通过） | 无影响 | 无 |
| **F-4** | `SHADOW_ROOT` 指向**不存在**路径时闸**自建**该目录（rc=0）⇒ 调用方若误传路径会得到一个**被创建**的目录（由调用方负责清理） | 设计内（`cep_shadow_reset` 的既有语义） | 若将来要求「不得创建调用方指定路径」，需另立判据 |
| **F-5** | `grep` 取证须排除 `scripts/test/__pycache__/*.pyc`（内含旧数字「22 份」）⇒ 本报告 AC-10 已用 `--include=*.py --include=*.sh --include=*.md --include=*.txt` 限定 | 取证方法 | 已在本报告留痕 |
| **F-6** | ⚠️ **change 的 `tasks.md` 与事实不同步**：归档后的 `tasks.md` 中 **T7.4 / T7.5 / T8.1 / T8.2 仍是未勾选**，但这 4 项**实际已完成**（AC-14 实测五步链完整、`artifact_count` 86 不变、KPI 已回 0/3、台账 Entry #437 在位）⇒ 根因 = 归档**之后**才执行这 4 步，**未回勾** | **记录债**（不影响任何判据的成立 —— AC-14/AC-15 已独立取证通过） | ⛔ **verifier 不改实施方产物**（`openspec/changes/**`）⇒ **交回 builder** 补勾 4 项（纯勾选，零内容改动） |
| **F-7** | 本 change 之后**又发生 3 个 change**（`cpptlm-spec-strict-compliance` / `cpptlm-spec-scenario-level-fix` 等）⇒ 其中 `spec-scenario-level-fix` 改了 **2 份非 `cep-*` 主 spec**，**不在** `cep-invariant-gate` ⇒ 对本 change 判据**无影响**（AC-09 实测仍通过） | 无影响 | 无 |

---

## 已用轮次与后续

- **已用轮次**：**1**（上限 3，与 `RDDF_VERIFIER_MAX_LOOPS=3` 对齐）。
- **结论**：**PASS** —— change `cpptlm-cep-gate-robustness` 的 **15/15** 判据均取得工具证据并通过；**T8（验收债）关闭**。
- **⛔ 本验收不改实现**：全程只读（`read` / `grep` / `shell` 取证），未修改任何实现文件。
- **后续**：① 剩余 T8 之外的未验收 change（本会话共 3 个：`flow-support-assertions` / `adr-verification-carriers` / `spec-strict-compliance` / `spec-scenario-level-fix` 中除本 change 外者，可另起验收轮）② 轮 2 的 R-1（75 份 Purpose + 70 份正文）③ 轮 3 的 architect 裁决（R-9 canonical / R-2 golden）。

**关联**：台账 Entry **#436**（立项）· **#437**（收口）· **#438**（全量梳理）· **#439**（R-7 收口）· `.rddf/improvements/cpptlm-cep-gate-robustness.md` · `openspec/changes/archive/2026-11-15-cpptlm-cep-gate-robustness/proposal.md`（`## 验收标准` 段 = 本报告判据来源）
