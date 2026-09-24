# cpptlm-dgpu-board-abi-hygiene: Change Proposal

> **配套**: [`design.md`](design.md) · [`tasks.md`](tasks.md) · [`specs/dgpu-board-abi-hygiene/spec.md`](specs/dgpu-board-abi-hygiene/spec.md)
> **关联 change**: [`cpptlm-dgpu-board-shell-cleanup` (Wave 1 父变更)](../cpptlm-dgpu-board-shell-cleanup/proposal.md) — 本 change 为其 Wave 3 实施
> **关联 change**: [`cpptlm-minimal-dgpu-soc-v1-test-coverage` (Wave 2)](../cpptlm-minimal-dgpu-soc-v1-test-coverage/proposal.md) — 提供 Wave 2 复盘发现的生产 bug 上下文
> **工期估算**: ~4h (Wave 3.0 ~30min, Wave 3a ~30min, Wave 3b ~1h, 收尾 + openspec validate ~1h, docs/sync ~1h)

---

## §1 范围

本 change 包含 4 项独立的 dGPU board / ABI 卫生化改动，全部为零行为变更或生产 bug 修复：

1. **PcieEndpointIP 短名注册（生产 bug 修复）** — `dgpu_soc_minimal_v1.json` / `dgpu_board_v1.json` / `examples/dgpu_soc_with_pcie_ip.json` 全部使用 `"type": "PcieEndpointIP"`（短名），但生产注册表只注册了 FQ `tlm::pcie::PcieEndpointIP`，导致生产 `pcie_ep()` 静默返 nullptr → `pcie_config_read/write`、`msix_*`、`mmio_gated` 等返 -ENOSYS。测试二进制因跨 TU 静态初始化掩蔽而绿。
2. **`PcieEndpointIP` 短名注册注释修正** — Wave 2 在 `modules_cluster.hh` 加 GmmuTLM 短名时注释声称 "EP 双注册同理"，但 EP 短名实际只在测试 TU 注册（生产缺失），该注释具有误导性。
3. **12 处 `dynamic_cast<PcieEndpointIP*>` 统一为 `pcie_ep()` accessor** — `dgpu_board_shell.cc` 中 12 处内联 cast 全部用既有 `pcie_ep()` accessor 替代；附带删除 `endpoint_bar_store_value` 中冗余 `const_cast`。
4. **删除 `dispatch_mmio_to_pcie` 与 `link_layer_tx_tlp_out_count`** — 前者 4 态 switch 全为 `break`（空操作，spec 名实不符）；后者恒返 0（无任何测试调用）。两者均不构成公共 API（仅 `dispatch_mmio_to_pcie` 在 .hh 有声明），但既有测试 `test_pcie_bypass_tlp_data_path.cc` 依赖 `attach_profile`/`PciePath`/`pcie_path()` 枚举与访问器——这些**保留**。

## §2 不在范围

- `PciePath` 枚举（4 态：`Legacy`/`AxiBypass`/`Tlp`/`Mock`）与 `pcie_path_` 成员：`test_pcie_bypass_tlp_data_path.cc` 直接断言 `board.pcie_path() == DGpuBoard::PciePath::AxiBypass/Legacy`，保留。
- `attach_profile()` / `pcie_path()` accessor：同上，保留。
- `endpoint_bar_store_value`：测试使用（L70 断言 `endpoint_bar_store_value(0,0,0x1000)==0`），保留。
- `sdma_engine_` 成员 + `set_sdma_engine()`：`test_minimal_dgpu_soc_e2e.cc` 通过 `sdma_engine()` accessor 触达 SOC 内建 sdma；doorbell 生产路径（dgpu_board_shell.cc:392/407/583）保留。
- 8 字节 doorbell wptr 截断：`cc:402-404 else if (reg_data.size() >= 8)` 永远不达（被 `>=4` 捕获）。**保留**——属于另一类 bug，不在本 change 范围。
- 双注册 `GmmuTLM`：Wave 2 已添加，不重复。
- 双注册 `SdmaEngineTLM` / `MemoryTLM` / `CompletionRingTLM`：已通过 `REGISTER_CHSTREAM` 自动以短名注册（Metis 复盘实证），不需添加。

## §3 用户价值

| 改动 | 价值 |
|------|------|
| PcieEndpointIP 短名 | **修复生产 bug**：恢复 `libcpptlm_emulator.so` / 主可执行的完整 BAR/config/MSI-X 接口 |
| 注释修正 | 防误读：未来读者不再误以为 EP 双注册已正确 |
| 12 cast 替换 + const_cast 删除 | 消除 12 处重复代码 + 1 处冗余 cast，行为不变；为 Wave 4+ 重构铺路 |
| 删 2 个 stub/空 switch | 消除名实不符（dispatch spec 名不符 link Layer R3）+ 死代码 |

## §4 兼容性约束

- **`include/abi/cpptlm_emulator.h` 冻结**：零 diff（ADR-088 §D5；tasks.md D3.2 硬门槛）。
- **既有 66864 assertions 零回归**：所有定向标签（`[pcie-bypass-tlp]`/`[dgpu]`/`[abi]`/`[pcie]`/`[gmmu]`/`[sdma][fence]`/`[minimal_dgpu_soc]`）保持原断言数。
- **`PciePath` / `attach_profile` / `pcie_path()` / `endpoint_bar_store_value` / `sdma_engine_`**：对外接口保留。

## §5 Alternatives Considered

| 备选 | 否决理由 |
|------|---------|
| 把 PcieEndpointIP 短名注册放进测试 TU（沿用 EP 旧惯例） | 生产路径静默失败；测试 TU 跨二进制链接掩蔽掩盖 bug |
| 删除 `PciePath` 枚举 / `pcie_path()` accessor | `test_pcie_bypass_tlp_data_path.cc:58-63,122-125` 依赖；ADR-DGPU-03 明确保留 |
| 把 `dispatch_mmio_to_pcie` 替换为 ADR-DGPU-03 的 registry | 超出本 change 范围；ADR-DGPU-03 仍为提案；本 change 仅删除空 switch，不引入新机制 |
| 删除 `endpoint_bar_store_value` 中 `const_cast` 的同时删除整个函数 | `test_pcie_bypass_tlp_data_path.cc:70` 断言 `endpoint_bar_store_value(0,0,0x1000)==0`，保留 |

## §6 依赖

- **依赖**: 无（独立可执行）
- **被依赖**: 未来 Wave 4+（如 ADR-DGPU-01/02/03/04 落地时，可在本 change 已统一 `pcie_ep()` accessor 基础上推进）

## §7 关键风险

| 风险 | 缓解 |
|------|------|
| PcieEndpointIP 短名注册可能与既有 test TU 注册冲突（同名重复注册） | `module_factory.hh:91-95` 已有 `existing entry is no-op` 早退，静默合并 |
| 删除 `dispatch_mmio_to_pcie` 改变 `pcie_path_` 状态机的可观察行为 | `dispatch_mmio_to_pcie` 4 态全为 `break`，删除前与删除后 `pcie_path_` 状态语义不变 |
| 删除公共声明 `.hh` 触发既有 ABI 契约回归 | `dispatch_mmio_to_pcie` 不是 ABI 导出（不在 `cpptlm_emulator.h` 15 个导出符号内）；`link_layer_tx_tlp_out_count` 仅供未发布的测试 stub |
| 12 cast 替换跨函数语义变更 | `pcie_ep()` 与内联 cast 严格等价（`soc_ null → nullptr` + `dynamic_cast<>` 命中时返回同一对象）；机械替换验证 build + 全量 66864 不变 |

## §8 验证标准

- ✅ `cmake --build build -j$(nproc)` exit 0
- ✅ `./build/bin/cpptlm_tests` 66864/1553 PASS（基线 zero regression）
- ✅ `git diff HEAD -- include/abi/cpptlm_emulator.h` 输出为空
- ✅ `openspec validate cpptlm-dgpu-board-abi-hygiene --strict` PASS
- ✅ `[pcie-bypass-tlp]` `[minimal_dgpu_soc]` `[dgpu]` `[abi]` 定向全绿

---

**Owner**: CppTLM Team · **版本**: v1.0 · **日期**: 2027-02-09
