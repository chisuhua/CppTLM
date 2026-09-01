# cpptlm-dgpu-pcie-ip-microarch: 实施路线图 (7 Phases × OpenSpec Changes)

> **配套**: [`proposal.md`](./proposal.md) · [`design.md`](./design.md) · [`decisions.md`](./decisions.md)
> **状态**: 📅 Planned — 2026-09-01
> **原则**: Oracle 建议"先 FC 后 LL"(backpressure 必须先就绪),修订了原"先 LL 后 FC"路线

## 总览

```
                    7 阶段实施甘特图(Oracle 修订 #6, 2026-09-01)
   ═══════════════════════════════════════════════════════════════
   Phase 1: Link Layer + FC Token Bucket      ████████░░░░░░░░  W1-W4
   Phase 2: 128b/130b Encoding (延迟建模)      ░░░░████░░░░░░░░  W5-W6
   Phase 3: PHY Digital Ctrl + Bypass Mux      ░░░░░░░████████░  W5-W8(并行 P2)
   Phase 4: SR-IOV VF Pool                    ░░░░░░░░░░░████░  W9-W12
   Phase 5: AXI Stream Adapter                ███░░░░░░░░░░░░░  W1-W3(并行 P1)
   Phase 6: AXI4Mapper 独立模块               ░░░░░░░░░░░░░██░  W13-W16
   Phase 7: Host Bypass + RC                  ░░░░░░░░░░░██░██  W17-W19(3 周工作量)
                                                 ░░░░░░░░░██░  W20-W22 集成测试
                                                 ░░░░░░░██░░  W23-W24 缓冲 + Bug 修复
   ═══════════════════════════════════════════════════════════════
   总工期: 24 周日历 = ~6 个月

   ⚠️ Oracle 修订(#6):
   1. 单人全职:Phase 工作量加总 = 4+2+4+4+3+4+3 = 24 人周,与日历 24 周相等(零缓冲)
   2. 图中 "P1+P5 并行" / "P2+P3 并行" 为**依赖独立性声明**(无依赖),非**实际并发执行**
   3. 实际单人顺序执行: P1 → P5 → P2 → P3 → P4 → P6 → P7 (依赖图顺序)
   4. P7 跨 W17-W24 是 8 周日历,实际工作量 3 周 + 5 周集成/缓冲
   5. **建议**:若时间紧,优先级 P1 → P3 → P4 → P6 → P5 → P2 → P7(MVP 优先)
```

## 阶段依赖图(Oracle 修订 #6)

```
    P1(LL+FC)  ─────┬────────────► P3(PHY) ────► P4(SR-IOV) ──┐
                    │                                  │        │
                    └────► P2(130b) ──────────────────┘        │
                                                               │
    P5(AXI Adapter)  ────────────────────────────────────► P6(AXI4Mapper)
                                                               │
                                                               ▼
                                                          P7(Host Bypass + RC)
                                                          (依赖 P5 + P3/P4)
```

**关键依赖**:
- **P1 → 全部后续阶段**: Link Layer 是 FC + 链路层控制的基础
- **P5 可与 P1 并行**(依赖独立性): AXI Adapter 不依赖 PCIe 内部细节
- **P6 依赖 P5**: AXI4Mapper 在 AXI StreamAdapter 之上
- **P7 依赖 P5**(必须)+ **P3/P4**(可选,若要完整 dGPU PCIe EP):
  - `HostBypassTLM` 桥接 AXI 接口(依赖 P5)
  - `PcieRootComplexTLM` 镜像 PcieEndpointIP(可选,需 P1-P4)

---

## Phase 1: Link Layer + Flow Control Token Bucket

### 元数据

| 项 | 值 |
|---|---|
| **OpenSpec Change 名** | `cpptlm-dgpu-pcie-link-layer-and-fc` |
| **Change folder** | `openspec/changes/2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/` |
| **工期** | 4 周(W1-W4) |
| **优先级** | 🔴 P0(所有后续阶段基础) |
| **Owner** | Sisyphus |
| **依赖** | 无(立即可启动) |
| **被依赖** | P2, P3, P4, P7 |

### Why

当前 `PcieEndpointTLM`(2026-08-26 archive)的 Link Layer + Flow Control 全部跳过(Board shell 注入 TLP 直接绕过)。这导致:
- **无 FC 反压**: Host 可以无限制推送 TLP,可能淹没 SoC 模块
- **无 ACK/NAK**: 无法模拟链路错误与重传
- **无 DLLP 协议**: 链路层协议完全缺失

Oracle 修订路线图:**先 FC 后 LL** —— Flow Control Token Bucket 是 backpressure 的基础,所有后续阶段(130b 编码 / PHY 数字 / SR-IOV / AXI 适配)都依赖它。

### What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_link_layer_tlm.hh` + `src/tlm/pcie/pcie_link_layer_tlm.cc` | **新**: PcieLinkLayer(模块化,可独立测试) |
| `include/tlm/pcie/pcie_flow_control_token_bucket.hh` + `.cc` | **新**: FcTokenBucket 类 |
| `include/bundles/pcie_dllp_bundles_tlm.hh` | **新**: PcieDllpBundle + PciePhyConfig |
| `src/tlm/gpu/pcie_endpoint_tlm.cc` | **改**: 集成 PcieLinkLayer(internal composition) |
| `test/test_pcie_link_layer_dllp.cc` | **新**: DLLP gen/parse 测试 |
| `test/test_pcie_link_layer_fc_token_bucket.cc` | **新**: FC Token Bucket 单元测试(无 refill,per Q2) |
| `test/test_pcie_link_layer_ack_nak.cc` | **新**: ACK/NAK 重传测试(累积确认) |
| `test/test_pcie_link_layer_error_injector.cc` | **新**: Q15 错误注入接口测试 |
| `test/test_pcie_link_layer_downstream_rx.cc` | **新**: Q17 下行(host→EP)Rx 链路层测试 |
| `test/test_pcie_endpoint_with_ll.cc` | **新**: PcieEndpointTLM + LL 集成测试 |
| `include/tlm/gpu/pcie_endpoint_tlm.h` | ❌ **不改**(Oracle Top-9: deprecated 推迟到 Phase 4 PcieEndpointIP 可用后;Phase 1 只锁 ABI 边界) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | **新**: 加 `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏(无 `#error`,per Oracle Top-1;项目约定 .hh 后缀,AGENTS.md) |
| `CMakeLists.txt` (root) | **改**: 注册新源文件 |

### 数据流(本 phase)

```
                     P1 Phase 内部数据流
   ═══════════════════════════════════════════════════════

   [Board shell / 未来 RC BFM]
       │ 注入 TLP
       ▼
   ┌─────────────────────────────────────────────┐
   │  PcieEndpointTLM (扩展)                      │
   │                                              │
   │   slave_in (PcieTlpBundle)                   │
   │       │                                      │
   │       ▼                                      │
   │   ┌──────────────────┐                       │
   │   │ Tx Path (新增)   │                       │
   │   │ 排序 + Tx Retry  │ ◄── DLLP ACK/NAK 反馈 │
   │   └──────┬───────────┘                       │
   │          │                                   │
   │          ▼                                   │
   │   ┌──────────────────┐                       │
   │   │ PcieLinkLayer    │ ◄── NEW 模块          │
   │   │  - DLLP gen/parse│                       │
   │   │  - ACK/NAK       │                       │
   │   │  - Retry Buffer  │                       │
   │   │  - 12-bit Seq #  │                       │
   │   │  - DLLP dispatch │                       │
   │   └──────┬───────────┘                       │
   │          │                                   │
   │          │ TLP / DLLP                        │
   │          ▼                                   │
   │   ┌──────────────────┐                       │
    │   │ FcTokenBucket    │ ◄── NEW 类            │
    │   │  - P bucket      │                       │
    │   │  - NP bucket     │                       │
    │   │  - Cpl bucket    │                       │
    │   │  - consume       │ (credit 单调非减,    │
    │   │  - update        │  仅 UpdateFC 补充,   │
    │   │  - EventQueue*   │  无自动 refill,      │
    │   │  (per Q2 修订)   │  per PCIe FC spec)   │
    │   └──────────────────┘                       │
   │                                              │
   │   mem_out / mmio_out (PcieTlpBundle)  ◄── 现有 │
   │   irq_out (MsiXDeliveryBundle)        ◄── 现有 │
   └─────────────────────────────────────────────┘
```

### Acceptance Gate

| Gate | 验证方法 |
|---|---|
| **P1-G1** `cmake --build build` 通过 + 新增 4 个测试文件 PASS | `ctest --output-on-failure -R "pcie_link_layer\|pcie_endpoint_with_ll"` |
| **P1-G2** DLLP gen/parse 单元测试 PASS(ACK/NAK/InitFC/UpdateFC/NOP) | `ctest -R test_pcie_link_layer_dllp` |
| **P1-G3** FC Token Bucket 单元测试 PASS(consume/update/over-consume/单调性/per-VF 隔离;**无 refill 测试**,per Q2 修订) | `ctest -R test_pcie_link_layer_fc_token_bucket` |
| **P1-G4** ACK/NAK 重传场景 PASS(模拟丢包 + 重传验证) | `ctest -R test_pcie_link_layer_ack_nak` |
| **P1-G5** 集成测试 PASS(LL 在 PcieEndpointTLM 内正常工作,无回归) | `ctest -R test_pcie_endpoint_with_ll` |
| **P1-G6** 23 ABI 边界:`CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏(新类文件)+ 类方法 `noexcept`/`[[nodiscard]]` 纪律(不修改旧类,per Oracle Top-1/Top-9) | 静态检查 |
| **P1-G7** 全量 `build/bin/cpptlm_tests` 无回归(拼写修正,per Oracle) | `ctest -j$(nproc)` PASS |

### Risk & Mitigation

| Risk | Mitigation |
|---|---|
| T1: 23 ABI 破坏(新 LL 模块接口变更) | 严格使用 `noexcept` + `[[nodiscard]]`;锁版本宏 |
| T2: DLLP 编码错误(bit 序 / 字段对齐) | 参考 Xilinx pcie-model 的 DLLP 实现 |
| T3: FC 反压过强(死锁) | Token Bucket 容量上限 + 测试覆盖 |

---

## Phase 2: 128b/130b 编码延迟建模

### 元数据

| 项 | 值 |
|---|---|
| **OpenSpec Change 名** | `cpptlm-dgpu-pcie-130b-encoding` |
| **Change folder** | `openspec/changes/2026-09-29-cpptlm-dgpu-pcie-130b-encoding/` |
| **工期** | 2 周(W5-W6) |
| **优先级** | 🟡 P1 |
| **依赖** | Phase 1 |
| **并行** | 可与 Phase 3 并行(W5-W6) |

### Why

Gen5 PCIe 使用 **128b/130b 编码**(不是 FLIT — FLIT 是 Gen6)。SoC 仿真不需 bit-level 编码,但需建模:
- **速率切换延迟**: Gen1→Gen5 链路宽度协商时间
- **块传输延迟**: Gen5 32 GT/s 下 256-bit block 约 32ns
- **扰码**: PCIe spec §4.2.3 强制扰码(可选建模)

### What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_encoding_latency_model.hh` + `.cc` | **新**: 延迟建模器(Gen1-5 速率表) |
| `include/tlm/pcie/pcie_link_layer_tlm.cc` | **改**: Tx/Rx Path 注入延迟 |
| `test/test_pcie_encoding_latency.cc` | **新**: 延迟单元测试 |
| `test/test_pcie_endpoint_throughput.cc` | **新**: Gen5 速率下吞吐回归 |

### Acceptance Gate

| Gate | 验证方法 |
|---|---|
| **P2-G1** 延迟模型单元测试 PASS(各 Gen 速率正确) | `ctest -R test_pcie_encoding_latency` |
| **P2-G2** PcieEndpointTLM 集成延迟后,Gen5 速率下吞吐符合 spec(>= 95% 理论带宽) | `ctest -R test_pcie_endpoint_throughput` |
| **P2-G3** Phase 1 集成测试无回归 | `ctest -R pcie_link_layer\|pcie_endpoint_with_ll` |

---

## Phase 3: PHY Digital Control + Bypass Mux

### 元数据

| 项 | 值 |
|---|---|
| **OpenSpec Change 名** | `cpptlm-dgpu-pcie-phy-digital-ctrl` |
| **Change folder** | `openspec/changes/2026-09-29-cpptlm-dgpu-pcie-phy-digital-ctrl/` |
| **工期** | 4 周(W5-W8,与 P2 并行) |
| **优先级** | 🟡 P1 |
| **依赖** | Phase 1 |
| **被依赖** | Phase 4, Phase 7 |

### Why

PHY Digital Control 是 PHY 数字侧(非 PHY 模拟本身):
- **LTSSM**: 11 主状态状态机 + Gen3+ 均衡(TS1/TS2 + Preset,非 Gen5 扩展,Oracle Top-8 修订)
- **均衡协商**: Gen3+ TS1/TS2 序列 + Preset 选择
- **PIPE 4-signal 端**: 接受/产生 `(rate, lanes_active, elec_idle, training_state)`
- **热插拔信号**: PWRGOOD / PERST# / REFCLK+ / MRL / PRSNT#
- **Bypass Mux**: 3 态模式切换(Full / Bypass / Partial)

### What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh` + `.cc` | **新**: LTSSM + Eq + Hot-Plug 状态机 |
| `include/tlm/pcie/pcie_bypass_mux.hh` + `.cc` | **新**: 3 态 Bypass Mux(状态清理) |
| `include/bundles/pcie_dllp_bundles_tlm.hh` | **改**: 加 `PciePipeSignal` + `PciePhyConfig` |
| `src/tlm/gpu/pcie_endpoint_tlm.cc` | **改**: 集成 PHY Digital Ctrl + Bypass Mux |
| `test/test_pcie_phy_digital_ltssm.cc` | **新**: LTSSM 状态转换测试 |
| `test/test_pcie_phy_digital_equalization.cc` | **新**: Gen3+ 均衡协商测试 |
| `test/test_pcie_phy_digital_hotplug.cc` | **新**: 热插拔信号测试 |
| `test/test_pcie_bypass_mux.cc` | **新**: Bypass 模式切换 + 状态清理测试 |

### Acceptance Gate

| Gate | 验证方法 |
|---|---|
| **P3-G1** LTSSM 11 主状态状态转换测试 PASS | `ctest -R test_pcie_phy_digital_ltssm` |
| **P3-G2** Gen3+ 均衡协商(TS1/TS2 + Preset)PASS | `ctest -R test_pcie_phy_digital_equalization` |
| **P3-G3** 热插拔信号(PWRGOOD/PERST#/MRL/PRSNT#)PASS | `ctest -R test_pcie_phy_digital_hotplug` |
| **P3-G4** Bypass Mux 3 态切换 + 状态清理 PASS | `ctest -R test_pcie_bypass_mux` |
| **P3-G5** 集成测试:P1+P2+P3 协同无回归 | `ctest -R pcie_endpoint_with_ll\|pcie_phy_digital\|pcie_bypass` |

---

## Phase 4: SR-IOV VF Pool

### 元数据

| 项 | 值 |
|---|---|
| **OpenSpec Change 名** | `cpptlm-dgpu-pcie-sriov` |
| **Change folder** | `openspec/changes/2026-10-27-cpptlm-dgpu-pcie-sriov/` |
| **工期** | 4 周(W9-W12) |
| **优先级** | 🟢 P2(GPGPU 主流,生产需要) |
| **依赖** | Phase 3 |
| **被依赖** | Phase 7 |

### Why

GPGPU 主流(Nvidia H100 / AMD MI300)标配 SR-IOV:
- 1 PF + 多 VF(典型 8-16)
- 每个 VF 独立 Config Space / MSI-X / BAR / FC / Retry / Seq#
- 避免 16 × 4 = 64 端口爆炸 → **VF Pool 模式**(共享 StreamAdapter)

### What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh` + `.cc` | **新**: VF Pool 路由表 |
| `include/tlm/gpu/pcie_config_space_mvp.hh` + `.cc` | **改**: 支持 VF 4KB 独立 config + ARI |
| `include/tlm/gpu/msix_table_mvp.hh` + `.cc` | **改**: per-VF 独立 MSI-X table |
| `src/tlm/gpu/pcie_endpoint_tlm.cc` | ❌ **不改**(Oracle Top-3: 归档 spec 冻结 `num_ports()==4`;17 端口属于新类) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` + `src/tlm/pcie/pcie_endpoint_ip.cc` | **新**: `PcieEndpointIP` 整合模块(composition 旧 `PcieEndpointTLM` + VF Pool),17 端口(PF0 + VF0-15) |
| `include/chstream_register.hh` | **改**: 新增 `REGISTER_CHSTREAM(PcieEndpointIP)`,旧注册不变(整合交付 W24 才移除) |
| `include/chstream_register.hh` | **改**: 注册 VF Pool StreamAdapter |
| `test/test_pcie_sriov_vf_pool_routing.cc` | **新**: VF 路由测试 |
| `test/test_pcie_sriov_per_vf_config.cc` | **新**: per-VF Config Space 测试 |
| `test/test_pcie_sriov_per_vf_msix.cc` | **新**: per-VF MSI-X 测试 |
| `test/test_pcie_sriov_ari_capability.cc` | **新**: ARI 能力测试 |

### Acceptance Gate

| Gate | 验证方法 |
|---|---|
| **P4-G1** VF Pool 路由(PF0/VF0-15 正确分发)PASS | `ctest -R test_pcie_sriov_vf_pool_routing` |
| **P4-G2** per-VF Config Space 独立测试 PASS | `ctest -R test_pcie_sriov_per_vf_config` |
| **P4-G3** per-VF MSI-X 独立测试 PASS | `ctest -R test_pcie_sriov_per_vf_msix` |
| **P4-G4** ARI 能力(替代 routing-id)PASS | `ctest -R test_pcie_sriov_ari_capability` |
| **P4-G5** 集成测试(P1-P4)无回归 | 全量 `ctest -R pcie` |

---

## Phase 5: AXI Stream Adapter

### 元数据

| 项 | 值 |
|---|---|
| **OpenSpec Change 名** | `cpptlm-dgpu-axi-stream-adapter` |
| **Change folder** | `openspec/changes/2026-09-08-cpptlm-dgpu-axi-stream-adapter/` |
| **工期** | 3 周(W1-W3,与 P1 并行) |
| **优先级** | 🟡 P1(独立模块,可与 P1 并行) |
| **依赖** | 无 |
| **被依赖** | Phase 6, Phase 7 |

### Why

PcieEndpointIP 需要 AXI 接口与 SoC 交互:
- **AXI master out**: EP → SoC(VRAM DMA / BAR MMIO)
- **AXI slave in**: SoC → EP(上行 DMA / admin)
- **AXI cfg slave in**: SoC → EP config(SR-IOV admin / 内部配置)

**关键**: 与现有 `PcieTlpBundle` 解耦,提供独立 AXI 通道。

### What Changes

| 文件 | 用途 |
|---|---|
| `include/bundles/axi4_bundles_tlm.hh` | **新**: Axi4Bundle + Axi4LiteBundle |
| `include/framework/axi4_stream_adapter.hh` + `src/framework/axi4_stream_adapter.cc` | **新**: 多端口 AXI Stream Adapter(master/slave/cfg_slave) |
| `include/tlm/pcie/pcie_axi_adapter_tlm.hh` + `src/tlm/pcie/pcie_axi_adapter_tlm.cc` | **新**: PcieEndpointIP 的 AXI 适配 |
| `src/tlm/gpu/pcie_endpoint_tlm.cc` | **改**: 集成 AXI Adapter(可选注入) |
| `test/test_axi4_bundle.cc` | **新**: Axi4Bundle 序列化测试 |
| `test/test_pcie_axi_adapter_basic.cc` | **新**: AXI Adapter 基础读写测试 |
| `test/test_pcie_axi_adapter_64byte_burst.cc` | **新**: 64-byte burst(Gen5 AXI 512-bit)测试 |

### Acceptance Gate

| Gate | 验证方法 |
|---|---|
| **P5-G1** Axi4Bundle / Axi4LiteBundle 序列化测试 PASS | `ctest -R test_axi4_bundle` |
| **P5-G2** AXI Adapter 基础读写测试 PASS | `ctest -R test_pcie_axi_adapter_basic` |
| **P5-G3** 64-byte burst(AXI 512-bit)测试 PASS | `ctest -R test_pcie_axi_adapter_64byte_burst` |

---

## Phase 6: AXI4Mapper 独立模块

### 元数据

| 项 | 值 |
|---|---|
| **OpenSpec Change 名** | `cpptlm-dgpu-axi4-mapper` |
| **Change folder** | `openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/` |
| **工期** | 4 周(W13-W16) |
| **优先级** | 🟢 P2 |
| **依赖** | Phase 5 |
| **被依赖** | Phase 7(可选) |

### Why

AXI4 有 Outstanding / Out-of-order / W-channel 等复杂特性,**不应内嵌在 PcieEndpointIP**:
- 内嵌 → 复杂度爆炸 + 与 SoC 其他 AXI 用户耦合
- 独立模块 → 可被 CrossbarTLM / CacheTLM 复用 + 按 JSON 可选注入

### What Changes

| 文件 | 用途 |
|---|---|
| `include/framework/axi4_mapper.hh` + `src/framework/axi4_mapper.cc` | **新**: AXI4 ↔ Bundle Mapper(独立模块) |
| `include/framework/axi4_signal_to_bundle.hh` + `.cc` | **新**: AXI 信号 ↔ Axi4Bundle 转换 |
| `include/framework/axi4_bundle_to_signal.hh` + `.cc` | **新**: Axi4Bundle ↔ AXI 信号 转换 |
| `test/test_axi4_mapper_basic.cc` | **新**: AXI4 ↔ Bundle 基础测试 |
| `test/test_axi4_mapper_outstanding.cc` | **新**: Outstanding transaction 测试 |
| `test/test_axi4_mapper_out_of_order.cc` | **新**: Out-of-order completion 测试 |
| `test/test_axi4_mapper_integration_with_pcie.cc` | **新**: 与 PcieEndpointIP 集成测试 |

### Acceptance Gate

| Gate | 验证方法 |
|---|---|
| **P6-G1** AXI4 ↔ Bundle 基础测试 PASS | `ctest -R test_axi4_mapper_basic` |
| **P6-G2** Outstanding transaction 测试 PASS | `ctest -R test_axi4_mapper_outstanding` |
| **P6-G3** Out-of-order completion 测试 PASS | `ctest -R test_axi4_mapper_out_of_order` |
| **P6-G4** 与 PcieEndpointIP 集成测试 PASS | `ctest -R test_axi4_mapper_integration_with_pcie` |

---

## Phase 7: Host Bypass + Root Complex(可选)

### 元数据

| 项 | 值 |
|---|---|
| **OpenSpec Change 名** | `cpptlm-dgpu-pcie-host-bypass-and-rc` |
| **Change folder** | `openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/` |
| **工期** | 3 周工作量(W17-W19)+ 5 周集成/缓冲(W20-W24) |
| **优先级** | 🟢 P2(完整 dGPU PCIe EP 仿真) |
| **依赖** | **P5(必须,HostBypass 桥接 AXI)+ P3/P4(可选,PcieRootComplexTLM 镜像需要 PcieEndpointIP 存在)**(Oracle C11 修订) |
| **被依赖** | (最终交付) |

### Why

完整 dGPU PCIe EP 仿真需要 Host 侧:
- **HostBypassTLM**: 软件 bring-up 阶段跳过 PCIe RC BFM
- **PcieRootComplexTLM**(可选):自研 RC 模型(若不用外部 VIP/QEMU)
- 可选 wrap `alexforencich/verilog-pcie` 提供真实 PHY 验证

### What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/host_bypass_tlm.hh` + `src/tlm/pcie/host_bypass_tlm.cc` | **新**: HostBypassTLM(独立组件) |
| `include/tlm/pcie/pcie_root_complex_tlm.hh` + `.cc` | **新**: PcieRootComplexTLM(可选镜像) |
| `external/alexforencich-verilog-pcie/` (git submodule, 可选) | wrap 真实 PHY |
| `test/test_host_bypass_basic.cc` | **新**: Host Bypass 基础测试 |
| `test/test_host_bypass_software_bringup.cc` | **新**: 软件 bring-up 场景测试 |
| `test/test_pcie_root_complex_enumeration.cc` | **新**: PCIe 枚举测试(若 RC 实现) |
| `examples/demo_pcie_full_e2e.py` | **新**: E2E demo(PcieEndpointIP ↔ Host) |

### Acceptance Gate

| Gate | 验证方法 |
|---|---|
| **P7-G1** HostBypassTLM 基础测试 PASS | `ctest -R test_host_bypass_basic` |
| **P7-G2** 软件 bring-up 场景测试 PASS | `ctest -R test_host_bypass_software_bringup` |
| **P7-G3**(可选) PCIe 枚举测试 PASS(若 RC 实现) | `ctest -R test_pcie_root_complex_enumeration` |
| **P7-G4** E2E demo 运行成功 | `python examples/demo_pcie_full_e2e.py` |

---

## 整合交付(W24 末)

### 顶层交付物

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_endpoint_ip.hh` + `src/tlm/pcie/pcie_endpoint_ip.cc` | **整合模块**: 包含 §1-§7 全部组件 |
| `include/chstream_register.hh` | **改**: 注册新 `PcieEndpointIP` |
| `examples/dgpu_soc_with_pcie_ip.json` | **新**: 完整 dGPU SOC + PCIe EP 示例配置 |
| `docs/architecture/14-pcie-ip-microarchitecture.md` | **新**: 完整架构文档(由本 design.md 迁移) |
| `test/test_pcie_endpoint_ip_full_e2e.cc` | **新**: 全链路 E2E 测试 |

### 旧代码迁移

- `include/tlm/gpu/pcie_endpoint_tlm.h` 从 `chstream_register.hh` 移除
- 加 `[[deprecated]]` 提示用户迁移到 `PcieEndpointIP`
- CMake `configure_file` 自动迁移旧 JSON configs

---

## 风险总览(全 7 阶段)

| ID | 风险 | 影响 | 缓解 |
|---|---|---|---|
| **R1** | 23 ABI 破坏 | 🔴 致命 | `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` + 严格 noexcept |
| **R2** | SR-IOV 端口爆炸 | 🔴 致命 | VF Pool 共享 StreamAdapter |
| **R3** | FC 反压死锁 | 🟡 中 | Token Bucket 容量上限 + 测试覆盖 |
| **R4** | Bypass 状态泄漏 | 🟡 中 | `apply_mode()` 强制 flush pending 状态 |
| **R5** | AXI4Mapper 与 PcieEndpointIP 耦合 | 🟡 中 | AXI4Mapper 独立模块 |
| **R6** | LTSSM 异步 vs TLM 同步 | 🟡 中 | 内部状态机 + EventQueue 建模异步 |
| **R7** | Gen5 ≠ FLIT 误植 | 🟢 低 | 文档冻结 Gen5 边界,6.0 单独 change |
| **R8** | 单工程师 24 周超期 | 🟡 中 | P1 + P5 并行启动;预留 1 周缓冲 |

---

## 启动指南:Phase 1 立即启动

### 1. 创建 Phase 1 OpenSpec change

```bash
# 由本 umbrella change 评审通过后执行
mkdir -p openspec/changes/2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/
# 拷贝 Phase 1 模板 → 详见 tasks.md §Phase 1
```

### 2. TDD 5 步(每个模块)

参见 [`tasks.md` §Phase 1](./tasks.md) 的 TDD 5 步结构:
1. Write failing test
2. Verify fail
3. Implement
4. Verify pass
5. Commit

### 3. 跨 PR 检查清单

- [ ] `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏已加
- [ ] 所有新 public 方法 `noexcept` / 异常规范明确
- [ ] 所有查询方法 `[[nodiscard]]`
- [ ] CMakeLists 源文件显式列出(禁 GLOB)
- [ ] 测试覆盖率 >= 90%
- [ ] `cmake --build` + `ctest` 无回归
- [ ] `openspec validate` PASS

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📅 Planned — 等待伞形 change 评审后,Phase 1 立即启动
