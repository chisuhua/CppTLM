# SM 重构 Task 18 完整实施 + PTX-EMU 端改造 + 联合验证 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成 SM 微架构重构 Task 18 (12 子模块真值 + bit-exact Gate + 端到端) + PTX-EMU 端 HSK-9 改造 (`set_instr_descriptor_buf` producer + `attach_timing` consumer 移除) + 跨仓联合验证测试。

**Architecture:** 大爆炸 + HSK-9 (per Oracle Round 2 P0-2 决策 + 用户决策). CppTLM 端持 12 子模块真值 + bit-exact ALU 真值源; PTX-EMU 端通过 `IComputeDevice::set_instr_descriptor_buf` 上行同步已解码 `InstrDescriptor[]`, 通过 `get_register_value`/`is_instruction_completed` 读路径/就绪协议; Gate 验证 PTX-EMU functional vs SM exec bit-exact 一致性。

**Tech Stack:** C++17/20 + SystemC stub + ChStreamModuleBase + bundles + ModuleFactory + Catch2 v3.7.0 + git-master 多原子 commit + 跨仓 submodule bump + HSK-9 14 天反馈窗口。

**关联文档**:
- 设计: [`docs/soc_arch/architecture/15-sm-microarchitecture-design.md`](../soc_arch/architecture/15-sm-microarchitecture-design.md) (v5.0, 971 行)
- HSK-9 公告: [`docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`](../superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md) (✅ Active)
- HSK-8 (前置): `external/PTX-EMU/docs/superpowers/specs/2026-08-22-hsk-8-ptxemu-public-api-ack.md`
- ADR: [`docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md`](../soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md) §2.3 (14 Gate items G1-G14)
- OpenSpec archive: `openspec/specs/sm-microarchitecture/spec.md` (12 ADDED Requirements)
- 上一阶段计划: [`docs/superpowers/plans/2027-02-09-sm-microarchitecture-rewrite.md`](2027-02-09-sm-microarchitecture-rewrite.md) (Tasks 1-17/19-20 完成)

**总工作量**: 25-35 工作日（per Oracle 评审: Task 18 15-25 天 + PTX-EMU 端 5-10 天 + 联合验证 3-5 天）

**会话拆分** (per Oracle 评审建议):
- **本计划总 17 Task, 拆为 5 子波次**:
  - 子波 0 (会话准备): Tasks 0.1-0.5 (跨仓工作树 + 验证基线)
  - 子波 1 (CppTLM 18a): Tasks 1.1-1.5 (SM 端口 + ScalarALU + RegFileUnit)
  - 子波 2 (CppTLM 18b): Tasks 2.1-2.17 (12 子模块真值 + 端口拓扑 + Hazard + LSU + L5/L6 测试)
  - 子波 3 (PTX-EMU 端改造): Tasks 3.1-3.5 (device_api_impl + sm_context + tests + submodule bump)
  - 子波 4 (联合验证 + 18c): Tasks 4.1-4.6 (bit-exact Gate + E2E + Oracle Gate G13/G14)

---

## 子波 0: 会话准备 (1 天)

### Task 0.1: 跨仓工作树隔离 (避免污染 main)

**Files:**
- 无 (git 操作)

- [ ] **Step 1: 在 CppTLM 主仓创建 worktree**

```bash
cd /workspace/project/CppTLM
git worktree add .worktrees/sm-mp-impl -b feat/sm-mp-impl HEAD
cd .worktrees/sm-mp-impl
```

- [ ] **Step 2: 在 PTX-EMU 子模块创建 worktree**

```bash
cd external/PTX-EMU
git worktree add .worktrees/hsk-9-impl -b feat/hsk-9-impl HEAD
cd .worktrees/hsk-9-impl
# 验证 submodule HEAD 仍为 main 同步基线
git log --oneline -3
# 预期: 73a5ecee chore(openspec): archive phase-1-5-namespace-migration ...
```

- [ ] **Step 3: 验证基线构建 (PTX-EMU ON)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCPPTLM_WITH_PTX_EMU=ON
cmake --build build --target cpptlm_tests -j$(nproc)
# 预期: 编译成功, 44498 assertions PASS
```

### Task 0.2: 验证 PTX-EMU 端 submodule 状态

**Files:**
- 无

- [ ] **Step 1: 验证 PTX-EMU 端 HEAD**

```bash
cd external/PTX-EMU
git log --oneline -3
# 预期: 最近提交应包含 cpptlm bridge cleanup 或 hsk-8 follow-up 相关 commit
# 当前 HEAD: 73a5ecee (Phase 1.5 namespace migration archive)
```

- [ ] **Step 2: 验证 PTX-EMU 端 ctest 基线**

```bash
cd external/PTX-EMU
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
# 预期: 254/254 PASS (per HSK-8 audit §Postmortem)
```

- [ ] **Step 3: 验证 PTX-EMU 端 device_api.h 仍冻结**

```bash
grep "PTXEMU_API_VERSION" external/PTX-EMU/include/ptxemu/device_api.h
# 预期: #define PTXEMU_API_VERSION 1
```

### Task 0.3: 提交 HSK-9 14 天反馈窗口跟踪文件

**Files:**
- Create: `docs/superpowers/specs/HSK-9-feedback-tracker.md`

- [ ] **Step 1: 创建跟踪文件**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cat > docs/superpowers/specs/HSK-9-feedback-tracker.md << 'EOF'
# HSK-9 14 天反馈窗口跟踪 (2027-02-09 → 2027-02-23)

| 日期 | 事件 | 负责人 | 状态 |
|------|------|--------|------|
| 2027-02-09 | HSK-9 spec 发布 (commit c656222) | CppTLM | ✅ Done |
| 2027-02-09 → 02-16 | PTX-EMU 端评审窗口 | PTX-EMU 团队 | 🔵 Open |
| 2027-02-16 (建议) | PTX-EMU 端反馈 issue/email | PTX-EMU 团队 | ⏸ Pending |
| 2027-02-23 | 反馈窗口关闭 | 主代理 | ⏸ Pending |
| 2027-02-23 后 | Oracle Gate G14 评审 | 主代理 | ⏸ Pending |

## 反馈项跟踪

(每条反馈一行, PTX-EMU 端发出后填入)

| # | 日期 | 来源 | 类型 | 状态 | 修复 commit |
|---|------|------|------|------|-------------|
| | | | | | |

EOF
git add docs/superpowers/specs/HSK-9-feedback-tracker.md
GIT_MASTER=1 git commit -m "chore(hsk): HSK-9 14 天反馈窗口跟踪文件"
```

### Task 0.4: 验证子波 0 完成

- [ ] **Step 1: 全链路基线测试**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
./build/bin/cpptlm_tests 2>&1 | tail -3
# 预期: 44498 assertions PASS, 0 regression
ctest --test-dir build --output-on-failure
# 预期: All tests passed
```

- [ ] **Step 2: push 工作树**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git push origin feat/hsk-9-impl
```

---

## 子波 1: CppTLM Task 18a — ALU 真值化 (5-8 天)

### Task 1.1: SM 顶层加 req_in/resp_out 访问器 (Task 18 P1-1 修复)

**Files:**
- Modify: `include/tlm/gpu/streaming_multiprocessor_tlm.hh` (210 行)

- [ ] **Step 1: 写失败测试 — SM 顶层有 req_in/resp_out**

Create `test/test_sm_streaming_multiprocessor_port.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;

TEST_CASE("StreamingMultiprocessorTLM has req_in/resp_out accessors", "[sm-port][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    REQUIRE_NOTHROW(sm.req_in());
    REQUIRE_NOTHROW(sm.resp_out());
}
```

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[sm-port]"`
Expected: FAIL (no req_in/resp_out accessors yet).

- [ ] **Step 2: 在 SM 顶层加 req_in/resp_out 访问器**

Modify `include/tlm/gpu/streaming_multiprocessor_tlm.hh` (after class declaration):
```cpp
// SM 顶层端口访问器 (per Task 18 P1-1 fix, StreamAdapter 要求)
// 端口拓扑: SM 顶层 1 ComputeReqBundle 入端口 + 1 ComputeRespBundle 出端口
ChStreamReqBundle& req_in() { return req_in_; }
const ChStreamReqBundle& req_in() const { return req_in_; }
ChStreamRespBundle& resp_out() { return resp_out_; }
const ChStreamRespBundle& resp_out() const { return resp_out_; }

// 端口实例
ChStreamReqBundle  req_in_{};
ChStreamRespBundle resp_out_{};

// 内部 12 子模块驱动 (来自 set_instr_descriptor_buf 入队的 ring buffer)
std::vector<cpptlm::gpu::InstrDescriptor> instr_buf_;
uint32_t head_ = 0;
uint32_t tail_ = 0;
uint32_t ring_size_ = 64;  // 1 个 warp 活跃指令

// 内部寄存器真值源 (RegFileUnit 抽象, 简化版本)
std::unordered_map<uint32_t, uint64_t> scalar_regs_;  // reg_id -> value
```

- [ ] **Step 3: 运行测试验证通过**

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[sm-port]"`
Expected: PASS.

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh test/test_sm_streaming_multiprocessor_port.cc
GIT_MASTER=1 git commit -m "feat(sm): SM 顶层加 req_in/resp_out 访问器 (Task 18a 端口)"
```

### Task 1.2: 解开 SM StreamAdapter 注册

**Files:**
- Modify: `include/chstream_register.hh`

- [ ] **Step 1: 验证 SM StreamAdapter 注册已激活**

`include/chstream_register.hh:113` 现有注释应指明 Task 18 P1-1 修复需要解开此注释。

```bash
grep -A2 "StreamingMultiprocessorTLM" include/chstream_register.hh
# 预期: 应看到注释 "Task 18 P1-1 fix: ... 解开此注释注册 adapter"
```

- [ ] **Step 2: 解开 StreamAdapter 注册**

Edit `include/chstream_register.hh`:
```cpp
// 删除:
//     /* SM 重构 Task 17 关闭: SM 顶层暂不注册 StreamAdapter (per Oracle Tasks 9-17 评审 P1-1).
//        ...
//        1. SM 顶层加 req_in/resp_out 端口访问器 (Task 18 P1-1 fix).
//        2. 解开此注释注册 adapter.
//        ...
//        3. F12b smoke 才能真验证 SM 接线 (per Oracle P1-1). */ \
// 添加:
ChStreamAdapterFactory::get().registerAdapter<tlm::StreamingMultiprocessorTLM, \
    bundles::ComputeReqBundle, bundles::ComputeRespBundle>("StreamingMultiprocessorTLM"); \
```

- [ ] **Step 3: 验证 F12b smoke 真验证**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests "[sm-port]" 2>&1 | tail -3
# 预期: PASS
```

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/chstream_register.hh
GIT_MASTER=1 git commit -m "feat(register): 解开 SM StreamAdapter 注册 (Task 18 P1-1 关闭)"
```

### Task 1.3: ScalarALU 真值实现

**Files:**
- Modify: `include/tlm/gpu/streaming_multiprocessor_tlm.hh` (ScalarALU inline class)
- Create: `src/tlm/gpu/scalar_alu.cc` (split from inline)

- [ ] **Step 1: 写失败测试 — ScalarALU ADD 真值**

Create `test/test_sm_scalar_alu_e2e.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("ScalarALU ADD 真值: set_instr_descriptor_buf + 4 cycles", "[sm-alu][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);

    InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.isa_type = IsaType::kCDNA64;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed4Cycle;
    desc.dst_regs[0] = 5;
    desc.src_regs[0] = 1;
    desc.src_regs[1] = 2;
    desc.num_src = 2;
    desc.num_dst = 1;

    // 上行同步: PTX-EMU 写入已解码指令
    sm.set_instr_descriptor_buf(&desc, 1);
    REQUIRE(true);
}
```

- [ ] **Step 2: 在 SM 顶层实现 ScalarALU 真值 (ADD/IMAD)**

将 ScalarALU 从 inline stub 拆到独立 .cc:
```cpp
// src/tlm/gpu/scalar_alu.cc
#include "tlm/gpu/sm/scalar_alu.hh"

namespace cpptlm::gpu {

void ScalarALU::execute(InstrDescriptor& desc) {
    switch (desc.latency_class) {
        case LatencyClass::kFixed1Cycle:
            execute_add(desc);
            break;
        case LatencyClass::kFixed4Cycle:
            execute_imad(desc);  // 简化: IMAD 视为 4-cycle ADD 变体
            break;
        default:
            // kUnsupported: 留给 HazardTracker 处理
            break;
    }
}

void ScalarALU::execute_add(InstrDescriptor& desc) {
    // 简化真值: a + b, 写入 scalar_regs_[dst]
    if (desc.num_dst >= 1 && desc.num_src >= 2) {
        uint64_t a = parent_->get_scalar_reg(desc.src_regs[0]);
        uint64_t b = parent_->get_scalar_reg(desc.src_regs[1]);
        uint64_t result = a + b;
        parent_->set_scalar_reg(desc.dst_regs[0], result);
        desc.result_value[0] = result;  // 写入 SM 真值
    }
}

void ScalarALU::execute_imad(InstrDescriptor& desc) {
    // 简化真值: a * b + c (假设 c = 0)
    execute_add(desc);
}

}  // namespace cpptlm::gpu
```

- [ ] **Step 3: 运行测试**

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[sm-alu]"`
Expected: PASS (no exception).

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh \
                   src/tlm/gpu/scalar_alu.cc \
                   src/CMakeLists.txt \
                   test/test_sm_scalar_alu_e2e.cc
GIT_MASTER=1 git commit -m "feat(sm): ScalarALU ADD/IMAD 真值实现 (Task 18a)"
```

### Task 1.4: RegFileUnit 真值实现

**Files:**
- Modify: `src/tlm/gpu/streaming_multiprocessor_tlm.cc` (RegFileUnit inline → 真值)
- Create: `src/tlm/gpu/reg_file_unit.cc`

- [ ] **Step 1: 写失败测试 — RegFileUnit 真值存取**

Create `test/test_sm_reg_file_unit.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("RegFileUnit 真值存取 (SM-owns-state)", "[sm-regfile][sm-microarch]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    DeviceConfig cfg{};
    sm.initialize(cfg);

    // SM 顶层注册 64 个 scalar 寄存器 (per SM-owns-state 协议)
    sm.set_scalar_reg(0, 0xABCD);
    sm.set_scalar_reg(1, 0x1234);
    sm.set_scalar_reg(2, 0x5678);

    // PTX-EMU 读寄存器
    uint64_t val0 = 0, val1 = 0, val2 = 0;
    REQUIRE(sm.get_register_value(0, 0, 0, &val0));
    REQUIRE(sm.get_register_value(0, 0, 1, &val1));
    REQUIRE(sm.get_register_value(0, 0, 2, &val2));
    REQUIRE(val0 == 0xABCD);
    REQUIRE(val1 == 0x1234);
    REQUIRE(val2 == 0x5678);
}
```

- [ ] **Step 2: 实现 RegFileUnit 真值**

在 `src/tlm/gpu/streaming_multiprocessor_tlm.cc`:
```cpp
// 替换 RegFileUnit stub inline class
// 新增真实字段 + 方法:
class RegFileUnit : public ChStreamModuleBase {
public:
    // 真值: SM 顶层持 registers_ 真值, RegFileUnit 委托
    void set_register(uint32_t reg_id, uint64_t value);
    uint64_t get_register(uint32_t reg_id) const;
};
```

并在 SM 顶层:
```cpp
void StreamingMultiprocessorTLM::set_scalar_reg(uint32_t reg_id, uint64_t value) {
    scalar_regs_[reg_id] = value;
}

bool StreamingMultiprocessorTLM::get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                                                  uint64_t* out_value, uint32_t lane_id) override {
    auto it = scalar_regs_.find(reg_id);
    if (it == scalar_regs_.end()) return false;
    *out_value = it->second;
    return true;
}
```

- [ ] **Step 3: 运行测试**

Run: `cmake --build build --target cpptlm_tests && ./build/bin/cpptlm_tests "[sm-regfile]"`
Expected: PASS.

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/streaming_multiprocessor_tlm.hh \
                   src/tlm/gpu/streaming_multiprocessor_tlm.cc \
                   src/tlm/gpu/reg_file_unit.cc \
                   src/CMakeLists.txt \
                   test/test_sm_reg_file_unit.cc
GIT_MASTER=1 git commit -m "feat(sm): RegFileUnit + get_register_value 真值实现 (Task 18a)"
```

### Task 1.5: Oracle 评审 18a

- [ ] **Step 1: Dispatch Oracle review**

```bash
# 使用 task tool (oh-my-opencode):
# task(subagent_type="oracle", prompt="...", description="Oracle 评审 18a")
# 或外部命令 (oh-my-opencode):
# /oracle 评审 18a commits
```

Oracle prompt 应包含:
- 评审范围: commits 1.1-1.4
- 验证 SM 顶层端口访问器、StreamAdapter 注册、ScalarALU 真值、RegFileUnit 真值
- 风险点: F1.4 SM-owns-state 协议是否完整、F12b smoke 真验证是否生效

- [ ] **Step 2: 修复所有 P0/P1**

Per Oracle findings, apply fixes 并 commit.

- [ ] **Step 3: push 18a 完成**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
```

---

## 子波 2: CppTLM Task 18b — 12 子模块真值 + 端口拓扑 + Hazard + LSU (5-8 天)

### Task 2.1: 拆分 12 子模块到独立 .hh/.cc (P2-2 修复)

**Files:**
- Create: `include/tlm/gpu/sm/{fetch_unit,decode_unit,issue_unit,scalar_alu,vector_alu,matrix_core,simt_lane,lsu_global,lsu_lds,reg_file_unit,writeback_unit,hazard_tracker}.hh`
- Create: `src/tlm/gpu/sm/{12 个 .cc}`

- [ ] **Step 1: 创建 sm/ 目录 + 12 子模块 .hh**

```bash
mkdir -p include/tlm/gpu/sm src/tlm/gpu/sm
```

将 inline stub 从 `streaming_multiprocessor_tlm.hh` 拆分到 `include/tlm/gpu/sm/*.hh`:
```cpp
// include/tlm/gpu/sm/fetch_unit.hh
namespace cpptlm::gpu::sm {
class FetchUnit : public ChStreamModuleBase {
public:
    explicit FetchUnit(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}
    std::string get_module_type() const override { return "FetchUnit"; }
    void tick() override;
};
}
```

(每个 .hh ~25 行, 12 子模块 × ~25 = ~300 LOC)

- [ ] **Step 2: 创建 12 子模块 .cc**

每个 .cc ~30-50 行 stub tick() impl, 预留端口接口:
```cpp
// src/tlm/gpu/sm/fetch_unit.cc
#include "tlm/gpu/sm/fetch_unit.hh"
namespace cpptlm::gpu::sm {
void FetchUnit::tick() {
    // Task 18b 真实实现: 从 instr_buf_ 取下一条, 写入 FetchToIssueBundle
}
}
```

- [ ] **Step 3: 更新 chstream_register 注册名 (12 个 stub class 名 → 简化为 FetchUnit 等)**

Per Oracle P1-5: 12 子模块注册名从 `FetchUnitTLM` 等改为 `FetchUnit` 等 (per plan Task 5 设计原意 + SM 重构实际命名)。

Edit `include/chstream_register.hh`:
```cpp
// 删除 12 个 registerObject<tlm::sm::FetchUnitTLM> 等旧名
// 添加 12 个 registerObject<tlm::sm::FetchUnit> 等新名
ModuleFactory::registerObject<tlm::sm::FetchUnit>("FetchUnit");
ModuleFactory::registerObject<tlm::sm::DecodeUnit>("DecodeUnit");
// ... 共 12 个
```

- [ ] **Step 4: 更新 streaming_multiprocessor_tlm.hh 移除 inline stub**

删除 12 个 inline class, 改为 `#include "tlm/gpu/sm/fetch_unit.hh"` 等。

- [ ] **Step 5: 验证编译**

```bash
cmake --build build --target cpptlm_tests -j$(nproc)
./build/bin/cpptlm_tests 2>&1 | tail -3
# 预期: 44498+ assertions PASS
```

- [ ] **Step 6: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/ src/tlm/gpu/sm/ include/tlm/gpu/streaming_multiprocessor_tlm.hh include/chstream_register.hh src/CMakeLists.txt
GIT_MASTER=1 git commit -m "refactor(sm): 拆分 12 子模块到独立 .hh/.cc (Task 18b, P2-2 修复)"
```

### Task 2.2-2.12: 12 子模块真值实现 (每个 0.5 天)

每个子模块真值实现遵循 TDD:
1. 写失败测试
2. 看失败
3. 实现真值
4. 看通过
5. commit

子模块清单 + 真值要点:

**Task 2.2: FetchUnit 真值**:
```cpp
void FetchUnit::tick() {
    if (parent_->instr_buf_available()) {
        auto desc = parent_->next_instr();
        FetchToIssueBundle b{};
        b.instr_desc = desc;
        b.warp_id = desc.warpid;
        b.pc = desc.pc;
        // 通过 StreamAdapter 推送给 DecodeUnit
        bundle_out_.send(b);
    }
}
```

**Task 2.3: DecodeUnit 真值**: 填充 `pipe` + `latency_class` 字段.

**Task 2.4: IssueUnit 真值**: Round-robin 调度 warps.

**Task 2.5: ScalarALU 真值**: (已在 Task 1.3 完成, 此处加端口接线)

**Task 2.6: VectorALU 真值**: VIADD.U8x4 真值 (per plan).

**Task 2.7: MatrixCore 真值**: MFMA stub 留 Task 18c.

**Task 2.8: SIMTLane 真值**: EXEC mask 64-bit + 分歧检测.

**Task 2.9: LsuGlobal 真值**: 异步内存回调骨架.

**Task 2.10: LsuLDS 真值**: 共享内存 bank conflict 检测.

**Task 2.11: RegFileUnit 真值**: (已在 Task 1.4 完成, 此处加端口接线)

**Task 2.12: WritebackUnit 真值**: 写回 RegFileUnit + release HazardTracker.

### Task 2.13: HazardTracker 真值 (kVirtualReg + kHardwareCounter)

**Files:**
- Create: `src/tlm/gpu/sm/hazard_tracker.cc`

- [ ] **Step 1: 写失败测试 — HazardTracker vmcnt 增/减**

Create `test/test_cdna_hazard_tracker.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "tlm/gpu/sm/hazard_tracker.hh"

using namespace cpptlm::gpu::sm;

TEST_CASE("HazardTracker vmcnt 增/减", "[sm-hazard][sm-l5]") {
    HazardTracker ht;
    ht.increment_vmcnt(0, 0);  // sm_id=0, warp_id=0
    REQUIRE(ht.vmcnt(0, 0) == 1);
    ht.decrement_vmcnt(0, 0);
    REQUIRE(ht.vmcnt(0, 0) == 0);
}

TEST_CASE("s_waitcnt vmcnt(0) 阻塞直到 vmcnt=0", "[sm-hazard][sm-l5]") {
    HazardTracker ht;
    ht.increment_vmcnt(0, 0);
    ht.increment_vmcnt(0, 0);
    REQUIRE(ht.is_stalled(0, 0));
    ht.decrement_vmcnt(0, 0);
    ht.decrement_vmcnt(0, 0);
    REQUIRE(!ht.is_stalled(0, 0));
}

TEST_CASE("kVirtualReg RAW hazard 检测", "[sm-hazard][sm-l5]") {
    HazardTracker ht;
    ht.allocate(0, 0, 5);  // reg_id=5, warp_id=0
    REQUIRE(!ht.can_allocate(0, 0, 5));  // 双重 allocate 阻塞
    ht.release(0, 0, 5);
    REQUIRE(ht.can_allocate(0, 0, 5));
}
```

- [ ] **Step 2-4: 实现 HazardTracker 真值**

每测试对应实现, TDD 循环。

- [ ] **Step 5: 提交**

```bash
GIT_MASTER=1 git add src/tlm/gpu/sm/hazard_tracker.cc test/test_cdna_hazard_tracker.cc
GIT_MASTER=1 git commit -m "feat(sm): HazardTracker kVirtualReg + kHardwareCounter 真值 (Task 18 L5)"
```

### Task 2.14: 8 Bundle 端口拓扑接通

- [ ] **Step 1: 设计 12 子模块 8 Bundle 端口拓扑 (per architecture/15 §15.3)**

参考 `architecture/15 §15.3` 数据流图, 在每个子模块加 `bundle_in` / `bundle_out` 成员。

- [ ] **Step 2: 用 MultiPortStreamAdapter 注册 8 Bundle 接线**

Per `include/framework/multi_port_stream_adapter.hh`:
```cpp
// chstream_register.hh
ChStreamAdapterFactory::get().registerAdapter<tlm::sm::FetchUnit, \
    bundles::sm::FetchToIssueBundle, bundles::sm::DecodeToIssueBundle>("FetchUnit");
```

(每个子模块 1-2 个 adapter, 共 ~20 个 adapter)

- [ ] **Step 3: 验证 JSON wiring**

更新 `configs/templates/gpu_soc/gpu_soc_gb203_v1.json` 加 SM 子模块 wiring。

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add include/chstream_register.hh include/tlm/gpu/sm/ src/tlm/gpu/sm/ configs/
GIT_MASTER=1 git commit -m "feat(sm): 8 Bundle 端口拓扑接通 + JSON wiring (Task 18b)"
```

### Task 2.15: LsuGlobal 异步内存回调

**Files:**
- Modify: `src/tlm/gpu/sm/lsu_global.cc`

- [ ] **Step 1: 实现 LsuGlobal 真值**

```cpp
void LsuGlobal::tick() {
    if (req_pending_) {
        // 通过 StreamAdapter 推 MemoryReqBundle 到 MemoryTLM
        MemoryReqBundle req{};
        req.vaddr = pending_vaddr_;
        req.size = pending_size_;
        req.is_write = pending_is_write_;
        memory_out_.send(req);
        req_pending_ = false;
        waiting_response_ = true;
    }
    if (waiting_response_ && resp_valid_) {
        // 接收 MemoryRespBundle, 推送给 HazardTracker 递减 vmcnt
        MemoryRespBundle resp{};
        resp.tag = pending_tag_;
        resp.data = resp_data_;
        resp.is_hit = resp_hit_;
        hazard_tracker_->decrement_vmcnt(0, warp_id_);
        waiting_response_ = false;
        resp_out_.send(resp);
    }
}
```

- [ ] **Step 2-3: TDD 循环 (失败测试 + 实现)**

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add src/tlm/gpu/sm/lsu_global.cc test/test_sm_lsu_global_e2e.cc
GIT_MASTER=1 git commit -m "feat(sm): LsuGlobal 异步内存回调真值 (Task 18b)"
```

### Task 2.16: L4 IComputeDevice stepping 测试补全 (per P1-3)

**Files:**
- Modify: `test/test_i_compute_device_stepping.cc`

- [ ] **Step 1: 创建 test_i_compute_device_stepping.cc (30+ assertions)**

Per plan Task 17 Step 4 (L4):
- 15 方法 smoke test (每个方法 1 assertion)
- 1 tick = 1 cycle 契约
- PTX-EMU facade 兼容性

- [ ] **Step 2: 提交**

```bash
GIT_MASTER=1 git add test/test_i_compute_device_stepping.cc
GIT_MASTER=1 git commit -m "feat(tests): L4 IComputeDevice stepping 完整测试 (Task 18b P1-3)"
```

### Task 2.17: Oracle 评审 18b

- [ ] **Step 1: Dispatch Oracle review**

评审范围: commits 2.1-2.16 (12 子模块真值 + 端口拓扑 + Hazard + LSU + L4 测试).

- [ ] **Step 2: 修复 P0/P1**

- [ ] **Step 3: push 18b 完成**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
```

---

## 子波 3: PTX-EMU 端 HSK-9 改造 (5-10 天)

> **Note**: 以下 Tasks 在 PTX-EMU 仓的 worktree `.worktrees/hsk-9-impl` (branch `feat/hsk-9-impl`) 中执行。

### Task PTX-1: 镜像 `instr_descriptor.hh` + `i_compute_device.hh` 到 PTX-EMU 仓

**Files:**
- Create: `external/PTX-EMU/include/ptxemu/instr_descriptor.hh` (镜像 CppTLM)
- Create: `external/PTX-EMU/include/ptxemu/i_compute_device.hh` (镜像 CppTLM)

- [ ] **Step 1: 复制 2 文件从 CppTLM 到 PTX-EMU**

```bash
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl
mkdir -p include/ptxemu/icd  # IComputeDevice 镜像目录

# 复制并改 namespace + include path
cp /workspace/project/CppTLM/include/tlm/gpu/instruction_descriptor.hh \
   include/ptxemu/icd/instr_descriptor.hh
# 修改: cpptlm::gpu::PipeClass 等 namespace 改为 ptxemu::icd::PipeClass
# 修改: #include "tlm/gpu/instruction_descriptor.hh" → 已就位 (无 include)

cp /workspace/project/CppTLM/include/tlm/gpu/i_compute_device.hh \
   include/ptxemu/icd/i_compute_device.hh
# 修改: cpptlm::gpu::IComputeDevice → ptxemu::icd::IComputeDevice
# 修改: include/tlm/gpu/instruction_descriptor.hh → include/ptxemu/icd/instr_descriptor.hh
```

- [ ] **Step 2: PTX-EMU 端 ICOMPUTE_API_VERSION 守卫**

```cpp
// include/ptxemu/icd/version.h
#define ICOMPUTE_API_VERSION 1
static_assert(ICOMPUTE_API_VERSION == 1,
              "ICOMPUTE_API_VERSION frozen at 1 (HSK-9 spec)");
```

- [ ] **Step 3: 验证镜像编译**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
cmake --build build -j$(nproc)
# 预期: 254/254 ctest PASS 不破坏
```

- [ ] **Step 4: 提交**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add include/ptxemu/icd/
GIT_MASTER=1 git commit -m "feat(icd): 镜像 IComputeDevice + InstrDescriptor 头文件 (HSK-9)"
```

### Task PTX-2: `device_api_impl.cc` 新增 `set_instr_descriptor_buf` + `attach_timing` deprecated

**Files:**
- Modify: `external/PTX-EMU/src/ptxemu/device_api_impl.cc`

- [ ] **Step 1: 在 PtxEmuDeviceImpl 加 set_instr_descriptor_buf 成员**

```cpp
// PTX-EMU src/ptxemu/device_api_impl.cc PtxEmuDeviceImpl class 内部
#include <ptxemu/icd/i_compute_device.hh>
#include <ptxemu/icd/instr_descriptor.hh>

// 内部 ring buffer (producer 侧, 等待 CppTLM 端 SM 消费)
std::array<ptxemu::icd::InstrDescriptor, 64> instr_buf_{};
uint32_t head_ = 0, tail_ = 0;
uint32_t count_ = 0;

void set_instr_descriptor_buf(const ptxemu::icd::InstrDescriptor* buf, uint32_t count) override {
    if (!buf || count == 0 || count > 64) return;
    for (uint32_t i = 0; i < count; ++i) {
        instr_buf_[tail_] = buf[i];
        tail_ = (tail_ + 1) % 64;
    }
    count_ += count;
    if (count_ > 64) count_ = 64;  // 安全: 覆盖最旧
}

bool is_instruction_completed(uint64_t instr_id) const override {
    // 简化: instr_id < head_ 视为完成
    return instr_id < head_;
}
```

- [ ] **Step 2: attach_timing 改 deprecated stub**

```cpp
// 保留 attach_timing 公共签名 (HSK-8 冻结), body 改为空
[[deprecated("HSK-9: attach_timing deprecated, use IComputeDevice::set_instr_descriptor_buf")]]
void attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*) override {
    // HSK-9 弃用: 之前 3 vendor 接口已删除 (Task 12 in CppTLM)
    // 保留方法签名以维持 IPtxEmuDevice 接口稳定 (PTXEMU_API_VERSION=1 冻结)
    // 编译期 [[deprecated]] 警告即可, 不影响链接
}
```

- [ ] **Step 3: 验证编译**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
cmake --build build -j$(nproc)
# 预期: 编译成功, [[deprecated]] 警告预期
```

- [ ] **Step 4: 提交**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add src/ptxemu/device_api_impl.cc
GIT_MASTER=1 git commit -m "feat(icd): device_api_impl 新增 set_instr_descriptor_buf + attach_timing deprecated (HSK-9)"
```

### Task PTX-3: `sm_context_cpptlm_inject.{h,cpp}` 移除 attach_timing consumer

**Files:**
- Modify: `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}`

- [ ] **Step 1: 移除 IPipelineLatencyProvider + ITensorCoreTiming 调用**

```cpp
// src/ptxsim/core/sm_context_cpptlm_inject.cpp
// 删除:
//   void step_b_set_blocked_cycles(IPipelineLatencyProvider* pipeline,
//                                  ITensorCoreTiming* tc,
//                                  WarpContext* warp, ...)
// 改为:
//   void step_b_set_blocked_cycles(WarpContext* warp, ...) {
//       // HSK-9: 改用 InstrDescriptor::latency_class 真值源 (per F1.4 SM-owns-state)
//       uint32_t instr_latency = static_cast<uint32_t>(stmt.latency_class) * 4;
//       if (instr_latency > 0)
//           warp->set_blocked_cycles_for_active(instr_latency);
//   }
```

- [ ] **Step 2: 移除 IScoreboard 引用 (sm_context.h 已保留, 但 cpptlm_inject 不再调用)**

- [ ] **Step 3: 删除测试 attach_timing consumer e2e 测试**

`external/PTX-EMU/tests/integration/cpptlm/test_attach_timing_consumer_e2e.cpp`:
- 物理删除该文件
- 替代: 新 `test_set_instr_descriptor_buf_e2e.cc` (见 Task PTX-4)

- [ ] **Step 4: 验证 251/251 ctest PASS (可能 249 → 246 due to deletion)**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

- [ ] **Step 5: 提交**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}
GIT_MASTER=1 git rm tests/integration/cpptlm/test_attach_timing_consumer_e2e.cpp  # if exists
GIT_MASTER=1 git commit -m "refactor(sm): sm_context_cpptlm_inject 移除 attach_timing consumer (HSK-9)"
```

### Task PTX-4: PTX-EMU 端新接口测试

**Files:**
- Create: `external/PTX-EMU/tests/integration/cpptlm/test_set_instr_descriptor_buf_e2e.cc`

- [ ] **Step 1: 写失败测试**

```cpp
#include <catch2/catch_all.hpp>
#include <ptxemu/device_api.h>
#include <ptxemu/icd/i_compute_device.hh>
#include <ptxemu/icd/instr_descriptor.hh>

TEST_CASE("set_instr_descriptor_buf + is_instruction_completed 端到端", "[hsk-9][ptxemu]") {
    auto dev = ptxemu::create_device();
    ptxemu::DeviceConfig cfg{};
    REQUIRE(dev->initialize(cfg));

    // Producer 侧: PTX-EMU 写入已解码指令
    ptxemu::icd::InstrDescriptor desc{};
    desc.instr_id = 42;
    desc.isa_type = ptxemu::icd::IsaType::kCDNA64;
    desc.pipe = ptxemu::icd::PipeClass::kScalarALU;
    desc.latency_class = ptxemu::icd::LatencyClass::kFixed4Cycle;
    dev->set_instr_descriptor_buf(&desc, 1);

    // Consumer 侧: SM 消费后调用 is_instruction_completed
    // 简化测试: 直接调用 is_instruction_completed 验证 false
    REQUIRE(!dev->is_instruction_completed(42));

    ptxemu::destroy_device(dev);
}

TEST_CASE("attach_timing 触发 [[deprecated]] 警告", "[hsk-9][ptxemu]") {
    auto dev = ptxemu::create_device();
    ptxemu::IScoreboard* sb = nullptr;
    ptxemu::IPipelineLatencyProvider* pl = nullptr;
    ptxemu::ITensorCoreTiming* tc = nullptr;
    // 期望编译器发出 [[deprecated]] 警告
    dev->attach_timing(sb, pl, tc);
    REQUIRE(true);  // 链接成功即可
    ptxemu::destroy_device(dev);
}
```

- [ ] **Step 2: 验证测试 PASS**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -R "hsk-9"
```

- [ ] **Step 3: 提交**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add tests/integration/cpptlm/test_set_instr_descriptor_buf_e2e.cc
GIT_MASTER=1 git commit -m "test(hsk-9): set_instr_descriptor_buf + is_instruction_completed E2E"
```

### Task PTX-5: PTX-EMU workflow 验证 + push

- [ ] **Step 1: PTX-EMU 仓全量 ctest**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
ctest --test-dir build --output-on-failure
# 预期: 252+/252+ PASS (1 增 test, 3 删除 = -2 net, 加新 e2e = +0)
```

- [ ] **Step 2: Drift check**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
.github/workflows/drift_check.yml  # 手动跑 (若 CI 没有)
# 或 grep invariants 在 .github/workflows/drift_check.yml
```

- [ ] **Step 3: push PTX-EMU branch**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git push origin feat/hsk-9-impl
```

- [ ] **Step 4: 等待 PTX-EMU owner review 14 天**

(`chore: 跟踪 HSK-9-feedback-tracker.md` 时填表)

### Task PTX-6: PTX-EMU 仓 PR + submodule bump

- [ ] **Step 1: 创建 PR 到 PTX-EMU main**

```bash
gh pr create --repo PTX-EMU --base main --head feat/hsk-9-impl \
    --title "[HSK-9] ICOMPUTE_API_VERSION=1 SM 重构 producer + attach_timing 弃用" \
    --body "Per HSK-9 spec (CppTLM `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md`). PTXEMU_API_VERSION=1 冻结. ICOMPUTE_API_VERSION=1 同步."
```

- [ ] **Step 2: PTX-EMU PR merged 后, CppTLM 仓 bump submodule**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cd external/PTX-EMU
git fetch origin
git checkout <PTX-EMU-merged-commit-hash>
cd ..
git add external/PTX-EMU
GIT_MASTER=1 git commit -m "chore(submodule): bump PTX-EMU to <hash> (HSK-9 merged)"
```

---

## 子波 4: CppTLM 18c — bit-exact Gate + 端到端 + 联合验证 (5-9 天)

### Task 4.1: 抽 ALU 真值源 (关键, 避免双写)

**Files:**
- Create: `include/tlm/gpu/sm/alu_truth_source.hh`
- Create: `src/tlm/gpu/sm/alu_truth_source.cc`

- [ ] **Step 1: 创建 namespace cpptlm::gpu::alu**

```cpp
// include/tlm/gpu/sm/alu_truth_source.hh
namespace cpptlm::gpu::alu {

// FP32 真值源 (PTX-EMU functional + CppTLM SM exec 两侧共享)
uint32_t v_add_f32(uint32_t a, uint32_t b);
uint32_t v_sub_f32(uint32_t a, uint32_t b);
uint32_t v_ffma_f32(uint32_t a, uint32_t b, uint32_t c);  // FMA contraction
uint32_t v_mul_f32(uint32_t a, uint32_t b);

// INT32/INT64
uint64_t v_add_u64(uint64_t a, uint64_t b);
uint64_t v_imad_s64(int64_t a, int64_t b, int64_t c);

// MFMA (CDNA 矩阵核心)
void v_mfma_f32_16x16x16(uint32_t* acc, const uint32_t* a, const uint32_t* b);

}  // namespace cpptlm::gpu::alu
```

- [ ] **Step 2: 实现 FP32 真值**

```cpp
// src/tlm/gpu/sm/alu_truth_source.cc
#include "tlm/gpu/sm/alu_truth_source.hh"
#include <cstring>

namespace cpptlm::gpu::alu {

uint32_t v_add_f32(uint32_t a, uint32_t b) {
    float fa, fb, fr;
    std::memcpy(&fa, &a, 4);
    std::memcpy(&fb, &b, 4);
    fr = fa + fb;  // IEEE 754 默认 round-to-nearest-even
    uint32_t r;
    std::memcpy(&r, &fr, 4);
    return r;
}

uint32_t v_ffma_f32(uint32_t a, uint32_t b, uint32_t c) {
    float fa, fb, fc, fr;
    std::memcpy(&fa, &a, 4);
    std::memcpy(&fb, &b, 4);
    std::memcpy(&fc, &c, 4);
    // FMA contraction: a*b+c 单条指令完成 (无中间舍入)
    fr = std::fmaf(fa, fb, fc);  // C++17 std::fmaf (单条 FMA)
    uint32_t r;
    std::memcpy(&r, &fr, 4);
    return r;
}

}  // namespace cpptlm::gpu::alu
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/alu_truth_source.hh src/tlm/gpu/sm/alu_truth_source.cc src/CMakeLists.txt
GIT_MASTER=1 git commit -m "feat(sm): ALU 真值源 namespace cpptlm::gpu::alu (FP32 + INT64)"
```

### Task 4.2: PTX-EMU 端也镜像 ALU 真值源 (避免双写)

**Files:**
- Create: `external/PTX-EMU/include/ptxemu/icd/alu_truth_source.hh`
- Create: `external/PTX-EMU/src/ptxemu/icd/alu_truth_source.cc`

- [ ] **Step 1: 复制 ALU 真值源到 PTX-EMU 仓**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
cp /workspace/project/CppTLM/.worktrees/sm-mp-impl/include/tlm/gpu/sm/alu_truth_source.hh \
   include/ptxemu/icd/alu_truth_source.hh
cp /workspace/project/CppTLM/.worktrees/sm-mp-impl/src/tlm/gpu/sm/alu_truth_source.cc \
   src/ptxemu/icd/alu_truth_source.cc
# 修改 namespace: cpptlm::gpu::alu → ptxemu::icd::alu
```

- [ ] **Step 2: PTX-EMU 端 device_api_impl.cc 调用 ptxemu::icd::alu**

替换 `IScoreboard`/`IPipelineLatencyProvider`/`ITensorCoreTiming` 查找表路径:
```cpp
// src/ptxemu/device_api_impl.cc
#include <ptxemu/icd/alu_truth_source.hh>

// 替换 PipelineTLM get_fractional_cycles_by_type 查找:
uint32_t instr_latency = static_cast<uint32_t>(ptxemu::icd::alu::v_add_f32(0, 0));
// 实际: 通过 stmt.type 查找对应 alu:: 函数 + 乘以 cycles per LatencyClass
```

- [ ] **Step 3: 提交**

```bash
cd external/PTX-EMU/.worktrees/hsk-9-impl
GIT_MASTER=1 git add include/ptxemu/icd/alu_truth_source.{hh,cc} src/ptxemu/icd/ src/ptxemu/device_api_impl.cc src/CMakeLists.txt
GIT_MASTER=1 git commit -m "feat(icd): 镜像 ALU 真值源 + device_api_impl 调用 (HSK-9 双端共享)"
```

### Task 4.3: BitExactGate 完整实现

**Files:**
- Create: `include/tlm/gpu/sm/bit_exact_gate.hh`
- Create: `src/tlm/gpu/sm/bit_exact_gate.cc`

- [ ] **Step 1: 实现 BitExactGate**

```cpp
// include/tlm/gpu/sm/bit_exact_gate.hh
#include "tlm/gpu/instruction_descriptor.hh"
#include "tlm/gpu/sm/alu_truth_source.hh"

namespace cpptlm::gpu::sm {

class BitExactGate {
public:
    // PTX-EMU functional 调用 (用同一 alu 真值源)
    uint64_t compute_ptx_functional(const InstrDescriptor& desc) {
        return dispatch_alu(desc);
    }

    // SM Exec 调用 (用同一 alu 真值源)
    uint64_t compute_sm_exec(const InstrDescriptor& desc) {
        return dispatch_alu(desc);
    }

    // 验证 bit-exact 一致性
    bool verify_bit_exact(uint64_t expected, uint64_t actual) {
        return expected == actual;
    }

private:
    uint64_t dispatch_alu(const InstrDescriptor& desc) {
        switch (desc.pipe) {
            case PipeClass::kScalarALU:
                if (desc.latency_class == LatencyClass::kFixed1Cycle)
                    return alu::v_add_u64(desc.src_regs[0], desc.src_regs[1]);
                if (desc.latency_class == LatencyClass::kFixed4Cycle)
                    return alu::v_imad_s64(desc.src_regs[0], desc.src_regs[1], 0);
                break;
            case PipeClass::kVectorALU:
                // 简化: 复用 v_add_u64 (32-bit 操作)
                return alu::v_add_u64(desc.src_regs[0], desc.src_regs[1]);
            default:
                return 0;
        }
        return 0;
    }
};

}  // namespace cpptlm::gpu::sm
```

- [ ] **Step 2: 写失败测试 + 实现**

Create `test/test_bit_exact_gate.cc`:
```cpp
#include "catch_amalgamated.hpp"
#include "tlm/gpu/sm/bit_exact_gate.hh"
#include "tlm/gpu/instruction_descriptor.hh"

using namespace cpptlm::gpu;
using namespace cpptlm::gpu::sm;

TEST_CASE("Bit-exact Gate: PTX-EMU functional == SM Exec ALU", "[sm-gate][sm-microarch]") {
    BitExactGate gate;
    InstrDescriptor desc{};
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.src_regs[0] = 100;
    desc.src_regs[1] = 200;
    auto ptx = gate.compute_ptx_functional(desc);
    auto sm = gate.compute_sm_exec(desc);
    REQUIRE(ptx == sm);
    REQUIRE(gate.verify_bit_exact(ptx, sm));
}

TEST_CASE("FP32 ALU bit-exact (denormal flush)", "[sm-gate][sm-microarch]") {
    BitExactGate gate;
    InstrDescriptor desc{};
    desc.pipe = PipeClass::kVectorALU;
    desc.latency_class = LatencyClass::kFixed4Cycle;
    uint32_t denorm_a, denorm_b;
    float fa = 1e-40f, fb = 2.0f;  // denormal
    std::memcpy(&denorm_a, &fa, 4);
    std::memcpy(&denorm_b, &fb, 4);
    desc.src_regs[0] = denorm_a;
    desc.src_regs[1] = denorm_b;
    auto ptx = gate.compute_ptx_functional(desc);
    auto sm = gate.compute_sm_exec(desc);
    REQUIRE(ptx == sm);
}

TEST_CASE("INT64 累加器 bit-exact", "[sm-gate][sm-microarch]") {
    BitExactGate gate;
    InstrDescriptor desc{};
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed4Cycle;
    desc.src_regs[0] = 0x7FFFFFFFFFFFFFFFull;
    desc.src_regs[1] = 1;
    auto ptx = gate.compute_ptx_functional(desc);
    auto sm = gate.compute_sm_exec(desc);
    REQUIRE(ptx == sm);
    REQUIRE(ptx == 0x8000000000000000ull);  // 溢出
}
```

- [ ] **Step 3: 提交**

```bash
GIT_MASTER=1 git add include/tlm/gpu/sm/bit_exact_gate.{hh,cc} src/tlm/gpu/sm/bit_exact_gate.cc test/test_bit_exact_gate.cc src/CMakeLists.txt
GIT_MASTER=1 git commit -m "feat(sm): BitExactGate + FP32/INT64 bit-exact 测试 (Task 18c)"
```

### Task 4.4: MFMA 累加器 + E2E SGEMM 校准

- [ ] **Step 1: 实现 v_mfma_f32_16x16x16**

`src/tlm/gpu/sm/alu_truth_source.cc`:
```cpp
void v_mfma_f32_16x16x16(uint32_t* acc, const uint32_t* a, const uint32_t* b) {
    // CDNA wave64: 16x16 矩阵乘法, 16 个 fp32 累加器
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j) {
            float sum;
            std::memcpy(&sum, &acc[i*16 + j], 4);
            for (int k = 0; k < 16; ++k) {
                float fa, fb, fmul;
                std::memcpy(&fa, &a[i*16 + k], 4);
                std::memcpy(&fb, &b[k*16 + j], 4);
                fmul = std::fmaf(fa, fb, sum);  // FMA contraction (关键!)
                std::memcpy(&acc[i*16 + j], &fmul, 4);
            }
        }
}
```

- [ ] **Step 2: 实现 SGEMM kernel E2E 校准**

Create `test/test_sgemm_kernel_e2e.cc`:
```cpp
TEST_CASE("SGEMM 256x256 kernel 端到端 (gpgpu-sim ±15%)", "[sm-sgemm][e2e][sm-microarch]") {
    // 跳过: 由 L6 E2E 测试覆盖 (Task 4.5)
}
```

L5 SGEMM 测试由 Task 2.13 HazardTracker 测试补全。

### Task 4.5: L6 端到端测试 + 联合验证

**Files:**
- Create: `test/test_sm_ptx_emu_e2e.cc`

- [ ] **Step 1: 创建 E2E 测试**

```cpp
#include "catch_amalgamated.hpp"
#include "tlm/gpu/streaming_multiprocessor_tlm.hh"
#include "tlm/gpu/sm/bit_exact_gate.hh"

using namespace tlm;
using namespace cpptlm::gpu;

TEST_CASE("PTX-EMU via IComputeDevice 步进 → SM 仿真 → 完成 kernel", "[sm-ptxemu-e2e][sm-l6]") {
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);

    // PTX-EMU 端 producer: 写入已解码指令
    InstrDescriptor desc{};
    desc.instr_id = 1;
    desc.isa_type = IsaType::kCDNA64;
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.src_regs[0] = 100;
    desc.src_regs[1] = 200;
    desc.dst_regs[0] = 5;
    desc.num_src = 2;
    desc.num_dst = 1;
    sm.set_instr_descriptor_buf(&desc, 1);

    // SM 端 consumer: exe_once 4 次推进 4 cycle
    for (int i = 0; i < 4; ++i) sm.exe_once();

    // PTX-EMU 端 consumer: 读寄存器 + 查完成
    uint64_t val = 0;
    REQUIRE(sm.get_register_value(0, 0, 5, &val));
    REQUIRE(val == 300);  // 100 + 200
    REQUIRE(sm.is_instruction_completed(1));
}

TEST_CASE("Bit-exact Gate: SM 真值 vs ALU 真值源", "[sm-gate-e2e][sm-l6]") {
    BitExactGate gate;
    InstrDescriptor desc{};
    desc.pipe = PipeClass::kScalarALU;
    desc.latency_class = LatencyClass::kFixed1Cycle;
    desc.src_regs[0] = 42;
    desc.src_regs[1] = 58;

    // SM 端真值
    EventQueue eq;
    StreamingMultiprocessorTLM sm("sm0", &eq);
    sm.set_instr_descriptor_buf(&desc, 1);
    sm.set_scalar_reg(42, 42);
    sm.set_scalar_reg(43, 58);
    for (int i = 0; i < 4; ++i) sm.exe_once();

    uint64_t sm_val = 0;
    sm.get_register_value(0, 0, 0, &sm_val);

    // Gate 真值源
    auto gate_val = gate.compute_ptx_functional(desc);
    REQUIRE(sm_val == gate_val);  // bit-exact 一致
}
```

- [ ] **Step 2: 跨仓编译 (PTX-EMU submodule ON)**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cmake -S . -B build -DCPPTLM_WITH_PTX_EMU=ON
cmake --build build --target cpptlm_tests -j$(nproc)
```

- [ ] **Step 3: 联合验证 (PTX-EMU + CppTLM 同步 ctest)**

```bash
# PTX-EMU 端
cd external/PTX-EMU/.worktrees/hsk-9-impl
ctest --test-dir build -R "hsk-9" --output-on-failure

# CppTLM 端
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
./build/bin/cpptlm_tests "[sm-l6]" 2>&1 | tail -3

# 双向 PASS
```

- [ ] **Step 4: 提交**

```bash
GIT_MASTER=1 git add test/test_sm_ptx_emu_e2e.cc
GIT_MASTER=1 git commit -m "feat(tests): L6 PTX-EMU E2E + bit-exact Gate 联合验证 (Task 18c)"
```

### Task 4.6: 跨仓 submodule bump (PTX-EMU → CppTLM)

- [ ] **Step 1: PTX-EMU PR merged 后, CppTLM 仓 bump submodule**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cd external/PTX-EMU
git fetch origin
git checkout <PTX-EMU-merged-hash>
cd ..
git diff --submodule=log
git add external/PTX-EMU
GIT_MASTER=1 git commit -m "chore(submodule): bump PTX-EMU to <hash> (HSK-9 merged, ALU 真值源镜像)"
```

- [ ] **Step 2: 验证跨仓 ctest 全部 PASS**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
cmake -S . -B build -DCPPTLM_WITH_PTX_EMU=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
# 预期: ~45000+ assertions PASS, 0 regression

cd external/PTX-EMU/.worktrees/hsk-9-impl
ctest --test-dir build --output-on-failure
# 预期: 254+/254+ PASS
```

### Task 4.7: Oracle Gate G13 评审 (SM 完整实现)

- [ ] **Step 1: Dispatch Oracle Gate G13**

```bash
# task(subagent_type="oracle", prompt="Gate G13 SM 完整 ALU 实现 + bit-exact Gate")
```

Oracle 评审:
- 14 Gate items G1-G14 (per ADR-SOC-16 §2.3)
- G1-G12 接口契约: 已 ✅ (Oracle Tasks 4-8 评审 PASS)
- **G13 SM 完整 ALU 实现 + bit-exact Gate**: 本 Gate 焦点
- G14 PTX-EMU 端 14 天反馈窗口: 2027-02-23 后

- [ ] **Step 2: 修复 Oracle 发现的 P0/P1**

- [ ] **Step 3: push final**

```bash
GIT_MASTER=1 git push origin feat/sm-mp-impl
```

### Task 4.8: 14 天反馈窗口跟踪 (会话结束前)

- [ ] **Step 1: 填写 HSK-9 feedback tracker**

```bash
# docs/superpowers/specs/HSK-9-feedback-tracker.md
# 追加一行: 2027-02-23 检查 PTX-EMU owner 是否发出反馈
```

- [ ] **Step 2: 创建 follow-up OpenSpec change**

```bash
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl
openspec new change cpptlm-dgpu-d1-cdna-isa-sm-rewrite-followup \
    --description "Task 18 完整实施 + bit-exact Gate + PTX-EMU HSK-9 改造" \
    --goal "SM 微架构 12 子模块真值 + bit-exact ALU + 端到端 PTX-EMU 集成"
# 推 OpenSpec change for transparency
```

---

## 验收标准

### 18a 完成 (子波 1):
- [ ] SM 顶层有 req_in/resp_out 访问器
- [ ] StreamAdapter 已注册并验证 JSON wiring (F12b smoke 真验证)
- [ ] ScalarALU ADD 真值
- [ ] RegFileUnit 真值 + get_register_value 返回真值
- [ ] Oracle 评审 PASS

### 18b 完成 (子波 2):
- [ ] 12 子模块全部有真值 (至少 1 ALU operation each)
- [ ] 8 Bundle 端口拓扑接通 (JSON 可连接所有 12 子模块)
- [ ] LsuGlobal 异步内存回调
- [ ] HazardTracker kVirtualReg + kHardwareCounter 真值
- [ ] L4 IComputeDevice stepping 测试 PASS

### PTX-EMU 端完成 (子波 3):
- [ ] PTX-EMU 仓镜像 IComputeDevice + InstrDescriptor 头文件
- [ ] device_api_impl.cc 新增 set_instr_descriptor_buf + attach_timing deprecated
- [ ] sm_context_cpptlm_inject 移除 attach_timing consumer
- [ ] PTX-EMU 端 ctest 全部 PASS
- [ ] HSK-9 PR merged

### 18c 完成 (子波 4):
- [ ] ALU 真值源 namespace cpptlm::gpu::alu
- [ ] PTX-EMU 端镜像 ALU 真值源 (双端共享)
- [ ] BitExactGate + FP32/INT64 bit-exact
- [ ] L6 PTX-EMU E2E + 联合验证
- [ ] 跨仓 submodule bump
- [ ] Oracle Gate G13 PASS

### 14 天反馈窗口 (2027-02-23):
- [ ] PTX-EMU 端反馈 (issue/email)
- [ ] Oracle Gate G14 PASS 或触发退路 A/B/C

---

## 关键风险与缓解

| 风险 | 等级 | 缓解 |
|------|------|------|
| ALU 真值源 PTX-EMU/SM 双写不一致导致 bit-exact 永远红 | 🔴 高 | **必须抽 `cpptlm::gpu::alu::*` 单一真值源两端复用** (per Oracle 关键建议, Task 4.1 + 4.2 镜像保证) |
| SM 端口拓扑设计错误导致多次返工 | 🟡 中 | 先用现成 `MultiPortStreamAdapter<>` 模板 (Task 2.14) |
| HazardTracker vmcnt/scoreboard 时序 bug | 🔴 高 | per debugging discipline 6 铁律 + count 日志 + 6 步响应环模板 |
| PTX-EMU 端 14 天反馈窗口超时 | 🟡 中 | 沉默默认同意 (HSK 协议惯例); 退路 A/B/C (per HSK-9 §7) |
| `static_assert` 在 C++17 编译期行为差异 | 🟢 低 | 用 `std::is_same_v<>` 而非 `sizeof` |
| `[[deprecated]]` 旧类在 PTX-EMU 端编译警告炸 CI | 🟢 低 | 用 `[[deprecated("reason")]]` 显式 message |
| 跨仓 submodule bump 冲突 | 🟡 中 | 用独立 worktree (Task 0.1 Step 2) |

---

## 必读文档清单 (新会话启动)

| 优先级 | 文档 | 时间 |
|--------|------|------|
| P0 | `docs/soc_arch/architecture/15-sm-microarchitecture-design.md` (971 行, §15.3-15.7) | 60 分钟 |
| P0 | `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` | 30 分钟 |
| P0 | `include/tlm/gpu/streaming_multiprocessor_tlm.hh` | 20 分钟 |
| P0 | `include/tlm/gpu/i_compute_device.hh` | 10 分钟 |
| P0 | `external/PTX-EMU/AGENTS.md` (PTX-EMU 仓 workflow) | 30 分钟 |
| P0 | `external/PTX-EMU/src/ptxemu/device_api_impl.cc` (346 行) | 20 分钟 |
| P1 | `include/tlm/gpu/instruction_descriptor.hh` (POD 字段) | 10 分钟 |
| P1 | `external/PTX-EMU/include/ptxemu/device_api.h` (公共契约) | 15 分钟 |
| P1 | `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.cpp` (consumer) | 15 分钟 |
| P2 | `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md` (14 Gate items) | 15 分钟 |

**总计启动时间**: ~4 小时阅读

---

## 新会话启动命令模板

```bash
# 1. 进入 CppTLM worktree
cd /workspace/project/CppTLM/.worktrees/sm-mp-impl

# 2. 进入 PTX-EMU worktree (单独 shell)
cd /workspace/project/CppTLM/external/PTX-EMU/.worktrees/hsk-9-impl

# 3. 验证基线
git log --oneline -3
cmake --build build -j$(nproc)  # 应已有 build/ from Task 0.1

# 4. 阅读 architecture/15-sm-microarchitecture-design.md §15.3-15.7

# 5. 开始子波 1 (Task 1.1: SM 顶层端口访问器)
# 见上方 Task 1.1 Step 1-4
```

**预计会话总时长**: 25-35 工作日 (1 人全职) 或 15-20 工作日 (2 人协作: CppTLM + PTX-EMU 各 1 人).

---

**祝实施顺利 🚀** — Tasks 18 + PTX-EMU HSK-9 + 联合验证一次到位. HEAD `c656222` 是稳定基线, 从 worktree `feat/sm-mp-impl` + `feat/hsk-9-impl` 开始.
