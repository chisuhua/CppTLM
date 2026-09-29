# driver-visible-minimal-soc Specification

## Purpose
TBD - created by archiving change cpptlm-driver-visible-minimal-soc. Update Purpose after archive.
## Requirements
### Requirement: v1.3 P0 阻塞修正 SHALL 全部应用 (前置门 — 实施前必须完成)

本 spec v1.3 修订 SHALL 包含 6 项 P0 阻塞修正，**实施 T1-T4 之前必须全部完成**：**Where**: `tasks.md` P0.1-P0.3 任务段。

The v1.3 P0 corrections SHALL:
- **B1 字段名**: SDMA 引用 DmaDescriptor 字段用 `host_iova` / `size` / `vram_offset`（per `dma_descriptor_mvp.hh:44-48`），**不**用 `dst_iova_offset` / `len`
- **B2 切型范围**: SDMA `mem_in` / `mem_out` 切 AxiMemBundle (chip-internal); `desc_in` / `done_out` / `host_out` 保持 PcieTlpBundle (board-level, minimal_v1 不接线)
- **B3 撤销 N4 双 adapter**: PcieMemoryDevice 存单 `MultiPortStreamAdapter* adapter_`（非 `adapters_[2]` 数组），tick 一次（其内部遍历 N 端口 per `multi_port_stream_adapter.hh:56-66`）
- **B4 改 lazy + mutex**: PcieMemoryDevice 不预分配 8GB（避免 8GB memset + 测试 OOM），改为 lazy alloc + `std::mutex` 保护
- **B5 64-bit 双 dword BAR**: BAR 寄存器断言用 `read(0x20) == (bar & 0xFFFFFFFF)` + `read(0x24) == (bar >> 32)`（不超 uint32_t）
- **B6 双注册**: PcieMemoryDevice + GmmuTLM 必须同时 `ModuleFactory::registerObject<T>()` + `registerMultiPortAdapter`/`registerAdapter`

The class SHALL NOT:
- 沿用 v1.2 的 `adapters_[2]` 数组或构造时 `memory_backing_.resize(8GB, 0)`
- 沿用 v1.2 的 `dst_iova_offset` / `len` 字段名
- 沿用 v1.2 的 `read(0x20) == 8589934592` 验收断言（uint32_t 溢出）

#### Scenario: 实施前 P0 修正验证通过
- **WHEN**: `openspec validate cpptlm-driver-visible-minimal-soc --strict`
- **THEN**: PASS（含 3/3 spec 校验）

#### Scenario: grep 检查无 v1.2 残留错误
- **WHEN**: `grep -r "dst_iova_offset\|adapters_\[2\]\|8589934592" openspec/changes/cpptlm-driver-visible-minimal-soc/ | grep -v "v1.2 错误\|v1.3 撤销\|v1.3 B[0-9]\|8589934592) \|bar_sizes\|8589934592 \\["`
- **THEN**: 仅匹配文档化的 v1.2 错误标注 / JSON 配置值 / B 编号引用（无未标注残留）

### Requirement: PcieMemoryDevice 提升为 soc 顶层独立组件 (v1.3 B3+B4+B6 修订)

The PcieMemoryDevice SHALL be a **soc 顶层独立组件** (ChStreamModuleBase 派生), 由 soc::internal_factory 负责生命周期管理。**Where**: `include/tlm/gpu/pcie_memory_device.hh` + `src/tlm/gpu/pcie_memory_device.cc` + `include/modules_cluster.hh`

The class SHALL:
- 派生自 `cpptlm::ChStreamModuleBase`
- **v1.8 H1 写死**: 暴露 **2 个** ChStream SlavePort 接受 chip-internal 请求 (PcieTlpBundle wire-format, **board-level 一致**); AxiMemBundle 标 D3+ VramController 内部专用
- **v1.3 B3 撤销 N4 双 adapter**: 存单 `cpptlm::MultiPortStreamAdapter<...>* adapter_`（非 `adapters_[2]` 数组），`tick()` **单 tick 一次** (`if (adapter_) adapter_->tick();`); MultiPortStreamAdapter 内部 `tick()` 已遍历全 N 端口（per `multi_port_stream_adapter.hh:56-66`）；事实：`module_factory.cc:695-705` 对 multi-port 只注入**单个** MultiPortStreamAdapter
- **v1.4 F4 撤销**: 不暴露 MMIO 寄存器空间 (无 `registers_` / `mmio_read` / `mmio_write`); BAR0 路径已由 board 处理
- **v1.4 B7**: **不**持有 backing (`uint8_t* backing_ptr_` + `uint64_t backing_size_` 由 `set_backing_store()` 注入; backing 归 `DGpuBoard::vram_storage_` 所有)
- **v1.4 B8**: `memory_read/write(offset, buf, len)` bound = injected `backing_size_`（**非** `kDefaultMemSize`; 小测试 backing 越界返 `-EINVAL` 不越界写）
- **v1.4 B9**: `handle_slave_port` 检查 `memory_read/write` 返回值; 失败置 `resp.resp.write(1)` (SLVERR); 不静默 OKAY
- **v1.4 B10**: 保留 `std::mutex backing_mutex_` 保护 `host mmio` + `sim_thread_` 并发访问; backing_ptr_ 永不变 (`unique_ptr default-init stable pointer`); mutex 仅用于并发保护
- **v1.4 B12**: 未注入 backing (`backing_ptr_ == nullptr`) 时 `memory_read/write` 返 **`-ENODEV`** (新错误码); `has_memory_backing() = backing_ptr_ != nullptr`; `kRegMemSizeLo/Hi` 未注入时报 0
- 提供 `mmio_read/write(offset, buf, len)` / `memory_read/write(offset, buf, len)` API (DGpuBoard backdoor 仍用, 内部 mutex 保护)
- 提供 `tick()` 方法推进 cycle counter + 处理 2 个 SlavePort 请求 + 单 adapter tick 一次
- device_id 标识常量 `kDeviceId=0x0002` (区别 D1 DisplayDevice 的 0x0001)
- ch_uint 字段赋值用 `.write()` (per design §3-§4)
- **v1.3 B6 双注册**: 必须 `ModuleFactory::registerObject<PcieMemoryDevice>("PcieMemoryDevice")` + `registerMultiPortAdapter<PcieMemoryDevice, AxiMemBundle, AxiMemBundle, 2>("PcieMemoryDevice")` 配对注册

The class SHALL NOT:
- 由 PcieEndpointIP 持有 (PcieEndpointIP 仅持 raw pointer)
- 在 PcieEndpointIP 构造时构造 (由 soc::internal_factory 负责)
- 触发任何中断 (无 MSI-X)
- 集成 GPU 计算
- **v1.8 H1 撤销**: 使用 AxiMemBundle 作为 SlavePort wire-format（已锁 PcieTlpBundle — H1 normative 已应用 spec.md:43）
- **v1.3 B3 撤销**: 存 `adapters_[2]` 数组或 `tick()` 遍历双 adapter
- **v1.4 B7 撤销**: 拥有 `memory_backing_` (vector owned) 或构造时分配
- **v1.4 B8 撤销**: `memory_read/write` bound 用 `kDefaultMemSize` (常量) 做边界
- **v1.4 B9 撤销**: `handle_slave_port` 忽略 `memory_read/write` 返回值
- **v1.4 B10 撤销**: 删除 `std::mutex backing_mutex_`

#### Scenario: PcieMemoryDevice 在 soc::internal_factory 中独立实例化 (v1.3 B6 验证)
- **WHEN**: `DGpuSoc` 从 `configs/dgpu_soc_minimal_v1.json` 实例化
- **THEN**: `pcie_memory` 模块独立创建; `pcie_ep.memory_device_` raw pointer 通过 `set_memory_device()` 注入; **registerObject + registerMultiPortAdapter 双注册生效**（否则 JSON `"type": "PcieMemoryDevice"` 实例化失败）

#### Scenario: PcieMemoryDevice 2 SlavePort 经**单 adapter**接受 chip-internal AXI 请求 (v1.3 B3 验证)
- **WHEN**: SDMA `mem_out` port emit AxiMemBundle MEM_WRITE 请求 (addr, data_buf, len), 且 PcieMemoryDevice 已注入 backing (4GB)
- **THEN**: PcieMemoryDevice `req_in_[0]` (PORT_SDMA) 收到, `handle_slave_port(0)` 处理, 写入 `backing_ptr_[addr : addr+len]`, 返 AxiMemBundle MEM_WRITE_RESP (resp=0) 通过 `resp_out_[0]` + **`adapter_->tick()` 一次**（由 MultiPortStreamAdapter 内部遍历两端口转发）

#### Scenario: GMMU 通过 PORT_GMMU 读 PTE (v1.3 B3 验证)
- **WHEN**: GMMU MasterPort emit AxiMemBundle MEM_READ (PTE 8 字节 @ pte_addr), 且 PcieMemoryDevice 已注入 backing
- **THEN**: PcieMemoryDevice `req_in_[1]` (PORT_GMMU) 收到, 读 `backing_ptr_[pte_addr:pte_addr+8]`, 返 MEM_READ_RESP (resp=0) 通过 `resp_out_[1]`（同一 `adapter_->tick()` 推送）；**不**需要单独的 `adapters_[1]->tick()`

#### Scenario: PcieMemoryDevice 内存分配 (**v1.4 B7 验证 — 注入式**)
- **WHEN**: `DGpuBoard::bind_memory_backings()` 调 `pcie_memory->set_backing_store(vram_storage_.get(), vram_size_)` (在 sim_thread 启动前, Inv-2 顺序)
- **THEN**: `pcie_memory->backing_ptr_ = vram_storage_.get()`; `pcie_memory->backing_size_ = vram_size_` (8GB); backing 永不变 (`unique_ptr` 持有); mutex 仅用于并发保护; **不**触发任何 O(n) lazy alloc 或 8GB memset

#### Scenario: AXI read with null backing → SLVERR (**v1.4 B12 验证**)
- **WHEN**: PcieMemoryDevice 未注入 backing (裸构造或 dual-mode legacy 分支) 且收到 AxiMemBundle MEM_READ
- **THEN**: `handle_slave_port` 检测 `backing_ptr_ == nullptr`, **不**调 `memory_read` (避免 nullptr 解引用); 置 `resp.resp.write(1)` (SLVERR); 不静默成功

#### Scenario: AXI write beyond backing_size_ → SLVERR (**v1.4 B8 验证**)
- **WHEN**: PcieMemoryDevice 已注入 backing (4KB) 且收到 AxiMemBundle MEM_WRITE offset=0x1000 len=8
- **THEN**: `handle_slave_port` 检测 `off >= backing_size_` (4KB), **不**调 `memory_write`; 置 `resp.resp.write(1)` (SLVERR); 防止 OOB memcpy 堆破坏

#### Scenario: PcieMemoryDevice 生命周期 (N11 "互不解引用")
- **WHEN**: DGpuSoc 销毁 (unordered_map 析构顺序不可强制)
- **THEN**: `~PcieEndpointIP` 不解引用 `memory_device_`; `~PcieMemoryDevice` 不触碰 `PcieEndpointIP`; `DGpuBoard::shutdown()` 先 `set_memory_device(nullptr)` 防御性保护

### Requirement: AxiMemBundle wire-format SHALL 标 D3+ chip-internal 专用 (v1.8 H1)

The AxiMemBundle SHALL be retained solely as a D3+ VramController chip-internal wire-format type reference; minimal_v1 SHALL NOT instantiate it (v1.8 H1 — PcieTlpBundle is the only wire-format on minimal_v1 wires).

> **v1.8 H1 normative 文本修订**: 本节为 AxiMemBundle 历史定义；v1.8 H1 已锁 PcieTlpBundle 为 minimal_v1 范围唯一 wire-format（见 PcieMemoryDevice Requirement §1 修订）。AxiMemBundle 仅作 "D3+ VramController chip-internal 专用" 类型保留在此。

The AxiMemBundle SHALL be a new bundle type for **chip-internal AXI 存储事务** (GMMU/SDMA ↔ PcieMemoryDevice), 严格分离于 board-level PcieTlpBundle (host↔EP 协议层)。**Where**: `include/bundles/axi_mem_bundles_tlm.hh` (新) + `include/framework/stream_adapter.hh` (N1 修改)

The bundle SHALL:
- 字段: `kind` (8-bit: MEM_READ/MEM_WRITE/MEM_READ_RESP/MEM_WRITE_RESP/DMA_DESC/DMA_DONE), `addr` (64-bit), `len` (32-bit: 1..4096), `id` (32-bit), `resp` (8-bit: 0=OKAY, 1=SLVERR), `data_buf` (4096 bytes inline)
- POD, 无堆指针
- MAX_DATA_BYTES = 4096

**N1 关键要求**: 框架侧 `OutputStreamAdapter::send` 和 `InputStreamAdapter::process` **必须** 按 `sizeof(BundleT)` 扩容 payload (2 行改动), 否则序列化失败 → 请求静默滞留。

The bundle SHALL NOT:
- 出现在 **任何 host↔board 端口** (v1.3 B2 明确: SDMA `host_out`/`desc_in`/`done_out` 必须保持 PcieTlpBundle, minimal_v1 不切型)
- 携带 coherent/non-coherent 属性

#### Scenario: AxiMemBundle 经真实 StreamAdapter round-trip (N1 必测, 不做裸 serialize)
- **WHEN**: PcieMemoryDevice SlavePort 接 mock Master 适配器, 经 `req_in_[0]` 发 AxiMemBundle MEM_WRITE → `resp_out_[0]` 收 MEM_WRITE_RESP
- **THEN**: 序列化/反序列化完整保留 (kind/addr/len/id/resp/data_buf); 框架 payload 扩容生效

#### Scenario: AxiMemBundle MEM_READ 请求
- **WHEN**: kind=MEM_READ, addr=0x1000, len=4096, id=42
- **THEN**: PcieMemoryDevice slave 收到, 读 memory_backing_[0x1000..0x1000+4095], 返 MEM_READ_RESP, data_buf=读回数据, id=42

#### Scenario: AxiMemBundle MEM_WRITE 请求
- **WHEN**: kind=MEM_WRITE, addr=0x2000, len=8, data_buf=host 数据, id=43
- **THEN**: PcieMemoryDevice slave 收到, 写 memory_backing_[0x2000..0x2007], 返 MEM_WRITE_RESP, id=43

#### Scenario: AxiMemBundle SLVERR (out-of-range)
- **WHEN**: addr=0x100000000000 (≥ 8GB) 或 len > MAX_DATA_BYTES
- **THEN**: resp=1 (SLVERR), 不读写 memory_backing_

### Requirement: PcieEndpointIP 持有 PcieMemoryDevice raw pointer (N8 修订: 不再双 tick)

The PcieEndpointIP SHALL 持有 `PcieMemoryDevice*` raw pointer (non-owning), 由 soc 通过 `set_memory_device()` 注入。**Where**: `include/tlm/pcie/pcie_endpoint_ip.hh` + `src/tlm/pcie/pcie_endpoint_ip.cc`

The class SHALL:
- `memory_device_` 字段类型从 `std::unique_ptr<PcieMemoryDevice>` 改为 `PcieMemoryDevice*`
- 默认 nullptr
- 提供 `set_memory_device(PcieMemoryDevice*)` setter
- `has_memory_device()` / `memory_device()` accessor 不变
- 构造时不创建 PcieMemoryDevice
- **N8 修订**: `tick()` 不再调 `memory_device_->tick()` (避免与 soc::tick 递归双 tick)
- `~PcieEndpointIP` 不解引用 `memory_device_`

The class SHALL NOT:
- 拥有 PcieMemoryDevice 的所有权
- 在 tick() 内调 memory_device_->tick() (N8 关键)

#### Scenario: PcieEndpointIP 不双 tick (N8)
- **WHEN**: soc 内 PcieMemoryDevice + PcieEndpointIP 都存在, 运行 N tick
- **THEN**: `PcieMemoryDevice::cycle_counter_ == N` (不是 2N)

#### Scenario: 通过 set_memory_device 注入后 has_memory_device() == true
- **WHEN**: `ep.set_memory_device(dev);`
- **THEN**: `ep.has_memory_device() == true`

#### Scenario: DGpuBoard BAR2 fast-path
- **WHEN**: `memory_routing_enabled_=true && bar=2 && has_memory_device()=true`
- **THEN**: `board.mmio_read(2, 0x1000, buf, 8)` → `ep->memory_device()->memory_read(0x1000, buf, 8)`

### Requirement: PcieConfigSpace BAR 寄存器从 bar_sizes 生成 (N6 立项)

The PcieConfigSpace SHALL 在 `init()` 时读 PcieEndpointIP 的 `bar_sizes` 参数, 写入 BAR 寄存器 (offset 0x10-0x2C), 让 host 枚举看到正确 BAR 大小。**Where**: `src/tlm/pcie/pcie_config_space_mvp.cc` (或对应文件)

The class SHALL:
- `init()` 函数: 遍历 `bar_sizes`, 写 BAR 寄存器
- BAR0 @ offset 0x10 = bar_sizes[0]
- BAR1 @ offset 0x18 = bar_sizes[1]
- BAR2 @ offset 0x20 = bar_sizes[2] (如存在)
- BAR3 @ offset 0x28 = bar_sizes[3] (如存在)
- ... 最多 6 BAR (PCIe spec)

#### Scenario: PcieEndpointIP 3-BAR config space 测试 (N6 + **v1.3 B5 关键: 64-bit 双 dword**)
- **WHEN**: PcieEndpointIP 接受 `bar_sizes=[4096, 16777216, 8589934592]` (8GB)
- **THEN** (**v1.3 B5**: 64-bit BAR 拆为两个 32-bit dword, 避免 uint32_t 溢出):
  - `ep->config_space().read(0x10)` = 4096 (BAR0 低 32-bit)
  - `ep->config_space().read(0x18)` = 16777216 (BAR1 低 32-bit)
  - `ep->config_space().read(0x20)` = `bar_sizes[2] & 0xFFFFFFFF` = 0 (BAR2 低 32-bit)
  - `ep->config_space().read(0x24)` = `bar_sizes[2] >> 32` = 2 (BAR2 高 32-bit, 表示 8GB)
- **v1.2 错误示例** (禁止): ~~`read(0x20) == 8589934592`~~ — 8589934592 = 0x2_0000_0000 超出 `uint32_t` 返回值, 必然失败

### Requirement: GMMU 异步状态机 + AXI Master Port (N2 iova 匹配 + 访问器)

The GmmuTLM SHALL 从 SimModule 改为 ChStreamModuleBase 派生, 添加 1 个 ChStream MasterPort 发 AxiMemBundle MEM_READ 请求读 PTE, 异步状态机 + -EAGAIN 重试; **N2 关键**: COMPLETE/WAIT 状态加 `pending_iova_` 匹配检查防乱序重试静默错配。**Where**: `include/tlm/gpu/gmmu_tlm.hh` + `src/tlm/gpu/gmmu_tlm.cc` (新)

The class SHALL:
- 派生自 `cpptlm::ChStreamModuleBase`
- 添加 1 个 ChStream MasterPort (`req_out_`, `resp_in_`, **v1.8 H1: PcieTlpBundle wire-format in minimal_v1**, AxiMemBundle 仅作 D3+ chip-internal 备用) + **访问器方法** `req_out()` / `resp_in()` (单端口模板要求)
- `translate()` 异步状态机实现 (per design.md §5):
  - `IDLE` → 校验失败返 `-EIO`; 否则发 AxiMemBundle MEM_READ(PTE), 转 WAIT, 返 `-EAGAIN`
  - `WAIT` → **N2 关键**: `iova != pending_iova_ || size != pending_size_` 返 `-EAGAIN` (不消费); 否则返 `-EAGAIN` (等 resp_in)
  - `COMPLETE` → **N2 关键**: 同 WAIT 检查, 匹配才消费, 返 0 + out_paddr
- **保留 §D3.8 同步签名** (`int translate(uint64_t, uint32_t, uint64_t&)`)
- 保留 `set_backing()` 作为 dual-mode legacy 同步路径
- 保留 v1.0 4KB 固定页 + 单级页表约束

The class SHALL NOT:
- 直持任何 framebuffer / memory backing 指针 (legacy backing 除外)
- 通过 set_backing 接收 raw pointer 用于生产路径

#### Scenario: GMMU translate 异步 (N2 iova 匹配)
- **WHEN**: `GmmuTLM gmmu; gmmu.set_pt_base(0x10000); gmmu.set_enabled(true);` (无 backing 注入)
- **THEN**: `gmmu.translate(0x1000, 4096, &phys)` 第一次返 `-EAGAIN` (state_=IDLE→WAIT, pending_iova_=0x1000); `gmmu.translate(0x2000, 4096, &phys)` (不同 iova) 返 `-EAGAIN` 不消费 (N2 乱序防御); 多 tick 后 `gmmu.translate(0x1000, 4096, &phys)` 返 0 + 正确 phys

#### Scenario: GMMU translate dual-mode legacy (测试路径)
- **WHEN**: `GmmuTLM gmmu; gmmu.set_backing(framebuffer_ptr_, framebuffer_size_);`
- **THEN**: `gmmu.translate(...)` 走 `translate_sync()`, 行为与 v1.0 逐字节一致

#### Scenario: 跨页 DMA 返 -EIO
- **WHEN**: `gmmu.translate(0x1000, 8192, &phys)` (跨页)
- **THEN**: 立即返 `-EIO`

### Requirement: SDMA 切型范围限定 (v1.3 B2) + retry driver + slot-2 resp (N3/N7) — **v1.3 B1+B2 修订**

The SdmaEngineTLM SHALL **minimal_v1 范围内 chip-internal 端口切型，board-level 端口保持**; **N3 retry driver**: tick 先 FIFO 重试 inflight_ 再收新 desc; **N7 slot-2 resp**: resp 从 `req_in_[PORT_MEM_OUT]` (slot 2) 消费; 保留 `set_vram_backdoor()` 作为 dual-mode 测试路径。**Where**: `include/tlm/gpu/sdma_engine_tlm.hh` + `src/tlm/gpu/sdma_engine_tlm.cc`

The class SHALL:
- **v1.3 B2 切型范围** (minimal_v1 限定):
  - `mem_in[PORT_MEM_IN]` + `mem_out[PORT_MEM_OUT]` → **AxiMemBundle** (chip-internal, 经 soc connections 连通 pcie_memory)
  - `desc_in[PORT_DESC_IN]` + `done_out[PORT_DONE_OUT]` + `host_out[PORT_HOST_OUT]` → **PcieTlpBundle** (board-level, minimal_v1 **不接线** — 经 BAR1 ring doorbell mmio 路径注入 descriptor + host_backdoor 注入数据 per N9)
  - **理由**: AxiMemBundle SHALL NOT 出现在 host↔board 端口（v1.3 B2 修订后的 AxiMemBundle Requirement）；混合端口模板在 minimal_v1 范围外（spec "不在范围"）
- `tick()` 先 `retry_inflight()` 再收 `desc_in` (N3 保 in-order)
- `process_inflight_step()` 状态机: PENDING_TRANSLATE → READY_TO_EMIT → WAITING_AXI_RESP → DONE
- **v1.3 B1 字段名** (per `dma_descriptor_mvp.hh:44-48`): PENDING_TRANSLATE 路径调 `translate_cb_(e.desc.host_iova, e.desc.size, phys)`（**非** `dst_iova_offset` / `len`）；DmaDescriptor 字段表: `dir / host_iova / vram_offset / size / tag`
- **N7 关键**: D2H/H2D 经 mem_out 发出后 resp 从 `req_in[PORT_MEM_OUT]` (slot 2) 消费 (ChStream 端口对称性)
- `set_vram_backdoor()` 保留为无条件 dual-mode 测试 API（既有 [sdma] 套件零逻辑改动）
- `set_translate_cb()` 接口不变 (SDMA → GMMU::translate, -EAGAIN 重试)

The class SHALL NOT (生产路径):
- 直持任何 framebuffer / memory backing 指针
- 通过 set_vram_backdoor 接收 raw pointer 用于生产路径
- **v1.3 B1 撤销**: 引用 `e.desc.dst_iova_offset` 或 `e.desc.len`（编译失败）
- **v1.3 B2 撤销**: `desc_in`/`done_out`/`host_out` 在 minimal_v1 内切 AxiMemBundle（与 spec 内 AxiMemBundle 边界矛盾）

#### Scenario: SDMA H2D 经 retry driver (N3) 写 PcieMemoryDevice (chip-internal mem_out AxiMemBundle)
- **WHEN**: SDMA descriptor 注入 desc_in (board-level PcieTlpBundle) → 转 inflight_ (PENDING_TRANSLATE)
- **THEN**: 多 tick 内 retry_inflight 循环: `translate_cb_(e.desc.host_iova, e.desc.size, phys)` (**v1.3 B1 字段名**) → -EAGAIN 留 PENDING_TRANSLATE → GMMU 收 resp → 下 tick translate 返 0 → READY_TO_EMIT → emit mem_out AxiMemBundle MEM_WRITE (per v1.3 B2 chip-internal 切型) → pcie_memory.req_in[0] → resp_out[0] → sdma.req_in[2] (slot 2, PORT_MEM_OUT) 消费 (N7) → DONE → emit done_out (PcieTlpBundle, board-level, v1.3 B2 不切)

#### Scenario: SDMA D2H 经 slot-2 resp 路径 (N7) — chip-internal mem_in AxiMemBundle
- **WHEN**: SDMA D2D emit mem_out MEM_READ → pcie_memory.resp_out[0] 返 MEM_READ_RESP
- **THEN**: sdma.req_in[2] (slot 2, PORT_MEM_OUT) 消费 (N7, AxiMemBundle) → sdma emit host_out + done (PcieTlpBundle, board-level, v1.3 B2 不切)

#### Scenario: SDMA translate -EAGAIN 重试 (N3 in-order) — 字段名 v1.3 B1
- **WHEN**: SDMA descriptor 处理中, `translate_cb_(e.desc.host_iova, e.desc.size, phys)` 返 `-EAGAIN`
- **THEN**: descriptor 留 inflight_ (PENDING_TRANSLATE), 不 emit done, 下 tick retry_inflight 再试; in-order 保证 (FIFO 重试)

#### Scenario: SDMA dual-mode legacy (测试路径, 既有 [sdma] 套件兼容)
- **WHEN**: `SdmaEngineTLM sdma; sdma.set_vram_backdoor(ptr, size);`
- **THEN**: `process_inflight_step()` 检测 `has_vram_backdoor() == true` → 直接 `memcpy(vram_backdoor_, e.host_buf, e.desc.size)` (**v1.3 B1**); 既有 [sdma] 套件零逻辑改动

### Requirement: DGpuBoard 双 size 字段 (v1.4 B11 — BAR1 窗口 vs VRAM 大小区分)

The DGpuBoard SHALL 持有两个独立的 size 字段：`bar1_window_size_`（BAR1 窗口大小，来自 `bar_sizes[1]`）与 `vram_size_`（VRAM 真 backing 大小，来自 `bar_sizes[2]`）。**Where**: `src/tlm/gpu/dgpu_board_shell.{hh,cc}`

The class SHALL:
- `bar1_window_size_` 取自 `bar_sizes[1]`（保持现行 framebuffer 16MB 默认值或 `framebuffer_size_bytes` override）
- `vram_size_` 取自 `bar_sizes[2]`（若 `bar_sizes[2] == 0` 或缺省 → **不分配 vram**，device 注入返 -ENODEV）；若 `memory_routing_enabled=true && bar_sizes[2]==0` → fail-fast 报错（配置矛盾）
- BAR1 fast-path bound (`dgpu_board_shell.cc:306,447`) 用 `bar1_window_size_`（**非** `framebuffer_size_`）
- 5 消费者注入 size (memory/sdma/gmmu/pcie_memory/backdoor) 用 `vram_size_`
- 4 个 `set_*_store` 调用点必须传同一 `vram_size_` 变量，**禁各自派生**

The class SHALL NOT:
- 用 `framebuffer_size_` 同时承担 BAR1 窗口 bound 和 VRAM 注入 size
- 让 `bar_sizes[2]==0` 静默 fallback 到 `kDefaultVramSize`（应 fail-fast 或返 -ENODEV）

#### Scenario: BAR1 越窗检测 (v1.4 B11 验证)
- **WHEN**: host ABI `cpptlm_emulator_mmio_read(1, offset=0x2000000, buf, 8)`（24MB > bar1_window_size_=16MB）
- **THEN**: 返 `-EINVAL`（**不**越窗成功; 不污染 vram[24MB]）

#### Scenario: BAR2 全 8GB 访问 (v1.4 B11 验证)
- **WHEN**: host ABI `cpptlm_emulator_mmio_read(2, offset=0x800000, buf, 8)`（8MB < vram_size_=8GB）
- **THEN**: 返 `vram_storage_[8MB]` 数据（不返 `-EINVAL`）

#### Scenario: 5 消费者共享同一 vram (v1.4 B11 验证)
- **WHEN**: `DGpuBoard::bind_memory_backings()` 调 `pcie_memory->set_backing_store(vram_storage_.get(), vram_size_)`
- **THEN**: `MemoryTLM.backing_ptr_ == pcie_memory->backing_ptr_ == vram_storage_.get()`（同一指针）; 4 个 size 参数都是 `vram_size_`

### Requirement: DGpuBoard::bind_memory_backings 条件注入 (N12)

The DGpuBoard::bind_memory_backings() SHALL **条件注入**: soc 含 pcie_memory 时走 AXI 路径, 否则保留 legacy framebuffer 注入 (兼容 dgpu_board_v1.json 等无 pcie_memory 配置)。**Where**: `src/tlm/gpu/dgpu_board_shell.cc`

The function SHALL:
- 若 soc 含 `pcie_memory` 实例:
  - 调用 `pcie_ep_->set_memory_device(pcie_memory_)`
  - 不调 `gmmu_->set_backing(framebuffer_ptr_, framebuffer_size_)` (生产路径走 mem_out AXI)
  - 不调 `sdma_->set_vram_backdoor(framebuffer_ptr_, framebuffer_size_)` (生产路径走 mem_out AXI)
- 若 soc **无** `pcie_memory` 实例 (e.g., dgpu_board_v1.json):
  - 保留 legacy: `gmmu_->set_backing(framebuffer_ptr_, framebuffer_size_)`
  - 保留 legacy: `sdma_->set_vram_backdoor(framebuffer_ptr_, framebuffer_size_)`
- 保留 `memory_->set_backing_store(framebuffer_ptr_, framebuffer_size_)` (无论哪种配置)

#### Scenario: 含 pcie_memory 时不注入 framebuffer 给 GMMU/SDMA (N12)
- **WHEN**: minimal_v1 JSON 加载后 bind_memory_backings 执行
- **THEN**: `gmmu_.backing_` 为 nullptr (字段保留, 不注入); `sdma_.vram_backdoor_` 为 nullptr (字段保留, 不注入)

#### Scenario: 无 pcie_memory 时保留 legacy framebuffer 注入 (N12)
- **WHEN**: dgpu_board_v1.json (无 pcie_memory) 加载后 bind_memory_backings 执行
- **THEN**: `gmmu_.backing_ == framebuffer_ptr_` (legacy 注入保留); `sdma_.vram_backdoor_ == framebuffer_ptr_` (legacy 注入保留); 既有 [sdma][h2d] 测试全绿

#### Scenario: shutdown 时先置 nullptr 避免 dangling (N11)
- **WHEN**: `DGpuBoard::shutdown()`
- **THEN**: 先 `pcie_ep_->set_memory_device(nullptr)` 再销毁 soc; PcieEndpointIP 析构时 memory_device_ 已是 nullptr

### Requirement: minimal_v1 JSON 配置扩展 (N6 启用 BAR2)

The minimal_v1 configuration SHALL 新增 `pcie_memory` 顶层模块 + 2 条 chip-internal AXI connections + 启用 BAR2 (`memory_routing_enabled=true` + `bar_sizes` 三元素)。**Where**: `configs/dgpu_soc_minimal_v1.json`

The config SHALL:
- 顶层新增 `"memory_routing_enabled": true`
- soc modules 数组新增 `pcie_memory` 模块 (`type: PcieMemoryDevice`, `params.capacity_gb: 8`)
- `pcie_ep.params.bar_sizes` 扩展为 `[4096, 16777216, 8589934592]`
- soc connections 数组新增 2 条: `sdma.2 → pcie_memory.0` + `gmmu.0 → pcie_memory.1`

**N6 关键**: BAR2 启用依赖 T0.4 PcieConfigSpace 实现 BAR 寄存器从 bar_sizes 生成; 无此实现时 BAR2 启用但 host 枚举看不到 BAR2 (size=0)。

#### Scenario: minimal_v1 JSON 加载后 soc 拓扑包含 pcie_memory
- **WHEN**: `ModuleFactory::instantiateAll("configs/dgpu_soc_minimal_v1.json")`
- **THEN**: soc 内有 **5** 个模块 (pcie_ep + pcie_memory + sdma + gmmu + completion) — v1.6 F12 MemoryTLM 移除后

#### Scenario: minimal_v1 JSON 加载后 chip-internal AXI connections 自动绑定
- **WHEN**: soc connections 处理
- **THEN**: sdma.2 → pcie_memory.0; gmmu.0 → pcie_memory.1; N7 slot-2 resp 路径正确

### Requirement: Coherence 域边界 (UsrLinuxEmu 端职责)

The coherent/non-coherent 系统内存语义 **SHALL NOT** 在 CppTLM 建模; 由 UsrLinuxEmu 端通过模拟 Linux 内部分配器 API (CMA / vmalloc / kmalloc) 实现不同类型系统内存的 malloc。**Where**: `specs/driver-visible-minimal-soc/spec.md` (本 spec 显式声明)

#### Scenario: CppTLM 侧无任何 coherence 字段/参数
- **WHEN**: grep `coheren` 于 `include/tlm/gpu/pcie_memory_device.*` `include/bundles/axi_mem_bundles_tlm.hh`
- **THEN**: 无匹配 (除本 Requirement 文本引用)

### Requirement: 驱动视角完整最小设备 (v1.2 修订)

The minimal SoC SHALL 让 driver 通过 15 ABI 函数看到完整的最小设备视图。**Where**: `test/test_minimal_soc_driver_visible_e2e.cc`

The driver-visible surface SHALL (v1.2 P1 修订后):
- `cpptlm_emulator_pcie_config_read(0x00, 4)` 返回 **vendor_id=0x10DE + device_id=0x1234** (来自 PcieConfigSpace)
- `cpptlm_emulator_mmio_write(2, off, val, len)` / `mmio_read(2, ...)` 经 BAR2 fast-path 读写 `PcieMemoryDevice::memory_backing_` (N6 依赖 BAR 寄存器生成)
- `cpptlm_emulator_backdoor_write(0, off, data, len)` / `backdoor_read(0, ...)` 经 DGpuBoard framebuffer_ptr_ 直读直写
- SDMA descriptor 注入 desc_in → mem_out (AxiMemBundle) → pcie_memory.req_in_[0] → memory_backing_ → resp 从 slot-2 消费 (N7) → done emit

#### Scenario: Driver config read 看到 vendor_id + device_id (N6)
- **WHEN**: driver 调 `pcie_config_read(0x00, 4)`
- **THEN**: 返 0x123410DE (vendor=0x10DE + device=0x1234, **来自 PcieEndpointIP::PcieConfigSpace::DEFAULT_VENDOR_ID/DEFAULT_DEVICE_ID + init() 写 regs_[0]**)

#### Scenario: Driver BAR2 MMIO write/read round-trip
- **WHEN**: `mmio_write(2, 0x1000, &data, 8)` + `mmio_read(2, 0x1000, buf, 8)`
- **THEN**: 返 data (从 PcieMemoryDevice::memory_backing_[0x1000..0x1007])

#### Scenario: Driver 经 BAR2 写 PTE + GMMU 经 AXI 读 PTE
- **WHEN**: `mmio_write(2, pte_off, &pte, 8)` 写入 PTE → SDMA descriptor → gmmu.translate() 经 AxiMemBundle MEM_READ 读 PTE
- **THEN**: gmmu.translate() 异步状态机完成 (IDLE→WAIT→COMPLETE), 返 0 + phys_offset

#### Scenario: Driver backdoor write/read round-trip (BAR1 framebuffer)
- **WHEN**: `backdoor_write(0, 0x1000, &data, 8)` + `backdoor_read(0, 0x1000, buf, 8)`
- **THEN**: 返 data (从 DGpuBoard::framebuffer_ptr_[0x1000])

#### Scenario: SDMA H2D 经 minimal_v1 配置写入 PcieMemoryDevice (N7)
- **WHEN**: driver 通过 desc_in 注入 SDMA H2D descriptor (iova=0x10000, len=4096, host_buf=src)
- **THEN**: 多 tick 后 PcieMemoryDevice::memory_backing_[phys_offset : phys_offset+4096] == src; SDMA done emit (AxiMemBundle DMA_DONE); resp 从 slot-2 消费 (N7)

### Requirement: 15 ABI Functions Unchanged

The 15 C ABI functions + 4 callback typedef in `include/abi/cpptlm_emulator.h` SHALL remain byte-for-byte identical (no signature changes, no additions, no removals)。**Where**: `include/abi/cpptlm_emulator.h`

#### Scenario: ABI header diff empty
- **WHEN**: 比较 D-AXI 实施前后 `include/abi/cpptlm_emulator.h` (`git diff HEAD -- include/abi/cpptlm_emulator.h`)
- **THEN**: 输出为空

#### Scenario: Function count = 15
- **WHEN**: `grep -c "^uint32_t cpptlm_emulator_\|^int cpptlm_emulator_\|^void cpptlm_emulator_\|^cpptlm_emulator_t\* cpptlm_emulator_" include/abi/cpptlm_emulator.h`
- **THEN**: 输出 15

### Requirement: Freeze Surface Untouched

D-AXI SHALL NOT modify any freeze surface file (pcie_endpoint_tlm.h / cpptlm_emulator.h / pcie_display_device.hh / pcie_bundles_tlm.hh)。**Where**: `include/tlm/gpu/pcie_endpoint_tlm.h` + `include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_display_device.hh` + `include/bundles/pcie_bundles_tlm.hh`

#### Scenario: pcie_endpoint_tlm.h not modified
- **WHEN**: `git diff HEAD -- include/tlm/gpu/pcie_endpoint_tlm.h`
- **THEN**: 输出为空 (PcieEndpointTLM 4 端口冻结头保持不变)

#### Scenario: cpptlm_emulator.h not modified
- **WHEN**: `git diff HEAD -- include/abi/cpptlm_emulator.h`
- **THEN**: 输出为空 (ABI 冻结)

#### Scenario: pcie_display_device.hh not modified
- **WHEN**: `git diff HEAD -- include/tlm/gpu/pcie_display_device.hh`
- **THEN**: 输出为空 (D1 保持不变)

#### Scenario: pcie_bundles_tlm.hh not modified
- **WHEN**: `git diff HEAD -- include/bundles/pcie_bundles_tlm.hh`
- **THEN**: 输出为空 (PcieTlpBundle board-level 边界保持不变; D-AXI 新建 AxiMemBundle 严格分离)

