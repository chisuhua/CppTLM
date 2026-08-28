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
// 4 个 method packet 类型枚举(per Phase F-H.3 路径 3 + s3 Oracle P1-1,2026-08-28)
enum class Pm4MethodType {
    DISPATCH_DIRECT,   // 0x4000-0x40FF — CTA 启动
    EVENT_WRITE,       // 0x4200-0x42FF — 时间戳/事件
    RELEASE_MEM,       // 0x4400-0x44FF — 显存释放
    ACQUIRE_MEM,       // 0x4500-0x45FF — 显存获取
    UNKNOWN,           // 不在 4 ranges 内,parse_method 错误通道
};

struct Pm4MethodHeader {
    uint32_t inc : 1;            // bit 0 (Increment register)
    uint32_t method_addr : 15;   // bits 1-15 (32K method addresses)
    uint32_t subchannel : 4;     // bits 16-19 (NVIDIA 4-bit)
    uint32_t data_count : 4;     // bits 20-23
    uint32_t reserved : 8;       // bits 24-31
};
// 真相源:UsrLinuxEmu gpfifo_translator.h:60-73 unpackPm4Header
// static_assert(sizeof(Pm4MethodHeader) == 4, "Pm4MethodHeader must be 32-bit"); (实施时 GCC/Clang LSB packing)

struct Pm4MethodDispatch {
    Pm4MethodType type = Pm4MethodType::UNKNOWN;
    uint16_t method_addr = 0;
    uint8_t subchannel_id = 0;
    uint8_t data_count = 0;
    // s3 will add (per Pm4MethodDispatch 填充):
    //   grid_x, grid_y, grid_z (per CTA grid dimensions)
    //   block_x, block_y, block_z (per CTA block dimensions)
    //   shared_mem_bytes
    //   args_vram_addr (kernel args VRAM pointer)
};

// parse_method 错误通道(per Oracle P1-2,2026-08-28):
//   - 不抛异常
//   - 错误响应 = Pm4MethodDispatch{type=Pm4MethodType::UNKNOWN, method_addr=原值, 其他=0}
//   - 调用方应先检查 type 字段,UNKNOWN 即为错误
```

## 3. CommandProcessor 5-state FSM(per Phase F-H.3 修订 + s2 骨架)

> **关键**(per Oracle ses_fe0b6e44 s2 骨架修复,2026-08-21): s2 已创建 `command_processor_mvp.hh` + `pm4_decoder_mvp.hh` 接口骨架(纯虚 `Pm4DecoderInterface`),s3 W5-6 填充实际实现(GPU VA fetch + parse_method + DECODE 实际逻辑)。

### 3.1 5 状态定义
- IDLE → FETCH → DECODE → DISPATCH → COMPLETE
- **FETCH**:`vram_read_cb_(gpu_va, sizeof(gpu_gpfifo_entry))` → `gpfifo_entry_`(per Phase F-C.3 H1,**不是 BAR0 MMIO**)
- DECODE:通过 `decoder_->parse_method(header, payload, max_dwords)` 解析 `Pm4MethodDispatch`
- DISPATCH:`dispatch_target_(TmuDispatchRecord)` → `TmuSubmitResult`,CP 据此推进/退避

### 3.2 装配接口(s3 W5 扩展 CommandProcessor .hh)

s2 骨架仅提供 `set_decoder(std::unique_ptr<Pm4DecoderInterface>)`。s3 实施需扩展 .hh 加 4 个装配方法(均在 `include/tlm/gpu/command_processor_mvp.hh`,s3 commit T-s3-2 引入):

```cpp
class CommandProcessor {
public:
    // ── s2 接口(已有) ──
    void set_decoder(std::unique_ptr<Pm4DecoderInterface> decoder);

    // ── s3 新增装配接口 ──
    // FETCH 阶段:CP 调 reader(gpu_va, out_buf, sizeof(gpu_gpfifo_entry))
    // 返回值语义与 DGpuBoardTLM::read_vram 对齐(0 = 成功,负值 = errno)
    using VramReadFn = std::function<int32_t(uint64_t gpu_va, void* out, size_t size)>;
    void set_vram_reader(VramReadFn reader);

    // DISPATCH 阶段:CP 把 Pm4MethodDispatch 适配为 TmuDispatchRecord 后调 fn(record)
    // 返回 TmuSubmitResult,CP 据此推进或退避(per §4.4)
    using DispatchFn = std::function<TmuSubmitResult(const TmuDispatchRecord&)>;
    void set_dispatch_target(DispatchFn fn);

    // 退避控制(per §4.4 + Oracle P1-a 修复 2026-08-28):
    // 主路径 = CP 内部自动从 dispatch_target 返回值退避(无需外部调用)。
    // on_backpressure / on_submit_queue_rejected 是**可选**外部通知接口,
    // 默认空实现;DGpuBoardTLM §3.3 装配 4 行不调用它们。
    // 测试断言通过 cp_backoff_count() / degraded() getter 直接读取 CP 内部状态。
    void on_backpressure(uint64_t cycles);
    void on_submit_queue_rejected(uint64_t cycles);
};
```

**设计意图**:
- 用 `std::function` 而非裸指针:便于测试注入 mock(`test_command_processor_mvp.cc` 可传 lambda 替代真实 VRAM/TMU),无需拆 dgpu_board_mvp.hh 的实现
- 解耦编译依赖:CommandProcessor .hh 不直接 include tmu_dispatch_processor_mvp.hh / submit_queue_mvp.hh,减少 s3 commit 头文件改动

### 3.2.1 状态查询 getter(per Oracle P1-a 修复,2026-08-28)

CP 内部从 `dispatch_target_` 返回值自动检测反压/退避,测试通过以下 getter 断言状态:

```cpp
// 状态机计数器(诊断/测试断言用)
uint64_t cp_backoff_count() const;            // 累计 BACKPRESSURED/SUBMIT_QUEUE_REJECTED 次数
uint64_t wake_count() const;                  // wake() 调用次数(s2 已有)
uint64_t tick_count() const;                  // tick() 调用次数(s2 已有)
uint64_t state_transitions() const;           // state 转换次数(s2 已有)

bool degraded() const;                        // CP_BACKOFF_DEGRADED_THRESHOLD 触发后返回 true
uint64_t backoff_cycles_remaining() const;    // 剩余退避 cycles(≥ MIN_BACKOFF_CYCLES 时 DISPATCH 跳过)

// State enum 新增 DEGRADED(per Oracle P2 决策):
enum class State { IDLE, FETCH, DECODE, DISPATCH, COMPLETE, DEGRADED };
// DEGRADED = 仅 fetch,不 dispatch,等 SQ 清空
// transition: ≥ CP_BACKOFF_DEGRADED_THRESHOLD 次反压 → 进入 DEGRADED
```

**常量定义**(放 `include/tlm/gpu/command_processor_mvp.hh`,s3 commit T-s3-2 引入):
```cpp
static constexpr uint64_t MIN_BACKOFF_CYCLES = 8;
static constexpr uint64_t CP_BACKOFF_DEGRADED_THRESHOLD = 3;
```

**测试模式**:`test_command_processor_mvp.cc` 用 lambda 注入 mock `dispatch_target`,连续返回 3 次 `BACKPRESSURED` → 断言 `cp_backoff_count() == 3` + `state() == State::DEGRADED` + `degraded() == true`。

### 3.3 DGpuBoardTLM 装配位置(s3 W6 T-s3-3 实施)

`src/tlm/gpu/dgpu_board_mvp.cc::init()` 增加如下装配(per Phase F-H.2 + Oracle ses_fe0b6e44):

```cpp
void DGpuBoardTLM::init() {
    if (impl_->initialized) return;
    impl_->bar.init();
    // ── s3 装配接线(T-s3-3 落地,避免跨 commit 半接线) ──
    impl_->cp.set_decoder(std::make_unique<Pm4Decoder>());
    impl_->cp.set_vram_reader([this](uint64_t va, void* out, size_t sz) {
        return this->read_vram(va, out, sz);  // 委托到 DGpuBoardTLM::read_vram(BAR1 DMA)
    });
    impl_->cp.set_dispatch_target([this](const TmuDispatchRecord& rec) {
        return impl_->tmu.submit(rec);  // 直接调 TMU,经其内部 handler 派发到 SQ
    });
    impl_->tmu.set_handler(std::make_unique<S3SubmitQueueHandler>(impl_->sq));
    impl_->initialized = true;
}
```

**注意**:
- S3SubmitQueueHandler 定义在 `src/tlm/gpu/dgpu_board_mvp.cc`(per §4.3),不放 .hh 避免污染 CommandProcessor 编译依赖
- 装配接线**必须**整体归入 T-s3-3 commit,不在 T-s3-2 提前——否则 CP 持有空 reader/target 会运行时崩
- T-s3-2 仅实现 CP .cc 逻辑,**不**创建 Pm4Decoder 实例(由 T-s3-3 init 注入);但 T-s3-2 测试需手动 set_decoder + set_vram_reader + set_dispatch_target mock

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

### 4.4 CP 退避策略(per Oracle M4 + P2-2 修复 2026-08-28)

CP 收到 `BACKPRESSURED` / `SUBMIT_QUEUE_REJECTED` 时:
- **不要立即重试** — 引入最小退避窗口(per Phase F-D.2 H5 + Oracle P2-2):
  - `static constexpr uint64_t MIN_BACKOFF_CYCLES = 8;` — 定义在 `include/tlm/gpu/command_processor_mvp.hh`,s3 T-s3-2 commit 引入
- 状态保持在 DECODE,DISPATCH 失败时**回到 FETCH 而不是 IDLE**
- 失败计数累计到 `cp_backoff_count_`,**≥ 阈值**时进入 **DEGRADED 状态**(只 fetch 不 dispatch,等 SQ 清空):
  - `static constexpr uint64_t CP_BACKOFF_DEGRADED_THRESHOLD = 3;` — 同上 .hh,s3 T-s3-2 commit 引入
  - 三方统一:design §4.4 = tasks T-s3-2 = tasks T-s3-3 测试 均为"**≥3 次进入 DEGRADED**"
  - 实施测试:`tmu` mock 连续返回 BACKPRESSURED 3 次 → CP.state() == DEGRADED(若 CP 引入 DEGRADED 状态;s3 仅作计数+future hook 即可)

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
| `test_pm4_decoder_mvp_integration.cc` | `[pm4-decoder-integration]` | CP + Decoder 集成 |
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
