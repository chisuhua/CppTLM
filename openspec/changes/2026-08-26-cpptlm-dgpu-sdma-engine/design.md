# cpptlm-dgpu-sdma-engine: Design

> **配套**: [`proposal.md`](./proposal.md) · [`tasks.md`](./tasks.md)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D3

> **字母代号约定**（与姊妹 changes 共享，本 change 内部不另设字母）：
> - **change A** = `cpptlm-dgpu-pcie-endpoint`（共享 `pcie_bundles_tlm.hh`）
> - **change B** = 本 change（`cpptlm-dgpu-sdma-engine`，自建 `dma_bundles_tlm.hh`）
> - **change C** = `cpptlm-dgpu-board-soc-split`（CP fetch 接线由 C 完成）
> - **change D** = `cpptlm-dgpu-abi-export`（SHARED 库暴露 23 ABI，本 change 的 host_out 翻译回调由 D 实现）

## 1. 组件定位

`SdmaEngineTLM` 是 SOC 片内的 PCIe **master** 引擎（对应真实 GPU 的 SDMA/copy engine）。它发起 upstream 事务访问 host 内存，访问 SOC VRAM 走内部 memory 端口。与 `PcieEndpointTLM`（slave，host→device）方向相反、职责互补。

```text
CP (GPFIFO fetch / 数据搬运请求)
    │ desc_in (DmaDescriptorBundle)
    ▼
SdmaEngineTLM
    ├─► mem_out/in ──► MemoryCluster (SOC VRAM)
    ├─► host_out ──► DGpuBoard ──► cpptlm_dma_translate_cb ──► 系统 IOMMU ──► host PA
    └─► done_out ──► CompletionRing (完成/错误)
```

## 2. Bundle 定义（`include/bundles/dma_bundles_tlm.hh`）

```cpp
namespace bundles {

// SdmaEngineTLM 接收的 DMA 描述符
struct DmaDescriptorBundle {
    enum class Dir : uint8_t { H2D, D2H };
    Dir      dir = Dir::H2D;
    uint64_t host_iova = 0;    // host 侧 IOVA（经 IOMMU translate）
    uint64_t vram_offset = 0;  // SOC VRAM 内偏移
    uint32_t size = 0;         // 字节数
    uint32_t tag = 0;          // 完成关联
};

// 完成通知（done_out）
struct CompletionBundle {
    uint32_t task_id = 0;
    int32_t  status = 0;        // 0=ok; <0=error (per errno)
    uint32_t tag = 0;
};

// 所有权声明：本 change 是 `bundles::CompletionBundle` 的唯一所有者。
// board-soc-split change (T-bs-2) 必须复用本类型，不得在其
// `dgpu_bundles_tlm.hh` 中重复定义；如需字段扩展，先在本文件改并通知 board-soc-split 跟进。

} // namespace bundles
```

**范围冻结**：`dma_bundles_tlm.hh` 由本 change 独立创建，**不修改** `include/bundles/pcie_bundles_tlm.hh`（change A 交付）。按能力域分文件，与 `cache/noc/compute_bundles_tlm.hh` 目录惯例一致。两 change 实施时此文件互不干扰，git merge 零冲突。

**内部 VRAM bundle 注**：本 MVP 选择 `PcieTlpBundle` 同时承担外部 PCIe TLP（`host_out`）与内部 VRAM 访问（`mem_in`/`mem_out`），承认二者语义域不同（外部 TLP vs 内部 memory）。这是 MVP 权宜——未来若引入专用 `MemoryReqBundle`（addr+size+data），可彻底解耦。当前 spec 冻结 `PcieTlpBundle::Kind::MEM_READ/MEM_WRITE` 字段语义在内部 VRAM 访问下与外部一致（字节数、offset 即 VRAM 内偏移）。

`PcieTlpBundle` 复用（来自 `pcie_bundles_tlm.hh`），用于 `mem_in/mem_out`（VRAM 访问）与 `host_out`（descriptor-only upstream，bulk 数据走 backdoor）。`DmaDescriptorBundle` 与 `PcieTlpBundle` **语义域不同**：前者是"内存拷贝描述符"，后者是"PCIe TLP 事务"。

## 2.5 Port index ordering lock（cross-change invariant）

`set_stream_adapter(StreamAdapterBase* adapters[])` 索引顺序在本 change 内**冻结**：
- 索引 0..k-1: ingress 端口，按声明序（`desc_in`=0, `mem_in`=1）
- 索引 k..n-1: egress 端口，按声明序（`mem_out`=2, `host_out`=3, `done_out`=4）

此约定与 `include/core/chstream_module.hh:37-47` 多端口 adapter 惯例一致；与配套的 `cpptlm-dgpu-pcie-endpoint` change 的 `PcieEndpointTLM`（4 端口，索引 0..3）独立维护但遵循同一规则。board-soc-split change 的 `DGpuBoard` JSON 必须按此索引顺序注入 `adapters[]`。

## 3. DMA 描述符 C++ 类型（`include/tlm/gpu/dma_descriptor_mvp.hh`）

```cpp
namespace tlm::gpu {

struct DmaDescriptor {
    enum class Dir : uint8_t { H2D, D2H };
    Dir      dir = Dir::H2D;
    uint64_t host_iova = 0;
    uint64_t vram_offset = 0;
    uint32_t size = 0;
    uint32_t tag = 0;
};

} // namespace tlm::gpu
```

组件 API 接受 `DmaDescriptor`（C++ 类型），内部转换为 `DmaDescriptorBundle`（POD bundle）经 `desc_in` 端口下发。

## 4. 处理流程

### H2D（host→device，如 driver H2D image upload / CP GPFIFO fetch）

1. `desc_in` 收到 `{dir=H2D, host_iova, vram_offset, size, tag}`
2. 经 `host_out` 发 upstream MEM_READ（Board shell 回调 `cpptlm_dma_translate_cb(iova,size,*phys)` → 读 host PA）
3. translate 失败 → 模拟 PCIe RequesterCompleterAbort → `done_out` 携带错误 + Board error 通道
4. 数据经 `mem_out` 写入 VRAM
5. `done_out` 发完成（tag）

### D2H（device→host，如完成回写 / 结果回收）

1. `desc_in` 收到 `{dir=D2H, ...}`
2. 经 `mem_in` 读 VRAM
3. 经 `host_out` 发 upstream MEM_WRITE（同样经 IOMMU translate）
4. `done_out` 发完成

## 5. 时序与反压

- 每 tick 处理至多 `max_inflight`（JSON 参数，默认 4）个描述符；
- translate callback 为同步调用（当前 ADR-088 §D3.8 签名），延迟以 `translate_latency` 参数注入；
- VRAM 端口反压经 StreamAdapter 自然传播（不驱逐，per s2 SubmitQueue 反压惯例）。

## 6. 错误路径

| 条件 | 行为 |
|------|------|
| translate cb 返回非 0 | RequesterCompleterAbort；`done_out` error；Board `cpptlm_error_cb_t`（DMA_TRANSLATION_FAULT） |
| vram_offset+size 越界 | 拒绝描述符，`done_out` error（-EINVAL 语义） |
| size == 0 | 拒绝（防御性） |

## 7. 测试策略

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_sdma_engine_h2d.cc` | `[sdma]` | H2D 全流程：desc → host read → VRAM write → done |
| `test_sdma_engine_d2h.cc` | `[sdma]` | D2H 全流程 |
| `test_sdma_engine_iommu_fault.cc` | `[sdma][error]` | fault → abort + error 通道 |
| `test_sdma_engine_from_config.cc` | `[sdma][json]` | JSON 实例化 + 5 端口注入 |

## 8. 风险与缓解

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | translate callback 同步签名将来扩展为异步（PASID/SVM） | 描述符携带 tag，完成机制已按异步设计；callback 签名变更走 ADR-088 BREAKING 流程 |
| R2 | 与 s3 CP fetch 的接线时序 | 本 change 不动 CP；change C 统一接线 |
| R3 | `max_inflight` 反压死锁 | 完成路径不依赖新描述符接收；测试覆盖满窗口场景 |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 待评审后实施
