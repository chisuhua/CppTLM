# Spec: CommandProcessor v0.5 + Pm4Decoder

> **配套**: [`../design.md` §3.1 + §3.2](../design.md) · [`../proposal.md`](../proposal.md) · [`../tasks.md`](../tasks.md) · [`per-warp-instruction.md`](per-warp-instruction.md) · [`ptx-emu-v05-submodule.md`](ptx-emu-v05-submodule.md)
> **状态**: 📋 Spec — W2-W4 实施
> **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`ADR-X.16-cpptlm-v05-redo.md` §D5](../../../../docs/adr/ADR-X.16-cpptlm-v05-redo.md) — CppTLM CP 解析 PM4 (反 v0.4.1)

---

## 1. 范围

v0.5 redo **反转** v0.4.1 的"PM4 解析委托 host"决策,**在 CppTLM CP 模块解析 PM4**,符合真实 PCIe 设备语义:
- host driver 写入 PM4 packets 到 ring buffer(`gpu_gpfifo_entry.payload[]`)
- **CppTLM CommandProcessor 模块解析** Mesa-style TYPE3 PM4
- 替代 v0.4.1 删除的 `CommandProcessor + Pm4Decoder` 设计

## 2. PM4 TYPE3 Header(per Mesa convention)

```cpp
// include/tlm/gpu/pm4_decoder_v05.hh
struct Pm4Type3Header {
    uint32_t IT : 1;             // bit 0 (Increment Type, NOT predicate!)
    uint32_t predicate : 1;     // bit 1
    uint32_t opcode : 8;        // bits 2-9 (256 opcodes)
    uint32_t reserved : 6;      // bits 10-15
    uint32_t count : 14;        // bits 16-29
    uint32_t type : 2;          // bits 30-31 = 0b11 (TYPE3)
};
```

**位字段修正**(per Oracle 一审 C-NEW-3 + Mesa convention):
- `IT` 在 bit 0(Increment Type, NOT predicate — 这是 v0.4.1 之前的错误)
- `predicate` 在 bit 1(条件执行,与 IT 区分)
- `opcode` 在 bits 2-9(8 bits, 256 个 opcodes)
- `count` 在 bits 16-29(14 bits, 16K dwords)

## 3. Pm4Decoder API

```cpp
class Pm4Decoder {
public:
    struct Pm4Packet {
        Pm4Type3Header header;
        std::vector<uint32_t> payload;  // count dwords
        bool IT;           // auto-increment address
        bool predicate;    // conditional execute
        uint8_t opcode;    // 7-bit opcode
        uint16_t count;    // 14-bit data dwords count
        uint32_t next_addr; // auto-increment next address
    };
    
    Pm4Packet parse_type3(uint32_t header_word,
                          const uint32_t* payload,
                          size_t max_dwords);
    
    // 4 MVP opcodes 支持
    static constexpr uint8_t OP_DISPATCH_DIRECT = 0x15;
    static constexpr uint8_t OP_EVENT_WRITE     = 0x46;
    static constexpr uint8_t OP_RELEASE_MEM     = 0x49;
    static constexpr uint8_t OP_ACQUIRE_MEM     = 0x58;
};
```

**4 MVP opcodes**(per `docs/research/CP/` 12 专利 + AMD/NVIDIA 通用 PM4 集):
- `0x15 DISPATCH_DIRECT` — 直接 dispatch kernel
- `0x46 EVENT_WRITE` — 写 event 到 memory
- `0x49 RELEASE_MEM` — 释放 memory (fence signal)
- `0x58 ACQUIRE_MEM` — 获取 memory (fence wait)

其余 14 opcodes(DISPATCH_INDIRECT / DISPATCH_TASK / WAIT_REG_MEM / REG_RMW / SET_CONFIG_REG / etc.)→ **P1+ 推迟**。

## 4. CommandProcessor(5-state FSM)

```cpp
class CommandProcessor {
public:
    enum class State { IDLE, FETCH, DECODE, DISPATCH, COMPLETE };
    
    void init();
    void shutdown();
    
    // host entry:driver 写 PM4 到 gpu_gpfifo_entry,Doorbell ring
    // → CP 在 FETCH 状态读 gpfifo_entry.payload[0]
    void submit_kernel(const uint8_t* image_bytes, size_t size,
                       uint32_t grid_dim, uint32_t block_dim,
                       uint8_t stream_id, SmImageId* out_id);
    
    // EventQueue 调度
    void tick();
    
    // 状态查询
    State state() const { return state_; }
    
private:
    Pm4Decoder decoder_;
    SubchannelContext sub_ctx_[5];  // subchannel 0-4 (MVP, v3.0 同)
    CommandDispatcher dispatcher_;
    State state_ = State::IDLE;
    
    // 5 状态转换:
    // IDLE → FETCH:Doorbell wake
    // FETCH → DECODE:读 gpfifo_entry.payload[0]
    // DECODE → DISPATCH:opcode 解析成功
    // DISPATCH → COMPLETE:opcode 派发完成
};
```

### 4.1 SubchannelContext(per subchannel)

```cpp
struct SubchannelContext {
    uint32_t next_addr;             // TYPE0 IT 续地址
    bool predicate;                 // 当前 predicate 状态
    uint32_t parked_count;          // COND_EXEC 暗藏计数器
    uint8_t pending_writes;         // 待写入寄存器 buffer
};
```

### 4.2 5-state FSM 转换图

```
       ┌──────────────────────────────────────────┐
       │                                            │
       ▼                                            │
     IDLE ──(doorbell wake)──> FETCH ──> DECODE    │
                                              │    │
                                              ▼    │
                                          DISPATCH ──> COMPLETE ──> IDLE
                                              │
                                  (opcode dispatch 失败)
                                              ▼
                                          (log error) ──> IDLE
```

## 5. 与 v3.0 PtxEmuSubmodule 集成

CommandProcessor **DISPATCH** 状态调用 ComputeUnitTLM v2 的 `dispatch_blackbox` 或 `dispatch_whitebox`:

```cpp
void CommandProcessor::onDispatchOpcode(uint8_t opcode) {
    switch (opcode) {
        case Pm4Decoder::OP_DISPATCH_DIRECT: {
            // 构造 TaskEntry 并派发
            TaskEntry entry;
            entry.kernel_id = ...;
            entry.grid_dim[0] = grid_dim_;
            entry.block_dim[0] = block_dim_;
            cu_.issueTask(entry);
            state_ = State::COMPLETE;
            break;
        }
        case Pm4Decoder::OP_EVENT_WRITE: {
            // 写 event 到 memory
            event_ring_->write(...);
            state_ = State::COMPLETE;
            break;
        }
        case Pm4Decoder::OP_RELEASE_MEM: {
            // Fence signal
            fence_ring_->signal(...);
            state_ = State::COMPLETE;
            break;
        }
        case Pm4Decoder::OP_ACQUIRE_MEM: {
            // Fence wait (SEMAPHORE 阻塞)
            wait_for_fence(...);
            state_ = State::COMPLETE;
            break;
        }
    }
}
```

## 6. 测试要求

### 6.1 单元测试

```cpp
TEST_CASE("Pm4Decoder: TYPE3 header bit field parse", "[pm4-decoder][bit-field]") {
    Pm4Decoder decoder;
    uint32_t header = 0x00000003;  // type=0b11, IT=0, predicate=0, opcode=0, count=0
    auto pkt = decoder.parse_type3(header, nullptr, 0);
    REQUIRE(pkt.header.type == 0x3);
    REQUIRE(pkt.header.opcode == 0);
    REQUIRE(pkt.header.count == 0);
}

TEST_CASE("Pm4Decoder: 4 MVP opcodes parse", "[pm4-decoder][opcode]") {
    for (uint8_t op : {0x15, 0x46, 0x49, 0x58}) {
        uint32_t header = (op << 2) | 0x3;  // TYPE3 + opcode
        auto pkt = decoder.parse_type3(header, nullptr, 0);
        REQUIRE(pkt.header.opcode == op);
    }
}

TEST_CASE("CommandProcessor: 5-state FSM transitions",
          "[command-processor][fsm]") {
    CommandProcessor cp;
    cp.init();
    
    REQUIRE(cp.state() == CommandProcessor::State::IDLE);
    
    // IDLE → FETCH:Doorbell wake(通过 DGpuBar doorbell handler 触发)
    cp.simulate_doorbell_wake();
    REQUIRE(cp.state() == CommandProcessor::State::FETCH);
    
    cp.tick();  // FETCH → DECODE
    REQUIRE(cp.state() == CommandProcessor::State::DECODE);
    
    cp.tick();  // DECODE → DISPATCH
    REQUIRE(cp.state() == CommandProcessor::State::DISPATCH);
    
    cp.tick();  // DISPATCH → COMPLETE
    REQUIRE(cp.state() == CommandProcessor::State::COMPLETE);
    
    cp.tick();  // COMPLETE → IDLE
    REQUIRE(cp.state() == CommandProcessor::State::IDLE);
}

TEST_CASE("CommandProcessor: DISPATCH_DIRECT → ComputeUnitTLM dispatch",
          "[command-processor][dispatch]") {
    CommandProcessor cp;
    cp.init();
    
    cp.simulate_doorbell_wake();
    cp.tick();  // FETCH
    cp.tick();  // DECODE
    cp.tick();  // DISPATCH
    
    // ComputeUnitTLM 应已收到 TaskEntry
    REQUIRE(cp.last_dispatched().kernel_id != 0);
    REQUIRE(cp.last_dispatched().grid_dim[0] == grid_dim);
}
```

### 6.2 集成测试

```cpp
TEST_CASE("E2E: cuModuleLoadData → doorbell → CP → ComputeUnit → CQ",
          "[P3][E2E]") {
    // 全链路集成测试
    DGpuBoardTLM board("dgpu0", nullptr, MOCK_PTXEMU_V05_SO);
    board.init();
    
    // host:写 image 到 VRAM + doorbell ring
    uint8_t bytes[16] = {0xDE, 0xAD, 0xBE, 0xEF};
    auto vram_addr = board.bar().vram_base();
    std::memcpy(vram_addr, bytes, 16);
    board.doorbell().ring(0, 1);  // 1 entry in queue
    
    board.tick();
    
    // CQ 应有完成事件
    auto e = board.completion_ring().try_pop();
    REQUIRE(e.has_value());
    REQUIRE(e->status == 0);
}
```

## 7. 与 v3.0 黑盒路径差异

| 维度 | v3.0 PtxEmuSubmodule | v0.5 CommandProcessor + Pm4Decoder |
|------|---------------------|-------------------------------------|
| **PM4 解析** | host (UsrLinuxEmu) | **CppTLM CP 模块**(NEW) |
| **颗粒度** | driver → CppTLM API 调用 | driver → ring buffer → CP FETCH → DECODE |
| **延迟开销** | 一次 API call | Doorbell wake + gpfifo_entry read + DECODE + DISPATCH |
| **符合 PCIe 语义** | ❌ driver 解析(应 driver 解析 + hardware decode) | ✅ **hardware CP 解析** |

## 8. 接口稳定性

### 8.1 冻结接口(P4' 前禁止变更)

- ✅ `Pm4Type3Header` bit field layout(Mesa convention)
- ✅ 4 MVP opcodes (DISPATCH_DIRECT / EVENT_WRITE / RELEASE_MEM / ACQUIRE_MEM)
- ✅ `CommandProcessor::State` 枚举
- ✅ `CommandProcessor::submit_kernel` 签名
- ✅ `CommandProcessor::tick` 签名

### 8.2 可演进接口(P1'-P3' 期间允许调整)

- 🟡 5-state FSM 内部转换细节
- 🟡 SubchannelContext 字段扩展
- 🟡 14 deferred opcodes(P1+ 添加)

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — W2-W4 实施
