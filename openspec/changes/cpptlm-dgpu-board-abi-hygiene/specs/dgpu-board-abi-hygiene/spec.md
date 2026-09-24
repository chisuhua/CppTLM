# dgpu-board-abi-hygiene: spec

> **所属 change**: [`cpptlm-dgpu-board-abi-hygiene`](../proposal.md)
> **范围**: Wave 3 卫生化（PcieEndpointIP 短名注册 + 12 cast 替换 + 删除空 switch / stub）
> **不变量**: 零行为变更（除 PcieEndpointIP 短名 bug 修复外）；既有测试断言数全部保持

---

## ADDED Requirements

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

## MODIFIED Requirements

> **MODIFIED 语义**: 本 change 删除 `dispatch_mmio_to_pcie` 实现 + 删除 `link_layer_tx_tlp_out_count`，但保留 `PciePath` 枚举、`pcie_path_` 成员、`attach_profile()` 与 `pcie_path()` accessor——既有依赖 `pcie_path()` 状态读写的测试必须继续通过。

---

## 兼容性约束（引用，非 Requirement）

- **15 ABI 字节级兼容** — `git diff HEAD -- include/abi/cpptlm_emulator.h` 必须为空
- **既有 66864 assertions 零回归**（per Wave 2 基线）
- **`PciePath` / `attach_profile` / `pcie_path()` / `endpoint_bar_store_value` / `sdma_engine_` / `sdma_engine()` accessor**：对外接口与 ABI 契约全部保留
- **`[pcie-bypass-tlp]` 17/3 PASS**：锁定 4 项公共契约不变
- **`[minimal_dgpu_soc]` 41/2 PASS**：锁定 `sdma_engine()` accessor 行为不变
- **Wave 1/2 改动仍保留**：errno 宏归一化、abi_guard 模板、Wave 2 E2E 测试文件等全部保留
