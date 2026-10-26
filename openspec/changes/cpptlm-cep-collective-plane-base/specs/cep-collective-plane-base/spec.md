## ADDED Requirements

### Requirement: cep-collective-plane-base

CEP SHALL 将 **Fabric Collective Plane** 固化成**可执行契约**:`CollectivePlaneIf` 单接口 + 类型集 + `NullCollectivePlane` + `LocalCollectivePlane`(group 生命周期 / region-epoch / BARRIER rendezvous)。

> **定位**:per `ADR-CEP-03 §2/§3` + `ADR-CEP-01 Decision 2`;旁路平面,与 Directory 侧**零调用边**。
> **行为化既有编号**(per `ADR-CEP-07` / `ADR-CEP-04`;**非新增**):`INV-CEP-07-IC3`、`INV-CEP-04-IC5`。
> **红绿**:判据退化 = 用例红;判定为红 = 用例**仍绿** + 结论登记为交付物。
> **积分面**:header-only、testbench 定位(不进 `ModuleFactory`)。

#### Scenario: `CollectivePlaneIf` 方法集与 `ADR-CEP-03 §2 Decision 2` 逐字对齐

- **WHEN** 核对 `CollectivePlaneIf` 的方法集
- **THEN** SHALL 恰含 group 生命周期 3 个(`group_create` / `group_destroy` / `group_register_region`)+ 数据路径 2 个(`send` / `recv`)+ `collective_barrier` + `query_capability`
- **AND** 原语枚举 SHALL **恰好 4 值** = `{REDUCE, BROADCAST, MULTICAST, BARRIER}`;AllReduce **不得**作为原语出现
- **AND** `group_id` 在本接口 SHALL 是一等公民(等价于 address 对 coherence plane 的通用性,per `ADR-CEP-03 §2 Decision 2`)

> **锚点**:`test_cep_collective_plane_base.cc` 的接口面断言(方法集 + 原语值域双向)

#### Scenario: 类型集齐备且 `CollectiveEvt` 含 Fault 变体

- **WHEN** 核对 collective 平面用到的类型集
- **THEN** SHALL 齐备:`group_id_t` / `member_set_t` / `group_attrs_t` / `addr_range_t` / `access_mode_t` / `CollectiveReq` / `CollectiveEvt` / `cap_t`
- **AND** `CollectiveReq` SHALL 携 group-scoped 字段:`group_id` / `op` / `tag`(相位/轮次)/ `src_member` / `payload_desc` / `reduce_op` / `dtype` / `multicast_mask`
- **AND** `CollectiveEvt` SHALL **恰好 3 个变体** = `Result(gid, tag, payload)` | `Completion(gid, tag)` | **`Fault(gid, culprit, cause)`** —— Fault 变体的存在即 partial failure 的**显式建模**(per `ADR-CEP-01 §3.3` 抓取点 6)
- **AND** `CollectiveEvt` 的三变体 SHALL **不得**退化为「只有 Result + Completion」(缺 Fault ⇒ 成员掉线将成不可见死锁点)

> **锚点**:`test_cep_collective_plane_base.cc` 的类型集断言(含 Fault 变体存在性)

#### Scenario: `NullCollectivePlane` 全原语返回「不支持」(**行为化** `INV-CEP-07-IC3`)

- **WHEN** 对 `NullCollectivePlane` 逐一调用全部原语(含 `group_create` / `group_destroy` / `group_register_region` / `send` / `recv` / `collective_barrier` / `query_capability`)
- **THEN** 每一个 SHALL 返回「**不支持**」语义(不得有任何原语产生副作用、不得建 group、不得投递事件)
- **AND** SHALL 保持 CHI / CXL 配置下的**默认绑定**地位(per `07 号 §6.4`)
- **AND** 「不支持」SHALL 有**可观测的返回通道**:每个原语调用后,实现的**状态查询口** SHALL 报「不支持」
  (per `FullMapDirectory::event_count` 同类先例 —— 状态查询是 `CollectivePlaneIf` 7 方法之外的**只读辅助口**)
- **AND** **反向 fixture**:注入一个「假装支持」的 plane ⇒ 断言必红(纯负向断言无正控 = 判据可能根本没生效)

> **锚点**:`test_cep_collective_plane_base.cc` 的 Null 全原语断言 + 反向 fixture

#### Scenario: `LocalCollectivePlane` group 生命周期状态机可执行

- **WHEN** 走 `group_create` → `group_register_region` → `group_destroy` 的合法路径
- **THEN** 三步 SHALL 均成功,且 group 状态 SHALL 可观测(create 后 group 可查;register 后 region 表含该 range;destroy 后 region 表移除且 epoch 失效)
- **AND** `group_destroy` 对 **in-flight 操作** SHALL 发 **`Fault(gid, culprit, cause)`** 且 **不阻塞**(本 change 对 `ADR-CEP-03 §3.1` R4 留白的拍板,per design D-ζ)
- **AND** `group_destroy` 后该 region SHALL **从 region 表中移除**(⇒ 该地址回到「无 region」状态),
  per R4「区域回归 coherence 域」—— **只**把 epoch 置为 invalid 而**不移除**会留下**残留**:
  重叠检查遍历全表不分 group 死活 ⇒ 该地址段被**永久报废**,新 group 再也注册不进去
- **AND** `collective_barrier` 到达 SHALL 校验**成员身份**:非成员到达 SHALL 被拒绝
  (SHALL NOT 计入阈值)—— 否则「N-1 到达不释放」被绕过,破掉本 plane **唯一的 ordering 承诺**
- **AND** `recv` 在**无事件**时 SHALL 报「无事件」状态并清空出参(调用方 SHALL 能区分
  「无事件」与「取到事件」;轮询式消费者不得把旧事件误读为新事件)

> **锚点**:`test_cep_collective_plane_base.cc` 的生命周期状态机断言(含 in-flight ⇒ Fault)

#### Scenario: BARRIER rendezvous —— N-1 到达不释放、N 到达释放

- **WHEN** 一个 N 成员 group 在 BARRIER 原语上陆续到达
- **THEN** 到达第 N-1 个成员时 SHALL **不**释放;到达第 N 个成员时 SHALL **释放全部**(group-scoped 完成纪律,per `ADR-CEP-01 Decision 2`)
- **AND** **同相位内** SHALL **允许多个成员先后到达且互不排斥**(到达第 N-1 个后,另一成员仍可到达 —— 可观察形态,不要求并发)
- **AND** **跨相位** SHALL **有序**(相位 N+1 的完成 SHALL 不得早于相位 N 的释放)—— 这是本 plane 唯一的 ordering 承诺(per `ADR-CEP-03 §2`)
- **AND** **反向 fixture**:阈值改 N-1 ⇒ 断言必红

> **锚点**:`test_cep_collective_plane_base.cc` 的 rendezvous 阈值断言 + 反向 fixture

#### Scenario: epoch invalid 窗口 = **正常服务放行**(非 fail-stop)(**行为化** `INV-CEP-04-IC5`)

> **分层声明(本 change 与 P3 的边界)**:本条交付的是**判读纯函数** ——「某地址的 collective 归属状态」判定
> (`R2` 谓词 `(dest_addr ∩ collective_region_table) && group_epoch_valid` 的**判读侧**)。
> **fabric ingress 的「接线」**(把判定结果接到 fabric 路由上)属 **P3**(`cpptlm-cep-nvlink-sharp-adapter`)。
> 之所以必须这样切:旁路平面**没有** coherence 路径可「走」,「放行」在 plane 侧的可观察形态
> 就是**判定结果为「不 divert」** —— 若把整条 R2 推迟到 P3,本条将无任何可测载体。

- **WHEN** 对某地址求 collective 归属判定,且该地址**命中** collective region 表、但其 group 的 **epoch 尚未 commit**(epoch invalid)
- **THEN** 判定结果 SHALL 为 **「已注册但 epoch 未生效」⇒ 不 divert**(= 放行正常服务),**不得**判为 divert、**不得** fail-stop(per `ADR-CEP-03 §3.1` R4「epoch 未 commit 前 = 正常服务」)
- **AND** epoch commit **之后**同一地址 SHALL 判定为 **「已注册且生效」⇒ divert**
- **AND** 未命中 region 表的地址 SHALL 判定为 **「无 region」⇒ 不 divert**
- **AND** 该判定 SHALL 有**只读观测口**(本 plane 的辅助查询,非 `CollectivePlaneIf` 的 7 方法之一 —— per `FullMapDirectory::view_of` 同类先例)⇒ 断言可写
- **AND** **反向 fixture**:把 epoch 有效性检查删掉(即永远判 divert)⇒ 断言必红

> **可构造性**:epoch-invalid 窗口由**两阶段**构成 —— `group_register_region` 返回后 epoch 仍为 invalid,
> 直至 drain 完成回告(`complete_drain` 类的**装配/驱动用**辅助口,per `FullMapDirectory::set_sharer` 同类先例)
> ⇒ 窗口**非零宽度**、可稳定构造,不依赖竞态。

> **锚点**:`test_cep_collective_plane_base.cc` 的 epoch 三态窗口断言 + 反向 fixture

#### Scenario: 区域互斥(CEP-13 语义)与 epoch 创建的前置条件

- **WHEN** 注册一个与已注册 region **重叠**的 range
- **THEN** SHALL 返回**明确的「区域重叠」状态**(立即报错,不静默),且**不得**创建新 epoch
  (状态字面 = `RegionOverlap`;比笼统的 `group_register_error` 更精确 —— 两者都是错误,
  但「重叠」与「未注册」是两种不同的运维情形)
- **AND** **drain 失败**时 SHALL **不建 epoch**(per `ADR-CEP-03 §3.2` CEP-13:「drain 失败 ⇒ `group_register_region` 返回 error,不创建 epoch」)
- **AND** **正控**:不重叠的合法注册 SHALL 成功
- **AND** 同一地址 SHALL **不得**同时「SHARP 已注册」且「coherence-cacheable」(CEP-13 不变量)

> **锚点**:`test_cep_collective_plane_base.cc` 的区域互斥断言(重叠 ⇒ error / 合法 ⇒ 成功)

#### Scenario: 未注册区域上的 collective 操作 SHALL **立即报错**,不静默

- **WHEN** 对**未注册**的 region 发起 REDUCE / BROADCAST / MULTICAST
- **THEN** SHALL **立即报错**(per `ADR-CEP-03 §3.4` guardrail:未注册区域的操作 = `group_register_error`,不静默)
- **AND** **不得**投递任何事件、**不得**建 group —— 错误**只经状态**传达
  (事件通道留给「已注册但被中断」这类真正的 partial failure)
- **AND** **反向 fixture**:静默返回 ⇒ 断言必红

> **锚点**:`test_cep_collective_plane_base.cc` 的未注册区域断言 + 反向 fixture

#### Scenario: 已注册且 epoch 生效的区域上,数据原语 SHALL **显式声明未实现**(不静默)

> **补这条的理由**:只规定「未注册区域 ⇒ 报错」会留下另一半 —— **已注册区域上** `send(REDUCE)` 到底返回什么?
> 若不规定,接口就「承诺了但行为缺失」,调用方无从判断是 bug 还是未实现。

- **WHEN** 对一个**已注册**且 **epoch 已生效**的区域发起 `send(REDUCE)` / `send(BROADCAST)` / `send(MULTICAST)`
- **THEN** SHALL 返回**明确的「数据路径未实现」状态**(与「未注册区域 ⇒ `group_register_error`」**可区分**)
- **AND** SHALL **不投递**任何 `CollectiveEvt`、**不**产生副作用
- **AND** **反向 fixture**:静默返回(无状态、无事件、无副作用)⇒ 断言必红

> **锚点**:`test_cep_collective_plane_base.cc` 的「未实现 ≠ 静默」断言 + 反向 fixture

#### Scenario: functional 模式下 collective SHALL **不产生 latency**

- **WHEN** 在 functional 模式(`simulation_mode` 顶层唯一维度,per `INV-CEP-07-IC1`)下跑 collective 原语
- **THEN** SHALL **不产生** latency 字段 / 周期数(与 `INV-CEP-07-IC6`「functional 模式下 `avg_latency_cycles == 0`」兼容)
- **AND** BARRIER 的完成 SHALL 是**零延迟即时完成**(不得引入等待周期)

> **锚点**:`test_cep_collective_plane_base.cc` 的 functional 模式断言

#### Scenario: `query_capability` 的边界契约 SHALL 写死

- **WHEN** 以**未知 `op`** 或 **`max_bytes == 0`** 调用 `query_capability`
- **THEN** 返回契约 SHALL 明确且**唯一**(不依赖实现细节):SHALL 返回「不支持」语义的 capability 值,**不得**抛异常、**不得**返回未定义值
- **AND** 已支持的 `op` SHALL 返回其 capability 值

> **锚点**:`test_cep_collective_plane_base.cc` 的 capability 边界断言

#### Scenario: `group_id` SHALL **不得**出现在 Coherence Interface 侧

- **WHEN** 核对 Coherence Interface 侧的接口与契约
- **THEN** `group_id` / `member_set_t` / `CollectiveReq` / `CollectiveEvt` 等 group-scoped 字段 SHALL **不出现**(完成域判别规则的**反向**面:requester-scoped → Coherence / group-scoped → Collective,per `ADR-CEP-01 Decision 2`)
- **AND** canonical 10 流的**清单与语义 SHALL 零 diff**(per `ADR-CEP-01 §3.3` 抓取点 4)

> **锚点**:`test_cep_collective_plane_contract.cc` 的完成域判别断言(与 zero-call-edge 闸**正交**)

#### Scenario: 零调用边 —— collective 侧 SHALL **不**依赖 directory / ordering / sharer 侧

- **WHEN** 扫描 collective 头文件的**直接 include** 与编译期依赖
- **THEN** SHALL **不出现** directory / ordering / sharer 相关头(per `ADR-CEP-01 §3.3` 抓取点 1:「Collective Plane → Directory 的调用边必须为零(可机器检查)。存在即层级纠缠」)
- **AND** **R3 的 drain** SHALL 由**发起注册的 node 端**经**自身** Coherence Interface 端口发出,collective plane 只输出 invalidate 请求清单 —— **plane 间零直接调用边**(per `ADR-CEP-03 §3.4`)
- **AND** **反向 fixture**:注入一条 directory include ⇒ 断言必红

> **锚点**:`test_cep_collective_plane_contract.cc` 的 include-graph 闸断言 + 反向 fixture
