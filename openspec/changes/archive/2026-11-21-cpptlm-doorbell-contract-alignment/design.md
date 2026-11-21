# design: cpptlm-doorbell-contract-alignment

> **裁决来源**：`.rddf/reviews/dgpu-doorbell-adjudication-q1-q12-2026-11-20.md`（architect · oracle REJECT 3 类 BLOCKER 全闭 + critic APPROVE_WITH_CONDITIONS · 无 BLOCKER）
> **本文件只记**「裁决内容 → 落地形态」的映射与**实测修正**；⛔ 不重述裁决的论证（避免第二真源）。

## D-1 · 三处主断言的改写形态（裁决 C-1 ~ C-3）

| 断言 | 旧形态（假绿源） | 新形态 | 为何这样改 |
|:--:|---|---|---|
| **A-1**（C-1） | Requirement 正文：`bar_sizes[1]` **必须 ≥ 256MB+64KB** | 正文改为「该偏移是**测试专用合成偏移** —— doorbell 路由**不依赖** `pcie_ep.params.bar_sizes[1]` 的大小(BAR1 实窗为 16MB 时同样命中);⛔ 本仓**不**设置"实窗大小"类约束(per `ADR-DGPU-09 §2.2.2`)」 | MUST 的**主语从「配置大小」换成「偏移识别方式」** ⇒ 保留一条**可判定且为真**的 MUST，替换一条**被全体违反**的 MUST（裁决 §4「放弃一条约束换一条真约束」）。⚠️ 保留对 `ADR-DGPU-09 §2.2.2` 的引用 ⇒ 新文本与既有已签批决策**同向**（落实，非反转）|
| **A-2**（C-2） | Scenario「doorbell 配置过小(检测 config bug)」：THEN `doorbell 访问越界,**不**触发 doorbell 路由` | Scenario **改写**为「doorbell 在小实窗下仍命中(合成偏移 · 非 config bug)」：THEN `doorbell 写**仍命中** doorbell 路由(合成偏移走 BAR1 内的**优先匹配**,先于实窗 bound 检查)` + AND `该结果**不**视为 config bug,⛔ 不破坏 SDMA ring consume 路径` | 裁决明令「⛔ 不删除 —— 保留 config-bug 检测意图」⇒ **改写**而非删。**关键**：把「越界 ⇒ 不路由」这一**与实现相反**的断言，反转为实现真正保证的性质（优先匹配 ⇒ 命中），并**保留**「不该被当成配置缺陷」的判断意图 |
| **A-3**（C-3） | Requirement「doorbell 配置 vs 代码 hardcoded 不一致」+ Scenario「风险追踪」（GIVEN 指向 §13 风险表行；THEN 记录「v1.0 仅 ABI 路径,v1.1 补」） | 整条改写为「doorbell 合成偏移是**已裁定的现状**(per ADR-DGPU-09 §2.2.2 + d3-evolution-roadmap)」+ 3 个 Scenario（现状被承认 / 复活条件·语义触发 / 复活条件·可机判触发带） | 旧 GIVEN 指向的 §13 风险表**已指针化到别仓**（`docs/designs/dgpu-soc/architecture.md` → §0.2）⇒ GIVEN **不可满足** ⇒「已记录」**永不可达** = 僵尸条款。新文本把「推迟项」改成「**已裁定的现状**」，并把**复活条件**写进契约 ⇒ 条款**自我封闭**（不再依赖外部台账的可达性）|

## D-2 · 复活锁选 **R-2**（裁决 §3.3 的三选一 · 择一写死）

| 候选 | 表述 | 本 change 是否选 |
|:--:|---|:--:|
| **R-1** | 出现 `(bar, offset) ≠ (1, 0x10010000)` 的 driver 侧 doorbell 调用 | ❌ 无「driver 侧」判定载体（`grep` 命中 0 ⇒ 恒不触发 ⇒ **空判据**）|
| **R-2** | 任一含 SoC 的配置出现 `bar_sizes[1] ≥ 268435464`（= 256MB+64KB） | ✅ **选它** |
| **R-3** | 冻结面 `docs/designs/dgpu-driver/architecture.md` 的 DOORBELL 行被正式修订 | ❌ 依赖**人**改文档（无可自动执行的触发面）|

**选 R-2 的理由（三条）**：

1. **它是旧 MUST 的「反面」** —— 旧 A-1 要求 `bar_sizes[1] ≥ 256MB+64KB`；R-2 正是「一旦这条变成现实 ⇒ 合成偏移落进实窗 ⇒ 前提失效」的**同一边界值** ⇒ 复活带与被废 MUST **同源**，不新造阈值。
2. **可自动执行且数值精确** —— `awk -F'[:,]' '{b=$3+0; if (b>=268435464) print}'`（裁决 §8.1 S-1 已实测：边界值命中 / 边界值-1 不命中 / 真实仓 16MB 不命中）⇒ ⛔ **不用正则**（正则 `[3-9][0-9]{8}` 漏匹配 268,501,000–299,999,999）。
3. **当下状态自洽** —— 三份 shipped 配置全为 16MB ⇒ 触发带**未命中** ⇒ 与「维持现状」并存不矛盾。

⇒ C-3 新文本**同时**保留 d3-roadmap 的**语义触发**（「真 driver 集成时 doorbell **物理地址 ≠ 0x10010000** ⇒ 才回头修」）与 R-2 的**可机判触发带** ⇒ 语义不得弱化（裁决 §3.3）+ 具备机判载体（裁决 §3.3 的 escape 要求）。

## D-3 · C-4 / C-5 / C-6 的同步边界

| 项 | 决定 | 理由 |
|:--:|---|---|
| **同步落点 = Requirement 正文**（⛔ 不动含偏移字面量的 Scenario 行） | ✅ | 契约的规范位置是 Requirement 正文；Scenario 是**例子**。裁决 AC-2 ① 要求 3 份连带 spec 的偏移字面量**命中集合与裁决前一致** ⇒ 追加到正文（正文行本身已含字面量 ⇒ 命中行数不变）**零风险**满足该判据；反之若改写 Scenario 行则命中集合会变 |
| **C-4 追加的措辞** | 「doorbell 识别**基于偏移量**(测试专用合成偏移),**不依赖** BAR1 实窗大小(16MB 实窗下同样命中)。」 | 与 C-1 的新 MUST **同向同形** ⇒ 3 份连带 spec 的口径一致 |
| **C-5 追加的措辞** | 同上 | 同上 |
| **C-6（`dgpu-soc-control-hub`）** | ⛔ **不改**（实测 `0x10010000\|kBar1DoorbellOffset` 命中 == **0** ⇒ 该 capability **不含** doorbell 偏移语义）| 裁决明令「⛔ 若不含 ⇒ **显式记录「不含，无需改」**（⛔ 不得静默跳过）」⇒ 本文件 + proposal 的 What Changes 表 + 验收报告**三处显式记录** |
| **`lwc-bus-hub-mvp` 的 NOTE 行 `dgpu_board_shell.cc:537`** | ⛔ **不改** | 该 NOTE 描述的是**被替换掉的历史直调**（今日 `:537` 已是 BAR2 分支；doorbell 分支在 `:566`，`handle_doorbell_transaction` 在 `:594`）⇒ 把它改指 `:566` 会指向**当前已不含该直调**的代码 ⇒ 反而更不准。⚠️ 记录为**已知的行号锚点腐化**（裁决 §6 已列该 3 处之一），⛔ 不在本 change 顺手改（改动最小化）|

## D-4 · ⚠️ AC-4 红因判据的**实测修正**（裁决括注被证伪）

裁决 AC-4 写：「红因必须是 doorbell 路由断言失败（鉴别测试锚定措辞「BAR1 doorbell 写」+ **`REQUIRE(rc == 0)` 失败**）」。

**实测结论（2026-11-21，影子副本 + 真实增量构建）**：

| 断言 | 删除 doorbell 分支后 | 说明 |
|:--:|:--:|---|
| `REQUIRE(rc == 0)`（`test_cpptlm_emulator_minimal_soc_smoke.cc` T1.4） | **仍为绿** | BAR1 写 `0x10010000`（256MB）超出 16MB 实窗 ⇒ `match_bar` 判 `BT::Unknown` ⇒ 落 `default: break` ⇒ 跳过 framebuffer 快路 ⇒ 仍写 `mmio_regs_` + 推 `inject_q_` ⇒ **`return 0`** ⇒ `rc` 不变 |
| `REQUIRE(board.pcie_ep_doorbell_count() == 1u)`（`test/test_dgpu_board_doorbell_routing.cc`） | **变红** | 该计数只在被删除的分支内 `++`（`src/tlm/gpu/dgpu_board_shell.cc` 的 doorbell 优先匹配分支）⇒ 计数保持 0 ⇒ 断言失败 |

⇒ **裁决的括注不成立**，但其**核心要求成立**：红因 = **doorbell 路由断言失败**（⛔ 编译失败 / 构建错误 / 找不到二进制 不构成红因）。
⇒ 本 change 的 AC-4 改以**实测红因**表述（`pcie_ep_doorbell_count()` 断言），并在验收报告里**显式登记**该修正（⛔ 不静默改判据）。

## D-5 · 为什么**只**改契约、不改代码与配置（裁决 N-2 / N-4 的复述 + 本 change 的实测确认）

- **不改 3 处硬编码**（`include/tlm/gpu/dgpu_board_shell.hh` · `include/tlm/pcie/pcie_endpoint_ip.hh` · `src/tlm/gpu/sdma_engine_tlm.cc` 裸字面量）：漏一处 = 行为分裂；且顶层 config 未知键被 `PcieEndpointIP` **静默忽略** ⇒ 配置写了不生效**比 hardcoded 更糟**。
- **不改 `configs/*.json`**：`bar_sizes[1]` 派生 `framebuffer_size_` ⇒ 改 256MB = 256MB FB 分配 ⇒ 破坏现状与既有测试。
- ⇒ 结论：**取值与来源都不动**，只让契约说实话。

## D-6 · C-7 的 ADR SU 形态

ADR-DGPU-09 已含 architect 2026-11-20 的 Q-12 SU（§「2026-11-20 — Q-12 裁决」），其末行写「**关联 change**：⏳ 待 builder 立项」⇒ 该指针在本 change 归档后**变陈旧**。

⇒ 本 change **追加**一段新 `## Status Update`（⛔ 正文零改动 · ⛔ 既有行 diff 行数 == 0）：

- 记录 Q-12 裁决的 **7 处连带已合并** + 本 change 名 + 裁决文件指针；
- 把「待 builder 立项」**结清**（指向已归档的 change）。

## D-7 · 归档合并的**预期副作用**（per I-11 的 AC-0 spike 实测）

`openspec archive` 的合并器会：① 在 `## Purpose` / `## Requirements` 后各插 1 空行；② 移除文件末空行；③ **丢弃中文 trailer**（`**维护**` / `**状态**`）但**保留英文 trailer**（`**Maintenance**` / `**Status**`）。

⇒ 本 change 涉及的 3 份 spec **均无 trailer**（实测 `grep -nE "^\*\*(维护|状态|Maintenance|Status)\*\*"` 命中 0）⇒ 副作用 ③ **不触发**；① ② 的空行变动属合并器固有行为（⛔ 非本 change 副作用，diff 里必须预期它）。
