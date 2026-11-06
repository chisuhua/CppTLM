# cpptlm-cep-tilelink-runtime-engine · alpha design

> per 台账 Entry #243 立项 · 2026-11-06 · 角色: planner

## D-1 引擎载体形态:header-only 新头 vs 复用既有引擎容器

**选项对比**,A 采纳:

| 选项 | 结论 | 理由 |
|---|---|---|
| **A. header-only 新头 `tilelink_protocol_engine.hh`,走 `ChiRnfProtocolEngine` 同款注册** | ✅ 采纳 | 3 先例同型全 header-only;❗**注册面实测**:`chi_rnf` / `mesif` 引擎都走了 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED`(`chstream_register.hh:128-142`)注册进 ObjectRegistry,JSON 才能逐实例选引擎(先例 = `test_cep_chi_rnf_subset.cc:457` 的 `"type"` 覆盖)⇒ 纪律是「**引擎注册可被 JSON 选择,但引擎本体不解析 JSON、不注册 ABI**」;⚠️ 本 change 产出同时是 **B3b 其余 4 族未来引擎 change 的模板**(引擎头 + 装配 + E2E + 判据四件套的结构),⛔ 实施时不得 TileLink 特例化(如把 5 流子集写死成引擎常量) |
| B._Null 引擎开 TS 特化参数_. | ❌ | `CohIfNullProtocolEngine` 的 zero-emission 是语义(测试占位),特化会把「没引擎」和「有引擎」混进一个头,违背 Null 语义的测试独立性 |
| C. 通用引擎参数化收编 6 族 | ❌ | 违背「协议特有处理在派生类 `on_*`」的基类设计(`protocol_engine_base.hh:224`);B3b 达成度将无法逐族点名(达成 = 每族可点验的引擎载体 + E2E) |

## D-1b 注册名连锁(**必要修改**,per metis)

- `chstream_register.hh` 注册第 4 个引擎名 ⇒ **`test_cep_registration_macro_contract.cc:167` 的引擎名三元组清单断言大概率红** ⇒ 列入 **P0 前置任务**(与引擎名引入同步改四元组),否则 AC-6「全量不回归」与该修改的因果关系断裂(违背 6 铁律之 3 的混乱 —— 「我没改它为什么红」)。

## D-2 事件语义消费面:**零自有副本,委托 T3 表**(统一消费载具,per metis)

引擎提供**唯一** protected choke point:

```
protected:
    constexpr std::uint8_t consumable_mask_of(PlatformIntent i) const noexcept {
        return tlm::cep::TileLinkStateSpec::t3_consumable_event_mask(i);
    }
```

⛔ **机械纪律(grep/AST 级,实施验收里逐条)**:引擎源文件
① 0 处 `kT3MappingCells` 直接引用(⛔ 唯一合法经 `t3_*` 访问器家族) ② 0 处 `event_mask` 成员访问
③ 0 处 `std::array<...,kPlatformIntentCount>` 类型的成员缓存(构造期拷贝 = **变相同有副本**,行为等价,纪律被架空)
④ `on_*` 全部经 `consumable_mask_of` 消费。

引擎内 **禁止**出现任何「意图 → 事件」的字面映射副本。全部经 `t3_consumable_event_mask(PlatformIntent)`。
后果:①锚点升级(未来 D2(1.9.3)消息表增补)时只改表,引擎不动;②fail-closed 自动传导(Pending ⇒ 掩码 0,引擎不消费)。
反件:AC-2 毒化实测 —— 只改表不改引擎,行为必须随表变。

## D-3 `Cap`「源不定」的实现语义

TileLink `Cap` 三条转移源态不定(D1 §8.1.4);T3 表里**没有** Cap 行(11 行是「意图→事件」,事件类别单值)
⇒ 引擎**摘要级**语义 = 一个事件类别位;「源态待写死」**显式留空**并做**显式测试**(TLE-05):
断言引擎对 `completion` 的事件**不预设源态**(只声明 Grow,不声明 from-state),违规 = 暗中携带源态语义。
⚠️ 这是 Entry #241 ③ 分支 1 预登记的「`Cap` 源不定语义显式测试」;⛔ 不是迁移引擎里实现 12 格矩阵(那是**规约层**真源,device 层只读)。

**引擎正面形态(缩小 §8 遗留 1 的「自决」留白)**:引擎 = **CohIf 意图面 → 事件面的事件计数/上报器**
—— 每个 `on_*` 收意图 ⇒ 经 choke point 取掩码 ⇒ 按位计数/上报事件类别,并**保留** `hit/line_state/dirty` 原始参数供上层(机挑不落地)。**不**建 PE 本地 3 态状态机(Nothing/Branch/Tip)——本 change 是「事件面贯到,其 owner 可选择把 12 格矩阵当只读规约查阅」;树状态机是**另一个原型范围**(需要在 anchors 的 §8 遗留 1 处登记决策),本 change **不做**。

## D-4 装配落点:测试侧 JSON(不改 `protocol_instances.json`)

`tilelink_c` 的 CohIf 意图入口 = CohIf 装配链;装配切换在**测试侧**完成(装配链的 `_json_assembly` 同域配置,
引擎替换 Null),`protocol_instances.json` **零字段变更**(实例在队列 #11 已 `implemented`,不需要状态级变化)。
后果:0 冻结面、0 schema;装配覆盖面收敛在可自动化验收的测试域。

## D-5 决定性变异载体:T3 表(不是引擎内部)

变异 = 删 `kT3MappingCells` 中被 E2E 消费的行(如 `TLC-M-02` completion 行)⇒ E2E 编译/断言**红**,
原因 = 消费面缺行(行宇宙锁 / 掩码断言),而**非** test case 随手 SKIP。
证据形态:取证后复原 + 全绿(per 判据铁律 1.1)。

## D-6 冻结面:引擎本身 0 授权;例外 = verifier 报告 F2 的**历史漏勾**回填

本 change **新增**文件全都不在 M1 冻结集 ⇒ 引擎落地这个动作本身不触发 `--update`。
**例外**:verifier `.rddf/reviews/b3b-t3-verify.md` F2 —— `07 号` SU ⑦ 的 4 处漏勾(L1/L2/L3/L9),
内容已实证完成但框未勾。**回填处理**:随**实施阶段头部 builder 动作**(先于引擎改动)一次性:
1. 台账 Entry #245 先落(漏勾授权;编号顺延:#244 = T3 普查归档回执);
2. `07 号` ⑦ 补勾 + (Entry #242 时点后) 4 框日期;
3. `cep_freeze_check.sh --update --authorized-by "Entry #245"`;
4. 清单单独 commit。
后果:一次 freeze churn 一次授权;后续引擎 diff 就干净了(到时 0 产物换 hash)。

## D-6b F1(台账当前态漂移)的收口路径

verifier 报告 F1(2 处当前态:「剩余 = T3 映射锚点普查」)随**本立项**(Entry #243)**立即就地收口**
—— 立项本身让「剩余」从 T3 普查前移为「引擎 change」⇒ 在立项 commit 内同步:
① `obj-cep-platform.md` L2455「终态」行 + L2457「待办指针」就地更新 + dated 句;
② 豁免面(per verifier 判定):Entry #240 块内句 + `.rddf/reviews/cep-b3a-b3b-ruling.md` L138 **不修**;
③ AC-7 加子项核验。

## D-7 KPI 与台账

- KPI:立项时 **1/3 → 2/3**(同步 7 文件 + AGENTS.md,per b3e08440 模式;⚠️ 条件句:若 t3 change 先归档,KPI 实际 = **0/3 → 1/3**)。t3 change 归档时机由用户裁决,本 change 不依赖它。
- 台账 Entry:#243(本立项 + verifier 报告指针 + F1 收口)· **#244**(T3 普查归档回执)· 实施头部 **#245**(F2 漏勾授权)· 实施回执/归档 **#246**(B3b 达成度 2/6 回填)。

## 风险与遗留

| # | 风险 | 缓解 |
|---|---|---|
| R1 | T3 表未来锚点变更(§7 失效条件) | fail-closed + AC-2 自动传导;引擎无需重写 |
| R2 | verifier F2 的 07 漏勾不在本 change 语义内 | D-6 收口路径显式化,⛔ 不得静默搁置(遗留清扫纪律) |
| R3 | L4 E2E 与 chi_rnf 先例不对称(通道数不同) | 模板对齐到 TileLink 需要的 5 流子集;多余断言不加(范围纪律) |

