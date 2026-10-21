# Tasks — cpptlm-msix-generator-ip v0.5.0 (Phase 2 Change E, TDD 5-step)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/msix-generator/spec.md`](specs/msix-generator/spec.md)
> **目标 tag**: `cpptlm-msix-generator-ip-completion`
> **估时**: **5-7d** (TDD 5-step + LWC 集成 + TLP 链路)
> **依赖**: Change A ✅ archived + Change B ✅ archived + Change C ✅ archived (Phase 1 完成)

---

## 总体时间线 (5 阶段, ~5-7d)

| 阶段 | 内容 | 估时 |
|------|------|:----:|
| Phase 1 | TDD RED (5 failing tests) | 0.5d |
| Phase 2 | GREEN: MsixGeneratorIP 模块 + trigger_msix API | 2d |
| Phase 3 | GREEN: LWC 集成 + TLP 链路 | 1.5d |
| Phase 4 | 0 回归锁 + E2E 测试 | 0.5d |
| Phase 5 | 文档同步 + archive | 0.5d |

---

## Phase 1: TDD RED — 写 5 failing tests (0.5d)

### Task 1.1: Write failing tests for trigger_msix API (RED)

**Files**: Create `test/test_msix_generator_ip.cc`

- [ ] T1: `DGpuBoard::trigger_msix` C++ API 不存在 → 编译失败 (CRITICAL C3 验证)
- [ ] T2: `MsixGeneratorIP` 模块不存在 → 编译失败
- [ ] T3: `include/tlm/pcie/msix_generator_ip.hh` 不存在 → 编译失败

**验证**: `cmake --build build` RED
**Commit**: `test(msix): RED 3 cases trigger_msix API missing (CRITICAL C3, Phase 1.1)`

### Task 1.2: Write failing tests for MSI-X table lookup (RED)

- [ ] T4: `trigger_msix(vector=3, ...)` → TLP addr+data 来自 table entry
- [ ] T5: masked vector → 0 TLP 生成, PBA bit set
- [ ] T6: PBA pending bit check → pending 时 TLP 生成

**Commit**: `test(msix): RED 3 cases MSI-X table lookup + PBA (Phase 1.2)`

---

## Phase 2: GREEN — MsixGeneratorIP 模块 + trigger_msix API (2d)

### Task 2.1: MsixTable per-VF/per-PF table 管理 (GREEN)

**Files**: Create `include/tlm/pcie/msix_table.hh` (~100 行)

- [ ] Step 1: `MsixTable` 类: vector_table_ (17 × 64 entries) + pba_ (17 × 64 bits)
- [ ] Step 2: `set_entry(vector, addr_lo, addr_hi, data, masked)` + `get_entry(vector)`
- [ ] Step 3: `set_pending(vector, bool)` + `get_pending(vector)`
- [ ] Step 4: `is_masked(vector)` 检查

**验证**: Task 1.2 全 GREEN (T4-T6)
**Commit**: `feat(msix): MsixTable per-VF/per-PF table + PBA (Phase 2.1)`

### Task 2.2: MsixGeneratorIP 核心模块 (GREEN)

**Files**: Create `include/tlm/pcie/msix_generator_ip.hh` + `src/tlm/pcie/msix_generator_ip.cc` (~400 行)

- [ ] Step 1: `MsixGeneratorIP` 类: `msi_table_` + `trigger_msix(vector, addr, data, masked)` API
- [ ] Step 2: LWC SlavePort `msix_in_` (MsixBundle in, per Change B)
- [ ] Step 3: TLP 生成逻辑: `submit_msix_tlp(vector, addr, data)` → PcieTlpBundle (MWr 64-bit)
- [ ] Step 4: `submit_tlp()` 入口 → PcieEndpointIP (Change D 协同)

**验证**: Task 1.1 + 1.2 全 GREEN
**Commit**: `feat(msix): MsixGeneratorIP core module + trigger_msix API (Phase 2.2)`

### Task 2.3: DGpuBoard trigger_msix 集成 (GREEN)

**Files**: Modify `src/tlm/gpu/dgpu_board_shell.cc` (~30 行)

- [ ] Step 1: 新增 `msix_generator_` 成员 (std::unique_ptr<MsixGeneratorIP>)
- [ ] Step 2: `DGpuBoard::trigger_msix(vector, addr, data, masked)` C++ API
- [ ] Step 3: `DGpuBoard::bind_memory_backings()` 装配 msix_generator_ + LWC hub wiring
- [ ] Step 4: `trigger_irq_async()` 改为调用 `msix_generator_->trigger_msix()`

**验证**: Task 1.1 全 GREEN (T1-T3)
**Commit**: `feat(board): DGpuBoard trigger_msix API + msix_generator wiring (Phase 2.3)`

---

## Phase 3: GREEN — LWC 集成 + TLP 链路 (1.5d)

### Task 3.1: LWC hub wiring (GREEN, per Change B 设施)

**Files**: Modify `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings` (~10 行)

- [ ] Step 1: `lwc_hub_->set_target(PortIndex::MSIX, msix_generator_.get())`
- [ ] Step 2: MsixBundle 已定义 (per Change B T8, `include/bundles/msix_bundles_tlm.hh`)
- [ ] Step 3: 验证 LWC hub route_to_target 正确分发 MsixBundle

**验证**: `[lwc]` 3 cases PASS
**Commit**: `feat(lwc): msix_generator LWC hub wiring (Phase 3.1)`

### Task 3.2: TLP 序列化链路 (GREEN, per Change D v0.5.2)

**Files**: Extend `include/utils/pcie_tlp_serializer.hh` or MsixGeneratorIP 内调用

- [ ] Step 1: `PcieTlpSerializer::encode_msix_tlp(vector, addr, data)` (复用既有 serializer)
- [ ] Step 2: MWr 64-bit encoding per PCIe spec §6.8.2
- [ ] Step 3: `submit_tlp()` → PcieEndpointIP (Change D submit_tlp additive API)

**验证**: TLP roundtrip test PASS
**Commit**: `feat(pcie): MSI-X TLP encode + submit_tlp chain (Phase 3.2)`

### Task 3.3: JSON topology 更新 (GREEN)

**Files**: Modify `configs/dgpu_soc_minimal_v1.json` (~+15 行)

- [ ] Step 1: `modules` 加 `msix_generator` 节点
- [ ] Step 2: `connections` 加 `lwc_hub.MSIX → msix_generator.0`
- [ ] Step 3: `msix_generator.0 → pcie_ep.0` (submit_tlp)

**验证**: SoC 启动 PASS
**Commit**: `feat(json): +msix_generator node + connections (Phase 3.3)`

---

## Phase 4: 0 回归锁 + E2E 测试 (0.5d)

### Task 4.1: [msix_generator] ≥ 5 cases GREEN

**Files**: Extend `test/test_msix_generator_ip.cc` (+~100 行)

- [ ] T7: vector boundary (0, 63)
- [ ] T8: masked vector → 0 TLP + PBA set
- [ ] T9: PBA pending bit persistence
- [ ] T10: unknown vector graceful handling
- [ ] T11: full E2E: device IP → trigger_msix → TLP → host

**Commit**: `test(msix): GREEN 5 cases full coverage (Phase 4.1)`

### Task 4.2: 既有 7 套件 0 回归

- [ ] Step 1: `[pcie]` 430/36672 assertions PASS
- [ ] Step 2: `[minimal_dgpu_soc]` 21/145 assertions PASS
- [ ] Step 3: `[dgpu_soc_timing]` 37/12024 assertions PASS

**Commit**: `test(regress): 7 套件 0 回归 (Phase 4.2)`

---

## Phase 5: 文档同步 + archive (0.5d)

### Task 5.1: 实施笔记 (TDD 5-step + commits 索引)

**Files**: Create `docs/implementation/2026-10-12-msix-generator-ip.md`

- [ ] Step 1: TDD 5-step 执行记录
- [ ] Step 2: 每 task commit hash 索引
- [ ] Step 3: frozen surface 验证 (23 ABI 0 diff + 4 frozen headers 0 diff)

**Commit**: `docs: msix-generator-ip 实施笔记 (Phase 5.1)`

### Task 5.2: archive

- [ ] Step 1: `openspec archive cpptlm-msix-generator-ip`
- [ ] Step 2: 验证 KPI: 3 → 2 (释放 1 slot)
- [ ] Step 3: post-flight 检查

**Commit**: `docs(openspec): archive Change E (cpptlm-msix-generator-ip-completion)`

---

## TDD 5-Step 执行顺序

```
Phase 1: TDD RED
  T1-T3: trigger_msix API 不存在 → 编译失败
  T4-T6: MSI-X table lookup 不工作 → 3 failing tests

Phase 2: GREEN
  Task 2.1: MsixTable → T4-T6 GREEN
  Task 2.2: MsixGeneratorIP → T1-T3 GREEN
  Task 2.3: DGpuBoard 集成 → T1-T3 GREEN

Phase 3: GREEN
  Task 3.1: LWC wiring → T7 GREEN
  Task 3.2: TLP chain → T8 GREEN
  Task 3.3: JSON → T9 GREEN

Phase 4: GREEN
  Task 4.1: E2E tests → T10-T11 GREEN
  Task 4.2: 0 回归 → 7 套件 PASS
```

---

## 关键依赖 (前置条件)

| 前置 | 来源 | 说明 |
|------|------|------|
| Change B LWC Hub | `cpptlm-lwc-bus-hub-mvp` archived | MsixBundle 已定义, LWC hub 已落地 |
| Change B MsixBundle | `include/bundles/msix_bundles_tlm.hh` | 4 fields (vector + addr_lo/hi + data + masked) |
| Change D submit_tlp | `cpptlm-pcie-tlp-entry-layer` archived | 准 frozen additive API |
| PcieTlpSerializer | `include/utils/pcie_tlp_serializer.hh` | MSI-X TLP encode |

---

## Frozen Surface 验证

- [ ] 23 ABI 签名 0 diff (per ADR-088 §D5)
- [ ] 4 frozen headers 0 diff
- [ ] `include/abi/cpptlm_emulator.h` 无 `trigger_msix` 符号
