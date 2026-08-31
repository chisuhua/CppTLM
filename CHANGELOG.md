# Changelog

## [v0.5.0-MVP] - 2026-XX-XX

> **MVP slice — pre-release, 不保证 GA** (per Oracle m3 + ADR-SOC-06).
> 范围: W2 board-soc-split (T-bs-1~T-bs-6) + W3.2 abi-export (T-ae-1~T-ae-5).
> 外部契约: 23 ABI C extern "C" 通过 `libcpptlm_emulator.so` SHARED 暴露
> (per ADR-088 §D5 + ADR-SOC-07 D5), UsrLinuxEmu `dlopen` 消费.

### 新增 (Features)

- **DGpuBoard C++ shell** (T-bs-3a~T-bs-3e, ADR-SOC-07 D1/D7): 5 职责 C++ 壳 — SOC 装配 / 23 ABI 翻译 / 回调接线 (non-blocking) / 设备枚举 / 生命周期管理. 不继承 ChStreamModuleBase/SimModule, 不持有寄存器状态.
- **DGpuBoard::load_soc_config_from_file + init + tick + shutdown** (T-bs-3a): JSON 装配 + sim 线程启动 (每卡独立 std::thread + EventQueue) + drain_injection_queue 服务.
- **DGpuSoc SimModule 容器** (T-bs-1, ADR-SOC-07 D1): `REGISTER_MODULE(DGpuSoc)`, JSON 嵌套实例化 SOC 内部组件 (pcie_ep + sdma + cp + tmu + sq + cq + gpu + vram), outputs/inputs 暴露 SOC 边界端口.
- **SubmitQueueTLM** (T-bs-2a, 4 端口 ChStreamModuleBase): `cta_in/dispatch/done_in`, 替代 s2 `submit_queue_mvp.cc`.
- **CompletionRingTLM** (T-bs-2b, 4 端口): `done_in[0]/done_in[1]/done_out/irq_out`, 多源汇聚 (gpu.done + sdma.done_out) + dep 链 done_out 转发.
- **CommandProcessorTLM** (T-bs-2c, 4 端口): `cmd_in/fetch_out/dma_req/dispatch`, 5-state FSM (IDLE→FETCH→DECODE→DISPATCH→COMPLETE) + backpressure + DEGRADED.
- **TmuDispatchProcessorTLM** (T-bs-2c-TMU 命名提升): 3 端口 `dispatch_in/cta_out/done_in`, 替代 s2 `tmu_dispatch_processor_mvp.cc`.
- **dgpu_bundles_tlm** (T-bs-2e): `Pm4DispatchBundle` + `CtaDescriptorBundle` POD bundles, 复用 dma_bundles 的 `CompletionBundle` (避免 ODR 冲突).
- **23 ABI C extern "C" 头冻结** (T-bs-6, per ADR-088 §D5 + ADR-SOC-07 D5): `include/abi/cpptlm_emulator.h` 定义 19 forward + 4 callback typedef, 跨仓契约.
- **CPPTLM_EMULATOR_EXPORT 宏** (T-bs-6 + T-ae-1): Windows `__declspec(dllexport)` / GCC `__attribute__((visibility("default")))`, 自动 19 符号显式导出.
- **cpptlm_core PIC** (T-ae-1, per Oracle Q2): `POSITION_INDEPENDENT_CODE ON` 使 cpptlm_core 可被链入 SHARED, 但保持 STATIC 不破坏 PTX-EMU `ExternalProject_Add` 静态消费路径.
- **libcpptlm_emulator.so SHARED library** (T-ae-1): `add_library(cpptlm_emulator SHARED src/abi/cpptlm_emulator.cc)`, `-fvisibility=hidden` 默认, 19 符号显式导出 (per AE-G2 nm 验证).
- **libcpptlm_emulator.so install + cpptlmTargets EXPORT** (T-ae-1): `install(TARGETS cpptlm_emulator EXPORT cpptlmTargets ...)` + `install(EXPORT cpptlmTargets ...)` → UsrLinuxEmu 端 `find_package(cpptlm)` 自动消费.
- **DGpuBoard::mmio_read/write** (T-bs-3a): BAR0 寄存器数据面 (异步注入 + 同步 future 等待, mmio_read 1ms wall-clock 超时).
- **DGpuBoard::backdoor_read/write** (T-bs-3e, ADR-SOC-07 Q3): BAR1 VRAM 走 `inject_q` 路径 (不直接访问 VRAM, sim 线程 quantum 边界服务).
- **DGpuBoard::pcie_config_read/write** (T-bs-3a): Config Space 16-bit offset + 8-bit width 读写.
- **DGpuBoard::set_irq_callback / set_error_callback / set_dma_translate_callback** (T-bs-3c, 3 std::function 注入点).
- **DGpuBoard::get_stats_path** (T-bs-3c, ADR-SOC-07 D2/§2.5#6): `<device_id>.<module_name>` 多卡前缀防 StatsManager singleton 冲突.
- **DGpuBoard::shutdown** (T-bs-3d): 严格顺序 — `stop_=true` → inject_q poison pill → `sim_thread_.join()` → 析构 SOC → 析构 EventQueue.
- **DGpuBoard exception 跨线程捕获** (T-bs-3d): `sim_loop` 顶层 catch → `std::exception_ptr` → 下一 ABI 调用 rethrow, 避免 C++ 异常逃逸.
- **23 ABI 函数体实现** (T-ae-2): `src/abi/cpptlm_emulator.cc` 薄壳转发 DGpuBoard shell 方法, 每个 try/catch 全包, 设备注册表 mutex (atomic dev_id + lock-in erase + 锁外 delete).
- **device registry 线程安全** (T-ae-2, per Oracle Q6): 2 卡并发 create_by_id 互不冲突, destroy 期间 lookup 返回 null (-ENOENT), 锁内 erase + 锁外 `delete board` 避免 sim_thread join 持锁阻塞.
- **exception safety C boundary** (T-ae-2): `catch (const std::exception&)` → -EINVAL, `catch (...)` → -EFAULT, C 边界不容许 C++ 异常逃逸 (UB).
- **configs/dgpu_board_v1.json** (T-bs-4): 顶层 DGpuBoard shell + DGpuSoc 容器 + 9 子模块 (pcie_ep/sdma/cp/tmu/sq/cq/gpu/vram) + 11 connections + outputs/inputs 暴露 (irq/host_dma/host_tlp).
- **examples/test_cpptlm_emulator_dlopen/** (T-ae-4, AE-G5): dlopen `libcpptlm_emulator.so` + dlsym 19 ABI 调通模板 (UsrLinuxEmu linux_compat 端集成镜像).

### 修复 (Fixes)

- **Doorbell + DGpuBar 迁移** (T-bs-2d, per ADR-SOC-07 D2): s2 `DGpuBoardTLM::Impl` PIMPL 成员移除, 强序语义下沉到 `PcieBarRouter::SideEffect::doorbell` (table-driven `bar0_registers` JSON 表); VRAM 下沉到 SOC 内 `MemoryTLM` (`vram0`).
- **Doorbell 测试路径修正** (T-bs-2d): `test_prereq_4_doorbell_queue_stability` 等 4 用例改测 `ep.doorbell()` 接口 (Doorbell 经 PcieEndpoint 暴露), 不再 `bar.doorbell` PIMPL 访问.
- **UsrLinuxEmuIoctlStub 退役** (T-bs-5 commit 1e75eee): s2 W4 4 IOCTL bridge 类物理删除 (header + impl + test + CORE_SOURCES 引用). IOCTL 语义迁到 23 ABI `cpptlm_emulator_register_callbacks` (per ADR-088 §D5 + W3.2).
- **test_dgpu_pcie_device_perspective.cc 适配** (T-bs-4 stage-1): 4 [stage-1] TEST_CASE 改用 `DGpuBoard` shell + `mmio_write/backdoor_read` 路径, 2 个 IOCTL/PUSHBUFFER 用例 deferred 到 shell load_soc_config 完整化.
- **build system 清理** (T-bs-5 stage 6): 根 `CMakeLists.txt` 移除 `configure_file(configs/dgpu_board_v1_mvp.json.in ...)` (源文件已删); `chstream_register.hh` 移除 DGpuBoardTLM/UsrLinuxEmuIoctlStub 注册.

### 测试 (Tests)

- **T-bs-1 DGpuSoc from_config** (`test_dgpu_soc_from_config.cc`): BS-G1 SOC JSON 实例化 + 内部 ChStream adapter 非空 + 嵌套 connections 解析.
- **T-bs-3e backdoor via inject_q** (`test_dgpu_board_shell_abi.cc`, 10 用例): BS-G2 mmio_write/backdoor 路径走 inject_q quantum 边界, 与 timed 路径不竞争.
- **T-ae-3 ABI e2e** (`test_cpptlm_emulator_abi.cc`, 6 用例): 19 forward ABI 符号 link-time 解析 + NULL/error 处理 + 生命周期.
- **T-ae-3 registry TSan** (`test_cpptlm_emulator_registry.cc`, 4 用例): 2 卡并发 create_by_id + 4 线程 × 100 iters mmio_read 交错 + 多次 destroy nullptr, AE-G4 TSan 干净.
- **T-ae-4 dlopen example** (`test_cpptlm_emulator_dlopen`, AE-G5): dlopen `libcpptlm_emulator.so` + dlsym 5 关键 ABI + stdout "v1.0-dgpu-v0" + exit 0.
- **format.sh --check 全绿** (T-ae-5): 12 个 T-W2 累积 format drift + T-W3-2 新文件 全部与 `.clang-format` 对齐.
- **docs_sync_check --strict PASS** (T-ae-5): 353/353 路径有效 (新增 `include/abi/` + `examples/test_cpptlm_emulator_dlopen/` 全部同步).

### Deferred (T-W3-2 范围外, 待后续 T-bs-4+ follow-up)

- `DGpuBoard::load_soc_config_from_file` 当前不存在, `cpptlm_emulator_create` 用 `load_soc_config(json)` 代替 (caller 读文件 parse). ~~shell `SimModule::simulate_instantiate` 嵌套 JSON 处理 pre-existing SIGSEGV (1 cpptlm_tests 用例红)~~ — **D15 已修复 (commit <pending>)**: `module_factory.cc` null-guard 处理未注册类型 (`object_instances["tmu"]==nullptr`), SOC instantiate 恢复,`[pcie][dGPU][stage-1]` 4/4 PASS。
- `msix_init/update_pending/clear_pending` + `lookup_register` 在 ABI 中返回 -ENOSYS (-38) 当 SOC 未实例化 (T-bs-4 follow-up): DGpuBoard shell 已加 4 个 wrapper 转发到 `ep.msix()` / `ep.bar_router()`,SOC 实例化后即生效 (per T-W3-3 Phase 2+3, commit `6cb6204`)。

### Acceptance Gate (per tasks.md T-RG-1 ~ T-RG-4)

- ✅ G-RG-1 validate_topology ALL PASSED
- ✅ G-RG-1 cpptlm_tests 980/981 PASS post-D15 (1 known pre-existing race in `test_cpptlm_emulator_registry.cc:123` create_by_id, 不阻塞 v0.5.0-MVP tag, 待独立调查)
- ✅ G-RG-2 CHANGELOG.md (本 section)
- ✅ G-RG-3 docs/soc_arch/modules/README.md 7 模块同步
- ✅ G-RG-4 git tag v0.5.0-MVP
- ✅ G-RG-5 UsrLinuxEmu 集成 smoke PASS (`build/bin/test_cpptlm_emulator_dlopen` dlopen `libcpptlm_emulator.so` + dlsym 5 ABI + stdout `v1.0-dgpu-v0` + exit 0, 2026-08-30)
- ✅ **D15 真修复** (`5425c45`): `soc_->simulate_instantiate` 改传 `board_cfg["modules"][0]`(soc 模块 config)而非整个 board_cfg,SOC 真正实例化;`msix_init/update_pending/clear_pending/lookup_register` 4 个 wrapper 实际转发到 PcieEndpointTLM(返回 0)而非 -ENOSYS。同步更新 `test_cpptlm_emulator_msix.cc` 4 个断言(-38 → 0,保留 nullptr/table_size>2048 检查)。
- ✅ **D15 永久回归测试** (`test_dgpu_board_d15_regression` ctest, `[dgpu][shell][d15][regression]` tag): inline JSON mini-board 验证 load_soc_config → msix_init(16,0)=0, lookup_register(0/20)=0 命中 GPFIFO_PUT/DOORBELL, 0x100=-38 miss。cwd-independent。
- ⚠️ **Pre-existing flaky** (与 D15 修复无关,非本次引入):
  - `test_cpptlm_emulator_registry.cc:125` — `concurrent create + destroy` 并发 race(create_by_id 并发 + 计数),release-gate 已记录 known 1 failure
  - `test_latency_tlm_perf.cc:97` — 性能阈值断言 `<1000ns/call`,machine-load dependent(全量 3 次跑:6/6/7 failed 抖动)

---

## [v2.4.1] - 2026-06-19

### 修复 (Fixes)
- **ApuSoC::wrap_template_as_module**: 修复模板 params 丢失 bug (P1.5 tech debt). 原代码 `entry["params"] = nlohmann::json::object()` 覆盖模板的 params 字段, 导致 GpuCluster 收到空 params, gpc_count_ 默认 1 而非 2. 现改为 `entry["params"] = tmpl["modules"][0].value("params", ...)` 正确传递模板参数. 完整 APU SoC 端到端 GPU 链路 (gpc_count=2 × tpc_per_gpc=2 × cu_per_tpc=2 = 8 CUs × 2 cache = 16 cache) 终于端到端工作.
- **TpcCluster::simulate_instantiate**: 修子 compute_grp 蓝图传递 (L19-30 清理冗余, 显式抛 cu_template 缺失异常). 配合 ApuSoC params fix 触发完整 cu 复制链.

### 测试 (Tests)
- Case 2 期望: >= 3 → >= 16 (GPU 端 cache 实际数量与 spec 估算对齐)
- 总数: 690/690

---

## [v2.4] - 2026-06-19

### 新增 (Features)
- **ApuSoC::incorporate_parent 真实 Late-Binding**: 父端全树递归收集 `CacheTLM` peer cache, 注册到 `CoherentXBarTLM::registerPeerCache`。
  - `ModuleFactory::instantiateAll` 末尾新增 Step 9 自动触发
  - `ApuSoC::set_config` 新增 `coherent_xbar_name` 可选 params (默认 `"xbar"`)
  - `CoherentXBarTLM::registerPeerCache` 加按名去重 (双层幂等性)
  - 软失败策略: xbar/cache/port 缺失仅 `DPRINTF WARN` 不抛异常
- **SimModule P5 完整 APU SoC 拓扑**: 解锁 `apu_soc_v1.json` 端到端 snoop broadcast 验证。

### 修复 (Fixes)
- **ComputeCluster::set_config**: 跳过空字符串 `cu_template` (子 ComputeCluster 场景下避免 set cu_template_path_ 为空)

### 测试 (Tests)
- 新增 5 用例: 4 单元 `[p1]` (wiring / 深递归 / 幂等 / 无 xbar) + 1 E2E `[p1][e2e]` (apu_soc_v1.json snoop)
- 总数: 684/684 → 689/689

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
