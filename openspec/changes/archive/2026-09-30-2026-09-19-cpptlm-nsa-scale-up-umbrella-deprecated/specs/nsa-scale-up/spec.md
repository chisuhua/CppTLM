# nsa-scale-up: NSA-aware Scale-Up 演进 umbrella 规范

> **所属 change**: [`2026-09-19-cpptlm-nsa-scale-up-umbrella`](../proposal.md)
> **范围**: NSA-aware Scale-Up 演进体系（9 份 NSA 草案 + 3 份 ADR + 方案 B 降级）
> **载体**: CppTLM 是 TLM 2.0 **仿真框架**（非 RTL 项目），本规范定义 TLM 功能模型 + 延时注入参数层（per ADR-SOC-23 D3 + Oracle 评审 2026-09-20）
> **SSOT**: [`docs/soc_arch/architecture/29-nsa-evolution-roadmap.md`](../../../../docs/soc_arch/architecture/29-nsa-evolution-roadmap.md) (5 阶段演进主路线)

---

## ADDED Requirements

### Requirement: nsa-fabric-address-format

The system MUST implement 64-bit NSA-aware Fabric Address 格式 = [Fabric ID(16b)][Local Addr(48b)], 与 CXL 3.0 Fabric Address 字段位分配完全对齐 (per ADR-SOC-22 D1)。

#### Scenario: 64-bit 地址格式字段对齐
- **WHEN** Stage 1 (v1.x) 启用 8-bit Fabric ID
- **THEN** Fabric ID 占 [63:56] (8 bits, 256 节点), Local Addr 占 [55:0] (56 bits)
- **AND** 与 CXL 3.0 Fabric Address 16-bit 字段完全对齐（Stage 3 启用 [63:48] 完整 16 bits）

#### Scenario: V3.1-Rev2.0 兼容
- **WHEN** Stage 1 默认 Fabric ID = 0
- **THEN** NSA-aware 关闭，与 V3.1-Rev2.0 GUPA 路由域划分完全一致
- **AND** 现有 driver 无需修改即可使用

#### Scenario: HRT Entry 32-bit 字段扩展
- **WHEN** driver 写 `CIU_REG_HRT_SHADOW[i]`
- **THEN** 32 bits 按 ADR-SOC-22 D3 分配 (route_tag:4 + vc_id:4 + qos:8 + fabric_id_lo:4 + fabric_id_hi:4 + remote_fabric:1 + capability_id:3 + tenant_id:4)

---

### Requirement: nsa-aware-mmu-tlb-model

The system MUST provide NSA-aware MMU / TLB 的 TLM 功能模型, 支持 8-bit Fabric ID 翻译 (per `25-nsa-hardware.md` §2, HW 真实电路推迟到 NSA Stage 2 由 RTL 仓实施)。

#### Scenario: Fabric ID 翻译支持
- **WHEN** `gmmu.translate(va, ctx_id, fabric_id)` 调用
- **AND** Stage 1 8-bit Fabric ID 启用
- **THEN** TLM 模型返回带 Fabric ID 的 64-bit Fabric Address
- **AND** Capability **TLM 校验路径** 与 HW 校验路径 TLM 复现并行 (HW 物理校验推迟到 Stage 2)

#### Scenario: Stage 1 TLB 不引入新数据结构
- **WHEN** TLM 模型填充 L1 TLB
- **THEN** 复用 GMMU MVP 64 entries 全关联结构 (per `20-gmmu-mvp.md` §3)
- **AND** 仅在 entry 中添加 fabric_id 字段 (per ADR-SOC-22 D4)

---

### Requirement: nsa-gsp-rm-firmware-model

The system MUST provide GSP-RM 微控制器的 TLM 功能模型 + 4 大服务固件行为仿真 (per `26-gsp-rm-firmware.md`, RTL 物理实施由 RTL 仓承接, 本仓仅仿真行为)。

#### Scenario: 4 大服务行为仿真
- **WHEN** TLM 模型模拟 GSP-RM 启动
- **THEN** Memory / Fault / Fabric / Tenant 4 大服务 TLM 行为可观测
- **AND** 模拟 RISC-V 200-400 MHz 行为级时间

#### Scenario: VirtIO Host Comm 接口
- **WHEN** Host FM 通过 VirtIO 发送 Tenant 管理命令
- **THEN** TLM 模型模拟 GSP-RM 接收、解析、执行、返回结果

---

### Requirement: nsa-capability-token

The system MUST 提供 Capability Token 128 bits 的 TLM 模型 + 校验路径 (per `27-nsa-capability.md` §2 + §4, HW 强制校验推迟到 NSA Stage 2)。

#### Scenario: 128 bits Token 字段分配
- **WHEN** driver 配置 Capability
- **THEN** Token 128 bits = [Fabric Addr Base 32b][Local Addr Mask 24b][Permissions 8b][Handle 8b][Epoch 16b][Reserved 8b][HMAC 32b]
- **AND** 字段分配 per ADR-SOC-24 D3

#### Scenario: TLM 校验路径
- **WHEN** MMU Lookup 触发 Capability 校验
- **THEN** TLM 模型并行执行 Capability 检查（与 HW 校验路径行为等价）
- **AND** 校验失败返回 -EPERM (= -1)
- **AND** HW 强制校验电路推迟到 NSA Stage 2

---

### Requirement: nsa-plan-b-fallback

The system MUST 提供 NSA-aware 硬件不就绪时的降级路径 (per ADR-SOC-23 D2 + `21-dist-scale-up-topology-b.md`)。

#### Scenario: NSA Switch 量产延期触发降级
- **WHEN** NSA Switch (CXL 3.0 Fabric Switch Tier 1) 量产延期 > 6 个月
- **THEN** Compute Tray 移除独立 CPU（合并到 Host Tray）
- **AND** Switch Tray 独立 Linux FM 协调 PCIe over UALink 软件协议栈
- **AND** 跨 tray 延时降至 1.8 μs (vs NSA-aware 500 ns, 性能损失 3.6×)

#### Scenario: Capability 在方案 B 软件层实现
- **WHEN** NSA 硬件 Capability 不可用
- **THEN** Capability 切到 Linux FM 软件层实现
- **AND** 功能保留，性能下降

---

### Requirement: nsa-cxl-3-fabric-compat

The system MUST 提供 NSA-aware 与 CXL 3.0 Fabric 兼容的 TLM 接口 (per `28-cxl-3-fabric.md` §3)。

#### Scenario: NSA Fabric ID = CXL Fabric Port ID
- **WHEN** TLM 模型在 Stage 1 处理 NSA Fabric ID
- **THEN** 8-bit Fabric ID [63:56] 与 CXL 3.0 Fabric Port ID [63:56] 字段位分配对齐
- **AND** Stage 3 启用 [63:48] 16-bit 时完全对齐

#### Scenario: Port-based Routing (PBR) 引擎
- **WHEN** TLM 模型模拟 Scale-Up Switch PBR
- **THEN** 根据 Fabric ID 路由到目标 Compute Tray / CXL Memory Pool
- **AND** HW PBR 引擎推迟到 NSA Stage 2

---

### Requirement: nsa-zero-abi-impact

The system MUST 保持 23 ABI 函数冻结不变 (per ADR-088 §D5 + ADR-020 ABI 二级精简基线), NSA Stage 1 期间 0 个新 ABI 函数。

#### Scenario: Stage 1 driver 兼容
- **WHEN** driver 调用 23 ABI 函数
- **THEN** 函数签名零修改
- **AND** driver 仅需更新 HRT/RCT 注入路径（Stage 1 仅注入 8-bit Fabric ID）

---

### Requirement: nsa-five-stage-roadmap

The system MUST 遵循 5 阶段演进路径 (per `29-nsa-evolution-roadmap.md`):

| NSA 阶段 | 时间 | TLM 仓交付物 |
|---------|------|-------------|
| NSA Stage 0 (已提交) | 2024-2026 | V3.1-Rev2.0 (方案 A) |
| NSA Stage 1 | 2026-2027 | NSA-aware MMU + GSP-RM TLM 模型 + Capability TLM 校验 (per ADR-SOC-23 D3, 仅软件层) |
| NSA Stage 2 | 2027-2028 | NSA-aware 物理硬件 + 分布式 Scale-Up (主选方案 C, RTL 仓) |
| NSA Stage 3 | 2028-2029 | CXL 3.0 Fabric 完整 (RTL 仓) |

#### Scenario: Stage 1 严格限定
- **WHEN** 进入 NSA Stage 1 实施
- **THEN** 仅交付 TLM 功能模型 + 延时注入参数
- **AND** 物理硬件 (Remote Atomic / Directory / NSA Switch CXL PHY) 由 RTL 仓承接

---

## §关联文档

- [`proposal.md`](../proposal.md) — umbrella 提案
- [`design.md`](../design.md) — umbrella 设计（实施期）
- [`docs/soc_arch/architecture/22-nsa-fabric-address-spec.md`](../../../../docs/soc_arch/architecture/22-nsa-fabric-address-spec.md) — 草案 1
- [`docs/soc_arch/architecture/29-nsa-evolution-roadmap.md`](../../../../docs/soc_arch/architecture/29-nsa-evolution-roadmap.md) — 5 阶段演进 SSOT
- [`docs/soc_arch/adr/ADR-SOC-22-nsa-fabric-address-format.md`](../../../../docs/soc_arch/adr/ADR-SOC-22-nsa-fabric-address-format.md) — ADR-SOC-22
- [`docs/soc_arch/adr/ADR-SOC-23-nsa-vs-plan-b-selection.md`](../../../../docs/soc_arch/adr/ADR-SOC-23-nsa-vs-plan-b-selection.md) — ADR-SOC-23
- [`docs/soc_arch/adr/ADR-SOC-24-capability-based-multitenant-isolation.md`](../../../../docs/soc_arch/adr/ADR-SOC-24-capability-based-multitenant-isolation.md) — ADR-SOC-24

---

## §维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v0.1-draft | Sisyphus | 首版 umbrella spec (整合 9 份草案 + 3 份 ADR) |
| 2026-09-20 | v0.2-rev | Sisyphus | Oracle 评审修订: 命名空间重命名 ADR-22/23/24 → ADR-SOC-22/23/24 + Stage 1 明确 TLM 功能模型层 (per ADR-SOC-23 D3) |