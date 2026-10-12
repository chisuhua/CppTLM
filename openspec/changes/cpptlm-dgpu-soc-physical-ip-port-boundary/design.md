# Design — cpptlm-dgpu-soc-physical-ip-port-boundary

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/dgpu-soc-physical-ip-port-boundary/spec.md`](specs/dgpu-soc-physical-ip-port-boundary/spec.md)
> **目标 tag**: `cpptlm-dgpu-soc-physical-ip-port-boundary-completion`
> **估时**: **14-22d** (5 模块改造 + 2 adapter + SocInternalNoc ChStream 化 + 回归测试)
> **依赖**: 全部 archived changes 已是 ✅;frozen headers 零 diff + 23 ABI 零变更

---

## 1. 总体设计思路

### 1.1 切型原则 (per 根 AGENTS.md §KEY INVARIANTS P0-revision 2026-10-12)

**SoC 内部 IP 接口按真实 dGPU 物理 IP 边界分层**:

| 物理 IP wire 类型 | 适用范围 | Bundle 类型 |
|----------------|---------|------------|
| **PCIe TLP** | PCIe Controller ↔ host 板级 (PcieEndpointIP ↔ host) | `PcieTlpBundle` (frozen) |
| **NoC flit** | SoC 内部 NoC mesh (SDMA ↔ SocInternalNoc ↔ VramController) | `NoCFlitBundle` (`noc_bundles_tlm.hh:26-50`) |
| **AXI4 / AXI-Lite** | SoC 内部 IP 寄存器访问, non-NoC bulk DMA | `AxiBundle` / `AxiStreamBundle` 系列 |
| **APB-lite / Posted-write** | LWC 总线 (CR-Bus + Doorbell) | `CrBundle` / `DoorbellBundle` (per Change B) |
| **MSI-X bundle** | LWC 总线 (MSI-X 中断) | `MsixBundle` (per Change B + Change E) |

### 1.2 三层 wire-format 转换架构

```
┌─────────────────────────────────────────────────────────────────┐
│ PCIe 板级层 (host ↔ chip)                                       │
│   PcieTlpBundle (frozen, per PcieEndpointIP 17 端口)             │
│   ↕ PcieTlpToPcieTlp 内部桥接 (per Phase 8 M1 既有)             │
├─────────────────────────────────────────────────────────────────┤
│ SoC 内部 NoC 层 (chip-internal)                                  │
│   NoCFlitBundle (16 字段, v2.1 决策, REQ/RESP 统一类型)         │
│   ↕ NoCFlitToAxiCache 转换 (本 change 新增)                     │
├─────────────────────────────────────────────────────────────────┤
│ SoC 内部 IP 协议层 (terminal)                                    │
│   - SdmaEngineTLM: PcieTlpBundle (PCIe 侧) + NoCFlitBundle (NoC 侧) │
│   - VramControllerTLM: NoCFlitBundle (NoC 侧) + CacheReqBundle (内部协议) │
│   - GmmuTLM: NoCFlitBundle (NoC 侧)                              │
│   - GpuInternalFabric: NoCFlitBundle (NoC 侧)                    │
│   - PcieMemoryDevice: PcieTlpBundle (PCIe 侧) + CacheReqBundle (内部协议) │
│   - LwcBusHub: CrBundle + DoorbellBundle + MsixBundle (LWC 侧)   │
└─────────────────────────────────────────────────────────────────┘
```

### 1.3 4 个新 wire-format adapter 矩阵

| Adapter | 输入 Bundle | 输出 Bundle | 应用位置 | 估时 |
|---------|------------|------------|---------|------|
| **`SDMA_PcieTlp_To_NocAdapter`** | PcieTlpBundle (mem_out 出口) | NoCFlitBundle | `sdma.mem_out` → `soc_noc.0` | 1-2d |
| **`SDMA_Noc_To_PcieTlpAdapter`** | NoCFlitBundle (mem_in 入口) | PcieTlpBundle | `soc_noc.Y` → `sdma.mem_in` | 1-2d |
| **`VramCtrl_Noc_To_AxiAdapter`** | NoCFlitBundle (req_in 上游) | CacheReqBundle (内部) | `soc_noc.Z` → `vram_ctrl.req_in_` | 1-2d |
| **`VramCtrl_Axi_To_NocAdapter`** | CacheRespBundle (内部) | NoCFlitBundle (resp_out 上游) | `vram_ctrl.resp_out_` → `soc_noc.W` | 1-2d |

### 1.4 SocInternalNoc + GpuInternalFabric ChStream 化路径

`SocInternalNoc` 改造 (per 当前实装 `include/tlm/cluster/soc_internal_noc.hh:50`):

```cpp
// 改造前 (当前)
class SocInternalNoc : public ::SimModule {
    // 0 ChStreamPort 字段
    // 走 set_*_target() programmatic bridge
    void set_pcie_target(...); void set_sdma_target(...);
    void set_gmmu_target(...);  void set_vram_target(...);
};

// 改造后 (本 change)
class SocInternalNoc : public ::SimModule {
    // N 组 ChStreamPort 字段 (req_in + req_out + resp_in + resp_out)
    cpptlm::InputStreamAdapter<bundles::NoCFlitBundle> req_in_[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle> req_out_[NUM_PORTS];
    cpptlm::InputStreamAdapter<bundles::NoCFlitBundle> resp_in_[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle> resp_out_[NUM_PORTS];

    // 保留 route_flit() 内部 mesh 路由
    void route_flit(NoCFlitBundle& flit);

    // ChStream 化
    void set_stream_adapter(cpptlm::StreamAdapterBase* a) override;
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
    unsigned num_ports() const override { return NUM_PORTS; }
};
```

**关键保留**:
- `route_flit()` mesh 路由逻辑(per e1 v1.1 既有, line 114)
- `vc_counts_[4]` 计数器 (per flit_category 分桶, line 137-140)
- `send_request/response` API (line 143-154)
- programmatic bridge 作为 fallback (per Change C §6 Deviation 4 先例)

---

## 2. 5 个模块端口切型详细设计

### 2.1 `SdmaEngineTLM` 端口切型

**当前** (per `include/tlm/gpu/sdma_engine_tlm.hh:84-99`):
```
0. desc_in   → PcieTlpBundle (保留)
1. mem_in    → PcieTlpBundle (切 NoCFlitBundle)
2. mem_out   → PcieTlpBundle (切 NoCFlitBundle)
3. host_out  → PcieTlpBundle (保留)
4. done_out  → PcieTlpBundle (保留)
```

**改造**:
- 端口 1, 2 切 `NoCFlitBundle`
- 端口 0, 3, 4 保留 `PcieTlpBundle`
- 新增内部转换函数: `pcie_tlp_to_noc_flit()` + `noc_flit_to_pcie_tlp()`
- 注册到 `ChStreamAdapterFactory`: `SDMA_PcieTlp_To_NocAdapter` + `SDMA_Noc_To_PcieTlpAdapter`

**JSON 拓扑变化**:
```json
// 改造前 (current)
{ "src": "sdma.2", "dst": "pcie_memory.0", "latency": 1 }

// 改造后 (target)
{ "src": "sdma.2", "dst": "soc_noc.0", "latency": 1 }   // 走 NoC 直连
{ "src": "soc_noc.X", "dst": "vram_ctrl.0", "latency": 1 }  // 走 NoC 直连
```

### 2.2 `VramControllerTLM` 端口切型

**当前** (per `include/tlm/vram_controller_tlm.hh:58`):
```
继承 MemoryTLM, 1 组 ChStreamPort (req_in_/req_out_/resp_in_/resp_out_), CacheReqBundle/CacheRespBundle 类型
```

**改造**:
- `req_in_/req_out_/resp_in_/resp_out_` 全部改 `NoCFlitBundle`(上游 NoC 化)
- 内部 `req_internal/resp_internal` 保留 `CacheReqBundle/CacheRespBundle`(MemoryTLM 协议真实)
- 新增内部转换: `noc_flit_to_cache_req()` + `cache_resp_to_noc_flit()`

**关键保留**:
- `handle_functional_request()` API (per Change A)
- `set_backing_view()` (per ADR-DGPU-10)
- `set_mode(FunctionalZeroDelay)` (per ADR-DGPU-11)

### 2.3 `GpuInternalFabric` ChStream 化

**当前** (per `include/tlm/cluster/gpu_internal_fabric.hh:37`):
```cpp
class GpuInternalFabric : public SimModule {
    unsigned num_ports_ = 4;
    void set_vram_target(VramControllerTLM* target) noexcept;
    void set_fabric_source(VramControllerTLM* src) noexcept;
    // 0 ChStreamPort 字段, 走 callback
};
```

**改造**:
- 加 4 组 ChStreamPort 字段 (req_in + req_out + resp_in + resp_out)
- `Fabric2VramCtrlBundle` 8 字段**保留**(内部 IP-间 wire)
- 取消 `set_vram_target/set_fabric_source` programmatic bridge
- `set_stream_adapter()` override + `num_ports()` override

### 2.4 `SocInternalNoc` ChStream 化

**当前** (per `include/tlm/cluster/soc_internal_noc.hh:50`):
```cpp
class SocInternalNoc : public ::SimModule {
    // 0 ChStreamPort 字段
    // 走 set_*_target() programmatic bridge
    void set_pcie_target(...); void set_sdma_target(...);
    void set_gmmu_target(...);  void set_vram_target(...);
    void route_flit(NoCFlitBundle& flit);  // 内部 mesh 路由
    void send_request(NoCFlitBundle& flit);  // 内部 API
    void send_response(NoCFlitBundle& flit);
    uint64_t vc_counts_[4];  // VC 计数器
    uint64_t flits_dispatched_;  // flit 计数
};
```

**改造**:
- 加 N 组 ChStreamPort 字段 (NoCFlitBundle 类型)
- 保留 `route_flit/send_request/send_response` 内部 API
- 保留 `vc_counts_[4]` + `flits_dispatched_` 计数
- 取消 4 个 `set_*_target` programmatic bridge
- JSON `connections[]` 可写 `sdma.2 → soc_noc.0` 等

### 2.5 `GmmuTLM` 端口切型

**当前** (per `include/tlm/gpu/gmmu_tlm.hh`):
```
- translate() 函数 (per Change C T2)
- req_out_ (PTE fetch, PcieTlpBundle)
- resp_in_ (PTE fetch 响应, PcieTlpBundle)
- mem_view_ (programmatic, timing-mode H-C6)
- translate_target_ (programmatic, Gap G1 修复)
```

**改造**:
- `req_out_` 改 `NoCFlitBundle`
- `resp_in_` 改 `NoCFlitBundle`
- 保留 `mem_view_/translate_timing` (timing-mode H-C6)
- 保留 `translate_target_` (Gap G1 修复后)

---

## 3. 4 个新 wire-format adapter 详细设计

### 3.1 `SDMA_PcieTlp_To_NocAdapter` (1-2d)

**输入**: `PcieTlpBundle` (SDMA 发出的 mem_write / mem_read)
**输出**: `NoCFlitBundle` (推给 SocInternalNoc)
**字段映射**:
```cpp
NoCFlitBundle convert(const PcieTlpBundle& pcie) {
    NoCFlitBundle noc;
    noc.transaction_id = pcie.tag;        // PCIe tag → NoC transaction_id
    noc.src_node = SDMA_NODE_ID;          // 硬编码 SDMA 节点号
    noc.dst_node = (pcie.addr >= VRAM_BASE) ? VRAM_NODE_ID : PCIE_NODE_ID;  // 路由决策
    noc.address = pcie.addr;              // 直接透传
    noc.data = pcie.data;                 // 64-bit payload 透传
    noc.size = pcie.length_bytes;         // 透传
    noc.vc_id = 0;                        // REQ → VC0
    noc.flit_type = (pcie.length_bytes > 8) ? NoCFlitBundle::FLIT_HEAD_TAIL : NoCFlitBundle::FLIT_HEAD;
    noc.flit_index = 0;
    noc.flit_count = (pcie.length_bytes + 7) / 8;  // 8-byte flit 对齐
    noc.hops = 0;
    noc.flit_category = NoCFlitBundle::CATEGORY_REQUEST;  // REQ
    noc.src_port = 0;
    noc.is_write = (pcie.kind == PcieTlpBundle::MEM_WRITE);
    noc.error_code = 0;
    return noc;
}
```

### 3.2 `SDMA_Noc_To_PcieTlpAdapter` (1-2d)

**输入**: `NoCFlitBundle` (从 SocInternalNoc 收到 VRAM 读响应)
**输出**: `PcieTlpBundle` (D2H 路径 mem_in 入口)
**字段映射**:
```cpp
PcieTlpBundle convert(const NoCFlitBundle& noc) {
    PcieTlpBundle pcie;
    pcie.tag = noc.transaction_id;        // NoC transaction_id → PCIe tag
    pcie.addr = noc.address;              // 透传
    pcie.data = noc.data;                 // 64-bit payload 透传
    pcie.length_bytes = noc.size;
    pcie.kind = (noc.flit_category == NoCFlitBundle::CATEGORY_RESPONSE)
        ? PcieTlpBundle::MEM_READ_RESP : PcieTlpBundle::MEM_WRITE_RESP;
    return pcie;
}
```

### 3.3 `VramCtrl_Noc_To_AxiAdapter` (1-2d)

**输入**: `NoCFlitBundle` (从 SocInternalNoc 收到 SDMA / GMMU / fabric 请求)
**输出**: `CacheReqBundle` (内部 MemoryTLM 协议)
**字段映射**:
```cpp
CacheReqBundle convert(const NoCFlitBundle& noc) {
    CacheReqBundle cache;
    cache.addr = noc.address;
    cache.data = static_cast<uint32_t>(noc.data & 0xFFFFFFFF);
    cache.vc_id = noc.vc_id;
    cache.is_write = noc.is_write;
    cache.size = noc.size;
    return cache;
}
```

### 3.4 `VramCtrl_Axi_To_NocAdapter` (1-2d)

**输入**: `CacheRespBundle` (MemoryTLM 协议响应)
**输出**: `NoCFlitBundle` (推回 SocInternalNoc)
**字段映射**:
```cpp
NoCFlitBundle convert(const CacheRespBundle& cache) {
    NoCFlitBundle noc;
    noc.transaction_id = cache.transaction_id;
    noc.address = cache.addr;
    noc.data = cache.data;
    noc.size = cache.size;
    noc.flit_category = NoCFlitBundle::CATEGORY_RESPONSE;  // RESP
    noc.vc_id = 1;                                          // RSP → VC1
    noc.is_ok = cache.hit;
    noc.error_code = cache.hit ? 0 : 1;
    return noc;
}
```

---

## 4. JSON 拓扑变化

### 4.1 改造前 (per `configs/dgpu_soc_minimal_v1.json:80-82`)

```json
"connections": [
  { "src": "sdma.2", "dst": "pcie_memory.0", "latency": 1 }
]
```

### 4.2 改造后

```json
"connections": [
  { "src": "sdma.2",       "dst": "soc_noc.0",   "latency": 1 },   // SDMA → NoC
  { "src": "soc_noc.1",    "dst": "vram_ctrl.0", "latency": 1 },   // NoC → VRAM
  { "src": "fabric.0",     "dst": "soc_noc.2",   "latency": 1 },   // fabric → NoC
  { "src": "soc_noc.3",    "dst": "gmmu.0",      "latency": 1 }    // NoC → GMMU (GMMU dedicated client)
]
```

**说明**:
- 4 条 connection 替代 1 条 + 4 处 programmatic bridge
- JSON 拓扑**真正**反映完整 wire 拓扑
- 与 `dgpu_board_v1.json:80-94` (SimModule 内部 connection) 模式一致

### 4.3 programmatic bridge 退役 (per `dgpu_board_shell.cc:840-852`)

- `soc_noc->set_pcie_target(...)` 取消
- `soc_noc->set_sdma_target(...)` 取消
- `soc_noc->set_gmmu_target(...)` 取消
- `soc_noc->set_vram_target(...)` 取消
- `fabric->set_vram_target(...)` 取消
- `vram_ctrl->set_fabric_source(...)` 取消

`bind_memory_backings()` 装配从 ~30 行简化到 ~5 行。

---

## 5. 测试覆盖

### 5.1 新增测试 (估时 3-5d)

| 标签 | 数量 | 内容 |
|------|------|------|
| `[port-typing]` | 6 cases | 4 个 adapter 双向 (8 个单测) + 1 个 E2E 直连 + 1 个回滚验证 |
| `[soc_noc][chstream]` | 4 cases | SocInternalNoc ChStream 化 (4 端口 wire) |
| `[gpu_fabric][chstream]` | 4 cases | GpuInternalFabric ChStream 化 |
| `[sdma][noc-bridge]` | 3 cases | SDMA 端口切型 + adapter 注入 |
| `[vram][noc-bridge]` | 3 cases | VramController 端口切型 + adapter 注入 |
| `[gmmu][noc-bridge]` | 2 cases | GmmuTLM 端口切型 |

### 5.2 回归测试 (估时 2-3d)

| 标签 | 基线 | 目标 |
|------|------|------|
| `[pcie]` | 430/36672 | 0 回归 |
| `[gmmu]` | 25/5273 | 0 回归 |
| `[noc]` | 64/304 | 0 回归 |
| `[sdma]` | 68/12945 | 0 回归 |
| `[minimal_dgpu_soc]` | 24/170 | 0 回归 + E2E 直连 case |
| `[dgpu_soc_timing]` | 37/12024 | 0 回归 (timing-mode 不动) |
| `[driver_visible]` | 4/40 | 0 回归 |
| `[pcie-memory]` | 17/48 | 0 回归 |

### 5.3 测试方法

- **TDD 5-step**: RED → GREEN → E2E → Notes → archive
- **per Change C §6 Deviation 4 先例**: 实施期保留 programmatic bridge fallback
- **frozen headers 0 diff + 23 ABI 0 变更**: 严格遵守 (per ADR-DGPU-08)

---

## 6. Deviation 预案 (per Change C §6 先例)

预期会出现的 deviation(参照 Change C §6 4 处 deviation 经验):

1. **`SdmaEngineTLM::NUM_PORTS = 5` 不变, 端口类型 3 变 2**: JSON 拓扑 `sdma.X` 索引保持,但 Bundle 类型变更;StreamAdapter 注入需重写
2. **`SocInternalNoc` ChStream 化后, programmatic bridge 保留作 fallback**: 实施期保留 4 个 set_*_target 方法,作为 `[pcie-memory]` characterization tests 兜底
3. **`GpuInternalFabric` ChStream 化后, `Fabric2VramCtrlBundle` 8 字段保留**: 内部 IP-间 wire 仍用 Fabric2VramCtrlBundle, 不动 BUNDLE_FIELD_WIDTH 约束
4. **`VramControllerTLM` timing-mode 路径不动**: 仅 functional-mode 切型, timing-mode 保持 `set_mode(TimingCycleApproximate)` + 当前 wire-format (per H-C6)
5. **`GmmuTLM` programmatic bridge 保留**: `mem_view_/translate_timing` (timing-mode) + `translate_target_` (Gap G1 修复) 不动, 只切 `req_out_/resp_in_` PTE fetch 端口

---

## 7. 实施期与归档

### 7.1 实施期

- **TDD 5-step** (per `docs/superpowers/plans/` P2-P5 经验)
- **拆 5 sub-task** (per tasks.md):
  - T1: SDMA 端口切型 (3-4d)
  - T2: VramController 端口切型 (3-4d)
  - T3: GpuInternalFabric ChStream 化 (2-3d)
  - T4: SocInternalNoc ChStream 化 (3-4d)
  - T5: GmmuTLM 端口切型 (1-2d)
  - T6: 4 个新 adapter 实施 (3-5d, 与 T1-T5 并行)
  - T7: 0 回归锁 + E2E 直连 case (2-3d)
  - T8: 实施笔记 + post-flight 验证 (1d)

### 7.2 归档

- spec 落盘到 `openspec/specs/dgpu-soc-physical-ip-port-boundary/spec.md` (本 change 已起草, 5 ADDED Requirements)
- 同步 spec 修改: `sdma-engine-tlm` / `vram-controller-functional-mode` / `gmmu-pte-wf` / `soc-internal-noc`
- 提交 archive 命令: `openspec archive cpptlm-dgpu-soc-physical-ip-port-boundary`
- 跟踪 proposal.md 从 index 移除
- KPI: 1 slot 释放

---

## 8. 不在范围内 (per proposal.md "What Changes" 末)

- 23 ABI 函数: 零 diff
- 4 frozen headers: 零 diff
- PcieEndpointIP 17 端口: 保留
- PCIe Controller ↔ host 边界: 保留 PcieTlpBundle
- timing-mode (dgpu_soc_timing_v1.json): 不动
- LWC 总线 (CrBundle / DoorbellBundle / MsixBundle): 不动 (per Change B + Change E)
- PCI 板级 PcieEndpointIP ↔ host 通信: 不动
