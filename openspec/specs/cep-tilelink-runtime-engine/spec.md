# cep-tilelink-runtime-engine Specification

## Purpose
定义 **TileLink-C 运行时引擎**(`TileLinkProtocolEngine`)在 CEP 平台上的**运行面可执行契约**:
CohIf 意图面 → 权限转移事件面(`Cap` / `Grow` / `Prune` / `Report`)的**事件计数/上报**语义 ——
事件真源**零自有副本**(全部委托 T3 表 `kT3MappingCells`,唯一消费载具 `consumable_mask_of`)、
**fail-closed** 消费(未锚行零发射)、**11 意图覆盖**(canonical 10 流 + atomic 意图)、
L4 事务级 5 流贯通与 JSON 装配面。

**上游真源(只读)**:`cep-tilelink-state-spec`(规约层载体 —— 稳定态 / 事件词表 / 转移矩阵 / 逐格 basis)·
`cep-tilelink-tree-manager`(适配器 + 行为面契约)。

> **边界**:不改 `perm` 4 值锁 · 不改 canonical 10 流词表 · 不改 `protocol_instance.schema.json` ·
> **不建** PE 本地树状态机(Nothing/Branch/Tip —— 规约层真源,运行面只读)·
> **不携带源态**(TileLink `Cap`「源不定」⇒ 只声明事件类别)· **不连带推进**其余协议族
> (CXL 2.0 显式冻结 / CXL 3.0 / NVLink / UALink 的 T1 载体未触发)。
>
> **来源**:change `cpptlm-cep-tilelink-runtime-engine`(2026-11-06 已完成归档,per 台账 **Entry #243 / #246 / #247**;
> 验收 `.rddf/reviews/tilelink-runtime-engine-verify.md` AC-1~AC-7 全 PASS)。

## Requirements

### Requirement: cep-tilelink-runtime-engine

`tilelink_c` 实例的 CohIf 消息/意图面 SHALL 由**真插件**引擎(`TileLinkProtocolEngine`,header-only)驱动,且引擎 SHALL **零自有事件表副本** —— 全部事件语义 SHALL 委托 `kT3MappingCells` / `t3_consumable_event_mask`(T3 真源,per `cep-tilelink-state-spec` main spec + `cep-tilelink-tree-manager` main spec 的 TMA-01 同型纪律;锚点审计面 = `docs/research/2026-11-06-cep-tilelink-t3-mapping-anchor-summary.md`)。

**已知边界(per 07 号性能禁令 + 职责分工)**:引擎**不**覆盖「跨 API 事务并发乱序 / credit 压测」(独立 perf 范畴);metrics/统计收集走 `metrics` 面,⛔ 不在本 capability。**范围之外的四族**(CXL 2.0 冻结 / CXL 3.0 / NVLink / UALink 未触发)SHALL NOT 因本 capability 被连带推进。

#### Scenario: TLE-01 —— 引擎载体 SHALL 为 header-only 且零自有事件副本

> 引擎 = `include/tlm/cep/tilelink_protocol_engine.hh`;
> grep 引擎源文件 **0 处**意图→事件字面映射;
> AC-2 毒化实测 = 只改 `kT3MappingCells` 某行事件值 ⇒ 引擎行为**随表变**。

#### Scenario: TLE-02 —— 10 个 canonical 流 SHALL **各有**显式 `on_*` 实现

> 10 虚函数覆盖 = **类型系统强制**(抽象类);7 空掩码意图各带**显式零事件断言**,
> 形态统一 = `conj(mask == 0, t3_basis_of(intent) == Direct)`(消除 Pending 行「合法零事件」真空);
> 断言经**统一 choke point** `consumable_mask_of`(`ProtocolEngineBase` 的 5 条 CohIf 腿内流由
> E2E 覆盖,5 条非腿内流 + atomic 意图 = **编程式直调** `on_*` / choke point 断言,调用载具**显式**);
> `AtomicIntent` 无 `on_*` 落点(**该事实显式承认**,⛔ 不伪造钩子、⛔ 不动基类);
> ⛔ 不得静默丢弃。

#### Scenario: TLE-03 —— 5 个有事件流 SHALL 贯通 L4(事务级)

> request 由 PE 发起 → completion Grow / snoop Prune / snoop_resp Prune / writeback Prune
> 逐流事件到位(E2E case 形态 = CR-10③ 模板,事件轴 = TileLink 5 流子集);
> ⛔ 不得为凑覆盖数补 CXL/UALink 语义断言(per `07 号` ⏭ 纪律)。

#### Scenario: TLE-04 —— **决定性变异**(删被消费映射行 ⇒ 红且原因正确)

> 变异实施在 **scratch 拷贝**(⛔ 不得动真源):删被 E2E 消费的行(如 `TLC-M-02`)⇒
> **期望失败签名钉死** = state_spec 层编译期 `static_assert`(行宇宙 / intent 唯一性锁)先行红,
> 「原因正确」= 失败信号与该签名匹配,⛔ 非 SKIP/非 E2E 静默放行;取证后须复原(真源 hash 一致)。
> per 判据铁律 1.1(判据 4 件套)。

#### Scenario: TLE-05 —— `Cap`「源不定」SHALL 集合语义实现,并 SHALL **有显式测试**

> 引擎事件面 SHALL 只声明**事件类别**(单值),SHALL NOT 预设 from-state(源态不定是 TileLink `Cap`
> 的规范事实,D1 §8.1.4;per 锚点摘要 §2.3bis「3 条通配转移」);
> **机械载体**:引擎源文件 grep **0 处** `TileLinkStateSpec::State` / transition matrix / `transition_cell*`
> 引用(⛔ 引擎不得把源态语义复制进自己), + completion 路径输出面**只有事件类别位**的正控断言;
> per Entry #241 ③ 二分支 1 预登记(`Cap` 源不定语义显式测试)。

#### Scenario: TLE-06 —— fail-closed SHALL 在消费面复验

> 变异实施在 **scratch 拷贝**(⛔ 不得动真源):把某行改 `Pending` ⇒
> **引擎事件发射出口**计数 = 0(判定出口 = 引擎行为级;⛔ 只断言 `consumable_mask_of` 返回值**不算**消费面反控,那是 state_spec 层重复;
> 引擎 SHALL NOT 自带 fail-closed 副本);取证 + 复原(真源 hash 一致)。

#### Scenario: TLE-08 —— JSON 装配面 SHALL 可选引擎类型 + 类型判别

> `chstream_register.hh` 注册第 4 个引擎名 `TileLinkProtocolEngine`(N=2,与 `CohIfProtocolEngine` 同款)
> ⇒ JSON 侧可逐实例选择;⛔ 引擎**不解析 JSON**(`vc_binding` 由装配方 `set_route(...)` 注入);
> 最小装配拓扑 = `configs/cep_tilelink_pe_minimal.json`(2 × 引擎 + `cohif_noc` 桥绑定 `tilelink_c` 实例);
> 判据 = 装配**成功** + `dynamic_cast` 类型判别为真 + **反控**(同拓扑换 Null ⇒ 类型判据**判否**)
> —— ⛔ 不得以「装配失败」蒙对判据。

#### Scenario: TLE-07 —— 冻结面 SHALL 零改动(Schema/instances/ABI)

> `protocol_instance.schema.json` · `protocol_instances.json` · `perm_upgrades.json` ·
> `include/abi/**` · 23 ABI 全零改动;`INV-CEP-*` 不新增编号。
> per proposal Impact + `07 号` 冻结纪律。
