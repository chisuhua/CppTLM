## 1.3d SDMA 完成通知

```cpp
// Fence descriptor (opcode=0x04) 触发 completion_ring
void on_fence_complete(uint64_t fence_id);  // done_out → CompletionRing → MSI-X trigger_msix(vector)
```

**Oracle 量化 AC（已复审，per design）**:
- Ring Buffer: cfg.ring_size ∈ {4KB, 8KB, 16KB, 64KB}, max 1024 entries @64B
- RPTR/WPTR: 32-bit
- Doorbell: BAR1 + 0x10010000
- SG: ≥ 8
- **NoC: ≥ 100 GB/s（Oracle O9 修订：测量定义 = bytes / simulated_latency；`gpu_mesh_noc` payload 转发后必须上报 simulated latency；测试断言 `simulated_throughput_GBps = payload_bytes / simulated_latency_s ≥ 100`）**
- dma_translate: identity/IOMMU 双模式
- MSI-X vector: 200ms 内 intr_cb ≥1


**fence-related vector 编号定义 (Oracle R-D 修订)**：
- PCI MSI-X 规范约定 fence = vector 0；本 change `msix_init(table_size=4)` 默认 fence 占 vector 0
- UE 集成测试断言：`captured_vector == 0`（fence 完成通知的 vector 编号）
- 链路：FENCE descriptor (opcode=0x04) → completion_ring → MSI-X vector 0 → `cpptlm_emulator.cc::msix_update_pending` → `board->trigger_irq_async(0)` → UE `intr_cb(user_ctx, 0, trans_id)`
## 风险评估

- **中风险**：Ring Buffer 大改造，需 Wire-format 验证
- **中等风险**：sim_loop drain 调度 race（per foundation 实测 0.55%）
- **需 Oracle 复审**：4 子阶段各 1 次

## 不在设计范围

- 阶段 1.4 电源管理（独立 change `stage-1-4-2-1`）
- 阶段 2.1 P2P（独立 change `stage-1-4-2-1`）
