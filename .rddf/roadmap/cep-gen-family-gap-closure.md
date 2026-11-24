# CEP × Gen 族 Gap 闭合实施路径

> **定位**：**planner 产出**（实施路径面）—— 只回答「按什么次序、闭什么 gap、谁来做、门禁是什么」。
> **不是**真源：判据细节以 `docs/adr/ADR-CEP-*.md` 为准；论文侧证据以 `docs/research/cep-coherence-protocol-space-survey.md` 为准；
> 协议语义设计权威归 ArchForge（per `docs/adr/ADR-CEP-00-platform-charter-and-governance.md`）。
> **来源**：2026-11-23 对 Oswald 博士论文（*ProtoGen* / *HieraGen* / *HeteroGen*，178 页一手 PDF）全文逐章核对
> × 本仓 CEP 实现面（`include/tlm/cep/**` 45 头文件 + 8 实例声明面 + 47 份 `openspec/specs/cep-*` + 24 份 CEP ADR）的对照分析。
> **本轮决策**：用户 2026-11-23 批准「I1 完整闭环 + I2 登记」。
> **状态更新（2026-11-24）**：**G1 / G2 / G6 已闭**（G2 = `ADR-CEP-22` 已签发 + change 已归档 · 验收 PASS 17/17）· **G10 的本仓可做部分已闭**（Stage B''' · change `cep-hop-timing-capability-declaration` 已归档 · 验收 PASS 17/17）· **G9 按设计闭**（触发式扩容判据已在 ADR-CEP-21）· 剩余 = **G4**（待 C5 素材盘点）/ **G7**（architect 裁取值域）/ **G3 协议侧 + G8**（跨仓 ArchForge）/ **G5**（论文侧亦无 ⇒ 不可抄）· ⛔ 本仓另有**仓级止血**项（Stage B'''' · 并行 ctest 下变异型闸的还原竞态 ⇒ 冻结件残留）。

---

## 1. 一句话结论

> **论文的核心产出是「原子 SSP → 非原子多跳并发 FSM」**（per-block 瞬态态格 + 转发请求改名 + State Set 对偶性）；
> **本仓已把论文的「判据端」抄全并机器化，但「输出端」（瞬态态 + 在飞事务 + 协议 FSM 驱动）尚未开始**。
> ⇒ 本路径的**第一性问题不是「引入生成器」**（`docs/adr/ADR-CEP-18-protocol-generation-scope.md` 决策 1/6 已排除，且切换判据 T1/T2 结构���未达），
> **而是「让现状可机判」** —— 把藏在引擎注释里的散文变成**编译期 + 闸双重保证的声明面**，
> 使后续任何「补齐瞬态态 / 在飞事务」的改动**能被检出**，而不是继续以文档漂移的形式存在。

---

## 2. 证据基线（缺口的事实面）

### 2.1 论文侧可借鉴机制（对本仓最有用的 5 条）

| # | 论文机制 | 一手出处 | 本仓对应物 | 现状 |
|:--:|---|---|---|---|
| M1 | **转发请求改名**（`Fwd GetS` / `O Fwd GetS`）⇒ 缓存可由收到的消息**反推竞态胜负** | 博士论文 §6.6.1 | 无 | ⛔ 缺（前提是先有瞬态态） |
| M2 | **State Set 对偶性**（`IMAD ∈ I ∩ M`）⇒ 瞬态态可同时表现两种稳定态 | 博士论文 §6.6.2 | 无 | ⛔ 缺 |
| M3 | **唯一序列化点**（树上两条竞争路径恰一个公共目录） | 博士论文 §7.5 | `docs/adr/ADR-CEP-08-ordering-point-portability.md` 决策 2（`INV-CEP-08-IC1`） | ✅ **已抄 + 已机器化** |
| M4 | **E 态静默升级兼容性**（层间组合必查项） | 博士论文 §7.4.4 | 同上 决策 4（`INV-CEP-08-IC2`）+ `include/tlm/cep/ordering_point_checker.hh` | ✅ **已抄 + 已机器化** |
| M5 | **复合一致性模型**（`acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)`）+ **一致性标签**（read / write / fence） | 博士论文 §8.5.2 / §8.2.2 | `docs/adr/ADR-CEP-13-coherence-domain-ownership.md` 决策 2（`INV-CEP-13-IC1`）+ `include/tlm/cep/protocol_instances.json` 的 `consistency_labels` | ⚠️ **公式已抄；判定空转**（见 §2.3 G2） |

### 2.2 本仓实现面实测（缺口的事实面）

| # | 事实 | 一手出处 |
|:--:|---|---|
| F1 | **8 个协议实例合计瞬态态 = 1**：`moesi_gpu` 的 `T`；`chi_rnf` / `mesif` / `cxl_2_0` 声明 `kTransientStateCount == 0`（含编译期断言）；`tilelink_c` 3 稳定态且 `kTransientStateCount == 0`；`cxl_cache`（`cxl_2_0` 的 adapter）有该字段；⚠️ **`ualink_adapter.hh` 与 `nvlink_adapter.hh` 无 `kTransientStateCount` 字段**（= 字段不存在，非「值为 0」）；`cxl_3_0_symmetric` 无转移表 | `include/tlm/cep/moesi_gpu_adapter.hh`、`include/tlm/cep/chi_rnf_adapter.hh`、`include/tlm/cep/mesif_state_spec.hh`、`include/tlm/cep/cxl_state_spec.hh`、`include/tlm/cep/tilelink_state_spec.hh`、`include/tlm/cep/ualink_adapter.hh`、`include/tlm/cep/nvlink_adapter.hh` |
| F2 | **「在飞的 snoop 集合」被显式推迟**：MESIF 引擎自陈「本引擎不建模在飞的 snoop 集合与屏障域（Stage 2）」；`moesi_gpu` 决策 M-6「canonical 10 流的 `on_*` 只记账」 | `include/tlm/cep/mesif_protocol_engine.hh`、`include/tlm/cep/moesi_gpu_protocol_engine.hh` |
| F3 | **仅 2/8 实例有真协议 FSM 驱动**（`chi_rnf` 有 `in_flight_` / egress；`mesif` 有 egress 但不建模在飞集合）；`tilelink_c` = 事件计数上报器；`moesi_gpu` / `ualink_4` = 零发射；`cxl_2_0` / `cxl_3_0_symmetric` / `nvlink_4` = **无引擎** | 同上 + `include/tlm/cep/ualink_protocol_engine.hh`、`include/tlm/cep/tilelink_protocol_engine.hh` |
| F4 | **架构文档断言 vs 实现背离**：`docs/architecture/cep/protocol-engine.md` 写「真实协议的瞬态复杂度全部归 Protocol Engine 插件」 | 该文件（**冻结面**） |
| F5 | **复合一致性公理当前不可证伪**：`rfe` / `fr` / `ws` 两仓均无定义 + 语料为空 ⇒ `reachable == false` | `include/tlm/cep/compound_graph_falsifier.hh`（2026-11-24 改名，原 `litmus_falsifier.hh`）+ `test/cep_compound_graph_corpus.json`（原 `cep_litmus_corpus.json`） |
| F6 | **「不完整目录知识」无表达**：`Completeness::PARTIAL` 是**非法声明**（冻结 schema 只允许 `EXACT` / `CONSERVATIVE`）；实际用 `CONSERVATIVE` 超集上界表达；⛔ 无 belief / unknown 模型 | `include/tlm/cep/sharer_tracker.hh`、`include/tlm/cep/ordering_point_checker.hh` |
| F7 | **死锁自由 = 装配期静态论证**：Fabric 层无 CEP 自有运行时实现；多 VC 仲裁被反向存在性闸锁住（「仓内不存在多 VC 仲裁实现」为真值，一旦出现即红） | `docs/architecture/cep/fabric-transport.md`、`scripts/test/cep_invariant_ic2_absent_check.sh` |
| F8 | **CEP 的 hierarchical = 目录聚合层级**（`HierarchicalDirectory` 3 级 + `TreeManager` 树形），**不是**分层协议组合；且 3 层语义标注 pending 待追认 | `include/tlm/cep/sharer_tracker.hh`、`include/tlm/cep/tilelink_tree_manager.hh`、`openspec/specs/cep-hierarchical-directory/spec.md` |

### 2.3 缺口清单（按可执行性分层）

| # | 缺口 | 等级 | 可执行性 | 归属 |
|:--:|---|:--:|:--:|---|
| **G1** | 协议 FSM 承载能力（瞬态态 / 在飞事务 / 引擎存在性）**无机器可查的现状面** | 🔴 P0 | **本仓可做**（声明面 + 编译期反射） | planner → builder |
| **G2** | 复合一致性公理**不可证伪**（边类语义 + 操作级模型缺失） | 🔴 P0 | 需独立立项 + 用户 ratify | **既有 backlog** `cep-memory-model-operational-trace`（本轮不动） |
| **G3** | 分层协议组合 = 零（目录层已就位，协议层未建） | 🟡 P1 | 需 architect 追认层级语义 | architect |
| **G4** | fence 无一致性标签（论文一致性接口第三元缺失） | 🟡 P1 | 需评估外部素材可复用性 | planner → backlog |
| **G5** | 协议级死锁自由论证不存在 | 🟡 P1 | 论文侧亦未解决（§9.3.1）⇒ **抄不到** | backlog |
| **G6** | 「litmus」命名与能力错配（实为通道图找环） | 🟡 P1 | **本仓可做**（低成本） | planner → builder |
| **G7** | 「不完整目录知识」无表达（无 belief 模型） | 🟡 P1 | 需 architect 判取值域 | architect |
| **G8** | `SnoopState` 无 `F` ⇒ MESIF 转发点角色在 tracker 不可保真 | 🟡 P2 | 已显式登记（HD-05） | backlog |
| **G9** | sharer 位图 64-bit 类型但只映射 4 bit | 🟡 P2 | `docs/adr/ADR-CEP-21-sharer-bitmap-expansion.md` 已立**触发式**扩容判据 | 既有 ADR（阶段 2 不得预先执行） |
| **G10** | 无网络 hop / 时序模型 | ✅ **本仓可做部分已闭**（2026-11-24） | **只登记现状 + 让缺口可检出**（⛔ **不补** hop / 时序模型 ⇒ 属协议 FSM 语义设计，归 ArchForge） | 依赖 G1（已闭）· change `cep-hop-timing-capability-declaration` 已归档 |

---

## 3. 实施路径（分期与依赖）

```
Stage A（本轮 · planner 产出）
  ├─ A1 实施路径文档（本文件）
  ├─ A2 improvement I1：cep-protocol-engine-capability-declaration   ← 实施主体
  ├─ A3 improvement I2：cep-litmus-falsifier-naming-disambiguation      ← 登记就绪
  └─ A4 metis 审查 1 轮 + 注册 + rdd-planner handoff

Stage B（本轮 · builder 实施，I1 闭环）  ← 2026-11-23 metis P0-1 后与 I1 的 D-3 修订对齐
  ├─ B1 新建 include/tlm/cep/engine_capabilities.hh（通用层单头：枚举 + 实例级声明表
  │     kEngineCapabilities[8] + SFINAE HasInFlightApi + 判据本体；⛔ 零 JSON / 零 schema / 零 loader）
  ├─ B2 5 个引擎头各追加 EngineTypeTraits<T> 特化（协议差异留在协议层）
  │     → 覆盖 chi_rnf / mesif / moesi_gpu / ualink_4 / tilelink_c
  ├─ B3 编译期交叉核对：EngineTypeTraits<T>::in_flight ⇔ HasInFlightApi<T>（逐实例 static_assert）
  │     + 瞬态态策略（仅对有 kTransientStateCount 的 adapter）
  ├─ B4 闸：scripts/test/cep_engine_capabilities_check.sh
  │     （实例集双向零 orphan + 引擎头文件集反向存在性 + 反向 fixture）
  └─ B5 验收：verifier 报告落 .rddf/reviews/

Stage B'（2026-11-23 · builder 实施，I2 阶段 1 闭环 ✅ 已完成并归档）  ← 用户裁决「选择 A」
  └─ B''（2026-11-24 · builder 实施，I2 **阶段 2** 改名闭环）—— 「litmus 证伪工装」⇒「**复合图证伪工装**」
       （活代码面 + 活契约面；零决策变更 · 零行为变更；⛔ 命名消歧闸**文件名与红因码保留**
       （其管辖违例类不随工件改名过期 + 阶段 1 审计链）；台账 Entry #540）
  ├─ B'1 include/tlm/cep/litmus_falsifier.hh：追加消歧常量
  │     （kFalsifierScope / kFalsifierIsMemoryModelLitmus=false）+ 诚实边界段升级为可引用声明
  ├─ B'2 test/test_cep_litmus_falsification.cc：尾部追加 2 条断言（⛔ 既有断言段逐字不变）
  ├─ B'3 闸：scripts/test/cep_litmus_naming_check.sh（禁用表述闸 · 三路反 fixture）
  ├─ B'4 形态：skip_specs: true ⇒ 零 main spec delta ⇒ 冻结清单零变化 ⇒ 无需五步链
  │     （⛔ 阶段 2 的改名 + 冻结面 spec REMOVED requirement 另立）
  └─ B'5 验收：verifier 本地验收（无 iteration.json ⇒ 不走 rdd-verifier skill）

Stage B'''（2026-11-24 · builder 实施，G10 闭环 ✅ 已完成并归档）—— 「协议语义层的 hop / 时序」承载能力声明面
  （gap **G10** 的**本仓可做部分** = **让缺口可检出**；⛔ **不补** hop / 时序模型（归 ArchForge）；
   ⛔ 零 schema / ADR 正文 / invariant 变更 · ⛔ 零协议头改动 · ⛔ 与 G1 正交；台账 Entry #543）
  ├─ B'''1 include/tlm/cep/hop_timing_capabilities.hh：8 行声明表（逐行 basis）+ 自洽谓词 + SFINAE 探针 + 3 个诚实边界常量
  ├─ B'''2 test/test_cep_hop_timing_capabilities.cc：5 实例引擎 × 2 编译期否定断言（失败签名带**实例 id**）+ 探针正控/负控 + J-1/J-2/J-3/J-6
  ├─ B'''3 闸：scripts/test/cep_hop_timing_capabilities_check.sh（12 判据 + 3 条反向 fixture + `--root` / `--selftest`）
  ├─ B'''4 契约面：main spec + delta（由主 spec **提取生成**）⇒ **冻结面新增 1** ⇒ 五步链 1 次（92 → 93）
  └─ B'''5 验收：verifier 本地验收 **PASS 17/17**（报告 `.rddf/reviews/cep-hop-timing-capability-declaration-verify.md`）
       · 归档：`openspec/changes/archive/2026-11-24-cep-hop-timing-capability-declaration/`
       · ⛔ 归档目录按 `.gitignore` 政策**不入库**（「已完成归档」不属 force-add 例外）

Stage B''''（待实施 · builder）—— 仓级止血：**并行 ctest 下「变异型闸」的还原竞态** ⇒ 冻结件残留
  （2026-11-24 G10 收口期实测：`ctest -j4` 并行下 `cep_freeze_reverse_verify.sh` 与 `cep_decl_reverse_verify.sh`
   **同改** `fabric.json` / `protocol_instances.json` 且靠 `trap` 还原 ⇒ **还原序错乱** ⇒ 冻结件被留成变异态
   （本次实测：`fabric.json` 清空 + `protocol_instances.json` 残留 `-MUT`）⇒ 之后冻结闸 + 多个 CEP 闸**假红**；
   仓内**既有正确形态**见 `test/CMakeLists.txt:347-359`（「只写自己的临时目录 ⇒ 可安全并行进 ctest」）；
   台账 Entry #543 ⑨ 附带发现段；⛔ **修法待用户裁决 A / B / C**）

Stage C（后续 · 需 architect / 用户 ratify）  ← C1 + C2 已于 2026-11-23 完成 · C3 已于 2026-11-24 完成
  ├─ C1 architect：裁定 F4 的措辞背离（docs/architecture/cep/protocol-engine.md 是冻结面 ⇒ 五步链）
  │     ✅ **已完成 2026-11-23**（台账 Entry #539 · oracle 判定 APPROVE · 审稿报告
  │        `.rddf/reviews/cep-gen-family-gap-closure-stage-c-review.md`）—— 追加「归属」三义口径澄清块
  │        （零正文行改动 · 零数值复述 · 「两个不」人工 gate 通过）
  ├─ C2 architect：在 ADR-CEP-20 显式登记「档 2 天花板被非穷尽锁定」
  │     ✅ **已完成 2026-11-23**（台账 Entry #539 · 同批五步链）—— 文末追加「可达」两义口径限定 SU 段
  │        （零正文行改动 · 零决策变更 · 新增 invariant 数 = 0）
  ├─ C3 用户 ratify：cep-memory-model-operational-trace（G2 的唯一路径）
  │     ✅ **已完成 2026-11-24**（台账 Entry #541 / #542 · `ADR-CEP-22` 已签发（冻结面 92）· change 已归档
  │        · 验收 **PASS 17/17** · 报告 `.rddf/reviews/cep-memory-model-operational-trace-verify.md`）
  ├─ C4 architect：追认 HierarchicalDirectory 的 3 层语义 + 5 条基线约束闸（G3）  ← ⏸ 待办
  └─ C5 planner：评估 fence → 标签映射素材（G4）  ← ⏸ 待办
```

**依赖方向单向**：`G1（Stage B · ✅ 已闭）` ⇒ `G10（Stage B''' · ✅ 本仓可做部分已闭 · 补模型跨仓）` ⇒ `G3 的协议侧（⛔ 跨仓 ArchForge）` ⇒ `G5（⛔ 不可抄：论文侧 §9.3.1 亦未解决）`。**G2 独立**（走既有 backlog 线 · ✅ 已闭 + 残余 5 项已单独登记）。

---

## 4. 本轮范围与不做项

### 4.1 做（3 个 improvement · I1 / I2 / **G10**）

| ID | improvement | 目标 | 冻结面影响 |
|:--:|---|---|---|
| **I1** | `cep-protocol-engine-capability-declaration` | 让「每个协议实例的协议语义承载能力」成为**机器可查的声明面**：引擎存在性 / 在飞事务建模 / 瞬态态现状 ⇒ **G1 从文档漂移变为可检出缺口** | **零**（新建声明源 + 新建契约头 + 新建闸；⛔ 不动 `include/tlm/cep/protocol_instance.schema.json` 的 13 键 · 不动 `include/tlm/cep/protocol_instances.json` 的实例面） |
| **I2** | `cep-litmus-falsifier-naming-disambiguation` | 消除「litmus」同名误导（实为通道依赖图找环，非内存模型 litmus）⇒ **G6** | **有**：`openspec/specs/cep-litmus-falsification/spec.md` 命中冻结 glob ⇒ 五步链 |

| **G10** | `cep-hop-timing-capability-declaration` | 让「协议语义层**有没有** hop 计数 / 消息级时序模型」从**引擎头文件里的散文**变成**机器可检的现状面**（声明面 + 引擎层反向存在性 + 编译期否定断言）⇒ **G10 从不可检出变为可检出**（⛔ 只登记现状 · ⛔ 不补模型） | **有**：新建 `openspec/specs/cep-hop-timing-capability-declaration/spec.md` ⇒ 冻结面**新增 1**（92 → 93 · 五步链 1 次） |

### 4.2 不做（及理由）

| 不做项 | 理由 | 归属 |
|---|---|---|
| 引入 DSL / 解析器 / 代码生成器 | `docs/adr/ADR-CEP-18-protocol-generation-scope.md` 决策 1/6 已排除；切换判据 T1（规模）T2（表达力）实测**结构���未达** | architect 已决 |
| 承诺自动协议组合 / 自动兼容性判定 / 无违例保证 | `docs/adr/ADR-CEP-13-coherence-domain-ownership.md` 决策 5/6 已排除 | architect 已决 |
| 实现瞬态态格 / 在飞 snoop 集合 | ⛔ **本轮只做「让缺口可检出」**，不做「补齐缺口」—— 后者是协议 FSM 语义设计，属 architect + ArchForge | Stage C |
| **补协议语义层的 hop 计数 / 消息级时序模型**（G10 的本体） | ⛔ G10 的**本仓可做部分只做「让缺口可检出」**（声明面 + 反向存在性 + 编译期否定断言）⇒ **补模型**属协议 FSM 语义设计，归 architect + ArchForge | ArchForge（跨仓） |
| 定义 `rfe` / `fr` / `ws` 语义 + 操作级模型 | 既有 backlog `cep-memory-model-operational-trace` 已在办（⛔ 开工前置 = architect 裁「每域模型来源 + 域间边构造规则」+ 用户 ratify）⇒ 本轮**不重复立项** | Stage C |
| 扩 `include/tlm/cep/protocol_instance.schema.json` 的 13 键 | 该 schema 是冻结面 ⇒ 五步链；且 `perm_upgrades.json` 已有**同型先例**（「本仓自持 ⇒ 不参与镜像比对」）⇒ 走**新建声明源**成本更低 | I1 的 D3 偏离 |
| 归档/改写 `docs/architecture/cep/protocol-engine.md` 的措辞 | 冻结面 + architect 判权 ⇒ planner 不代裁 | Stage C |

---

## 5. 与既有决策的对齐（复评）

### 5.1 `docs/adr/ADR-CEP-18-protocol-generation-scope.md` 切换判据 T1–T4 复评

| 判据 | 2026-11-23 实测 | 结论 |
|:--:|---|:--:|
| **T1 规模**（同族实例 > 8 且跨 ≥3 族） | 8 实例 / 5 工业族 + 1 学术族；同族最大 1 | ❌ 未达（差一个数量级） |
| **T2 表达力**（全部协议私有状态可被 SSP 表达） | HTM 明确排除 ⇒ **结构性不可达**；瞬态辅助状态的可表达性**从未测** | ❌ 结构性未达 |
| **T3 前置层**（合同测试可覆盖「生成↔手写」一致性） | 合同测试已签发；无「生成插件」可比对象 | 🟡 部分 |
| **T4 产出定位**（消费者 = CEP 插件） | 决策 4 已锁插件 | ✅ 满足 |

⇒ **不满足切换判据，与决策 1 一致 —— 本轮无需重新评审路线。**
⇒ **本轮 I1 反而是决策 4 的「前置层」的具体化**：T2 的「瞬态辅助状态可表达性从未测」这句话，在 I1 落地后**第一次有了可测的现状面**。

### 5.2 与 `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` 的关系

`ADR-CEP-12` 决策 1 的 schema（以实例为 key）管的是**「什么协议存在」**；
I1 管的是**「该实例的协议语义由谁承载、承载到哪一层」** ⇒ **两者正交，不是重复**：
`protocol_instances.json` 回答前者，I1 新头 `include/tlm/cep/engine_capabilities.hh` 的 `kEngineCapabilities[]` 回答后者（**C++ constexpr 表，非 JSON**）。⛔ I1 **不改**前者（⛔ 不扩 13 键 ⇒ 不触发镜像比对问题）。

### 5.3 与 `docs/adr/ADR-CEP-13-coherence-domain-ownership.md` / `docs/adr/ADR-CEP-08-ordering-point-portability.md` 的关系

I1 **不新增**任何一致性判据，**不改变**任何既有 invariant 语义 ⇒ ⛔ 不触碰 `INV-CEP-13-IC1` / `INV-CEP-08-IC1` / `INV-CEP-08-IC2`。
I1 的产出是**现状面**（把「哪一层被承载」变成事实），为 G2 / G3 的后续判定提供输入。

---

## 6. 门禁与验收口径（I1）

| 门禁 | 类型 | 判据形态 |
|---|---|---|
| `scripts/test/cep_engine_capabilities_check.sh` | 新建 | ① 抽取 `kEngineCapabilities[]` 初始化区的实例集（grep **数据行**，⛔ 不 grep API/类名）② **双向零 orphan**（`protocol_instances.json` 的 `id` ↔ `kEngineCapabilities`）③ **引擎头文件集反向存在性**（`include/tlm/cep/*_protocol_engine.hh` 恰为 5 个已登记引擎头；出现第 6 个 ⇒ 必红）④ 反向 fixture |
| `test/test_cep_engine_capabilities.cc` | 新建 | ① **编译期反射一致**（`EngineTypeTraits<T>::in_flight` ⇔ `HasInFlightApi<T>` ⇒ 逐实例 `static_assert`）② 瞬态态**策略**判据（**仅对有 `kTransientStateCount` 的 adapter**；`ualink_adapter` / `nvlink_adapter` 无该字段 ⇒ 不在范围）③ 双向零 orphan（运行期复述） |
| 既有 `scripts/test/cep_*.sh` 全量 | 回归 | rc=0 |
| `ctest` 全量 | 回归 | rc=0（⛔ 不写死断言数） |

**判据纪律**（沿用仓内既有口径）：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。

---

## 7. 风险与诚实边界

| # | 风险 | 缓解 |
|:--:|---|---|
| R1 | **声明面与代码漂移**（声明 `in_flight_modeled: true` 但引擎其实没有） | 由**编译期反射**兜底 ⇒ 漂移直接编译失败，不是运行期才发现 |
| R2 | **反射标志选错**（探测的 API 名与未来实现不匹配） | 探测点必须是**公开可观察**的 API；探测名的选取写进判据并登记 ⇒ 改名即触发判据复核 |
| R3 | **声明面与 `protocol_instances.json` 的 `status` 语义重叠** | ⛔ 严守分工：`status` = 「实例是否存在且被装配」；`kEngineCapabilities` = 「协议语义承载能力」⇒ 两面**正交**；且 `kEngineCapabilities` **不含**瞬态态计数 / 在飞建模字段（唯一真源分别是 adapter 头与 `EngineTypeTraits` 特化） |
| R4 | **声明源被当作能力承诺**（读者把「egress = none」读成「协议不发消息 = 正确」） | ⛔ 措辞纪律：声明的是**承载能力现状**，不是**协议行为正确性**；⛔ 不得表述为「已验证」 |
| R5 | I2 触及冻结面 spec ⇒ 五步链成本 | I2 **本轮只登记**，实施时单独排期 |

**诚实边界**：本路径的结论基于**静态代码与文档核对**，**未运行**测试与闸；
论文侧引用为 2026-11-23 对 178 页 PDF 抽取件的本人核对（与 `docs/research/cep-coherence-protocol-space-survey.md` 的既有引文口径不同，两套不可混引）；
**ArchForge 侧未纳入核对** ⇒ 全部「协议语义」结论只对 CppTLM 仓实现面成立。

---

## 8. 关联登记

| 项 | 路径 |
|---|---|
| 本轮 I1 improvement | `.rddf/improvements/cep-protocol-engine-capability-declaration.md` |
| 本轮 I2 improvement | `.rddf/improvements/cep-litmus-falsifier-naming-disambiguation.md` |
| Stage C 审稿报告（C1 + C2 · APPROVE） | `.rddf/reviews/cep-gen-family-gap-closure-stage-c-review.md` |
| Stage C 授权与判据（台账） | `.rddf/roadmap/objectives/obj-cep-platform.md` **Entry #539** |
| 论文侧调研真源 | `docs/research/cep-coherence-protocol-space-survey.md` |
| 决策图（导航面） | `docs/adr/CEP-DECISION-DIGEST.md` |
| 生成路线范围 ADR | `docs/adr/ADR-CEP-18-protocol-generation-scope.md` |
| 一致性域 ADR | `docs/adr/ADR-CEP-13-coherence-domain-ownership.md` |
| 定序点 ADR | `docs/adr/ADR-CEP-08-ordering-point-portability.md` |
| 能力地图 ADR | `docs/adr/ADR-CEP-12-protocol-engine-capability-map.md` |
| G2 既有 backlog | `.rddf/improvements/cep-memory-model-operational-trace.md` |
| G10 change（已归档） | `openspec/changes/archive/2026-11-24-cep-hop-timing-capability-declaration/`（⛔ 归档目录按 `.gitignore` 不入库） |
| G10 main spec（冻结面） | `openspec/specs/cep-hop-timing-capability-declaration/spec.md` |
| G10 验收报告 | `.rddf/reviews/cep-hop-timing-capability-declaration-verify.md`（PASS 17/17） |
| G10 台账（立项 + 收口 + 附带发现） | `.rddf/roadmap/objectives/obj-cep-platform.md` **Entry #543** |
| G10 improvement 草稿 | `.rddf/improvements/cep-hop-timing-capability-declaration.md` |
| 仓级止血项（Stage B'''' · 并行变异闸竞态） | 台账 `.rddf/roadmap/objectives/obj-cep-platform.md` **Entry #543 ⑨** |
