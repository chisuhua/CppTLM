# cpptlm-cep-tilelink-runtime-engine: TileLink-C 运行时引擎 —— B3b 第 2 个实例(CHI RN-F 之后)

> **状态**: 🟡 **Proposed**(2026-11-06 立项 —— change 目录已建;KPI 并发 **1/3 → 2/3**,per 台账 Entry #243)
> **立项依据**:前置全清 —— T3 映射锚点普查**已实施完成**(per 台账 **Entry #242**:11 行 `TLC-M-*` 全条款级 `Direct`、`Pending` = 0、fail-closed 载体、判据 4 件套绿;verifier 复核 `.rddf/reviews/b3b-t3-verify.md` **0 BLOCKER**)⇒ `07 号` SU 2026-11-06 §⑤ 二分支验收命中**分支 1**(全行有锚 ⇒ 引擎 change 合法,⛔ 不再构成推理填空)
> **队列**: 不占 `obj-cep-platform §4.0` 队列 slot(队列 #1~#14 已全归档;本 change 为 **B3b 推进项**,归 `§6.2 B3b` 6 族清单)
> **关联 ADR**: `ADR-CEP-01`(6 层架构)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值锁)· `ADR-CEP-05`(Stub 只暴露稳定态)· `ADR-CEP-07A`(协议私有状态边界)· `ADR-CEP-07`(协议引擎最小内核)· `ADR-CEP-09`(VC 依赖集)· `ADR-CEP-11`(规范版本锁定 `tilelink-c-1.7.1`)· `ADR-CEP-12`(实例声明 + 6 类消息类词表)
> **规划输入**: `07 号` **SU 2026-11-06** §②–§⑦(T1–T4 判定式)· 台账 **Entry #241**(二分支预登记)· **#242**(T3 实施回执)· verifier `.rddf/reviews/b3b-t3-verify.md`
> **冻结面**: **零冻结面改动** —— ⛔ `tilelink_state_spec.hh`(T3 表真源)**本 change 零改动**(引擎只读消费);`protocol_instance.schema.json` / `protocol_instances.json` / `perm_upgrades.json` 零改动;⇒ **不触发** `--update --authorized-by`(⚠️ verifier 报告的 F2 漏勾例外 —— 那次同步仍须走两步授权,见 design.md D-6)
> **估时**: 1 周(引擎新头 + 装配 + L4 E2E + 判据;映射与锚全部现成 ⇒ 无普查成本)

---

## Why

B3b(运行时 ProtocolEngine)当前 **1/6**(仅 CHI RN-F)。TileLink-C 的四要件 **T1✅ T2✅ T3✅ T4✅ 全满足**(per `07 号` SU 2026-11-06 ③;T3 于 Entry #242 闭合)⇒ TileLink-C 是 **B3b 前置最先清零的族**,推进它使 B3b 达成度 **1/6 → 2/6**,剥除终态 **DoD Go** 的最后结构性前置。

三个断点(逐 `file:line` 实测):

| # | 断点 | 证据 |
|---|------|------|
| **D1** | **`tilelink_c` 实例的 CohIf 消息面无协议引擎** | 当前该实例协议侧终点是 `TileLinkCAdapter`(规约面)+ `TreeManager`(行为面),而 CohIf 意图到来时引擎侧由 `CohIfNullProtocolEngine` 承接(zero-emission 语义)⇒ TileLink-C 的「平台消息/意图 → 权限转移」链路**从未运行过**(与 chi_rnf 引擎落地前的 D3 同型,per 队列 #8 proposal) |
| **D2** | **T3 映射已有真源但无消费者** | `kT3MappingCells`(11 行全 `Direct`)+ `t3_consumable_event_mask`(fail-closed)落 `tilelink_state_spec.hh`(Entry #242)后,**仓内 0 个运行时消费者** —— 悬空判据纪律要求「判据必须被消费」,否则普查产物退化成死表 |
| **D3** | **B3b 6 族推进停摆风险** | 其余 4 族(CXL 2.0 `T4❌` 显式冻结 / CXL 3.0、NVLink、UALink 载体无矩阵 `T1❌`未触发)在**架构上**没有推进出口 ⇒ 唯一合法出口 = TileLink-C;不做 ⇒ B3b 停在 1/6,终态 Go 永久阻塞 |

---

## What Changes

1. **新增**引擎新头 `include/tlm/cep/tilelink_protocol_engine.hh`(header-only,模板 = `chi_rnf_protocol_engine.hh` / `mesif_protocol_engine.hh` 先例):
   - 实现 `ProtocolEngineBase` 的 **10 个 `on_*` 虚函数**;
   - **零自有事件表副本**(TMA-01 同款纪律):事件语义一律**委托** `kT3MappingCells` + `t3_consumable_event_mask`(⇒ 未来锚点升级 `Direct`→`Pending` 反向时自动 fail-closed,无需改引擎);
   - `Cap`「源不定」语义按**集合语义**实现(T3-02 行 `potentially` 同款处理,per 锚点摘要 §4);
   - 7 行空掩码意图(`forward_data` / `back_invalidate` / `address_resolve` / `barrier` / `snoop_completion_ack` / `request` 自身 / atomic 意图)⇒ **显式零事件断言**(不得静默丢弃)。
2. **新增**装配:把 `tilelink_c` 实例的 CohIf 引擎绑定从 `CohIfNullProtocolEngine` 切换为 `TileLinkProtocolEngine`(落点 = CohIf 装配链的测试侧 JSON,与 `_json_assembly` 同域;**不改** `protocol_instances.json`);
3. **新增** 1 条 **L4 事务级 E2E**(`submit → tick → 分派`,模板 = `test_cep_chi_rnf_subset.cc` CR-10③ 形态):覆盖 5 个有事件流(request 发起 → completion Grow / snoop Prune / snoop_resp Prune / writeback Prune);
4. **新增**判据(**判据四件套本 change 全落**):编译期锁(消费面路径)+ 正控 + 反向 fixture + **决定性变异**(删 `kT3MappingCells` 任一行被消费 ⇒ E2E **按正确原因红**);
5. **同步**:`07 号` `## Status Update` 追加(引擎落地 = TileLink-C 行 + B3b 达成度更新)· 台账 Entry(obj `§6.2 B3b` 状态与终态回填口径)。

---

## Acceptance Criteria

| # | 判据 | 机器可判证据 |
|---|---|---|
| AC-1b | 10 个 `on_*` 虚函数**类型系统强制全覆盖**(不实现即抽象类不可实例化) | 编译通过即自证(降级为注释级说明,⛔ 不当主判据) |
| AC-1 | **7 空掩码意图**各有显式零事件断言,且**同时**断言 `t3_basis_of == Direct`(消除 Pending 行「合法零事件」真空) | 11-intent 参数化测试:每意图 `consumable_mask_of` 值 + basis 双断言;`AtomicIntent` 无 `on_*` 落点的事实**显式承认**,断言经统一 choke point 覆盖 |
| AC-2 | **零自有事件表副本**:引擎内 0 处事件映射字面量 或 拷贝缓存(机械纪律见 design D-2 ①~④) | 毒化实测(**scratch 拷贝 + 取证 + 复原 + 冻结 hash 一致**):改 `kT3MappingCells` 某行事件 ⇒ 引擎行为**随表变**;grep 级 lint 4 条齐 |
| AC-3 | L4 E2E 5 流贯通(请求由 PE 发起,逐流事件到位) | `[cep][tilelink-engine][e2e]` case 绿 |
| AC-4 | **决定性变异**:变异实施在 **scratch 拷贝**(临时 include 路径),⛔ 不得在真源上直接改;期望失败签名**钉死** = state_spec 层的**编译期 `static_assert`(行宇宙/intent 唯一性锁)**先行红 —— 判断「原因正确」= 失败信号与该签名匹配,⛔ 非 SKIP/非 E2E 静默放行 | 变异取证(build log 含 `static_assert` 文本)+ 复原后全绿 + 冻结 hash 一致 |
| AC-5 | fail-closed:把某行改 `Pending` ⇒ 该意图**零事件发射**(⛔ 判定出口 = **钩住引擎事件发射出口**的零发射断言;只断言 `consumable_mask_of` 返回 0 = **不算**消费面反控,那是 state_spec 层重复) | scratch 拷贝变异 + E2E 段零发射计数断言 + 取证 + 复原 |
| AC-6 | 全量不回归:`[cep]` 全绿 + `ctest` 全绿 + **具名 6 闸 rc=0**:`cep_freeze_check.sh` / `cep_docs_sync_check.sh` / `cep_doc_ref_check.sh` / `cep_decl_alignment_check.sh` / `generate_tilelink_anchor_checklist.sh` / `cep_ualink_inc_reverse_verify.sh` | CI 等价命令(modified registration contract test 一并绿) |
| AC-7 | 文档收口:`07 号` SU + 台账 Entry(#243/#244/#245)+ 本 spec 归档合并 + **F1 两处当前态行已就地更新**(已随立项 Entry #243 收口,届时复核) | 闸 + `openspec validate --strict` + 台账 grep |

---

## Capabilities

**ADDED Requirement: cep-tilelink-runtime-engine**(delta 落 `openspec/changes/<name>/specs/cep-tilelink-runtime-engine/spec.md`;归档时合并成 main spec `openspec/specs/cep-tilelink-runtime-engine/`)

场景前缀 `TLE-01`~`TLE-07`(见 delta spec)。

> ⛔ **不**放进 main spec `cep-tilelink-tree-manager`(那里是规约面 contract;引擎是运行面 capability,分立 spec;两 spec 交叉引用)。

---

## Impact

- **新增**:`include/tlm/cep/tilelink_protocol_engine.hh` · 装配测试 · `test/test_cep_tilelink_engine.cc` · delta spec
- **连带小改**(注册名连锁):`include/chstream_register.hh`(PRE INI 注册第 4 个引擎名,同 `chi_rnf` 先例 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED`)· `test/test_cep_registration_macro_contract.cc:167` 引擎名三元组清单 → **四元组**(若新增第 4 名即 red,前置任务 P0-1 处理)
- **零改动**:`tilelink_state_spec.hh`(T3 表真源只读消费)· `tilelink_c_adapter.hh` · `protocol_instances.json` / `.schema.json` / `perm_upgrades.json` · 23 ABI · canonical 10 流词表 · `CATEGORY_*` · `tilelink_tree_manager.hh`(行为面不变)
- **不涉及** ⏭ 族(CXL 2.0 冻结 / CXL 3.0、NVLink、UALink 未触发)——⛔ 不连带推进
- **KPI**:Proposed 并发 **1/3 → 2/3**(⚠️ 条件句:若 `t3-mapping-survey` 先归档,实际 = **0/3 → 1/3**;实测以 `openspec list --json` 为准)

---

## Out of Scope

- ⛔ **不做** UALink / NVLink / CXL 引擎(缺口在 T1 载体,不在本 change)
- ⛔ **不做** `protocol_instances.json` 状态/字段变更(实例已于队列 #11 `implemented`)
- ⛔ **不做** ABI 新增 / 冻结面 schema 变更
- ⛔ **不做** 性能结论(per `07 号` 口径;引擎 correctness 面为准)
- ⛔ **不改** T3 表真源(锚点升级须走 architect 流程,非本 change)
- ⛔ **不改** `tilelink_tree_manager.hh`(行为面不变;引擎只接 CohIf 意图面)
- ⛔ **不更新**锚点摘要文档(§7 失效条件未被触发)
- ⛔ **不建** PE 本地树状态机(Nothing/Branch/Tip)—— 摘要 §8 遗留 1 的「自决」留白,本 change 正面形态 = 事件面计数/上报器,树状态机明确外置
