# 审稿报告：ADR-CEP-22（每域模型来源与域间边构造规则）

> **日期**：2026-11-24 · **角色**：architect · **对象**：新建 ADR `ADR-CEP-22-memory-model-source-and-cross-domain-edges.md`（**仓外草案** `/tmp/opencode/adr/`；仓内落盘路径 = `docs/adr/ADR-CEP-22-…`，由 builder 在台账 Entry 授权下走五步链）
> **状态**：🟡 **Proposed**（v0.3）—— oracle 复审 **APPROVE** · critic **APPROVE_WITH_CONDITIONS**（`mergeable: yes` · **0 BLOCKER**）⇒ ⛔ **用户 ratify 未完成** ⇒ **未签发**、未走冻结面授权链、未据本 ADR 立项 G2 实施

---

## 一、意图检查点处置（per build.md §oracle 调用前的意图检查点）

| 闸门项 | 处置 | 证据 |
|---|---|---|
| 意图是否明确 | ✅ **明确** —— 裁定对象 = 「每域模型来源 + 域间边构造规则」，出自 `ADR-CEP-13` 决策 2 的公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` + 裁决记录 §三（backlog 登记裁定） | 用户口令「同意你的建议，请继续推进」· 台账 Entry #540 后的 G2 开工前置 |
| 是否存在 ≥2 种合理解读 | ✅ **存在**（域身份三选 / 模型来源三选 / 边构造 / 写序非唯一 / 全局序载体 …）⇒ **不**直接进 oracle，先走 metis | metis 1 轮（6 节输出：隐藏意图 / 歧义 / AI slop 风险 / 缺失验收标准 / 边界情况 / 建议修改）⇒ 其「建议修改」10 条（P0×6 · P1×2 · P2×2）**全部**被采纳或显式处置 |
| 是否需问用户 | ⛔ **否** —— 意图（要解决什么 + 范围边界）由上游已签批 ADR + 裁决记录 + backlog 草稿唯一确定；**具体取舍**由本 ADR 的选项表承载，最终由**用户 ratify** 裁决 ⇒ 未提前占用 ratify | — |
| **metis 先行的产出处置** | metis 的「缺失的验收标准」= **feature 级 AC** ⇒ 落 **backlog 草稿 / 下游 change 的 tasks**，⛔ **未**灌进 ADR 正文；metis 的「边界情况」= 材料 ⇒ 进 ADR §2/§4.2/§Verification | ADR §5.1（两处 backlog AC 缺陷登记）+ §4.2（缺口①②）+ §Verification（V-22-1~11） |

⇒ **判定**：意图已定 ⇒ oracle 审查成立（非「意图未定 ⇒ 只给建议不给判定」）。

---

## 二、审查轨迹与判定行原文

| 轮次 | 审查者 | 判定行原文 | 处置 |
|:--:|---|---|---|
| 1 | **metis**（gap 分析 · architect 路径 1 轮） | 六节输出（**材料**，非判据） | 10 条建议全部处置（详见 §三 表） |
| 1 | **oracle**（审查域 = 其他审查） | `判定: REJECT` | 8 项修订 ⇒ 出 v0.2 |
| 2 | **oracle**（**复审** · 只验闭合） | `判定: APPROVE`（8/8 高影响 + 4/4 中低全闭 · 无新 BLOCKER；唯一非阻断注记「R-2e 节号 §三 ⇒ §五」已修） | ✅ 通过 |
| 1 | **critic**（`review_profile: arch`） | `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER** · 3 SHOULD_FIX + 4 NITS | 3 SHOULD_FIX + 4 NITS 全部修 ⇒ 出 v0.3 |

> ⚠️ **复审纪律**：oracle 复审**只**提交「上一轮 finding 的闭合证据 + 本轮新增 diff」；⛔ 未要求全量重审；⛔ 未在 prompt 里写「攻击新判据 / 找逃逸 / 变异测试」类开放式指令。

---

## 三、oracle 首审 REJECT 的 8 项 + 4 项中低影响 —— 逐项闭合

| # | finding | 闭合证据（v0.2/v0.3 位置） |
|:--:|---|---|
| 1 | **事实错误**：`ualink_4` = PLO/PLO（现役实例），我原称「TSO/PLO 本仓无实例」 | 元数据「一手锚点」第 2 条改为「8 个实例 … `ualink_4` = **PLO/PLO** ⇒ **`TSO` 0 实例；`PLO` 1 现役实例**」（实测 `protocol_instances.json:518-521`，`status: implemented`） |
| 2 | **PLO 须在 v1 登记**（现役实例 + 仓内锚点） | §2 决策 3 表 PLO 行：读侧 ∅ / 写侧 {into writes} = R→W ∪ W→W，溯源 ✅ 仓内可复核（`protocol-engine.md:104` 逐字） |
| 3 | **`RC-data` 应为 ∅**（relaxed 不保留 po；`po_loc` 属过严 ⇒ 假阳性） | §2 决策 3 表 RC-data 行读/写双侧 ∅ + ⛔ 理由；自洽性对照 `moesi_gpu` ⇒ R→W · W→W |
| 4 | **`fr` 规则 vacuous**（v0.1 的 ≺-派生形态被 `ws` 吸收 ⇒ 冗余）+ 方向非 cat 标准 | §2 决策 4：`fr(r,w2) ⟺ ∃w1: rf(w1,r) ∧ ws(w1,w2)`（静态 · 读→写）+ ⚠️ 约定依赖段（写→写形态亦经典 ⇒ 实施 change 须锚点复核并登记） |
| 5 | **`ws` 应取逐地址 `co`**（全局写序含跨地址 ⇒ 永不改变判定 ⇒ 误导） | §2 决策 4 `ws` 行 + 明写「⛔ 不取全局写序」及理由 |
| 6 | **枚举面应收窄** | §2 决策 5：候选 = **逐地址写序候选**（笛卡尔积）+ 静态图判环 + ⑤ 实现口径（⛔ 无需构造全局 mo） |
| 7 | **须补登两条缺口** | §4.2 **缺口①**（域内一致性/值面/SWMR **不由本公理检查** + 反例 + 「双图折衷不掩盖」）与 **缺口②**（「迹域标识忠实代表 OP 作用域」由**迹作者**承担）· 另落 §2 决策 1 / 决策 6 表 / §5.2 抓取点 6 / **V-22-11** |
| 8 | **V-22-7 金标准迹不可达** | 换**跨域 `rfe` 拉环**真环迹（域 A `r1(X)`→`w2(Y)` · 域 B `r2(Y)`→`w1(X)`）⇒ 环 `w1→r1→w2→r2→w1` **在 `ppo_com ∪ rfe` 内 ⇒ 与 `co` 候选无关** ⇒ ∀ 候选成环 ⇒ `falsified`；毒化改「**关掉 `rfe`**」（并注明 ⛔ 不可用「关掉 `fr`」） |
| 9 | TSO 仍不登记 | §2 决策 3 TSO 行（0 实例 + 仓内无**序**规则 ⇒ `undecided` + `model-semantics-unavailable`）· 候选规则记入 §6 后续③ |
| 10 | 判定态名拼写 | 全文无 `not_falsized` 残留；changelog 亦改为「修判定态名拼写（漏 `i`）」以免字面残留 |
| 11 | R-2e 指针偏松 | 改引 I-6 裁决记录 `.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md` **§五**（`R-2e` 本体所在节；oracle 复审注记已修） |

---

## 四、critic 的 3 SHOULD_FIX + 4 NITS —— 逐项处置（v0.3）

| # | critic 意见 | 处置 |
|:--:|---|---|
| **S1** | 决策 4「跨域面归属」段与决策 4 表**自相矛盾** —— 「`fr` 跨地址部分」在 `co` 逐地址下不存在；critic 判为 v0.1 残留（规范表唯一，解释句有误 ⇒ 实施方照此会产出跨地址 `fr` 边） | ✅ **已修**（v0.3）：该段改为「`fr` 中 `r` 与 `w2` **恒同址** ⇒ **不产生跨地址边**；二者**可不同域** ⇒ `fr` **可跨域**；`ws` = 逐地址 `co` ⇒ 同址写之间（含**跨域**写）有 `ws` 边」+ 补口径「上游的『全局内存序』是**无环性判据的语义**，⛔ **不是**第四条边类 ⇒ 取逐地址 `co` 与上游**无违例**」 |
| **S2** | 元数据「复审未完成」与事实不符（oracle 复审 = APPROVE） | ✅ **已修**（v0.3）：状态行改为「v0.3 · **oracle 复审 APPROVE** + **critic APPROVE_WITH_CONDITIONS**（`mergeable: yes` · 0 BLOCKER）；⛔ 用户 ratify 未完成 ⇒ 未签发」 |
| **S3** | 无**显式回滚路径** | ✅ **已修**（v0.3）：决策 9 补第 4 项 —— 回滚 = **不 ratify**（零成本）或 **追加 SU 修订**（`ADR-CEP-00 §2 Decision 3` 形态）⇒ 无数据迁移 / 无 schema 变更 / 无实现回退负担（决策 1~8 产物全部**未落地**） |
| **N1** | 选项表字母跳号（A~G 后直接到 I） | ✅ **已修**：重编号为 A~K（11 行） |
| **N2** | 决策 5 ⑤ 的「⟺」在病态迹上不成立（`ppo(r→w) ∧ rf(w→r)` ⇒ 右图成环而 `G_ax` 无环；该形态**不在** AC-10 畸形闭集内） | ✅ **已修**：⑤ 由「⟺ 等价」弱化为**实现口径** + 加 **well-formedness 前置**（迹须满足「读不会读到自己程序序更晚的写」）+ 明写「违反者其 `not_falsified` 判定**不可**由本口径推出 ⇒ 实施 change 须纳入畸形集或显式登记为限制」 |
| **N3** | §8 修订记录含过程日志（arch 档口径属治理台账） | ⛔ **维持现状** —— 先例 `ADR-CEP-20 §8` 同形态已签发；且 ADR 签发后正文不可改 ⇒ 修订史是唯一可追溯处 |
| **N4** | 正文 260 行 > 250 目标 | ⛔ **维持并登记**（理由见 §五） |

---

## 五、一处**已知偏差**登记（正文 263 行 > 250 目标）

- **事实**：正文（§1 起）**263 行**，超出「≤ 250 行」目标 13 行（+5.2%）。
- **不压缩的理由**：本 ADR 的**决策内容本身即规则本体** —— `rfe`/`fr`/`ws` 的构造规则与逐标签授权表**必须**在 ADR 内（backlog `cep-memory-model-operational-trace` What Changes #3 明令「⛔ 不在 ADR 正文外另立第二真源 ⇒ 若需 ADR ⇒ **新建** ADR」）；若把规则外移到 `design.md` / 主题文档 ⇒ 产生**第二真源** ⇒ 与仓内「真源分级」纪律冲突。
- **已做的压缩**：v0.1→v0.3 共压 **~30 行散文**（§1.1/§1.2/§1.3 合并 · §3 选项 G/H 合并 · §4.1/§4.2 合并 · §5.2 抓取点行合并 · 决策 4 输入面压缩 · 决策 9 压缩）⇒ **规则内容零删减**。
- **处置**：登记为**已知偏差**（critic N4 已判「偏差可接受（登记已做）」）；若用户要求，可由 **architect 侧**再压散文 ~13 行（§3 选项表 11 行 → 8 行 + §Verification 折行），⛔ 不动规则。

---

## 六、ADR 完成的标志（5 条）核对

| # | 标志 | 状态 | 说明 |
|:--:|---|:--:|---|
| 1 | 四要素齐（元数据 / 选项表 ≥3 行 / 独立后果章 / Verification 前瞻判据） | ✅ | 元数据含 **AC 映射**（指向 backlog 的 `AC-N`，change 尚未立项 ⇒ 以 `backlog: cep-memory-model-operational-trace` 占位）· 选项表 **11 行**（A~K）· §4 独立「后果 / 代价」章（正面 / 负面 / 风险）· Verification **11 条**（前瞻判据 + 反向 fixture + 归因 id + 载体状态三档） |
| 2 | oracle + critic 双审完成；BLOCKER 全闭 | ✅ | oracle 首审 REJECT → v0.2 → **复审 APPROVE**；critic **0 BLOCKER**（`mergeable: yes`） |
| 3 | Status 已转为已决状态 | ⛔ **未转** | 仍 `🟡 Proposed` ⇒ **待用户 ratify** ⇒ 不得签发 |
| 4 | 关联 change 已落库或已登记 | ✅ | `backlog: cep-memory-model-operational-trace`（**尚未立项** ⇒ 立项后回填 change 指针 + AC 映射） |
| 5 | 审稿报告已落 `.rddf/reviews/<adr>.md` 且 ADR 元数据写有指针 | ✅ | 本文件 + ADR 元数据「审稿报告」指针（见 §七） |

⇒ **仅缺 #3（用户 ratify）** ⇒ 落盘与冻结链在 ratify 后由 builder 执行。

---

## 七、下一棒（ratify 之后，⛔ 未获用户 ratify 前一律不动）

1. **architect**：追加 **签发 SU**（记录双审结论 + 用户 ratify + 台账 Entry 指针）⇒ ADR 转 `✅ 已签发`；
2. **planner**：台账 Entry（**授权 + 新增声明行**：`docs/adr/ADR-CEP-22-…` 命中 `FROZEN_GLOBS`）+ backlog 草稿的 **AC-7 / AC-8 修订**（§5.1 D-1/D-2，planner 写集）+ openspec change 立项；
3. **builder**：ADR 仓内落盘 + `docs/adr/README.md` 索引（`gen_adr_index.py --write`，份数 24 → 25）+ `docs/adr/ADR-CEP-NUMBER-MAP.md` §2 决策 1 表新增 `CEP-22` 行 + `test/cep_adr_structural_deferrals.json` 的 `decision_adrs`（21 → 22）⇒ **产物 commit** ⇒ `cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ **清单单独 commit** ⇒ **无参复验** ⇒ 随后实施 G2（两处冻结面五步链 ×2）。

---

**关联**：ADR 草案 `/tmp/opencode/adr/ADR-CEP-22-memory-model-source-and-cross-domain-edges.md`（v0.3 · 281 行）· 上游 = 台账 Entry #540 后的 G2 开工前置 · `ADR-CEP-13` 决策 1/2/5② · `ADR-CEP-12` 决策 3/4 · `ADR-CEP-08` 决策 1 · `ADR-CEP-20` 决策 6 · I-6 裁决记录 §三 / §五 · backlog `.rddf/improvements/cep-memory-model-operational-trace.md`
