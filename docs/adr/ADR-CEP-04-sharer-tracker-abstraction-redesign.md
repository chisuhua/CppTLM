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

## 10. Status Update(2026-10-25,per 台账 Entry #62)

> **授权**:Entry #62(队列 #4 建 change + 3 项 DRI 裁决 + S1-7 (β) 同批补入)。
> **口径**:本节是 **§8「后续 OpenSpec」的 4 个 change 名**的**当前状态** —— **§8 正文零改动**(per ADR 不可变 + Status Update 先例)。

### 10.1 §8 的 4 个后续 change 名 —— **已作废(被 obj §4.0 重排取代)**

| 项 | 内容 |
|---|---|
| **原文**(§8 末段) | 「后续 OpenSpec(架构师不写,builder 阶段):`cpptlm-cep-sharer-tracker-base`(P2)· `cpptlm-cep-sharer-tracker-impls`(P3,FullMap + Sparse-SF)· `cpptlm-cep-sharer-tracker-cxl-dcoh`(P4)· `cpptlm-cep-sharer-tracker-tilelink-tree`(P5)」 |
| **§8 正文** | **零改动**(逐字保留) |
| **§8 当前状态** | ⛔ **已作废** —— 4 个 change 名**永久失指**。**唯一**的后续 OpenSpec = **`cpptlm-cep-directory-ordering-plane`**(单 change,1 KPI slot) |
| **落地位置** | `openspec/changes/cpptlm-cep-directory-ordering-plane/`(🟡 Proposed,2026-10-25 建目录;planner 阶段产出 proposal + design + 3 个 delta capability) |
| **作废理由** | ① 与 `ADR-CEP-00 §2 Decision 3` 的「11 个 change,串行 11 季度」算术冲突(队列 #4 为**单一** change,其名不在 §8 的 4 名之列);② obj §4.0 是实施路径唯一权威(per 队列 #3 同型先例,`ADR-CEP-05 §14` Entry #58) |

**替代与承接形态**(per 2026-10-25 DRI 裁决):

| # | 裁定 |
|---|---|
| **范围切小** | 队列 #4 承接「**SharerTrackerIf 单接口 + caps descriptor(第一天就位) + D-5 全量契约 + FullMapDirectory 唯一行为实现**」;**其余 4 种实现**(`SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull`)**跟随各自的消费队列**(obj §4.0 队列 #6 ~ #9 的各自协议族)—— 即「**单接口 + N 实现,按 N 个协议族载体**」 |
| **FullMapDirectory 选型** | `completeness=EXACT` 无 broadcast fallback 分支;07 号 §5.1.2 表 3 个 Ruby 实例的共同选型;IC2/IC3 生命周期**必须有 ≥1 个真实实现才可执行断言**(per 队列 #3 G3 先例) |
| **承接的 Stage 1 前置** | S1-1 ②建图层(装配期校验)+ S1-7 (β) 重评估(同批含 INV-CEP-08-IC2) + S1-2/S1-3/S1-4(owner 重指向,per Entry #61) |
| **不承接** | S1-5 表体 + contract test(归队列 #6)· M-4 行为侧(已由队列 #3 落成 `cep-vc-mapping-contract`,登记行保留以免孤儿项失主,per Entry #61) |

### 10.2 未变更项(明示)

§2 决策 1 ~ 3(α 权限三件套 / DCOH 完备性契约 / caps descriptor)· §3 D-5 重构 6 小节 · §4 同地址并发 lookup hazard 契约表 · §5 lookup 的同步性假设 · §6 条目生命周期事件 · §7 抓取点 · `INV-CEP-04-IC1/IC2/IC3` —— **零改动**。

> **变更史**:§8 原文逐字引用见上表「原文」行;本条作废授权与理由见台账 **Entry #62** —— 过程记录归台账,不在本 ADR 内累积。

## 13. Status Update(2026-10-29,per 台账 Entry #96)—— R-1 裁决:第 6 种 SharerTracker 实现 `HierarchicalDirectory` + `aggregation_at` 新枚举值 `HIERARCHICAL`

> **性质**:追加式修订(per AGENTS.md「ADR 不可变 + Status Update」先例)—— §1 ~ §12 正文一律不改,仅登记**一项决策**。
> **触发**:ArchForge `32 号 §9.1:737-741` 自陈的 **R-1** —— NSA 需要 `scope=SYSTEM` + `completeness=EXACT` + `authority=GLOBAL` + **多级 `aggregation_at`**(L1 per-GPC / L2 per-GPU / L3 per-Compute-Tray),而本 ADR §2 决策 3 的 `AggregationAt` 枚举只有 `HOME` / `REQUESTER` / `TREE_PARENT` / `NONE` —— `HOME` 是**单点**、`TREE_PARENT` 是**单父**,**无「多级」表达** ⇒ D-FABRIC 域在 CEP 内不可表达(per `32 号 §13` RK-3)。
> **关联**:台账 `obj-cep-platform §11` **Entry #96**;ArchForge `32 号 §9.1`(缺口自陈)/ `§13` RK-3(降级方案)。

### 13.1 决策 —— 新增第 6 种实现 `HierarchicalDirectory`,`AggregationAt` 增枚举值 `HIERARCHICAL`

**选项对比**:

| 方案 | 结论 | 理由 |
|---|---|---|
| **A′** 新增第 6 种实现 `HierarchicalDirectory` + `AggregationAt` 增 1 个枚举值 `HIERARCHICAL`;`caps` **仍为 4 属性** | ✅ **采纳** | ① 表达力**完整**:多级聚合被表达为「聚合发生**在哪里**」的一个取值,与其余 3 个取值同层;② `caps` 的语义边界**不破** —— 4 属性是**能力属性** descriptor(层级深度是**实现参数**,混入 caps 会让不同实现因深度不同而不可比);③ 与 ArchForge 自陈的建议(`32 号 §9.1:740-741`)**同构** ⇒ 后续可被跨仓追认;④ **不**依赖「降级近似」,不引入新的语义损失 |
| **B** 把 `caps` 扩为 **5 维**(加 `hierarchy_depth`) | ❌ 否决 | ① 把**实现参数**塞进**能力 descriptor** ⇒ 「同一实现不同深度」变成两个不同能力集,破坏 caps 的可比性;② 冻结 schema 的 `caps.required` 要加属性 ⇒ 冻结面扩大且**存量实例**被迫补字段;③ depth 是**可配置项**,不是「能不能」的二元属性 |
| **C** 降级:用现有 `TreeManager`(`scope=SUBTREE`)近似承载三级目录 | ❌ 否决 | ① 直接复制 R-1 的**症状**:声明若写 `SYSTEM/EXACT/GLOBAL` 则与 `TreeManager` 行**不符**(实现承载不了声明的能力);若照抄 `TreeManager` 行则**丢掉** `GLOBAL`/`SYSTEM` 语义 ⇒ 把缺口**藏**进「近似」二字;② `32 号 §13` RK-3 自己把该路线标为「**降级**方案」而非解法 |
| **D** 只加 `HIERARCHICAL` 枚举值,**不**加实现(复用 `TreeManager` 承载) | ❌ 否决 | enum 值与实现**脱钩** ⇒ caps 声称的能力**无实现可承载** ⇒ 变成纸面声明(与 `INV-CEP-04-IC1`「caps 值必须 ∈ enum」的**可执行性**精神相悖) |

**采纳方案的具体形态**:

| 项 | 值 | 落点 |
|---|---|---|
| 新增实现 | `HierarchicalDirectory` | `ADR-CEP-04 §2` 决策 3 的 5×4 表**追加第 6 行**;`sharer_tracker.hh` 的 `kTrackerTraits` 追加第 6 行 |
| 其 `caps` | `scope=SYSTEM` · `completeness=EXACT` · `authority=GLOBAL` · `aggregation_at=HIERARCHICAL` | 同上 |
| 新增枚举值 | `AggregationAt::HIERARCHICAL` | `ADR-CEP-04 §2` 决策 3 的 enum;`sharer_tracker.hh` 的 `AggregationAt` |
| `caps` 维度 | **不变(4 属性)** | 冻结 schema 的 `caps.required` **零改动** |

**连带表述修订(追加式,正文不改)**:§2 决策 1 的「(α) 单抽象接口 + **5 种实现**」⇒ 追加式修订为「**6 种实现**」(第 6 种 = `HierarchicalDirectory`)。§2 决策 1 的其余表述、§3 ~ §7、`INV-CEP-04-IC1` ~ `IC3` —— **零改动**;**新增 invariant 数 = 0**。

### 13.2 后果 / 代价

**正面**:① D-FABRIC 域的「多级目录」在 CEP 内**可表达** ⇒ 解锁 ArchForge `32 号 §13` RK-3 记录的「D-FABRIC 域无法建模 / NSA Stage 2~3 阻塞」;② 与 ArchForge 的建议同构 ⇒ 跨仓追认成本低;③ `caps` 维度不变 ⇒ 存量 5 种实现**零迁移**。

**负面(代价)**:① `ADR-CEP-04 §2` 表 **5 → 6 行**;② `AggregationAt` 枚举 **+1 值** ⇒ 枚举字面量与 schema 枚举**各 +1**;③ **冻结面**改动 2 件(`ADR-CEP-04` 本文件 + `protocol_instance.schema.json`)⇒ 走**两步授权序列**;④ 落地涉及 `sharer_tracker.hh` 的实现表 + 守门判据 + 反向 fixture。

**风险 / 已知残留**:
- ⚠️ **`HIERARCHICAL` 的粒度未定** —— 「聚合发生在多级目录的**哪**一级」仍是**实现参数**(L1/L2/L3 的选择),**未**进 caps ⇒ 若将来需要按级声明能力,仍要再动 caps ⇒ 登记为**已知残留**,不由本次决策解决。
- ⚠️ **本 ADR 不覆盖 `mesif` 的状态机与投影表** —— ArchForge 侧**无** MESIF 状态转换规约、**无** 5→4 `perm` 投影表 ⇒ 属**独立缺口**(另立 change),**不由本决策派生**。
- ⚠️ **`mesif` 的声明权威**问题(ArchForge `32 号 §6.2` 声明 `impl=SparseSnoopFilter`,与 §9.1 的三级目录不自洽;且 `SparseSnoopFilter` 已被 `moesi_gpu` 占用)⇒ 由 `ADR-CEP-12` 的**权威迁移**裁决承担(不在本 ADR 范围)。

### 13.3 Verification(前瞻判据,未实施)

- [ ] **V1** `protocol_instance.schema.json` 的 `sharer_tracker.impl` 枚举含 `HierarchicalDirectory`,且 `caps.aggregation_at` 枚举含 `HIERARCHICAL`。
- [ ] **V2** `include/tlm/cep/sharer_tracker.hh` 的 `kTrackerTraits` **恰 6 行**,第 6 行 caps == `SYSTEM` / `EXACT` / `GLOBAL` / `HIERARCHICAL`。
- [ ] **V3** `caps` 的 `required` **仍为 4 项**(未扩维度)。
- [ ] **V4** `protocol_instances.json` 的 `mesif` 实例 `sharer_tracker.impl == "HierarchicalDirectory"`,且其 `caps` 与 V2 的第 6 行**逐字段相等**。
- [ ] **V5** **反向 fixture**:声明 `impl=HierarchicalDirectory` 但 `caps` 照抄 `TreeManager` 行 ⇒ 必红;`caps.aggregation_at` 取未登记值 ⇒ 必红。
- [ ] **V6** **正控**:`moesi_gpu` / `chi_rnf` / `mesif` 三条既有声明的 `sharer_tracker` 行**零改动**通过校验。

---

## 14. Status Update(2026-10-30,per 台账 Entry #105 / #106)—— 第 6 种实现的**行为面**已落地

> **正文不改**(ADR 不可变)。本条登记 §13 裁决的**落地结果**,以及随之产生的**新债**。

**① §13 裁决已落地**:`HierarchicalDirectory` 不再只有 enum + traits 行,已有**行为实现**
(`include/tlm/cep/sharer_tracker.hh`):`SharerTrackerIf` 4 方法 + 目录安装面
(`set_sharer` / `clear_sharer`,带层级参数)+ 多级聚合查询面(`aggregate_view_at` /
`own_view_at` / `lookup_scoped`)。

**② 与 `FullMapDirectory` 的**可观测**差异**(而非「克隆 + 纸面 caps」)**:
聚合视图是**派生**的(各级只存**自有**位掩码,查询时 OR)⇒ **不存在「陈旧聚合」**;
且存在**严格更窄**的级内视图 ⇒ 「`FullMapDirectory` 克隆」在此**不可能**。

**③ 行为决策**无真源** ⇒ 逐条登记 + 挂跨仓追认债**(本条是**新债**,不是「已裁决」):
`kBehaviorDecisions[5]`(B-1 层级数 / B-2 `ESCALATE_UP` 不使用 / B-3 fold 复用 `SnoopMechanics`
/ B-4 满表拒绝 / B-5 **F 态转发点角色不可分辨**),每条 `basis` 非空 + `pending = true`;
判据 = `behavior_decisions_are_consistent()`(可变异谓词)+ `test_cep_hierarchical_directory.cc`
HD-04 / HD-05 + 反向 fixture `cep_mesif_adapter_reverse_verify.sh`。

**④ 附带纪律**:`SnoopMechanics`(零虚函数)被两个行为实现**共用一份** ⇒ 禁双实现
(`SharerTrackerIf` 仍**恰好 4 个**虚方法,虚析构计入 = 5)。

**⑤ 状态**:§13 的「新增第 6 种实现」= ✅ **已落地归档**(队列 #9,2026-10-30)。

---

**版本**: v0.3 草案 + Status Update 2026-10-25(§8 五 change 名失指)+ Status Update 2026-10-29(§13 —— R-1 裁决:第 6 种实现 `HierarchicalDirectory` + `HIERARCHICAL`) + **Status Update 2026-10-30(§14 —— 第 6 实现的「行为面」已落地 + 行为决策跨仓追认债)**
**最后更新**: 2026-10-30
**批准状态**: 🟡 待 Oracle v0.5 二次复审
**维护者**: CppTLM build agent(architect mode)

---

## Status Update(2026-11-01,per 台账 Entry #124)—— `SparseSnoopFilter` 行为实现的**归属复核**与 R11 的**语义层缺口**登记

> **正文不改**(ADR 不可变;§2 caps 表 · §4 hazard 表 · §13 R-1 裁决 · 其余全部保留)。
> 本条登记**「`SparseSnoopFilter` 行为实现归属」**的复核结论,以及 R11 登记的**升级**。
> ⚠️ **冻结面两步授权**:授权记录 = 台账 **Entry #124**(`--update --authorized-by "Entry #124"`)。

### 触发

台账 **Entry #112 §④** 曾把 `SparseSnoopFilter` 的**行为实现**归属**去重**到队列 **#10 实现层**
(理由:「R11 是 🟡 中、**无迫近消费者阻塞**,且实现会与 #10 实现层重复 ⇒ 等 #10」)
⇒ 用户要求复核该归属是否仍成立。

### 结论

| # | 裁决 | 依据(2026-11-01 实测) |
|---|---|---|
| **1** | **归属维持**队列 **#10 实现层**;`obj §10` 的 backlog 行**继续关闭** | Entry #112 的两条前提**均未失效**:① `SparseSnoopFilter` **零实例化点**(全仓仅 traits 行 + 声明值引用)⇒ 「无迫近消费者阻塞」仍成立;② 与 #10 实现层**同载体** `sharer_tracker.hh` ⇒ 「实现会重复」仍成立 ⇒ 去重的两个目的(消除双 owner + 不绕过规约层先行)**同时**成立 |
| **2** | ⚠️ **纠正上一轮的口头误判** | 「#10 排期推迟 ⇒ 前提失效 ⇒ 需重开」的推理**不成立** —— Entry #112 的前提中**没有**「#10 何时做」这一项 ⇒ #10 的排期**不影响**该裁决有效性 |
| **3** | **R11 登记升级为两条并列** | ① **meta 层**(原表述):`moesi_gpu` 声明 `impl = SparseSnoopFilter`,而该实现**只有 traits 行、无行为类** ⇒ 声明面承诺在实现面**未兑现**;② **语义层(新增)**:本 ADR 的契约消费者侧明载「`moesi_gpu`:`O→WRITEBACK` 的损失**由** `SparseSnoopFilter`(`scope=SYSTEM`)**补偿**」⇒ 该补偿机制**当前无行为实现** ⇒ `moesi_gpu` 的 `perm` 投影完备性判据存在一处「**可补偿但未补偿**」的缺口 |
| **4** | ⚠️ **与 `chi_rnf` 的同类损失不得混谈** | `chi_rnf` 的「是否还有其他 sharer」在 RN 侧**物理不可得**(该信息只存在于 HN-F 的目录)⇒ 其同类损失是**协议信息拓扑的固有损失**、**非**实现缺口、RN 侧**不可补偿** ⇒ 二者的**可补偿性相反** ⇒ 措辞必须写准(per 载体侧既有告诫) |
| **5** | **R11 降级条件明确化** | 🟡 中 → 🟢 低 SHALL **同时**满足:① `SparseSnoopFilter` 行为类落地 **且** ② `moesi_gpu` 的 `O→WRITEBACK` **补偿路径有判据** ⇒ ⚠️ 防「落地了但补偿语义未验」就降级 |
| **6** | **债期登记** | 起点 **2026-10-27**(队列 #6 `moesi_gpu` 归档,声明首次指向空实现)⇒ 终点 = 队列 #10 实现层落地 ⇒ **每次 #10 排期变动 SHALL 复核并更新** ⇒ 防债期无声延长 |

### 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR** | **零改动**(仅追加本段);§2 caps 表 / §4 hazard 表 / §13 全部保留 |
| **`SparseSnoopFilter` 的 caps / hazard 契约** | **零改动** —— §2 的 `SYSTEM`/`CONSERVATIVE`/`GLOBAL`/`HOME` 与 §4 的 `Hazard_STALL` **已冻结** ⇒ 该实现的**行为语义不依赖任何协议规约层**(含 CXL 规约层)⇒ 这正是它**可被独立排期**的技术根据 |
| **平台自证能力** | ⚠️ **债期延长**(起点 2026-10-27)⇒ `moesi_gpu` 实例的声明面承诺在队列 #10 实现层落地前**持续未兑现** ⇒ 现已**显式登记**(本条 ⑥),不再无声延长 |
| **降级纪律** | 新增**双条件**(落地 **且** 补偿有判据)⇒ 降级门槛**提高**而非降低 ⇒ ⚠️ 后续不得以「行为类已落地」单独作为降级依据 |

### Verification(前瞻判据 · 可勾选)

- [ ] **V-1** `SparseSnoopFilter` 行为类落地时,其**行为** SHALL 与 §2 的 caps(`SYSTEM`/`CONSERVATIVE`/`GLOBAL`/`HOME`)及 §4 的 `Hazard_STALL` **逐字段一致**(消费,不重推)
- [ ] **V-2** 落地时 SHALL 同时交付「`moesi_gpu` 的 `O→WRITEBACK` **补偿路径有判据**」的**可执行**判据(否则 R11 不得降级)
- [ ] **V-3** `moesi_gpu` 的既有测试 SHALL **0 回归**;若 `CONSERVATIVE` 语义导致 snoop 数变化 ⇒ SHALL **显式登记 + 重基线**(不得静默改期望值)
- [ ] **V-4** 载体头注释 SHALL 同步「R11 已收口」+ 债期终点(per 本条 ⑥)
- [ ] **V-5** R11 降级 SHALL 同时满足 **V-1 ∧ V-2 ∧ V-3** ⇒ 任一未达成 SHALL 保持 🟡 中

**关联**: 台账 `obj-cep-platform.md` **Entry #124**(复核 + 授权记录 + 债期)· #112(去重裁决 · 原文保留)
· #107(R11 登记 + 落位映射)· #111(#10 关键交付)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值)· `ADR-CEP-05`(投影)

---

## Status Update(2026-11-01,per 台账 Entry #131)—— 第 **3** 种行为实现 `SparseSnoopFilter` **落地** + R11 降级评估

> **正文不改**(§2 caps 表 · §4 hazard 表 · §13 全部保留;本段**纯追加**)。
> ⚠️ **冻结面两步授权**:授权记录 = 台账 **Entry #131**(`--update --authorized-by "Entry #131"`)。

### ① 事件

`SparseSnoopFilter` 的**行为实现**于 2026-11-01 在 `include/tlm/cep/sharer_tracker.hh` 落地
(队列 #10 实现层 change `cpptlm-cep-cxl-nonsymmetric`,per Entry #128 转正)。

| 面 | 内容 |
|---|---|
| 落点 | `include/tlm/cep/sharer_tracker.hh` —— 与 `FullMapDirectory` / `HierarchicalDirectory` **同文件** |
| 组合 | `final : public SharerTrackerIf, public SnoopMechanics` ⇒ **禁双实现**(snoop 在飞句柄与被动记账共用一份机械) |
| caps / hazard | **消费** `kTrackerTraits[1]`(§2 的 `SYSTEM`/`CONSERVATIVE`/`GLOBAL`/`HOME` + §4 的 `Hazard_STALL` + `broadcast_fallback = true`)**不重推** ⇒ 本类**不**另写一份 caps 字面量 |
| 语义 | **CONSERVATIVE** ⇒ filter **不能**回答「**没有**其他 sharer」⇒ **永不**返回 `ANSWER_HERE`;「无条目」⇒ `BROADCAST`;唯一可能 sharer 且脏 ⇒ `FORWARD_TO`;可能 sharer > 1 ⇒ `BROADCAST`(单点转发会**漏探**) |

### ② ⚠️ 与 `FullMapDirectory` 的**可观测差异**(非克隆)

EXACT 目录**能**回答「没有其他 sharer」;CONSERVATIVE filter **不能**
⇒ 二者的 `lookup` 结果**可判别** ⇒ 满足本 ADR §13「克隆体不可能」的判据精神。

### ③ R11 降级评估(V-1 ∧ V-2 ∧ V-3)

| 条件 | 判定 | 依据 |
|---|---|---|
| **V-1** 行为与 §2 / §4 **逐字段一致** | ✅ **达成** | caps / behavior **取** `kTrackerTraits[1]`;判据 `test_cep_sparse_snoop_filter.cc`(5 cases / 39 assertions)+ 2 条毒化实测鉴别力 |
| **V-2** `moesi_gpu` 的 `O→WRITEBACK` **补偿路径有判据** | 🔴 **未达成** | 补偿路径需把 SF 的**保守查询**接入 `moesi_gpu` 的投影补偿 ⇒ **后续**工作 |
| **V-3** `moesi_gpu` 既有测试 **0 回归** | ✅ **达成** | 全量 `ctest` **129/129** 全绿 |

⇒ ⛔ **R11 维持 🟡 中**(合取不成立)⇒ **不得**降级。
⇒ **meta 层**缺口(声明指向只有 traits 行、无行为类)⇒ **已闭合**;
**语义层**缺口(`O→WRITEBACK` 补偿无判据)⇒ **仍在**。

### ④ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR** | **零改动**(仅追加本段);§2 / §4 / §13 全部保留 |
| **`SparseSnoopFilter` 的 caps / hazard 契约** | **零改动** —— §2 / §4 的冻结值**被消费** ⇒ 该实现的**行为语义不依赖任何协议规约层**(含 CXL 规约层)⇒ 它**可被独立排期** |
| **平台自证能力** | 声明面承诺**部分兑现**(meta 层闭合);⚠️ 语义层仍待补偿判据 ⇒ R11 不降级(见 ③) |
| **实现面** | `protocol_instances.json` 的 `moesi_gpu` 声明 **逐字不变**(其 `impl = "SparseSnoopFilter"` 现在**有行为实现**了 ⇒ 声明**兑现**) |

**关联**: 台账 `obj-cep-platform.md` **Entry #131**(授权记录)· #124(R11 复核 + 债期)· #128(实现层转正)
· #129(Phase 1 落地)· `ADR-CEP-02`(canonical 10 流 + `perm` 4 值)· `ADR-CEP-05`(投影)

---

## Status Update(2026-11-01,per 台账 Entry #137)—— **R11 降级**:🟡 中 → **🟢 低**

> **授权**:台账 `objective-cep-platform §11` **Entry #137** [scope-change] —— **冻结面授权记录**。
> **形态**:**纯追加**(本 ADR **正文不改**;§2 caps 表 · §4 hazard 表 · §13 全部保留)。
> **前序**:本 ADR 的 `## Status Update(2026-11-01,per 台账 Entry #131)` 已登记「第 3 种行为实现
> `SparseSnoopFilter` 落地」并做过一次 R11 降级评估(当时 **V-2 🔴** ⇒ 维持 🟡 中);
> **本段**是**该评估的第二次**,**V-2 已达成** ⇒ 降级。

### ① 事件

R11 的降级三条件(V-1 ∧ V-2 ∧ V-3)**全部达成** ⇒ **R11 由 🟡 中 降为 🟢 低**。

| 条件 | 判定 | 证据(逐条) |
|---|---|---|
| **V-1** 行为与 §2 caps / §4 hazard **逐字段一致** | ✅ **达成** | `SparseSnoopFilter::caps` / `behavior` 取 `kTrackerTraits[1]`(不另写字面量)⇒ §2 / §4 的冻结值**被消费**、不重推;判据 `test_cep_sparse_snoop_filter.cc`(5 cases / 39 assertions)+ **2 条毒化实测** |
| **V-2** `moesi_gpu` 的 `O→WRITEBACK` **补偿路径有判据** | ✅ **达成**(2026-11-01,per 台账 **Entry #136**) | ① **运行期载体**:`MoesiGpuProtocolEngine`(此前**不存在** —— `MoesiGpuAdapter` 是**纯静态**的且 `moesi_gpu` **无 protocol engine**)② **接口面扩展**:`SharerTrackerIf` 的 `caps` / `behavior` 提为基类**数据成员** ⇒ 持接口指针**即可**读 `completeness` ③ 判据 `test_cep_moesi_gpu_compensation.cc`(**5 cases / 65 assertions**)+ **2 条毒化实测** |
| **V-3** `moesi_gpu` 既有测试 **0 回归** | ✅ **达成** | 全量 `ctest` **131/131** 全绿 |

### ② ⚠️ 降级的**语义边界**(不得越界表述)

| 不得表述为 | 理由 |
|---|---|
| 「CEP 已有完整协议实现」 | 降级的是 **R11**(「声明超前于实现」);本载体只服务**补偿路径** —— canonical 10 流的 `on_*` **只记账**(引擎侧决策 M-6)⇒ **不是**完整协议引擎 |
| 「等价恢复已实现」 | 真实 tracker 是 `CONSERVATIVE` ⇒ 补偿结论**恒为** `ConservativeOnly` ⇒ **只能**给「**可能**还有其他 sharer」的**保守近似**(per 本 ADR 的 V-2 补记)⇒ 等价恢复**仅**在 tracker 声明 `EXACT` 时才成立 |
| 「`SharerTrackerIf` 变成 5 个方法」 | `caps` / `behavior` 是**数据成员**、**不是**虚方法 ⇒ `ADR-CEP-04 §3` 的「接口恰好 **4** 个方法」契约**不变**(`count_member_virtuals` 仍 = **5**,含虚析构)⇒ 已归档的 `cep-sharer-tracker-base` 的 **Scenario 全部不受影响** ⇒ 其 main spec **零改动** |

### ③ `SharerTrackerIf` 的**接口面扩展**(受控,记录在案)

| 面 | 内容 |
|---|---|
| **变更** | `caps` / `behavior` 从**各行为实现类**移到 **`SharerTrackerIf` 基类**(public **数据成员**) |
| **根因** | `caps` 此前**只在实现类上** ⇒ 持 `SharerTrackerIf*` 的消费方**读不到** `completeness` ⇒ 补偿路径无法判 verdict ⇒ 只剩「抄第二份 caps」(**违反单源纪律**)或「提到接口上」⇒ **本次走后者** |
| **存储唯一性** | 3 个行为实现**删除**自己的成员副本 ⇒ 改在**构造器**里给基类成员赋值 ⇒ `impl.caps` 写法**仍可用**(继承而来)⇒ **源码兼容**;各实现的 caps 取值仍**逐字取自** `kTrackerTraits[N]` |
| **契约影响** | **零** —— 虚方法数不变(4 + 虚析构 = 5);`ADR-CEP-04 §2` / `§4` 的冻结值**不变**;已归档 main spec **零改动** |
| **反向验证连带** | `cep_mesif_adapter_reverse_verify.sh` 的 `tracker-caps-drift` 毒化锚点随之**迁移**(第三次复现「状态变迁 ⇒ 锚点失效」形态)⇒ 反向验证 **18 项通过 / 0 项失败** |

### ④ R11 债期**收口**

起点 **2026-10-27**(队列 #6 `moesi_gpu` 归档,声明首次指向空实现)⇒ **终点 2026-11-01**
(Entry #136 交付 V-2)⇒ **债期闭合**。
⚠️ **残余(不属 R11)**:`FullMapDirectory` 的**反向缺口**(有行为实现 · **零**实例声明)
⇒ 仍挂台账 **Entry #133 ③**(事实登记;落位裁决**待 architect**)。

### ⑤ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR** | **零改动**(仅追加本段);§2 / §4 / §13 全部保留 |
| **`SharerTrackerIf` 的契约** | **零改动** —— 方法数、签名、caps / hazard 的冻结值全部不变;只是**存放位置**上移 |
| **`SparseSnoopFilter` 的行为语义** | **零改动** —— 仍**不依赖任何协议规约层** ⇒ 仍**可被独立排期** |
| **平台自证能力** | 声明面承诺**已兑现**(meta 层 + 语义层**双闭合**)⇒ 「`moesi_gpu` 的 `sharer_tracker.impl = SparseSnoopFilter`」**不再超前于实现** |
| **新增的运行期面** | `moesi_gpu` 现在**有**运行期载体 ⇒ 但它**只服务补偿路径** ⇒ ⚠️ **不得**据此推断 `moesi_gpu` 的协议引擎已完整(见 ②) |

**关联**: 台账 `obj-cep-platform.md` **Entry #137**(授权记录)· #136(V-2 交付)· #133(债期重开 + 反向缺口)
· #131(第 3 实现落地 + 首次降级评估)· #129(Phase 1 落地)· #124(R11 复核)
· `moesi_gpu_protocol_engine.hh` · `sharer_tracker.hh` · `test_cep_moesi_gpu_compensation.cc`

---

## Status Update(2026-11-01,per 台账 Entry #139)—— `FullMapDirectory` 的**落位裁决**(**无消费者 ⇒ 不排期、保留登记**)

> **授权**:台账 `objective-cep-platform §11` **Entry #139** [scope-change] —— **冻结面授权记录**。
> **形态**:**纯追加**(本 ADR **正文不改**;§2 caps 表 · §4 hazard 表 · §13 全部保留)。
> ⚠️ **本段不改 §2 caps 表的 `FullMapDirectory` 行** —— caps 是**能力描述**,与「是否有消费者」**正交**。

### ① 事实复核(2026-11-01 实测)

| 项 | 实测 |
|---|---|
| 行为类 | ✅ **存在**(`sharer_tracker.hh` 的 `class FullMapDirectory final : public SharerTrackerIf, public SnoopMechanics`) |
| traits 行 | ✅ 存在(`kTrackerTraits[0]`;caps = `SYSTEM`/`EXACT`/`GLOBAL`/`HOME`;hazard = `STALL`) |
| **实例声明** | ❌ **零** —— `protocol_instances.json` 的 4 个实例(`moesi_gpu` / `mesif` / `chi_rnf` / `cxl_2_0`)**无一**声明它 |

### ② 裁决:**无消费者 ⇒ 不排期、保留登记**(照 `BroadcastNull` 先例,per Entry #112)

| 候选 | 裁决 | 理由 |
|---|---|---|
| ① `chi_rnf` 升 **HN-F** | ❌ 否决 | 本 ADR §2 的 caps 表把 `FullMapDirectory` 标注为「CHI HN-F」载体 —— 但队列 #8 只做 **RN-F subset**,**队列表无 CHI-full 行** ⇒ 无消费者 ⇒ 强行造消费者 = 为一个实现**逆向发明**需求 |
| ② 队列 #13 `nvlink-sharp` / #14 `ualink-inc` 的 tracker 选型 | ❌ 暂不裁决 | 二者**均未起手**(§4.0 状态列 = 📋 Planned)⇒ 其 tracker 选型**尚不存在** ⇒ 现在指定 = **猜测**;且二者是否需要集中目录**本身**未定 |
| ③ **无消费者 ⇒ 不排期、保留登记** | ✅ **裁决** | 照 `BroadcastNull` 先例(Entry #112)⇒ 未来出现消费者时再评 |

### ③ ⚠️ 与 R11 的**方向相反**(不得混谈)

R11 是「**声明超前于实现**」(声明了 `SparseSnoopFilter` 却无行为类)⇒ 已于 Entry #137 降级为 🟢 低;
本条是「**实现超前于声明**」(有行为类却无实例声明)⇒ **不属 R11** ⇒ **不因 R11 的降级而闭合**。

### ④ 与 `BroadcastNull` 的**区别**(不得混读)

| 面 | `BroadcastNull` | `FullMapDirectory` |
|---|---|---|
| 行为实现 | ❌ 无(仅 traits) | ✅ **有** |
| 裁决 | ③ 不排期、保留登记 | ③ **同款** |
| **可复用性** | 未来**广播型**协议族接入时**直接可用** | 未来 **CHI-full 队列**出现时**直接可用** ⇒ **实现不废弃、只是暂不排期** ⇒ ⚠️ **不得**表述为「该实现无用 / 应删除」 |

### ⑤ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR** | **零改动**(仅追加本段);§2 caps 表 · §4 hazard 表 · §13 全部保留 |
| **caps 契约** | **零改动** —— `FullMapDirectory` 的 caps = `SYSTEM`/`EXACT`/`GLOBAL`/`HOME` **不变** |
| **实现面** | **零改动** —— `sharer_tracker.hh` 的 `FullMapDirectory` 行为类**不删不改**;仅头注释补落位口径 |
| **平台自证能力** | ✅ 「6 种实现中哪几种有行为类 / 哪几种有实例声明」**可逐项核对** ⇒ 反向缺口**显式登记**,不沉默 |
| **残余** | ⚠️ `FullMapDirectory` 在 **CHI-full 队列**出现前**持续无消费者** ⇒ 该缺口**保持开放**(登记态,非闭合态) |

**关联**: 台账 `obj-cep-platform.md` **Entry #139**(授权记录 + 裁决)· #133(反向缺口首次登记)
· #137(R11 降级 · **方向相反**)· #131 · #129 · **#112(`BroadcastNull` 落位裁决先例)** · #107
· `07 号` §11 · `openspec/specs/cep-sharer-tracker-base/spec.md` · `sharer_tracker.hh`

---

## Status Update(2026-11-02,per 台账 Entry #154)—— §3.5 聚合点二元:**`REQUESTER` 分支被公开来源反证** ⇒ 现行口径 = 聚合点**恒为 `HOME`**

> **§3.5 正文零改动**(二元表原文保留,作历史);本段登记**反证 + 现行口径 + 待确认路径**。
> ⚠️ **冻结面两步授权**:授权记录 = 台账 **Entry #154**(`--update --authorized-by "Entry #154"`)。
> ⚠️ **本段不改 §2 caps 表 · §4 hazard 表 · §13** —— 只处置 §3.5 的**聚合点二元**。

### ① 事件:公开来源普查发现 §3.5 的 `REQUESTER` 分支**未被任何公开来源支持**

队列 #12 的 **Task 0 公开来源普查**(产物 `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`,
**零规范正文暴露**,ECA §3 + AI AUP 合规)对 §3.5 的二元做了核对:

| 场景 | §3.5 原文 | 公开来源(S4 = ACM CSUR 2024 · arXiv `2306.11227v3` · sha256 `30f5baac…75bf` · **architect 一手复核**) | 判定 |
|---|---|---|---|
| **HDM P2P**(device-to-device data pull) | `aggregation_at = REQUESTER`;理由「请求者驱动 fold 状态;**其他设备**发 BIRsp」 | 「UIO transactions are serviced directly from memory… **else the memory controller back-snoops the host processor** using the BI flows」(S4 p.21,§5.3 / Fig.16)⇒ 仲裁者是 **memory controller**(= home);且 snoop 目标是 **host processor**,**不是**「其他设备」 | ❌ **反证**(聚合点 + snoop 对象**两处**冲突) |
| **HDM home**(归属设备作为 home) | `aggregation_at = HOME` | 「the memory device (GFD/MLD) maintains a directory (or a snoop filter) to track ownership of cache lines」(S4 p.21,§5.3 / Fig.17b) | ✅ **印证** |

⇒ **两处场景的公开来源都把仲裁点放在 home 一侧** ⇒ §3.5 的「二元」在**聚合点维度**上**不成立**。

### ② 选项对比(≥3 方案)

| # | 方案 | 聚合点口径 | 代价 | 判定 |
|---|---|---|---|---|
| **A** | **保留二元不动**,只登记反证 | 载体**不得**锁 `aggregation_at` ⇒ 该格标 `pending` | 规约层**少一个可锁结构**;`DcohSymmetric` 的 traits 与 ADR 的**张力持续**;载体 basis 里留一个永远 pending 的格 | ❌ 覆盖不足 |
| **B** | **二元收敛为 `HOME`**(直接删掉 `REQUESTER` 分支) | `aggregation_at = HOME` 恒定 | ⚠️ 若人类成员条款摘要最终**推翻**反证 ⇒ 需**再次**修订 ⇒ **反复**;且**丢掉**了 P2P / HDM-home 的**触发源差异**(该差异是**真实**的,只是**不决定聚合点**) | ❌ 过度收敛 + 丢信息 |
| **C** | **聚合点收敛为 `HOME`;P2P / HDM-home 降级为「场景标签」** | `aggregation_at = HOME` 恒定;场景维度**仍区分**两者(承载不同的 fold 触发源) | 需把「P2P ⇒ REQUESTER」的语义位置从**聚合点**改为**场景标签** ⇒ 属**语义重定位**而非删除 ⇒ 信息**不丢**;⚠️ 仍**不改正文**(只追加本段) | ✅ **裁决采用** |

### ③ 裁决(现行口径)

1. **聚合点恒为 `HOME`** —— CXL 3.0 对称模式下,**公开来源不区分** P2P 与 HDM-home 的**仲裁位置**(均在 home 一侧)⇒
   `aggregation_at` **不再随场景变化** ⇒ **载体可锁** `aggregation_at == HOME`。
2. **P2P / HDM-home 保留为「场景标签」** —— 二者的**触发源差异**(P2P = device 经 UIO 直接访问 memory;
   HDM-home = 多 host 共享同一行)仍然成立 ⇒ **保留**为载体可登记的场景维度,**不**用于选择聚合点。
3. ⚠️ **`REQUESTER` 分支不得作为载体锁定依据** —— 载体侧若出现该值 ⇒ 视为**未钉死** ⇒ 须标 `pending`。
4. ⚠️ **`AggregationAt::REQUESTER` 枚举值** —— **保留**(公共契约面;`protocol_instance.schema.json` 的值级枚举含它 ⇒ **冻结 schema 零改动**)⇒
   改 traits 后该值**无消费者** ⇒ 登记为「保留但无消费者」(照 `FullMapDirectory` 的**反向缺口**同款登记范式)。
5. ⛔ **不得**表述为「§3.5 的二元被规范推翻」 —— 本段登记的是**公开来源反证**;**规范性依据**须由「**人类成员条款摘要**」确认。

### ④ 待确认路径(分层处置,防反复)

| 阶段 | 触发 | 动作 |
|---|---|---|
| **现在** | 公开来源反证已一手复核 | 按 ③ 的现行口径推进载体(`aggregation_at = HOME` **可锁**)⇒ 规约层**不被** §3.5 的历史二元阻塞 |
| **人类成员条款摘要到位** | 拿到 CXL 规范正文的条款摘要(**AI 不接触正文**) | 若**确认**反证 ⇒ **走 ADR 修订**:改正 §3.5 二元表 + 追加第 4 则 SU(登记修订)⇒ **本段随之追加**「已正式修订」 |
| | 若**推翻**反证 | 本段追加「反证不成立」+ 恢复 §3.5 原口径 ⇒ 载体该格由 `HOME` 改回 `REQUESTER` ⇒ ⚠️ 因载体该格**可锁**,此改动**成本可控** |

### ⑤ 后果 / 代价

| 面 | 后果 |
|---|---|
| **本 ADR** | **零正文改动**(仅追加本段);§2 caps 表 · §4 hazard 表 · §13 全部保留 |
| **`§3.5` 的状态** | ⚠️ 二元表**原文保留**但**已被本段登记为「聚合点维度不成立」** ⇒ 载体与实现**不得**再据其第一支锁定 |
| **载体面** | ✅ **受益** —— `aggregation_at` 由「不可锁」变为**可锁**(`HOME`)⇒ 规约层少一个 `pending` 格 |
| **traits 面** | `DcohSymmetric` 的 `AggregationAt::REQUESTER` ⇒ **订正为 `HOME`**(该实现**行为未落地** ⇒ **零行为影响**);连带 `test_cep_sharer_tracker_base.cc` 的**二元断言**(`∈ {REQUESTER, HOME}`)⇒ **收紧**为 `== HOME` |
| **冻结 schema** | **零改动** —— `protocol_instance.schema.json` 的 `AggregationAt` 值级枚举**保留** `REQUESTER` |
| **对外表述** | ⚠️ **不得**表述为「CXL 3.0 对称模式下聚合点在 requester」;现行口径 = **home** |
| **残余** | ⚠️ 规范性依据**待**人类成员条款摘要确认(§④)⇒ 登记态 |

### ⑥ Verification(前瞻判据 · 可勾选)

- [ ] `sharer_tracker.hh` 的 `DcohSymmetric` traits 的 `aggregation_at` == `AggregationAt::HOME`
- [ ] `test_cep_sharer_tracker_base.cc` 的该行断言**收紧**为 `== HOME`(不再是二元 `∈ {REQUESTER, HOME}`)
- [ ] `protocol_instance.schema.json` **零改动**(枚举仍含 `REQUESTER`)
- [ ] 队列 #12 规约层载体可**静态锁死** `aggregation_at == HOME`(不标 `pending`)
- [ ] 载体把 P2P / HDM-home 登记为**场景标签**(非聚合点选择)
- [ ] 人类成员条款摘要到位后:本段追加「已正式修订」或「反证不成立」(§④ 的两条路径之一)

**关联**: 台账 `obj-cep-platform.md` **Entry #154**(授权记录 + 本裁决)· **#153**(Task 0 普查登记 + architect 裁决面)
· #152(组 1 归档)· #139(`FullMapDirectory` 落位裁决 · **「保留但无消费者」登记范式**)· #137(R11 降级)· #127(队列 #10 规约层归档)
· `docs/research/2026-11-02-cep-cxl-dcoh-symmetric-anchor-summary.md`(§3.4 / §6 #3 —— **本段的一手复核依据**)
· `07 号` §5.1.1 / §11(第 19 则 SU)· `openspec/specs/cep-sharer-tracker-base/spec.md` · `sharer_tracker.hh`

## Status Update(2026-11-03, per 台账 Entry #193)—— 队列 #14 的 tracker 选型:**候选集 + 逐实例理由登记**(非派生规则)

> **授权**:台账 `obj-cep-platform §11` **Entry #193** [adr-amendment] —— **冻结面授权记录**。
> **形态**:**纯追加**(本 ADR 正文与前十则是 SU 一律不改;**§2 caps 表 · §13 ~ §14 第 6 种实现 · §15 落位裁决 全部保留**)。
> **触发**:本 ADR §15 裁定 ② 明写「队列 #13 `nvlink-sharp` / #14 `ualink-inc` 的 tracker 选型 ❌ **暂不裁决**
> —— 二者均未起手 ⇒ 现在指定 = 猜测」;队列 #13 的 `BroadcastNull` 由**用户裁决 2026-11-03** 逐例给定;
> 队列 #14 已进入开工前置阶段 ⇒ **必须**先把「选型方法」定为**可审计的规则**,否则实现层落地即**越权猜测**。
> **形态**: 本段**第 1 稿(「由 `flow_support` 派生」)经 critic 第 1 轮 REJECT 后重写** ⇒ 修订对照见 **⑤**。

### ① 裁决:选型是**逐实例的显式声明 + 理由登记**,**不存在**可机械派生的全局规则

**第 1 稿试图立「由 `flow_support` 派生」的规则,已被仓内数据反证**(见 ②)⇒ 本段改为:

> **裁决**:每个协议实例的 `sharer_tracker.impl` 是**该实例的显式声明**,SHALL 随该实例的声明源一并登记
> **选择理由**(可审计:指向该协议的**公开来源**对该协议是否需要 sharer-set 知识的结论)。
> ⇒ **候选集**与**判别依据**如下;**判别依据是语义层的**(协议是否需要 sharer-set 知识),
> **不**落在本仓的 `flow_support` 上(理由见 ②)。

**候选集与判别依据**(**7 行** —— 与 schema 枚举一致;每个候选的「结论 + 理由」,⛔ 不得只列被选中的一项):

**候选集 = schema 枚举的**全部 7 值**(`protocol_instance.schema.json` 的 `sharer_tracker.impl` `enum` 实测)——
⛔ 不得少于它(少列 ⇒ 现役实例落表外 ⇒ 元测试恒红),⛔ 也不得**照抄它作判据**(照抄 ⇒「∈ 候选集」**恒真**,
撞「⛔ 不得写恒真判据」铁律)⇒ 候选表只作**决策面**的说明,**判据**另取有鉴别力者(见 ④):

| 候选 | 结论 | 理由 | 现役实例 |
|---|---|---|---|
| **`不参与`**(字面值) | ✅ **可选** | 判别依据:该协议的**权限由副本 / token 携带** ⇒ 协议**不需要** sharer-set 知识 ⇒ tracker 不参与(per 本 ADR `INV-CEP-04-IC7`) | `chi_rnf` / `cxl_2_0` |
| **`BroadcastNull`** | ✅ **可选** | 判别依据:该协议**有** snoop 类流量但**没有**集中目录 ⇒ 只能由**恒广播兜底**承担(§2 caps 表:唯一「无目录」形态) | `nvlink_4` |
| `FullMapDirectory` | ⚠️ **仅当**该协议**确有**集中全图目录语义 | §2 caps 表标为「CHI HN-F」载体 ⇒ 用在别的协议上 = **假声明** | (暂无) |
| `SparseSnoopFilter` | ⚠️ **仅当**该协议**没有**精确目录、只能**保守多探** | §2 caps 表的「Sparse-SF」行:`completeness=CONSERVATIVE` ⇒ 适用「无精确 sharer-set 视图」的协议 | `moesi_gpu` |
| `DcohSymmetric` | ⚠️ **仅当**该协议是**设备内的一致性代理**(可做**对称** snoop) | §2 caps 表的「DCOH」行:设备侧发起反向 snoop ⇒ 只适用 CXL 3.0 对称模式 | `cxl_3_0_symmetric` |
| `HierarchicalDirectory` | ⚠️ **仅当**该协议**确有**多级目录聚合语义 | §13 的 `scope=SYSTEM` + 多级 `aggregation_at` ⇒ 用在无此结构的协议上 = **假声明** | `mesif` |
| `TreeManager` | ⚠️ **仅当**该协议**确有**树形拓扑 | §2 caps 表的 `SUBTREE` / `TREE_PARENT` 假设**树形** ⇒ 交换式 fabric 上不成立 | `tilelink_c` |

⚠️ **`不参与` 与 `BroadcastNull` 在 token 类语境下语义等价**(per `INV-CEP-04-IC7`:「实现上等价 `BroadcastNull` 或 tracker 不参与」)
⇒ 它们**不是**互斥的分支,登记时**任一**均可,理由须写明依据的是哪一条。

### ② ⚠️ 前提订正:三条**错账**(留档防止复用)

| 错账 | 实测事实 | 出处 |
|---|---|---|
| 「选型可由 `flow_support` 机械派生」 | **被反证**:`protocol_instances.json` 的 **7 个实例全部** `flow_support.snoop == true` 且 `snoop_resp == true`,而 `sharer_tracker.impl` 分别为 `SparseSnoopFilter` / `HierarchicalDirectory` / `不参与` / `不参与` / `DcohSymmetric` / `TreeManager` / `BroadcastNull` ⇒ **6/7** 不满足任何「由该位派生」的规则 | `include/tlm/cep/protocol_instances.json`(逐条可查) |
| 「`flow_support` 不承载 sharer-set 需求」 | ✅ **成立**:`flow_support` 是**能力矩阵**(canonical 10 流覆盖矩阵,per `ADR-CEP-12 §2` 决策 1),`snoop` 表达「协议**支持** snoop 流」,与「是否需要 sharer-set 知识」**无对应** | 同上 |
| 「`HierarchicalDirectory` 无实例声明 ⇒ 可用队列 #14 闭合 R11 反向缺口」 | **错**:`mesif.sharer_tracker.impl = HierarchicalDirectory` ⇒ **有消费者**;零消费者的**反向缺口**是 `FullMapDirectory`(§15,Entry #139) | 同上 / 本 ADR §15 |

⇒ 另订正一处**先例引用**:第 1 稿把 `chi_rnf` / `cxl_2_0` 当作「`snoop` / `snoop_resp` 皆 false」的先例 ⇒ **事实错误**
(二者两字段皆 `true`)⇒ 它们实为「**有 snoop 却声明 `不参与`**」的实例 ⇒ **恰好**证明「`flow_support` 与选型无机械对应」。
⇒ 同理,`不参与` **不在** §2 caps 表内,且 `chi_rnf` / `cxl_2_0` 的 caps 与 `BroadcastNull` 行**逐字段相同**
(`NONE` / `EXACT` / `NONE` / `NONE`)⇒ **caps 无法区分二者** ⇒ 「caps 可用于校验选型」不成立。

### ③ 后果 / 代价

| 面 | 后果 |
|---|---|
| **§2 caps 表 / §13 / §14 / §15** | **零改动** |
| **新增实现** | **0**(五种候选都已有载体或已是字面值) |
| **新增判据** | 2 条元测试:① **caps ↔ traits 逐字段相等**(有鉴别力,per ④);② **理由登记存在性**(`protocol_instances.json` 顶层 `sharer_tracker_rationales{}`,含可判定的来源标识)。⛔ **不**校验「与 `flow_support` 的派生一致性」(该关系**不存在**);⛔ **不**把「`impl` ∈ 候选集」当独立判据(恒真) |
| **新增声明面** | `protocol_instances.json` **顶层** `sharer_tracker_rationales{}` 块(理由**进不了**实例对象 —— 实例级 `additionalProperties: false`);顶层不受实例 schema 约束,且 ⑥ 镜像闸只逐实例比 12 键 ⇒ **不触发**跨仓闸 |
| **代价** | 真正局限:**选型所需信息不在本仓的机器可读声明面内** ⇒ 依赖**逐实例的人工理由登记**(可审计,但**不可机械派生**)⇒ 登记表须随声明源演进,否则新实例会**无理由**(元测试必红) |
| **残余** | `ualink_4` 的实际取值**尚未裁决** —— 取决于 Task 0 普查确认「UALink INC 是否需要 sharer-set 知识」;⛔ **不得**由 architect 或 builder 预先钉死 |

### ④ Verification(前瞻判据 · 可勾选)

- [ ] **caps ↔ traits 一致性**(有鉴别力的主判据):每个实例的 `sharer_tracker.caps` SHALL 与
      `include/tlm/cep/sharer_tracker.hh:217` `kTrackerTraits` 中**对应行**的 caps **逐字段相等**
      ⇒ 反 fixture:改实例 caps 的一格 ⇒ 必红(per §13 V5 同类反 fixture 可复用)
      ⇒ ⚠️ **`不参与` 无 traits 行**(`kTrackerTraits` **恰 6 行**,覆盖 **6** 个真实实现 —— 实测
      `FullMapDirectory` / `SparseSnoopFilter` / `DcohSymmetric` / `TreeManager` / `BroadcastNull` /
      `HierarchicalDirectory`)⇒ 其参照 =
      **退化集** `NONE` / `EXACT` / `NONE` / `NONE`(实测 `chi_rnf` / `cxl_2_0` 的 caps 即此)
      ⇒ 该退化集与 `BroadcastNull` 行**逐字段相同** —— 二者**语义等价**(per `INV-CEP-04-IC7`)
      ⇒ 该判据**不**需要区分二者(只锁「caps 不得偏离其实现对应的行」)
- [ ] **理由登记存在性**:`protocol_instances.json` **顶层** `sharer_tracker_rationales{}` 块 SHALL 为
      **每个**已声明实例提供一条理由 ⇒ 理由文本 SHALL 含**可判定的来源标识**
      (公开规范名 + 章节号 / DOI / 公开规范名,或**本 ADR 的既有裁决条目号**)
      ⇒ 反 fixture:删除某实例的理由 ⇒ 必红
- [ ] ⛔ **不存在**「`flow_support` ⇒ `sharer_tracker.impl` 派生一致性」断言(该关系**已被反证**)
- [ ] ⛔ **不存在**「`impl` ∈ 候选集」作为**独立判据**(候选集 = schema 枚举 ⇒ 该合取项**恒真**)
- [ ] 任何「用队列 #14 闭合某个 R11 缺口」的理由**不**出现在登记里(见 ② 第三行)

**理由登记的载体(why 不是实例对象)**:`protocol_instance.schema.json` 的实例级与 `sharer_tracker` 级
`additionalProperties` 均为 `false` ⇒ 理由**进不了实例对象** ⇒ 落 `protocol_instances.json` **顶层**
`sharer_tracker_rationales{}` 块(顶层**不受**实例 schema 约束;`cep_decl_alignment_check.sh` ⑥ 只
**逐实例**比 12 键 ⇒ 顶层新键**不触发** ArchForge 镜像闸)。

**元测试的作用域**:**全部**已声明实例(含存量 7 个)⇒ 存量实例的理由由**本段给出**
(见下表)⇒ 实现层只需落盘即可全绿;⛔ 不设「存量豁免」(豁免 ⇒ 该断言对存量**恒真**)。

| 实例 | `impl` | 理由(来源标识) |
|---|---|---|
| `moesi_gpu` | `SparseSnoopFilter` | 无精确 sharer-set 视图 ⇒ 保守多探(本 ADR §2 caps 表「Sparse-SF」行) |
| `mesif` | `HierarchicalDirectory` | 多级目录聚合(本 ADR §13 第 6 种实现) |
| `chi_rnf` | `不参与` | CHI RN-F subset 无 home 目录 ⇒ 权限由副本携带(本 ADR `INV-CEP-04-IC7`) |
| `cxl_2_0` | `不参与` | CXL 2.0 host 侧 snoop filter ⇒ 权限由副本/token 携带(本 ADR `INV-CEP-04-IC7`) |
| `cxl_3_0_symmetric` | `DcohSymmetric` | CXL 3.0 对称模式的设备内一致性代理(本 ADR §2 caps 表「DCOH」行) |
| `tilelink_c` | `TreeManager` | TileLink 树形拓扑(本 ADR §2 caps 表 `SUBTREE` / `TREE_PARENT` 行) |
| `nvlink_4` | `BroadcastNull` | 无目录 + 恒广播兜底(本 ADR §2 caps 表;行为类见 `broadcast_null_tracker.hh`,队列 #13 交付) |

### ⑤ critic 第 1 轮(REJECT,BLOCKER)的修订对照

| # | critic 的 finding | 处置 |
|---|---|---|
| **B1** | 「由 `flow_support` 派生」的规则**被 6/7 存量声明反证**;与 `INV-CEP-04-IC7`(二者语义等价)矛盾;先例引用**事实错误**;「caps 表里唯一无目录形态」的依据不成立 | ⇒ **撤回**派生规则,改为 **① 候选集 + 语义层判别依据 + 逐实例理由登记**;**②** 逐条订正三条错账 + 一处先例引用 + caps 论证;**③** 元测试改为「候选集 + 理由登记」,⛔ 删掉派生一致性断言 |
| **N2** | 代价栏「派生表是二值的」表述不准确 | ⇒ **③** 改为「真正局限 = 选型所需信息不在本仓机器可读声明面内」 |

### ⑥ critic 第 2 轮(REJECT,新 BLOCKER)的修订对照

| # | critic 的 finding | 处置 |
|---|---|---|
| **B4** | 候选集只列 5 项 ⇒ **漏** `SparseSnoopFilter` / `DcohSymmetric`(二者是 schema 枚举内 + 现役取值)⇒ 元测试在字面读法下**今日必红**、在宽松读法下退化为**恒真**;且与本 ADR 自己声明「保留不改」的 §2 caps 表 / §13 矛盾 | ⇒ **① 候选集补全为 schema 枚举的 7 值**(逐个给判别依据 + 现役实例);**④ 判据换掉恒真项** ⇒ 主判据改为「**caps ↔ `kTrackerTraits` 对应行逐字段相等**」,「`impl` ∈ 候选集」**不再**作独立判据;并显式登记「不得照抄 enum 作判据」 |
| **S-2** | 理由登记**无载体**(实例级与 `sharer_tracker` 级 `additionalProperties` 均为 `false`)+ 元测试作用域未钉 ⇒ 今日 7 实例**全红** | ⇒ **④** 指定载体 = `protocol_instances.json` **顶层** `sharer_tracker_rationales{}` 块(顶层不受实例 schema 约束 + ⑥ 只逐实例比 12 键 ⇒ 不触发镜像闸);作用域 = **全部**已声明实例,并由**本段给出存量 7 条理由**(实现层落盘即可全绿),⛔ 不设存量豁免(豁免 ⇒ 恒真);**③** 后果表同步 |
| **N-2** | 「五种候选」与 §2 的 6 种实现 + schema 的 7 值枚举不一致 | ⇒ **①** 改为「**七种**(与 schema 枚举一致)」的表述 |
| **第 3 轮** | B4 **已闭合**;残留计数/措辞:「5 行」vs 表体 7 行;「覆盖 5 个真实实现」vs 实测 **6** 个 | ⇒ 表头改「**7 行**」;「`不参与` 参照」段改「覆盖 **6** 个真实实现」+ 列名;本行补记 |

**关联**: 台账 **Entry #193**(授权记录)· #139(`FullMapDirectory` 反向缺口)· #154(§3.5 聚合点二元)· #96(R-1 第 6 种实现)
· #112(`BroadcastNull`「无消费者 ⇒ 保留登记」)· 用户裁决 2026-11-03(队列 #13 的 `BroadcastNull`)
· `include/tlm/cep/broadcast_null_tracker.hh`(队列 #13 交付的行为类)· `include/tlm/cep/protocol_instances.json`

## Status Update 2026-11-04(#1)—— §3.4 的 BISnp **举例方向**订正(device 发起;`observe_snoop` 接口语义**零变化**)

**主题**:§3.4「被动观察(BISnp 接收侧)」的 systemverilog 注释写「**远端设备收到 BISnp 后**更新自己 DCOH 中
『peer 对该线的认知』」⇒ 隐含 BISnp = **host → device**。该**举例方向**与三方证据相反。

**① 三方证据(全部一致)**

| 证据 | 位置 | 结论 |
|---|---|---|
| **CXL 3.0 规范** | — | BISnp = **device → host**(device 发起的 back-invalidate snoop,要求 host 撤销其缓存副本);BIRsp = host → device |
| **本仓真源 1** | `docs/architecture/07-coherence-exploration-platform.md` §5.4 的 Verification | 已勾选「`back_invalidate` 行的 BISnp 方向 == **device→host(S2M)**」 |
| **实现(规约载体)** | `include/tlm/cep/cxl_dcoh_state_spec.hh` | `CDS-03` 的 `static_assert` 锁「S2M_BISnp SHALL 是**设备发起**(local)」;`M2S_BIRsp` = Remote(host 回应) |

⇒ §3.4 是**孤例**(同 ADR `:28`「BISnp 往返涌现」的方向表述本身正确)。

**② 订正范围(刻意收窄)**

- **仅举例方向**:`§3.4` 注释中的「远端设备**收到** BISnp」⇒ 应为「**device 发起** BISnp、**host 接收**;
  device 侧收到的是 BIRsp」。
- ⛔ **`observe_snoop(addr, from_node, resulting_local_state)` 接口语义零变化** ——
  `from_node` 仍是「**snoop 来源节点**」;在 BISnp 场景下 `from_node` = 发起 back-invalidate 的**设备**
  ⇒ 订正后该接口**恰好**与其原语义自洽(此前是举例写反而非接口有问题)。
- ⛔ 本 ADR **正文 §1 ~ §13 逐字不改**(ADR 不可变纪律;本则只追加)。
- ⛔ `observe_snoop` 的**禁止项不变**:仍禁止 `subscribe_remote_snoop()`(传输通道概念不得泄进 tracker 平面)。

**③ 连带订正(已落地,builder 域)**:`include/tlm/cep/cxl_dcoh_state_spec.hh` 的 **6 行** `CxlMemChannel`
方向注释原整体写反(M2S 标成「memory → host」)⇒ 已订正为 M2S = host → device / S2M = device → host
(纯注释,枚举值 / 方向表 / `CDS-03` 断言**零改动**),per 台账 **Entry #214**。

**④ Verification(前瞻判据 · 可勾选)**

- [x] `ADR-CEP-04` §3.4 的举例方向与 `07 号 §5.4` + `CDS-03` + CXL 3.0 三方一致
- [x] `observe_snoop` 三参语义(`from_node` = snoop 来源节点)未被本则改动
- [x] `cxl_dcoh_state_spec.hh` 的 6 行方向注释与本则一致(builder 域,Entry #214)
- [x] `include/tlm/cep/protocol_instance.schema.json` **零改动**
- [ ] `ADR-CEP-04` 正文 §1 ~ §13 逐字未改(本则仅追加)

**关联**: 台账 **Entry #214**(代码域注释订正)· **#170** ⑤ X3(本项原始登记)·
`docs/architecture/07-coherence-exploration-platform.md` §5.4(Verification 已勾选项)·
`include/tlm/cep/cxl_dcoh_state_spec.hh`(`CDS-03`)· `ADR-CEP-12` 决策 2(协议族能力映射口径)


---

## Status Update(2026-11-06,per 台账 Entry #238)—— `ualink_4` 实例取值**正式裁决**(§③残余收口)+ 两项**登记面**的落地义务

> **性质**:追加式(正文 §1 ~ §13 **逐字不改**)—— 本段是 **§③残余**「`ualink_4` 的实际取值**尚未裁决**」的**正式收口**。
> ⚠️ **本段只裁 13 键中的 4 键**(`flow_support.snoop`/`snoop_resp` · `perm` · `sharer_tracker.impl` · `consistency_labels`);
> **其余 9 键**(`family` · `flow_support` 其余 8 位 · `atomic` · `ordering_assumption` · `vc_binding` ·
> `combination_capabilities`)由 **`ADR-CEP-12` Status Update(2026-11-06,per 台账 **Entry #239**)** 裁
> ⇒ **13 键分散在两份 ADR** ⇒ 本段与该段**双向交叉引用**,⛔ 读者不得误以为 13 键全裁于本段。
> **前置已满足**:§③残余所依赖的「Task 0 普查」**已完成**(per 台账 **Entry #237** §1.4 +
> `docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md` §3)⇒
> 本段**不是**「预先钉死」,而是该残余所预留的**裁决位**。
> **审查**:`.rddf/reviews/cep-b3a-b3b-ruling.md`(**oracle → critic**;**APPROVE_WITH_CONDITIONS**,三档均无 BLOCKER)。
> **关联 change**:`backlog: cpptlm-cep-b3a-b3b-advancement`(占位,立项后回填)。
> ⚠️ **本段不改本 ADR 的整体 Status**(仍为「起草中 v0.6」)—— 本段只裁 `ualink_4` 的取值。

### ① 四项取值裁决

| 键 | 取值 | basis | 依据 |
|---|---|---|---|
| `flow_support.snoop` / `snoop_resp` | `false` / `false` | **`public-nonnormative-derived`** | **4 个互相独立的公开非规范来源**一致:① Linux 内核 `docs.kernel.org/gpu/amdgpu/ualink.html`(7.4-rc1)——「direct loads/stores over the fabric」,**无 snoop 事务类型**(其 remote TLB shootdown 是 **TLB 失效**非 cache snoop)② AMD amd-gfx **95 个补丁**系列(2026-08-31)——「no copy involved; direct loads/stores over the fabric」,无 coherence 引擎 ③ UALink Consortium 官方博客(2025-05-15)——「**Coherency is maintained through software**」④ UnifabriX webinar(联盟官网托管,2025-12-10)——「software coherency」,并把 `Coherent (via CXL.mem/CXL.cache/CHI C2C)` 明确归给 **host 侧** |
| `perm` | `["NONE"]` | **`self-derived`** | N-9 实测:公开材料**零** permission / cache-state 词汇,且 UALink 被公开定义为 memory-semantic + software coherency ⇒ 该问法在 UALink 侧**无对应物** ⇒ 该值是**CEP 建模声明**;选取规则 per `ADR-CEP-02` **SU 2026-11-06**(最小可锚定子集规则 + `["NONE"]` 的语义边界) |
| `sharer_tracker.impl` | `不参与` | `self-derived` + 语义层判别依据 | §① 的判别依据 = 「协议**不需要 sharer-set 知识**」;`BroadcastNull` 的判别依据(「**有** snoop 类流量但无集中目录」)因 N-8 落地而**事实不适用** ⇒ `不参与` 是唯一判别依据成立的候选。caps 取**退化集** `NONE`/`EXACT`/`NONE`/`NONE`(per `chi_rnf` / `cxl_2_0` 两实例) |
| `consistency_labels.read` / `.write` | `PLO` / `PLO` | **假设登记** | 现有 5 值枚举**无** `software-coherence`;加值 = 契约扩张 + **零消费者** ⇒ 不做。`PLO` 是 5 值中**最弱**且唯一可由「memory-semantic」公开定义支撑的标签 ⇒ 以**假设**登记进既有证伪通道(per H 系列 litmus 可标 `violated`);登记载体 = 台账 Entry + 本段 |

> ⚠️ **N-8 的推断边界**(必须随声明一起留档):公开可钉版文本中**没有字面 "no snoop"**
> (该字面句只在**禁读**的规范正文与**不可钉版**的 YouTube 转录里)⇒
> 「software coherency ⇒ `snoop=false`」是**一步推断**,⛔ 不得标 `spec-pinned`;⛔ 本轮**未**读取任何规范 PDF 技术正文(ECA 同型约束照旧)。

### ② `perm` 与 `impl` 的**两个不可混淆点**

- `perm = ["NONE"]` 是**信息下限**(零可锚定值的唯一合法表达),⛔ **不是**「UALink 无缓存」的事实断言 —— per `ADR-CEP-02` **SU 2026-11-06** §④。
- `impl = 不参与` 与 `impl = BroadcastNull` 的 caps **逐字段相同**(§② 已登记的 caps 词表退化集局限)⇒
  **二者语义等价**(per `INV-CEP-04-IC7`)⇒ 该判据**不**区分二者 ⇒ **区分职责由理由登记承担**
  ⇒ ⛔ 理由登记**不得**写成「因为 caps 不同」。

### ③ 两项**登记面**的落地义务(**先补,后声明**)

| # | 义务 | 现状(实测) | 判据 |
|---|---|---|---|
| **O1** | `protocol_instances.json` **顶层** `sharer_tracker_rationales{}` 块(**7 条存量 + `ualink_4`**)+ 理由**存在性**元测试 | ⛔ **从未落地**(JSON 顶层**无**该键;全仓 `test/` **零命中**;git 历史**从未**添加)⇒ 本 ADR §④ 的「理由登记存在性」Verification 条目**至今未勾选** | 删任一条理由 ⇒ 元测试**按正确原因红** |
| **O2** | `caps ↔ kTrackerTraits` **逐字段**一致性元测试(`不参与` 参照**退化集**) | ⛔ **未落地**(`kTrackerTraits` 只在实现类头文件里被自引用,**无测试**比对实例声明 vs traits 行) | 改实例 caps 任一格 ⇒ 元测试**按正确原因红** |

### ④ `ualink_inc_state_spec.hh` 的 `Uis08` / `Uis09` 翻转(**载体侧**)

| 项 | 内容 |
|---|---|
| **翻转依据** | `Uis08`(`flow_support`)—— N-8 已由 4 个独立公开来源闭合;`Uis09`(`perm` 子集)—— **仍不可由公开来源闭合**,只能作为**建模声明** |
| **载体 `BasisKind` 的诚实映射** | 该载体的 `BasisKind` 只有 **`SelfDerived` / `Pending`** 两值 ⇒ ⛔ **不发明第三值** ⇒ `Uis08` / `Uis09` 翻转后 basis = **`SelfDerived`** + **锚点摘要新增 `C-10`** 条目(承载 4 来源 + 「一步推断」的裁决本体)⇒ 「非规范来源支撑的建模声明」在载体上的诚实表达 = **锚到仓内已签锚点**,⛔ 不是「宣称有规范依据」 |
| **同步范围** | 载体 `BasisCell` / `static_assert` / `:650` 的消息文本(`SHALL 保持 pending`)**同步改写**;锚点摘要新增 `C-10` + `N-8` 状态变更 + **「9 项 pending」计数同步** |
| **归属** | ⛔ **不由 builder 顺手改** —— 属 architect 在本 SU 的登记项,由 builder 在**声明实例之前**落地(顺序铁律) |

### ⑤ 顺序铁律(⛔ 倒序即拒收)

> ① architect 签本 SU ⇒ ② builder 落地 O1 / O2 + 锚点摘要 `C-10` + 载体翻转 ⇒ ③ builder 声明 `ualink_4` 实例 ⇒ ④ 计数链同步

| 步骤 | 内容 | 载体 |
|---|---|---|
| ④ 计数链 | `protocol_instances.json` 实例 **7 → 8** · `implemented` **7 → 8** · `no_upgrade_instances` **3 → 4** · `07 号` **三处**(§5.1.1 UALink 行 / §11 计数 / §6.2 SU 注记)· `ADR-CEP-11 §3` 锁表 8 行 vs 实例数的 **drift 自然消除** | builder 域 |

> ⚠️ **声明 ≠ 装配**:实例声明落地后,`07 号` §5.1.1 UALink 行**仍须保留**「实例级装配待 backlog」的措辞
> (声明的是**取值**,不是**装配闭环**)⇒ ⛔ 不得被读成「已闭环」。

### ⑥ 后果 / 代价

| 面 | 后果 |
|---|---|
| **§1 ~ §13 正文** | **零改动**(逐字未改) |
| **§③残余** | **正式收口** —— 「`ualink_4` 取值尚未裁决」**不再成立**;⛔ 若未来 UALink 规范公开且 INC 含 coherence 模式 ⇒ 本裁决**须重判**(trigger 已登记 ⇒ 成本可控) |
| **schema** | **零改动**(`perm` 保持 `minItems: 1`;`impl` 保持 7 值;`consistency_labels` 保持 5 值)⇒ **不触发**冻结面 `--update --authorized-by` |
| **新增声明面** | `protocol_instances.json` 顶层 `sharer_tracker_rationales{}`(**O1**)—— 顶层不受实例 schema 约束;镜像闸只逐实例比键 ⇒ **不触发**跨仓闸 |
| **代价** | ① `perm` / `consistency_labels` 两项是**建模声明 / 假设**,⛔ 不是 UALink 事实 ⇒ 该实例的**证据强度低于**其他 7 个;② N-8 的 basis 是**一步推断**;③ O1 / O2 两面**此前**未落地 ⇒ 本轮**先欠账再声明**;④ `no_upgrade_instances` 3→4 须回查与 `ADR-CEP-09` VC 依赖集的**隐性冲突**(排除理由必须可审计) |

### ⑦ Verification(前瞻判据 · 可勾选)

- [ ] O1:`protocol_instances.json` 顶层 `sharer_tracker_rationales{}` 已落地(7 条存量 + `ualink_4`)+ 理由存在性元测试
- [ ] O2:`caps ↔ kTrackerTraits` 逐字段一致性元测试(`不参与` 参照退化集)
- [ ] 锚点摘要已新增 `C-10`(4 来源 + 「一步推断」裁决本体)+ `N-8` 状态变更 + **「9 项 pending」计数同步**
- [ ] `ualink_inc_state_spec.hh` 的 `Uis08` / `Uis09` 已翻转为 `SelfDerived` + `C-10` 锚 + `:650` 消息文本同步改写
- [ ] `ualink_4` 实例已声明(13 键)+ `repo_owned_instance_ids` 已更新
- [ ] **决定性变异**:删理由登记任一条 ⇒ 元测试必红;改 `ualink_4` 的 caps 任一格 ⇒ 一致性元测试必红
- [ ] **存量计数断言已同步**:`test_cep_instance_declaration_contract.cc:108` · `test_cep_tilelink_instance_decl.cc:209` · `test_cep_ualink_inc.cc:398`(**7 → 8**)
- [ ] `07 号` §5.1.1 UALink 行 / §11 计数 / §6.2 SU 注记已同步,且**保留**「实例级装配待 backlog」措辞
- [ ] `cep_decl_alignment_check.sh --strict` 绿 · `cep_freeze_check.sh --strict` 绿 · `cep_prereg_drift_check.sh --strict` 绿 · `[cep]` 全绿

**关联**:台账 **Entry #237** · **#238** · **#193**(SU 2026-11-03 的授权)· **#170**(归因 id 纪律)·
`ADR-CEP-02` **SU 2026-11-02 R-T4** + **SU 2026-11-06**(`perm` 实例级语义)· `ADR-CEP-11 §3` · `ADR-CEP-12` ·
`ADR-CEP-09`(VC 依赖集)· `docs/research/2026-11-04-cep-ualink-inc-anchor-summary.md` §0 / §2bis / §3 ·
`include/tlm/cep/ualink_inc_state_spec.hh` · `include/tlm/cep/sharer_tracker.hh` ·
`.rddf/reviews/cep-b3a-b3b-ruling.md`(审稿报告)


---

## Status Update(2026-11-06 · 四,per 台账 Entry #250)—— `backlog:` 占位**回填**(⛔ 不改任何决策内容)

> **性质**:纯**指针回填**(append-only)。⛔ 不 reopen 任何决策、⛔ 不改正文、⛔ 不新增 invariant 编号。
> **原占位**:`backlog: cpptlm-cep-b3a-b3b-advancement`(占位,立项后回填)
> **回填**:该 backlog 条目已交付 —— `openspec/changes/archive/2026-11-06-cpptlm-cep-tilelink-t3-mapping-survey/`(T3 映射锚点普查,✅ 已完成归档)+ `openspec/changes/archive/2026-11-06-cpptlm-cep-tilelink-runtime-engine/`(TileLink-C 运行时引擎,✅ 已完成归档);剩余 4 族处置见 `07 号` **SU 2026-11-06 · 四** + 台账 **Entry #249**。
