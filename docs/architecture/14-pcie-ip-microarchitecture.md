# docs/architecture/14-pcie-ip-microarchitecture.md
# dGPU PCIe IP 微架构文档 (Phase 1-7 整合版)
# 作者: CppTLM Team
# 日期: 2027-02-09
# 来源: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md (931 行迁移)
# Phase 7 Oracle M2 标注: RC 枚举为 PF0-only 简化模型

## 目录

1. [组件定位与分层](#1-组件定位与分层)
2. [顶层数据流(系统级)](#2-顶层数据流系统级)
3. [PcieEndpointIP 内部数据流](#3-pcieendpointip-内部数据流)
4. [链路层数据流](#4-链路层数据流)
5. [PHY 数字控制数据流](#5-phy-数字控制数据流)
6. [Bundle 定义清单](#6-bundle-定义清单)
7. [Bypass Mux 三态架构](#7-bypass-mux-三态架构)
8. [SR-IOV VF Pool 架构](#8-sr-iov-vf-pool-架构)
9. [23 ABI 兼容性边界](#9-23-abi-兼容性边界)
10. [配置 Schema](#10-配置-schema)
11. [附录 A: 与现有 PcieEndpointTLM 的迁移路径](#附录-a与现有-pcieendpointtlm-的迁移路径)

---

## 1. 组件定位与分层

`PcieEndpointIP` 是 dGPU SoC 片内 PCIe Endpoint IP 的完整 CppTLM 模型，目标行为接近真实硬件数字逻辑：链路训练 → 链路层握手 → 事务层路由 → SoC AXI 交互全流程。

### 与现有 `PcieEndpointTLM` 的关系

- 现有 `PcieEndpointTLM` (2026-08-26 archive, `include/tlm/gpu/pcie_endpoint_tlm.h`) = 仅事务层 + Config Space + BAR + MSI-X
- 新 `PcieEndpointIP` (本设计) = 现有 + 链路层 + PHY 数字控制 + SR-IOV + AXI StreamAdapter
- 现有代码作为 Phase 1-3 的事务层基础，**不删除** (向后兼容)

### 7 个内部组件 (对应设计 §3 图)

| § | 组件 | 职责 | Phase |
|---|---|---|---|
| §1 | `PciePhyDigitalCtrl` | LTSSM 11 主状态 / Gen3+ 均衡(非 Gen5 扩展,Oracle 修订) / PIPE 4-signal 数字 / 热插拔(平台事件,非 LTSSM 状态) | P3 |
| §2 | `PcieLinkLayer` | DLLP gen/parse / ACK-NAK / Retry buffer | P1 |
| §3 | `PcieTransactionLayer` (扩展现有) | TLP gen/parse / 路由 / SR-IOV VF Pool | P1 + P4 |
| §4 | `PcieConfigSpace` + `MsiXTable` (扩展现有) | Config Space / MSI-X / ARI / VF config | P4 |
| §5 | `BypassMux` | 3 态模式切换(Full / Bypass / Partial) — **P3 基础 + P7 HostBypass 独立组件** (Oracle C10 修订) | P3+P7 |
| §6 | `AxiStreamAdapter` | 多端口 AXI 适配(master + slave + cfg_slave) | P5 |
| §7 | `SoCInterconnect` (现有) | ChStream 上行 + AXI 下行交互 | 已存在 |

---

## 2. 顶层数据流(系统级)

```
┌──────────────────────── Host / Root Complex 侧 ─────────────────────────┐
│                                                                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐  ┌──────────────┐   │
│  │ Cadence VIP │  │ Synopsys VIP│  │ QEMU vPCI   │  │ 自研 RC /    │   │
│  │ (商业 RC)   │  │ (商业 RC)   │  │ (开源 RC)   │  │ HostBypass   │   │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘  └──────┬───────┘   │
│         │                │                │                │           │
│         └────────────────┴────────┬───────┴────────────────┘           │
│                                   │ (二选一)                              │
│                          ┌────────▼─────────┐                            │
│                          │  PCIe PHY BFM    │  ← 详见 §C                   │
│                          │  (数字侧接口)   │                            │
│                          └────────┬─────────┘                            │
│                                   │ PIPE 4-signal                       │
│                                   │ (rate, lanes_active,                │
│                                   │  elec_idle, training_state)          │
└───────────────────────────────────┼────────────────────────────────────┘
                                    │
                                    ▼
┌══════════════════ PcieEndpointIP CppTLM (本 change 核心) ═════════════════┐
│                                                                            │
│  §1 PHY Digital Ctrl ─► §2 Link Layer ─► §3 Transaction Layer ─► §4 Config│
│       (LTSSM)             (FC+ACK/NAK)    (TLP+SR-IOV)        Space      │
│              │                                       │                    │
│              ▼                                       ▼                    │
│         §5 Bypass Mux ──────────────────────► §6 AXI StreamAdapter       │
│      (3 态: Full/Bypass/Partial)                  (multi-port)            │
│                                                        │                  │
│                                                        ▼                  │
│                                            §7 SoC Interconnect            │
└══════════════════════════════════════════════════════════════════════════┘
                                    │
                          ┌─────────┴─────────┐
                          ▼                   ▼
               ┌──────────────────┐  ┌──────────────────┐
               │ SoC Memory Bus   │  │ SoC Internal     │
               │ (VRAM via AXI4)  │  │ Modules (CP etc.)│
               └──────────────────┘  └──────────────────┘
```

### 关键交互注释

- **PIPE 接口**: 仅 4 个信号，详见 `decisions.md` §Q3
- **AXI 接口**: 多端口(master / slave / cfg_slave)，详见 §6
- **Host Bypass**: 独立组件，见 `roadmap.md` §Phase 7
- **PHY BFM**: 开源选项见 `decisions.md` §Q4

---

## 3. PcieEndpointIP 内部数据流

```
                                 ┌────────────────────────────────────────────┐
                                 │     PcieEndpointIP CppTLM Module            │
                                 │     (ChStreamModuleBase + 多端口 adapter)  │
                                 └────────────────────────────────────────────┘
                                                           │
    PIPE 4-signal ─►                                         │
    (rate, lanes,        ┌──────────────┐                   │
    elec_idle, state)    │  §1 PHY      │                   │
         │               │  Digital Ctrl │                   │
         └──────────────►│              │                   │
                         │  - LTSSM FSM │                   │
                         │  - Equaliz.  │                   │
                         │  - Hot-plug  │                   │
                         │  - PIPE BFM  │                   │
                         │    端        │                   │
                         └──────┬───────┘                   │
                                │                           │
                                │ PHY-Ready + Link-Up      │
                                │ (速率,通道状态)            │
                                ▼                           │
                         ┌──────────────┐                   │
                         │  §2 Link     │                   │
                         │  Layer       │                   │
                         │              │                   │
                         │  - Tx Path   │                   │
                         │    TLP→128b  │                   │
                         │    DLLP→8b   │                   │
                         │  - Rx Path   │                   │
                         │    解析+     │                   │
                         │    ACK/NAK   │                   │
                         │  - FC Engine │                   │
                         │    Token     │                   │
                         │    Bucket ×N │                   │
                         │  - Retry Buf │                   │
                         └──────┬───────┘                   │
                                │                           │
                                │ TLP req/resp              │
                                │ (PcieTlpBundle)            │
                                ▼                           │
                         ┌──────────────┐                   │
                         │  §3 TX Layer │                   │
                         │  (扩展现有)   │                   │
                         │              │                   │
                         │  - TLP gen/  │                   │
                         │    parse     │                   │
                         │  - Routing   │                   │
                         │    (ID/Addr) │                   │
                         │  - SR-IOV    │                   │
                         │    VF Pool   │                   │
                         │  - Hot-plug  │                   │
                         │    事件      │                   │
                         └──────┬───────┘                   │
                                │                           │
                                │ Config read/write         │
                                │ BAR MMIO read/write       │
                                │ BAR MEM read/write        │
                                │ MSI-X pending update      │
                                ▼                           │
                         ┌──────────────┐                   │
                         │  §4 Config   │                   │
                         │  Space +     │                   │
                         │  MSI-X + ARI │                   │
                         │  (扩展现有)   │                   │
                         │              │                   │
                         │  - PF 256B/  │                   │
                         │    4KB       │                   │
                         │  - VF 4KB    │                   │
                         │    ×N (P4)   │                   │
                         │  - MSI-X     │                   │
                         │    per-VF    │                   │
                         │  - ARI cap   │                   │
                         └──────┬───────┘                   │
                                │                           │
                                │ 解码后 AXI 事务            │
                                ▼                           │
                         ┌──────────────┐                   │
                         │  §5 Bypass   │                   │
                         │  Mux         │                   │
                         │              │                   │
                         │  mode=Full:  │                   │
                         │   §1↔§2↔§3   │                   │
                         │   ↔§6        │                   │
                         │  mode=Bypass:│                   │
                         │   §3↔§6      │                   │
                         │   (跳过§1§2) │                   │
                         │  mode=Part.: │                   │
                         │   §1 skip,   │                   │
                         │   §2§3↔§6    │                   │
                         └──────┬───────┘                   │
                                │                           │
                                │ AXI 事务                  │
                                ▼                           │
                         ┌──────────────┐                   │
                         │  §6 AXI      │                   │
                         │  Stream      │                   │
                         │  Adapter     │                   │
                         │              │                   │
                         │  - axi_m_out │ ──────────────────►│──► SoC Memory Bus
                         │  - axi_s_in  │ ◄──────────────────│◄── SoC Modules
                         │  - cfg_s_in  │ ◄──────────────────│◄── SoC Admin CPU
                         │  - (可选     │                   │
                         │   AXI4Mapper │                   │
                         │   per port)  │                   │
                         └──────────────┘                   │
                                                           │
                                 ┌─────────────────────────┘
                                 │
                                 │ ChStream 上行 (PcieTlpBundle)
                                 │ MsiXDeliveryBundle
                                 ▼
                         ┌──────────────┐
                         │  §7 SoC      │  ──► soc_master_out (DMA 写完成)
                         │  Interconnect│  ◄── soc_slave_in   (上行 DMA 请求)
                         │  (现有)       │
                         └──────────────┘
```

### 内部数据流注释

1. **§1→§2**: PHY-Ready 信号触发链路进入 L0; Link-Up 后开放 DLLP/TLP 通道
2. **§2→§3**: TLP 解码后送事务层; FC Token Bucket 监控流量
3. **§3→§4**: Config / BAR 路由查询 Config Space
4. **§4→§5**: 解码后的 AXI 事务送 Bypass Mux 决定路径
5. **§5→§6**: AXI 事务按模式路由到对应 StreamAdapter 端口
6. **§6→SoC**: 经 AXI4 (或 AXI4Lite) 物理信号连接到 SoC 内部
7. **§7**: 上行 DMA 通过 soc_slave_in 进入,经 §3 包装为 PCIe TLP 发出

---

## 4. 链路层数据流

```
                         §2 Link Layer 内部详细数据流
                         ═══════════════════════════════

  ┌─────────────┐
  │ PHY Digital │  PIPE 4-signal
  │ Ctrl (§1)   │ ─────────────────────┐
  └─────────────┘                      │
                                       ▼
                               ┌─────────────────┐
                               │  PHY Adapter    │  ← 将 PIPE 4-signal 转为内部 TLM 事件
                               │  (PciePipeEvent)│
                               └────────┬────────┘
                                       │
                                       │ phy_event_t { ts, rate, elec_idle, training }
                                       ▼
    ┌───────────────────────────────────────────────────────────────────────────┐
    │                                                                          │
    │   ┌──────────────────┐              ┌──────────────────┐                 │
    │   │  Rx Path         │              │  Tx Path         │                 │
    │   │  (PHY → TL)      │              │  (TL → PHY)      │                 │
    │   │                  │              │                  │                 │
    │   │  ┌────────────┐  │              │  ┌────────────┐  │                 │
    │   │  │ 128b/130b  │  │              │  │ TLP Queue  │  │                 │
    │   │  │ Decoder    │  │              │  │ (排序)     │  │                 │
    │   │  │ (透明,     │  │              │  │            │  │                 │
    │   │  │  P2 简化) │  │              │  │ priority   │  │                 │
    │   │  └─────┬──────┘  │              │  │ Posted > NP│  │                 │
    │   │        │         │              │  │ > Cpl      │  │                 │
    │   │        ▼         │              │  └─────┬──────┘  │                 │
    │   │  ┌────────────┐  │              │        │         │                 │
    │   │  │ DLLP/TLP   │  │              │        ▼         │                 │
    │   │  │ Type       │  │              │  ┌────────────┐  │                 │
    │   │  │ Dispatch   │  │              │  │ Retry Buf  │  │ ◄──┐           │
    │   │  └─────┬──────┘  │              │  │ (重传缓冲) │  │    │           │
    │   │        │         │              │  └─────┬──────┘  │    │           │
    │   │   ┌────┴────┐    │              │        │         │    │           │
    │   │   │         │    │              │        ▼         │    │           │
    │   │   ▼         ▼    │              │  ┌────────────┐  │    │           │
    │   │ ┌─────┐ ┌─────┐  │              │  │ 128b/130b  │  │    │           │
    │   │ │TLP→ │ │DLLP→│  │              │  │ Encoder    │  │    │           │
    │   │ │  §3 │ │ACK  │  │              │  │ (透明)     │  │    │           │
    │   │ │ TX  │ │NAK  │  │              │  └─────┬──────┘  │    │           │
    │   │ │     │ │FC   │  │              │        │         │    │           │
    │   │ └──┬──┘ └──┬──┘  │              │        ▼         │    │           │
    │   │    │       │     │              │   ┌──────────┐   │    │           │
    │   │    │       │     │              │   │  PHY     │   │    │           │
    │   └────┼───────┼─────┘              │   │  Adapter │   │    │           │
    │        │       │                    │   └────┬─────┘   │    │           │
    │        │       ▼                    │        │         │    │           │
    │        │   ┌──────────────┐         │        │ PIPE 4-signal          │
    │        │   │ FC Engine    │         │        │         │    │           │
    │        │   │ Token Bucket │         │        ▼         │    │           │
    │        │   │              │         │   PHY Digital    │    │           │
    │        │   │ {P, NP, Cpl} │         │   Ctrl (§1)      │    │           │
    │        │   │ per VC       │         │                  │    │           │
    │        │   │ per VF (P4)  │         └──────────────────┘    │           │
    │        │   └──────┬───────┘                                  │           │
    │        │          │                                          │           │
    │        │          │ credit_consume / credit_update           │           │
    │        │          ▼                                          │           │
    │        │   ┌──────────────┐                                  │           │
    │        │   │ Sequence     │  12-bit seq#                     │           │
    │        │   │ Number Track │  per VC per VF                   │           │
    │        │   └──────┬───────┘                                  │           │
    │        │          │                                          │           │
    │        │          │ seq# compare on ACK/NAK DLLP             │           │
    │        └──────────┼──────────────────────────────────────────┘           │
    │                   │                                                      │
    │                   ▼                                                      │
    │            ┌──────────────┐      NAK DLLP received                       │
    │            │ Retry Buf    │ ◄────────────────────────────┘               │
    │            │ (重传缓冲)   │                                               │
    │            │              │  ACK DLLP received → 清空 retry buf          │
    │            │ per VC       │                                               │
    │            │ per VF       │                                               │
    │            └──────────────┘                                               │
    │                                                                          │
    └──────────────────────────────────────────────────────────────────────────┘
                                        │
                                        │ TLP req/resp (PcieTlpBundle)
                                        ▼
                               §3 Transaction Layer
```

### 链路层关键路径说明

| 路径 | 方向 | 触发 | 关键控制 |
|---|---|---|---|
| Rx TLP | PHY → TL | 128b/130b 解码后 | DLLP/TLP 分流;TLP 送 §3;DLLP 送 ACK/NAK 处理 |
| Rx DLLP | PHY → Engine | DLLP 类型匹配 | ACK → 清 Retry Buf;NAK → 重发;UpdateFC → 更新 Token Bucket |
| Tx TLP | TL → PHY | §3 推送 TLP | 经 TLP Queue 排序 → Retry Buf → 128b/130b 编码 → PHY |
| Tx DLLP | Engine → PHY | ACK/NAK/UpdateFC/NOP 触发 | 直接进 128b/130b 编码器 |
| FC 信用消耗 | TL → Engine | 每次 Tx TLP/DLLP | Token Bucket `consume()` 检查,不足则阻塞 |
| FC 信用补充 | Engine ← RX | UpdateFC DLLP 解析 | Token Bucket `update()` 增加 credit |

---

## 5. PHY 数字控制数据流

```
                         §1 PHY Digital Ctrl 内部详细数据流
                         ═══════════════════════════════════

                               PIPE 4-signal
                               (rate, lanes_active,
                                elec_idle, training_state)
                                       │
                                       ▼
                               ┌─────────────────┐
                               │  PIPE BFM       │
                               │  Endpoint 端    │
                               │  (数字侧)       │
                               └────────┬────────┘
                                       │ phy_event_t
                                       ▼
    ┌───────────────────────────────────────────────────────────────────┐
    │                                                                   │
    │   ┌─────────────────────────────────────────────────────────────┐ │
    │   │  LTSSM FSM (11 主状态)                                       │ │
    │   │  (Oracle 修订: Hot-Plug 是平台机制,非 LTSSM 状态)            │ │
    │   │                                                              │ │
    │   │   Detect ─► Polling ─► Configuration ─► L0                  │ │
    │   │   ▲                     │              │                     │ │
    │   │   │                     ▼              ▼                     │ │
    │   │   └─ Hot Reset ── Recovery ── L0s/L1/L2/Disabled           │ │
    │   │                                                              │ │
    │   │   (Gen3+ 均衡: TS1/TS2 + Preset 协商在 Polling/Config 内,   │ │
    │   │    Oracle 修订: 不是 Gen5 扩展,是 Gen3+ 通用特性)            │ │
    │   └───────────────────────────┬─────────────────────────────────┘ │
    │                               │                                    │
    │                               │ LTSSM state change                 │
    │                               ▼                                    │
    │   ┌─────────────────────────────────────────────────────────────┐ │
    │   │  Equalization Engine (Gen3+)                                 │ │
    │   │                                                              │ │
    │   │   ┌──────────────┐      ┌──────────────┐                    │ │
    │   │   │ TS1/TS2      │      │ Preset/Coeff │                    │ │
    │   │   │ Sequence     │◄────►│ Negotiation  │                    │ │
    │   │   │ Handler      │      │ Tables       │                    │ │
    │   │   └──────┬───────┘      └──────────────┘                    │ │
    │   │          │                                                    │ │
    │   │          │ preset select (8 presets per Gen5 spec §8.3.1)    │ │
    │   │          ▼                                                    │ │
    │   │   ┌──────────────┐                                            │ │
    │   │   │ Phase 2/3   │  ← Gen3+: Phase 2 (TX), Phase 3 (RX)        │ │
    │   │   │ Eq FSM      │                                            │ │
    │   │   └──────┬───────┘                                            │ │
    │   │          │                                                    │ │
    │   │          │ preset / coefficient update                       │ │
    │   │          ▼                                                    │ │
    │   │   ┌──────────────┐                                            │ │
    │   │   │ PIPE Rate    │  → rate = {GEN1=2, GEN2=5, GEN3=8,        │ │
    │   │   │ & Lane       │     GEN4=16, GEN5=32} GT/s per-lane           │ │
    │   │   │ Status       │  → lanes_active = 1/2/4/8/16               │ │
    │   │   │ Update       │  (Oracle 修订 #5: 原 25/50/.../400 MT/s 是 │ │
    │   │   │              │   单位错误,正确是 GT/s 档位枚举)            │ │
    │   │   └──────────────┘                                            │ │
    │   └─────────────────────────────────────────────────────────────┘ │
    │                                                                   │
    │                                                                   │
    │   ┌─────────────────────────────────────────────────────────────┐ │
    │   │  Hot-Plug State Machine (PCIe 机箱规范)                     │ │
    │   │                                                              │ │
    │   │   Signals: PWRGOOD, PERST#, REFCLK+, MRL, PRSNT#            │ │
    │   │                                                              │ │
    │   │   ┌────────────┐                                             │ │
    │   │   │ PRSNT#     │  → 物理存在检测 (低有效)                    │ │
    │   │   │ Detect     │                                             │ │
    │   │   └─────┬──────┘                                             │ │
    │   │         │                                                    │ │
    │   │         ▼                                                    │ │
    │   │   ┌────────────┐                                             │ │
    │   │   │ MRL Sensor │  → Manually-operated Retention Latch         │ │
    │   │   │ (M-RI)     │                                             │ │
    │   │   └─────┬──────┘                                             │ │
    │   │         │                                                    │ │
    │   │         ▼                                                    │ │
    │   │   ┌────────────┐                                             │ │
    │   │   │ PWRGOOD    │  → 电源稳定                                  │ │
    │   │   │ Monitor    │                                             │ │
    │   │   └─────┬──────┘                                             │ │
    │   │         │                                                    │ │
    │   │         ▼                                                    │ │
    │   │   ┌────────────┐                                             │ │
    │   │   │ REFCLK+    │  → 参考时钟就绪                             │ │
    │   │   │ Detect     │                                             │ │
    │   │   └─────┬──────┘                                             │ │
    │   │         │                                                    │ │
    │   │         ▼                                                    │ │
    │   │   ┌────────────┐                                             │ │
    │   │   │ PERST#     │  → 触发 LTSSM Detect                       │ │
    │   │   │ Deassert   │                                             │ │
    │   │   └─────┬──────┘                                             │ │
    │   │         │                                                    │ │
    │   │         ▼                                                    │ │
    │   │   LTSSM Detect ─► (链路训练)                                 │ │
    │   └─────────────────────────────────────────────────────────────┘ │
    │                                                                   │
    └───────────────────────────────────────────────────────────────────┘
                                        │
                                        │ Link-Up + Rate + Lanes
                                        ▼
                               §2 Link Layer (开放 DLLP/TLP 通道)
```

### LTSSM 状态转换表 (简化版)

| 当前状态 | 触发事件 | 下一状态 | 说明 |
|---|---|---|---|
| Detect | Receiver Detect 完成 | Polling | 物理信号检测 |
| Polling | TS1/TS2 交换完成 | Configuration | 比特/符号锁定 |
| Configuration | TS1/TS2 + 通道号协商 | L0 | 通道宽度协商 |
| L0 | 正常链路 | L0s / L1 | 电源管理触发 |
| L0s | 短空闲唤醒 | L0 | 自动恢复 |
| L1 | 深度空闲唤醒 | L0 | REFCLK+ 可关闭 |
| L2 | 电源关闭 | Detect | 仅 PERST# 唤醒 |
| Recovery | 速率切换请求 | Configuration → L0 | Gen3+ 速率变更 |
| Hot-Plug | PRSNT# 变化 | Detect | 物理插拔 |

---

## 6. Bundle 定义清单

### 6.1 已有 bundle (继承)

| Bundle | 文件 | 状态 | 用途 |
|---|---|---|---|
| `PcieTlpBundle` | `include/bundles/pcie_bundles_tlm.hh` | 现有 | host↔EP 事务(CFG/MMIO/MEM/IRQ_DELIVERY) |
| `MsiXDeliveryBundle` | `include/bundles/pcie_bundles_tlm.hh` | 现有 | MSI-X 中断投递 |

### 6.2 新增 bundle (本 change 设计)

| Bundle | 文件 | 字段 | 用途 |
|---|---|---|---|
| `PcieDllpBundle` | `include/bundles/pcie_dllp_bundles_tlm.hh` (新) | `kind`(ACK/NAK/InitFC1/InitFC2/UpdateFC/NOP/Vendor), `vc_id`(默认 0), `credit_P/NP/Cpl`(3 组,per Token Bucket), `seq_num`(12-bit), `seq_num_ack`, `trans_id` | §2 链路层 DLLP 通道 |
| `PciePipeEvent` | `include/bundles/pcie_dllp_bundles_tlm.hh` (新) | `rate`(GT/s 枚举 Gen1-5), `lanes_active`, `elec_idle`, `training_state`(11 主状态,非 Gen5 扩展), `phy_ts`(时间戳) | §1 PHY 数字事件 |
| `PciePhyConfig` | `include/bundles/pcie_dllp_bundles_tlm.hh` (新) | `max_speed`, `max_lanes`, `preset_P`, `preset_NP`, `preset_Cpl`, `sr_iov_vf_pool_size`, `hot_plug_supported` | §1 PHY 配置 |
| `Axi4Bundle` | `include/bundles/axi4_bundles_tlm.hh` (新, Phase 5) | `awaddr`, `awlen`, `awsize`, `awburst`, **`awid`**(16-bit), `wdata`, `wstrb`, `wlast`, **`bid`**(16-bit,响应 ID), `bresp`, `araddr`, `arlen`, `arsize`, `arburst`, **`arid`**(16-bit), **`rid`**(16-bit,响应 ID), `rdata`, `rresp`, `rlast` | §6 AXI4 事务(标准 AXI4,OOO 响应需 `bid`/`rid`) |
| `Axi4LiteBundle` | `include/bundles/axi4_bundles_tlm.hh` (新, Phase 5) | `awaddr`, `awid`, `wdata`, `wstrb`, `bresp`, `araddr`, `arid`, `rdata`, `rresp` | §6 AXI4Lite 事务(配置访问) |

### 6.3 Bundle 字段宽度 (全文档冻结)

| 字段 | 宽度 | 备注 |
|---|---|---|
| `PcieDllpBundle.kind` | 8 bits | 6 种 DLLP type(ACK/NAK/InitFC1/InitFC2/UpdateFC/NOP)+ Vendor-specific |
| `PcieDllpBundle.vc_id` | 4 bits | PCIe spec VC 0-7(默认 0,per Q11 单 VC) |
| `PcieDllpBundle.credit_P/NP/Cpl` | 16 bits(×3) | 3 组(P/NP/Cpl 桶),PCIe spec 12-bit credit + 4-bit 保留 |
| `PcieDllpBundle.seq_num` | 16 bits | PCIe spec 12-bit seq + 4-bit 保留(实际 wrap 在 4095) |
| `PciePipeEvent.rate` | 8 bits(枚举) | GT/s per-lane 档位:`{GEN1=2, GEN2=5, GEN3=8, GEN4=16, GEN5=32}` |
| `PciePipeEvent.lanes_active` | 8 bits | 1/2/4/8/16 |
| `PciePipeEvent.training_state` | 8 bits | 11 LTSSM 主状态(Detect/Polling/Configuration/Recovery/L0/L0s/L1/L2/Disabled/Loopback/Hot_Reset,**不含 Hot-Plug**) |
| `Axi4Bundle.awaddr` | 64 bits | 64-bit 地址空间 |
| `Axi4Bundle.wdata` | 512 bits | 64-byte burst data(Gen5 AXI 512-bit) |
| `Axi4Bundle.awid` / `arid` | 16 bits | 请求 ID(写/读独立 ID 空间) |
| `Axi4Bundle.bid` / `rid` | 16 bits | **响应 ID(必需,用于 OOO completion matching,per Oracle Top-4)** |

### 6.4 关键 Bundle 注意事项 (Oracle 修订)

- ⚠️ **Axi4Bundle OOO 支持**: spec 要求 `AXI4_MAPPER` 支持 Out-of-order completion,响应侧必须含 `bid`/`rid` 字段;AXI4 协议本身通过 `rid` 把乱序 `rdata` 关联回原事务,这是 OOO 的核心机制。
- ⚠️ **DLLP kind 数量**: 原 "4 种" 已修正为 **6+ 种**(ACK/NAK/InitFC1/InitFC2/UpdateFC/NOP,加 Vendor-specific 可选);字段 8-bit 足够扩展。
- ⚠️ **Credit 字段**: 与 Q2 Token Bucket 决策对齐(3 组:P/NP/Cpl,**非** "×6 头")。

---

## 7. Bypass Mux 三态架构

```
                          ┌──────────────────────────────┐
                          │      Bypass Mux (§5)        │
                          └──────────────────────────────┘
                                       │
                                       │ mode (JSON 配置)
                                       │
         ┌──────────────────────────────┼──────────────────────────────┐
         │                              │                              │
         ▼                              ▼                              ▼
    ┌─────────┐                    ┌─────────┐                    ┌─────────┐
    │ mode =  │                    │ mode =  │                    │ mode =  │
    │  Full   │                    │ Bypass  │                    │ Partial │
    └─────────┘                    └─────────┘                    └─────────┘
         │                              │                              │
         ▼                              ▼                              ▼
    §1 ↔ §2 ↔ §3 ↔ §6            §3 ↔ §6                       §2 ↔ §3 ↔ §6
    (完整 PCIe 链路)             (跳过 §1 + §2)                  (跳过 §1, 保留 DL)


    Full 模式数据路径(精确仿真):
    ────────────────────────────
    [Host RC] → [PHY BFM] → §1(PIPE) → §2(LL+FC) → §3(TL+路由) → §5(mux) → §6(AXI) → [SoC]
                                                   ↑               │
                                                   └─ §4(CFG/BAR/MSI-X) ←┘


    Bypass 模式数据路径(快速仿真):
    ────────────────────────────
    [Host Bypass] ←→ §3(TL) → §5(mux) → §6(AXI) → [SoC]
                     ↑
                     └─ §4(CFG/BAR/MSI-X)
                     (§1, §2 整体短路)

    注: Host Bypass 是独立组件(Phase 7),此处 Bypass 模式仅指 EP 内部 mux;
        Host Bypass 决定 Host 如何连接(直接 AXI 还是走 PCIe BFM)


    Partial 模式数据路径(平衡):
    ────────────────────────────
    [Host RC] → [PHY BFM] → §2(LL+FC) → §3(TL) → §5(mux) → §6(AXI) → [SoC]
                                 ↑              ↑
                                 └─ FC 仍然工作  └─ §4
                             (§1 PIPE/PHY Digital 短路, FC 反压保留)
```

### 模式切换的关键约束 (Oracle 修订 #7, 2026-09-01)

```cpp
// 切换前必须清理所有 pending 状态(防 R4 陷阱)
// Oracle Top-7 修订: 必须处理 in-flight 事务、seq# 失步、Partial 守卫、对端通知
class BypassMux {
    enum class DrainPolicy { GRACEFUL_DRAIN, IMMEDIATE_ABORT };
    DrainPolicy drain_policy_ = DrainPolicy::GRACEFUL_DRAIN;

    void apply_mode(BypassMode new_mode) {
        // 0. 通知对端(RC BFM / HostBypass)准备切换
        notify_peer_mode_change(new_mode);

        // 1. 暂停所有 DLLP/TLP 传输
        link_layer_.pause();

        // 2. 处理 in-flight TLP/AXI 事务(Oracle Top-7 关键)
        if (drain_policy_ == DrainPolicy::GRACEFUL_DRAIN) {
            // 等在途事务完成(有超时,默认 1µs)
            wait_for_in_flight_completion(sc_time(1, SC_US));
        } else {
            // 立即 abort + 记录中断通知
            abort_in_flight_tlps();
            record_pending_irqs();
        }

        // 3. 清理 Retry Buffer(累积确认语义,清到 ACK seq 而非全清)
        link_layer_.retry_buf_.clear_to(seq_num_last_acked_);

        // 4. ⚠️ 重置 seq# 计数器(Oracle Top-7 seq# 失步)
        seq_num_tx_ = 0;
        seq_num_rx_expected_ = 0;

        // 5. 重置 FC Token Bucket(所有 VF/VC)
        fc_engine_.reset_all_buckets();

        // 6. ⚠️ Partial 模式守卫(§1 未初始化不能 flush)
        if (new_mode == BypassMode::PARTIAL && !phy_digital_ctrl_.is_initialized()) {
            throw std::logic_error("Partial mode requires PHY Digital Ctrl initialized");
        }
        if (new_mode == BypassMode::FULL || new_mode == BypassMode::PARTIAL) {
            phy_digital_ctrl_.flush_pipe_state();
        }

        // 7. ⚠️ MSI-X pending 状态清理(Oracle Top-7 遗漏)
        msix_table_.clear_all_pending();

        // 8. 提交新模式
        mode_ = new_mode;

        // 9. 通知对端切换完成(对端可恢复传输)
        notify_peer_mode_complete(new_mode);

        // 10. 恢复传输
        link_layer_.resume();
    }

    void notify_peer_mode_change(BypassMode new_mode);
    void notify_peer_mode_complete(BypassMode new_mode);
    void wait_for_in_flight_completion(sc_time timeout);
    void abort_in_flight_tlps();
    void record_pending_irqs();
};
```

### 关键修订点

1. **In-flight 事务处理**: `DrainPolicy` 二选一,默认 GRACEFUL_DRAIN
2. **Seq# 重置**: 切换后 seq# 必须从 0 重新开始,避免对端序列号失步
3. **Partial 模式守卫**: §1 未初始化时切 Partial 必须失败
4. **MSI-X pending 清理**: 避免模式切换后中断丢失或重复投递
5. **对端通知**: RC BFM / HostBypass 需感知模式切换,否则链路会突然 silence

### 模式选择决策矩阵 (JSON 配置 `params.bypass_mode`)

| 场景 | 推荐模式 | 理由 |
|---|---|---|
| 真实 PCIe 链路训练 / 热插拔测试 | Full | 必须仿真 PHY 训练 |
| 性能分析 / 带宽建模 | Full | 链路延迟精确 |
| 软件 bring-up(driver / firmware) | Bypass | 跳过链路,加速 100-1000x |
| FC 反压 / VRAM 饱和测试 | Partial | 保留 FC,跳过 PHY 训练 |
| 单元测试 / 集成测试 | Bypass | 快速 setup/teardown |

---

## 8. SR-IOV VF Pool 架构

```
                          ┌─────────────────────────────────┐
                          │   SR-IOV VF Pool (§3 + §4)     │
                          │   (避免 16 VF × 4 ports 端口爆炸)│
                          └─────────────────────────────────┘
                                             │
                                             │ incoming TLP
                                             │ (含 Requester ID)
                                             ▼
                                   ┌──────────────────┐
                                   │  PF/VF Routing   │
                                   │  Table (per-cap) │
                                   │                  │
                                   │  PF0 → Stream 0  │
                                   │  VF0 → Stream 1  │
                                   │  VF1 → Stream 2  │
                                   │  ...             │
                                   │  VF15 → Stream 16│
                                   └────────┬─────────┘
                                            │
                                            │ stream_id routing
                                            ▼
    ┌──────────────────────────────────────────────────────────────────┐
    │  StreamAdapter Pool (单 MultiPortStreamAdapter, 共享)           │
    │                                                                  │
    │  port[0]  = PF0  AXI master out ─────────────────────► SoC Bus  │
    │  port[1]  = VF0  AXI master out ─────────────────────► SoC Bus  │
    │  port[2]  = VF1  AXI master out ─────────────────────► SoC Bus  │
    │  port[3]  = VF2  AXI master out ─────────────────────► SoC Bus  │
    │  ...                                                             │
    │  port[15] = VF14 AXI master out ────────────────────► SoC Bus  │
    │  port[16] = VF15 AXI master out ────────────────────► SoC Bus  │
    │                                                                  │
    │  (共 17 ports, 避免 N×16 端口爆炸)                              │
    └──────────────────────────────────────────────────────────────────┘

    Per-VF 内部状态(独立):
    ──────────────────────
    - VF Config Space (4KB, 独立基址)
    - VF MSI-X Table (per-VF 独立 vector 表)
    - VF BAR0/BAR1 (per-VF, 可独立配置大小)
    - VF FC Token Bucket (per-VF)
    - VF Retry Buffer (per-VF)
    - VF Sequence Number (per-VF)
```

### VF Pool 设计要点

1. **共享 StreamAdapter**: 17 个 port(PF0 + VF0..VF15),而非 17 × 4 = 68 端口
2. **内部 stream_id 路由**: 用 `stream_id` 区分 VF,避免请求/响应错位
3. **per-VF 状态独立**: Config Space / MSI-X / BAR / FC / Retry / Seq# 全部 per-VF 维护
4. **PF/VF 路由表**: 通过 PCIe ARI(Alternative Routing-ID)能力支持紧凑路由

### 配置示例 (JSON `params.sr_iov`)

```json
{
  "sr_iov": {
    "enabled": true,
    "initial_vfs": 8,
    "total_vfs": 16,
    "num_vfs": 8,
    "vf_bar0_size": 0x10000,
    "vf_bar1_size": 268435456,
    "vf_msix_vectors": 4,
    "ari_capable": true,
    "vf_pool_stream_adapter": "shared"  // 共享模式(默认)
  }
}
```

---

## 9. 23 ABI 兼容性边界 (Oracle 重大修订 #1, 2026-09-01)

### 核心原则 (per ADR-088 §D5): 23 ABI 符号必须 ABI 稳定。

### 9.0 23 ABI 真实边界 (per `include/abi/cpptlm_emulator.h` 逐行核实)

**🔴 Oracle 关键修正 (2026-09-01)**: 原版 §9 把 ABI 保护对象错位为 C++ 类方法(`noexcept`/`[[nodiscard]]`),但 **23 ABI = 19 个 C 前向函数 + 4 个 callback typedef**,完全在 `cpptlm_emulator.h` 中定义,与 `PcieEndpointTLM` / `PcieEndpointIP` 的 C++ 类**无关**。

#### 23 ABI 符号清单 (per `include/abi/cpptlm_emulator.h`)

| # | 类型 | 符号 | 用途 |
|---|---|---|---|
| 1 | 前向 | `cpptlm_emulator_get_version` | 版本查询 |
| 2 | 前向 | `cpptlm_emulator_get_device_count` | 设备计数 |
| 3 | 前向 | `cpptlm_emulator_get_device_info` | 设备信息 |
| 4 | 前向 | `cpptlm_emulator_create` | 创建实例 |
| 5 | 前向 | `cpptlm_emulator_create_by_id` | 按 ID 创建 |
| 6 | 前向 | `cpptlm_emulator_destroy` | 销毁实例 |
| 7 | 前向 | `cpptlm_emulator_mmio_write` | MMIO 写 |
| 8 | 前向 | `cpptlm_emulator_mmio_read` | MMIO 读 |
| 9 | 前向 | `cpptlm_emulator_pcie_config_write` | PCIe Config 写 |
| 10 | 前向 | `cpptlm_emulator_pcie_config_read` | PCIe Config 读 |
| 11 | 前向 | `cpptlm_emulator_backdoor_read` | backdoor 读(>=8B bulk) |
| 12 | 前向 | `cpptlm_emulator_backdoor_write` | backdoor 写 |
| 13 | 前向 | `cpptlm_emulator_msix_init` | MSI-X 初始化 |
| 14 | 前向 | `cpptlm_emulator_msix_update_pending` | MSI-X pending 更新 |
| 15 | 前向 | `cpptlm_emulator_msix_clear_pending` | MSI-X pending 清除 |
| 16 | 前向 | `cpptlm_emulator_lookup_register` | 寄存器查找 |
| 17 | 前向 | `cpptlm_emulator_register_callbacks` | 回调注册 |
| 18 | 前向 | `cpptlm_emulator_register_backdoor_cb` | backdoor 回调注册 |
| 19 | 前向 | `cpptlm_emulator_register_dma_translate_cb` | DMA 翻译回调 |
| 20-23 | typedef | `cpptlm_intr_deliver_cb_t` / `cpptlm_error_cb_t` / `cpptlm_reset_complete_cb_t` / `cpptlm_power_cb_t` | 4 个 callback 类型(per `cpptlm_emulator.h` 头文件,Oracle C1 修订;此前文档误写为 msix/backdoor/dma_translate 已纠正) |

#### 保护策略

- 本 change **NOT modify** `include/abi/cpptlm_emulator.h` (严禁)
- 23 符号的任何修改 → 走 ADR 流程 (per project convention)
- 修改 `cpptlm_emulator.h` 的 PR 必须附带 `cpptlm_emulator_abi_diff.md`

### 9.1 ABI 版本宏 (类方法层,源兼容,非二进制 ABI)

⚠️ **范围澄清**:此宏只影响 C++ 源码编译期兼容(避免旧头文件被新定义 silently 升级);**不影响** `cpptlm_emulator.so` 的 dlopen ABI。

```cpp
// include/tlm/pcie/pcie_endpoint_ip.hh(项目约定 .hh 后缀,AGENTS.md)
#ifndef CPPTLM_PCIE_ENDPOINT_ABI_VERSION
#define CPPTLM_PCIE_ENDPOINT_ABI_VERSION 2  // v2 = 链路层 + PHY 数字 + SR-IOV
#endif

// ⚠️ 修订(Oracle Top-1):删除原 `#error` 硬阻断(避免破坏 v1 消费者编译)
// 旧 v1 = 仅事务层(2026-08-26 archive);v1 消费者可继续 include,仅不获新功能
// #if CPPTLM_PCIE_ENDPOINT_ABI_VERSION != 2
// #error "ABI version mismatch"
// #endif
```

### 9.2 类方法纪律 (noexcept / [[nodiscard]],编译期源纪律)

```cpp
class PcieEndpointIP : public ChStreamModuleBase {
public:
    // 配置类
    [[nodiscard]] BypassMode mode() const noexcept;
    void set_mode(BypassMode m) noexcept;

    // 查询类
    [[nodiscard]] bool is_link_up() const noexcept;
    [[nodiscard]] uint8_t current_speed() const noexcept;
    [[nodiscard]] uint8_t active_lanes() const noexcept;

    // 状态类(per VF)
    [[nodiscard]] std::size_t vf_count() const noexcept;
    [[nodiscard]] bool vf_enabled(uint16_t vf_id) const noexcept;

    // 错误类
    [[nodiscard]] PcieError last_error() const noexcept;

    // 回调注册
    void set_link_up_callback(std::function<void()> cb) noexcept;  // 回调可抛,仅 setter noexcept
    void set_link_down_callback(std::function<void()> cb) noexcept;
};
```

⚠️ **范围澄清**:`noexcept`/`[[nodiscard]]` 是**编译期源纪律**,不改变 Itanium ABI 的 C++ mangled name 签名(对 dlopen 消费者无影响);真正二进制 ABI 边界是 §9.0 的 23 C 符号。

### 9.3 不可破坏的现有 API (`PcieEndpointTLM` 冻结)

⚠️ **Oracle Top-3 关键修正**:17 端口**不属于** `PcieEndpointTLM`(其 `num_ports() = 4` 由归档 spec 冻结),17 端口属于**新类** `PcieEndpointIP`(Phase 4 创建,独立 `REGISTER_CHSTREAM`)。

| 现有 API(`PcieEndpointTLM`) | 是否变更 |
|---|---|
| `REGISTER_CHSTREAM(PcieEndpointTLM)` | ❌ 不变(向后兼容) |
| `slave_in / mmio_out / mem_out / irq_out` 4 端口 | ❌ 不变 |
| `num_ports() = 4` | ❌ 不变(由 archive spec 冻结) |
| `set_stream_adapter(adapters[4])` 多端口注入 | ❌ 不变 |
| JSON params 子集(`config_size`, `msix_num_vectors`, `bar0_registers`, `capabilities`) | ❌ 不变(扩展新字段) |
| 成员变量布局 | ❌ 不变(避免 v1 二进制消费者崩溃) |

| 新 API(`PcieEndpointIP`, Phase 4 新增) | 备注 |
|---|---|
| `REGISTER_CHSTREAM(PcieEndpointIP)` | 新注册宏,不替换旧注册 |
| 17 端口(PF0 + VF0..VF15) | 仅新类 |
| `num_ports() = 17` | 仅新类 |
| 新 JSON params(`sr_iov`, `link_layer`, `phy_digital` 等) | 仅新类 |

### 9.4 双层 ABI 检查清单 (每个 PR 必跑)

**🔴 23 ABI 边界(C 符号)**:
- [ ] 不修改 `include/abi/cpptlm_emulator.h`
- [ ] CI 跑 `abi-compliance-checker --lib cpptlm_emulator.so` 对比 v1 vs v2

**🟡 类布局边界(C++ 类)**:
- [ ] `PcieEndpointTLM` 成员变量不增不减(冻结)
- [ ] `PcieEndpointTLM` 虚函数表不变
- [ ] CI 跑 `bloaty --symbols cpptlm_core.a` 对比 v1 vs v2

**🟢 类方法纪律(编译期)**:
- [ ] 新增 public 方法标 `noexcept` 或明确异常规范
- [ ] 查询方法标 `[[nodiscard]]`
- [ ] 不删除或重命名已有 JSON params 字段

---

## 10. 配置 Schema

### 10.1 顶层 JSON Schema (实例化 PcieEndpointIP)

```json
{
  "name": "pcie_ep",
  "type": "PcieEndpointIP",
  "params": {
    // 顶层配置(对应 §1-§7)
    "bypass_mode": "full|bypass|partial",          // §5 (默认: full)
    
    // 链路层配置(对应 §2)
    "link_layer": {
      "max_speed": "gen5",                          // Gen1-5
      "max_lanes": 16,                              // 1/2/4/8/16
      "fc_token_bucket_capacity": 256,              // Token 容量(InitFC1/InitFC2 设定上限)
      // ⚠️ Oracle 修订 C2: 删除 fc_refill_rate。credit 只能由 UpdateFC DLLP 增加(per Q2),自动 refill 会让反压永不触发
      "retry_buffer_size": 4096                     // Retry buffer 深度
    },
    
    // PHY 数字配置(对应 §1)
    "phy_digital": {
      "pipe_interface": "lightweight_4_signal",     // 锁定 4-signal
      "ltssm_initial_state": "detect",
      "equalization_preset_p": 7,                   // Gen3+ preset P
      "equalization_preset_np": 7,
      "hot_plug_supported": true,
      "perst_signal_initial": "asserted"
    },
    
    // 事务层 + SR-IOV(对应 §3, §4)
    "transaction_layer": {
      "config_size": 4096,                          // 256 或 4096
      "msix_num_vectors": 16,
      "bar0_registers": [...],                      // 沿用 v1
      "bar_sizes": [0x10000, 268435456],            // [BAR0, BAR1]
      "capabilities": [...]
    },
    "sr_iov": {
      "enabled": true,
      "initial_vfs": 8,
      "total_vfs": 16,
      "num_vfs": 8,
      "vf_bar0_size": 0x10000,
      "vf_bar1_size": 268435456,
      "vf_msix_vectors": 4,
      "ari_capable": true
    },
    
    // AXI Stream Adapter(对应 §6, Phase 5)
    "axi_adapter": {
      "data_width": 512,                            // Gen5 AXI 512-bit
      "address_width": 64,
      "axi4_mapper_inject": true,                   // 是否注入 AXI4Mapper
      "ports": {
        "axi_master_out":  {"enabled": true, "type": "axi4"},
        "axi_slave_in":    {"enabled": true, "type": "axi4"},
        "cfg_slave_in":    {"enabled": true, "type": "axi4lite"}
      }
    }
  }
}
```

### 10.2 连接(connection)示例

```json
{
  "name": "soc",
  "type": "DGpuSoc",
  "connections": [
    {"src": "pcie_ep.axi_master_out", "dst": "vram_cluster.s_axi"},
    {"src": "cp_regbank.axi_master", "dst": "pcie_ep.cfg_slave_in"},
    {"src": "pcie_ep.irq_out", "dst": "board_shell.msi_x_callback"},
    {"src": "host_bypass.axi_master", "dst": "pcie_ep.axi_slave_in"}
  ]
}
```

---

## 附录 A:与现有 PcieEndpointTLM 的迁移路径 (Oracle C4 修订)

**本设计不删除** `include/tlm/gpu/pcie_endpoint_tlm.h` (Phase 1-3 期间保留作 reference)。

**Phase 1** (LL + FC,本 change 首个子 change):
- ❌ **不**在旧 `PcieEndpointTLM` 加 `[[deprecated]]` (per Oracle Top-9 + C9)
- 新建 `include/tlm/pcie/pcie_endpoint_ip.hh` 顶部加 `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏(声明意图,非实际保护)

**Phase 4 完成后** (PcieEndpointIP 模块可用):
- 旧 `PcieEndpointTLM` 标记 `[[deprecated("Use PcieEndpointIP")]]`
- 新 `PcieEndpointIP` 作为正式名称
- 旧 `set_stream_adapter(adapters[4])` 调用方式保持兼容
- 旧 JSON params 全部继承

**整合交付 W24 末** (Phase 7 后):
- 旧 `PcieEndpointTLM` 从 `chstream_register.hh` 移除
- 新 `REGISTER_CHSTREAM(PcieEndpointIP)` 接管
- 旧代码自动迁移(由 CMake 脚本 + deprecation warning 引导)

⚠️ **迁移时间线唯一确定**:deprecated 在 P4 末(PcieEndpointIP 可用后),移除在 W24 末(整合交付)。中途 P1-P3 完全不动旧类,避免编译警告铺满。

---

## Phase 7 Oracle M2 标注

> **⚠️ RC 枚举为 PF0-only 简化模型**
> 
> 根据 Phase 7 Oracle 评审结论 (2027-01-19), `PcieRootComplexTLM::enumerate()` 的实现仅发现 PF0 (device 0, function 0)。VF (VF0..VF15) 的发现需要 ARI capability + SR-IOV capability 的完整枚举流程,当前模型简化为:枚举只报告 PF0;VF 通过 `stream_id` 直接访问配置空间 (`config_read/write(device, function, offset, stream_id)`)。这是已知边界,不构成缺陷,但在使用 `PcieRootComplexTLM` 进行枚举测试时必须注意此限制。

---

## 维护记录

| 版本 | 日期 | 作者 | 变更 |
|---|---|---|---|
| v1.0 | 2027-02-09 | CppTLM Team | 从 `openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md` (931 行) 迁移,含 Phase 7 Oracle M2 标注 |
| v1.1 | 2027-02-09 | CppTLM Team | 添加 Phase 7 Oracle M2 标注 (RC 枚举 PF0-only) |

**维护**: CppTLM Team (Sisyphus)
**状态**: 📄 Architecture — Phase 8 整合交付完成