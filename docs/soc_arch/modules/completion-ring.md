# completion-ring 微架构文档

> **类别**: GPU > Completion Ring · **状态**: 🔵 MVP 切片 (per ADR-SOC-06) + 📋 v1.0 dGPU SoC 战略补充(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) + L2 Completion & Notification)
> **Header**: `include/tlm/gpu/completion_ring_mvp.hh` → **`include/tlm/gpu/completion_ring_tlm.hh`** (T-bs-2b rename)
> **位置**: DGpuBoardTLM 内部组件 (s2) → **SOC 内 `CompletionRingTLM`** (T-bs-2b, 4 端口 ChStreamModuleBase, per ADR-SOC-07 D1/D7)
> **蓝图来源**: NVIDIA Completion Queue + Doorbell 强序 (per `docs/research/PCIe/PCIe_上的保序write.md` §4)
> **OpenSpec**: `openspec/changes/2026-08-26-cpptlm-dgpu-board-soc-split/`
> **关联 ADR**: [`ADR-SOC-06-cpptlm-v05-mvp.md`](../../adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5 · [`ADR-SOC-07-dgpu-board-soc-layering.md`](../../adr/ADR-SOC-07-dgpu-board-soc-layering.md) D1/D2
> **关联模块**: [`submit-queue.md`](./submit-queue.md) · [`tmu-dispatch-processor.md`](./tmu-dispatch-processor.md) · [`dgpu-board.md`](./dgpu-board.md)
> **首版 commit**: 🔵 W3-4 实施 (s2) · **命名提升**: T-bs-2b `49659f5` · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team (Sisyphus)

---

## 1. 设计目标 (per Phase F-H.5 + T-bs-2b)

`CompletionRing`(CQ)是 **GPU 命令完成通知路径**,接收 GPU warps / SDMA engine 完成事件,维护 FIFO ring buffer,并在 ring 满/半满时触发 **host 侧中断 (MSI-X)**。

**MVP 职责** (s2 W3-4):
- `on_warp_complete(task_id, status)`: 接收 warp 完成事件 (exactly-once, 不重复)
- `push` / `host_notify`: ring buffer push + host 通知回调
- FIFO 顺序: entries 按完成顺序投递,payload (task_id/status) 完整保留

**T-bs-2b 提升** (per design §3.5 端口表 + ADR-SOC-07 D2):
- **4 端口 ChStreamModuleBase**: `done_in[0]` / `done_in[1]` / `done_out` / `irq_out`
- **多源汇聚**: `done_in[0]` ← `gpu.done` (GPU warp 完成), `done_in[1]` ← `sdma.done_out` (SDMA 完成) — 避免 JSON 单端口多边汇聚 (per design §3.5 陷阱 1)
- **dep 链释放**: `done_out` → `tmu.done_in` (dep chain 释放传播, 缺失会导致 deadlock 检测不到 — 强制声明 per 陷阱 2)
- **irq_out**: `MsiXDeliveryBundle` 或经 `pcie_ep.irq_out` 转发

**MVP 模块归属** (per Phase A 修复 M4 协调):
- ✅ **S1-S4 固定为 DGpuBoardTLM 内部组件**
- ✅ **T-bs-2b 后提升为 SOC 内 `CompletionRingTLM`** (per design §3.5 + ADR-SOC-07 D1)
- 🟡 **v0.5 完整版** 可评估独立 host 侧 CQ 模型

---

## 2. 核心特性

- **exactly-once**: `on_warp_complete` 不重复投递 (ring 深度保护)
- **FIFO 保序**: entries 按完成顺序对 host 可见
- **强序 doorbell**: 与 Doorbell strong-order 路径协同 (per PCIe 保序 write)
- **多源汇聚**: GPU + SDMA 双源完成 (T-bs-2b 新增)

---

## 3. 与 s2 的差异 (T-bs-2b 命名提升)

| 维度 | s2 `CompletionRing` | T-bs-2b `CompletionRingTLM` |
|------|--------------------|----------------------------|
| 基类 | 普通 C++ 类 (Impl 值成员) | `ChStreamModuleBase` (4 端口) |
| 端口 | — | `done_in[0]`/`done_in[1]`/`done_out`/`irq_out` |
| Bundle | — | `CompletionBundle` (复用 dma_bundles_tlm.hh, sdma 唯一所有者) |
| 注册 | 无 | `REGISTER_CHSTREAM` |
| 多源 | 单源 (仅 GPU) | GPU + SDMA 双源汇聚 |

---

**维护**: CppTLM Team (Sisyphus)
**下次更新**: T-bs-4 follow-up (shell `msix_*` 完整化时同步)
