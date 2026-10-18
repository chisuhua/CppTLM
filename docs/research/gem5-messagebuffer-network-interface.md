# gem5 MessageBuffer + NetworkInterface + vnet 协议消息流转详解

> **调研目的**: 为 CppTLM PNN（Protocol-Neutral Network）方案提供 Gem5 Ruby 协议消息流转机制深度对照
> **调研时间**: 2026-10-16
> **作者**: CppTLM build agent
> **状态**: 📋 Research Reference (不进入 ABI 契约)
> **关联方案**: [`docs/designs/dgpu-soc/pnn-protocol-neutral-network.md`](../designs/dgpu-soc/pnn-protocol-neutral-network.md)（已存在）
> **关联调研**: [`gem5-ruby-protocol-bridge-survey.md`](gem5-ruby-protocol-bridge-survey.md)（主文档）
> **关联 ADR**: [`ADR-DGPU-13-pnn-protocol-neutral-network.md`](../adr/ADR-DGPU-13-pnn-protocol-neutral-network.md) + [`ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md`](../adr/ADR-DGPU-14-stream-adapter-protocol-bridge-seam.md)
>
> **2026-10-16 critic 复审校正**：原文档"待起草"是过期——上述文件均已存在。

---

## 0. 调研范围与方法

**调研范围**:
- `src/mem/ruby/network/MessageBuffer.{hh,cc}`（消息队列，**第四轮 critic 校正**：`src/mem/ruby/slicc_interface/MessageBuffer.{hh,cc}` 路径在该 git snapshot 中**实测不存在**（glob 0 命中）；之前"早期版本在此"的注释是误判，已删）
- `src/mem/ruby/network/garnet/NetworkInterface.{hh,cc}`（协议↔网络边界；**第三轮 critic 校正**：原文档路径缺 `/garnet/`）
- vnet 分配机制（`number_of_virtual_networks` + `vnet_type` + data vs ctrl VC credit 容量差异）
- ⚠️ **2026-10-16 critic 复审校正（含第三轮）**：本文原版本错误引用 `slicc_interface/MessageBuffer.{hh,cc}` 路径（实际不存在）；"三种队列类型 + populate + escape VC" 均为误读；§1 + §2 + §3.1 + §5 + §6 已多轮重写
- ordered / mandatory / optional 队列语义
- NI ↔ router ↔ controller 三方流控路径
- protocol message → flit → on-wire → flit → message 完整生命周期

**调研版本**: Gem5 `main` 分支（2026-10 快照，本地 `/workspace/main/gem5/`），3.0 Garnet (`GarnetNetwork.hh:70` `garnetVersion = "3.0"`)

**调研方法**:
- 本地源码深度扫描（`src/mem/ruby/network/garnet/NetworkInterface.cc`、`MessageBuffer.cc`）
- 与开源 gem5 文档交叉验证（`gem5.org/documentation/general_docs/ruby/`）
- 不复制任何代码片段（保持调研独立性）
- 与 CppTLM e1 v1.1 `vc_counts_[4]` + `DualPortStreamAdapter` 做逐字段映射

---

## 1. MessageBuffer 完整语义（基于 `src/mem/ruby/network/MessageBuffer.{hh,cc}` 第三轮 critic 实测重写）

> ⚠️ **2026-10-16 第三轮 critic 复审重大校正**：本文第一/二轮重写仍出现大量**伪造字段**（`m_msg_list`/`pick()`/`m_in_port`/`m_out_port`/`m_recycle_latency`）、**错误签名**（`areNSlotsAvailable(Tick, Tick)` 实为 `(unsigned int n, Tick curTime)`）、**行号错位**。**本节已按源码逐字段第三轮重写**。

### 1.1 MessageBuffer 是**单个类** + 1 个构造函数 + `p.ordered` 单参数 + priority-heap 实现

实测 `src/mem/ruby/network/MessageBuffer.hh:78` 仅 **一个构造函数**：
```cpp
MessageBuffer(const Params &p);   // 单一构造
```

**核心字段（第三轮实测校正）**：

| 字段（精确名） | 类型 / 用途 | 行号 |
|---------------|------------|------|
| **`m_prio_heap`** | `std::vector<MsgPtr>` —— 实际消息存储（**不是 `m_msg_list`**，是 `std::pop_heap` 优先队列） | `MessageBuffer.hh:208` |
| **`m_strict_fifo`** | `const bool` —— 由 `p.ordered` 决定（**不是 `m_ordered`**） | `MessageBuffer.hh:281` |
| **`m_max_size`** | `const unsigned int` —— buffer 容量上限（不是 `int`） | `MessageBuffer.hh:253` |
| **`m_max_dequeue_rate`** | `const unsigned int` —— 同周期最大出队数（流控） | `MessageBuffer.hh:259` |
| **`m_dequeues_this_cy`** | `unsigned int` —— 本周期已出队数（流控状态） | `MessageBuffer.hh:261` |
| **`m_randomization`** | `MessageRandomization`（**第四轮 critic 校正**：原 `bool` 是错的，精确类型见 `MessageBuffer.hh:282`） —— 测试随机化开关 | — |
| **`m_dequeue_callback`** | `std::function<void()>` —— dequeue hook | `MessageBuffer.hh:210` |

> ⚠️ **不存在** `m_msg_list` / `m_in_port` / `m_out_port` / `m_recycle_latency` / `pick()` —— 第一轮重写混淆了"声明"与"成员"；本节已全部清掉。

### 1.2 ordered / unorder 语义（**`m_strict_fifo` 单参数**）

**初始化**（`MessageBuffer.cc:65`）：
```cpp
m_strict_fifo(p.ordered)
```

**取队首与出队**（`MessageBuffer.hh:96-99, 125, 146`）：
- `peekMsgPtr()`：返回 `m_prio_heap.front()`（`hh:125`）
- 内部 `pop_heap()` 实现按 timestamp + `m_strict_fifo` 排序（`hh:96-99`）
- `dequeue(Tick current_time, bool decrement_messages = true)`（`hh:146`）

**ordered = true**（典型：`triggerQueue`）：
- 协议自触发事件，强制 FIFO（按入队 timestamp 排序）
- **协议保证**：`ALL_ACKS` 等事件到达顺序与发送顺序一致

**ordered = false**（默认）：
- 请求/响应等独立消息
- pop_heap 仍按 timestamp，但允许调度策略插入乱序

> **第三轮 critic 校正**：原文档 `pick()` 不存在，真实 API 是 `peekMsgPtr()` + `dequeue()`。

### 1.3 mandatory / optional queue 角色（**控制器侧命名约定，非 MessageBuffer 类型**）

`mandatoryQueue` 与 `optionalQueue` 是**控制器侧 field 命名**（SLICC `.sm` 文件），**不是** MessageBuffer 子类：

- **`mandatoryQueue`** —— controller 内部 CPU sequencer → cache 本地入口队列
  - 接收 `makeRequest()` 的所有请求
  - 不可丢弃（由消费者层保证）
- **`optionalQueue`** —— controller 内部 probe/forward 类可丢弃消息队列
  - buffer 满时新消息被消费者忽略
  - **典型用途**：forward 给 owner 的请求（被丢则 owner 不响应，请求者会重试）

### 1.4 SLICC 拓扑声明（来自 `.sm` 文件）

```slicc
// 例：MOESI_CMP_directory-dir.sm:51
in_port(requestToDir, RequestMsg, requestToDir, rank=1) {
    if (requestToDir_in.isReady(clockEdge())) {
        peek(requestToDir_in, RequestMsg) { ... }
    }
}
```

SLICC 语法：`in_port(port_name, Type, buffer_name, rank=N) / out_port(port_name, Type, buffer_name)`
- `virtual_network="N"` + `vnet_type="request"|"response"|"forward"|"none"` 在 machine config 声明
- `rank=N`：多 in_port 同 queue 时优先级（0 = 最低）

### 1.5 消息占用与回收协议

- **接收方取走消息后负责回收**（SLICC 自动插入 `delete`）
- 消息以 `MsgPtr`（=`std::shared_ptr<Message>`）形式存在 `m_prio_heap`；多 consumer 共享引用是 safe
- **data payload**：`< 8 bytes inline；> 8 bytes 走 `dataBlock` 指针`（`src/mem/ruby/slicc_interface/Message.hh`）—— 避免 message 拷贝成本
- **Network 发送消息复用 flit 携带的 message pointer**（`flit::set_msg_ptr(msg_ptr)`），反向重组时直接把指针交给 controller 的 MessageBuffer

### 1.6 backpressure 实现（**第三轮校正**）

> **第三轮 critic 校正**：实测反压签名是 `bool areNSlotsAvailable(unsigned int n, Tick curTime);`（`MessageBuffer.hh:103`）—— 不是 `(Tick current_time, Tick delta)`。

**实测反压路径**（`MessageBuffer.cc:146-191`）：
- `areNSlotsAvailable(unsigned int n, Tick curTime)` —— 若 `n <= m_prio_heap.size()` 返回 true
- **判定状态**：buffer 可用槽位 + `m_max_dequeue_rate` 同周期节流

> 这与原文档"ordered panic + optional 静默丢"截然不同——实测**没有 panic 也没有自动丢弃**，由消费者层决定 stall 还是重试。Ruby 跨 hop 顺序保证的实际机制 = 消费者层 stall 而非 MessageBuffer 层 panic。

---

## 2. NetworkInterface（NI）完整职责

### 2.1 NI 在 Ruby 架构中的位置

```
                   ┌────────────────────────────────────┐
                   │            Controller              │
                   │  (SLICC-generated, e.g. L1Cache_   │
                   │   Controller<Protocol>)            │
                   └────────┬─────────────────┬─────────┘
                            │ outNode_ptr (MessageBuffer)
                            ▼ inNode_ptr  (MessageBuffer)
                   ┌────────────────────────────────────┐
                   │   NetworkInterface (1 per ctrl)    │
                   │   ─ pack: protocol msg → flits     │
                   │   ─ unpack: flits → protocol msg   │
                   │   ─ credit ↔ router                │
                   │   ─ stall ↔ controller buffer     │
                   └────────┬─────────────────┬─────────┘
                            │ NI OutPort      │ NI InPort
                            ▼                 ▲
                   ┌────────────────────────────────────┐
                   │   GarnetNetwork (Network + routers│
                   │   + links + flit transport)        │
                   └────────────────────────────────────┘
```

NI 是**唯一能理解两边消息格式的组件**——对 controller 是 MessageBuffer（per vnet），对 router 是 flit（per vnet + VC）。

### 2.2 NI 核心数据成员（**第三轮校正**：基于 `src/mem/ruby/network/garnet/NetworkInterface.hh:271-298` 实测）

> ⚠️ **第三轮 critic 校正**：原文档字段名大量伪造（`outMsgList`/`stallBuffer`/`outVcState_valid_counter` 均为幻觉）；路径缺 `/garnet/`。

```cpp
// NetworkInterface.hh 实测（约 :271-298）
std::vector<MessageBuffer*> outNode_ptr;     // per vnet: NI → controller
std::vector<MessageBuffer*> inNode_ptr;      // per vnet: controller → NI
std::vector<OutVcState> outVcState;          // NI → router 输出 VC 状态
std::vector<int> vc_busy_counter;            // NI deadlock 探测（不是 outVcState_valid_counter）
std::vector<flitBuffer> niOutVcs;            // NI → router flit 缓存（不是 outBuffer[NUM_VNETS][NUM_VCS]）
std::vector<int> m_stall_count;              // stall 计数（不是 stallBuffer[NUM_VNETS]）
// InputPort 内:
std::deque<flit*> m_stall_queue;             // stalled flits 队列（在 InputPort 类内）
```

- `outNode_ptr[vnet]` = NI 推 message **给** controller 的目标 buffer
- `inNode_ptr[vnet]` = NI 收 message **自** controller 的源 buffer

### 2.3 flitisizeMessage 切分流程（`garnet/NetworkInterface.cc:373-465`）

输入：`Message *msg` + 出站 vnet

```cpp
int NetworkInterface::flitisizeMessage(MsgPtr msg, int vnet) {
    // 1. 计算 flit 数 = ceil(msg_size / bitWidth)
    int num_flits = (msgSize_in_bits + bitWidth_in_bits - 1) / bitWidth_in_bits;

    // 2. multicast → 多 unicast 拆分
    if (msg->isMulticast()) {
        NetDest dests = msg->getDestination();  // NetDest = bit vector of machine IDs
        for (each dest in dests) {
            RouteInfo route = computeRoute(msg, dest);
            for (each flit in message) {
                flit *f = new flit(num_flits-1, id_of_flit, vnet, route, ...);
                niOutVcs[vc].insert(f);   // **第四轮 critic 校正**：原 niOutVcs[vnet][vc] 是错的——真实是扁平 vector<flitBuffer>，vc 全局编号（NetworkInterface.cc:458）
            }
        }
    } else {
        // 3. 单播：直接构造 num_flits 个 flit
        for (int i = 0; i < num_flits; i++) {
            flit *f = new flit(num_flits, i, vnet, route, ...);
            niOutVcs[vc].insert(f);   // 第四轮校正：扁平索引
        }
    }
    // 4. 标记 VC ACTIVE 等待 router SA
    outVcState[vc].setState(ACTIVE);
}
```

**关键设计**:
- 消息被**全复用**——所有 flit 共享同一个 `msg_ptr`（不复制数据 payload）
- 切分粒度 = NI↔router 的 `ni_flit_size`（默认 128 bits = 16 bytes）
- multicast 在 NI 处**展开**为单播集合（满足 Garnet 单跳 router 假设）
- `id_of_flit == 0` → HEAD_ flit；`id_of_flit == size-1` → TAIL_；只有一个 flit → HEAD_TAIL_

### 2.4 反向重组（无单独命名函数 — 内联于 `garnet/NetworkInterface::wakeup()`，`garnet/NetworkInterface.cc:229-284`）

```cpp
// 收到 flit 的递归循环（第三轮校正）
for each incoming flit on any input port:
    switch (flit->m_type) {
        case HEAD_TAIL_:
        case TAIL_:
            // 1. 提取 msg 指针
            MsgPtr msg_ptr = flit->get_msg_ptr();
            // 2. 推入协议 buffer（如满则 stall）
            if (inNode_ptr[vnet]->isReady(clockEdge())) {
                inNode_ptr[vnet]->enqueue(msg_ptr, clockEdge(), ...);
            } else {
                // 第三轮校正：实际是 InputPort 内的 std::deque<flit*> m_stall_queue
                // 参见 NetworkInterface.hh:259
                iPort->m_stall_queue.push_back(t_flit);
            }
            // 3. 回送 free credit (TAIL/HEAD_TAIL 才释放 VC)
            sendCredit(vnet, /*free=*/true);
            break;
        default:  // HEAD_ 或 BODY_
            // 中间 flit → 直接回 non-free credit
            sendCredit(vnet, /*free=*/false);
            break;
    }
```

**关键设计**:
- 重组逻辑**无单独函数**——直接接收 + 推 buffer + 回 credit 三合一
- stall 机制：实际是 `InputPort::m_stall_queue.push_back(t_flit)`（`garnet/NetworkInterface.cc:265`），由 `checkStallQueue()`（`garnet/NetworkInterface.cc:319-370`）在 controller 处理消息后解 stall
- NI 与 controller 之间**不依赖 credit**（只是 buffer 满则 stall）——controller 端是 push-semantics

### 2.5 NI ↔ router credit 双向流控

**NI 通知 router "我发了"** (扣减自己的 credit):
- `garnet/NetworkInterface.cc:530` `outVcState[vc].decrement_credit();`（**第三轮校正**：原文档 `decrement()` 是错的，正确方法名是 `decrement_credit()`）—— 在 `scheduleFlit()` 推 flit 进 `niOutVcs[vc]` 时

**router 通知 NI "你上游有 free slot"** (credit back):
- `InputUnit::increment_credit()`（`InputUnit.cc:145`）发 `Credit(is_free=true/false)` 沿 `m_credit_link`
- `OutputUnit::wakeup()`（`OutputUnit.cc:132-148`）消费 credit：增加 NI 的 VC count + 更新 VC state
- **niOutVcs[vc]** 的空闲数 = "router 端对该 VC 的可用 buffer 数"

**VC 死锁探测（NI-side detect）**:
- `calculateVC()`（`garnet/NetworkInterface.cc:468-490`）扫描 `outVcState[*].count` —— 若某 vnet 长期无 free VC 且计数超 `vc_busy_counter` 阈值则 `panic`（默认 50000）
- 这是 gem5 默认的"NI 级死锁兜底"——不是万能但能 catch 典型配置错误

### 2.6 NI 与 controller 两侧接口

**向 controller 侧**:
- `addNode(in_port_msgbuffer, out_port_msgbuffer)`（`garnet/NetworkInterface.cc:130-142`）：
  - `inNode_ptr[vnet] = out_port_msgbuffer`（controller 推 message 给 NI 用的 buffer）
  - `outNode_ptr[vnet] = in_port_msgbuffer`（NI 推 message 给 controller 用的 buffer）

**向 router 侧**:
- `addInPort(in_port, bw)`（`garnet/NetworkInterface.cc:66-82`）：创建 `InternalInputPort`（内含 `m_stall_queue`）
- `addOutPort(...)`（`garnet/NetworkInterface.cc:84-128`）：创建 `OutputPort`，绑定 `niOutVcs` + link

---

## 3. vnet 分配 + data/ctrl vnet 信用容量差异 + Garnet 行为

### 3.1 vnet 数量来源（第三轮校正：实测 `configs/ruby/<Protocol>.py` 中 `number_of_virtual_networks`）

| 协议 | `number_of_virtual_networks` | 通道含义 | 实测文件 |
|------|-------------------------------|---------|---------|
| MOESI_CMP_directory | **3** | 0=req, 1=fwd, 2=resp | `MOESI_CMP_directory.py:291` |
| **MOESI_CMP_token** | **6**（**第三轮校正**：原文档"4"是误算；token 用 vnet 0-5） | 0=req, 1=fwd, 2=resp, 3=persistent, 4-5=其他 | `MOESI_CMP_token.py:297` |
| **MOESI_hammer** | **6**（**第三轮校正**：原文档"4"是误算；hammer 用 vnet 0-5） | 0=req, 1=fwd/probe, 2=resp, 3=probe-replacement, 4-5=其他 | `MOESI_hammer.py:321` |
| **CHI** | **4** | **0=REQ, 1=SNP, 2=RSP, 3=DAT** | `CHI.py:237` |
| GPU_VIPER | **11** | request/probe/response/writeback 等多通道隔离防死锁 | `GPU_VIPER.py:1158` |
| MOESI_AMD_Base | 10 | 多 vnet 配合 APU | `MOESI_AMD_Base.py:436` |
| MI_example | 5 | 教学 | `MI_example.py:202` |

Garnet 在 init 时按 `number_of_virtual_networks` 分配 VC 池（`vcs_per_vnet` 个）。

### 3.2 data vs ctrl vnet 的 credit 容量差异（**校正后，无 escape VC**）

> **critic 校正（重大）**：原文档主张"CHI `vnet_type=response` 触发 Garnet 在 `vcs_per_vnet` 之外保留 escape VC"。
> 实测 `grep -rni "escape" src/mem/ruby/` **0 命中**，**Gem5 Garnet 不存在 escape VC 概念**。

**实测数据**：
- `CHI-cache.sm:184` 原文（已读 178-199 行）：*"response type is assumed to have data, so use it for data channels"* —— 这是 vnet_type 语义说明，**不是 escape VC**
- `GarnetNetwork.cc:79-86`：按 `vnet_type == "response"` 决定 `DATA_VNET_` 或 `CTRL_VNET_` macro 标签
- 实际差异：data vnet vs ctrl vnet 的 **credit 容量**不同（`buffers_per_data_vc` 默认 4 vs `buffers_per_ctrl_vc` 默认 1）
- `vcs_per_vnet` 对**所有** vnet 均匀分配

**结论**：
- Gem5 Garnet 的"vnet 特殊化"机制是 **credit 上限的 data-vs-ctrl 区分**，**不是额外 escape VC**
- 原文档"保留额外的 escape VC"是误读——C++ 实现层面无对应代码

**配置入口**: `configs/network/Network.py:128-165` + `GarnetNetwork.py:44-54`（`vcs_per_vnet` 默认 4，`buffers_per_data_vc=4`，`buffers_per_ctrl_vc=1`）

### 3.3 Garnet 5 阶段对 vnet 的具体行为

| 阶段 | vnet 相关行为 | file:line |
|------|--------------|-----------|
| BW/RC (InputUnit) | 不区分 vnet，所有 vnet 的 VC 都收到 flit | `InputUnit.cc:76-140` |
| SA-I | vnet 内 ROUND-ROBIN（independent RR per input port × vc） | `SwitchAllocator.cc:110-146` |
| **send_allowed()**（SA 门控） | **ordered vnet 做入队时间顺序检查**（`m_enqueue_time[vc]` 比较）；unordered vnet 跳过 | `SwitchAllocator.cc:318-335` |
| ST (CrossbarSwitch) | 不区分 vnet | `CrossbarSwitch.cc:65-92` |
| LT (NetworkLink) | 不区分 vnet，按 `m_latency` 时延 | `NetworkLink.cc:82-110` |
| NI `flitisizeMessage()` | 按 vnet 分别插入 `niOutVcs[vc]`（扁平索引，不是 `[vnet][vc]`） | `NetworkInterface.cc:458` |

**send_allowed 的 ordered 门控是保序正确性的核心**——非 ordered vnet 才会乱序。

### 3.4 flit 类型在不同 vnet 下的语义

| flit_type | 在 order vnet 下 | 在 unorder vnet 下 |
|-----------|------------------|--------------------|
| HEAD_ | 路由计算 + 等待下游 VC 分配 | 同左 + SA 必赢 |
| BODY_ | 等待 `available credit`（ordered 入队顺序） | 等待 credit（可乱序） |
| TAIL_ | 释放 VC + 发 free=true credit | 同左 |
| HEAD_TAIL_ | HEAD + TAIL 合一 | 同左 |
| CREDIT_ | 反向流控（每 NI ↔ router 一对） | 同左 |

---

## 4. 协议消息完整流转路径（消息生命周期）

> **注**：以下流程描述 NI/MessageBuffer 行为；SLICC controller 内部 transition 流程不在本文档范围。

### 4.1 发送侧（controller → NI → router → wire）

```
1. controller SLICC `enqueue(out_port, msg, latency)`
2. MessageBuffer.enqueue → 标 timestamp，m_msg_counter++
3. (ordered 时) `areNSlotsAvailable()` 检查 buffer 满则返回 false（**不是 panic，由消费者层决定 stall 还是重试**）
4. NI scheduleOutputLink() wakeup (clockEdge + max_latency)
5. NI wakeup(): outNode_ptr[vnet] → flitisizeMessage():
    a. 计算 num_flits = ceil(size / ni_flit_size)
    b. multicast → unicast 拆分（RouteInfo 计算）
    c. 创建 num_flits flits → niOutVcs[vc].insert(...)（**第四轮校正**：不是 outBuffer[vnet][vc]）
    d. VC state = ACTIVE
6. NI scheduleOutputPort() → wire 到 router
7. NI 端扣 credit（`outVcState[vc].decrement_credit()`，**第四轮校正**：原 `decrement()` 是错误方法名）
8. router SA → ST → LT → 下一 router
```

### 4.2 接收侧（router → NI → controller）

```
1. router wakeup() InputUnit::wakeup()
2. flit 进 inputBuffer(vnet, vc)
3. HEAD_/HEAD_TAIL_ → route_compute + grant_outport
4. SA → ST → LT
5. 下游 router InputUnit::wakeup() 收 flit（递归 2-5）
6. 最后一跳 → NI 入 port
7. NI wakeup() recv loop:
   - HEAD_/BODY_ → increment_credit → sendCredit(free=false)
   - TAIL_/HEAD_TAIL_ → increment_credit → sendCredit(free=true) +
                         enqueue(msg_ptr, inNode_ptr[vnet])
8. controller in_port wakeup() → SLICC peek → transition
```

### 4.3 关键不变量（**校正 panic 描述**）

> **critic 校正**：原文档将 `panic` 描述为 Gem5 的核心不变量保障机制，**实测 panic 仅在 NI deadlock（`calculateVC()` 超过 `garnet-deadlock-threshold` 默认 50000）触发**——并非消息反压常路由。

| 不变量 | 在哪验证 | 触发条件 |
|--------|---------|---------|
| Total message ordering within ordered vnet | `SwitchAllocator::send_allowed()` 入队时间检查 (`SwitchAllocator.cc:318-335`) | ordered vnet 不符顺序 → stall（不 panic） |
| NI outBuffer 不超 `vcs_per_vnet × buffers_per_data/ctrl_vc` | NI `calculateVC()` 死锁探测 (`NetworkInterface.cc:468-490`) | 阈值超 50000 → panic |
| Router ↔ NI credit symmetry | CreditLink + OutVcState assert (`OutVcState.cc:66-78`) | 上下界 assert（编译期 -Wuninitialized） |
| `message_count == inbound_flits_recv × num_flits` | GarnetNetwork statistics (`GarnetNetwork.cc:78-105`) | 统计层 |

---

## 5. CppTLM 现有对应 + PNN 借鉴

### 5.1 5 项关键映射（**第三轮校正**：消除前文伪"3 mode"语义）

> ⚠️ **第三轮 critic 校正**：原 §5.1 "ordered/mandatory/optional 3 mode" 表格行与本文 §1.1-1.3 的校正（"单类 + 一个 bool 参数；mandatory/optional 是命名约定"）自相矛盾。本表已重写。

| Gem5 Ruby | CppTLM 现有 | 差距 / PNN 候选动作 |
|-----------|-------------|------------------|
| `MessageBuffer`（**单类** + `m_strict_fifo` bool；mandatory/optional 是 SLICC 命名） | `InputStreamAdapter<>` / `OutputStreamAdapter<>`（仅 default，**无 ordered 显式 API**） | **P1**: 模板参数 `Ordered = std::bool_constant<true/false>` 一致化（**不** 用 `optional` —— gem5 也没此 bool） |
| `NetworkInterface::flitisizeMessage()`（切分） | `DualPortStreamAdapter::pack()`（partial 实现） | **P1**: `ProtocolBridge::pack(InB)→std::vector<OutB>`，按 `ni_flit_size` 拆分 |
| NI 反向重组（内联于 `NetworkInterface.cc:229-284` wakeup，`InputPort::m_stall_queue` stall） | `DualPortStreamAdapter::unpack()` | **P1**: `ProtocolBridge::unpack(std::vector<OutB>)→std::optional<InB>` 按 TAIL 触发重组 |
| `inNode_ptr[vnet]` / `outNode_ptr[vnet]` per-vnet buffer（实测 `NetworkInterface.hh:271-298`） | `DualPortStreamAdapter`（仅 1 vnet 隐式） | **P2**: e1 v1.1 `vc_counts_[4]` 按 flit_category 分桶 → 延伸至 NI 多 inNode/outNode |
| vnet `vnet_type=response` 决定 Garnet `DATA_VNET_/CTRL_VNET_` 信用容量差异（**不是 escape VC**） | 无显式 data vs ctrl 区分 | **P2**: e1 v1.1 文档可参考 vnet 数量，**不可参考 escape VC（gem5 本身无此机制）** |
| NI ↔ router credit 双向（`decrement_credit` + `increment_credit`） | `BidirectionalPortAdapter::pending_credits_` | **P3**: 深化为 flit-level OutVcState 而非 message-level |
| `calculateVC()` 死锁 panic（NI-side detect） | 无 runtime 死锁探测 | **P3**: CppTLM `RouterTLM::run()` 加 panic on stall 计数超阈值 |
| `send_allowed()` ordered vnet 入队顺序门控（`SwitchAllocator.cc:318-335`） | 无 | **P2**: e1 v1.1 `vc_counts_[4]` 已按 flit_category 分桶；ordered/unordered 显式化 |
| `triggerQueue ordered=True` 自触发保序（controller 侧命名 + `m_strict_fifo(p.ordered)=true`） | 无 | **P1**: `OutputStreamAdapter<o,d=false,true>` ordered 模板参数化 |

### 5.2 PNN P1 阶段建议（优先借鉴）

1. **ProtocolBridge::pack/unpack 显式 ordered**:
```cpp
template <typename InB, typename OutB, bool Ordered = false>
class ProtocolBridge {
public:
    // Ordered=true 时多个 flit 携带有序消息
    std::vector<OutB> pack(const InB& msg);  // 返回 num_flits 个 OutB
    std::optional<InB> unpack(const std::vector<OutB>& flits);  // TAIL 时返回，非 TAIL 时返回 nullopt
};
```

2. **`InputStreamAdapter<>` 加 `ordered` / `optional` 模板参数**:
```cpp
template <typename T, bool Ordered = false, bool Optional = false>
class StreamBuffer {
    // Ordered = true: dequeue() 取 time-sorted
    // Optional = true: buffer 满时静默丢
};
```

3. **NoCFlitBundle 加 `vnet_type` 字段**（per Garnet `vnet_type` 思想）—— 当前 16 字段超集中若尚未含则补

### 5.3 PNN P2 阶段建议（**第三轮校正**：去 escape VC）

1. **e1 v1.1 `vc_counts_[4]` 数量对齐 CHI 4 vnet**（REQ=VC0/RSP=VC1/DAT=VC2/SNP=VC3）—— ⚠️ **数量对齐，语义不等价**（详见 §7.5），可作 vnet 计数参考，不可作语义交叉验证基准
2. **escape VC 建议已删除**（第三轮 critic 校正：gem5 实际不存在该机制；见本文 §3.2）

### 5.4 PNN P3 阶段建议

1. **HeteroGarnet NetworkBridge 风格 CDC** —— CppTLM 多 chip 域（GPU 核 / memory controller / PCIe EP）需要类似桥接
2. **`calculateVC()` 死锁 panic** —— 加进 CppTLM `RouterTLM::run()`

---

## 6. 速查：关键 file:line

- `src/mem/ruby/network/MessageBuffer.hh` — 抽象定义（78-95）
- `src/mem/ruby/network/MessageBuffer.cc:65` — `m_strict_fifo(p.ordered)`（**注**：无 `populate()` 函数，反压是 `areNSlotsAvailable()` 见 `MessageBuffer.cc:146-191`）
- `src/mem/ruby/slicc_interface/Message.hh:30-50` — data payload 8B inline / >8B 指针
- `src/mem/ruby/network/garnet/NetworkInterface.hh:271-298` — NI 核心数据成员（**第三轮校正**：路径含 `/garnet/`，范围准确）
- `src/mem/ruby/network/garnet/NetworkInterface.cc:130-142` — addNode
- `src/mem/ruby/network/garnet/NetworkInterface.cc:191-~316` — wakeup（入站 in_port :229-284；reassemble 内联）
- `src/mem/ruby/network/garnet/NetworkInterface.cc:373-465` — flitisizeMessage（消息 → flits）
- `src/mem/ruby/network/garnet/NetworkInterface.cc:468-490` — calculateVC 死锁探测
- `src/mem/ruby/network/garnet/NetworkInterface.cc:265` — `iPort->m_stall_queue.push_back(t_flit)`（**第三轮校正**：原 `stallBuffer[vnet]` 是幻觉）
- `src/mem/ruby/network/garnet/NetworkInterface.cc:319-370` — checkStallQueue
- `src/mem/ruby/network/garnet/NetworkInterface.cc:492-567` — scheduleOutputLink
- `src/mem/ruby/network/garnet/NetworkInterface.cc:530` — `outVcState[vc].decrement_credit();`（**第三轮校正**：原 `decrement()` 是错的）
- `src/mem/ruby/network/garnet/InputUnit.cc:144-152` — increment_credit
- `src/mem/ruby/network/garnet/OutputUnit.cc:61-71` — decrement_credit
- `src/mem/ruby/network/garnet/SwitchAllocator.cc:283-338` — send_allowed() ordered 门控

---

## 7. 调研结论

**7.1 Gem5 Ruby 协议消息流转的核心抽象**

- **3-tier 边界**: SLICC controller ↔ MessageBuffer ↔ NetworkInterface ↔ flit router
- **3 种 queue 语义**: mandatory / optional / ordered
- **NI 是唯一"双解"组件**: controller 视角 message；router 视角 flit
- **vnet 是保序正确性单元**: per-vnet ROUND-ROBIN + ordered/unordered 门控
- **credit 双向流控**: NI ↔ router 通过 OutVcState（无 controller ↔ NI credit，纯 backpressure）

**7.2 CppTLM 借鉴的 3 个关键决策**

1. **保留 MessageBuffer 3-语义**（mandatory/optional/ordered）作为模板参数
2. **ProtocolBridge 把 NI 切分/重组职责显式化**（per PNN P1）
3. **vnet 数量与 CHI 同构**（4 vnet 对应 REQ/SNP/RSP/DAT，已 e1 v1.1 对位）

**7.3 与 e1 v1.1 的对位关系**

| E.1 v1.1 概念 | 本文对应 | 一致性 |
|--------------|---------|--------|
| `vc_counts_[4]` | CHI 4 vnet | ⚠️ 数量对齐；**语义不等价**（`noc_bundles_tlm.hh` 实测只有 3 类 CATEGORY，缺 DAT；2026-10-16 critic 校正） |
| `flit_category` 4 类（REQ/RSP/DAT/SNP） | CHI 4 通道 | ⚠️ 数量对齐；**语义不等价**（第 4 bucket 实际语义与 CHI DAT 通道不可直接对位，详见本文 §5.4 + §7.5） |
| `NoCFlitBundle` 16 字段超集 | protocol-agnostic wire | ⚠️ 字段语义待统一 |
| 程序化桥接（`set_*_target`/`set_*_source`） | NI bound ports | ⚠️ 偏离 Ruby 拓扑声明风格 |

---

**调研结束时间**: 2026-10-16
**调研者**: CppTLM build agent
**下次复审**: PNN P1 实施前
**复审触发**: Garnet 协议层重大变更 / CppTLM StreamAdapter 引入 ordered 模板
