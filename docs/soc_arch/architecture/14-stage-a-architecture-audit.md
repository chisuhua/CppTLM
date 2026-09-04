# CDNA 演进 change 架构溯源：是否采用 CppTLM 模块搭建 + Bundle 连接？

> **审计日期**: 2027-02-09
> **审计对象**: `openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a`（阶段 A change）+ 设计文档 `architecture/11-cdna-real-isa-integration.md` / `architecture/13-cdna-emu-selector.md`
> **结论先给**:
> 1. **❌ 阶段 A change 不采用 CppTLM 模块搭建**（与现有 GPU compute 内核模式一致）
> 2. **❌ 模块间无 Bundle 连接定义**（与现有 GPU compute 内核模式一致）
> 3. **✅ 但这是有意为之的架构选择**，与项目既有 GPU 计算侧模式对齐；不是疏忽

---

## 1. 证据汇总

### 1.1 基类 + 注册模式

| 类 | 继承 | REGISTER_* | ModuleFactory 注册 |
|----|------|-----------|---------------------|
| `PipelineTLM` (现有) | `IPipelineLatencyProvider`（vendor 纯虚） | ❌ 无 | ❌ 不在 chstream_register.hh |
| `ScoreboardTLM` (现有) | `IScoreboard`（vendor 纯虚） | ❌ 无 | ❌ 不在 chstream_register.hh |
| `KernelLaunchTLM` (现有) | `ChStreamModuleBase` | ❌ 无 REGISTER_CHSTREAM | ❌ 不在 chstream_register.hh |
| `CdnaPipelineTLM` (阶段 A 规划) | `IPipelineLatencyProvider`（与 PipelineTLM 平行） | ❌ 无 | ❌ 计划不注册 |
| `ScoreboardTLMv2` (阶段 A 规划) | `IHazardTracker`（新接口） | ❌ 无 | ❌ 计划不注册 |

**实证**：
- `include/chstream_register.hh:47-56` REGISTER_CHSTREAM 宏只注册 8 个模块（CacheTLM/MemoryTLM/CrossbarTLM/CoherentXBarTLM/CPUTLM/TrafficGenTLM/ArbiterTLM/RouterTLM），**没有 GPU 算力侧任何模块**
- `include/modules_cluster.hh:36-44` REGISTER_MODULE 宏只注册 9 个 SimModule 派生（Cluster 系列），**没有 GPU 算力侧任何模块**
- `src/core/module_factory.cc` 中**无** PipelineTLM/ScoreboardTLM/KernelLaunchTLM 字样
- 测试 `test/test_apu_soc_from_config.cc:35` 才调用 `factory.instantiateAll(config)`；GPU 算力侧测试都用 `new PipelineTLM()` 直接构造

### 1.2 Bundle / Port 定义

**`include/tlm/gpu/` 目录下没有任何 bundle / port / stream 头文件**（grep `bundle|port|stream` 结果为空）。

**`PipelineTLM`/`ScoreboardTLM` 既无 `ChStreamPort` 也无 `ChStreamBundle`**（grep `ChStreamPort|ChStreamBundle|m_port|out_port|in_port` 命中 0 次）。

**`KernelLaunchTLM` 唯一相关代码**：`include/tlm/gpu/kernel_launch_tlm.hh:67` `set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override` —— 它**继承** `ChStreamModuleBase` 但**没有**自己的 port 字段，只是给基类注入一个 adapter。

### 1.3 阶段 A change 计划对比

阶段 A change `cpptlm-dgpu-d1-cdna-isa-phase-a` 计划新增 6 个文件：

| 文件 | 模式 | 是否走 ModuleFactory |
|------|------|---------------------|
| `include/tlm/gpu/instruction_descriptor.hh` | POD 头文件 | ❌ |
| `include/tlm/gpu/cdna_pipeline_tlm.hh` + `.cc` | `class CdnaPipelineTLM : public IPipelineLatencyProvider` | ❌ |
| `include/tlm/gpu/hazard_tracker_interface.hh` | 抽象接口 | ❌ |
| `include/tlm/gpu/scoreboard_tlm_v2.hh` + `.cc` | `class ScoreboardTLMv2 : public IHazardTracker` | ❌ |

**`tasks.md` 中无任何"ModuleFactory 注册"或"chstream 注册"任务**——设计层面就不打算走 CppTLM 模块路径。

---

## 2. 原因分析：为什么 GPU 算力侧不采用 CppTLM 模块模式

### 2.1 职责差异

**采用 CppTLM 模块模式的 8 个对象**（均在 `chstream_register.hh`）：
- 全部是**仿真边界端点**（Cache/Memory/Crossbar/CoherentXBar/CPU/Arbiter/Router/Link/PCIe）
- 持有**显式 ChStream 输入/输出端口**
- 在 JSON 拓扑中**独立**可见、单独配置
- 通过 `instantiateAll` + StreamAdapter 注入 + connection_resolver 接线

**未走 CppTLM 模块模式的 GPU 算力侧对象**：
- **全部是**仿真内部**协调者状态**（Pipeline latency provider / Hazard tracker / Kernel dispatcher）
- 没有 ChStream 端口（不收发 TLM 事务包）
- 在 JSON 拓扑中**不可见**（不出现在 `apu_soc_v1.json` 的 `modules` 列表）
- 由 `KernelLaunchTLM` 通过 **setter 注入**（`set_scoreboard` / `set_pipeline` / `set_tensor_core`）

### 2.2 架构层级

CppTLM 有 **双注册表**（per AGENTS.md §CONVENTIONS）：
- `SimObject`（object，单实例 Leaf）→ `REGISTER_CHSTREAM` 路径
- `SimModule`（module，参数化容器）→ `REGISTER_MODULE` 路径

GPU 算力侧（PipelineTLM / ScoreboardTLM / CdnaPipelineTLM / ScoreboardTLMv2 / IHazardTracker / InstrDescriptor）属于**第三种**：
- 既不是独立仿真端点（不是 SimObject）
- 也不是参数化容器（不是 SimModule）
- 而是**协调者注入对象**（Coordinator-injected Dependency），类似 Spring 的 `@Autowired` 但不需容器管理

HSK-8 ACCEPTED 的 `IPtxEmuDevice` + `IScoreboard` + `IPipelineLatencyProvider` + `ITensorCoreTiming` 4 个 vendor 纯虚接口，就是这层依赖的契约。

### 2.3 PTX-EMU HSK 协议锁定的形态

`external/PTX-EMU/include/ptxemu/device_api.h` 中 `IPtxEmuDevice` 接口规范就是**纯虚接口注入**模式，**不是** ChStream 模块模式：

```cpp
class IPtxEmuDevice {
public:
    virtual bool initialize(const DeviceConfig& cfg) = 0;
    virtual void attach_timing(IScoreboard* sb,
                               IPipelineLatencyProvider* pipe,
                               ITensorCoreTiming* tc) = 0;  // 三参数注入
    virtual int exe_once() = 0;  // 单步推进
    // ...
};
```

CppTLM 端通过 `CudaCoreAdapterMVP` 把 PTX-EMU 的 `IPtxEmuDevice` 包装为**协调者**（`KernelLaunchTLM` 持有），PTX-EMU 内部通过 `attach_timing` 回调**反向**获取 CppTLM 的 Pipeline/Scoreboard/TC 实现。这是**反向注入**模型，不适合 ChStream 端口对接。

### 2.4 阶段 A 决策 D2 / D3 的明确选择

`openspec/changes/cpptlm-dgpu-d1-cdna-isa-phase-a/design.md` §Decisions 明确：

> **D2. PipelineTLM 双轨实现：保留旧 + 新增 CdnaPipelineTLM**（方案 A 采纳）
> - 旧 `PipelineTLM` 保留 100% 原状
> - 新增 `CdnaPipelineTLM` 实现相同 `IPipelineLatencyProvider` 接口
> - `KernelLaunchTLM::tick()` 通过 `set_pipeline(...)` 选择注入哪一个

> **D3. HazardTracker 双模实现：单文件双枚举**（方案 A 采纳）
> - `IHazardTracker` 抽象接口
> - `ScoreboardTLMv2` 实现 `IHazardTracker`，构造函数接受 `enum class Mode`
> - `kVirtualReg` 模式内部直接复用 `ScoreboardTLM` 实现
> - `kHardwareCounter` 模式仅占位实现（保留计数器字段）

阶段 A 100% 沿用了 HSK-8 vendor 接口的纯虚注入模式，**不引入任何 ChStream 端口/bundle**。

---

## 3. 与项目其他模块对比

| 模式 | 例 | ChStream 端口 | Bundle | ModuleFactory 注册 | 拓扑 JSON 可见 |
|------|-----|---------------|--------|---------------------|-----------------|
| **SimObject (REGISTER_CHSTREAM)** | CacheTLM / MemoryTLM / CPUTLM | ✅ | ✅ | ✅ | ✅ |
| **SimModule (REGISTER_MODULE)** | CpuCluster / GpuCluster / ApuSoC | ✅ | ✅ | ✅ | ✅ |
| **Coordinator-injected Dependency (现状 GPU)** | PipelineTLM / ScoreboardTLM / KernelLaunchTLM | ❌ | ❌ | ❌ | ❌ |
| **Stage A 计划 (CDNA 中立化)** | CdnaPipelineTLM / ScoreboardTLMv2 / IHazardTracker | ❌ | ❌ | ❌ | ❌ |
| **Stage C 计划 (CDNA 引擎)** | CdnaEmuDevice : IComputeDevice | ❌ | ❌ | ❌ | ❌ |

**结论**：阶段 A change **完全沿用**第三种模式（Coordinator-injected Dependency），与现有 GPU 算力侧 (`PipelineTLM`/`ScoreboardTLM`/`KernelLaunchTLM`) **完全一致**。

---

## 4. 是否有遗漏的"应做但未做"项？

### 4.1 阶段 A 范围内：**没有**

阶段 A 的核心目标是**消除 PTX 字符串查表 + 虚拟寄存器硬编码**，建立 `InstrDescriptor` + `IHazardTracker` 抽象。这是**接口层重构**，不是新的仿真端点。

- `InstrDescriptor` 是 POD 描述符
- `IHazardTracker` 是纯虚接口
- `CdnaPipelineTLM` / `ScoreboardTLMv2` 是 vendor 接口的**实现**（与 `PipelineTLM` / `ScoreboardTLM` 同形态）

**没有任何功能需求要求它们暴露 ChStream 端口**。如果硬加，反而破坏"协调者注入"模式。

### 4.2 阶段 B/C/D 范围内：**值得考虑**

| 阶段 | 潜在 ChStream 化需求 | 建议 |
|------|----------------------|------|
| **阶段 B (IMemoryPort)** | `IMemoryPort::send_request` 涉及真实 CppTLM NoC/Cache 事务 | ⚠️ 值得评估——是否应作为 ChStream 端口暴露？需对比 PCIe EP 7 阶段既有 Cfg 注入模式（per `include/tlm/pcie/pcie_endpoint_ip.hh`） |
| **阶段 C (CDNA 引擎)** | `CdnaEmuDevice` 内部 SM/Wavefront 状态 | ⚠️ 内部状态不需要外部端口；外部集成走 `IComputeDevice` 纯虚接口 |
| **阶段 D (双轨校准)** | `ICalibrationSink` 输出 | ❌ 不需要——校准数据是 JSON 输出，不是 TLM 事务 |

**阶段 B 的 IMemoryPort 是唯一值得重新评估的**。建议阶段 B 启动时（`cpptlm-dgpu-d1-cdna-isa-phase-b` change）专门决策一次：
- **方案 B-A**（保守）：`IMemoryPort` 维持阶段 A 同样的"回调 + setter 注入"模式，与 CppTLM 内部 `MemoryBridge` 通过 set_response_callback 协调
- **方案 B-B**（ChStream 化）：`IMemoryPort` 暴露 ChStream 端口，与 CppTLM `CacheReqBundle` 双向接线，走 ModuleFactory + JSON 拓扑
- **方案 B-C**（混合）：保留 `IMemoryPort` 抽象接口，但 CppTLM 端实现 `GpuMemoryPortAdapter` 是 ChStream 模块；CDNA-EMU 仍然只看到抽象接口

**当前默认**：方案 B-A（与现有 GPU 算力侧保持一致）。

---

## 5. 风险与建议

### 5.1 风险

| # | 风险 | 等级 | 说明 |
|---|------|------|------|
| **R1** | 阶段 A 缺乏拓扑层可见性：CDNA-EMU 切换不能通过 JSON 拓扑配置 | 🟡 中 | 当前 `apu_soc_v1.json` 不含 `pipeline/scoreboard/hazard_tracker` 配置；CDNA 切换需改 C++ 代码。ADR-SOC-15 §4.4 D1 计划加 `"isa_type"` 字段——是配置层补丁，不是模块层重构 |
| **R2** | 阶段 A 缺乏模块热插拔：测试需重启进程才能切换 `CdnaPipelineTLM` / `PipelineTLM` | 🟢 低 | 当前模式是 `set_pipeline(...)` 注入，运行时切换是 setter 调用即可；不算硬性风险 |
| **R3** | 阶段 B IMemoryPort 走 ChStream 化可大幅提升 NoC 集成精度 | 🟢 低 | 阶段 B 启动时再评估；不影响阶段 A |

### 5.2 建议

1. **阶段 A 保持现状**：继续走"协调者注入"模式，与现有 GPU 算力侧一致。**这是正确的架构选择**。
2. **阶段 B 启动时专门评估 IMemoryPort 走 ChStream 化**：在 `cpptlm-dgpu-d1-cdna-isa-phase-b` 提案的 `design.md` 中加一个 Decision 节点，列举 B-A / B-B / B-C 三个方案并论证。
3. **不需要为此回退阶段 A**：阶段 A 的接口设计（`InstrDescriptor` + `IHazardTracker` + `IComputeDevice`）已经 ISA-agnostic，无论 ChStream 化与否，CppTLM 内部接口契约不变。
4. **ADR-SOC-15 不需要更新**：当前 ADR 仅给"双轨前端 + 统一 Timing 宿主"高层架构，未规定"是否走 ChStream 模式"。阶段 A 决策在 OpenSpec change 内部 `design.md` 即可。

---

## 6. 总结

> **用户的 2 个问题直接回答**:
> 1. **Q: change 的实现方式是采用 CppTLM 模块搭建吗？**
>    **A: ❌ 不是**。阶段 A change 走"协调者注入"模式（Coordinator-injected Dependency），与现有 `PipelineTLM` / `ScoreboardTLM` / `KernelLaunchTLM` 完全一致；未走 CppTLM `REGISTER_CHSTREAM` / `REGISTER_MODULE` 模块路径。
> 2. **Q: 如果是，模块之间连接的 bundle 定义有吗？**
>    **A: N/A**。阶段 A 没有任何模块，因此没有 bundle 定义。`include/tlm/gpu/` 目录下无任何 bundle/port 头文件；`PipelineTLM`/`ScoreboardTLM`/`KernelLaunchTLM` 也不含 ChStream 端口/bundle。
>
> **架构合理性**: 阶段 A 是接口层抽象（消除 PTX 字符串查表 + 虚拟寄存器硬编码），不是新的仿真端点；走"协调者注入"模式是 HSK-8 vendor 接口锁定的形态，是**正确的架构选择**。
>
> **后续关注点**: 阶段 B 的 `IMemoryPort` 涉及真实 CppTLM NoC/Cache 事务，是**唯一值得重新评估** ChStream 化的节点。阶段 B 启动时（`cpptlm-dgpu-d1-cdna-isa-phase-b`）专门决策一次即可。
>
> **不在阶段 A 范围**的 bundle 化需求：阶段 D 拓扑层"isa_type"切换是 JSON 配置补丁，不需模块化。阶段 C 内部 SM/Wavefront 状态不需要外部端口。