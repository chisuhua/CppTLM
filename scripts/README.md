# scripts/ — CppTLM 工具脚本

按用途分 5 个子目录组织：

## 子目录结构

| 目录 | 内容 | 用途 |
|------|------|------|
| `build/` | `build.sh`, `format.sh`, `build_ptx_emu.sh` | 构建与代码格式化（`build_ptx_emu.sh` 启用 PTX-EMU 集成，dGPU/APU SoC 默认路径） |
| `test/` | `test.sh`, `run_all_tests.sh`, `ci_e2e_test.sh` | 测试与 E2E 验证 |
| `pipeline/` | `run_full_pipeline.sh` | 完整仿真流程（生成→仿真→可视化） |
| `topology/` | `topology_validator.py`, `topology_generator.py`, `analyzer.py`, `path_tracer.py`, `credit_flow.py` | 拓扑生成/分析/验证 |
| `stats/` | `stats_annotator.py`, `stats_watcher.py`, `layout_manager.py`, `derive_expr.py` | 性能统计与可视化 |

## 路径引用注意

- `scripts/CMakeLists.txt` 调度 `topology/topology_validator.py`
- 各 bash 脚本用 `BASH_SOURCE` 解析自身目录，所有跨子目录引用用相对路径 `../`
- Python demo 导入用 `sys.path.insert(0, ...)` 显式添加

## 验证

```bash
# CMake target 应仍工作
cmake --build build --target validate_topology

# CI 集成测试
./scripts/test/ci_e2e_test.sh
./scripts/test/run_all_tests.sh --quick

# 文档路径同步检查（pre-commit hook 也会自动运行）
./scripts/test/docs_sync_check.sh --strict
```

## 文档同步检查 (`scripts/test/docs_sync_check.sh`)

- 扫描 `AGENTS.md` / `docs/ONBOARDING.md` / `roadmap.md` / `scripts/README.md` 中所有 `path/to/file.ext` 反引号引用
- 通过 CMake `INCLUDE_PATH_PREFIXES` 智能补全（处理无 `core/` 前缀的简写形式如 `<file>.hh`）
- 通过 `VIRTUAL_PATHS` 数组排除已删除/归档文件（仅在文档中说明删除原因时引用）
- `--strict` 模式发现任何缺失返回非零退出码（用于 pre-commit hook）
- 当前快照：[`docs/docs_audit_report.md`](../docs/docs_audit_report.md)
