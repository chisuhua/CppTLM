# cpptlm-minimal-dgpu-soc-v1-test-coverage: 设计文档

> **配套**: [`proposal.md`](proposal.md) · [`specs/minimal-dgpu-soc-test-coverage/spec.md`](specs/minimal-dgpu-soc-test-coverage/spec.md) · [`tasks.md`](tasks.md)
> **配套 change SSOT**: [`../archive/2026-09-24-cpptlm-minimal-dgpu-soc-v1/`](../archive/2026-09-24-cpptlm-minimal-dgpu-soc-v1/) (v1.0 主实施, 已归档)
> **设计 SSOT**: [`docs/designs/2027-02-09-minimal-dgpu-soc.md`](../../../docs/designs/2027-02-09-minimal-dgpu-soc.md) (v1.0)
> **本文件定位**: 记录 v1.0 主 change 推迟的 7 项测试任务的实施期设计决策

---

## §1 范围概览

本 change 仅补全 v1.0 主 change 推迟的测试任务,**不引入新模块或修改实现代码**。所有改动在 `test/` 目录内,加少量边界/race 用例。

## §2 代码组织

```
test/test_minimal_dgpu_soc_characterization.cc   # 扩展: C1.1 + C1.2 边界
test/test_dgpu_board_framebuffer.cc             # 扩展: C1.2 framebuffer_size_=0
test/test_gmmu_tlm.cc                           # 扩展: C1.3 race 锁定
test/test_minimal_dgpu_soc_e2e.cc               # 新建: C2.1 + C2.3 E2E (Oracle 复审: 实际不依赖 T-bs-4 修复, T-bs-4 已于 8cd1f033 完成)
```

## §3 关键设计决策

| # | 决策点 | 选择 | 理由 / 依据 |
|---|--------|------|------------|
| **D1** | C2 E2E 与 T-bs-4 关系 | C2 E2E 失败走 cpptlm-debug SKILL 独立定位, **不再预设是 T-bs-4 问题** | Oracle 2027-02-10 复审更正: T-bs-4 已于 commit `8cd1f033` (D15 true fix - null-guard unregistered module types in instantiateAll) 修复; `dgpu_board_shell.cc:63` 注释陈腐 (17413e4 时代残留), 实际第 77 行 `soc_->simulate_instantiate(soc_cfg)` 调用正常, 996 cases 全绿。C2.1 E2E 失败根因优先级: (1) test harness framebuffer 挂载缺漏 (`load_soc_config` 不消费 JSON 顶层 `framebuffer_size_bytes`), (2) SDMA/GMMU/PCIeEP 数据通路 bug |
| **D2** | C1.2 framebuffer_size_=0 测试方式 | 测试场景而非生产路径 | Inv-2 + D10 派生逻辑已排除 size_=0 生产路径; 测试仅锁定"若 size=0 全访问 OUT_OF_RANGE"行为, 作为防御性 |
| **D3** | C1.3 PT_BASE LO/HI race 语义 | 锁定为 "LO 已写 HI 未写时 translate 用中间态 (LO only)", 与 spec Inv-4 "v1.0 接受 race" 一致 | spec 已 verbatim 写 "v1.0 接受此 race (简单实现)"; 测试不是负收益 (写锁定 race 行为有助于未来 v2.1 加锁时正确性回归) |
| **D4** | C2.1 PTE 编码 | `(paddr & ~0xFFF) | 1` 显式 (per design A2 修订) | design §5.3: PTE bit12-63 直接存 paddr, `<<12` 偏移错位 |
| **D5** | C2.1 描述符提交 | `ring_write_entry` + doorbell `mmio_write`, **不调用** 不存在的 `submit_descriptor` | per design A4 修订 + SdmaEngineTLM 既有 API (NG4 不改 SDMA) |
| **D6** | C2.1 host backdoor 大小 | 16KB (≥ phys + size) | design A3 修订: 4KB host_backdoor 触发越界 memcpy 静默跳过, 改 16KB |
| **D7** | C2.3 MSI-X coalescing | `msix_coalesce_enabled_ = false` 显式 disable | 排除合并窗口干扰, 验证 fence → vector 0 直接投递 |

## §4 测试场景映射

| tasks.md 任务 | 本 change 测试场景 | 依赖 |
|---|---|---|
| C1.1 标签汇总 | `test_minimal_dgpu_soc_characterization.cc` 加 SECTION 跑 4 标签组合 | — |
| C1.2 边界 framebuffer_size_=0 | `test_dgpu_board_framebuffer.cc` 加 SECTION: `attach_framebuffer_for_testing(0, 0)` 后全访问 OUT_OF_RANGE | — |
| C1.2 边界非 4/8 字节 len | `test_dgpu_board_framebuffer.cc` 加 SECTION: BAR1 写 1/2/3/16 字节 readback 一致 | — |
| C1.3 race 锁定 | `test_gmmu_tlm.cc` 加 SECTION: set_pt_base_lo 后立即 translate (HI 仍 0) → -EIO | — |
| C2.1 E2E H2D + 双读回 | `test_minimal_dgpu_soc_e2e.cc` Scenario "全链路 H2D + 双读回一致" | — (Oracle 2027-02-10: 实际无架构依赖) |
| C2.2 修复链路 | (跟随 C2.1 FAIL → 按 cpptlm-debug SKILL 6 步定位) | 同 C2.1 |
| C2.3 E2E fence MSI-X | `test_minimal_dgpu_soc_e2e.cc` Scenario "fence → MSI-X vector 0" | 同 C2.1 + MSI-X coalescing disable |
| C2.4 诊断清零 | `grep -l "static FILE\* diag" include/ src/ test/ -r` 为空 (v1.0 已零) | — |

## §5 Alternatives Considered

| 备选 | 否决理由 |
|------|---------|
| **Option 1 (合并到 v1.0 主 change)** | 阻塞 v1.0 归档; 与零债务原则冲突 |
| **Option 3 (归档 v1.0 + 接受 AC8 推迟)** | 用户选择 Option 2 优先于 oracle 推荐, 项目分轨更清晰 (避免 C1.x housekeeping 散落) |
| **直接实施 T-bs-4 修复** | 超出本 change 范围 (Oracle 2027-02-10: T-bs-4 已于 8cd1f033 完成, 无需再修) |

## §6 Tradeoffs

- **测试代码单独立 change vs 合并**: 本 change 仅测试代码 (无实现改动), 立 change 便于测试策略独立评审与归档
- **C2 失败根因优先级** (Oracle 2027-02-10 评估): (1) test harness `attach_framebuffer_for_testing` 显式调用缺漏, 概率高; (2) SDMA ring_write_entry + GMMU translate 组合路径 bug; (3) PcieEndpointIP BAR0 寄存器→GmmuTLM 转发路径 bug
- **C1.3 race 锁定可能与 v2.1 写锁冲突**: 测试只锁定当前 v1.0 行为, v2.1 写锁后该测试需重写 (这是符合 spec 的预期)

## §7 Technical Risks

| # | 风险 | 等级 | 缓解 |
|---|------|------|------|
| R1 | C2.1 E2E FAIL 暴露 test harness / 子模块数据通路 bug | 🟡 中 | 走 cpptlm-debug SKILL 6 步定位; 首要排查 `framebuffer_size_=0` (load_soc_config 不消费 JSON 顶层 `framebuffer_size_bytes`), 次要排查 SDMA/GMMU/PCIeEP 数据通路 |
| R2 | C1.2 边界用例发现 framebuffer_/vram_segments_ fallback 切换逻辑 bug | 🟢 低 | 现有 A2 测试已防回归; 失败即登记新 spec issue |
| R3 | C2.3 MSI-X coalescing disable 未生效导致 fence 永远合并 | 🟢 低 | `set_msix_coalesce_enabled(false)` 显式 + `drain` 后检查 |

---

**Owner**: CppTLM Team · **版本**: v1.0.1 (Oracle 复审修订) · **日期**: 2027-02-10