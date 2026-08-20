# cpptlm-v05-mvp-s3-command-pipeline: Design

> **配套**: [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/proposal.md) · [`tasks.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/tasks.md)
> **状态**: 📐 Design — 依赖 s1+s2(per Oracle 拆分) · **Owner**: CppTLM Team
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5

## 1. 架构概览(本 change)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ DGpuBoardTLM::tick()  串联 4 阶段(per Phase F-H.2)                    │
│                                                                              │
│   ┌─────────────────────────────────────────────────────────┐           │
│   │ cp_.tick()  ★ 本 change 实施                                │           │
│   │   ├─ FETCH:mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry))│           │
│   │   ├─ DECODE:Pm4Decoder(NVIDIA method packet)              │           │
│   │   │  4 method_addr ranges: 0x4000-0x40FF DISPATCH_DIRECT  │           │
│   │   │                       0x4200-0x42FF EVENT_WRITE         │           │
│   │   │                       0x4400-0x44FF RELEASE_MEM        │           │
│   │   │                       0x4500-0x45FF ACQUIRE_MEM        │           │
│   │   └─ DISPATCH: tmu_.submit(Pm4MethodDispatch)            │           │
│   └─────────────────────────────────────────────────────────┘           │
│                              │                                              │
│   ┌───────────────────────────▼──────────────────────────────────┐      │
│   │ tmu_.tick()  ★ 本 change 实施                                  │      │
│   │   ├─ submit → check_dep_latches → pre_dispatch            │      │
│   │   │     若 BACKPRESSURED(per Phase F-D.2 H5)→ CP 重试 │      │
│   │   └─ try_chain_dependent → sq_.enqueue(cta_descriptor)  │      │
│   └──────────────────────────────────────────────────────────────┘      │
│                              │                                              │
│                              ▼ (sq_.tick() + cuda_core_.tick() 在 s2 已实施)│
└─────────────────────────────────────────────────────────────────────────────┘
```

## 2. Pm4Decoder NVIDIA method packet(per Phase F-H.3 路径 3)

```cpp
struct Pm4MethodHeader {
    uint32_t inc : 1;            // bit 0 (Increment register)
    uint32_t method_addr : 15;   // bits 1-15 (32K method addresses)
    uint32_t subchannel : 4;     // bits 16-19 (NVIDIA 4-bit)
    uint32_t data_count : 4;     // bits 20-23
    uint32_t reserved : 8;       // bits 24-31
};
// 真相源:UsrLinuxEmu gpfifo_translator.h:60-73 unpackPm4Header

struct Pm4MethodDispatch {
    uint16_t method_addr;   // 4 method_addr ranges(见上)
    uint8_t subchannel_id;
    uint8_t data_count;
    // ... decoded fields: grid, block, shared_mem, args_vram_addr
};
```

## 3. CommandProcessor 5-state FSM(per Phase F-H.3 修订 + s2 骨架)

> **关键**(per Oracle ses_fe0b6e44 s2 骨架修复,2026-08-21): s2 已创建 `command_processor_mvp.hh` + `pm4_decoder_mvp.hh` 接口骨架(纯虚 `Pm4DecoderInterface`),s3 W5-6 填充实际实现(GPU VA fetch + parse_method + DECODE 实际逻辑)。

- IDLE → FETCH → DECODE → DISPATCH → COMPLETE
- **FETCH**:`mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry))` (per Phase F-C.3 H1,不是 BAR0 MMIO)
- DECODE:通过 s2 `set_decoder()` 注入的 `Pm4DecoderInterface` 调 `parse_method`(替代 parse_type3,per Phase F-H.3)
- DISPATCH:`tmu_.submit(Pm4MethodDispatch)`

## 4. TmuDispatchProcessor 反压停 fetch(per Phase F-D.2 H5)

```cpp
TmuSubmitResult TmuDispatchProcessor::submit(TmuDispatchRecord record, ...) {
    if (record.dep_enable && !check_dep_latches(record))
        return TmuSubmitResult::DEP_LATCH_MISMATCH;
    if (scheduler_cache_.size() >= max_active_tasks_) {
        backpressure_count_++;
        return TmuSubmitResult::BACKPRESSURED;  // 反压 CP,非 LIFO 驱逐
    }
    // ... 提交并通过 SubmitQueue 派发(per Phase F-H.4)
    submit_queue_.enqueue(cta_descriptor(record));
    return TmuSubmitResult::SUBMITTED;
}
```

## 5. 测试策略(per Phase F-H.3 + F-D.2 H5 + F-H.4)

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_pm4_decoder_mvp.cc` | `[pm4-decoder][mvp]` | NVIDIA method packet + 4 method_addr ranges |
| `test_command_processor_mvp.cc` | `[command-processor][mvp]` | 5 transition + GPU VA fetch |
| `test_pm4_decoder_mvp_integration.cc` | `[command-processor][pm4-decoder][integration]` | CP + Decoder 集成 |
| `test_tmu_dispatch_processor_mvp.cc` | `[tmu][mvp][glue]` | submit / 反压停 fetch / dep chain / 环检测 |

## 6. 阶段化交付(本 change)

```
s3-W5 (2026-09-19~25): Pm4Decoder + CommandProcessor + 3 测试
s3-W6 (2026-09-26~10-02): TmuDispatchProcessor + 1 测试
s3-W7-9 (2026-10-03~23): validate_topology + baseline ≥880 测试
s3-W10 (2026-10-24~30): CHANGELOG + v0.5.0-MVP tag
```

## 7. 风险与缓解(本 change)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | Pm4Decoder NVIDIA method packet 与 UsrLinuxEmu `unpackPm4Header` 比特字段不对齐 | 中 | 中 | 确认 `gpfifo_translator.h:60-73` 真相源 + 单元测试 |
| R2 | CP 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R3 | TMU 反压停 fetch 频繁触发 | 中 | 中 | 32 slot + JSON `tmu_max_active_tasks` 可配置 + BACKPRESSURED 后 CP 退避 |
| R4 | TMU dep 链式推进死循环 | 低 | 高 | 简化环检测(链深 ≤ 8);每任务 visited flag |
| R5 | 880 测试达不到 | 中 | 中 | s1+s2 已 ≥50 新增测试,baseline 850 + ≥50 = ≥900 |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 依赖 s1+s2 (per Oracle 拆分)
**下次更新**: W5 s3 启动时
