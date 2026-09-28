# D-AXI Design — Driver-Visible Minimal SoC 详细设计 (v1.7 — v1.3 P0 (B1-B6) + v1.4 架构根因 (B7-B13) + v1.5 隐藏缺陷 (B14-B28) + v1.6 架构锁定 (F1-F12) + 演进路线图 §X + 用户目标验证 §Y)

> **配套**: [proposal.md](proposal.md) · [tasks.md](tasks.md) · [specs/driver-visible-minimal-soc/spec.md](specs/driver-visible-minimal-soc/spec.md)
> **基于**: D1 v1.1.1 + D2 v1.1 + v1.2 P1 + v1.3 第三方审查 (Metis/Oracle/Librarian) 6 P0 修正
> **作废**: `2026-09-20-cpptlm-pcie-memory-device-mvp` (D2 v1.1, 已 archive)
> **修订**: 2026-09-26 — v1.3 P0 修正 (B1 字段名 + B2 切型范围 + B3 撤双 adapter + B4 改 mutex lazy + B5 64-bit BAR + B6 双注册)

---

## §1 系统拓扑（D-AXI v1.4 — 单一 VRAM backing 所有权归 DGpuBoard）

> **v1.5 关键变化**（在 v1.4 之上叠加 11 项隐藏缺陷修正）：
> - **B14 消灭 vram_segments_**（`dgpu_board_shell.cc:532-591, 796-814`）：第三个存储 → backdoor 改走 `vram_storage_` 唯一路径
> - **B15 强制删除 framebuffer_storage_**：`framebuffer_ptr_ = vram_storage_.get()`（消除二选一歧义）
> - **B16 attach_framebuffer_for_testing 优先级规则**：vram_storage_ 已分配时拒绝 attach（防测试绕开）
> - **B17 新增 `Packet::payload_resize()`** + `PacketPool::acquire_with_min_size()`（N1 框架修复）
> - **B18 `on_config_loaded` 真实接线 capacity_gb**（消除 1GB cap 谎言）
> - **B19 SDMA vram_size_bytes 由 board 注入**（与 vram_size_ 同步）
> - **B20 `set_translate_cb` + `set_sdma_engine` 无条件注入**（防 SDMA 静默挂起）
> - **B21 GMMU dummy `resp_out()` + `req_in()`**（对齐 MemoryTLM 模板契约）
> - **B22/B23 Fault Path 显式化**（SLVERR latch + translate 错误 emit done, 防永久挂死）
> - **B25 SDMA ↔ PcieMemoryDevice 统一为 PcieTlpBundle**（放弃 v1.3 B2 切型，框架限制）
> - **B26/B27/B28 backdoor bound + BAR0 简化 + MemoryTLM capacity 三方矛盾消解**

> **v1.4 关键变化**: 单一 VRAM 真源在 `DGpuBoard::vram_storage_`（8GB，`std::unique_ptr<uint8_t[]>` default-init, Linux lazy commit）；PcieMemoryDevice 持有**注入指针**（不拥有 backing）；5 消费者共享同一 vector：
> 1. DGpuBoard backdoor（v1.5 改走 vram_storage_ 唯一路径, 删 vram_segments_）
> 2. BAR1 fast-path（bound = `bar1_window_size_` = `bar_sizes[1]`，与 vram 解耦）
> 3. BAR2 via `ep->memory_device().memory_read` → 转发到 vram
> 4. MemoryTLM.backing_ptr_（CacheReqBundle 消费者; **v1.5 B12 接线 capacity_gb**）
> 5. SDMA/GMMU legacy `set_vram_backdoor/set_backing`（注入 size = `vram_size_`; **v1.5 B13 同步**）

```
┌─────────────────────────────────────────────────────────────┐
│             Host (UsrLinuxEmu)                              │
│   coherent/non-coherent 语义由 UE 端模拟(CMA/vmalloc/kmalloc) │
│            │                                                │
│            ▼                                                │
│   CpptlmBridge (15 ABI fn wrappers)                        │
│            ▼ (C ABI call)                                   │
├────────────────────────────────────────────────────────────┤
│                CppTLM (实现仓)                             │
│            ▼                                                │
│   DGpuBoard (顶层 board shell + host-side backdoor 特权)  │
│      ├─ vram_storage_ (8GB, **v1.4 B7 单一 VRAM 真源**)    │
│      │     ├─ framebuffer_ptr_ = vram_storage_.get() (**v1.5 B15 别名**, framebuffer_storage_ 删除) │
│      │     ├─ vram_size_        (= bar_sizes[2] = 8GB) │
│      │     ├─ bar1_window_size_ (= bar_sizes[1] = 16MB) │
│      │     └─ mmio_read/write BAR2 fast-path (bound=vram_size_) │
│      └─ soc_ (DGpuSoc)                                     │
│            ├─ pcie_ep  (PcieEndpointIP)                     │
│            │     ├─ memory_device_: PcieMemoryDevice*      │
│            │     │   (raw ptr, non-owning)                 │
│            │     ├─ config_space: vendor=0x10DE            │
│            │     │   device=0x1234 (PcieConfigSpace)       │
│            │     └─ **T1.4: 不再 tick memory_device_**    │
│            │                                               │
│            ├─ pcie_memory (PcieMemoryDevice,                │
│            │     ChStreamModuleBase, 2 SlavePorts,         │
│            │     **单 adapter_ + tick 一次 (v1.3 B3 撤销 N4)**) │
│            │     port0 ← sdma.mem_out (PORT_MEM_OUT)      │
│            │     port1 ← gmmu.req_out                      │
│            │     wire-format: AxiMemBundle (chip-internal)│
│            │     ├─ backing_ptr_ (注入式, 指向 board vram_storage_, **v1.4 B7**)│
│            │     │   └─ std::mutex backing_mutex_ (保留, **v1.4 B10**) │
│            │     └─ BAR2 暴露 (需 T0.4 EP BAR 64-bit 双 dword 实现)│
│            │     └─ BAR2 暴露 (需 T0.4 EP BAR 64-bit 双 dword 实现)│
│            │                                               │
│            ├─ sdma  (SdmaEngineTLM, 5 ports, AxiMemBundle)│
│            │     ├─ mem_out → pcie_memory.0               │
│            │     ├─ **resp 从 req_in[2] 消费 (N7)**       │
│            │     ├─ **tick 先 FIFO 重试 inflight_ (N3)** │
│            │     └─ set_vram_backdoor 保留 dual-mode       │
│            │                                               │
│            ├─ gmmu  (GmmuTLM, ChStreamModuleBase,          │
│            │     1 MasterPort, 异步状态机)                  │
│            │     ├─ req_out → pcie_memory.1               │
│            │     ├─ **COMPLETE/WAIT iova 匹配 (N2)**      │
│            │     └─ set_backing 保留 dual-mode            │
│            ├─ memory  (MemoryTLM, 不变)                    │
│            └─ completion (CompletionRingTLM, 不变)        │
│                                                             │
│   chip-internal AXI Connections (AxiMemBundle):            │
│     sdma.mem_out  → pcie_memory.req_in[0]                  │
│     gmmu.master   → pcie_memory.req_in[1]                  │
│     sdma.mem_in[2] (slot 2, PORT_MEM_OUT resp) ← pcie_memory.resp_out[0]
│     gmmu.resp_in   ← pcie_memory.resp_out[1]                │
│                                                             │
│   board-level (PcieTlpBundle, 不变):                       │
│     HostBypass/RC ↔ pcie_ep (4 端口冻结)                  │
└─────────────────────────────────────────────────────────────┘
```

## §2 关键设计决策（用户 5 澄清 + v1.2 N1-N12 + **v1.3 B1-B6 修正**）

| ID | 来源 | 落点 |
|----|------|------|
| U1 | 用户澄清 ① | Coherence UE 端职责；spec Requirement 显式声明 |
| U2 | 用户澄清 ② | AxiMemBundle 新建（design §3）；与 PcieTlpBundle 严格分离 |
| U3 | 用户澄清 ③ | BAR2 启用 + PTE 经 BAR2 写入 memory_backing_ |
| U4 | 用户澄清 ④ | device_id 来自 PcieConfigSpace（已源码验证） |
| U5 | 用户澄清 ⑤ | Oracle 修订应用 |
| **N1** | Oracle 二次 | **§3 AxiMemBundle 需框架 payload 扩容 2 行**；T0.2 经 StreamAdapter 真实 round-trip |
| **N2** | Oracle 二次 | **§5 GMMU COMPLETE/WAIT 加 pending_iova_ 匹配** |
| **N3** | Oracle 二次 | **§6 SDMA inflight retry driver 伪码** |
| **N4** | Oracle 二次 ~~（**v1.3 B3 撤销**）~~ | ~~§4 存 adapters_[2] + tick 双 adapter~~ → **§4 改为存单 `MultiPortStreamAdapter* adapter_`**（事实：module_factory.cc:695-705 对 multi-port 只注入单个 adapter；其内部 tick 已遍历全端口） |
| **N5** | Oracle 二次 | tasks T1.5/T3.x 显式列"既有测试机械迁移"任务 |
| **N6** | Oracle 二次 | tasks T0.4 立项 EP BAR 寄存器生成；~~v1.3 B5 修正为 64-bit 双 dword 编码~~ |
| **N7** | Oracle 二次 | SDMA resp 路径改为 slot-2 消费 |
| **N8** | Oracle 二次 | tasks T1.4 删除 EP memory_device_->tick() 转发 |
| N9 | Oracle 二次 | D2H/H2D host 侧数据由 host_backdoor 注入（spec 显式声明） |
| **N10** | Oracle 二次 ~~（**v1.3 B4 重设计**）~~ | ~~spec 要求 PcieMemoryDevice 构造时预分配 memory_backing_ (8GB) 防 race~~ → **§4 改为 lazy alloc + std::mutex 保护**（单线程 sim 无 race 前提；8GB 预分配导致测试 OOM；mutex 保证 future 线程扩展兼容） |
| N11 | Oracle 二次 | spec 改 "互不解引用" 而非 "销毁顺序保证" |
| N12 | Oracle 二次 | tasks T1.3 条件注入 (soc 无 pcie_memory 时保留 legacy) |
| **v1.3 B1** | Oracle/Metis 三方 | §6 SDMA 字段名: `dst_iova_offset`/`len` → **`host_iova`/`size`/`vram_offset`** (per `dma_descriptor_mvp.hh:44-48`) |
| **v1.3 B2** | Metis | §6 SDMA 切型范围限定: minimal_v1 仅切 `mem_in`/`mem_out` (chip-internal); `desc_in`/`done_out`/`host_out` 保持 PcieTlpBundle (minimal_v1 不接线 host↔board 端口) |
| **v1.3 B6** | Oracle/Metis | §4 注册段: PcieMemoryDevice + GmmuTLM **双注册** (`registerObject` + `registerMultiPortAdapter`/`registerAdapter`) |
| **v1.4 B7** | Oracle 二轮 | **§4 PcieMemoryDevice 持注入指针，不拥有 backing**；DGpuBoard `vram_storage_`（`unique_ptr<uint8_t[]>`, default-init, 8GB virtual-only, stable pointer）单一所有权 |
| **v1.4 B8** | Oracle 二轮 | **§4 `memory_read/write` bound = injected `backing_size_`**（非 `kDefaultMemSize`）；`kDefaultMemSize` 降级为 board 派生默认值 |
| **v1.4 B9** | Oracle 二轮 | **§4 `handle_slave_port` 检查 `memory_read/write` 返回值**：失败置 `resp.resp.write(1)` (SLVERR) |
| **v1.4 B10** | Metis 二轮 | **§4 保留 B4 的 mutex 部分**（去 lazy 保 mutex）：`sim_thread_` + host mmio 真实并发；lazy 部分随所有权上提消失 |
| **v1.4 B11** | Oracle 二轮 | **§1/§4 拆分 `bar1_window_size_` vs `vram_size_`**：BAR1 fast-path bound 用前者，5 消费者注入 size 用后者 |
| **v1.4 B12** | Oracle 二轮 | **§4 新增 `-ENODEV` 错误码**（未注入 backing）；`kRegMemSizeLo/Hi` 读 injected `backing_size_`（非常量） |
| **v1.4 B13** | Metis 二轮 | **tasks P0.7 24-case 处置表**：4 处语义反转（lazy alloc 测试删除，`has_memory_backing` 仍 false 但后续依赖注入）+ `[minimal_dgpu_soc]`/`[abi]` 添加 BAR2 |
| **v1.5 B14** | Oracle 三轮 + Metis 三轮 | **§1 消灭 vram_segments_ 双存储**：backdoor 改走 `vram_storage_` 唯一路径 |
| **v1.5 B15** | Oracle 三轮 + Metis 三轮 | **§1 强制删 framebuffer_storage_**：`framebuffer_ptr_ = vram_storage_.get()` |
| **v1.5 B16** | Oracle 三轮 | **§1 attach_framebuffer_for_testing 优先级规则**：vram_storage_ 已分配时拒绝 |
| **v1.5 B17** | Metis 三轮 | **§3 新增 `Packet::payload_resize()` + `PacketPool::acquire_with_min_size()`**：N1 框架修复 |
| **v1.5 B18** | Oracle 三轮 + Metis 三轮 | **§1/§13 MemoryTLM `on_config_loaded` 真实接线 capacity_gb**：消除 1GB cap 谎言 |
| **v1.5 B19** | Oracle 三轮 | **§4/§13 SDMA `vram_size_bytes` 由 board 注入**：与 vram_size_ 同步 |
| **v1.5 B20** | Metis 三轮 | **§4 `set_translate_cb` + `set_sdma_engine` 无条件注入**：防 SDMA 静默挂起 |
| **v1.5 B21** | Metis 三轮 | **§5 GMMU dummy `resp_out()` + `req_in()`**：对齐 MemoryTLM 模板契约 |
| **v1.5 B22/B23** | Oracle 三轮 | **§5/§6 Fault Path 显式化**：SLVERR latch + translate 错误 emit done（防永久挂死） |
| **v1.5 B25** | Metis 三轮 | **§6 SDMA ↔ PcieMemoryDevice 统一为 PcieTlpBundle**：放弃 v1.3 B2 切型（框架限制） |
| **v1.5 B26/B27/B28** | Oracle 三轮 | **§1 backdoor bound 统一 vram_size_**；**§4 BAR0 简化**；**§13 MemoryTLM capacity 三方矛盾消解** |

## §3 `AxiMemBundle` 定义（v1.2 P1 — 需框架配合）

```cpp
// include/bundles/axi_mem_bundles_tlm.hh
// AxiMemBundle: chip-internal AXI 存储事务载体（GMMU/SDMA ↔ PcieMemoryDevice）
// 严格分离于 board-level PcieTlpBundle。
//
// **N1 必须配合**: sizeof(AxiMemBundle)≈4136B > PacketPool kMinPayloadBytes=256B。
// 框架侧 OutputStreamAdapter::send / InputStreamAdapter::process 须按 sizeof(BundleT)
// 扩容 payload (2 行改动)，否则序列化失败 → 请求静默滞留。
// T0.2 经真实 StreamAdapter round-trip 测试锁定。
#ifndef BUNDLES_AXI_MEM_BUNDLES_TLM_HH
#define BUNDLES_AXI_MEM_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <array>
#include <cstdint>

namespace bundles {

struct AxiMemBundle : public bundle_base {
    static constexpr uint8_t MEM_READ       = 0;
    static constexpr uint8_t MEM_WRITE      = 1;
    static constexpr uint8_t MEM_READ_RESP  = 2;
    static constexpr uint8_t MEM_WRITE_RESP = 3;
    static constexpr uint8_t DMA_DESC       = 8;  // desc_in：DmaDescriptor 编码
    static constexpr uint8_t DMA_DONE       = 9;  // done_out：CompletionBundle 编码
    static constexpr std::size_t MAX_DATA_BYTES = 4096;

    ch_uint<8>  kind;
    ch_uint<64> addr;
    ch_uint<32> len;
    ch_uint<32> id;
    ch_uint<8>  resp;
    std::array<uint8_t, MAX_DATA_BYTES> data_buf{};

    AxiMemBundle() = default;

    bool is_read()  const { return kind.read() == MEM_READ; }
    bool is_write() const { return kind.read() == MEM_WRITE; }
    bool is_resp()  const {
        const uint8_t k = kind.read();
        return k == MEM_READ_RESP || k == MEM_WRITE_RESP;
    }

    // **ch_uint 赋值约定**: 所有 ch_uint 字段必须用 .write() 赋值,
    // 不可直接 `field = value;` (explicit ctor). 见 design §4-§6 修订骨架.
};

} // namespace bundles

#endif // BUNDLES_AXI_MEM_BUNDLES_TLM_HH
```

**框架侧 N1 修复** (伪代码, 2 行):
```cpp
// include/framework/stream_adapter.hh (或对应文件)
template<typename BundleT>
int OutputStreamAdapter<BundleT>::send(const BundleT& b) {
    ensure_payload_size(sizeof(BundleT));  // 新增: 按 BundleT 类型大小扩容
    return serialize_to_payload(b);
}

template<typename BundleT>
int InputStreamAdapter<BundleT>::process(...) {
    ensure_payload_size(sizeof(BundleT));  // 新增
    return deserialize_from_payload(...);
}
```

**T0.2 测试** (N1 关键验证): 经真实 StreamAdapter (PcieMemoryDevice SlavePort 接 SDMA MasterPort) 收发 AxiMemBundle，**不**做裸 serialize round-trip。

## §4 PcieMemoryDevice 改造骨架（2 端口 + **单 adapter + v1.3 B3/B4/B6 修正**）

```cpp
// include/tlm/gpu/pcie_memory_device.hh 修订后 (v1.3: 单 adapter + lazy alloc + mutex + 双注册)
#include "core/chstream_module.hh"
#include "bundles/axi_mem_bundles_tlm.hh"
#include <mutex>

namespace tlm::gpu {

class PcieMemoryDevice : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_PORTS = 2;
    static constexpr unsigned PORT_SDMA = 0;  // sdma.mem_out 接入
    static constexpr unsigned PORT_GMMU = 1;  // gmmu.req_out 接入

    PcieMemoryDevice(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
        init_identity_regs();
        // **v1.4 B7**: 不再预分配 8GB; backing 由 DGpuBoard 注入 (vram_storage_)
        // **v1.4 B10**: 保留 mutex (保护 sim_thread + host mmio 并发访问)
        // backing_ptr_ 永不变 (unique_ptr default-init stable pointer); mutex 仅用于并发保护
    }

    std::string get_module_type() const override { return "PcieMemoryDevice"; }
    unsigned num_ports() const override { return NUM_PORTS; }

    // ── ChStream SlavePort（多端口适配器要求 public）──
    cpptlm::InputStreamAdapter<bundles::AxiMemBundle>  req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::AxiMemBundle> resp_out[NUM_PORTS];

    // **v1.3 B3 撤销 N4**: 存单 MultiPortStreamAdapter* 而非 adapters_[2]
    // 事实 (per module_factory.cc:695-705): multi-port 模块 framework 只注入单个
    // MultiPortStreamAdapter, 其内部 tick() 已遍历全 N 端口 (multi_port_stream_adapter.hh:56-66)
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override {
        adapter_ = a;  // 单 adapter 存储
    }

    // ── 既有 API 全部保留 (DGpuBoard backdoor + D2 测试用) ──
    int mmio_read(uint64_t offset, void* buf, size_t len);
    int mmio_write(uint64_t offset, const void* buf, size_t len);

    // **v1.4 B7**: backing 由 board 注入; bound 用 backing_size_ (B8)
    // **v1.4 B12**: 未注入 (backing_ptr_==nullptr) 返 -ENODEV
    // **v1.4 B10**: mutex 保护 host/sim 并发访问
    void set_backing_store(uint8_t* ptr, uint64_t size_bytes) noexcept {
        std::lock_guard<std::mutex> lock(backing_mutex_);
        backing_ptr_ = ptr;
        backing_size_ = size_bytes;
    }
    bool has_memory_backing() const noexcept { return backing_ptr_ != nullptr; }
    uint64_t backing_size() const noexcept { return backing_size_; }

    int memory_read(uint64_t offset, void* buf, size_t len);  // **v1.4 B10**: mutex; **B8** bound=backing_size_; **B12** -ENODEV on null
    int memory_write(uint64_t offset, const void* buf, size_t len);

    void tick() override {
        cycle_counter_++;
        for (unsigned p = 0; p < NUM_PORTS; ++p) {
            handle_slave_port(p);
        }
        // **v1.3 B3**: 单 tick 一次; MultiPortStreamAdapter 内部遍历 N 端口
        if (adapter_) adapter_->tick();
    }

private:
    void handle_slave_port(unsigned p) {
        if (!req_in[p].valid() || !req_in[p].ready()) return;
        const auto& req = req_in[p].data();

        bundles::AxiMemBundle resp;
        // resp.id = req.id;  // ch_uint copy-assignment OK (POD-to-POD)
        resp.id.write(req.id.read());               // **ch_uint .read()/.write() 习惯用法**
        const uint64_t off = req.addr.read();
        const uint32_t len = req.len.read();

        // **v1.4 B9**: 检查 memory_read/write 返回值, 失败置 SLVERR (resp=1)
        // **v1.4 B8**: bound 检查用 backing_size_ (非 kDefaultMemSize)
        // **v1.4 B12**: backing_ptr_==nullptr 返 -ENODEV, 路径置 SLVERR
        const bool null_backing = (backing_ptr_ == nullptr);
        const bool oob = (len == 0 || len > bundles::AxiMemBundle::MAX_DATA_BYTES ||
                          off >= backing_size_ || len > backing_size_ - off);
        if (null_backing || oob) {
            resp.resp.write(1);                    // SLVERR
        } else if (req.is_read()) {
            int r = memory_read(off, resp.data_buf.data(), len);
            resp.kind.write(bundles::AxiMemBundle::MEM_READ_RESP);
            resp.resp.write(r == 0 ? 0 : 1);       // **B9**: 传播错误
        } else if (req.is_write()) {
            int r = memory_write(off, req.data_buf.data(), len);
            resp.kind.write(bundles::AxiMemBundle::MEM_WRITE_RESP);
            resp.resp.write(r == 0 ? 0 : 1);       // **B9**: 传播错误
        } else {
            resp.resp.write(1);
        }
        resp_out[p].write(resp);
        req_in[p].consume();
    }

    cpptlm::StreamAdapterBase* adapter_ = nullptr;  // **v1.3 B3**: 单 adapter (非数组)
    // **v1.4 B7**: backing 注入式 (非 owned vector)
    uint8_t* backing_ptr_ = nullptr;
    uint64_t backing_size_ = 0;
    // **v1.4 B10**: 保留 mutex (保护 host mmio + sim_thread 并发访问)
    mutable std::mutex backing_mutex_;
    // registers_ / cycle_counter_ 不变 (registers_ 仍 owned, 与 backing 解耦)
};

} // namespace tlm::gpu
```

**v1.3 B6 双注册** (`chstream_register.hh`):
```cpp
// **B6 关键**: 必须同时 registerObject + registerMultiPortAdapter
ModuleFactory::registerObject<tlm::gpu::PcieMemoryDevice>("PcieMemoryDevice");
ChStreamAdapterFactory::get()
    .registerMultiPortAdapter<tlm::gpu::PcieMemoryDevice,
                             bundles::AxiMemBundle, bundles::AxiMemBundle, 2>("PcieMemoryDevice");
```

## §5 GMMU 异步状态机（iova 匹配 + 访问器）

```cpp
// include/tlm/gpu/gmmu_tlm.hh 修订后 (N2 + 访问器适配 + ch_uint API)
#include "core/chstream_module.hh"
#include "bundles/axi_mem_bundles_tlm.hh"

namespace tlm::gpu {

class GmmuTLM : public ChStreamModuleBase {
public:
    explicit GmmuTLM(const std::string& n, EventQueue* eq)
        : ChStreamModuleBase(n, eq) {}

    std::string get_module_type() const override { return "GmmuTLM"; }

    // ── ChStream MasterPort 访问器 (单端口模板要求) ──
    cpptlm::OutputStreamAdapter<bundles::AxiMemBundle>& req_out() { return req_out_; }
    cpptlm::InputStreamAdapter<bundles::AxiMemBundle>&  resp_in() { return resp_in_; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override { adapter_ = a; }

    // ── 寄存器接口 ──
    void set_pt_base_lo(uint32_t lo) noexcept { pt_base_lo_ = lo; }
    void set_pt_base_hi(uint32_t hi) noexcept { pt_base_hi_ = hi; }
    void set_enabled(bool en) noexcept { enabled_ = en; }
    uint64_t pt_base() const noexcept { return ...; }

    // ── dual-mode legacy ──
    void set_backing(uint8_t* ptr, uint64_t sz) noexcept {
        backing_ = ptr; backing_size_ = sz;
    }

    // §D3.8 签名冻结: 同步 int 返回。
    // **N2 修订**: COMPLETE/WAIT 加 pending_iova_ 匹配检查
    int translate(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
        if (!enabled_ || pt_base() == 0) return -EIO;

        // legacy 同步路径
        if (backing_) return translate_sync(iova, size, out_paddr);

        constexpr uint64_t page_mask = 4095;
        if ((iova & ~page_mask) != ((iova + size - 1) & ~page_mask)) return -EIO;
        const uint64_t pte_addr = pt_base() + (iova >> 12) * 8;

        if (state_ == State::IDLE) {
            bundles::AxiMemBundle req;
            req.kind.write(bundles::AxiMemBundle::MEM_READ);  // ch_uint .write()
            req.addr.write(pte_addr);
            req.len.write(8);
            req.id.write(next_req_id_++);
            req_out_.write(req);
            state_ = State::WAIT;
            pending_iova_ = iova;
            pending_size_ = size;
            return -EAGAIN;
        }

        // **N2 关键**: WAIT/COMPLETE 检查 iova 匹配（乱序重试防御）
        if (state_ == State::WAIT || state_ == State::COMPLETE) {
            if (iova != pending_iova_ || size != pending_size_) {
                return -EAGAIN;  // 不消费，pending_iova_ 仍在等待
            }
        }

        if (state_ == State::WAIT) return -EAGAIN;
        // COMPLETE
        state_ = State::IDLE;
        uint64_t pte = 0;
        std::memcpy(&pte, pte_buf_.data(), 8);
        if (!(pte & 1ULL)) return -EIO;
        out_paddr = (pte & ~page_mask) | (iova & page_mask);
        return 0;
    }

    void tick() override {
        if (state_ == State::WAIT && resp_in_.valid()) {
            const auto& r = resp_in_.data();
            if (r.resp.read() == 0) {
                std::memcpy(pte_buf_.data(), r.data_buf.data(), 8);
                state_ = State::COMPLETE;
            } else {
                state_ = State::IDLE;
            }
            resp_in_.consume();
        }
        if (adapter_) adapter_->tick();
    }

private:
    enum class State { IDLE, WAIT, COMPLETE };
    int translate_sync(uint64_t, uint32_t, uint64_t&);

    State state_ = State::IDLE;
    uint64_t pending_iova_ = 0;
    uint32_t pending_size_ = 0;
    uint32_t next_req_id_ = 1;
    std::array<uint8_t, 8> pte_buf_{};
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

    // **关键**: req_out_/resp_in_ 必须作为数据成员存在, 但访问器方法提供接口
    cpptlm::OutputStreamAdapter<bundles::AxiMemBundle> req_out_;
    cpptlm::InputStreamAdapter<bundles::AxiMemBundle>  resp_in_;

    uint8_t* backing_ = nullptr;
    uint64_t backing_size_ = 0;
};

} // namespace tlm::gpu
```

**死锁 + 乱序论证**:
- `translate()` 任何路径都不自旋；`-EAGAIN` 把"等 PTE"推迟到调用方下一 tick
- **乱序保护**: WAIT/COMPLETE 入口的 iova 匹配检查防止 SDMA 重试时不同 iova 静默错配
- 响应经 `pcie_memory.tick()`（同一 sim 超步内后续模块）→ `adapter_->tick()` → 下一 GMMU tick 可见
- **v1.0 约束**: 1 outstanding translate（单槽位 adapter）；并发由 SDMA `max_inflight` 串行化重试

## §6 SDMA 5 端口换型 + retry driver + slot-2 resp (N3/N7) — **v1.3 B1+B2 修正：字段名 + 切型范围**

**v1.3 B2 切型范围（minimal_v1 限定）**：

| 端口 | wire-format (v1.2) | wire-format (v1.3) | 理由 |
|------|---------------------|---------------------|------|
| `mem_in[PORT_MEM_IN]` | PcieTlpBundle | **AxiMemBundle** | chip-internal (pcie_memory resp → sdma) |
| `mem_out[PORT_MEM_OUT]` | PcieTlpBundle | **AxiMemBundle** | chip-internal (sdma → pcie_memory req) |
| `desc_in[PORT_DESC_IN]` | PcieTlpBundle | **PcieTlpBundle** (不变) | board-level, minimal_v1 不接线 (经 BAR1 ring doorbell) |
| `done_out[PORT_DONE_OUT]` | PcieTlpBundle | **PcieTlpBundle** (不变) | board-level, minimal_v1 不接线 |
| `host_out[PORT_HOST_OUT]` | PcieTlpBundle | **PcieTlpBundle** (不变) | board-level, minimal_v1 不接线 (host 侧数据由 N9 host_backdoor 注入) |

→ 实际接入是**混合端口**：`mem_in`/`mem_out` 切型 + `desc_in`/`done_out`/`host_out` 保持。混合端口模板在 minimal_v1 范围外（spec "不在范围"）。同构 `MultiPortStreamAdapter<SdmaEngineTLM, PcieTlpBundle, PcieTlpBundle, 5>` 整体保留 (desc_in/done_out/host_out 仍为 PcieTlpBundle)；mem_in/mem_out 的 AxiMemBundle 转换通过 `to_axi_mem_descriptor` / `from_axi_mem_completion` helper 在 chip-internal 路径完成（不通过 StreamAdapter 跨类型）。

**v1.3 B1 字段名修正**（per `dma_descriptor_mvp.hh:44-48`）:

```cpp
// 错误 (v1.2):
int tr = translate_cb_(e.desc.dst_iova_offset, e.desc.len, phys);

// 正确 (v1.3): 真实字段名 host_iova / size / vram_offset
int tr = translate_cb_(e.desc.host_iova, e.desc.size, phys);
//                              ^^^^^^^^^^   ^^^^
//                              iova 经     字节数
//                              GMMU → PA
```

DmaDescriptor 字段表（per `dma_descriptor_mvp.hh`）:
- `dir` (H2D/D2H/D2D) — 方向
- `host_iova` (uint64) — host 侧 IOVA 经 IOMMU translate
- `vram_offset` (uint64) — SOC VRAM 内偏移
- `size` (uint32) — 字节数
- `tag` (uint32) — 完成关联 ID

**N3 retry driver + N7 slot-2 resp 核心代码**:

```cpp
// sdma_engine_tlm.cc 新逻辑 (v1.3 字段名修正 + 切型范围限定)

void SdmaEngineTLM::tick() {
    // **N3 关键**: 先 FIFO 重试 inflight_ 再处理新 desc (保 in-order)
    retry_inflight();

    // 然后正常收 desc_in (board-level, 保持 PcieTlpBundle)
    while (desc_in_.valid() && desc_in_.ready()) {
        auto desc = desc_in_.data();
        desc_in_.consume();
        inflight_.push({desc, State::PENDING_TRANSLATE});
    }
    drain_inflight();  // 处理就绪的 inflight
    if (adapter_) adapter_->tick();  // PcieTlpBundle 同构 adapter (mem_in/mem_out 通过 helper 转换)
}

void SdmaEngineTLM::retry_inflight() {
    for (auto& e : inflight_) {
        if (e.state == State::PENDING_TRANSLATE ||
            e.state == State::PENDING_AXI_RESP) {
            process_inflight_step(e);  // 调 translate / 等 AXI resp / emit mem_out
        }
    }
}

void SdmaEngineTLM::process_inflight_step(InflightEntry& e) {
    if (e.state == State::PENDING_TRANSLATE) {
        uint64_t phys = 0;
        // **v1.3 B1**: 字段名 host_iova / size (非 dst_iova_offset / len)
        int tr = translate_cb_(e.desc.host_iova, e.desc.size, phys);
        if (tr == -EAGAIN) return;             // 下 tick 再试
        if (tr != 0) { /* 错误处理 */ return; }
        e.phys_offset = phys;
        e.state = State::READY_TO_EMIT;
    }
    if (e.state == State::READY_TO_EMIT && !e.emitted) {
        if (has_vram_backdoor()) {
            // dual-mode legacy (既有 [sdma] 测试路径, 零逻辑改动)
            std::memcpy(static_cast<uint8_t*>(vram_backdoor_) + e.phys_offset,
                        e.host_buf, e.desc.size);  // **v1.3 B1**: size 而非 len
            e.state = State::DONE;
        } else {
            // chip-internal path: emit mem_out AxiMemBundle
            bundles::AxiMemBundle req;
            req.kind.write(bundles::AxiMemBundle::MEM_WRITE);
            req.addr.write(e.phys_offset);
            req.len.write(e.desc.size);          // **v1.3 B1**
            req.id.write(e.tx_id);
            std::memcpy(req.data_buf.data(), e.host_buf, e.desc.size);  // **v1.3 B1**
            resp_out_[PORT_MEM_OUT].write(req);  // emit to pcie_memory
            e.state = State::WAITING_AXI_RESP;
        }
    }
    if (e.state == State::WAITING_AXI_RESP) {
        // **N7 关键**: resp 从 slot 2 (PORT_MEM_OUT 同一端口的 resp_in) 消费
        // ChStream 端口对称性: resp_in[p] 与 resp_out[p] 同 port, master 端
        // sdma.req_in[PORT_MEM_OUT] (slot 2) 接收 pcie_memory.resp_out[0] 的 resp
        if (req_in_[PORT_MEM_OUT].valid()) {
            auto resp = req_in_[PORT_MEM_OUT].data();
            req_in_[PORT_MEM_OUT].consume();
            if (resp.resp.read() == 0) e.state = State::DONE;
        }
    }
    if (e.state == State::DONE) {
        // emit done, 移除 inflight_
    }
}
```

**字段名映射表** (v1.2 错名 → v1.3 正名 per `dma_descriptor_mvp.hh`):
- ~~`d.dst_iova_offset`~~ → **`e.desc.host_iova`** (IOVA, 经 GMMU translate 为 PA)
- ~~`d.len`~~ → **`e.desc.size`** (字节数)
- (新增)`e.desc.vram_offset` — SOC VRAM 内偏移, 用于 H2D 写入目标
- (新增)`e.desc.tag` — 完成关联 ID, 用于 done_out 回传

**trade-off 声明** (v1.3 修正): `desc_in`/`done_out`/`host_out` 语义上是 board-level (PcieTlpBundle, 不变); minimal_v1 生产路径经 BAR1 ring doorbell (mmio) 注入 descriptor + 读取 completion；**port 直注仅测试用**；`host_out` 数据由 N9 host_backdoor 注入（host 侧数据非 chip-internal AXI 路径）。

## §7 minimal_v1 JSON 修订（含 N6 标注）

```diff
   "display_routing_enabled": false,
   "storage_routing_enabled": true,
   "gmmu_routing_enabled": true,
+  "memory_routing_enabled": true,
   "framebuffer_size_bytes": 16777216,
   "modules": [
     {
       "name": "soc",
       "type": "DGpuSoc",
       "modules": [
+        {
+          "name": "pcie_memory",
+          "type": "PcieMemoryDevice",
+          "params": { "capacity_gb": 8 }
+        },
         {
           "name": "pcie_ep",
           "type": "PcieEndpointIP",
           "params": {
             "config_size": 4096,
             "num_msix_vectors": 16,
-            "bar_sizes": [4096, 16777216],
+            "bar_sizes": [4096, 16777216, 8589934592],
             "bar0_registers": [ ... 不变 ... ]
           }
         },
         ... sdma / gmmu / memory / completion 不变 ...
-      ]
+      ],
+      "connections": [
+        { "src": "sdma.2", "dst": "pcie_memory.0", "latency": 1 },
+        { "src": "gmmu.0", "dst": "pcie_memory.1", "latency": 1 }
+      ]
     }
   ]
```

**N6 标注**: BAR2 启用依赖 T0.4 EP BAR 寄存器从 bar_sizes 生成 (`PcieConfigSpace::init()` 须读 EP 的 bar_sizes 字段填 BAR 寄存器)。无此实现时 BAR2 启用但 host 枚举看不到 BAR2 (size=0)。

## §8 数据流验证（v1.2 N7 修订）

```
1. Driver cpptlm_emulator_pcie_config_read(0x00, 4)
   → ep->config_space().read(0) = 0x123410DE (vendor 0x10DE + device 0x1234)

2. Driver mmio_write(2, 0x1000, data, 8)
   → BAR2 fast-path → ep->memory_device().memory_write(0x1000, data, 8)
   → memory_backing_[0x1000..0x1007] = data (mutex 保护 lazy alloc + write)

3. Driver mmio_read(2, 0x1000, buf, 8) → data (mutex 保护 read)

4. Driver mmio_write(2, pte_off, pte, 8) → PTE 写入 memory_backing_

**v1.3 B5 修正**: BAR 寄存器读断言应为 **64-bit 双 dword 编码**：
- `ep->config_space().read(0x20) == 0` (BAR2 低 32-bit)
- `ep->config_space().read(0x24) == 2` (BAR2 高 32-bit, 表示 8GB)
- ~~`read(0x20) == 8589934592`~~ (**v1.2 错误**: 8589934592 = 0x2_0000_0000 超出 uint32_t 返回值, 必失败)

5. SDMA H2D (desc via desc_in)
   → sdma.tick() → retry_inflight() (N3) → 先处理 inflight_
   → 新 desc 入 inflight_ (state=PENDING_TRANSLATE)
   → retry_inflight() 调 translate_cb_(iova, size, phys)
   → gmmu.translate(iova, size, phys):
     - iova 与 pending_iova_ 不匹配 → -EAGAIN (N2)
     - iova 匹配且 state_=IDLE → 发 AXI MEM_READ → -EAGAIN
   → sdma 留 inflight_ 在 PENDING_TRANSLATE
   → 多 tick 后 gmmu 收 resp → state_=COMPLETE
   → 下 tick sdma.retry_inflight() → translate 返 0 → READY_TO_EMIT
   → emit mem_out[2] AxiMemBundle MEM_WRITE
   → pcie_memory.handle_slave_port(0) → memory_write
   → pcie_memory.resp_out[0] → sdma.req_in[2] (slot 2, PORT_MEM_OUT) (N7)
   → sdma.retry_inflight() → WAITING_AXI_RESP → DONE
   → emit host_out + done_out

6. GMMU translate 同样 N2+N3 路径

7. Driver backdoor:
   → cpptlm_emulator_backdoor_write(0, 0x1000, data, 8)
   → DGpuBoard::backdoor_write → framebuffer_ptr_[0x1000] 直写
```

## §9 测试覆盖（v1.2 P1 修订）

### 9.1 现有测试保持 (N5 需迁移)
- `[pcie-memory]` — basic/backing/routing_characterization/routing_flag (N5 改: 适配 EP 默认 nullptr + 2-port)
- `[sdma]` — 6+ 文件 (N5 改: PcieTlpBundle → AxiMemBundle)

### 9.2 新增测试 (v1.2)
| 测试 | 标签 | 来源 |
|------|------|------|
| `test_axi_mem_bundle_stream_adapter_roundtrip.cc` | `[axi_mem][stream]` | **N1** 经 StreamAdapter 真实 round-trip |
| `test_pcie_endpoint_ip_three_bar.cc` | `[pcie-ep][bar]` | **N6** EP BAR 寄存器生成 |
| `test_pcie_endpoint_ip_tick_nesting.cc` | `[pcie-ep][tick]` | **N8** EP 不双 tick |
| `test_gmmu_iova_match.cc` | `[gmmu][iova]` | **N2** iova 匹配检查 |
| `test_sdma_retry_driver.cc` | `[sdma][retry]` | **N3** inflight retry driver |
| `test_sdma_slot2_response.cc` | `[sdma][slot2]` | **N7** resp slot-2 |
| `test_dgpu_board_legacy_dual_mode.cc` | `[board][legacy]` | **N12** 无 pcie_memory 保留 legacy |
| `test_pcie_memory_device_preallocate.cc` | `[pcie-memory][prealloc]` | **N10** 预分配防 race |
| `test_pcie_memory_device_axi_gmmu.cc` | `[pcie-memory][axi][gmmu]` | GMMU AXI 路径 |
| `test_pcie_memory_device_axi_sdma.cc` | `[pcie-memory][axi][sdma]` | SDMA AXI 路径 |
| `test_minimal_soc_driver_visible_e2e.cc` | `[minimal_dgpu_soc][driver_visible]` | 端到端 |
| `test_pcie_memory_device_topology_lifecycle.cc` | `[pcie-memory][topology][lifecycle]` | **N11** 改 "互不解引用" 断言 |
| `test_pcie_memory_device_pte_unified.cc` | `[pcie-memory][pte][unified]` | PTE 经 BAR2 |

### 9.3 验证清单

- [ ] N1 经 StreamAdapter round-trip 通过
- [ ] N2 iova 匹配检查通过
- [ ] N3 retry driver 多 tick 完成 in-order
- [ ] **v1.3 B3 撤销 N4** adapters_[2] 双 tick 测试（不可能）→ **新增 v1.3 B3**: 单 adapter tick 后两端口 resp_out 都出
- [ ] N5 既有 [sdma]/[pcie-memory] 测试迁移后全绿
- [ ] N6 EP 3-BAR config space 测试通过（**v1.3 B5**: 64-bit 双 dword, `read(0x20)==0 && read(0x24)==2`）
- [ ] N7 SDMA slot-2 resp 测试通过
- [ ] N8 EP 不双 tick 测试通过
- [ ] N10 预分配测试通过
- [ ] N11 互不解引用测试通过
- [ ] N12 legacy dual-mode 回归测试通过
- [ ] openspec validate --changes --strict PASS
- [ ] docs_sync_check --strict PASS
- [ ] git diff HEAD -- include/abi/cpptlm_emulator.h 空
- [ ] 15 ABI 函数签名不变
- [ ] 4 callback typedef 不变
- [ ] pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh 零 diff

## §10 ABI 冻结: 0 影响

## §11 gem5 语义对齐（v1.2 P1 修订）

| gem5 概念 | D-AXI 对齐 |
|-----------|-----------|
| PhysicalMemory::backing_store | PcieMemoryDevice::memory_backing_ (8GB, 预分配) + DGpuBoard::framebuffer_ptr_ (16MB) |
| MemBackdoor | DGpuBoard::backdoor_read/write (host-side 特权) |
| AbstractMemory 端口模型 | PcieMemoryDevice ChStream SlavePort × 2 (SDMA + GMMU) |
| 地址区间路由 | BAR 路由 (flag-based MVP); spec 写明 "backdoor offset = BAR 内偏移" |
| coherent/non-coherent | **不建模** (UsrLinuxEmu 端职责) |
| 驱动页表维护 | BAR2 mmio 写 PTE + GMMU AXI 读同一 backing |

## §12 工时统计（v1.2 修订）

| Task | 估时 | 累计 | 来源 |
|------|------|------|------|
| **P1 修订** | 0.5d | 0.5d | — |
| **T0** 表征 + AxiMemBundle StreamAdapter round-trip + EP 3-BAR + baseline | 1d | 1.5d | N1/N6/N12 |
| **T1** PcieMemoryDevice 2-port + **单 adapter** (v1.3 B3 撤 N4 双 adapter) + EP raw ptr + 删 tick 转发 + EP BAR 实现 (v1.3 B5 64-bit 双 dword) + 既有 [pcie-memory] 测试迁移 | 2d | 3.5d | **v1.3 B3/B4/B5/B6**/N8/N5/N6 |
| **T2** GMMU 异步 + iova 匹配 + 访问器 + 注册迁移 | 1.5d | 5d | N2 |
| **T3** SDMA 5 端口换型 + retry driver + slot-2 resp + D2H 异步 + 既有 [sdma] 测试迁移 | 2.5d | 7.5d | N3/N7/N5/N9 |
| **T4** JSON + BAR2 fast-path + bind_memory_backings 条件化 + E2E + 文档 | 1.5d | 9d | N10/N12 |
| **合计** | **~9d ≈ 1.8 周** | | |

## §13 不在范围 + v1.4 延期声明

| 项 | 后置阶段 | 备注 |
|----|---------|------|
| 多 PcieMemoryDevice 实例 | 不实现 | minimal_v1 单实例 |
| GMMU 多级页表（v1.1） | 不实现 | v1.4 仅一级 4KB 固定页 |
| GPU 计算设备 | D3 | **D3 seam 已预留**（`handle_slave_port` ↔ `backing_ptr_` 之间可插入 VramControllerTLM/MemoryClusterTLM，无 API 变更） |
| Coherent/Non-coherent 在 CppTLM 建模 | **不建模** (UE 端职责) | spec Requirement "Coherence 域边界" 显式 |
| 混合端口模板（desc_in PcieTlpBundle） | 后续单独立项 | v1.3 B2 临时以"端口混合切型"应对；D3 立项异构 adapter |
| ArchForge 跨仓引用 | 零依赖 | |
| **D1 PcieDisplayDevice 32MB FB 同构问题** | **D3 一并收编（不阻塞 v1.4）** | **理由**：`pcie_display_device.hh` 是冻结面（per v1.3 spec "Freeze Surface Untouched" + ADR-088 §D5），v1.4 不得修改；minimal_v1 `display_routing_enabled=false` 不触发；D3 按 Option D 模式（`set_backing_store(ptr, size)`）将 D1 FB 也归 board 持有 |
| **BAR1 doorbell offset 0x10010000 > BAR1 16MB 实窗** | **测试专用合成偏移（不修复）** | **理由**：现行 `dgpu_board_shell.cc:306-447` 在 BAR1 fast-path bound 检查**之前**先匹配 doorbell 路径（不依赖 BAR1 实窗大小），实际不触发越窗；该常量（`kBar1DoorbellOffset`，`dgpu_board_shell.hh:171`）是 driver 测试的虚拟地址合成，非 PCIe 物理 BAR 内偏移；v1.4 拆分 `bar1_window_size_` 后该语义更清晰但**不修**——避免改常量弄断既有 `[sdma][doorbell]` 测试 |
| **MemoryTLM `capacity_gb=1` vs `vram_size_=8GB` 交互** | **v1.4 不修改 capacity_gb，保持 1GB（不静默）** | **理由**：minimal_v1 JSON `memory.params.capacity_gb=1` 是 CPU 侧 cache 路径的合理容量（GMMU 单级 4KB 页表 × 64MB PT = 16K PTE × 8B = 128KB，远小于 1GB）；CPU 侧 cache 经 MemoryTLM 访问 ≥ 1GB 仍按现 v2.2 行为返 `error_code=1` (OUT_OF_RANGE)；BAR2 路径不受影响（独立经 PcieMemoryDevice 转发到 vram）；spec Requirement 显式声明 "single VRAM 对 MemoryTLM 消费者为 `min(vram_size_, MemoryTLM.size_cap_)`"，即 memory 视角最大 1GB、driver 视角最大 8GB；D3 引入真 VramController 时一并评估 capacity 同步策略 |

## §X 演进路线图 (minimal → 完整 GPU)

> **设计原则**: 架构支持 minimal → 完整 GPU 演进而不引起重大重构 (user 确认, 2026-09-27)。本节定义 D3-D5 演进阶段, 锁定 5 大 seam 接口, 确保每个阶段的 module 插入不破坏邻居接口。

### X.1 演进阶段

| 阶段 | 目标 | 关键 Module 变化 |
|------|------|----------------|
| **Phase minimal (当前)** | driver-visible + H2D + D2H + D2D 基础路径 | 6 模块 (v1.5) → 5 模块 (v1.6, 移除 MemoryTLM) |
| **Phase D3** (streaming_multiprocessor) | + GPU 计算单元 (StreamingMultiprocessor) | + SM + VramController seam 就位 |
| **Phase D4** (HBM controller) | + 真 memory controller + timing | + MemoryClusterTLM, vram_storage_ 替换为多 channel |
| **Phase D5+** (完整 GPU) | + GMMU 多级页表 + 多设备共存 + SR-IOV | + 多 VF + System MMU + GPU L2 cache |

### X.2 演进原则 (5 条)

1. **抽象稳定**: module 边界 + adapter wire-format 不变
2. **接口契约**: PcieEndpointIP 4 端口冻结 + 15 ABI 函数签名不变
3. **可插拔**: 每个 module 可替换/增强不破坏邻居 (e.g., MemoryTLM → VramControllerTLM 不改邻居接口)
4. **向后兼容**: driver 看到的 BAR 接口稳定, 新增 GPU 能力通过 BAR 内部寄存器扩展
5. **单一真源**: 每条数据只有一条路径 (v1.4 B7 教训: dual-VRAM 是 bug 根源)

### X.3 5 大能力扩展路径 (minimal → 完整 GPU)

| 路径 | minimal (当前) | D3 (GPU compute) | D5 (完整 GPU) |
|------|----------------|-------------------|----------------|
| **H2D** | minimal 已通 (SDMA → PcieMemoryDevice → vram) | + GPU compute writeback (SM → VramController → vram) | + GPU L2 cache 一致性 (multi-cache coherent) |
| **D2H** | minimal 已通 (PcieMemoryDevice read) | + GPU completion notify (completion_ring → host) | + P2P via NoC (D2D) |
| **D2D** | minimal stub (vram_storage_ 单一 buffer) | + GPU compute writeback (SM result → vram) | + 多 device P2P (multi-SOC NoC) |
| **GMMU** | 1 级 4KB 固定页 (v1.4) | 1 级不变 | + 多级 2MB/1GB 大页 (PTE walk) |
| **多 device** | 1 PF (minimal_v1) | 1 PF 不变 | + SR-IOV: 1 PF + N VF (每个 VF 独立 vram) |

### X.4 Module 演进定位表

| Module | 阶段定位 | v1.6 状态 | 演进预期 |
|--------|---------|----------|---------|
| PcieEndpointIP | 长期核心 (D1-D5+) | 已冻结 (4 端口) | 4 端口冻结, 不变 |
| PcieMemoryDevice | 长期核心 (D3-D5+) | v1.4 降级为 PCIe 外观层 | minimal 纯门面 → D3+ 加 VramController seam |
| SdmaEngineTLM | 长期核心 (D3-D5+) | 5 端口 (minimal_v1 切型) | 5 端口切型范围扩展 (D3 加 GPU readback) |
| GmmuTLM | 长期核心 (D3-D5+) | 1 MasterPort, async | 1 级 → 多级 (D5) |
| **MemoryTLM** | **v1.6 移除** → D3+ 重新引入 | **F12 删除** | minimal_v1 零消费者冗余 → D3+ 作 cache 下游 |
| **CompletionRingTLM** | **v1.6 dormant** → D3+ 重新引入 | **F8 dormant** | minimal 零连接 → D3+ 作 GPU fence 语义 |
| (D3) StreamingMultiprocessor | D3 引入 | — | 1 SM/SoC → D5 多 SM/SOC |
| (D3) VramControllerTLM | D3 引入 | — | 插 `handle_slave_port ↔ backing_ptr_` 之间, 接口零变更 |
| (D4) MemoryClusterTLM | D4 引入 | — | 多通道 HBM (取代 vram_storage_ 单一通道) |
| (D5) GPU L2 cache | D5 引入 | — | 插 SM ↔ sdma/gmmu 之间 |
| (D5) SmmuTLM (System MMU) | D5 引入 | — | 多 device 隔离 (每个 VF 独立 SmmuTLM) |

### X.5 D3-D5 Seams (v1.4 已预留, v1.5 已清理债务, v1.6 锁定)

| Seam | 插入点 | v1.4 状态 | v1.5 清理 | v1.6 锁定 |
|------|--------|----------|----------|---------|
| **1. handle_slave_port ↔ backing_ptr_** | `pcie_memory_device.cc::handle_slave_port()` 与 `backing_ptr_` 之间 | B7 已预留 (backing 归 board) | B14/B15 删 vram_segments_/framebuffer_storage_ | D3 VramController 插入点, 接口零变更 |
| **2. 5 端口切型范围** | `SdmaEngineTLM` 的 5 端口 | B2 已限定 minimal_v1 范围 | B25 统一 PcieTlpBundle | D3 增加 GPU readback port, B2 已限定 minimal_v1 范围 |
| **3. sdma/gmmu/MemoryTLM 注入点** | `bind_memory_backings` 注入调用 | B7/B19/B20 已统一注入 | B14/B15/B26 删 dual-storage + 统一 bound | D5 多 cache 一致性 (Coherence 边界, U1 已显式 UE 端) |
| **4. 4 端口 PcieEndpointIP** | EP 4 端口 | N8 已删 tick 转发 | B14/B27 清理装饰 | D5 SR-IOV 加 VF 端口 (EP 内部扩展, 不改 4 端口契约) |
| **5. bind_memory_backings** | `DGpuBoard::bind_memory_backings()` 注入函数 | B7 已设单一 vram_storage_ | B19 统一 vram_size_ 注入 | D4 替换 vram_storage_ 为 VramController/MemoryClusterTLM (5 消费者保持注入接口) |

### X.6 重构触发条件 (何时触发 seam 插入)

| D 阶段 | 启动条件 | 触发动作 | 预期影响 |
|--------|---------|---------|---------|
| **D3** 启动 | driver 端需要 kernel launch (GPU 计算) + 真实 DMA timing | PcieMemoryDevice 重构 (Phase 10) + VramController 引入 + StreamingMultiprocessor 引入 | minimal_v1 BAR2 路径不变, 新增 SM ↔ VramController ↔ vram 路径 |
| **D4** 启动 | 多通道 HBM 仿真需要 | vram_storage_ 替换为 MemoryClusterTLM (单一 buffer → 多 channel + timing) | BAR2 映射到 MemoryClusterTLM 多通道, driver 视角 BAR 接口不变 |
| **D5** 启动 | 多 device 共存 + GPU L2 | 多 PcieMemoryDevice 实例 + SmmuTLM (System MMU, 多 device 隔离) + GPU L2 cache | SR-IOV 1 PF + N VF, 每个 VF 独立 vram + SmmuTLM, EP 4 端口契约不变 |

> **v1.6 锁定**: 所有 5 个 seam 接口在 v1.4/v1.5 已预留或清理债务, v1.6 不引入新 seam, 仅文档化锁定。D3-D5 触发条件由 user 决策, CppTLM 侧实现不阻塞。

---

## §Y 用户目标验证与 H1-H7 修复路线 (v1.7)

> **目标**: v1.7 最终满足用户原始目标 — "实现GPU里最基本的 H2D/D2H/D2D 能力，在最大化保存驱动代码兼容性同时，soc架构可以支持演进成完整的GPU架构"。H1-H7 是 Oracle 第四轮审计发现的 7 项 P0/P1 缺陷，必须全部修复后才能声称满足用户目标。

### Y.1 用户目标 vs v1.7 能力映射

| 用户目标 | minimal_v1 实现方式 | 验证场景 |
|---------|-------------------|---------|
| **H2D (Host → Device)** | BAR1 ring window 写 descriptor → doorbell 触发 SDMA → chip-internal AXI 写 vram | Scenario H2D-1: driver 经 BAR1 ring + doorbell → vram 数据可见 |
| **D2H (Device → Host, host-pull)** | driver 经 BAR2 mmio_read 直读 vram | Scenario D2H-1: BAR2 mmio_read 返回 vram 内容 |
| **D2D (Device → Device)** | descriptor D2D 提交走 ring, SDMA mem_out 两拍 (read+write 同一 backing) | Scenario D2D-1: D2D status=-ENOSYS 或真实两拍搬运 |
| **driver 源码兼容** | 15 ABI 零变更 + BAR0/1/2 布局稳定 | Scenario compat-1: driver 二进制兼容 minimal → D5 |
| **SoC 可演进** | 5 seams 接口零变更 | Scenario evol-1: D3 VramController 插入无影响 |

### Y.2 H1-H7 修复路线详述

#### H1: wire-format 矛盾

**问题**: spec.md 既说 AxiMemBundle 又说 PcieTlpBundle，design §4 说 PcieMemoryDevice SlavePort wire-format，§5 GMMU MasterPort 用 AxiMemBundle，矛盾无法实施。

**修复决策**: 采纳 **B25 全 PcieTlpBundle**（推荐），理由:
- `AxiMemBundle` 是 D3 VramController chip-internal 专用
- minimal_v1 范围所有端口（EP ↔ board）都是 PcieTlpBundle
- GMMU MasterPort 在 minimal_v1 经 PcieTlpBundle 连接 pcie_memory

**修改范围**:
- `spec.md:43/62`: 删 AxiMemBundle 路由描述，改为"PcieTlpBundle 统一 board-level wire-format"
- `design.md §1`: PcieMemoryDevice port0/port1 wire-format 改为 PcieTlpBundle
- `design.md §5 GMMU`: MasterPort 改为 PcieTlpBundle（minimal_v1 范围）
- `docs/pcie/driver-visible-minimal-soc.md §2.1`: 对应修订
- `include/bundles/axi_mem_bundles_tlm.hh`: 保留，标注"D3+ VramController chip-internal 专用"

#### H2: descriptor 不可驱动

**问题**: SDMA 有 `enable_ring_mode` + `ring_write_entry` API，但 board `mmio_write` 对 BAR1 ring 区间无路由，driver 无法注入 descriptor。

**修复**: `DGpuBoard::mmio_write` 增加 BAR1 ring 区间检测（kRingDescriptorBase=0x10000000, kRingDescriptorSize=4KB），路由到 `sdma->ring_write_entry(index, data, len)`。`bind_memory_backings` 自动调用 `sdma->enable_ring_mode(RingSize::KB_64, EntrySize::B_64)`。

#### H3: host 内存无注入者

**问题**: H2 ring-via-BAR1 需要 host 侧数据注入 backing，但 `set_host_backdoor` 从未在 `bind_memory_backings` 调用，H2/H3 形成死锁。

**修复**: `bind_memory_backings` 无条件调用 `sdma->set_host_backdoor(vram_storage_.get(), bar1_window_size_)`。spec.md 文档化: "emulator 即 host: BAR1 窗口区域即 host 内存仿真 (per N9)"。

#### H4: D2D false-success

**问题**: D2D descriptor 处理路径（`process_d2d` 或 `d2d_forward`）在 `vram_backdoor_ == nullptr` 时直接 return（无 done emit），但调用方误以为成功。

**修复**: D2D 诚实语义二选一：
- **选项A（推荐）**: D2D 走 `mem_out` 两拍（同一 backing 先 read 再 write），emit done status=0
- **选项B**: D2D minimal_v1 不支持，emit done status=-ENOSYS
- **禁止**: status=0 + 零搬运（静默 false-success）

#### H5: doorbell handler 竞态

**问题**: `mmio_write` 中 doorbell 路径既 enqueue 到 `inject_q_` 又同步调用 `sdma_engine_->mmio_write()`，跨线程并发时可能竞态。

**修复**: doorbell handler 仅负责 enqueue + 置 flag（`doorbell_pending_flag_.store(true)`），实际 ring consume 移入 `sdma.tick()`（与 N3 retry-driver 天然同构）。spec 显式声明: "SDMA 状态仅 sim 线程触碰，doorbell handler 仅负责 enqueue"。

#### H6: spec 数字漂移

**问题**: spec.md Scenario "soc 内有 6 模块"（旧）与 F12 Scenario "5 模块"（MemoryTLM 移除后）矛盾。

**修复**: 统一为"5 模块"（pcie_ep + pcie_memory + sdma + gmmu + completion）；spec.md 删所有"6 模块"引用。tasks.md 每项 H 加 `[x]` checkbox 或 `implemented-in:` 列（F11 落地）。

#### H7: inject_q_ 无限增长 + design §8 陈旧引用

**问题**: `inject_q_` 无 max size 限制，可能导致内存耗尽；design §8 Step 7 引用 `cpptlm_emulator_backdoor_write/read`（旧 ABI），与当前 BAR1 mmio 语义不符。

**修复**: `mmio_write` 加 `MAX_INJECT_Q_SIZE=4096` 限制 + DPRINTF WARN on overflow + `return -EOVERFLOW`。design §8 Step 7 + spec Scenario 引用 `cpptlm_emulator_backdoor_write/read` 改写为"driver 经 BAR1 mmio_write 写 descriptor ring 区间（经 H2 ring-via-BAR1 路由）"。

### Y.3 H1-H7 与 §X 演进路线图关系

| H项 | 影响 seam | 与 D3-D5 关系 |
|-----|---------|---------------|
| H1 | Seam 2（5 端口切型） | D3 加 GPU readback 端口仍用 PcieTlpBundle（H1 写死后不变） |
| H2 | BAR1 ring 路由 | D3 SM 新增 submit queue 映射到同一 ring 区间（接口兼容） |
| H3 | host_backdoor 注入 | D5 多 VF 时每个 VF 独立 host_backdoor 注入（接口不变） |
| H4 | D2D 语义 | D5 D2D 走 NoC 多跳（H4 诚实语义延续） |
| H5 | doorbell handler | D5 doorbell 路由扩展到多个 VF（H5 线程安全模式不变） |
| H6 | spec 数字 | D3 模块数变为 6（+ VramController），spec 需同步更新 |
| H7 | inject_q_ 上限 | D5 多通道时每个通道独立 inject_q_（接口不变） |

---

## §14 D-AXI v1.2 vs D2 v1.1 主要差异

（与 v1.1 P0 修订版本相同，外加 must-fix 标注）

---

**D-AXI v1.2 修订依据**:
- 用户 5 澄清 + Oracle R1-R7 + Metis M1-M5 (v1.1)
- Oracle 二次审查 N1-N12 + 8 must-fix (v1.2)