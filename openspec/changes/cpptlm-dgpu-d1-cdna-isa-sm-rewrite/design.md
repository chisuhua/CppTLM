# Design: SM Microarchitecture Rewrite

> **Status**: Proposed
> **Date**: 2027-02-09
> **Parent change**: `cpptlm-dgpu-d1-cdna-isa-sm-rewrite`
> **Supersedes**: `cpptlm-dgpu-d1-cdna-isa-phase-a`（阶段 A 双轨决策反转）
> **Author**: CppTLM dGPU SoC team
> **References**: `docs/soc_arch/architecture/15-sm-microarchitecture-design.md` (v5.0, 971 行, commit `650e9e9`) — 本设计文档仅作为 OpenSpec 入口，详细设计请参考 architecture/15。

---

## 1. 背景

ADR-SOC-02 (2026-06-14) "CU 黑盒优先" 决策无法承载 dGPU SoC v1.0 CDNA 真实 ISA 集成（per ADR-SOC-15），主要问题：

- 精度对齐失败（CDNA 校准基线 M1-M5 需要 wavefront 调度 + SIMT 分歧 + scoreboard 计数）
- PTX-EMU 真值集成阻塞（HSK-9 协议要求 SM 端可读寄存器值 + 指令完成状态）
- HSK-9 跨仓协调失败（`attach_timing` 需废弃，改为 SM-owns-state 模式）
- 业界范式背离（完整 SM 微架构是 NV/AMD/MGPUSim 标准做法）

ADR-SOC-16（commit `90c9778`）反转 ADR-SOC-02 + ADR-SOC-15 §3 D2，确立完整 SM 微架构重构决策。

## 2. SM 拓扑（per architecture/15 §15.2）

```
StreamingMultiprocessorTLM (顶层 ChStreamModuleBase + IComputeDevice)
├── FetchUnitTLM        # 指令抓取
├── DecodeUnitTLM       # 指令解码 → InstrDescriptor
├── IssueUnitTLM        # 指令发射（round-robin + CGGTY 阈值）
├── ScalarALU           # INT/FP32/FP64/SFU/Branch 子管道
├── VectorALU           # V-pipe SIMD
├── MatrixCore          # CDNA MFMA 子集（20 指令）
├── SIMTLane           # EXEC mask + 派态分歧
├── LsuGlobal          # 全局内存
├── LsuLDS             # 共享内存
├── RegFileUnit        # 寄存器堆（SM 真值源）
├── WritebackUnit      # 写回 + 释放 Hazard
└── HazardTracker      # RAW hazard + vmcnt/lgkmcnt 计数
```

## 3. IComputeDevice 接口（per architecture/15 §15.5）

14 方法 = 11 preserved from `IPtxEmuDevice` + 1 new (`set_instr_descriptor_buf`) + 2 new (Round 4 user decisions):

```cpp
class IComputeDevice {
public:
    virtual ~IComputeDevice() = default;

    // 11 preserved
    virtual bool initialize(const DeviceConfig& cfg) = 0;
    virtual void shutdown() = 0;
    virtual int  exe_once() = 0;
    virtual int  sm_exe_once(uint32_t sm_id) = 0;
    virtual int  warp_exe_once(uint32_t sm_id, uint32_t warp_id) = 0;
    virtual bool set_scoreboard(uint32_t sm_id, uint32_t warp_id, uint64_t mask) = 0;
    virtual int  get_thread_state(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id) = 0;
    virtual bool set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask) = 0;
    virtual bool set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc) = 0;
    virtual WarpStatus get_warp_status(uint32_t sm_id, uint32_t warp_id) = 0;
    virtual bool is_finished() = 0;

    // 1 new: HSK-9 同步通道
    virtual void set_instr_descriptor_buf(const InstrDescriptor* buf, uint32_t count) = 0;

    // 2 new: Round 4 user decisions
    virtual bool get_register_value(uint32_t sm_id, uint32_t warp_id, uint32_t reg_id,
                                     uint64_t* out_value, uint32_t lane_id = 0xFFFFFFFF) = 0;
    virtual bool is_instruction_completed(uint64_t instr_id) = 0;

    // 1 reset
    virtual void reset() = 0;
};
```

`attach_timing` 不在此接口（per HSK-9 F3.1，保留为 `IPtxEmuDevice` deprecated stub）。

## 4. SM-owns-state 同步协议（per architecture/15 §15.5.6）

- SM 持寄存器唯一真值源（`RegFileUnit`）
- PTX-EMU 端通过 `set_instr_descriptor_buf()` 注入已解码 InstrDescriptor（含 `isa_type` / `instr_id` / `result_value[]` / `memory_data`）
- PTX-EMU 通过 `get_register_value()` / `is_instruction_completed()` 读路径/就绪协议同步
- **双计算 + Gate bit-exact**（per Oracle Round 4 F1.4 决策）：PTX-EMU functional 用于控制流 + SM Exec 算 timing 真值 + Gate 验证两边 ALU 实现 bit-exact

## 5. 8 Bundle（per architecture/15 §15.4）

| Bundle | 方向 | 关键字段 |
|--------|------|----------|
| FetchToIssueBundle | Fetch→Issue | instr_desc, warp_id, pc |
| DecodeToIssueBundle | Decode→Issue | + PipeClass, LatencyClass |
| IssueToExecBundle | Issue→Exec | + src_values[4], src_valid[4] |
| ExecToWritebackBundle | Exec→WB | + result_value[4], memory_data, exec_cycles |
| WritebackToRegFileBundle | WB→RegFile | dst_regs[4], values[4], is_accvgpr |
| MemoryReqBundle | Lsu→NoC | vaddr, size, lane_mask |
| MemoryRespBundle | NoC→Lsu | data, cycles |
| ScoreboardQueryBundle | →Hazard | QueryType, ctrl |

## 6. 删除范围（per architecture/15 §15.7）

| 类别 | 数量 | 文件 |
|------|------|------|
| 实现删除 | 6 | KernelLaunchTLM + CudaCoreAdapterMVP + PtxEmuSubmoduleMVP + PipelineTLM + ScoreboardTLM + TensorCoreTLM |
| Vendor 接口 | 3 | include/cudart/{pipeline,scoreboard,tensor_core}_interface.h + cudart 目录 |
| 测试删除 | 15 | per architecture/15 §15.7.1.B |
| JSON config | 4 | vector_add_n1024 + compute_unit_v1 + gpu_soc_gb203_v1 + dgpu_soc_with_pcie_ip |
| 旁路修复 | 4 | gpu_soc_tlm.{h,cc} + async_completion_adapter + main.cpp + chstream_register |
| DOC HYGIENE | 9 项 | AGENTS.md + ONBOARDING + 7 modules docs + 3 main specs + VIRTUAL_PATHS + test scripts |

## 7. Gate 14 项（per architecture/15 §15.10）

1. ✅ SM 拓扑：12 子模块 + ChStreamModuleBase + Bundle 连接
2. ✅ IComputeDevice 14 方法签名冻结 + 命名空间 `cpptlm::gpu`
3. ✅ SM-owns-state：RegFileUnit 唯一真值源 + PTX-EMU 通过 set_instr_descriptor_buf 同步
4. ✅ 8 Bundle POD 字段完整 + 流向正确
5. ✅ Gate bit-exact：PTX-EMU functional 与 SM Exec ALU 实现 bit-exact
6. ✅ HSK-9 协议：attach_timing 保留为 IPtxEmuDevice deprecated stub（device_api.h 不动）
7. ✅ SFU 子管道在 ScalarALU 内（INT/FP32/FP64/SFU/Branch 5 子管道）
8. ✅ 23 ABI 冻结：include/abi/cpptlm_emulator.h 零修改 + pcie_endpoint_tlm.h 仅可加 [[deprecated]]
9. ✅ 删除范围：6 实现 + 3 vendor + 15 测试 + 4 JSON + 旁路 + DOC HYGIENE
10. ✅ 20 原子 commit：每 commit 可独立编译
11. ✅ OpenSpec change sm-rewrite (supersedes phase-a)
12. ✅ HSK-9 公告草稿 179 行发布
13. ✅ 测试覆盖：12 子模块单测 + L2-L6 集成测试 + L7 JSON reload (146+ assertions)
14. ✅ Oracle 5 轮评审通过

## 8. 实施

20 个原子 commit（per `docs/superpowers/plans/2027-02-09-sm-microarchitecture-rewrite.md`），每 commit 可独立编译。

---

**详细设计**：参考 `docs/soc_arch/architecture/15-sm-microarchitecture-design.md`（v5.0, 971 行）