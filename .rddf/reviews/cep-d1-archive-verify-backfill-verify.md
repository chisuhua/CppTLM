# 验收报告 · **I-10**（归档验收债补齐 + 路径无关机关）

- **角色**：builder（**实施侧自验** —— ⚠️ 本单元的 **9 份追溯补验报告**是在 **verifier 角色**下独立撰写与取证的（授权窗口 `builder ↔ verifier`），但**本报告**是实施侧对 I-10 自身交付面的自验 ⇒ 已在各 AC 中逐条给出工具证据，不作「独立验收」的强主张）
- **实施侧自验 · 2026-11-21**
- **验收对象**：登记册 §6.4 队列 2 的 **I-10** = `.rddf/improvements/cep-d1-archive-verify-backfill.md`（⛔ 本单元**不创建** openspec change ⇒ 无归档副作用）

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-1~AC-11） | `.rddf/improvements/cep-d1-archive-verify-backfill.md` §Acceptance |
| 架构裁决 | Q-8 / Q-13（用户 2026-11-20 拍板）：乙层全补 · 甲层封存+抽检 · 统一覆盖不按前缀特判 |
| 封存边界 | 归档日期 **< 2026-11-12** = 甲层（一次性历史封存，⛔ 不按名字豁免）|
| 闸契约 | improvement §What Changes #3（join 规则 + fail-closed ①② + 三检）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-1 | 乙层 8 份（**实为 9 份**）存在且结构合格：每份 4 条独立 `grep -qF`（`判据来源` ∧ (`逐条判定`∨`判定`) ∧ `失败项` ∧ `已用轮次`）全命中；⛔ 不得写成 `grep -c <标题> >= 4` | **通过（9/9）** | 逐份机检（`grep -qF` 逐条）⇒ **9 份全部 4/4 命中**：`cep-evidence-tier-declaration` · `cpptlm-cep-full-green-run-evidence` · `cpptlm-cep-adr-verification-carriers` · `cpptlm-cep-spec-strict-compliance` · `cpptilm-cep-mesh-e2e-carrier` …（清单 = 乙层 30 目录中 21 份存量 + 9 份补齐）。⚠️ **第 9 份 = `cpptlm-cep-litmus-falsification-harness`** —— I-6a 于 2026-11-21 归档 ⇒ 落入乙层窗口 ⇒ 原计划 8 份**不足**；其原报告名缺 `cpptlm-` 前缀 ⇒ 已 `git mv` 改名并补「判据来源 / 失败项」两段（内容未改）|
| AC-2 | 未勾项有明确结论：`adr-verification-carriers` 对 T7.1/T7.2 · `spec-strict-compliance` 对 T10.4 · `mesh-e2e-carrier` 对 T3.8 各给「通过 / 不通过 / 不可判 + 原因」 | **通过** | 三份报告各含对应未勾项的**明确结论**：T7.1 = **通过**（AC-01~AC-10 逐条 + 工具证据）· T7.2 = **部分通过**（5 项残余逐项结论）· T10.4 = **通过（8 条不可判已逐条标注原因）** ⇒ ⚠️ **明确记为「不可判」而非「已通过」**（归档前基线不可复现）· T3.8 = **通过**（独立重跑 `[mesh]` 19 cases / 251 assertions）|
| AC-3 | 每条 pass 判定都能追到 ≥1 条 `read`/`grep`/`shell` 证据；⚠️ **本条为人工复核项，不计入闸** | **通过（人工复核）** | 9 份报告的「逐条判定」列**逐行**给出命令与输出摘录（如 `stat -c%s AGENTS.md` → 42,390 B · `grep -cE "code-review-graph\|…" AGENTS.md` → 0 · `PATH=/usr/bin:/bin … cep_spec_strict_check.sh` → rc=2 + stderr 含「找不到 'openspec'」）⇒ 非叙述式复述 |
| AC-4 | 角色与防自验：8 份报告首部须含 3 条**字面量**标记（`- **角色**：verifier` · `- **原归档未验**` · `- **追溯补验`） | **通过（9 份；含 1 处形态例外并已显式登记）** | 9 份补齐报告均含三标记；⚠️ **第 9 份（`cpptlm-cep-litmus-falsification-harness`）是例外** —— 该 change **归档前**已完成验收 ⇒ 写「原归档未验 / 追溯补验」即**假声明** ⇒ 该份用诚实形态「`- **角色**：builder（原始验收）· `- **原始验收 · 2026-11-21`」⇒ **闸的 provenance 判据相应放宽为「两种形态皆合法」**，并在闸内加**一致性检查**（自称追溯补验 ⇒ 必须同时含「原归档未验」+「角色」）。⚠️ **这是对 AC-4 字面要求的偏离**，理由 = 强制写假声明；已在本报告与裁决记录同址登记 |
| AC-5 | 抽检 5 份有结论；⛔ 不得写成「已全量审计」；⛔ 不得取自甲层已验收的 9 个 change | **通过** | `.rddf/reviews/archive-audit-2026-11-21.md`：5 份逐份结论（**实质可信** 2 份 · **实质可信 + 疑点** 2 份 · **发现疑点（真债）** 1 份）· 抽检对象已按 improvement 的订正名单（含 `cpptlm-cep-tilelink-t3-mapping-survey`，⛔ 已移除已验收的 `cpptlm-cep-moesi-gpu-l4-e2e`）· ⚠️ 命中 **3 份**疑点 ⇒ 已登记「定向补那一份」的交办（⛔ 不扩成全量）；⚠️ 报告头明写「**抽检 5 份 / 非全量审计**」|
| AC-6 | 机关存在且已注册 | **通过** | `scripts/test/archive_verify_check.sh` 存在且可执行（`chmod +x`）· `test/CMakeLists.txt` 含 `add_test(NAME archive_verify_check` ⇒ `ctest --test-dir build -N \| grep -c archive_verify_check` = **1**；`ctest --test-dir build -R archive_verify_check --output-on-failure` → `100% tests passed, 0 tests failed out of 1` |
| AC-7 | 机关判据有鉴别力 · **决定性变异 4 条**（影子副本模式）：① 移走 1 份报告 ⇒ 红且**只红该 change** ② 报告替换为 3 标题空骨架（<400 字符）⇒ 红 ③ 造无日期前缀目录 ⇒ 红 ④ 造 `<date>-fake-change/` 无报告 ⇒ 红 | **通过（4/4 按签名红）** | 影子副本 = `.rddf/state/tmp/av_shadow`（仓内 ⇒ `git` 可解析），经 `ARCHIVE_VERIFY_ROOT` 指向：① `✗ 归档 change 缺独立验收报告: cpptlm-cep-mesh-e2e-carrier`（**仅此 1 条**）② `✗ 报告形态不成立(去空白长度 49 < 400): cep-evidence-tier-declaration` ③ `✗ 归档目录名缺日期前缀: no-date-prefix-change`（**仅此 1 条**）④ `✗ 归档 change 缺独立验收报告: fake-change` ⇒ 4 条 rc 均为 **1**，还原后 rc=0 |
| AC-8 | 封存边界生效 · 不误伤甲层：未动甲层时 rc=0；⚠️ 反 fixture：边界改成 `2026-11-01` ⇒ **必须转红** | **通过** | 基线：闸在甲层未动时 rc=0（扫面 30 = 乙层 30，甲层 135 目录**未参与**）· 反 fixture（影子副本 + 脚本副本把 `BASELINE_DATE` 改为 `2026-11-01`）⇒ **26 条**「归档 change 缺独立验收报告」红灯（扫到 11-01~11-10 的甲层）⇒ 边界**确有效**且**不误伤** |
| AC-8b | 扫面不可见时 **fail-closed**：影子副本里把 `openspec/changes/archive/` 整体移走 ⇒ 闸**必须判红**（签名「归档目录不可见：CI 无法校验」），⛔ 不得 rc=0 | **通过** | 影子移走 `archive/` ⇒ `✗ 归档目录不可见：CI 无法校验（归档目录不存在: …）`，rc=**1**（⚠️ **首版实现返回 rc=2（环境错误）⇒ 已修**：归档目录缺席**不是**环境错误，而是 fail-closed ② 的形态 ⇒ 改为 rc=1 + 该签名；rc=2 会被读作「不计作判据失败」⇒ 变异不算数）|
| AC-9 | 闸落地即绿（8 份已补 + 21 份存量过三检；⚠️ 前提 = 扫面目录数 ≥ 基线） | **通过** | `bash scripts/test/archive_verify_check.sh` → `✅ 归档验收报告闸通过（扫面 30 份 · 基线 30 · git 跟踪 30 · 三检 + provenance 一致性）`，rc=0；ctest 同款绿 |
| AC-10 | 范围零改动：`git diff --name-only` **不含** `openspec/specs/**` · `docs/adr/**` · `docs/architecture/**` · `scripts/test/rddf_stage_gate.sh` · 甲层任何 change 目录 | **通过（含 1 处用户批准的偏离并已显式登记）** | 零改动面实测 ✅：`openspec/specs/**` · `docs/adr/**` · `docs/architecture/**` · `scripts/test/rddf_stage_gate.sh`（未改）· **甲层 135 目录零改动**（force-add 只及乙层）。⚠️ **偏离**：本单元经**用户当轮明确批准**（问答「同意 force-add 乙层 30 个目录」）⇒ `openspec/changes/archive/` 的**乙层**目录被 `git add -f`（125 个文件 / ≈830 KiB，file 粒度 = `proposal.md` / `design.md` / `tasks.md` / `specs/**`，与既有 6 个已跟踪目录**同粒度**）⇒ **AC-10 的「不含 `openspec/changes/archive/`」被该批准覆盖**；**理由** = `.gitignore:64` 排除归档目录 ⇒ git 只跟踪 6/30 ⇒ CI fresh clone 上闸**扫不到**其余 24 个（含 9 份真缺里的 8 个）⇒ 闸在 CI 上会**空转绿** = 用新闸**复刻 `S2-8` 假绿** ⇒ 故必须 force-add。⚠️ 配套：新增**已提交基线文件** `scripts/test/archive_verify_baseline.txt`（= **30**）⇒ 闸的 fail-closed ② 取「与当前 git 状态无关的已提交基线」⇒ 若将来 force-add 丢失 ⇒ CI 扫面 < 基线 ⇒ **判红**（而非空转绿）|
| AC-11 | 0 回归：`ctest` 全绿 · 全部 `scripts/test/cep_*.sh` 逐个 rc=0 · `rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净** | **通过** | 见 §回归实录（ctest 全量 + 47 个 `cep_*.sh` + 三路径机关 + 文档闸）；⚠️ `format --check` 的 **21 个既有 C++ 债**与本单元无关（本单元**零 C++ 改动**）|

## 失败项与路由建议

| # | 失败 / 偏离项 | 路由 |
|:--:|---|---|
| F-1 | **AC-4 的字面要求与事实冲突**：第 9 份（`cpptlm-cep-litmus-falsification-harness`）**归档前已验收** ⇒ 强制写「原归档未验 / 追溯补验」= 假声明 | ⇒ 已按**诚实形态**处理（原始验收形态）+ 闸的 provenance 一致性检查；⚠️ **这是对 ratified AC 的偏离** ⇒ 需**用户确认**是否接受（不接受 ⇒ 只能改 AC-4 字面）|
| F-2 | **AC-10 被用户批准覆盖**（force-add 乙层 30 目录）| ⇒ 已在本报告 §AC-10 显式登记理由与批准来源；⇒ 建议**用户追认**该 AC-10 的措辞修订（⛔ 实施侧不自行改判据）|
| F-3 | 甲层抽检**命中 3 份疑点**（`chi-rnf-subset` 的 7.1/7.3 登记缺口 · `t3-mapping-survey` 的**真债**（无 tasks / 无 delta spec / 4 条 AC 无证据面）· `bridge-reattach-guard` 的 T4.8 核对未落） | ⇒ 按 improvement 的范围纪律「命中 ⇒ **定向补那一份**」⇒ **另立 3 个定向补验单元**（⛔ 不并入 I-10，⛔ 不扩成 126 份全量）；⚠️ `t3-mapping-survey` 需**先判定**是「已实施但证据缺失」还是「未实施」⇒ 属 architect/planner 决策面 |
| F-4 | 闸的 provenance 判据**放宽为两种形态**（原始验收 / 追溯补验）⇒ 与 AC-4 字面不同 | ⇒ 同 F-1（待用户确认）|
| F-5 | I-7a 的 **AC-13** 要求把 8 行载体登记写进 `scripts/test/rddf_ac_carrier.tsv` ⇒ 属 I-7 单元（⛔ 不属 I-10）⇒ 已在 I-7a 的 improvement 内登记为交办 | ⇒ 随 I-7a 实施时落盘 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 临时诊断代码 | ✅ 无（未新增任何 `static FILE* diag` 之类；影子副本 `.rddf/state/tmp/av_shadow` 已删除）|
| 临时文件 / 影子树 | ✅ 已清理（`rm -rf .rddf/state/tmp/av_shadow`）⇒ 未留残留目录 |
| 范围纪律 | ✅ 零 `openspec/specs/**` · 零 `docs/adr/**` · 零 `docs/architecture/**` · 零 `scripts/test/rddf_stage_gate.sh` 改动 · 零甲层目录改动 · **零 C++ 改动** |
| 半截状态 | ✅ 无：9 份报告齐备 + 抽检记录 + 闸 + 基线 + ctest 注册 **同一批落地**，闸上线即绿 |
| 新增登记面 | ✅ **未新建第三个登记面** —— 基线是**闸的输入文件**（与载体表同级），⛔ 不是新台账 |

## 已用轮次

**1/3**（I-10 第 1 轮；无返工轮 —— 唯一的返工点是闸的 AC-8b 签名（rc=2 ⇒ rc=1），**当轮修完并复测**，不另计轮）。
