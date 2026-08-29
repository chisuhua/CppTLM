# cpptlm-v05-mvp-s3-command-pipeline: CP + PM4 + TMU + 全链路 E2E + v0.5.0-MVP tag

> **状态**: 🚧 In Progress (39/53, T-s3-1/2/3 done, T-s3-4/5 deferred to v05-mvp-release-gate) · **日期**: 2026-08-21 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5
> **取代**: [`openspec/changes/archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/`](../../archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/) (原单 change)
> **依赖**: [`s1-ptxemu-integration/`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) + [`s2-dgpu-board/`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/) 必须已 archive
> **目标**: W5-10 交付 CP/NVIDIA PM4/TMU 深度集成 + 全链路 E2E + v0.5.0-MVP tag

---

## Why

s3 是 MVP 切片的"数据面贯通"——把 s1 (PTX-EMU 集成) + s2 (板卡骨架) 通过 NVIDIA method packet + TMU 反压停 fetch 接通到端到端 cuLaunchKernel→kernel 执行→cuStreamSynchronize 完整数据面。

s3 价值(必须依赖 s1+s2):
- CP fetch NVIDIA method packet(替代原 Mesa-style TYPE3)对齐 UsrLinuxEmu `unpackPm4Header`
- TMU 反压停 fetch(替代 LIFO)对齐真实 GPU 行为
- 全链路 E2E + v0.5.0-MVP tag 收官

**触发事件**:
- s1+s2 已 archive(PtxEmuSubmoduleMVP + CudaCoreAdapter + DGpuBoardTLM)
- 2026-08-20 Phase F-H.3 决定 NVIDIA method packet 格式家族
- 2026-08-20 Phase F-D.2 H5 决定反压停 fetch

---

## What Changes

### 1. 修改文件(per Oracle ses_fe0b6e44 s2 骨架修复,2026-08-21)

| 文件 | 修改 |
|------|------|
| `include/tlm/gpu/command_processor_mvp.hh` + `.cc`(s2 骨架已创建) | **填充实现**: NVIDIA method packet 5-state FSM(GPU VA fetch + parse_method + DECODE 实际逻辑) |
| `include/tlm/gpu/pm4_decoder_mvp.hh`(s2 接口已创建)| **填充实现**: `Pm4Decoder` 继承 `Pm4DecoderInterface`,4 method_addr ranges 解析 |
| `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc`(s2 骨架已创建) | **填充实现**: dep chain + 反压停 fetch + `set_handler` 注入到 SubmitQueue |
| `test/test_command_processor_mvp.cc` | 5 transition + NVIDIA method packet decode 真实测试(替代 s2 骨架的 no-op 测试) |
| `test/test_pm4_decoder_mvp.cc` + `test_pm4_decoder_mvp_integration.cc` | NVIDIA method packet + CP 集成 PASS |
| `test/test_tmu_dispatch_processor_mvp.cc` | submit / 反压停 fetch / dep chain / 环检测 真实测试(替代 s2 骨架的反压测试) |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `CHANGELOG.md` | 记录 v0.5.0-MVP release |
| `git tag` | 创建 `v0.5.0-MVP` tag |

### 3. **不**修改文件(沿用)

- `include/tlm/gpu/scoreboard_tlm_v05_mvp.hh` + `pipeline_tlm_v05_mvp.hh`(**已取消**,per Phase I.2,直接使用现有 `scoreboard_tlm.hh`/`pipeline_tlm.hh`)
- ~~`test_cuda_core_adapter_mvp_whitebox.cc`~~(**已删除**,per DP4=C)

---

## Acceptance Gate(本 change 独立)

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **s3-G1** Pm4Decoder NVIDIA method packet 4 method_addr ranges 测试 PASS | CppTLM | ⏳ W5 | `ctest -R "test_pm4_decoder_mvp"` PASS |
| **s3-G2** CommandProcessor 5 transition + GPU VA fetch 测试 PASS | CppTLM | ⏳ W5 | `ctest -R "test_command_processor_mvp"` PASS |
| **s3-G3** TmuDispatchProcessor 反压停 fetch + dep chain + 环检测 测试 PASS | CppTLM | ⏳ W5 | `ctest -R "test_tmu_dispatch_processor_mvp"` PASS |
| **s3-G4** CP + Pm4Decoder 集成测试 PASS | CppTLM | ⏳ W5 | `ctest -R "test_pm4_decoder_mvp_integration"` PASS |
| **s3-G5** validate_topology CMake target 集成 PASS | CppTLM | ⏳ W9 | `cmake --build build --target validate_topology` PASS |
| **s3-G6** 全部 ≥850 测试 PASS(per ADR-SOC-06 G-MVP-4 + Oracle M1) | CppTLM | ⏳ W9 | `build/bin/cpptlm_tests` PASS |
| **s3-G7** `v0.5.0-MVP` tag 创建 | CppTLM | ⏳ W10 | `git tag -a v0.5.0-MVP -m "..."` |

**最终验收(MVP 完成时)**:
- [ ] s3-G1 ~ s3-G7 全部 ✅
- [ ] 全部 ≥850 测试 PASS(baseline 817 per AGENTS.md 2026-08-24 + s1 12 + d1 7-9 + MVP s2/s3 新增)
- [ ] 编译防火墙验证仍 PASS
- [ ] docs 同步检查 PASS
- [ ] `git tag -a v0.5.0-MVP -m "..."`

---

## Cross-Repo Coordination(本 change)

| 仓 | 跟踪载体 | 状态 |
|----|---------|:---:|
| **PTX-EMU** | 无新 API 需求 | ✅ 已 satisfied |
| **UsrLinuxEmu** | 无新 API 需求(0x28 -ENOSYS 永久锁定)| ✅ 已 satisfied |
| **TaskRunner** | `cuLaunchKernel` 解析 | ✅ 已 ship |

---

**Refs**:
- [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/proposal.md)
- [`design.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/design.md)
- [`tasks.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/tasks.md)
- [`../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)
- [`../../../docs/soc_arch/modules/command-processor.md`](../../../docs/soc_arch/modules/command-processor.md)
- [`../../../docs/soc_arch/modules/pm4-decoder.md`](../../../docs/soc_arch/modules/pm4-decoder.md)
- [`../../../docs/soc_arch/modules/tmu-dispatch-processor.md`](../../../docs/soc_arch/modules/tmu-dispatch-processor.md)
- [`../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) (依赖)
- [`../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/) (依赖)

---

**起草**: Sisyphus (2026-08-21,per Oracle ses_fe179d02 拆分建议)
**Owner**: CppTLM Team
**状态**: 🚧 In Progress
