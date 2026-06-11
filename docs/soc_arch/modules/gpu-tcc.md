# gpu-tcc 微架构文档

> **类别**: gpu > tcc
> **状态**: 🟡 规划中（Phase 7.D）
> **Header**: (规划) `include/tlm/gpu/tcc_tlm.hh`
> **注册**: (规划) `REGISTER_CHSTREAM` 扩展 `ModuleFactory::registerObject<tlm::TCC_TLM>("TCC_TLM")`
> **蓝图来源**: gem5 `src/mem/ruby/protocol/GPU_VIPER-TCC.sm`
> **首版 commit**: 🟡 蓝图（来自 spec §3.3 + plan §3.4）
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.3, §2.4
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §3.3
> - Plan: [`docs/superpowers/plans/2026-06-11-phase7a-gpu-infra.md`](../../superpowers/plans/2026-06-11-phase7a-gpu-infra.md) §3.4
> - 通用 GPU 概念: [gpu.common.md](./gpu.common.md)
> - 真实 CU: [gpu-compute_unit.md](./gpu-compute_unit.md)

---

## 1. 设计目标（规划）

`tlm::TCC_TLM` 是 Phase 7.D 引入的 **GPU Last-Level Cache**——承担 3 大职能：

1. **Aggregate GPU 请求**（TCP/SQC 上行流）
2. **写合并**（write coalescing，同一地址连续写合并）
3. **Snoop fan-in**（从 `CoherenceDomain` 接收 probe，转发给上游 TCP）

**核心架构**：使用 **`DualPortStreamAdapter`**（TCP 侧 ↔ Directory/Memory 侧），这是 CppTLM 现有 Adapter 形态对位 NICTLM 的**完美应用场景**。

**与 gem5 对位**: `gem5::GPU_VIPER_TCC_Controller`（来自 `src/mem/ruby/protocol/GPU_VIPER-TCC.sm`，~300+ 行 slicc 代码，CppTLM 用 C++ `switch` 表简化）。

## 2. 架构概览（规划）

```
                          ┌─────────────────────────┐
   (TCP/SQC 上行)         │      TCC_TLM             │   (CoherenceDomain probe)
   ComputeReq/Resp        │                         │   snoop fan-in
        │                 │   ┌─────────────────┐   │         │
        ▼                 │   │  Write Coalescer │   │         │
   ┌─────────────────┐    │   │  - per-address  │   │         │
   │ TCP side        │◄──►│   │    pending list │   │◄────────┘
   │ (Cache Bundle)  │    │   └─────────────────┘   │
   └─────────────────┘    │            │              │
        │                 │            ▼              │
        │                 │   ┌─────────────────┐   │
        │                 │   │  Coalesced       │   │
        │                 │   │  Request Buffer  │   │
        │                 │   │  (per-addr count)│   │
        │                 │   └─────────────────┘   │
        │                 │            │              │
        │                 │            ▼              │
        │                 │   ┌─────────────────┐   │
        │                 │   │  Forwarder       │   │
        │                 │   │  → MemoryTLM /   │   │
        │                 │   │    CoherenceDomain│   │
        │                 │   └─────────────────┘   │
        │                 └─────────────────────────┘
        │                              │
        ▼                              ▼
   CacheRespBundle              req → MemoryTLM
        │                              │
        └──────────◄──────────────────┘
                     response
```

### 2.1 端口（DualPortStreamAdapter 形态）

| 端口侧 | 类型 | 数量 | 角色 |
|--------|------|------|------|
| TCP side `req_in_` | `InputStreamAdapter<ComputeReqBundle>` | 1 | 接收 TCP/SQC 上行请求 |
| TCP side `resp_out_` | `OutputStreamAdapter<ComputeRespBundle>` | 1 | 发送响应回 TCP/SQC |
| Memory side `req_out_` | `OutputStreamAdapter<ComputeReqBundle>` | 1 | aggregate 后转发到 MemoryTLM |
| Memory side `resp_in_` | `InputStreamAdapter<ComputeRespBundle>` | 1 | 接收 MemoryTLM 响应 |
| 探针输入 | (来自 `CoherenceDomain::register_bridge` ) | 1 | 接收 snoop request |
| `adapter_` | `DualPortStreamAdapter<TCC_TLM, ComputeReq, ComputeResp, ComputeReq, ComputeResp>*` | 1 | ChStream 桥接 |

## 3. 接口（规划）

```cpp
namespace tlm {

// 写合并条目
struct PendingCoalescedWrite {
    uint64_t address;
    uint32_t size;
    uint64_t data;
    uint32_t count;           // 合并次数
    uint64_t first_issue_cycle;
    uint32_t kernel_id;       // 跨域一致性（Phase 7.D）
};

class TCC_TLM : public ChStreamModuleBase {
public:
    static constexpr uint32_t MAX_PENDING_COALESCED = 64;
    static constexpr uint32_t COALESCE_WINDOW = 8;  // 8 cycles 内合并

    explicit TCC_TLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "TCC_TLM"; }

    // === 配置 ===
    void on_config_loaded() override;
    void set_coalesce_window(uint32_t cyc) { coalesce_window_ = cyc; }
    void set_max_pending(uint32_t n) { max_pending_ = n; }

    // === TCP 侧访问器 ===
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>&   tcp_req_in() { return tcp_req_in_; }
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle>& tcp_resp_out() { return tcp_resp_out_; }

    // === Memory 侧访问器 ===
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>&   mem_req_out() { return mem_req_out_; }
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>&  mem_resp_in() { return mem_resp_in_; }

    // === ChStream 桥接 ===
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void do_reset(const ResetConfig& config) override;
    tlm_stats::StatGroup* get_stats_group() override;

    // === 探针接口 (Phase 7.D CoherenceDomain 集成) ===
    void receive_snoop_probe(uint64_t addr, const std::string& probe_type);

private:
    void handle_tcp_request(const ComputeReqBundle& req);
    bool try_coalesce(const ComputeReqBundle& req);
    void flush_coalesced_writes();
    void forward_to_memory(const ComputeReqBundle& req);

    // TCP 侧
    cpptlm::InputStreamAdapter<bundles::ComputeReqBundle>   tcp_req_in_;
    cpptlm::OutputStreamAdapter<bundles::ComputeRespBundle> tcp_resp_out_;

    // Memory 侧
    cpptlm::OutputStreamAdapter<bundles::ComputeReqBundle>   mem_req_out_;
    cpptlm::InputStreamAdapter<bundles::ComputeRespBundle>  mem_resp_in_;

    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // 写合并状态
    std::unordered_map<uint64_t, PendingCoalescedWrite> coalesce_table_;  // addr → write
    uint32_t coalesce_window_ = COALESCE_WINDOW;
    uint32_t max_pending_ = MAX_PENDING_COALESCED;
    uint32_t cycles_since_flush_ = 0;

    // inflight 跟踪
    std::unordered_map<uint64_t, uint64_t> inflight_txns_;  // txn_id → issue_cycle

    // 统计
    tlm_stats::Scalar requests_aggregated_;
    tlm_stats::Scalar writes_coalesced_;
    tlm_stats::Scalar snoop_probes_received_;
    tlm_stats::Distribution coalesce_efficiency_;  // avg count per coalesced write
};
}
```

## 4. 行为流程（规划）

### 4.1 tick() 4 阶段

```cpp
void TCC_TLM::tick() {
    // 1. 响应消费 (Memory 侧响应 + TCP 侧响应)
    if (mem_resp_in_.valid() && mem_resp_in_.ready()) {
        // ... 消费 + 转发到 tcp_resp_out_
    }

    // 2. TCP 请求处理
    if (tcp_req_in_.valid() && tcp_req_in_.ready()) {
        const auto& req = tcp_req_in_.data();
        handle_tcp_request(req);
        tcp_req_in_.consume();
    }

    // 3. 写合并 flush (周期性)
    if (cycles_since_flush_ >= coalesce_window_) {
        flush_coalesced_writes();
        cycles_since_flush_ = 0;
    }
    cycles_since_flush_++;

    // 4. Adapter tick
    if (adapter_) adapter_->tick();
}
```

### 4.2 handle_tcp_request()

```cpp
void TCC_TLM::handle_tcp_request(const ComputeReqBundle& req) {
    if (req.is_write.read()) {
        // 写 → 尝试合并
        if (try_coalesce(req)) {
            writes_coalesced_++;
            return;  // 不立即发到 Memory
        }
        // 合并失败（无空闲 slot / 新地址）→ 立即转发
    }

    // 读 / 未合并写 → 直接转发到 Memory
    forward_to_memory(req);
    inflight_txns_[req.transaction_id.read()] = getCurrentCycle();
    requests_aggregated_++;
}
```

### 4.3 try_coalesce

```cpp
bool TCC_TLM::try_coalesce(const ComputeReqBundle& req) {
    uint64_t addr = req.address.read();

    auto it = coalesce_table_.find(addr);
    if (it != coalesce_table_.end()) {
        // 合并到现有条目
        it->second.count++;
        it->second.data = req.data.read();  // 最新数据覆盖
        it->second.size = req.size.read();
        return true;
    }

    // 新地址 → 创建条目（若无空闲 slot 则 false）
    if (coalesce_table_.size() >= max_pending_) return false;

    PendingCoalescedWrite pcw;
    pcw.address = addr;
    pcw.size = req.size.read();
    pcw.data = req.data.read();
    pcw.count = 1;
    pcw.first_issue_cycle = getCurrentCycle();
    pcw.kernel_id = req.kernel_id.read();
    coalesce_table_[addr] = pcw;
    return true;
}
```

### 4.4 flush_coalesced_writes

```cpp
void TCC_TLM::flush_coalesced_writes() {
    for (auto& [addr, pcw] : coalesce_table_) {
        // 构造合并后的请求
        ComputeReqBundle req;
        req.transaction_id.write(next_txn_id_++);
        req.address.write(addr);
        req.size.write(pcw.size);
        req.is_write.write(true);
        req.data.write(pcw.data);
        req.kernel_id.write(pcw.kernel_id);
        req.workgroup_id.write(0);  // TCC 级不再区分 WG

        // 转发到 Memory
        forward_to_memory(req);
        inflight_txns_[req.transaction_id.read()] = getCurrentCycle();
        requests_aggregated_++;
        coalesce_efficiency_.sample(pcw.count);
    }
    coalesce_table_.clear();
}
```

### 4.5 receive_snoop_probe (Phase 7.D 协议集成)

```cpp
void TCC_TLM::receive_snoop_probe(uint64_t addr, const std::string& probe_type) {
    snoop_probes_received_++;

    // 简化：仅 invalidate 对应地址
    if (probe_type == "PrbInv" || probe_type == "PrbDowngrade") {
        // 检查 coalesce_table_ 中是否有该地址
        coalesce_table_.erase(addr);
        // 注：v0 不维护真实 cache 行状态
    }
}
```

## 5. Bundle 字段使用（规划）

**ComputeReqBundle / ComputeRespBundle 字段**（与 GPUTLM v0 一致）：

| 字段 | TCC_TLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——`inflight_txns_` 映射键 |
| `address` | **关键**——coalesce_table_ 查表键 |
| `is_write` | 决定走 coalesce 或 direct forward |
| `data` | coalesce 时取最新值 |
| `size` | 透传 |
| `kernel_id` | 跨域一致性（Phase 7.C 集成） |

## 6. 蓝图对齐

- gem5 `src/mem/ruby/protocol/GPU_VIPER-TCC.sm`（TCC 状态机）
- gem5 `src/mem/ruby/protocol/GPU_VIPER.slicc`（协议构成 + TCC 取代 L3 注释）
- gem5 `src/mem/ruby/structures/`（Ruby 通用结构）
- spec §3.3（TCC 蓝图章节）
- plan §3.4（Phase 7.D 实施路径）

## 7. 实施路径

### 7.1 Phase 7.D 步骤

1. 新建 `include/tlm/gpu/tcc_tlm.hh`（~300 行）
2. 修改 `include/chstream_register.hh`：
   - 加 `#include "tlm/gpu/tcc_tlm.hh"`
   - 加 `ModuleFactory::registerObject<tlm::TCC_TLM>("TCC_TLM");`
   - 加 `ChStreamAdapterFactory::registerDualPortAdapter<tlm::TCC_TLM, ComputeReqBundle, ComputeRespBundle, ComputeReqBundle, ComputeRespBundle>("TCC_TLM");`
3. 写 `try_coalesce` + `flush_coalesced_writes`（写合并核心）
4. 写 `receive_snoop_probe`（Phase 7.C 协议集成）
5. **关键：修复 R5 风险**——`MemoryTLM` 双 Bundle 注册（同时接受 `ComputeReqBundle/RespBundle`）
6. 加 Catch2 测试：`test/test_tcc_tlm.cc`（`[gpu]` 标签），覆盖：
   - 单写合并
   - 多地址合并
   - flush 周期
   - snoop probe 触发
7. 更新 `AGENTS.md` + `docs/ONBOARDING.md`（TCC_TLM 注册条目）
8. 更新 `docs/soc_arch/modules/README.md`（新增 `gpu-tcc.md` 链接）
9. 更新 `docs/soc_arch/modules/memory-memtlm.md`（修复 R5）

### 7.2 验收标准

- [ ] 编译通过（Release + Debug）
- [ ] `cpptlm_tests "[gpu]"` 全部通过
- [ ] `cpptlm --config configs/tcc_test.json` 端到端可执行
- [ ] `docs_sync_check.sh --strict` 通过
- [ ] 零 TODO/FIXME/XXX in new files
- [ ] 写合并效率 = `coalesce_efficiency.mean() > 1.0`（多地址场景）
- [ ] **R5 风险修复**——`MemoryTLM` 端到端连接 GPUTLM 真正可工作

### 7.3 估计工作量

- 设计: 1-2 周（写合并 + 协议集成双重复杂度）
- 实施: 2-3 周
- 测试: 1 周
- 文档: 0.5 周
- **总计: 4.5-6.5 周**（Phase 7.D 是 4 个子阶段中最重的一个）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **R5 风险**——MemoryTLM 不接受 ComputeReqBundle | 高 | 高 | Phase 7.D 同步修复：MemoryTLM 双 Bundle 注册 |
| R2 | **写合并 flush 时机**——`coalesce_window_=8` 硬编码 | 中 | 中 | v0 简化；on_config_loaded 可覆盖 |
| R3 | **`coalesce_efficiency_` 仅在 flush 时更新** | 低 | 低 | 接受 |
| R4 | **无真实 cache 行状态**——仅按地址合并 | 中 | 中 | D2 决策接受；Phase 7.F+ 真实 cache |
| R5 | **snoop fan-in 极简**——仅 erase 对应地址 | 中 | 中 | v0 简化；Phase 7.C 协议完整实现 |
| R6 | **`inflight_txns_` 无限增长** | 低 | 中 | v0 接受；v2.2 加 `MAX_INFLIGHT` |
| R7 | **D2 决策缺口**——5-stage pipeline 不模拟 | 中 | 低 | 与 GPUTLM v0 决策一致 |
| R8 | **跨域 CoherenceDomain 集成**——Phase 7.D 同步 Phase 7.C | 中 | 中 | D5 决策接受；接口预留 |

## 9. 设计决策点

### D1 写合并粒度

- **Q**: 写合并是按地址（同一地址连续写合并），还是按 flit（同一 packet 写合并）？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 按地址（与真实 GPU TCC 行为一致）
- **依赖**: gem5 `TCC.sm` 行为

### D2 coalesce_window 触发

- **Q**: 按 cycle 数（coalesce_window_=8）还是按 byte 数？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: cycle 数（v0 简化）
- **依赖**: 与真实 GPU 行为对比

### D3 snoop probe 行为

- **Q**: receive_snoop_probe 触发后，erase 对应地址？还是发送 invalidate response？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: erase（v0 简化，snoop fan-in 无响应回送）
- **依赖**: Phase 7.C CoherenceDomain snoop filter 真实实现

### D4 dual_port 注册时序

- **Q**: TCC_TLM 的 DualPortStreamAdapter 注册与 NICTLM 的关系（共用工厂）？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: 独立注册（同 NICTLM 模式）
- **依赖**: `ChStreamAdapterFactory::registerDualPortAdapter` 模板

### D5 与 CacheTLM 关系

- **Q**: TCC_TLM 是不是 CacheTLM 的 GPU 特化？两者代码是否可共享？
- **状态**: 留待 Phase 7.D 设计时确定
- **建议**: TCC_TLM 是独立模块（实现 Coalescing + Snoop，CacheTLM 不实现）
- **依赖**: Phase 7.C CacheTLM protocol-aware 改造

## 10. 修订历史

- **2026-06-11**: 蓝图初版（来自 spec §3.3 + plan §3.4）
- **2026-06-11**: B3 批次设计 — 提取 D1-D5 + 蓝图对齐 + R5 风险标注
- **Phase 7.D (未来)**: 实施 TCC_TLM + 修复 R5（MemoryTLM 双 Bundle 注册）
- **Phase 7.F+ (未来)**: 真实 cache 行状态 + 协议集成完整实现
