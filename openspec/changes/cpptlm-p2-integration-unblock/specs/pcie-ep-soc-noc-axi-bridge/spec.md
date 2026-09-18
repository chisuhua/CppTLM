# pcie-ep-soc-noc-axi-bridge: PcieEndpointIP ↔ SoC NOC AXI 桥接规范

> **所属 change**: [`cpptlm-p2-integration-unblock`](../proposal.md)
> **范围**: 4 集成断点修复 (Axi4CacheAdapter + MSI-X + SDMA + CompletionRing)
> **关联 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`

---

## ADDED Requirements

### Requirement: axi4-cache-adapter-protocol-bridge

`Axi4CacheAdapter` SHALL 在 `Axi4Bundle` (16-bit ID, OOO, wstrb) 与 `CacheReqBundle`/`CacheRespBundle` (8-bit ID, 简单) 之间双向转换，使 PcieEndpointIP 通过 xbar 无缝访问 SoC 内存。

#### Scenario: AXI 写事务转换 (D2: EP→SoC 写)

- **WHEN** PcieEndpointIP 通过 `axi_master_out` 发出 AW+W (awid=0x1234, awaddr=0x80000000, wdata=0xDEADBEEF, wstrb=0xF, wlast=1)
- **THEN** Axi4CacheAdapter 转换为 CacheReq (src_id=0x12, addr=0x80000000, cmd=WR, data=0xDEADBEEF, byte_en=0xF)
- **AND** 维护 `awid_to_src_id_` 映射表记录 `0x1234 → 0x12`
- **AND** 经 xbar 路由到目标 memory 控制器

#### Scenario: AXI 读事务转换 (D3: EP→SoC 读)

- **WHEN** PcieEndpointIP 通过 `axi_master_out` 发出 AR (arid=0x5678, araddr=0x80001000, arlen=0)
- **THEN** Axi4CacheAdapter 转换为 CacheReq (src_id=0x56, addr=0x80001000, cmd=RD)
- **AND** 维护 `arid_to_src_id_` 映射表记录 `0x5678 → 0x56`

#### Scenario: Cache 响应 → AXI R+B (D4: SoC→EP 读响应)

- **WHEN** xbar 返回 CacheResp (dst_id=0x56, data=0xCAFEBABE, status=OK)
- **THEN** Axi4CacheAdapter 查询 `src_id_to_arid_` 表, 还原 `rid=0x5678`
- **AND** 转换为 AXI R channel (rid=0x5678, rdata=0xCAFEBABE, rresp=OK, rlast=1)
- **AND** 通过 PcieAxiAdapter `master_resp()` 投递给 EP

#### Scenario: 写响应 B channel 转换

- **WHEN** xbar 返回 CacheResp (dst_id=0x12, status=OK) 对应先前 AW
- **THEN** Axi4CacheAdapter 查询 `src_id_to_awid_` 表, 还原 `bid=0x1234`
- **AND** 转换为 AXI B channel (bid=0x1234, bresp=OK)

#### Scenario: OOO 响应匹配

- **WHEN** EP 发出 AR arid=0x1000, AR arid=0x2000, AR arid=0x3000 (顺序发出)
- **AND** xbar OOO 返回顺序为 0x3000, 0x1000, 0x2000
- **THEN** Axi4CacheAdapter 各自还原 rid 并投递
- **AND** PcieAxiAdapter 通过 `rid` 字段正确匹配回原 AR 事务

#### Scenario: AXI 错误响应

- **WHEN** xbar 返回 CacheResp (status=DECERR)
- **THEN** Axi4CacheAdapter 转换为 AXI `rresp=DECERR` 或 `bresp=DECERR`
- **AND** 错误码正确传递到 PcieAxiAdapter

#### Scenario: 8-bit src_id 冲突

- **WHEN** EP 同时发出 AW awid=0x1200 和 AW awid=0x1201 (高 8 bit 同为 0x12)
- **THEN** Axi4CacheAdapter 拒绝第 2 个事务, 返回 AXI `bresp=SLVERR`
- **AND** 不污染 `awid_to_src_id_` 映射表

### Requirement: msix-delivery-ep-to-host

`PcieEndpointIP` SHALL 通过新增 2 个独立 MSI-X 端口 (`msix_delivery_in` ingress + `msix_delivery_out` egress) 聚合 SDMA + CompletionRing 中断源, 并主动 push 到 `HostBypassTLM::msix_delivery_in`, 实现 EP→Host 中断路径。

**端口契约** (Oracle 2027-09-17 修订):
- `PcieEndpointIP::msix_delivery_in` (ingress, MsiXDeliveryBundle): 接收来自 SDMA `done_out[4]` / CompletionRing `irq_out[3]` 等内部 MSI-X 源
- `PcieEndpointIP::msix_delivery_out` (egress, MsiXDeliveryBundle): 主动 push 到 HostBypassTLM (经程序化桥接, per `design.md` D7)
- `HostBypassTLM::msix_delivery_in` (ingress, MsiXDeliveryBundle): 接收 EP 推送, 累加 `msix_delivery_count_`, 经 `cpptlm_emulator_msix_clear_pending()` ABI 清除 pending

#### Scenario: MSI-X 单 vector 投递

- **WHEN** PcieEndpointIP::tick() 检测到内部 MSI-X 源触发 (SDMA done_out[4] 或 CompletionRing irq_out[3])
- **THEN** 构造 MsiXDeliveryBundle {vector=N, msg_addr=0xFEE00000, msg_data=0xCAFE}
- **AND** 主动 push 到 `PcieEndpointIP::msix_delivery_out` (egress)
- **AND** EP.tick() 转发到 `HostBypassTLM::msix_delivery_in` (ingress, 经程序化桥接)
- **AND** HostBypassTLM 接收后清除 pending (经 `cpptlm_emulator_msix_clear_pending` ABI)

#### Scenario: MSI-X 多 vector 优先级

- **WHEN** PcieEndpointIP 同时从 SDMA + CompletionRing 接收到 vector 0, 1, 2
- **THEN** EP 内部按 vector 编号聚合到 `msix_delivery_out` (0 → 1 → 2)
- **AND** 每个 vector 经 EP→HB 转发后清除对应 pending 位

#### Scenario: MSI-X 端口无 HostBypassTLM 绑定

- **WHEN** PcieEndpointIP 未连接 HostBypassTLM (单元测试场景, `set_host_bypass()` 未调用)
- **THEN** MSI-X 事件仍写入 `msix_delivery_in`, 但 `msix_delivery_out` 推送失败时记录错误日志
- **AND** 不影响 EP 内部状态 (pending 累积待下次 HB 绑定后批量 flush)

#### Scenario: MSI-X 端口命名严格区分

- **WHEN** 设计/实现引用 EP MSI-X 端口
- **THEN** 接收方向 (← SDMA/CR) **MUST** 使用 `msix_delivery_in`
- **AND** 发送方向 (→ HB) **MUST** 使用 `msix_delivery_out`
- **AND** 不允许混用或简写

### Requirement: sdma-engine-programmatic-bridge

`SdmaEngineTLM` SHALL 通过 `PcieEndpointIP::set_sdma_engine(SdmaEngineTLM*)` setter 实现程序化桥接 (per `design.md` D4 + D7), 绕过 `19-pcie-ip-microarchitecture.md` §14.2.3 JSON 声明式 EP 外层端口静默丢弃限制。

**SDMA 现有 5 端口** (per `sdma_engine_tlm.hh` L82-86, header 注释锁定):
- `desc_in[0]` (ingress, **PcieTlpBundle** kind=DMA_DESC) — descriptor ring 入口
- `mem_in[1]` (ingress, PcieTlpBundle) — VRAM 读响应
- `mem_out[2]` (egress, **PcieTlpBundle**) — VRAM 读写 (非 AXI)
- `host_out[3]` (egress, **PcieTlpBundle**) — Host 完成通知 (index=3, header 锁定)
- `done_out[4]` (egress, **PcieTlpBundle** kind=DMA_DONE) — MSI-X 中断源 (index=4, header 锁定)

**SDMA 新增 2 端口** (本 change 引入, 供未来 AXI 路径扩展):
- `axi_slave_in` (ingress, Axi4Bundle) — AXI 写入
- `axi_master_out` (egress, Axi4Bundle) — AXI 读出

**EP-SDMA 程序化桥接** (替代 JSON 声明式):
- `PcieEndpointIP::set_sdma_engine(SdmaEngineTLM*)` setter (P0.5-7)
- `PcieEndpointIP::tick()` 内部转发 SDMA 事件 (per Phase 8 HB/RC tick 转发模式)

#### Scenario: SDMA doorbell 触发 (程序化桥接)

- **WHEN** 软件经 HostBypassTLM 写入 SDMA doorbell 寄存器 (BAR1+0x10010000)
- **THEN** PcieEndpointIP::tick() 识别 doorbell 写入
- **AND** 调用 `sdma_->trigger_doorbell()` 触发 SDMA 引擎 (经程序化 setter 注入的指针)
- **AND** SDMA 通过 `desc_in[0]` 端口 (PcieTlpBundle kind=DMA_DESC) 读取 descriptor ring
- **AND** descriptor 解析后 SDMA 进入 DMA 执行态

#### Scenario: SDMA H2D/D2H 数据传输

- **WHEN** SDMA 执行 H2D (Host→Device) 数据搬运
- **THEN** SDMA 通过 `mem_out[2]` 端口 (PcieTlpBundle, **非 AXI master**) 写入 VRAM
- **AND** VRAM 写入完成后 SDMA 通过 `host_out[3]` 端口 (PcieTlpBundle) 写完成状态
- **AND** EP 程序化桥接 `sdma_->pop_host_out()` → 构造 BAR write 投递给 Host

#### Scenario: SDMA 完成中断

- **WHEN** SDMA 完成 descriptor ring 处理
- **THEN** SDMA 通过 `done_out[4]` 端口 (PcieTlpBundle kind=DMA_DONE) 发送完成信号 (index=4, header 锁定)
- **AND** EP 程序化桥接 `sdma_->pop_done_out()` → 构造 MsiXDeliveryBundle push 到 EP `msix_delivery_in`
- **AND** EP 经 `msix_delivery_out` 转发到 `HostBypassTLM::msix_delivery_in`

#### Scenario: SDMA JSON 接线最小化

- **WHEN** `dgpu_soc_with_pcie_ip.json` 包含 sdma 模块 (仅声明, 无端口连接)
- **THEN** 添加 sdma 模块声明 (name, type, params): `{ "name": "sdma", "type": "SdmaEngineTLM", "params": {...} }`
- **AND** **不添加**任何 sdma.↔pcie_ep. 端口 JSON 连接 (会被 19 §14.2.3 静默丢弃)
- **AND** **不添加**任何 sdma.↔xbar. JSON 连接 (走 EP 程序化桥接, 经 mem_out/host_out)
- **AND** `validate_topology` PASS (WARN 可接受, 实际数据流由 EP.set_sdma_engine() 程序化建立)

### Requirement: completion-ring-irq-out-wiring

`CompletionRingTLM::irq_out[3]` SHALL 经 `PcieEndpointIP::set_completion_ring(CompletionRingTLM*)` setter 程序化桥接到 `PcieEndpointIP::msix_delivery_in` (per `design.md` D7, 替代 JSON 声明式连接), 使 GPU 命令完成事件经 MSI-X 路径投递到 Host。

#### Scenario: CompletionRing IRQ 投递 (程序化桥接)

- **WHEN** GPU 命令完成, CompletionRing::tick() 设置 `irq_pending_ = true`
- **THEN** CompletionRing 通过 `irq_out[3]` 发出 MsiXDeliveryBundle {vector=3, ...}
- **AND** EP 程序化桥接 `completion_ring_->pop_irq_out()` → 构造 MsiXDeliveryBundle push 到 EP `msix_delivery_in`
- **AND** EP 经 `msix_delivery_out` 转发到 `HostBypassTLM::msix_delivery_in`
- **AND** Host 端 `cpptlm_emulator_msix_clear_pending(emu, 0, 3)` 清除 pending

#### Scenario: CompletionRing 与 SDMA done_out 共享 MSI-X 端口

- **WHEN** EP `msix_delivery_in` 接收 SDMA done_out (vector=N, index=4) 和 CompletionRing irq_out (vector=3, index=3)
- **THEN** PcieEndpointIP 维护 pending 队列, 按 vector 编号聚合到 `msix_delivery_out`
- **AND** 不丢失任一来源的中断

#### Scenario: header 注释同步

- **WHEN** 修改本 change 完成
- **THEN** `completion_ring_mvp.hh` L13 header 注释从 "irq_out[3] → pcie_ep.irq_out 转发" 改为 "irq_out[3] → pcie_ep.msix_delivery_in (程序化桥接, per `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` §14.2.3 限制)"
- **AND** header 注释与 spec 描述一致

### Requirement: pcie-endpoint-tick-four-direction-closed

`PcieEndpointIP::tick()` SHALL 实现 4 方向 AXI 数据流全部闭环: Host→EP 写/读, EP→SoC 写/读, 且不破坏 Phase 8 M1 既有程序化闭环逻辑。

#### Scenario: D1 Host→EP 写 (程序化闭环保留)

- **WHEN** HostBypassTLM 发出 `bar_write(BAR0, addr=0x100, data=0xCAFE)`
- **THEN** HB tick() 通过 `axi_slave_in` 推送给 PcieAxiAdapter
- **AND** PcieEndpointIP::tick() 消费后写入 `bar_store_[0x100] = 0xCAFE`
- **AND** 返回 `bresp=OK` 经程序化路径回 HB

#### Scenario: D2 EP→SoC 写 (新桥接路径)

- **WHEN** SDMA 通过 EP `axi_master_out[0]` 发起 VRAM 写 (awid=0xABCD, awaddr=0x10000000, wdata=...)
- **THEN** Axi4CacheAdapter 转换为 CacheReq → xbar → VRAM 控制器
- **AND** VRAM 控制器返回 CacheResp (status=OK)
- **AND** Axi4CacheAdapter 还原 bid=0xABCD + bresp=OK 投递给 EP
- **AND** SDMA 接收 B channel 后进入下一 descriptor

#### Scenario: D3 EP→SoC 读 (新桥接路径)

- **WHEN** GPU shader 发起 VRAM 读 (经 PcieAxiAdapter)
- **THEN** Axi4CacheAdapter 转换为 CacheReq → xbar → VRAM
- **AND** VRAM 返回 CacheResp (data=0xDEADBEEF)
- **AND** Axi4CacheAdapter 还原 rid + rdata + rlast 投递给 EP
- **AND** PcieAxiAdapter OOO 匹配 (via rid 字段) 正确返回给原事务

#### Scenario: D4 SoC→EP 读响应 (本 change 新增闭环)

- **WHEN** xbar 返回 CacheResp (dst_id 匹配 EP outstanding read)
- **THEN** Axi4CacheAdapter 接收并还原 AXI R channel
- **AND** PcieEndpointIP::tick() 推送 `axi_master_resp()`
- **AND** HostBypassTLM tick() 接收并返回给原 initiator

### Requirement: validate-topology-with-new-modules

`examples/dgpu_soc_with_pcie_ip.json` SHALL 通过 `cmake --build build --target validate_topology`,包含 sdma 模块 + msix 路径 + completion_ring 路径,且不影响既有模块验证。

#### Scenario: 拓扑验证通过

- **WHEN** `cmake --build build --target validate_topology` 跑
- **THEN** 退出码 0
- **AND** 输出包含 "sdma" "msix_delivery" "completion_ring" 模块
- **AND** 不报 "unwired port" 错误

#### Scenario: 既有模块连接不破坏

- **WHEN** 新增 sdma + msix 路径
- **THEN** 既有 `pcie_ep`/`xbar`/`vram0`/`host_bypass`/`root_complex` 模块连接全部保留
- **AND** `[pcie]` 测试零回归 (≥36,454 assertions PASS)

### Requirement: pcie-ep-soc-bridge-test-tag

新增 4 个测试文件 SHALL 使用 **双标签** `[pcie][pcie-ep-soc-bridge]` (per Oracle 2027-09-17 G9, Catch2 标签不嵌套), 便于 CI 过滤和验收 Gate G2 + G6。

**双标签强制要求** (Oracle blocker):
- 每个 TEST_CASE **MUST** 同时声明 `[pcie]` 和 `[pcie-ep-soc-bridge]` 标签
- `[pcie-ep-soc-bridge]` 用于精准过滤 (本 change 测试)
- `[pcie]` 用于回归基线 (G6, 避免 Catch2 标签不嵌套导致 `[pcie]` 过滤器排除新测试)

#### Scenario: 标签可过滤 ([pcie-ep-soc-bridge])

- **WHEN** 跑 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"`
- **THEN** 4 个测试文件全部运行
- **AND** 至少 30 assertions 全部 PASS

#### Scenario: 标签过滤 [pcie] 包含新测试

- **WHEN** 跑 `./build/bin/cpptlm_tests "[pcie]"`
- **THEN** 既有 36,454 assertions + 新增 ≥250 assertions 全部 PASS
- **AND** 总断言数 ≥36,700 (G6 基线数学成立, 因双标签)

#### Scenario: 双标签不会重复统计

- **WHEN** 跑 `./build/bin/cpptlm_tests "[pcie]" "[pcie-ep-soc-bridge]"` (多过滤器)
- **THEN** Catch2 按 OR 逻辑运行, 无重复断言计数
- **AND** 总运行数 = 既有 + 新增, 不出现 2x

#### Scenario: 单标签失败验证 (反向)

- **WHEN** 新测试文件**仅**声明 `[pcie-ep-soc-bridge]` (漏掉 `[pcie]`)
- **THEN** 跑 `[pcie]` 时**不包含**新测试 (Catch2 标签不嵌套)
- **AND** G6 增长数学失效 (36,454 → 36,454, 无新增)
- **AND** 因此 G9 强制双标签, 任何漏标视为 P0 实施缺陷

---

## 关联

- **所属 change**: [`cpptlm-p2-integration-unblock`](../proposal.md)
- **design**: [`../design.md`](../design.md) (4 决策 + 7 风险)
- **tasks**: [`../tasks.md`](../tasks.md) (P0.5-1..P0.5-9 实施步骤)
