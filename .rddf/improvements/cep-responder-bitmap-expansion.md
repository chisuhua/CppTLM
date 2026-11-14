---
name: cep-responder-bitmap-expansion
priority: P2
phase: phase-10
主题: ADR-CEP-21 **阶段 1（机制化）**落地 —— 三处硬编码全消 + 扩容流程成文 + 满射判据
依赖:
  - ADR-CEP-21
  - ADR-CEP-04
状态: "🟡 待实施(2026-11-14 起草 · **v2 · 已按 metis 第 1 轮 12 条建议逐条修订**)⚠️ 范围 = **仅阶段 1**;⛔ 不含阶段 2(扩容)"
created: 2026-11-14
---

# cep-responder-bitmap-expansion: ADR-CEP-21 阶段 1（机制化）

> **范围纪律**:⛔ **不含阶段 2（扩容）** —— `kResponderMappedBits` **必须仍 = 4**;⛔ **不改** `responder_set_t` 宽度;⛔ **不改** `SharerTrackerIf` 虚方法数(5);⛔ **不改** `ADR-CEP-04`;⛔ **不填**任何 node id;新增 invariant 数 = **0**。
> **产出形态**:标准 OpenSpec change（proposal / design / tasks）+ 实施 commit + verifier 验收
> **估时**:**0.5 ~ 0.7 人日**（3 文件 · **6 项** · 全部为**语义等价**替换 + 注释 + 1 条新判据;⚠️ v2 计入「编译零 warning 实测 + 变异验证」）
> **上游**:`docs/adr/ADR-CEP-21-sharer-bitmap-expansion.md`（🟢 Proposed · 双审通过 · 0 BLOCKER）· 台账 **Entry #421**（ADR 落盘）/ **#422**（档 2 普查）

## Why

**ADR-CEP-21 已 Proposed（双审通过、五步链走完），但其阶段 1 一行未动** ⇒ 决策与代码长期不一致。

**⚠️ v2 订正（metis 指出原稿夸大）**：原稿写「三处缺口**今天就存在**」是**修辞放大**。准确的分层是：

| # | 缺口 | 实测 | **今天**的后果 | 扩容后的后果 |
|:--:|---|---|---|---|
| **M3** | `test/cep_directory_experiment.hh:47` 的 `broadcast_fanout` 注释写「映射位只有 4 个 ⇒ **上限 4**」 | 见左 | ⚠️ **语义错误：今天就错**（4 是该实验参数的**默认值**;上限是 `kResponderMappedBits`）⇒ 掩盖真实上限 | 注释继续误导 |
| **M1** | 同文件 `mapped_prefix_mask` 用 `if (n > 4) n = 4;` 硬编码 | `:165` | ⚠️ **零影响**（唯一调用方 `:107`/`:139` 传的就是 `cfg_.broadcast_fanout` = 4） | ❌ 与常量脱钩 ⇒ clamp 仍只到 4 ⇒ **假绿** |
| **M2** | 同文件 snoop 遍历 `bit < 4` 硬编码 | `:180` | ⚠️ **零影响**（同上,当前常量就是 4） | ❌ 只探前 4 个节点 ⇒ **假绿** |
| **M4** | 扩容流程**无成文定义** | ADR-CEP-21 之前全无 | ⚠️ 首个需要 > 4 的族会**先撞静默 no-op** 再被发现 | 同左 |
| **M5** | `test/test_cep_sharer_tracker_base.cc:214-219` 的双向一致性用**显式 initializer list**（4 个具体 node） | 见左 | ✅ **今天无害**（与 W=4 等价） | ❌ bits 4~7 **零覆盖** ⇒ 新位映射错也发现不了 |

⇒ **只有 M3 是「今天就错」;M1/M2 是「今天无害、扩容才致命」;M4 是流程缺口;M5 是为阶段 2 预置的守门**。
⇒ ⚠️ **反向动机（降级为 review 门）**：本 change 的价值**只**在 M1~M5 的**机制就绪**，⛔ 不在「消除今日假绿」（今日仅 M3 一处）。**若实施中出现「为让 ADR 好看」而改变运行时行为 ⇒ 本 change 失败。**

**⚠️ 仓内无「某族需要 > 4 sharer」的实测需求**（Entry #422 的 8 族普查：适用仅 2 族，`mesif` 已覆盖、`tilelink_c` 可推广但未做）⇒ **本 change 不扩容**。

## What Changes

### ① 交付项（**6 项** —— v2 由 5 项增补 `using`/全限定行）

| # | ADR 条目 | 改动 | 语义等价性 |
|:--:|---|---|:--:|
| **W1** | **S1-1①** | `test/cep_directory_experiment.hh` 的 snoop 遍历：`for (unsigned bit = 0; bit < 4; ++bit)` ⇒ `bit < tlm::cep::kResponderMappedBits`。⚠️ **必须全限定** —— 该文件的 `namespace cep_exp` 的 `using` 列表（`:30-39`）**无** `namespace cep = tlm::cep;` ⇒ 写 `cep::` **编译失败**（metis 实测）。⚠️ **循环变量保持 `unsigned`** —— `responder_node_of(unsigned)` 同名类型，且 `sharer_tracker.hh:118` 已有 `unsigned` vs `std::size_t` 比较的**同型先例**（双方均无符号 ⇒ **无 `-Wsign-compare`**） | ✅ 当前 4 ⇒ 等价 |
| **W2** | **S1-1②** | 同文件 `mapped_prefix_mask`：`if (n > 4) n = 4;` ⇒ `if (n > tlm::cep::kResponderMappedBits) n = static_cast<std::uint64_t>(tlm::cep::kResponderMappedBits);`。⚠️ **⛔ 不用 `std::min`** —— 那会引入 `<algorithm>` 新依赖 + 显式模板实参（ADR 字面写了 `std::min` ⇒ **此处偏离并登记**：等价且零新依赖）。⚠️ **保留 `(n == 0) ? 0 : ((1<<n)-1)` 不变** | ✅ 当前 4 ⇒ 等价 |
| **W3** | **S1-1③** | 同文件 `broadcast_fanout` 的**注释**重写（⛔ 不得写「上限 = `kResponderMappedBits`」—— 该参数的**上限是它自己**，常量只是**循环/掩码**上限）⇒ 新注释：<br>`/// 无 sharer 信息时的广播**探测量** = 本参数**默认值**;`<br>`/// 调用方可配到 [0, tlm::cep::kResponderMappedBits];snoop 循环上界 = kResponderMappedBits。` | ✅ 仅注释（**值 4 保留**） |
| **W4** | **S1-2** | `include/tlm/cep/sharer_tracker.hh` 的 `kResponderBitToNode` 上方**成文登记四项**（v2 按 metis 补齐）：<br>① 扩容 = 改 `kResponderMappedBits` + 填表项;**⚠️ `W > 8` 时必须先扩 `kResponderBitToNode` 数组**（现为 `std::array<node_id_t, 8>` ⇒ 否则 `responder_node_of` **越界 OOB/UB**）<br>② **64 是 `responder_set_t` 的上限,⛔ 不是表的上限**;`kNoBit = 64u` 同值<br>③ 扩容**必须**同步:（a）`openspec/specs/cep-sharer-tracker-base/spec.md:140` 的 spec delta（`bits 4..63` ⇒ `bits W..63`）·（b）新族的装配期硬断言（`responder_mask_of(node)==0 ⇒ mark_observation_invalid`）·（c）运行时 mask 判据（`kMappedResponderMask == (1<<W)-1`）<br>④ ⛔ 不得填无权威来源的 node id;**若某族需 > 64 sharer ⇒ 另立 ADR**（候选:层级聚合 / 多字位图） | ✅ 仅注释 |
| **W5** | **S1-4** | `test/test_cep_sharer_tracker_base.cc` **新增满射判据**：`for (std::size_t b = 0; b < cep::kResponderMappedBits; ++b) { CHECK(cep::responder_node_of(b) != cep::kInvalidNode); CHECK(cep::responder_bit_of(cep::responder_node_of(b)) == b); }`。⚠️ **⛔ 不得替代** `:211-212` 与 `:214-219`（满射只证**双射自洽**,**不钉 bit→具体 node** ⇒ 置换两表项仍绿）⇒ **两组并存** | ✅ 新增断言（W=4 时与既有的 4 个 node 断言等价） |
| **W6** | **（v2 新增）** | `test/cep_directory_experiment.hh` 的 `using` 列表（`:30-39`）**加** `using tlm::cep::kResponderMappedBits;` ⇒ 与 W1 二选一（**v2 定为：W1 用全限定 ⇒ 本项不做**;⚠️ 若实施时发现全限定在 `static` 成员函数内不可用则回退到本项） | ✅ |

### ② ⛔ 明确排除（out-of-scope）

| 排除项 | 理由 |
|---|---|
| **阶段 2（扩容到 W）** | ADR 明定「**触发后**执行，⛔ 不得预先执行」;且仓内无 > 4 的实测需求 |
| **`broadcast_fanout` 的值改为 `kResponderMappedBits`** | **v2 裁决 = D-2 方案 (b)：值保留 4**。理由：① 它是**实验参数**（语义 = 默认探前 N 个）⇒ 绑到常量会失去独立可调性;② 方案 (a) 会在阶段 2 把 broadcast latency 从 18 改到 26 ⇒ **破坏与 `experimental/` 已冻结 run 的可比性**。⚠️ 代价 = M1/M2 的「今日后果」保持零影响（已如实改写） |
| **V1-7（观测面可定位触发节点）** | ADR 的 V1-7 标注「阶段 1 需补;**若判定不补则 2a 降级为人裁 + 装配日志**」⇒ **本 change 判「不做」并登记为待 architect 裁定**（⛔ planner 不代裁 —— ADR 的判权在 architect）。若 architect 判「必须补」⇒ 另立 change（会触及 `mesif_protocol_engine.hh` 的无效标记路径） |
| **`nvlink_4` / `moesi_gpu` / `cxl_3_0_symmetric` 的档 2 适用性** | Entry #422 已判**不适用** ⇒ ⛔ 本 change 不碰 |
| **`tilelink_c` 的档 2 推广** | 用户裁决 A：只普查、不推广 |
| **消除 `NO_INV` 双真源** · **D-1（objective schema）** | 既有债 ⇒ 各自另案 |

### ③ ⚠️ 已知风险（v2 订正）

| # | 风险 | 防线 |
|:--:|---|---|
| **R-1** | ~~`-Wsign-compare`~~ ⇒ **v2 删除该论断**（`unsigned` vs `std::size_t` 双方均无符号;`:118` 有同型先例）⇒ 真正风险 = **遗漏全限定** ⇒ 编译失败 | W1 用 `tlm::cep::` 全限定;AC 加「`cmake --build` rc=0」 |
| **R-2** | ~~`n == 0` 短路被破坏~~ ⇒ **v2 订正**：`(1<<0)-1 = 0` ⇒ **短路不是 correctness 必需**。真正要守的是 **`n >= 64` 的 shift UB**（`1 << 64`）⇒ `mapped_prefix_mask` **无** `kMappedResponderMask` 那样的 `>= 64` 三元守卫 | **v2 新增**：在 `sharer_tracker.hh` 加 `static_assert(kResponderMappedBits <= 64, ...)`（属 W4 注释块的一部分）⇒ 守卫从「运行期」上移为「编译期」 |
| **R-3** | W5 在 W=4 时**恒真**（与 ADR 反对的「死判据」同形） | W5 的**判别力只在 `kResponderMappedBits > 4` 时生效** ⇒ 须在注释与 AC 里**显式写明**「本判据今日无增量判别力,是为阶段 2 预置」⛔ 不得计入「今日价值」 |
| **R-4** | W4 的注释若被写成「W 可到 64」⇒ 读者会在 `W > 8` 时撞 OOB | W4① **必须**写明「`W > 8` 须先扩数组」 |
| **R-5** | `count_member_virtuals`（`test_cep_sharer_tracker_base.cc:81-90`）先 `strip_comments` 再数行首 `"    virtual "` ⇒ W4 注释**不得**出现行首 4 空格 + `virtual` | W4 注释不写 `virtual` 字样 |

## Acceptance

> **判据来源**:ADR-CEP-21 的 **Verification 阶段 1 组（V1-1 ~ V1-6）** + **X-2** + 本 improvement 的 W1 ~ W6。

| # | 判据 | 可机判的验证动作 |
|:--:|---|---|
| **AC-1** | **三处硬编码全消**（V1-1 · **v2 限定单文件**） | ① `grep -c 'bit < 4' test/cep_directory_experiment.hh` = **0** 且含 `tlm::cep::kResponderMappedBits`;② `grep -c 'n > 4' test/cep_directory_experiment.hh` = **0** 且含 `tlm::cep::kResponderMappedBits`;③ `grep -n 'broadcast_fanout' test/cep_directory_experiment.hh` ⇒ 只允许 `:47` 声明 + `:107`/`:139` 调用，且**注释含「默认」且不含「上限 4」** |
| **AC-2** | **注释四项齐备**（V1-2 · **v2 加内容正确性**） | `kResponderBitToNode` 上方注释含:① 「改 `kResponderMappedBits` + 填表项」**且**「`W > 8` 须先扩数组」② 「64 是 `responder_set_t` 上限」**且**「不是表的上限」③ 「spec delta + 装配期断言 + 运行时 mask 判据」三项同步义务 ④ 「不得填无权威来源」+「> 64 ⇒ 另立 ADR」 |
| **AC-3** | **编译通过 + 零新增 warning**（**v2 新增** —— AC-1 全是 grep,代码不编译也能「通过」） | `cmake --build build -j` ⇒ rc=**0**;对比改动前后的 warning 数 ⇒ **零新增** |
| **AC-4** | **阶段 2 未执行**（V1-4 · **v2 放宽写法**） | `grep -cE 'kResponderMappedBits\s*=\s*4u?;' include/tlm/cep/sharer_tracker.hh` = **1**;且 **W5 满射判据绿**;且原有用例（含 `:211-219`）全绿 |
| **AC-5** | **0 回归**（V1-5） | `ctest` 全绿 · `scripts/test/cep_*.sh` 逐个 rc=0 · `format --check` rc=0 |
| **AC-6** | **诚实登记**（V1-6） | 「当前无机器闸钉死 `kResponderMappedBits == 4`」+「W5 今日无增量判别力」⇒ **须在 `tasks.md` 显式登记** |
| **AC-7** | **满射判据有牙**（R-3 · **v2 改为唯一可隔离的变异**） | 影子树里把 `kResponderMappedBits` 4→**5** 且 **不填** `kResponderBitToNode[4]` ⇒ **只有 W5 红**（`:214-219` 仍绿）⇒ 还原 ⇒ 全绿。⚠️ **须显式记录**：改 `bit 0..3` 任一表项**无法**只打红满射（`:214-219` 会同时红）⇒ ⛔ 不写「只触发」以外的措辞 |
| **AC-8** | **范围纪律** | `git diff` ⇒ `test/cep_experiment_harness.hh` · `docs/adr/` · `include/tlm/cep/protocol_instances.json` · `test/cep_evidence_tiers.json` · **`openspec/specs/cep-sharer-tracker-base/spec.md`**（**v2 新增** —— ADR 依赖点 #7 是**阶段 2** 义务,阶段 1 顺手改即制造 spec/实现脱钩）· **`experimental/run-*/metrics.json`**（**v2 新增** —— 语义等价的数值证据,比重跑实验便宜且等效）⇒ **全部零 diff**;新增 `INV-CEP-` = **0**;`SharerTrackerIf` 虚方法数 = **5**;**`static_assert(sizeof(responder_set_t) == 8)` 仍在**（**v2 新增**,ADR X-2） |

## Capabilities

**新增能力（⚠️ v2 按 metis 收紧表述 —— 不再声称「消除今日假绿」）**:

| # | 能力 | 之前 | 之后 | ⚠️ 生效条件 |
|:--:|---|---|---|---|
| **C-1** | **实验覆盖面的循环上界与掩码 clamp 不再硬编码** | 3 处魔法数 | 全部引用 `kResponderMappedBits` | ✅ **立即可用**（但**实际覆盖面由调用方 `broadcast_fanout` 决定** ⇒ 当前仍 4 ⇒ ⛔ 不称「自动同步」） |
| **C-2** | **扩容流程可查**（含 `W > 8` 的 OOB 警告 + 三项同步义务 + >64 出口） | 无成文 | 注释 4 项 | ✅ 立即可用 |
| **C-3** | **新位映射有守门用例** | initializer list ⇒ bits ≥ 4 零覆盖 | 满射判据 | ⚠️ **仅 `kResponderMappedBits > 4` 时有判别力** ⇒ 今日**无增量**（为阶段 2 预置） |
| **C-4** | **注释语义正确** | 「上限 4」（错） | 「默认 4;循环/掩码上界 = `kResponderMappedBits`」（对） | ✅ 立即可用（**今日唯一实打实的修复**） |

**⛔ 不新增**:`kResponderMappedBits` 仍 = **4** · invariant **0** · ADR 数 **0** · 冻结产物数不变 · 运行时行为不变

## Impact

| 文件 | 改动 | 冻结面 |
|---|---|:--:|
| `test/cep_directory_experiment.hh` | W1（snoop 循环上界）· W2（`mapped_prefix_mask` clamp）· W3（`broadcast_fanout` 注释）· （W6 备选:using 行） | ✗ |
| `include/tlm/cep/sharer_tracker.hh` | W4（注释 4 项）+ **`static_assert(kResponderMappedBits <= 64)`**（R-2 的编译期守卫） | ✗（实测不在 M1 冻结集） |
| `test/test_cep_sharer_tracker_base.cc` | W5（新增满射判据） | ✗ |

**KPI**:Proposed **0/3 → 1/3**（本 change 占 1 slot）
**回滚**:单 commit ⇒ `git revert`（无数据迁移、无 schema 变更、无行为变化）

## 待裁

| # | 待裁项 | 谁能裁 | 本 improvement 的处置 |
|:--:|---|---|---|
| **D-1** | **V1-7（观测面可定位触发节点）不补**是否可接受 | **architect**（ADR 判权） | 本 change **不做**，并**登记为待 architect 裁定**（⛔ planner 不代裁）;若判「必须补」⇒ 另立 change（触及 `mesif_protocol_engine.hh`） |
| **D-2** | `broadcast_fanout` 的**值**是否跟随 `kResponderMappedBits` | architect | **v2 裁决 = 方案 (b)：值保留 4**（理由见 §② 排除项）⇒ ⚠️ 若 architect 反对 ⇒ 阶段 2 会改 broadcast latency ⇒ 须在 ADR SU + 台账登记可比性影响 |
| **D-3** | `std::min` vs `if` 的偏离 | architect | **v2 裁决 = `if` + `static_cast`**（零新 include、零模板实参）⇒ ⚠️ 与 ADR S1-1② 字面不一致 ⇒ **已登记为偏离** |

## metis 审查（第 1 轮 · 2026-11-14）处置，逐条

> **审查者**:metis（planner 域固定审查者 · 本 improvement **总计 1 轮**）· 返回 6 节 · **12 条建议 + 多项事实发现**
> **处置**:**12 条全部采纳**（无驳回）;其中 **P0 四条是阻断级**（含一条会导致**编译失败**的硬错误）

| # | metis 发现 | 独立复核 | 处置 |
|:--:|---|---|---|
| **1** | W1/W2 的 `cep::kResponderMappedBits` **本文件不存在**（`namespace cep_exp` 的 using 列表无 `namespace cep`）⇒ **编译失败** | ✅ **成立**（`:28` 是 `namespace cep_exp`;`:30-36` using 列表无 cep 别名） | **W1/W2 改 `tlm::cep::` 全限定** + **W6 备选 using 行** + **AC-3 加「编译 rc=0」** |
| **2** | **D-2 必须绑定** —— `broadcast_fanout` 唯一调用方传 4 ⇒ W=8 时 `mapped_prefix_mask(4)` 仍只覆盖 4 位 ⇒ M1/M2 的收益**不成立** | ✅ **成立**（`:107`/`:139` 实测传 `cfg_.broadcast_fanout`;`ServiceConfig` 从未被 override） | **D-2 裁方案 (b)** ⇒ **M1/M2 的「今日后果」改写为「零影响」** + **C-1 删「自动同步」绝对表述** |
| **3** | AC-3 自我矛盾且**实测不可满足**（全仓 `n > 4` 有 3 处无关误报：`writeback.cc:31` + 2 个 PCIe 测试的 `len > 4096` 子串） | ✅ **成立**（实测 4 处命中,仅 1 处是目标） | **AC-3 重写为三条单文件判据**（`grep` 限定 `test/cep_directory_experiment.hh`） |
| **4** | AC-7 的变异**不可隔离**（改 `bit 0..3` ⇒ `:214-219` 同时红;改 index 4 ⇒ 被短路惰性） | ✅ **成立** | **AC-7 改为「影子树 `kResponderMappedBits` 4→5 且不填第 5 项」⇒ 只有 W5 红** + 显式记录「改 0..3 无法只打红满射」 |
| **5** | W3 的「实际上限 = `kResponderMappedBits`」**又是错的**（该参数上限是它自己） | ✅ **成立** | **W3 注释重写**（「探测量 = 默认值;调用方可配到 [0, kResponderMappedBits]」） |
| **6** | W4 四项**内容不全**（漏 `W > 8` 的 OOB / 64 是 `responder_set_t` 上限非表上限 / 三项同步义务） | ✅ **成立**（`kResponderBitToNode` 是 `array<node_id_t, 8>`;`responder_node_of` 上界用 `kResponderMappedBits` ⇒ W>8 直接 OOB） | **W4 重写四项**（含 ①`W>8` 须扩数组 ② 64 非表上限 ③ spec delta + 装配断言 + mask 判据 ④ 不得填无权威 id + >64 另立 ADR） |
| **7** | W2 的 `std::min` 引入新 include + 模板实参 ⇒ 过度设计 | ✅ **成立** | **W2 改 `if` + `static_cast`** + **D-3 登记为对 ADR 字面的偏离** |
| **8** | W1 的 `-Wsign-compare` 论断**错误**（双方均无符号;`:118` 有同型先例） | ✅ **成立** | **W1 保持 `unsigned bit`** + **R-1 改写为「遗漏全限定 ⇒ 编译失败」** |
| **9** | 缺 AC：X-2 static_assert / 编译通过 / `spec.md` 零 diff / metrics 逐键一致 | ✅ **成立** | **AC-3（编译）· AC-8 补 X-2 + `spec.md` + `experimental/run-*/metrics.json` 零 diff** |
| **10** | D-1（V1-7 不补）判权在 architect，planner 不代裁 | ✅ **成立**（ADR 原文「若判定不补」未定义判权归属） | **D-1 明确「登记为待 architect 裁定」** + §② 排除项同步 |
| **11** | R-2 的 `n == 0` 短路**不是 correctness 必需**（`(1<<0)-1 = 0`）;真正要守的是 `n >= 64` 的 shift UB | ✅ **成立** | **R-2 改写** + **W4 加 `static_assert(kResponderMappedBits <= 64)`**（守卫上移为编译期） |
| **12** | 数量自相矛盾（`5 项` vs `4 项`）+ Impact 表缺 using 行 | ✅ **成立** | **统一为 6 项**（W1~W6,含 W6 备选）+ Impact 表补齐 |

**⚠️ metis 的另一处关键提醒（已纳入）**：W5 在 W=4 时**恒真** ⇒ 与 ADR 自己反对的「死判据」同形 ⇒ **必须在注释与 AC-6 显式写明「今日无增量判别力,为阶段 2 预置」**（⛔ 不得计入今日价值）—— 已写入 C-3 的「生效条件」列与 AC-6。

## 关联

· `docs/adr/ADR-CEP-21-sharer-bitmap-expansion.md`（🟢 Proposed · 本 change 是其**阶段 1** 的落地）· 台账 **Entry #421** / **#422** / **#420** / **#419** · `ADR-CEP-04`（不可变）· `include/tlm/cep/sharer_tracker.hh`（`:91` `kNoBit` / `:92` `kResponderMappedBits` / `:96-102` `kResponderBitToNode` / `:118` `responder_node_of` 上界 / `:125-127` `kMappedResponderMask`）· `test/cep_directory_experiment.hh`（`:28` `namespace cep_exp` / `:30-39` using 列表 / `:47` / `:107` / `:139` / `:165` / `:180`）· `test/test_cep_sharer_tracker_base.cc`（`:81-90` `count_member_virtuals` / `:211-219` / `:221`）· `openspec/specs/cep-sharer-tracker-base/spec.md:140`（**阶段 2** 义务,本 change ⛔ 不动）
