# gpu.common 微架构文档

> **类别**: gpu > (common)
> **状态**: 🟡 规划中（跨 Phase 7.B-F 共享概念）
> **Header**: (无独立文件 — 概念文档)
> **蓝图来源**: gem5 `src/gpu-compute/ComputeUnit.py` + `src/gpu-compute/Wavefront.py`
> **首版 commit**: 蓝图（来自 `docs/research-cpptlm-gpu-fused-soc-survey.md` §2.3 + §2.5）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.3
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §3.3

---

## 1. 设计目标（蓝图）

本文件是 **GPU 路径的跨 phase 概念文档**——不绑定任何具体类，记录 v0 已采纳 + Phase 7.B-F 待细化的 GPU 通用术语、设计决策、字段语义对位。**与 gem5 对位**: `ComputeUnit.py` + `Wavefront.py`（共享概念子集）。

**目标读者**:
- `gpu-gputlm.md` / `gpu-compute_unit.md` / `gpu-kernel-launch.md` / `gpu-tcc.md` / `gpu-pcie_bridge.md` 的实施者
- Phase 7.A+ 任何 GPU 模块的 reviewer

## 2. 通用概念（规划）

### 2.1 GPU 抽象层次

```
┌─────────────────────────────────────────────────────────────┐
│                  GPU 抽象层次（v0+）                        │
├─────────────────────────────────────────────────────────────┤
│ Application                                                  │
│   ↓ roc runtime / HSA AQL packet (模拟层)                  │
│ GPUDispatcher (HSAPacketProcessor 替代)                     │
│   ↓ 简化版 kernel launch                                    │
│ KernelLaunchTLM (Phase 7.B 引入)                          │
│   ↓ WorkGroup 调度                                          │
│ ComputeUnitTLM (Phase 7.B 引入) [1 CU]                     │
│   ├─ N_WF × Wavefront (state machine)                       │
│   ├─ N_SIMD × VectorRegisterFile                           │
│   ├─ ScalarRegisterFile + RegisterFileCache                │
│   ├─ LdsState (banks, bank conflict)                       │
│   └─ 5-stage pipeline (Fetch/Issue/Decode/Execute/Mem)     │
│       ↓                                                       │
│ TCP (L1) → SQC (L1 I) → TCC (L2) → Memory                 │
│   ↑ 抽象层（CppTLM 不模拟 GPU 内部微架构）                 │
│ CPU / Memory（共享 host DDR，APU 形态）                    │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 关键术语

| 术语 | 含义 | v0 实现 | Phase 7.B+ |
|------|------|---------|-----------|
| **Kernel** | 1 个 GPU 启动任务（GEM `dispatch_t`） | `num_kernels_` + `cur_kernel_id_` | 真实 AQL 包 |
| **Workgroup (WG)** | 1 个 kernel 内的并行单元（CUDA `block`） | `num_workgroups_` | 真实调度 |
| **Wavefront (WF)** | 1 个 WG 内的 SIMD lane 组（CUDA `warp`，64 lanes） | `wavefront_id` 字段 | 真实状态机 `S_RUNNING`/`S_STALLED` |
| **SIMD Lane** | 单个 GPU 线程 | 抽象（`coalescing_factor` 字段） | 完整 VRF 模型 |
| **Coalescing** | N 个 wavefront lane 访存合并为 1 笔请求 | `coalescing_factor` 字段 | 真实 `VIPERCoalescer` |
| **LDS (Local Data Share)** | WG 私有片上 SRAM（CUDA `shared memory`） | 抽象 | Phase 7.F+ 完整模型 |
| **VRF (Vector Register File)** | 每 SIMD lane 的向量寄存器 | 抽象 | Phase 7.F+ 完整模型 |
| **Barrier** | WG 内同步（CUDA `__syncthreads()`） | 抽象 | Phase 7.F+ 完整模型 |
| **Waitcnt** | 内存访问完成计数（VA/LGKM/EXP 三类） | 抽象 | Phase 7.F+ 完整模型 |
| **Acquire/Release** | 跨 WG 内存一致性屏障（GPU-VIPER 协议） | 抽象 | Phase 7.C 协议集成 |
| **HSA AQL** | Heterogeneous System Architecture Queue Language 包 | 抽象 | Phase 7.F+ |
| **Doorbell** | Host → GPU 唤醒信号（mmio 写） | 抽象 | Phase 7.F+ |
| **Mtype** | 缓存一致性 3-bit 编码（C_RW_S / UC_L2 / UC_All） | 抽象 | Phase 7.C |

## 3. 设计决策（D1-D5 跨 GPU 路径）

| # | 决策 | 采纳方案 | 影响 GPU 路径所有模块 |
|---|------|----------|------------------------|
| **D1** | 协议策略 | Phase 7.A 用简化 I/S/M 三态 + write-through bypass；Phase 7.C 升级 MOESI + GPU_VIPER 简化版 | CacheTLM protocol-aware 改造 → GPU L1 |
| **D2** | CU 粒度 | **黑盒优先**（只发请求，不模拟 5-stage pipeline / ISA / SIMD） | GPUTLM v0 / ComputeUnitTLM（Phase 7.B） |
| **D3** | Wavefront/Coalescing | **抽象掉**（用 `coalescing_factor` 参数代替精确模拟） | `coalescing_factor` 字段已含于 ComputeReqBundle |
| **D4** | HSAPP/CP/Dispatcher | **简化到极致**（KernelLaunchTLM ~150 行代替 HSA 三件套 3000+ 行） | 推迟到 Phase 7.F+ |
| **D5** | 目录结构 | `include/tlm/gpu/` 子目录 | ComputeUnitTLM / KernelLaunchTLM / TCC_TLM / PCIBridgeTLM |

## 4. 字段语义对位（gem5 ↔ CppTLM）

| gem5 字段 / 状态 | CppTLM 字段 / 状态 | 来源 gem5 文件 |
|------------------|---------------------|---------------|
| `Kernel.id` | `kernel_id` (u32) | `src/gpu-compute/ComputeUnit.py:439-498` |
| `ComputeUnit.dispWorkgroup(wg_id)` | `workgroup_id` (u32) | `src/gpu-compute/ComputeUnit.py:439-498` |
| `Wavefront.wfSlotId` | `wavefront_id` (u32) | `src/gpu-compute/Wavefront.py:78-118` |
| `Wavefront.status_e` (S_RUNNING / S_STALLED / S_BARRIER) | **v0 抽象** | `src/gpu-compute/Wavefront.py:78-118` |
| `VIPERCoalescer.coalesce_factor` | `coalescing_factor` (u32) | `src/mem/ruby/protocol/GPU_VIPER-TCP.sm` |
| `LdsState.banks / bankConflictPenalty` | **v0 抽象** | `src/gpu-compute/lds_state.py` |
| `Shader.impl_kern_launch_acq` | **v0 总是 true**（APU 形态默认） | `configs/example/apu_se.py:170-180` |
| `Shader.impl_kern_end_rel` | **v0 总是 false**（write-through 无需 release） | `configs/example/apu_se.py:170-180` |
| AQL `dispatch_id` | `kernel_id` | `src/dev/hsa/hsa_packet.hh:28-72` |

## 5. Bundle 字段使用（ComputeReqBundle 跨 GPU 路径）

**`ComputeReqBundle` 字段**（`include/bundles/compute_bundles_tlm.hh`）——所有 GPU 模块共享：

| 字段 | 必用 | 含义 | 跨 GPU 模块一致性 |
|------|------|------|------------------|
| `transaction_id` | ✅ | 跨模块事务追踪 | ✅ 相同（`ComputeUnitTLM`/`TCC_TLM`/`GPUTLM` 共用） |
| `kernel_id` | ✅ | 标识当前 kernel | ✅ 同 GPUTLM |
| `workgroup_id` | ✅ | 标识 WG | ✅ 同 GPUTLM |
| `wavefront_id` | ✅ | 标识 WF | ✅ 同 GPUTLM |
| `coalescing_factor` | ✅ | 抽象 coalescing | ✅ 同 GPUTLM |
| `parent_id` / `fragment_id` / `fragment_total` | 可选 | 多拍 fragment（v0 全部 = 0/0/1） | Phase 7.F+ 启用 |
| `address` | ✅ | 内存地址 | ✅ 同 GPUTLM |
| `data` | ✅ | 写数据 | ✅ 同 GPUTLM |
| `is_write` | ✅ | 读写标志 | ✅ 同 GPUTLM |
| `size` | ✅ | 字节数 | ✅ 同 GPUTLM |

## 6. 蓝图对齐

### 6.1 gem5 蓝图来源

- `src/gpu-compute/ComputeUnit.py`（CU 核心 ~1000 行）
- `src/gpu-compute/Wavefront.py`（WF 状态机 ~200 行）
- `src/gpu-compute/scoreboard_check_stage.py` / `schedule_stage.py` / `exec_stage.py`（5 阶段管线）
- `src/gpu-compute/lds_state.py`（LDS 状态机）
- `src/mem/ruby/protocol/GPU_VIPER-{TCP,SQC,TCC}.sm`（一致性协议状态机）
- `configs/example/apu_se.py:439-498`（CU 装配循环 + 拓扑创建）
- `src/dev/hsa/hsa_packet.hh`（HSA AQL 包结构）
- `src/dev/hsa/hsa_packet_processor.py`（HSAPP PIO doorbell 处理）

### 6.2 CppTLM 抽象层级对照

| gem5 真实层级 | CppTLM v0 抽象 | Phase 7.F+ 增强 |
|---------------|---------------|---------------|
| Wavefront status state machine (7 states) | 抽象（一个 `cur_kernel_id_`） | Phase 7.B 引入 `WavefrontTLM` |
| 5-stage pipeline (Fetch/Issue/Decode/Execute/Mem) | 抽象（每个 tick 直接发请求） | Phase 7.F+ |
| LDS bank conflict penalty | 抽象 | Phase 7.F+ |
| VRF / SRF / RegisterFileCache | 抽象 | Phase 7.F+ |
| HSA AQL 包解析（dispatch / barrier / agent） | 抽象 | Phase 7.F+ |
| Doorbell PIO 监听 | 抽象 | Phase 7.F+ |
| SQC I-cache + ITLB | 抽象（v0 不区分 I/D） | Phase 7.F+ |
| TCP write-through to TCC | 抽象 | Phase 7.D TCC_TLM 真实实现 |
| Acquire/Release 一致性 | 抽象（v0 总是 true） | Phase 7.C MOESI 协议 |
| Mtype 编码 | 抽象 | Phase 7.C |

## 7. 实施路径

### 7.1 v0 已实施（Phase 7.A，commit `828f037`）

- ✅ `ComputeReqBundle` / `ComputeRespBundle` 12 字段
- ✅ `GPUTLM` v0 黑盒（5 setter + 4 阶段 tick + 7 stats）
- ✅ 5/5 Catch2 测试通过（`[gpu]` 标签）

### 7.2 Phase 7.B 蓝图

- `ComputeUnitTLM`（`include/tlm/gpu/compute_unit_tlm.hh`）：从 `ChStreamModuleBase` 派生，共享 `compute_unit_base` 基类
- `KernelLaunchTLM`（`include/tlm/gpu/kernel_launch_tlm.hh`）：简化版 HSA Dispatcher
- 抽出 `compute_unit_base` 共享基类（消除与 GPUTLM v0 的代码重复）
- 修复 `on_config_loaded` JSON 解析（spec §6.1 R5 风险之一）

### 7.3 Phase 7.D 蓝图

- `TCC_TLM`（`include/tlm/gpu/tcc_tlm.hh`）：GPU L2 + 写合并 + snoop fan-in
- `DualPortStreamAdapter` 桥接（TCP 侧 ↔ Directory/Memory 侧）
- MemoryTLM 双 Bundle 注册（修复 R5 风险）

### 7.4 Phase 7.F+ 蓝图

- 完整 5-stage pipeline
- 真实 LDS / VRF / SRF 模型
- HSA AQL 包解析
- Doorbell PIO 监听

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **v0 完全不模拟 GPU 内部微架构**——5-stage pipeline / wavefront / LDS 全部抽象 | 高 | 中 | D2 决策接受；Phase 7.F+ 增强 |
| R2 | **跨模块代码重复**（GPUTLM v0 ↔ ComputeUnitTLM ↔ TrafficGenTLM ↔ CPUTLM） | 高 | 中 | Phase 7.B 抽出 `compute_unit_base` |
| R3 | **`on_config_loaded` 缺口**（v0 stub） | 高 | 中 | Phase 7.B 统一修复 |
| R4 | **R5 风险**——MemoryTLM 不接受 ComputeReqBundle | 高 | 中 | Phase 7.D 双 Bundle 注册 |
| R5 | **D4 决策缺口**——HSAPP/CP/Dispatcher 简化过度，可能漏掉真实 APU 行为 | 中 | 中 | Phase 7.F+ 渐进增强 |
| R6 | **Acq/Rel 一致性**——v0 总是 true/false，可能漏 race condition | 中 | 高 | Phase 7.C 协议测试 |
| R7 | **GPU ISA 不模拟**——v0 完全无指令解码 | 高 | 中 | D2 决策接受；TLM 框架不模拟 ISA |
| R8 | **CUISA / SIMT 行为**——v0 无 SIMT 概念 | 中 | 低 | 文档化"v0 黑盒"语义 |

## 9. 设计决策点

### D1 协议细节

- **Q**: Phase 7.C MOESI 状态机，TCP/SQC/TCC 各自实现哪几个子状态？
- **状态**: 留待 Phase 7.C spec/plan 设计时确定
- **依赖**: gem5 `GPU_VIPER-{TCP,SQC,TCC}.sm` 文件

### D2 共享基类 API

- **Q**: `compute_unit_base` 暴露什么接口？是否包含 `inflight_txns_` 跟踪？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: 含 `set_num_kernels` / `set_kernel_duration` / 共享 `inflight_txns_` / 共享 `do_reset`

### D3 KernelLaunch 触发机制

- **Q**: `KernelLaunchTLM` 触发 `ComputeUnitTLM` 是用 JSON 配置时间表，还是 periodic ？
- **状态**: 留待 Phase 7.B 设计时确定
- **建议**: periodic（每 `kernel_launch_interval_` 周期触发）

### D4 TCC 写合并粒度

- **Q**: `TCC_TLM` 写合并是按地址（同一地址连续写合并），还是按 flit（同一 packet 写合并）？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 按地址（与真实 GPU 行为一致）

### D5 PCIe BAR 路由

- **Q**: `PCIBridgeTLM` 暴露 BAR0（VRAM）/ BAR2（doorbell）/ BAR5（MMIO）三组还是统一？
- **状态**: 留待 Phase 7 备选 dGPU 设计时确定
- **建议**: 三组（与 gem5 AMDGPUDevice 行为一致）

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自 `docs/research-cpptlm-gpu-fused-soc-survey.md` §2.3 + §2.5）
- **2026-06-11**: B3 批次设计 — 提取 D1-D5 跨 GPU 决策 + 字段语义对位 + 蓝图对齐表
- **Phase 7.B (未来)**: 新增 `ComputeUnitTLM` / `KernelLaunchTLM` 蓝图
- **Phase 7.D (未来)**: 新增 `TCC_TLM` 蓝图
- **Phase 7.F+ (未来)**: 新增 5-stage pipeline / 真实 LDS / VRF / HSA AQL 蓝图
