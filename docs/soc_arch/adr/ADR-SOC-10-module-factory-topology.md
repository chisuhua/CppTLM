# ADR-SOC-10: dGPU SoC v1.0 ModuleFactory 拓扑层 — 9 类 SimModule 容器 + ApuSoC

> **状态**: 📋 Proposed — 2027-02-09
> **日期**: 2027-02-09
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: ModuleFactory 单一入口 JSON 拓扑 + 9 类 SimModule P2-P5 层级 + ApuSoC 顶层
> **类别**: SoC 架构 / v1.0 拓扑
> **关联文档**:
> - [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.0 PASS（§1.3 多层 SimModule 拓扑）
> - [`include/tlm/cluster/`](../../../include/tlm/cluster/)（9 类 SimModule + ApuSoC 真实代码）
> - [`include/modules_cluster.hh`](../../../include/modules_cluster.hh)（REGISTER_MODULE 集中注册）
> - [`examples/dgpu_soc_with_pcie_ip.json`](../../../examples/dgpu_soc_with_pcie_ip.json)（Phase 8 完整 dGPU SoC + PCIe EP 配置）
> **关联 ADR**:
> - ADR-SOC-09 D1（v1.0 NVIDIA+AMD 双 vendor 战略）
> - ADR-SOC-11（PcieEndpointIP 替代 PcieEndpointTLM）
> **关联 OpenSpec**: [`2027-02-09-cpptlm-dgpu-pcie-ip-integration/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-pcie-ip-integration/)

---

## 1. Context（背景）

### 1.1 9 类 SimModule 容器 + ApuSoC 顶层（per `00-overview` §1.3）

CppTLM 9 类 SimModule P2-P5 层级容器 + ApuSoC 顶层：

```
ApuSoC (顶层)
├── CpuCluster (CPU 侧 P3: 持有 CPUTLM/CacheTLM/MemoryTLM)
├── GpuCluster (GPU 顶层 P4: 持有 GpcCluster + 共享 L2 + HBM 控制器)
│   └── GpcCluster × M (P4: 持有 TpcCluster × N)
│       └── TpcCluster × N (P3: 持有 ComputeCluster × K)
│           └── ComputeCluster × K (P2: CU 蓝图复制 cu_template + cu_count)
│               └── ComputeUnitTLM × J
├── PcieEndpointIP (P3: 17 ports req_in[17] + resp_out[17])
├── SdmaEngineTLM / HostBypassTLM / PcieRootComplexTLM
├── CacheCluster (P3: L1×N + L2 聚合)
├── MemoryCluster (P3: 多通道 HBM/DDR 控制器)
├── GpuNoC (P4: Mesh NoC interconnect)
└── Crossbar (CoherentXBar / NonCoherentCrossbar)
```

### 1.2 ModuleFactory 设计

- **ModuleFactory.instantiateAll()**：从 JSON config 解析 + instantiate
- **validate_topology()**：验证拓扑合法性
- **统一入口**：`dgpu_soc_with_pcie_ip.json` 完整配置
- **集中注册**：`include/modules_cluster.hh` REGISTER_MODULE 集中 9 个 SimModule 派生类

### 1.3 Phase 8 整合交付现状

- `examples/dgpu_soc_with_pcie_ip.json` 完整 dGPU SoC + PCIe EP 配置（已交付）
- `scripts/CMakeLists.txt` 中 `validate_topology` CMake target（已建立）
- 9 类 SimModule 注册（已完成）

---

## 2. Decision（决策）

### D1. 单一入口 JSON 拓扑

✅ **单一入口 JSON 拓扑**（per Phase 8 `examples/dgpu_soc_with_pcie_ip.json` 99 行真实结构）：

**真实结构**:顶层 `name/description/modules/connections` 4 个 key（扁平 modules 数组 + 顶层 connections），**非** 嵌套 `children` 结构。

```json
{
  "name": "dGPU SoC + PCIe EP Example (Phase 8)",
  "description": "Complete dGPU SoC with 17-port PcieEndpointIP + HostBypassTLM + PcieRootComplexTLM + CoherentXBarTLM + MemoryTLM",
  "modules": [
    {
      "name": "pcie_endpoint_ip_0",
      "type": "PcieEndpointIP",
      "params": {
        "bypass_mode": "bypass",
        "axi_adapter": {
          "data_width": 512,
          "address_width": 64,
          "axi4_mapper_inject": true,
          "ports": ["axi_master_out", "axi_slave_in", "cfg_slave_in"]
        },
        "link_layer": { "speed": "gen5", "width": 16 },
        "phy_digital": { "ltssm_state": "L0" },
        "sr_iov": { "total_vfs": 16, "num_vfs": 8, "initial_vfs": 8 },
        "transaction_layer": { "credit_initial_posted": 32 }
      }
    },
    { "name": "host_bypass_0", "type": "HostBypassTLM" },
    { "name": "pcie_root_complex_0", "type": "PcieRootComplexTLM" },
    { "name": "coherent_xbar_0", "type": "CoherentXBarTLM" },
    { "name": "memory_tlm_0", "type": "MemoryTLM" }
  ],
  "connections": [
    { "src": "host_bypass_0.axi_master_out", "dst": "pcie_endpoint_ip_0.req_in[0]", "latency": 0 },
    { "src": "pcie_endpoint_ip_0.resp_out[0]", "dst": "host_bypass_0.axi_master_resp" },
    { "src": "pcie_root_complex_0.axi_master_out", "dst": "pcie_endpoint_ip_0.req_in[0]" },
    { "src": "pcie_endpoint_ip_0.req_out[0]", "dst": "coherent_xbar_0" },
    { "src": "coherent_xbar_0", "dst": "memory_tlm_0" }
  ]
}
```

**注**：`apu_soc_full.json`（`configs/`）等含 `ApuSoC` + `CpuCluster` + `GpuCluster` 的嵌套 `children` 树形 schema 是 dGPU 完整版的另一示例；本 D1 引用的是 `examples/dgpu_soc_with_pcie_ip.json`（Phase 8 整合交付）的扁平 modules 结构。

### D2. SimModule 多层容器层级

✅ **SimModule 多层容器**（per `include/tlm/cluster/`）：

- **P5 ApuSoC** 顶层
- **P4 GpuCluster / GpcCluster / GpuNoC**（per 9 类 SimModule）
- **P3 TpcCluster / CacheCluster / MemoryCluster / PcieEndpointIP**
- **P2 ComputeCluster / P3 CpuCluster**（与 `00-overview.md` §1.3 一致）

### D3. PcieEndpointIP 作为 17 ports ChStreamModuleBase

✅ **PcieEndpointIP 17 ports**（per ADR-SOC-11）：

- `req_in[NUM_PORTS]` + `resp_out[NUM_PORTS]` 数组
- NUM_PORTS = 17（1 PF + 16 VF）
- 内部 `stream_id` 路由

### D4. JSON `axi4_mapper_inject` 可选注入

✅ **JSON 可选注入**（per `docs/soc_arch/architecture/02-command-processor.md` + `examples/dgpu_soc_with_pcie_ip.json`）：

```json
{
  "params": {
    "axi4_mapper_inject": true   // v1.0 MVP 启用 OOO Mapper
  }
}
```

---

## 3. Consequences（后果）

### 3.1 正面影响

- **配置驱动**：单一 JSON 配置覆盖整个 dGPU SoC
- **拓扑灵活**：9 类 SimModule 任意组合
- **可测试**：validate_topology + instantiateAll 自动化

### 3.2 负面影响

- **多层容器复杂度**：ApuSoC → GpuCluster → GpcCluster → TpcCluster → ComputeCluster → CU（5 层）
- **配置复杂**：`dgpu_soc_with_pcie_ip.json` 文件体积增长
- **调试困难**：多层嵌套错误定位

---

## 4. Implementation（实施）

### 4.1 已实施（Phase 8 整合交付）

| 模块 | 路径 |
|------|------|
| ApuSoC 顶层 | `include/tlm/cluster/apu_soc.hh` |
| GpuCluster | `include/tlm/cluster/gpu_cluster.hh` |
| GpcCluster | `include/tlm/cluster/gpc_cluster.hh` |
| TpcCluster | `include/tlm/cluster/tpc_cluster.hh` |
| ComputeCluster | `include/tlm/cluster/compute_cluster.hh` |
| CpuCluster | `include/tlm/cluster/cpu_cluster.hh` |
| CacheCluster | (待新建) |
| MemoryCluster | `include/tlm/gpu/memory_cluster_tlm.hh` |
| GpuNoC | `include/tlm/cluster/gpu_noc_cluster.hh` |
| 集中注册 | `include/modules_cluster.hh` REGISTER_MODULE × 9 |

### 4.2 配置示例

完整配置见 [`examples/dgpu_soc_with_pcie_ip.json`](../../../examples/dgpu_soc_with_pcie_ip.json)。

---

## 5. Risks（风险）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 多层容器配置错误难调试 | 🟡 中 | validate_topology + 测试覆盖 |
| **R2** | JSON 配置体积增长 | 🟢 低 | 单一入口 + JSON 嵌套 |
| **R3** | 9 类 SimModule 集中注册顺序依赖 | 🟢 低 | REGISTER_MODULE 显式顺序 |

---

## 6. 参考文献

### 6.1 关联 ADR

| ADR | 关联 |
|-----|------|
| ADR-SOC-09 | v1.0 双 vendor 战略 |
| ADR-SOC-11 | PcieEndpointIP 17 ports |

### 6.2 关联 OpenSpec

| Change | 关联 |
|--------|------|
| `2027-02-09-cpptlm-dgpu-pcie-ip-integration` | Phase 8 整合交付 + 完整 dGPU SoC + PCIe EP 配置 |

### 6.3 关联模块

| 模块 | 微架构文档 |
|------|-----------|
| `apu_soc.md` | (待新建/参考 `00-overview` §1.3) |
| `gpu_cluster.md` | (待新建) |
| `gpu_noc_mesh.md` | `docs/soc_arch/modules/gpu-noc-mesh.md` |

---

## Status Update

- **2027-02-09**: 📋 Proposed。9 类 SimModule + ApuSoC 顶层已实施（Phase 8 整合交付）；本 ADR 文档化 ModuleFactory 拓扑层决策。