# CppTLM ↔ ArchForge 跨仓联邦

> **创建日期**: 2026-09-20
> **触发事件**: ArchForge 拆仓（PR `25c29ec` + `4024b16e`）

## 关系

- **CppTLM**（本仓）: TLM 仿真框架 + PCIe EP 实现 + GPGPU 仿真代码
- **ArchForge**（设计仓, `/workspace/project/ArchForge/`）: dGPU SoC 设计文档（架构 + ADR + 模块微架构 + 路线图 + 规格）

## 引用规则

1. CppTLM 中的 `docs/` 可引用 ArchForge 路径（通过 `VIRTUAL_PATHS` 机制，见 `scripts/test/docs_sync_check.sh`）
2. 实现变更影响设计 → 更新 ArchForge 仓后再提交 CppTLM
3. 设计变更牵涉实现 → 在 CppTLM 中创建 openspec change

## 已迁移路径

| CppTLM 路径 | ArchForge 路径 | 文件数 |
|-------------|----------------|--------|
| `docs/soc_arch/architecture/` | `docs/soc_arch/architecture/` | 43 |
| `docs/soc_arch/adr/` | `docs/soc_arch/adr/` | 27 |
| `docs/soc_arch/modules/` | `docs/soc_arch/modules/` | 53 |
| `docs/soc_arch/specs/` | `docs/soc_arch/specs/` | 1 |
| `docs/soc_arch/roadmap/` | `docs/soc_arch/roadmap/` | 9 |

## CI 检查

本地 CI 通过 `scripts/test/docs_sync_check.sh --strict` 验证文档引用（370 个路径）。

CI 中可增加 ArchForge 存在性检查（推荐，未来执行）：

```bash
if [ ! -d /workspace/project/ArchForge/docs/soc_arch ]; then
  echo "WARNING: ArchForge design repo not cloned at /workspace/project/ArchForge/"
  echo "         docs/soc_arch/* references resolve to placeholder."
fi
```

## 维护历史

| 日期 | 版本 | 说明 |
|------|------|------|
| 2026-09-20 | v1.0 | 初版：ArchForge 拆仓后建立跨仓联邦约定 |