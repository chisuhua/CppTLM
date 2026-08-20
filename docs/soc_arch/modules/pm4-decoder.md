# pm4-decoder 微架构文档

> **类别**: GPU > PM4 Decoder · **状态**: 🔵 MVP 切片 (per ADR-X.17)
> **Header**: `include/tlm/gpu/pm4_decoder_mvp.hh`
> **位置**: CommandProcessor 内部组件(非独立 ChStreamModuleBase)
> **蓝图来源**: AMD PM4 spec + Mesa convention
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D5
> **关联模块**: [`command-processor.md`](./command-processor.md)
> **首版 commit**: 🔵 W5-6 实施 · **最近更新**: 2026-08-19
> **维护者**: CppTLM Team (Sisyphus)

---

## 1. 设计目标

`Pm4Decoder` 是 CommandProcessor 内部组件,负责 **解析 Mesa-style TYPE3 PM4 packet header**,提取 opcode + count + payload 字段,供 CP dispatcher 路由 handler。

**核心特性**:
- **Mesa-style TYPE3** header(per `ADR-X.16 §3.2` Oracle 一审 C-NEW-3 修正)
- **MVP 4 opcodes**:DISPATCH_DIRECT(0x15)/ EVENT_WRITE(0x46)/ RELEASE_MEM(0x49)/ ACQUIRE_MEM(0x58)
- 14 deferred opcodes 框架预留(MVP 仅 log warn)
- bit field 严格对齐 Mesa convention(`IT` bit 0 + `predicate` bit 1 + `opcode` bits 2-9 + `count` bits 16-29 + `type` bits 30-31)

---

## 2. 数据结构

### 2.1 Pm4Type3Header(Mesa convention)

```cpp
struct Pm4Type3Header {
    uint32_t IT          : 1;    // bit 0 (Increment Type, NOT predicate!)
    uint32_t predicate  : 1;    // bit 1
    uint32_t opcode     : 8;    // bits 2-9 (256 opcodes, Oracle 修正: 8 bits)
    uint32_t reserved   : 6;    // bits 10-15 (Oracle 修正: 6 bits)
    uint32_t count      : 14;   // bits 16-29 (16K dwords)
    uint32_t type       : 2;    // bits 30-31 = 0b11 (TYPE3 标志)

    Pm4Type3Header() = default;
    explicit Pm4Type3Header(uint32_t raw) {
        IT = (raw >> 0) & 0x1;
        predicate = (raw >> 1) & 0x1;
        opcode = (raw >> 2) & 0xFF;
        reserved = (raw >> 10) & 0x3F;
        count = (raw >> 16) & 0x3FFF;
        type = (raw >> 30) & 0x3;
    }

    uint32_t to_raw() const {
        return (IT & 0x1) |
               ((predicate & 0x1) << 1) |
               ((opcode & 0xFF) << 2) |
               ((reserved & 0x3F) << 10) |
               ((count & 0x3FFF) << 16) |
               ((type & 0x3) << 30);
    }

    bool is_type3() const { return type == 0b11; }
};
```

### 2.2 Pm4Opcode 枚举(MVP 4 个 + 14 预留)

```cpp
enum class Pm4Opcode : uint8_t {
    // === MVP 4 opcodes(per ADR-X.16 §3.2) ===
    DISPATCH_DIRECT  = 0x15,  // 启动 compute kernel
    EVENT_WRITE      = 0x46,  // 写 event 到 CQ
    RELEASE_MEM      = 0x49,  // 释放 memory 依赖
    ACQUIRE_MEM      = 0x58,  // 获取 memory 依赖

    // === 14 deferred opcodes(MVP 不支持,框架预留) ===
    DISPATCH_TASK    = 0x1B,  // P1+: task graph dispatch
    WAIT_REG_MEM     = 0x3C,  // P1+: wait register
    REG_RMW          = 0x21,  // P1+: register read-modify-write
    SET_CONFIG_REG   = 0x68,  // P1+: set config register
    // ... 其余 10 个(per AMD PM4 spec)
};
```

### 2.3 Pm4Packet 结构

```cpp
struct Pm4Packet {
    Pm4Type3Header header;
    Pm4Opcode opcode = Pm4Opcode::DISPATCH_DIRECT;  // 提取自 header.opcode
    uint8_t subchannel_id = 0;
    uint16_t count = 0;
    std::vector<uint32_t> payload;  // count 个 dwords
    bool valid = false;

    // 便捷访问
    uint32_t dword_at(size_t i) const {
        return i < payload.size() ? payload[i] : 0;
    }
};
```

---

## 3. 接口(Public API)

```cpp
class Pm4Decoder {
public:
    Pm4Decoder() = default;
    ~Pm4Decoder() = default;

    /// 主入口:解析 TYPE3 PM4 packet
    /// @param header 32-bit PM4 header
    /// @param payload 指向 payload 起始(至少 max_dwords 个 dwords)
    /// @param max_dwords payload buffer 最大 dwords 数
    /// @return Pm4Packet(valid=false 表示解析失败)
    Pm4Packet parse_type3(uint32_t header,
                          const uint32_t* payload,
                          size_t max_dwords);

    /// 测试用:header → Pm4Opcode 转换
    static Pm4Opcode opcode_from_header(uint32_t header) {
        return static_cast<Pm4Opcode>((header >> 2) & 0xFF);
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

    /// 校验 TYPE3 magic(bits 30-31 == 0b11)
    bool validate_type3_magic(const Pm4Type3Header& h) const {
        return h.type == 0b11;
    }

    /// 校验 opcode 是否在 MVP 支持集
    bool is_mvp_opcode(Pm4Opcode op) const {
        switch (op) {
            case Pm4Opcode::DISPATCH_DIRECT:
            case Pm4Opcode::EVENT_WRITE:
            case Pm4Opcode::RELEASE_MEM:
            case Pm4Opcode::ACQUIRE_MEM:
                return true;
            default:
                return false;
        }
    }
};
```

---

## 4. 行为流程(parse_type3)

```cpp
Pm4Packet Pm4Decoder::parse_type3(uint32_t header_raw,
                                  const uint32_t* payload,
                                  size_t max_dwords) {
    Pm4Packet packet;
    packet.header = Pm4Type3Header(header_raw);
    packet.opcode = static_cast<Pm4Opcode>(packet.header.opcode);

    // 1. 校验 TYPE3 magic
    if (!validate_type3_magic(packet.header)) {
        // 不是 TYPE3 packet,返回 invalid
        return packet;  // valid=false
    }

    // 2. 提取 count
    packet.count = packet.header.count;
    if (packet.count > max_dwords) {
        // payload buffer 不够,截断(避免 OOB)
        packet.count = static_cast<uint16_t>(max_dwords);
    }

    // 3. 复制 payload
    packet.payload.reserve(packet.count);
    for (uint16_t i = 0; i < packet.count; ++i) {
        packet.payload.push_back(payload[i]);
    }

    // 4. 提取 subchannel_id(MVP 简化:固定 0)
    packet.subchannel_id = extract_subchannel_id(packet);

    // 5. 校验 opcode 是否支持
    if (!is_mvp_opcode(packet.opcode)) {
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

**验收标准**(per ADR-X.17 G-MVP-3):
- `Pm4Type3Header::to_raw() == Pm4Type3Header(raw)` PASS
- 4 MVP opcodes 解析 PASS(DISPATCH_DIRECT/EVENT_WRITE/RELEASE_MEM/ACQUIRE_MEM)
- TYPE3 magic 校验 PASS(`type == 0b11`)
- 非 TYPE3 packet 返回 `valid=false`
- count 截断 PASS(`count > max_dwords`)

---

## 7. 实施路径(S3 W5-6)

1. 新建 `include/tlm/gpu/pm4_decoder_mvp.hh` + `src/tlm/gpu/pm4_decoder_mvp.cc`(~200 LOC)
2. 新建 `test/test_pm4_decoder_mvp.cc`(bit field + 4 opcode + 截断)
3. 集成到 `CommandProcessor`(DECODE 状态调 `parse_type3`)

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

- **2026-08-19**: 初版 — per ADR-X.17 D5 切片(MVP 4 阶段 S3)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
