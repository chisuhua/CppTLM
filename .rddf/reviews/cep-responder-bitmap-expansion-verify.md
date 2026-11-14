# 验收报告 —— `cep-responder-bitmap-expansion`（ADR-CEP-21 阶段 1）

> **角色**:verifier（只验不改）· **日期**:2026-11-14 · **已用轮次**:**1**
> **判据来源**:① `openspec/changes/cep-responder-bitmap-expansion/proposal.md` 的 **AC-1 ~ AC-8** ② ADR-CEP-21 的 **V1-1 ~ V1-6 / X-1 ~ X-4**
> **验收对象**:change 全部交付面（6 项 + W7 delta · 3 代码文件 + 1 spec delta）

## 0 · 判定汇总

| 判定 | 条数 |
|---|:--:|
| ✅ **通过** | **8 / 8**（AC-1 ~ AC-8） |

⇒ **总判定：通过**（0 未通过;1 项为**已披露的诚实登记**,非缺口）。

## 1 · 逐条判定与工具证据

| # | 判据 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| **AC-1** | 三处硬编码全消（单文件 grep） | ✅ | `bit < 4` = **0** · `n > 4` = **0** · `上限 4` = **0**;含 `tlm::cep::kResponderMappedBits` = **4** 处 |
| **AC-2** | 注释四项齐备且内容正确 | ✅ | 五项逐一命中：填本表项 /「`W > 8` 时必须先扩本数组」/「掩码上限 = 64」/「不得填无权威来源」/「另立 ADR」 |
| **AC-3** | 编译 rc=0 且零新增 warning | ✅ | `cmake --build build --target cpptlm_tests` ⇒ **成功**;`grep` 三个目标文件的 error/warning ⇒ **空** |
| **AC-4** | 阶段 2 未执行 | ✅ | `include/tlm/cep/sharer_tracker.hh:92` = `kResponderMappedBits = 4;`（唯一一处）;满射判据绿;原有用例全绿 |
| **AC-5** | 0 回归 | ✅ | `ctest` **177/177** · 42 个 `scripts/test/cep_*.sh` 逐个 rc=0 · `format --check` rc=0 · `[cep]` **527 cases / 11797 assertions** |
| **AC-6** | 诚实登记两项 | ✅ | `tasks.md` 的验证段已逐字写明：① 「当前无任何机器闸钉死 `kResponderMappedBits == 4`」② 「W5 在 W=4 时恒真 ⇒ 今日无增量判别力,为阶段 2 预置」 |
| **AC-7** | 满射判据有牙 | ✅ | 变异实测（`kResponderMappedBits` 4→5 不填表项）⇒ **3 个 test case 红**：`test_cep_sharer_tracker_base.cc:232`（满射 `responder_node_of(4) != kInvalidNode`）· `:237`（`responder_bit_of(kInvalidNode)` = **4 ≠ 64**）· `:238`（`mask(kInvalidNode)` = **16 ≠ 0**）+ 连带 `:243/:247` · `test_cep_sparse_snoop_filter.cc:191` · `test_cep_tier2_observation.cc:291` ⇒ 还原后全绿 |
| **AC-8** | 范围纪律 | ✅ | `test/cep_experiment_harness.hh` · `docs/adr/` · `protocol_instances.json` · `cep_evidence_tiers.json` · `openspec/specs/cep-sharer-tracker-base/spec.md`（**archive 前**）· `experimental/run-*/metrics.json` ⇒ **全零 diff**;新增 `INV-CEP-` = **0**;`SharerTrackerIf` 虚方法 = **5**;`static_assert(sizeof(responder_set_t) == 8)` **仍在** |

## 2 · ⚠️ 实施期的一处范围调整（如实登记）

**openspec 要求 change 至少 1 个 delta**（`openspec validate` 的 ERROR）⇒ 新增 **W7**：`ADDED` Requirement「responder 位宽扩容的机制与触发判据」（6 Scenario）。

⚠️ **这是提升而非扩大**：把 W4 的「注释」升为 **Requirement** ⇒ 机制从「文档约定」变「契约」✅
⚠️ **连带修正**：AC-8 的 `cep-sharer-tracker-base/spec.md` 判据口径由「零 diff」改为「**archive 前**零 diff」（归档时 delta 合并是正常流程）⇒ 已同步 `tasks.md` 与 `proposal.md`。

## 3 · ⚠️ AC-7 的措辞订正（mutant 结果比预期更丰富）

design D-6 记录：变异会让**3 个 test case** 红而非「只有 W5 红」⇒ ⇒ **这组结果实证了 ADR-CEP-21 选项 B 的否决理由**（`kResponderMappedBits` 变宽而表项为 `kInvalidNode` ⇒ `responder_bit_of(kInvalidNode)` 在 bit 4 命中 ⇒ **sentinel 被破坏**）。
⇒ ⛔ 已在 `tasks.md` 的 AC-7 明确写「**⛔ 不写『只有 W5 红』**」⇒ 措辞准确。

## 4 · 遗留

| # | 项 | 分类 | 说明 |
|:--:|---|---|---|
| **L-1** | V1-7（观测面可定位触发节点）**未做** | **已披露**（ADR 允许「判定不补则 2a 降级」） | 待 **architect** 裁定;若判「必须补」⇒ 另立 change |
| **L-2** | W5 今日**无增量判别力** | **已披露**（ADR 反对死判据,故必须写明） | 其价值**只在 `kResponderMappedBits > 4` 时**兑现 |

⚠️ **无本轮引入的未披露遗留**。

## 5 · 关联

change `openspec/changes/cep-responder-bitmap-expansion/`（proposal / design / tasks / specs）· ADR-CEP-21 · 台账 **Entry #423**（立项）/ **#424**（验收回执 + 归档授权）· `test/cep_directory_experiment.hh` · `include/tlm/cep/sharer_tracker.hh` · `test/test_cep_sharer_tracker_base.cc`
