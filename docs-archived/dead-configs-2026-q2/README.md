# dead-configs-2026-q2/ — 2026 Q2 失效 JSON 配置归档

> 创建日期: 2026-06-15
> 触发审计: `findings_architecture_review.md` 后续清理
> 关联变更: `cpptlm-p0-debt-remediation.md`

## 失效原因

以下 3 个 example 目录 (合计 10 文件) 引用的模块类型在 v2.1.0 主线 `BUILD_LEGACY_MODULES=OFF` (默认) 下未注册或永久 no-op:

| 类型 | 注册状态 | 触发配置 |
|------|----------|----------|
| `CPUSim` | 仅 BUILD_LEGACY_MODULES=ON 注册 | (configs/mesh_4x4.json 已迁移) |
| `CPUCluster` | 同上 (REGISTER_MODULE) | example_hier2/chip.json |
| `MemCluster` | 永无注册 | example_hier2/chip.json |
| `MemorySim` | 永无注册 | example_hier2/mem.json |
| `CacheSim` | 永无注册 | example_hierarchy/chip.json |
| `Router` (非 RouterTLM) | 永无注册 | example_hierarchy/node.json, example_layout/mesh_grid.json |
| `Arbiter` (非 ArbiterTLM*) | 永无注册 | example_layout/pipeline_auto.json |
| `SimModule` | 永无注册 | example_hierarchy/chip.json |

## 归档文件清单

| 原路径 | 字节 | 备注 |
|--------|------|------|
| configs/example_hier2/chip.json | - | CPUCluster + MemCluster |
| configs/example_hier2/cpu_cluster.json | - | CPUCluster |
| configs/example_hier2/mem.json | - | MemorySim |
| configs/example_hierarchy/chip.json | - | CacheSim + SimModule |
| configs/example_hierarchy/node.json | - | Router (non-TLM) |
| configs/example_hierarchy/system.json | - | (引用已失效 node.json) |
| configs/example_layout/mesh_grid.json | - | Router |
| configs/example_layout/pipeline_auto.json | - | Arbiter |
| configs/example_layout/manual.json | - | (引用 pipeline_auto) |
| configs/example_layout/ring_radial.json | - | (引用 Router) |

## 恢复方法

如需恢复使用:
1. 设置 CMake `BUILD_LEGACY_MODULES=ON` 重新编译
2. 或将所有 legacy 类型引用改写为对应 TLM 模块 (CPUTLM/MemoryTLM/CrossbarTLM/RouterTLM/ArbiterTLM2)
3. 修改后从 `docs-archived/dead-configs-2026-q2/` 移回 `configs/`

## 关联审计

- `docs-archived/findings_architecture_review.md` (原根目录) — Phase 7 P0 阻塞问题调研
