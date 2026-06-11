# noc-nic 微架构文档

> **类别**: NoC > NIC
> **状态**: ✅ 已实施
> **Header**: `include/tlm/nic_tlm.hh`
> **注册**: `REGISTER_CHSTREAM`（`include/chstream_register.hh:38`）
> **蓝图来源**: gem5 `src/mem/ruby/network/NetworkInterface.hh`（Garnet 2.0）
> **首版 commit**: v2.1 路径同步
> **最近更新**: 2026-06-11
> **维护者**: CppTLM Team

> **关联文档**:
> - 索引: [README.md](./README.md)
> - 调研: [`docs/research-cpptlm-gpu-fused-soc-survey.md`](../../research-cpptlm-gpu-fused-soc-survey.md) §2.4
> - Spec: [`docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md`](../../superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md) §8

---

## 1. 设计目标

`tlm::NICTLM` 是 **NoC 网络接口卡**，承担 **PE 端 (Cache Bundle) ↔ NoC 端 (Flit Bundle) 双向协议转换**。**与 gem5 对位**: `gem5::NetworkInterface`（Garnet 2.0 / NetworkBridge）。

**核心特性**（来自 `nic_tlm.hh:81-170`）：
- **4 端口非对称**：
  - PE 侧：`pe_req_in_` (CacheReq) + `pe_resp_out_` (CacheResp)
  - Network 侧：`net_req_out_` (NoCFlit) + `net_resp_in_` (NoCFlit)
- **`FLITS_PER_PACKET = 4`** 切分（Packet → 4 个 flit）
- **`MAX_PENDING_PACKETS = 16`** 重组缓冲
- **`AddressMap`**: 地址 → 节点 ID 映射
- 5 个 StatGroup 指标

## 2. 架构概览

### 2.1 端口拓扑

```
   PE 侧 (Cache Bundle)                    Network 侧 (NoC Flit Bundle)
   ──────────────                          ──────────────
         │                                          ▲
         ▼                                          │
   ┌─────────────────────────────────────────────┐
   │ pe_req_in_      packetize      net_req_out_ │
   │              ─────────────►                  │
   │                                              │──► to RouterTLM (LOCAL)
   │                                              │
   │              ◄─────────────  reassemble     │
   │ pe_resp_out_                 net_resp_in_  │
   │                                              │◄── from RouterTLM
   └─────────────────────────────────────────────┘
            │                      ▲
            ▼                      │
       CacheTLM /              (downstream response
       CPUTLM                 flits reassembled)
```

### 2.2 内部结构

```
   ┌───────────────────────────────┐
   │  NICTLM                        │
   │                                │
   │  addr_map_: AddressMap         │
   │   └─ regions_: vector<...>     │
   │                                │
   │  pending_packets_: vector<>    │──► 重组缓冲（up to 16）
   │  pending_flit_queue_: queue<>   │──► 发送缓冲
   │                                │
   │  handle_pe_req()               │──► packetize 4 flits
   │  handle_net_resp()             │──► reassemble → resp
   │                                │
   │  tick() 4 阶段:                │
   │   1. handle_pe_req()           │
   │   2. 发送 net_req_out_         │
   │   3. handle_net_resp()         │
   │   4. 发送 pe_resp_out_         │
   └───────────────────────────────┘
```

## 3. 接口（Public API）

```cpp
namespace tlm {
struct AddressRegion {
    uint64_t base_addr;
    uint64_t size;
    uint32_t target_node;
    std::string target_type;
};

class AddressMap {
public:
    void add_region(uint64_t base, uint64_t size, uint32_t node,
                    const std::string& type = "MEMORY_CTRL");
    uint32_t lookup_node(uint64_t addr) const;
    // node_to_coord / coord_to_node 辅助
};

class NICTLM : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_VCS = 4;
    static constexpr unsigned FLITS_PER_PACKET = 4;
    static constexpr unsigned MAX_PENDING_PACKETS = 16;

    NICTLM(const std::string& name, EventQueue* eq,
           uint32_t node_id = 0, uint32_t mesh_x = 4, uint32_t mesh_y = 4);

    std::string get_module_type() const override { return "NICTLM"; }
    unsigned num_ports() const override { return 4; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override;
    void tick() override;
    void on_config_loaded() override;

    // PE 侧访问器
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>&   pe_req_in();
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>&  pe_resp_out();

    // Network 侧访问器
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>&   net_req_out();
    cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>&    net_resp_in();

    // 地址映射
    void add_address_region(uint64_t base, uint64_t size, uint32_t node,
                            const std::string& type = "MEMORY_CTRL");
    uint32_t lookup_node(uint64_t addr) const;
};
}
```

**硬编码常量**：

| 常量 | 值 | 含义 |
|------|----|------|
| `NUM_VCS` | 4 | 虚拟通道数 |
| `FLITS_PER_PACKET` | 4 | 单 packet 切 4 个 flit |
| `MAX_PENDING_PACKETS` | 16 | 重组缓冲最大数 |
| `mesh_x` / `mesh_y` 默认 | 4×4 | mesh 维度（与 RouterTLM 对位） |

## 4. 行为流程

### 4.1 tick() 4 阶段（基于 `nic_tlm.hh:131-132` 的 helper 方法）

```cpp
void NICTLM::tick() {
    // 阶段 1: PE 端请求 → packetize
    if (handle_pe_req()) {
        // 创建 PendingPacket, 切 4 个 flit
    }

    // 阶段 2: 发送 net_req_out_ (pending_flit_queue_ 弹出)
    flush_pending_flits();

    // 阶段 3: Network 端响应 → reassemble
    if (handle_net_resp()) {
        // PendingPacket 累积 flits_received, 满了 → 重组 CacheResp
    }

    // 阶段 4: 发送 pe_resp_out_ (重组完成)
    flush_completed_responses();
}
```

### 4.2 packetize 流程（v0 高层伪代码）

```cpp
void NICTLM::packetize(const CacheReqBundle& req) {
    PendingPacket pp;
    pp.transaction_id = req.transaction_id.read();
    pp.dst_node = addr_map_.lookup_node(req.address.read());
    pp.is_write = req.is_write.read();
    pp.address = req.address.read();
    pp.flits_received = 0;

    for (uint8_t i = 0; i < FLITS_PER_PACKET; i++) {
        NoCFlitBundle flit;
        flit.transaction_id.write(req.transaction_id.read());
        flit.src_node.write(node_id_);
        flit.dst_node.write(pp.dst_node);
        flit.address.write(req.address.read());
        flit.data.write(req.data.read());
        flit.flit_index.write(i);
        flit.flit_count.write(FLITS_PER_PACKET);
        flit.is_write.write(req.is_write.read());
        pp.flits[i] = flit;
    }

    if (pending_packets_.size() < MAX_PENDING_PACKETS) {
        pending_packets_.push_back(pp);
    }
}
```

### 4.3 reassemble 流程

```cpp
bool NICTLM::reassemble(const NoCFlitBundle& flit) {
    uint64_t txn_id = flit.transaction_id.read();
    auto it = std::find_if(pending_packets_.begin(),
                           pending_packets_.end(),
                           [txn_id](const PendingPacket& p) {
                               return p.transaction_id == txn_id;
                           });
    if (it == pending_packets_.end()) return false;

    uint8_t idx = flit.flit_index.read();
    it->flits[idx] = flit;
    it->flits_received++;

    if (it->flits_received == FLITS_PER_PACKET) {
        // 重组完成，构造 CacheRespBundle
        CacheRespBundle resp;
        resp.transaction_id.write(txn_id);
        resp.data.write(it->flits[0].data.read());  // 取首 flit data
        resp.is_hit.write(1);
        resp.error_code.write(0);
        pe_resp_out_.write(resp);
        pending_packets_.erase(it);
        return true;
    }
    return false;
}
```

## 5. Bundle 字段使用

### 5.1 PE 侧（`CacheReqBundle` / `CacheRespBundle`）

| 字段 | NICTLM 使用 |
|------|---------------|
| `transaction_id` | **关键**——跨侧 ID 关联 |
| `address` | `addr_map_.lookup_node()` 决定 dst_node |
| `is_write` | 透传到 flit |
| `data` | 透传到 flit |
| 其他 | 忽略 |

### 5.2 Network 侧（`NoCFlitBundle`）

| 字段 | NICTLM 使用 |
|------|---------------|
| `transaction_id` | 关联到 PendingPacket |
| `src_node` | 设为 `node_id_` |
| `dst_node` | 由 `addr_map_` 决定 |
| `address` / `data` | 透传 |
| `flit_index` / `flit_count` | 切分 / 重组 |
| `is_write` | 透传 |
| 其他 | 透传 |

## 6. 统计

| 指标 | 类型 | 含义 |
|------|------|------|
| `stats_flits_sent_` | Scalar | 发送的 flit 总数 |
| `stats_flits_received_` | Scalar | 接收的 flit 总数 |
| `stats_packets_sent_` | Scalar | 发送的 packet 总数 |
| `stats_packets_received_` | Scalar | 接收的 packet 总数 |
| `stats_latency_` | Distribution | Packet 端到端延迟 |

**路径**: `system.nic_<node_id>`

## 7. 蓝图（未来演进）

### 7.1 Phase 7.E 应用

调研 §2.4 + Phase 7.E：NICTLM 在 GPU 内部 mesh 中连接 CU ↔ RouterTLM：
- PE 侧接 `ComputeUnitTLM`（Phase 7.B）
- Network 侧接 `RouterTLM`（已存在）
- `AddressMap` 加入 GPU 内存区域（如 0x10000+ → GPU node 0）

### 7.2 蓝图增强

- **多 packet 重组**（v0 仅 1 packet 粒度）
- **VC 选择**（v0 `NUM_VCS=4` 常量未实际用）
- **flit 错误检测**（CRC / parity）
- **真实延迟**（packetize 耗时）
- **双 Bundle 兼容**（Phase 7.D 需 `ComputeReqBundle/RespBundle` 兼容方案）

## 8. 风险与缓解

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|------|------|------|
| R1 | **`MAX_PENDING_PACKETS=16`**——背压场景可能丢包 | 中 | 中 | v2.2 加 back-pressure 反馈或动态扩容 |
| R2 | **`reassemble` 顺序假设**——乱序到达的 flit 会被 `std::find_if` 多次扫描 | 中 | 中 | v2.2 用 `unordered_map<txn_id, PendingPacket>` 替代 `vector` |
| R3 | **`AddressMap::lookup_node` 简单线性扫描**——O(N) | 中 | 低 | N 通常 ≤ 16，复杂度可接受；v2.2 改区间树 |
| R4 | **NUM_VCS=4 硬编码且未使用**——`net_req_out_` 不分配 VC | 中 | 中 | v2.2 实现 VC 分配（v0 仅声明） |
| R5 | **数据仅取首 flit `data`**——多 flit 数据未完整重组 | 高 | 中 | v2.2 修复：聚合多 flit data |
| R6 | **`on_config_loaded` 与 GPUTLM 同类缺口**——`add_address_region` 仅程序化 API | 中 | 中 | Phase 7.B 统一修复 |

## 9. 验收

| 项 | 状态 | 证据 |
|----|------|------|
| 编译（Release） | ✅ | `cmake --build build` 通过 |
| 单测覆盖 | ✅ | `test/test_router_tlm.cc` 系列 + mesh 2x2/4x4 集成 |
| 端到端 (CPU↔NIC↔Router) | ✅ | `configs/mesh_2x2_tlm.json` / `mesh_4x4_tlm.json` |
| Packet 切分/重组 | ✅ | `FLITS_PER_PACKET=4` 验证 |
| 地址映射 | ✅ | `AddressMap` 真实实现 |
| 5 个统计 | ✅ | `stats_flits_sent_/received_/packets_sent_/received_/latency_` |
| **多 flit data 完整重组** | ⚠️ v0 仅首 flit | 见 R5 |

## 10. 修订历史

- **2026-04-24**: NICTLM 初版（`include/tlm/nic_tlm.hh`）
- **2026-04-26**: `DualPortStreamAdapter` 集成
- **2026-04-30**: `AddressMap` 真实实现
- **2026-06-08**: v2.1 Release 标签
- **2026-06-11**: 本微架构文档创建（B2 批次）
