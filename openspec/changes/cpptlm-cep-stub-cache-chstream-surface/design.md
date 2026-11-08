# design —— `cpptlm-cep-stub-cache-chstream-surface`

> 本 design 只落地**已签发决策**;⛔ 不新增架构取舍(凡新取舍须回 architect 追加 SU)。

## D-1 形态:组合式 wrapper(非「升格」)

`StubCacheEndpointTLM`(`ChStreamModuleBase` 派生)内部**持有** `StubCacheCore` 实例 ⇒ ⇒ `StubCacheCore` **纯类零 diff**(`INV-SC1`~`INV-SC6` 与 14 个既有单测零变化)。
⛔ **不得**把端口加进 `StubCacheCore` 本体(会破 `ADR-CEP-05` 决策 2 的 cpu_side 接口形态 + 冻结 spec)。

## D-2 映射面归属:wrapper 内部

映射(op 编码 / `evict_line` 缺口 / 同步 `AccessResult` ⇒ 异步 `CacheRespBundle` / net_side 5 流编码)**仅许**存在于 `StubCacheEndpointTLM` 自身 `.hh`/`.cc`。
**红线**:⛔ 新增映射代码禁入 `include/framework/`;⛔ 不得改 `include/tlm/cep/cohif_noc_bridge.hh` 的 `pack`/`unpack`;⛔ 不得另立独立 mapper / 翻译 / 桥接模块。
`evict_line` 缺口 ⇒ **保留 core-side 功能方法 + 装配夹具显式调用**,⛔ 不伪造 `CacheReqBundle` 字段。

## D-3 观测面:wrapper 只暴露原始事实

per-txn issue/complete 周期戳 + in-flight 计数(原始事实)。⛔ 不做 latency 聚合 / 分位统计 —— 聚合归 `test/` 的 harness(`cep_experiment_harness.hh`),**实验装置不得升格进契约面**。

## D-4 home 简化模型:最小应答语义,⛔ 条款级

`CohIfResponderTLM` = **真实 home 的简化模型**。承载:Request→Completion(参数化延迟)· 按读可配发 Snoop · Writeback 接收即消费。⛔ 不承载 credit / retry / ordering point / 多拍聚合 / Perm 规则。
**防误读句(逐字,须进 delta 与台账)**:home 简化模型承载最小应答语义 ⇒ ⛔ 不构成任何家族的 T1–T4 条款级证据 ⇒ `B3b-2` 维持 0/4。
**声明面 = 甲**:⛔ 不进 `protocol_instances.json`(它不是协议实例);契约面 delta 登记**豁免 4 项**。

## D-5 两拓扑 + J-9 分档

- **A**(`configs/cep_chi_rnf_e2e_a.json`):省略 `cep.bridges` 键 ∧ 全部 `connections` `latency = 0` ⇒ 装配闸 `SKIP`(正确语义,非缺陷)。
  ⚠️ **必须省略键**,⛔ 空数组 `[]` 是 `[IAG-01]` FAIL。⚠️ A 的判据 ⛔ 不得含 `unconsumed_bridges().empty()`(`cohif_noc` 是全局注册)。
- **B**(`configs/cep_chi_rnf_e2e_b.json`):恰好 1 条 `cep.bridges` ∧ `modules` 恰好 3 ∧ ≥1 条 `latency > 0`。
  ⚠️ entry **必须**带 `protocol_instance`(已声明实例,`[IAG-02]`),否则 B 永远 FAIL。
  ⚠️ **A 必须零延迟**的理由(**因果序不可颠倒**):主理由 = **判别力**(A 若带 latency ⇒ B 的「互联可达」面退化为 A 的子集 ⇒ B 失独立价值);次理由 = latency 断言纯度。
- **J-9**:A ⇒ 闸 `SKIP` ∧ 无 `cep.bridges` ∧ 无 `latency>0`;B ⇒ 闸 `PASS` ∧ 1/3/≥1 ∧ 三模块桥指针同 `bridge_id`。
  ⚠️ `ChStreamModuleBase` **无 `bridge()` 访问器** ⇒ 两模块须**自存桥指针 + 暴露只读访问器**(⛔ 不动基类)。

## D-6 桥挂载 = B-1(一 entry 三端点)

一个 bridge entry,`modules:[StubCacheEndpointTLM, ChiRnfProtocolEngine, CohIfResponderTLM]`。语义降级 = 「一个 entry 多模块 = 同一 CohIf 投影语境;端点间链路由 `connections[]` 独立表达;⛔ 桥不产连通性」。
⚠️ **跨 entry 模块互斥**(`bridge_` 单槽静默覆盖)⇒ 实施时不得让同一模块出现在两个 entry。

## D-7 可复现:seed + git rev + 工具链

`TrafficGenTLM` 新增 seed / 地址段 / mode setter;config 快照记 **种子 + git rev + 工具链**。
⚠️ `uniform_int_distribution` 跨 stdlib **非位精确** ⇒ 只承诺「**可复跑**」,⛔ 不承诺 bit-identical。

## D-8 台账 7 字段 + 双向闸 + retention 丙

字段见 proposal AC-8;`not-collected` 须带 **reason**;verdict 用 **三态词表**(✓ 通过 / ❌ 违例 fail / ⚪ 不适用)。
**双向闸**:仓内每份已提交 `metrics.json` ↔ 一个台账条目(⛔ orphan),sha256 相等。
**retention = 丙**:只入库 `metrics.json` + `simulation_config.json`;⛔ `trace.h5` / `report.html`;⛔ 历史 127 run 回填。
**边界**:「登记 ≠ 可复算」;schema 漂移不影响 sha256 对账。

## 风险与对策

| 风险 | 对策 |
|---|---|
| 把「链路跑通」读成「协议语义达成」 | AC-9 的逐字声明句 + AC-10 的计数零变化断言 |
| 为凑「13 键全有值」给未测键填值 | AC-7 要求「**各有结论**」(`not-collected` + reason 合法) |
| 映射代码被做成「通用协议桥」复用件 | D-2 红线 + review 闸 |
| 同一模块挂两个 bridge entry ⇒ 桥被静默覆盖 | D-6 互斥纪律;建议加 `[IAG-08]`(W4) |
| 判据「因错误原因红」 | AC-6 要求 J-5 变异**按正确原因**红 |

## 铁律

1. ⛔ 不改 `stub_cache_core.hh` / `cohif_noc_bridge.hh` / `include/framework/` / `protocol_instances.json` / `protocol_instance.schema.json`;
2. ⛔ 不新增 `INV-*`;
3. ⛔ 不改 `B3b-2` 计数(仍 0/4);
4. `:56` MODIFIES **仅此一条**,且须**另立台账 Entry + 两步授权**;
5. 判据落 Catch2(`[cep][e2e]`),⛔ 不落 shell-only 闸。
