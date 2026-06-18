## 1. 基础设施：SimModule 限深 + CpuCluster 增强

- [x] 1.1 修改 `include/core/sim_module.hh`：加 `static constexpr int MAX_DEPTH = 8` + `static thread_local int depth_` 成员 + `MAX_DEPTH` 编译宏覆盖点
- [x] 1.2 修改 `include/core/sim_module.hh`：新增公共方法 `void simulate_instantiate(const json& cfg)`，实现：入口幂等守卫（`internal_factory->getAllInstances()` 非空则早退）→ `++depth_` → 构造 RAII `DepthGuard`（析构时 `--depth_`，异常安全）→ depth_ 越界抛 `runtime_error` → `internal_factory->instantiateAll(cfg)` + `parsePortConfigs(cfg)` → 遍历子模块用 `dynamic_cast<SimModule*>` 识别后递归触发其 `simulate_instantiate`
- [x] 1.3 修改 `include/core/sim_module.hh`：保留现有 `instantiate(const json& cfg)` 作为"构造期初始化"入口，**改为委托** `simulate_instantiate`（保持向后兼容）
- [x] 1.4 新建 `include/tlm/cluster/cpu_cluster.hh`：声明 CpuCluster 类（继承 SimModule），含 `num_cpus_/cluster_id_` 成员 + `get_module_type/set_config/tick` override
- [x] 1.5 新建 `include/tlm/cluster/cpu_cluster.cc`：实现 `set_config`（解析 num_cpus/cluster_id）+ `tick`（转发到 `internal_factory->getAllInstances()`）
- [x] 1.6 修改 `include/modules.hh`：删除 `#ifdef BUILD_LEGACY_MODULES ... #else ... #endif` 守卫；`REGISTER_OBJECT` 改为 no-op 注释；`REGISTER_MODULE` 改为无条件 `ModuleFactory::registerModule<CpuCluster>("CpuCluster");`
- [x] 1.7 验证编译：`cmake --build build -j$(nproc)` 通过，零 warning（CpuCluster 实现期间）

## 2. legacy 目录与 CPUSim 退场

- [x] 2.1 全局搜索 CPUSim 引用点（覆盖**所有 git tracked 文件**，含 `docs/`、`docs-archived/`、`.github/`、CMake、CMakeLists）：`git ls-files | xargs grep -l "CPUSim"`（**排除** `openspec/changes/json-nested-simmodule/` 自身、`CHANGELOG.md` 历史变更说明、`docs/migration-v2.2.md` 迁移指南；这三处允许 CPUSim 字符串）
- [x] 2.2 逐个迁移 CPUSim 引用到 CPUTLM：测试代码 (`test/*.cc`)、示例代码 (`examples/*.cc`)、JSON 配置 (`configs/*.json`、历史归档 `docs-archived/`)
- [x] 2.2.1 **完成度校验**：迁移后重跑 2.1 命令，确认**剩余 CPUSim 引用仅 3 处**（`CHANGELOG.md` + `docs/migration-v2.2.md` + `openspec/changes/json-nested-simmodule/` 自身），**其他目录 0 命中**；未达标则继续迁移直到通过
- [x] 2.3 删除 `include/modules/legacy/cpu_sim.hh` 文件
- [x] 2.4 删除 `include/modules/legacy/cpu_cluster.hh` 文件（**确认内容已移至** `include/tlm/cluster/cpu_cluster.hh`）
- [x] 2.5 删除 `include/modules/legacy/README.md` 文件
- [x] 2.6 `rmdir include/modules/legacy/` 空目录
- [x] 2.7 验证目录物理删除：`ls include/modules/legacy/` 应返回 `No such file or directory`
- [x] 2.8 修改 `CMakeLists.txt`：删除 `option(BUILD_LEGACY_MODULES ...)` 行（原第 44 行）
- [x] 2.9 修改 `src/CMakeLists.txt`：删除 `if(BUILD_LEGACY_MODULES) ... target_compile_definitions(...)` 块（原第 40-44 行）
- [x] 2.10 验证全局无残留：`grep -rn "BUILD_LEGACY_MODULES" .` 仅命中 `.git/` 与 `openspec/changes/json-nested-simmodule/`（change 自身）
- [x] 2.11 验证编译 + 610 C++ 测试全绿：`ctest --test-dir build --output-on-failure -j4`

## 3. C++ Catch2 测试

- [ ] 3.1 新建 `test/test_simmodule_nested.cc`：用例 1 `1 层嵌套 - 内部 SimObject`（CpuCluster + CPUTLM × 4 + CacheTLM + MemoryTLM）
- [ ] 3.2 用例 2 `顶层 + 暴露端口 outputs/inputs`（验证 findInternalPath/isExposedPort/getInternalOutputPort 端到端）
- [ ] 3.3 用例 3 `限深护栏 - depth > 8 抛错`（构造 9 层深嵌套触发异常 + 验证 depth_ 恢复）
- [ ] 3.4 用例 4 `JSON 静态 2 层但运行时只激活 1 层` —— **NOTE**: 用户最终确认"N 层运行时嵌套"，本用例改为验证"2 层运行时均激活"
- [ ] 3.5 用例 5 `CpuCluster E2E - CPUTLM→CacheTLM→MemoryTLM 数据流`（100 周期后 last_response_transaction_id_ > 0）
- [ ] 3.5.1 用例 5b `CpuCluster outputs 暴露端口 E2E`：JSON 配置 CpuCluster 暴露 `outputs: [{"internal": "cpu0.req_out", "external": "cpu0_to_bus"}]`，外部 `connections: [{src: "cluster.cpu0_to_bus", dst: "bus.cpu_in", ...}]` 串接外部 `bus` 模块；100 周期后 `bus` 收到来自 `cpu0` 的真实 transaction（`bus.last_received_addr` 等于 cpu0 发出的地址），**证明跨暴露端口的数据流贯通**
- [ ] 3.6 用例 6 `CpuCluster set_config(params) 透传`（num_cpus=8 / cluster_id 显式 / 默认 num_cpus=4）
- [ ] 3.7 修改 `test/CMakeLists.txt`：注册 `test_simmodule_nested.cc` 到 `cpptlm_tests` 目标
- [ ] 3.8 运行 `./build/bin/cpptlm_tests "[simmodule]"` 全绿（7/7 pass：3.1-3.6 + 3.5.1）
- [ ] 3.9 运行 `./build/bin/cpptlm_tests` 全绿（617/617 pass：原 610 + 新 7，含 3.5.1）

## 4. C++ 示例

- [ ] 4.1 新建 `examples/example_simmodule_nested.cc`：main 函数加载 `configs/example_simmodule_nested_2level.json`，打印拓扑（modules / connections / depth）、调用 simulate_instantiate、跑 100 周期 E2E
- [ ] 4.2 修改 `examples/CMakeLists.txt`：注册 `example_simmodule_nested` 可执行目标
- [ ] 4.3 验证编译：`cmake --build build --target example_simmodule_nested`
- [ ] 4.4 运行 `./build/bin/example_simmodule_nested configs/example_simmodule_nested_2level.json`：输出含 "CpuCluster constructed", "Simulate instantiate depth=2 OK", "100 cycles completed, last_response_transaction_id=N"

## 5. Python 示例与 JSON 配置

- [ ] 5.1 新建 `configs/example_simmodule_nested_2level.json`：顶层 CpuCluster 内部 4 CPUTLM + 1 CacheTLM + 1 MemoryTLM + outputs/inputs 暴露端口
- [ ] 5.2 新建 `configs/example_simmodule_nested_3level_static.json`：三层 CpuCluster 嵌套，每层结构 + 暴露端口
- [ ] 5.3 验证 JSON 加载：`./build/bin/cpptlm_sim --config configs/example_simmodule_nested_2level.json`（如 main 入口支持）
- [ ] 5.4 新建 `examples/generate_nested_soc.py`：用 `cpptlm.topo` + `cpptlm.library` 生成多层嵌套 JSON config，保存到 `configs/example_emitter_nested.json`，调用 `cpptlm.simulate.run()` 跑端到端
- [ ] 5.5 运行 `python examples/generate_nested_soc.py`：输出生成的 JSON 路径 + 仿真完成日志

## 6. Python pytest 测试

- [ ] 6.1 新建 `test/python/test_simmodule_emitter.py`：用例 `test_emit_1level()`（单层 CpuCluster JSON 正确性 + 可被 C++ ModuleFactory 加载）
- [ ] 6.2 用例 `test_emit_2level_static()`（2 层嵌套 JSON 正确）
- [ ] 6.3 用例 `test_emit_3level_static_runtime_nlevel()`（3 层 JSON 静态 OK，运行时 3 层均激活）
- [ ] 6.4 用例 `test_emit_outputs_inputs_expose()`（outputs/inputs 字段生成正确 + 反向解析）
- [ ] 6.5 用例 `test_emit_cpp_load()`（生成的 JSON 实际被 C++ ModuleFactory 加载不报错）
- [ ] 6.6 用例 `test_emit_with_chstream_modules()`（含 ChStream 模块的嵌套配置 + 端到端 req/resp）
- [ ] 6.7 修改 `test/python/AGENTS.md` 或 `pyproject.toml`：注册新测试文件
- [ ] 6.8 运行 `pytest test/python/test_simmodule_emitter.py -v`：6/6 pass
- [ ] 6.9 运行 `pytest test/python/ -v` 全绿（213/213 pass：原 207 + 新 6）

## 7. 文档同步

- [ ] 7.1 修改 `include/AGENTS.md`：注册宏体系段移除 BUILD_LEGACY_MODULES 守卫说明；REGISTER_OBJECT 加 no-op 注释；REGISTER_MODULE 标注无条件
- [ ] 7.2 修改 `include/tlm/AGENTS.md`：模块列表加 CpuCluster 行（含 cluster/ 子目录说明）
- [ ] 7.3 修改 `configs/AGENTS.md`：移除"已归档"段 BUILD_LEGACY_MODULES 描述；加"SimModule 嵌套 JSON" schema 示例段
- [ ] 7.4 新建 `docs/migration-v2.2.md`：CPUSim → CPUTLM 迁移指南 + `#include "modules/legacy/..."` → `#include "tlm/..."` 路径变更
- [ ] 7.5 修改 `CHANGELOG.md`：v2.2 条目（含 Removed/Added/BREAKING 三个子项，参考 `legacy-modules-removal/spec.md` Scenario "CHANGELOG.md v2.2 条目"）
- [ ] 7.6 修改 `include/tlm/AGENTS.md` 子目录说明：加 `cluster/` 子目录索引
- [ ] 7.7 运行 `scripts/test/docs_sync_check.sh --strict`：退出码 0，路径 100% 有效

## 8. CI 验证与归档准备

- [ ] 8.1 完整编译：`cmake --build build -j$(nproc)` 零 error 零 warning
- [ ] 8.2 C++ 测试：`ctest --test-dir build --output-on-failure -j4` 全绿
- [ ] 8.3 Python 测试：`pytest test/python/ -v` 全绿
- [ ] 8.4 E2E 脚本：`./scripts/test/run_all_tests.sh --quick` 通过
- [ ] 8.5 格式检查：`./scripts/build/format.sh --check` 通过
- [ ] 8.6 完整 Lint：`./scripts/test/docs_sync_check.sh --strict` 通过
- [ ] 8.7 更新 `openspec/AGENTS.md`（如存在）：加 `json-nested-simmodule` change 状态
- [ ] 8.8 准备归档：所有 task 100% 完成 → 运行 `openspec archive json-nested-simmodule`
