# tmu-dispatch-processor 微架构文档

> **类别**: GPU > TMU Glue · **状态**: 🔵 MVP 切片 (per ADR-SOC-06) + 📋 **v1.0 TMU 三代形态 + TMD prefetch 扩展**(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D5 + L4 TMU/TMD 子系统架构)
> **Header**: `include/tlm/gpu/tmu_dispatch_processor_mvp.hh`
> **位置**: DGpuBoardTLM 内部组件(非独立 ChStreamModuleBase)
> **蓝图来源**: US20230236878A1 Task Dependency Table + per ADR-X.16 + v0.4 design §3.8
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**:
> - [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5 — v0.5 路径
> - [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D5 — v1.0 TMU 共享 + 依赖预取 + PDL
> **关联模块**: [`command-processor.md`](./command-processor.md) · [`cuda-core-adapter.md`](./cuda-core-adapter.md) · [L4 TMU/TMD 子系统架构](../architecture/03-task-management-unit.md)
> **首版 commit**: 🔵 W5-6 实施 · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team (Sisyphus)

> **关联调研**: [`docs/research/TMU/`](../../research/TMU/) (16 文件,US20230236878A1 Hopper WSDU + Kepler TMU + Volta/Ampere TMU/WDU)

---

## 1. 设计目标

`TmuDispatchProcessor`(TMU Glue)是 DGpuBoardTLM 内部组件,负责 **SQ(SQ ↔ CudaCoreAdapter)之间的依赖解耦与调度策略**,实现 Task Management Unit 行为(简化版)。

**核心特性**:
- **MVP 简化版**:`submit` / `on_complete` / `try_chain_dependent` 三接口
- **`inflight_kernel_reqs_` map**:32 slot(MVP 简化,v0.5 完整版 256 slot)
- **反压停 fetch**(per Phase F-D.2 H5 修订):容量满时**拒绝新任务**并等待 CP tick 反压(对齐真实硬件 + UsrLinuxEmu `HardwarePullerEmu` channel backpressure);**不主动驱逐入队任务**(LIFO 驱逐会破坏 `pending_fence_id_` + `sim_fence_id_signal` 记账契约,per Oracle ses_fe29aa0d 审查)
  - 历史(已废弃):原 LIFO eviction 模式 per Phase A S3 修订— **Oracle 审查发现此为 modeling red flag**:真实 GPU 队列满是反压停 fetch,不会主动驱逐+ 错误完成
- **dep latch**:`wait_on_latch_id ↔ arrive_at_latch_id` 匹配检查
- **pre-exit policy**:MVP 仅 NONE 档(LAST_BLOCK/EXPLICIT_KERNEL_MARKER 推到 v0.5 完整版)

**MVP vs v0.5 完整版简化**:
- ✅ 保留:3 接口 + dep latch + **反压停 fetch**(per Phase F-D.2 H5 修订,原 LIFO eviction 改为反压)
- ❌ 裁剪:32 slot(v0.5 完整版 256 slot)
- ❌ 裁剪:Scheduler Cache LRU(仅 hash map)
- ❌ 裁剪:TMD 字段 6 区精细化(per `docs/research/TMU/TMD.md`)
- ❌ 裁剪:LAST_BLOCK/EXPLICIT_KERNEL_MARKER 档(MVP 仅 NONE)
- ❌ 裁剪:refcount + OutDependence[] 管理
- ❌ 裁剪:PREEXIT/ACQBULK **指令语义**(由 PTX-EMU 自含);**device-side 调度动作**(per Hopper WSDU PDL per `docs/research/TMU/US20230236878A1`)推迟到 v0.5 完整版(per Phase F-D.1 H4)— 真实 PDL 链路见 UsrLinuxEmu `sim_pdl_launch`(`plugins/gpu_driver/sim/pdl.cpp`)

---

## 2. 架构概览(per Phase F-H.4 修订)

```
CommandProcessor (CP)
    │ DISPATCH_DIRECT method_addr (0x4000-0x40FF)
    ▼
TmuDispatchProcessor::submit(record)
    │
    ├─ 1. select_cluster(stream_id) → cluster_id(MVP 单 cluster 返回 0)
    ├─ 2. inflight_kernel_reqs_[task_id] = record
    ├─ 3. pre_dispatch():
    │      - dep_enable=false → 直接 dispatch 到 SubmitQueue
    │      - dep_enable=true → check_dep_latches()
    │      - 若 inflight_kernel_reqs_.size() >= MAX_ACTIVE_TASKS → **反压 stop fetch**(per Phase F-D.2 H5)
    └─ 4. SubmitQueue[cluster_id].enqueue(cta_descriptor)
         │
         ▼ (分发网络:SubmitQueue = WDU + Work Distribution Crossbar 简化版)
         │
SubmitQueue::dispatch_to_core(cta_desc)
    │
    ├─ 路由:cluster_id → target_core_id(MVP 单 SM = target_core_id=0)
    └─ CudaCoreAdapter[target_core_id].on_cta_arrival(cta_desc)
         │
         ▼ (驱动式 warp 执行,深度集成 PTX-EMU)
         │
CudaCoreAdapter::on_warp_complete(task_id, status)
    │
    ▼
SubmitQueue::on_warp_complete(task_id) → TmuDispatchProcessor::on_complete
    │
    ├─ 1. inflight_kernel_reqs_.erase(task_id)
    ├─ 2. CompletionRing::push(task_id, status)
    ├─ 3. try_chain_dependent(record):
    │      - 遍历 inflight_kernel_reqs_,查找 wait_on_latch_id == record.arrive_at_latch_id
    │      - 链式推进(dep.ptr 指向的下一任务)
    └─ 4. 下一 task 的 pre_dispatch() (若 dep 满足)
```

---

## 3. 数据结构

### 3.1 TmuDispatchRecord(MVP 简化版,9 字段 vs v0.5 21 字段)

```cpp
struct TmuDispatchRecord {
    uint32_t task_id = 0;             // 唯一 ID(CP 分配)
    uint8_t  stream_id = 0;           // host stream
    uint8_t  cluster_id = 0;          // 固定绑定(stream_id % cluster_count)
    uint32_t grid_dim[3] = {1, 1, 1}; // kernel grid 维度
    uint32_t block_dim[3] = {1, 1, 1};// block 维度
    uint64_t args_vram_addr = 0;      // args buffer VRAM 偏移
    uint32_t args_size = 0;
    uint32_t shared_mem_bytes = 0;
    uint32_t wait_on_latch_id = 0;    // dep latch: 等
    uint32_t arrive_at_latch_id = 0;  // dep latch: 到
    bool     dep_enable = false;      // 是否有 dep
    uint64_t enqueue_cycle = 0;       // 入队 cycle(超时检测)

    /// [MVP 简化] pre_exit_policy 仅 NONE
    enum class PreExitPolicy : uint8_t { NONE };
    PreExitPolicy pre_exit_policy = PreExitPolicy::NONE;
};
```

### 3.2 TmuSubmitResult 枚举(per Phase F-H.4 修订:SUBMIT_QUEUE_REJECTED 替代 CUDA_CORE_REJECTED)

```cpp
enum class TmuSubmitResult : uint8_t {
    SUBMITTED,              // 成功提交到 SubmitQueue
    BACKPRESSURED,           // 容量满,反压等待(per Phase F-D.2 H5 修订,替代原 EVICTED_PREVIOUS)
    DEP_LATCH_MISMATCH,     // dep latch 不匹配
    SUBMIT_QUEUE_REJECTED,  // SubmitQueue 拒绝(per Phase F-H.4:TMU 不再直接调 CudaCore)
};
```

---

## 4. 接口(Public API)

```cpp
class TmuDispatchProcessor {
public:
    /// MVP 简化:32 slot 默认(可由 JSON params 配置)
    static constexpr uint32_t MAX_ACTIVE_TASKS_MVP = 32;

    explicit TmuDispatchProcessor(SubmitQueue& submit_queue,
                                   CompletionRing& cq);  // per Phase F-H.4:SubmitQueue 替代 CudaCoreAdapter

    /// CP 调:提交 kernel launch 请求
    /// @return TmuSubmitResult(若 BACKPRESSURED,evicted_task_id 仍通过 out_evicted 输出但表示被拒绝,需 CP tick 重试)
    TmuSubmitResult submit(TmuDispatchRecord record,
                           uint32_t* out_evicted = nullptr);

    /// SubmitQueue 完成后调(经 CudaCoreAdapter::on_warp_complete → SubmitQueue::on_warp_complete → TMU::on_complete)
    void on_complete(uint32_t task_id, int32_t status);

    /// 内部 dep chain 推进
    void try_chain_dependent(const TmuDispatchRecord& completed_record);

    /// Per-tick 推进(由 DGpuBoardTLM::tick() 调用)
    void tick();

    // === 测试/监控接口 ===
    size_t inflight_count() const { return scheduler_cache_.size(); }
    uint64_t submit_count() const { return submit_count_; }
    uint64_t evict_count() const { return backpressure_count_; }  // per Phase F-D.2 H5:重命名为 backpressure_count_
    uint64_t complete_count() const { return complete_count_; }

    /// JSON params 注入
    void set_max_active_tasks(uint32_t n) { max_active_tasks_ = n; }
    void set_backpressure(bool enabled) { backpressure_ = enabled; }  // per Phase F-D.2 H5 修订

private:
    /// 简化版 cluster 选择(MVP:固定绑定 stream_id % cluster_count)
    uint8_t select_cluster(uint64_t stream_id) const {
        // MVP 单 cluster(per Phase A 修复 S3):
        //   - JSON `max_streams` 默认值在 MVP 阶段仅用于 SQ 多流管理,**不启用多 cluster 调度**
        //   - 真实 GPU 中 stream_id 与 cluster_id 是不同维度(stream 跨 cluster 调度),
        //     MVP 不仿真该跨维度,直接返回 0(stream 与 cluster 1:1 映射,所有 stream 共享单 cluster)
        //   - v0.5 完整版: select_cluster() 升级支持 `cluster_count` JSON param +
        //     跨 cluster dep chain 调度
        return static_cast<uint8_t>(stream_id % 1);  // MVP 单 cluster
    }

    /// dep latch 匹配检查
    bool check_dep_latches(const TmuDispatchRecord& record) const;

    /// 反压等待(per Phase F-D.2 H5 修订:替代原 LIFO eviction)
    /// 返回 true 表示等待成功(callers 应 back off);false 表示 scheduler_cache_ 已空
    bool backpressure_wait();

    // === 依赖(per Phase F-H.4 修订) ===
    SubmitQueue& submit_queue_;       // 分发网络入口
    CompletionRing& cq_;

    // === 状态 ===
    std::unordered_map<uint32_t, TmuDispatchRecord> scheduler_cache_;
    uint32_t max_active_tasks_ = MAX_ACTIVE_TASKS_MVP;
    bool backpressure_ = true;  // per Phase F-D.2 H5:反压停 fetch 替代 LIFO

    // === 统计 ===
    uint64_t submit_count_ = 0;
    uint64_t backpressure_count_ = 0;  // per Phase F-D.2 H5
    uint64_t complete_count_ = 0;
    uint64_t dep_chain_advance_count_ = 0;
};
```

---

## 5. 行为流程

### 5.1 submit()

```cpp
TmuSubmitResult TmuDispatchProcessor::submit(TmuDispatchRecord record,
                                              uint32_t* out_evicted) {
    record.cluster_id = select_cluster(record.stream_id);
    record.task_id = next_task_id_++;
    record.enqueue_cycle = current_cycle_;

    // 1. dep latch 检查(MVP)
    if (record.dep_enable && !check_dep_latches(record)) {
        return TmuSubmitResult::DEP_LATCH_MISMATCH;
    }

    // 2. 容量检查 + 反压停 fetch(per Phase F-D.2 H5 修订)
    //    替代原 LIFO eviction:真实 GPU 队列满时**反压** stop fetch,
    //    不会主动驱逐入队任务(避免破坏 pending_fence_id_ + sim_fence_id_signal 记账契约)
    if (scheduler_cache_.size() >= max_active_tasks_) {
        // 反压:返回 BACKPRESSURED,调用者应 back off 并重试
        backpressure_count_++;
        return TmuSubmitResult::BACKPRESSURED;
    }

    // 3. 插入 scheduler cache
    scheduler_cache_[record.task_id] = record;
    submit_count_++;

    // 4. 派发到 SubmitQueue(per Phase F-H.4 修订:TMU 不再直接调 CudaCoreAdapter)
    bool enqueued = submit_queue_.enqueue(cta_descriptor);
    if (!enqueued) {
        scheduler_cache_.erase(record.task_id);
        return TmuSubmitResult::SUBMIT_QUEUE_REJECTED;
    }

    return TmuSubmitResult::SUBMITTED;
}
```

### 5.2 on_complete()

```cpp
void TmuDispatchProcessor::on_complete(uint32_t task_id, int32_t status) {
    auto it = scheduler_cache_.find(task_id);
    if (it == scheduler_cache_.end()) {
        return;  // 已驱逐,忽略
    }
    TmuDispatchRecord completed = it->second;
    scheduler_cache_.erase(it);
    complete_count_++;

    // 1. CompletionRing push
    cq_.push(completed.task_id, status);

    // 2. dep chain 推进
    try_chain_dependent(completed);
}
```

### 5.3 try_chain_dependent()

```cpp
void TmuDispatchProcessor::try_chain_dependent(const TmuDispatchRecord& completed) {
    // 遍历 inflight tasks,查找 wait_on_latch_id == completed.arrive_at_latch_id
    for (auto& [task_id, record] : scheduler_cache_) {
        if (record.dep_enable &&
            record.wait_on_latch_id == completed.arrive_at_latch_id) {
            // dep 满足,重新 pre_dispatch(per Phase F-H.4:经 SubmitQueue 派发)
            if (check_dep_latches(record)) {
                submit_queue_.enqueue(cta_descriptor(record));  // FIX-C3:替代 cuda_core_.issueTask
                dep_chain_advance_count_++;
            }
        }
    }
}
```

### 5.4 反压停 fetch(per Phase F-D.2 H5 修订,替代原 LIFO eviction)

```cpp
// FIX-C3(Oracle ses_fe179d02 审查纠正):
// 原 §5.4 声称"反压停 fetch 替代 LIFO"但正文仍贴旧 lifo_evict() 完整实现,矛盾。
// 规范实现如下 —— 容量满时**拒绝新任务**并反压,不驱逐任何入队任务:

TmuSubmitResult TmuDispatchProcessor::submit(TmuDispatchRecord record,
                                              uint32_t* /*out_evicted*/) {
    // 1. dep latch 检查(MVP)
    if (record.dep_enable && !check_dep_latches(record)) {
        return TmuSubmitResult::DEP_LATCH_MISMATCH;
    }
    // 2. 容量检查 + 反压停 fetch(per Phase F-D.2 H5)
    //    **不驱逐入队任务** — 真实 GPU 队列满时反压 stop fetch,
    //    避免破坏 pending_fence_id_ + sim_fence_id_signal 记账契约
    if (scheduler_cache_.size() >= max_active_tasks_) {
        backpressure_count_++;
        return TmuSubmitResult::BACKPRESSURED;  // CP 应 back off 重试
    }
    // 3. 插入 scheduler cache
    scheduler_cache_[record.task_id] = record;
    submit_count_++;
    // 4. 经 SubmitQueue 派发(per Phase F-H.4:TMU 不再直接调 CudaCoreAdapter)
    bool enqueued = submit_queue_.enqueue(cta_descriptor(record));
    if (!enqueued) {
        scheduler_cache_.erase(record.task_id);
        return TmuSubmitResult::SUBMIT_QUEUE_REJECTED;
    }
    return TmuSubmitResult::SUBMITTED;
}
```

---

## 6. 关键设计取舍

### 6.1 MVP 32 slot vs v0.5 256 slot

- **MVP 默认 32 slot**:够覆盖 1-4 stream × 8 active tasks MVP 验证场景
- **v0.5 完整版 256 slot**:覆盖真实 GPU 一代活跃任务上界
- **反压停 fetch 默认启用**(替代 LIFO eviction,per Phase F-D.2 H5),容量满时 `BACKPRESSURED` 返回 CP,不驱逐任何入队任务
- 溢出率 >5% 触发 review(统计 `backpressure_count_ / submit_count_`)

### 6.2 单 cluster(MVP 简化)

- **MVP 阶段**:**仅 1 cluster**(全 stream 共享,per Phase A 修复 S3)— `select_cluster()` 永远返回 0
- **JSON `max_streams` 默认 = 1**(而非 `dgpu-board.md` §2.3 示例的 4)— 避免 S3 期间被误解为多 cluster 调度
  - **或**:保留 `max_streams=4` 但**仅用于 SQ 多流 FIFO 数量**,**不**进入 TMU cluster 选择
  - **建议**:S1 实施前在 `dgpu-board.md` §2.3 JSON 示例默认值改为 `max_streams=1`(per S3 修复)
- v0.5 完整版支持多 cluster + cluster_id 选择策略

### 6.3 pre_exit_policy 仅 NONE

per Oracle v0.4.1 简化(per `ADR-X.15 §7.3` v0.5 反转):
- LAST_BLOCK / EXPLICIT_KERNEL_MARKER 需 PTX-EMU 提供 block/PREEXIT 进度回调
- 深度集成路径(functional facade)**当前**无此信息通道(per Phase I.1 重构,MVP 不实施 PREEXIT/ACQBULK)
- MVP 仅 NONE 档(直接 dispatch)

### 6.4 dep latch 简化

MVP dep latch 实现:
- `wait_on_latch_id`:dep 任务等待的 latch
- `arrive_at_latch_id`:当前任务完成时设置的 latch
- 匹配规则:若 `record.dep_enable=true && record.wait_on_latch_id != 0`,检查 inflight tasks 中是否有 `arrive_at_latch_id == record.wait_on_latch_id` 的已完成任务

---

## 7. 测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_tmu_dispatch_processor_mvp.cc` | `[tmu][mvp][glue]` | submit / on_complete / try_chain_dependent / **反压停 fetch** / dep chain / 环检测 |

**验收标准**(per ADR-SOC-06 G-MVP-3):
- 3 接口(单测) PASS
- **32+1 反压停 fetch PASS**(第 33 task 返回 `BACKPRESSURED`,不驱逐)
- dep chain 链式推进 PASS(depth=3)
- 环检测 PASS(depth > 8 拒绝)
- `TmuSubmitResult::BACKPRESSURED` 返回后 CP 重试逻辑 PASS

---

## 8. 实施路径(S3 W5-6)

1. 新建 `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `src/tlm/gpu/tmu_dispatch_processor_mvp.cc`(~250 LOC)
2. 引用 `submit_queue_mvp.hh` + `completion_ring_mvp.hh`(per Phase F-H.4:TMU 不再直接调 CudaCoreAdapter)
3. 新建 `test/test_tmu_dispatch_processor_mvp.cc`(~10 单测)
4. 集成到 `DGpuBoardTLM::tick()`

---

## 9. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| ~~R1 | LIFO 频繁驱逐~~ | — | — | **🗑️ 风险已消除(per Phase F-D.2 H5)**:反压停 fetch,容量满拒绝不驱逐 |
| R2 | dep 链式推进死循环 | 低 | 高 | 简化环检测(链深 ≤ 8);每任务 visited flag |
| ~~R3 | LIFO 驱逐丢失 completion 通知~~ | — | — | **🗑️ 风险已消除(per Phase F-D.2 H5)**:无反压驱逐,无 CUDA_ERROR_LAUNCH_TIMEOUT 通知路径 |
| R4 (原) | 反压停 fetch 频繁触发导致 CP 忙等 | 中 | 中 | JSON `tmu_max_active_tasks` 可配置;`BACKPRESSURED` 后 CP tick 退避重试 |
| R5 | 32 slot MVP 不够真实工作负载 | 中 | 低 | JSON params 暴露 `tmu_max_active_tasks` 可配置 |

---

## 10. 修订历史

- **2026-08-19**: 初版 — per ADR-SOC-06 D5 切片(MVP 4 阶段 S3)
- **2026-08-20**: Phase F-D.2 H5 修订(LIFO eviction → 反压停 fetch)
- **2026-08-20**: **Phase F-H.4 修订**:**TMU 不再直接调 CudaCoreAdapter**;改为 `TmuDispatchProcessor → SubmitQueue → CudaCoreAdapter` 链路(per `dgpu-board.md` §2.3.1 架构重定义 + `cuda-core-adapter.md` §F-H.1 深度集成)。`TmuSubmitResult::CUDA_CORE_REJECTED` → `SUBMIT_QUEUE_REJECTED`。R1/R3 缓解措施同步更新(无 LIFO 驱逐,无驱逐丢失 completion 路径)
- **2026-08-20**: **FIX-C3(Oracle ses_fe179d02 审查)**:消除 LIFO 残留 — §5.3 `try_chain_dependent` 改调 `submit_queue_.enqueue`(替代 `cuda_core_.issueTask`);§5.4 删除旧 `lifo_evict()` 实现,补规范反压实现;§6.1/§7/§9 R1/R3/R4 同步(反压停 fetch + BACKPRESSURED);§8 引用改 `submit_queue_mvp.hh`

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-20*
