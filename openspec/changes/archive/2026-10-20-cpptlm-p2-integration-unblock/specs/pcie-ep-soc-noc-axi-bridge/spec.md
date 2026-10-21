# pcie-ep-soc-noc-axi-bridge: PcieEndpointIP ↔ SoC NOC AXI 桥接规范 (Oracle 2027-09-17 重写版)

> **所属 change**: [`cpptlm-p2-integration-unblock`](../proposal.md)
> **范围**: 4 集成断点修复 (Axi4CacheAdapter + MSI-X + SDMA + CompletionRing)
> **关联 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`
> **关键修订 (Metis + Oracle fact-check 2027-09-17)**:
> - D2 重写: transaction_id 直通, 去除 8-bit src_id 冲突 (基于不存在前提)
> - D3 API 修正: `attach_to_endpoint()` 而非误引用的 `set_host_bypass()`
> - D8 新增: framebuffer 单一真源契约
> - 测试基线重算: `[pcie]` 36,598 / 415 cases, `[chstream]` 184 / 46 cases (实测)

---

## MODIFIED Requirements

### Requirement: axi4-cache-adapter-protocol-bridge (D2 重写)

`Axi4CacheAdapter` SHALL 在 `Axi4Bundle` (16-bit ID, OOO, wstrb) 与 `CacheReqBundle`/`CacheRespBundle` (**64-bit `transaction_id`**, **无 src_id/dst_id 字段**) 之间做 1:1 字段转换, 使 PcieEndpointIP 通过 xbar 无缝访问 SoC 内存。

**核心事实 (Metis + Oracle 2027-09-17 代码核验)**:
- `CacheReqBundle` (`include/bundles/cache_bundles_tlm.hh:30`) 字段: `transaction_id[64]`, `parent_id[64]`, `fragment_id[8]`, `fragment_total[8]`, `address[64]`, `size[8]`, `is_write`, `data[64]`
- `CacheRespBundle` 字段: `transaction_id[64]`, `data[64]`, `is_hit`, `error_code[8]`, `first`, `last`
- **无 `src_id`/`dst_id` 字段** (旧 design 误诊)
- 响应路径用源端口 index 位置路由 + `transaction_id` 直通配对 (`crossbar_tlm.hh:97-144`)

#### Scenario: AXI 写事务转换 (D2: EP→SoC 写)

- **WHEN** PcieEndpointIP 通过 `axi_master_out` 发出 AW+W (awid=0x1234, awaddr=0x80000000, awsize=2, awlen=0, wdata=0xDEADBEEF, wstrb=0xF, wlast=1)
- **THEN** Axi4CacheAdapter 转换为 CacheReq (`transaction_id=0x1234`, `address=0x80000000`, `size=2`, `is_write=true`, `data=0xDEADBEEF`, `fragment_id=0`, `fragment_total=1`)
- **AND** 经 xbar 路由到目标 memory 控制器
- **AND** 16-bit AXI ID **直接装入** 64-bit transaction_id (无压缩, 无冲突)

#### Scenario: AXI 读事务转换 (D3: EP→SoC 读)

- **WHEN** PcieEndpointIP 通过 `axi_master_out` 发出 AR (arid=0x5678, araddr=0x80001000, arsize=2, arlen=0)
- **THEN** Axi4CacheAdapter 转换为 CacheReq (`transaction_id=0x5678`, `address=0x80001000`, `size=2`, `is_write=false`, `fragment_id=0`, `fragment_total=1`)
- **AND** 经 xbar 路由到目标 memory 控制器

#### Scenario: Cache 响应 → AXI R+B (D4: SoC→EP 读响应)

- **WHEN** xbar 返回 CacheResp (`transaction_id=0x5678`, `data=0xCAFEBABE`, `error_code=0` (OK))
- **THEN** Axi4CacheAdapter 拆分 `transaction_id=0x5678` → `rid=0x5678`
- **AND** 转换为 AXI R channel (`rid=0x5678`, `rdata=0xCAFEBABE`, `rresp=OK (0)`, `rlast=true`)
- **AND** 通过 PcieAxiAdapter `master_resp()` 投递给 EP

#### Scenario: 写响应 B channel 转换

- **WHEN** xbar 返回 CacheResp (`transaction_id=0x1234`, `error_code=0` (OK)) 对应先前 AW
- **THEN** Axi4CacheAdapter 拆分 `transaction_id=0x1234` → `bid=0x1234`
- **AND** 转换为 AXI B channel (`bid=0x1234`, `bresp=OK (0)`)

#### Scenario: OOO 响应匹配 (transaction_id 天然支持)

- **WHEN** EP 发出 AR arid=0x1000, AR arid=0x2000, AR arid=0x3000 (顺序发出)
- **AND** xbar OOO 返回顺序为 0x3000, 0x1000, 0x2000
- **THEN** Axi4CacheAdapter 各自还原 `rid` 并投递 (无映射表查询, 直接从 CacheResp.transaction_id[15:0] 拆分)
- **AND** PcieAxiAdapter 通过 `rid` 字段正确匹配回原 AR 事务 (16-bit ID 足够)

#### Scenario: AXI 错误响应 (error_code 映射)

- **WHEN** xbar 返回 CacheResp (`transaction_id=0x5678`, `error_code=1` (DECERR))
- **THEN** Axi4CacheAdapter 映射 `error_code=1` → AXI `rresp=DECERR (3)` (per `AxiErrorCode::DECERR`)
- **AND** 错误码正确传递到 PcieAxiAdapter (供后续 Phase 6 `Axi4Mapper` 处理)

#### Scenario: 多拍 burst 拆分 (fragment 多拍)

- **WHEN** EP 发出 AW (awid=0xABCD, awlen=3, 4 拍 wdata wdata wdata wdata)
- **THEN** Axi4CacheAdapter 拆分为 4 个 CacheReq, 每个 `fragment_id=0..3`, `fragment_total=4`
- **AND** 第一个 CacheReq 的 `is_first_fragment()=true`, 最后一个的 `is_last_fragment()=true`
- **AND** 4 个 `transaction_id=0xABCD` (同 ID, fragment 区分) 经 xbar 路由
- **AND** 4 个 CacheResp 经 OOO 匹配还原 rid+rlast

#### Scenario: Backpressure (容量满时拒收)

- **WHEN** Axi4CacheAdapter 已 outstanding 满 (默认 16, 与 Axi4Mapper 对齐)
- **AND** EP 发出新 AW (awid=0x1234)
- **THEN** Axi4CacheAdapter 返 AXI `awready=0` (拒收, 非 SLVERR)
- **AND** EP 必须等待 `awready=1` 才发数据
- **AND** outstanding 释放后 `awready=1` 自动恢复

---

### Requirement: msix-delivery-ep-to-host (D3 API 修正)

`PcieEndpointIP` SHALL 通过新增 2 个独立 MSI-X 端口 (`msix_delivery_in` ingress + `msix_delivery_out` egress) 聚合 SDMA + CompletionRing 中断源, 并经 `HostBypassTLM::attach_to_endpoint(EP*)` 既有 API 关联 push 到 `HostBypassTLM::msix_delivery_in`。

**API 修正 (Metis 2027-09-17 fact-check)**: HB↔EP 关联**不是** `PcieEndpointIP::set_host_bypass()` setter (此 API 不存在), 而是既有 **`HostBypassTLM::attach_to_endpoint(EP*)`** (HB 侧 API, Phase 8 M1 已 ship). EP tick() 内调用 `host_bypass_->trigger_msi(vector, ...)` 经此关联推送.

**vector 命名空间 (Oracle 新增)**: MSI-X vector 由 EP MsiXTable 统一分配. SDMA / CompletionRing 不应硬编码 vector 编号, 桥接代码须经 `ep->allocate_msix_vector(SDMA_FENCE_VECTOR)` 接口向 EP 注册.

**端口契约** (Oracle 修订):
- `PcieEndpointIP::msix_delivery_in` (ingress, MsiXDeliveryBundle): 接收来自 SDMA `done_out[4]` / CompletionRing `irq_out[3]` 等内部 MSI-X 源
- `PcieEndpointIP::msix_delivery_out` (egress, MsiXDeliveryBundle): 主动 push 到 HostBypassTLM
- `HostBypassTLM::msix_delivery_in` (ingress, MsiXDeliveryBundle): 接收 EP 推送, 累加 `msix_delivery_count_`, 经 `cpptlm_emulator_msix_clear_pending()` ABI 清除 pending

#### Scenario: MSI-X 单 vector 投递

- **WHEN** PcieEndpointIP::tick() 检测到内部 MSI-X 源触发 (SDMA done_out[4] 或 CompletionRing irq_out[3])
- **THEN** 构造 MsiXDeliveryBundle {vector=N, msg_addr=0xFEE00000, msg_data=0xCAFE, trans_id=T}
- **AND** 主动 push 到 `PcieEndpointIP::msix_delivery_out` (egress)
- **AND** EP.tick() 经 `host_bypass_->trigger_msi(N)` (via attach_to_endpoint 关联) 转发到 `HostBypassTLM::msix_delivery_in`
- **AND** HostBypassTLM 接收后清除 pending (经 `cpptlm_emulator_msix_clear_pending` ABI)

#### Scenario: MSI-X 多 vector 优先级

- **WHEN** PcieEndpointIP 同时从 SDMA + CompletionRing 接收到 vector 0, 1, 2
- **THEN** EP 内部按 vector 编号聚合到 `msix_delivery_out` (0 → 1 → 2)
- **AND** 每个 vector 经 EP→HB 转发后清除对应 pending 位

#### Scenario: MSI-X 端口无 HostBypassTLM 绑定

- **WHEN** PcieEndpointIP 未连接 HostBypassTLM (单元测试场景, `HostBypassTLM::attach_to_endpoint()` 未调用)
- **THEN** MSI-X 事件仍写入 `msix_delivery_in`, 但 `msix_delivery_out` 推送跳过 (`host_bypass_ == nullptr` 检查)
- **AND** 不影响 EP 内部状态 (pending 累积待下次 HB 绑定后批量 flush)

#### Scenario: MSI-X 端口命名严格区分

- **WHEN** 设计/实现引用 EP MSI-X 端口
- **THEN** 接收方向 (← SDMA/CR) **MUST** 使用 `msix_delivery_in`
- **AND** 发送方向 (→ HB) **MUST** 使用 `msix_delivery_out`
- **AND** 不允许混用或简写

---

### Requirement: sdma-engine-programmatic-bridge (D4 锁定)

`SdmaEngineTLM` SHALL 通过 **`PcieEndpointIP::set_sdma_engine(SdmaEngineTLM*)` setter** 实现程序化桥接 (per `design.md` D4 + D7), 绕过 `19-pcie-ip-microarchitecture.md` §14.2.3 JSON 声明式 EP 外层端口静默丢弃限制。

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

---

### Requirement: completion-ring-irq-out-wiring (D5 锁定)

`CompletionRingTLM::irq_out[3]` SHALL 经 `PcieEndpointIP::set_completion_ring(CompletionRingTLM*)` setter 程序化桥接到 `PcieEndpointIP::msix_delivery_in` (per `design.md` D5, 替代 JSON 声明式连接), 使 GPU 命令完成事件经 MSI-X 路径投递到 Host。

**vector 命名空间修正 (Oracle 新增)**: CompletionRing 不应硬编码 vector=3, 应经 `ep->allocate_msix_vector(COMPLETION_RING_VECTOR)` 向 EP 注册唯一 vector 编号.

#### Scenario: CompletionRing IRQ 投递 (程序化桥接)

- **WHEN** GPU 命令完成, CompletionRing::tick() 设置 `irq_pending_ = true`
- **THEN** CompletionRing 通过 `irq_out[3]` 发出 MsiXDeliveryBundle {vector=CR_VECTOR, ...}
- **AND** EP 程序化桥接 `completion_ring_->pop_irq_out()` → 构造 MsiXDeliveryBundle push 到 EP `msix_delivery_in`
- **AND** EP 经 `msix_delivery_out` 转发到 `HostBypassTLM::msix_delivery_in`
- **AND** Host 端 `cpptlm_emulator_msix_clear_pending(emu, 0, CR_VECTOR)` 清除 pending

#### Scenario: CompletionRing 与 SDMA done_out 共享 MSI-X 端口

- **WHEN** EP `msix_delivery_in` 接收 SDMA done_out (vector=SDMA_VECTOR, index=4) 和 CompletionRing irq_out (vector=CR_VECTOR, index=3)
- **AND** SDMA_VECTOR ≠ CR_VECTOR (EP MsiXTable 唯一分配)
- **THEN** PcieEndpointIP 维护 pending 队列, 按 vector 编号聚合到 `msix_delivery_out`
- **AND** 不丢失任一来源的中断
- **AND** vector 间无冲突 (每个 vector 独立 pending bit)

#### Scenario: header 注释同步

- **WHEN** 修改本 change 完成
- **THEN** `completion_ring_mvp.hh` L13 header 注释从 "irq_out[3] → pcie_ep.irq_out 转发" 改为 "irq_out[3] → pcie_ep.msix_delivery_in (程序化桥接, per `19-pcie-ip-microarchitecture.md` §14.2.3 限制)"
- **AND** header 注释与 spec 描述一致

---

### Requirement: pcie-endpoint-tick-four-direction-closed (D3/D4 闭环)

`PcieEndpointIP::tick()` SHALL 实现 4 方向 AXI 数据流全部闭环: Host→EP 写/读, EP→SoC 写/读, 且**不破坏 Phase 8 M1 既有程序化闭环逻辑** (flag-gated 默认 disable, Oracle 新增纪律).

#### Scenario: D1 Host→EP 写 (程序化闭环保留, Phase 8 M1 既有)

- **WHEN** HostBypassTLM 发出 `bar_write(BAR0, addr=0x100, data=0xCAFE)`
- **THEN** HB tick() 通过 `axi_slave_in` 推送给 PcieAxiAdapter
- **AND** PcieEndpointIP::tick() 消费后写入 `bar_store_[0x100] = 0xCAFE`
- **AND** 返回 `bresp=OK` 经程序化路径回 HB

#### Scenario: D2 EP→SoC 写 (新桥接路径, flag-gated)

- **WHEN** SDMA 通过 EP `axi_master_out[0]` 发起 VRAM 写 (awid=0xABCD, awaddr=0x10000000, wdata=...) (在 `ep->set_xbar_master(adapter_)` setter 调用后)
- **THEN** Axi4CacheAdapter 转换 `awid[16] → CacheReq.transaction_id[64]` (=0xABCD) → xbar → VRAM 控制器
- **AND** VRAM 控制器返回 CacheResp (`transaction_id=0xABCD`, `error_code=0`)
- **AND** Axi4CacheAdapter 拆分 `transaction_id=0xABCD → bid=0xABCD` + `bresp=OK (0)` 投递给 EP
- **AND** SDMA 接收 B channel 后进入下一 descriptor

#### Scenario: D3 EP→SoC 读 (新桥接路径)

- **WHEN** GPU shader 发起 VRAM 读 (经 PcieAxiAdapter)
- **THEN** Axi4CacheAdapter 转换 `arid[16] → CacheReq.transaction_id[64]` → xbar → VRAM
- **AND** VRAM 返回 CacheResp (`data=0xDEADBEEF`)
- **AND** Axi4CacheAdapter 拆分 `transaction_id → rid + rdata + rlast` 投递给 EP
- **AND** PcieAxiAdapter OOO 匹配 (via rid 字段) 正确返回给原事务

#### Scenario: D4 SoC→EP 读响应 (本 change 新增闭环)

- **WHEN** xbar 返回 CacheResp (`transaction_id` 匹配 EP outstanding read)
- **THEN** Axi4CacheAdapter 接收并还原 AXI R channel
- **AND** PcieEndpointIP::tick() 推送 `axi_master_resp()`
- **AND** HostBypassTLM tick() 接收并返回给原 initiator

#### Scenario: 默认 disable 不破坏 Phase 8 M1 回归 (Oracle 新增纪律)

- **WHEN** Axi4CacheAdapter 已实现但 `ep->set_xbar_master()` 未被调用
- **THEN** `axi_master_out[0]` 走旧路径 (bar_store_ 旁路), 既有 Phase 8 M1 E2E 零回归
- **AND** `[pcie]` 36,598 / 415 cases PASS

---

### Requirement: framebuffer-single-source-contract (D8 新增)

本 change 新增 4 断点修复**SHALL NOT 破坏** framebuffer 单一真源约定. Axi4CacheAdapter / SDMA / CompletionRing 出站写目标 **SHALL** 等于 `DGpuBoard::framebuffer_` backing (P0.5-landing 单一真源契约).

#### Scenario: Axi4CacheAdapter 出站写入 framebuffer_

- **WHEN** EP 经 Axi4CacheAdapter 发起 VRAM 写 (awid=0x1234, awaddr=0x10000000, wdata=0xDEADBEEF)
- **AND** DGpuBoard::init() 已分配 `framebuffer_storage_(N, 0)` + bind_memory_backings() 注入 SDMA/GMMU/MemTLM
- **THEN** 数据写到 `framebuffer_storage_[awaddr]` (=0x10000000 偏移)
- **AND** BAR1 mmio_read(1, 0x10000000, out, 4) 返回相同字节 (帧缓冲单源一致性)
- **AND** SDMA `set_vram_backdoor(framebuffer_.data(), N)` 读同一地址拿到相同数据

#### Scenario: DGpuBoard init order assert (D8 不变量 4)

- **WHEN** `DGpuBoard::init()` 执行
- **AND** `framebuffer_size_ > 0` (从 JSON bar_sizes[1] 派生)
- **AND** `framebuffer_storage_.resize()` 已分配
- **THEN** `framebuffer_ptr_ != nullptr` assert 通过
- **AND** bind_memory_backings() 注入 SDMA/GMMU backdoor
- **AND** set_sdma_engine() + set_completion_ring() + set_xbar_master() setter 调用
- **AND** sim_thread_ 启动
- **ELSE** (framebuffer_ptr_ == nullptr) runtime_error, init 失败, sim_thread_ 不启动

#### Scenario: 拒绝 setter 接线 (framebuffer 未分配)

- **WHEN** 用户绕过 init() 直接调 `set_sdma_engine(sdma)` (framebuffer_ptr_ 仍 nullptr)
- **THEN** setter 返 -EINVAL, 记录错误日志
- **AND** sdma_engine_ 保持 nullptr
- **AND** 后续 tick() 检测到 sdma_engine_ == nullptr 跳过 SDMA 桥接逻辑

---

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
- **AND** `[pcie]` 测试零回归 (≥36,598 / 415 cases PASS, 实测基线)

---

### Requirement: pcie-ep-soc-bridge-test-tag (G9 修订)

新增 4 个测试文件 SHALL 使用 **双标签** `[pcie][pcie-ep-soc-bridge]` (per Oracle 2027-09-17 G9, Catch2 标签不嵌套), 便于 CI 过滤和验收 Gate G2 + G6。

**双标签强制要求** (Oracle blocker):
- 每个 TEST_CASE **MUST** 同时声明 `[pcie]` 和 `[pcie-ep-soc-bridge]` 标签
- `[pcie-ep-soc-bridge]` 用于精准过滤 (本 change 测试)
- `[pcie]` 用于回归基线 (G6, 避免 Catch2 标签不嵌套导致 `[pcie]` 过滤器排除新测试)

**Catch2 标签语义修正 (Metis 2027-09-17)**:
- 单 filter 命令 `./cpptlm_tests "[pcie]"`: 匹配 `[pcie]` 标签测试, 含双标签测试
- 单 filter 命令 `./cpptlm_tests "[pcie-ep-soc-bridge]"`: 匹配该标签测试, 含双标签测试
- 双 filter 命令 `./cpptlm_tests "[pcie]" "[pcie-ep-soc-bridge]"` (空格): **AND (交集)**, 不是 OR
- 双 filter 命令 `./cpptlm_tests "[pcie],[pcie-ep-soc-bridge]"` (逗号): **OR (并集)**, 才无重复计数

#### Scenario: 标签可过滤 ([pcie-ep-soc-bridge])

- **WHEN** 跑 `./build/bin/cpptlm_tests "[pcie-ep-soc-bridge]"`
- **THEN** 4 个测试文件全部运行
- **AND** 至少 30 assertions 全部 PASS

#### Scenario: 标签过滤 [pcie] 包含新测试 (G6 floor)

- **WHEN** 跑 `./build/bin/cpptlm_tests "[pcie]"`
- **THEN** 既有 36,598 assertions + 新增 ≥250 assertions 全部 PASS
- **AND** 总断言数 ≥36,850 (G6 floor = 既有基线 + 双标签新测试增量, 数学成立)

#### Scenario: 双 filter 交集 (Metis 修订)

- **WHEN** 跑 `./build/bin/cpptlm_tests "[pcie]" "[pcie-ep-soc-bridge]"` (空格分隔)
- **THEN** 仅运行双标签测试 (AND 交集)
- **AND** 不是 OR (避免双重计数误解)

#### Scenario: 单标签失败验证 (反向)

- **WHEN** 新测试文件**仅**声明 `[pcie-ep-soc-bridge]` (漏掉 `[pcie]`)
- **THEN** 跑 `[pcie]` 时**不包含**新测试 (Catch2 标签不嵌套)
- **AND** G6 增长数学失效 (36,598 → 36,598, 无新增)
- **AND** 因此 G9 强制双标签, 任何漏标视为 P0 实施缺陷

---

## 关联

- **所属 change**: [`cpptlm-p2-integration-unblock`](../proposal.md)
- **design**: [`../design.md`](../design.md) (D1-D8 决策 + 风险表 + 实施计划)
- **tasks**: [`../tasks.md`](../tasks.md) (4 commit 实施路径)
- **前置**: Phase 9 P0.5-cpptlm-minimal-dgpu-soc-v1-landing (archived, 2027-09-17)