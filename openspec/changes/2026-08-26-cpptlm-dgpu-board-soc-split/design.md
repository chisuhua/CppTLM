# cpptlm-dgpu-board-soc-split: Design

> **配套**: [`proposal.md`](./proposal.md) · [`tasks.md`](./tasks.md)
> **关联 ADR**: [`docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md`](../../../docs/soc_arch/adr/ADR-SOC-07-dgpu-board-soc-layering.md) D1/D4/D5/D6

## 1. 两层结构

```text
UsrLinuxEmu linux_compat
    │  23 ABI (ADR-088 §D5，契约不变)
    ▼
┌─ DGpuBoard (C++ shell, 非 SimObject/ChStreamModuleBase 数据面) ─┐
│  1. ABI 翻译: mmio_write(bar,off,val) → PcieTlpBundle → 注入    │
│  2. 设备枚举: dev_id → SOC JSON profile                          │
│  3. SOC 装配: ModuleFactory::instantiateAll(board_json)          │
│  4. 回调接线: register_callbacks → pcie_ep.irq_out;              │
│               register_dma_translate_cb → sdma.host_out          │
│  5. 生命周期: create/destroy/reset                               │
└──────────────────────────────────────────────────────────────────┘
    │  持有
    ▼
┌─ DGpuSoc (SimModule 容器, REGISTER_MODULE) ─────────────────────┐
│  internal_factory: pcie_ep / sdma / cp / tmu / sq / cq /        │
│                    gpu_cluster / memory                          │
│  connections: JSON 声明, StreamAdapter 注入                       │
└──────────────────────────────────────────────────────────────────┘
```

## 2. DGpuBoard shell 接口（内部 C++，非 23 ABI 本身）

```cpp
class DGpuBoard {
public:
    DGpuBoard(const std::string& name, EventQueue* eq);
    ~DGpuBoard();

    // 装配
    bool load_soc_config(const nlohmann::json& board_cfg);   // DGpuSoc + 兄弟 memory 节点
    bool init();
    void shutdown();
    void tick();   // 转发 DGpuSoc::tick()（SimModule 递归 tick 内部组件）

    // ABI 翻译入口（被 23 ABI C 函数调用）
    int  mmio_read (uint8_t bar, uint64_t offset, void* buf, size_t len);
    int  mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len);
    int  pcie_config_read (uint16_t offset, uint8_t width, uint32_t* val);
    int  pcie_config_write(uint16_t offset, uint8_t width, uint32_t val);

    // 回调接线
    void set_irq_callback(/* intr deliver cb */);
    void set_dma_translate_callback(/* dma translate cb */);
    void set_error_callback(/* error cb */);

    // 枚举
    uint32_t device_id() const;
    const BoardDeviceInfo& device_info() const;

private:
    std::unique_ptr<DGpuSoc> soc_;
    EventQueue* eq_;
    // 无寄存器/无 BAR/无 SOC 内部对象成员
};
```

**关键约束**（per ADR-SOC-07 D7）：shell 不继承 `ChStreamModuleBase`（它不是数据面组件，无 StreamAdapter 需求）；不持有任何寄存器状态；不直接 `tick()` 内部组件（交给 `DGpuSoc` SimModule 递归 tick）。

### 2.5 执行模型（per ADR-SOC-07 Status Update Q6 裁决）

每张 `DGpuBoard` 在构造时启动独立 `std::thread`，持有独立 `EventQueue*`，驱动自己的仿真时间推进。多卡 = 多线程并行，每张卡独立仿真时间。

```cpp
class DGpuBoard {
    // ...
private:
    std::unique_ptr<EventQueue> eq_;            // 每卡独立，event_queue.hh 非线程安全
    std::unique_ptr<DGpuSoc>     soc_;
    std::thread                  sim_thread_;
    std::atomic<bool>            stop_{false};
    std::mutex                   inject_mu_;
    std::deque<PendingReq>       inject_q_;    // host→sim 注入队列
};
```

**线程模型**（`load_soc_config` 完成后启动）：

```text
host thread (UsrLinuxEmu)
    │ cpptlm_emulator_mmio_write(emu, bar, off, val)
    │ ↓ push PendingReq{bar,off,val} 进 inject_q_ (mutex+condvar)
    │ ↓ wait future with timeout=1ms wall-clock (resp)
    ▼
sim thread (DGpuBoard::sim_loop)
    while (!stop_.load()) {
        eq_->run(quantum);               // quantum 默认 1000 cycles, JSON 可配
        drain_injection_queue();         // 构造 PcieTlpBundle 注入 pcie_ep.slave_in
    }
    │ 事务经 slave_in → PcieEndpointTLM → mem_out/mmio_out → SOC 内部
    │ 响应回到 inject_q 的 future.set_value()
    ▼
host thread 收到 resp, 继续 ABI 调用
```

**关键约束**：

1. **EventQueue 非线程安全**（`event_queue.hh:49` `run(N)` 裸 `priority_queue`，无锁）——每卡独立 `EventQueue` 是唯一正确选择，框架已预见多线程（`SimModule::depth_` 是 `thread_local`，`sim_module.hh:44,257` 注释明示）。
2. **host→sim 注入**：UsrLinuxEmu 线程**禁止**直接 `eq_->schedule()`（数据竞争）；必须经 `mutex + deque<PendingReq>`，sim 线程在 quantum 边界 drain 并构造 `PcieTlpBundle` 注入 `pcie_ep.slave_in`。
3. **同步等待**：`mmio_read` 等需返回值的调用，host 线程投递请求后阻塞在 `std::promise/future`，sim 线程在 resp 通道收到响应时 `set_value`；**必须带超时**（如 1ms wall-clock）防 sim 线程死锁拖死 host。
4. **sim→host callback**：`irq_out` / dma translate / error callback 在 sim 线程上下文执行，**必须非阻塞**——投递到 UsrLinuxEmu 侧队列立即返回；**严禁** callback 内反向调 23 ABI（host 线程可能正持 inject_mu → 死锁）。
5. **backdoor 一致性**（呼应 §4 Q3 裁决）：`backdoor_read/write` 同样走 inject_q 由 sim 线程 quantum 边界服务，保证 timed 路径与 backdoor 路径访问同一 VRAM 存储时不发生数据竞争。
6. **StatsManager 多卡注册**：`module_factory.cc:713` `StatsManager::instance()` 是进程级单例。多卡 `instantiateAll` 会重复注册同名 `stats_path`——`DGpuBoard` 实现 `get_stats_path()` 必须带 `device_id` 前缀（如 `"board_0.pcie_ep"`），否则第二张卡实例化即冲突。
7. **多卡创建阶段同步**：两张卡**同时** `instantiateAll` 时，任何延迟注册路径（如 plugin dlopen）需全局 instantiate mutex；运行期（各卡独立线程）零锁（除 inject_mu 等显式边界）。
8. **异常跨线程**：sim 线程内任何异常（如 `simulate_instantiate` 的 depth limit throw）必须在 `sim_loop` 顶层 catch 并存入 `std::exception_ptr`，下次 ABI 调用或 `destroy` 时 rethrow 给 host 线程——静默吞异常 = 卡死无诊断。
9. **TickEvent 自续**（`sim_object.hh:379-382`：process → tick → initiate_tick）——事件队列永不空，`run(quantum)` 必跑满 quantum 周期；idle 检测要用 CQ/SQ 计数器而非 `event_queue.empty()`。
10. **destroy 顺序**：`stop_=true` → inject_q 推入 poison pill 唤醒 sim 线程 → `sim_thread_.join()` → 析构 SOC → 析构 EventQueue。顺序反了 = 悬垂指针。

## 3. DGpuSoc 容器

```cpp
class DGpuSoc : public SimModule {
public:
    explicit DGpuSoc(const std::string& n, EventQueue* eq) : SimModule(n, eq) {}
    std::string get_module_type() const { return "DGpuSoc"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;  // 嵌套 JSON
    // SimModule::tick() 默认递归内部组件，无需 override
};
```

- 注册：`REGISTER_MODULE(DGpuSoc)`（`include/modules_cluster.hh`，SimModule 注册表，per `include/AGENTS.md` 双注册表规则——SimModule 派生必须走 `registerModule`）。
- board JSON 中 `"type": "DGpuSoc"` + 内联 `modules`/`connections`（per `configs/AGENTS.md` SimModule 嵌套 JSON 规范，`MAX_DEPTH=8` 限深保护天然适用）。
- shell 通过 `getInternalInstance("pcie_ep")` 等获取组件指针完成回调接线（仅构造期接线，运行期零穿透）。

**StreamAdapter 注入路径**（per Metis/Oracle Q1 裁决，**自动递归**无需修改）：`SimModule::simulate_instantiate()`（`sim_module.hh:95`）调 `internal_factory->instantiateAll(config)`，`instantiateAll` 的 Step 7（`module_factory.cc:580-698`）操作本层 factory 自己的 `object_instances` 局部表，因此嵌套 factory 各自执行自己的 adapter 注入。先例：`ComputeCluster::simulate_instantiate`（`compute_cluster.cc:28-53`）二次调 `internal_factory->instantiateAll(cu_cfg)` 跑嵌套注入已被验证。**change C 无需修改 `sim_module.hh` 或 `module_factory.cc`**。

**陷阱**：Step 7b 只读本层 factory 的 `final_config["connections"]`——SOC 内部连线必须写在 board JSON 的 `DGpuSoc` 条目内层（§4 已正确这么做）；**跨边界连接**（shell → `pcie_ep.slave_in`）无法走 JSON，shell 必须在构造期调 `soc_->getInternalInputPort("pcie_ep.slave_in")` + 手动 `PortPair`，或经 `inputs/outputs` 暴露标签。BS-G1 测试必须断言内部 4 个 ChStream 组件（`pcie_ep`/`sdma`/`cp`/...）adapter 非空。

### 3.5 组件端口表（per Metis/Oracle Q4 裁决）

s2 helper 提升为 ChStream 组件后的端口表冻结如下（`set_stream_adapter(adapters[])` 索引顺序：**ingress 在前按声明序，egress 在后按声明序**）：

| 组件 | 端口 | 方向 | Bundle 类型 | 索引 | 对应 s2 API |
|------|------|------|-------------|-----|-------------|
| `PcieEndpointTLM` (change A) | `slave_in` | in | `PcieTlpBundle` | 0 | shell 注入入口 |
| | `mmio_out` | out | `PcieTlpBundle` | 1 | doorbell → CP |
| | `mem_out` | out | `PcieTlpBundle` | 2 | BAR1 → VRAM |
| | `irq_out` | out | `MsiXDeliveryBundle` | 3 | MSI-X → host |
| `SdmaEngineTLM` (change B) | `desc_in` | in | `DmaDescriptorBundle` | 0 | CP/上层 |
| | `mem_in` | in | `PcieTlpBundle` | 1 | VRAM 读 |
| | `mem_out` | out | `PcieTlpBundle` | 2 | VRAM 写 |
| | `host_out` | out | `PcieTlpBundle`（descriptor-only，bulk 数据走 backdoor） | 3 | upstream → IOMMU |
| | `done_out` | out | `CompletionBundle` | 4 | → CQ |
| `CommandProcessorTLM` | `cmd_in` | in | `PcieTlpBundle` | 0 | doorbell → `wake()` |
| | `fetch_out` ⭐新增 | out | `CacheReqBundle` 复用 | 1 | FETCH 态 `mem_read_vram(GPU VA, sizeof(gpu_gpfifo_entry))`（s3 T-s3-2） |
| | `dma_req` | out | `DmaDescriptorBundle` | 2 | FSM 中大块搬运 |
| | `dispatch` | out | `Pm4DispatchBundle`（新，POD 化 `Pm4MethodDispatch`） | 3 | → TMU |
| `TmuDispatchProcessorTLM` | `dispatch_in` | in | `Pm4DispatchBundle` | 0 | → TMU.submit |
| | `cta_out` | out | `CtaDescriptorBundle`（包 s2 `CtaDescriptor` 字段） | 1 | → SQ |
| | `done_in` | in | `CompletionBundle`（来自 change B dma_bundles_tlm.hh，本 change 复用） | 2 | dep chain 释放 |
| `SubmitQueueTLM` | `cta_in` | in | `CtaDescriptorBundle` | 0 | enqueue |
| | `dispatch` | out | `CtaDescriptorBundle` | 1 | → GpuCluster |
| | `done_in` ⭐新增 | in | `CompletionBundle`（来自 change B dma_bundles_tlm.hh，本 change 复用） | 2 | `on_warp_complete(task_id, status)` |
| | ~~`full_signal`~~ | — | — | — | **删除**——反压由 ChStream resp 方向天然承载（adapter 有 resp 通道返回 REJECTED），与 s3 `SUBMIT_QUEUE_REJECTED` 语义对齐 |
| `CompletionRingTLM` | `done_in[0]` | in | `CompletionBundle`（来自 change B dma_bundles_tlm.hh，本 change 复用） | 0 | ← gpu.done |
| | `done_in[1]` | in | `CompletionBundle`（来自 change B dma_bundles_tlm.hh，本 change 复用） | 1 | ← sdma.done_out（多源汇聚用 2 端口） |
| | `done_out` ⭐新增 | out | `CompletionBundle`（来自 change B dma_bundles_tlm.hh，本 change 复用） | 2 | → tmu.done_in（dep chain 释放传播） |
| | `irq_out` | out | `MsiXDeliveryBundle` | 3 | 或经 `pcie_ep.irq_out` 转发 |

**陷阱**：

1. CQ 的 `done_in` 是**多源汇聚**（`gpu.done` + `sdma.done_out`）——CQ 注册为**多端口**（`done_in[0]`/`done_in[1]`），不是 JSON 单端口多边汇聚（`module_factory.cc:792-793` `PortPair` push_back 无检查多对一会建两个 PortPair，行为依赖 SlavePort 实现）。CQ `num_ports() == 4`。
2. `cq.done_out → tmu.done_in` 这一边**不可缺**——缺失将导致 dep chain 永远不死锁检测不到。board JSON §4 强制声明。
3. CP 新增 `fetch_out` 而非复用 `dma_req`：FETCH 阶段读 VRAM GPFIFO 与 SDMA 描述符语义不同（前者是固定大小 raw fetch，后者是 host↔device 搬运）；端口语义清晰优先于最小化端口数。`CacheReqBundle` 复用（addr+size 字段对齐），零新增 bundle 类型。
4. `Pm4MethodDispatch` / `TmuDispatchRecord` / `CtaDescriptor` 当前是 s3/s2 内部 C++ 类型，提升为 bundle 时必须 POD 化（值语义，per Q3 裁决）。`CompletionBundle` 不在本文件创建，复用 sdma-engine（`change B`）交付的 `include/bundles/dma_bundles_tlm.hh::bundles::CompletionBundle`，避免同 namespace 重定义。
5. TMU handler 模式（`S3SubmitQueueHandler` on_dispatch → SQ.enqueue，s3 T-s3-3）在 change C 端口化后被**替代**——handler 注入模式删除，改由 `tmu.cta_out → sq.cta_in` 端口连接实现同向行为。这是 change C 设计中"逻辑不变"承诺的**唯一例外**点，需在 `tasks.md` T-bs-2 显式记录。

## 4. Board JSON 示例（`configs/dgpu_board_v1.json`）

```json
{
  "name": "dgpu_board_v1",
  "params": { "device_id": "0x1234", "quantum_cycles": 1000,
              "ptx_emu_root": "@PTX_EMU_ROOT@" },
  "modules": [
    {
      "name": "soc",
      "type": "DGpuSoc",
      // 注：`params` 是 DGpuSoc 的（device_id / ptx_emu_root 在此传入 SOC）；
      //     顶层 `params` 是 board JSON 根的（quantum_cycles）
      "modules": [
        { "name": "pcie_ep", "type": "PcieEndpointTLM", "params": {
            "config_size": 4096, "num_msix_vectors": 16,
            "bar_sizes": [65536, 268435456],
            "bar0_registers": [
              { "offset": 20, "name": "GPU_REG_DOORBELL", "access": "wo",
                "side_effect": "doorbell" }
            ]
        }},
        { "name": "sdma", "type": "SdmaEngineTLM", "params": { "max_inflight": 4 } },
        { "name": "cp",   "type": "CommandProcessorTLM" },
        { "name": "tmu",  "type": "TmuDispatchProcessorTLM" },
        { "name": "sq",   "type": "SubmitQueueTLM" },
        { "name": "cq",   "type": "CompletionRingTLM" },
        { "name": "gpu",  "type": "GpuCluster",
          "config": "configs/templates/gpu_2gpc_2tpc_2cu.json" },
        { "name": "vram", "type": "MemoryTLM", "params": { "capacity_gb": 1 } }
      ],
      "connections": [
        { "src": "pcie_ep.mmio_out", "dst": "cp.cmd_in" },
        { "src": "pcie_ep.mem_out",  "dst": "vram.0" },
        { "src": "cp.fetch_out",     "dst": "vram.0" },
        { "src": "cp.dma_req",       "dst": "sdma.desc_in" },
        { "src": "cp.dispatch",      "dst": "tmu.dispatch_in" },
        { "src": "tmu.cta_out",      "dst": "sq.cta_in" },
        { "src": "sq.dispatch",      "dst": "gpu.cta_in" },
        { "src": "gpu.done",         "dst": "cq.done_in[0]" },
        { "src": "sdma.done_out",    "dst": "cq.done_in[1]" },
        { "src": "cq.done_out",      "dst": "tmu.done_in" },
        { "src": "gpu.done",         "dst": "sq.done_in[0]" }
      ],
      // 注：CQ 与 SQ 共享 gpu.done 完成源（CQ 经 `done_in[0]` + SQ 经 `done_in[0]`）；JSON 解析层需在
      //     module_factory.cc 的 final_config["connections"] 处理中支持同一 src 多次出现（fan-out），
      //     否则需要新增中间 fan-out 组件或扩展 JSON schema——后续 PR 跟进，本 change 仅冻结语义。
      "outputs": [
        { "internal": "pcie_ep.irq_out", "external": "irq" },
        { "internal": "sdma.host_out",   "external": "host_dma" }
      ],
      "inputs": [
        { "internal": "pcie_ep.slave_in", "external": "host_tlp" }
      ]
    }
  ],
  "connections": []
}
```

要点：
- SOC 内部连接全部 JSON 声明（s2 `tick()` 硬编码串联被 connections + SimModule 递归 tick 替代）；
- `outputs`/`inputs` 暴露 SOC 边界端口（per SimModule `outputs`/`inputs` 规范），shell 的 ABI 翻译层接 `host_tlp` 入口、`irq`/`host_dma` 出口；
- 显存放 SOC 内（MVP）或作为 board 级兄弟节点（保真模式）由 JSON 决定，不改代码（per ADR-SOC-07 D4）；
- `params.quantum_cycles` 默认 1000（per §2.5）；
- `params.device_id` 必须用于 `StatsManager::instance().register_group()` 的 `stats_path` 前缀（多卡时防冲突，per §2.5 第 6 项）；
- **CQ `done_in` 多源汇聚**：CQ 注册为 4 端口组件，`done_in[0]` 与 `done_in[1]` 分别接 `gpu.done` 与 `sdma.done_out`（per §3.5 端口表 陷阱 1）；
- **BAR1 MEM 块访问数据面**（per ADR-SOC-07 Status Update Q3）：`>8B` 走 shell 23 ABI 的 `backdoor_read/write` 通道 + `mem_out` 发 descriptor-only TLP 推进带宽模型。board JSON 当前显式 `mem_out → vram.0` 已支持；backdoor ABI 实现归后续独立 change（`cpptlm-dgpu-abi-export`），本 change 仅冻结接口与 JSON 路径。
- `UsrLinuxEmuIoctlStub` 不出现在 board JSON——它留在测试文件里直接构造（IOCTL 是 host 侧语义，非 SOC 组件）。

## 5. s2 单体退役两阶段

**阶段 1（deprecated 别名）**：
- `DGpuBoardTLM` 保留注册但标记 deprecated（注释 + 文档）；新 board JSON 走 `DGpuSoc`；
- `test_dgpu_pcie_device_perspective.cc` 适配：shell→`pcie_ep.slave_in` 注入替代直接 `write_reg()`，6 测试语义保留（BAR0 doorbell wake、BAR1 round-trip、IOCTL 生命周期、pushbuffer→SQ、BAR1 越界、GPFIFO PUT 无副作用）；
- 全量测试双绿。

**阶段 2（物理删除）**：
- 删除 `dgpu_board_mvp.hh/.cc`、`chstream_register.hh` 中 `DGpuBoardTLM` 注册、`configs/dgpu_board_v1_mvp.json.in`；
- `grep -rl "DGpuBoardTLM" include/ src/ test/ configs/` 应为 0 命中（除变更日志/归档文档；与 tasks.md BS-G4 一致）。

## 6. 与 s3 的排序（per ADR-SOC-07 Status Update Q5 裁决）

s3（command-pipeline，0/43 tasks 进行中）填充 CP/PM4/TMU **逻辑**；本 change 把 CP/TMU **提升为注册组件**。change C 启动条件 = **s3 T-s3-1 + T-s3-2 + T-s3-3 三个 commit 落地**（头文件接口冻结点），不等 T-s3-4/5（发布动作，不改接口）。

**判据**（三个 commit 落地 + 全部满足）：

1. T-s3-1（T-s3-1 Pm4Decoder `.cc` 填充）+ T-s3-2（T-s3-2 CommandProcessor `.cc` 5-state FSM）+ T-s3-3（T-s3-3 TmuDispatchProcessor `.cc` + `TmuHandlerInterface` 扩展为 `TmuHandlerResult`）三个 commit 全部落地；
2. `git log --oneline -- include/tlm/gpu/{command_processor,tmu_dispatch_processor,pm4_decoder,submit_queue,completion_ring}_mvp.hh` 在 T-s3-3 之后无新 commit；
3. `test_pm4_decoder_mvp` / `test_command_processor_mvp` / `test_tmu_dispatch_processor_mvp` 三个测试文件全 PASS。

**为何 T-s3-3 是关键锚点而非仅 T-s3-1/2**：s3 T-s3-3 包含**头文件接口变更**——`TmuHandlerInterface` 从 `on_dispatch()` (void) 扩展为 `on_dispatch() → TmuHandlerResult`（s3 `tasks.md:64-65`）。若 change C 在 T-s3-2 后启动，TMU 端口化做到一半接口又变，必返工。T-s3-3 完成后头文件接口冻结，change C 端口化可安全执行。

**例外点记录**：s3 的 `S3SubmitQueueHandler` 模式（`tasks.md:71`：handler 调 `sq_.enqueue`）在 change C 端口化后被**替代**——handler 注入模式删除，改由 `tmu.cta_out → sq.cta_in` 端口连接实现同向行为。这是 change C 设计中"逻辑不变"承诺的**唯一例外**，需在 `tasks.md` T-bs-2 显式记录。

## 7. 测试策略

| 测试文件 | 标签 | 内容 |
|----------|------|------|
| `test_dgpu_soc_from_config.cc` | `[dgpu][json]` | SOC 完整实例化、connections 非空解析、outputs/inputs 暴露（BS-G1） |
| `test_dgpu_board_shell_abi.cc` | `[dgpu][shell]` | shell 5 职责：mmio_write→slave_in 注入、枚举、回调接线、reset 传播（BS-G2） |
| `test_dgpu_pcie_device_perspective.cc` | `[pcie][dGPU]` | 6 测试适配新路径（BS-G3） |

## 8. 风险与缓解

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | s3 排期滑移导致 CP/TMU 文件冲突 | 硬依赖：T-s3-1/2/3 commit 全部落地（per §6 Q5），不等 archive |
| R2 | SimModule 嵌套 connections 对 ChStream 组件的 StreamAdapter 注入路径未覆盖新组件 | BS-G1 专测（断言内部 ChStream 组件 adapter 非空）；复用 CpuCluster 嵌套先例 |
| R3 | PCIe 视角测试适配引入语义漂移 | 6 测试断言逐条映射表（适配时逐条核对） |
| R4 | shell 持有组件指针造成生命周期悬垂 | 仅构造期接线；指针有效性 = soc_ 生命周期；destroy 顺序 stop→join→destruct（per §2.5 第 10 项） |
| R5 | 多卡 `StatsManager` 单例注册冲突 | `get_stats_path()` 必须带 `device_id` 前缀（per §2.5 第 6 项） |
| R6 | CQ `done_in` 多源汇聚行为未验证 | 多端口注册（`done_in[0]/[1]`），board JSON 强制两条边分离；BS-G1 覆盖 |
| R7 | BAR1 大块数据走 backdoor ABI 与 timed 路径竞争 | timed 路径与 backdoor 路径都走 inject_q（per §2.5 第 5 项），sim 线程 quantum 边界串行服务，VRAM 单一 owner |

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📐 Design — 待评审后实施
