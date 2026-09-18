# cpptlm-p2-integration-unblock: Design (4 断点修复详细实现路径)

> **配套**: [`proposal.md`](proposal.md) · [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md) · [`tasks.md`](tasks.md)
> **目标**: Phase 9 P2 (CP 跨仓接入) 启动的 4 个集成断点修复
> **关联阶段**: `docs/soc_arch/roadmap/phase9-p2-unblock.md` (本 change 创建)

---

## 1. Context

### 1.1 当前状态 (Phase 8 整合遗留)

Phase 8 M1 整合交付完成了 `PcieEndpointIP` 与 SoC 的基础接线 (4 方向 AXI 主从框架)，但实际数据流存在 4 个断点：

```
PcieEndpointIP
  │ internal PcieAxiAdapter + Axi4StreamAdapter
  ├── axi_master_out  ────► xbar.0 (单方向, CacheReqBundle)
  ├── axi_slave_in   ◄──── (HB/RC 端程序化)
  ├── slave_resp     ────► (HB/RC 端程序化)
  ├── cfg_slave_in   ◄──── (未使用)
  └── MsiXTable::irq_out ──► ❌ 无任何外部连接

SdmaEngineTLM    ────► ❌ 不在 dgpu_soc_with_pcie_ip.json
CompletionRing   ────► ❌ irq_out[3] 未接线
```

### 1.2 协议不兼容核心问题

`PcieAxiAdapter` 用 `Axi4Bundle` (Phase 5 标准化):
- `awaddr`, `awid`, `awlen`, `awsize`, `awburst`, `wdata`, `wstrb`, `wlast`, `bid`, `bresp`, `araddr`, `arid`, `arlen`, `rdata`, `rresp`, `rlast`
- **16-bit ID 空间** (`awid`/`arid`/`bid`/`rid`) 支持 OOO completion

`CrossbarTLM` 用 `CacheReqBundle`/`CacheRespBundle`:
- `req_out`: addr, cmd (RD/WR), data, byte_en, src_id
- `resp_in`: data, status, dst_id

**协议不兼容原因**:
- CacheReqBundle 无 `awid`/`bid`/`rid` 等 OOO 字段
- CacheRespBundle 无 `rresp` (AXI 错误码) / `rlast` (burst 终止)
- 地址语义不同: AXI64 字节地址 vs Cache 行地址

### 1.3 4 方向 AXI 流量

| 方向 | 路径 | 当前状态 |
|------|------|---------|
| **D1**: Host→EP 写/读 | HB→`axi_slave_in` | ✅ 程序化闭环 |
| **D2**: EP→Host 响应 | `slave_resp`→HB | ✅ 程序化闭环 |
| **D3**: EP→SoC 写 | `axi_master_out`→xbar | ⚠️ 单向连通, 无响应 |
| **D4**: EP→SoC 读响应 | xbar→`master_resp` | ❌ BROKEN |
| **MSI-X**: EP→Host | MsiXTable::irq_out | ❌ 无连接 |
| **SDMA**: EP→SDMA | BAR doorbell→SDMA | ❌ 不在 JSON |
| **CompletionRing**: CR→EP | irq_out[3]→? | ❌ 未接线 |

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
- ❌ 扩展 CrossbarTLM 支持 Axi4Bundle: 改动范围大, 影响其他 155 个 `[chstream]` 测试
- ❌ 协议层统一为 CacheReq/Resp: 破坏 AXI 标准化, 影响 Phase 5-6 已交付接口

### D2: Axi4CacheAdapter ID 映射策略 — **2 字节 → 1 字节压缩 + Tagging**

**决策**:
- AXI 16-bit `awid`/`arid` → CacheReq 8-bit `src_id` (压缩高 8 bit, 低 8 bit 作为 `ax_id` 保留)
- AXI 16-bit `bid`/`rid` → CacheResp 8-bit `dst_id` + 4-bit `ax_id` 用于 OOO 匹配
- **Tagging 策略**: 维护 `awid_to_src_id_` 和 `src_id_to_awid_` 双向映射表, capacity 256

**理由**:
- CacheReqBundle 限制 `src_id`/`dst_id` 为 8-bit (per `cpphdl_types.hh`)
- AXI 16-bit ID 空间太大, 压缩不可避免
- 8-bit `src_id` 足够 PCIe EP 使用 (单 EP 不会同时发出 >256 写请求)

**考虑过的方案**:
- ❌ 扩展 CacheReqBundle 为 16-bit: 破坏现有 `[chstream]` 测试
- ❌ 完全 OOO 处理: Phase 6 `Axi4Mapper` 已实现, Axi4CacheAdapter 仅做协议转换不处理 OOO

### D3: MSI-X 投递路径 — **EP 双端口 + HB 单 ingress 端口**

**决策 (Oracle 修订版)**:
- `PcieEndpointIP` 新增 **2 个独立端口**:
  - `msix_delivery_in` (ingress, MsiXDeliveryBundle) — 接收来自 SDMA `done_out[4]` / CompletionRing `irq_out[3]` 的 MSI-X 事件
  - `msix_delivery_out` (egress, MsiXDeliveryBundle) — 主动 push 到 HostBypassTLM
- `HostBypassTLM` 新增 1 个 ingress 端口 `msix_delivery_in` (MsiXDeliveryBundle), 由 PcieEndpointIP tick() 经程序化桥接推送

**理由 (修订版)**:
- **数据流方向**: EP 内部聚合 (in) → EP 转发 (out) → HB 接收 (in), 严格单向
- **协议简单**: MsiXDeliveryBundle {vector, msg_addr, msg_data} 无需握手
- **不污染 AXI 路径**: 单独端口避免与 AXI 流混淆
- **EP 内部聚合**: SDMA done + CompletionRing irq 共用 EP→HB 通道, 减少端口爆炸
- **可独立测试**: 单独 ingress 便于 `[pcie-ep-soc-bridge]` 标签测试

**考虑过的方案**:
- ❌ 通过 `axi_slave_in` 复用: 协议不同 (MSI-X 是中断信号, AXI 是事务), 混淆语义
- ❌ 新增专门 MSI-X 控制器模块: 过度设计, HostBypassTLM 已足够
- ❌ 单一 EP 端口 (in 或 out 二选一): 无法聚合 SDMA + CompletionRing 两路来源

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

**决策**: 本 change 内 SDMA + MSI-X + CompletionRing **全部走程序化桥接**, 不依赖 JSON 声明式 EP 外层端口接线

**理由 (per Oracle 2027-09-17)**:
- **19 §14.2.3 已锁定**: `pcie_ep.axi_slave_in/axi_master_out` 声明式接线在 connection_resolver 两层下钻时**被静默丢弃** (test_axislavein_bridge_path_intact 锁定该行为), 且 19 明确"未来扩展属独立 change"
- **G5 风险**: 若强行 JSON 接线, `validate_topology` 可能产生 WARN 而非 FAIL, 实际数据流未建立
- **唯一可靠路径**: 程序化桥接 (Phase 8 HB/RC tick 转发先例), EP.set_sdma_engine() setter + tick() 内部转发
- **未来扩展路径**: 如需 JSON 声明式, 需先修订 19 §14.2.3 + 解锁 test_axislavein_bridge_path_intact (独立 change)

**JSON 在本 change 内的合法用途**:
- 添加 sdma 模块声明 (name, type, params)
- 添加 hb/ep/rc 顶层模块连接 (hb.axi_master ↔ ep.axi_slave_in 在 Phase 8 M1 已程序化闭环)
- 添加 msix_delivery 程序化桥接的 metadata 注释 (便于阅读)

**JSON 在本 change 内的禁用范围**:
- `ep.axi_master_out[N]` 任何目标 (xbar, sdma, vram0) — 静默丢弃
- `ep.axi_slave_in[N]` 任何源 (host_bypass 已程序化) — 同上
- `ep.msix_delivery_in/out` 任何连接 — 走 EP.set_host_bypass() / EP.set_sdma_engine() setter

---

## 4. Risks / Trade-offs (Oracle 修订版)

| Risk | 等级 | 缓解 |
|------|:----:|------|
| `Axi4CacheAdapter` 状态机设计缺陷 | 🟡 中 | TDD 5 步先写测试, 包含 OOO/error/timeout 场景 |
| AXI 16-bit ID → 8-bit 压缩冲突 | 🟡 中 | D2 决策: 高 8 bit 进 src_id, 低 8 bit 保留, 双向映射表 capacity 256 |
| EP tick() 扩展破坏 Phase 8 M1 既有 4 方向闭环 | 🟡 中 | `[pcie]` 回归测试覆盖, 36,454 assertions 基线 |
| **JSON 接线被 19 §14.2.3 静默丢弃** (Oracle blocker) | 🔴 **高** | **D7 决策: 程序化桥接强制** (Phase 8 HB/RC 先例), 不依赖 JSON 声明式 EP 外层端口接线 |
| **SDMA bundle 类型误标** (Oracle blocker) | 🔴 **高** | **D4 修订: 按真实 bundle 类型 (PcieTlpBundle) 重写接线描述, 新增 2 个 AXI 端口供未来扩展** |
| **SDMA 端口索引错误** (design D4 host_out[0]/done_out[0]) | 🟡 中 | **D4 修订: 锁定 header 注释 host_out=3, done_out=4** |
| **MSI-X 端口命名混乱** (tasks 5.1.1 out vs design in vs spec 方向) | 🟡 中 | **D3 修订: EP 双端口 msix_delivery_in (←SDMA/CR) + msix_delivery_out (→HB)** |
| **Catch2 标签不嵌套导致 G6 失效** | 🟡 中 | **D6 修订: 4 新测试文件双标签 `[pcie][pcie-ep-soc-bridge]`, G9 验证** |
| CompletionRing irq_out[3] header 注释说"→pcie_ep.irq_out" | 🟢 低 | D5 决策: 同步 header 注释改为"→pcie_ep.msix_delivery_in", P0.5-7 包含 |
| `[pcie-ep-soc-bridge]` 标签 0 命中基线 | 🟢 低 | 4 个新测试文件, 至少 30 assertions, G9 双标签保 G6 数学 |

---

## 5. Migration Plan

### 5.1 实施顺序 (TDD 5 步, 9 步对应 P0.5-1..P0.5-9)

```
P0.5-1: 写 test_axi4_cache_adapter.cc (TDD 红)
  ↓
P0.5-2: 实现 include/framework/axi4_cache_adapter.hh + .cc
  ↓
P0.5-3: 验证 Axi4CacheAdapter 单测 PASS (TDD 绿)
  ↓
P0.5-4: 修改 Axi4StreamAdapter 暴露桥接点
  ↓
P0.5-5: 修改 PcieEndpointIP::tick() 补 D3/D4 + MSI-X 投递
  ↓
P0.5-6: 修改 HostBypassTLM 新增 msix_delivery_in 端口
  ↓
P0.5-7: 修改 dgpu_soc_with_pcie_ip.json (sdma + msix + completion_ring)
  ↓
P0.5-8: 写 3 个 E2E 测试 (msix + sdma + completion_ring)
  ↓
P0.5-9: 全量回归 + openspec validate --strict
```

### 5.2 回滚策略

每个 P0.5 步骤独立 commit, 出问题可单独回滚:
- P0.5-1..3: 仅新增 Axi4CacheAdapter, 既有代码无影响
- P0.5-4: 既有代码兼容性扩展, 可独立回滚
- P0.5-5..6: EP/HB 扩展, 既有 `[pcie]` 回归测试保护
- P0.5-7: JSON 配置, `validate_topology` 立即验证
- P0.5-8..9: 测试和回归, 失败可整体跳过本 change

---

## 6. Open Questions

- **Q1**: `Axi4CacheAdapter` 是否需要支持 burst 拆分? (AXI 4KB burst vs CacheReq 64B 行)
  - **当前答案**: 不需要, Axi4StreamAdapter 已处理 burst 拆分为 beat
- **Q2**: MSI-X 端口在 HostBypassTLM 是否需要 sequence number?
  - **当前答案**: 不需要, MSI-X 是 fire-and-forget 单向信号
- **Q3**: SDMA 16 outstanding 是否足够? 实际驱动可能需要 64+
  - **当前答案**: 16 是 MVP, 后续 P3+ 阶段可扩展

---

## 7. 关联

- **P2 主计划**: `docs/soc_arch/roadmap/phase9-p2-cp-attach-via-axi.md`
- **架构文档**: `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` (EP 内部数据流)
- **P2 unblock 阶段文件**: `docs/soc_arch/roadmap/phase9-p2-unblock.md` (P0.5-1 创建)
- **specs**: [`specs/pcie-ep-soc-noc-axi-bridge/spec.md`](specs/pcie-ep-soc-noc-axi-bridge/spec.md)
- **相关 spec**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`
