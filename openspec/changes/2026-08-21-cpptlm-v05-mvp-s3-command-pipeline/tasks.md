# cpptlm-v05-mvp-s3-command-pipeline: Tasks (W5-10)

> **配套**: [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/proposal.md) · [`design.md`](../2026-08-21-cpptlm-v05-mvp-s3-command-pipeline/design.md)
> **结构**: W5-10 任务清单 · **Owner**: CppTLM Team (Sisyphus)
> **依赖**: s1 + s2 必须已 archive
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D5

---

## W5 (2026-09-19 ~ 2026-09-25)

### T-s3-1: Pm4Decoder (填充实现,per Oracle ses_fe0b6e44 s2 骨架修复)

> **关键**: s2 已创建 `pm4_decoder_mvp.hh` 接口(纯虚 `Pm4DecoderInterface`)+ `command_processor_mvp.hh` 骨架;s3 填充实际实现。

**Acceptance**:
- [ ] 填充 `src/tlm/gpu/pm4_decoder_mvp.cc`(~200 LOC,头文件 s2 已创建)
- [ ] `Pm4Decoder` 继承 `Pm4DecoderInterface`(per s2 骨架)
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
- [ ] `set_decoder()` 注入到 CommandProcessor(替换 s2 no-op 骨架)
- [ ] `test/test_pm4_decoder_mvp.cc` 全部 PASS

**Commit**:
```bash
git commit -am "feat(pm4-decoder-mvp): fill NVIDIA method packet parsing (per Phase F-H.3 path 3)"
```

### T-s3-2: CommandProcessor (填充实现,per s2 骨架)

**Acceptance**:
- [ ] 填充 `src/tlm/gpu/command_processor_mvp.cc`(.cc,头文件 s2 已创建,~150 LOC 填充 + ~150 LOC 已有骨架)
- [ ] 5-state FSM: IDLE → FETCH → DECODE → DISPATCH → COMPLETE
- [ ] **FETCH**:`mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry))`(per Phase F-C.3 H1)
- [ ] DECODE 调 `pm4_decoder_->parse_method()`(通过 s2 的 `set_decoder` 注入,非直接构造)
- [ ] **DISPATCH**:`tmu_.submit(Pm4MethodDispatch)`(替代 `record`)
- [ ] `test/test_command_processor_mvp.cc` 5 transition + NVIDIA method packet decode PASS(替代 s2 no-op 骨架测试)
- [ ] `test/test_pm4_decoder_mvp_integration.cc` CP + Decoder 集成 PASS

**Commit**:
```bash
git commit -am "feat(command-processor-mvp): fill 5-state FSM (NVIDIA method packet decode)"
```

## W6 (2026-09-26 ~ 2026-10-02)

### T-s3-3: TmuDispatchProcessor (填充实现,per s2 骨架 + Phase F-D.2 H5 + Oracle M4)

**Acceptance**:
- [ ] 填充 `src/tlm/gpu/tmu_dispatch_processor_mvp.cc`(.cc,头文件 s2 已创建,~250 LOC 填充)
- [ ] `TmuDispatchRecord` 9 字段
- [ ] `PreExitPolicy` 枚举: NONE 档(MVP)
- [ ] `inflight_kernel_reqs_` 32 slot + **反压停 fetch**(`BACKPRESSURED` 替代 LIFO)
- [ ] `TmuSubmitResult` 枚举: SUBMITTED / **BACKPRESSURED** / DEP_LATCH_MISMATCH / **SUBMIT_QUEUE_REJECTED** / **INTERNAL_ERROR**(per Phase F-H.4 + Oracle M4 新增 INTERNAL_ERROR)
- [ ] **TmuHandlerInterface 扩展**(per Oracle M4):`on_dispatch() → TmuHandlerResult`(原 void 改返回 result,SQ_REJECTED 需上报)
- [ ] `TmuHandlerResult` 枚举: HANDLED / SQ_REJECTED / INVALID_RECORD(per Oracle M4)
- [ ] 依赖锁存器 `wait_on_latch_id ↔ arrive_at_latch_id` 匹配检查
- [ ] pre-dispatch 3 段条件检查
- [ ] **依赖**:`CompletionRing& cq_`(per Phase F-H.4,**不**直接调 CudaCore);**派发路径经 handler 注入**(per s2 T-s2-3b + Oracle M4):handler 在 on_dispatch 内部调 SubmitQueue,TMU 不直持 SQ 引用;handler 返回 SQ_REJECTED → TMU 上报 SUBMIT_QUEUE_REJECTED
- [ ] **CP 退避策略**(per Oracle M4):收到 BACKPRESSURED / SUBMIT_QUEUE_REJECTED → 退避窗口 8 cycles + 状态 DECODE → FETCH(非 IDLE);超阈值进入 DEGRADED 状态
- [ ] `set_handler()` 注入到 TmuDispatchProcessor(替换 s2 no-op 骨架,`TmuHandlerInterface`)
- [ ] S3SubmitQueueHandler 类(per design §4.3):handler 内部 `sq_.enqueue(cta_desc)` + 返回 HANDLED/SQ_REJECTED
- [ ] `test/test_tmu_dispatch_processor_mvp.cc` ~10 测试 PASS(替代 s2 骨架的反压测试,per design §4.5):
  - submit / on_complete / BACKPRESSURED / dep chain / 环检测
  - **新增**:SQ_REJECTED 路径(handler 探测 SQ 满)
  - **新增**:INTERNAL_ERROR 路径(handler 返回 INVALID_RECORD)
  - **新增**:CP 退避窗口测试(连续 3 次 BACKPRESSURED → DEGRADED)

**Commit**:
```bash
git commit -am "feat(tmu-dispatch-mvp): fill dep chain + backpressure (per Phase F-D.2 H5 + Oracle M4)"
```

## W7-9 (2026-10-03 ~ 2026-10-23)

### T-s3-4: validate_topology 集成 + 全量 baseline

**Acceptance**:
- [ ] `validate_topology` CMake target 集成(可能已在 s2 完成)
- [ ] 全部 ≥850 测试 PASS(baseline 817 per AGENTS.md 2026-08-24 + s1 已 12 + d1 7-9 + s2 已 9 + s3 预计 ≥5)
- [ ] `build/bin/cpptlm_tests` 全部 PASS
- [ ] 无 regression

**Commit**:
```bash
git commit -am "build: integrate validate_topology target for dgpu_board_v1_mvp.json"
```

## W10 (2026-10-24 ~ 2026-10-30)

### T-s3-5: CHANGELOG + v0.5.0-MVP tag

**Acceptance**:
- [ ] **`CHANGELOG.md` 记录 v0.5.0-MVP release**(per Oracle m4 + CHANGELOG.md 已确立的格式约定):
  - 顶部新增 `## [v0.5.0-MVP] - 2026-10-30`(per T-s3-5 W10 截止)
  - 子章节按现有约定:`### 新增 (Features)` / `### 修复 (Fixes)` / `### 测试 (Tests)`
  - MVP 标注:在标题后或注释中标注 **MVP slice — pre-release, 不保证 GA**(per Oracle m3)
  - 每个 bullet 用 `**Component::method**` 格式,s2 + s3 累计 ~10 组件
- [ ] **版本号论证(per Oracle m3)**:v0.5.0-MVP 直接从 v2.4.1 跳号而非 v2.5.0,**MVP 视为 feature branch 命名而非 semver 顺序** — 与 v3.0.0 dGPU extract (per ADR-X.15) 并列,均代表 dGPU 演进的分支版本。**正式 GA 应为 v0.5.0**(MVP 验证 + bugfix 之后,M11+ 规划)
- [ ] `docs/soc_arch/modules/README.md` 同步 7 模块(6 + SubmitQueue)
- [ ] `scripts/test/docs_sync_check.sh --strict` PASS
- [ ] `git tag -a v0.5.0-MVP -m "..."`(tag annotation 引用此 change proposal.md 路径)

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
| R5 | 850 测试达不到 | 中 | 中 | baseline 817 + s1 12 + s2 9 + s3 5 + d1 7-9 = 850-852(per Oracle M1) |
| R6 | s1/s2 失败拖累 s3 | 中 | 中 | s3 仅在 s1+s2 archive 后启动,失败 blast radius 已隔离 |

---

## 验收检查表

最终 v0.5.0-MVP tag 前:
- [ ] T-s3-1 ~ T-s3-5 完成
- [ ] ≥850 测试 PASS
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

---

## Oracle Review Status Update (2026-08-26)

Per Oracle review (2026-08-26, s2/s3 联合审查),以下 corrections 已应用(3 个 commit):

| Finding | Commit | 修复位置 |
|---------|--------|---------|
| **C1** (CRITICAL) s1 archive 元数据腐烂 | `bec31a6` | 重建 s1 archive 目录 from `b68abe6^` |
| **M1** baseline 数字过期(764→817) | `2c2d55a` | s3 proposal.md + design.md + tasks.md |
| **M2** cuda_core_.tick() 接口归属 | `2c2d55a` | s2 design.md L39 + s3 design.md L31 |
| **M3** s2 E2E SECTION 数(6→5+1 deferred) | `2c2d55a` | s2 proposal.md + tasks.md + spec |
| **M4** TMU 反压传播路径细化 | `0775c78` | s3 design.md §4 (4.1-4.6) + tasks.md T-s3-3 |
| **m3+m4** 跳号论证 + CHANGELOG 规范 | `this commit` | T-s3-5 acceptance |
| **m5** WIP git commits 备忘 | `this commit` | 本节 |

**未修改**: ADR-SOC-06 (immutable,仅追加 Status Update 段);s2/s3 proposals 中 `📋 Proposed` 状态(openspec list 的 in-progress 由工具根据 tasks checkboxes 自动计算,非 proposal 文本)。

**WIP commit 备忘**: git 自动产生的 `2ec944a WIP on ...` + `3f62d4a index on ...` 是 worktree 切换 checkpoint,**无害**,已合并 s1 主分支。彻底清理需 `git rebase --interactive --root`,属 cosmetic 不在 s2/s3 scope。
