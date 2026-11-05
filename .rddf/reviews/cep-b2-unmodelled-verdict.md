# C1 附裁 —— B2 剩余 4 条分叉的判定（未建模 vs 漏断言）+ 5 条 L2 的处置

**状态**:Accepted(2026-11-05;oracle 1 轮修正 + critic 1 轮 APPROVE_WITH_CONDITIONS)· **日期**:2026-11-05 · **角色**:architect
**对象**:`.rddf/reviews/cep-b2-mapping-audit.md` §7.3 的 4 条 + §7.4 的 5 条 L2
**依据**:`ADR-CEP-13` 决策 5（承诺边界）· `ADR-CEP-09` 决策 6/7 与其 Status Update · `ADR-CEP-12:188`（声明项消费关系）
**审稿**:`oracle` 1 轮（推翻草稿 1 条 + 指出 ⏭ 逃逸口）· `critic` 1 轮（`mergeable: yes` / 0 BLOCKER / SHOULD_FIX + NIT 已处置）

---

## 0. 结论

| invariant | 判定 | 一句话依据 |
|---|---|---|
| `INV-CEP-09-IC2`（仲裁策略不得引入新环） | ⏭ **未建模** + 触发条件 | 仲裁器**未建模** ⇒ 无可断言对象 |
| `INV-CEP-09-IC3`（变更 ⇒ 依赖图重算 + 检查重跑） | ✅ **可在工具链层断言** | `run_channel_gate(final_config)` **每次装配从 config 现算** ⇒ 判定是当前 config 的确定性函数 |
| `INV-CEP-13-IC1`（复合一致性公理） | ⏭ **未落地**（三行如实登记,见 §1.3） | 「**采用**该公理为判据」这一**选择事实**无机器可检声明项 |
| `INV-CEP-13-IC3`（桥接并发粒度由对端 ack 决定） | ⏭ **未落地** + **硬化**触发条件 | schema 13 个顶层属性**无**该声明项 |

⇒ **4 条里 1 条可立即补断言、3 条登记「⏭ 未建模/未落地 + 触发条件」**（⛔ 不硬造断言）。

⚠️ **本则经 oracle 复核推翻了草稿的 1 条**（`INV-CEP-13-IC1` 由「声明侧已闭合」降为 **⏭ 未落地**）—— 与本轮审计推翻 3 条结论**同方向**：把无载体的判成已闭合。故 ⏭ 登记**必须配机器可检载体**（§5），否则 B2 永不可达成。

---

## 1. 逐条依据

### 1.1 `INV-CEP-09-IC2` —— 未建模

- ADR-CEP-09 决策 6 的对象是「**仲裁策略**」；该 ADR 自身 Status Update 记「vc3 内 `E` 不被 `D` 饿死 —— **未建模**」，多 VC 仲裁属 **Stage 4+**。
- ⇒ 当前仓**没有仲裁器** ⇒ 「仲裁策略不得引入新环」**无可断言对象**。
- ⛔ 硬造断言 = 为凑 66/66 造证据（`SU#4` 排除路径 ② 同族）。
- **登记**：`⏭ 未建模`；**触发条件** = 多 VC 仲裁器落地（`ADR-CEP-09` Stage 4+ 批次）。

### 1.2 `INV-CEP-09-IC3` —— ✅ 可在工具链层断言（**关键：不是运行期**）

判据原文：「`ordering_assumption` / `vc_binding` 变更 ⇒ 依赖图**重算** ⇒ 无环性检查**重跑**」。

- 这是**配置期**性质 ⇒ 载体 = 装配期闸。
- 现状：`src/core/module_factory.cc:175-188` 每次 `instantiateAll` 现算 `run_channel_gate(final_config)` + `run_ordering_gate(final_config)`。
- ⛔ 但「天然满足」≠「有载体」：必须有一条**可判别**的断言。
- **可判定形态（措辞经 oracle 修正）**：
  - ① **确定性**：同一 config 连续两次 `run_channel_gate` ⇒ 判定**一致**；
  - ② **闸层重算敏感性**（**有牙主体**；经 critic 收紧）：两个 config **仅差** `ordering_assumption` / `vc_binding` ⇒ 判定**相反**。
    - ⚠️ **两个坑（critic 指出）**：① `validateConfig` 在闸**之前**执行（`module_factory.cc:163` vs `:175`）⇒ delta 若被 schema 层拒（两字段均属 `required`），证明的是 schema 校验**不是**闸重算 ⇒ **两个 config 都必须过 `validateConfig`**，且 **FAIL 必须出自闸层日志**；
    - ⚠️ **环判定的翻转对不可构造**：边来自**声明边表**而非由这两字段推导（`channel_dependency_graph.hh:14`）⇒ 不存在「无环变有环」的 delta ⇒ ⛔ 不得把本断言表述为「环判定重算敏感性」；
    - ✅ **可构造的真翻转对**：`ordering_assumption` 值变化（命中 ordering gate J1，`ordering_point_checker.hh:116-130`）或 `vc_binding` 越界值 —— 两者均可保持 schema 合法；
    - ⚠️ **敏感性断言必须实证 exhibit，不得假定**（先例：`ualink_inc_state_spec.hh:569-735` 有「vc_binding 语义分配恒真」的实测教训）⇒ 载体须登记**具体 config 对**；若构造不出 ⇒ 按 §1.2 精神**回退为 ⏭ 登记**，⛔ 不得硬造。
- ⚠️ **不要求「无缓存」**：以 config 为 key 的**正常缓存**同样满足 ①② ⇒ ⛔ 不得把「实现机制无缓存」写成判据（那是对实现的过度约束）。①+② 实际证明的是「**判定是当前 config 的确定性函数且对 `ordering_assumption` / `vc_binding` 敏感**」—— 这**就是**该 invariant 的真实语义。

### 1.3 `INV-CEP-13-IC1` —— ⏭ **未落地**（草稿「声明侧已闭合」被 oracle 推翻）

- 判据的关键动词是「**采用**」——「跨域组合的判据**采用**复合一致性模型」是一个**选择事实**。
- 实测：`consistency_labels` 承载的是**每实例的序模型标签**（read/write/request_level 的 SC/TSO/RC/PLO 枚举），它是 `ppo_com(t) ≡ ppo_i` 的**输入侧 vocabulary**，⛔ **不是**「采用该公理为判据」的声明。
- `rfe` / `fr` / `ws` 三关系**零声明载体**（只能来自执行观测 = litmus）。
- ⚠️ 草稿引用的 `test_cep_schema.cc:295-304` 只是 **enum 合法值断言** ⇒ 其变红条件是「schema 枚举被改」，与本条语义**不咬合**（C1 规则 B 的「活路径条件与 invariant 语义不咬合」形态）；且它与 **`INV-CEP-12-IC1` 共享** ⇒ ⛔ **不得**计为本条专属有牙证据（否则 66 条内**双计**）。
- `ADR-CEP-12:188` 的「13 消费 `consistency_labels`」是**消费关系**（输入被消费），⛔ **不构成**「判据已转写为结构化声明项」的证据。
- ⇒ **登记（三行如实）**：

  | 运行期验证 | ⏭ litmus 载体未建（已知待办：litmus 证伪判据需**独立 change** 立项） |
  | **承诺锚**（critic 追加） | 「**per ADR-CEP-13 决策 5 ①，承诺未兑现**」—— ⛔ 不得表述为「未来范围 / 已退役」：退役一条承诺须走 supersede 流程 |
  |---|---|
  | 输入侧 vocabulary | `consistency_labels` 有载体（schema + 合法值断言）—— **与 `INV-CEP-12-IC1` 共享，⛔ 不计为本条专属证据** |
  | 判据选择（「采用该公理」） | ⛔ **无机器可检声明项**（schema 13 个顶层属性中无「组合判据」类项） |
  | 运行期验证 | ⏭ litmus 载体未建（已知待办：litmus 证伪判据需**独立 change** 立项） |

- **触发条件** = ADR-CEP-13 决策 5 ① 的**剩余转写**（组合判据成为结构化声明项）+ litmus change 立项。

### 1.4 `INV-CEP-13-IC3` —— 未落地

- 判据：「跨域桥接的并发粒度**由对端协议是否允许提前 ack 决定**，**不由框架固定**」。
- ⇒ 可判定形态需要「**adapter 是否允许提前 ack**」这一**可声明事实**。
- 实测：`protocol_instance.schema.json` 的 13 个顶层属性 = {`id`,`family`,`adapter`,`status`,`protocol_version`,`flow_support`,`perm`,`atomic`,`consistency_labels`,`ordering_assumption`,`vc_binding`,`sharer_tracker`,`combination_capabilities`} ⇒ **无** ack 行为声明项；`combination_capabilities` 三维 = concurrent / hierarchical / heterogeneous（回答「能与什么组合」，⛔ 与 ack 无关）。
- ⇒ 该判据**尚未转写成结构化声明项** ⇒ **未落地**（⛔ 不是「漏断言」—— 没有声明源可断言）。
- **登记**：`⏭ 未落地`；**触发条件（含代价，oracle 硬化）** = 「adapter 提前 ack 行为」成为实例声明项 ⇒ 落地代价 = `protocol_instance.schema.json` **13 → 14 顶层属性** + `INV-CEP-12-IC1`（7 组 / 13 属性，`test_cep_schema.cc:234`）**联动修订** + `ADR-CEP-12` 追加式修订。
  ⚠️ 该代价与 R-2 / `perm_upgrades` 独立声明源先例同族（不是随手能加的字段）⇒ ⛔ **在此之前该判据不得出现在任何 Stage DoD**。

---

## 2. 5 条 L2 的处置（本则只给判据方向，核实由 builder 执行）

| invariant | 可判定形态（建议） | 若核实成立 ⇒ |
|---|---|---|
| `CEN-Ib6`（`forward_data` 对 stub 透明） | 同一 `forward_data` 输入 ⇒ stub 侧**不可分辨**于直接转发 ⇒ 断言 stub 观测到的字节序列**逐字相同** | 补断言（正控 + 改一字节即被分辨 ⇒ 反向） |
| `CEN-Ib8`（`address_resolve` 在 ProtocolEngine 内终结） | 断言该流**不出现在** engine 的 egress 计数里 ⇒ 需 egress 可观测面 | 若无 egress 面 ⇒ 登记「未落地 + 触发条件」 |
| `INV-CEP-08-IC1`（恰在一个 Ordering Point 序列化） | 两条冲突事务 ⇒ 被**恰好一个** OP 序列化 ⇒ 需「被哪个 OP 序列化」的可观测字段 | 若该字段不存在 ⇒ 同上 |
| `INV-CEP-08-IC3`（OP 不绑定物理实体） | OP 的实现类**不得**出现在 `ordering_point_checker.hh` 的物理实体表里 ⇒ 结构断言 | 补断言（结构级） |
| `INV-CEP-14-IC3`（检查项缺失 ⇒ fail，不得静默跳过） | 删掉某检查项 ⇒ 组合检查**必须 fail**（⛔ 不得 SKIP） | 补断言（反向 fixture：删项 ⇒ fail） |

⚠️ **本表是「核实方向」，不是结论** —— 逐条核实前不得写 ✅ 或 ⏭。

---

## 3. 剩余工程量（C1 裁决 §4 的动作 2/3/4）

| # | 动作 | 角色 | 状态 |
|---|---|---|---|
| 1 | 补 `INV-CEP-09-IC3` 断言（确定性 + 变更敏感性） | builder | 判据形态见 §1.2 |
| 2 | 5 条 L2 逐条核实 → 补断言或登记 | builder | 方向见 §2 |
| 3 | 给 16 条 L1 加 `// INV-CARRIER:` 标记 | builder | — |
| 4 | **映射表生成器 + ctest 闸**（规则 C）**并纳入 ⏭ 登记物**（§5） | builder | **尚未开工** —— 这是 B2 达成的硬前提 |
| 5 | `INV-CEP-06-IC3` 的 `INV-CARRIER` 标记 | planner 授权 → builder | ⚠️ 载体在**冻结件** `tools/render_report.py` |
| 6 | 4 条的登记（3 条 ⏭ + 1 条可断言）+ 台账 Entry | planner | 本则 |

⇒ **B2 达成的最后一道硬前提 = 动作 4**（映射表 + 闸 + ⏭ 登记物）。⛔ 映射表未全绿前，**B2 不得登记 ✅**。

---

## 4. oracle 复核的处置记录

| # | oracle 指出 | 处置 |
|---|---|---|
| R1 | `INV-CEP-13-IC1` 的「声明侧已闭合」是**假闭合** —— `consistency_labels` 是输入侧 vocabulary；schema 合法值断言与本条语义**不咬合**且与 `INV-CEP-12-IC1` **共享**（双计风险） | ✅ 已改判为 ⏭，并按「输入侧 / 判据选择 / 运行期」三行如实登记 |
| R2 | `INV-CEP-09-IC3` 的断言形态对，但**理由贴错标签**（① ② 证明的是「确定性 + 变更敏感性」，不是「无缓存」）；② 需 config 仅差该字段且判定相反 + 带 `cep` 段 | ✅ 已改措辞并放行 builder |
| R3 | `INV-CEP-13-IC3` 的触发条件**不够硬**（未写落地代价） | ✅ 已硬化（schema 13→14 + `INV-CEP-12-IC1` 联动 + ADR 追加式修订） |
| R4 | ⏭ 登记**没有机器可检表示形态** ⇒ 闸对 ⏭ 恒红 ⇒ B2 永不可达成；剩下两条出路都坏（硬造断言 / 缩分母 = B7 计数缩水同族） | ✅ 已加 §5：`cep_invariant_deferrals.json` + 三项闸校验 + 登记格式「66/66（N 载体 + M 授权⏭）」，**分母永不可缩** |

---

## 5. ⏭ 登记的**机器可检载体**（C1 规则 C 的追加式补裁 —— 本则新增）

⚠️ **问题**（oracle 指出）：C1 规则 C 的闸是「标记 id 集合 == 66 条集合」；而 ⏭ 条目**没有任何机器可检表示形态**
⇒ 闸对 ⏭ 条目**恒红** ⇒ B2 按「66/66 全绿」口径**永不可达成**；剩下两条出路都坏：
① 给 ⏭ 条目发明载体（硬造断言，已被 `SU#4` 排除路径 ② 排除）；
② **把 ⏭ id 从 66 集合里删掉** —— 这正是 B7「计数缩水」同族漏洞（且比 B7 隐蔽：缩分母要动编号真源 `INV-CEP-NUM-IC1`）。

| **闸新增四项** | ① ⏭ id **⊆** 66 集合；② `reason` / `trigger_condition` / `ledger_entry` **非空**（反向 fixture：置空 ⇒ 红；先例 = `ADR-CEP-12 §13.3` 的 `authority_migrations[]` V6 机制）；③ **有牙载体 id 集合 ∪ 授权⏭ id 集合 == 66 集合**，且**交集为空**；④ **`ledger_entry` 必须解析到真实存在的台账 Entry，且该 Entry 正文含该 id**（critic 追加 —— 台账为 planner 侧产物，与 `authority_migrations[]` 的「台账锚」先例同构） |
| **登记格式** | 「B2 ✅ **66/66**(N 项有牙载体 + M 项授权⏭，映射表全绿，Entry #XXX)」—— ⛔ **66 的分母永不可缩**；且 **N ≥ 1**（全 ⏭ 即 N=0 ⇒ 不得登记为达成，critic 追加） |

| 项 | 内容 |
⇒ 有了本节，⏭ 登记**堵住了「缩分母」与「空占位」两条逃逸口**：它有载体、有反向 fixture、且**不改变分母**。
⚠️ **但「完全不是逃逸口」是过度声称**（critic 指出，已收紧）：**「挪窝」路径仍存在** —— builder 同时删映射表侧的 `INV-CARRIER` 标记 + 在 deferral 登记物补一条理由似真的条目，前三项校验**会全绿**。
⇒ 该路径由**校验 ④ + planner 登记双闸把守**：④ 验证 `ledger_entry` 指向**真实存在且正文含该 id** 的台账 Entry（台账是 planner 侧产物，builder 无法凭空造）；且本批 4 条 ⏭ 已过 oracle 逐条复核。
⚠️ **残余风险**：builder 若能**同时**改 deferral 登记物**与** objective 台账正文，仍可挪窝 ⇒ 缓解 = 台账 append-only + 本登记的 reason 含**承诺锚** + 每次 B2 登记须附 verifier 复核记录。
| **新登记物** | `test/cep_invariant_deferrals.json` —— 逐条 `{id, disposition: "⏭", reason, trigger_condition, ledger_entry}`（与映射表**同由闸校验**） |
| **闸新增三项** | ① ⏭ id **⊆** 66 集合；② `reason` / `trigger_condition` / `ledger_entry` **非空**（反向 fixture：置空 ⇒ 红；先例 = `ADR-CEP-12 §13.3` 的 `authority_migrations[]` V6 机制）；③ **有牙载体 id 集合 ∪ 授权⏭ id 集合 == 66 集合**，且**交集为空** |
| **登记格式** | 「B2 ✅ **66/66**(N 项有牙载体 + M 项授权⏭，映射表全绿，Entry #XXX)」—— ⛔ **66 的分母永不可缩** |
| **验收侧** | B2 最终登记时核验格式 = 「66/66(N + M)」；⛔ **拒收**任何分母 ≠ 66 的达成宣称 |

⇒ 有了本节，⏭ 登记**不是逃逸口**：它有载体、有反向 fixture、且**不改变分母**。

---

## 6. critic 1 轮处置记录（APPROVE_WITH_CONDITIONS · 0 BLOCKER）

| # | 级别 | 内容 | 处置 |
|---|---|---|---|
| S1 | SHOULD_FIX | 「⏭ 登记**不是逃逸口**」过度声称 —— 「挪窝」路径真实存在（删标记 + 补 deferral ⇒ 三项校验全绿） | ✅ 已收紧表述 + **追加闸校验 ④**（`ledger_entry` 须解析到真实台账 Entry 且正文含该 id）+ 残余风险显式登记 |
| S2 | SHOULD_FIX | `INV-CEP-09-IC3` ② 的翻转对有两个坑（schema 层先验 / 环判定翻转不可构造） | ✅ 已收紧：两 config 都须过 `validateConfig` + FAIL 必须出自**闸层**；改名「**闸层重算敏感性**」（⛔ 非环判定重算）；给出可构造的真翻转对；**必须实证 exhibit**，构造不出 ⇒ 回退 ⏭ |
| S3 | SHOULD_FIX | `INV-CEP-13-IC1` 的 ⏭ 应锚定「承诺未兑现」而非仅「未落地」 | ✅ 已加**承诺锚**行 + deferral `reason` 字段须含承诺锚的要求 |
| N1 | NIT | 状态头预填 critic 结论（过程日志与状态耦合） | ✅ 已改为不预填，定稿时回填 |
| N2 | NIT | 登记格式未要求 N ≥ 1 | ✅ 已加 |

---


## 6. 关联

`.rddf/reviews/cep-b2-coverage-verdict.md`（C1 主裁决）· `.rddf/reviews/cep-b2-mapping-audit.md`（盘点 + §7 复核修正）·
台账 **Entry #232**（C1 落表）· `ADR-CEP-09` 决策 6/7 · `ADR-CEP-13` 决策 5 · `ADR-CEP-12 §13.3`（`authority_migrations[]` V6 先例）·
`src/core/module_factory.cc:175-188`（装配期闸）· `test/test_cep_channel_dependency_gate.cc:277`（注入环 ⇒ fail 先例）
