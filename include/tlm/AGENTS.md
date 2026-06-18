# include/tlm/ — TLM 2.0 模块（V2.1+ 新增）

**域**: Transaction Level Modeling 模块
**基类**:
- ChStream 协议模块（`CacheTLM`/`MemoryTLM`/`CrossbarTLM`/`CPUTLM` 等）— 从 `ChStreamModuleBase` 派生
- SimModule 容器（9 类: `CpuCluster` + `ComputeCluster` + `TpcCluster` + `GpcCluster` + `GpuCluster` + `CacheCluster` + `MemoryCluster` + `GpuNoC` + `ApuSoC`）— 从 `SimModule` 派生
**注册**:
- ChStream 协议模块 → `REGISTER_CHSTREAM` 宏（`include/chstream_register.hh`）
- SimModule 容器 → `REGISTER_MODULE` 宏（`include/modules.hh`，9 个类集中在 `include/modules_cluster.hh`）

## 模块列表

| 文件 | 模块 | 端口 | 作用 |
|------|------|------|------|
| `cache_tlm.hh` | CacheTLM | 1 (单端口) | L1 缓存仿真: hit/miss/替换策略 |
| `memory_tlm.hh` | MemoryTLM | 1 (单端口) | 主存仿真: 读/写/延迟模拟 |
| `crossbar_tlm.hh` | CrossbarTLM | 4 (多端口) | 交叉开关路由: 地址路由/VC 映射 |
| `cpu_tlm.hh` | CPUTLM | 1 (单端口) | CPU 发起器: 顺序地址生成 + 响应回环观测 (`last_response_transaction_id_`) |
| `tlm_stub.hh` | TLM Stub | — | 桩实现（283 行）: SystemC TLM 2.0 多 extension + thread-safe API |
| `cluster/cpu_cluster.hh` | `CpuCluster` | N (按 `modules` 字段) | 集群容器，持有 N 个 CPUTLM + CacheTLM + MemoryTLM（SimModule 派生） |
| `cluster/compute_cluster.hh` | `ComputeCluster` | 1 (input) + 1 (output) | 单 CU 容器：引用 `cu_template` 蓝图 + `cu_count` 复制，支持子模块名规范化 |

## 端口架构

**单端口模块** (CacheTLM/MemoryTLM):
- 1 组 ChStreamPort (req_in + resp_out + req_out + resp_in)
- 通过 `set_stream_adapter(StandaloneAdapter*)` 注入

**多端口模块** (CrossbarTLM):
- N 组 ChStreamPort (CrossbarTLM 为 4 端口)
- 通过 `set_stream_adapter(MultiPortAdapter*[])` 注入
- `num_ports() const override { return N; }`

## StreamAdapter 流程

```
JSON连接 → ModuleFactory.instantiateAll() → Step 7 StreamAdapter注入
  → ChStreamAdapterFactory::createAdapter(moduleType)
  → 绑定 MasterPort↔SlavePort 端口对
  → module.set_stream_adapter(adapter)
  → tick() 时 adapter 转发 req/resp
```

## 约定

- Bundle 定义见 `include/bundles/cache_bundles_tlm.hh`（轻量级，非 Ch 原生 Bundle）
- 跨模块延迟通过 JSON 配置的 `latency` 字段注入（StreamAdapter 层面实现）
- `USE_SYSTEMC_STUB=ON` 时 tlm_stub.hh 提供 tlm_generic_payload 桩实现（含多 extension: `tlm_extension_registry` + `tlm_array<T>` + 完整 `set/get/clear/release_extension<T>()` API）

## 子目录

### `cluster/` — SimModule 集群容器

**目的**: 存放 `SimModule` 派生类的"集群/容器"型模块（持有内部子模块 + 可选暴露端口）。`SimModule` 与 `SimObject` 的双注册表分离（详见 `include/AGENTS.md` "注册宏体系"）要求这类模块走 `REGISTER_MODULE` 而非 `REGISTER_CHSTREAM`。

**当前模块**:

| 文件 | 模块 | 派生自 | 作用 |
|------|------|--------|------|
| `cpu_cluster.hh` | `CpuCluster` | `SimModule` | 持有 N 个 CPUTLM/CacheTLM/MemoryTLM 子模块的集群容器，支持 N 层 JSON 嵌套 (`outputs`/`inputs` 暴露端口) + `MAX_DEPTH=8` 限深保护 |
| `compute_cluster.hh` | `ComputeCluster` | `SimModule` | 单 CU 容器：`cu_template` 引用 `configs/templates/compute_unit_v1.json` 蓝图 + `cu_count` 控制复制数，cross-layer 端口引用 (D.4 修复) |
| `tpc_cluster.hh` | `TpcCluster` | `SimModule` | GPU Thread Processing Cluster：持有 2-8 个 ComputeCluster + 共享 L1 data cache (D.4 修复支持 cross-layer `tpc0.cu0`) |
| `gpc_cluster.hh` | `GpcCluster` | `SimModule` | GPU General Processing Cluster：持有 N 个 TpcCluster + 几何/光栅/渲染子单元 |
| `gpu_cluster.hh` | `GpuCluster` | `SimModule` | 顶层 GPU：持有 N 个 GpcCluster + 共享 L2 cache + 显存控制器 (4 级 GPU 层次) |
| `cache_cluster.hh` | `CacheCluster` | `SimModule` | L1×N (私有) + L2 (共享) 缓存层次聚合，inline 连接 Arbiter |
| `memory_cluster.hh` | `MemoryCluster` | `SimModule` | 多通道 HBM/DDR 控制器 + Arbiter + 通道间负载均衡 |
| `gpu_noc_cluster.hh` | `GpuNoC` | `SimModule` | Garnet 风格 NxN mesh 网络（routers 节点），GPU 端 interconnect |
| `apu_soc.hh` | `ApuSoC` | `SimModule` | 顶层 APU SoC：CPU 侧 (CpuCluster) + GPU 侧 (GpuCluster) + CrossbarTLM 互联，**`incorporate_parent` 钩子**（借鉴 gem5 `incorporate_cache(board)` late-binding） |

**注册**: `REGISTER_MODULE` 宏（`include/modules.hh`），所有 9 个 SimModule 派生类集中在 `include/modules_cluster.hh`

**架构层次** (P2-P5 落地, 2026-06-19):
```
ApuSoC (顶层)
├── CpuCluster        ── 持有 CPUTLM/CacheTLM/MemoryTLM
├── GpuCluster        ── 持有 N × GpcCluster
│   └── GpcCluster    ── 持有 N × TpcCluster
│       └── TpcCluster── 持有 N × ComputeCluster (cu_count 复制 cu_template)
├── CacheCluster      ── L1×N + L2 聚合
├── MemoryCluster     ── 多通道 HBM/DDR
└── GpuNoC            ── GPU 端 mesh interconnect
```

**示例配置**:
- `configs/example_simmodule_nested_2level.json` — 顶层 CpuCluster + 4 CPUTLM + CacheTLM + MemoryTLM
- `configs/example_simmodule_nested_3level_static.json` — 3 层 CpuCluster 嵌套（outer → mid → inner）
- `configs/templates/compute_unit_v1.json` — ComputeCluster 蓝图 (P2 引入, 被 cu_template 引用)
- `configs/templates/gpu_2gpc_2tpc_2cu.json` — 完整 4 级 GPU 层次蓝图 (P2)
- `configs/apu_soc_v1.json` — 完整 APU SoC 端到端 (P5: CPU + GPU + CrossbarTLM)
