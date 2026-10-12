# include/tlm/cluster/ — SimModule 集群容器

**域**: SimModule 派生类(9 类, P2-P5 落地)
**基类**: `SimModule` (`include/core/sim_module.hh`)
**注册**: `REGISTER_MODULE(T)` (集中在 `include/modules_cluster.hh`)

## 模块清单

| 文件 | 模块 | ChStream 化? | 子模块 | 作用 |
|------|------|:---:|--------|------|
| `cpu_cluster.hh` | `CpuCluster` | ✅ | CPUTLM + CacheTLM + MemoryTLM | CPU 集群,支持 N 层 JSON 嵌套 (`outputs`/`inputs` 暴露端口) + `MAX_DEPTH=8` 限深保护 |
| `compute_cluster.hh` | `ComputeCluster` | ✅ | 单 CU 蓝图 (`cu_template`) | 引用 `configs/templates/compute_unit_v1.json` + `cu_count` 复制,支持 cross-layer 端口引用 (D.4 修复) |
| `tpc_cluster.hh` | `TpcCluster` | ✅ | 2-8 个 ComputeCluster + 共享 L1 data cache | GPU Thread Processing Cluster (D.4 修复支持 cross-layer `tpc0.cu0`) |
| `gpc_cluster.hh` | `GpcCluster` | ✅ | N × TpcCluster + 几何/光栅/渲染 | GPU General Processing Cluster |
| `gpu_cluster.hh` | `GpuCluster` | ✅ | N × GpcCluster + 共享 L2 + 显存控制器 | 顶层 GPU (4 级 GPU 层次) |
| `cache_cluster.hh` | `CacheCluster` | ✅ | L1×N + L2 聚合 + Arbiter | 缓存层次聚合,inline 连接 Arbiter |
| `memory_cluster.hh` | `MemoryCluster` | ✅ | 多通道 HBM/DDR 控制器 + Arbiter | 通道间负载均衡 |
| `gpu_noc_cluster.hh` | `GpuNoC` | ✅ | Garnet 风格 NxN mesh | GPU 端 interconnect |
| `apu_soc.hh` | `ApuSoC` | ✅ | CpuCluster + GpuCluster + CrossbarTLM | 顶层 APU SoC, **`incorporate_parent` 钩子** (gem5 `incorporate_cache(board)` late-binding) |
| `soc_internal_noc.hh` | `SocInternalNoc` | ❌ callback | 内部 mesh DataNoC | e1 v1.1 (archived 2026-10-09), 走 `set_*_target` programmatic bridge |
| `gpu_internal_fabric.hh` | `GpuInternalFabric` | ❌ callback | 桥接 IP ↔ VRAM | 走 `set_vram_target/set_fabric_source` programmatic bridge |

**重要**: 9 个 P2-P5 SimModule 默认走 ChStream 端口;`SocInternalNoc` + `GpuInternalFabric` 是**该类实现选择**,不是 SimModule 类型限制 (per 根 AGENTS.md "SimModule ChStream 端口能力" + 详细对照表见 `include/tlm/AGENTS.md`)。

## 架构层次 (P2-P5 落地, 2026-06-19)

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

## 端口连接 vs 装配桥接 (新增 2026-10-12)

### 决策矩阵

| 场景 | 走 JSON `connections[]`? | 走 programmatic 装配? | 决策依据 |
|------|:---:|:---:|------|
| 子模块 A (ChStream 化) → 子模块 B (ChStream 化) | ✅ | ❌ | `dgpu_board_v1.json:80-94` 实证 |
| 跨 SimModule 边界,通过 outputs/inputs 暴露 | ✅ | ❌ | `findInternalPath` 跨层下钻 + `test_simmodule_nested.cc` case 5b 实证 |
| **SimModule (callback 化, 如 `SocInternalNoc`) ↔ ChStream 模块** | ❌ | ✅ (`set_*_target` programmatic bridge) | `dgpu_board_shell.cc:840-852` 4 target 桥接 |
| **Bundle 类型不兼容 (PcieTlpBundle ↔ NoCFlitBundle)** | ❌ (StreamAdapter 层拒绝) | ✅ (需加 wire-format 转换 adapter) | `docs/implementation/2026-10-09-gmmu-pte-bus-correction.md` §6 Deviation 1 |
| LWC 逻辑名注册 (GMMU / SDMA) | ❌ (无 JSON schema) | ✅ (`lwc_hub->register_cr_target("gmmu", ...)`) | `dgpu_board_shell.cc:857-867` |

### callback 化 SimModule 改造为 ChStream 化 (工程参考)

`SocInternalNoc` (per `include/tlm/cluster/soc_internal_noc.hh:50`):
```cpp
class SocInternalNoc : public ::SimModule {
    // 当前实现: set_*_target() programmatic bridge + route_flit() 内部推送
    // 改造路径 (Gap I 推进):
    //   1. 加 ChStreamPort 字段 (N 组 req_in/req_out/resp_in/resp_out)
    //   2. override set_stream_adapter()
    //   3. JSON outputs/inputs 暴露内部子模块端口
    //   4. JSON connections[] 走 ChStream wire (sdma.2 → soc_noc.0 等)
};
```

**未实施原因** (per e1 v1.1 archived 2026-10-09 + Change C 实施笔记 §6):
- SimModule 是"内部总线"分层
- NoC 是"中间件",不是"端点"
- 跨模块路由逻辑归 mesh 中央,归 SimModule 节点管
- 避免异构 wire-format 转换 (SDMA 发 PcieTlpBundle,NoC 内部 NoCFlitBundle,VramCtrl 收 CacheReqBundle,NoC 走 ChStream 端口要做 3 套 wire-format 转换)

**未来推进** (per `cpptlm-dgpu-soc-physical-ip-port-boundary` 待起草 change):PcieTlpBundle ↔ NoCFlitBundle wire-format 转换层 + SocInternalNoc ChStream 化,合并实施,估时 14-22d。

## 关键 API 索引 (`include/core/sim_module.hh`)

- **基类 ChStream 暴露**:
  - `SlavePort* getInternalInputPort(const std::string& internal_path)` (line 132-138)
  - `MasterPort* getInternalOutputPort(const std::string& internal_path)` (line 140-146)
- **跨层端口查找**:
  - `std::string findInternalPath(const std::string& external_label) const` (line 149-161, P1 修复 D.4 递归下钻)
- **JSON 暴露端口配置**:
  - `parsePortConfigs(const json& cfg)` (line 207-235, 解析 `outputs` / `inputs` 字段)
- **深度限制**:
  - `static constexpr int MAX_DEPTH = 8` (line 38, 编译宏 `CPPTLM_SIMMODULE_MAX_DEPTH` 可覆盖)
  - `thread_local int depth_` + RAII `DepthGuard` (line 44-49, 异常安全)

## 测试覆盖

- `test/test_simmodule_nested.cc` — 7 cases, 验证多层嵌套 + outputs/inputs 暴露 (case 5b)
- `test/test_simmodule_recursive_bug.cc` — `findInternalPath` 跨层递归
- `test/test_simmodule_infra_clusters.cc` — 9 个 SimModule 集成
- `test/test_simmodule_gpu_hierarchy.cc` — P2 GPU 端 4 类 SimModule 集成

## 约定

- **JSON 嵌套**: `SimModule` 派生类支持 JSON 内联 `modules` 数组实现多层嵌套实例化,不限深度但有 `MAX_DEPTH=8` 运行时保护
- **跨层引用**: `outputs[]` / `inputs[]` 的 `internal` 字段支持跨层引用 (如 `"mid.cpu0_to_bus"`),`findInternalPath` 递归下钻到子 SimModule 拼接路径前缀
- **配置模板**: 3 份蓝图在 `configs/templates/` (compute_unit_v1.json + cpu_cluster_2level.json + gpu_2gpc_2tpc_2cu.json)
- **JSON `params` 字段**: 模块参数 dict,通过 `ModuleFactory::validateConfig` 校验,注入到 `SimObject::set_config()` (per `configs/AGENTS.md` "模块字段语义边界")

## 常见错误 (历史 + 修正)

| 错误归纳 | 实际情况 |
|---------|---------|
| "SimModule 无 ChStream 端口" | ❌ 基类**完全支持** outputs/inputs 暴露;实证: `dgpu_board_v1.json:80-94` + case 5b |
| "SocInternalNoc 不能走 ChStream" | ❌ 是**该类当前未声明** ChStreamPort 字段 (走 callback);改造是工程改动 |
| "JSON `connections[]` 只能连 SimObject 端口" | ❌ SimModule 内部 `connections[]` 完全可行;SimModule 顶层通过 `outputs/inputs` 暴露内部子模块端口 |
| "SimModule 必须 programmatic 装配" | ❌ 仅 callback 化 SimModule + Bundle 类型不兼容场景需 programmatic |

## 修正记录

- **2026-10-12 (本文件新建)**: 旧版 `include/tlm/AGENTS.md` 把 `SocInternalNoc` callback 模型误推广为"SimModule 类型限制",本文件 + 根 AGENTS.md + `include/tlm/AGENTS.md` 三处同步修正
