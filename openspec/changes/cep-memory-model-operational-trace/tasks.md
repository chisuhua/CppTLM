## Tasks

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **时序钉**：⛔ 主 spec 的**整条替换**与实现**同批 commit**（delta 由主 spec **提取生成** ⇒ 零漂移）⇒ 五步链 1 次（Entry #542）。

### Task 0: 授权（先于改冻结面）

- [x] 台账 Entry **#542**（立项授权 · 前置闭合链 6 项 · backlog AC-7/8/12 修订登记）⇒ 已 commit
- [x] `openspec/changes/cep-memory-model-operational-trace/proposal.md`（planner 域）+ `.openspec.yaml` + `design.md`

### Task 1: 操作级轨迹模型（新增 `include/tlm/cep/memory_model_trace.hh`）

- [x] `Op` / `DomainDecl` / `AddrCoCandidate` / `AddrCoChoices` / `Trace`
- [x] 一致性标签词表（`ADR-CEP-12` 决策 3）+ `load_instance_labels()`（从 `protocol_instances.json` 抽声明面）
- [x] **决策 3 的授权表**：`allow_read` / `allow_write` / `domain_preserves_po`（并集）
- [x] `parse_trace()`：拒收面（派生边 / 通道边 / 畸形 / 重复 id / 自反 ppo / rf 自环 / 未绑定域 / 锚点 allowlist）

### Task 2: 工装改型（`include/tlm/cep/compound_graph_falsifier.hh`）

- [x] 判定面四态 + **原因码闭集**（11 码）
- [x] 消歧常量同步（`kFalsifierScope` ⇒ `operation-level-composite-graph`；⛔ `kFalsifierIsMemoryModelLitmus` 保持 `false`）
- [x] 公理面图（`ppo_com` / `rfe` / `ws` / `fr`）· **双图折衷**（域内 `rf` 只进观测面）
- [x] 候选枚举（逐地址写序候选的笛卡尔积 · odometer 进位）+ **∃/∀ 聚合**（短路）+ **截断**
- [x] `Decision` 增 `candidate_outcomes` / `axiom_edges` / `rf_edge_classes` / `cross_domain_edge_classes`

### Task 3: 语料与测试

- [x] `test/cep_memory_model_traces.json`（5 条迹：金标准 / companion / 写序二择一 / 单候选派生面 / `rf` 域内 / `rf` 跨域）
- [x] `test/test_cep_compound_graph_falsification.cc` 改写（6 个用例 · 91 断言）
- [x] AC-2/3①/3②/3③/4/5/6/7/8/9/10 的机检面

### Task 4: 闸与登记

- [x] `scripts/test/cep_compound_graph_falsification_check.sh` **臂替换**（旧→新映射表落进脚本注释 · 六条臂）
- [x] `scripts/test/cep_litmus_naming_check.sh` **臂④ 同步**（判据对象 ⇒ 操作级作用域标记；⛔ 文件名与红因码保留）
- [x] `scripts/README.md` 守门表两行更新

### Task 5: 契约面（**冻结面** · 与实现同批）

- [x] 主 spec **整条替换**（`openspec/specs/cep-compound-graph-falsification/spec.md`）
- [x] delta spec **从主 spec 提取生成**（`## MODIFIED Requirements` ⇒ 零漂移）
- [x] `openspec validate cep-memory-model-operational-trace --strict` rc=0
- [x] AC-12 判据：`grep -c '(instance, network, vc)' openspec/specs/cep-compound-graph-falsification/spec.md` == 0

### Task 6: 五步授权链（Entry #542）

- [x] 步 1：台账 Entry #542 先落 ✅
- [x] 步 2：产物 commit
- [x] 步 3：`cep_freeze_check.sh --update --authorized-by "Entry #542"` ⇒ 期望「**新增 0 · 移出 0 · hash 变更 1**」· `artifact_count` **92 → 92**
- [x] 步 4：冻结清单**单独** commit
- [x] 步 5：无参复验 `cep_freeze_check.sh`（rc=0 · 92 产物 hash 一致）

### Task 7: 验收与归档

- [x] 本地验收（verifier）：17 条 AC 逐条取证 · 遗留清扫
- [x] `ctest -R cep` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · `cep_docs_sync_check.sh --strict` rc=0
- [x] 归档 `openspec archive cep-memory-model-operational-trace --yes` ⇒ 验证「主 spec 的 Purpose **非占位**」
- [x] 计数 / KPI 回填（active 1 → 0 · KPI 1/3 → 0/3）
- [x] 台账 Entry #542 收口 + 验收报告落 `.rddf/reviews/`
