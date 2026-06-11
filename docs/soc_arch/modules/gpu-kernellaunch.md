# gpu-kernellaunch 微架构文档

> **类别**: gpu > kernellaunch
> **状态**: 🟡 规划中（Phase 7.B）
> **Header**: (规划) `include/tlm/gpu/kernel_launch_tlm.hh`
> **注册**: (规划) `REGISTER_CHSTREAM` 扩展 `ModuleFactory::registerObject<tlm::KernelLaunchTLM>("KernelLaunchTLM")`
> **蓝图来源**: gem5 `src/dev/hsa/hsa_packet_processor.py` + `src/gpu-compute/dispatcher.py`（**简化版**）
> **首版 commit**: 🟡 蓝图（来自 spec §3.2 + plan Task 8+）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §3.2
> - Plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../../superpowers/plans/2026-06-11-phase7a-gpu-infra.md) §3.2
> - 通用 GPU 概念: [gpu.common.md](./gpu.common.md)
> - 真实 CU: [gpu-compute_unit.md](./gpu-compute_unit.md)

---

## 1. 设计目标（规划）

`tlm::KernelLaunchTLM` 是 Phase 7.B 引入的**简化版 HSA Dispatcher 替代**——按 **D4 决策**（"简化到极致"），用 ~150 行代码代替 gem5 `HSAPacketProcessor` + `GPUCommandProcessor` + `GPUDispatcher` 三件套共 3000+ 行的真实 APU 启动通路。

**核心特征**：
- **无 PIO doorbell 监听**——v0 抽象，host → GPU 唤醒信号不模拟
- **无 AQL 包解析**——dispatch/agent/barrier/vendor 四种 64-byte 包不解析
- **无 scratch pool 管理**——scratch lazy allocation 不模拟
- **无 VM fault 处理**——page fault 跨地址翻译不模拟
- **tick() 周期性向 ComputeUnitTLM 发 kernel launch 命令**——按 `kernel_launch_interval_` 触发

**与 gem5 对位**: `gem5::HSAPacketProcessor` + `gem5::GPUCommandProcessor` + `gem5::GPUDispatcher`（**仅接口** — v0 简化为单方法 `enqueue_kernel()`）。

## 2. 架构概览（规划）

```
┌─────────────────────────────────────────────────────────────┐
│               KernelLaunchTLM (Phase 7.B)                   │
│                                                             │
│  ┌──────────────────────────────────────────────────┐      │
│  │ 简化版 HSA Dispatcher (~150 行)                  │      │
│  │                                                    │      │
│  │  - std::vector<KernelLaunchDesc> launch_queue_  │      │
│  │  - launch_interval_ (默认 100 cyc)             │      │
│  │  - tick() 中按 interval 触发 launch              │      │
│  │  - enqueue_kernel() 程序化 API                    │      │
│  └──────────────────────────────────────────────────┘      │
│                            ↓ 调用                            │
│  ┌──────────────────────────────────────────────────┐      │
│  │ 接受方: ComputeUnitTLM (Phase 7.B 实施)         │      │
│  │   enqueue_workgroup(WorkGroup wg)                │      │
│  └──────────────────────────────────────────────────┘      │
│                                                             │
│  端口: 1 个 OutputStreamAdapter<KernelLaunchCmd>            │
│   （与 ComputeUnitTLM 的 InputStreamAdapter 配对）            │
└─────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {

// Kernel launch descriptor (简化版 AQL 替代)
struct KernelLaunchDesc {
    uint32_t kernel_id;
    uint32_t num_workgroups;
    uint32_t workgroup_size;
    uint32_t coalescing_factor;
    uint64_t kernarg_addr;     // 模拟
    uint32_t lds_size;         // 模拟
};

class KernelLaunchTLM : public ChStreamModuleBase {
public:
    explicit KernelLaunchTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "KernelLaunchTLM"; }

    // === 配置 ===
    void set_launch_interval(uint32_t cyc) { launch_interval_ = cyc; }

    // === on_config_loaded (真实 JSON 解析，Phase 7.B 修复 R3) ===
    void on_config_loaded() override;

    // === 程序化 API (替代 HSA doorbell PIO) ===
    void enqueue_kernel(KernelLaunchDesc desc);

    // === ChStream 桥接 (向 ComputeUnitTLM 发 launch cmd) ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

    // 适配器访问器
    cpptlm::OutputStreamAdapter<KernelLaunchDesc>& cmd_out() { return cmd_out_; }
    cpptlm::StreamAdapterBase* get_adapter() const { return adapter_; }

private:
    cpptlm::OutputStreamAdapter<KernelLaunchDesc> cmd_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    std::vector<KernelLaunchDesc> launch_queue_;
    uint32_t launch_interval_ = 100;
    uint32_t cycles_since_launch_ = 0;

    // 统计
    tlm_stats::Scalar kernels_launched_;
    tlm_stats::Scalar workgroups_dispatched_;
    tlm_stats::Distribution launch_latency_;
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 2 阶段

```cpp
void KernelLaunchTLM::tick() {
    // 阶段 1: 周期性 launch
    if (cycles_since_launch_ >= launch_interval_) {
        if (!launch_queue_.empty()) {
            KernelLaunchDesc desc = launch_queue_.front();
            launch_queue_.erase(launch_queue_.begin());

            // 推送给 ComputeUnitTLM (通过 OutputStreamAdapter)
            cmd_out_.write(desc);
            kernels_launched_++;
            workgroups_dispatched_ += desc.num_workgroups;
        }
        cycles_since_launch_ = 0;
    }
    cycles_since_launch_++;

    // 阶段 2: Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 on_config_loaded() 真实实现

```cpp
void KernelLaunchTLM::on_config_loaded() {
    const json& cfg = get_config();

    // 读 launch_interval
    if (cfg.contains("launch_interval")) {
        set_launch_interval(cfg["launch_interval"]);
    }

    // 预填 launch_queue (按 launch_descs 列表)
    if (cfg.contains("launch_descs") && cfg["launch_descs"].is_array()) {
        for (const auto& item : cfg["launch_descs"]) {
            KernelLaunchDesc desc;
            desc.kernel_id        = item.value("kernel_id", 0);
            desc.num_workgroups   = item.value("num_workgroups", 4);
            desc.workgroup_size   = item.value("workgroup_size", 64);
            desc.coalescing_factor = item.value("coalescing_factor", 1);
            desc.kernarg_addr     = item.value("kernarg_addr", 0);
            desc.lds_size         = item.value("lds_size", 0);
            launch_queue_.push_back(desc);
        }
    }
}
```

### 4.3 关键设计取舍

- **无 PIO doorbell 监听**（D4 决策）——host 启动 GPU 完全不模拟
- **无 AQL 包解析**——dispatch/agent/barrier/vendor 不区分
- **周期性而非事件驱动**——按 `launch_interval_` tick 触发 launch
- **JSON 真实解析**（B 修复 v0 R3 缺口）
- **不模拟 scratch pool**——v0 简化
- **不模拟 VM fault**——v0 简化
- **接受方**：ComputeUnitTLM（Phase 7.B 同周实施）

## 5. Bundle 字段使用（规划）

**KernelLaunchDesc 字段**（**非** ComputeReqBundle，是新结构）：

| 字段 | 含义 | 与 gem5 AQL 对位 |
|------|------|------------------|
| `kernel_id` | kernel 标识 | AQL `dispatch_id` |
| `num_workgroups` | 1 kernel 多少 WG | AQL `workgroup_size_x/y/z` 之积 |
| `workgroup_size` | 1 WG 多少 lane | AQL `workgroup_size_x` |
| `coalescing_factor` | 抽象 coalescing | (CppTLM 自创，gem5 无对位) |
| `kernarg_addr` | 模拟 kernel 参数地址 | AQL `kernarg_address` |
| `lds_size` | 模拟 LDS 大小 | AQL `group_segment_size` |

**与 ComputeReqBundle 区别**：
- `KernelLaunchDesc` = launch-time 元数据（一次性）
- `ComputeReqBundle` = 运行时 memory 请求（持续）

## 6. 蓝图对齐

- gem5 `src/dev/hsa/hsa_packet_processor.py`（HSAPP PIO doorbell — 简化）
- gem5 `src/gpu-compute/dispatcher.py`（Dispatcher — 简化）
- gem5 `src/dev/hsa/gpu_command_processor.py`（GCP scratch/lazy alloc — 简化）
- spec §3.2（KernelLaunchTLM 蓝图章节）
- plan §3.2（Phase 7.B 实施路径）

## 7. 实施路径

### 7.1 Phase 7.B 步骤

1. 新建 `include/tlm/gpu/kernel_launch_tlm.hh`（~150 行）
2. 修改 `include/chstream_register.hh`：
   - 加 `#include "tlm/gpu/kernel_launch_tlm.hh"`
   - 加 `ModuleFactory::registerObject<tlm::KernelLaunchTLM>("KernelLaunchTLM");`
   - 加 `ChStreamAdapterFactory::registerAdapter<tlm::KernelLaunchTLM, KernelLaunchDesc, void>("KernelLaunchTLM");`（注：响应 Bundle 类型 = void，因为这是单向 launch）
3. 写 `on_config_loaded` 真实 JSON 解析
4. 与 `ComputeUnitTLM` 同周联调（同一 `tick()` 流程）
5. 加 Catch2 测试：`test/test_kernel_launch.cc`（`[gpu]` 标签），覆盖：
   - 单 launch 触发
   - 多 launch 队列
   - JSON on_config_loaded
   - launch_interval 节流
6. 更新 `AGENTS.md` + `docs/ONBOARDING.md`（KernelLaunchTLM 注册条目）
7. 更新 `docs/soc_arch/modules/README.md`（新增 `gpu-kernellaunch.md` 链接）

### 7.2 验收标准

- [ ] 编译通过（Release + Debug）
- [ ] `cpptlm_tests "[gpu]"` 全部通过（5 + 3 + 3 = 11 个 GPU 测试）
- [ ] `cpptlm --config configs/kernel_launch_test.json` 端到端可执行
- [ ] `docs_sync_check.sh --strict` 通过
- [ ] 零 TODO/FIXME/XXX in new files
- [ ] **总行数 ≤ 200**（与 D4 决策"简化到极致"对齐）

### 7.3 估计工作量

- 设计: 0.5 周（接口简单）
- 实施: 0.5-1 周
- 测试: 0.5 周
- 文档: 0.25 周
- **总计: 1.5-2.5 周**（比 ComputeUnitTLM 简单）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **过度简化**——可能漏掉真实 APU 启动行为（HSA agent 协商、scratch 调整） | 中 | 中 | D4 决策接受；Phase 7.F+ 渐进增强 |
| R2 | **无 Doorbell 监听**——host → GPU 唤醒信号丢失 | 中 | 中 | D4 决策接受；用 JSON `launch_descs` 预填代替 |
| R3 | **响应 Bundle 类型 void**——与其他 adapter 注册不一致 | 中 | 低 | 注册时显式标注 `void`；ChStreamAdapterFactory 支持 |
| R4 | **与 ComputeUnitTLM 同步**——接口契约变更 | 中 | 中 | 同周设计 + 联调测试 |
| R5 | **JSON `launch_descs` 数组**——v0 无外部测试 | 中 | 低 | B+ 阶段加 2-3 个典型 config（rocm-like 启动模式） |
| R6 | **`launch_interval_` 硬编码默认 100** | 低 | 低 | v0 简化；可由 on_config_loaded 覆盖 |
| R7 | **launch_queue_ 无限增长**——on_config_loaded 加载过多 | 低 | 中 | B+ 阶段加 `MAX_LAUNCH_QUEUE` |
| R8 | **launch_latency_ 永不更新**（v0 抽象） | 中 | 低 | v0 不更新；与 GPUTLM 一致 |

## 9. 设计决策点

### D1 launch_interval 触发 vs 事件驱动

- **Q**: 周期性 tick 触发 launch，还是收到 ComputeReqBundle 事件触发？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 周期性（与 ComputeUnitTLM kernel_duration_ 概念对齐）
- **依赖**: ComputeUnitTLM D3 决策

### D2 KernelLaunchDesc vs AQL dispatch_packet 字段对齐

- **Q**: KernelLaunchDesc 应包含哪些字段？仅 minimum 集还是完整 AQL 子集？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 6 字段最小集（kernel_id / num_workgroups / workgroup_size / coalescing_factor / kernarg_addr / lds_size）
- **依赖**: gem5 `src/dev/hsa/hsa_packet.hh:_hsa_dispatch_packet_t`（14 字段，v0 子集）

### D3 响应 Bundle 类型

- **Q**: 接受方 ComputeUnitTLM 是否需要响应 launch 接受状态？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 不需要（单向 fire-and-forget，ComputeUnitTLM 总是接受）
- **替代**: 若需要，响应类型 = `KernelLaunchAckDesc`（仅含 `kernel_id` + `accepted`）

### D4 launch_queue_ 容量

- **Q**: 静态预填（on_config_loaded 一次性）还是动态 enqueue？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 静态预填（v0 简化）

### D5 launch_interval 粒度

- **Q**: launch_interval_ 是按 cycle 数还是按 wall-clock 时间？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: cycle 数（与 GPUTLM kernel_duration_ 一致）

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自 spec §3.2 + plan §3.2）
- **2026-06-11**: B3 批次设计 — 提取 D1-D5 + 蓝图对齐
- **Phase 7.B (未来)**: 实施 KernelLaunchTLM（≤200 行）
- **Phase 7.F+ (未来)**: 渐进增强 HSA AQL 真实实现
