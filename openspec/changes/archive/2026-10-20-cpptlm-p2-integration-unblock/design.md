# cpptlm-p2-integration-unblock: Design (4 断点修复详细实现路径)

> **配套**: [`proposal.md`](proposal.md) · [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md) · [`tasks.md`](tasks.md)
> **目标**: Phase 9 P2 (CP 跨仓接入) 启动的 4 个集成断点修复
> **关联阶段**: ArchForge 仓 `docs/roadmap/phase9-p2-unblock.md` (本 change 仅声明存在, 创建在 ArchForge)
> **修订**: D2 ID 映射策略重写 (transaction_id 直通, 去 8-bit 压缩), D3 API 引用修正 (set_host_bypass → attach_to_endpoint), D8 framebuffer 契约新增, 文档仓路径修正 (ArchForge)

---

## 1. Context

### 1.1 当前状态 (Phase 8 整合遗留,代码事实)

Phase 8 M1 整合交付完成了 `PcieEndpointIP` 与 SoC 的基础接线 (4 方向 AXI 主从框架)，但实际数据流存在 4 个断点。代码事实核验 (`pcie_endpoint_ip.cc` grep 无 xbar/CrossbarTLM/CacheReq 引用, 桥接点在 HostBypassTLM 而非 EP):

```
PcieEndpointIP
  │ internal PcieAxiAdapter + Axi4StreamAdapter
  ├── axi_slave_in   ◄──── (HB/RC 端程序化, Phase 8 M1)
  ├── slave_resp     ────► (HB/RC 端程序化, Phase 8 M1)
  ├── cfg_slave_in   ◄──── (未使用)
  ├── axi_master_out ────► ❌ 无外部连接 (Phase 8 M1 实际未接 xbar, JSON 声明式被 19 §14.2.3 静默丢弃)
  └── MsiXTable::irq_out ──► ❌ 无任何外部连接

SdmaEngineTLM    ────► ❌ 不在 dgpu_soc_with_pcie_ip.json
CompletionRing   ────► ❌ irq_out[3] 未接线 (per spec sdma-engine, 应 → pcie_ep.msix_delivery_in)
```

**关键代码事实**:
- `pcie_endpoint_ip.cc` grep 无 `xbar`/`CrossbarTLM`/`CacheReq`/`CacheResp` 引用 → EP→SoC 通路压根没接 (per Oracle 2027-09-17 fact-check)
- `HostBypassTLM::tick()` 是 4 方向 AXI 桥接实现处 (Phase 8 M1)
- DGpuBoardShell `bind_memory_backings()` (line 567) 是 SDMA/GMMU/MemTLM 注入 framebuffer backing 的程序化桥接点
- `MsiXDeliveryBundle` 已在 `include/bundles/pcie_bundles_tlm.hh:119` 定义 (字段: `vector[16]`, `msg_data[32]`, `msg_addr[64]`, `trans_id[32]`) — **不是新文件依赖**

### 1.2 协议不兼容核心问题 (Oracle + Metis 共识重写)

`PcieAxiAdapter` 用 `Axi4Bundle` (Phase 5 标准化):
- 字段: `awaddr`, `awid[16]`, `awlen`, `awsize`, `awburst`, `wdata`, `wstrb`, `wlast`, `bid[16]`, `bresp[2]`, `araddr`, `arid[16]`, `arlen`, `rdata`, `rresp[2]`, `rlast`
- **16-bit ID 空间** (`awid`/`arid`/`bid`/`rid`) 支持 OOO completion

`CrossbarTLM` 用 `CacheReqBundle`/`CacheRespBundle` (per `include/bundles/cache_bundles_tlm.hh` 实际字段):
- `CacheReqBundle`: `transaction_id[64]`, `parent_id[64]`, `fragment_id[8]`, `fragment_total[8]`, `address[64]`, `size[8]`, `is_write`, `data[64]`
- `CacheRespBundle`: `transaction_id[64]`, `parent_id[64]`, `fragment_id[8]`, `fragment_total[8]`, `data[64]`, `is_hit`, `error_code[8]`, `first`, `last`
- **关键事实 (经代码核实)**: **无 `src_id`/`dst_id` 字段**. 响应路径用源端口 index 位置路由 (`resp_out[i] = req_in[i]`), 关联靠 `transaction_id` 直通

**协议"不兼容"真相 (Oracle 2027-09-17 修订)**:
- ❌ **错误前提**: 旧 design 假设 `CacheReqBundle` 有 8-bit `src_id` (per `cpphdl_types.hh` 假定), 需要 16→8 压缩 + 双向映射表
- ✅ **真实情况**: `CacheReqBundle.transaction_id` 是 **64-bit**, AXI 16-bit ID 完全能塞进 (1:1 映射), 无压缩、无冲突、无 SLVERR 需要
- **错误码差异真实存在**: `CacheRespBundle` 无 `rresp`/`bresp` 字段, 错误码用 `error_code[8]` (0=OK, ≠0=err). Axi4CacheAdapter 需做 `AXI rresp/bresp → CacheRespBundle.error_code` 映射, **不是 ID 映射**

**修正后的 Axi4CacheAdapter 真实复杂度**: 
- 1:1 字段映射 (`awid[16] → transaction_id[64]` 直接赋值, `address[64]`/`size[8]`/`is_write`/`data[64]` 直通)
- `error_code → rresp/bresp` 反向映射 (rresp: 0=OK, 1/2/3=DECERR/SLVERR/EXOKAY)
- `fragment_id/fragment_total` 处理多拍请求 (`awlen` > 0 时需拆分)
- **复杂度大幅降低**: 旧设计 8-bit 冲突 + OOO 双向映射表全不需要

### 1.3 4 方向 AXI 流量 (代码事实修订)

| 方向 | 路径 | 当前状态 | 修复路径 |
|------|------|---------|----------|
| **D1**: Host→EP 写/读 | HB→`axi_slave_in` | ✅ 程序化闭环 (Phase 8 M1) | 不动 |
| **D2**: EP→Host 响应 | `slave_resp`→HB | ✅ 程序化闭环 (Phase 8 M1) | 不动 |
| **D3**: EP→SoC 写 | `axi_master_out`→❌ 无接线 | ❌ **从未接,非"单向连通"** (旧 design 误诊) | 新增 `Axi4CacheAdapter` + `EP.set_xbar_master(adapter)` setter |
| **D4**: EP→SoC 读响应 | xbar.resp_out→❌ 无接线 | ❌ BROKEN (与 D3 同根因) | `Axi4CacheAdapter` 的 `resp` 端推送 `PcieEndpointIP::tick()` 经 `slave_resp` 转 HB (借用既有路径) |
| **MSI-X**: EP→Host | MsiXTable::irq_out | ❌ 无连接 | D3 修订 (EP 双端口 `msix_delivery_in/out`, HB 增 ingress) |
| **SDMA**: EP↔SDMA | BAR doorbell↔SDMA | ❌ 不在 JSON | D4 (程序化桥接 `EP.set_sdma_engine()`) |
| **CompletionRing**: CR→EP | irq_out[3]→? | ❌ 未接线 | D5 (`EP.set_completion_ring()` 桥接 → `msix_delivery_in`) |

---

## 2. Goals / Non-Goals

### 2.1 Goals

- **G1**: 新增 `Axi4CacheAdapter` 桥接组件, 解决 D3/D4 协议不兼容
- **G2**: EP→Host MSI-X 中断投递路径建立 (D5)
- **G3**: SDMA 模块接入 `dgpu_soc_with_pcie_ip.json` (D6)
- **G4**: CompletionRing→EP irq_out 接线明确 (D7)
- **G5**: 4 方向 AXI 数据流全部 ✅, E2E 闭环
- **G6**: `[pcie-ep-soc-bridge]` 标签测试覆盖 (4 文件)
- **G7**: 既有 `[pcie]`/`[chstream]` 零回归

### 2.2 Non-Goals

- **NG1**: CP (CommandProcessor) 寄存器映射设计 — P2-1 单独 change
- **NG2**: P2-2/3/4/5/6/7/8 实际任务执行 — 本 change 仅 unblock
- **NG3**: 真实 RTL 桥接 (CppHDL/HybridCache)
- **NG4**: 性能优化 (cycle-accurate 时序)
- **NG5**: PCIe 协议层增强 (FC, retry buffer)
- **NG6**: 新 ABI 函数追加 (P5 冻结)

---

## 3. Decisions

### D1: Axi4CacheAdapter 设计 — **新独立类 vs 嵌入 PcieAxiAdapter**

**决策**: 新独立类 `Axi4CacheAdapter`, 路径 `include/framework/axi4_cache_adapter.hh`

**理由**:
- **复用性**: 未来 GPGPU/SoC 集成可能需要同样桥接 (e.g., 自研 IP → CacheNoC)
- **单一职责**: PcieAxiAdapter 专注 PCIe TLP↔AXI 转换, Axi4CacheAdapter 专注 AXI↔CacheReq/Resp 转换
- **可测试性**: 独立类便于单测 (无需 PcieEndpointIP 上下文)
- **2 方向状态机**: 请求 (AW+AR→req) + 响应 (R+B→resp) 分别管理

**考虑过的方案**:
- ❌ 嵌入 PcieAxiAdapter: 破坏单一职责, 增加 PcieAxiAdapter 复杂度
- ❌ 扩展 CrossbarTLM 支持 Axi4Bundle: 改动范围大, 影响其他 `[chstream]` 测试 (184 assertions / 46 cases)
- ❌ 协议层统一为 CacheReq/Resp: 破坏 AXI 标准化, 影响 Phase 5-6 已交付接口

### D2: Axi4CacheAdapter ID 映射策略 — **transaction_id 直通 (Oracle + Metis 2027-09-17 重写)**

**决策 (重写)**: Axi4 16-bit ID 与 CacheReq 64-bit `transaction_id` **1:1 直通**, **不压缩**、**无双映射表**、**无 SLVERR 冲突**:

```cpp
// 出站 (AW/AR → req): ID 直接打包
CacheReqBundle req;
req.transaction_id.write(aw.awid.read());  // 16-bit ID 直接装进 64-bit
req.address.write(aw.awaddr.read());
req.size.write(aw.awsize.read());
req.is_write.write(true);  // AW 路径
req.fragment_id.write(aw_beat_idx);  // 多拍
req.fragment_total.write(aw.awlen.read() + 1);

// 入站 (resp → R/B): transaction_id 直接拆分
CacheRespBundle resp;
const uint16_t bid = static_cast<uint16_t>(resp.transaction_id.read());  // 提取
// resp → B 通道: bid, bresp = decode_error_code(resp.error_code)
```

**真实复杂度 (~80 行实现)**:
- `awid[16]` ↔ `transaction_id[64]`: 直接赋值, 无损映射
- `rresp[2]/bresp[2]` ↔ `error_code[8]`: 反向映射 (0=OK, 1=DECERR, 2=SLVERR)
- `awlen[8]` ↔ `fragment_total-1[8]`: 多拍拆分
- `arid[16]` ↔ `transaction_id[64]`: 同 awid 路径
- `rlast` ↔ `fragment_id == fragment_total-1`: 多拍最后一拍

**理由 (Oracle 修订版)**:
- **代码事实 (已核验)**: `CacheReqBundle.transaction_id` 是 `ch_uint<64>`, 完全容纳 16-bit AXI ID (`include/bundles/cache_bundles_tlm.hh:30`)
- **真实问题**: 旧 design 假设 `src_id`/`dst_id` 字段存在 (per `cpphdl_types.hh` 假定), 但 `CacheReqBundle`/`CacheRespBundle` **无这两个字段**
- **响应路由**: `CrossbarTLM::tick()` (line 97-144) 按源端口 index 位置路由 (`resp_out[i] = req_in[i]`) + `transaction_id` 直通配对 (`resp.transaction_id = req.transaction_id`)
- **OOO 天然支持**: AXI 16-bit ID 装入 64-bit `transaction_id`, OOO 匹配在 `PcieAxiAdapter` 内嵌的 `Axi4Mapper` (Phase 6) 处理, Axi4CacheAdapter 仅做字段级转换

**考虑过的方案**:
- ❌ 旧 design 的"8-bit src_id 压缩 + 双向映射表 capacity 256": **基于不存在前提**, Oracle 2027-09-17 fact-check 否定
- ❌ 扩展 CacheReqBundle 为 16-bit ID: 破坏现有 `[chstream]` 测试
- ❌ 完全 OOO 处理: Phase 6 `Axi4Mapper` 已实现 (per `axi4_mapper.hh` L27-28 N+1 拒绝语义), Axi4CacheAdapter 仅做字段转换

**Backpressure 策略 (Oracle 新增)**:
- Axi4CacheAdapter 接受 AW/AR 前**预检** outstanding 容量 (默认 ≤16, Axi4Mapper 行为对齐)
- 容量满时 **拒收** (`awready/arready = 0`), 不返错误 (避免 SDMA/EP 触发不存在 retry loop)
- 与 Phase 6 `Axi4Mapper` 的 N+1 拒绝语义保持一致

### D3: MSI-X 投递路径 — **EP 双端口 + HB 单 ingress 端口 (API 引用修正)**

**决策 (Oracle 修订版 + API 修正)**:
- `PcieEndpointIP` 新增 **2 个独立端口**:
  - `msix_delivery_in` (ingress, MsiXDeliveryBundle) — 接收来自 SDMA `done_out[4]` / CompletionRing `irq_out[3]` 的 MSI-X 事件
  - `msix_delivery_out` (egress, MsiXDeliveryBundle) — 主动 push 到 HostBypassTLM
- `HostBypassTLM` 新增 1 个 ingress 端口 `msix_delivery_in` (MsiXDeliveryBundle)
- **API 修正 (Metis 2027-09-17)**: HB↔EP 关联**不是** `PcieEndpointIP::set_host_bypass()` setter, 而是既有 **`HostBypassTLM::attach_to_endpoint(EP*)`** (HB-side, 已有 API). EP tick() 内调用 `host_bypass_->trigger_msi(vector, ...)` 经此关联推送 MSI-X
- **vector 命名空间 (Oracle 新增)**: MSI-X vector 分配**由 EP MsiXTable 统一管理**, SDMA/CR 不应硬编码 vector=3. 桥接代码需向 EP 注册 vector (类似 `ep->allocate_msix_vector(SDMA_FENCE_VECTOR)`)

**理由 (修订版)**:
- **数据流方向**: EP 内部聚合 (in) → EP 转发 (out) → HB 接收 (in), 严格单向
- **协议简单**: MsiXDeliveryBundle {vector[16], msg_addr[64], msg_data[32], trans_id[32]} (per `pcie_bundles_tlm.hh:119-131`) 无需握手
- **不污染 AXI 路径**: 单独端口避免与 AXI 流混淆
- **EP 内部聚合**: SDMA done + CompletionRing irq 共用 EP→HB 通道, 减少端口爆炸
- **可独立测试**: 单独 ingress 便于 `[pcie-ep-soc-bridge]` 标签测试
- **API 一致性**: 使用既有 `attach_to_endpoint` 模式, 避免新增 setter (与 Phase 8 HB 设计语言一致)

**考虑过的方案**:
- ❌ 通过 `axi_slave_in` 复用: 协议不同 (MSI-X 是中断信号, AXI 是事务), 混淆语义
- ❌ 新增专门 MSI-X 控制器模块: 过度设计, HostBypassTLM 已足够
- ❌ 单一 EP 端口 (in 或 out 二选一): 无法聚合 SDMA + CompletionRing 两路来源
- ❌ `EP::set_host_bypass()` setter: API 不存在, 误引用 (per Metis 2027-09-17 fact-check)

### D4: SDMA JSON 接线 — **程序化桥接 (Phase 8 先例)**

**决策 (Oracle 修订版)**: `dgpu_soc_with_pcie_ip.json` 添加 sdma 模块, **但端口接线走程序化桥接模式** (per 19 §14.2.3 限制 + Phase 8 HB/RC tick 转发先例):

```json
{
  "name": "sdma",
  "type": "SdmaEngineTLM",
  "params": { "max_outstanding": 16, "fence_size": 4096 }
}
```

**程序化桥接路径** (非 JSON 声明式):
- **EP `set_sdma_engine(SdmaEngineTLM*)` setter** (P0.5-7): EP 持有 SDMA 指针, tick() 主动调用 SDMA API
- **SDMA 现有 5 端口** (per `sdma_engine_tlm.hh` L82-86):
  - `desc_in[0]` (ingress, PcieTlpBundle kind=DMA_DESC) — EP 经 setter push BAR 空间 descriptor
  - `mem_in[1]` (ingress, PcieTlpBundle) — VRAM 读响应
  - `mem_out[2]` (egress, PcieTlpBundle) — VRAM 读写
  - `host_out[3]` (egress, PcieTlpBundle) — Host 完成通知 (注意: index=3, 不是 0)
  - `done_out[4]` (egress, PcieTlpBundle kind=DMA_DONE) — MSI-X 中断源 (注意: index=4, 不是 0)
- **SDMA 新增 2 个 AXI 端口** (P0.5-7 任务): `axi_slave_in` (Axi4Bundle) + `axi_master_out` (Axi4Bundle) 供未来扩展
- **EP tick() SDMA 桥接逻辑**:
  1. 识别 doorbell 写入 (BAR1+0x10010000) → 触发 SDMA descriptor ring 处理
  2. SDMA `done_out[4]` 产生 → EP 接收 → 构造 MsiXDeliveryBundle push 到 `msix_delivery_in`
  3. SDMA `host_out[3]` 产生 → EP 接收 → 经 BAR write push 给 Host

**理由 (修订版)**:
- **19 §14.2.3 限制**: JSON 声明式 `pcie_ep.axi_slave_in/axi_master_out` 接线被静默丢弃, 程序化桥接是唯一可靠路径
- **Phase 8 HB/RC 先例**: HostBypassTLM 已在 tick() 内程序化转发 EP↔HB AXI 流, SDMA 沿用同模式
- **bundle 类型真实**: SDMA 现有端口全 PcieTlpBundle, 错误地标注为 "AXI master" 会导致误解
- **端口索引锁定**: header 注释已固化 host_out=3, done_out=4, 严格遵守

### D5: CompletionRing→EP irq_out — **程序化桥接 (per D7, 替代原 JSON 声明式)**

**决策 (Oracle 修订版)**: 修改 `completion_ring_mvp.cc` tick() 输出 `irq_out[3]`, **经 `PcieEndpointIP::set_completion_ring(CompletionRingTLM*)` setter 程序化桥接到 EP `msix_delivery_in`** (per design D7, 不在 JSON 中声明)

**理由 (修订版)**:
- **复用 MSI-X 路径**: CompletionRing 本身不是 PCIe 设备, 其中断应通过 EP 投递
- **避免新端口**: 不在 EP 上加专门 CompletionRing 端口, 与 MSI-X 共享路径
- **D7 一致性**: 程序化桥接而非 JSON 声明式, 避开 19 §14.2.3 静默丢弃限制
- **DGpuBoardShell setter 调用**: `ep->set_completion_ring(cr)` 在 board 装配阶段建立绑定

### D6: 测试策略 — **4 个独立测试文件 + 统一 `[pcie-ep-soc-bridge]` 标签 + 双标签防 G6 失效**

**决策 (Oracle 修订版)**:
- `test_axi4_cache_adapter.cc` — 桥接单测 (2 方向 round-trip, OOO, error)
- `test_pcie_endpoint_ip_msix_path.cc` — MSI-X E2E (EP→HB)
- `test_pcie_endpoint_ip_sdma_wiring.cc` — SDMA 程序化桥接 E2E (per D4)
- `test_pcie_endpoint_ip_completion_ring_wiring.cc` — CompletionRing→EP 接线 E2E
- **所有 4 文件必须双标签** `[pcie][pcie-ep-soc-bridge]` (per Oracle G9, Catch2 标签不嵌套, 避免 `[pcie]` 过滤器排除新测试, 保证 G6 基线增长数学成立)

**理由 (修订版)**:
- **职责单一**: 每个文件聚焦一个断点
- **标签双轨**: `[pcie-ep-soc-bridge]` 用于精准过滤 (本 change 测试), `[pcie]` 用于回归基线 (G6)
- **Catch2 GLOB 自动发现**: 无需 CMakeLists.txt 显式添加

### D7: JSON 接线约束 (Oracle 新增) — **声明式接线范围限制 + 程序化桥接强制**

**决策**: 本 change 内 SDMA + MSI-X + CompletionRing + **axi_master_out→xbar** **全部走程序化桥接**, 不依赖 JSON 声明式 EP 外层端口接线

**理由 (per Oracle 2027-09-17)**:
- **19 §14.2.3 已锁定**: `pcie_ep.axi_slave_in/axi_master_out` 声明式接线在 connection_resolver 两层下钻时**被静默丢弃** (test_axislavein_bridge_path_intact 锁定该行为), 且 19 明确"未来扩展属独立 change"
- **G5 风险**: 若强行 JSON 接线, `validate_topology` 可能产生 WARN 而非 FAIL, 实际数据流未建立
- **唯一可靠路径**: 程序化桥接 (Phase 8 HB/RC tick 转发先例), EP.set_sdma_engine() setter + tick() 内部转发
- **未来扩展路径**: 如需 JSON 声明式, 需先修订 19 §14.2.3 + 解锁 test_axislavein_bridge_path_intact (独立 change)

**JSON 在本 change 内的合法用途**:
- 添加 sdma 模块声明 (name, type, params)
- 添加 hb/ep/rc 顶层模块连接 (hb.axi_master ↔ ep.axi_slave_in 在 Phase 8 M1 已程序化闭环)
- 添加 msix_delivery 程序化桥接的 metadata 注释 (便于阅读)

**JSON 在本 change 内的禁用范围 (扩展, Oracle 修订)**:
- `ep.axi_master_out[N]` 任何目标 (xbar, sdma, vram0) — 静默丢弃 (per 19 §14.2.3)
- `ep.axi_slave_in[N]` 任何源 (host_bypass 已程序化) — 同上
- `ep.msix_delivery_in/out` 任何连接 — 走 EP.set_sdma_engine() / set_completion_ring() setter
- **新增**: `xbar[N]` 任何目标 (sdma/vram/memory) — 19 §14.2.3 同样适用 (D3/D4 修复需经 Axi4CacheAdapter 程序化注入)

### D8: framebuffer 单一真源契约 (P0.5-landing 集成, Oracle 新增)

**决策**: Axi4CacheAdapter 出站 (`AW/AR → req`) 写目标 = `DGpuBoard::framebuffer_` backing (P0.5-landing 单一真源), **禁止**独立 mmap 任何 backing.

**契约条目**:
- **不变量 1**: `Axi4CacheAdapter` 不知道 framebuffer_ 存在; 写目标仅是 `CacheReqBundle.address` 字段, 由 `CrossbarTLM::tick()` 地址路由 (`route_address()` 函数) 决定下游
- **不变量 2**: SDMA `set_vram_backdoor()` (P0.5-landing `bind_memory_backings()` line 580) 已注入 `framebuffer_.data()` — Axi4CacheAdapter 不应重复 mmap
- **不变量 3**: `DGpuBoard::init()` 顺序 Inv-2 扩展: framebuffer 分配 → bind_memory_backings → set_xbar_master(Axi4CacheAdapter) → set_sdma_engine() → set_completion_ring() → 启动 sim_thread_. setter 必须在 framebuffer 分配之后, 否则 SDMA/GMMU 写错 backing
- **不变量 4 (init order assert)**: 在 `DGpuBoard::init()` 末尾加 assert `framebuffer_ptr_ != nullptr` 否则 `runtime_error` ("framebuffer 未分配, Axi4CacheAdapter/SDMA 接线将被拒收")

**理由**:
- P0.5-landing 已 ship framebuffer 单一真源 (per `minimal-dgpu-soc-abi-landing` capability + AGENTS.md KEY INVARIANT)
- 本 change 新增 4 断点修复**不能破坏**这一约定: 否则 SDMA/CP/CR 数据会写到独立 mmap, 与 framebuffer 路径**不同步**
- 4 个断点中 3 个 (SDMA, CompletionRing, Axi4CacheAdapter) 都涉及 framebuffer 数据, **必须复用同一 backing**

**spec 对应 Scenario (新增)**:
- `axi4-cache-adapter-uses-framebuffer-single-source` (framebuffer_ 写入与 BAR1 写入数据一致)
- `dgpu-board-init-order-assert` (framebuffer 未分配时 setter 接线拒收)

---

---

## 4. Risks / Trade-offs (Oracle 修订版)

| Risk | 等级 | 缓解 |
|------|:----:|------|
| ~~`Axi4CacheAdapter` 状态机设计缺陷~~ | ~~🟡 中~~ | **已废 (Oracle 重写)**: transaction_id 直通消除 OOO/conflict/SLVERR 复杂度, 设计复杂度降至 ~80 行 |
| ~~AXI 16-bit ID → 8-bit 压缩冲突~~ | ~~🟡 中~~ | **已废**: `CacheReqBundle.transaction_id` 是 64-bit, 直通无冲突, 不需要压缩 |
| **`Axi4CacheAdapter` 实施错误**: transaction_id 直通错位 / fragment 拆分 bug | 🟡 中 | TDD 5 步先写测试, fragment 多拍 round-trip + 多 outstanding 并发 |
| EP tick() 扩展破坏 Phase 8 M1 既有 4 方向闭环 | 🟡 中 | flag-gated 默认路径 (P0.5-5 setter 默认关闭), `[pcie]` 回归 + test_pcie_endpoint_ip_full_e2e 显式 gate |
| **`D3` API 误引用 `set_host_bypass()`** (Metis 2027-09-17) | 🟡 中 | **已修 (D3 Oracle 修订)**: 用既有 `HostBypassTLM::attach_to_endpoint()` API |
| **`D2` 旧前提 8-bit 压缩** (Metis + Oracle 共识) | 🔴 **高 (历史)** | **已废**: D2 Oracle 重写为 transaction_id 直通, 实施前 P0.5-2 code review 必须确认无压缩逻辑 |
| **JSON 接线被 19 §14.2.3 静默丢弃** | 🔴 **高** | **D7 决策: 程序化桥接强制** (Phase 8 HB/RC 先例), D7 范围扩展含 xbar 下游 |
| **SDMA bundle 类型误标** | 🔴 **高** | **D4 修订: 按真实 bundle 类型 (PcieTlpBundle) 重写接线描述, 新增 2 个 AXI 端口供未来扩展** |
| **SDMA 端口索引错误** | 🟡 中 | **D4 修订: 锁定 header 注释 host_out=3, done_out=4** |
| **MSI-X vector 命名空间冲突** (Oracle) | 🟡 中 | **D3 Oracle 修订: vector 由 EP MsiXTable 统一分配, 桥接代码调用 `ep->allocate_msix_vector()`** |
| **framebuffer 单一真源破坏** (Oracle 新增) | 🔴 **高** | **D8 新增: Axi4CacheAdapter 出站写目标 = framebuffer_, DGpuBoardShell 加 init order assert** |
| **Catch2 标签语义误解** (Metis 2027-09-17) | 🟢 低 | spec Scenario 3 重写: 双标签目的是 G6/G2 floor 数学, 不是"OR 逻辑" |
| **测试基线陈旧** (Metis 2027-09-17) | 🟢 低 | 36,454/155 → 实际 36,598/184 (实测), G6/G7 重算 |

---

## 5. Migration Plan

### 5.1 实施顺序 (Metis + Oracle 2027-09-17 重构: 9 步压缩到 4 commit)

```
Commit 1 (Axi4CacheAdapter, 独立 P0.5-1..4):
  - 实现 Axi4CacheAdapter (D2 重写: transaction_id 直通)
  - chstream_register 注册
  - axi4_stream_adapter 暴露桥接点
  - test_axi4_cache_adapter.cc (transaction_id + fragment + OOO + backpressure)

Commit 2 (EP/HB tick 扩展, P0.5-5..6):
  - PcieEndpointIP: 新增 msix_delivery_in/out 端口, flag-gated 默认关闭
  - HostBypassTLM: 新增 msix_delivery_in 端口, attach_to_endpoint 关联
  - flag-gated: setter 默认关闭, Phase 8 M1 4 方向闭环回归零影响
  - test_pcie_endpoint_ip_msix_path.cc

Commit 3 (SDMA/CR/json, P0.5-7):
  - SdmaEngineTLM: 端口扩展 (axi_slave_in/axi_master_out 占位)
  - CompletionRingTLM: irq_out[3] 目标 EP.msix_delivery_in (vector 分配走 EP)
  - DGpuBoardShell: bind_memory_backings + set_sdma_engine + set_completion_ring + init order assert (D8)
  - dgpu_soc_with_pcie_ip.json: 添加 sdma 模块声明 (无接线声明, per D7)
  - test_pcie_endpoint_ip_sdma_wiring.cc + test_pcie_endpoint_ip_completion_ring_wiring.cc

Commit 4 (跨仓 doc + 验证, P0.5-9):
  - ArchForge 仓: 19-pcie §13 + dgpu-soc-pcie-slice §9.4 (跨仓 PR, 不在本仓)
  - CppTLM 仓: AGENTS.md + test/CMakeLists.txt (若需)
  - 验证: openspec validate --strict + cpptlm_tests "[pcie]" ≥36,598 PASS
```

**为什么 4 commit 而非 9** (Metis + Oracle 共识):
- P0.5-5..7 改同一文件 `pcie_endpoint_ip.cc` 多次, 无法独立回滚 (commit 2+3 合并)
- 9 commit 增加 cherry-pick 复杂度, 降低 PR 可读性
- 4 commit 粒度更合理: 桥(独立) + tick 扩展(独立) + SDMA/CR 集成(独立) + 验证

### 5.2 回滚策略 (flag-gated 默认关闭, Oracle 新增)

**关键纪律**: 所有 P0.5-5..7 新增分支**默认 disable** (编译期 flag 或 runtime config), 仅在 setter 被调时激活. 这样:
- Phase 8 M1 4 方向闭环回归**零影响** (默认路径不变)
- 出问题可单独 disable 一个 setter 验证影响范围

**Flag 设计** (建议):
- `ep.msix_delivery_in` port: 默认 disable (no consumer), 仅 `set_completion_ring()` 后激活
- `Axi4CacheAdapter` setter: 默认 `nullptr` (EP 走旧路径), 仅 `set_xbar_master(adapter)` 后激活
- SDMA setter: 默认 `nullptr` (SDMA 不参与 EP 通路), 仅 `set_sdma_engine()` 后激活

### 5.3 时间线重估 (Metis 修订)

旧估时 "1.5-2 周" **过度乐观**. 修正后:
- Commit 1 (桥, ~80 行 + 测试): 2-3 天 (TDD 5 步严格)
- Commit 2 (EP/HB tick, 最高风险): 3-4 天 (含 flag-gated 实施 + Phase 8 M1 回归)
- Commit 3 (SDMA/CR 集成, 3 模块 + json + init order assert): 3-5 天
- Commit 4 (跨仓 doc + 验证): 1 天 (跨仓 PR 异步)
- **总计**: 9-13 天 ≈ **2-3 周单人**

---

## 6. Open Questions

- **Q1**: `Axi4CacheAdapter` 是否需要支持 burst 拆分? (AXI 4KB burst vs CacheReq 64B 行)
  - **当前答案 (修订)**: **需要 fragment 多拍拆分**. `CacheReqBundle.fragment_id/total[8]` 字段存在, Axi4CacheAdapter 必须把 `awlen[8]+1` 拆分为 `fragment_total` 个 req. Axi4StreamAdapter 的 beat 拆分在它上游, 这里再做一次 fragment 拆分 (per `cache_bundles_tlm.hh` line 67-72 fragment_id 字段)
- **Q2**: MSI-X 端口在 HostBypassTLM 是否需要 sequence number?
  - **当前答案**: 不需要, MSI-X 是 fire-and-forget 单向信号 (per `MsiXDeliveryBundle.trans_id[32]` 已含关联 ID)
- **Q3**: SDMA 16 outstanding 是否足够? 实际驱动可能需要 64+
  - **当前答案**: 16 是 MVP, 后续 P3+ 阶段可扩展. **静态守卫**: `sdma.max_outstanding` ≤ Axi4CacheAdapter capacity, 违反 init 失败
- **Q4** (Metis 新增): 旧 8-bit 冲突用例 (test_axi4_cache_adapter.cc §7+test cases) 是否需删除?
  - **当前答案**: **是**. 重写 D2 后冲突场景不存在, 删除避免锁定错误逻辑
- **Q5** (Metis 新增): D7 范围扩展 (含 xbar 下游) 是否需 connection_resolver 修订配套?
  - **当前答案**: 是, 列为后续 change ("connection_resolver strict 模式, 未解析连接 FAIL 而非 WARN"). 不阻塞本 change

---

## 6.5 Axi4CacheAdapter P0.5-2 设计评审检查项 (GPGPU 侧复用, Oracle 精简版)

> **新增 (Phase 9 P0.5-cpptlm-minimal-dgpu-soc-v1-landing 隐藏建议)**: Axi4CacheAdapter 在 P0.5-2 实施前花 30 分钟做以下检查项. **Oracle 修订**: 删去 YAGNI 项 (第 1/4 项), 仅保留"不依赖 PCIe + 容量参数化 + 错误码 enum"三条.

### 检查项清单 (精简)

| # | 检查项 | 通过条件 |
|---|--------|----------|
| 1 | **不依赖 PCIe 符号** (PcieEndpointIP/TLP 任何符号) | 头文件 `git grep PcieEndpoint\|TLP` 无命中 |
| 2 | **错误码映射通过 enum/class 抽象** (非硬编码 PCI DECERR) | 提供 `AxiErrorCode` enum, 含 OK/DECERR/SLVERR/EXOKAY/UNKNOWN 5 值 |
| 3 | **fragment 拆分支持** (`awlen` → `fragment_total`) | unit test 覆盖 `awlen=3` (4 拍) round-trip |

### 实施检查 (在 P0.5-2 code review 时)

```cpp
class Axi4CacheAdapter : public ChStreamModuleBase {
public:
    enum class AxiErrorCode { OK, DECERR, SLVERR, EXOKAY, UNKNOWN };
    Axi4CacheAdapter(const std::string& n, EventQueue* eq,
                     size_t max_outstanding = 16);  // 与 Axi4Mapper 对齐
    // 不假设 PCIe: 头文件不 include pcie_endpoint_ip.hh / pcie_*.hh
};
```

### 评审结论模板 (P0.5-2 PR 必含)

```markdown
## Axi4CacheAdapter 评审 (per design §6.5 精简版)

- [ ] 不依赖 PCIe 符号 (头文件 git grep 验证)
- [ ] AxiErrorCode enum 抽象 5 值
- [ ] fragment 拆分 unit test 覆盖 awlen=3
```

**通过条件**: 全部 [x] 才允许 P0.5-2 PR merge. YAGNI 项 (PeerKind 参数化 / Bundle visitor) 列为未来 change 备选, 不阻塞当前 PR.

---

## 7. 关联

- **P2 主计划** (ArchForge): `docs/roadmap/phase9-p2-cp-attach-via-axi.md`
- **架构文档** (ArchForge): `docs/architecture/19-pcie-ip-microarchitecture.md` (EP 内部数据流)
- **P2 unblock 阶段文件** (ArchForge): `docs/roadmap/phase9-p2-unblock.md` (本 change 异步创建)
- **dgpu-soc-pcie-slice** (ArchForge): `docs/microarchitecture/dgpu-soc-pcie-slice.md` §9.4 (本 change 异步追加)
- **specs**: [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md)
- **相关 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`
- **P0.5-landing 前置** (archived 2027-09-17): `openspec/changes/archive/2026-09-25-2027-09-17-cpptlm-minimal-dgpu-soc-v1-landing/`
  - 关键依赖: `DGpuBoard::load_soc_config` 自动消费 JSON `framebuffer_size_bytes` (size cap 64GB, override warning, reverse-order guard)
  - **D8 契约**: 本 change 新增 4 断点修复**不能破坏** framebuffer 单一真源
  - 零文件重叠, 可并行推进
- **AGENTS.md**: KEY INVARIANTS (framebuffer 自动分配条目 + OpenSpec Proposed ≤3 KPI)
- **AGENTS.md DOC HYGIENE**: `docs/soc_arch/` 已迁 ArchForge, 本仓禁直接修改. 设计文档变更 → ArchForge 仓 PR (per anti-patterns)
