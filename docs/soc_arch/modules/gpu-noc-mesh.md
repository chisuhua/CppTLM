# gpu-noc-mesh 微架构文档

> **类别**: Interconnect > GPU NoC · **状态**: ✅ Phase 8.A Task 3 + 📋 v1.0 dGPU SoC 战略补充(per L8 NoC 子系统架构)
> **Header**: `include/tlm/gpu/gpu_mesh_noc_tlm.hh`
> **注册**: `REGISTER_CHSTREAM` (`include/chstream_register.hh`)
> **蓝图来源**: gpgpu-sim SM_120 paper + NVIDIA GPC mesh interconnect
> **首版 commit**: `d164497` (2026-06-28) · **最近更新**: 2027-02-09 (v1.0 dGPU SoC 战略补充)
> **类名**: `GpuMeshNoC`（避免与 `include/tlm/cluster/gpu_noc_cluster.hh` 中已有的 `GpuNoC` SimModule 类冲突）

> **关联文档**:
> - 索引: [README.md](./README.md)
> - **L8 NoC Interconnect 子系统架构**: [`docs/soc_arch/architecture/08-noc-interconnect.md`](../architecture/08-noc-interconnect.md) §3-4
> - **关联 ADR**: [`ADR-SOC-10-module-factory-topology.md`](../../adr/ADR-SOC-10-module-factory-topology.md) D2 — SimModule 多层容器
> - Spec: [`docs/superpowers/specs/2026-06-24-gpu-soc-architecture.md`](../../superpowers/specs/2026-06-24-gpu-soc-architecture.md) §3.3
> - 已有 NoC Router: [`noc-router.md`](./noc-router.md)

---

## 1. 设计目标

`GpuMeshNoC` 是 GPU 端 **GPC 之间 mesh interconnect 简化模型**，实现 XY 维度序路由 + hops × latency 延迟模型。与现有的 `RouterTLM`（五阶段流水线 + Credit 控流）不同，本模块采用**简化查表**：不模拟 VC 分配、交换仲裁、拥塞控制。

**核心特性**:
- 继承 `ChStreamModuleBase`，单端口
- N×N mesh 拓扑参数化（`dim`）
- XY 维度序路由：曼哈顿距离 × 每跳延迟
- 纯延迟模型，不缓存/转发 flit

## 2. 架构概览

### 2.1 内部结构

```
    GpuMeshNoC (N×N mesh, XY routing)
         │
    ┌────┴────┐
    │  dim_ = 2 (N×N)
    │  hops_latency_ = 2
    │
    │  route_latency(src, dst):
    │    return (|dx| + |dy|) × hops_latency
    │
    │  tick():
    │    cycle_counter_++
    │    adapter_->tick()
    └─────────┘
         │
    adapter_ → 连接 CU 侧 → MemoryCluster
```

### 2.2 XY 维度序路由原理

```
(0,0) ──→ (0,1) ──→ (0,2)
  │         │         │
(1,0) ──→ (1,1) ──→ (1,2)
  │         │         │
(2,0) ──→ (2,1) ──→ (2,2)

示例: src=(0,0) → dst=(2,1)
  dx = |2-0| = 2, dy = |1-0| = 1
  total hops = 2 + 1 = 3
  latency = 3 × 2 = 6 cycles
```

### 2.3 端口表

| 端口 | 类型 | 数量 | 角色 |
|------|------|:---:|------|
| `adapter_` | `StreamAdapterBase*` | 1 | ChStream 桥接 |

## 3. 接口（Public API）

```cpp
class GpuMeshNoC : public ChStreamModuleBase {
public:
    explicit GpuMeshNoC(const std::string& name, EventQueue* eq);

    std::string get_module_type() const override { return "GpuMeshNoC"; }

    uint32_t route_latency(std::pair<uint32_t, uint32_t> src,
                           std::pair<uint32_t, uint32_t> dst) const;

    // Setter（JSON 解析后注入）
    void set_dim(uint32_t dim);
    void set_hops_latency(uint32_t hops_latency);

    uint32_t get_dim() const;
    uint32_t get_hops_latency() const;

    void tick() override;
};
```

## 4. 配置参数

| 参数 | 类型 | 默认值 | 说明 |
|------|------|:---:|------|
| `dim` | uint32 | 2 | Mesh 维度 (N×N)，GB203 最小: 2×2 |
| `hops_latency` | uint32 | 2 | 每跳延迟 (cycles)，简化: 统一 hops |

## 5. 测试覆盖

| 测试文件 | 标签 | 测试数 | 覆盖内容 |
|------|------|:---:|------|
| `test/test_gpu_noc_tlm.cc` | `[gpu][noc][phase8a]` | 10 | XY 路由延迟 / dim getter / tick cycle 计数 |

**关键测试用例**:
```cpp
TEST_CASE("GpuMeshNoC: 2x2 mesh XY routing", "[gpu][noc]") {
    GpuMeshNoC noc("noc", nullptr);
    REQUIRE(noc.route_latency({0,0}, {1,1}) == 4);  // (1+1) × 2
}
```

## 6. 与现有 RouterTLM 的关系

| 特性 | GpuMeshNoC (Phase 8.A) | RouterTLM (v2.1) |
|------|------------------------|-------------------|
| 协议 | ChStream 单端口 | NoCFlitBundle + Credit |
| 端口数 | 1 (简化) | 5 (N/E/S/W/Local) |
| 路由算法 | XY 查表 | XY 维度序 routing |
| 延迟模型 | hops × latency | 六阶段流水线 |
| VC 分配 | 无 | 2 VC per port |
| 拥塞控制 | 无 | Credit-based |
| 用途 | GPU GPC 间简化互联 | NoC 详细仿真 |

## 7. 风险

| 风险 | 缓解 |
|------|------|
| 简化模型忽略拥塞可能导致多 SM 场景不准 | Phase 8.B 可选升级到 RouterTLM 多端口模式 |
| 与现有 `GpuNoC` SimModule 类名冲突 | 使用独立类名 `GpuMeshNoC`，不同命名空间职责 |

## 8. 参考文献

- gpgpu-sim SM_120 paper: Blackwell SM mesh interconnect
- NVIDIA GB203: 9 GPC × 6 TPC, crossbar-based interconnect
- 已有 NoC 文档: [`noc-router.md`](./noc-router.md), [`noc.common.md`](./noc.common.md)

---

*维护者: CppTLM Team · 最后更新: 2026-07-02*