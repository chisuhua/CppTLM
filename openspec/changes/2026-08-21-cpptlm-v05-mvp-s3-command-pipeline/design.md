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
│                              ▼ (sq_.tick() s2 已实施 + cuda_core_.tick() s1 CudaCoreAdapterMVP 已实施,s2 仅消费不实施)│
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

## 4. TmuDispatchProcessor 反压停 fetch(per Phase F-D.2 H5 + Oracle M4)

### 4.1 反压传播 4 路径(per Oracle M4 细化)

```
┌────────────────────────────────────────────────────────────────────┐
│ TMU.submit(record)                                                  │
│   │                                                                │
│   ├─① check_dep_latches                                            │
│   │     FAIL → return DEP_LATCH_MISMATCH                            │
│   │     (CP 不重试,记录 dep_latch_mismatch_count_)                  │
│   │                                                                │
│   ├─② scheduler_cache.size() ≥ max_active_tasks_?                   │
│   │     YES → return BACKPRESSURED                                  │
│   │     (CP 退避,记录 backpressure_count_)                          │
│   │                                                                │
│   ├─③ handler_->on_dispatch(record) → TmuHandlerResult              │
│   │     handler 内部调 sq_.enqueue(cta_desc)                        │
│   │                                                                │
│   │     SQ 满(pending FIFO 32 满) → handler 探测到 SQ.enqueue=false │
│   │     → handler 返回 SQ_REJECTED → TMU 记录 sq_rejected_count_    │
│   │     → TMU 撤销 record 注册 → return SUBMIT_QUEUE_REJECTED        │
│   │     (CP 退避同 BACKPRESSURED,语义区分便于 metrics)              │
│   │                                                                │
│   │     SQ 成功 → handler 返回 HANDLED → TMU 注册 record 到          │
│   │     scheduler_cache_ → return SUBMITTED                         │
│   │                                                                │
│   └─ (其他内部错误 → return INTERNAL_ERROR)                         │
└────────────────────────────────────────────────────────────────────┘
```

### 4.2 Handler 接口扩展(per Oracle M4)

```cpp
// s2 原始接口(仅单向调用):
class TmuHandlerInterface {
public:
    virtual ~TmuHandlerInterface() = default;
    virtual void on_dispatch(const TmuDispatchRecord& record) = 0;
};

// s3 扩展为返回 result(handler 探测 SQ 失败需上报):
class TmuHandlerInterface {
public:
    virtual ~TmuHandlerInterface() = default;
    virtual TmuHandlerResult on_dispatch(const TmuDispatchRecord& record) = 0;
};

enum class TmuHandlerResult {
    HANDLED,              // SQ.enqueue 成功
    SQ_REJECTED,          // SQ 满(handler 调 sq_.enqueue 返回 false)
    INVALID_RECORD,       // record 字段非法(防御性)
};
```

### 4.3 TMU → SQ 派发:handler 实现示例(s3 W6 实施)

```cpp
class S3SubmitQueueHandler : public TmuHandlerInterface {
public:
    explicit S3SubmitQueueHandler(SubmitQueue& sq) : sq_(sq) {}

    TmuHandlerResult on_dispatch(const TmuDispatchRecord& record) override {
        CtaDescriptor cta_desc = build_cta_descriptor(record);
        if (sq_.enqueue(cta_desc)) {       // SQ 内部:32 满 → 返回 false
            return TmuHandlerResult::HANDLED;
        }
        return TmuHandlerResult::SQ_REJECTED;
    }

private:
    SubmitQueue& sq_;
};

// TMU 注入方式(s3 W6 T-s3-3):
tmu_.set_handler(std::make_unique<S3SubmitQueueHandler>(sq_));
```

### 4.4 CP 退避策略(per Oracle M4)

CP 收到 `BACKPRESSURED` / `SUBMIT_QUEUE_REJECTED` 时:
- **不要立即重试** — 引入最小退避窗口(per Phase F-D.2 H5:`MIN_BACKOFF_CYCLES = 8`)
- 状态保持在 DECODE,DISPATCH 失败时**回到 FETCH 而不是 IDLE**
- 失败计数累计到 `cp_backoff_count_`,> 阈值时进入 **DEGRADED 状态**(只 fetch 不 dispatch,等 SQ 清空)

### 4.5 测试覆盖(s3 W6 T-s3-3 acceptance,新增 4 项)

| 测试场景 | 期望 TmuSubmitResult | 期望 handler 行为 |
|----------|---------------------|------------------|
| submit 正常 | SUBMITTED | sq_.enqueue=true,返回 HANDLED |
| scheduler 满 | BACKPRESSURED | handler 未调用,TMU 内 counter++ |
| dep latch 不匹配 | DEP_LATCH_MISMATCH | handler 未调用 |
| SQ 满(handler 探测)| SUBMIT_QUEUE_REJECTED | sq_.enqueue=false,返回 SQ_REJECTED,TMU 撤销 record |
| dep chain 环检测 | DEP_LATCH_MISMATCH | handler 未调用 |
| handler 返回 INVALID_RECORD | INTERNAL_ERROR | record 校验失败,防御性 |

### 4.6 原始代码模板(保留作参考)

```cpp
TmuSubmitResult TmuDispatchProcessor::submit(TmuDispatchRecord record, ...) {
    // ① dep 检查
    if (record.dep_enable && !check_dep_latches(record))
        return TmuSubmitResult::DEP_LATCH_MISMATCH;
    // ② 容量检查
    if (scheduler_cache_.size() >= max_active_tasks_) {
        backpressure_count_++;
        return TmuSubmitResult::BACKPRESSURED;  // 反压 CP,非 LIFO 驱逐
    }
    // ③ handler 派发(handler 内部调 SQ.enqueue)
    TmuHandlerResult hresult = handler_->on_dispatch(record);
    if (hresult == TmuHandlerResult::SQ_REJECTED) {
        sq_rejected_count_++;
        return TmuSubmitResult::SUBMIT_QUEUE_REJECTED;
    }
    if (hresult != TmuHandlerResult::HANDLED) {
        return TmuSubmitResult::INTERNAL_ERROR;
    }
    // ④ 注册到 scheduler_cache
    scheduler_cache_[record.task_id] = record;
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
s3-W7-9 (2026-10-03~23): validate_topology + baseline ≥850 测试
s3-W10 (2026-10-24~30): CHANGELOG + v0.5.0-MVP tag
```

## 7. 风险与缓解(本 change)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | Pm4Decoder NVIDIA method packet 与 UsrLinuxEmu `unpackPm4Header` 比特字段不对齐 | 中 | 中 | 确认 `gpfifo_translator.h:60-73` 真相源 + 单元测试 |
| R2 | CP 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R3 | TMU 反压停 fetch 频繁触发 | 中 | 中 | 32 slot + JSON `tmu_max_active_tasks` 可配置 + BACKPRESSURED 后 CP 退避 |
| R4 | TMU dep 链式推进死循环 | 低 | 高 | 简化环检测(链深 ≤ 8);每任务 visited flag |
| R5 | 850 测试达不到 | 中 | 中 | baseline 817 + s1 已 ≥12 + s2 已 ≥9 + s3 预计 ≥5 + d1 预计 ≥7 = ≥850(per Oracle M1) |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 依赖 s1+s2 (per Oracle 拆分)
**下次更新**: W5 s3 启动时
