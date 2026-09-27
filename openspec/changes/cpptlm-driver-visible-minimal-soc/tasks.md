# Tasks: Driver-Visible Minimal SoC (v1.4 — v1.3 P0 (B1-B6) + v1.4 架构根因 (B7-B13))

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

### T0.2 RED — AxiMemBundle 经真实 StreamAdapter round-trip (N1)

**测试文件**: `test/test_axi_mem_bundle_stream_adapter_roundtrip.cc`
- **不**做裸 serialize round-trip
- 实例化 PcieMemoryDevice (2-port) + mock Master 适配器
- 经 `req_in_[0]` 发 AxiMemBundle MEM_WRITE → `resp_out_[0]` 收 MEM_WRITE_RESP
- 经 `req_in_[1]` 发 AxiMemBundle MEM_READ → resp 含 data_buf

**前置**: 框架侧已应用 N1 payload 扩容 (2 行)。如未应用此测试**必失败**。

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
- 通过 port0/port1 发/收 AxiMemBundle
- **v1.3 B3 撤销 N4 双 adapter 测试**：改为验证**单 adapter tick 后两端口都工作**：
  - 经 port0 发 AxiMemBundle MEM_WRITE → 1 tick 后 port0 resp_out 收到 MEM_WRITE_RESP
  - 经 port1 发 AxiMemBundle MEM_READ → 1 tick 后 port1 resp_out 收到 MEM_READ_RESP
  - **不**再断言"双 tick"或"adapters_[2] 数组"——module_factory 对 multi-port 只注入单个 MultiPortStreamAdapter（其内部 tick 已遍历全端口）

### T1.2 GREEN — PcieMemoryDevice 改造 — **v1.3 B3 + B6 修正**
- `include/bundles/axi_mem_bundles_tlm.hh`: 新建 (per design §3)
- `include/tlm/gpu/pcie_memory_device.hh`: 派生从独立类 → ChStreamModuleBase; **存单 `MultiPortStreamAdapter* adapter_`**（非 `adapters_[2]`）
- `src/tlm/gpu/pcie_memory_device.cc::tick()`: **单 tick 一次** (`if (adapter_) adapter_->tick();`)，由 `MultiPortStreamAdapter` 内部遍历全端口
- `handle_slave_port(p)`: ch_uint API 用 `.write()` (修正 v1.1 skeleton 错误)
- **v1.3 B6**: `include/chstream_register.hh` 补 `registerObject<PcieMemoryDevice>("PcieMemoryDevice")`（与 `registerMultiPortAdapter` 配对）

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

### T2.4 注册迁移 — **v1.3 B6 修正：双注册 (object + adapter)**
- `include/modules_cluster.hh`: 删 GmmuTLM 双 REGISTER_MODULE 行（GmmuTLM 从 SimModule 派生切到 ChStreamModuleBase 派生后必须移除，否则 static_assert 失败）
- `include/chstream_register.hh`: **双注册**（v1.3 B6 关键）：
  - `ModuleFactory::registerObject<GmmuTLM>("GmmuTLM")` — **必须**（否则 JSON `"type": "GmmuTLM"` 实例化失败）
  - `ChStreamAdapterFactory::get().registerAdapter<GmmuTLM, AxiMemBundle, AxiMemBundle>("GmmuTLM")` — adapter 注册

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

**v1.3 B2 切型范围限定（minimal_v1）**：
- **chip-internal 端口（必须切）**: `mem_in[PORT_MEM_IN]` + `mem_out[PORT_MEM_OUT]` → **AxiMemBundle**
- **board-level 端口（minimal_v1 不切）**: `desc_in[PORT_DESC_IN]` + `done_out[PORT_DONE_OUT]` + `host_out[PORT_HOST_OUT]` → **保持 PcieTlpBundle**（因 minimal_v1 生产路径经 BAR1 ring doorbell，host_out/desc_in/done_out 端口**不实际接线**；仅 chip-internal mem_in/mem_out 经 soc connections 连通 pcie_memory）
- **理由**: AxiMemBundle SHALL NOT 出现在 host↔board 端口（spec.md:60-62）；混合端口模板在 minimal_v1 范围外
- 4 个 helper 重定向 AxiMemBundle（kind 用 DMA_DESC=8/DMA_DONE=9，仅 `to_axi_mem_descriptor`/`from_axi_mem_completion`；`desc_in`/`done_out`/`host_out` helper 保持 `to_pcie_tlp_*`）

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
- `PcieTlpBundle desc_pkt = ...` → `AxiMemBundle desc_pkt = ...`
- `sdma.req_in[PORT_DESC_IN].data() = to_pcie_tlp_descriptor(...)` → `to_axi_mem_descriptor(...)` (kind=DMA_DESC)
- `from_pcie_tlp_completion(...)` → `from_axi_mem_completion(...)` (kind=DMA_DONE)
- 字段名适配

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
  4. 写 PTE: `mmio_write(2, pte_off, pte, 8)` → GMMU translate 经 AxiMemBundle MEM_READ 读同一 PTE
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
- 内容: 拓扑 + driver 视角路径 + AxiMemBundle vs PcieTlpBundle 边界 + Coherence 域边界声明 + **N1-N12 修订要点索引**

### T5.2 AGENTS.md 更新
- "WHERE TO LOOK" 表: 添加 `[driver-visible]` `[axi_mem]` 标签
- "PHASE STATE": 添加 D-AXI phase 行 (v1.2 P1 修订完成 / T0-T4 实施计划)
- "KEY INVARIANTS": 添加 AxiMemBundle vs PcieTlpBundle 边界 + Coherence 边界声明
- bundle 章节: "board-level (PcieTlpBundle) vs chip-internal (AxiMemBundle)" 边界说明
- 测试章节: 标注 N5 既有测试机械迁移清单

### T5.3 提交策略 (10 commits)
```bash
git commit -m "test(stream): AxiMemBundle 经 StreamAdapter round-trip (N1 前置验证)"
git commit -m "feat(framework): payload 按 sizeof(BundleT) 扩容 (N1 框架 2 行)"
git commit -m "feat(bundle): 新建 AxiMemBundle (chip-internal AXI 4KB inline payload)"
git commit -m "feat(pcie-memory): PcieMemoryDevice ChStreamModuleBase 化 + 2 SlavePorts + **单 adapter_** (v1.3 B3 撤销 N4 双 adapter)"
git commit -m "refactor(pcie): PcieEndpointIP raw pointer 化 + 删 memory_device_->tick() (N8)"
git commit -m "feat(pcie-cfg): PcieConfigSpace BAR 寄存器从 bar_sizes 生成 (N6)"
git commit -m "feat(gmmu): GMMU 异步状态机 + iova 匹配 (N2) + 访问器 + AxiMemBundle MasterPort"
git commit -m "refactor(sdma): SDMA 5 端口 AxiMemBundle + retry driver (N3) + slot-2 resp (N7)"
git commit -m "feat(board): BAR2 fast-path + bind_memory_backings 条件化 (N12)"
git commit -m "test(pcie): 既有 [sdma]/[pcie-memory] 测试机械迁移 (N5)"
git commit -m "test(minimal-soc): driver-visible E2E + N1-N12 must-fix 测试套件"
git commit -m "docs(pcie): driver-visible-minimal-soc.md + AGENTS.md 同步 (含 N1-N12 must-fix 索引)"
```

## 工时统计 (v1.2 修订)

| Task | 估时 | 累计 |
|------|------|------|
| **P1** 修订文档 | 0.5d | 0.5d |
| **T0** 表征 + AxiMemBundle StreamAdapter + EP 3-BAR + baseline | 1d | 1.5d |
| **T0.4** PcieConfigSpace BAR 寄存器生成 (N6 子任务) | +0.5d (含 T0 内) | — |
| **T1** PcieMemoryDevice 2-port + **单 adapter_** (v1.3 B3 撤 N4) + EP raw ptr + 删双 tick + EP BAR (v1.3 B5 64-bit 双 dword) + 既有测试迁移 | 2d | 3.5d |
| **T2** GMMU 异步 + iova 匹配 + 访问器 + 注册迁移 | 1.5d | 5d |
| **T3** SDMA 5 端口换型 + retry driver + slot-2 resp + 既有测试迁移 | 2.5d | 7.5d |
| **T4** JSON + BAR2 fast-path + bind_memory_backings 条件化 + E2E | 1.5d | 9d |
| **合计** | **~9d ≈ 1.8 周** | |

## 验证清单 (v1.2 P1 修订后)

- [ ] P1 修订完成, openspec validate --changes --strict PASS
- [ ] **N1** AxiMemBundle 经真实 StreamAdapter round-trip 通过
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
| Wire-format | N/A | **AxiMemBundle** (新建, **需框架 payload 扩容**) |
| GMMU 派生 | SimModule | ChStreamModuleBase, **COMPLETE/WAIT iova 匹配** |
| SDMA wire-format | PcieTlpBundle | AxiMemBundle, **retry driver**, **slot-2 resp** |
| EP BAR | 2-BAR | **3-BAR (T0.4 显式立项)** |
| EP 双 tick | 双 tick | **删除转发** |
| bind_memory_backings | 全局注入 | **条件注入 (N12)** |
| 工时 | N/A | **9d** |