# Phase7.A — GPU 基础设施设计文档

> **Document ID**: IMPL-011-Phase7.A
> **Version**: 1.0
> **Date**: 2026-06-11
> **Status**: 🔄 Draft (待用户 review)
> **Author**: Based on Phase7 调研报告 + brainstorming 设计草案
> **Parent Roadmap**: `roadmap.md` Phase 7.A
> **Research Reference**: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §4 Phase 0

---

## 0. 阅读引导

本文档结构：

- **§1 范围** — 新增/修改文件清单
- **§2 ComputeReqBundle / ComputeRespBundle** — GPU 消息格式
- **§3 GPUTLM v0** — 黑盒发起器模块
- **§4 注册与配置** — `REGISTER_CHSTREAM` 扩展 + JSON 配置
- **§5 验收标准** — 编译、测试、配置、文档、零债务
- **§6 风险与缓解**
- **§7 反模式（明确不做）**
- **§8 与已有架构的兼容性**
- **§9 文件清单 + LOC 估算**
- **§10 用户审批点（5 项）**

---

## 1. 范围（Scope）

### 1.1 新增文件（4 个）

| 文件 | 角色 | LOC 估算 |
|------|------|----------|
| `include/bundles/compute_bundles_tlm.hh` | `ComputeReqBundle` / `ComputeRespBundle` 类型定义 | ~80 |
| `include/tlm/gpu/gpu_tlm.hh` | `GPUTLM` v0 类（ChStreamModuleBase 派生） | ~250 |
| `configs/gpu_standalone.json` | 端到端验证配置（1× GPUTLM + 1× MemoryTLM） | ~20 |
| `test/test_gpu_standalone.cc` | Catch2 单元测试（3 个测试用例） | ~150 |

### 1.2 修改文件（3 个）

| 文件 | 修改内容 |
|------|----------|
| `include/chstream_register.hh` | 在 `REGISTER_CHSTREAM` 宏体中加入 GPUTLM 注册（+2 行）+ 新头文件引用（+3 行） |
| `AGENTS.md` | STRUCTURE 节添加 `include/tlm/gpu/` 子目录说明 |
| `docs/ONBOARDING.md` | §5 脚本表或类似位置添加 GPU 模块路径说明 |

**总计：~520 LOC（含注释和头文件 boilerplate）**

---

## 2. ComputeReqBundle / ComputeRespBundle 设计

### 2.1 设计原则

- **沿用 CacheReqBundle 模式**：继承 `bundle_base` + `ch_uint<N>` 字段 + 谓词方法（与 `include/bundles/cache_bundles_tlm.hh:38-69` 一致）。
- **组合而非继承**：不复制 CacheReqBundle 8 个字段，通过字段嵌入（而非 `public CacheReqBundle` 继承）保持 POD 兼容 `bundle_serialization.hh::memcpy` 序列化。
- **GPU 字段语义对位 gem5**：`kernel_id` ≈ AQL `dispatch_id`，`workgroup_id` ≈ ComputeUnit `dispWorkgroup`，`wavefront_id` ≈ SIMD `wfSlotId`。

### 2.2 ComputeReqBundle（GPU → Memory 请求）

```cpp
namespace bundles {

struct ComputeReqBundle : public bundle_base {
    // === 继承自 CacheReqBundle 风格的字段 ===
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;        // 0 = 根事务
    ch_uint<8>  fragment_id;      // 当前拍序号（0-based）
    ch_uint<8>  fragment_total;   // 总拍数（1 = 不分片）
    ch_uint<64> address;          // global_mem_addr
    ch_uint<8>  size;             // 请求字节数
    ch_bool     is_write;
    ch_uint<64> data;             // 写数据首 8 字节

    // === GPU 特定字段（v2.1 §4.1 风格）===
    ch_uint<32> kernel_id;        // 当前 kernel 标识（HSA AQL dispatch_id）
    ch_uint<32> workgroup_id;     // workgroup 标识（CU 内调度）
    ch_uint<32> wavefront_id;     // wavefront 标识（SIMD lane 组）
    ch_uint<32> coalescing_factor;// N 个独立请求合并为 1 个（1 = 不合并）

    ComputeReqBundle() = default;

    ComputeReqBundle(uint64_t tid, uint32_t kid, uint32_t wgid, uint32_t wfid,
                     uint64_t addr, uint8_t sz, bool wr, uint64_t d,
                     uint32_t cf = 1)
        : transaction_id(tid), parent_id(0), fragment_id(0), fragment_total(1)
        , address(addr), size(sz), is_write(wr), data(d)
        , kernel_id(kid), workgroup_id(wgid), wavefront_id(wfid)
        , coalescing_factor(cf) {}

    // 谓词（与 CacheReqBundle 对齐）
    bool is_first_fragment() const { return fragment_id.read() == 0; }
    bool is_last_fragment() const {
        return fragment_id.read() + 1 >= fragment_total.read();
    }
    bool is_root() const {
        return parent_id.read() == 0 && fragment_total.read() == 1;
    }
};

} // namespace bundles
```

### 2.3 ComputeRespBundle（Memory → GPU 响应）

```cpp
namespace bundles {

struct ComputeRespBundle : public bundle_base {
    // === 继承自 CacheRespBundle 风格的字段 ===
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;
    ch_uint<8>  fragment_id;
    ch_uint<8>  fragment_total;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;
    ch_bool     first;
    ch_bool     last;

    // === GPU 特定字段 ===
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
```

### 2.4 字段语义表

| 字段 | 类型 | gem5 对位 | CppTLM 用途 |
|------|------|-----------|-------------|
| `kernel_id` | u32 | `Shader::kernel_id` / AQL `dispatch_id` | 标识当前 kernel launch |
| `workgroup_id` | u32 | `ComputeUnit::dispWorkgroup(wg_id)` | 标识 workgroup（CU 调度单元） |
| `wavefront_id` | u32 | `Wavefront::wfSlotId` | 标识 wavefront（SIMD lane 组） |
| `coalescing_factor` | u32 | `VIPERCoalescer::coalesce_factor` | 抽象 coalescing（D3）：N 个请求合并为 1 |

---

## 3. GPUTLM v0 设计

### 3.1 类签名

```cpp
namespace tlm {

class GPUTLM : public ChStreamModuleBase {
public:
    GPUTLM(const std::string& name, EventQueue* eq = nullptr);

    // JSON 参数读取（v0 stub，Phase7.B 真正实现）
    void on_config_loaded() override;

    // 主循环：tick() 中按 kernel_duration_ 周期发出 ComputeReqBundle
    void tick() override;

    // StreamAdapter 注入（ChStreamModuleBase 抽象）
    void set_stream_adapter(StreamAdapterBase* adapter) override;
    StreamAdapterBase* stream_adapter() const { return adapter_; }

    // === 黑盒行为参数（程序化 setter，JSON 解析在 Phase7.B）===
    void set_num_kernels(uint32_t n) { num_kernels_ = n; }
    void set_kernel_duration(uint32_t cyc) { kernel_duration_ = cyc; }
    void set_num_workgroups(uint32_t n) { num_workgroups_ = n; }
    void set_workgroup_size(uint32_t sz) { workgroup_size_ = sz; }
    void set_coalescing_factor(uint32_t cf) { coalescing_factor_ = cf; }

    // === 统计（StatGroup）===
    std::unique_ptr<tlm_stats::StatGroup> get_stats_group() override;

    // === 重置（继承自 ChStreamModuleBase）===
    void do_reset() override;

private:
    // StreamAdapter 单端口（与 TrafficGenTLM 同型）
    StreamAdapter<GPUTLM, ComputeReqBundle, ComputeRespBundle>* adapter_;

    // === 黑盒参数默认值 ===
    uint32_t num_kernels_      = 1;    // 启动几个 kernel
    uint32_t kernel_duration_  = 100;  // 每 kernel 周期数
    uint32_t num_workgroups_   = 4;    // 每 kernel 多少 workgroup
    uint32_t workgroup_size_   = 64;   // wavefront 大小
    uint32_t coalescing_factor_= 1;    // 抽象 coalescing

    // === 运行期状态 ===
    uint32_t cur_kernel_id_    = 0;
    uint32_t cur_workgroup_id_ = 0;
    uint32_t cur_wavefront_id_ = 0;
    uint64_t next_txn_id_      = 1;
    uint32_t cycles_since_launch_ = 0;
    bool     kernel_active_    = false;

    // inflight 跟踪
    std::unordered_map<uint64_t, uint64_t> inflight_txns_; // txn_id → issue_cycle

    // === 统计 ===
    tlm_stats::Scalar       kernels_launched_{"kernels_launched"};
    tlm_stats::Scalar       workgroups_dispatched_{"workgroups_dispatched"};
    tlm_stats::Scalar       requests_issued_{"requests_issued"};
    tlm_stats::Scalar       requests_completed_{"requests_completed"};
    tlm_stats::Distribution latency_{"latency"};
};

} // namespace tlm
```

### 3.2 tick() 行为流程

```
每个 tick():
1. 响应消费:
   if (resp_in.valid() && resp_in.ready())
       bundle = resp_in.read()
       issue_cycle = inflight_txns_[bundle.transaction_id]
       latency = current_cycle - issue_cycle
       inflight_txns_.erase(bundle.transaction_id)
       requests_completed_++
       latency_.record(latency)

2. 请求发起:
   if (kernel_active_ == false):
       if (cur_kernel_id_ < num_kernels_):
           kernel_active_ = true
           cycles_since_launch_ = 0
           cur_kernel_id_++
           kernels_launched_++

   else: // kernel_active_ == true
       if (cycles_since_launch_ < kernel_duration_):
           // 发起当前 kernel 的所有 workgroup 请求
           for wg in [0, num_workgroups_):
               for wf in [0, ceil(workgroup_size_ / coalescing_factor_)):
                   bundle = ComputeReqBundle(
                       txn_id = next_txn_id_++,
                       kid = cur_kernel_id_,
                       wgid = wg,
                       wfid = wf,
                       addr = 0x10000 + wg * 0x1000,  // v0 固定地址
                       size = 4,
                       is_write = (rng() % 2 == 0),  // 50% 写
                       data = 0xCAFEBABE,
                       cf = coalescing_factor_)
                   inflight_txns_[bundle.transaction_id] = current_cycle
                   req_out.write(bundle)
                   requests_issued_++
           workgroups_dispatched_ += num_workgroups_
       else:
           kernel_active_ = false  // 当前 kernel 完成，等待下一个

3. 周期计数:
   cycles_since_launch_++

4. adapter->tick()  // StreamAdapter 自身的握手推进
```

### 3.3 黑盒行为约束（v0 显式不做）

- ❌ 不模拟 SIMD pipeline / 5-stage（Fetch → Scoreboard → Schedule → Exec → Mem）
- ❌ 不模拟 wavefront scheduling 状态机（S_STOPPED / S_RUNNING / S_STALLED 等）
- ❌ 不模拟 LDS / Register File / 寄存器分配
- ❌ 不实现 HSA Runtime / doorbell PIO / AQL packet 解析
- ❌ 不实现 `on_config_loaded` JSON 解析（Phase7.B 做）
- ❌ 不实现 barrier / waitcnt / 信号量

---

## 4. 注册与配置

### 4.1 REGISTER_CHSTREAM 扩展

修改 `include/chstream_register.hh`：

```cpp
// === 在第 5 行之后、第 6 行之前插入新 include ===
#include "tlm/gpu/gpu_tlm.hh"
#include "bundles/compute_bundles_tlm.hh"

// === 在 REGISTER_CHSTREAM 宏体内（现有 9 个 registerObject 之后）插入 ===
ModuleFactory::registerObject<tlm::GPUTLM>("GPUTLM");

// === 在现有 registerAdapter 之后插入 ===
ChStreamAdapterFactory::get().registerAdapter<tlm::GPUTLM,
    bundles::ComputeReqBundle, bundles::ComputeRespBundle>("GPUTLM");
```

完整宏体新增行：

```cpp
#define REGISTER_CHSTREAM \
    ModuleFactory::registerObject<CacheTLM>("CacheTLM"); \
    ModuleFactory::registerObject<MemoryTLM>("MemoryTLM"); \
    ModuleFactory::registerObject<CrossbarTLM>("CrossbarTLM"); \
    ModuleFactory::registerObject<CPUTLM>("CPUTLM"); \
    ModuleFactory::registerObject<TrafficGenTLM>("TrafficGenTLM"); \
    ModuleFactory::registerObject<ArbiterTLM<2>>("ArbiterTLM2"); \
    ModuleFactory::registerObject<ArbiterTLM<4>>("ArbiterTLM4"); \
    ModuleFactory::registerObject<tlm::RouterTLM>("RouterTLM"); \
    ModuleFactory::registerObject<tlm::NICTLM>("NICTLM"); \
    ModuleFactory::registerObject<tlm::LinkTLM>("LinkTLM"); \
    ModuleFactory::registerObject<tlm::GPUTLM>("GPUTLM"); \  // ← 新增
    ChStreamAdapterFactory::get().registerAdapter<CacheTLM, \
        bundles::CacheReqBundle, bundles::CacheRespBundle>("CacheTLM"); \
    /* ... 现有 8 个 registerAdapter ... */ \
    ChStreamAdapterFactory::get().registerAdapter<tlm::LinkTLM, \
        bundles::NoCFlitBundle, bundles::NoCFlitBundle>("LinkTLM"); \
    ChStreamAdapterFactory::get().registerAdapter<tlm::GPUTLM, \  // ← 新增
        bundles::ComputeReqBundle, bundles::ComputeRespBundle>("GPUTLM"); \
    HYBRID_CACHE_WRAPPER_REGISTER_RTL
```

### 4.2 configs/gpu_standalone.json

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

> **注意**: v0 不实现 `on_config_loaded`，因此 `params` 不会被 GPUTLM 实际读取——所有行为由构造函数硬编码默认值决定。这与现有 `TrafficGenTLM` / `CPUTLM` 的语义落差一致；Phase7.B 才统一修复 JSON params 读取。

### 4.3 test/test_gpu_standalone.cc

Catch2 v3.7.0 测试（沿用 `test/test_phase6_integration.cc` 风格）：

```cpp
#include <catch2/catch_test_macros.hpp>
#include "tlm/gpu/gpu_tlm.hh"
#include "framework/chstream_adapter_factory.hh"

using namespace tlm;
using namespace bundles;

TEST_CASE("GPUTLM_Standalone.SendReadRequest", "[gpu]") {
    // 单 kernel，1 workgroup，1 wavefront，read-only
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(1);
    gpu.set_num_workgroups(1);
    gpu.set_workgroup_size(64);
    gpu.set_coalescing_factor(64);

    // 注入 adapter + 验证请求发送
    auto* adapter = ChStreamAdapterFactory::get().create("GPUTLM", &gpu);
    gpu.set_stream_adapter(adapter);

    // 运行 N ticks，验证 inflight 累积
    for (int i = 0; i < 200; ++i) gpu.tick();

    // 验证: requests_issued == ceil(64/64) == 1
    REQUIRE(gpu.stats_requests_issued() == 1);
}

TEST_CASE("GPUTLM_Standalone.SendWriteRequest", "[gpu]") {
    // 50% 写概率验证（1000 ticks 统计）
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(1);
    gpu.set_num_workgroups(2);
    gpu.set_workgroup_size(64);
    gpu.set_coalescing_factor(32);  // 64/32 = 2 requests per WG

    auto* adapter = ChStreamAdapterFactory::get().create("GPUTLM", &gpu);
    gpu.set_stream_adapter(adapter);

    for (int i = 0; i < 1000; ++i) gpu.tick();

    // 2 WG × 2 req = 4 requests issued
    REQUIRE(gpu.stats_requests_issued() == 4);
    // 至少 1 个写请求
    REQUIRE(gpu.stats_writes() >= 1);
}

TEST_CASE("GPUTLM_Standalone.MultiKernel", "[gpu]") {
    GPUTLM gpu("gpu0");
    gpu.set_num_kernels(3);
    gpu.set_num_workgroups(2);
    gpu.set_workgroup_size(32);
    gpu.set_kernel_duration(50);

    auto* adapter = ChStreamAdapterFactory::get().create("GPUTLM", &gpu);
    gpu.set_stream_adapter(adapter);

    // 3 kernels × kernel_duration(50) + margin = 200 ticks
    for (int i = 0; i < 250; ++i) gpu.tick();

    REQUIRE(gpu.stats_kernels_launched() == 3);
    REQUIRE(gpu.stats_workgroups_dispatched() == 6);
}
```

---

## 5. 验收标准（Acceptance）

### 5.1 编译验证

```bash
# Release 构建
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# 预期: 零警告，零错误
```

### 5.2 测试验证

```bash
# GPU 专项测试
./build/bin/cpptlm_tests "[gpu]"
# 预期: 3 个测试通过

# 全量回归（确保未破坏现有 84+ 测试）
./build/bin/cpptlm_tests
# 预期: 84+ 现有测试 + 3 新测试全部通过
```

### 5.3 配置可执行

```bash
./build/bin/cpptlm --config configs/gpu_standalone.json
# 预期:
# - 仿真正常结束（无 assertion / segfault）
# - stats 输出: kernels_launched=2, workgroups_dispatched=8, requests_issued=8
# - requests_completed == requests_issued（所有请求都有响应）
```

### 5.4 文档同步

```bash
bash scripts/test/docs_sync_check.sh --strict
# 预期: 路径引用总数 + 3-5 (新增路径), 缺失路径数 = 0
```

新增路径需纳入 `scripts/test/docs_sync_check.sh` 的 `VIRTUAL_PATHS`：
- `include/bundles/compute_bundles_tlm.hh`
- `include/tlm/gpu/gpu_tlm.hh`
- `configs/gpu_standalone.json`
- `test/test_gpu_standalone.cc`

### 5.5 零债务验证

```bash
# 严禁遗留 TODO
grep -rn "TODO" include/tlm/gpu/ include/bundles/compute_bundles_tlm.hh
# 预期: 零命中

grep -rn "FIXME\|XXX" include/tlm/gpu/ include/bundles/compute_bundles_tlm.hh
# 预期: 零命中
```

### 5.6 格式检查

```bash
./scripts/format.sh --check
# 预期: 零 diff（clang-format 4 空格缩进符合 AGENTS.md 约定）
```

---

## 6. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | `bundle_serialization.hh::memcpy` 因 GPU 字段扩展导致 Bundle 大小增加，影响 Packet payload | 中 | 低 | 新增 Bundle 独立大小（~64 bytes 增量），Packet payload 已有扩容能力；测试覆盖序列化路径 |
| R2 | `GPUTLM` 与 `TrafficGenTLM` 代码大量重复（tick 循环 / inflight 跟踪 / 统计模式）| 中 | 中 | v0 故意接受重复（保持黑盒简单性）；Phase7.B 抽出 `compute_unit_base` 共享基类 |
| R3 | `kernel_id/workgroup_id/wavefront_id` 用 32 位可能不够 | 低 | 低 | gem5 实际用 16 位足够；32 位是预留扩展空间 |
| R4 | JSON `params` 不读（v0 不实现 `on_config_loaded`）| 中 | 中 | v0 显式声明硬编码默认（与 `TrafficGenTLM` 现状一致）；`on_config_loaded` 在 Phase7.B 统一实现 |
| R5 | `GPUTLM` 与 `MemoryTLM` 的 Bundle 类型不匹配（Compute vs Cache）| 中 | 高 | **v0 直接连 MemoryTLM 不行**——必须经过 `ComputeUnitTLM` 或改用 Cache Bundle；**v0 妥协方案**：GPUTLM v0 临时输出 `CacheReqBundle`（与 ComputeReqBundle 字段兼容子集），Phase7.B 才完整分离 |

### 6.1 R5 详细说明（关键风险）

**问题**: `MemoryTLM` 注册为 `registerAdapter<MemoryTLM, CacheReqBundle, CacheRespBundle>`，而 `GPUTLM` 输出 `ComputeReqBundle`。两者类型不同，StreamAdapter 类型擦除后无法直接相连。

**解决选项**:
- **(A) GPUTLM v0 直接使用 CacheReqBundle**（丢弃 GPU 字段）→ 退化为简单 CPU 流量发生器，**违背 Phase7.A 目的**
- **(B) 新增 `GPUMemoryBridgeTLM`**（用 DualPortStreamAdapter）→ 引入额外模块，**超出 Phase7.A 范围**
- **(C) MemoryTLM 同时注册 ComputeReq/Resp** → 复用 MemoryTLM，**简单但有类型重复风险**
- **(D) v0 只验证 GPUTLM 内部状态**（不连真实下游，用 mock adapter）→ 牺牲端到端验证

**推荐方案: (C) + MemoryTLM 双 Bundle 注册**

```cpp
// MemoryTLM 在 chstream_register.hh 中追加第二个 adapter 注册
ChStreamAdapterFactory::get().registerAdapter<MemoryTLM,
    bundles::ComputeReqBundle, bundles::ComputeRespBundle>("MemoryTLM_Compute");
```

代价: MemoryTLM 类内部实现需要支持双 Bundle 类型（用模板特化或 type-erased 处理）。可在 v0 简化版中只验证路径，**完整实现在 Phase7.B**。

---

## 7. 反模式（明确不做）

| # | 不做 | 理由 |
|---|------|------|
| X1 | 复制 gem5 `.sm` 协议文件 | gem5 slicc 5000+ 行不可读；用 C++ `switch` 表驱动状态转换 |
| X2 | 模拟 GPU ISA（GCN/RDNA/CDNA 指令集） | CppTLM 是 TLM 框架，不是架构模拟器；不属 Phase7.A 范围 |
| X3 | 实现 HSA Runtime / doorbell PIO / AQL 解析 | 3000+ 行；Phase7.A 范畴之外（D4 决策：推迟到 Phase7.F+）|
| X4 | 建模 LDS / 寄存器文件 / 5-stage pipeline | D2 决策：黑盒优先；Phase7.A 仅发起请求 |
| X5 | 精确 wavefront coalescing 模拟 | D3 决策：抽象掉；用 `coalescing_factor` 参数代替 |
| X6 | 引入新外部依赖 | 当前零外部依赖编译模型不能打破 |
| X7 | 在 v0 实现 `on_config_loaded` JSON 解析 | 留给 Phase7.B；v0 仅暴露程序化 setter |
| X8 | 实现 barrier / waitcnt / 信号量 | D4 决策：KernelLaunchTLM 代替 |
| X9 | 把 GPUTLM 写成模板类 | v0 单实例足够；模板化延迟到 Phase7.D（multi-CU）|

---

## 8. 与已有架构的兼容性

### 8.1 StreamAdapter 复用

GPUTLM v0 用**单端口 `StreamAdapter<GPUTLM, ComputeReqBundle, ComputeRespBundle>`**，与 `TrafficGenTLM` / `CPUTLM` 同型。`ChStreamAdapterFactory::registerAdapter<>` 模板已支持任意 Bundle 对。

### 8.2 ChStreamModuleBase 接口沿用

- `set_stream_adapter(StreamAdapterBase*)` ✓
- `tick()` ✓
- `on_config_loaded()` override ✓（v0 stub，Phase7.B 真正实现）
- `get_stats_group()` override ✓（返回 StatGroup 实例）
- `do_reset()` override ✓（重置内部状态）

### 8.3 StatGroup 沿用

- `tlm_stats::Scalar` ✓（kernels_launched / workgroups_dispatched / requests_issued / requests_completed）
- `tlm_stats::Distribution` ✓（latency）

### 8.4 CMake 自动发现

`include/tlm/gpu/gpu_tlm.hh` + `include/bundles/compute_bundles_tlm.hh` 都通过 `cmake --build` 自动加入 `cpptlm_core` 静态库 target（`include/` 已在 CMake PUBLIC include 目录中）。

### 8.5 模块命名空间

按 AGENTS.md §约定：tlm 模块用 `tlm::` 子命名空间（与 `tlm::RouterTLM` / `tlm::NICTLM` / `tlm::LinkTLM` 一致）。`bundles::` 命名空间（与 `CacheReqBundle` 一致）。

### 8.6 测试标签

新测试用 `[gpu]` 标签（与现有 `[chstream]` / `[phase6]` 标签风格一致）：

```bash
./build/bin/cpptlm_tests "[gpu]"           # 仅 GPU 测试
./build/bin/cpptlm_tests "[gpu]~[skip]"    # GPU 测试，排除 skip 标签
```

---

## 9. 文件清单 + LOC 估算

| 文件 | 类型 | LOC 估算 | 说明 |
|------|------|----------|------|
| `include/bundles/compute_bundles_tlm.hh` | 新增 | 80 | ComputeReqBundle + ComputeRespBundle |
| `include/tlm/gpu/gpu_tlm.hh` | 新增 | 250 | GPUTLM v0 完整类 |
| `include/chstream_register.hh` | 修改 | +5 | 新 include + 1 registerObject + 1 registerAdapter |
| `configs/gpu_standalone.json` | 新增 | 20 | 端到端配置 |
| `test/test_gpu_standalone.cc` | 新增 | 150 | Catch2 3 个测试 |
| `AGENTS.md` | 修改 | +5 | STRUCTURE 节添加 `include/tlm/gpu/` |
| `docs/ONBOARDING.md` | 修改 | +5 | GPU 模块路径说明 |
| `scripts/test/docs_sync_check.sh` | 修改 | +5 | VIRTUAL_PATHS 新增 4 路径 |
| **总计** | — | **~520** | 包含注释与头文件 boilerplate |

---

## 10. 用户审批点（5 项）

> 请确认以下 5 项均通过后进入实施：

### 审批点 1: §2 ComputeReqBundle/RespBundle 字段设计

字段集：`transaction_id` / `parent_id` / `fragment_id` / `fragment_total` / `address` / `size` / `is_write` / `data` + GPU 扩展 `kernel_id` / `workgroup_id` / `wavefront_id` / `coalescing_factor`。

- [ ] 接受此字段设计
- [ ] 需要调整（请说明哪个字段）

### 审批点 2: §3 GPUTLM v0 黑盒行为

行为：5 个程序化 setter（num_kernels / kernel_duration / num_workgroups / workgroup_size / coalescing_factor）+ tick() 流程（响应消费 + 请求发起 + 周期计数）+ 4 个 StatGroup 统计 + 50% 读写混合。

- [ ] 接受此行为
- [ ] 需要调整（请说明哪个参数 / 行为）

### 审批点 3: §4 注册扩展

在 `REGISTER_CHSTREAM` 宏体中加 2 行（1 个 registerObject + 1 个 registerAdapter）+ 2 行新 include。

- [ ] 接受此注册方式
- [ ] 需要调整（请说明）

### 审批点 4: §6 R5 解决方案

**关键决策**: MemoryTLM 需要双 Bundle 注册（CacheReq/Resp + ComputeReq/Resp）以接受 GPUTLM 直接连接。MemoryTLM 类内部需要支持双 Bundle 类型。

- [ ] 接受此方案
- [ ] 选择其他方案（A / B / D，见 §6.1）
- [ ] 需要讨论

### 审批点 5: §5 验收标准

5 项验收：编译 + 测试 + 配置可执行 + 文档同步 + 零 TODO。

- [ ] 接受此验收
- [ ] 需要补充（请说明哪项）

---

## 附录 A: 与 gem5 ComputeUnit 的对位

| gem5 ComputeUnit.py | CppTLM GPUTLM v0 | 差距 |
|--------------------|------------------|------|
| `Shader::Shader(...)` 构造 + CU list | `GPUTLM::GPUTLM(name, eq)` 单实例 | v0 单 CU；multi-CU 留 Phase7.E |
| `ComputeUnit::n_wf × SIMD × Wavefront` | 不建模；用 `coalescing_factor` 抽象 | D3 决策 |
| `FetchStage → Scoreboard → Schedule → Exec` 5-stage pipeline | 不建模；tick() 直接发请求 | D2 决策 |
| `LdsState::reserveSpace(dispId, wgId, size)` | 不建模 | Phase7.F+ 推迟 |
| `MemoryPort[wf_size=64] → VIPERCoalescer → TCP` | 单端口 req_out 直接发请求；coalescer 抽象 | 简化 |
| `GPUDispatcher::dispatch(task)` | 不建模；KernelLaunchTLM 代替 | Phase7.A+ |
| `HSAPacketProcessor::write(Pkt)` doorbell | 不建模 | Phase7.F+ 推迟 |

---

## 附录 B: 后续 Phase 衔接

- **Phase7.B**: 抽出 `compute_unit_base` 共享基类 + 实现 `on_config_loaded` + `KernelLaunchTLM` + 多 CU 实例化
- **Phase7.C**: `CacheTLM` protocol-aware 改造（最高风险）
- **Phase7.D**: TCC 桥 + Memory hbm_mode 扩展
- **Phase7.E**: 多 CU + GPU NoC + `BidirectionalPortAdapter<N>`
- **Phase7.F**: Full APU SoC Demo 配置文件 + 端到端 Python 测试

---

**Document Status**: 🔄 Draft — 待用户对 §10 五个审批点的反馈后进入实施阶段

**Next Action**: 用户 review → commit spec doc → 转 `writing-plans` skill 写实施计划