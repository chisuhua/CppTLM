# C1 附裁 ② —— 44 条 ⏭-pending 的**分批策略**（B → 批 0/1/2）

**状态**:Accepted(2026-11-05)· **日期**:2026-11-05 · **角色**:architect
**对象**:`test/cep_invariant_deferrals.json` 的 44 条 `⏭-pending`
**依据**:C1 裁决(规则 A/B/C + §5 登记形态)· 映射表闸的**邻域有牙校验实测**(50 条假标记被全部抓红)· 本轮强载体扫描

---

## 0. 结论：分三批，pending **不得长期存在**

| 批 | 内容 | 条数 | 成本 | 完成判据 |
|---|---|---:|---|---|
| **批 0** | **标题/标记规范化**：case 语义**已咬合**该 invariant，但标题用**短式 id** ⇒ 改标题为**完整 id** + 加 `INV-CARRIER` 标记 + **写明「变红理由」** | **~30** | 0.2-0.3h/条 ⇒ **6-9 人时** | 每条能说清「**破坏什么 ⇒ 变红**」 |
| **批 1** | **文本断言 → 语义断言**：现载体只是 `text.find("INV-…-ICn")`（变红条件 = 「ADR 里删掉这条 id」⇒ **不咬合语义**）⇒ 在同 case 内**补一条真语义断言**并把标记迁到它旁边 | **~6** | 0.5-1h/条 ⇒ **3-6 人时** | 语义断言**因语义原因红** |
| **批 2** | **无载体** ⇒ 新建语义断言 | **~8** | 0.5-1h/条 ⇒ **4-8 人时** | 同上 |

⇒ 合计 **13-23 人时 ≈ 2-3 工作日**（**取代** Entry #234 的「3-5.5 工作日」估计 —— 后者假设 44 条全部从零补断言,实测其中约 30 条**已有咬合载体**,只是无法被自动定位）。

---

## 1. 两条**判据**（本则的核心）

### 1.1 「文本存在性断言」**不计入**有牙载体

C1 规则 B 的排除项含「**条件与 invariant 语义不咬合**」（oracle 对 `INV-CEP-13-IC1` 的判定已用过此判据）。
`text.find("INV-CEP-XX-ICn") != npos` 这类断言的**变红条件**是「ADR 里删掉了这条 id」，
⛔ **不咬合** invariant 的**语义**（例如「`INV-CEP-11-IC3`:版本变更须留痕」—— ADR 里留着这句话，
但**没有任何版本变更留痕机制**时,这条断言仍绿）。

⇒ 故：`contracts_crosscheck` / `canonical_invariants` / `collective_plane_contract` 中的一批**文本断言**
**不算**有牙载体 ⇒ 归**批 1**（补语义断言）或保持 pending。
⚠️ **不得**把「文本断言」标上 `INV-CARRIER` 充数 —— 映射表闸的邻域校验**抓不到**这种不咬合,
必须由**人工**按本判据排除。

### 1.2 批 0 的**必要条件**：能说清「**破坏什么 ⇒ 变红**」

每条批 0 转正须在 case 注释里写下**变红理由**（一句）。
⚠️ **说不清 ⇒ 不许转正**（防「借壳」：把一个语义无关的 case 挂到该 id 上,只为让闸变绿）。

---

## 2. 批 0 候选清单（逐条给出目标 id 与咬合理由；**执行前须逐条复核**）

| 载体 | 短式 | 目标 id | 咬合理由（⇒ 破坏什么就变红） |
|---|---|---|---|
| `test_cep_module_factory_seam.cc:131` | `IC1/IC3` | `INV-CEP-15-IC1` / `IC3` | CacheTLM 源文本出现协议标识符 ⇒ 红 |
| `test_cep_module_factory_seam.cc:227` | `IC2` | `INV-CEP-15-IC2` | CEP 侧出现第二个缓存实现 ⇒ 红 |
| `test_cep_module_factory_seam.cc:258` | `IC1` | `INV-CEP-07-IC1` | 出现 `network.mode` 等第二模式维度 ⇒ 红 |
| `test_cep_module_factory_seam.cc:268` | `IC7` | `INV-CEP-07-IC7` | 组合维度接缝被移除 ⇒ 红 |
| `test_cep_module_factory_seam.cc:99` | `IC4` | `INV-CEP-07-IC4` | 注册机制被换成新宏 ⇒ 红 |
| `test_cep_simulation_mode_lock.cc:61` | — | `INV-CEP-06-IC1` | 4 件套缺件/多件 ⇒ 红 |
| `test_cep_simulation_mode_lock.cc:69` | — | `INV-CEP-06-IC2` | 13 必纳字段缺一 ⇒ 红 |
| `test_cep_simulation_mode_lock.cc:98` | — | `INV-CEP-06-IC4` | 不可观测 metric 被写成 0 而非 `not-collected` ⇒ 红 |
| `test_cep_simulation_mode_lock.cc:180` | `IC5` | `INV-CEP-06-IC5` | `memory_violations` 只覆盖单分桶 ⇒ 红 |
| `test_cep_simulation_mode_lock.cc:194` | — | `INV-CEP-07-IC1` | 出现 `network.mode` ⇒ 红 |
| `test_cep_contracts_crosscheck.cc:54` | `IC1` | `INV-CEP-NUM-IC1` | 编号表出现重号 ⇒ 红 |
| `test_cep_contracts_crosscheck.cc:86` | `IC2` | `INV-CEP-NUM-IC2` | 不变量 id 与 ADR 编号命名空间混用 ⇒ 红 |
| `test_cep_contracts_crosscheck.cc:94` | `IC3` | `INV-CEP-16-IC1` | CEP 文档引用 P6 ⇒ 红 |
| `test_cep_contracts_crosscheck.cc:105` | `IC2` | `INV-CEP-11-IC2` | 锁表出现第二份真源 ⇒ 红 |
| `test_cep_contracts_crosscheck.cc:129` | `IC3` | `INV-CEP-11-IC3` | 版本变更不留痕 ⇒ 红 |
| `test_cep_contracts_crosscheck.cc:319` | `IC3` | `INV-CEP-07A-IC3` | `coherence_domains` 被删除 ⇒ 红 |
| `test_cep_schema.cc:480` | — | `INV-CEP-11-IC1` | `protocol_version` 越出锁表 ⇒ 红 |
| `test_cep_schema.cc:703` | `IC3` | `INV-CEP-12-IC3` | `status` 非 implemented 仍计入 adapter 计数 ⇒ 红 |
| `test_cep_schema.cc:307/615/637` | `IC2` | `INV-CEP-12-IC2` | `ordering_assumption`/`vc_binding` 非 per-network 嵌套 / `vc_id` 越界 ⇒ 红 |
| `test_cep_schema.cc:735` | `IC3` | `INV-CEP-07-IC3` | adapter 实例数与 07 号表格行数不一致 ⇒ 红 |
| `test_cep_ordering_point_checker.cc:248` | `IC2` | `INV-CEP-08-IC2` | E 态静默升级检查项缺失 ⇒ 红 |
| `test_cep_tools_contract.cc:137` | `IC3` | `INV-CEP-06-IC3` | 闸不生效（违例类 `not-collected` 却渲染通过）⇒ 红 |
| `test_cep_dummy_signals.cc:104` | — | `INV-CEP-14-IC1` / `IC2` | C1~C10 清单缺项 ⇒ 红 |
| `test_cep_sharer_tracker_base.cc:431` | `IC2` | `INV-CEP-04-IC2` | 同参 `lookup` 返回不同 ⇒ 红 |
| `test_cep_contracts_crosscheck.cc:142/157/170/184` | `IC1~IC3` | `INV-CEP-07A-IC1~IC3` | ⚠️ **逐条复核**（同文件多个短式,易错挂） |

---

## 3. ⏭-pending 的**存续规则**(堵「长期挂账」)

1. 每条 pending **必须**有:`owner` + 所属批次 + 完成判据 ⇒ ⛔ 无主 pending 视为**登记无效**;
2. 批 0/1/2 执行完毕后**仍无**语义断言者 ⇒ **逐条转 ⏭-unmodelled**(须含**承诺锚** + 触发条件),⛔ 不得继续挂 pending;
3. `⏭-pending` 计数**只减不增** ⇒ 新发现的缺口必须**同批**消化(⛔ 不得「先挂 pending,以后再说」);
4. **B2 达成宣称**仍要求 `⏭-pending == 0`(映射表闸 `--for-achievement` 模式)⇒ pending 清零前 **B2 未达成**。

---

## 4. 关联

`.rddf/reviews/cep-b2-coverage-verdict.md`(C1 主裁决)· `.rddf/reviews/cep-b2-unmodelled-verdict.md`(4 条判定 + ⏭ 登记形态)·
`.rddf/reviews/cep-b2-mapping-audit.md`(盘点 + §7 复核修正)· 台账 **#232 / #233 / #234** · `scripts/test/cep_invariant_map_check.py`
