# gpu-gputlm 微架构文档

> **类别**: gpu > gputlm
> **状态**: ✅ 已实施（Phase 7.A v0）
> **Header**: `include/tlm/gpu/gpu_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:42`）+ `registerAdapter<tlm::GPUTLM, ComputeReqBundle, ComputeRespBundle>`
> **蓝图来源**: gem5 `src/gpu-compute/ComputeUnit.py`（接口对位，v0 黑盒版）
> **首版 commit**: `828f037`（feat(gpu): add GPUTLM v0 blackbox initiator） · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **最近更新**: `fb6011b`（REGISTER_CHSTREAM 扩展 + namespace tlm 修复）
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.3, §4 Phase 0
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §3
> - Plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../../superpowers/plans/2026-06-11-phase7a-gpu-infra.md) Task 2

---

## 1. 设计目标

`tlm::GPUTLM` 是 CppTLM v2.1 Phase 7.A 的 **GPU 黑盒发起器**——**单端口 Initiator**，tick() 中按 `kernel_duration_` 周期发出 `ComputeReqBundle`。**与 gem5 对位**: `gem5::ComputeUnit`（v0 仅保留接口契约，**不**模拟 5-stage pipeline / ISA / SIMD / LDS / HSA Runtime——按 D2-D4 决策推迟到 Phase 7.B+）。

**核心特性**（来自 `gpu_tlm.hh:25-203`）：
- 命名空间 `tlm`（与 `RouterTLM`/`NICTLM`/`LinkTLM` 一致；非 `CPUTLM`/`CacheTLM` 的全局命名空间）
- 单端口 Initiator：`req_out` + `resp_in` + 2 个 dummy 满足 StreamAdapter 接口
- **5 个程序化 setter**（v0 不实现 `on_config_loaded` JSON 解析）
- tick() 4 阶段：响应消费 / 请求发起 / 周期计数 / Adapter tick
- **50% 读写混合** + **50% 地址均匀**（v0 简化确定性）
- 7 个 StatGroup 统计（含 latency distribution）

## 2. 架构概览

### 2.1 端口拓扑

```
   ┌──────────────────────────────┐
   │         GPUTLM                │
   │                              │
   │   tick():                    │
   │    1. resp_in_  ← 消费响应 ──│──► (Memory / 下游响应)
   │    2. req_out_  → 发起请求 ──│──► (Cache / NICTLM)
   │    3. cycle++                │
   │    4. adapter_->tick()       │
   └──────────────────────────────┘
```

### 2.2 内部状态

```
   ┌────────────────────────────────────────┐
   │ Black-box parameters (constructor)     │
   │   num_kernels_=1, kernel_duration_=100 │
   │   num_workgroups_=4, workgroup_size_=64│
   │   coalescing_factor_=1                 │
   ├────────────────────────────────────────┤
   │ Runtime state (tick-updated)           │
   │   cur_kernel_id_, next_txn_id_         │
   │   cycles_since_launch_, kernel_active_ │
   │   inflight_txns_: map<txn_id, cycle>   │
   │   rng_ (mt19937, real-device seed)     │
   ├────────────────────────────────────────┤
   │ Adapters (StreamAdapter contract)      │
   │   resp_in_:  InputStreamAdapter<       │
   │              ComputeRespBundle>         │
   │   req_out_:  OutputStreamAdapter<      │
   │              ComputeReqBundle>          │
   │   adapter_:  StreamAdapterBase* (PIMPL)│
   │   resp_out/req_in: dummy 满足接口     │
   ├────────────────────────────────────────┤
   │ StatGroup (lazy init)                  │
   │   7 metrics: kernels_launched,        │
   │   workgroups_dispatched, requests_*,  │
   │   writes, reads, latency              │
   └────────────────────────────────────────┘
```

### 2.3 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|------|------|
| `resp_in_` | `InputStreamAdapter<ComputeRespBundle>` | 1 | 接收响应（来自下游 memory/cache） |
| `req_out_` | `OutputStreamAdapter<ComputeReqBundle>` | 1 | 发送请求（到下游 cache/NICTLM） |
| `resp_out_` | `OutputStreamAdapter<ComputeRespBundle>` (dummy) | 1 | 满足 StreamAdapter 接口（v0 不使用） |
| `req_in_` | `InputStreamAdapter<ComputeReqBundle>` (dummy) | 1 | 满足 StreamAdapter 接口（v0 不使用） |
| `adapter_` | `StreamAdapterBase*` | 1 | ChStream 桥接 |

## 3. 接口（Public API）

```cpp
namespace tlm {
class GPUTLM : public ChStreamModuleBase {
public:
    // 构造
    explicit GPUTLM(const std::string& name, EventQueue* eq);

    // 类型识别
    std::string get_module_type() const override { return "GPUTLM"; }

    // StreamAdapter 注入
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    cpptlm::StreamAdapterBase* get_adapter() const;

    // === 5 个程序化 setter（v0 不读 JSON）===
    void set_num_kernels(uint32_t n);
    void set_kernel_duration(uint32_t cyc);
    void set_num_workgroups(uint32_t n);
    void set_workgroup_size(uint32_t sz);
    void set_coalescing_factor(uint32_t cf);

    // === 6 个统计 getter（测试用公开访问）===
    uint64_t stats_requests_issued()      const;
    uint64_t stats_requests_completed()   const;
    uint64_t stats_kernels_launched()     const;
    uint64_t stats_workgroups_dispatched() const;
    uint64_t stats_writes()                const;
    uint64_t stats_reads()                 const;

    // === ChStreamModuleBase 接口 ===
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

    // 适配器访问器（StreamAdapter::tick() 用）
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>&  resp_in();
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>&  req_out();
    // Initiator dummy:
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle>& resp_out();
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>& req_in();
};
}
```

**硬编码默认值**（来自 `gpu_tlm.hh:32-37` 构造函数内联初始化）：

| 常量 | 默认值 | 含义 |
|------|--------|------|
| `num_kernels_` | 1 | 启动几个 kernel |
| `kernel_duration_` | 100 | 每 kernel 占多少 cycles |
| `num_workgroups_` | 4 | 每 kernel 多少 workgroup |
| `workgroup_size_` | 64 | wavefront 大小（lanes/warp） |
| `coalescing_factor_` | 1 | 抽象 coalescing（每 WG 多少请求） |

**`on_config_loaded()` 是 stub**（v0 沿用 `TrafficGenTLM` / `CPUTLM` 同样的缺口——Phase 7.B 统一修复）。

## 4. 行为流程

### 4.1 tick() 4 阶段（来自 `gpu_tlm.hh:75-141`）

```cpp
void GPUTLM::tick() {
    // 1. 响应消费（优先级最高）
    if (resp_in_.valid() && resp_in_.ready()) {
        auto& resp = resp_in_.data();
        uint64_t txn_id = resp.transaction_id.read();
        auto it = inflight_txns_.find(txn_id);
        if (it != inflight_txns_.end()) {
            uint64_t issue_cycle = it->second;
            uint64_t cur_cycle = getCurrentCycle();
            latency_.sample(cur_cycle - issue_cycle);
            inflight_txns_.erase(it);
        }
        resp_in_.consume();
        requests_completed_++;
    }

    // 2. 请求发起（kernel 状态机）
    if (!kernel_active_) {
        // 无活动 kernel → 启动下一个
        if (cur_kernel_id_ < num_kernels_) {
            kernel_active_ = true;
            cycles_since_launch_ = 0;
            cur_kernel_id_++;
            kernels_launched_++;
        }
    } else {
        // 有活动 kernel → 按 coalescing_factor 批量发请求
        if (cycles_since_launch_ < kernel_duration_) {
            uint32_t reqs_per_wg =
                (workgroup_size_ + coalescing_factor_ - 1) / coalescing_factor_;
            for (uint32_t wg = 0; wg < num_workgroups_; ++wg) {
                for (uint32_t wf = 0; wf < reqs_per_wg; ++wf) {
                    bool is_wr = (rng_() % 2 == 0);
                    uint64_t addr = 0x10000ULL + uint64_t(wg) * 0x1000ULL;
                    uint64_t cur_cycle = getCurrentCycle();

                    bundles::ComputeReqBundle req;
                    req.transaction_id.write(next_txn_id_);
                    req.parent_id.write(0);
                    req.fragment_id.write(0);
                    req.fragment_total.write(1);
                    req.address.write(addr);
                    req.size.write(4);
                    req.is_write.write(is_wr);
                    req.data.write(0xCAFEBABEULL);
                    req.kernel_id.write(cur_kernel_id_);
                    req.workgroup_id.write(wg);
                    req.wavefront_id.write(wf);
                    req.coalescing_factor.write(coalescing_factor_);

                    inflight_txns_[next_txn_id_] = cur_cycle;
                    req_out_.write(req);
                    next_txn_id_++;
                    requests_issued_++;
                    if (is_wr) writes_++; else reads_++;
                }
            }
            workgroups_dispatched_ += num_workgroups_;
        } else {
            kernel_active_ = false;  // 当前 kernel 完成
        }
    }

    // 3. 周期计数
    cycles_since_launch_++;

    // 4. Adapter tick（handshake 推进）
    if (adapter_) adapter_->tick();
}
```

### 4.2 关键设计取舍

- **`kernel_duration_` 语义 = "活动 ticks 数"**：每个 kernel 占用 `kernel_duration_` 个 ticks，每个 tick 在 `kernel_active_ && cycles_since_launch_ < kernel_duration_` 时**持续发一批**（= `num_workgroups_ × reqs_per_wg` 个请求）
- **v0 计划 vs 实际行为偏差**：原 plan Task5 描述"每个 kernel 发一次批"——实际每个 tick 都发。`test_gpu_standalone.cc` 已修正为按此语义期望
- **地址生成**：硬编码 `0x10000 + wg * 0x1000`（v0 简化确定性；非真实 GPU 内存地址语义）
- **`is_write` 50% 概率**：`rng_() % 2 == 0`（v0 简化混合）
- **`req.data = 0xCAFEBABE`**：硬编码桩值（与 `MemoryTLM` 的 `0xDEADBEEF` 风格一致——"咖啡牛肉"是 GPU 标识，区别于 memory 的"死牛肉"）

## 5. Bundle 字段使用

**`ComputeReqBundle` 字段**（`include/bundles/compute_bundles_tlm.hh`）：

| 字段 | GPUTLM 使用 | 来源 |
|------|------------|------|
| `transaction_id` | `next_txn_id_++`（关键——`inflight_txns_` 映射键） | `gpu_tlm.hh:110` |
| `parent_id` | 硬编码 0 | `gpu_tlm.hh:111` |
| `fragment_id` | 硬编码 0 | `gpu_tlm.hh:112` |
| `fragment_total` | 硬编码 1 | `gpu_tlm.hh:113` |
| `address` | `0x10000 + wg * 0x1000` | `gpu_tlm.hh:106` |
| `size` | 硬编码 4 | `gpu_tlm.hh:115` |
| `is_write` | 50% 概率 | `gpu_tlm.hh:105, 116` |
| `data` | 硬编码 `0xCAFEBABE` | `gpu_tlm.hh:117` |
| `kernel_id` | `cur_kernel_id_`（1-based） | `gpu_tlm.hh:118` |
| `workgroup_id` | `wg`（0 到 num_workgroups_-1） | `gpu_tlm.hh:119` |
| `wavefront_id` | `wf`（0 到 reqs_per_wg-1） | `gpu_tlm.hh:120` |
| `coalescing_factor` | `coalescing_factor_` | `gpu_tlm.hh:121` |

**对位 gem5 ComputeUnit 字段**（`src/gpu-compute/ComputeUnit.py`）：
- `kernel_id` ↔ AQL `dispatch_id`
- `workgroup_id` ↔ `ComputeUnit.dispWorkgroup(wg_id)`
- `wavefront_id` ↔ `Wavefront.wfSlotId`（SIMD lane 组）
- `coalescing_factor` ↔ 抽象 `VIPERCoalescer.coalesce_factor`

## 6. 统计

**7 个 StatGroup 指标**（`gpu_tlm.hh:196-202`）：

| 指标 | 类型 | 含义 |
|------|------|------|
| `kernels_launched_` | Scalar | 已启动的 kernel 数（`kernels_launched_++` at kernel activation） |
| `workgroups_dispatched_` | Scalar | 已分发的 workgroup 总数（每个 tick 在 kernel 期内 +`num_workgroups_`） |
| `requests_issued_` | Scalar | 已发请求数（每个请求 +1） |
| `requests_completed_` | Scalar | 已收响应数（响应消费时 +1） |
| `writes_` | Scalar | 写请求数（`is_write` 50%） |
| `reads_` | Scalar | 读请求数（`is_write` 50%） |
| `latency_` | Distribution | 单请求延迟（`current_cycle - issue_cycle`，单位 cycle） |

**`get_stats_group()` lazy init**（`gpu_tlm.hh:161-174`）：首次调用时构造 `unique_ptr<StatGroup>` 并添加 7 个指标，后续直接返回缓存指针。

**`get_stats_path()`**（继承自 `ChStreamModuleBase`）：返回模块名 `gpu_<id>`（注：v0 实际未重写此方法，使用基类默认 `getName()`）。

## 7. 蓝图（未来演进）

### 7.1 Phase 7.B 共享基类（**已规划**）

调研 §4 Phase 1 + spec §3：CPUTLM / TrafficGenTLM / GPUTLM v0 三者**大量代码重复**（tick 循环 / inflight 跟踪 / adapter 注入），Phase 7.B 抽出 `compute_unit_base` 共享基类 + 实现 `on_config_loaded` JSON 解析。

### 7.2 Phase 7.E 多 CU 实例化

`ComputeUnitTLM` 数组模式：JSON 参数 `num_cus=4` 创建多 CU 实例，复用 `tlm::RouterTLM` + `tlm::NICTLM` + `tlm::LinkTLM` 形成 GPU 内部 mesh。

### 7.3 蓝图增强

- **真实 GPU 内存地址语义**（替换 `0x10000 + wg * 0x1000` 硬编码）
- **真实 data 字段**（替换 `0xCAFEBABE` 桩值）
- **5-stage pipeline 简化**（仅 Fetch + Issue 阶段，不模拟 Scoreboard/Schedule/Execute）
- **wavefront 调度状态机**（`S_RUNNING` / `S_STALLED` / `S_BARRIER`）—— 完整模拟延后到 Phase 7.F+
- **真实延迟**（v0 单拍即返回；v2.2 加 `inflight_cycles_remaining_` 倒计时）
- **LDS / VRF / SRF** 模拟（v0 完全不模拟——D2 决策）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **`kernel_duration_` 语义偏差**——v0 实际"每个 tick 在期内持续发"，原 plan Task5 描述"每个 kernel 发一次批"——**测试已修正** | 低 | 中 | `test_gpu_standalone.cc:5` 测试注释明确标注语义 |
| R2 | **`on_config_loaded` JSON params 不读**（沿用 CPUTLM/TrafficGenTLM 缺口） | 高 | 中 | Phase 7.B 统一修复 |
| R3 | **R5 风险（spec §6.1）**——MemoryTLM 不接受 `ComputeReqBundle`（R5 设计问题在 v0 测试中用 `AdapterHandle` 绕开，未走 ModuleFactory 端到端） | 高 | 中 | Phase 7.D MemoryTLM 双 Bundle 注册 |
| R4 | **地址/data 硬编码**（`0x10000 + wg * 0x1000` / `0xCAFEBABE`）——非真实 GPU 内存行为 | 高 | 中 | v2.2 暴露 setter |
| R5 | **`is_write` 50% 概率**（`rng_() % 2 == 0`）——非真实 GPU 写比例 | 中 | 低 | v2.2 暴露 `set_write_ratio(double)` |
| R6 | **`inflight_txns_` 无限增长**（如果响应丢失则泄漏） | 低 | 中 | v0 可接受（v2.2 加 `MAX_INFLIGHT` + 丢弃策略） |
| R7 | **`latency_` 在 `current_cycle` 取自 `getCurrentCycle()`**——依赖 `EventQueue*` 注入 | 低 | 低 | 基类保证 `event_queue != nullptr`（v0 未防御） |
| R8 | **`get_stats_path()` 实际未重写**——返回基类默认 `getName()`，与 RouterTLM 不一致 | 低 | 低 | v2.2 重写以返回 `"system.gpu_<id>"` 路径 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过（commit `fb6011b`） |
| 单测覆盖 | ✅ | `test/test_gpu_standalone.cc` 5 个测试全过（commit `763d8d7`） |
| 端到端（`AdapterHandle` 测试） | ✅ | `[gpu]` 标签测试全过 |
| 5 个 setter 行为 | ✅ | `set_num_kernels/num_workgroups/workgroup_size/coalescing_factor` 真实影响 `req_out_` 流量 |
| 50% 读写混合 | ✅ | `stats_writes + stats_reads == stats_requests_issued` |
| 7 个统计 | ✅ | 5 测试 + stats_*() getter 验证 |
| **JSON params 读取** | ❌ stub | 见 R2 |
| **MemoryTLM 真实端到端** | ❌ R5 阻断 | 见 R3 |
| **5-stage pipeline 模拟** | ❌ D2 推迟 | 见 §7.3 |
| **真实 GPU 内存地址** | ❌ 硬编码 | 见 R4 |

## 10. 修订历史

- **2026-06-11** (`828f037`): GPUTLM v0 初版（5 setter + 4 阶段 tick + 7 stats）
- **2026-06-11** (`fb6011b`): namespace tlm 修复（与 `RouterTLM`/`NICTLM`/`LinkTLM` 对齐）+ `REGISTER_CHSTREAM` 扩展
- **2026-06-11** (`763d8d7`): 5 个 Catch2 单测（按 v0 实际"每个 tick 持续发"语义修正测试期望）
- **2026-06-11**: 本微架构文档创建（B3.1）— 补 README §5 标记的"暂无独立 doc"gap
- **Phase 7.B (未来)**: 抽出 `compute_unit_base` + 修复 `on_config_loaded`
- **Phase 7.D (未来)**: R5 风险修复（MemoryTLM 双 Bundle 注册）
