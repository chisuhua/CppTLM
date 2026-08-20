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
- [ ] `test/test_doorbell_strong_order_mvp.cc`:latency 区间 + 同 stream 顺序 PASS

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
  - `test/test_submit_queue_mvp_route.cc`
  - `test/test_submit_queue_mvp_enqueue.cc`
  - `test/test_submit_queue_mvp_dispatch.cc`
  - `test/test_submit_queue_mvp_complete.cc`
  - `test/test_submit_queue_mvp_concurrent.cc`

**Commit**:
```bash
git commit -am "feat(submit-queue-mvp): WDU distribution network (single-SM, per Phase F-H.5)"
```

### T-s2-3a: CommandProcessor 骨架(per Oracle ses_fe0b6e44 修复 CRITICAL s2 逆依赖 s3)

> **关键**: s2 W3-4 须创建 CP/TMU 类骨架(头文件 + stub 实现),使 DGpuBoardTLM 可独立编译;s3 W5-6 填充数据面(NVIDIA method packet + 反压停 fetch)。

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/command_processor_mvp.hh` + `src/tlm/gpu/command_processor_mvp.cc`(~150 LOC 骨架,~30 LOC 实现)
- [ ] **接口契约**:
  ```cpp
  class CommandProcessor {
  public:
      enum class State { IDLE, FETCH, DECODE, DISPATCH, COMPLETE };
      State state() const { return state_; }
      void wake();  // 供 DGpuBoardTLM 在 Doorbell ring 后调
      void tick();  // 5-state FSM 主入口
      void set_decoder(std::unique_ptr<Pm4DecoderInterface> decoder);  // s3 注入实现
  };
  ```
- [ ] **骨架实现**(W3-4 即可编译): tick() 内**默认 no-op**(状态机转换但不实际 fetch/decode);提供 `set_decoder` 接口供 s3 注入
- [ ] `include/tlm/gpu/pm4_types_mvp.hh`(数据头,~30 LOC,per Phase L round 3 Oracle 补):
  ```cpp
  struct Pm4MethodHeader {
      uint32_t inc : 1;            // bit 0
      uint32_t method_addr : 15;   // bits 1-15
      uint32_t subchannel : 4;     // bits 16-19
      uint32_t data_count : 4;     // bits 20-23
      uint32_t reserved : 8;       // bits 24-31
  };
  struct Pm4MethodDispatch {
      uint16_t method_addr;
      uint8_t subchannel_id;
      uint8_t data_count;
      // ... decoded fields: grid, block, shared_mem, args_vram_addr
  };
  ```
- [ ] `include/tlm/gpu/pm4_decoder_mvp.hh`(纯接口头,s3 可扩展加具体类):
  ```cpp
  #include "tlm/gpu/pm4_types_mvp.hh"  // s2 定义 Pm4MethodHeader/Pm4MethodDispatch

  class Pm4DecoderInterface {
  public:
      virtual ~Pm4DecoderInterface() = default;
      // s3 填充:4 method_addr ranges NVIDIA method packet 解析
      // (per Phase F-H.3,真相源 UsrLinuxEmu gpfifo_translator.h:60-73 unpackPm4Header)
      virtual Pm4MethodDispatch parse_method(
          uint32_t method_header,
          const uint32_t* payload,
          uint32_t max_dwords) = 0;
  };

  // s3 可选添加具体类(若 s2 头文件已含可省略):
  // class Pm4Decoder : public Pm4DecoderInterface {
  //     Pm4MethodDispatch parse_method(...) override { ... }
  // };
  ```
- [ ] 6 SECTION E2E 第 3 项"H2D 写 VRAM → install_kernel_module 返回 0"在 s2 W4 即可 PASS(CP 骨架 no-op,install_kernel_module 直接调 facade)
- [ ] 6 SECTION E2E 第 4 项"IOCTL 0x01 pushbuffer → CQ 收到 N entry"**在 s2 W4 仍 FAIL**(待 s3 填充 CP decoder 后);s2 E2E 降级为 5 SECTION + "CP-pending" 标记
- [ ] `test/test_command_processor_mvp_skeleton.cc`:CP 状态机骨架测试 PASS(5 state 转换 no-op + wake)

**Commit**:
```bash
git commit -am "feat(command-processor-mvp): skeleton (5-state FSM stub, per Oracle s2-dep fix)"
```

### T-s2-3b: TmuDispatchProcessor 骨架(per Oracle 修复)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/tmu_dispatch_processor_mvp.hh` + `src/tlm/gpu/tmu_dispatch_processor_mvp.cc`(~150 LOC 骨架)
- [ ] **接口契约**:
  ```cpp
  class TmuDispatchProcessor {
  public:
      TmuSubmitResult submit(TmuDispatchRecord record, uint32_t* out_evicted = nullptr);
      void on_complete(uint32_t task_id, int32_t status);
      void try_chain_dependent(const TmuDispatchRecord& completed_record);
      size_t inflight_count() const;
      // s3 填充
      void set_handler(std::unique_ptr<TmuHandlerInterface> handler);  // s3 注入
  };
  ```
- [ ] **骨架实现**(W3-4 即可编译): submit() 在容量满时返回 `BACKPRESSURED`(反压,不驱逐);on_complete() / try_chain_dependent() 骨架 no-op;`set_handler` 注入
- [ ] `include/tlm/gpu/tmu_handler_mvp.hh`(纯接口头,15 LOC):
  ```cpp
  class TmuHandlerInterface {
  public:
      virtual ~TmuHandlerInterface() = default;
      virtual void on_dispatch(const TmuDispatchRecord& record) = 0;  // s3 填充→调 SubmitQueue
  };
  ```
- [ ] `test/test_tmu_dispatch_processor_mvp_skeleton.cc`:反压 + 容量管理测试 PASS

**Commit**:
```bash
git commit -am "feat(tmu-dispatch-mvp): skeleton (backpressure stub, per Oracle s2-dep fix)"
```

### T-s2-3: DGpuBoardTLM(8 组件包装,per Phase F-H.2 修订)

**Acceptance**:
- [ ] 新建 `include/tlm/gpu/dgpu_board_mvp.hh` + `src/tlm/gpu/dgpu_board_mvp.cc`(~500 LOC)
- [ ] 内部 **8 组件**(全部 s2 内部成员):
  - `DGpuBar` + `Doorbell` + `CommandProcessor`(s2 骨架)+ `TmuDispatchProcessor`(s2 骨架)+ `SubmitQueue`(s2 新增) + `CompletionRing` + `CudaCoreAdapter`(s1) + `PtxEmuSubmoduleMVP`(s1)
- [ ] `tick()` 串联 4 阶段:`cp_.tick() → tmu_.tick() → sq_.tick() → cuda_core_.tick()`(per Phase F-H.2 §4)
- [ ] 入口方法:`install_kernel_module(vram_addr, size)` + `submit_kernel(req)` + `write_reg(offset, value)`
- [ ] `include/chstream_register.hh` 追加 `REGISTER_CHSTREAM(DGpuBoardTLM)`
- [ ] **6 SECTION E2E 降级验收**(s2 W4 可达):
  1. `validate_topology` 通过该 JSON
  2. `instantiateAll` 返回 true,`getInstance("dgpu_board0")` 存在
  3. H2D: 写 VRAM → `install_kernel_module` 返回 0 + image_handle ≠ 0 ✅
  4. Launch(0x01 pushbuffer)→ `cp_.tick()` no-op + CQ 收到 N entry(因 s3 CP decoder 未填充,**此处可能 N=0 — 在 s3 完成后才 N=N)⏳
  5. host_notify 触发 ≥1 次 ✅
  6. 负面: `ptx_emu_root` 指向不存在路径 → `instantiateAll` 失败 ✅
- [ ] **W3-4 可独立 archive**(CP/TMU 骨架编译通过 + 5 SECTION PASS + 第 4 项标注"待 s3 填充")

**Commit**:
```bash
git commit -am "feat(dgpu-board-mvp): DGpuBoardTLM ChStreamModuleBase with 8 components (per Phase F-H.2 + s2-dep fix)"
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
- [ ] `test/test_usrlxemu_ioctl_stub.cc`:**4 IOCTL** PASS(含 0x28 -ENOSYS 验证 + driver fallback 路径)

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
| R1 | 8 组件接口签名对齐 | 中 | 中 | s1 冻结接口约束 + s2 单元测试覆盖 |
| R2 | 4 IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R3 | SubmitQueue 反压链路 | 中 | 中 | 容量满 → 拒绝不驱逐 + CP backoff |
| R4 | Doorbell strong-order 延迟违反 | 中 | 中 | 测试断言 250-700ns 区间 |
| R5 | `ptx_emu_root` 注入失败(JSON config 负面场景)| 中 | 低 | 6 SECTION 第 6 项专门验证 |

---

## 验收检查表

最终 s2 archive 前:
- [ ] T-s2-1 ~ T-s2-5 完成
- [ ] **10 个测试文件 PASS**(per Phase L:1 Doorbell + 5 SQ + 1 E2E 6 SECTION + 1 IOCTL 4 IOCTL + 2 骨架测试 CP/TMU)
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
