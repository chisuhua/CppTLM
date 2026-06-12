# CppTLM — C++ TLM Hybrid Simulation Framework

**Generated:** 2026-05-31
**Commit:** 29940d1 — update on hierarchy archive
**Branch:** main
**Version:** 2.1.0

## OVERVIEW

TLM 2.0 周期精确片上网络仿真框架。支持 JSON 驱动拓扑、ModuleFactory 动态注入、ChStream 内部通信协议。分层融合架构 v2.1 已实现 CacheTLM/CrossbarTLM/MemoryTLM 端到端全链路。

## STRUCTURE

```
CppTLM/
├── include/         # 头文件全部在此（10 子目录，无 src 头文件混用）
│   ├── core/        # 核心：SimObject/ModuleFactory/Port/ChStream 基类
│   ├── tlm/         # TLM 2.0 模块：CacheTLM, CrossbarTLM, MemoryTLM
│   │   └── gpu/      # Phase7.A+ GPU 模块：GPUTLM v0 (黑盒发起器，2026-06-11)
│   ├── framework/   # StreamAdapter 适配器（ChStream↔TLM 转换层）
│   ├── bundles/     # Bundle 定义（CacheReqBundle, CacheRespBundle 等）
│   ├── modules/     # 旧版 Legacy 模块（已迁移至 modules/legacy/ 子目录）
│   ├── ext/         # TLM 扩展插件接口
│   ├── utils/       # 工具：ConfigUtils, RegexMatcher, DynamicLoader
│   ├── sc_core/     # SystemC 兼容层
│   ├── chstream_register.hh  # ChStream 注册宏入口（REGISTER_CHSTREAM）
│   └── modules.hh   # Object 注册宏入口（REGISTER_OBJECT/REGISTER_MODULE）
├── src/             # 源实现 + 可执行文件
│   ├── core/        # ModuleFactory connection/instantiate 实现
│   ├── rtl/         # RTL 桥接实现（HybridCacheComponent, HybridCacheWrapper，BUILD_RTL=ON）
│   ├── tlm/         # TLM 2.0 模块实现（router_tlm, nic_tlm, link_tlm）
│   ├── utils/       # DynamicLoader 实现
│   ├── main.cpp          # 主仿真入口
├── test/            # 测试（Catch2 v3.7.0，75 文件）
├── test/python/     # Python 单元测试（analyzer, linter, path_tracer 等）
├── configs/         # JSON 拓扑配置（支持端口索引语法 "xbar.0"）
├── docs/            # 架构文档、ADR、实现计划
├── docs-archived/   # 已归档文档（v1 架构、dead-code、disabled-tests 等 12 子目录）
├── plans/ # 实施计划 JSON/Markdown
├── samples/ # 示例拓扑
├── scripts/ # 工具脚本（按用途分 5 个子目录：build/, test/, pipeline/, topology/, stats/）
├── cpptlm/ # Python 库（新，pyproject.toml 注册；含 cli.py, topo/, config/, simulation/）
├── cpptlm_config/ # Python 配置包（旧，builder.py/models.py/validator.py，examples 与 scripts/topology 引用）
├── .github/
│ └── workflows/
│ └── ci.yml # GitHub Actions CI/CD 工作流
└── CMakeLists.txt # 根构建配置
```

## 归档示例（docs-archived/samples-orphaned/）

`samples/simple1/` 和 `samples/simple_hier/` 已于 2026-06-08 归档：
- **simple1/**: `cpu_cluster` 在 v2.1 标记 DEPRECATED（v1 时代的 legacy 模块），推荐使用 `include/tlm/cpu_tlm.hh`（`cpu_cluster.cc` 源文件随 sample 一同归档至 docs-archived/samples-orphaned/simple1/）
- **simple_hier/**: 无 CMakeLists.txt（孤儿），引用 v2.1 不再支持的 `CpuCluster` / `NOCTile`
- 恢复方法：参见 [docs-archived/samples-orphaned/README.md](docs-archived/samples-orphaned/README.md)
```

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| 添加新 TLM 模块 | `include/tlm/` + `include/chstream_register.hh` | 从 ChStreamModuleBase 派生，REGISTER_CHSTREAM 注册 |
| 添加新 Legacy 模块 | `include/modules/legacy/` | 从 SimObject 派生，modules.hh REGISTER_OBJECT 注册 |
| 修改模块工厂逻辑 | `src/core/module_factory.cc` | instantiateAll + Step 7 StreamAdapter 注入 |
| 修改 JSON 配置格式 | `configs/` + `include/utils/config_utils.hh` | 支持 group/connection/latency |
| 添加 StreamAdapter | `include/framework/stream_adapter.hh` 或 `multi_port_stream_adapter.hh` | 单端口 vs 多端口 |
| 添加新 Bundle 类型 | `include/bundles/` | Bundle 定义 ChStream 内部消息格式 |
| 查看 ChStream 集成验证 | `test/test_phase6_integration.cc` | Cache→Crossbar→Memory 端到端 |
| 修改 CI/CD 工作流 | `.github/workflows/ci.yml` | GitHub Actions 配置 |
| 运行测试 | `./build/bin/cpptlm_tests` | Catch2，支持 `[tag]` 过滤 |

## CONVENTIONS

- **缩进**: 4 空格（.clang-format: IndentWidth: 4）
- **例外**: 本文档（AGENTS.md）使用 2 空格缩进
- **命名**: CamelCase 类, camelCase 函数, snake_case 变量, SCREAMING_SNAKE_CASE 宏
- **注释**: 中文，文件头必须含功能描述/作者/日期
- **模块注册**: 双注册表 — SimObject(对象) vs SimModule(模块)，分离 create 函数类型
- **ChStream 注册**: `REGISTER_CHSTREAM` 宏同时注册对象 + StreamAdapter + 多端口适配器
- **JSON 端口索引**: `"dst": "xbar.0"` 语法表示模块 `xbar` 的第 0 端口
- **测试标签**: Catch2 TAG, `[phaseX]` 按阶段分组, `[chstream]` 标记 Stream 集成测试
- **CMake**: 显式列出源文件（禁用 GLOB），核心库 `cpptlm_core`（静态）
- **零债务原则**: 每个 Phase 完成即编译通过、测试覆盖、文档同步。禁止遗留 TODO、跳过测试、未归档的技术债

## CI/CD WORKFLOW

### 工作流配置

`.github/workflows/ci.yml` 定义了 CI/CD 流程：

```yaml
on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main, develop]

jobs:
  build-and-test:
    strategy:
      matrix:
        build-type: [Release, Debug]
        # 仅 stub 路径（USE_SYSTEMC option 已删除）
    # 步骤: checkout → apt → ccache → cmake → build → test → upload-artifact

  code-format:
    # 步骤: checkout → clang-format → format.sh --check
```

### 本地提交流程（必须遵循）

```
1. 本地构建验证
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   cd build && ctest --output-on-failure

2. 创建特性分支
   git checkout -b feature/xxx 或 git checkout -b fix/xxx

3. 提交更改（使用 atomic commit）
   git add <files>
   git commit -m "type(scope): 简短描述"

4. 推送分支
   git push -u origin HEAD

5. 创建 PR（使用 GitHub CLI 或 Web UI）
   gh pr create --title "type: 描述" --body "## Summary\n\n..."
   # 或通过 GitHub Web UI 创建

6. 等待 CI 通过
   gh run watch 或在 GitHub Actions 页面查看

7. 合并 PR
   gh pr merge #<number> --squash 或通过 Web UI 合并
```

### GitHub Token 配置

使用 GitHub CLI 需要设置 Token：
```bash
export GITHUB_TOKEN="github_pat_xxx"
echo $GITHUB_TOKEN | gh auth login --with-token
gh auth status
```

### PR 合并策略

- **推荐**: Squash merge（保留干净的历史）
- **CI 必须通过**才能合并
- Artifact 名称使用 `run_id` 避免冲突：`test-results-${{ runner.os }}-${{ matrix.build-type }}-${{ github.run_id }}`

### 快速验证命令

```bash
# 本地完整构建 + 测试
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && cd build && ctest --output-on-failure

# 格式检查
./scripts/format.sh --check

# 查看 CI 状态
gh run list --limit 5

# 监控 CI
gh run watch
```

## ANTI-PATTERNS (THIS PROJECT)

- **禁止 GLOB 源文件**: CMakeLists.txt 使用 `set(CORE_SOURCES ...)` 显式列举（test/ 除外，使用 GLOB 自动发现测试）
- **禁止直接创建对象**: 必须通过 ModuleFactory::registerObject/registerModule 注册后由 instantiateAll 创建
- **禁止跳过 StreamAdapter**: ChStreamModuleBase 派生类必须通过 set_stream_adapter() 注入，不可直接操作 ChStreamPort
- **Legacy 模块**: `include/modules/legacy/` 下的模块已归档，仅修复严重 bug，新功能开发使用 `include/tlm/`
- **测试禁止 `.disabled`**: `test_config_loader.cc.disabled` 等是已知跳过状态，非错误。新代码禁止创建 .disabled 测试
- **禁止 TODO 残留**: Step 7 中的 `// TODO: bind_ports_array` 等未完成逻辑必须在 Phase 完成前清除或归档
- **禁止跳过本地 CI 验证**: 推送到 remote 前必须本地通过构建和测试

## 文档维护原则

**核心目标**: 防止 `AGENTS.md` / `ONBOARDING.md` / `roadmap.md` / `scripts/README.md` 中提及的路径随代码结构变更而漂移。

### 自动化保障

- **路径同步检查**: `scripts/test/docs_sync_check.sh` 扫描上述 4 个核心文档中所有 `path/to/file.ext` 反引号引用，验证文件/目录真实存在
- **Pre-commit hook**: `.pre-commit-config.yaml` 已配置；开发者运行 `pre-commit install` 后，提交前自动执行 `--strict` 模式
- **当前快照**: 见 [`docs/docs_audit_report.md`](docs/docs_audit_report.md)（2026-06-10：354/354 路径有效）

### 手动规范

- **结构调整 PR 必须包含**:
  1. 同步更新 AGENTS.md 的 STRUCTURE 节
  2. 同步更新 scripts/README.md 子目录表
  3. 同步更新 docs/ONBOARDING.md §5.5 脚本表
  4. 重命名/删除文件 → 检查 4 个核心文档中的引用
- **PR 模板检查项** (建议未来加入 `.github/PULL_REQUEST_TEMPLATE.md`):
  - [ ] 运行 `./scripts/test/docs_sync_check.sh --strict` 零误报
  - [ ] 若新增/删除 `include/` `src/` `scripts/` `cpptlm*/` 下文件，更新 AGENTS.md / ONBOARDING.md / scripts/README.md
  - [ ] 若重大阶段完成，更新 roadmap.md 状态表
- **ADR 不可变原则**: ADR 文件（`docs/adr/ADR-X.*-*.md`）一旦签发不可修改；后续状态通过在文件末尾追加 `## Status Update` 段落记录

### 添加新忽略路径

`scripts/test/docs_sync_check.sh` 的 `VIRTUAL_PATHS` 数组维护文档中"说明删除原因"时引用的已删除文件。当新文档提及此类文件时，添加条目而非删除文档段落。

## UNIQUE STYLES

- **DPRINTF 调试宏**: 全局日志 `DPRINTF(MODULE, "fmt", args...)`，编译期 `-DDEBUG_PRINT` 启用
- **端口索引语法**: `parsePortSpec(name)` 从 `"module.port_index"` 解析模块名 + 端口号
- **ChStreamArchitecture**: 分层通信协议 Bundle→StreamAdapter→ChStreamModuleBase→ChStreamPort
- **Phase 驱动开发**: Phase 0-6 + Phase 3.x (Port管理/Python验证) 已完成，Phase 3.2 端口类型系统已实施
- **债务修复状态**: P0 (PortPair泄漏/wildcard/死代码) 和 P1 (routing_algo/src_port) 已完成
- **预编译头**: test/ 使用 catch_amalgamated.cpp（2 文件版本）而非 FetchContent 实时下载

## COMMANDS

```bash
# 配置
cmake -S . -B build

# 编译
cmake --build build

# 运行测试
./build/bin/cpptlm_tests                    # 全部
./build/bin/cpptlm_tests "[chstream]"       # ChStream 相关 (84 用例)
./build/bin/cpptlm_tests "[phase6]"         # Phase 6 集成 (9 用例)
./build/bin/cpptlm_tests "[crossbar]"       # Crossbar 相关 (16 用例)
./build/bin/cpptlm_tests ~"[crossbar]"      # 排除 Crossbar

# 使用 ctest
ctest --test-dir build --output-on-failure

# 快速验证编译
cmake --build build -- -j$(nproc)

# GitHub CLI（需要 GITHUB_TOKEN）
gh run list --limit 5                        # 查看 CI 状态
gh run watch                                 # 监控当前 CI
gh pr create --title "type: 描述" --body "..."  # 创建 PR
gh pr merge #<number> --squash               # Squash 合并 PR
```

## NOTES

- **TLM stub 默认启用**: `USE_SYSTEMC_STUB=ON` 是默认配置（根 CMakeLists.txt），不再需要外部 SystemC 依赖
- **ccache 自动检测**: 编译加速，未安装时降级（非 fatal）
- **测试状态**: 无已知失败 (P0-remediation 后, 2026-06-12；当前实测 604 用例, ~14840 断言)
- **构建产物**: `build/bin/` 下所有可执行文件，`build/lib/` 下 `cpptlm_core.a`
- **CI 配置**: `.github/workflows/ci.yml` 定义了 Release/Debug 双模式构建和测试

<!-- code-review-graph MCP tools -->
## MCP Tools: code-review-graph

**IMPORTANT: This project has a knowledge graph. ALWAYS use the
code-review-graph MCP tools BEFORE using Grep/Glob/Read to explore
the codebase.** The graph is faster, cheaper (fewer tokens), and gives
you structural context (callers, dependents, test coverage) that file
scanning cannot.

### When to use graph tools FIRST

- **Exploring code**: `semantic_search_nodes` or `query_graph` instead of Grep
- **Understanding impact**: `get_impact_radius` instead of manually tracing imports
- **Code review**: `detect_changes` + `get_review_context` instead of reading entire files
- **Finding relationships**: `query_graph` with callers_of/callees_of/imports_of/tests_for
- **Architecture questions**: `get_architecture_overview` + `list_communities`

Fall back to Grep/Glob/Read **only** when the graph doesn't cover what you need.

### Key Tools

| Tool | Use when |
| ------ | ---------- |
| `detect_changes` | Reviewing code changes — gives risk-scored analysis |
| `get_review_context` | Need source snippets for review — token-efficient |
| `get_impact_radius` | Understanding blast radius of a change |
| `get_affected_flows` | Finding which execution paths are impacted |
| `query_graph` | Tracing callers, callees, imports, tests, dependencies |
| `semantic_search_nodes` | Finding functions/classes by name or keyword |
| `get_architecture_overview` | Understanding high-level codebase structure |
| `refactor_tool` | Planning renames, finding dead code |

### Workflow

1. The graph auto-updates on file changes (via hooks).
2. Use `detect_changes` for code review.
3. Use `get_affected_flows` to understand impact.
4. Use `query_graph` pattern="tests_for" to check coverage.
