# HSK-9 Reading Log (per plan Task 0.4 + Oracle §8)

> SM Task 18 + PTX-EMU HSK-9 联合实施必读文档阅读打卡 (per plan Task 0.4 line 276-303).
> **策略**: 结构梳理 (Sisyphus agent, 2026-09-06) + 深度阅读 (后续工程师在子波 1 启动前).
> Oracle §8 推荐 tracker 阅读打卡制 — 此文件为打卡载体.

## 文档清单与结构梳理 (Sisyphus 已完成)

### P0 (必读, ~3 小时)

| ID | 文档 | 行数 | 摘要 (1-3 行) | 阅读状态 |
|----|------|------|--------------|----------|
| P0-1 | `docs/soc_arch/architecture/15-sm-microarchitecture-design.md` §15.3-15.7 | 971 | **大爆炸重写规划, 12 子模块 + 8 Bundle + IComputeDevice**. §15.1-15.2 动机 + 顶层架构; §15.3-15.4 子模块 + Bundle; §15.5 IComputeDevice 契约 (含 set_instr_descriptor_buf 同步协议); §15.6 23 ABI 不变量; §15.7 删除范围 (11 测试 + 6 实现 + 3 vendor + 5 资产); §15.10 Gate 验证清单 (本计划 v3.1 已对齐) | ✅ 已梳理结构 |
| P0-2 | `docs/superpowers/specs/2027-02-09-hsk-9-icompute-api-v1-sm-rewrite.md` | 261 | **HSK-9 公告原文 (Active since 2027-02-09, commit c656222)**. ICOMPUTE_API_VERSION=1 引入 + SM-owns-state 契约; 14d 反馈窗口 2027-02-09 → 2027-02-23; PTXEMU_API_VERSION=1 冻结 + ICOMPUTE_API_VERSION=1 新增 (HSK-9 实际语义, Oracle F-1 已确认). | ✅ 已读前 30 行 + Oracle F-1 复核 |
| P0-3 | `include/tlm/gpu/streaming_multiprocessor_tlm.hh` | 209 | **SM 顶层容器 stub**. 12 子模块 (FetchUnit/DecodeUnit/IssueUnit/ScalarALU/VectorALU/MatrixCore/SIMTLane/LsuGlobal/LsuLDS/RegFileUnit/WritebackUnit/HazardTracker); IComputeDevice 15 方法实现; 4-getter 范式 (GPUTLM InputStreamAdapter/OutputStreamAdapter, Task 1.1 v2 P0-1 修订). | ⏳ 待深度阅读 (Task 1.1 启动前必读) |
| P0-4 | `include/tlm/gpu/i_compute_device.hh` | 101 | **跨仓契约头冻结 (HSK-9 实际语义)**. 15 纯虚方法 (per Oracle F-1 perl 多行正则实测 = 15, 跨行声明 get_register_value 包含在内); 1 dtor + 14 pure virtual + 1 reset = 16 虚方法 (line 95 静态断言); HSK-9 实际内容是 CppTLM 端新接口 + 镜像头, 不是 PTX-EMU 公共头变更. | ✅ 已读全文 |
| P0-5 | `include/tlm/gpu/instruction_descriptor.hh` | 119 | **POD 指令描述符 (ISA-agnostic, per HSK-9 §3)**. PipeClass (7 枚举: ScalarALU/VectorALU/MatrixCore/SIMTLane/LsuGlobal/LsuLDS/Branch); LatencyClass (6 枚举: Fixed1/4/8/16/32Cycle + Memory); isaType (Unknown/CDNA64/PTX70/SASS); CtrlBits (branch_type/is_accvgpr); InstrDescriptor POD 含 64-bit instr_id + PC + exec_mask + result_value[4] + dst/src_regs[4]. 跨行声明易误算纯虚方法数 (Oracle F-1 已警示). | ✅ 已读前 80 行 |
| P0-6a | `external/PTX-EMU/AGENTS.md` | 158 | **PTX-EMU 仓总则 + HSK 链**. HSK-1..7 ACCEPTED, HSK-8 ACCEPTED (PR #14 merged), **HSK-9 📤 已发布** (Task 0.3 修订, 单 owner 下 PR merge = ack); drift_check 8 invariants; ctest 命名 `unit_/integration_/e2e_` 前缀; Catch2 标签 `<type>;<subject>` (e.g. `[hsk-9;ptxemu]`); PTX-EMU 硬性规则 (公共头冻结 + 任何变更须 HSK). | ✅ 已读 HSK chain (line 20-30) |
| P0-6b | `external/PTX-EMU/include/ptxemu/device_api.h` | 129 | **PTX-EMU 公共 API 冻结 (HSK-8 spec)**. `IPtxEmuDevice` 11 preserved 方法 (initialize/shutdown/exe_once/sm_exe_once/warp_exe_once/set_scoreboard/get_thread_state/set_active_mask/set_next_pc/get_warp_status/is_finished) + 1 deprecated (attach_timing, HSK-9 §3 F3.1 保留 stub); `PTXEMU_API_VERSION=1` 冻结 + static_assert 守卫 (`公共签名变更必须签发 HSK-9 bump VERSION`); ThreadState/WarpStatus 枚举 + struct 定义. | ✅ 已读全文 |
| P0-6c | `external/PTX-EMU/src/ptxemu/device_api_impl.cc` | 346 | **IPtxEmuDevice 实现 (HSK-8 Phase 2)**. PTX-EMU 端 device API 的具体实现. Task PTX-2 v2 P0-2 修订: 不新增 override, 改用 injector API 模式 (`set_compute_device(icd::IComputeDevice*)`). | ⏳ 待深度阅读 (子波 3 PTX-2 启动前必读) |

### P1 (重要, ~15 分钟)

| ID | 文档 | 行数 | 摘要 | 阅读状态 |
|----|------|------|------|----------|
| P1-1a | `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.h` | 28 | **injector API 入口头 (per ADR-0020)**. 声明 `sm_cpptlm_inject::step_b_set_blocked_cycles()` 函数, 接收 `IPipelineLatencyProvider*` + `ITensorCoreTiming*` + `WarpContext*` + `ptxemu::ir::StatementContext&`; no-op fallback (per lessons-learned §14 byte-identical contract). | ✅ 已读全文 |
| P1-1b | `external/PTX-EMU/src/ptxsim/core/sm_context_cpptlm_inject.cpp` | 39 | **step_b 阻塞周期计算实现**. pipeline fractional cycles → ceil → instr_latency; tensor core fallback; PTXIR 指令表 fallback; warp.set_blocked_cycles_for_active() 应用. 子波 3 PTX-3 改造重点. | ✅ 已读全文 |

### P2 (参考, ~15 分钟)

| ID | 文档 | 行数 | 摘要 | 阅读状态 |
|----|------|------|------|----------|
| P2-1 | `docs/soc_arch/adr/ADR-SOC-16-sm-microarchitecture.md` | 179 | **SM 微架构 ADR 背书**. §1 背景; §2 决策 (SM 拓扑 + 8 Bundle + IComputeDevice 15 方法 + SM-owns-state 模式 + 删除范围); §3 反转既有 ADR (ADR-SOC-02 CU 黑盒优先); §4 后果; §5 实施; §7 Gate 14 项 (per architecture/15 §15.10); §8 风险与缓解; §9 评审汇总 (Oracle 5 轮); §10 参考. | ✅ 已梳理章节结构 |

## 关键关联 (cross-reference)

| 文档 | 引用 |
|------|------|
| 计划 `2027-02-10-sm-task18-impl-and-ptxemu-hsk9.md` | P0-1 §15.10 Gate G1-G14; P0-2 HSK-9 spec; P2-1 ADR-SOC-16 §2 + §7 |
| AGENTS.md (根) | KEY INVARIANTS 节: 测试状态 44498 assertions (v3.1 修订); HSK-9 镜像 |
| CppTLM `i_compute_device.hh` | per Oracle F-1: 15 纯虚方法, perl 多行正则实测 = 15 (含跨行 get_register_value) |
| PTX-EMU `device_api.h` | HSK-8 spec §"公共头路径" #1; PTXEMU_API_VERSION=1 冻结 |
| PTX-EMU `sm_context_cpptlm_inject.h` | ADR-0020 cpptlm injection extraction; PTX-3 子波 3 改造重点 |
| HSK-9 feedback tracker | `docs/superpowers/specs/HSK-9-feedback-tracker.md` (Task 0.3 创建, PR #21 已 MERGED d5a58cf5) |

## 阅读顺序建议 (per Oracle §8 + plan line 1976-1990)

1. **子波 1 启动前必读 (~2 小时)**:
   - P0-1 §15.5-15.6 (IComputeDevice 契约 + 23 ABI 不变量)
   - P0-2 (HSK-9 spec 完整公告)
   - P0-3 (SM 顶层 stub — Task 1.1 实现目标)
   - P0-4 (IComputeDevice 接口完整)
   - P0-5 (InstrDescriptor POD 完整)
   - P0-6a (PTX-EMU AGENTS.md HSK chain + drift_check)

2. **子波 2 启动前补读 (~30 分钟)**:
   - P0-1 §15.3-15.4 (12 子模块 + 8 Bundle 细节)

3. **子波 3 启动前必读 (~1 小时)**:
   - P0-6b + P0-6c (PTX-EMU 公共 API + 实现)
   - P1-1a + P1-1b (injector API 入口 + 实现)

4. **参考 (按需 ~15 分钟)**:
   - P2-1 (ADR-SOC-16 完整内容)
   - P0-1 §15.7 (删除范围, 仅做清理时读)

## 阅读打卡 (后续工程师填写)

| 工程师 session | 阅读完成时间 | 必读完成 (P0-1/2/3/4/5 + P0-6a) | 备注 |
|----------------|--------------|------------------------------|------|
| _TBD_ | | | 子波 1 启动前必填 |
| _TBD_ | | | 子波 3 启动前必填 |

## 风险与缓解 (per Oracle §8)

| 级别 | 风险 | 缓解 |
|------|------|------|
| P0 | 工程师跳过必读直接进子波 1 | 子波 1 启动 prompt 模板硬编码 "先 cat AGENTS.md + 读 P0 清单 + 在本 reading-log 打卡" |
| P1 | reading-log 与 plan Task 0.4 内容漂移 | reading-log 章节摘要直接 grep 章节标题, 与 plan 一致; 后续 commit 维护 |

## 关联文档

- 计划: `docs/superpowers/plans/2027-02-10-sm-task18-impl-and-ptxemu-hsk9.md` Task 0.4 (line 276-303)
- Oracle 复审 Task 0.3 §8: `ses_f8909c150ffeVZ7aQA0VgEaynB`
- HSK-9 feedback tracker: `docs/superpowers/specs/HSK-9-feedback-tracker.md`
- HSK-9 baseline tracker: `docs/superpowers/specs/HSK-9-baseline-tracker.md`
- 必读文档清单 (plan line 1976-1990): 完整 P0/P1/P2 列表


---

## 深度摘要 (per Oracle final verdict 选项 B, Sisyphus 2026-09-06 深度阅读)

> 此节为结构化深度摘要, 基于 Read/grep 全文 + Oracle P0 风险分析 + ADR-SOC-16 Gate 1-14 关联.
> 后续工程师仍应在子波 1/3 实施前做完整通读 (~2h), 此节为索引 + 关键约束清单.

### P0-1 SM 微架构设计 (architecture/15) — 深度摘要

**971 行, 12 章节, 大爆炸重写规划 (v1.0-draft, 2027-02-09)**

关键章节 (子波 1 必读):
- **§15.5 IComputeDevice 接口契约** (含 15.5.0 方向反转 + 15.5.2 Functional/Timing 划分 + 15.5.6 同步协议)
  - **方向反转**: SM-owns-state 模式 — SM (CppTLM) 实现 IComputeDevice, PTX-EMU 通过 `set_instr_descriptor_buf()` 注入已解码 InstrDescriptor, SM 持寄存器唯一真值
  - **双计算 + bit-exact Gate**: PTX-EMU functional (控制流决策) + SM timing (唯一真值源) 必须 bit-exact, Gate 在每次 `set_instr_descriptor_buf()` 后 pull `result_value[]` 验证
  - **15 方法契约表**: 11 preserved + 1 HSK-9 (set_instr_descriptor_buf) + 2 Round 4 (get_register_value + is_instruction_completed) + 1 reset
  - **状态划分表** (PC / 寄存器 / EXEC mask / Hazard / 内存): PTX-EMU 持有 PC + EXEC mask; SM 持寄存器真值 + Hazard + 内存数据 + Cycle 计数
- **§15.6 23 ABI 不变量保护** (含 15.6.1 严格禁止 + 15.6.2 严格保护 + 15.6.3 HSK-9 必要性 + 15.6.4 PTX-EMU 子模块公共头修改边界 + 15.6.5 SFU 单元归属)
  - **严格禁止**: 23 ABI 冻结头 + PcieEndpointIP 17 端口 + PTX-EMU 公共头 + CppTLM Clean Room
  - **HSK-9 退路**: 如 PTX-EMU 14d 窗口内无法同步改造, CppTLM 保留 3 vendor 接口兼容 shim
  - **SFU 归属**: 归 ScalarALU 子模块内部 SubPipe enum (kINT_FP32_Shared, kFP64, kSFU, kBranch)
  - **特殊同步 (s_waitcnt/s_barrier)**: 归 ScalarALU 子模块
- **§15.10 Gate 验证清单** (大爆炸 Gate 14 项):
  - 23 ABI 头文件零修改 / PcieEndpointIP 17 端口布局不变
  - [pcie]/[axi] 测试 100% PASS 基线 / [gpu] 新基线 100% PASS / [sm-microarch] 新增测试 100% PASS
  - JSON reload L7 测试 (4 config, Task 2.18)
  - docs_sync_check.sh --strict 通过 (Task 2.16 DOC HYGIENE)
  - PTX-EMU 侧 CI 绿 / PTX-EMU functional 闭环验证 (双计算 bit-exact Gate, Task 4.3 + 4.7 + 4.9)
  - openspec validate PASS / Oracle 评审 0 P0 + ≤3 P1

### P0-2 HSK-9 spec (2027-02-09) — 深度摘要

**261 行, CppTLM → PTX-EMU 协调公告 (Active since 2027-02-09, commit c656222)**

关键条款:
- **§1 重构动机**: 删除 3 vendor 接口 (set_scoreboard via IScoreboard*, attach_timing 3 vendor)
- **§2 接口变更总览**: 12 方法保留 (含 attach_timing deprecated stub) + 4 方法新增 (IComputeDevice 专属)
- **§3 跨仓契约细节**: IComputeDevice = 11 preserved (签名与 IPtxEmuDevice 逐字同构) + 1 new + 2 new + 1 reset = 15 方法
- **版本号**: PTXEMU_API_VERSION=1 冻结 + ICOMPUTE_API_VERSION=1 新增
- **§8 退路**: PTX-EMU 14d 窗口失败 → CppTLM 保留 3 vendor 接口兼容 shim
- **§9 实施任务清单**: 20 tasks, 已完成 Task 1-17 + 19-20

### P0-3 SM 顶层 stub (streaming_multiprocessor_tlm.hh) — 深度摘要

**210 行, namespace tlm::sm 内 12 子模块 stub + namespace tlm 内 StreamingMultiprocessorTLM**

关键发现 (子波 1 必读):
- **12 子模块 stub** (line 36-130): FetchUnitTLM / DecodeUnitTLM / IssueUnitTLM / ScalarALU / VectorALU / MatrixCore / SIMTLane / LsuGlobal / LsuLDS / RegFileUnit / WritebackUnit / HazardTracker
- **类名保留**: per v2 P0-5 修订 (Task 2.1 拆独立 .hh/.cc 时**不改名**)
- **StreamingMultiprocessorTLM** (line 136-208): 继承 ChStreamModuleBase + IComputeDevice, 15 方法 stub + reset() 名字遮蔽修复 (line 181-185)
- **GPUTLM 4-getter 范式** (per v2 P0-1): Task 1.1 应加 InputStreamAdapter<>/OutputStreamAdapter<> 4 端口访问器

### P0-5 InstrDescriptor POD (instruction_descriptor.hh) — 深度摘要

**120 行, namespace cpptlm::gpu 内, ISA-agnostic**

字段分解: 8B header + 8B instr_id + 16B exec info + 32B result_value[]/dst/src + 16B memory_data + 2B pipe/latency + 4B CtrlBits + 8B lane_mask
- **静态断言** (line 115): `sizeof(InstrDescriptor) <= 128` (G3 Gate)
- **PipeClass 7 枚举** (line 28-36): kScalarALU/kVectorALU/kMatrixCore/kSIMTLane/kLsuGlobal/kLsuLDS/kBranch
- **LatencyClass 6 枚举** (line 39-46): kFixed1Cycle/kFixed4Cycle/kFixed8Cycle/kFixed16Cycle/kFixed32Cycle/kMemory

### P0-6a PTX-EMU AGENTS.md — 深度摘要

**158 行, HSK chain + drift_check 8 invariants + 结构 + code map**

关键条款 (子波 3 必读):
- **HSK 链**: HSK-1..8 ACCEPTED, **HSK-9 📤 已发布** (Task 0.3 修订, PR #21 MERGED d5a58cf5)
- **drift_check 8 invariants**: Invariant 1-5 (HSK-8) + Invariant 6 (Phase 2.2/2.3 delegated methods) + Invariant 7 (ANTLR4 path) + Invariant 8 (Phase 1.5 namespace)
- **PTX-EMU 结构**: src/{cudart, grammar, ptx_ir, ptx_parser, ptxsim/{barrier, core, instructions, memory}, ptxir, memory, register}, include/, tests/{unit, integration, e2e}
- **ctest baseline**: 254/254 PASS (Phase 1.5 namespace migration)
- **HSK-9 PTX-EMU 端必须修改 5 项**: `device_api_impl.cc attach_timing deprecated stub` + `sm_context_cpptlm_inject.cpp 移除 attach_timing consumer` + `sm_context_cpptlm_inject.{h,cpp} 重构/删除` + `sm_context.cpp L34/67/206 改造` + 3 个依赖 attach_timing 测试重定位
- **测试命名**: `unit_<subject>` / `integration_<subject>` / `e2e_<subject>`; Catch2 标签 `<type>;<subject>` (e.g. `[hsk-9;ptxemu]`)
- **HSK protocol 文档**: `docs/superpowers/specs/HSK-PROTOCOL-NOTES.md`

---

## 阅读打卡 (per Oracle final verdict 选项 B)

| 工程师 session | 阅读完成时间 | 必读完成 (P0-1 §15.5-15.6 + §15.10 + P0-2 + P0-3 + P0-4 + P0-5 + P0-6a) | 备注 |
|----------------|--------------|--------------------------------------------------------------|------|
| **Sisyphus** (2026-09-06, 选项 B 主动深度阅读) | **2026-09-06 22:30** | ✅ **全部 6 P0 必读完成** (结构化深度摘要, 见上) | 子波 1 启动前完成 |
| _后续工程师_ | | 子波 3 启动前补 P0-6b/c + P1-1a/b |

## 关键约束清单 (子波 1 实施时必查)

1. **15 方法签名冻结**: IComputeDevice 11 preserved 方法签名与 IPtxEmuDevice 逐字同构
2. **公共头不动**: 修改 IComputeDevice ≠ 修改 PTX-EMU 公共头 (`device_api.h` 冻结)
3. **result_value[] 回填**: PTX-EMU 下次 `set_instr_descriptor_buf()` 时 SM 在 buf `result_value[]` 字段回填 (双计算 bit-exact Gate)
4. **reset() 名字遮蔽修复**: `void reset() override { do_reset({}); }` + `using ChStreamModuleBase::reset;`
5. **12 子模块类名保留**: Task 2.1 拆独立 .hh/.cc 时**保 FetchUnitTLM 等**, 仅物理拆分 (v2 P0-5)
6. **SM-owns-state**: 寄存器值唯一存在 SM `RegFileUnit`, PTX-EMU functional 仅做控制流决策
7. **HSK-9 退路**: PTX-EMU 14d 窗口失败 → CppTLM 保留 3 vendor 接口兼容 shim
8. **InstrDescriptor POD 大小约束**: `sizeof(InstrDescriptor) <= 128` (G3 Gate)
9. **15.5.2 Functional/Timing 划分**: 双计算 + bit-exact Gate (per Oracle Round 4 F1.4)
10. **GPUTLM 4-getter 范式** (v2 P0-1): Task 1.1 加 InputStreamAdapter<>/OutputStreamAdapter<> 4 端口访问器

## 关联文档

- 计划 Task 0.4 (line 276-303): P0/P1/P2 文档清单 + 时间估算
- Oracle final verdict (session `ses_f88ec48cdffeqIHpez0aJ7d2ed`) §9 选项 B 推荐
- 会话准备总结: `docs/superpowers/specs/HSK-9-session-prep-summary.md` (commit `9766800`)
