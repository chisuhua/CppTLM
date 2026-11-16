# 验收报告：`cpptlm-spec-purpose-batch3`

> **角色**: verifier（角色内建验收 · **独立于实施者**）
> **被验对象**: change `cpptlm-spec-purpose-batch3`（台账 Entry #444 立项 / #445 收口）
> **归档目录**: `openspec/changes/archive/2026-11-16-cpptlm-spec-purpose-batch3/`
> **验收日期**: 2026-11-16 · **轮次**: 第 1 轮 · **产物 commit**: `73a3af5e`（归档）· 基线 `f1cb3cb8`
> **结论**: ✅ **PASS**（10/10 判据通过 · 0 不通过 · **0 未取证**）

---

## 判据来源

| 优先级 | 来源 | 取用 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 「对CEP领域相关的完成和后续任务进行梳理……再切builder进行实施」⇒ 步骤 5 = 批 3 的独立验收 |
| 2 | `proposal.md` 的 `## 验收标准` 段 | **AC-01 ~ AC-10** + **AC-02c**（本报告主判据） |
| 3 | 本次变更自己声明的行为 | commit `73a3af5e` + 归档目录内容 + 归因载体 |
| 4 | ADR 的 Verification 章 | 本 change 未新增 ADR ⇒ 不适用 |

**分流判定**（是否走 `rdd-verifier` skill）：① change 含 `## 验收标准` 段 ✓ · ② **P3 archive gate 信号缺失**（`.rddf/state/iteration.json` **不存在** —— 本会话从未跑 `rdd-builder` skill ⇒ 无状态机文件）✗ · ③ skill 可加载 ✓
⇒ **条件 ② 不满足 ⇒ 走本地验收**（5 步）。⚠️ 本地验收的额外门（`iteration.json` 的 `tasks_done == tasks_total`）在**该文件不存在时无法适用** ⇒ **本报告的 pass 结论完全由逐条工具证据支撑**。

---

## 独立性原则（本轮的核心方法）

被验 change **自带**校验脚本 `scripts/test/cep_spec_purpose_batch3_check.py` ⇒ 用它验自己 = 自证。
⇒ 本验收**另写一份独立实现**（`/tmp/opencode/verify_batch3.py`，**不同代码路径 / 不同判定口径**）：

| 被验脚本的做法 | 本验收的独立做法 |
|---|---|
| `lcs_len` = 滚动 DP | `lcs_sub` = 窗口 + `in` 判定（逐字符 brute） |
| `lcs_word_seq` | `lcs_words` = 独立重写的窗口实现 |
| `norm` = 正则剥标点 | `PUNCT` = 反向白名单（仅保留字母数字 CJK） |
| `repo_has_symbol` = `grep -rIl`（子串） | `grep -rlw`（**词边界精确匹配**） |
| 归因闭合读载体 | 独立重算 `isfile` / `git show` / 核心名命中 |
| diff 形状读载体 | 独立 `git diff --numstat` + **逐份 `## Requirements` 段字节比对** |
| pre/post 读载体 | **独立构造基线影子根**（`git archive`）并**实跑 openspec** 重算 pre/post |

⇒ **两套实现结论一致才算证据**。
**被验脚本本身的判别力另行验证**：落地**前**跑 ⇒ **0/14 绿**（占位残留被 C-1 抓到）⇒ 有牙。

---

## 逐条判定

| # | 判据 | 判定 | 证据 |
|:--:|---|:--:|---|
| **AC-01** | 14 份 Purpose 限段零占位 | ✅ | 独立实现逐份限段扫 `TBD\|TODO` ⇒ 命中 **0** 份 |
| **AC-02** | 定向：14/14 无 Purpose 类 issue | ✅ | 独立跑 `openspec validate <spec> --type spec --strict --json` ⇒ 14 份中含 `placeholder` 类 issue 的 = **0** |
| **AC-02b** | 全局：`pre − post = k` 且逐份归因闭合 | ✅ | **独立构造基线影子根**（`git archive f1cb3cb8 openspec`）并**实跑 openspec** 重算 ⇒ **pre 65 / post 55 / k = 10**；载体声明（pre 65 / post 55 / k 10）与独立复算**一致**；载体 6 条机判经独立复核 = **True** |
| **AC-02c** | 全局闭合载体（`test/spec_purpose_batch3_reconcile.json`） | ✅ | 载体存在 · `fixed == pre−post` · `post.red ∪ fixed == pre.red` · 两集**不相交** · `k == len(pre)−len(post)` · `fixed` 的 **pre** issues ⊆ 仅 Purpose 类 · `remaining[].reasons` 是真实 issue code —— **6 条全部独立复核通过** |
| **AC-03** | 四条正交（逐份机判） | ✅ | **独立实现**复算 14 份：`50 ≤ len ≤ 160` 全满足 · 全单物理行 · CJK-only LCS 全 < 阈值 · 英文词序列全 < 5 词 · **15 类禁式**（过程叙述 / 技术字面量 / change 引用 / 日期 / 摘要信号）全 0 命中 · **C-10**（载体 `purpose_text` == spec 现行 Purpose）一致 ⇒ 问题 **0** 项 |
| **AC-04** | 零语义改动 + diff 形状 | ✅ | `git diff --numstat f1cb3cb8 73a3af5e -- openspec/specs/` ⇒ **14 文件** · 增行数非 1 的 = **0**；**14 份的 `## Requirements` 段与基线逐字节比对 ⇒ 变动 0 份**（⚠️ 1 份删 2 行系原占位后带尾随空行，属 Purpose 段尾部，**不触及 Requirements**） |
| **AC-05** | 不声称不存在的能力 | ✅ | 独立复算 14 份：① **C-6** spec 级 superseded 互斥（独立实现：前 10 行 + `## Status` 段 + 11 项词表 + IGNORECASE）② **C-7** 证据**可复现**（`isfile` 或 `git show <fixed-sha>:<path>`）**且**核心名命中前 2000 字符（12 份 archived + 2 份 git-history）③ **C-8** 锚点用 **`grep -rlw`（词边界）** 独立重算 ⇒ 14 份全部命中 ⇒ 问题 **0** |
| **AC-06** | `cep-*` 不受影响 | ✅ | 43 份 `cep-*` 逐份 `--strict` ⇒ rc≠0 = **0**；`artifact_count` = **86**（零冻结产物 ⇒ 免五步链） |
| **AC-07** | 范围零 diff | ✅ | `git diff --name-only <base> 73a3af5e -- include/ src/ 'test/*.cc' docs/adr/ 'openspec/specs/cep-*/'` ⇒ **0** 个文件 |
| **AC-08** | 闸 0 回归 | ✅ | **45 闸**逐个实跑全 rc=0（`docs_sync` / `doc_ref` 带 `--strict`）· 闸数（磁盘）= **45** 不变 · `ctest -R cep` **100/100** · 全量 `ctest` **180/180** · `[cep]` **529 用例 / 12288 断言** · 被验校验脚本 **14/14** |
| **AC-09** | KPI + 体量 | ✅ | `openspec list --json` ⇒ active = **0** · 归档 **change 目录数** = **151**（⚠️ 只数目录，`listdir` 会把 `STATUS_AUDIT_*.md` 也计入）· `docs_sync --strict` rc=0 |
| **AC-10** | 归档语义可判别 | ✅ | 归档 proposal 顶部「✅ **已完成归档 · 主 spec 直改（无 delta）**」标记行在位 · 归因 JSON `archive_semantics == "completed-direct-main-spec"` · 归档目录**无 `specs/`** |

**判定汇总**：**10/10 通过 · 0 不通过 · 0 未取证**。

---

## 验证顺序（早失败早停，未跳步）

| 步 | 内容 | 结果 |
|:--:|---|---|
| **1 · 静态检查** | `py_compile`（2 个脚本）· `openspec validate --changes --strict` rc=0 · `agents_md_lint` rc=0 | ✅ |
| **2 · 单元测试** | `[cep]` **529 用例 / 12288 断言** 全绿 | ✅ |
| **3 · 集成 / 端到端** | `ctest -R cep` **100/100** · 全量 **180/180** · 45 闸逐个实跑 rc=0 · 被验校验脚本 **14/14** · `openspec validate --changes --strict` rc=0 | ✅ |
| **4 · 边界与错误路径** | ① **落地前**跑校验 ⇒ **0/14**（占位被 C-1 抓到 ⇒ 判据有牙）② **判据假阳性演练**：C-3 度量被自己的判据打了一次（初稿「含标识符 LCS<12」被合法产品名 `PcieEndpointIP` 触发 ⇒ 拆为 CJK-only + 英文词序列）③ **判据漏判演练**：C-6 大小写漏洞（critic SHOULD_FIX-1）⇒ 已加 `IGNORECASE` 并实测 14 份状态位置仍无命中 ④ **C-8 的排除力实证**：3 份来源 change 未实施 ⇒ 锚点全不命中 ⇒ 被判据自动移出（**不是**靠人工挑） | ✅ |
| **5 · 遗留清扫复核** | 见下 | ✅ |

### 第 5 步 · 遗留清扫（本次范围）

| 类别 | 结果 |
|---|---|
| **TODO / 半截实现** | 本次触碰的 4 类文件（2 脚本 + 2 载体 + 14 份 spec）内 `TODO/FIXME/HACK` = **0** |
| **死代码** | 本轮新写的 2 个脚本**无死代码**（`lcs_len` / `lcs_word_seq` / `cjk_only` / `words_of` 均被调用；`__init__.py` 无未用导入） |
| **被 skip 的测试 / `.disabled`** | `test/CMakeLists.txt` 内 `mayfail` / `[!skip]` / `.disabled` = **0** |
| **未替换的 mock** | N/A（本 change 无 mock 面） |
| **临时 fallback** | 校验器的 `DOMAIN_TOKENS`（领域缩写白名单）为 **C-6 兜底** ⇒ 已登记在 design §D-3 并在脚本内注释说明用途 |
| **资源释放** | 校验器的 `--reconcile` 路径用 `tempfile.mkdtemp` 造影子根 ⇒ **已 `shutil.rmtree` 回收**（实测：影子根在 `fixed_pure` 计算后、载体落盘前删除，避免提前删导致 pre 复算失效） |
| **跨批遗留** | 本 change 新登记 **R-15 ~ R-20**（6 条）⇒ 均为**范围外既有缺口**，登记不回改 |

---

## 本次验收的**发现**

| # | 发现 | 性质 | 处置 |
|:--:|---|---|---|
| **F-1** | ⚠️ **被验脚本 `--reconcile` 的基线取 `HEAD` 会假失效** —— 落地已 commit 后 `HEAD` 即 post 态 ⇒ 重跑得 `pre=55 / k=0`（看起来像闭合失效） | **被验脚本的缺陷**（critic 验证建议指出，builder 已修） | 已修为**基线显式化**（`--base <sha>` > 载体 `base_commit` > `HEAD`）⇒ verifier 用 `--base f1cb3cb8` 实跑 ⇒ pre 65 / post 55 / k 10 |
| **F-2** | ⚠️ **AC-04 的「删 1 增 1」是过严代理** —— 实测 1 份删 2（原占位后带尾随空行） | **判据表述缺陷**（非强度放宽） | 已改为「增 1 行 + 删 1 或 2 行」；**实质判据「`## Requirements` 段逐字节不变」实测 14/14 变动 0**（critic 复核确认是**结构性保证**：替换正则保留 `\n## Requirements` 起的一切字节）⇒ 判据**未放宽** |
| **F-3** | ⚠️ **C-7 核心名曾误用带日期前缀的归档目录名** ⇒ 12 份全判红 | **被验脚本的缺陷**（builder 自查时抓到） | 已修为「核心名 = **去日期前缀**」+ 另存 `source_archive_dir`；本验收用**独立实现**复算 ⇒ 14/14 通过 |
| **F-4** | ⚠️ **C-6 大小写漏洞** —— `re.search` 大小写敏感 ⇒ 状态位写小写 `deprecated` 会漏判真 superseded | **被验脚本的缺陷**（critic SHOULD_FIX-1） | 已加 `re.IGNORECASE`；⚠️ 词表**故意不含** `SUPERSEDE`（不带 d）—— `| **supersede** | 原 X |` 的语义是「**本 spec 取代了 X**」⇒ 命中它 = 假红。verifier 实测 14 份状态位置仍无命中 |
| **F-5** | ⚠️ **C-9 与 design D-3 / AC-02c 三处不一致** —— `fixed_pure` 算了却未纳入 mc · 查 post 而非 pre · `k_equals_len_fixed` 硬编码恒真 | **被验脚本的缺陷**（critic SHOULD_FIX-2） | 已修；verifier **独立复核载体 6 条机判** ⇒ 全 True |
| **F-6** | ⚠️ **实施判定不能用 tasks 勾选** —— 批 1 的 F-6 已证「归档后执行未回勾」⇒ tasks 是记录债；实测本批 **8 份「0 勾选」中 6 份实现锚点全部命中** | **方法论要点** | 本验收用 `grep -rlw`（**词边界精确匹配**，比被验脚本的 `-rIl` 子串匹配更严）独立复算 C-8 ⇒ 14/14 命中 |
| **F-7** | ⚠️ **`repo_has_symbol` 递归扫 `test/` 且含 `*.py`**，比 design §D-3 文档写的 `test/*.cc` **更宽** | 文档/实现不一致（critic NIT-2） | 已收窄到与文档一致；本验收独立实现本就只用 `grep -rlw`（无 `--include` 过滤）⇒ 结论不受影响 |
| **F-8** | ⚠️ **R-20**：`dgpu-board-adapter-info/spec.md:10,21` 的 Scenario 仍引用**已删除**的 `cpptlm_emulator_get_adapter_info` | **既有缺陷**（critic `scope_exceeded: true`） | 登记**不回改**（属 Requirements 正文 staleness ⇒ 批 2 或 owner 处理）；**本 change 的 Purpose 已回避该符号**（实测提及 **0** 次）⇒ 未把假事实写进契约 |

---

## 失败项与路由建议

**无失败项** ⇒ 无需路由（`implementation_gap` / `proposal_drift` 分类不适用）。

---

## 已用轮次与后续

- **已用轮次**：**1**（上限 3，与 `RDDF_VERIFIER_MAX_LOOPS=3` 对齐）。
- **结论**：**PASS** —— change `cpptlm-spec-purpose-batch3` 的 **10/10** 判据均取得工具证据并通过。
- **⛔ 本验收不改实现**：全程只读取证（`read` / `grep` / `shell` / `git`），未修改任何实现或规范文件。
- **后续**（台账 Entry #446 + Entry #444 §3 的实施路径）：
  ① **R-1 批 4~6**（其余 **45** 份 Purpose —— 复用本批的生成器 + 校验器 + 归因载体范式；⚠️ 判据 **C-1~C-10 一经落地不得改语义，只能追加** —— 判据每批复制一次，漏洞也会被复制）
  ② **R-1 批 2**（**26** 份有「正文 >500」issue —— 裁剪**会动 `## Requirements` 段 ⇒ 另一档风险**，须另立 change + architect 参与）
  ③ **architect 裁决批**：**B-1 后 2 条** invariant（schema 13→14）· **B-2** 19 条 ADR 归因收敛（其中 10 条 = 「⛔ id 被已签批 ADR 挡住」⇒ 属 architect）· **B-3** DoD 7 口径订正（66 条 invariant 实为 **61 条有载体 + 5 条 ⏭**）· **C-3** R-10~R-15 · **C-4** R-9 / R-2 · `AGENTS.md` 归档分类表补记「✅ 主 spec 直改（无 delta）」变体
  ④ **R-6** 版本 pinning · **R-3** ArchForge 缺席（**本机不成立**）
  ⑤ **B3b-2 = 0/4**（4 族 external）—— ⛔ **本仓不可推进**，不排期

**关联**：台账 Entry **#445**（收口）· **#444**（立项 + 全量核对 + 本轮 metis 处置）· **#443**/**#442**/**#441**（**批 1** 的验收 / 收口 / 立项）· **#438**（上轮全量梳理）· `openspec/changes/archive/2026-11-16-cpptlm-spec-purpose-batch3/proposal.md`（`## 验收标准` 段 = 本报告判据来源）· `scripts/test/cep_spec_purpose_batch3_{gen,check}.py` · `test/spec_purpose_batch3_{attribution,reconcile}.json`
