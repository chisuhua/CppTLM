# cpptlm-dgpu-board-abi-hygiene: 设计文档

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/dgpu-board-abi-hygiene/spec.md`](specs/dgpu-board-abi-hygiene/spec.md)
> **设计 SSOT**: [`docs/designs/2027-02-09-minimal-dgpu-soc.md`](../../../docs/designs/2027-02-09-minimal-dgpu-soc.md) (v1.0)
> **本文件定位**: 记录 Wave 3 实施期的设计决策与取舍

---

## §1 范围概览

本 change 实施 3-Wave 计划中的 Wave 3（结构性清理），承接 Wave 2 复盘发现的实锤生产 bug：

- Wave 1（机械清理）已完成：清理陈腐注释 + 19 处裸 errno 归一化 + abi_guard 模板替换 10 个 try-catch 克隆 + (void) 转换清理
- Wave 2（测试覆盖）已完成：新增 `test_minimal_dgpu_soc_e2e.cc` (41 assertions) + `DGpuBoard::sdma_engine()` accessor + GmmuTLM 短名注册
- **Wave 3（本 change）**：PcieEndpointIP 短名生产注册（实锤 bug 修复）+ 12 cast 替换 + 删除空 switch / stub

## §2 代码组织

```
include/modules_cluster.hh                    # 改: +PcieEndpointIP 短名注册 + 修正注释
include/tlm/gpu/dgpu_board_shell.hh           # 改: 删 2 个公共声明（link_layer + dispatch）
src/tlm/gpu/dgpu_board_shell.cc              # 改: 12 cast → pcie_ep(), 删 dispatch/link_layer, 删 const_cast
openspec/changes/cpptlm-dgpu-board-abi-hygiene/  # 新: 本 change artifacts
```

**不动**: `configs/dgpu_soc_minimal_v1.json`、`include/abi/cpptlm_emulator.h`、所有 `docs/`、`test/`。

## §3 关键设计决策

| # | 决策点 | 选择 | 理由 / 依据 |
|---|--------|------|------------|
| **D1** | PcieEndpointIP 短名注册位置 | `modules_cluster.hh`（生产注册中枢）而非测试 TU | Wave 2 GmmuTLM 短名先例 + `test_pcie_endpoint_ip_simmodule_refactor.cc` 跨 TU 掩蔽已暴露生产 bug |
| **D2** | 12 cast 替换策略 | 整段 `replaceAll` 而非逐处 | 8 处单行 cast + 4 处内联 if-cast 共 12 处，文本完全等价；`pcie_ep()` accessor 与内联 cast 严格等价（`soc_ null → nullptr`、否则同一对象） |
| **D3** | 保留 `PciePath` 枚举 + `attach_profile` | 保留 | `test_pcie_bypass_tlp_data_path.cc:58-63,122-125` 直接断言；ADR-DGPU-03 明确保留 |
| **D4** | 删除 `dispatch_mmio_to_pcie` | 整体删除 4 态空 switch | 4 态全 `break`，spec 名实不符（per ADR-DGPU-03 §B4 计划删除）；`pcie_path_` 状态机可观察行为不变 |
| **D5** | 删除 `link_layer_tx_tlp_out_count` | 整体删除 | 无任何测试调用，恒返 0，仅供未发布 stub |
| **D6** | 删除 `endpoint_bar_store_value` 中 `const_cast` | 删 | `pcie_ep()` 是 const 方法，const_cast 冗余；测试 `endpoint_bar_store_value(0,0,0x1000)==0` 仍可通过 |
| **D7** | 保留 `endpoint_bar_store_value` / `sdma_engine_` | 保留 | 测试使用 + doorbell 生产路径依赖 |
| **D8** | 不动 `mmio_self_drain_enabled` / `attach_framebuffer_for_testing` 等公开旋钮 | 不动 | Wave 2 测试依赖 |
| **D9** | 不顺手修复 8 字节 doorbell wptr 截断 | 不动 | 行为变更，超出本 change；属另一类 bug |

## §4 模块集成图（Wave 3 前后）

### Wave 3 前后对比

| 路径 | Wave 2 前 | Wave 3 后 |
|------|----------|----------|
| `cpptlm_emulator_create` → `DGpuBoard::load_soc_config(minimal_v1.json)` | `pcie_ep = nullptr`（生产静默） | `pcie_ep != nullptr`（短名注册生效） |
| `board.pcie_config_read()` | 返 -ENOSYS | 正常返回 config space |
| `board.msix_init(4,0)` | 返 -ENOSYS | 正常初始化 MSI-X |
| `DGpuBoard::pcie_config_read` 内部 | 12 处内联 `dynamic_cast<PcieEndpointIP*>` | 12 处 `pcie_ep()` accessor |
| `DGpuBoard::endpoint_bar_store_value` | `const_cast<DGpuBoard*>(this)->pcie_ep()` | `pcie_ep()` |
| `DGpuBoard::mmio_write` | 调用空 switch `dispatch_mmio_to_pcie` | 无此调用 |
| `DGpuBoard::link_layer_tx_tlp_out_count` | 恒返 0 | 已删除 |

## §5 Alternatives Considered

| 备选 | 否决理由 |
|------|---------|
| 把 PcieEndpointIP 短名注册放进 `test_pcie_endpoint_ip_simmodule_refactor.cc` 删除原行、移至生产 | 测试 TU 跨二进制链接掩蔽正是问题根源；必须移到生产中枢 |
| 删除 `PciePath` 枚举 / `pcie_path()` accessor | 测试 + ADR-DGPU-03 双重依赖 |
| 删除整个 `endpoint_bar_store_value` | `test_pcie_bypass_tlp_data_path.cc:70` 依赖；只删冗余 const_cast 而已 |
| 把 `dispatch_mmio_to_pcie` 替换为 ADR-DGPU-03 的 registry | 超出本 change 范围；ADR-DGPU-03 仍为提案；本 change 仅删空 switch |
| 顺手修 8 字节 doorbell wptr 截断 (`else if (>= 8)` 死分支) | 行为变更；属另一类 bug；记入 Wave 4+ backlog |
| 把 12 cast 替换拆为 4 批 + 每批独立 commit | `replaceAll` 严格等价；全量回归一次即可；分批不增信心 |

## §6 Tradeoffs

- **PcieEndpointIP 短名注册与既有 `test_pcie_endpoint_ip_simmodule_refactor.cc:305-307` 重复**：重复注册已由 `module_factory.hh:91-95` early-return 兜底（同名重复静默合并）；不删除测试 TU 的注册以免破坏该测试的隔离性。
- **12 cast → `pcie_ep()` 的风格差异**：`pcie_ep()` 每次调用走 `dynamic_cast<>`（无缓存）；内联 cast 写时亦调用一次。两者均无缓存，性能等价。
- **删除 `dispatch_mmio_to_pcie` 后 `pcie_path_` 状态机的可观察行为变化**：4 态 switch 全 `break`，删除前后对 `pcie_path_` 状态的读写路径完全相同（仅 `pcie_path()` accessor 与 `attach_profile()` 仍可写/读 `pcie_path_`）。
- **不动 8 字节 doorbell 截断**：保留为已知 latent bug；明确记入 backlog，避免 Wave 3 scope creep。

## §7 Technical Risks

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| R1 | PcieEndpointIP 短名与测试 TU 重复注册冲突 | 🟢 低 | `module_factory.hh:91-95` early-return |
| R2 | 删除 `dispatch_mmio_to_pcie` 后 `pcie_path_` 状态语义变化 | 🟢 低 | 4 态全 `break`，删除前后等价 |
| R3 | 删除 `link_layer_tx_tlp_out_count` 公共声明破坏既有测试 | 🟢 低 | 已 grep 全仓，零调用方 |
| R4 | 12 cast 替换跨函数语义变更 | 🟢 低 | `pcie_ep()` 与内联 cast 严格等价；build + 全量 66864 验证 |
| R5 | 删除 `endpoint_bar_store_value` 中 `const_cast` 误改行为 | 🟢 低 | `pcie_ep()` 是 const 方法，const 成员函数内直接调用合法 |

---

**Owner**: CppTLM Team · **版本**: v1.0 · **日期**: 2027-02-09
