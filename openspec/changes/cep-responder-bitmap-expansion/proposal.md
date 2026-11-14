# proposal: cep-responder-bitmap-expansion —— ADR-CEP-21 阶段 1（机制化）

> **状态**: 🟡 Proposed（2026-11-14 立项 · planner 起草 · 待 builder 补 design/tasks 并实施）
> **依据**: `docs/adr/ADR-CEP-21-sharer-bitmap-expansion.md`（🟢 Proposed · oracle + critic 双审通过 · **0 BLOCKER**）· improvement `.rddf/improvements/cep-responder-bitmap-expansion.md`（v2 · metis 12 条全采纳）· 台账 **Entry #421** / **#422**
> **KPI**: 立项后 Proposed **0/3 → 1/3**

## 为什么

ADR-CEP-21 已 Proposed（双审通过、五步链走完），但其**阶段 1（机制化）一行未动** ⇒ 决策与代码长期不一致。

**⚠️ 如实分层（⛔ 不夸大）**：

| 缺口 | 今天的后果 | 扩容后的后果 |
|---|---|---|
| **M3** `broadcast_fanout` 注释写「映射位只有 4 个 ⇒ 上限 4」 | ⚠️ **语义错误:今天就错**（4 是默认值;循环/掩码上限才是 `kResponderMappedBits`） | 注释继续误导 |
| **M1** `mapped_prefix_mask` 的 `if (n > 4) n = 4;` | ⚠️ **零影响**（唯一调用方传的就是 4） | ❌ 与常量脱钩 ⇒ 假绿 |
| **M2** snoop 遍历 `bit < 4` | ⚠️ **零影响**（同上） | ❌ 只探前 4 个节点 ⇒ 假绿 |
| **M4** 扩容流程无成文定义 | ⚠️ 首个需要 > 4 的族会先撞**静默 no-op** | 同左 |
| **M5** 双向一致性用显式 initializer list（4 个 node） | ✅ **今天无害** | ❌ bits 4~7 **零覆盖** |

⇒ **今日唯一实打实的错误是 M3**;其余为「机制就绪」⇒ ⛔ 本 change 的价值**不**在「消除今日假绿」。

⚠️ **仓内无「某族需要 > 4 sharer」的实测需求**（Entry #422 的 8 族普查：适用仅 2 族，`mesif` 已覆盖、`tilelink_c` 可推广未做）⇒ **本 change 不扩容**。

## 变更内容

**范围 = 仅阶段 1**，6 项（W1 ~ W6），详见 improvement 的 §What Changes。

| # | 内容 | 文件 |
|:--:|---|---|
| **W1** | snoop 遍历 `bit < 4` ⇒ `bit < tlm::cep::kResponderMappedBits`（⚠️ **必须全限定** —— 该文件的 `namespace cep_exp` 无 `cep` 别名 ⇒ 写 `cep::` **编译失败**） | `test/cep_directory_experiment.hh` |
| **W2** | `if (n > 4) n = 4;` ⇒ `if (n > tlm::cep::kResponderMappedBits) n = static_cast<std::uint64_t>(...);`（⛔ 不用 `std::min` —— 零新 include） | 同上 |
| **W3** | `broadcast_fanout` 注释重写（⛔ 不得写「上限 = `kResponderMappedBits`」—— 该参数上限是它自己） | 同上 |
| **W4** | `kResponderBitToNode` 上方注释 4 项（含 **`W > 8` 须先扩数组**——否则 OOB;64 是 `responder_set_t` 上限**非表上限**;三项同步义务;不得填无权威 id）+ `static_assert(kResponderMappedBits <= 64)` | `include/tlm/cep/sharer_tracker.hh` |
| **W5** | 新增**满射判据**（`responder_node_of(b) != kInvalidNode && responder_bit_of(responder_node_of(b)) == b`）⚠️ **不替代**既有 `:211-219` | `test/test_cep_sharer_tracker_base.cc` |
| **W6** | （备选）`using tlm::cep::kResponderMappedBits;` | `test/cep_directory_experiment.hh` |
| **W7** | **delta spec**（⚠️ 立项后补 —— openspec 要求至少 1 个 delta）：`ADDED` Requirement「responder 位宽扩容的机制与触发判据」+ 6 Scenario ⇒ **把 W4 的「注释」升为「Requirement」** ⇒ 机制从文档约定变契约 | `specs/cep-sharer-tracker-base/spec.md` |

**⛔ 边界**：`kResponderMappedBits` **必须仍 = 4** · `responder_set_t` 宽度不变 · `SharerTrackerIf` 虚方法 = 5 · `ADR-CEP-04` 零 diff · 新增 invariant = **0** · 不填任何 node id · **运行时行为不变**（三处替换均为语义等价）

## 验收标准

| # | 判据 |
|:--:|---|
| **AC-1** | 三处硬编码全消（**grep 限定单文件** —— 全仓 `n > 4` 有 3 处无关误报） |
| **AC-2** | 注释四项齐备**且内容正确**（含 `W > 8` 须扩数组 / 64 非表上限 / 三项同步义务） |
| **AC-3** | **`cmake --build` rc=0 且零新增 warning**（AC-1 全是 grep，代码不编译也能「通过」） |
| **AC-4** | 阶段 2 未执行（`kResponderMappedBits = 4`）+ W5 满射判据绿 + 原有用例全绿 |
| **AC-5** | 0 回归（`ctest` 全绿 · `scripts/test/cep_*.sh` 逐个 rc=0 · `format --check` rc=0） |
| **AC-6** | 诚实登记（「无机器闸钉死 4」+「W5 今日无增量判别力」写入 `tasks.md`） |
| **AC-7** | 满射判据有牙：影子树 `kResponderMappedBits` 4→5 不填第 5 项 ⇒ **只有 W5 红** |
| **AC-8** | 范围纪律：`test/cep_experiment_harness.hh` · `docs/adr/` · `protocol_instances.json` · `cep_evidence_tiers.json` · **`openspec/specs/cep-sharer-tracker-base/spec.md`** · **`experimental/run-*/metrics.json`** 全零 diff;新增 `INV-CEP-` = 0;虚方法数 = 5;**`static_assert(sizeof(responder_set_t) == 8)` 仍在** |

## 不做什么

| 排除项 | 理由 |
|---|---|
| **阶段 2（扩容到 W）** | ADR 明定「触发后执行，⛔ 不得预先执行」;仓内无 > 4 的实测需求 |
| **`broadcast_fanout` 值改为 `kResponderMappedBits`** | 它是**实验参数**（默认探 N 个）⇒ 绑定会失去可调性;且阶段 2 会把 broadcast latency 18→26 ⇒ 破坏与已冻结 run 的可比性 |
| **V1-7（观测面可定位触发节点）** | ⚠️ **登记为待 architect 裁定**（ADR 的判权在 architect,⛔ planner 不代裁）;若判「必须补」⇒ 另立 change |
| `nvlink_4` / `moesi_gpu` / `cxl_3_0_symmetric` 的档 2 适用性 | Entry #422 已判**不适用** |
| `tilelink_c` 的档 2 推广 | 用户裁决 A：只普查、不推广 |
| 消除 `NO_INV` 双真源 · D-1（objective schema） | 既有债 ⇒ 各自另案 |
