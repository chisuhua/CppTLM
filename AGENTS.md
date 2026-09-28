# CppTLM — dGPU SoC Multi-IP Microarchitecture

**Version**: 3.1 (SoC) · **Branch**: main · **Last verified**: 2027-02-09 @ `429327d`

## WHAT THIS IS

TLM 2.0 周期精确片上网络 (NoC) 仿真框架，目标仿真 **dGPU SoC**（由 PCIe Endpoint IP + GPGPU Cluster + Memory Cluster + NoC Interconnect + 多层 SimModule 容器等 IP 构成）。2026-2027 年完成 **7 阶段 PCIe EP 微架构**全链路交付（链路层 → 编码 → PHY 数字控制 → SR-IOV → AXI Stream Adapter → AXI4Mapper → Host Bypass + RC → 整合），同时继承既有 GPGPU 端（GPU CU / TPC / GPC / GPU Cluster 多层 + DMA/Doorbell/CommandProcessor/CompletionRing MVP）与顶层 ApuSoC 容器。**Phase 1-7 PCIe EP 全部通过 Oracle 评审放行；Phase 8 整合交付完成**。

**First read** for new agents: `docs/ONBOARDING.md` (knowledge-graph-generated ramp-up)。
Architecture 必读:
- `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` (PCIe EP 整合文档, **已迁 ArchForge**, 占位 README 见 `docs/soc_arch/README.md`)
- `docs/architecture/01-hybrid-architecture-v2.1.md` (整体 NoC 架构)
- `docs/architecture/多层次混合仿真.md` (GPGPU 多层 SimModule 拓扑)

D1 PCIe Device 实现笔记:
- `docs/pcie/display-device-mvp.md` (Display IO device MVP, **D1 v1.1 实施完成 2026-09-20**)
  - PcieDisplayDevice (4KB MMIO + 32MB FB + VBLANK MSI-X)
  - DGpuBoard BAR 0/1 fast-path 路由层
  - 0 个新 ABI 函数 (per ADR-088 §D5)
  - **后续**: D2 memory device MVP, D3 GMMU PoC

D-AXI (cpptlm-driver-visible-minimal-soc) 实现笔记:
- `docs/pcie/driver-visible-minimal-soc.md` (**D-AXI v1.4 完成 2026-09-26**, 跨仓镜像 ArchForge `docs/architecture/19a-driver-visible-minimal-soc.md`)
  - **第一类 driver-visible SoC MVP**：6 模块 + chip-internal AXI + 单一 VRAM 真源
  - **v1.4 架构根因**（B7-B13）：单一 backing 所有权归 DGpuBoard；PcieMemoryDevice 退化为 PCIe 外观层；`unique_ptr<uint8_t[]>` 8GB default-init (Linux lazy commit)
  - **5 消费者共享 vram_storage_**：backdoor / BAR1 fast-path / BAR2 via memory_device / MemoryTLM backing / SDMA-GMMU legacy ptr
  - **driver 视角闭环**：15 ABI + 3 BAR (BAR0 4KB MMIO + BAR1 16MB 窗口 + BAR2 8GB vram aperture, 64-bit 双 dword)
  - **0 个新 ABI 函数**（per ADR-088 §D5）
  - **冻结面零 diff**：pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh / cpptlm_emulator.h
  - **D3 seam 已预留**：`handle_slave_port ↔ backing_ptr_` 之间可插入 VramControllerTLM/MemoryClusterTLM
  - **7 条实施铁律**：见 `docs/pcie/driver-visible-minimal-soc.md` §4
  - **3 项遗留议题**：D1 Display FB (冻结面约束, D3 收编) / doorbell 0x10010000 (测试合成偏移) / MemoryTLM capacity_gb=1 (不静默)

## STRUCTURE (verified @ 429327d)

```
include/                 # 所有 .hh 头文件（src/ 仅放 .cc, 无混用）
  core/                  # SimObject/ModuleFactory/Port/ChStream 基类 + ext/ 子目录
  tlm/                   # TLM 2.0 基础模块
    cache_tlm.hh / crossbar_tlm.hh / memory_tlm.hh / cpu_tlm.hh / router_tlm.hh
    nic_tlm.hh / link_tlm.hh / traffic_gen_tlm.hh / arbiter_tlm.hh
  tlm/cluster/           # ★ dGPU SoC 多层 SimModule 容器 (9 类 P2-P5,顶层容器 ApuSoC)
    cpu_cluster.hh        # CPU 侧: 持有 CPUTLM/CacheTLM/MemoryTLM
    compute_cluster.hh    # 单 CU 蓝图复制 (cu_template + cu_count)
    tpc_cluster.hh        # GPGPU Thread Processing Cluster: 持有 2-8 个 ComputeCluster
    gpc_cluster.hh        # GPGPU General Processing Cluster: 持有 N 个 TpcCluster
    gpu_cluster.hh        # ★ 顶层 GPGPU: 持有 GpcCluster + 共享 L2 + 显存控制器 (4 级 GPU 层次)
    cache_cluster.hh      # L1×N + L2 聚合
    memory_cluster.hh     # 多通道 HBM/DDR 控制器
    gpu_noc_cluster.hh    # GPU 端 mesh interconnect
    apu_soc.hh            # ★ 顶层 dGPU/APU SoC: CPU侧 + GPGPU侧 + Crossbar 互联
  tlm/gpu/               # ★ SM 重构 GPGPU 端 (Task 4-13, 2027-02-09)
    streaming_multiprocessor_tlm.hh    # ★ SM 顶层容器 (12 子模块 + IComputeDevice 15 方法)
    i_compute_device.hh / instruction_descriptor.hh    # SM-owns-state 跨仓契约 (HSK-9)
│       │  docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md   # HSK-9 CppTLM-side mirror (Phase 3)
    sm/                       # ★ 12 个 ChStream SM 子模块 (Fetch/Decode/Issue/ScalarALU/VectorALU/MatrixCore/SIMTLane/LsuGlobal/LsuLDS/RegFileUnit/WritebackUnit/HazardTracker)
    gpu_tlm.hh / shared_memory_tlm.hh / memory_cluster_tlm.hh
    dma_descriptor_mvp.hh / doorbell_mvp.hh / completion_ring_mvp.hh
    command_processor_mvp.hh / async_completion_adapter.hh
    gpu_cluster_shared_interface.hh
    pcie_endpoint_tlm.h        # PcieEndpointTLM 4 端口冻结 (Phase 7.A, [[deprecated]])
    sdma_engine_tlm.hh / msix_table_mvp.hh / pcie_config_space_mvp.hh / gmmu_tlm.hh
    # [[deprecated]] 类 (Task 10): vector_regfile_tlm / minimal_warp_scheduler_tlm / wavefront_tlm (已删除, Task 16)
  tlm/pcie/              # ★ 7 阶段 PCIe EP 微架构 (本项目主体,2026-2027)
    pcie_link_layer_tlm.{hh,cc}                # Phase 1: 链路层 + DLLP + FC Token Bucket
    pcie_link_phy_mux_tlm.{hh,cc}              # ★ Phase 1/2: LL+PHY+Mux composite (17 端口, EP internal_factory 持有)
    pcie_encoding_latency_model.hh             # Phase 2: 128b/130b Encoding
    pcie_phy_digital_ctrl_tlm.{hh,cc}          # Phase 3: PHY 数字控制 (LTSSM 11 态)
    pcie_bypass_mux.{hh,cc}                    # Phase 3: 3 态模式切换 (Full/Bypass/Partial)
    pcie_sriov_vf_pool_tlm.{hh,cc}              # Phase 4: SR-IOV VF Pool (17 端口 PcieEndpointIP)
    pcie_endpoint_ip.{hh,cc}                   # Phase 4: 整合模块 (1 PF + 16 VF); Phase 2 基类 SimModule
                                               #   (REGISTER_MODULE; composite 归 internal_factory)
    pcie_config_space_per_vf_tlm.hh            # Phase 4: per-VF Config Space
    pcie_msix_per_vf_tlm.hh                    # Phase 4: per-VF MSI-X
    pcie_completion_tracker_tlm.hh             # Phase 4: NP↔CplD trans_id 关联
    pcie_ari_router_tlm.hh                     # Phase 4: ARI 路由
    pcie_axi_adapter_tlm.{hh,cc}               # Phase 5: AXI 事务边界 + PcieAxiAdapter
    axi4_mapper.{hh,cc}                        # Phase 6: AXI4↔Bundle Mapper + OOO rid 关联
    host_bypass_tlm.{hh,cc}                    # Phase 7: 软件 bring-up 跳过 RC BFM
    pcie_root_complex_tlm.{hh,cc}              # Phase 7: 自研 RC 模型 (枚举 PF0-only)
  framework/             # StreamAdapter 转换层 (单/多/双端口 + 双向 + AXI4StreamAdapter)
  bundles/               # Bundle 定义: cache/noc/compute/pcie_bundles + cpphdl_types + dgpu_bundles
  abi/                   # 23 ABI C extern "C" 头冻结 (per ADR-088 §D5)
  modules/legacy/        # CPUSim (BUILD_LEGACY_MODULES=OFF 默认, 已归档)
  ext/                   # TLM 扩展插件 (credit_stream/error_context/mem/transaction_context)
  metrics/               # histogram/stats/metrics_reporter/streaming_reporter
  rtl/                   # RTL 桥接头文件 (fragment_mapper, hybrid_cache_*)
  utils/                 # config_utils/json_includer/var_resolver/wildcard/dynamic_loader
  sc_core/               # SystemC 兼容层 (stub)
  chstream_register.hh   # REGISTER_CHSTREAM 宏入口 (注册 Object + StreamAdapter)
  modules.hh             # REGISTER_OBJECT / REGISTER_MODULE 宏入口
  modules_cluster.hh     # REGISTER_MODULE 参数化入口 (9 个 SimModule 派生类集中注册)
  bundles/               # Bundle 定义: cache/noc/compute/pcie_bundles + cpphdl_types + dgpu_bundles
                          # + sm_bundles_tlm.hh (Task 6: 8 种 SM 内部 Bundle)
  abi/                   # 23 ABI C extern "C" 头冻结 (per ADR-088 §D5)

src/                    # .cc 实现 + main.cpp
  core/                  # module_factory / connection_resolver / param_parser / plugin_loader
  tlm/                   # router_tlm / nic_tlm / link_tlm
  tlm/cluster/           # SimModule 派生类 .cc 实现 (P2-P5 9 类)
  tlm/pcie/              # ★ 7 阶段 PCIe EP 实现 (.cc) — Phase 8 M1 真实数据路径接线
                         #   host_bypass_tlm.cc / pcie_root_complex_tlm.cc
                         #   pcie_endpoint_ip.cc (Phase 8 M1: tick() 处理 PcieAxiAdapter slave 请求)
                         #   pcie_axi_adapter_tlm.cc
  tlm/gpu/               # dgpu_board_shell.cc / pcie_endpoint_tlm.cc (Phase 7.A 冻结)
                         # + streaming_multiprocessor_tlm.cc (Task 4 SM 顶层 stub, Task 18 完整实现)
  framework/             # axi4_stream_adapter.cc / multi_port_stream_adapter.cc
  rtl/                   # hybrid_cache_component / hybrid_cache_wrapper (BUILD_RTL=ON)
  utils/                 # dynamic_loader 实现
  main.cpp               # 主仿真入口

test.sh                 # 统一构建与测试入口（auto/off/ptx-emu/both）

test/                   # Catch2 v3.7.0 测试套件 (≥100 个 test_*.cc)
  catch_amalgamated.hpp  # Catch2 预编译 (非 FetchContent)
  CMakeLists.txt         # 测试构建配置 (file GLOB test_*.cc — AGENTS.md 例外)
  pcie/                  # ★ 7 阶段 PCIe EP 测试 (Phase 1-8 全覆盖)
  openspec/              # 变更提案工作流 (changes/<name>/proposal → design → specs → tasks)
  test_pcie_endpoint_ip_full_e2e.cc   # ★ Phase 8 全链路 E2E (solve Phase 7 M1)
  test_pcie_endpoint_*.cc              # Phase 1/4/5/6/7/8 链路测试
  test_pcie_sriov_*.cc                 # Phase 4 SR-IOV VF Pool 测试
  test_pcie_link_layer_*.cc test_pcie_phy_digital_*.cc test_pcie_bypass_*.cc  # Phase 1-3
  test_pcie_axi_adapter_*.cc test_axi4_*.cc test_axi4_mapper_*.cc             # Phase 5-6
  test_host_bypass_*.cc test_pcie_root_complex_*.cc                          # Phase 7
  mock_modules.hh         # 测试用 Mock 模块

configs/                 # JSON 拓扑配置
  common/ param_rules/ test/ examples/ templates/         # 共享 + 测试 + 示例模板
  apu_soc_v1.json         # 顶层 SoC 配置 (CPU + GPU + Crossbar)
  apu_soc_full.json / apu_soc_phase7a.json / apu_soc_phase7b.json  # 阶段性
  dgpu_board_v1.json     # dGPU 板级
  dgpu_soc_v1.json.in     # dGPU SoC 配置 (Phase 4+ 模板)
  dgpu_soc_with_pcie_ip.json  # ★ Phase 8 完整 dGPU SoC + PCIe EP 配置

docs/
  architecture/          # 通用基础架构文档（事务/错误/复位/拓扑/可视化）
    01-hybrid-architecture-v2.1.md      # ★ 整体 NoC 架构
    19-pcie-ip-microarchitecture.md     # ★ PCIe EP 微架构 (从 umbrella design.md 迁移, **已迁 ArchForge**)
    多层次混合仿真.md                   # ★ GPGPU 多层 SimModule 拓扑
    02-04 / 08-13 ...                   # 其他架构决策（事务/错误/复位/指标/拓扑/相干/仪表板）
  designs/               # ★ dGPU 应用架构 (Board 内部 + SoC 业务 + Driver 接口 三视角, 2026-09-26 重组)
    README.md                           #   总入口（与 docs/architecture/ 分工说明）
    dgpu-board/architecture.md          #   Board 内部架构 (5 层 + 5 组件 + 析构协议)
    dgpu-soc/architecture.md            #   SoC 业务架构 (6 模块拓扑 + GMMU + SDMA + 存储子系统)
    dgpu-driver/architecture.md         #   Driver 接口架构 (15 ABI + 4 BAR + driver 闭环)
  adr/                   # 通用不可变 ADR (12+ 份, 状态追加 ## Status Update 段)
    ADR-DGPU-01~04-callback-worker-replaces-detached-threads.md  # DGpuBoard v2.0 重构期基础架构 ADR
    ADR-DGPU-05-vram-storage-ownership.md                         # D-AXI B7 单一 VRAM 所有权（P0 根因）
    ADR-DGPU-06-axi-mem-bundle-boundary.md                        # D-AXI B2 chip-internal vs board-level 边界（P0）
    ADR-DGPU-07-minimal-soc-evolution-seam.md                     # D-AXI §X D3-D5 演进 seam（接口零变更）
    ADR-DGPU-08-abi-freeze-policy.md                              # D-AXI 23 ABI 冻结 + 0 新增（P0）
    ADR-DGPU-09-driver-visible-minimal-soc-scope.md               # D-AXI 范围定义（4 BAR + 15 ABI + 0 新增）
  soc_arch/              # ⚠️ dGPU SoC 子项目文档已迁至独立仓（占位 + VIRTUAL_PATHS）
                        # 详见 ../ArchForge/ (新仓)
                        # 旧文件 2026-09-20 全部迁出 (commit 4024b16e)
                        # 当前目录仅保留 README.md 占位说明文件
                        # 历史归档：曾含 24 ADR + 53 modules + 43 architecture docs
  guide/                 # GETTING_STARTED / DEVELOPER / PYTOOLING / TOPOLOGY_USER
  development/           # CONTRIBUTING (pre-commit + clang-format + 测试规范)
  roadmap/               # ★ 实施路线图 + 实时状态看板（README + current_status.md）
  superpowers/plans/     # 实施计划 + 进度跟踪归档层（与 openspec/tasks.md 互补）
  requirements/          # 需求规格
  research/              # 研究材料 + cpptlm-gpu-fused-soc-survey
  skills/                # 技能文档
  implementation/        # 实现笔记
  validation/            # 验证报告
  user-guide/            # 用户手册
  archive/               # 已归档文档
  ONBOARDING.md          # 新人上手 (图谱生成)
  README.md              # docs/ 索引
  migration-v2.2.md      # v2.2 迁移指南
  docs_audit_report.md   # 路径漂移审计报告

examples/                # C++ example_*.cc + Python demo_e2e_*.py
  example_basic_transaction.cc / example_error_handling.cc
  example_simmodule_nested.cc          # SimModule 多层嵌套示例 (P2-P5)
  demo_pcie_full_e2e.{cc,py}           # ★ Phase 8 E2E demo (dGPU SoC + Host)
  demo_e2e_soc.py / demo_e2e_hierarchical_soc.py  # Python SoC demo
  dgpu_soc_with_pcie_ip.json            # Phase 8 示例配置
  demo_configs/                          # Python demo 配置

cpptlm/                  # Python 库 (pyproject.toml): cli/topo/config/simulation/analysis

openspec/                # 变更提案工作流 (changes/<name>/{proposal.md,specs/,tasks.md})
  changes/              # 实际 proposals (见 §PHASE STATE)
  specs/                # main specs (spec.md 当前 spec)

external/                # git submodule (CppHDL, json, PTX-EMU 等)

## WHERE TO LOOK

### ★ PCIe EP 微架构 (2026-2027 主体)
| 任务 | 位置 |
|------|------|
| **修改 PCIe EP 数据路径** | `include/tlm/pcie/pcie_endpoint_ip.{hh,cc}` + `src/tlm/pcie/pcie_endpoint_ip.cc` (Phase 8 M1 接线) |
| AXI Stream 适配 | `include/framework/axi4_stream_adapter.{hh,cc}` (Phase 5 产物) |
| AXI4Mapper OOO | `include/framework/axi4_mapper.{hh,cc}` (Phase 6 产物, outstanding + rid 关联) |
| AXI↔PCIe TLP 边界 | `include/tlm/pcie/pcie_axi_adapter_tlm.{hh,cc}` (Phase 5) |
| 链路层 + FC | `include/tlm/pcie/pcie_link_layer_tlm.{hh,cc}` (Phase 1) |
| PHY 数字控制 | `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.{hh,cc}` (Phase 3) |
| SR-IOV VF Pool | `include/tlm/pcie/pcie_sriov_vf_pool_tlm.{hh,cc}` (Phase 4) |
| **D-AXI (Driver-Visible Minimal SoC)** | 实施笔记 `docs/pcie/driver-visible-minimal-soc.md` (v1.8)；三视角架构 `docs/designs/dgpu-{board,soc,driver}/architecture.md` (Board 内部 + SoC 业务 + Driver 接口)；5 个业务 ADR `docs/adr/ADR-DGPU-05~09-*.md`；OpenSpec `openspec/changes/cpptlm-driver-visible-minimal-soc/` (v1.8 tasks.md T0-T5)；E2E `[minimal_dgpu_soc][driver_visible]` |
| **dGPU 文档分层（4 层模型）** | `docs/designs/README.md` 总入口 + 三视角索引；详见 §DOC HYGIENE "4 层文档模型" 小节 |
| E2E 全链路 | `test/test_pcie_endpoint_ip_full_e2e.cc` + `examples/demo_pcie_full_e2e.{cc,py}` |

### ★ GPGPU / SoC 多层
| 任务 | 位置 |
|------|------|
| GPGPU 多层容器 (CPU→CU→TPC→GPC→GPU) | `include/tlm/cluster/` (cpu/compute/tpc/gpc/gpu_cluster) |
| 顶层 SoC 容器 (CPU侧 + GPU侧 + Crossbar) | `include/tlm/cluster/apu_soc.hh` (顶层,带 incorporate_parent 钩子) |
| GPU CU / Warp / Register File | `include/tlm/gpu/` (gpu_compute_unit_tlm / streaming_multiprocessor_tlm / sm/) |
| DMA / Doorbell / Command Processor | `include/tlm/gpu/` (dma_descriptor_mvp / doorbell_mvp / command_processor_mvp) |
| dGPU 板级整合 | `src/tlm/gpu/dgpu_board_shell.cc` |

### 框架基础
| 任务 | 位置 |
|------|------|
| 添加新 TLM 模块 | `include/tlm/<name>_tlm.hh` + `include/chstream_register.hh` (REGISTER_CHSTREAM) |
| 添加 Legacy 模块 | `include/modules/legacy/` (需 `BUILD_LEGACY_MODULES=ON`) |
| 写注册宏 | `include/AGENTS.md` (宏体系表) — 已迁移到 chstream_register.hh 注释 |
| 修改模块工厂 | `src/core/module_factory.cc` (instantiateAll + Step 7 StreamAdapter 注入) |
| 修改 JSON 配置格式 | `configs/` + `include/utils/config_utils.hh` (group/connection/port_index) |
| 添加 StreamAdapter | `include/framework/{stream,multi_port_stream,dual_port_stream,bidirectional_port}_adapter.hh` |
| 7 阶段 roadmap | `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` (主, **已迁 ArchForge**) + `openspec/changes/2026-09-01-.../roadmap.md` |
| OpenSpec 提案 | `openspec/changes/<name>/` (proposal → design → specs → tasks) |
| 调试 test fail | `.opencode/skills/cpptlm-debug/SKILL.md` (auto-loads) |
| 贡献代码/PR | `docs/development/CONTRIBUTING.md` (pre-commit + clang-format + 测试规范) |

## CONVENTIONS

- **缩进**: 4 空格 (`.clang-format: IndentWidth: 4`)；本 AGENTS.md 例外用 2 空格
- **命名**: CamelCase 类 / camelCase 函数 / snake_case 变量 / SCREAMING_SNAKE_CASE 宏
- **注释**: 中文，文件头必含功能/作者/日期
- **双注册表**: `SimObject` (object) vs `SimModule` (module), create 函数类型分离 → 见 `include/AGENTS.md`
- **ChStream 注册**: `REGISTER_CHSTREAM` 宏 (`include/chstream_register.hh`) 一次性注册 Object + StreamAdapter
- **SimModule 集中注册**: `REGISTER_MODULE(T)` (在 `modules_cluster.hh` 中集中注册 9 个 SimModule 派生类)
- **JSON 端口索引**: `"dst": "xbar.0"` 解析为 `module=xbar, port_index=0`
- **测试标签**: Catch2 `[phaseX]` 按阶段 / `[chstream]` Stream 集成 / `[pcie]` PCIe / `[axi]` AXI / `[e2e]` 全链路
- **CMake**: 显式列源 (`set(CORE_SOURCES ...)`)，禁 GLOB (test/ 例外)
- **核心库**: `cpptlm_core` (静态, `build/lib/`) + `build-on/lib/` (PTX-EMU 模式)
- **零债务原则**: Phase 完成 = 编译通过 + 测试覆盖 + 文档同步；禁 TODO 残留 / 新建 .disabled 测试

## COMMANDS

### 统一构建与测试入口

```bash
# 默认模式（auto-detect OFF/PTX-EMU，执行完整回归）
./test.sh

# 显式模式
./test.sh --mode off                         # OFF 路径：Python + Catch2 + TLM E2E + 可执行文件
./test.sh --mode ptx-emu                     # PTX-EMU 路径：12 项核心检查 + CuTe 链接检查 + ON 回归
./test.sh --mode both                        # 依次运行 OFF + PTX-EMU 全量

# 快速验证（跳过 E2E 和部分核心测试）
./test.sh --quick                            # OFF: Python + phase6 Catch2; PTX-EMU: 核心 6 项
./test.sh --mode ptx-emu --quick             # PTX-EMU 快速子集

# 仅构建/仅测试
./test.sh --build-only                       # 配置并构建，不测试
./test.sh --test-only                        # 仅测试（需已有构建产物）
./test.sh --mode ptx-emu --test-only         # 仅 PTX-EMU 测试

# 专项测试
./test.sh --python-only                      # 仅 Python（不要求构建产物）
./test.sh --e2e                              # 仅 TLM CLI E2E
./test.sh --ctest                            # 仅 ctest（替代旧 scripts/test/test.sh）

# 环境变量控制
BUILD_DIR=build-on ./test.sh --test-only    # 指定构建目录
BUILD_TYPE=Debug ./test.sh --mode off        # 指定构建类型
```

**PTX-EMU 集成模式范围**：严格限于 PTXIR image H2D DMA；**不执行** kernel/CuTe 程序。

### 执行测试回归命令

```bash
# 完整回归（推荐 CI 和本地验证）
./test.sh --mode both                        # OFF + PTX-EMU 全量（~5-10 分钟）

# OFF 路径完整回归
./test.sh --mode off                         # Python + Catch2 全量 + TLM E2E + 可执行文件

# PTX-EMU 路径完整回归
./test.sh --mode ptx-emu                     # 12 项核心 + Catch2 ON 全量 + TLM/SoC 回归

# 快速回归（本地开发）
./test.sh --quick                            # OFF: Python + phase6; PTX-EMU: 6 项核心

# 单独 Catch2 测试（按标签）
./build/bin/cpptlm_tests "[pcie]"            # 全部 PCIe EP 测试 (1500+ assertions)
./build/bin/cpptlm_tests "[axi]"             # 全部 AXI 测试
./build/bin/cpptlm_tests "[phase6]"          # Phase 6 端到端
./build/bin/cpptlm_tests "[sdma][h2d]"       # SDMA H2D 测试（含新 PTXIR 回归）
./build-on/bin/cpptlm_tests "[sdma][h2d][ptxir]"  # 仅 PTXIR H2D 测试

# CTest（CMake 注册的测试集）
ctest --test-dir build --output-on-failure -j4
ctest --test-dir build-on --output-on-failure -j4

# 专项脚本（高级用法，优先使用根 test.sh）
BUILD_DIR=build ./scripts/test/test_off.sh --quick
BUILD_DIR=build-on ./scripts/test/test_ptx_emu.sh --quick
```

### 直接构建命令

```bash
# OFF 路径（默认）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# PTX-EMU 路径（需初始化 submodule）
git submodule update --init --recursive external/PTX-EMU
cmake -S . -B build-on -DCMAKE_BUILD_TYPE=Release -DCPPTLM_WITH_PTX_EMU=ON
cmake --build build-on -j$(nproc)
```

### SoC 端到端 demo

```bash
python3 examples/demo_e2e_soc.py             # 顶层 SoC Python demo
python3 examples/demo_pcie_full_e2e.py       # PCIe EP ↔ Host demo
```

### 格式化与文档检查

```bash
# 格式化（脚本在 scripts/build/，不在 scripts/ 根目录）
./scripts/build/format.sh --check
./scripts/build/format.sh

# 文档路径同步（pre-commit 自动跑）
./scripts/test/docs_sync_check.sh --strict
cat docs/docs_audit_report.md

# 拓扑验证（CMake target）
cmake --build build --target validate_topology
```

### 调试与工作流

```bash
# 调试 test fail → auto-loads .opencode/skills/cpptlm-debug/SKILL.md

# OpenSpec 工作流
openspec status --change <name> --json
openspec instructions apply --change <name> --json
openspec validate <name>
openspec validate --changes --strict
```

## ANTI-PATTERNS

- **GLOB 源文件**: CMakeLists 用 `set(... 显式列举)`；`test/` 例外
- **直接 `new` 对象**: 必须经 `ModuleFactory::registerObject/registerModule` + `instantiateAll`
- **跳过 StreamAdapter**: `ChStreamModuleBase` 派生类必须 `set_stream_adapter()`, 禁直接动 `ChStreamPort`
- **新增 `.disabled` 测试**: `test_config_loader.cc.disabled` 等已归档；新代码禁创建
- **TODO 残留**: `// TODO::bind_ports_array` 等未完成逻辑必须在 Phase 关闭前清掉或归档
- **跳过本地 CI 验证**: 推送前必跑 `cmake --build build -j$(nproc)` + `ctest`
- **修改 legacy**: `include/modules/legacy/` 仅修严重 bug，新功能走 `include/tlm/`
- **架构性变更未走 OpenSpec**: 重大变更（影响 ≥2 模块 / 公共 API / 配置文件 schema）必须先在 `openspec/changes/` 提案
- **docs/soc_arch 直接修改**: dGPU SoC 设计文档已迁至独立 ArchForge 仓（`/workspace/project/ArchForge/`）。CppTLM 中的 `docs/soc_arch/` 仅为占位 README。设计变更 → ArchForge 仓 PR；实现变更 → CppTLM 仓 PR。`scripts/test/docs_sync_check.sh` 的 VIRTUAL_PATHS 包含 `docs/soc_arch/<sub>/` 前缀。

## DEBUGGING DISCIPLINE (P0-5b, 6 独立根因)

完整流程: `.opencode/skills/cpptlm-debug/SKILL.md`（auto-loads on "test fail" / "fix doesn't work" / "X not received"）

**4 件套验证**（任何"修复"后必跑）:
```bash
git diff --stat <file>                                     # 1. 改对位置了?
stat -c '%y %n' build/bin/cpptlm_tests src/xxx.cc          # 2. binary 时间戳新于源?
strings build/bin/cpptlm_tests | grep -c "<marker>"        # 3. 修复在 binary 里?
# 4. 修复在执行? 修复前后各加日志对比
```

**6 条铁律**:
1. 诊断用 `static FILE* diag = fopen("/tmp/cpptlm_xxx.log", "a")` + `fflush`，**不要** printf/stderr（Catch2 抑制）
2. test pass 后**立即**清理诊断代码（`grep -l "static FILE\* diag" include/ -r` 一键定位）
3. N 个独立症状 = N 个独立根因；修 1 后失败模式变了 = 还有根因，不要"逐个试修"
4. 虚函数 override 必须 `= override`（`std::size_t` vs `unsigned` 不构成 override，编译器不警告）
5. 状态设置必须在**最后一个** `reset()` 之后（`PacketPool::acquire()` 调两次 reset，修复放错位置白做）
6. "A→B→A" 响应丢失：按 6 步加 count 日志定位（A 发请求 / B 收到 / B 写 resp / B 发 resp / A 收到 / A 消费）—— count=0 即根因

## DOC HYGIENE (硬性)

- **结构调整 PR 必含**: 同步 `AGENTS.md` STRUCTURE 节 + `scripts/README.md` 子目录表 + `docs/ONBOARDING.md` §5.5 脚本表
- **路径漂移防护**: `scripts/test/docs_sync_check.sh` 扫描 4 个核心文档（`AGENTS.md` / `ONBOARDING.md` / `roadmap.md` / `scripts/README.md`）中所有反引号路径，`--strict` 模式 pre-commit 自动执行
- **VIRTUAL_PATHS**: 文档中提及已删除/归档文件时，必须在 `docs_sync_check.sh` 的 `VIRTUAL_PATHS` 数组添加条目（而非删除段落）
- **ADR 不可变**: `docs/adr/ADR-X.*.md` 签发后不改，状态变化追加 `## Status Update` 段
- **AGENTS.md 层级**: 根 + 子目录 AGENTS.md (域内详细表，如 `include/AGENTS.md` 的注册宏体系)
- **OpenSpec Proposed 存量 ≤3 (复审 KPI, P0.5-landing 新增)**: `openspec/changes/` 下 Proposed change 数量上限 3. 超出时:
  - **季度复审**: 评估是否 archive, merge, 或明确 deprecate
  - **新提案门槛**: 现有 ≥3 时新提案需先 archive 一个现有 change
  - **当前基线 (2027-09-17)**: 4 个 Proposed (cpptlm-dgpu-gmmu-mvp / mas-soc-topology-mvp / nsa-scale-up-umbrella / pcie-memory-device-mvp / pcie-ep-integration) — 已超 KPI, 需在 Phase 10+ 季度复审中处理
  - **检测命令**: `ls openspec/changes/ | grep -v archive | wc -l` (排除 archive 目录)

### 4 层文档模型 (2026-09-26 重组)

| 层 | 职责 | 路径 | 数量控制 | 修改门槛 |
|----|------|------|---------|---------|
| **第 1 层 长期维护架构文档** | 设计意图 + 现状真相 | `docs/architecture/` (通用基础) + `docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/` (dGPU 应用三视角) | 每主题 1-2 篇 | Oracle 评审 or ADR 触发 |
| **第 2 层 架构决策记录** | 不可变决策 + Status Update | `docs/adr/ADR-{X,DGPU}-*.md` | 每个不可逆决策 1 篇 | 签发后不改，只追加 Status Update |
| **第 3 层 OpenSpec change** | 实施变更提案 | `openspec/changes/<name>/{proposal,design,spec,tasks}.md` | 每个独立变更 1 个 | 标准 OpenSpec 工作流 |
| **第 4 层 实施跟踪** | TDD 5 步 + 进度报告 | `docs/superpowers/plans/<name>.md` + `archive/` | 每个大 Phase 1 个 | 实施期短期，archive 后归档 |

**dGPU 应用架构三视角** (`docs/designs/`):
- `dgpu-board/architecture.md` — Board 内部架构（5 层 + 5 组件 + 析构协议）
- `dgpu-soc/architecture.md` — SoC 业务架构（6 模块拓扑 + GMMU + SDMA + 存储子系统）
- `dgpu-driver/architecture.md` — Driver 接口架构（15 ABI + 4 BAR + driver 闭环数据流）

**dGPU 关键 ADR** (`docs/adr/ADR-DGPU-05~09-*.md`):
- `ADR-DGPU-05` — 单一 VRAM 所有权归 DGpuBoard（D-AXI v1.4 B7 根因）
- `ADR-DGPU-06` — AxiMemBundle vs PcieTlpBundle 边界严格分离（v1.3 B2）
- `ADR-DGPU-07` — Minimal → 完整 GPU 演进 seam（D3-D5 接口零变更）
- `ADR-DGPU-08` — 23 ABI 冻结 + 0 新增约束（v1.4 ADR-088 §D5）
- `ADR-DGPU-09` — Driver-Visible Minimal SoC 范围定义（4 BAR + 15 ABI + 0 新增）

**同步规则（单向，OpenSpec → 架构文档）**：
- 修改 OpenSpec change 的 design.md 时，若涉及设计意图变化，应同步更新 `docs/designs/<topic>/architecture.md` 顶部 `## 关联 OpenSpec changes` 段
- 架构文档代表长期设计意图（why）；OpenSpec design.md 代表实施指导（how）
- 反向同步不强制（架构文档可以领先于实施）

## KEY INVARIANTS

- **TLM stub 默认**: `USE_SYSTEMC_STUB=ON`（根 CMakeLists.txt），无外部 SystemC 依赖
- **ccache**: 自动检测，未安装降级（非 fatal）
- **ASan**: `USE_ASAN=ON` 仅 Debug 有效（CI 矩阵排除 Release+ASan）
- **构建产物**: `build/bin/` 可执行 + `build/lib/cpptlm_core.a` 静态库
- **测试状态**: **66951 assertions 全绿**（含 Phase 1-8 + 7 阶段全链路 + dgpu_soc_minimal_v1 E2E `[minimal_dgpu_soc]` 41 assertions + ABI smoke `[abi][minimal_dgpu_soc]` 28 assertions + D2 memory `[pcie-memory]` 67 assertions / 1587 cases）；**openspec validate PASS**
- **23 ABI 冻结**: `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改（仅可加 `[[deprecated]]` 属性）
- **PcieEndpointTLM deprecated**: `[[deprecated("use PcieEndpointIP")]]`，chstream_register 仍注册（既有 Phase 4 测试依赖），新增 PcieEndpointIP 并存
- **512-bit 数据限制**: `ch_uint<512>` 内部 `uint64_t`，`wdata/rdata` 真实宽度 64-bit（per `include/bundles/cpphdl_types.hh`）
- **PCIe Cfg 地址编码** (v1.1 实现完成): `PcieEndpointIP::tick()` 按 PCIe 规范解码 AXI 配置请求 — 低 2 bit 对齐保留位，`awaddr` 直接用于 cfg/BAR 路径范围判定，cfg 内部屏蔽低 2 bit 后右移得到 byte offset；测试覆盖见 `[cfg-encoding]` 标签。
- **framebuffer 自动分配** (P0.5-landing, 2027-09-17): `DGpuBoard::load_soc_config` 自动从 `pcie_ep.params.bar_sizes[1]` 派生 `framebuffer_size_`（顶层 `framebuffer_size_bytes` 可 override），`init()` 自动分配 backing → **UE 端 driver 通过 ABI 即可闭环 SoC 全能力**，无需 `attach_framebuffer_for_testing()` 兜底。测试覆盖：`[abi][minimal_dgpu_soc]` 8 cases / 23 assertions + Python demo `examples/demo_dgpu_soc_minimal_via_abi.py`。
- **memory_routing_enabled_ 路由开关** (D2 v1.1 修订, 2027-09-26): DGpuBoard 新增 `memory_routing_enabled_` flag 防止无条件路由劫持 root cause 4（与 D1 `display_routing_enabled_` 对称）。默认 `false`，仅当 JSON 顶层 `memory_routing_enabled=true` 时启用 BAR 2 → PcieMemoryDevice 路由。测试覆盖：`[pcie-memory]` 24 cases / 67 assertions（4 文件双标签 `[pcie][pcie-memory]`）。
- **D-AXI 单一 VRAM 真源在 DGpuBoard** (cpptlm-driver-visible-minimal-soc v1.4, 2027-09-26): `DGpuBoard::vram_storage_` (8GB, `std::unique_ptr<uint8_t[]>` default-init, Linux lazy commit, stable pointer) 是 5 消费者 (backdoor / BAR1 fast-path / BAR2 via memory_device / MemoryTLM backing / SDMA-GMMU legacy ptr) 共享的唯一 backing；`PcieMemoryDevice::memory_backing_` 已被 v1.4 B7 删除，改注入指针 (`set_backing_store(ptr, size)` 对齐 `MemoryTLM v2.2` 模式)。bound = injected `backing_size_`（非 `kDefaultMemSize` 常量）。dual size 字段：`bar1_window_size_` (BAR1 16MB 窗口) vs `vram_size_` (VRAM 8GB 真大小)。测试覆盖：`[minimal_dgpu_soc][driver_visible]` E2E + `[pcie-memory]` 24 cases (v1.4 B13 机械迁移后)。
- **chip-internal AXI vs board-level PCIe 边界严格分离** (D-AXI v1.4): `AxiMemBundle` (新建, 4KB inline payload, chip-internal) **SHALL NOT** 出现在 host↔board 端口；`PcieTlpBundle` (冻结, board-level TLP) 保持不变。SDMA 5 端口混合 wire-format：mem_in/mem_out 切 AxiMemBundle (chip-internal), desc_in/done_out/host_out 保持 PcieTlpBundle (minimal_v1 不实际接线)。

## PHASE STATE

### ★ PCIe EP 微架构 (2026-2027,7 阶段全部交付)

| Phase | 内容 | commits | Oracle | 状态 |
|------|------|---------|--------|------|
| Umbrella | openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/ | — | — | ✅ 引导 |
| Phase 1 | PCIe Link Layer + FC | `e9b1b..` 等 | ✅ PASS | ✅ 完成 |
| Phase 2 | 128b/130b Encoding Latency | `8d1f1d5` `b21d290` | ✅ PASS | ✅ 完成 |
| Phase 3 | PHY Digital Ctrl + Bypass Mux | `05be913`..`bac9267` | ✅ 9/10 复评 PASS | ✅ 完成 |
| Phase 4 | SR-IOV VF Pool + PcieEndpointIP | `4e9564e` + `478cdd9` `a442b65` `536dbfc` `6ebbd7d` | ✅ PASS | ✅ 完成 |
| Phase 5 | AXI Stream Adapter | `710c734`..`dd8c44a` | ✅ M1/M2/M3 修复 + 复评 PASS | ✅ 完成 |
| Phase 6 | AXI4Mapper | `8b92bfb`..`fe4d745` | ✅ PASS | ✅ 完成 |
| Phase 7 | Host Bypass + RC | `ce50b05`..`45763fa` | ✅ 有条件 PASS (M1/M2) | ✅ 完成 |
| **Phase 8** | **整合交付** | **`e29defd`..`429327d`** | **—** | **✅完成** |

**Phase 8 整合交付 (W24 末)**：
- `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` (从 umbrella design.md 迁移, **已迁 ArchForge**)
- `examples/dgpu_soc_with_pcie_ip.json` 完整 dGPU SoC + PCIe EP 配置
- `test/test_pcie_endpoint_ip_full_e2e.cc` 全链路 E2E (3 TEST_CASE, solve Phase 7 M1)
- `PcieEndpointTLM` 加 `[[deprecated]]` 标注（迁移提示, ABI 不动）
- 桥接修复 (429327d): HostBypassTLM/RC::tick() 自动转发 4 方向 AXI 通道, 让 EP 真实消费请求

**已知问题 (Minor, 不阻断)**:
- `ch_uint<512>` 实际 64-bit 存储 (per Phase 5 M1 文档化限制)

### ★ D-AXI Driver-Visible Minimal SoC（2026-2027, 跨 Phase 8 与 D-AXI change）

| Phase | 内容 | commits | Oracle | 状态 |
|------|------|---------|--------|------|
| v1.0 | 初版提案（5 澄清 + R1-R7 + M1-M5） | — | — | ✅ 引导 |
| v1.1 | P0 修订（5 澄清落点 + R1-R7 + M1-M5） | — | — | ✅ 完成 |
| v1.2 | Oracle 二次审查 8 must-fix (N1-N12 全部落地) | — | ✅ PASS | ✅ 完成 |
| **v1.4** | **Metis/Oracle 二轮 + 用户质疑 + 单一 VRAM 所有权上提** (B7-B13 + 7 实施铁律 + 3 遗留议题延期) | — | **✅ 2 轮 PASS** | **🔄 实施中** |

**v1.4 P0 修正（B7-B13, 架构根因）**：
- **B7**: 单一 VRAM backing 所有权归 DGpuBoard（`vram_storage_` = 8GB `unique_ptr<uint8_t[]>` default-init, Linux lazy commit, stable pointer）
- **B8**: `memory_read/write` bound = injected `backing_size_`（非 `kDefaultMemSize` 常量）
- **B9**: `handle_slave_port` 检查 `memory_read/write` 返回值；失败置 `resp.resp.write(1)` (SLVERR)
- **B10**: 保留 `std::mutex backing_mutex_`（host/sim 并发保护；lazy 部分随所有权上提消失）
- **B11**: 双 size 字段拆分 `bar1_window_size_` (BAR1 窗口 = `bar_sizes[1]`) vs `vram_size_` (VRAM = `bar_sizes[2]`)
- **B12**: 未注入 backing 时 `memory_read/write` 返 `-ENODEV`（spec 新增错误码）；`kRegMemSizeLo/Hi` 读 injected `backing_size_`（非常量）
- **B13**: 24-case `[pcie-memory]` 机械迁移 + `[minimal_dgpu_soc]` 添加 BAR2 + 4 处 lazy alloc 行为删除

**D3 演进 seam (v1.4 已就位)**：`handle_slave_port ↔ backing_ptr_` 之间可插入真 VramControllerTLM/MemoryClusterTLM（接口零变更）。

### ★ GPGPU / SoC 既有交付（2026 之前，跨 Phase 1-7 与本项目同期）

| 组件 | 内容 | 状态 |
|------|------|------|
| 9 类 SimModule (P2-P5) | CpuCluster / ComputeCluster / TpcCluster / GpcCluster / GpuCluster / CacheCluster / MemoryCluster / GpuNoC / ApuSoC | ✅ 完成 + Oracle 评审 |
| GPGPU 端 MVP | GPU CU / Warp / Vector RegFile / KernelLaunch / MeshNoC / SharedMemory / MemoryCluster | ✅ 完成 |
| SM 重构 (Task 1-14) | 12 ChStream 子模块 + 8 Bundle + IComputeDevice 15 方法; supersedes KernelLaunch/CudaCoreAdapter/PtxEmuSubmodule | ✅ 完成 |
| DMA / Doorbell / CommandProcessor / CompletionRing MVP | 端到端 SoC 集成路径 | ✅ 完成 |
| 顶层 ApuSoC | CPU侧 + GPGPU侧 + Crossbar 互联, `incorporate_parent` 钩子(借鉴 gem5 late-binding) | ✅ 完成 |
| dGPU Board Shell | `src/tlm/gpu/dgpu_board_shell.cc` 端到端板级集成 | ✅ 完成 |
| 配置文件 | `apu_soc_v1.json` / `apu_soc_full.json` / `dgpu_soc_v1.json.in` / `dgpu_soc_with_pcie_ip.json` | ✅ 完成 |

## ARCHIVES & HISTORY

子目录级 AGENTS.md 索引:
- `include/AGENTS.md` — 注册宏体系完整表（`REGISTER_OBJECT` / `REGISTER_MODULE` / `REGISTER_CHSTREAM` / `REGISTER_ALL`）
- `include/tlm/AGENTS.md` — TLM 模块列表与基类
- `include/core/AGENTS.md` — 核心框架
- `src/core/AGENTS.md` — 核心实现
- `src/tlm/AGENTS.md` — TLM 实现
- `test/AGENTS.md` — 测试套件
- `configs/AGENTS.md` — 配置 Schema
- `cpptlm_config/AGENTS.md` — Python配置

docs-archived/: 已归档旧文档（Phase 1-3 早期决策、`samples-orphaned/`、`dead-code-headers-2026-q2/`、`v1-architecture/`、`v2-architecture/`、`p0-p1-architecture-debt-fix-v2/`、`superpowers/`/`plans/` 已清理文件）。各子目录 `README.md` 含恢复方法。

## MCP TOOLS (code-review-graph)

`.mcp.json` 已配置 `code-review-graph` server。**优先于** Grep/Glob/Read（图谱更快、更便宜，给出调用链/测试覆盖等结构上下文）。

| 场景 | 工具 |
|------|------|
| 探索代码 | `semantic_search_nodes` / `query_graph` |
| 影响分析 | `get_impact_radius` / `get_affected_flows` |
| 代码审查 | `detect_changes` + `get_review_context` |
| 架构视图 | `get_architecture_overview` + `list_communities` |
| 重构 | `refactor_tool`（rename / dead_code / suggest） |

Graph 通过 hooks auto-update。Fallback to Grep only when graph doesn't cover。
