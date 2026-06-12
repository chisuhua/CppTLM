# test/ — 测试套件

**域**: 单元测试 + 集成测试（C++ Catch2 v3.7.0 + Python pytest）
**框架**: Catch2（预编译 2 文件版本）
**可执行文件**: `./build/bin/cpptlm_tests` (C++) + `python3 -m pytest test/python/` (Python)

## 测试分类

| 类别 | 文件数 | 标签 | 说明 |
|------|--------|------|------|
| Phase 集成测试 | 8 | `[phase2]`-`[phase8]` | Phase 2-8 端到端验证 |
| ChStream 集成 | 2 | `[chstream]` | StreamAdapter/端口握手 |
| TLM 模块测试 | 3 | `[phase6]` | CacheTLM/CrossbarTLM/MemoryTLM |
| 基础功能 | ~25 | 无/通用 | Packet, Pool, Config, Regex, Port |
| Python 工具测试 | 5 | `pytest` | validator, analyzer, linter, path_tracer |
| 跳过/禁用 | 4 | `.disabled` | 文件名尾缀 `.cc.disabled` (非编译) |

## 已知跳过测试

> 历史遗留：以下 `.cc.disabled` 文件已从磁盘删除，文档待清理。

历史记录（已移除）：
- `test_config_loader.cc.disabled` — JSON 配置加载（待修复）
- `test_end_to_end_delay.cc.disabled` — 端对端延迟验证（待修复）
- `test_latency_injection.cc.disabled` — 延迟注入（待修复）
- `test_layout_styles.cc.disabled` — 布局样式（已归档）

> P0-remediation 后（2026-06-12）已无已知失败。原"12 个 Pool/Wildcard/Connection 失败"为历史残留描述，已实际验证为 0 失败。

## 基础设施

| 文件 | 作用 |
|------|------|
| `catch_amalgamated.hpp` + `.cpp` | Catch2 预编译（2 文件版本，非 FetchContent 实时下载） |
| `CMakeLists.txt` | 测试构建配置 |
| `mock_modules.hh` | 测试用 Mock 模块（旧版 GTest 参考，实际使用 SimObject 子类） |
| `README.md` | 测试说明（部分过时，提到 GTest 但实际用 Catch2） |

## 运行测试

```bash
# C++ 测试
./build/bin/cpptlm_tests                    # 全部 (604 用例, ~14840 断言)
./build/bin/cpptlm_tests "[chstream]"       # ChStream 相关 (84 用例)
./build/bin/cpptlm_tests "[phase6]"         # Phase 6 端到端 (9 用例)
./build/bin/cpptlm_tests "[crossbar]"       # Crossbar 相关 (16 用例)
./build/bin/cpptlm_tests ~"[crossbar]"      # 排除 Crossbar

# Python 测试
python3 -m pytest test/python/ -v           # 87 用例

ctest --test-dir build --output-on-failure  # CTest 方式
```

## 约定

- 添加新 C++ 测试文件时：文件放 `test/test_<feature>.cc`，CMake 自动通过 `file(GLOB test_*.cc)` 包含
- 添加新 Python 测试文件时：文件放 `test/python/test_<feature>.py`
- 测试使用 Catch2 v3.7.0，`TEST_CASE`/`SECTION`/`CHECK`/`REQUIRE`/`WARN`
- Phase 集成测试按顺序编号（Phase 2-8），每个 Phase 对应独立 .cc 文件

## 更新记录

- **2026-05-09**: v1.1 — 更新 Catch2 版本至 v3.7.0，添加 Python 测试分类
