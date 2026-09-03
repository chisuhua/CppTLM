# ADR-SOC-09: dGPU SoC v1.0 融合 NVIDIA + AMD 双 Driver Stack 战略

> **状态**: 📋 Proposed — 2027-02-09
> **日期**: 2027-02-09
> **Owner**: CppTLM Team (Sisyphus)
> **影响**: L3/L5/L6 双 vendor 路径 + UsrLinuxEmu 跨仓依赖标注 + 23 ABI 兼容性
> **类别**: SoC 架构 / v1.0 战略
> **关联文档**:
> - [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.1 PASS（v1.0 总架构蓝图，§1.2 关键决策 + §4-bis 范围矩阵 R01/R06/R09/R17）
> - [`docs/soc_arch/architecture/02-command-processor.md`](../architecture/02-command-processor.md)（L3 双 vendor 解码落地）
> - [`docs/soc_arch/architecture/05-sm-compute-unit.md`](../architecture/05-sm-compute-unit.md)（L6 双 vendor 内核落地）
> **关联 ADR**:
> - **ADR-SOC-04**（HSAPP/CP/Dispatcher 极简化，§4 "永不做"列表与本 ADR 部分冲突，已修订规划 Status Update）
> - **ADR-SOC-06 D5**（CommandProcessor + Pm4Decoder 双 vendor 扩展）
> - **ADR-SOC-08**（v5.5+ 系统级硬件仿真集成的前置测试契约）
> **关联 OpenSpec**: [`2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)
> **关联研究综述**:
> - [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md)（NVIDIA WDU + AMD SPI/SQ）
> - [`docs/research/CP/nvidia/overview.md`](../../research/CP/nvidia/overview.md)
> - [`docs/research/CP/amd/overview.md`](../../research/CP/amd/overview.md)
> - [`docs/research/SM/overview.md`](../../research/SM/overview.md)

---

## 1. Context（背景）

### 1.1 v1.0 战略定位

dGPU SoC v1.0 = 在 v0.5 MVP（ADR-SOC-06）+ Phase 8 PCIe EP 整合（`2027-02-09-cpptlm-dgpu-pcie-ip-integration` HEAD `429327d`）基础上，完成向**同时支持 NVIDIA + AMD 双 driver stack**的 dGPU SoC 形态跃迁。

**关键事实**：

| 维度 | v0.5 MVP | v1.0 战略 |
|------|---------|----------|
| driver stack | UsrLinuxEmu IOCTL 0x27/0x29/0x01 + 0x28 永久 -ENOSYS | CUDA + ROCm/KFD 双栈 |
| 共享 vs 双 vendor | 仅 NVIDIA 路径 | L1/L2/L4/L7 共享 + L3/L5/L6 双 vendor |

### 1.2 共享 vs 双 vendor 分层（per `00-overview` §1.2）

| 层级 | 共享/双 vendor |
|------|---------------|
| **L1 Host 接口** | 共享 |
| **L2 命令流** | 共享 |
| **L3 命令协议** | **双 vendor** |
| **L4 TMU/TMD** | 共享 |
| **L5 WDU→SM** | **双 vendor** |
| **L6 SM/CU 内核** | **双 vendor** |
| **L7 Memory/Coherence** | 共享 |

### 1.3 ADR-SOC-04 §4 的"永不做"列表与本 ADR 冲突

ADR-SOC-04 §4 明确：
- ❌ ROCm KFD 接口（Post MVP）
- ❌ AQL packet 流解析（永久不做）
- ❌ Doorbell 寄存器交互（永久不做）

**v1.0 双 vendor 战略与上述冲突**：
- ROCm KFD 接口 → v1.0 AMD 路径必需
- AQL packet 流解析 → Pm4Decoder-AMD 必需
- Doorbell 寄存器交互 → AMD Ring Buffer / Doorbell 必需

**已修订规划**（per `revision-plan-v1.0.md` A6）：in-place Status Update 标注 "Superseded partial"，保留 ADR-SOC-04 HSA Runtime 简化决策（KernelLaunchTLM ~150 行）。

### 1.4 UsrLinuxEmu 跨仓依赖

- **UsrLinuxEmu ADR-088** 当前 NVIDIA-only（per `cpptlm_emulator_*` C ABI + `cpptlm_dma_translate_cb` 契约）
- **AMD KFD 路径**：需 UsrLinuxEmu owner 联签确认 KFD IOCTL 实施时间表
- **本 ADR 不单方宣布** AMD KFD 实施时间，等 UsrLinuxEmu 反馈后追加 D4 子 ADR

---

## 2. Decision（决策）

### D1. 共享 ComputeUnitTLM 蓝图模式（双 vendor 路径）

✅ **共享 ComputeUnitTLM 蓝图模式**（per `00-overview` D2）：

```cpp
class ComputeUnitTLM {
public:
    void tick();  // 5-state FSM
    
    // 双 vendor 路径(运行时 config 切换
    bool nv_mode_;   // 默认 NVIDIA 路径
    bool amd_mode_;  // 可选 AMD 路径
};
```

- `cu_template` + `cu_count`(per `ComputeCluster`, per ADR-SOC-06)
- 所有 CUDA 与 ROCm 路径共享同一份 CU 代码
- 运行时通过 `nv_mode` / `amd_mode` config param 切换

### D2. L3/L5/L6 三层双 vendor 实现

✅ **L3 命令协议双 vendor**：

| 子模块 | 状态 | 来源 |
|--------|------|------|
| Pm4Decoder-NV(NVIDIA method packet)| ✅ v0.5 实施 | per `pm4-decoder.md` |
| Pm4Decoder-AMD(AMD PM4 TYPE3 opcode)| 🔵 v1.0 MVP 新增 | per `02-command-processor.md` §5 |

✅ **L5 WDU→SM 双 vendor**（per `00-overview` §3.5）：

| NVIDIA 路径 | AMD 路径 |
|------------|---------|
| WDU + Crossbar(US20240356866A1 DDS)| SPI 两级仲裁(US10579388B2)|
| CGA Cluster(US12333311B2, v1.1)| split workgroup(EP3785113B1)|
| 两级 WD(GPU2GPC/GPC2SM)| Wavefront 重组(US9135077B2)|
| | 多内核波前调度器(EP3803583B1)|

✅ **L6 SM/CU 双 vendor**（per `00-overview` §3.6）：

| NVIDIA 路径 | AMD 路径 |
|------------|---------|
| Warp × 4 (32 threads) | SIMD32/64 wavefront(CDNA 3 = wave64)|
| wgmma(Hopper 4 代)| MFMA Matrix Core |
| mbarrier(US20230289242A1)| HQD 两级仲裁 |
| TMA(US20230289304A1)| LDS(64KB per CU)|
| DSMEM(v1.1, US20250173152A1)| split workgroup(EP3785113B1)|
| TMEM(v1.1, arXiv 2512.02189)| (无对应) |
| Tensor Core(wgmma/tcgen05)| MFMA |

### D3. UsrLinuxEmu 跨仓依赖标注

✅ **UsrLinuxEmu 跨仓依赖标注**：

- **NVIDIA 路径**：UsrLinuxEmu 23 ABI 仓内已实现 19/19 函数 + 4 回调 typedef 契约(`src/abi/cpptlm_emulator.cc` 433 行)
- **AMD KFD 路径**：**需依赖 UsrLinuxEmu 跨仓承诺**——UsrLinuxEmu ADR-088 当前 NVIDIA-only,需 owner 联签/分阶段实施
- **本 ADR 不单方宣布** AMD KFD 实施时间表

### D4. Coherence 兼容性

✅ **Coherence 共享**（per ADR-SOC-01 + `00-overview` D8）：

- 共享 MOESI/GPU 6 状态 × 6 事件基础
- 跨域桥接 v1.0 MVP 基础 + v1.1 完整版(per `00-overview` §4-bis R23/R24)
- **NVIDIA 通常不需要 CPU↔GPU coherence**(per USRI path)
- **AMD Infinity Fabric 需要 CPU↔GPU coherence**(per ROCm path)

---

## 3. Consequences（后果）

### 3.1 正面影响

- **生态价值**：同时服务 AI/HPC(NVIDIA 主流)+ 部分 AMD 客户(ROCm 已商业化)
- **复用价值**：PcieEndpointIP/HBM/L2/NoC 共享 + 23 ABI 不变
- **风险可控**：双 vendor 内核隔离，任一路径失败不影响另一路径
- **多 L6/L5/L3 子模块**：Pm4Decoder-NV/AMD、 SPI/SQ、Wavefront/warp 各自分离

### 3.2 负面影响

- **双 vendor 内核实现工作量**：双份实现（CppTLM 通过蓝图共享降低此负担）
- **AMD 技术细节代偿**：代码库是 NVIDIA 主导，AMD 内容（KFD/AQL/PM4 TYPE3）需严格引用 `docs/research/WDUtoSM` + AMD 官方资料
- **跨仓承诺延迟**：AMD KFD 路径需 UsrLinuxEmu owner 联签

### 3.3 关键限制

- **amd_mode 启用前提**：UsrLinuxEmu 跨仓承诺达成（当前 NVIDIA-only，per ADR-088）
- **AMD 路径 v1.0 MVP 简化**：仅基础 Pm4Decoder-AMD + SPI/SQ；v1.1 完整版追加 IB 链完整 + 动态工作创建
- **双 vendor 切换**：运行时 vendor 锁定直到下次 reset（per `02-command-processor.md` §6.3）

---

## 4. Implementation（实施）

### 4.1 v1.0 MVP 实施清单（per `00-overview` §4-bis）

| R# | 特性 | v1.0 MVP |
|---|------|---------|
| R01 | driver stack | ✅ CUDA + ROCm/KFD 双栈 |
| R05 | NV ComputeUnit 路径 | ✅ 黑盒 warp + wgmma + FP8/FP16/BF16/TF32 |
| R06 | AMD ComputeUnit 路径 | ✅ 黑盒 + wave64 + MFMA + split workgroup + HQD |
| R07 | CommandProcessor 5-state FSM | ✅ 共享 |
| R08 | Pm4Decoder-NV | ✅ 4 method_addr + 18 opcodes 简化 |
| R09 | Pm4Decoder-AMD | ✅ TYPE3 opcode 基础 |
| R17 | AMD SPI→SQ | ✅ 完整路径(US10579388B2 + EP3785113B1 + US9135077B2 + EP3803583B1)|

### 4.2 v1.1 完整版追加

| R# | 特性 | v1.1 完整版 |
|---|------|------------|
| R08 | Pm4Decoder-NV | + 全 18 opcodes |
| R09 | Pm4Decoder-AMD | + 全 TYPE3 opcode + IB 链完整 |
| R05 | NV ComputeUnit | + 白盒 warp + tcgen05 + TMEM + FP4/FP6/MX |
| R06 | AMD ComputeUnit | + 白盒完整 |
| R24 | Coherence 跨域桥接 | + 完整 + snoop filter 优化 |

### 4.3 关键证据引用（必须附 `文件:行号`）

**所有数字断言必须附 `文件:行号` + grep 证据**：

- **Pm4Decoder-NV 来源**：`docs/research/CP/nvidia/overview.md` + `include/tlm/gpu/pm4_decoder_mvp.h`
- **Pm4Decoder-AMD 来源**：`docs/research/CP/amd/overview.md` + 8 件 AMD CP 专利
- **CU 蓝图模式**：`include/tlm/cluster/compute_cluster.hh` 的 `cu_template` + `cu_count` 字段
- **PDL vs CDP 区分**：PDL = CUDA 11.8+ / US20230236878A1(WSDU/pre-exit);CDP = CUDA 5.0+ / US20210349763A1(嵌套并行)

---

## 5. Risks（风险）

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | AMD KFD 跨仓承诺空头(UsrLinuxEmu NVIDIA-only)| 🟡 中 | 不单方宣布 AMD KFD 实施时间;等 UsrLinuxEmu 反馈后追加 D4 子 ADR |
| **R2** | 双 vendor 内核实现工作量 | 🟡 中 | 蓝图模式共享 CU,运行时 config 切换 |
| **R3** | AMD 技术细节代偿(KFD/AQL/PM4 TYPE3)| 🟡 中 | 严格引用 `docs/research/WDUtoSM` + AMD 官方资料 |
| **R4** | ADR-SOC-04 §4 与 v1.0 双 vendor 矛盾 | 🟢 低 | in-place Status Update 标注 "Superseded partial"(per revision-plan A6)|
| **R5** | 23 ABI 兼容性边界 | 🟢 低 | 头冻结不变量,仓内 19/19 函数实现 + 4 回调 typedef 契约 |

---

## 6. 参考文献

### 6.1 关联 ADR

| ADR | 关联 |
|-----|------|
| ADR-SOC-04 §4 | "永不做 ROCm KFD/AQL/Doorbell"列表 → 修订规划 A6 in-place Status Update |
| ADR-SOC-06 D5 | CommandProcessor + Pm4Decoder 双 vendor 扩展 |
| ADR-SOC-08 | v5.5+ 跨仓协调 + 23 ABI 现状 |

### 6.2 关联 OpenSpec changes

| Change | 关联 |
|--------|------|
| `2026-09-01-cpptlm-dgpu-pcie-ip-microarch/` | umbrella(Phase 1-7 整合)|
| `2027-02-09-cpptlm-dgpu-pcie-ip-integration/` | Phase 8 整合交付 |
| `2027-02-09-cpptlm-dgpu-soc-v1-architecture/` | v1.0 总架构蓝图 + 子架构 |

### 6.3 关联研究综述

| 综述 | 关联 |
|------|------|
| [`docs/research/WDUtoSM/overview.md`](../../research/WDUtoSM/overview.md) | NVIDIA WDU + AMD SPI/SQ |
| [`docs/research/CP/nvidia/overview.md`](../../research/CP/nvidia/overview.md) | NVIDIA CP/SM queue manager |
| [`docs/research/CP/amd/overview.md`](../../research/CP/amd/overview.md) | AMD HQD/IB/doorbell |
| [`docs/research/SM/overview.md`](../../research/SM/overview.md) | NVIDIA Hopper/Blackwell SM |

### 6.4 关联 UsrLinuxEmu ADR

| ADR | 关联 |
|-----|------|
| UsrLinuxEmu ADR-088 | dGPU complete simulation(23 ABI + dma_translate_cb)|
| UsrLinuxEmu ADR-089 v0.5 | v5.5+ 系统级硬件仿真扩展(VFIO/IOMMUFD/vDPA/Migration)|

---

## Status Update

- **2027-02-09**: 📋 Proposed。本 ADR 替代/补充 ADR-SOC-04 §4 "永不做"列表中与 v1.0 双 vendor 战略冲突的部分（ROCm KFD/AQL/Doorbell 永久不做 → v1.0 AMD 路径必需）；ADR-SOC-06 D5 提供 CommandProcessor + Pm4Decoder 基础；ADR-SOC-08 提供跨仓协调框架。AMD KFD 实施时间表需 UsrLinuxEmu owner 联签确认。