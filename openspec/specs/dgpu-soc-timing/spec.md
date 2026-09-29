# dgpu-soc-timing Specification

## Purpose
TBD - created by archiving change cpptlm-dgpu-soc-timing-mvp. Update Purpose after archive.
## Requirements
### Requirement: 仿真模式切换 (per §1.4 仿真模式声明)

DGpuBoard SHALL 通过 JSON 顶层 `simulation_mode` 字段在 functional-mode 与 timing-mode 间切换,默认 functional-mode,**不**破坏既有 functional-mode 行为。

#### Scenario: functional-mode 默认 (向后兼容)

- **GIVEN** `dgpu_soc_minimal_v1.json` (无 `simulation_mode` 字段)
- **WHEN** `DGpuBoard::load_soc_config()` + `init()` 调用
- **THEN** `simulation_mode_ = SimulationMode::Functional`
- **AND** 既有 functional-mode 路径触发,**[minimal_dgpu_soc]** 41 assertions 全绿
- **AND** 既有 `[pcie-memory]` 24 cases 全绿
- **AND** 既有 `[abi][minimal_dgpu_soc]` 28 assertions 全绿

#### Scenario: timing-mode 显式启用

- **GIVEN** `dgpu_soc_timing_v1.json` 含 `simulation_mode: "timing"`
- **WHEN** `DGpuBoard::load_soc_config()` + `init()` 调用
- **THEN** `simulation_mode_ = SimulationMode::Timing`
- **AND** `DGpuBoard::init_timing_mode()` 分支触发
- **AND** **7 个模块**实例化 (VramControllerTLM **或** MemoryTLM 二选一, per H2 互斥): `PcieEndpointIP` + `SdmaEngineTLM` + `GmmuTLM` + `CrossbarTLM` + (VramControllerTLM **或** MemoryTLM) + `CompletionRingTLM` + `DGpuSoc`
- **AND** 5 个时序参数 setter 被调用 (MemoryTLM + GmmuTLM + SDMA + VramCtrl + Crossbar)
- **AND** `MemoryTLM::set_use_zero_delay_for_test(false)` 显式调用,启用 defer_response 路径 (per M5)
- **AND** `DGpuBoard::cycle_advance_modules_` 注册 `[MemoryTLM, VramControllerTLM]` (**剔除** GmmuTLM 同步 per M14 + **剔除** SdmaEngineTLM 无 advance_cycle 方法 per R10 统一决策 + 六轮 P0-2 再确认)

---

### Requirement: cycle advance 统一推进 (per TInv-3)

DGpuBoard SHALL 在 `tick()` 内统一 advance cycle counter 并广播到所有 timing-mode 模块;**任何**模块不可自行 ++cycle。

#### Scenario: cycle advance 集中

- **GIVEN** `simulation_mode_ = Timing`, **2 个** timing 模块已注册 (`cycle_advance_modules_`) — `MemoryTLM` + `VramControllerTLM` (**剔除** SdmaEngineTLM per R10 统一决策 + GmmuTLM 同步 per M14)
- **WHEN** `DGpuBoard::tick()` 调用
- **THEN** `DGpuBoard::current_cycle_` 自增 1
- **AND** 每个 `cycle_advance_modules_` 中的模块 `advance_cycle()` 被调用
- **AND** 每个模块读 `current_cycle()` 必返回 `DGpuBoard::current_cycle_` 值
- **AND** **无**模块自行 ++current_cycle_

#### Scenario: cycle advance 与 EventQueue 对齐

- **GIVEN** EventQueue 调度 `tick()` N 次
- **WHEN** 每次 `tick()` 内部 advance cycle
- **THEN** `current_cycle_` 严格 = N
- **AND** 所有模块 `current_cycle()` 严格 = N
- **AND** 跨模块 cycle 漂移为 0

#### Scenario: advance_cycle() 基类可达 (per H4 + P2-R10)

- **GIVEN** `cycle_advance_modules_` 类型 `std::vector<ChStreamModuleBase*>`
- **WHEN** `DGpuBoard::tick()` 内部 `mod->advance_cycle()` 调用
- **THEN** `ChStreamModuleBase::advance_cycle()` 虚函数存在 (T0 实施时添加 默认空实现)
- **AND** `MemoryTLM::advance_cycle()` 覆盖基类 (`++current_cycle_`)
- **AND** `VramControllerTLM` 继承 `MemoryTLM`,自动继承 `advance_cycle()`
- **AND** SdmaEngineTLM 沿用既有 (无 advance_cycle 注册)

---

### Requirement: timing-mode 显式门控 use_zero_delay_for_test (per M5 Oracle 反馈)

`MemoryTLM::use_zero_delay_for_test_` SHALL **默认** `true` (functional-mode 兼容),timing-mode 必须在 `init_timing_mode()` 内显式 `set_use_zero_delay_for_test(false)` 启用 defer_response 路径。**禁止**其他路径修改默认值。

#### Scenario: functional-mode 默认值保 zero-delay

- **GIVEN** `simulation_mode_ = Functional` (默认)
- **WHEN** `DGpuBoard::init()` 调用
- **THEN** `MemoryTLM::use_zero_delay_for_test_` 保持 `true` (per design §3.3 字段默认值)
- **AND** `[memory_tlm]` 既有单测零回归 (立即 resp, 0 cyc latency)
- **AND** functional-mode 路径 (`memory_tlm.hh:114-140`) 行为零修改

#### Scenario: timing-mode 显式启用 deferred 路径

- **GIVEN** `simulation_mode_ = Timing`
- **WHEN** `DGpuBoard::init_timing_mode()` 调用
- **THEN** `MemoryTLM::set_use_zero_delay_for_test(false)` 显式调用
- **AND** defer_response 路径启用: req 接收 + pending_resps_ 入队 + 按 ready_cycle 排序发出
- **AND** stats_latency_read_/write_ 启用 sample

#### Scenario: 测试不得通过其他路径修改默认值

- **GIVEN** 任何测试代码
- **WHEN** 尝试 `MemoryTLM::use_zero_delay_for_test_ = false` (直接访问 private 字段)
- **THEN** 编译失败 (per C++ access control) — **不可**绕过 setter
- **AND** 测试**必须**通过 `set_use_zero_delay_for_test(false)` setter 调用
- **AND** setter 内附 `DCHECK(simulation_mode_ == Timing)` 断言 (per design §6.2)

---

### Requirement: MemoryTLM 数据粒度限制 (per M12 Oracle 反馈)

`MemoryTLM::tick()` 内 `std::memcpy(backing_view_ + addr, &val, std::min<size_t>(sz, 8))` SHALL 限制单 req payload 最多 8 字节 (`CacheReqBundle.data` 是 `ch_uint<64>`)。SDMA **不**拆分 (沿用 functional-mode memcpy, per H3): SDMA 内部 `translate_cb_(iova, size, &phys)` + memcpy(vram_backdoor_, host_backdoor_, size) 一次完成;**不**做 SDMA 端分片;**不**走 Crossbar → VramCtrl cascade (per H2). 上层 driver (host) 保证 size ≤ 255B (CacheReqBundle size 上限)。**v0.1 timing-mode E2E 验证范围**为 "cycle accounting + 8B chunk 数据正确性",**不**验证 4KB 单 req 整块搬运。

#### Scenario: 8B payload 上限

- **GIVEN** CacheReqBundle.data 字段类型 `ch_uint<64>` (8 字节)
- **WHEN** MemoryTLM::tick() 接 req, `req.size.read()` = N
- **THEN** memcpy 大小 = `min(N, 8)` 字节 (per `memory_tlm.hh:125,133` 既有逻辑保留)
- **AND** **不**尝试 memcpy > 8 字节 (避免 ch_uint 溢出)

#### Scenario: SDMA 4KB descriptor 完整搬运 (v0.2 简化, 沿用 functional-mode memcpy 路径)

- **GIVEN** timing-mode + descriptor (H2D, iova=0, vram=0, size=4096), simulation_mode_ = Timing
- **WHEN** `submit_descriptor(desc)` 调用 (沿用 functional-mode API, **不**拆分)
- **THEN** SDMA 内部 `translate_cb_(0, 4096, &phys)` 调用 GmmuTLM::translate_timing → lat 经 SDMA::last_tlb_latency_ 成员回传 (per 七轮 P0-13 终稿, board 回调注入 `last_descriptor_complete_cycle_`, **不**SDMA 自加 current_cycle_)
- **AND** SDMA memcpy(vram_backdoor_+0, host_backdoor_+phys, 4096) **一次完成** (functional-mode 0 cycle)
- **AND** **不**拆分 512×8B sub-req (v0.2 删除 SDMA 端分片, per H3)
- **AND** **不**走 Crossbar → VramCtrl → Memory cascade 路径 (per H2 删除 multi-hop 拓扑)
- **AND** CacheReqBundle **不**承载 4KB 单 req (size 字段是 `ch_uint<8>` 上限 255);**不**通过 SDMA 端分片解决,而是通过**上层 driver** (host 端) 保证 size ≤ 255

#### Scenario: E2E 验证范围限定

- **GIVEN** timing-mode E2E H2D 测试, 4KB descriptor
- **WHEN** 数据正确性验证
- **THEN** 验证每 8B chunk (chunk_count=512) 数据正确, **或** 等 fence 完成 + backdoor_read 全块校验
- **AND** **不**断言 "单 req 搬运 4KB" (会失败,因 CacheReqBundle payload 上限 8B)
- **AND** cycle accounting 验证:`requests_read_total = 1 × size` (单次 memcpy,per functional-mode 路径);`total_cycles = pcie_tlp_latency + doorbell_parse + tlb_miss_cycles (4-54)` (per v0.2 §6.2 时序累加图, 无 Crossbar/VramCtrl cascade)

---

### Requirement: MemoryTLM defer_response 路径 (per design §2)

MemoryTLM SHALL 在 timing-mode 下延迟发出 resp(按 `ready_cycle` 排序),**不**阻塞 req 接收;**functional-mode** 路径(既有 v2.2 行为)**不**改变。

#### Scenario: timing-mode 读延迟

- **GIVEN** `read_latency_hit_cycles_=100`, backing 已注入
- **WHEN** cycle N: MemoryTLM 接 req read addr 0x1000 (row hit)
- **THEN** cycle N: pending_resps_ push {resp, ready_cycle=N+100}
- **AND** cycle N: req_in_.consume() (新 req 可接收)
- **AND** cycle N+99: pending_resps_.top().ready_cycle = N+100 > N, **不**发 resp (priority_queue 升序, per M1)
- **AND** cycle N+100: resp_out_.write(resp)
- **AND** stats_latency_read_.sample(100) 被调用

#### Scenario: timing-mode 写延迟

- **GIVEN** `write_latency_cycles_=120`, backing 已注入
- **WHEN** cycle N: MemoryTLM 接 req write addr 0x2000
- **THEN** backing 写入立即发生(per TInv-2 强 ordering)
- **AND** pending_resps_ push {resp, ready_cycle=N+120}
- **AND** stats_latency_write_.sample(120) 被调用

#### Scenario: out-of-range 错误立即响应

- **GIVEN** backing_view_size_=1024
- **WHEN** cycle N: MemoryTLM 接 req read addr 0x10000 (越界)
- **THEN** resp.error_code = 1 (OUT_OF_RANGE), latency = 1
- **AND** cycle N+1: resp 发出(per design §2.3 错误立即)

#### Scenario: pending_resps_ 顺序保证 (priority queue)

- **GIVEN** cycle N: req A 到达 (lat=100, ready_cycle=N+100), cycle N+5: req B 到达 (lat=50, ready_cycle=N+55)
- **WHEN** MemoryTLM::tick() 在 cycle N+5..N+150 多次调用, **priority queue** 按 ready_cycle 排序
- **THEN** cycle N+55: req B resp 发出 (ready=N+55, 优先于 A)
- **THEN** cycle N+100: req A resp 发出 (ready=N+100)
- **AND** 顺序严格按 ready_cycle 升序 (per TInv-2)
- **AND** **不**使用 FIFO (避免 head-of-line blocking)

#### Scenario: pending_resps_ 错误响应立即 (latency=1)

- **GIVEN** backing 已注入, `read_latency_miss_cycles_=200`, addr=0x10000 (越界)
- **WHEN** cycle N: MemoryTLM 接 req read (越界)
- **THEN** error_code=1 (OUT_OF_RANGE), latency=1 (per design §3.3 错误立即)
- **AND** cycle N+1: resp 发出
- **AND** 优先于其他 in-flight 正常 resp 发出 (latency=1 < 任何其他 ready_cycle)

---

### Requirement: GmmuTLM TLB hit/miss + page walk (per design §3)

GmmuTLM SHALL 在 timing-mode 下实现 32-entry TLB,TLB miss 累加 `tlb_miss_latency_cycles_` 并填充 TLB。**functional-mode** 既有 `translate()` 行为**不**改变。

#### Scenario: TLB hit (顺序访问)

- **GIVEN** `tlb_miss_latency_cycles_=50`, 32-entry TLB 已初始化
- **WHEN** cycle N: `translate_timing(iova=0x1000, 4096, &paddr, &lat)` 第 1 次 (TLB miss, fill)
- **THEN** lat = 50, paddr = 0x20000 (示例)
- **AND** TLB[idx=0].iova_page=0x1, paddr=0x20000, valid=true
- **AND** translate_stats_.misses++ (TLB miss)
- **WHEN** cycle N+5: 同一 iova 再调 `translate_timing(...)`
- **THEN** lat = 0 (TLB hit), paddr 不变
- **AND** translate_stats_.hits++ (TLB hit)

#### Scenario: 128KB 工作集重复访问 → TLB hit rate 100%

- **GIVEN** 32-entry 直接映射 TLB, 工作集 = 32 pages = 128KB (恰好填满 TLB, 0 LRU 冲突)
- **WHEN** 提交 1024 个 descriptor (每个 4KB, iova 在 [0x1000, 0x21000) 区间循环, 32 pages 重复访问)
- **THEN** 32 次 initial TLB miss (首次 fill), 992 次 TLB hit (后续重复访问)
- **AND** translate_stats_.misses = 32, hits = 992
- **AND** hit rate = 992 / 1024 ≈ 96.875%

#### Scenario: 4MB 顺序流访问 → TLB 全部 miss (thrashing)

- **GIVEN** 32-entry 直接映射 TLB (无 LRU), 4MB 数据 = 1024 pages
- **WHEN** 提交 1024 个 descriptor (顺序访问 page 0 → page 1023)
- **THEN** TLB direct-mapped thrashing: idx = page_num % 32, page N+32 替换 page N
- **AND** translate_stats_.misses = 1024, hits = 0
- **AND** hit rate = 0%
- **AND** 反映"无 LRU 简化模型"在顺序流下的极限场景;D4 引入 LRU 后命中提升

> **设计说明**: 32-entry TLB 是**简化模型**(per arch doc §5.3),无 LRU 替换策略。顺序流测试**不**适合验证 hit rate;测试应使用**工作集 ≤ 32 pages** 的重复访问场景才能体现 TLB 价值。

#### Scenario: page walk 跨页错误

- **GIVEN** size=8192, iova=0x1000 (跨 2 页: 0x1 与 0x2)
- **WHEN** `translate_timing(iova=0x1000, 8192, &paddr, &lat)`
- **THEN** 返回 -EIO (页跨界,per minimal_v1 §5.1)
- **AND** TLB 不填充

#### Scenario: translate() 向后兼容 (functional-mode)

- **GIVEN** 既有 minimal_v1 调用方使用 `translate(iova, size, &paddr)` (无 latency 输出)
- **WHEN** 调用 `translate()`
- **THEN** 内部转 `translate_timing()` 并丢弃 latency
- **AND** 返回值与 paddr 与 functional-mode 完全一致
- **AND** 既有 `[gmmu]` 测试零回归

---

### Requirement: SdmaEngineTLM cycle accounting (per design §4 v0.2, **不**新增 outstanding/AXI master port)

SdmaEngineTLM SHALL 在 timing-mode 下保持 5-port + backdoor 路径不变，仅：
- 通过既有 `set_translate_cb()` 注册的回调内部转调 `GmmuTLM::translate_timing()` 累加 latency
- 记录 `last_descriptor_complete_cycle_` 成员供外部测试断言
- **不**新增端口、**不**新增 outstanding 表、**不**拆 4KB sub-req、**不**接 Crossbar 仲裁

> **🔧 v0.2 重构 (per Oracle H2/H3)**: SdmaEngineTLM 在 timing-mode 下**不**新增 outstanding 拆分、**不**新增 AXI master port、**不**用 `rid`/`user` 字段（`CacheReqBundle` 无此字段）。SDMA 沿用既有 5-port + `set_vram_backdoor()` 路径，仅在 descriptor 处理路径中通过 `translate_cb_` 回调注入 `GmmuTLM::translate_timing()` 累加 TLB miss latency 到 `current_cycle_`。

#### Scenario: 4KB H2D descriptor (TLB hit)

- **GIVEN** `simulation_mode_=Timing`, descriptor (H2D, iova=0, vram=0, size=4096), GMMU TLB hit (lat=0)
- **WHEN** `submit_descriptor(desc)` 调用 (沿用 functional-mode API)
- **THEN** `translate_cb_(iova, size, &phys)` 调用 GmmuTLM::translate_timing → lat=0
- **AND** memcpy(vram_backdoor_+offset, host_backdoor_+phys, 4096) (functional-mode 0 cycle)
- **AND** `last_descriptor_complete_cycle_` 由 `DGpuBoard::sdma_fence_complete` 回调注入 = board.current_cycle_ (per TInv-3 + 七轮 P0-13)
- **AND** fence 触发 (per functional-mode 路径)
- **AND** completion_ring_->push(entry) → msix_update_pending (累加 msix_latency_cycles)

#### Scenario: 4KB H2D descriptor (TLB miss)

- **GIVEN** `tlb_miss_latency_cycles_=50`, descriptor iova 不在 TLB
- **WHEN** `submit_descriptor(desc)` 调用
- **THEN** GmmuTLM::translate_timing 返回 lat=50 (通过 SDMA::last_tlb_latency_ 成员回传, per 六轮 P0-12, 不二次调用避免 TLB hit 误读)
- **AND** SDMA memcpy(vram_backdoor_+offset, host_backdoor_+phys, 4096) (functional-mode 0 cycle, **不**经 ChStream req/resp)
- **AND** `last_descriptor_complete_cycle_` 由 DGpuBoard::sdma_fence_complete 回调注入 = board.current_cycle_ (per TInv-3: 任何模块不可自行 ++cycle, 由 board 集中推进)
- **AND** fence 触发 (per functional-mode 路径)
- **AND** completion_ring_->push(entry) → msix_update_pending (累加 msix_latency_cycles)
- **AND** cycle ≈ 54 cyc (0 msix + 1 fence + 0 memcpy + 50 tlb_miss + 0 sdma + 1 doorbell + 2 tlp = 54, per timing-mode.md §6.2 时序累加表, 七轮终稿)

#### Scenario: 翻译失败 (PTE 无效)

- **GIVEN** descriptor iova=0xFFFFFFFFFF, PTE invalid
- **WHEN** `submit_descriptor(desc)`
- **THEN** GmmuTLM::translate_timing 返回 -EIO
- **AND** submit_fence(tag=desc.tag, status=kFenceError) 立即触发
- **AND** memcpy **不**调用
- **AND** `last_descriptor_complete_cycle_` 由 `DGpuBoard::sdma_fence_complete` 回调注入 = board.current_cycle_ (per TInv-3 + 七轮 P0-13, 失败时立即触发, board.current_cycle_ 即当前 cycle)

#### Scenario: SDMA 公共 API 零修改

- **GIVEN** minimal_v1 functional-mode 调用方使用 `submit_descriptor(desc)` (无 latency 输出)
- **WHEN** timing-mode 启用
- **THEN** 内部调用 `translate_cb_(iova, size, &phys)` 与 functional-mode **完全一致**
- **AND** **不**新增 `submit_descriptor_timing` API
- **AND** **不**新增端口 (`axi_master_out` / `axi_master_in`)
- **AND** 既有 `[sdma]` 测试**零回归**

#### Scenario: 时序数字断言

- **GIVEN** TLB hit (lat=0), PCIe TLP 2 cyc, doorbell parse 1 cyc, MSI-X 1 cyc
- **WHEN** 4KB H2D descriptor 完成 + fence + MSI-X
- **THEN** `last_descriptor_complete_cycle_` - `submit_cycle_` ≈ 4 cycles (TLB hit 路径)
- **WHEN** TLB miss (lat=50)
- **THEN** 差 ≈ 54 cycles (TLB miss 路径)
- **AND** 测试用 `cycle_total ≈ analytical_model ± 5%` 断言

---

### Requirement: VramControllerTLM 行缓冲 + bandwidth (per design §5)

VramControllerTLM SHALL 继承 MemoryTLM 行为,**新增** 行缓冲 hit/miss tracking + bandwidth 上限模拟。

#### Scenario: 行缓冲 hit

- **GIVEN** `row_hit_cycles_=100, row_miss_cycles_=200`, row_buffer_base_=0x10000 (最近访问 0x10000)
- **WHEN** VramControllerTLM::tick() 接 req read addr 0x10500 (同行)
- **THEN** row_hit(0x10500) == true
- **AND** stats_vram_row_hits_++
- **AND** latency 累加 row_hit_cycles_=100

#### Scenario: 行缓冲 miss + 更新

- **GIVEN** row_buffer_base_=0x10000
- **WHEN** req read addr 0x20000 (跨行)
- **THEN** row_hit(0x20000) == false
- **AND** stats_vram_row_misses_++
- **AND** row_buffer_base_ = 0x20000, row_buffer_valid_ = true (更新)
- **AND** latency 累加 row_miss_cycles_=200

#### Scenario: bandwidth 上限拖慢

- **GIVEN** `bandwidth_gbps_=32`, req read 128 bytes
- **WHEN** VramControllerTLM::tick() 处理
- **THEN** cycles_needed = ceil(128 / 32_GBps) = 4 cycles
- **AND** latency += 4 (bandwidth wait)
- **AND** stats_bandwidth_limit_waits_++ (累加 wait cycles)

---

### Requirement: CrossbarTLM 4 routes 仲裁 (per design §6, M9 Oracle 反馈: GMMU 不在 Crossbar 数据路径)

CrossbarTLM SHALL 配置 4 routes: SDMA req out / Host egress / VramCtrl req in / Memory req in (GMMU 不在 Crossbar 数据路径,per arch doc §3.6 拓扑说明),暴露 `stats_arb_cycles_` + `stats_port_utilization_`。

#### Scenario: 4 routes 仲裁无冲突

- **GIVEN** CrossbarTLM 4 routes 配置完成 (per `dgpu_soc_timing_v1.json`)
- **WHEN** cycle N: 仅 1 个 port 有 req
- **THEN** arb_cycles = 0 (no wait)
- **AND** req 立即转发到目标 (VramCtrl 或 Memory)

#### Scenario: 仲裁等待 (多 port 并发)

- **GIVEN** cycle N: port[0] (SDMA) + port[1] (Host egress via Phase 5) — GMMU **不**在 Crossbar 数据路径 (per M9) 同时有 req
- **WHEN** CrossbarTLM::tick() 处理
- **THEN** `arb_latency_cycles_=4` 默认值
- **AND** 2 个 req 中 1 个立即通过, 1 个 wait 4 cycles
- **AND** stats_arb_cycles_ += 4 (累加)

#### Scenario: stats_port_utilization_ 计算

- **GIVEN** 10000 cycles 仿真
- **WHEN** port[0] (SDMA) 在 3000 cycles 内有 req
- **THEN** port_utilization[0] = 3000 / 10000 = 30%

---

### Requirement: 沿用 functional-mode Invariants (PT_BASE race + 指针稳定性)

timing-mode SHALL **沿用** functional-mode Inv-2 (指针稳定性: resize 先于 set_backing) + Inv-4 (GMMU PT_BASE LO/HI 写 race, translate_timing 同样直读 PT_BASE,race 被静默继承)。

#### Scenario: 指针稳定性沿用

- **GIVEN** timing-mode + functional-mode 共享 vram_storage_
- **WHEN** `DGpuBoard::init()` 调用
- **THEN** `vram_storage_.resize()` 必先于所有 `set_backing_view()` / `set_mem_view()` 调用
- **AND** resize 后指针稳定,无 UAF 风险

#### Scenario: PT_BASE LO/HI 写 race 沿用

- **GIVEN** `GmmuTLM::translate_timing(iova, ...)` 调用
- **WHEN** host 写 LO 寄存器 + 未写 HI 期间
- **THEN** `pt_base()` 返回的中间态 (LO 已更新 / HI 未更新) **不**影响 translate_timing 语义(v1.0 接受,per minimal_v1 Inv-4 + AGENTS.md 注 "v2.1 可加写锁")
- **AND** `translate_timing` 直接读 `pt_base_lo_` / `pt_base_hi_` 字段拼接 (无原子保护)

---

### Requirement: 共享 vram_storage_ 真源 (per TInv-1, timing-mode 适用)

DGpuBoard::vram_storage_ SHALL 仍是 SoC 全部访存的唯一 backing owner (per ADR-DGPU-05)。timing-mode **不**重新分配 vram_storage_;与 minimal_v1 **同一份 backing**。

#### Scenario: backing 注入 timing-mode

- **GIVEN** `simulation_mode_=Timing`, `DGpuBoard::vram_storage_` 已分配 (8GB default-init)
- **WHEN** `DGpuBoard::bind_memory_backings()` 调用
- **THEN** MemoryTLM::set_backing_view(vram_storage_.get(), vram_size_) 调用
- **AND** MemoryTLM 写入 backing 字节立即对 Crossbar / SDMA / GMMU 可见(同 cycle 内,per TInv-2 强 ordering)

#### Scenario: backing 路径与 functional-mode 同一来源

- **GIVEN** timing-mode 与 functional-mode 同一 DGpuBoard 实例(共享 vram_storage_)
- **WHEN** 切换 `simulation_mode` JSON 字段
- **THEN** 同一 vram_storage_ 真源
- **AND** 不需要重新分配 (per ADR-DGPU-05 v1.4 B7)

---

### Requirement: backdoor 禁用性能测试 (per TInv-4)

timing-mode 性能测试 SHALL **禁止** 用 `DGpuBoard::backdoor_read/write` 验证数据;backdoor 仅用于 correctness sanity。

#### Scenario: 性能测试用 cycle-approximate 路径

- **GIVEN** SDMA 提交 4KB H2D, GMMU TLB miss + VramCtrl row miss
- **WHEN** 测试验证数据正确性
- **THEN** 必须等 fence 完成 + MSI-X 触发,**或**用 `inflight_count()==0` 条件
- **AND** **不可**用 backdoor_read 在 fence 完成前验证(可能看到陈旧值)

#### Scenario: correctness sanity 用 backdoor

- **GIVEN** timing-mode 仿真完成 (10000 cycles)
- **WHEN** 测试 sanity check 用 backdoor_read 验证最终数据
- **THEN** 数据**应**正确(因为 cycle accounting 完成 + 一致性保证)
- **AND** **不**可作为性能回归指标(per TInv-4)

---

### Requirement: BAR 路由优先级 (per TInv-5,沿用 minimal_v1)

BAR1 路由 SHALL 沿用 minimal_v1 §4.2 优先级;timing-mode 在 doorbell 路径上累加 1 cyc parse latency。

#### Scenario: doorbell 路径时序

- **GIVEN** host mmio_write(BAR1, off=0x10010000, wptr)
- **WHEN** DGpuBoard::mmio_write 检测 kBar1DoorbellOffset
- **THEN** pcie_tlp_latency (2 cyc) + doorbell parse (1 cyc) = 3 cyc 累加
- **AND** sdma_engine_->mmio_write(1, 0x10010000, wptr) 调用
- **AND** SDMA ring consume (1 cyc 内部)

#### Scenario: storage 路径时序

- **GIVEN** host mmio_write(BAR1, off=0x1000, data) (非 doorbell)
- **WHEN** DGpuBoard::mmio_write 检测 PcieStorage 路由
- **THEN** pcie_tlp_latency (**2 cyc**, per Phase 5 PcieAxiAdapter) + functional-mode backdoor memcpy (**0 cyc**, per v0.2 §4 简化: BAR 路径不引入 Crossbar/VramCtrl cascade)
- **AND** 数据写入 vram_storage_ (同 cycle 内 per TInv-2)

---

### Requirement: translate_cb 签名兼容 (per TInv-6)

SdmaEngineTLM::translate_cb_ 签名 SHALL 严格保持 minimal_v1 不变;timing-mode 通过额外参数传递 latency。

#### Scenario: translate_cb 签名不变

- `using DmaTranslateCb = std::function<int(uint64_t iova, uint32_t size, uint64_t& phys)>;`
- **GIVEN** SdmaEngineTLM::set_translate_cb 注册 GmmuTLM::translate 回调
- **WHEN** SDMA 在 timing-mode 触发 callback
- **THEN** GmmuTLM::translate() 调用 (内部转 translate_timing,lat=0 丢弃)
- **AND** 返回值与 paddr 与 functional-mode 完全一致
- **AND** **不**破坏 callback 签名兼容性

#### Scenario: latency 内部额外传递 (v0.2 简化)

- **GIVEN** timing-mode 启用 + SDMA `set_translate_cb()` 注册回调
- **WHEN** SDMA descriptor 处理需要 GMMU TLB latency
- **THEN** `translate_cb_(iova, size, &phys)` 内调用 `gmmu_->translate_timing(iova, size, &phys, &lat_cycles)` (额外参数 `lat_cycles`)
- **AND** SDMA **不**自加 current_cycle_ (per TInv-3);last_descriptor_complete_cycle_ 由 DGpuBoard::sdma_fence_complete 回调注入 = board.current_cycle_ (per 六轮 P0-13)
- **AND** SDMA 沿用既有 `submit_descriptor()` API,**不**新增 `submit_descriptor_timing`

---

### Requirement: 23 ABI 字节级兼容 (per NG3)

23 ABI 签名 SHALL 完全冻结;timing-mode 是仿真模式切换,**不**改变 ABI 含义。

#### Scenario: 23 ABI 签名零修改

- **GIVEN** `include/abi/cpptlm_emulator.h` 现状
- **WHEN** `git diff HEAD -- include/abi/cpptlm_emulator.h`
- **THEN** 仅包含既有 deprecation marker,**不**有 signature 改动
- **AND** `simulation_mode` 是 DGpuBoard 内部字段,**不**暴露 ABI

#### Scenario: driver 视角零影响

- **GIVEN** UE driver (ctypes/dlopen) 加载 `libcpptlm_emulator.so`
- **WHEN** `cpptlm_emulator_create("configs/dgpu_soc_timing_v1.json")` 调用
- **THEN** driver 端 ABI 调用与 minimal_v1 完全一致
- **AND** `[abi][minimal_dgpu_soc]` 28 assertions 全绿(用 timing JSON 也通过)

---

### Requirement: minimal_v1 functional-mode 零回归 (per §11 兼容性)

timing-mode SHALL **不**影响 minimal_v1 functional-mode 任何行为;既有 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路) **不**有任何回归。

#### Scenario: minimal_v1 functional 测试零回归

- **GIVEN** `dgpu_soc_minimal_v1.json` (functional-mode 默认)
- **WHEN** 完整测试套件运行
- **THEN** `[minimal_dgpu_soc]` 41 assertions 全绿
- **AND** `[pcie-memory]` 24 cases 全绿
- **AND** `[abi][minimal_dgpu_soc]` 28 assertions 全绿
- **AND** `[dgpu_board]` 既有套件全绿
- **AND** `[sdma]` 既有套件全绿
- **AND** `[gmmu]` 既有套件全绿
- **AND** `[memory_tlm]` 既有套件全绿

#### Scenario: simulation_mode=functional 显式声明

- **GIVEN** JSON 顶层 `simulation_mode: "functional"` (显式声明,非缺省)
- **WHEN** DGpuBoard::init() 调用
- **THEN** `simulation_mode_ = SimulationMode::Functional`
- **AND** 既有 functional-mode 路径触发(与缺省行为完全一致)
- **AND** init_timing_mode() **不**调用

---

### Requirement: timing-mode 性能回归测试套件 (per design §8)

timing-mode SHALL 提供 ≥ 30 unit cases + ≥ 5 E2E cases,新增 `[dgpu_soc_timing]` Catch2 标签。

#### Scenario: unit tests 覆盖

- **GIVEN** `[dgpu_soc_timing]` 标签测试套件
- **WHEN** 编译并运行 `cpptlm_tests "[dgpu_soc_timing]"`
- **THEN** ≥ 30 cases 全绿 (MemoryTLM + GmmuTLM + SDMA + VramCtrl + Crossbar)
- **AND** 测试文件: `test_memory_tlm_timing.cc` (6) + `test_gmmu_tlm_timing.cc` (6) + `test_sdma_engine_timing.cc` (6) + `test_vram_controller_tlm.cc` (5) + `test_crossbar_timing.cc` (3) + 其他

#### Scenario: E2E H2D throughput 回归

- **GIVEN** `dgpu_soc_timing_v1.json` 加载, 4MB H2D 测试
- **WHEN** 提交 1024 个 4KB descriptor + 跑仿真 1M cycles
- **THEN** throughput ≥ 10 GB/s (VramCtrl 32 GB/s 上限 × ~30% 利用率)
- **AND** throughput ≤ 32 GB/s (不超过 bandwidth 上限)
- **AND** 平均 read latency = 100-200 cyc (per MemoryTLM stats)

#### Scenario: E2E fence cycle accuracy

- **GIVEN** 1 个 4KB H2D descriptor (TLB hit + row hit)
- **WHEN** 仿真跑 N cycles
- **THEN** fence 完成 cycle ≈ **4 cycles** (TLB hit) 或 **~54 cycles** (TLB miss) (per timing-mode.md §6.2 v0.2 时序累加表)
- **AND** 实测 cycle 与 analytical 模型误差 ±5%

#### Scenario: 6 条新 Invariants 全验证

- **GIVEN** `test_dgpu_soc_timing_inv.cc` 6 个 test cases
- **WHEN** 运行测试
- **THEN** Inv-1 (共享 vram_storage_) PASS
- **AND** Inv-2 (同 cycle 内 ordering) PASS
- **AND** Inv-3 (cycle advance 对齐) PASS
- **AND** Inv-4 (backdoor 禁用性能测试) PASS
- **AND** Inv-5 (BAR 路由优先级) PASS
- **AND** Inv-6 (translate_cb 签名兼容) PASS

---

### Requirement: JSON dgpu_soc_timing_v1.json 配置 (per design §7)

`configs/dgpu_soc_timing_v1.json` SHALL 配置 7-8 模块 (VramCtrl **或** Memory 二选一, 互斥), **0 new connections** (per H2), 顶层 `simulation_mode: "timing"`。

#### Scenario: JSON 配置完整

- **GIVEN** `dgpu_soc_timing_v1.json` 文件
- **WHEN** `cmake --build build --target validate_topology`
- **THEN** JSON schema 验证通过
- **AND** **7 模块实例化** (VramControllerTLM **或** MemoryTLM 二选一, 互斥): PcieEndpointIP + SdmaEngineTLM + GmmuTLM + CrossbarTLM + (VramControllerTLM **或** MemoryTLM) + CompletionRingTLM + DGpuSoc (per H2)
- **AND** **0 new connections** (per H2: timing-mode 沿用 minimal_v1 functional-mode 拓扑, 不引入新 connection)

#### Scenario: 模块注册完整性

- **GIVEN** `include/chstream_register.hh:46` 宏体内追加 `ModuleFactory::registerObject<VramControllerTLM>("VramControllerTLM")` (per H1 Oracle 反馈: `REGISTER_CHSTREAM` 是无参宏,**不**接受参数; `REGISTER_MODULE(VramControllerTLM)` 会触发 `is_base_of<SimModule>` 静态断言失败 — VramControllerTLM 继承 MemoryTLM → ChStreamModuleBase → SimObject,不是 SimModule)
- **WHEN** ModuleFactory::instantiateAll() 调用
- **THEN** VramControllerTLM 在 getModuleRegistry() 中可查
- **AND** 通过 type 字符串 "VramControllerTLM" 实例化成功

---

