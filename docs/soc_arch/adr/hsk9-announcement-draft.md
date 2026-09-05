# HSK-9 协调公告 — `ICOMPUTE_API_VERSION=1` SM 重构版

> **状态**: 🆕 Active（待阶段 A Gate 通过后正式发布；本文件为 SM 重构版正文，覆盖原 commit `4105602` 草稿）
> **原状态**: 📋 Draft（per commit `4105602` 阶段 A 后启动）—— **本文 supersede 旧版**
> **发布触发**: SM 重构 §15.10 Gate 通过 + Oracle Round 3 评审 + PTX-EMU 端 14 天反馈窗口
> **发布渠道**: CppTLM-PTX-EMU HSK 协议链（per `external/PTX-EMU/AGENTS.md` L21-23）
> **影响范围**:
> - **CppTLM 端**：删除 6 个 GPU 算力侧模块 + 3 vendor 接口 + 14 测试；新增 SM 微架构 12 子模块 + 8 Bundle + `IComputeDevice`
> - **PTX-EMU 端**：`SMContext::exe_once()` 必须同步改造移除 `attach_timing()` 调用栈 + `device_api_impl.cc` 新增 `set_instr_descriptor_buf()` 实现
> - **跨仓契约变更**：1 个方法删除 (`attach_timing`) + 1 个方法新增 (`set_instr_descriptor_buf`)，净 12 → 12 方法不变
> - **版本号**：`PTXEMU_API_VERSION=1` 冻结（保持）+ `ICOMPUTE_API_VERSION=1` 保持（语义质变已显式标注为 breaking change，HSK 纪律要求所有方法签名变更必须 bump 版本号，本变更恰好不触发——净新增 1 + 净删除 1 = 0 方法数变化）
> **关联文档**:
> - [`docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`](../../../../docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md) §3 D3 R3（HSK 协调纪律）
> - [`docs/soc_arch/architecture/15-sm-microarchitecture-design.md`](../../../../docs/soc_arch/architecture/15-sm-microarchitecture-design.md) §15.6.3 HSK-9 必要性（用户 2027-02-09 决策）
> - [`openspec/changes/cpptlm-dgpu-d1-cdna-isa-sm-rewrite/`](../../../../openspec/changes/cpptlm-dgpu-d1-cdna-isa-sm-rewrite/)（新 SM rewrite change）
> - [`openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/`](../../../../openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/)（**superseded** by sm-rewrite）
> - [`external/PTX-EMU/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md`](../../../../external/PTX-EMU/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md)（HSK-6 桥接关系废止）
> **关联 HSK 链**:
> - HSK-1..8 ACCEPTED（已交付）
> - **HSK-9（本公告）**: `ICOMPUTE_API_VERSION=1` SM 重构版
> - HSK-10（未来）: IMemoryPort 协议发布（阶段 B 触发）

---

## 公告正文

### HSK-9 / 2027-02-XX: `ICOMPUTE_API_VERSION=1` SM 重构版

**摘要**：CppTLM 端 GPU 算力侧重构为完整 SM 微架构（12 ChStream 子模块 + 8 Bundle + `IComputeDevice` 14 方法），删除 PTX-EMU 端 `attach_timing()` 调用栈依赖。本公告邀请 PTX-EMU 端评审 + 协调 CppTLM 端 `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` change 的接口变更。

#### 1. 重构动机

C++ dGPU SoC v1.0 周期精确仿真框架按 ADR-SOC-15 路线图进入 SM 重构阶段。当前 `IPtxEmuDevice` 接口（12 方法）依赖 3 个 vendor 接口：
- `set_scoreboard(IScoreboard*)`（实际上 IS scoreboard mask setter，per `device_api.h:101`）
- `attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)`

这 3 vendor 接口导致：
1. `PipelineTLM` 字符串查表（`has(instr, "fma")` 等 6 类子串匹配）
3. `ScoreboardTLM` 虚拟寄存器 hazard 模型（与 CDNA 显式计数器模型不兼容）
4. `TensorCoreTLM` 单独 TC 抽象（与 SM MatrixCore 子模块模块功能重复）

升级到 SM 重构后：
- 字符串查表被 `LatencyClass` 枚举查表替代（per `InstrDescriptor` POD）
- 虚拟寄存器被 `IHazardTracker` 抽象（kVirtualReg 兼容 + kHardwareCounter CDNA 显式）
- TC 集成到 SM `MatrixCoreUnit` 子模块
- PTX-EMU 调用 `set_instr_descriptor_buf()` 注入已解码 `InstrDescriptor`；SM 通过 SM-owns-state 模式返回寄存器写值（per architecture/15 §15.5.6 同步协议）

**目标**：在不破坏现有 PTX-EMU 仓 public header (`IPtxEmuDevice` 12 方法签名冻结 + `PTXEMU_API_VERSION=1`) 的前提下，重构 CppTLM 端 GPU 算力侧为完整 SM 微架构，支持阶段 B/C 的 CDNA 引擎接入。

#### 2. 接口变更总览

| 现有方法 | 状态 | 备注 |
|----------|------|------|
| `initialize` / `shutdown` | ✅ 保留 | 生命周期 |
| `exe_once` / `sm_exe_once` / `warp_exe_once` | ✅ 保留（per `device_api.h:95-97`）| 单步推进 |
| `set_scoreboard(sm_id, warp_id, mask)` | ✅ 保留（per `device_api.h:101`，mask setter）| 现有接口签名不变 |
| `get_thread_state(sm_id, warp_id, lane_id) → ThreadState` | ✅ 保留（per `device_api.h:104`）| per-lane state |
| `set_active_mask(sm_id, warp_id, mask)` | ✅ 保留 | EXEC mask |
| `set_next_pc(sm_id, warp_id, lane_id, pc)` | ✅ 保留 | per-lane PC 控制 |
| `get_warp_status(sm_id, warp_id) → WarpStatus` | ✅ 保留 | per-warp status |
| `is_finished()` | ✅ 保留 | 全局 finished 状态 |
| `attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)` | ❌ **删除**（per `device_api.h:114`）| 3 vendor 接口已废 |
| `set_instr_descriptor_buf(InstrDescriptor*, uint32_t)` | 🆕 **新增** | producer 侧：PTX-EMU 写入已解码的 `InstrDescriptor[]`；consumer 侧：SM 接收推进 timing |
| `get_register_value(sm, warp, reg_id, out_value, lane_id)` | 🆕 **新增**（IComputeDevice 扩）| PTX-EMU functional simulator 读 SM 寄存器真值（分支判定/地址计算）|
| `is_instruction_completed(instr_id)` | 🆕 **新增**（IComputeDevice 扩）| PTX-EMU 等 SM `WritebackUnit` 写回 `RegFileUnit` 完成 |

**关键不变量**：
- ✅ `IPtxEmuDevice` 12 方法签名**冻结**（除 `attach_timing` 删除 + `set_instr_descriptor_buf` 新增；**净方法数仍为 12**——HSK-9 不触发 VERSION bump）
- ✅ 配套 `IComputeDevice` 是 **CppTLM 端新接口**（`include/tlm/gpu/i_compute_device.hh`），14 方法（独立于 `IPtxEmuDevice`，不修改 PTX-EMU 公共头）
- ✅ `PTXEMU_API_VERSION=1` 冻结
- ✅ `ICOMPUTE_API_VERSION=1` 冻结（语义质变已显式标注，**未触发** HSK-N bump）
- ✅ 现有 `[pcie]/[axi]/[gpu]` PTX 模式测试保持基线 100% 通过

#### 3. 跨仓契约细节

**CppTLM 端新增头文件**（待 SM 重构 commit 4-7 创建）：

```cpp
// include/tlm/gpu/i_compute_device.hh
namespace cpptlm::gpu {
class IComputeDevice {
public:
    virtual ~IComputeDevice() = default;
    virtual bool initialize(const DeviceConfig& cfg) = 0;
    virtual void shutdown() = 0;
    virtual void reset() = 0;
    virtual int  exe_once() = 0;
    virtual int  sm_exe_once(uint32_t sm_id) = 0;
    virtual int  warp_exe_once(uint32_t sm_id, uint32_t warp_id) = 0;
    virtual void set_instr_descriptor_buf(const InstrDescriptor* buf, uint32_t count) = 0;
    virtual bool is_wave_stalled(uint32_t sm_id, uint32_t wave_id) = 0;
    virtual bool is_finished() = 0;
    virtual uint64_t get_cycle_count() const = 0;
    virtual void set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask) = 0;
    virtual void set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc) = 0;
};
}
```

**PTX-EMU 端必须修改**（commit 20 同步推送）：

| 文件 | 修改内容 |
|------|----------|
| `external/PTX-EMU/src/ptxemu/device_api_impl.cc` | 新增 `set_instr_descriptor_buf()` 实现（producer 侧：从 PTX-EMU decode 后的 InstrDescriptor 写入 SM）；现有 `attach_timing()` 方法改为空 stub（保留以满足接口方法数要求？或直接删——见 §4 决策点）|
| `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.cpp` | 移除 `attach_timing()` consumer 路径；`sm_context->exe_once()` 改为调用 `SM::exe_once()`（per IComputeDevice 方向反转）|
| `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.h` | 移除 `IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming` 头文件依赖 |
| `external/PTX-EMU/tests/integration/cpptlm/test_attach_timing_consumer_e2e.cpp` 等 5 个依赖 attach_timing 的测试 | 重定位到 SM 重构版测试（per `test_sm_ptx_emu_e2e.cc`）|

#### 4. 协调时间表

| 日期 | 里程碑 |
|------|--------|
| 2027-02-XX | **本公告发布**（HSK-9 启动） |
| 2027-02-XX + 7d | PTX-EMU 端反馈窗口（review 接口 + 提建议） |
| 2027-02-XX + 14d | CppTLM 端 `cpptlm-dgpu-d1-cdna-isa-sm-rewrite` 实施完成（20 原子 commit）+ Oracle Round 3 评审通过 |
| 2027-02-XX + 21d | PTX-EMU 端 SMContext 改造完成（per §3 文件清单）|
| 2027-02-XX + 21d | **HSK-9 冻结**（双向契约不可增量修改）|
| 2027-02-XX + 21d | 阶段 B 启动：`cpptlm-dgpu-d1-cdna-isa-phase-b`（IMemoryPort 引入）|

#### 5. 风险与缓解

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R9.1** | PTX-EMU 端拒绝 `set_instr_descriptor_buf` 新增（坚持 `IPtxEmuDevice` 原 12 方法）| 🟡 中 | HSK 协议允许净方法数不变的 breaking change（语义质变但接口数稳定）；提供退路：`attach_timing` 改为空 stub 不删 |
| **R9.2** | `InstrDescriptor` POD 字段 PTX-EMU 端不兼容 | 🟢 低 | POD 已固定 47 bytes；Stage A 已有 spec；PTX-EMU 端只需构造 POD |
| **R9.3** | `set_instr_descriptor_buf` 调用频度导致 PTX-EMU 端性能下降 | 🟢 低 | 调用频度 = 1 per SM cycle；PTX-EMU 端仅 memcpy 47-byte POD |
| **R9.4** | 跨仓版本错位（CppTLM SM 重构完成时 PTX-EMU 端未改造）| 🟡 中 | 阶段 B 启动前必须 PTX-EMU 端同步确认；`openspec archive` 阻塞 |
| **R9.5** | SM-owns-state 模式破坏 PTX-EMU functional state 假设 | 🟢 低 | SM `RegFileUnit` 是唯一寄存器值真值源；PTX-EMU 通过 `set_instr_descriptor_buf` 上行字段拉取（per §15.5.6 同步协议）|
| **R9.6** | HSK-9 语义质变但版本不变违反 HSK 纪律 | 🟢 低 | 本公告显式标注 "语义质变 breaking change"；未来如有方法数变化（如阶段 B IMemoryPort 新增），触发 HSK-N bump |

#### 6. 兼容性测试要求

- ✅ CppTLM `[pcie]` 测试保持基线 ~15000 assertions 全绿
- ✅ CppTLM `[axi]` 测试保持基线 ~500 assertions 全绿
- ✅ CppTLM `[e2e]` + `[wave2]` + `[gpu]` 测试保持全绿（注：14 旧测试删除，需重新生成等价覆盖）
- ✅ 新增 `[sm-microarch]` 测试（12 个新测试文件 + L2-L7 集成测试）
- ✅ PTX-EMU 仓 `PTX-7a` 7 Mock + `PTX-7b` 4 集成测试保持 210/210 PASS（保留基线）
- ✅ PTX-EMU 仓 `tests/integration/cpptlm/test_attach_timing_consumer_e2e.cpp` 等 5 个 attach_timing 测试**重定位**到 `test_sm_ptx_emu_e2e.cc`
- ✅ PTX-EMU 仓 `src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}` 重构或删除

#### 7. 退路方案

如果 HSK-9 在 14 天反馈窗口内未达成共识：
- **退路 A**（最保守）：CppTLM 端**保留** 3 个 vendor 接口的兼容 shim（仅废弃 `KernelLaunchTLM` + `CudaCoreAdapterMVP` + `PtxEmuSubmoduleMVP` + 旧 4 个孤岛模块）；SM 重构推迟到 HSK-10
- **退路 B**（中间）：HSK-9 编号不变但语义分两批——第一批仅删 `attach_timing`，第二批新增 `set_instr_descriptor_buf`（2 个 HSK 周期）
- **退路 C**（最激进）：将 SM 重构改名为 `ICdnaComputeDevice`（新接口），与 `IPtxEmuDevice` 共存；PTX-EMU 端无须修改

主推方案仍是**大爆炸 + HSK-9**（per 用户决策 + Oracle Round 2 P0-2 拍板）。

---

## 公告发送清单（待填）

| 接收方 | 渠道 | 状态 |
|--------|------|------|
| PTX-EMU maintainers | `external/PTX-EMU/docs/superpowers/specs/` + GitHub issue | 待发送（HSK-9 启动后）|
| CppTLM maintainers | `openspec/changes/cpptlm-dgpu-d1-cdna-isa-sm-rewrite/` | 待 SM 重构实施 |
| Oracle 评审 | `task(subagent_type="oracle", ...)` | 待 SM 重构 Gate |
| rdd-hub (Hub-Spoke 联邦) | `gh issue create --repo rdd-hub ...` | 待 SM 重构 Gate |

---

## 与既有 HSK-9 草稿的关系（supersede 声明）

本文档**supersede** `docs/soc_arch/adr/hsk9-announcement-draft.md`（commit `4105602`）：

| 维度 | 旧草稿 (4105602) | 新版 (本文档) |
|------|-------------------|---------------|
| `ICOMPUTE_API_VERSION` | 1 (保持) | 1 (保持，语义质变已标注) |
| CppTLM 端实现 | 保留 3 vendor 接口 + Stage A 双轨 | 删 3 vendor 接口 + 完整 SM 重构 |
| PTX-EMU 端代码变更 | 零修改（HSK-8 兼容假设） | 必须修改（SMContext 移除 attach_timing 路径） |
| `IPtxEmuDevice` 方法 | 12 方法保持 + `set_instr_descriptor_buf` 新增 = 13 方法 | 12 方法保持（删 attach_timing + 新增 set_instr_descriptor_buf = 净 12 方法）|
| 触发阶段 | 阶段 A Gate 后 | SM 重构 Gate 后（阶段 A supersede by SM rewrite） |

**supersede 理由**：旧草稿基于"阶段 A 双轨并存"假设（PTX-EMU 与 CppTLM 双边均有 `PipelineTLM/ScoreboardTLM` 实现）；本设计反转该假设（SM 重构后 PTX-EMU 端必须同步改造）。HSK 协议允许同号公告被新版正文 supersede（per HSK-8 spec §Doc Hygiene）。

---

## 状态跟踪

- **2027-02-09**: 🆕 Active。本公告为 SM 重构版正文，supersede commit `4105602` 草稿；与设计文档 `architecture/15-sm-microarchitecture-design.md` §15.6.3 同步发布。
- **TBD**: 正式发布到 PTX-EMU 仓 docs/superpowers/specs/（commit 20 时同步）。