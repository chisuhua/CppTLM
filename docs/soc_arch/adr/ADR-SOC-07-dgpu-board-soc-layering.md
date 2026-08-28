# ADR-SOC-07: dGPU Board/SOC 两层分离 — PCIe Endpoint 归属 SOC

> **状态**: 📋 Proposed — 2026-08-26 · **日期**: 2026-08-26 · **Owner**: CppTLM Team (Sisyphus)
> **修订关系**: 细化 [`ADR-SOC-06-cpptlm-v05-mvp.md`](./ADR-SOC-06-cpptlm-v05-mvp.md) D4/D5 的"板卡"内部结构（**不撤销** ADR-SOC-06 的任何决策，仅澄清 DGpuBoardTLM 的内部构造模型）
> **关联 ADR**: [`ADR-SOC-06`](./ADR-SOC-06-cpptlm-v05-mvp.md) · UsrLinuxEmu [`ADR-088`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-088-dgpu-complete-simulation.md)（23 ABI 外部契约）· UsrLinuxEmu [`ADR-090 v2`](https://github.com/chisuhua/UsrLinuxEmu/blob/main/docs/00_adr/adr-090-ptxir-via-h2d-dma-v2.md)（PTX-EMU 归属 SOC submodule）
> **关联 OpenSpec**:
> - `openspec/changes/2026-08-26-cpptlm-dgpu-pcie-endpoint/`（PcieEndpointTLM 组件）
> - `openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/`（SdmaEngineTLM 组件）
> - `openspec/changes/2026-08-26-cpptlm-dgpu-board-soc-split/`（Board shell + SOC JSON 绑定 + s2 helper 迁移）

---

## 1. Context（背景）

### 1.1 s2 单体实现的架构偏差

s2 交付的 `DGpuBoardTLM`（[include/tlm/gpu/dgpu_board_mvp.hh](../../include/tlm/gpu/dgpu_board_mvp.hh)）声明为 `ChStreamModuleBase`，但内部 8 个子模块（`DGpuBar` / `Doorbell` / `SubmitQueue` / `CompletionRing` / `CommandProcessor` / `TmuDispatchProcessor` / `CudaCoreAdapterMVP` / `PtxEmuSubmoduleMVP`）全部是 `Impl` PIMPL 结构体中的**普通 C++ 值成员**，由 `DGpuBoardTLM::tick()` 手动串联驱动：

```text
DGpuBoardTLM (ChStreamModuleBase 外壳)
└── Impl { bar, doorbell, sq, cq, cp, tmu, cuda_core, ptx_emu }   ← 全部 std::make_unique 手动构造
```

由此产生的具体问题：

1. 内部组件**无注册表注册**——`modules_cluster.hh` 不含其中任何一个，JSON 无法单独声明；
2. 内部连接**硬编码在 `tick()`**——CP→TMU→SQ→CudaCore 的串联顺序写死在 C++，不走 `ModuleFactory::instantiateAll` 的 connections 解析；
3. `set_stream_adapter()` 是 **no-op**——`DGpuBoardTLM` 继承了 ChStream 身份但从未接入 StreamAdapter 数据面；
4. [configs/dgpu_board_v1_mvp.json.in](../../configs/dgpu_board_v1_mvp.json.in) 的 `connections` 为空——当前 JSON 只创建 3 个顶层对象，无法表达板内拓扑；
5. 门铃偏移（`GPU_REG_DOORBELL = 0x0014`）等 PCIe 寄存器语义以 C++ `if-else` 硬编码在 `write_reg()`，换芯片型号需改代码。

这不符合 CppTLM "组件通过 JSON + ModuleFactory + StreamAdapter 构建连接" 的核心模型（per `include/AGENTS.md` 注册宏体系 + `configs/AGENTS.md` SimModule 嵌套 JSON）。

### 1.2 真实硬件拓扑依据（ADR-088 P1 承诺）

UsrLinuxEmu ADR-088 的核心承诺是 **driver 端到端零修改移植 + 仿真拓扑与真硬件一致**。真实 dGPU 板卡的物理结构：

```text
dGPU 板卡（PCB）
├── PCIe 金手指 / 连接器
├── GPU SOC 芯片（die）
│   ├── PCIe Endpoint IP（Config Space + BAR + MSI-X capability）  ← 在 SOC 片内
│   ├── SDMA / copy engines（PCIe master 方向，发起 upstream DMA）
│   ├── Command Processor / TMU / Work Distribution
│   ├── Compute Clusters（SM 阵列）
│   └── Memory Controllers
└── HBM/GDDR 显存颗粒（板上，非 SOC 片内）
```

**关键事实**：PCIe Config Space、BAR0 寄存器、BAR1 显存 aperture、MSI-X table 全部属于 **SOC 片内 PCIe Endpoint IP** 的属性，不是"板卡"的属性。板卡本身只是连接器 + 供电 + 显存颗粒的载体。

因此 s2 把 BAR/Config Space 语义放在板卡 C++ 单体里，同时违反了 CppTLM 组件化模型和 ADR-088 的拓扑保真承诺。

### 1.3 方向区分（slave vs master）

PCIe 设备在 SOC 内有**两个方向**的接口，必须拆成两个组件：

| 方向 | 语义 | 归属组件 |
|------|------|---------|
| **Slave（下游）** | host/driver 访问设备：Config Space 读写、BAR0 MMIO、BAR1 显存读写、MSI-X 投递（设备→host 中断） | `PcieEndpointTLM` |
| **Master（上游）** | 设备访问 host：SDMA 发起 upstream DMA 读写，经系统 IOMMU 翻译 IOVA→PA | `SdmaEngineTLM` |

### 1.4 外部契约稳定性约束

ADR-088 §D5 定义的 23 ABI（`cpptlm_emulator_*`）是 CppTLM 对 UsrLinuxEmu 的**外部契约**，目前尚未实现（per 2026-08-26 跨仓审计：两仓 `cpptlm_emulator_*` 符号 0 命中）。本 ADR 的分层修正**不改变 23 ABI 的任何签名与语义**——只决定这些 ABI 在 CppTLM 内部如何落到组件拓扑上。

---

## 2. Decision（决策）

### D1. 两层分离：DGpuBoard（C++ ABI shell）+ DGpuSoc（JSON 拓扑）

✅ **采纳**。dGPU 仿真由两层构成：

```text
UsrLinuxEmu driver
        │ Linux PCI / BAR / DMA / MSI-X API
        ▼
UsrLinuxEmu linux_compat（host 侧 PCI 子系统 API 表面）
        │ C ABI（ADR-088 23 ABI，外部契约不变）
        ▼
DGpuBoard（C++，非 SimObject/ChStreamModuleBase 数据面组件）
        │ ABI 调用 → 事务注入 SOC 端口
        ▼
DGpuSoc（SimModule 容器，JSON + ModuleFactory 构建）
        ├── PcieEndpointTLM      （PCIe slave，见 D2）
        ├── SdmaEngineTLM        （PCIe master，见 D3）
        ├── CommandProcessorTLM  （s3 填充的 CP 提升为组件）
        ├── TmuDispatchProcessorTLM
        ├── SubmitQueueTLM
        ├── CompletionRingTLM
        ├── GpuCluster / ComputeCluster
        └── MemoryCluster / VRAM
```

**DGpuBoard（C++ shell）的完整职责边界**——且仅限以下五项：

1. **ABI 翻译**：`cpptlm_emulator_mmio_write(emu, bar, offset, val)` 等 ABI 调用翻译为事务，注入 `PcieEndpointTLM` 的 slave ingress port；板卡自身**不持有任何寄存器状态**。
2. **设备枚举**：`get_device_count()` / `get_device_info()` / `create_by_id(dev_id)`；dev_id → SOC JSON profile 的解析表放在板卡层。
3. **SOC 装配**：加载 board JSON、调用 `ModuleFactory::instantiateAll()`、持有 `EventQueue` 引用。
4. **回调接线**：`register_callbacks()` / `register_dma_translate_cb()` 把 host 回调绑定到 SOC 组件的 egress port（`PcieEndpointTLM.irq_out`、`SdmaEngineTLM.host_out`）。
5. **生命周期**：create/destroy/reset/shutdown 转发到 SOC 容器。

**DGpuSoc** 继承 `SimModule`（非 `ChStreamModuleBase`），通过嵌套 JSON 实例化全部内部组件，内部连接全部走 JSON `connections` + StreamAdapter。

### D2. PcieEndpointTLM：PCIe slave 归属 SOC

✅ **采纳**。SOC 片内新增 `PcieEndpointTLM` 组件（`ChStreamModuleBase` 派生，`REGISTER_CHSTREAM` 注册），持有：

- **Config Space**（256B/4KB 数组 + capability chain 链表，尺寸参数化）
- **BAR0 寄存器路由表**（寄存器定义数据化：doorbell 等偏移成为 JSON/表驱动，非 C++ if-else）
- **BAR1 显存 aperture 映射**（映射到 SOC 内 MemoryCluster 端口，不直接持有显存）
- **MSI-X table / PBA / pending bitmap**（vector 数量参数化）
- **门铃语义**（BAR0 写 → 路由到 CP wake；沿用 s2 `Doorbell` strong-order 语义，迁移为本组件内部 register block 或 JSON 连接）

**端口协议**（详见 change A design）：

| 端口 | 方向 | 用途 |
|------|------|------|
| `slave_in` | ingress | host 发起的事务（config r/w、BAR0 MMIO r/w、BAR1 mem r/w） |
| `mmio_out` | egress → SOC 内部 | 解码后的寄存器写副作用（doorbell → CP wake） |
| `mem_out` | egress → MemoryCluster | BAR1 aperture 访问转发 |
| `irq_out` | egress → Board | MSI-X 投递（host_notify），Board 接线到 UsrLinuxEmu 中断回调 |

### D3. SdmaEngineTLM：PCIe master 归属 SOC

✅ **采纳**。SOC 片内新增 `SdmaEngineTLM` 组件，承担设备发起的 upstream DMA：

- 从 CP 接收 DMA 描述符（`desc_in`）
- 读 SOC VRAM（`mem_in`，接 MemoryCluster）
- 向 host 发起 upstream 事务（`host_out` → Board → `cpptlm_dma_translate_cb` → 系统 IOMMU → host 内存）
- 完成写回（`done_out` → CompletionRing）
- 错误路径：translate callback 返回非 0 → PCIe RequesterCompleterAbort → `cpptlm_error_cb_t` 通道（per ADR-088 §D3.8）

### D4. Board JSON 作为顶层容器

✅ **采纳**。板卡级 JSON（如 `configs/dgpu_board_v1.json`）是顶层 SimModule 容器配置：

```json
{
  "modules": [
    { "name": "pcie_ep", "type": "PcieEndpointTLM", "params": { ... } },
    { "name": "sdma",    "type": "SdmaEngineTLM",   "params": { ... } },
    { "name": "cp",      "type": "CommandProcessorTLM" },
    { "name": "vram",    "type": "MemoryCluster",   "params": { ... } }
  ],
  "connections": [
    { "src": "pcie_ep.mmio_out", "dst": "cp.cmd_in" },
    { "src": "pcie_ep.mem_out",  "dst": "vram.port0" },
    { "src": "sdma.host_out",    "dst": "board.upstream" }
  ]
}
```

**显存归属细化**：MVP 阶段 MemoryTLM/MemoryCluster 可直接放在 SOC JSON 内；追求拓扑保真时，显存颗粒作为 board 级 JSON 中 SOC 的兄弟节点（SOC 片内只留 memory controller）。两种形态都受本 ADR 支持，选择由 board JSON 表达而非 C++ 代码决定。

### D5. 23 ABI 外部契约不变

✅ **采纳**。本分层修正对 ADR-088 §D5 的 23 ABI **零影响**：ABI 函数签名、调用时序、语义全部不变，仅内部实现从"操作板卡 C++ 成员"改为"向 SOC 端口发事务"。UsrLinuxEmu 侧无感知。23 ABI 标记为 **planned contract**（当前未实现），其实现归属 Board shell 层。

### D6. s2 单体的迁移路径

✅ **采纳**。s2 `DGpuBoardTLM` 单体按下表迁移（详见 change C）：

| s2 helper（Impl 值成员） | 迁移目标 |
|--------------------------|---------|
| `DGpuBar`（BAR0 寄存器数组 + VRAM 指针） | 拆分：BAR0 寄存器表 → `PcieEndpointTLM`；VRAM → MemoryCluster（JSON 组件） |
| `Doorbell` | `PcieEndpointTLM` 内部 register block（strong-order 语义保留） |
| `SubmitQueue` | 提升为 `SubmitQueueTLM`（注册组件） |
| `CompletionRing` | 提升为 `CompletionRingTLM`（注册组件） |
| `CommandProcessor` / `TmuDispatchProcessor` | s3 填充后提升为注册组件（change C 在 s3 archive 后执行） |
| `CudaCoreAdapterMVP` / `PtxEmuSubmoduleMVP` | 保持 s1 边界，经 JSON 参数装配进 GpuCluster 路径 |
| `UsrLinuxEmuIoctlStub` | **不进入 SOC**——IOCTL 语义在真实硬件中不存在于 SOC 内，是测试基础设施快捷方式，保留为板外测试工具 |

### D7. 明确反对项（Non-Decisions）

- ❌ **不允许**新组件继续作为 `Impl` 普通成员手动 `tick()`——所有周期驱动组件必须注册并经 EventQueue/ModuleFactory 调度；
- ❌ **不允许** UsrLinuxEmu 直接引用 SOC 内部组件类型（`CommandProcessorTLM` 等）——跨仓边界只暴露 23 ABI；
- ❌ **不在本 ADR 引入 YAML**——board profile/SOC 拓扑统一 JSON（修正 ADR-088 §D3.2 的 yaml 表述，见 Status Update 注记）；
- ❌ `DGpuBoard` C++ shell **不继承** `ChStreamModuleBase`——它不是数据面组件，无 StreamAdapter 需求；SOC 才是。

---

## 3. Consequences（后果）

### 正面

1. **拓扑保真**：PCIe endpoint 归属 SOC 片内，与真硬件一致，直接支撑 ADR-088 P1 端到端零修改承诺；
2. **配置化**：BAR 布局、Config Space capability、MSI-X vector 数、门铃寄存器偏移全部 JSON/表驱动，换 dev_id 只换配置文件；
3. **组件纪律回归**：CP/TMU/SQ/CQ 成为可独立实例化、独立测试、可经 StreamAdapter 连接的注册组件；
4. **ABI 稳定**：外部 23 ABI 零变更，分层修正完全封装在 CppTLM 仓内；
5. **跨仓边界清晰**：UsrLinuxEmu 只见 Board contract，SOC 内部组件永不跨仓泄漏。

### 负面

1. **s2 单体返工**：`DGpuBoardTLM::Impl` 8 成员需拆分迁移，s2 测试需适配（change C 负责）；
2. **与 s3 的排序约束**：s3 正在填充 CP/TMU 实现，change C 的组件化迁移必须在 s3 archive 后执行（避免同文件冲突），或 s3 直接按组件形态交付（需修订 s3 tasks）；
3. **新增 2 个组件 + 1 个 bundle 包**的维护面（`pcie_bundles_tlm.hh`）。

### 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|:---:|:---:|------|
| s3 与 change C 文件级冲突 | 高 | 中 | change C 显式声明依赖 s3 archive；proposal 中锁定执行顺序 |
| PcieEndpointTLM 端口协议设计不足导致返工 | 中 | 高 | change A design 先冻结 bundle 结构 + 端口表，评审后实施 |
| s2 PCIe 视角测试（`test_dgpu_pcie_device_perspective.cc`）失效 | 中 | 低 | 测试语义保留，适配到 shell→SOC 端口注入路径 |

---

## 4. References

- [`ADR-SOC-06-cpptlm-v05-mvp.md`](./ADR-SOC-06-cpptlm-v05-mvp.md)（本 ADR 细化其 D4/D5）
- [`dgpu-board.md`](../modules/dgpu-board.md)（s2 8 组件单体文档，已加修订注记指向本 ADR）
- UsrLinuxEmu ADR-088 §D5（23 ABI 外部契约）/ §D3.8（DMA translate callback）
- UsrLinuxEmu ADR-090 v2 §D3.3（dGPU 板卡最小完整集：DGpuBar + Doorbell + SQ/CQ）
- `include/AGENTS.md`（注册宏体系）/ `configs/AGENTS.md`（SimModule 嵌套 JSON 规范）
- 2026-08-26 跨仓审计记录（23 ABI 未实现、`src/system_hw/` 为规划目录等事实核对）

---

## Status Update

- **2026-08-26**: 📋 Proposed 创建。修正 ADR-088 §D3.2 "加载 yaml" 表述 → CppTLM 统一 JSON（board profile / SOC topology）。三个实施 change 已起草（pcie-endpoint / sdma-engine / board-soc-split），change C 依赖 s3 archive。
- **2026-08-26 (Metis/Oracle 评审后裁决记录)**：以下三项决策作为本次评审输出记录，**正文 D1-D7 不变**，后续 change 需遵循：
  - **Q3 BAR1 大块数据路径裁决**：bundle 保持 POD（descriptor-only, `offset+size`），timed TLP 路径一律经 `PcieEndpointTLM::mem_out` → `MemoryTLM`；>8B 的实际数据走 23 ABI 已有的 `backdoor_read/backdoor_write` 通道（绕过 timing 的批量数据通道，与 SystemC TLM DMI 同构），经"descriptor-only TLP 推进带宽模型"实现时序一致性。VRAM 存储单一 owner 为 `MemoryCluster`，backdoor 路径必须落到同一存储。**禁止** driver-side BAR1 访问绕过 `PcieEndpointTLM`（违反本 ADR D2）。
  - **Q5 s3 archive 触发器定义**：change C 启动条件 = s3 T-s3-1 + T-s3-2 + **T-s3-3** 三个 commit 落地（`TmuHandlerInterface` 接口扩展在 T-s3-3 完成，change C 端口化必须以此为锚点），不等 T-s3-4/5（发布动作，不改接口）。判据：3 commit hash + `command_processor_mvp.hh`/`tmu_dispatch_processor_mvp.hh`/`pm4_decoder_mvp.hh` 在 T-s3-3 后无 diff + 对应测试 PASS。
  - **Q6 shell 执行模型裁决**：每张 `DGpuBoard` 在 `cpptlm_emulator_create_by_id()` 时启动独立 `std::thread`，持有独立 `EventQueue*`（`event_queue.hh` 非线程安全，故每卡独立 EQ 是唯一正确选择）。多卡 = 多线程并行，每张卡独立仿真时间。SimModule::depth_ 已为 thread_local 框架层支持多线程。**禁止** host 线程直接 `eq_->schedule()`，必须经注入队列（`mutex+deque`）；sim→host callback 必须非阻塞并禁止反向调用 ABI（防死锁）；`backdoor_read` 同样走注入队列由 sim 线程 quantum 边界服务（呼应 Q3 一致性要求）；`StatsManager` 单例多卡注册需 `stats_path` 带 `device_id` 前缀；quantum 循环 `while(!stop) { eq_->run(quantum); drain_injection_queue(); }`，quantum 默认 1000 cycles（JSON 可配）；destroy 顺序 `stop_→join→destruct SOC`（防悬垂）。
