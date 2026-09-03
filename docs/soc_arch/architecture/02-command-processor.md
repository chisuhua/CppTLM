# 02. dGPU SoC v1.0 Command Processor 架构 — 双 vendor PM4 解码

> **类别**: SoC Architecture > 子系统架构 (L3 Command Protocol)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（§3.3 L3 Command Protocol 层）
> **关联现有模块微架构**:
> - [`docs/soc_arch/modules/command-processor.md`](../modules/command-processor.md)（ADR-SOC-06 D5 v0.5 MVP 路径）
> - [`docs/soc_arch/modules/pm4-decoder.md`](../modules/pm4-decoder.md)（NVIDIA method packet 简化）
> **关联研究综述**:
> - [`docs/research/CP/nvidia/overview.md`](../../research/CP/nvidia/overview.md)（NVIDIA CP 专利综述:SM 队列管理器 + 计算图 + 可执行图修改）
> - [`docs/research/CP/amd/overview.md`](../../research/CP/amd/overview.md)（AMD CP 专利综述:HQD + IB prefetch + 用户态分发）
> **关联 ADR**: ADR-SOC-04（HSAPP 极简化）/ ADR-SOC-06 D5（CommandProcessor + Pm4Decoder + 双 vendor 扩展）/ ADR-SOC-09（Proposed,v1.0 NVIDIA+AMD dual vendor）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.3 L3 Command Protocol 层的**子系统架构**详细化文档,涵盖双 vendor PM4 协议解码。

- 想快速理解 L3 层结构 → 读 §1(范围与目标) + §2(顶层数据流)
- 想理解 CommandProcessor 5-state FSM → 读 §3
- 想理解 NVIDIA PM4 method packet → 读 §4
- 想理解 AMD PM4 TYPE3 opcode → 读 §5
- 想理解双 vendor 切换与状态机 → 读 §6
- 想理解 PushBuffer/Ring 读取 → 读 §7
- 想理解 Pm4Decoder 子模块结构 → 读 §8
- 想理解 v1.0 战略对齐 → 读 §9
- 想理解配置 Schema → 读 §10
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §11
- 想评估风险 → 读 §12

---

## 1. 范围与目标

### 1.1 L3 Command Protocol 层定位

**L3 Command Protocol 层** = dGPU SoC v1.0 系统拓扑的**前端命令流层**,负责:

- **接收** L2 层 PushBuffer (NVIDIA) / Ring Buffer (AMD AQL) 写入的命令流
- **解码** 双 vendor PM4 命令包(NVIDIA method packet / AMD PM4 TYPE3 opcode)
- **派发** 解码后的命令到 L4 TMU/TMD 层
- **状态机** 5-state FSM(IDLE → FETCH → DECODE → DISPATCH → COMPLETE)

**L3 层模块清单**(per `00-overview` §3.3):

| 模块 | 角色 | v1.0 状态 |
|------|------|---------|
| **CommandProcessor** | 5-state FSM 主控 | ✅ v0.5 实施(per `command-processor.md` + ADR-SOC-06 D5) |
| **Pm4Decoder-NV** | NVIDIA method packet 解码子模块 | ✅ v0.5 4 method_addr ranges + 18 opcodes 简化(per `pm4-decoder.md`) |
| **Pm4Decoder-AMD** | AMD PM4 TYPE3 opcode 解码子模块 | 🔵 v1.0 MVP 新增(per ADR-SOC-09 D2) |
| **PushBuffer Reader** | NVIDIA pushbuffer 读取 | ✅ v0.5 实施 |
| **Ring Buffer Reader** | AMD ring/AQL reader | 🔵 v1.0 MVP 新增 |

### 1.2 v1.0 战略关键决策(per `00-overview` §4-bis R07-R09)

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联决策 |
|--------|---------|------------|---------|
| CommandProcessor 5-state FSM | ✅ 共享 NVIDIA/AMD | 同 v1.0 | D5(ADR-SOC-06) |
| Pm4Decoder-NV | ✅ 4 method_addr + 18 opcodes 简化 | + 全 18 opcodes | D5 |
| Pm4Decoder-AMD | ✅ TYPE3 opcode 基础 | + 全 TYPE3 opcode + 子类型 | ADR-SOC-09 D2（Proposed） |
| 双 vendor 运行时切换 | ✅ `nv_mode` / `amd_mode` config param | 同 v1.0 | D2(ADR-SOC-09) |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.1 PASS 的 §3.3 L3 Command Protocol 层 + §4-bis 范围矩阵 R07-R09 + §6.1 兼容性分析。

---

## 2. 顶层数据流图

### 2.1 L3 层数据流总览

```
                    ┌─────────────────────────────────────────────┐
                    │            L2 Command Stream                │
                    │  - NVIDIA PushBuffer                        │
                    │  - AMD Ring Buffer / AQL Queue              │
                    │  - Doorbell (L2 强序 write 通知)           │
                    └───────────────────┬─────────────────────────┘
                                        │
                                        ▼
                  ┌──────────────────────────────────────────┐
                  │  CommandProcessor  (5-state FSM 主控)    │
                  │  IDLE → FETCH → DECODE → DISPATCH → COMPLETE│
                  └─────────┬────────────────────┬────────────┘
                            │                    │
              ┌─────────────▼──────┐    ┌─────────▼──────────────┐
              │ Pm4Decoder-NV     │    │ Pm4Decoder-AMD         │
              │ NVIDIA method     │    │ AMD PM4 TYPE3 opcode   │
              │ packet (4 ranges, │    │ (HQD + IB + 用户态分发)│
              │ 18 opcodes)       │    │                        │
              └─────────┬─────────┘    └────────────┬───────────┘
                        │                          │
                        └─────────────┬────────────┘
                                      │
                                      ▼
                  ┌──────────────────────────────────────────┐
                  │           L4 TMU/TMD 层                   │
                  │  - TMD Cache (优先级分层)                 │
                  │  - 依赖预取(per US11182207B2)             │
                  │  - Stream Event(嵌套流, v1.1)             │
                  └──────────────────────────────────────────┘

   ──────────────────  运行时 vendor 切换  ──────────────────

   ┌────────────────────────────────────────────────────────┐
   │  JSON config param: `nv_mode` / `amd_mode`             │
   │  - 默认 nv_mode (NVIDIA 路径,CUDA driver 主流)         │
   │  - amd_mode 可选(ROCm KFD driver,需 UsrLinuxEmu 联签)│
   └────────────────────────────────────────────────────────┘
```

### 2.2 关键交互注释

- **L2 → L3**:PushBuffer (NVIDIA) / Ring Buffer (AMD) 经 PCIe TLP / Posted Write 写入到 SoC 内存;Doorbell 强序 write 通知
- **L3 解码**:CommandProcessor 在 FETCH 状态读取 PushBuffer/Ring;DECODE 状态调用 Pm4Decoder-NV 或 Pm4Decoder-AMD
- **L3 → L4**:解码后的命令封装为 TMD (Task Metadata Descriptor),送入 TMU 优先级分层 Scheduler Table

---

## 3. CommandProcessor 5-state FSM

### 3.1 状态机定义

CommandProcessor 实现 5-state FSM(per ADR-SOC-06 D5 + `command-processor.md`):

| 状态 | 转换条件 | 动作 |
|------|---------|------|
| **IDLE** | doorbell_ring() 触发 | → FETCH |
| **FETCH** | 读取 PushBuffer/Ring 头部成功 | 解析命令指针 + 参数;→ DECODE |
| **DECODE** | Pm4Decoder-NV/AMD 解析成功 | 构造 TMD;→ DISPATCH |
| **DISPATCH** | TMU 接受 TMD | TMU Scheduler Table 入队;→ COMPLETE |
| **COMPLETE** | 当前命令处理完 | 返回 IDLE(等待下一个 doorbell) |

### 3.2 状态机实现

```cpp
class CommandProcessor {
public:
    enum class State { IDLE, FETCH, DECODE, DISPATCH, COMPLETE };
    
    void tick() {
        switch (state_) {
            case State::IDLE:
                if (doorbell_ring()) {
                    state_ = State::FETCH;
                    fetch_addr_ = current_doorbell_addr_;
                }
                break;
            case State::FETCH:
                if (read_pushbuffer_header(fetch_addr_, &cmd_header)) {
                    state_ = State::DECODE;
                }
                break;
            case State::DECODE:
                // 双 vendor 解码
                if (nv_mode_) {
                    tmd_ = pm4_decoder_nv_.decode(cmd_header);
                } else {
                    tmd_ = pm4_decoder_amd_.decode(cmd_header);
                }
                if (tmd_.valid) state_ = State::DISPATCH;
                break;
            case State::DISPATCH:
                if (tmu_.enqueue(tmd_)) {
                    state_ = State::COMPLETE;
                }
                break;
            case State::COMPLETE:
                state_ = State::IDLE;
                break;
        }
    }
};
```

### 3.3 关键设计原则

- **单一 FSM 主控**:NVIDIA/AMD 双 vendor 共用 5-state FSM(per `00-overview` §3.3 D5)
- **解码子模块化**:Pm4Decoder-NV/AMD 作为独立子模块,通过 `nv_mode` config param 切换
- **TMD 统一输出**:双 vendor 解码后均输出标准 TMD 结构(per `00-overview` §3.5 L4 TMU)

---

## 4. NVIDIA PM4 Method Packet(per Pm4Decoder-NV)

### 4.1 NVIDIA PM4 协议背景

**NVIDIA PM4 method packet** 是 NVIDIA GPU 私有命令协议(per `docs/research/CP/nvidia/overview.md`):

- **PM4 method packet** = GPU command packet,封装 pushbuffer 写入
- **4 method_addr ranges**(per ADR-SOC-06 D5 + `pm4-decoder.md`):
  - `DISPATCH_DIRECT`:Kernel launch 直接派发
  - `EVENT_WRITE`:事件写入(信号量/同步)
  - `RELEASE_MEM`:内存释放同步
  - `ACQUIRE_MEM`:内存获取同步
- **18 opcode 范围框架(4 MVP + 14 deferred)**:v1.0 仅 4 核心 opcode 完整支持(per pm4-decoder.md),v1.1 全 18 opcode

### 4.2 关键专利参考(per `docs/research/CP/nvidia/overview.md`)

| 专利 | 主题 | 用途 |
|------|------|------|
| **US9489763B2** | pushbuffer/draw call 建立(Kepler 世代) | pushbuffer → host interface 206 → front end 212 → task/work unit 207 通路 |
| **US10489056B2** | SM 系统队列管理器(Volta 时代,2017/2019) | GPC 内 TPC 层 QM 718,base/head/tail/xhead/xtail 四指针,标量命令 2B 编码 |
| **US20230153146A1** | 用户态直接提交(Ampere+ 安全计算) | Secure Task Launch System 400,Notifier/Scheduler/Copy Engines |
| **US20210149719A1** | 可执行图修改(CUDA Graphs) | graph 构建 3210/实例化 3220/启动 3230/就地修改节点参数 3240 |
| **US20210248115A1** | 计算图优化 | graph code 实例化一次后以不同参数二次执行免 reinstantiation |

### 4.3 Pm4Decoder-NV 实现要点

- **解码 NVIDIA method packet**（NVIDIA 格式无 TYPE 字段；AMD PM4 TYPE3 由独立 Pm4Decoder-AMD 处理）
- **method_addr 路由**:根据 method_addr 值路由到 4 个处理子模块(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM)
- **18 opcode 范围框架(4 MVP + 14 deferred):v1.0 仅 4 核心 opcode 完整支持(per `pm4-decoder.md`),v1.1 全 18 opcode
- **TMD 构造**:解码后构造标准 TMD 描述符(Init/Sched/Exec/QueueState/HW-only/Dep/Queue 区,per `docs/research/TMU/TMD.md`)

---

## 5. AMD PM4 TYPE3 Opcode(per Pm4Decoder-AMD)

### 5.1 AMD PM4 TYPE3 协议背景

**AMD PM4 TYPE3 opcode** 是 AMD GPU 命令协议(per `docs/research/CP/amd/overview.md`):

- **PM4 packet TYPE3** = 含 dword 索引的命令包(类似 NVIDIA method packet 但格式不同)
- **HQD**(Hardware Queue Descriptor):AMD 私有硬件队列描述符(per US8310492B2)
- **IB**(Indirect Buffer):间接命令缓冲(per US20210304349A1 / US20220091847A1)
- **用户态分发**:KFD IOCTL → 用户态直接提交(per US9176795B2)

### 5.2 关键专利参考(per `docs/research/CP/amd/overview.md`)

| 专利 | 主题 | 用途 |
|------|------|------|
| **US8310492B2** | HQD 硬件队列调度 | Queue Descriptor + ring buffer 模型 |
| **US8675002B1** | 统一命令缓冲 | Unified command buffer |
| **US9176795B2** | 用户态图形处理分发 | User-mode graphics dispatch |
| **US20210191730A1** | 未映射队列聚合门铃 | Unmapped queue aggregated doorbell |
| **US20210304349A1** | 迭代 IB(Indirect Buffer) | Iterative IB 链式 |
| **US20220091847A1** | IB 预取 | IB prefetch |
| **US11822956B2** | 自适应线程组分发 | Adaptive threadgroup dispatch |
| **US12131186B2** | 硬件加速动态工作创建 | Dynamic work creation |

### 5.3 Pm4Decoder-AMD 实现要点(v1.0 MVP)

- **解码 PM4 TYPE3 opcode**(AMD 公开,格式与 NVIDIA 不同)
- **HQD 字段映射**:Queue Descriptor → 标准 TMD 字段
- **IB 链式解析**:支持多级 IB 链(per US20210304349A1)
- **用户态门铃支持**:KFD IOCTL → Ring entry(per US20210191730A1)
- **TMD 构造**:与 Pm4Decoder-NV 共用标准 TMD 描述符
- **v1.0 MVP 简化**:仅基础 TYPE3 opcode + IB 链式 + HQD 映射;v1.1 完整版追加全 TYPE3 opcode + 动态工作创建

### 5.4 双 vendor 切换示例

```cpp
class Pm4DecoderAMD {
public:
    TMD decode(const Pm4Packet& pkt) {
        if (pkt.type != Pm4Type::TYPE3) return {};  // 仅支持 TYPE3
        
        switch (pkt.opcode) {
            case 0x15:  // DISPATCH_DIRECT (AMD)
                return build_dispatch_tmd(pkt);
            case 0x40:  // DISPATCH_INDIRECT (IB,示意映射)
                return build_indirect_tmd(pkt);
            case 0x46:  // EVENT_WRITE (AMD)
                return build_event_tmd(pkt);
            // AMD 基础 TYPE3 opcode；完整 TYPE3 opcode 推迟到 v1.1
            default:
                return {};  // 未实现
        }
    }
};
```

---

## 6. 双 Vendor 切换与状态机

### 6.1 运行时切换机制(per D2 ADR-SOC-09)

**双 vendor 运行时切换**通过 JSON config param 实现:

```json
{
  "name": "command_processor_0",
  "type": "CommandProcessor",
  "params": {
    "vendor_mode": "nv_mode",   // "nv_mode" (默认) | "amd_mode"
    "pushbuffer_base": "0x10000000",
    "ring_buffer_base": "0x20000000",
    "max_outstanding": 32
  }
}
```

**切换行为**:
- **nv_mode**(默认):CommandProcessor 启动时初始化 Pm4Decoder-NV;AmdMode 解码器不可用
- **amd_mode**:CommandProcessor 启动时初始化 Pm4Decoder-AMD;NVMode 解码器不可用
- **共享状态机**:5-state FSM 在两种模式下完全相同,仅 DECODE 阶段调用不同 decoder

### 6.2 与 UsrLinuxEmu 跨仓依赖

**amd_mode 启用前提**(per ADR-SOC-09 D3):
- UsrLinuxEmu 仓需实现 KFD IOCTL 路径(per UsrLinuxEmu ADR-088 §C2 23 ABI)
- 当前 UsrLinuxEmu ADR-088 范围是 NVIDIA-only,**amd_mode 需 UsrLinuxEmu owner 联签**
- v1.0 MVP:amd_mode 接口预留,**跨仓承诺未达成前不强制要求通过 E2E 测试**

### 6.3 与 ComputeUnitTLM 蓝图共享(per D2 ADR-SOC-09)

```cpp
class CommandProcessor {
    // 共享 5-state FSM + TMD 输出
    void tick();  // 状态机
    
    // 双 vendor decoder
    Pm4DecoderNV pm4_decoder_nv_;   // nv_mode 时启用
    Pm4DecoderAMD pm4_decoder_amd_; // amd_mode 时启用
    
    // 输出标准 TMD → TMU(per L4 TMU/TMD)
    void dispatch_tmd(const TMD& tmd);
};
```

---

## 7. PushBuffer / Ring Buffer 读取

### 7.1 PushBuffer 读取(NVIDIA)

**PushBuffer 结构**(per US9489763B2):
- **指针链表**:pushbuffer 存命令数据结构指针的指针
- **dword 索引**:每个 PM4 packet 含 TYPE 字段 + dword 索引
- **异步执行**:GPU 异步于 CPU 读取 pushbuffer

**PushBuffer Reader 实现**:
- **v0.5 已实施**(per `command-processor.md` §3)
- **物理地址**映射到 GPU VA (VRAM)(per `command-processor.md` H1 修订:CP 从 GPU VA 读 ring buffer,BAR0 MMIO 仅放 doorbell)
- **v1.0 升级**:与 HostBypassTLM 兼容,支持 4 方向 AXI 桥接(per 429327d)

### 7.2 Ring Buffer 读取(AMD, v1.0 新增)

**Ring Buffer 结构**(per US8310492B2 HQD):
- **base + tail**:ring buffer 起始地址 + 当前写指针
- **doorbell**:CPU 写 doorbell 通知 GPU 新命令
- **KFD queue descriptor**:per-process/per-queue metadata

**Ring Buffer Reader 实现**(v1.0 MVP 简化):
- **HQD 字段解析**:从 HQD 读取 base/tail/dma_address
- **Ring entry 解析**:从 base + tail * entry_size 读取下一条 ring entry
- **AQL packet 解析**:若 HQD 标记为 AQL queue,使用 AQL 协议解析(per AMD AQL spec)
- **v1.1 完整版**:支持 KFD 用户态分发(per US9176795B2)

### 7.3 共享物理地址空间

```
PcieEndpointIP BAR0 window (pushbuffer/ring buffer 共用)
├── PushBuffer (NVIDIA, nv_mode)    [v0.5 实施]
└── Ring Buffer (AMD, amd_mode)     [v1.0 MVP 新增]
```

---

## 8. Pm4Decoder 子模块结构

### 8.1 Pm4Decoder-NV

**模块位置**:`include/tlm/gpu/pm4_decoder_nv.hh`(per `pm4-decoder.md` 命名)

**类结构**:
```cpp
class Pm4DecoderNV {
public:
    TMD decode(const Pm4Packet& pkt);
    
    // 4 method_addr ranges 处理
    TMD decode_dispatch_direct(const Pm4Packet& pkt);   // DISPATCH_DIRECT
    TMD decode_event_write(const Pm4Packet& pkt);       // EVENT_WRITE
    TMD decode_release_mem(const Pm4Packet& pkt);       // RELEASE_MEM
    TMD decode_acquire_mem(const Pm4Packet& pkt);       // ACQUIRE_MEM
    
private:
    // 18 opcodes MVP 简化
    std::map<uint8_t, OpcodeHandler> opcode_handlers_;
};
```

### 8.2 Pm4Decoder-AMD

**模块位置**:`include/tlm/gpu/pm4_decoder_amd.hh`(v1.0 MVP 新增)

**类结构**:
```cpp
class Pm4DecoderAMD {
public:
    TMD decode(const Pm4Packet& pkt);
    
    // 基础 TYPE3 opcode 处理
    TMD decode_dispatch_direct(const Pm4Packet& pkt);   // 0x10
    TMD decode_dispatch_indirect(const Pm4Packet& pkt);  // 0x11 (IB)
    TMD decode_event_write(const Pm4Packet& pkt);       // 0x12
    // ... 基础 8 opcode v1.0 MVP
    
private:
    std::map<uint8_t, OpcodeHandler> opcode_handlers_;
    HqdState hqd_state_;  // HQD 状态跟踪
};
```

### 8.3 与 TMD 输出统一

**双 vendor 解码均输出标准 TMD**:
- 共享 `TMD` 结构(Init/Sched/Exec/QueueState/HW-only/Dep/Queue 区,per `docs/research/TMU/TMD.md`)
- L4 TMU 无需区分 vendor,统一 Scheduler Table 入队
- **简化 L4 实现**:TMU 仅需处理一种 TMD 格式

---

## 9. v1.0 战略对齐

### 9.1 与 `00-overview` 一致性

| 维度 | `00-overview` 描述 | 本文实现 |
|------|-------------------|---------|
| L3 层 5-state FSM | ✅ 共享(per §3.3 D5) | ✅ §3 实施 |
| Pm4Decoder-NV | ✅ v0.5 4 method_addr + 18 opcodes 简化 | ✅ §4 实施 |
| Pm4Decoder-AMD | ✅ v1.0 MVP 新增 TYPE3 opcode 基础 | ✅ §5 实施 |
| 双 vendor 切换 | ✅ `nv_mode`/`amd_mode` config param | ✅ §6 实施 |
| PushBuffer/Ring | ✅ PushBuffer v0.5 实施;Ring v1.0 新增 | ✅ §7 实施 |

### 9.2 v1.0 MVP / v1.1 范围矩阵(per §4-bis)

| 特性 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| CommandProcessor 5-state FSM | ✅ | 同 v1.0 |
| Pm4Decoder-NV | ✅ 4 method_addr + 18 opcodes 简化 | + 全 18 opcodes |
| Pm4Decoder-AMD | ✅ TYPE3 opcode 基础 | + 全 TYPE3 + IB 链完整 + 动态工作创建 |
| PushBuffer Reader | ✅(v0.5 沿用) | 同 v1.0 |
| Ring Buffer Reader | ✅ 基础 + HQD + AQL | + KFD 用户态分发 |

### 9.3 与 ADR-SOC 一致性

| ADR | 关联 |
|-----|------|
| ADR-SOC-04 | HSAPP/CP/Dispatcher 极简化(单 KernelLaunchTLM ~150 行,本文 §3 FSM 简化版) |
| ADR-SOC-06 D5 | CommandProcessor + Pm4Decoder 双 vendor 扩展 |
| ADR-SOC-09 D2（Proposed） | 双 decoder 子模块双 vendor 切换 |
| ADR-SOC-09 D3（Proposed） | UsrLinuxEmu KFD 跨仓依赖标注 |

---

## 10. 配置 Schema

### 10.1 顶层 JSON Schema

```json
{
  "name": "command_processor_0",
  "type": "CommandProcessor",
  "params": {
    "vendor_mode": "nv_mode",     // "nv_mode" (默认) | "amd_mode"
    "pushbuffer_base": "0x10000000",  // 仅 nv_mode 使用
    "pushbuffer_size": 262144,        // 256 KB 默认
    "ring_buffer_base": "0x20000000", // 仅 amd_mode 使用
    "ring_buffer_size": 262144,       // 256 KB 默认
    "max_outstanding": 32,
    "enable_ib_chain": true,           // AMD IB 链式(per US20210304349A1)
    "enable_kfd_user_space": false,    // v1.0 默认 false(需 UsrLinuxEmu 联签)
    "pm4_decoder_nv_opcodes": 18,      // v0.5 沿用
    "pm4_decoder_amd_opcodes": 8       // v1.0 MVP 基础
  }
}
```

### 10.2 连接(connection)示例（概念路径，非当前 JSON Schema）

```json
{
  "connections": [
    {
      "src": "host_bypass.axi_slave_in",
      "dst": "pcie_ep.req_in[0]"
    },
    {
      "src": "command_processor_0.tmd_out",
      "dst": "tmu_0.tmd_in"
    },
    {
      "src": "gpu_va_memory.pushbuffer_read",
      "dst": "command_processor_0.pushbuffer_in"
    },
    {
      "src": "gpu_va_memory.ring_buffer_read",
      "dst": "command_processor_0.ring_buffer_in"
    }
  ]
}
```

---

## 11. ADR/微架构/OpenSpec 引用矩阵

### 11.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-04 | HSAPP/CP/Dispatcher 极简化(简化基础) |
| ADR-SOC-06 D5 | CommandProcessor 5-state FSM + Pm4Decoder 基础 |
| ADR-SOC-09（Proposed） | v1.0 NVIDIA+AMD dual vendor 战略(本文 §6 双 vendor 切换依据) |

### 11.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **CommandProcessor** | [`docs/soc_arch/modules/command-processor.md`](../modules/command-processor.md) |
| **Pm4Decoder (旧)** | [`docs/soc_arch/modules/pm4-decoder.md`](../modules/pm4-decoder.md)(NVIDIA method packet 简化,v0.5 MVP) |
| **PcieEndpointIP** | [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md) |

### 11.3 关联 OpenSpec changes

| Change | 关联内容 |
|--------|---------|
| `2026-08-19-cpptlm-v05-redo/` (archive) | v0.5 redo 完整版 12 周 P0'-P4' 路径 |
| `2027-02-09-cpptlm-dgpu-pcie-ip-integration/` | Phase 8 整合交付 + 4 方向 AXI 桥接 |
| `2027-02-09-cpptlm-dgpu-soc-v1-architecture/` | v1.0 总架构蓝图归口 |

### 11.4 关联研究综述

| 综述 | 关联内容 |
|------|---------|
| [`docs/research/CP/nvidia/overview.md`](../../research/CP/nvidia/overview.md) | NVIDIA CP 专利综述(SM 队列管理器 + 计算图) |
| [`docs/research/CP/amd/overview.md`](../../research/CP/amd/overview.md) | AMD CP 专利综述(HQD + IB prefetch + 用户态分发) |
| [`docs/research/US10489056B2_SM系统队列管理器_解析.md`](../../research/CP/nvidia/US10489056B2_SM系统队列管理器_解析.md) | SM 级 queue manager QM 718 |
| [`docs/research/US8310492B2_HQD硬件队列调度_解析.md`](../../research/CP/amd/US8310492B2_HQD硬件队列调度_解析.md) | AMD HQD 模型 |
| [`docs/research/US20210191730A1_未映射队列聚合门铃_解析.md`](../../research/CP/amd/US20210191730A1_未映射队列聚合门铃_解析.md) | AMD 聚合 doorbell |
| [`docs/research/US9176795B2_用户态图形处理分发_解析.md`](../../research/CP/amd/US9176795B2_用户态图形处理分发_解析.md) | AMD 用户态 KFD 分发 |

---

## 12. 风险与缓解 R1-R5

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | amd_mode 启用需 UsrLinuxEmu 跨仓承诺(当前 NVIDIA-only) | 🟡 中 | amd_mode 接口预留;v1.0 MVP 不强制 E2E 测试;UsrLinuxEmu 反馈后追加 D4 子 ADR |
| **R2** | Pm4Decoder-AMD 仅基础 8 opcode,v1.1 完整版需扩展 | 🟢 低 | v1.1 完整版追加;v1.0 MVP 覆盖核心 dispatch/event/release/acquire |
| **R3** | PushBuffer/Ring 物理地址空间共享,可能地址冲突 | 🟢 低 | JSON config 显式指定 pushbuffer_base / ring_buffer_base |
| **R4** | 18 opcodes 简化可能遗漏关键功能 | 🟡 中 | per ADR-SOC-06 D5,v1.0 MVP 简化;v1.1 完整版追加 |
| **R5** | 双 vendor 切换的运行时状态(已派发命令)处理 | 🟡 中 | 切换需 reset State Machine;运行时 vendor 锁定直到下次 reset |

---

## 13. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L3 Command Protocol 子系统架构,基于 NVIDIA/AMD PM4 双 vendor + 现有 command-processor.md + pm4-decoder.md v0.5 MVP 路径) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS