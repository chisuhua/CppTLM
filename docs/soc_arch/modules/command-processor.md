# command-processor 微架构文档

> **类别**: GPU > Command Processor · **状态**: 🔵 MVP 切片 (per ADR-X.17)
> **Header**: `include/tlm/gpu/command_processor_mvp.hh`
> **位置**: DGpuBoardTLM 内部组件(非独立 ChStreamModuleBase)
> **蓝图来源**: AMD PM4 spec + Mesa convention + per ADR-X.16 §3.2
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D5
> **首版 commit**: 🔵 W5-6 实施 · **最近更新**: 2026-08-19
> **维护者**: CppTLM Team (Sisyphus)

> **关联文档**:
> - 索引: [README.md](./README.md)
> - DGpuBoardTLM 包装: [`dgpu-board.md`](./dgpu-board.md)
> - PM4 解析: [`pm4-decoder.md`](./pm4-decoder.md)
> - TMU Glue: [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md)
> - AMD PM4 调研: [`docs/research/CP/`](../../research/CP/) (12 专利)

---

## 1. 设计目标

`CommandProcessor`(CP)是 DGpuBoardTLM 内部组件,负责 **解析 PM4 (Packet Manager 4) 命令**(Mesa-style TYPE3 header),将 host 写入 ring buffer 的命令分发到对应 handler(MVP: TmuDispatchProcessor)。

**核心特性**:
- **5-state FSM**:`IDLE → FETCH → DECODE → DISPATCH → COMPLETE`
- **Mesa-style TYPE3** header 解析(per ADR-X.16 §3.2 位字段修正)
- **MVP 4 opcodes**:DISPATCH_DIRECT(0x15)/ EVENT_WRITE(0x46)/ RELEASE_MEM(0x49)/ ACQUIRE_MEM(0x58)
- **5 subchannel context**(subchannel 0-4,per Mesa convention)
- **反 v0.4.1 决策**:host driver 仅写入 ring buffer,硬件 CP 解析 PM4(符合真实 PCIe 设备语义)

**MVP vs v0.5 完整版简化**:
- ✅ 保留:5-state FSM + Mesa TYPE3 + 5 subchannel
- ❌ 裁剪:14 deferred opcodes(MVP 仅 4 个)
- ❌ 裁剪:PREEXIT/ACQBULK 指令仿真(由 PTX-EMU 自含)
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
    └────┬────� payload[0](PM4 header) │
         │ + 解析 header.type == TYPE3 │
         ▼                              │
    ┌─────────┐                        │
    │ DECODE  │ Pm4Decoder::parse_type3│
    └────┬────┘ 提取 opcode + count    │
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
    ├─ DECODE: pm4_decoder_.parse_type3(header, payload, max_dwords)
    │       → Pm4Packet { opcode, subchannel_id, count, payload }
    │
    ├─ DISPATCH (per opcode):
    │   - DISPATCH_DIRECT(0x15):
    │       tmu_.submit(TmuDispatchRecord{
    │           grid_dim, block_dim, args_vram_addr, kernel_id, ...
    │       })
    │   - EVENT_WRITE(0x46):
    │       cq_.push(event_id, status)
    │   - RELEASE_MEM(0x49):
    │       tmu_.try_chain_dependent(record)
    │   - ACQUIRE_MEM(0x58):
    │       tmu_.check_dependencies(record)
    │
    └─ COMPLETE: cp_state_.next_entry()
```

---

## 3. 接口(Public API)

```cpp
class CommandProcessor {
public:
    /// 状态枚举(per Mesa convention)
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

    // === Subchannel context[5](per Mesa convention) ===
    struct SubchannelContext {
        uint64_t ring_base_addr = 0;
        uint64_t ring_size = 0;
        uint64_t wptr = 0;
        uint64_t rptr = 0;
    };
    std::array<SubchannelContext, 5> sub_ctx_;

    // === PM4 解析 ===
    Pm4Decoder decoder_;
    uint32_t max_dwords_per_packet_ = 64;  // MVP 简化

    // === 依赖注入 ===
    DGpuBar& bar_;
    TmuDispatchProcessor& tmu_;
    CompletionRing& cq_;

    // === 统计 ===
    uint64_t packets_decoded_ = 0;
    uint64_t dispatch_direct_count_ = 0;
    uint64_t event_write_count_ = 0;
    uint64_t release_mem_count_ = 0;
    uint64_t acquire_mem_count_ = 0;

    // === 内部方法 ===
    Pm4Packet fetch_packet();
    void dispatch_packet(const Pm4Packet& packet);
    void handle_dispatch_direct(const Pm4Packet& packet);
    void handle_event_write(const Pm4Packet& packet);
    void handle_release_mem(const Pm4Packet& packet);
    void handle_acquire_mem(const Pm4Packet& packet);
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
            decoder_.parse_type3(current_packet_.header,
                                  current_packet_.payload.data(),
                                  current_packet_.count);
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

### 4.2 fetch_packet()

```cpp
Pm4Packet CommandProcessor::fetch_packet() {
    Pm4Packet packet;

    // 读 gpu_gpfifo_entry.payload[0](PM4 header)
    // 注:每 gpfifo_entry = 16 字节(per UsrLinuxEmu ADR-057 D2 v2 修订:76→84 字节;
    //     头部 4 字节(PM4 header)+ payload[0] 起点 4 字节 + 后续 dword 步进 4 字节)
    //     MVP 简化:每 entry 固定 16 字节(header + 起始 payload dword)
    uint32_t header = bar_.read_reg(CP_HEADER_OFFSET + current_entry_index_ * 16);
    packet.header = header;
    packet.count = (header >> 16) & 0x3FFF;  // bits 16-29

    // 读 payload(count dwords)
    for (uint32_t i = 0; i < packet.count && i < max_dwords_per_packet_; ++i) {
        packet.payload.push_back(
            bar_.read_reg(CP_HEADER_OFFSET + current_entry_index_ * 16 + 4 + i * 4)
        );
    }

    return packet;
}
```

> **gpfifo_entry size 来源**:
> - **真实硬件**: AMD gpfifo_entry = 8 dword(32 字节),前 4 dword 是 PM4 header
> - **UsrLinuxEmu `gpu_gpfifo_entry`**: per [UsrLinuxEmu ADR-057](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-057-cp-profiling-hooks-timestamp.md) D2 v2 修订后 = 84 字节(原 76 字节 + 8 字节 timestamp 扩展);MVP 简化采用 16 字节步进假设(header 4B + 起始 payload 4B + 步进 4B × 后续)
> - **v0.5 完整版**:对齐 UsrLinuxEmu 真实 84 字节结构,fetch_packet() 引入 `payload_offset_within_entry` 字段
> - **追溯锚点**: UsrLinuxEmu ADR-057 §D3 方案 2 + `include/gpu_gpfifo_entry` 真实定义

### 4.3 dispatch_packet()(MVP 4 opcodes)

```cpp
void CommandProcessor::dispatch_packet(const Pm4Packet& packet) {
    switch (packet.opcode) {
        case Pm4Opcode::DISPATCH_DIRECT:  // 0x15
            handle_dispatch_direct(packet);
            dispatch_direct_count_++;
            break;

        case Pm4Opcode::EVENT_WRITE:  // 0x46
            handle_event_write(packet);
            event_write_count_++;
            break;

        case Pm4Opcode::RELEASE_MEM:  // 0x49
            handle_release_mem(packet);
            release_mem_count_++;
            break;

        case Pm4Opcode::ACQUIRE_MEM:  // 0x58
            handle_acquire_mem(packet);
            acquire_mem_count_++;
            break;

        default:
            // 14 deferred opcodes(MVP 不支持,log warn)
            log_warn("CommandProcessor: unsupported opcode 0x%02x", packet.opcode);
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

### 5.2 Mesa-style TYPE3 header(per Oracle C-NEW-3 修正)

per `ADR-X.16 §3.2`:
```cpp
struct Pm4Type3Header {
    uint32_t IT          : 1;    // bit 0 (Increment Type)
    uint32_t predicate  : 1;    // bit 1
    uint32_t opcode     : 8;    // bits 2-9 (256 opcodes)
    uint32_t reserved   : 6;    // bits 10-15
    uint32_t count      : 14;   // bits 16-29 (16K dwords)
    uint32_t type       : 2;    // bits 30-31 = 0b11 (TYPE3 标志)
};
```

**注意**:v0.5 修正了 `opcode` 字段宽度从 7 bits → 8 bits,`reserved` 从 7 bits → 6 bits(per Oracle 评审)。

### 5.3 5 subchannel context

per Mesa convention,每个 subchannel 独立 ring buffer:
- subchannel 0-3: graphics / compute / DMA / VCN
- subchannel 4: reserved

MVP 仅用 subchannel 0 (compute),其他 4 个预留。

### 5.4 状态转换正确性

5-state FSM 转换必须严格:
- `IDLE → FETCH`: 仅 doorbell wake
- `FETCH → DECODE`: 仅当 header.type == TYPE3(否则回 IDLE + log error)
- `DECODE → DISPATCH`: 仅当 Pm4Decoder 返回有效 Pm4Packet
- `DISPATCH → COMPLETE`: 仅当 handler 成功返回
- `COMPLETE → IDLE|FETCH`: ring 空回 IDLE,否则回 FETCH

---

## 6. 测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_command_processor_mvp.cc` | `[command-processor][mvp]` | 5-state FSM 转换测试 + 4 opcode 路径测试 |
| `test_pm4_decoder_mvp_integration.cc` | `[command-processor][pm4-decoder][integration]` | CP + Pm4Decoder 集成测试 |

**验收标准**(per ADR-X.17 G-MVP-3):
- 5 transition 测试 PASS(每个 FSM 转换)
- 4 opcode 路径 PASS(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM)
- Mesa-style TYPE3 bit field round-trip PASS

---

## 7. 实施路径(S3 W5-6)

1. 新建 `include/tlm/gpu/command_processor_mvp.hh` + `src/tlm/gpu/command_processor_mvp.cc`(~250 LOC)
2. 引用 `include/tlm/gpu/pm4_decoder_mvp.hh` + `tmu_dispatch_processor_mvp.hh` + `dgpu_bar.hh`
3. 新建 `test/test_command_processor_mvp.cc`(5 transition + 4 opcode)
4. 新建 `test/test_pm4_decoder_mvp_integration.cc`(CP + Decoder 集成)
5. 更新 `include/chstream_register.hh`(若 CP 暴露为独立 module;MVP 不暴露)

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | Mesa-style TYPE3 bit field 与 KFD 约定不同 | 中 | 中 | 同时验证 Mesa + KFD 两种 convention |
| R2 | 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R3 | 14 deferred opcodes 缺失触发 host 程序崩溃 | 中 | 中 | log warn + 错误传播 + CQ::push(status=ERROR) |
| R4 | Subchannel context 切换错误 | 低 | 中 | MVP 仅用 subchannel 0,其他预留 |
| R5 | ring buffer 空触发状态机卡死 | 低 | 高 | COMPLETE 状态显式检查 `more_entries_in_ring()` |

---

## 9. 修订历史

- **2026-08-19**: 初版 — per ADR-X.17 D5 切片(MVP 4 阶段 S3)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
