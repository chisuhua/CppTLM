# 2026-09-19-cpptlm-dgpu-gmmu-mvp: dGPU GMMU v1.0 MVP 实现

> **状态**: 📋 Proposed — 2026-09-19
> **目标**: 在 CppTLM dGPU 仓内实现 GMMU v1.0 MVP(7 个最小能力),替代未 ship 的 GART v0.1 设计,达成"IO-DMA → GMMU → PCIe EP → Host Memory"端到端数据通路。同时**重命名 SDMA → IO-DMA** 与 GMMU 提案概念对齐。
> **关联 P1 计划**: `docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md`(v1.0 MVP 阶段 SSOT)
> **关联 P1 详细设计**: `docs/soc_arch/architecture/20-gmmu-mvp.md`(模块 SSOT)
> **关联 Superseeded**: `docs/soc_arch/architecture/20-gart-module.md`(归档,不实施)
> **路线图位置**: GMMU 5 阶段演进路线图 v1.0 MVP 阶段

## Why

CppTLM dGPU 当前在 IO 域地址翻译上有 3 个核心问题:

1. **GART v0.1 设计从未 ship**: `docs/soc_arch/architecture/20-gart-module.md` Draft v0.1 至今无代码实现、无测试、无跨仓 driver 集成。继续按 GART 路线走,会产生"先 ship 一个简化方案再升级"的债务。
2. **实际 IO-DMA 使用 stub 回调**: SDMA engine 通过抽象回调 `cpptlm_dma_translate_cb`(per ADR-088 §D3.8),由 DGpuBoard 注入一个 stub lambda 实现。**没有真实 GPU 端 MMU**,无法支撑生产场景。
3. **概念与 GMMU 提案脱节**: GMMU 提案(MAS-3.1 Rev2.0)是行业标准 GPU MMU 设计;现行 GART 设计反而与 NVIDIA Hopper/Blackwell / AMD CDNA 3 / Intel ATS 等生产硬件的 MMU 设计无映射关系。

**主因(按阻断优先级排序)**:

1. **缺 GPU 端 MMU(最关键)**: IO-DMA 翻译路径无真实硬件模块,driver 端必须通过 host IOMMU/DRM 完成翻译,GMMU 内部的"GUPA→Route_Tag"两阶段设计完全缺失。
2. **无 Context 隔离**: 当前 GART 设计假设单 Context,无法支撑 NVIDIA MIG / AMD Multi-VF 等多租户场景。
3. **无 Page Fault 处理**: 当前 GART 直通模式,不存在 Page Fault 概念,无法支持 SVA zero-copy 路径。
4. **命名不一致**: `SdmaEngineTLM` 命名与 GMMU 提案的 "IO-DMA" 不对齐,造成后续 v2.0/v3.0 演进文档中需要反复解释"SDMA 就是 IO-DMA"。

**预期收益**:

- IO-DMA → GMMU → PCIe EP → Host Memory 端到端数据通路贯通(driver 配置 GMMU 后,IO-DMA 直接访问 host memory)
- 5 阶段无债务演进路径建立(v1.0 → v3.0),每个阶段 shippable + 不制造未来清理债务
- 与 GMMU 行业提案(MAS-3.1)对齐,提升 CppTLM 仿真真实性
- **0 个新 ABI 函数**(per ADR-088 §D5 严格遵守)

## What Changes

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/tlm/gpu/gmmu_tlm.hh` | **新** | `GmmuTLM` 类声明(per [`20-gmmu-mvp.md` §3](20-gmmu-mvp.md)) |
| `src/tlm/gpu/gmmu_tlm.cc` | **新** | `GmmuTLM` 实现(translate / PTW / TLB / Invalidate) |
| `include/tlm/gpu/gmmu_ptw_engine.hh` | **新** | `GmmuPtwEngine` 4 级 walk 引擎(per [`20-gmmu-mvp.md` §6](20-gmmu-mvp.md)) |
| `src/tlm/gpu/gmmu_ptw_engine.cc` | **新** | PTW 实现(单 outstanding, mock in-memory 页表) |
| `include/tlm/gpu/gmmu_tlb.hh` | **新** | `GmmuTlb` 类(64 entries 全关联 + Round-Robin 替换) |
| `src/tlm/gpu/gmmu_tlb.cc` | **新** | TLB lookup / fill / invalidate |
| `include/tlm/gpu/io_dma_tlm.hh` | **新** | `IoDmaTLM` 类声明(原 `SdmaEngineTLM`,per §1.3 重命名) |
| `src/tlm/gpu/io_dma_tlm.cc` | **新** | `IoDmaTLM` 实现(从 sdma_engine_tlm 重命名 + 接入 GMMU) |
| `include/tlm/gpu/sdma_engine_tlm.hh` | **归档** | 重命名后保留 1 版本过渡,加 `[[deprecated]]` 标注 |
| `src/tlm/gpu/sdma_engine_tlm.cc` | **归档** | 同上 |
| `include/chstream_register.hh` | **改** | 注册 `GmmuTLM` + `IoDmaTLM`(重命名 `SdmaEngineTLM → IoDmaTLM`) |
| `src/tlm/gpu/dgpu_board_shell.cc` | **改** | 注入 GMMU → IO-DMA(替代原 `set_translate_cb` lambda stub) |
| `test/test_gmmu_*.cc` | **新** | 10 个单元测试(per [`20-gmmu-mvp.md` §12.1](20-gmmu-mvp.md)) |
| `test/test_io_dma_gmmu_pcie_e2e.cc` | **新** | 端到端 shippable demo 测试(per [`20-gmmu-mvp.md` §12.2](20-gmmu-mvp.md)) |
| `docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md` | **新** | 5 阶段演进路线图(SSOT for evolution) |
| `docs/soc_arch/architecture/20-gmmu-mvp.md` | **新** | GMMU v1.0 MVP 详细设计(SSOT for module) |
| `docs/soc_arch/architecture/20-gart-module.md` | **改** | 顶部加 superseded banner + 引用 `20-gmmu-mvp.md` |
| `docs/soc_arch/architecture/00-overview.md` | **改** | L1 / L7 章节加 GMMU 引用 + SDMA→IO-DMA 迁移注释 |
| `docs/soc_arch/architecture/17-sdma-engine-design.md` | **改** | 顶部加 IO-DMA 重命名迁移说明 + 引用 GMMU |
| `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` | **改** | §L1 章节加 GMMU 引用 + SDMA→IO-DMA 注释 |
| `test/CMakeLists.txt` | **改** | 添加新测试文件(file GLOB 自动发现) |
| `openspec/changes/2026-09-18-cpptlm-iommu-gart/` | **跨仓归档** | UsrLinuxEmu 侧 driver change 重命名(由用户协调)|

## Scope

### IN-SCOPE

- **GmmuTLM 模块**(新 C++ 类,~400 LOC):Context_ID 表 + L1 TLB + PTW walker + Invalidate Ring + Page Fault 处理
- **GmmuPtwEngine**(新 C++ 类,~200 LOC):4 级页表 walk(单 outstanding,v1.0 单级)
- **GmmuTlb**(新 C++ 类,~150 LOC):全关联 64 entries + Round-Robin 替换
- **IoDmaTLM 重命名**(原 SdmaEngineTLM,~660 LOC 复制 + 接口替换):从 `set_translate_cb` lambda 改为 `set_gmmu_translator(gmmu_translator_if*)`
- **MMIO 寄存器布局**:7 个寄存器块(per [`20-gmmu-mvp.md` §4](20-gmmu-mvp.md))
- **5 阶段演进路线图文档**:`20-gmmu-evolution-roadmap.md`
- **10 个单元测试**:`test_gmmu_*.cc`
- **1 个端到端测试**:`test_io_dma_gmmu_pcie_e2e.cc`(per §6 demo)
- **AGENTS.md 同步**:`include/tlm/gpu/` 子目录 AGENTS.md 加 GMMU 条目

### OUT-OF-SCOPE

- L2 TLB(全局 TLB)— 推迟到 v1.1
- Huge Page(2MB / 1GB)— 推迟到 v1.1
- Multi-initiator(SM LSU / TC-DMA)— 推迟到 v1.1
- Multi-Context(实际使用)— 仅 1 context,v1.0 数据结构预留扩展
- ATS Translation Request 协议 — 推迟到 v2.0
- Page Request Interface(PRI)— 推迟到 v2.0
- Stage 2(GUPA→Route_Tag)+ UDD/HRT — 推迟到 v3.0(前置:CXL/UALink 立项)
- RCT(NIC-DMA Rx 远端权限)— 推迟到 v2.1(前置:UALink 立项)
- Host 真实页表 walk(需 driver 暴露)— v1.0 用 mock in-memory
- Page Fault MSI-X 中断 — v1.0 driver polling;v2.0+ 改中断
- 真实 RTL 桥接(CppHDL/HybridCache)— 维持 v2.1 推迟状态
- 23 ABI 扩展 — v1.0 严格 0 个新 ABI 函数(per ADR-088 §D5)
- 性能优化(cycle-accurate 时序模型)— 维持 v0.5 简化基线
- UsrLinuxEmu 仓 driver 改造 — **由用户协调**(单独 change `2026-09-19-cpptlm-dgpu-gmmu-mvp`)

## Acceptance Gate(10 项)

- [ ] **G1**: `openspec validate 2026-09-19-cpptlm-dgpu-gmmu-mvp --strict` PASS
- [ ] **G2**: `cmake --build build --target cpptlm_tests -j$(nproc)` PASS
- [ ] **G3**: `./build/bin/cpptlm_tests "[gmmu]"` PASS(10 个单元测试)
- [ ] **G4**: `./build/bin/cpptlm_tests "[io_dma]"` PASS(含 `test_io_dma_gmmu_pcie_e2e` 端到端测试)
- [ ] **G5**: `./build/bin/cpptlm_tests "[pcie]"` 既有测试零回归(基线 ≥ **44,498** assertions PASS, per AGENTS.md KEY INVARIANTS)
- [ ] **G6**: `./build/bin/cpptlm_tests "[chstream]"` 既有测试零回归(基线 155 assertions PASS)
- [ ] **G7**: AGENTS.md `include/tlm/` 子目录 AGENTS.md 同步 + `20-gart-module.md` 顶部加 superseded banner
- [ ] **G8**: `00-overview.md` L1/L7 + `17-sdma-engine-design.md` + `dgpu-soc-pcie-slice.md` 加 GMMU 引用 + SDMA→IO-DMA 注释
- [ ] **G9**: 跨仓 driver 协调:`UsrLinuxEmu/openspec/changes/2026-09-18-cpptlm-iommu-gart/` 重命名为 `2026-09-19-cpptlm-dgpu-gmmu-mvp`(由用户驱动)
- [ ] **G10**: Oracle 评审 PASS(预期 ≥ 9.0/10)

## Capabilities

### New Capabilities

- `gmmu-mvp`: GMMU v1.0 MVP 完整实现(7 必含能力 + 6 步端到端 shippable demo + 4 条无债务演进不变量)

### Modified Capabilities

- `sdma-engine-tlm`(已存在 spec):**不修改** requirement 集,仅更新模块命名 SDMA → IO-DMA(在 spec 顶部加迁移说明,requiremen 集不变)
- `cpptlm-pcie-endpoint-ip-json-config`(已存在 spec):**不修改**,但未来 v2.0 引入 ATS 时需扩展(per `20-gmmu-evolution-roadmap.md`)

### 涉及 spec(新建 + 修改)

- **新建**: `openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/specs/gmmu-mvp/spec.md`
- **更新引用**: `sdma-engine-tlm/spec.md` 顶部加 IO-DMA 重命名迁移说明

## Impact

**新增代码**(估算):

| 模块 | LOC |
|------|----:|
| `gmmu_tlm.{hh,cc}` | ~400 |
| `gmmu_ptw_engine.{hh,cc}` | ~200 |
| `gmmu_tlb.{hh,cc}` | ~150 |
| `io_dma_tlm.{hh,cc}`(重命名 + 接口替换)| ~660(复制) |
| 单元测试 `test_gmmu_*.cc` × 10 | ~500 |
| 端到端 `test_io_dma_gmmu_pcie_e2e.cc` | ~200 |
| **总计** | **~2110 LOC** |

**修改代码**:

| 文件 | 影响 |
|------|------|
| `dgpu_board_shell.cc` | `set_translate_cb` lambda → `set_gmmu_translator` 指针注入(50 LOC) |
| `chstream_register.hh` | 注册 GmmuTLM + IoDmaTLM(20 LOC) |
| `test/CMakeLists.txt` | file GLOB 自动发现新测试(0 改动) |
| `00-overview.md` / `17-sdma-engine-design.md` / `dgpu-soc-pcie-slice.md` | 添加交叉引用 + SDMA→IO-DMA 迁移注释(~30 LOC 总和) |
| `20-gart-module.md` | 顶部加 superseded banner(~10 LOC) |

**修改 ABI**: **0 个**(per ADR-088 §D5 严格遵守,23 ABI 函数签名不变)

**跨仓影响**:

- UsrLinuxEmu 仓 change `2026-09-18-cpptlm-iommu-gart` 重命名为 `2026-09-19-cpptlm-dgpu-gmmu-mvp`,driver 端从"写 PTE 表"改为"维护 host 页表 + 写 Context_BASE 寄存器 + 写 Invalidate 命令"(由用户协调)
- 23 ABI 函数不变,driver 视角兼容(只是从 `gart_map_page` 改为 `mmio_write`)

**测试覆盖**:

| 标签 | 新增用例 | 既有基线 |
|------|----------|----------|
| `[gmmu]` | 10 | 0(新)| 
| `[io_dma]` | 1(端到端)| 0(替换 `[sdma]`)|
| `[pcie]` | 0 改动 | ≥ **44,498**(零回归, per AGENTS.md KEY INVARIANTS)|
| `[chstream]` | 0 改动 | 155(零回归)|
| `[gmmu][io_dma][pcie][e2e]` | 1(交叉标签)| — |

**风险**:

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| R1 | 跨仓 driver 协调失败 | 🟡 中 | 由用户明确承担;失败时降级为"GMMU driver 端仍走 MMIO 写 PTE 表"风格(违背 GMMU 设计初衷,但保留 GART 兼容性)|
| R2 | IO-DMA 重命名跨文件引用更新遗漏 | 🟢 低 | 自动化 grep + 一次性 commit + 后续回归测试零容忍 |
| R3 | GMMU 模块与 23 ABI 签名不匹配 | 🟢 低 | v1.0 MVP 严格保持 `cpptlm_dma_translate_cb` 原签名不变 |
| R4 | 4 条无债务演进不变量在实施中被破坏 | 🟡 中 | 在 code review checklist 中明确检查 4 条不变量(per [`20-gmmu-mvp.md` §13](20-gmmu-mvp.md))|

**关联 ADR**:

- ADR-088 §D3.8 — `cpptlm_dma_translate_cb` 契约(本 change 严格遵守)
- ADR-088 §D5 — 23 ABI 冻结(本 change 不动 ABI)
- ADR-SOC-09 — v1.0 NVIDIA+AMD dual vendor 战略(GMMU 共享 vendor 路径)

**关联 OpenSpec change**:

- 跨仓 driver 端:UsrLinuxEmu `2026-09-19-cpptlm-dgpu-gmmu-mvp`(由用户协调,原 `2026-09-18-cpptlm-iommu-gart` 重命名)
- 关联 spec:`sdma-engine-tlm`(仅更新命名引用,requirement 集不变)

---

## 关联文档

- **演进路线图**: [`docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md`](../../../../docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md)
- **详细设计**: [`docs/soc_arch/architecture/20-gmmu-mvp.md`](../../../../docs/soc_arch/architecture/20-gmmu-mvp.md)
- **被取代设计**: [`docs/soc_arch/architecture/20-gart-module.md`](../../../../docs/soc_arch/architecture/20-gart-module.md)(SUPERSEDED)

**下次更新**: Oracle 评审反馈后 v1.1