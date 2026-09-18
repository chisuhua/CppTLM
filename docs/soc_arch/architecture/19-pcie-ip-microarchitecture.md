# docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md
# dGPU PCIe IP 微架构文档 (Phase 1-7 整合版)
# 作者: CppTLM Team
# 日期: 2027-02-09 (原始 14-pcie-ip-microarchitecture.md)
# 迁移: 2027-09-17 从 `docs/architecture/14-pcie-ip-microarchitecture.md` → `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` (git mv, 历史保留)
# 来源: openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md (931 行迁移)
# Phase 7 Oracle M2 标注: RC 枚举为 PF0-only 简化模型
#
# 同目录关联文档:
# - `16-pcie-endpoint-architecture.md` — 跨仓架构 (Driver-to-Hardware)
# - `18-pcie-endpoint-entry.md` — 实施入口 (双仓 SSOT)
# - 本文档 (19) — PcieEndpointIP 内部微架构 (Phase 1-7 整合)

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
12. [Phase 9+ 完整 TLP 链路 + profile 选路](#phase-9-完整-tlp-链路--profile-选路-2026-09-17)

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

## 14.2 PcieLinkPhyMuxTLM Composite Architecture (Phase 2, 2026-09-16)

> **来源**: `openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/` (Phase 1 + Phase 2)
> **决策**: Oracle R4 = C（不引入外层 JSON `pcie_ep.pf0`/`pcie_ep.vfN` 接线）

### 14.2.1 类结构

`PcieEndpointIP` 在 Phase 2 由 `ChStreamModuleBase` 切换为 **`SimModule`**；其 17 个 TLP 端口
（1 PF + 16 VF）与 LL/PHY/Mux 三个强耦合子模块全部移入内部工厂持有的 composite child：

```
PcieEndpointIP : SimModule
├── pool_ : PcieSriovVfPool                      (值成员, 不变 — Oracle 驳回拆分)
├── bar_store_ / resizable_bars_ / power_state_  (值成员, 不变)
├── internal_factory (ModuleFactory)
│   └── PcieLinkPhyMuxTLM : ChStreamModuleBase   (17-port multiport adapter)
│       ├── PcieLinkLayer        link_
│       ├── PciePhyDigitalCtrl   phy_   (construct 后 phy_.link_layer(&link_))
│       ├── PcieBypassMux        mux_   (construct 即持 &link_)
│       ├── req_in[17] / resp_out[17]
│       └── tick(): phy → link → 17 adapters
└── tick() override: AXI slave 处理 → composite->tick()
```

| 不变量 | 落地 |
|--------|------|
| INV-1 | `phy_.link_layer(&link_)` + `set_link_up(true)` + `mux_.set_phy_initialized(true)` 全在 composite 构造函数内 |
| INV-3 | `EP::tick()` override 显式定序；**不**依赖 `SimModule` 默认 `unordered_map` 迭代序 |
| INV-4 | LL↔PHY↔Mux 保持直接指针 + `std::function` sink，**禁止** Bundle 化（rate switch / 10-step cleanup 是 0-cycle 同步调用） |
| INV-6 | 4 子模块静态注册表 API 行为保留（composite-first → legacy-fallback） |

### 14.2.2 Composite 单一所有权

composite 由 EP 的 `internal_factory->instantiateAll(wrap)` 构造，`PcieEndpointIP::composite()`
经 `internal_factory->getInstance(getName() + "_lpm")` 取得 —— **不**另持 raw 成员指针。

- Phase 1 的静态注册表 `PcieLinkPhyMuxTLM::attach_to_endpoint/detach_from_endpoint` 标注
  `[[deprecated("Phase 2 composite 迁 internal_factory; Phase 3 清理")]]`
- `PcieLinkPhyMuxTLM::for_endpoint(name)` 改为 **composite-first** wrapper：
  先调 `PcieEndpointIP::find_composite(name)`（扫描 EP 实例静态列表 + `internal_factory`），
  未命中再回落 Phase 1 静态注册表（保 `PcieEndpointTLM` 冻结路径兼容）
- EP ctor/dtor 维护 `instances_for_test()`（`std::vector<PcieEndpointIP*>`）供 `find_composite` 扫描

### 14.2.3 Composite 17 端口内部可达性（R4 决策 C）

17 个 TLP 端口**仅经程序化 API 可达**，**不**提供外层 JSON 声明式接线：

```cpp
// 程序化可达: 命中 EP internal_factory + Step 7 mirror (sim_module.hh:140-146)
auto* master = ep.getInternalOutputPort("pcie_ep_lpm.resp_out[0]");   // 非 null
auto* slave  = ep.getInternalInputPort("pcie_ep_lpm.req_in[0]");      // 非 null
```

**已知限制（R4 决策 C）**：`connection_resolver.cc` 对 `ep.<label>` → `<composite>.<port>`
的**两层下钻无递归解析支持**（只查外层 `object_instances`）。因此
`examples/dgpu_soc_with_pcie_ip.json` 中的 `pcie_ep.axi_slave_in` / `pcie_ep.axi_master_out`
接线在 Phase 2 后被静默丢弃（EP 入 `module_instances`,
`findInternalPath("axi_slave_in")==""`）并产生 `[WARN] Source/Destination port not found` ——
**这是预期的**，由 `test_axislavein_bridge_path_intact` 锁定，避免未来误判为回归。

Phase 8 真实数据路径**不**依赖该外层接线：靠 429327d 的
`HostBypassTLM`/`PcieRootComplexTLM::tick()` **程序化桥接**
（直接调 `PcieAxiAdapter::for_endpoint(ep_name)->axi()`），本 change 不破坏该路径。

**未来扩展**：若需外层声明式 TLP 接线，需扩 `connection_resolver.cc` 支持
`SimModule::getInternalOutputPort` 递归解析 —— 属 Phase 3（AxiAdapter 拆分）独立 change。

### 14.2.4 Tick 顺序契约（Phase 1 休眠 → Phase 2 激活）

| 阶段 | `EP::tick()` 行为 | PHY tick 驱动方 |
|------|------------------|----------------|
| Phase 1 | `adapters_[i]->tick()` + `for_endpoint(ll)->tick()` | 测试外部（如 `test_aspm.cc`） |
| **Phase 2** | AXI slave 处理 → `composite()->tick()`（内含 `phy → link → 17 adapters`） | **EP::tick()**（composite 内定序） |

Phase 2 审计结果：`grep -rn "phy.*->tick()" test/` 命中的
`test_aspm.cc` / `test_pcie_phy_digital_hotplug.cc` / `test_pcie_phy_digital_rate_switch.cc`
**均不创建 `PcieEndpointIP`**（走 PHY 独立静态注册表），故无双重驱动风险，**零修改**通过。
`test_pcie_endpoint_ip_simmodule_refactor.cc::[phy-migration]` 断言方向已从
"EP::tick 不推进 PHY" 翻转为 "EP::tick 推进 PHY LTSSM"。

### 14.2.5 注册迁移（R2 双注册清理）

| 项 | 变更 |
|----|------|
| `chstream_register.hh` | 删 `registerObject<PcieEndpointIP>` + `registerMultiPortAdapter<PcieEndpointIP,...,17>`；新增同等 `PcieLinkPhyMuxTLM` 两条 |
| `modules_cluster.hh` | 新增 `REGISTER_MODULE(tlm::pcie::PcieEndpointIP)` |
| `ModuleFactory::getRegisteredModuleTypes()` | 含 `"PcieEndpointIP"` |
| `ModuleFactory::getRegisteredObjectTypes()` | **不含** `"PcieEndpointIP"`（R2 缓解） |

**ABI 安全**：基类 `ChStreamModuleBase` → `SimModule` 是非虚、非 ABI 暴露成员变化；
`CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 保持；23 ABI header 零触碰（INV-5）。

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

## Phase 9+ 完整 TLP 链路 + profile 选路 (2026-09-17)

> **状态**: ✅ 完成 (11 commits, 全量 66,564 assertions, 1,470 test cases)
> **跨仓**: HSK-10 (UsrLinuxEmu ADR-088 §D5 Status Update 待 Hub ack)
> **父 spec**: `openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md`
> **proposal.md**: `openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/proposal.md`
> **tasks.md**: `openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md`

### 12.1 概览

Phase 9+ 完成 T-P9-1 到 T-P12-2 共 11 个 commit, 闭合完整 PCIe TLP 链路 (host↔EP):

- **tlp-codec**: wire-format 编解码 (CRC-32/LCRC/DLLP CRC-16)
- **completer-engine**: 替换 dispatch_tlp default no-op (full branches + CplD generation)
- **requester-engine**: MRd 发起 + tag 关联 + CompletionTracker 超时
- **bypass-tlp-data-path**: axi_bypass profile 路径
- **pcie-mock-ip**: 独立组件 (gem5-style, 无 TLP/LL/PHY 依赖)
- **profile-routing**: 4 态选路 (tlp/axi_bypass/mock/legacy)
- **dispatch-tlp-tick**: tick() 三态分派 + set_tlp_sink 真正激活
- **end-to-end-tlp**: UsrLinuxEmu ABI 路径真实产生 TLP → LL → bar_store_ → CplD

### 12.2 架构图

```
[Host] <--tx_tlp/rx_tlp--> [PcieLinkLayer] <--set_tlp_sink--> [PcieEndpointIP::dispatch_tlp_entry]
   |                              |                                    |
   |                              v                                    v
   |                       [CompleterEngine::handle_tlp] --(read)-->[CplD via tx_tlp]
   |                                                                       |
   v                                                                       v
[tx_tlp_out_] <-----------------------------------------------------------/

[RequesterEngine::mrd_read] --tx_tlp(MRd)--> [Host] --rx_tlp(CplD)--> [on_cpld_received]
[SDMA H2D desc] --> [RequesterEngine] --> MRd --> host
[MSI-X pending] --> [SriovVfPool::dispatch_msix] --> MWr --> host

[DGpuBoard mmio_write] --"pcie_path"--> {Legacy: mmio_regs_ | AxiBypass: HostBypassTLM | Tlp: PcieTlpEncoder -> LL | Mock: PcieMockIP}
[DGpuBoard mmio_read] --"pcie_path" tlp--> MRd + 读泵环(<=1000 cycle) + 超时降级 mmio_regs_

[PcieMockIP] --独立 gem5 风格--> BAR space + MSI-X (直调 cpptlm_intr_deliver_cb_t) + D3hot gate (全 BAR DECERR)
```

### 12.3 关键模块清单

| 模块 | 路径 | 状态 | 行数 |
|------|------|------|------|
| PcieTlpCodec | `include/tlm/pcie/pcie_tlp_codec.hh` `.cc` | T-P9-1 | hh 114 + cc 542 |
| PcieTlpWireBundle | `include/bundles/pcie_bundles_tlm.hh` | T-P9-2 | +128 行 |
| PcieMockIP | `include/tlm/pcie/pcie_mock_ip.hh` `.cc` | T-P9-3 | 144 + 224 (368 <=800) |
| PcieCompleterEngine | `include/tlm/pcie/pcie_completer_engine.hh` `.cc` | T-P10-1 | 89 + 292 |
| bar_store_ 三维 key | `include/tlm/pcie/pcie_endpoint_ip.hh` `.cc` | T-P10-2 | 5 处改点 |
| dispatch_tlp_entry + set_tlp_sink | `include/tlm/pcie/pcie_endpoint_ip.hh` `.cc` | T-P10-3 | tick 三态 |
| PcieRequesterEngine | `include/tlm/pcie/pcie_requester_engine.hh` `.cc` | T-P11-1 | 141 + 105 |
| SDMA H2D 接 RequesterEngine | `src/tlm/gpu/sdma_engine_tlm.cc` | T-P11-2 | +104 行 |
| MSI-X MWr 投递链 | `src/tlm/pcie/pcie_sriov_vf_pool_tlm.cc` | T-P11-3 | +74 行 |
| DGpuBoard profile 4 态 | `include/tlm/gpu/dgpu_board_shell.hh` `.cc` | T-P12-1 | +33 +74 |
| E2E 完整链路 | `test/test_pcie_endpoint_ip_tlp_path_e2e.cc` | T-P12-2 | 388 行 |

### 12.4 关键设计决策

#### 12.4.1 set_tlp_sink 真正激活 (T-P10-3 修复)

- 此前 `set_tlp_sink` 在生产代码中零调用
- T-P10-3: 在 `attach_composition` 内调 `link_layer_->set_tlp_sink(callback)` 注册 CompleterEngine 入口
- `inject_tlp_from_host_for_test` -> `dispatch_tlp_entry` -> `completer_engine_.handle_tlp(tlp)` (注意: 返回的 CplD **必须**转发回 host)

#### 12.4.2 CplD Forwarding (T-P12-2 修复)

- T-P12-2 前: `dispatch_tlp_entry` 调用 `completer_engine_.handle_tlp(tlp)` 但 **丢弃返回的 CplD**
- T-P12-2 后: `if (cpld.kind == CPLD) ll->tx_tlp(cpld, 0)` -> CplD 经 tx_tlp 发出 -> host -> 通过 `try_pop_tx_tlp()` 接收

#### 12.4.3 读泵环 (spec.md §profile-pcie-path-routing)

- tlp profile 下 `DGpuBoard::mmio_read` 是同步 C ABI
- 调用线程在 `cpptlm_emulator_mmio_read` 内循环驱动 `eq_->run()` <= 1000 虚拟周期
- 等待 CplD -> 写 `pending_data_[trans_id]` -> 回填 buf
- 超时降级 `mmio_regs_` (per 修复 #5 既有语义)
- 关键约束: `abi_call_mutex_` 保护 `pending_data_` (ABI 线程与 sim 线程并发访问)

#### 12.4.4 bar_store_ 三维 key (T-P10-2 修复 P10 技术债)

- 此前: `std::unordered_map<uint64_t, uint64_t>` (裸地址)
- 现在: `std::unordered_map<BarStoreKey, uint64_t>` (BDF x BAR x addr)
- 新增 `erase_all_for_bar(bdf, bar, new_size)` helper
- 5 处读写点全部三维化 (AXI tick 3 处 + mmio_write 1 处 + on_bar_resize 1 处)

#### 12.4.5 fc_type_for_kind CplD credit 桶修复 (T-P10-1 修潜伏 bug)

- 此前: `case IRQ_DELIVERY: default: return Posted` (CplD 误消耗 Posted credit)
- 现在: `case CPLD: return Completion` (正确路由)
- 注释更新: 删除"CplD TLP kind not in PcieTlpBundle yet"

### 12.5 测试统计

| Phase | Commit | Assertions | TEST_CASE |
|-------|--------|-----------|-----------|
| T-P9-1 | ec9abf11 | +17.7K (codec) | 11 |
| T-P9-2 | 082efcc9 | +38 (wire-bundle) | 3 |
| T-P9-3 | 2847ab66 | +29 (mock-ip) | 6 |
| T-P10-1 | d1a2239f | +20 (completer) | 6 |
| T-P10-2 | c30261b9 | +28 (bar-isolation) | 5 |
| T-P10-3 | c13fd669 | +6 (tick-dispatch) | 3 |
| T-P11-1 | 26548df5 | +37 (requester) | 5 |
| T-P11-2 | 41486360 | +12 (sdma-h2d) | 2 |
| T-P11-3 | ac0930c0 | +20 (msix-mwr) | 3 |
| T-P12-1 | 7e5a2d48 | +17 (bypass-tlp) | 3 |
| T-P12-2 | 656c194f | +33 (tlp-e2e) | 3 |
| **总计** | **11 commits** | **+18.2K 新断言** | **52 新测试** |
| **全量 baseline** | | **66,564 assertions, 1,470 test cases** | |

### 12.6 8 个 ADDED Requirements 映射

| spec.md §Requirement | 文档章节 | 实现 |
|----------------------|---------|------|
| tlp-codec-wire-format | §12.1 | PcieTlpCodec + Golden Fixture |
| completer-engine-full | §12.1 | PcieCompleterEngine + dispatch_tlp_entry 委派 |
| requester-engine-outgoing | §12.1 | PcieRequesterEngine + SDMA/MSI-X 集成 |
| bypass-tlp-data-path | §12.1 | DGpuBoard::mmio_write profile 分流 |
| pcie-mock-ip | §12.2 | PcieMockIP (gem5-style) |
| profile-pcie-path-routing | §12.2 | DGpuBoard 4 态选路 + 读泵环 |
| pcie-endpoint-tick-tlp-dispatch | §12.1 | dispatch_tlp_entry 三态 + set_tlp_sink 激活 |
| end-to-end-tlp-path | §12.5 | E2E test 验证完整链路 |

### 12.7 跨仓协调 (HSK-10)

参见 `docs/cross_repo/HSK-10-cpptlm-tlp-wire-datapath.md`:

- Hub 侧 (UsrLinuxEmu ADR-088 §D5) Status Update 已提交 (T-P9-0-pre)
- 10 工作日响应上限 (自 2027-02-10 -> 截止 2027-02-24)
- 超时 fallback: T-P9-0 切出为 follow-up `cpptlm-abi-slimming`
- 本仓 Day 12+ 才能执行 T-P9-0 (主线不受影响)
- Hub 拒绝回退策略: 待协同确定 (HSK-10 §5.3)

### 12.8 已知限制

- **ch_uint<512> = 64-bit** (per AGENTS.md KEY INVARIANTS): wire bundle 强制 `std::array<uint32_t, 1024>` 绕开
- **Read pump timeout**: 1000 虚拟周期硬上限 (per design.md §2.5 quantum 边界)
- **PcieMockIP 行数约束**: ~500 行目标, 800 行上限 (per V-5 Fz-4)
- **TLP cascade dep**: set_tlp_sink 必须先于 LLFC; dispatch_tlp_entry CplD 转发必须先于 tx_tlp

### 12.9 后续工作 (Day 12+)

| T-P | 内容 | 阻塞 |
|-----|------|------|
| T-P12-4 | CMake + 全量验证 | 无 |
| T-P9-0 | ABI 精简 (22->18 函数) | Hub ADR-088 ack |

---

## 13. EP↔SoC 桥接 (Phase 9 P2 unblock 章节, 2027-09-17)

> **状态**: 📋 **Planned** — 由 `openspec/changes/cpptlm-p2-integration-unblock/` 设计,实施 P0.5-1..9 后落地
> **来源**: spec [`pcie-ep-soc-noc-axi-bridge`](../../openspec/changes/cpptlm-p2-integration-unblock/specs/pcie-ep-soc-noc-axi-bridge/spec.md) 7 ADDED Requirements
> **Oracle 复评**: 2027-09-17 PASS-WITH-MINOR + 4/5 minor 修订完成

### 13.1 4 集成断点总览

| # | 断点 | 修复路径 | 现状 (P2 unblock 前) |
|---|------|---------|:----:|
| 1 | **Axi4Bundle ↔ CacheReqBundle 协议不兼容** | 新增 `Axi4CacheAdapter` (`include/framework/axi4_cache_adapter.hh/cc`) | ❌ 未实现 |
| 2 | **MSI-X 中断路径无连接** | EP 新增 `msix_delivery_in`/`msix_delivery_out` 双端口 | ❌ 未实现 |
| 3 | **SDMA 不在 `dgpu_soc_with_pcie_ip.json`** | EP `set_sdma_engine()` setter + tick 程序化桥接 | ❌ 未实现 |
| 4 | **CompletionRing `irq_out[3]` 未接线** | EP `set_completion_ring()` setter + tick 程序化桥接 | ❌ 未实现 |

### 13.2 Axi4CacheAdapter 桥接 (§6 AXI 边界的扩展)

**问题**: `PcieAxiAdapter` 用 `Axi4Bundle` (16-bit ID, wstrb, OOO), `CrossbarTLM` 用 `CacheReqBundle`/`CacheRespBundle` (8-bit ID, 简单行协议)。EP→SoC 读响应路径 BROKEN。

**方案**: 在 `include/framework/axi4_cache_adapter.hh/cc` 新增独立 `Axi4CacheAdapter` 类 (per design D1), 2 方向状态机:

```
AXI 写事务 (EP → SoC):
  AW (awaddr, awid[16], awlen, awsize) + W (wdata, wstrb, wlast)
    ↓ 状态机映射
  CacheReq (src_id[8], addr, cmd=WR, data, byte_en)

AXI 读事务 (EP → SoC):
  AR (araddr, arid[16], arlen)
    ↓ 状态机映射
  CacheReq (src_id[8], addr, cmd=RD)

Cache 响应 (SoC → EP):
  CacheResp (dst_id[8], data, status)
    ↓ 状态机反查
  AXI R (rid[16], rdata, rresp, rlast)  -- 读响应
  AXI B (bid[16], bresp)              -- 写响应
```

**ID 映射策略** (per design D2):
- AXI 16-bit `awid`/`arid` → CacheReq 8-bit `src_id` (高 8 bit 压缩, 低 8 bit 作 `ax_id` 保留)
- 双向映射表 `awid_to_src_id_` / `src_id_to_awid_` (capacity 256)
- 8-bit `src_id` 冲突时拒绝新事务, 返回 AXI `bresp=SLVERR`

**OOO 支持**: `Axi4CacheAdapter` 仅做协议转换, 不处理 OOO; OOO 由 PcieAxiAdapter + Axi4Mapper (Phase 6) 通过 16-bit `bid`/`rid` 字段完成。

### 13.3 MSI-X 双端口路径 (§4 Config Space + MSI-X 的扩展)

**问题**: EP 内部 `MsiXTable::irq_out` 无任何外部接线, MSI-X 投递 → Host 路径完全断。

**方案** (per design D3 + spec `msix-delivery-ep-to-host`): EP 新增 2 个独立端口:

| 端口 | 方向 | Bundle | 来源/目标 |
|------|------|--------|----------|
| `PcieEndpointIP::msix_delivery_in` | ingress | MsiXDeliveryBundle | 来自 SDMA `done_out[4]` / CompletionRing `irq_out[3]` |
| `PcieEndpointIP::msix_delivery_out` | egress | MsiXDeliveryBundle | → HostBypassTLM `msix_delivery_in` (程序化) |
| `HostBypassTLM::msix_delivery_in` | ingress | MsiXDeliveryBundle | ← EP 程序化推送 |

**数据流**:
```
SDMA 完成 → done_out[4] (PcieTlpBundle kind=DMA_DONE)
              ↓ EP.set_sdma_engine() 程序化桥接
EP.msix_delivery_in (ingress)
              ↓ EP.tick() 按 vector 编号聚合
EP.msix_delivery_out (egress)
              ↓ EP.set_host_bypass() 程序化桥接
HostBypassTLM.msix_delivery_in (ingress)
              ↓ HB.tick() 累加 msix_delivery_count_ + ABI cpptlm_emulator_msix_clear_pending()
Host 端清除 pending
```

### 13.4 SDMA 程序化桥接 (§3 Transaction Layer 的扩展)

**问题**: `SdmaEngineTLM` 不在 `examples/dgpu_soc_with_pcie_ip.json` 的 `modules` 列表中, 无法通过配置接线。当前仅依赖 `dgpu_board_shell.cc:305-330` 手动转发。

**方案** (per design D4 + spec `sdma-engine-programmatic-bridge`): 程序化桥接 (替代 JSON 声明式, 因 19 §14.2.3 限制 JSON 声明式 `pcie_ep.*` 端口被静默丢弃):

```
SDMA 现有 5 端口 (per sdma_engine_tlm.hh L82-86, 全 PcieTlpBundle):
  desc_in[0]    (ingress, PcieTlpBundle kind=DMA_DESC)   — descriptor ring
  mem_in[1]     (ingress, PcieTlpBundle)                 — VRAM 读响应
  mem_out[2]    (egress, PcieTlpBundle)                  — VRAM 读写
  host_out[3]   (egress, PcieTlpBundle)                  — Host 完成通知
  done_out[4]   (egress, PcieTlpBundle kind=DMA_DONE)    — MSI-X 中断源

SDMA 新增 2 端口 (本 change 引入, 供未来 AXI 路径扩展):
  axi_slave_in   (ingress, Axi4Bundle)
  axi_master_out (egress, Axi4Bundle)

EP 程序化桥接:
  PcieEndpointIP::set_sdma_engine(SdmaEngineTLM*) setter
  PcieEndpointIP::tick() 调用 sdma_->tick() (Phase 8 HB/RC tick 转发先例)
  DGpuBoardShell 在 board 装配时 ep->set_sdma_engine(sdma)
```

**JSON 最小化**: `dgpu_soc_with_pcie_ip.json` 仅添加 sdma 模块声明 (name, type, params), **不添加**任何 sdma ↔ pcie_ep 端口连接 (会被 §14.2.3 静默丢弃)。

### 13.5 CompletionRing 程序化桥接 (与 §13.4 共享 MSI-X 通道)

**方案** (per design D5 + spec `completion-ring-irq-out-wiring`): 与 SDMA 同模式程序化桥接:

```
CompletionRing::irq_out[3] (egress, PcieTlpBundle kind=COMPLETION)
              ↓ EP.set_completion_ring() 程序化桥接
EP.msix_delivery_in (与 SDMA done_out[4] 共享 ingress)
              ↓ EP 内部按 vector 编号聚合
EP.msix_delivery_out → HostBypassTLM.msix_delivery_in
```

**header 注释同步**: `completion_ring_mvp.hh` L13 从 `"irq_out[3] → pcie_ep.irq_out 转发"` 改为 `"irq_out[3] → pcie_ep.msix_delivery_in (程序化桥接, per 19 §14.2.3 限制)"`。

### 13.6 4 方向 AXI 闭环 (与 Phase 8 M1 关系)

| 方向 | 路径 | 状态 (本 change 后) |
|------|------|:----:|
| **D1** Host→EP 写/读 | HB→EP.axi_slave_in (Phase 8 M1 程序化闭环) | ✅ 已闭环 |
| **D2** EP→Host 响应 | EP.slave_resp→HB (Phase 8 M1 程序化闭环) | ✅ 已闭环 |
| **D3** EP→SoC 写 | EP.axi_master_out→Axi4CacheAdapter→xbar (本 change §13.2) | 📋 P0.5-2..4 |
| **D4** SoC→EP 读响应 | xbar→Axi4CacheAdapter→EP.axi_master_resp (本 change §13.2) | 📋 P0.5-2..4 |
| **MSI-X** EP→Host | EP.msix_delivery_out→HB.msix_delivery_in (本 change §13.3) | 📋 P0.5-5..6 |
| **SDMA** EP↔SDMA | EP.set_sdma_engine() 程序化桥接 (本 change §13.4) | 📋 P0.5-7 |
| **CompletionRing** EP↔CR | EP.set_completion_ring() 程序化桥接 (本 change §13.5) | 📋 P0.5-7 |

### 13.7 测试与验证 (新增 4 文件 + 双标签)

**测试文件** (per design D6):
- `test/test_axi4_cache_adapter.cc` — Axi4CacheAdapter 桥接单测 (2 方向 round-trip, OOO, error)
- `test/test_pcie_endpoint_ip_msix_path.cc` — MSI-X E2E (EP→HB)
- `test/test_pcie_endpoint_ip_sdma_wiring.cc` — SDMA 程序化桥接 E2E
- `test/test_pcie_endpoint_ip_completion_ring_wiring.cc` — CompletionRing→EP 接线 E2E

**标签策略** (per spec `pcie-ep-soc-bridge-test-tag` + G9):
- 所有 4 文件**必须**双标签 `[pcie][pcie-ep-soc-bridge]` (Catch2 标签不嵌套)
- `[pcie-ep-soc-bridge]` 精准过滤 (本 change 测试)
- `[pcie]` 回归基线 (保证 G6 数学 ≥36,700)

**验收 Gate (9 项)** (per tasks.md Acceptance Gate):
- **G1**: `openspec validate cpptlm-p2-integration-unblock --strict` PASS ✅
- **G2**: `[pcie-ep-soc-bridge]` 标签 ≥30 assertions PASS
- **G3**: 4 方向 AXI + MSI-X + SDMA + CompletionRing 全部 ✅
- **G4**: `Axi4CacheAdapter` 2 方向 round-trip PASS
- **G5**: `dgpu_soc_with_pcie_ip.json` `validate_topology` PASS
- **G6**: 既有 `[pcie]` 零回归 (实测基线 36,454, 双标签保证 ≥36,700)
- **G7**: 既有 `[chstream]` 零回归 (实测基线 155)
- **G8**: AGENTS.md + `dgpu-soc-pcie-slice.md` §9.4 + **本章节 (§13)** 文档落地 ✅
- **G9**: 4 新测试文件**双标签**验证

### 13.8 已知限制与未来扩展

- **JSON 声明式 EP 外层端口接线** 当前禁用 (per §14.2.3), 未来如需扩展需先修订本节 + 解锁 `test_axislavein_bridge_path_intact` (独立 change)
- **Axi4CacheAdapter burst 拆分**: 当前假设 Axi4StreamAdapter 已处理, 不在 Axic4 职责 (Open Q1)
- **MSI-X sequence number**: 当前 fire-and-forget (Open Q2), 未来如需 ack 可加
- **SDMA outstanding**: MVP 16, 未来可调整 (Open Q3)

---

## 维护记录

| 版本 | 日期 | 作者 | 变更 |
|---|---|---|---|
| v1.0 | 2027-02-09 | CppTLM Team | 从 `openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md` (931 行) 迁移,含 Phase 7 Oracle M2 标注 |
| v1.1 | 2027-02-09 | CppTLM Team | 添加 Phase 7 Oracle M2 标注 (RC 枚举 PF0-only) |
| v1.2 | 2026-09-16 | CppTLM Team | 新增 §14.2 PcieLinkPhyMuxTLM Composite Architecture (Phase 2, openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor) |
| v1.3 | 2026-09-17 | CppTLM Team | 新增 §12 Phase 9+ 完整 TLP 链路 + profile 选路章节 (openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath) |

**维护**: CppTLM Team (Sisyphus)
**状态**: 📄 Architecture — Phase 8 整合交付完成 · Phase 9+ TLP 链路完成