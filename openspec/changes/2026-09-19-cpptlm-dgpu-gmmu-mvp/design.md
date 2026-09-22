# 2026-09-19-cpptlm-dgpu-gmmu-mvp: 设计文档

> **配套**: [`proposal.md`](proposal.md) · [`specs/gmmu-mvp/spec.md`](specs/gmmu-mvp/spec.md) · [`tasks.md`](tasks.md)
> **SSOT**: [`docs/soc_arch/architecture/20-gmmu-mvp.md`](../../../../docs/soc_arch/architecture/20-gmmu-mvp.md)(模块详细设计)+ [`docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md`](../../../../docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md)(5 阶段路线图)

---

## §1 设计概览

本 change 的实现细节 SSOT 已在以下文档充分阐述:

- **模块详细设计**: `docs/soc_arch/architecture/20-gmmu-mvp.md` §1-§16(完整模块类、MMIO 布局、翻译流程、PTW walker、L1 TLB、Context_ID 表、Invalidate、IO-DMA 集成、PCIe EP 集成、测试覆盖、不变量、边界)
- **5 阶段演进路线图**: `docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md` §1-§8(范围与目标、阶段总览、v1.0 范围、后续阶段详细边界、4 条不变量、shippable demo、跨仓契约、反模式)

本设计文档**不再重复**以上内容,仅补充 proposal 中**实施期**特有的设计决策(代码组织、模块依赖、迁移策略、TDD 顺序)。

---

## §2 代码组织(实施期)

### §2.1 新增文件清单(与 proposal §What Changes 一致)

```
include/tlm/gpu/
├── gmmu_tlm.hh            # GMMU 类声明(主入口)
├── gmmu_ptw_engine.hh     # PTW walker 类声明
├── gmmu_tlb.hh            # TLB 类声明
├── io_dma_tlm.hh          # IO-DMA 类声明(从 sdma_engine_tlm 重命名)
└── (sdma_engine_tlm.hh   # 归档,加 [[deprecated]])

src/tlm/gpu/
├── gmmu_tlm.cc            # GMMU 实现
├── gmmu_ptw_engine.cc     # PTW 实现(单 outstanding + mock 页表)
├── gmmu_tlb.cc            # TLB 实现(全关联 64 entries + RR)
├── io_dma_tlm.cc          # IO-DMA 实现(从 sdma_engine_tlm 复制 + 改接口)
└── (sdma_engine_tlm.cc   # 归档,加 [[deprecated("use IoDmaTLM")]])

test/
├── test_gmmu_mmu_enable.cc          # 单元测试 1: enable gate
├── test_gmmu_context_validate.cc    # 单元测试 2: Context 校验
├── test_gmmu_l1_tlb_hit.cc          # 单元测试 3: TLB hit
├── test_gmmu_l1_tlb_miss_ptw.cc     # 单元测试 4: TLB miss → PTW
├── test_gmmu_ptw_4level_walk.cc     # 单元测试 5: 4 级 walk
├── test_gmmu_page_fault.cc          # 单元测试 6: Page Fault 处理
├── test_gmmu_tlb_rr_eviction.cc     # 单元测试 7: RR 替换
├── test_gmmu_invalidate_full.cc     # 单元测试 8: INV_FULL
├── test_gmmu_invalidate_iova.cc     # 单元测试 9: INV_BY_IOVA
├── test_gmmu_ctx_id_tag.cc          # 单元测试 10: Context_ID tag
└── test_io_dma_gmmu_pcie_e2e.cc     # 端到端测试(per §6 demo)
```

### §2.2 模块依赖图

```
DGpuBoard (C++ shell)
  │
  ├─ creates → GmmuTLM (ModuleFactory::instantiate<GmmuTLM>)
  │              │
  │              ├─ composes → GmmuTlb (L1 TLB)
  │              └─ composes → GmmuPtwEngine (PTW walker)
  │
  ├─ creates → IoDmaTLM (ModuleFactory::instantiate<IoDmaTLM>)
  │              │
  │              └─ injected translator → GmmuTLM::translator port
  │
  └─ registers → PcieEndpointIP::mmio_target["gmmu_aperture"] ← GmmuTLM::aperture_port
```

### §2.3 关键设计决策(代码层)

| 决策点 | 选择 | 理由 |
|--------|------|------|
| GmmuTLM 命名空间 | `cpptlm::gpu` | 与现有 gpu 域模块对齐(per `dgpu-soc-pcie-slice.md` 命名空间惯例) |
| GmmuTlb 数据结构 | `std::array<GmmuTlbEntry, 64>` + Round-Robin | v1.0 简化;v1.1+ 可换 LRU |
| GmmuPtwEngine outstanding 数 | 1(v1.0 单 outstanding)| v1.0 简化;v1.1+ 可加队列 |
| Host 页表存储 | mock in-memory (`std::unordered_map<uint64_t, GmmuPte>`) | v1.0 MVP 链路贯通优先;v2.0+ 切换真实路径 |
| PTW 触发时机 | TLB miss 同步触发(无异步) | v1.0 简化;性能 v1.1+ 优化 |
| Page Fault 处理 | 同步返回 -EFAULT(无异步 replay) | v1.0 MVP 简化;v1.1+ 加 Replay Buffer |
| IO-DMA 重命名策略 | 新建 io_dma_tlm.{hh,cc} + sdma_engine_tlm.{hh,cc} 归档 | 避免一次性 commit 跨多个文件;保留过渡期兼容 |

---

## §3 迁移策略

### §3.1 SDMA → IO-DMA 重命名 3 阶段

**阶段 1: 引入新文件(本 change)**

- 创建 `io_dma_tlm.{hh,cc}`,内容是 `sdma_engine_tlm.{hh,cc}` 的**复制** + 重命名 + 接口替换
- 旧文件 `sdma_engine_tlm.{hh,cc}` 保留,加 `[[deprecated("use IoDmaTLM")]]`
- `chstream_register.hh` 注册新类 `REGISTER_CHSTREAM(IoDmaTLM)`,旧类 `REGISTER_CHSTREAM(SdmaEngineTLM)` 保留(过渡期)

**阶段 2: 切换消费者(本 change 验收前完成)**

- `dgpu_board_shell.cc`: 改用 `IoDmaTLM` 实例
- 测试代码: 新测试用 `IoDmaTLM`;旧测试用 `SdmaEngineTLM` 验证兼容
- Catch2 标签: `[io_dma]` 与 `[sdma]` 并存(v1.0 MVP 兼容期)

**阶段 3: 归档旧类(本 change 后续独立 change)**

- 提议独立 change `2026-XX-XX-cpptlm-archive-sdma-engine-tlm`
- 删除 `sdma_engine_tlm.{hh,cc}` 文件
- `chstream_register.hh` 移除 `REGISTER_CHSTREAM(SdmaEngineTLM)`
- Catch2 标签移除 `[sdma]`
- **本 change 不包含此阶段**,避免范围蔓延

### §3.2 GART v0.1 归档策略

- `20-gart-module.md` 顶部加 `> **SUPERSEDED**` banner
- 引用新文档 `20-gmmu-mvp.md`
- **不删除**文件(保留作为历史参考,per `docs/docs_audit_report.md` VIRTUAL_PATHS 约定)
- 文件内容**不修改**(避免 Git history 污染)

### §3.3 23 ABI 严格不动

- `cpptlm_emulator_register_dma_translate_cb` 签名不变
- GMMU 内部实现 `int translate(uint64_t va, uint32_t size, uint64_t& pa)` 严格匹配
- `dgpu_board_shell.cc` 内部把 GMMU 实例注入到 `set_translate_cb` lambda

```cpp
// 旧(纯 stub):
sdma.set_translate_cb([](uint64_t iova, uint32_t size, uint64_t& phys) -> int {
    phys = iova;  // identity
    return 0;
});

// 新(GMMU 接入):
auto* gmmu = ...; // ModuleFactory 构造的 GmmuTLM 实例
sdma.set_translate_cb([gmmu](uint64_t va, uint32_t size, uint64_t& phys) -> int {
    return gmmu->translate(va, size, phys);  // 调用 GMMU translate
});
```

### §3.4 ModuleFactory 注册顺序

按 `chstream_register.hh` 现有模式,新模块按依赖顺序注册:

```cpp
// chstream_register.hh 末尾追加:
REGISTER_CHSTREAM(GmmuTlb)         // 无内部依赖
REGISTER_CHSTREAM(GmmuPtwEngine)   // 依赖 GmmuTlb mock
REGISTER_CHSTREAM(GmmuTLM)         // 依赖 GmmuTlb + GmmuPtwEngine
REGISTER_CHSTREAM(IoDmaTLM)        // 新类(原 SdmaEngineTLM 重命名)
// 旧类保留:REGISTER_CHSTREAM(SdmaEngineTLM) [deprecated]
```

---

## §4 TDD 顺序(per tasks.md)

### §4.1 TDD 5 步纪律

每个新模块严格遵循 5 步:

1. **写失败测试**(TDD 红):编译失败 + 运行时断言失败
2. **验证失败**:`./build/bin/cptltm_tests [gmmu]` 编译/运行失败(确认测试在测真实行为)
3. **写实现**(TDD 绿):最小可工作实现
4. **验证通过**:`./build/bin/cptltm_tests [gmmu]` PASS
5. **重构 + commit**:改善可读性 + 推迟 commit

### §4.2 关键 TDD 任务(7 步)

per `tasks.md` 详细:

- **T1**: GMMU 骨架 + 测试 1(enable gate)— TDD 红 → 绿
- **T2**: Context 表 + 测试 2(Context 校验)— TDD 红 → 绿
- **T3**: L1 TLB + 测试 3-4(hit / miss + PTW)— TDD 红 → 绿
- **T4**: PTW 4 级 walk + 测试 5(4 级 walk)— TDD 红 → 绿
- **T5**: Page Fault 处理 + 测试 6(Page Fault)— TDD 红 → 绿
- **T6**: TLB 替换 + Invalidate + 测试 7-10(全部 Invalidate 路径)— TDD 红 → 绿
- **T7**: IO-DMA 重命名 + 端到端测试 + 测试 `test_io_dma_gmmu_pcie_e2e`— TDD 红 → 绿

### §4.3 4 条不变量验证(代码层)

每步 commit 前**必须**验证 4 条不变量在代码层未破坏:

| 不变量 | 验证方式 |
|--------|---------|
| 1. `phys` 输出语义 | `gmmu.translate()` API 签名 `(uint64_t&, ...)` 不变;无新增参数 |
| 2. Context_ID 表 extensible | `std::array<GmmuContextEntry, MAX_CONTEXTS>` 始终是 array,MAX_CONTEXTS=8 |
| 3. TLB entry 包含 Context_ID tag | `GmmuTlbEntry::context_id` 字段始终存在,TLB lookup 包含 ctx_id 匹配 |
| 4. Invalidate 命令分类完整 | `InvOp` 枚举 4 个值(FULL/BY_IOVA/BY_CONTEXT/BY_IOVA_AND_CONTEXT)始终定义 |

---

## §5 失败恢复

### §5.1 单步失败恢复

- **TDD 红无法变绿**: 检查 mock 页表实现是否正确(最常见错误);检查 Context 表 pt_root 是否设置
- **编译错误**: 检查 namespace 是否正确(`cpptlm::gpu`);检查 include 顺序
- **测试运行失败**: 检查时序假设(Catch2 `[!shouldfail]` 标注意外失败)

### §5.2 跨步失败恢复

- **T2 失败影响 T3+**: Context 表是 TLB/PTW 的前置,先修 T2 再继续
- **T7 端到端失败**: 检查 IO-DMA → GMMU 接线(`dgpu_board_shell.cc` 的 `set_translate_cb` lambda)

### §5.3 跨仓 driver 协调失败

per proposal §Impact R1: 降级为"GMMU driver 端仍走 MMIO 写 PTE 表"风格
- v1.0 MVP 设计前提被打破
- 需要**追加** change 修复
- 不影响本 change 的 CppTLM 侧代码

---

## §6 关联变更追踪

### §6.1 已存在的相关 change

- `2026-09-18-cpptlm-iommu-gart`(UsrLinuxEmu 侧,Proposed)— **跨仓 driver change 待重命名**
- `cpptlm-p2-integration-unblock`(Active)— 含 SDMA 端口扩展,可能与 IO-DMA 重命名冲突,**需协调**

### §6.2 后续 change 提议

| 提议 change | 范围 | 时间 |
|------------|------|------|
| `2026-10-XX-cpptlm-gmmu-v1.1` | L2 TLB + Huge Page + Multi-Context | v1.0 ship 后 |
| `2026-XX-XX-cpptlm-archive-sdma-engine-tlm` | 删除 SdmaEngineTLM 类(本 change 的阶段 3)| IO-DMA 切换消费者完成后 |
| `2027-XX-XX-cpptlm-gmmu-v2.0-ats` | ATS Translation 协议硬化 | PcieEndpointIP 加 ATS 能力后 |
| `2027-XX-XX-cpptlm-gmmu-v3.0-stage2` | Stage 2 GUPA→Route_Tag + UDD/HRT | CXL/UALink 任一立项后 |

---

## §7 风险与缓解

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| R1 | 跨仓 driver 协调失败 | 🟡 中 | 由用户承担;失败时降级(per proposal §Impact)|
| R2 | IO-DMA 重命名跨文件引用遗漏 | 🟢 低 | 自动化 grep + 一次性 commit |
| R3 | GMMU 模块与 23 ABI 签名不匹配 | 🟢 低 | v1.0 严格保持原签名 |
| R4 | 4 条不变量在实施中被破坏 | 🟡 中 | code review checklist 强制检查 |
| R5 | `cpptlm-p2-integration-unblock` 冲突 | 🟡 中 | 实施前先 check 该 change 的 SDMA 端口扩展是否已完成;如冲突,等待其合并后再实施 |
| R6 | Mock 页表与未来真实页表 API 不一致 | 🟡 中 | v1.0 mock API 设计预留 v2.0 切换点(install_pte + read_pte 接口) |

---

## §8 维护记录

| 日期 | 版本 | 作者 | 修订 |
|------|------|------|------|
| 2026-09-19 | v1.0-draft | Sisyphus | 首版:实施期设计补充(代码组织 + 迁移策略 + TDD 顺序 + 不变量验证 + 失败恢复)|

---

**关联 OpenSpec**: [`proposal.md`](proposal.md) · [`specs/gmmu-mvp/spec.md`](specs/gmmu-mvp/spec.md) · [`tasks.md`](tasks.md)
**下次更新**: Oracle 评审反馈后 v1.1