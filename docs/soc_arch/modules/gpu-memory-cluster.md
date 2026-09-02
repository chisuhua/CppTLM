# gpu-memory-cluster 微架构文档

> **类别**: GPU > 内存子系统 · **状态**: ✅ Phase 8.A Task 2 + 📋 v1.0 dGPU SoC HBM3e 补充(per [`ADR-SOC-09`](../../adr/ADR-SOC-09-v1-nvidia-amd-dual-vendor.md) D5 + L7 Memory 子系统架构)
> **Header**: `include/tlm/gpu/memory_cluster_tlm.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`)
> **蓝图来源**: NVIDIA HBM/GDDR 多通道内存控制器
> **首版 commit**: `6410ea9` (2026-06-28) · **最近更新**: 2026-07-02

> **关联文档**:
> - 索引: [README.md](./README.md)
> - Spec: [`docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md`](../../superpowers/specs/2026-06-24-gpu-soc-architecture.md) §3.2
> - 已有内存文档: [`memory-memtlm.md`](./memory-memtlm.md), [`memory-simple.md`](./memory-simple.md)

---

## 1. 设计目标

`MemoryClusterTLM` 是 GPU 端 **HBM/GDDR 多通道内存控制器简化模型**，实现 round-robin channel 分配。按 ADR-NV-01 D2 决策，不模拟真实 DRAM 调度（bank/rank/页策略），仅建模 channel 级并行度对带宽的影响。

**核心特性**:
- 继承 `ChStreamModuleBase`，单端口
- 多通道 round-robin channel 分配
- 容量管理（GB 级）
- 统计：已完成请求数（`requests_completed`）

## 2. 架构概览

### 2.1 内部结构

```
    MemoryClusterTLM (channels=4, capacity_gb=8)
         │
    ┌────┴────┐
    │  allocate_channel(request_id):
    │    ch = rr_counter_ % channels_
    │    rr_counter_++
    │    return ch
    │
    │  tick():
    │    requests_completed_++
    │    adapter_->tick()
    └─────────┘
         │
    adapter_ → GpuMeshNoC → CU
```

### 2.2 Round-robin 通道分配示例

```
channels = 4

request_id=0 → channel 0
request_id=1 → channel 1
request_id=2 → channel 2
request_id=3 → channel 3
request_id=4 → channel 0  (round-robin wrap)
```

### 2.3 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|:---:|------|
| `adapter_` | `StreamAdapterBase*` | 1 | ChStream 桥接 |

## 3. 接口（Public API）

```cpp
class MemoryClusterTLM : public ChStreamModuleBase {
public:
    explicit MemoryClusterTLM(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "MemoryClusterTLM"; }

    // Round-robin 分配
    uint32_t allocate_channel(uint64_t request_id);

    // Setter（JSON 解析后注入）
    void set_channels(uint32_t channels);
    void set_capacity_gb(uint32_t capacity_gb);

    uint32_t get_channels() const;
    uint32_t get_capacity_gb() const;
    uint64_t requests_completed() const;

    void tick() override;
};
```

## 4. 配置参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|:---:|------|
| `channels` | uint32 | 4 | 内存通道数，GB203: 4 |
| `capacity_gb` | uint32 | 8 | 显存容量 (GB)，GB203: 8-72 GB |

### 不同 SKU 的典型配置

| SKU | channels | capacity_gb | 内存类型 |
|-----|:---:|:---:|------|
| GB203 消费级 | 4 | 8 | GDDR7 |
| GB200 数据中心 | 12 | 192 | HBM3e |
| GH100 Hopper | 8 | 80 | HBM3 |
| GA100 Ampere | 8 | 80 | HBM2e |

## 5. 测试覆盖

| 测试文件 | 标签 | 测试数 | 覆盖内容 |
|------|------|:---:|------|
| `test/test_memory_cluster_tlm.cc` | `[gpu][memcluster][phase8a]` | 8 | round-robin channel / getter / tick cycle |

**关键测试用例**:
```cpp
TEST_CASE("MemoryClusterTLM: 4-channel round-robin", "[gpu][memcluster]") {
    MemoryClusterTLM mc("mc", nullptr, 4, 8);
    REQUIRE(mc.allocate_channel(0) == 0);
    REQUIRE(mc.allocate_channel(3) == 3);
    REQUIRE(mc.allocate_channel(4) == 0);  // wrap
}
```

## 6. 与现有多通道 MemoryCluster 的关系

| 特性 | MemoryClusterTLM (Phase 8.A) | MemoryCluster (SimModule) |
|------|------------------------------|---------------------------|
| 派生类 | ChStreamModuleBase | SimModule |
| 端口 | 1 (简化) | N (多端口 + Arbiter) |
| 通道管理 | round-robin 计算 | 多 MemoryTLM 实例 + Arbiter |
| 注册 | REGISTER_CHSTREAM | REGISTER_MODULE |
| 用途 | GPU 显存控制器 | 通用多通道内存聚合 |

## 7. 已识别限制 (Oracle 审查)

| 限制 | 影响 | 计划 |
|------|------|------|
| tick() 无条件递增 `requests_completed_` | 统计数字不反映真实内存请求 | Phase 8.B 改为事件驱动 |
| 无带宽/延迟模型（纯计数） | 无法做性能验证 | Phase 8.B 与 MemoryTLM 集成 |

## 8. 参考文献

- NVIDIA GB203: 384-bit GDDR7, 4 通道, 8-72 GB
- gpgpu-sim SM_120 paper: memory controller model
- 已有内存文档: [`memory-memtlm.md`](./memory-memtlm.md)

---

*维护者: CppTLM Team · 最后更新: 2026-07-02*