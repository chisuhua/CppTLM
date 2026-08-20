# tmu-dispatch-processor 微架构文档

> **类别**: GPU > TMU Glue · **状态**: 🔵 MVP 切片 (per ADR-X.17)
> **Header**: `include/tlm/gpu/tmu_dispatch_processor_mvp.hh`
> **位置**: DGpuBoardTLM 内部组件(非独立 ChStreamModuleBase)
> **蓝图来源**: US20230236878A1 Task Dependency Table + per ADR-X.16 + v0.4 design §3.8
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**: [`ADR-X.17-cpptlm-v05-mvp.md`](../../adr/ADR-X.17-cpptlm-v05-mvp.md) D5
> **关联模块**: [`command-processor.md`](./command-processor.md) · [`cuda-core-adapter.md`](./cuda-core-adapter.md)
> **首版 commit**: 🔵 W5-6 实施 · **最近更新**: 2026-08-19
> **维护者**: CppTLM Team (Sisyphus)

> **关联调研**: [`docs/research/TMU/`](../../research/TMU/) (16 文件,US20230236878A1 Hopper WSDU + Kepler TMU + Volta/Ampere TMU/WDU)

---

## 1. 设计目标

`TmuDispatchProcessor`(TMU Glue)是 DGpuBoardTLM 内部组件,负责 **SQ(SQ ↔ CudaCoreAdapter)之间的依赖解耦与调度策略**,实现 Task Management Unit 行为(简化版)。

**核心特性**:
- **MVP 简化版**:`submit` / `on_complete` / `try_chain_dependent` 三接口
- **`inflight_kernel_reqs_` map**:32 slot(MVP 简化,v0.5 完整版 256 slot)
- **LIFO eviction**:容量满时驱逐最近入队的非 pinned 任务
- **dep latch**:`wait_on_latch_id ↔ arrive_at_latch_id` 匹配检查
- **pre-exit policy**:MVP 仅 NONE 档(LAST_BLOCK/EXPLICIT_KERNEL_MARKER 推到 v0.5 完整版)

**MVP vs v0.5 完整版简化**:
- ✅ 保留:3 接口 + dep latch + LIFO eviction
- ❌ 裁剪:32 slot(v0.5 完整版 256 slot)
- ❌ 裁剪:Scheduler Cache LRU(仅 hash map)
- ❌ 裁剪:TMD 字段 6 区精细化(per `docs/research/TMU/TMD.md`)
- ❌ 裁剪:LAST_BLOCK/EXPLICIT_KERNEL_MARKER 档(MVP 仅 NONE)
- ❌ 裁剪:refcount + OutDependence[] 管理
- ❌ 裁剪:PREEXIT/ACQBULK 指令仿真(由 PTX-EMU 自含)

---

## 2. 架构概览

```
CommandProcessor (CP)
    │ DISPATCH_DIRECT opcode
    ▼
TmuDispatchProcessor::submit(record)
    │
    ├─ 1. select_cluster(stream_id) → cluster_id(固定绑定)
    ├─ 2. inflight_kernel_reqs_[task_id] = record
    ├─ 3. pre_dispatch():
    │      - dep_enable=false → 直接 dispatch
    │      - dep_enable=true → check_dep_latches()
    │      - 若 inflight_kernel_reqs_.size() >= MAX_ACTIVE_TASKS → LIFO evict
    └─ 4. CudaCoreAdapter::issueTask(record)
         │
         ▼ (kernel 执行)
         │
CudaCoreAdapter::on_complete(image_id)
    │
    ▼
TmuDispatchProcessor::on_complete(record)
    │
    ├─ 1. inflight_kernel_reqs_.erase(task_id)
    ├─ 2. CompletionRing::push(image_id, status)
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

### 3.2 TmuSubmitResult 枚举

```cpp
enum class TmuSubmitResult : uint8_t {
    SUBMITTED,              // 成功提交
    EVICTED_PREVIOUS,       // 容量满,驱逐前一任务(返回 evicted_id)
    DEP_LATCH_MISMATCH,     // dep latch 不匹配
    CUDA_CORE_REJECTED,     // CudaCoreAdapter 拒绝
};
```

---

## 4. 接口(Public API)

```cpp
class TmuDispatchProcessor {
public:
    /// MVP 简化:32 slot 默认(可由 JSON params 配置)
    static constexpr uint32_t MAX_ACTIVE_TASKS_MVP = 32;

    explicit TmuDispatchProcessor(CudaCoreAdapter& cuda_core,
                                   CompletionRing& cq);

    /// CP 调:提交 kernel launch 请求
    /// @return TmuSubmitResult(若 EVICTED_PREVIOUS,evicted_task_id 通过 out_evicted 输出)
    TmuSubmitResult submit(TmuDispatchRecord record,
                           uint32_t* out_evicted = nullptr);

    /// CudaCoreAdapter 完成 kernel 后调
    void on_complete(uint32_t task_id, int32_t status);

    /// 内部 dep chain 推进
    void try_chain_dependent(const TmuDispatchRecord& completed_record);

    /// Per-tick 推进(由 DGpuBoardTLM::tick() 调用)
    void tick();

    // === 测试/监控接口 ===
    size_t inflight_count() const { return scheduler_cache_.size(); }
    uint64_t submit_count() const { return submit_count_; }
    uint64_t evict_count() const { return evict_count_; }
    uint64_t complete_count() const { return complete_count_; }

    /// JSON params 注入
    void set_max_active_tasks(uint32_t n) { max_active_tasks_ = n; }
    void set_lifo_evict(bool enabled) { lifo_evict_ = enabled; }

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

    /// LIFO eviction(选择最近入队的非 pinned 任务)
    uint32_t lifo_evict();

    // === 依赖 ===
    CudaCoreAdapter& cuda_core_;
    CompletionRing& cq_;

    // === 状态 ===
    std::unordered_map<uint32_t, TmuDispatchRecord> scheduler_cache_;
    uint32_t max_active_tasks_ = MAX_ACTIVE_TASKS_MVP;
    bool lifo_evict_ = true;

    // === 统计 ===
    uint64_t submit_count_ = 0;
    uint64_t evict_count_ = 0;
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

    // 2. 容量检查 + LIFO eviction
    uint32_t evicted_id = 0;
    if (scheduler_cache_.size() >= max_active_tasks_) {
        if (!lifo_evict_) {
            return TmuSubmitResult::EVICTED_PREVIOUS;  // 不可驱逐
        }
        evicted_id = lifo_evict();
        if (out_evicted) *out_evicted = evicted_id;
        evict_count_++;
    }

    // 3. 插入 scheduler cache
    scheduler_cache_[record.task_id] = record;
    submit_count_++;

    // 4. 派发到 CudaCoreAdapter
    bool issued = cuda_core_.issueTask(record);
    if (!issued) {
        scheduler_cache_.erase(record.task_id);
        return TmuSubmitResult::CUDA_CORE_REJECTED;
    }

    return evicted_id ? TmuSubmitResult::EVICTED_PREVIOUS : TmuSubmitResult::SUBMITTED;
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
            // dep 满足,重新 pre_dispatch
            if (check_dep_latches(record)) {
                cuda_core_.issueTask(record);
                dep_chain_advance_count_++;
            }
        }
    }
}
```

### 5.4 LIFO eviction

```cpp
uint32_t TmuDispatchProcessor::lifo_evict() {
    // 选择最近入队(enqueue_cycle 最大)的非 pinned 任务
    auto victim_it = scheduler_cache_.end();
    uint64_t max_cycle = 0;
    for (auto it = scheduler_cache_.begin(); it != scheduler_cache_.end(); ++it) {
        if (it->second.enqueue_cycle > max_cycle) {
            max_cycle = it->second.enqueue_cycle;
            victim_it = it;
        }
    }

    if (victim_it != scheduler_cache_.end()) {
        uint32_t evicted_id = victim_it->first;
        // 通知 CompletionRing:evicted status
        cq_.push(evicted_id, /*status=*/CUDA_ERROR_LAUNCH_TIMEOUT);
        scheduler_cache_.erase(victim_it);
        return evicted_id;
    }
    return 0;
}
```

---

## 6. 关键设计取舍

### 6.1 MVP 32 slot vs v0.5 256 slot

- **MVP 默认 32 slot**:够覆盖 1-4 stream × 8 active tasks MVP 验证场景
- **v0.5 完整版 256 slot**:覆盖真实 GPU 一代活跃任务上界
- LIFO eviction 默认启用,溢出率 >5% 触发 review

### 6.2 单 cluster(MVP 简化)

- **MVP 阶段**:**仅 1 cluster**(全 stream 共享,per Phase A 修复 S3)— `select_cluster()` 永远返回 0
- **JSON `max_streams` 默认 = 1**(而非 `dgpu-board.md` §2.3 示例的 4)— 避免 S3 期间被误解为多 cluster 调度
  - **或**:保留 `max_streams=4` 但**仅用于 SQ 多流 FIFO 数量**,**不**进入 TMU cluster 选择
  - **建议**:S1 实施前在 `dgpu-board.md` §2.3 JSON 示例默认值改为 `max_streams=1`(per S3 修复)
- v0.5 完整版支持多 cluster + cluster_id 选择策略

### 6.3 pre_exit_policy 仅 NONE

per Oracle v0.4.1 简化(per `ADR-X.15 §7.3` v0.5 反转):
- LAST_BLOCK / EXPLICIT_KERNEL_MARKER 需 PTX-EMU 提供 block/PREEXIT 进度回调
- 8 ABI(`image_load/image_execute` 等)无此信息通道
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
| `test_tmu_dispatch_processor_mvp.cc` | `[tmu][mvp][glue]` | submit / on_complete / try_chain_dependent / LIFO eviction / dep chain / 环检测 |

**验收标准**(per ADR-X.17 G-MVP-3):
- 3 接口(单测) PASS
- 32+1 LIFO eviction PASS(第 33 task 触发驱逐)
- dep chain 链式推进 PASS(depth=3)
- 环检测 PASS(depth > 8 拒绝)

---

## 8. 实施路径(S3 W5-6)

1. 新建 `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `src/tlm/gpu/tmu_dispatch_processor_mvp.cc`(~250 LOC)
2. 引用 `cuda_core_adapter_mvp.hh` + `completion_ring_mvp.hh`
3. 新建 `test/test_tmu_dispatch_processor_mvp.cc`(~10 单测)
4. 集成到 `DGpuBoardTLM::tick()`

---

## 9. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | LIFO 频繁驱逐 | 中 | 中 | 32 slot MVP 默认;溢出率 >5% 触发 review |
| R2 | dep 链式推进死循环 | 低 | 高 | 简化环检测(链深 ≤ 8);每任务 visited flag |
| R3 | LIFO 驱逐丢失 completion 通知 | 低 | 高 | `cq_.push(evicted_id, CUDA_ERROR_LAUNCH_TIMEOUT)` 显式通知 |
| R4 | submit_count_/evict_count_ 计数器溢出 | 低 | 低 | uint64_t 计数 |
| R5 | 32 slot MVP 不够真实工作负载 | 中 | 低 | JSON params 暴露 `tmu_max_active_tasks` 可配置 |

---

## 10. 修订历史

- **2026-08-19**: 初版 — per ADR-X.17 D5 切片(MVP 4 阶段 S3)

---

*维护者: CppTLM Team (Sisyphus) · 最后更新: 2026-08-19*
