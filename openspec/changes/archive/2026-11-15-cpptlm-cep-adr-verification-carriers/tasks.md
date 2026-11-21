# tasks: cpptlm-cep-adr-verification-carriers

> **范围**:`ADR-CEP-21` 位宽状态机器锁 + `ADR-CEP-20` 纳入决策块闸覆盖 · **零 `include/tlm/cep/**` 改动 · 零闸代码改动 · 零 ADR 改动**
> **AC 映射**:proposal AC-01 ~ AC-10 · design D-1 ~ D-5
> ⚠️ **零实现层改动** —— 本 change 只动 `test/` + `scripts/test/` + 2 份 delta spec + 1 份真源文档

---

## Phase 0 · 立项态核对(planner 域已完成)

- [x] **T0.1** `openspec validate cpptlm-cep-adr-verification-carriers --strict` ⇒ **rc=0**(零 WARNING)
- [x] **T0.2** improvement v2(metis 14 MUST + 9 SHOULD + 6 NIT 全处置)+ change 目录 + proposal + **2 份 delta spec**
- [x] **T0.3** 台账 **Entry #431** 落盘(含**授权快照覆盖集合 +1**)· KPI **0/3 → 1/3**(6 处活口径 + obj 并发句)
- [x] **T0.4** `design.md` + `tasks.md` 落盘(builder 域)

## Phase 1 · W1 判据(先写判据,后写 fixture ⇒ TDD)

- [x] **T1.1** `test/test_cep_sharer_tracker_base.cc`:新增 1 个 TEST_CASE `[cep][sharer-tracker] responder 位宽常量与掩码数值锚`
  - `REQUIRE(kResponderMappedBits == 4u)` —— 宽度状态钉死
  - `REQUIRE(kMappedResponderMask == 0xFu)` —— **数值锚**(⛔ **不得**写恒等式 `== (1 << kResponderMappedBits) - 1`)
  - ⛔ **不**重复 `static_assert(kResponderMappedBits <= 64)`(已在 `sharer_tracker.hh:109`)
  - ⛔ **不**新增测试文件(`test/CMakeLists.txt:6` 的 GLOB 在 configure 时展开)
- [x] **T1.2** `cmake --build build` ⇒ rc=0 且**零新增 warning**
- [x] **T1.3** `./build/bin/cpptlm_tests '[cep][sharer-tracker]'` ⇒ 绿

## Phase 2 · W2 快照(⚠️ 依赖 T0.3 的 Entry #431 已落)

- [x] **T2.1** ⚠️ **先独立实测期望值**(⛔ 禁用外部 `grep`):
  `python3 -c "import importlib.util as u; s=u.spec_from_file_location('g','scripts/test/cep_decision_block_check.py'); g=u.module_from_spec(s); s.loader.exec_module(g); print(g.analyze_section2(g.section2(g.strip_fences(open('docs/adr/ADR-CEP-20-topology-ic3-evidence-strength.md').read()))))"`
  ⇒ 期望 `(6, [])`;⚠️ **若 ≠ 6 ⇒ 以闸口径为准并在本 change 内订正**
- [x] **T2.2** `test/cep_decision_block_snapshot.json`:`files` 加 `ADR-CEP-20-topology-ic3-evidence-strength.md: 6`;`count` → **23**;`total` → **131**;`authorized_by` → `Entry #431`
  - ⚠️ **⛔ 不得**把 `ADR-CEP-21-sharer-bitmap-expansion.md` 加进 `files`(无 `## 2` 决策章 ⇒ 结构性不适用 ⇒ 会造出「期望 0 块」的虚假断言)
  - ⚠️ 用 `json.dump` 写回(**⛔ 禁手工编辑** ⇒ 尾逗号会让整个闸红)
- [x] **T2.3** `bash scripts/test/cep_decision_block_check.sh` ⇒ **rc=0**(闸对 `ADR-CEP-20` 逐份比对 ⇒ 期望值错会立刻红)
- [x] **T2.4** 快照自洽自检:`count == len(files) == 23` 且 `total == sum(files.values()) == 131` 且 `authorized_by == 'Entry #431'`
- [x] **T2.5** `ADR-CEP-21` **不在**快照的负向断言(防未来误加)

## Phase 3 · W4 持久反向 fixture

- [x] **T3.1** 新建 `scripts/test/cep_adr_verification_carriers_reverse_verify.sh` —— **只写 `mktemp -d`** + `trap cleanup EXIT`;⚠️ **⛔ 不复用** wrapper 的固定影子路径 `/tmp/opencode/cep-decision-block`
- [x] **T3.2** ① 未毒化 ⇒ **绿**
- [x] **T3.3** ② **宽度毒化**:影子树复制 `include/tlm/cep/sharer_tracker.hh` ⇒ 改 4→8 ⇒ 编译**最小探针 TU** ⇒ 运行**退出码 ≠ 0** = 判红
  - ⚠️ **编译失败 ⇒ exit 2 环境错误**(⛔ 编译失败不得冒充判据失败)
  - ⚠️ 探针用**运行期判定** ⇒ 退出码即判决
- [x] **T3.4** ③ **覆盖毒化**:影子树复制 `docs/adr` ⇒ 删 `ADR-CEP-20` 一个 `### 决策 N` 段 ⇒ `python3 scripts/test/cep_decision_block_check.py --root <影子树>` ⇒ **rc≠0** 且输出**同时**含 `决策块计数快照偏离` 与 `ADR-CEP-20`
  - ⚠️ **⛔ 不毒化快照文件**(闸的 `load_snapshot()` 固定读真实仓根;`--root` 只重定向文档语料)
- [x] **T3.5** ④ 恢复态 ⇒ **绿**
- [x] **T3.6** 仓内零改动自证:**`sha256sum` 前后比对**目标文件(⛔ 不用 `git diff --quiet` —— 脏工作区会假红)
- [x] **T3.7** `test/CMakeLists.txt` 注册 `cep_adr_verification_carriers_reverse_verify` 进 ctest(`WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}`)⇒ ctest 条目数 **+1**

## Phase 4 · W6a 文档连带(⛔ 不含 07 文档)

- [x] **T4.1** `scripts/README.md`:新增脚本行(§CEP 契约守门脚本表 + 目录表脚本清单)
- [x] **T4.2** ⚠️ **⛔ 本阶段不改 `docs/architecture/07-coherence-exploration-platform.md`** —— 它 ∈ **M1 冻结面** ⇒ 归档前改会让冻结闸报红、污染「归档前全绿」口径;归档后改又必须**在 `--update` 之前** ⇒ 见 Phase 5 的 T5.4

## Phase 5 · 复验(0 回归)

- [x] **T5.1** `cmake --build build` ⇒ rc=0 且**零新增 warning**
- [x] **T5.2** `./build/bin/cpptlm_tests '[cep]'` ⇒ 绿;用例数 **+1**
- [x] **T5.3** `ctest --test-dir build -R 'cep' -j4` ⇒ **0 failures**;ctest 条目数 **+1**
- [x] **T5.4** `bash scripts/test/cep_adr_verification_carriers_reverse_verify.sh` ⇒ rc=0 且 4 闭环输出齐全
- [x] **T5.5** **43 个 `cep_*.sh`**(**按 ctest 同参数**:docs_sync / doc_ref 带 `--strict`)⇒ 全 rc=0
- [x] **T5.6** `openspec validate --changes --strict` rc=0 + `agents_md_lint` 通过
- [x] **T5.7** **范围零 diff 复核**:`include/tlm/cep/**` · `docs/adr/**` · `scripts/test/cep_decision_block_check.py` 全零 diff;新增 `INV-CEP-*` = **0**;新增 spec Requirement = **0**
- [x] **T5.8** ⚠️ **归档前冻结闸必绿**(delta 在 change 目录内 ⇒ **刻意不匹配**冻结 glob)⇒ **不得**拿「归档前 rc=0」当已合规

## Phase 6 · 归档 + 五步链 + KPI 回填

- [x] **T6.1** 台账 Entry(**归档期**那条,planner 域)= 归档授权记录
- [x] **T6.2** `openspec archive cpptlm-cep-adr-verification-carriers --yes`(⛔ **禁** `--skip-specs`;delta **合并**进 `openspec/specs/cep-sharer-tracker-base/spec.md` + `openspec/specs/cep-invariant-gate/spec.md`)
- [x] **T6.3** ⚠️ **W6b · 同步真源 1**(∈ **M1 冻结面**):`docs/architecture/07-coherence-exploration-platform.md:590` 的「22 份 ADR … 22 键 + `total`=125」→ **23 份 … 23 键 + `total`=131**
  - ⚠️ **必须在此刻**(archive **之后**、`--update` **之前**)⇒ 否则清单里它的 hash 是旧的 ⇒ 无参复验必红
- [x] **T6.4** 产物 commit(2 份 main spec 的 delta 合并 + 07 文档 + 其余产物)
- [x] **T6.5** `cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ 清单 diff **恰含 3 份产物**的 hash(`cep-sharer-tracker-base/spec.md` · `cep-invariant-gate/spec.md` · `docs/architecture/07-coherence-exploration-platform.md`)+ `authorized_by` + `frozen_at` + `frozen_at_commit` **元数据** ⇒ **无其他 artifact hash 变化**;`artifact_count` 不变(**86**)
- [x] **T6.6** 清单**单独** commit
- [x] **T6.7** 无参复验 `cep_freeze_check.sh` ⇒ rc=0 + provenance 一致
- [x] **T6.8** KPI **1/3 → 0/3** 回填(6 处活口径 + obj 的「当前 Proposed 并发」句;⚠️ 该句带 `**` 前缀 ⇒ **前次 grep 漏检过**,以 `docs_sync --strict` 为准)+ 体量数字(归档数 **145 → 146**)
- [x] **T6.9** 台账归档回执 Entry(planner 域)+ 5 项残余状态更新

## Phase 7 · 验收

- [ ] **T7.1** verifier 独立验收(proposal AC-01 ~ AC-10 逐条 + 工具证据)⇒ 报告落 `.rddf/reviews/cpptlm-cep-adr-verification-carriers-verify.md`
- [ ] **T7.2** 5 项残余状态确认:① F-1 全仓 87/129 份超长(backlog)② 镜像 20 值(architect)③ 覆盖集合无自愈(backlog)④ 编译期锁不可追(不追)⑤ ArchForge 缺席零保护(architect)
