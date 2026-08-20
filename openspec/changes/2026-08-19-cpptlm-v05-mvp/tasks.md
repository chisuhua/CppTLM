# cpptlm-v05-mvp: dGPU Board MVP Slice — Tasks (S1-S4 6-10 周)

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`specs/`](../specs/)
> **结构**: S1-S4 阶段化任务清单 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md)
> **关联路线图**: [`docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md)

---

## S1 MVP-Cut (W1-2)

### T-S1-1: git submodule add external/PTX-EMU

**Acceptance**:
- [ ] `git submodule add https://github.com/chisuhua/PTX-EMU.git external/PTX-EMU`
- [ ] `.gitmodules` 添加入口(参考 `.gitmodules:1-4` CppHDL 格式)
- [ ] submodule pin commit(per Oracle §E.1 推荐:`87820951` 或当前 HSK 兼容点)
- [ ] `git submodule update --init` 验证 submodule 内容
- [ ] `external/PTX-EMU/build/` 加入 `.gitignore`(防止构建产物入库)

**验证命令**:
```bash
git submodule status  # 显示 PTX-EMU commit hash + path
ls external/PTX-EMU/ | head -10  # 验证 submodule 已检出
```

**Commit**:
```bash
git add .gitmodules external/PTX-EMU .gitignore
git commit -m "chore(submodule): add external/PTX-EMU@<commit_hash>"
```

### T-S1-2: CMakeLists.txt 集成 PTX-EMU

**Acceptance**:
- [ ] `CMakeLists.txt` 添加 `add_subdirectory(external/PTX-EMU)`
- [ ] 设置 `PTX_EMU_BUILD_TESTS=OFF`(不构建 PTX-EMU 自家测试)
- [ ] 设置 `PTX_EMU_BUILD_SHARED=OFF`(强制静态库)
- [ ] 设置 `-fvisibility=hidden`(per Oracle §E.1 风险 R5)
- [ ] cpptlm_core 静态链接 PTX-EMU
- [ ] `tests/test_*` 添加 v0.5 MVP 测试目标
- [ ] 现有 ≥850 测试仍通过

**验证命令**:
```bash
cmake --build build -j$(nproc)
build/bin/cpptlm_tests --reporter compact
# 预期: All tests passed (≥850 assertions in ≥850 test cases)
```

**Commit**:
```bash
git add CMakeLists.txt
git commit -m "build(cmake): add_subdirectory(external/PTX-EMU) — submodule static link"
```

### T-S1-3: PtxEmuSubmoduleMVP (Adapter pattern)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/ptx_emu_submodule_mvp.hh` + `src/tlm/gpu/ptx_emu_submodule_mvp.cc`
- [ ] **关键约束**:`ptx_emu_submodule_mvp.cc` 是**唯一** include PTX-EMU 头的 .cc(编译防火墙)
- [ ] 其他 CppTLM 代码只见前向声明(`namespace ptxsim { class SMContext; class WarpContext; }`)
- [ ] 黑盒路径:`image_load` / `image_execute` / 6 其他 ABI(v3.0 兼容,走 `image_execute`)
- [ ] 白盒路径:`stepOneWarpInstruction(warp_id, out_pc, out_status, out_cycle_count) → int32`(S3 启用)
- [ ] `init(ptx_emu_root)` + `shutdown()`(RAII 模式)
- [ ] `verify_dual_path_consistency(max_warp_steps)`(per Oracle §F.4)
- [ ] `tests/test_ptx_emu_submodule_mvp.cc`:
  - `[ptx-emu-v05][mvp]` 8 ABI 单测
  - `[ptx-emu-v05][mvp][compile-firewall]` 编译防火墙验证

**验证命令**:
```bash
# 编译防火墙检查
git grep "include.*ptxsim" -- "include/tlm/gpu/*.hh" "src/tlm/gpu/*.cc"
# 预期: 仅 src/tlm/gpu/ptx_emu_submodule_mvp.cc

# 测试 PASS
ctest -R "test_ptx_emu_submodule_mvp" --output-on-failure
```

**Commit**:
```bash
git add include/tlm/gpu/ptx_emu_submodule_mvp.hh src/tlm/gpu/ptx_emu_submodule_mvp.cc tests/test_ptx_emu_submodule_mvp.cc CMakeLists.txt
git commit -m "feat(ptx-emu-mvp): adapter pattern with stepOneWarpInstruction (per-warp precision)"
```

### T-S1-4: CudaCoreAdapter (新概念,黑盒 MVP 路径)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/cuda_core_adapter_mvp.hh` + `src/tlm/gpu/cuda_core_adapter_mvp.cc`
- [ ] 持有 `PtxEmuSubmoduleMVP&`(adapter)
- [ ] `issueTask(record)` + `dispatch_blackbox(image_handle, params)`(MVP 路径)
- [ ] `on_complete(task_id, status)` + `dispatch_whitebox(warp_count, max_cycles)`(S3 路径)
- [ ] `tests/test_cuda_core_adapter_mvp.cc` 黑盒 dispatch 测试

**验证命令**:
```bash
ctest -R "test_cuda_core_adapter_mvp" --output-on-failure
# 预期: 黑盒 dispatch_blackbox + on_complete PASS
```

**Commit**:
```bash
git add include/tlm/gpu/cuda_core_adapter_mvp.hh src/tlm/gpu/cuda_core_adapter_mvp.cc tests/test_cuda_core_adapter_mvp.cc CMakeLists.txt
git commit -m "feat(cuda-core-mvp): CudaCoreAdapter with dispatch_blackbox (image_execute path)"
```

---

## S2 Real-Board-Bind (W3-4)

### T-S2-1: Doorbell + SubmissionQueue + CompletionRing

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/doorbell_mvp.hh` + `.cc`(SQ tail register + strong-order write)
- [ ] `Doorbell::ring()` 实现 `weak atomic write → strong-ordered store` (per design.md §6)
- [ ] 延迟区间断言 250-700ns(PCIe Gen5 x16)
- [ ] 新建 `include/tlm/gpu/submission_queue_mvp.hh` + `.cc`(per-stream FIFO consumer)
- [ ] 新建 `include/tlm/gpu/completion_ring_mvp.hh` + `.cc`(push + host_notify 重设计)
- [ ] `tests/test_doorbell_strong_order_mvp.cc`:latency 区间 + 同 stream 顺序 PASS
- [ ] `tests/test_submission_queue_mvp.cc` PASS
- [ ] `tests/test_completion_ring_mvp.cc` PASS

**Commit**:
```bash
git add include/tlm/gpu/doorbell_mvp.hh src/tlm/gpu/doorbell_mvp.hh ...
git commit -m "feat(doorbell-mvp): SQ tail register with strong-order write path (250-700ns)"
git commit -m "feat(submission-queue-mvp): per-stream FIFO consumer"
git commit -m "feat(completion-ring-mvp): push + host_notify hook"
```

### T-S2-2: DGpuBoardTLM (5 组件包装)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc`(~400 LOC)
- [ ] `DGpuBoardTLM : public ChStreamModuleBase`
- [ ] 内部成员持有:DGpuBar + Doorbell + SQ + CQ + CudaCoreAdapter + PtxEmuSubmoduleMVP
- [ ] 构造函数 `DGpuBoardTLM(name, EventQueue* eq, const DGpuBoardParams& params)`
- [ ] `tick()` 由 EventQueue 调度
- [ ] `install_kernel_module(vram_addr, size)` + `submit_kernel(req)` + `write_reg(offset, value)`(模拟 UsrLinuxEmu driver)
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(DGpuBoardTLM)`

**验证**:
```bash
cmake --build build -j8
ctest -R "test_dgpu_board_v1_mvp_from_config" --list-tests
```

**Commit**:
```bash
git commit -m "feat(dgpu-board-mvp): DGpuBoardTLM ChStreamModuleBase with 5 components"
```

### T-S2-3: UsrLinuxEmuIoctlStub (IOCTL 0x27/0/28/0/29)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc`(~250 LOC)
- [ ] 实现 3 IOCTL:
  - 0x27 `LOAD_KERNEL_MODULE` → `DGpuBoardTLM::install_kernel_module()`
  - 0x28 `LAUNCH_KERNEL_MODULE` → `DGpuBoardTLM::submit_kernel()`
  - 0x29 `UNLOAD_KERNEL_MODULE` → `DGpuBoardTLM::uninstall_kernel_module()`
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(UsrLinuxEmuIoctlStub)`
- [ ] `tests/test_usrlxemu_ioctl_stub.cc`:3 IOCTL PASS

**Commit**:
```bash
git commit -m "feat(usrlxemu-ioctl-stub): IOCTL 0x27/0x28/0/29 stub for Mode B dGPU board"
```

### T-S2-4: JSON config + validate_topology

**Acceptance**:
- [ ] 新建 `configs/dgpu_board_v1_mvp.json.in`(CMake configure_file 注入 `${PTX_EMU_ROOT}`)
- [ ] 1 个 `DGpuBoardTLM` 模块(`ptx_emu_root` 用 `${PTX_EMU_ROOT}` placeholder)
- [ ] 1 个 `MemoryTLM` 模块作为 H2D DMA + VRAM backing
- [ ] 1 个 `UsrLinuxEmuIoctlStub` 绑定 dgpu_board0
- [ ] 根 CMakeLists 配置 `configure_file(...)`
- [ ] 纳入 `validate_topology` CMake target 扫描
- [ ] 新建 `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION 验收)

**6 SECTION E2E 验收**(per ADR-X.17 G-MVP-2):
1. `validate_topology` 通过该 JSON
2. `instantiateAll` 返回 true,`getInstance("dgpu_board0")` 存在
3. H2D: 写 VRAM → `install_kernel_module` 返回 0 + image_handle ≠ 0
4. Launch: N=4 stream 各 1 次 IOCTL 0x28 → `eq.run(budget)` 内 CQ 收到 N 个 entry
5. host_notify 触发 ≥1 次
6. 负面: `ptx_emu_root` 指向不存在路径 → `instantiateAll` 失败

**验证**:
```bash
cmake --build build --target validate_topology
ctest -R "test_dgpu_board_v1_mvp_from_config" --output-on-failure
# 预期: 6 SECTION PASS
```

**Commit**:
```bash
git commit -m "feat(configs): dgpu_board_v1_mvp.json with validate_topology support"
git commit -m "test(dgpu-board-v1-mvp): 6 SECTION E2E + 3 IOCTL tests"
```

---

## S3 Warp-Precision (W5-6)

### T-S3-1: Pm4Decoder (Mesa-style TYPE3)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/pm4_decoder_mvp.hh` + `.cc`(~200 LOC)
- [ ] `Pm4Type3Header` 结构体(Mesa convention):
  ```cpp
  uint32_t IT : 1;        // bit 0
  uint32_t predicate : 1; // bit 1
  uint32_t opcode : 8;    // bits 2-9 (256 opcodes)
  uint32_t reserved : 6;  // bits 10-15
  uint32_t count : 14;    // bits 16-29
  uint32_t type : 2;      // bits 30-31 = 0b11
  ```
- [ ] `parse_type3(uint32_t header, const uint32_t* payload, size_t max_dwords) → Pm4Packet`
- [ ] 4 MVP opcodes:DISPATCH_DIRECT(0x15) / EVENT_WRITE(0x46) / RELEASE_MEM(0x49) / ACQUIRE_MEM(0x58)
- [ ] `tests/test_pm4_decoder_mvp.cc`:bit field round-trip + 4 opcode PASS

**Commit**:
```bash
git commit -m "feat(pm4-decoder-mvp): Mesa-style TYPE3 header parsing (4 MVP opcodes)"
```

### T-S3-2: CommandProcessor (5-state FSM)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/command_processor_mvp.hh` + `.cc`(~250 LOC)
- [ ] 5-state FSM:IDLE → FETCH → DECODE → DISPATCH → COMPLETE
- [ ] `submit_kernel(...)` API
- [ ] `tick()` 由 EventQueue 调度
- [ ] 内部调用 `Pm4Decoder::parse_type3` 解析 PM4
- [ ] 5-state FSM 状态转换正确(测试覆盖所有 transition)
- [ ] `tests/test_command_processor_mvp.cc`:5 transition 测试 PASS
- [ ] `tests/test_pm4_decoder_mvp_integration.cc`:CP + Decoder 集成 PASS

**Commit**:
```bash
git commit -m "feat(command-processor-mvp): 5-state FSM with Pm4Decoder integration"
```

### T-S3-3: TmuDispatchProcessor (TMU Glue, MVP 简化版)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc`(~250 LOC)
- [ ] `TmuDispatchRecord` 9 字段(MVP 简化,vs v0.5 21 字段)
- [ ] `PreExitPolicy` 枚举:NONE 档(MVP)
- [ ] `inflight_kernel_reqs_ map` 32 slot + LIFO eviction
- [ ] 接口契约:`submit(record)` / `on_complete(record)` / `try_chain_dependent(record)`
- [ ] 依赖锁存器 `wait_on_latch_id ↔ arrive_at_latch_id` 匹配检查
- [ ] pre-dispatch 3 段条件检查(per design.md)
- [ ] `tests/test_tmu_dispatch_processor_mvp.cc` ~10 测试 PASS

**Commit**:
```bash
git commit -m "feat(tmu-dispatch-mvp): TMU Glue with dep chain + LIFO (32 slot MVP)"
```

### T-S3-4: CudaCoreAdapter 白盒路径(S3 启用)

**Acceptance**:
- [ ] 升级 `cuda_core_adapter_mvp.cc` 加入白盒 `dispatch_whitebox`
- [ ] per-warp WarpState 跟踪(pc + cycle_count + register_deps)
- [ ] `PtxEmuSubmoduleMVP::stepOneWarpInstruction` API 接入(若 PTX-EMU 接受)
- [ ] `tests/test_cuda_core_adapter_mvp_whitebox.cc`:per-warp cycle 跟踪 PASS

**Commit**:
```bash
git commit -m "feat(cuda-core-mvp): whitebox dispatch_whitebox + per-warp WarpState"
```

---

## S4 Production (W7-10)

### T-S4-1: ScoreboardTLM 升级 production

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/scoreboard_tlm_v05_mvp.hh`(继承现有 `scoreboard_tlm.hh`)
- [ ] 新增 `WarpState { pc, cycle_count, register_deps }` 数据结构
- [ ] `tick()` 增加 per-warp tracking
- [ ] 与 `PtxEmuSubmoduleMVP::stepOneWarpInstruction` cycle_count 同步
- [ ] `tests/test_scoreboard_v05_mvp.cc` per-warp cycle 单元测试 PASS

**Commit**:
```bash
git commit -m "feat(scoreboard-v05-mvp): per-warp cycle tracking (upgrade from Legacy)"
```

### T-S4-2: PipelineTLM 升级 production

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/pipeline_tlm_v05_mvp.hh`(继承现有 `pipeline_tlm.hh`)
- [ ] 保留 A100 latency 表
- [ ] 新增 `issue(latency)` API,响应 PTX-EMU::Pipeline::step_b_set_blocked_cycles
- [ ] `tests/test_pipeline_v05_mvp.cc` latency issue 测试 PASS

**Commit**:
```bash
git commit -m "feat(pipeline-v05-mvp): latency issue API (upgrade from Legacy)"
```

### T-S4-3: 全量 baseline 验证(≥880 测试)

**Acceptance**:
- [ ] `build/bin/cpptlm_tests --reporter compact`
- [ ] 预期 ≥880 test cases PASS(v0.4.1 baseline 850 + MVP 新增 ≥50)
- [ ] assertions ≥19000
- [ ] 无 regression

**验证命令**:
```bash
build/bin/cpptlm_tests --reporter compact 2>&1 | tail -3
# 预期: All tests passed (≥19000 assertions in ≥880 test cases)
```

**Commit**:
```bash
git tag -a v0.5.0-MVP-rc1 -m "v0.5.0-MVP-rc1: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → Cuda Core → PTX-EMU warp"
git push origin v0.5.0-MVP-rc1
```

### T-S4-4: v0.5.0-MVP tag + docs

**Acceptance**:
- [ ] `CHANGELOG.md` 记录 v0.5.0-MVP release
- [ ] Tag `v0.5.0-MVP` with commit message
- [ ] `docs/soc_arch/modules/README.md` 已同步 v0.5 MVP 模块
- [ ] `docs/adr/README.md` 已更新 ADR-X.15/X.16 archived 状态
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS

**Commit**:
```bash
git add CHANGELOG.md
git commit -m "docs(changelog): record v0.5.0-MVP release (MVP slice)"
git tag -a v0.5.0-MVP -m "cpptlm-v05-mvp: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → Cuda Core → PTX-EMU warp"
```

---

## 风险登记(per ADR-X.17 §6.3)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | PTX-EMU submodule 版本漂移 | 中 | 中 | submodule pin commit + 月度 bump PR |
| R2 | PTX-EMU 维护者拒收 `stepOneWarpInstruction` API | 中 | 中 | MVP 仅黑盒,白盒推迟到 v0.5 完整版;fork 兜底 |
| R3 | UsrLinuxEmu IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R4 | CommandProcessor 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R5 | TmuDispatchProcessor LIFO 频繁驱逐 | 中 | 中 | 32 slot MVP;溢出率 >5% 触发 review |
| R6 | 6-10 周时间线偏紧 | 中 | 中 | MVP 切片(4 件)+ 严格 TDD 5 步 |
| R7 | PtxEmuSubmoduleMVP 编译防火墙破裂 | 低 | 高 | 严格 `git grep` 检查 + CI 拦截 |
| R8 | PTX-EMU submodule 构建依赖扩散 | 中 | 低 | `PTX_EMU_BUILD_TESTS=OFF` + `-fvisibility=hidden` |
| R9 | 真实 GPU 周期对齐偏差 | 已确认 | 低 | MVP 仅"内部一致性验证" |

---

## 验收检查表

最终 v0.5.0-MVP tag 前:
- [ ] T-S1-1 ~ T-S1-4 完成
- [ ] T-S2-1 ~ T-S2-4 完成
- [ ] T-S3-1 ~ T-S3-4 完成
- [ ] T-S4-1 ~ T-S4-4 完成
- [ ] 全部 ≥880 测试 PASS
- [ ] 编译防火墙验证 PASS(`git grep "include.*ptxsim"` 仅命中 `ptx_emu_submodule_mvp.cc`)
- [ ] 6 SECTION E2E 测试 PASS
- [ ] docs 同步检查 PASS(`scripts/test/docs_sync_check.sh --strict`)
- [ ] 跨仓协调:PTX-EMU submodule pin 已 bump
- [ ] `git tag -a v0.5.0-MVP -m "..."`

---

**Cc**: CppTLM Team · PTX-EMU Architecture Team · UsrLinuxEmu Architecture Team

**Refs**:
- [`proposal.md`](../proposal.md)
- [`design.md`](../design.md)
- [`../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md`](../../../docs/adr/ADR-X.17-cpptlm-v05-mvp.md)
- [`../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md`](../../../docs/soc_arch/roadmap/roadmap-mvp-to-v05.md)

---

**起草**: Sisyphus (2026-08-19)
**Owner**: CppTLM Team
**状态**: 📋 Tasks — 等 W1 S1 启动后开始实施