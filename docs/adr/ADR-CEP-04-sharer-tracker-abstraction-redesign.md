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
