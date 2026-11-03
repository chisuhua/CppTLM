# ADR-CEP-03: Fabric Collective Plane 内部接口与跨平面契约

> **状态**: ✅ **已签发(v1.0)**(2026-11-03 定稿;per 台账 **Entry #169** —— 队列 #13 `cpptlm-cep-nvlink-sharp` 的**开工前置**)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)/ **2026-11-03 定稿**
> **作者**: CppTLM build agent(architect mode)
> **关联真源**:
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节
> - 真源 1:`06-bis-coherence-as-network-extension.md` v0.3 §5.2 `SnoopProbe`/`SnoopRespBundle`/`CoherenceMsg`(per `ADR-CEP-00A §3.2` 实证行号)— **沿用**,无 non-cacheable 概念

---

## 1. 背景

`ADR-CEP-01` 锁定 Fabric Collective Plane 作为旁路平面 + 完成域判别规则。本 ADR 锁定该平面的**内部接口 + 跨平面契约 + SHARP/UALink 防误分类规则**,作为 SHARP / UALink 插件实施的前置门。

**核心问题**:集合通信的完成域是 group-scoped,与 coherence 的 pairwise 纪律不同。两个平面之间的 ordering/visibility 必须另有契约,否则:
- 同一 endpoint 既发 coherent atomic 又发 SHARP reduce 到重叠地址时,两个 plane 之间的可见性/顺序无定义
- ReadReduce 要读内存,集合 plane 会自然长出类 read/write 原语,慢慢变成第二个、更弱的 coherence interface

---

## 2. 决策

### 2.0 选项对比表(定稿补入 —— 「集合平面怎么不与目录纠缠」的可审计取舍)

| 方案 | 结论 | 理由 |
|---|---|---|
| **A** 集合 plane **直接调** Directory 做归约 | ❌ **否决** | 违反 `ADR-CEP-01 §3.3` 抓取点 1(调用边**必须为零**)⇒ **层级纠缠**;且 coherence 的完成纪律是 requester-scoped,与归约的 group-scoped **不同步** |
| **B** 归约**只**在 host / 软件侧做(SHARP **不建模**) | ❌ **否决** | 平台答不了「网内归约 vs coherence 可见性」⇒ 队列 #13 / #14 的**存在意义**消失;且 07 号 §5.1.1 的 NVLink 行明写「NVSwitch 内嵌 SHARP」 |
| **C** 独立 collective plane + **跨平面契约**(R1~R5 + A~D)+ registered region guardrail | ✅ **采纳** | 两平面**隔离** + 跨平面契约**显式化**;R3 的 drain 走**发起方自身**的 Coherence Interface 端口发 `request(invalidate)` ⇒ **仍零直接调用边**(per §3.4) |
| **D** SHARP 状态机**完整**写进本 ADR | ❌ **否决** | 属**推导 / 实现细节**(per 产出形态契约「推导与证据外移」)⇒ 留在 ADR 会变成**纸面契约** ⇒ 降级为「判据摘要 + 实现指针」(per §4) |

### 决策 1:3 个生效条件(per A6 拍板)

**任何 SHARP / UALink 插件落地前必成**:

| # | 生效条件 | **当前满足状态**(2026-11-03 定稿时点) |
|---|---|---|
| 1 | **跨 plane ordering/visibility 契约必须先成文**(本 ADR §3 详化) | ✅ **已满足** —— §3.1 R1~R5 + §3.3 A~D |
| 2 | **集合 plane guardrail**:操作只允许作用于 registered collective region(per ADR-CEP-01 §3.1 抓取点 1 派生) | ✅ **已满足** —— §3.4 |
| 3 | **coherence `barrier` 流重钉为 Ordering Point**(per `ADR-CEP-02` Decision 3) | ✅ **已满足** —— `ADR-CEP-02` 决策 3(2026-11-03 定稿签发) |

> ⚠️ **定稿裁决(2026-11-03)**:三条**内容**前置**均已满足** ⇒ **契约面**可开工;
> 剩余前置 = ① **本三则 ADR 的文本定稿**(用户裁决 2026-11-03,本条即授权记录)② 队列 #13 的 **Task 0 公开来源普查**
> ⇒ **两者都满足**方可建 change 目录(per 台账 Entry #168 的 Acceptance 第 1、2 项)。

### 决策 2:Fabric Collective Plane 抽象接口

```systemverilog
// Fabric Collective Plane —— 与 Coherence Interface 平行的独立抽象
// 完成纪律: group-scoped (完成 = 全组 ack / 全组到达)
// ⚠️ **恰好 7 个虚方法**(per ADR-CEP-00:331 的交付范围;判据 `ADR-CEP-03` §9 V1)
interface CollectivePlaneIf;
  // ---- group 生命周期(慢路径 · 管理语义;放 control plane 是正当的)----
  function group_id_t group_create(member_set_t members, group_attrs_t attrs);
  function void       group_destroy(group_id_t gid);
  function status_t   group_register_region(group_id_t gid, addr_range_t range,
                                            access_mode_t mode);   // SHARP 式注册:触发 coherence drain(跨 plane 契约入口)
  // ---- 数据路径 ----
  task send(CollectiveReq req);        // 字段即 plane 的"通用词汇":
                                        // { group_id, op, tag(phase/round), src_member,
                                        //   payload_desc(addr+len | inline), reduce_op, dtype, multicast_mask }
                                        // group_id 在此是一等公民 —— 它对本 plane 的通用性
                                        // 等价于 address 对 coherence plane 的通用性
  task recv(output CollectiveEvt evt); // 完成语义: 不是单点 ack,而是 GroupCompletion(gid, tag)
                                        // CollectiveEvt ::= Result(gid,tag,payload)
                                        //                 | Completion(gid,tag) | Fault(gid,culprit,cause)
  task collective_barrier(group_id_t gid, phase_t phase); // 相位转换原语:同相位内跨参与者无序,相相间有序
  function cap_t query_capability(op_t op, size_t max_bytes); // 能力协商:CHI → {multicast_sync}
                                        // (per ADR-CEP-01 Decision 1 裁剪实例)· NVLink → SHARP op 集 · UALink → 全集合族
endinterface
```

**原语最小集**: `{REDUCE, BROADCAST, MULTICAST, BARRIER}`(AllReduce 不列为原语 = REDUCE + BROADCAST 的复合,由 plane 内部或调用方组合)。

### 决策 3:NullCollectivePlane 作为"启停干净"验收测试

- CHI / CXL 配置下,Collective Plane 默认绑 `NullCollectivePlane`(所有原语返回"不支持")
- 这是"启停干净"的可测试载体(per ADR-CEP-01 §3.3 抓取点)
- 不需要 framework 改造,直接测试 boot / shutdown 期间不调用任何 collective 原语

---

## 3. 跨平面 ordering/visibility 契约

### 3.1 SHARP atomic vs cache-coherent atomic 区分

**关键事实**:NVLink 线上是同一个 TL 请求,**区分点不在 opcode**。硬件真相是 NVSwitch 依据**fabric 侧状态**(该地址是否落在已注册的 SHARP reduction context / group table 内)决定是在 switch 内终结还是继续路由到 home node。

**5 条防误分类规则**(全部入 ADR 正文,per ADR-CEP-01 §3.3 抓取点 5):

| 规则 | 内容 | 实施载体 |
|------|------|---------|
| **R1:opcode 永远不作分类器** | 源端插件只发一种 ATOMIC,不知道、也不需要知道 SHARP 的存在 | ProtocolEngine NVLink 适配器 |
| **R2:唯一分类点在 fabric ingress** | `(dest_addr ∩ collective_region_table) && group_epoch_valid` | Fabric & Transport Plane ingress |
| **R3:区域互斥强制** | 一个地址不得同时"SHARP 已注册"且"coherence-cacheable"。组注册握手时执行 drain(invalidate/flush 覆盖该区域的缓存副本,或将该区域在 coherence plane 降级为 non-cacheable)| `group_register_region` 内部 drain 流程 |
| **R4:epoch 纪律** | group_destroy 后区域回归 coherence 域,回归动作 = 完成事件触发 stale copy 无效化。epoch 未 commit 前,对该区域的 coherence atomic = **正常服务**;commit 后 = divert | `group_destroy` + Fabric ingress divert 表 |
| **R5:可观测性** | divert 计数器 + fail-stop 断言("epoch commit 后仍有 coherence atomic 命中已注册区域 = error") | observability plane / testbench |

### 3.2 R3 "non-cacheable 降级" 语义首次定义

**修订**:06-bis §5 coherence_bundles 全文无 non-cacheable 概念(grep 零匹配)。**non-cacheable 降级语义由本 ADR 首次定义**,作为 `group_register_region` 内部 drain 流程的一个选项:

```
non-cacheable 降级语义(CEP-13 · 首次定义):
  - 触发: `group_register_region(gid, range, mode=NC_DROP_CACHEABLE)` → 对 range 内所有 cache line
    调 Coherence Interface `request(invalidate)` 流(per ADR-CEP-02 canonical 10 流)
  - 完成判定: 全部 invalidate 完成 → region 降级为 non-cacheable;此后该 region 的 coherence
    `request` 命中 → not-allowed 错误(CEN-Ib7 扩展)
  - 复位: group_destroy 触发 R4 失效化,region 回归 coherence-cacheable
不变量(CEP-13): 同一地址不得同时"SHARP 已注册"且"coherence-cacheable";drain 失败 → 返回 error,不创建 epoch
```

**后续**:`06-bis-coherence-as-network-extension.md` v0.3 §5 后续 supersede 后,可在新 `07-coherence-exploration-platform.md` 真源 1 中重述。

### 3.3 跨平面 ordering 契约

**复合流的可见性要求**:

| 场景 | 契约 |
|------|------|
| A. Coherent atomic → SHARP reduce 同一地址 | Coherent atomic 必须先 completion 后,SHARP reduce 才能访问该地址(否则 reduce 读到 stale) |
| B. SHARP reduce → Coherent atomic 同一地址 | SHARP reduce 必须先 commit(ep_done)后,Coherent atomic 才能 miss 该地址 |
| C. SHARP reduce → SHARP reduce 同一 group | 同一 group 内 reduce 顺序由 code 排序约束(`guard`);不同时相的 reduce 跨 group 无序 |
| D. SHARP barrier → SHARP reduce | per Collective Plane 内部 ordering(group-scoped rendezvous) |
| E. Coherent barrier → Coherent atomic | per Coherence Interface `barrier` 流(requester-scoped drain) |

(per oracle N2 修订:编号 C 补回,原缺 C)

**实现**:handler 以 PolicyRule = `(plane_A_op, plane_B_op, ordering_requirement)` 形式声明;Core / Traffic Generator 编排时按规则调整发射顺序。**禁止**通过拓宽 coherence 事务集来表达跨平面 ordering。

### 3.4 guardrail:registered collective region

**强制规则**:
- 集合 plane 操作只允许作用于 registered collective region
- 未注册区域的 reduce/broadcast/multicast 操作 = `group_register_error` (立即报错,不静默)
- 注册时执行 R3 区域互斥 drain(**由发起注册的 node 端经自身 Coherence Interface 端口发出 `request(invalidate)`,plane 间零直接调用边**,per oracle B2-8 修订 + `ADR-CEP-01 §3.3 抓取点 1`),注销时执行 R4 epoch 失效化

---

## 4. SHARP 插件状态机:**判据摘要 + 实现指针**(⚠️ 定稿降级)

**定稿裁决(2026-11-03)**:本节原先是**占位符伪码**的「状态机雏形」⇒ 若原样签发即**纸面契约**
(与 R11「声明超前于实现」同型)⇒ per 产出形态契约「**推导与证据外移**」,本节降级为
**判据摘要 + 实现指针**;**可执行细节**(类型 / 表结构 / 计数 / 断言)落**队列 #13 的规约层** ⇒
**本 ADR 的决策内容零改动**(R1~R5 与 §3.2 / §3.3 / §3.4 逐条不变)。

**判据摘要(3 条 —— 规约层必须逐条**可判定**,不得再退化为伪码)**:

| # | 判据 | 可观测行为 |
|---|---|---|
| **S1** | **唯一分类点在 fabric ingress** | 同一地址在 `collective_region_table` 命中 **且** `group_epoch_valid` ⇒ **divert** 到 Collective Plane;否则 ⇒ 走 coherence `request` 流;**命中但 epoch 未 commit** ⇒ **放行**(per **R4**「正常服务」)—— ⚠️ **不得**写成「未 commit 前拒绝」(与 R4 矛盾) |
| **S2** | **注册 = 登记 + drain + 建 epoch**(三步**原子**) | `group_register_region` 成功 ⇒ 区域**已在**表内 **且** drain **已完成** **且** `epoch_valid = true`;drain 失败 ⇒ **不建 epoch** 且区域**不**在表内(per §3.2 不变量) |
| **S3** | **注销 = 失效化 + 回归 coherence 域** | `group_destroy` 触发 stale copy 失效化 ⇒ 区域**退出**表 ⇒ 后续 coherence 请求**正常服务**(per **R4**) |
| **S4** | **R1:opcode 永不作分类器** | 源端插件对**已注册**与**未注册**地址发**同一** opcode(不因「这地址是 SHARP 区」而改指令)⇒ **分类只发生在 fabric ingress**;⚠️ 源端**不得**持有 `collective_region_table` 的可见性 |

**实现指针**:可执行形态落 `include/tlm/cep/nvlink_sharp_state_spec.hh` + 队列 #13 规约层的 `design.md`
⇒ **本 ADR 不复制该载体**(避免双真源);规约层的**逐格 `basis`** 是其唯一人工审计面。

**⚠️ 未落地时的回退口径(critic S5)**:队列 #13 **未落地** ⇒ **禁止**声明 `nvlink_4` 实例 / SHARP capability
⇒ **登记态 ≠ 能力**(声明面零兑现)⇒ 任何 `nvlink_4` 实例声明若先于本节载体出现 ⇒ 视为**过度声明**
⇒ 判据 = 队列 #13 的实例声明判据(`CDX-06`)与本节载体**同时**存在才成立。

---

## 5. 与 06-bis v0.3 §5 coherence_bundles 的关系

06-bis v0.3 §5 已定义:
- `SnoopProbe` / `SnoopRespBundle`(per-node snoop 语义,属 coherence plane)
- `CoherenceMsg` 协议 ID 真源表
- `bundles::` 命名空间 + `include/bundles/coherence_bundles_tlm.hh` 路径

**CEP Fabric Collective Plane 的 Bundle 定义**:
- **新增** `CollectiveReqBundle` / `CollectiveEvtBundle`(per §2 Decision 2 接口字段)
- **沿用** `bundles::` 命名空间 + `coherence_bundles_tlm.hh` 路径(避免另起路径)
- **沿用** `BUNDLE_FIELD_WIDTH` 编译期锁(per `include/bundles/cpphdl_types.hh`)

---

## 6. 关联 ADR / OpenSpec

- 前置:`ADR-CEP-01-fabric-collective-plane.md`(旁路平面)
- 前置:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流清单 + barrier 流附注)
- 关联:`ADR-CEP-00-platform-charter-and-governance.md`(归口,草案)
- 关联:`06-bis-coherence-as-network-extension.md` v0.3 §5(coherence_bundles 沿用)
- 后续 OpenSpec(架构师不写,builder 阶段):
  - `cpptlm-cep-collective-plane-base` —— ✅ **已归档**(2026-10-26;**Stage 1**;交付面 = `CollectivePlaneIf` 恰 7 方法 + `NullCollectivePlane` + `LocalCollectivePlane` + `collective_planes.json`)
  - `cpptlm-cep-nvlink-sharp` —— 📋 **Planned**(队列 #13 · **Stage 2** · 需本 ADR §3 契约先成;**两段式**:规约层 `cpptlm-cep-nvlink-sharp-state-spec` + 实现层)
  - `cpptlm-cep-ualink-inc` —— 📋 **Planned**(队列 #14 · **Stage 3**)

> ⚠️ **2026-11-03 订正**(per 台账 Entry #169):① 原写 `cpptlm-cep-nvlink-sharp-adapter` / `cpptlm-cep-ualink-inc-adapter` —— **该二名在本仓不存在**,台账实名是 `cpptlm-cep-nvlink-sharp` / `cpptlm-cep-ualink-inc`;② 原写「P2 / P3 / P4 实施 change」⇒ 与台账 **Stage** 映射(队列 #5 = **Stage 1** · #13 = **Stage 2** · #14 = **Stage 3**)**不一致**。

---

## 7. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 内部接口雏形 | `CollectivePlaneIf` (per §2 Decision 2) | ✅ 已拍板 |
| 跨平面契约 | 5 条防误分类规则 + ordering 契约 + registered region guardrail | ✅ 已拍板 |
| group_id 归属 | Collective Plane 一等公民(不进 Coherence Interface) | ✅ 已拍板 |
| AllReduce 归类 | = REDUCE + BROADCAST 复合,不列原语 | ✅ 已拍板 |
| NullCollectivePlane | "启停干净"验收载体 | ✅ 已拍板 |
| Bundle 路径 | ⚠️ **偏离登记**(per **Entry #66** SU):**类型内嵌** `include/tlm/cep/collective_plane.hh`,**不落 Bundle`;Bundle 化**推迟**至队列 #13(届时有真实传输通道) | ✅ **已拍板 + 已偏离登记** |
| 3 个生效条件 | 跨 plane 契约先成 / guardrail / barrier 重钉 | ✅ 已拍板 |
| non-cacheable 语义 | §3.2 CEP-13 首次定义(06-bis §5 无此概念) | ✅ 首次定义(per B10) |
| CHI 插件能力 | `{multicast_sync}`(per ADR-CEP-01) — 非"全空"(per S7 修订) | ✅ 已拍板 |

---

## 8. 后果 / 代价

### 8.1 正面

| # | 后果 |
|---|---|
| 1 | R1~R5 把「**opcode 不是分类器**」「**唯一分类点在 fabric ingress**」钉成**机械判据** ⇒ 避免「源端插件知道 SHARP 存在」这一**最易写错**的实现形态;registered region guardrail + non-cacheable 降级 ⇒ 未注册区域的操作**报错不静默** ⇒ 不会有「悄悄算错」的 reduce |
| 3 | 跨平面 ordering **A~D** ⇒ 混合流(coherent atomic ↔ reduce)的**可见性次序有定义**;§4 降级为「判据摘要 + 实现指针」⇒ **ADR 不再持有伪码** ⇒ 规约层是唯一可执行真源(无双真源) |

### 8.2 负面 / 代价

| # | 代价 |
|---|---|
| 1 | **三平面耦合**:collective plane 的正确性依赖 coherence plane 的 drain 能力 ⇒ **跨平面**测试成本显著高于单平面;R3 drain **代价高**(逐 cache line invalidate / flush)⇒ 仿真中的 drain 语义与真实硬件**不等价** ⇒ **性能结论不可外推**(本 ADR 只承诺功能正确性) |
| 2 | §3 的契约(R1~R5 + A~D + guardrail)横跨**两个平面** ⇒ 任一平面演进都要**复核**本 ADR;`BroadcastNull` 作为退化基线 ⇒ 「NVLink 实例**无目录**」这一选择**必须在实例声明里显式化**(否则与 `ADR-CEP-04 §2` 的 caps 表**冲突**) |

### 8.3 风险

1. **epoch 语义易写反** —— R4 的「epoch 未 commit 前 = **正常服务**」「commit 后 = divert」必须**逐字**保留(写成「未 commit 前**拒绝**」即与 §3.3 ordering A 冲突;⚠️ **已发生一次**,per 台账 Entry #169 ②-⑨)⇒ 缓解 = §9 **V6** 双向判据;
   **VC 编号臆造** —— 多 VC 仲裁属 `ADR-CEP-09`(⚠️ **2026-11-03 订正**:原写「(未起草)」**失实** —— 已起草(v0.1,2026-10-21),**仲裁策略属其决策 4**)⇒ 队列 #13 只能用**预留**口径(VC 预留判据在 **`ADR-CEP-01 §7 V6-a` / `V6-b`** —— ⚠️ **V6-a 是「现状恒真登记」**,**真正**的交付门在 **V6-b**);⚠️ 另见台账 **Entry #170 ⑤-补 ⑩**:两个网络的 `vc_id` 0-3 **全被占满** ⇒ **当前无解** ⇒ 须**先裁决 VC 空间方案**(见队列 #13 的 architect 裁决项);
3. **纸面契约回流** —— §4 的指针若被后续 change 忽略 ⇒ 回到「有契约无行为面」⇒ 缓解 = §9 全部判据**只**落在**可执行载体**上。

---

## 9. Verification(前瞻判据 · 可勾选 —— **不是**已跑完的实测日志)

| # | 判据 | 载体 | 反向 fixture(毒化 ⇒ 必红 + 归因 id) |
|---|---|---|---|
| **V1** | `CollectivePlaneIf` **恰 7 个**虚方法 | 结构断言 | 加第 8 个方法 ⇒ 必红(`CP-IFACE-7`) |
| **V2** | 原语最小集**恰 4**(`REDUCE` / `BROADCAST` / `MULTICAST` / `BARRIER`;`AllReduce` **不**列为原语) | 结构断言 | 加第 5 个原语 ⇒ 必红(`CP-PRIM-SET`) |
| **V3** | 未注册区域的操作 = **报错不静默** | 运行期 | **正控**:已注册区域**正常服务**;毒化:未注册区域被静默执行 ⇒ 必红(`CP-GUARDRAIL`) |
| **V4** | R3 drain 失败 ⇒ 注册失败且**不建 epoch** | 运行期 | **正控**:drain 成功 ⇒ 建 epoch;毒化:失败仍建 epoch ⇒ 必红(`CP-DRAIN-FAIL`) |
| **V5** | R5 divert 计数器 + **fail-stop** 断言(epoch commit 后仍有 coherence atomic 命中已注册区域 = error) | 运行期 | **正控**:正常路径计数器**递增**;毒化:该情形**不**报错 ⇒ 必红(`CP-DIVERT-STOP`) |
| **V6** | **epoch 语义双向**:命中表 **且** epoch **未** commit ⇒ **放行**;epoch **已** commit ⇒ **divert** | 运行期 | **正控**:两条路径各一例;毒化:「未 commit 前拒绝」⇒ 必红(`CP-EPOCH-SEMANTICS`)—— ⚠️ **本条正是 R4 的回归锁** |
| **V7** | Collective Plane → Directory 的调用边**恒为零** —— ⚠️ **判据本体在 `ADR-CEP-01 §7 V1`**(去重)⇒ 本行**不重复**实现 | 见 `ADR-CEP-01 §7 V1` | 归因 id **唯一** = `CP-01` |
| **V8** | **R1:opcode 永不作分类器** —— 源端对已注册 / 未注册地址发**同一** opcode(源端**不得**持有区域表可见性) | 队列 #13 规约层的 ProtocolEngine NVLink 适配器契约(**载体尚未存在** ⇒ 该判据在 #13 落地前**不可执行**) | 毒化:源端按「是否 SHARP 区」改 opcode / 查区域表 ⇒ 必红(`CP-OPCODE-CLASSIFIER`);⚠️ 归因 id **待 #13 建立** |

---

## Status Update 2026-10-26 —— §5 Bundle 路径的**偏离登记**(队列 #5 `cpptlm-cep-collective-plane-base`)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §8 正文一律不改**,仅登记一项**偏离**。
> ⚠️ **2026-11-03 订正**(per 台账 Entry #169 / #170):本段内**括注的 change 名 `cpptlm-cep-nvlink-sharp-adapter` 已作废** ⇒ 以**台账实名** `cpptlm-cep-nvlink-sharp` 为准。
> ⚠️ **编号漂移说明**(2026-11-03,per 台账 Entry #170 ⑤-补-3):本行「**§1 ~ §8**」按**当时编号**理解 —— 定稿新增两章(**§8 后果/代价** · **§9 Verification**)⇒ 原 §8「v0.2 修订记录」**已顺延为 §10** ⇒ 本行的「一律不改」**不**覆盖新增的两章。
> **偏离项**: **§5**「新增 `CollectiveReqBundle` / `CollectiveEvtBundle`,**沿用** `bundles::` 命名空间 + `coherence_bundles_tlm.hh` 路径」。
> **偏离理由**: §5 假设的 `include/bundles/coherence_bundles_tlm.hh` **在本仓不存在** —— 它是 `06-bis §5` 标注的「P2 实施」产物,而 06-bis 的 P2 **尚未实施**。三条候选路径的取舍:
>   1. **提前新建 `coherence_bundles_tlm.hh`** ⇒ 抢 06-bis P2 的文件,两个 change 改同一文件 ⇒ 冲突(与 `ADR-CEP-07A` 的 artifact disposition 冲突);
>   2. **新建 `include/bundles/collective_bundles_tlm.hh` 并立即落 Bundle** ⇒ 本 change 的 plane 是纯内存 + rendezvous,**无真实传输通道** ⇒ Bundle 无消费方 ⇒ 死文件 + 提前承担 `BUNDLE_FIELD_WIDTH` 锁的维护成本;
>   3. **类型内嵌 `include/tlm/cep/collective_plane.hh`**(沿用 `tlm::cep` 命名空间 + `ch_uint` 宽度体系,与队列 #3/#4 的 `stub_cache_core.hh` / `sharer_tracker.hh` 风格一致)⇒ **采纳**。
> **本 change 的落地**: 采用 (3) —— **不落 Bundle**;类型(`group_id_t` / `member_set_t` / `group_attrs_t` / `addr_range_t` / `access_mode_t` / `CollectiveReq` / `CollectiveEvt` / `cap_t`)内嵌于 `collective_plane.hh`。
> **Bundle 化的未来路径(具体,不写「以后再说」)**: 推迟至 **P3**(`cpptlm-cep-nvlink-sharp-adapter`)—— 届时有真实传输通道,新建 `include/bundles/collective_bundles_tlm.hh`,沿用 `bundles::` 命名空间 + `BUNDLE_FIELD_WIDTH` 编译期锁(与 §5 的其余约定一致)。> ⚠️ **2026-11-03 订正**(per 台账 Entry #169):括号内的 change 名 `cpptlm-cep-nvlink-sharp-adapter` **在本仓不存在** ⇒ 台账实名 = **`cpptlm-cep-nvlink-sharp`**(队列 #13 · **Stage 2**);「P3」在台账口径下 = **Stage 2**(per `ADR-CEP-16` 的 P↔Stage 承接映射)。
> **本 ADR 的 §2 Decision 2 / §3 跨平面契约 / §4 状态机雏形**: **零改动**,仍然有效;§4 明确「builder 阶段实施时按 C++ 实施细节展开」⇒ 本 change 正是那个展开,但**只展开到 BARRIER rendezvous**(REDUCE / BROADCAST / MULTICAST 的数据路径需 fabric ingress 分类,属 P3)。
> **台账**: `.rddf/roadmap/objectives/obj-cep-platform.md §11` **Entry #66**
> **冻结面**: 本文件属 M1 冻结面,本次追加已走授权闸(见 Entry #66)。

---

## 10. v0.2 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。

---

## 附录 · Verification 判据的**载体归属**与**归因 id 登记**(2026-11-03,per 台账 Entry #170)

**⚠️ 判据 id 体系(须与既有机器闸一致)** —— 本仓既有归因 id = **`CP-01`(零调用边)· `CP-02`(canonical 10 流零 diff)· `CP-03`(collective VC 预留)· `CP-04`(区域互斥)· `CP-05`(完成域判别)· `CP-06`(抓点可测性登记)**(见 `include/tlm/cep/collective_plane_contract.hh`)⇒ 本章判据**一律复用该体系**;**尚未纳入既有体系**者 ⇒ 归因 id **待队列 #13 建立**(登记于 #13 的 tasks)⇒ **禁**在本仓新造平行 id 体系。

**载体归属总表(S3 —— 每条判据落在**哪个队列的哪个载体**上)**

| # | 判据 | 载体归属 | 载体状态 |
|---|---|---|---|
| V1 | `CollectivePlaneIf` 恰 7 个虚方法 | 结构断言 · 队列 #5 已交付(`include/tlm/cep/collective_plane.hh`) | ✅ **已存在**(= **现状已满足 + 回归锁**) |
| V2 | 原语最小集恰 4 | 结构断言(`CollectiveOp` 恰 4 值)· 队列 #5 已交付 | ✅ **已存在**;归因 id **待 #13 建立** |
| V3 | 未注册区域报错不静默 | 运行期 · 队列 #5 的 `NullCollectivePlane` / `LocalCollectivePlane` **部分**覆盖 | ⚠️ **完整 guardrail 行为面**由队列 #13 交付(既有 id `CP-04`) |
| V4 | R3 drain 失败不建 epoch | 运行期(R3 drain 流程) | ⚠️ **载体尚不存在**(属队列 #13 规约层)⇒ 在 #13 落地前**不可执行**;归因 id **待 #13 建立** |
| V5 | R5 divert 计数 + fail-stop | 运行期 | ⚠️ 同上(队列 #13);归因 id **待 #13 建立** |
| V6 | epoch 语义双向(R4 回归锁) | 运行期 | ⚠️ 同上(队列 #13);归因 id **待 #13 建立** |
| V7 | 调用边恒为零 | **判据本体 = `ADR-CEP-01 §7 V1`**(去重) | ✅ **已存在**(队列 #5;既有 id `CP-01`) |
| V8 | R1 opcode 永不作分类器 | 队列 #13 规约层的 ProtocolEngine NVLink 适配器契约 | ⚠️ **载体尚不存在** ⇒ 由队列 #13 交付;归因 id **待 #13 建立** |

> ⚠️ **悬空判据禁令**(per Entry #66 对抓取点 2 / 7 的处置):**载体尚不存在**者 ⇒ 该判据在载体落地前**不可执行** ⇒ **禁**写成恒真 / 恒 SKIP 判据。

> ⚠️ **纪律**(per 队列 #12 的 4 铁律):① **纯负向断言须配正控** ② 每条判据须有**可执行反向 fixture 且因正确原因红**(须写明**载体**与**归因 id**)③ **编译期锁不得只以「编译红」充当 ctest 判据**。
> ⚠️ **载体归属**:下列判据**只**落在**可执行载体**(队列 #5 / #13 的实现)上,**不**以本 ADR 正文为判据对象。


---

## 定稿说明(2026-11-03,per 台账 **Entry #169** —— 队列 #13 `cpptlm-cep-nvlink-sharp` 的**开工前置**)

> **性质**:本段是**追加式**(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §9 **正文内容**除下列**订正**外**一律不改**。

**① 本次定稿的实质(4 类)**

| # | 动作 | 说明 |
|---|---|---|
| 1 | **§4 状态机雏形 ⇒ 降级为「判据摘要 + 实现指针」** | 原 §4 是**占位符伪码** ⇒ 若原样签发即**纸面契约**(与 R11「声明超前于实现」同型)⇒ per 产出形态契约「**推导与证据外移**」改为 **3 条可判定判据(S1~S3)+ 实现指针**(细节落队列 #13 的**规约层**)⇒ **R1~R5 与 §3.2 / §3.3 / §3.4 逐条不变** |
| 2 | **三处口径订正** | ① §6 + Entry #66 SU 段的 change 名 `cpptlm-cep-nvlink-sharp-adapter` / `cpptlm-cep-ualink-inc-adapter` ⇒ 台账实名 **`cpptlm-cep-nvlink-sharp`** / **`cpptlm-cep-ualink-inc`**;② §7 汇总「Bundle 路径」行 ⇒ 与 Entry #66 SU 的**偏离登记**对齐(**类型内嵌**,**不落 Bundle**);③ §6 的「P2 / P3 / P4」⇒ 台账 **Stage** 口径(#5 = Stage 1 · #13 = Stage 2 · #14 = Stage 3) |
| 3 | **决策 1 三条生效条件逐条状态化** | 三条**内容**前置**均已满足** ⇒ **契约面**可开工;剩余前置 = ① 本三则 ADR 的**文本定稿**(本段即)② 队列 #13 的 **Task 0 公开来源普查** ⇒ **两者都满足**方可建 change 目录 |
| 4 | 补 **§2.0 选项对比表** / **§8 后果·代价** / **§9 Verification** | per 产出形态契约的 4 必备要素 ⇒ 本次补齐后三者;Verification **只**落在**可执行载体**上(**不**以 ADR 正文为判据对象) |

**② 关联 change**:`cpptlm-cep-collective-plane-base`(✅ 已归档 2026-10-26)· `cpptlm-cep-nvlink-sharp`(队列 #13 · 📋 Planned)· `cpptlm-cep-ualink-inc`(队列 #14 · 📋 Planned)

**③ ⚠️ 决策内容零改动** —— §2 三决策 / §3 R1~R5 + A~D / §3.2 CEP-13 / §3.4 guardrail **逐条不变**;本次只做**形式补齐 + 口径订正 + §4 降级**。
⇒ 「一决策一文档」的**存量越界**(本 ADR 含 3 个决策)按契约「**存量文档不追溯**」⇒ **记 backlog**(per 台账 Entry #169 ⑤)。

**④ 行数口径**:**正文行数** = §1 ~ §9(**不含**修订记录段与 Status Update 追加段)⇒ 本 ADR 正文 = **249 行**(2026-11-03 定稿后实测;口径 = §1 ~ §9 + Verification 章,**不含**修订记录 / Status Update 追加段 / **附录**)≤ 250。

---

## Status Update 2026-11-03 —— 载体形态裁决 **① / ⑤**(队列 #13 规约层的前置)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §9 正文一律不改**。
> 本段**补充** §4 判据摘要的 **S5 / S6** 两条,并登记**载体归属**;自本段起以本段文本为准。
> **台账**: `obj-cep-platform §11` **Entry #176**(`decision`,architect,2026-11-03)。
> **背景**: 队列 #13 的 improvement 列了 **5 项**载体形态决定项;②(`BroadcastNull`)、③、④ 已由既有裁决覆盖,
> **① 与 ⑤** 是规约层 Phase 1 的**唯一**未决前置 ⇒ 本段一次性裁掉。

### E.1 裁决 ① —— `NvlinkAdapter` 的载体归属

| 项 | 裁决 |
|---|---|
| **形态** | **独立头** `nvlink_adapter.hh` —— 与既有 **6** 个 adapter 的**一协议一头**惯例一致 |
| **委托** | **委托** `nvlink_sharp_state_spec.hh` —— **有先例**(`cxl_dcoh_adapter.hh` include `cxl_dcoh_state_spec.hh`) |
| **⚠️ 委托的前提** | state spec 载体 SHALL **自足**(只 include **标准库**)且 SHALL **NOT** 定义 / 暴露 `collective_region_table` 类型 ⇒ **区域表的运行时状态归实现层**(collective 侧)⇒ 载体只保留**纯类型 + 常量 + 纯函数**(输入是「是否命中」/「epoch 是否有效」这类**已判定的事实**)⇒ **委托 ≠ 源端获得区域表可见性** |
| **推论** | 因载体自足 ⇒ collective 侧扫描集合**无需扩展** ⇒ **契约面零改动** |
| **⚠️ 仍缺的判据** | 「源端不获得区域表可见性」**目前无判据**:现有机制只扫**一个** coherence 侧文件,且标识清单**无** `collective_region_table` ⇒ 契约面扩展(扩扫描集合 + 补标识)**归实现层**,**复用既有 id**,**不新造平行 id 体系** |

### E.2 裁决 ⑤ —— epoch / reduce 族

| 项 | 裁决 |
|---|---|
| **reduce 算子族** | **复用既有 `ReduceOp`**(恰 **6** 值: `Sum` / `Min` / `Max` / `BitAnd` / `BitOr` / `BitXor`)⇒ **不新增枚举**、**不改**既有头 ⇒ 载体**不另立**算子族 |
| **SHARP ↔ 算子的逐项支持** | **pending**(公开来源只给**举例**,非闭集)⇒ 载体 SHALL **NOT** 断言任何具体算子可用 |
| **epoch · 平台侧** | = 本 ADR §3.1 **R3 / R4**(**已定稿**)⇒ 载体该部分记 `self-derived` + 非 pending |
| **epoch · 协议侧细节** | **pending**(epoch 状态机与提交时点无公开来源)⇒ 载体 SHALL **NOT** 断言协议侧状态机 |
| **epoch 载体位置** | 与 group 表同处 state spec 载体 ⇒ **不新增载体** |
| **⚠️ 原语集** | **不扩张** —— §2 决策 2 的原语最小集**恰 4** ⇒ 公开来源提到的 **ReduceScatter / AllGather 不在本平台原语集内** ⇒ 记 `pending`,**不**新增原语值 |

### E.3 补充的判据摘要(自本段起,与 §4 的 S1~S4 并列)

| # | 判据 | 可判定行为 |
|---|---|---|
| **S5** | **R1 / S4 的可判定形式:源端不得获得区域表可见性** | 载体 SHALL **NOT** 定义 / 暴露 `collective_region_table` 类型;载体 SHALL **自足**(只 include 标准库)⇒ adapter **委托**载体**不**构成源端可见性;⚠️ 该条的实现侧判据(coherence 侧扫描集合 + 标识清单)**归实现层**并**复用既有 id** |
| **S6** | **⑤ 的锁定:reduce 族复用既有枚举 + epoch 分层** | 载体 SHALL **引用**既有 `ReduceOp`(恰 6 值)且 **SHALL NOT** 另立算子枚举;SHARP 逐算子支持 SHALL 为 **pending**;epoch 平台侧 SHALL 等于 R3 / R4,协议侧 SHALL 为 **pending**;平台原语集 SHALL **维持恰 4** |

### E.4 载体归属

| 载体 / 义务 | 归属 |
|---|---|
| state spec 载体的**纯类型 + 常量 + 纯函数** | **规约层**(队列 #13 第 0 个 change 的 Phase 1) |
| `nvlink_adapter.hh`(独立头 + 委托载体) | **实现层**(队列 #13 第 1 个 change) |
| 区域表的**运行时状态** | **实现层**(collective 侧) |
| coherence 侧扫描集合扩展 + 标识清单补项 | **实现层**(**复用既有 id**) |
| `nvlink_4` 实例声明 | **实现层** |

### E.5 未采取的路径

| 决策点 | 未采 | 理由 |
|---|---|---|
| ① | adapter **不**独立头(并入 state spec) | 与**一协议一头**惯例冲突;混淆「源端」与「collective 侧」边界 |
| ① | state spec **不**自足(include collective 侧取类型) | ⇒ collective 侧扫描集合必须扩展 ⇒ 契约面改动;自足可**零改动** |
| ① | 载体**含**区域表类型 | ⇒ 委托即**获得区域表可见性** ⇒ 违反 R1 与 S4 |
| ⑤ | 另立 SHARP 专用 reduce 枚举 | 与既有 `ReduceOp` **重复** ⇒ 平台两套算子集 ⇒ 与「克隆 + 纸面 caps」禁令同型 |
| ⑤ | 扩原语集加 ReduceScatter / AllGather | 破 §2 决策 2 的「恰 4」;且公开来源**不**支撑其语义 ⇒ 杜撰 |

### E.6 重访触发条件(命中任一 ⇒ **必须**重开本裁决)

1. 实例声明 schema 变更 ⇒ 载体常量面重核;
2. §2 决策 2 的原语集变更 ⇒ E.2 的 reduce 族裁决重核;
3. 平台引入**跨 fabric** collective ⇒ 能力域裁决重核。

---

## Status Update 2026-11-03(#2)—— critic 全面审的修复(**B1** 三选一死结 + **B2** id 归一 + 载体归属澄清)

> **性质**: 追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— **§1 ~ §9 正文与 2026-11-03(#1) 段一律不改**。
> 本段**取代**(#1) 段的 **E.1 / E.2 / E.4 相应行** 与 **S5 / S6** 两条,并**补充** id 归一表(F.3)。
> **台账**: `obj-cep-platform §11` **Entry #178**。

### F.1 **B1 的裁决(采 (c):载体保持自足,且不表达 reduce 族)**

| # | 条款(取代文本) |
|---|---|
| **F.1-a** | 载体 SHALL **自足**:只 include **标准库**,SHALL **NOT** include `collective_plane.hh` |
| **F.1-b** | 载体 SHALL **NOT** 定义 / 承载任何**词表型**语义(`ReduceOp` / `CollectiveOp` / `AccessMode` 等枚举)⇒ **算子族与原语族的唯一真源恒为 `tlm::cep::ReduceOp` / `tlm::cep::CollectiveOp`** |
| **F.1-c** | 载体 MAY 自带**形状型**辅助类型(`group_id` / 地址区间)—— 形状**不承载**语义枚举 ⇒ 不构成「第二套词表」 |
| **F.1-d** | 「算子族**恰 6** 值」的**运行期断言归实现层**(测试可同时 include 两头的头)⇒ 载体**不**自证 6 值 |
| **F.1-e** | 载体 SHALL **NOT** 定义 / 暴露 `collective_region_table` 类型 ⇒ 区域表的**运行时状态归实现层**(collective 侧) |
| **F.1-f** | 因 F.1-a ⇒ collective 侧扫描集合**无需扩展** ⇒ **契约面零改动**(限**规约层**;实现层的 `CP-05` 扩展见 F.4) |

> ⚠️ **为何不是 (a)/(b)**:(a) 载体 include `collective_plane.hh` ⇒ 须扩 CP-01 扫描集合 + `ContractPaths` 结构变更
> + 对**真正冻结**的 `openspec/specs/cep-collective-plane-contract/spec.md` 出 delta ⇒ 代价高于收益;
> (b) 本地重定义 `ReduceOp` ⇒ **两套算子词表** + adapter 边界转换 ⇒ 与 `ADR-CEP-04 §13`「克隆 + 纸面 caps」禁令同型
> (且既有 6 份 `enum class Perm` 已是该代价的实证)。

### F.2 **取代 #1 段 S5 / S6 的规范文本**

| # | 判据 | 可判定行为 |
|---|---|---|
| **S5** | **R1 / S4 的可判定形式:源端不得获得区域表可见性** | 载体 SHALL **NOT** 定义 / 暴露 `collective_region_table` 类型(F.1-e)· 载体 SHALL **NOT** include `collective_plane.hh`(F.1-a)⇒ adapter **委托**载体**不**构成源端可见性;⚠️ 实现侧判据(coherence 侧扫描集合 + 标识清单补 `collective_region_table`)**归实现层**并**复用既有 id `CP-05`** |
| **S6** | **⑤ 的锁定:算子族唯一真源 + epoch 分层** | 载体 SHALL **NOT** 定义任何 reduce 算子枚举 / 常量;**唯一真源** = `tlm::cep::ReduceOp`(恰 6 值)(F.1-b/d)· SHARP 逐算子支持 SHALL 为 **pending**;epoch 平台侧 SHALL 等于 **R3 / R4**,协议侧 SHALL 为 **pending**;平台原语集 SHALL **维持恰 4** |

> ⚠️ **S6 的运行期断言归实现层** ⇒ 规约层 SHALL **NOT** 把「恰 6 值」写成载体自证的判据(载体无从自证)。

### F.3 **判据 id 归一表(**B2**;同一义务不得有两个 id)**

| 义务 | **唯一归因 id** | 登记处 | 备注 |
|---|---|---|---|
| R1:opcode 永不作分类器 | **`CP-OPCODE-CLASSIFIER`** | `ADR-CEP-03 §9` V8 | 规约层的 `NSS-01` **降级为 Scenario 标题 / 离线标签**,不再作归因 id |
| 源端不得持有区域表可见性 | **`CP-05`** | `ADR-CEP-03` 本段 S5 + `ADR-CEP-01` 附录 | 实现层扩展 coherence 侧扫描集合时**复用**该 id |
| R4 epoch 语义双向 | **`CP-EPOCH-SEMANTICS`** | `ADR-CEP-03 §9` V6 | `NSS-02` 降级为标签 |
| R3 drain 失败不建 epoch | **`CP-DRAIN-FAIL`** | `ADR-CEP-03 §9` V4 | `NSS-03` 降级为标签 |
| R5 divert 计数 + fail-stop | **`CP-DIVERT-STOP`** | `ADR-CEP-03 §9` V5 | `NSS-05` 降级为标签 |
| collective VC 预留状态 | **`CP-03`** | `ADR-CEP-01` SU(D.4) | `NSS-07` 降级为标签 |
| 原语最小集恰 4 | **`CP-PRIM-SET`** | `ADR-CEP-03 §9` V2 | — |
| 调用边恒为零 | **`CP-01`** | `ADR-CEP-01 §7` V1 | — |
| canonical 10 流零 diff | **`CP-02`** | `ADR-CEP-02 §8` V1 | — |
| 注销后回归 coherence 域 | **新建 id**(登记于队列 #13 规约层 `design §3` + `tasks 3.2`) | 规约层 | 原 `NSS-04`;**单一登记**,不在本 ADR 重复登记 |
| ordering A/B/C/D 可判定 | **新建 id**(同上) | 规约层 | 原 `NSS-06` |
| 能力域 = 节点内 | **新建 id**(同上) | 规约层 | 原 `NSS-08` |
| 逐格 `basis` 核对 | **新建 id**(同上) | 规约层 | 原 `NSS-09` |
| partial failure 显式建模 | **移交实现层** | `ADR-CEP-01 §7` V5 | 载体已在队列 #5 交付 ⇒ id **待实现层建立** |
| `V6-c` 前提约束 | **移交实现层** | 本段 S5 + `ADR-CEP-01` SU | 载体 = 队列 #13 规约层 / 实现层的载体清单 |

> ⇒ 本 ADR 附录「归因 id **待 #13 建立**」的清单**以本表为准**:已建立者**列出 id**;移交实现层者**指名归属**。

### F.4 **载体归属(取代 #1 段 E.4 的相应行)**

| 载体 / 义务 | 归属 | 归因 id |
|---|---|---|
| state spec 的**纯类型 + 常量 + 纯函数**(SHALL NOT 定义词表型语义) | **规约层** | 新建 id 登记于规约层 `design §3` |
| `nvlink_adapter.hh`(独立头 + 委托载体) | **实现层** | — |
| 区域表的**运行时状态** | **实现层**(collective 侧) | — |
| coherence 侧扫描集合扩展 + 标识清单补 `collective_region_table` | **实现层** | **复用 `CP-05`**(不新造) |
| 「算子族恰 6 值」的运行期断言 | **实现层**(F.1-d) | 复用 `CP-05` 之外的实现层断言面,**不**由载体自证 |
| `nvlink_4` 实例声明 | **实现层** | — |

### F.5 **V3 与 `NSS-01` 的区分(S2)**

`ADR-CEP-01 §7` **V3**(barrier 分类 litmus:地址 key ⇒ Ordering / 组 key ⇒ Collective)与规约层 `NSS-01`
(`classify_at_fabric_ingress`:reduce / coherence 分流)**不是同一判据** ⇒
⇒ **V3 不由 `NSS-01` 覆盖**;V3 的载体 owner = 队列 #13 **实现层** 的 barrier 分类函数(名字由实现层定),
其归因 id **待实现层建立**;`NSS-01` 的新 id 只覆盖 reduce / coherence 分流。

### F.6 **冻结面真源澄清(S4)**

**真正冻结**的产物是 `openspec/specs/cep-collective-plane-contract/spec.md`(M1 清单内);
`include/tlm/cep/collective_planes.json` / `.schema.json` / `collective_plane_contract.hh` **不在** M1 冻结集内
(且 `collective_planes.json` 自述「诞生即不冻结」)⇒
⇒ **选 (a)(载体 include `collective_plane.hh`)的代价主要在 main spec 的 delta,而非代码文件**;
本段采 (c) ⇒ 规约层**不触发**该 delta。
