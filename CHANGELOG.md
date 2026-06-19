# Changelog

## [v2.3] - 2026-06-19

### 修复 (Fixes)
- **D.1 PortManager Mirror**: `SimModule::getInternalOutputPort` 对 ChStream 模块返回非空 (was nullptr)。
  新增 `PortManager::mirrorExistingDownstreamPort` / `mirrorExistingUpstreamPort` 方法 (仅入 map)。
  `ModuleFactory` Step 7 创建 ChStream 端口后镜像注册到子模块 PortManager。

### 新增 (Features)
- **CoherentXBarTLM**: APU 顶层跨域 snoop 广播总线 (继承 CrossbarTLM)。
  Phase 7.A/7.B = write-through 透传; 6×6 state table 留 Phase 7.C。
  `apu_soc_v1.json` 顶层 `top_xbar` 从 `CrossbarTLM` 升级为 `CoherentXBarTLM`。

### 重构 (Refactor)
- **Helper Safety**: 移除 P3 helpers (`CacheTLM::connectBus` / `CrossbarTLM::connectCPUSideBus` /
  `connectMemSideBus`) 的 lazy registration + 死 throw 路径, 改为清晰报错。
  符合零债务原则: 配置错误早暴露优于静默 fallback 到 {4} buffer。

### 测试 (Tests)
- 新增 11 用例: 4 (D.1 port visibility) + 4 (CoherentXBarTLM) + 3 (helper safety)
- 升级 1 用例: `test_simmodule_nested.cc` L195-206 WARN → REQUIRE
- 修复 5 用例: `test_chstream_helpers.cc` 显式注册依赖 port (替换 lazy)
- 总数: 673/673 → 684/684

## [v2.2.0] - 2026-06-18

### Added
- **Added** `CpuCluster` 增强：override `tick`/`set_config`/`get_module_type`，支持 N 层 JSON 嵌套 + `MAX_DEPTH=8` 限深
- **Added** `configs/example_simmodule_nested_{2level,3level_static}.json` 示例配置
- **Added** `test/test_simmodule_nested.cc` 7 用例（含跨 cluster outputs 暴露端口 E2E）+ `test/python/test_simmodule_emitter.py` 6 用例
- **Added** `include/tlm/cluster/cpu_cluster.hh` (CpuCluster 移出 legacy, 无条件注册; 实现为 header-only inline)
- **Added** `SimModule::simulate_instantiate()` 公共方法 + `static thread_local int depth_` + `MAX_DEPTH=8` 限深护栏
- **Added** `include/tlm/cluster/` 子目录, 预留 `MemoryCluster`/`CacheCluster` 未来扩展
- **Added** `docs/migration-v2.2.md` (CPUSim → CPUTLM 迁移指南 + 路径替换速查表)

### Changed
- `include/AGENTS.md`: 移除 `BUILD_LEGACY_MODULES` 守卫说明, `REGISTER_OBJECT` 标注 no-op, `REGISTER_MODULE` 标注无条件
- `include/tlm/AGENTS.md`: 模块列表加 CpuCluster 行, 新增 `cluster/` 子目录说明
- `configs/AGENTS.md`: 移除"已归档"段 `BUILD_LEGACY_MODULES` 描述, 新增"SimModule 嵌套 JSON" schema 示例段
- `include/modules.hh`: 重构（移除 `#ifdef BUILD_LEGACY_MODULES` 守卫）

### Removed
- **Removed** `BUILD_LEGACY_MODULES` CMake option (`AGENTS.md` migration path updated)
- **Removed** `include/modules/legacy/` 目录 (`cpu_sim.hh` removed; `cpu_cluster.hh` moved to `include/tlm/cluster/`)
- **Removed** `CPUSim` 类型（由 `CPUTLM` (`include/tlm/cpu_tlm.hh`) 替代, 走 `REGISTER_CHSTREAM`）
- **Removed** `REGISTER_OBJECT` 宏的 `CPUSim` 注册代码（宏体改为 no-op 注释）

### Fixed
- `ModuleFactory` 顶层 `instantiateAll` 触发 `SimModule::simulate_instantiate`（修复嵌套激活死代码问题）
- `ModuleFactory` 安全的 per-factory cleanup（避免跨实例析构冲突）
- `ModuleGroup::eraseInstance` 公开（支持 dynamic_cast + cleanup 路径）

### BREAKING
- **BREAKING** v2.1.x 引用 `BUILD_LEGACY_MODULES=ON` 项目需迁移到 CPUTLM（见 `docs/migration-v2.2.md`）
- **BREAKING** `#include "modules/legacy/cpu_sim.hh"` 路径不存在, 需改为 `#include "tlm/cpu_tlm.hh"`
- **BREAKING** `#include "modules/legacy/cpu_cluster.hh"` 路径不存在, 需改为 `#include "tlm/cluster/cpu_cluster.hh"`
- **BREAKING** JSON `"type": "CPUSim"` 不再识别, 需改为 `"type": "CPUTLM"`

## [Unreleased]

### Added
- Phase 7.A GPU 基础设施落地（2026-06-11）
  - `include/bundles/compute_bundles_tlm.hh` — ComputeReqBundle / ComputeRespBundle 类型
  - `include/tlm/gpu/gpu_tlm.hh` — GPUTLM v0 黑盒发起器
  - `REGISTER_CHSTREAM` 宏注册 GPUTLM（同时绑定 StreamAdapter 与多端口适配器）
  - `configs/gpu_standalone.json` — 验证用最小拓扑配置
  - `test/test_gpu_standalone.cc` — 5 个 `[gpu]` 标签单元测试
  - AGENTS.md STRUCTURE 节新增 `include/tlm/gpu/` 子目录条目（Phase7.A+ GPU 模块）

### Notes
- Phase 7.A 状态: 🟡 Pending → ✅ Done
- 后续: Phase 7.B–7.F（ComputeUnitTLM / Coherence / TCC / Multi-CU / Full APU Demo）保持 🟡 Pending

## P1 - 2026-06-19: SimModule API 修复

- fix(simmodule): make findInternalPath recurse to child SimModule (D.4)
  解锁 3 层 JSON 配置端到端工作
- fix(simmodule): add default recursive tick in SimModule base (D.5)
  为新 SimModule 子类 (P2 ComputeCluster 等) 提供默认 tick
- test: 3-level JSON config end-to-end regression (P1-T1.5)

## P2-P5 - 2026-06-19: SimModule 多级层次 + JSON 模板复用

### P2: GPU 端 4 核心 SimModule 类
- feat(simmodule): add ComputeCluster with cu_template/cu_count reuse
- feat(configs): add compute_unit_v1.json blueprint template
- feat(simmodule): add TpcCluster + GpcCluster + GpuCluster (4-level GPU hierarchy)
- feat(register): add modules_cluster.hh for 4 GPU SimModule + CpuCluster
  - 重构 REGISTER_MODULE 宏为参数化 (从无参 statement 改为 template 注册)
- test(simmodule): add 4 GPU SimModule integration tests (4 cases)
- 关键修复: SimModule::simulate_instantiate 缺 virtual + cfg schema {modules, connections}

### P3: ChStream helper 方法 (partial)
- feat(chstream): add connectBus/connectCPUSideBus/connectMemSideBus helpers
- connectCPU 依赖 D.1 修复, 暂未实施

### P4: 基础设施 3 类
- feat(simmodule): add CacheCluster + MemoryCluster + GpuNoC
  - CacheCluster: L1×N (私有) + L2 (共享)
  - MemoryCluster: 多通道 HBM/DDR + Arbiter
  - GpuNoC: Garnet 风格 NxN mesh (routers)

### P5: 顶层 + incorporate_parent
- feat(simmodule): add ApuSoC top + incorporate_parent hook
- apu_soc_v1.json: 完整 APU (CPU + GPU + CrossbarTLM) 端到端
- incorporate_parent 借鉴 gem5 incorporate_cache(board) late-binding
- test(python): add apu_soc emitter tests (2 cases)

### 统计
- 10 个 SimModule 派生类 (CpuCluster + 8 新 + 1 顶层 ApuSoC 算容器)
- 31 新增文件 + 12 修改文件
- 659/659 C++ tests + 2/2 Python tests pass

## [v2.1.0] - 2026-06-08

### Added
- tlm_stub multi-extension support (SystemC TLM 2.0 API compatible)
  - tlm_extension_registry singleton with type_index → ID mapping
  - tlm_array<T> for O(1) indexed extension storage
  - new release_extension<T>() API (delete + nullify)
  - new resize_extensions() / free_all_extensions() / deep_copy_from() APIs
- test_tlm_multi_extension.cc with 12 test cases (auto-globbed by test/CMakeLists.txt)
- docs/adr/ADR-X.13-stub-multi-extension.md
- .github/workflows/ci.yml code-format job (aligns with AGENTS.md)
- BUILD_LEGACY_MODULES CMake option (default OFF, #ifdef guard for legacy CPUSim/CpuCluster)
- include/AGENTS.md: BUILD_LEGACY_MODULES documentation in registration macro table

### Changed
- **BREAKING**: tlm_extension<T>::ID migrated from per-TU function-local static
  to class static const (compile-time registration, TU-safe)
- TransactionContextExt explicitly released on MemoryV2 error path
  (modules_v2.hh:79 — semantic clarity, no longer relies on stream_id fallback)
- ~tlm_generic_payload() destructor and reset() now loop-delete all extensions
  (was: single delete of a single extension pointer)
- include/core/ext/cmd_exts.hh reduced to macros-only library
  (deleted duplicate ReadCmdExt/WriteCmdExt/etc. class definitions; only
  ReqIDExt is unique to cmd_exts.hh)
- CMakeLists.txt version synced from 2.0.3 to 2.1.0
- docs/architecture/README.md index updated to v2.1
- docs/architecture/01-hybrid-architecture-v2.1.md: §4.4 pseudocode corrected
- docs/architecture/01-hybrid-architecture-v2.1.md: Bundle description updated to lightweight
- docs/architecture/02-complex-topology-architecture.md: class names updated
  (MeshRouter→RouterTLM, Processor→CPUTLM, Bus→BusSim, Crossbar→CrossbarTLM)
- docs/architecture/02-transaction-architecture.md: version label v1.0→v2.1
- include/AGENTS.md: dual registry design intent documentation
- include/core/AGENTS.md: dual registry motivation paragraph
- include/modules.hh: design intent header comment added

### Removed
- **BREAKING**: USE_SYSTEMC build option (always use TLM stub)
- external/systemc/ directory (was placeholder README only, not a submodule)
- src/sc_main.cpp (10-line empty stub; only existed behind USE_SYSTEMC=ON guard)
- extern "C" int sc_main placeholder at src/main.cpp:21
- src/cpu_main.cpp and src/traffic_main.cpp (15 + 22 lines of placeholder
  main() with 2 TODOs in v2.1 upgrade backlog). Use cpptlm_sim with configs/ instead.
- include/core/ext/packet_to_payload.hh and payload_to_packet.hh
  (zero .cc users; archived to docs-archived/dead-code-headers-2026-q2/)
- mock_modules.hh duplicate #ifdef USE_SYSTEMC_STUB nesting
- test/*.cc, include/ext/*.hh, include/core/ext/*.hh:16 files'
  #ifdef USE_SYSTEMC_STUB blocks (USE_SYSTEMC option is gone; only stub path)
- include/modules/legacy/modules_v2.hh (208-line v2 module definitions, archived)
- 5 test files migrated from v2 to TLM modules
  (test_phase5_modules.cc, test_phase6_regression.cc, test_phase7_transaction_lifecycle.cc,
   test_phase8_performance_stress.cc, test_p3_2_tlm_integration.cc)

### Fixed
- MemoryV2 error path no longer silently destroys upstream
  TransactionContextExt (modules_v2.hh:79 + Phase 1c multi-extension
  array implementation co-fix)
- Type ID cross-TU consistency (no more per-TU local static counter)
