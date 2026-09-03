# 01. dGPU SoC v1.0 Host Interface 架构 — PcieEndpointIP + HostBypassTLM + PcieRootComplexTLM

> **类别**: SoC Architecture > 子系统架构 (L1 Host Interface)
> **状态**: 📋 Draft v1 (待 Oracle 评审,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **归属 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联总架构蓝图**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（§3.1 L1 Host Interface 层）
> **关联 PCIe EP 微架构**: [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md)（950 行,Phase 1-7 PCIe EP 整合）
> **关联真实代码**:
> - [`include/tlm/pcie/pcie_endpoint_ip.hh`](../../../include/tlm/pcie/pcie_endpoint_ip.hh)
> - [`include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh`](../../../include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh)
> - [`include/tlm/pcie/host_bypass_tlm.hh`](../../../include/tlm/pcie/host_bypass_tlm.hh)
> - [`include/tlm/pcie/pcie_root_complex_tlm.hh`](../../../include/tlm/pcie/pcie_root_complex_tlm.hh)
> - [`include/tlm/pcie/pcie_axi_adapter_tlm.hh`](../../../include/tlm/pcie/pcie_axi_adapter_tlm.hh)
> - [`include/tlm/pcie/pcie_link_layer_tlm.hh`](../../../include/tlm/pcie/pcie_link_layer_tlm.hh)
> - [`include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh`](../../../include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh)
> - [`include/tlm/pcie/pcie_bypass_mux.hh`](../../../include/tlm/pcie/pcie_bypass_mux.hh)
> - [`include/framework/axi4_mapper.hh`](../../../include/framework/axi4_mapper.hh)
> - [`include/framework/axi4_stream_adapter.hh`](../../../include/framework/axi4_stream_adapter.hh)
> **关联 OpenSpec changes**(已存在):
> - [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch/`](../../../openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/)（umbrella）
> - [`2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc`](../../../openspec/changes/2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/)（Phase 1 链路层 + FC）
> - [`2026-09-29-cpptlm-dgpu-pcie-130b-encoding`](../../../openspec/changes/2026-09-29-cpptlm-dgpu-pcie-130b-encoding/)（Phase 2 128b/130b 编码）
> - [`2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl`](../../../openspec/changes/2026-10-06-cpptlm-dgpu-pcie-phy-digital-ctrl/)（Phase 3 PHY + Bypass Mux）
> - [`2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool`](../../../openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/)（Phase 4 SR-IOV VF Pool）
> - [`2026-11-03-cpptlm-dgpu-axi-stream-adapter`](../../../openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/)（Phase 5 AXI Stream Adapter）
> - [`2026-12-22-cpptlm-dgpu-axi4-mapper`](../../../openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/)（Phase 6 AXI4Mapper）
> - [`2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc`](../../../openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/)（Phase 7 Host Bypass + RC）
> - [`2027-02-09-cpptlm-dgpu-pcie-ip-integration`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-pcie-ip-integration/)（Phase 8 整合交付,HEAD `429327d`）

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 总架构蓝图 §3.1 L1 Host Interface 层的**子系统架构**详细化文档。

- 想快速理解 L1 层结构 → 读 §1(范围与目标) + §2(顶层数据流图)
- 想理解 PcieEndpointIP 内部结构 → 读 §3(端口定义 + 7 个内部组件)
- 想理解 7 个内部组件细节 → 读 §4(链路层) + §5(PHY 数字控制) + §6(Bypass Mux) + §7(SR-IOV VF Pool) + §8(AXI Stream Adapter) + §9(AXI4Mapper) + §10(AXI↔PCIe Adapter)
- 想理解软件 bring-up → 读 §11(HostBypassTLM)
- 想理解 RC 简化模型 → 读 §12(PcieRootComplexTLM)
- 想理解 23 ABI 边界 → 读 §13
- 想理解配置 Schema → 读 §14
- 想查阅 ADR/微架构/OpenSpec 引用 → 读 §15
- 想评估风险 → 读 §16

---

## 1. 范围与目标

### 1.1 L1 Host Interface 层定位

**L1 Host Interface 层** = dGPU SoC v1.0 系统拓扑的**最外层接口**,负责 Host CPU 与 dGPU SoC 内部模块(PcieEndpointIP + SdmaEngineTLM + 内部 PCIe 状态)之间的**双向数据交互**:

- **Host → dGPU**(PCIe Posted Write / Non-Posted Read / Completion):
 - Host CPU 写入 PushBuffer/Ring Buffer (doorbell)
  - Host CPU 发起 MMIO/Config 读 (BAR 访问 + Config Space)
  - Host CPU 发起 DMA 读/写 (H2D/D2H)
- **dGPU → Host**(PCIe Completion / MSI-X):
 - GPU 完成 ring 写回
  - MSI-X 中断投递

**L1 层模块清单**(per `00-overview` §3.1):
| 模块 | 来源 | 角色 |
|------|------|------|
| **`PcieEndpointIP`** | 17 ports 整合模块 (Phase 4-8) | PCIe EP slave/master 端, 1 PF + 16 VF |
| **`SdmaEngineTLM`** | ADR-SOC-07 D3 (已归档) | PCIe master 端,H2D/D2H DMA |
| **`HostBypassTLM`** | Phase 7 (per `2027-01-19`) | 软件 bring-up 跳过 RC BFM,直接桥接 AXI |
| **`PcieRootComplexTLM`** | Phase 7 (per `2027-01-19`) | 自研 RC,枚举 PF0-only(Oracle M2 标注) |

### 1.2 v1.0 战略关键决策

| 决策点 | v1.0 MVP | v1.1 完整版 | 关联 ADR |
|--------|---------|------------|---------|
| 17 ports PcieEndpointIP | ✅ | 同 v1.0 | ADR-SOC-11（Proposed） |
| PcieEndpointTLM 4 端口 | deprecated(`[[deprecated]]`) | 维持 deprecated | ADR-SOC-11 |
| SdmaEngineTLM 归属 SOC | ✅ | 同 v1.0 | ADR-SOC-07 |
| HostBypassTLM M1 修复 (429327d) | ✅ 4 方向 AXI 桥接 | 同 v1.0 | ADR-SOC-12（Proposed） |
| PcieRootComplexTLM PF0-only | ✅ Oracle M2 标注 | + ARI/SR-IOV capability | ADR-SOC-12 |
| 23 ABI 头冻结 + 仓内实现 19/19 + 4 回调 typedef | ⚠️ 仓内就绪 / 端侧未闭环 | UsrLinuxEmu 4 回调注册未闭环 | ADR-SOC-14（Proposed） |

### 1.3 与总架构蓝图的一致性

本文档**严格对齐** `00-overview.md` v3.1 PASS 的 §3.1 L1 Host Interface 层 + §4-bis 范围矩阵 R01-R03 + §6.1 兼容性分析 + §7.2 模块微架构引用矩阵。

---

## 2. 顶层数据流图

### 2.1 L1 层数据流总览

```
                    ┌─────────────────────────────────────────────┐
                    │            Host CPU (x86/ARM)                │
                    │  - CUDA Driver / ROCm KFD                    │
                    │  - PushBuffer / Ring Buffer                   │
                    │  - 23 ABI cpptlm_emulator_* (per ADR-088)    │
                    └───────────────────┬─────────────────────────┘
                                        │
                  ┌─────────────────────┼─────────────────────┐
                  │ PCIe Gen5/6 PHY    │ PCIe Link Layer      │
                  │ (PHY 数字控制)      │ (FC token bucket)    │
                  └──────────┬──────────┴──────────────────────┘
                             │
                  ┌──────────▼──────────────────────────────┐
                  │  PcieEndpointIP  (17 ports)             │
                  │  ┌─────────────────────────────────────┐ │
                  │  │  port[0]   = PF0  ─→ SoC Bus     │ │
                  │  │  port[1]   = VF0  ─→ SoC Bus     │ │
                  │  │  ...                                │ │
                  │  │  port[16]  = VF15 ─→ SoC Bus     │ │
                  │  └─────────────────────────────────────┘ │
                  │  + SdmaEngineTLM (PCIe master)            │
                  └───────────────────┬───────────────────────┘
                                      │
                                      ▼
                  ┌──────────────────────────────────────────┐
                  │  SoC Internal (L2-L7 per 00-overview)    │
                  │  - L2 Command Stream                      │
                  │  - L3 Command Protocol                    │
                  │  - L4 TMU/TMD                             │
                  │  - L5 WDU → SM/CU                        │
                  │  - L6 SM/CU 内核                          │
                  │  - L7 Memory Hierarchy                    │
                  └──────────────────────────────────────────┘

   ──────────────────  软件 Bring-up 路径  ──────────────────

                  ┌──────────────────────────────────────────┐
                  │  HostBypassTLM                            │
                  │  - skip PCIe RC BFM                       │
                  │  - AXI 4-direction bridge (429327d)       │
                  │  - axi_master_out / axi_slave_in          │
                  │  - cfg_slave_in (AXI4-Lite)               │
                  └───────────────────┬───────────────────────┘
                                      │
                                      ▼
                  ┌──────────────────────────────────────────┐
                  │  PcieEndpointIP  (软件 bring-up 路径)     │
                  └──────────────────────────────────────────┘
```

### 2.2 关键交互注释

- **物理层路径**(左):Host CPU 通过 PCIe PHY (Gen5/6) + Link Layer (FC token bucket) → PcieEndpointIP 17 ports → SoC Internal
- **软件 bring-up 路径**(下):HostBypassTLM 直接桥接 AXI ↔ PcieEndpointIP,跳过真实 PCIe PHY/Link Layer,**用于 bring-up 阶段快速验证**
- **双向性**:PcieEndpointIP 同时是 PCIe slave(Host 写入)+ PCIe master(SdmaEngineTLM DMA 发起)

---

## 3. PcieEndpointIP 端口定义(真实实现)

### 3.1 端口定义(per `include/tlm/pcie/pcie_endpoint_ip.hh:50-53`)

```cpp
class PcieEndpointIP : public ChStreamModuleBase {
public:
    static constexpr unsigned NUM_PORTS = 17;  // 0=PF, 1..16=VF0..VF15

    cpptlm::InputStreamAdapter<bundles::PcieTlpBundle> req_in[NUM_PORTS];
    cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle> resp_out[NUM_PORTS];
```

### 3.2 端口表(权威)

| 端口类型 | 数量 | 索引 | 元素类型 | 用途 |
|---------|-----|------|---------|------|
| `req_in` | 17 | `port[0]` = PF0 / `port[1..16]` = VF0..VF15 | `cpptlm::InputStreamAdapter<bundles::PcieTlpBundle>` | PCIe TLP 请求入口(per function) |
| `resp_out` | 17 | 同上 | `cpptlm::OutputStreamAdapter<bundles::PcieTlpBundle>` | PCIe TLP 响应出口(per function) |

**关键设计**:
- **NUM_PORTS = 17**:1 PF + 16 VF,避免 N×16 端口爆炸
- **内部 stream_id 路由**:用 `stream_id`(PCIe Requester ID 的 function 部分)区分 VF,避免端口按 VF 数量级展开(per `docs/architecture/14` §3 端口图)
- **MSI-X / Config Space / BAR**:非端口,通过内部方法访问(见 §3.3)

### 3.3 内部访问方法(per `pcie_endpoint_ip.hh:85-89`)

| 方法 | 返回 | 用途 |
|------|------|------|
| `vf_pool()` / `vf_pool() const` | `PcieSriovVfPool&` | SR-IOV VF Pool (1 PF + 16 VF) |
| `completions()` / `completions() const` | `CompletionTracker&` | 完成追踪(单一真源在 VfPool,避免双份 outstanding 失配,per Q12) |
| `config_of(uint16_t stream_id)` | `tlm::gpu::PcieConfigSpace&` | per-VF Config Space 访问(per stream_id 路由) |
| `msix_of(uint16_t stream_id)` | `tlm::gpu::MsiXTable&` | per-VF MSI-X Table 访问 |

### 3.4 Stream Adapter 注入(per `pcie_endpoint_ip.hh:63-66`)

```cpp
void set_stream_adapter(cpptlm::StreamAdapterBase* a) override;
void set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) override;
unsigned num_ports() const override { return NUM_PORTS; }
```

- **多端口注入**:通过 `adapters[17]` 数组注入,自动路由到对应 PF/VF 端口
- **17 个独立 StreamAdapter**:每个 PF/VF 端口独立,避免状态混合

---

## 4. 内部组件 1 — PcieLinkLayerTLM(Phase 1)

### 4.1 组件定位

PcieLinkLayerTLM 是 PCIe 链路层实现,**位于 PHY 数字控制之上、PcieEndpointIP 集成层之下**,负责:

- **DLLP**(Data Link Layer Packet)处理:ACK/NAK 协议 + CRC 校验 + 序列号管理
- **FC**(Flow Control)token bucket:Posted/Non-Posted/Completion 三类信用管理
- **链路层重传**:基于 ACK/NAK 协议的 TLP 重传
- **链路训练**:与 PHY 协同完成 Gen5/Gen6 速率协商

### 4.2 关键数据结构

| 结构 | 字段 | 用途 |
|------|------|------|
| `LinkState` | `state` (Active/L0s/L1/L2) + `speed` (Gen5/6) + `width` (x1/x4/x8/x16) | 链路状态 |
| `FcCredit` | `posted_credits` + `np_credits` + `cpl_credits` | 三类信用 |
| `DllpFrame` | `type` (ACK/NAK/UpdateFC/InitFC1/2) + `seq_num` + `payload` | DLLP 帧 |
| `RetryBuffer` | `tlp_queue` + `ack_history` + `nak_count` | 重传缓冲 |

### 4.3 关键流程

```
1. 接收 TLP → CRC 校验 → 分配 seq_num → 入 RetryBuffer → 触发 ACK DLLP
2. 接收 DLLP(ACK) → 更新 RetryBuffer → 释放已确认 TLP
3. 接收 DLLP(NAK) → 重传 NAK 序列号 → 入 RetryBuffer
4. UpdateFC DLLP → 更新三类信用计数器
5. InitFC1/2 DLLP → 初始化链路训练阶段信用
```

### 4.4 关联研究

- **PCIe 保序 write**(per `docs/research/PCIe/PCIe_上的保序write.md`):MMU ordering pipe + dummy non-posted read flush
- **Gen5 x16 latency 周期精度**:仅 PCIe 范围(per `docs-archived/adr/ADR-X.16` §周期精度注记)

---

## 5. 内部组件 2 — PciePhyDigitalCtrlTLM(Phase 3)

### 5.1 组件定位

PciePhyDigitalCtrlTLM 是 PHY 数字控制实现,**位于 PCIe PHY 模拟层之上、链路层之下**,负责:

- **LTSSM**(Link Training and Status State Machine)11 态状态机
- **物理层协商**:速度 (Gen5/6) + 宽度 (x1/x4/x8/x16) + 通道反转
- **Polling/Speed.Change/Recovery 状态**:PCIe 规范定义的标准握手
- **Electrical Idle 检测**:L1/L2 低功耗状态进入/退出

### 5.2 LTSSM 11 态(per `docs/architecture/14` §5.5 LTSSM 状态转换表)

```
Detect → Polling → Configuration → L0 → Recovery
   ↓        ↓           ↓           ↓       ↓
  Loopback HotReset Disable L0s/L1/L2 Loopback
```

**详细状态**:
- **Detect**:检测 PCIe 设备存在
- **Polling**:TS1/TS2 ordered set 交换(Gen5/6 速率)
- **Configuration**:Lane/Link number 协商 + 通道反转
- **L0**:Active 状态(正常 TLP/DLLP 交换)
- **Recovery**:链路错误恢复(L0 → Recovery → L0)
- **L0s/L1/L2**:低功耗状态
- **Hot Reset**:PCIe 规范定义的热复位
- **Loopback**:物理层回环测试
- **Disable**:链路禁用状态

### 5.3 关键数据结构

| 结构 | 字段 | 用途 |
|------|------|------|
| `LtssmState` | `state` (11 态枚举) + `speed` + `width` + `lane_reversal` | LTSSM 状态 |
| `TsOrderedSet` | `TS1 type` + `link_num` + `lane_num` + `symbol_num` | 物理层 ordered set |
| `ElectricalIdle` | `lane_idle_count` + `idle_threshold` | EID 检测 |

### 5.4 关联研究

- **Gen5 x16 物理层参数**:32 GT/s, ×16 lane → 64 GB/s
- **Gen6 物理层参数**:64 GT/s, ×16 lane → 128 GB/s(NVIDIA Blackwell Ultra,per `docs/research/SM/NVIDIA_Blackwell_Architecture_Technical_Brief_v2.1_官方技术简报_解析.md`)

---

## 6. 内部组件 3 — PcieBypassMux(Phase 3)

### 6.1 组件定位

PcieBypassMux 是 PCIe 物理层与功能层之间的**模式切换开关**,支持 3 种模式(per `docs/architecture/14` §7):

| 模式 | 用途 | 路径 |
|------|------|------|
| **Full** | 真实 PCIe PHY (Link + PHY + TLP) | PHY → Link → TLP |
| **Bypass** | 完全跳过 PHY/Link,直接 AXI 适配 | AXI Adapter ↔ SoC Bus |
| **Partial** | 仅跳过 PHY,Link + TLP 完整 | Link → TLP |

### 6.2 模式选择决策矩阵(per `docs/architecture/14` §7)

| 场景 | 推荐模式 | 理由 |
|------|---------|------|
| **真实 PCIe PHY 验证**(FPGA 仿真) | Full | 真实 PHY 行为 |
| **软件 bring-up** | Bypass | 跳过 PCIe PHY 初始化,快速 SoC 验证 |
| **混合仿真** (RTL-PHY + TLM-SoC) | Partial | PHY 实测 + TLM 高效 |

### 6.3 JSON 配置(`params.bypass_mode`)

```json
{
  "name": "pcie_endpoint_ip_0",
  "type": "PcieEndpointIP",
  "params": {
    "bypass_mode": "bypass",      // "full" | "bypass" | "partial"
    "speed": "gen5",              // "gen5" | "gen6"
    "width": 16                   // 1 | 4 | 8 | 16
  }
}
```

### 6.4 关键约束(per Oracle 修订 #7)

- **Bypass 模式**仅供软件 bring-up 使用,**不可用于真实 PCIe PHY 验证**
- **Partial 模式**需要 RTL-PHY 子模块支持(`alexforencich/verilog-pcie` 可选)
- **Full 模式**需要 PHY 数字控制 + Link Layer 全部就绪

---

## 7. 内部组件 4 — PcieSriovVfPoolTLM(Phase 4)

### 7.1 组件定位

PcieSriovVfPoolTLM 是 **SR-IOV VF Pool** 实现,负责管理 1 PF + 16 VF 的:
- **per-VF Config Space**(4096 B PCIe ECAM 兼容 + 256 B 简化)
- **per-VF MSI-X Table**(mask/unmask/PBA)
- **per-VF FC**(Flow Control)信用
- **per-VF seq#**(序列号管理)
- **Completion tracking**(trans_id 关联,单一真源)

### 7.2 关键设计原则

- **单一真源**:所有 VF 状态(`completions_`, `msix_of(vf_id)`, `config_of(vf_id)`)均在 `PcieSriovVfPoolTLM` 自持;`PcieEndpointIP` 仅委托访问,**不另持副本**(per `pcie_endpoint_ip.hh:82-83`)
- **stream_id 路由**:用 `stream_id` 区分 VF,避免 16 个独立 StreamAdapter

### 7.3 Completion Tracking(per Q12 修复)

- **生产路径** (`dispatch_tlp`) 在 `pool_.completions()` 登记 outstanding
- **完成路径** (`resp_out`) 从 `pool_.completions()` 清除 outstanding
- **响应方向**:`req_in → 登记` →`->resp_out → 清除`(单向流水线)
- **避免双份 outstanding**:生产端登记的 trans_id 必须能映射到响应端的同一 trans_id

### 7.4 关键数据结构

| 结构 | 字段 | 用途 |
|------|------|------|
| `VfState` | `vf_id` (1..16) + `msix_mask` + `msix_pending` + `fc_credits` | per-VF 状态 |
| `PfState` | `pf_id` (0) + 同 VfState 字段 | PF 状态 |
| `CompletionEntry` | `trans_id` + `stream_id` + `cpl_status` + `timestamp` | 完成追踪条目 |

---

## 8. 内部组件 5 — Axi4StreamAdapter(Phase 5)

### 8.1 组件定位

Axi4StreamAdapter 是 **AXI4 ↔ PCIe TLP** 边界适配器,负责:
- **AXI4 → TLP**:AXI4 事务编码为 PCIe TLP(MRd/MWr/CfgRd/CfgWr)
- **TLP → AXI4**:PCIe TLP 响应解码为 AXI4 响应(BVALID/RVALID)
- **outstanding 跟踪**:AXI4 master/slave 独立 outstanding 计数
- **OOO completion**:AXI4 响应可乱序(per `outstanding_id`)

### 8.2 三端口设计(per `host_bypass_tlm.hh` 注释)

| 端口 | 类型 | 用途 |
|------|------|------|
| `axi_master_out` | AXI4 master | SoC → Host 主动访问(如 GPU → Host BAR) |
| `axi_slave_in` | AXI4 slave | Host → SoC 事务入口(如 Host 写入 PushBuffer) |
| `cfg_slave_in` | AXI4-Lite slave | 配置空间访问(AXI4-Lite 子集) |

### 8.3 关键数据结构(per `axi4_bundles_tlm.hh`)

- **Axi4Bundle**:17 字段(transaction_id / parent_id / address / size / data / aw* / w* / b* / ar* / r*)
- **Axi4LiteBundle**:9 字段(transaction_id / address / size / data / aw* / w* / b* / ar* / r*)

### 8.4 已知限制(per Phase 5 M1)

- **`ch_uint<512>` 实际为 64-bit 存储**(per `include/bundles/cpphdl_types.hh`)
- **512-bit 字段需多次 burst 拼接**,真实 PCIe PHY 兼容

---

## 9. 内部组件 6 — Axi4Mapper(Phase 6)

### 9.1 组件定位

Axi4Mapper 是 **AXI4 OOO(Out-of-Order)Mapper** 模块,负责:
- **outstanding 跟踪**:AXI4 多事务并行,按 `outstanding_id` 路由
- **OOO completion 关联**:`rdata`/`bresp` 可乱序,但 `rid` 与 `arid` 关联
- **可独立复用**:与 PcieEndpointIP 解耦,可被 CrossbarTLM / CacheTLM 复用
- **JSON 可选注入**:`axi4_mapper_inject: true` 时启用

### 9.2 关键数据结构

- **outstanding table**:`unordered_map<rid, pending_req>`
- **completion queue**:`deque<CplEntry>`(按 rid 排序)
- **rid allocator**:`std::atomic<uint16_t>`

### 9.3 已知问题(per Phase 6 PASS + Phase 8 简化)

- **PCIe Cfg 地址编码简化**(per `00-overview` D5 + R-n):Phase8 M1 用 `awaddr` 当 offset;**完整实现需按 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset`**(未来 v1.1 细化)

---

## 10. 内部组件 7 — PcieAxiAdapter(Phase 5+8 M1)

### 10.1 组件定位

PcieAxiAdapter 是 **PcieEndpointIP 内部 AXI↔PCIe 适配器**(per `pcie_axi_adapter_tlm.hh`),负责:
- **绑定 PcieEndpointIP**:通过 `PcieAxiAdapter::set_endpoint()` 绑定
- **处理 AXI↔PCIe TLP 边界**:事务类型转换(AXI 4 类 → PCIe 4 类)
- **Phase 8 M1 修复**(per `429327d`):`PcieEndpointIP::tick()` 驱动 `PcieAxiAdapter` 处理 slave 请求 + 写 cfg/BAR + 产生响应

### 10.2 关键约束

- **Phase 5 M3 修复**(per `c5b58ac`):`PcieAxiAdapter::set_endpoint()` + `PcieEndpointIP::attach_composition` 独立 axi_adapter 分支绑定 `this`,**避免 composition 路径丢失 EP 指针**
- **Phase 5 M1 文档化**:`ch_uint<512>` 实为 64-bit 存储(已写入 spec)
- **Phase 5 M2 修复**(per `c5b58ac`):master_resp 读分支 `if (resp.rlast.read())` 才清除 outstanding

### 10.3 内部组件接口(per `pcie_axi_adapter_tlm.hh`)

```cpp
class PcieAxiAdapter {
public:
    void set_endpoint(PcieEndpointIP* ep) noexcept;
    // ... AXI↔PCIe 边界适配方法
};
```

---

## 11. HostBypassTLM 设计

### 11.1 模块定位

HostBypassTLM 是**软件 bring-up 阶段**的核心组件,负责**跳过 PCIe RC BFM**,直接桥接 AXI 接口 → PcieEndpointIP。

**使用场景**:
- **bring-up 阶段**:跳过真实 PCIe PHY/Link Layer 初始化(节省数小时仿真时间)
- **回归测试**:快速验证 SoC 内部模块,不依赖 PCIe PHY 状态机
- **E2E demo**(`demo_pcie_full_e2e.{cc,py}`):PcieEndpointIP ↔ Host 全链路

### 11.2 真实实现(per `host_bypass_tlm.hh`)

```cpp
class HostBypassTLM {
public:
    // 持有 Axi4StreamAdapter 三端口
    // axi_master_out / axi_slave_in / cfg_slave_in
    
    void init();
    void attach_to_endpoint(PcieEndpointIP* ep) noexcept;  // 绑定 EP
    void detach() noexcept;
    
    void tick();  // Phase 8 M1 修复:4 方向 AXI 桥接
    
    // ===================== axi_master_out =====================
    void set_axi_master_ready(bool r);
    void axi_master_req_consume();
    void axi_master_resp_consume();
    
    // ===================== axi_slave_in =====================
    void set_axi_slave_ready(bool r);
    void axi_slave_req_consume();
    void axi_slave_resp_consume();
    
    // ===================== cfg_slave_in =====================
    void set_axi_cfg_ready(bool r);
    void axi_cfg_req_consume();
    void axi_cfg_resp_consume();
    
    void reset();
};
```

### 11.3 Phase 8 M1 修复(per `429327d`)

**关键变更**:`HostBypassTLM::tick()` **自动转发 4 方向 AXI 通道**:
- `master_out ↔ slave_in`(双向)
- `slave_resp ↔ master_resp`(双向)

**修复目的**:让 PcieEndpointIP **真实消费** AXI 请求,而非"接而不消费"。

### 11.4 测试覆盖

| 测试 | 场景 |
|------|------|
| `test_host_bypass_basic.cc` | 基本桥接功能 |
| `test_host_bypass_software_bringup.cc` | 软件 bring-up 路径 |
| `test_host_bypass_root_complex_enumeration.cc` | 与 RC 协同 |

---

## 12. PcieRootComplexTLM 设计

### 12.1 模块定位

PcieRootComplexTLM 是**自研 Root Complex** 简化模型,**可选组件**(per `2027-01-19` 范围:"可选自研 RC 模型")。

**使用场景**:
- **集成测试**:不需要外部 PCIe VIP/QEMU
- **PCIe 枚举验证**:PF0-only 简化(per Oracle M2)

### 12.2 真实实现(per `pcie_root_complex_tlm.hh`)

```cpp
struct DiscoveredDevice {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus_num;
    uint8_t device_num;
    uint8_t function_num;
};

class PcieRootComplexTLM {
public:
    void init();
    void attach_to_endpoint(PcieEndpointIP* ep) noexcept;
    void detach() noexcept;
    
    bool enumerate();  // PCIe 枚举:发现 EP 设备/功能(PF0 + VF0..VF15)
    
    // 已发现的设备列表(enumerate 后填充)
    // 私有 members_(L134) + 公有访问器 discovered_devices()(L81)
const std::vector<DiscoveredDevice>& discovered_devices() const noexcept;
    
    void set_axi_master_ready(bool r);
    void axi_master_req_consume();
    void axi_master_resp_consume();
    
    void tick();
    void reset();
};
```

### 12.3 Oracle M2 标注(per `docs/architecture/14` L939)

**简化边界**:
- **枚举只报告 PF0**(device 0, function 0)
- **VF(VF0..VF15)的发现**需要 ARI capability + SR-IOV capability 的完整枚举流程
- **当前模型简化**:VF 通过 `stream_id` 直接访问配置空间 (`config_read/write(device, function, offset, stream_id)`)
- **已知边界**:不构成缺陷,但使用 `PcieRootComplexTLM` 进行枚举测试时必须注意此限制

### 12.4 v1.0 MVP vs v1.1 完整版

| 功能 | v1.0 MVP | v1.1 完整版 |
|------|---------|------------|
| PF0 枚举 | ✅ | ✅ |
| VF 0..15 枚举 | ❌ (PF0-only 简化) | ✅ ARI + SR-IOV capability |
| BAR 分配 | ✅ | ✅ |
| MSI-X 投递 | ✅ | ✅ |

---

## 13. 23 ABI 兼容性边界

### 13.1 ABI 冻结原则(per ADR-SOC-07 D5 + ADR-088 §D5)

**23 ABI 头冻结**(`include/abi/cpptlm_emulator.h`),**不变量**:
- C extern "C" 接口(用于 UsrLinuxEmu 集成)
- 函数签名 + 行为契约
- 头文件 layout 不可破坏

### 13.2 v1.0 状态(per `00-overview` §4-bis R26/R27)

| 维度 | 状态 |
|------|------|
| **23 ABI 头冻结** | ✅ 19 声明(per `include/abi/cpptlm_emulator.h`) |
| **仓内实现** | ✅ 19/19 函数 + 4 回调 typedef 契约(per `src/abi/cpptlm_emulator.cc` 433 行) |
| **UsrLinuxEmu 集成** | ❌ 未闭环 |
| **完整 23 函数交付** | ⚠️ 取决于 UsrLinuxEmu ADR-089 v0.5 节奏 |

### 13.3 关键 ABI 函数(per `00-overview` §4-bis + `src/abi/cpptlm_emulator.cc` 抽样)

```cpp
// C extern "C" 接口
int cpptlm_emulator_create(...);
void cpptlm_emulator_destroy(...);
int cpptlm_emulator_mmio_write(uint16_t stream_id, uint64_t offset, uint32_t value);
int cpptlm_emulator_mmio_read(uint16_t stream_id, uint64_t offset, uint32_t* value);
int cpptlm_emulator_pcie_config_write(uint16_t stream_id, uint64_t offset, uint32_t value);
int cpptlm_emulator_pcie_config_read(uint16_t stream_id, uint64_t offset, uint32_t* value);
int cpptlm_emulator_backdoor_read(uint16_t stream_id, uint64_t offset, void* buf, size_t len);
int cpptlm_emulator_backdoor_write(uint16_t stream_id, uint64_t offset, const void* buf, size_t len);
int cpptlm_emulator_msix_init(...);
int cpptlm_emulator_msix_update_pending(...);
int cpptlm_emulator_msix_clear_pending(...);
// ... 共 19 声明,~18 实现
```

### 13.4 PcieEndpointTLM 兼容性(per `[[deprecated]]` 标注 429327d)

**PcieEndpointTLM** (4 端口,s2 单体,**已 deprecated**):
- `chstream_register.hh` **保留注册**(既有 Phase 4 测试 + `dgpu_board_shell.cc` 依赖)
- 新代码统一使用 `PcieEndpointIP`(17 ports)
- `include/tlm/gpu/pcie_endpoint_tlm.h` layout **不变**(仅 `[[deprecated]]` 属性)

---

## 14. 配置 Schema

### 14.1 顶层 JSON Schema

```json
{
  "name": "pcie_endpoint_ip_0",
  "type": "PcieEndpointIP",
  "params": {
    "bypass_mode": "bypass",         // "full" | "bypass" | "partial"
    "speed": "gen5",                // "gen5" | "gen6"
    "width": 16,                    // 1 | 4 | 8 | 16
    "pf_count": 1,                  // 物理功能数
    "vf_count": 16,                 // 虚拟功能数
    "enable_msix": true,
    "enable_ari": false,            // v1.1+
    "axi4_mapper_inject": true,      // JSON 可选注入 OOO Mapper
    "credit_initial_posted": 32,
    "credit_initial_non_posted": 32,
    "credit_initial_completion": 32
  }
}
```

### 14.2 连接(connection)示例

```json
{
  "connections": [
    // 真实 per `examples/dgpu_soc_with_pcie_ip.json:95-98`
    { "src": "host_bypass.axi_master",   "dst": "pcie_ep.axi_slave_in" },
    { "src": "pcie_ep.axi_master_out",    "dst": "xbar.0" },
    { "src": "root_complex.axi_master",  "dst": "pcie_ep.axi_slave_in" }
  ]
  // 注:仓内 JSON 端口索引语法为 `<module>.<port>.<idx>` 或 `<module>.<port>`(per AGENTS.md STRUCTURE),
  // `req_in[N]` 仅用于程序化初始化。完整示例见 `examples/dgpu_soc_with_pcie_ip.json`。
}
```

### 14.3 完整 dGPU SoC + PCIe EP 配置

完整配置示例见 [`examples/dgpu_soc_with_pcie_ip.json`](../../../examples/dgpu_soc_with_pcie_ip.json)(Phase 8 交付)。

---

## 15. ADR/微架构/OpenSpec 引用矩阵

### 15.1 关联 ADR

| ADR | 关联内容 |
|-----|----------|
| ADR-SOC-07 | Board/SOC 两层分离 + PCIe EP 归属 SOC(原 PcieEndpointTLM 4 端口冻结;现状 PcieEndpointIP 17 ports 由 Phase 4 引入) |
| ADR-SOC-11（Proposed） | PcieEndpointIP 替代 PcieEndpointTLM 完整决策 |
| ADR-SOC-12（Proposed） | Host Bypass 软件 bring-up 路径决策 |
| ADR-SOC-14（Proposed） | v5.5+ 系统级硬件仿真集成修订 |

### 15.2 关联模块微架构文档

| 模块 | 微架构文档 |
|------|-----------|
| **PcieEndpointIP** | [`docs/soc_arch/modules/dgpu-soc-pcie-slice.md`](../modules/dgpu-soc-pcie-slice.md)(旧版,4 端口) + [`docs/architecture/14-pcie-ip-microarchitecture.md`](../../architecture/14-pcie-ip-microarchitecture.md)(新版,17 端口整合) |
| **PcieLinkLayerTLM** | (待新建 `pcie-link-layer.md`) |
| **PciePhyDigitalCtrlTLM** | (待新建 `pcie-phy-digital-ctrl.md`) |
| **PcieBypassMux** | (待新建 `pcie-bypass-mux.md`) |
| **PcieSriovVfPoolTLM** | (待新建 `pcie-sriov-vf-pool.md`) |
| **Axi4StreamAdapter** | [`axi4-stream-adapter.md`](../../modules/axi4-stream-adapter.md) ✅ 已交付 |
| **Axi4Mapper** | [`axi4-mapper.md`](../../modules/axi4-mapper.md) ✅ 已交付 |
| **PcieAxiAdapter** | (待新建 `pcie-axi-adapter.md`) |
| **HostBypassTLM** | [`host-bypass.md`](../../modules/host-bypass.md) ✅ 已交付 |
| **PcieRootComplexTLM** | [`root-complex.md` (实际为 `pcie-root-complex.md`)](../../modules/pcie-root-complex.md) ✅ 已交付 |

### 15.3 关联 OpenSpec changes

8 个 OpenSpec change(已列于文档头),涵盖:
- **2026-09-01 umbrella**:PCIe IP 微架构总体
- **2026-09-08 / 09-29 / 10-06 / 10-13**:Phase 1-4 链路层 / 编码 / PHY / SR-IOV
- **2026-11-03 / 12-22**:Phase 5-6 AXI Stream Adapter / AXI4Mapper
- **2027-01-19**:Phase 7 Host Bypass + RC
- **2027-02-09**:Phase 8 整合交付(HEAD `429327d`)

---

## 16. 风险与缓解 R1-R6

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | `ch_uint<512>` 实际为 64-bit 存储(已知限制) | 🟢 低 | 已 Phase 5 M1 文档化;真实 PCIe PHY 兼容通过多次 burst 拼接 |
| **R2** | PCIe Cfg 地址编码简化(Phase8 M1 用 `awaddr` 当 offset) | 🟡 中 | 已知 Minor,未来 v1.1 按 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset` 细化 |
| **R3** | 23 ABI 头冻结 + 仓内 19/19 函数实现（+4 回调 typedef 契约）+ UsrLinuxEmu 端集成未闭环 | 🟡 中 | per `00-overview` §4-bis R26/R27;UsrLinuxEmu ADR-089 v0.5 节奏决定 |
| **R4** | PcieRootComplexTLM 仅 PF0-only(VF 需 ARI + SR-IOV capability) | 🟡 中 | Oracle M2 已文档化;v1.1 完整版追加 ARI/SR-IOV capability |
| **R5** | Phase 7 M1 修复(HostBypassTLM 4 方向 AXI 桥接)回归风险 | 🟢 低 | 已 commit `429327d` + `test_pcie_endpoint_ip_full_e2e.cc` 3 TEST_CASE PASS |
| **R6** | PcieEndpointTLM 4 端口 + PcieEndpointIP 17 ports 双轨并存 | 🟡 中 | PcieEndpointTLM 已 `[[deprecated]]`;新代码统一 17 ports |

---

## 17. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(L1 Host Interface 子系统架构,基于真实 PcieEndpointIP 17 ports + HostBypassTLM tick() 修复 429327d + PcieRootComplexTLM PF0-only 简化) |

**下次更新**:Oracle 评审反馈后 v1.1 → 归档 PASS