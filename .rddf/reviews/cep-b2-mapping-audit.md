# B2 映射审计（动作 1）—— 37 条候选（29 none + 8 weak）的逐条载体判定

**日期**:2026-11-05 · **角色**:builder · **性质**:纯只读审计（grep 只作**线索生成**，⛔ 不作判定依据）
**依据**:`.rddf/reviews/cep-b2-coverage-verdict.md`（C1 裁决 · 规则 A/B/C）· `.rddf/reviews/cep-b2-invariant-coverage-inventory.md`（盘点输入）
**关联**:台账 **Entry #232**

---

## 0. 结论

| 层 | 数量 | 含义 | 后续 |
|---|---:|---|---|
| **L1 确认有载体** | **16** | 候选 TEST_CASE 与 invariant 语义**强对应**（标题含关键术语 + 文件属该域） | 只需补 `// INV-CARRIER:` 标记（规则 C） |
| **L2 疑似有载体** | **5** | 关键词命中但语义待确认（可能对应到别的流 / 别的不变量） | 人工确认后归 L1 或 L3 |
| **L3 确认无载体** | **11** | 全仓无对应语义断言 ⇒ **真缺口** | 须补断言（正控 + 反控） |
| 合计 | 37 | = 29 none + 8 weak | |

**缺口收敛**：盘点给的是「19–28 条」，本审计收敛到 **11 条确认 + 5 条待确认** ⇒ 补断言量级 **11–16 条**（低于早估）。

---

## 1. L1 确认有载体（16 条 —— 只需补 `INV-CARRIER` 标记）

| # | invariant | 载体 | 判定依据 |
|---|---|---|---|
| 1 | `CEN-Ib2` | `test_cep_stub_cache_core.cc:308`「INV-SC4 completion 与 request 严格配对(无 unsolicited)」 | 定义即「`completion` 不携带 unsolicited 数据(per INV-SC4)」⇒ 同源 |
| 2 | `CEN-Ib5` | 同上文件 `INV-SC2 evict 不选取 outstanding line` | 定义「`writeback` 不与 `request` 竞态(per INV-SC2 evict 互斥)」 |
| 3 | `INV-CEP-04-IC2` | `test_cep_sharer_tracker_base.cc:378`「同地址并发 lookup 返回 STALL」+ `:291`「FullMapDirectory lookup 语义」 | 定义 = `lookup`/`snoop_*` **幂等** |
| 4 | `INV-CEP-04-IC7` | `test_cep_schema.cc:265`「flow_support 恰好 10 项且与 canonical 10 流逐名对齐」 | SU 撤回了「由 flow_support 派生」的规则 ⇒ 现约束是「恰好 10 项 + 逐名对齐」 |
| 5 | `INV-CEP-06-IC4` | `test_cep_simulation_mode_lock.cc:98`「不可观测 metric 的 not-collected 契约存在」+ `:134`「not-collected 必须配 per-metric 原因(反向 fixture)」 | 定义逐字对应，含反向 fixture |
| 6 | `INV-CEP-06-IC5` | `test_cep_simulation_mode_lock.cc:180`「memory_violations 须覆盖 SWMR + data-value 双不变量(IC5)」+ H3 两条例题反控 | 定义逐字对应 |
| 7 | `INV-CEP-07-IC5` | `test_cep_module_factory_seam.cc:115`「4 个 frozen headers 零 diff」+ `:107`「23 ABI 零 diff」 | 定义 =「不破 23 ABI / frozen headers / `[[deprecated]]`」 |
| 8 | `INV-CEP-09-IC1` | `test_cep_channel_dependency_gate.cc:277`「注入环 ⇒ 配置 fail」（**真 `instantiateAll` 驱动**） | 装配期无环 = 该不变量 |
| 9 | `INV-CEP-11-IC1` | `test_cep_schema.cc:480`「protocol_version 值域必须容纳 ADR-CEP-11 锁定表全部取值」 | 定义 = 取值须落 §3 锁定表 |
| 10 | `INV-CEP-11-IC2` | 同上（§3 为唯一真源） | 同源 |
| 11 | `INV-CEP-15-IC1` | `test_cep_module_factory_seam.cc:131`「CacheTLM 零协议知识(per ADR-CEP-15 IC1/IC3)」+ 同文件 frozen 检查 | 定义 = `cache_tlm.*` 0 diff |
| 12 | `INV-CEP-15-IC2` | `test_cep_module_factory_seam.cc:227`「CEP 侧只有一个缓存实现(per ADR-CEP-15 IC2)」 | 定义逐字对应 |
| 13 | `INV-CEP-15-IC3` | 同 11（标题含「IC1/IC3」） | 定义 = 不得实现 canonical 10 流 |
| 14 | `INV-CEP-16-IC1` | `scripts/test/cep_docs_sync_check.sh` 的「[3/12] 06 §15.4 P 编号口径(禁 P6)」 | 定义 = P 编号不得出 P6 |
| 15 | `INV-CEP-NUM-IC1` | `test/test_cep_canonical_invariants.cc` 的「编号预分配表 = 单一真源」检查（owner 映射 + 无孤儿） | 定义 = NUMBER-MAP §2 是编号单一真源 |
| 16 | `INV-CEP-NUM-IC2` | `test_cep_contracts_crosscheck.cc:86`「不变量 id 命名空间与 ADR 编号分离(IC2)」 | 定义逐字对应 |

---

## 2. L2 疑似有载体（5 条 —— 人工确认后归 L1 / L3）

| # | invariant | 定义 | 候选 | 待确认点 |
|---|---|---|---|---|
| 1 | `CEN-Ib6` | `forward_data` 对 stub **透明**（`ADR-CEP-05 §2` 折叠映射表不可分辨性，契约测试 1） | `test_cep_chi_rnf_subset.cc` / `test_cep_chstream_bidirectional_macro.cc` / `test_cep_collective_plane_contract.cc` 均含 `forward_data` | 是「存在该流」还是「对 stub **透明**」？后者需可分辨性断言 |
| 2 | `CEN-Ib8` | `address_resolve` 在 **ProtocolEngine 内终结** | 4 个文件含 `address_resolve` | 「终结」= 不再外发 ⇒ 需 egress 计数/无外发断言 |
| 3 | `INV-CEP-08-IC1` | 任意两条冲突事务**恰在一个** Ordering Point 序列化 | `[cep][ordering-point]` 的 J1/J2/J3 | 「恰在一个」需可判定断言（J1 是静态路径覆盖，未必等价） |
| 4 | `INV-CEP-08-IC3` | Ordering Point **不绑定**任何物理实体 | 同上 | 「不绑定」需结构断言（是否有？） |
| 5 | `INV-CEP-14-IC3` | 组合接缝检查项**缺失**时，组合检查**必须判 fail**（不得静默跳过） | `test_cep_tools_contract.cc:174` / `test_cep_schema.cc:637` / `test_cep_experiment_harness.cc:201` | 候选都是「非法输入 ⇒ fail」，但「**检查项缺失**」这一特定形态是否有 fixture？ |

---

## 3. L3 确认无载体（11 条 —— 真缺口，须补断言）

| # | invariant | 定义 | 实测 |
|---|---|---|---|
| 1 | `CEN-Ib3` | `snoop` **必有** `snoop_resp` | `test_cep_cohif_carrier.cc` 仅 `CC-01` flow_id 域检查；**无配对断言** |
| 2 | `CEN-Ib4` | `snoop_resp` **必须含 `txn_id`** 用于 ack 配对 | 同上，无 |
| 3 | `CEN-Ib9` | `barrier` 流**不**携带 `group_id` / `participant_count` / `epoch_id` | 全仓 `group_id` 在该文件**零命中** |
| 4 | `CEN-Ib10` | `snoop_completion_ack` 配对 `snoop_resp`、**不**配对 `completion` | 候选仅泛泛 flow_id / INV-SC4 配对（针对 request/completion），**非本条语义** |
| 5 | `INV-CEP-04-IC3` | 5 个事件名 + **时序契约**在 5 种实现中一致 | 无候选 |
| 6 | `INV-CEP-04-IC6` | 5 种实现 **hazard 行为契约**（`NONE`/`STALL`/`CHAIN`）字面值收敛 | 无候选 |
| 7 | `INV-CEP-06-IC3` | 「应 0」语义**随 `verification_mode` 变化**，须在报告中一并呈现 | 无候选（`[cep][simulation-mode]` 有 verification_mode 四档，但非本条） |
| 8 | `INV-CEP-09-IC2` | **仲裁策略与依赖图一致** | 无候选 |
| 9 | `INV-CEP-09-IC3` | **依赖传播契约** | 无候选 |
| 10 | `INV-CEP-13-IC1` | 跨域组合采用**复合一致性模型** `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` | 无候选 |
| 11 | `INV-CEP-13-IC3` | 跨域桥接**并发粒度**由对端是否允许提前 ack 决定 | 无候选 |

---

## 4. 缺口特征（影响补断言的难度）

| 类 | 条目 | 补法 |
|---|---|---|
| **协议流配对/字段**（CEN-Ib3/4/10、CEN-Ib9） | 4 条 | 在 `test_cep_cohif_carrier.cc` 扩断言：流配对矩阵 + 字段域检查（正控：合法流通过；反控：缺配对/带禁止字段 ⇒ 必须被拒） |
| **多实现一致性**（INV-CEP-04-IC3/IC6） | 2 条 | 5 个 tracker 实现的**参数化用例**（同一组事件 → 同一 hazard 值），正控 + 反控 |
| **语义随模式变化**（INV-CEP-06-IC3） | 1 条 | 报告层断言：不同 `verification_mode` 下「应 0」的呈现不同 |
| **仲裁/依赖图**（INV-CEP-09-IC2/IC3） | 2 条 | 需先确认**是否已建模**（若未建模 ⇒ 不是「补断言」而是「登记未建模」，须 architect 裁） |
| **跨域一致性**（INV-CEP-13-IC1/IC3） | 2 条 | 同上：复合公理 `acyclic(...)` 是否需要运行时建模 |

⚠️ **关键分叉**：末两类（4 条）可能**不是「漏了断言」而是「能力未建模」**。若是后者，按 `B3b` 先例应登记
**⏭ 未建模 + 触发条件**，⛔ 不得硬造断言（那是「为凑上限造证据」）。**未核实**（需 architect 判）。

---

## 5. 精确缺口清单（供下一步）

```
须补断言（确认无载体、且模型已存在）  : CEN-Ib3, CEN-Ib4, CEN-Ib9, CEN-Ib10,
                                       INV-CEP-04-IC3, INV-CEP-04-IC6, INV-CEP-06-IC3
待确认是「漏断言」还是「未建模」      : INV-CEP-09-IC2, INV-CEP-09-IC3,
                                       INV-CEP-13-IC1, INV-CEP-13-IC3
待人工确认载体（可能只需加标记）      : CEN-Ib6, CEN-Ib8, INV-CEP-08-IC1,
                                       INV-CEP-08-IC3, INV-CEP-14-IC3
只需加 INV-CARRIER 标记               : 16 条（§1 表）
```

⇒ **补断言量级 = 7 确认 + 5 待确认 = 7–12 条**；另 4 条可能转「未建模登记」。

---

## 6. 本审计的边界（⛔ 必须随引用重复)

1. **关键词级初筛**，不是规则 B 的「有牙」确认 —— L1 的 16 条仍需按 `INV-CARRIER` 标记 + 邻域断言宏校验；
2. **grep 假阴性仍在**（`INV-CEP-15-IC1` 的教训）⇒ L3 的「无载体」结论须在补断言前**人工再核一次**；
3. **未判定「是否已建模」** —— §4 末两类的分叉需 architect 裁；
4. 本审计**不改任何代码/表**，仅产出清单。
