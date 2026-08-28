# cpptlm-dgpu-abi-export: Tasks

> **配套**: [`proposal.md`](./proposal.md) · [`design.md`](./design.md)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D5 + Status Update Q2/Q6
> **前置**: change C (board-soc-split) 已 archive（含 T-bs-6 `include/abi/cpptlm_emulator.h` 冻结 + shell 5 职责）

---

### T-ae-1: cpptlm_core PIC + cpptlm_emulator SHARED 构建

- [ ] `src/CMakeLists.txt`: `set_target_properties(cpptlm_core PROPERTIES POSITION_INDEPENDENT_CODE ON)`（仅本 change 一次，全量重编可接受）
- [ ] `src/CMakeLists.txt`: 新增 `cpptlm_emulator` SHARED target（per design §5），含 `-fvisibility=hidden` + 23 符号显式 `CPPTLM_EMULATOR_EXPORT`
- [ ] 根 `CMakeLists.txt`: install 规则补 cpptlm_emulator + `EXPORT cpptlmTargets`（per design §5）
- [ ] 全量 `cmake --build build` PASS + `ctest` 全绿（AE-G1）
- [ ] `nm -D --defined-only build/lib/libcpptlm_emulator.so | grep -c 'cpptlm_emulator_' == 19`（16 forward + 3 register；AE-G2 精确计数）
- [ ] **Commit**: `build(abi): cpptlm_emulator SHARED library + cpptlm_core PIC`

### T-ae-2: ABI 函数体实现

- [ ] 新建 `src/abi/cpptlm_emulator.cc`
- [ ] `include/abi/cpptlm_emulator.h` 补 `CPPTLM_EMULATOR_EXPORT` 宏 + 给 16 forward 函数 + 3 callback-register 函数 + 4 typedef 加宏（共 23 个 ABI 符号应用宏；typedef 不导出但宏保持 ABI 注释一致；register 函数作为 forward 函数需显式导出以满足 AE-G2 的 19 符号计数）
- [ ] 16 forward 函数体（薄壳）：每个 try/catch 全包，转发 `DGpuBoard` shell 方法；`get_version` 返回 `"v1.0-dgpu-v0"`
- [ ] 设备注册表：`namespace { std::mutex registry_mu_; std::unordered_map<uint32_t, DGpuBoard*> registry_; std::atomic<uint32_t> next_dev_id_{1}; }` + `lookup()` helper
- [ ] 销毁顺序（per design §4）：锁内 erase，解锁后 `delete board`
- [ ] **Commit**: `feat(abi): 23 ABI function bodies with exception safety + registry mutex`

### T-ae-3: 单元测试（2 文件，TDD）

- [ ] `test/test_cpptlm_emulator_abi.cc`（AE-G3）：端到端 create→mmio_write/read→pcie_config_read/write→msix_init/update/clear→backdoor_read/write→destroy；含 ABI/shell 行为等价对比
- [ ] 根 `CMakeLists.txt` 添加 `USE_TSAN` option（仅 Debug 模式有效，与 `USE_ASAN` 互斥，参考 ASan 模式实现）；CI matrix 新增 TSan × Debug 验证 AE-G4
- [ ] `test/test_cpptlm_emulator_registry.cc`（AE-G4）：2 卡并发 create_by_id + mmio_read 交错 + destroy；TSan 干净
- [ ] **Commit**: `test(abi): end-to-end ABI + multi-device registry TSan coverage`

### T-ae-4: dlopen 示例（UsrLinuxEmu 端集成模板）

- [ ] 新建 `examples/test_cpptlm_emulator_dlopen/test_dlopen.cc` + `CMakeLists.txt`
- [ ] `dlopen("libcpptlm_emulator.so")` → `dlsym` 23 ABI 中至少 `get_version` + `create_by_id` + `mmio_read/write` + `destroy` → 调通
- [ ] 示例二进制构建并执行（AE-G5）：stdout 输出 `"v1.0-dgpu-v0"` + 成功 create/destroy 退出码 0
- [ ] **Commit**: `examples(abi): dlopen usage template for UsrLinuxEmu integration`

### T-ae-5: 集成验证 + archive

- [ ] 全量 `build/bin/cpptlm_tests` PASS + `validate_topology` PASS（AE-G6）
- [ ] `./scripts/test/docs_sync_check.sh --strict` PASS（365/365 路径有效）
- [ ] `scripts/build/format.sh --check` PASS
- [ ] 在 `AGENTS.md` §CROSS-PROJECT INTEGRATION 段补一行："CppTLM 端 23 ABI 由 `libcpptlm_emulator.so` SHARED 暴露（2026-08-26 起），UsrLinuxEmu `ExternalProject_Add` / dlopen 消费"
- [ ] **Commit**: `chore(abi): integration validation + AGENTS.md cross-project note`
