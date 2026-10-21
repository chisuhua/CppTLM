# CEP 架构决策日志(架构师工作产物,v0.6 模型)

> **位置说明**(per oracle v0.4 路线图评审 S6 修订):本文件原位于 `.rddf/state/`,但该目录惯例仅存 JSON state 文件;已迁至 `.rddf/drafts/`(已存在的草稿层)。**不得**作为 subagent 真源 — 真源是 10 份 ADR v0.6。

> **Status**: ✅ v0.2/v0.3 模型(per oracle B2-3 + metis G3 修订)
> **会话 ID**: ses_eabcf0976ffeE8qHB4KbJO1YG3
> **轮次**: Round 1(2026-10-19)+ Round 2(2026-10-20)
> **角色**: architect(per AGENTS.md §2 角色矩阵)
> **范围**: Coherence Exploration Platform(CEP)5 个先决议题拍板 + v0.2 修订 + metis 战略盲点补充
> **关联真源**:
> - 真源 1(通用基础):`docs/architecture/06-inter-module-network.md` v0.4 + `06-bis-coherence-as-network-extension.md` v0.3
> - 真源 1:`docs/adr/ADR-CEP-00A-real-source-anchor-map.md`(本批起草,真源章节映射)
> - 真源 3(不可变 ADR):`docs/adr/ADR-DGPU-13` / `ADR-DGPU-14`(已 superseded by 06 v0.4)

**⚠️ stale 警告**:本文件 v0.3 之前版本含过期表述(net-side 4 / ADR-I01 / barrier_resp / adapter),**禁止**作为 subagent 真源。真源是 10 份 ADR v0.6(`docs/adr/ADR-CEP-*.md`)。

---

## 拍板汇总(v0.3 模型)

### A1 归口 ✅(c) 修正版 + supersede 裁决 + 单 DRI

**决策**: 代码全归 CppTLM,设计权威按定性分层;CEP ↔ 06-bis P2-P5 supersede 路径(具体锚点 per `ADR-CEP-00A` 真源映射表);设架构师本人为 CEP DRI。

**修订要点**(per oracle B2-1 + B2-2):
- Phase 0 umbrella change 计入 KPI(修正 B2-1 自相矛盾)
- 06-bis §7.8 = "v0.3 总 KPI 调整"(KPI 串行约束),不是阶段范围;**06-bis P2-P5 阶段范围实为 §7.3-§7.7**
- KPI 算术闭环:Phase 0 + Phase 1(6 个) + Phase 2(2 个) + Phase 3(1 个) = 10 个 change(per `ADR-CEP-00 §2 Decision 3`)

### A2 Fabric Collective 归属 ✅(γ) 旁路平面 + barrier 重钉 + 完成域判别规则

**决策**: 5 层 → 6 层(Directory & Ordering Plane ∥ Fabric Collective Plane 横向并列);`barrier` 流重钉为 Ordering Point(零流数变更);完成域判别规则作为新 ADR 锚。

**修订要点**(per oracle B2-4):
- 集合流量通道隔离 = **vc_id**(`noc_bundles_tlm.hh:38` 已有 0-3 字段)+ 多 VC 仲裁待 CEP-09
- flit_category 实为 **3 类**(REQUEST=0 / RESPONSE=1 / CREDIT=2,per `noc_bundles_tlm.hh:60-62`),不是初版写的 2 类

### A6 SHARP 语义 ✅(β) 内部接口 + 3 个生效条件

**决策**: SHARP / UALink INC 走 Fabric Collective Plane 内部接口;group_id 不进 Coherence Interface。

**修订要点**(per oracle B2-7 + B2-8):
- R4 epoch 未 commit 窗口= **正常服务**(per `noc_bundles_tlm.hh:60-62` 实证);伪码 step 4 fail-stop 引错规则,改引 R4
- drain 调用边从 **node 端** Coherence Interface 端口发出,plane 间零直接调用边(per `ADR-CEP-01 抓取点 1`)

### A4 CXL/CHI 共享 Tracker ✅(α′) α + γ′ + D-5 重构

**决策**: 单接口 + 5 种实现(FullMap / Sparse-SF / DcohSymmetric / TreeManager / BroadcastNull);caps descriptor 替代 bool;D-5 重构。

**修订要点**(per oracle B2-5 + B2-10):
- 6 份 ADR 头部真源分级统一:**ADR 一律真源 3**(per AGENTS.md)
- canonical 流 4 = **`snoop_resp`**(per `ADR-CEP-02 §2`);CEP-05 stub 接口 = `snoop_resp`(逐字对齐)
- Hazard 字面值统一:enum `Hazard_STALL` / `Hazard_CHAIN` / `Hazard_NONE`(per `ADR-CEP-04 §4` 修订,scoped 到 ADR 内部)
- CXL aggregation 二元:P2P → `REQUESTER`,HDM home → `HOME`(spec 写死)

### A7 Stub vs CohIf 边界 ✅(α') RN 投影 + 命名复用

**决策**: Stub 接口 = Coherence Interface RN 侧投影(net-side 5 + core-side 1);命名复用 CohIf 词表;三明治挂载(cpu_side / net_side);bypass 模式保留为 testbench 配置选项。

**修订要点**(per oracle B2-9 + B2-10):
- **canonical 流清单**(`ADR-CEP-02 §2`):
  ```
  流 1: request (node→center)
  流 2: completion (center→node)
  流 3: snoop (center→node)
  流 4: snoop_resp (node→center)
  流 5: writeback (node→center)
  流 6: forward_data (node↔node,经中心调度)
  流 7: back_invalidate (**center→node**,per BISnp host→device 方向)
  流 8: address_resolve (node↔center)
  流 9: barrier (center→node ↔ center,双向,requester-scoped)
  流 10: snoop_completion_ack (node→center)
  ```
- **Stub 接口命名逐字对齐**(per `ADR-CEP-05 §2`):`request` / `snoop` / `snoop_resp` / `writeback` / `completion` + `evict_line`(core-side)

---

## 6 份 ADR 草案清单(v0.3 修订后)

| # | 文件路径 | 行数(v0.3)| 关联议题 |
|---|---------|----------|---------|
| 00 | `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` | 193 | A1 归口 + KPI 排队 |
| 00A | `docs/adr/ADR-CEP-00A-real-source-anchor-map.md` | 233 | 真源章节映射(解 B2-2/B2-3)|
| 01 | `docs/adr/ADR-CEP-01-fabric-collective-plane.md` | 187 | A2 Collective 旁路 |
| 02 | `docs/adr/ADR-CEP-02-coherence-barrier-semantics-clarification.md` | 197 | canonical 10 流清单 + barrier |
| 03 | `docs/adr/ADR-CEP-03-fabric-collective-plane-internals.md` | 232 | SHARP 内部 + non-cacheable |
| 04 | `docs/adr/ADR-CEP-04-sharer-tracker-abstraction-redesign.md` | 314 | D-5 重构 + caps |
| 05 | `docs/adr/ADR-CEP-05-stub-cache-core-interface.md` | 286 | RN 投影 + 折叠表 |
| 06 | `docs/adr/ADR-CEP-06-experiment-protocol.md` | 202 | 实验输出定义(解 G1) |
| 07 | `docs/adr/ADR-CEP-07-simulation-and-integration-semantics.md` | 230 | 仿真语义 + 集成 seam(解 G4) |
| NUM | `docs/adr/ADR-CEP-NUMBER-MAP.md` | 171 | 编号预分配(解 G5 隐式依赖环) |

**总规模**:**2245 行 / ~117 KB**(10 份 ADR)

---

## v0.3 修订汇总(10 BLOCKERS 全解)

| # | 解法 | 修订 ADR |
|---|------|---------|
| **B2-1** | KPI 算术闭环:Phase 0 计入 KPI,总 10 change = 1 + 6 + 2 + 1 | CEP-00 §2 Decision 3 |
| **B2-2** | 06-bis 锚点集群:全部改为 §7.3-§7.7(阶段)+ §7.8(KPI) | 5 份 ADR 9 处 + ADR-CEP-00A |
| **B2-3** | 06-bis §11.5 误锚:头部与 §4 同步 | CEP-02 + CEP-05 |
| **B2-4** | flit_category "2 类" 改 "3 类";通道隔离 = vc_id | CEP-01 §3.3 |
| **B2-5** | 真源分级统一:ADR 一律真源 3 | 6 份 ADR 头部 |
| **B2-6** | back_invalidate 方向 center→node | CEP-02 §2 + CEP-05 §2 |
| **B2-7** | R4 vs 伪码矛盾:R4 正常服务为准;伪码 step 4 改引 R4 | CEP-03 §4 |
| **B2-8** | drain 调用边:从 node 端 Coherence Interface 端口发出 | CEP-03 §3.2 |
| **B2-9** | B4 表面修订残留:§4 §4.2 锚点改"本 ADR 首次定义" | CEP-05 §4 |
| **B2-10** | snoop_response → snoop_resp(逐字对齐 canonical)| CEP-05 §2 |

---

## 不变量集合(v0.3,6 份 ADR 共同守门)

| 不变量 | 守门规则 | ADR |
|--------|---------|-----|
| **CEN-Ib1 ~ CEN-Ib10** | Coherence Interface canonical 10 条流 | CEP-02 §2 |
| **INV-SC1 ~ INV-SC6** | Stub Cache Core 6 条 | CEP-05 §4.3 |
| **CEP-04-IC1** | caps struct 字段值 ∈ enum 字面值(机器可查)| CEP-04 §2 |
| **CEP-04-IC2** | lookup/snoop_begin/snoop_fold 幂等 | CEP-04 §5 |
| **CEP-04-IC3** | 5 个条目生命周期事件名 + 时序契约 | CEP-04 §6 |
| **CEP-13** | SHARP R3 non-cacheable 降级契约 | CEP-03 §3.2 |
| **CEP-00A-IC1** | ADR 章节引用必须命中 `ADR-CEP-00A 真源映射表` | ADR-CEP-00A |
| **CEP-06-IC1** | 实验输出必须含 metric + 对比报表 | ADR-CEP-06 |
| **CEP-07-IC1** | simulation_mode ∈ {functional, timing} 单一维度 | ADR-CEP-07 |

---

## metis G1/G3/G4 战略盲点并行解除

- **G1 实验输出**: `ADR-CEP-06-experiment-protocol.md`(本批起草)
- **G3 状态文件**: 本文件 v0.3 重写
- **G4 仿真语义 + 集成 seam**: `ADR-CEP-07-simulation-and-integration-semantics.md`(本批起草)

---

## 第 3 回合预告(等修订完成 + oracle 二次评审通过)

9 个议题按 ADR-CEP-NUMBER-MAP.md 预分配编号起草:
- **CEP-08**: Ordering Point 可迁移性规则(A3)
- **CEP-09**: TileLink-C Tree Tip 映射 + 多 VC 仲裁(A5)
- **CEP-10**: 可观测性归属(A8)
- **CEP-11**: CHI 版本锁定 + 其他协议版本策略(A9)
- **CEP-12**: ProtocolEngine 插件能力映射表 schema(A10)
- **CEP-13**: 一致性域归属(A11,**与不变量 CEP-13 编号冲突,改名 INV-CEP-13NC**)
- **CEP-14**: Stub 可验证性合同测试(A12)
- **CEP-15**: CppTLM CacheTLM 对齐(A13)
- **CEP-16**: 06 §15.4 P2.0-P5 演进路线对应(A14,**无 P6**)

**总计**:6 + 1(00A) + 1(06) + 1(07) + 1(NUM) + 9(CEP-08 ~ CEP-16) = **19 份 ADR**

---

## 评审闭环(2026-10-19 ~ 2026-10-20)

- **oracle v0.3 REJECT**(2026-10-19):7 BLOCKERS + 11 SHOULD_FIX + 10 NITS
- **v0.4 修订**(2026-10-19):架构师自带 grep,oracle 实证,oracle B3-2/3/4/5/6/7 + B2-2/3/5/9/10 关闭
- **oracle v0.4 REJECT**(2026-10-20):8 BLOCKERS + 10 SHOULD_FIX + 6 NITS
- **v0.5 修订**(2026-10-20):B1/B2/B3/B4/B5/B6/B8 关闭,B7(注册宏形式)残留 4 处
- **oracle v0.5 窄口 REJECT**(2026-10-20):1 BLOCKER B7 + 6 SHOULD_FIX + 8 NITS,其余 9 份达可签发水准
- **v0.6 修订**(2026-10-20):B7(注册宏)完全关闭 + 全部 10 份 ADR 头部标 v0.6
- **oracle 评级**:"无需再开架构层讨论;修完后本批 10 份(CEP-00 待签发仓拍板)即可进入 OpenSpec tasks.md 起草阶段"

## 路线图承接(per planner mode 2026-10-20)

- **新增 phase-10**:`.rddf/roadmap/phases/phase-10.md` 起草完成(CEP 规划阶段)
- **新增 improvement**:`.rddf/improvements/cpptlm-cep-umbrella.md` 起草完成(5-segment draft: Why / What Changes / Non-Goals / Capabilities / Acceptance)
- **更新 roadmap.md 主文档**:phase-10 加入 Phase Skeleton + AUTO-INDEX;baseline 标注为 2/3 (phase-9 active 2 + phase-10 umbrella pending 1,合规)
- **状态文件归档**:本文件 v0.6 模型已稳定,移交 planner / rdd-builder 阶段
- **下阶段入口**:`openspec/changes/cpptlm-cep-umbrella/`(builder 模式起草)或 `.rddf/plans/cpptlm-cep-umbrella.md`(rdd-workflow 计划生成)

---

**架构师工作产物** — 不修改 src/openspec/.rddf/plans/,仅起草/修订 ADR + 维护状态文件。