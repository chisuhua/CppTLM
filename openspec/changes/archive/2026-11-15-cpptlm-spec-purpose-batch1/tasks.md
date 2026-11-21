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

- [x] **T1.1** 写 `scripts/test/cep_spec_purpose_batch1_check.py`（D-3 的 **C-1 ~ C-7**：限段占位 / 长度+单行 / **LCS<12** / 禁过程叙述与字面量 / `claim_kind` 互斥 / 标识符存在性 / 归因闭合）
- [x] **T1.2** 写归因载体**原始输入**（`test/spec_purpose_batch1_attribution.json` 的 `items[]`：`spec` / `claim_kind` / `source_change` / `source_evidence` / `requirement_titles` / `purpose_text` / `waiver`）—— ⛔ **不手填 `checks`**（由 T1.1 脚本生成）
- [x] **T1.3** 逐份**只替换 Purpose 段**：正则 `## Purpose\n(.*?)\n## Requirements` 整段替换 ⇒ **Requirements 段逐字节不动**；**单物理行**、**不新增空行**
- [x] **T1.4** 跑 T1.1 脚本 ⇒ **16/16 全绿**（C-1~C-7）

## Phase 2 · 复验

- [x] **T2.1** **AC-01** 限段占位计数 = 0
- [x] **T2.2** **AC-02** 16/16 份 `--strict --json` 无 Purpose 类 issue
- [x] **T2.3** **AC-02b** 全局闭合：**pre 77 → post 65 ⇒ k = 12**（本批 **12 份**完全转绿）；本批 16 份的 Purpose 占位**全清**（全仓 75 → **59** = −16 精确闭合）；批外仍红 **61** 份，红因**全为「正文 >500」**（Purpose 类 0 条残留于批外）
- [x] **T2.4** **AC-03** 四条正交（脚本判定 + 人工抽验 ≥3 份）
  - **人工抽验名单与结论（2026-11-15 · 4 份跨 4 组）**：
    ① `pcie-ip-microarch`（组② · 11 条 Requirement · 伞形）—— 源 proposal 为**伞形 change**（「不直接实施，仅产出设计文档 + 决策 + 路线图」）⇒ Purpose 取「**划定可观察行为边界**」的**规范用途**表述，⛔ 未声称「已交付实现」⇒ **一致**
    ② `pcie-slice-prerequisites`（组① · 含悬空引用 R-12）—— Purpose 的「由仓内**编译期断言**守护」⇒ 仓内 `test/test_pcie_slice_wire_format_snapshot.cc` **存在**且含 **29 条 `static_assert`** ⇒ 属**当前事实** ⇒ **一致**（⚠️ 该 spec `:14` 引用已删除的 `include/cudart/abi_guards.h` ⇒ 已登记 R-12，**未写进 Purpose**）
    ③ `cpptlm-stage-1-2-msix`（组③ · 历史动机比重高）—— Purpose 的「此前回调虽已接线却全仓无调用方」⇒ 源 proposal `:21` 逐字有「intr_cb 真实接线但 `trigger_irq_async` 全仓无调用方」⇒ **动机陈述与源一致**，且属**合法 why-范畴**（⛔ 非「被审查确认」式过程叙述）⇒ **一致**
    ④ `cpptlm-pcie-endpoint-ip-json-config`（组④）—— Purpose 的「以告警形式暴露并回退到缺省取值后继续运行」⇒ spec 的 Scenario 逐字为 `warning "…must be 256 or 4096; got 1024, falling back to 4096"` ⇒ **一致**；且 Purpose ⛔ 未声称任何 Python 工具能力（与源 proposal「本次只解决 C++ 端消费，**不动 Python 工具**」边界一致）⇒ **一致**
  - **结论**：4/4 **一致** ⇒ 无「声称不存在的能力」。⚠️ **机判边界**（design §D-3 C-6）：**纯中文的能力声称不在机判射程内** ⇒ 该边界由本次人工抽验 + critic 的 4 组逐组签署共同承担（批 2 起须保留同等人审环节）。
- [x] **T2.5** **AC-04** diff 形状：16 文件 · 每份 1 hunk · 删 1 增 1 · 不新增空行
- [x] **T2.6** **AC-05** `claim_kind` 互斥 + 标识符存在性 + `source_evidence` 全 `isfile`
- [x] **T2.7** **AC-06/07** 43 份 `cep-*` `--strict` rc≠0 = 0 · `artifact_count` 86 不变 · 范围零 diff
- [x] **T2.8a** **连带**:台账登记 R-10/R-12/R-14 时引用了 4 条**已删除路径** ⇒ `cep_doc_ref_check.sh` 报红 ⇒ 按 `AGENTS.md`「提及已删除文件须登记 `VIRTUAL_PATHS`」约定补 **4 条**（`include/cudart/`·`abi_guards.h`·`cpptlm_bridge.h`·`19-pcie-ip-microarchitecture.md`，各带理由）⇒ `doc_ref --strict` 恢复绿
- [x] **T2.8** **新残余登记**（R-10 ~ R-14 已写入 improvement「残余」表）（本 change 暴露的既有缺口，不在本 change 修）：**R-10** superseded spec 存在性裁定 · **R-11** 4 份同源 spec 内容重复 · **R-12** `abi_guards.h` 悬空引用 · **R-13** spec 内部旧状态行 · **R-14** `19-pcie-ip-microarchitecture.md` 已迁 ArchForge

## Phase 3 · 审查与归档

- [x] **T3.0** **归档语义备案**：实测已收敛为**唯一路径 = 出口 A**（`skip_specs: true` 是 `validate --changes --strict` 保持绿的必要条件 ⇒ 出口 B 会破坏校验基线；工具 INFO 措辞「no spec-level behavior changes」与本 change 精确匹配）⇒ 落**标记行 + 归因 JSON `archive_semantics` 字段**，并请 architect 确认 `AGENTS.md` 分类表是否补记该变体（⛔ 本 change 不改 `AGENTS.md`）
- [x] **T3.1** critic 审（`arch` 档；**按 4 组签**：① `pcie-*` 数据通路 ② `pcie-*` 集成与微架构 ③ `cpptlm-stage-*` ④ `cpptlm-*` 其他 —— 一组不过只回退该组）
- [x] **T3.2** 影子根**预演**归档 ⇒ main spec 零改动 + 归档无警告
- [x] **T3.3** 真仓归档（按 T3.0 的出口）+ 复验 16 份 `--strict`
- [x] **T3.4** **AC-08** 闸 0 回归（**归档前跑一次**：45 闸 + `ctest -R cep` + `[cep]` == T0）
- [x] **T3.5** 产物 commit
- [x] **T3.6** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 归档数 **149 → 150** + 台账闭合回执 Entry **#442**
- [x] **T3.7** 闸最终复验 + `git status` 全清
- [ ] **T3.8** verifier 独立验收（报告落 `.rddf/reviews/cpptlm-spec-purpose-batch1-verify.md`）

## Phase 4 · 遗留

- [x] **T4.1** 提请 architect：归档分类表补「✅ 已完成归档 · 主 spec 直改（无 delta）」变体 ⇒ 补进 `AGENTS.md`（⚠️ `AGENTS.md` 属治理面 ⇒ **本 change 不改**）
- [x] **T4.2** 提请 architect：**R-10**（superseded spec 存在性裁定）· **R-11**（4 份同源 spec 合并/退役）· **R-9**（清单增殖 canonical）· **R-2**（13 键 golden）
