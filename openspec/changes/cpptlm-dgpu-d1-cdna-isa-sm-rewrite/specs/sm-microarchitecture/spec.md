# Spec: SM Microarchitecture

> **Capability**: `sm-microarchitecture`
> **Status**: Proposed
> **Created**: 2027-02-09
> **References**: `docs/soc_arch/architecture/15-sm-microarchitecture-design.md` (v5.0) · `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md`

---

## ADDED Requirements

### Requirement: SM 拓扑与 12 子模块

The system SHALL SM 顶层容器 (`StreamingMultiprocessorTLM`) SHALL持有 12 个 ChStream 子模块，每个子模块继承 `ChStreamModuleBase`，通过 8 种 Bundle 连接成完整 SM 微架构：

- `FetchUnitTLM`：指令抓取（per-warp PC + instr buf）
- `DecodeUnitTLM`：指令解码 → `InstrDescriptor`（`PipeClass` + `LatencyClass` + `CtrlBits`）
- `IssueUnitTLM`：指令发射（round-robin + CGGTY 5-warp 阈值）
- `ScalarALU`：标量 ALU（INT/FP32/FP64/SFU/Branch 5 子管道；SFU 子管道 per architecture/15 §15.6.5）
- `VectorALU`：向量 ALU（V-pipe SIMD）
- `MatrixCore`：CDNA MFMA 子集（20 指令 per architecture/13）
- `SIMTLane`：64-bit EXEC mask + 派态分歧检测
- `LsuGlobal`：全局内存访问（接 `IMemoryPort` 异步）
- `LsuLDS`：intra-SM 共享内存访问（bank conflict 检测）
- `RegFileUnit`：寄存器堆（**SM 端唯一真值源**）
- `WritebackUnit`：写回 `RegFileUnit` + 释放 `HazardTracker`
- `HazardTracker`：RAW hazard（`kVirtualReg` PTX 兼容）+ vmcnt/lgkmcnt 计数（`kHardwareCounter` CDNA stage C 预埋）

#### Scenario: 12 子模块注册为 ChStreamModuleBase

- **WHEN** 任何 `ChStreamModuleBase` 派生类被实例化
- **THEN** `get_module_type()` SHALL返回对应字符串（`"FetchUnitTLM"` / `"DecodeUnitTLM"` / `"IssueUnitTLM"` / `"ScalarALU"` / `"VectorALU"` / `"MatrixCore"` / `"SIMTLane"` / `"LsuGlobal"` / `"LsuLDS"` / `"RegFileUnit"` / `"WritebackUnit"` / `"HazardTracker"`）

### Requirement: 8 Bundle POD 定义

The system SHALL `include/bundles/sm_bundles_tlm.hh` SHALL定义 8 种 Bundle 类型：

- `FetchToIssueBundle`：Fetch → Issue（`instr_desc`, `warp_id`, `pc`）
- `DecodeToIssueBundle`：Decode → Issue（+ `PipeClass`, `LatencyClass`）
- `IssueToExecBundle`：Issue → Exec（+ `src_values[4]`, `src_valid[4]` PTX-EMU 上行同步）
- `ExecToWritebackBundle`：Exec → Writeback（+ `result_value[4]`, `memory_data`, `exec_cycles`）
- `WritebackToRegFileBundle`：Writeback → RegFile（`dst_regs[4]`, `values[4]`, `is_accvgpr` CDNA MFMA 累加器）
- `MemoryReqBundle`：Lsu → NoC（`vaddr`, `size`, `lane_mask` intra-SM 合并）
- `MemoryRespBundle`：NoC → Lsu（`data`, `cycles` HazardTracker 释放）
- `ScoreboardQueryBundle`：→ Hazard（`QueryType`, `ctrl`）

#### Scenario: Bundle 字段完整性验证

- **WHEN** Bundle POD 构造时
- **THEN** 每个字段类型 + 默认值SHALL符合 architecture/15 §15.4 表

### Requirement: IComputeDevice 15 方法接口契约

The system SHALL `include/tlm/gpu/i_compute_device.hh` SHALL定义 `IComputeDevice` 抽象接口，含 15 个纯虚方法，命名空间 `cpptlm::gpu`：

**11 preserved from IPtxEmuDevice**：
- `initialize`, `shutdown`, `exe_once`, `sm_exe_once`, `warp_exe_once`
- `set_scoreboard`, `get_thread_state`, `set_active_mask`, `set_next_pc`
- `get_warp_status`, `is_finished`

**1 new (HSK-9 同步通道)**：
- `set_instr_descriptor_buf(const InstrDescriptor*, uint32_t count)` — PTX-EMU 上行同步入口

**2 new (Round 4 user decisions)**：
- `get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id, uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF)` — PTX-EMU 读寄存器真值
- `is_instruction_completed(uint64_t instr_id)` — PTX-EMU 读指令完成状态

**1 reset**：
- `reset()` — 清所有子模块状态

`attach_timing` 不在此接口（per HSK-9 F3.1，保留为 `IPtxEmuDevice` deprecated stub）。

#### Scenario: IComputeDevice 是抽象类

- **WHEN** `std::is_abstract_v<IComputeDevice>` 求值
- **THEN** SHALL返回 `true`（至少 1 个纯虚方法）

#### Scenario: StreamingMultiprocessorTLM 实现 15 方法

- **WHEN** `StreamingMultiprocessorTLM` 派生自 `IComputeDevice`
- **THEN** SHALL override 全部 15 方法（不可遗漏）

### Requirement: SM-owns-state 同步协议

The system SHALL `set_instr_descriptor_buf` SHALL支持 PTX-EMU 上行注入 `InstrDescriptor` 数组，SM 端通过 `FetchUnit → Decode → Exec → RegFileUnit` 流水写入寄存器真值；`get_register_value` SHALL从 `RegFileUnit` 读取真值（不可绕过）；`is_instruction_completed` SHALL查询 `HazardTracker` 完成状态。

**协议语义**（per architecture/15 §15.5.6 + Oracle Round 4 F1.4 双计算决策）：
1. **tick 驱动方**：PTX-EMU 在调用 `set_instr_descriptor_buf` 之后必须调用 `exe_once()` 推进 SM cycle（1 调用 = 1 cycle 契约）；不允许 PTX-EMU 在不调 `exe_once()` 的情况下连续 `set_instr_descriptor_buf`。
2. **末批指令结果取回**：kernel 最后一批指令（无后续 `set_instr_descriptor_buf` 调用）必须由 PTX-EMU 调用 `is_finished()` 阻塞等待 + 轮询 `get_register_value()` 取回剩余寄存器真值；SM 不主动 push。
3. **buf 内存所有权**：`InstrDescriptor* buf` 由 PTX-EMU 持有，SM 仅在 `set_instr_descriptor_buf` 调用期间浅拷贝字段值；PTX-EMU 在 `set_instr_descriptor_buf` 返回后即可复用/释放 buf。SM 不持有 buf 指针。
4. **`get_register_value` lane_id 默认 0xFFFFFFFF** 语义：表示"该 warp 所有 lane 寄存器值相同（SIMT 同构），返回 lane 0 的值"；具体 lane_id 调用时返回该 lane 真值。
5. **`is_instruction_completed` 轮询语义**：PTX-EMU 必须轮询（spin）直到返回 `true` 或 `exe_once()` 调用 N 次后仍 false（per `exe_once()` 返回值）；不允许基于时间/事件回调（无 back-pressure 机制）。

#### Scenario: PTX-EMU 注入 + 读寄存器

- **GIVEN** PTX-EMU 调用 `set_instr_descriptor_buf(&desc, 1)` 注入 1 条 `v_add_f32` 指令
- **AND** `desc.isa_type = InstrDescriptor::IsaType::kCDNA64`, `desc.instr_id = 42`
- **AND** `desc.result_value[0] = 1.0f + 2.0f = 3.0f`（PTX-EMU functional 计算）
- **WHEN** SM 执行 1 cycle 后
- **THEN** `get_register_value(sm_id, warp_id, dst_reg, &out_value)` SHALL返回 `out_value = 3.0f`（SM Exec 计算真值，per F1.4 决策）
- **AND** `is_instruction_completed(42)` SHALL返回 `true`

#### Scenario: buf 内存所有权

- **WHEN** PTX-EMU 调用 `set_instr_descriptor_buf(buf, count)` 并立即 `free(buf)` 或复用 `buf` 内存
- **THEN** SM 端 SHALL NOT 持有 `buf` 指针或解引用 `buf` 在调用返回后
- **AND** SM 端 SHALL 已在调用期间浅拷贝全部字段值到内部 storage

#### Scenario: 末批指令结果取回

- **GIVEN** PTX-EMU 已调用 `set_instr_descriptor_buf` 注入 kernel 最后 N 条指令
- **WHEN** `exe_once()` 已推进到 `is_finished() == true`
- **THEN** PTX-EMU SHALL 通过 `get_register_value(...)` 轮询取回剩余寄存器真值
- **AND** `get_register_value` SHALL 返回 SM `RegFileUnit` 中的最终真值

### Requirement: bit-exact Gate 验证

The system SHALL `BitExactGate` SHALL验证 PTX-EMU functional 与 SM Exec ALU 输出 bit-exact 一致，覆盖 FP32/INT32/INT64 ALU + CDNA MFMA ACCVGPR 累加器。

#### Scenario: v_add_f32 bit-exact

- **GIVEN** PTX-EMU 计算 `v_add_f32(1.0f, 2.0f) → 3.0f`（FP32 IEEE 754 bit-exact）
- **WHEN** SM Exec ALU 计算同一操作
- **THEN** 两边输出二进制位级一致

### Requirement: 删除旧模块

The system SHALL 下列模块SHALL从 `include/tlm/gpu/` + `src/tlm/gpu/` 删除，并从 `chstream_register.hh` 注销：

- `KernelLaunchTLM` (`.hh` + `.cc`)
- `CudaCoreAdapterMVP` (`.hh` + `.cc`)
- `PtxEmuSubmoduleMVP` (`.hh` + `.cc`)
- `PipelineTLM` (`.hh` + `.cc`)
- `ScoreboardTLM` (`.hh` + `.cc`)
- `TensorCoreTLM` (`.hh` + `.cc`)

`include/cudart/{pipeline,scoreboard,tensor_core}_interface.h` SHALL删除，`include/cudart/` 目录SHALL为空目录。

15 旧测试文件（per architecture/15 §15.7.1.B）SHALL从 `test/` 删除。

#### Scenario: 旧模块符号无残留

- **WHEN** 编译构建产物
- **THEN** `KernelLaunchTLM` / `PipelineTLM` / `ScoreboardTLM` / `TensorCoreTLM` 等符号不应出现在 binary 中

### Requirement: 23 ABI 冻结不变量

The system SHALL 下列头文件SHALL零修改（除 `pcie_endpoint_tlm.h` 可加 `[[deprecated]]` 属性）：

- `include/abi/cpptlm_emulator.h`
- `include/tlm/gpu/pcie_endpoint_tlm.h`（仅可加 `[[deprecated]]`）
- `external/PTX-EMU/include/ptxemu/device_api.h`（HSK-8 ACCEPTED，`IPtxEmuDevice` 12/12 frozen）

#### Scenario: ABI 头文件 diff 为空

- **WHEN** 实施完成后 git diff 这 3 个头文件
- **THEN** 除 `pcie_endpoint_tlm.h` 可能新增的属性外，diff 应为空

### Requirement: JSON config 修订

The system SHALL 下列 JSON config SHALL修订：

- `configs/vector_add_n1024.json`：`"type": "KernelLaunchTLM"` → `"type": "StreamingMultiprocessorTLM"`
- `configs/templates/compute_unit_v1.json`：注释更新（per Oracle Round 3 P2，无 type 字符串改动）
- `configs/templates/gpu_soc/gpu_soc_gb203_v1.json`：删除 `KernelLaunchTLM` module + 重写连接 + `GpuComputeUnitTLM` → `StreamingMultiprocessorTLM`（per Oracle Round 3 P1-d 事实修正）
- `examples/dgpu_soc_with_pcie_ip.json`：验证无引用被删模块

#### Scenario: JSON reload L7 测试

- **WHEN** 4 JSON config 通过 `ModuleFactory::loadConfig()` + `instantiateAll()` 加载
- **THEN** 不抛异常 + 实例化成功

### Requirement: DOC HYGIENE 9 项

The system SHALL - `AGENTS.md` STRUCTURE + WHERE-TO-LOOK + PHASE-STATE + ADR 计数（8 → 16）
- `docs/ONBOARDING.md` §5.5 脚本表
- `docs/soc_arch/modules/` 删除 7 个旧模块 docs + README.md 加 VIRTUAL_PATHS
- `openspec/specs/cpptlm-d1-p1-pipeline-scoreboard/spec.md` + `gpgpu-precision-wave2/spec.md` + `cli-f12b-flag/spec.md` 标 superseded
- `scripts/test/docs_sync_check.sh` VIRTUAL_PATHS（12 + 4 条目）
- `test/python/test_f12b_smoke.py` L47 kernel_launch 重定位

#### Scenario: DOC HYGIENE 完整性验证

- **WHEN** 运行 `test/test_doc_hygiene.sh`
- **THEN** 输出 "DOC HYGIENE PASS"，所有 VIRTUAL_PATHS 路径有效

### Requirement: 测试覆盖 146+ assertions

The system SHALL SHALL新增下列测试覆盖：

- 12 子模块单测（每个 1 文件，约 30 LOC × 12）
- L2 Bundle 接线测试（20+ assertions）
- L3 SM 顶层集成测试（30+ assertions，含 bit-exact Gate）
- L4 IComputeDevice 步进测试（25+ assertions）
- L5 CDNA waitcnt 计数测试（15+ assertions）
- L6 端端测试（20+ assertions，SGEMM kernel + architecture/12 校准基线对齐）
- L7 JSON reload 测试
- Gate bit-exact 测试

总 assertions ≥ 146。

#### Scenario: 全测试套件通过

- **WHEN** `./build/bin/cpptlm_tests "[sm-unit][sm-bundle][sm-top][sm-icompute][sm-cdna][sm-e2e][sm-json][sm-gate]"` 执行
- **THEN** 146+ assertions PASS

### Requirement: OpenSpec change lifecycle

The system SHALL 本 change SHALL supersede `cpptlm-dgpu-d1-cdna-isa-phase-a`，archive 时同步合并 `cdna-isa-abstraction` capability spec 到 `openspec/specs/sm-microarchitecture/spec.md`。

#### Scenario: archive 同步

- **WHEN** 实施完成 + Gate 14 项 PASS
- **THEN** `openspec archive cpptlm-dgpu-d1-cdna-isa-sm-rewrite` SHALL成功 + 同步 archive phase-a
- **AND** `openspec/specs/sm-microarchitecture/spec.md` SHALL存在

### Requirement: HSK-9 跨仓协调

The system SHALL HSK-9 公告SHALL发布到 PTX-EMU 仓 `docs/superpowers/specs/2027-02-09-hsk-9-cpptlm-sm-rewrite.md`（per architecture/15 §15.6.3 final draft），CppTLM 仓镜像到 `docs/cross_repo/HSK-9-2027-02-09-cpptlm-sm-rewrite.md`。

PTX-EMU 端改造SHALL跟踪（14 天反馈窗口）：

- `external/PTX-EMU/src/ptxemu/device_api_impl.cc::attach_timing()` → deprecated stub body
- `external/PTX-EMU/src/ptxsim/core/sm_context.cpp` (L34/67/206) → 删除 IScoreboard/IPipelineLatencyProvider/ITensorCoreTiming 用法，改用 IComputeDevice::set_instr_descriptor_buf()
- `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.{h,cpp}` → 删除或重构
- 3 测试重定位（`test_attach_timing_consumer_e2e` → `test_sm_ptx_emu_e2e` 等）

#### Scenario: PTX-EMU 端 14 天反馈

- **WHEN** HSK-9 公告发布
- **THEN** PTX-EMU 团队 14 天反馈窗口（不阻塞 CppTLM 侧 archive）