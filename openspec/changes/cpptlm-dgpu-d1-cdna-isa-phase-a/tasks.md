# Tasks: cpptlm-dgpu-d1-cdna-isa-phase-a

> **Status**: Proposed（阶段 A 启动，2027-02-09）
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

## 1. InstrDescriptor POD 头文件（1-2 工作日）

- [ ] 1.1 在 `include/tlm/gpu/instruction_descriptor.hh` 定义 `PipeClass` enum（7 个值）
- [ ] 1.2 在同文件定义 `LatencyClass` enum（6 个值）
- [ ] 1.3 在同文件定义 `CtrlBits` struct（6 字段，1 byte 总）
- [ ] 1.4 在同文件定义 `InstrDescriptor` POD struct（≤64 bytes，含 `dst_regs[4]`/`src_regs[4]`/`is_memory`/`target_vaddr`/`mem_size`）
- [ ] 1.5 提供 `std::hash<InstrDescriptor>` 特化（用于 unordered_set）
- [ ] 1.6 添加 `static_assert(std::is_trivially_copyable_v<InstrDescriptor>)` 编译期保障

## 2. CdnaPipelineTLM（2-3 工作日）

- [ ] 2.1 在 `include/tlm/gpu/cdna_pipeline_tlm.hh` 定义 `class CdnaPipelineTLM : public IPipelineLatencyProvider`
- [ ] 2.2 添加 `enum class Mode { kPtxCompat, kCdnaStrict }` 构造函数参数
- [ ] 2.3 实现 `double get_latency(LatencyClass lc) const`（6 入口查表）
- [ ] 2.4 实现 `double get_fractional_cycles(string, PipeClass)` PTX 兼容 shim（6 个 has() 子串映射到 LatencyClass 后查表）
- [ ] 2.5 添加单元测试 `test/test_cdna_pipeline_tlm.cc`：6 个 LatencyClass 查表精度 + PTX 字符串 → 数值映射
- [ ] 2.6 `Mode::kCdnaStrict` 模式下，`get_fractional_cycles` 抛 `std::logic_error`（CDNA 不接受 PTX 字符串）

## 3. IHazardTracker + ScoreboardTLMv2（2-3 工作日）

- [ ] 3.1 在 `include/tlm/gpu/hazard_tracker_interface.hh` 定义 `class IHazardTracker` 抽象接口（5 个虚方法：tick/is_stalled/notify_instruction_issued/notify_instruction_completed/reset）
- [ ] 3.2 在 `include/tlm/gpu/scoreboard_tlm_v2.hh` 定义 `class ScoreboardTLMv2 : public IHazardTracker`
- [ ] 3.3 添加 `enum class Mode { kVirtualReg, kHardwareCounter }` 构造函数参数
- [ ] 3.4 `Mode::kVirtualReg` 路径：内部 `ScoreboardTLM sb_` 成员，`notify_instruction_issued/completed` 委托给 `sb_` + 转换 `InstrDescriptor.dst_regs` 到 `reg_id`
- [ ] 3.5 `Mode::kHardwareCounter` 路径：定义 `vmcnt_/lgkmcnt_/expcnt_` 三维数组 `[sm_id][wave_id]`；`notify_instruction_issued` 增 `ctrl.*_req`；`notify_instruction_completed` 减；`is_stalled` 检查 `ctrl.wait_*cnt` 与当前计数器
- [ ] 3.6 `Mode::kHardwareCounter` 构造函数加 `[[deprecated("stage C only")]]` 警告
- [ ] 3.7 添加单元测试 `test/test_scoreboard_tlm_v2.cc`：双模式单元测试（kVirtualReg 行为同 ScoreboardTLM + kHardwareCounter 计数器增减）

## 4. KernelLaunchTLM 双轨接入（1 工作日）

- [ ] 4.1 在 `include/tlm/gpu/kernel_launch_tlm.hh` **新增** setter：`set_scoreboard_v2(IHazardTracker*)` 与 `set_pipeline_v2(CdnaPipelineTLM*)`
- [ ] 4.2 **保留** 旧 setter：`set_scoreboard(IScoreboard*)` 与 `set_pipeline(IPipelineLatencyProvider*)`
- [ ] 4.3 修改 `src/tlm/gpu/kernel_launch_tlm.cc:tick()` 三路径调度逻辑（详见 spec.md Requirement: cpptlm-kernel-launch-v2-setters）
- [ ] 4.4 路径 2/3 添加 unit test：`test_kernel_launch_v2_paths.cc`（混合注入合法性）

## 5. 回归对比 + 门禁验证（1-2 工作日）

- [ ] 5.1 在 `test/test_pipeline_parity.cc` 实现：生成 1000 条 PTX 指令序列，断言 `CdnaPipelineTLM(Mode::kPtxCompat)` 与 `PipelineTLM` 输出 **bit-identical**
- [ ] 5.2 在 `test/test_instruction_descriptor.cc` 实现：`std::bit_cast<uint64_t>` 序列化 + 反序列化跨字段 round-trip
- [ ] 5.3 全量回归：`./build/bin/cpptlm_tests "[pcie]"` 全部 PASS（基线 ~15000 assertions）
- [ ] 5.4 全量回归：`./build/bin/cpptlm_tests "[axi]"` 全部 PASS（基线 ~500 assertions）
- [ ] 5.5 全量回归：`./build/bin/cpptlm_tests "[e2e]"` 全部 PASS
- [ ] 5.6 全量回归：`./build/bin/cpptlm_tests "[wave2]"` 全部 PASS
- [ ] 5.7 全量回归：`./build/bin/cpptlm_tests "[gpu]"` 全部 PASS
- [ ] 5.8 新增回归：`./build/bin/cpptlm_tests "[cdna-phase-a]"` 全部 PASS（4 个新测试文件）

## 6. 文档同步 + OpenSpec 收尾（0.5 工作日）

- [ ] 6.1 更新 `include/tlm/gpu/AGENTS.md`（如存在）记录 `instruction_descriptor.hh` + `cdna_pipeline_tlm.hh` + `hazard_tracker_interface.hh` + `scoreboard_tlm_v2.hh` 4 个新头文件
- [ ] 6.2 更新 `include/tlm/gpu/kernel_launch_tlm.hh` 注释：标注"3 路径调度逻辑"指向 spec.md
- [ ] 6.3 验证 `openspec validate cpptlm-dgpu-d1-cdna-isa-phase-a --strict` PASS
- [ ] 6.4 `git log -1 --stat` 确认 commit 改动文件数与 proposal.md "Impact" 节一致

## 7. Gate A 验收（per ADR-SOC-15 §4.1）

- [ ] 7.1 Oracle 评审（应用 `oracle` subagent）确认 0 P0 + ≤2 P1 风险
- [ ] 7.2 `openspec validate` 10/10 PASS
- [ ] 7.3 `test_pipeline_parity.cc` PTX bit-identical 1000/1000
- [ ] 7.4 全部 `[pcie]/[axi]/[e2e]/[wave2]/[gpu]` 测试保持基线 100% 通过
- [ ] 7.5 23 ABI 头文件 `git diff` 确认零修改

## 阶段 A Gate 后解锁

✅ **Gate A 通过后启动 HSK-9 协调**：进入 §B 阶段（IMemoryPort 引入），按 `docs/soc_arch/adr/ADR-SOC-15-cdna-real-isa-roadmap.md` §3 R3 协调公告执行
✅ **OpenSpec archive**：`openspec archive cpptlm-dgpu-d1-cdna-isa-phase-a --reason "Stage A complete"` 自动合并 spec 到 `openspec/specs/cdna-isa-abstraction/spec.md`

---

## Deferred Tasks Summary

无（阶段 A 范围内任务 100% 完成即 Gate 通过；不向后续阶段转交任务）