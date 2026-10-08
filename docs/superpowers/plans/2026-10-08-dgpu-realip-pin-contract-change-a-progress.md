# dGPU Real-IP Pin-Contract Change A — Progress Report (2026-10-08)

## 完成

Change A (Port-Contract 代码化) 全部 8 phase / 12 task 完成, 12 项 AC gates 全绿, 0 回归。

### 已合并 commits (9 个, since Phase C tag `dgpu-realip-refactor-completion`)

| Commit | Phase | 内容 |
|--------|:-----:|------|
| `a7046260` | A.1 | RED failing test stub (`test_realip_pin_contract.cc` TEST_CASE #1+#2+#3 stub) |
| `33671233` | A.1 | `Fabric2VramCtrlBundle` bundle header (8 字段, max width 64-bit) |
| `e9ccadf2` | A.2 | `ch_uint::width` static const + `port_contract_asserts.hh` `BUNDLE_FIELD_WIDTH` macro (Momus 修订: 删 PORT_IS_INITIATOR/TARGET) |
| `35c002bf` | A.3 | `GpuInternalFabric::set_vram_target` setter (forward-decl VramControllerTLM) |
| `cf989fb7` | A.4 | `VramControllerTLM::set_fabric_source` setter (forward-decl GpuInternalFabric, 解决循环 include) |
| `691bdcc5` | A.5 | `DGpuBoard::bind_memory_backings` 末尾 wire programmatic bridge via `soc_->getInternalInstance("fabric")` + `"vram_ctrl"` |
| `c0c80854` | A.5 | Full round-trip test (TEST_CASE #1 + #2 + #3 完整化, 21 assertions / 5 test cases) |
| `27bf44de` | A.6 | `try_compile_legacy_flags_removed.cc` probe (Change C 负向测试 setup) + `test_dgpu_board_no_legacy_flags.cc` XFAIL (2 cases) |
| `6764dca8` | A.7 | Doc sync: `architecture.md` §2.4.1 + `AGENTS.md` §KEY INVARIANTS + OpenSpec resplit (proposal/design/tasks Change A scope) |

### 测试覆盖 (12 AC gates 全绿)

| Gate | 状态 | 数据 |
|------|:----:|------|
| AC-A1 openspec validate --strict | ✅ | "Change 'dgpu-realip-pin-contract' is valid" |
| AC-A2 `[realip][pin-contract]` | ✅ | 5 cases / 21 assertions |
| AC-A3 编译期契约 | ✅ | `BUNDLE_FIELD_WIDTH` static_assert 生效 (TEST_CASE #1 编译期检查 8 字段) |
| AC-A4 23 ABI 零新增 | ✅ | `cpptlm_emulator.h` 零 diff vs Phase C tag |
| AC-A5 `[realip][p3]` | ✅ | 9 cases / 122 assertions / 0 回归 |
| AC-A6 `[fabric]` | ✅ | 7 cases / 13 assertions / 0 回归 |
| AC-A7 `[pcie]` | ✅ | 433 cases / 36682 assertions / 0 回归 |
| AC-A8 `[minimal_dgpu_soc]` | ✅ | 12 cases / 69 assertions / 0 回归 (Oracle C6 红线) |
| AC-A9 `[pcie-memory]` | ✅ | 24 cases / 69 assertions / 0 回归 |
| AC-A10 `[dgpu_soc_timing]` | ✅ | 37 cases / 12024 assertions / 0 回归 |
| AC-A11 4 frozen header 零 diff | ✅ | `cpptlm_emulator.h` / `pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` 全 0 改动 |
| AC-A12 0 build error | ✅ | `cmake --build build` 0 编译错误 |

### 关键决策 (Momus REJECT 修复)

1. **删 PORT_IS_INITIATOR / PORT_IS_TARGET macro** — 原计划基于不存在的 `chstream::ChStreamInitiatorPort<Bundle>` 模板 (实际 `cpptlm::ChStreamInitiatorPort` 是非模板类, 需 `(name, eq)` 构造)。改 `BUNDLE_FIELD_WIDTH` 单一 macro, 仅检查 bundle 字段宽度。
2. **ch_uint 加 `width` 静态常量** — 在 `ch_uint_storage` + `ch_uint_wide` 加 `static constexpr unsigned width = W;` 让 `decltype(&BundleType::Member)::width` 可达。
3. **forward-decl 解决循环 include** — `GpuInternalFabric` 与 `VramControllerTLM` 互指, 双向 forward-decl (`class VramControllerTLM;` + `namespace tlm::cluster { class GpuInternalFabric; }`)。
4. **setter-only 模式替代 ChStream 端口成员** — `set_vram_target` / `set_fabric_source` 双向 setter + private 指针缓存, 运行时 round-trip 测试验证 (替代编译期端口类型断言)。
5. **`soc_->getInternalInstance("fabric")` 而非 `modules_by_name`** — 实际 API 是 `SimModule::getInternalInstance`; 模块名是 `"fabric"` (per `configs/dgpu_soc_v2_realip_p2.json:22`), 不是 `"gpu_internal_fabric"`。
6. **不动 routing flag** — 删 5 routing_enabled + PciePath enum 推迟 Change C (前置 = Change B 完成且 acceptance 0 回归)。

### 文档同步

| 文档 | 改动 |
|------|------|
| `docs/designs/dgpu-soc/architecture.md` §2.4.1 | 新增 port-contract 章节 (8 字段表 + programmatic bridge 说明 + 编译期契约 + Change A scope 边界) |
| `AGENTS.md` §KEY INVARIANTS | 新增 Fabric2VramCtrlBundle invariant (per `cpphdl_types.hh` A.2 修订 + `BUNDLE_FIELD_WIDTH` macro) |
| `AGENTS.md` OpenSpec baseline | 1→2 (新增 `dgpu-realip-pin-contract` Change A) |
| `openspec/changes/dgpu-realip-pin-contract/proposal.md` | Resplit header (Oracle REJECT 修订) |
| `openspec/changes/dgpu-realip-pin-contract/design.md` | Goals/Non-Goals 改 Change A scope (G1-G8 4-IP 端口契约, NG1-NG14 推迟 B/C/D) |
| `openspec/changes/dgpu-realip-pin-contract/tasks.md` | 4-commit 结构 → 8-phase 结构 (A.1-A.8 TDD 5 步) |

`openspec validate dgpu-realip-pin-contract --strict` PASS; `./scripts/test/docs_sync_check.sh --strict` PASS (0 missing paths).

## 下一步: Change B 启动

Change A 完成, Change B (Facade 真正接管数据路径) 现在可以无锁启动:

### Change B 任务 outline (per `docs/superpowers/plans/2026-10-08-dgpu-realip-pin-contract-resplit.md` §Out-of-Scope / Change B)

- **B.1**: RED test — `DGpuBoard::init` 装配后 `controller_ip_ != nullptr` (新增 `[realip][facade-wiring]` Integration test)
- **B.2**: 在 `DGpuBoard::init` 中调 `pcie_ep->set_endpoint_attachment(this)` + `set_pcie_controller_ip(pcie_ctrl)` (复用 `init_timing_mode` cast helper)
- **B.3**: RED test — `match_bar` 对 BAR0 offset 0-16 (GMMU 寄存器窗) 返回 `BarTarget::Bar0Mmio`, 对 offset >= 0x800000 (display FB) 返回 `BarTarget::Bar0Display`
- **B.4**: 扩展 `BarTarget` enum (PcieControllerIP) 加 `Bar0Display` 变体 + 扩展 `match_bar` signature 加 `offset_in_bar` 参数
- **B.5**: 重写 `test/test_pcie_bypass_tlp_data_path.cc` — 把 `board.pcie_path()` 断言改为 `board.controller_ip()` + `controller_ip->match_bar(bar, offset)`
- **B.6**: 全回归 — `[realip][p3]` + `[minimal_dgpu_soc]` + `[pcie-memory]` + `[pcie]` 必须 0 回归 (这是 Change C 启动的硬前置)

### 关键约束 (Oracle C1/C2/C5 修复)

- **C1**: production 路径必须调 `attach_endpoint` + `set_pcie_controller_ip` (Strangler gate 才能真激活)
- **C2**: `match_bar` BAR0 offset-aware 子窗口 (GMMU 寄存器 vs display device 区分) — 否则破坏 ADR-DGPU-10 防劫持 root cause 4
- **C5**: 重写 `test_pcie_bypass_tlp_data_path.cc` (用 `controller_ip()` + `match_bar()` 替代 `pcie_path()`)

### 预估

4-5 天单人, 完成并 acceptance test 0 回归后, Change C 可启动 (Legacy flag 删除)。

---

## Status Update

**状态**: ✅ Change A 完成, 全部 12 AC gates PASS, 0 回归, 4 frozen header 零 diff, 23 ABI 零新增, docs_sync PASS, openspec validate PASS.

**待启动**: Change B (Facade Production Wiring, 4-5 天预估) — 前置已就绪 (Change A 全部 commit 落).
