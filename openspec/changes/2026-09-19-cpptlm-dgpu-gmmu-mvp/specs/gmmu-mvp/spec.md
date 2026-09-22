# gmmu-mvp: GMMU v1.0 MVP 模块规范

> **所属 change**: [`2026-09-19-cpptlm-dgpu-gmmu-mvp`](../proposal.md)
> **范围**: GMMU v1.0 MVP(7 必含能力)+ IO-DMA 重命名 + 端到端 shippable demo
> **关联 spec**: `sdma-engine-tlm`(仅迁移命名,requirement 集不变)

---

## ADDED Requirements

### Requirement: gmmu-translate-api

`GmmuTLM` SHALL 提供 `int translate(uint64_t va, uint32_t size, uint64_t& pa)` 翻译查询接口,签名与 `cpptlm_dma_translate_cb` (per ADR-088 §D3.8) **完全一致**。返回值: `0 = 成功(pa 填入)`; `<0 = errno` (v1.0 MVP 仅 `-EFAULT = Page Fault`)。

#### Scenario: 正常翻译成功

- **WHEN** 调用 `gmmu.translate(0x4000_0000, 4096, pa)`
- **AND** GMMU 已 enable + Context 0 已 enable + Context 0 的 pt_root 设置正确
- **THEN** GMMU 内部走 L1 TLB lookup → (hit 或 miss + PTW) → 返回 0 + `pa = 0x8000_0000`(假设 PTW 命中)
- **AND** `pa` 值是 4KB aligned Host Physical Address

#### Scenario: 翻译未启用直通

- **WHEN** 调用 `gmmu.translate(0x4000_0000, 4096, pa)`
- **AND** GMMU `control.enable == 0`
- **THEN** 返回 0 + `pa = va`(直通模式,等价于 GART v0.1 的 bypass)
- **AND** 不触发 PTW walker

#### Scenario: Page Fault 返回

- **WHEN** 调用 `gmmu.translate(0x4000_0000, 4096, pa)`
- **AND** L1 TLB miss + PTW walker 在 mock 主机页表找不到对应 PTE
- **THEN** 返回 `-EFAULT` (= -14)
- **AND** `pa` 不修改(调用方可安全忽略)
- **AND** GMMU 内部记录 `status_fault_va_ = 0x4000_0000` + `status_fault_ctx_ = 0`

### Requirement: gmmu-l1-tlb-cache

`GmmuTLM` SHALL 提供 L1 TLB,容量 64 entries,全关联,Round-Robin 替换策略。TLB entry 包含 `(va, pa, context_id, valid, perms)` 字段,**`context_id` 字段从 v1.0 起即存在**(per [`20-gmmu-mvp.md` §13.3 不变量 3](20-gmmu-mvp.md))。

#### Scenario: TLB hit 性能优化

- **WHEN** 第一次 `translate(0x4000_0000, 4096, pa)` 触发 PTW walk + tlb_fill
- **AND** 第二次相同 VA 调用 `translate(0x4000_0000, 4096, pa)`
- **THEN** 第二次命中 L1 TLB,跳过 PTW
- **AND** `tlb_hit_rate() > 0`(统计递增)

#### Scenario: TLB miss 触发 PTW

- **WHEN** `translate(0x4000_0000, 4096, pa)` 首次调用,TLB 空
- **THEN** L1 TLB lookup 返回 miss
- **AND** 调用 `ptw_walk(va, ctx_id, pa)` 走 4 级 mock 页表
- **AND** 成功后调用 `tlb_fill()` 填充 entry
- **AND** 后续同 VA 命中

#### Scenario: TLB Round-Robin 替换

- **WHEN** 64 个 entry 全部 valid 后调用 `tlb_fill(va_new, ctx_id, pa_new, perms)`
- **THEN** 按 `l1_tlb_evict_idx_` 替换最旧 entry
- **AND** `l1_tlb_evict_idx_ = (l1_tlb_evict_idx_ + 1) % 64`(循环)

#### Scenario: Context_ID tag 隔离

- **WHEN** Context 0 的 `translate(0x4000_0000, 4096, pa)` 触发 tlb_fill(entry[0].context_id = 0)
- **AND** Context 1 的 `translate(0x4000_0000, 4096, pa)` 查询(假设 v1.1+ 启用)
- **THEN** Context 1 不命中 Context 0 的 entry(va 匹配但 context_id 不匹配)
- **AND** Context 1 走自己的 PTW(可能 Page Fault 因 Context 1 未配置)

### Requirement: gmmu-hw-ptw-walker

`GmmuPtwEngine` SHALL 实现 4 级页表 walk(v1.0 MVP 仅 4KB 页,无 huge page),单 outstanding(v1.0 简化)。PTE 格式与 x86-64 4KB 页表兼容。

#### Scenario: 4 级 walk 成功

- **WHEN** `ptw_walk(0x4000_0000, ctx_id=0, pa)` 从 `contexts_[0].pt_root` 开始
- **AND** mock 主机页表正确安装 L3 → L2 → L1 → leaf 4 级 PTE
- **THEN** 返回 0 + `pa = (leaf.phys_pa << 12) | (0x4000_0000 & 0xFFF)` = `0x8000_0000`
- **AND** 4 级 walk 共 4 次 `read_pte()` 调用

#### Scenario: PTE invalid 触发 Page Fault

- **WHEN** `ptw_walk(0x4000_0000, ctx_id=0, pa)`
- **AND** 第 N 级 PTE `valid == 0`
- **THEN** 立即返回 -EFAULT
- **AND** `status_fault_va_ = 0x4000_0000`
- **AND** `status_fault_ctx_ = 0`

#### Scenario: Mock in-memory 页表(单测 setup)

- **WHEN** 测试 setup 中调用 `gmmu_tlm.install_pte(root_addr, leaf_pte)`
- **THEN** GMMU 内部 mock 页表存储该 PTE
- **AND** PTW walker 调用 `read_pte(root_addr)` 时返回该 leaf_pte
- **AND** v2.0+ 切换为真实 host 页表路径时,`install_pte()` API 可平滑替换为 `read_remote_pte()` ABI 回调

### Requirement: gmmu-context-id-table

`GmmuTLM` SHALL 维护 `std::array<GmmuContextEntry, MAX_CONTEXTS>` (MAX_CONTEXTS=8) Context 表,**v1.0 MVP 仅 1 个活跃 Context,但数据结构 extensible**(per [`20-gmmu-mvp.md` §13.2 不变量 2](20-gmmu-mvp.md))。

#### Scenario: 单 Context 启用(v1.0 MVP 默认)

- **WHEN** driver 写 `GMMU_REG_CONTEXT_BASE[0] = host_pt_root`
- **AND** driver 写 `GMMU_REG_CONTEXT_CTRL[0] = 0x1` (enable)
- **THEN** `contexts_[0] = {pt_root=host_pt_root, asid=0, enable=1}`
- **AND** 其他 context 表项保持 `{pt_root=0, asid=0, enable=0}`

#### Scenario: Context 校验

- **WHEN** 调用 `gmmu.translate(va, size, pa)`
- **AND** `contexts_[0].enable == 0`
- **THEN** 返回 -EFAULT
- **AND** `status_fault_va_ = va`
- **AND** `status_fault_ctx_ = 0`

### Requirement: gmmu-tlb-invalidate-commands

`GmmuTLM` SHALL 提供 4 类 TLB Invalidate MMIO 命令,MMIO 寄存器布局从 v1.0 起即分类完整(per [`20-gmmu-mvp.md` §13.4 不变量 4](20-gmmu-mvp.md)): `INV_FULL=0`、`INV_BY_IOVA=1`、`INV_BY_CONTEXT=2`、`INV_BY_IOVA_AND_CONTEXT=3`。v1.0 MVP 实现全部 4 类。

#### Scenario: INV_FULL

- **WHEN** driver 写 `GMMU_REG_INV = op=0`
- **THEN** L1 TLB 全部 64 entry `valid = 0`
- **AND** `l1_tlb_evict_idx_ = 0`(重置替换指针)

#### Scenario: INV_BY_IOVA

- **WHEN** driver 写 `GMMU_REG_INV = op=1 | ctx_mask=1 | iova_lo=0x4000_0000`
- **AND** driver 写 `GMMU_REG_INV_HI = iova_hi=0x4000_0FFF`
- **THEN** L1 TLB 中所有 `entry.va ∈ [0x4000_0000, 0x4000_0FFF]` 且 `entry.context_id ∈ ctx_mask` 的 entry `valid = 0`

#### Scenario: INV_BY_CONTEXT

- **WHEN** driver 写 `GMMU_REG_INV = op=2 | ctx_mask=1`(仅 ctx 0)
- **THEN** L1 TLB 中所有 `entry.context_id = 0` 的 entry `valid = 0`
- **AND** 其他 context 的 entry 保持

#### Scenario: INV_BY_IOVA_AND_CONTEXT

- **WHEN** driver 写 `GMMU_REG_INV = op=3 | ctx_mask=1 | iova_lo=0x4000_0000`
- **THEN** L1 TLB 中所有 `entry.va ∈ [iova_lo, iova_hi]` 且 `entry.context_id = 0` 的 entry `valid = 0`
- **AND** v1.0 MVP 实现为 INV_BY_IOVA + INV_BY_CONTEXT 的组合

### Requirement: gmmu-mmu-register-layout

`GmmuTLM` MMIO 寄存器 SHALL 位于 PCIe BAR 内 0x5000-0x6FFF 区间(per [`20-gmmu-mvp.md` §4.1](20-gmmu-mvp.md)),通过 `PcieEndpointIP::mmio_target["gmmu_aperture"]` 路由。

#### Scenario: MMIO 控制寄存器读取

- **WHEN** driver 读 `GMMU_REG_CTRL` (offset 0x5000)
- **THEN** 返回 `{enable=current_state, max_contexts=1 (v1.0 MVP 固定), reserved=0}`

#### Scenario: MMIO Context 寄存器写入

- **WHEN** driver 写 `GMMU_REG_CONTEXT_BASE[0]` (offset 0x5020) = host_pt_root
- **THEN** `contexts_[0].pt_root = host_pt_root`

#### Scenario: MMIO Context 启用

- **WHEN** driver 写 `GMMU_REG_CONTEXT_CTRL[0]` (offset 0x5024) = 0x1
- **THEN** `contexts_[0].enable = 1`
- **AND** `contexts_[0].asid = 0`(低 16 位)

#### Scenario: MMIO Invalidate 寄存器写入

- **WHEN** driver 写 `GMMU_REG_INV` (offset 0x6000) = 命令值
- **THEN** 解析 `op` + `ctx_mask` + `iova_lo`
- **AND** 如 `op == 3` (BY_IOVA_AND_CONTEXT) 进一步读 `GMMU_REG_INV_HI` (offset 0x6008)
- **AND** 执行对应 invalidate 操作

### Requirement: gmmu-page-fault-handling

`GmmuTLM` SHALL 提供 Page Fault 检测与状态记录,返回 `-EFAULT` 给 IO-DMA 翻译调用方。v1.0 MVP **不**触发 MSI-X 中断,driver 通过 polling `GMMU_REG_STATUS` 识别 fault(per [`20-gmmu-mvp.md` §14.1](20-gmmu-mvp.md) v1.0 边界)。

#### Scenario: 同步 Page Fault

- **WHEN** PTW walker 返回 -EFAULT
- **THEN** `translate()` 调用方收到 `-EFAULT`
- **AND** `status_fault_va_` + `status_fault_ctx_` 记录最近 fault 信息
- **AND** `tlb_fill()` **不**被调用(失败的 entry 不入 TLB)

#### Scenario: driver 通过 STATUS 寄存器识别 fault

- **WHEN** driver 读 `GMMU_REG_STATUS` (offset 0x5004)
- **THEN** 返回 `{enabled, fault_valid, fault_iova_hi}`
- **AND** driver 进一步读 `GMMU_REG_STATUS_LO` (0x5008) + `GMMU_REG_FAULT_CTX` (0x500C) 获取完整 fault VA + ctx

### Requirement: io-dma-rename-sdma-to-io-dma

`SdmaEngineTLM` SHALL 重命名为 `IoDmaTLM`(per [`20-gmmu-mvp.md` §1.3](20-gmmu-mvp.md) 与 GMMU 提案对齐)。原类**保留并加 `[[deprecated("use IoDmaTLM")]]`** 标注,作为过渡期兼容。

#### Scenario: 仓内代码使用新类名

- **WHEN** 新代码 `instantiate<IoDmaTLM>(name, eq)` 经 ModuleFactory 构造
- **THEN** 返回 `IoDmaTLM*` 实例
- **AND** `chstream_register.hh` 注册的 `REGISTER_CHSTREAM(IoDmaTLM)` 触发

#### Scenario: 旧代码使用旧类名(过渡期)

- **WHEN** 旧代码 `instantiate<SdmaEngineTLM>(name, eq)` 经 ModuleFactory 构造
- **THEN** 编译器发出 `[[deprecated]]` 警告
- **AND** 仍返回 `SdmaEngineTLM*` 实例(过渡期兼容)
- **AND** 后续独立 change 移除旧类时,改代码改为 `IoDmaTLM`

#### Scenario: Catch2 标签迁移

- **WHEN** v1.0 MVP 实施期间新测试用 `[io_dma]` 标签
- **AND** 旧测试用 `[sdma]` 标签(v1.0 期间并存)
- **THEN** `./build/bin/cpptlm_tests "[io_dma]"` 包含新测试
- **AND** `./build/bin/cpptlm_tests "[sdma]"` 包含旧测试(过渡期)
- **AND** 后续独立 change 删除旧类时移除 `[sdma]` 标签

### Requirement: io-dma-gmmu-translator-injection

`IoDmaTLM` SHALL 提供 `set_gmmu_translator(gmmu_translator_if*)` 注入 API,**替代**原 `set_translate_cb(DmaTranslateCb)`(由 DGpuBoard 注入 lambda stub)。注入后,IO-DMA 翻译请求经 GMMU 实例处理。

#### Scenario: GMMU 注入后翻译路径

- **WHEN** DGpuBoard 调用 `io_dma.set_gmmu_translator(&gmmu_translator)`
- **AND** IO-DMA 发起 `process_h2d(d)` 内部调用 `gmmu->translate(d.host_iova, d.size, phys)`
- **THEN** 翻译请求进入 GMMU 模块
- **AND** GMMU 返回 0 + phys 或 -EFAULT
- **AND** IO-DMA 据此发起 PCIe TLP 或记录 fault

#### Scenario: GMMU 注入前(过渡期兼容)

- **WHEN** DGpuBoard 仍用旧 API `io_dma.set_translate_cb(lambda)`
- **THEN** IO-DMA 调用 lambda,不走 GMMU(等同原 v0.5 stub 行为)
- **AND** 旧测试(用 lambda)继续通过

### Requirement: end-to-end-io-dma-gmmu-pcie-host

`test_io_dma_gmmu_pcie_e2e.cc` SHALL 验证完整 6 步端到端数据通路(per [`20-gmmu-mvp.md` §2](20-gmmu-mvp.md) shippable demo): driver 配 GMMU → driver 提交 IO-DMA descriptor → IO-DMA 调 GMMU translate → GMMU PTW walk → IO-DMA 经 PCIe EP 发起 TLP → host backdoor 验证。

#### Scenario: 完整 H2D 流程

- **WHEN** Step 1: driver 写 `GMMU_REG_CONTEXT_BASE[0] = mock_pt_root` + `GMMU_REG_CTRL = 0x1`
- **AND** Step 2: driver 经 PCIe BAR 提交 IO-DMA descriptor {iova=0x4000_0000, vram_offset=0x0, size=4096, dir=H2D}
- **AND** Step 3: IO-DMA 调 `gmmu.translate(0x4000_0000, 4096, phys)` → L1 miss → PTW walk 成功 → `phys = 0x8000_0000`
- **AND** Step 4: IO-DMA 经 PCIe EP 发起 MRd TLP {addr=0x8000_0000, size=4096}
- **AND** Step 5: CplD 返回 → IO-DMA memcpy vram_offset + size bytes
- **THEN** Step 6: host backdoor 0x8000_0000 内容 == IO-DMA descriptor data

#### Scenario: L1 TLB hit on second call

- **WHEN** 第二次相同 iova=0x4000_0000 调用 `gmmu.translate()`
- **THEN** L1 TLB hit,跳过 PTW
- **AND** 翻译耗时 < 50% of PTW(性能断言)

#### Scenario: Page Fault on invalid ctx

- **WHEN** 调用 `gmmu.translate(0x4000_0000, 4096, phys)`
- **AND** `contexts_[0].enable == 0`(driver 未启用)
- **THEN** 返回 -EFAULT
- **AND** IO-DMA 记录 fault 不发起 PCIe TLP

---

## Migration Notes (Modified Capabilities)

### sdma-engine-tlm (Modified Capability)

**不修改** requirement 集,仅在 spec 顶部加迁移说明:

> **迁移说明 (2026-09-19)**: `SdmaEngineTLM` 已重命名为 `IoDmaTLM`(per [`2026-09-19-cpptlm-dgpu-gmmu-mvp`](../proposal.md))。原类保留并加 `[[deprecated("use IoDmaTLM")]]` 标注。requirement 集不变,新代码请使用 `IoDmaTLM`。