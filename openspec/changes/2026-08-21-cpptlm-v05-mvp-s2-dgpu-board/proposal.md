# cpptlm-v05-mvp-s2-dgpu-board: DGpuBoard 板卡 + 4 IOCTL stub

> **状态**: 📋 Proposed — 2026-08-21 · **日期**: 2026-08-21 · **Owner**: CppTLM Team (Sisyphus)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D4/D5
> **取代**: [`openspec/changes/archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/`](../../archive/2026-08-19-cpptlm-v05-mvp-superseded-by-s1-s2-s3/) (原单 change)
> **依赖**: [`2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) (编译依赖,需 s1 已完成 PtxEmuSubmoduleMVP + CudaCoreAdapter)
> **目标**: W3-4 交付 DGpuBoard 板卡 + 4 IOCTL stub + SubmitQueue WDU 分发网络

---

## Why

DGpuBoardTLM 是 MVP 切片的"宿主"——把 s1 完成的 PtxEmuSubmoduleMVP + CudaCoreAdapter 包装为可 JSON 驱动的 ChStreamModuleBase 板卡,接入 UsrLinuxEmu 真实 IOCTL 路径。

s2 价值独立(但**编译依赖 s1**):
- 可独立测试 4 IOCTL stub 端到端(driver fallback 路径)
- DGpuBoardTLM 8 组件包装是 S3 数据面的物理载体
- SubmitQueue WDU 分发网络是 NVIDIA Hopper 蓝图的关键中间层

**触发事件**:
- s1 已完成 PtxEmuSubmoduleMVP + CudaCoreAdapter(per Oracle 拆分)
- 2026-08-20 Phase F-H.3 决定 4 IOCTL(0x27/0x29/0x01 真实 + 0x28 永久 -ENOSYS)
- 2026-08-20 Phase F-H.5 新增 SubmitQueue WDU 分发网络

---

## What Changes

### 1. 新建文件

| 文件 | 用途 |
|------|------|
| `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc` | DGpuBoardTLM ChStreamModuleBase(**8 组件**包装,per Phase F-H.2) |
| `include/tlm/gpu/doorbell_mvp.hh` + `.cc` | SQ tail register + strong-order write(per `docs/research/PCIe/PCIe_上的保序write.md`) |
| `include/tlm/gpu/submit_queue_mvp.hh` + `.cc` | **🆕 WDU 分发网络**(per Phase F-H.5,per `docs/research/WDUtoSM/overview.md` NVIDIA Hopper) |
| `include/tlm/gpu/completion_ring_mvp.hh` + `.cc` | push + host_notify 重设计 |
| `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc` | **4 IOCTL** stub(per Phase F-H.3): 0x27 LOAD / 0x28 -ENOSYS / 0x29 UNLOAD / 0x01 PUSHBUFFER |
| `include/tlm/gpu/command_processor_mvp.hh` + `src/tlm/gpu/command_processor_mvp.cc` | **🆕 CP 骨架**(per s2 T-s2-3a,5-state FSM no-op + set_decoder 注入接口,s3 填充) |
| `include/tlm/gpu/pm4_decoder_mvp.hh` | **🆕 Pm4DecoderInterface 纯接口头**(per s2 T-s2-3a,含 `parse_method` 纯虚方法,s3 填充) |
| `include/tlm/gpu/pm4_types_mvp.hh` | **🆕 Pm4MethodHeader/Pm4MethodDispatch 数据类型**(per s2 T-s2-3a,被 pm4_decoder_mvp.hh include,s3 依赖此类型定义 `parse_method` 返回值) |
| `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `src/tlm/gpu/tmu_dispatch_processor_mvp.cc` | **🆕 TMU 骨架**(per s2 T-s2-3b,反压 + set_handler 注入接口,s3 填充) |
| `include/tlm/gpu/tmu_handler_mvp.hh` | **🆕 TmuHandlerInterface 纯接口头**(per s2 T-s2-3b,s3 填充) |
| `configs/dgpu_board_v1_mvp.json.in` | JSON config + CMake configure_file 注入 `${PTX_EMU_ROOT}` |
| `test/test_dgpu_board_v1_mvp_from_config.cc` | 6 SECTION E2E(per ADR-SOC-06 G-MVP-2)**s2 W4 降级为 5 SECTION**(item 4 标 ⏳) |
| `test/test_usrlxemu_ioctl_stub.cc` | **4 IOCTL** PASS(per Phase F-H.3) |
| `test/test_submit_queue_mvp_route.cc` + `_enqueue.cc` + `_dispatch.cc` + `_complete.cc` + `_concurrent.cc` | **5 个单测**(per Phase F-H.5 §7) |
| `test/test_doorbell_strong_order_mvp.cc` | Doorbell strong-order 250-700ns 区间 + 同 stream 顺序 PASS |
| `test/test_command_processor_mvp_skeleton.cc` | **🆕 CP 骨架测试**:5 state 转换 no-op + wake PASS |
| `test/test_tmu_dispatch_processor_mvp_skeleton.cc` | **🆕 TMU 骨架测试**:反压 + 容量管理 PASS |

### 2. 修改文件

| 文件 | 修改 |
|------|------|
| `include/chstream_register.hh` | 加 `REGISTER_CHSTREAM(DGpuBoardTLM)` + `UsrLinuxEmuIoctlStub` |

### 3. 复用(不修改)

- `include/tlm/gpu/dgpu_bar.hh`(v0.4 已实施)
- `include/tlm/gpu/minimal_warp_scheduler_tlm.hh` / `scoreboard_tlm.hh` / `pipeline_tlm.hh` / `tensor_core_tlm.hh`(s1 已集成)

---

## Acceptance Gate(本 change 独立)

| Gate | Owner | 状态 | 验证方法 |
|------|-------|:---:|----------|
| **s2-G1** DGpuBoardTLM 8 组件编译通过 | CppTLM | ⏳ W3 | `cmake --build build` 通过 |
| **s2-G2** Doorbell strong-order 250-700ns 区间测试 PASS | CppTLM | ⏳ W3 | `ctest -R "test_doorbell_strong_order_mvp"` PASS |
| **s2-G3** SubmitQueue WDU 5 个单测 PASS | CppTLM | ⏳ W3 | `ctest -R "test_submit_queue_mvp"` 5 个文件 PASS |
| **s2-G4** UsrLinuxEmuIoctlStub 4 IOCTL 端到端 PASS | CppTLM | ⏳ W4 | `ctest -R "test_usrlxemu_ioctl_stub"` PASS(0x27/0x28-ENOSYS/0x29/0x01) |
| **s2-G5** DGpuBoardTLM 6 SECTION E2E 测试 PASS | CppTLM | ⏳ W4 | `ctest -R "test_dgpu_board_v1_mvp_from_config"` 6 SECTION PASS |
| **s2-G6** validate_topology 集成 PASS | CppTLM | ⏳ W4 | `cmake --build build --target validate_topology` PASS |

**最终验收(本 change 完成时)**:
- [ ] s2-G1 ~ s2-G6 全部 ✅
- [ ] **10 个测试文件 PASS**(per Phase L:1 Doorbell + 5 SQ + 1 E2E 6 SECTION + 1 IOCTL 4 IOCTL + 2 骨架测试 CP/TMU)
- [ ] **本 change 可独立 archive**

---

## Cross-Repo Coordination(本 change)

| 仓 | 跟踪载体 | 状态 |
|----|---------|:---:|
| **PTX-EMU** | 无新 API 需求(s1 已 pin) | ✅ 已 satisfied |
| **UsrLinuxEmu** | IOCTL 0x27/0x29/0x01 stub + 0x28 -ENOSYS(per Phase F-H.3) | 🟡 W3-4 stub 模式 |
| **TaskRunner** | `cuModuleLoadData` 解析 | ✅ 已 ship |

---

**Refs**:
- [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/proposal.md)
- [`design.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/design.md)
- [`tasks.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/tasks.md)
- [`../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)
- [`../../../docs/soc_arch/modules/dgpu-board.md`](../../../docs/soc_arch/modules/dgpu-board.md)
- [`../../../docs/soc_arch/modules/submit-queue.md`](../../../docs/soc_arch/modules/submit-queue.md)
- [`../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) (依赖)

---

**起草**: Sisyphus (2026-08-21,per Oracle ses_fe179d02 拆分建议)
**Owner**: CppTLM Team
**状态**: 📋 Proposed — 等 s1 archive + W3 启动后开始实施
