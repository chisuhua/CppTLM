# scripts/ — CppTLM 工具脚本

按用途分 5 个子目录组织：

## 子目录结构

| 目录 | 内容 | 用途 |
|------|------|------|
| `build/` | `build.sh`, `format.sh` | 构建与代码格式化 |
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
```
