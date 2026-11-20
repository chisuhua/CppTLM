---
name: cep-d1-archive-verify-backfill
priority: P0
phase: phase-10
category: governance
type: chore
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-0035
状态: "🟡 **已立项 · user 已拍板**(2026-11-20 · 关联登记册 `obj-cep-platform-remaining.md` §1.1 D-1 / §6.2 S3-5 / §6.3.1 · 决策来源 = 用户 2026-11-20 四项拍板 Q-8 / Q-9 / Q-10 / **Q-13**)"
created: 2026-11-20
---

# cep-d1-archive-verify-backfill: 归档验收债的分层处置 + 路径无关机关

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **范围纪律**: ⛔ **不补甲层 126 份**（只抽检 5 份；⚠️ 旧「35」是 CEP-only 混合口径，已废 —— 统一口径 = 全仓 **134** 真缺，见架构依据表）· ⛔ **不改任何已签批 ADR 正文** · ⛔ **不改 `openspec/specs/**`**（B 类 spec 债归 `cep-spec-stale-refs-r14-r15-r20`）· ⛔ **不新建登记面**（机关 = 单条存在性断言，不是第三个登记册）· ⛔ **不改 `rddf_stage_gate.sh`**（该闸管阶段顺序，语义不混）· ⛔ **不用「0.5 d / 38 份」口径**（已废除）· ⛔ **不按 `cep` 前缀特判、不做名字白名单豁免**（Q-13 已裁）。

## 架构依据

### D-1 是**两类债**，不是一类

| 层 | 归档区间 | 份数 | 性质 | 处置（user 2026-11-20） |
|---|---|:--:|---|---|
| **乙层 · 规则已形成期** | 归档 **≥ 2026-11-12**（不含甲层） | **8**（**5 CEP + 3 非 CEP**） | 同窗口 **21/29** 已有报告（**全部为名称 join 命中**）⇒ 这 8 份是**执行缺口** | **全部真补** |
| **甲层 · 早期期** | 归档 **< 2026-11-12** | **126**（全仓口径） | ⚠️ **并非「无该实践」** —— **11-06/11-07 已有 8 份 verifier 报告覆盖 9 个 change**（报告头明文声明验收对象），只是文件名未遵 `<change>-verify.md` 规范 ⇒ **分界线是「命名与四段格式是否统一」，不是「有无实践」** | **永久封存 + 抽检 5 份** |

**⚠️ 事实更正（2026-11-20 · per metis B1/B2 复验）**：本单元初稿写「甲层 35 份 · 仓内最早 verify 报告 = 2026-11-12 ⇒ 当时无该实践」—— **为假**：

| 复验命令 | 结果 |
|---|---|
| `git log --diff-filter=A --format="%ad" --date=short -1 -- .rddf/reviews/<8 份>.md` | **2026-11-06** × 4 · **2026-11-07** × 4 |
| 8 份报告的**报告头**（`验收对象` / `Change / Task` 行）| **明文声明**各自的验收 change |
| 这些 change 的**归档日期** | 全部 **< 2026-11-12** ⇒ **落在甲层** |

⇒ **甲层被覆盖的 9 个 change**（已验收，只欠命名规范化）：`cpptlm-cep-tilelink-runtime-engine` · `cpptlm-cep-basis-source-kind-and-cell-free` · `cpptlm-cep-source-kind-contract-alignment` · `cpptlm-cep-ualink-instance-assembly` · `cpptlm-cep-moesi-gpu-l4-e2e`（**与上一项同属一份报告** `cep-instance-assembly-and-l4-e2e-verify.md`）· `cpptlm-cep-ualink-coherence-engine` · `cpptlm-cep-dcoh-nvlink-refkind-migration` · `cpptlm-cep-refkind-carrier-whitelist-contract` · `cpptlm-cep-instance-assembly-contract-alignment`（**8 份报告覆盖 9 个 change**）

**统一口径（取代旧的「40」与「43」）**：

| 口径 | 总数 | 已验收 | 真缺 |
|---|:--:|:--:|:--:|
| **全仓统一**（Q-13 口径 · 不按前缀特判）| 164 | **30**（21 名称 join + **9** 内容已有）| **134** |
| CEP-only（参考口径，⛔ **不作判据**）| 50 | **19**（10 + 9）| **31** |

⛔ **旧「43」不匹配任何单一规则**（乙层 8 含 3 份非 CEP + 甲层 35 是 CEP-only）⇒ **已废**；⛔ **旧「40」= 只做名称 join 未查报告内容 ⇒ 虚高 8** ⇒ **已废**。

> ⚠️ **甲层「封存」的对象须按「真缺」界定**：甲层 **126** 份真缺里，有 **9** 个 change 已验收（欠命名规范化）—— 抽检时**不得**抽到这 9 个（抽了也无意义）；其余 **117** 份才是「未验收」的真债。
> ⚠️ **计数单位是 change 不是报告**（per critic BLOCKER-1）：`cep-instance-assembly-and-l4-e2e-verify.md` **一份覆盖 2 个 change** ⇒ 按报告份数计会少算 1。

### 乙层 8 份的构成（5 CEP + 3 非 CEP）

> ✅ **该清单经 metis B1 复验后完全不变** —— 乙层 29 个目录中 21 份是**名称 join 命中**、**0 份**是「内容已有但名不符」⇒ 真缺恰为这 8 份。

| # | change | 期望报告路径 | 备注 |
|:--:|---|---|---|
| 1 | `2026-11-13-cep-evidence-tier-declaration` | `.rddf/reviews/cep-evidence-tier-declaration-verify.md` | |
| 2 | `2026-11-13-cpptlm-cep-full-green-run-evidence` | `.rddf/reviews/cpptlm-cep-full-green-run-evidence-verify.md` | |
| 3 | `2026-11-15-cpptlm-cep-adr-verification-carriers` | `.rddf/reviews/cpptlm-cep-adr-verification-carriers-verify.md` | **T7.1 未勾即归档** |
| 4 | `2026-11-15-cpptlm-cep-spec-strict-compliance` | `.rddf/reviews/cpptlm-cep-spec-strict-compliance-verify.md` | **T10.4 未勾即归档** |
| 5 | `2026-11-16-cpptlm-cep-mesh-e2e-carrier` | `.rddf/reviews/cpptlm-cep-mesh-e2e-carrier-verify.md` | **T3.8 未勾即归档** |
| 6 | `2026-11-15-cpptlm-spec-scenario-level-fix` | `.rddf/reviews/cpptlm-spec-scenario-level-fix-verify.md` | 非 CEP（Q-13 纳入）|
| 7 | `2026-11-19-cpptlm-agents-md-slimming` | `.rddf/reviews/cpptlm-agents-md-slimming-verify.md` | 非 CEP（Q-13 纳入）|
| 8 | `2026-11-19-cpptlm-subdir-agents-governance` | `.rddf/reviews/cpptlm-subdir-agents-governance-verify.md` | 非 CEP（Q-13 纳入）|

### 这 8 份的债务是「自描述」的

3 份 CEP 的 `tasks.md` **带着未勾的验收项就归档了**，而它们的 `proposal.md` 里有完整 AC 清单、`tasks.md` 里有**写死的报告路径**：

| change | tasks 勾选 | 未勾项 |
|---|:--:|---|
| `cpptlm-cep-adr-verification-carriers` | 38/40 | **T7.1 verifier 独立验收** + T7.2 残余确认 |
| `cpptlm-cep-spec-strict-compliance` | 39/40 | **T10.4 verifier 独立验收（AC-01~AC-18 逐条 + 工具证据）** |
| `cpptlm-cep-mesh-e2e-carrier` | 26/27 | **T3.8 verifier 独立验收**（含「须另写独立实现复算」）|

⇒ 补债成本不是从零调研，是**按现成 AC 逐条取证**。

### 根因不是「规则没写」，是「规则只挂在一条路径上」

| 事实（2026-11-20 实测） | 含义 |
|---|---|
| rdd-builder P3 **有**归档门（ADR-0035：gated on verdict cache；缺失/过期/不全 ⇒ `exit 7` 拒绝归档）| 规则**存在**且**有牙** |
| 上述 3 份**无** `.rddf/state/builder/<name>.json` | ⇒ **未走 rdd-builder 状态机**，直接 `openspec archive` |
| ⇒ verdict cache 闸**从未运行** | ⇒ **路径可绕过** |

⇒ **只写一条文档约定 = 同样的绕过会再发生**。必须让机关**与路径无关**。

### ⛔ 机关的**路径无关性有一处未覆盖**（per metis B3）

`openspec/changes/archive/` **被 `.gitignore:64` 排除** ⇒ git 只跟踪 **40 个**归档目录（186 个 force-add 文件），本地却有 **164** 个目录 ⇒ **CI 的 fresh clone 上闸看到的不是全集**。

⇒ 若闸在扫面目录数 < 基线时仍 rc=0 ⇒ 就是用新闸**复刻 `S2-8` 的假绿**（本仓刚判过的那类病）。⇒ 见 What Changes #3 的 **fail-closed** 条款。

## 范围

- **In-Scope**：
  - 乙层 **8 份**独立验收报告 → `.rddf/reviews/<change>-verify.md`（**精确命名**，见 What Changes #3）
    - 按 verifier 产出契约 1–5 步：编译/静态检查 → 单测 → 集成 → 边界错误路径 → 遗留清扫，每条判据带工具证据
    - ⚠️ **写报告的是 `verifier` 角色**（⛔ 不是 builder —— builder 写自己的验收 = 自证）
    - ⚠️ 3 份带未勾 AC 的 change **必须非原实现者复验**，且报告头标注「**原归档未验 · 本报告为 YYYY-MM-DD 追溯补验**」（可 `grep` 机判）
  - 甲层**抽检 5 份**（均匀覆盖「规则尚未统一期」：`10-23 cpptlm-cep-umbrella` · `10-28 cpptlm-cep-chi-rnf-subset` · ``11-02 cpptlm-cep-tilelink-tree-manager` · `11-06 cpptlm-cep-tilelink-t3-mapping-survey` · `11-10 cpptlm-cep-bridge-reattach-guard`）→ 抽检记录落 `.rddf/reviews/`<br>⚠️ **名单订正（per critic BLOCKER-1）**：初稿的第 4 项 `cpptlm-cep-moesi-gpu-l4-e2e` **已被验收**（`cep-instance-assembly-and-l4-e2e-verify.md` 报告头明文声明验收该 change）⇒ **已移除**，替换为同窗口（11-06）**未验收**的 `cpptlm-cep-tilelink-t3-mapping-survey`（⚠️ 该 change **有 `## Acceptance` 段但无 tasks.md、无专属验收报告** ⇒ 是**真债**）
  - 新建 `scripts/test/archive_verify_check.sh` + `test/CMakeLists.txt` 注册
- **Out-Scope**：
  - ⛔ 甲层 126 份逐份补（仅抽检 5 份；抽检命中问题 ⇒ **定向补那一份**，不扩成全量）
  - ⛔ `openspec/specs/**` 任何改动（B 类 5 项归 `cep-spec-stale-refs-r14-r15-r20`）
  - ⛔ `rddf_stage_gate.sh`（阶段顺序闸，语义不混）
  - ⛔ 已签批 ADR 正文

## Why

**不做的代价**：真缺 135 份里已经出现 **3 份带着「未做验收」归档**的实例 ⇒ 「归档即已验证」这个假设在本仓**已被证伪**。若不补且不上闸，下一轮同一条路径会再漏 —— 而**债务是无声的**（没有闸会告诉你）。

**为什么乙层必须真补而不是「登记为已知缺口」**：那 3 份的未勾项**就是验收本身**，且 `proposal.md` 里写着 AC 清单 ⇒ 不补 = 让「实现是否真的对」永久无结论。

**为什么甲层不补**：甲层 **126 份真缺**在**规则尚未统一期**归档（其中 **9** 个 change 已验收、只欠命名规范化），属口径未建立；全补 ≈30~50 d 且零能力产出。抽检的作用不是消债，是**检验「甲层只是形式缺位、实质无问题」这个假设**。

**为什么闸要统一覆盖、不按前缀特判**：Q-13 实测发现 3 份**非 CEP** 同性质债务（归档 11-15 / 11-19）。若闸只管 `cep`，等于把「非 CEP 归档不需要验收」写成明文特判 —— 与本次纠出的「窄检口径」是同一类错误。

**为什么闸必须 fail-closed（per metis B3）**：`openspec/changes/archive/` 被 `.gitignore:64` 排除（git 只跟踪 **40/164** 个目录）⇒ CI fresh clone 上闸看到的是**残缺集合** ⇒ 闸若在残缺集合上 rc=0，就是**用新闸复刻 `S2-8` 的假绿**（本仓刚判过的那类病）⇒ 必须「扫面目录数 < 基线 ⇒ 判红」。

## What Changes

| # | 变更 | 落点 | 形态 / 约束 |
|:--:|---|---|---|
| 1 | **乙层 8 份验收报告** | `.rddf/reviews/<change>-verify.md` × **8** | 按 verifier 产出契约；⚠️ 必须对**未勾的 T7.1 / T10.4 / T3.8** 给出明确结论（不是「跳过」）；⚠️ 报告头含「追溯补验」标注 |
| 2 | **甲层抽检 5 份** | `.rddf/reviews/archive-audit-<date>.md` | 每份结论 = 「实质可信 / 发现疑点（列具体项）」；命中 ⇒ 定向补该份 |
| 3 | **新闸** | `scripts/test/archive_verify_check.sh` | **join 规则（精确）**：`report = ".rddf/reviews/" + strip_date_prefix(archive_dir) + "-verify.md"`。<br>**扫面**：`openspec/changes/archive/` 下**目录**中日期 **≥ 2026-11-12** 者（**统一覆盖，不按 `cep` 前缀特判**）；**日期 < 2026-11-12 ⇒ 白名单跳过**。<br>**fail-closed ①（畸形目录名）**：目录名不匹配 `^\d{4}-\d{2}-\d{2}-` ⇒ **判红**（红因签名「归档目录名缺日期前缀」），⛔ 不静默 strip；非目录项（如 `STATUS_AUDIT_2026-06-15.md`）跳过。<br>**fail-closed ②（扫面不可见 · per metis B3）**：`openspec/changes/archive/` **被 `.gitignore:64` 排除**（git 只跟踪 40/164 个目录）⇒ 闸内实测**扫面目录数 < 29** ⇒ **判红**（红因签名「归档目录不可见：CI 无法校验」），⛔ **不得静默 rc=0**（否则就是用新闸复刻 `S2-8` 的假绿）。<br>**三检**：① 报告存在 ② 含 `判据来源` ∧ (`判定` ∨ `逐条判定`) ∧ (`遗留` ∨ `遗留清扫`) ③ 去空白长度 ≥ **400** 字符。<br>⛔ 不判内容对错（语义归 verifier）|
| 4 | **ctest 注册** | `test/CMakeLists.txt` | `add_test(NAME archive_verify_check ...)`，与既有 `cep_*` 闸同形 |
| 5 | **封存边界登记** | 登记册 §1.1 D-1 | 已写：封存边界 = 归档日期 < 2026-11-12；⚠️ 是**一次性历史封存**，⛔ 不得按名字逐项豁免 |
| 6 | **命名规范** | 闸文件头 + 本单元 | 未来归档 change **必须**产出 `<change>-verify.md`；⛔ 不用 `-verify-<date>.md` 变体；归档目录名**必须单日期前缀** |
| 7 | **报告头机器标记** | 8 份报告首行 | 强制 3 行：`- **角色**：verifier` · `- **原归档未验**` · `- **追溯补验 · YYYY-MM-DD**` ⇒ ⛔ 不靠人读，闸可 `grep -qF` 校验（per metis S4）|

> ⚠️ **执行顺序是硬约束**：**先 ① 补 8 份，再 ③ 上闸** —— 反过来会让 CI 在补债期间保持红。闸上线时必须**即绿**（8 份已补完 + 21 份存量过三检）。
> ⚠️ **闸是地板，AC 是交付标准**（⛔ 不是两套并列契约）：闸只证「报告存在且形态成立」；本单元 AC-1 要求的 4 段（含「失败项与路由建议」）**高于**闸。⛔ 不得把闸绿读作「报告达到 AC 标准」。

## Acceptance

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔「rc≠0」不构成红因。

- [ ] **AC-1（乙层 8 份存在且结构合格 · 可机判）**：对清单 8 份中的每一份 `f`，下列 **4 条独立 `grep -qF` 用 `&&` 串联**全部命中：① `判据来源` ② `逐条判定` **或** `判定` ③ `失败项` ④ `已用轮次`；⚠️ ⛔ **不得**写成 `grep -c <标题> >= 4`（同标题重复 4 次即通过 ⇒ 假绿）；⚠️ 8 份的**文件名**须精确等于 `<change>-verify.md`（去掉日期前缀）
- [ ] **AC-2（未勾项有明确结论）**：`cpptlm-cep-adr-verification-carriers` 的报告对 **T7.1 / T7.2**、`cpptlm-cep-spec-strict-compliance` 对 **T10.4**、`cpptlm-cep-mesh-e2e-carrier` 对 **T3.8** 各给出「通过 / 不通过 / 不可判 + 原因」；⛔ **不得**以「本项不适用」空转（须给工具证据或明确阻断原因）
- [ ] **AC-3（每条判据至少有 1 条工具证据 · 人工复核项）**：8 份报告中**每条 pass 判定**都能追到 ≥1 条 `read`/`grep`/`shell` 证据；⚠️ 反 fixture：抽一份，删掉某条判据的取证 ⇒ 该判据不得维持 pass。⚠️ **本条为人工复核项，不计入闸**（⛔ 闸不判内容对错 —— 它只证「形态成立」）
- [ ] **AC-4（角色与防自验 · per oracle SF-1 · 可机判）**：8 份报告**首部**须含 3 条**字面量**标记（⛔ 不靠人读）：`- **角色**：verifier` · `- **原归档未验**` · `- **追溯补验` ⇒ `grep -qF` 逐份命中；⚠️ 反 fixture：把某份报告的「角色」改回 builder ⇒ 判红
- [ ] **AC-5（抽检 5 份有结论）**：抽检记录存在，5 份各给「实质可信 / 发现疑点」结论；若命中 ⇒ 登记「定向补哪一份」；⚠️ ⛔ 不得把抽检写成「已全量审计」；⚠️ **抽检对象不得取自甲层已验收的 9 个 change**（`cpptlm-cep-tilelink-runtime-engine` / `cpptlm-cep-basis-source-kind-and-cell-free` / `cpptlm-cep-source-kind-contract-alignment` / `cpptlm-cep-ualink-instance-assembly` / `cpptlm-cep-moesi-gpu-l4-e2e` / `cpptlm-cep-ualink-coherence-engine` / `cpptlm-cep-dcoh-nvlink-refkind-migration` / `cpptlm-cep-refkind-carrier-whitelist-contract` / `cpptlm-cep-instance-assembly-contract-alignment`）⇒ ⚠️ **原名单中的 `cpptlm-cep-moesi-gpu-l4-e2e` 已移除**（已被 `cep-instance-assembly-and-l4-e2e-verify.md` 验收）⇒ 替换为 `cpptlm-cep-tilelink-t3-mapping-survey`
- [ ] **AC-6（机关存在且已注册）**：`scripts/test/archive_verify_check.sh` 存在且可执行；`test/CMakeLists.txt` 含 `add_test(NAME archive_verify_check`；`ctest -N | grep archive_verify_check` 命中 1
- [ ] **AC-7（机关判据有鉴别力 · 决定性变异 4 条）**：影子副本模式，四条各自「破坏 ⇒ 按签名红 ⇒ 还原 ⇒ 绿」：
  - ① 移走 8 份报告之一 ⇒ 红且**只红该 change**（签名「归档 change 缺独立验收报告：<name>」）
  - ② 把某份报告替换为**只有 3 个标题的空骨架**（< 400 字符）⇒ 红（签名「报告形态不成立：<name>」）
  - ③ 造一个 `<dir>` 目录名不带日期前缀 ⇒ 红（签名「归档目录名缺日期前缀」）
  - ④ **造一个 `<date>-fake-change/` 归档目录 + 无报告** ⇒ 红（签名「归档 change 缺独立验收报告：fake-change」）—— ⚠️ **这一条才是绕过 rdd-builder 的真实形态**，前三条都测不到它（per metis B3）
- [ ] **AC-8（封存边界生效 · 不误伤甲层）**：未动任何甲层 change 时 rc=0 ⇒ 甲层白名单生效；⚠️ 反 fixture：把边界从 `2026-11-12` 改成 `2026-11-01` ⇒ 闸**必须转红**（扫到 11-01~11-10 的甲层）
- [ ] **AC-8b（扫面不可见时 fail-closed · per metis B3）**：影子副本里把 `openspec/changes/archive/` **整体移走** ⇒ 闸**必须判红**（签名「归档目录不可见：CI 无法校验」），⛔ **不得** rc=0；还原 ⇒ 绿。⚠️ 这是「新闸不得复刻 S2-8 假绿」的机械保证
- [ ] **AC-9（闸落地即绿）**：① ② 完成后跑 `bash scripts/test/archive_verify_check.sh` ⇒ rc=0（8 份已补 + 21 份存量过三检；⚠️ 前提 = 扫面目录数 ≥ **29**，否则按 AC-8b 判红）
- [ ] **AC-10（范围纪律 · 零越权）**：`git diff --name-only` **不含** `openspec/specs/**` · `docs/adr/**` · `docs/architecture/**` · `scripts/test/rddf_stage_gate.sh` · 甲层任何 change 目录
- [ ] **AC-11（0 回归）**：`ctest` 全绿（⛔ 不写死数字，以 `ctest -N` 实测为准）· 全部 `scripts/test/cep_*.sh`（**数量以 `ls scripts/test/cep_*.sh | wc -l` 实测为准** —— ⛔ 不写死「45」）逐个 rc=0 · `bash scripts/test/rddf_stage_gate.sh` rc=0 · `docs_sync --strict` / `agents_md_lint` / `format --check` 全 rc=0 · **工作区干净**（⚠️ 本单元**不创建** openspec change ⇒ 无归档副作用 ⇒ 「工作区干净」成立）

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| （无） | — | ⛔ **零能力增量** —— 补验收报告 + 上存在性/形态闸 ⇒ **不产 delta spec**、**不合并 main spec** ⇒ 归档时零契约面变化 |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `.rddf/reviews/<8 份>-verify.md` · `.rddf/reviews/archive-audit-<date>.md` · `scripts/test/archive_verify_check.sh` |
| **修改文件** | `test/CMakeLists.txt`（+1 add_test）|
| **零改动（硬查）** | `openspec/specs/**` · `docs/adr/**` · `docs/architecture/**` · `scripts/test/rddf_stage_gate.sh` · 甲层任何 change 目录 |
| **冻结面** | **零**（`scripts/test/` 与 `test/CMakeLists.txt` 不在 `FROZEN_GLOBS`）|
| **闸** | ctest **+1**（`archive_verify_check`）；该闸对**未来**归档 change 生效 ⇒ 规则从「文风」变「机关」；⚠️ 该闸**不匹配 `cep_*` 命名** ⇒ 「`cep_*.sh` 全绿」**不覆盖**它 ⇒ 须单独列名验收（per metis 歧义 12）|
| **闸的域归属** | ⚠️ **仓级域**（借用 CEP 章节作载体）—— 统一覆盖 3 份非 CEP 债 ⇒ **⛔ 不表示 CEP 范围扩张**；owner = **仓级**（CEP objective 关闭后闸仍须有人负责，per metis 缺失 AC 11）|
| **KPI** | **0/3 → 0/3**（不创建 openspec change）|
| **成本** | **≈5~5.5 d**（8 份 × **≥0.5 d 下限** = 4 d + 闸 ≈0.5 d + 抽检 ≈0.5~1 d ⇒ **按此公式下界为 5 d**；⚠️ 旧「3~4 d」**与自身公式矛盾**，已订正 —— per metis S2）|
| **风险** | ① 闸写成对**全部**归档目录生效 ⇒ 误伤甲层封存层 ⇒ AC-8 反 fixture ② 报告写成模板 ⇒ AC-1/AC-3 兜底 ③ 抽检被读成全量审计 ⇒ AC-5 明文禁止 ④ 执行顺序颠倒 ⇒ CI 补债期常红 ⇒ What Changes 顺序约束 + AC-9 ⑤ **builder 自验** ⇒ AC-4 兜底 ⑥ 未来纯 doc-only change 被闸卡 ⇒ ⚠️ **Q-13 已明确不设豁免**（doc-only 也有先例：`cpptlm-spec-purpose-batch1-verify.md`）⇒ 需要报告 ⑦ **CI 上扫面残缺 ⇒ 闸空转绿**（`.gitignore:64` 只跟踪 40/164 目录）⇒ AC-8b fail-closed 兜底 ⑧ **甲层已验收的 8 份被重复补报告** ⇒ AC-5 明列禁抽名单 |

## 待裁

✅ **无** —— Q-8（分层处置 / 废除 0.5 d / 独立闸）、Q-13（统一覆盖 + 8 份）均已由用户 2026-11-20 拍板。

## 审查轨迹（planner 起草 · 2026-11-20）

| 阶段 | 结论 | 对本单元的影响 |
|---|---|---|
| 缺口梳理（metis 1 + oracle 1 + critic 3） | 定级 S3-5；指出「全补 ≈12 d 是惩罚性成本」 | 促成「分层处置」而非「全补」 |
| architect 裁决（oracle 1 + critic 2） | Q-8 建议「规则化 + 补 0 份 + 抽样疑点定向补」 | 本单元把「补 0」细化为「乙层必补」（因 3 份带未勾验收项归档）|
| **user 拍板 2026-11-20（Q-8）** | 乙层全补 · 甲层封存+抽检 · 废除 0.5 d · 机关 = 独立闸 | 定稿 v1（5 份）|
| **oracle 对 §10 判 REJECT** | BLOCKER-1（Q-13 未传播）· BLOCKER-2（4 标题不可满足）· SF-1（角色/防自验）| ⇒ 本单元全量同步：8 份 · 3 标题闸判据 · AC-4 角色约束 |
| **user 拍板 2026-11-20（Q-13）** | 统一覆盖 + 8 份全补（不按前缀特判）| 定稿 v2（本版）|
