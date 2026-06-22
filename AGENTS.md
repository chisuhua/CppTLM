# CppTLM — C++ TLM Hybrid Simulation Framework

**Version**: 2.1.0 · **Branch**: main · **Last verified**: 2026-06-16 @ `8d8ad6c`

## WHAT THIS IS

TLM 2.0 周期精确片上网络 (NoC) 仿真框架。C++ 核心 + Python 工具链 + JSON 驱动拓扑，CacheTLM/CrossbarTLM/MemoryTLM 端到端全链路已验证（Phase 6），Phase 7.A GPU 黑盒发起器已落地（2026-06-11）。

**First read** for new agents: `docs/ONBOARDING.md` (knowledge-graph-generated ramp-up)。
Architecture 必读: `docs/architecture/01-hybrid-architecture-v2.1.md`。

## STRUCTURE (verified)

```
include/         # 所有 .hh 头文件（src/ 仅放 .cc, 无混用）
  core/          # SimObject/ModuleFactory/Port/ChStream 基类 + ext/ 子目录
  tlm/           # TLM 2.0 模块: cache/crossbar/memory/cpu/link/nic/router/traffic_gen/arbiter
  tlm/cluster/   # SimModule 多级层次: CpuCluster/ComputeCluster/TpcCluster/GpcCluster/
                 #   GpuCluster/CacheCluster/MemoryCluster/GpuNoC/ApuSoC (9 类 P2-P5)
  tlm/gpu/       # Phase 7.A+ GPUTLM v0 (黑盒发起器)
  framework/     # StreamAdapter 转换层 (单/多/双端口 + 双向)
  bundles/       # Bundle 定义: cache/noc/compute_bundles_tlm + cpphdl_types
  modules/legacy/# CPUSim (BUILD_LEGACY_MODULES=OFF 默认, 已归档)
  ext/           # TLM 扩展插件 (credit_stream / error_context / mem / transaction_context)
  metrics/       # histogram / stats / metrics_reporter / streaming_reporter
  rtl/           # RTL 桥接头文件 (fragment_mapper, hybrid_cache_*)
  utils/         # config_utils / json_includer / var_resolver / wildcard / dynamic_loader
  sc_core/       # SystemC 兼容层 (stub)
  chstream_register.hh  # REGISTER_CHSTREAM 宏入口
  modules.hh     # REGISTER_OBJECT / REGISTER_MODULE 宏入口
  modules_cluster.hh    # REGISTER_MODULE 参数化入口 (9 个 SimModule 派生类)
  AGENTS.md      # 注册宏体系完整表 (必读)

src/             # .cc 实现 + main.cpp
  core/          # module_factory(.cc 604 行, ⭐complex) / connection_resolver / param_parser
                 # port_compatibility / coherence_domain / topology_parser / plugin_loader
  tlm/           # router_tlm(.cc 805 行) / nic_tlm / link_tlm
  tlm/cluster/   # SimModule 派生类 .cc 实现 (P2-P5)
  rtl/           # hybrid_cache_component / hybrid_cache_wrapper (BUILD_RTL=ON 才编)
  utils/         # dynamic_loader 实现
  main.cpp       # 主仿真入口

test/            # Catch2 v3.7.0, 88 个 test_*.cc + Python pytest
  catch_amalgamated.{cpp,hpp}  # 预编译头 (非 FetchContent 实时下载)
  python/        # 15 用例: analyzer / path_tracer / topo_* / validator / apu_soc_emitter
  rtl/           # RTL 桥接测试 (BUILD_RTL=ON)
  AGENTS.md      # 测试分类 + 标签表

configs/         # 30+ JSON 拓扑 (含 apu_soc_* Phase 7.A/7.B/7.F)
  common/  param_rules/  test/   # 子目录: 共享 include 片段 / 参数规则 / 测试配置
  templates/     # JSON 蓝图 (P2): compute_unit_v1.json / cpu_cluster_2level.json /
                 #   gpu_2gpc_2tpc_2cu.json (被 cu_template 引用, cu_count 控制复制)
  AGENTS.md      # Schema 文档

docs/
  architecture/  # 01-hybrid-architecture-v2.1.md (主架构, 必读) + 02-transaction + 03-error + ...
  adr/           # 不可变 ADR (12+ 份, 状态追加 ## Status Update 段)
  soc_arch/adr/  # APU SoC 子项目 ADR (D1-D5)
  guide/         # GETTING_STARTED / DEVELOPER / PYTHON_TOOLING / TOPOLOGY_USER
  ONBOARDING.md  # 新人上手 (图谱生成)
  docs_audit_report.md  # 365/365 路径快照

docs-archived/   # 12+ 子目录: dead-code / disabled-tests / samples-orphaned / v1-architecture
openspec/        # 变更提案工作流 (changes/<name>/proposal.md → design.md → specs/ → tasks.md)
examples/        # C++ example_*.cc + Python demo_e2e_*.py + demo_configs/ + generate_*.py
samples/         # 示例拓扑 (含已归档子集, 见 docs-archived/samples-orphaned/)
external/        # git 子模块 (CppHDL, json)
scripts/         # 5 子目录: build/(format.sh, build.sh) | test/(docs_sync_check.sh 等 4) | pipeline/ | topology/ | stats/
cpptlm/          # Python 库 (新, pyproject.toml): cli / topo / config / simulation / analysis / visualization
cpptlm_config/   # Python 配置包 (旧, examples 引用): builder / models / validator / topology_adapter
plans/           # 实施计划: phase7-completion / p0-alignment-remediation / performance-metrics 等
```

子目录级 AGENTS.md 索引: `include/AGENTS.md`（注册宏体系）· `include/tlm/AGENTS.md` · `include/core/AGENTS.md` · `src/core/AGENTS.md` · `src/tlm/AGENTS.md` · `test/AGENTS.md` · `configs/AGENTS.md` · `cpptlm_config/AGENTS.md`

## WHERE TO LOOK

| 任务 | 位置 |
|------|------|
| 添加新 TLM 模块 | `include/tlm/<name>_tlm.hh` + `include/chstream_register.hh` (REGISTER_CHSTREAM) |
| 添加 Legacy 模块 | `include/modules/legacy/` + `include/modules.hh` (需 `BUILD_LEGACY_MODULES=ON`) |
| 写注册宏 | `include/AGENTS.md` (完整宏体系表 + 设计意图) |
| 修改模块工厂 | `src/core/module_factory.cc` (instantiateAll + Step 7 StreamAdapter 注入) |
| 修改 JSON 配置格式 | `configs/` + `include/utils/config_utils.hh` (支持 group/connection/latency + 端口索引 `"xbar.0"`) |
| 添加 StreamAdapter | `include/framework/{stream,multi_port_stream,dual_port_stream,bidirectional_port}_adapter.hh` |
| 添加 Bundle 类型 | `include/bundles/{cache,noc,compute}_bundles_tlm.hh` |
| APU SoC / GPU 拓扑 | `configs/apu_soc_*.json` + `include/tlm/gpu/gpu_tlm.hh` + `examples/generate_apu_soc.py` |
| E2E 集成验证 | `test/test_phase6_integration.cc` (Cache→Crossbar→Memory) + `test/test_complex_topologies_e2e.cc` |
| 提议 / 实施架构变更 | `openspec/changes/<name>/` (proposal.md → design.md → specs/ → tasks.md) |
| 修改 CI | `.github/workflows/ci.yml` (Release/Debug × ASan[OFF,ON] 矩阵) |
| 调试 test fail | `.opencode/skills/cpptlm-debug/SKILL.md` (auto-loads on "test fail") |

## CONVENTIONS

- **缩进**: 4 空格 (`.clang-format: IndentWidth: 4`)；本 AGENTS.md 例外用 2 空格
- **命名**: CamelCase 类 / camelCase 函数 / snake_case 变量 / SCREAMING_SNAKE_CASE 宏
- **注释**: 中文，文件头必含功能/作者/日期
- **双注册表**: `SimObject` (object) vs `SimModule` (module), create 函数类型分离 → 见 `include/AGENTS.md`
- **ChStream 注册**: `REGISTER_CHSTREAM` 宏一次性注册 Object + StreamAdapter + 多端口适配器
- **JSON 端口索引**: `"dst": "xbar.0"` 解析为 `module=xbar, port_index=0`
- **测试标签**: Catch2 `[phaseX]` 按阶段 / `[chstream]` Stream 集成 / `[gpu]` GPU / `[crossbar]` Crossbar
- **CMake**: 显式列源 (`set(CORE_SOURCES ...)`)，禁 GLOB (test/ 例外)；核心库 `cpptlm_core` (静态)
- **零债务原则**: Phase 完成 = 编译通过 + 测试覆盖 + 文档同步；禁 TODO 残留 / 新建 .disabled 测试 / 跳过本地 CI

## COMMANDS

```bash
# 配置 + 编译
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# C++ 测试 (Catch2, 88 文件 / 659 用例)
./build/bin/cpptlm_tests                    # 全部
./build/bin/cpptlm_tests "[chstream]"       # Stream 相关 (84 用例)
./build/bin/cpptlm_tests "[phase6]"         # Phase 6 集成
./build/bin/cpptlm_tests "[crossbar]"       # Crossbar (16 用例)
./build/bin/cpptlm_tests ~"[crossbar]"      # 排除
ctest --test-dir build --output-on-failure -j4  # CMake 注册测试

# 完整套件 (C++ + Python + E2E)
./scripts/test/run_all_tests.sh --quick     # 快速模式
./scripts/test/run_all_tests.sh --python-only  # 纯 Python
./scripts/test/ci_e2e_test.sh               # CI 端到端

# 格式化 (脚本在 scripts/build/, 不在 scripts/ 根目录!)
./scripts/build/format.sh --check           # clang-format 校验
./scripts/build/format.sh                   # 自动修复

# 文档路径同步 (pre-commit 自动跑)
./scripts/test/docs_sync_check.sh --strict  # 当前: 365/365 有效
cat docs/docs_audit_report.md               # 快照报告

# 拓扑验证 (CMake target)
cmake --build build --target validate_topology

# 调试 test fail → auto-loads .opencode/skills/cpptlm-debug/SKILL.md
```

## ANTI-PATTERNS

- **GLOB 源文件**: CMakeLists 用 `set(... 显式列举)`；`test/` 例外
- **直接 `new` 对象**: 必须经 `ModuleFactory::registerObject/registerModule` + `instantiateAll`
- **跳过 StreamAdapter**: `ChStreamModuleBase` 派生类必须 `set_stream_adapter()`，禁直接动 `ChStreamPort`
- **新增 `.disabled` 测试**: `test_config_loader.cc.disabled` 等是历史跳过状态（多数已从磁盘删除），新代码禁创建
- **TODO 残留**: `// TODO: bind_ports_array` 等未完成逻辑必须在 Phase 关闭前清掉或归档
- **跳过本地 CI 验证**: 推送前必跑 `cmake --build` + `ctest`
- **修改 legacy**: `include/modules/legacy/` 仅修严重 bug，新功能走 `include/tlm/`
- **架构性变更未走 OpenSpec**: 重大变更（影响 ≥2 模块 / 公共 API / 配置文件 schema）必须先在 `openspec/changes/` 提案

## DEBUGGING DISCIPLINE (P0-5b, 6 独立根因)

完整流程: `.opencode/skills/cpptlm-debug/SKILL.md`（auto-loads on "test fail" / "fix doesn't work" / "X not received"）。

**4 件套验证**（任何"修复"后必跑）:
```bash
git diff --stat <file>                                     # 1. 改对位置了?
stat -c '%y %n' build/bin/cpptlm_tests src/xxx.cc          # 2. binary 时间戳新于源?
strings build/bin/cpptlm_tests | grep -c "<marker>"        # 3. 修复在 binary 里?
# 4. 修复在执行? 修复前后各加日志对比
```

**6 条铁律**:
1. 诊断用 `static FILE* diag = fopen("/tmp/cpptlm_xxx.log", "a")` + `fflush`，**不要** printf/stderr（Catch2 抑制）
2. test pass 后**立即**清理诊断代码（`grep -l "static FILE\* diag" include/ -r` 一键定位）
3. N 个独立症状 = N 个独立根因；修 1 个后失败模式变了 = 还有根因，不要"逐个试修"
4. 虚函数 override 必须 `= override`（`std::size_t` vs `unsigned` 不构成 override，编译器不警告）
5. 状态设置必须在**最后一个** `reset()` 之后（`PacketPool::acquire()` 调两次 reset，修复放错位置白做）
6. "A→B→A" 响应丢失：按 6 步加 count 日志定位（A 发请求 / B 收到 / B 写 resp / B 发 resp / A 收到 / A 消费）—— count=0 即根因

## DOC HYGIENE (硬性)

- **结构调整 PR 必含**: 同步 `AGENTS.md` STRUCTURE 节 + `scripts/README.md` 子目录表 + `docs/ONBOARDING.md` §5.5 脚本表
- **路径漂移防护**: `scripts/test/docs_sync_check.sh` 扫描 4 个核心文档（`AGENTS.md` / `ONBOARDING.md` / `roadmap.md` / `scripts/README.md`）中所有反引号路径，`--strict` 模式 pre-commit 自动执行
- **VIRTUAL_PATHS**: 文档中提及已删除/归档文件时，必须在 `docs_sync_check.sh` 的 `VIRTUAL_PATHS` 数组添加条目（而非删除段落）
- **ADR 不可变**: `docs/adr/ADR-X.*.md` 签发后不改，状态变化追加 `## Status Update` 段
- **AGENTS.md 层级**: 根 + 子目录 AGENTS.md（域内详细表，如 `include/AGENTS.md` 的注册宏体系）

## KEY INVARIANTS

- **TLM stub 默认**: `USE_SYSTEMC_STUB=ON`（根 CMakeLists.txt），无外部 SystemC 依赖
- **ccache**: 自动检测，未安装降级（非 fatal）
- **ASan**: `USE_ASAN=ON` 仅 Debug 有效（CI 矩阵排除 Release+ASan）
- **构建产物**: `build/bin/` 可执行 + `build/lib/cpptlm_core.a` 静态库
- **测试状态**: **659/659 pass**（2026-06-19，88 个 test_*.cc）+ **222/222 Python** pass

## 归档索引（docs-archived/）

- `samples-orphaned/`: `simple1/`（`cpu_cluster` DEPRECATED，推荐 `include/tlm/cpu_tlm.hh`）+ `simple_hier/`（孤儿，引用已删除的 `CpuCluster`/`NOCTile`）
- `dead-code-headers-2026-q2/`: v2.1 归档的 `ext/packet_to_payload.hh` 等
- `v1-architecture/` / `v2-architecture/`: 旧架构图与决策记录
- `p0-p1-architecture-debt-fix-v2/`: P0/P1 修复记录（issues/learnings/decisions）
- `superpowers/`（2026-06-22 清理）: `plans/` 13 个 + `specs/` 5 个（已完成/已被取代/未执行的 planning 与设计文档）；活跃内容见 `docs/superpowers/{plans,specs}/`
- `plans/`（2026-06-22 清理）: 项目根 `plans/` 下 10 个文件（已完成/已被取代/一次性报告的 planning 文档）；活跃规划见 `docs/superpowers/plans/2026-06-20-future-work-roadmap.md`
- 恢复方法：各子目录 `README.md`

## MCP TOOLS (code-review-graph)

`.mcp.json` 已配置 `code-review-graph` server。**优先于** Grep/Glob/Read（图谱更快、更便宜，给出调用链/测试覆盖等结构上下文）。

| 场景 | 工具 |
|------|------|
| 探索代码 | `semantic_search_nodes` / `query_graph` |
| 影响分析 | `get_impact_radius` / `get_affected_flows` |
| 代码审查 | `detect_changes` + `get_review_context` |
| 架构视图 | `get_architecture_overview` + `list_communities` |
| 重构 | `refactor_tool`（rename / dead_code / suggest） |

Graph 通过 hooks auto-update。Fallback to Grep only when graph doesn't cover。
