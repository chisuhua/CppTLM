# cpptlm-dgpu-board-abi-hygiene: Tasks (TDD)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/dgpu-board-abi-hygiene/spec.md`](specs/dgpu-board-abi-hygiene/spec.md)
> **依赖**: Wave 1（机械清理）已完成；Wave 2（测试覆盖）已完成，本 change 为 Wave 3
> **工期估算**: ~4h (3.0: 30min; 3a: 30min; 3b: 1h; 收尾: 2h)
> **硬门槛**: 既有 66864 assertions 零回归 + 15 ABI 字节级兼容 + `[pcie-bypass-tlp]` `[minimal_dgpu_soc]` 保持

---

## 文件清单

| 文件 | 变化 | 任务 |
|------|------|:----:|
| `include/modules_cluster.hh` | **改** | 3.0 |
| `include/tlm/gpu/dgpu_board_shell.hh` | **改** | 3b (删 2 公共声明) |
| `src/tlm/gpu/dgpu_board_shell.cc` | **改** | 3a (12 cast) + 3a (const_cast) + 3b (dispatch + link_layer) |

---

## Phase 3.0: PcieEndpointIP 短名生产注册（实锤 bug 修复）— 估 30min

### 3.0.1 PcieEndpointIP 短名注册 — 估 20min

- [x] **3.0.1** RED: 探针验证 `cpptlm_emulator_create("configs/dgpu_soc_minimal_v1.json")` 后 `board.pcie_ep() == nullptr`（生产路径短名未注册）— **[5min]**
- [x] **3.0.2** IMPL: `include/modules_cluster.hh` 加 `ModuleFactory::registerModule<tlm::pcie::PcieEndpointIP>("PcieEndpointIP")`（仿 Wave 2 GmmuTLM 短名模式）+ 修正误导注释 "EP 双注册同理" → 记录 EP 短名原本仅在测试 TU 注册、本 change 补回生产 — **[10min]**
- [x] **3.0.3** VALIDATE: 探针 `pcie_ep() != nullptr` + 构建零错 + 全量 66864/1553 PASS — **[5min]**

**验收**: AC1 达成。

---

## Phase 3a: 12 处 EP cast → `pcie_ep()` + 删冗余 const_cast — 估 30min

### 3a.1 12 处 cast 替换 — 估 15min

- [x] **3a.1.1** IMPL: `src/tlm/gpu/dgpu_board_shell.cc` 中 8 处单行 `auto* ep = dynamic_cast<...>(soc_->getInternalInstance("pcie_ep"));`（L432/443/453/602/621/640/655/672）替换为 `auto* ep = pcie_ep();` — `[5min]`
- [x] **3a.1.2** IMPL: 4 处内联 `if (auto* ep = dynamic_cast<...>(\n        soc_->getInternalInstance("pcie_ep")))`（L261/343/466/515）替换为 `if (auto* ep = pcie_ep())` — `[5min]`
- [x] **3a.1.3** VALIDATE: 构建零错 + `grep "dynamic_cast<PcieEndpointIP\*>"` 返回 0 + 全量回归 — `[5min]`

### 3a.2 删除冗余 const_cast — 估 15min

- [x] **3a.2.1** IMPL: `endpoint_bar_store_value` const 方法中 `auto* ep = const_cast<DGpuBoard*>(this)->pcie_ep();` → `auto* ep = pcie_ep();`（`pcie_ep()` 是 const 方法，const_cast 冗余）— `[5min]`
- [x] **3a.2.2** VALIDATE: `[pcie-bypass-tlp]` 17 assertions PASS（含 `endpoint_bar_store_value` 断言）+ 全量回归 — `[10min]`

**验收**: AC2 达成。

---

## Phase 3b: 删除空 switch `dispatch_mmio_to_pcie` + stub `link_layer_tx_tlp_out_count` — 估 1h

### 3b.1 删除 `dispatch_mmio_to_pcie` 全部 3 处 — 估 30min

- [x] **3b.1.1** IMPL: `dgpu_board_shell.cc` 删除 `mmio_write` 中调用 `dispatch_mmio_to_pcie(bar, offset, buf, len);` 行（含上一行 `// T-P12-1: 按 pcie_path 分流` 注释）— `[5min]`
- [x] **3b.1.2** IMPL: `dgpu_board_shell.cc` 删除 `void DGpuBoard::dispatch_mmio_to_pcie(...)` 函数定义 + 上方 `// ── T-P12-1: dispatch_mmio_to_pcie ──` 注释 — `[10min]`
- [x] **3b.1.3** IMPL: `dgpu_board_shell.hh` 删除 `void dispatch_mmio_to_pcie(...)` 声明 — `[5min]**
- [x] **3b.1.4** VALIDATE: `grep "dispatch_mmio_to_pcie"` 返回 0 + `[pcie-bypass-tlp]` PASS（验证 `pcie_path_` 状态语义不变） + 全量 — `[10min]`

### 3b.2 删除 `link_layer_tx_tlp_out_count` 全部 3 处 — 估 30min

- [x] **3b.2.1** 验证零调用方：`grep -rn "link_layer_tx_tlp_out_count" src/ include/ test/` — `[5min]`
- [x] **3b.2.2** IMPL: `dgpu_board_shell.cc` 删除 `size_t DGpuBoard::link_layer_tx_tlp_out_count() const { return 0; }` 定义 + 上方 `// 验证链路层无 TLP 发出 (axi_bypass 路径)` 注释 — `[5min]`
- [x] **3b.2.3** IMPL: `dgpu_board_shell.hh` 删除 `size_t link_layer_tx_tlp_out_count() const;` 声明 — `[5min]`
- [x] **3b.2.4** VALIDATE: `grep "link_layer_tx_tlp_out_count"` 返回 0 + 构建零错 + 全量 — `[15min]`

**验收**: AC3 + AC4 达成。

---

## Phase D: 收尾与验证 — 估 1h

### D1: openspec validate + 全量回归 — 估 30min

- [x] **D1.1** `openspec validate cpptlm-dgpu-board-abi-hygiene --strict` PASS — `[10min]`
- [x] **D1.2** 全量 `./build/bin/cpptlm_tests` 66864/1553 PASS（基线 zero regression）— `[10min]`
- [x] **D1.3** ABI 冻结: `git diff HEAD -- include/abi/cpptlm_emulator.h` 输出为空 — `[10min]`

**验收**: AC5 达成。

### D2: 文档同步 — 估 30min

- [x] **D2.1** `docs/architecture/14-dgpu-board-ideal-arch.md` §M5 移除"dispatch 空 switch"作为 v1.0 已知问题（Wave 3 已修复）— `[15min]`
- [x] **D2.2** `docs/adr/ADR-DGPU-03` §B4 标记"前置清理已完成"（Wave 3 删空 switch）— `[15min]`

---

## 依赖图

```
3.0.1 → 3.0.2 → 3.0.3
            ↓
3a.1.1-3 → 3a.2.1 → 3a.2.2
            ↓
3b.1.1 → 3b.1.2 → 3b.1.3 → 3b.1.4
3b.2.1 → 3b.2.2 → 3b.2.3 → 3b.2.4
            ↓
D1.1 → D1.2 → D1.3 → D2.1 → D2.2
```

**关键路径**: 3.0.3 → 3a.2.2 → 3b.2.4 → D1.2

---

**硬门槛**:
- 既有 66864 assertions 零回归（D1.2）
- ABI 字节级兼容（D1.3）
- `[pcie-bypass-tlp]` 17/3 PASS（3a.2.2 + 3b.1.4 锁定 `attach_profile`/`pcie_path()`/`endpoint_bar_store_value` 行为不变）
- `[minimal_dgpu_soc]` 41/2 PASS（锁定 `sdma_engine()` accessor 行为不变）
