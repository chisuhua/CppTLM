# tasks: cpptlm-spec-purpose-batch3

> **AC 映射**: proposal AC-01 ~ AC-10 · design D-0 ~ D-5
> **上游**: improvement `cpptlm-spec-purpose-batch3.md` · 台账 **Entry #444**
> **复用**: 批 1 的 D-0 写作规则 + 校验脚本范式 + 归因载体范式 + critic 分组签 + verifier 独立实现

---

## Phase 0 · 立项态（planner）

- [x] **T0.1** 实测 R-1 剩余构成：`--strict` 红 **65** 份 = **59** 份 Purpose 占位 + **69** 条「正文 >500」issue（落 **26** 份）；`cep-*` 43/43 --strict 绿；非 strict 口径 129/129 绿
- [x] **T0.2** 按**能力域**聚类剩余 59 份 ⇒ **4 组**（D1 PCIe 物理链路 5 / D2 框架端口配置 20 / **D3 dGPU 板级 SoC 17** / D4 SM-GPU-命令驱动 17）；**批 3 = D3**
- [x] **T0.3** 事实基础提取：17 份的 Requirement 标题 + 来源 proposal「为什么」段 + **逐条可追性核对**（design D-1 末）
- [x] **T0.4** **风险预扫描**（16 份有标记）⇒ 逐条核实 ⇒ **本批 0 份 spec 级 superseded、0 份单 Requirement、0 份需 waiver**
- [x] **T0.5** **17 份 canonical Purpose 草稿**（design §D-1；写作规则沿用批 1 的 D-0）
- [x] **T0.6** **判据差异登记**：C-6 收紧到 **spec 级** superseded（批 1 口径会假红，8/9 命中是行级历史引用；批 1 已归档**不追溯**）· C-7 扩展 `evidence_kind`（2 份主 spec 直写无归档目录）· ⛔ **不用 TBD 行做 glob 键**
- [x] **T0.7** improvement + proposal + design + 本 tasks + `.openspec.yaml`（`skip_specs: true`）+ 台账 **Entry #444**
- [ ] **T0.8** **metis 预审**（planner 的固定审查者）

## Phase 1 · 落地（builder）

- [ ] **T1.1** 写归因载体**生成器**（独立脚本）：从 `design.md` §D-1 表格**机器提取** 17 条 canonical 草稿（⛔ 不手抄）+ **glob 解析**来源归档目录（⛔ 不依赖 TBD 行）+ 从 spec **自动抽取** `requirement_titles` ⇒ 写 `test/spec_purpose_batch3_attribution.json`（**⛔ 不手填 `checks`**）
- [ ] **T1.2** 写 `scripts/test/cep_spec_purpose_batch3_check.py`（D-3 的 **C-1 ~ C-7**；带 `--apply` 落地模式）
- [ ] **T1.3** 逐份**只替换 Purpose 段**：正则整段替换 ⇒ **Requirements 段逐字节不动**；**单物理行**、**不新增空行**
- [ ] **T1.4** 跑 T1.2 脚本 ⇒ **17/17 全绿**（C-1~C-7）
- [ ] **T1.5** **判别力验证**：落地**前**跑校验 ⇒ 须 **0/17 绿**（占位残留被 C-1 抓到）⇒ 判据有牙

## Phase 2 · 复验

- [ ] **T2.1** **AC-01** 限段占位计数 = 0
- [ ] **T2.2** **AC-02** 17/17 份 `--strict --json` 无 Purpose 类 issue
- [ ] **T2.3** **AC-02b** 全局：`pre − post = k` 且 k 与剩余红数**逐份归因闭合**
- [ ] **T2.4** **AC-03** 四条正交（脚本判定 + **人工抽验 ≥3 份**跨组；结论**落盘**到本 tasks）
- [ ] **T2.5** **AC-04** diff 形状：17 文件 · 每份 1 hunk · 删 1 增 1 · 不新增空行 · `## Requirements` 段逐字节不变
- [ ] **T2.6** **AC-05** `claim_kind` 与 spec 级 superseded 互斥 + 标识符可追 + `source_evidence` 全部 `isfile` 且按 `evidence_kind` 命中
- [ ] **T2.7** **AC-06/07** 43 份 `cep-*` `--strict` rc≠0 = 0 · `artifact_count` 86 不变 · 范围零 diff
- [ ] **T2.8** **新残余登记**：**R-15**（`v1-architecture` 引用已迁 ArchForge 的 `00-overview.md`）等

## Phase 3 · 审查与归档

- [ ] **T3.0** **归档语义备案**：出口 A（实测收敛为唯一路径）⇒ 落 **标记行 + 归因 JSON `archive_semantics`**（⛔ 本 change 不改 `AGENTS.md`）
- [ ] **T3.1** critic 审（`arch` 档；**按 4 组签**：① 板级外观与 ABI 卫生（4 份）② SoC 控制面与物理边界（2 份）③ 板卡/SoC 时序与 ABI 落地（4 份）④ 内部互联与搬运（7 份）—— 一组不过只回退该组）
- [ ] **T3.2** 影子根**预演**归档 ⇒ main spec 零改动 + 归档无警告
- [ ] **T3.3** 真仓归档（出口 A）+ 复验 17 份 `--strict`
- [ ] **T3.4** **AC-08** 闸 0 回归（**归档前跑一次**：45 闸 + `ctest -R cep` + `[cep]` == T0）
- [ ] **T3.5** 产物 commit
- [ ] **T3.6** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 归档数 **150 → 151** + 台账闭合回执 Entry **#445**
- [ ] **T3.7** 闸最终复验 + `git status` 全清
- [ ] **T3.8** verifier 独立验收（报告落 `.rddf/reviews/cpptlm-spec-purpose-batch3-verify.md`；**须另写独立实现复算**）

## Phase 4 · 遗留

- [ ] **T4.1** 提请 architect：R-9（清单增殖 canonical）· R-2（13 键 golden）· R-10（superseded spec 存废）· R-11（4 份同源 spec 合并退役）· **DoD 7 口径订正**（66 条 invariant 实为 61 条有载体 + 5 条 ⏭）· `AGENTS.md` 归档分类表补记「✅ 主 spec 直改（无 delta）」变体
- [ ] **T4.2** 提请 architect：**R-1 批 2**（26 份正文 >500 裁剪 —— 会动 `## Requirements` 段 ⇒ 另一档风险，须另立 change）
