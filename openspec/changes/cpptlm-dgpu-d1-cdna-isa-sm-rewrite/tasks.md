# Tasks: SM Microarchitecture Rewrite

> **Status**: Proposed
> **Date**: 2027-02-09
> **Parent change**: `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`
> **References**: `docs/superpowers/plans/2027-02-09-sm-microarchitecture-rewrite.md` (1499 行, 20 Task 详细步骤)

---

## Task 列表

本 change 由 20 个原子 commit 组成（per architecture/15 §15.9.1 + `docs/superpowers/plans/`），每 commit 可独立编译。本文件为 OpenSpec tasks.md 高层摘要，详细 step-by-step 见 plan。

### 阶段 1: 文档 + OpenSpec（Task 1-3）

- [ ] **Task 1**: ADR 背书 SM 重构 + 修订 ADR-SOC-02/15 Status Update（✅ commit `90c9778`）
- [ ] **Task 2**: 验证设计文档 v5.0 + HSK-9 草稿已在 main（commit `650e9e9`）✅
- [ ] **Task 3**: 启动 OpenSpec change `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`（supersedes phase-a）✅ 当前任务

### 阶段 2: 接口 + 12 子模块 stub（Task 4-5）

- [ ] **Task 4**: 新增 SM 顶层容器 `StreamingMultiprocessorTLM` + `IComputeDevice` 接口（stub 实现）
  - 文件: `include/tlm/gpu/i_compute_device.hh` + `streaming_multiprocessor_tlm.{hh,cc}` + `instruction_descriptor.hh`
  - TDD: 失败测试 → 接口 stub → SM 顶层 stub → PASS
- [ ] **Task 5**: 新增 12 个 ChStream 子模块 stub（.hh + 空 .cc）
  - 文件: `include/tlm/gpu/sm/{fetch,decode,issue,scalar_alu,vector_alu,matrix_core,simt_lane,lsu_global,lsu_lds,reg_file_unit,writeback_unit,hazard_tracker}_tlm.{hh,cc}` × 12
  - TDD: 失败测试 → 12 stub → PASS

### 阶段 3: Bundle + 子模块 full + 注册（Task 6-8）

- [ ] **Task 6**: 新增 `sm_bundles_tlm.hh` 8 种 Bundle 定义
  - TDD: 失败测试 → 8 Bundle POD → PASS
- [ ] **Task 7**: 完整实现 12 个 ChStream 子模块（连接 Bundle）
  - TDD: 12 子模块行为测试 → 实现 → PASS
- [ ] **Task 8**: `chstream_register.hh` 注册 SM 顶层 + 12 子模块 + 8 Bundle 适配器
  - 验证: `cmake --build build` PASS

### 阶段 4: 旧模块重构（Task 9-11）

- [ ] **Task 9**: `GpuComputeUnitTLM` 重构为 SM 内部状态机
  - SM 顶层持有 12 子模块 unique_ptr + IComputeDevice 14 方法委托
- [ ] **Task 10**: 重构 `WavefrontTLM` / `MinimalWarpSchedulerTLM` / `VectorRegFileTLM` 为 SM 内部子模块
  - `[[deprecated]]` typedef 指向 `tlm::sm::SIMTLane` / `IssueUnitTLM` / `RegFileUnit`
  - chstream_register 注销旧名
- [ ] **Task 11**: 修复旁路依赖（`gpu_soc_tlm.{h,cc}` + `async_completion_adapter` + `main.cpp` + `chstream_register`）
  - 改接 `IComputeDevice*`
  - TDD: 失败测试验证 KernelLaunchTLM 符号无残留

### 阶段 5: 删除（Task 12-13）

- [ ] **Task 12**: 删除 `KernelLaunchTLM` + `CudaCoreAdapterMVP` + `PtxEmuSubmoduleMVP`
- [ ] **Task 13**: 删除 `PipelineTLM` + `ScoreboardTLM` + `TensorCoreTLM` + 3 vendor 接口 + `include/cudart/` 目录

### 阶段 6: JSON + 删除测试 + DOC（Task 14-16）

- [ ] **Task 14**: 修订 4 个 JSON config（vector_add_n1024 + compute_unit_v1 注释 + gpu_soc_gb203_v1 删除 + 重写 + dgpu_soc_with_pcie_ip 验证）
- [ ] **Task 15**: DOC HYGIENE 全套（AGENTS.md + ONBOARDING + 7 modules docs + 3 main specs + VIRTUAL_PATHS + test scripts）
- [ ] **Task 16**: 删除 15 旧测试文件（per architecture/15 §15.7.1.B）

### 阶段 7: 新测试 + 完整实现（Task 17-18）

- [ ] **Task 17**: 新增 12 SM 子模块单测 + L2-L6 集成测试 + L7 JSON reload + `test_f12b_smoke` 重定位（146+ assertions）
- [ ] **Task 18**: 完整实现 IComputeDevice 14 方法与同步协议 + bit-exact Gate + 12 子模块 full + 全量回归（Gate 14 项）

### 阶段 8: Archive + 跨仓协调（Task 19-20）

- [ ] **Task 19**: Archive OpenSpec change + 同步 main specs（`cdna-isa-abstraction` → `sm-microarchitecture`）
- [ ] **Task 20**: HSK-9 公告正式发布 + PTX-EMU 端 `sm_context.cpp` 改造跟踪（attach_timing deprecated stub + 14 天反馈窗口）

---

## 工作量与依赖

- **总工作量**: 25-30 工作日（CppTLM 侧）+ 5-10 工作日（PTX-EMU 侧）
- **commit 数量**: 20 原子 commit，每 commit 可独立编译
- **Gate 14 项**: per architecture/15 §15.10
- **OpenSpec sync**: archive 时合并 `cdna-isa-abstraction` capability spec

## 验证

每 Task 完成后 `cmake --build build` + `ctest` + 验证测试套件。Task 18 Step 6 全量回归验证 Gate 14 项。

详细 step-by-step 见 `docs/superpowers/plans/2027-02-09-sm-microarchitecture-rewrite.md`。