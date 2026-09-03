# HSK-9 协调公告（草稿）— `ICOMPUTE_API_VERSION=1` bump

> **状态**: 📋 Draft（阶段 A Gate 通过后才能正式发布）
> **发布触发**: 阶段 A `cpptlm-dgpu-d1-cdna-isa-phase-a` 全部任务完成 + Oracle Gate A 评审通过 + 全量回归 + bit-identical 验证
> **发布渠道**: CppTLM-PTX-EMU HSK 协议链（per `external/PTX-EMU/AGENTS.md` L21-23）
> **影响范围**:
> - **CppTLM 端**：`PTXEMU_API_VERSION=1` 冻结 → 新增 `ICOMPUTE_API_VERSION=1` 协议层
> - **PTX-EMU 端**：`IPtxEmuDevice` 12 方法逐步迁移至 `IComputeDevice` 抽象（向后兼容）
> - **跨仓契约变更**：`include/cudart/compute_device_interface.h`（CppTLM vendored）成为新契约头文件
> **关联文档**:
> - [`docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md`](../../../../docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md) §3 D3 R3（HSK 协调纪律）
> - [`openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/proposal.md`](../../../../openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/proposal.md) §Impact
> - [`external/PTX-EMU/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md`](../../../../external/PTX-EMU/docs/superpowers/specs/2026-08-18-hsk-6-cpptlm-bridge-deprecation.md)（HSK-6 桥接关系废止）
> **关联 HSK 链**:
> - HSK-1..8 ACCEPTED（已交付）
> - **HSK-9（本公告）**: `ICOMPUTE_API_VERSION=1` 协调
> - HSK-10（未来）: `IMemoryPort` 协议发布（阶段 B 触发）

---

## 公告正文（待阶段 A Gate 后发送）

### HSK-9 / 2027-02-XX: `ICOMPUTE_API_VERSION=1` 协议层建立

**摘要**：CppTLM 与 PTX-EMU 跨仓契约从 PTX 特化的 `IPtxEmuDevice` 升级到 ISA-agnostic 的 `IComputeDevice`。本公告邀请 PTX-EMU 端评审 + 协调 CppTLM 端 `cpptlm-dgpu-d1-cdna-isa-phase-a` change 的接口定义。

#### 1. 升级动机

C++ dGPU SoC v1.0 周期精确仿真框架按 ADR-SOC-15 路线图进入 **阶段 A：契约中立化**。当前 `IPtxEmuDevice` 接口（12 方法）带有以下 PTX 特化：
- `set_scoreboard(IScoreboard*)`：PTX 虚拟寄存器 hazard 模型
- `attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)`：3 vendored timing 接口紧耦合
- 字符串查表：`PipelineTLM::get_fractional_cycles(string, pipe)`（per Oracle 范式分析）

升级到 `IComputeDevice` 后：
- Hazard 抽象化为 `IHazardTracker`（双模：`kVirtualReg` 兼容 PTX / `kHardwareCounter` 预埋 CDNA）
- Timing 抽象化为 `InstrDescriptor::latency_class` 查表 + 单一 timing 接口
- 描述符化为 `InstrDescriptor` POD（CppTLM 不解析文本）

**目标**：在不破坏现有 PTX 模式（HSK-8 ACCEPTED 的 12 方法保持可用）的前提下，引入 ISA-agnostic 抽象层，支持阶段 B/C 的 CDNA 引擎接入。

#### 2. 接口变更总览

| 现有接口 | 新接口 | 兼容性 |
|---|---|---|
| `IPtxEmuDevice::set_scoreboard(IScoreboard*)` | **保留** + 新增 `IComputeDevice::set_scoreboard_v2(IHazardTracker*)` | 双轨并存 |
| `IPtxEmuDevice::attach_timing(IScoreboard*, IPipelineLatencyProvider*, ITensorCoreTiming*)` | **保留** + 新增 `IComputeDevice::attach_timing_v2(const InstrDescriptor&)` | 双轨并存 |
| `IPipelineLatencyProvider::get_fractional_cycles(string, pipe)` | **保留** + 新增 `get_latency(LatencyClass)` | 双轨并存 |
| `IScoreboard` (vendored, 0 依赖) | **保留** + 新增 `IHazardTracker` 抽象 | 双轨并存 |
| `InstrDescriptor` (CppTLM 端 POD) | **新增** | 新增 |
| `LatencyClass` / `PipeClass` / `CtrlBits` enums | **新增** | 新增 |

**关键不变量**：
- ✅ PTX-EMU 端无任何代码变更（阶段 A 仅改造 CppTLM 端）
- ✅ HSK-8 ACCEPTED 的 12 个 `IPtxEmuDevice` 方法全部保持可用
- ✅ 现有 `[pcie]/[axi]/[e2e]/[wave2]` 测试不变（PTX 模式 bit-identical）

#### 3. 跨仓契约细节

**CppTLM 端新增头文件**（待阶段 A 实施时创建）：

```cpp
// include/tlm/gpu/instruction_descriptor.hh
namespace cpptlm::gpu {
enum class PipeClass : uint8_t { /* 7 个值 */ };
enum class LatencyClass : uint8_t { /* 6 个值 */ };
struct CtrlBits { /* 6 字段 */ };
struct InstrDescriptor {
    PipeClass pipe; LatencyClass latency_class; CtrlBits ctrl;
    uint16_t dst_regs[4]; uint16_t src_regs[4];
    uint8_t num_dst; uint8_t num_src;
    bool is_memory; uint64_t target_vaddr; uint32_t mem_size;
    // 预留字段
    uint8_t reserved[8];
};
static_assert(sizeof(InstrDescriptor) <= 64);
} // namespace cpptlm::gpu

// include/tlm/gpu/hazard_tracker_interface.hh
class IHazardTracker {
public:
    virtual ~IHazardTracker() = default;
    virtual void tick() = 0;
    virtual bool is_stalled(uint32_t sm_id, uint32_t wave_id) const = 0;
    virtual void notify_instruction_issued(const InstrDescriptor& instr, uint32_t sm_id, uint32_t wave_id) = 0;
    virtual void notify_instruction_completed(const InstrDescriptor& instr, uint32_t sm_id, uint32_t wave_id) = 0;
    virtual void reset() = 0;
};
```

**PTX-EMU 端可选修改**（待协调）：
- `IPtxEmuDevice` 升级为继承 `IComputeDevice`（推荐；不强制）
- 新增 `IPtxEmuDevice::set_scoreboard_v2(IHazardTracker*)` 转发到 `IComputeDevice`
- `SMContext::exe_once()` 调用 `IHazardTracker::notify_instruction_issued`（推荐；可选集成）

#### 4. 协调时间表

| 日期 | 里程碑 |
|------|--------|
| 2027-02-XX | **本公告发布**（HSK-9 启动） |
| 2027-02-XX + 7d | PTX-EMU 端反馈窗口（review 接口 + 提建议） |
| 2027-02-XX + 14d | CppTLM 端 `cpptlm-dgpu-d1-cdna-isa-phase-a` 实施完成 + Oracle 评审通过 |
| 2027-02-XX + 21d | `ICOMPUTE_API_VERSION=1` 冻结（跨仓契约不可增量修改） |
| 2027-02-XX + 21d | 阶段 B 启动：`cpptlm-dgpu-d1-cdna-isa-phase-b`（IMemoryPort 引入） |

#### 5. 风险与缓解

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R3.1** | PTX-EMU 端拒绝 `IComputeDevice` 抽象（坚持 `IPtxEmuDevice` 命名） | 🟡 中 | 阶段 A 实施时 `IPtxEmuDevice` 仍可作为 `IComputeDevice` 的具体实现，不破坏 PTX-EMU 端 |
| **R3.2** | `InstrDescriptor` 大小超出 PTX-EMU 期望 | 🟢 低 | 当前 39 bytes + reserved 8 bytes = 47 bytes，远小于 PTX packet 上限 |
| **R3.3** | `IHazardTracker::notify_instruction_issued` 调用频度导致 PTX-EMU SMContext 性能下降 | 🟢 低 | PTX-EMU 端可选集成；不强制 |
| **R3.4** | 跨仓版本错位（CppTLM 升级到 `ICOMPUTE_API_VERSION=1` 时 PTX-EMU 仍 `PTXEMU_API_VERSION=1`） | 🟡 中 | 阶段 A 实施时双轨并存；阶段 C 切换前必须 PTX-EMU 端确认 |

#### 6. 兼容性测试要求

- ✅ CppTLM `[pcie]` 测试保持基线 ~15000 assertions 全绿
- ✅ CppTLM `[axi]` 测试保持基线 ~500 assertions 全绿
- ✅ CppTLM `[e2e]` + `[wave2]` + `[gpu]` 测试保持全绿
- ✅ 新增 `[cdna-phase-a]` 测试（4 个新测试文件）
- ✅ PTX-EMU 仓 `PTX-7a` 7 Mock + `PTX-7b` 4 集成测试保持 210/210 PASS
- ✅ PTX-EMU 端注入点（`include/ptxsim/{scoreboard,pipeline,tensor_core}_interface.h` + `SMContext`）零修改（阶段 A 范围内）

#### 7. 退路方案

如果 HSK-9 在 14 天反馈窗口内未达成共识：
- **退路 A**：推迟阶段 A，仅在 CppTLM 仓内做 `InstrDescriptor` 抽象（不跨仓）
- **退路 B**：保留现有 `IPtxEmuDevice` 命名，新增 `IComputeDevice` 作为基类，CppTLM 端使用 `IPtxEmuDevice`（兼容旧）
- **退路 C**：阶段 A 范围缩减到仅 `InstrDescriptor` + `LatencyClass` 枚举，`IHazardTracker` 推迟到阶段 C

---

## 公告发送清单（待填）

| 接收方 | 渠道 | 状态 |
|--------|------|------|
| PTX-EMU maintainers | `external/PTX-EMU/docs/superpowers/specs/` + GitHub issue | 待发送 |
| CppTLM maintainers | `openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/` | 待阶段 A 实施 |
| Oracle 评审 | `task(subagent_type="oracle", ...)` | 待阶段 A Gate |
| rdd-hub (Hub-Spoke 联邦) | `gh issue create --repo rdd-hub ...` | 待阶段 A Gate |

---

## 状态跟踪

- **2027-02-09**: 📋 Draft 撰写。本公告作为阶段 A 实施的前提，发布时机与 `cpptlm-dgpu-d1-cdna-isa-phase-a` Gate 同步。
- **TBD**: 📋 正式发布（阶段 A Gate 通过后 + PTX-EMU 反馈窗口启动）