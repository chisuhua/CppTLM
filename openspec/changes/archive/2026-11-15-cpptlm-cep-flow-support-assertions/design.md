# design: cpptlm-cep-flow-support-assertions

> **角色**:builder · **上游**:`openspec/changes/cpptlm-cep-flow-support-assertions/proposal.md` + improvement `.rddf/improvements/cpptlm-cep-flow-support-assertions.md`(v2)· 台账 **Entry #427**
> **范围**:repo_owned 6 实例 × 10 流 = **60 格** `flow_support` 期望值 + 逐格机器锁 + 脚本侧持久反向 fixture · **零 src / 零声明源 / 零 schema 改动**

## D-1 · 期望表载体 = `test/cep_flow_support_expected.json`(独立 JSON)

**决策**:期望表落**独立 JSON 文件**,⛔ 不内联进 C++ 测试文件。

**理由**:
1. **持久反向 fixture 必须能在不改仓内文件的前提下毒化期望表** ⇒ 期望表需可被脚本指向**副本** ⇒ 必须是独立文件(C++ 源码内联常量无法被脚本副本化)。
2. **C++ 与脚本共用一份真源** ⇒ 符合仓内既有形态(`include/tlm/cep/adapter_schema_contracts.hh` = 「判据实现**单一真源** —— Catch2 测试与离线 CLI **共用**一份,**禁双实现**」)。
3. **机器可读 + 可 diff** ⇒ 声明值变更时,期望表的 diff 与台账 Entry 可并排核对。

**表结构**:
```json
{ "schema": "cep-flow-support-expected/v1",
  "instances": {
    "<id>": {
      "source": { "coverage": "full|partial|none", "file": "...",
                  "anchors": [ { "text": "...", "keys": [...] } ],
                  "provenanced_keys": [...], "note": "..." },
      "flow_support": { "<canonical 流名>": true|false, ... 共 10 键 }
    } } }
```

⚠️ **`anchors` 是列表**(⛔ 非单串):`ualink_4` 的 10 键在 `ADR-CEP-12` 的 13 键取值表里**跨 4 个表行** ⇒ 单锚点串表达不了 ⇒ 每条 anchor 记「锚点文本 + 该锚点覆盖的键集合」。

**D-1.1 `source.coverage` 三态 + 机器可核**(per improvement AC-02):
| coverage | 含义 | 机器判据 |
|---|---|---|
| `full` | **10/10** 键均有逐值出处 | `file` 存在 **且** 每个 `anchors[].text` 逐字命中该文件 **且** `provenanced_keys` == 10 键全集 |
| `partial` | 部分键有逐值出处 | `file` 存在 **且** 每个 `anchors[].text` 逐字命中 **且** `provenanced_keys` == ∪`anchors[].keys` ≠ ∅ **且** ⊆ 10 键 **且** `note` 含「无逐值出处」 |
| `none` | **无**逐值出处 | `file` / `anchors` **必须缺席** **且** `provenanced_keys` 为空 **且** `note` 含「无逐值出处」 |

⚠️ **诚实分层的实测结果**(逐族独立复核,非推断):

| 实例 | coverage | 出处 | 有出处的键 |
|---|---|---|---|
| `ualink_4` | **full** | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md`(13 键取值表行 2~5) | **10/10** |
| `chi_rnf` | partial | `docs/adr/ADR-CEP-19-cohif-carrier-and-protocol-bridge.md:551` | 5(request/completion/snoop/snoop_resp/writeback = true) |
| `cxl_3_0_symmetric` | partial | `openspec/specs/cep-cxl-dcoh-symmetric/spec.md:100` | 1(back_invalidate = true) |
| `cxl_2_0` | partial | 同上(该行显式写「与 `cxl_2_0` 的 `false` **不同**」) | 1(back_invalidate = false) |
| `tilelink_c` | **none** | — | 0 |
| `nvlink_4` | **none** | — | 0 |

⇒ ⛔ **不得**用「ADR-CEP-12 能力映射」做笼统背书(其 `## Verification` 六条判据**无**「逐实例 `flow_support` 值」对应项)· ⛔ **不得**伪造出处。

## D-2 · 判据落既有载体 `test/test_cep_instance_declaration_contract.cc`

**决策**:新增 **1 个** `TEST_CASE`(标签 `[cep][instance-decl]`),⛔ **不**新建测试文件。

**理由**:`test/` 用 `file(GLOB)` 收录 ⇒ 新文件零 CMake 改动;该文件已是「实例声明契约」的归属载体(`INV-CEP-12-IC1` 等),判据同域。

**判据构成**(per improvement AC-01~AC-05):

| # | 判据 | 可验收点 |
|---|---|---|
| ① | **期望表 ⟷ 声明源逐格比对** | 失败消息含 `id` + `key` + 期望 + 实际 |
| ② | **形状联判** | 每实例 `flow_support` 键集 **恰好 10** 且与 `tlm::cep::kCanonicalFlowNames`(代码真源)**逐名对齐** |
| ③ | **类型 + id 唯一性** | `is_boolean()`(避免 `get<bool>()` 对整数抛 `type_error` ⇒ 崩而非干净失败);id 唯一 |
| ④ | **反向控制**(内存副本) | 逐实例轮转翻转 1 格 ⇒ `REQUIRE(翻转值 != 原值)` + `REQUIRE_FALSE(比对通过)` |
| ⑤ | **域 = `repo_owned_instance_ids` 全集** | ⛔ 不按 `status` 过滤;**双向集合相等**(表 ids == 声明源 repo_owned ids)⇒ 新增实例不会「无覆盖且绿」 |
| ⑥ | **出处可核** | per D-1.1 三态判据 |

⚠️ **逐实例 `SECTION`**(⛔ 不用单个巨型 `REQUIRE` 链):Catch2 的首个 `REQUIRE` 失败即中止 ⇒ 巨型链会**看不见其余 79 格**;`SECTION` 让每格独立报告。

⚠️ **禁止「找到才断言」形态**(反例:`test/test_cep_ualink_engine.cc:345-362` 的循环**从不断言自己找到了 `ualink_4`** ⇒ 实例被改名即整段静默空转、测试仍绿)⇒ 本判据以 **id 查表 + 集合相等**取代「遍历时找」。

## D-3 · `load_json` 加 env 覆盖(供持久 fixture 指向副本)

**决策**:在 `test_cep_instance_declaration_contract.cc` 的 `load_json` helper 上加**两个** env 覆盖:
- `CEP_FLOW_SUPPORT_DECL` ⇒ 覆盖声明源路径(`include/tlm/cep/protocol_instances.json`)
- `CEP_FLOW_SUPPORT_EXPECTED` ⇒ 覆盖期望表路径(`test/cep_flow_support_expected.json`)

**理由**:持久反向 fixture 必须毒化**副本**而**不改仓内文件**(否则 `ctest -j4` 并行会读到中间态 —— `cep_decl_reverse_verify.sh` 因此**刻意不注册 ctest**)。⇒ 本 fixture 走 env 覆盖 ⇒ **只写临时目录** ⇒ **可注册 ctest 并行安全**。

⚠️ **零外溢**:env 覆盖只在**本文件内**的 `load_json` 生效 ⇒ 其他测试文件的 `load_json` 不受影响(⚠️ 本文件既有 5 处 `load_json("include/tlm/cep/protocol_instances.json")` 调用**也会**被 env 覆盖 ⇒ 这是**期望行为**:持久 fixture 毒化声明源时,本文件的**全部**声明判据都应随之变化 ⇒ 否则 fixture 只验了新用例、放过既有判据的静默漂移)。

## D-4 · 持久反向 fixture = `scripts/test/cep_flow_support_reverse_verify.sh`

**形态**(照抄 `scripts/test/cep_decl_reverse_verify.sh` 的骨架,但**只写临时目录**):
1. 复制声明源 + 期望表到 `$(mktemp -d)`;
2. **毒化声明源副本** 1 格(某 repo_owned 实例的某 `flow_support` bool 取反)⇒ 跑 `build/bin/cpptlm_tests '[cep][instance-decl]'` ⇒ **必须红**;
3. **毒化期望表副本** 1 格 ⇒ 跑 ⇒ **必须红**;
4. 未毒化 ⇒ **必须绿**;
5. `trap cleanup EXIT` 删临时目录(⛔ 仓内文件零改动)。

⚠️ **归因不模糊**:两种毒化分别单独跑 ⇒ 失败必然归因到「声明源漂移」或「期望表漂移」,不混计。

⚠️ **失败模式断言**:红 ⇒ 必须**非零退出**;⛔ **不**接受「编译失败」冒充「判据失败」(仓内反例:`cep_mesif_state_spec_reverse_verify.sh` 把两者合并计数 ⇒ 「因正确原因红」无机器区分 —— per `D-η` ③ 的既有口径差异)。

## D-5 · 归档期五步链(⛔ 归档时才触发)

delta 合并进 `openspec/specs/cep-core-contracts/spec.md` ⇒ 该文件在 **M1 冻结面**(`openspec/specs/cep-*/spec.md` glob)⇒ 归档步须走:台账 Entry(**归档期**那条)→ 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单**单独** commit → 无参复验 rc=0。

⚠️ **时序**:归档前冻结闸**必绿**(delta 在 `openspec/changes/<name>/specs/...`,冻结 glob **刻意不匹配**该路径,per `cep_freeze_check.sh:73-77`)⇒ **不得**拿「归档前 rc=0」当已合规。
⚠️ `--update` 是**全量重算** ⇒ 更新后必须 `git diff` 清单、确认**只有** `cep-core-contracts/spec.md` 的 hash 变化。
⚠️ 归档**禁 `--skip-specs`**(本 change 是「✅ 已完成归档」语义 ⇒ 必须合并 delta)。
⚠️ `artifact_count` **不变**(现 86;不新增冻结文件)。

## D-6 · Scenario 标题债的消化(⛔ 工具约束,非疏漏)

`openspec` 的 `MODIFIED` 按**场景名精确匹配**且 archive **拒绝丢弃**既有场景名(`requirement-blocks.js:453-480` + `validator.js:671-680`);`ADDED`+`REMOVED` 同名被拒(`specs-apply.js:200-203`)⇒ **含数量的 Scenario 标题一经落盘即无法经 delta 改名**。

⇒ delta **保留**标题「5 实现 + caps 四属性齐备」+ 正文加**显式免责句**(实现数 = 6,以正文为准)。⚠️ 该形态**严格优于拆出**(拆出则 main spec 仍写「恰好 5 个实现」= 明错)。

## D-7 · 边界(与 proposal 一致,零放宽)

⛔ `include/tlm/cep/**` 零 diff · `protocol_instances.json` 零 diff · `protocol_instance.schema.json` 零 diff · 引擎 / adapter / tracker 零 diff · 新增 invariant = **0** · 不断言镜像实例(`moesi_gpu` / `mesif`)· 不勾选 `ADR-CEP-12` 任何 Verification 项 · 不改任何已签批 ADR(只**引用** `ADR-CEP-04 §13` 的「6 种实现」追加式修订)。
