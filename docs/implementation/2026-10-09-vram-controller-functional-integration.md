# 实施笔记 — cpptlm-vram-controller-functional-integration (Phase 0 Change A)

> **Change**: `openspec/changes/cpptlm-vram-controller-functional-integration/`
> **目标 tag**: `cpptlm-vram-controller-functional-integration-completion`
> **业务定位**: `docs/designs/dgpu-soc/architecture.md` §0.1 原则 4 + §0.2 Phase 0 Change A (P0, 3-5d)
> **修复 Gap**: G3 (VramControllerTLM functional-mode 数据路径)
> **作者**: CppTLM Team · **日期**: 2026-10-09

---

## T0 — 所有权验证 (setup, per ADR-DGPU-05 + D-AXI v1.4 B7)

**验证目标**: minimal_v1 单一 VRAM 真源 `vram_storage_` 所有权完整, Change A 仅新增配置接口 + 激活既有注入点, **不修改所有权**.

**证据** (`src/tlm/gpu/dgpu_board_shell.cc`):
- `dgpu_board_shell.cc:167` — `vram_storage_ = std::make_unique<uint8_t[]>(framebuffer_size_);` (单一 owner)
- `dgpu_board_shell.cc:168` — `framebuffer_ptr_ = vram_storage_.get();` (stable pointer, 5 消费者共享)
- `dgpu_board_shell.cc:700` — `pcie_mem->set_backing_view(framebuffer_ptr_, ...)`
- `dgpu_board_shell.cc:738` — `vram_ctrl->set_backing_view(framebuffer_ptr_, ...)` (T23C 第 4 路, 已存在)
- `dgpu_board_shell.cc:711` / `:725` — sdma / gmmu 同源注入

**baseline 测试计数** (Change A 实施前):
- `[minimal_dgpu_soc]` — 12 cases / 66 assertions ✅
- `[pcie-memory]` — 17 cases / 48 assertions ✅

**结论**: 所有权完整; Change A 仅在 `vram_ctrl` 注入块内加 `set_mode()` + `set_vram_controller()` 装配, 0 改动所有权.

---

## 计划和 TDD 5-step 流程

严格按 T0-T7 顺序 (RED → 实现 → GREEN), 每 T 一个 atomic commit:

| T | 类型 | 内容 |
|---|------|------|
| T0 | setup | 所有权验证 (本段) |
| T1 | test RED | `set_mode(FunctionalZeroDelay)` case |
| T2 | impl GREEN | `VramControllerMode` enum + `set_mode()`/`mode()` |
| T3 | test RED | `handle_functional_request()` 入口 case |
| T4 | impl GREEN | `handle_functional_request()` + `PcieMemoryDevice` port0 委托 |
| T5 | config | `dgpu_soc_minimal_v1.json` `simulation_mode` + `vram_ctrl` 节点 |
| T6 | impl | `DGpuBoard::bind_memory_backings()` 装配 vram_ctrl + pcie_memory |
| T7 | test GREEN | functional-mode E2E 闭环, `[minimal_dgpu_soc]` 12→≥15 cases |

**关键设计决策** (per design.md A-B1/A-B3 + ADR-DGPU-11 H2 互斥):
- `handle_functional_request()` 是**同步 functional API** (模仿 gem5 `AbstractMemory::functionalAccess()`), 仅 functional-mode 使用; timing-mode 仍走既有 ChStream `tick()` 路径.
- **functional / timing 互斥**: `vram_ctrl` mode 由顶层 `simulation_mode` 字段决定; timing JSON 保持默认 `TimingCycleApproximate` 且**不** wire `pcie_memory`, 保证 `[dgpu_soc_timing]` 100% 零回归.
- **descriptor-only bulk TLP** (SDMA H2D `len>8`, data=0, 大数据由 SDMA 经共享 backing 搬运): 委托时传 `data=nullptr` → 控制器仅做范围/协议确认, **不**覆盖数据, 保证 minimal E2E 字节一致.
- GMMU `pcie_memory` port 1 **保持不变** (per H-B6 + tasks.md Task 3.2 Step 4).
