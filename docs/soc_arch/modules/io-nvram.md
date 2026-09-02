# io-nvram 微架构文档

> **类别**: io > nvram · **状态**: 🟡 规划中 + 📋 v1.0 dGPU SoC 战略补充
> **状态**: 🟡 规划中
> **Header**: (规划) `include/tlm/io/nvram_tlm.hh`
> **蓝图来源**: gem5 `src/mem/noncoherent_xbar.hh` + NVRAM 设备仿真（无直接 gem5 对位，参考 Optane persistent memory 模型）
> **首版 commit**: 蓝图（来自调研 §2.5）
> **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.5
> - 邻接: [memory-memtlm.md](./memory-memtlm.md) (DRAM) | [io-disk.md](./io-disk.md) (块设备) | [coherence-protocol.md](./coherence-protocol.md)

---

## 1. 设计目标（蓝图）

`tlm::NvramTLM` 是 CppTLM v2.2+ 规划的 **非易失性 RAM 设备**——字节可寻址持久内存，介于 DRAM 和块设备之间（典型 Intel Optane / 3D XPoint）。**与 gem5 对位**: gem5 无直接 NVRAM 设备（Optane persistent memory 通过 DRAM 模拟），本模块提供**独立模块**填补此空缺。

**核心特征**：
- **字节可寻址**（与 DRAM 同接口，可直接 mmap）
- **持久性**（仿真断电保留）
- **可配延迟**（DRAM-like ~100 ns，比 SSD 快）
- **可选写穿/写回策略**
- **可选 CPU cache flush 语义**（CLFLUSH / CLWB）

## 2. 架构概览

```
┌─────────────────────────────────────────────────────────────┐
│                  NvramTLM 简化                               │
│                                                             │
│  ┌──────────────┐                                            │
│  │  ReqIn       │                                            │
│  │  (CacheReq)  │                                            │
│  └──────┬───────┘                                            │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  write_buffer_ (持久化队列)                       │     │
│  │    - write-back 模式: 写先入 buffer，再 flush     │     │
│  │    - write-through 模式: 立即写                   │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────────────────────────────────────────┐     │
│  │  nvram_storage_                                   │     │
│  │    - in-memory 模拟（v0）                          │     │
│  │    - file-backed 持久化（Phase 7+）                │     │
│  │    - 容量：典型 128 GB - 1 TB                     │     │
│  └──────────────────────────────────────────────────┘     │
│         │                                                    │
│         ▼                                                    │
│  ┌──────────────┐                                            │
│  │  RespOut     │                                            │
│  └──────────────┘                                            │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 与 DRAM/Disk 的对比

| 维度 | DRAM (MemoryTLM) | NVRAM (NvramTLM) | Disk (SimpleDiskTLM) |
|------|------------------|-------------------|----------------------|
| **访问粒度** | 字节 | 字节 | 块（4 KB） |
| **延迟** | 100 ns | 300-1000 ns | 100 μs |
| **持久性** | ❌ 断电丢失 | ✅ 持久 | ✅ 持久 |
| **CPU 访问** | load/store | load/store (clflush) | DMA only |
| **典型容量** | 16-64 GB | 128 GB - 1 TB | 1-16 TB |

### 2.2 端口表

| 端口 | 类型 | 角色 |
|------|------|------|
| `req_in_` | `InputStreamAdapter<CacheReqBundle>` | 接收请求 |
| `resp_out_` | `OutputStreamAdapter<CacheRespBundle>` | 发送响应 |
| `flush_in_` | `InputStreamAdapter<FlushBundle>` | 接收 cache flush（CLFLUSH） |

### 2.3 内部结构

```
┌────────────────────────────────────────────────────────────┐
│                  NvramTLM 内部                              │
│                                                             │
│  配置:                                                      │
│    - read_latency_: 300 ns (default)                       │
│    - write_latency_: 1000 ns (default)                     │
│    - capacity_bytes_: 256 GB (default)                      │
│    - write_policy_: WRITETHROUGH / WRITEBACK               │
│    - persistence_file_: optional (Phase 7+)                 │
│                                                             │
│  状态:                                                      │
│    - write_buffer_: std::deque<WriteEntry>                 │
│    - nvram_storage_: std::vector<uint8_t>                  │
│    - dirty_blocks_: std::set<uint64_t>                      │
│                                                             │
│  特殊语义:                                                  │
│    - clflush(addr): 强制 write-buffer 刷到 storage          │
│    - clwb(addr): write-back hint                            │
│    - sfence: 全局写顺序保证                                 │
└────────────────────────────────────────────────────────────┘
```

## 3. 接口（规划）

```cpp
namespace tlm {

enum class NvramWritePolicy { WRITE_THROUGH, WRITE_BACK };

class NvramTLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t DEFAULT_READ_LATENCY = 300;   // 300 ns
    static constexpr uint32_t DEFAULT_WRITE_LATENCY = 1000;  // 1 μs
    static constexpr uint64_t DEFAULT_CAPACITY = 256ULL * 1024 * 1024 * 1024;  // 256 GB

    explicit NvramTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "NvramTLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_read_latency(uint32_t cycles) { read_latency_ = cycles; }
    void set_write_latency(uint32_t cycles) { write_latency_ = cycles; }
    void set_capacity(uint64_t bytes) { capacity_bytes_ = bytes; }
    void set_write_policy(NvramWritePolicy p) { write_policy_ = p; }
    void set_persistence_file(const std::string& path) {
        persistence_file_ = path;
        load_from_file();
    }

    // === 持久性语义 ===
    void clflush(uint64_t addr);   // 强制刷 dirty
    void clwb(uint64_t addr);      // write-back hint
    void sfence();                 // 全局写顺序

    // === 端口 ===
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();
    cpptlm::InputStreamAdapter<bundles::FlushBundle>& flush_in();

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

private:
    void write_through(const CacheReqBundle& req);
    void write_back(const CacheReqBundle& req);
    void flush_dirty_block(uint64_t block_addr);
    void save_to_file();
    void load_from_file();

    // 配置
    uint32_t read_latency_;
    uint32_t write_latency_;
    uint64_t capacity_bytes_;
    NvramWritePolicy write_policy_;
    std::string persistence_file_;

    // 状态
    std::vector<uint8_t> nvram_storage_;
    std::map<uint64_t, uint64_t> pending_responses_;
    std::deque<uint64_t> write_buffer_;
    std::set<uint64_t> dirty_blocks_;
    uint64_t clflush_count_;
    uint64_t clwb_count_;
    uint64_t sfence_count_;

    // 统计
    tlm_stats::Scalar reads_;
    tlm_stats::Scalar writes_;
    tlm_stats::Scalar bytes_read_;
    tlm_stats::Scalar bytes_written_;
    tlm_stats::Scalar clflush_ops_;
    tlm_stats::Scalar clwb_ops_;
    tlm_stats::Scalar sfence_ops_;
    tlm_stats::Average write_buffer_depth_;
    tlm_stats::Distribution read_latency_actual_;
    tlm_stats::Distribution write_latency_actual_;
};

}  // namespace tlm
```

## 4. 行为流程

```cpp
void NvramTLM::tick() {
    uint64_t now = current_cycle();

    // 1. 派发到期的 responses
    dispatch_pending_responses(now);

    // 2. 处理 flush 请求
    if (flush_in_.valid() && flush_in_.ready()) {
        const auto& flush = flush_in_.data();
        switch (flush.type) {
            case FlushType::CLFLUSH:
                clflush(flush.addr);
                ++clflush_ops_;
                break;
            case FlushType::CLWB:
                clwb(flush.addr);
                ++clwb_ops_;
                break;
            case FlushType::SFENCE:
                sfence();
                ++sfence_ops_;
                break;
        }
        flush_in_.consume();
    }

    // 3. 处理新请求
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        if (req.is_write.read()) {
            if (write_policy_ == NvramWritePolicy::WRITE_THROUGH) {
                write_through(req);
            } else {
                write_back(req);
            }
            ++writes_;
            bytes_written_ += req.size.read();
        } else {
            uint64_t addr = req.address.read();
            uint64_t depart = now + read_latency_;
            pending_responses_[req.transaction_id.read()] = depart;
            ++reads_;
            bytes_read_ += req.size.read();
        }
        req_in_.consume();
    }

    // 4. write-back 模式: 周期刷 dirty block
    if (write_policy_ == NvramWritePolicy::WRITE_BACK) {
        while (!write_buffer_.empty() && now - last_flush_cycle_ > 1000) {
            flush_dirty_block(write_buffer_.front());
            write_buffer_.pop_front();
        }
    }

    write_buffer_depth_.sample(write_buffer_.size());
    if (adapter_) adapter_->tick();
}

void NvramTLM::clflush(uint64_t addr) {
    uint64_t block = addr & ~0x3FULL;  // 64 B cache line
    if (write_policy_ == NvramWritePolicy::WRITE_BACK) {
        // 强制刷 block
        flush_dirty_block(block);
        write_buffer_.erase(std::remove(write_buffer_.begin(),
                                        write_buffer_.end(), block),
                            write_buffer_.end());
    }
    dirty_blocks_.erase(block);
}
```

## 5. 蓝图对齐

> **注**: gem5 无直接 NVRAM 设备。本模块参考：
> - Intel Optane Persistent Memory 仿真模型
> - gem5 `src/mem/noncoherent_xbar.hh` (非一致 crossbar)
> - 学术界 NVRAM 仿真论文（ATLAS、Wisp 等）

| 参考 | CppTLM 对应 | 差异 |
|------|------------|------|
| Optane 物理层 | `nvram_storage_` | 抽象存储（无 3D XPoint 细节） |
| clflush / clwb 指令 | `clflush()` / `clwb()` | 同语义 |
| write-back hint | `write_policy_ = WRITE_BACK` | 同语义 |
| 文件持久化 | `persistence_file_` | Phase 7+ 实施 |

## 6. 实施路径

### Phase 7+ 步骤

1. 新建 `include/tlm/io/nvram_tlm.hh`（~250 行）
2. 新建 `FlushBundle` 在 `include/bundles/`
3. 实现 in-memory 存储（v0）
4. 实现 write-back 写缓冲
5. 实现 clflush / clwb / sfence 语义
6. 加 Catch2 测试
7. Phase 7+：实现 file backend 持久化

**估计工作量**: 2-3 周

## 7. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **持久性语义模拟**——v0 断电保留不真实 | 中 | 中 | Phase 7+ 真实 file backend |
| R2 | **clflush 频率过高**——性能开销大 | 中 | 中 | 写合并（同地址合并 flush） |
| R3 | **大内存默认**——256 GB 默认过大 | 中 | 中 | 默认 1 GB，配置可调 |
| R4 | **coherence 与 cache 冲突**——CPU cache 与 NVRAM 一致性 | 中 | 中 | 文档明确：v0 NVRAM 直连 CPU cache（无 crossbar） |
| R5 | **持久化格式**——v0 无标准格式 | 中 | 中 | Phase 7+ 标准化（JSON header + binary） |

## 8. 决策点

### D1 默认 write policy
- **Q**: 默认 write-through 还是 write-back？
- **建议**: write-through（v0 简化，性能更可预测）
- **依赖**: 应用场景

### D2 默认延迟
- **Q**: read/write 延迟默认值？
- **建议**: read=300ns, write=1000ns（典型 Optane）
- **依赖**: 设备 spec

### D3 clflush 行为
- **Q**: clflush 立即生效还是延迟？
- **建议**: 立即生效（CPU 等待 clflush 完成）
- **依赖**: CPU 流水线模型

### D4 持久化后端
- **Q**: v0 持久化还是 Phase 7+ 实施？
- **建议**: v0 无持久化（in-memory only）；Phase 7+ 加 file backend
- **依赖**: 复杂度 vs 真实度

## 9. 修订历史
- **2026-06-12**: B3 批次蓝图初版（来自调研 §2.5）
- **Phase 7+ (未来)**: 基础版实施（write-back + clflush）
- **Phase 7+ (未来)**: file backend 持久化
