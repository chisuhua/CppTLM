# submit-queue 微架构文档(per Phase F-H.5 新增)

> **类别**: GPU > Submit Queue (WDU 分发网络) · **状态**: 🟢 MVP 切片 (per ADR-SOC-06) + 📋 **v1.0 双 vendor 共享 + CGA Cluster 推迟**(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D2 + [`ADR-SOC-12`](../../adr/ADR-SOC-12-host-bypass-and-rc.md))
> **Header**: `include/tlm/gpu/submit_queue_mvp.hh`
> **位置**: DGpuBoardTLM 内部组件(非独立 ChStreamModuleBase)
> **蓝图来源**: NVIDIA Hopper WDU + Work Distribution Crossbar(per `docs/research/WDUtoSM/overview.md` §三) + AMD SPI/SQ(per §四)
> **OpenSpec**: `openspec/changes/2026-08-19-cpptlm-v05-mvp/`
> **关联 ADR**:
> - [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5 — v0.5 路径
> - [`ADR-SOC-09-v1-nvidia-amd-dual-vendor.md`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D2 — v1.0 双 vendor 共享
> - [`ADR-SOC-10-module-factory-topology.md`](../../adr/ADR-SOC-10-module-factory-topology.md) D2 — SimModule 9 类容器
> **关联模块**: [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md) · [`cuda-core-adapter.md`](./cuda-core-adapter.md) · [`L5 WDU 子系统架构`](../architecture/04-work-distribution.md)
> **首版 commit**: 🔵 W3-4 实施 · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team (Sisyphus)

> **关联调研**: [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md)(NVIDIA WDU + Work Distribution Crossbar + 12 专利族)、[`docs/research/WDUtoSM/nvidia/US8112614B2_...md`](../../research/WDUtoSM/nvidia/US8112614B2_协作线程阵列并行数据处理_解析.md)(CTA 奠基)

---

## 1. 设计目标(per Phase F-H.5 新增)

`SubmitQueue`(SQ)是 **DGpuBoardTLM 内部组件**,位于 **TMU 与 CudaCore 之间**,实现**简化版 WDU(Work Distribution Unit) + Work Distribution Crossbar** 分发网络,把 CTA 路由到 target SM/CudaCore。MVP 阶段单 SM 路由即可(per `cuda-core-adapter.md` §F-H.1 `num_sms=1`),但**接口与数据结构按真实 NVIDIA WDU 设计**,v0.5 完整版可平滑升级为多 SM + Work Distribution Crossbar(per `US20240356866A1` 动态目的选择 + `US20240036952A1` WDU pending/active pool 32/4 槽)。

**为什么 MVP 阶段需要 SQ**(用户原话,2026-08-20):
> "TMU 到 CudaCore 之间有一个分发的网络,我希望在 MVP 阶段能尽量接近最终的形态"

**MVP 模块归属**(per Phase A 修复 M4 协调):
- ✅ **S1-S4 阶段固定为 DGpuBoardTLM 内部组件**
- ❌ **不允许独立暴露为 ChStreamModuleBase**(会破 CP→TMU→SQ→CudaCore 单链路契约)
- 🟡 **v0.5 完整版**可评估独立 ChStreamModuleBase(per ADR-X.16 D4 ComputeUnit Adapter pattern)— 但 MVP 不实施

**核心特性**(per Phase F-H.5):
- **WDU 分发**:简化版 `select_target_core(cta_desc)` → MVP 始终返回 0(单 SM 路由)
- **Pending/Active Pool**(per `US20240036952A1` 披露 32 pending / 4 active 槽):MVP 简化 `pending_queue_[cluster_id]`(每个 cluster 一个 FIFO),active 槽简化为 `in_flight_tasks_[target_core_id]`
- **CTA descriptor**(per `US8112614B2` 奠基定义):包含 grid/block/shared_mem/args,跨 SM 唯一 CTA ID
- **提交/完成双向流**:TMU::on_complete → SQ::on_warp_complete → CudaCore 完成回调

**MVP vs v0.5 完整版简化**:
- ✅ 保留:WDU 路由接口(`select_target_core`)
- ✅ 保留:Per-cluster pending FIFO
- ✅ 保留:CTA descriptor 数据结构
- ❌ 裁剪:Work Distribution Crossbar 逐周期动态目的选择(MVP 单 SM,无 crossbar)
- ❌ 裁剪:Multi-SM 路由(MVP `num_sms=1`,per `cuda-core-adapter.md` §5)
- ❌ 裁剪:CTA 完整性多 SM 跨域分发(MVP 单 SM 即可)
- ❌ 裁剪:Speculative launch + DSMEM(MVP 不实施)

---

## 2. 架构概览

```
TmuDispatchProcessor::submit(record)
    │
    ▼
SubmitQueue::enqueue(cta_descriptor)
    │
    ├─ 1. select_target_core(cta_desc) → target_core_id(MVP 始终 0)
    ├─ 2. pending_queue_[cluster_id].push(cta_desc)
    └─ 3. target_core_id 槽位检查:
         - 若 in_flight_tasks_[target_core_id] < max_active_per_core_:
           → 直接 dispatch → CudaCoreAdapter[target].on_cta_arrival
         - 否则:留在 pending queue,等待 target core 完成释放
    │
    ▼ (per-tick)
SubmitQueue::tick()
    │
    ├─ 遍历所有 cluster 的 pending queue
    ├─ 对每个 cluster:
    │   ├─ 检查 in_flight_tasks_[target_core_id] < max_active_per_core_
    │   ├─ 若有空槽:pop front → dispatch_to_core(cta_desc)
    │   └─ 否则:break(单 cluster 简化,无 crossbar 重路由)
    │
    ▼ (warp 完成回调)
SubmitQueue::on_warp_complete(task_id, status)
    │
    ├─ 1. in_flight_tasks_[target_core_id].erase(task_id)
    ├─ 2. tmu_->on_complete(task_id, status)
    └─ 3. (next tick 触发新 dispatch)
```

---

## 3. 接口(Public API)

```cpp
#include <deque>
#include <memory>
#include <unordered_map>
#include <vector>

class SubmitQueue {
public:
    /// CTA 描述符(WDU 跨 SM 分发的基本单元,per `US8112614B2`)
    /// 注意:这里 CtaDescriptor 等价于 CudaCoreAdapter::CtaDescriptor(per cuda-core-adapter.md §3)
    ///       SQ 持有指向 CudaCoreAdapter 的引用,转发时构造 CudaCoreAdapter::CtaDescriptor
    struct CtaDescriptor {
        uint32_t task_id;               // 唯一 CTA ID(TMU 分配)
        uint64_t vram_image_addr;       // PTX IR 在 VRAM 中的偏移
        size_t image_size;
        uint32_t grid_x, grid_y, grid_z;
        uint32_t block_x, block_y, block_z;
        size_t shared_mem_bytes;
        void** kernel_args;
        size_t args_count;
        uint8_t cluster_id;             // 来源 cluster(TMU 路由)
        uint8_t target_core_id;         // 目标 SM/CudaCore(WDU 路由)
        uint64_t enqueue_cycle;         // 入队 cycle(超时检测)
    };

    /// MVP 默认参数(per `US20240036952A1` WDU 披露 32 pending / 4 active 槽简化)
    static constexpr uint32_t MAX_PENDING_PER_CLUSTER_MVP = 32;
    static constexpr uint32_t MAX_ACTIVE_PER_CORE_MVP = 4;
    static constexpr uint32_t MAX_CLUSTERS_MVP = 1;
    static constexpr uint32_t MAX_CORES_MVP = 1;  // per cuda-core-adapter.md §5(num_sms=1)

    explicit SubmitQueue(std::vector<CudaCoreAdapter*>& cuda_cores,
                         TmuDispatchProcessor* tmu);

    /// TMU 调用:提交 CTA 描述符
    /// @return true=成功入队(可能立即 dispatch 到 target core),false=拒绝(pending queue 满)
    bool enqueue(CtaDescriptor cta);

    /// Per-tick 推进(由 DGpuBoardTLM::tick() 调用)
    void tick();

    /// CudaCore 完成回调(per cuda-core-adapter.md §4.3)
    void on_warp_complete(uint32_t task_id, int32_t status);

    // === 测试/监控接口 ===
    size_t pending_count(uint8_t cluster_id) const;
    size_t in_flight_count(uint8_t core_id) const;
    uint64_t enqueue_count() const { return enqueue_count_; }
    uint64_t dispatch_count() const { return dispatch_count_; }
    uint64_t complete_count() const { return complete_count_; }

    /// JSON params 注入
    void set_max_pending_per_cluster(uint32_t n) { max_pending_per_cluster_ = n; }
    void set_max_active_per_core(uint32_t n) { max_active_per_core_ = n; }

private:
    /// WDU 路由:CTA descriptor → target core id
    /// MVP:始终返回 0(单 SM 路由)
    /// v0.5: 升级为 Work Distribution Crossbar 动态目的选择(per US20240356866A1)
    uint8_t select_target_core(const CtaDescriptor& cta) const {
        // MVP 简化:固定路由到 core 0
        // 真实 GPU:crossbar 逐周期仲裁,按信用 + availability + 亲和性选择
        return 0;
    }

    /// 从 pending queue 派发一个 CTA 到目标 CudaCore
    void dispatch_to_core(CtaDescriptor cta);

    // === 依赖 ===
    std::vector<CudaCoreAdapter*>& cuda_cores_;
    TmuDispatchProcessor* tmu_;

    // === 状态 ===
    std::vector<std::deque<CtaDescriptor>> pending_queue_;     // per-cluster FIFO
    std::unordered_map<uint32_t, uint8_t> in_flight_tasks_;   // task_id → core_id

    // === 配置 ===
    uint32_t max_pending_per_cluster_ = MAX_PENDING_PER_CLUSTER_MVP;
    uint32_t max_active_per_core_ = MAX_ACTIVE_PER_CORE_MVP;
    uint32_t max_clusters_ = MAX_CLUSTERS_MVP;
    uint32_t max_cores_ = MAX_CORES_MVP;

    // === 统计 ===
    uint64_t enqueue_count_ = 0;
    uint64_t dispatch_count_ = 0;
    uint64_t complete_count_ = 0;
    uint64_t queue_full_count_ = 0;  // pending 满拒绝
};
```

---

## 4. 行为流程

### 4.1 enqueue()

```cpp
bool SubmitQueue::enqueue(CtaDescriptor cta) {
    cta.target_core_id = select_target_core(cta);
    cta.enqueue_cycle = current_cycle_;

    // 1. pending 队列容量检查
    uint8_t cluster = cta.cluster_id;
    if (pending_queue_[cluster].size() >= max_pending_per_cluster_) {
        queue_full_count_++;
        return false;  // 队列满拒绝(TMU 反压或 retry)
    }

    // 2. 入队
    pending_queue_[cluster].push_back(std::move(cta));
    enqueue_count_++;
    return true;
}
```

### 4.2 tick()(per-tick 派发)

```cpp
void SubmitQueue::tick() {
    // 遍历所有 cluster 的 pending queue,尝试 dispatch 到有空槽的 target core
    for (uint32_t c = 0; c < max_clusters_; ++c) {
        auto& q = pending_queue_[c];
        while (!q.empty()) {
            CtaDescriptor& front = q.front();
            uint8_t core = front.target_core_id;

            // 检查 target core active 槽
            size_t active_count = count_in_flight_for_core(core);
            if (active_count >= max_active_per_core_) {
                break;  // 目标 core 满,等待下一 tick
            }

            // 派发
            CtaDescriptor cta = std::move(q.front());
            q.pop_front();
            dispatch_to_core(std::move(cta));
        }
    }
}

void SubmitQueue::dispatch_to_core(CtaDescriptor cta) {
    uint8_t core = cta.target_core_id;
    in_flight_tasks_[cta.task_id] = core;
    dispatch_count_++;

    // 构造 CudaCoreAdapter::CtaDescriptor + 转发
    CudaCoreAdapter::CtaDescriptor core_cta {
        .vram_image_addr = cta.vram_image_addr,
        .image_size = cta.image_size,
        .grid_x = cta.grid_x, .grid_y = cta.grid_y, .grid_z = cta.grid_z,
        .block_x = cta.block_x, .block_y = cta.block_y, .block_z = cta.block_z,
        .shared_mem_bytes = cta.shared_mem_bytes,
        .kernel_args = cta.kernel_args,
        .args_count = cta.args_count,
        .task_id = cta.task_id,
        .cluster_id = cta.cluster_id
    };
    cuda_cores_[core]->on_cta_arrival(core_cta);
}
```

### 4.3 on_warp_complete()

```cpp
void SubmitQueue::on_warp_complete(uint32_t task_id, int32_t status) {
    auto it = in_flight_tasks_.find(task_id);
    if (it == in_flight_tasks_.end()) {
        return;  // 不在 flight,忽略
    }
    in_flight_tasks_.erase(it);
    complete_count_++;

    // 转发到 TMU(由 TMU 推进 dep chain + CQ push)
    if (tmu_) {
        tmu_->on_complete(task_id, status);
    }
}
```

---

## 5. 单元测试覆盖

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_submit_queue_mvp_route.cc` | `[submit-queue][mvp][route]` | `select_target_core` 单 SM 路由 |
| `test_submit_queue_mvp_enqueue.cc` | `[submit-queue][mvp][enqueue]` | 入队 + pending 满拒绝 |
| `test_submit_queue_mvp_dispatch.cc` | `[submit-queue][mvp][dispatch]` | tick() 派发到 active 槽满为止 |
| `test_submit_queue_mvp_complete.cc` | `[submit-queue][mvp][complete]` | on_warp_complete → TMU 转发 |
| `test_submit_queue_mvp_concurrent.cc` | `[submit-queue][mvp][concurrent]` | 多 CTA 入队 + 完成链 |

---

## 6. 验收门(G-MVP-6)

### G-MVP-6:SQ 分发网络验证
- [ ] `enqueue()` 入队 ≤ `MAX_PENDING_PER_CLUSTER`(MVP 默认 32)
- [ ] `tick()` 派发到 `in_flight_count < MAX_ACTIVE_PER_CORE`(MVP 默认 4)
- [ ] `on_warp_complete()` 正确释放槽位 + 转发 TMU
- [ ] `select_target_core()` MVP 始终返回 0(单 SM 路由)

---

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:---:|:---:|------|
| R1 | pending queue 满拒绝频繁 | 中 | 中 | MVP `MAX_PENDING_PER_CLUSTER=32`;溢出率 >5% 触发 review |
| R2 | CTA 描述符损坏导致 CudaCore 拒绝 | 低 | 高 | dispatch_to_core 检查 `on_cta_arrival` 返回值,失败时回滚 pending |
| R3 | 多 CTA 反压链路断流 | 中 | 中 | pending 满 → enqueue 返回 false → TMU 应 back off(SQ 不主动 retry) |
| R4 | 计数器溢出 | 低 | 低 | uint64_t 计数 |

---

## 8. 修订记录

| 日期 | 修订 |
|------|------|
| 2026-08-20 | **Phase F-H.5 新增**:用户要求"TMU 到 CudaCore 之间有一个分发的网络,我希望在 MVP 阶段能尽量接近最终的形态"。本模块填补 CP→TMU→SQ→CudaCore 单链路中 SQ 分发网络的缺口。MVP 单 SM 简化版 WDU + 路由接口,接口与数据结构按真实 NVIDIA WDU 设计(`US8112614B2` CTA + `US20240036952A1` pending/active pool),v0.5 完整版可平滑升级为多 SM + Work Distribution Crossbar |