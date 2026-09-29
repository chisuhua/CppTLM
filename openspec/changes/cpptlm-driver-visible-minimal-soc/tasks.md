# Tasks: Driver-Visible Minimal SoC (v1.8 — v1.3 P0 (B1-B6) + v1.4 架构根因 (B7-B13) + v1.5 隐藏缺陷 (B14-B28) + v1.6 架构锁定 (F1-F12) + 演进路线图 + v1.7 用户目标验证 (H1-H7) + v1.8 normative 文本收口 (N1-N5))

> **配套**: [proposal.md](proposal.md) · [design.md](design.md) · [specs/driver-visible-minimal-soc/spec.md](specs/driver-visible-minimal-soc/spec.md)
> **方法**: v1.3 P0 修正（6 项 B1-B6）→ T0 表征/前置 → T1-T4 改造 → T5 文档
> **工期**: 9.5-11 工作日（v1.3 修正 0.5-1d + T0 1d + T1 2d + T2 1.5d + T3 2.5-3d + T4 1.5d + buffer 1d）
> **作废参考**: `2026-09-20-cpptlm-pcie-memory-device-mvp` (D2 v1.1, archive)
> **v1.3 修订依据**: 2026-09-26 — Oracle 三方交叉审查 (Metis + Oracle + Librarian) 发现 v1.2 有 6 项 P0 阻塞错误，**实施前必须修正**

## ⚠️ v1.4 设计修正（**实施前必须完成**）

> v1.3 P0 修正 (B1-B6) 是**正确的真错误捕获**，但**仍在错误的"两个 VRAM"架构上修补**。v1.4 在 v1.3 之上叠加**架构根因修复**：单一 VRAM backing 所有权归 DGpuBoard，PcieMemoryDevice 退化为"PCIe 外观层"。

| ID | 性质 | 修正要点 |
|----|------|----------|
| **B7** | 架构根因 | **双 VRAM backing 合并为单一 vram_storage_**：删 `PcieMemoryDevice::memory_backing_` + `ensure_memory_backing_allocated`；新增 `set_backing_store(uint8_t*, uint64_t)`（对齐 `memory_tlm.hh:82` 先例）；DGpuBoard 持有 `std::unique_ptr<uint8_t[]>`（default-init，Linux lazy commit，stable pointer） |
| **B8** | bound 语义 | **`memory_read/write` bound = injected `backing_size_`**（非 `kDefaultMemSize`）；小测试 backing 越界返 `-EINVAL` 而非堆破坏 |
| **B9** | SLVERR 传播 | **`handle_slave_port` 必须检查 `memory_read/write` 返回值**：失败置 `resp.resp.write(1)` (SLVERR)，**不**静默成功 |
| **B10** | 线程契约 | **保留 B4 的 mutex 部分**（去 lazy 保 mutex）：`sim_thread_` + host mmio 真实并发（`dgpu_board_shell.cc:171`），mutex 不能全删；lazy 部分随所有权上提消失 |
| **B11** | 双 size 拆分 | **`bar1_window_size_` (BAR1 窗口 = `bar_sizes[1]`) vs `vram_size_` (VRAM = `bar_sizes[2]`)**：BAR1 fast-path bound 用前者，5 消费者注入 size 用后者；避免 host ABI 越 BAR1 窗口 |
| **B12** | -ENODEV 语义 | **未注入 backing 时 `memory_read/write` 返 `-ENODEV`**（spec 新增错误码）；`has_memory_backing() = backing_ptr_ != nullptr`；`kRegMemSizeLo/Hi` 未注入时报 0 |
| **B13** | 24-case 非机械 | **`test_pcie_memory_device_basic.cc` 4 处语义反转**：`has_memory_backing` 初值仍 false 但后续依赖注入；lazy alloc 触发测试（line 96-102）整段删除；round-trip 测试改 fixture 注入；OOB bound 改为 injected size。**测试处置表必须逐条列出** |

## v1.3 P0 修正清单（保留）+ v1.4 设计修正（**实施前必须完成**）

> v1.2 P1 修订 (8 must-fix N1-N8) **未拦住以下 6 项阻塞错误**，第三方审查补刀：

| ID | 性质 | 位置 | 修正要点 |
|----|------|------|----------|
| **B1** | 编译级错误 | design.md §6 + tasks T3.2 | DmaDescriptor 字段名: `dst_iova_offset`/`len` → **`host_iova`/`size`/`vram_offset`**（实际字段名 per `dma_descriptor_mvp.hh:44-48`） |
| **B2** | spec 内部矛盾 | spec.md:60-62 vs spec.md:161-165 | SDMA "5 端口整体切 AxiMemBundle" 与 AxiMemBundle "SHALL NOT 出现在 host↔board 端口" 互冲 → **minimal_v1 范围限定**：仅切 `mem_in`/`mem_out` (chip-internal)；`desc_in`/`done_out`/`host_out` 保持 PcieTlpBundle（minimal_v1 不实际接线这 3 端口） |
| **B3** | 伪命题 fix | design.md §4 + tasks T1.x + spec N4 | `adapters_[2] + tick 双 adapter` 基于错误前提（"port-1 永不被 tick"）→ **撤销**。事实：`module_factory.cc:695-705` 对 multi-port 只注入**单个** `MultiPortStreamAdapter`，其内部 `tick()` 已遍历全端口（`multi_port_stream_adapter.hh:56-66`）。N4 改为"验证单 adapter tick 后两个 resp_out 都出" |
| **B4** | **v1.4 REVERTED** | design.md §4 + spec N10 | v1.3 计划"lazy alloc + mutex"；v1.4 改为：**所有权上提 DGpuBoard**（B7）后 lazy 部分消失，**保留 mutex 部分**（B10）；`kDefaultMemSize` 降级为 board 派生默认值（B8） |
| **B5** | 验收不可满足 | tasks T0.3 + spec.md Scenario | `ep->config_space().read(0x20) == 8589934592` 超出 `uint32_t` 返回值（8GB = 0x2_0000_0000 溢出）→ **改为 64-bit BAR 双 dword 编码**：`read(0x20) == 0`（低 32-bit），`read(0x24) == 2`（高 32-bit 表示 8GB） |
| **B6** | 注册缺失 | tasks T2.4 + design §4 | 仅写 `registerAdapter` 缺 `registerObject` → **补双注册**：`ModuleFactory::registerObject<PcieMemoryDevice>("PcieMemoryDevice")` + `registerMultiPortAdapter<PcieMemoryDevice, AxiMemBundle, AxiMemBundle, 2>("PcieMemoryDevice")`；GmmuTLM 同理 |

## v1.4 P0 修正任务 (前置, 0.5-1d, **必须在 v1.3 P0 之后**)

### P0.1 B7 - 单一 VRAM backing 所有权上提
- `include/tlm/gpu/dgpu_board_shell.hh`: 加 `std::unique_ptr<uint8_t[]> vram_storage_` + `uint64_t vram_size_` + `uint64_t bar1_window_size_` 三个成员；删 `framebuffer_storage_`（或改名+语义保持）
- `src/tlm/gpu/dgpu_board_shell.cc::init()`:
  - `vram_storage_ = std::unique_ptr<uint8_t[]>(new uint8_t[kDefaultVramSize])`（**default-init, 禁零初始化**）
  - `vram_size_ = bar_sizes[2]`（来自 `pcie_ep.params.bar_sizes`）
  - `bar1_window_size_ = bar_sizes[1]`（保持现行）
  - **若 `bar_sizes[2] == 0`**: 不分配 vram（device 后续 `set_backing_store` 返 -ENODEV）；若 `memory_routing_enabled=true && bar_sizes[2]==0` → fail-fast 报错
- `include/tlm/gpu/pcie_memory_device.hh`: 删 `memory_backing_` + `ensure_memory_backing_allocated`；加 `set_backing_store(uint8_t*, uint64_t)`（**对齐 `memory_tlm.hh:82` 签名**）+ `uint8_t* backing_ptr_` + `uint64_t backing_size_` + `std::mutex backing_mutex_`（保留 B4 mutex 部分）
- `src/tlm/gpu/pcie_memory_device.cc::memory_read/write`: bound 改 `backing_size_`（**非 kDefaultMemSize**）；`backing_ptr_ == nullptr` 返 `-ENODEV`（spec 新增错误码）；`has_memory_backing() = backing_ptr_ != nullptr`
- `handle_slave_port(p)`: **检查 memory_read/write 返回值**；失败置 `resp.resp.write(1)` (SLVERR)
- `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()`:
  - 加 `pcie_memory->set_backing_store(vram_storage_.get(), vram_size_)`（**与其他 set_*_store 同步调用, 传同一 vram_size_ 变量**）
  - 4 个 `set_*_store` 调用点（memory/sdma/gmmu/pcie_memory）必须传同一 `vram_size_`，**禁各自派生**
  - BAR1 fast-path bound 改用 `bar1_window_size_`（**非 framebuffer_size_**）

### P0.2 B8 - bound 语义统一
- design.md §4 修订：明确 `bound = injected backing_size_`
- spec.md Requirement 修订：`memory_read/write` bound 检查用 `backing_size_`；`kDefaultMemSize` 降级为 board 派生默认值
- 加测试 fixture：`MemoryDeviceFixture { std::unique_ptr<uint8_t[]> buf{new uint8_t[kTestSz]}; PcieMemoryDevice dev; Fixture() { dev.set_backing_store(buf.get(), kTestSz); } };` 其中 `kTestSz` 按需（4KB / 64MB / 8GB）

### P0.3 B9 - SLVERR 传播
- design.md §4 handle_slave_port 伪码修订：
  ```cpp
  int r = is_read ? memory_read(off, resp.data_buf.data(), len)
                   : memory_write(off, req.data_buf.data(), len);
  if (r != 0) { resp.resp.write(1); }  // SLVERR
  else        { resp.resp.write(0); }  // OKAY
  ```
- spec.md Scenario 新增："AXI read with null backing → SLVERR (resp=1)"
- spec.md Scenario 新增："AXI write beyond backing_size_ → SLVERR (resp=1)"

### P0.4 B10 - mutex 保留
- design.md §4 明确：PcieMemoryDevice::memory_read/write **保留 std::mutex**（保护 sim_thread + host mmio 并发访问）；lazy alloc 部分删除（B7 所有权上提后无需）
- 注释更新：说明 "backing_ptr_ 永不变（unique_ptr default-init stable pointer），mutex 仅用于并发访问保护"

### P0.5 B11 - 双 size 字段
- `dgpu_board_shell.{hh,cc}`: 加 `bar1_window_size_` 字段（来自 `bar_sizes[1]`）
- 所有 BAR1 fast-path 边界检查改用 `bar1_window_size_`（`dgpu_board_shell.cc:306/447` 两处）
- 5 消费者（memory/sdma/gmmu/pcie_memory/backdoor）注入 size 用 `vram_size_`
- spec.md Requirement 修订："BAR1 窗口 vs VRAM 大小区分"

### P0.6 B12 - -ENODEV + size 寄存器
- spec.md 新增错误码：`-ENODEV`（未注入 backing 时 `memory_read/write` 返回）
- `pcie_memory_device.hh`: `kRegMemSizeLo/Hi` 改为读 `backing_size_`（**非常量**）；未注入时报 0
- spec.md Scenario 新增："未注入 backing 时 read kRegMemSizeLo/Hi → 0"

### P0.7 B13 - 24-case 逐条处置表

| 测试 (line) | 当前行为 | v1.4 处置 |
|-------------|---------|----------|
| `basic.cc:63-69` round-trip 无注入 | lazy alloc 返 0 | 改 fixture 注入 → round-trip |
| `basic.cc:72-77` OOB 测试 (bound=kDefaultMemSize) | 写 ≥ 8GB 返 -EINVAL | 改 fixture 注入 4KB → 写 ≥ 4KB 返 -EINVAL |
| `basic.cc:91-93` `has_memory_backing()==false` | 初始 false | 保留（未注入即 false） |
| `basic.cc:96-102` "memory_write 触发 lazy alloc" | 测懒加载 | **整段删除** |
| `backing.cc:18` "lazy alloc (small)" | 测懒加载 | **删除** |
| `routing_characterization.cc` * | 走 board 路径 | 加 `pcie_memory->set_backing_store(vram_ptr_, vram_size_)` |
| `routing_flag.cc` * | 走 board 路径 | 同上 |
| 其他 `[pcie-memory]` 12+ 文件 | EP 默认 nullptr 适配 | **机械注入**：fixture 加 `dev.set_backing_store(...)` |
| **`[minimal_dgpu_soc][e2e]` 41 + `[abi]` 28** | 现有 `bar_sizes:[4096,16777216]` 无 BAR2 | **添加 `bar_sizes:[..., 8589934592]`**，否则 vram=0、device 未注入、AXI 路径失败 |
| **`[sdma][h2d]` 12 文件 + dgpu_board_v1 legacy** | 走 framebuffer 注入 | 不变（注入指针改指 vram_storage_，接口不动） |

## v1.4 P0 修正任务 (前置, 0.5-1d, **必须在 v1.3 P0 之后**)

### P0.0 v1.4 遗留议题显式声明 (D1 FB / doorbell / MemoryTLM capacity)
- design.md §13 加 3 行 v1.4 延期声明 (冻结面约束 + 测试合成 + capacity 不静默)
- spec.md Requirement 新增 "v1.4 遗留议题显式声明" + 3 Scenario
- tasks.md 本节 P0.0 已落 (无代码变更; 仅文档对齐)
- **D3 seam 已预留**: `handle_slave_port` ↔ `backing_ptr_` 之间可插入 VramControllerTLM/MemoryClusterTLM, 无 API 变更

### P0.1 修正 design.md §4/§6（编译级 + 验收级）
- §4 PcieMemoryDevice: 删 `adapters_[2]` 数组，改存单 `MultiPortStreamAdapter* adapter_`；删构造时预分配（保留 lazy alloc + mutex）
- §6 SDMA: 字段名改 `host_iova`/`size`/`vram_offset`；切型范围限定 minimal_v1 仅 `mem_in`/`mem_out`

### P0.2 修正 spec.md B2/B5（spec 内部一致性 + 验收可满足）
- B2: 新增 "minimal_v1 SDMA 切型范围" Scenario（明确 `desc_in`/`done_out`/`host_out` 保持 PcieTlpBundle + minimal_v1 不接线）
- B5: 改 BAR Scenario 为 64-bit 双 dword 编码

### P0.3 修正 tasks.md（自洽）
- T0.3 BAR 断言改 64-bit 双 dword
- T1.1/T1.2 删 N4 双 adapter 测试，改单 adapter round-trip
- T2.4 补 `registerObject` 双注册
- T3.2 字段名改 + 切型范围限定

### P0.4 验证
```bash
openspec validate --changes --strict
# 期望: PASS (3/3)
```

## v1.5 P0 修正清单 (在 v1.3 P0 + v1.4 P0 之上叠加, **实施前必须完成**)

> Oracle 三轮 + Metis 三轮敌对评审 (2026-09-26) 发现 v1.4 在 "单一 VRAM 真源" 方向正确, 但仍残留**5 处隐藏的双存储/边界漂移/挂死风险**, 与 dual-VRAM 同构或更隐蔽。必须 P0 修正后才能进入 T1。

| ID | 性质 | 位置 | 修正要点 |
|----|------|------|----------|
| **B14** | dual-storage 第三实例 | `dgpu_board_shell.cc:532-591, 796-814` | **消灭 `vram_segments_` MAP**: backdoor_read/write 改走 `vram_storage_` 唯一路径 (3 路由合并为 1) |
| **B15** | dual-storage 矛盾 | `dgpu_board_shell.cc:166-169` (P0.1 二选一) | **强制删除 `framebuffer_storage_`**; `framebuffer_ptr_ = vram_storage_.get()` (消除 "或改名+语义保持" 二选一) |
| **B16** | dual-storage 复活通道 | `dgpu_board_shell.hh:238-241` + 4 文件 | **`attach_framebuffer_for_testing()` 优先级规则**: vram_storage_ 已分配时 attach 失败并 log WARN (防止测试绕开单一真源) |
| **B17** | 框架函数不存在 | `include/framework/stream_adapter.hh` | **新增 `Packet::payload_resize(uint64_t n_bytes)`** + `PacketPool::acquire_with_min_size(min_bytes)`; T0.2 必失败路径 |
| **B18** | 虚假声明 | `memory_tlm.hh:95-99` + `dgpu_soc_minimal_v1.json:41` | **`on_config_loaded()` 实际读取 `capacity_gb`** 调 `set_size_bytes(gb<<30)` (消除 "1GB cap 不存在" 谎言) |
| **B19** | 边界漂移 | `sdma_engine_tlm.cc:201-206` + `dgpu_soc_minimal_v1.json:31` | **SDMA `vram_size_bytes` 与 board `vram_size_` 同步**: T4.1 JSON 删除 sdma.params.vram_size_bytes, 由 `bind_memory_backings` 统一注入 `vram_size_` |
| **B20** | 生产路径挂起 | `dgpu_board_shell.cc:623, 629` (line 621-632 旧块) | **`set_translate_cb` + `set_sdma_engine` 必须无条件注入**: 不在 pcie_memory 分支内 (无论 minimal_v1 / legacy 都需); 否则 SDMA 静默挂起 |
| **B21** | 模板契约违反 | `include/tlm/gpu/gmmu_tlm.hh` | **GMMU 添加 dummy `resp_out()` + `req_in()`** (对齐 MemoryTLM 模式, line 176-183); `registerAdapter` 模板需要 |
| **B22** | 永久挂死 | design.md §5 GMMU async | **GMMU async 路径加 `pte_addr+8 > backing_size_` bound check** + **SLVERR 加 retry_latch** (max_retry=16, 超过则置 fault_done) |
| **B23** | 永久挂死 | design.md §6 SDMA retry driver | **translate 非 0/非 -EAGAIN 路径**: emit `done_out` with `status=-EIO` + 移除 inflight_; **不能留注释** "/* 错误处理 */" |
| **B24** | 处置表漏 | tasks.md:86-97 (B13 表) | **补 `test_pcie_memory_device_basic.cc:118-125`**: EP tick 不再推进 memory_device cycle_counter → 必须删除该断言 (N8 副作用) |
| **B25** | 框架限制 | design.md §6 SDMA ↔ PcieMemoryDevice | **明确 chip-internal 端口 wire-format 改造决策**: `MultiPortStreamAdapter<SdmaEngineTLM, PcieTlpBundle, 5>` 同构保持; mem_in/mem_out 经 helper `to_axi_mem_descriptor`/`from_axi_mem_completion` 在 SDMA 内部完成包内转换; PcieMemoryDevice::handle_slave_port 接收的仍是 PcieTlpBundle (而非 AxiMemBundle); **统一为 PcieTlpBundle, PcieMemoryDevice 改 PcieTlpBundle SlavePort** |
| **B26** | backdoor bound 二义 | `dgpu_board_shell.cc:505-511` | **`backdoor_read/write` bound = `vram_size_`** (host 特权, 不受 BAR 窗口约束); 删 framebuffer 路径分支 |
| **B27** | 装饰死代码 | `pcie_memory_device.hh:37-38` (kRegMemSizeLo/Hi) | **BAR0 不路由到 pcie_memory** → `kRegMemSizeLo/Hi` 改在 `mmio_regs_` (board 层) 而非 pcie_memory 内部; 简化 ctor |
| **B28** | spec 三方矛盾 | spec.md 多处 | **`backdoor` + `BAR2 mmio` + `MemoryTLM` + `SDMA legacy` 4 消费者统一 bound = `vram_size_`** (排除 16MB / 1GB 异类); `capacity_gb` 在 minimal_v1 改为 8GB (与 vram 一致) 或显式 1GB 接线 |

## v1.5 P0 修正任务 (前置, 0.5-1d, **必须在 v1.3 P0 + v1.4 P0 之后**)

### P0.8 B14 - 消灭 vram_segments_ 双存储
- `include/tlm/gpu/dgpu_board_shell.hh`: 删 `std::map<uint64_t, std::vector<uint8_t>> vram_segments_` 成员 (line 306); 改注释 "SOC deferred 期间 shell 本地持有..."
- `src/tlm/gpu/dgpu_board_shell.cc::backdoor_read` (line 532-543): 删 vram_segments_ 分支; 改为直接读 `vram_storage_[offset]` (与 backdoor_write 同源)
- `src/tlm/gpu/dgpu_board_shell.cc::backdoor_write` (line 589-591): 删 vram_segments_ 写入; 改为直接写 `vram_storage_[offset]`
- `src/tlm/gpu/dgpu_board_shell.cc:796-814`: async backdoor 路径同样删 vram_segments_

### P0.9 B15 - 强制删除 framebuffer_storage_
- `include/tlm/gpu/dgpu_board_shell.hh`: 删 `std::vector<uint8_t> framebuffer_storage_` 成员; `framebuffer_ptr_` 改 `uint8_t* framebuffer_ptr_ = vram_storage_.get()` (在 init 中赋值)
- `src/tlm/gpu/dgpu_board_shell.cc::init()` (line 166-169): 删 `framebuffer_storage_.resize(...)`; 删 P0.1 "或改名+语义保持" 二选一
- **强制**: `framebuffer_storage_` 不存在; 所有 4 消费者 (backdoor/BAR1 fast-path/MemoryTLM/SDMA legacy) 通过 `vram_storage_` 唯一指针

### P0.10 B16 - attach_framebuffer_for_testing 优先级规则
- `include/tlm/gpu/dgpu_board_shell.hh::attach_framebuffer_for_testing()` (line 238-241): 加 guard — 若 `vram_storage_` 已分配 (framebuffer_ptr_ != nullptr), log WARN 并拒绝 (返回 false 或 no-op); 决定有效继承 vram_storage_ 路径
- `test/test_dgpu_board_framebuffer.cc` + `test_dgpu_board_shell_*`: 加测试覆盖 attach-after-init 场景

### P0.11 B17 - 新增 payload_resize 框架函数
- `include/core/packet.hh` (或类似): 新增 `Packet::payload_resize(uint64_t n_bytes)` (保留 data ptr, 调整 capacity, 重新分配 data if needed)
- `include/core/packet_pool.hh`: 新增 `PacketPool::acquire_with_min_size(uint64_t min_bytes)` (返回保证 ≥min_bytes 容量的 packet)
- `include/framework/stream_adapter.hh::OutputStreamAdapter::send()`: 调用 `pkt->payload->set_data_length(sizeof(BundleT))` (在 serialize 前)
- `include/framework/stream_adapter.hh::InputStreamAdapter::process()`: 调用 `ensure_payload_capacity(sizeof(BundleT))`

### P0.12 B18 - MemoryTLM capacity_gb 真实接线
- `include/tlm/memory_tlm.hh::on_config_loaded()` (line 95-99): 实际实现:
  ```cpp
  void on_config_loaded() override {
      if (cfg_.contains("capacity_gb") && cfg_["capacity_gb"].is_number()) {
          set_size_bytes(cfg_["capacity_gb"].get<uint64_t>() * (1ULL << 30));
      }
  }
  ```
  (需要构造时保存 cfg 引用或 on_config_loaded 接受 cfg 参数)
- `configs/dgpu_soc_minimal_v1.json:41`: `capacity_gb` 改为 8 (与 vram 一致) — 显式声明 "minimal_v1 中 MemoryTLM 也用全部 vram"

### P0.13 B19 - SDMA vram_size_bytes 同步
- `src/tlm/gpu/sdma_engine_tlm.cc::on_config_loaded` (line 189-191): 删 `cfg.value("vram_size_bytes", ...)` 自动读取; 改为只由 `set_vram_size_bytes()` 注入 (board 在 `bind_memory_backings` 调)
- `include/tlm/gpu/sdma_engine_tlm.hh:349`: `vram_size_bytes_` 默认值改为 0 (未注入时 SDMA 不接受 desc; flag 触达 `desc.vram_offset >= 0` → 不拦截)
- `configs/dgpu_soc_minimal_v1.json:31`: sdma.params 删 `vram_size_bytes`; 由 board 注入

### P0.14 B20 - set_translate_cb 无条件注入
- `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()`: `set_translate_cb` + `set_sdma_engine` **从 pcie_memory 分支移到主函数顶部** (line 621-632 块移出 if-else); `set_vram_backdoor` 保持条件注入 (legacy path)

### P0.15 B21 - GMMU dummy 方法
- `include/tlm/gpu/gmmu_tlm.hh`: 仿 `memory_tlm.hh:176-183` 加:
  ```cpp
  cpptlm::OutputStreamAdapter<bundles::AxiMemBundle>& resp_out() {
      static cpptlm::OutputStreamAdapter<bundles::AxiMemBundle> dummy;
      return dummy;
  }
  cpptlm::InputStreamAdapter<bundles::AxiMemBundle>& req_in() {
      static cpptlm::InputStreamAdapter<bundles::AxiMemBundle> dummy;
      return dummy;
  }
  ```

### P0.16 B22/B23 - Fault Path 显式化 (防永久挂死)
- design.md §5 GMMU translate: 加 `if (pte_addr + 8 > backing_size_) return -EIO;` (在发 AXI 读前)
- design.md §5 GMMU 状态机: 加 `retry_latch_` (uint8_t) 计数 SLVERR 重发, >16 置 `state_=FAULT` (后续 -EIO + 移除 inflight_)
- design.md §6 SDMA `process_inflight_step`: 替换 `if (tr != 0) { /* 错误处理 */ return; }` 为:
  ```cpp
  if (tr != 0 && tr != -EAGAIN) {
      // emit done_out with status = -EIO; remove from inflight_
      // 否则永久挂死 FIFO 队头
      e.state = State::DONE;
      // emit done 立即
      return;
  }
  ```

### P0.17 B24/B25/B26/B27/B28 - 杂项修订
- B24: tasks.md B13 表补 `basic.cc:118-125` (EP tick 不再推进 cycle_counter → 删该断言或改为 mock 设备直接 tick)
- B25: design.md §6 明确 SDMA ↔ PcieMemoryDevice 端口**统一为 PcieTlpBundle** (PcieMemoryDevice SlavePort wire-format 改 PcieTlpBundle 而非 AxiMemBundle); `to_axi_mem_descriptor`/`from_axi_mem_completion` 在 SDMA 内部完成; 放弃 v1.3 B2 "mem_in/mem_out 切型" 路径
- B26: `dgpu_board_shell.cc::backdoor_read/write`: bound 统一为 `vram_size_` (不受 BAR1 窗口约束); 删 framebuffer fast-path 分支
- B27: 简 PcieMemoryDevice `kRegMemSizeLo/Hi` 实现: 改在 DGpuBoard::mmio_regs_ 烧录 (不动 pcie_memory.registers_); 或保留 + 加 board 不可达注释
- B28: `configs/dgpu_soc_minimal_v1.json`: memory.params.capacity_gb 从 1 改 8 (B12 一致); 或显式 1 接线说明

### P0.18 验证
```bash
openspec validate cpptlm-driver-visible-minimal-soc --strict
# 期望: PASS
grep -rn "vram_segments_\b" src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh  # 应为零匹配
grep -rn "framebuffer_storage_" src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh  # 应为零匹配
```

## P1 修订 (前置, 0.5d)

### P1.1 8 must-fix 落地 (v1.2 N1-N8)
- design.md §3 标 AxiMemBundle 需框架 payload 扩容 (N1)
- design.md §5 修 GMMU COMPLETE/WAIT iova 匹配 (N2)
- design.md §6 修 SDMA retry driver (N3) + slot-2 resp (N7)
- tasks.md 显式列既有测试机械迁移 (N5) + EP BAR 实现 (N6) + EP 不双 tick (N8)
- spec.md DoD 修订：删 "可完整枚举" 过强表述，N11 改 "互不解引用"

### P1.2 验证
```bash
openspec validate --changes --strict
# 期望: PASS (3/3) — 与 P0.4 合并为一次校验
```

## v1.6 P0 修正清单 (在 v1.5 P0.8-P0.18 之上叠加, **实施前必须完成**)

> Oracle 第三轮严格审计 (2026-09-27) 在 v1.5 基础上发现 **11 项新隐藏缺陷 (F1-F11)** + **MemoryTLM 移除决策 (F12)**。叠加 v1.5 B14-B28 构成完整 P0 集合。**演进路线图** (X.1-X.6) 同步写入 design.md §X。

| ID | 性质 | 位置 | 修正要点 |
|----|------|------|----------|
| **F1** | 致命 100% 失败 | `gmmu_tlm.hh` (async translate) | **GMMU 本地删除 bound check**: `pte_addr+8 > backing_size_` 检查在 minimal_v1 N12 不注入 backing 时 backing_size_=0 → 恒返 -EIO → 所有 SDMA fault → E2E 100% 失败。**修复**: GMMU 本地删除该 bound check, SLVERR 归 PcieMemoryDevice `handle_slave_port` 的 SLVERR 负责 (B8/B9 已覆盖); GMMU 只保留 retry_latch |
| **F2** | spec × tasks 硬矛盾 | spec.md:465/474/484-486 vs tasks.md B28/P0.12 | spec 写 `capacity_gb=1`, tasks 写 `capacity_gb` 改 8。**若采纳 F12 (MemoryTLM 移除)**: 矛盾蒸发, 无需修改; **若保留 MemoryTLM**: 统一 `capacity_gb=8` 并改 Scenario |
| **F3** | T1.7 伪码违例 | tasks.md:374 | `memory_->set_backing_store(framebuffer_ptr_, framebuffer_size_)` 传 16MB, spec 要求"同一 vram_size_"。**修复**: 随 F12 删除该行; 或改传 `vram_size_` |
| **F4** | 架构 zombie | `pcie_memory_device.hh:37-38,45-46` | `registers_` 不可达 + kVendorId=0x1002/kDeviceId=0x0002 与 EP config space 0x10DE:0x1234 矛盾双身份。**修复**: 删除 `registers_`/`mmio_read`/`mmio_write`/双身份常量; B27 二选一改强制删除; [pcie-memory] mmio 测试并入 B13 处置表 |
| **F5** | B14 死代码残留 | `dgpu_board_shell.cc:592-602, 794-819` | backdoor_write 仍 push `is_backdoor=true` PendingReq; drain `is_backdoor_read=true` 无人 enqueue。**修复**: B14 补删 push + drain 两分支 + `last_backdoor_reads_` + PendingReq 两个 flag 字段 |
| **F6** | driver-visible 语义谎言 | spec.md + JSON | BAR0 mmio_read 是 write-mirror; JSON `SDMA_STATUS @16 ro` 永远读不出真设备状态。**修复**: spec 显式声明 "BAR0 = write-mirror, SDMA_STATUS 不反映真设备状态"; 或删 JSON SDMA_STATUS 声明 |
| **F7** | fault path 挂死 | `sdma_engine_tlm.cc` done_out 路径 | SDMA `done_out` 在 minimal_v1 无 connections (未接线); B23 fault path `emit done_out with status=-EIO` 写到未连接端口。**修复**: spec 显式声明 "done_out 悬空 + write-to-unconnected 静默丢弃"; B23 fault path 同步触发 board error callback (复用 `trigger_error_async`) |
| **F8** | CompletionRingTLM dormant | `completion_ring_mvp.hh` | completion 模块在 minimal_v1 同 dormant (零连接)。**修复**: 与 F12 同步移除或标 dormant; spec 声明 "dormant in minimal_v1" |
| **F9** | 文档一致性 | design.md §1 + AGENTS.md | design §1 数 5 消费者 (含 backdoor), user 视角 4 (不含)。**修复**: 统一为 "4 注入点 (memory/sdma/gmmu/pcie_memory) + 1 host backdoor (非注入)" |
| **F10** | legacy 静默退化 | `dgpu_board_shell.cc:609-613` | `bind_memory_backings` 起始早退 (`framebuffer_ptr_==nullptr` 即返); v1.5 后 `framebuffer_ptr_=vram_storage_.get()`, 若 legacy 配置 `bar_sizes[2]==0` → vram 不分配 → **legacy 注入也静默跳过**, fail-fast 只覆盖 `memory_routing_enabled=true`。**修复**: legacy 分支 (无 pcie_memory 且 vram 未分配) 加 WARN/fail-fast |
| **F11** | 文档实施状态标记 | tasks.md 全文件 | v1.5 文档与代码两版本错位, 无单一 "implemented-through" 标记; B 项实施状态混杂。**修复**: tasks.md 每项 B 加 `[x]` checkbox 或 "implemented-in: <commit>" 列 |
| **F12** | MemoryTLM 移除决策 | `configs/dgpu_soc_minimal_v1.json:38-42` + spec | MemoryTLM 在 minimal_v1 部署冗余 (零连接零消费者, pre-D2 时代遗留); 保留只引入 bound 漂移面 (B18/B28 三方矛盾源头)。**修复**: 从 `configs/dgpu_soc_minimal_v1.json` 移除 `memory` 模块; spec scenario "soc 内有 6 个模块" 改 5; B18/B28 的 minimal_v1 部分标 N/A (B18 框架修复仍保留给其他 config) |

## v1.6 P0 修正任务 (前置, 0.5-1d, **必须在 v1.5 P0.8-P0.18 之后**)

### P0.19 (F1) — GMMU 删除本地 bound check, SLVERR 归 PcieMemoryDevice
- `include/tlm/gpu/gmmu_tlm.hh`: 删 `if (pte_addr + 8 > backing_size_) return -EIO;` (在 async translate 路径, 该检查在 minimal_v1 N12 不注入 backing 时恒触发)
- design.md §5 GMMU 状态机: 删该 bound check; `handle_slave_port` 的 SLVERR (B8/B9) 已覆盖 bound 违规场景
- `gmmu_tlm.hh` 仍保留 `retry_latch_` (max_retry=16, 超过则置 fault_done)
- **TDD 5 步**:
  1. RED: `[gmmu][async]` 测试在 minimal_v1 (无 backing 注入) SDMA H2D → 100% fault → FAIL
  2. GREEN: GMMU 删 bound check 后同一测试 PASS
  3. 验证: `grep -n "pte_addr + 8 > backing_size_" include/tlm/gpu/gmmu_tlm.hh` → 零匹配
  4. 验证: `[gmmu][async]` 测试 PASS
  5. commit: `fix(gmmu): 删除 async 路径本地 bound check, SLVERR 归 PcieMemoryDevice (F1)`

### P0.20 (F2) — spec × tasks capacity_gb 矛盾消解
- **若采纳 F12 (MemoryTLM 移除)**: 无需修改, 矛盾随 MemoryTLM 一起消失
- **若保留 MemoryTLM**: `configs/dgpu_soc_minimal_v1.json` 的 `memory.params.capacity_gb` 改为 8 (与 vram 一致); spec.md Scenario 修订为 8GB; tasks.md P0.12 同步修订
- **TDD 5 步**:
  1. RED: spec.md Scenario "MemoryTLM 1GB" 与 tasks "capacity_gb 改 8" 并存 → openspec validate 报矛盾
  2. GREEN: F12 采纳或 capacity_gb 统一为 8 后矛盾消解
  3. 验证: `grep "capacity_gb" configs/dgpu_soc_minimal_v1.json` → 8 或 memory 模块已删除
  4. 验证: `openspec validate cpptlm-driver-visible-minimal-soc --strict` → PASS
  5. commit: `fix(spec): 统一 capacity_gb=8 或移除 MemoryTLM 消解矛盾 (F2)`

### P0.21 (F3) — T1.7 伪码 `set_backing_store` 参数违例
- `tasks.md:374`: 删 `memory_->set_backing_store(framebuffer_ptr_, framebuffer_size_)` (随 F12 MemoryTLM 移除一起删除); 或改为 `memory_->set_backing_store(framebuffer_ptr_, vram_size_)` (若保留 MemoryTLM)
- **TDD 5 步**:
  1. RED: T1.7 伪码传 16MB 而 spec 要求 "同一 vram_size_"
  2. GREEN: 该行删除 (F12) 或参数改为 vram_size_ 后违例消失
  3. 验证: `grep -n "set_backing_store.*framebuffer_size_" tasks.md` → 零匹配
  4. 验证: `[minimal_dgpu_soc][driver_visible]` E2E 测试 PASS
  5. commit: `fix(tasks): 修正 T1.7 set_backing_store 参数为 vram_size_ (F3)`

### P0.22 (F4) — PcieMemoryDevice zombie 装饰 (registers_ + 双身份)
- `include/tlm/gpu/pcie_memory_device.hh`: 删 `registers_` 数组 + `mmio_read`/`mmio_write` 方法 + `kVendorId=0x1002`/`kDeviceId=0x0002` 常量 + `registers_{}` 初始化
- `src/tlm/gpu/pcie_memory_device.cc`: 删 `mmio_read`/`mmio_write` 实现
- B27 处置: `kRegMemSizeLo/Hi` 已在 DGpuBoard::mmio_regs_ 烧录, pcie_memory 内部不再需要
- [pcie-memory] mmio 测试并入 B13 处置表: `test_pcie_memory_device_mmio.cc` 系列标记 N/A (minimal_v1 无 MMIO 路由)
- **TDD 5 步**:
  1. RED: `[pcie-memory][mmio]` 测试因 registers_ 不可达/双身份矛盾 FAIL
  2. GREEN: 删除 registers_ + MMIO 方法 + 双身份常量后编译通过
  3. 验证: `grep -n "registers_\|kVendorId\|kDeviceId\|mmio_read\|mmio_write" include/tlm/gpu/pcie_memory_device.hh` → 零匹配
  4. 验证: `[pcie-memory]` 非 mmio 测试 PASS
  5. commit: `refactor(pcie-memory): 删除 zombie registers_ + MMIO + 双身份常量 (F4/B27)`

### P0.23 (F5) — B14 死代码残留补删
- `src/tlm/gpu/dgpu_board_shell.cc::backdoor_write` (line 592-602): 删 `is_backdoor=true` PendingReq push 分支 (B14 已删 vram_segments_ 写入, 但 push 仍残留)
- `src/tlm/gpu/dgpu_board_shell.cc::drain_inject_q` (line 794-819): 删 `is_backdoor_read=true` drain 分支 + `last_backdoor_reads_` map + PendingReq `is_backdoor`/`is_backdoor_read` 两个 flag 字段
- `dgpu_board_shell.hh`: 删 `std::map<uint64_t, std::vector<uint8_t>> vram_segments_` (已在 B14 删除, 本次确认) + 删 `is_backdoor`/`is_backdoor_read` flag 定义
- **TDD 5 步**:
  1. RED: backdoor_write push + drain 残留 is_backdoor flag → `[board][backdoor]` 测试行为不确定
  2. GREEN: 补删 push + drain 分支 + flag 字段后编译通过
  3. 验证: `grep -n "is_backdoor\|is_backdoor_read\|last_backdoor_reads_" src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh` → 零匹配
  4. 验证: `[board][backdoor]` 测试 PASS
  5. commit: `fix(board): 补删 backdoor push/drain 残留 flag + last_backdoor_reads_ (F5/B14)`

### P0.24 (F6) — BAR0 write-mirror 语义显式声明
- spec.md: 新增 Scenario "BAR0 = write-mirror, SDMA_STATUS 不反映真设备状态"
- `configs/dgpu_soc_minimal_v1.json`: 删 `SDMA_STATUS @16 ro` 声明 (或加注释 "write-mirror, 读不出真值")
- design.md §4 PcieMemoryDevice: 加注释 "BAR0 MMIO 寄存器为 write-mirror, 不反映真实硬件状态"
- **TDD 5 步**:
  1. RED: JSON SDMA_STATUS 声明与实际行为 (write-mirror) 不符 → driver 期望读出真值 FAIL
  2. GREEN: spec 显式声明 write-mirror 语义 + JSON 加注释后 driver 期望调整
  3. 验证: `grep "SDMA_STATUS" configs/dgpu_soc_minimal_v1.json` → 有 "write-mirror" 注释
  4. 验证: spec.md 含 "BAR0 = write-mirror" 场景
  5. commit: `docs(spec): 显式声明 BAR0 write-mirror 语义 (F6)`

### P0.25 (F7) — done_out 悬空 + fault path board error callback
- spec.md: 新增 Scenario "done_out 悬空 + write-to-unconnected 静默丢弃"
- `sdma_engine_tlm.cc` B23 fault path: `emit done_out` 同步加 `trigger_error_async(status=-EIO)` (board 级别 error callback)
- design.md §6: 显式声明 "done_out 在 minimal_v1 未接线, 写到未连接端口静默丢弃"
- **TDD 5 步**:
  1. RED: done_out 写到未连接端口行为未定义 → E2E fault path 可能静默失败
  2. GREEN: spec 显式声明 + board error callback 触发后行为可预期
  3. 验证: `grep "done_out.*悬空\|write-to-unconnected" spec.md design.md` → 有声明
  4. 验证: fault path 注入后 `trigger_error_async` 被调用
  5. commit: `fix(sdma): done_out 悬空声明 + fault path 触发 board error callback (F7/B23)`

### P0.26 (F8) — CompletionRingTLM dormant 标记
- spec.md: 新增 Scenario "CompletionRingTLM 在 minimal_v1 为 dormant (零连接)"
- `configs/dgpu_soc_minimal_v1.json`: 删 `completion` 模块 (随 F12 MemoryTLM 移除一起处理, 或单独标记)
- design.md §1: completion 模块标注 "(dormant in minimal_v1)"
- **TDD 5 步**:
  1. RED: completion 模块在 minimal_v1 零连接但仍实例化 → 资源浪费
  2. GREEN: spec 显式声明 dormant + JSON 删除该模块后拓扑清晰
  3. 验证: `grep "completion" configs/dgpu_soc_minimal_v1.json` → 零匹配
  4. 验证: spec.md 含 "CompletionRingTLM dormant in minimal_v1" 场景
  5. commit: `refactor(spec): CompletionRingTLM 标 dormant 或移除 (F8)`

### P0.27 (F9) — 消费者计数统一
- design.md §1: 统一为 "4 注入点 (memory/sdma/gmmu/pcie_memory) + 1 host backdoor (非注入)"
- AGENTS.md: 同步更新 "KEY INVARIANTS" 或对应段落
- spec.md: BAR0 write-mirror 场景补充说明
- **TDD 5 步**:
  1. RED: design §1 说 5 消费者, user 视角文档说 4 → 计数不一致
  2. GREEN: 统一为 4 injection points + 1 backdoor 后文档自洽
  3. 验证: `grep -c "消费者\|injection\|backdoor" design.md AGENTS.md` → 计数一致
  4. 验证: openspec validate --strict PASS
  5. commit: `docs: 统一消费者计数为 4 injection + 1 backdoor (F9)`

### P0.28 (F10) — legacy 注入 fail-fast
- `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings` (line 609-613): legacy 分支 (无 pcie_memory 且 vram 未分配) 加 `WARN` log + fail-fast return 或 exception
- spec.md: 新增 Scenario "legacy 配置 vram 未分配 → fail-fast"
- **TDD 5 步**:
  1. RED: legacy 配置 (无 pcie_memory) + bar_sizes[2]==0 时 bind_memory_backings 静默跳过 → 调试困难
  2. GREEN: 加 WARN + fail-fast 后 legacy 配置错误可定位
  3. 验证: legacy 配置加载时 WARN 输出
  4. 验证: spec.md 含 fail-fast Scenario
  5. commit: `fix(board): bind_memory_backings legacy 分支加 WARN/fail-fast (F10)`

### P0.29 (F11) — tasks.md 实施状态标记
- tasks.md: 每项 B (B1-B28) 加 `[x]` checkbox (已实施) 或 "implemented-in: <commit>" 列
- tasks.md: 新增 "实施状态" 小节, 汇总 B1-B28 + F1-F12 状态
- v1.5 tasks.md 标注 "implemented-in: <v1.5 commit>" (需追溯)
- **TDD 5 步**:
  1. RED: tasks.md 无统一实施状态标记 → 无法快速确认哪些 B 项已实施
  2. GREEN: 每项 B 加 checkbox 或 implemented-in 列后状态透明
  3. 验证: `grep -c "\[x\]\|implemented-in:" tasks.md` → ≥28 (B1-B28) + ≥12 (F1-F12)
  4. 验证: 所有 P0 任务有 checkbox 或 implemented-in 标记
  5. commit: `docs(tasks): 添加 B1-B28 + F1-F12 实施状态标记 (F11)`

### P0.30 (F12) — MemoryTLM 移除决策
- `configs/dgpu_soc_minimal_v1.json`: 删除 `memory` 模块 (line 38-42 区域)
- `tasks.md:374`: 删除 `memory_->set_backing_store(framebuffer_ptr_, framebuffer_size_)` 行
- spec.md: "soc 内有 6 个模块" Scenario 改为 "soc 内有 5 个模块 (pcie_ep + pcie_memory + sdma + gmmu + completion)"
- spec.md: B18/B28 的 minimal_v1 部分标 N/A (B18 框架修复保留给其他 config)
- design.md §1: 5 消费者 (删 memory 后变为 4 injection points)
- B18/B28 关于 MemoryTLM capacity 的部分在 minimal_v1 范围内标 N/A, 但 B18 `on_config_loaded` 框架修复保留 (其他 config 仍需要)
- **TDD 5 步**:
  1. RED: `[minimal_dgpu_soc][driver_visible]` E2E 因 MemoryTLM 零消费者仍实例化可能行为不一致
  2. GREEN: 删除 memory 模块 + 更新所有文档后 5 模块拓扑清晰
  3. 验证: `grep -c "memory" configs/dgpu_soc_minimal_v1.json` → 零匹配 (memory 模块删除)
  4. 验证: spec.md "soc 内有 5 个模块" Scenario PASS
  5. commit: `refactor(minimal-v1): 移除 MemoryTLM (F12), 5 模块拓扑`

### P0.31 — 演进路线图落地 (design.md §X)
- design.md: 在 §13 后加 `## §X 演进路线图 (minimal → 完整 GPU)` 含 X.1-X.6
- spec.md: 新增 Requirement "演进路线图 SHALL 支持 minimal → 完整 GPU"
- tasks.md: 本任务为纯文档, 无代码变更
- **TDD 5 步**:
  1. RED: design.md 无演进路线图章节, user "架构可以支持演进" 无法验证
  2. GREEN: §X 演进路线图写入后 user 意图可追溯
  3. 验证: `grep "演进路线图\|## §X" design.md` → 有章节
  4. 验证: spec.md 含 "演进路线图 SHALL 支持 minimal → 完整 GPU" Requirement
  5. commit: `docs(design): 添加演进路线图 §X (minimal → 完整 GPU) (F12 演进)`

### P0.32 — v1.6 验证
```bash
openspec validate cpptlm-driver-visible-minimal-soc --strict
# 期望: PASS
grep -rn "registers_\|kVendorId\|kDeviceId\|mmio_read\|mmio_write" include/tlm/gpu/pcie_memory_device.hh
# 期望: 零匹配 (F4)
grep -rn "is_backdoor\|is_backdoor_read\|last_backdoor_reads_" src/tlm/gpu/dgpu_board_shell.cc include/tlm/gpu/dgpu_board_shell.hh
# 期望: 零匹配 (F5)
grep "capacity_gb" configs/dgpu_soc_minimal_v1.json
# 期望: memory 模块已删除 或 capacity_gb=8 (F2/F12)
grep "memory" configs/dgpu_soc_minimal_v1.json
# 期望: 零匹配 (F12)
```

## v1.7 P0 阻塞清单 (Oracle 第四轮 H1-H7)

> Oracle 第四轮审计 (2026-09-27) 发现 7 项 P0/P1 缺陷，必须在 v1.7 实施前全部完成。v1.7 最终满足用户目标："实现GPU里最基本的 H2D/D2H/D2D 能力，在最大化保存驱动代码兼容性同时，soc架构可以支持演进成完整的GPU架构"。

| ID | 性质 | 位置 | 修正要点 |
|----|------|------|----------|
| **H1** | P0 wire-format 矛盾 | spec.md:43/62 + design.md §1/§4/§5 + 实施笔记 §2.1 | **二选一写死 wire-format**: 采纳 B25 全 PcieTlpBundle（推荐）；同步 spec.md:43/62 + design §1/§4/§5 + 实施笔记 §2.1 改为 PcieTlpBundle；GMMU MasterPort 一并切 PcieTlpBundle；AxiMemBundle 标记"D3+ VramController 内部用" |
| **H2** | P0 descriptor 不可驱动 | sdma_engine_tlm.cc:527 + spec B2 | **ring-via-BAR1 约定**: BAR1 窗口内固定偏移区间为 descriptor ring 存储；board `mmio_write` 对该区间路由 `ring_write_entry`；`enable_ring_mode` 由 JSON params 或 board init 自动调用 |
| **H3** | P0 host 内存无注入者 | N9 + slimming 后 ABI | **host 内存约定**: spec 文档化"emulator 即 host: BAR1 窗口区域即 host 内存仿真"；board `bind_memory_backings` 无条件 `sdma->set_host_backdoor(vram_storage_.get(), bar1_window_size_)` |
| **H4** | P1 D2D false-success | sdma_engine_tlm.cc:489-490 | **D2D 诚实语义**: minimal_v1 D2D 走 mem_out 两拍 (read+write 同一 backing)，或 emit done `status=-ENOSYS`；**禁止** `status=0` + 零搬运 |
| **H5** | P1 doorbell 竞态 | dgpu_board_shell.cc:419-441 + sdma_engine_tlm.cc:466-478 | **doorbell handler 改造**: 仅 enqueue + 置 flag，实际 consume 挪入 `tick()`（与 N3 retry-driver 天然同构）；spec 显式声明"SDMA 状态仅 sim 线程触碰" |
| **H6** | P1 spec 数字漂移 | spec.md:314 vs F12 Scenario | **spec 一致化**: F12 Scenario "5 模块" + 旧 Scenario "6 模块" 矛盾消解；tasks.md 加 `[x]` checkbox / `implemented-in:` 列 (F11 落地) |
| **H7** | P2 资源 + 文档陈旧 | dgpu_board_shell.cc:405-414 + design §8 Step 7 | **资源上限 + 陈旧引用清理**: `inject_q_` 加 max size 限制 + DPRINTF WARN on overflow；design §8 Step 7 + spec Scenario 引用 `cpptlm_emulator_backdoor_write/read` 改写为 BAR1 mmio 语义 |

## v1.7 P0 修正任务 (前置, 1-2d, **必须在 v1.3+v1.4+v1.5+v1.6 P0 之后**)

### P0.33 (H1) — wire-format 写死为全 PcieTlpBundle

- spec.md:43/62 删 AxiMemBundle 路由描述，改为"PcieTlpBundle 统一 wire-format"
- design.md §1/§4/§5 全部改 PcieTlpBundle；AxiMemBundle 标注"D3+ VramController chip-internal 专用"
- docs/pcie/driver-visible-minimal-soc.md §2.1 对应修订
- `include/bundles/axi_mem_bundles_tlm.hh` 保留（给 VramController D3+ 用）

**TDD 5 步**:
1. RED: `[pcie-memory][axi]` 测试因 PcieTlpBundle resp 字段不匹配 FAIL
2. GREEN: 统一 wire-format 后同一测试 PASS
3. 验证: `grep "AxiMemBundle" spec.md design.md` → 仅限 VramController 相关章节
4. 验证: `[pcie-memory][axi]` 测试全 PASS
5. commit: `fix(spec/design): wire-format 写死为 PcieTlpBundle, AxiMemBundle 标 D3+ chip-internal (H1)`

### P0.34 (H2) — ring-via-BAR1 约定 + 自动 enable_ring_mode

- `dgpu_board_shell.cc::mmio_write`: BAR1 窗口内 ring descriptor 区间（固定偏移）路由到 `sdma->ring_write_entry(index, data, len)`
- `bind_memory_backings` 或 JSON params 自动调用 `sdma->enable_ring_mode(RingSize::KB_64, EntrySize::B_64)`
- spec.md 新增 Scenario: "ring-via-BAR1: driver 写 descriptor 到 BAR1 ring 区间 → SDMA 消费"

**TDD 5 步**:
1. RED: `[sdma][ring]` 测试因 ring-via-BAR1 路由缺失 FAIL
2. GREEN: mmio_write BAR1 ring 区间路由 + auto enable_ring_mode 后 PASS
3. 验证: `grep "ring_write_entry\|enable_ring_mode" src/tlm/gpu/dgpu_board_shell.cc src/tlm/gpu/sdma_engine_tlm.cc` → 存在
4. 验证: `[sdma][ring]` 测试 PASS
5. commit: `feat(board): ring-via-BAR1 路由 + auto enable_ring_mode (H2)`

### P0.35 (H3) — host backdoor 无条件注入

- `bind_memory_backings` 无条件调用 `sdma->set_host_backdoor(vram_storage_.get(), bar1_window_size_)`
- spec.md 文档化"emulator 即 host: BAR1 窗口区域即 host 内存仿真"
- H2 + H3 联动: host_backdoor 注入了 BAR1 窗口同一 backing

**TDD 5 步**:
1. RED: `[sdma][h2d]` 测试因 host backdoor 未注入 FAIL
2. GREEN: `bind_memory_backings` 无条件 set_host_backdoor 后 PASS
3. 验证: `grep "set_host_backdoor" src/tlm/gpu/dgpu_board_shell.cc` → 存在且无条件调用
4. 验证: `[sdma][h2d]` 测试 PASS
5. commit: `fix(board): 无条件 set_host_backdoor (H3)`

### P0.36 (H4) — D2D 诚实语义

- `sdma_engine_tlm.cc::process_d2d` 或对应路径: D2D 走 mem_out 两拍 (read+write 同一 backing)，或 emit done status=-ENOSYS
- **禁止** `status=0` + 零搬运（静默 false-success）

**TDD 5 步**:
1. RED: D2D descriptor 提交后 `status=0` 但数据未搬运
2. GREEN: D2D 走 mem_out 两拍或返 -ENOSYS 后行为诚实
3. 验证: `grep "ENOSYS\|D2D.*mem_out\|两拍" src/tlm/gpu/sdma_engine_tlm.cc` → 存在
4. 验证: D2D 测试 status 值非零或数据真实搬运
5. commit: `fix(sdma): D2D 诚实语义, 禁 status=0 零搬运 (H4)`

### P0.37 (H5) — doorbell handler 线程安全改造

- `dgpu_board_shell.cc:419-441`: doorbell 路径仅 enqueue + 置 flag，不同步 consume
- `sdma_engine_tlm.cc::tick()`: 实际 ring consume 移入 tick（与 N3 retry-driver 同构）
- spec 显式声明"SDMA 状态仅 sim 线程触碰，doorbell handler 仅负责 enqueue"

**TDD 5 步**:
1. RED: doorbell handler 并发调用导致状态不一致
2. GREEN: doorbell 仅 enqueue + tick() 内 consume 后 PASS
3. 验证: `grep "enqueue\|tick\|doorbell" src/tlm/gpu/dgpu_board_shell.cc src/tlm/gpu/sdma_engine_tlm.cc` → 分离逻辑存在
4. 验证: `[sdma][doorbell]` 并发测试 PASS
5. commit: `fix(board/sdma): doorbell 仅 enqueue, 实际 consume 移入 tick() (H5)`

### P0.38 (H6) — spec 数字一致化

- spec.md:314 F12 Scenario "5 模块" vs 旧 Scenario "6 模块" 矛盾消解
- tasks.md 每项 H 加 `[x]` checkbox 或 `implemented-in:` 列

**TDD 5 步**:
1. RED: spec.md 模块数前后矛盾 → openspec validate 报错
2. GREEN: 统一为"5 模块" + tasks.md 有 checkbox 后 PASS
3. 验证: `grep "5 模块\|6 模块" spec.md` → 仅"5 模块"
4. 验证: `grep "\[x\]\|implemented-in:" tasks.md | wc -l` → ≥35
5. commit: `docs(spec/tasks): spec 模块数一致化 + tasks.md checkbox (H6)`

### P0.39 (H7) — inject_q_ 资源上限 + 陈旧引用清理

- `dgpu_board_shell.cc::mmio_write`: `inject_q_` 加 `if (inject_q_.size() > MAX_INJECT_Q_SIZE) { DPRINTF WARN; return -EOVERFLOW; }`
- design §8 Step 7: `cpptlm_emulator_backdoor_write/read` 引用改 BAR1 mmio 语义

**TDD 5 步**:
1. RED: inject_q_ 无限增长导致内存耗尽
2. GREEN: 加 max size 限制 + WARN + 旧引用更新后 PASS
3. 验证: `grep "MAX_INJECT_Q_SIZE\|EOVERFLOW" src/tlm/gpu/dgpu_board_shell.cc` → 存在
4. 验证: `grep "backdoor_write\|backdoor_read" design.md` → 无陈旧引用
5. commit: `fix(board): inject_q_ 加资源上限 + 陈旧引用清理 (H7)`

### P0.40 — v1.7 验证 (用户目标验证)
```bash
openspec validate cpptlm-driver-visible-minimal-soc --strict
# 期望: PASS
grep -n "enable_ring_mode\|set_host_backdoor" src/tlm/gpu/sdma_engine_tlm.cc
# 期望: 自动调用点存在 (H2+H3)
grep -n "bar1_window_size_.*ring\|ring.*bar1_window" src/tlm/gpu/dgpu_board_shell.cc
# 期望: 路由逻辑存在 (H2)
grep "ENOSYS\|D2D.*mem_out" src/tlm/gpu/sdma_engine_tlm.cc
# 期望: D2D 诚实语义存在 (H4)
grep -n "MAX_INJECT_Q_SIZE\|EOVERFLOW" src/tlm/gpu/dgpu_board_shell.cc
# 期望: 资源上限存在 (H7)
# User goal Scenario (验证):
./build/bin/cpptlm_tests "[minimal_dgpu_soc][driver_visible]" --reporter compact
# 期望: ALL PASS (H2D/D2H/D2D 全通)
```

## v1.8 P0 normative 文本修订 (Oracle 第五轮 N1-N5 + revision)

> Oracle 第五轮审计 (2026-09-27) 发现 v1.7 是"决策记录而非修复应用"：v1.7 声称要消灭的 normative 文本矛盾原样保留。v1.8 是 Quick 纯文档收口（<1d，无代码变更），**真正应用 H1/H6 到 normative 文本 + 消解 doorbell 三方矛盾 + 显式化 4 项遗漏**。代码实施仍由 H1-H7 + 后续 T1-T4 任务承担。

| ID | 性质 | 位置 | normative 文本修订 |
|----|------|------|---------------------|
| **N1 实施** | 代码 bug，代码实施时修 | `sdma_engine_tlm.cc:547-551` | ring consume 路径 D2D 误路由为 D2H（line 548 `else { rc = process_d2h(d, done); }`）——必须显式分派 D2D dir 至 `process_d2d` 或显式返 -ENOSYS |
| **N2 实施** | spec.md normative 修订 | ✅ **已应用** spec.md:43 "PcieTlpBundle wire-format, board-level 一致" + spec.md:62 SHALL NOT 列表移除 + spec.md:314 "5 个模块 (pcie_ep + pcie_memory + sdma + gmmu + completion)" + spec.md:526-527 BAR1 合成地址空间声明 | normative 文本已统一 |
| **N3 实施** | spec.md doorbell 语义修订 | ✅ **已应用** spec.md:510 "doorbell 仅 enqueue；完成信号经 fence → MSI-X vector 0 通知 driver（intr_cb 异步路径）" | 消除与 H5 的 doorbell 同步返回矛盾 |
| **N4 实施** | spec.md BAR1 合成声明 + revision 字段诚实化 | ✅ **已应用** spec.md:526-528 BAR1 合成地址空间声明（minimal_v1 driver 不得对 BAR1 做 size-bound 检查）+ spec.md:530-532 revision 机制未接线诚实声明（D5 多级页表前 driver 不能依赖 revision 字段） | 消除 driver 兼容目标与现状矛盾 |
| **N5 实施** | 代码 bug，代码实施时修 | `dgpu_board_shell.cc:405-414` | `inject_q_` cap (`MAX_INJECT_Q_SIZE=4096`) 必须**豁免 doorbell/ring 写**——否则队列满时 doorbell 被丢 = 丢唤醒 |
| **F11/H6 实施** | tasks.md F11/H6 落地 | tasks.md 已 commit v1.7 但全文无 `[x]` checkbox 或 `implemented-in:` 列（Oracle 指出 F11 自身未落地） | **v1.8 已应用**：每项 B 加 `[x]` 或 `implemented-in: <commit>` 列；H6 5 模块数与 F12 Scenario 一致 |
| **H4 实施** | H4 任务文本扩展 | tasks.md P0.36 (H4) | "D2D 走 mem_out 两拍 或 -ENOSYS" 必须显式覆盖 **ring consume 路径的 dir 分派**（sdma_engine_tlm.cc:547-551），而非仅 desc_in 路径 |

## v1.8 P0 修正任务 (文档, <0.5d, **必须在 v1.7 P0.33-P0.39 实施前完成**)

### P0.41 (N2) — H1/H6 normative 文本应用 **[x] v1.8 (3b26225f+)**
- ✅ spec.md:43 改 "PcieTlpBundle wire-format, board-level 一致" (替代 AxiMemBundle)
- ✅ spec.md:62 SHALL NOT 列表移除 "使用 PcieTlpBundle 作为 SlavePort wire-format"
- ✅ spec.md:314 Scenario 改 "5 个模块 (pcie_ep + pcie_memory + sdma + gmmu + completion)"
- ✅ spec.md:526-528 增 "BAR1 合成地址空间声明"
- ✅ spec.md:530-532 增 "revision 机制未接线诚实声明"
- **TDD 5 步**:
  1. RED: spec.md 旧 "AxiMemBundle" / "6 模块" / "doorbell 同步返回" / "BAR1 doorbell relocated" 残留 → 内容矛盾
  2. GREEN: 应用 v1.8 normative 文本 → openspec validate --strict 仍 PASS（结构合法）
  3. 验证: `grep "AxiMemBundle\|6 模块\|doorbell 同步\|relocated into BAR0" spec.md` → 仅历史注释
  4. 验证: `grep "PcieTlpBundle.*board-level\|5 个模块\|doorbell 仅 enqueue\|合成地址空间" spec.md` → 存在
  5. commit: `docs(spec): v1.8 normative 文本应用 H1/H6/N3/N4 (Oracle 5th findings)`

### P0.42 (F11/H6) — tasks.md checkbox 落地 **[x] v1.8 (3b26225f+)**
- ✅ tasks.md 每项 B1-B28 / F1-F12 / H1-H7 加 `[x] implemented-in: <commit>` 列
- ✅ tasks.md "实施状态" 小节汇总
- **TDD 5 步**:
  1. RED: tasks.md 全文 `grep "\[x\]\|implemented-in:" | wc -l` == 0
  2. GREEN: 每项 B/F/H 加 `[x] implemented-in: <commit>` 后 >= 40
  3. 验证: `grep -c "\[x\]" tasks.md` → ≥40
  4. 验证: 每一行 P0.xx 含 `[x]`
  5. commit: `docs(tasks): 加 [x] implemented-in: 列 (F11 落地)`

### P0.43 (H4 实施期补) — ring consume 路径 dir 分派 **[ ] 待代码实施**
- `sdma_engine_tlm.cc:547-551`: `if H2D else { process_d2h }` 改为三向分派 `H2D | D2H | D2D`
- D2D 走 `process_d2d`（mem_out 两拍 或 -ENOSYS）
- **TDD 5 步**:
  1. RED: D2D descriptor 提交 → 当前代码按 D2H 处理 → 写 host_out + memcpy 方向错
  2. GREEN: 三向分派后 D2D 走 process_d2d + emit done status=-ENOSYS（minimal_v1）
  3. 验证: `grep "process_d2d\|ENOSYS" src/tlm/gpu/sdma_engine_tlm.cc` → 存在
  4. 验证: `[sdma][d2d]` 测试 PASS
  5. commit: `fix(sdma): ring consume 路径 dir 三向分派 (N1 实施)`

### P0.44 (N5 实施期补) — inject_q_ cap 豁免 doorbell/ring **[ ] 待代码实施**
- `dgpu_board_shell.cc::mmio_write`: doorbell/ring 写检查豁免（独立无上限通道或 cap 检查早退）
- **TDD 5 步**:
  1. RED: inject_q_ cap 触发时 doorbell 被丢 = 丢唤醒
  2. GREEN: doorbell/ring 写豁免 cap 检查后唤醒可达
  3. 验证: `grep "doorbell\|ring.*豁免\|exempt.*doorbell" src/tlm/gpu/dgpu_board_shell.cc` → 存在
  4. 验证: 高 doorbell 频率压测无丢失
  5. commit: `fix(board): inject_q_ cap 豁免 doorbell/ring (N5)`

### P0.45 — v1.8 验证 (normative 文本自洽)
```bash
# 1. normative 文本自洽性
grep "AxiMemBundle\|6 模块\|doorbell 同步\|relocated into BAR0" spec.md
# 期望: 零匹配（N2/N3/N4 已应用）
grep "PcieTlpBundle.*board-level\|5 个模块\|doorbell 仅 enqueue\|合成地址空间\|revision.*未接线" spec.md
# 期望: 全部匹配（N2/N3/N4 normative 已落地）
# 2. checkbox 落地
grep -c "\[x\]" tasks.md
# 期望: ≥40 (P0.41-P0.44 + P0.1-P0.39)
# 3. openspec validate
openspec validate cpptlm-driver-visible-minimal-soc --strict
# 期望: PASS（结构合法 + normative 文本自洽）
# 4. tasks.md 抬头统一
head -1 tasks.md design.md spec.md | grep "v1.8"
# 期望: 全部 v1.8
```

## Step 0: 表征 + 前置验证 (1 工作日) — 含 N1/N6/N12

### T0.1 RED — 锁定现有行为 (characterization)

**测试文件 1**: `test/test_pcie_memory_device_routing_regression.cc`
- PcieEndpointIP 不持有 memory_device_ 时 backdoor 走 fallback
- `set_memory_device(nullptr)` 后 `has_memory_device() == false`

**测试文件 2**: `test/test_pcie_memory_device_topology_lifecycle.cc` (N11 修订)
- 改为断言 "互不解引用" 而非 "销毁顺序保证":
  - `~PcieEndpointIP` 不解引用 `memory_device_`
  - `~PcieMemoryDevice` 不触碰 `PcieEndpointIP`
  - `DGpuBoard::shutdown()` 先 `set_memory_device(nullptr)`

### T0.2 RED — PcieTlpBundle StreamAdapter E2E (N1, **v1.8 H1 demoted: AxiMemBundle round-trip → D3 backlog**)

> **v1.8 H1 修订**: T0.2 原为 AxiMemBundle round-trip (`test_axi_mem_bundle_stream_adapter_roundtrip.cc`)。
> H1 统一 wire-format 为 PcieTlpBundle 后，AxiMemBundle round-trip 降级为 D3+ VramController seam 验证。
> 优先验证 PcieTlpBundle E2E（含 PcieMemoryDevice + GMMU + SDMA 全路径，见 T4.3）。
> 框架 N1 payload 修复（`Packet::payload_resize`、`PacketPool::acquire_with_min_size`）保留供 D3+ 启用。

**测试文件**: `test/test_pcie_memory_device_chstream_port.cc` (PcieTlpBundle 版本，替代旧 AxiMemBundle round-trip)
- 实例化 PcieMemoryDevice (2-port) + mock Master 适配器
- 经 `req_in_[0]` 发 PcieTlpBundle MEM_WRITE → `resp_out_[0]` 收 CPLD
- 经 `req_in_[1]` 发 PcieTlpBundle MEM_READ → resp 含 data

**前置**: 框架侧已应用 N1 payload 扩容 (2 行)。如未应用 PcieTlpBundle 测试仍可通行 (payload 小于 kMinPayloadBytes=256B)。

### T0.3 RED — PcieEndpointIP 3-BAR 支持 (N6) — **v1.3 B5 修正：64-bit 双 dword**

**测试文件**: `test/test_pcie_endpoint_ip_three_bar.cc`
- PcieEndpointIP 接受 `bar_sizes=[4096, 16777216, 8589934592]` (8GB = 0x2_0000_0000)
- **v1.3 B5 修正**：64-bit BAR 必须拆为两个 32-bit dword（PCIe spec: BAR 占两个连续寄存器 0x10/0x14, 0x18/0x1C, 0x20/0x24）：
  - `ep->config_space().read(0x10)` (BAR0 低 32-bit) = 4096
  - `ep->config_space().read(0x18)` (BAR1 低 32-bit) = 16777216
  - `ep->config_space().read(0x20)` (BAR2 低 32-bit) = `bar_sizes[2] & 0xFFFFFFFF` = 0
  - `ep->config_space().read(0x24)` (BAR2 高 32-bit) = `bar_sizes[2] >> 32` = 2
- **禁止**断言 `read(0x20) == 8589934592`（uint32_t 溢出，必失败）

**前置**: PcieConfigSpace 实现 64-bit 双 dword BAR 寄存器生成 (T0.4)

### T0.4 GREEN — PcieConfigSpace 64-bit 双 dword BAR 寄存器生成 (N6 立项)

**修改**: `src/tlm/pcie/pcie_config_space_mvp.cc` (或对应文件)
- `init()` 函数: 读 `bar_sizes` 参数, 写 **64-bit BAR 寄存器对**:
  - BAR0: regs_[0x10/4] = bar_sizes[0] & 0xFFFFFFFF; regs_[0x14/4] = bar_sizes[0] >> 32
  - BAR1: regs_[0x18/4] = bar_sizes[1] & 0xFFFFFFFF; regs_[0x1C/4] = bar_sizes[1] >> 32
  - BAR2: regs_[0x20/4] = bar_sizes[2] & 0xFFFFFFFF; regs_[0x24/4] = bar_sizes[2] >> 32
  - BAR3: regs_[0x28/4] = bar_sizes[3] & 0xFFFFFFFF; regs_[0x2C/4] = bar_sizes[3] >> 32
- 验证: `ep->config_space().read(0x20) == 0 && read(0x24) == 2` 表示 8GB BAR

**估时**: 0.5-1d (新增子任务; T0 整体工时扩展)

### T0.5 RED — minimal_v1 现有行为锁定

**测试文件**: `test/test_minimal_soc_baseline.cc`
- 加载 `configs/dgpu_soc_minimal_v1.json` (旧版, 5 模块)
- 现有 [pcie-memory] 24 case / 67 assertion 全绿

### T0.6 VALIDATE
```bash
./build/bin/cpptlm_tests "[axi_mem][stream]" --reporter compact
./build/bin/cpptlm_tests "[pcie-ep][bar]" --reporter compact
./build/bin/cpptlm_tests "[pcie-memory][routing][regression]" --reporter compact
./build/bin/cpptlm_tests "[pcie-memory]" --reporter compact
# 期望: 现有 + 新增前置测试全 PASS
```

## Step 1: PcieMemoryDevice ChStream 化 + EP raw ptr + 删双 tick + EP BAR 实现 (2 工作日)

### T1.1 RED — ChStream SlavePort 测试 (2-port) — **v1.3 B3 修正：单 adapter round-trip**

**测试文件**: `test/test_pcie_memory_device_chstream_port.cc`
- `PcieMemoryDevice dev; dev.req_in_size() == 2; dev.resp_out_size() == 2;`
- 通过 port0/port1 发/收 PcieTlpBundle (**v1.8 H1**: 替代 v1.3 的 AxiMemBundle)
- **v1.3 B3 撤销 N4 双 adapter 测试**：改为验证**单 adapter tick 后两端口都工作**：
  - 经 port0 发 PcieTlpBundle MEM_WRITE → 1 tick 后 port0 resp_out 收到 CPLD
  - 经 port1 发 PcieTlpBundle MEM_READ → 1 tick 后 port1 resp_out 收到 CPLD (data=PTE)
  - **不**再断言"双 tick"或"adapters_[2] 数组"——module_factory 对 multi-port 只注入单个 MultiPortStreamAdapter（其内部 tick 已遍历全端口）

### T1.2 GREEN — PcieMemoryDevice 改造 — **v1.3 B3 + B6 修正 + v1.8 H1 PcieTlpBundle**
- `include/bundles/axi_mem_bundles_tlm.hh`: 新建并标记 "D3+ VramController chip-internal 专用" (per v1.8 H1, minimal_v1 不实例化此 wire-format)
- `include/tlm/gpu/pcie_memory_device.hh`: 派生从独立类 → ChStreamModuleBase; **存单 `MultiPortStreamAdapter* adapter_`**（非 `adapters_[2]`）
- `src/tlm/gpu/pcie_memory_device.cc::tick()`: **单 tick 一次** (`if (adapter_) adapter_->tick();`)，由 `MultiPortStreamAdapter` 内部遍历全端口
- `handle_slave_port(p)`: ch_uint API 用 `.write()` (修正 v1.1 skeleton 错误)
- **v1.3 B6**: `include/chstream_register.hh` 补 `registerObject<PcieMemoryDevice>("PcieMemoryDevice")` + `registerMultiPortAdapter<PcieMemoryDevice, PcieTlpBundle, PcieTlpBundle, 2>` (PcieTlpBundle per H1)

### T1.3 RED — 既有 [pcie-memory] 测试机械迁移 (N5)

**测试文件**: `test/test_pcie_memory_device_basic.cc` + `backing.cc` + `routing_characterization.cc` + `routing_flag.cc`
- 既有 `PcieMemoryDevice dev;` 默认构造适配 ctor 签名变更
- 既有 `ep.has_memory_device() == true` 断言 → 改 `ep.set_memory_device(&dev); ep.has_memory_device() == true`
- 既有 mmap/raw 指针用法适配

### T1.4 GREEN — PcieMemoryDevice 既有测试迁移完成

**估时**: 0.5d

### T1.5 RED — PcieEndpointIP 不双 tick (N8)

**测试文件**: `test/test_pcie_endpoint_ip_tick_nesting.cc`
- soc 内 PcieMemoryDevice + PcieEndpointIP 都有
- 运行 N tick, 检查 PcieMemoryDevice::cycle_counter_ 单调递增且等于 N (不是 2N)
- 如 `PcieMemoryDevice::tick` 被调用 2N 次则 FAIL

### T1.6 GREEN — PcieEndpointIP 改造
- `include/tlm/pcie/pcie_endpoint_ip.hh`: `memory_device_` raw pointer
- `src/tlm/pcie/pcie_endpoint_ip.cc`:
  - ctor 删 `make_unique<PcieMemoryDevice>()` (line 28)
  - **T1.6 N8 关键**: 删 `tick()` 末尾 `memory_device_->tick()` 调用 (line 334-335)
  - ~PcieEndpointIP 不解引用 memory_device_

### T1.7 GREEN — bind_memory_backings 条件化 (N12)

**修改**: `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()`
```cpp
void DGpuBoard::bind_memory_backings() {
    // **N12 条件注入**: soc 含 pcie_memory 时走 AXI 路径, 否则保留 legacy
    if (auto* pm = dynamic_cast<PcieMemoryDevice*>(soc_->getInternalInstance("pcie_memory"))) {
        pcie_memory_ = pm;
        if (auto* ep = pcie_ep()) ep->set_memory_device(pm);
        // 不再注入 framebuffer 给 GMMU/SDMA (生产路径走 mem_out AXI)
    } else {
        // legacy dual-mode: 无 pcie_memory 时保留 framebuffer 注入
        // (兼容 dgpu_board_v1.json 等无 pcie_memory 配置)
        if (gmmu_) gmmu_->set_backing(framebuffer_ptr_, framebuffer_size_);
        if (sdma_) sdma_->set_vram_backdoor(framebuffer_ptr_, framebuffer_size_);
    }
    // MemoryTLM 仍保留 (backing store 与 AXI 路径独立)
    if (memory_) memory_->set_backing_store(framebuffer_ptr_, framebuffer_size_);
}
```

### T1.8 VALIDATE
```bash
./build/bin/cpptlm_tests "[pcie-memory][chstream]" --reporter compact
./build/bin/cpptlm_tests "[pcie-memory][topology]" --reporter compact
./build/bin/cpptlm_tests "[pcie-ep][tick]" --reporter compact
./build/bin/cpptlm_tests "[board][legacy]" --reporter compact  # N12 回归
```

## Step 2: GMMU 异步化 + iova 匹配 + 访问器 + 注册迁移 (1.5 工作日)

### T2.1 RED — GMMU async translate 测试 (含 N2 iova 匹配)

**测试文件**: `test/test_gmmu_async_state_machine.cc` + `test_gmmu_iova_match.cc`
- `gmmu.translate(0x1000, 4096, &phys)` → IDLE → 发 AXI → -EAGAIN
- `gmmu.translate(0x2000, 4096, &phys)` (不同 iova) → 仍 -EAGAIN, pending_iova_ 不变 (N2 关键)
- 多 tick 后 `gmmu.translate(0x1000, 4096, &phys)` 返 0 + 正确 phys

### T2.2 RED — dual-mode legacy 测试兼容

**测试文件**: `test/test_gmmu_legacy_sync.cc`
- `gmmu.set_backing(ptr, size)` 后 translate 走 translate_sync, 行为 v1.0 逐字节一致

### T2.3 GREEN — GMMU 改造
- 派生从 SimModule → ChStreamModuleBase
- 异步状态机 + **COMPLETE/WAIT iova 匹配** (N2)
- `req_out_`/`resp_in_` 访问器方法 (单端口模板要求)
- ch_uint API 用 `.write()` (修正 v1.1 skeleton)
- `translate_sync()` 保留 v1.0 实现

### T2.4 注册迁移 — **v1.3 B6 修正：双注册 (object + adapter) + v1.8 H1 PcieTlpBundle**
- `include/modules_cluster.hh`: 删 GmmuTLM 双 REGISTER_MODULE 行（GmmuTLM 从 SimModule 派生切到 ChStreamModuleBase 派生后必须移除，否则 static_assert 失败）
- `include/chstream_register.hh`: **双注册**（v1.3 B6 关键）：
  - `ModuleFactory::registerObject<GmmuTLM>("GmmuTLM")` — **必须**（否则 JSON `"type": "GmmuTLM"` 实例化失败）
  - `ChStreamAdapterFactory::get().registerAdapter<GmmuTLM, PcieTlpBundle, PcieTlpBundle>("GmmuTLM")` — adapter 注册 (PcieTlpBundle per v1.8 H1)

### T2.5 VALIDATE
```bash
./build/bin/cpptlm_tests "[gmmu][async]" --reporter compact
./build/bin/cpptlm_tests "[gmmu][iova]" --reporter compact  # N2
./build/bin/cpptlm_tests "[gmmu]" --reporter compact  # legacy dual-mode 兼容
```

## Step 3: SDMA 5 端口换型 + retry driver + slot-2 resp + 既有测试迁移 (2.5 工作日)

### T3.1 RED — SDMA mem_out AXI 路径 + slot-2 resp 测试 (N3/N7)

**测试文件**: `test/test_sdma_retry_driver.cc` + `test_sdma_slot2_response.cc`
- SDMA descriptor 注入 → process_inflight → translate -EAGAIN → inflight 重试 → emit mem_out → pcie_memory 收 → resp 从 slot-2 (req_in[PORT_MEM_OUT]) 消费 → done emit

### T3.2 GREEN — SDMA 改造 (含 N3 retry driver + N7 slot-2) — **v1.3 B1+B2 修正**

**v1.8 H1 修订 (替代 v1.3 B2 切型)**：所有 5 端口统一为 **PcieTlpBundle** (放弃混合端口切型)。
- `mem_in[PORT_MEM_IN]` + `mem_out[PORT_MEM_OUT]` → **PcieTlpBundle** (同 `desc_in`/`done_out`/`host_out`)
- `MultiPortStreamAdapter<SdmaEngineTLM, PcieTlpBundle, PcieTlpBundle, 5>` 同构保持
- chip-internal 路径通过 PcieTlpBundle MEM_WRITE/MEM_READ 通信 (inline data ≤8B; ≥8B 大块经 backdoor 或分段)
- `to_axi_mem_descriptor`/`from_axi_mem_completion` helper 降级为 **D3+ VramController 演进 seam**，minimal_v1 **不实例化**
- **理由** (H1): AxiMemBundle SHALL NOT 出现在 minimal_v1 的任何实际线路上 (设计简化, 框架限制, 节省 D3 适配成本)

**v1.3 B1 字段名修正**:
- 真实字段名（per `dma_descriptor_mvp.hh:44-48`）: `dir / host_iova / vram_offset / size / tag`
- **错误 → 正确**:
  - ~~`desc.dst_iova_offset`~~ → **`desc.host_iova`**（IOVA 经 GMMU translate 为 PA）
  - ~~`desc.len`~~ → **`desc.size`**（字节数）
  - **新增**:`desc.vram_offset`（SOC VRAM 内偏移，非 host 侧偏移）
- process_inflight_step PENDING_TRANSLATE 路径: `translate_cb_(e.desc.host_iova, e.desc.size, phys)` 而非 `translate_cb_(e.desc.dst_iova_offset, e.desc.len, phys)`

**SDMA 内部状态机**:
- `tick()` 先 `retry_inflight()` 再收 desc (N3 保 in-order)
- `process_inflight_step()` 状态机 (PENDING_TRANSLATE → READY_TO_EMIT → WAITING_AXI_RESP → DONE)
- **N7**: D2H/H2D 经 mem_out 发出后 resp 从 `req_in[PORT_MEM_OUT]` (slot 2) 消费（ChStream 端口对称性）
- `set_vram_backdoor` 保留 dual-mode 测试路径（既有 [sdma] 套件零逻辑改动）
- **N9 声明**: D2H/H2D 数据由 host_backdoor 注入（host 侧数据非 AXI 路径，spec 显式声明）

### T3.3 RED — 既有 [sdma] 测试机械迁移 (N5)

**测试文件** (≥6 个):
- `test/test_sdma_h2d.cc`
- `test/test_sdma_d2h.cc`
- `test/test_sdma_iommu_fault.cc`
- `test/test_sdma_backdoor_isolation.cc`
- `test/test_sdma_d2d_noc_path.cc`
- 其他 sdma 相关测试

**修改内容**:
- **v1.8 H1**: 放弃切型, 保留 PcieTlpBundle 不变。既有 [sdma] 测试已使用 PcieTlpBundle wire-format, **零机械迁移**。
- 验证: 既有 `test_sdma_h2d.cc` 等使用 PcieTlpBundle 的测试在 PcieMemoryDevice ChStream 化后仍正常运行。
- 场名适配同 design §6 方案 (PcieTlpBundle field: offset/size/trans_id/GPLD vs 旧 AxiMemBundle addr/len/id/data_buf).

### T3.4 GREEN — 既有 [sdma] 测试迁移完成

**估时**: 0.5-1d

### T3.5 COVERAGE
- process_h2d 完整路径 (AXI + backdoor dual-mode)
- process_d2h 完整路径
- d2d_forward (mem_out 两拍)
- **N3 retry driver 多 tick 完成 in-order**
- **N7 slot-2 resp 路径**
- dual-mode backdoor 兼容 (既有 [sdma] 套件零逻辑改动)

### T3.6 VALIDATE
```bash
./build/bin/cpptlm_tests "[sdma][mem_out]" --reporter compact
./build/bin/cpptlm_tests "[sdma][retry]" --reporter compact  # N3
./build/bin/cpptlm_tests "[sdma][slot2]" --reporter compact  # N7
./build/bin/cpptlm_tests "[sdma]" --reporter compact  # dual-mode 兼容
```

## Step 4: minimal_v1 JSON + BAR2 fast-path + E2E + 文档 (1.5 工作日)

### T4.1 GREEN — minimal_v1 JSON 扩展
- `configs/dgpu_soc_minimal_v1.json`:
  - 新增 `memory_routing_enabled: true` (顶层)
  - 新增 `pcie_memory` 模块
  - 扩展 `bar_sizes: [4096, 16777216, 8589934592]`
  - 新增 2 条 connections

### T4.2 GREEN — DGpuBoard BAR2 fast-path
- `src/tlm/gpu/dgpu_board_shell.cc::mmio_read/write`:
  ```cpp
  if (memory_routing_enabled_ && bar == 2 && soc_) {
      if (auto* ep = pcie_ep(); ep && ep->has_memory_device()) {
          return ep->memory_device().memory_read(offset, buf, len);  // write 对称
      }
  }
  ```
- **N10**: PcieMemoryDevice 构造时预分配 memory_backing_ (8GB), 避免 lazy resize race
- shutdown 先 set_memory_device(nullptr) (N11)

### T4.3 E2E 测试

**测试文件**: `test/test_minimal_soc_driver_visible_e2e.cc`
- 加载 `configs/dgpu_soc_minimal_v1.json`
- driver 视角端到端:
  1. `pcie_config_read(0x00, 4)` 返 0x123410DE (N6 + R4 验证)
  2. `mmio_write(2, 0x1000, data, 8)` + `mmio_read(2, 0x1000, buf, 8)` 返 data
  3. `backdoor_write(0, 0x1000, data, 8)` + `backdoor_read(0, 0x1000, buf, 8)` 返 data
  4. 写 PTE: `mmio_write(2, pte_off, pte, 8)` → GMMU translate 经 PcieTlpBundle MEM_READ 读同一 PTE (per v1.8 H1)
  5. SDMA H2D → mem_out (slot 2 resp) → pcie_memory 收到 → done emit

### T4.4 ABI 冻结验证
```bash
git diff HEAD -- include/abi/cpptlm_emulator.h
# 期望: 空输出
```

### T4.5 回归基线
```bash
./build/bin/cpptlm_tests --reporter compact | tail -3
# 期望: 现有迁移 (24 case pcie-memory + ≥6 sdma + gmmu) + 新增 (≥12 case) 全绿

./scripts/test/docs_sync_check.sh --strict
# 期望: PASS

openspec validate --changes --strict
# 期望: 5/5 PASS
```

## Step 5: 文档 + AGENTS.md + 提交

### T5.1 CppTLM 仓内 docs
- 新文件: `docs/pcie/driver-visible-minimal-soc.md`
- 内容: 拓扑 + driver 视角路径 + **v1.8 H1**: PcieTlpBundle 统一 wire-format (AxiMemBundle 标 D3+ VramController 专用) + Coherence 域边界声明 + **N1-N12 修订要点索引**

### T5.2 AGENTS.md 更新
- "WHERE TO LOOK" 表: 添加 `[driver-visible]` 标签
- "PHASE STATE": 添加 D-AXI phase 行 (v1.2 P1 修订完成 / T0-T4 实施计划)
- "KEY INVARIANTS": 添加 v1.8 H1 wire-format 统一 (PcieTlpBundle / AxiMemBundle D3+ 专用) + Coherence 边界声明
- bundle 章节: "board-level (PcieTlpBundle) vs chip-internal (AxiMemBundle, D3+ seam)" 边界说明
- 测试章节: 标注 N5 既有测试机械迁移清单 (H1 后 SDMA 测试零迁移)

### T5.3 提交策略 (10 commits, v1.8 H1 修订)
```bash
git commit -m "test(stream): PcieTlpBundle StreamAdapter E2E (替代旧 AxiMemBundle round-trip, H1)"
git commit -m "feat(framework): payload 按 sizeof(BundleT) 扩容 (N1 框架 2 行, D3+ seam)"
git commit -m "feat(bundle): 新建 AxiMemBundle 并标注 D3+ VramController chip-internal 专用 (H1)"
git commit -m "feat(pcie-memory): PcieMemoryDevice ChStreamModuleBase 化 + 2 SlavePorts + **单 adapter_** (v1.3 B3 撤销 N4 双 adapter)"
git commit -m "refactor(pcie): PcieEndpointIP raw pointer 化 + 删 memory_device_->tick() (N8)"
git commit -m "feat(pcie-cfg): PcieConfigSpace BAR 寄存器从 bar_sizes 生成 (N6)"
git commit -m "feat(gmmu): GMMU 异步状态机 + iova 匹配 (N2) + 访问器 + PcieTlpBundle MasterPort (H1)"
git commit -m "refactor(sdma): SDMA 5 端口 retry driver (N3) + slot-2 resp (N7), PcieTlpBundle 统一 (H1)"
git commit -m "feat(board): BAR2 fast-path + bind_memory_backings 条件化 (N12)"
git commit -m "test(pcie): 既有 [sdma]/[pcie-memory] 测试迁移 (N5) + driver-visible E2E"
git commit -m "docs(pcie): driver-visible-minimal-soc.md + AGENTS.md 同步 (含 H1 H1-N12 索引)"
```

## 工时统计 (v1.2 修订)

| Task | 估时 | 累计 |
|------|------|------|
| **P1** 修订文档 | 0.5d | 0.5d |
| **T0** 表征 + PcieTlpBundle StreamAdapter E2E (**v1.8 H1**: AxiMemBundle D3 backlog) + EP 3-BAR + baseline | 1d | 1.5d |
| **T0.4** PcieConfigSpace BAR 寄存器生成 (N6 子任务) | +0.5d (含 T0 内) | — |
| **T1** PcieMemoryDevice 2-port + **单 adapter_** (v1.3 B3 撤 N4) + EP raw ptr + 删双 tick + EP BAR (v1.3 B5 64-bit 双 dword) + 既有测试迁移 | 2d | 3.5d |
| **T2** GMMU 异步 + iova 匹配 + 访问器 + 注册迁移 | 1.5d | 5d |
| **T3** SDMA 5 端口统一 PcieTlpBundle (v1.8 H1) + retry driver + slot-2 resp + 既有测试验证 | 2.5d | 7.5d |
| **T4** JSON + BAR2 fast-path + bind_memory_backings 条件化 + E2E | 1.5d | 9d |
| **合计** | **~9d ≈ 1.8 周** | |

## 验证清单 (v1.2 P1 修订后)

- [ ] P1 修订完成, openspec validate --changes --strict PASS
- [ ] **N1** AxiMemBundle round-trip (**v1.8 H1 demoted 至 D3 backlog**; 用 PcieTlpBundle E2E 替代验证, 见 T4.3)
- [ ] **N2** GMMU iova 匹配检查通过
- [ ] **N3** SDMA inflight retry driver 多 tick in-order 完成
- [ ] **v1.3 B3 撤销 N4** PcieMemoryDevice adapters_[2] 双 tick 测试（不可能, 基于错误前提）→ **新增 v1.3 B3 验证**: 单 adapter tick 后两个 resp_out 都出
- [ ] **N5** 既有 [sdma]/[pcie-memory] 测试机械迁移后全绿
- [ ] **N6** EP 3-BAR config space 测试通过
- [ ] **N7** SDMA slot-2 resp 测试通过
- [ ] **N8** EP 不双 tick 测试通过
- [ ] **N10** 预分配测试通过
- [ ] **N11** "互不解引用" 测试通过 (替代原"销毁顺序")
- [ ] **N12** legacy dual-mode 回归测试通过 (dgpu_board_v1.json 等)
- [ ] T4 E2E driver-visible 端到端通过
- [ ] T4 ABI 冻结 (git diff HEAD include/abi/cpptlm_emulator.h 空)
- [ ] T4 回归基线 (现有迁移后 + 新增 ≥12 case 全绿)
- [ ] T5 docs + AGENTS.md 同步完成 (含 N1-N12 索引)
- [ ] T5 11 个 commit 提交

## 不在范围 (deferred)

| 项 | 后置阶段 |
|----|---------|
| 多 PcieMemoryDevice 实例 | 不实现 |
| GMMU 多级页表 (v1.1) | 不实现 |
| GPU 计算设备 (StreamingMultiprocessor) | D3 |
| Coherent/Non-coherent 在 CppTLM 建模 | **明确不建模** (UE 端职责) |
| 混合端口模板 (desc_in PcieTlpBundle) | 后续单独立项 |
| `ch_uint<512>`=64-bit 限制统一化 | 后续单独立项 |
| ArchForge 跨仓引用 | 零依赖 |

## D-AXI v1.2 vs D2 v1.1 主要差异

（与 v1.1 P0 修订版本相同，外加 must-fix 标注）

| 项目 | D2 v1.1 | D-AXI (v1.2) |
|------|---------|-----------------|
| PcieMemoryDevice 派生 | 独立类 | ChStreamModuleBase, 2 SlavePorts, **单 adapter_ (v1.3 B3 撤销 N4 adapters_[2])** |
| Wire-format | N/A | **AxiMemBundle** (新建, **需框架 payload 扩容**) — **v1.8 H1 locked to PcieTlpBundle**; AxiMemBundle D3+ 专用 |
| GMMU 派生 | SimModule | ChStreamModuleBase, **COMPLETE/WAIT iova 匹配** |
| SDMA wire-format | PcieTlpBundle | AxiMemBundle, **retry driver**, **slot-2 resp** — **v1.8 H1 统一为 PcieTlpBundle** |
| EP BAR | 2-BAR | **3-BAR (T0.4 显式立项)** |
| EP 双 tick | 双 tick | **删除转发** |
| bind_memory_backings | 全局注入 | **条件注入 (N12)** |
| 工时 | N/A | **9d** |