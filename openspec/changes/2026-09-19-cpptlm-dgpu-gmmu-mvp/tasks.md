# 2026-09-19-cpptlm-dgpu-gmmu-mvp: Tasks (7 步 TDD 5 步结构)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/gmmu-mvp/spec.md`](specs/gmmu-mvp/spec.md)
> **关联设计**: [`docs/soc_arch/architecture/20-gmmu-mvp.md`](../../../../docs/soc_arch/architecture/20-gmmu-mvp.md)
> **关联路线图**: [`docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md`](../../../../docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md)
> **工期估算**: 2-3 周(per [`proposal.md` §Impact](../../../../docs/soc_arch/architecture/20-gmmu-mvp.md))

---

## 文件清单

| 文件 | 变化 | 步骤 |
|------|------|:----:|
| `include/tlm/gpu/gmmu_tlm.hh` | **新** | T1-T6 |
| `src/tlm/gpu/gmmu_tlm.cc` | **新** | T1-T6 |
| `include/tlm/gpu/gmmu_ptw_engine.hh` | **新** | T4 |
| `src/tlm/gpu/gmmu_ptw_engine.cc` | **新** | T4 |
| `include/tlm/gpu/gmmu_tlb.hh` | **新** | T3, T6 |
| `src/tlm/gpu/gmmu_tlb.cc` | **新** | T3, T6 |
| `include/tlm/gpu/io_dma_tlm.hh` | **新** (重命名 + 接口替换) | T7 |
| `src/tlm/gpu/io_dma_tlm.cc` | **新** (复制 sdma_engine_tlm + 改接口) | T7 |
| `include/tlm/gpu/sdma_engine_tlm.hh` | **改** (加 `[[deprecated]]`) | T7 |
| `src/tlm/gpu/sdma_engine_tlm.cc` | **改** (加 `[[deprecated]]` 注释) | T7 |
| `include/chstream_register.hh` | **改** (注册 `GmmuTlb` / `GmmuPtwEngine` / `GmmuTLM` / `IoDmaTLM`) | T1-T7 |
| `src/tlm/gpu/dgpu_board_shell.cc` | **改** (注入 GMMU → IO-DMA) | T7 |
| `test/test_gmmu_mmu_enable.cc` | **新** (TDD 5 步) | T1 |
| `test/test_gmmu_context_validate.cc` | **新** (TDD 5 步) | T2 |
| `test/test_gmmu_l1_tlb_hit.cc` | **新** (TDD 5 步) | T3 |
| `test/test_gmmu_l1_tlb_miss_ptw.cc` | **新** (TDD 5 步) | T3 |
| `test/test_gmmu_ptw_4level_walk.cc` | **新** (TDD 5 步) | T4 |
| `test/test_gmmu_page_fault.cc` | **新** (TDD 5 步) | T5 |
| `test/test_gmmu_tlb_rr_eviction.cc` | **新** (TDD 5 步) | T6 |
| `test/test_gmmu_invalidate_full.cc` | **新** (TDD 5 步) | T6 |
| `test/test_gmmu_invalidate_iova.cc` | **新** (TDD 5 步) | T6 |
| `test/test_gmmu_ctx_id_tag.cc` | **新** (TDD 5 步) | T6 |
| `test/test_io_dma_gmmu_pcie_e2e.cc` | **新** (端到端 demo, TDD 5 步) | T7 |
| `docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md` | **新** | T0 |
| `docs/soc_arch/architecture/20-gmmu-mvp.md` | **新** | T0 |
| `docs/soc_arch/architecture/20-gart-module.md` | **改** (顶部加 superseded banner) | T0 |
| `docs/soc_arch/architecture/00-overview.md` | **改** (L1/L7 加 GMMU 引用) | T7 |
| `docs/soc_arch/architecture/17-sdma-engine-design.md` | **改** (顶部加 IO-DMA 重命名说明) | T7 |
| `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` | **改** (§L1 加 GMMU 引用) | T7 |
| `test/CMakeLists.txt` | 不改 (file GLOB 自动发现新测试) | — |

---

## T0: 文档与归档 (1 天)

- [ ] 0.1 创建 `docs/soc_arch/architecture/20-gmmu-evolution-roadmap.md`(5 阶段路线图 SSOT)
- [ ] 0.2 创建 `docs/soc_arch/architecture/20-gmmu-mvp.md`(GMMU v1.0 MVP 详细设计 SSOT)
- [ ] 0.3 `docs/soc_arch/architecture/20-gart-module.md` 顶部加 `> **SUPERSEDED**` banner + 引用 `20-gmmu-mvp.md`(文件内容不修改)
- [ ] 0.4 创建本 `tasks.md`(已完成)

**验收**:
- 2 个新文档存在且内容完整
- GART v0.1 文档顶部加 superseded 标注,引用新文档

---

## T1: GMMU 骨架 + enable gate (TDD 5 步, 0.5 天)

### T1.1 写失败测试 (TDD 红)

- [ ] 1.1.1 创建 `test/test_gmmu_mmu_enable.cc`(双标签 `[gmmu][io_dma]`)
- [ ] 1.1.2 写测试:`GmmuTLM` 类不存在时编译失败
- [ ] 1.1.3 写测试:`GmmuTLM::translate()` 返回 0 + phys=va 当 control.enable=0(bypass 模式)
- [ ] 1.1.4 写测试:`GmmuTLM::translate()` 不存在时编译失败

### T1.2 验证失败

- [ ] 1.2.1 跑 `cmake --build build --target cpptlm_tests -j$(nproc)` 应编译失败(找不到 `GmmuTLM`)
- [ ] 1.2.2 **FAIL** 确认测试在测真实行为

### T1.3 写实现 (TDD 绿)

- [ ] 1.3.1 创建 `include/tlm/gpu/gmmu_tlm.hh`(最小骨架,仅 control + translate 接口)
- [ ] 1.3.2 创建 `src/tlm/gpu/gmmu_tlm.cc`(translate() bypass 模式最小实现)
- [ ] 1.3.3 添加 `chstream_register.hh` 注册 `REGISTER_CHSTREAM(GmmuTLM)`
- [ ] 1.3.4 添加 `test/CMakeLists.txt` 注册新测试

### T1.4 验证通过

- [ ] 1.4.1 跑 `cmake --build build --target cpptlm_tests -j$(nproc)` PASS
- [ ] 1.4.2 跑 `./build/bin/cpptlm_tests "[gmmu]"` PASS
- [ ] 1.4.3 跑 `./build/bin/cpptlm_tests "[pcie]"` 既有测试零回归

### T1.5 重构 + commit

- [ ] 1.5.1 添加 `gmmu_translator_if` 接口(sc_interface 风格)
- [ ] 1.5.2 添加 file-level 注释(类功能 + 作者 + 日期 + ADR 引用)
- [ ] 1.5.3 推迟 commit(per tasks.md 顶部"P5 推迟 commit"惯例)

**验收**:
- `GmmuTLM` 类骨架存在,enable gate 行为正确
- 既有测试零回归

---

## T2: Context 表 + 校验 (TDD 5 步, 0.5 天)

### T2.1 写失败测试

- [ ] 2.1.1 创建 `test/test_gmmu_context_validate.cc`(双标签 `[gmmu][context]`)
- [ ] 2.1.2 写测试:`contexts_[0].enable=0` 时 translate 返回 -EFAULT
- [ ] 2.1.3 写测试:写 `GMMU_REG_CONTEXT_BASE[0]` + `GMMU_REG_CONTEXT_CTRL[0]` MMIO 路径
- [ ] 2.1.4 写测试:`status_fault_ctx_` 记录最近 fault 的 context_id

### T2.2 验证失败

- [ ] 2.2.1 编译失败(无 Context 表 + MMIO 处理)

### T2.3 写实现

- [ ] 2.3.1 `gmmu_tlm.hh` 添加 `MAX_CONTEXTS = 8` + `std::array<GmmuContextEntry, MAX_CONTEXTS> contexts_`
- [ ] 2.3.2 `gmmu_tlm.hh` 添加 `status_fault_va_` + `status_fault_ctx_` 状态字段
- [ ] 2.3.3 `gmmu_tlm.cc` 实现 `handle_context_base_write()` + `handle_context_ctrl_write()` MMIO 处理
- [ ] 2.3.4 `translate()` 主流程添加 Context 校验

### T2.4 验证通过

- [ ] 2.4.1 跑 `[gmmu]` PASS
- [ ] 2.4.2 跑 `[pcie]` 零回归

### T2.5 重构 + commit

- [ ] 2.5.1 验证不变量 2:Context 表是 `std::array` + `MAX_CONTEXTS=8`,v1.0 仅 1 ctx 活跃
- [ ] 2.5.2 推迟 commit

**验收**:
- Context 表数据结构 extensible(`array<ContextEntry, 8>`)
- MMIO 路径处理正确
- 既有测试零回归

---

## T3: L1 TLB + hit/miss (TDD 5 步, 1 天)

### T3.1 写失败测试

- [ ] 3.1.1 创建 `test/test_gmmu_l1_tlb_hit.cc`(双标签 `[gmmu][tlb]`)
- [ ] 3.1.2 创建 `test/test_gmmu_l1_tlb_miss_ptw.cc`(双标签 `[gmmu][tlb][ptw]`)- 先不实现 PTW,只测 miss 路径
- [ ] 3.1.3 写测试:第一次 translate 触发 tlb_fill,第二次命中(计时断言)
- [ ] 3.1.4 写测试:TLB 64 entries 满后,新 entry 替换最旧(Round-Robin)

### T3.2 验证失败

- [ ] 3.2.1 编译失败(无 TLB 类)

### T3.3 写实现

- [ ] 3.3.1 创建 `include/tlm/gpu/gmmu_tlb.hh`(`GmmuTlb` 类)
- [ ] 3.3.2 创建 `src/tlm/gpu/gmmu_tlb.cc`(lookup / fill / invalidate_all / RR 替换)
- [ ] 3.3.3 `GmmuTlbEntry` 结构包含 `va, pa, context_id, valid, perms` 字段(per 不变量 3)
- [ ] 3.3.4 `gmmu_tlm.cc` 添加 `lookup_l1_tlb()` + `tlb_fill()` 调用点
- [ ] 3.3.5 `chstream_register.hh` 注册 `REGISTER_CHSTREAM(GmmuTlb)`

### T3.4 验证通过

- [ ] 3.4.1 跑 `[gmmu]` PASS
- [ ] 3.4.2 跑 `[pcie]` 零回归
- [ ] 3.4.3 验证性能断言(第二次 < 50% PTW)

### T3.5 重构 + commit

- [ ] 3.5.1 验证不变量 3:TLB entry 始终包含 `context_id` 字段
- [ ] 3.5.2 推迟 commit

**验收**:
- L1 TLB 64 entries 全关联 + Round-Robin 替换
- Hit/miss 行为正确
- 既有测试零回归

---

## T4: PTW 4 级 walk (TDD 5 步, 1 天)

### T4.1 写失败测试

- [ ] 4.1.1 创建 `test/test_gmmu_ptw_4level_walk.cc`(三标签 `[gmmu][ptw][walk]`)
- [ ] 4.1.2 写测试:安装 mock PTE 4 级链,验证 walk 成功
- [ ] 4.1.3 写测试:第 N 级 PTE valid=0 时返回 -EFAULT
- [ ] 4.1.4 写测试:leaf PTE 提取 `phys_pa` 字段 + `(va & 0xFFF)` offset

### T4.2 验证失败

- [ ] 4.2.1 编译失败(无 PTW walker 类)

### T4.3 写实现

- [ ] 4.3.1 创建 `include/tlm/gpu/gmmu_ptw_engine.hh`(`GmmuPtwEngine` 类)
- [ ] 4.3.2 创建 `src/tlm/gpu/gmmu_ptw_engine.cc`(4 级 walk 实现)
- [ ] 4.3.3 `GmmuPte` 结构:`valid, is_leaf, perms, next_table` 字段(per x86-64 4KB PTE 格式)
- [ ] 4.3.4 mock 页表 `MockPageTable::install_pte(addr, pte)` + `read_pte(addr)`(per 设计 §6.2)
- [ ] 4.3.5 `gmmu_tlm.cc` 调用 PTW walker 实现 `ptw_walk()` 主流程
- [ ] 4.3.6 `chstream_register.hh` 注册 `REGISTER_CHSTREAM(GmmuPtwEngine)`

### T4.4 验证通过

- [ ] 4.4.1 跑 `[gmmu]` PASS
- [ ] 4.4.2 跑 `[pcie]` 零回归
- [ ] 4.4.3 验证 4 级 walk 性能(40 cycles per walk)

### T4.5 重构 + commit

- [ ] 4.5.1 mock 页表 API 设计预留 v2.0+ 真实路径切换点(per 设计 §6.2)
- [ ] 4.5.2 推迟 commit

**验收**:
- 4 级 walk 成功 + PTE 字段正确解析
- Page Fault 同步返回
- 既有测试零回归

---

## T5: Page Fault 处理 + status 记录 (TDD 5 步, 0.5 天)

### T5.1 写失败测试

- [ ] 5.1.1 创建 `test/test_gmmu_page_fault.cc`(双标签 `[gmmu][fault]`)
- [ ] 5.1.2 写测试:Page Fault 后 `status_fault_va_` + `status_fault_ctx_` 正确记录
- [ ] 5.1.3 写测试:driver 读 `GMMU_REG_STATUS` / `GMMU_REG_STATUS_LO` / `GMMU_REG_FAULT_CTX` 获取完整 fault 信息
- [ ] 5.1.4 写测试:TLB **不**填充 fault entry(失败不入 TLB)

### T5.2 验证失败

- [ ] 5.2.1 编译失败(无 status 寄存器 + MMIO read)

### T5.3 写实现

- [ ] 5.3.1 `gmmu_tlm.cc` 添加 `record_fault(va, ctx_id)` 函数
- [ ] 5.3.2 MMIO read path 处理 `GMMU_REG_STATUS` / `GMMU_REG_STATUS_LO` / `GMMU_REG_FAULT_CTX`
- [ ] 5.3.3 `translate()` 主流程:PTW 失败时不调 `tlb_fill()`(已存在,仅验证)

### T5.4 验证通过

- [ ] 5.4.1 跑 `[gmmu]` PASS
- [ ] 5.4.2 跑 `[pcie]` 零回归

### T5.5 重构 + commit

- [ ] 5.5.1 推迟 commit

**验收**:
- Page Fault 同步返回 -EFAULT
- Status 寄存器可读
- 失败的 entry 不入 TLB
- 既有测试零回归

---

## T6: TLB 替换 + Invalidate 命令 (TDD 5 步, 0.5 天)

### T6.1 写失败测试

- [ ] 6.1.1 创建 `test/test_gmmu_tlb_rr_eviction.cc`(双标签 `[gmmu][tlb][evict]`)
- [ ] 6.1.2 创建 `test/test_gmmu_invalidate_full.cc`(双标签 `[gmmu][invalidate]`)
- [ ] 6.1.3 创建 `test/test_gmmu_invalidate_iova.cc`(双标签 `[gmmu][invalidate]`)
- [ ] 6.1.4 创建 `test/test_gmmu_ctx_id_tag.cc`(双标签 `[gmmu][context]`)
- [ ] 6.1.5 写测试:64 entries 满后 tlb_fill 替换最旧
- [ ] 6.1.6 写测试:INV_FULL 清空所有 entry
- [ ] 6.1.7 写测试:INV_BY_IOVA 仅清 range 内 entry
- [ ] 6.1.8 写测试:INV_BY_CONTEXT 仅清指定 ctx entry
- [ ] 6.1.9 写测试:跨 ctx 访问不命中对方 TLB entry(不变量 3)

### T6.2 验证失败

- [ ] 6.2.1 部分编译失败(INV_BY_CONTEXT / INV_BY_IOVA_AND_CONTEXT 未实现)

### T6.3 写实现

- [ ] 6.3.1 `GmmuTlb` 添加 `tlb_invalidate_by_iova(va_lo, va_hi, ctx_mask)` 实现
- [ ] 6.3.2 `GmmuTlb` 添加 `tlb_invalidate_by_context(ctx_mask)` 实现
- [ ] 6.3.3 `GmmuTlb` 添加 `tlb_invalidate_by_iova_and_context(...)` 实现(组合前两者)
- [ ] 6.3.4 `gmmu_tlm.cc` MMIO 处理 `GMMU_REG_INV` / `GMMU_REG_INV_HI` 寄存器
- [ ] 6.3.5 `InvOp` 枚举定义 `INV_FULL=0, INV_BY_IOVA=1, INV_BY_CONTEXT=2, INV_BY_IOVA_AND_CONTEXT=3`(per 不变量 4)

### T6.4 验证通过

- [ ] 6.4.1 跑 `[gmmu]` PASS(全部 10 个单元测试)
- [ ] 6.4.2 跑 `[pcie]` 零回归

### T6.5 重构 + commit

- [ ] 6.5.1 验证不变量 4:`InvOp` 枚举 4 个值始终定义
- [ ] 6.5.2 推迟 commit

**验收**:
- 4 类 Invalidate 命令全部正确
- 跨 ctx TLB entry 隔离
- 既有测试零回归
- `[gmmu]` 标签下 10 个单元测试全绿

---

## T7: IO-DMA 重命名 + GMMU 接入 + 端到端 (TDD 5 步, 1.5 天)

### T7.1 写失败测试

- [ ] 7.1.1 创建 `test/test_io_dma_gmmu_pcie_e2e.cc`(多标签 `[io_dma][gmmu][pcie][e2e]`)
- [ ] 7.1.2 写测试 6 步端到端 demo(per [`20-gmmu-mvp.md` §2](20-gmmu-mvp.md)):
  - Step 1: driver 写 GMMU_REG_CONTEXT_BASE + GMMU_REG_CTRL
  - Step 2: driver 提交 IO-DMA descriptor
  - Step 3: IO-DMA 调 gmmu.translate → L1 miss → PTW walk
  - Step 4: IO-DMA 经 PCIe EP 发起 MRd TLP
  - Step 5: CplD 返回 → IO-DMA 写 VRAM
  - Step 6: 验证 host backdoor 内容
- [ ] 7.1.3 写测试:第二次同 VA 调用 L1 TLB hit
- [ ] 7.1.4 写测试:Page Fault 时 IO-DMA 记录 fault 不发起 PCIe TLP

### T7.2 验证失败

- [ ] 7.2.1 编译失败(无 `IoDmaTLM` 类)

### T7.3 写实现

- [ ] 7.3.1 创建 `include/tlm/gpu/io_dma_tlm.hh`(复制 sdma_engine_tlm.hh + 重命名类 + 接口替换)
- [ ] 7.3.2 创建 `src/tlm/gpu/io_dma_tlm.cc`(复制 sdma_engine_tlm.cc + 改 `set_translate_cb` → `set_gmmu_translator`)
- [ ] 7.3.3 `include/tlm/gpu/sdma_engine_tlm.hh` 加 `[[deprecated("use IoDmaTLM")]]` 标注
- [ ] 7.3.4 `src/tlm/gpu/sdma_engine_tlm.cc` 顶部加 deprecated 注释
- [ ] 7.3.5 `chstream_register.hh` 注册 `REGISTER_CHSTREAM(IoDmaTLM)`(保留 `REGISTER_CHSTREAM(SdmaEngineTLM)` 过渡期)
- [ ] 7.3.6 `dgpu_board_shell.cc` `setup_dma_chain()` 改用 `IoDmaTLM` + 注入 GMMU 实例(per 设计 §10.2)
- [ ] 7.3.7 `IoDmaTLM::process_h2d()` 调 `gmmu_->translate()` 替代原 `translate_cb_`(per 设计 §10.1)

### T7.4 验证通过

- [ ] 7.4.1 跑 `[io_dma]` PASS(含端到端测试)
- [ ] 7.4.2 跑 `[gmmu]` PASS(10 个单元测试)
- [ ] 7.4.3 跑 `[pcie]` 既有测试零回归(基线 ≥ 36,454)
- [ ] 7.4.4 跑 `[chstream]` 既有测试零回归(基线 155)
- [ ] 7.4.5 跑 `./build/bin/cpptlm_tests "[io_dma][gmmu][pcie][e2e]"` PASS

### T7.5 重构 + commit

- [ ] 7.5.1 验证不变量 1:`gmmu.translate()` API 签名不变
- [ ] 7.5.2 验证 4 条不变量全部满足(per design.md §4.3)
- [ ] 7.5.3 更新 `docs/soc_arch/architecture/00-overview.md` L1 / L7 章节加 GMMU 引用
- [ ] 7.5.4 更新 `docs/soc_arch/architecture/17-sdma-engine-design.md` 顶部加 IO-DMA 重命名说明
- [ ] 7.5.5 更新 `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` §L1 加 GMMU 引用
- [ ] 7.5.6 推迟 commit

**验收**:
- IO-DMA 重命名完成,新旧类并存(过渡期)
- GMMU 注入 IO-DMA 翻译路径
- 6 步端到端 demo 全绿
- 4 条不变量全部满足
- 文档同步完成
- 既有 `[pcie]` / `[chstream]` 测试零回归
- G1-G10 acceptance gate 全过

---

## 跨步 checklist(每步 commit 前必查)

### 4 条无债务演进不变量验证

| 不变量 | 验证方式 |
|--------|---------|
| 1. `phys` 输出语义只增不换 | `gmmu.translate()` 签名 `(uint64_t&, ...)` 未改;无新增输出参数 |
| 2. Context_ID 表 extensible | `std::array<GmmuContextEntry, MAX_CONTEXTS>`;MAX_CONTEXTS=8 |
| 3. TLB entry 包含 Context_ID tag | `GmmuTlbEntry::context_id` 字段始终存在;TLB lookup 包含 ctx_id 匹配 |
| 4. Invalidate 命令分类完整 | `InvOp` 枚举 4 个值始终定义 |

### 跨仓协调 checklist

- [ ] UsrLinuxEmu 仓 change `2026-09-19-cpptlm-dgpu-gmmu-mvp` 是否启动?(由用户协调)
- [ ] 跨仓集成测试 `test_io_dma_gmmu_pcie_e2e_ue.cc` 是否规划?(由 UsrLinuxEmu 端实现)

### 既有测试零回归

- [ ] `[pcie]` ≥ 36,454 assertions PASS
- [ ] `[chstream]` 155 assertions PASS
- [ ] `[sdma]` 既有测试仍 PASS(SdmaEngineTLM 兼容期)
- [ ] `[axi]` 既有测试 PASS

---

## 推迟 commit 节点

per tasks.md 顶部惯例,**所有 commit 推迟到本 change 全部 T1-T7 完成后统一 commit**(per docs/development/CONTRIBUTING.md 的 TDD 5 步纪律)。

中间步骤**不**单独 commit,避免 Git history 碎片化。

---

## 风险与失败恢复

| 风险 | 缓解 |
|------|------|
| T4 mock 页表实现错误 | TDD 红阶段先写 walk 测试,确保测试用例真实反映 walk 行为 |
| T7 端到端失败 | 检查 `dgpu_board_shell.cc` 的 `set_translate_cb` lambda 是否正确桥接 GMMU |
| 4 条不变量破坏 | code review 必查项;每步 commit 前 checklist 强制 |
| 跨仓 driver 协调失败 | 由用户承担(per proposal §Impact R1) |

---

**关联 OpenSpec**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/gmmu-mvp/spec.md`](specs/gmmu-mvp/spec.md)
**下次更新**: Oracle 评审反馈后 v1.1