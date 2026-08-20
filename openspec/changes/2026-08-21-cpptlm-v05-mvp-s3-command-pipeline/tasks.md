# cpptlm-v05-mvp-s3-command-pipeline: Tasks (W5-10)

> **配套**: [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/proposal.md) · [`design.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/design.md)
> **结构**: W5-10 任务清单 · **Owner**: CppTLM Team (Sisyphus)
> **依赖**: s1 + s2 必须已 archive
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5

---

## W5 (2026-09-19 ~ 2026-09-25)

### T-s3-1: Pm4Decoder (NVIDIA method packet,per Phase F-H.3 路径 3)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/pm4_decoder_mvp.hh` + `.cc`(~200 LOC)
- [ ] **`Pm4MethodHeader` 结构体**(per `unpackPm4Header`):
  ```cpp
  uint32_t inc : 1;            // bit 0
  uint32_t method_addr : 15;   // bits 1-15
  uint32_t subchannel : 4;     // bits 16-19
  uint32_t data_count : 4;     // bits 20-23
  uint32_t reserved : 8;       // bits 24-31
  ```
- [ ] **`Pm4MethodDispatch`**(替代原 `Pm4Packet`)
- [ ] `parse_method(method_header, payload, max_dwords) → Pm4MethodDispatch`
- [ ] 4 method_addr ranges: 0x4000-0x40FF DISPATCH_DIRECT / 0x4200-0x42FF EVENT_WRITE / 0x4400-0x44FF RELEASE_MEM / 0x4500-0x45FF ACQUIRE_MEM
- [ ] `tests/test_pm4_decoder_mvp.cc` 全部 PASS

**Commit**:
```bash
git commit -am "feat(pm4-decoder-mvp): NVIDIA method packet parsing (per Phase F-H.3 path 3)"
```

### T-s3-2: CommandProcessor (5-state FSM,per Phase F-H.3)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/command_processor_mvp.hh` + `.cc`(~250 LOC)
- [ ] 5-state FSM: IDLE → FETCH → DECODE → DISPATCH → COMPLETE
- [ ] **FETCH**:`mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry))`(per Phase F-C.3 H1)
- [ ] 内部调 `Pm4Decoder::parse_method`(替代 `parse_type3`)
- [ ] **DISPATCH**:`tmu_.submit(Pm4MethodDispatch)`(替代 `record`)
- [ ] `tests/test_command_processor_mvp.cc` 5 transition PASS
- [ ] `tests/test_pm4_decoder_mvp_integration.cc` CP + Decoder 集成 PASS

**Commit**:
```bash
git commit -am "feat(command-processor-mvp): 5-state FSM with Pm4Decoder integration (NVIDIA method packet)"
```

## W6 (2026-09-26 ~ 2026-10-02)

### T-s3-3: TmuDispatchProcessor (反压停 fetch,per Phase F-D.2 H5)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `.cc`(~250 LOC)
- [ ] `TmuDispatchRecord` 9 字段
- [ ] `PreExitPolicy` 枚举: NONE 档(MVP)
- [ ] `inflight_kernel_reqs_` 32 slot + **反压停 fetch**(`BACKPRESSURED` 替代 LIFO)
- [ ] `TmuSubmitResult` 枚举: SUBMITTED / **BACKPRESSURED** / DEP_LATCH_MISMATCH / **SUBMIT_QUEUE_REJECTED**(per Phase F-H.4)
- [ ] 依赖锁存器 `wait_on_latch_id ↔ arrive_at_latch_id` 匹配检查
- [ ] pre-dispatch 3 段条件检查
- [ ] **依赖**:`SubmitQueue& submit_queue_` + `CompletionRing& cq_`(per Phase F-H.4,不直接调 CudaCore)
- [ ] `tests/test_tmu_dispatch_processor_mvp.cc` ~10 测试 PASS:
  - submit / on_complete / BACKPRESSURED / dep chain / 环检测

**Commit**:
```bash
git commit -am "feat(tmu-dispatch-mvp): TMU Glue with dep chain + backpressure (32 slot, per Phase F-D.2 H5)"
```

## W7-9 (2026-10-03 ~ 2026-10-23)

### T-s3-4: validate_topology 集成 + 全量 baseline

**Acceptance**:
- [ ] `validate_topology` CMake target 集成(可能已在 s2 完成)
- [ ] 全部 ≥880 测试 PASS(v0.4.1 baseline 850 + MVP 新增 ≥30,per s1+s2+s3 累计 ≥50+)
- [ ] `build/bin/cpptlm_tests` 全部 PASS
- [ ] 无 regression

**Commit**:
```bash
git commit -am "build: integrate validate_topology target for dgpu_board_v1_mvp.json"
```

## W10 (2026-10-24 ~ 2026-10-30)

### T-s3-5: CHANGELOG + v0.5.0-MVP tag

**Acceptance**:
- [ ] `CHANGELOG.md` 记录 v0.5.0-MVP release
- [ ] `docs/soc_arch/modules/README.md` 同步 7 模块(6 + SubmitQueue)
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS
- [ ] `git tag -a v0.5.0-MVP -m "..."`

**Commit**:
```bash
git add CHANGELOG.md
git commit -m "docs(changelog): record v0.5.0-MVP release (MVP slice)"
git tag -a v0.5.0-MVP -m "cpptlm-v05-mvp: MVP slice - UsrLinuxEmu IOCTL → CP → TMU → SQ → CudaCore + PTX-EMU functional/timing split"
```

---

## 风险登记(本 change 子集)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | Pm4Decoder NVIDIA method packet 与 `unpackPm4Header` 不一致 | 中 | 中 | 单元测试 + 集成测试双重验证(per FIX-C2 教训) |
| R2 | CP 5-state FSM 状态转换遗漏 | 中 | 高 | TDD 5 transition 测试 |
| R3 | TMU 反压停 fetch 频繁触发 | 中 | 中 | 32 slot + JSON `tmu_max_active_tasks` 可配置 + BACKPRESSURED 后 CP 退避 |
| R4 | TMU dep 链式推进死循环 | 低 | 高 | 简化环检测(链深 ≤ 8) |
| R5 | 880 测试达不到 | 中 | 中 | s1 已 ≥12 + s2 已 ≥9 + s3 预计 ≥5 = ≥26 新增测试,baseline 850 + ≥30 = ≥880 |
| R6 | s1/s2 失败拖累 s3 | 中 | 中 | s3 仅在 s1+s2 archive 后启动,失败 blast radius 已隔离 |

---

## 验收检查表

最终 v0.5.0-MVP tag 前:
- [ ] T-s3-1 ~ T-s3-5 完成
- [ ] ≥880 测试 PASS
- [ ] 编译防火墙验证仍 PASS(s1+s2 已验证)
- [ ] docs 同步检查 PASS
- [ ] `git tag -a v0.5.0-MVP -m "..."`

---

**Refs**:
- [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/proposal.md)
- [`design.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/design.md)
- [`../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md)
- [`../../docs/soc_arch/modules/command-processor.md`](../../../docs/soc_arch/modules/command-processor.md)
- [`../../docs/soc_arch/modules/pm4-decoder.md`](../../../docs/soc_arch/modules/pm4-decoder.md)
- [`../../docs/soc_arch/modules/tmu-dispatch-processor.md`](../../../docs/soc_arch/modules/tmu-dispatch-processor.md)
- [`../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/`](../2026-08-21-cpptlm-v05-mvp-s1-ptxemu-integration/) (依赖)
- [`../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/) (依赖)

---

**起草**: Sisyphus (2026-08-21,per Oracle ses_fe179d02 拆分建议)
**Owner**: CppTLM Team
**状态**: 📋 Tasks — 等 s1+s2 archive + W5 启动后开始实施
