# samples-orphaned — CppTLM 归档示例

本目录包含已从主项目 samples/ 目录移除的示例代码。

## 归档原因

| 示例 | 归档原因 | 归档时间 |
|------|----------|----------|
| `simple1/` | 内部 CMakeLists.txt 独立项目，根项目未引用；`modules/cpu_cluster.cc:1-5` 显式标注 `DEPRECATED in v2.1`，推荐使用 `include/tlm/cpu_tlm.hh` | 2026-06-08 |
| `simple_hier/` | 无 `CMakeLists.txt`（孤儿项目），`src/cpu_cluster.cc` / `src/noc_tile.cc` 引用 v2.1 不再支持的 `CpuCluster` / `NOCTile` 模块 | 2026-06-08 |

## 状态

- **保留方式**: `git mv`（保留完整 git 历史）
- **不维护**: 归档代码不再接受新功能、测试或修复
- **不删除**: 保留以供历史参考和潜在移植参考

## 恢复方法（如确需恢复）

```bash
# 从归档恢复（保留 git 历史）
git mv docs-archived/samples-orphaned/simple1 samples/
git mv docs-archived/samples-orphaned/simple_hier samples/
```

## 替代实现（v2.1+ 推荐）

参见 `samples/` 目录的 6 个活跃 demo：
- `stats_demo` / `traffic_gen_demo` / `streaming_demo`（C++）
- `topology_generator_demo` / `stats_visualization_demo` / `stats_watcher_demo`（Python）

以及 `include/tlm/` 下的官方 TLM 模块：`CacheTLM` / `CrossbarTLM` / `MemoryTLM` / `CpuTLM` / `RouterTLM` / `NICTLM` / `LinkTLM` / `TrafficGenTLM` / `ArbiterTLM`。
