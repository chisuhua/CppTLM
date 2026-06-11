# memory-memtlm 微架构文档

> **类别**: Memory > Simplified
> **状态**: ✅ 已实施
> **Header**: `include/tlm/memory_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:31`）
> **蓝图来源**: gem5 `src/mem/simple_mem.hh`（SimpleMemory 桩）
> **首版 commit**: v2.1 路径同步
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.2
> - Phase 7.D: [`roadmap.md`](../../../roadmap.md) §Phase 7.D (hbm_mode 扩展)

---

## 1. 设计目标

`MemoryTLM` 是 **v0 简化版主存模型**，作为 CacheTLM 下游验证 ChStream 完整数据通路。**与 gem5 对位**: `gem5::SimpleMemory`（更复杂的版本含吞吐/带宽/队列模型）。

**核心特性**（来自 `memory_tlm.hh:16-115`）：
- 读延迟 100 周期 / 写延迟 120 周期（**硬编码**）
- 行缓冲命中模拟：地址低 16KB (`addr & 0xF000 == 0`) 视为行命中
- 响应 `data` 恒为 **`0xDEADBEEF` 桩值**——不真实存储
- ChStream 单端口

## 2. 架构概览

```
        req_in_  (InputStreamAdapter<CacheReqBundle>)
            │
            ▼
   ┌──────────────────────────┐
   │ tick():                  │
   │  if (valid && ready):    │
   │    is_write → 120 cyc    │
   │    else → 100 cyc        │
   │    row_hit = (addr & 0xF000)==0
   │    build resp with       │
   │    data = 0xDEADBEEF     │
   │    resp_out_.write()     │
   │  adapter_->tick()        │
   └──────────────────────────┘
            │
            ▼
        resp_out_  (OutputStreamAdapter<CacheRespBundle>)
```

## 3. 接口（Public API）

```cpp
class MemoryTLM : public ChStreamModuleBase {
public:
    MemoryTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "MemoryTLM"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig&) override;

    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& resp_out();
    cpptlm::StreamAdapterBase* get_adapter() const;

    tlm_stats::StatGroup& stats();
    void dumpStats(std::ostream& os) const;
};
```

## 4. 行为流程

### 4.1 tick() 主循环

```cpp
void MemoryTLM::tick() {
    if (req_in_.valid() && req_in_.ready()) {
        const auto& req = req_in_.data();
        bool is_write = req.is_write.read();

        if (is_write) {
            ++stats_requests_write_;
            stats_latency_write_.sample(120);
        } else {
            ++stats_requests_read_;
            stats_latency_read_.sample(100);
        }

        uint64_t addr = req.address.read();
        bool row_hit = (addr & 0xF000) == 0;

        if (row_hit) ++stats_row_hits_;
        else ++stats_row_misses_;

        bundles::CacheRespBundle resp;
        resp.transaction_id.write(req.transaction_id.read());
        resp.data.write(0xDEADBEEF);  // 桩值！
        resp.is_hit.write(row_hit ? 1 : 0);
        resp.error_code.write(0);
        resp_out_.write(resp);
        req_in_.consume();
    }
    if (adapter_) adapter_->tick();
}
```

### 4.2 关键设计取舍

- **延迟仅统计**：`stats_latency_*.sample(N)` 记录但**未实际延迟响应**——单拍即返回（同 CacheTLM 简化）
- **行缓冲 16KB 硬编码**：`addr & 0xF000 == 0`（地址低 16KB 视为行命中）——**完全无真实存储**
- **data=0xDEADBEEF**：经典"dead beef" 桩值——让测试能识别"这是桩"
- **无 storage state**：v0 没有 `std::map<addr, data>`，**只是响应桩**

## 5. Bundle 字段使用

| 字段 | MemoryTLM 使用 |
|------|---------------|
| `transaction_id` | 透传到 resp |
| `is_write` | 决定延迟（120 vs 100） |
| `address` | **行缓冲命中判断**（低 16KB） |
| 其他 | 忽略 |

## 6. 统计

| 指标 | 类型 | 含义 |
|------|------|------|
| `stats_requests_read_` | Scalar | 读请求数 |
| `stats_requests_write_` | Scalar | 写请求数 |
| `stats_row_hits_` | Scalar | 行缓冲命中数 |
| `stats_row_misses_` | Scalar | 行缓冲未命中数 |
| `stats_latency_read_` | Distribution | 读延迟（v0 恒为 100） |
| `stats_latency_write_` | Distribution | 写延迟（v0 恒为 120） |
| `stats_row_buffer_hit_rate_` | **Formula** | `row_hits / (row_hits + row_misses)` |

**Formula 用例**：唯一在 v0 用 `Formula` 的模块——展示动态计算指标。
**路径**: `system.memory`

## 7. 蓝图（未来演进）

### 7.1 Phase 7.D 应用

调研 §4 Phase 3：MemoryTLM 升级为 `SimpleMemoryTLM`（吞吐/带宽/队列）+ `DRAMCtrlTLM`（bank/rank/页策略）：
- `set_bandwidth(GiB/s)` / `set_throughput(ops/cycle)`
- 真实存储（`std::map<addr, data>` 或 fixed-size buffer）
- 请求队列 + 仲裁（bank 冲突 / rank 并行）
- Phase 7.D 同步：接受 `ComputeReqBundle/RespBundle`（双 Bundle 注册解决 R5 风险）

### 7.2 蓝图增强（gap）

- `qos::MemCtrlTLM`（gem5 `src/mem/qos/`）— 优先级 + 公平调度
- `HBM_TLM`（HBM2Stack 风格）— 多通道
- `NVM_TLM`（NVMInterface 风格）— 非易失
- 真实延迟（`sample()` 后实际等 N 周期）
- **data 存储**：v0 完全无存储

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **data=0xDEADBEEF 桩值**——任何依赖 data 内容的下游测试将失败 | 高 | 中 | v0 显式标注"桩值"；v2.2 真实存储 |
| R2 | **延迟仅统计不真延迟** | 高 | 中 | v0 简化；同 CacheTLM 风险 |
| R3 | **行缓冲 16KB 硬编码** | 中 | 中 | v2.2 加 `set_row_buffer_size()` |
| R4 | **无 storage**——多次写同一地址不保留 | 高 | 中 | 同 R1 |
| R5 | 端口类型与 GPUTLM 不兼容（GPUTLM 用 `ComputeReqBundle/RespBundle`） | 高 | 中 | Phase 7.D 双 Bundle 注册（spec §6.1 推荐方案 C）；当前 GPUTLM 走 `CacheReqBundle` 子集绕过 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ✅ | `test/test_phase6_integration.cc` 端到端 |
| 端到端 (Cache→Xbar→Memory) | ✅ | `configs/single_cluster_soc.json` |
| 读/写延迟区分 | ✅ | 100/120 cycle 硬编码 |
| 行缓冲命中率 | ✅ | Formula 动态计算 |
| **真实存储** | ❌ 桩 | 见 R1 |
| **真实延迟** | ❌ 桩 | 见 R2 |

## 10. 修订历史

- **2026-04-12**: MemoryTLM 初版
- **2026-04-15**: Crossbar/Memory 集成测试
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B1 批次）
