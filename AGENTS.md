# CppTLM — C++ TLM Hybrid Simulation Framework

**Generated:** 2026-04-13
**Commit:** 5f168db — feat(P6): 端到端集成验证 — Cache→Crossbar→Memory 全链路
**Branch:** main
**Version:** 2.0.0

## OVERVIEW

TLM 2.0 周期精确片上网络仿真框架。支持 JSON 驱动拓扑、ModuleFactory 动态注入、ChStream 内部通信协议。分层融合架构 v2.1 已实现 CacheTLM/CrossbarTLM/MemoryTLM 端到端全链路。

## STRUCTURE

```
CppTLM/
├── include/         # 头文件全部在此（10 子目录，无 src 头文件混用）
│   ├── core/        # 核心：SimObject/ModuleFactory/Port/ChStream 基类
│   ├── tlm/         # TLM 2.0 模块：CacheTLM, CrossbarTLM, MemoryTLM
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
│   ├── noc/         # NoC 相关实现
│   ├── utils/       # DynamicLoader 实现
│   ├── main.cpp          # 主仿真入口
│   ├── cpu_main.cpp      # CPU 模式入口
│   ├── traffic_main.cpp  # 流量仿真入口
│   └── sc_main.cpp       # SystemC sc_main 入口
├── test/            # 测试（Catch2 v3.5.0，43 文件）
├── configs/         # JSON 拓扑配置（支持端口索引语法 "xbar.0"）
├── docs/            # 架构文档、ADR、实现计划
├── docs-archived/   # 已归档文档
├── docs-pending/    # 待整理文档
├── plans/           # 实施计划 JSON/Markdown
├── samples/         # 示例拓扑
└── CMakeLists.txt   # 根构建配置
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
| 运行测试 | `./build/bin/cpptlm_tests` | Catch2，支持 `[tag]` 过滤 |

## CONVENTIONS

- **缩进**: 4 空格（.clang-format: IndentWidth: 4），非 AGENTS.md 全局配置 2 空格
- **命名**: CamelCase 类, camelCase 函数, snake_case 变量, SCREAMING_SNAKE_CASE 宏
- **注释**: 中文，文件头必须含功能描述/作者/日期
- **模块注册**: 双注册表 — SimObject(对象) vs SimModule(模块)，分离 create 函数类型
- **ChStream 注册**: `REGISTER_CHSTREAM` 宏同时注册对象 + StreamAdapter + 多端口适配器
- **JSON 端口索引**: `"dst": "xbar.0"` 语法表示模块 `xbar` 的第 0 端口
- **测试标签**: Catch2 TAG, `[phaseX]` 按阶段分组, `[chstream]` 标记 Stream 集成测试
- **CMake**: 显式列出源文件（禁用 GLOB），核心库 `cpptlm_core`（静态）
- **零债务原则**: 每个 Phase 完成即编译通过、测试覆盖、文档同步。禁止遗留 TODO、跳过测试、未归档的技术债

## ANTI-PATTERNS (THIS PROJECT)

- **禁止 GLOB 源文件**: CMakeLists.txt 使用 `set(CORE_SOURCES ...)` 显式列举（test/ 除外，使用 GLOB 自动发现测试）
- **禁止直接创建对象**: 必须通过 ModuleFactory::registerObject/registerModule 注册后由 instantiateAll 创建
- **禁止跳过 StreamAdapter**: ChStreamModuleBase 派生类必须通过 set_stream_adapter() 注入，不可直接操作 ChStreamPort
- **禁止修改 src/core/ 外部的 .cc 文件**: 头文件在 include/，所有实现在 src/（除测试用 mock）
- **Legacy 模块不可修改**: `include/modules/legacy/` 下的模块已归档，新功能使用 `include/tlm/`
- **测试禁止 `.disabled`**: `test_config_loader.cc.disabled` 等是已知跳过状态，非错误。新代码禁止创建 .disabled 测试
- **禁止 TODO 残留**: Step 7 中的 `// TODO: bind_ports_array` 等未完成逻辑必须在 Phase 完成前清除或归档

## UNIQUE STYLES

- **DPRINTF 调试宏**: 全局日志 `DPRINTF(MODULE, "fmt", args...)`，编译期 `-DDEBUG_PRINT` 启用
- **端口索引语法**: `parsePortSpec(name)` 从 `"module.port_index"` 解析模块名 + 端口号
- **ChStreamArchitecture**: 分层通信协议 Bundle→StreamAdapter→ChStreamModuleBase→ChStreamPort
- **Phase 驱动开发**: Phase 0-6 已完成，每个 Phase 对应独立测试文件
- **预编译头**: test/ 使用 catch_amalgamated.cpp（2 文件版本）而非 FetchContent 实时下载

## COMMANDS

```bash
# 配置
cmake -S . -B build -DUSE_SYSTEMC=ON

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
```

## NOTES

- **SystemC 可选**: `-DUSE_SYSTEMC=OFF` 时使用 TLM stub（`USE_SYSTEMC_STUB=ON` 默认开启）
- **ccache 自动检测**: 编译加速，未安装时降级（非 fatal）
- **已知失败**: Pool/Wildcard/Connection 相关测试 12 个失败，为历史遗留问题，零回归
- **构建产物**: `build/bin/` 下所有可执行文件，`build/lib/` 下 `cpptlm_core.a`
- **文档不同步**: `docs/architecture/01-hybrid-architecture-v2.1.md` 停留在 v2.1.5，Phase 6 未同步
