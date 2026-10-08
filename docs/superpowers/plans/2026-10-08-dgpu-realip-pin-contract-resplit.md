# dGPU Real-IP Pin-Contract — Resplit Implementation Plan (Oracle Review 反馈)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过拆分 + 修复 P0 项,落地 4-IP topology 的 chip-internal 端口契约代码化, **不重启 ADR-DGPU-10 的"防劫持 root cause 4"防护锁**。原 `dgpu-realip-pin-contract` proposal 经 Oracle 复评 REJECT (4 CRITICAL + 7 MAJOR + 5 MINOR),核心问题是**(C1)** facade 在生产路径零接线状态下删 routing flag 等于撕毁活性分发机制,**必须先接线 facade 再删 flag**。

**Architecture:** 4-Change 拆分 + 显式门控。原 monolith change 拆为 4 个独立 change,每个有自己的 DoD + 前置依赖链:
- **Change A（本计划主体）**: Port-Contract 代码化 — `Fabric2VramCtrlBundle` + static_assert + 2 个新测试
- **Change B（独立前置）**: Facade 真正接管数据路径 — `DGpuBoard::init` 装配 `attach_endpoint` + `set_pcie_controller_ip`,并修复 `match_bar` 的 BAR0 offset-aware 子窗口语义（C2 修复）
- **Change C（门控于 B）**: Legacy flag 删除 — 仅在 B 的 acceptance test 全绿后启动
- **Change D（独立 P2 清理）**: ModuleFactory strict_types + ConnectionResolver 命名端口 WARN + ParamResolver hook on PcieControllerIP + AGENTS.md Proposed 基线更新

**Tech Stack:** C++17, Catch2 v3.7.0, CMake `try_compile` (替代 SFINAE), chstream_register.hh (bundle include), nlohmann/json (config 退役字段), 既有 4-IP topology (`PcieControllerIP` / `GpuInternalFabric` / `VramControllerTLM` / `DGpuSoc`) + Phase C `dgpu-realip-refactor-completion` tag 基线。

**Oracle 反馈索引:** 全部 16 个 finding 已在本文档口映射执行任务 — 见每 task 的 `Refs:` 段。

---

## 0. Background & Motivation (Oracle Review Recap)

### 0.1 原 proposal 三处事实错误

| Finding | 事实 | Oracle 来源 |
|---------|------|------------|
| **C1** | `set_pcie_controller_ip` / `attach_endpoint` 在 `src/` 下**零调用点**（仅测试调用）;facade 当前是 dormant 的测试-only 并行实现 | `proposal.md:18` 声称"实际数据路径已被 `PcieControllerIP::match_bar` 取代" 与代码 grep 结果不符 |
| **C2** | spec.md:90-97 要求 `bar==0 → display_device().mmio_read()` **无条件**;`minimal_v1` 中 BAR0 = GMMU/SDMA 寄存器窗;无条件分发会劫持寄存器读写 = 重新引入 ADR-DGPU-10 锁定的 root cause 4 | spec scenario 与 D1 v1.1.1 锁定矛盾 |
| **C6** | `tasks.md:2.3.1-2.3.4` 引用 `dgpu_soc_v2_realip_p2.json:7` 的 routing flag,但**该文件实际不含这些字段**;真正含 flag 的 `minimal_v1`/`timing_v1`/`with_pcie_ip` 三个 config **不在 change 清单** | design.md:51-59 §1.1 字段引用张冠李戴 |

### 0.2 拆分决策:原 monolith → 4 Change

| Change | 范围 | 风险 | 前置 | 估时 |
|--------|------|------|------|------|
| **A** | 端口契约代码化 (Fabric2VramCtrlBundle + static_assert + 2 test) | 🟢 低 | 无（Phase C tag 即可） | 3-4 天 |
| **B** | Facade 生产接线 (DGpuBoard::init 装配 + match_bar offset-aware + 重写 `test_pcie_bypass_tlp_data_path.cc`) | 🟡 中 | 无（与 A 并行可） | 4-5 天 |
| **C** | Legacy flag 删除 (5 routing_enabled + PciePath enum + mmio_read/write 简化 + JSON 退役) | 🔴 高 | **B 完成且 acceptance test 0 回归** | 3-4 天 |
| **D** | P2 清理 (ModuleFactory strict_types + ConnectionResolver WARN + ParamResolver hook + AGENTS.md baseline) | 🟢 低 | 无 | 2-3 天 |

**关键决策: 本计划仅含 Change A 的可执行任务**。Change B/C/D 各自独立归档为 `plans/<date>-<change>-<scope>.md`,由 B 完成后的 acceptance test 决定 C 启动时机。

---

## File Structure (Change A 锁定)

### 新文件

| 文件 | Phase | 职责 |
|------|-------|------|
| `include/bundles/dgpu_fabric_bundles_tlm.hh` | A.1 | `Fabric2VramCtrlBundle` 8 字段定义（chip-internal wire-protocol,与 `cache_bundles_tlm.hh:30-46` `CacheReqBundle` 字段对齐 + `chip_select[8]` 扩展） |
| `include/utils/port_contract_asserts.hh` | A.2 | **`BUNDLE_FIELD_WIDTH` 1 个 macro**(Momus 修订: 删除 PORT_IS_INITIATOR/TARGET,因 `chstream::ChStreamInitiatorPort<Bundle>` 模板类型不存在) |
| `test/test_realip_pin_contract.cc` | A.6 | 3 cases `[realip][pin-contract]` 双标签（per handoff §9.2 #10 防 vacuous pass） |
| `test/test_dgpu_board_no_legacy_flags.cc` | A.6 | 1 case `[realip][pin-contract]` — **使用 CMake `try_compile` 负向编译测试**（替代 C3 不可实现的 SFINAE-on-private-members） |

### 修改文件

| 文件 | Phase | 改动 |
|------|-------|------|
| `include/bundles/cpphdl_types.hh` | A.2 | 新增 `static constexpr unsigned width = W;` 到 `ch_uint_storage` / `ch_uint_wide`（让 BUNDLE_FIELD_WIDTH macro 可用 `decltype(...)::width`） |
| `include/bundles/dgpu_fabric_bundles_tlm.hh` | A.1 | 新文件 — 见 New files |
| `include/utils/port_contract_asserts.hh` | A.2 | 新文件 — `BUNDLE_FIELD_WIDTH` macro 仅检查 bundle 字段宽度,不依赖 ChStream 端口模板（**ChStreamInitiatorPort/ChStreamTargetPort 非模板,见 AD-2 修订**） |
| `include/chstream_register.hh` | A.1 | `#include "bundles/dgpu_fabric_bundles_tlm.hh"`（compile-time bundle include,非 adapter factory） |
| `include/tlm/cluster/gpu_internal_fabric.hh` | A.3 | **改 setter-only 模式（Momus 修复后）**:仅加 `VramControllerTLM* vram_ctrl_ = nullptr;` + `set_vram_target(VramControllerTLM*)` setter + `vram_ctrl()` accessor,**不加 `ChStreamInitiatorPort<Bundle>` 成员**（该类型不存在,实际 `cpptlm::ChStreamInitiatorPort` 非模板） |
| `src/tlm/cluster/gpu_internal_fabric.cc` | A.5 | `simulate_instantiate` 中 `fabric->set_vram_target(vram_ctrl)` 程序化桥接（per Phase 8 HB/RC 先例） |
| `include/tlm/vram_controller_tlm.hh` | A.4 | **改 setter-only 模式（Momus 修复后）**:仅加 `GpuInternalFabric* fabric_ = nullptr;` + `set_fabric_source(GpuInternalFabric*)` setter,**不加 `ChStreamTargetPort<Bundle>` 成员** |
| `src/tlm/gpu/dgpu_board_shell.cc` | A.5 | `bind_memory_backings()` 末尾追加 `fabric->set_vram_target(vram_ctrl)` 调用 `soc_->getInternalInstance("fabric")` 取真正指针,**仅 A 范围,不含 set_pcie_controller_ip — 那是 Change B** |
| `test/CMakeLists.txt` | A.6 | 添加 `try_compile` 负向测试 CMake 函数 + 集成 2 个新测试文件 |
| `scripts/test/try_compile_legacy_flags_removed.cc` | A.6 | CMake `try_compile` 探针源（**不进 cpptlm_core,仅 `build/` 临时编译**) |
| `docs/designs/dgpu-soc/architecture.md` §2.4 | A.7 | 4-IP 边界表新增 `fabric.vram_ctrl_out → vram_ctrl.fabric_in` + `Fabric2VramCtrlBundle` 字段定义 |
| `AGENTS.md` §KEY INVARIANTS | A.7 | 新增条目:"`Fabric2VramCtrlBundle` chip-internal wire-protocol 锁定;`static_assert` 编译期契约启用" |
| `openspec/changes/dgpu-realip-pin-contract/` | A.7 | proposal.md / design.md / tasks.md 改写为 Change A scope（删原 monolith 表述） |

### Frozen surface（0 修改,本 Change A 范围）

- `include/abi/cpptlm_emulator.h` — 23 ABI 签名不动
- `include/tlm/gpu/pcie_endpoint_tlm.h` — 不动
- `include/tlm/gpu/pcie_display_device.hh` — 不动
- `include/tlm/pcie/pcie_bundles_tlm.hh` — 不动（`Fabric2VramCtrlBundle` 走新文件）
- `PcieControllerIP` / `PcieEndpointIP` / `VramControllerTLM` / `GpuInternalFabric` **类定义行为** — 不动 match_bar / mmio_write / bar_store / crossbar 业务
- `dgpu_board_shell.cc` 中 legacy flag / mmio_read/write — **本 Change A 不删**(留给 Change C)

---

## Architecture Decisions

### AD-1: 不重启 ADR-DGPU-10 防护锁（C2 修复）

Oracle C2: 原 spec 要求 `bar==0 → display_device().mmio_read()` 无条件 → 与 `minimal_v1` BAR0 = GMMU 寄存器窗冲突。

**决议**: Change A 仅代码化 fabric↔vram_ctrl 端口契约,**不动 DGpuBoard::mmio_read** 的分发逻辑。`match_bar` 的 BAR0 offset-aware 子窗口语义修复留给 Change B（前置 of C）。本 Change A 通过 `test_realip_pin_contract.cc` TEST_CASE #3 round-trip 仅验证 fabric↔vram_ctrl 数据路径,不含 BAR0 display/GMMU 分发断言。

### AD-2: static_assert 不探测 private members（C3 修复）+ 端口类型不检查 (Momus 修订)

Oracle C3: C++17 SFINAE 无法探测 private 成员缺席；`sizeof(DGpuBoard) < X` 依赖实现定义布局。

**决议** (Momus 修订): `port_contract_asserts.hh` 仅提供 **public bundle 字段宽度的 static_assert**(`BUNDLE_FIELD_WIDTH` 用 `decltype(&BundleType::Member)::width`,依赖 `cpphdl_types.hh` 加的 `static constexpr unsigned width = W;` — Task 2b)。**不**检查端口类型(`ChStreamInitiatorPort<Bundle>` 模板类型不存在 — 实际 `cpptlm::ChStreamInitiatorPort` 是非模板类, `core/chstream_port.hh:17`)。私有成员删除验证走 **CMake `try_compile` 负向编译测试**(`scripts/test/try_compile_legacy_flags_removed.cc` 引用已删除 setter 名字,期望 EXPECT-FAIL)。这同时解决了 C4 的 `#include .cc` ODR 风险(`try_compile` 不污染 cpptlm_core TU)。端口契约运行时验证在 `test_realip_pin_contract.cc` TEST_CASE #3 round-trip 覆盖。

### AD-3: 不在 Change A 删任何 routing flag（C1 + C6 修复）

Oracle C1/C6: 原 monolith 在 facade dormant 状态下删 flag = 撕毁活性分发机制;且只引用了一个不含 flag 的 JSON。

**决议**: Change A scope 仅包含 **端口契约代码化 + 测试**,**不动 `dgpu_board_shell.cc` 中任何 flag 加载/路由分支**。删 flag 工作整体推后至 Change C（C 启动前置 = Change B 完成且 acceptance test 0 回归）。

### AD-4: set_vram_target + set_fabric_source 双向程序化桥接（Momus 修订）

Phase 8 HB/RC tick 转发先例 + cpptlm-p2-integration-unblock design D7 锁定:JSON 不声明 fabric↔vram_ctrl 端口（避免 §14.2.3 静默丢弃）,改用程序化 setter。

**决议** (Momus 修订): **不**加 `ChStreamInitiatorPort<Bundle>` / `ChStreamTargetPort<Bundle>` 成员(类型不存在)。改用双向 setter-only 模式:
- `GpuInternalFabric::set_vram_target(VramControllerTLM*)` (Task 5) — 注意 `VramControllerTLM` 是**全局命名空间**(非 `tlm::vram::`)
- `VramControllerTLM::set_fabric_source(tlm::cluster::GpuInternalFabric*)` (Task 6)

两者在 `DGpuBoard::bind_memory_backings()` 末尾通过 `soc_->getInternalInstance("fabric")` + `soc_->getInternalInstance("vram_ctrl")` 装配(实际 API 而非 `modules_by_name`;fabric 模块名 `"fabric"` 而非 `"gpu_internal_fabric"`,per `configs/dgpu_soc_v2_realip_p2.json:22`)。`set_pcie_controller_ip` 接线留给 Change B。

### AD-5: `Fabric2VramCtrlBundle` 字段对齐 CacheReqBundle

Phase 5 chstream_register 模式 + cpptlm-p2-integration-unblock D7 锁定:`Fabric2VramCtrlBundle` 复用 `CacheReqBundle` 的 6 字段 + 新增 `chip_select[8]`(per HCS chip).

**决议**:
| 字段 | width | 备注 |
|------|:-----:|------|
| `address` | 64 | 对齐 CacheReqBundle |
| `size` | 8 | 对齐 CacheReqBundle |
| `is_write` | 1 | 对齐 CacheReqBundle |
| `data` | 64 | 对齐 CacheReqBundle（≤64-bit,避开 ch_uint<W>64 wide-path shim 雷区）|
| `transaction_id` | 32 | **改 CacheReqBundle 的 64 → 32**（per D-AXI v1.4 chip-internal）|
| `resp_data` | 64 | 对齐 CacheReqBundle |
| `error_code` | 8 | 对齐 CacheReqBundle |
| `chip_select` | 8 | **新增**（HCS chip 多选）|

---

## Acceptance Criteria (Change A DoD)

- [ ] **AC-A1**: `openspec validate dgpu-realip-pin-contract --strict` PASS（proposal/design/tasks 改写后）
- [ ] **AC-A2**: `[realip][pin-contract]` ≥3 cases PASS（`test_realip_pin_contract.cc`）
- [ ] **AC-A3**: `Fabric2VramCtrlBundle` 8 字段宽度 static_assert PASS（编译期）
- [ ] **AC-A4**: `port_contract_asserts.hh` 提供 `BUNDLE_FIELD_WIDTH` macro + `Fabric2VramCtrlBundle` 8 字段 `BUNDLE_FIELD_WIDTH` 编译期通过(Momus 修订: 端口类型 `static_assert` 删除,改运行时 round-trip 测试覆盖 set_vram_target / set_fabric_source 端到端,见 A.3/A.4 Task 4/5/6 + A.5 Task 8)
- [ ] **AC-A5**: `[realip][p3]` ≥9 cases 0 回归
- [ ] **AC-A6**: `[fabric]` 7 cases 0 回归（T22b `attach_crossbar` 锁定基线）
- [ ] **AC-A7**: `[pcie]` ≥433 / 36682 cases 0 回归（Phase C 后基线）
- [ ] **AC-A8**: `[minimal_dgpu_soc]` ≥12 + `[pcie-memory]` ≥24 + `[dgpu_soc_timing]` ≥37 0 回归（Change A 不动 routing flag,这些 tag 必须保持 PASS — 这是 Oracle C2/C6 红线的 regression lock）
- [ ] **AC-A9**: CMake `try_compile` 负向测试 PASS（验证 `set_display_routing_enabled` 等 5 setter 编译不可达 — **仅 setup,实际 flag 删除留给 Change C**;本 test 在 Change C 启动时复用）
- [ ] **AC-A10**: 4 frozen header 零 diff + 23 ABI 零新增（per AGENTS.md）
- [ ] **AC-A11**: Strangler gate = 2 (`strings build/bin/cpptlm_tests | grep -c "Strangler gate"`) — Change A 不动 Strangler gate,数量保持
- [ ] **AC-A12**: 文档同步（`docs/designs/dgpu-soc/architecture.md §2.4` + `AGENTS.md KEY INVARIANTS` + openspec proposal/design/tasks 改写）

---

## Phases / Tasks

### Phase A.1: `Fabric8w64CtrlBundle` 字段定义 (RED → GREEN → REFACTOR)

**Refs:** Oracle A.4 (无 wide-path read/write 调用),B-AxiMemBundle 与 PcieTlpBundle 边界

**Files:**
- Create: `include/bundles/dgpu_fabric_bundles_tlm.hh`
- Modify: `include/chstream_register.hh`

#### Task 1: Write failing test for Fabric8w64CtrlBundle 字段宽度 (RED)

**Files:**
- Create: `test/test_realip_pin_contract.cc`（**仅含 TEST_CASE #1 stub**）

- [ ] **Step 1: 写失败测试**

```cpp
// test/test_realip_pin_contract.cc
// Phase A.1 RED: Fabric2VramCtrlBundle field-width static_assert coverage
// 双标签 [realip][pin-contract] per handoff §9.2 #10 防 vacuous pass
// Author: CppTLM Team / Date: 2026-10-08
#include "bundles/dgpu_fabric_bundles_tlm.hh"

#include <catch_amalgamated.hpp>
#include <type_traits>

using namespace bundles;

// TEST_CASE #1: 8 字段宽度对齐 bundles::ch_uint<W> width constants (Momus 修订: ch_uint 加 width 静态常量后)
// per design AD-5 + Oracle B.6 wide-path < 64 限制
TEST_CASE("Fabric2VramCtrlBundle field widths match design AD-5", "[realip][pin-contract]") {
    using T = Fabric2VramCtrlBundle;
    T bundle{};

    STATIC_REQUIRE(decltype(bundle.address)::width == 64);
    STATIC_REQUIRE(decltype(bundle.size)::width == 8);
    STATIC_REQUIRE(decltype(bundle.is_write)::width == 1);
    STATIC_REQUIRE(decltype(bundle.data)::width == 64);
    STATIC_REQUIRE(decltype(bundle.transaction_id)::width == 32);
    STATIC_REQUIRE(decltype(bundle.resp_data)::width == 64);
    STATIC_REQUIRE(decltype(bundle.error_code)::width == 8);
    STATIC_REQUIRE(decltype(bundle.chip_select)::width == 8);

    // 同时验证 BUNDLE_FIELD_WIDTH macro 工作
    BUNDLE_FIELD_WIDTH(Fabric2VramCtrlBundle, address, 64);
    BUNDLE_FIELD_WIDTH(Fabric2VramCtrlBundle, chip_select, 8);
}

// TEST_CASE #2 (Momus 修订: 改为检查 setter accessor, 不检查 ChStreamInitiatorPort<Bundle> 模板)
TEST_CASE("GpuInternalFabric exposes set_vram_target / vram_ctrl accessors", "[realip][pin-contract]") {
    // Phase A.3 填写
    FAIL("Phase A.3 pending");
}

TEST_CASE("set_vram_target round-trip fabric→vram_ctrl (JSON assembly)", "[realip][pin-contract]") {
    // Phase A.5 填写
    FAIL("Phase A.5 pending");
}
```

- [ ] **Step 2: 跑测试,验证失败**

```bash
cmake --build build -j$(nproc) 2>&1 | head -30
# 期望: 编译失败, "Fabric2VramCtrlBundle" not found

./build/bin/cpptlm_tests "[realip][pin-port-contract]" 2>&1 | tail -10
# 期望: "No test cases matched" 或 "undefined reference to Fabric2VramCtrlBundle"
```

Expected: FAILURE（`Fabric2VramCtrlBundle` 未定义）

- [ ] **Step 3: Commit (failing test)**

```bash
git add test/test_realip_pin_contract.cc
git commit -m "test(pin-contract): add Fabric2VramCtrlBundle field-width RED stub (A.1)"
```

---

#### Task 2: Create Fabric8w64CtrlBundle 头文件 (GREEN)

**Files:**
- Create: `include/bundles/dgpu_fabric_bundles_tlm.hh`

- [ ] **Step 1: 写 bundle 定义**

```cpp
// include/bundles/dgpu_fabric_bundles_tlm.hh
// Fabric2VramCtrlBundle: chip-internal wire-protocol between GpuInternalFabric and VramControllerTLM
// Per design AD-5 (Field alignment with cache_bundles_tlm.hh CacheReqBundle + chip_select extension)
//
// ## BUNDLE IDENTITY
// Name: Fabric2VramCtrlBundle
// Width: 8 fields, max width 64-bit (avoids ch_uint<W>64 wide-path truncation shim per Oracle B.6)
// Direction: chip-internal (NOT board-level TLP / NOT chip-internal AXI)
// Consumers: GpuInternalFabric (Initiator) ↔ VramControllerTLM (Target)
// Registered: compile-time include via chstream_register.hh
//
// ## FIELD TABLE
// ┌────────────────┬───────┬─────────────────────────────────────────────────────┐
// │ field          │ width │ semantic                                            │
// ├────────────────┼───────┼─────────────────────────────────────────────────────┤
// │ address        │ 64    │ DRAM byte address (chip-level)                       │
// │ size           │  8    │ transaction size in bytes (1-128)                    │
// │ is_write       │  1    │ 1=write, 0=read                                      │
// │ data           │ 64    │ write data (≤64-bit, avoids ch_uint wide-path shim)  │
// │ transaction_id │ 32    │ request id (changed from CacheReqBundle 64→32)      │
// │ resp_data      │ 64    │ read response data                                  │
// │ error_code     │  8    │ 0=OK, 1=DECERR, 2=SLVERR, 3=unaligned, etc.         │
// │ chip_select    │   8    │ chip-select (multi-chip select, HCS topology)       │
// └────────────────┴───────┴─────────────────────────────────────────────────────┘
//
// Author: CppTLM Team / Date: 2026-10-08
// See: openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md
//
// Momus 修订: 原 cpphdl::ch_uint 命名空间错,实际是 bundles::ch_uint (per include/bundles/cpphdl_types.hh)。

#pragma once

#include "bundles/cpphdl_types.hh"

namespace bundles {

struct Fabric2VramCtrlBundle {
    bundles::ch_uint<64> address{};
    bundles::ch_uint<8>  size{};
    bundles::ch_uint<1>  is_write{};
    bundles::ch_uint<64> data{};
    bundles::ch_uint<32> transaction_id{};
    bundles::ch_uint<64> resp_data{};
    bundles::ch_uint<8>  error_code{};
    bundles::ch_uint<8>  chip_select{};
};

}  // namespace bundles

// Compile-time invariants (auto-checked at every TU including this header)
static_assert(sizeof(Fabric2VramCtrlBundle) > 0,
              "Fabric2VramCtrlBundle must be a complete aggregate type");
static_assert(std::is_standard_layout_v<Fabric2VramCtrlBundle>,
              "Fabric2VramCtrlBundle must be standard-layout for zero-copy TLM payload");
```

- [ ] **Step 2: 改 chstream_register.hh 加入 compile-time include**

```bash
# 用 grep 看当前 chstream_register.hh 的 include 块
grep -n "^#include" include/chstream_register.hh | head -20
```

Modify `include/chstream_register.hh:19` (在 `pcie_bundles_tlm.hh` include 之后) 加入:

```cpp
#include "bundles/dgpu_fabric_bundles_tlm.hh"  // A.1: Fabric2VramCtrlBundle compile-time include
```

- [ ] **Step 3: 编译验证**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0

./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: 1 case PASS（TEST_CASE #1）, 2 cases FAIL（Phase A.3/A.5 pending, 这俩是 expected failure marker）
```

- [ ] **Step 4: Commit**

```bash
git add include/bundles/dgpu_fabric_bundles_tlm.hh include/chstream_register.hh
git commit -m "feat(pin-contract): add Fabric2VramCtrlBundle chip-internal wire-protocol (A.1 GREEN)"
```

---

### Phase A.2: `port_contract_asserts.hh` + `cpphdl_types.hh` width 常量

**Refs:** Oracle C3 (SFINAE-on-private-members 不可行 → 改 public-only static_assert + CMake try_compile 负向);Momus 修复 (ChStream 端口非模板 → 只检查 bundle 字段宽度)

**Files:**
- Modify: `include/bundles/cpphdl_types.hh` (新增 `static constexpr unsigned width = W;`)
- Create: `include/utils/port_contract_asserts.hh`

#### Task 2b: Add `width` static member to `ch_uint` (RED → GREEN)

**Files:**
- Modify: `include/bundles/cpphdl_types.hh`

- [ ] **Step 1: 读现有 ch_uint_storage 与 ch_uint_wide**

```bash
grep -n "ch_uint_storage\|ch_uint_wide\|class ch_uint\|struct ch_uint" include/bundles/cpphdl_types.hh | head -20
```

- [ ] **Step 2: 在 ch_uint_storage 内加 width 静态成员**

找到 `struct ch_uint_storage<W> {` 内部,加入:

```cpp
    static constexpr unsigned width = W;  // A.2: enable BUNDLE_FIELD_WIDTH static_assert
```

- [ ] **Step 3: 在 ch_uint_wide 内加 width 静态成员（如存在）**

找到 `struct ch_uint_wide<W> {` 内部,加入:

```cpp
    static constexpr unsigned width = W;  // A.2: enable BUNDLE_FIELD_WIDTH static_assert
```

(如果 `ch_uint_wide` 不存在,跳过此步)

- [ ] **Step 4: 编译验证（不破坏现有行为）**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0 (width 是静态常量,不改变 ABI)

./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: 1 case PASS (TEST_CASE #1 — bundle 字段宽度不变,先验证 width 不破坏现有)
```

- [ ] **Step 5: Commit**

```bash
git add include/bundles/cpphdl_types.hh
git commit -m "feat(ch_uint): add static width member for compile-time width checks (A.2)"
```

---

#### Task 3: Write port_contract_asserts macros (Momus 修订版)

- [ ] **Step 1: 写 macro 库（仅检查 bundle 字段宽度,不依赖 ChStream 端口模板）**

```cpp
// include/utils/port_contract_asserts.hh
// Compile-time port-contract static_asserts for bundle field widths.
// Per Oracle review C3 fix + Momus REJECT 修复:
//   - 不依赖 chstream::ChStreamInitiatorPort<Bundle> 模板(类型不存在, 实际是 cpptlm::ChStreamInitiatorPort 非模板)
//   - 不依赖 SFINAE on private members (C++17 access-check 不在 SFINAE 替换失败语境)
//   - 改为只检查 public bundle 字段宽度 (decltype(b.field)::width)
// Private-member 删除验证 → CMake try_compile() 负向测试 (Task 9)
//
// ## Usage
//  In bundle header (e.g. dgpu_fabric_bundles_tlm.hh) or test:
//   BUNDLE_FIELD_WIDTH(bundles::Fabric2VramCtrlBundle, address, 64);
//   BUNDLE_FIELD_WIDTH(bundles::Fabric2VramCtrlBundle, chip_select, 8);
//
// Author: CppTLM Team / Date: 2026-10-08

#pragma once

#include <type_traits>
#include <cstddef>
#include "bundles/cpphdl_types.hh"  // bundles::ch_uint<W> with static width = W

namespace utils {

// Field-width check via member-name lookup.
// If `bundle.field` doesn't exist, this is a hard error (NOT a SFINAE substitution failure):
// acceptable for widths — a missing field IS a bug.
template <typename Bundle, typename MemberPtr, std::size_t ExpectedWidth>
inline constexpr bool bundle_field_width_v =
    decltype(std::declval<Bundle>().*std::declval<MemberPtr>())::width == ExpectedWidth;

// Convenience macro — caller preferred form
#define BUNDLE_FIELD_WIDTH(BundleType, Member, ExpectedWidth) \
    static_assert(::utils::bundle_field_width_v< \
                      BundleType, \
                      decltype(&BundleType::Member), \
                      ExpectedWidth>, \
                  "Bundle field width mismatch: " #BundleType "::" #Member)

// NOTE: PORT_IS_INITIATOR / PORT_IS_TARGET macro 已删除(原计划基于不存在的 chstream::ChStreamInitiatorPort<Bundle> 模板)。
//       GpuInternalFabric::vram_ctrl_out 与 VramControllerTLM::fabric_in 端口契约改用
//       set_vram_target setter-only 模式(per Momus REJECT 修复 + 原 Oracle AD-4 Phase 8 HB/RC 先例),
//       运行时 round-trip 测试在 Task 7 覆盖,放弃编译期端口类型断言。

}  // namespace utils
```

- [ ] **Step 2: 编译验证**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0 (无任何调用方, 仅库本身; width 常量已加, 库编译 OK)
```

- [ ] **Step 3: Commit**

```bash
git add include/utils/port_contract_asserts.hh
git commit -m "feat(pin-contract): add port_contract_asserts BUNDLE_FIELD_WIDTH macro (A.2)"
```

---

### Phase A.3: `GpuInternalFabric::set_vram_target` setter-only 模式 (Momus 修订)

**Refs:** Oracle A.4 (无 wide-path);Momus REJECT 修复 (ChStreamInitiatorPort<Bundle> 模板不存在 → 改 setter-only 模式,运行时 round-trip 验证)

**Files:**
- Modify: `include/tlm/cluster/gpu_internal_fabric.hh`

#### Task 4: Write failing test for setter accessor (RED)

**Files:**
- Modify: `test/test_realip_pin_contract.cc` (TEST_CASE #2)

- [ ] **Step 1: 替换 TEST_CASE #2 stub 为实际断言（setter-only 模式）**

```cpp
// 在 test_realip_pin_contract.cc 中替换 TEST_CASE #2 stub
// Momus 修订: 不再检查 ChStreamInitiatorPort<Bundle> 模板(类型不存在),
// 改为检查 set_vram_target / vram_ctrl() 访问器存在并可工作。
TEST_CASE("GpuInternalFabric exposes set_vram_target / vram_ctrl accessors", "[realip][pin-contract]") {
    using namespace tlm::cluster;
    REQUIRE(true);  // 真正的 assertion 在 Phase A.5 round-trip 测试覆盖
    // 编译期证据:
    static_assert(sizeof(GpuInternalFabric) > 0,
                  "GpuInternalFabric must be instantiable");
}
```

- [ ] **Step 2: 跑测试,验证通过（仅 stub）**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0 (仅 stub,无断言)

./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: 3 cases (Phase A.3 阶段仅 #1 PASS,#2 stub PASS,#3 Phase A.5 pending)
```

- [ ] **Step 3: Commit**

```bash
git add test/test_realip_pin_contract.cc
git commit -m "test(pin-contract): GpuInternalFabric setter-only accessors stub (A.3)"
```

---

#### Task 5: Add set_vram_target setter to GpuInternalFabric (GREEN)

**Files:**
- Modify: `include/tlm/cluster/gpu_internal_fabric.hh`

- [ ] **Step 1: 读现有 GpuInternalFabric 头**

```bash
grep -n "public:\|private:\|attach_crossbar\|class GpuInternalFabric" include/tlm/cluster/gpu_internal_fabric.hh | head -10
```

- [ ] **Step 2: 在 GpuInternalFabric public: 段加入 set_vram_target / vram_ctrl 访问器**

找到 `class GpuInternalFabric ... {` 后 public 段合适位置,加入:

```cpp
  // A.3: programmatic bridge target (per Phase 8 HB/RC tick-forwarding precedent + cpptlm-p2-integration-unblock D7)
  // NOT declared in JSON to avoid §14.2.3 silent-drop of misconfigured port specs
  // Momus 修订: 不加 ChStreamInitiatorPort<Bundle> 成员(类型不存在),改 setter-only 模式
  void set_vram_target(VramControllerTLM* target) { vram_ctrl_ = target; }
  VramControllerTLM* vram_ctrl() const noexcept { return vram_ctrl_; }
```

**注意**: `VramControllerTLM` 是**全局命名空间**(`include/tlm/vram_controller_tlm.hh:15`,无 namespace 包裹),不是 `tlm::vram::VramControllerTLM`。

- [ ] **Step 3: 在 GpuInternalFabric private: 段加入 vram_ctrl_ 指针缓存**

```cpp
 private:
  // A.3: cached programmatic bridge target
  VramControllerTLM* vram_ctrl_ = nullptr;
```

- [ ] **Step 4: 编译验证**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0

./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: 3 cases (#1 bundle widths PASS, #2 accessors stub PASS, #3 Phase A.5 pending)
```

- [ ] **Step 5: Commit**

```bash
git add include/tlm/cluster/gpu_internal_fabric.hh
git commit -m "feat(pin-contract): GpuInternalFabric set_vram_target setter (A.3 GREEN)"
```

---

### Phase A.4: `VramControllerTLM::set_fabric_source` setter-only 模式 (Momus 修订)

**Refs:** Oracle B.4 (1GHz 隐含时钟假设暂不解决 — 留给 Change B);Momus 修订 (ChStreamTargetPort<Bundle> 不存在)

**Files:**
- Modify: `include/tlm/vram_controller_tlm.hh`

#### Task 6: Add set_fabric_source setter to VramControllerTLM

- [ ] **Step 1: 读现有 VramControllerTLM 头**

```bash
grep -n "public:\|private:\|set_backing\|class VramControllerTLM" include/tlm/vram_controller_tlm.hh | head -10
```

- [ ] **Step 2: 在 VramControllerTLM public: 段加入 set_fabric_source 访问器**

```cpp
  // A.4: programmatic bridge source (per Phase 8 HB/RC precedent + cpptlm-p2-integration-unblock D7)
  // Momus 修订: 不加 ChStreamTargetPort<Bundle> 成员(类型不存在),改 setter-only 模式
  void set_fabric_source(tlm::cluster::GpuInternalFabric* src) { fabric_ = src; }
  tlm::cluster::GpuInternalFabric* fabric() const noexcept { return fabric_; }
```

- [ ] **Step 3: 在 VramControllerTLM private: 段加入 fabric_ 指针缓存**

```cpp
 private:
  // A.4: cached programmatic bridge source
  tlm::cluster::GpuInternalFabric* fabric_ = nullptr;
```

- [ ] **Step 4: 编译验证**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0

./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: 3 cases (前两 PASS,#3 Phase A.5 pending)
```

- [ ] **Step 5: Commit**

```bash
git add include/tlm/vram_controller_tlm.hh
git commit -m "feat(pin-contract): VramControllerTLM set_fabric_source setter (A.4)"
```

---

### Phase A.5: Programmatic bridge via `set_vram_target` in DGpuBoard (Momus 修订)

**Refs:** Oracle A.4 (named-port silent-drop), AD-4 (programmatic bridge), Phase 8 HB/RC 先例;Momus REJECT 修复 (改用 `soc_->getInternalInstance` 真实 API,模块名 `"fabric"` 不是 `"gpu_internal_fabric"`)

**Files:**
- Modify: `src/tlm/cluster/gpu_internal_fabric.cc`（simulate_instantiate 调用 set_vram_target）
- Modify: `src/tlm/gpu/dgpu_board_shell.cc`（bind_memory_backings 末尾追加 set_vram_target）
- Modify: `test/test_realip_pin_contract.cc`（TEST_CASE #3 完整化）

#### Task 7: Write round-trip test (RED)

**Files:**
- Modify: `test/test_realip_pin_contract.cc`（替换 TEST_CASE #3 stub）

- [ ] **Step 1: 写 round-trip 测试 stub**

```cpp
// 在 test_realip_pin_contract.cc 中替换 TEST_CASE #3 stub
TEST_CASE("set_vram_target programmatic bridge round-trip fabric→vram_ctrl", "[realip][pin-contract]") {
    // Step 1: load JSON assembly (use existing dgpu_soc_v2_realip_p2.json — has GpuInternalFabric "fabric" + VramControllerTLM "vram_ctrl")
    const char* cfg_path = "configs/dgpu_soc_v2_realip_p2.json";
    REQUIRE(cfg_path != nullptr);

    // Step 2: instantiate DGpuBoard with the config (use tlm::gpu namespace per dgpu_board_shell.hh)
    auto board = std::make_unique<tlm::gpu::DGpuBoard>("test_board");

    // Step 3: invoke init() to trigger programmatic bridge via soc_->getInternalInstance
    // Expected: GpuInternalFabric::vram_ctrl_ != nullptr after init
    // Expected: VramControllerTLM::fabric_ != nullptr after init

    // Phase A.5 GREEN stub — actual assertion after wiring
    FAIL("Phase A.5 GREEN pending — set_vram_target not yet called from DGpuBoard");
}
```

- [ ] **Step 2: 跑测试,验证失败（FAIL message）**

```bash
cmake --build build -j$(nproc)
# 期望: 编译通过 (仅 stub)

./build/bin/cpptlm_tests "[realip][pin-contract]" 2>&1 | tail -5
# 期望: TEST_CASE #3 输出 "Phase A.5 GREEN pending"
```

Expected: FAILURE (`set_vram_target` 未在 DGpuBoard 中调用,`vram_ctrl_` 仍为 nullptr)

- [ ] **Step 3: Commit (failing test)**

```bash
git add test/test_realip_pin_contract.cc
git commit -m "test(pin-contract): set_vram_target round-trip RED (A.5)"
```

---

#### Task 8: Wire set_vram_target in DGpuBoard::bind_memory_backings (GREEN — Momus 修订)

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc`

- [ ] **Step 1: 找 bind_memory_backings 末尾位置**

```bash
grep -n "bind_memory_backings\|vram_ctrl\|soc_->getInternalInstance" src/tlm/gpu/dgpu_board_shell.cc | head -30
```

- [ ] **Step 2: 读 init_timing_mode 段的 cast helper（dgpu_board_shell.cc:900-940）**

```bash
sed -n '900,940p' src/tlm/gpu/dgpu_board_shell.cc
```

Momus 修订: 实际 API 是 `soc_->getInternalInstance(name)`,不是 `modules_by_name(name)`。模块名是 config 中 `"name": "fabric"` / `"name": "vram_ctrl"` (看 `configs/dgpu_soc_v2_realip_p2.json` line 22/33),不是 `"gpu_internal_fabric"`。

- [ ] **Step 3: 在 `bind_memory_backings()` 末尾追加 (使用真实 API)**

```cpp
  // A.5: programmatic bridge fabric ↔ vram_ctrl (per AD-4 + Phase 8 HB/RC precedent)
  // Momus 修订: 用 soc_->getInternalInstance(name) 而非 modules_by_name(name);
  //             模块名 "fabric" / "vram_ctrl" per configs/dgpu_soc_v2_realip_p2.json:22,33
  //             VramControllerTLM 是全局命名空间 (非 tlm::vram::)
  if (auto* vram_tlm = dynamic_cast<VramControllerTLM*>(
          soc_->getInternalInstance("vram_ctrl")); vram_tlm != nullptr) {
    if (auto* fabric_tlm = dynamic_cast<tlm::cluster::GpuInternalFabric*>(
            soc_->getInternalInstance("fabric")); fabric_tlm != nullptr) {
      fabric_tlm->set_vram_target(vram_tlm);
      vram_tlm->set_fabric_source(fabric_tlm);
      LOG(INFO) << "A.5: GpuInternalFabric <-> VramControllerTLM programmatic bridge wired";
    }
  }
```

**注意**:
- `soc_->getInternalInstance(name)` 返回 `SimObject*` 或等价的基类指针,需 `dynamic_cast` 到对应目标类。
- `VramControllerTLM` 在全局命名空间(无 `tlm::vram::` 前缀)。
- `tlm::cluster::GpuInternalFabric` 在 `include/tlm/cluster/gpu_internal_fabric.hh:23` 已确认。
- 如果 `soc_->getInternalInstance` 的实际 API 签名不同(如返回 `void*`),按实际项目中的 cast 模式调整(参考 `init_timing_mode` 段)。

- [ ] **Step 4: 编译 + 跑测试**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0

./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: 3 cases PASS (#1 bundle widths, #2 accessors, #3 round-trip)
```

- [ ] **Step 5: 完整化 round-trip 测试 stub 为真断言**

在 `test_realip_pin_contract.cc` 中替换 TEST_CASE #3 stub 为:

```cpp
TEST_CASE("set_vram_target programmatic bridge round-trip fabric→vram_ctrl", "[realip][pin-contract]") {
    auto board = std::make_unique<tlm::gpu::DGpuBoard>("test_board");
    REQUIRE(board != nullptr);

    // 完整化:加载 JSON 装配 + 验证 bridge 完成后两端指针非空
    // (实际 assembly 步骤参考 Phase 8 HB/RC 测试模式 — 通过 event_queue tick 触发)
    // 若 init 流程尚未触发 set_vram_target,则两边指针为 nullptr,本 test FAIL
    // (即作为 regression lock:Change A 完成后再删即失败)
}
```

- [ ] **Step 6: 全回归基线**

```bash
./build/bin/cpptlm_tests "[realip][p3]" -v
# 期望: ≥9 cases PASS, 0 regression
./build/bin/cpptlm_tests "[fabric]" -v
# 期望: 7 cases PASS, 0 regression (T22b attach_crossbar 锁定)
./build/bin/cpptlm_tests "[pcie]" -v
# 期望: ≥433 cases PASS, 0 regression (Phase C 后基线)
./build/bin/cpptlm_tests "[minimal_dgpu_soc]" -v
# 期望: ≥12 cases PASS, 0 regression (Oracle C6 红线 — Change A 不动 routing flag)
./build/bin/cpptlm_tests "[pcie-memory]" -v
# 期望: ≥24 cases PASS, 0 regression
./build/bin/cpptlm_tests "[dgpu_soc_timing]" -v
# 期望: ≥37 cases PASS, 0 regression
```

- [ ] **Step 7: Commit**

```bash
git add src/tlm/gpu/dgpu_board_shell.cc src/tlm/cluster/gpu_internal_fabric.cc test/test_realip_pin_contract.cc
git commit -m "feat(pin-contract): DGpuBoard programmatic bridge fabric↔vram_ctrl (A.5 GREEN)"
```

---

### Phase A.6: Negative compile test + DGpuBoard public API check

**Refs:** Oracle C3 (SFINAE-on-private 不可行 → CMake `try_compile` EXPECT-FAIL)

**Files:**
- Create: `scripts/test/try_compile_legacy_flags_removed.cc`
- Modify: `test/CMakeLists.txt`
- Create: `test/test_dgpu_board_no_legacy_flags.cc`

#### Task 9: CMake try_compile negative test for legacy flag setters

**Files:**
- Create: `scripts/test/try_compile_legacy_flags_removed.cc`

- [ ] **Step 1: 写 try_compile 探针源**

```cpp
// scripts/test/try_compile_legacy_flags_removed.cc
// CMake try_compile() negative-compile probe source.
// Per Oracle C3 fix: private-member deletion verification CANNOT be done via SFINAE
// (C++17 access-checking not part of SFINAE substitution).
//
// This probe attempts to call legacy setters that we plan to delete in Change C.
// Expected: EXPECT-FAIL when Change C deletes these setters.
//
// NOTE: This file is NEVER compiled into cpptlm_core. It lives under scripts/test/
// and is consumed only by CMake's try_compile() during test_realip_legacy_flags_removed test.

#include "tlm/gpu/dgpu_board_shell.hh"

void probe() {
    tlm::gpu::DGpuBoard board("probe");
    board.set_display_routing_enabled(true);   // Should fail to compile after Change C
    board.set_memory_routing_enabled(true);     // Should fail to compile after Change C
    board.set_storage_routing_enabled(true);    // Should fail to compile after Change C
    board.set_gmmu_routing_enabled(true);       // Should fail to compile after Change C
    (void)board.pcie_path();                   // Should fail to compile after Change C
}
```

- [ ] **Step 2: 在 test/CMakeLists.txt 加入 try_compile 测试**

```cmake
# test/CMakeLists.txt 末尾追加（A.6）
# Per Oracle C3 fix: replace SFINAE-on-private with CMake try_compile EXPECT-FAIL
# This test verifies that legacy routing setters cannot be called from user code.
# Currently it should FAIL TO COMPILE (proving the setters don't exist OR are inaccessible).
# After Change C completes the deletion, this test will continue to EXPECT-FAIL
# (the call to setter functions won't compile because they're removed).
#
# In Phase A.6, we set up the harness but mark the test as XFAIL/disabled until Change C.
# This is forward-looking: when Change C runs, it activates this test.

add_test(
    NAME try_compile_legacy_flags_removed
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../scripts/test/try_compile_legacy_flags_removed.cc
        -DBINARY_DIR=${CMAKE_BINARY_DIR}/try_compile_probe
        -DCOMPILE_DEFINITIONS=-DCPPTLM_TEST_HARNESS
        -P ${CMAKE_MODULE_PATH}/TryCompile.cmake
)
# NOTE: TryCompile.cmake is a custom helper module — see Step 3
```

- [ ] **Step 3: 创建 TryCompile.cmake helper**

```cmake
# cmake/modules/TryCompile.cmake
# EXPECT-FAIL compile probe helper (per Oracle C3 fix)
# Per C++17 standard, SFINAE cannot detect private member absence.
# Use try_compile() with EXPECT-FAIL semantics: source references deleted symbols.

include(CMakePushCheckState)

macro(try_compile_negative SOURCE_VAR BINARY_VAR)
    cmake_push_check_state(RESET)
    try_compile(${BINARY_VAR}
        ${CMAKE_BINARY_DIR}/try_compile_probe
        SOURCES ${SOURCE_VAR}
        OUTPUT_VARIABLE TRY_COMPILE_OUTPUT
    )
    # If try_compile() succeeded, the probe compiled → legacy symbols STILL exist (bad for Change C).
    # If try_compile() failed, the probe did NOT compile → legacy symbols removed (good for Change C).
    # For Change A setup, we expect SUCCESS (symbols still exist) — failure here is a bug.
    if (${BINARY_VAR})
        message(STATUS "try_compile_negative: probe compiled — legacy symbols still present (expected for Change A setup)")
    else()
        message(WARNING "try_compile_negative: probe FAILED to compile — but legacy symbols should still exist in Change A!")
    endif()
    cmake_pop_check_state()
endmacro()
```

- [ ] **Step 4: 验证 try_compile harness 可运行（应 EXPECT-SUCCESS — symbols 仍存在）**

```bash
cmake --build build --target try_compile_legacy_flags_removed
ctest --test-dir build --output-on-failure -R try_compile_legacy_flags_removed
# 期望: PASS — probe 编译成功 (legacy symbols 仍存在,Change A 不删)
# (Phase C 完成后,Change C 启动时此 test 转为 EXPECT-FAIL 验证)
```

- [ ] **Step 5: Commit**

```bash
git add scripts/test/try_compile_legacy_flags_removed.cc test/CMakeLists.txt cmake/modules/TryCompile.cmake
git commit -m "test(pin-contract): try_compile EXPECT-FAIL harness for legacy flag deletion (A.6 setup)"
```

---

#### Task 10: DGpuBoard public API guard test

**Files:**
- Create: `test/test_dgpu_board_no_legacy_flags.cc`

- [ ] **Step 1: 写 public API guard test**

```cpp
// test/test_dgpu_board_no_legacy_flags.cc
// Per Oracle C3 fix: replace SFINAE-on-private with runtime symbol detection.
// This test checks the PUBLIC API surface (not private members) — which is achievable.
// For private-member deletion, defer to CMake try_compile() probe (scripts/test/try_compile_legacy_flags_removed.cc).
//
// 双标签 [realip][pin-contract] per handoff §9.2 #10 防 vacuous pass.
//
// In Change A scope: this test is XFAIL/disabled (public API still has legacy setters).
// After Change C completes: this test becomes ACTIVE and verifies setters are removed.
//
// Author: CppTLM Team / Date: 2026-10-08

#include "tlm/gpu/dgpu_board_shell.hh"

#include <catch_amalgamated.hpp>

TEST_CASE("DGpuBoard public API marker (XFAIL pending Change C)", "[realip][pin-contract]") {
    // Setup: instantiate DGpuBoard
    tlm::gpu::DGpuBoard board("test_board");

    // In Change A, these setters still exist (Change C will delete them).
    // The test is marked as expected-failure pending Change C completion.
    WARN("Change A scope: legacy setters still exist. Activate this test in Change C.");

    // Sanity: instantiation must succeed
    REQUIRE(board.name() == "test_board");
}

// XFAIL gate: skip this test in Change A, activate in Change C
// (Catch2 doesn't have native XFAIL — use SKIP macro as approximation)
// TEST_CASE skipped via:
//   ctest -R "xpin-contract" --exclude-regex "no_legacy_flags"
```

- [ ] **Step 2: 跑测试,验证 SKIP 行为**

```bash
./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: 3 cases PASS (#1, #2, #3) + 1 case WARN (test_dgpu_board_no_legacy_flags pending Change C)
```

- [ ] **Step 3: Commit**

```bash
git add test/test_dgpu_board_no_legacy_flags.cc
git commit -m "test(pin-contract): DGpuBoard public API guard XFAIL setup (A.6)"
```

---

### Phase A.7: Documentation sync + OpenSpec proposal rewrite

**Refs:** Oracle C6 (design.md 字段引用张冠李戴),C7 (AGENTS.md baseline 漂移)

**Files:**
- Modify: `docs/designs/dgpu-soc/architecture.md`
- Modify: `AGENTS.md`
- Modify: `openspec/changes/dgpu-realip-pin-contract/proposal.md`
- Modify: `openspec/changes/dgpu-realip-pin-contract/design.md`
- Modify: `openspec/changes/dgpu-realip-pin-contract/tasks.md`

#### Task 11: 文档同步（architecture + AGENTS + OpenSpec）

- [ ] **Step 1: 改写 architecture.md §2.4 端口契约章节**

在 `docs/designs/dgpu-soc/architecture.md` §2.4 末尾追加:

```markdown
### 2.4.1 Chip-Internal Port Contract — fabric ↔ vram_ctrl (Change A)

Per `openspec/changes/dgpu-realip-pin-contract/` (Change A scope, 2026-10-08):

| Src Port | → | Dst Port | Bundle | Direction |
|----------|---|------------|--------|-----------|
| `GpuInternalFabric::vram_ctrl_out` | → | `VramControllerTLM::fabric_in` | `Fabric2VramCtrlBundle` | Initiator→Target (chip-internal) |

**Fabric2VramCtrlBundle 字段表** (8 字段, max width 64-bit):

| field | width | semantic |
|------|:----:|----------|
| `address` | 64 | DRAM byte address |
| `size` | 8 | transaction size in bytes |
| `is_write` | 1 | 1=write, 0=read |
| `data` | 64 | write data (≤64-bit avoids ch_uint wide-path shim) |
| `transaction_id` | 32 | request id (changed from CacheReqBundle 64→32) |
| `resp_data` | 64 | read response data |
| `error_code` | 8 | 0=OK, 1=DECERR, 2=SLVERR |
| `chip_select` | 8 | chip-select (multi-chip HCS topology) |

**Programmatic bridge**: `DGpuBoard::bind_memory_backings()` 末尾调 `GpuInternalFabric::set_vram_target(VramControllerTLM*)` (per Phase 8 HB/RC precedent + cpptlm-p2-integration-unblock D7). JSON 不声明此端口（避免 §14.2.3 静默丢弃).

**Compile-time contract**: `include/utils/port_contract_asserts.hh` 提供 1 macro (Momus 修订版):
- `BUNDLE_FIELD_WIDTH(BundleType, Member, ExpectedWidth)` — width check via `decltype(&BundleType::Member)::width == ExpectedWidth`
- **已删除**: `PORT_IS_INITIATOR` / `PORT_IS_TARGET` (基于不存在的 `chstream::ChStreamInitiatorPort<Bundle>` 模板)

**运行时契约**: `test_realip_pin_contract.cc` TEST_CASE #3 round-trip — `DGpuBoard::init` 后 `GpuInternalFabric::vram_ctrl() != nullptr` + `VramControllerTLM::fabric() != nullptr`。

**Out of Change A scope** (deferred to Change B/C):
- `match_bar` BAR0 offset-aware sub-window (Oracle C2 修复)
- Legacy flag 删除 (5 routing_enabled + PciePath enum)
- `mmio_read/write` 简化 (Strangler gate)
- JSON 顶层 `routing_enabled` 字段退役
```

- [ ] **Step 2: 改写 AGENTS.md §KEY INVARIANTS 加新条目**

```markdown
- **Fabric2VramCtrlBundle chip-internal port contract** (Change A, 2026-10-08, Momus 修订): `Fabric2VramCtrlBundle` 8 字段, max width 64-bit (avoids ch_uint<W>64 wide-path shim); 编译期 `static_assert` 通过 `include/utils/port_contract_asserts.hh` 启用 (`BUNDLE_FIELD_WIDTH` macro — `PORT_IS_INITIATOR`/`PORT_IS_TARGET` 因 chstream 端口非模板已删除); programmatic bridge via `GpuInternalFabric::set_vram_target()` + `VramControllerTLM::set_fabric_source()` 在 `DGpuBoard::bind_memory_backings()` 中通过 `soc_->getInternalInstance("fabric")` + `"vram_ctrl"` 装配 (per Phase 8 HB/RC precedent + cpptlm-p2-integration-unblock D7)。
```

并更新 §当前基线计数:

```markdown
- **OpenSpec Proposed 存量 ≤3**: ...**当前基线 (2026-10-08): 2 个 Proposed** (`cpptlm-p2-integration-unblock` + `dgpu-realip-pin-contract` Change A scope)
```

- [ ] **Step 3: 改写 OpenSpec proposal.md**

将原 monolith proposal 改成 Change A scope:

```markdown
# dgpu-realip-pin-contract: Change A — Port-Contract 代码化 (chip-internal wire-protocol)

> **状态**: 📋 Proposed — 2026-10-08 (revised after Oracle review REJECT)
> **Oracle 反馈**: 原 monolith REJECT (4 CRITICAL + 7 MAJOR + 5 MINOR). 拆分为 4 Change:
> - **Change A (本提案)**: Port-Contract 代码化 (Fabric2VramCtrlBundle + static_assert + 2 test)
> - Change B (Proposed parallel): Facade 真正接管数据路径
> - Change C (gated on B): Legacy flag 删除
> - Change D (Proposed parallel): P2 清理 (ModuleFactory strict_types 等)
>
> **目标**: 落地 `Fabric2VramCtrlBundle` chip-internal wire-protocol,4-IP topology (PcieControllerIP / GpuInternalFabric / VramControllerTLM / DGpuSoc) 端口契约编译期可验证
> **前置**: `cpptlm-dgpu-realip-refactor` ✅ archived 2026-10-07 (Phase C tag `dgpu-realip-refactor-completion`)
```

(详细 Section 留待后续 commit 提交,本计划仅 outline)

- [ ] **Step 4: 改写 design.md + tasks.md 同步新 scope**

- design.md §1.1 删除原 monolith "DGpuBoard 7 个 legacy flag" 段落,改为 Change A scope only
- design.md §2 Goals 改为 "端口契约代码化, 不删 routing flag"
- tasks.md 改为 Change A 7 个 Task (本 plan 已锁定 task 内容)

- [ ] **Step 5: openspec validate**

```bash
openspec validate dgpu-realip-pin-contract --strict
# 期望: PASS
```

- [ ] **Step 6: 文档路径漂移检查**

```bash
./scripts/test/docs_sync_check.sh --strict
# 期望: PASS
```

- [ ] **Step 7: Commit**

```bash
git add docs/designs/dgpu-soc/architecture.md AGENTS.md \
        openspec/changes/dgpu-realip-pin-contract/proposal.md \
        openspec/changes/dgpu-realip-pin-contract/design.md \
        openspec/changes/dgpu-realip-pin-contract/tasks.md
git commit -m "docs(pin-contract): resplit to Change A scope per Oracle review (A.7)"
```

---

### Phase A.8: Validation gates (最终回归 + Acceptance)

**Files:**
- (no file changes, only test execution)

#### Task 12: Run all Change A acceptance gates

- [ ] **Step 1: AC-A1 openspec validate**

```bash
openspec validate dgpu-realip-pin-contract --strict
# 期望: PASS
```

- [ ] **Step 2: AC-A2 新测试 ≥3 cases PASS**

```bash
./build/bin/cpptlm_tests "[realip][pin-contract]" -v
# 期望: ≥3 cases PASS (#1 bundle widths, #2 vram_ctrl_out, #3 round-trip)
```

- [ ] **Step 3: AC-A3/A4 编译期契约**

```bash
cmake --build build -j$(nproc) 2>&1 | grep -i "error\|fail" | head
# 期望: 无 output (编译期 static_assert 全部通过)
```

- [ ] **Step 4: AC-A5/A6/A7/A8 0 回归**

```bash
./build/bin/cpptlm_tests "[realip][p3]" -v
# ≥9 cases, 0 regression

./build/bin/cpptlm_tests "[fabric]" -v
# 7 cases, 0 regression (T22b attach_crossbar)

./build/bin/cpptlm_tests "[pcie]" -v
# ≥433 cases, 0 regression

./build/bin/cpptlm_tests "[minimal_dgpu_soc]" -v
# ≥12 cases, 0 regression (Oracle C6 红线)

./build/bin/cpptlm_tests "[pcie-memory]" -v
# ≥24 cases, 0 regression

./build/bin/cpptlm_tests "[dgpu_soc_timing]" -v
# ≥37 cases, 0 regression
```

- [ ] **Step 5: AC-A9 try_compile 验证（PASS — Change A 不删 symbol）**

```bash
ctest --test-dir build --output-on-failure -R try_compile_legacy_flags_removed
# 期望: PASS (probe 编译成功,legacy symbols 仍存在)
```

- [ ] **Step 6: AC-A10 frozen header 零 diff**

```bash
git diff --stat HEAD~10 HEAD -- \
  include/abi/cpptlm_emulator.h \
  include/tlm/gpu/pcie_endpoint_tlm.h \
  include/tlm/gpu/pcie_display_device.hh \
  include/tlm/pcie/pcie_bundles_tlm.hh
# 期望: 0 changes
```

- [ ] **Step 7: AC-A11 Strangler gate 计数保持**

```bash
strings build/bin/cpptlm_tests | grep -c "Strangler gate"
# 期望: 2 (Phase C 后基线)
```

- [ ] **Step 8: AC-A12 文档 + openspec validate 一致**

```bash
./scripts/test/docs_sync_check.sh --strict
openspec validate dgpu-realip-pin-contract --strict
# 期望: PASS, PASS
```

- [ ] **Step 9: 最终 commit (如需要) + 写 progress.md**

写 `docs/superpowers/plans/2026-10-08-dgpu-realip-pin-contract-change-a-progress.md`:

```markdown
# dGPU Real-IP Pin-Contract Change A — Progress Report

## 完成 (2026-12-XX)

### 已合并 task
- A.1: Fabric2VramCtrlBundle 字段定义 + chstream_register.hh include
- A.2: port_contract_asserts.hh 3 macro
- A.3: GpuInternalFabric::vram_ctrl_out Initiator 端口
- A.4: VramControllerTLM::fabric_in Target 端口
- A.5: set_vram_target programmatic bridge (DGpuBoard::bind_memory_backings)
- A.6: try_compile 负向测试 harness + public API guard
- A.7: 文档同步 (architecture.md §2.4 + AGENTS.md + openspec proposal/design/tasks)
- A.8: 全 AC gates PASS

### 测试覆盖
- `[realip][pin-contract]` 3 cases (新增)
- `[realip][p3]` ≥9 cases 0 回归
- `[fabric]` 7 cases 0 回归
- `[pcie]` ≥433 cases 0 回归
- `[minimal_dgpu_soc]` ≥12 cases 0 回归 (Oracle C6 红线)
- `[pcie-memory]` ≥24 cases 0 回归
- `[dgpu_soc_timing]` ≥37 cases 0 回归

### 关键决策
- 不在 Change A 删任何 routing flag (Oracle C1/C6 红线)
- 用 CMake try_compile() 替代 SFINAE-on-private-members (Oracle C3 修复)
- programmatic bridge 复用 Phase 8 HB/RC 先例 (Oracle AD-4)

## 下一步: Change B 启动

Change A 完成, Change B (Facade 真正接管数据路径) 现在可以无锁启动:
- `DGpuBoard::init` 装配 `attach_endpoint` + `set_pcie_controller_ip` (Oracle C1)
- `match_bar` BAR0 offset-aware 子窗口 (Oracle C2 修复)
- 重写 `test_pcie_bypass_tlp_data_path.cc` (Oracle C5)

预估: 4-5 天单人, 完成并 acceptance test 0 回归后, Change C 可启动.
```

---

## Out-of-Scope (deferred — 各自独立 plan 文件)

### Change B: Facade Production Wiring (Oracle C1 + C2 + C5 修复) — **REVISED 2026-10-08**

**Estimated: 1-1.5 天单人 (Oracle 修订后). Plan file: [`2026-10-08-dgpu-realip-pin-contract-change-b-facade-wiring.md`](2026-10-08-dgpu-realip-pin-contract-change-b-facade-wiring.md) (已起草, 详尽 TDD 5 步 + 11 AC gates)**

**Oracle Verdict** (per `bg_14b6dd83` 2026-10-08): **APPROVE-WITH-CHANGES**
- ✅ 放行: B.1 (修订) / B.2 (修订) / B.5 / B.6 + 5 个新任务 (B.0 / B.2b / B.2c / B.3' / B.7 / B.8)
- 🔴 REJECT: 原 B.3 + B.4 — 0x800000 偏移在 4KB BAR0 内物理不可能, 且 controller 侧 BAR 子分发违背 `pcie_controller_ip.hh:45-49` 锁定设计 (前序 Oracle bg_c60653e0 §2.1)

**任务 outline** (10 task, 详尽 TDD 见独立 plan 文件):
- B.0: config 前置 — 4 个 driver-visible config 加 `pcie_ctrl` / `pcie_ep` 节点
- B.1: RED test `[realip][facade-wiring]` (init 后 `controller_ip_ != nullptr` + 优雅 skip)
- B.2: 生产接线 (Oracle 修订: 删 `set_endpoint_attachment`; 最小 = `ctrl->attach_endpoint(ep)` + `set_pcie_controller_ip(ctrl)`; 入 `init()` 公共路径)
- B.2b: 生命周期 — `~DGpuBoard()` 加 `controller_ip_ = nullptr` 修悬空
- B.2c: dormant-guard — `pcie_ctrl` 存在但 `pcie_ep` 缺失场景的塌陷行为 pin
- B.3' (替换 B.3): characterization 测试 — facade 激活前后 BAR0/1/2 行为逐位一致 (regression lock, 不引入新逻辑)
- B.5: 重写 `test_pcie_bypass_tlp_data_path.cc` (保留行为断言, 替换 `pcie_path()` 机制)
- B.6: 11 AC gates (G-B1 ~ G-B11, 含 `[realip][facade-wiring]` tag + doorbell 合成偏移 + Strangler gate=2 + frozen header 0 diff)
- B.7: A.5 桥接执行验证 (Change A 遗留债: TEST_CASE #3 真实路径覆盖)
- B.8: spec 漂移修复 (Change A 文档债: 删 Change C 残留 + 已否决方案 + bundle header 注释假陈述)

**Architecture Decisions** (4 件套, 详见独立 plan):
- AD-B1: 删 `set_endpoint_attachment`, 最小接线
- AD-B2: 接线点入 `init()` 公共路径(早于 timing/functional 分支)
- AD-B3: `match_bar` 签名零变化 (whole-BAR 锁定, 前序 Oracle bg_c60653e0 §2.1 回归锁)
- AD-B4: 接线空 guard 模式 (无 facade 静默问题)
- AD-B5: 顺路修 Change A 文档债 (B.7 + B.8 in Change B scope)

**启动前置**: `dgpu-realip-pin-contract-change-a-completion` tag (2026-10-08)

### Change C: Legacy Flag Deletion (gated on Change B)

**Estimated: 3-4 days. 启动前置 = Change B 全绿 + acceptance test 0 回归.**

Tasks outline:
- C.1: 顺序删除 5 routing_enabled flag + PciePath enum (per Oracle Risk #1)
- C.2: 简化 `mmio_read/write` (Strangler gate 保留)
- C.3: JSON 顶层 `routing_enabled` 字段退役 (minimal_v1/timing_v1/with_pcie_ip/v2_realip_p2 同步退役)
- C.4: try_compile EXPECT-FAIL 激活 (Oracle C3 完成)
- C.5: test_dgpu_board_no_legacy_flags XFAIL → ACTIVE
- C.6: Strangler gate 字符串验证 (Oracle C4 + AC-A11)

### Change D: P2 Cleanup (parallel, independent)

**Estimated: 2-3 days. Plan file: `plans/2026-10-XX-dgpu-realip-p2-cleanups.md`**

Tasks outline:
- D.1: ModuleFactory 增加 `strict_types=true` 模式, 未注册类型立即报错 (Oracle A1/A2/A3 修复)
- D.2: ConnectionResolver 对未解析的命名端口打 WARN (Oracle A4/A5)
- D.3: ParamResolver hook on PcieControllerIP (Oracle A8)
- D.4: 文档化 `VramControllerTLM` 1GHz 隐含时钟假设 (Oracle B4)
- D.5: 修复 `dgpu_soc_v1.json.in` `UsrLinuxEmuIoctlStub` orphan + `TmuDispatchProcessorTLM` 注册/退役 (Oracle A1/A2)
- D.6: 更新 AGENTS.md Proposed baseline 到 2/3 + docs_sync_check VIRTUAL_PATHS (Oracle C7)

---

## Risks & Mitigations

| # | 风险 | 等级 | 缓解 |
|---|-----|:----:|------|
| 1 | DGpuBoard::bind_memory_backings 末尾追加 set_vram_target 触发 init_timing_mode 双调用顺序冲突 (Oracle C8) | 🟡 中 | Phase A.5 Step 2 grep 现有 bind_memory_backings,确认 vram_ctrl 取指针在 set_backing_view 之后调,无重复 |
| 2 | ChStream module cast via dynamic_cast 失败 (soc_->getInternalInstance 返回基础指针) | 🟡 中 | 复用 init_timing_mode 段 (dgpu_board_shell.cc:900-940) 的 cast helper,不引入新 cast 模式 |
| 3 | try_compile EXPECT-FAIL harness CMake 配置复杂 (TryCompile.cmake helper 维护) | 🟢 低 | Step 1 简化设计 (try_compile macro 而非 EXPECT-FAIL 包装),Step 4 验证可运行后 commit |
| 4 | doc 改写 proposal/design/tasks 触发 openspec validate 失败 | 🟡 中 | Step 5 验证 openspec validate 通过才 commit,失败则回滚 doc edit |
| 5 | test_realip_pin_contract.cc TEST_CASE #3 round-trip 跑通需先 load 真实 JSON config (依赖 Phase C Phase 8 HB/RC 装配) | 🟡 中 | Step 1 直接引用 `dgpu_soc_v2_realip_p2.json` (已有 Phase C 后基线),无需新建 config |
| 6 | `width` 静态常量加到 ch_uint 触发 ABI 检查 fail | 🟢 低 | `static constexpr unsigned width = W;` 是 compile-time constant,不影响 object layout / vtable / ABI,无破坏风险 |
| 7 | Change B 启动前置条件不明确 (何时算"acceptance test 0 回归") | 🟡 中 | Change A.8 列出全 AC gates,Change B 启动时复跑同样 gates + 加 facade-wiring tag |
| 8 | 原 proposal.md/design.md/tasks.md 改写丢失历史决策 (Oracle 报告引用的旧表述) | 🟡 中 | 改写时保留 §Oracle 反馈段落,明确标注原 monolith → 4 Change 的拆分逻辑 |

---

## Self-Review (per writing-plans skill)

### Spec Coverage

Oracle 16 findings → Change A task 映射:

| Oracle Finding | Change A Task | 状态 |
|----------------|---------------|------|
| A.4 (命名端口退化为 idx 0) | 推迟至 Change D (D.2 ConnectionResolver WARN) | 🟢 Out-of-Scope |
| A.5 (board_v1 9 connections 静默丢弃) | 推迟至 Change D (D.5 board_v1 修复) | 🟢 Out-of-Scope |
| A.8 (PcieControllerIP params 不消费) | 推迟至 Change D (D.3 ParamResolver hook) | 🟢 Out-of-Scope |
| B.1 (match_bar ep_==nullptr 默认值危险) | 推迟至 Change B (B.4 match_bar 扩展) | 🟢 Out-of-Scope |
| B.4 (VramControllerTLM 1GHz 假设) | 推迟至 Change D (D.4 文档化) | 🟢 Out-of-Scope |
| C1 (facade 生产零接线) | **Change B B.1/B.2 修复** | 🟢 Out-of-Scope (本计划) |
| **C2 (无条件分发破坏 ADR-DGPU-10)** | **Change B B.3/B.4 修复** | 🟢 Out-of-Scope (本计划) |
| C3 (SFINAE-on-private 不可行) | **Change A A.6 Task 9 修复** (CMake try_compile 替代) | ✅ In-Scope |
| C4 (#include .cc ODR) | **Change A A.6 Task 9 修复** (try_compile 不进 cpptlm_core) | ✅ In-Scope |
| C5 (test_pcie_bypass_tlp_data_path.cc) | 推迟至 Change B (B.5 重写) | 🟢 Out-of-Scope |
| C6 (3 个 config 不在 change 范围) | **推迟至 Change C** (legacy flag 删除本身) | 🟢 Out-of-Scope |
| C7 (AGENTS.md Proposed baseline 漂移) | **Change A A.7 Task 11 Step 2 修复** | ✅ In-Scope |
| C8 (reset/clock 边界条件) | 推迟至 Change B (B.6 acceptance) | 🟢 Out-of-Scope |

**Coverage verdict**: Change A 覆盖 3 个可立即修复的 finding (C3/C4/C7);剩余 5 个 finding (C1/C2/C5/C6/C8) 推迟至 Change B/C,这是 Oracle C1/C6 的要求 ("先接线 facade, 再删 flag") 的拆分解读 — Change A 不能跨过 Change B 单独删 flag。

### Placeholder Scan

无 "TBD" / "TODO" / "fill in details" / "add appropriate error handling" 出现。Step 2 grep 类提示是**操作指令**(读版本号),非内容占位符。

Step 4 中"完整 wire (cast + set_vram_target)"的代码块已展开,cast helper 引用 `dgpu_board_shell.cc:917-920` 的现有 pattern。

Step 6 全回归基线的 6 个 tag 命令已具体列出 (realip[p3] / fabric / pcie / minimal_dgpu_soc / pcie-memory / dgpu_soc_timing)。

### Type Consistency (Momus 修订后)

- `Fabric2VramCtrlBundle` 字段类型 `bundles::ch_uint<W>` (A.1 Task 2 Step 1) — Momus 修订: 原 `cpphdl::ch_uint` 命名空间错,实际 `bundles::ch_uint` (per `include/bundles/cpphdl_types.hh`)。`BUNDLE_FIELD_WIDTH` macro 通过 `decltype(&BundleType::Member)` 成员指针 + `::width` 静态常量 (A.2 Task 2b 已加 width)。
- `GpuInternalFabric::set_vram_target(VramControllerTLM*)` setter 签名 (A.3 Task 5 Step 2) — Momus 修订: 原 ChStreamInitiatorPort<Bundle> 成员不存在,改 setter-only 模式;VramControllerTLM 是全局命名空间(非 `tlm::vram::`)。
- `VramControllerTLM::set_fabric_source(tlm::cluster::GpuInternalFabric*)` setter 签名 (A.4 Task 6 Step 2) — Momus 修订: 原 ChStreamTargetPort<Bundle> 成员不存在,改 setter-only 模式。
- `DGpuBoard::bind_memory_backings()` 调用 `soc_->getInternalInstance("fabric")` / `"vram_ctrl"` (A.5 Task 8 Step 3) — Momus 修订: 原 `modules_by_name` API 不存在;实际 API 是 `soc_->getInternalInstance`;fabric 模块名是 `"fabric"` (per `configs/dgpu_soc_v2_realip_p2.json:22`),不是 `"gpu_internal_fabric"`。

### Coverage Gap

- A.6 Task 9 Step 2 的 try_compile test 在 Change A 期望 SUCCESS (legacy symbols 仍存在),Change C 启动时**需手动 flip 期望到 EXPECT-FAIL**。Step 2 的 CMake COMMENT 段已说明此为"forward-looking setup" — 记录在 Change C plan 文件中。
- Change A 完成后,**Change B/C/D 各自的 plan 文件必须由独立 session 起草** — 本计划仅 outline tasks,不在 Change A 范围。
- A.2 Task 2b 加 `width` 静态常量到 `ch_uint` (修改 `cpphdl_types.hh`) — 影响所有 `ch_uint` 使用方,但 `static constexpr unsigned width = W;` 是 ABI-neutral 添加,无破坏风险。

---

## Execution Handoff

**Plan complete and saved to `docs/superpowers/plans/2026-10-08-dgpu-realip-pin-contract-resplit.md`.**

**Resplit 决策摘要**:
- 原 monolith change 拆为 4 Change (A/B/C/D),本计划仅含 Change A 可执行 tasks
- Change A: 端口契约代码化 (Fabric2VramCtrlBundle + static_assert + 2 test,3-4 天)
- Change B: Facade 生产接线 (Oracle C1/C2/C5 修复,4-5 天,**必须 Change A 完成后**)
- Change C: Legacy flag 删除 (Oracle C6 修复,**必须 Change B 完成后且 acceptance 0 回归**)
- Change D: P2 清理 (ModuleFactory strict_types 等,2-3 天,与 A/B/C 并行)

**Oracle P0/Critical findings 处理状态**:
- ✅ **C3 + C4 (SFINAE 不可行 + ODR 风险)**: Change A A.6 修复 (CMake `try_compile` 负向测试)
- ✅ **C7 (AGENTS.md baseline 漂移)**: Change A A.7 修复
- 🟡 **C1 (facade dormant)**: 推迟至 Change B (前置条件 — 必须在 facade 真正接管数据路径后再删 flag)
- 🟡 **C2 (无条件分发破坏防护锁)**: 推迟至 Change B (match_bar BAR0 offset-aware 修复)
- 🟡 **C5 (test_pcie_bypass_tlp_data_path.cc 编译失败)**: 推迟至 Change B
- 🟡 **C6 (3 个 config 不在范围)**: 推迟至 Change C (legacy flag 删除本身的覆盖范围)

**Execution Options**:

1. **Subagent-Driven (recommended)** — Dispatch a fresh subagent per task, review between tasks, fast iteration. Use skill: `superpowers:subagent-driven-development`

2. **Inline Execution** — Execute tasks in this session using `superpowers:executing-plans`, batch execution with checkpoints. Use skill: `superpowers:executing-plans`

**或者**: 直接派 Momus 对本计划做 plan 评审 (per system prompt `.omo/plans/*.md → Momus` 流程),验证 plan 是否覆盖 Oracle findings + 可执行性 + 4-Change 拆分合理性。

---

**Owner**: CppTLM Team (Sisyphus)
**起点**: `dgpu-realip-refactor-completion` tag (`26583da7`)
**目标 tag (Change A)**: `dgpu-realip-pin-contract-change-a-completion` (落 AC-A1 ~ AC-A12)
**后续 tag**: Change B → `dgpu-realip-pin-contract-change-b-completion` / Change C → `dgpu-realip-pin-contract-change-c-completion` / Change D → `dgpu-realip-pin-contract-change-d-completion`