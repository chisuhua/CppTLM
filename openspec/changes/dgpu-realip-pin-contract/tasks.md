# dgpu-realip-pin-contract: Tasks (Change A — 8 phase TDD)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/dgpu-4ip-port-contract/spec.md`](specs/dgpu-4ip-port-contract/spec.md)
> **起点**: `dgpu-realip-refactor-completion` tag (Phase C, 2026-10-07)
> **目标 tag**: `dgpu-realip-pin-contract-change-a-completion`
> **关键修订**: 起点 Phase C 后 4-IP topology 锁定。Change A 范围:端口契约代码化 + programmatic bridge,**不动 routing flag,不动 mmio_read/write,不动 match_bar**。Oracle REJECT 修复后 (Momus 修订版) 通过, 4-Change 拆分。
>
> **后续 change (独立 plan, 不在本文件范围)**:
> - Change B: Facade 真正接管数据路径 (`DGpuBoard::init` 装配 `attach_endpoint` + `set_pcie_controller_ip`, `match_bar` BAR0 offset-aware 子窗口修复, 重写 `test_pcie_bypass_tlp_data_path.cc`)
> - Change C: Legacy flag 删除 (5 routing_enabled + PciePath enum + mmio_read/write 简化 + JSON 退役, **门控于 Change B 完成**)
> - Change D: P2 清理 (ModuleFactory strict_types + ConnectionResolver WARN + ParamResolver hook + AGENTS.md baseline)

---

## 文件清单 (Change A scope, 2026-10-08)

| 文件 | 变化 | Phase |
|------|------|:-----:|
| `include/bundles/dgpu_fabric_bundles_tlm.hh` | **新** (~60 行, 8 字段) | A.1 |
| `include/bundles/cpphdl_types.hh` | 改 (`static constexpr unsigned width = W;` 加到 ch_uint_storage + ch_uint_wide) | A.2 |
| `include/utils/port_contract_asserts.hh` | **新** (~40 行, BUNDLE_FIELD_WIDTH macro) | A.2 |
| `include/chstream_register.hh` | 改 (compile-time include bundle) | A.1 |
| `include/tlm/cluster/gpu_internal_fabric.hh` | 改 (set_vram_target + vram_ctrl() accessor + vram_ctrl_ 成员) | A.3 |
| `include/tlm/vram_controller_tlm.hh` | 改 (set_fabric_source + fabric() accessor + fabric_ 成员) | A.4 |
| `src/tlm/gpu/dgpu_board_shell.cc` | 改 (bind_memory_backings 末尾追加 programmatic bridge + 加 include) | A.5 |
| `test/test_realip_pin_contract.cc` | **新** (3 cases 双标签 `[realip][pin-contract]`) | A.1+A.3+A.5 |
| `test/test_dgpu_board_no_legacy_flags.cc` | **新** (2 cases, Change A XFAIL 标记) | A.6 |
| `scripts/test/try_compile_legacy_flags_removed.cc` | **新** (Change C 负向测试探针, Change A 仅 setup) | A.6 |
| `docs/designs/dgpu-soc/architecture.md` §2.4.1 | 改 (新增 port-contract 章节) | A.7 |
| `AGENTS.md` §KEY INVARIANTS | 改 (新增 Fabric2VramCtrlBundle invariant + OpenSpec baseline 1→2) | A.7 |

**总文件数**: 12 (含 4 个新建 + 4 个改 + 4 个 docs)。**净 ~0 LOC 变化** (新 ~120 行 + 文档 ~30 行 + 测试 ~120 行)。

**Frozen surface (0 修改, Change A scope)**:
- `include/abi/cpptlm_emulator.h` — 23 ABI 签名不动
- `include/tlm/gpu/pcie_endpoint_tlm.h` — 不动
- `include/tlm/gpu/pcie_display_device.hh` — 不动
- `include/tlm/pcie/pcie_bundles_tlm.hh` — 不动 (`Fabric2VramCtrlBundle` 走新文件)
- `PcieControllerIP` / `PcieEndpointIP` / `VramControllerTLM` / `GpuInternalFabric` **业务行为** — 不动 match_bar / mmio_write / bar_store / crossbar
- `DGpuBoard` 7 legacy flag + mmio_read/write — **本 Change A 不删** (留给 Change C, 前置 = Change B 完成)

---

## Phase A.1: `Fabric2VramCtrlBundle` 字段定义 (TDD 5 步)

### Task 1: Write failing test (RED)

**Files:**
- Create: `test/test_realip_pin_contract.cc`

- [ ] Step 1: 写失败测试 stub (TEST_CASE #1 8 字段宽度 + #2/#3 pending stub)

**验证**: `cmake --build build` → 编译失败 (`Fabric2VramCtrlBundle` 未定义)
**Commit**: `test(pin-contract): add Fabric2VramCtrlBundle field-width RED stub (A.1)`

### Task 2: Create bundle header (GREEN)

**Files:**
- Create: `include/bundles/dgpu_fabric_bundles_tlm.hh` (60 行, 8 字段)
- Modify: `include/chstream_register.hh` (compile-time include)

**验证**: `cmake --build build` 退出码 0
**Commit**: `feat(pin-contract): add Fabric2VramCtrlBundle chip-internal wire-protocol (A.1 GREEN)`

---

## Phase A.2: `port_contract_asserts.hh` + `cpphdl_types.hh` width 常量

### Task 2b: Add `width` static member to `ch_uint`

**Files:**
- Modify: `include/bundles/cpphdl_types.hh`

- [ ] Step 1: 在 `ch_uint_storage<W>` 和 `ch_uint_wide<W>` 内加 `static constexpr unsigned width = W;`

**验证**: 编译无 regression, `[realip][pin-contract]` TEST_CASE #1 编译失败 (因 Step 3 macro 还未就绪)
**Commit**: `feat(ch_uint): add static width member for compile-time width checks (A.2)`

### Task 3: Write port_contract_asserts macros (Momus 修订版)

**Files:**
- Create: `include/utils/port_contract_asserts.hh` (~40 行, 只含 BUNDLE_FIELD_WIDTH macro; **删 PORT_IS_INITIATOR/PORT_IS_TARGET** 因 `chstream::ChStreamInitiatorPort<Bundle>` 模板类型不存在)

**验证**: 编译退出码 0, TEST_CASE #1 现在 PASS (8 字段宽度)
**Commit**: `feat(pin-contract): add port_contract_asserts BUNDLE_FIELD_WIDTH macro (A.2)`

---

## Phase A.3: `GpuInternalFabric::set_vram_target` setter-only (Momus 修订)

### Task 4: Test stub

**Files:**
- Modify: `test/test_realip_pin_contract.cc` (TEST_CASE #2 完整化)

### Task 5: Add setter to GpuInternalFabric header

**Files:**
- Modify: `include/tlm/cluster/gpu_internal_fabric.hh`
  - 加 `class VramControllerTLM;` forward-decl (在 tlm::cluster 命名空间外, **因为 VramControllerTLM 是全局命名空间**)
  - 加 public `set_vram_target(VramControllerTLM*)` + `vram_ctrl()` accessor
  - 加 private `VramControllerTLM* vram_ctrl_ = nullptr;`

**验证**: 编译退出码 0, TEST_CASE #2 PASS
**Commit**: `feat(pin-contract): GpuInternalFabric set_vram_target setter (A.3 GREEN)`

---

## Phase A.4: `VramControllerTLM::set_fabric_source` setter-only (Momus 修订)

### Task 6: Add setter to VramControllerTLM header

**Files:**
- Modify: `include/tlm/vram_controller_tlm.hh`
  - 加 `namespace tlm::cluster { class GpuInternalFabric; }` forward-decl
  - 加 public `set_fabric_source(tlm::cluster::GpuInternalFabric*)` + `fabric()` accessor
  - 加 private `tlm::cluster::GpuInternalFabric* fabric_ = nullptr;`

**注意**: **必须 forward-decl, 不能 #include** `tlm/cluster/gpu_internal_fabric.hh` — 会触发循环 include (gpu_internal_fabric.hh 也 forward-decl VramControllerTLM)

**验证**: 编译退出码 0
**Commit**: `feat(pin-contract): VramControllerTLM set_fabric_source setter + forward-decl circular (A.4 GREEN)`

---

## Phase A.5: Programmatic bridge via `set_vram_target` in DGpuBoard

### Task 7: Test stub (RED)

**Files:**
- Modify: `test/test_realip_pin_contract.cc` (TEST_CASE #3 完整化)

### Task 8: Wire in DGpuBoard::bind_memory_backings (GREEN, Momus 修订)

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc`
  - 加 `#include "tlm/vram_controller_tlm.hh"` + `#include "tlm/cluster/gpu_internal_fabric.hh"` (修复 dynamic_cast incomplete-type 错误)
  - 在 `bind_memory_backings()` vram_ctrl 块之后追加:
    ```cpp
    if (auto* fabric = dynamic_cast<tlm::cluster::GpuInternalFabric*>(
            soc_->getInternalInstance("fabric"))) {
        if (vram_ctrl_seen != nullptr) {
            fabric->set_vram_target(vram_ctrl_seen);
            vram_ctrl_seen->set_fabric_source(fabric);
        }
    }
    ```
  - **模块名 `"fabric"` (per `configs/dgpu_soc_v2_realip_p2.json:22`)**, **不是 `"gpu_internal_fabric"`**
  - **API `soc_->getInternalInstance(name)`** (实际 API), **不是 `modules_by_name(name)`**
  - **VramControllerTLM 是全局命名空间** (非 `tlm::vram::`)

**验证**: 编译退出码 0, TEST_CASE #3 PASS (双向桥接), `[realip][p3]` + `[fabric]` + `[pcie]` 0 回归
**Commit**: `feat(pin-contract): DGpuBoard programmatic bridge fabric↔vram_ctrl (A.5 GREEN)`

---

## Phase A.6: try_compile harness + public API guard (Change C forward-looking setup)

### Task 9: Create try_compile probe source (Change C XFAIL 探针)

**Files:**
- Create: `scripts/test/try_compile_legacy_flags_removed.cc` (调用 5 个 legacy setter; 在 Change A 应编译通过, Change C 启动后改探针后应 EXPECT-FAIL)

**验证**: 文件存在, 暂不接 try_compile CMake harness (过度工程化, 推迟)
**Commit**: `feat(pin-contract): try_compile probe + public API guard XFAIL (A.6 setup)`

### Task 10: Public API guard test (XFAIL in Change A)

**Files:**
- Create: `test/test_dgpu_board_no_legacy_flags.cc`
  - TEST_CASE #1: public API 标记 (SUCCEED 验证 instantiation)
  - TEST_CASE #2: 5 legacy setter 当前应存在 (在 Change A scope)

**注意**: 不可用 `board.name() == "..."` (DGpuBoard::name_ 是 private 字段, 无 public accessor)
**验证**: 2 cases PASS (Change A scope)
**Commit**: 同 Task 9 (合并提交)

---

## Phase A.7: 文档同步 (architecture.md + AGENTS.md + openspec)

### Task 11: 文档同步

**Files:**
- Modify: `docs/designs/dgpu-soc/architecture.md` §2.4 (新增 2.4.1 章节)
- Modify: `AGENTS.md` §KEY INVARIANTS (新增 Fabric2VramCtrlBundle 条目)
- Modify: `AGENTS.md` (OpenSpec baseline 1→2)
- Modify: `openspec/changes/dgpu-realip-pin-contract/proposal.md` (resplit header)
- Modify: `openspec/changes/dgpu-realip-pin-contract/design.md` (Goals/Non-Goals 改 Change A scope)
- Modify: `openspec/changes/dgpu-realip-pin-contract/tasks.md` (本文件, 4-commit → 8-phase)

**验证**:
- `openspec validate dgpu-realip-pin-contract --strict` PASS
- `./scripts/test/docs_sync_check.sh --strict` PASS

**Commit**: `docs(pin-contract): resplit to Change A scope per Oracle review (A.7)`

---

## Phase A.8: 验证 gates (最终回归 + Acceptance)

### Task 12: 12 项 AC gates

- [ ] **AC-A1**: `openspec validate dgpu-realip-pin-contract --strict` PASS
- [ ] **AC-A2**: `[realip][pin-contract]` ≥3 cases PASS (3 cases / 21 assertions 全绿)
- [ ] **AC-A3**: 编译期契约生效 (BUNDLE_FIELD_WIDTH static_assert 触发, 无 error)
- [ ] **AC-A4**: 0 个新 ABI 函数 (frozen per ADR-DGPU-08)
- [ ] **AC-A5**: `[realip][p3]` ≥9 cases 0 回归
- [ ] **AC-A6**: `[fabric]` 7 cases 0 回归 (T22b attach_crossbar 锁定)
- [ ] **AC-A7**: `[pcie]` ≥433 cases 0 回归 (Phase C 后基线)
- [ ] **AC-A8**: `[minimal_dgpu_soc]` ≥12 cases 0 回归 (Oracle C6 红线 — Change A 不动 routing flag)
- [ ] **AC-A9**: `[pcie-memory]` ≥24 cases 0 回归
- [ ] **AC-A10**: `[dgpu_soc_timing]` ≥37 cases 0 回归
- [ ] **AC-A11**: 4 frozen header 零 diff (per AGENTS.md 冻结面, Change A 不动)
- [ ] **AC-A12**: 0 公共文件破坏 (build 0 error)

**Commit**: (无, 已在 Phase A.7 包含; 此 phase 仅是 verification)

---

## 实施时间线 (修订, Change A 8 phase)

| Commit | 内容 | 估时 |
|:------:|------|:----:|
| Task 1 | RED failing test stub | 0.5h |
| Task 2 | `Fabric2VramCtrlBundle` bundle header | 0.5h |
| Task 2b | `ch_uint::width` static const | 0.5h |
| Task 3 | `port_contract_asserts.hh` BUNDLE_FIELD_WIDTH macro | 0.5h |
| Task 5 | `GpuInternalFabric::set_vram_target` setter | 0.5h |
| Task 6 | `VramControllerTLM::set_fabric_source` setter + forward-decl | 0.5h |
| Task 8 | `DGpuBoard::bind_memory_backings` 末尾 wire programmatic bridge | 1.5h |
| Task 9+10 | try_compile probe + public API guard test | 0.5h |
| Task 11 | 文档同步 (architecture.md + AGENTS.md + openspec) | 1h |
| Task 12 | 12 AC gates verification | 1h |
| **总计** | — | **~7h (1 天单人)** |

---

**Owner**: CppTLM Team (Sisyphus)
**起点**: `dgpu-realip-refactor-completion` tag (`26583da7`)
**目标 tag (Change A)**: `dgpu-realip-pin-contract-change-a-completion` (落 AC-A1 ~ AC-A12)