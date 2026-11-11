# CppTLM — dGPU SoC Multi-IP Microarchitecture

**Version**: 3.2 (SoC + CEP Platform) · **Branch**: main · **Last verified**: 2026-11-05 @ `3f5e99ad`

## WHAT THIS IS

TLM 2.0 周期精确片上网络 (NoC) 仿真框架，目标仿真 **dGPU SoC**（由 PCIe Endpoint IP + GPGPU Cluster + Memory Cluster + NoC Interconnect + 多层 SimModule 容器等 IP 构成）。2026-2027 年完成 **7 阶段 PCIe EP 微架构**全链路交付（链路层 → 编码 → PHY 数字控制 → SR-IOV → AXI Stream Adapter → AXI4Mapper → Host Bypass + RC → 整合），**Phase 1-7 全部通过 Oracle 评审放行；Phase 8 整合交付完成**。2026-10 起承载 **CEP 平台**（Coherence Exploration Platform —— 跨协议一致性探索平台：22 份 ADR + 30 份 main spec + 25 个契约闸），同时继承既有 GPGPU 端（GPU CU / TPC / GPC / GPU Cluster 多层 + DMA/Doorbell/CommandProcessor/CompletionRing MVP）与顶层 ApuSoC 容器。

**First read** for new agents: `docs/ONBOARDING.md` (knowledge-graph-generated ramp-up)。
Architecture 必读:
- `docs/adr/ADR-CEP-00A-real-source-anchor-map.md` (CEP 真源章节映射表 —— **引用任何 CEP 章节前必查**)
- `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` (PCIe EP 整合文档, **已迁 ArchForge**, 占位 README 见 `docs/soc_arch/README.md`)
- `docs/architecture/01-hybrid-architecture-v2.1.md` (整体 NoC 架构)
- `docs/designs/dgpu-soc/multi-level-hybrid-simulation.md` (GPGPU 多层 SimModule 拓扑 —— 原 `docs/architecture/多层次混合仿真.md`, 2026-09-26 迁入 `docs/designs/`)

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
  - **✅ 配套 timing-mode SoC (v0.2 终稿, Oracle 八轮复评 PASS, 21/21 硬伤落盘, T0 已签发 ADR-DGPU-11)**：`docs/designs/dgpu-soc/timing-mode.md` + `openspec/specs/dgpu-soc-timing/` (18 ADDED Requirements + 52 scenarios) + [ADR-DGPU-11-timing-mode-soc-scope.md](docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md) (T0 已完成, 11-12d 工程量, **T1-T8 实施完成 2027-02-11, 37 cases + 0 regressions**)
  - **7 条实施铁律**：见 `docs/pcie/driver-visible-minimal-soc.md` §4
  - **3 项遗留议题**：D1 Display FB (冻结面约束, D3 收编) / doorbell 0x10010000 (测试合成偏移) / MemoryTLM capacity_gb=1 (不静默)

## STRUCTURE (verified @ 3f5e99ad, 2026-11-05)

> 本树是**导航图**，非穷举清单；逐文件清单以 `ls` 为准。标注 `★` 的是本仓主线区域。

```
include/                 # 所有 .hh 头文件（src/ 仅放 .cc, 无混用）
  core/                  # SimObject/ModuleFactory/Port/ChStream 基类 + ext/ 子目录
  tlm/                   # TLM 2.0 基础模块
    cache_tlm.hh / crossbar_tlm.hh / memory_tlm.hh / cpu_tlm.hh / router_tlm.hh
    nic_tlm.hh / link_tlm.hh / traffic_gen_tlm.hh / arbiter_tlm.hh
    coherent_xbar_tlm.hh / vram_controller_tlm.hh / vram_noc_adapter.hh / tlm_stub.hh
    cluster/           # ★ dGPU SoC 多层 SimModule 容器 (9 类 P2-P5,顶层容器 ApuSoC)
      cpu_cluster.hh        # CPU 侧: 持有 CPUTLM/CacheTLM/MemoryTLM
      compute_cluster.hh    # 单 CU 蓝图复制 (cu_template + cu_count)
      tpc_cluster.hh        # GPGPU TPC: 持有 2-8 个 ComputeCluster
      gpc_cluster.hh        # GPGPU GPC: 持有 N 个 TpcCluster
      gpu_cluster.hh        # ★ 顶层 GPGPU: 持有 GpcCluster + 共享 L2 + 显存控制器 (4 级 GPU 层次)
      cache_cluster.hh      # L1×N + L2 聚合
      memory_cluster.hh     # 多通道 HBM/DDR 控制器
      gpu_noc_cluster.hh    # GPU 端 mesh interconnect
      apu_soc.hh            # ★ 顶层 dGPU/APU SoC: CPU侧 + GPU侧 + Crossbar 互联
      gpu_internal_fabric.hh # 4-IP fabric (走 programmatic bridge, 无 ChStream 端口字段)
      soc_internal_noc.hh   # SoC 内部 DataNoC (走 programmatic bridge)
    gpu/               # ★ SM 重构 GPGPU 端 + dGPU 板级组件
      streaming_multiprocessor_tlm.hh    # ★ SM 顶层容器 (12 子模块 + IComputeDevice 15 方法)
      i_compute_device.hh / instruction_descriptor.hh    # SM-owns-state 跨仓契约 (HSK-9)
      sm/                       # ★ 12 个 ChStream SM 子模块 (Fetch/Decode/Issue/ScalarALU/
                                #   VectorALU/MatrixCore/SIMTLane/LsuGlobal/LsuLDS/RegFileUnit/
                                #   WritebackUnit/HazardTracker) —— 每模块 2 个头 (X.hh + X_tlm.hh)
      gpu_tlm.hh / shared_memory_tlm.hh / memory_cluster_tlm.hh
      dma_descriptor_mvp.hh / doorbell_mvp.hh / completion_ring_mvp.hh
      command_processor_mvp.hh / submit_queue_mvp.hh / pm4_decoder_mvp.hh
      sdma_engine_tlm.hh / sdma_noc_adapter.hh / msix_table_mvp.hh
      pcie_config_space_mvp.hh / gmmu_tlm.hh
      pcie_endpoint_tlm.h        # PcieEndpointTLM 4 端口冻结 (Phase 7.A, [[deprecated]])
      pcie_display_device.hh / pcie_memory_device.hh / pcie_bar_router_mvp.hh
      dgpu_board_shell.hh / dgpu_soc.hh / tmu_dispatch_processor_tlm.hh
    pcie/              # ★ 7 阶段 PCIe EP 微架构 (本项目主体,2026-2027)
      pcie_link_layer_tlm.hh            # Phase 1: 链路层 + DLLP + FC Token Bucket
      pcie_link_phy_mux_tlm.hh          # ★ Phase 1/2: LL+PHY+Mux composite (17 端口, EP internal_factory 持有)
      pcie_encoding_latency_model.hh   # Phase 2: 128b/130b Encoding
      pcie_phy_digital_ctrl_tlm.hh      # Phase 3: PHY 数字控制 (LTSSM 11 态)
      pcie_bypass_mux.hh                # Phase 3: 3 态模式切换 (Full/Bypass/Partial)
      pcie_sriov_vf_pool_tlm.hh         # Phase 4: SR-IOV VF Pool (17 端口 PcieEndpointIP)
      pcie_endpoint_ip.hh               # Phase 4: 整合模块 (1 PF + 16 VF); Phase 2 基类 SimModule
                                        #   (REGISTER_MODULE; composite 归 internal_factory)
      pcie_config_space_per_vf_tlm.hh   # Phase 4: per-VF Config Space
      pcie_msix_per_vf_tlm.hh           # Phase 4: per-VF MSI-X
      pcie_completion_tracker_tlm.hh    # Phase 4: NP↔CplD trans_id 关联
      pcie_ari_router_tlm.hh            # Phase 4: ARI 路由
      pcie_axi_adapter_tlm.hh           # Phase 5: AXI 事务边界 + PcieAxiAdapter
      pcie_resizable_bar.hh / pcie_tlp_codec.hh / pcie_acs_extended_cap.hh
      host_bypass_tlm.hh                # Phase 7: 软件 bring-up 跳过 RC BFM
      pcie_root_complex_tlm.hh          # Phase 7: 自研 RC 模型 (枚举 PF0-only)
      pcie_controller_ip.hh              # 4-IP 顶层 facade (4 BAR decode; attach_endpoint() 注入 EP)
      pcie_axi_bridge_ip.hh             # EP composite (非顶层; REGISTER_MODULE 已移除)
      host_pcie_test_stub.hh            # in-process 仿真测试桩 (FLR / Resizable BAR)
    cep/               # ★ CEP 平台 (Coherence Exploration Platform) —— header-only, 43 文件
      protocol_engine_base.hh / protocol_family.hh / collective_plane.hh / collective_plane_contract.hh
      fabric_collective_plane.hh / sharer_tracker.hh / ordering_point_checker.hh
      stub_cache_core.hh / address_resolver.hh / channel_dependency_graph.hh
      *_protocol_engine.hh / *_adapter.hh / *_state_spec.hh   # CHI-RNF / CXL-DCOH / MESIF / MOESI-GPU /
                                        #   NVLink-Sharp / UALink-Inc / CoHIF / TileLink
      fabric.json / collective_planes.json / perm_upgrades.json / protocol_instances.json
      # ↑ 同名 .schema.json —— 协议实例 / 权限升级的机器可读 schema
    lwc/               # LWC 总线 hub (header-only, 32-port cap)
      lwc_bus_hub.hh
  framework/           # StreamAdapter 转换层 (单/多/双端口 + 双向) + AXI4Mapper + protocol_bridge
  bundles/             # Bundle 定义: cache/noc/compute/pcie/dgpu/sm/coherence/cr/doorbell/msix + cpphdl_types
  abi/                 # 23 ABI C extern "C" 头冻结 (per ADR-088 §D5)
  ext/                 # TLM 扩展插件 (credit_stream/error_context/mem/transaction_context)
  metrics/             # histogram/stats/metrics_reporter/streaming_reporter
  rtl/                 # RTL 桥接头文件 (fragment_mapper, hybrid_cache_*)
  utils/               # config_utils/json_includer/var_resolver/wildcard/dynamic_loader/port_contract_asserts
  chstream_register.hh   # REGISTER_CHSTREAM / REGISTER_ALL 宏入口 (注册 Object + StreamAdapter)
  modules.hh             # REGISTER_OBJECT (v2.2 起 no-op) / REGISTER_MODULE 宏入口
  modules_cluster.hh     # REGISTER_MODULE 参数化入口 (9 个 SimModule 派生类集中注册)

src/                    # .cc 实现 + main.cpp
  core/                  # module_factory / connection_resolver / param_parser / plugin_loader / topology_*
  tlm/                   # router_tlm / nic_tlm / link_tlm / cache_tlm / crossbar_tlm / coherent_xbar_tlm
  tlm/cluster/           # SimModule 派生类 .cc 实现 (P2-P5 9 类 + gpu_internal_fabric / soc_internal_noc)
  tlm/pcie/              # ★ 7 阶段 PCIe EP 实现 (.cc) —— 18 个 .cc (含 Phase 8 M1 接线)
  tlm/gpu/               # dgpu_board_shell / dgpu_soc / pcie_endpoint_tlm (Phase 7.A 冻结) + SM 子模块 .cc
  framework/             # axi4_stream_adapter / axi4_mapper / axi4_bundle_to_signal / axi4_signal_to_bundle
  abi/                   # cpptlm_emulator.cc
  bundles/               # pcie_tlp_wire_bundle.cc
  rtl/                   # hybrid_cache_component / hybrid_cache_wrapper (BUILD_RTL=ON)
  utils/                 # dynamic_loader 实现
  main.cpp               # 主仿真入口

build.sh                # 独立构建入口（OFF/PTX-EMU，薄包装转 scripts/build/build.sh）
test.sh                 # 统一测试入口（默认只 test；--build 显式 build+test；--verify-fix 一键回归）

test/                   # Catch2 v3.7.0 测试套件 (349 个 test_*.cc, 全在顶层)
  catch_amalgamated.hpp  # Catch2 预编译 (非 FetchContent)
  CMakeLists.txt         # 测试构建配置 (file GLOB test_*.cc — AGENTS.md 例外)
  test_pcie_endpoint_ip_full_e2e.cc   # ★ Phase 8 全链路 E2E (solve Phase 7 M1)
  test_pcie_endpoint_*.cc              # Phase 1/4/5/6/7/8 链路测试
  test_pcie_sriov_*.cc                 # Phase 4 SR-IOV VF Pool 测试
  test_pcie_link_layer_*.cc test_pcie_phy_digital_*.cc test_pcie_bypass_*.cc  # Phase 1-3
  test_pcie_axi_adapter_*.cc test_axi4_*.cc test_axi4_mapper_*.cc             # Phase 5-6
  test_host_bypass_*.cc test_pcie_root_complex_*.cc                          # Phase 7
  test_cep_*.cc                        # ★ CEP 平台测试 (标签 [cep], 主力测试集)
  python/                # Python 侧测试 (21 个 test_*.py)
  mock_modules.hh        # 测试用 Mock 模块

configs/                 # JSON 拓扑配置 (顶层 49 个 json, 递归 60 个)
  common/ param_rules/ templates/ test/    # 共享 + 规则 + 模板 + 测试配置
  apu_soc_v1.json         # 顶层 SoC 配置 (CPU + GPU + Crossbar)
  apu_soc_full.json / apu_soc_phase7a.json / apu_soc_phase7b.json  # 阶段性
  dgpu_board_v1.json     # dGPU 板级 (23 ABI driver-visible 入口)
  dgpu_soc_minimal_v1.json  # Minimal dGPU SoC v1.8 (D-AXI 终态, functional)
  dgpu_soc_timing_v1.json  # Timing-mode dGPU SoC v0.5 (timing)
  dgpu_soc_v2_realip_p2.json  # 4-IP nested topology (Phase C 终态, pin-contract archived)
  dgpu_soc_v1.json.in.disabled  # ⚠️ Deprecated 2026-10-09 (PcieEndpointTLM deprecated)
  # ★ Phase 8 完整 dGPU SoC + PCIe EP 配置在 examples/dgpu_soc_with_pcie_ip.json (不在 configs/)

docs/
  architecture/          # 真源 1: 通用基础架构 (事务/错误/复位/拓扑/可视化/协议中立基线)
    01-hybrid-architecture-v2.1.md      # ★ 整体 NoC 架构
    06-inter-module-network.md          # ★ CEP 真源章节 (per ADR-CEP-00A §3.1)
    06-bis-coherence-as-network-extension.md  # CEP 一致性扩展真源
    07-coherence-exploration-platform.md # CEP 平台总纲(唯一入口;§4 invariant 定义处)
    cep/                                # ★ CEP 按层子文档 8 篇(实体;节号以 07 号 为准)
      stub-cache-core.md / coherence-interface.md / protocol-engine.md
      directory-ordering.md / collective-plane.md / fabric-transport.md
      simulation-and-experiment.md / core-traffic-gen.md
    02-04 / 08-13 ...                   # 其他架构决策（事务/错误/复位/指标/拓扑/相干/仪表板）
  designs/               # 真源 2: dGPU 应用架构 (Board 内部 + SoC 业务 + Driver 接口 三视角)
    README.md                           #   总入口（与 docs/architecture/ 分工说明）
    dgpu-board/architecture.md          #   Board 内部架构 (5 层 + 5 组件 + 析构协议)
    dgpu-soc/architecture.md            #   SoC 业务架构 (6 模块拓扑 + GMMU + SDMA + 存储子系统)
    dgpu-soc/timing-mode.md             #   timing-mode SoC 业务设计
    dgpu-driver/architecture.md         #   Driver 接口架构 (15 ABI + 4 BAR + driver 闭环)
  adr/                   # 真源 3: 不可变 ADR (58 份: ADR-CEP-* 22 + ADR-DGPU-* 14 + 其他 22)
    ADR-CEP-00-platform-charter-and-governance.md   # ★ CEP 治理宪章 (跨仓归口 + 准入门槛)
    ADR-CEP-00A-real-source-anchor-map.md           # ★ CEP 真源章节映射表 (ADR 引用锚点)
    # 其余 ADR-CEP-01 ~ ADR-CEP-19 / 07A 与 ADR-DGPU-01 ~ 09 见 ls docs/adr/
    ADR-DGPU-05-vram-storage-ownership.md            # D-AXI B7 单一 VRAM 所有权（P0 根因）
    ADR-DGPU-06-axi-mem-bundle-boundary.md           # D-AXI B2 chip-internal vs board-level 边界（P0）
    ADR-DGPU-07-minimal-soc-evolution-seam.md        # D-AXI §X D3-D5 演进 seam（接口零变更）
    ADR-DGPU-08-abi-freeze-policy.md                 # D-AXI 23 ABI 冻结 + 0 新增（P0）
    ADR-DGPU-09-driver-visible-minimal-soc-scope.md  # D-AXI 范围定义（4 BAR + 15 ABI + 0 新增）
  soc_arch/              # ⚠️ dGPU SoC 子项目文档已迁至独立仓（占位 + VIRTUAL_PATHS）
                        #   详见 ../ArchForge/ (新仓)；当前目录仅保留 README.md 占位说明
  cross_repo/            # 跨仓镜像文档 (HSK-9 SM 重构 / HSK-12 ABI 精简 等)
  implementation/        # 实现笔记 (CEP ArchForge 接口对齐清单 等)
  guide/                 # GETTING_STARTED / DEVELOPER / PYTOOLING / TOPOLOGY_USER
  development/           # CONTRIBUTING (pre-commit + clang-format + 测试规范)
  roadmap/               # ★ 实施路线图 + 实时状态看板 (README.md)
  superpowers/plans/     # 实施计划 + 进度跟踪归档层（与 openspec/tasks.md 互补）
  requirements/          # 需求规格 (PRD)
  research/              # 框架级调研 (CEP 一致性 + gem5 协议族 + cpptlm 融合)
  skills/                # 技能文档
  validation/            # 验证报告
  user-guide/            # 用户手册
  archive/               # 已归档文档
  legacy/                # 旧版文档
  ONBOARDING.md          # 新人上手 (图谱生成)
  README.md              # docs/ 索引
  docs_audit_report.md   # 路径漂移审计报告

examples/                # C++ example_*.cc + Python demo_*.py
  example_basic_transaction.cc / example_error_handling.cc
  example_simmodule_nested.cc          # SimModule 多层嵌套示例 (P2-P5)
  demo_pcie_full_e2e.{cc,py}           # ★ Phase 8 E2E demo (dGPU SoC + Host)
  demo_e2e_soc.py / demo_e2e_hierarchical_soc.py  # Python SoC demo
  dgpu_soc_with_pcie_ip.json            # ★ Phase 8 完整配置 (configs/ 下无此文件)
  demo_configs/                          # Python demo 配置

cpptlm/                  # Python 库 (pyproject.toml): cli/topo/config/simulation/analysis

openspec/                # 变更提案工作流 (changes/<name>/{proposal.md,specs/,tasks.md})
  changes/              # 实际 proposals (当前 1 active: T3 映射锚点普查; 122 份已归档于 changes/archive/)
  specs/                # main specs (116 份: cep-* 30 + 其他 86)

scripts/                 # 工具脚本 (详见 scripts/README.md)
  build/                # 构建 + 格式化 (build.sh / build_ptx_emu.sh / format.sh)
  test/                 # 测试 + 守门脚本 (docs_sync_check.sh + agents_md_lint.py
                        #   + 25 个 cep_*.sh 契约闸 + 15 个 cep_* probe .cc)
  topology/ pipeline/ stats/ cep/    # 拓扑 / 流水线 / 统计 / CEP 辅助

external/                # git submodule (CppHDL, json, PTX-EMU 等)
```

## WHERE TO LOOK

### ★ CEP 平台 (Coherence Exploration Platform —— 2026-10 起的当前主线)

| 任务 | 位置 |
|------|------|
| **CEP 治理宪章 + 真源锚点** | `docs/adr/ADR-CEP-00-platform-charter-and-governance.md`（跨仓归口 + 准入门槛）+ `docs/adr/ADR-CEP-00A-real-source-anchor-map.md`（**引用任何 CEP 章节前必查该表**，禁凭记忆写节号） |
| **CEP 实现层** | `include/tlm/cep/`（43 个 header-only 文件：protocol engine / adapter / state spec / JSON schema） |
| **CEP 契约守门脚本** | `scripts/test/cep_*.sh`（25 个 shell 闸：冻结面校验 / 实验运行器 / 预登记 drift / ArchForge 对齐 / 声明对齐 / 反向 fixture；另有 15 个 `cep_*_probe.cc` 编译探针） |
| **CEP 契约一致性** | `scripts/test/cep_docs_sync_check.sh`（22 份 ADR 的不变量 ↔ owner ADR 实际定义条数一致）+ `scripts/test/cep_doc_ref_check.sh`（对外文档引用节号校验） |
| **CEP 实验 4 件套** | `scripts/test/cep_experiment_run.sh`（H2/H3/H4/H6 产出与契约校验）+ `scripts/test/cep_freeze_check.sh`（产物冻结先于解读） |
| **CEP 真实 IP 对齐** | `docs/implementation/2026-10-21-cep-archforge-interface-alignment.md`（ArchForge 接口对齐清单，§6 C-4 / §7 漂移检测） |
| **CEP 开放规格** | `openspec/specs/cep-*/`（37 份 main spec）+ `openspec/changes/archive/`（132 份已归档 change） |
| **CEP 测试** | `test/test_cep_*.cc`（标签 `[cep]`，规模最大的测试集） |

### ★ PCIe EP 微架构 (2026-2027 主体)

| 任务 | 位置 |
|------|------|
| **修改 PCIe EP 数据路径** | `include/tlm/pcie/pcie_endpoint_ip.hh` + `src/tlm/pcie/pcie_endpoint_ip.cc`（Phase 8 M1 接线） |
| AXI Stream 适配 | `include/framework/axi4_stream_adapter.hh` + `src/framework/axi4_stream_adapter.cc`（Phase 5 产物） |
| AXI4Mapper OOO | `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc`（Phase 6 产物, outstanding + rid 关联） |
| AXI↔PCIe TLP 边界 | `include/tlm/pcie/pcie_axi_adapter_tlm.hh` + `src/tlm/pcie/pcie_axi_adapter_tlm.cc`（Phase 5） |
| 链路层 + FC | `include/tlm/pcie/pcie_link_layer_tlm.hh` + `src/tlm/pcie/pcie_link_layer_tlm.cc`（Phase 1） |
| PHY 数字控制 | `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh` + `src/tlm/pcie/pcie_phy_digital_ctrl_tlm.cc`（Phase 3） |
| SR-IOV VF Pool | `include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh` + `src/tlm/pcie/pcie_sriov_vf_pool_tlm.cc`（Phase 4） |
| **`PcieControllerIP` 4-IP 顶层 facade** | `include/tlm/pcie/pcie_controller_ip.hh` + `src/tlm/pcie/pcie_controller_ip.cc`（PR-A facade + PR-B B1 组合 refactor; 4 BAR decode: Bar0Mmio / Bar1FbFastPath / Bar2VramAperture / Unknown; `attach_endpoint()` 注入 EP） |
| **`PcieAxiBridgeIP` EP composite (非顶层)** | `include/tlm/pcie/pcie_axi_bridge_ip.hh` + `src/tlm/pcie/pcie_axi_bridge_ip.cc`（类保留 + `REGISTER_MODULE` REMOVED per Oracle `bg_c60653e0 §4`; JSON 顶层不可实例化） |
| **`GpuInternalFabric` 4-IP fabric** | `include/tlm/cluster/gpu_internal_fabric.hh` + `src/tlm/cluster/gpu_internal_fabric.cc`（P3 T22 scaffold; T22b pin-contract test `[fabric]` 已就位） |
| **`VramControllerTLM` 4-IP VRAM 控制器** | `include/tlm/vram_controller_tlm.hh` + `src/tlm/vram_controller_tlm.cc`（Phase 4 T4 timing-mvp 已实现; P3 promote via JSON top-level per spec §6.4; backing 注入走 `DGpuBoard::bind_memory_backings()`） |
| **D-AXI (Driver-Visible Minimal SoC)** | 实施笔记 `docs/pcie/driver-visible-minimal-soc.md`（v1.8）；三视角架构 `docs/designs/dgpu-{board,soc,driver}/architecture.md`；5 个业务 ADR `docs/adr/ADR-DGPU-05~09-*.md`；OpenSpec `openspec/specs/driver-visible-minimal-soc/`（v1.8 tasks.md T0-T5）；E2E `[minimal_dgpu_soc][driver_visible]` |
| **dGPU 文档分层（4 层模型）** | `docs/designs/README.md` 总入口 + 三视角索引；详见 §DOC HYGIENE "4 层文档模型" 小节 |
| **4-IP 边界表** | `docs/designs/dgpu-soc/architecture.md §2.4`（P3 4-IP 边界映射, RTL team 对照实现） |
| E2E 全链路 | `test/test_pcie_endpoint_ip_full_e2e.cc` + `examples/demo_pcie_full_e2e.{cc,py}` |

### ★ GPGPU / SoC 多层

| 任务 | 位置 |
|------|------|
| GPGPU 多层容器 (CPU→CU→TPC→GPC→GPU) | `include/tlm/cluster/`（cpu/compute/tpc/gpc/gpu_cluster） |
| 顶层 SoC 容器 (CPU侧 + GPU侧 + Crossbar) | `include/tlm/cluster/apu_soc.hh`（顶层,带 incorporate_parent 钩子） |
| SM 顶层容器 (12 子模块) | `include/tlm/gpu/streaming_multiprocessor_tlm.hh` + `include/tlm/gpu/sm/` |
| DMA / Doorbell / Command Processor | `include/tlm/gpu/`（dma_descriptor_mvp / doorbell_mvp / command_processor_mvp / submit_queue_mvp） |
| dGPU 板级整合 | `src/tlm/gpu/dgpu_board_shell.cc` |

### 框架基础

| 任务 | 位置 |
|------|------|
| 添加新 TLM 模块 | `include/tlm/<name>_tlm.hh` + `include/chstream_register.hh`（REGISTER_CHSTREAM） |
| 写注册宏 | `include/AGENTS.md`（宏体系表）—— `REGISTER_OBJECT` 自 v2.2 起为 no-op（CPUSim 已退场） |
| 修改模块工厂 | `src/core/module_factory.cc`（instantiateAll + Step 7 StreamAdapter 注入） |
| 修改 JSON 配置格式 | `configs/` + `include/utils/config_utils.hh`（group/connection/port_index） |
| 添加 StreamAdapter | `include/framework/{stream,multi_port_stream,dual_port_stream,bidirectional_port}_adapter.hh` |
| 7 阶段 roadmap | `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`（已迁 ArchForge）+ `openspec/changes/archive/2026-09-03-2026-09-01-cpptlm-dgpu-pcie-ip-microarch/` |
| OpenSpec 提案 | `openspec/changes/<name>/`（proposal → design → specs → tasks） |
| 调试 test fail | `.opencode/skills/cpptlm-debug/SKILL.md`（auto-loads） |
| 贡献代码/PR | `docs/development/CONTRIBUTING.md`（pre-commit + clang-format + 测试规范） |

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
# 推荐流程: 先构建再测试
./build.sh && ./test.sh

# 默认模式（auto-detect OFF/PTX-EMU，仅测试，binary 须已构建）
./test.sh

# 显式模式
./test.sh --mode off                         # OFF 路径：Python + Catch2 + TLM E2E + 可执行文件
./test.sh --mode ptx-emu                     # PTX-EMU 路径：12 项核心检查 + CuTe 链接检查 + ON 回归
./test.sh --mode both                        # 依次运行 OFF + PTX-EMU 全量

# 快速验证（跳过 E2E 和部分核心测试）
./test.sh --quick                            # OFF: Python + phase6 Catch2; PTX-EMU: 核心 6 项
./test.sh --mode ptx-emu --quick             # PTX-EMU 快速子集

# 构建 + 测试一次性
./test.sh --build                            # 等价于 ./build.sh && ./test.sh
./test.sh --build-only                       # 配置并构建，不测试

# 仅测试（需已有构建产物）
./test.sh --test-only                        # 默认行为（已无需 --test-only）
./test.sh --mode ptx-emu --test-only         # 仅 PTX-EMU 测试

# 一键回归验证（[dgpu_framebuffer] 修复 + 4 commit 7 步验证）
./test.sh --verify-fix                       # 委托 scripts/test/verify_test_seam_fix.sh

# 构建选项（根 build.sh，薄包装转 scripts/build/build.sh）
./build.sh                                   # OFF 路径增量构建
./build.sh --rebuild                         # 干净构建
./build.sh --ptx-emu                         # PTX-EMU 集成（需 git submodule init）
./build.sh --debug                           # Debug 模式
./build.sh --jobs 4                          # 自定义并行度（默认 -j2 OOM-safe）

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
- **直接 `new` 对象**: 必须经 `REGISTER_CHSTREAM` / `REGISTER_MODULE` + `ModuleFactory::instantiateAll`（`REGISTER_OBJECT` 自 v2.2 起为 no-op）
- **跳过 StreamAdapter**: `ChStreamModuleBase` 派生类必须 `set_stream_adapter()`, 禁直接动 `ChStreamPort`
- **新增 `.disabled` 测试**: `test_config_loader.cc.disabled` 等已归档；新代码禁创建
- **TODO 残留**: `// TODO::bind_ports_array` 等未完成逻辑必须在 Phase 关闭前清掉或归档
- **跳过本地 CI 验证**: 推送前必跑 `cmake --build build -j$(nproc)` + `ctest`
- **复活 legacy 层**: `include/modules/legacy/` 与 `BUILD_LEGACY_MODULES` 已于 v2.2 完全删除（per `openspec/specs/legacy-modules-removal/spec.md`）；`CPUSim` 由 `CPUTLM` 替代，新代码一律走 `include/tlm/`
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
- **路径漂移防护**: `scripts/test/docs_sync_check.sh` 扫描 4 个核心文档（`AGENTS.md` / `ONBOARDING.md` / `roadmap.md` / `scripts/README.md`）中所有反引号路径（**含 `.md`**，仅仓库根起算的路径），`--strict` 模式 pre-commit 自动执行
- **AGENTS.md 体量守门**: `scripts/test/agents_md_lint.py` 校验 3 项 —— ① STRUCTURE 目录树逐条路径存在性 ② 文件体量 ≤ 64 KiB ③ 单行 ≤ 1000 字符。**过程日志（finding 编号 / 评审轮次 / 归档流水）一律落 ADR `## Status Update` 段或 `openspec/changes/archive/`，不进 AGENTS.md** —— AGENTS.md 注入每个 agent 的上下文，体量直接等于每会话 token 成本
- **VIRTUAL_PATHS**: 文档中提及已删除/归档文件时，必须在 `docs_sync_check.sh` 的 `VIRTUAL_PATHS` 数组添加条目（而非删除段落）
- **ADR 不可变**: `docs/adr/ADR-X.*.md` 签发后不改，状态变化追加 `## Status Update` 段
- **AGENTS.md 层级**: 根 + 子目录 AGENTS.md (域内详细表，如 `include/AGENTS.md` 的注册宏体系)
- **OpenSpec Proposed 存量 ≤3 (复审 KPI, P0.5-landing 新增)**: `openspec/changes/` 下 Proposed change 数量上限 3. 超出时:
  - **季度复审**: 评估是否 archive, merge, 或明确 deprecate
  - **新提案门槛**: 现有 ≥3 时新提案需先 archive 一个现有 change
  - **范围判定 (2026-09-30 季度复审后)**: CppTLM=建模框架 + 基本 SoC 验证；SoC 架构建模在 ArchForge。任何 SoC 架构组件/拓扑/演进类提案应直接在 ArchForge openspec 创建，不进 CppTLM 计数
  - **doc-only cleanup 类 change 不计入 Proposed KPI 上限** (per `cpptlm-dgpu-soc-arch-truth-cleanup` Proposal §6 显式声明): 修改 docs + 注释 + 注解, 0 函数签名/逻辑/构建改动, 不消耗 KPI slot
  - **⏸ 两种归档语义必须区分 (2026-10-20 新增, 显式区分 per 用户裁决)**:

    | 语义 | 触发条件 | 命令 | delta spec 处理 | proposal 标记 | 语义 |
    |------|---------|------|----------------|-------------|------|
    | **✅ 已完成归档** | 实现已落地,契约已在系统内 | `openspec archive <name>` | **合并**进 `openspec/specs/` | 无需特殊标记(实现可自证) | 断言该能力**已在契约内** |
    | **⏸ 暂存归档** | 实现**未启动 / 0%**,但设计与判据仍有价值 | `openspec archive --skip-specs <name>` | **不合并**(保持 main specs 干净) | **必须**在 proposal.md 顶部加 `⏸ 暂存归档` 段 | 断言"**设计保留,能力未落地**",供以后重建 |

    - **⏸ 暂存归档的 4 项硬约束**:
      1. **必须**用 `--skip-specs` —— 默认归档会合并 delta spec,把**未实现**的能力写进 main specs,污染系统契约
      2. **必须**在 `proposal.md` 顶部加 `⏸ 暂存归档` 标记(注明日期 + tasks 完成度 + 重建路径)—— 否则 archive 目录里**无法区分**"已完成"与"未实施",重建时会误判
      3. **必须**同步所有指向该 change 的活文档引用(路径会变 `openspec/changes/<name>/` → `openspec/changes/archive/<date>-<name>/`)
      4. **不得**在暂存状态下把该能力写进任何 Stage 的 DoD / 验收条款
    - **与旧约定的关系**: `docs/archive/architecture/05-packet-semantic-network-archived-2026-10.md` §0.2bis 曾写"未实施 ⇒ 没有可以 archive 的对象"—— 该表述**仅对"✅ 已完成归档"语义成立**;⏸ 暂存归档是**第三种语义**,不与其冲突,但**必须**遵守上述 4 项硬约束
    - **先例**: 部分归档 change 未合并 spec(deprecated / doc-only / 被取代类) —— 见 `openspec/changes/archive/*/specs/` 与 `openspec/specs/` 的差集
  - **实测基线** (2026-11-10, per `openspec list --json`): **0 个 active** → **0/3**（`cpptlm-cep-bridge-reattach-guard`(R-3 装配路径关口)已归档,per 台账 Entry #320）；已归档 change **135** 份（含 `cpptlm-cep-bridge-reattach-guard`(✅ 已完成归档语义 · **带 delta**,per 台账 Entry #320)、`cpptlm-cep-bidirectional-multiport-inbound`(✅ 已完成归档语义 · 零 delta,per 台账 Entry #318)与 `cpptlm-cep-stub-cache-chstream-surface`,per 台账 Entry #305)（含 T3 映射锚点普查 `cpptlm-cep-tilelink-t3-mapping-survey`,per 台账 Entry #241 + TileLink-C 运行时引擎 `cpptlm-cep-tilelink-runtime-engine`,per 台账 Entry #247 —— 均 2026-11-06 归档,✅ 已完成归档语义）
  - **历史快照**: 2026-10-20 前 3 个 active 全部 ⏸ 暂存归档 ⇒ 0/3; 更早的 e1 v1.1 / flr-rebar v1.1 已于 2026-10-09 归档（详见 `openspec/changes/archive/2026-10-09-*`）
  - **检测命令**: `ls openspec/changes/ | grep -v archive | wc -l` (排除 archive 目录)

### 真源原则 (Single Source of Truth, 2026-10-19 确立)

**核心立场**: 文档维护有真源 (canonical) 与附属 (subsidiary) 之别。冲突时**真源优先**, 附属文档必须**对齐真源**, 不重发明。

**真源分级** (从高到低):

| 级别 | 目录 | 角色 | 修改门槛 |
|------|------|------|---------|
| **真源 1 (通用基础架构)** | `docs/architecture/` | 通用基础架构唯一真源 (TLM 框架 + 事务/错误/复位/拓扑/Metrics/可视化/协议中立基线). 任何通用基础架构变更**必须先改这里** | Oracle 评审 or ADR 触发 |
| **真源 2 (dGPU 应用架构)** | `docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/` | dGPU 应用架构唯一真源 (Board + SoC + Driver 三视角). dGPU 业务架构变更**必须先改这里** | Oracle 评审 or ADR 触发 |
| **真源 3 (不可变决策)** | `docs/adr/ADR-{X,CEP,DGPU}-*.md` | 不可变决策真源 (签发后不改, 只追加 Status Update) | 仅追加 Status Update |
| **附属 1 (调研)** | `docs/research/` | 框架级调研附属 (CEP 一致性 + gem5 协议族 + cpptlm 融合). 引用真源, 不重发明. 硬件微架构调研已于 2026-10-27 迁 ArchForge | 不进架构评审; 仅作 reference |
| **附属 2 (业务实施笔记)** | `docs/pcie/` + `docs/superpowers/` + `docs/implementation/` | 实施笔记 + 计划跟踪 (跨边界混合). 引用真源 + OpenSpec, 不重发明架构决策 | 实施期短期, archive 后归档 |
| **附属 3 (早期方案)** | `docs/archive/` + `docs/legacy/` + ADR 顶部 `## Status Update` 段 | 历史保留 (调研价值 + superseded 标注). 不重写, 仅追加归档说明或 Status Update | 仅 admin |

**冲突解决规则**:
1. **真源 vs 附属冲突** → 改附属, 保持真源 (例如: `docs/designs/dgpu-soc/architecture.md` 引用 `docs/architecture/06-inter-module-network.md`, 不在 `docs/designs/` 重述)
2. **真源 1 (通用基础) vs 真源 2 (dGPU 应用) 冲突** → 真源 1 优先 (dGPU 应用依赖通用基础, 后者变更面更窄 + 影响面更广)
3. **真源 3 (ADR) vs 真源 1/2 冲突** → ADR 不可变, 真源 1/2 顶部加 `## Status Update` 段对齐
4. **OpenSpec change vs 真源 1/2 冲突** → OpenSpec change design.md 描述**未来**, 真源代表**当前**, 实施完成后同步更新真源

> **CEP 系列的章节级锚点**: CEP 的真源章节映射在 `docs/adr/ADR-CEP-00A-real-source-anchor-map.md` —— **引用任何 CEP 章节前必查该表**, 禁凭记忆写节号 (ADR 锚点 lint 见该 ADR §5.1)。

**真源 vs 4 层文档模型**: 第 1 层 ≈ 真源 1 + 真源 2; 第 2 层 = 真源 3; 第 3 层 (OpenSpec) 优先级 < 真源; 第 4 层 (实施跟踪) 优先级最低。

### 4 层文档模型 (2026-09-26 重组)

| 层 | 职责 | 路径 | 数量控制 | 修改门槛 |
|----|------|------|---------|---------|
| **第 1 层 长期维护架构文档** | 设计意图 + 现状真相 | `docs/architecture/` (真源 1) + `docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/` (真源 2) | **每主题 = 1 主纲 + N 子文档**（子文档**仅从主纲进入**;须服务**同一主纲的同一设计意图**;**子文档不计入篇数**。真源 1 的「唯一真源」是**目录级**约束,拆子文档不违反;先例 = `docs/designs/dgpu-soc/` 与 `docs/architecture/cep/`） | Oracle 评审 or ADR 触发 |
| **第 2 层 架构决策记录** | 不可变决策 + Status Update | `docs/adr/ADR-*.md` (真源 3, 58 份) | 每个不可逆决策 1 篇 | 签发后不改，只追加 Status Update |
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

### 仿真框架 vs 应用设计 — 文档路径区分（2026-09-30 季度复审产物）

**项目愿景分层**:
- **CppTLM** = **仿真框架** + 基本 SoC 验证（本仓范围）
- **ArchForge** = SoC 架构建模（跨仓镜像 `ArchForge/docs/architecture/`，不属于本仓）

| 设计层次 | 路径 | 职责 | 典型文档 |
|---------|------|------|----------|
| **仿真框架** (TLM 基础设施) | `docs/architecture/` | 跨 SoC 复用的 TLM 框架 + 通用架构 + 拓扑/错误/复位 | `01-hybrid-architecture-v2.1.md`、`02-transaction-architecture.md`、`06-inter-module-network.md` |
| **应用设计** (基于框架的 SoC 业务) | `docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/` | dGPU 应用架构三视角：Board 内部 + SoC 业务 + Driver 接口 | `dgpu-soc/architecture.md`、`dgpu-soc/timing-mode.md`、`dgpu-board/architecture.md` |
| **设计目标声明** (🟡 DRAFT 愿景) | `docs/designs/<topic>/<name>-design-target.md` 或 `<name>-roadmap.md` | 长期愿景 / 演进路径 / 治理杠杆（未实施但定义清楚） | `dgpu-soc/soc-internal-tlm-design-target.md`、`dgpu-soc/d3-evolution-roadmap.md` |
| **实施提案** (TDD 5 步 + 评审) | `openspec/changes/<name>/` | 变更提案 | — |
| **实施跟踪** | `docs/superpowers/plans/<name>.md` | 进度报告（完成后 archive/） | — |

**新文档路径决策树**:

```
我写的文档是关于什么？
├─ TLM 框架本身（模块、基础设施、通用拓扑）
│   └─ → docs/architecture/<NN>-<topic>.md
├─ dGPU 应用设计（在框架上跑的具体业务）
│   └─ → docs/designs/{dgpu-board,dgpu-soc,dgpu-driver}/
│       ├─ 已实施的设计意图 → <topic>/architecture.md 或 <aspect>.md
│       ├─ 多 config 对比 → <topic>/data-flow-and-topology-comparison.md
│       └─ 未实施的愿景/roadmap → <topic>/<name>-design-target.md 或 -roadmap.md (🟡 DRAFT)
├─ 不可变决策记录
│   └─ → docs/adr/ADR-{X,CEP,DGPU}-<NN>-<topic>.md
├─ 实施提案（TDD 5 步 + 评审）
│   └─ → openspec/changes/<name>/{proposal,design,spec,tasks}.md
└─ 实施跟踪 + 进度报告
    └─ → docs/superpowers/plans/<name>.md（完成后 archive/）
```

**禁止**:
- ❌ 把通用 TLM 框架文档放到 `docs/designs/`（应用设计专用）
- ❌ 把 dGPU SoC 业务设计放到 `docs/architecture/`（仿真框架专用）
- ❌ 把 SoC 架构组件/拓扑/演进类文档或 OpenSpec change 放到 CppTLM 仓（应去 ArchForge）
- ❌ 把**硬件微架构调研**（专利/论文/仿真器剖析）留在 CppTLM 仓（已迁 ArchForge `docs/research/`）—— 本仓 `docs/research/` 仅留**框架级**调研（CEP + gem5 + cpptlm 融合），因其是已签发 ADR / active change 的判据定义真源

**迁移历史**: 2026-09-26 把 dGPU 应用架构三视角从 `docs/architecture/` 迁移至 `docs/designs/` 子目录；2026-10-19 显式声明真源原则；2026-10-27 硬件微架构调研迁 ArchForge。

## KEY INVARIANTS

> 本节只放**跨 change 长期成立**的事实。每 change 的实现细节 / 评审过程 / finding 编号请查对应 OpenSpec change 与 ADR `## Status Update` 段，**不要**在 AGENTS.md 复制。

- **SimModule ChStream 端口能力** (P0-revision 2026-10-12): SimModule 基类**完全支持** ChStream 端口,通过 `outputs/inputs` 字段 + `getInternalInputPort/getInternalOutputPort` API 把内部子模块的 ChStream 端口暴露到父命名空间。9 个 SimModule 派生类默认走 ChStream 端口。**特定派生类**走 callback(`set_*_target` / `accept_fn`) —— 这是**该类实现选择**,不是 SimModule 类型限制: `SocInternalNoc`(archived) / `GpuInternalFabric`; 改造为 ChStream 化需加 ChStreamPort 字段 + `set_stream_adapter()` override。详见 `include/tlm/AGENTS.md` + `include/tlm/cluster/AGENTS.md`。
- **SoC 内部 Bundle 类型分层**: `PcieTlpBundle` **只**出现在 PCIe Controller ↔ 板级(host)边界; SoC 内部 NoC 接口用 `NoCFlitBundle`(`include/bundles/noc_bundles_tlm.hh`, v2.1 决策 16 字段, 编译期 `BUNDLE_FIELD_WIDTH` 锁), 其他内部 IP 互连用 AxiBundle / AxiStreamBundle。**当前 MVP 临时方案**: `SdmaEngineTLM` 5 端口 + `VramControllerTLM` 端口 + `GpuInternalFabric` 内部 wire 全 PcieTlpBundle,**违背该分层** —— 推进路径见 `openspec/changes/archive/2026-10-19-cpptlm-dgpu-soc-physical-ip-port-boundary/`（⏸ 暂存归档的 Proposed Draft v0.5.0, **未实施**）。真源: ADR-DGPU-06 + `docs/designs/dgpu-soc/architecture.md §2.4`。
- **chip-internal AXI vs board-level PCIe 边界严格分离**: `AxiMemBundle`(新建, chip-internal) **SHALL NOT** 出现在 host↔board 端口; `PcieTlpBundle`(冻结, board-level TLP) 保持不变 —— per ADR-DGPU-06 Status Update 2026-10-10。
- **23 ABI 冻结**: `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改（仅可加 `[[deprecated]]` 属性）—— per ADR-DGPU-08 §2.3 / ADR-088 §D5。
- **PcieEndpointTLM deprecated**: `[[deprecated("use PcieEndpointIP")]]`，chstream_register 仍注册（既有 Phase 4 测试依赖），与 `PcieEndpointIP` 并存。
- **D-AXI 单一 VRAM 真源在 DGpuBoard**: `DGpuBoard::vram_storage_`(8GB `unique_ptr<uint8_t[]>`, Linux lazy commit, stable pointer) 是 5 消费者(backdoor / BAR1 fast-path / BAR2 via memory_device / MemoryTLM backing / SDMA-GMMU legacy ptr)共享的唯一 backing; `PcieMemoryDevice` 退化为 PCIe 外观层（注入指针而非自持 backing）。bound = injected `backing_size_`（非常量）; 未注入时读写返 `-ENODEV`。双 size 字段: `bar1_window_size_`(BAR1 16MB 窗口) vs `vram_size_`(VRAM 8GB 真大小)。per ADR-DGPU-05 + `docs/pcie/driver-visible-minimal-soc.md` §4。
- **VramControllerTLM functional vs timing 互斥**: `set_mode(VramControllerMode)` 切换 `FunctionalZeroDelay`(latency=0 cycles, 协议真实) vs `TimingCycleApproximate`(latency = `kOperatingFrequencyHz × bandwidth_gbps^-1` cycles); **默认** Timing 保证 backward compat。`DGpuBoard::bind_memory_backings()` 依 config `simulation_mode` 字段自动 set_mode + wire pcie_memory port0(缺省 `"functional"`, per ADR-DGPU-11 §6.4 schema)。Spec: `openspec/specs/vram-controller-functional-mode/spec.md`。
- **VramControllerTLM 1 GHz 操作频率假设**: `tlm::vram::kOperatingFrequencyHz = 1'000'000'000ULL` + `kSimCycleNs = 1`, 仅文档化锚点不参与仿真逻辑。`bandwidth_gbps_` 字段语义实为 **bytes/cycle**（历史命名, 避免破坏 ABI）⇒ 真实带宽 = `bandwidth_gbps_` GB/s; `1 sim_cycle = 1 ns @ 1 GHz`。变更须同步 `docs/designs/dgpu-soc/timing-mode.md §2.4`。
- **LWC 总线 hub 同步路由 + 32-port cap**: `include/tlm/lwc/lwc_bus_hub.hh`(header-only, 无 .cc) 同步委托 `handle_cr_transaction()`(GMMU 路径) + `handle_doorbell_transaction()`(SDMA 路径) —— **同步契约**(mmio_write 后必须立即生效, 异步 tick drain 不满足)。32-port cap 编译期 `BUNDLE_FIELD_WIDTH` 锁。`mmio_regs_` 保留作无-hub fallback 给 `[pcie]` characterization tests。Spec: `openspec/specs/lwc-bus-hub-mvp/spec.md`。
- **ch_uint 宽类型**: `ch_uint<W≤64>` 保留 `uint64_t value_` 存储（零行为变化, `is_trivially_copyable` + `is_standard_layout` 编译期断言）; `ch_uint<W>64>` 用 `std::array<uint64_t, N>` 多字存储(`ch_uint_wide`)。Wide 路径提供 `low()` / `slice<>` / `set_slice<>` 显式操作; `read()` / `write()` / `operator uint64_t()` 在 W>64 时**退化为低 64-bit 截断**（向后兼容 shim, 迁移到 slice/set_slice 后可移除）。详见 `docs/superpowers/specs/2026-10-06-dgpu-realip-refactor-design.md §5.1`。
- **PCIe Cfg 地址编码**: `PcieEndpointIP::tick()` 按 PCIe 规范解码 AXI 配置请求 —— 低 2 bit 对齐保留位, `awaddr` 直接用于 cfg/BAR 路径范围判定, cfg 内部屏蔽低 2 bit 后右移得 byte offset。测试标签 `[cfg-encoding]`。
- **framebuffer 自动分配**: `DGpuBoard::load_soc_config` 自动从 `pcie_ep.params.bar_sizes[1]` 派生 `framebuffer_size_`（顶层 `framebuffer_size_bytes` 可 override）, `init()` 自动分配 backing → driver 通过 ABI 即可闭环 SoC 全能力, 无需 `attach_framebuffer_for_testing()` 兜底。内部 `routing_enabled_*_` 成员**保留**作 ADR-DGPU-10 防劫持屏障（删 flag 会劫持 BAR0 —— `bind_display_backings` 无条件 attach display device）。
- **DGpuBoard destroy→init cycle re-entrancy**: `destroy()` Step 5 调 `eq_.reset()`; 第二轮 `load_soc_config` 入口必须重建 `eq_`（每卡独立 EQ 政策 + ADR-DGPU-02 uniform destruction）。否则 `sim_loop()` 第一行 `eq_->run()` 空指针解引用 → SIGSEGV。回归测试覆盖 cycle 1→3 轮 + 每轮 `ctrl->endpoint() != nullptr`。
- **TLM stub 默认**: `USE_SYSTEMC_STUB=ON`（根 CMakeLists.txt），无外部 SystemC 依赖
- **ccache**: 自动检测，未安装降级（非 fatal）
- **ASan**: `USE_ASAN=ON` 仅 Debug 有效（CI 矩阵排除 Release+ASan）
- **构建产物**: `build/bin/` 可执行 + `build/lib/cpptlm_core.a` 静态库
- **测试规模**: 350+ 个 `test_*.cc` / 750+ 个 Catch2 标签（2026-11-05 实测 350 / 755）。主力标签: `[cep]` / `[pcie]` / `[axi]` / `[realip]` / `[lwc]` / `[dgpu_soc_timing]` / `[minimal_dgpu_soc]` / `[pcie-memory]` / `[driver_visible]` / `[fabric]`。**实时计数用 `./build/bin/cpptlm_tests --list-tags` 取** —— 计数随新测试增长, 本行只给量级与标签入口, **不要**在此固化精确值（会静默腐化）。**0 已知 flaky**（2026-10-08 Step 0 P0 SIGSEGV 修复后）。

## PHASE STATE

> 本节只记**当前态**。历史评审过程 / finding 编号 / 归档流水见 OpenSpec `openspec/changes/archive/` 与 ADR `## Status Update` 段。

### ★ CEP 平台 (Coherence Exploration Platform) —— 当前主线

| 项 | 内容 |
|----|------|
| **治理** | `ADR-CEP-00`（跨仓归口 + 准入门槛）+ `ADR-CEP-00A`（真源章节锚点表） |
| **ADR** | 22 份 `ADR-CEP-*`（fabric / coherence / protocol / experiment / ownership 各域） |
| **实现** | `include/tlm/cep/` 43 个 header-only 文件 + `test/test_cep_*.cc` |
| **OpenSpec** | 37 份 `cep-*` main spec；**0 active** change；132 份已归档 change |
| **守门** | `scripts/test/cep_*.sh` 25 个契约闸 + 15 个 `.cc` 编译探针（冻结面 / 实验 4 件套 / 预登记 drift / ArchForge 对齐 / 声明对齐 / 反向 fixture） |
| **测试** | `[cep]` —— 规模最大的测试集 |
| **状态** | 队列 #3~#14 已全部归档（2026-10-26 ~ 2026-11-04）; 当前 1 个 active change（T3 映射锚点普查,per Entry #241） |

### ★ PCIe EP 微架构 (2026-2027, 7 阶段全部交付)

| Phase | 内容 | commits | Oracle | 状态 |
|------|------|---------|--------|------|
| Umbrella | `openspec/changes/archive/2026-09-03-2026-09-01-cpptlm-dgpu-pcie-ip-microarch/` | — | — | ✅ 引导 |
| Phase 1 | PCIe Link Layer + FC | `e9b1b..` 等 | ✅ PASS | ✅ 完成 |
| Phase 2 | 128b/130b Encoding Latency | `8d1f1d5` `b21d290` | ✅ PASS | ✅ 完成 |
| Phase 3 | PHY Digital Ctrl + Bypass Mux | `05be913`..`bac9267` | ✅ 9/10 复评 PASS | ✅ 完成 |
| Phase 4 | SR-IOV VF Pool + PcieEndpointIP | `4e9564e` + `478cdd9` `a442b65` `536dbfc` `6ebbd7d` | ✅ PASS | ✅ 完成 |
| Phase 5 | AXI Stream Adapter | `710c734`..`dd8c44a` | ✅ M1/M2/M3 修复 + 复评 PASS | ✅ 完成 |
| Phase 6 | AXI4Mapper | `8b92bfb`..`fe4d745` | ✅ PASS | ✅ 完成 |
| Phase 7 | Host Bypass + RC | `ce50b05`..`45763fa` | ✅ 有条件 PASS (M1/M2) | ✅ 完成 |
| **Phase 8** | **整合交付** | **`e29defd`..`429327d`** | **—** | **✅完成** |

**Phase 8 整合交付 (W24 末)**:
- `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`（从 umbrella design.md 迁移, **已迁 ArchForge**）
- `examples/dgpu_soc_with_pcie_ip.json` 完整 dGPU SoC + PCIe EP 配置
- `test/test_pcie_endpoint_ip_full_e2e.cc` 全链路 E2E (3 TEST_CASE, solve Phase 7 M1)
- `PcieEndpointTLM` 加 `[[deprecated]]` 标注（迁移提示, ABI 不动）
- 桥接修复 (429327d): HostBypassTLM/RC::tick() 自动转发 4 方向 AXI 通道, 让 EP 真实消费请求

**已知问题 (Minor, 不阻断)**:
- `ch_uint<512>` 既有 `.read()/.write()` 调用仍走低 64-bit 截断(向后兼容 shim,per P0 fix 2026-10-06);P1/P2 迁移到 `slice<>/set_slice<>` 后可移除 shim

### ★ D-AXI Driver-Visible Minimal SoC（跨 Phase 8 与 D-AXI change）

| Phase | 内容 | Oracle | 状态 |
|------|------|--------|------|
| v1.0 | 初版提案（5 澄清 + R1-R7 + M1-M5） | — | ✅ 引导 |
| v1.1 | P0 修订（5 澄清落点 + R1-R7 + M1-M5） | — | ✅ 完成 |
| v1.2 | Oracle 二次审查 8 must-fix (N1-N12 全部落地) | ✅ PASS | ✅ 完成 |
| **v1.4** | Metis/Oracle 二轮 + 用户质疑 + 单一 VRAM 所有权上提 (B7-B13 + 7 实施铁律 + 3 遗留议题延期) | ✅ 2 轮 PASS | ✅ **实施完成** |
| **v0.5 timing-mode (AT) SoC MVP** | Phase 4 T1-T8 完成 (MemoryTLM cycle-approximate + GmmuTLM TLB + SDMA cycle accounting + VramControllerTLM + `DGpuBoard::init_timing_mode` + `dgpu_soc_timing_v1.json` + 37 E2E cases) | ADR-DGPU-11 八轮 PASS | ✅ **实施完成** |

**v1.4 P0 修正 (B7-B13, 架构根因)**: B7 单一 VRAM backing 所有权归 DGpuBoard（8GB `unique_ptr<uint8_t[]>` default-init, Linux lazy commit, stable pointer）/ B8 bound = injected `backing_size_`（非 `kDefaultMemSize` 常量）/ B9 `handle_slave_port` 检查 `memory_read/write` 返回值（失败置 `resp.resp.write(1)` = SLVERR）/ B10 保留 `std::mutex backing_mutex_`（host/sim 并发保护）/ B11 双 size 字段拆分 `bar1_window_size_` vs `vram_size_` / B12 未注入 backing 时返 `-ENODEV` + `kRegMemSizeLo/Hi` 读 injected 值 / B13 24-case `[pcie-memory]` 机械迁移 + `[minimal_dgpu_soc]` 添加 BAR2 + 4 处 lazy alloc 行为删除。详见 `docs/pcie/driver-visible-minimal-soc.md` §4 + ADR-DGPU-05~09。

**7 条实施铁律**与**3 项遗留议题**（D1 Display FB / doorbell 0x10010000 / MemoryTLM capacity_gb=1）见 `docs/pcie/driver-visible-minimal-soc.md`。

**D3 演进 seam (v1.4 已就位, Phase 4 T4 实施完成)**：`handle_slave_port ↔ backing_ptr_` 之间已插入 VramControllerTLM (继承 MemoryTLM + 行缓冲 + bandwidth 上限)。

### ★ 已归档的 dGPU SoC 提案（不再 active）

| Change | 归档 | 状态 |
|--------|------|------|
| `cpptlm-dgpu-soc-internal-noc-e1` v1.1 | 2026-10-09 | ✅ 已归档（DataNoC only 范围缩减；详见 `openspec/changes/archive/2026-10-09-cpptlm-dgpu-soc-internal-noc-e1/`） |
| `cpptlm-pcie-host-test-flr-rebar` v1.1 | 2026-10-09 | ✅ 已归档（复用既有 `flr_pf()`/`flr_vf()` + Resizable BAR；详见 `openspec/changes/archive/2026-10-09-cpptlm-pcie-host-test-flr-rebar/`） |
| `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 | 2026-10-10 | ✅ 已归档（doc-only，19 项 finding 修复；不计入 Proposed KPI 上限） |

### ★ GPGPU / SoC 既有交付（2026 之前）

| 组件 | 内容 | 状态 |
|------|------|------|
| 9 类 SimModule (P2-P5) | CpuCluster / ComputeCluster / TpcCluster / GpcCluster / GpuCluster / CacheCluster / MemoryCluster / GpuNoC / ApuSoC | ✅ 完成 + Oracle 评审 |
| GPGPU 端 MVP | GPU CU / Warp / Vector RegFile / KernelLaunch / MeshNoC / SharedMemory / MemoryCluster | ✅ 完成 |
| SM 重构 (Task 1-14) | 12 ChStream 子模块 + 8 Bundle + IComputeDevice 15 方法; supersedes KernelLaunch/CudaCoreAdapter/PtxEmuSubmodule | ✅ 完成 |
| DMA / Doorbell / CommandProcessor / CompletionRing MVP | 端到端 SoC 集成路径 | ✅ 完成 |
| 顶层 ApuSoC | CPU侧 + GPGPU侧 + Crossbar 互联, `incorporate_parent` 钩子(借鉴 gem5 late-binding) | ✅ 完成 |
| dGPU Board Shell | `src/tlm/gpu/dgpu_board_shell.cc` 端到端板级集成 | ✅ 完成 |
| 配置文件 | `apu_soc_v1.json` / `apu_soc_full.json` / `dgpu_board_v1.json` / `dgpu_soc_minimal_v1.json` / `dgpu_soc_timing_v1.json` / `dgpu_soc_v2_realip_p2.json` | ✅ 完成 |
| 退役配置 | `configs/dgpu_soc_v1.json.in.disabled` (per 2026-10-09 季度复审, PcieEndpointTLM deprecated) | ⚠️ Deprecated |

## ARCHIVES & HISTORY

子目录级 AGENTS.md 索引（15 份）:
- `include/AGENTS.md` — 头文件总览 + 注册宏体系表（`REGISTER_OBJECT` / `REGISTER_MODULE` / `REGISTER_CHSTREAM` / `REGISTER_ALL`）
- `include/core/AGENTS.md` — 核心框架
- `include/tlm/AGENTS.md` — TLM 2.0 模块（V2.1+ 新增）
- `include/tlm/cluster/AGENTS.md` — SimModule 集群容器
- `include/bundles/AGENTS.md` — Bundle 类型定义
- `include/framework/AGENTS.md` — 流适配器层
- `include/utils/AGENTS.md` — 工具类
- `include/ext/AGENTS.md` — TLM 扩展接口
- `src/core/AGENTS.md` — 核心实现
- `src/tlm/AGENTS.md` — TLM 模块实现
- `test/AGENTS.md` — 测试套件
- `configs/AGENTS.md` — JSON 拓扑配置 Schema
- `cpptlm/AGENTS.md` — Python library for CppTLM
- `cpptlm_config/AGENTS.md` — Python 配置验证与生成工具
- `docs/research/AGENTS.md` — 框架级调研（CEP 一致性 / gem5 协议族 / cpptlm 融合）

docs-archived/: 已归档旧文档（Phase 1-3 早期决策、`samples-orphaned/`、`dead-code-headers-2026-q2/`、`v2-architecture/`、`v2-plans/`、`completed-plans-2026-q2/`、`superpowers/`/`plans/` 已清理文件）。各子目录 `README.md` 含恢复方法。

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
