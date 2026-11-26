## Tasks

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」。
> **时序钉（不可拆）**：⛔ spec 改名 · 闸扫描集改路径 · 语料改名 · `test_cep_combination_contract.cc:312` 单行替换 ⇒ **同批 commit**（critic 硬约束：否则新 FAIL 逻辑 + 指向未建的新 spec ⇒ 归档前全程红）。

### Task 0: 台账授权（须先于改冻结面）

- [x] 台账 `.rddf/roadmap/objectives/obj-cep-platform.md` 追加 **Entry #N**（authorizer = architect 裁定记录）· 含**冻结面移出声明行**（`cep_freeze_check.sh` C-1b 硬要求）
- [x] commit 授权（产物 commit 之前）

### Task 1: 闸缺陷修复（前置 commit · 可独立）

- [x] `scripts/test/cep_litmus_naming_check.sh`：**扫描集缺失 ⇒ FAIL**（HDR 不存在 ⇒ 整闸 SKIP 保持不变；HDR 在而 TESTF/SPEC 缺失 ⇒ 红因 `litmus-scan-set-missing`）
- [x] 臂② 新增**第④路**影子 fixture（扫描集缺一成员 ⇒ 必须红）
- [x] 闸头注释追加「**必不改**保留理由」（文件名 + 红因码 + 阶段 1 审计链指针）

### Task 2: 活代码面改名（同批 commit B）

- [x] `git mv` 头文件 + 测试文件 + 语料文件 + 既有闸文件（4 个）
- [x] 头文件：3 个符号 + include guard 改名；⛔ **消歧段逐字保留**
- [x] 测试：Catch2 tag 族 ⇒ `[cep][compound-graph][falsification][*]` + 符号更新
- [x] 既有闸：env var `CPPTLM_LITMUS_{CORPUS,REPORT}` ⇒ `CPPTLM_COMPOUND_GRAPH_{CORPUS,REPORT}`（**生产/消费两侧同批**）· tag 过滤串 · `CORPUS` 路径 · 闸头（历史 change 名**保留**）
- [x] 命名消歧闸：扫描集 3 路径 + probe include + sed 影子路径同步（⛔ **文件名/红因码保留**）
- [x] **`test/test_cep_combination_contract.cc:312` 单行等长替换**（critic BLOCKER 闭合项；⛔ 长度须不变 ⇒ assertion map 行号有效）

### Task 3: 契约面退役 + 新增（冻结面 · 同批 commit B）

- [x] `git mv openspec/specs/cep-litmus-falsification/spec.md` ⇒ `openspec/specs/cep-compound-graph-falsification/spec.md`
- [x] Requirement 名 ⇒ `cep-compound-graph-falsification`
- [x] Purpose 按**四类措辞边界**改写：**逐字保留**三处诚实边界 · **必须改**「litmus 证伪工装」→「复合图证伪工装」等 · **必须增**改名沿革行（含真实 Purpose，⛔ 非占位）· ⛔ **不得引入**任何新 capability 声言
- [x] 4 个 Scenario：结构与全部否定句**逐字保留**；仅改「必须改」类措辞

### Task 4: delta spec（从实施期新 spec **提取生成** ⇒ 零漂移）

- [x] `specs/cep-litmus-falsification/spec.md`（## REMOVED Requirements）—— 从 `git show HEAD:<旧 spec>` 提取
- [x] `specs/cep-compound-graph-falsification/spec.md`（## ADDED Requirements）—— **从实施期新 spec 提取**（单一来源）
- [x] `.openspec.yaml`：`retire_capabilities: true`（openspec 硬要求，per 探针 P-1/P-2）
- [x] `openspec validate cep-litmus-falsifier-compound-graph-rename --strict` rc=0

### Task 5: 登记与同步

- [x] `test/CMakeLists.txt`：2 个 add_test 条目 + `:802` 注释措辞
- [x] `scripts/README.md`：守门表 1 行 + §1 枚举
- [x] `.rddf/roadmap/cep-gen-family-gap-closure.md`：F5 真值表路径 + Stage 标注
- [x] `.rddf/improvements/cep-memory-model-operational-trace.md`：6 处旧路径引用修订（**planner 域**）

### Task 6: 冻结面链（commit C + D）

- [ ] `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ 输出须为「**新增 1 · 移出 1 · hash 变更 0**」· `artifact_count` **91 → 91**
- [ ] 冻结清单**单独** commit
- [ ] `bash scripts/test/cep_freeze_check.sh`（无参）rc=0

### Task 7: 验收与归档

- [ ] 机检 ①：`grep -rn "cep_litmus\|litmus_falsifier" include/ test/ scripts/ openspec/specs/` 命中项 ⊆ 「必不改」登记表
- [ ] 机检 ②：改名后 `[cep][compound-graph][falsification]` 全绿 + 既有闸两臂 rc=0 + `test_cep_combination_contract` 绿
- [ ] 机检 ④：扫描集缺一成员 ⇒ 闸必红（`litmus-scan-set-missing`）
- [ ] 人工 ①②：三处诚实边界逐字在场 · invariant 载体对账
- [ ] 机检 ⑥：`ctest -R cep` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · `cep_docs_sync_check.sh --strict` rc=0
- [ ] 归档 `openspec archive cep-litmus-falsifier-compound-graph-rename --yes` ⇒ 验证「`openspec/specs/cep-litmus-falsification/` **无残留空壳**」+ 新 spec Purpose **非占位**
- [ ] 计数 / KPI 回填（归档 172→173 · CEP 56→57 · 闸 50→51 ⇒ **51** · 闸族 64→65 · spec 总数 **139 不变** · `cep-*` **48 不变** · KPI 1/3 → **0/3**）
- [ ] 台账 Entry 收口 + 验收报告落 `.rddf/reviews/`
