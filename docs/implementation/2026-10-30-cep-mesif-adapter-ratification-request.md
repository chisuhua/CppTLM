# MESIF 实现层跨仓追认请求清单(B-1~B-5 / E-1~E-6 / A-1)

> **来源**:CppTLM 仓队列 #9 `cpptlm-cep-mesif-adapter`(实现层,已归档 2026-10-30)
> **规范性载体**:`include/tlm/cep/sharer_tracker.hh`(B-1 ~ B-5)+ `include/tlm/cep/mesif_protocol_engine.hh`(E-1 ~ E-6)—— 二者是**唯一真源**
> **生成方式**:`scripts/test/generate_mesif_ratification_request.sh`(入口)+ `scripts/test/cep_mesif_ratification_probe.cc`(生成器)—— 本清单**由载体生成**
> ⇒ 载体变更 ⇒ 跑一次生成器;一致性由 `test_cep_mesif_ratification_request.cc` 逐条核对
> **日期**:2026-10-30 · **跨仓结论载体**:待 ArchForge 侧出具(先例:`42 号` 逐格追认结论表)
> **本仓处置**:3 项跨仓债的**本仓侧**部分已全部完成(`basis` 登记 + `pending` 标记);
> 本清单是**请求侧**入口 ⇒ 对方出具结论后,本仓按结论翻转 `pending` 并回填 `ref`

---

## 0. 追认口径(per `42 号` 先例)

| 项 | 口径 |
|---|---|
| **确认** | 该决策的依据在 ArchForge 侧**仍然成立**,且不违反对方既有架构约束 |
| **纠正** | 对方给出**不同取值** ⇒ 本仓按对方结论改载体(并更新 `basis`) |
| **补规约** | 该行为语义应进对方规约层 ⇒ 本仓 `pending` 保留,直到对方规约落地 |
| **未回执** | 对方未回执 ⇒ 本仓 `pending` **保持 true**,**不得**写成「已裁决」 |

---

## 1. 请求项(12 条 = 债 #1 的 5 条 + 债 #3 的 6 条 + 债 #2 的 1 条)

### B-1 —— 层级数与层级语义(L0 最细 → L2 最粗)

| 字段 | 内容 |
|---|---|
| **跨仓债** | #1 |
| **决策点** | 层级数与层级语义(L0 最细 → L2 最粗) |
| **本仓当前取值** | 层级数与层级语义(L0 最细 → L2 最粗) |
| **本仓依据** | sharer_tracker.hh:138-141 的注释级描述(L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray) + ADR-CEP-04 §13 的「层级深度是实现参数,不是能力属性」 |
| **机器载体** | `include/tlm/cep/sharer_tracker.hh · HierarchicalDirectory::kBehaviorDecisions` |
| **待裁定** | 该行为语义在 ArchForge 侧无规约 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### B-2 —— LookupHazard::ESCALATE_UP 不使用;收窄语义由 lookup_scoped 显式提供

| 字段 | 内容 |
|---|---|
| **跨仓债** | #1 |
| **决策点** | LookupHazard::ESCALATE_UP 不使用;收窄语义由 lookup_scoped 显式提供 |
| **本仓当前取值** | LookupHazard::ESCALATE_UP 不使用;收窄语义由 lookup_scoped 显式提供 |
| **本仓依据** | ADR-CEP-04 §2 的 hazard 语义(4 值)+ 本仓 traits 只声明 STALL ⇒ 引入 ESCALATE_UP 会让声明与实现脱节 |
| **机器载体** | `include/tlm/cep/sharer_tracker.hh · HierarchicalDirectory::kBehaviorDecisions` |
| **待裁定** | 该行为语义在 ArchForge 侧无规约 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### B-3 —— snoop_fold 的合并规则复用 SnoopMechanics::join_state(与 FullMapDirectory 同源)

| 字段 | 内容 |
|---|---|
| **跨仓债** | #1 |
| **决策点** | snoop_fold 的合并规则复用 SnoopMechanics::join_state(与 FullMapDirectory 同源) |
| **本仓当前取值** | snoop_fold 的合并规则复用 SnoopMechanics::join_state(与 FullMapDirectory 同源) |
| **本仓依据** | ADR-CEP-04 §4 的 fold 语义不随实现而异 ⇒ 禁双实现 |
| **机器载体** | `include/tlm/cep/sharer_tracker.hh · HierarchicalDirectory::kBehaviorDecisions` |
| **待裁定** | 该行为语义在 ArchForge 侧无规约 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### B-4 —— handle 空间满 ⇒ 拒绝(返回 handle = 0),不覆盖存活 handle

| 字段 | 内容 |
|---|---|
| **跨仓债** | #1 |
| **决策点** | handle 空间满 ⇒ 拒绝(返回 handle = 0),不覆盖存活 handle |
| **本仓当前取值** | handle 空间满 ⇒ 拒绝(返回 handle = 0),不覆盖存活 handle |
| **本仓依据** | 与 FullMapDirectory 同源(SnoopMechanics::allocate_handle 的线性探测 + 满表拒绝) |
| **机器载体** | `include/tlm/cep/sharer_tracker.hh · HierarchicalDirectory::kBehaviorDecisions` |
| **待裁定** | 该行为语义在 ArchForge 侧无规约 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### B-5 —— F 态(转发点)角色在 tracker 中不可分辨 —— 只承载转发目标节点

| 字段 | 内容 |
|---|---|
| **跨仓债** | #1 |
| **决策点** | F 态(转发点)角色在 tracker 中不可分辨 —— 只承载转发目标节点 |
| **本仓当前取值** | F 态(转发点)角色在 tracker 中不可分辨 —— 只承载转发目标节点 |
| **本仓依据** | sharer_tracker.hh:128 的 SnoopState 只有 I/S/E/M,无 F ⇒ 「谁是转发点」对 tracker 不可见 ⇒ 登记为转发点保真度缺口 |
| **机器载体** | `include/tlm/cep/sharer_tracker.hh · HierarchicalDirectory::kBehaviorDecisions` |
| **待裁定** | 该行为语义在 ArchForge 侧无规约 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### E-1 —— F 态在 wire 上投影为 LineState::S + dirty = 0(perm 仍取 WRITEBACK)

| 字段 | 内容 |
|---|---|
| **跨仓债** | #3 |
| **决策点** | F 态在 wire 上投影为 LineState::S + dirty = 0(perm 仍取 WRITEBACK) |
| **本仓当前取值** | F 态在 wire 上投影为 LineState::S + dirty = 0(perm 仍取 WRITEBACK) |
| **本仓依据** | CohIfBundle::line_state 只有 NONE/I/S/E/M,无 F;而 F 语义是「已写回 ⇒ 非脏」 ⇒ 发 S 才不让对端误记脏位(发 M 会违反「M ⇒ dirty」这条别处依赖的不变量) |
| **机器载体** | `include/tlm/cep/mesif_protocol_engine.hh · MesifProtocolEngine::kEngineDecisions` |
| **待裁定** | 该建模决策为本仓自推 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### E-2 —— SnoopType::CLEAN 与 SnoopType::FORWARD 都映射为 Event::SnpShared

| 字段 | 内容 |
|---|---|
| **跨仓债** | #3 |
| **决策点** | SnoopType::CLEAN 与 SnoopType::FORWARD 都映射为 Event::SnpShared |
| **本仓当前取值** | SnoopType::CLEAN 与 SnoopType::FORWARD 都映射为 Event::SnpShared |
| **本仓依据** | 规约的 6 事件词表(Rd/RdUnique/Wr/Upgrade/SnpShared/SnpUnique)没有「降级为 S」的独立事件 ⇒「降到 S」与「成为转发点」在事件层不可分辨(6 事件的表达能力缺口) |
| **机器载体** | `include/tlm/cep/mesif_protocol_engine.hh · MesifProtocolEngine::kEngineDecisions` |
| **待裁定** | 该建模决策为本仓自推 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### E-3 —— on_request(线缆上收到请求)只记账不迁态;本地请求由 submit_local_request 发起

| 字段 | 内容 |
|---|---|
| **跨仓债** | #3 |
| **决策点** | on_request(线缆上收到请求)只记账不迁态;本地请求由 submit_local_request 发起 |
| **本仓当前取值** | on_request(线缆上收到请求)只记账不迁态;本地请求由 submit_local_request 发起 |
| **本仓依据** | 本引擎建模的是节点侧(目录在 PE 之外)⇒ 请求的响应由 home 以 on_completion 回来;收到请求就地迁态等于把 home 的行为塞进节点侧 |
| **机器载体** | `include/tlm/cep/mesif_protocol_engine.hh · MesifProtocolEngine::kEngineDecisions` |
| **待裁定** | 该建模决策为本仓自推 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### E-4 —— on_completion(perm = WRITEBACK) ⇒ RdUnique 后接 Wr(两次表查询)

| 字段 | 内容 |
|---|---|
| **跨仓债** | #3 |
| **决策点** | on_completion(perm = WRITEBACK) ⇒ RdUnique 后接 Wr(两次表查询) |
| **本仓当前取值** | on_completion(perm = WRITEBACK) ⇒ RdUnique 后接 Wr(两次表查询) |
| **本仓依据** | E + Wr → M 是本规约的静默升级(is_silent_upgrade);一次直接跳 M 会绕过该路径的记账 |
| **机器载体** | `include/tlm/cep/mesif_protocol_engine.hh · MesifProtocolEngine::kEngineDecisions` |
| **待裁定** | 该建模决策为本仓自推 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### E-5 —— on_snoop_resp / on_forward_data / on_barrier / on_snoop_completion_ack 只记账

| 字段 | 内容 |
|---|---|
| **跨仓债** | #3 |
| **决策点** | on_snoop_resp / on_forward_data / on_barrier / on_snoop_completion_ack 只记账 |
| **本仓当前取值** | on_snoop_resp / on_forward_data / on_barrier / on_snoop_completion_ack 只记账 |
| **本仓依据** | 本引擎不建模在飞 snoop 集合与屏障域(归 Stage 2)⇒ 只记账不会产出虚假行为 |
| **机器载体** | `include/tlm/cep/mesif_protocol_engine.hh · MesifProtocolEngine::kEngineDecisions` |
| **待裁定** | 该建模决策为本仓自推 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### E-6 —— VC 路由只记意图(last_atomic_network / last_atomic_vc),不写进 Bundle

| 字段 | 内容 |
|---|---|
| **跨仓债** | #3 |
| **决策点** | VC 路由只记意图(last_atomic_network / last_atomic_vc),不写进 Bundle |
| **本仓当前取值** | VC 路由只记意图(last_atomic_network / last_atomic_vc),不写进 Bundle |
| **本仓依据** | CohIfBundle 无 vc 字段(VC 由 fabric 按 vc_binding 分配)⇒ 引擎只能回答「按声明该走哪个 (network, vc)」;扩字段会破 CohIfBundle 零改动边界 |
| **机器载体** | `include/tlm/cep/mesif_protocol_engine.hh · MesifProtocolEngine::kEngineDecisions` |
| **待裁定** | 该建模决策为本仓自推 ⇒ 请裁定:确认 / 纠正 / 补规约 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

### A-1 —— atomic 的 consistency_intent 在协议侧的精确定义(是否需要独立字段 / 取值域 / 与 perm 的正交关系)

| 字段 | 内容 |
|---|---|
| **跨仓债** | #2 |
| **决策点** | atomic 的 consistency_intent 在协议侧的精确定义(是否需要独立字段 / 取值域 / 与 perm 的正交关系) |
| **本仓当前取值** | atomic 的 consistency_intent 在协议侧的精确定义(是否需要独立字段 / 取值域 / 与 perm 的正交关系) |
| **本仓依据** | 本仓当前处置:最小可用路径 —— 意图位走 CohIfBundle::atomic(1-bit),perm 与 intent 正交;不新增字段(per CEN-Ib11 的 4 值锁) |
| **机器载体** | `**无载体**(本仓未做该决策)⇒ 纯请求项` |
| **待裁定** | 请给出 consistency_intent 的协议侧定义:字段必要性 / 取值域 / 与 perm 的正交约束 / atomic 类型的分类 |
| **裁定后本仓动作** | 确认 ⇒ `pending = false` + 回填对方结论行号;纠正 ⇒ 改载体取值 + 更新 `basis` + 追加台账 `adr-amendment`;补规约 ⇒ `pending` 保持 true |

---

## 2. 残余风险(不得写成「已消除」)

| # | 风险 | 状态 |
|---|---|---|
| **A** | `HierarchicalDirectory` 行为语义**无真源** ⇒ 实现者被迫自推 | 🟡 中 —— 5 条已登记待追认(B-1 ~ B-5);`pending` 全 true |
| **C** | `atomic.supported = true` 与实现可能脱节(`consistency_intent` 无独立字段) | 🟡 中 —— 已做最小可用路径 + 显式登记;协议侧定义待对方给出(A-1) |
| **E** | 引擎侧建模决策 E-1 ~ E-6 为本仓自推 | 🟡 中 —— 6 条已登记待追认;`pending` 全 true |

> **口径**:本仓**不**把「已登记」写成「已裁决」;未回执 ⇒ `pending` 保持 true。

---

## 3. 维护记录

| 日期 | 变更 |
|---|---|
| 2026-10-30 | 初版:3 项跨仓债的 12 条请求项(B-1 ~ B-5 / E-1 ~ E-6 / A-1);**由载体生成** |
