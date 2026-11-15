# 验收报告:cpptlm-cep-flow-support-assertions

- **日期**:2026-11-15
- **角色**:verifier(只验不改)
- **对象**:已归档 change `cpptlm-cep-flow-support-assertions`(归档名 `2026-11-15-cpptlm-cep-flow-support-assertions`)
- **本轮**:第 **1** 轮(`tasks.md` T6.1)
- **总判定**:**有条件通过**(AC-01~AC-07 / AC-09 / AC-10 **PASS**;AC-08 **部分达成** ⇒ 见 F-1)

## 判据来源(按优先级)

1. `openspec/changes/archive/2026-11-15-cpptlm-cep-flow-support-assertions/proposal.md` 的 `## 验收标准`(**AC-01 ~ AC-10**)
2. `openspec/changes/archive/2026-11-15-cpptlm-cep-flow-support-assertions/tasks.md` 的 T6.1 / T6.2
3. 本次变更自己声明的行为(5 个 commit + 台账 Entry #427/#428/#429)
4. `design.md` D-1~D-7

> ⚠️ 本 change 的 delta **已合并进 main spec** ⇒ 归档后判据载体 = `openspec/specs/cep-core-contracts/spec.md`(per 台账 Entry #429 ⑤)。

---

## 逐条判定

### AC-01 期望表 = 6 实例 × 10 键 = 60 格 · 格数可机器验收 — **PASS**

| 证据 | 结果 |
|---|---|
| `test/cep_flow_support_expected.json` 独立复算 | `schema = cep-flow-support-expected/v1` · **实例数 = 6** · **格数 = 60** |
| 键集 ⟷ canonical 10 流 | ✅ 全符(逐实例 10 键且集合相等) |
| 值类型 | ✅ 全部 `bool`(无整数/字符串混入) |
| 域双向相等(表 ids == 声明源 `repo_owned_instance_ids`) | ✅ |
| 期望值 ⟷ 声明源**逐格一致**(独立复算,不依赖测试) | ✅ **不一致格数 = 0** |
| 「字面量而非派生」的机器化 | ✅ `REQUIRE(表实例数 == 6)`(:604 附近的域/唯一性断言)+ 逐实例 `DYNAMIC_SECTION`(`:621`)+ 期望值为 JSON 字面量载体(独立文件,非从声明源推导) |

### AC-02 每实例 `source` 可机器验收 · 3 族显式标「无逐值出处」· 禁笼统背书 — **PASS**

| 实例 | coverage | file 存在 | 锚点逐字命中 | `provenanced_keys` == ∪anchors | 覆盖度判据 | note |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| `ualink_4` | **full** | ✅ | ✅(4 条) | ✅ | ✅ ==10 键全集 | — |
| `chi_rnf` | partial | ✅ | ✅(1 条) | ✅ | ✅ 5/10 | 含「无逐值出处」 |
| `cxl_3_0_symmetric` | partial | ✅ | ✅(1 条) | ✅ | ✅ 1/10 | 含「无逐值出处」 |
| `cxl_2_0` | partial | ✅ | ✅(1 条) | ✅ | ✅ 1/10 | 含「无逐值出处」 |
| `tilelink_c` | **none** | ✅ 缺席 | ✅ 缺席 | ✅ 空 | ✅ | 含「无逐值出处」 |
| `nvlink_4` | **none** | ✅ 缺席 | ✅ 缺席 | ✅ 空 | ✅ | 含「无逐值出处」 |

- **禁笼统背书**:`source` 全文中**无**「能力映射」兜底措辞 ✅
- 逐字锚点已独立验真:`ADR-CEP-12` 的 4 条(`flow_support.request` / `.completion` 等)、`ADR-CEP-19:551`、`cep-cxl-dcoh-symmetric/spec.md:100`(含 `cxl_2_0` 的对照形态)⇒ **文件存在 + 锚点逐字命中**均实测

### AC-03 反向控制机器化四要件 — **PASS**

| 要件 | 证据 |
|---|---|
| ① 逐实例 `SECTION` | `test/test_cep_instance_declaration_contract.cc:618`(`for ... table.at("instances").items()`)+ `:621`(`DYNAMIC_SECTION("实例 " << id)`) |
| ② 被变异实例数 == 域大小 | `:741` `REQUIRE(mutated == owned.size())` |
| ③ 翻转值 ≠ 原值(防空转) | `:714` `const auto before = ...` + `:717` `REQUIRE(kv.value().get<bool>() != before)` |
| ④ 失败消息含 id + key + 期望 + 实际 | `:656` `INFO("实例 " << id << " / 流 " << key << ":期望 " << ... << " ≠ 声明源 " << ...)`;实测毒化输出:`实例 chi_rnf / 流 address_resolve:期望 0 ≠ 声明源 1` |
| ⛔ 禁「找到才断言」 | 域检查用**集合相等**(`:604` `REQUIRE(owned == tids)`),查表失败即 `:635` `REQUIRE(inst != nullptr)` ⇒ 无「遍历时找、找不到就跳过」形态 |

### AC-04 形状联判 + 事实订正 — **PASS**

- 形状联判取**代码真源**:`:593` `for (const auto* n : tlm::cep::kCanonicalFlowNames)`;`:651` `REQUIRE(dkeys == canonical)`;`:652` `REQUIRE(ekeys == canonical)`
- **事实订正复核**:`test/test_cep_schema.cc:265-275` 实测读的是 `load_schema()` → `s.at("properties").at("flow_support")` ⇒ 锁的是 **schema 形状**(`properties.size()` / `required` / `additionalProperties`),**不是任何实例** ⇒ proposal 的订正表述**成立** ✅

### AC-05 持久反向 fixture 必红 — **PASS**

```
✓ [未毒化(声明源副本 = 原样)] 判绿
✓ [声明源副本漂移 1 格] 判红(退出码 1)
✓ [期望表副本漂移 1 格] 判红(退出码 1)
✓ [声明源 + 期望表同时漂移] 判红(退出码 2)
✓ include/tlm/cep/protocol_instances.json 未被改动
✓ test/cep_flow_support_expected.json 未被改动
✅ 全部通过(6 项)  rc=0
```
- **只写临时目录**:`mktemp`/`TMPDIR` 3 处 + `trap cleanup EXIT`;`git diff --quiet` 零改动自证(`:127`)
- **删除韧性(独立验证)**:`cpptlm_tests '[cep][flow-support-NOMATCH]'` ⇒ **rc=2**(Catch2 无匹配)⇒ 若 C++ 用例被删,fixture 的「未毒化须绿」会失败 ⇒ **fixture 仍红** ⇒ 满足仓内铁律 2 ✅
- **失败模式**:红 ⇒ 非零退出;脚本**不重编译** ⇒ ⛔ 不可能「编译失败」冒充「判据失败」

### AC-06 实施期零改动 · 新增 invariant = 0 — **PASS**

| 项 | 结果 |
|---|---|
| `include/tlm/cep/**` 变更文件数 | **0** ✅ |
| `src/tlm/cep` 变更文件数 | **0** ✅ |
| 声明源 / schema / 引擎 / adapter / tracker | 同上零 diff(全量 `git diff --stat e17bd426^ HEAD` 复核) |
| **新增 invariant 数** | **0** ✅ —— 集合级复核:`INV-CEP-*`/`CEN-Ib*` **立项前 20 / 现在 20 · 新增 ∅ · 删除 ∅**(MODIFIED 整块重写使该行在 diff 中显示为「整行 +」,但 invariant 列表与原文**逐字相同**,仅「5 种实现」→「6 种实现」) |

### AC-07 不勾选 `ADR-CEP-12` 任何 Verification 项 — **PASS**

- `ADR-CEP-12` 的 `## Verification` 段:立项前 **5** 个 `- [x]` / 现在 **5** 个 ⇒ **本轮未勾任何项**
- `docs/adr/` 本轮变更文件数 = **0** ⇒ ADR 零 diff ✅

### AC-08 `openspec validate --strict` rc=0 — **⚠️ 部分达成**

| 语境 | 结果 |
|---|---|
| **归档前(authorize 时,delta 语境)** | ✅ `openspec validate cpptlm-cep-flow-support-assertions --strict` = **rc=0**(立项时实测,见台账 Entry #427 ①) |
| **归档后(main spec 语境,per Entry #429 ⑤)** | ⚠️ `openspec validate cep-core-contracts --strict` = **rc=1** —— ⚠ **WARNING**(非 ERROR):3 个 Requirement 正文 >500 字符 |

**3 个超长 Requirement(独立量测)**:

| # | Requirement | 正文长度 | 归属 |
|:--:|---|:--:|---|
| 1 | `cep-stub-cache-contract` | **510** | ⚠️ **既存** —— 立项前(`e17bd426^`)**已 510** ⇒ 同阈值(`MAX_REQUIREMENT_TEXT_LENGTH = 500`)⇒ **该 main spec 在本 change 之前 `--strict` 就是红的** |
| 2 | `cep-sharer-tracker-contract` | **727** | ⚠️ **本 change 引入/加剧**(立项前 143 → 现在 727;来自 MODIFIED 块的「订正 + 标题债」两段 `>` 注) |
| 3 | `cep-flow-support-value-lock` | **527** | ⚠️ **本 change 新增**(ADDED Requirement 的正文 + 边界/依据注) |

⇒ **判据**:AC-08 在**归档前语境**达成(这是 AC 的字面对象 —— change 的 delta);在**归档后语境**未达成,且**其中 1 项为既存** ⇒ 记为 **F-1(SHOULD_FIX)**,不作 BLOCKER(⛔ 因为该 main spec 在本 change 之前即已 `--strict` 红,本 change 未「从绿变红」)。

### AC-09 `[cep]` 0 failures · 用例数 +1 — **PASS**

| 项 | 结果 |
|---|---|
| `./build/bin/cpptlm_tests '[cep]'` | ✅ **528 用例 / 12286 断言**,rc=0 |
| 基线(台账 Entry #424 实测) | 527 用例 / 11797 断言 ⇒ **+1 用例 / +489 断言** ✅ |
| `[cep][flow-support]` 专项 | ✅ **489 断言 / 1 用例** |
| 无硬编码用例数写入文档 | ✅(AGENTS.md 仍用「350+ / 750+」量级 + 「实时计数用 `--list-tags`」) |

### AC-10 归档期五步链 — **PASS**

| 项 | 证据 |
|---|---|
| ① 授权 Entry id 与台账标题**精确相等** | ✅ `### Entry #428 — ` 命中 1 次;`--update --authorized-by "Entry #428"` 输出「授权校验通过」 |
| ② `--update` 全量重算后**只有**目标文件 hash 变化 | ✅ 清单 commit `81a0cd5e` = **1 file changed, 4 insertions / 4 deletions**(`cep-core-contracts/spec.md` hash + `authorized_by` + `frozen_at` + `frozen_at_commit`)⇒ **零附带漂移** |
| ③ 归档**未**用 `--skip-specs` | ✅ `468e5352` 的 commit body 中 `skip-specs` 命中 **0**;且实测 delta 已合并(`+1 added / ~1 modified`) |
| ④ 时序:归档前必绿 → 合并后必红 → 才走五步链 | ✅ 立项时 `cep_freeze_check` rc=0;合并后「1 被改」;`--update` 后 rc=0 |
| ⑤ `artifact_count` 不变 | ✅ **86**(与立项前一致) |
| ⑥ `frozen_at_commit` ≠ HEAD 为常态告警 | ✅ 告警存在,`frozen_at_commit = 468e5352`,未重复 `--update` |

---

## T6.2 · 3 项残余状态确认(全部**未关闭**)

| # | 残余 | 实测状态 |
|:--:|---|---|
| ① | **镜像 20 值(`mesif` / `moesi_gpu`)的 SKIP 条件性洞** | ✅ **确认存在**:期望表覆盖 = 6 个 `repo_owned` 实例,**不含** `mesif`/`moesi_gpu`;`cep_decl_alignment_check.sh:41-43` 在 ArchForge 缺席时打印 `[SKIP]` 并 **`exit 0`** ⇒ **无 ArchForge 环境里这 20 个值零保护**。⛔ **待 architect 裁决**(是否纳入仓内权威 / 或要求 ArchForge 仓在 CI 常驻) |
| ② | **`cxl_2_0` / `tilelink_c` / `nvlink_4` 无逐值出处** | ✅ **确认并存量化**:`tilelink_c` / `nvlink_4` = `none`(0/10);`cxl_2_0` / `cxl_3_0_symmetric` = partial **1/10**;`chi_rnf` = partial **5/10`。⚠️ 这些族的**期望值仍被锁**(golden 表),只是「无外部权威出处」⇒ 已在表内显式标注,⛔ 未伪造 |
| ③ | **Scenario 标题债** | ✅ **确认存在**:`openspec/specs/cep-core-contracts/spec.md:88` 标题仍为「`5 实现 + caps 四属性齐备`」,而 `:93` 正文写「恰好 **6** 个实现」+ 免责句 ⇒ 机械约束下的最优形态(openspec `MODIFIED` 场景名不可改);前向纪律已写入 Requirement 正文 |

---

## 失败项与路由建议

| # | 级别 | 内容 | 路由 |
|:--:|---|---|---|
| **F-1** | **SHOULD_FIX** | `openspec/specs/cep-core-contracts/spec.md` 有 **3 个** Requirement 正文 >500 字符 ⇒ `validate --strict` rc=1(**WARNING** 级,非 ERROR)。其中 `cep-stub-cache-contract`(510)**既存**;`cep-sharer-tracker-contract`(727)与 `cep-flow-support-value-lock`(527)**本 change 引入** | ⚠️ **需动冻结 main spec** ⇒ 超出 verifier 写集 ⇒ 路由 **builder**(实施)+ **planner**(台账 Entry)⇒ 另走**一条**五步链。建议做法:把两段 `>` 注(「订正 + 标题债」)从 Requirement 正文**移入第一个 Scenario 内**,并把 ADDED Requirement 的边界/依据注**压缩或移入 Scenario** ⇒ 正文 <500。⛔ **不新增 Requirement**、⛔ 不改语义 |
| F-2 | NOTE | 镜像 20 值 SKIP 洞 | 路由 **architect**(判权) |
| F-3 | NOTE | 3 族无逐值出处 | 已显式标注,非缺陷 |
| F-4 | NOTE | Scenario 标题债 | 机械约束,已以前向纪律消化 |

> ⚠️ **F-1 不判 BLOCKER 的判据**:① 该 main spec 在本 change **之前**即因既存项(510)而 `--strict` 红 ⇒ 本 change **未造成「绿 → 红」**;② 该项是 **WARNING** 级(非 ERROR);③ AC-08 的字面对象是 **change 的 delta**,其 rc=0 已达成。⛔ 但**不得**因此忽略 —— 本 change 确使超长项从 1 → 3,属**质量退化**,应修。

---

## 遗留清扫结果(本 change 范围内)

| 清扫项 | 结果 |
|---|---|
| 新增 `TODO` / `FIXME` / `XXX` | **0**(期望表 / fixture 脚本 / 测试文件) |
| 被 skip / disabled 的测试(`#if 0` / `DISABLED_` / `[!mayfail]` / `[!shouldfail]`) | **0** |
| 临时调试输出(`printf` / `std::cout` / `std::cerr`) | **0** |
| 注释掉的代码块 | **0** |
| 未替换的 mock | **0**(本 change 无 mock) |
| U+FFFD 乱码 | **0**(实施期一度出现于期望表 `note`,已修并复验) |
| 空 catch / 无断言用例 | **0** |
| 实施期自引入并已修的缺陷 | 3 项:期望表 `note` 乱码 / fixture 第 4 条「双侧毒化同格」设计缺陷(改为不同实例)/ 并发事故致声明源污染(已 `git checkout --` 按字节恢复) |

---

## 独立复验(verifier 自身执行)

| 项 | 结果 |
|---|---|
| 43 个 `scripts/test/cep_*.sh`(**按 ctest 同参数**:docs_sync / doc_ref 带 `--strict`) | ✅ 全 **rc=0** |
| `ctest --test-dir build -R cep -j4` | ✅ **98/98** |
| `./build/bin/cpptlm_tests '[cep]'` | ✅ **528 用例 / 12286 断言** |
| `bash scripts/test/cep_flow_support_reverse_verify.sh` | ✅ **6/6** |
| `python3 scripts/test/agents_md_lint.py` | ✅ 通过 |
| `openspec validate --changes --strict` | ✅ rc=0(0 active) |
| `openspec validate cep-core-contracts --strict` | ⚠️ **rc=1**(见 F-1) |
| `cep_freeze_check.sh`(无参) | ✅ rc=0 + provenance 一致 |
| 工作区 | ✅ 干净(`git status` 空) |

---

## 判定

**有条件通过(CONDITIONAL PASS)**

- **AC-01 ~ AC-07、AC-09、AC-10**:全部 **PASS**(逐条工具证据见上)
- **AC-08**:**归档前语境 PASS**(delta rc=0)/ **归档后语境 FAIL**(main spec rc=1)⇒ **部分达成**
- **唯一 SHOULD_FIX = F-1**(main spec 3 个超长 Requirement,1 既存 + 2 本 change 引入)⇒ 需另走一条五步链修复
- 3 项残余 **全部未关闭**(其中镜像 SKIP 洞待 architect 裁决)

**本报告不构成对 F-1 的放行** —— 建议排入下一批(与残余 ① 一并处理,同属「契约面质量/覆盖」类)。
