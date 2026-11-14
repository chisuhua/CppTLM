# ADR-CEP-21 的 critic 复核（第 1 轮 · 2026-11-14）

> **审查者**:critic（arch 档 · architect 流程的第二审,oracle 之后）· **结论**:**APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**
> **处置**:4 条 SHOULD_FIX + 2 条 NIT ⇒ **全部采纳并已落入 ADR**（逐条见下）
> ⚠️ 本文件是**审稿报告**（过程日志）⇒ ⛔ 不进 ADR 正文（per architect 产出形态契约）

## 1 · BLOCKER：无

critic 逐条核对契约面（`ADR-CEP-04` 全文关键段 / `ADR-CEP-12` 的 `INV-CEP-12-IC1` / `ADR-CEP-09` / `sharer_tracker.hh:75-127` / `mesif_protocol_engine.hh:333-354` / `protocol_instance.schema.json:222-251` / `test` 两处硬编码与 initializer list / `cep-sharer-tracker-base` main spec 的拒绝场景）⇒ **未发现架构级 BLOCKER**。
并明确：① 与 `ADR-CEP-04` **无冲突**（caps 4 属性 / 5 虚方法 / 64-bit `static_assert` 均不在改动面；**4-bit 映射宽度本身不在 ADR-CEP-04 文本中**，grep 零命中 ⇒ 阶段 2 扩宽度不构成对已签发 ADR 的修订）;② `ADR-CEP-12` 的 IC1 依赖表述**准确**;③ `ADR-CEP-09` 正交声明**成立**。

## 2 · SHOULD_FIX（4 条 · 全部已采纳）

| # | critic 发现 | 独立复核 | 处置 |
|:--:|---|---|---|
| **S1** | 依赖清单标题断言「扩容**必须**同步的 **6 处**」隐含完备性承诺,但**漏了第 7 处** —— `openspec/specs/cep-sharer-tracker-base/spec.md:140` 的 Requirement 正文写「含**未映射位**(**bits 4..63**)」⇒ 阶段 2 后 bits 4~7 变已映射 ⇒**该 Requirement 语言直接失效** | ✅ **成立**（实测 `:140` 原文即「bits 4..63」;且这是清单中**唯一契约面**） | **清单加第 7 行** + 单列说明「第 7 处为何单列（唯一契约面,须随阶段 2 走 spec delta）」;并登记「`cep-tilelink-tree-manager/spec.md:103` 用的是**语义性**措辞『含未映射位』⇒ 阶段 2 后仍成立 ⇒ ⛔ 无需 delta（仅登记）」 |
| **S2** | 选项 B 的否决理由②（永久 STALL）与理由①**自相矛盾** —— B① 已建立 `responder_mask_of(kInvalidNode) = 1<<4 ≠ 0`,而 B② 却写「`responder_node_of(4) = kInvalidNode` ⇒ **mask = 0**」 | ✅ **成立**（同一假设下矛盾） | **B② 改写**：把中间步骤改为「**真正的第 5 个真实 node id 在表内无匹配** ⇒ `responder_mask_of(该 node)` **恒 0** ⇒ 扣减永不到位 ⇒ 永久 STALL」;保留 B① + B② 双理由 |
| **S3** | 触发判据 2a 残留**自我修正草稿**（带问号的不可能残句 + 「更准确地」改写）⇒ 作为「唯一触发条件」的文本有**二读风险**;且未限定触发源（`mark_observation_invalid()` 有 4 个源） | ✅ **成立** | **删残句**，保留「更准确地」后的定义为 2a 唯一文本;并**限定触发源**为「由 `responder_mask_of(node)==0` 这一分支触发」（4 源中只有它指向「映射宽度不足」） |
| **S4** | **状态行 + 审稿记录段残留过程日志**（复核条数 / 轮次 / BLOCKER 清单） | ✅ **成立**（per architect 产出形态契约「过程日志禁止进文档」） | 状态行改为「已过 oracle + critic 双审，待转 Proposed」（不含条数）;「审稿记录」段改为**一行指针**（结论与逐条处置见 `.rddf/reviews/cep-adr-021-*`） |

## 3 · NIT（2 条 · 全部已采纳）

| # | critic 建议 | 处置 |
|:--:|---|---|
| **N1** | V1-1③ / V2-1 的「语义是否为默认」「依据是否充分」需人裁 ⇒ 与 V1-6 的诚实登记口径不一致 | **两条各加「由人裁」登记** |
| **N2** | 2a 的「该 node 确实被提交了 sharer 操作」依赖观测面反查 node id ⇒ 补前瞻判据 | **新增 V1-7**：「观测面可定位触发节点」⇒ 并诚实标注「当前 mesif 观测面**未见**该反查能力 ⇒ 阶段 1 需补;若不补则 2a 降级为『人裁 + 装配日志』」 |

## 4 · critic 的「针对 focus 的逐项结论」（摘要）

| focus | critic 结论 |
|---|---|
| 与已签批 ADR 冲突 | **未发现**（三项逐一核对;详见 §1） |
| 是否有未决写成已决 | **否** —— 2b（人裁）与 W 值（默认机制参数、实测可上调）属**恰当的条件化授权** |
| 决策质量 | 两阶段成立、**非空 ADR**（S1-1 消除的假绿面**今天就在**：`:47` 注释「上限 4」是语义错误、`broadcast_fanout=4` 实为默认值）;选项覆盖无关键遗漏;否决理由中 B①/C′/D 技术上正确,唯 B② 表述失真（已修） |
| 过程日志残留 | 除已移出的 oracle 报告外,**状态行 + 审稿记录段**是残留 ⇒ 已按 S4 修 |
| Verification 前瞻 | 17 条**全部为前瞻可勾选判据**,无实测日志混入;V1-6 的诚实登记做法**正确** |
| 元数据四要素 | **齐备**;「change 未立项 ⇒ AC 暂缺」的豁免被**正确援引** |
| 诚实边界 | 「仓内无 >4 sharer 实测需求」是**诚实登记 + 结构性正当性**（并非回避）⇒ 本 ADR **成立,无需降级** |

## 5 · 关联

ADR `docs/adr/ADR-CEP-21-sharer-bitmap-expansion.md` · oracle 报告 `.rddf/reviews/cep-adr-021-oracle-review.md` · 判据源 `ADR-CEP-04` / `ADR-CEP-12` / `ADR-CEP-09` · `openspec/specs/cep-sharer-tracker-base/spec.md:140` · `include/tlm/cep/sharer_tracker.hh`
