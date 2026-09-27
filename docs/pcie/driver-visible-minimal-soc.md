# D-AXI Driver-Visible Minimal SoC — 实施笔记 (v1.4)

> **状态**: ✅ v1.4 P0 阻塞修正完成（2026-09-26）
> **配套 openspec**: `openspec/changes/cpptlm-driver-visible-minimal-soc/`
> **设计稿**: 4 文件（`proposal.md` / `design.md` / `tasks.md` / `specs/driver-visible-minimal-soc/spec.md`）
> **配套 ADRs**: ADR-088 §D5（ABI 冻结）, ADR-SOC-21（V3.1-Rev2.0 拓扑）, ADR-SOC-18/14（PCIe EP / DMA）
> **配套设备笔记**: `docs/pcie/display-device-mvp.md`（D1） — 同为 PCIe 设备 MVP 链
> **跨仓镜像**: ArchForge `docs/architecture/19a-driver-visible-minimal-soc.md`
> **Oracle 复审 session**: `ses_f21e9e147ffeeuTmILtLvnBvTb` (P0), `ses_f21924eb3ffeb8SMW0SPMGLVf1` (P1), `ses_f21351631ffew5Q5lHy9D0AC4q` (P2 v1.3 Option D), `ses_f212cf80cffeK93MaMjNxfjvjJ` + `ses_f212cfac0ffePU5YScA9PyW0ld` (Metis 二轮 v1.4)

## 1. 范围

D-AXI（Driver-Visible AXI Minimal SoC）在 CppTLM 中提供**第一类 driver-visible SoC 拓扑**——一个完整的最小可驱动 dGPU SoC，让 UsrLinuxEmu 端 Linux driver 通过标准 PCIe BAR + 内部 chip-internal AXI 总线看到真实、可驱动验证的最小 dGPU 抽象。

**核心目标**：
- **driver 视角完整**：15 ABI 函数覆盖 BAR 枚举 / mmio 读写 / backdoor 特权 / DMA 完成中断
- **架构清洁**：chip-internal AXI 与 board-level PCIe 严格分离
- **单一 VRAM 真源**：5 消费者共享同一 backing，所有路径在物理地址层一致（driver 视角"single VRAM"语义）
- **可扩展性**：D3 GPU 计算 / 真 HBM timing 可在 `handle_slave_port ↔ backing_ptr_` seam 插入

## 2. 设计成果

### 2.1 6 模块拓扑（v1.4 实施后）

```
┌──── DGpuSoc (顶层 SimModule 容器, 1 个 JSON 模块: "DGpuSoc") ────┐
│                                                                      │
│  ┌─── pcie_ep (PcieEndpointIP, 4 端口冻结) ──────────────────┐    │
│  │  ├─ memory_device_: PcieMemoryDevice* (raw ptr, v1.3 N8)  │    │
│  │  ├─ config_space: vendor=0x10DE, device=0x1234           │    │
│  │  ├─ BAR0 (4KB MMIO 控制寄存器)                            │    │
│  │  ├─ BAR1 (16MB 窗口, framebuffer_window)                  │    │
│  │  ├─ BAR2 (8GB, vram aperture, 64-bit 双 dword)            │    │
│  │  └─ tick 不转发 memory_device_ (v1.3 N8 防双 tick)        │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                            │                                          │
│                            │ set_memory_device(raw ptr)              │
│                            ▼                                          │
│  ┌─── pcie_memory (PcieMemoryDevice, **v1.4 后为 PCIe 外观层**)─┐  │
│  │  ├─ registers_ (BAR0, 4KB MMIO 控制寄存器, owned)          │    │
│  │  ├─ backing_ptr_ (注入 → board.vram_storage_, v1.4 B7)   │    │
│  │  ├─ backing_size_ (注入 = vram_size_, 非常量)             │    │
│  │  ├─ backing_mutex_ (host/sim 并发保护, v1.4 B10)         │    │
│  │  ├─ 2 AxiMemBundle SlavePorts:                            │    │
│  │  │     port0 ← sdma.mem_out  (PORT_SDMA, AxiMem 4KB)      │    │
│  │  │     port1 ← gmmu.req_out  (PORT_GMMU, AxiMem 4KB)     │    │
│  │  ├─ 单 adapter_ (v1.3 B3 撤 N4 双 adapter)               │    │
│  │  ├─ has_memory_backing() = backing_ptr_ != nullptr        │    │
│  │  └─ 未注入 → memory_read/write 返 -ENODEV (v1.4 B12)      │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌─── sdma (SdmaEngineTLM, 5 端口, **混合 wire-format**) ────┐    │
│  │  chip-internal (AxiMemBundle):                            │    │
│  │     mem_in[PORT_MEM_IN]  ← pcie_memory.resp_out[0]      │    │
│  │     mem_out[PORT_MEM_OUT] → pcie_memory.req_in[0]      │    │
│  │  board-level (PcieTlpBundle, 保持):                       │    │
│  │     desc_in[PORT_DESC_IN]   (minimal_v1 不接线, 经 BAR1 ring doorbell mmio 注入)│    │
│  │     done_out[PORT_DONE_OUT] (minimal_v1 不接线)            │    │
│  │     host_out[PORT_HOST_OUT] (minimal_v1 不接线, host 侧数据由 N9 host_backdoor 注入)│
│  │  ├─ retry driver FIFO (v1.3 N3) + slot-2 resp (v1.3 N7)   │    │
│  │  ├─ 字段名 host_iova / size / vram_offset (v1.3 B1)       │    │
│  │  └─ set_vram_backdoor 注入 vram_ptr_ (dual-mode legacy)  │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌─── gmmu (GmmuTLM, 1 MasterPort) ──────────────────────────┐    │
│  │  ├─ req_out → pcie_memory.req_in[1] (AxiMem MEM_READ PTE)  │    │
│  │  ├─ 状态机: IDLE → WAIT → COMPLETE (v1.3 N2)              │    │
│  │  ├─ pending_iova_ 匹配 (防乱序重试, v1.3 N2)              │    │
│  │  ├─ -EAGAIN 重试契约 (与 sdma retry driver 对接)          │    │
│  │  └─ set_backing 注入 vram_ptr_ (dual-mode legacy)          │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌─── memory (MemoryTLM, 通用 backing, 不变) ──────────────┐    │
│  │  ├─ 1 单端口 CacheReqBundle (cache 下游)                  │    │
│  │  ├─ backing_ptr_ 注入 vram_ptr_ (共享 vram, v1.4 B7)       │    │
│  │  ├─ size_cap_ = capacity_gb = 1GB (v1.4 不改, §6 显式)    │    │
│  │  └─ v2.2 backing-store 路径: 零时 memcpy (8B/单事务)       │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  ┌─── completion (CompletionRingTLM, 不变) ──────────────────┐    │
│  └─────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────┘
```

### 2.2 单一 VRAM 真源 + 5 消费者共享（v1.4 核心架构）

```
DGpuBoard::vram_storage_ (8GB, std::unique_ptr<uint8_t[]>)
   │   ▲─ default-init, Linux lazy commit, stable pointer, 永不 realloc
   │
   ├── vram_ptr_ (= vram_storage_.get())
   │   ├── vram_size_        (= bar_sizes[2] = 8GB)
   │   └── bar1_window_size_  (= bar_sizes[1] = 16MB)
   │
   └── 5 consumers share同一指针:
       ┌────────────────┬─────────────┬─────────────┐
       ▼                ▼             ▼             ▼
   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
   │ backdoor │  │ BAR1     │  │ MemoryTLM│  │ SDMA/GMMU│
   │ direct   │  │ fast-path│  │ backing  │  │ legacy   │
   │ (host特  │  │ 16MB 窗 │  │ ptr_     │  │ ptr_     │
   │  权)    │  │  bound=  │  │          │  │          │
   │          │  │ bar1_win │  │          │  │          │
   └──────────┘  └──────────┘  └──────────┘  └──────────┘
                            │
                            ▼
                ┌───────────────────────┐
                │ PcieMemoryDevice      │
                │ backing_ptr_ ← 注入  │
                │ backing_size_ =       │
                │   vram_size_ (8GB)    │
                └───────────────────────┘
                            │
                            ▼ chip-internal AXI
                ┌───────────────────────┐
                │ sdma.mem_out   → port0 │
                │ gmmu.req_out   → port1 │
                └───────────────────────┘
```

**关键不变式**：5 路径**字节级同步**。driver 写 BAR2 PTE → SDMA/GMMU 经 chip-internal AXI 读同一物理地址 → 命中同一 backing 字节。

### 2.3 driver 视角完整数据流（6 步闭环）

```
Step 1 — PCIe 枚举 (host thread)
   cpptlm_emulator_pcie_config_read(0x00, 4)
   → DGpuBoard → PcieConfigSpace → 返 0x123410DE (vendor 0x10DE + device 0x1234)

Step 2 — BAR1 doorbell 触发 SDMA (host thread)
   cpptlm_emulator_mmio_write(1, 0x10010000, &wptr, 4)
   → DGpuBoard::mmio_write → doorbell 路径优先 (BEFORE bound check)
   → sdma_engine 收 wptr

Step 3 — driver 经 BAR2 写 PTE (host thread)
   cpptlm_emulator_mmio_write(2, pte_off, &pte, 8)
   → DGpuBoard::mmio_write → memory_routing_enabled_=true && bar==2
   → ep->memory_device().memory_write(pte_off, &pte, 8)  [mutex 保护]
   → memcpy(vram_storage_.get() + pte_off, &pte, 8)  [single backing]

Step 4 — GMMU translate 经 chip-internal AXI 读 PTE (sim thread)
   sdma retry_inflight → translate_cb_(host_iova, size, &phys)
   → gmmu.translate(iova, size, &phys)
     - state_=IDLE → 发 AxiMemBundle MEM_READ (pte_addr)
     - pcie_memory.handle_slave_port(1) → memory_read(pte_addr, ..., 8)
     - memcpy(vram_storage_.get() + pte_addr, pte_buf, 8)  [同一 backing!]
     - 返 MEM_READ_RESP → gmmu.resp_in
     - state_=COMPLETE, out_paddr = phys

Step 5 — SDMA H2D 经 chip-internal AXI 写 VRAM (sim thread)
   sdma retry → translate 返 0 → READY_TO_EMIT
   → emit mem_out[PORT_MEM_OUT] AxiMemBundle MEM_WRITE (phys_offset, host_buf)
   → pcie_memory.handle_slave_port(0) → memory_write(phys_offset, host_buf, len)
   → memcpy(vram_storage_.get() + phys_offset, host_buf, len)  [同一 backing!]
   → 返 MEM_WRITE_RESP → sdma.req_in[2] (slot 2, PORT_MEM_OUT)  [N7]
   → sdma emit done_out (PcieTlpBundle, board-level) + host_backdoor 注入 [N9]

Step 6 — driver backdoor 读 (host thread, host-side 特权)
   cpptlm_emulator_backdoor_write(0, off, data, 8)
   → DGpuBoard::backdoor_write → framebuffer_ptr_[off] 直写
   (framebuffer_ptr_ = vram_storage_.get()，仅 16MB 窗口内有效)
```

**关键观察**：driver、GMMU、SDMA、backdoor 四个路径**全部打到同一 vram_storage_ 字节**。Step 3 driver 写 PTE → Step 4 GMMU 读 PTE 是同一物理地址的读写——这才是"driver 可驱动 SoC"的真正闭环。

## 3. 关键设计决策

### 3.1 单一 VRAM 真源在 DGpuBoard（v1.4 B7）

**选择**：删 `PcieMemoryDevice::memory_backing_` + `ensure_memory_backing_allocated`；新增 `set_backing_store(uint8_t*, uint64_t)`（对齐 `memory_tlm.hh:82` 先例）；backing 归 `DGpuBoard::vram_storage_` 持有。

**理由**：
- **消除 dual-VRAM bug**：原 framebuffer_storage_ (16MB) + PcieMemoryDevice::memory_backing_ (8GB) 是两块独立 vector，driver 视角不一致 → 拆雷
- **避免 8GB memset + RSS 爆炸**：`unique_ptr<uint8_t[]>` default-init（无 zero-fill），Linux lazy commit（VA 占用，RSS 不增长）
- **stable pointer**：`unique_ptr` 永不变（无 realloc），注入 5 个 consumer 不会悬垂
- **D3 演进 seam 干净**：未来 `handle_slave_port ↔ backing_ptr_` 之间插入真 VramControllerTLM/MemoryClusterTLM，接口零变更

### 3.2 bound = injected backing_size_（v1.4 B8）

**选择**：`memory_read/write` bound = injected `backing_size_`（**非** `kDefaultMemSize` 常量）。

**理由**：
- 小测试 backing（如 4KB）注入后，写 ≥ 4KB 必须返 `-EINVAL` 而非越界 memcpy
- `kDefaultMemSize` 降级为 board 派生默认值（来自 `bar_sizes[2]`），device 内不再引用做边界

### 3.3 SLVERR 传播（v1.4 B9）

**选择**：`handle_slave_port` 检查 `memory_read/write` 返回值；失败置 `resp.resp.write(1)` (SLVERR)；**不**静默 OKAY。

**理由**：当前 D2 v1.1 实现不检查返回值 — nullptr backing 时 SLVERR 被吞，driver 读到垃圾数据，静默损坏。

### 3.4 mutex 保留（v1.4 B10）

**选择**：保留 B4 的 `std::mutex backing_mutex_`（**去** lazy 保 mutex）。

**理由**：
- `sim_thread_` + host mmio/backdoor 真实并发（`dgpu_board_shell.cc:171-173`）
- `sim_thread_` 内 MemoryTLM tick 读 + host thread backdoor_write 写同一 backing = 数据竞争（UB）
- mutex 仅用于并发保护；backing_ptr_ 永不变（unique_ptr stable pointer），mutex 不会因 lazy alloc 而升级

### 3.5 双 size 字段（v1.4 B11）

**选择**：拆分为 `bar1_window_size_` (BAR1 窗口 = `bar_sizes[1]`) + `vram_size_` (VRAM 真大小 = `bar_sizes[2]`)。

**理由**：
- BAR1 是 16MB 窗口（PCIe 物理 BAR 内地址范围）
- VRAM 是 8GB backing（实际内存）
- BAR1 fast-path bound 用前者（不允许 host 越窗）；5 消费者注入 size 用后者
- 否则 host 经 BAR1 可写 ≥ 16MB 地址，违反 PCIe 语义

### 3.6 chip-internal AXI vs board-level PCIe 边界（v1.3 B2）

**选择**：SDMA 5 端口**混合 wire-format**：
- `mem_in` / `mem_out` → `AxiMemBundle` (chip-internal)
- `desc_in` / `done_out` / `host_out` → `PcieTlpBundle` (board-level, minimal_v1 不实际接线)

**理由**：
- `AxiMemBundle` 是 4KB payload 的 chip-internal AXI 协议载体
- `PcieTlpBundle` 是 TLP 级别的 board-level 协议载体
- AxiMemBundle SHALL NOT 出现在 host↔board 端口（混用会污染边界语义）
- minimal_v1 经 BAR1 ring doorbell mmio 路径注入 descriptor（不用 `desc_in` 端口），host 数据由 N9 host_backdoor 注入

### 3.7 Coherence 域边界（U1 用户澄清）

**选择**：coherent/non-coherent 系统内存语义 **不在 CppTLM 建模**。

**理由**：UE 端（UsrLinuxEmu）通过模拟 Linux 内部分配器 API（CMA / vmalloc / kmalloc）实现不同类型系统内存的 malloc。spec Requirement "Coherence 域边界" 显式声明。

## 4. 7 条实施铁律

```
1. 分配: std::unique_ptr<uint8_t[]>(new uint8_t[N]) default-init
   禁 vector::resize / 禁 new uint8_t[N]() 零初始化

2. 所有 bound = injected backing_size_
   kDefaultMemSize 仅 board 派生默认值, device 内禁引用做边界

3. bar1_window_size_ (BAR1 窗口) vs vram_size_ (VRAM) 拆分
   BAR1 fast-path 用前者; 5 消费者注入 size 用后者

4. set_backing_store 仅在 sim_thread 启动前调一次（保持 Inv-2）
   禁运行时再注入

5. kRegMemSizeLo/Hi 读 injected backing_size_
   未注入时报 0

6. 未注入 → memory_read/write 返 -ENODEV
   has_memory_backing() = backing_ptr_ != nullptr

7. handle_slave_port 检查 memory_read/write 返回值
   失败置 resp=1 (SLVERR)
```

## 5. 与现有 MVP 链的关系

| 设备 | 角色 | 文档 |
|------|------|------|
| **D1** PcieDisplayDevice | 显示 IO 设备 MVP（4KB MMIO + 32MB FB + VBLANK MSI-X） | `docs/pcie/display-device-mvp.md` |
| **D-AXI** PcieMemoryDevice + 6 模块 SoC | 第一类 driver-visible 完整 SoC MVP（本文档） | `docs/pcie/driver-visible-minimal-soc.md` |
| D2 v1.1 | PcieMemoryDevice 独立类版本（已 archive，被 D-AXI 取代） | openspec/changes/2026-09-20-cpptlm-pcie-memory-device-mvp/ (archive) |
| D3 | GMMU PoC / StreamingMultiprocessor | D-AXI 完成后启动 |

## 6. 遗留议题（D3 收编，不阻塞 v1.4）

| 议题 | 延期理由 | D3 收编计划 |
|------|---------|------------|
| **D1 PcieDisplayDevice 32MB FB 同构** | `pcie_display_device.hh` 是冻结面（per ADR-088 §D5 + spec "Freeze Surface Untouched"）；minimal_v1 `display_routing_enabled=false` 不触发 | D3 按 Option D 模式（`set_backing_store(ptr, size)`）将 D1 FB 也归 board 持有 |
| **BAR1 doorbell offset 0x10010000 > BAR1 16MB 实窗** | 现行 `dgpu_board_shell.cc:306-447` 在 BAR1 fast-path bound 检查**之前**先匹配 doorbell 路径（不依赖 BAR1 实窗大小），实际不触发越窗；该常量（`kBar1DoorbellOffset`，`dgpu_board_shell.hh:171`）是 driver 测试虚拟地址合成，非 PCIe 物理 BAR 内偏移；v1.4 拆分 `bar1_window_size_` 后该语义更清晰但**不修**——避免改常量弄断既有 `[sdma][doorbell]` 测试 | — |
| **MemoryTLM `capacity_gb=1` vs `vram_size_=8GB` 交互** | minimal_v1 JSON `memory.params.capacity_gb=1` 是 CPU 侧 cache 路径的合理容量；CPU 侧 cache 经 MemoryTLM 访问 ≥ 1GB 仍按现 v2.2 行为返 `error_code=1` (OUT_OF_RANGE)；BAR2 路径不受影响（独立经 PcieMemoryDevice 转发到 vram）；spec Requirement 显式声明 "single VRAM 对 MemoryTLM 消费者为 `min(vram_size_, MemoryTLM.size_cap_)`"，即 memory 视角最大 1GB、driver 视角最大 8GB | D3 引入真 VramController 时一并评估 capacity 同步策略 |

## 7. D3 演进 seam（v1.4 已就位）

| D3 需求 | 插入点 | 接口影响 |
|---------|--------|---------|
| 引入 VramControllerTLM (HBM timing) | `pcie_memory_device.cc::handle_slave_port()` ↔ `backing_ptr_` 之间 | **零 API 变更**（controller 替代直接 memcpy；端口/adapter/注册全不动） |
| 引入 MemoryClusterTLM (多通道 HBM) | `dgpu_board_shell.cc::bind_memory_backings()` 注入点 | **零 API 变更**（controller 变第 6 指针消费者） |
| D1 Display FB 收编 | `dgpu_board_shell.cc` 注入点（与 pcie_memory 并列） | D3 立项 |
| GPU 计算 (StreamingMultiprocessor) | 新增 `tlm/gpu/sm/` 子模块 | 与 v1.4 解耦 |
| 多 PcieMemoryDevice 实例 | minimal_v1 单实例；多实例需 bound/allocator 重审 | D3+ |

## 8. v1.4 阻塞修正索引（B1-B13）

| ID | 类别 | 位置 |
|----|------|------|
| B1 | 字段名 | SDMA `e.desc.dst_iova_offset/len` → `host_iova/size/vram_offset`（per `dma_descriptor_mvp.hh:44-48`） |
| B2 | spec 内部矛盾 | SDMA 切型范围限定（仅 chip-internal 切型） |
| B3 | 伪命题 fix | 撤 N4 双 adapter，单 `adapter_` |
| B4 | v1.4 REVERTED | v1.3 计划"lazy alloc + mutex"；v1.4 改为所有权上提（去 lazy 保 mutex） |
| B5 | 验收不可满足 | 64-bit BAR 双 dword 编码 |
| B6 | 注册缺失 | PcieMemoryDevice + GmmuTLM 双注册 |
| **B7** | **v1.4 架构根因** | **单一 VRAM backing 所有权归 DGpuBoard** |
| **B8** | **v1.4 bound 语义** | **`memory_read/write` bound = injected `backing_size_`** |
| **B9** | **v1.4 SLVERR 传播** | **`handle_slave_port` 检查 `memory_read/write` 返回值** |
| **B10** | **v1.4 线程契约** | **保留 mutex（保护 host/sim 并发）** |
| **B11** | **v1.4 双 size** | **`bar1_window_size_` vs `vram_size_` 拆分** |
| **B12** | **v1.4 -ENODEV** | **未注入 → -ENODEV；`kRegMemSizeLo/Hi` 读 injected size** |
| **B13** | **v1.4 24-case 处置** | **4 处语义反转 + `[minimal_dgpu_soc]` 添加 BAR2** |

## 9. 验证清单

### 设计阶段（已完成）

- [x] `openspec validate cpptlm-driver-visible-minimal-soc --strict` PASS
- [x] `openspec validate --changes --strict` PASS (5/5)
- [x] v1.4 P0 修正 (B1-B13) 全部应用，62 处标注
- [x] D3 演进 seam 已预留
- [x] 3 项遗留议题延期理由显式声明
- [x] 7 条实施铁律已写入

### 实施阶段（待执行）

- [ ] T0: 表征 + AxiMemBundle 经真实 StreamAdapter round-trip（v1.3 N1）
- [ ] T0.4: PcieConfigSpace 64-bit 双 dword BAR 寄存器生成（v1.3 B5）
- [ ] T1: PcieMemoryDevice v1.4 重构（注入式 backing + 2 SlavePort + single adapter + 删 backing vector）
- [ ] T1.6: PcieEndpointIP raw pointer + 删 `memory_device_->tick()`（v1.3 N8）
- [ ] T1.7: `bind_memory_backings()` 条件注入 + 拆分 `bar1_window_size_` / `vram_size_`（v1.3 N12 + v1.4 B11）
- [ ] T2: GMMU 异步状态机 + pending_iova_ 匹配（v1.3 N2）
- [ ] T2.4: PcieMemoryDevice + GmmuTLM 双注册（v1.3 B6）
- [ ] T3: SDMA 切型范围限定 + retry driver + slot-2 resp（v1.3 N3/N7/B2）
- [ ] T3.3: 既有 [sdma] 测试机械迁移（v1.3 N5/B1）
- [ ] T4.1: minimal_v1 JSON 扩展（+`pcie_memory` + bar_sizes 3 元素 + 2 connections）
- [ ] T4.2: DGpuBoard BAR2 fast-path + 单 `vram_storage_` 分配（v1.4 B7）
- [ ] T4.3: E2E 测试 `[minimal_dgpu_soc][driver_visible]`
- [ ] T4.4: `git diff HEAD -- include/abi/cpptlm_emulator.h` 空
- [ ] T4.5: 24-case `[pcie-memory]` 机械迁移 + `[minimal_dgpu_soc]` 添加 BAR2（v1.4 B13）
- [ ] T5: docs/pcie/driver-visible-minimal-soc.md (本文档) + AGENTS.md 同步

### 提交策略

预计 11 个 commits（per tasks.md T5.3），按 T0-T4 顺序：

```
1.  test(stream): AxiMemBundle 经 StreamAdapter round-trip (v1.3 N1)
2.  feat(framework): payload 按 sizeof(BundleT) 扩容 (v1.3 N1)
3.  feat(bundle): 新建 AxiMemBundle (chip-internal AXI 4KB inline)
4.  feat(pcie-memory): PcieMemoryDevice ChStreamModuleBase + 2 SlavePorts + 单 adapter (v1.4 B7/B3)
5.  refactor(pcie): PcieEndpointIP raw pointer + 删 memory_device_->tick() (v1.3 N8)
6.  feat(pcie-cfg): PcieConfigSpace 64-bit 双 dword BAR 生成 (v1.3 B5)
7.  feat(gmmu): 异步状态机 + iova 匹配 + 访问器 + AxiMem MasterPort (v1.3 N2 + B6)
8.  refactor(sdma): 5 端口混合 wire-format + retry + slot-2 resp + DmaDescriptor 字段名修正 (v1.3 N3/N7/B1/B2)
9.  feat(board): BAR2 fast-path + bind_memory_backings 条件化 + 单 vram_storage_ 分配 (v1.4 B7/B11)
10. test(pcie/sdma/memory): 既有 [sdma]/[pcie-memory] 测试机械迁移 (v1.3 N5 + v1.4 B13)
11. test(minimal-soc): driver-visible E2E + N1-N12 + v1.4 B7-B13 must-fix 测试套件
12. docs(pcie): driver-visible-minimal-soc.md (本文档) + AGENTS.md 同步 (含 N1-N12 + B7-B13 索引)
```

## 10. 性能特征

- **同步 BAR 读/写**：device 直读直写，无 inject_q_ 延迟（< 5us/操作）
- **同步 BAR1 doorbell**：与现行 SDMA 路径一致（doorbell 在 bound check 前匹配）
- **chip-internal AXI**：sim 超步内完成（与现有 CacheTLM/MemoryTLM tick 节奏一致）
- **8GB backing lazy commit**：minimal_v1 启动 RSS ≈ 0（Linux overcommit）
- **GMMU translate 单 outstanding**：minimal 范围可接受；D3 引入多 outstanding 需重审

## 11. 关联文档

### CppTLM 仓内
- `docs/pcie/display-device-mvp.md` — D1 显示 IO 设备 MVP 实施笔记（同 PCIe 设备 MVP 链）
- `docs/architecture/14-dgpu-board-ideal-arch.md` — DGpuBoard 理想架构 v2.0
- `docs/architecture/多层次混合仿真.md` — GPGPU 多层 SimModule 拓扑
- `docs/guide/UE_DRIVER_MIGRATION_GUIDE.md` — UE driver 移植指南（framebuffer_size 单一真源约定）

### CppTLM 仓内 openspec
- `openspec/changes/cpptlm-driver-visible-minimal-soc/` — 本设计 change
  - `proposal.md` — Why / What Changes
  - `design.md` — 详细设计（629 行, 含 v1.3 B1-B6 + v1.4 B7-B13）
  - `tasks.md` — T0-T5 任务分解（371 行）
  - `specs/driver-visible-minimal-soc/spec.md` — Requirements + Scenarios（339 行）

### ArchForge 仓（跨仓镜像）
- `docs/architecture/19a-driver-visible-minimal-soc.md` — 本文档镜像
- `docs/architecture/19-pcie-ip-microarchitecture.md` — PCIe EP 集成文档（前置）
- `docs/microarchitecture/<ip-name>.md` — 9 大类 IP 微架构索引
- `docs/adr/ADR-SOC-21-v31-rev2-topology-correction.md` — MAS-3.1 拓扑修正

### 配套 ADR
- ADR-088 §D5 — 23 ABI 冻结约定
- ADR-SOC-18 — PCIe EP 端点架构
- ADR-SOC-21 — MAS-3.1 V3.1-Rev2.0 拓扑修正

## 12. 维护记录

| 日期 | 版本 | 修订 |
|------|------|------|
| 2026-09-26 | v1.4 | Oracle/Metis 三轮交叉审查；P0 修正 B1-B13 全部应用；单一 VRAM 所有权归 DGpuBoard（PcieMemoryDevice 退化为 PCIe 外观层）；3 项遗留议题延期理由显式声明；7 条实施铁律锁定；D3 seam 已预留 |
| 2026-09-26 | v1.3 | Oracle/Metis 二方审查；P0 修正 B1-B6（字段名/切型范围/双 adapter/64-bit BAR/双注册）；单 adapter 模型（事实：MultiPortStreamAdapter 内部遍历全端口） |
| 2026-09-26 | v1.2 | 8 must-fix N1-N12 全部应用 |
| 2026-09-26 | v1.1 | P0 修订：5 澄清落点 + R1-R7 + M1-M5 |
| 2026-09-26 | v1.0 | 初版提案：完整最小设备 + 驱动视角整合 |