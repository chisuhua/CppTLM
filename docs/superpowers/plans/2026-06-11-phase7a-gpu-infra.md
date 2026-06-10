# Phase7.A — GPU 基础设施实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CppTLM v2.1 中建立 GPU Bundle 类型 + GPUTLM v0 黑盒发起器 + 注册 + 配置文件，端到端验证 GPU 消息能在 StreamAdapter 管线中流通。

**Architecture:** 沿用现有 CacheReqBundle 模式定义 `ComputeReqBundle`/`ComputeRespBundle`（在 CacheReq/Resp 字段基础上加 4 个 GPU 维度字段：`kernel_id`/`workgroup_id`/`wavefront_id`/`coalescing_factor`）。新建 `tlm::GPUTLM` 继承 `ChStreamModuleBase`，作为单端口 Initiator（与 CPUTLM/TrafficGenTLM 同型），tick() 中按 `kernel_duration_` 周期发出 ComputeReqBundle。`REGISTER_CHSTREAM` 宏体扩展 2 行（1 个 registerObject + 1 个 registerAdapter）。

**Tech Stack:** C++17, Catch2 v3.7.0 (预编译 2 文件), CMake + Ninja, `bundles::` + `tlm::` 命名空间, `ch_uint<N>`/`ch_bool` Bundle 字段, `tlm_stats::Scalar`/`Distribution` StatGroup, `StreamAdapter<ModuleT, Req, Resp>` 单端口适配器, JSON 拓扑配置。

**Reference Documents:**
- Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../specs/2026-06-11-phase7a-gpu-infra-design.md) (IMPL-011-Phase7.A, 660 lines, 10 sections)
- Research: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) (Phase 0 描述)
- Parent roadmap: `roadmap.md` Phase 7.A

---

## File Structure

| File | Responsibility | Status |
|------|----------------|--------|
| `include/bundles/compute_bundles_tlm.hh` | `ComputeReqBundle` / `ComputeRespBundle` 类型定义 | Create |
| `include/tlm/gpu/gpu_tlm.hh` | `GPUTLM` v0 黑盒发起器类 | Create |
| `include/chstream_register.hh` | GPUTLM 注册扩展（+5 行） | Modify |
| `configs/gpu_standalone.json` | 端到端验证配置 | Create |
| `test/test_gpu_standalone.cc` | Catch2 单元测试（3 测试） | Create |
| `AGENTS.md` | STRUCTURE 节添加 `include/tlm/gpu/` | Modify |
| `docs/ONBOARDING.md` | GPU 模块路径说明 | Modify |
| `scripts/test/docs_sync_check.sh` | VIRTUAL_PATHS 扩展（+4 路径） | Modify |

**Total: 4 new files + 4 modified files, ~520 LOC.**

---

## Task 1: ComputeReqBundle / ComputeRespBundle 头文件

**Files:**
- Create: `include/bundles/compute_bundles_tlm.hh`

- [ ] **Step 1.1: 创建头文件**

创建 `include/bundles/compute_bundles_tlm.hh`：

```cpp
// include/bundles/compute_bundles_tlm.hh
// Compute Bundle 定义（GPU 发起请求 / 响应，轻量级 TLM 侧）
// 功能描述：定义 GPU Compute 请求/响应 Bundle，在 CacheReq/Resp 字段基础上
//           扩展 GPU 维度字段（kernel_id / workgroup_id / wavefront_id /
//           coalescing_factor），支持 Phase7.A 端到端 GPU 消息流通验证。
//           沿用 bundles::bundle_base 基类，POD 兼容 bundle_serialization.hh。
// 作者 CppTLM Team / 日期 2026-06-11
// 参考：docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md §2
//      gem5 src/gpu-compute/ComputeUnit.py (字段语义对位)
#ifndef BUNDLES_COMPUTE_BUNDLES_TLM_HH
#define BUNDLES_COMPUTE_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief GPU Compute 请求 Bundle（轻量级 TLM 侧）
 *
 * 字段分为两部分：
 *   - 继承自 CacheReqBundle 风格的 8 个核心字段（transaction 元数据 + 地址/数据）
 *   - GPU 特定字段：kernel_id / workgroup_id / wavefront_id / coalescing_factor
 *
 * 与 CacheReqBundle 的差异：
 *   - 字段集为 CacheReqBundle 的超集（组合而非继承，保持 POD 兼容 memcpy）
 *   - GPU 字段对位 gem5：kernel_id ≈ AQL dispatch_id，workgroup_id ≈ ComputeUnit
 *     dispWorkgroup(wg_id)，wavefront_id ≈ Wavefront.wfSlotId，coalescing_factor
 *     抽象 VIPERCoalescer::coalesce_factor
 */
struct ComputeReqBundle : public bundle_base {
    // === 核心字段（与 CacheReqBundle 对位）===
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;        // 0 = 根事务
    ch_uint<8>  fragment_id;      // 当前拍序号（0-based）
    ch_uint<8>  fragment_total;   // 总拍数（1 = 不分片）
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;

    // === GPU 特定字段 ===
    ch_uint<32> kernel_id;
    ch_uint<32> workgroup_id;
    ch_uint<32> wavefront_id;
    ch_uint<32> coalescing_factor;

    ComputeReqBundle() = default;

    ComputeReqBundle(uint64_t tid, uint32_t kid, uint32_t wgid, uint32_t wfid,
                     uint64_t addr, uint8_t sz, bool wr, uint64_t d,
                     uint32_t cf = 1)
        : transaction_id(tid), parent_id(0), fragment_id(0), fragment_total(1)
        , address(addr), size(sz), is_write(wr), data(d)
        , kernel_id(kid), workgroup_id(wgid), wavefront_id(wfid)
        , coalescing_factor(cf) {}

    bool is_first_fragment() const { return fragment_id.read() == 0; }
    bool is_last_fragment() const {
        return fragment_id.read() + 1 >= fragment_total.read();
    }
    bool is_root() const {
        return parent_id.read() == 0 && fragment_total.read() == 1;
    }
};

/**
 * @brief GPU Compute 响应 Bundle（轻量级 TLM 侧）
 *
 * 字段：8 个核心字段 + 3 个 GPU 维度字段（kernel_id / workgroup_id / wavefront_id）。
 * 注：响应侧不需要 coalescing_factor（合并是请求侧的事）。
 */
struct ComputeRespBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;
    ch_uint<8>  fragment_id;
    ch_uint<8>  fragment_total;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;
    ch_bool     first;
    ch_bool     last;

    ch_uint<32> kernel_id;
    ch_uint<32> workgroup_id;
    ch_uint<32> wavefront_id;

    ComputeRespBundle() = default;

    ComputeRespBundle(uint64_t tid, uint32_t kid, uint32_t wgid, uint32_t wfid,
                      uint64_t d, bool hit, uint8_t err = 0)
        : transaction_id(tid), parent_id(0), fragment_id(0), fragment_total(1)
        , data(d), is_hit(hit), error_code(err), first(true), last(true)
        , kernel_id(kid), workgroup_id(wgid), wavefront_id(wfid) {}

    bool is_first_fragment() const {
        return fragment_id.read() == 0 || first.read();
    }
    bool is_last_fragment() const {
        return fragment_id.read() + 1 >= fragment_total.read() || last.read();
    }
};

} // namespace bundles

#endif // BUNDLES_COMPUTE_BUNDLES_TLM_HH
```

- [ ] **Step 1.2: 验证编译（仅头文件，无新 .cc）**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

预期：编译通过，无新增 warning（仅 Include 头文件不影响编译产物）。

- [ ] **Step 1.3: 提交**

```bash
git add include/bundles/compute_bundles_tlm.hh
git commit -m "feat(gpu): add ComputeReqBundle/ComputeRespBundle types

- New include/bundles/compute_bundles_tlm.hh (~80 lines)
- ComputeReqBundle: 8 core fields + 4 GPU fields
  (kernel_id, workgroup_id, wavefront_id, coalescing_factor)
- ComputeRespBundle: 8 core fields + 3 GPU fields
- POD-compatible (inherits bundle_base, memcpy-safe)
- No registration yet (Task 2-3); Bundle type definition only
- Part of Phase7.A (IMPL-011-Phase7.A)"
```

---

## Task 2: GPUTLM v0 头文件骨架

**Files:**
- Create: `include/tlm/gpu/gpu_tlm.hh`

- [ ] **Step 2.1: 创建头文件**

创建 `include/tlm/gpu/gpu_tlm.hh`：

```cpp
// include/tlm/gpu/gpu_tlm.hh
// GPUTLM v0 — GPU 黑盒发起器（ChStreamModuleBase 派生）
// 功能描述：作为单端口 Initiator，tick() 中按 kernel_duration_ 周期发出
//           ComputeReqBundle。v0 不模拟 SIMD pipeline / ISA / LDS / HSA
//           Runtime（D2/D3/D4 决策：推迟到 Phase7.B+）。
//           5 个程序化 setter 控制 num_kernels / kernel_duration /
//           num_workgroups / workgroup_size / coalescing_factor。
// 作者 CppTLM Team / 日期 2026-06-11
// 参考：docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md §3
//      gem5 src/gpu-compute/ComputeUnit.py (字段语义对位)
//      gem5 src/gpu-compute/Wavefront.py (status_e 状态机延后)
#ifndef TLM_GPU_GPU_TLM_HH
#define TLM_GPU_GPU_TLM_HH

#include "core/sim_object.hh"
#include "core/chstream_module.hh"
#include "core/event_queue.hh"
#include "bundles/compute_bundles_tlm.hh"
#include "framework/stream_adapter.hh"
#include "metrics/stats.hh"
#include "metrics/stats_manager.hh"
#include <memory>
#include <unordered_map>
#include <random>
#include <cstdint>

namespace tlm {

/**
 * @brief GPUTLM v0 — GPU 黑盒发起器
 *
 * 单端口 Initiator（req_out + resp_in），与 TrafficGenTLM / CPUTLM 同型。
 * v0 行为：
 *   - 每个 kernel 在 kernel_duration_ 周期内发起
 *     num_workgroups × ceil(workgroup_size / coalescing_factor) 个请求
 *   - 地址生成：固定 0x10000 + wg_id × 0x1000
 *   - 50% 读写混合（与 TrafficGenTLM::RANDOM 一致）
 *   - inflight_txns_ 跟踪 pending 请求
 *   - StatGroup 统计：kernels_launched / workgroups_dispatched /
 *     requests_issued / requests_completed / latency
 */
class GPUTLM : public ChStreamModuleBase {
public:
    GPUTLM(const std::string& name, EventQueue* eq = nullptr)
        : ChStreamModuleBase(name, eq)
        , adapter_(nullptr)
        , rng_(std::random_device{}()) {}

    ~GPUTLM() override = default;

    // JSON 参数读取（v0 stub，Phase7.B 真正实现）
    void on_config_loaded() override {
        // v0 故意不读 JSON params —— 所有行为由构造函数硬编码默认决定。
        // Phase7.B 修复 TrafficGenTLM / CPUTLM / GPUTLM 三者统一的 params 读取。
    }

    // 主循环
    void tick() override;

    // StreamAdapter 注入（ChStreamModuleBase 抽象）
    void set_stream_adapter(StreamAdapterBase* adapter) override;
    StreamAdapterBase* stream_adapter() const override { return adapter_; }

    // === 黑盒行为参数（程序化 setter）===
    void set_num_kernels(uint32_t n) { num_kernels_ = n; }
    void set_kernel_duration(uint32_t cyc) { kernel_duration_ = cyc; }
    void set_num_workgroups(uint32_t n) { num_workgroups_ = n; }
    void set_workgroup_size(uint32_t sz) { workgroup_size_ = sz; }
    void set_coalescing_factor(uint32_t cf) { coalescing_factor_ = cf; }

    // === 统计 getter（测试使用）===
    uint64_t stats_requests_issued() const { return requests_issued_.value(); }
    uint64_t stats_requests_completed() const { return requests_completed_.value(); }
    uint64_t stats_kernels_launched() const { return kernels_launched_.value(); }
    uint64_t stats_workgroups_dispatched() const { return workgroups_dispatched_.value(); }
    uint64_t stats_writes() const { return writes_.value(); }
    uint64_t stats_reads() const { return reads_.value(); }

    // === 统计（StatGroup）===
    std::unique_ptr<tlm_stats::StatGroup> get_stats_group() override;

    // === 重置 ===
    void do_reset() override {
        cur_kernel_id_ = 0;
        cur_workgroup_id_ = 0;
        cur_wavefront_id_ = 0;
        next_txn_id_ = 1;
        cycles_since_launch_ = 0;
        kernel_active_ = false;
        inflight_txns_.clear();
        kernels_launched_.reset();
        workgroups_dispatched_.reset();
        requests_issued_.reset();
        requests_completed_.reset();
        writes_.reset();
        reads_.reset();
        latency_.reset();
    }

private:
    StreamAdapterBase* adapter_;

    // === 黑盒参数默认值 ===
    uint32_t num_kernels_       = 1;
    uint32_t kernel_duration_   = 100;
    uint32_t num_workgroups_    = 4;
    uint32_t workgroup_size_    = 64;
    uint32_t coalescing_factor_ = 1;

    // === 运行期状态 ===
    uint32_t cur_kernel_id_       = 0;
    uint32_t cur_workgroup_id_    = 0;
    uint32_t cur_wavefront_id_    = 0;
    uint64_t next_txn_id_         = 1;
    uint32_t cycles_since_launch_ = 0;
    bool     kernel_active_       = false;

    std::unordered_map<uint64_t, uint64_t> inflight_txns_; // txn_id → issue_cycle

    std::mt19937 rng_;

    // === 统计 ===
    tlm_stats::Scalar       kernels_launched_{"kernels_launched"};
    tlm_stats::Scalar       workgroups_dispatched_{"workgroups_dispatched"};
    tlm_stats::Scalar       requests_issued_{"requests_issued"};
    tlm_stats::Scalar       requests_completed_{"requests_completed"};
    tlm_stats::Scalar       writes_{"writes"};
    tlm_stats::Scalar       reads_{"reads"};
    tlm_stats::Distribution latency_{"latency"};
};

} // namespace tlm

#endif // TLM_GPU_GPU_TLM_HH
```

- [ ] **Step 2.2: 创建 tick() 实现文件占位（header-only 暂用 inline）**

在头文件 `class GPUTLM { ... };` 之前添加 inline 实现（GPUTLM v0 为 header-only，遵循 CPUTLM/TrafficGenTLM 风格）：

将以下实现紧接在 `private:` 段之前、`// === 统计 ===` 之后插入：

```cpp
public:
    inline void tick() override {
        // 1. 响应消费
        if (adapter_ != nullptr) {
            auto* typed_adapter = dynamic_cast<StreamAdapter<GPUTLM,
                bundles::ComputeReqBundle, bundles::ComputeRespBundle>*>(adapter_);
            if (typed_adapter != nullptr && typed_adapter->resp_in() != nullptr
                && typed_adapter->resp_in()->valid()
                && typed_adapter->resp_in()->ready()) {
                bundles::ComputeRespBundle resp;
                typed_adapter->resp_in()->read(resp);
                auto it = inflight_txns_.find(resp.transaction_id.read());
                if (it != inflight_txns_.end()) {
                    uint64_t issue_cycle = it->second;
                    uint64_t cur_cycle = (event_queue() != nullptr)
                        ? event_queue()->getCurrentCycle() : 0;
                    uint64_t lat = cur_cycle - issue_cycle;
                    latency_.record(lat);
                    inflight_txns_.erase(it);
                }
                requests_completed_++;
            }
        }

        // 2. 请求发起
        if (!kernel_active_) {
            if (cur_kernel_id_ < num_kernels_) {
                kernel_active_ = true;
                cycles_since_launch_ = 0;
                cur_kernel_id_++;
                kernels_launched_++;
            }
        } else {
            if (cycles_since_launch_ < kernel_duration_ && adapter_ != nullptr) {
                auto* typed_adapter = dynamic_cast<StreamAdapter<GPUTLM,
                    bundles::ComputeReqBundle, bundles::ComputeRespBundle>*>(adapter_);
                if (typed_adapter != nullptr && typed_adapter->req_out() != nullptr) {
                    uint32_t reqs_per_wg = (workgroup_size_ + coalescing_factor_ - 1)
                                           / coalescing_factor_;
                    for (uint32_t wg = 0; wg < num_workgroups_; ++wg) {
                        for (uint32_t wf = 0; wf < reqs_per_wg; ++wf) {
                            bool is_wr = (rng_() % 2 == 0);
                            uint64_t addr = 0x10000ULL + uint64_t(wg) * 0x1000ULL;
                            uint64_t cur_cycle = (event_queue() != nullptr)
                                ? event_queue()->getCurrentCycle() : 0;
                            bundles::ComputeReqBundle req(
                                next_txn_id_,
                                cur_kernel_id_,
                                wg,
                                wf,
                                addr,
                                /*size*/ 4,
                                is_wr,
                                /*data*/ 0xCAFEBABEULL,
                                coalescing_factor_);
                            inflight_txns_[next_txn_id_] = cur_cycle;
                            typed_adapter->req_out()->write(req);
                            next_txn_id_++;
                            requests_issued_++;
                            if (is_wr) writes_++; else reads_++;
                        }
                    }
                    workgroups_dispatched_ += num_workgroups_;
                }
            } else {
                kernel_active_ = false;
            }
        }

        // 3. 周期计数
        cycles_since_launch_++;

        // 4. Adapter 自身 tick（handshake 推进）
        if (adapter_ != nullptr) {
            adapter_->tick();
        }
    }

    inline void set_stream_adapter(StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    inline std::unique_ptr<tlm_stats::StatGroup> get_stats_group() override {
        auto group = std::make_unique<tlm_stats::StatGroup>(name());
        group->addScalar(kernels_launched_);
        group->addScalar(workgroups_dispatched_);
        group->addScalar(requests_issued_);
        group->addScalar(requests_completed_);
        group->addScalar(writes_);
        group->addScalar(reads_);
        group->addDistribution(latency_);
        return group;
    }
```

- [ ] **Step 2.3: 验证编译**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
```

预期：编译通过，可能有 1-2 个 unused variable warning（`resp_in()` / `req_out()` 接口名可能因项目现状略有差异，根据实际编译错误调整，见 Step 2.4）。

- [ ] **Step 2.4: 修复编译错误（如果出现）**

如报 `req_out()` / `resp_in()` 接口不存在，参考 `include/framework/stream_adapter.hh` 实际 API 调整（plan 已知可能需要微调）；如报 `tlm_stats::Scalar` API 差异，参考 `include/metrics/stats.hh` 实际接口。

常见调整：
- 若 `req_out()` 是 protected：改为 `friend class StreamAdapter<GPUTLM, ...>` 或在 `gpu_tlm.hh` 内访问
- 若 `event_queue()` getter 名不同：用 `SimObject` 实际接口名

修复后重新编译直到通过：

```bash
cmake --build build -j$(nproc) 2>&1 | tail -5
```

- [ ] **Step 2.5: 提交**

```bash
git add include/tlm/gpu/gpu_tlm.hh
git commit -m "feat(gpu): add GPUTLM v0 blackbox initiator

- New include/tlm/gpu/gpu_tlm.hh (~250 lines)
- Inherits ChStreamModuleBase, single-port initiator
- 5 setters: num_kernels, kernel_duration, num_workgroups,
  workgroup_size, coalescing_factor
- tick(): consume resp -> issue requests per kernel -> advance cycle
- StatGroup: kernels_launched, workgroups_dispatched, requests_issued,
  requests_completed, writes, reads, latency
- v0 deliberately skips on_config_loaded JSON parsing (Phase7.B)
- No SIMD pipeline / ISA / LDS / HSA modeling (D2/D3/D4 deferred)
- Part of Phase7.A (IMPL-011-Phase7.A)"
```

---

## Task 3: REGISTER_CHSTREAM 扩展

**Files:**
- Modify: `include/chstream_register.hh:5-15` (新增 include) + `:39-65` (REGISTER_CHSTREAM 宏体扩展)

- [ ] **Step 3.1: 添加新 include**

在 `include/chstream_register.hh` 第 14 行（`#include "rtl/hybrid_cache_wrapper.hh"`）之后插入：

```cpp
// Phase7.A GPU 基础设施
#include "tlm/gpu/gpu_tlm.hh"
#include "bundles/compute_bundles_tlm.hh"
```

- [ ] **Step 3.2: 在 REGISTER_CHSTREAM 中添加 GPUTLM 注册**

定位 `REGISTER_CHSTREAM` 宏体（在 `:29` 开始）。在 `ModuleFactory::registerObject<tlm::LinkTLM>("LinkTLM");` 之后（第 39 行）插入：

```cpp
    ModuleFactory::registerObject<tlm::GPUTLM>("GPUTLM"); \
```

然后在 `ChStreamAdapterFactory::get().registerAdapter<tlm::LinkTLM, \
        bundles::NoCFlitBundle, bundles::NoCFlitBundle>("LinkTLM"); \` 之后插入：

```cpp
    ChStreamAdapterFactory::get().registerAdapter<tlm::GPUTLM, \
        bundles::ComputeReqBundle, bundles::ComputeRespBundle>("GPUTLM"); \
```

- [ ] **Step 3.3: 验证编译**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
```

预期：编译通过，GPUTLM 注册到 ModuleFactory。

- [ ] **Step 3.4: 验证注册生效**

```bash
./build/bin/cpptlm --list-types 2>&1 | grep -i gpu || echo "若 cpptlm 不支持 --list-types，跳过此步"
```

或用 grep 检查主入口是否输出 GPUTLM：

```bash
grep -A2 "REGISTER_ALL\|GPUTLM" src/main.cpp | head -20
```

预期：看到 `GPUTLM` 注册条目。

- [ ] **Step 3.5: 提交**

```bash
git add include/chstream_register.hh
git commit -m "feat(gpu): register GPUTLM via REGISTER_CHSTREAM macro

- Add #include for gpu_tlm.hh and compute_bundles_tlm.hh
- Extend REGISTER_CHSTREAM: registerObject<GPUTLM> + registerAdapter
  (ComputeReqBundle, ComputeRespBundle)
- GPUTLM now discoverable by ModuleFactory::instantiateAll()
- Part of Phase7.A (IMPL-011-Phase7.A)"
```

---

## Task 4: 端到端配置文件

**Files:**
- Create: `configs/gpu_standalone.json`

- [ ] **Step 4.1: 创建配置文件**

创建 `configs/gpu_standalone.json`：

```json
{
  "modules": [
    {
      "name": "gpu0",
      "type": "GPUTLM",
      "params": {
        "num_kernels": 2,
        "kernel_duration": 50,
        "num_workgroups": 4,
        "workgroup_size": 64,
        "coalescing_factor": 1
      }
    },
    {"name": "mem", "type": "MemoryTLM"}
  ],
  "connections": [
    {"src": "gpu0", "dst": "mem", "latency": 100}
  ]
}
```

> **注意**: v0 不实现 `on_config_loaded`，因此 `params` 不会被 GPUTLM 实际读取——所有行为由构造函数硬编码默认决定。这与 `TrafficGenTLM` / `CPUTLM` 现状一致；Phase7.B 统一修复。

- [ ] **Step 4.2: 验证配置可解析**

```bash
./build/bin/cpptlm --config configs/gpu_standalone.json --validate 2>&1 | tail -10 || \
./build/bin/cpptlm --config configs/gpu_standalone.json 2>&1 | tail -20
```

预期：JSON 可被 ModuleFactory 解析；GPUTLM 类型被识别；连接图无 cycle。

- [ ] **Step 4.3: 提交**

```bash
git add configs/gpu_standalone.json
git commit -m "feat(gpu): add gpu_standalone.json validation config

- 1x GPUTLM (v0 hardcoded params: 2 kernels x 50 cyc x 4 WG x 64 WFx)
- 1x MemoryTLM
- 1 connection (gpu0 -> mem, latency=100)
- v0 params not actually consumed by GPUTLM (Phase7.B)
- Part of Phase7.A (IMPL-011-Phase7.A)"
```

---

## Task 5: Catch2 单元测试

**Files:**
- Create: `test/test_gpu_standalone.cc`

> **CMake 自动发现**: `test/CMakeLists.txt` 用 `file(GLOB TEST_SOURCES "test_*.cc")`，新测试文件无需修改 CMake。

- [ ] **Step 5.1: 创建测试文件**

创建 `test/test_gpu_standalone.cc`：

```cpp
// test/test_gpu_standalone.cc
// GPUTLM v0 单元测试（Catch2 v3.7.0）
// 功能描述：验证 GPUTLM 黑盒发起器在 3 个典型场景下行为正确。
// 作者 CppTLM Team / 日期 2026-06-11
// 参考：docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md §4.3

#include <catch2/catch_test_macros.hpp>
#include "tlm/gpu/gpu_tlm.hh"
#include "framework/chstream_adapter_factory.hh"

using namespace tlm;
using namespace bundles;

namespace {

// 辅助：创建一个绑定到 GPUTLM 的 adapter（用于测试）
struct AdapterHandle {
    GPUTLM* gpu;
    StreamAdapterBase* adapter;
    AdapterHandle(GPUTLM* g, const std::string& type)
        : gpu(g), adapter(nullptr) {
        adapter = ChStreamAdapterFactory::get().create(type, g);
        g->set_stream_adapter(adapter);
    }
    ~AdapterHandle() {
        if (adapter) {
            delete adapter;
        }
    }
};

} // namespace

TEST_CASE("GPUTLM_Standalone.SendReadRequest", "[gpu]") {
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(1);
    gpu.set_num_workgroups(1);
    gpu.set_workgroup_size(64);
    gpu.set_coalescing_factor(64);  // 64/64 = 1 request per WG

    AdapterHandle handle(&gpu, "GPUTLM");

    // 1 kernel × 50 cycle 内发起 1 个请求
    for (int i = 0; i < 200; ++i) gpu.tick();

    // 验证: kernel_duration_ 默认 100，但 set_num_kernels(1) 只跑 1 个 kernel
    // 在 200 ticks 内，1 个 kernel 跑完 100 ticks，1 request issued
    REQUIRE(gpu.stats_requests_issued() == 1);
}

TEST_CASE("GPUTLM_Standalone.SendWriteRequest", "[gpu]") {
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(1);
    gpu.set_num_workgroups(2);
    gpu.set_workgroup_size(64);
    gpu.set_coalescing_factor(32);  // 64/32 = 2 requests per WG

    AdapterHandle handle(&gpu, "GPUTLM");

    for (int i = 0; i < 1000; ++i) gpu.tick();

    // 2 WG × 2 req = 4 requests issued
    REQUIRE(gpu.stats_requests_issued() == 4);
    // 50% 写概率：至少 1 个写（4 次独立事件，p(至少 1 写) ≈ 0.94）
    REQUIRE(gpu.stats_writes() + gpu.stats_reads() == 4);
    REQUIRE(gpu.stats_writes() >= 1);
    REQUIRE(gpu.stats_reads() >= 1);
}

TEST_CASE("GPUTLM_Standalone.MultiKernel", "[gpu]") {
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(3);
    gpu.set_num_workgroups(2);
    gpu.set_workgroup_size(32);
    gpu.set_kernel_duration(50);

    AdapterHandle handle(&gpu, "GPUTLM");

    // 3 kernels × kernel_duration(50) + margin = 200 ticks
    for (int i = 0; i < 250; ++i) gpu.tick();

    REQUIRE(gpu.stats_kernels_launched() == 3);
    REQUIRE(gpu.stats_workgroups_dispatched() == 6);  // 3 kernels × 2 WG
    // 每个 kernel 32/1 = 32 req，3 kernels × 32 = 96 requests
    REQUIRE(gpu.stats_requests_issued() == 96);
}

TEST_CASE("GPUTLM_Standalone.CoalescingFactor", "[gpu]") {
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(1);
    gpu.set_num_workgroups(1);
    gpu.set_workgroup_size(64);
    gpu.set_coalescing_factor(8);  // 64/8 = 8 requests per WG

    AdapterHandle handle(&gpu, "GPUTLM");

    for (int i = 0; i < 200; ++i) gpu.tick();

    REQUIRE(gpu.stats_requests_issued() == 8);
    REQUIRE(gpu.stats_workgroups_dispatched() == 1);
}

TEST_CASE("GPUTLM_Standalone.Reset", "[gpu]") {
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(2);
    gpu.set_num_workgroups(1);

    AdapterHandle handle(&gpu, "GPUTLM");

    for (int i = 0; i < 100; ++i) gpu.tick();
    REQUIRE(gpu.stats_requests_issued() > 0);

    gpu.do_reset();
    REQUIRE(gpu.stats_requests_issued() == 0);
    REQUIRE(gpu.stats_kernels_launched() == 0);
}
```

- [ ] **Step 5.2: 编译测试**

```bash
cmake --build build -j$(nproc) 2>&1 | tail -10
```

预期：编译通过，cpptlm_tests target 包含新测试文件。

- [ ] **Step 5.3: 运行 GPU 测试**

```bash
./build/bin/cpptlm_tests "[gpu]" --reporter compact 2>&1 | tail -20
```

预期：5 个测试用例通过。

> **注意**: 第一次运行可能因 `req_out()` / `resp_in()` / `StreamAdapter` 接口名差异需要微调（参考 Task 2.4 修复策略）。调整后重跑。

- [ ] **Step 5.4: 运行全量回归**

```bash
./build/bin/cpptlm_tests --reporter compact 2>&1 | tail -10
```

预期：528+ 现有测试 + 5 新测试全部通过（或 12 个已知失败不变 —— 见 `test/AGENTS.md`）。

- [ ] **Step 5.5: 提交**

```bash
git add test/test_gpu_standalone.cc
git commit -m "test(gpu): add GPUTLM v0 unit tests (5 cases, [gpu] tag)

- SendReadRequest: 1 kernel × 1 WG × 64/64 coalescing = 1 request
- SendWriteRequest: 2 WG × 2 req with 50/50 read/write split
- MultiKernel: 3 kernels × 2 WG = 6 WG dispatched, 96 requests
- CoalescingFactor: coalescing_factor=8 produces 8 req per WG
- Reset: do_reset() clears all stats counters
- Auto-discovered by test/CMakeLists.txt GLOB (no CMake change)
- Part of Phase7.A (IMPL-011-Phase7.A)"
```

---

## Task 6: 文档同步 + VIRTUAL_PATHS 扩展

**Files:**
- Modify: `AGENTS.md` (STRUCTURE 节)
- Modify: `docs/ONBOARDING.md` (GPU 路径说明)
- Modify: `scripts/test/docs_sync_check.sh` (VIRTUAL_PATHS 数组)

- [ ] **Step 6.1: 更新 AGENTS.md STRUCTURE 节**

定位 `AGENTS.md` STRUCTURE 节（`include/` 子目录列表）。在 `├── tlm/         # TLM 2.0 模块：CacheTLM, CrossbarTLM, MemoryTLM` 行附近修改为：

```
│   ├── tlm/         # TLM 2.0 模块：CacheTLM, CrossbarTLM, MemoryTLM
│   │   └── gpu/      # Phase7.A+ GPU 模块：GPUTLM v0 (黑盒发起器)
```

> 若 STRUCTURE 节是文本表格/缩进图，按相同风格修改。

- [ ] **Step 6.2: 更新 ONBOARDING.md**

定位 `docs/ONBOARDING.md` §5.5 脚本表附近或 WHERE TO LOOK 表附近。添加一行：

```markdown
| 添加新 GPU 模块 | `include/tlm/gpu/` + `include/chstream_register.hh` | 从 `ChStreamModuleBase` 派生，`REGISTER_CHSTREAM` 注册 |
```

> 若 WHERE TO LOOK 表格已存在，按相同格式追加；否则在合适位置添加新节。

- [ ] **Step 6.3: 扩展 VIRTUAL_PATHS**

> **注意**: 此步**仅在 Task 1-5 创建的文件尚未 commit 时需要**。若每个 Task 已 commit（路径已存在），则 VIRTUAL_PATHS 不需要扩展。检查：

```bash
git ls-files include/tlm/gpu/gpu_tlm.hh include/bundles/compute_bundles_tlm.hh configs/gpu_standalone.json test/test_gpu_standalone.cc
```

预期：5 个文件全部在 git 索引中。若已存在，**跳过本步**。否则在 `scripts/test/docs_sync_check.sh` 的 `VIRTUAL_PATHS=(...)` 数组中追加：

```bash
    # Phase 7.A GPU 基础设施 (2026-06-11)
    "compute_bundles_tlm.hh"
    "gpu_tlm.hh"
    "gpu_standalone.json"
    "test_gpu_standalone.cc"
```

- [ ] **Step 6.4: 验证文档同步**

```bash
bash scripts/test/docs_sync_check.sh --strict 2>&1 | tail -10
```

预期：路径引用总数 + 5，缺失路径数 = 0。

- [ ] **Step 6.5: 验证零 TODO**

```bash
grep -rn "TODO\|FIXME\|XXX" include/tlm/gpu/ include/bundles/compute_bundles_tlm.hh configs/gpu_standalone.json test/test_gpu_standalone.cc 2>&1
```

预期：零命中。

- [ ] **Step 6.6: 验证 clang-format**

```bash
./scripts/format.sh --check 2>&1 | tail -20
```

预期：零 diff。若有 diff，运行 `./scripts/format.sh` 自动格式化后重跑。

- [ ] **Step 6.7: 提交**

```bash
git add AGENTS.md docs/ONBOARDING.md scripts/test/docs_sync_check.sh
git commit -m "docs(gpu): sync AGENTS.md/ONBOARDING.md + extend VIRTUAL_PATHS

- AGENTS.md STRUCTURE: document include/tlm/gpu/ subdirectory
- ONBOARDING.md WHERE TO LOOK: add 'add new GPU module' row
- docs_sync_check.sh VIRTUAL_PATHS: 4 Phase7.A paths
  (compute_bundles_tlm.hh, gpu_tlm.hh, gpu_standalone.json, test_gpu_standalone.cc)
- Verified: docs_sync_check.sh --strict passes
- Verified: zero TODO/FIXME/XXX in new files
- Verified: clang-format --check clean
- Part of Phase7.A (IMPL-011-Phase7.A)"
```

---

## Task 7: 端到端验证 + 最终统计报告

**Files:** None (verification only)

- [ ] **Step 7.1: 完整 Release 构建**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) 2>&1 | tail -5
```

预期：零错误，零警告。

- [ ] **Step 7.2: 完整 Debug 构建（额外验证）**

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc) 2>&1 | tail -5
```

预期：零错误。

- [ ] **Step 7.3: GPU 测试**

```bash
./build/bin/cpptlm_tests "[gpu]" --reporter compact 2>&1 | tail -15
```

预期：5/5 测试通过。

- [ ] **Step 7.4: 全量回归**

```bash
./build/bin/cpptlm_tests --reporter compact 2>&1 | tail -5
```

预期：528+ 现有测试 + 5 新测试 = 533+ 通过（或 12 已知失败不变）。

- [ ] **Step 7.5: 配置文件端到端执行**

```bash
./build/bin/cpptlm --config configs/gpu_standalone.json 2>&1 | tail -30
```

预期：仿真正常结束；统计输出包含 `kernels_launched`、`workgroups_dispatched`、`requests_issued`、`requests_completed`；`requests_completed == requests_issued`（所有请求都有响应）。

- [ ] **Step 7.6: 文档同步 + 零债务最终验证**

```bash
bash scripts/test/docs_sync_check.sh --strict 2>&1 | tail -5
grep -rn "TODO\|FIXME\|XXX" include/tlm/gpu/ include/bundles/compute_bundles_tlm.hh 2>&1
./scripts/format.sh --check 2>&1 | tail -5
```

预期：
- docs_sync_check.sh 通过（0 缺失）
- 零 TODO/FIXME/XXX
- format 干净

- [ ] **Step 7.7: 提交验证报告（可选 commit 标记完成）**

```bash
# 如所有验证通过，无文件变更，commit 为空
git commit --allow-empty -m "verify(phase7.a): all acceptance criteria pass

- Release + Debug builds: zero errors, zero warnings
- 5 [gpu] tests pass
- 533+ regression tests pass (12 pre-existing failures unchanged)
- gpu_standalone.json end-to-end: requests_issued == requests_completed
- docs_sync_check.sh --strict: 0 missing paths
- Zero TODO/FIXME/XXX
- clang-format clean
- Phase7.A (IMPL-011-Phase7.A) complete"
```

---

## Acceptance Summary（所有任务完成后回查）

| Criterion | Source | Expected |
|-----------|--------|----------|
| 编译通过 | §5.1 | Release + Debug 零错误 |
| GPU 测试 | §5.2 | 5/5 通过 |
| 全量回归 | §5.4 | 533+/533+ 通过 |
| 配置可执行 | §5.3 | requests_completed == requests_issued |
| 文档同步 | §5.4 | docs_sync_check.sh --strict 通过 |
| 零 TODO | §5.5 | grep 零命中 |
| 格式 | §5.6 | format.sh --check 干净 |

---

## Self-Review Notes

**1. Spec coverage** — Spec §1-§9 均有对应任务：
- §1 范围 → Task 1-6
- §2 Bundle 设计 → Task 1
- §3 GPUTLM v0 → Task 2
- §4 注册配置 → Task 3-4
- §4.3 测试 → Task 5
- §5 验收 → Task 7
- §6 风险 → Task 2.4 已标注 API 差异风险
- §9 文件清单 → 全部覆盖

**2. Placeholder scan** — Plan 无 TBD / FIXME / "implement later"。每个 Task 含完整代码块。

**3. Type consistency** — Task 1 定义 `ComputeReqBundle`/`ComputeRespBundle`，Task 2-3-5 均引用同一类型签名。`GPUTLM::stats_*()` getter 在 Task 2 定义，Task 5 测试调用一致。`set_stream_adapter(StreamAdapterBase*)` 沿用 ChStreamModuleBase 接口。

**4. 已知微调点** — Task 2.4 / Task 5.3 标注如 `req_out()` / `resp_in()` / `tlm_stats::Scalar` API 实际签名可能与 plan 假设略有差异，需根据 `include/framework/stream_adapter.hh` 和 `include/metrics/stats.hh` 实际接口微调。已给出常见修复方向。

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`. Two execution options:**

**1. Subagent-Driven (recommended)** — 我为每个 Task 派发独立 subagent，Task 之间人工 review，迭代快。

**2. Inline Execution** — 在本会话内串行执行所有 Task，批量执行 + checkpoint review。

**Which approach?**