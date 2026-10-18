# gem5 Ruby 一致性协议调研：Directory / Token / Hammer / GPU / CHI / CMP

> 调研对象：/workspace/main/gem5/（本地 develop 分支快照，无网络）
> 调研方式：只读源码扫描，所有结论均附 file:line 证据
> 日期：2026-10-16

---

## 0. 协议总览（src/mem/ruby/protocol/ 全量清单）

该 gem5 版本采用**扁平协议目录**（非按协议分子目录，CHI 除外）：

| 协议 | .slicc 主文件 | 组成 .sm 文件 | 类别 |
|------|--------------|---------------|------|
| MI_example | MI_example.slicc | msg/dir/cache/dma | 教学最小协议 |
| MESI_Two_Level | MESI_Two_Level.slicc | msg/dir/L1cache/L2cache/dma | directory, 2 级 |
| MESI_Three_Level | MESI_Three_Level.slicc | msg/L0cache/L1cache | directory, 3 级（复用 MESI_Two_Level 的 L2/dir/dma） |
| MESI_Three_Level_HTM | MESI_Three_Level_HTM.slicc | L0cache (HTM) + 复用 MESI_* | directory + 硬件事务内存 |
| MOESI_CMP_directory | MOESI_CMP_directory.slicc | msg/dir/L1cache/L2cache/dma | **directory, MOESI, CMP 模板** |
| MOESI_CMP_token | MOESI_CMP_token.slicc | msg/dir/L1cache/L2cache/dma | **token coherence** |
| MOESI_hammer | MOESI_hammer.slicc | msg/dir/cache/dma | **AMD Hammer (probe filter)** |
| MOESI_AMD_Base | MOESI_AMD_Base.slicc | msg/dir/dma/L3cache/probeFilter/Region*/CorePair | AMD APU 基座（含 GPU CorePair） |
| GPU_VIPER | GPU_VIPER.slicc | msg/TCP/TCC/SQC + 复用 MOESI_AMD_Base 的 dir/dma/CorePair/L3 | **GPU 专用（AMD dGPU/APU）** |
| Garnet_standalone | Garnet_standalone.slicc | msg/dir/cache | 网络 standalone 测试 |
| CHI | chi/CHI.slicc | **12 个 .sm**（**第三轮 critic 校正**：原"13 .sm"是误算，glob 实测 12） | **ARM CHI** |

注意：本快照中**不存在 MESI_CMP_directory** 协议（历史 gem5 曾有，现已删除，仅 MOESI 版存活）。§1.4 详述。

公共基础设施（协议无关）：
- RubySlicc_interfaces.slicc / RubySlicc_Types.sm / RubySlicc_Exports.sm / RubySlicc_MemControl.sm / RubySlicc_ComponentMapping.sm / RubySlicc_Defines.sm / RubySlicc_Util.sm
  —— 全部位于 src/mem/ruby/protocol/，被所有 .slicc 隐式包含。

---

## 1. Directory-based coherence：MOESI_CMP_directory

### 1.1 文件清单（6 个文件）

| 文件 | 功能 |
|------|------|
| src/mem/ruby/protocol/MOESI_CMP_directory.slicc:1-6 | 协议入口，include 其余 5 个 .sm |
| .../MOESI_CMP_directory-msg.sm | RequestMsg/ResponseMsg 结构定义（含 RequestorMachine/SenderMachine 字段，:102,:130） |
| .../MOESI_CMP_directory-L1cache.sm | L1 (MESI 私有) 状态机，vnet0=request, vnet2=response (:53-64) |
| .../MOESI_CMP_directory-L2cache.sm | L2 (共享 bank, MOESI) 状态机 |
| .../MOESI_CMP_directory-dir.sm | **Directory controller 状态机（核心，1120 行）** |
| .../MOESI_CMP_directory-dma.sm | DMA controller |

### 1.2 Directory controller 状态机核心

`machine(MachineType:Directory, "Directory protocol")` 声明于 MOESI_CMP_directory-dir.sm:41。
MessageBuffer 拓扑（dir.sm:47-59）：
- `requestToDir` (From, vnet 1, request) / `responseToDir` (From, vnet 2, response)
- `forwardFromDir` (To, vnet 1, forward) / `responseFromDir` (To, vnet 2, response)
- `requestToMemory` / `responseFromMemory` —— **点对点直连内存控制器，不走 NoC**（dir.sm:57-58）
- `triggerQueue` —— 内部自触发（ALL_ACKS 完成检测，dir.sm:302-312）

**Directory Entry 结构**（dir.sm:123-128）：
```
State DirectoryState   // I/S/O/M + 14 种瞬态
NetDest Sharers        // sharers bit-vector
NetDest Owner          // owner（单 bit 的 NetDest）
int WaitingUnblocks    // 等待的 unblock 计数
```

**稳定态**（`MOESI_CMP_directory-dir.sm:63-68`）：`I` / `S` / `O (Maybe_Stale)` / `M (Maybe_Stale)`。
**瞬态**：IS_M/IS/SS/OO/MO/MM_M/MM/MI/MIS/OS/OSS/WBI/WBS/XI_M/XI_M_U/XI_U/OI_D/OD/MD（**19 种，MOESI_CMP_directory-dir.sm:72-94**，**校正**原文档"14 种"为误读）。

### 1.3 Owner / Sharers 维护

| 操作 | 位置 | 语义 |
|------|------|------|
| c_clearOwner | dir.sm:447-449 | 清 owner |
| c_moveOwnerToSharer | dir.sm:451-454 | O→S 降级时 owner 并入 sharers |
| cc_clearSharers | dir.sm:456-458 | 清空 sharers |
| e_ownerIsUnblocker | dir.sm:500-505 | unblock 到达时把 sender 设为新 owner |
| m_addUnlockerToSharers | dir.sm:583-589 | unblock 到达时把 sender 加入 sharers（仅 L2Cache 类型） |
| 不变量断言 | dir.sm:197-209 | S 态 owner.count()==0；O 态 owner.count()==1 且 sharers⊅owner；M 态 owner==1 且 sharers==0 |
| dealloc 断言 | dir.sm:171-173 | 释放目录项前 owner 与 sharers 必须都已清空 |

### 1.4 Forwarded GETX / GETS 行为

- **GETX on O/M**：`transition(O, GETX, MM)` / `transition(M, GETX, MM)`（dir.sm:862, :896）→
  `f_forwardRequest`（dir.sm:507-522）：把原始请求**原样转发给 owner**（`out_msg.Type := in_msg.Type`，
  `Destination.addNetDest(Owner)`），并附上 `Acks := Sharers.count()`（若 requestor 本身是 sharer 则减 1，dir.sm:515-518）。
  同时 `g_sendInvalidations`（dir.sm:541-558）向除 requestor 外的全部 sharers 广播 INV。
- **GETS on O/M**：`transition(M, GETS, MO)`（dir.sm:911-915）/ `transition({O,OO}, GETS, OO)`（dir.sm:890-894）→
  forward 给 owner，owner 提供数据，目录进入 OO/MO 等 unblock；unblock 到达后
  `m_addUnlockerToSharers` 把新 sharer 记账（dir.sm:958-962, :998-1008）。
- **GETS on I/S**：直接取内存（IS_M/SS，dir.sm:832-850），`Memory_Data_Cache` 到达后 `d_sendDataMsg`（dir.sm:460-485，
  含 Acks 计算逻辑 dir.sm:472-476），不 forward。
- **目录自己作为 requestor 的 DMA 场景**：`f_forwardRequestDirIsRequestor`（dir.sm:524-539），Requestor 改写为目录自身。

### 1.5 Memory writeback 协议

- 请求侧：`qw_queueMemoryWBFromCacheRequest`（dir.sm:617-631，cache 整行 WB）/
  `qw_queueMemoryWBFromCacheResp`（dir.sm:633-651，部分 WB，用 `DataBlk.copyPartial` 合并 TBE 旧数据）/
  `qw_queueMemoryWBFromDMARequest`（dir.sm:673-687，DMA 写）。
  三者均发 `MemoryRequestType:MEMORY_WB` 到 `requestToMemory` 并置 `tbe.WaitingWBAck := true`。
- 完成侧：`Memory_Ack` 事件（dir.sm:389-390）→ `clearWBAck`（dir.sm:441-445）+
  `checkForCompletion`（dir.sm:572-581：WaitingWBAck 与 WaitingDMAAck 全清后经 triggerQueue 自触发 ALL_ACKS）→
  `transition(XI_U, All_Acks, I)`（dir.sm:804-808）释放 TBE 与目录项。
- 终态路径：`transition(WBI, Memory_Ack, I)`（dir.sm:1042-1047）、`transition(WBS, Memory_Ack, S)`（dir.sm:1075-1079）。

### 1.6 MOESI_CMP_directory 目录状态转换表（节选自 dir.sm:748-1118）

| 当前态 | 事件 | 下一态 | 关键动作 |
|--------|------|--------|----------|
| I | GETX | MM_M | allocDirEntry + 取内存 (dir.sm:748) |
| I | GETS | IS_M | allocDirEntry + 取内存 (dir.sm:832) |
| S | GETX | MM_M | 取内存 + g_sendInvalidations (dir.sm:810) |
| S | GETS/DMA_READ | SS | 取内存 + n_incrementOutstanding (dir.sm:839) |
| O | GETX | MM | f_forwardRequest + INV sharers (dir.sm:862) |
| O/OO | GETS | OO | f_forwardRequest + 计 outstanding (dir.sm:890) |
| M | GETX | MM | f_forwardRequest (dir.sm:896) |
| M | GETS | MO | f_forwardRequest + TBE (dir.sm:911) |
| M | PUTX/PUTO | MI | WB ack (dir.sm:917, :924) |
| O | PUTO | OS | WB ack (dir.sm:936) |
| MM/MO | Exclusive_Unblock | M | e_ownerIsUnblocker (dir.sm:951) |
| MO | Unblock | O | m_addUnlockerToSharers (dir.sm:958) |
| MI | Dirty_Writeback | WBI | 清 owner/sharers + 写内存 (dir.sm:1010) |
| WBI | Memory_Ack | I | 释放全部资源 (dir.sm:1042) |
| 任意瞬态 | 冲突请求 | — | zz_recycleRequest 回收重试 (dir.sm:947-949, :964-966) |

### 1.7 与 MESI_CMP_directory 的差异

**本代码库不存在 MESI_CMP_directory**（§0 已列全量协议）。历史上（gem5 ≈2015 前）两者并存，
MOESI 版多出 O 态 + PUTO/PUTO_SHARERS 事件 + owner forwarding；当前代码库中
“MESI directory” 的最近等价物是 **MESI_Two_Level**（dir 在 MESI_Two_Level-dir.sm），
其目录侧无 Owner 字段，仅靠 Sharers 位图，GETX 时对全部 sharer 发 INV 而非 forward 给 owner。

---

## 2. Token Coherence：MOESI_CMP_token（学术创新）

### 2.1 文件清单（6 个文件）

| 文件 | 功能 |
|------|------|
| src/mem/ruby/protocol/MOESI_CMP_token.slicc:1-6 | 协议入口 |
| .../MOESI_CMP_token-msg.sm | RequestMsg/ResponseMsg（**ResponseMsg 含 `int Tokens` 字段**，:109）+ **PersistentMsg**（:60-78，独立消息类型） |
| .../MOESI_CMP_token-L1cache.sm | L1 token cache（含 PersistentTable 接口，:175-177, :203） |
| .../MOESI_CMP_token-L2cache.sm | L2 token cache |
| .../MOESI_CMP_token-dir.sm | **Directory/token-home 状态机（核心）** |
| .../MOESI_CMP_token-dma.sm | DMA controller |

### 2.2 Token 概念（代码语义）

- **每行固定总票数**：`max_tokens() = 1024`，硬编码于 src/mem/ruby/slicc_interface/RubySlicc_Util.hh:108-111。
- **目录项持票**：`structure(Entry)` 含 `int Tokens, default="max_tokens()"`（token-dir.sm:126）
  —— 即冷启动时全部 1024 票"停在"目录（stationary tokens 思想的 gem5 实现）。
- **正确性不变量（票守恒 + 写独占）**，全部以断言形式落码：
  - 写（M 态）须持全部票：`assert(cache_entry.Tokens == max_tokens())`（token-L1cache.sm:347）
  - 读（S 态）须 ≥1 票且 < 半数：<br>`assert(cache_entry.Tokens >= 1); assert(cache_entry.Tokens < (max_tokens()/2))`（token-L1cache.sm:320-321）
  - 票不能恰好等于半数（消除歧义）：`assert(cache_entry.Tokens != (max_tokens()/2))`（token-L1cache.sm:304）
  - 目录侧范围检查：`Tokens ∈ [0, max_tokens()]`（token-dir.sm:217-218）
- **owner 票**：目录 O 态 = "Owner, memory has valid data, but not necessarily all the tokens"（token-dir.sm:70）
  —— owner 持 1 张特殊 owner 票 + 数据责任，其余票可散布各处。
- **non-coherent copy**：数据转移与票转移解耦——`ResponseMsg` 同时携带 `DataBlk` 与 `Tokens`（token-msg.sm:104-112），
  协议允许"给数据不给票"（Data_Owner vs Data_All_Tokens 事件区分，token-dir.sm:103-108），
  无票副本不可用于一致性读写，仅作预测/预取用（token coherence 论文的 decoupling 思想）。
- **目录的 Sharers/Owner 是 soft state**：token-dir.sm:128-134 注释明确"all of this state is 'soft state'
  that does not need to be correct (as long as you're eventually willing to resort to broadcast)"
  —— 这是 token 协议与目录协议的**本质架构差异**：正确性靠票守恒，不靠目录准确性。

### 2.3 饥饿自由：Persistent Request 机制

- 专用 **vnet 3 = persistent**（token-L1cache.sm:58-70：persistentFromL1Cache / persistentToL1Cache）。
- `PersistentTable`（external C++ 结构，token-L1cache.sm:175-177）：`persistentRequestLock/Unlock/isLocked/findSmallest/typeOfSmallest/okToIssueStarving`。
- 请求超时后广播 Persistent_GETX/GETS（PersistentMsg，token-msg.sm:60-78）；
  收到者把请求者记入 lock 表，按 machineID 最小者优先（token-L1cache.sm:279-280, :466-468, :504-507）。
- 目录侧同样响应 persistent（token-dir.sm:366-386, :463-477 px_tryIssuingPersistentGETXRequest / :531-537 ps_tryIssuingPersistentGETSRequest）。

### 2.4 Token 计数在 SLICC 中的位置

- **定义**：`int Tokens` 字段 —— L1 端 token-L1cache.sm:146，目录端 token-dir.sm:126。
- **转移**：消息字段 `ResponseMsg.Tokens`（token-msg.sm:109）。
- **关键函数/动作**：
  - `getTokens(Entry)`（token-L1cache.sm:265-267）
  - `a_sendTokens`（token-dir.sm:446-461：把自己全部票发给 requestor 后清零）
  - `aa_sendTokensToStarver`（token-dir.sm:595-608）、`d_sendMemoryDataWithAllTokens`（token-dir.sm:610-625）
  - 合票判定：`if (getDirectoryEntry(in_msg.addr).Tokens + in_msg.Tokens == max_tokens())` →
    触发 Data_All_Tokens / Ack_All_Tokens（token-dir.sm:312-321）
  - DMA 全票判定：token-dir.sm:430-431（`Tokens == max_tokens()` → DMA_WRITE_All_Tokens）
- **广播取数/收票**：`br_broadcastRead`（token-dir.sm:572）、`bw_broadcastWrite`（token-dir.sm:504-521，
  断言 `Tokens != max_tokens()` 才广播）。

### 2.5 Token coherence 持票转移表（依据 §2.2-2.4 断言与动作归纳）

| 场景 | 发起方票数变化 | 目录票数变化 | 其他持有者 | 证据 |
|------|----------------|--------------|------------|------|
| 冷行 GETS | 0 → ≥1 | N → N−k（发出 k 票+数据） | 不变 | token-dir.sm:610-625 |
| 冷行 GETX | 0 → 1024（全票） | N → 0 | 全部上交（广播收票） | token-dir.sm:504-521, :312 |
| S→M 升级 | ≥1 → 1024 | 视持票而定 | 其余票全部回收 | token-L1cache.sm:347 断言 |
| M 写回 | 1024 → 0 | 0 → 1024 | — | token-dir.sm:792 (`Tokens == max_tokens()`) |
| Persistent 饥饿请求 | 不变（锁表记账） | 收到后定向放票给 starver | 持票者被 aa_sendTokensToStarver 抽票 | token-dir.sm:595-608 |
| DMA 写（目录已全票） | — | 1024 → 0（随写完成释放） | 须先收全票 | token-dir.sm:430-431 |

### 2.6 为什么 token coherence 适合（或不适合）GPU —— 评估

代码证据支持的两面：

**理论上适合 GPU 的点**（学术动机，Martin/Hill/Wood Token Coherence 2003 的 gem5 实现）：
1. **无目录精确性负担**：Sharers/Owner 是 soft state（token-dir.sm:128-134），GPU 上百 CU 下目录位图
   要么巨大要么受限（limited directory 的 pointer overflow 问题不存在——票就是计数器）。
2. **正确性与性能解耦**：broadcast 兜底 + persistent 防饿死，适合 GPU 不规则共享模式。
3. **non-coherent copy 天然支持**：GPU 大量只读 streaming 数据可以无票拷贝（见 §2.2）。

**gem5 实际没有选择它做 GPU 的证据**：
1. gem5 的 GPU 协议是 **GPU_VIPER**（§4），而非 MOESI_CMP_token。
2. token 协议核心操作是**广播**（br_broadcastRead / bw_broadcastWrite，token-dir.sm:504, :572）——
   在 32-128 CU 的 GPU 规模下广播风暴不可接受；token 协议在 gem5 中定位为中小规模 CMP 研究载体。
3. persistent 机制占用**额外一整个 vnet**（vnet 3，token-L1cache.sm:58-70），NoC 资源开销大。

**结论**：token coherence 对 GPU 的适配性是"学术上优雅、工程上被 VIPER 取代"——
它解决了目录可扩展性，但引入了广播可扩展性。对 CppTLM 的启示：若建模 GPU 一致性，
票计数思想可用于"无需精确 sharer 列表"的目录设计，但传输层必须避免广播。

---

## 3. MOESI_hammer（AMD Hammer/Opteron 风格）

### 3.1 文件清单（5 个文件）

| 文件 | 功能 |
|------|------|
| MOESI_hammer.slicc:1-5 | 协议入口 |
| .../MOESI_hammer-msg.sm | 消息 |
| .../MOESI_hammer-cache.sm | 单级 cache 状态机（无 L1/L2 分层，HyperTransport 风格全互联） |
| .../MOESI_hammer-dir.sm | **"目录"（实为 memory controller + probe filter）** |
| .../MOESI_hammer-dma.sm | DMA |

### 3.2 设计重点

`machine(MachineType:Directory, "AMD Hammer-like protocol")`（hammer-dir.sm:33），
带 `CacheMemory * probeFilter`（hammer-dir.sm:35）与开关 `probe_filter_enabled`（:38）。

**目录态**（hammer-dir.sm:70-76）：
- `E` = Exclusive Owner, **no probe filter entry**（:76）
- `O` = Owner, probe filter entry exists（:75）
- `NX` = Not Owner, block in O at Owner（:72）
- `NO` = Not Owner, block in E/M at Owner（:73）
- `S` = clean, probe filter points to current owner（:74）
- 大量 `*_B`（blocked）/`*_W`（waiting for Dram）/`*_R`（probe filter replacement）瞬态（:78-103）。

**核心结构**：`PfEntry`（probe filter entry，hammer-dir.sm:153-157：`MachineID Owner` + `Set Sharers`）。
probe filter 是一个 **CacheMemory**（组相联、会替换）——目录信息可以被驱逐！
替换触发 `Pf_Replacement` 事件（hammer-dir.sm:120）与 `*_R` 状态（:78-80），
被驱逐时须反向 probe 全部 cache（hammer-dir.sm:424-430 的 `probeFilter.cacheProbe(victim)` 路径）。

### 3.3 Hammer vs Directory vs Token 核心差异

| 维度 | MOESI_CMP_directory | MOESI_hammer | MOESI_CMP_token |
|------|--------------------:|-------------:|----------------:|
| 一致性元数据 | 精确目录（每行 Sharers+Owner，无替换） | **probe filter（可替换的 cache）** | 票计数（soft state，可不准） |
| 元数据缺失时行为 | 不会发生（目录完备） | 必须 probe 全部 cache（广播兜底） | 广播收票兜底 |
| 写回语义 | WB 经目录转内存（dir.sm:617-687） | 含 `SilentAcks` 计数（hammer-dir.sm:166）支持 **silent eviction**（clean 行静默丢弃） | 全票回目录 |
| 硬件原型 | 通用 CMP | AMD Opteron/HyperTransport（南北桥时代） | 学术（Wisconsin） |
| cache 分层 | L1+L2 两级 | 单级 + probe filter | L1+L2 两级 |

---

## 4. GPU coherence 特殊处理：GPU_VIPER

### 4.1 Ruby 配置：configs/ruby/GPU_VIPER.py

存在且是完整的 GPU 拓扑配置脚本。`create_system`（GPU_VIPER.py:978-1160）：

- **三级 Cluster 层次**：`mainCluster ⊇ {cpuCluster, gpuCluster}`（:994-1007, :1155-1156），
  基于 topologies/Cluster.py + topologies/Crossbar.py（:50-51）。
- **CPU 侧**：CorePair 控制器（CPCntrl, :104；2 核共享 L1I+L2，私有 L1D×2）+ 目录（DirCntrl, :382）。
- **GPU 侧**（全部加入 gpuCluster）：
  - TCP（construct_tcps, :1088-1093）—— GPU L1 数据缓存，每 CU 一个
  - SQC（construct_sqcs, :1096-1101）—— GPU L1 指令缓存
  - Scalar cache（construct_scalars, :1104-1109）—— 标量数据缓存（复用 SQCCntrl, :854）
  - Command Processor（construct_cmdprocs, :1112-1117）
  - TCC（construct_tccs, :1120-1124）—— GPU L2，banked（`--num-tccs`, :444）
  - DMA（:1126-1152）
- **地址映射**：`TCC_bits = log2(num_tccs)`（:546），TCC bank 选择位注入各控制器
  （`TCC_select_num_bits=TCC_bits`, :776, :821, :887）。
- **vnet 数量**：`number_of_virtual_networks = 11`（:1158）——全协议中最多
  （GPU 需要 request/probe/response/writeback 等多通道隔离防死锁）。

### 4.2 协议文件与机器类型

GPU_VIPER.slicc:1-10：复用 MOESI_AMD_Base 的 msg/dir/dma/CorePair/L3cache（CPU 侧），
GPU 专用 3 个 .sm：

| 文件 | machine | 证据 |
|------|---------|------|
| GPU_VIPER-TCP.sm:35 | `MachineType:TCP` "GPU TCP (L1 Data Cache)" | 带 `VIPERCoalescer* coalescer`（:36） |
| GPU_VIPER-SQC.sm | `MachineType:SQC` "GPU L1 Instr Cache" | MachineType 枚举 RubySlicc_Exports.sm:282 |
| GPU_VIPER-TCC.sm:34 | `MachineType:TCC` "TCC Cache" (GPU L2) | — |

### 4.3 GPU workload 适配证据（coalescing / write-through / streaming）

**Coalescing**：TCP 内嵌 `VIPERCoalescer`（GPU_VIPER-TCP.sm:36），
bypass 完成回调直接调 `coalescer.readCallback/writeCallback`（:381, :395）。

**Write-through L1**：
- TCP 事件集显式含 `StoreThrough` = "Store directly to L2(L1 is clean)"（GPU_VIPER-TCP.sm:68）
  与 `Store` = "Store to L1 (L1 is dirty)"（:67）——**L1 是写穿+写脏混合**（GPU 语义：kernel 内不写共享）。
- TCC 侧对应事件 `WrVicBlk` = "L1 Write Through"（GPU_VIPER-TCC.sm:61-63）。
- TCP Entry 含 `WriteMask writeMask`（:99）——字节级写掩码，匹配 GPU scatter/gather。

**Streaming / bypass**：
- 两级均支持整级旁路：TCP `LoadBypassEvict`/`Bypass`（:66, :81），TCC `RdBypassEvict`/`Bypass`（:60, :79）。
- TBE 含 `isGLCSet`（bypass L1）/ `isSLCSet`（bypass L1+L2）（:108-109）
  —— 直接映射 AMD GCN/RDNA 指令集的 GLC/SLC 位。

**简化状态空间**（kernel 边界同步语义，kernel 内 L1 间无一致性）：
- TCP 稳定态仅 **I / V / A / F**（:56-60）——无 M/O/S/E！V=Valid 即可读，invalidate 集中在 kernel 边界
  （`Flush`/`InvL2` 事件 :70-74）。
- TCC 稳定态仅 **M / W / V / I**（:84-87）。

**结论**：对 GPU workload（coalescing/write-through/streaming）支持最好的协议族是 **GPU_VIPER**
（唯一带 coalescer、GLC/SLC bypass、I/V/A/F 极简态的协议）。次优是 MI_example（无一致性开销，但不建模一致性）。

### 4.4 GPU 一致性拓扑图（依 GPU_VIPER.py create_system 还原）

```
                        ┌──────────── mainCluster (Crossbar, 16GB/s×BW) ──────────┐
                        │                                                         │
            ┌──── cpuCluster ────┐                                  ┌──── gpuCluster ───────────────────┐
            │                    │                                  │                                    │
        [CPCntrl ×N/2]      [DirCntrl ×num_dirs]            [TCP ×num_CUs]  [SQC ×N]  [Scalar]  [CmdProc]
         L1D0 L1D1 L1I       (MOESI_AMD_Base-dir)             │ (L1D, coalescer,                         │
         共享 L2              │  ◄── 内存侧真实目录             │  WT+bypass, I/V/A/F)                     │
            │                 │                               ├────────────────────────────────────────┤
            │                 │                               │        [TCC ×num_tccs] (GPU L2 banked) │
            │                 │                               │        M/W/V/I, TCC_select_num_bits    │
            │                 │                               ├────────────────────────────────────────┤
            │                 │                               │        [DMA controllers]               │
            └─────────────────┘                               └────────────────┬───────────────────────┘
                        │                                                      │
                        └──────────────► [Memory Controllers] ◄────────────────┘
                        (vnet = 11;  Dir 仅 CPU 侧可见 —— GPU 侧 L2 TCC 内含 TCCdir 目录变体,
                         MachineType:TCCdir 见 RubySlicc_Exports.sm:281)
```

### 4.5 MI200 / MI300X 的协议选择依据（评估）

gem5 对 AMD MI 系列 GPU 的建模选择 GPU_VIPER 的依据（从代码结构推断 + 标注为推断）：
1. **VIPER 是 gem5 中命名与 AMD GPU 硬件单元语义对齐的 GPU 协议**（**第三轮 critic 校正**：原文"经 AMD 硬件语义校准"无直接证据（S9），降级为"命名语义对齐"）—— TCP/TCC/SQC 命名即 AMD GPU 硬件单元名
   （Texture Cache per Pipe / Texture Cache per Channel / Sequencer Cache，RubySlicc_Exports.sm:279-282），
   GLC/SLC bypass 位直接对应 GCN/RDNA ISA（GPU_VIPER-TCP.sm:108-109）—— **第三轮 critic 校正**：原文 `GCN/RDMA` 是 typo（RDMA=Remote DMA），应为 `GCN/RDNA`。
2. **kernel-boundary 一致性**（acquire/release 在 kernel 启动/结束时 flush+invalidate，
   事件 Flush/Evict/InvL2, GPU_VIPER-TCP.sm:70-74）匹配 ROCm HSA 内存模型；
   MI200/MI300X 的 L1 间 kernel 内同样无硬件一致性。
3. **CPU↔GPU 一致性**走 MOESI_AMD_Base-dir（CPU 侧目录）+ CorePair，对应 APU/IF 互联场景。
4. MI300X 级 chiplet 规模下，目录协议的精确 sharer 位图仍是 gem5 可接受的（TCCdir 按 bank 分布，
   `noTCCdir=True` 选项见 GPU_VIPER.py:570, :634，可选择 L2 内嵌或独立目录）。
   ⚠️ 本小节 4 为工程推断（代码内无 MI300X 字样）；gem5 官方 GPU 文档（gpu.readthedocs）离线不可达。

---

## 5. CHI 协议族（src/mem/ruby/protocol/chi/）

### 5.1 完整文件清单（2026-10-16 critic 校正：**12 `.sm` 文件 + 1 `.slicc` + 2 手写 facade/外部子目录**）

```
chi/SConsopts, chi/Kconfig                          — 构建选项
chi/CHI.slicc:1-6                                   — 协议入口 ("CHI" partial_func_reads)
chi/CHI-msg.sm                                      — CHI 消息结构
chi/CHI-cache.sm                (+transitions/-ports/-funcs/-actions 4 个拆分文件)
                                                  — RN-F cache 状态机 (machine MachineType:Cache, CHI-cache.sm:39)
chi/CHI-mem.sm                                      — SN 内存节点 (machine MachineType:Memory, CHI-mem.sm:39)
chi/CHI-dvm-misc-node.sm        (+transitions/-ports/-funcs 4 个拆分文件)
                                                  — MN: DVM 操作分发 (machine MachineType:MiscNode, CHI-dvm-misc-node.sm:39)
```

SLICC 文件**按机器×方面二维拆分**（cache 拆成 transitions/ports/funcs/actions 4 文件）——
全协议库中唯一采用此结构的协议，反映 CHI 状态机的规模。

### 5.2 4 通道 → vnet 映射

CHI 的 REQ/SNP/RSP/DAT 四通道直接一对一映射到 4 个 Ruby vnet，
在**每种节点中布局完全一致**（CHI-cache.sm:189-197 / CHI-mem.sm:55-63 / CHI-dvm-misc-node.sm:100-108）：

| CHI 通道 | Ruby vnet | 方向语义 | vnet_type（Garnet 用） |
|----------|-----------|----------|------------------------|
| REQ | 0 | To/From 各一 | none |
| SNP | 1 | To/From 各一 | none |
| RSP | 2 | To/From 各一 | none |
| DAT | 3 | To/From 各一 | **response**（仅决定 credit 容量差异 data/ctrl，**不是 escape VC**，CHI-cache.sm:184 注释原文 "response type is assumed to have data, so use it for data channels"） |

即：`reqOut/reqIn` (vnet0)、`snpOut/snpIn` (vnet1)、`rspOut/rspIn` (vnet2)、`datOut/datIn` (vnet3)。
通道级分离保证 CHI 协议层防死锁（SNP 不被 REQ 阻塞）。

> **2026-10-16 critic 复审校正**：`grep -rni "escape" src/mem/ruby/` **0 命中**，原文档"escape VC"为误读；实际 Garnet 仅以 `vnet_type=response` 决定 credit 容量（`GarnetNetwork.cc:79-86`），并不增加 VC 数量。

### 5.3 多 master 多 slave 配置

**配置脚本**：configs/ruby/CHI.py + configs/ruby/CHI_config.py。

- 节点类型类（CHI_config.py）：`CHI_RNF`（master 请求节点，:476，含 L1I/L1D/L2 + 可选 L3）、
  `CHI_HNF`（Home Node + L3/LLC，:620）、`CHI_SNF_MainMem`/`CHI_SNF_BootMem`（slave 内存节点，:730/:779）、
  `CHI_RNI_DMA` / `CHI_RNI_IO`（I/O 请求节点，CHI.py:102-103）、`CHI_MN`（Misc/DVM 节点，:687）。
- **数量控制命令行参数**（gem5 标准 options）：`--num-cpus`（决定 RNF 数，CHI.py:123-128 每 CPU 一 RNF）、
  `--num-l3caches`（HNF 数，CHI.py:163-168）、`--num-dirs`（SNF 数，CHI.py:181-184，CHI 复用 num_dirs 语义）、
  `--enable-dvm`（CHI.py:52, 78-83）、`--chi-config <file>`（自定义 NoC 绑定，CHI.py:44-51, :86-89）。
- HNF 地址交织：`CHI_HNF.createAddrRanges(sysranges, cache_line_size, hnf_list)`（CHI.py:164）。
- 典型命令行：
  ```
  build/CHI/gem5.opt configs/example/ruby_fs.py --protocol=CHI \
      --num-cpus=8 --num-l3caches=4 --num-dirs=2 \
      --topology=Mesh_XY --mesh-rows=4 [--enable-dvm]
  ```
  （CustomMesh 拓扑必须配 `--chi-config` 文件，CHI.py:88-89。）
- 注意：CHI **不使用**其他协议共享的 `create_directories`（CHI.py:177-179 注释）——SNF/HNF 概念替代了
  传统 directory/memory 分离。

---

## 6. CMP（chip-multiprocessor）架构模式

### 6.1 控制器的统一抽象

- 所有 SLICC `machine(...)` 经 slicc 编译为 C++ 类 `<Protocol>_<MachineType>_Controller`，
  统一继承 **AbstractController**（src/mem/ruby/slicc_interface/AbstractController.hh/.cc）。
- **MachineType 全局枚举**（RubySlicc_Exports.sm:268-289）——一个协议不必实现全部类型，
  但只能用枚举内的类型，且每类型至多一个状态机（:265-267 注释）：
  `L0Cache/L1Cache/L2Cache/L3Cache/Directory/DMA/Collector/L1Cache_wCC/L2Cache_wCC/CorePair/
   TCP/TCC/TCCdir/SQC/RegionDir/RegionBuffer/Cache/Memory/MiscNode`。
- 各控制器角色在 CMP 模板中的统一抽象：
  | 角色 | MachineType | 典型协议落点 |
  |------|-------------|--------------|
  | Directory controller | `Directory` | MOESI_CMP_directory-dir.sm:41 |
  | Memory controller | `Memory`（CHI）或 dir 内嵌 `requestToMemory` 直连（其余协议，dir.sm:57-58） | RubySlicc_MemControl.sm 提供 functionalMemoryRead/Write |
  | DMA controller | `DMA` | *-dma.sm，配 DMASequencer（MOESI_CMP_directory.py:240-249） |
  | HTM | 无独立 MachineType —— **HTM 是 L0Cache 状态机内嵌能力**（MESI_Three_Level_HTM-L0cache.sm:42 `HTMSequencer* sequencer`，事件 HTM_Start/Commit/Abort :152-158） | MESI_Three_Level_HTM.slicc:1-8 复用 MESI_Two_Level 的 dir/L2/dma |

### 6.2 控制器互连的 MessageBuffer 拓扑（以 MOESI_CMP_directory 为例）

Python 装配侧（configs/ruby/MOESI_CMP_directory.py:138-148, 191-207, 222-234, 254-262）
与 SLICC 声明侧（dir.sm:47-59, L1cache.sm:53-69）一一对应：

```
            ┌──────────────── Ruby Network (NoC, 3 vnets: 0=req 1=fwd/dir-req 2=resp) ───────────────┐
            │                                                                                       │
 [L1Cache_i]                [L2Cache_j]                      [Directory_k]                 [DMA_m]
  mandatoryQueue             mandatoryQueue                   triggerQueue (ordered)      mandatoryQueue
  requestFromL1Cache  ─To►   GlobalRequestFromL2Cache ─To►    requestToDir      ◄From─    reqToDir ─To►
  responseFromL1Cache─To►    L1RequestFromL2Cache   ─To►      responseToDir     ◄From─    respToDir ─To►
  requestToL1Cache  ◄From─   GlobalRequestToL2Cache ◄From─    responseFromDir   ─To►      responseFromDir ◄From─
  responseToL1Cache ◄From─   responseToL2Cache      ◄From─    forwardFromDir    ─To►      triggerQueue
  triggerQueue               L1RequestToL2Cache     ◄From─         │
                             triggerQueue                          │ requestToMemory ──►  [MemCtrl]
                                                                   │ (点对点直连, 不过 NoC)
                                                                   │ responseFromMemory ◄──
```

装配规则（证据）：
1. 每个 `MessageBuffer *` 在 .sm 中声明 `network="To"/"From"` + `virtual_network="N"` + `vnet_type=...`，
   Python 侧把 `*.out_port = ruby_system.network.in_port`（To）或 `*.in_port = ...out_port`（From）
   （MOESI_CMP_directory.py:139-148）。
2. **triggerQueue 一律 `ordered=True`**（:148, :207, :234, :262）——协议内自触发事件保序。
3. **mandatoryQueue 不联网**——它是 CPU sequencer → cache 的本地入口队列（:139, :255）。
4. **目录↔内存控制器是网络外点对点**（:232-233 不挂 network port）——off-chip 访存不占 NoC 带宽。
5. 节点创建顺序必须与 NetDest 编号一致（:80-83 注释：L1 → dir → dma）。
6. vnet 总数由协议声明：MOESI_CMP_directory=3（:291），GPU_VIPER=11（GPU_VIPER.py:1158），
   CHI=4（§5.2），**token=6**（**第三轮 critic 校正**：原"token=4"是误算，实测 `MOESI_CMP_token.py:297` `number_of_virtual_networks=6`，包含 vnet 0-5；persistent 占用 vnet 3，但还有别的 vnet 4-5）

---

## 7. 对 CppTLM 的可迁移结论（oracle 视角）

1. **目录协议的正确性内核 = Owner/Sharers 记账 + recycle 兜底**（dir.sm:947-949 的
   "瞬态遇冲突请求→recycle 重试"模式），与 CppTLM PcieCompletionTracker 的 trans_id 关联是同一模式族。
2. **Token 思想的价值不在协议本身，而在"soft-state 目录 + 计数不变量"** —— 若 CppTLM 未来建模
   GPU 规模一致性，建议借鉴"正确性不变量（票守恒）与性能结构（目录）解耦"，但传输走
   NoCFlitBundle 单播/多播，不要引入广播 vnet。
3. **GPU_VIPER 的 I/V/A/F + kernel-boundary flush/invalidate 是 GPU L1 的正确简化**，
   与 CppTLM `LsuGlobal/LsuLDS` 的 GPU memory 语义对齐成本最低；GLC/SLC bypass 位（TBE :108-109）
   可直接映射为 Bundle 字段。
4. **CHI 4 通道 → 4 vnet 映射与 CppTLM `vc_counts_[4]` 的对齐关系（**2026-10-16 critic 复审校正**：**下述"完全同构"主张不成立**）
   - gem5 CHI 设计：`reqOut/reqIn`(vnet0)/`snpOut/snpIn`(vnet1)/`rspOut/rspIn`(vnet2)/`datOut/datIn`(vnet3)
   - CppTLM e1 v1.1 `vc_counts_[4]`：`soc_internal_noc.hh:67-75` 注释声称 4 bucket（REQ=VC0/RSP=VC1/DAT=VC2/SNP=VC3）
   - **关键校正**：`noc_bundles_tlm.hh:60-62` 实测只有 3 个 `CATEGORY_*` 常量（CATEGORY_REQUEST=0、CATEGORY_RESPONSE=1、CATEGORY_CREDIT=2），**不存在 `CATEGORY_DAT`**
   - 同时 `soc_internal_noc.hh:67-75` 注释把 `CATEGORY_CREDIT(2)` 错标为 `VC_SNP(3)`——**这是 CppTLM 内部文档与代码自相矛盾**（非 PNN 调研能解决）
   - **降级表述**：e1 v1.1 4 bucket 设计与 CHI 4 通道在**数量级上巧合对齐**，**语义上不等价**——第 4 bucket (DAT/SNP) 的实际语义与 gem5 CHI DAT 通道不可直接对应
   - gem5 CHI 仍可作为 vnet 数量 / ordered-unordered 设计的参考，**但不应作为 e1 v1.1 4 bucket 语义对位的交叉验证基准**
5. **MessageBuffer 装配纪律**（triggerQueue ordered、目录↔内存直连不占 NoC、节点创建顺序=NetDest 编号）
   是 Ruby 防死锁与确定性的工程地基，值得 CppTLM SimModule 装配层对照。

## 附录：关键证据索引

| 主题 | 位置 |
|------|------|
| Directory 状态机 | src/mem/ruby/protocol/MOESI_CMP_directory-dir.sm:41-1118 |
| Owner/Sharers 不变量 | MOESI_CMP_directory-dir.sm:197-209 |
| forwardRequest | MOESI_CMP_directory-dir.sm:507-539 |
| WB 协议 | MOESI_CMP_directory-dir.sm:617-687, :1042-1079 |
| max_tokens=1024 | src/mem/ruby/slicc_interface/RubySlicc_Util.hh:108-111 |
| token 目录 Entry | MOESI_CMP_token-dir.sm:124-134 |
| token L1 断言 | MOESI_CMP_token-L1cache.sm:302-362 |
| persistent vnet | MOESI_CMP_token-L1cache.sm:58-70 |
| hammer probe filter | MOESI_hammer-dir.sm:33-38, :70-103, :153-157 |
| GPU VIPER 拓扑 | configs/ruby/GPU_VIPER.py:978-1160 |
| TCP coalescer/WT/bypass | GPU_VIPER-TCP.sm:36, :56-81, :108-109 |
| TCC 状态 | GPU_VIPER-TCC.sm:34, :57-87 |
| CHI vnet 映射 | chi/CHI-cache.sm:189-197; chi/CHI-mem.sm:55-63 |
| CHI 配置 | configs/ruby/CHI.py:66-199; configs/ruby/CHI_config.py:476-779 |
| MachineType 枚举 | src/mem/ruby/protocol/RubySlicc_Exports.sm:268-289 |
| HTM | src/mem/ruby/protocol/MESI_Three_Level_HTM-L0cache.sm:41-42, :151-158 |
| CMP MessageBuffer 装配 | configs/ruby/MOESI_CMP_directory.py:138-293 |
