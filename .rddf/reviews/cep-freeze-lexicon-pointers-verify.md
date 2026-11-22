# 验收报告 · **`cep-freeze-lexicon-pointers`**（冻结面「引擎」三词口径指针 · 登记册 §6.4 W3-a · 缺口 G-8）

- **角色**：builder（**实施侧自验** —— 本单元由同一 builder 会话实施并即时验收；⚠️ **无独立 verifier 环节**）
- **日期**：2026-11-22 · **实施单元基线 commit**：`68025dc0`（立项）
- **授权**：`Entry #520`（台账 `obj-cep-platform.md` · 授权记录 · commit `d2094a08`）
- **机制**：M1 冻结面五步授权链（`docs/guide/cep-usage.md §12.6`）

## 判据来源

| 来源 | 位置 |
|---|---|
| **AC-1 ~ AC-8**（逐条可机判） | `.rddf/improvements/cep-freeze-lexicon-pointers.md` 的「## Acceptance」段 |
| 写盘范围与措辞授权（逐条，含「⛔ 不授权」项） | 台账 **Entry #520** 的「授权范围」表 |
| 指针措辞与「为什么必须走五步链」 | architect 裁决记录 `.rddf/reviews/cep-gap-plan-adjudication-q1-q11-2026-11-20.md` §2.1 / §5.2 / L-3 |
| 五步链机制与硬约束 | `docs/guide/cep-usage.md §12.6`（⛔ 台账与清单不得合并提交） |

## 五步链执行记录

| 步 | 要求 | 实际 | 结果 |
|:--:|---|---|:--:|
| 1 | 台账 Entry 先落（授权记录） | **Entry #520** · commit `d2094a08`（**单独 commit**） | ✅ |
| 2 | 产物 commit（3 个冻结文件） | commit `d860430a`（**单独 commit** · 3 文件 +21/−2） | ✅ |
| 3 | `--update --authorized-by` | `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #520"` ⇒ 「授权校验通过:台账 entry 'Entry #520'」· 差集「**新增 0 · 移出 0 · hash 变更 3**」 | ✅ |
| 4 | 清单**单独** commit | commit `e92aa61d`（**只**含 `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`） | ✅ |
| 5 | 无参复验 rc=0 | `bash scripts/test/cep_freeze_check.sh` ⇒ rc=0 | ✅ |

> ⚠️ **第 5 步的一条告警是常态、不阻断**：`! 冻结清单的 frozen_at_commit(d860430a) ≠ 当前 HEAD(e92aa61d)` —— per `cep-usage.md §12.6`「清单 commit 之后 `frozen_at_commit` 落后 HEAD 一个 commit 是**常态且不阻断**（闸仅告警）⇒ ⛔ **不要为消除该告警重复 `--update`**」⇒ **未重复执行**。

## 逐条判定（AC-1 ~ AC-8）

| # | 判据 | 实测 | 结论 |
|:--:|---|---|:--:|
| **AC-1** | 指针在 07 号**第 138 行** | `sed -n '138p' docs/architecture/07-coherence-exploration-platform.md \| grep -q '§12.2.1'` ⇒ **rc=0**（实测该行含「⚠️『引擎』口径 = 协议族引擎(5) —— 与 engine 文件(6,含 Null 外壳)/ adapter(8) 是三个不同的词 ⇒ 口径见 docs/guide/cep-usage.md §12.2.1」） | ✅ |
| **AC-2** | 07 号**行数零位移** | `git diff --numstat` ⇒ **1 增 / 1 删**（同行替换，**净 0 行**）⇒ `bash scripts/test/cep_index_position_check.sh` **rc=0**（该闸逐行核对**索引 A / 索引 B 的「行」列** == 实测行号 ⇒ 插行即红） | ✅ |
| **AC-3** | 指针在 protocol-engine **第 44 行**；`:16` 字面量零位移 | `sed -n '44p' … \| grep -q '§12.2.1'` ⇒ **rc=0**；`sed -n '16p' … \| grep -q '16 个 adapter 实例'` ⇒ **rc=0**（跨仓机器锚点仍在第 16 行）⇒ diff **1 增 / 1 删** | ✅ |
| **AC-4** | ADR **只追加 SU**，`:88` / `:126` 原文未变 | diff **19 增 / 0 删**（`git diff -U0 … \| grep '^-'` ⇒ **零删除行**）⇒ 文末含 `## Status Update 2026-11-22(per 台账 **Entry #520** …)` 且含 `§12.2.1` | ✅ |
| **AC-5** | 冻结链完整 | 无参 `cep_freeze_check.sh` **rc=0**；清单 `artifact_count` == **90**（不变）；`--update` 差集恰 **3** 处 `~ hash 变更`（**零新增 / 零移出** ⇒ 无需任何移出 / 排除声明行）；`frozen_at_commit` = `d860430a`（**产物 commit 的 sha**）· `authorized_by` = `Entry #520` | ✅ |
| **AC-6** | 零 ADR 决策变更 / 零 KPI / 零 OpenSpec change | `git diff --name-only 68025dc0..HEAD -- openspec/specs openspec/changes src include test` ⇒ **空**（零 C++ / spec / test 改动）；`ls openspec/changes/ \| grep -v archive \| wc -l` = **0**（前后相等）；KPI 活口径行**零改动**（P-5 实测「0 处活口径全部同步（锚定 7 处，逐处取值 == 实测）」） | ✅ |
| **AC-7** | 相关闸全绿 | `cep_index_position_check` · `cep_doc_ref_check` · `cep_docs_sync_check` · `cep_adr_structural_check` · `cep_freeze_check` **全 rc=0** | ✅ |
| **AC-8** | 单元 ↔ 登记册可对账 | `for f in .rddf/improvements/cep-*.md; do grep -q "$(basename "$f" .md)" … \|\| echo "无归属: $f"; done` ⇒ **输出为空**（19 份单元全部有归属行；本单元见登记册 §6.3 **G-8** 行 / §6.4 **W3-a** 行 / §6.8 单元表） | ✅ |

## 关键设计（为什么这样改，而不是更省事的做法）

| 设计点 | 取舍 |
|---|---|
| 两处**同行追加**（而非插行） | 07 号的**索引 A / 索引 B 带「行」列**且由 `cep_index_position_check.sh` 逐行核对 ⇒ 插一行 ⇒ 后面所有 SU 行号位移 ⇒ 闸红。同理跨仓锚点 `protocol-engine.md:16` 会位移 ⇒ **零行数位移是硬约束，不是风格**。代价：指针与被释义的文字同行（读起来稍密）——可接受，因为它就在读者读到「5 个引擎」的那一行 |
| 07 号的指针写在**代码块内**（`:138` 在 ```` ``` ```` 128–147 内） | 改不了 —— 代码块内无法渲染 markdown 链接。取舍：写**纯文本路径**（`docs/guide/cep-usage.md §12.2.1`），读者可搜到；⛔ 不用反引号（代码块内会显示成字面量） |
| ADR **只追加 SU** | `ADR-CEP-20` 的「签发后的边界」明写「决策内容自此不可改 ⇒ 只能以追加 `## Status Update` 段的形式进行」⇒ 正文零字节改动（AC-4 的零删除行是硬证据） |
| 指针**不复制**三词定义 | `cep-usage.md:50` 的「⛔ 本节不给分级清单」禁令继续有效 ⇒ 本单元只写「口径见 §12.2.1」，⛔ 不在冻结面另立一张强度表（那会立刻腐化） |
| 本单元**独立立项**（`cep-freeze-lexicon-pointers`） | architect 裁决 §5.2 末段已裁定：归属 `cep-registry-stale-cleanup`？⇒ **⛔ 不**（其 AC-6 禁止 `docs/architecture/` diff ⇒ 物理不可执行）⇒ **另立单元**。若硬塞进既有单元，等于给「零冻结面 diff」这条机判判据开口子 |

## 失败项与路由建议

| # | 项 | 路由 |
|---|---|---|
| **F-1** | **`cep-usage.md §12.5` 的「闸清单」段仍写「**44** 个可执行 `scripts/test/cep_*.sh`**（磁盘 **45** 个）」** —— 与 v5 实测（**48** `.sh` + **14** `.py`）**不符** | ⛔ 本单元**不碰**（`docs/guide/cep-usage.md` 不在 M1 冻结集，但属**另一样本面**）⇒ **登记为观察项**，交 planner 决定是否单独立项（⛔ 不得在本单元顺手改 —— 否则五步链的授权范围与实际改动面不一致） |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次改动 | ✅ 4 个 commit：立项（`68025dc0`）· 台账授权（`d2094a08`）· 产物（`d860430a`）· 清单（`e92aa61d`）+ 载体表 + 本报告 |
| 临时诊断代码 / 临时文件 | ✅ **无**（全在 `/tmp/opencode/`；仓内零残留） |
| TODO / 半截实现 / 被 skip 的测试 | ✅ 无新增 |
| 未替换的 mock / 临时 fallback | ✅ 无 |
| 范围纪律 | ✅ 零 C++ / spec / test 改动 · 零 OpenSpec change · 零 KPI 占用 · ADR 正文零改动 · **未改 `cep-usage.md`**（虽它是 Q-1 的落点，但不在本单元授权面内） |
| 冻结面纪律 | ✅ 零新增 / 零移出 / 零排除 · `artifact_count` **90 → 90** · 授权 Entry 先于产物 commit · 清单单独 commit · 台账与清单未合并 |
| 载体表 `state` 纪律 | ✅ `W-3a` 行与本报告**同 commit** 落 `done` |

## 回归记录

| 检查 | 命令 | 结果 |
|---|---|---|
| 冻结面校验 | `bash scripts/test/cep_freeze_check.sh` | ✅ rc=0（90 个产物 hash 全部一致） |
| 索引行号闸 | `bash scripts/test/cep_index_position_check.sh` | ✅ rc=0 |
| 文档引用闸 | `bash scripts/test/cep_doc_ref_check.sh` | ✅ rc=0 |
| 架构文档同步闸 | `bash scripts/test/cep_docs_sync_check.sh` | ✅ rc=0 |
| ADR 结构闸 | `bash scripts/test/cep_adr_structural_check.sh` | ✅ rc=0 |
| 顺序机关 | `bash scripts/test/rddf_stage_gate.sh` | ✅ rc=0 |
| 活基线闸（W2-a） | `python3 scripts/test/cep_registry_baseline_check.py` | ✅ rc=0 |
| 旧口径残留闸（W2-b） | `bash scripts/test/cep_registry_stale_refs_check.sh` | ✅ rc=0 |
| 文档路径同步闸 | `bash scripts/test/docs_sync_check.sh --strict` | ✅ rc=0 |
| 全量 `cep_*` ctest | `ctest --test-dir build -R "cep_"` | ✅ **108/108 Passed** |

## 已用轮次

**1/3**（W3-a 实施侧自验第 1 轮 —— 8 条 AC 一次全绿；无返工轮）。
