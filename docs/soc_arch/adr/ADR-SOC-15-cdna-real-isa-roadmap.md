# ADR-SOC-15: dGPU SoC v1.0 CDNA 真实物理 ISA 演进路线图

> **状态**: 📋 Proposed — 2027-02-09
> **日期**: 2027-02-09
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: L5/L6 计算设备 + L7 Memory 系统 + 跨仓契约 (HSK-9 触发)
> **类别**: SoC 架构 / v1.0 战略 + 长期演进
> **关联文档**:
> - [`docs/soc_arch/architecture/11-cdna-real-isa-integration.md`](../architecture/11-cdna-real-isa-integration.md)（完整方案设计：本 ADR 仅给路线图与门禁）
> - [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（v1.0 总架构蓝图）
> **关联 ADR**:
> - **ADR-SOC-09**（NVIDIA + AMD 双 Vendor 战略 — 本 ADR 实施其 AMD 路径的物理 ISA 演进）
> - **ADR-SOC-11**（PcieEndpointIP 17 ports 整合 — 23 ABI 冻结不变量保护本 ADR）
> - **ADR-SOC-06**（cpptlm-v05-mvp — 提供 KernelLaunchTLM 基线）
> **关联 OpenSpec**: 待建（阶段 A 启动时开 `cpptlm-dgpu-d1-cdna-isa-phase-a` 等 4 个 change）
> **关联研究综述**:
> - Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP`（双时钟域契约）
> - Oracle 演进分析 `ses_f982f1597ffejYGzVek5F7zBfP` follow-up（4 阶段路线图 + 选型）
> - MGPUSim（AMD GCN3 emulator + timing 公开参考）
> - gem5 多 ISA 描述符模式

---

## 1. Context（背景）

### 1.1 当前架构现状
- **PTX-EMU** 作为虚拟 ISA 执行器已交付 `IPtxEmuDevice` 12/12 wired（HSK-8 ACCEPTED，drift_check 6 invariants PASS）
- **CppTLM** 作为 dGPU SoC Multi-IP Microarchitecture 周期精确仿真框架，已交付 PCIe EP 7 阶段 + 9 类 SimModule + PipelineTLM/TensorCoreTLM/ScoreboardTLM
- **HSK-6 ACCEPTED** 已物理删除 `IPtxEmuDriver` + `PtxEmuDriverShim` + vendored `cpptlm_bridge.h`，明确"CppTLM 做 timing，PTX-EMU 做 functional" 的双时钟域契约
- **当前架构属于"timing 参数注入"模型**：PTX-EMU `SMContext::exe_once()` 内嵌 SM 内 timing 应用，CppTLM 提供 timing 参数 + 系统级 timing（per Oracle 范式分析）

### 1.2 演进动机
- **真实工作负载需求**：仅 PTX 不足以验证 MI300X 等 CDNA GPU 上的真实软件栈（ROCm / KFD / AQL）
- **精度验证基线断裂**：PTX 模式的"正确性"是自洽的，真实 ISA 需要与真实硬件或 MGPUSim/gem5 对拍
- **现有抽象接口被 PTX 特化污染**：`PipelineTLM::get_fractional_cycles(string, pipe)` 字符串查表无法扩展到 SASS/CDNA/RVV

### 1.3 与 ADR-SOC-09 的关系
- ADR-SOC-09 §2 D3 明确："AMD KFD 路径：需依赖 UsrLinuxEmu 跨仓承诺，**本 ADR 不单方宣布** AMD KFD 实施时间表"
- 本 ADR (SOC-15) 范围：**仅 CppTLM 端**的物理 ISA 演进（前端执行器 + 内存 Seam + 微架构资源建模）
- AMD KFD 用户态 IOCTL 与 Driver Stack 由 UsrLinuxEmu ADR-088 协调（不在本 ADR 范围）

---

## 2. Decision（决策）

### D1. 演进目标：双轨前端 + 统一 Timing 宿主

✅ **前期 PTX 启动，后期无缝切换到类 AMD CDNA 真实物理 ISA**：

```
[阶段 A: 中立化]  (PTX 阶段即可启动, 5-8 工作日)
   └─ 引入 InstrDescriptor 替代字符串查表
   └─ PipelineTLM 改为 LatencyClass 枚举查表
   └─ ScoreboardTLM 抽象为双模适配器 (VirtualReg / HardwareCounter)

[阶段 B: 内存 Seam] (最高优先, 8-15 工作日, HSK-9)
   └─ 定义异步 IMemoryPort 接口 (send_request / set_response_callback)
   └─ 关闭 PTX 模式 "查表 200 cycle" 假内存
   └─ exe_once() 契约升级为 "1 cycle step + 事件交换"

[阶段 C: CDNA 引擎接入] (15-30 工作日)
   └─ 引入 CDNA-EMU (GFX942), 包装为 IComputeDevice
   └─ 覆盖核心指令集 (SALU/VALU/MFMA/global_load/global_store)
   └─ 实施 CDNAHazardTracker (vmcnt/lgkmcnt/expcnt)

[阶段 D: 双轨运行时与生产校准] (15-20 工作日)
   └─ 拓扑层运行时 config 切换 ("is_type": "ptx" / "cdna3")
   └─ 接入 ADR-SOC-09 Pm4Decoder-AMD 路径
   └─ 5 类 microbenchmark vs AMD 官方白皮书 ±15% 校准
```

### D2. 抽象接口：IComputeDevice + IMemoryPort

✅ **将 `IPtxEmuDevice` 升级为 `IComputeDevice`**：
- 12 方法大部分 ISA-agnostic，保留：`initialize/shutdown/exe_once/sm_exe_once/wave_exe_once/get_thread_state/set_active_mask/set_next_pc/get_warp_status/is_finished`
- 2 方法深度 PTX 污染需重构：`set_scoreboard`（移除，CDNA 改 `CDNAHazardTracker`）+ `attach_timing` 三参数接口（替换为描述符化）

✅ **新增 `IMemoryPort` 异步内存事务接口**：
- `send_request(MemRequest)`：执行器向 CppTLM 发射内存事务
- `set_response_callback(cb)`：CppTLM 异步回调
- 这是**真实 ISA 演进的最大架构增值**，Phase A 之前不做就永远做不成

✅ **新增 `InstrDescriptor` POD 标准化指令描述符**：
- 字段：`PipeClass` / `LatencyClass` / `CtrlBits` / 寄存器号 / 内存操作元数据
- 由执行器 decode 阶段产出，CppTLM 仅消费不解析文本

### D3. 引擎选型：类 AMD CDNA3 (GFX942) 优先

✅ **第一目标：AMD CDNA3（MI300X）**：
- ISA 手册完全公开（AMD 官方 Architecture Reference）
- MGPUSim / gem5-GPU 有现成开源实现可对照
- 原生 Wave64 与 CppTLM 现有 Wavefront 抽象兼容度最高
- `v_mfma_*` 指令族是 AI/HPC 主流工作负载

✅ **次选：RISC-V Vector (RVV)**（未来扩展）：
- 完全开源，gem5/Spike 参考实现可重用
- 但需在接口中适度泛化 Wave/Lane 概念

❌ **不推荐 NVIDIA SASS 作为第一目标**：
- 无公开 ISA 手册，靠逆向（nvdisasm 输出），控制位语义模糊
- 没有除真实芯片外的可靠基准对拍

### D4. 关键不变量保护

✅ **23 ABI 冻结保护**（per ADR-SOC-11）：
- `include/abi/cpptlm_emulator.h` 零修改
- `include/tlm/gpu/pcie_endpoint_tlm.h` 仅加 `[[deprecated]]` 标注
- `PcieEndpointIP` 17 端口布局保留

✅ **CppTLM Clean Room 原则**：
- PTX-EMU / CDNA-EMU / RVV-EMU 均作为 `external/` 子模块或动态库注入
- CppTLM 仅感知 `InstrDescriptor` + `IComputeDevice` + `IMemoryPort`

✅ **HSK 协调纪律**：
- `PTXEMU_API_VERSION=1` → `ICOMPUTE_API_VERSION=1` 触发需 HSK-9
- 跨仓契约变更不增量加方法，必须冻结发布

### D5. 双轨运行时配置

✅ **拓扑 JSON 层运行时切换**（per ADR-SOC-09 D1 蓝图模式）：

```json
"compute_engine": {
    "isa_type": "cdna3",    // 可选: "ptx" (阶段 A-D) / "cdna3" (阶段 C-D) / "rvv" (未来)
    "wave_size": 64,          // CDNA=64, PTX=32
    "cu_count": 64,
    "hazard_mode": "counter"  // "register" (PTX virtual) / "counter" (CDNA waitcnt)
}
```

---

## 3. Consequences（后果）

### 3.1 正面影响
- **架构增值**：双 ISA 兼容让 CppTLM 既服务 AI/HPC（NVIDIA 主流）也覆盖 ROCm 已商业化市场
- **精度跃升**：从 PTX 推测式 timing 提升到 CDNA 显式计数器 timing，IPC 与官方白皮书对齐
- **验证基线建立**：与 MGPUSim/gem5-GPU 对拍，建立可重复的性能基准
- **抽象泛化红利**：InstrDescriptor / IComputeDevice / IMemoryPort 三接口支持未来 RVV 等更多 ISA

### 3.2 负面影响
- **跨仓契约膨胀**：`IPtxEmuDevice` 12 方法升级为 `IComputeDevice` 15+ 方法 + `IMemoryPort` 4 方法，HSK-9 协调成本
- **执行器选型风险**：AMD CDNA emulator 选型决定阶段 C 进度，需在阶段 A 启动时同步调研
- **精度校准周期**：阶段 D 的 microbenchmark vs 真实硬件对拍可能暴露模型误差，需多轮迭代
- **现有 PTX 代码解耦成本**：阶段 A 重构 `PipelineTLM` 字符串查表为 `LatencyClass` 查表，需重写 6 个查表函数（per `pipeline_tlm.cc:21-137`）

### 3.3 关键限制
- **阶段 A 必须在阶段 B 之前完成**：因为 `IMemoryPort` 需要 `InstrDescriptor` 的 `is_memory + target_vaddr + mem_size` 字段
- **阶段 B 必须在阶段 C 之前完成**：CDNA 引擎需要真实内存路径才能验证 `s_waitcnt` 计数器精度
- **AMD KFD 用户态由 UsrLinuxEmu ADR-088 协调**：本 ADR 不涉入

---

## 4. Implementation（实施）

### 4.1 阶段 A：契约中立化（5-8 工作日）

| Task | 内容 | 文件 |
|------|------|------|
| A.1 | 引入 `InstrDescriptor` + `PipeClass` + `LatencyClass` 枚举 | `include/tlm/gpu/instruction_descriptor.hh` |
| A.2 | PTX-EMU ANTLR4 decode 出口补充描述符转换器 | `external/PTX-EMU/src/ptxsim/`（PTX-EMU 仓） |
| A.3 | `PipelineTLM::get_fractional_cycles(string, pipe)` 重写为 `get_latency(LatencyClass)` | `src/tlm/gpu/pipeline_tlm.cc` |
| A.4 | `ScoreboardTLM` 抽象为双模适配器 (VirtualReg/HardwareCounter) | `include/tlm/gpu/scoreboard_tlm.hh` |
| **Gate A** | `[pcie][axi][e2e][wave2]` 全绿 + PTX 输出与改造前 bit-identical | |

### 4.2 阶段 B：内存 Seam 闭环（8-15 工作日，HSK-9）

| Task | 内容 | 文件 |
|------|------|------|
| B.1 | 定义 `IMemoryPort` 虚基类 + `MemRequest` 结构 | `include/tlm/gpu/memory_port_interface.hh` |
| B.2 | `GpuMemoryPortAdapter` 实现（MemRequest → CacheReqBundle / Axi4Bundle） | `src/tlm/gpu/gpu_memory_port_adapter.cc` |
| B.3 | 拦截 PTX-EMU `ld.global`/`st.global`，改为 `IMemoryPort::send_request()` | `external/PTX-EMU/src/ptxsim/core/`（PTX-EMU 仓） |
| B.4 | `KernelLaunchTLM::tick()` 增加 `device_->exe_once()` + `IMemoryPort` 响应回收（Wave 2 T4.2 落地） | `src/tlm/gpu/kernel_launch_tlm.cc` |
| **Gate B** | 内存延迟动态波动（L1 Hit vs L2 Miss vs HBM 调度差异）+ G-D8 chaos test 闭环 | |

### 4.3 阶段 C：CDNA 引擎接入（15-30 工作日）

| Task | 内容 | 文件 |
|------|------|------|
| C.1 | CDNA-EMU 选型（建议：参考 MGPUSim 剥离 + LLVM MC for GFX942） | `external/CDNA-EMU/` |
| C.2 | `CdnaEmuDevice : IComputeDevice` 实现 + 工厂 | `include/tlm/gpu/cdna_emu_device.hh` |
| C.3 | CDNA 指令 → `InstrDescriptor` 映射 (SALU/VALU/MFMA/global_load/global_store) | `external/CDNA-EMU/src/` |
| C.4 | `CDNAHazardTracker` (vmcnt/lgkmcnt/expcnt) | `include/tlm/gpu/cdna_hazard_tracker.hh` |
| **Gate C** | SGEMM/Vector Add CDNA kernel 通过；周期数与官方白皮书 ±15% | |

### 4.4 阶段 D：双轨运行时与生产校准（15-20 工作日）

| Task | 内容 | 文件 |
|------|------|------|
| D.1 | 拓扑 JSON 层运行时 config 切换 (`isa_type`/`wave_size`/`hazard_mode`) | `examples/dgpu_soc_with_pcie_ip.json` |
| D.2 | 接入 ADR-SOC-09 `Pm4Decoder-AMD` 路径 | `include/tlm/gpu/pm4_decoder_mvp.hh` |
| D.3 | 5 类 microbenchmark vs AMD 官方白皮书 ±15% 校准 | `test/python/test_gpgpu_sim_comparison.py` |
| **Gate D** | 切换 `"isa_type": "ptx"` 与 `"cdna3"` 各自完整回归 | |

### 4.5 总预估工作量

| 阶段 | 工作量 | 触发 |
|------|--------|------|
| A. 抽象中立化 | 5-8 人天 | 即刻 |
| B. 内存 Seam | 8-15 人天 | A 完成后 |
| C. CDNA 引擎 | 15-30 人天 | B 完成后 |
| D. 双轨校准 | 15-20 人天 | C 完成后 |
| **合计** | **43-73 人天** | |

---

## 5. Risks（风险）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 阶段 B 内存 Seam 延期（最关键路径） | 🔴 高 | 阶段 A 启动时同步调研 `IMemoryPort` 接口设计；HSK-9 协调提前 14 天 |
| **R2** | CDNA-EMU 选型风险（开源成熟度不足） | 🟡 中 | 阶段 A 末尾启动选型 spiek；备选：自研最小子集 + AMD 官方 ISA 手册手写 decode |
| **R3** | 跨仓契约膨胀导致 HSK-N 反复 | 🟡 中 | 禁止 PR 增量加方法；阶段 A/B 各只 bump 一次 VERSION |
| **R4** | 验证基线断裂（精度校准与真实硬件偏差超 15%） | 🟡 中 | 阶段 D 多轮迭代；保留 fallback 到 accel-sim trace-driven 模式 |
| **R5** | PRF bank conflict / s_waitcnt 显式计数器实现复杂度 | 🟡 中 | 阶段 C/D 渐进实施；先覆盖主流 `v_*` 指令，边界情况文档化 |
| **R6** | AMD KFD 用户态与本 ADR 时序错位 | 🟢 低 | per ADR-SOC-09 D3：不单方宣布；UsrLinuxEmu 反馈后追加子 ADR |
| **R7** | 23 ABI 兼容性边界被破坏 | 🟢 低 | 阶段 A 启动时静态断言校验；任何 ABI 头修改立即触发 HSK-N |
| **R8** | CppTLM Clean Room 边界被污染（引入执行器头文件） | 🟢 低 | PR review 强制检查；阶段 C 引入子模块而非源码耦合 |

---

## 6. 参考文献

### 6.1 关联 ADR

| ADR | 关联 |
|-----|------|
| ADR-SOC-09 | NVIDIA + AMD 双 Vendor 战略（提供 AMD 路径战略依据） |
| ADR-SOC-11 | PcieEndpointIP 17 ports 整合（23 ABI 冻结不变量保护） |
| ADR-SOC-06 | cpptlm-v05-mvp（提供 KernelLaunchTLM 基线） |

### 6.2 关联 OpenSpec changes（待建）

| Change | 计划启动 |
|--------|----------|
| `2027-02-09-cpptlm-dgpu-d1-cdna-isa-phase-a` | 阶段 A 启动时 |
| `2027-02-09-cpptlm-dgpu-d1-cdna-isa-phase-b` | 阶段 A Gate 通过后 |
| `2027-02-09-cpptlm-dgpu-d1-cdna-isa-phase-c` | 阶段 B Gate 通过后 |
| `2027-02-09-cpptlm-dgpu-d1-cdna-isa-phase-d` | 阶段 C Gate 通过后 |

### 6.3 关联研究综述

| 综述 | 关联 |
|------|------|
| Oracle 范式分析 `ses_f982f1597ffejYGzVek5F7zBfP` | 双时钟域契约 + gpgpu-sim/gem5/MGPUSim 范式对比 |
| Oracle 演进分析 `ses_f982f1597ffejYGzVek5F7zBfP` follow-up | 4 阶段路线图 + 选型 + 风险 |
| `docs/research/WDUtoSM/overview.md` | AMD SPI/SQ 双 Vendor 路径 |
| `docs/research/CP/amd/overview.md` | AMD HQD/IB/doorbell |

### 6.4 跨仓协议

| 协议 | 状态 |
|------|------|
| HSK-6 (桥接关系废止) | ✅ ACCEPTED |
| HSK-8 (`IPtxEmuDevice` 12/12) | ✅ ACCEPTED |
| HSK-9 (`ICOMPUTE_API_VERSION=1` 触发) | 🔵 预留 |

---

## Status Update
- **2027-02-09**: 📋 Proposed。本 ADR 是 ADR-SOC-09 §2 D3"AMD KFD 路径"在 CppTLM 端的执行延伸；AMD KFD 用户态 IOCTL 与 Driver Stack 由 UsrLinuxEmu ADR-088 协调（不在本 ADR 范围）。4 阶段路线图（中立化 → 内存 Seam → CDNA 引擎 → 双轨校准）合计 43-73 人天。完整方案设计见 [`docs/soc_arch/architecture/11-cdna-real-isa-integration.md`](../architecture/11-cdna-real-isa-integration.md)。