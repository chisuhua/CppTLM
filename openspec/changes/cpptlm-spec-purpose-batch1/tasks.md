# tasks: cpptlm-spec-purpose-batch1

> **AC 映射**: proposal AC-01 ~ AC-10 · design D-0 ~ D-5
> **上游**: improvement `cpptlm-spec-purpose-batch1.md` · 台账 **Entry #441**
> **metis 预审**: 7 MUST + 8 SHOULD + 5 NIT 全处置（5 条推翻初稿事实 ⇒ 范围 17→16、AC-02/03/05 换判据、Entry #441 真实落盘、3 处来源路径订正）

---

## Phase 0 · 立项态（planner）

- [x] **T0.1** 实测 R-1 构成：非 `cep-*` 86 份中 **75** 份 Purpose 占位（全同一形态）+ **70** 份正文 >500；`cep-*` 43/43 --strict 绿；非 strict 口径 129/129 绿
- [x] **T0.2** 批 1 范围 = **16** 份（`pcie-*` 10 + `cpptlm-*` 6）；`cpptlm-d1-p1-pipeline-scoreboard` **已 superseded ⇒ 移出**（R-10）
- [x] **T0.3** 事实基础提取：16 份的来源 change proposal「为什么」段 + Requirement 标题
- [x] **T0.4** **写作规则 D-0**（7 条，含 superseded 一律移出 / 同源派生锚定独占内容 / 单 Requirement 豁免）
- [x] **T0.5** **16 份 canonical Purpose 草稿**（design §D-1；每条附事实依据）
- [x] **T0.6** **归档载体实验**：① delta 的 `## Purpose` 被忽略 ⇒ 0 份 delta 是唯一路径 ② 无 delta + `skip_specs: true` ⇒ 归档无警告 + main spec 零改动
- [x] **T0.7** improvement + proposal + design + 本 tasks + 台账 **Entry #441**（**真实落盘**）· KPI **0/3 → 1/3**
- [x] **T0.8** **metis 预审**（7 MUST + 8 SHOULD + 5 NIT 全处置，见 improvement 对照表）

## Phase 1 · 落地（builder）

- [ ] **T1.1** 写 `scripts/test/cep_spec_purpose_batch1_check.py`（D-3 的 **C-1 ~ C-7**：限段占位 / 长度+单行 / **LCS<12** / 禁过程叙述与字面量 / `claim_kind` 互斥 / 标识符存在性 / 归因闭合）
- [ ] **T1.2** 写归因载体**原始输入**（`test/spec_purpose_batch1_attribution.json` 的 `items[]`：`spec` / `claim_kind` / `source_change` / `source_evidence` / `requirement_titles` / `purpose_text` / `waiver`）—— ⛔ **不手填 `checks`**（由 T1.1 脚本生成）
- [ ] **T1.3** 逐份**只替换 Purpose 段**：正则 `## Purpose\n(.*?)\n## Requirements` 整段替换 ⇒ **Requirements 段逐字节不动**；**单物理行**、**不新增空行**
- [ ] **T1.4** 跑 T1.1 脚本 ⇒ **16/16 全绿**（C-1~C-7）

## Phase 2 · 复验

- [ ] **T2.1** **AC-01** 限段占位计数 = 0
- [ ] **T2.2** **AC-02** 16/16 份 `--strict --json` 无 Purpose 类 issue
- [ ] **T2.3** **AC-02b** 全局：`pre − post = k` 且 k 与剩余红数**逐份归因闭合**
- [ ] **T2.4** **AC-03** 四条正交（脚本判定 + 人工抽验 ≥3 份）
- [ ] **T2.5** **AC-04** diff 形状：16 文件 · 每份 1 hunk · 删 1 增 1 · 不新增空行
- [ ] **T2.6** **AC-05** `claim_kind` 互斥 + 标识符存在性 + `source_evidence` 全 `isfile`
- [ ] **T2.7** **AC-06/07** 43 份 `cep-*` `--strict` rc≠0 = 0 · `artifact_count` 86 不变 · 范围零 diff
- [ ] **T2.8** **新残余登记**（本 change 暴露的既有缺口，不在本 change 修）：**R-10** superseded spec 存在性裁定 · **R-11** 4 份同源 spec 内容重复 · **R-12** `abi_guards.h` 悬空引用 · **R-13** spec 内部旧状态行 · **R-14** `19-pcie-ip-microarchitecture.md` 已迁 ArchForge

## Phase 3 · 审查与归档

- [ ] **T3.0** ⚠️ **归档语义硬门**：architect 裁决 **出口 A 或 B**（二选一）⇒ 落标记/台账记录（AC-10）—— **不裁决则不得归档**
- [ ] **T3.1** critic 审（`arch` 档；**按 4 组签**：① `pcie-*` 数据通路 ② `pcie-*` 集成与微架构 ③ `cpptlm-stage-*` ④ `cpptlm-*` 其他 —— 一组不过只回退该组）
- [ ] **T3.2** 影子根**预演**归档 ⇒ main spec 零改动 + 归档无警告
- [ ] **T3.3** 真仓归档（按 T3.0 的出口）+ 复验 16 份 `--strict`
- [ ] **T3.4** **AC-08** 闸 0 回归（**归档前跑一次**：45 闸 + `ctest -R cep` + `[cep]` == T0）
- [ ] **T3.5** 产物 commit
- [ ] **T3.6** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 归档数 **149 → 150** + 台账闭合回执 Entry **#442**
- [ ] **T3.7** 闸最终复验 + `git status` 全清
- [ ] **T3.8** verifier 独立验收（报告落 `.rddf/reviews/cpptlm-spec-purpose-batch1-verify.md`）

## Phase 4 · 遗留

- [ ] **T4.1** 提请 architect：归档分类表补「✅ 已完成归档 · 主 spec 直改（无 delta）」变体 ⇒ 补进 `AGENTS.md`（⚠️ `AGENTS.md` 属治理面 ⇒ **本 change 不改**）
- [ ] **T4.2** 提请 architect：**R-10**（superseded spec 存在性裁定）· **R-11**（4 份同源 spec 合并/退役）· **R-9**（清单增殖 canonical）· **R-2**（13 键 golden）
