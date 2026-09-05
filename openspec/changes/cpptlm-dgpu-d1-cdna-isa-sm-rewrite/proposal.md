## Why

CppTLM 当前 dGPU SoC v1.0 GPU 算力侧通过 ComputeUnitTLM 黑盒（per ADR-SOC-02）简化建模：

1. **精度对齐失败**：CDNA 校准基线（`architecture/12-cdna-calibration-baseline.md` M1-M5 microbenchmarks）需要精确的 wavefront 调度、SIMT 分歧、scoreboard waitcnt 计数，黑盒 CU 不足；
2. **PTX-EMU 真值集成阻塞**：`IPtxEmuDevice` 12 方法 + `set_instr_descriptor_buf()` 协议要求 SM 端可读寄存器值（`get_register_value`）+ 指令完成状态（`is_instruction_completed`），黑盒 CU 无内部状态可暴露；
3. **HSK-9 跨仓协调失败**：PTX-EMU 端 `attach_timing` 接口需废弃，改为 SM-owns-state 模式，CU 黑盒不持状态无法承接；
4. **业界范式背离**：完整 SM 微架构（Fetch→Decode→Issue→ScalarALU/VectorALU/MatrixCore/SIMTLane/LsuGlobal/LsuLDS/RegFileUnit/WritebackUnit/HazardTracker）是 GPU 建模标准（NV Blackwell SM_120、AMD MI300X CDNA3、gpgpu-sim、MGPUSim）；
5. **ADR-SOC-15 4 阶段路线图风险**：阶段 A 双轨并存（PipelineTLM 100% 不变 + 新增 CdnaPipelineTLM）无法在阶段 B/C 收敛到统一时序模型。

**真实工作负载需求**：MI300X CDNA 真实软件栈（ROCm / KFD / AQL）+ MFMA 矩阵指令 + s_waitcnt 计数需要 SM 微架构精度，CU 黑盒无法承载。

**ADR 背书**：ADR-SOC-16（dGPU SoC v1.0 SM 微架构重构）已发布，反转 ADR-SOC-02（CU 黑盒）+ ADR-SOC-15 §3 D2（PipelineTLM 双轨实现）。

**前置**：
- ✅ ADR-SOC-16 背书（`docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md`，commit `90c9778`）
- ✅ 设计文档 v5.0（`docs/soc_arch/architecture/15-sm-microarchitecture-design.md`，971 行，commit `650e9e9`）
- ✅ HSK-9 公告草稿（`docs/soc_arch/adr/hsk9-announcement-draft.md`，179 行）
- ✅ 实施计划（`docs/superpowers/plans/2027-02-09-sm-microarchitecture-rewrite.md`，1499 行，20 Task）
- ✅ Oracle 5 轮评审通过（Round 5 0 P0, 0 P1）
- ✅ 全部现有 Catch2 测试 `[pcie]/[axi]/[e2e]/[wave2]` 全绿
- ⚠️ **supersedes** `cpptlm-dgpu-d1-cdna-isa-phase-a`（阶段 A 双轨并存决策反转）

**禁止（Anti-patterns）**：
- ❌ 修改 `include/abi/cpptlm_emulator.h`（23 ABI 冻结）
- ❌ 修改 `include/tlm/gpu/pcie_endpoint_tlm.h` 布局（仅可加 `[[deprecated]]`）
- ❌ 修改 `external/PTX-EMU/include/ptxemu/device_api.h`（HSK-8 ACCEPTED，`IPtxEmuDevice` 12/12 frozen）
- ❌ 引入 PTX-EMU 内部头文件到 CppTLM（Clean Room 原则）
- ❌ 修改 `architecture/15` + HSK-9 已定稿内容（除非有新 Oracle 评审）

## What Changes

### 新增产物

#### SM 顶层 + 12 子模块（per architecture/15 §15.2-§15.3）

- **新增** `include/tlm/gpu/i_compute_device.hh`（15 方法接口契约）
- **新增** `include/tlm/gpu/streaming_multiprocessor_tlm.hh` + `src/tlm/gpu/streaming_multiprocessor_tlm.cc`（SM 顶层容器 + `IComputeDevice` 实现）
- **新增** `include/tlm/gpu/instruction_descriptor.hh`（`PipeClass` + `LatencyClass` + `CtrlBits` + `InstrDescriptor` POD）
- **新增** `include/tlm/gpu/sm/fetch_unit_tlm.{hh,cc}`（指令抓取）
- **新增** `include/tlm/gpu/sm/decode_unit_tlm.{hh,cc}`（指令解码）
- **新增** `include/tlm/gpu/sm/issue_unit_tlm.{hh,cc}`（指令发射）
- **新增** `include/tlm/gpu/sm/scalar_alu_tlm.{hh,cc}`（标量 ALU，5 子管道）
- **新增** `include/tlm/gpu/sm/vector_alu_tlm.{hh,cc}`（向量 ALU，V-pipe）
- **新增** `include/tlm/gpu/sm/matrix_core_tlm.{hh,cc}`（CDNA MFMA 子集，20 指令）
- **新增** `include/tlm/gpu/sm/simt_lane_tlm.{hh,cc}`（SIMT 分歧检测）
- **新增** `include/tlm/gpu/sm/lsu_global_tlm.{hh,cc}`（全局内存）
- **新增** `include/tlm/gpu/sm/lsu_lds_tlm.{hh,cc}`（共享内存）
- **新增** `include/tlm/gpu/sm/reg_file_unit_tlm.{hh,cc}`（寄存器堆，SM 真值源）
- **新增** `include/tlm/gpu/sm/writeback_unit_tlm.{hh,cc}`（写回 + 释放 Hazard）
- **新增** `include/tlm/gpu/sm/hazard_tracker_tlm.{hh,cc}`（RAW hazard + vmcnt/lgkmcnt 计数）
- **新增** `include/tlm/gpu/sm/bit_exact_gate.{hh,cc}`（双计算 Gate 验证）

#### 8 Bundle（per architecture/15 §15.4）

- **新增** `include/bundles/sm_bundles_tlm.hh`：FetchToIssueBundle / DecodeToIssueBundle / IssueToExecBundle / ExecToWritebackBundle / WritebackToRegFileBundle / MemoryReqBundle / MemoryRespBundle / ScoreboardQueryBundle

#### 测试（per architecture/15 §15.8.1-§15.8.2）

- **新增** 12 个 SM 子模块单测（`test_sm_<unit>_tlm.cc`）
- **新增** L2 Bundle 接线测试（`test_sm_bundle_wiring.cc`，20+ assertions）
- **新增** L3 SM 顶层集成测试（`test_streaming_multiprocessor_tlm.cc`，30+ assertions）
- **新增** L4 IComputeDevice 步进测试（`test_i_compute_device_stepping.cc`，25+ assertions）
- **新增** L5 CDNA waitcnt 计数测试（`test_cdna_hazard_tracker.cc`，15+ assertions）
- **新增** L6 端端测试（`test_sm_ptx_emu_e2e.cc`，20+ assertions）
- **新增** L7 JSON reload 测试（`test_json_reload_sm.cc`）
- **新增** Gate bit-exact 测试（`test_bit_exact_gate.cc`）
- **新增** `test_doc_hygiene.sh`（DOC HYGIENE 完整性）

#### 文档

- **新增** `docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md`（HSK-9 正式公告，CppTLM 仓镜像）

### 修改产物

- **修改** `include/chstream_register.hh`：注册 SM 顶层 + 12 子模块 + 8 Bundle 适配器；注销 `KernelLaunchTLM` / `GpuComputeUnitTLM` / `WavefrontTLM` / `MinimalWarpSchedulerTLM` / `VectorRegFileTLM`
- **修改** `include/tlm/gpu/gpu_soc_tlm.{hh,cc}`：移除 `KernelLaunchTLM*` forward decl + setter；改接 `IComputeDevice*`
- **修改** `include/tlm/gpu/async_completion_adapter.hh`：改接 `IComputeDevice*`
- **修改** `src/main.cpp`：移除 `kernel_launch_tlm.hh` include
- **修改** `include/tlm/gpu/wavefront_tlm.hh`：`[[deprecated]]` typedef 指向 `tlm::sm::SIMTLane`
- **修改** `include/tlm/gpu/minimal_warp_scheduler_tlm.hh`：`[[deprecated]]` typedef 指向 `tlm::sm::IssueUnitTLM`
- **修改** `include/tlm/gpu/vector_regfile_tlm.hh`：`[[deprecated]]` typedef 指向 `tlm::sm::RegFileUnit`
- **修改** `configs/vector_add_n1024.json`：`KernelLaunchTLM` → `StreamingMultiprocessorTLM`
- **修改** `configs/templates/compute_unit_v1.json`：注释更新（per Oracle Round 3 P2，无 type 字符串改动）
- **修改** `configs/templates/gpu_soc/gpu_soc_gb203_v1.json`：删除 `KernelLaunchTLM` module + 重写连接 + `GpuComputeUnitTLM` → `StreamingMultiprocessorTLM`（per Oracle Round 3 P1-d 事实修正）
- **修改** `src/CMakeLists.txt`：新增 12 个 SM 子模块 .cc；删除 6 个旧 .cc
- **修改** `AGENTS.md`：STRUCTURE + WHERE-TO-LOOK + PHASE-STATE + ADR 计数（8 → 16）
- **修改** `docs/ONBOARDING.md` §5.5：脚本表更新
- **修改** `docs/soc_arch/modules/README.md`：VIRTUAL_PATHS 条目
- **修改** `openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md`：标 superseded
- **修改** `openspec/specs/gpgpu-precision-wave2/spec.md`：标 superseded
- **修改** `openspec/specs/cli-f12b-flag/spec.md`：标 superseded
- **修改** `scripts/test/docs_sync_check.sh`：VIRTUAL_PATHS（12 + 4 条目）
- **修改** `test/python/test_f12b_smoke.py` L47：kernel_launch 重定位

### 删除产物

- **删除** `include/tlm/gpu/kernel_launch_tlm.{hh,cc}` + `src/tlm/gpu/kernel_launch_tlm.cc`
- **删除** `include/tlm/gpu/cuda_core_adapter_mvp.{hh,cc}` + `src/tlm/gpu/cuda_core_adapter_mvp.cc`
- **删除** `include/tlm/gpu/ptx_emu_submodule_mvp.{hh,cc}` + `src/tlm/gpu/ptx_emu_submodule_mvp.cc`
- **删除** `include/tlm/gpu/pipeline_tlm.{hh,cc}` + `src/tlm/gpu/pipeline_tlm.cc`
- **删除** `include/tlm/gpu/scoreboard_tlm.{hh,cc}` + `src/tlm/gpu/scoreboard_tlm.cc`
- **删除** `include/tlm/gpu/tensor_core_tlm.{hh,cc}` + `src/tlm/gpu/tensor_core_tlm.cc`
- **删除** `include/cudart/pipeline_interface.h`
- **删除** `include/cudart/scoreboard_interface.h`
- **删除** `include/cudart/tensor_core_interface.h`
- **删除** `include/cudart/AGENTS.md`
- **删除** `include/cudart/`（空目录）
- **删除** `docs/soc_arch/modules/gpu-kernel-launch.md`
- **删除** `docs/soc_arch/modules/cuda-core-adapter.md`
- **删除** `docs/soc_arch/modules/ptx-emu-submodule-mvp.md`
- **删除** `docs/soc_arch/modules/dgpu-board.md`（已合并到 SM 顶层）
- **删除** `docs/soc_arch/modules/gpu-compute_unit.md`（superseded by SM）
- **删除** `docs/soc_arch/modules/gpu-soc.md`（改接 IComputeDevice）
- **删除** `docs/soc_arch/modules/gpu.common.md`（同上）
- **删除** 15 旧测试文件（per architecture/15 §15.7.1.B）

## Capabilities

### New Capabilities

- `sm-microarchitecture`：SM 微架构完整重构（12 子模块 + 8 Bundle + IComputeDevice 15 方法 + bit-exact Gate + SM-owns-state 模式）

### Modified Capabilities

- `cpptlm-d1-p1-pipeline-scoreboard`（MODIFIED）：阶段 A 双轨决策失效；PipelineTLM / ScoreboardTLM 全部删除，需求由 SM 微架构 HazardTracker + ScalarALU 子管道承载
- `gpgpu-precision-wave2`（MODIFIED）：WavefrontTLM / MinimalWarpSchedulerTLM / VectorRegFileTLM 全部内化到 SM 子模块
- `cli-f12b-flag`（MODIFIED）：kernel_launch 重定位到 StreamingMultiprocessorTLM
- `cdna-isa-abstraction`（SUPERSEDED）：阶段 A 双轨改为 SM 完整实现

## Impact

**影响代码**：
- 新增 16 .hh + 16 .cc（SM 顶层 + 12 子模块 + 1 bit_exact_gate + 1 streaming_multiprocessor_tlm + 1 instruction_descriptor）
- 新增 1 .hh（8 Bundle 头文件）
- 新增 21 测试 .cc + 1 脚本
- 删除 6 实现 + 3 vendor + 1 cudart 目录
- 删除 15 测试文件
- 删除 7 modules docs
- 修改 5 头文件（旁路）+ 1 chstream_register + 1 CMakeLists.txt + 3 JSON + 8 文档

**影响测试**：
- 现有 `[pcie]/[axi]/[e2e]/[wave2]` 必须保持 100% 通过
- 新增 `[icompute]/[sm-unit]/[sm-bundle]/[sm-top]/[sm-icompute]/[sm-cdna]/[sm-e2e]/[sm-json]/[sm-gate]` 标签测试

**影响依赖**：
- 0 新增外部依赖
- 不修改 23 ABI 头文件
- 不引入 PTX-EMU 内部头文件
- 不修改 `architecture/15` + HSK-9 内容

**影响文档**：
- `docs/soc_arch/architecture/15-sm-microarchitecture-design.md`（已发布）
- `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md`（已发布）
- `docs/soc_arch/adr/ADR-SOC-02/15 Status Update`（已修订）
- `docs/soc_arch/adr/hsk9-announcement-draft.md`（已发布）
- `openspec/specs/cdna-isa-abstraction/spec.md`（archive 时合并）
- `openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md`（标 superseded）
- `openspec/specs/gpgpu-precision-wave2/spec.md`（标 superseded）
- `openspec/specs/cli-f12b-flag/spec.md`（标 superseded）
- `openspec/specs/sm-microarchitecture/spec.md`（archive 时新建）

**工作总量**：25-30 工作日（CppTLM 侧）+ 5-10 工作日（PTX-EMU 侧）；20 个原子 commit（每 commit 可独立编译）。