# dGPU GMMU 演进路线图 (GMMU Evolution Roadmap)

> **目的**: 定义 CppTLM dGPU **GMMU (GPU MMU)** 的多阶段演进路径(v1.0 MVP → v3.0 完整两阶段),确立每个阶段的 **shippable 价值** + **前置依赖** + **不引入项** + **无债务演进约束**,确保整个演进过程不产生"未来需清理"的债务
>
> **状态**: Draft v1.0 (2026-09-19)
> **审计**: 待 Oracle 评审(预期 ≥9.0/10 PASS)
> **归属 OpenSpec**: [`openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/`](../../../openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/proposal.md)
> **关联文档**:
> - [`20-gmmu-mvp.md`](20-gmmu-mvp.md) — **GMMU v1.0 MVP 详细设计**(本路线图第一阶段 SSOT)
> - [`20-gart-module.md`](20-gart-module.md) — **SUPERSEDED** (旧版设计,归档参考)
> - [`00-overview.md`](00-overview.md) §3.1 L1 Host Interface + §3.7 L7 Memory
> - [`17-sdma-engine-design.md`](17-sdma-engine-design.md) — **IO-DMA 引擎**(原 SDMA,本路线图覆盖路径)
> - UsrLinuxEmu change `2026-09-19-cpptlm-dgpu-gmmu-mvp` — driver-side 协调(原 `2026-09-18-cpptlm-iommu-gart` 重命名)
> **关联 ADR**:
> - ADR-088 §D3.8 — `cpptlm_dma_translate_cb` 外部契约(本路线图 v1.0 严格遵守)
> - ADR-088 §D5 — 23 ABI 冻结(本路线图 v1.0 不动 ABI;v2.0 引入时按 ADR-088 §D5 流程扩展)
> - ADR-SOC-09 — v1.0 NVIDIA+AMD dual vendor 战略(GMMU 共享 vendor 路径)
> **关联 Linux 蓝图**:
> - `drivers/iommu/intel/iommu.c` — Intel VT-d(MMU 经典实现)
> - `drivers/gpu/drm/amd/amdgpu/amdgpu_gart.c` — AMD GART(被 GMMU 取代)
> - `drivers/gpu/drm/amd/amdkfd/kfd_migrate.c` — KFD HMM migration(GMMU v2.0 ATS 雏形)
> - NVIDIA Hopper/Blackwell SM 内部 MMU + ATS/PRI(MAS-3.1 提案参考)

---

## §0 阅读引导

- 想理解 GMMU 整体战略 → 读 §1(范围与目标)+ §2(阶段总览)
- 想理解 v1.0 MVP 的具体内容 → 读 §3
- 想理解后续阶段(v1.1 / v2.0 / v2.1 / v3.0)边界 → 读 §4
- 想理解"无债务演进"的不变量 → 读 §5
- 想理解 v1.0 的端到端 demo → 读 §6
- 想理解跨仓契约 → 读 §7
- 想查阅反面模式与不做项 → 读 §8

---

## §1 范围与目标

### §1.1 为什么需要 GMMU 演进路线图

CppTLM dGPU 当前缺失一个**统一的 GPU 端 MMU**:
- **旧方案**: GART v0.1(见 `20-gart-module.md`,未 ship,无代码实现)
  - 1 级 iova→pa 翻译
  - 无 TLB,无 Context_ID,无硬件 PTW,无 Page Fault 处理
- **实际使用**: SDMA engine 通过抽象回调 `cpptlm_dma_translate_cb`(per ADR-088 §D3.8)由 DGpuBoard 注入一个 stub 实现
- **隐患**: 无 Context 隔离、无 TLB 性能、无 Page Fault 处理 → 不可支撑 NVIDIA MIG / AMD Multi-VF / SVA zero-copy 等生产场景

**GMMU 演进的总体目标**: 用 6 个阶段逐步把 GMMU 从"SDMA 可用"建设到"完整两阶段翻译与路由",**每个阶段 shippable + 不制造未来清理债务**。

### §1.2 v1.0 MVP 的核心价值(用户明确目标)

> **"GMMU MVP 可让现在 DMA 支持通过 PCIe EP 来访问系统内存"**

具象化为 1 个端到端测试(demo 详见 §6):

```
IO-DMA 引擎发起 DMA descriptor (iova)
  ↓
GMMU v1.0 MVP 接管 translate_cb_
  ↓
L1 TLB miss → HW PTW walker 走 4 级 host 页表
  ↓
返回 phys (Host PA)
  ↓
IO-DMA 经 PcieEndpointIP 发起 PCIe TLP (MRd/MWr)
  ↓
host memory 真实读写
```

### §1.3 命名约定:SDMA → IO-DMA

为与 GMMU 提案(MAS-3.1 Rev2.0)对齐,本路线图起,**SDMA 重命名为 IO-DMA**:

| 旧名 | 新名 | 原因 |
|------|------|------|
| `SdmaEngineTLM` | `IoDmaTLM` | 与 GMMU 提案一致;强调"IO 域(Host↔Device)DMA 引擎" |
| `sdma_engine_tlm.{hh,cc}` | `io_dma_tlm.{hh,cc}` | 同上 |
| `test_sdma_engine_*.cc` | `test_io_dma_*.cc` | 同上 |
| `cpptlm_dma_translate_cb` | 不变(ABI 冻结) | 跨仓契约保持 |
| `[sdma]` Catch2 标签 | `[io_dma]` | 同上 |

**重要边界**: 命名变更**不**影响跨仓 ABI 签名(per ADR-088 §D5),仅影响仓内代码可读性 + 与 GMMU 提案的概念对齐。代码级重命名为独立任务(见 `tasks.md` §3.2)。

---

## §2 阶段总览(时间线 + 关键约束)

```
v1.0 MVP         v1.1              v2.0             v2.1            v3.0
   ●───────────────●─────────────────●────────────────●───────────────●
   │  IO-DMA only   │  Multi-initiator│  ATS/PRI 硬化   │  NIC-DMA Rx   │  Stage 2
   │  4KB pages     │  Huge page +    │  + per-VF ATC   │  RCT 安全闭环 │  + UDD/HRT
   │  1 Context     │  L2 TLB         │  SVA zero-copy   │                 │
   │  MMIO Inv. 基线 │  Multi-Context  │                  │                 │
```

| 阶段 | 时间估算 | 核心 shippable 价值 | 关键依赖 | 不引入(仍推迟) |
|------|----------|---------------------|----------|------------------|
| **v1.0 MVP** | 2-3 周 | IO-DMA → GMMU → PCIe EP → Host Memory(单 Context,4KB pages,L1 TLB,单级 PTW) | 跨仓 driver 协调就绪(UsrLinuxEmu change `2026-09-19-cpptlm-dgpu-gmmu-mvp`) | L2 TLB / Huge / Multi-init / ATS / PRI / Stage 2 |
| **v1.1** | 1-2 周 | + L2 TLB(共享);+ 2MB Huge page;+ Multi-Context 表(支持 MIG 雏形);+ Invalidate Ring 完整 | v1.0 ship + Page Fault 路径稳定 | ATS / PRI / Stage 2 |
| **v2.0** | 3-4 周 | + ATS Translation Request 协议硬化到 IO-DMA;+ per-VF ATC;+ SVA zero-copy(Host Driver 注册页表根) | v1.1 ship + PcieEndpointIP 加 ATS 能力(per `dgpu-soc-pcie-slice.md §0.1` 高级可选) | Stage 2 / RCT |
| **v2.1** | 2 周 | + NIC-DMA Rx RCT 校验;+ 远端 GUPA 权限闭环 | UALink 立项(无论是否实施 Stage 2) | Stage 2 |
| **v3.0** | 4-6 周 | + Stage 2(GUPA→Route_Tag);+ UDD Hub;+ HRT(per-GUPA 高位查表) | CXL.mem **或** UALink 任一立项 | (无更后项 — 收敛) |

---

## §3 v1.0 MVP 范围(详细)

### §3.1 v1.0 MVP 必含的 7 个最小能力

| # | 能力 | 必含理由 |
|---|------|---------|
| **1** | IO-DMA 可调用的 translate API | 没有这个,IO-DMA 无法用 GMMU |
| **2** | 单级页表 PTW walker | GMMU 与 GART 的本质区别——不引入等于"换皮的 GART" |
| **3** | L1 TLB per IO-DMA(16~64 entries,全关联) | 没有 TLB,每次都 PTW walk → 性能 regression |
| **4** | Context_ID 表(≥1 context,extensible) | 不引入 = 未来 MIG 加 Context_ID 时必重构,违反"无债务"原则 |
| **5** | Page Fault 返回 -EFAULT(非阻塞简化) | IO-DMA 翻译失败时必须返回错误码 |
| **6** | TLB Invalidate MMIO 命令(全表 + 按 iova range) | 不引入 = driver 无 GMMU 维护路径 |
| **7** | VA → Host PA 翻译语义(无 Stage 2) | 兼容 `cpptlm_dma_translate_cb` ABI(per ADR-088 §D3.8) |

### §3.2 v1.0 MVP 推迟到后续阶段的 7 项

| # | 推迟项 | 推迟到 | 不引入原因 |
|---|--------|--------|------------|
| **1** | L2 TLB(共享) | v1.1 | 单 IO-DMA + L1 够用 |
| **2** | Huge Page(2MB / 1GB) | v1.1 | 4KB MVP 先跑通;huge 是性能优化 |
| **3** | Multi-initiator(SM LSU / TC-DMA / TEE Frontend) | v1.1 | v1.0 只服务 IO-DMA |
| **4** | ATS Translation Request 协议 | v2.0 | PcieEndpointIP 当前不支持 ATS(`dgpu-soc-pcie-slice.md §0.1`) |
| **5** | Page Request Interface(PRI) | v2.0 | 同上 |
| **6** | Stage 2(GUPA→Route_Tag) + UDD/HRT | v3.0 | CXL/UALink 未立项 |
| **7** | RCT(NIC-DMA Rx 远端权限) | v2.1 | 远端路径不存在 |

---

## §4 后续阶段详细边界

### §4.1 v1.1: 性能增强 + 多 Context 雏形

| 维度 | 边界 |
|------|------|
| **新增能力** | L2 TLB(per-Context,4-way associative,~256 entries);2MB Huge Page;Multi-Context 表(最多 8 Context,支持 MIG 雏形);Invalidate Ring 完整(per-Context 精准 invalidate) |
| **前置依赖** | v1.0 ship + Page Fault 路径稳定 + Context_ID 数据结构已 extensible |
| **不引入** | ATS / PRI / Stage 2 |
| **验证标准** | L2 TLB hit rate > 80% 在标准 benchmark;Huge page 翻译正确;Context 切换 < 1us |
| **关键文件** | `src/tlm/gpu/gmmu_l2_tlb.{hh,cc}` 新建;`gmmu_tlm.cc` 扩 L2 + Context 表 |

### §4.2 v2.0: ATS/PRI 硬化(IO-DMA SVA zero-copy)

| 维度 | 边界 |
|------|------|
| **新增能力** | ATS Translation Request 协议(PcieEndpointIP 拦截 Host SVA DMA VA,转 GMMU);per-VF ATC(IO-DMA 内部 SRAM);PRI(Page Fault → PCIe PRI Request TLP → 自动重试);Host Driver 注册页表根(经 23 ABI 扩展) |
| **前置依赖** | v1.1 ship + PcieEndpointIP 加 ATS 能力(per `dgpu-soc-pcie-slice.md §0.1` 高级可选阶段启动) + 23 ABI 扩展流程(per ADR-088 §D5) |
| **不引入** | Stage 2 / RCT |
| **验证标准** | Host SVA DMA 完整 zero-copy path;Page Fault → PRI → 自动重试 latency < 100us |
| **关键文件** | `src/tlm/pcie/pcie_endpoint_ip.cc` 加 ATS Translation 拦截;`io_dma_tlm.cc` 加 ATC + PRI 状态机;**23 ABI 扩展**: `cpptlm_emulator_ats_translate_request` 新增 |

### §4.3 v2.1: NIC-DMA Rx RCT 安全闭环

| 维度 | 边界 |
|------|------|
| **新增能力** | NIC-DMA Rx 接收远端发来的 GUPA;UDD Hub 查 RCT 校验远端节点是否有权访问;越权 → 丢弃 + 触发 Security Trap |
| **前置依赖** | UALink 立项(无论是否实施 Stage 2) |
| **不引入** | Stage 2 |
| **验证标准** | 远端越权 GUPA 访问被拦截 + Security Trap 触发;合法远端访问正常完成 |
| **关键文件** | `src/tlm/gpu/nic_dma_rct_tlm.{hh,cc}` 新建;`io_dma_tlm.cc` 加 Rx 路径 |

### §4.4 v3.0: 完整两阶段(Stage 2 GUPA→Route_Tag)

| 维度 | 边界 |
|------|------|
| **新增能力** | Stage 2: GMMU 输出 GUPA 后,UDD Hub 查 HRT(per-GUPA[63:48] 查表)生成 Route_Tag + VC_ID;CXL/UALink 动态路由;CXL Back-invalidate 经 SnoopFilter 精准 invalidate GMMU L2 |
| **前置依赖** | CXL.mem **或** UALink 任一立项 + UDD Hub 模块立项 + HRT 表 |
| **不引入** | (无 — 收敛) |
| **验证标准** | GUPA 高位 [63:48] 正确路由到 HBM/UALink/PCIe;HRT 变更不触发 GMMU TLB 全量刷新 |
| **关键文件** | `src/tlm/gpu/udd_hub.{hh,cc}` + `src/tlm/gpu/hrt_table.{hh,cc}` + `src/tlm/pcie/cxl_mem_endpoint.{hh,cc}` |

---

## §5 无债务演进约束(4 条不变量)

### §5.1 不变量 1: v1.0 MVP 的 "phys" 输出语义在演进中只增不换

> v1.0 MVP ship 后,通过 `cpptlm_dma_translate_cb` ABI 看到的 `phys` 参数含义:
> - **v1.0**: `phys == Host PA`(Stage 2 推迟)
> - **v3.0**: `phys == Host PA + Route_Tag 二元组`(Stage 2 引入)
>
> **承诺**: v3.0 引入时,旧调用点不受破坏,**新增**第二个输出参数 `route_tag` 和 `vc_id`,旧 `phys` 字段保留含义不变。

**实现方式**: v1.0 MVP 的 `translate()` API 已预留 `(phys, route_tag, vc_id)` 元组结构(即使 v1.0 只填 phys),避免 v3.0 改 API 签名。

### §5.2 不变量 2: Context_ID 数据结构从 v1.0 起 extensible

> v1.0 MVP 即定义 Context_ID 表为动态数组(`std::vector<ContextEntry>`),即使 v1.0 只 1 个 Context。
>
> v1.1 增加 Context 时**只追加**数组项,不重构数据结构。

**实现方式**: `gmmu_tlm.hh::contexts_[MAX_CONTEXTS]` 为 vector,初始 size = 1,API 全部按索引访问。

### §5.3 不变量 3: TLB entry 格式从 v1.0 起包含 Context_ID tag

> v1.0 MVP 即使只有 1 Context,TLB entry 也包含 `context_id` 字段(per-entry tag)。
>
> v1.1 多 Context 时,TLB 查找按 (va, context_id) 元组匹配,无 entry 格式重构。

**实现方式**: `tlb_entry_t { va, pa, context_id, valid }`,TLB lookup 始终 `va_match && context_id_match`。

### §5.4 不变量 4: TLB Invalidate MMIO 命令分类从 v1.0 起按 iova-range + context_id

> v1.0 MVP 即使只支持"全表 invalidate",MMIO 命令布局也保留 `(iova_base, iova_limit, context_id_mask)` 字段。
>
> v1.1 增加"按 iova range + Context_ID 精准 invalidate"时**只追加**新命令码,不重写既有命令格式。

**实现方式**: `GMMU_REG_INV` 寄存器为 (op, ctx_id, iova_lo, iova_hi) 四字段,op 枚举为 {FULL, BY_IOVA, BY_CONTEXT, BY_IOVA_AND_CONTEXT}。v1.0 只实现 FULL。

---

## §6 v1.0 MVP 端到端 Shippable Demo(6 步测试)

这是 v1.0 MVP 必须通过的"绿灯测试",证明整个 IO-DMA → GMMU → PCIe EP → Host Memory 链路贯通:

```
┌──────────────────────────────────────────────────────────────────────┐
│  v1.0 MVP Test: test_io_dma_gmmu_pcie_e2e                            │
├──────────────────────────────────────────────────────────────────────┤
│  Step 1: Driver 经 PCIe MMIO 写 GMMU                                  │
│            - GMMU_REG_CONTEXT_BASE[ctx=0] = host_page_table_root       │
│            - GMMU_REG_CTRL = enable                                    │
│                                                                       │
│  Step 2: Driver 经 PCIe BAR 提交 IO-DMA descriptor                   │
│            - iova = 0x4000_0000                                        │
│            - vram_offset = 0x0                                         │
│            - size = 4096                                               │
│            - dir = H2D                                                 │
│                                                                       │
│  Step 3: IO-DMA 引擎发起 H2D                                          │
│            - 调用 gmmu.translate(0x4000_0000, 4096, &phys)            │
│            - GMMU L1 TLB miss                                          │
│            - HW PTW walker 从 root 走 4 级页表(mock in-memory)       │
│            - 命中 → phys = 0x8000_0000                                │
│            - L1 TLB 填充 entry                                         │
│                                                                       │
│  Step 4: IO-DMA 经 PCIe EP 发起 MRd TLP                               │
│            - dst = 0x8000_0000, size = 4096                           │
│            - 经 PcieEndpointIP.req_in[PF] → host backdoor             │
│                                                                       │
│  Step 5: CplD 返回 → IO-DMA 写入 VRAM                                 │
│            - memcpy vram_offset + size bytes                           │
│            - done_out emit completion                                 │
│                                                                       │
│  Step 6: 验证 host memory 状态                                         │
│            - 读取 backdoor 0x8000_0000 内容 == IO-DMA descriptor      │
│            - 第二次相同 iova 请求 → L1 TLB hit(latency < 50% PTW)    │
│            - invalid_context_id 请求 → -EFAULT                        │
└──────────────────────────────────────────────────────────────────────┘
```

**验收标准**: 6 步全部通过 + 测试用例 ≥ 8 个(见 [`20-gmmu-mvp.md` §12 测试覆盖](20-gmmu-mvp.md))。

---

## §7 跨仓契约与 UsrLinuxEmu 协调

### §7.1 跨仓职责边界

| 维度 | CppTLM 仓(本仓)| UsrLinuxEmu 仓(driver 侧)|
|------|------------------|--------------------------|
| **MMIO 寄存器布局** | 定义 GMMU 寄存器(见 `20-gmmu-mvp.md` §4) | driver 按布局写 BAR |
| **Page Table 格式** | 接受 driver 配置的 root,4 级 4KB 页 | driver 维护 host 页表(MMU 子系统) |
| **Context_ID 注入** | 接受 driver 写 Context_BASE 寄存器 | driver 分配 ctx_id + 写入 |
| **Page Fault 返回** | GMMU 内部状态 + 返回 -EFAULT | driver 接收 -EFAULT → 调页 + 重发 |
| **TLB Invalidate** | 提供 MMIO 命令 | driver 主动 invalidate(va unmap / ctx 销毁时) |
| **23 ABI 签名** | `cpptlm_dma_translate_cb` 不变 | driver 端实现回调,内部委托 GMMU |

### §7.2 UsrLinuxEmu 改造路径

原 driver-side change `2026-09-18-cpptlm-iommu-gart` 重命名为 `2026-09-19-cpptlm-dgpu-gmmu-mvp`(由用户在本次协调中明确承担),改造内容:

| 原 GART 实现 | 新 GMMU 实现 | 差异 |
|--------------|--------------|------|
| `gart_map_page(emu, iova, pa, flags)` 写 PTE 到 MMIO | driver 不写 PTE,改写 Context_BASE 寄存器 + MMU 子系统维护 host 页表 | driver 视角从"GART 表"变为"MMU 域" |
| `gart_unmap_page(emu, iova)` 写 invalid PTE | driver 调 MMU 子系统 unmap + 发 TLB invalidate MMIO 命令 | 增加 invalidate 步骤 |
| GART 表是 driver 维护的"副本" | driver 维护**真实的 host 页表**(由 Linux MM 子系统) | 与 GART 设计哲学根本不同 |
| 单 Context 假设 | 多 Context 设计(虽 v1.0 仅 1 ctx,接口预留) | 体现无债务演进约束 |

### §7.3 跨仓 PR 协调

按 ADR-091 §R5.1 跨仓 PR 流程:
1. CppTLM 仓: 实现 GMMU v1.0 MVP 模块 + 8+ 测试(本文档 + tasks.md)
2. UsrLinuxEmu 仓: driver 改造 GART → GMMU 风格(由用户承担协调)
3. 跨仓集成测试: `test_io_dma_gmmu_pcie_e2e.cc`(per §6 demo)
4. 同步 PR(无 ABI 破坏,跨仓风险低)

---

## §8 反模式(明确不做)

| 反模式 | 不做的原因 |
|--------|-----------|
| ❌ **完整 NVIDIA Hopper MMU RTL 复刻** | CppTLM 是 TLM 行为级仿真,非 RTL 仿真(per `00-overview.md §9`) |
| ❌ **多级多发射 PTW 引擎** | v1.0 MVP 单 outstanding PTW 足够仿真;并发 PTW 推迟到 v1.1 |
| ❌ **Host 页表真实 walk 路径** | v1.0 用 mock in-memory 页表;真实路径需 driver 端 host 页表暴露(per §5.2 推荐) |
| ❌ **ATS / PRI 在 v1.0** | PcieEndpointIP 不支持 ATS(per `dgpu-soc-pcie-slice.md §0.1`),v2.0 引入 |
| ❌ **Stage 2 GUPA→Route_Tag 在 v1.0** | CXL/UALink 未立项,无"动态路由"动机 |
| ❌ **嵌套 MMU(guest + host)** | v1.0 MVP 不支持,推到 v3.x |
| ❌ **TLB entry 共享多 process VA range** | TLB 按 (va, context_id) 元组匹配,无 aliasing |
| ❌ **driver 维护 PTE 表副本** | GART 风格,违背 GMMU 设计初衷 |

---

## §9 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版:5 阶段路线图(v1.0/v1.1/v2.0/v2.1/v3.0)+ 4 条不变量 + 6 步 shippable demo + SDMA→IO-DMA 重命名 |

---

**关联 OpenSpec change**: [`openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/`](../../../openspec/changes/2026-09-19-cpptlm-dgpu-gmmu-mvp/proposal.md)
**下次更新**: Oracle 评审反馈后 v1.1