# pcie-ip-microarch Spec — 顶层 ADDED Requirements

> **配套**: [`proposal.md`](../proposal.md) · [`design.md`](../design.md) · [`decisions.md`](../decisions.md) · [`roadmap.md`](../roadmap.md)
> **状态**: 📋 Spec — 2026-09-01
> **Parent capability**: `pcie-endpoint` (2026-08-29 archive)

## Purpose

本 spec 定义 `PcieEndpointIP` 模块(扩展当前 `PcieEndpointTLM`)的**顶层能力**。每个 ADDED Requirement 由对应的 phase 子 change 实施交付:

| Requirement | Phase | 实施 change |
|---|---|---|
| `link-layer` | P1 | `cpptlm-dgpu-pcie-link-layer-and-fc` |
| `flow-control-token-bucket` | P1 | 同上 |
| `encoding-latency-model` | P2 | `cpptlm-dgpu-pcie-130b-encoding` |
| `phy-digital-control` | P3 | `cpptlm-dgpu-pcie-phy-digital-ctrl` |
| `bypass-mux-3-mode` | P3 | 同上 |
| `sr-iov-vf-pool` | P4 | `cpptlm-dgpu-pcie-sriov` |
| `axi-stream-adapter` | P5 | `cpptlm-dgpu-axi-stream-adapter` |
| `axi4-mapper` | P6 | `cpptlm-dgpu-axi4-mapper` |
| `host-bypass-and-rc` | P7 | `cpptlm-dgpu-pcie-host-bypass-and-rc` |

## ADDED Requirements

### Requirement: link-layer

PcieEndpointIP **SHALL** 实现 PCIe 链路层(DLLP gen/parse / ACK-NAK / Retry buffer / 12-bit Sequence Number / 双向 Rx + 下行 ACK 生成),通过 `PcieLinkLayer` 组件暴露 `link_error_injector_t` API 支持 ACK/NAK retry TDD。

理由:当前 `PcieEndpointTLM` 链路层完全缺失(Board shell 注入 TLP 直接绕过)。

范围: PCIe 5.0 Base Specification §3.4(FC)/§3.5(DLLP)/§3.6(重传)。

实现位置:
- `include/tlm/pcie/pcie_link_layer_tlm.hh`
- `src/tlm/pcie/pcie_link_layer_tlm.cc`

#### Scenario: DLLP gen/parse

- **WHEN** 链路层收到任意 kind ∈ {ACK, NAK, InitFC1, InitFC2, UpdateFC, NOP, Vendor} 的 DLLP
- **THEN** 必须正确 parse 字段(vc_id, credit_P/NP/Cpl × 3, seq_num 12-bit)
- **AND** 必须按 kind 分发到对应处理路径
- **AND** 必须生成正确的对端响应 DLLP

#### Scenario: ACK/NAK retry(累积确认)

- **WHEN** 收到 ACK DLLP(seq=AckSeq)
- **THEN** Retry Buffer 中所有 seq ≤ AckSeq 的条目**累积清空**(非"全清")
- **WHEN** 收到 NAK DLLP(seq=NakSeq)
- **THEN** Retry Buffer 重发所有 seq ≥ NakSeq 的 TLP
- **AND** 12-bit seq wrap(4095 → 0)正确处理

#### Scenario: 下行(host→EP)Rx 路径(双向)

- **WHEN** host→EP 方向 TLP/DLLP 到达
- **THEN** 128b/130b 解码后 DLLP/TLP 分流
- **AND** TLP 解析后送 §3 事务层
- **AND** EP 收 TLP 后生成 ACK DLLP 发回 host(累积确认)
- **AND** EP 收方向 retry buffer 与上行独立

#### Scenario: 错误注入接口(Q15)

- **WHEN** 调用 `link_error_injector_t::inject_nak(seq=N)` / `inject_dllp_loss()` / `inject_tlp_loss(seq=N)`
- **THEN** Link Layer 在收到对应 seq 时模拟 NAK / DLLP 丢包 / TLP 丢包
- **AND** 默认 disable(无副作用),JSON `params.link_error_injection.enabled=true` 启用

---

### Requirement: flow-control-token-bucket

PcieEndpointIP **SHALL** 实现 Flow Control Token Bucket 引擎(P/NP/Cpl 各一个桶,weight + capacity;**仅 UpdateFC DLLP 补充,无自动 refill**)。

理由:当前 `PcieEndpointTLM` FC 完全缺失,无法建模 backpressure → SoC 反压失真。

范围:
- 替代 6-header credit 追踪(per [decisions.md §Q2](../decisions.md))
- 每 VF 一个桶(SR-IOV 时 per-VF bucket pool,per Q11 单 VC0)
- ⚠️ **无 `refill_rate`**:credit 单调非减,仅由 UpdateFC DLLP 增加(per PCIe spec §3.4);自动 refill 会让反压永不触发

实现位置:
- `include/tlm/pcie/pcie_flow_control_token_bucket.hh`
- `src/tlm/pcie/pcie_flow_control_token_bucket.cc`

#### Scenario: FC Token Bucket 基础行为

- **WHEN** 调用 `consume(P)` 且 token 充足(≥ weight_P)
- **THEN** 返回 true 并扣减 token
- **WHEN** 调用 `consume(P)` 且 token 不足
- **THEN** 返回 false,**不扣减** token

#### Scenario: UpdateFC 补充(唯一补充路径)

- **WHEN** 收到 UpdateFC DLLP(credit = X)
- **THEN** 调用 `update(P, X)` 增加 P 类型 token(不超过 capacity)
- **AND** token 单调非减(无自动 refill)

#### Scenario: 过载与恢复

- **WHEN** 长期 credit 不足(发送阻塞)
- **THEN** UpdateFC 到达后 token 恢复,can_send 重新返回 true
- **AND** 不会死锁(无自动 refill 路径,严格等 UpdateFC)

#### Scenario: Per-VF 桶隔离

- **WHEN** VF0 桶耗尽
- **THEN** VF1 桶不受影响
- **AND** per-VF token 独立计数

---

### Requirement: encoding-latency-model

PcieEndpointIP **SHALL** 建模 128b/130b 编码延迟(Gen1-5 各 Gen 速率对应 128B per-lane 块延迟),**NOT** 实现 bit-level 编码/解码。

理由:SoC 仿真不需要 bit-level 130b 编码;Gen5 ≠ FLIT(FLIT 是 Gen6 特性,per Librarian 修正)。

范围(Oracle 修订):
- ⚠️ 单位修正: GT/s per-lane per-direction(不是 MB/s,不是 MT/s)
- `enum class Rate { GEN1=2, GEN2=5, GEN3=8, GEN4=16, GEN5=32 }` GT/s
- 块延迟(128B / 1024-bit per-lane): Gen5 ≈ 32ns, Gen4 ≈ 64ns, Gen3 ≈ 128ns
- 速率切换延迟: ~µs 级(包含 Gen3+ 均衡协商)

实现位置:
- `include/tlm/pcie/pcie_encoding_latency_model.hh`
- `src/tlm/pcie/pcie_encoding_latency_model.cc`

#### Scenario: Gen5 链路延迟

- **WHEN** 速率 = GEN5 且 lanes = x1,128B block
- **THEN** 延迟 ≈ 32ns(1024 bit / 32 GT/s)
- **AND** 总带宽测试下吞吐 ≥ 95% 理论带宽

---

### Requirement: phy-digital-control

PcieEndpointIP **SHALL** 实现 PHY 数字控制(LTSSM 11 主状态 / Gen3+ 均衡 / PIPE 4-signal 端 / 热插拔平台事件 / Surprise Removal)。

理由:PHY 数字控制是 PCIe 链路建立/训练/热插拔的关键,当前完全"外部责任"。

范围(Oracle 修订 #8):
- LTSSM **11 主状态**:Detect / Polling / Configuration / Recovery / L0 / L0s / L1 / L2 / Disabled / Loopback / Hot_Reset
  - ⚠️ "Hot-Plug" 是**平台机制**,不是 LTSSM 状态
  - ⚠️ 均衡(Equalization)是 **Gen3+ 通用特性**,不是 Gen5 扩展
- 均衡协商: Gen3+ TS1/TS2 + 8 Preset 选择
- PIPE 4-signal 端: `(rate, lanes_active, elec_idle, training_state)`(rate 枚举 per Oracle 修订 #5)
- 热插拔(平台事件): PWRGOOD / PERST# / REFCLK+ / MRL / PRSNT#
- Surprise Removal(Q14): PRSNT# 变化时 drain(1µs) + abort + 回 Detect

实现位置:
- `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh`
- `src/tlm/pcie/pcie_phy_digital_ctrl_tlm.cc`

#### Scenario: LTSSM 状态转换

- **WHEN** LTSSM 处于 Detect 且接收检测完成
- **THEN** 转换到 Polling
- **AND** 状态转换正确触发 PIPE 4-signal 端(rate / lanes_active 更新)

#### Scenario: Gen3+ 均衡协商

- **WHEN** 速率切换到 Gen3+ 且需要均衡
- **THEN** 发送 TS1/TS2 序列 + Preset 选择(8 preset)
- **AND** Phase 2/3 EQ FSM 正确收敛

#### Scenario: 热插拔(Surprise Removal)

- **WHEN** PRSNT# 检测到 surprise removal
- **THEN** drain in-flight TLP(1µs 超时)
- **AND** abort 剩余事务 + 记录中断通知
- **AND** 链路回 Detect 状态

---

### Requirement: bypass-mux-3-mode

PcieEndpointIP **SHALL** 实现 3 态 Bypass Mux(Full / Bypass / Partial),模式切换时**MUST** flush 所有 pending 状态(in-flight TLP、retry buffer、FC token buckets、seq# 计数器、PIPE state、MSI-X pending)。

理由:SoC 仿真需要按场景切换仿真精度,避免状态泄漏(per [decisions.md §Q7](../decisions.md))。

范围:
- `mode=Full`: §1 ↔ §2 ↔ §3 ↔ §6(完整 PCIe 链路)
- `mode=Bypass`: §3 ↔ §6(跳过 §1 + §2,快速软件 bring-up)
- `mode=Partial`: §2 ↔ §3 ↔ §6(跳过 §1 PHY Digital,保留 FC)
- 状态清理清单:DrainPolicy(GRACEFUL_DRAIN / IMMEDIATE_ABORT) + in-flight + retry buffer(累积清到 ack seq) + seq# 重置 + FC 全桶重置 + Partial 守卫 + MSI-X pending + 对端通知

实现位置:
- `include/tlm/pcie/pcie_bypass_mux.hh`
- `src/tlm/pcie/pcie_bypass_mux.cc`

#### Scenario: Bypass 模式切换(全状态清理)

- **WHEN** 调用 `apply_mode(new_mode)`
- **THEN** 通知对端准备切换 → 暂停传输 → 处理 in-flight(drain 1µs 或 abort)
- **AND** Retry Buffer 清到 ack seq(累积确认,非全清)
- **AND** seq# 计数器重置 0
- **AND** FC Token Bucket 重置(所有 VF/VC)
- **AND** Partial 模式时若 §1 未初始化则 throw
- **AND** MSI-X pending 全清
- **AND** 通知对端切换完成 + 恢复传输

---

### Requirement: sr-iov-vf-pool

PcieEndpointIP **SHALL** 实现 SR-IOV VF Pool(1 PF + 最多 16 VF,共享 1 个 17-端口 MultiPortStreamAdapter,内部 stream_id 路由)。

理由:GPGPU 主流(Nvidia H100 / AMD MI300)标配 SR-IOV;per-VF 独立 Config Space / MSI-X / BAR / FC / Retry / seq#;避免 16 × 4 = 64 端口爆炸。

范围(Oracle 修订):
- VF 数可参数化(默认 8,最多 16)
- per-VF 独立: Config Space (4KB), MSI-X table, BAR0/BAR1, FC token bucket, Retry buffer, Sequence number
- ARI(Alternative Routing-ID Interpretation)能力支持 — 提供紧凑 routing-id(8-bit Function number 替代完整 16-bit BDF)
- ⚠️ **17 端口属于新类 `PcieEndpointIP`**(独立 `REGISTER_CHSTREAM`),**不**扩展旧 `PcieEndpointTLM`(其 `num_ports() == 4` 由归档 spec 冻结)
- ⚠️ FLR(Function Level Reset,Q10):PF 全状态复位,VF 仅对应 VF 状态复位

实现位置:
- `include/tlm/pcie/pcie_sriov_vf_pool_tlm.hh`
- `src/tlm/pcie/pcie_sriov_vf_pool_tlm.cc`

#### Scenario: VF Pool 路由

- **WHEN** 17 端口 StreamAdapter 收到 stream_id ∈ [0..16]
- **THEN** 正确路由到对应 PF/VF 内部状态
- **AND** PF0 = port 0, VF0 = port 1, ..., VF15 = port 16

#### Scenario: Per-VF 独立状态

- **WHEN** VF0 Config Space / MSI-X / BAR 写
- **THEN** 不影响 VF1 / PF0
- **AND** per-VF FC / Retry / seq# 独立计数

#### Scenario: FLR(Q10)

- **WHEN** host 发起 PF FLR
- **THEN** PF + 所有 VF 状态全复位
- **WHEN** host 发起 VF FLR(VFx)
- **THEN** 仅 VFx 状态复位(PF + 其他 VF 不变)

---

### Requirement: axi-stream-adapter

PcieEndpointIP **SHALL** 暴露多端口 AXI Stream Adapter(master / slave / cfg_slave),与 PcieTlpBundle 解耦。

理由:PcieEndpointIP 需要 AXI 接口与 SoC 交互(VRAM DMA / BAR MMIO / SR-IOV admin / 内部配置)。

范围:
- 3 端口: `axi_master_out`, `axi_slave_in`, `cfg_slave_in`
- Bundle 类型: `Axi4Bundle`(64B burst, Gen5 512-bit, 含 `bid`/`rid` 响应 ID per Oracle Top-4) + `Axi4LiteBundle`(config)
- JSON 可选 AXI4Mapper 注入(per [decisions.md §Q9](../decisions.md))

实现位置:
- `include/bundles/axi4_bundles_tlm.hh`
- `include/framework/axi4_stream_adapter.hh`
- `src/framework/axi4_stream_adapter.cc`

#### Scenario: AXI4 Stream Adapter 基础读写

- **WHEN** SoC 通过 axi_slave_in 写入 EP 内部寄存器
- **THEN** 写入经 cfg_slave_in → §4 Config Space
- **WHEN** EP 通过 axi_master_out 读 VRAM
- **THEN** 经 StreamAdapter 路由到 MemoryCluster

---

### Requirement: axi4-mapper

AXI4Mapper **SHALL** 是独立模块,实现 AXI4 信号 ↔ Axi4Bundle 双向映射(支持 Outstanding / Out-of-order),通过 JSON 配置注入 PcieEndpointIP 的 AXI 端口。

理由:AXI4 协议复杂(Outstanding / Out-of-order / W-channel),不应内嵌在 PcieEndpointIP。

范围(Oracle 修订 #4):
- AXI4 信号 → Axi4Bundle(适配器)
- Axi4Bundle → AXI4 信号(反向适配器)
- Outstanding transaction tracking
- Out-of-order completion handling — ⚠️ **依赖 `Axi4Bundle.bid`/`rid` 字段**(per 设计 §6.2 修订),AXI4Mapper 内部维护 `awid → bid` / `arid → rid` 映射表

实现位置:
- `include/framework/axi4_mapper.hh`
- `src/framework/axi4_mapper.cc`

#### Scenario: Outstanding + Out-of-order

- **WHEN** 同一 master 发起多个 outstanding 写/读请求
- **THEN** 完成顺序可任意,Mapper 通过 `rid` 关联 `rdata` 回原请求者

---

### Requirement: host-bypass-and-rc

CppTLM **SHALL** 提供独立 `HostBypassTLM` 组件(允许 Host 通过 AXI 直连 SoC,跳过 PCIe RC BFM + PCIe EP 数字逻辑),**AND** 可选的 `PcieRootComplexTLM` 镜像(自研 RC 模型)。

理由:软件 bring-up 阶段需要快速 Host attachment(per [decisions.md §Q7](../decisions.md));完整 dGPU 仿真需要 RC 镜像(若不用外部 VIP/QEMU)。

范围:
- `HostBypassTLM`: 把 PcieTlpBundle 直接桥接 AXI StreamAdapter
- `PcieRootComplexTLM`(可选): 完整 RC 模型(对 PcieEndpointIP 镜像对称)
- `examples/demo_pcie_full_e2e.py`: 端到端示例

实现位置:
- `include/tlm/pcie/host_bypass_tlm.hh`
- `src/tlm/pcie/host_bypass_tlm.cc`
- `include/tlm/pcie/pcie_root_complex_tlm.hh`(可选)
- `src/tlm/pcie/pcie_root_complex_tlm.cc`(可选)

#### Scenario: Host Bypass 桥接

- **WHEN** host 通过 HostBypassTLM 写 SoC 寄存器
- **THEN** 跳过 PCIe RC BFM + EP 数字逻辑
- **AND** 直接经 AXI StreamAdapter 到达 SoC

---

**维护**: CppTLM Team (Sisyphus)
**状态**: 📋 Spec — 等待评审 + Phase 1 启动实施
