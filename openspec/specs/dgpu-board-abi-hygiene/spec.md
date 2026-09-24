# dgpu-board-abi-hygiene Specification

## Purpose
TBD - created by archiving change cpptlm-dgpu-board-abi-hygiene. Update Purpose after archive.
## Requirements
### Requirement: pcie-endpoint-ip-short-name-registered

生产注册表 SHALL 同时登记 PcieEndpointIP 的全限定名 `tlm::pcie::PcieEndpointIP` 与短名 `PcieEndpointIP`，确保配置文件使用 `"type": "PcieEndpointIP"`（如 `configs/dgpu_soc_minimal_v1.json`、`dgpu_board_v1.json`、`examples/dgpu_soc_with_pcie_ip.json`）能在生产环境（非测试二进制）中正确实例化。

#### Scenario: 生产路径加载 minimal_v1.json 后 pcie_ep 非空

- **WHEN** `DGpuBoard::load_soc_config(minimal_v1_json)` + `init()`
- **THEN** `board.pcie_ep() != nullptr`
- **AND** `board.pcie_config_read/write`、`board.msix_*`、`board.is_mmio_gated()` 不返 -ENOSYS（除非参数本身非法）

### Requirement: dgpu-board-internal-cast-uses-accessor

`DGpuBoard` 内部所有需要 `PcieEndpointIP*` 的位置 SHALL 通过既有 `pcie_ep()` accessor 触达，不直接 `dynamic_cast<PcieEndpointIP*>` 内联展开。

#### Scenario: dgpu_board_shell.cc 中 PcieEndpointIP cast 收敛到 pcie_ep()

- **WHEN** `grep -c "dynamic_cast<tlm::pcie::PcieEndpointIP\*>" src/tlm/gpu/dgpu_board_shell.cc`
- **THEN** 计数 = 0

### Requirement: dispatch-empty-switch-removed

`DGpuBoard::dispatch_mmio_to_pcie` 4 态空 switch SHALL 整体删除；`pcie_path_` 状态机仍可通过 `attach_profile()` + `pcie_path()` accessor 读写。

#### Scenario: 既有 bypass 测试在 dispatch 缺失下仍绿

- **WHEN** `[pcie-bypass-tlp]` 标签测试运行
- **THEN** 17 assertions / 3 cases ALL PASS
- **AND** `board.pcie_path() == DGpuBoard::PciePath::AxiBypass/Legacy` 断言仍通过（state 语义保留）

### Requirement: link-layer-stub-removed

`DGpuBoard::link_layer_tx_tlp_out_count()` 桩方法 SHALL 删除（恒返 0，无任何测试调用）。

#### Scenario: 链接器零引用

- **WHEN** `grep -rn "link_layer_tx_tlp_out_count" src/ include/ test/`
- **THEN** 0 匹配

---

