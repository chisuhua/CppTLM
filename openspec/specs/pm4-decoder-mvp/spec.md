# pm4-decoder-mvp Specification

## Purpose
TBD - created by archiving change 2026-08-21-cpptlm-v05-mvp-s3-command-pipeline. Update Purpose after archive.
## Requirements
### Requirement: pm4-decoder-data-types

The system MUST define `Pm4MethodHeader` bitfield struct (per `unpackPm4Header` semantics):
```cpp
type {
  uint32_t inc          : 1;   // bit 0
  uint32_t method_addr  : 15;  // bits 1-15
  uint32_t subchannel   : 4;   // bits 16-19
  uint32_t data_count   : 4;   // bits 20-23
  uint32_t reserved     : 8;   // bits 24-31
};
```
The system MUST define `Pm4MethodDispatch` type (替代原 `Pm4Packet`).

These types MUST live in `include/tlm/gpu/pm4_types_mvp.hh` (s2 已 create header; s3 填充详细注释).

#### Scenario: Pm4MethodHeader bit layout matches NVIDIA spec
- **WHEN** a 32-bit word `0x12345678` is parsed
- **THEN** `inc=0`, `method_addr=0x2B3C`, `subchannel=0x4`, `data_count=0x3`, `reserved=0x12`
- **AND** the bit positions match the documented layout

> **字段校验参考** (per bitfield: inc:1, method_addr:15, subchannel:4, data_count:4, reserved:8):
> - `method_addr` = `(0x12345678 >> 1) & 0x7FFF` = `0x2B3C`
> - `subchannel`  = `(0x12345678 >> 16) & 0xF`  = `0x4`

### Requirement: pm4-decoder-class

The system MUST provide a `Pm4Decoder` class in `include/tlm/gpu/pm4_decoder_mvp.hh` (s2 已 create) implementing `Pm4DecoderInterface` with:
- `parse_method(method_header, payload, max_dwords) -> Pm4MethodDispatch`
- Coverage of 4 `method_addr` ranges:
  - `0x4000-0x40FF` — DISPATCH_DIRECT
  - `0x4200-0x42FF` — EVENT_WRITE
  - `0x4400-0x44FF` — RELEASE_MEM
  - `0x4500-0x45FF` — ACQUIRE_MEM
- `set_decoder()` injection into CommandProcessor (replaces s2 no-op 骨架)

#### Scenario: DISPATCH_DIRECT (0x4000-0x40FF) parsed
- **WHEN** `parse_method(0x80004001u /* inc=1, method_addr=0x4001, subchannel=0, data_count=3, reserved=0 */, payload, 16)` is called
- **THEN** result MUST be `Pm4MethodDispatch{type=Pm4MethodType::DISPATCH_DIRECT, method_addr=0x4001, subchannel_id=0, data_count=3}`

#### Scenario: Unknown method_addr rejected
- **WHEN** `parse_method` receives a packed header word with `method_addr` not in any of the 4 ranges (e.g. `0x19999`)
- **THEN** it MUST return `Pm4MethodDispatch{type=Pm4MethodType::UNKNOWN, method_addr=0x9999, ...}`(其他字段填充为 0)(per design.md §2 — 错误通道 = `type=Pm4MethodType::UNKNOWN`,非抛异常)
- **AND** MUST NOT throw

#### Scenario: Decoder injected into CP via set_decoder
- **WHEN** `CommandProcessor::set_decoder(std::make_unique<Pm4Decoder>())` is called
- **THEN** subsequent CP `tick()` DECODE transitions MUST use the injected decoder's `parse_method`

### Requirement: pm4-decoder-tests

The system MUST provide:
- `test/test_pm4_decoder_mvp.cc` — 4 method_addr range parse correctness
- `test/test_pm4_decoder_mvp_integration.cc` — CP + Decoder 集成 PASS

Both MUST pass `ctest -R "test_pm4_decoder_mvp"`.

#### Scenario: Both pm4-decoder tests pass
- **WHEN** `ctest -R "test_pm4_decoder_mvp"` runs after s3 W5
- **THEN** both `test_pm4_decoder_mvp` (parse correctness) and `test_pm4_decoder_mvp_integration` (CP+Decoder) MUST PASS

