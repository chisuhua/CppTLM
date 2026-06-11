# gpu-compute_unit 微架构文档

> **类别**: gpu > compute_unit
> **状态**: 🟡 规划中（Phase 7.B）
> **Header**: (规划) `include/tlm/gpu/compute_unit_tlm.hh`
> **注册**: (规划) `REGISTER_CHSTREAM` 扩展 `ModuleFactory::registerObject<tlm::ComputeUnitTLM>("ComputeUnitTLM")`
> **蓝图来源**: gem5 `src/gpu-compute/ComputeUnit.py`（接口对位，黑盒版）
> **首版 commit**: 🟡 蓝图（来自 spec §3 + plan Task 8+）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.3
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §3.1
> - Plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../../superpowers/plans/2026-06-11-phase7a-gpu-infra.md) §3.2
> - 通用 GPU 概念: [gpu.common.md](./gpu.common.md)

---

## 1. 设计目标（规划）

`tlm::ComputeUnitTLM` 是 Phase 7.B 引入的**真实 Compute Unit 抽象**——与 `GPUTLM` v0 共享 `compute_unit_base` 基类（D2 决策消除代码重复），但增加：

- **JSON `on_config_loaded` 真实实现**（修复 v0 R5/R3 缺口）
- **WG 调度状态机**（v0 仅一个 `cur_kernel_id_`，B+ 引入 `std::queue<WorkGroup>`）
- **`std::vector<WorkGroup>` 模式**（Phase 7.E 多 CU 实例化前提）
- 共享 `inflight_txns_` / `latency_` 跟踪

**与 gem5 对位**: `gem5::ComputeUnit`（v0 黑盒版，**不**模拟 5-stage pipeline — D2 决策推迟到 Phase 7.F+）。

## 2. 架构概览（规划）

```
┌────────────────────────────────────────────────────────────┐
│             ComputeUnitTLM (Phase 7.B)                     │
│                                                            │
│  ┌──────────────────────────────────────────────────┐     │
│  │ compute_unit_base (共享基类，Phase 7.B 引入)     │     │
│  │   - num_kernels_/kernel_duration_/...            │     │
│  │   - inflight_txns_: map<txn_id, cycle>          │     │
│  │   - latency_ distribution                       │     │
│  │   - 6 个 Scalar stats                            │     │
│  └──────────────────────────────────────────────────┘     │
│                          ↑ 继承                              │
│  ┌──────────────────────────────────────────────────┐     │
│  │ ComputeUnitTLM 特有                              │     │
│  │   - std::vector<WorkGroup> wg_queue_             │     │
│  │   - 接受 KernelLaunchTLM 的 launch 命令            │     │
│  │   - on_config_loaded() 真实 JSON 解析             │     │
│  │   - 7 个 StatGroup stats（含 GPU 特有指标）       │     │
│  └──────────────────────────────────────────────────┘     │
│                                                            │
│  端口（与 GPUTLM v0 同型，单端口 Initiator）              │
│   resp_in_:  InputStreamAdapter<ComputeRespBundle>        │
│   req_out_:  OutputStreamAdapter<ComputeReqBundle>        │
│   adapter_:  StreamAdapterBase* (PIMPL)                 │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {

struct WorkGroup {
    uint32_t kernel_id;
    uint32_t wg_id;
    uint32_t wg_size;           // wavefront 数
    std::array<uint32_t, 3> grid_dim;
    std::array<uint32_t, 3> wg_dim;
    uint64_t kernarg_addr;     // 模拟
    uint32_t lds_size;         // 模拟
    uint32_t priv_size;        // 模拟
};

class compute_unit_base : public ChStreamModuleBase {
    // 共享字段（来自 GPUTLM v0）
protected:
    uint32_t num_kernels_ = 1;
    uint32_t kernel_duration_ = 100;
    uint32_t num_workgroups_ = 4;
    uint32_t workgroup_size_ = 64;
    uint32_t coalescing_factor_ = 1;

    uint32_t cur_kernel_id_ = 0;
    uint64_t next_txn_id_ = 1;
    uint32_t cycles_since_launch_ = 0;
    bool kernel_active_ = false;

    std::unordered_map<uint64_t, uint64_t> inflight_txns_;
    std::mt19937 rng_;

    // 共享统计
    tlm_stats::Scalar kernels_launched_;
    tlm_stats::Scalar workgroups_dispatched_;
    tlm_stats::Scalar requests_issued_;
    tlm_stats::Scalar requests_completed_;
    tlm_stats::Scalar writes_;
    tlm_stats::Scalar reads_;
    tlm_stats::Distribution latency_;

public:
    // 共享 setter
    void set_num_kernels(uint32_t n);
    void set_kernel_duration(uint32_t cyc);
    void set_num_workgroups(uint32_t n);
    void set_workgroup_size(uint32_t sz);
    void set_coalescing_factor(uint32_t cf);
};

class ComputeUnitTLM : public compute_unit_base {
public:
    // 构造
    explicit ComputeUnitTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "ComputeUnitTLM"; }

    // === 真实 on_config_loaded (Phase 7.B 修复 R3 缺口) ===
    void on_config_loaded() override;  // 读 JSON params

    // === WG 调度新增 ===
    void enqueue_workgroup(WorkGroup wg);
    size_t pending_wg_count() const;
    void complete_workgroup(uint32_t wg_id);

    // === 适配器访问器（与 GPUTLM 同型）===
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>&  resp_in();
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>&  req_out();
    cpptlm::StreamAdapterBase* get_adapter() const;
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle>& resp_out();  // dummy
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>& req_in();    // dummy

    // === tick / reset / stats (与 GPUTLM 同型) ===
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    std::deque<WorkGroup> wg_queue_;  // 待执行 WG 队列
    WorkGroup current_wg_;            // 当前正在执行的 WG
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 4 阶段

```cpp
void ComputeUnitTLM::tick() {
    // 1. 响应消费（同 GPUTLM v0）
    if (resp_in_.valid() && resp_in_.ready()) {
        // ... 消费 + latency 统计 + inflight erase
    }

    // 2. 请求发起（4 状态机）
    if (!kernel_active_) {
        if (!wg_queue_.empty()) {
            current_wg_ = wg_queue_.front();
            wg_queue_.pop_front();
            kernel_active_ = true;
            cycles_since_launch_ = 0;
            workgroups_dispatched_++;
        }
    } else {
        // 沿用 GPUTLM v0: cycles_since_launch_ < kernel_duration_ 时
        // 每个 tick 持续发请求（按 coalescing_factor 批量）
        // 请求的 address 改为 base + wg_id * offset（不是 GPUTLM v0 的硬编码 0x10000）
    }

    // 3. 周期计数
    cycles_since_launch_++;

    // 4. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 on_config_loaded() 真实实现（Phase 7.B 修复 R3）

```cpp
void ComputeUnitTLM::on_config_loaded() {
    const json& cfg = get_config();

    // 读 JSON params
    if (cfg.contains("num_kernels")) set_num_kernels(cfg["num_kernels"]);
    if (cfg.contains("kernel_duration")) set_kernel_duration(cfg["kernel_duration"]);
    if (cfg.contains("num_workgroups")) set_num_workgroups(cfg["num_workgroups"]);
    if (cfg.contains("workgroup_size")) set_workgroup_size(cfg["workgroup_size"]);
    if (cfg.contains("coalescing_factor")) set_coalescing_factor(cfg["coalescing_factor"]);

    // 预填 WG 队列（Phase 7.B 简化：按 num_workgroups * num_kernels 静态预填）
    for (uint32_t k = 0; k < num_kernels_; ++k) {
        for (uint32_t w = 0; w < num_workgroups_; ++w) {
            WorkGroup wg;
            wg.kernel_id = k + 1;
            wg.wg_id = w;
            wg.wg_size = workgroup_size_ / coalescing_factor_;
            wg_queue_.push_back(wg);
        }
    }
}
```

### 4.3 关键设计取舍

- **共享基类** `compute_unit_base` 吸收 GPUTLM v0 的 5 setter + 7 统计 + 4 阶段 tick 重复代码
- **WG 队列**引入 `std::deque<WorkGroup>`，支持 B+ 真实多 WG 并发（v0 简化为单个 cur_kernel_id_）
- **`on_config_loaded` 真实读 JSON**——修复 v0 R3 缺口
- **地址生成**不再硬编码 0x10000——按 `wg.kernel_id * 0x10000 + wg.wg_id * 0x1000` 派生
- **不做 5-stage pipeline**（D2 决策）——tick 直接发请求

## 5. Bundle 字段使用（规划）

**ComputeReqBundle 12 字段**（与 GPUTLM v0 一致，详见 [gpu-gputlm.md §5](./gpu-gputlm.md#5-bundle-字段使用)）：

| 字段 | ComputeUnitTLM 使用 | 与 GPUTLM v0 差异 |
|------|-------------------|------------------|
| `transaction_id` | `next_txn_id_++` | 同 |
| `kernel_id` | `current_wg_.kernel_id`（从 WG 队列） | **从队列取**，非计数器 |
| `workgroup_id` | `current_wg_.wg_id`（从 WG 队列） | **从队列取**，非计数器 |
| `wavefront_id` | 0 到 `reqs_per_wg-1` 循环 | 同 |
| `coalescing_factor` | `coalescing_factor_` | 同 |
| `address` | `wg_id * 0x1000`（B+ 改进） | **派生**而非硬编码 |
| `data` | 0xCAFEBABE (v0 桩) | 同 |
| `is_write` | 50% 概率 | 同 |
| 其他 | 透传 | 同 |

## 6. 蓝图对齐

- gem5 `src/gpu-compute/ComputeUnit.py:439-498`（CU 装配循环）
- gem5 `ComputeUnit.dispWorkgroup(wg_id)` 方法
- spec §3.1（ComputeUnit 蓝图章节）
- plan §3.2（Phase 7.B 实施路径）

## 7. 实施路径

### 7.1 Phase 7.B 步骤

1. 新建 `include/tlm/gpu/compute_unit_tlm.hh`（含 `compute_unit_base` + `ComputeUnitTLM`）
2. 修改 `include/chstream_register.hh`：
   - 加 `#include "tlm/gpu/compute_unit_tlm.hh"`
   - 加 `ModuleFactory::registerObject<tlm::ComputeUnitTLM>("ComputeUnitTLM");`
   - 加 `ChStreamAdapterFactory::registerAdapter<tlm::ComputeUnitTLM, ComputeReqBundle, ComputeRespBundle>("ComputeUnitTLM");`
3. 从 `gpu_tlm.hh` 提取 `compute_unit_base`（基类抽象）
4. `GPUTLM` v0 改为 `class GPUTLM : public compute_unit_base`（消除代码重复）
5. 修复 `on_config_loaded` JSON 解析（`traffic_gen_tlm` / `cpu_tlm` / `gpu_tlm` 三者统一）
6. 加 Catch2 测试：`test/test_compute_unit.cc`（`[gpu]` 标签），覆盖：
   - 单 WG 调度
   - 多 WG 队列
   - JSON on_config_loaded
   - 共享基类行为
7. 更新 `AGENTS.md` + `docs/ONBOARDING.md`（ComputeUnitTLM 注册条目）
8. 更新 `docs/soc_arch/modules/README.md`（新增 `gpu-compute_unit.md` 链接）

### 7.2 验收标准

- [ ] 编译通过（Release + Debug）
- [ ] `cpptlm_tests "[gpu]"` 全部通过（含 5 个 GPUTLM v0 + 3 个 ComputeUnitTLM 新增）
- [ ] `cpptlm --config configs/compute_unit_test.json` 端到端可执行
- [ ] `docs_sync_check.sh --strict` 通过
- [ ] 零 TODO/FIXME/XXX in new files
- [ ] JSON `params` 真正被读取（与 v0 R3 缺口对比）
- [ ] 共享基类无代码重复（与 GPUTLM v0 行为对齐）

### 7.3 估计工作量

- 设计: 1-2 周（spec/plan 细化）
- 实施: 1-2 周
- 测试: 1 周
- 文档: 0.5 周
- **总计: 3-5 周**

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **共享基类抽象不充分**——GPUTLM v0 与 ComputeUnitTLM 仍有显著差异（WG 队列 vs 单 kernel counter） | 中 | 中 | 基类暴露虚函数钩子（`virtual void dispatch_wg()`）；不强制重写 |
| R2 | **跨模块代码重复未消除**——`cpu_tlm` / `traffic_gen_tlm` 与 ComputeUnitTLM 仍有重复 | 高 | 中 | Phase 7.B 同步抽象 `cpu_unit_base`（4 模块共享） |
| R3 | **`on_config_loaded` 修复引发回归**——`traffic_gen_tlm` / `cpu_tlm` 同时修复可能破 528+ 现有测试 | 中 | 中 | 增量修复 + 每模块独立测试 |
| R4 | **WG 队列与单 kernel counter 不兼容**——`compute_unit_base` 接口设计不当 | 中 | 中 | 基类保留 v0 `cur_kernel_id_` 行为，B+ 子类用 WG 队列覆盖 |
| R5 | **JSON on_config_loaded 与 set_*() 程序化 API 冲突** | 中 | 中 | 优先级：JSON > 程序化（若两者都设，JSON 覆盖） |
| R6 | **`cache_tlm` / `memory_tlm` R5 风险**未在 Phase 7.B 解决 | 高 | 中 | Phase 7.D MemoryTLM 双 Bundle 注册 |
| R7 | **WG 调度与 KernelLaunchTLM 同步**——`enqueue_workgroup()` 接口契约 | 中 | 中 | Phase 7.B 同周设计 `KernelLaunchTLM`（D3 决策） |
| R8 | **Wavefront/SIMT 行为缺口**（v0 抽象） | 中 | 低 | D2 决策接受；Phase 7.F+ |

## 9. 设计决策点

### D1 共享基类 API 边界

- **Q**: `compute_unit_base` 应暴露什么抽象方法？纯虚函数 vs 钩子函数？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 虚函数 `virtual void dispatch_wg(const WorkGroup&)`，默认实现 = GPUTLM v0 行为
- **依赖**: 与 [gpu.common.md D2](./gpu.common.md) 决策对齐

### D2 WorkGroup 字段最小集

- **Q**: `WorkGroup` 结构应包含哪些字段？仅 `kernel_id + wg_id` 还是完整 HSA AQL 子集？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 最小集（kernel_id / wg_id / wg_size），其他字段预留 stub
- **依赖**: KernelLaunchTLM（D3 决策）

### D3 on_config_loaded 与 set_*() 优先级

- **Q**: 当 JSON params 和程序化 setter 都调用时，谁覆盖谁？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: JSON 后调用者覆盖 setter（v0 GPUTLM 不会有此问题，B+ 修复时确定）

### D4 WG 队列的调度策略

- **Q**: `wg_queue_` 是 FIFO 还是带优先级？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: FIFO（v0 简化）
- **依赖**: 与真实 GPU scheduler 一致性

### D5 Phase 7.E 数组化

- **Q**: `ComputeUnitTLM` 是否在 B+ 阶段就支持 `num_cus` 数组化，还是延后到 E？
- **状态**: 留待 Phase 7.B/E 设计时确定
- **建议**: B+ 仅支持 1 CU 实例化，E 阶段引入 `std::vector<ComputeUnitTLM>` 数组化

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自 spec §3.1 + plan §3.2）
- **2026-06-11**: B3 批次设计 — 提取 D1-D5 + 蓝图对齐
- **Phase 7.B (未来)**: 实施 `compute_unit_base` + ComputeUnitTLM 真实实现
- **Phase 7.E (未来)**: 数组化（`num_cus=4`）
- **Phase 7.F+ (未来)**: 5-stage pipeline 简化版
