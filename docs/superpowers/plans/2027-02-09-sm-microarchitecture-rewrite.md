# dGPU SoC v1.0 SM Microarchitecture 重 rewrite 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完整重构 CppTLM GPU 算力侧：删除 6 个 GPU 算力侧模块 + 3 vendor 接口 + 15 测试；新增 12 个 ChStream SM 子模块 + 8 种 Bundle + `IComputeDevice` 14 方法；PTX-EMU 仅通过 `IComputeDevice` 接口步进模拟指令（`attach_timing` 保留为 deprecated stub，device_api.h 不动）。

**Architecture:** 双轨前端 + 统一 Timing 宿主模式（per Oracle 范式分析 + architecture/15 v5.0 + HSK-9 草稿 v3）。SM 端持寄存器唯一真值（RegFileUnit），PTX-EMU 端通过 `set_instr_descriptor_buf()` 注入已解码 InstrDescriptor + 通过 `get_register_value()`/`is_instruction_completed()` 读路径/就绪协议同步；Gate 验证两边 ALU 实现 bit-exact。12 子模块通过 8 种 Bundle 连接：FetchUnit→DecodeUnit→IssueUnit→ScalarALU/VectorALU/MatrixCore/SIMTLane/LsuGlobal/LsuLDS/RegFileUnit/WritebackUnit/HazardTracker。

**Tech Stack:** C++17 + SystemC stub + ChStreamModuleBase + bundles + ModuleFactory + Catch2 v3.7.0 + git-master 多原子 commit + OpenSpec change `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` (supersedes `cpptlm-dgpu-d1-cdna-isa-phase-a`)

**关联文档**:
- 设计: [`docs/soc_arch/architecture/15-sm-microarchitecture-design.md`](../soc_arch/architecture/15-sm-microarchitecture-design.md) (v5.0, commit `650e9e9`, 971 行)
- HSK-9 草稿: [`docs/soc_arch/adr/hsk9-announcement-draft.md`](../soc_arch/adr/hsk9-announcement-draft.md) (179 行, supersede commit `4105602`)
- ADR: [`docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`](../soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md) §3 D2 (需 Status Update 标记 superseded-by sm-rewrite)
- 反转 ADR: `ADR-SOC-02-cu-granularity.md` (黑盒优先→完整 SM) + 新建 `ADR-SOC-16-sm-microarchitecture.md`
- Stage A supersede: `openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/`

**总工作量**: 25-30 工作日（per Oracle Round 2 评估）+ 20 个原子 commit（per §15.9.1，每 commit 可编译）。

---

## 工作 Task 列表 (20 个)

### Task 1: ADR 背书 SM 重构 + 修订 ADR-SOC-02 Status Update

**Files:**
- Create: `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md`
- Modify: `docs/soc_arch/adr/ADR-SOC-02-cu-granularity.md` (append Status Update section)
- Modify: `docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md` (Status Update 标记 superseded-by sm-rewrite)
- Modify: `docs/soc_arch/adr/README.md` (append ADR-SOC-16 row)

- [ ] **Step 1: 创建 ADR-SOC-16 (背书 SM 微架构重构)**

Create `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md` with full content per design §15.2-§15.6 + §15.10 Gate + §15.12 Round 5 close-out. Use the existing ADR-SOC-15 template structure (Status / Context / Decision / Consequences / Implementation / Risks).

- [ ] **Step 2: 修订 ADR-SOC-02 Status Update 段**

Append to `docs/soc_arch/adr/ADR-SOC-02-cu-granularity.md`:
```markdown
## Status Update
- **2027-02-09**: 本 ADR 决策**被反转**。新设计 `architecture/15-sm-microarchitecture-design.md` §15.2-§15.6 实施完整 SM 微架构（5-stage pipeline + SIMT lane + wavefront 调度 + 精确 issue/execute 周期建模），取代本 ADR 的"CU 黑盒 + 永久推迟"决策。背书: ADR-SOC-16。
```

- [ ] **Step 3: 修订 ADR-SOC-15 Status Update 段**

Append to `docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`:
```markdown
## Status Update
- **2027-02-09**: §3 D2 "PipelineTLM 双轨实现"决策**被反转**。`cpptlm-dgpu-d1-cdna-isa-phase-a` (阶段 A) OpenSpec change 被 `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` **superseded**。新设计直接构建完整 SM 微架构（per architecture/15），不再分阶段双轨。背书: ADR-SOC-16。
```

- [ ] **Step 4: 同步 ADR README.md 索引**

Edit `docs/soc_arch/adr/README.md` append ADR-SOC-16 row to the table:
```markdown
| [ADR-SOC-16-sm-microarchitecture.md](./ADR-SOC-16-sm-microarchitecture.md) | **dGPU SoC v1.0 SM 微架构重构**（反转 ADR-SOC-02 黑盒优先，12 个 ChStream 子模块 + 8 种 Bundle + IComputeDevice 14 方法 + SM-owns-state 模式；实施见 `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`） | ✅ Accepted | v1.0 (2027-Q3+) |
 |
```

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md \
                   docs/soc_arch/adr/ADR-SOC-02-cu-granularity.md \
                   docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md \
                   docs/soc_arch/adr/README.md
GIT_MASTER=1 git commit -m "docs(adr): ADR-SOC-16 背书 SM 微架构重构 + 修订 ADR-SOC-02/15 Status Update"
```

### Task 2: 修订设计文档 v5.0 终稿 + HSK-9 草稿（已推送 — 重 commit reference）

**Files:**
- Modify: 无（设计文档已在 commit `650e9e9` 推送；HSK-9 草稿同 commit）

- [ ] **Step 1: 验证 HEAD 为 commit `650e9e9`**

Run: `GIT_MASTER=1 git log --oneline -5 | grep 650e9e9`
Expected: 命中 `650e9e9 docs(soc_arch): 修订 15-sm-microarchitecture-design v5.0 (Round 5 终稿) + HSK-9 草稿同步`

- [ ] **Step 2: 验证设计文档 971 行**

Run: `wc -l docs/soc_arch/architecture/15-sm-microarchitecture-design.md docs/soc_arch/adr/hsk9-announcement-draft.md`
Expected: 971 + 179 行。

- [ ] **Step 3: 提交**

无（仅 verification）。

### Task 3: 启动 OpenSpec change `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` (supersedes phase-a)

**Files:**
- Create: `openspec/changes/cpptlm-dgpu-d1-cdna-isa-sm-rewrite/`
  - `.openspec.yaml`
  - `README.md`
  - `proposal.md`
  - `design.md`
  - `tasks.md`
  - `specs/sm-microarchitecture/spec.md`
- Modify: `openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/README.md` (标 superseded)

- [ ] **Step 1: 创建 change 骨架**

Run: `cd /workspace/project/CppTLM && openspec new change cpptlm-dgpu-d1-cdna-isa-sm-rewrite --description "dGPU SoC SM 微架构重构 - 12 ChStream 子模块 + 8 Bundle + IComputeDevice 14 方法" --goal "完整重构 GPU 算力侧为 gpgpu-sim 风格 SM 微架构；PTX-EMU 仅通过 IComputeDevice 接口步进；supersedes cpptlm-dgpu-d1-cdna-isa-phase-a"`
Expected: 创建 6 个文件骨架。

- [ ] **Step 2: 覆写 proposal.md**

Write proposal.md per architecture/15 §15.1 + §15.10.4: Why (PTX-EMU 集成清理 + SM 微架构) + What Changes (12 子模块 + 8 Bundle + IComputeDevice 14 方法 + 18 删除 + DOC HYGIENE 9 项) + Capabilities (sm-microarchitecture) + Impact (20 commits, per Oracle Round 4 终稿).

- [ ] **Step 3: 覆写 design.md**

Write design.md per architecture/15 §15.2-§15.6 + §15.5.6 sync 协议 + §15.6.3 HSK-9 + §15.6.5 SFU. Reference architecture/15 sections by section number.

- [ ] **Step 4: 覆写 specs/sm-microarchitecture/spec.md**

Write spec.md per OpenSpec schema: 14 Requirements covering 12 submodules + 8 bundles + IComputeDevice 14 methods + dual-compute bit-exact Gate. Scenarios for each.

- [ ] **Step 5: 覆写 tasks.md**

Write tasks.md per architecture/15 §15.9.1 (20 atomic commits with dependency graph, each compilable). Reference the 20 commits by number with file paths.

- [ ] **Step 6: 标 phase-a superseded**

Edit `openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/README.md` append:
```markdown
## Status Update
- **2027-02-09**: **Superseded** by `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`. 阶段 A 双轨并存决策被反转；CDNA 引擎接入由 SM 重构直接承接。
```

- [ ] **Step 7: Validate**

Run: `openspec validate cpptlm-dgpu-d1-cdna-isa-sm-rewrite --strict`
Expected: PASS.

- [ ] **Step 8: 提交**

```bash
GIT_MASTER=1 git add openspec/changes/cpptlm-dgpu-d1-cdna-isa-sm-rewrite/ \
                   openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/README.md
GIT_MASTER=1 git commit -m "feat(openspec): 启动 cpptlm-dgpu-d1-cdna-isa-sm-rewrite change + 标 phase-a superseded"
```

### Task 4: 新增 SM 顶层容器 + IComputeDevice 接口 (stub 实现)

**Files:**
- Create: `include/tlm/gpu/i_compute_device.hh`
- Create: `include/tlm/gpu/streaming_multiprocessor_tlm.hh` (renamed from gpu_compute_unit_tlm)
- Create: `src/tlm/gpu/streaming_multiprocessor_tlm.cc` (stub)

- [ ] **Step 1: 写失败测试 — IComputeDevice 接口契约**

Create `test/test_i_compute_device_interface.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/i_compute_device.hh"
#include "tlm/gpu/instruction_descriptor.hh"

using namespace cpptlm::gpu;

TEST_CASE("IComputeDevice has 14 pure virtual methods", "[icompute][cdna-phase-a]") {
    // Count pure virtual methods via SFINAE or static_assert
    static_assert(
        std::is_abstract_v<IComputeDevice>,
        "IComputeDevice must be abstract"
    );
}

TEST_CASE("InstrDescriptor ISA discriminator + instr_id fields exist", "[icompute][cdna-phase-a]") {
    InstrDescriptor desc{};
    desc.isa_type = InstrDescriptor::IsaType::kCDNA64;
    desc.instr_id = 42;
    REQUIRE(desc.isa_type == InstrDescriptor::IsaType::kCDNA64);
    REQUIRE(desc.instr_id == 42);
}
```

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[icompute]"`
Expected: FAIL with "include/tlm/gpu/i_compute_device.hh: No such file or directory".

- [ ] **Step 2: 实现 IComputeDevice 接口 (14 方法 stub)**

Create `include/tlm/gpu/i_compute_device.hh`:
```cpp
#ifndef TLM_GPU_I_COMPUTE_DEVICE_HH
#define TLM_GPU_I_COMPUTE_DEVICE_HH

#include "tlm/gpu/instruction_descriptor.hh"
#include <cstdint>
#include <cstddef>

namespace cpptlm::gpu {

struct DeviceConfig {
    uint32_t num_sms = 1;
    uint32_t max_warps_per_sm = 64;
    uint32_t max_threads_per_sm = 2048;
    std::size_t shared_mem_size_per_sm = 48 * 1024;
};

class IComputeDevice {
public:
    virtual ~IComputeDevice() = default;
    
    // === 11 preserved from IPtxEmuDevice ===
    virtual bool initialize(const DeviceConfig& cfg) = 0;
    virtual void shutdown() = 0;
    virtual int  exe_once() = 0;
    virtual int  sm_exe_once(uint32_t sm_id) = 0;
    virtual int  warp_exe_once(uint32_t sm_id, uint32_t warp_id) = 0;
    virtual bool set_scoreboard(uint32_t sm_id, uint32_t warp_id, uint64_t mask) = 0;
    virtual int  get_thread_state(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id) = 0;
    virtual bool set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask) = 0;
    virtual bool set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc) = 0;
    virtual WarpStatus get_warp_status(uint32_t sm_id, uint32_t warp_id) = 0;
    virtual bool is_finished() = 0;
    // attach_timing removed from this interface (per HSK-9; kept on IPtxEmuDevice as deprecated stub)
    
    // === 1 new from SM rewrite ===
    virtual void set_instr_descriptor_buf(const InstrDescriptor* buf, uint32_t count) = 0;
    
    // === 2 new from Round 4 user decision ===
    virtual bool get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                                     uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF) = 0;
    virtual bool is_instruction_completed(uint64_t instr_id) = 0;
    
    // === 1 SM-owns-state reset ===
    virtual void reset() = 0;
};

} // namespace cpptlm::gpu

#endif
```

Add `class WarpStatus` stub (per `external/PTX-EMU/include/ptxemu/device_api.h:70-77`):

```cpp
// In i_compute_device.hh
enum class ThreadState : uint32_t {
    kIdle = 0, kRun = 1, kExit = 2, kBarSync = 3
};
struct LaneStatus {
    uint32_t lane_id = 0;
    ThreadState state = ThreadState::kIdle;
    uint32_t pc = 0;
};
struct WarpStatus {
    uint32_t warp_id = 0;
    uint32_t sm_id = 0;
    std::vector<LaneStatus> lanes;
    uint32_t active_count = 0;
    int32_t blocked_cycles = 0;
};
```

Update `include/tlm/gpu/instruction_descriptor.hh` (from Stage A spec, may need to add ISA + instr_id fields if not present; per architecture/15 §15.5.6 — but Stage A hasn't been implemented yet, so this file may need creation too):

Check `include/tlm/gpu/instruction_descriptor.hh` existence: `ls include/tlm/gpu/instruction_descriptor.hh 2>&1`. If absent, create per architecture/15 §15.5.6 (PipeClass/LatencyClass/CtrlBits/InstrDescriptor with isa_type + instr_id + result_value + memory_data fields).

- [ ] **Step 3: 实现 SM 顶层容器 stub**

Create `include/tlm/gpu/streaming_multiprocessor_tlm.hh`:
```cpp
#ifndef TLM_GPU_STREAMING_MULTIPROCESSOR_TLM_HH
#define TLM_GPU_STREAMING_MULTIPROCESSOR_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include "tlm/gpu/i_compute_device.hh"
#include <string>

namespace tlm {

class StreamingMultiprocessorTLM : public ChStreamModuleBase, public cpptlm::gpu::IComputeDevice {
public:
    explicit StreamingMultiprocessorTLM(const std::string& name, EventQueue* eq);
    ~StreamingMultiprocessorTLM() override = default;
    
    std::string get_module_type() const override { return "StreamingMultiprocessorTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    
    // IComputeDevice 14 methods: all stub return false/0
    bool initialize(const cpptlm::gpu::DeviceConfig& cfg) override { return false; }
    void shutdown() override {}
    int  exe_once() override { return 0; }
    int  sm_exe_once(uint32_t sm_id) override { return 0; }
    int  warp_exe_once(uint32_t sm_id, uint32_t warp_id) override { return 0; }
    bool set_scoreboard(uint32_t sm_id, uint32_t warp_id, uint64_t mask) override { return false; }
    int  get_thread_state(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id) override { return 0; }
    bool set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask) override { return false; }
    bool set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc) override { return false; }
    cpptlm::gpu::WarpStatus get_warp_status(uint32_t sm_id, uint32_t warp_id) override { return {}; }
    bool is_finished() override { return false; }
    void set_instr_descriptor_buf(const cpptlm::gpu::InstrDescriptor* buf, uint32_t count) override {}
    bool get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                             uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF) override { return false; }
    bool is_instruction_completed(uint64_t instr_id) override { return false; }
    void reset() override {}
    
    void tick() override {}

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
};

} // namespace tlm

#endif
```

Create `src/tlm/gpu/streaming_multiprocessor_tlm.cc`:
```cpp
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

namespace tlm {
StreamingMultiprocessorTLM::StreamingMultiprocessorTLM(const std::string& name, EventQueue* eq)
    : ChStreamModuleBase(name, eq) {}

void StreamingMultiprocessorTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapter) {
    adapter_ = adapter;
}
}
```

Add to `src/CMakeLists.txt`: `tlm/gpu/streaming_multiprocessor_tlm.cc`

- [ ] **Step 4: 运行测试验证通过**

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[icompute]"`
Expected: PASS.

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/i_compute_device.hh \
                   include/tlm/gpu/streaming_multiprocessor_tlm.hh \
                   src/tlm/gpu/streaming_multiprocessor_tlm.cc \
                   src/CMakeLists.txt \
                   test/test_i_compute_device_interface.cc
GIT_MASTER=1 git commit -m "feat(tlm): 新增 SM 顶层容器 StreamingMultiprocessorTLM + IComputeDevice 接口 (stub)"
```

### Task 5: 新增 12 个 ChStream 子模块 stub (.hh + 空 .cc)

**Files:**
- Create: `include/tlm/gpu/sm/fetch_unit_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/decode_unit_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/issue_unit_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/scalar_alu_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/vector_alu_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/matrix_core_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/simt_lane_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/lsu_global_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/lsu_lds_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/reg_file_unit_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/writeback_unit_tlm.{hh,cc}`
- Create: `include/tlm/gpu/sm/hazard_tracker_tlm.{hh,cc}`

- [ ] **Step 1: 写失败测试 — 12 子模块注册验证**

Create `test/test_sm_modules_stub.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/sm/fetch_unit_tlm.hh"
#include "tlm/gpu/sm/decode_unit_tlm.hh"
#include "tlm/gpu/sm/issue_unit_tlm.hh"
#include "tlm/gpu/sm/scalar_alu_tlm.hh"
#include "tlm/gpu/sm/vector_alu_tlm.hh"
#include "tlm/gpu/sm/matrix_core_tlm.hh"
#include "tlm/gpu/sm/simt_lane_tlm.hh"
#include "tlm/gpu/sm/lsu_global_tlm.hh"
#include "tlm/gpu/sm/lsu_lds_tlm.hh"
#include "tlm/gpu/sm/reg_file_unit_tlm.hh"
#include "tlm/gpu/sm/writeback_unit_tlm.hh"
#include "tlm/gpu/sm/hazard_tracker_tlm.hh"

using namespace tlm::sm;

TEST_CASE("12 SM submodules instantiate as ChStreamModuleBase stubs", "[sm-unit][cdna-phase-a]") {
    EventQueue eq;
    auto fu = std::make_unique<FetchUnitTLM>("fu", &eq);
    REQUIRE(fu->get_module_type() == "FetchUnitTLM");
    auto du = std::make_unique<DecodeUnitTLM>("du", &eq);
    REQUIRE(du->get_module_type() == "DecodeUnitTLM");
    auto iu = std::make_unique<IssueUnitTLM>("iu", &eq);
    REQUIRE(iu->get_module_type() == "IssueUnitTLM");
    auto sa = std::make_unique<ScalarALU>("sa", &eq);
    REQUIRE(sa->get_module_type() == "ScalarALU");
    auto va = std::make_unique<VectorALU>("va", &eq);
    REQUIRE(va->get_module_type() == "VectorALU");
    auto mc = std::make_unique<MatrixCore>("mc", &eq);
    REQUIRE(mc->get_module_type() == "MatrixCore");
    auto sl = std::make_unique<SIMTLane>("sl", &eq);
    REQUIRE(sl->get_module_type() == "SIMTLane");
    auto lg = std::make_unique<LsuGlobal>("lg", &eq);
    REQUIRE(lg->get_module_type() == "LsuGlobal");
    auto ll = std::make_unique<LsuLDS>("ll", &eq);
    REQUIRE(ll->get_module_type() == "LsuLDS");
    auto rf = std::make_unique<RegFileUnit>("rf", &eq);
    REQUIRE(rf->->get_module_type() == "RegFileUnit");
    auto wb = std::make_unique<WritebackUnit>("wb", &eq);
    REQUIRE(wb->get_module_type() == "WritebackUnit");
    auto ht = std::make_unique<HazardTracker>("ht", &eq);
    REQUIRE(ht->get_module_type() == "HazardTracker");
}
```

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-unit]"`
Expected: FAIL with "include/tlm/gpu/sm/fetch_unit_tlm.hh: No such file or directory".

- [ ] **Step 2: 创建 12 子模块 stub .hh 文件**

For each of the 12 submodules, create `include/tlm/gpu/sm/<name>_tlm.hh` with the following template (example for FetchUnitTLM):

```cpp
#ifndef TLM_GPU_SM_FETCH_UNIT_TLM_HH
#define TLM_GPU_SM_FETCH_UNIT_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <string>

namespace tlm::sm {

class FetchUnitTLM : public ChStreamModuleBase {
public:
    explicit FetchUnitTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {}
    ~FetchUnitTLM() override = default;

    std::string get_module_type() const override { return "FetchUnitTLM"; }
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    void tick() override {}

private:
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
};

} // namespace tlm::sm

#endif
```

Repeat with class names: FetchUnitTLM / DecodeUnitTLM / IssueUnitTLM / ScalarALU / VectorALU / MatrixCore / SIMTLane / LsuGlobal / LsuLDS / RegFileUnit / WritebackUnit / HazardTracker. Each file: 12 submodules × ~25 lines = ~300 LOC total.

- [ ] **Step 3: 创建 12 子模块 stub .cc 文件**

For each, create `src/tlm/gpu/sm/<name>_tlm.cc`:
```cpp
#include "tlm/gpu/sm/fetch_unit_tlm.hh"
// (or whichever name)
```

(empty body or include only)

Add to `src/CMakeLists.txt`:
```cmake
tlm/gpu/sm/fetch_unit_tlm.cc
tlm/gpu/sm/decode_unit_tlm.cc
tlm/gpu/sm/issue_unit_tlm.cc
tlm/gpu/sm/scalar_alu_tlm.cc
tlm/gpu/sm/vector_alu_tlm.cc
tlm/gpu/sm/matrix_core_tlm.cc
tlm/gpu/sm/simt_lane_tlm.cc
tlm/gpu/sm/lsu_global_tlm.cc
tlm/gpu/sm/lsu_lds_tlm.cc
tlm/gpu/sm/reg_file_unit_tlm.cc
tlm/gpu/sm/writeback_unit_tlm.cc
tlm/gpu/sm/hazard_tracker_tlm.cc
```

- [ ] **Step 4: 运行测试验证通过**

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-unit]"`
Expected: PASS (12 stub modules instantiate).

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/ src/tlm/gpu/sm/ src/CMakeLists.txt test/test_sm_modules_stub.cc
GIT_MASTER=1 git commit -m "feat(sm): 新增 12 个 ChStream 子模块 stub (.hh + 空 .cc)"
```

### Task 6: 新增 sm_bundles_tlm.hh 8 种 Bundle 定义

**Files:**
- Create: `include/bundles/sm_bundles_tlm.hh`

- [ ] **Step 1: 写失败测试 — 8 Bundle POD 字段**

Create `test/test_sm_bundles.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "bundles/sm_bundles_tlm.hh"

TEST_CASE("FetchToIssueBundle carries instr_desc + warp_id + pc", "[sm-bundle][cdna-phase-a]") {
    bundles::sm::FetchToIssueBundle b{};
    b.warp_id = 5;
    b.pc = 0x1000;
    REQUIRE(b.warp_id == 5);
    REQUIRE(b.pc == 0x1000);
}
```

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-bundle]"`
Expected: FAIL with "include/bundles/sm_bundles_tlm.hh: No such file or directory".

- [ ] **Step 2: 创建 8 种 Bundle 类型**

Create `include/bundles/sm_bundles_tlm.hh`:
```cpp
#ifndef BUNDLES_SM_BUNDLES_TLM_HH
#define BUNDLES_SM_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include "bundles/bundle_serialization.h"
#include "tlm/gpu/instruction_descriptor.hh"
#include <cstdint>

namespace bundles::sm {

struct FetchToIssueBundle : public bundle_base {
    cpptlm::gpu::InstrDescriptor instr_desc{};
    uint32_t warp_id = 0;
    uint32_t pc = 0;
    FetchToIssueBundle() = default;
};

struct DecodeToIssueBundle : public FetchToIssueBundle {
    cpptlm::gpu::PipeClass pipe = cpptlm::gpu::PipeClass::kScalarALU;
    cpptlm::gpu::LatencyClass latency_class = cpptlm::gpu::LatencyClass::kFixed1Cycle;
};

struct IssueToExecBundle : public DecodeToIssueBundle {
    uint64_t src_values[4] = {0, 0, 0, 0};  // PTX-EMU pre-computed source operand values (per Oracle Round 4 F1.4 dual-compute)
    bool src_valid[4] = {false, false, false, false};
};

struct ExecToWritebackBundle : public IssueToExecBundle {
    uint64_t result_value[4] = {0, 0, 0, 0};  // SM-computed (timing truth)
    uint8_t  result_num = 0;
    bool     memory_data_valid = false;
    uint64_t memory_data = 0;
    uint32_t exec_cycles = 0;  // for HazardTracker release
};

struct WritebackToRegFileBundle : public bundle_base {
    uint32_t warp_id = 0;
    uint16_t dst_regs[4] = {0, 0, 0, 0};
    uint64_t values[4] = {0, 0, 0, 0};
    uint8_t  num_dst = 0;
    bool     is_accvgpr = false;  // CDNA MFMA accumulator
    WritebackToRegFileBundle() = default;
};

struct MemoryReqBundle : public bundle_base {
    uint64_t vaddr = 0;
    uint32_t size = 0;
    bool     is_write = false;
    uint32_t sm_id = 0;
    uint32_t wave_id = 0;
    uint64_t tag = 0;
    uint64_t lane_mask = 0;  // intra-SM coalescing
};

struct MemoryRespBundle : public bundle_base {
    uint64_t tag = 0;
    uint32_t sm_id = 0;
    uint32_t wave_id = 0;
    uint64_t data = 0;
    bool     is_hit = true;
    uint32_t cycles = 0;  // for HazardTracker
};

struct ScoreboardQueryBundle : public bundle_base {
    enum class QueryType : uint8_t { kIsStalled, kDecrement, kIncrement };
    QueryType query_type = QueryType::kIsStalled;
    uint32_t warp_id = 0;
    uint32_t sm_id = 0;
    cpptlm::gpu::CtrlBits ctrl{};
};

} // namespace bundles::sm

#endif
```

- [ ] **Step 3: 运行测试验证通过**

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-bundle]"`
Expected: PASS.

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/bundles/sm_bundles_tlm.hh test/test_sm_bundles.cc
GIT_MASTER=1 git commit -m "feat(bundles): 新增 sm_bundles_tlm.hh 8 种 Bundle 定义"
```

### Task 7: 完整实现 12 个 ChStream 子模块 (连接 Bundle)

**Files:**
- Modify: 12 `include/tlm/gpu/sm/*_tlm.hh` (add methods per architecture/15 §15.3.3)
- Modify: 12 `src/tlm/gpu/sm/*_tlm.cc` (implement methods)

- [ ] **Step 1: 写失败测试 — 单子模块行为 (12 个测试场景)**

Create `test/test_sm_submodule_behavior.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/sm/fetch_unit_tlm.hh"
#include "bundles/sm_bundles_tlm.hh"

using namespace tlm::sm;
using namespace bundles::sm;

TEST_CASE("FetchUnit tick with empty instr buf returns 0 cycles", "[sm-unit][cdna-phase-a]") {
    EventQueue eq;
    FetchUnitTLM fu("fu", &eq);
    fu.set_instr_buf(nullptr, 0);
    fu.tick();
    REQUIRE(fu.cycles_fetched() == 0);
}
```

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-unit]"`
Expected: FAIL (FetchUnitTLM::set_instr_buf / cycles_fetched not implemented).

- [ ] **Step 2: 实现 FetchUnit (per architecture/15 §15.3.3.1)**

Modify `include/tlm/gpu/sm/fetch_unit_tlm.hh`:
```cpp
class FetchUnitTLM : public ChStreamModuleBase {
public:
    // ... existing ctor/dtor ...
    void set_instr_buf(const cpptlm::gpu::InstrDescriptor* buf, uint32_t count) {
        instr_buf_ = buf; instr_count_ = count;
    }
    uint64_t cycles_fetched() const { return cycles_fetched_; }
    void tick() override;

private:
    const cpptlm::gpu::InstrDescriptor* instr_buf_ = nullptr;
    uint32_t instr_count_ = 0;
    uint64_t cycles_fetched_ = 0;
};
```

Modify `src/tlm/gpu/sm/fetch_unit_tlm.cc`:
```cpp
#include "tlm/gpu/sm/fetch_unit_tlm.hh"

namespace tlm::sm {
void FetchUnitTLM::tick() {
    if (instr_buf_ == nullptr || instr_count_ == 0) {
        cycles_fetched_ = 0;
        return;
    }
    cycles_fetched_ += instr_count_;  // 1 cycle per fetch (per NV Blackwell 3 cycle dispatch)
}
}
```

- [ ] **Step 3: 重复 Step 1-2 模式实现其余 11 子模块**

Per architecture/15 §15.3.3.2-§15.3.3.12:
- **DecodeUnit**: classify InstrDescriptor → PipeClass + LatencyClass (no string parsing)
- **IssueUnit**: round-robin warp selection + CGGTY 5-warp threshold
- **ScalarALU**: SubPipe {kINT_FP32_Shared, kFP64, kSFU, kBranch} + execute + result_value[]
- **VectorALU**: V-pipe SIMD ops
- **MatrixCore**: MFMA subset (20 instructions per architecture/13)
- **SIMTLane**: 64-bit EXEC mask tracking (no divergence detection)
- **LsuGlobal**: MemoryReqBundle → IMemoryPort stub
- **LsuLDS**: intra-SM LDS access (no NoC)
- **RegFileUnit**: writeback + get_register_value read API
- **WritebackUnit**: ExecToWritebackBundle → WritebackToRegFileBundle + HazardTracker release
- **HazardTracker**: kVirtualReg (PTX) + kHardwareCounter (CDNA, stage C only)

Each submodule: ~30-50 LOC .hh + ~30-50 LOC .cc. Total ~600-800 LOC.

- [ ] **Step 4: 运行测试验证通过**

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-unit]"`
Expected: PASS (all 12 submodule behaviors verified).

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/ src/tlm/gpu/sm/ test/test_sm_submodule_behavior.cc
GIT_MASTER=1 git commit -m "feat(sm): 完整实现 12 个 ChStream 子模块 (连接 Bundle, 7 步 TDD)"
```

### Task 8: chstream_register.hh 注册 SM 顶层 + 12 子模块 + 8 Bundle 适配器

**Files:**
- Modify: `include/chstream_register.hh`

- [ ] **Step 1: 验证 chstream_register.hh 中注册调用尚未存在 (grep)**

Run: `grep -E "StreamingMultiprocessorTLM|FetchUnitTLM|RegisterAdapter.*Bundle" include/chstream_register.hh | head`
Expected: 空输出 (尚未注册)。

- [ ] **Step 2: 添加 12 子模块注册 + SM 顶层 + 8 Bundle 适配器**

Modify `include/chstream_register.hh` REGISTER_CHSTREAM macro body, append:
```cpp
ModuleFactory::registerObject<tlm::StreamingMultiprocessorTLM>("StreamingMultiprocessorTLM");
ModuleFactory::registerObject<tlm::sm::FetchUnitTLM>("FetchUnitTLM");
ModuleFactory::registerObject<tlm::sm::DecodeUnitTLM>("DecodeUnitTLM");
ModuleFactory::registerObject<tlm::sm::IssueUnitTLM>("IssueUnitTLM");
ModuleFactory::registerObject<tlm::sm::ScalarALU>("ScalarALU");
ModuleFactory::registerObject<tlm::sm::VectorALU>("VectorALU");
ModuleFactory::registerObject<tlm::sm::MatrixCore>("MatrixCore");
ModuleFactory::registerObject<tlm::sm::SIMTLane>("SIMTLane");
ModuleFactory::registerObject<tlm::sm::LsuGlobal>("LsuGlobal");
ModuleFactory::registerObject<tlm::sm::LsuLDS>("LsuLDS");
ModuleFactory::registerObject<tlm::sm::RegFileUnit>("RegFileUnit");
ModuleFactory::registerObject<tlm::sm::WritebackUnit>("WritebackUnit");
ModuleFactory::registerObject<tlm::sm::HazardTracker>("HazardTracker");
```

Add 8 Bundle adapter registration (per existing pattern):
```cpp
ChStreamAdapterFactory::get().registerAdapter<tlm::StreamingMultiprocessorTLM,
    bundles::sm::FetchToIssueBundle, bundles::sm::DecodeToIssueBundle,
    bundles::sm::IssueToExecBundle, bundles::sm::ExecToWritebackBundle,
    bundles::sm::WritebackToRegFileBundle, bundles::sm::MemoryReqBundle,
    bundles::sm::MemoryRespBundle, bundles::sm::ScoreboardQueryBundle
>("StreamingMultiprocessorTLM");
```

- [ ] **Step 3: 验证编译通过**

Run: `cmake --build build`
Expected: PASS.

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/chstream_register.hh
GIT_MASTER=1 git commit -m "feat(register): chstream_register.hh 注册 SM 顶层 + 12 子模块 + 8 Bundle 适配器"
```

### Task 9: GpuComputeUnitTLM 重构为 SM 内部状态机

**Files:**
- Rename: `include/tlm/gpu/gpu_compute_unit_tlm.{hh,cc}` → `include/tlm/gpu/streaming_multiprocessor_tlm_internal.{hh,cc}` (deprecated)
- Create: `include/tlm/gpu/sm_top_legacy_adapter.hh` (forward-compat shim)

- [ ] **Step 1: 写失败测试 — 旧 GpuComputeUnitTLM API 调用兼容**

Create `test/test_sm_top_legacy_compat.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "tlm/gpu/streaming_multiprocessor_tlm_internal.hh"

TEST_CASE("Legacy GpuComputeUnitTLM alias compiles via shim", "[sm-compat][cdna-phase-a]") {
    // Verify the shim typedef still compiles
    using LegacyCU = tlm::gpu_compute_unit_tlm;
    static_assert(std::is_same_v<LegacyCU, tlm::GpuComputeUnitTLM>,
                  "Legacy alias must point to GpuComputeUnitTLM");
}
```

- [ ] **Step 2: 保留旧 GpuComputeUnitTLM 作为 SM 顶层内部状态机**

Modify `include/tlm/gpu/streaming_multiprocessor_tlm.hh` to internally hold 12 submodules:
```cpp
class StreamingMultiprocessorTLM : public ChStreamModuleBase, public cpptlm::gpu::IComputeDevice {
private:
    std::unique_ptr<sm::FetchUnitTLM> fu_;
    std::unique_ptr<sm::DecodeUnitTLM> du_;
    std::unique_ptr<sm::IssueUnitTLM> iu_;
    std::unique_ptr<sm::ScalarALU> sa_;
    std::unique_ptr<sm::VectorALU> va_;
    std::unique_ptr<sm::MatrixCore> mc_;
    std::unique_ptr<sm::SIMTLane> sl_;
    std::unique_ptr<sm::LsuGlobal> lg_unit_;
    std::unique_ptr<sm::LsuLDS> ll_;
    std::unique_ptr<sm::RegFileUnit> rf_;
    std::unique_ptr<sm::WritebackUnit> wb_;
    std::unique_ptr<sm::HazardTracker> ht_;
    
public:
    explicit StreamingMultiprocessorTLM(const std::string& name, EventQueue* eq);
    // ... IComputeDevice 14 methods delegate to submodules ...
    void tick() override;
};
```

- [ ] **Step 3: 在 chstream_register.hh 注销旧 GpuComputeUnitTLM 注册**

Modify `include/chstream_register.hh`:
```cpp
// 删除
ModuleFactory::registerObject<tlm::GpuComputeUnitTLM>("GpuComputeUnitTLM");  // 已注销, 改名为 SM 顶层
ChStreamAdapterFactory::get().registerAdapter<tlm::GpuComputeUnitTLM, ...>  // 同上
```

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh \
                   src/tlm/gpu/streaming_multiprocessor_tlm.cc \
                   include/chstream_register.hh \
                   test/test_sm_top_legacy_compat.cc
GIT_MASTER=1 git commit -m "refactor(gpu_compute_unit): GpuComputeUnitTLM 重构为 SM 内部状态机 (chstream_register 注销旧名)"
```

### Task 10: 重构 WavefrontTLM/MinimalWarpSchedulerTLM/VectorRegFileTLM 为 SM 内部子模块

**Files:**
- Modify: `include/tlm/gpu/wavefront_tlm.{hh,cc}` → 内化到 SIMTLane 子模块
- Modify: `include/tlm/gpu/minimal_warp_scheduler_tlm.{hh,cc}` → 内化到 IssueUnit
- Modify: `include/tlm/gpu/vector_regfile_tlm.{hh,cc}` → 内化到 RegFileUnit
- Modify: `include/chstream_register.hh` 注销 3 个旧名

- [ ] **Step 1: 写失败测试 — 3 旧模块 API 调用测试 (应被新模块替代)**

Skip — these are pure refactor without API change at test level (新 API 在 Task 5-7 已测试).

- [ ] **Step 2: 将 3 旧模块内化到对应 SM 子模块 (Task 7 已部分实现, 此处只需文件合并)**

For each: copy useful methods from old .hh to new submodule .hh, mark old class as `[[deprecated]]` typedef, then in commit 16 delete the old files entirely.

For now (Task 10): just add `[[deprecated]]` typedef in old .hh:
```cpp
// In include/tlm/gpu/wavefront_tlm.hh
#include "tlm/gpu/sm/simt_lane_tlm.hh"
namespace tlm {
using WavefrontTLM [[deprecated("use tlm::sm::SIMTLane")]] = sm::SIMTLane;
}
```

Repeat for MinimalWarpSchedulerTLM → IssueUnitTLM and VectorRegFileTLM → RegFileUnit.

- [ ] **Step 3: 在 chstream_register.hh 注销 3 旧名**

```cpp
// 删除
ModuleFactory::registerObject<tlm::WavefrontTLM>("WavefrontTLM");
ModuleFactory::registerObject<tlm::MinimalWarpSchedulerTLM>("MinimalWarpSchedulerTLM");
ModuleFactory::registerObject<tlm::VectorRegFileTLM>("VectorRegFileTLM");
```

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/wavefront_tlm.hh \
                   include/tlm/gpu/minimal_warp_scheduler_tlm.hh \
                   include/tlm/gpu/vector_regfile_tlm.hh \
                   include/chstream_register.hh
GIT_MASTER=1 git commit -m "refactor(legacy): 重构 WavefrontTLM/MinimalWarpSchedulerTLM/VectorRegFileTLM 为 SM 内部子模块 (chstream_register 注销旧名)"
```

### Task 11: 修复旁路依赖 (gpu_soc_tlm.{h,cc} + async_completion_adapter + main.cpp + chstream_register)

**Files:**
- Modify: `include/tlm/gpu/gpu_soc_tlm.{hh,cc}` (移除 KernelLaunchTLM 引用 + chstream_register)
- Modify: `include/tlm/gpu/async_completion_adapter.hh` (改接 IComputeDevice)
- Modify: `src/main.cpp` (移除 kernel_launch_tlm.hh include)

- [ ] **Step 1: 写失败测试 — 旁路修复验证 (KernelLaunchTLM 符号应全无残留)**

Create `test/test_no_kernel_launch_residual.cc`:
```cpp
#include "catch_amalgamated.hpp"

// Verify all removed files are not referenced
TEST_CASE("Removed modules not transitively referenced", "[gpu-cleanup][cdna-phase-a]") {
    // This test runs at compile time: if any of the removed headers
    // are still included by any other .hh in the build, this file
    // fails to compile (we include everything that might reference them)
    #include "tlm/gpu/kernel_launch_tlm.hh"  // should fail
}
```

Run: should fail to compile.

- [ ] **Step 2: 修改 gpu_soc_tlm.{h,cc}**

In `gpu_soc_tlm.hh`: remove `class KernelLaunchTLM;` forward decl, replace `KernelLaunchTLM*` setters with `IComputeDevice*`:
```cpp
#include "tlm/gpu/i_compute_device.hh"
// remove: class KernelLaunchTLM;
class GpuSocTLM {
    // ...
    void set_compute_device(cpptlm::gpu::IComputeDevice* dev) { compute_dev_ = dev; }
private:
    cpptlm::gpu::IComputeDevice* compute_dev_ = nullptr;
};
```

In `gpu_soc_tlm.cc`: remove all `#include "tlm/gpu/kernel_launch_tlm.hh"` and `KernelLaunchTLM` references.

- [ ] **Step 3: 修改 async_completion_adapter.hh**

Replace `KernelLaunchTLM` with `IComputeDevice`:
```cpp
#include "tlm/gpu/i_compute_device.hh"
class AsyncCompletionAdapter {
public:
    void set_target_device(cpptlm::gpu::IComputeDevice* dev) { target_ = dev; }
private:
    cpptlm::gpu::IComputeDevice* target_ = nullptr;
};
```

- [ ] **Step 4: 修改 src/main.cpp**

Remove `#include "tlm/gpu/kernel_launch_tlm.hh"` and any direct usage.

- [ ] **Step 5: 修改 chstream_register.hh (Task 9 已部分完成, 此处清理剩余 KernelLaunchTLM 注销)**

```cpp
// 删除 (if not already in Task 9)
ModuleFactory::registerObject<tlm::KernelLaunchTLM>("KernelLaunchTLM");
ChStreamAdapterFactory::get().registerAdapter<tlm::KernelLaunchTLM, ...>;
```

- [ ] **Step 6: 验证编译通过**

Run: `cmake --build build`
Expected: PASS.

- [ ] **Step 7: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/gpu_soc_tlm.hh \
                   include/tlm/gpu/gpu_soc_tlm.cc \
                   include/tlm/gpu/async_completion_adapter.hh \
                   src/main.cpp \
                   include/chstream_register.hh \
                   test/test_no_kernel_launch_residual.cc
GIT_MASTER=1 git commit -m "refactor(headers): 修复 gpu_soc_tlm.{h,cc} + async_completion_adapter + main.cpp + chstream_register 旁路依赖 (改接 IComputeDevice)"
```

### Task 12: 删除 KernelLaunchTLM + CudaCoreAdapterMVP + PtxEmuSubmoduleMVP

**Files:**
- Delete: `include/tlm/gpu/kernel_launch_tlm.hh`
- Delete: `src/tlm/gpu/kernel_launch_tlm.cc`
- Delete: `include/tlm/gpu/cuda_core_adapter_mvp.{hh,cc}`
- Delete: `include/tlm/gpu/ptx_emu_submodule_mvp.{hh,cc}`
- Delete: `src/tlm/gpu/cuda_core_adapter_mvp.cc`
- Delete: `src/tlm/gpu/ptx_emu_submodule_mvp.cc`
- Modify: `src/CMakeLists.txt` (删除 6 行引用)

- [ ] **Step 1: 物理删除 6 个文件**

Run: `git rm include/tlm/gpu/kernel_launch_tlm.hh src/tlm/gpu/kernel_launch_tlm.cc include/tlm/gpu/cuda_core_adapter_mvp.hh src/tlm/gpu/cuda_core_adapter_mvp.cc include/tlm/gpu/ptx_emu_submodule_mvp.hh src/tlm/gpu/ptx_emu_submodule_mvp.cc`

- [ ] **Step 2: 修改 src/CMakeLists.txt**

Remove 6 lines:
```cmake
tlm/gpu/kernel_launch_tlm.cc
tlm/gpu/cuda_core_adapter_mvp.cc
tlm/gpu/ptx_emu_submodule_mvp.cc
```

- [ ] **Step 3: 验证编译通过**

Run: `cmake --build build`
Expected: PASS (旁路修复已完成).

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add -A
GIT_MASTER=1 git commit -m "refactor(soc): 删除 KernelLaunchTLM + CudaCoreAdapterMVP + PtxEmuSubmoduleMVP"
```

### Task 13: 删除 PipelineTLM + ScoreboardTLM + TensorCoreTLM + 3 vendor 接口头文件

**Files:**
- Delete: `include/tlm/gpu/pipeline_tlm.{hh,cc}` + `src/tlm/gpu/pipeline_tlm.cc`
- Delete: `include/tlm/gpu/scoreboard_tlm.{hh,cc}` + `src/tlm/gpu/scoreboard_tlm.cc`
- Delete: `include/tlm/gpu/tensor_core_tlm.{hh,cc}` + `src/tlm/gpu/tensor_core_tlm.cc`
- Delete: `include/cudart/{pipeline_interface,scoreboard_interface,tensor_core_interface}.h`
- Delete: `include/cudart/AGENTS.md`
- Delete: `include/cudart/` (空目录)
- Modify: `src/CMakeLists.txt`

- [ ] **Step 1: 物理删除 9 个文件 + 1 目录**

Run:
```bash
git rm include/tlm/gpu/pipeline_tlm.hh include/tlm/gpu/scoreboard_tlm.hh include/tlm/gpu/tensor_core_tlm.hh
git rm src/tlm/gpu/pipeline_tlm.cc src/tlm/gpu/scoreboard_tlm.cc src/tlm/gpu/tensor_core_tlm.cc
git rm include/cudart/pipeline_interface.h include/cudart/scoreboard_interface.h include/cudart/tensor_core_interface.h include/cudart/AGENTS.md
rmdir include/cudart 2>/dev/null || true
```

- [ ] **Step 2: 修改 src/CMakeLists.txt**

Remove 3 lines:
```cmake
tlm/gpu/pipeline_tlm.cc
tlm/gpu/scoreboard_tlm.cc
tlm/gpu/tensor_core_tlm.cc
```

- [ ] **Step 3: 验证编译通过**

Run: `cmake --build build`
Expected: PASS.

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add -A
GIT_MASTER=1 git commit -m "refactor(tlm): 删除 PipelineTLM + ScoreboardTLM + TensorCoreTLM + 3 vendor 接口头文件 (include/cudart/)"
```

### Task 14: 修订 4 个 JSON config

**Files:**
- Modify: `configs/vector_add_n1024.json`
- Modify: `configs/templates/compute_unit_v1.json`
- Modify: `configs/templates/gpu_soc/gpu_soc_gb203_v1.json`
- Verify: `examples/dgpu_soc_with_pcie_ip.json` (no change needed)

- [ ] **Step 1: 写失败测试 — JSON reload L7 测试**

Create `test/test_json_reload_sm.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/module_factory.hh"

TEST_CASE("4 JSON configs load with SM-rewrite module types", "[sm-json][cdna-phase-a]") {
    ModuleFactory factory;
    
    SECTION("vector_add_n1024.json") {
        // StreamingMultiprocessorTLM must replace KernelLaunchTLM
        auto config = factory.loadConfig("configs/vector_add_n1024.json");
        REQUIRE_NOTHROW(factory.instantiateAll(config));
    }
    SECTION("compute_unit_v1.json") {
        REQUIRE_NOTHROW(factory.instantiateAll(factory.loadConfig("configs/templates/compute_unit_v1.json")));
    }
    SECTION("gpu_soc_gb203_v1.json") {
        // remove KernelLaunchTLM module entry + rewire connections
        REQUIRE_NOTHROW(factory.instantiateAll(factory.loadConfig("configs/templates/gpu_soc/gpu_soc_gb203_v1.json")));
    }
}
```

Run: should fail because `KernelLaunchTLM` is no longer registered.

- [ ] **Step 2: 修订 configs/vector_add_n1024.json**

Change `"type": "KernelLaunchTLM"` → `"type": "StreamingMultiprocessorTLM"` (1 occurrence expected).

- [ ] **Step 3: 修订 configs/templates/compute_unit_v1.json**

Update comment mentioning "GpuComputeUnitTLM" → "StreamingMultiprocessorTLM" (no `type` string change needed per Oracle Round 3 P2).

- [ ] **Step 4: 修订 configs/templates/gpu_soc/gpu_soc_gb203_v1.json**

Two changes:
1. Remove the `"type": "KernelLaunchTLM"` module entry entirely (not rename — deletion per Oracle Round 3 P1-d).
2. Change `"type": "GpuComputeUnitTLM"` → `"type": "StreamingMultiprocessorTLM"`.
3. Remove connections referencing the deleted kernel_launch module.

- [ ] **Step 5: 验证 examples/dgpu_soc_with_pcie_ip.json 无引用被删模块**

Run: `grep -E "KernelLaunchTLM|GpuComputeUnitTLM|StreamingMultiprocessorTLM" examples/dgpu_soc_with_pcie_ip.json`
Expected: empty (no change needed).

- [ ] **Step 6: 运行 L7 测试验证通过**

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-json]"`
Expected: PASS.

- [ ] **Step 7: 提交**

```bash
GIT_MASTER=1 git add configs/vector_add_n1024.json \
                   configs/templates/compute_unit_v1.json \
                   configs/templates/gpu_soc/gpu_soc_gb203_v1.json \
                   test/test_json_reload_sm.cc
GIT_MASTER=1 git commit -m "chore(configs): 修订 4 个 JSON config (vector_add_n1024, compute_unit_v1 注释, gpu_soc_gb203_v1 modules + connections)"
```

### Task 15: DOC HYGIENE 全套 (AGENTS.md + ONBOARDING + 7 modules docs + 3 main specs + VIRTUAL_PATHS + test scripts)

**Files:**
- Modify: `AGENTS.md` STRUCTURE 节 + WHERE-TO-LOOK 表 + PHASE-STATE 表 + ADR 计数
- Modify: `docs/ONBOARDING.md` §5.5 脚本表
- Delete: 7 `docs/soc_arch/modules/{gpu-kernel-launch,cuda-core-adapter,ptx-emu-submodule-mvp,dgpu-board,gpu-compute_unit,gpu-soc,gpu.common}.md`
- Modify: `docs/soc_arch/modules/README.md` add VIRTUAL_PATHS
- Modify: `openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md` + `gpgpu-precision-wave2/spec.md` + `cli-f12b-flag/spec.md` (标 superseded)
- Modify: `scripts/test/docs_sync_check.sh` VIRTUAL_PATHS (12 + 4 条目)
- Modify: `test/python/test_f12b_smoke.py` L47 kernel_launch 重定位

- [ ] **Step 1: 写失败测试 — DOC HYGIENE 完整性验证**

Create `test/test_doc_hygiene.sh`:
```bash
#!/bin/bash
# Verify all paths in docs_sync_check.sh VIRTUAL_PATHS resolve
set -e
for path in $(grep -oP 'docs/soc_arch/architecture/15-...' scripts/test/docs_sync_check.sh | sort -u); do
    [ -f "$path" ] || (echo "Missing: $path" && exit 1)
done
for path in $(grep -oP 'include/cudart/' scripts/test/docs_sync_check.sh | head -3); do
    [ ! -d "$path" ] || (echo "Should be deleted: $path" && exit 1)
done
echo "DOC HYGIENE PASS"
```

- [ ] **Step 2: 修订根 AGENTS.md STRUCTURE 节 + WHERE-TO-LOOK + PHASE-STATE + ADR 计数**

Edit 4 places:
- L118 ADR 计数 "8 份" →"15 份 + ADR-SOC-16 新增"
- L178 WHERE-TO-LOOK "★ PCIe EP 微架构" 行追加 SM 微架构条目
- L404 PHASE-STATE "Phase 8 整合交付" 改"SM 重构完成 (commit `650e9e9`)"
- 删除 L42 GPU compute 部分对 KernelLaunchTLM 的引用

- [ ] **Step 3: 修订 docs/ONBOARDING.md §5.5 脚本表**

Update or remove entries mentioning KernelLaunchTLM/CudaCoreAdapterMVP/PtxEmuSubmoduleMVP.

- [ ] **Step 4: 物理删除 7 modules docs**

Run: `git rm docs/soc_arch/modules/{gpu-kernel-launch,cuda-core-adapter,ptx-emu-submodule-mvp,dgpu-board,gpu-compute_unit,gpu-soc,gpu.common}.md`

- [ ] **Step 5: 修订 docs/soc_arch/modules/README.md 加 VIRTUAL_PATHS 条目**

Append:
```markdown
## VIRTUAL_PATHS (历史模块, 已废弃)
- `docs/soc_arch/modules/gpu-kernel-launch.md` → superseded by `architecture/15-sm-microarchitecture-design.md` §15.7.2 (commit `650e9e9`)
- `docs/soc_arch/modules/cuda-core-adapter.md` → 同上
- `docs/soc_arch/modules/ptx-emu-submodule-mvp.md` → 同上
- `docs/soc_arch/modules/dgpu-board.md` → 重构: `dgpu_soc_tlm` 已合并到 SM 顶层
- `docs/soc_arch/modules/gpu-compute_unit.md` → superseded by `StreamingMultiprocessorTLM` (Task 9)
- `docs/soc_arch/modules/gpu-soc.md` → 重构: 改接 `IComputeDevice` (Task 11)
- `docs/soc_arch/modules/gpu.common.md` → 同上
```

- [ ] **Step 6: 修订 3 main specs 标 superseded**

For each `openspec/specs/{cpptlm-d1-p1-pipeline-scoreboard,gpgpu-precision-wave2,cli-f12b-flag}/spec.md`, append Status Update section.

- [ ] **Step 7: 修订 scripts/test/docs_sync_check.sh VIRTUAL_PATHS**

Append 4 new entries (architecture/11-15) + 12 deleted entries.

- [ ] **Step 8: 修订 test/python/test_f12b_smoke.py L47 kernel_launch 重定位**

Replace `kernel_launch` reference with `streaming_multiprocessor` test scenario.

- [ ] **Step 9: 运行 doc_hygiene 测试验证通过**

Run: `bash test/test_doc_hygiene.sh`
Expected: "DOC HYGIENE PASS".

- [ ] **Step 10: 提交**

```bash
GIT_MASTER=1 git add AGENTS.md \
                   docs/ONBOARDING.md \
                   docs/soc_arch/modules/ \
                   openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/ \
                   openspec/specs/gpgpu-precision-wave2/ \
                   openspec/specs/cli-f12b-flag/ \
                   scripts/test/docs_sync_check.sh \
                   test/python/test_f12b_smoke.py \
                   test/test_doc_hygiene.sh
GIT_MASTER=1 git commit -m "chore(docs): AGENTS.md STRUCTURE + ONBOARDING.md + 7 modules docs + 3 main specs + VIRTUAL_PATHS + test scripts (DOC HYGIENE 全套)"
```

### Task 16: 删除 15 旧测试文件

**Files:**
- Delete: 15 test files (per architecture/15 §15.7.1.B)

- [ ] **Step 1: 物理删除 15 旧测试文件**

Per architecture/15 §15.7.1.B (15 items):
- test_kernel_launch_tlm.cc
- test_kernel_launch_ptx_integration.cc
- test_async_completion_adapter.cc
- test_cuda_core_adapter_timing.cc
- test_latency_tlm_perf.cc
- test_pipeline_tlm.cc
- test_scoreboard_tlm.cc
- test_tensor_core_tlm.cc
- test_gpu_soc_tlm.cc
- test_gpu_compute_unit_tlm.cc
- test_gpu_compute_unit_integration.cc
- test_gpu_soc_phase8a.cc
- test_wavefront_tlm.cc
- test_vector_regfile_tlm.cc
- test_minimal_warp_scheduler_tlm.cc

Run: `git rm test/test_kernel_launch_tlm.cc test/test_kernel_launch_ptx_integration.cc ...` (15 files).

- [ ] **Step 2: 验证 test/CMakeLists.txt 仍正确 (GLOB 自动) + 编译通过**

Run: `cmake --build build`
Expected: PASS.

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add -A
GIT_MASTER=1 git commit -m "refactor(tests): 删除 15 旧测试文件"
```

### Task 17: 新增 12 SM 子模块单测 + L2-L6 集成测试 + L7 JSON reload + test_f12b_smoke 重定位

**Files:**
- Create: 12 个 test_sm_<unit>_tlm.cc 单测
- Create: `test/test_sm_bundle_wiring.cc` (L2)
- Create: `test/test_streaming_multiprocessor_tlm.cc` (L3)
- Create: `test/test_i_compute_device_stepping.cc` (L4)
- Create: `test/test_cdna_hazard_tracker.cc` (L5)
- Create: `test/test_sm_ptx_emu_e2e.cc` (L6)
- Modify: `test/python/test_f12b_smoke.py`

- [ ] **Step 1: 写失败测试 — 12 子模块单测 (各 1 file)**

Create per architecture/15 §15.8.2 L1 测试场景:
- `test_sm_fetch_unit_tlm.cc`: (a) 抓取指令 (b) PC 边界检查 (c) instr buf 空时阻塞
- `test_sm_decode_unit_tlm.cc`: (a) PTX 指令分类 (b) CDNA MFMA 分类 (c) LatencyClass 推断
- `test_sm_issue_unit_tlm.cc`: (a) round-robin 调度 (b) CGGTY 阈值 (c) warpid % 4 静态绑定
- `test_sm_scalar_alu_tlm.cc`: (a) FFMA 延迟 (b) IMAD 延迟 (c) 多 warps 并发
- `test_sm_vector_alu_tlm.cc`: (a) VIADD.U8x4 延迟 (b) FMNMX 路径 (c) V-pipe 共发
- `test_sm_matrix_core_tlm.cc`: (a) MFMA 延迟 (b) ACCVGPR 写入 (c) v_mfma_* 20 条子集
- `test_sm_simt_lane_tlm.cc`: (a) EXEC mask 64-bit (b) SIMT 分歧检测 (c) lane 活跃状态
- `test_sm_lsu_global_tlm.cc`: (a) global_load → IMemoryPort (b) 异步回调处理 (c) vmcnt 递减
- `test_sm_lsu_lds_tlm.cc`: (a) LDS 同步访问 (b) bank conflict 检测 (c) intra-SM 路径
- `test_sm_reg_file_unit_tlm.cc`: (a) 32-bit 读 (b) 64-bit 写 (c) ACCVGPR 累加
- `test_sm_writeback_unit_tlm.cc`: (a) 写回 RegFile (b) release HazardTracker (c) 写回顺序
- `test_sm_hazard_tracker_tlm.cc`: (a) kVirtualReg RAW hazard (b) kHardwareCounter vmcnt (c) s_waitcnt 等待

Each file ~30-50 LOC, totaling ~400 LOC.

- [ ] **Step 2: 写 L2 Bundle 接线测试**

Create `test/test_sm_bundle_wiring.cc` (per architecture/15 §15.8.1 L2, 20+ assertions):
- 8 Bundle 字段验证
- Bundle 流向正确性（Fetch→Decode→Issue→Exec→Writeback→RegFile）
- MemoryReq/Resp 异步配对
- ScoreboardQuery hazard 跟踪

- [ ] **Step 3: 写 L3 SM 顶层集成测试**

Create `test/test_streaming_multiprocessor_tlm.cc` (per architecture/15 §15.8.2 L3, 30+ assertions):
- 5 warps × 32 线程 PTX 步进
- 4 warps × 64 线程 CDNA 步进
- 内存访问 + s_waitcnt 集成
- bit-exact Gate 验证 (per Oracle Round 4 F1.4 决策)

- [ ] **Step 4: 写 L4 IComputeDevice 步进测试**

Create `test/test_i_compute_device_stepping.cc` (per architecture/15 §15.8.2 L4, 25+ assertions):
- 14 方法 smoke test (每个方法 1 assertion)
- 1 tick = 1 cycle 契约
- PTX-EMU facade 兼容性 (与 HSK-8 同构)

- [ ] **Step 5: 写 L5 CDNA waitcnt 计数测试**

Create `test/test_cdna_hazard_tracker.cc` (per architecture/15 §15.8.2 L5, 15+ assertions):
- vmcnt 增/减
- s_waitcnt vmcnt(N) wait until count ≤ N
- per-SM × per-wave 三维数组

- [ ] **Step 6: 写 L6 端端测试**

Create `test/test_sm_ptx_emu_e2e.cc` (per architecture/15 §15.8.2 L6, 20+ assertions):
- PTX-EMU 通过 IComputeDevice 步进 → CppTLM SM 仿真 → 完成 kernel 仿真
- SGEMM kernel 测试
- 与 architecture/12 §12 校准基线对齐

- [ ] **Step 7: 修订 test_f12b_smoke.py (Task 15 已部分, 此处补完)**

Replace kernel_launch test references with StreamingMultiprocessorTLM scenario.

- [ ] **Step 8: 运行所有新测试验证通过**

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-unit][sm-bundle][sm-top][sm-icompute][sm-cdna][sm-e2e][sm-json]"`
Expected: 146+ assertions PASS.

- [ ] **Step 9: 提交**

```bash
GIT_MASTER=1 git add test/test_sm_*.cc test/test_streaming_multiprocessor_tlm.cc test/test_i_compute_device_stepping.cc test/test_cdna_hazard_tracker.cc test/test_sm_ptx_emu_e2e.cc test/python/test_f12b_smoke.py
GIT_MASTER=1 git commit -m "feat(tests): 新增 12 SM 子模块单测 + L2-L6 集成测试 + L7 JSON reload 测试 + test_f12b_smoke 重定位 (146+ assertions)"
```

### Task 18: 完整实现 IComputeDevice 14 方法与同步协议 (含 Gate 验证)

**Files:**
- Modify: `src/tlm/gpu/streaming_multiprocessor_tlm.cc` (full impl)
- Modify: 12 submodule `.cc` (full impl per Task 7 + Gate)
- Create: `src/tlm/gpu/sm/bit_exact_gate.cc` (new — dual-compute verifier)

- [ ] **Step 1: 写失败测试 — bit-exact Gate**

Create `test/test_bit_exact_gate.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "tlm/gpu/sm/bit_exact_gate.hh"

TEST_CASE("Bit-exact Gate: PTX-EMU functional == SM Exec ALU", "[sm-gate][cdna-phase-a]") {
    BitExactGate gate;
    auto ptx_result = gate.compute_ptx_functional("v_add_f32", {1.0f, 2.0f});
    auto sm_result = gate.compute_sm_exec("v_add_f32", {1.0f, 2.0f});
    REQUIRE(ptx_result == sm_result);
}
```

Run: should fail.

- [ ] **Step 2: 实现 BitExactGate**

Create `include/tlm/gpu/sm/bit_exact_gate.hh` + `src/tlm/gpu/sm/bit_exact_gate.cc`:
```cpp
class BitExactGate {
public:
    // PTX-EMU functional: 解码 PTX + 算 ALU + 仅用于控制流 (不下行)
    std::variant<float, int64_t, uint64_t> compute_ptx_functional(
        const std::string& instr_name, std::vector<double> src_values);
    
    // SM Exec: 同样 ALU 但写入 RegFileUnit 作为 timing 真值
    std::variant<float, int64_t, uint64_t> compute_sm_exec(
        const std::string& instr_name, std::vector<double> src_values);
    
    // 每次 set_instr_descriptor_buf 后 PTX-EMU 调用
    bool verify_bit_exact(uint64_t expected_emu, uint64_t actual_sm);
};
```

Implement with explicit FP32/INT32/INT64 ALU bit-exact semantics (refer to PTX-EMU functional executor + ACCVGPR MFMA accumulation).

- [ ] **Step 3: 完整实现 SM 顶层 tick() 与 14 方法**

Modify `src/tlm/gpu/streaming_multiprocessor_tlm.cc`:
- `tick()`: 协调 12 子模块 + 调用 bit_exact Gate 验证
- `exe_once()`: 1 cycle step
- `sm_exe_once()` / `warp_exe_once()`: per SM/warp
- `set_instr_descriptor_buf()`: 接收 buf + 触发 FetchUnit 抓取 + 后续流水
- `get_register_value()`: 委托 RegFileUnit + 处理 lane_id
- `is_instruction_completed()`: 委托 HazardTracker
- `set_active_mask()` / `set_next_pc()`: 委托 SIMTLane
- `set_scoreboard()`: 委托 HazardTracker (kVirtualReg)
- `reset()`: 清所有子模块状态

- [ ] **Step 4: 完整实现 12 子模块 .cc (**

补全 Task 7 的 stub, 加入真实 ALU / Load / Store / Branch 逻辑 + HazardTracker 完整实现 (kVirtualReg + kHardwareCounter stub)

- [ ] **Step 5: 运行 bit-exact Gate 测试通过**

Run: `cmake --build build && ./build/bin/cpptlm_tests "[sm-gate]"`
Expected: PASS.

- [ ] **Step 6: 全量回归**

Run: `./build/bin/cpptlm_tests "[pcie]"` + `[axi]` + `[gpu]` + `[e2e]` + `[sm-microarch]` + `[wave2]`
Expected: All PASS (per Gate D 14 项).

- [ ] **Step 7: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/bit_exact_gate.hh \
                   src/tlm/gpu/sm/bit_exact_gate.cc \
                   src/tlm/gpu/streaming_multiprocessor_tlm.cc \
                   src/tlm/gpu/sm/*.cc
GIT_MASTER=1 git commit -m "feat(tlm): 完整实现 IComputeDevice 与 14 方法同步协议 + Execute 单元真实逻辑 + bit-exact Gate"
```

### Task 19: Archive OpenSpec change + 同步 main specs

**Files:**
- Modify: `openspec/specs/cdna-isa-abstraction/spec.md` (Stage A 归档, 标 superseded)
- Modify: `openspec/specs/sm-microarchitecture/spec.md` (archive 同步)

- [ ] **Step 1: 验证 openspec validate**

Run: `openspec validate cpptlm-dgpu-d1-cdna-isa-sm-rewrite --strict`
Expected: PASS.

- [ ] **Step 2: Archive stage-a change**

Run: `openspec archive cpptlm-dgpu-d1-cdna-isa-phase-a --reason "Superseded by cpptlm-dgpu-d1-cdna-isa-sm-rewrite (commit `650e9e9`); 阶段 A 双轨并存决策被 SM 重构反转"`

- [ ] **Step 3: Archive sm-rewrite change (after Gate passes)**

Run: `openspec archive cpptlm-dgpu-d1-cdna-isa-sm-rewrite --reason "SM rewrite complete (Gate 14 项 PASS)"`
Expected: Archives spec.md to `openspec/specs/sm-microarchitecture/spec.md`.

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add openspec/changes/archive/ openspec/specs/
GIT_MASTER=1 git commit -m "chore(openspec): archive cpptlm-dgpu-d1-cdna-isa-sm-rewrite + cpptlm-dgpu-d1-cdna-isa-phase-a (superseded)"
```

### Task 20: HSK-9 公告正式发布 + PTX-EMU 端 sm_context 改造同步

**Files:**
- Modify: `external/PTX-EMU/docs/superpowers/specs/2027-02-09-hsk-9-cpptlm-sm-rewrite.md` (publish per architecture/15 §15.6.3 final draft)
- Modify: `external/PTX-EMU/src/ptxemu/device_api_impl.cc` (attach_timing → deprecated stub)
- Modify: `external/PTX-EMU/src/ptxsim/core/sm_context.cpp` (remove IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming usage)
- Modify: `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.cpp` (remove or refactor)

**Note**: Tasks 20 在 PTX-EMU 仓执行 (submodule), 不能直接在本仓 commit。改用发布 HSK-9 公告文件 + 等 PTX-EMU 端反馈。

- [ ] **Step 1: 发布 HSK-9 公告到 PTX-EMU 仓 docs/superpowers/specs/**

This is a cross-repo action; create the announcement file in CppTLM `external/PTX-EMU/docs/superpowers/specs/` (via submodule push) OR copy to `docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md` for reference.

Create `docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md` mirroring the final draft content from `docs/soc_arch/adr/hsk9-announcement-draft.md`.

- [ ] **Step 2: PTX-EMU 端 attach_timing → deprecated stub (per F3.1)**

Document the expected change for PTX-EMU maintainers:
- `external/PTX-EMU/src/ptxemu/device_api_impl.cc`: `attach_timing()` body改为:
```cpp
void IPtxEmuDeviceImpl::attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*) {
    [[deprecated("CppTLM SM rewrite: use set_instr_descriptor_buf() instead")]];
    // body empty
}
```

- [ ] **Step 3: PTX-EMU 端 sm_context.cpp 改造 (L34/67/206)**

Document expected change for PTX-EMU maintainers:
- Replace `IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming` usage with `IComputeDevice::set_instr_descriptor_buf()`
- Remove `sm_context_cpptlm_inject.{h,cpp}` cross-injection

- [ ] **Step 4: PTX-EMU 端 3 测试重定位**

Document expected change:
- `tests/integration/cpptlm/test_attach_timing_consumer_e2e.cpp` → rename to `test_sm_ptx_emu_e2e`
- `tests/unit/ptxemu/test_device_api_attach_timing.cpp` → rename to `test_i_compute_device_attach_timing`
- `tests/unit/cpptlm/test_cpptlm_injection_interfaces.cpp` → 删除 (interfaces 已废)

- [ ] **Step 5: 等待 PTX-EMU 端 14 天反馈窗口**

(本任务剩余时长等待; 不阻塞后续 CppTLM 工作)

- [ ] **Step 6: 提交 (CppTLM 侧 HSK-9 公告归档)**

```bash
GIT_MASTER=1 git add docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md docs/soc_arch/adr/hsk9-announcement-draft.md
GIT_MASTER=1 git commit -m "chore(cross-repo): HSK-9 公告正式发布 + PTX-EMU 端 sm_context.cpp 改造跟踪 (attach_timing deprecated stub)"
```

---

## Self-Review (per writing-plans skill)

**1. Spec coverage**: 架构文档 v5.0 15 节每节都有对应任务:
- §15.2 顶层架构 → Task 4, 7, 9, 10
- §15.3 12 子模块 → Task 5, 7
- §15.4 8 Bundle → Task 6
- §15.5 IComputeDevice + 同步协议 → Task 4, 18
- §15.6 HSK-9 + 23 ABI + SFU → Task 1, 11, 20
- §15.7 删除范围 (11 实现 + 15 测试 + 4 JSON + 旁路 + DOC HYGIENE) → Task 11, 12, 13, 14, 15, 16
- §15.8 测试策略 (L1-L6 + L7) → Task 5, 6, 7, 8, 17
- §15.9 20 commits → Task 1-20 本身是顺序拆分
- §15.10 Gate (14 项) → Task 18 步骤 6 全量回归覆盖
- §15.12 评审汇总 → Task 19 (归档 supersede)

**2. Placeholder scan**: 检查每 Task 的代码块完整（无 TBD/TODO/fill in details）,每个 step 都有具体命令与预期输出。✓

**3. Type consistency**:
- `IComputeDevice` 14 方法在 Task 4 定义 (Step 2) 与 Task 18 Step 3 实现一致
- `InstrDescriptor` 字段 (isa_type, instr_id, result_value, memory_data) 在 Task 4 Step 2 与 §15.5.6 一致
- `PipeClass`/`LatencyClass`/`CtrlBits` enum 在 Task 4 引用 Task 7 实现, 8 Bundle 在 Task 6 定义与 Task 7 引用一致
- `BitExactGate` 在 Task 18 Step 1 测试定义, Step 2 实现, Task 18 Step 3 在 SM 顶层 tick() 协调调用

**4. 实施就绪**: 20 个 Task 覆盖 971 行设计文档全部需求;每个 Task 都有 fail → implement → pass → commit 4-5 step TDD 闭环。

---

## 总工作量 (per Oracle Round 2 评估)

- **Tasks 1-6 (文档 + 接口 + Bundle)**: 2-3 人天
- **Tasks 7-8 (12 子模块 + chstream_register)**: 5-7 人天
- **Tasks 9-11 (旧模块重构 + 旁路修复)**: 3-4 人天
- **Tasks 12-13 (删除)**: 0.5 人天
- **Tasks 14-16 (JSON + DOC HYGIENE + 测试删除)**: 2-3 人天
- **Tasks 17-18 (新测试 + 完整实现)**: 6-8 人天
- **Tasks 19-20 (archive + HSK-9 发布)**: 1-2 人天
- **PTX-EMU 端改造 (Task 20 部分)**: 5-10 人天（外仓成本, 由 PTX-EMU 团队负责）

**合计**: 25-30 工作日（CppTLM 侧） + 5-10 工作日（PTX-EMU 侧）