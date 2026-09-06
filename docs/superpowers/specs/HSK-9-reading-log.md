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
