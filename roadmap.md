# CppTLM Roadmap

> **Version**: 2.1
> **Last Updated**: 2026-06-14
> **Status**: ✅ v2.1 Released (2026-06-08) · 🚀 Phase 7 进行中（7.A ✅ 2026-06-11）

## 已完成里程碑 (v2.0 → v2.1)

### v2.1.0 — 2026-06-08
**架构**: 分层融合 v2.1 — CacheTLM / CrossbarTLM / MemoryTLM 端到端全链路验证  
**Build**: TLM stub 默认启用，移除 USE_SYSTEMC 依赖  
**Extension**: tlm_stub 多扩展支持（tlm_extension_registry + tlm_array + release_extension API）  
**Codebase**: 21 项 cleanup（备份/孤儿头/openspec 残留/根目录过期 notes 已清理）

---

## Phase 4: Hierarchy Core（已完成 4/6）

| 子任务 | 状态 | 实现 |
|--------|:----:|------|
| 4.1 Hierarchy Tree Parser | ✅ | `include/core/topology_node.hh`, `src/core/topology_parser.cc` |
| 4.2 CoherenceDomain C++ Module | ✅ | `include/core/coherence_domain.hh`, `src/core/coherence_domain.cc` |
| 4.3 Domain Boundary Validation | ✅ | `src/core/module_factory_validate.cc:445 validate_domain_boundary()` |
| 4.4 Snoop Routing Logic | ✅ | `CoherenceDomain::get_snoop_targets()` + snoop fanout |
| 4.5 Directory Protocol Stub | ⏳ | `CoherenceDomain::lookup_home_node()` 已实现 address mapping；独立 `Directory` 类未拆分（暂不必要） |
| 4.6 Python Hierarchy Generator | ✅ | `cpptlm/topo/layer.py` 含 `HierarchicalTopologyGenerator` |

**评估**: 4.5 的核心功能已被 `CoherenceDomain::lookup_home_node()` 覆盖，独立 `Directory` 类不是阻塞项。

---

## Phase 5: Protocol Bridge (待启动)

- 5.1 ProtocolBridge C++ module
- 5.2 Address translation engine
- 5.3 Protocol conversion logic
- 5.4 Cross-protocol validation
- 5.5 Python bridge config generator

---

## Phase 6: Multi-Cluster SoC Validation (待启动)

- 6.1 2x CPU Cluster + GPU Cluster config → **拆分至 Phase 7**（详见下方）
- 6.2 Cross-cluster coherence validation
- 6.3 Protocol bridge integration test
- 6.4 Full SoC example (4 CPU + 1 GPU) → **拆分至 Phase 7**（详见下方）

---

## Phase 7: CPU+GPGPU Fused SoC (APU-first, 进行中) 🚀

> **规划依据**: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](docs/research-cpptlm-gpu-fused-soc-survey.md)（2026-06-11 调研）
> **蓝图**: gem5 `configs/example/apu_se.py`（APU 形态，shared DDR + MOESI_AMD_Base + GPU_VIPER）
> **目标形态**: APU-first（单 RubySystem + 单 DDR 共享）；dGPU 留作 APU 完成后的 Phase 2 备选
> **总估算**: 16-22 周（Phase 7.A → 7.F）；MVP（Phase 7.A + 7.C）≈ 7-9 周
> **范围约束**: CPU 建模不在范围内（沿用 `TrafficGenTLM` / `CPUTLM`）；零新增外部依赖

### 已采纳的架构决策（D1-D5）

| 决策 | 采纳方案 | 适用阶段 |
|------|----------|----------|
| **D1 协议策略** | 分步走：Phase 7.A 用简化 I/S/M 三态 + write-through bypass；Phase 7.C 升级为 MOESI + GPU_VIPER 简化版（C++ `switch` 表，**不**复制 gem5 slicc） | 7.A → 7.C |
| **D2 CU 粒度** | 黑盒优先（只发请求，不模拟 5-stage pipeline / ISA / SIMD） | 7.A → 7.F |
| **D3 Wavefront/Coalescing** | 抽象（用 `coalescing_factor` 参数代替精确模拟） | 7.A → 7.F |
| **D4 HSAPP/CP/Dispatcher** | 简化到极致（用 `KernelLaunchTLM` ~150 行代替 HSA 三件套 3000+ 行） | 7.A → 7.F |
| **D5 目录结构** | 新建 `include/tlm/gpu/` 子目录（`compute_unit_tlm.hh` / `tcc_tlm.hh` / `kernel_launch_tlm.hh` / `pcie_bridge_tlm.hh`） | 7.A 起 |

### Phase 7 子任务（与调研报告 §4 对齐）

| 子任务 | 调研报告对应 | 状态 | 范围 | 风险 | 验收 |
|--------|:------------:|:----:|------|------|------|
| **7.A GPU 基础设施** | Phase 0（2-3 周） | ✅ Done (2026-06-11) | 新增 `include/bundles/compute_bundles_tlm.hh` + `include/tlm/gpu/gpu_tlm.hh` v0 + `REGISTER_CHSTREAM` 注册 + `configs/gpu_standalone.json` | 🟢 Low | `cpptlm_tests "[gpu]"` 通过；`gpu_standalone.json` 可执行 |
| **7.B ComputeUnit 黑盒** | Phase 1（3-4 周） | 🟡 Pending | `ComputeUnitTLM`（tick loop 发请求 + inflight_kernel_reqs_ map + workgroup_progress_）；CrossbarTLM 扩展 GPU 地址路由；CacheTLM 临时 bypass GPU 请求 | 🟡 Med-High | `configs/apu_demo_v1.json` 端到端运行；`cpptlm_tests "[gpu][phase6]"` |
| **7.C Coherence Protocol 集成** | Phase 2（4-5 周） | 🟡 Pending | CacheTLM protocol-aware 改造（`CacheLine = {data, state, sharers}` + 6×6 状态转换表）；CoherenceDomain 与 CacheTLM 集成（snoop callback + lookup_home_node） | 🔴 **High（最高风险）** | `configs/apu_demo_v2.json` 跨 cache 一致性；`cpptlm_tests "[coherence][gpu]"` |
| **7.D TCC Bridge + 内存层次** | Phase 3（3-4 周） | 🟡 Pending | `TCC_TLM`（DualPortStreamAdapter，write coalescing + snoop fan-in）；`MemoryTLM` 扩展 `hbm_mode` 参数 | 🟡 Med | `configs/apu_demo_v3.json` 写合并；`cpptlm_tests "[gpu][tcc]"` |
| **7.E Multi-CU + NoC** | Phase 4（2-3 周） | 🟡 Pending | `ComputeUnitTLM` 数组模式（`num_cus=4`）；`BidirectionalPortAdapter<N>` 连接 CU ↔ GPU Crossbar；复用 `RouterTLM` / `LinkTLM` / `NoCFlitBundle` | 🟢 Low | `configs/apu_demo_v4.json` 4 CU 并发；`cpptlm_tests "[gpu][noc]"` |
| **7.F Full APU SoC Demo** | Phase 5（2-3 周） | 🟡 Pending | `configs/apu_full_soc.json`（2× TrafficGenTLM + 4× ComputeUnitTLM + TCC + Crossbar + Memory + 单 `CoherenceDomain("apu_domain")`）；`test/python/test_apu_soc.py` 端到端验证；统计 dashboard | 🟢 Low | JSON 配置可读、trace 可观察、测试可断言 |

### Phase 7 备选 — dGPU: Disjoint Coherence + PCIe Bridge

> **触发条件**: Phase 7.A-F 全部完成后；且评估 ROI 通过
> **范围**: 双独立 `CoherenceDomain`（`cpu_domain` + `gpu_domain`）+ `PCIBridge_TLM`（DualPortStreamAdapter，latency 100-500 cycles）+ `HBM_TLM`（独立 GPU memory）+ 两组独立 NoC 拓扑
> **蓝图**: gem5 `configs/example/gpufs/Disjoint_VIPER.py` + `src/dev/amdgpu/amdgpu_device.py`
> **估算**: 4-6 周（仅在 Phase 7 全部完成后启动）

### 最高风险与缓解

- **风险**: CacheTLM protocol-aware 改造（Phase 7.C）。当前 CacheTLM 只有 `std::map<addr, data>`，无 state machine、无 snoop。
- **缓解**:
  - 不复制 gem5 slicc（5000+ 行不可读）；用 C++ `switch` 表驱动（`uint8_t state_transition[6][6]`）
  - CacheTLM 改造分两小步：先加 `CoherenceState` 标签（不做 snoop），再加 snoop callback
  - `[coherence]` 标签下每个状态转换独立单元测试
- **触发升级条件**: Phase 7.C 测试发现 CacheTLM 改造导致 5+ 测试回归 → 暂停 coherence 方案，回到 Phase 7.B 的 write-through bypass 策略

### 可 defer 内容（不影响 MVP）

- 完整 wavefront 调度（Phase 7.E+）
- LDS 建模（Phase 7.F+）
- PCIe bridge / dGPU（Phase 7 备选）
- HSA runtime / ROCm KFD（Post MVP）
- 完整 GPU_VIPER slicc state machine（永远不做）
- DMA engine（Post MVP）

---

## 当前活跃工作（2026-06-10）

### docs/cleanup boulder（已完成归档）

详细归档清单见 [`docs-archived/omo-archived/INDEX.md`](docs-archived/omo-archived/INDEX.md)：
- 18 个已完成的 boulder plans
- 4 个 notepad 子目录（tlm-stub-multi-extension-and-cleanup、dashboard-implementation、p0-p1-architecture-debt-fix-v2、phase-0-tag）
- 第一批 + 第二批共 28 个文件

### Python 包双轨制（已形式化）

- **状态**: 双包共存架构（2026-06-10 决策）
- **结构**:
  - `cpptlm_config/` （PyPI 名 `cpptlm-config`，715 行）— 核心依赖：ConfigBuilder、TopologyValidator、TopologyAdapter
  - `cpptlm/` （PyPI 名 `cpptlm`）— 用户友好 facade：cli、simulation、topo、visualization
- **依赖关系**: `pyproject.toml` (cpptlm) 显式声明 `cpptlm-config>=0.1.0` 作为依赖
- **运行时**: 通过 `sys.path.insert(0, ...)` 引用 monorepo 内 `cpptlm_config/` 目录

### 文档维护机制（已建立）

- `scripts/test/docs_sync_check.sh` — 验证 4 个核心文档中路径引用
- `.pre-commit-config.yaml` — 提交前自动运行 `--strict` 模式
- `docs/docs_audit_report.md` — 当前快照（354/354 路径有效）
- AGENTS.md §"文档维护原则" — 手动规范

### 待办（未来）

- [ ] Phase 5（ProtocolBridge）启动后回填 5.1-5.5 子任务细节
- [ ] Phase 6.2 / 6.3 启动后回填 cross-cluster coherence + bridge 集成测试细节
- [x] Phase 7.A 启动前需更新 AGENTS.md STRUCTURE 节（添加 `include/tlm/gpu/` 子目录）— 2026-06-11 已完成（AGENTS.md L19）
- [ ] Phase 7.C 启动前需更新 scripts/README.md（如新增 gpu_kernel_trace_gen.py）
- [ ] Phase 7.F 完成后把 roadmap.md 升级为 v2.2
- [ ] 考虑扩展 docs_sync_check.sh 扫描范围（Markdown 链接、include/AGENTS.md 一致性）

---

## Metadata

- **Architecture Reference**: `docs/architecture/01-hybrid-architecture-v2.1.md`
- **Implementation Plan**: `docs/implementation/11-tgms-v4-implementation-plan.md`
- **Archived Roadmap (v1.0)**: `docs-archived/omo-archived/plans/` 下的 `cpptlm-cleanup.md` 与 `architecture-debt-cleanup.md`
- **Project Overview**: [`AGENTS.md`](AGENTS.md)
