# proposal: cep-memory-model-operational-trace —— 操作级轨迹模型 + 复合一致性公理的可判定落地

> **来源**：架构裁定 `ADR-CEP-22`「每域模型来源与域间边构造规则」（✅ 已签发 v1.0 · 台账 Entry #541 · **用户 ratify「A」**）· 审稿报告 `.rddf/reviews/ADR-CEP-22-memory-model-source-and-cross-domain-edges-review.md`
> **上游**：backlog `.rddf/improvements/cep-memory-model-operational-trace.md`（⛔ 其 AC-7 / AC-8 / AC-12 已按裁定 §5.1 **D-1/D-2** + 决策 6 修订）· gap 分析 `.rddf/roadmap/cep-gen-family-gap-closure.md`（G2）· `ADR-CEP-13` 决策 1 / 2 / 5②
> **定位**：把 `INV-CEP-13-IC1` 从**不可证伪**（语料为空 ⇒ 「边类无定义」）走向**可机判**。⛔ **零决策变更** · 零 invariant 新增（复用 `INV-CEP-13-IC1`）· ⛔ 不改 schema。

## Why

| 事实 | 一手出处 |
|---|---|
| 现役工装的图顶点 = `(instance, network, vc)` = **通道依赖图** ⇒ 与公理要求的**操作图**不同型 ⇒ 由声明项派生的边**不能**证伪该公理（**范畴错误 ⇒ 假绿**） | `include/tlm/cep/compound_graph_falsifier.hh` 文件头「诚实边界」段 |
| 语料 `compound_edges` 当前为 `[]` ⇒ 判定恒为「**不可证伪**」这一真结果（⛔ 不是「公理已成立」） | `test/cep_compound_graph_corpus.json` 的 `_status` |
| `rfe`/`fr`/`ws` **两仓均无定义**；`combination_contract.compound_consistency.edge_classes` 只登记**取值域** | `ADR-CEP-12` schema description |
| 「每域模型来源 + 域间边构造规则」= 本仓**从未裁定** ⇒ 公理**不可判定** | `ADR-CEP-13` 决策 2 + 裁定记录 §三 |

**不做的代价**：`INV-CEP-13-IC1` 的「结构化声明项」已由 I-6 落地（声明**采用**该判据），但**判定**仍无载体 ⇒ 跨域组合的正确性维度对平台**永久不可用**。

**为什么现在做**：`ADR-CEP-22` 已把该单元的**开工前置**（模型来源 + 边构造规则）闭合并经用户 ratify ⇒ 剩余的是纯实施 ⇒ **无待决架构问题** ⇒ 可直接实施。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **操作级轨迹模型**（新 header） | 线程 → 操作（读/写 + 地址 + 值）→ 事件图（节点 = 操作）；载 `rf` 映射、**同地址写序候选**、每线程**域标识**；⛔ **不含** `fr`/`ws` 输入（派生边） |
| 2 | **`ppo_i` 派生**（per `ADR-CEP-22` 决策 3） | 读侧/写侧授权表**并集**：SC = 全边 · RC-acquire = {从读} · RC-release = {入写} · **RC-data = ∅** · PLO = {入写}；**TSO 不登记** ⇒ `undecided` + `model-semantics-unavailable`；标签来源 = 域绑定实例的 `consistency_labels`（⛔ 不改 schema） |
| 3 | **工装改型**：图由**通道依赖图**改为**操作图** | 公理面边类 = `{ppo_com, rfe, fr, ws}`（⛔ **不含**域内 `rf`）；观测面（判定产物）**报告**域内 `rf` ⇒ **双图折衷**（决策 6） |
| 4 | **派生边构造**（决策 4） | `rfe(w,r) ⟺ rf(w,r) ∧ dom(w) ≠ dom(r)` · `ws` = **逐地址** `co` 候选 · `fr(r,w2) ⟺ ∃w1: rf(w1,r) ∧ ws(w1,w2)`（静态 · 读→写）⇒ 候选面收窄为**同地址写序候选** |
| 5 | **∃/∀ 聚合语义**（决策 5） | ∃ 可行候选 ⇒ `not_falsified`（短路）· ∀ 候选成环 ⇒ `falsified`（需穷举 · 见证 = 候选摘要 + 代表性成环候选）· 候选数触上限 ⇒ `undecided` + `search-truncated`（⛔ 不得判 `not_falsified`） |
| 6 | **判定面四态 + 原因码闭集**（决策 7） | `falsified` / `not_falsized`→**`not_falsified`** / `undecided` / `invalid`（拒收，⛔ 与「不可判」**分离**）+ 10 个原因码 |
| 7 | **通道环臂「替换」而非「删除」**（AC-11） | 既有闸的通道环臂改写为「通道环 ⇒ **必须不** `falsified` + 原因码 `channel-edge-not-op-edge`」+ 脚本注释写明**取代关系 + 取代理由**；旧→新语义映射**逐条列出**。⚠️ **通道环的真判据不在本工装** —— 归 `channel_dependency_graph.hh`（`INV-CEP-09-IC1` + `test/test_cep_channel_dependency_gate.cc`）⇒ **不是能力回退，是纠正范畴错误** |
| 8 | **物理分离** | 通道语料留在 `test/cep_compound_graph_corpus.json`（**作为「必须被拒收」的反 fixture**）· 操作迹另置 `test/cep_memory_model_traces.json` ⇒ 两文件记录**互不出现对方字段**（可机检） |
| 9 | **主 spec requirement 整条替换**（AC-12） | delta 以 **MODIFIED** 替换 `cep-compound-graph-falsification` requirement（新正文**不含**「顶点 SHALL 为 `(instance, network, vc)`」）⇒ 冻结面 **1 处**（spec hash 变更） |
| 10 | **命名消歧闸的臂④ 同步** | 臂④ 的三要素从「通道顶点形态」改为「**操作级**形态」⇒ ⛔ 该闸的**文件名与红因码保留**（per `ADR-CEP-22` 决策 1 / 裁定必不改表） |

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **术语（逐字复用）**：操作 / 执行迹 / 见证（witness）/ **未证伪** / **不可判** —— 「未证伪」≠「无违例」≠「已证明」（per `ADR-CEP-13` 抓取点 5）。

- [ ] **AC-1（每域模型来源有据 · 文档层）**：新建 ADR 逐条列出「哪条公理依赖哪个域模型语义」+ **域间边构造规则** ⇒ ⛔ 不得只写结论；⚠️ 本条 = **人工评审**（architect/oracle 签核）—— 归档要求「引用一手锚点」属不可机检项，**不写成 checkbox** ⇒ ✅ **已由 `ADR-CEP-22` 闭合**（签发 v1.0）⇒ 本单元**消费**，不再重复
- [ ] **AC-2（操作图 ≠ 通道图 · 行为判据）**：喂一份**只含通道环**的语料 ⇒ 工装**不得**报 `falsified`（原因码 `channel-edge-not-op-edge`）⇒ ⛔ 不新造 fixture（复用既有通道环臂，改写语义）；⚠️ 类型层「顶点结构不同」**不算**判据
- [ ] **AC-3（输入边 vs 派生边分离）**：① 迹里带**显式** `fr` / `ws` 字段 ⇒ 拒收（rc≠0，原因码 `derived-edge-as-input`）② 给定 `rf` 映射 + 写序候选 ⇒ 断言**派生**的 `fr` / `ws` 集合逐条等于期望集 ③ **物理分离**：通道语料只在 `test/cep_compound_graph_corpus.json` · 操作迹只在 `test/cep_memory_model_traces.json` ⇒ 断言两文件的记录**互不出现对方的字段**
- [ ] **AC-4（判据可判别 · 双迹）**：两个**具名** litmus 迹（来源写入迹文件的 `source_litmus` / `anchor_id` 字段）—— ① 一个**已知违例**迹 ⇒ 报 `falsified = true` 且给出见证 ② companion 迹（**唯一差异**）⇒ 报**未证伪**；⛔ 「无环迹 ⇒ 未证伪」**不是**判据（任何单线程迹都无环 ⇒ 恒绿）
- [ ] **AC-5（锚点可审计 · 机检部分）**：每条迹含非空 `family` / `source_litmus` / `anchor_id`，且 `anchor_id` ∈ 预定义 allowlist；反 fixture：空锚点 ⇒ 红。⚠️ 「锚点是否**真的**对应该边类」属**人工评审**（并入 AC-1）⇒ ✅ **已由 `ADR-CEP-22` 决策 3 的溯源两档标注闭合**（人工部分）
- [ ] **AC-6（域绑定不可判）**：迹里每个线程须解析到一个一致性域；反 fixture：线程未声明域 ⇒ 判定为 `invalid` + 原因码 `unbound-domain` 且 rc≠0（⛔ 不得静默降级为「未证伪」）
- [ ] **AC-7（写序候选的逐候选判定 + ∃/∀ 聚合）**：写序非唯一时须对**所有候选写序**判定；三写迹 ⇒ `co1` 成环 / `co2` 不成环 ⇒ **逐候选**判定分别为「成环」/「可行」，⛔ **同一条迹**的**聚合**判定 = `not_falsified`（`co2` 可行 ⇒ 模型允许该迹）⇒ `falsified` 须**全部**候选成环（见证含 `co_order == co1` 作**成环候选标识**）；⚠️ 「某候选成环 ⇒ 证伪」是**假阳性** ⇒ ⛔ 禁止
- [ ] **AC-8（`rf` vs `rfe` 分类边界）**：同一份迹仅改 writer 所属域 ⇒ 同域 ⇒ 边类集合 == `{rf}` · 跨域 ⇒ == `{rfe}`；并断言 ① 迹中出现的**每条边**的类型 ∈ schema `edge_classes` 枚举（从 `protocol_instance.schema.json` **运行时读**，⛔ 不写死字面量）② 跨域读自边的类型 == `rfe`、域内 == `rf`（观测面）
- [ ] **AC-9（判定三档 + 截断标记）**：判定产物含 `search_truncated: bool` ⇒ `true` 时判定字段**不得**为「未证伪」，须为 `undecided` + 原因码（⛔ 大迹不得静默变假阴性）
- [ ] **AC-10（畸形输入必红）**：语料解析失败 / `rf` 指向不存在的写 / 重复 op id / 自反 `ppo` 边 ⇒ 各一份反 fixture，rc≠0 且原因码唯一；⚠️ 现状 `compound_graph_falsifier.hh` 对坏输入**静默跳过** ⇒ ⛔ 不得继承该行为
- [ ] **AC-11（闸臂「替换」而非「删除」）**：既有闸的通道环臂改写为新语义（通道环 ⇒ **必须不** `falsified`，原因码 `channel-edge-not-op-edge`），并在脚本注释写明「取代关系 + 取代理由」；旧→新语义映射**逐条列出**（⛔ 判据不得被静默删除）
- [ ] **AC-12（主 spec requirement 的整条替换）**：delta spec 以 **MODIFIED** 整条替换 `cep-compound-graph-falsification` requirement（新正文**不含**「顶点 SHALL 为 `(instance, network, vc)`」）；归档后 `grep -c '(instance, network, vc)' openspec/specs/cep-compound-graph-falsification/spec.md` == 0 ∧ `openspec validate --changes --strict` rc=0
- [ ] **AC-13（新 ADR 的登记与冻结 · bookkeeping 三件套）**：① 新 ADR 登记进 `test/cep_adr_structural_deferrals.json` 的 `decision_adrs` ⇒ `bash scripts/test/cep_adr_structural_check.sh` rc=0 ② 新 ADR 命中 `FROZEN_GLOBS` ⇒ 台账 Entry + `--update --authorized-by "Entry #N"` + **清单单独 commit** + 无参复验 rc=0 ③ `git diff --name-only HEAD~..HEAD -- docs/adr` 只出现 **1 个新增**路径（既有 `docs/adr/ADR-CEP-*.md` **零改动**）⇒ ✅ **已由 `ADR-CEP-22` 落盘闭环**（Entry #541 · 五步链 1 次 · `新增 1 · 移出 0 · hash 变更 1` · 91 → 92）
- [ ] **AC-14（不新增 invariant 编号）**：66 号集合逐字不变（`cep_invariant_map_check.py` rc=0）；归因 id **复用** `INV-CEP-13-IC1`（per `ADR-CEP-13` §Verification「归因 id 一律复用」）
- [ ] **AC-15（V-13-4 的退场链路）**：二选一并写清 —— (a) 本单元提供运行期载体 ⇒ 追加 SU 声明 V-13-4 判读被取代（**正文不动**）· (b) 本单元**不**提供 ⇒ 显式 Out-of-Scope 并指向 I-6 后续单元 ⇒ ✅ **已由 `ADR-CEP-22` 决策 8 裁定 = (b)**（v1 轨迹 = 手写 ⇒ 无运行期载体）⇒ 本单元**沿用 (b)**，⛔ 不得沉默
- [ ] **AC-16（前置可机判）**：开工门 = `grep -c '"combination_contract"' include/tlm/cep/protocol_instance.schema.json` 输出 **1** ⇒ 把「I-6 已落地」从隐含假设变成**单命令判据**
- [ ] **AC-17（零回归 · 白名单外）**：ctest 全绿（⛔ 不写死数字）· 全部 `scripts/test/cep_*.sh` rc=0 · `rddf_stage_gate.sh` rc=0；⚠️ **例外白名单** = AC-11 列出的既有判据改写清单（⛔ 其余用例**逐字未变**）

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-compound-graph-falsification` | **MODIFIED** | 工装从「通道图 + 跨域边」改为「操作图 + 操作迹」⇒ 判定对象与公理**同型**；⚠️ delta 以 **MODIFIED** **整条替换**该 requirement ⇒ 旧顶点形态的正文整体消失（⛔ 非「改名」—— 工装名 `compound_graph_falsifier` **本就更贴合**：复合图 = `ppo_com ∪ rfe ∪ fr ∪ ws` 的操作级图 ⇒ **零改名成本**） |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | `include/tlm/cep/memory_model_trace.hh`（操作级轨迹模型）· `test/cep_memory_model_traces.json`（操作迹语料） |
| **修改文件** | `include/tlm/cep/compound_graph_falsifier.hh`（改型为操作图判定器）· `test/test_cep_compound_graph_falsification.cc` · `scripts/test/cep_compound_graph_falsification_check.sh`（闸臂替换）· `scripts/test/cep_litmus_naming_check.sh`（臂④ 三要素同步）· `scripts/README.md`（闸描述）· `openspec/specs/cep-compound-graph-falsification/spec.md`（**冻结面** ⇒ 五步链） |
| **零改动（硬查）** | `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/protocol_instances.json` · `include/tlm/cep/channel_dependency_graph.hh`（通道环判据归它）· 任何 `docs/adr/*.md` 正文 · `test/cep_invariant_assertion_map.json` · `openspec/specs/cep-core-contracts/spec.md` |
| **冻结面** | **1 处**（`openspec/specs/cep-compound-graph-falsification/spec.md` 的 hash 变更）⇒ 五步链 **1 次**（⚠️ 原 backlog 估的「两处 ×2」已因 `ADR-CEP-22` 先行落盘而收敛） |
| **闸** | 既有闸**闸臂替换**（⛔ 不新增判据 ⇒ 闸计数不变）· 命名消歧闸臂④ 同步（闸计数不变） |
| **测试** | `test_cep_*.cc` 计数**不变**（改写非新增） |
| **KPI** | 本单元占 1 slot（活口径 1/3）⇒ 归档后释放回 0/3（⛔ 不写死数值） |
| **风险** | ① 操作图与通道图混用 ⇒ 假绿（⇒ AC-2/AC-3 物理分离）② 聚合语义被实现反 ⇒ 假阳性（⇒ AC-7）③ `undecided` 被降级为「未证伪」⇒ 假绿（⇒ AC-9）④ 枚举爆炸 ⇒ 大迹判不出（⇒ 上限 + 截断）⑤ `fr` 约定形态漂移（⇒ per `ADR-CEP-22` 决策 4 的锚点复核义务，⚠️ 仓内不可复核 ⇒ 须显式登记）⑥ **域内一致性不由本公理检查**（`ADR-CEP-22` §4.2 缺口① ⇒ 实现**不得**声称覆盖） |

## 关联

`ADR-CEP-22`（✅ 已签发 v1.0 · 本单元的**架构裁定**）· 台账 Entry **#541**（裁定落盘授权）· `ADR-CEP-13` 决策 1 / 2 / 5② · `ADR-CEP-12` 决策 3 / 4 + R-2e · `ADR-CEP-08` 决策 1 · `ADR-CEP-20` 决策 6 · `ADR-CEP-09` 决策 A（通道依赖图无环判据 = 通道环的真判据归属）· backlog `.rddf/improvements/cep-memory-model-operational-trace.md`（**AC-7 / AC-8 / AC-12 已修订**）· 审稿报告 `.rddf/reviews/ADR-CEP-22-memory-model-source-and-cross-domain-edges-review.md`
