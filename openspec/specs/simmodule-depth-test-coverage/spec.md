# simmodule-depth-test-coverage Specification

## Purpose
TBD - created by archiving change thread-local-depth-isolation-test. Update Purpose after archive.
## Requirements
### Requirement: simmodule-depth-guard 必须有显式 C++ 测试覆盖

`simmodule-depth-guard` spec 现有 8 个 Scenarios 中，2 个 MUST 由对应的 C++ TEST_CASE 显式覆盖（不允许仅由其他测试间接覆盖）："8 层嵌套 depth=8 不抛错（边界）"（line 18-21）与"thread_local 在多线程隔离"（line 33-36）。本 requirement 跟踪这 2 个测试覆盖任务，作为 `AGENTS.md` 零债务原则（"Phase 完成 = 编译通过 + 测试覆盖 + 文档同步"）的闭环验证。

#### Scenario: 8 层深度边界测试存在

- **WHEN** `test/test_simmodule_nested.cc` 包含 `TEST_CASE("SimModule 8-level boundary: ...", "[simmodule][depth][boundary]")` 并构造 8 层 `CpuCluster` 嵌套（最内层持有 1 个 `CPUTLM`）
- **THEN** `factory.instantiateAll(cfg)` 返回 `true`（不抛 `std::runtime_error`）；`SimModule::getCurrentDepth() == 0`（RAII 恢复）；最内层 `CpuCluster` 通过 `getInternalInstance("cpu0")` 返回非空 `CPUTLM*` 指针
- **AND** 编译时 `SimModule::MAX_DEPTH == 8`（不修改编译宏），且测试**不**依赖 9 层超限用例的隐式覆盖

#### Scenario: thread_local 多线程隔离测试存在

- **WHEN** `test/test_simmodule_nested.cc` 包含 `TEST_CASE("SimModule thread_local depth isolation: ...", "[simmodule][depth][threads]")` 启动 2 个 `std::thread`，线程 A 跑 5 层嵌套 `CpuCluster`、线程 B 跑 3 层嵌套，各自独立 `EventQueue` + `ModuleFactory`
- **THEN** 两线程通过 `std::promise<int>`/`std::future` 同步完成后，主线程 `SimModule::getCurrentDepth() == 0`（不受两线程并发 `simulate_instantiate` 影响）；线程 A 内部 `depth_` 周期 1→5→0 完整；线程 B 内部 1→3→0；两线程 `depth_` 互不污染
- **AND** 测试**不**使用 `std::async`（可能 deferred 到同一线程）、**不**使用 `sleep_for` 等时序依赖、**不**共享堆数据

