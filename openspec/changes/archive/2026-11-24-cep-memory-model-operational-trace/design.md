# design: cep-memory-model-operational-trace —— 操作级轨迹模型 + 判定面

> **来源**：`proposal.md`（同目录）· **裁定**：`ADR-CEP-22`（✅ 已签发 v1.0 · 台账 Entry #541 · 用户 ratify「A」）
> **范围纪律**：⛔ 零决策变更 · 零 invariant 新增（复用 `INV-CEP-13-IC1`）· ⛔ 不改 schema · ⛔ 不改任何 ADR 正文 · ⛔ 轨迹 = **手写** litmus 迹 · ⛔ 不承诺「已证明」

---

## D-1 落点：两个头文件（**模型** 与 **判定** 分离）

| 文件 | 角色 | 内容 |
|---|---|---|
| `include/tlm/cep/memory_model_trace.hh`（**新增**） | **输入面** | `Op` / `DomainDecl` / `AddrCoCandidate` / `Trace` · 一致性标签词表 · `load_instance_labels()`（从 `protocol_instances.json` 抽声明面）· **决策 3 的授权表**（`allow_read` / `allow_write` / `domain_preserves_po`）· `parse_trace()`（**拒收**面） |
| `include/tlm/cep/compound_graph_falsifier.hh`（**改型**） | **判定面** | 四态 + **原因码闭集**（11 码）· 公理面图 + DFS 三色判环 · 候选枚举（逐地址写序候选的**笛卡尔积**）· ∃/∀ 聚合 · 截断 · 观测面边类 |

⛔ **不新建第三个文件** ⇒ 判定器仍在「复合图证伪工装」名下（工装名与「复合图 = 操作级 `ppo_com ∪ rfe ∪ fr ∪ ws`」**本就更贴合** ⇒ 零改名成本）。

## D-2 候选空间 = 逐地址写序候选的**笛卡尔积**（决策 5 ①）

- 迹声明**逐地址**的写序候选集合（`co_candidates` 的键 = 地址）⇒ 工具取**笛卡尔积**；
- 每个候选 = 各地址取一个候选 ⇒ 该候选给出**同地址**写序（`ws` 边）；
- ⛔ **不枚举全局内存序**（v1.0 的决策 4/5 形态 —— per `ADR-CEP-22` 已签发口径；⚠️ 实施期曾起草过「改全局 `ws`」的 SU 草案 ⇒ 经 **oracle REJECT**（反例误用 ∃/∀ 量化）⇒ **不落 SU**，v1.0 维持不动）。

## D-3 公理面图（决策 4）+ **双图折衷**（决策 6）

| 边 | 构造 | 公理面 | 观测面 |
|---|---|:--:|:--:|
| `ppo_com` | 同线程程序序（**全序对**，非仅相邻）× **决策 3 的授权表**过滤 | ✅ | ✅ |
| `rfe` | `rf(w,r)` ∧ **域标识不等** | ✅ | ✅ |
| `ws` | 该候选下**同地址**写的全序（⛔ 不跨地址） | ✅ | ✅ |
| `fr` | `∃w1: rf(w1,r) ∧ ws(w1,w2)` ⇒ `r → w2`（**静态**读→写形态） | ✅ | ✅ |
| `rf`（域内） | 域内读自 | ❌ | ✅（**仅报告**） |

⇒ 判定产物分三组边类：`edge_classes_observed`（全部）· `cross_domain_edge_classes`（⊆ schema `edge_classes` 枚举）· `rf_edge_classes`（`rf` / `rfe`）。

## D-4 聚合语义（决策 5 ③）—— **∃/∀**，且「短路」与「逐候选报告」并存

- **∃ 可行候选 ⇒ `not_falsified`**（**短路**：找到一个即停 ⇒ 判据面省时）；
- **∀ 候选成环 ⇒ `falsified`**（需穷举 ⇒ 见证 = 候选摘要 + 代表性成环候选）；
- ⚠️ **逐候选报告与短路并存** ⇒ 语料的**候选次序**必须让**成环**的候选排在前面 ⇒ 否则短路后看不到第二个候选 ⇒ ⇒ **语料 `co-two-candidates` 的次序是判据面的一部分**（在迹的 `_status` 注释里登记）；
- **截断** ⇒ `undecided` + `search-truncated`（⛔ 不得判 `not_falsified` / `falsified`）。

## D-5 拒收面（AC-3① / AC-6 / AC-10）—— 解析期即拒收（⛔ 不得「静默跳过」）

| 原因码 | 触发 | AC |
|---|---|---|
| `derived-edge-as-input` | 迹含 `fr` / `ws` / `fr_edges` / `ws_edges` / `derived_edges` 字段 | AC-3① |
| `channel-edge-not-op-edge` | 迹含 `compound_edges` / `from_net` / `to_net` / `from_vc` / `to_vc` 字段 | AC-2 / AC-11 |
| `unbound-domain` | 线程未声明域 / 域标识不在域声明表 | AC-6 |
| `no-domain-instance` | 域绑定的实例 id 不在声明面 | AC-6 |
| `model-semantics-unavailable` | 域标签为 `TSO` / 未知（v1 未登记） | 决策 3 |
| `malformed-trace` | 解析失败 / 缺 `family` / `source_litmus` / `anchor_id` / 锚点不在 allowlist | AC-5 / AC-10 |
| `duplicate-op-id` / `rf-target-missing` / `self-ppo-edge` / `rf-self-loop` | 畸形输入细分 | AC-10 |
| `search-truncated` | 候选枚举触上限 | AC-9 |

⇒ **拒收发生在建图之前** ⇒ 坏输入**永不**进图 ⇒ ⛔ 不会造出假环。

## D-6 闸的**臂替换**（AC-11）—— 旧→新映射表落进脚本注释

旧闸的两条臂（正控「语料为空 ⇒ 不可证伪」+ 反 fixture「通道环 ⇒ `falsified`」）⇒ **整条替换**为**六条臂**（见 `scripts/test/cep_compound_graph_falsification_check.sh` 的头部映射表）⇒ ⛔ **判据不得被静默删除**。

⚠️ **通道环的真判据不在本工装** —— 归 `channel_dependency_graph.hh`（`INV-CEP-09-IC1` + `test/test_cep_channel_dependency_gate.cc`）⇒ 本次改造**不是能力回退**，是**纠正范畴错误**。

## D-7 命名消歧闸的**臂④ 同步**（⛔ 文件名与红因码**保留**）

臂④ 的判据对象由「通道顶点形态 `(instance, network, vc)`」改为「**操作级**作用域标记 `kFalsifierScope = "operation-level-composite-graph"`」⇒ 工装改型后旧顶点形态**不再出现** ⇒ 臂④ 须同步（否则该闸必红）。
⛔ **文件名 `cep_litmus_naming_check.sh` 与红因码 `litmus-scope-claim` 保留**（其管辖违例类不随工件改名过期 + 阶段 1 审计链）。

## D-8 措辞纪律：产物文本**不得**含「已证明 / proved」字样

闸的产物扫描是**朴素子串匹配** ⇒ **否定语境里的同字面也会命中** ⇒ ⇒ 拒收路径的 `coverage_note` 用「不得表述为公理**成立**」而非「不得表述为「已证明」」（⛔ 否则闸自红）。

---

## 判据（17 条 AC 的机检面 → 落点）

| AC | 落点 |
|:--:|---|
| AC-2 / AC-11 | 闸臂② + 测试 `[fixture]`（通道语料 ⇒ `invalid` + `channel-edge-not-op-edge`） |
| AC-3① | 解析期拒收 + 测试 `[fixture]` |
| AC-3② | `Decision::axiom_edges`（公理面边逐条可核）+ 测试 |
| AC-3③ | 测试（两语料文件的字段**互不出现**） |
| AC-4 | 测试 `[repo-corpus]`（金标准迹 ⇒ `falsified` + 见证；companion ⇒ `not_falsified`） |
| AC-5 | `anchor_allowlist()` + 解析期拒收 + 测试 |
| AC-6 | 解析期拒收 + 测试 |
| AC-7 | `Decision::candidate_outcomes` + 测试（∃/∀） |
| AC-8 | `Decision::rf_edge_classes` / `cross_domain_edge_classes`（⛔ 枚举**运行时**从 schema 读）+ 测试 |
| AC-9 | `Decision::search_truncated` + 测试 |
| AC-10 | 解析期拒收族 + 测试（每码一条） |
| AC-12 | delta 以 **MODIFIED 整条替换** ⇒ `grep -c '(instance, network, vc)'` == 0 |
| AC-16 | 闸臂⑥（`combination_contract` 恰 1 处） |
| AC-17 | ctest 全绿 + 全部 `cep_*.sh` rc=0 |
| AC-1 / AC-13 / AC-14 / AC-15 | ✅ **已由 `ADR-CEP-22` 闭合**（人工评审 + 登记 + 五步链 + 归因复用 + 退场选择 (b)） |
