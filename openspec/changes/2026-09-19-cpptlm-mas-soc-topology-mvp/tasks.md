# 2026-09-19-cpptlm-mas-soc-topology-mvp: Tasks (TDD 5 步结构)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/soc-topology-mvp/spec.md`](specs/soc-topology-mvp/spec.md)
> **关联设计**: [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../../../../docs/soc_arch/architecture/21-soc-topology-mvp.md)（SoC 顶层物理布局规范 SSOT）
> **工期估算**: 5-7 天（文档 2 天 + C++ 1 天 + 测试 1 天 + Oracle 1-2 天）

---

## 文件清单

| 文件 | 状态 | 步骤 |
|------|------|:----:|
| `docs/soc_arch/architecture/21-soc-topology-mvp.md` | **新** | T0 |
| `docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md` | **新** | T1 |
| `docs/soc_arch/architecture/21-fabric-switch-mvp.md` | **改** | T2 |
| `docs/soc_arch/architecture/21-tee-udd-mvp.md` | **改** | T2 |
| `docs/soc_arch/architecture/21-microarch-ifc-mvp.md` | **改** | T2 |
| `docs/soc_arch/architecture/21-dma-backends-mvp.md` | **改** | T2 |
| `docs/soc_arch/architecture/21-tee-udd-evolution-roadmap.md` | **改** | T2 |
| `docs/soc_arch/architecture/21-dma-backends-evolution-roadmap.md` | **改** | T2 |
| `docs/soc_arch/architecture/21-fabric-switch-evolution-roadmap.md` | **改** | T2 |
| `docs/soc_arch/architecture/21-microarch-ifc-evolution-roadmap.md` | **改** | T2 |
| `include/tlm/soc/soc_topology_defs.hh` | **新** | T3 |
| `src/tlm/soc/soc_shell.cc` | **新** | T3 |
| `test/test_soc_topology_e2e.cc` | **新** | T4 |

---

## T0: SoC 顶层物理布局规范文档 (1 天)

**目的**: 创建 SoC 顶层物理布局规范 SSOT，统一五大子系统划分。

- [x] **T0.1** 创建 `docs/soc_arch/architecture/21-soc-topology-mvp.md`（739 行 SSOT）
  - [x] T0.1.1 §1 概述（含 V3.1-Rev2.0 三点关键修正表）
  - [x] T0.1.2 §2 关键模块重新划分（五大子系统）
  - [x] T0.1.3 §3 关键数据流路径（TMA 5 步 + CXL 6 步 + IO-DMA）
  - [x] T0.1.4 §4 控制流修正（HRT Route_Tag + Drain 三方协调 + AWT Trap Type）
  - [x] T0.1.5 §5 模块接口修订影响（5 项 RTL-IFC 联动）
  - [x] T0.1.6 §6 v1.0 MVP 端到端 Demo（10 步）
  - [x] T0.1.7 §7 无债务演进约束（3 条不变量）
  - [x] T0.1.8 §8-§12 边界 / 跨仓 / 反模式 / 引用 / 维护记录

**TDD 5 步 (T0)**:
- [x] **T0.W** Write: 创建 `21-soc-topology-mvp.md` 草案 ✅
- [x] **T0.V** Verify: 检查文档结构与 GMMU MVP 格式对齐 ✅
- [x] **T0.I** Implement: 完成全部 12 章节 ✅
- [x] **T0.P** Pass: 通过格式一致性检查 ✅
- [x] **T0.C** Commit: 准备提交（无 git 操作，待提案批准后提交） ✅

---

## T1: ADR-SOC-21 起草 (1 天)

**目的**: 将 V3.1-Rev2.0 拓扑修正作为正式 ADR 落地。

- [ ] **T1.1** 创建 `docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md`
  - [ ] T1.1.1 §1 Context（背景：V3.0 拓扑的 3 个错误）
  - [ ] T1.1.2 §2 Decision（5 项决策 D1-D5）
    - [ ] D1: TC-DMA 物理归属 GPC 内（不与 HBM-DMA 平级）
    - [ ] D2: GPU Die 上严禁 CXL PHY/Controller
    - [ ] D3: HRT 严禁 CXL_DIRECT Route_Tag
    - [ ] D4: IO-DMA 在 Fabric & Edge IO Subsystem（与 NIC-DMA 平级）
    - [ ] D5: CXL Drain 三方协调机制（GPU-Switch-FM）
  - [ ] T1.1.3 §3 3 条无债务演进不变量（TC-DMA 归属 / GPU 无 CXL / HRT 无 CXL_DIRECT）
  - [ ] T1.1.4 §4 Consequences（正面收益 + 负面影响 + 风险缓解）
  - [ ] T1.1.5 §5 5 阶段约束（v1.0/v1.1/v2.0/v2.1/v3.0 不变量约束）

**TDD 5 步 (T1)**:
- [ ] **T1.W** Write: 起草 ADR-SOC-21 草案
- [ ] **T1.V** Verify: 与 ADR-SOC-10/ADR-SOC-19 格式对齐
- [ ] **T1.I** Implement: 完成全部 5 章节 + 5 项决策 + 3 条不变量
- [ ] **T1.P** Pass: 通过格式审查（标题/状态/日期/Owner/关联 ADR）
- [ ] **T1.C** Commit: 提交到 `docs/soc_arch/adr/`

---

## T2: 4 份子系统 MVP + 4 份 Roadmap 修正 (1 天)

**目的**: 同步 8 份子系统文档的 V3.1-Rev2.0 拓扑修正（已完成部分）。

- [x] **T2.1** 修正 `21-fabric-switch-mvp.md`
  - [x] T2.1.1 关联文档新增 `21-soc-topology-mvp.md` + ADR-SOC-21
  - [x] T2.1.2 §1.2 新增 V3.1-Rev2.0 拓扑修正横幅（CXL Memory Pool 在外部 Switch 下）
  - [x] T2.1.3 §1.3 表格新增 SoC-Topology 行
  - [x] T2.1.4 §4.4 Route_Tag 表显式标注 "via Switch"，新增 CXL_DIRECT 移除说明
  - [x] T2.1.5 §7.1 Poison 编码明确 `REMOTE_CXL_POISON_VIA_SWITCH`
- [x] **T2.2** 修正 `21-tee-udd-mvp.md`
  - [x] T2.2.1 关联文档新增 `21-soc-topology-mvp.md` + ADR-SOC-21
  - [x] T2.2.2 新增 §4.0 GUPA 空间划分（5 段路由域表）
  - [x] T2.2.3 §4.0 HRT Route_Tag 分配表（0x0/0x1~0xE/0xF, 无 CXL_DIRECT）
  - [x] T2.2.4 §4.1 MMIO 表新增 HRT Shadow Entry 32-bit 字段位分配说明
- [x] **T2.3** 修正 `21-microarch-ifc-mvp.md`
  - [x] T2.3.1 关联文档新增 `21-soc-topology-mvp.md` + ADR-SOC-21
  - [x] T2.3.2 §7.6 AWT Trap Type 改为 4 列完整表（含名称 / 来源识别 / 物理介质归属 / 软件处理）
  - [x] T2.3.3 §7.6 Trap Type `0x1` 明确为 `REMOTE_CXL_POISON_VIA_SWITCH`
- [x] **T2.4** 修正 `21-dma-backends-mvp.md`
  - [x] T2.4.1 关联文档新增 `21-soc-topology-mvp.md` + ADR-SOC-21
- [x] **T2.5** 修正 4 份 Roadmap（关联文档同步）
  - [x] T2.5.1 `21-tee-udd-evolution-roadmap.md`
  - [x] T2.5.2 `21-dma-backends-evolution-roadmap.md`
  - [x] T2.5.3 `21-fabric-switch-evolution-roadmap.md`
  - [x] T2.5.4 `21-microarch-ifc-evolution-roadmap.md`

**TDD 5 步 (T2)**:
- [x] **T2.W** Write: 起草 4 份子系统 MVP 修正点 ✅
- [x] **T2.V** Verify: 验证 9 份文档 SoC-Topology + ADR-SOC-21 引用一致性 ✅
- [x] **T2.I** Implement: 完成所有 8 份文档修正 ✅
- [x] **T2.P** Pass: 通过 grep 验证关键概念引用 ✅
- [x] **T2.C** Commit: 准备提交 ✅

---

## T3: SoC 顶层 C++ 基础设施 (1 天)

**目的**: 创建 `soc_topology_defs.hh` + `soc_shell.cc`，提供 SoC 顶层注入与拓扑验证。

- [ ] **T3.1** 创建 `include/tlm/soc/soc_topology_defs.hh`
  - [ ] T3.1.1 `SubsystemType` 枚举（GPC / MEMORY / FABRIC_IO / CONTROL / EXTERNAL）
  - [ ] T3.1.2 5 大子系统边界宏（SUBSYSTEM_GPC_BOUNDARY_MAX 等）
  - [ ] T3.1.3 TC-DMA 物理归属 GPC 内宏（TC_DMA_LOCATION_GPC_INSIDE）
  - [ ] T3.1.4 GPU Die 严禁 CXL 宏（GPU_DIE_FORBIDDEN_CXL_PHY 等）
  - [ ] T3.1.5 HRT 严禁 CXL_DIRECT 宏（HRT_FORBIDDEN_CXL_DIRECT_ROUTE_TAG）
  - [ ] T3.1.6 CXL 访存路径必须经 Switch 宏（CXL_ACCESS_PATH_MUST_VIA_SWITCH）
- [ ] **T3.2** 创建 `src/tlm/soc/soc_shell.cc`
  - [ ] T3.2.1 `SocShell` 类（私有成员：5 大 subsystem vector）
  - [ ] T3.2.2 `instantiate_all_subsystems()` 方法（per 21-soc-topology-mvp.md §2）
  - [ ] T3.2.3 `validate_topology()` 方法（3 条不变量检查）
- [ ] **T3.3** RTL-IFC 5 项联动变更
  - [ ] T3.3.1 移除 `UDD ↔ CXL Bridge` 接口（compile-time check）
  - [ ] T3.3.2 修正 `TC-DMA ↔ UDD Agent` 物理位置（GPC 内）
  - [ ] T3.3.3 新增 `IO-DMA ↔ Mem & IO NoC` 接口（v1.1+ 预留）
  - [ ] T3.3.4 修正 `NIC-DMA ↔ Scale-Up Switch`（仅 UALink Flit）
  - [ ] T3.3.5 修正 `AWT Trap Payload[7:0]`（`0x1 = REMOTE_CXL_POISON_VIA_SWITCH`）

**TDD 5 步 (T3)**:
- [ ] **T3.W** Write: 起草 `soc_topology_defs.hh` + `soc_shell.cc` 草案
- [ ] **T3.V** Verify: 运行 `cmake --build build` 应失败（缺测试）
- [ ] **T3.I** Implement: 完成 5 大 subsystem 注入与 3 条不变量检查
- [ ] **T3.P** Pass: `cmake --build build` 通过，无编译错误
- [ ] **T3.C** Commit: 提交到 `include/tlm/soc/` + `src/tlm/soc/`

---

## T4: SoC 端到端 demo 测试 (1 天)

**目的**: 创建 `test_soc_topology_e2e.cc`，10 步端到端 demo 验证 3 项 V3.1-Rev2.0 修正。

- [ ] **T4.1** 创建 `test/test_soc_topology_e2e.cc`（TDD 5 步）
  - [ ] **T4.W** Write failing test
    - [ ] T4.1.1 测试用例骨架（10 步 demo）
    - [ ] T4.1.2 Step 1-3: Driver 经 PCIe MMIO 写 UDD HRT + SM 发起 Load + GMMU 翻译
    - [ ] T4.1.3 Step 4-6: UDD Agent HRT 查表 + Compute NoC 传输 + Global UDD Hub 仲裁
    - [ ] T4.1.4 Step 7-9: HBM-DMA 处理 + UDD Response 返回 + TEE Frontend 接收
    - [ ] T4.1.5 Step 10: 验证 SoC 拓扑正确性（5 项断言）
  - [ ] **T4.V** Verify fail: 运行测试，预期失败（soc_shell.cc 未实现）
  - [ ] **T4.I** Implement: 实现 `SocShell`（per T3.2）
  - [ ] **T4.P** Pass: `ctest --output-on-failure` 通过
    - [ ] Step 10 验证清单:
      - [ ] TC-DMA 物理归属 GPC 内（不直连 HBM）
      - [ ] IO-DMA 在 Fabric & Edge IO Subsystem（与 NIC-DMA 平级）
      - [ ] CXL Memory Pool 不在 GPU Die 上（在 Scale-Up Switch 下）
      - [ ] HRT 无 `CXL_DIRECT` Route_Tag（仅 0x0/0x1~0xE/0xF）
      - [ ] AWT Trap Type 区分 Local/Remote (`0x1 = REMOTE_CXL_POISON_VIA_SWITCH`)
  - [ ] **T4.C** Commit: 提交到 `test/`

---

## T5: Oracle 评审 + 复评 (1-2 天)

**目的**: 提交 Oracle 评审（预期 ≥9.0/10 PASS），≤ 3 次重试。

- [ ] **T5.1** 提交 Oracle 评审
  - [ ] T5.1.1 提交 9 份架构文档 + 1 份 ADR + 2 个 C++ 文件 + 1 个测试
  - [ ] T5.1.2 等待 Oracle 评分（预期 ≥9.0/10）
- [ ] **T5.2** 复评（如有）
  - [ ] T5.2.1 处理 Oracle 反馈意见
  - [ ] T5.2.2 提交复评（≤ 3 次重试）
- [ ] **T5.3** Oracle PASS 报告归档

**TDD 5 步 (T5)**:
- [ ] **T5.W** Write: 起草 Oracle 评审清单（10 项 Acceptance Gate）
- [ ] **T5.V** Verify: Oracle 评分 ≥9.0/10
- [ ] **T5.I** Implement: 复评修复（如有）
- [ ] **T5.P** Pass: Oracle 最终 PASS
- [ ] **T5.C** Commit: Oracle 评审结果归档到 `docs/validation/`

---

## Oracle 评审清单（10 项 Acceptance Gate）

- [ ] **AG1**: `21-soc-topology-mvp.md` 已创建并含 5 大子系统划分 + 修正后拓扑图 + 3 条不变量
- [ ] **AG2**: `21-fabric-switch-mvp.md` §1.2 / §4.4 / §7.1 三处均明确标注 V3.1-Rev2.0 拓扑修正
- [ ] **AG3**: `21-tee-udd-mvp.md` 新增 §4.0 GUPA 空间划分 + HRT Route_Tag 分配表
- [ ] **AG4**: `21-microarch-ifc-mvp.md` §7.6 AWT Trap Type 明确为 `REMOTE_CXL_POISON_VIA_SWITCH`
- [ ] **AG5**: 8 份子系统文档的关联文档部分均含 `21-soc-topology-mvp.md` + ADR-SOC-21
- [ ] **AG6**: `ADR-SOC-21` 已创建，含 Context / Decision / Consequences / 5 阶段约束
- [ ] **AG7**: `soc_topology_defs.hh` 定义 5 大子系统边界宏 (SUBSYSTEM_GPC / SUBSYSTEM_MEMORY / SUBSYSTEM_FABRIC_IO / SUBSYSTEM_CONTROL / SUBSYSTEM_EXTERNAL)
- [ ] **AG8**: `soc_shell.cc` 注入 5 大子系统拓扑 (GPC × 8, HBM-DMA × 2, NIC-DMA × 4, IO-DMA × 1, CIU × 1)
- [ ] **AG9**: `test_soc_topology_e2e.cc` 10 步端到端 demo 测试通过 (TC-DMA 归属 + IO-DMA 在 Fabric & Edge IO + CXL 不在 GPU Die + HRT 无 CXL_DIRECT + AWT Trap Type 区分)
- [ ] **AG10**: 0 个新 ABI 函数 (per ADR-088 §D5 严格遵守)

---

## 进度跟踪

| 步骤 | 状态 | 完成度 |
|------|------|--------|
| T0: SoC 顶层物理布局规范文档 | ✅ 完成 | 100% |
| T1: ADR-SOC-21 起草 | ⏳ 待执行 | 0% |
| T2: 8 份子系统文档修正 | ✅ 完成 | 100% |
| T3: SoC 顶层 C++ 基础设施 | ⏳ 待执行 | 0% |
| T4: SoC 端到端 demo 测试 | ⏳ 待执行 | 0% |
| T5: Oracle 评审 + 复评 | ⏳ 待执行 | 0% |
| **总体进度** | **⏳ 进行中** | **40%** |

---

## 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: V3.1-Rev2.0 拓扑修正 TDD 5 步任务清单 (T0-T5, 10 项 Acceptance Gate, 进度 40%) |