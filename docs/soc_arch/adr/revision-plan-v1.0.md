# ADR-SOC 系列修订规划 — dGPU SoC v1.0 战略下的修订框架

> **类别**: SoC Architecture > ADR 修订规划
> **状态**: 📋 Draft v2 (Metis 评审 → 修复中,2027-02-09)
> **日期**: 2027-02-09 · **作者**: CppTLM Team (Sisyphus)
> **关联 OpenSpec**: [`openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/`](../../../openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-architecture/proposal.md)（v1.0 总架构蓝图 + 子系统架构归口,已建立）+ `openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-adr-revision/`(本规划的归属 change,**待建立**)
> **关联文档**: [`docs/soc_arch/architecture/00-overview.md`](../architecture/00-overview.md) v3.0 PASS + 现有 8 份 ADR-SOC + 50+ 模块微架构 + [`docs/soc_arch/adr/README.md`](./README.md)（维护规则权威源）
> **本版本修复 Metis 评审 2027-02-09 暴露的 P0 问题**: X1 D8 重号 / X2/X4/X9 A4 去重 / X3 C6 D3 虚构 ABI 增量 / X6 C1 D1 AMD 路径 / X7 C1 D3 KFD 跨仓依赖 / M1 ADR-SOC-04 / M2 ADR-SOC-05 / 工作量 6-8 hr → 15-25 hr

---

## 0. 阅读引导

本文档是 dGPU SoC v1.0 战略下的 **ADR-SOC 修订框架**, 面向 Metis 审查与 ADR 作者:

- 想了解修订背景与整体策略 → §1(背景) + §2(策略)
- 想了解每份 ADR 修订/新建的具体动作 → §3(修订清单 A1-A7) + §4(状态追加 B) + §5(新增 C1-C6)
- 想了解执行顺序与风险 → §6(执行计划) + §7(风险与缓解)
- 想了解文档体系映射 → §8(ADR 树 v1.0) + §9(与模块微架构的映射)

---

## 1. 修订背景

### 1.1 现有 ADR-SOC 状态(2027-02-09 HEAD `429327d`)

| ADR | 主题 | 状态 | 日期 | 行数 | 关键事实 |
|-----|------|------|------|-----:|----------|
| **ADR-SOC-01** | coherence-protocol-strategy | ✅ Accepted | 2026-06-14 | 58 | 分步走(A→B):Phase 7.A-B 黑盒 GPU 直写 / Phase 7.C MOESI 6×6 状态机 |
| **ADR-SOC-02** | cu-granularity(黑盒优先) | ✅ Accepted | 2026-06-14 | 68 | CU 是 tick() 循环,emit_kernel_launch_request |
| **ADR-SOC-03** | wavefront-coalescing-abstraction | ✅ Accepted | 2026-06-14 | 64 | coalescing_factor 抽象 wavefront 内部 |
| **ADR-SOC-04** | hsapp-cp-dispatcher-simplification | ✅ Accepted | 2026-06-14 | 77 | KernelLaunchTLM ~150 行代替 HSA 3 件套,**❌ ROCm KFD 接口(Post MVP)/ ❌ AQL packet 流解析(永久不做)/ ❌ Doorbell 寄存器交互(永久不做)** |
| **ADR-SOC-05** | gpu-directory-structure | ✅ Accepted | 2026-06-14 | 72 | `include/tlm/gpu/` 子目录(未反映 pcie/ + cluster/ 后续扩展) |
| **ADR-SOC-06** | cpptlm-v05-mvp | 📋 Proposed (8 月) | 2026-08-19/修订 2026-08-20 | 506 | **8 决策 D1-D8**(注意:D8 接口稳定性已存在,L206):4 阶段 MVP + PTX-EMU 深度集成 + IOCTL 0x28 永久 -ENOSYS + CP→TMU→SQ→Cuda Core 链路 + JSON config + ABI v2 冻结 + 接口稳定性 |
| **ADR-SOC-07** | dgpu-board-soc-layering | 📋 Proposed (6 月) | 2026-08-26 | 227 | 7 决策 D1-D7:Board/SOC 两层分离 + PcieEndpointTLM(4 端口冻结)/SdmaEngineTLM 归属 SOC + Board JSON 顶层 + 23 ABI 不变 + s2 迁移路径 + Oracle 3 项记录(BAR1/s3触发器/多线程) |
| **ADR-SOC-08** | v55-system-hw-integration-preconditions | 📋 Proposed (5 月) | 2026-08-28 | 172 | 5 决策 D1-D5:接受 ADR-089 v0.5 范围 + 5 项前置测试 + 4 项 UsrLinuxEmu 跨仓提议 + change 命名 + 失败 governance |

### 1.2 v1.0 战略下的关键差距(已由 Metis 重新梳理)

| 差距 | 影响 | 关键事实 |
|------|------|---------|
| **G1** | P0 | ADR-SOC-02/03 与 ADR-SOC-06 D2 直接矛盾(CU 黑盒 vs 白盒 warp 级) |
| **G2** | P0 | 8 个 ADR 中 3 个仍 Proposed(ADR-SOC-06/07/08),长期未升 Accepted |
| **G3** | P1 | ADR-SOC-06/07 写于 4 端口 PcieEndpointTLM 时代,未反映 Phase 4+ 17 端口 PcieEndpointIP |
| **G4** | P1 | 无 ADR 反映 v1.0 融合 NVIDIA + AMD 双 driver stack 战略 |
| **G5** | P1 | Phase 1-3 PCIe 子链路(Link Layer + FC / 130b Encoding / PHY Digital Ctrl)3 项决策点无对应 ADR |
| **G6** | P1 | ADR-SOC-04 §4"永不做 ROCm/AQL/Doorbell"与 v1.0 双 vendor 直接矛盾(M1) |
| **G7** | P1 | ADR-SOC-05 目录结构过时(仅 gpu/,缺 pcie/ cluster/)(M2) |
| **G8** | P2 | 9 类 SimModule 容器无对应 ADR |
| **G9** | P2 | ADR-SOC-08 5 项前置测试当前状态未跟踪 |
| **G10** | P2 | 跨仓承诺(UsrLinuxEmu ADR-088 NVIDIA-only)与 v1.0 AMD KFD 路径依赖未声明 |

---

## 2. 修订策略

### 2.1 修订原则

1. **尊重 ADR 不可变原则**: 已签发 ADR 正文**不改**,通过 `## Status Update` 段追加状态变化;若决策需变更,**新 ADR**承载决策而非修改旧文
2. **决策与状态严格分离**:
 - **决策变更**(Decision 内容修订)→ 创建新 ADR(SOC-NN),旧 ADR 通过 Status Update 标注 "Superseded by ADR-SOC-NN"
  - **状态变化**(实施落地 / Phase 完成 / 跨仓反馈)→ 原 ADR Status Update 段追加
  - **禁止**:在 Status Update 中塞入决策性内容(违反原则 1 + 制造"矛盾 ADR")
3. **状态升级反映现实**: 长期 Proposed 但实际已实施的 ADR-SOC-06/07 → 升 Accepted + Status Update 段(仅记录 git commit / 测试 / 配置作为证据)
4. **新增 ADR 对齐 v1.0 战略**: 6 份新 ADR(09-14)覆盖融合 NVIDIA+AMD 战略 + Phase 5-8 PCIe EP 子链路 + v5.5+ 集成
5. **每份 ADR 经 Oracle 评审**: 修订/新建每份 ADR 必须经 Oracle 独立评审 PASS,作为归档前置;纯状态追加可批量合并 1 次 Oracle 评审
6. **README 索引同步**: 任何 ADR 状态变更 / 新增 / 编号变化,必须在同 commit 更新 `docs/soc_arch/adr/README.md` 索引表 + 状态图例

### 2.2 修订分级

| 级别 | 含义 | 工作量/份 | Oracle 评审 |
|------|------|-----------|------------|
| **P0 必修** | 状态升级 + 关键事实错误修复 + 矛盾 ADR 处理 | 1-2 hr | 必须(决策性);批量(纯状态) |
| **P1 强烈建议** | 新增覆盖 v1.0 战略 + Phase 5-8 子链路 | 2-4 hr | 必须(独立) |
| **P2 可选** | 9 类 SimModule + 文档体系完善 | 1-2 hr | 必须(独立) |

---

## 3. 现有 ADR 修订清单(必须)

### A1. ADR-SOC-01: coherence-protocol-strategy(状态追加) — P0

**修订类型**: 仅 `## Status Update` 段追加(正文不变)

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: Phase 7.C 部分落地(per `docs/soc_arch/modules/coherence-protocol.md` +
  `coherence-domain.md` + `coherence-bridge.md` + `coherent_xbar.md` + `snoop_filter.md`
  已实施,Phase 7.D Cache protocol 升级推迟)。
  v1.0 战略下 MOESI/GPU 6×6 状态机成为基础(per 00-overview D8 + §4-bis R23);
  跨域桥接 v1.0 MVP 基础 + v1.1 完整两阶段实施(per §4-bis R24)。
```

**Oracle 评审重点**: 状态与现实一致性 + 引用文件存在性

---

### A2. ADR-SOC-02: cu-granularity(in-place Superseded) — P0

**修订类型**: 在原文末追加 `## Status Update` 段,**不创建 rev1.md**(避免非标准命名 + 双份历史)

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: ⚠️ **Superseded by ADR-SOC-06 D2** (CU 黑盒路径 → CudaCoreAdapter
  SM 微架构探索器 + 深度集成 PTX-EMU internal + WarpState 镜像 PC/cycle)
  理由:Phase 7.B+ 黑盒 MVP 路径不足以观测 PC/cycle,与"接近真实 CudaCore"目标矛盾;
  PTX-EMU internal C++ 接口已成熟(per `docs/research/SM/overview.md` §3.2)。
  保留原文作为 v0.5 s2 历史记录;`ComputeUnitTLM::tick()` 黑盒循环在 v0.5 MVP
  仍有 1 处使用(per `docs/soc_arch/specs/apu-soc-design.md` D2)。
```

**命名修正**: 不创建 `ADR-SOC-02-rev1.md`(per README 维护规则)

---

### A3. ADR-SOC-03: wavefront-coalescing-abstraction(in-place Superseded) — P0

**修订类型**: 在原文末追加 `## Status Update` 段

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: ⚠️ **Superseded by ADR-SOC-06 D2**
  (wavefront 抽象 → WavefrontState 镜像 PC/cycle 观测)
  理由:v0.5 真实白盒需要 PC/cycle 观测,wavefront 抽象不再成立;
  `coalescing_factor` 作为 TLM 端配置参数保留用于 L2/NoC 性能建模
  (不影响 warp 级真实模拟,per `docs/soc_arch/specs/apu-soc-design.md` D3)。
```

---

### A4. ADR-SOC-06: cpptlm-v05-mvp(状态升级 + 决策性变更 → 新 ADR) — P0

**修订类型**: 状态升级 + Status Update 段,**决策变更移到 C1/C2 新 ADR**(避免 X1/X2/X4/X9 冲突)

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: ✅ Accepted(由 Proposed 升级)
  证据清单:
  - git commits: `6ebbd7d`(Phase 4 critical fix)+ `710c734..b6e5be1`(Phase 5 AXI Stream Adapter)+ `8b92bfb..fe4d745`(Phase 6 AXI4Mapper)+ `ce50b05..45763fa`(Phase 7 Host Bypass + RC)+ `e29defd..429327d`(Phase 8 整合交付)
  - 测试: `test_dgpu_board_v1_mvp_from_config.cc` 6 SECTION PASS
  - 配置: `configs/dgpu_soc_with_pcie_ip.json`(Phase 8 完整 dGPU SoC + PCIe EP 配置)
- **2027-02-09**: D5 状态补充(非决策变更):CP→TMU→SQ→Cuda Core 链路**当前**接入 PcieEndpointIP(17 ports)
  - 决策性内容详见 ADR-SOC-11(per C3 状态段)
- **2027-02-09**: 双 vendor 战略决策 → ADR-SOC-09(per C1)
- **2027-02-09**: ModuleFactory 拓扑决策 → ADR-SOC-10(per C2)
- **2027-02-09**: 决策性内容不再追加 D8/D9(避免与既有 D8 接口稳定性编号冲突)
```

---

### A5. ADR-SOC-07: dgpu-board-soc-layering(状态升级 + 决策性变更 → 新 ADR) — P0

**修订类型**: 状态升级 + Status Update 段(同 A4 策略)

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: ✅ Accepted(由 Proposed 升级)
  证据清单:
  - git commits: `4e9564e`(SR-IOV VF Pool)+ `2026-10-13`(Phase 4 整合模块)+ `429327d`(Phase 8)
  - 测试: `test_pcie_endpoint_ip_full_e2e.cc` 3 TEST_CASE 全链路 PASS
  - 配置: `examples/dgpu_soc_with_pcie_ip.json` validates_topology PASS
- **2027-02-09**: D2/D3/D6 状态补充(非决策变更):
  - PcieEndpointTLM(4 端口冻结)已 `[[deprecated]]` 标注(429327d)
  - PcieEndpointIP(17 ports)替代决策 → ADR-SOC-11(per C3)
  - 23 ABI 头冻结 + 仓内 19/19 函数实现 + 4 回调 typedef 契约(per `src/abi/cpptlm_emulator.cc`)
  - UsrLinuxEmu 集成未闭环 → ADR-SOC-14(per C6)
- **2027-02-09**: Oracle 3 项记录(BAR1 / s3 触发器 / 多线程)保留作决策记录(不变)
```

---

### A6. ADR-SOC-04: hsapp-cp-dispatcher-simplification(Status Update + 矛盾) — P0

**修订类型**: Status Update 段,**不直接修订 §4"永不做"**(尊重 ADR 不可变原则)

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: ⚠️ **Section 4 "永不做" 列表与 v1.0 双 vendor 战略直接矛盾**
  (❌ ROCm KFD / ❌ AQL packet 流解析 / ❌ Doorbell 寄存器交互 全部推迟)
  - ROCm KFD 接口 → ADR-SOC-09 D3 升级为 v1.0 跨仓依赖项(需 UsrLinuxEmu owner 联签)
  - AMD PM4 TYPE3 opcode(与 NVIDIA PM4 method packet 同源但格式不同)→ ADR-SOC-09 D2 双 decoder
  - Doorbell 寄存器交互 → 仍为 NVIDIA 路径核心(0x28 永久 -ENOSYS),AMD 路径仅接口预留
  - HSA Runtime 三件套(~3000 行) → KernelLaunchTLM ~150 行简化决策**保留**(不冲突)
  决策性变更不写入本 ADR 正文;需双 vendor 战略落地的具体决策请参考 ADR-SOC-09。
```

---

### A7. ADR-SOC-05: gpu-directory-structure(Status Update 补充) — P1

**修订类型**: Status Update 段

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: 目录结构已扩展,实际包含 3 个子目录:
  - `include/tlm/gpu/` (本 ADR 决策,保留)
  - `include/tlm/pcie/` (2026-09..2027-02 Phase 1-7 PCIe EP 子链路新增,17+ 文件)
  - `include/tlm/cluster/` (2026-06..2026-08 Phase 8 SimModule 9 类 P2-P5 容器,ApuSoC 顶层)
  `include/tlm/` 顶层目录决策保留;新增子目录决策详见 ADR-SOC-10(per C2)。
```

---

## 4. 现有 ADR 状态追加(强烈建议)

### B. ADR-SOC-08: v55-system-hw-integration-preconditions(状态追加) — P1

**修订类型**: 仅 `## Status Update` 段追加

**追加内容**(2027-02-09):
```
## Status Update

- **2027-02-09**: 5 项前置测试当前状态(全部已实施,per Phase 8):
  - P1 MSI-X 状态机细粒度(mask/unmask/PBA):**已实施** ✅
    测试: `test/test_pcie_endpoint_msix_state.cc`
  - P2 wire-format 跨仓字节级一致性:**已实施** ✅
    测试: `test/test_pcie_slice_wire_format_snapshot.cc`
  - P3 DMA translate callback 边界(phys 越界):**已实施** ✅
    测试: `test/test_sdma_engine_iommu_fault.cc`
  - P4 Doorbell 排队/并发:**已实施** ✅
    测试: `test/test_pcie_endpoint_doorbell_queue.cc`
  - P5 backdoor ABI 隔离(MMIO 与 backdoor 路径):**已实施** ✅
    测试: `test/test_sdma_engine_backdoor_isolation.cc`
- **2027-02-09**: 4 项 UsrLinuxEmu 跨仓提议反馈状态(全部待回应):
  - 1 ADR-089 §D7 IOMMUFD consumer 追加 CppTLM 跨仓契约验证段:**待 UsrLinuxEmu 回应**
  - 2 ADR-089 §D7 VFIO consumer BAR0 寄存器行为契约来源:**待 UsrLinuxEmu 回应**
  - 3 ADR-088 §C2 CppTLM C ABI 范围扩展:**待 UsrLinuxEmu 回应**
  - 4 ADR-089 v0.5 更新到 v5.5+ 节奏:**待 UsrLinuxEmu 回应**
- **2027-02-09**: 仓内 23 ABI 实现进展:
  - 头冻结:✅(per `include/abi/cpptlm_emulator.h` 19 声明)
  - 仓内实现:✅(per `src/abi/cpptlm_emulator.cc` 433 行,19/19 函数 + 4 回调 typedef 契约)
  - UsrLinuxEmu 侧集成与完整 23 函数闭环:❌(待 UsrLinuxEmu v5.5+ 节奏)
```

**Oracle 评审重点**: 5 项前置测试状态与 git log / 测试报告的一致性

---

## 5. 新增 ADR 清单(强烈建议)

### C1. ADR-SOC-09: dGPU SoC v1.0 融合 NVIDIA + AMD 双 vendor 战略 — P1

**主题**: v1.0 战略 = 同时支持 CUDA + ROCm 双 driver stack + 共享 PCIe/HBM/NoC + 双 vendor 内核

**关键修订(per Metis X6/X7)**:
- **D1**: AMD 路径与 NVIDIA 路径**共享 ComputeUnitTLM 蓝图**(`cu_template` + `nv_mode/amd_mode` config 切换,per `00-overview` D2 + §4-bis R04),**不是**"AMD 仅 BAR1+Doorbell+MMIO"
- AMD 计算路径 v1.0 MVP 实施内容: **R06(AMD ComputeUnit ✅)/ R09(Pm4Decoder-AMD ✅)/ R17(AMD SPI→SQ ✅)**(per `00-overview` §4-bis)
- **D2**: 两类 driver stack 的 ABI 隔离(per `00-overview` §1.2 L3 共享 + L5/L6 双 vendor)
  - L3 双 vendor: NVIDIA PM4 method packet(per US10489056B2) + AMD PM4 TYPE3 opcode(per US8310492B2)
  - L5 双 vendor: NVIDIA WDU + Crossbar(per US20240356866A1) + CGA Cluster(per US12333311B2) vs AMD SPI→SQ(per US10579388B2) + split workgroup(per EP3785113B1)
  - L6 双 vendor: NVIDIA Warp + wgmma(per US20230289398A1) + mbarrier(per US20230289242A1) + TMA(per US20230289304A1) vs AMD SIMD32/64 wavefront + split workgroup + load-rating(per EP3785113B1)
- **D3**: UsrLinuxEmu IOCTL 分发(跨仓依赖标注):
  - NVIDIA 路径 0x27/0x29/0x01 已实施 + 0x28 永久 -ENOSYS(per ADR-SOC-06 D5)
  - AMD KFD 路径**需依赖 UsrLinuxEmu 跨仓承诺**——UsrLinuxEmu ADR-088 当前 NVIDIA-only,需 owner 联签/分阶段实现
  - **本 ADR 不单方宣布 KFD 实施时间表**,等 UsrLinuxEmu 反馈后追加 D4 子 ADR
- **D4**: Coherence 兼容性(Nvidia 通常不需要 coherence;AMD Infinity Fabric 需要)
  - 共享 MOESI/GPU 基础(per ADR-SOC-01 + `00-overview` D8)
  - 跨域桥接 v1.0 MVP 基础 + v1.1 完整(per `00-overview` §4-bis R23/R24)

**证据附录(强制)**:
- v1.0 双 vendor 路径:**所有数字断言必须附 `文件:行号` + grep 证据**(如"PM4 TYPE3 opcode: per `docs/research/CP/amd/overview.md` L30-50" + 实际文件存在)
- AMD ComputeUnit 蓝图:**引用** `include/tlm/cluster/compute_cluster.hh` 的 `cu_template + cu_count` 字段
- PDL vs CDP 区分: PDL = CUDA 11.8+ / US20230236878A1(per `00-overview` v3.0 §1.4);CDP = CUDA 5.0+ / US20210349763A1

**Oracle 评审重点**: AMD 路径与 00-overview §4-bis R06/R09/R17 一致性 + UsrLinuxEmu 跨仓依赖标注 + 证据附录完整性

---

### C2. ADR-SOC-10: dGPU SoC v1.0 ModuleFactory 拓扑层 — P1

**主题**: 9 类 SimModule P2-P5 层级容器 + ApuSoC 顶层

**决策点**(D1-D4):
- D1: 单一入口 JSON 拓扑(`dgpu_soc_with_pcie_ip.json` 已示范,`ls configs/` 验证)
- D2: SimModule 多层容器(`include/tlm/cluster/`:`ApuSoC` → `GpuCluster` → `TpcCluster` → `ComputeCluster` → `CU`,共 9 类)
- D3: PcieEndpointIP 作为 17 ports ChStreamModuleBase(`req_in[17] + resp_out[17]` + 内部 `stream_id` 路由),与 HostBypassTLM / PcieRootComplexTLM 通过静态注册表桥接
- D4: JSON `axi4_mapper_inject: true` 可选注入 OOO Mapper(per `examples/dgpu_soc_with_pcie_ip.json`)

**证据附录**:
- `include/tlm/cluster/` 9 文件 + 1 ApuSoC 顶层文件:`ls include/tlm/cluster/` 验证
- `include/modules_cluster.hh` 集中注册 9 个 SimModule 派生类

---

### C3. ADR-SOC-11: PcieEndpointIP 替代 PcieEndpointTLM — P1

**主题**: 17 ports PcieEndpointIP 整合模块替代 4 端口 PcieEndpointTLM

**决策点**(D1-D4):
- D1: PcieEndpointTLM 加 `[[deprecated("use PcieEndpointIP")]]`(已实施,429327d;`include/tlm/gpu/pcie_endpoint_tlm.h`)
- D2: `chstream_register.hh` 保留 PcieEndpointTLM 注册(既有 Phase 4 测试 + `dgpu_board_shell.cc` 依赖)
- D3: 新代码统一使用 PcieEndpointIP(per `include/tlm/pcie/pcie_endpoint_ip.hh` 17 ports)
- D4: 23 ABI 冻结(per `include/tlm/gpu/pcie_endpoint_tlm.h` layout 不变,仅加属性;per ADR-088 §D5)

**证据附录**:
- D1: `git log -p include/tlm/gpu/pcie_endpoint_tlm.h | grep "deprecated"` 验证 429327d
- D3: `grep -c "req_in\[" include/tlm/pcie/pcie_endpoint_ip.hh` 验证 17 ports
- D4: `diff` 验证仅增加 `[[deprecated]]` 属性,无 layout 变化

---

### C4. ADR-SOC-12: Host Bypass 软件 bring-up 路径 — P1

**主题**: Phase 7 HostBypassTLM + PcieRootComplexTLM(PF0-only 简化)

**决策点**(D1-D3):
- D1: 软件 bring-up 跳过 PCIe RC BFM(HostBypassTLM 直接桥接 AXI;per `2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc`)
- D2: PcieRootComplexTLM 枚举 PF0-only(VF 经 `stream_id` 直访非枚举发现——Oracle M2 标注;per `docs/architecture/14` L939)
- D3: M1 修复(429327d)— HostBypassTLM/RC::tick() 自动转发 4 方向 AXI 通道(master_out↔slave_in + slave_resp↔master_resp)

**证据附录**:
- 测试: `test/test_pcie_endpoint_ip_full_e2e.cc` 3 TEST_CASE(`{config, bar, rc}`) PASS

---

### C5. ADR-SOC-13: AXI Stream Adapter + AXI4Mapper 集成 — P1

**主题**: Phase 5/6 Axi4Bundle + Axi4StreamAdapter + Axi4Mapper

**决策点**(D1-D5):
- D1: `Axi4Bundle` 17 字段 + `Axi4LiteBundle` 9 字段(per `include/bundles/axi4_bundles_tlm.hh`)
- D2: `Axi4StreamAdapter` 三端口(master_out / slave_in / cfg_slave_in,per `include/framework/axi4_stream_adapter.hh`)+ outstanding 跟踪 + OOO completion
- D3: `Axi4Mapper` 独立模块(per `include/framework/axi4_mapper.hh`,与 PcieEndpointIP 解耦,可被 CrossbarTLM / CacheTLM 复用)+ JSON 可选注入(`axi4_mapper_inject: true`)
- D4: 512-bit 数据宽度已知限制(`ch_uint<512>` 实为 64-bit 存储 per Phase 5 M1,`include/bundles/cpphdl_types.hh`)
- D5: PCIe Cfg 地址编码简化(Phase8 M1 用 `awaddr` 当 offset;需未来细化按 PCIe 规范 `bits[1:0]=0, bits[7:2]=offset`)

---

### C6. ADR-SOC-14: v5.5+ 系统级硬件仿真集成修订 — P1

**主题**: Phase 8 完成后 v5.5+ 集成修订(基于 ADR-SOC-08 + 23 ABI 状态)

**关键修订(per Metis X3)**:
- **D1**: 5 项前置测试当前实施状态(详见 B 段状态追加,本 ADR 仅指针引用)
- **D2**: 4 项 UsrLinuxEmu 跨仓提议反馈状态(同上,仅指针引用)
- **D3**(修订后): **23 ABI 冻结不变量**(per ADR-088 §D5 + ADR-SOC-07 D5)
  - 仓内 19/19 函数 + 4 回调 typedef 契约(per `src/abi/cpptlm_emulator.cc` 433 行)
  - **不引入新 ABI 增量**——`axi_master_out / axi_slave_in / cfg_slave_in` 是 TLM 端口名(per `pcie_axi_adapter_tlm.hh`),**不是** ABI 函数名
  - 若需增量 ABI,需发起 ADR-088 修订并获 UsrLinuxEmu owner 联签(跨仓治理边界)
- **D4**: live migration 集成点推迟时间表(v5.5.4 Live Migration,等 UsrLinuxEmu ADR-089 更新)

---

## 6. 执行计划(分批)

### 6.1 批次划分

| 批次 | 工作量 | 工作内容 | Oracle 评审 |
|------|--------|---------|------------|
| **B1**(1-2 hr) | C3(ADR-11) | PcieEndpointIP 替代 PcieEndpointTLM(已实施 429327d,完整 ADR 模板) | 必须(独立) |
| **B2**(45-60 min) | A1(ADR-01)+ B(ADR-08) | 纯 Status Update 追加(A1 + B 合并 1 次 Oracle 评审) | 必须(批量 1 次) |
| **B3**(3-5 hr) | A4(ADR-06)+ A5(ADR-07) | 状态升级 ✅ + 引用更新 + 证据清单 | 必须(独立或批量 2 次) |
| **B4**(30 min) | A2(ADR-02 Status Update)+ A3(ADR-03 Status Update) | 标注"Superseded by ADR-SOC-06 D2",in-place Status Update(非 rev1 文件) | 必须(批量 1 次) |
| **B5**(4-8 hr) | C1(ADR-09 融合 NVIDIA+AMD)+ C2(ADR-10 拓扑层) | v1.0 战略基础 | 必须(独立 2 次) |
| **B6**(2-4 hr) | C4(ADR-12)+ C5(ADR-13)+ C6(ADR-14) | 已实施的 ADR 化 | 必须(独立 3 次或批量) |
| **B7**(1-2 hr) | A6(ADR-04)+ A7(ADR-05)+ README + 00-overview §7.1 | 状态追加 + 索引同步 + 引用矩阵更新 | 必须(批量 1 次) |
| **B8**(1 hr) | (1 hr) | 全部 ADR 评审 PASS 后,提交 openspec validate + archive | 必须 |

**总计约 15-25 hr**(修正后估算,per Metis C1;原计划低估 2-3 倍)

### 6.2 依赖关系

```
B1 (C3 立即,ADR-11 文档化)
  ↓
B2 (A1+B 状态追加,无依赖,可与 B1 并行)
  ↓
B3 (A4+A5 状态升级,依赖 B1)
  ↓
B4 (A2+A3 Superseded 标注,依赖 B3)
  ↓
B5 (C1+C2 v1.0 战略,依赖 B3)
  ↓
B6 (C4+C5+C6,依赖 B3)
  ↓
B7 (A6+A7+README+00-overview,依赖 B4)
  ↓
B8 归档 + validate
```

### 6.3 Oracle 评审预算

- 首轮评审工件数: A1 + A2 + A3 + A4 + A5 + A6 + A7 + B + C1..C6 = **14 份**(原 12 份 + A6/A7 增量)
- 通过 B2/B4/B7 批量合并: **11 次评审**
- 每次 5-13 min → 首轮 **55-143 min**
- 迭代 1-2 轮(per R2)→ 预期 **2-4 hr Oracle 总占用**

---

## 7. 风险与缓解 R1-R5

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| **R1** | 推翻已确认决策(ADR-SOC-02/03)可能引发社区争议 | 🟡 中 | 严格 ADR 不可变原则:不修改原文,in-place Status Update 标注 Superseded |
| **R2** | 11 份 ADR 工件 Oracle 评审失败需迭代 | 🟡 中 | 每份独立 Oracle 评审 PASS 才归档,失败则修订再评审;预留 2-4 hr Oracle 总占用 |
| **R3** | 状态升级证据不足导致 Oracle 不通过 | 🟢 低 | A4/A5 升级证据清单(git commit hash + 测试用例 + 配置文件路径)按 B 段规格补齐 |
| **R4** | AMD KFD 跨仓承诺空头(UsrLinuxEmu NVIDIA-only) | 🟡 中 | ADR-09 D3 仅承诺"依赖跨仓协调",不宣布具体时间表 |
| **R5** | 23 ABI 状态变化频繁,文档易过期 | 🟡 中 | Status Update 段每 6 个月审计一次(per ADR 维护约定) |

---

## 8. ADR 树 v1.0(修订/新建完成后)

```
ADR-SOC-01  coherence-protocol-strategy      [✅ + Status Update] ─┐
ADR-SOC-02  cu-granularity                    [⚠️ Superseded]      │
ADR-SOC-03  wavefront-coalescing-abstraction [⚠️ Superseded]      │
ADR-SOC-04  hsapp-cp-dispatcher-simplification[⚠️ Superseded partial]├─ APU + dGPU SoC 基础
ADR-SOC-05  gpu-directory-structure           [✅ + Status Update]│
ADR-SOC-06  cpptlm-v05-mvp                    [✅ + Status Update]│
ADR-SOC-07  dgpu-board-soc-layering           [✅ + Status Update]┘
ADR-SOC-08  v55-system-hw-integration         [📋 + Status Update]─┐
                                                                    │
ADR-SOC-09  v1.0 NVIDIA+AMD dual vendor战略  [📋 NEW]              │
ADR-SOC-10  v1.0 ModuleFactory 拓扑层         [📋 NEW]             ├─ v1.0 战略 + Phase 5-8
ADR-SOC-11  PcieEndpointIP 替代 PcieEndpointTLM [📋 NEW]          │
ADR-SOC-12  Host Bypass 软件 bring-up 路径     [📋 NEW]            │
ADR-SOC-13  AXI Stream Adapter + Mapper 集成   [📋 NEW]            │
ADR-SOC-14  v5.5+ 集成修订                     [📋 NEW]           ─┘
```

**图例扩展**(per adr/README.md):
- ⚠️ Superseded:原决策被新 ADR 替代;以 in-place Status Update 段标注
- ⚠️ Superseded partial:仅部分"永不做"列表被推翻,正文保留
- ✅ + Status Update:原决策不变,追加状态变化
- 📋 Proposed:新建 ADR 待评审

---

## 9. 与模块微架构文档的映射

| ADR | 关联模块微架构(共 25+ 份) |
|-----|------------------------|
| ADR-SOC-01 | coherence-protocol.md / coherence-domain.md / coherence-bridge.md / snoop_filter.md / coherent_xbar.md |
| ADR-SOC-02 | gpu-compute_unit.md / cuda-core-adapter.md |
| ADR-SOC-03 | gpu-compute_unit.md |
| ADR-SOC-04 | gpu-kernel-launch.md |
| ADR-SOC-05 | (模块目录结构本身)|
| ADR-SOC-06 | command-processor.md / pm4-decoder.md / tmu-dispatch-processor.md / submit-queue.md / cuda-core-adapter.md / ptx-emu-submodule-mvp.md / dgpu-board.md |
| ADR-SOC-07 | dgpu-board.md / dgpu-soc-pcie-slice.md / completion-ring.md |
| ADR-SOC-08 | (跨仓 23 ABI 契约,无直接模块) |
| ADR-SOC-09 | dgpu-board.md + command-processor.md + pm4-decoder.md + cuda-core-adapter.md |
| ADR-SOC-10 | modules/README.md(9 类 SimModule 容器)+ 全部 cluster 模块 |
| ADR-SOC-11 | dgpu-soc-pcie-slice.md |
| ADR-SOC-12 | (待新建 host-bypass.md + root-complex.md,per `2027-02-09-cpptlm-dgpu-soc-v1-modules-update`) |
| ADR-SOC-13 | (待新建 axi4-stream-adapter.md + axi4-mapper.md,同上) |
| ADR-SOC-14 | (跨仓集成,无直接模块) |

**依赖排序**: ADR-12/13 必须在 `modules-update` change 创建 host-bypass.md/root-complex.md/axi4-*.md **之后**评审(否则评审因引用空路径 FAIL)

---

## 10. 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2027-02-09 | v1.0-draft | Sisyphus | 首版创建(基于 v1.0 战略 + Oracle 评审 ADR-SOC 现状分析) |
| 2027-02-09 | v2.0-fixed | Sisyphus | Metis 评审 → 修复 P0(X1 D8 重号 / X2/X4/X9 A4 去重 / X3 C6 D3 虚构 ABI 增量 / X6 C1 D1 AMD 路径 / X7 C1 D3 KFD 跨仓依赖 / M1 ADR-SOC-04 / M2 ADR-SOC-05 / 工作量 6-8 hr → 15-25 hr)+ P1(P3 PDL/CDP 区分 / F4 命名合规 / F5 证据附录 / F6 Oracle 矩阵一致性检查) |

**关联 OpenSpec**: `openspec/changes/2027-02-09-cpptlm-dgpu-soc-v1-adr-revision/`(**待建立**,本规划 v2 为该 change 的 proposal 输入)

**下次更新**:Metis 复审通过 → 创建 ADR 归档载体 OpenSpec change → B1-B7 分批执行 → 每份 Oracle 评审 PASS → B8 归档