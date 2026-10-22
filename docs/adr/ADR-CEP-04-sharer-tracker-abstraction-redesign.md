# ADR-CEP-04: Sharer Tracker 抽象重构(D-5 重构 + caps descriptor)

> **状态**: 🟡 起草中(v0.6)
> **日期**: 2026-10-19(初版)/ 2026-10-20(v0.5 修订)
> **作者**: CppTLM build agent(architect mode)
> **关联真源**:
> - 真源 1:`06-inter-module-network.md` v0.4 §15.4 P2.0-P5(per `ADR-CEP-00A §3.1` 实证 — **无 P6**,per oracle v0.3 B3-7 修订)Coherence 演进 seam
> - 真源 1:`06-bis-coherence-as-network-extension.md` v0.3 §7.3-§7.7 阶段范围(per `ADR-CEP-00A §3.2` 实证 — §7.8 实为 KPI 串行约束,非阶段范围,per oracle v0.3 B2-2 修订);**`§2.3` "D-5" 接口不存在**,全文 grep "sharer" / "D-5" 零匹配,per oracle 评审 B5
> - 真源 1:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流清单)
> - 真源 3:同系列 ADR 互引见本 ADR 文末「## N. 关联 ADR / 后续」节

---

## 1. 背景

### 1.1 关于"06-bis §2.3 D-5 接口"伪引的修订

**经 oracle 评审实证**:`06-bis-coherence-as-network-extension.md` v0.3 全文 grep "sharer" / "D-5" 零匹配;`§2.3` 是"命名空间一致性",**不存在** Sharer Tracker 接口雏形。

**结论**:Sharer Tracker 抽象(`SharerTrackerIf` / `lookup` / `snoop_begin` / `snoop_fold` 等)**由本 ADR 首次定义**。06-bis v0.3 §7.5 P3 SnoopFilter 范围被本 ADR 完整继承(per ADR-CEP-00 §3 supersede 范围映射表 + `ADR-CEP-00A §3.2` 实证)。

### 1.2 根因

如上所述,Sharer Tracker 接口 D-5 雏形不存在,但事实上接口语义**隐式假设了"全局权威答案"**:

- `lookup() → sharer_bitmap / target_nodes / can_complete` 三个返回字段假设"存在一个节点能同步给出全局完备答案"
- 这对 CHI HN-F(目录/SF 在 home)成立
- 对 CXL 对称模式(DCOH 是 per-node 局部视图,全局答案由 BISnp 往返**涌现**)不成立
- 对 TileLink(权限沿树递归,lookup 可能向上 escalate)也不成立

`lookup` 假设单次同步回答全局问题 — 在 5 种实现里(CHI HN-F / Sparse-SF / CXL 对称 DCOH / TileLink Tree / BroadcastNull),DCOH 和 TileLink 会**被迫对返回字段善意撒谎**,造成"语法统一、语义分裂"(最隐蔽的抽象泄漏)。

### 1.3 真实解决的两条独立原则

1. **lookup 的承诺**:从"全局答案"降级为"作用域内的局部视图 + 显式的完备性/权威性声明"
2. **snoop 生命周期**:从"单次 complete 事件"改为"begin → fold → 归零派生 complete"

DCOH 条目和目录条目**结构上是同构的**(都是 per-line → per-node 状态矩阵),差异全部收敛到 4 个可声明的属性(scope / completeness / authority / aggregation_at)。

---

## 2. 决策

### 决策 1:α′ = (α) + (γ′) + D-5 重构(三件套缺一不可)

**符号定义**(per oracle S2 修订,首次定义于本 ADR):

| 符号 | 含义 | 出处 |
|------|------|------|
| **α** | 单抽象接口 + N 实现(N 种具体协议策略) | oracle A4 |
| **α**' | α + 命名复用(本 ADR §4 lookup 签名)+ 三明治挂载(per `ADR-CEP-05`)| oracle A4 / A7 |
| **β** | 拆 Central/Distributed interface tree | **明确否决**|
| **γ** | bool `is_distributed` flag | **明确否决**|
| **γ**' | caps descriptor struct 替代 bool | oracle A4 |
| **α**′ | **本 ADR 决策** = α(单接口 + N 实现)+ γ'(caps descriptor) + D-5 重构(三件套)| oracle A4 |

**α′ 三件套**:
- **(α) 单抽象接口 + 5 种实现**:FullMap / Sparse-SF / DcohSymmetric / TreeManager / BroadcastNull
- **(γ′) caps descriptor 替代 bool**:4 属性 struct 替代 `is_distributed` bool(per §2 Decision 3)
- **D-5 重构**:lookup → 结果枚举(ANSWER/FORWARD/ESCALATE/BROADCAST)+ 规范 intent;snoop_complete → begin/fold 生命周期;can_complete → scoped 语义(per §3)

### 决策 2:DCOH 完备性契约钉死(正确性命门)

**consistency contract**:
- `CONSERVATIVE` = 绝不漏 sharer(可超集)
- `PARTIAL` 必须声明 broadcast fallback(否则 = 失效实例,禁止使用)
- 禁止无 fallback 的 PARTIAL

**理由**:CXL 对称模式下 hint 若可能漏真实持有者,则"R 归零即 complete"不健全。

### 决策 3:caps descriptor struct(替代 bool,第一天就位)

```cpp
// 4 属性 enum + struct(per oracle B9 修订,enum 字面值与 5×4 表完全对齐)
enum class Scope         { SYSTEM, SUBTREE, LOCAL_MEMORY_REGION, NONE };
enum class Completeness  { EXACT, CONSERVATIVE, PARTIAL };
enum class Authority     { GLOBAL, LOCAL_HINT, LOCAL, NONE };  // B9 修订:补 LOCAL/NONE
enum class AggregationAt { HOME, REQUESTER, TREE_PARENT, NONE };  // B9 修订:补 NONE

struct SharerTrackerCaps {
  Scope          scope;
  Completeness   completeness;
  Authority      authority;
  AggregationAt  aggregation_at;
};
```

**禁止**:协议插件代码中出现任何 `is_distributed` / `is_central` / `is_global` bool flag(立法禁止,第一天就位)。

**5 × 4 × 4 × 4 mapping 表 enum 字面值收敛**:

| 实现 | scope | completeness | authority | aggregation_at |
|------|-------|--------------|-----------|----------------|
| **FullMapDirectory**(CHI HN-F)| `SYSTEM` | `EXACT` | `GLOBAL` | `HOME` |
| **SparseSnoopFilter**(CHI SF / CXL 2.0 Host SF)| `SYSTEM` | `CONSERVATIVE` | `GLOBAL` | `HOME` |
| **DcohSymmetric**(CXL 3.0 对称模式,per-device DCOH)| `LOCAL_MEMORY_REGION` | **`CONSERVATIVE`(必须)** | `LOCAL_HINT` | `REQUESTER` 或 `HOME`(per §3.5 二元裁决)|
| **TreeManager**(TileLink ClientMetadata)| `SUBTREE` | `EXACT` | `LOCAL` | `TREE_PARENT` |
| **BroadcastNull**(无目录)| `NONE` | `EXACT` (degenerate, "never needs care")| `NONE` | `NONE` |

**不变量**(INV-CEP-04-IC1):caps struct 字段值必须 ∈ enum 字面值(机器可检查,per oracle B9 抓取点 + oracle SHOULD-FIX #7 — `Hazard_*` 枚举同标准)。

---

## 3. D-5 重构详化(per oracle A4 §2,本 ADR 首次定义)

### 3.1 lookup 结果改为显式结果枚举(per oracle S6 修订:统一签名)

```systemverilog
// canonical lookup 签名(per oracle S6 修订:合并两版)
typedef enum {
  LookupResult_ANSWER_HERE,    // 本地视图充分回答(可 +LocalView)
  LookupResult_FORWARD_TO,     // 需要向指定目标转发(可 +targets)
  LookupResult_ESCALATE_UP,    // TileLink 向上递归(可 +tree_parent)
  LookupResult_BROADCAST       // 无目录,需广播(无附加字段)
} LookupResult;

typedef enum {
  Hazard_NONE,
  Hazard_STALL,    // 等待 in-flight snoop 完成后重试
  Hazard_CHAIN     // 加入同一 in-flight snoop 的 expected_responders
} LookupHazard;

// canonical lookup 签名(per oracle S6 修订唯一版本)
interface SharerTrackerIf {
  task lookup(input addr, input intent,
              output LookupResult result,
              output LocalView view,
              output LookupHazard hazard);
  // hazard != NONE 时 view 无意义,调用方按 hazard 类型重试或 chain
  // intent 用规范意图(见 §3.2),而非协议 opcode
};
```

### 3.2 intent 用规范意图(替代协议 opcode)

`intent ∈ {want_read, want_write, want_own, evict}`:

- CHI ReadUnique / CXL RdOwn / TL NtoT 都映射到 `want_own`
- 协议特有 op 放 opaque 扩展字段(不破"协议无关性"原则 — `ADR-G04` 不存在,沿用描述性原则)

### 3.3 snoop 生命周期重构(替代 snoop_complete)

```systemverilog
typedef logic [15:0] snoop_handle_t;

interface SharerTrackerIf {
  // ---- begin/fold 生命周期 ----
  task snoop_begin(input addr, input ResponderSet expected_responders,
                   output snoop_handle_t handle);
  task snoop_fold(input snoop_handle_t handle, input node_id_t node_id,
                  input resp_state, dirty, data_supplied,
                  output ResponderSet remaining_mask,
                  output MergedState merged_state);
  // complete = remaining_mask == 0 的派生事件
  // 3 种协议(CHISF / CXL Birsp / 广播型)用同一台机器
  // —— 只是 expected_responders 来源不同
};
```

**ResponderSet 来源差异**:
- CHI SF:home node 持有 SF 向量
- CXL 对称:本地 DCOH hint 向量(hint miss 时退化为 broadcast — 这就是 completeness 必须 CONSERVATIVE 的原因)
- 广播型:全集

### 3.4 被动观察(BISnp 接收侧)

```systemverilog
interface SharerTrackerIf {
  // ---- 被动记账(不参与判断)----
  task observe_snoop(input addr, input node_id_t from_node,
                     input resulting_local_state);
  // 远端设备收到 BISnp 后更新自己 DCOH 中"peer 对该线的认知"
  // 这是对端 tracker 的被动写入,不是订阅
};
```

**禁止加 `subscribe_remote_snoop()`**:传输通道概念泄进 tracker 平面。

### 3.5 CXL 对称模式 aggregation_at 二元(P2P vs HDM home,per oracle A4 抓取点 3)

**spec 写死的裁决**(不留给实现):

| 场景 | aggregation_at | 理由 |
|------|---------------|------|
| **HDM P2P (device-to-device data pull)** | `REQUESTER` | 请求者驱动 fold 状态;其他设备发 BIRsp |
| **HDM home(归属设备作为 home)** | `HOME` | 归属设备 RID;与 CHI HN-F fold 模式一致 |

**实现**:Spec 显式声明上述二元,**协议插件代码中不得自行切换**。`DcohSymmetric` 实现按请求类型动态选择,但在 spec 文档中以查询表固化。

### 3.6 禁止的旧接口(per oracle B5 修订:本 ADR 首次定义,无 supersede 对象)

- ❌ `lookup() → sharer_bitmap / target_nodes / can_complete`(平铺返回假设全局答案) — **本 ADR 首次定义,无 supersede**
- ❌ `snoop_complete()`(单次 complete 事件,无法表达 fold 生命周期) — 同上
- ❌ 任何 `is_distributed` / `is_central` bool flag — 立法禁止
- ❌ `subscribe_remote_snoop()`(把传输通道概念泄进 tracker)

**关于 supersede 标注**:**06-bis §7.8 = KPI 串行约束,非阶段范围**(per `ADR-CEP-00A §3.2` 实证);06-bis P3 SnoopFilter 范围在 §7.4,非 §7.8。supersede 实施只针对 06-bis §7.4 P3 SnoopFilter 范围(per ADR-CEP-00 §3 supersede 范围映射表),不针对 §7.8(KPI)。本 ADR 顶部不引用 ADR-DGPU-13/14(已 superseded by 06 v0.4,与本 ADR 平行不冲突)。

---

## 4. 同地址并发 lookup hazard 语义(per oracle A4 抓取点 4 + S5 修订:补 SparseSnoopFilter)

**关键问题**:snoop 在飞期间第二个同地址 lookup 必须返回 hazard/stall/chain 之一,否则两个 begin 会创建两个 pending 互相踩。

**5 种实现的 hazard 行为契约**(per oracle S5 修订,补 SparseSnoopFilter + oracle SHOULD-FIX #7 字面值收敛):

| 实现 | Hazard 行为 | 理由 |
|------|------------|------|
| **FullMapDirectory**(CHI HN-F)| `Hazard_STALL` | HN-F 内部 busy bit |
| **SparseSnoopFilter**(CHI SF / CXL 2.0 Host SF)| `Hazard_STALL` | SF busy 位与 HN-F 同构 |
| **DcohSymmetric**(CXL 对称)| `Hazard_STALL` | 本地 busy 位(per-device) |
| **TreeManager**(TileLink)| `Hazard_CHAIN` | 向上 escalate 时,parent chain 加入 |
| **BroadcastNull**(无目录)| `Hazard_NONE` | 无状态,无需等待 |

(per oracle SHOULD-FIX #7:`Hazard_*` 枚举字面值统一 — 不变量 INV-CEP-04-IC1 覆盖所有 enum)

---

## 5. lookup 的同步性假设

**关键问题**:TileLink `ESCALATE_UP` 意味着 lookup 可能异步重入。Ordering Point 平面是否假设 lookup 同步幂等?

**决策**:lookup 是**同步调用**,但 `ESCALATE_UP` 时返回 `LookupResult_ESCALATE_UP + tree_parent`,由 caller(ProtocolEngine 或 ProtocolEngine + Ordering Point)负责向上发起后续 lookup 调用。**Sharer Tracker 不持有异步重入状态**。

**契约**(INV-CEP-04-IC2):Sharer Tracker 内部所有 `lookup` / `snoop_begin` / `snoop_fold` 调用幂等(同一 addr + 同一 intent + 同一外部状态 = 同一返回)。

---

## 6. 条目生命周期事件

**统一契约**(防止分布式 peer 之间对 expected_responders 的认知漂移):

| 事件 | 触发 | Sharer Tracker 内部动作 |
|------|------|------------------------|
| `allocate-on-first-forward` | 第一次 lookup 返回 ANSWER_HERE 或 FORWARD_TO | 建立 entry |
| `merge-on-snoop-begin` | snoop_begin(addr, R) | entry.pending_responder_count = popcount(R) |
| `decrement-on-snoop-fold` | snoop_fold(handle, ...) | entry.pending_responder_count -= 1 |
| `retire-on-zero-sharers` | sharer_bitmap 归零 + dirty=false | 释放 entry |
| `retire-on-zero-pending` | snoop_fold 后 remaining_mask == 0 | 释放 pending 状态,触发 complete |

**不变量**(INV-CEP-04-IC3):5 个事件名 + 时序契约在所有 5 种实现中一致,防止分布式 peer 漂移。

---

## 7. 抓取点(评审检查单,按风险排序)

**高 / 正确性**:
1. **DCOH 完备性契约钉死**:CONSERVATIVE = 绝不漏 sharer,PARTIAL 必须 broadcast fallback,禁止无 fallback 的 PARTIAL
2. **can_complete 重定义写 truth table**:按 5 种实现 × 各返回枚举逐一列出含义
3. **聚合点所有权在 CXL 对称下显式裁决**:P2P → REQUESTER,HDM home → HOME(spec 写死,不留给实现)

**中 / 并发危害**:
4. **同地址并发 lookup hazard 语义**:snoop 在飞期间第二个 lookup 返回 STALL 或 CHAIN(per oracle S5 修订:5/5 实现覆盖)
5. **lookup 同步性假设**:Sharer Tracker 不持有异步重入状态,ESCALATE_UP 由 caller 负责

**低 / 演进**:
6. **条目生命周期事件统一**:5 个事件名 + 时序契约(防止 peer 漂移)
7. **立法禁止 bool flag**:caps struct 从第一天就位

---

## 8. 关联 ADR / OpenSpec

- 取代:`06-bis-coherence-as-network-extension.md` v0.3 §7.4 P3 SnoopFilter 范围(将被 supersede 段,per ADR-CEP-00 §3 + v0.4 B1 实证:§7.8 非阶段范围)
- 配套:`ADR-CEP-01-fabric-collective-plane.md`(旁路平面)
- 配套:`ADR-CEP-02-coherence-barrier-semantics-clarification.md`(canonical 10 流清单)
- 关联:`ADR-CEP-00-platform-charter-and-governance.md`(归口)
- 后续 OpenSpec(架构师不写,builder 阶段):
  - `cpptlm-cep-sharer-tracker-base`(P2 实施 change,1 KPI slot)
  - `cpptlm-cep-sharer-tracker-impls`(P3 实施 change,FullMap + Sparse-SF)
  - `cpptlm-cep-sharer-tracker-cxl-dcoh`(P4 实施 change,需 §2 Decision 2 完备性契约先成)
  - `cpptlm-cep-sharer-tracker-tilelink-tree`(P5 实施 change)

---

## 9. 决策汇总

| 决策项 | 决策 | 状态 |
|--------|------|------|
| D-5 重构 | α′ = α + γ′ + D-5 重构(三件套) | ✅ 已拍板 |
| 接口实现数 | 单接口 + 5 种实现 | ✅ 已拍板 |
| caps 替代 bool | caps descriptor struct 第一天就位 | ✅ 已拍板 |
| DCOH 完备性 | CONSERVATIVE 必须;PARTIAL 必须 broadcast fallback | ✅ 已拍板 |
| 5 × 4 mapping | per §2 Decision 3 表格(per B9 enum 收敛) | ✅ 已拍板 |
| lookup 结果 | 结果枚举(ANSWER/FORWARD/ESCALATE/BROADCAST) | ✅ 已拍板 |
| snoop 生命周期 | begin/fold 替代 snoop_complete | ✅ 已拍板 |
| intent 用法 | 规范意图(want_read/want_write/want_own/evict) | ✅ 已拍板 |
| 同地址 hazard | `Hazard_STALL` / `Hazard_CHAIN` / `Hazard_NONE` 返回值(5/5 实现覆盖,per S4 字面值统一)| ✅ 已拍板 |
| lookup 同步性 | 同步,ESCALATE_UP 由 caller 处理 | ✅ 已拍板 |
| 条目生命周期 | 5 个事件名 + 时序契约 | ✅ 已拍板 |
| CXL aggregation 二元 | P2P → REQUESTER,HDM home → HOME(spec 写死) | ✅ 已拍板(per A4 抓取点 3)|
| 符号定义 | α/α'/β/γ/γ'/α′ 首次定义于本 ADR §2 Decision 1 | ✅ 首次定义(per S2) |

---

## 10. v0.2 修订记录

> **变更史**:见 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 台账(授权记录单一真源)。
## 11. v0.5 补齐不变量

**INV-CEP-04-IC5**(新增):`fabric_ingress_classify` 中 "table 命中 + epoch invalid" 窗口 = 正常服务放行(per R4 正常服务),非 fail-stop。

**INV-CEP-04-IC6**(新增):5 种实现 hazard 行为契约 `Hazard_NONE` / `Hazard_STALL` / `Hazard_CHAIN` 全字面值收敛。

**INV-CEP-04-IC7**(新增,2026-10-20 落位):**token 类协议下,Sharer Tracker 不跟踪 sharer set**(token 由数据副本携带权限);实现上等价 `BroadcastNull` 或 tracker 不参与。
- **来源**:`ADR-CEP-17 §2 Decision 3`;一手依据 gem5 Ruby `MOESI_CMP_token` 的 token 语义 + HeteroGen "GPU"(Spandex)协议同样不做 sharer tracking 式中心目录(per 调研 v0.3 §3.5)
- **落位说明**:此前该条仅在 `ADR-CEP-17 §3.1` 声明,未落 owner ADR —— 导致 `07 号 §4` 的"36 条单一真源"名存实亡。本条为**补落**,非新增。

**命名空间统一**:
- 跨 ADR 引用:**统一加 `INV-` 前缀**(`INV-CEP-04-IC1` 而非 `CEP-04-IC1`)
- 本地定义:可裸名,但建议统一加 `INV-`
- `CEN-Ib1~10` / `INV-SC1~6` 保留原命名(分别属 ADR-CEP-02 / ADR-CEP-05)
- `INV-CEP-NN-ICx`:跨 ADR 引用 canonical 形式(per `NUMBER-MAP §2 Decision 2`)

---

**版本**: v0.3 草案
**最后更新**: 2026-10-20
**批准状态**: 🟡 待 Oracle v0.5 二次复审
**维护者**: CppTLM build agent(architect mode)
