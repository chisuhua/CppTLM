# io-disk 微架构文档

> **类别**: io > disk
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/simple_disk_tlm.hh`
> **蓝图来源**: gem5 `src/dev/storage/simple_disk.hh`（简化块设备）
> **首版 commit**: 蓝图（来自调研 §2.5）
> **最近更新**: 2026-06-12
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [io-dma.md](./io-dma.md) (DMA 传输) | [io-pci.md](./io-pci.md) | [io-uart.md](./io-uart.md)

---

## 1. 设计目标（蓝图）

`tlm::SimpleDiskTLM` 是 CppTLM v2.2+ 规划的 **简化块设备**——模拟 SSD/HDD 行为的简版模型，块级 read/write 访问。**与 gem5 对位**: `gem5::SimpleDisk`（~200 行，固定延迟 + 块大小）。

**核心特征**：
- **块设备**（典型 4 KB 块大小）
- **固定读写延迟**（read=100μs, write=200μs）
- **DMA 传输**（继承 DmaDeviceTLM）
- **可选 NCQ 队列**（v0 简化：单队列 FIFO）
- **容量可配**（典型 1 GB - 1 TB）

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                SimpleDiskTLM 继承 DmaDeviceTLM              │
│                                                             │
│  ┌──────────────────────────────────────────────────┐     │
│  │  block_storage_ (固定数组或文件)                  │     │
│  │    - 块大小: 4 KB                                 │     │
│  │    - 容量: 配置                                  │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  pending_io_queue_                                │     │
│  │    - 每 IO 关联延迟 + 起始 LBA + 数据             │     │
│  └──────────────────────────────────────────────────┘     │
│            │                                                │
│            ▼                                                │
│  ┌──────────────────────────────────────────────────┐     │
│  │  on_dma_complete (从 DmaDeviceTLM 继承)           │     │
│  │    - 块存储读/写                                  │     │
│  │    - 启动下一 IO                                  │     │
│  └──────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 端口表

继承 DmaDeviceTLM 的 2 端口（master + slave）。

### 2.2 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  SimpleDiskTLM 内部                         │
│                                                             │
│  配置:                                                      │
│    - block_size_: 4096 (4 KB)                               │
│    - capacity_bytes_: 默认 16 GB                            │
│    - read_latency_cycles_: 10000 (100 μs)                   │
│    - write_latency_cycles_: 20000 (200 μs)                  │
│    - max_outstanding_io_: 32 (NCQ)                          │
│                                                             │
│  存储:                                                      │
│    - block_storage_: std::vector<uint8_t>  (in-memory)      │
│    - 或 file_path_ (文件持久化，Phase 7+ 实施)               │
│                                                             │
│  队列:                                                      │
│    - pending_io_queue_: std::deque<IORequest>              │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {
class SimpleDiskTLM : public DmaDeviceTLM {
public:
    static constexpr uint64_t DEFAULT_BLOCK_SIZE = 4096;
    static constexpr uint64_t DEFAULT_CAPACITY = 16ULL * 1024 * 1024 * 1024;  // 16 GB
    static constexpr uint32_t DEFAULT_READ_LATENCY = 10000;   // 100 μs
    static constexpr uint32_t DEFAULT_WRITE_LATENCY = 20000;  // 200 μs

    explicit SimpleDiskTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "SimpleDiskTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_block_size(uint64_t size) { block_size_ = size; }
    void set_capacity(uint64_t bytes) { capacity_bytes_ = bytes; }
    void set_read_latency(uint32_t cycles) { read_latency_cycles_ = cycles; }
    void set_write_latency(uint32_t cycles) { write_latency_cycles_ = cycles; }
    void set_file_backend(const std::string& path) { file_path_ = path; }

    // === DmaDevice 抽象实现 ===
    void on_dma_complete(uint64_t txn_id, bool success) override;

    // === 块设备访问（高层 API，可选） ===
    void read_blocks(uint64_t lba, uint32_t count, uint64_t mem_addr);
    void write_blocks(uint64_t lba, uint32_t count, uint64_t mem_addr);

    // === ChStream 桥接 ===
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void process_io_queue();
    void service_io(IORequest& req);

    uint64_t block_size_;
    uint64_t capacity_bytes_;
    uint32_t read_latency_cycles_;
    uint32_t write_latency_cycles_;
    std::string file_path_;
    std::vector<uint8_t> block_storage_;

    struct IORequest {
        uint64_t lba;
        uint32_t count;
        uint64_t mem_addr;
        bool is_write;
        uint64_t arrival_cycle;
        uint64_t completion_cycle;
        DmaCallback callback;
    };
    std::deque<IORequest> pending_io_queue_;

    // 统计
    tlm_stats::Scalar reads_completed_;
    tlm_stats::Scalar writes_completed_;
    tlm_stats::Scalar bytes_read_;
    tlm_stats::Scalar bytes_written_;
    tlm_stats::Average io_queue_depth_;
    tlm_stats::Distribution io_latency_;
};
}
```

## 4. 行为流程

```cpp
void SimpleDiskTLM::tick() {
    DmaDeviceTLM::tick();  // 处理 DMA 响应

    uint64_t now = current_cycle();

    // 处理到期的 IO 请求
    while (!pending_io_queue_.empty() &&
           pending_io_queue_.front().completion_cycle <= now) {
        IORequest req = pending_io_queue_.front();
        pending_io_queue_.pop_front();

        if (req.is_write) {
            // DMA 写完成后，数据已到内存，无需额外操作
            ++writes_completed_;
        } else {
            // DMA 读完成：数据已在内存
            ++reads_completed_;
        }
        if (req.callback) req.callback(req.lba, true);
    }

    // 启动新 IO（NCQ 风格）
    while (pending_io_queue_.size() < max_outstanding_io_ &&
           !io_pending_queue_.empty()) {
        service_io(io_pending_queue_.front());
        io_pending_queue_.pop_front();
    }

    io_queue_depth_.sample(pending_io_queue_.size());
    if (adapter_) adapter_->tick();
}

void SimpleDiskTLM::service_io(IORequest& req) {
    uint64_t latency = req.is_write ? write_latency_cycles_ : read_latency_cycles_;
    req.completion_cycle = req.arrival_cycle + latency;

    if (req.is_write) {
        // 发起 DMA 写
        dmaWrite(req.mem_addr, req.lba * block_size_,
                 req.count * block_size_,
                 [this, req](uint64_t, bool) { /* write complete */ });
    } else {
        // 发起 DMA 读
        dmaRead(req.lba * block_size_, req.count * block_size_, req.mem_addr,
                [this, req](uint64_t, bool) { /* read complete */ });
    }
    pending_io_queue_.push_back(req);
}
```

## 5. 蓝图对齐

| gem5 蓝图 | CppTLM 对应 | 差异 |
|----------|------------|------|
| `src/dev/storage/simple_disk.hh` SimpleDisk | `tlm::SimpleDiskTLM` | 同语义 |
| `SimpleDisk::read` | `read_blocks` + `on_dma_complete` | 拆分为两步（DMA + 存储） |
| `SimpleDisk::write` | `write_blocks` + `on_dma_complete` | 同上 |
| `SimpleDisk::blockSize` | `block_size_` | 同语义 |
| `SimpleDisk::responseLatency` | `read_latency_cycles_` | 同语义 |
| `SimpleDisk::storage` | `block_storage_` | 同语义 |

## 6. 实施路径

### Phase 7+ 步骤

1. 新建 `include/tlm/io/simple_disk_tlm.hh`（~200 行）
2. 继承 DmaDeviceTLM
3. 实现 in-memory 块存储（v0）或 file backend（Phase 7+）
4. 加 Catch2 测试

**估计工作量**: 1-2 周

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **in-memory 存储内存占用**——16 GB 默认 | 中 | 中 | 默认 1 GB，配置可调 |
| R2 | **固定延迟不真实**——SSD/HDD 延迟变化大 | 中 | 中 | Phase 7+ 加延迟分布 |
| R3 | **NCQ 顺序优化缺失**——v0 简单 FIFO | 中 | 中 | 暴露 `set_ncq_depth()` |

## 8. 决策点

### D1 块大小
- **Q**: 默认块大小 4 KB 还是 512 B？
- **建议**: 4 KB（现代 SSD）
- **依赖**: 设备特性

### D2 存储后端
- **Q**: 默认 in-memory 还是文件？
- **建议**: in-memory（v0 简化）
- **依赖**: 持久化需求

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7+ (未来)**: 基础版实施
