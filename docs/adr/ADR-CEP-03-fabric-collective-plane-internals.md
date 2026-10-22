# ADR-CEP-03: Fabric Collective Plane 内部接口与跨平面契约

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)
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

### 决策 1:3 个生效条件(per A6 拍板)

**任何 SHARP / UALink 插件落地前必成**:
1. **跨 plane ordering/visibility 契约必须先成文**(本 ADR §3 详化)
2. **集合 plane guardrail**:操作只允许作用于 registered collective region(per ADR-CEP-01 §3.1 抓取点 1 派生)
3. **coherence `barrier` 流重钉为 Ordering Point**(per `ADR-CEP-02` Decision 3)

### 决策 2:Fabric Collective Plane 抽象接口

```systemverilog
// Fabric Collective Plane —— 与 Coherence Interface 平行的独立抽象
// 完成纪律: group-scoped (完成 = 全组 ack / 全组到达)

interface CollectivePlaneIf;

  // ---- group 生命周期 (慢路径, 管理语义; 这部分放 control plane 是正当的) ----
  function group_id_t group_create(member_set_t members, group_attrs_t attrs);
  function void       group_destroy(group_id_t gid);
  // SHARP 式注册: 注册时触发 coherence drain (跨 plane 契约的入口)
  function status_t   group_register_region(group_id_t gid, addr_range_t range,
                                            access_mode_t mode);

  // ---- 数据路径原语 ----
  // CollectiveReq 字段即 plane 的"通用词汇":
  //   { group_id, op, tag(phase/round), src_member, payload_desc(addr+len | inline),
  //     reduce_op, dtype, multicast_mask }
  // 注意: group_id 在此是一等公民 —— 它对这个 plane 的通用性
  //       等价于 address 对 coherence plane 的通用性
  task send(CollectiveReq req);

  // 完成语义: 不是单点 ack, 而是 GroupCompletion(gid, tag)
  task recv(output CollectiveEvt evt);
  //   CollectiveEvt ::= Result(gid, tag, payload) | Completion(gid, tag) | Fault(gid, culprit, cause)

  // ---- barrier = 相位转换原语 ----
  task collective_barrier(group_id_t gid, phase_t phase);
  // 同相位内跨参与者无序; 相相间有序 —— 这是本 plane 唯一的 ordering 承诺

  // ---- 能力协商 (插件描述符的一部分) ----
  function cap_t query_capability(op_t op, size_t max_bytes);
  //   CHI 插件 → {multicast_sync} (per ADR-CEP-01 Decision 1 裁剪实例)
  //   NVLink 插件 → SHARP op 集
  //   UALink 插件 → 全集合族
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
  - 触发时机: `group_register_region(gid, range, mode=NC_DROP_CACHEABLE)`
  - 动作:对 range 内所有 cache line,调用 Coherence Interface `request(invalidate)` 流(per ADR-CEP-02 canonical 10 流清单)
  - 完成判定:所有 cache line invalidate 完成 → region 在 coherence plane 降级为 non-cacheable
  - 后续:对该 region 的 coherence `request` 流命中 → 返回 not-allowed 错误(CEN-Ib7 扩展)
  - 复位:group_destroy 触发 R4 失效化,region 回归 coherence-cacheable

不变量(CEP-13):
  - 同一地址不得同时"SHARP 已注册"且"coherence-cacheable"
  - drain 失败 → group_register_region 返回 error,不创建 epoch
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

## 4. SHARP 插件状态机雏形(per ADR-CEP-03 后续 OpenSpec 展开)

**说明**:以下占位符为架构师勾勒状态机雏形,**类型未细化**。builder 阶段实施时按 C++ 实施细节展开。

```cpp
// 类型为占位符,builder 阶段细化
void nvlink_sharp_create_group(member_set_t members, group_id_t& gid) {
  // 1. 生成 group_id(全局唯一)
  // 2. 注册 group metadata(NVSwitch SPU table entry)
  // 3. 初始化 group_epoch_valid = false(待 register_region 完成)
}

status_t nvlink_sharp_register_region(group_id_t gid,
                                      addr_range_t range,
                                      access_mode_t mode) {
  // 1. 在 collective_region_table 添加 (range → gid, mode)
  // 2. R3 drain:对 range 内所有 cache line 触发 coherence plane 的 invalidate/flush
  //    (或将该区域在 coherence plane 降级为 non-cacheable,per §3.2 CEP-13)
  // 3. R5 observability:记录 divert_count++
  // 4. 设置 gid.epoch_valid = true
}

divert_t fabric_ingress_classify(ck_req_t req) {
  // 1. 查 collective_region_table:(req.addr ∩ table_hit)
  // 2. 查 group_epoch_valid:gid.epoch_valid
  // 3. 双向命中 → divert 到 Collective Plane (R5: counter++)
  // 4. 命中但 epoch invalid → 放行(per R4 正常服务,等待 epoch commit)(per oracle B2-7 修订:伪码原引 R5 错误,改引 R4)
  // 5. 未命中 → 走 coherence `request` 流继续
}
```

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
  - `cpptlm-cep-collective-plane-base`(P2 实施 change,1 KPI slot)
  - `cpptlm-cep-nvlink-sharp-adapter`(P3 实施 change,1 KPI slot,需本 ADR §3 契约先成)
  - `cpptlm-cep-ualink-inc-adapter`(P4 实施 change,1 KPI slot)

---

## 7. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| 内部接口雏形 | `CollectivePlaneIf` (per §2 Decision 2) | ✅ 已拍板 |
| 跨平面契约 | 5 条防误分类规则 + ordering 契约 + registered region guardrail | ✅ 已拍板 |
| group_id 归属 | Collective Plane 一等公民(不进 Coherence Interface) | ✅ 已拍板 |
| AllReduce 归类 | = REDUCE + BROADCAST 复合,不列原语 | ✅ 已拍板 |
| NullCollectivePlane | "启停干净"验收载体 | ✅ 已拍板 |
| Bundle 路径 | 沿用 `bundles::coherence_bundles_tlm.hh` + 新增 CollectiveReq/Evt | ✅ 已拍板 |
| 3 个生效条件 | 跨 plane 契约先成 / guardrail / barrier 重钉 | ✅ 已拍板 |
| non-cacheable 语义 | §3.2 CEP-13 首次定义(06-bis §5 无此概念) | ✅ 首次定义(per B10) |
| CHI 插件能力 | `{multicast_sync}`(per ADR-CEP-01) — 非"全空"(per S7 修订) | ✅ 已拍板 |

---

## 8. v0.2 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
