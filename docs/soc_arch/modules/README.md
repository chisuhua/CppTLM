# CppTLM 模块架构索引

> **生成时间**: 2026-06-11
> **用途**: 梳理项目内已实现 + 正准备开发的 SoC 模块，按模块类型分组
> **关联文档**:
> - 调研报告：[`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md)
> - Roadmap Phase 7：[`roadmap.md`](../../../roadmap.md) §Phase 7
> - Phase 7.A spec：[`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md)
> - Phase 7.A plan：[`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../../superpowers/plans/2026-06-11-phase7a-gpu-infra.md)

---

## 1. 模块类型分类总览

CppTLM 模块按功能域分 **8 大类**：

| # | 类型 | 已实现 | 正准备开发 | 来源 |
|---|------|--------|------------|------|
| 1 | **CPU 端模块** (Initiator) | 3 | 0 | 调研 §2.3, §4 Phase 0 | [`cpu-cputlm.md`](./cpu-cputlm.md) [`cpu-traffic_gen.md`](./cpu-traffic_gen.md) [`cpu-cpusim_legacy.md`](./cpu-cpusim_legacy.md) | ✅ / ✅ / ⚠️ |
| 2 | **内存子系统** | 2 | 2 | 调研 §2.2 | [`memory-memtlm.md`](./memory-memtlm.md) | ✅ |
| 3 | **Cache 层级** | 0 (v0 归为 memory) | 3 | 调研 §2.3 | [`cache-l1.md`](./cache-l1.md) | ✅ (CacheTLM 在 §2.4) |
| 4 | **Interconnect / NoC** | 5 | 1 | 调研 §2.4 | [`interconnect-crossbar.md`](./interconnect-crossbar.md) [`interconnect-arbiter.md`](./interconnect-arbiter.md) [`noc-router.md`](./noc-router.md) [`noc-nic.md`](./noc-nic.md) | ✅ / ✅ / ✅ / ✅ |
| 5 | **GPU / GPGPU** | 1 | 5 | 调研 §2, §4 Phase 7.A-F | [`gpu-gputlm.md`](./gpu-gputlm.md) | ✅ (v0 已有 doc)；其余 5 个 🟡 |
| 6 | **IO / DMA / 设备** | 0 | 5 | 调研 §2.5 | (暂无) | 🟡 全部 |
| 7 | **Coherence / 桥接** | 0 | 3 | 调研 §2.6, §4 Phase 7.C-D | [`coherence-domain.md`](./coherence-domain.md) | ✅ (CoherenceDomain 基础设施) |
| 8 | **RTL 桥接** | 2 | 0 | 调研 §1.3 | [`rtl-hybrid_cache.md`](./rtl-hybrid_cache.md) [`rtl-fragment_mapper.md`](./rtl-fragment_mapper.md) | ✅ / ✅ |

---

## 2. 已实现模块（v2.1）

### 2.1 CPU 端模块 (Initiator)

| 模块类 | Header | 命名空间 | 角色 | 注册位置 | 状态 |
|--------|--------|----------|------|----------|------|
| `CPUTLM` | `include/tlm/cpu_tlm.hh` | global | 0x1000–0x1100 线性只读，10cy 间隔 | `REGISTER_CHSTREAM` (chstream_register.hh:33) | ✅ v2.1 |
| `TrafficGenTLM` | `include/tlm/traffic_gen_tlm.hh` | global | 3 基础 + 4 压力模式 (SEQUENTIAL/RANDOM/TRACE/HOTSPOT/STRIDED/NEIGHBOR/TORNADO) | `REGISTER_CHSTREAM` (chstream_register.hh:34) | ✅ v2.1 |
| `CPUSim` | `include/modules/legacy/cpu_sim.hh` | global | Legacy CPU 仿真 | `REGISTER_OBJECT` (modules.hh:30, **仅 BUILD_LEGACY_MODULES=ON**) | ⚠️ Legacy |

### 2.2 内存子系统

| 模块类 | Header | 命名空间 | 角色 | 注册位置 | 状态 |
|--------|--------|----------|------|----------|------|
| `MemoryTLM` | `include/tlm/memory_tlm.hh` | global | 简化 DRAM 桩（rd 100cy / wr 120cy + 行缓冲） | `REGISTER_CHSTREAM` (chstream_register.hh:31) | ✅ v2.1 |
| `HybridCacheWrapper` | `include/rtl/hybrid_cache_wrapper.hh` | `cpptlm::rtl` | TLM↔RTL 桥接（PIMPL → CppHDL HybridCacheComponent） | `REGISTER_CHSTREAM` (chstream_register.hh:65, **仅 BUILD_RTL=ON**) | ✅ v2.1 |

### 2.3 Cache 层级

> **现状**: 仅有 `MemoryTLM` 内置的简化 cache 行为（无独立 CacheTLM 类）。`include/tlm/cache_tlm.hh` 实际是简化版 cache 模块（见 §2.4）。

### 2.4 Interconnect / NoC

| 模块类 | Header | 命名空间 | 角色 | 注册位置 | 状态 |
|--------|--------|----------|------|----------|------|
| `CacheTLM` | `include/tlm/cache_tlm.hh` | global | 简化 L1（hit 5cy / miss 50cy，std::map 查表） | `REGISTER_CHSTREAM` (chstream_register.hh:30) | ✅ v2.1 |
| `CrossbarTLM` | `include/tlm/crossbar_tlm.hh` | global | 4 端口地址路由 + 冲突检测 | `REGISTER_CHSTREAM` (chstream_register.hh:32 + MultiPortAdapter) | ✅ v2.1 |
| `ArbiterTLM<N>` | `include/tlm/arbiter_tlm.hh` | global | N→1 + 1 出 Round-Robin | `REGISTER_CHSTREAM` (chstream_register.hh:35-36) | ✅ v2.1（特化 N=2,4） |
| `tlm::RouterTLM` | `include/tlm/router_tlm.hh` | `tlm` | 5 端口双向 (N/E/S/W/Local) 六阶段流水线 + Credit 控流 + XY 路由 | `REGISTER_CHSTREAM` (chstream_register.hh:37 + BidirectionalAdapter) | ✅ v2.1 |
| `tlm::NICTLM` | `include/tlm/nic_tlm.hh` | `tlm` | 双端口非对称：PE 侧 CacheBundle + Net 侧 NoCFlitBundle，`FLITS_PER_PACKET=4` packetize | `REGISTER_CHSTREAM` (chstream_register.hh:38 + DualPortAdapter) | ✅ v2.1 |
| `tlm::LinkTLM` | `include/tlm/link_tlm.hh` | `tlm` | 物理链路延迟 + Credit 传递 | `REGISTER_CHSTREAM` (chstream_register.hh:39) | ✅ v2.1 |

### 2.5 GPU / GPGPU

| 模块类 | Header | 命名空间 | 角色 | 注册位置 | 状态 |
|--------|--------|----------|------|----------|------|
| `tlm::GPUTLM` | `include/tlm/gpu/gpu_tlm.hh` | `tlm` | GPU 黑盒发起器（5 setter + 50% 读写混合 + 7 个 StatGroup 统计） | `REGISTER_CHSTREAM` (chstream_register.hh:42 + ComputeReqBundle/ComputeRespBundle) | ✅ **v0 已实施**（Phase 7.A commit `828f037`） |

### 2.6 IO / DMA / 设备

> **现状**: **0 个模块**。gem5 的 PIO / DMA / Disk / PCIe / 以太网设备**未在 CppTLM 中建模**（调研 §2.5）。

### 2.7 Coherence / 桥接

| 模块类 | Header | 命名空间 | 角色 | 注册位置 | 状态 |
|--------|--------|----------|------|----------|------|
| `CoherenceDomain` (API) | `include/core/coherence_domain.hh` | global | MESI/MOESI enum + snoop fanout + home node + 跨域 bridge | (基础设施，**非** TLM 模块) | ✅ v2.1 基础已就绪 |
| `CoherenceState` 枚举 | `include/ext/error_context_ext.hh` | (global) | I/S/E/M/O/T — MOESI 完整状态机 | (基础设施) | ✅ v2.1 |
| `ProtocolBridge` | (规划中) | `tlm` | 跨 coherence 域桥接 | (Phase 5 待启动) | 🟡 Pending |

### 2.8 RTL 桥接

| 模块类 | Header | 命名空间 | 角色 | 状态 |
|--------|--------|----------|------|------|
| `cpptlm::rtl::HybridCacheComponent` | `include/rtl/hybrid_cache_component.hh` | `cpptlm::rtl` | 纯 C++20 CppHDL 组件，2 态 FSM | ✅ v2.1 |
| `cpptlm::rtl::FragmentMapper` | `include/rtl/fragment_mapper.hh` | `cpptlm::rtl` | TLM↔RTL fragment 转换模板 | ✅ v2.1 |

---

## 3. 正准备开发的模块（按 Phase 7 路径）

### 3.1 Phase 7.A — GPU 基础设施（**v0 已实施**）

| 模块类 | Header 计划路径 | 命名空间 | 角色 | 蓝图 |
|--------|----------------|----------|------|------|
| `bundles::ComputeReqBundle` | `include/bundles/compute_bundles_tlm.hh` | `bundles` | GPU 请求 Bundle（核心 8 字段 + kernel_id/workgroup_id/wavefront_id/coalescing_factor） | gem5 AQL packet |
| `bundles::ComputeRespBundle` | 同上 | `bundles` | GPU 响应 Bundle | 同上 |
| `tlm::GPUTLM` v0 | `include/tlm/gpu/gpu_tlm.hh` | `tlm` | GPU 黑盒发起器（5 setter + tick() + 7 stats） | gem5 ComputeUnit.py（接口） |

**实施状态**: ✅ 全部 3 个文件已创建并 commit（`8288ad1` / `828f037` / `fb6011b`），5/5 GPU 测试通过。

### 3.2 Phase 7.B — ComputeUnit 黑盒（**Pending**）

| 模块类 | Header 计划路径 | 命名空间 | 角色 | 蓝图 |
|--------|----------------|----------|------|------|
| `tlm::ComputeUnitTLM` | `include/tlm/gpu/compute_unit_tlm.hh` | `tlm` | 真正的 Compute Unit 抽象（与 GPUTLM v0 共享基类 + `KernelLaunchTLM` 驱动） | gem5 ComputeUnit（黑盒版） |
| `tlm::KernelLaunchTLM` | `include/tlm/gpu/kernel_launch_tlm.hh` | `tlm` | 简化版 HSA Dispatcher（~150 行，tick() 中定期发 kernel launch 命令） | gem5 HSAPacketProcessor + GPUDispatcher（精简） |
| `tlm::GPUTLM` 升级 | `include/tlm/gpu/gpu_tlm.hh` | `tlm` | 实现 `on_config_loaded` 读 JSON params + 共享 `compute_unit_base` 基类 | TrafficGenTLM 现存 JSON 读取风格 |

**待办**（spec §3, plan Task 8+）：
- 抽出 `compute_unit_base` 共享基类
- 实现 `on_config_loaded` JSON 解析（修复 `TrafficGenTLM` / `CPUTLM` / `GPUTLM` 三者统一 params 读取）
- 写 `KernelLaunchTLM` 简化版 HSA 三件套
- Multi-CU 实例化（JSON 参数 `num_cus`）

### 3.3 Phase 7.C — Coherence Protocol 集成（**Pending** / 最高风险）

| 模块类 | Header 计划路径 | 命名空间 | 角色 | 蓝图 |
|--------|----------------|----------|------|------|
| `CacheTLM` (升级) | `include/tlm/cache_tlm.hh` | global | protocol-aware 改造：`CacheLine = {data, CoherenceState, sharers_bitmask}` + 6×6 状态转换表 | gem5 TCP/TCC/SQC 状态机（slicc → C++ switch） |
| `CoherenceDomain` 升级 | `include/core/coherence_domain.hh` | global | 与 CacheTLM 集成：snoop callback + `lookup_home_node()` + 跨域 bridge 真实连接 | gem5 MOESI_AMD_Base-dir |

**待办**（调研 §4 Phase 2）：
- 扩展 `CacheLine` 结构（加 `CoherenceState` + sharers bitmask）
- 实现 6×6 状态转换表：`I→S`(read), `I→M`(write), `S→M`(upgrade), `M→I`(invalidation), `S→I`(eviction)
- 实现 `get_snoop_targets()` / `handle_snoop()` 接口
- ModuleFactory 创建 CacheTLM 时查找所属 CoherenceDomain → 注册 snoop callback
- **触发升级条件**: 5+ 测试回归 → 回 Phase 7.B write-through bypass

### 3.4 Phase 7.D — TCC Bridge + 内存层次（**Pending**）

| 模块类 | Header 计划路径 | 命名空间 | 角色 | 蓝图 |
|--------|----------------|----------|------|------|
| `tlm::TCC_TLM` | `include/tlm/gpu/tcc_tlm.hh` | `tlm` | GPU last-level cache：aggregate GPU 请求 + write coalescing + snoop fan-in | gem5 TCC 状态机 |
| `MemoryTLM` 升级 | `include/tlm/memory_tlm.hh` | global | 扩展 `hbm_mode` 参数（高带宽高延迟） + 接受 ComputeReqBundle/RespBundle（双 Bundle 注册解决 R5 风险） | gem5 DRAMCtrl / HBM2Stack |

**待办**（调研 §4 Phase 3）：
- TCC 使用 `DualPortStreamAdapter`（TCP 侧 ↔ Directory/Memory 侧）
- MemoryTLM 同时注册 `CacheReqBundle/RespBundle` + `ComputeReqBundle/RespBundle`
- write coalescing（同一地址连续写合并）
- snoop fan-in 收集

### 3.5 Phase 7.E — Multi-CU + NoC 集成（**Pending**）

| 模块类 | Header 计划路径 | 命名空间 | 角色 | 蓝图 |
|--------|----------------|----------|------|------|
| `ComputeUnitTLM` 数组模式 | `include/tlm/gpu/compute_unit_tlm.hh` | `tlm` | JSON 参数 `num_cus=4` 创建多 CU 实例 | gem5 `CUs = [ComputeUnit(...) for _ in range(num_cu)]` |
| GPU NoC (复用 RouterTLM) | (无新模块) | `tlm` | 通过 `BidirectionalPortAdapter<N>` 连接 CU ↔ GPU Crossbar | gem5 GarnetRouter（5 端口）+ NetworkLink/CreditLink |

**待办**（调研 §4 Phase 4）：
- 数组化 CU 实例化（`std::vector<ComputeUnitTLM>` + JSON 触发）
- 复用现有 `RouterTLM` / `LinkTLM` / `NoCFlitBundle`
- 验证 4 CU 并行 kernel 请求经 NoC 路由到 TCC→Memory

### 3.6 Phase 7.F — Full APU SoC Demo（**Pending**）

| 模块类 | Header 计划路径 | 命名空间 | 角色 | 蓝图 |
|--------|----------------|----------|------|------|
| `apu_full_soc.json` | `configs/apu_full_soc.json` | — | 端到端 SoC 配置：2× TrafficGenTLM + 4× ComputeUnitTLM + TCC_TLM + CrossbarTLM + MemoryTLM | gem5 apu_se.py |
| 单一 `CoherenceDomain("apu_domain", MOESI)` | (配置) | global | 跨所有 cache 的一致性域 | gem5 GPU_VIPER |

**待办**（调研 §4 Phase 5）：
- 组装完整 SoC 配置 JSON
- Python 端到端测试 `test/python/test_apu_soc.py`
- 统计 dashboard 扩展（GPU utilization, kernel completion time, TCC coalescing ratio）

### 3.7 Phase 7 备选 — dGPU（**Pending, 仅 APU 完成后评估**）

| 模块类 | Header 计划路径 | 命名空间 | 角色 | 蓝图 |
|--------|----------------|----------|------|------|
| `tlm::PCIBridgeTLM` | `include/tlm/gpu/pcie_bridge_tlm.hh` | `tlm` | 双独立 coherence 域桥接（latency 100-500 cyc） | gem5 AMDGPUDevice + PciHost |
| `HBM_TLM` | (扩展 MemoryTLM) | global | 独立 HBM2 设备内存控制器 | gem5 HBM2Stack |
| 拓扑 DSL 扩展 | (configs 扩展) | — | Disjoint Network（双独立 NoC） | gem5 Disjoint_VIPER |

**待办**：见调研 §4 Phase 2 备选（4-6 周工作量，仅 APU 完成后启动）。

### 3.8 Phase 5 / 6 现状（调研未深挖，roadmap 列出待启动）

| Phase | 计划模块 | 蓝图来源 |
|-------|---------|---------|
| Phase 5 | `ProtocolBridge` C++ module + Address translation engine + Protocol conversion logic | gem5 Bridge |
| Phase 6.1-6.4 | Multi-Cluster SoC Validation（含原 6.1 "2x CPU + GPU Cluster"，已拆分至 Phase 7） | gem5 apu_se.py |
| Phase 6.2 | Cross-cluster coherence validation | — |
| Phase 6.3 | Protocol bridge integration test | — |

### 3.9 全局待补模块（gap，调研未深挖）

| 模块类 | gem5 蓝图 | 调研章节 |
|--------|----------|---------|
| `CommMonitorTLM` | gem5 CommMonitor（流量监控/带宽/延迟/outstanding） | §2.4 |
| `SimpleMemoryTLM` | gem5 SimpleMemory（吞吐/带宽/队列模型） | §2.2 |
| `DRAMCtrlTLM` | gem5 DRAMCtrl（bank/rank/页策略） | §2.2 |
| `qos::MemCtrl` | gem5 QoS 内存调度 | §2.2 |
| `PioDeviceTLM` (abstract) | gem5 PioDevice | §2.5 |
| `DmaDeviceTLM` | gem5 DmaDevice（含 DmaPort） | §2.5 |
| `SimpleDiskTLM` | gem5 SimpleDisk | §2.5 |
| `TerminalTLM` / `UartTLM` | gem5 Terminal / Uart8250 | §2.5 |
| `EtherLinkTLM` / `EtherDeviceTLM` | gem5 EtherLink / EtherDevice | §2.5 |
| `PciDeviceTLM` (abstract) | gem5 PciDevice | §2.5 |
| `SnoopFilterTLM` | gem5 SnoopFilter | §2.4 |
| `CoherentXBarTLM` | gem5 CoherentXBar（含 snoop 广播） | §2.4 |
| `NoncoherentCacheTLM` | gem5 NoncoherentCache | §2.3 |
| 替换策略族（LRU/LFU/FIFO/RRIP） | gem5 replacement policies | §2.3 |

---

## 4. Bundle 类型映射

| Bundle | Header | 命名空间 | 用于 |
|--------|--------|----------|------|
| `CacheReqBundle` / `CacheRespBundle` | `include/bundles/cache_bundles_tlm.hh` | `bundles` | CacheTLM / MemoryTLM / CPU / TrafficGen / Crossbar / Arbiter / NICTLM (PE 侧) |
| `NoCFlitBundle` | `include/bundles/noc_bundles_tlm.hh` | `bundles` | RouterTLM / LinkTLM / NICTLM (Network 侧) — REQUEST=0/RESPONSE=1/CREDIT=2 |
| `ComputeReqBundle` / `ComputeRespBundle` | `include/bundles/compute_bundles_tlm.hh` | `bundles` | GPUTLM v0 (Phase 7.A) — 核心 8 字段 + kernel_id/workgroup_id/wavefront_id/coalescing_factor |
| `CacheReqBundleRTL` / `CacheRespBundleRTL` | `include/bundles/cache_bundles_rtl.hh` | `bundles` | HybridCacheComponent（RTL 侧） |

---

## 5. StreamAdapter 形态映射

| Adapter | Header | 用于模块 | Phase7 应用 |
|---------|--------|----------|-------------|
| `StreamAdapter<ModuleT, Req, Resp>` (单端口) | `include/framework/stream_adapter.hh` | CacheTLM/MemoryTLM/CPUTLM/TrafficGenTLM/GPUTLM | GPUTLM v0 (Phase 7.A) |
| `MultiPortStreamAdapter<ModuleT, Req, Resp, N>` | `include/framework/multi_port_stream_adapter.hh` | CrossbarTLM(4 端口) / ArbiterTLM<N> | GPU 内存控制器扇出（Phase 7.D+） |
| `BidirectionalPortAdapter<ModuleT, BundleT, N>` | `include/framework/bidirectional_port_adapter.hh` | RouterTLM(5 端口 N/E/S/W/Local) | GPU 内部 mesh（Phase 7.E） |
| `DualPortStreamAdapter<ModuleT, PE_R, PE_RR, Net_R, Net_RR>` | `include/framework/dual_port_stream_adapter.hh` | NICTLM(PE 侧 CacheBundle + Net 侧 NoCFlitBundle) | **TCC_TLM**（Phase 7.D）|

---

## 6. 阶段-模块依赖矩阵

| 阶段 | 依赖现有模块 | 依赖 Phase 7 前置 |
|------|--------------|------------------|
| **7.A**（已完成） | `ChStreamModuleBase` + `StreamAdapter` 单端口 + Bundle 体系 | (无) |
| **7.B** | 7.A 全部 | (依赖 7.A) |
| **7.C** | 7.A + `CoherenceDomain` | 依赖 7.B（CU 黑盒先于 coherence） |
| **7.D** | 7.A + 7.C + `DualPortStreamAdapter` | 依赖 7.C（coherence 是 TCC 前提） |
| **7.E** | 7.B + 7.D + `RouterTLM` / `LinkTLM` / `BidirectionalPortAdapter` | 依赖 7.D |
| **7.F** | 7.A-7.E 全部 | 依赖 7.E |
| **7 备选 dGPU** | 7.F 全部 + `PCIe` 桥接模块 | 依赖 7.F |

---

## 7. 后续整理计划（按本 README 创建者建议）

按用户指令"重新梳理 spec"的延伸，本 README 当前只完成"按模块类型梳理现有 + 待开发"任务。**未做的事项**：

1. **每个模块独立架构文档**——用户提到"对这个目录，依据模块类型，对每个模块类型都创建一个架构文档"，但本轮**仅创建了 README.md**（按用户"先创建一个 README.md"指示）。每个模块类型的独立 doc 待后续。
2. **APU 整体架构 spec**——调研报告 + spec + plan 3 份文档范围/字段有差异（spec 仅 Phase 7.A；调研报告 6 阶段），建议策略 B（中等动作）：创建 APU 整体 spec，Phase 7.A spec 降级为子 spec。
3. **R5 风险修复**——MemoryTLM 双 Bundle 注册未实施，`configs/gpu_standalone.json`实际加载可能失败（v0 测试用 AdapterHandle 绕开）。

请告知下一步整理方向（创建每个模块的独立 doc / 整理 spec / 修复 R5 / 其他）。