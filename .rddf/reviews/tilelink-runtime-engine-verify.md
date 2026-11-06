# 引擎 change 验收报告 —— `cpptlm-cep-tilelink-runtime-engine`

- **判据来源**：① 用户「继续完成剩余的任务」（= 验收 + 归档）;② `openspec/changes/archive/2026-11-06-cpptlm-cep-tilelink-runtime-engine/proposal.md` 的 AC-1~AC-7 + delta spec `TLE-01`~`TLE-08`;③ 台账 Entry #243（立项）/ #246（实施回执）
- **角色**：verifier · **已用轮次**：1/3 · **日期**：2026-11-06
- **结论**：**AC-1~AC-7 + TLE-08 全部 PASS**（AC-7 的「spec 归档合并」子项已于 2026-11-06 **归档动作闭合**:delta 合并进 `openspec/specs/cep-tilelink-runtime-engine/spec.md`,per 台账 Entry #247）。**0 BLOCKER / 0 SHOULD_FIX。**

## 一、逐条判定（每条附工具证据）

| AC | 判据 | 判定 | 证据 |
|---|---|:--:|---|
| AC-1b | 10 个 `on_*` 类型系统强制全覆盖 | ✅ | 编译通过（`ProtocolEngineBase<2>` 派生类实例化成功 ⇒ 抽象方法全实现）；`nm`/源码面 10 个 `override` |
| AC-1 | 7 零事件意图各带 `mask==0 && basis==Direct` 双断言 | ✅ | `TLE-02` case 断言 `kZeroEventIntents` 逐条 + `zero_event_intents()` 计数；`[cep][tilelink][engine]` 7 cases / **101 assertions** 绿 |
| AC-2 | 零自有事件表副本（lint 4 条 + 毒化行为随表变） | ✅ | `TLE-01`：0 处 `kT3MappingCells` / `event_mask` 字段 / `kT3EventAll` / `std::array<std::uint8_t>`；唯一消费载具调用**恰 1 处**且位于 `consumable_mask_of` 内。**行为面**：影子树改表（pending）⇒ 引擎行为**随表变**（脚本 ②，见 AC-5） |
| AC-3 | L4 事务级 5 流贯通 | ✅ | `TLE-03`：`dispatch_cohif` 5 流 ⇒ Grow×1 + Prune×3、`request` 零事件、域外 `flow_id` 零事件；Null 对照（同一 5 流：真插件 4 事件 vs Null 零事件面） |
| AC-4 | 决定性变异（scratch 拷贝 + 签名钉死） | ✅ | 脚本 ③：影子树删 `TLC-M-02` 行 ⇒ **编译红且 `T3:` 签名命中**；真源 sha256 未变 |
| AC-5 | fail-closed 判定出口 = **引擎发射出口** | ✅ | 脚本 ②：影子树改 `Pending` ⇒ 探针 `events == 0`（同时正控 `observed == 1`）⇒ 非「掩码函数返回值」的重复断言 |
| AC-6 | 全量不回归 + 具名 6 闸 rc=0 | ✅ | `[cep]` **442 cases / 10345 assertions**；`ctest` **154/154**；6 闸：`cep_freeze_check` / `cep_docs_sync_check` / `cep_doc_ref_check` / `cep_decl_alignment_check` / `generate_tilelink_anchor_checklist` / `cep_ualink_inc_reverse_verify` **全 rc=0** |
| AC-7 | 文档收口 + F1 复核 | ✅ **PASS** | 台账 Entry #243~#246 齐（4 条）；`07 号` SU 2026-11-06 · 三在位；F1 两处当前态已就地更新；**「本 spec 归档合并」已完成**（`openspec/specs/cep-tilelink-runtime-engine/spec.md` 已由 delta 合并生成,+1 requirement,per Entry #247） |
| TLE-08 | JSON 装配面（正控 + 反控） | ✅ | `TLE-08`：`instantiateAll` 成功 + `dynamic_cast` 类型判据真 + 换 Null ⇒ 判否 + 注册端口数 = 2 |

## 二、边界与清扫复核

- **引擎形态**：0 处「意图→事件」映射表；仅 1 处 `t3_consumable_event_mask` 调用（choke point 内）；`intent_observed` 为**计数**数组（非掩码副本）；⛔ 无 PE 本地树状态机 / 无源态引用（`TLE-05`）。
- **遗留清扫**：新文件 0 TODO / FIXME / 诊断日志；影子树目录在 `/tmp/opencode`（仓外）；工作区 clean（验收前）。
- **范围纪律**：未触碰 `protocol_instance.schema.json` / `protocol_instances.json` / `perm_upgrades.json` / 23 ABI；未连带推进其余 4 族。
- **B3b 回填**：§6.2 定义行 / 终态行 / 待办指针 = **2/6**，措辞未过度宣称（⛔ 未写「B3b 已达成」）。

## 三、路由建议

1. **AC-7 收尾**：执行归档（delta spec 合并 ⇒ main spec `openspec/specs/cep-tilelink-runtime-engine/`）+ KPI **1/3 → 0/3** 连带同步 + 台账归档回执 Entry → 再复核 AC-7。
2. 无实现类返工项。
