# dgpu-soc-control-hub Specification

## Purpose
TBD - created by archiving change cpptlm-dgpu-soc-control-hub. Update Purpose after archive.
## Requirements
### Requirement: dgpu-soc-control-hub-module

**ControlHub (Chub) 模块 SHALL 新建**, 集中承担 4 BAR decode + MSI-X 聚合 + Error 聚合, 替代当前 LwcBusHub + PcieControllerIP 分裂承担的职责。

**位置**: `include/tlm/cluster/control_hub.{hh,cc}` + `src/tlm/cluster/control_hub.cc`

**基类**: `SimModule` (ChStream 化, per `include/core/sim_module.hh:132-161` 既有 SimModule ChStream 化模式)

**6 类端口** (per design.md §2.1):

| 端口 | 类型 | 方向 | Bundle | 对接目标 |
|------|------|------|--------|---------|
| `axi_slave_in` | AXI slave | 接收 | `Axi4Bundle` | `pcie_ep.axi_master_out` |
| `cfg_slave_in` | AXI4-Lite slave | 接收 | `Axi4LiteBundle` | `pcie_ep.cfg_master_out` |
| `cr_master_out[]` | CrBundle master | 发送 | `CrBundle` | `lwc_hub` → `gmmu.cr_in` |
| `doorbell_out` | DoorbellBundle master | 发送 | `DoorbellBundle` | `lwc_hub` → `sdma.doorbell_in` |
| `noc_master_out` | NoC master | 发送 | `NoCFlitBundle` | `soc_noc.0` (BAR2 data path) |
| `msix_in` | MsixBundle slave | 接收 | `MsixBundle` | `lwc_hub` → `msix_generator` (Phase 2) |

**4 BAR decode 路由** (per design.md §2.3):
- BAR0 (4KB MMIO) → `cr_master_out` → `lwc_hub` → `gmmu.cr_in`
- BAR1 (16MB window) → `doorbell_out` → `lwc_hub` → `sdma.doorbell_in`
- BAR2 (8GB vram aperture) → `noc_master_out` → `soc_noc` → `vram_ctrl`
- BAR3 (隐含, per ADR-DGPU-09) → 预留

**内部 3 类转换 adapter** (per design.md §2.4):
- `Chub_Axi4_To_CrAdapter` (Axi4 → CrBundle, BAR0)
- `Chub_Axi4_To_DoorbellAdapter` (Axi4 → DoorbellBundle, BAR1)
- `Chub_Axi4_To_NocAdapter` (Axi4 → NoCFlitBundle, BAR2)

#### Scenario: Chub 4 BAR decode

- **WHEN** Chub `axi_slave_in` 收到 Axi4Bundle 写入请求
- **THEN** Chub 调用 `decode_bar(bar, offset, out_sub_offset)` 路由 4 BAR
- **AND** BAR0 → `route_bar0_cr(req)` → 内部 Axi4 → CrBundle 转换 → `cr_master_out[0]`
- **AND** BAR1 → `route_bar1_doorbell(req)` → 内部 Axi4 → DoorbellBundle 转换 → `doorbell_out`
- **AND** BAR2 → `route_bar2_vram(req)` → 内部 Axi4 → NoCFlitBundle 转换 → `noc_master_out`
- **AND** BAR3 → 返回 Unknown (预留)

#### Scenario: Chub ChStream 化

- **WHEN** Chub 模块实例化并注入 StreamAdapter
- **THEN** 类内有 6 类 ChStreamPort 字段 (axi_slave_in + cfg_slave_in + cr_master_out[] + doorbell_out + noc_master_out + msix_in)
- **AND** `set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override` 实现
- **AND** `unsigned num_ports() const override { return NUM_PORTS; }` (NUM_PORTS = 6)
- **AND** JSON `connections[]` 可写 `{ "src": "pcie_ep.axi_master_out", "dst": "chub.axi_slave_in" }` 等
- **AND** 0 处 programmatic bridge (per 提议 6 完全 JSON 化)

#### Scenario: Chub 注册

- **WHEN** `chstream_register.hh` 注册 ControlHub
- **THEN** `REGISTER_CHSTREAM(ControlHub)` 一次性注册 Object + StreamAdapter
- **AND** `modules_cluster.hh` 加 `REGISTER_MODULE(ControlHub)`
- **AND** JSON `"type": "ControlHub"` 可被 ModuleFactory 解析

---

### Requirement: dgpu-soc-sdma-translate-port

**SdmaEngineTLM SHALL 新增独立 translate_req/resp 端口**, 替代 programmatic `set_translate_cb` 桥接。

**端口数**: 5 → 8 (新增 axi_master_out + translate_req/resp + doorbell_in, 删除 desc_in/host_out/done_out)

**TranslateReqBundle 字段** (per design.md §3.2):
- `ch_uint<64> iova` — 输入虚拟地址
- `ch_uint<32> size` — 翻译长度
- `ch_uint<8>  access_type` — 读/写/原子
- `ch_uint<8>  task_id` — SDMA 任务 ID
- `ch_uint<16> request_id` — 请求 ID (OOO 匹配)

**TranslateRespBundle 字段**:
- `ch_uint<64> paddr` — 物理地址
- `ch_uint<8>  status` — OK=0 / PERMISSION_FAULT=1 / PAGE_FAULT=2
- `ch_uint<8>  task_id` — 回写
- `ch_uint<16> request_id` — 回写

#### Scenario: SDMA translate 直连 (替代 programmatic bridge)

- **WHEN** `sdma.process_h2d()` 收到 DMA descriptor (axi_master_out 端口)
- **THEN** SDMA 调用 `translate_req_out` 发 `TranslateReqBundle`
- **AND** JSON `sdma.translate_req → gmmu.translate_in` 直连走通 (无 programmatic bridge)
- **AND** GMMU 翻译完成后 `gmmu.translate_resp → sdma.translate_resp_in` 收回 `TranslateRespBundle`
- **AND** SDMA 用 paddr 继续 `mem_out` 发 `NoCFlitBundle` VRAM 写
- **AND** 旧版 `set_translate_cb` / `set_translate_target` programmatic API **已删除** (标记 `[[deprecated]]`)

#### Scenario: TranslateBundle 编译期字段宽度锁

- **WHEN** `TranslateReqBundle` / `TranslateRespBundle` 实例化
- **THEN** 编译期 `BUNDLE_FIELD_WIDTH` assert 锁字段宽度 (per `include/utils/port_contract_asserts.hh`)
- **AND** 任何字段宽度改动触发编译错误

---

### Requirement: dgpu-soc-pcie-memory-device-removal

**PcieMemoryDevice 模块 SHALL 完全移除**, driver BAR2 路径改走 Chub→NoC→VramController, 与 SDMA bulk DMA 路径完全统一。

**移除的 2 件事** (per design.md §6.1):
1. BAR2 8GB vram aperture 外观层 → 改走 Chub BAR2 decode → NoC → VramController
2. SDMA bulk DMA 委托 VramController functional port → 改走 SDMA mem_out → NoC → VramController (JSON 直连)

**测试迁移**:
- `[pcie-memory]` 17 cases → 迁移到 `[pcie][bar2]` 8 + `[soc_noc][bar2]` 4 + `[chub][bar2]` 5

**23 ABI 函数 0 diff** (per ADR-DGPU-08 §2.3):
- `cpptlm_emulator_dma_read/write` 入口不变
- BAR2 路径内部从 PcieMemoryDevice 委托 → Chub/NoC 委托, driver 视角行为完全一致

#### Scenario: driver BAR2 写 vram 路径

- **WHEN** driver 调用 `cpptlm_emulator_dma_write(iova, data)`
- **THEN** `DGpuBoard::mmio_write(bar=2, offset, data)` 触发
- **AND** Chub `axi_slave_in` 接收 Axi4Bundle 写入
- **AND** Chub `decode_bar(2, offset)` → `BarTarget::Bar2VramAperture`
- **AND** Chub `route_bar2_vram(req)` 内部 Axi4 → NoCFlitBundle 转换
- **AND** Chub `noc_master_out` 发 NoCFlitBundle
- **AND** JSON `chub.noc_master_out → soc_noc.0` 直连 (StreamAdapter: Chub_Axi4_To_NocAdapter)
- **AND** SocInternalNoc mesh 路由 → `soc_noc.3`
- **AND** JSON `soc_noc.3 → vram_ctrl.req_in` 直连 (StreamAdapter: VramCtrl_Noc_To_AxiAdapter)
- **AND** VramController `handle_functional_request()` → `vram_storage_` (8GB, DGpuBoard 持有)

#### Scenario: SDMA bulk DMA 与 driver BAR2 完全统一

- **WHEN** SDMA H2D bulk DMA 触发
- **THEN** `sdma.mem_out` (NoCFlitBundle) → JSON `sdma.2 → soc_noc.1` 直连
- **AND** SocInternalNoc mesh 路由 → `soc_noc.3` → VramController
- **AND** **SDMA 路径与 driver BAR2 路径经同一 NoC + VramController 通道** (per 提议 5 简化)

---

### Requirement: dgpu-soc-full-json-topology

**`configs/dgpu_soc_minimal_v1.json` 顶层 `soc.connections[]` SHALL 从 1 条扩展到 13 条**, 完全 JSON 声明, 0 处 programmatic bridge。

**13 条 connection** (per design.md §7.1):

| # | 分类 | 源端 | 目标端 | Bundle |
|---|------|------|-------|--------|
| 1 | PCIe ↔ Chub AXI master | pcie_ep.axi_master_out | chub.axi_slave_in | Axi4Bundle |
| 2 | PCIe ↔ Chub AXI slave | chub.axi_master_out | pcie_ep.axi_slave_in | Axi4Bundle |
| 3 | PCIe ↔ Chub cfg | pcie_ep.cfg_master_out | chub.cfg_slave_in | Axi4LiteBundle |
| 4 | Chub → LWC CR | chub.cr_master_out | lwc_hub.0 | CrBundle |
| 5 | Chub → LWC Doorbell | chub.doorbell_out | lwc_hub.1 | DoorbellBundle |
| 6 | LWC → GMMU CR | lwc_hub.cr_target_gmmu | gmmu.cr_in | CrBundle |
| 7 | LWC → SDMA Doorbell | lwc_hub.doorbell_target_sdma | sdma.doorbell_in | DoorbellBundle |
| 8 | Chub → SoC NoC (BAR2) | chub.noc_master_out | soc_noc.0 | NoCFlitBundle |
| 9 | SDMA → SoC NoC | sdma.mem_out | soc_noc.1 | NoCFlitBundle |
| 10 | GMMU → SoC NoC (PTE fetch) | gmmu.req_out | soc_noc.2 | NoCFlitBundle |
| 11 | SoC NoC → VramController | soc_noc.3 | vram_ctrl.req_in | NoCFlitBundle |
| 12 | SDMA → GMMU translate req | sdma.translate_req | gmmu.translate_in | TranslateReqBundle |
| 13 | GMMU → SDMA translate resp | gmmu.translate_resp | sdma.translate_resp_in | TranslateRespBundle |

#### Scenario: JSON 拓扑完整性

- **WHEN** `ModuleFactory::instantiateAll()` 加载 `dgpu_soc_minimal_v1.json`
- **THEN** 13 条 `connections[]` 全部解析成功
- **AND** 每条 connection 触发 `ChStreamAdapterFactory::createAdapter()` 自动选 adapter
- **AND** 13 个 StreamAdapter 注入到对应模块 (per `set_stream_adapter()` override)
- **AND** 0 处 programmatic bridge (per 提议 6 完全 JSON 化)
- **AND** 与 `dgpu_board_shell.cc::bind_memory_backings()` 装配结果**一致** (cross-validation)

#### Scenario: programmatic bridge 全部退役

- **WHEN** `DGpuBoard::bind_memory_backings()` 完成本 change 装配
- **THEN** 取消 4 个 `soc_noc->set_*_target()` 调用
- **AND** 取消 `fabric->set_vram_target()` + `vram_ctrl->set_fabric_source()` 调用
- **AND** 取消 `gmmu_->set_vram_target()` 调用
- **AND** 取消 `pcie_mem->set_vram_controller()` 调用 (PcieMemoryDevice 移除)
- **AND** 取消 `sdma->set_translate_cb()` / `set_translate_target()` 调用 (TranslateBundle 直连)
- **AND** 保留 `vram_ctrl->set_backing_view()` (backdoor 必需)
- **AND** 保留 `lwc_hub->register_*_target()` (LWC 总线 programmatic 保留, 本 change 不强求)
- **AND** `bind_memory_backings()` 从 ~30 行简化到 ~3 行

#### Scenario: 0 回归锁

- **WHEN** 实施期所有 module 端口切型 + adapter 注入 + ChStream 化 + Chub 新建 + PcieMemoryDevice 移除完成
- **THEN** 0 回归 (per Phase 0+1 实测基线):
  - `[pcie]` 430/36672 0 失败
  - `[gmmu]` 25/5273 0 失败
  - `[noc]` 64/304 0 失败
  - `[sdma]` 68/12945 0 失败
  - `[minimal_dgpu_soc]` 24→32/170→220 (新增 8 E2E 直连 cases) 0 失败
  - `[dgpu_soc_timing]` 37/12024 0 失败 (timing-mode 不动)
  - `[driver_visible]` 4/40 0 失败
  - `[pcie-memory]` 删除 (迁移到 [pcie][bar2] + [soc_noc][bar2] + [chub][bar2])
  - `[pcie-ep-soc-bridge]` 8 cases 0 失败 (per p2-integration-unblock)
- **AND** 新增 ≥ 52 cases (Chub 6 + Translate 5 + SDMA 3 + GMMU 3 + BAR2 4 + 4 adapter + 4 misc + 8 端口 + 4 实施)
- **AND** frozen headers 0 diff + 23 ABI 0 变更 (per ADR-DGPU-08)
- **AND** `docs_sync_check.sh --strict` exit 0

---

### Requirement: dgpu-soc-physical-ip-boundary-deferred

本 change SHALL 明确锁定以下 11 项 **NOT in scope** 项 (per proposal.md "What Changes" 末 + design.md §11):

- **timing-mode** (`dgpu_soc_timing_v1.json`): SHALL 不动 (per H-C6 + ADR-DGPU-11, 范围锁定 minimal_v1 functional-mode, timing-mode 走 dgpu_soc_timing_v1 独立 SoC 拓扑, 与本 change 正交)
- **23 ABI 函数** (`include/abi/cpptlm_emulator.h`): 零 diff (frozen)

- **23 ABI 函数** (`include/abi/cpptlm_emulator.h`): 零 diff (frozen)
- **4 frozen headers** (`pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` / `cpptlm_emulator.h`): 零 diff
- **PcieEndpointIP 内部 17 端口 PcieTlpBundle composite**: 保留 (frozen by Phase 4 桥接)
- **PcieAxiAdapter 3 端口** (axi_master_out + axi_slave_in + cfg_slave_in): 保留 (per PcieEndpointIP 内部)
- **PCIe Controller ↔ host 边界**: 保留 PcieTlpBundle (board-level)
- **timing-mode** (`dgpu_soc_timing_v1.json`): 不动 (per H-C6)
- **LWC 总线语义** (`CrBundle` / `DoorbellBundle` / `MsixBundle`): 不动 (per Change B 既有)
- **Change E (MSI-X Generator IP)**: 独立 change, 本 change 准备接口 (Chub.msix_in)
- **Change F (Error/CDC Bus)**: 独立 change, 本 change 准备接口 (Chub.err_out)
- **LwcBusHub programmatic `register_*_target`**: 保留 (本 change 不强求 LWC 总线 JSON 化)
- **PcieControllerIP facade 角色**: 保留 (4 BAR decode 验证 facade)

#### Scenario: deferred scope 严格隔离

- **WHEN** 实施期本 change 触及 deferred scope
- **THEN** 自动触发 `frozen headers 0 diff` 检查失败
- **AND** `git diff include/abi/cpptlm_emulator.h` 必须为 0 行
- **AND** `git diff include/tlm/gpu/pcie_endpoint_tlm.h` 必须为 0 行
- **AND** `git diff include/bundles/pcie_bundles_tlm.hh` 必须为 0 行
- **AND** timing-mode 测试 `[dgpu_soc_timing]` 37/12024 必须 0 失败

---

