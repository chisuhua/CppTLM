# include/tlm/gpu/ — GPGPU 端 + dGPU 板级组件

**状态**: v1（2026-11-19 建档）· **最后更新**: 2026-11-19
**域**: SM 重构后的 GPGPU 端 + dGPU 板级/MVP 外设（28 个顶层 `.hh` + `sm/` 21 个）

## 职责

两块东西共用本目录，**不要混用**：

| 块 | 内容 | 典型入口 |
|---|---|---|
| **GPGPU 端** | SM 顶层容器 + 12 个子模块 + mesh NoC | `streaming_multiprocessor_tlm.hh` · `sm/` · `gpu_mesh_noc_tlm.hh` |
| **dGPU 板级 / 外设 MVP** | 板级整合、显存/显示设备、GMMU/SDMA、命令处理器、MSI-X | `dgpu_board_shell.hh` · `dgpu_soc.hh` · `gmmu_tlm.hh` · `sdma_engine_tlm.hh` |

## 文件索引

### SM（`sm/` 子目录，12 个 ChStream 子模块）

| 模块 | 接口头 | 实现头 |
|---|---|---|
| Fetch | —（仅实现头） | `fetch_unit_tlm.hh` |
| Decode | —（仅实现头） | `decode_unit_tlm.hh` |
| Issue | —（仅实现头） | `issue_unit_tlm.hh` |
| ScalarALU | `scalar_alu.hh` | `scalar_alu_tlm.hh` |
| VectorALU | `vector_alu.hh` | `vector_alu_tlm.hh` |
| MatrixCore | `matrix_alu.hh` | `matrix_core_tlm.hh` |
| SIMTLane | `simt_lane.hh` | `simt_lane_tlm.hh` |
| LsuGlobal | `lsu_global.hh` | `lsu_global_tlm.hh` |
| LsuLDS | `lsu_lds.hh` | `lsu_lds_tlm.hh` |
| RegFileUnit | `reg_file.hh` | `reg_file_unit_tlm.hh` |
| WritebackUnit | `writeback.hh` | `writeback_unit_tlm.hh` |
| HazardTracker | `hazard_tracker.hh` | `hazard_tracker_tlm.hh` |

⚠️ 命名**不完全统一**（`matrix_alu`↔`matrix_core_tlm`、`writeback`↔`writeback_unit_tlm`）—— 按上表查，别按规则猜。

### 板级与存储

`dgpu_board_shell.hh`（板级整合）· `dgpu_soc.hh`（SoC 组合）· `gmmu_tlm.hh`（GMMU/TLB）· `sdma_engine_tlm.hh` + `sdma_noc_adapter.hh`（SDMA）

### PCIe 外设（板级视角）

`pcie_display_device.hh` · `pcie_memory_device.hh` · `pcie_bar_router_mvp.hh` · `pcie_config_space_mvp.hh` · `msix_table_mvp.hh`

### 命令与队列 MVP

`command_processor_mvp.hh` · `pm4_decoder_mvp.hh` · `pm4_types_mvp.hh` · `dma_descriptor_mvp.hh` · `doorbell_mvp.hh` · `completion_ring_mvp.hh`

### 其他

`streaming_multiprocessor_tlm.hh`（SM 顶层容器）· `i_compute_device.hh` + `instruction_descriptor.hh`（跨仓契约）· `gpu_mesh_noc_tlm.hh` · `gpu_cluster_shared_interface.hh` · `tmu_dispatch_processor_tlm.hh`

## 本目录铁律

1. **跨仓契约不可单方面改**：`i_compute_device.hh` 的 **15 个方法**与 `instruction_descriptor.hh` 是 SM-owns-state 的**跨仓契约**（HSK-9）—— 改签名/语义须与 ArchForge 侧协商，⛔ 不要只改本仓。
2. **单一 VRAM 真源 = `DGpuBoard`**：`vram_storage_` 是唯一 backing，`PcieMemoryDevice` 是**外观层**（注入指针，不自持存储）。⛔ 不要给外设另开一份显存（ADR-DGPU-05）。
3. **`PcieEndpointTLM` 已废弃**：`pcie_endpoint_tlm.h` 打 `[[deprecated]]`，新代码一律用 `include/tlm/pcie/pcie_endpoint_ip.hh`。
4. **SM 子模块成对演进**：改接口头必须同步实现头（`.cc` 在 `src/tlm/gpu/`），否则 ChStream 端口契约漂移。
5. **测试标签**：`[sm]` / `[minimal_dgpu_soc]` / `[dgpu_soc_timing]` / `[pcie-memory]` —— 按改动面选，别只跑 `[e2e]`。

## 常见坑

- **`dgpu_board_shell` vs `dgpu_soc`**：前者是板级整合（拥有 EA/ABI 入口），后者是 SoC 内部组合 —— 加逻辑前先确认归哪个。
- **MVP 件不参与 CEP**：`dma_descriptor_mvp` / `doorbell_mvp` / `completion_ring_mvp` 属 GPGPU/板级路径，改它们不需要跑 `[cep]`。
- **`ch_uint<512>` 的低 64-bit 截断**：既有 `.read()/.write()` 走兼容 shim，新代码用 `slice<>` / `set_slice<>`。

## 关联

`src/tlm/gpu/`（19 个 `.cc`）· `include/tlm/cluster/`（多层容器）· `docs/pcie/driver-visible-minimal-soc.md`（D-AXI 实施笔记）· `docs/designs/dgpu-soc/architecture.md` · `test/AGENTS.md`（标签索引）
