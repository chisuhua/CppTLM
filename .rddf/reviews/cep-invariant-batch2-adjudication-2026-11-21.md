# 裁决记录 · **I-6**（invariant 建模批 2）与 **I-7**（档 2 扩面到 `tilelink_c`）

> **状态**：🟡 **形态已裁 · 待用户 ratify（R-2）**
> **日期**：2026-11-21 · **裁决人**：architect（build agent 会话，`role-switch.sh architect --by user`）
> **对象**：登记册 §6.4 队列 2 的 **I-6** / **I-7**（§6.9 阶段 ③ 决策门）
> **输入**：`.rddf/reviews/i6-adjudication-request-2026-11-21.md`（builder 提出的事实与问题清单）
> **关联 change**：`openspec/changes/` 尚无 I-6 / I-7 的 change（⛔ architect 不创建 openspec change；立项由 planner）
> **AC 映射**：✅ **已回填**（2026-11-21 · **主会话代录** per I-7a 的 AC-14）——
>   · **I-6** ⇒ change `cpptlm-cep-invariant-modeling-batch2`（已归档）的 **AC-1 ~ AC-14**：§六 H-2 的「步骤 0 四件机械件」= **AC-1**（含 T0 的 deferrals 删除 + 映射表重生成）· 「AC-3 修订」= **AC-3**（schema 13→14 + `required` 仍 13 + 反 fixture）· 「AC-4 修订」= **AC-4**（4 个断言点 / 2 个用例标题改形清单）· 「`early_ack` 逐实例声明 + `unknown` 计数机器化」= **AC-9** · 「golden 零改动」= **AC-8**；其余 AC = 该 change proposal 的逐条判据。
>   · **I-7a** ⇒ change `cpptlm-cep-tilelink-tree-manager-view-of`（已归档）的 **AC-1 ~ AC-14**：§四.2 的「`view_of` 签名同形」= **AC-1** · 「6 行夹具口径」= **AC-2** · 「快照 vs 判决」= **AC-3** · 「脏性口径」= **AC-4** · 「CHAIN in-flight 快照」= **AC-5** · 「零副作用」= **AC-6** · 「档 2 仍单族」= **AC-7** · 「零 diff 面」= **AC-8** · 「0 回归 + 队列语义」= **AC-10** · 「载体身份 + 逐字锚点」= **AC-11** · 「多 Tip 如实返回」= **AC-12** · 「载体表 8 行」= **AC-13**。
>   · **I-7b** ⇒ `.rddf/improvements/cep-tilelink-home-side-state-driver.md`（⏳ **待立项**）⇒ AC 映射**待其 change 立项后回填**。
> ⚠️ 本次回填**只登记编号对应** ⇒ ⛔ **未改动**本记录的任何决策文本（per AC-14「代录」限定）
> **修订**：v3（2026-11-21 · oracle 轮 1 的 6 条高/中影响修正 + critic 轮 1 的 4 条 SHOULD_FIX + 4 条 NITS；修正 v1 的 3 处事实错误，见 §八）

---

## 一、裁决摘要与用户拍板记录

| # | 决策 | 用户拍板（2026-11-21） | 落定形态 |
|:--:|---|---|---|
| **1** | I-6 的 schema 形态 | **接受 13→14** | `properties` 13→14，新增**可选**顶层属性 `combination_contract`；`required` **仍 13** ⇒ `INV-CEP-12-IC1` 逐字为真（见 §2.1） |
| **2** | I-6 的边类语义来源 | **登记 backlog，独立立项解决** | 本仓**不定义** `rfe/fr/ws` 语义；只登记**取值域** + 语义锚点引用（见 §三） |
| **3** | I-7 的扩面路径 | **同意 3A（拆 a/b）** | **I-7a** = `TreeManager::view_of`（档 2 仍单族）；**I-7b** = 新建 TileLink-C home 侧状态驱动 + tracker 侧观测 + 档 2 第二族（独立立项，见 §四） |

### 本轮同时更正的三处事实（裁决前提）

| # | 请求文件中的说法 | 实测结论 | 影响 |
|:--:|---|---|---|
| F-1 | Q-a 的候选 A =「给 `SharerTrackerIf` 增纯虚 `view_of`」 | **三重不可行**：① 破 `count_member_virtuals == 5` 两处断言（`test/test_cep_sharer_tracker_base.cc:187`、`test/test_cep_tilelink_adapter.cc:447`）② 破 `ADR-CEP-04 §3`「恰 4 个虚方法」契约 ③ **违反 I-7 自己的 AC-9**（禁改 `sharer_tracker.hh`） | Q-a 改裁为「**具体类**上补 `view_of`」（§4.3） |
| F-2 | I-6 的阻塞 =「schema 13→14 的形态」 | 「13」= `ADR-CEP-12` 决策 1 的 yaml 全量列举键数（**13**）；schema 文件**无 version 字段** ⇒「schema 13」是派生叫法 | 形态裁的是**声明项的字段名 / 取值域 / 必填策略**，不是「版本号」 |
| F-3 | litmus 语料「由声明项派生填充」 | **范畴错误**：工装的图顶点 = `(instance, network, vc)`（`include/tlm/cep/litmus_falsifier.hh:63-70`）= **通道依赖图**；而 `rfe/fr/ws` 是**操作级**关系 ⇒ 填出来的环**不是**对复合一致性公理的证伪 ⇒ **假绿** | ⛔ 明令**禁止**派生填充（§2.8） |

---

## 二、决策 1 · I-6 的声明项形态（D-1 ~ D-5）

### 2.1 D-1 · schema 13→14 的**实现路径**（关键精化）

**结论**：`properties` 13 → 14（**属性总数**），新增属性**不进 `required`**（**必含**属性数仍 13）。

**依据（逐条可复现）**：

| 事实 | 证据 | 对方案的影响 |
|---|---|---|
| `INV-CEP-12-IC1` 的**机器载体**断言的是 `required` 集合，不是 `properties` 数 | `test/test_cep_instance_declaration_contract.cc:104-115`：`REQUIRE(required.size() == 13u)` + `REQUIRE(from_schema == from_groups)`（7 字段组并集**逐名一致**）+ `REQUIRE(from_groups.size() == 13u)` | 新增属性若**不进** `required` ⇒ 该载体**逐字为真、零改动** |
| 4 处 per-family 断言读的也是 `required` | `test/test_cep_cxl_dcoh_instance_decl.cc:93`、`test/test_cep_mesif_instance_decl.cc:137`、`test/test_cep_tilelink_instance_decl.cc:79`、`test/test_cep_instance_declaration_contract.cc:105` | 同上 ⇒ **零改动** |
| 镜像域声明用**本仓 schema** 做 jsonschema 全量校验 | `scripts/test/cep_decl_alignment_check.sh:263-268`（`$ARCHFORGE` 缺席时该闸 `:38-42` 走 SKIP） | 新增属性**不进 `required`** ⇒ 镜像域（`moesi_gpu` / `mesif`，权威在 ArchForge）**零跨仓改动**；进 `required` ⇒ 必须跨仓 PR |
| `ADR-CEP-12` 决策 1 的 yaml 顶层键**恰 13** | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md:43-83`（`id`…`combination_capabilities`） | 「13 个顶层属性」= **必含**属性数（`required`）；`properties` 数 ≠ 该口径 ⇒ IC1 措辞**不需要**改 |
| ADR 演进机制允许「属性内部形态变更」且 IC1 可不变 | `ADR-CEP-12 §8`（2026-10-22 per-network 嵌套）：「正文不改，自本段起以本段文本为准」+「取代范围 … `IC1` / `IC3` **不变**」 | 本次追加 SU 沿用同一机制 |

⚠️ **措辞歧义风险（必须在 SU 中澄清）**：schema 的 `properties` 有 14 项而 `required` 有 13 项 ⇒ 读者可能把 IC1 的「13 个顶层属性」读成 `properties` 数 ⇒ 误判矛盾。⇒ SU 必须写明「IC1 的『13 个顶层属性』= **必含**（`required`）属性数；schema 属性**总数**为 14，第 14 项为**可选**声明项」；**并且**必须显式中和 `ADR-CEP-12:158` 括注里的「(= schema 实际属性数)」（该括注在正文，⛔ 不可改 ⇒ 只能由 SU 记为「被取代表述」，见 §2.7.1 item 2）。

**⛔ 硬约束**：第 14 个属性**不得**加入 `required` 数组。若要必含 ⇒ 必须重开本裁决（连带 IC1 语义变更 + 4 处断言 + ADR 取代范围 + 跨仓）。

#### 2.1.1 被否掉的替代方案（可审计性 —— 三个方向各记一次）

| 替代方案 | 收益 | 代价 | 裁决 |
|---|---|---|---|
| **嵌套进既有容器**（`consistency_labels.compound` + `combination_capabilities.bridging`）| 全部 13 绑定断言**零改动**（`test/test_cep_schema.cc` 的 `:252/:257/:423/:447`）· 无 SU 口径澄清 · 零跨仓 | ① `combination_capabilities` 有「恰 3 维」机器锁（`test/test_cep_schema.cc:401-404`）⇒ 嵌套即红 ② **语义混层**：capabilities = 能力 / contract = 判据 ③ `consistency_labels` 是请求级标签面（HeteroGen 标签体系，per `ADR-CEP-12` 决策 3/4），与跨域桥接判据不同域 | ❌ 否（语义分层优先；「零测试改动」不是目标，**诚实可判**才是） |
| **另立声明文件（sidecar 模式）**（如 `perm_upgrades.json` 形态）| schema 与既有测试**零改动** | 声明**有 6 处消费方**（装配闸 / 桥消费路径 / 装载器 / VC 占用快照 / 判据输入 / 测试）⇒ 另立文件需新建消费面 + 双文件漂移风险 | ❌ **仓内已有裁决否决**（⛔ 非本次新判）：`ADR-CEP-12 §11` 的「**为何不能走『另立声明文件』**」⇒ 本次沿用（§10 的 `perm_upgrade` 是**仓内单方能力升级**的案例，§11 的否决理由是**通用**的）|
| **新增属性进 `required`** | 「必含」语义最强 | IC1 语义变更 + 4 处断言 + ADR 取代范围 + 镜像域跨仓 PR（§2.1 依据表）| ❌ 否（AC-6「IC1 只联动不改语义」是本单元的硬承诺）⇒ **仅当用户重开裁决**才可考虑 |

### 2.2 D-1 · 新增声明项的**规范形态**

**顶层属性名**：`combination_contract`（与既有 `combination_capabilities` 平行：`capabilities` = 组合**能力**，`contract` = 组合**判据/行为契约**）。

```jsonc
"combination_contract": {
  "description": "组合契约(per ADR-CEP-12 Status Update 2026-11-21):① bridging = 跨域桥接行为契约 ② compound_consistency = 复合一致性判据的实例化声明。⛔ 本属性为**可选**(不进 required);⛔ 边类语义不在本仓定义(见 ADR-CEP-13 决策 2)",
  "type": "object",
  "additionalProperties": false,
  "minProperties": 1,                      // ⛔ 禁空对象入库（否则「声明了」不可判）
  "properties": {
    "bridging": {
      "description": "跨域桥接行为契约(per ADR-CEP-13 决策 4:并发粒度由对端协议是否允许提前 ack 决定)。⚠️ 消费方向:桥 A↔B 的并发粒度读 **B**(对端)实例的 `early_ack`",
      "type": "object",
      "additionalProperties": false,
      "minProperties": 1,
      "required": ["early_ack"],
      "properties": {
        "early_ack": {
          "description": "对端协议是否允许提前 ack(枚举前两项与 ADR-CEP-13 决策 4 表的两行「允许提前写 ack」/「不允许提前完成」**语义逐字对应**,kebab-case 转写;⛔ 非 ADR 原文的英文直译)。⛔ `unknown` = 无一手协议锚点时的**显式**声明(⛔ 不得编值);⛔ 本仓**无**「本实例是否参与跨域桥接」的判定面 ⇒ 该事实暂由 `unknown` 覆盖(见 §2.8 P-9)",
          "enum": ["allow-early-write-ack", "deny-early-completion", "unknown"]
        }
      }
    },
    "compound_consistency": {
      "description": "复合一致性判据的实例化声明(per ADR-CEP-13 决策 2 + 决策 5 ①)。⛔ 本项只登记**取值域**与**可验范围**;⛔ 边类语义锚点 = ADR-CEP-13 决策 2(⛔ 本仓不复述、不重定义)",
      "type": "object",
      "additionalProperties": false,
      "minProperties": 1,
      "required": ["adopted"],
      "properties": {
        "adopted": { "type": "boolean", "description": "本实例参与的组合是否采用复合一致性判据" },
        "edge_classes": {
          "type": "array",
          "description": "判据在跨域关系上使用的边类取值域(⛔ 取值域 ≠ 语义定义)",
          "items": { "enum": ["rfe", "fr", "ws"] }
        },
        "verification_scope": {
          "type": "string",
          "minLength": 1,
          "description": "域内可验范围(per ADR-CEP-13 决策 5 ②:平台只尝试证伪,不提供无违例保证)"
        }
      }
    }
  }
}
```

**必填策略（三层，逐层收紧）**：

| 层 | 规则 | 判据载体 |
|:--:|---|---|
| 顶层 | `combination_contract` **可选**（但存在时 `minProperties >= 1` ⇒ 禁空壳） | jsonschema |
| 子对象 | 声明了 `bridging` ⇒ `early_ack` 必填；声明了 `compound_consistency` ⇒ `adopted` 必填 | jsonschema（子对象 `required`）|
| 语义条件 | `adopted == true` ⇒ `edge_classes` **非空** ∧ `verification_scope` **非空** | ⛔ schema 不写 `if/then` ⇒ 落**闸判据**（§2.4）|

**为何 `early_ack` 只有 3 值（⛔ 已删 `not-applicable`）**：`not-applicable` 断言的是「本实例不参与跨域桥接」—— 而本仓**没有**「桥接组合登记面」⇒ 该值**无适用 predicate ⇒ 不可判的声明项**（声明可能静默误导粒度推导，且闸永远绿）。⇒ 三值的后果分类完备：`allow` / `deny`（有 ADR 措辞支撑）+ `unknown`（其余全部情况：既可能是无一手锚点，也可能是不参与桥接 —— 二者的共同后果都是「粒度不可判」⇒ 合并不损失可判性）。⇒ 「本实例是否参与桥接」的事实留给未来的组合登记面（登记为 backlog 依赖，§三）。

**为何 `early_ack` 保留 `unknown` 作合法值**：8 个族未必都有一手协议锚点。**显式 `unknown` 远好于编值** —— 编值 = 制造一份看似可信、实则无源的声明面（闸永远绿，且后人无法分辨哪条是真锚点）。本仓既有同型文化：`status: planned | excluded`、`protocol_version` 锁表外取值的显式处理。

### 2.3 D-2 · 边类语义来源

**裁决**：本仓**不定义** `rfe` / `fr` / `ws` 的语义。

- `ADR-CEP-13` 决策 2 的公式**已是真源**；在其之外另写定义 = **第二真源**（禁止项）。
- 边类语义需要**操作级模型**承载（线程 / 操作 / 读写事件 / 值）⇒ 本仓无 ⇒ 此时写下的定义**没有任何断言能检验** ⇒ 是无判据的空文。
- 选哪个 memory model 家族（ARM / POWER / C++）本身是**架构决策** ⇒ 不在 I-6 内顺手做掉 ⇒ 归入 §三 的 backlog。
- ⇒ `combination_contract.compound_consistency` 只登记**取值域**（`edge_classes` 枚举）+ **语义锚点引用**（写在 ADR-CEP-12 的追加 SU，⛔ 不写进 schema —— schema 不承载文档指针）。

### 2.4 D-3 · 载体落点与实施顺序

| invariant | 载体 | 判据形态 |
|---|---|---|
| `INV-CEP-13-IC3` | schema `combination_contract.bridging.early_ack` 枚举 + 逐实例声明 | ① 枚举封闭（**3** 值）② **本仓权威域**的实例（`repo_owned_instance_ids` ∪ `authority_migrations[].instance_id`，口径同 `scripts/test/cep_decl_alignment_check.sh:257-258` ⇒ 未来权威迁移不漏检）**必须**声明 `bridging.early_ack`（值可为 `unknown`）③ **`unknown` 计数机器化 = 专用 expected fixture**（⛔ **不读台账散文** —— 台账是 append-only 散文，不可机检）：新增 `test/cep_combination_contract_expected.json`（`schema: cep-combination-contract-expected/v1`）· 域**双向集合相等**（`repo_owned == fixture ids`，防「新增实例无覆盖且绿」）· 逐实例 `early_ack` 期望值 · `unknown` **精确计数** ⇒ 载体形态同 `test/cep_flow_support_expected.json` 先例（`test/test_cep_instance_declaration_contract.cc:576-606`）|
| `INV-CEP-13-IC1` | schema `combination_contract.compound_consistency` + 条件判据 | ① `adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空 ② `edge_classes` ⊆ `{rfe, fr, ws}`（jsonschema 枚举自动）③ 语料 `_fill_when` 已改（§2.8 P-2）|

**实施顺序**（同一 change、同一 Entry）：

- **步骤 0（⛔ 判据成立的机械件，缺则本单元「已落地」是假宣称）**：
  1. 新增带 `// INV-CARRIER: INV-CEP-13-IC1` 与 `// INV-CARRIER: INV-CEP-13-IC3` 刚性标记的载体（`test/` 或 `scripts/test/`），且每个标记的**邻域 ±8 行**须含断言宏（C/C++ `REQUIRE`/`CHECK`/`static_assert` 或 shell 非零退出式）—— per `scripts/test/cep_invariant_map_check.py:117-122`「恒真占位」判红机制；
  2. 从 `test/cep_invariant_deferrals.json` **删除这 2 条**登记（该文件现恰有这 2 条 ⇒ 无歧义）；
  3. `python3 scripts/test/cep_invariant_map_check.py --generate` 重生成 `test/cep_invariant_assertion_map.json`；
  4. ⛔ **并集闸硬约束**：`载体 ∪ ⭭ == 66` 且**交集为空**（`scripts/test/cep_invariant_map_check.py:152-165`）⇒ 只删登记不加载体 ⇒ 闸立刻红（「既无载体也无 ⭭ 登记」）；只加载体不删登记 ⇒ 闸判「⭭ 与载体重叠」红。
- **步骤 1**：`combination_contract` 的 schema 面（13→14）+ ADR 追加 SU（§2.7 + §2.4 的 H-7）；
- **步骤 2**：`test/test_cep_schema.cc` 的派生断言改形（§2.5 的 5 个断言点 / 2 个用例标题）；
- **步骤 3**：条件判据（`adopted` ⇒ 必填）+ **本仓权威域**实例补齐 + `unknown` 计数的机器化断言；
- **步骤 4**：`test/cep_litmus_corpus.json` 的 `_fill_when` 修订 + `litmus_falsifier.hh` 头注释补「通道图 ≠ 操作图」（§2.8 P-2）；
- **步骤 5**：**golden 零改动**（`test/cep_instance_golden.json` 的 `_scope = [moesi_gpu, mesif]`，与本仓权威域**不相交** ⇒ 本单元只动权威域实例 ⇒ golden 闸不受影响 ⇒ ⛔ **不得**「重生成」golden —— 方向① 会因「键不在 13 键表内」判红：`scripts/test/cep_instance_golden_check.py:80-83`）。

**⛔ IC1 不依赖 IC3 的判定逻辑**，两者只共享**同一次** schema 变更 ⇒ 无实施顺序耦合；建议 IC3 先（枚举简单）→ IC1 后（需写语义边界说明）—— ⚠️ 该顺序指**判据与声明的撰写/补齐顺序**，⛔ **不是**把 schema 分两次改（两个子对象在**同一次** schema 变更中落地）。

### 2.5 D-4 · `INV-CEP-12-IC1` 的联动边界 + 派生断言的**完整清单**

| 断言点 / 用例 | 现状 | 处置 |
|---|---|---|
| **IC1 的机器载体**（`test/test_cep_instance_declaration_contract.cc:98-115`） | 断言 `required.size() == 13u` + 键集合逐名一致 + 字段组并集 13 | **零改动**（`required` 仍 13、键集合仍逐名一致）⇒ AC-6「只联动不改语义」**成立** |
| IC1 的 `reason` / `trigger_condition` / `ledger_entry` | deferrals 登记行 | **逐字不动**（本单元是**删除**该登记，不是改写）|
| `test/test_cep_schema.cc:234` 用例「schema 必含 13 个顶层属性」+ `:252`（`props.size() == groups.size()`）+ `:257`（`required.size() == groups.size()`） | 两处绑在**同一个** `groups` 列表上 | **拆成两个列表**：`required` 13（不变）+ 可选属性 1（新增）⇒ 用例**标题**改为「必含 13 个顶层属性 + 1 个可选声明项」|
| `test/test_cep_schema.cc:418` 用例「schema 字段组完整性(7 组 / 13 个顶层属性)」+ `:423`（`props.size() == 13`）+ `:447`（`union_ids.size() == props.size()`） | **两处都绑死 13** | `:423` ⇒ 14；`:447` ⇒ 改为「**分组并集 == `required` 集合**」（不再与 `props.size()` 比）⇒ 用例**标题**改为「7 组 / 13 个必含属性 + 1 个可选属性」 |
| `test/test_cep_schema.cc:454`（`REQUIRE(adr12.find("13 个顶层属性") != npos)`） | grep ADR 正文 | **保持绿**（ADR 正文不改）⇒ ⚠️ 会**误导**（读者看到正文 13 / schema 14）⇒ 追加 SU 中澄清口径（§2.1 末 + §2.7.1 item 2）|
| `docs/adr/ADR-CEP-12-...md:158` 口径修订括注「(= schema 实际属性数)」 | 属**正文**（⛔ 不可改）⇒ schema 14 化后与事实**字面相撞**，而 `:454` 的正向 grep 使该矛盾**长期绿、不可见** | 由追加 SU **显式列为「被取代表述」**（自本段起仅指必含属性数）⇒ 见 §2.7.1 item 2 ⛔ **不改正文** |
| `test/test_cep_schema.cc:455`（负向断言：ADR 全文**不得**含「6 个字段组」串） | 负向守卫 | **保持绿** ⇒ ⚠️ **落盘检查项**：追加 SU 的全文**不得**含「6 个字段组」串 |
| `test/test_cep_schema.cc:401`（`combination_capabilities` 恰 3 维） | — | **零改动**（本次不动该对象）|
| 4 处 per-family `required.size() == 13u` | `test_cep_cxl_dcoh_instance_decl.cc:93` · `test_cep_mesif_instance_decl.cc:137` · `test_cep_tilelink_instance_decl.cc:79` · `test_cep_instance_declaration_contract.cc:105` | **零改动** |
| `scripts/test/cep_decl_alignment_check.sh:11/:20/:261/:603` 注释里的「13 个 required 顶层属性」 | 注释 | 可选更新措辞（口径仍真：required 仍 13）|

### 2.6 D-5 · 冻结面五步链的授权顺序

⛔ **architect 不预留 Entry 编号** —— 台账是 builder 的 append-only 落点，预留会造成两个写入者 + 撞号。

- Entry 编号 = **落库时的实测最大值 + 1**（**以落库时实测为准**；本记录撰写时实测最大 = `Entry #513` ⇒ 预计 `#514`，**不写死** —— 时点事实会腐化）。
- 顺序（不可颠倒）：
  1. 落台账 Entry（写明授权范围 = `include/tlm/cep/protocol_instance.schema.json` 的**可选**顶层属性扩展 + `ADR-CEP-12` / `ADR-CEP-13` 的追加 SU）；
  2. 改 schema + 测试 + 实例 + 两份 ADR 追加 SU（⛔ 原文逐字不改）；
  3. commit；
  4. `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #5xx"`；
  5. **清单单独 commit**（⛔ 台账与清单不得合并提交）；
  6. 无参复验 rc=0。

### 2.7 两份 ADR 的追加式 SU 的**规范文本**（供 builder 直接追加，⛔ 原文不改）

#### 2.7.1 `ADR-CEP-12` 追加 SU

> 起草要点（措辞可微调但**语义不得漂移**）：
>
> 1. **性质**：追加式修订（per `AGENTS.md`「ADR 不可变 + Status Update」先例，形态照本 ADR §8）—— §2 决策 1 的 yaml 列举、§3 `INV-CEP-12-IC1` ~ `IC3` 的编号与数量、§4 抓取点、§6 决策汇总、§7 修订记录 **原文不改**；**自本段起以本段文本为准**。
> 2. **新增声明项**：`combination_contract`（**可选**顶层属性）—— `properties` 由 13 项增至 14 项；**`required` 仍为 13 项** ⇒ `INV-CEP-12-IC1` 的「13 个顶层属性」**逐字为真、机器载体零改动**。⚠️ **口径澄清（两处，缺一即产生长期绿的自相矛盾）**：① IC1 的「13 个顶层属性」指**必含**（`required`）属性数；schema 属性**总数**为 14，第 14 项为**可选**声明项。② **被取代表述**：§3 `INV-CEP-12-IC1` 的口径修订括注「(= schema 实际属性数)」—— 自本段起该括注**仅指必含属性数**（⛔ 正文不改；`:158` 属正文 ⇒ 以本段为准）。
> 3. **子声明项**：`bridging.early_ack`（**3** 值枚举：`allow-early-write-ack` / `deny-early-completion` / `unknown`，前两项与 `ADR-CEP-13` 决策 4 表的两行「允许提前写 ack」/「不允许提前完成」**语义逐字对应**，kebab-case 转写）与 `compound_consistency`（`adopted` / `edge_classes` / `verification_scope`）。
> 4. **消费方向**：桥 A↔B 的并发粒度读 **B**（对端）实例的 `early_ack`（per `ADR-CEP-13` 决策 4「对端协议是否允许提前 ack」）。
> 5. **⛔ 边类语义不在本 ADR 定义**：`edge_classes` 只登记**取值域**；语义锚点 = `ADR-CEP-13` 决策 2 的公式（⛔ 本 ADR 不复述、不重定义 ⇒ 避免第二真源）。定义工作归独立立项（backlog `cep-memory-model-operational-trace`）。
> 6. **⛔ 禁止派生填充 litmus 语料**：`combination_contract` **不得**被用作 `test/cep_litmus_corpus.json` 的语料来源 —— 工装的图是**通道依赖图**（顶点 = 实例 × 网络 × VC），而 `rfe/fr/ws` 是**操作级**关系 ⇒ 由声明项派生的环**不是**对复合一致性公理的证伪（假绿）。
> 7. **落盘检查项**：本段全文**不得**含「6 个字段组」串（保 `test/test_cep_schema.cc:455` 的负向断言绿）。
> 8. **关联**：登记册 §6.4 队列 2 的 **I-6**；台账 Entry（builder 落库时取实测最大 + 1）。

#### 2.7.2 `ADR-CEP-13` 追加 SU（**v1 遗漏，本轮补**）

> 1. **被取代的判读**：§Verification 的 **V-13-4**（复合一致性模型的**可判定性**）与 **V-13-5**（跨域桥接并发粒度由对端是否允许提前 ack 决定）当前标注「⚠️ **机器载体不可得**」⇒ 本段登记：该判读**已被 I-6 的载体取代** —— 载体 = `combination_contract` 的两个子声明项（schema 侧）+ `INV-CARRIER: INV-CEP-13-IC1/IC3` 的判定级载体（实现侧）。
> 2. **⛔ 被取代的只是「载体不可得」这一判读**，V-13-4 / V-13-5 的**判据内容**（公理形式、保守/激进粒度规则）**逐字不变**。
> 3. ⚠️ **诚实边界**：载体成立**不等于**公理已被判定 —— 复合一致性公理的**可判定性**仍受限于「本仓无操作级模型」（背log `cep-memory-model-operational-trace`）⇒ ⛔ 不得表述为「公理已成立 / 已被检验」。
> 4. **关联**：登记册 §6.4 队列 2 的 **I-6**；台账 Entry（与 `ADR-CEP-12` 的 SU 同一 Entry 授权即可）。

### 2.8 ⛔ 本单元的禁止事项（判据化的「不得」清单）

| # | 禁止 | 理由 | 判据形态 |
|:--:|---|---|---|
| P-1 | 把第 14 个属性加进 `required` | ⇒ IC1 语义变更 + 4 处断言 + 跨仓（§2.1）| `test/test_cep_schema.cc` 的 `required.size()` 断言 + 裁决记录 §2.1 |
| P-2 | 由 `combination_contract` 派生填充 litmus 语料 | 范畴错误 ⇒ 假绿（§一 F-3）| `test/cep_litmus_corpus.json` 的 `_fill_when` 文案 + `litmus_falsifier.hh` 头注释 + 新增判据（语料仍空）|
| P-3 | 在本仓定义 `rfe` / `fr` / `ws` 语义 | 无承载物 ⇒ 第二真源（§2.3）| 追加 SU §5 措辞 + 裁决记录 |
| P-4 | 把「声明面存在」表述为「公理已成立 / 已被检验」 | 声明 ≠ 判定（I-6 AC-2 已警告「处置 ≠ 建模」，此处扩为「声明 ≠ 判定」）| **载体点名**：`test/test_cep_tools_contract.cc`（同款禁述判据的既有载体，per `ADR-CEP-13` V-13-3）+ 登记册/报告口径 |
| P-5 | 为 `early_ack` 编造协议事实值 | 编值 = 看似可信的无源声明面 ⇒ 闸永远绿 | `unknown` 计数**机器化**（断言 == 台账 Entry 登记值）+ 逐实例值可溯源 |
| P-6 | 在本单元产 litmus change | 归 I-6a（已归档）| 台账 / change 清单 |
| P-7 | 给**镜像域实例**（`moesi_gpu` / `mesif`）或 `test/cep_instance_golden.json` 加 `combination_contract` | ① golden 方向① 判「键不在 13 键表内」红（`scripts/test/cep_instance_golden_check.py:80-83`）② 镜像闸 ⑥ 判「键只存在于一侧」红（`scripts/test/cep_decl_alignment_check.sh:551`）⇒ 等 ArchForge 侧更新后再同步 | golden 闸 + 镜像闸（**零改动**即可保持绿 ⇒ 本身就是判据）|
| P-8 | 改 `ADR-CEP-13` 正文的 V-13-4 / V-13-5 | ADR 不可变 ⇒ 只能追加 SU（§2.7.2）| `git diff` 对该文件的改动**只增行** |
| P-9 | 声明「本实例不参与跨域桥接」为 `early_ack` 的一个取值 | 无适用 predicate ⇒ 不可判的声明项 ⇒ 本单元**只允许 3 值**（§2.2）| schema 枚举封闭断言 |

---

## 三、决策 2 · backlog 登记（操作级模型 + 边类语义定义）

**裁决**：本仓不定义边类语义 ⇒ 该工作**独立立项**，本单元只消费其未来的产物。

| 项 | 内容 |
|---|---|
| **backlog 名** | `cep-memory-model-operational-trace`（拟） |
| **中文名** | CEP 内存模型操作级轨迹 + `rfe/fr/ws` 边类语义定义 |
| **归属** | planner（improvement 起草）；⛔ architect **不代写** `.rddf/improvements/**`（该面属 planner 写集，工具层门禁已实测拒绝）|
| **落盘状态** | ⏳ 待 planner 立项（草稿全文见 §附录 A，**可直接落盘**）|
| **与 I-6 的关系** | I-6 只登记**取值域** + 锚点引用；本 backlog 负责**定义** + 提供**操作级模型**（线程 / 操作 / 读写事件 / 值）|
| **触发条件** | 需要「复合一致性公理**可判**」时（当前 `combination_contract.compound_consistency.adopted` 只能声明**采用**，⛔ 不能判定）|
| **连带依赖** | 「本实例是否参与跨域桥接」的判定面（`early_ack` 的 `not-applicable` 语义缺口，§2.2 / §2.8 P-9）|

---

## 四、决策 3 · I-7 拆 a / b

### 4.1 为什么必须拆（裁决背景）

- 档 2 判据要什么：mesif 侧判定行 = `kind=transition ∧ event=Wr ∧ to==M`，违例 = 写前视图里存在他人 sharer（`test/test_cep_tier2_observation.cc:63,99-103`）⇒ 本质 =「**写者拿到独占写权限时目录里还有别人**」。
- 同一判据在 TileLink-C 侧**天然可表达**：Tip = 持 R/W 副本、Branch = 只读副本 ⇒ 「Tip 获取瞬间 `sharers & ~self != 0`」即 SWMR 违例形态 ⇒ **tracker 侧有判据空间**。
- **但驱动面是空的**：`TreeManager::set_sharer` 全仓**只有测试构造点**（`test/test_cep_cxl_dcoh_tracker.cc:279`，作 CHAIN 对照），**无任何协议流驱动**；mesif 的驱动在 `include/tlm/cep/mesif_protocol_engine.hh:720-730`（引擎状态机驱动）。
- 而 tilelink 引擎受**两条已签批契约**约束（**逐字引用**，避免宽转述）：
  - `openspec/specs/cep-tilelink-runtime-engine/spec.md:14`（边界行）：「**不建** PE 本地树状态机（Nothing/Branch/Tip —— 规约层真源，运行面只读）· **不携带源态**（TileLink `Cap`「源不定」⇒ 只声明事件类别）」；
  - 同文件 **Scenario TLE-05**：「引擎事件面 SHALL 只声明事件类别（单值），SHALL NOT 预设 from-state」+ 机械载体「引擎源文件 grep **0 处** `TileLinkStateSpec::State` / transition matrix / `transition_cell*` 引用」。
  ⇒ 窄读下的推论仍成立：**任何带源态迁移的驱动放进引擎都违反 TLE-05** ⇒ 驱动**不进引擎**。（「源不定」是 TileLink-C **规范事实**，per `ADR-CEP-13` 同型先例 D1 §8.1.4 ⇒ 推翻契约 = 与规约层语义冲突。）
- ⇒ **I-7 原改善稿低估了一个数量级**（它假设「引擎已有 tracker 同步、只差两处观测落点」）⇒ 拆 a/b。

### 4.2 I-7a（**planner 修订 AC 后即可开工** · builder）

| 项 | 内容 |
|---|---|
| 范围 | ① `TreeManager::view_of`（新增 const 纯查询）② 档 2 **仍单族**（`evidence_tiers` 不动）③ 登记「扩面的真实前置」指针 |
| 载体 | `include/tlm/cep/tilelink_tree_manager.hh`（~10 行，同形于 `FullMapDirectory::view_of`：`include/tlm/cep/sharer_tracker.hh:681`）|
| 零改动面 | `include/tlm/cep/sharer_tracker.hh`（⇒ 两处 `count_member_virtuals == 5` 断言、ADR-CEP-04 §3、I-7 AC-9 全不受影响）· `docs/adr/**` · `docs/architecture/**` |
| 边界纪律 | ⚠️ **不得**在 I-7a 内顺手扩档 2（那是 I-7b）⇒ 避免「登记了但没判据」的半截状态 |
| **开工前置** | planner 先修订 I-7 的 AC-1 / AC-9「修改文件」白名单（加 `tilelink_tree_manager.hh`）+ 按 **I-6a 先例**（登记册 §6.4 的 sub-id「不占独立编号位」）把 **I-7a / I-7b** 登记为 I-7 的 sub-id + 载体表加行 ⇒ 避免「I-7 未完成但 I-7a 已落地」与队列闸 P-2a 串行判据冲突 |

### 4.3 Q-a 的形态裁决

**裁决**：**不动** `SharerTrackerIf`；在 **具体类** `TreeManager` 上补 `LocalView view_of(std::uint64_t addr) const`，与 `FullMapDirectory::view_of`（`sharer_tracker.hh:681`）/ `SparseSnoopFilter`（`:836`）/ `HierarchicalDirectory`（`:1084`）**同形**；消费方按 mesif 先例持**具体类型**（`mesif_protocol_engine.hh:322-329`：「按具体实现接线是诚实的」）。

- ⛔ **不改** `SharerTrackerIf`（理由见 §一 F-1 的三重否决）。
- ⛔ **不做**「可选能力查询 + `dynamic_cast`」的扩展接口（双接口 ⇒ 复杂度上升，而 `view_of` 语义仍需定义 ⇒ 无净收益）。
- ⛔ **不做**「档 2 放弃扩面 + 清理 `coverage_map`」（`tilelink_c` 的 `tier2_applicable = true` 已登记且其理由（`TreeManager` 的 completeness = EXACT）**仍成立** ⇒ 无口径矛盾；阻断来自**引擎契约**，与 tracker 能力无关）。

### 4.4 I-7b（独立立项 · planner / backlog）

| 项 | 内容 |
|---|---|
| 范围 | ① 新建 **TileLink-C home 侧状态驱动组件**（消息流 → 树形 sharer 分布：谁在 A/B/C/G 之后决定某节点是 Tip / Branch / None）② tracker 侧观测面 ③ `test/cep_evidence_tiers.json` 新增 `tilelink_c` 档 2 条目 |
| 档 2 读法 | 「**两族 / 两种载体形态** / 单拓扑 / **无通过态**」；⚠️ 明标与 `mesif-observation` **不同型**（tracker 侧，无引擎态迁移）⇒ ⛔ 禁「同强度」「全族达成」「检测力已证」 |
| 溯源纪律 | 观测点必须在**驱动边界**（调用前后的快照）⇒ ⛔ 不得放在 `TreeManager` 内部（那会观测它自己维持的不变量 ⇒ 同义反复 ⇒ 必绿）|
| 载体类型 | `carrier_kind = "tracker-observation"`（与 `mesif-observation` 的 `protocol-engine-observation` 区分）|
| 落盘状态 | ⏳ 待 planner 立项（草稿全文见 §附录 B，**可直接落盘**）|

---

## 五、待用户 ratify 项（**R-2**）

⛔ **R-1（schema 版本递增）已按决策 1 的精化撤销** —— 无「版本」可递增（schema 文件无 version 字段）；变更对象是 **`properties` 属性总数 13 → 14**。

| # | ratify 对象 | 需确认的具体取值 |
|:--:|---|---|
| **R-2a** | **顶层属性名** | `combination_contract`（与既有 `combination_capabilities` 平行）|
| **R-2b** | **子声明项与字段名** | `bridging.early_ack` · `compound_consistency.{adopted, edge_classes, verification_scope}` |
| **R-2c** | **`early_ack` 取值域** | `allow-early-write-ack` · `deny-early-completion` · `unknown`（**3** 值；前两项与 `ADR-CEP-13` 决策 4 表的两行「允许提前写 ack」/「不允许提前完成」**语义逐字对应**（kebab-case 转写，⛔ 非 ADR 原文直译）；⛔ 已删 `not-applicable` —— 无适用 predicate ⇒ 不可判的声明项）|
| **R-2d** | **`edge_classes` 取值域** | `rfe` · `fr` · `ws`（**只登记取值域**，⛔ 本仓不定义语义）|
| **R-2e** | **必填策略** | 顶层**可选**（但 `minProperties >= 1`）；`bridging.early_ack` 与 `compound_consistency.adopted` 在**声明了对应子对象时**必填；`adopted == true` ⇒ `edge_classes` 非空 ∧ `verification_scope` 非空（闸判据）|
| **R-2f** | **消费方向** | 桥 A↔B 的并发粒度读 **B**（对端）实例的 `early_ack` |

⇒ **ratify 之前 builder 不得开工 I-6**（字段名与取值域是契约面，⛔ 不得由实施侧自决）。

---

## 六、交办与遗留

| # | 交办对象 | 内容 |
|:--:|---|---|
| H-1 | **用户** | ratify R-2a ~ R-2f |
| H-2 | **planner** | 立项 I-6 的 openspec change（占 1 slot）+ 修订 AC：**步骤 0 的四件机械件**（INV-CARRIER 双标记 + 邻域断言 + 反向 fixture + deferrals 删除 + `--generate` 重生成）· AC-3（改为「属性总数 13→14 + `required` 仍 13 + 反 fixture」）· AC-4（五步链 ×1，冻结产物数**不变**）· AC-6（IC1 载体**零改动** + `test_cep_schema.cc` 的 **5 个断言点 / 2 个用例标题**改形清单）· 新增「`early_ack` 逐实例声明 + `unknown` 计数机器化」判据 · 新增「golden **零改动** + P-7」判据 |
| H-3 | **planner** | 立项 §附录 A / §附录 B 的两份 backlog improvement（⏳ 草稿已备好）|
| H-4 | **planner** | 更新登记册 §6.4 的 **I-6 / I-7** 行「前置」列（② 裁决已落 ✅ ⇒ 前置②③ 已满足；I-6 剩 ratify，I-7 剩 I-7b 立项）+ 登记 **I-7a / I-7b** sub-id（per I-6a 先例，不占独立编号位）+ 载体表加行 |
| H-5 | **planner** | 同步台账 `obj-cep-platform.md` 中 `INV-CEP-13-IC1` / `INV-CEP-13-IC3` 的两行 ⭭ 登记（**按 id 检索**，⛔ 不用行号锚 —— 该文件万行级、行号会漂移）→「有牙」|
| H-6 | **builder** | I-7a 的实施（`TreeManager::view_of`）—— ⛔ 须在 H-2 修订 I-7 范围 + H-4 登记 sub-id 后开工 |
| H-7 | **builder** | I-6 的实施（§2.4 的步骤 0~5 + §2.6 的六步五步链 + §2.7 的两份 ADR SU）|

---

## 七、审查轨迹

| 轮次 | 审查者 | 判定 | 处置 |
|:--:|---|:--:|---|
| 1 | **oracle**（对抗式 · 目标场景失败模式） | **APPROVE** + 6 条高/中影响修正 | ✅ 全部并入 v2（S1-1 deferral 机械件 · S1-2 golden 事实错误 · S1-3 `ADR-CEP-13` SU · S2-4 断言清单补全 · S2-5 `early_ack` 取值域删 `not-applicable` + 消费方向 · S2-6 I-7a 口径 + sub-id · S2-7 TLE-05 逐字引用）+ S3 低优先项（引用漂移 · `minProperties` · `unknown` 计数机器化 · 权威域口径 · P-4 载体点名）|
| 1 | **critic**（`review_profile: build, arch` · 契约冲突） | **APPROVE_WITH_CONDITIONS**（**无 BLOCKER** · `mergeable: yes` · `scope_exceeded: false`）| ✅ 4 条 SHOULD_FIX + 4 条 NITS 全部并入 v3（见 §八）|

> ⚠️ 本记录是**裁决记录**（含已落盘的形态决策），⛔ 不是 ADR ⇒ 不适用「ADR 完成的标志」5 条（Status 转换 / 关联 change 落库 / AC 映射回填）；后者待 change 立项后由 planner + architect 补齐。

---

## 八、本记录 v1 → v2 的修订摘要（oracle 轮 1 驱动）

| # | v1 的问题 | v2 的处置 |
|:--:|---|---|
| 1 | §2.4 实施顺序**漏掉** deferral 机械件（INV-CARRIER 双标记 + deferrals 删除 + `--generate`）⇒ 按字面执行则「⭭ 由 2 降为 0」不成立 | §2.4 新增**步骤 0**（四件套 + 并集闸硬约束说明）|
| 2 | §2.4 步骤 3「golden 重生成」= **事实错误**（golden `_scope = [moesi_gpu, mesif]`，与本仓权威域不相交 ⇒ 不受影响；且真去重生成会破方向①） | 改为「golden **零改动**」+ 新增禁止项 **P-7** |
| 3 | §2.7 只规划了 `ADR-CEP-12` 的 SU，**漏** `ADR-CEP-13`（其正文 V-13-4 / V-13-5 写「机器载体不可得」） | 新增 §2.7.2 + 交办 H-5 |
| 4 | §2.5 漏了 `test/test_cep_schema.cc:418` 用例的 `:423` / `:447` 两处绑定 | §2.5 补全为 **5 个断言点 / 2 个用例标题** + SU 负向串检查项 |
| 5 | `early_ack` 的 `not-applicable` 无适用 predicate；IC3「对端」方向未落 | 取值域删为 **3 值** + 新增 **R-2f 消费方向** + 禁止项 **P-9** + backlog 连带依赖 |
| 6 | I-7a「可立即实施」与 H-5「须先 planner 修订」自相矛盾；I-7a 无登记面 | §4.2 改口径 + 按 I-6a 先例登记 **sub-id**（交办 H-4）|
| 7 | TLE-05 被宽转述（写成「引擎不持状态」） | §4.1 改为**逐字引用**边界行 + TLE-05 两处原文 |
| 8 | 引用漂移：`test_cep_tilelink_adapter.cc:445` → **:447**；`cep_decl_alignment_check.sh:38-42` → **:263-268**（`:38-42` 是 ArchForge 缺席的 SKIP 块）| 已更正 |
| 9 | 未记录「**嵌套进既有容器**」这一 rejected alternative 的代价对比 ⇒ 取舍不可审计 | 新增 **§2.1.1「被否掉的替代方案」**（三方向各记一次：嵌套进既有容器 / 另立声明文件 sidecar / 进 `required`）|

### v2 → v3 的修订摘要（critic 轮 1 驱动 · `APPROVE_WITH_CONDITIONS`，无 BLOCKER）

| # | critic 的 finding | v3 的处置 |
|:--:|---|---|
| 10 | **SHOULD_FIX ①**：`ADR-CEP-12:158` 口径修订括注「(= schema 实际属性数)」在 schema 14 化后**字面相撞**，而 `test_cep_schema.cc:454` 的正向 grep 使该矛盾**长期绿、不可见** | §2.5 增该行的处置；§2.7.1 item 2 增「被取代表述」条目（⛔ 正文不改，以 SU 为准）|
| 11 | **SHOULD_FIX ②**：未记录 **sidecar 模式**（另立声明文件）为何被拒 ⇒ 取舍不可审计 | §2.1.1 增该行；**沿用仓内既有裁决**（`ADR-CEP-12 §11`「为何不能走『另立声明文件』」）⇒ ⛔ 非本次新判 |
| 12 | **SHOULD_FIX ③**：「断言 `unknown` 计数 == 台账 Entry 登记值」**不可机检**（台账是散文）；先例实为**专用 expected fixture** + 双向集合相等 | §2.4 判据③改写为 `test/cep_combination_contract_expected.json`（`schema` tag + 域双向集合相等 + 逐实例期望值 + `unknown` 精确计数），形态同 `cep_flow_support_expected.json` |
| 13 | **SHOULD_FIX ④**：「`early_ack` 前两项**逐字取自** ADR」是过度声明（表行是中文，enum 是 kebab-case 转写，`deny-early-completion` 含 ADR 没有的 "completion"） | §2.2 枚举 description / §2.7.1 item 3 / R-2c 三处统一改为「**语义逐字对应**（kebab-case 转写，锚 `ADR-CEP-13` 决策 4 表两行）」 |
| 14 | **NITS** 4 条 | ① §2.4 末补「顺序建议指**撰写顺序**，非 schema 分两次改」② §2.6 Entry 改为「以落库时实测为准」③ H-5 行号锚 → **按 id 检索** ④ 附录 A AC-3 补**迁移路径**（双形态并存 + 分流计入）|

---

## 附录 A · backlog 草稿（`cep-memory-model-operational-trace`，可直接落盘）

> ⛔ **落盘者 = planner**（`.rddf/improvements/**` 属 planner 写集，architect 写集不含它 —— 工具层门禁已实测拒绝）· 本附录是**可直接采用的 5 段全文**。

```markdown
---
name: cep-memory-model-operational-trace
priority: P2
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-13
状态: "🟡 **待立项**(2026-11-21 · architect 裁决提出 · 关联登记册 §6.4 队列 2 的 I-6 决策 2 · 决议 = 本仓**不定义** `rfe/fr/ws` 语义,独立立项解决)"
created: 2026-11-21
---

# cep-memory-model-operational-trace: 内存模型操作级轨迹 + 边类语义定义

> **范围纪律**: ⛔ **不改任何已签批 ADR 正文**(边类语义若需落 ADR ⇒ 另立新 ADR)· ⛔ **不产 run** · ⛔ **新增 invariant 数 = 0** · ⛔ **不改** `combination_contract` 的取值域(已 ratify)

## 架构依据

`ADR-CEP-13` 决策 2 采用的复合一致性公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 是**操作级**判据:
`rfe` / `fr` / `ws` 是**内存操作之间的关系**(read-from external / from-read / write-serialization),
`ppo_com(t) ≡ ppo_i` 是**程序序**——判据的输入必须是**执行迹(operations + reads/writes + values)**。

⛔ **当前缺口**:本仓**无操作级模型** ⇒
  ① 边类语义**无处承载**(写在文档里的定义将无任何断言能检验 ⇒ 事实上的第二真源);
  ② `include/tlm/cep/litmus_falsifier.hh` 的图顶点 = `(instance, network, vc)` = **通道依赖图**
     ⇒ 与操作图**不同型** ⇒ 由声明项派生的边**不能**证伪该公理(假绿)。

## 范围

- **In-Scope**:操作级轨迹模型(线程 / 操作 / 读写事件 / 值 / 序关系)· `rfe/fr/ws` 的**语义定义**(落到操作模型上)· 操作图与 litmus 工装的**接缝**(取代当前「通道图 + 语料」混图)· `verification_scope` 的可执行化
- **Out-Scope**:⛔ 通道依赖图(既有 `channel_dependency_graph.hh`)· ⛔ 4 件套 run · ⛔ 各协议族的协议语义 · ⛔ `combination_contract` 的取值域

## Why

**不做的代价**:`INV-CEP-13-IC1` 的「结构化声明项」已由 I-6 落地(声明**采用**该判据),但**判定**仍无载体
⇒ 档 2/档 3 体系里「复合一致性」永远只是**声明**而非**判定** ⇒ 跨域组合的正确性维度对平台永久不可用。

**为什么必须独立立项**:选 memory model 家族(ARM / POWER / C++)本身是**架构决策**;
操作级轨迹是**新载体**(与通道依赖图正交)⇒ 规模与审批链都与 I-6 不同级 ⇒ 混进 I-6 会把一个载体面变更绑到规格面修订上。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **memory model 家族选型** | 架构决策(需独立裁决):候选 = ARM / POWER / C++ ⇒ 决定 `ppo_com` 的定义面与 `fr`/`ws` 的精确语义 |
| 2 | **操作级轨迹模型** | 线程 → 操作(读/写 + 地址 + 值)→ 事件图(节点 = 操作,边 = `ppo`/`rf`/`fr`/`ws`/`rfe`) |
| 3 | **`rfe/fr/ws` 定义落点** | 定义写在**操作模型**上(可被断言检验)⇒ ⛔ 不在 ADR 正文外另立第二真源 ⇒ 若需 ADR ⇒ 新立 ADR |
| 4 | **litmus 工装接缝** | `litmus_falsifier.hh` 的图**改为操作图**(通道图仅作输入侧证据)⇒ 语料改为**操作迹** ⇒ ⛔ 不得混图 |
| 5 | **`verification_scope` 可执行化** | `combination_contract.compound_consistency.verification_scope` 从**散文**变为**可枚举范围** |

## Acceptance

> **判据纪律**:每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红(红因有签名)** ⇒ 还原 ⇒ **必须绿**」

- [ ] **AC-1(家族选型有据)**:家族选型的 ADR(新建)逐条列出「哪条公理依赖哪个家族语义」⇒ ⛔ 不得只写结论
- [ ] **AC-2(操作图 ≠ 通道图)**:机检判据 —— 操作图顶点 ≠ `(instance, network, vc)` ⇒ ⛔ 混图必红
- [ ] **AC-3(语料 = 操作迹)**:语料文件的每条记录含**操作 id / 类型 / 地址 / 值**;⛔ `from_net`/`to_net` 形态不得再出现;⚠️ **迁移路径**:现存语料结构(`litmus_falsifier.hh` 的 `Extra{from_net, to_net, from_vc, to_vc}`)与操作迹**不同型** ⇒ 迁移期须**双形态并存**(旧字段读作通道边 + 新字段读作操作边)且**分流计入**(⛔ 不得把通道边当操作边判环),迁移完成后删旧字段
- [ ] **AC-4(判据可判别)**:一个**已知违例**的操作迹 ⇒ 工装报 `falsified = true`;一个无环迹 ⇒ 报「**未证伪**」(⛔ 不是「已证明」)
- [ ] **AC-5(不发明语义)**:边类定义逐条引用**所选家族的一手文献/规范锚点** ⇒ ⛔ 无锚点者**不得**写入
- [ ] **AC-6(零回归)**:ctest 全绿(⛔ 不写死数字)· 全部 `scripts/test/cep_*.sh` rc=0 · `rddf_stage_gate.sh` rc=0

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-litmus-falsification` | **MODIFIED** | 工装从「通道图 + 跨域边」改为「操作图 + 操作迹」⇒ 判定对象与公理**同型** |

## Impact

| 面 | 影响 |
|---|---|
| **修改文件** | `include/tlm/cep/litmus_falsifier.hh` · 操作级轨迹模型(新)· `test/cep_litmus_corpus.json`(形态改)· `openspec/specs/cep-litmus-falsification/spec.md`(**冻结面** ⇒ 五步链) |
| **零改动(硬查)** | `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/protocol_engine_base.hh` · `include/tlm/cep/channel_dependency_graph.hh` · `docs/adr/**` 正文 |
| **冻结面** | `openspec/specs/cep-litmus-falsification/spec.md` 已在 `FROZEN_GLOBS` ⇒ 归档须走一次 `--update --authorized-by` |
| **闸** | ctest +1(操作图)· `cep_litmus_falsification_check.sh` 判据升级(两臂) |
| **KPI** | `0/3 → 1/3` ⇒ 归档 ⇒ `0/3` |
| **风险** | ① 家族选型被低估 ⇒ 规模失控 ② 操作图与通道图混用 ⇒ 假绿(**本单元的首要风险**)③ `verification_scope` 散文化 ⇒ 判据不可执行 |
```

---

## 附录 B · backlog 草稿（`cep-tilelink-home-side-state-driver`，可直接落盘）

```markdown
---
name: cep-tilelink-home-side-state-driver
priority: P2
phase: phase-10
category: core-impl
type: feature
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖:
  - ADR-CEP-20
  - ADR-CEP-12
状态: "🟡 **待立项**(2026-11-21 · architect 裁决提出 · 关联登记册 §6.4 队列 2 的 **I-7b**(I-7 拆分后的扩面部分)· 前置 = **I-7a**(`TreeManager::view_of`)落地)"
created: 2026-11-21
---

# cep-tilelink-home-side-state-driver: TileLink-C home 侧状态驱动 + 档 2 第二族

> **范围纪律**: ⛔ **不改** `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界与 **TLE-05**(事件面只声明事件类别 / SHALL NOT 预设 from-state / grep 0 处 `TileLinkStateSpec::State` / transition matrix —— 「`Cap` 源不定」是 TileLink-C **规范事实**)· ⛔ **不产 4 件套 run** · ⛔ **新增 invariant 数 = 0** · ⛔ **不扩** `perm` 4 值锁 / canonical 10 流词表

## 架构依据

档 2 = 证据阶梯的**天花板档**(协议语义级,per `ADR-CEP-20` 决策 6 的三档表)。现行载体**只有 1 族**(`mesif`,engine 侧观测面)。

⛔ **现状缺口**(architect 2026-11-21 实测):
- `tilelink_c` 在 `test/cep_evidence_tiers.json` 的 `coverage_map.families` 已登记 `tier2_applicable = true`
  (理由:`TreeManager` 的 completeness = EXACT 且无广播兜底 ⇒ 可回答「没有其他 sharer」)—— **该理由仍成立**;
- 但 **`TreeManager::set_sharer` 全仓无协议流驱动**(唯一构造点是 `test/test_cep_cxl_dcoh_tracker.cc:279`,作 CHAIN 对照实现)
  ⇒ 档 2 判据所需的「写者获取独占写权限时目录里还有别人」**没有产生点**;
- 而 tilelink 引擎受 `openspec/specs/cep-tilelink-runtime-engine/spec.md` 的边界行 + **TLE-05** 约束
  ⇒ 驱动**不能**放进引擎。

⇒ 缺口 = **缺一个 home 侧状态驱动组件**(消费 TileLink 消息流 → 维护树形 sharer 分布)。

## 范围

- **In-Scope**:TileLink-C home 侧状态驱动组件(消息流 → `TreeManager` 的 `set_sharer`/`clear_sharer` 调用 + 观测记录)· 驱动边界快照(观测点)· `test/cep_evidence_tiers.json` 新增 `tilelink_c` 档 2 条目 · `scripts/test/cep_e2e_reconcile_check.sh` 的 T4/T5 对新条目的校验
- **Out-Scope**:⛔ 4 件套 run / `hypothesis_id` / verdict · ⛔ 版本维 / 权限维 / 标签维 · ⛔ `moesi_gpu` / `cxl_*` / `nvlink_4` / `chi_rnf` / `ualink_4` · ⛔ `include/tlm/cep/sharer_tracker.hh` · ⛔ `include/tlm/cep/tilelink_protocol_engine.hh` 的引擎状态面 · ⛔ `openspec/specs/cep-tilelink-runtime-engine/spec.md`

## Why

**不做的代价**:档 2 停在**单族单点** ⇒ 「协议语义级证据」在本仓实质上只是 `mesif` 一族的一次观测 ⇒ 扩族价值归零。

**为什么必须先有 I-7a**:档 2 的快照取法必须是 **const 纯查询**(`view_of`)⇒ `TreeManager` 当前**没有**该面 ⇒ I-7a 先落地。

**为什么不放进引擎**:TLE-05 是**已签批主契约**,且其「源不定」源于 TileLink-C 规范事实 ⇒ 推翻它 = 与规约层语义冲突。

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **home 侧状态驱动组件** | 消费 TileLink-C 消息流 ⇒ 决定各节点的 Tip / Branch / None ⇒ 调用 `TreeManager` 的安装面;⛔ 组件**不得**进 `tilelink_protocol_engine.hh` |
| 2 | **观测点 = 驱动边界** | 快照取 `TreeManager::view_of`(const 纯查询)⇒ ⛔ **不得**用 `lookup`(有副作用)⇒ ⛔ **不得**把观测点放进 `TreeManager` 内部(那会观测它自己维持的不变量 ⇒ 同义反复 ⇒ 必绿) |
| 3 | **档 2 第二族条目** | `carrier_kind = "tracker-observation"` · `run_produced = false` · `has_verdict = false` · `tier = 2` |
| 4 | **读法措辞** | 「**两族 / 两种载体形态** / 单拓扑 / **无通过态**」;⚠️ `cannot_claim` 明写「与 `mesif-observation` **不同型**(tracker 侧,无引擎态迁移)」⇒ ⛔ 禁「同强度」「全族达成」「检测力已证」 |
| 5 | **T4/T5 覆盖** | 载体文件**实际存在** + 字段齐备 + 禁肯定式通过态表述 + 禁夸大短语 + 禁跨档 run 引用 |

## Acceptance

- [ ] **AC-1(档 2 覆盖 1 族 → 2 族)**:`test/cep_evidence_tiers.json` 档 2 条目数 ≥ 2;`cep_e2e_reconcile_check.sh` T4/T5 全绿
- [ ] **AC-2(非空洞判据)**:正控须机器断言 —— ① ≥1 条观测 `sharers & ~self_mask != 0` ② ≥1 个写 episode 完整(按 `(addr, write-episode)` 归并)
- [ ] **AC-3(溯源 · 反 fixture)**:每个 sharer bit 可溯源到**驱动方显式** `set_sharer` 调用;⚠️ **不得**溯源到「引擎 `apply_event → sync_tracker`」(引擎无该路径 ⇒ 旧判据须改写)
- [ ] **AC-4(负控 · 精确计数)**:3 个负控各自「同一输入、唯一差异」⇒ 违例数**精确 == 1**
- [ ] **AC-5(判定行钉死)**:判定行只取「**Tip 获取**」事件;`kind = fill` 式行**显式豁免**;计数按写 episode 归并
- [ ] **AC-6(装配期硬断言)**:3 条硬断言各 1 个反 fixture ⇒ 置 `observation_invalid` ⇒ 判「**不可判**」(⛔ 不得记 0)
- [ ] **AC-7(档 1 零改动)**:`test/cep_experiment_harness.hh` **零 diff**
- [ ] **AC-8(诚实边界)**:新增产出物内 grep 禁 `检测力已证` / `全族达成` / `所有族` / `档 2 强于档 1` / `通过态`
- [ ] **AC-9(范围纪律 · 零越权)**:`git diff --name-only` 不含 `docs/adr/**` · `docs/architecture/**` · `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/tilelink_protocol_engine.hh` · `openspec/specs/cep-tilelink-runtime-engine/spec.md`
- [ ] **AC-10(0 回归)**:ctest 全绿(⛔ 不写死数字)· 全部 `scripts/test/cep_*.sh` rc=0(数量以 `ls scripts/test/cep_*.sh | wc -l` 实测)

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-evidence-tier2-observation` | **MODIFIED** | 档 2 观测面覆盖族从 `mesif` 扩到 `mesif` + `tilelink_c`;⚠️ ⭔ 仍不产 run / ⭔ 仍无 verdict / ⚠️ **两种载体形态**(engine 侧 vs tracker 侧) |

## Impact

| 面 | 影响 |
|---|---|
| **新增文件** | TileLink-C home 侧状态驱动组件(header-only)· 档 2 观测测试 |
| **修改文件** | `test/cep_evidence_tiers.json`(+1 条目)· `openspec/specs/cep-evidence-tier2-observation/spec.md`(**冻结面** ⇒ 五步链) |
| **零改动(硬查)** | `include/tlm/cep/sharer_tracker.hh` · `include/tlm/cep/tilelink_protocol_engine.hh` · `openspec/specs/cep-tilelink-runtime-engine/spec.md` · `docs/adr/**` · `docs/architecture/**` |
| **冻结面** | `openspec/specs/cep-evidence-tier2-observation/spec.md` 已在 `FROZEN_GLOBS` ⇒ 归档须走一次 `--update --authorized-by` |
| **KPI** | `0/3 → 1/3` ⇒ 归档 ⇒ `0/3` |
| **风险** | ① 驱动组件的协议语义保真度(⭐ 主风险)② 观测点误置 `TreeManager` 内部 ⇒ 同义反复假绿 ③ 读法被读成「同强度两族」 |
```
