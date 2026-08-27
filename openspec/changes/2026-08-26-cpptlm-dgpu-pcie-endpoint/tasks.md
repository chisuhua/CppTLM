# cpptlm-dgpu-pcie-endpoint: Tasks

> **配套**: [`proposal.md`](./proposal.md) · [`design.md`](./design.md)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D2

---

### T-pe-1: PCIe Bundle 定义

- [x] 新建 `include/bundles/pcie_bundles_tlm.hh`：`PcieTlpBundle` + `MsiXDeliveryBundle`（per design §2）
- [x] N/A：pcie_bundles_tlm.hh 是纯头文件，CMake 无头文件登记约定（由 include path 自动可见）
- [x] **Commit**: `feat(bundles): add PCIe TLP/MSI-X bundles for dGPU endpoint`

### T-pe-2: PcieConfigSpace + PcieBarRouter + MsiXTable 内部块

- [x] 新建 `include/tlm/gpu/pcie_config_space_mvp.hh` + `src/tlm/gpu/pcie_config_space_mvp.cc`（参数化 256B/4KB + capability chain JSON 声明）
- [x] 新建 `include/tlm/gpu/pcie_bar_router_mvp.hh` + `.cc`（寄存器表数据化，`side_effect` 声明式；门铃 strong-order 250-700ns 沿用 s2 语义）
- [x] 新建 `include/tlm/gpu/msix_table_mvp.hh` + `.cc`（vector 数参数化，默认 16）
- [x] `src/tlm/gpu/pcie_config_space_mvp.cc` + `src/tlm/gpu/pcie_bar_router_mvp.cc` + `src/tlm/gpu/msix_table_mvp.cc` 加入 `src/CMakeLists.txt` 的 `CORE_SOURCES` 显式列表（项目约定禁 GLOB）
- [x] **Commit**: `feat(pcie-endpoint): config space + BAR router + MSI-X table internals`

### T-pe-3: PcieEndpointTLM 组件（4 端口）

- [x] 新建 `include/tlm/gpu/pcie_endpoint_tlm.hh` + `src/tlm/gpu/pcie_endpoint_tlm.cc`（ChStreamModuleBase，`num_ports()=4`，多端口 `set_stream_adapter(adapters[])`）
- [x] `include/chstream_register.hh` 注册 `REGISTER_CHSTREAM(PcieEndpointTLM)` + multi-port adapter
- [x] `src/tlm/gpu/pcie_endpoint_tlm.cc` 加入 `src/CMakeLists.txt` 的 `CORE_SOURCES` 显式列表（项目约定禁 GLOB）
- [x] 4 个新测试文件（`test_pcie_endpoint_config_space.cc` / `_bar_routing.cc` / `_msix.cc` / `_from_config.cc`）加入 `test/CMakeLists.txt` 的 ctest 注册列表
- [x] **Commit**: `feat(pcie-endpoint): PcieEndpointTLM 4-port component + registration`

### T-pe-4: 单元测试（4 文件）

- [x] `test/test_pcie_endpoint_config_space.cc`（PE-G2）
- [x] `test/test_pcie_endpoint_bar_routing.cc`（PE-G3，含 `0x0014` 门铃表驱动验证，断言无 if-else 硬编码路径；**新增断言**：`GPU_REG_DOORBELL` MMIO_WRITE 在 [250ns, 700ns] 区间内驱动 `mmio_out` 门铃事务发出——沿用 s2 strong-order 语义（per design §3 + spec Scenario "Doorbell register write is table-driven"），可通过 quantum 数 × cycle 时间间接测量）
- [x] `test/test_pcie_endpoint_msix.cc`（PE-G4）
- [x] `test/test_pcie_endpoint_from_config.cc`（PE-G5：JSON 实例化 + adapter 注入）
- [x] **Commit**: `test(pcie-endpoint): config/BAR/MSI-X/from-config coverage`

### T-pe-5: 集成验证

- [x] `configs/dgpu_soc_v1.json.in` 加入 `pcie_ep` 节点示例（cmake `configure_file` 替换 `@PTX_EMU_ROOT@` 占位符 → 生成 `build-off/configs/dgpu_soc_v1.json`，与 `dgpu_board_v1_mvp.json.in` 同源）
- [x] CMakeLists.txt 加 `configure_file(dgpu_soc_v1.json.in → dgpu_soc_v1.json)` 步骤
- [x] `cmake --build build --target validate_topology` PASS
- [x] `configs/test/pcie_endpoint_min.json` fixture：最小 `DGpuSoc` + `pcie_ep` JSON，用于 `test_pcie_endpoint_from_config.cc`（PE-G5 验证依赖）
- [x] 全量 `build/bin/cpptlm_tests` 无回归
- [x] 本 change 所有 spec 场景满足 → 可 archive
- [x] **Commit**: `chore(pcie-endpoint): integrate SOC config sample + validate_topology`
