# cpptlm-v05-mvp-s2-dgpu-board: Tasks (W3-4)

> **配套**: [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/proposal.md) · [`design.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/design.md)
> **结构**: W3-4 任务清单 · **Owner**: CppTLM Team (Sisyphus)
> **依赖**: s1 必须已 archive(PtxEmuSubmoduleMVP + CudaCoreAdapter 已实现并测试通过)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D4/D5

---

## W3 (2026-09-05 ~ 2026-09-11)

### T-s2-1: Doorbell + CompletionRing

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/doorbell_mvp.hh` + `.cc`(SQ tail register + strong-order write,per `docs/research/PCIe/PCIe_上的保序write.md` §4 250-700ns 区间)
- [ ] 新建 `include/tlm/gpu/completion_ring_mvp.hh` + `.cc`(push + host_notify 重设计)
- [ ] `tests/test_doorbell_strong_order_mvp.cc`:latency 区间 + 同 stream 顺序 PASS

**Commit**:
```bash
git commit -am "feat(doorbell-mvp): SQ tail register with strong-order write path (250-700ns)"
git commit -am "feat(completion-ring-mvp): push + host_notify hook"
```

### T-s2-2: SubmitQueue(🆕 WDU 分发网络,per Phase F-H.5)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/submit_queue_mvp.hh` + `.cc`(~150 LOC,per `docs/research/WDUtoSM/overview.md` NVIDIA Hopper)
- [ ] `SubmitQueue::enqueue(cta_desc) → bool`(per-cluster pending FIFO 32 槽)
- [ ] `SubmitQueue::tick()` 派发(per-core active 4 槽)
- [ ] `SubmitQueue::on_warp_complete(task_id, status)` 反向流
- [ ] `select_target_core(cta_desc) → uint8_t`:MVP 固定返回 0
- [ ] **5 个单测**全部 PASS:
  - `tests/test_submit_queue_mvp_route.cc`
  - `tests/test_submit_queue_mvp_enqueue.cc`
  - `tests/test_submit_queue_mvp_dispatch.cc`
  - `tests/test_submit_queue_mvp_complete.cc`
  - `tests/test_submit_queue_mvp_concurrent.cc`

**Commit**:
```bash
git commit -am "feat(submit-queue-mvp): WDU distribution network (single-SM, per Phase F-H.5)"
```

### T-s2-3: DGpuBoardTLM(**6 组件**包装,per Phase F-H.2)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc`(~500 LOC)
- [ ] 内部 6 组件(从 s1 拿 PtxEmuSubmoduleMVP + CudaCoreAdapter,s2 新增 SubmitQueue):
  - `DGpuBar` + `Doorbell` + `CommandProcessor` + `TmuDispatchProcessor` + **`SubmitQueue`** + `CompletionRing` + `CudaCoreAdapter`(s1) + `PtxEmuSubmoduleMVP`(s1)
- [ ] `tick()` 串联 4 阶段:`cp_.tick() → tmu_.tick() → sq_.tick() → cuda_core_.tick()`(per Phase F-H.2 §4)
- [ ] 入口方法:`install_kernel_module(vram_addr, size)` + `submit_kernel(req)` + `write_reg(offset, value)`
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(DGpuBoardTLM)`

**Commit**:
```bash
git commit -am "feat(dgpu-board-mvp): DGpuBoardTLM ChStreamModuleBase with 8 components (per Phase F-H.2)"
```

## W4 (2026-09-12 ~ 2026-09-18)

### T-s2-4: UsrLinuxEmuIoctlStub(**4 IOCTL**,per Phase F-H.3)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/usrlxemu_ioctl_stub_mvp.hh` + `.cc`(~300 LOC)
- [ ] 实现 4 IOCTL(per Phase F-H.3):
  - 0x27 `LOAD_KERNEL_MODULE` → `install_kernel_module()`(真实工作,per UsrLinuxEmu ADR-090 §D2.1)
  - **0x28 `LAUNCH_KERNEL_MODULE` → 永久 -ENOSYS**(per UsrLinuxEmu ADR-090 §D2.2 + ADR-023 §D4 append-only 治理)
  - 0x29 `UNLOAD_KERNEL_MODULE` → 走 FREE_BO 路径 → `uninstall_kernel_module(vram_addr)`(真实工作)
  - **0x01 `PUSHBUFFER_SUBMIT_BATCH` → 写 gpfifo_entries[] 到 DGpuBar.vram.pushbuffer_ring + Doorbell ring**(per Phase F-H.3 真实 launch 入口)
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(UsrLinuxEmuIoctlStub)`
- [ ] `tests/test_usrlxemu_ioctl_stub.cc`:**4 IOCTL** PASS(含 0x28 -ENOSYS 验证 + driver fallback 路径)

**Commit**:
```bash
git commit -am "feat(usrlxemu-ioctl-stub): 4 IOCTL stub (0x27/0x28-ENOSYS/0x29/0x01) for Mode B dGPU board"
```

### T-s2-5: JSON config + validate_topology

**Acceptance**:
- [ ] 新建 `configs/dgpu_board_v1_mvp.json.in`(CMake configure_file 注入 `${PTX_EMU_ROOT}`)
- [ ] 1 个 `DGpuBoardTLM` 模块(`ptx_emu_root` 用 `${PTX_EMU_ROOT}` placeholder)
- [ ] 1 个 `MemoryTLM` 模块作为 H2D DMA + VRAM backing
- [ ] 1 个 `UsrLinuxEmuIoctlStub` 绑定 dgpu_board0
- [ ] 根 CMakeLists 配置 `configure_file(...)`
- [ ] 纳入 `validate_topology` CMake target 扫描
- [ ] 新建 `test/test_dgpu_board_v1_mvp_from_config.cc`(6 SECTION 验收)
- [ ] 6 SECTION E2E 验收(per ADR-SOC-06 G-MVP-2):
  1. `validate_topology` 通过该 JSON
  2. `instantiateAll` 返回 true,`getInstance("dgpu_board0")` 存在
  3. H2D: 写 VRAM → `install_kernel_module` 返回 0 + image_handle ≠ 0
  4. Launch: N=4 stream 各 1 次 IOCTL 0x01 pushbuffer → `eq.run(budget)` 内 CQ 收到 N 个 entry
  5. host_notify 触发 ≥1 次
  6. 负面: `ptx_emu_root` 指向不存在路径 → `instantiateAll` 失败

**Commit**:
```bash
git commit -am "feat(configs): dgpu_board_v1_mvp.json with validate_topology support"
git commit -am "test(dgpu-board-v1-mvp): 6 SECTION E2E + 4 IOCTL tests (per Phase F-H.3)"
```

---

## 风险登记(本 change 子集)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | 6 组件接口签名对齐 | 中 | 中 | s1 冻结接口约束 + s2 单元测试覆盖 |
| R2 | 4 IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R3 | SubmitQueue 反压链路 | 中 | 中 | 容量满 → 拒绝不驱逐 + CP backoff |
| R4 | Doorbell strong-order 延迟违反 | 中 | 中 | 测试断言 250-700ns 区间 |
| R5 | `ptx_emu_root` 注入失败(JSON config 负面场景)| 中 | 低 | 6 SECTION 第 6 项专门验证 |

---

## 验收检查表

最终 s2 archive 前:
- [ ] T-s2-1 ~ T-s2-5 完成
- [ ] 7+ 个测试 PASS(6 SECTION + 5 SQ + 4 IOCTL + 1 Doorbell)
- [ ] 编译防火墙仍 PASS(s1 验证基础 + s2 不破坏)
- [ ] docs 同步检查 PASS

---

**Refs**:
- [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/proposal.md)
- [`design.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/design.md)
- [`../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)
- [`../../docs/soc_arch/modules/dgpu-board.md`](../../../docs/soc_arch/modules/dgpu-board.md)
- [`../../docs/soc_arch/modules/submit-queue.md`](../../../docs/soc_arch/modules/submit-queue.md)
- [`../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) (依赖)

---

**起草**: Sisyphus (2026-08-21,per Oracle ses_fe179d02 拆分建议)
**Owner**: CppTLM Team
**状态**: 📋 Tasks — 等 s1 archive + W3 启动后开始实施
