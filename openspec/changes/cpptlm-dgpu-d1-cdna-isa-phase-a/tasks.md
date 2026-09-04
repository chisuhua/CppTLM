# Tasks: cpptlm-dgpu-d1-cdna-isa-phase-a

> **Status**: Proposed（阶段 A 启动，2027-02-09; post-Oracle-revision 2027-02-09）
> **Pre-implementation audit**: Oracle `ses_f962d8ef5ffe2tuGZUCI4gfY0T` (5 P0 + 5 P1 + 3 P2 closed)
> **Parent change**: [`proposal.md`](./proposal.md) + [`design.md`](./design.md) + [`specs/cdna-isa-abstraction/spec.md`](./specs/cdna-isa-abstraction/spec.md)
> **Parent ADR**: [`ADR-SOC-15-cdna-real-isa-roadmap.md`](../../../docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md)
> **Parent design**: [`docs/soc_arch/architecture/11-cdna-real-isa-integration.md`](../../../docs/soc_arch/architecture/11-cdna-real-isa-integration.md)
> **前置**: 0（PTX-EMU 端无任何变更；本 change 仅改造 CppTLM 端）
> **总工时**: ~5-8 工作日（per ADR-SOC-15 §4.1）

## 启动条件

- [x] ADR-SOC-15 已发布（commit `3c76398`）
- [x] 方案设计文档 `architecture/11-...` 已发布（commit `5a9eb4c`）
- [x] 现有 `[pcie]/[axi]/[e2e]/[wave2]` 全绿（阶段 A 基线）
- [x] 23 ABI 冻结不变量生效（`include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h` 不修改）
- [x] Oracle 预审通过（5 P0 + 5 P1 + 3 P2 全部 closed；spec 已重写为基于实际 `pipeline_tlm.cc:21-137` 数值）

## 1. InstrDescriptor POD 头文件（1-2 工作日）

- [ ] 1.1 在 `include/tlm/gpu/instruction_descriptor.hh` 定义 `PipeClass` enum（7 个值）
- [ ] 1.2 在同文件定义 `LatencyClass` enum（6 个值）
- [ ] 1.3 在同文件定义 `CtrlBits` struct（**6 字段，每字段 1 字节 = 6 bytes 总，**per arch 11 §11.2.1 默认值 0xFF 语义，**NOT 1 byte**）
- [ ] 1.4 在同文件定义 `InstrDescriptor` POD struct（含 `dst_regs[4]`/`src_regs[4]`/`is_memory`/`target_vaddr`/`mem_size`/`transaction_id:uint64_t=0`/`reserved[8]:uint8_t`），加 **`static_assert(sizeof(InstrDescriptor) == 64, "size drift from arch 11 §11.2.1")`**（per Oracle 二轮算术验证：字段表精确 60 bytes + 末尾 4 bytes 隐式 padding 到 8-byte 对齐边界 = 64 bytes；不强制 packed，用具名 reserved 填平中间 padding hole）
- [ ] 1.5 提供 `std::hash<InstrDescriptor>` 特化（用于 unordered_set），加 **P2 退化测试**（10k random descriptors collision rate < 30%, load_factor < 0.7）
- [ ] 1.6 添加 `static_assert(std::is_trivially_copyable_v<InstrDescriptor>)` 编译期保障
- [ ] 1.7 文档化 sizeof 精确统计到 `instruction_descriptor.hh` 头注释（per spec §cpptlm-instruction-descriptor）

## 2. CdnaPipelineTLM（2-3 工作日）

- [ ] 2.1 在 `include/tlm/gpu/cdna_pipeline_tlm.hh` 定义 `class CdnaPipelineTLM : public IPipelineLatencyProvider`
- [ ] 2.2 添加 `enum class Mode { kPtxCompat, kCdnaStrict }` 构造函数参数
- [ ] 2.3 实现 `double get_latency(LatencyClass lc) const`（6 入口查表，kMemoryVariable 返回 -1.0 哨兵）
- [ ] 2.4 实现 `double get_fractional_cycles(const std::string&, PipelineId) override` —— **必须逐字复制 `pipeline_tlm.cc:21-94` 四个匿名命名空间函数（`has()` + `latency_p0/p1/p2/p3`）完整逻辑与分支顺序，不经过 LatencyClass 枚举表**（per Oracle P0-A1）
- [ ] 2.5 实现 `double get_fractional_cycles_by_type(int, PipelineId) override` —— **per `pipeline_tlm.cc:120-137` 6 管线 switch 真值表 (P0=2, V=1, P1=8, P2=8, P3=200, P4=0)**（per Oracle P0-A2）
- [ ] 2.6 `Mode::kCdnaStrict` 模式下，`get_fractional_cycles` 抛 `std::logic_error`（CDNA 不接受 PTX 字符串）
- [ ] 2.7 添加单元测试 `test/test_cdna_pipeline_tlm.cc`：6 个 LatencyClass 查表 + 6 PipelineId × 全模式矩阵 parity + `get_fractional_cycles_by_type` 全 6 管线 parity

## 3. IHazardTracker + ScoreboardTLMv2（2-3 工作日）

- [ ] 3.1 在 `include/tlm/gpu/hazard_tracker_interface.hh` 定义 `class IHazardTracker` 抽象接口（**与 vendored `IScoreboard` (commit `8acfd2d1`) 语义对齐**：5 个方法 `has_free_entry/try_acquire(instr,sm,wave)→bool/release(instr,sm,wave)/tick` + 1 个 P2 阶段 C 扩展点 `mark_waiting(sm,wave,wait_vmcnt,wait_lgkmcnt,wait_expcnt) { /* default no-op */ }`）
- [ ] 3.2 在 `include/tlm/gpu/scoreboard_tlm_v2.hh` 定义 `class ScoreboardTLMv2 : public IHazardTracker`
- [ ] 3.3 添加 `enum class Mode { kVirtualReg, kHardwareCounter }` 构造函数参数
- [ ] 3.4 `Mode::kVirtualReg` 路径：内部组合 `ScoreboardTLM sb_` 成员 + 影子 per-warp outstanding 集合；`try_acquire` 委托给 `sb_.allocate(reg_id, warp_id)`，从 `instr.dst_regs[0..num_dst]` 提取 reg_id
- [ ] 3.5 `Mode::kHardwareCounter` 路径：定义 `vmcnt_[64][64]/lgkmcnt_[64][64]/expcnt_[64][64]` 三维数组（MAX_SM_ID=64, MAX_WAVE_PER_SM=64 per `apu_soc_v1.json`）；`try_acquire` 按 `instr.ctrl.*_req` **increment** 对应 counter（CDNA 真实语义：issue 增, completed 减）；`release` decrement；sm_id ≥64 抛 `std::out_of_range`
- [ ] 3.6 `Mode::kHardwareCounter` **枚举值**加 `[[deprecated("stage C only")]]` 警告（**C++17 enumerator deprecation, NOT 构造函数——deprecate 构造函数会连累 kVirtualReg**）
- [ ] 3.7 添加单元测试 `test/test_scoreboard_tlm_v2.cc`：kVirtualReg 行为对齐 ScoreboardTLM（duplicate-allocate 返回 false）+ kHardwareCounter 计数器增减 + 越界抛 out_of_range

## 4. CudaCoreAdapterMVP 双轨注入（1 工作日，per Oracle P0-A3）

- [ ] 4.1 在 `include/tlm/gpu/cuda_core_adapter_mvp.hh` **新增** `set_scoreboard_v2(IHazardTracker*)` 与 `set_pipeline_v2(CdnaPipelineTLM*)` setter
- [ ] 4.2 **不修改**原有 `inject_timing_modules()` 函数主体（保留 `make_unique<ScoreboardTLM/PipelineTLM>` 默认路径）
- [ ] 4.3 修改 `src/tlm/gpu/cuda_core_adapter_mvp.cc:inject_timing_modules()`：当 `scoreboard_v2_/pipeline_v2_` 非 nullptr 时使用 v2 实例，否则回退到 `make_unique<>`（3 路径：默认 / v2 / v2+kHardwareCounter）
- [ ] 4.4 添加单元测试 `test_cuda_core_adapter_v2_paths.cc`：默认路径不变 / v2 注入 / v2 nullptr 回退 / 混合注入（仅 v2 一个 setter 调用）

> **重要修订 (Oracle P0-A3)**：本 task group **不修改** `kernel_launch_tlm.hh`。原 spec 假设在 `KernelLaunchTLM` 注入 v2 是基于错误代码假设——`KernelLaunchTLM` 当前**无** `set_scoreboard/set_pipeline` setter 且 `tick()` 不消费 scoreboard/pipeline。真正的 scoreboard_/pipeline_ 持有者是 `CudaCoreAdapterMVP`（per `src/tlm/gpu/cuda_core_adapter_mvp.cc:60-61`）。

## 5. PtxStringToDescriptor 转换器 (P1-B7 闭环验证链, 0.5 工作日)

- [ ] 5.1 在 `include/tlm/gpu/ptx_string_to_descriptor.hh` + `src/tlm/gpu/ptx_string_to_descriptor.cc` 实现纯函数 `InstrDescriptor ptx_string_to_descriptor(const std::string& ptx_instr, uint32_t sm_id, uint32_t wave_id)` —— 不引入 PTX-EMU 内部头（**Clean Room**），仅消费 vendored `IScoreboard`/`IPipelineLatencyProvider`/`ITensorCoreTiming` 公共头
- [ ] 5.2 转换规则：解析 `ptx_instr` 子串（`fma`→kVectorALU+kFixed16Cycles, `ld.global`→kLSU_Global+kMemoryVariable+is_memory=true+mem_size=4 等），提取 `target_vaddr`（若有 `[]` 立即数）
- [ ] 5.3 单元测试 `test_ptx_string_to_descriptor.cc`：覆盖 6 个 PTX pattern × 2 method，与 `PipelineTLM::get_fractional_cycles` 真值对照
- [ ] 5.4 接入点：阶段 A `parity` 测试用此函数构造 descriptor（**不**实际用于仿真，仅单测 + parity）

> **重要 (Oracle P1-4)**：本 task 闭环了"v2 链路无生产者"问题——阶段 A 端到端验证有了 descriptor 构造入口。`KernelLaunchTLM::tick()` 真实调用 `facade_->exe_once()` 仍是阶段 B ADR-SOC-15 B.4 待办（per `kernel_launch_tlm.hh:13`）。

## 6. 回归对比 + 门禁验证（1-2 工作日）

- [ ] 6.1 在 `test/test_pipeline_parity.cc` 实现：**枚举式分支覆盖 + 1000 条随机（固定 seed）双层**：
  - 硬编码覆盖 latency_p0/p1/p2/p3 每个 return 分支 ≥2 用例（fma/mul.f32/mad/空串/bar/sin/cos/rcp/sqrt/lg.global/st.global/atom/red./ld.shared/ld.local 等）
  - 6 PipelineId × 全模式矩阵
  - `get_fractional_cycles_by_type` 全 6 管线矩阵
  - 边界（空串/无匹配/大小写变体）
  - 1000 条固定 seed 随机 PTX 指令序列
- [ ] 6.2 在 `test/test_instruction_descriptor.cc` 实现：`std::bit_cast<uint64_t>` 序列化 + 反序列化跨字段 round-trip + 10k random hash collision 验证
- [ ] 6.3 全量回归：`./build/bin/cpptlm_tests "[pcie]"` 全部 PASS（基线 15137 assertions）
- [ ] 6.4 全量回归：`./build/bin/cpptlm_tests "[axi]"` 全部 PASS（基线 561 assertions）
- [ ] 6.5 全量回归：`./build/bin/cpptlm_tests "[e2e]"` 全部 PASS（基线 657 assertions）
- [ ] 6.6 全量回归：`./build/bin/cpptlm_tests "[wave2]"` 全部 PASS（保持 6 个 mock-only 用例 PASS, per `test_kernel_launch_ptx_integration.cc`；不计入真实覆盖率）
- [ ] 6.7 全量回归：`./build/bin/cpptlm_tests "[gpu]"` 全部 PASS
- [ ] 6.8 新增回归：`./build/bin/cpptlm_tests "[cdna-phase-a]"` 全部 PASS（5 个新测试文件：test_instruction_descriptor / test_cdna_pipeline_tlm / test_scoreboard_tlm_v2 / test_pipeline_parity / test_cuda_core_adapter_v2_paths / test_ptx_string_to_descriptor）
- [ ] 6.9 **新增**：跑现有 `[wave2]/PTX 模式 E2E`，记录改造前后 `kernels_launched`/cycle 计数做 diff（per Oracle P1-5 端到端 parity）

## 7. 文档同步 + OpenSpec 收尾（0.5 工作日）

- [ ] 7.1 **新增** `include/tlm/gpu/AGENTS.md`（原不存在，per Oracle P2-3 nit）记录 `instruction_descriptor.hh` + `cdna_pipeline_tlm.hh` + `hazard_tracker_interface.hh` + `scoreboard_tlm_v2.hh` + `ptx_string_to_descriptor.hh` 5 个新头文件
- [ ] 7.2 更新 `include/tlm/gpu/kernel_launch_tlm.hh` 注释不变（**本 change 不修改 KernelLaunchTLM**）
- [ ] 7.3 更新 `src/tlm/gpu/cuda_core_adapter_mvp.hh` 注释：标注"3 路径调度逻辑"指向 spec.md cpptlm-cuda-core-adapter-v2-injection
- [ ] 7.4 验证 `openspec validate cpptlm-dgpu-d1-cdna-isa-phase-a --strict` PASS（**post-Oracle-revision 版本**）
- [ ] 7.5 `git log -1 --stat` 确认 commit 改动文件数与 proposal.md "Impact" 节一致
- [ ] 7.6 **新增（per Oracle P1-2 / E1）**：提交 `[CDNA-Phase-C] Cleanup-Trigger` issue，列阶段 A 引入的 4 个 Mode 枚举（kPtxCompat/kCdnaStrict/kVirtualReg/kHardwareCounter）+ 2 个双轨类（CdnaPipelineTLM/ScoreboardTLMv2）的废弃 owner (assignee + target commit)；阶段 A Gate 不通过

## 8. Gate A 验收（per ADR-SOC-15 §4.1）

- [ ] 8.1 Oracle 评审（应用 `oracle` subagent）确认 0 P0 + ≤2 P1 风险（**二轮评审，独立 session 续用 `ses_f962d8ef5ffe2tuGZUCI4gfY0T`**）
- [ ] 8.2 `openspec validate` PASS
- [ ] 8.3 `test_pipeline_parity.cc` PTX 字节级 bit-identical 全矩阵 PASS（6 PipelineId × 全模式 × 2 method）
- [ ] 8.4 全部 `[pcie]/[axi]/[e2e]/[wave2]/[gpu]` 测试保持基线 100% 通过
- [ ] 8.5 23 ABI 头文件 `git diff` 确认零修改
- [ ] 8.6 `instruction_descriptor.hh` `static_assert(sizeof == 64)` 通过
- [ ] 8.7 端到端 PTX 模式 `kernels_launched`/cycle 计数 diff = 0

## 阶段 A Gate 后解锁

✅ **Gate A 通过后启动 HSK-9 协调**：进入 §B 阶段（IMemoryPort 引入），按 `docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md` §3 R3 协调公告执行
✅ **OpenSpec archive**：`openspec archive cpptlm-dgpu-d1-cdna-isa-phase-a --reason "Stage A complete"` 自动合并 spec 到 `openspec/specs/cdna-isa-abstraction/spec.md`

---

## Deferred Tasks Summary

无（阶段 A 范围内任务 100% 完成即 Gate 通过；不向后续阶段转交任务，但 [CDNA-Phase-C] Cleanup-Trigger issue 强制追踪阶段 C 收口责任）
