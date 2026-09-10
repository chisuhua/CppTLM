# Design: cpptlm-stage-1-3-sdma — SDMA 4 子阶段

## 1.3a SDMA Ring Buffer

```cpp
// sdma_ring_buffer.h
class SdmaRingBuffer {
public:
    SdmaRingBuffer(size_t cfg_size, size_t entry_size);  // cfg_size ∈ {4KB, 8KB, 16KB, 64KB}
    std::atomic<uint32_t>& wptr() { return wptr_; }  // 32-bit
    std::atomic<uint32_t>& rptr() { return rptr_; }
    void write_doorbell(uint32_t val);  // BAR1+0x10010000 写入触发
private:
    std::atomic<uint32_t> wptr_{0}, rptr_{0};
    std::vector<uint8_t> entries_;
};
```

## 1.3b D2D NoC

```cpp
// d2d_noc_path.h - payload 转发 ≥ 100 GB/s
void d2d_noc_forward(uint64_t src_va, uint64_t dst_va, size_t len);
```

## 1.3c dma_translate（修复 #2）

```cpp
// cpptlm_emulator.cc:443-460 - identity 模式返 0, IOMMU 模式返负 errno
int cpptlm_emulator_register_dma_translate_cb(cpptlm_emulator_t* emu, cpptlm_dma_translate_cb cb) {
    if (cb == nullptr) return -EINVAL;
    emu->board->set_dma_translate_callback(cb);  // 真实接线，无 (void)cb
    return 0;
}
// DGpuBoard::dma_translate：identity → pa=iova, ret=0; IOMMU cb 失败 → -ENOSYS/-EIO → error_cb
```

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
- NoC: ≥ 100 GB/s
- dma_translate: identity/IOMMU 双模式
- MSI-X vector: 200ms 内 intr_cb ≥1

## 风险评估

- **中风险**：Ring Buffer 大改造，需 Wire-format 验证
- **中等风险**：sim_loop drain 调度 race（per foundation 实测 0.55%）
- **需 Oracle 复审**：4 子阶段各 1 次

## 不在设计范围

- 阶段 1.4 电源管理（独立 change `stage-1-4-2-1`）
- 阶段 2.1 P2P（独立 change `stage-1-4-2-1`）
