# gem5 Garnet Network 内部实现 + 拓扑抽象层调研

调研目标仓：`/workspace/main/gem5/`（只读）。

**关键前提**：本 checkout 中**不存在** `src/mem/ruby/network/garnet2.0/` 目录，也不存在 garnet 1.0 standalone 网络模型目录；唯一的网络实现是 `src/mem/ruby/network/garnet/`，即 Garnet 3.0（`GarnetNetwork.hh:70` `garnetVersion = "3.0"`；`garnet/README.txt:1` "README for Garnet3.0"）。命令行的 `--network=garnet2.0` 只是 legacy 别名（`configs/network/Network.py:131-137` 警告后映射为 `garnet`；`RELEASE-NOTES.md:1739`）。`standalone` 一词在本仓指的是 Ruby coherence protocol `Garnet_standalone`（合成流量协议，`configs/ruby/Garnet_standalone.py`、`src/mem/ruby/protocol/Garnet_standalone*.sm`），不是独立网络模型。下文所有 "garnet2.0 差异" 均按"garnet1.0 有独立 VA 级、2.0+ 合并进 SA"的历史差异解释（`SwitchAllocator.cc:81-88`、`README.txt:85`）。同理 `src/mem/ruby/network/topologies/` 不存在，拓扑在 `configs/topologies/`（Python，运行时）+ `src/python/gem5/components/cachehierarchies/ruby/topologies/`（库 API）。

---

## A. Garnet 3.0 router pipeline

### A.1 `src/mem/ruby/network/garnet/` 文件清单（每个 50 字内）

| 文件 | 职责 |
|------|------|
| `Router.hh/.cc` | 顶层 router：持有 Input/OutputUnit 数组 + RoutingUnit/SwitchAllocator/CrossbarSwitch，`wakeup()` 驱动整条流水线 |
| `InputUnit.hh/.cc` | 每个输入端口：从 NetworkLink 收 flit、HEAD 做 route compute、缓存进 VC、回送 credit |
| `OutputUnit.hh/.cc` | 每个输出端口：跟踪 downstream VC 的 credit（OutVcState）、消费 credit、把 flit 放入 outBuffer |
| `VirtualChannel.hh/.cc` | 输入 VC：flitBuffer + IDLE/ACTIVE 状态 + output_port/output_vc |
| `OutVcState.hh/.cc` | 单个输出 VC 的信用计数 + 状态；上限取自 buffers_per_data/ctrl_vc |
| `SwitchAllocator.hh/.cc` | 两段式 SA-I/SA-II + VC 分配（garnet3 无独立 VA 级） |
| `CrossbarSwitch.hh/.cc` | Switch Traversal：把 SA 赢家 flit 从 switchBuffer 送到 OutputUnit |
| `RoutingUnit.hh/.cc` | 路由计算：weight-based table / XY / custom 三种 |
| `NetworkLink.hh/.cc` | forward link，带流水线时延把 flit 送到下游 |
| `CreditLink.hh` | backward 信用链路；仅头文件，是 NetworkLink 的空子类 |
| `Credit.hh/.cc` | 信用信号（flit 子类，type=CREDIT_，含 is_free_signal） |
| `flit.hh/.cc` | flit 数据结构（packet id/vc/vnet/route/type/stage） |
| `flitBuffer.hh/.cc` | 按 `m_time` 的时间有序 buffer（isReady 判时间） |
| `NetworkInterface.hh/.cc` | NI：message↔flit 转换、与本侧 router 的信用/流控、与协议 buffer 对接 |
| `NetworkBridge.hh/.cc` | HeteroGarnet 的 CDC/SerDes 桥（时钟域/位宽转换） |
| `GarnetNetwork.hh/.cc` | 网络顶层：建 router/link/NI、收集统计、`createLinks` |
| `GarnetNetwork.py` | GarnetNetwork/GarnetRouter/GarnetNetworkInterface 参数定义 |
| `GarnetLink.py` | NetworkLink/CreditLink/NetworkBridge/GarnetIntLink/GarnetExtLink 参数 |
| `CommonTypes.hh` | 枚举：flit_type / VC_state_type / flit_stage / link_type / RoutingAlgorithm + RouteInfo |
| `SConscript` / `README.txt` | 构建 + Garnet3.0 code-flow 说明 |

### A.2 5-stage pipeline 边界与精确顺序

`flit_stage` 枚举：`I_, VA_, SA_, ST_, LT_`（`CommonTypes.hh:51`）。注意 **VA_ 已废弃不用**：garnet1.0 有独立 VCAllocator 级，garnet3.0 把 VC allocation 折叠进 SA-II（`SwitchAllocator.cc:81-88`、`README.txt:85`）。因此实际流水线是：Buffer Write / Route Compute（InputUnit）→ SA → ST → LT。

`Router::wakeup()` 精确顺序（`Router.cc:71-97`）：
1. `Router.cc:78-80` 循环所有 InputUnit 调 `InputUnit::wakeup()`（收 flit + route compute）。
2. `Router.cc:88-90` 循环所有 OutputUnit 调 `OutputUnit::wakeup()`（**credit 更新发生在 SA 之前**，注释 `Router.cc:82-87` 说明 buffer turnaround = credit traversal(1) + SA(1) + Link(1)）。
3. `Router.cc:93` `switchAllocator.wakeup()`（SA）。
4. `Router.cc:96` `crossbarSwitch.wakeup()`（ST）。

各阶段边界：
- **BW/RC**：`InputUnit::wakeup()`（`InputUnit.cc:76-140`）。HEAD/HEAD_TAIL 在 `InputUnit.cc:97` 调 `Router::route_compute()`（`Router.cc:161-165`）→ `RoutingUnit::outportCompute()`，结果写入 VC（`InputUnit.cc:103` `grant_outport`）。flit 插入 VC（`InputUnit.cc:111`）。时延建模：`pipe_stages==1` 直接进 SA（`InputUnit.cc:120-123`），否则等 `pipe_stages-1` 周期（`InputUnit.cc:124-134`）。
- **SA**：`SwitchAllocator::wakeup()`（`SwitchAllocator.cc:90-98`）→ `arbitrate_inports()`（SA-I）→ `arbitrate_outports()`（SA-II，含 VC allocation）→ `clear_request_vector()` → `check_for_wakeup()`。赢家 `advance_stage(ST_)`（`SwitchAllocator.cc:220`）+ `Router::grant_switch`（`SwitchAllocator.cc:221`）→ `CrossbarSwitch::update_sw_winner`（`Router.cc:167-171`）。
- **ST**：`CrossbarSwitch::wakeup()`（`CrossbarSwitch.cc:65-92`）。对每个 switchBuffer，若 `is_stage(ST_)`（`CrossbarSwitch.cc:78`）则 `advance_stage(LT_, clockEdge(1))`（`CrossbarSwitch.cc:82`）并 `OutputUnit::insert_flit`（`CrossbarSwitch.cc:87`）。
- **LT**：`NetworkLink::wakeup()`（`NetworkLink.cc:82-110`）。取 link_srcQueue 顶 flit，`set_time(clockEdge(m_latency))`（`NetworkLink.cc:100`），插入 linkBuffer，`link_consumer->scheduleEventAbsolute(clockEdge(m_latency))`（`NetworkLink.cc:102`）。

#### SwitchAllocator 仲裁算法
- **SA-I（输入仲裁）`arbitrate_inports()`（`SwitchAllocator.cc:110-146`）**：每个输入端口独立 round-robin，起点 `m_round_robin_invc[inport]`；对每个 VC 若 `need_stage(SA_, curTick())`（`SwitchAllocator.cc:121`），调 `send_allowed()` 判断可发，第一个通过者记 `m_port_requests[inport]=outport`、`m_vc_winners[inport]=invc` 并 `break`（`SwitchAllocator.cc:132-138`）。
- **SA-II（输出仲裁）`arbitrate_outports()`（`SwitchAllocator.cc:162-267`）**：每个输出端口独立 round-robin，起点 `m_round_robin_inport[outport]`；若 `m_port_requests[inport]==outport`（`SwitchAllocator.cc:175`）则：
  - 若 `outvc==-1` 调 `vc_allocate()`（`SwitchAllocator.cc:185`）→ `OutputUnit::select_free_vc`（`OutputUnit.cc:110-122`）；
  - `getTopFlit`（`SwitchAllocator.cc:189`）、`set_outport/set_vc`（`210/214`）、`decrement_credit(outvc)`（`SwitchAllocator.cc:217`）；
  - `advance_stage(ST_)`（`220`）、`grant_switch`（`221`）；TAIL/HEAD_TAIL 释放 VC 并发送 `free=true` credit（`224-235`），否则发 `free=false` credit（`236-240`）；
  - 更新两个 RR 指针（`246-256`）。
- `send_allowed()`（`SwitchAllocator.cc:283-338`）：HEAD/HEAD_TAIL 要求 output port 有空闲 VC（`300-307`）；BODY/TAIL 要求有 credit（`309`）；ordered vnet 做入队时间顺序检查（`318-335`）。
- `check_for_wakeup()`（`SwitchAllocator.cc:356-373`）：下周期仍有 ready SA flit 则 `schedule_wakeup(1)`。

#### VC 分配的死锁避免
- 信用上限即 buffer 深度：`buffers_per_data_vc`（默认 4）/`buffers_per_ctrl_vc`（默认 1）（`OutVcState.cc:55-63`、`GarnetNetwork.py:47-48`）；BODY/TAIL 只有 `has_credit` 才能发（`OutputUnit.cc:88-93`）。
- VC 只能从**同 vnet** 的空闲 VC 中分配（`OutputUnit::has_free_vc` `OutputUnit.cc:97-107`、`select_free_vc` `110-122`）。
- NI 侧死锁探测：`calculateVC()` 若某 vnet 长期无空闲 VC 且计数超 `garnet-deadlock-threshold`（默认 50000，`GarnetNetwork.py:52`）则 `panic`（`NetworkInterface.cc:484-487`）。
- 路由级死锁自由靠 link weight 限制转向（见 F 节）。

### A.3 Credit-based flow control

**CreditLink**：`CreditLink.hh:46-51` 只是 `NetworkLink` 的空子类（无独立 .cc），复用 NetworkLink 的流水线时延机制。携带 `Credit`（`Credit.cc:48-53`，`m_type=CREDIT_`，含 `m_is_free_signal`）。

**发（生产）时序**：
- `InputUnit::increment_credit(in_vc, free_signal, curTime)`（`InputUnit.cc:144-152`）：`new Credit(in_vc, free_signal, curTime)` → 插入 `creditQueue` → `m_credit_link->scheduleEventAbsolute(clockEdge(1))`。
- 由 SA-II 调用：TAIL/HEAD_TAIL `free=true`（`SwitchAllocator.cc:235`），其余 `free=false`（`SwitchAllocator.cc:239`）。

**收（消费）时序**：
- `OutputUnit::wakeup()`（`OutputUnit.cc:132-148`）：若 `m_credit_link->isReady` 则 `consumeLink` → `increment_credit`；若 `is_free_signal` 则 `set_vc_state(IDLE_)`。
- 在 `Router::wakeup` 中该步先于 SA（`Router.cc:88-90`）。

**信用消费路径**：`OutputUnit::decrement_credit`（`OutputUnit.cc:61-71`）在 SA-II 时扣减（`SwitchAllocator.cc:217`）；`OutVcState::decrement/increment_credit` 带上下界 assert（`OutVcState.cc:66-78`）。

**NI 侧信用**：NI 直接向 router 汇报（NI 自己不缓存收到的 flit）：收到非 tail 回 `free=false`（`NetworkInterface.cc:273`），收到 tail 回 `free=true`（`NetworkInterface.cc:254`；stall 解除后同样 `NetworkInterface.cc:344`）；NI 发送 flit 时扣减自己的 outVcState（`NetworkInterface.cc:530`），收到信用时增加（`NetworkInterface.cc:288-299`）。

### A.4 flit 类型 + buffer 处理
`flit_type`：`HEAD_, BODY_, TAIL_, HEAD_TAIL_, CREDIT_`（`CommonTypes.hh:47`）。归类逻辑在 `flit::flit()`（`flit.cc:64-74`）：`size==1` → HEAD_TAIL_；`id==0` → HEAD_；`id==size-1` → TAIL_；否则 BODY_。

处理差异：
- InputUnit：HEAD/HEAD_TAIL → `set_vc_active` + route compute + `grant_outport`（`InputUnit.cc:90-104`）；BODY/TAIL 要求 VC 已 ACTIVE（`InputUnit.cc:105-107`）。
- SA：HEAD/HEAD_TAIL 需要 outvc 分配（`SwitchAllocator.cc:295-307`）；BODY/TAIL 需要 credit（`SwitchAllocator.cc:308-310`）。
- TAIL/HEAD_TAIL 触发 VC 释放 + free credit（`SwitchAllocator.cc:224-235`）。
- NI 接收：TAIL/HEAD_TAIL → 重组 message（`NetworkInterface.cc:242-270`）；非 tail flit 直接回信用并删除 flit（`NetworkInterface.cc:271-282`）。

#### pipeline 时序图（单 router，pipe_stages=1）
```
cycle:    t0        t1        t2        t3        t4
          |         |         |         |         |
Link in:  [flit arrives]                              (NetworkLink::wakeup, NetworkLink.cc:82)
InputUnit:          [recv+RC]                          (InputUnit.cc:76, HEAD route @97)
                    (advance I->SA @123)
SA:                           [SA-I/SA-II]             (SwitchAllocator.cc:90)
                              (dec credit @217; advance SA->ST @220)
Crossbar(ST):                           [ST]           (CrossbarSwitch.cc:65, advance ST->LT @82)
                                        (insert outBuffer)
Link out:                                         [LT ...]
Credit back:                  <- credit generated @SwitchAllocator.cc:235/239
                              (InputUnit::increment_credit @144)
```
buffer turnaround = credit traversal(1) + SA(1) + Link traversal(1)（`Router.cc:83-87`）。

---

## B. "standalone vs garnet2.0" 的真实差异

- 本仓**没有** `garnet/`（1.0 standalone）与 `garnet2.0/` 两套目录；只有一套 `garnet/` = 3.0。`--network=garnet2.0` 是别名（`configs/network/Network.py:131-137`）。`choices` 为 `["simple","garnet"]`（`Network.py:58`）。
- 历史真实差异（由代码注释佐证）：
  1. **VCAllocator 级**：garnet1.0 有独立 VA 级；2.0+/3.0 合并进 SA-II —— 见 `SwitchAllocator.cc:81-88`（"There is no separate VCAllocator stage like the one in garnet1.0"）与 `README.txt:85`。`CommonTypes.hh:51` 仍保留 `VA_` 枚举仅为兼容，实际不再使用。
  2. **HeteroGarnet 新增**：`NetworkBridge` 做 CDC/SerDes（`README.txt:72-78`、`GarnetLink.py:66-76`、`NetworkBridge.hh/.cc`）。
  3. garnet1.0 standalone 是独立网络仿真器；本仓集成版挂在 Ruby 之下（`GarnetNetwork` 继承 `Network`，`GarnetNetwork.cc:64`）。
- `Garnet_standalone` 是 Ruby 协议（合成流量），配套 `garnet_standalone` 参数在 Sequencer/GPUCoalescer（`Sequencer.py:104`），不是网络模型差异。

---

## C. NetworkInterface（NI）

- **切分函数名不是 `messageToFlits`，实际是 `flitisizeMessage`**（`NetworkInterface.hh:297` 声明，`NetworkInterface.cc:373-465` 定义）。
  - 切分规则：`num_flits = ceil(MessageSizeType_to_int(msgSize) / oPort->bitWidth())`（`NetworkInterface.cc:386-387`）。
  - multicast → unicast 拆分（`NetworkInterface.cc:394-428`）；为每个目的节点建 `RouteInfo`（vnet/net_dest/src_ni/src_router/dest_ni/dest_router，`434-444`，`hops_traversed=-1` 使首跳自增为 0）。
  - 循环创建 num_flits 个 flit 并插入 `niOutVcs[vc]`（`NetworkInterface.cc:449-459`），`outVcState[vc].setState(ACTIVE_)`（`462`）。
  - VC 选择 `calculateVC(vnet)`（`NetworkInterface.cc:468-490`），round-robin 取空闲 VC，否则计数判死锁。
- **反向重组**：没有单独命名的 de-flitisize 函数，逻辑内联在 `NetworkInterface::wakeup()` 的入 port 循环（`NetworkInterface.cc:229-284`）：收到 TAIL/HEAD_TAIL 时把 `t_flit->get_msg_ptr()` enqueue 进 `outNode_ptr[vnet]` 协议 buffer（`247-250`）；buffer 满则进 stall queue（`265-269`），由 `checkStallQueue()`（`319-370`）解 stall。非 tail flit 直接回 credit（`271-282`）。
- **两侧接口**：
  - controller 侧：`addNode(in, out)`（`NetworkInterface.cc:130-142`）设置 `inNode_ptr/outNode_ptr`（MessageBuffer）。
  - router 侧：`addInPort`（`66-82`）、`addOutPort`（`84-128`）；端口用内部类 `OutputPort`/`InputPort`（`NetworkInterface.hh:94-268`）。
  - 发送调度：`scheduleOutputLink/scheduleOutputPort`（`NetworkInterface.cc:492-567`）、`scheduleFlit`（`599-614`）。

---

## D. Topology 抽象

**注意：`src/mem/ruby/network/topologies/` 在本 checkout 不存在。** 拓扑文件在 `configs/topologies/`：

| 文件 | 30 字内说明 |
|------|------|
| `BaseTopology.py` | 定义 `BaseTopology`/`SimpleTopology` 基类与 `makeTopology` 契约（`:30-77`） |
| `Pt2Pt.py` | 点对点：每 controller 一 router，全互连（`:34`） |
| `Crossbar.py` | simple 网络用：N+1 个 router 构成集中 crossbar（`:33`） |
| `CrossbarGarnet.py` | garnet 用：单个 router 内部模拟 crossbar（`:33`） |
| `Mesh_XY.py` | 2D mesh + XY（weight 强制维序）（`:40`） |
| `Mesh_westfirst.py` | 2D mesh + west-first 路由（`:44`） |
| `MeshDirCorners_XY.py` | 4 个角放 directory 的 mesh（`:38`） |
| `CustomMesh.py` | 可配 cross link 的灵活 mesh（`:50`） |
| `Cluster.py` | 层次化 cluster 拓扑，可嵌套（`:30`） |
| `__init__.py` | 空 |
| 另有 `src/python/gem5/components/cachehierarchies/ruby/topologies/simple_pt2pt.py:35` | 库 API 版 `SimplePt2Pt(SimpleNetwork)` |

### BasicLink.py 精确字段（`BasicLink.py:33-75`）
`BasicLink`：`link_id`、`latency`（默认 1）、`bandwidth_factor`（simple only）、`weight`（默认 1，"used to restrict routing in shortest path analysis"）、`supported_vnets`。`BasicExtLink`：`ext_node`、`int_node`、`bandwidth_factor=16`。`BasicIntLink`：`src_node`、`dst_node`、`src_outport`、`dst_inport`（garnet only）、`bandwidth_factor=16`。

### BasicRouter.py 精确字段（`BasicRouter.py:31-39`）
`BasicRouter(ClockedObject)`：`router_id`（Int）、`latency`（Cycles，默认 1，"number of cycles inside router"，garnet 用作流水线级数）。

`Network.py`（`src/mem/ruby/network/Network.py:37-69`）`RubyNetwork` 字段：`topology`(String)、`number_of_virtual_networks`、`control_msg_size`(8)、`ruby_system`、`routers`、`netifs`、`ext_links`、`int_links`、`in_port/out_port`、`data_msg_size`。

### makeTopology 调用链
1. `configs/ruby/Ruby.py:245` `Network.create_network(options, ruby)` → 返回 `(network, IntLinkClass, ExtLinkClass, RouterClass, InterfaceClass)`（`configs/network/Network.py:128-165`）。
2. `configs/ruby/Ruby.py:210-220` `create_topology(controllers, options)`：`import_module(f"topologies.{options.topology}")`，实例化 `topology_class(controllers=controllers)`。
3. `configs/ruby/Ruby.py:265-267` `topology.makeTopology(options, network, IntLinkClass, ExtLink)`。
4. `configs/ruby/Ruby.py:274` `Network.init_network(options, network, InterfaceClass)`。
5. C++ 侧：`Network` 构造建 `Topology`（`Network.cc:104-107`）→ `GarnetNetwork::init()` 调 `m_topology_ptr->createLinks(this)`（`GarnetNetwork.cc:122`）→ `Topology::createLinks`（`Topology.cc:119-220`）→ `makeLink`（`255-336`）→ `net->makeExtInLink/makeExtOutLink/makeInternalLink`（`GarnetNetwork.cc:162-366`）。

### 写一个自定义 Mesh_XY 变体的最小步骤
1. 新建 `configs/topologies/MyMesh.py`，`class MyMesh(SimpleTopology): description="MyMesh"`，`__init__(self, controllers): self.nodes = controllers`。
2. 实现 `makeTopology(self, options, network, IntLink, ExtLink, Router)`：建 Router 列表 → 建 ExtLink（controller↔router）→ 建 IntLink（router↔router，带 `src_outport/dst_inport/weight/latency`）→ 赋 `network.routers/ext_links/int_links`（参照 `Mesh_XY.py:49-198`）。
3. 若用 XY/custom 算法，确保 `--mesh-rows` 设置（`num_rows/num_cols`，`GarnetNetwork.cc:124-135`）。
4. （可选）实现 `registerTopology(options)` 供 SE faux FS。
5. 运行：`--network=garnet --topology=MyMesh --routing-algorithm={0|1|2}`（`configs/network/Network.py:43-104`）。

#### ASCII 拓扑图（4x4 Mesh_XY）
```
(R) router  (C) controller/NI
  (R)--E-->(R)--E-->(R)--E-->(R)     East/West weight = 1  (Mesh_XY.py:136,155)
   |        |        |        |      North/South weight = 2 (Mesh_XY.py:174,193)
   S        S        S        S      -> XY 维序，保证无环
   |        |        |        |
  (R)--E-->(R)--E-->(R)--E-->(R)
   |        |        |        |
   S        S        S        S
   |        |        |        |
  (R)--E-->(R)--E-->(R)--E-->(R)
   |        |        |        |
   S        S        S        S
   |        |        |        |
  (R)--E-->(R)--E-->(R)--E-->(R)
   ^        ^        ^        ^
   |        |        |        |
  (C)      (C)      (C)      (C)     ExtLink controller<->router (Mesh_XY.py:92-100)
```
（每个 router 通过 ExtLink 挂若干 controller；`num_rows` 由 `--mesh-rows` 决定。）

---

## E. Simple Network（对比）

`src/mem/ruby/network/simple/` 文件：`SimpleNetwork.{py,hh,cc}`、`Switch.{hh,cc}`、`PerfectSwitch.{hh,cc}`、`Throttle.{hh,cc}`、`SimpleLink.{py,hh,cc}`、`SConscript`。

- **`PerfectSwitch`（`PerfectSwitch.cc`）职责**：路由 + 仲裁 + 转发 message（非 flit）。`wakeup()`（`277-300`）按 vnet 遍历并做优先级反转（`PRIORITY_SWITCH_LIMIT=128`，`60`）；`operateVnet`（`152-183`）在优先级组内选 oldest message 的端口先服务；`operateMessageBuffer`（`185-275`）调用 routing unit 算输出链、检查各输出 buffer slot、多目的克隆、enqueue。注意 simple 网络以 **message 为粒度**，无 flit/VC/credit。
- **`Throttle`（`Throttle.cc`）流控实现**：基于**带宽预算**（bytes/cycle），不是 credit。`wakeup()`（`247-306`）：`bw_remaining = getTotalLinkBandwidth()`（`252`，`endpoint_bandwidth × bandwidth_factor`，`getLinkBandwidth` `139-147`）；遍历 vnet，`operateVnet`（`166-245`）在预算内搬运 message，`units_remaining = network_message_to_size(msg) = MessageSizeType_to_int × MESSAGE_SIZE_MULTIPLIER(1000)`（`322-334`、`60`）；受 `out->areNSlotsAvailable`（buffer 空间）门控；带宽饱和或输出阻塞则 `scheduleEvent(1)`（`300-305`）。
- `Switch`（`Switch.cc:57-134`）组合 `PerfectSwitch`（内部仲裁）+ 每输出口一个 `Throttle`，中间用 port_buffers（`SimpleNetwork.py:130-167`）。

对比：Simple = message 粒度 + 带宽流控；Garnet = flit 粒度 + credit 流控 + VC + 真实流水线 + 拓扑路由表。

---

## F. Routing 算法（`RoutingUnit.cc`）

三模式的函数边界：
- **分派入口** `outportCompute(route, inport, inport_dirn)`（`RoutingUnit.cc:168-202`）：若 `route.dest_router == m_router->get_id()` 直接 `lookupRoutingTable` 用 Local 端口（`174-181`）；否则按 `getRoutingAlgorithm()` switch（`188-198`）：`TABLE_`(0) → `lookupRoutingTable`；`XY_`(1) → `outportComputeXY`；`CUSTOM_`(2) → `outportComputeCustom`；default → table。
- **table-based**：`lookupRoutingTable(vnet, net_dest)`（`RoutingUnit.cc:96-145`）。在所有 routing-table 条目与目的 NetDest 相交的 link 中取最小 weight（`112-119`），收集同最小 weight 的候选（`122-131`），unordered vnet 随机选、ordered vnet 固定取首个（`138-143`）。
- **XY**：`outportComputeXY`（`RoutingUnit.cc:207-261`），用 `num_rows/num_cols` 做维序（先 X 后 Y），带端口方向 assert。
- **custom**：`outportComputeCustom`（`RoutingUnit.cc:265-271`）目前是 `panic` 占位。
- 注册：`addRoute`（`56-65`，路由表）、`addWeight`（`67-71`，权重表）、`addIn/OutDirection`（`148-160`）。

**weight 语义差异**：
- **Garnet**：`BasicLink.weight` 在拓扑构造时经 `Router::addOutPort(..., link_weight, ...)`（`Router.cc:144-146`）写入 RoutingUnit 的 weight 表；运行时 `lookupRoutingTable` 用它**在候选 link 中挑最小 weight**（`RoutingUnit.cc:112-131`），从而把路由表限定为无死锁路径。Mesh_XY 用 East/West=1、North/South=2 实现 XY 维序（`Mesh_XY.py:136,155,174,193`）。
- **Simple**：weight 由 `WeightBased` routing unit（`SimpleNetwork.py:88-93`）使用，语义是"最短路径分析中限制路由"（`BasicLink.py:48` 注释）；C++ 侧 `Topology::createLinks` 用其生成最短路路由表（`Topology.cc:151-219`）。Garnet 同样复用这张最短路表，但额外在运行时用 weight 做 tie-break。

**deadlock-free 最小保证**：
1. 拓扑层：`Topology::createLinks` 用各 vnet 的最短路 + link weight 生成路由表（`Topology.cc:190-219`），`RoutingUnit.cc:90-95` 明确"Correct weight assignments are critical to provide deadlock avoidance"。
2. 权重限制转向（维序 / west-first），从源头消除通道依赖环（`Mesh_XY.py:34-37`、`Mesh_westfirst.py:34-41`）。
3. VC + credit 防止 buffer 级死锁（A.3）。

---

## G. 配置驱动

`configs/network/Network.py` 中 `--network` 实现边界（`Network.py:56-137`）：
- `--network` choices = `["simple","garnet"]`（`56-60`）；`garnet2.0` 被 `create_network` 接受后 warn 并改写为 `garnet`（`131-137`）。
- `create_network`（`128-165`）按 network 选类：
  - garnet：`GarnetNetwork` / `GarnetIntLink` / `GarnetExtLink` / `GarnetRouter` / `GarnetNetworkInterface`（`140-145`）。
  - simple：`SimpleNetwork` / `SimpleIntLink` / `SimpleExtLink` / `Switch` / `InterfaceClass=None`（`147-152`）。
- `init_network`（`168-284`）：garnet 设 `num_rows/vcs_per_vnet/ni_flit_size/routing_algorithm/garnet_deadlock_threshold` 并建 NetworkBridge（`169-266`）；simple 调 `setup_buffers()`（`268-273`）；有 InterfaceClass 则建 netifs（`275-279`）；fault model（`281-284`）。

可注入参数：
- `BasicRouter.py`：`router_id`、`latency`（garnet 流水线级数）（`36-39`）。
- `BasicLink.py`：`link_id`、`latency`、`bandwidth_factor`（simple）、`weight`、`supported_vnets`（`38-49`）；IntLink 额外 `src_outport/dst_inport`（`71-72`）。
- `GarnetNetwork.py`：`num_rows`、`ni_flit_size`(16)、`vcs_per_vnet`(4)、`buffers_per_data_vc`(4)、`buffers_per_ctrl_vc`(1)、`routing_algorithm`(0)、`enable_fault_model`、`garnet_deadlock_threshold`(50000)（`44-54`）；`GarnetRouter`：`vcs_per_vnet/virt_nets/width`（`74-86`）。
- 命令行：`--link-latency/--router-latency/--link-width-bits/--vcs-per-vnet/--routing-algorithm/--garnet-deadlock-threshold/--mesh-rows/--network-fault-model/--simple-physical-channels`（`configs/network/Network.py:43-125`）。

---

## 关键结论摘要
- 本仓 garnet 网络 = **Garnet 3.0**；无 garnet2.0/standalone 独立目录，`garnet2.0` 仅命令行别名。
- pipeline：InputUnit(BW+RC) → SA(含 VC 分配) → ST(Crossbar) → LT(Link)，`Router::wakeup` 顺序见 `Router.cc:71-97`。
- 流控：credit-based（每 VC buffer 深度 = credit 数），credit 由 InputUnit 生成、OutputUnit 消费，NI 侧同步管理。
- NI message↔flit：正向 `flitisizeMessage`（`NetworkInterface.cc:373`），反向内联于 `wakeup`（`NetworkInterface.cc:242-282`）。
- 拓扑：`configs/topologies/*.py` 的 `makeTopology` 生成 Router/ExtLink/IntLink；C++ `Topology` 生成路由表；garnet 运行时用 weight 在 RoutingUnit 选路。
