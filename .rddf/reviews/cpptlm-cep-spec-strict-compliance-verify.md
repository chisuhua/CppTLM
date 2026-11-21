# 验收报告 · `cpptlm-cep-spec-strict-compliance`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-15；归档时 **T10.4 未勾** ⇒ 本报告为**独立追溯验收**，非原实现者自证）

> **范围**：验收对象 = 归档 change `openspec/changes/archive/2026-11-15-cpptlm-cep-spec-strict-compliance/` 的 **AC-01 ~ AC-18** 逐条 + **T10.4** 结论。
> ⚠️ 本 report 只对**可复现**的判据取证；归档前的历史状态（当时的 manifest / diff / 计数）**不可复现** ⇒ 如实记「不可判」。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-01~AC-18） | `openspec/changes/archive/2026-11-15-cpptlm-cep-spec-strict-compliance/proposal.md` §验收标准 |
| 未勾项（T10.4） | 同 change `tasks.md:80` |
| 交付物 | `scripts/test/cep_spec_strict_check.sh` · `scripts/test/cep_test_shadow.sh` · `removals.json`（基线 fixture）· 复用闸 `scripts/test/cep_adr_structural_check.sh` · `scripts/test/cep_decision_block_check.sh` |
| 关联 | `ADR-CEP-12`（schema 契约面）· `openspec/specs/cep-*/spec.md`（47 份）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-01 | `cep-*` 逐份 `--strict` **0 红** | **通过** | `bash scripts/test/cep_spec_strict_check.sh` → `✅ cep-* 全部 valid（47 份）`，rc=0；独立复核 `openspec validate --specs --strict --json` ⇒ `cep-` 项 **47** · **`cep-` invalid = 0** |
| AC-02 | 12 个目录名**逐个** `valid == true` | **不可判**（对象已漂移）| ⚠️ 当时的 12 个具体目录名未在本报告可取证范围内逐一枚举；当前事实 = `cep-` **47 份全部 valid**（AC-01 的更强表述）⇒ **实质成立**，但「那 12 个」的逐个清单不可复现 ⇒ 记不可判 |
| AC-03 | 覆盖对账：`items` == `ls openspec/specs \| wc -l`（**129**）∧ `cep-` 项 == **43** | **通过（口径自洽）/ 基线数字漂移** | 闸实跑 → `✅ items 数 == openspec/specs 目录数（138）` + `✅ cep- 项数 == cep-* 目录数（47）` ⇒ **闸的对账机制成立**；⚠️ 绝对数字 129→**138**、43→**47**（此后新增 5 份 cep spec）⇒ 绝对值不可判，**自洽机制可判** |
| AC-04 | 规范句移除**逐句归因**：`removals.json` 的 `sentence_sha256` 集合 == 基线−终态的 SHALL/MUST 句差集 | **不可判** | ⚠️ 归因 fixture 的基线取自**归档前**的 spec 正文；现正文已多次演进 ⇒ 差集不再等于 `removals.json`；且本环境未见该 fixture 的当前落点 ⇒ 不取证，**不猜** |
| AC-05 | 术语表**不整体消失**（受控 token 仍在） | **不可判** | 同 AC-04：基线态不可复现 ⇒ 不取证 |
| AC-06 | 既有 Scenario **逐块 byte-equal**（基线取 `git show <base>:<path>`） | **不可判** | ⚠️ 基线 commit 未知（AC 未在本报告范围内给出 `<base>`）⇒ 不可执行逐块比对 ⇒ 不取证，**不猜** |
| AC-07 | 新增场景**已登记**（标题 ∈ design 下沉清单 ∧ 正文 ≥1 bullet 共享核心 token） | **不可判** | 同上（依赖归档前 design + 目标 spec 的对应关系）|
| AC-08 | Purpose 四条正交（`openspec --json` 判 valid · ≥50 字符 · 不复制 Requirement 首句 · 不含占位）| **通过（现态）** | 闸实跑含 `✅ [毒化A:Purpose 占位] 毒化 cep-adapter-schema-contracts ⇒ invalid_ids == {毒化 id}（恰好 +1，归因精确）` ⇒ Purpose 占位**可判**；`openspec validate --specs --strict --json` ⇒ `cep-` invalid = 0 ⇒ 现态四条成立 |
| AC-09 | 防退闸 + 反向：闸 rc=0；fixture 首步断言 `invalid_ids(基线) == ∅`（否则 **exit 2**）；毒化 ⇒ 按签名红 | **通过** | 闸 rc=0；实跑输出两条毒化臂均「恰好 +1，归因精确」（毒化 A=Purpose 占位 / 毒化 B=Requirement >500 字符）⇒ 判据有牙且归因精确 |
| AC-10 | 反向 fixture **仓内零改动**：`openspec/` + `docs/adr/` 的**全量** sha256 manifest 前后相等 | **通过** | 闸实跑输出 `✅ 仓内零改动（openspec/ + docs/adr/ 全量 sha256 manifest 前后相等）` |
| AC-11 | 环境缺失 ⇒ **exit 2**（`PATH` 去掉 `openspec` 后跑闸，stderr 含 `openspec`） | **通过** | 实测 `PATH=/usr/bin:/bin bash scripts/test/cep_spec_strict_check.sh` ⇒ **rc=2**（须 2）且 stderr 含 `✗ 环境错误:找不到 'openspec' —— 无法取证（exit 2，⛔ 不计作判据失败）` ⇒ **实测复现**（⛔ 非读码推断）|
| AC-12 | 范围零改动（`git diff --name-only` 不含 `include/` · `src/` · `test/*.cc` · `docs/adr/` · `openspec/changes/archive/`） | **不可判** | ⚠️ 归档前的 diff 不可复现（同 F-2）|
| AC-13 | 0 回归（**无魔数**）：`[cep]` 用例数 == 基线 · `ctest -R cep` 全绿 | **部分通过 / 计数不可判** | `ctest --test-dir build -N` → `Total Tests: 196`（全绿面由本单元的 §0 回归命令覆盖）；⚠️ `[cep]` 用例数的**基线**未记录 ⇒ 不可判；`ctest -R cep` 现状全绿 |
| AC-14 | helper 改动的回归：共用该 helper 的 `cep_adr_structural_check.sh` 与 `cep_decision_block_check.sh` 全绿 | **通过** | `python3 scripts/test/cep_decision_block_check.py` → `✅ CEP 治理判据闸:全部判据通过`，rc=0；`bash scripts/test/cep_adr_structural_check.sh` rc=0（与本单元同批实跑）|
| AC-15 | 五步链 **7 步**（顺序固定） | **部分通过 / 历史不可判** | 可证端 = 归档产物 + `cep_freeze_check.sh` 无参 rc=0（授权在位）；⛔ 7 步的**历史执行序列**不可复现 ⇒ 记不可判 |
| AC-16 | provenance：`manifest.frozen_at_commit` == 产物 commit 的 `git rev-parse` 值 | **不可判** | ⚠️ 本环境未见该 change 产出的 manifest 落点（`removals.json` 未随归档入库）⇒ 不取证，**不猜** |
| AC-17 | 归档后 diff 只落目标段（5 份的 hunk 行号落在「目标 Requirement 段 ∪ Purpose 段」） | **不可判** | 同上（依赖归档前 diff）|
| AC-18 | KPI 回填（归档后 1/3 → 0/3 + 体量数字） | **通过（现态）** | 当前 active change 数 = **0**（`openspec list --json` 口径）⇒ KPI 活口径现态为 0/3；体量数字（146→147）**不可判**（归档前基线不可复现）|
| **T10.4** | verifier 独立验收（AC-01~AC-18 逐条 + 工具证据） | **通过（8 条不可判已逐条标注原因）** | ✅ 本报告即 T10.4 的交付；⚠️ 18 条中 **8 条不可判**（AC-02/04/05/06/07/12/16/17 + AC-03/08/13/15/18 的历史数字部分）⇒ **不满足「全条通过」** ⇒ 结论 = **不可判（历史基线不可复现）**，⛔ **不**记为「已通过」|

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | **AC-04 / AC-05 / AC-06 / AC-07 / AC-16 / AC-17 六条依赖「归档前基线」⇒ 不可判**；AC-02 / AC-12 同理 | ⛔ **不可补**（历史状态不可回溯）⇒ 归**制度面**：今后归档的 change，其验收报告须在**归档前**固化「基线 commit + 计数 + manifest」三元组（**下一批归档模板**必含）|
| F-2 | T10.4 的**终态结论**只能是「不可判」而非「已通过」 | ⚠️ 这是**结论形态**问题，不是失败 —— 记入台账：「3 份带未勾验收项归档的 change 中，**2 份**的未勾项在事后**不可判**（T7.1 的 8/10 可判、T10.4 的 8/18 可判）」⇒ **反向支撑**本单元的立论（归档前必须验）|
| F-3 | `AC-08` 的「Purpose 不复制 Requirement 首句」等 4 条中，只有「占位」可由毒化臂证明 | ⇒ 建议下一版闸补 1 条毒化臂（把 Purpose 改成 Requirement 首句复制 ⇒ 须按签名红）—— 归 **I-11** 或新开 micro-ticket，⛔ 不并入本单元 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（毒化臂在**影子根**内执行，闸自带 `openspec/` + `docs/adr/` 全量 sha256 前后比对，且实跑输出 `✅ 仓内零改动`）|
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 未触碰 `include/` · `src/` · `test/*.cc` · `docs/adr/` · `openspec/specs/` · 冻结清单 |

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮）。
