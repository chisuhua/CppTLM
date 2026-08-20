# cpptlm-v05-mvp-s2-dgpu-board: Design

> **配套**: [`proposal.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/proposal.md) · [`tasks.md`](../2026-08-21-cpptlm-v05-mvp-s2-dgpu-board/tasks.md)
> **状态**: 📐 Design — 依赖 s1 (PtxEmuSubmoduleMVP + CudaCoreAdapter) · **Owner**: CppTLM Team
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md`](../../../docs/soc_arch/adr/ADR-SOC-06-cpptlm-v05-mvp.md) D4/D5

## 1. 架构概览(本 change)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                  DGpuBoardTLM (ChStreamModuleBase, 6 组件)               │
│                                                                              │
│  DGpuBar + Doorbell + CommandProcessor + TmuDispatchProcessor + SubmitQueue │
│  + CompletionRing + CudaCoreAdapter(从 s1)+ PtxEmuSubmoduleMVP(从 s1) │
│                                                                              │
│  tick() 串联 4 阶段(per Phase F-H.2):                                       │
│    cp_.tick() → tmu_.tick() → sq_.tick() → cuda_core_.tick()              │
│                                                                              │
│  ↔ UsrLinuxEmuIoctlStub(4 IOCTL,per Phase F-H.3)                          │
└─────────────────────────────────────────────────────────────────────────────┘
                              ↕ PCIe BAR0/1 + Doorbell ring
┌─────────────────────────────────────────────────────────────────────────────┐
│  UsrLinuxEmu driver                                                       │
│    cuModuleLoadData → ioctl(0x27) → DGpuBoardTLM::install_kernel_module   │
│    cuLaunchKernel → ioctl(0x01) → pushbuffer ring → CP fetch             │
│                              → ioctl(0x28) → 永久 -ENOSYS(per ADR-090)│
│    cuModuleUnload → ioctl(0x29) → DGpuBoardTLM::uninstall_kernel_module  │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 2. DGpuBoardTLM 接口(per Phase F-H.2 + Phase I.2 修订)

```cpp
class DGpuBoardTLM : public ChStreamModuleBase {
public:
    void install_kernel_module(const uint8_t* image_bytes, size_t size);  // 0x27
    int32_t submit_kernel(const KernelLaunchRequest& req);                // UMD shim 简化路径
    void write_reg(uint32_t offset, uint32_t value);                      // Doorbell ring trigger
    void tick();  // cp_→ tmu_→ sq_→ cuda_core_ 4 阶段

    // 6 私有成员(s1 提供的 + s2 新增的):
    DGpuBar bar_;
    Doorbell doorbell_;
    CommandProcessor cp_;
    TmuDispatchProcessor tmu_;
    SubmitQueue sq_;               // 🆕 s2 新增(WDU 分发网络)
    CompletionRing cq_;
    CudaCoreAdapter cuda_core_;    // s1
    PtxEmuSubmoduleMVP ptx_emu_;   // s1
};
```

## 3. SubmitQueue WDU 分发网络(per Phase F-H.5)

**核心组件**: per `docs/research/WDUtoSM/overview.md` NVIDIA Hopper WDU + Work Distribution Crossbar 简化版
- `select_target_core(cta_desc) → uint8_t`:MVP 固定返回 0(单 SM 路由);v0.5 完整版升级为 crossbar 逐周期仲裁
- `per-cluster pending FIFO`:32 槽,容量满拒绝不驱逐
- `per-core active 槽`:4 槽
- `CtaDescriptor { task_id, vram_image_addr, grid_xyz, block_xyz, shared_mem_bytes, ... }`

## 4. 4 IOCTL stub(per Phase F-H.3)

| IOCTL | handler | CppTLM 端 |
|-------|---------|-----------|
| 0x27 `LOAD_KERNEL_MODULE` | HAL #66 → H2D DMA | `install_kernel_module()`(真实工作) |
| 0x28 `LAUNCH_KERNEL_MODULE` | **永久 -ENOSYS**(per ADR-090 §D2.2) | 验证 handler 返回 -ENOSYS,driver fallback 至 0x01 |
| 0x29 `UNLOAD_KERNEL_MODULE` | 走 FREE_BO 路径 | `uninstall_kernel_module(vram_addr)`(真实工作) |
| 0x01 `PUSHBUFFER_SUBMIT_BATCH` | 写 gpfifo_entries[] → DGpuBar.vram.pushbuffer_ring | CP::tick() fetch + decode + dispatch(per s3) |

## 5. 测试策略(per Phase F-H.3 + F-H.5 + Phase I.2)

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_doorbell_strong_order_mvp.cc` | `[doorbell][mvp][strong-order]` | 250-700ns 区间 + 同 stream 顺序 |
| `test_submit_queue_mvp_route.cc` | `[submit-queue][mvp][route]` | `select_target_core` 单 SM 路由 |
| `test_submit_queue_mvp_enqueue.cc` | `[submit-queue][mvp][enqueue]` | enqueue + pending 满拒绝 |
| `test_submit_queue_mvp_dispatch.cc` | `[submit-queue][mvp][dispatch]` | tick() 派发到 active 槽满 |
| `test_submit_queue_mvp_complete.cc` | `[submit-queue][mvp][complete]` | on_warp_complete → SQ → TMU |
| `test_submit_queue_mvp_concurrent.cc` | `[submit-queue][mvp][concurrent]` | 多 CTA 并发 |
| `test_dgpu_board_v1_mvp_from_config.cc` | `[dgpu-board][mvp][e2e]` | 6 SECTION E2E |
| `test_usrlxemu_ioctl_stub.cc` | `[usrlxemu-ioctl][stub]` | **4 IOCTL** PASS |

## 6. 阶段化交付(本 change)

```
s2-W3 (2026-09-05~11): DGpuBoardTLM 6 组件 + Doorbell + SubmitQueue + CompletionRing
s2-W4 (2026-09-12~18): 4 IOCTL stub + JSON config + validate_topology + 8 个测试 PASS + archive
```

## 7. 风险与缓解(本 change)

| ID | 风险 | 概率 | 影响 | 缓解 |
|----|------|:---:|:---:|------|
| R1 | 6 组件接口签名对齐 | 中 | 中 | s1 冻结接口约束 + s2 单元测试覆盖 |
| R2 | 4 IOCTL stub 与真实 IOCTL 行为偏差 | 中 | 中 | stub 严格遵循 `gpu_ioctl.h` 真实结构 |
| R3 | SubmitQueue 反压链路 | 中 | 中 | 容量满 → 拒绝不驱逐 + CP backoff |
| R4 | Doorbell strong-order 延迟违反 | 中 | 中 | 测试断言 250-700ns 区间(per `docs/research/PCIe/PCIe_上的保序write.md` §4) |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 依赖 s1 (per Oracle 拆分)
**下次更新**: W3 s2 启动时
