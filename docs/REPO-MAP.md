# REPO-MAP — CppTLM 逐文件结构清单（详版）

> **分工**：本文件 = **逐文件清单**（详版，随结构调整同步）；[`../AGENTS.md`](../AGENTS.md) §STRUCTURE = **主线导航**（简版，只留 ★ 主线与一级目录）。
> **维护**：结构调整 PR 须同时更新两处（详版增删条目、简版改主线指针）；`scripts/test/agents_md_lint.py` 只校验 `AGENTS.md` 的简版树路径存在性。
> **标注**：`★` = 当前主线区域；`⚠️` = 已废弃 / 已迁出 / 有坑。

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
    cep/               # ★ CEP 平台 (Coherence Exploration Platform) —— 43 个 .hh + 9 个 JSON/schema 声明源(共 52 文件)
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

test/                   # Catch2 v3.7.0 测试套件 (370 个 test_*.cc, 全在顶层)
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

configs/                 # JSON 拓扑配置 (顶层 55 个 json, 递归 66 个)
  common/ param_rules/ templates/ test/    # 共享 + 规则 + 模板 + 测试配置
  apu_soc_v1.json         # 顶层 SoC 配置 (CPU + GPU + Crossbar)
  apu_soc_full.json / apu_soc_phase7b.json  # 阶段性
  # ⚠️ apu_soc_phase7a.json / gpu_standalone.json / templates/gpu_soc/gpu_soc_gb203_v1.json
  #    已于 2026-11-18 删除（设计类型归档 per change cpptlm-config-type-registration-align T7）
  dgpu_board_v1.json     # dGPU 板级 (23 ABI driver-visible 入口)
  dgpu_soc_minimal_v1.json  # Minimal dGPU SoC v1.8 (D-AXI 终态, functional)
  dgpu_soc_timing_v1.json  # Timing-mode dGPU SoC v0.5 (timing)
  dgpu_soc_v2_realip_p2.json  # 4-IP nested topology (Phase C 终态, pin-contract archived)
  dgpu_soc_v1.json.in.disabled  # ⚠️ Deprecated 2026-10-09 (PcieEndpointTLM deprecated)
  # ★ Phase 8 完整 dGPU SoC + PCIe EP 配置在 examples/dgpu_soc_with_pcie_ip.json (不在 configs/)

docs/
  architecture/          # 真源 1: 通用基础架构 (事务/错误/复位/拓扑/可视化/协议中立基线)
    01-hybrid-architecture-v2.1.md      # ★ 整体 NoC 架构
    06-inter-module-network.md          # 协议中立基线(通用真源 1;CEP 的**引用源**映射见 ADR-CEP-00A §3.1)
    06-bis-coherence-as-network-extension.md  # CEP 一致性扩展真源
    07-coherence-exploration-platform.md # CEP 平台总纲(唯一入口;§4 invariant 定义处)
    cep/                                # ★ CEP 子文档 9 篇(实体;节号以 07 号 为准)
      parity-baseline.md                #   H7 对拍基准规格(独立验证手段面;**不在 §N 体系内**)
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
  adr/                   # 真源 3: 不可变 ADR (63 份: ADR-CEP-* 24 + ADR-DGPU-* 15 + ADR-X* 18 + 其他 6)
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
  pcie/                  # PCIe 设备实施笔记 (D1 Display / D-AXI)
  implementation/        # 实施笔记 + CEP 契约载体（完结件已归档至 docs/archive/implementation/）
  guide/                 # GETTING_STARTED / DEVELOPER / PYTOOLING / TOPOLOGY_USER / CEP
  development/           # CONTRIBUTING (pre-commit + clang-format + 测试规范)
  roadmap/               # ⚠️ 已归档的重定向存根（当前路线图真源 = .rddf/roadmap.md）
  superpowers/           # 第 4 层实施跟踪（plans/ 计划 + specs/ 规格；完结件归档至 docs/archive/tracking/）
  requirements/          # 需求规格 (PRD)
  research/              # 框架级调研 (CEP 一致性 + gem5 协议族 + cpptlm 融合)
  validation/            # 验证报告 (CEP 实验 + 拓扑 oracle-pass + raw/)
  user-guide/            # 用户手册 (Python E2E 建模模板 / 层次化 SoC 配置)
  archive/               # 已归档文档（architecture/ designs/ implementation/ validation/ tracking/ process/ roadmap/ skills/）
  ONBOARDING.md          # 新人上手 (图谱生成)
  README.md              # docs/ 总索引（按真源分级组织；计数以实测为准）
  docs_audit_report.md   # 路径漂移审计状态（活状态 + 重生成命令）

examples/                # C++ example_*.cc + Python demo_*.py
  example_basic_transaction.cc / example_error_handling.cc
  example_simmodule_nested.cc          # SimModule 多层嵌套示例 (P2-P5)
  demo_pcie_full_e2e.{cc,py}           # ★ Phase 8 E2E demo (dGPU SoC + Host)
  demo_e2e_soc.py / demo_e2e_hierarchical_soc.py  # Python SoC demo
  dgpu_soc_with_pcie_ip.json            # ★ Phase 8 完整配置 —— 硬件建模面示例 (规格归 ArchForge)
  dgpu_soc_pcie_ep_framework_min.json   # EP 最小框架验证示例 (只含 axi_adapter + bar_sizes)
  demo_configs/                          # Python demo 配置

cpptlm/                  # Python 库 (pyproject.toml): cli/topo/config/simulation/analysis

openspec/                # 变更提案工作流 (changes/<name>/{proposal.md,specs/,tasks.md})
  changes/              # 实际 proposals (当前 0 active; 162 份已归档于 changes/archive/ —— 2026-11-19 实测 `ls -d openspec/changes/archive/*/ | wc -l`)
  specs/                # main specs (137 份: cep-* 46 + 其他 91 —— 2026-11-18 实测)

scripts/                 # 工具脚本 (详见 scripts/README.md)
  build/                # 构建 + 格式化 (build.sh / build_ptx_emu.sh / format.sh)
  test/                 # 测试 + 守门脚本 (docs_sync_check.sh + agents_md_lint.py
                        #   + cep_*.sh 契约闸（数量以 `ls scripts/test/cep_*.sh` 为准）+ 17 个 cep_* probe .cc)
  topology/ pipeline/ stats/ cep/    # 拓扑 / 流水线 / 统计 / CEP 辅助

external/                # git submodule (CppHDL, json, PTX-EMU 等)
```
