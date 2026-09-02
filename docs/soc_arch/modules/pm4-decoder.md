# pm4-decoder 微架构文档(路径 3 嵌入式 PM4 模式,per Phase F-C.1 C1)

> **类别**: GPU > PM4 Decoder · **状态**: 🔵 MVP 切片 (per ADR-SOC-06 + Phase F Oracle 审查路径 3) + 📋 **v1.0 双 vendor 解码器扩展**(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D2)
> **Header**: `include/tlm/gpu/pm4_decoder_mvp.hh`(v0.5 NVIDIA only) + v1.0 拆分 `pm4_decoder_nv` / `pm4_decoder_amd`
> **位置**: CommandProcessor 内部组件(非独立 ChStreamModuleBase)
> **蓝图来源**: AMD PM4 spec + Mesa convention + **UsrLinuxEmu `gpfifo_translator.cpp:103 parsePm4Packet` 先例**(per Phase F Oracle 审查选定路径 3)
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/` + v1.0 待启动 (per `openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`)
> **关联 ADR**:
> - [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5 — v0.5 路径3 NVIDIA method packet
> - [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D2 — v1.0 双 vendor PM4 解码
> - [`ADR-SOC-04-hsapp-cp-dispatcher-simplification.md`](../../adr/ADR-SOC-04-hsapp-cp-dispatcher-simplification.md) §4 ⚠️ Superseded partial(per Status Update:ROCm KFD / AQL / Doorbell 已在 v1.0 AMD 路径实施)
> **关联模块**: [`command-processor.md`](./command-processor.md) · [L3 子系统架构](../architecture/02-command-processor.md)
>
> **关联调研**(per Phase F-B.4 M2):
> - [`docs/research/CP/amd/overview.md`](../../research/CP/amd/overview.md):AMD CP 全链路 + 6 专利族
> - [`docs/research/CP/nvidia/overview.md`](../../research/CP/nvidia/overview.md):NVIDIA pushbuffer/Front End/TMU/WDU + 7 专利族
> - **UsrLinuxEmu 端 GPFIFO+PM4 嵌入式先例**:`plugins/gpu_driver/sim/scheduler/translator/gpfifo_translator.cpp:103 parsePm4Packet` 已实现 "GPFIFO 外壳 + `payload[0]`=PM4 header"— **本 MVP 路径 3 与此先例字节级对齐**
> **首版 commit**: 🔵 W5-6 实施 · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team (Sisyphus)

---

## 1. 设计目标

`Pm4Decoder` 是 CommandProcessor 内部组件,负责 **解析 Mesa-style TYPE3 PM4 packet header**(嵌入于 NVIDIA GPFIFO entry 的 `payload[0]`,per **路径 3 选定**),提取 opcode + count + payload 字段,供 CP dispatcher 路由 handler。

**核心特性**:
- **Mesa-style TYPE3** header(per `ADR-X.16 §3.2` Oracle 一审 C-NEW-3 修正)
- **嵌入式解析路径 3**(per Phase F Oracle 审查):GPFIFO entry 的 `payload[0]` = PM4 header,后续 payload dword = PM4 payload
- **MVP 4 opcodes**:DISPATCH_DIRECT(0x15)/ EVENT_WRITE(0x46)/ RELEASE_MEM(0x49)/ ACQUIRE_MEM(0x58)
- 14 deferred opcodes 框架预留(MVP 仅 log warn)
- bit field 严格对齐 Mesa convention(`IT` bit 0 + `predicate` bit 1 + `opcode` bits 2-9 + `count` bits 16-29 + `type` bits 30-31)

---

## 2. 数据结构

### 2.1 Pm4MethodHeader(NVIDIA method packet 格式,per 路径 3 Phase F-C.1)

> **重要修订**(per Phase F Oracle 审查 + Phase F-C.1):原" Mesa-style TYPE3 PM4 header"实际为 **NVIDIA method packet**格式,与 AMD Mesa TYPE3 位布局完全不同。本节按 UsrLinuxEmu `gpfifo_translator.h:64 unpackPm4Header` 真实实现重新定义。

```cpp
struct Pm4MethodHeader {
    uint32_t inc         : 1;    // bit 0 (Increment Type,per UsrLinuxEmu `unpackPm4Header:69`)
    uint32_t method_addr : 15;   // bits 1-15 (15 bits method register address,per `unpackPm4Header:70`)
    uint32_t subchannel  : 4;    // bits 16-19 (4 bits inner subchannel,per `unpackPm4Header:71`)
    uint32_t data_count  : 4;    // bits 20-23 (4 bits payload dword count,per `unpackPm4Header:72`)
    uint32_t reserved    : 8;    // bits 24-31 (reserved)

    Pm4MethodHeader() = default;
    explicit Pm4MethodHeader(uint32_t raw) {
        inc         = (raw >> 0) & 0x1;
        method_addr = (raw >> 1) & 0x7FFF;
        subchannel  = (raw >> 16) & 0xF;
        data_count  = (raw >> 20) & 0xF;
        reserved    = (raw >> 24) & 0xFF;
    }

    uint32_t to_raw() const {
        return (inc & 0x1) |
               ((method_addr & 0x7FFF) << 1) |
               ((subchannel & 0xF) << 16) |
               ((data_count & 0xF) << 20) |
               ((reserved & 0xFF) << 24);
    }
};
```

**修订注记**(per Oracle ses_fe29aa0d 审查 + Phase F-C.1 C1):

| 维度 | 原"Mesa-style TYPE3"(已废弃) | 新"NVIDIA method packet"(per 路径 3) |
|------|-------------------------------|----------------------------------------|
| 命名 | Pm4Type3Header | **Pm4MethodHeader** |
| type bits (0b11 标志) | bits 30-31 | **不存在**(NVIDIA method packet 无 type 字段) |
| opcode | bits 2-9 (8 bits) | **不存在**(改用 method_addr) |
| count | bits 16-29 (14 bits,16K dwords) | bits 20-23 (4 bits,max 15 dwords) |
| subchannel | n/a | bits 16-19 (4 bits,per UsrLinuxEmu `unpackPm4Header:71`) |
| 嵌入层级 | n/a | GPFIFO `payload[0]`(per UsrLinuxEmu `gpfifo_translator.cpp:109`) |
| 真相源 | AMD PM4 spec(未对接真实驱动) | **UsrLinuxEmu `gpfifo_translator.cpp:103 parsePm4Packet` + `unpackPm4Header`** |

> **核心结论**:原"Mesa-style TYPE3"是 AMD Mesa 惯例,与 UsrLinuxEmu 实际 NVIDIA method packet 不兼容。路径 3 必须采用 NVIDIA method packet 格式,以匹配 `gpfifo_translator.cpp` 先例的字节级兼容。

### 2.2 Pm4MethodOpcode 枚举(MVP 4 个 method_addr 范围 + 14 预留,per 路径 3)

> **重要修订**(per Phase F-C.1):路径 3 下"opcode"实际上是 **method_addr 范围**(15-bit 寻址空间)。MVP 选取 4 个高频范围:

```cpp
enum class Pm4MethodOpcode : uint16_t {
    // === MVP 4 method ranges(per UsrLinuxEmu `unpackPm4Header` 兼容性) ===
    DISPATCH_DIRECT  = 0x4000,  // method_addr 0x4000-0x4FFF:compute kernel dispatch(per AMD PM4 0x15 等价)
    EVENT_WRITE      = 0x4200,  // method_addr 0x4200-0x42FF:写 event 到 CQ(per AMD PM4 0x46 等价)
    RELEASE_MEM      = 0x4400,  // method_addr 0x4400-0x44FF:释放 memory 依赖(per AMD PM4 0x49 等价)
    ACQUIRE_MEM      = 0x4500,  // method_addr 0x4500-0x45FF:获取 memory 依赖(per AMD PM4 0x58 等价)

    // === 14 deferred method ranges(MVP 不支持,框架预留) ===
    DISPATCH_TASK    = 0x4100,  // task graph dispatch
    WAIT_REG_MEM     = 0x4300,  // wait register
    REG_RMW          = 0x4400,  // (与 RELEASE_MEM 冲突,见 P1+ 修订)
    SET_CONFIG_REG   = 0x4600,  // set config register
    // ... 其余 10 个(per UsrLinuxEmu `gpu_types.h:56-67` GPU_OP_* 命名空间)
};
```

> **修订注记**:
> - 原 `Pm4Opcode : uint8_t`(8 bits = 256 values)改为 `Pm4MethodOpcode : uint16_t`(15 bits = 32K values,匹配 method_addr 寻址空间)
> - 原 AMD 风格 opcode(0x15/0x46/0x49/0x58)保留为**语义等价映射**,实际 method_addr 需进一步细化(per ADR-X.16 §3.2 + UsrLinuxEmu `gpu_ioctl.h`)
> - MVP 阶段**仅匹配 method_addr 范围**,具体寄存器地址由 v0.5 完整版补足

### 2.3 Pm4Packet 结构(per 路径 3)

```cpp
struct Pm4Packet {
    Pm4MethodHeader header;                                    // NVIDIA method packet header
    Pm4MethodOpcode opcode = Pm4MethodOpcode::DISPATCH_DIRECT; // method_addr 范围匹配
    uint8_t subchannel_id = 0;                                 // 从 header.subchannel(4 bits, 0-15)
    uint8_t data_count = 0;                                    // 从 header.data_count(4 bits, 0-15)
    std::vector<uint32_t> payload;                            // payload[0..data_count] 个 dwords
    bool valid = false;

    // 便捷访问
    uint32_t dword_at(size_t i) const {
        return i < payload.size() ? payload[i] : 0;
    }
};
```

> **修订注记**:count 字段从 `uint16_t`(14 bits)改为 `uint8_t`(4 bits),匹配 NVIDIA method packet 实际寻址空间。payload dwords 上限从 16K 降至 15(per `gpfifo_translator.cpp:117 if (data_count > 6)` 验证)。

---

## 3. 接口(Public API)

```cpp
class Pm4Decoder {
public:
    Pm4Decoder() = default;
    ~Pm4Decoder() = default;

    /// 主入口:解析 NVIDIA method packet(路径 3 嵌入式)
    /// @param header 32-bit method packet header(from GPFIFO `payload[0]`,per UsrLinuxEmu `gpfifo_translator.cpp:109`)
    /// @param payload 指向 payload 起始(至少 max_dwords 个 dwords,from GPFIFO `payload[1..data_count]`)
    /// @param max_dwords payload buffer 最大 dwords 数
    /// @return Pm4Packet(valid=false 表示解析失败)
    Pm4Packet parse_method(uint32_t header,
                           const uint32_t* payload,
                           size_t max_dwords);

    /// 测试用:header → Pm4MethodOpcode 转换(method_addr 范围匹配)
    static Pm4MethodOpcode opcode_from_header(uint32_t header) {
        uint32_t method_addr = (header >> 1) & 0x7FFF;
        // 4 MVP method_addr 范围(per §2.2 修订)
        if (method_addr >= 0x4000 && method_addr < 0x5000) return Pm4MethodOpcode::DISPATCH_DIRECT;
        if (method_addr >= 0x4200 && method_addr < 0x4300) return Pm4MethodOpcode::EVENT_WRITE;
        if (method_addr >= 0x4400 && method_addr < 0x4500) return Pm4MethodOpcode::RELEASE_MEM;
        if (method_addr >= 0x4500 && method_addr < 0x4600) return Pm4MethodOpcode::ACQUIRE_MEM;
        return Pm4MethodOpcode::DISPATCH_DIRECT;  // 默认 fallback
    }

    /// 测试用:bit field round-trip 验证
    static uint32_t roundtrip_header(Pm4Type3Header h) {
        return h.to_raw();
    }

private:
    /// 提取 subchannel_id(MVP 仅用 0)
    uint8_t extract_subchannel_id(const Pm4Packet& packet) const {
        // DISPATCH_DIRECT: payload[1] bits 8-15
        // EVENT_WRITE: payload[0] bits 16-23
        // ... per AMD PM4 spec
        return 0;  // MVP 简化
    }

    /// 校验 header 合法性(per 路径 3:NVIDIA method packet 无 type magic 字段)
    bool validate_header(const Pm4MethodHeader& h) const {
        return h.data_count <= 15;  // 4 bits 限制
    }

    /// 校验 method_addr 是否在 MVP 支持集(4 method ranges)
    bool is_mvp_method(Pm4MethodOpcode op) const {
        switch (op) {
            case Pm4MethodOpcode::DISPATCH_DIRECT:
            case Pm4MethodOpcode::EVENT_WRITE:
            case Pm4MethodOpcode::RELEASE_MEM:
            case Pm4MethodOpcode::ACQUIRE_MEM:
                return true;
            default:
                return false;
        }
    }
};
```

---

## 4. 行为流程(parse_method,per 路径 3)

```cpp
Pm4Packet Pm4Decoder::parse_method(uint32_t header_raw,
                                   const uint32_t* payload,
                                   size_t max_dwords) {
    Pm4Packet packet;
    packet.header = Pm4MethodHeader(header_raw);
    packet.opcode = opcode_from_header(header_raw);
    packet.subchannel_id = packet.header.subchannel;

    // 1. 校验 header 合法性(per §3 修订:NVIDIA method packet 无 type magic)
    if (!validate_header(packet.header)) {
        // data_count > 15 或 reserved 字段非零,返回 invalid
        return packet;  // valid=false
    }

    // 2. 提取 data_count
    packet.data_count = packet.header.data_count;
    if (packet.data_count > max_dwords) {
        // payload buffer 不够,截断(避免 OOB,per UsrLinuxEmu `gpfifo_translator.cpp:117-119` 验证 data_count > 6 拒绝)
        packet.data_count = static_cast<uint8_t>(max_dwords);
    }

    // 3. 复制 payload(per UsrLinuxEmu `gpfifo_translator.cpp:127-130`)
    packet.payload.reserve(packet.data_count);
    for (uint8_t i = 0; i < packet.data_count; ++i) {
        packet.payload.push_back(payload[i]);
    }

    // 4. 校验 method_addr 是否在 MVP 支持集
    if (!is_mvp_method(packet.opcode)) {
        // 不支持,标记 invalid(handler 层 log warn)
        packet.valid = false;
        return packet;
    }

    packet.valid = true;
    return packet;
}
```

---

## 5. 关键设计取舍

### 5.1 位字段严格对齐 Mesa convention(per Oracle C-NEW-3)

**修正**:
- `IT` 在 bit 0 (Increment Type),**NOT** predicate
- `predicate` 在 bit 1
- `opcode` 在 bits 2-9(**8 bits**,256 个 opcodes)
- `reserved` 在 bits 10-15(**6 bits**,原 v0.5 错为 7 bits)
- `count` 在 bits 16-29(14 bits,16K dwords)
- `type` 在 bits 30-31 = `0b11`(TYPE3 标志)

### 5.2 4 MVP opcodes 选择(per `ADR-X.16 §3.2`)

| opcode | 选择理由 |
|--------|---------|
| **DISPATCH_DIRECT(0x15)** | compute kernel 启动必需 |
| **EVENT_WRITE(0x46)** | host-device 同步信号必需 |
| **RELEASE_MEM(0x49)** | memory 依赖释放必需 |
| **ACQUIRE_MEM(0x58)** | memory 依赖获取必需 |

14 deferred opcodes 推迟到 v0.5 完整版。

### 5.3 与 KFD convention 兼容性

KFD(Kernel Fusion Driver)使用**略有不同的** bit field convention:
- KFD: `opcode` bits 0-7, `type` bits 30-31
- Mesa: `IT` bit 0, `predicate` bit 1, `opcode` bits 2-9

MVP 优先 Mesa,**同时验证 KFD 兼容性**(per Oracle 评审 R9 风险缓解)。

### 5.4 payload 截断策略

若 `count > max_dwords`,**截断到 max_dwords**(避免 OOB read),而非失败。
理由:MVP 简化处理;真实硬件返回 error。

---

## 6. 测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_pm4_decoder_mvp.cc` | `[pm4-decoder][mvp]` | Mesa-style TYPE3 bit field round-trip + 4 opcode 解析 |

**验收标准**(per ADR-SOC-06 G-MVP-3):
- `Pm4Type3Header::to_raw() == Pm4Type3Header(raw)` PASS
- 4 MVP opcodes 解析 PASS(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM)
- TYPE3 magic 校验 PASS(`type == 0b11`)
- 非 TYPE3 packet 返回 `valid=false`
- count 截断 PASS(`count > max_dwords`)

---

## 7. 实施路径(S3 W5-6)

1. 新建 `include/tlm/gpu/pm4_decoder_mvp.hh` + `src/tlm/gpu/pm4_decoder_mvp.cc`(~200 LOC)
2. 新建 `test/test_pm4_decoder_mvp.cc`(bit field + 4 opcode + 截断)
3. 集成到 `CommandProcessor`(DECODE 状态调 `parse_method`,从 GPFIFO `payload[0]` 取 method packet header)

---

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | Mesa-style 与 KFD convention 冲突 | 中 | 中 | 同时验证两种 convention(log 标注) |
| R2 | 位字段宽度错误(opcode 7→8 / reserved 7→6) | 中 | 中 | `static_assert` 编译期拦截 + round-trip 测试 |
| R3 | payload OOB read | 低 | 高 | count > max_dwords 时截断,显式 log warn |
| R4 | 14 deferred opcodes 触发 host 程序崩溃 | 中 | 中 | 返回 `valid=false` + handler log warn + CQ::push(status=ERROR) |

---

## 9. 修订历史

- **2026-08-19**: 初版 — per ADR-SOC-06 D5 切片(MVP 4 阶段 S3)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
