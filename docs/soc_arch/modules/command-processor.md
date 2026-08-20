# command-processor 微架构文档

> **类别**: GPU > Command Processor · **状态**: 🔵 MVP 切片 (per ADR-SOC-06)
> **Header**: `include/tlm/gpu/command_processor_mvp.hh`
> **位置**: DGpuBoardTLM 内部组件(非独立 ChStreamModuleBase)
> **蓝图来源**: AMD PM4 spec + Mesa convention + per ADR-X.16 §3.2 + **per Phase F-B.2 H2 修订:路径3(Oracle 推荐)嵌入式 PM4 模式**(GPFIFO 外壳 + `payload[0]`=PM4 header,与 UsrLinuxEmu `gpfifo_translator.cpp:103 parsePm4Packet` 先例对齐)
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5
> **首版 commit**: 🔵 W5-6 实施 · **最近更新**: 2026-08-19
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - DGpuBoardTLM 包装: [`dgpu-board.md`](./dgpu-board.md)
> - PM4 解析: [`pm4-decoder.md`](./pm4-decoder.md)
> - TMU Glue: [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md)
> - AMD PM4 调研: [`docs/research/CP/`](../../research/CP/) (12 专利)
>   - [`docs/research/CP/amd/overview.md`](../../research/CP/amd/overview.md):AMD CP/IB/doorbell/HQD/SPI 全链路 + 6 专利族 + 与 NVIDIA 对照
>   - [`docs/research/CP/nvidia/overview.md`](../../research/CP/nvidia/overview.md):NVIDIA pushbuffer/Front End/TMU/WDU + 7 专利族
> - **UsrLinuxEmu 端 GPFIFO 先例**:`plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp:103 parsePm4Packet` 已实现 "GPFIFO 外壳 + `payload[0]`=PM4 header" 嵌入式— **本 MVP 路径 3 与此先例字节级对齐**(per Phase F-B.4 M2)

---

## 1. 设计目标

`CommandProcessor`(CP)是 DGpuBoardTLM 内部组件,负责 **解析 NVIDIA method packet 命令**(per Phase F-H.3 路径 3,真相源 UsrLinuxEmu `gpfifo_translator.h:60-73 unpackPm4Header`),将 host 写入 ring buffer 的命令分发到对应 handler(MVP: TmuDispatchProcessor)。

**核心特性**:
- **5-state FSM**:`IDLE → FETCH → DECODE → DISPATCH → COMPLETE`
- **NVIDIA method packet** header 解析(per Phase F-H.3 路径 3,替代原 Mesa-style TYPE3)
- **MVP 4 method_addr ranges**:0x4000-0x40FF DISPATCH_DIRECT / 0x4200-0x42FF EVENT_WRITE / 0x4400-0x44FF RELEASE_MEM / 0x4500-0x45FF ACQUIRE_MEM
- **8 subchannel context**(subchannel 0-7,per UsrLinuxEmu `gpu_types.h:40` `gpu_gpfifo_entry.subchannel : 3`)— **Phase F-B.2 H2 修订**:原"5 个 per Mesa convention"为事实错误;AMD PM4 实际无 subchannel 概念(用 per-queue/ring doorbell),NVIDIA GPFIFO 是 3-bit(0-7)
- **反 v0.4.1 决策**:host driver 仅写入 ring buffer,硬件 CP 解析 NVIDIA method packet(符合真实 PCIe 设备语义)

**MVP vs v0.5 完整版简化**:
- ✅ 保留:5-state FSM + **NVIDIA method packet** + **8 subchannel**(per Phase F-B.2 H2 修订)
- ❌ 裁剪:14 deferred opcodes(MVP 仅 4 个)
- ❌ 裁剪:PREEXIT/ACQBULK **指令语义**(由 PTX-EMU 自含);**device-side 调度动作**推迟到 v0.5 完整版(per Phase F-D.1 H4 修订,纠正原"PTX-EMU 自含"混淆)— 真实 PDL 链路见 UsrLinuxEmu `sim_pdl_launch`
- ❌ 裁剪:ring buffer 完整 cycle 精度(仅延迟区间断言)
- ❌ 裁剪:CP microcode(per AMD PM4 spec §D7,推到 v0.5 完整版)

---

## 2. 架构概览

### 2.1 5-State FSM

```
    ┌─────────┐
    │  IDLE   │ ◄─────────────────────┐
    └────┬────┘                        │
         │ doorbell_wake                │
         ▼                              │
    ┌─────────┐                        │
    │  FETCH  │ 读 gpu_gpfifo_entry    │
    └────┬────� payload[0](method header) │
         │ + 解析 unpackPm4Header │
         ▼                              │
    ┌─────────┐                        │
    │ DECODE  │ Pm4Decoder::parse_method│
    └────┬────┘ 提取 method_addr + data_count    │
         │                              │
         ▼                              │
    ┌─────────┐                        │
    │ DISPATCH│ handler 路由:          │
    └────┬────┘ - DISPATCH_DIRECT → TMU│
         │ - EVENT_WRITE → CQ::push    │
         │ - RELEASE_MEM/ACQUIRE_MEM   │
         │   → tmu.try_chain_dependent │
         ▼                              │
    ┌─────────┐                        │
    │COMPLETE │ advance to next entry  │
    └─────────┘ ──────────────────────►┘
```

### 2.2 内部数据流

```
Doorbell ring (write_reg 0x1000+stream_id)
    │
    ▼ CP.wake()
CP.tick()
    │
    ├─ FETCH: dgpu_bar_.read_reg(CP_FETCH_OFFSET) → header
    │       payload = dgpu_bar_.read_payload(count dwords)
    │
    ├─ DECODE: pm4_decoder_.parse_method(method_header, payload, max_dwords)
    │       → Pm4MethodDispatch { method_addr, subchannel_id, data_count, decoded_fields }
    │
    ├─ DISPATCH (per method_addr range,per Phase F-H.3):
    │   - 0x4000-0x40FF DISPATCH_DIRECT:
    │       tmu_.submit(TmuDispatchRecord{
    │           grid_dim, block_dim, args_vram_addr, kernel_id, ...
    │       })
    │   - 0x4200-0x42FF EVENT_WRITE:
    │       cq_.push(event_id, status)
    │   - 0x4400-0x44FF RELEASE_MEM:
    │       tmu_.try_chain_dependent(record)
    │   - 0x4500-0x45FF ACQUIRE_MEM:
    │       tmu_.check_dependencies(record)
    │
    └─ COMPLETE: cp_state_.next_entry()
```

---

## 3. 接口(Public API)

```cpp
class CommandProcessor {
public:
    /// 状态枚举(per Mesa convention + ADR-X.16 §3.2)
    enum class State { IDLE, FETCH, DECODE, DISPATCH, COMPLETE };

    /// 5-state FSM 状态查询
    State state() const { return state_; }

    /// Wake from IDLE(由 Doorbell ring 触发)
    void wake();

    /// Per-tick 推进(由 DGpuBoardTLM::tick() 调用)
    void tick();

    /// 测试/统计接口
    uint64_t packets_decoded() const { return packets_decoded_; }
    uint64_t dispatch_direct_count() const { return dispatch_direct_count_; }
    uint64_t event_write_count() const { return event_write_count_; }

    /// JSON params 注入(可选)
    void set_max_dwords_per_packet(uint32_t n) { max_dwords_per_packet_ = n; }

private:
    // === 5-state FSM 状态 ===
    State state_ = State::IDLE;
    uint32_t current_entry_index_ = 0;
    uint32_t current_dword_count_ = 0;

    // === Subchannel context[8](per Phase F-B.2 H2 修订:NVIDIA GPFIFO 3-bit = 8 subchannels) ===
    struct SubchannelContext {
        uint64_t ring_base_addr = 0;
        uint64_t ring_size = 0;
        uint64_t wptr = 0;
        uint64_t rptr = 0;
    };
    std::array<SubchannelContext, 8> sub_ctx_;

    // === PM4 解析(per Phase F-H.3 修订:NVIDIA method packet 替代 Mesa-style TYPE3) ===
    //   per s2 T-s2-3a / s3 T-s3-1 契约:decoder 注入,非直接成员
    std::unique_ptr<Pm4DecoderInterface> decoder_;  // s2 注入,s3 填充实现
    uint32_t max_dwords_per_packet_ = 64;  // MVP 简化

    // === 依赖注入 ===
    DGpuBar& bar_;
    TmuDispatchProcessor& tmu_;       // 输出 Pm4MethodDispatch → TMU
    CompletionRing& cq_;

    // === 统计 ===
    uint64_t packets_decoded_ = 0;
    uint64_t dispatch_direct_count_ = 0;
    uint64_t event_write_count_ = 0;
    uint64_t release_mem_count_ = 0;
    uint64_t acquire_mem_count_ = 0;

    // === 内部方法(per Phase F-H.3 修订) ===
    Pm4MethodDispatch fetch_packet();         // 返回 method_addr + decoded fields
    void dispatch_packet(const Pm4MethodDispatch& packet);
    void handle_dispatch_direct(const Pm4MethodDispatch& packet);
    void handle_event_write(const Pm4MethodDispatch& packet);
    void handle_release_mem(const Pm4MethodDispatch& packet);
    void handle_acquire_mem(const Pm4MethodDispatch& packet);
};
```

---

## 4. 行为流程

### 4.1 tick() 4 阶段

```cpp
void CommandProcessor::tick() {
    switch (state_) {
        case State::IDLE:
            // 等 doorbell wake(由 Doorbell::ring 触发 state_ = FETCH)
            break;

        case State::FETCH:
            current_packet_ = fetch_packet();
            state_ = State::DECODE;
            break;

        case State::DECODE:
            // per Phase F-H.3 修订:NVIDIA method packet 解码(替代 Mesa-style TYPE3)
            decoder_.parse_method(current_packet_.method_header,
                                  current_packet_.payload.data(),
                                  current_packet_.data_count);
            state_ = State::DISPATCH;
            packets_decoded_++;
            break;

        case State::DISPATCH:
            dispatch_packet(current_packet_);
            state_ = State::COMPLETE;
            break;

        case State::COMPLETE:
            current_entry_index_++;
            if (more_entries_in_ring()) {
                state_ = State::FETCH;
            } else {
                state_ = State::IDLE;
            }
            break;
    }
}
```

### 4.2 fetch_packet()(per 路径 3 修订:GPU VA 内存读 + 真实 sizeof 步进)

```cpp
Pm4MethodDispatch CommandProcessor::fetch_packet() {
    Pm4MethodDispatch packet;

    // 注(per Phase F-C.3 H1 修订):CP 从 **GPU VA 内存** 读 GPFIFO entries,
    //    不是从 BAR0 MMIO 读(per UsrLinuxEmu `hardware_puller_emu.cpp:82-83` +
    //    `fetchEntry(entry)` + Oracle ses_fe29aa0d 审查)
    //    ring buffer 在设备内存(VRAM),BAR0 MMIO 只放 doorbell

    // 步进 = 真实 sizeof(gpu_gpfifo_entry) packed ≈ 109 字节
    constexpr size_t ENTRY_STRIDE = sizeof(gpu_gpfifo_entry);  // ≈ 109B
    uint64_t entry_addr = ring_base_addr_ + current_entry_index_ * ENTRY_STRIDE;

    // 读 entry header(bitfield + format)
    gpu_gpfifo_entry entry;
    mem_read_vram(entry_addr, &entry, sizeof(entry));

    // 路径 3(per Phase F-C.1 + Phase F-H.3):FORMAT_PM4 时,从 payload[0] 取 **NVIDIA method packet header**
    if (entry.format == FORMAT_PM4) {
        uint32_t method_header = static_cast<uint32_t>(entry.payload[0]);
        if (method_header == 0) return packet;  // invalid, per UsrLinuxEmu `gpfifo_translator.cpp:105`

        // 调 Pm4Decoder::parse_method(从 payload[1..data_count] 取 method payload)
        // per Phase F-H.3 + FIX-C2:NVIDIA method packet 格式(per UsrLinuxEmu `unpackPm4Header`):
        //   inc = header & 1 (bit 0)
        //   method_addr = (header >> 1) & 0x7FFF (bits 1-15)
        //   subchannel = (header >> 16) & 0xF (bits 16-19)
        //   data_count = (header >> 20) & 0xF (bits 20-23)
        //   WARNING:原代码写 method_addr=h&0x7FFF(含inc位) + data_count=h>>24(错位),
        //   经 Oracle ses_fe179d02 审查纠正(2026-08-20)
        packet = pm4_decoder_.parse_method(method_header,
                                            &entry.payload[1],
                                            max_dwords_per_packet_);
        packet.subchannel_id = static_cast<uint8_t>((method_header >> 16) & 0xF);
        packet.method_addr = static_cast<uint16_t>((method_header >> 1) & 0x7FFF);  // FIX-C2:>>1
        packet.data_count = static_cast<uint8_t>((method_header >> 20) & 0xF);      // FIX-C2:>>20
    } else {
        // 非 FORMAT_PM4 entry:原生 GPFIFO 方法(per UsrLinuxEmu `gpu_types.h:56-67` GPU_OP_*)
        // MVP 阶段仅支持 FORMAT_PM4;其他格式视为 invalid
        packet.valid = false;
        return packet;
    }

    return packet;
}
```

> **修订注记**(per Phase F-C.3 H1 + Oracle ses_fe29aa0d 审查):
> 1. **fetch 源位置**:`mem_read_vram(GPU_VA)` 替代 `bar_.read_reg(BAR0 MMIO)` — 真实硬件与 UsrLinuxEmu sim 均从设备内存拉 ring,BAR0 仅放 doorbell
> 2. **步进大小**:`sizeof(gpu_gpfifo_entry)` packed ≈ 109B 替代原"16 字节" — 原值是 MVP 简化错误,真实结构详见 UsrLinuxEmu `plugins/gpu_driver/shared/gpu_types.h:36-53`
> 3. **FORMAT 字段**:`gpu_gpfifo_entry` 含 `format` 字段(per `gpu_ioctl.h`),路径 3 仅处理 `FORMAT_PM4`,其他 format 视为 invalid
> 4. **追溯锚点**:UsrLinuxEmu `gpu_types.h:36-53` + `gpu_ioctl.h:723-779` + `gpfifo_translator.cpp:103-140`

### 4.3 dispatch_packet()(per Phase F-H.3:NVIDIA method_addr ranges 替代 Pm4Opcode)

```cpp
void CommandProcessor::dispatch_packet(const Pm4MethodDispatch& packet) {
    // NVIDIA method_addr 范围判定(per Pm4MethodOpcode 枚举):
    switch (packet.method_addr) {
        case Pm4MethodOpcode::DISPATCH_DIRECT:  // 0x4000-0x40FF range
            handle_dispatch_direct(packet);
            dispatch_direct_count_++;
            // → tmu_.submit(dispatch_packet)
            break;

        case Pm4MethodOpcode::EVENT_WRITE:  // 0x4200-0x42FF range
            handle_event_write(packet);
            event_write_count_++;
            // → cq_.push(event_id)
            break;

        case Pm4MethodOpcode::RELEASE_MEM:  // 0x4400-0x44FF range
            handle_release_mem(packet);
            release_mem_count_++;
            // → cq_.push(mem_release_status)
            break;

        case Pm4MethodOpcode::ACQUIRE_MEM:  // 0x4500-0x45FF range
            handle_acquire_mem(packet);
            acquire_mem_count_++;
            // → cq_.push(mem_acquire_status)
            break;

        default:
            // 14 deferred method_addr ranges(MVP 不支持,log warn)
            log_warn("CommandProcessor: unsupported method_addr 0x%04x", packet.method_addr);
            break;
    }
}
```

---

## 5. 关键设计取舍

### 5.1 反 v0.4.1 决策:host driver 仅写 ring buffer

per ADR-X.16 D5:
- **真实 PCIe 设备语义**: driver 写入 ring buffer,**硬件 CP 解析 PM4**
- v0.4.1 设计删除 CP(让 host 解析),**反 PCIe 设备语义**
- MVP 重新启用 CP 模块,符合真实硬件

### 5.2 NVIDIA method packet header(per Phase F-H.3 修订,替代 Mesa-style TYPE3)

per UsrLinuxEmu `plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.h:60-73` `unpackPm4Header`:
```cpp
struct Pm4MethodHeader {
    uint32_t inc        : 1;    // bit 0 (Increment register;1=write-then-inc,0=write-only)
    uint32_t method_addr: 15;   // bits 1-15 (32K method addresses)
    uint32_t subchannel : 4;    // bits 16-19 (NVIDIA GPFIFO 4-bit = 16 subchannels)
    uint32_t data_count : 4;    // bits 20-23 (up to 15 dwords payload)
    uint32_t reserved   : 8;    // bits 24-31
};
```

**修订要点**(per Phase F-H.3):
- **MVP MVP method_addr 范围**(per Pm4MethodOpcode):
  - `0x4000-0x40FF`:DISPATCH_DIRECT(per Hopper PM4 spec)
  - `0x4200-0x42FF`:EVENT_WRITE
  - `0x4400-0x44FF`:RELEASE_MEM
  - `0x4500-0x45FF`:ACQUIRE_MEM
- **subchannel**:`unpackPm4Header` 是 4-bit(per UsrLinuxEmu 真实),但 `gpu_gpfifo_entry.subchannel` 是 3-bit(per `gpu_types.h:40`)= 8 subchannels(MVP 实际使用上限);Phase F-H.3 与 Phase F-B.2 协调
- **与 Mesa-style TYPE3 不兼容**:Mesa 用 `type=0b11` 标识 TYPE3 header,MVP 路径 3 用 NVIDIA 字段布局;**两种格式字节级不兼容**,MVP 路径 3 仅支持 NVIDIA

### 5.3 8 subchannel context(per Phase F-B.2 H2 修订)

per UsrLinuxEmu `gpu_types.h:40` `gpu_gpfifo_entry.subchannel : 3`(NVIDIA GPFIFO 惯例),每个 subchannel 独立 ring buffer(AMD PM4 不使用此概念,仅列作对比参考):
- subchannel 0-3: graphics / compute / DMA / VCN(per Mesa 命名约定,**仅作参考**)
- subchannel 4-7: reserved(per NVIDIA GPFIFO 3-bit 全 8 范围)

MVP 仅用 subchannel 0 (compute),其他 7 个预留。

> **修订注记**(per Phase F-B.2 H2 + Oracle ses_fe29aa0d 审查):
> 原文档"5 个 subchannel per Mesa convention"为事实错误,**两头不靠**:
> - NVIDIA GPFIFO 是 3-bit(8 subchannels),UsrLinuxEmu 真实采用
> - AMD PM4 无 subchannel 概念(用 per-queue doorbell)
> - "Mesa convention"无权威定义(grep `docs/research/CP/{amd,nvidia}/overview.md` 中 "subchannel" 零命中)

### 5.4 状态转换正确性

5-state FSM 转换必须严格:
- `IDLE → FETCH`: 仅 doorbell wake
- `FETCH → DECODE`: 仅当 `entry.format == FORMAT_PM4` 且 `method_header != 0`(per `gpfifo_translator.cpp:105`)
- `DECODE → DISPATCH`: 仅当 Pm4Decoder::parse_method 返回有效 Pm4MethodDispatch
- `DISPATCH → COMPLETE`: 仅当 handler 成功返回
- `COMPLETE → IDLE|FETCH`: ring 空回 IDLE,否则回 FETCH

---

## 6. 测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_command_processor_mvp.cc` | `[command-processor][mvp]` | 5-state FSM 转换测试 + 4 opcode 路径测试 |
| `test_pm4_decoder_mvp_integration.cc` | `[command-processor][pm4-decoder][integration]` | CP + Pm4Decoder 集成测试 |

**验收标准**(per ADR-SOC-06 G-MVP-3):
- 5 transition 测试 PASS(每个 FSM 转换)
- 4 method_addr range 路径 PASS(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM)
- **NVIDIA method packet** bit field round-trip PASS(`inc/method_addr/subchannel/data_count` per `unpackPm4Header`)

---

## 7. 实施路径(s2 骨架 + s3 填充,per Phase L split)

**s2 W3-4 创建骨架**(per s2 T-s2-3a):
1. 新建 `include/tlm/gpu/pm4_types_mvp.hh`(Pm4MethodHeader/Pm4MethodDispatch 数据类型)
2. 新建 `include/tlm/gpu/pm4_decoder_mvp.hh`(Pm4DecoderInterface 纯接口,含 `parse_method` 纯虚)
3. 新建 `include/tlm/gpu/command_processor_mvp.hh` + `src/tlm/gpu/command_processor_mvp.cc`(5-state FSM 骨架,`set_decoder(unique_ptr<Pm4DecoderInterface>)` 注入接口,~250 LOC)
4. 引用 `include/tlm/gpu/dgpu_bar.hh`
5. 新建 `test/test_command_processor_mvp_skeleton.cc`(CP 状态机 no-op + wake 测试)

**s3 W5-6 填充实现**(per s3 T-s3-1/2):
6. 填充 `src/tlm/gpu/pm4_decoder_mvp.cc`(新增 Pm4Decoder 具体类继承 Pm4DecoderInterface,4 method_addr ranges NVIDIA method packet 解析)
7. 填充 `src/tlm/gpu/command_processor_mvp.cc` 的 DECODE 状态(调 `decoder_->parse_method()`)
8. 新建 `test/test_command_processor_mvp.cc`(5 transition + NVIDIA method packet decode 真实测试)
9. 新建 `test/test_pm4_decoder_mvp_integration.cc`(CP + Decoder 集成)

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | NVIDIA method packet 与 AMD PM4 TYPE3 字节级不兼容(per Phase F-H.3) | 中 | 中 | MVP 路径 3 仅支持 NVIDIA;AMD PM4 推迟到 v0.5 完整版 |
| R2 | 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R3 | 14 deferred method_addr ranges 缺失触发 host 程序崩溃 | 中 | 中 | log warn + 错误传播 + CQ::push(status=ERROR) |
| R4 | Subchannel context 切换错误 | 低 | 中 | MVP 仅用 subchannel 0,其他预留 |
| R5 | ring buffer 空触发状态机卡死 | 低 | 高 | COMPLETE 状态显式检查 `more_entries_in_ring()` |

---

## 9. 修订历史

- **2026-08-19**: 初版 — per ADR-SOC-06 D5 切片(MVP 4 阶段 S3)
- **2026-08-20**: Phase F-C.1/C.2/C.3/B.2/B.4/B.3 修订(路径 3 NVIDIA method packet + GPU VA fetch + 8 subchannels + 0x28 stub 语义)
- **2026-08-20**: **Phase F-H.3 修订**:**完全切换至 NVIDIA method packet 格式家族** — `Pm4Packet/Pm4Opcode` 替换为 `Pm4MethodDispatch/Pm4MethodOpcode`(uint16_t method_addr 范围);**删除 Mesa-style TYPE3 header 段落**;`dispatch_packet` 4 cases 改为 4 method_addr range;`parse_type3` 替换为 `parse_method`。**CP 输出统一为 Pm4MethodDispatch packet → TMU**(per `tmu-dispatch-processor.md` §F-H.4 修订)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-20*
