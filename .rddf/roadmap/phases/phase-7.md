---
id: phase-7
kind: phase
status: completed
phase_refs: []
主题: D-AXI Driver-Visible Minimal SoC v1.4
---

## phase-7: D-AXI Driver-Visible Minimal SoC v1.4

> **真源**: [`docs/pcie/driver-visible-minimal-soc.md`](../../docs/pcie/driver-visible-minimal-soc.md) (v1.8 实施笔记)
> **三视角架构**: [`docs/designs/dgpu-board/architecture.md`](../../docs/designs/dgpu-board/architecture.md) + [`docs/designs/dgpu-soc/architecture.md`](../../docs/designs/dgpu-soc/architecture.md) + [`docs/designs/dgpu-driver/architecture.md`](../../docs/designs/dgpu-driver/architecture.md)
> **5 业务 ADR**: [`docs/adr/ADR-DGPU-05~09-*.md`](../../docs/adr/)
> **OpenSpec 已交付**: [driver-visible-minimal-soc](../../openspec/specs/driver-visible-minimal-soc/) (v1.8 tasks.md T0-T5)

### v1.0 → v1.4 演进 (关键根因)

| 版本 | 关键根因 / 变更 | 状态 |
|------|---------------|------|
| **v1.0** | 初版提案 (5 澄清 + R1-R7 + M1-M5) | ✅ 引导 |
| **v1.1** | P0 修订 (5 澄清落点 + R1-R7 + M1-M5) | ✅ 完成 |
| **v1.2** | Oracle 二次审查 8 must-fix (N1-N12 全部落地) | ✅ PASS |
| **v1.4** | **Metis/Oracle 二轮 + 用户质疑 + 单一 VRAM 所有权上提** (B7-B13 + 7 实施铁律 + 3 遗留议题延期) | ✅ **实施中** |

### v1.4 关键架构根因 (B7-B13)

- **B7**: 单一 VRAM backing 所有权归 DGpuBoard (`vram_storage_` = 8GB `unique_ptr<uint8_t[]>` default-init, Linux lazy commit, stable pointer)
- **B8**: `memory_read/write` bound = injected `backing_size_` (非 `kDefaultMemSize` 常量)
- **B9**: `handle_slave_port` 检查 `memory_read/write` 返回值; 失败置 `resp.resp.write(1)` (SLVERR)
- **B10**: 保留 `std::mutex backing_mutex_` (host/sim 并发保护)
- **B11**: 双 size 字段拆分 `bar1_window_size_` (BAR1 窗口 = `bar_sizes[1]`) vs `vram_size_` (VRAM = `bar_sizes[2]`)
- **B12**: 未注入 backing 时 `memory_read/write` 返 `-ENODEV`; `kRegMemSizeLo/Hi` 读 injected `backing_size_`
- **B13**: 24-case `[pcie-memory]` 机械迁移 + `[minimal_dgpu_soc]` 添加 BAR2 + 4 处 lazy alloc 行为删除

### 7 实施铁律 (per `docs/pcie/driver-visible-minimal-soc.md` §4)

1. 单一 VRAM 所有权归 DGpuBoard
2. `AxiMemBundle` chip-internal vs `PcieTlpBundle` board-level 边界严格分离 (per ADR-DGPU-06)
3. 23 ABI 冻结 + 0 新增 (per ADR-DGPU-08 §2.3)
4. D3 演进 seam 已就位 (per ADR-DGPU-07)
5. driver 视角闭环 (15 ABI + 4 BAR + driver 闭环, per ADR-DGPU-09)
6. 5 消费者共享 `vram_storage_` (backdoor / BAR1 fast-path / BAR2 via memory_device / MemoryTLM backing / SDMA-GMMU legacy ptr)
7. 0 个新 ABI 函数 (per ADR-088 §D5)

### 3 遗留议题 (延期, per v1.4)

- **D1 Display FB**: 冻结面约束, D3 收编
- **doorbell 0x10010000**: 测试合成偏移
- **MemoryTLM capacity_gb=1**: 不静默 (per VramControllerTLM 1 GHz 假设)

### D3 演进 seam (v1.4 已就位)

- `handle_slave_port ↔ backing_ptr_` 之间已插入 VramControllerTLM (继承 MemoryTLM + 行缓冲 + bandwidth 上限)
- Phase 4 T4 实施完成 (per `cpptlm-dgpu-soc-timing-mvp` T4)

### 5 业务 ADR (per `docs/adr/` 真源 3)

- [`ADR-DGPU-05-vram-storage-ownership.md`](../../docs/adr/ADR-DGPU-05-vram-storage-ownership.md) — D-AXI B7 单一 VRAM 所有权 (P0 根因)
- [`ADR-DGPU-06-axi-mem-bundle-boundary.md`](../../docs/adr/ADR-DGPU-06-axi-mem-bundle-boundary.md) — D-AXI B2 chip-internal vs board-level 边界 (P0)
- [`ADR-DGPU-07-minimal-soc-evolution-seam.md`](../../docs/adr/ADR-DGPU-07-minimal-soc-evolution-seam.md) — D-AXI §X D3-D5 演进 seam (接口零变更)
- [`ADR-DGPU-08-abi-freeze-policy.md`](../../docs/adr/ADR-DGPU-08-abi-freeze-policy.md) — D-AXI 23 ABI 冻结 + 0 新增 (P0)
- [`ADR-DGPU-09-driver-visible-minimal-soc-scope.md`](../../docs/adr/ADR-DGPU-09-driver-visible-minimal-soc-scope.md) — D-AXI 范围定义 (4 BAR + 15 ABI + 0 新增)