# 2026-09-19-cpptlm-mas-soc-topology-mvp: 设计文档

> **配套**: [`proposal.md`](proposal.md) · [`specs/soc-topology-mvp/spec.md`](specs/soc-topology-mvp/spec.md) · [`tasks.md`](tasks.md)
> **SSOT**: [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../../../../docs/soc_arch/architecture/21-soc-topology-mvp.md)（SoC 顶层物理布局规范 SSOT）

---

## §1 设计概览

本 change 的实现细节 SSOT 已在以下文档充分阐述:

- **SoC 顶层物理布局规范**: `docs/soc_arch/architecture/21-soc-topology-mvp.md` §1-§12（五大子系统划分 / 修正后拓扑图 / TMA/CXL/IO 三路径 / RTL-IFC 联动 / 端到端 demo / 3 条不变量 / 边界 / 跨仓契约 / 反模式 / 引用）
- **4 份子系统 MVP 文档** (本次已同步 V3.1-Rev2.0 拓扑修正):
  - `docs/soc_arch/architecture/21-tee-udd-mvp.md` — 数据面
  - `docs/soc_arch/architecture/21-dma-backends-mvp.md` — 后端物理实现
  - `docs/soc_arch/architecture/21-fabric-switch-mvp.md` — 协议层
  - `docs/soc_arch/architecture/21-microarch-ifc-mvp.md` — 实现层契约

本设计文档**不再重复**以上内容,仅补充 proposal 中**实施期**特有的设计决策（代码组织、模块依赖、迁移策略、TDD 顺序、跨子系统一致性验证矩阵）。

---

## §2 代码组织（实施期）

### §2.1 新增文件清单（与 proposal §What Changes 一致）

```
include/tlm/soc/
└── soc_topology_defs.hh         # 五大子系统边界宏定义

src/tlm/soc/
└── soc_shell.cc                  # SoC 顶层注入拓扑 (instantiateAll 五大子系统)

openspec/changes/2026-09-19-cpptlm-mas-soc-topology-mvp/
├── proposal.md                   # ✅ 已创建
├── design.md                     # ✅ 本文件
├── tasks.md                      # TDD 5 步任务清单
└── specs/soc-topology-mvp/
    └── spec.md                   # delta spec (Requirements + Scenarios)

docs/soc_arch/architecture/
├── 21-soc-topology-mvp.md        # ✅ SoC 顶层物理布局规范 SSOT (已创建)
├── 21-fabric-switch-mvp.md       # ✅ 已修正 (V3.1-Rev2.0 拓扑修正)
├── 21-tee-udd-mvp.md             # ✅ 已修正 (新增 §4.0 GUPA 空间划分)
├── 21-microarch-ifc-mvp.md       # ✅ 已修正 (§7.6 AWT Trap Type)
├── 21-dma-backends-mvp.md        # ✅ 已修正 (关联文档)
├── 21-tee-udd-evolution-roadmap.md      # ✅ 已修正 (关联文档)
├── 21-dma-backends-evolution-roadmap.md # ✅ 已修正 (关联文档)
├── 21-fabric-switch-evolution-roadmap.md # ✅ 已修正 (关联文档)
└── 21-microarch-ifc-evolution-roadmap.md # ✅ 已修正 (关联文档)

docs/soc_arch/adr/
└── ADR-SOC-21-v31-rev2-topology-correction.md  # V3.1-Rev2.0 拓扑修正 ADR (T1)

test/
└── test_soc_topology_e2e.cc      # SoC 端到端 demo (T4)
```

### §2.2 五大子系统边界宏定义（`soc_topology_defs.hh` 设计）

```cpp
// include/tlm/soc/soc_topology_defs.hh
// 五大子系统边界宏 (per 21-soc-topology-mvp.md §2)
namespace cpptlm::soc {

// Subsystem Type 枚举
enum class SubsystemType : uint8_t {
    GPC = 0x1,           // GPC Subsystem (计算 + TC-DMA + L2 + UDD Agent)
    MEMORY = 0x2,        // Memory Subsystem (HBM-DMA + HBM3e Stacks)
    FABRIC_IO = 0x3,     // Fabric & Edge IO Subsystem (Global Hub + NIC-DMA + IO-DMA)
    CONTROL = 0x4,       // Control Subsystem (CIU + AWT + GMMU Hub + Boot ROM)
    EXTERNAL = 0x5       // External (Scale-Up Switch + CXL Memory Pool + Host)
};

// Subsystem Boundary Macros (per 21-soc-topology-mvp.md §7 不变量)
#define SUBSYSTEM_GPC_BOUNDARY_MAX          8    // GPC 数量上限 (N = 8 typical)
#define SUBSYSTEM_MEMORY_BOUNDARY_MAX       2    // HBM-DMA 数量上限 (Ctrl0 + Ctrl1)
#define SUBSYSTEM_FABRIC_IO_BOUNDARY_MAX    5    // NIC-DMA Port 4 + IO-DMA 1
#define SUBSYSTEM_CONTROL_BOUNDARY_MAX      1    // CIU 单例

// TC-DMA 物理归属 GPC 内 (per ADR-SOC-21 D1 不变量 1)
#define TC_DMA_LOCATION_GPC_INSIDE          1    // 强制 TC-DMA 在 GPC 内
#define TC_DMA_FORBIDDEN_DIRECT_HBM         1    // 禁止 TC-DMA 直连 HBM-DMA

// GPU Die 严禁 CXL (per ADR-SOC-21 D2 不变量 2)
#define GPU_DIE_FORBIDDEN_CXL_PHY           1    // 强制 GPU Die 无 CXL PHY
#define GPU_DIE_FORBIDDEN_CXL_CONTROLLER    1    // 强制 GPU Die 无 CXL Controller

// HRT Route_Tag 严禁 CXL_DIRECT (per ADR-SOC-21 D3 不变量 3)
#define HRT_FORBIDDEN_CXL_DIRECT_ROUTE_TAG  1    // 强制 HRT 无 0xF0 ~ 0xFF CXL 专用槽位
#define HRT_ROUTE_TAG_MAX                   0xF  // 0x0=HBM, 0x1~0xE=UALink, 0xF=PCIe

// CXL 访存路径必须经 Scale-Up Switch (per ADR-SOC-21 D2)
#define CXL_ACCESS_PATH_MUST_VIA_SWITCH     1    // 强制所有 CXL 访存经 NIC-DMA → Switch

}  // namespace cpptlm::soc
```

### §2.3 SoC 顶层注入拓扑（`soc_shell.cc` 设计）

```cpp
// src/tlm/soc/soc_shell.cc
#include <tlm/soc/soc_topology_defs.hh>
#include <tlm/core/module_factory.h>
#include <tlm/cluster/gpc_cluster.hh>  // GPC Subsystem (per include/tlm/cluster/)
#include <tlm/dma/hbm_dma_ubc_tlm.hh>
#include <tlm/dma/nic_dma_ubc_tlm.hh>
#include <tlm/dma/ciu_tlm.hh>
// ... 其他模块 include

namespace cpptlm::soc {

class SocShell {
public:
    void instantiate_all_subsystems() {
        // 1. GPC Subsystem × N (per 21-soc-topology-mvp.md §2.1)
        for (int i = 0; i < SUBSYSTEM_GPC_BOUNDARY_MAX; ++i) {
            auto* gpc = ModuleFactory::instantiate<GpcClusterTLM>("gpc_" + std::to_string(i));
            // TC-DMA 在 GpcClusterTLM 内部实例化 (GPC 紧耦合, per D1 不变量)
            gpc_subsystems_.push_back(gpc);
        }

        // 2. Memory Subsystem × M (per 21-soc-topology-mvp.md §2.2)
        for (int i = 0; i < SUBSYSTEM_MEMORY_BOUNDARY_MAX; ++i) {
            auto* hbm = ModuleFactory::instantiate<HbmDmaUbcTLM>("hbm_dma_" + std::to_string(i));
            memory_subsystems_.push_back(hbm);
        }

        // 3. Fabric & Edge IO Subsystem (per 21-soc-topology-mvp.md §2.3)
        auto* global_hub = ModuleFactory::instantiate<GlobalUddHubTLM>("global_udd_hub");
        for (int i = 0; i < 4; ++i) {  // NIC-DMA Port 0~3
            auto* nic = ModuleFactory::instantiate<NicDmaUbcTLM>("nic_dma_" + std::to_string(i));
            fabric_io_subsystems_.push_back(nic);
        }
        // IO-DMA v1.1+ 引入 (per 21-dma-backends-evolution-roadmap.md §4.1)
        // auto* io_dma = ModuleFactory::instantiate<IoDmaUbcTLM>("io_dma");

        // 4. Control Subsystem (per 21-soc-topology-mvp.md §2.4)
        auto* ciu = ModuleFactory::instantiate<CIUTLM>("ciu");
        // auto* awt = ModuleFactory::instantiate<AwtControllerTLM>("awt_controller");
        // auto* gmmu_hub = ModuleFactory::instantiate<GmmuHubTLM>("gmmu_hub");

        // 5. External (NOT instantiated in GPU Die, per 21-soc-topology-mvp.md §2.5 + D2 不变量)
        // Scale-Up Switch + CXL Memory Pool 是外部设备, 仅作 BFM Mock
        // auto* switch_bfm = ModuleFactory::instantiate<ScaleUpSwitchBFM>("scale_up_switch_bfm");
    }

    // 拓扑验证 (per ADR-SOC-10 §D1)
    bool validate_topology() const {
        // 1. 验证 TC-DMA 不在 Memory Subsystem
        for (const auto* hbm : memory_subsystems_) {
            assert(!hbm->has_tc_dma_connected() && "TC-DMA 不能直连 HBM-DMA!");
        }
        // 2. 验证 GPU Die 无 CXL PHY
        for (const auto* gpc : gpc_subsystems_) {
            assert(!gpc->has_cxl_phy() && "GPU Die 严禁 CXL PHY!");
        }
        // 3. 验证 HRT Route_Tag 无 CXL_DIRECT
        auto* ciu_instance = control_subsystem_;
        assert(!ciu_instance->has_cxl_direct_route() && "HRT 严禁 CXL_DIRECT Route_Tag!");
        return true;
    }

private:
    std::vector<GpcClusterTLM*> gpc_subsystems_;        // GPC Subsystem × N
    std::vector<HbmDmaUbcTLM*> memory_subsystems_;       // Memory Subsystem × M
    std::vector<NicDmaUbcTLM*> fabric_io_subsystems_;    // Fabric & Edge IO
    CIUTLM* control_subsystem_ = nullptr;                // Control Subsystem
};

}  // namespace cpptlm::soc
```

### §2.4 关键 RTL-IFC 联动变更（per `21-soc-topology-mvp.md` §5）

| 接口 | V3.0 状态 | V3.1-Rev2.0 状态 | 实施步骤 |
|------|----------|-------------------|----------|
| `UDD ↔ CXL Bridge` | 已定义 | **移除**（compile-time check） | T2 + T3 |
| `TC-DMA ↔ UDD Agent` | 已定义 | 修正：GPC 内紧耦合，物理端口在 GpcClusterTLM 内部 | T3 |
| `IO-DMA ↔ Mem & IO NoC` | 缺失 | **新增**（v1.1+ 实施，v1.0 预留接口） | T3（仅接口预留） |
| `NIC-DMA ↔ Scale-Up Switch` | 已定义 | 修正：仅 UALink Flit（移除 CXL 信号） | T3 |
| `AWT Trap Payload[7:0]` | 已定义 | 修正：`0x1 = REMOTE_CXL_POISON_VIA_SWITCH` | T3 |

---

## §3 模块依赖图（实施期）

### §3.1 五大子系统依赖关系

```
[GPC Subsystem × N]
   │
   ├─[Compute NoC]─┐
   │               │
   │               ▼
   │     [Global UDD Hub (Fabric)]
   │               │
   │               ├─→ [HBM-DMA × M] (Memory Subsystem)
   │               │
   │               ├─→ [NIC-DMA × 4] (Fabric & Edge IO)
   │               │         │
   │               │         └─→ [Scale-Up Switch] (External)
   │               │                   │
   │               │                   └─→ [CXL Memory Pool] (External)
   │               │
   │               └─→ [IO-DMA × 1] (Fabric & Edge IO, v1.1+)
   │                         │
   │                         └─→ [Host CPU + NVMe] (External)
   │
   ▼
[CIU (Control)] ──APB──→ [All Subsystems for HRT/RCT/CSR config]
```

### §3.2 关键依赖约束

1. **TC-DMA → HBM-DMA 不允许直连**: 仅允许 `TC-DMA → GPC UDD Agent → Compute NoC → Global UDD Hub → HBM-DMA`（per D1 不变量 1）
2. **GPU Die → CXL Memory Pool 不允许直连**: 仅允许 `GPU NIC-DMA → UALink → Scale-Up Switch PTE → CXL Memory Pool`（per D2 不变量 2）
3. **HRT 不允许 `CXL_DIRECT` Route_Tag**: 仅允许 `0x0=HBM, 0x1~0xE=UALink, 0xF=PCIe`（per D3 不变量 3）
4. **CXL Drain 必须三方协调**: GPU NIC-DMA + Switch PTE + FM（per `21-soc-topology-mvp.md` §4.2）

---

## §4 迁移策略（TDD 5 步 + Oracle 评审）

### §4.1 文档与 ADR 阶段（T0-T1, 1-2 天）

**T0: 文档与归档** (per `tasks.md` T0)
- [x] T0.1 创建 `21-soc-topology-mvp.md` ✅（已创建, 739 行）
- [x] T0.2 修正 8 份子系统文档关联文档 ✅（已修正）
- [x] T0.3 创建本 proposal.md / design.md / tasks.md ✅（已创建）

**T1: ADR 起草** (per `tasks.md` T1)
- [ ] T1.1 创建 `ADR-SOC-21-v31-rev2-topology-correction.md`（V3.1-Rev2.0 拓扑修正决策）
- [ ] T1.2 ADR 含 Context / Decision D1-D3 / Consequences / 5 阶段约束

### §4.2 文档交叉一致性验证阶段（T2, 1 天）

**T2: 跨子系统文档一致性验证**
- [ ] T2.1 验证 4 份子系统 MVP § 修正点完整（per `tasks.md` T2.1）
- [ ] T2.2 验证 4 份 Roadmap 关联文档同步（per `tasks.md` T2.2）
- [ ] T2.3 验证 AG5: 8 份子系统文档关联文档部分均含 `21-soc-topology-mvp.md` + ADR-SOC-21

### §4.3 C++ 基础设施阶段（T3, 1 天）

**T3: SoC 顶层注入基础设施**
- [ ] T3.1 创建 `include/tlm/soc/soc_topology_defs.hh`（五大子系统边界宏 + 3 条不变量编译期检查）
- [ ] T3.2 创建 `src/tlm/soc/soc_shell.cc`（SoCShell 类 + instantiate_all_subsystems() + validate_topology()）
- [ ] T3.3 RTL-IFC 5 项联动变更（per §2.4）
  - [ ] T3.3.1 移除 `UDD ↔ CXL Bridge` 接口（compile-time check）
  - [ ] T3.3.2 修正 `TC-DMA ↔ UDD Agent` 物理位置（GPC 内）
  - [ ] T3.3.3 新增 `IO-DMA ↔ Mem & IO NoC` 接口（v1.1+ 预留）
  - [ ] T3.3.4 修正 `NIC-DMA ↔ Scale-Up Switch`（仅 UALink Flit）
  - [ ] T3.3.5 修正 `AWT Trap Payload[7:0]`（`0x1 = REMOTE_CXL_POISON_VIA_SWITCH`）

### §4.4 端到端测试阶段（T4, 1 天）

**T4: SoC 端到端 demo 测试**
- [ ] T4.1 创建 `test/test_soc_topology_e2e.cc`（TDD 5 步: Write failing test → Verify fail → Implement → Verify pass → Commit）
  - 10 步端到端 demo (per `21-soc-topology-mvp.md` §6)
  - 验证 TC-DMA 归属 + IO-DMA 在 Fabric & Edge IO + CXL 不在 GPU Die + HRT 无 CXL_DIRECT + AWT Trap Type 区分

### §4.5 Oracle 评审阶段（T5, 1-2 天）

**T5: Oracle 评审 + 复评**
- [ ] T5.1 提交 Oracle 评审（per `tasks.md` T5）
- [ ] T5.2 复评（per Oracle 反馈，< 3 次重试）

---

## §5 跨子系统一致性验证矩阵

### §5.1 关键概念 9 份文档交叉验证

| 关键概念 | 期望引用 | 验证命令 |
|---------|----------|----------|
| **CXL 不在 GPU Die** | 5+ 份文档 | `grep -c "CXL 不在 GPU Die\|GPU Die.*无 CXL" 21-*.md` |
| **HRT 无 CXL_DIRECT** | 3 份文档 (soc-topology/tee-udd/fabric-switch) | `grep -c "CXL_DIRECT" 21-*.md` |
| **REMOTE_CXL_POISON_VIA_SWITCH** | 3 份文档 (soc-topology/fabric-switch/microarch-ifc) | `grep -c "REMOTE_CXL_POISON_VIA_SWITCH" 21-*.md` |
| **TC-DMA 归属 GPC 内** | 2+ 份文档 (soc-topology/microarch-ifc) | `grep -c "TC-DMA.*GPC 内\|TC-DMA.*紧耦合" 21-*.md` |
| **IO-DMA 在 Fabric & Edge IO** | 1+ 份文档 (soc-topology) | `grep -c "IO-DMA.*Fabric.*Edge IO\|IO-DMA.*平级" 21-*.md` |
| **V3.1-Rev2.0 拓扑修正横幅** | 4 份子系统 MVP 文档 | `grep -c "V3.1-Rev2.0" 21-*.md` |
| **ADR-SOC-21 引用** | 8+ 份子系统文档 + ADR 自身 | `grep -c "ADR-SOC-21" 21-*.md` |
| **21-soc-topology-mvp 引用** | 8+ 份子系统文档 | `grep -c "21-soc-topology-mvp" 21-*.md` |

### §5.2 实施期交叉验证（TLM 仿真层）

| 验证项 | 期望结果 | 验证工具 |
|--------|----------|----------|
| `soc_shell.cc` instantiate_all_subsystems() | 5 大子系统全部实例化 | ctest --output-on-failure |
| `soc_shell.cc` validate_topology() | 3 条不变量全部通过 | assert + ctest |
| `test_soc_topology_e2e.cc` 10 步 demo | 全部通过 | ctest |
| TC-DMA 物理位置检测 | TC-DMA 仅在 GPC 内 | validate_topology() |
| HRT 查表 | GUPA 0x1~0xE → NIC-DMA（无 CXL_DIRECT） | unit test |

---

## §6 风险评估与缓解（实施期）

| 风险 | 等级 | 触发条件 | 缓解策略 |
|------|------|----------|----------|
| RTL-IFC 改动遗漏 | 中 | T3.3 漏改某项 | T3.3.x 子任务逐项 + Oracle 评审覆盖 |
| TC-DMA 误连 HBM | 高 | T3.2 物理端口错误 | validate_topology() 编译期 + 运行时检查 |
| HRT 引入 CXL_DIRECT | 中 | HRT 配置错误 | D3 不变量编译期检查 + CI test |
| Oracle 评分 < 9.0 | 中 | 文档 / 代码不完整 | Oracle 复评 ≤ 3 次 |
| 跨仓 PR 失败 | 低 | UsrLinuxEmu 不接受新拓扑 | driver HRT 初始化保持 0 ABI 改动 |

---

## §7 关联文档

- [`proposal.md`](proposal.md) — 提案总览（Why / What Changes / Scope / Acceptance Gate / Capabilities / Impact）
- [`specs/soc-topology-mvp/spec.md`](specs/soc-topology-mvp/spec.md) — delta spec（Requirements + Scenarios）
- [`tasks.md`](tasks.md) — TDD 5 步任务清单
- [`docs/soc_arch/architecture/21-soc-topology-mvp.md`](../../../../docs/soc_arch/architecture/21-soc-topology-mvp.md) — SoC 顶层物理布局规范 SSOT
- [`docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md`](../../../../docs/soc_arch/adr/ADR-SOC-21-v31-rev2-topology-correction.md) — V3.1-Rev2.0 拓扑修正 ADR

---

## §8 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版: V3.1-Rev2.0 拓扑修正设计文档（代码组织 + 模块依赖 + TDD 5 步 + 跨子系统一致性验证矩阵） |