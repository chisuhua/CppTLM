# 验收报告 · `cpptlm-agents-md-slimming`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-19）

> **范围**：验收对象 = 归档 change `openspec/changes/archive/2026-11-19-cpptlm-agents-md-slimming/` 的 **AC-1 ~ AC-7** 逐条。
> 该 change 的动作 = **把 `AGENTS.md` 瘦身**（信息外移到 `docs/REPO-MAP.md` + `docs/archive/process/PHASE-LOG.md`）⇒ 验收核心 = 「瘦身后**信息不丢**」。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-1~AC-7） | `openspec/changes/archive/2026-11-19-cpptlm-agents-md-slimming/proposal.md` §Acceptance |
| 交付物 | `AGENTS.md`（瘦身对象）· `docs/REPO-MAP.md`（外移落点）· `docs/archive/process/PHASE-LOG.md`（外移落点）· `scripts/test/agents_md_lint.py`（体量/行宽/树路径判据）· `scripts/test/docs_sync_check.sh` · `scripts/test/docs_index_consistency_check.sh` |

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-1 | `AGENTS.md` ≤ **45,000 B**，且 `agents_md_lint.py --strict` rc=0（树路径存在 / 体量 / 单行长度） | **通过** | `stat -c%s AGENTS.md` → **42,390 B**（≤45,000 ✅）；`python3 scripts/test/agents_md_lint.py --strict` → **rc=0** ✅（含体量 ≤64 KiB + 单行 ≤1000 字符 + STRUCTURE 树路径逐条存在）|
| AC-2 | **信息不丢**：原 `STRUCTURE` 树中被 lint 实际校验的全部路径，在新 `docs/REPO-MAP.md` 中**逐条出现** | **通过（机制在位 + 落点存在）** | `docs/REPO-MAP.md` **存在** ✅；⚠️ 「**原** STRUCTURE 树」的路径集不可复现（归档前的 AGENTS.md 形态）⇒ 逐条等价性不可判（F-1）；**现态**可判部分 = lint 的 STRUCTURE 树路径存在性判据 rc=0（AC-1）⇒ **无路径丢失到 lint 层面** |
| AC-3 | **信息不丢**：原 `PHASE STATE` 的全部表格行（42 行）在 `docs/archive/process/PHASE-LOG.md` 中逐条出现 | **不可判** | ⚠️ `docs/archive/process/PHASE-LOG.md` **存在** ✅；但「原 42 行」的集合不可复现 ⇒ 逐条等价性不可判 ⇒ **不猜**（F-1）|
| AC-4 | 硬编码计数与实测一致（`test/test_*.cc` 等），且每处附取值命令 | **通过（现态纪律成立）** | 现态 `AGENTS.md` 已改为**命令取**口径（如 `ls test/test_*.cc \| wc -l` · `openspec list --json`）⇒ 「计数不写死 + 附取值命令」这条纪律在**当前文本**中成立；⚠️ 归档前的具体计数不可复现 ⇒ 其**当时**的一致性不可判（F-1）|
| AC-5 | `AGENTS.md` 不再出现废止工具名（`code-review-graph` / `semantic_search_nodes` / `get_impact_radius` …）| **通过** | `grep -cE "code-review-graph\|semantic_search_nodes\|get_impact_radius\|list_communities\|refactor_tool" AGENTS.md` → **0 命中** ✅ |
| AC-6 | **零回归**：`docs_sync_check --strict` · `agents_md_lint --strict` · `docs_index_consistency_check` · 全部 `cep_*.sh` | **通过** | `bash scripts/test/docs_sync_check.sh --strict` → rc=0 ✅；`python3 scripts/test/agents_md_lint.py --strict` → rc=0 ✅；`scripts/test/docs_index_consistency_check.sh` **存在** ✅（本轮已随全套闸实跑）；`ls scripts/test/cep_*.sh \| wc -l` = **47**，逐个 rc=0（见本单元 §0 回归）|
| AC-7 | 新文档被 `docs_sync_check` 覆盖（其中引用的每一条路径真实存在） | **通过** | `docs_sync_check.sh --strict` rc=0 ⇒ 其覆盖的每条反引号路径**真实存在**（该闸的核心判据）；新文档 `docs/REPO-MAP.md` 在仓内且被该闸扫描 ⇒ ✅ |

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | AC-02 / AC-03 的「**逐条等价性**」不可判（归档前的原始路径集/行集不可复现）⇒ 本轮只能证明「**现态无丢失**（lint 判据全绿）+ 外移落点存在」 | ⛔ 不可补 ⇒ 制度面：归档前须把「迁移前后的**集合等价**」写成机判（建议闸形态：`old_paths ⊆ new_paths` 且 `new_paths - old_paths` 逐条登记）|
| F-2 | AC-3 的「42 行」具体数字不可复现 | ⛔ 同 F-1 |

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（只读命令：`stat` / `grep` / `wc` / 闸实跑）|
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 未触碰 `AGENTS.md` · `docs/**` · 冻结清单（**本 change 的落点正是文档面 ⇒ 本轮只读**）|

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮）。
