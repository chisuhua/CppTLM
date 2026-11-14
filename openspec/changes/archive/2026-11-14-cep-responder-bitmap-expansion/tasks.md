# tasks: cep-responder-bitmap-expansion（ADR-CEP-21 阶段 1）

> **范围**:仅阶段 1（机制化）· `kResponderMappedBits` **必须仍 = 4**

## W1 · snoop 遍历上界去硬编码
- [x] `test/cep_directory_experiment.hh` 的 `for (unsigned bit = 0; bit < 4; ++bit)` ⇒ `bit < tlm::cep::kResponderMappedBits`（⚠️ **必须全限定** —— `namespace cep_exp` 无 `cep` 别名 ⇒ 写 `cep::` 编译失败，per metis P0-1）⇒ 循环变量保持 `unsigned`（与 `responder_node_of(unsigned)` 同型;`:118` 有 `unsigned` vs `size_t` 同型先例 ⇒ 无 warning）

## W2 · `mapped_prefix_mask` 去硬编码
- [x] `if (n > 4) n = 4;` ⇒ `if (n > tlm::cep::kResponderMappedBits) n = static_cast<std::uint64_t>(tlm::cep::kResponderMappedBits);`（⛔ **不用 `std::min`** —— 零新 include;per design D-2,已登记为对 ADR 字面的偏离）
- [x] 保留 `(n == 0) ? 0 : ((1<<n)-1)` 不变（⚠️ metis 订正:`n == 0` 短路**非** correctness 必需,保留仅为可读性）
- [x] 注释「bit 0..3」⇒ 「bit 0 .. `kResponderMappedBits-1`」

## W3 · `broadcast_fanout` 注释重写
- [x] `test/cep_directory_experiment.hh` 的注释 ⇒ 「探测量 = 本参数**默认值**;调用方可配到 [0, `kResponderMappedBits`];snoop 循环上界 = `kResponderMappedBits`」+ 「本参数的上限**是它自己**,⛔ 不是 `kResponderMappedBits`」（⚠️ metis:原拟的「实际上限 = `kResponderMappedBits`」**又是错的**）
- [x] **值保留 4**（per design D-3:实验参数独立性 + 冻结 run 可比性）

## W4 · 扩容流程成文 + 编译期守卫
- [x] `include/tlm/cep/sharer_tracker.hh` 的 `kResponderBitToNode` 上方注释**四项**：① 扩容 = 改常量 + 填表项，**`W > 8` 须先扩数组**（否则 `responder_node_of` OOB/UB）② **64 是 `responder_set_t` 上限,⛔ 不是表上限**（表上限 = 数组尺寸 8）③ **三项同步义务**（spec delta / 装配期硬断言 / 运行时 mask 判据）④ ⛔ 不得填无权威来源 id + **> 64 ⇒ 另立 ADR**
- [x] `static_assert(kResponderMappedBits <= 64, ...)`（per design D-5:把守卫从运行期上移为编译期）
- [x] 注释**不含**行首 4 空格 + `virtual`（否则 `count_member_virtuals` 的 strip_comments 会受影响）

## W5 · 满射判据
- [x] `test/test_cep_sharer_tracker_base.cc` 新增 `for (b in [0, kResponderMappedBits)) { responder_node_of(b) != kInvalidNode; responder_bit_of(responder_node_of(b)) == b; }`
- [x] ⚠️ **不替代** `:211-212` 与 `:214-219`（满射只证双射自洽,不钉 bit→具体 node）
- [x] 补 `#include <cstddef>`

## W7 · delta spec（⚠️ 立项后补 —— openspec 要求 change 至少 1 个 delta）

- [x] 新增 `openspec/changes/cep-responder-bitmap-expansion/specs/cep-sharer-tracker-base/spec.md`：**ADDED** Requirement「responder 位宽扩容的机制与触发判据」（6 个 Scenario）
- [x] ⚠️ **这提升而非扩大范围**：把 W4 的「注释」升为 **Requirement** ⇒ 机制从「文档约定」变「契约」✅
- [x] `openspec validate cep-responder-bitmap-expansion --strict` ⇒ **valid**（rc=0）

## W6 · （备选，未执行）
- [x] **已决定不做**（⛔ 非待办）—— W1 用 `tlm::cep::` 全限定已足够;若将来需要 `using` 别名则回退到本项（决策依据见 design D-1）

## 验证

- [x] **AC-1** 三处硬编码全消（单文件 grep）: `bit < 4` = 0 · `n > 4` = 0 · 注释无「上限 4」;含 `tlm::cep::kResponderMappedBits` 4 处
- [x] **AC-2** 注释四项齐备（含 `W > 8` 扩数组 / 64 非表上限 / 三项同步 / 不得填无权威 id / >64 另立 ADR）
- [x] **AC-3** `cmake --build build --target cpptlm_tests` rc=**0** 且**零 error / 零相关新增 warning**
- [x] **AC-4** `kResponderMappedBits = 4;` 仍为 **1** 处;W5 满射判据绿;原有用例全绿
- [x] **AC-5** `[cep]` **527 cases / 11797 assertions** 全绿 · 全量 `ctest` / 42 闸 / `format --check` 见台账回执
- [x] **AC-6 ⚠️ 诚实登记两项**：① **当前无任何机器闸钉死 `kResponderMappedBits == 4`** ⇒ 阶段 2 的「未执行」只能靠 AC-4 的 grep + 人肉复核;② **W5 在 `kResponderMappedBits = 4` 时恒真** ⇒ **今日无增量判别力**，其价值**只在 `> 4` 时**兑现（为阶段 2 预置）
- [x] **AC-7** 变异实测：`kResponderMappedBits` 4→**5** 且不填表项 ⇒ **W5 必红**（`responder_node_of(4) != kInvalidNode`）+ **sentinel 断言同时红**（`responder_bit_of(kInvalidNode)` = **4 ≠ 64**;`mask(kInvalidNode)` = **16 ≠ 0**）+ 连带 3 个 test case ⇒ ⚠️ **⛔ 不写「只有 W5 红」**;这组结果**实证了 ADR 选项 B 的否决理由**（sentinel 破坏）⇒ 已还原并复验全绿
- [x] **AC-8** 范围纪律：`test/cep_experiment_harness.hh` · `docs/adr/` · `protocol_instances.json` · `cep_evidence_tiers.json` · `openspec/specs/cep-sharer-tracker-base/spec.md`（⚠️ **archive 前**零 diff —— 归档时 delta 会合并,那是正常流程）· `experimental/run-*/metrics.json` **全零 diff**;新增 `INV-CEP-` = **0**;`SharerTrackerIf` 虚方法 = **5**;`static_assert(sizeof(responder_set_t) == 8)` **仍在**
