# dGPU Real-IP Pin-Contract Change C — Legacy Flag Retirement Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Change B 完成（facade 生产接线, Strangler gate observation-only）基础上, 删除 DGpuBoard 的 4 个 legacy routing flag + PciePath enum + `attach_profile()` 死代码, 把 mmio_read/write + backdoor 共 **11 处** flag-keyed 分支迁移到 `match_bar` verdict, 并退役 JSON 顶层 `routing_enabled` 字段。同时重写 **9 个**引用 flag API 的测试文件（防止 C.3 删 API 后编译断裂）。

**Architecture:** "**dispatch migration FIRST → delete flags → remove gate**" 三段式 (per Oracle `bg_a3f78cd9` C.2 顺序反转 + Metis `bg_fd20b1ed` 3 硬伤修订, 2026-10-08):
- **C.0 (pre-flight)**: 修 pre-existing `test_minimal_soc_driver_visible_e2e.cc` **3 处 BAR 断言** (143/148/154, per `pcie_endpoint_ip.cc:116-124` `install_bar_registers` 对全部 6 个 BAR dword 写 0) + AGENTS.md baseline 实测修正
- **C.1 (核心, 分发迁移)**: mmio_read/write + backdoor **11 处** flag-keyed 分支 key 到 `match_bar` verdict; 保留 4 条非 facade 语义 (power gate / doorbell / target-presence / GMMU 子窗+互斥); 新增 **null-controller fallback** (AC-C2.5) 防止无 controller 时所有路由丢失
- **C.2 (终态)**: 删除 Strangler gate + expected mirror (`std::abort` 路径); 保留 DEBUG-only assert 防止 ep_ 不对称静默漂移; spec.md + architecture.md §2.4.2 同 commit 同步重写 (修复 spec.md:62-68 vs G-C6 "==0" vs "==2" 矛盾)
- **C.3~C.7**: 删 4 flag + PciePath + attach_profile → JSON 退役 (同序列) → 重写 9 个测试文件 → try_compile WILL_FAIL → no_legacy_flags ACTIVE → mmio_regs_ defer 到 Change D

**Tech Stack:** C++17, Catch2 v3.7.0, `[realip][facade-wiring]` characterization suite (Change B 已交付), `switch (match_bar verdict)` 分发, CTest `WILL_FAIL`, 源码 grep 断言 (per Oracle C3 REJECT 勿复活 SFINAE)。

**前置:** `dgpu-realip-pin-contract-change-b-completion` tag (2026-10-08, G-B1~G-B11 全绿)。
**后续:** Change D (P2 Cleanup) — mmio_regs_ 清理 (条件性) + ModuleFactory strict_types + bandwidth 字段正名。

---

## File Structure (Change C locked by this plan — Metis `bg_fd20b1ed` 修订版)

### 修改文件 — 生产代码 (src/ include/ configs/)

| 文件 | Phase | 改动 |
|------|:-----:|------|
| `src/tlm/gpu/dgpu_board_shell.cc` | C.1, C.2, C.3 | (C.1) **11 处** flag-keyed 分支迁移到 `switch (match_bar verdict)` + null-controller fallback; (C.2) 删 Strangler gate + expected mirror, 保留 DEBUG-only assert; (C.3) 删 4 `routing_enabled_*_` + `pcie_path_` + `attach_profile` 引用 |
| `include/tlm/gpu/dgpu_board_shell.hh` | C.3 | 删 `PciePath` enum + `pcie_path_` + `pcie_path()` + `attach_profile()` + 4 `routing_enabled_*_` 成员 + 4 setter + 4 accessor (**不是 5+5+5**) |
| `src/tlm/gpu/dgpu_board_shell.cc` (load_soc_config) | C.3, C.4 | (C.3) 删 4 `routing_enabled` JSON 加载; (C.4) 加一次性 deprecation WARN |
| `configs/dgpu_soc_minimal_v1.json` | C.4 | 删 `display/storage/gmmu/memory_routing_enabled` 顶层字段 + description 文本更新 |
| `configs/dgpu_soc_timing_v1.json` | C.4 | 删 `memory/storage/gmmu_routing_enabled` 顶层字段 |
| `examples/dgpu_soc_with_pcie_ip.json` | C.4 | 删 `display/memory_routing_enabled` + `_display_routing_comment`/`_memory_routing_comment` |

### 修改文件 — 测试 (test/) — **9 个** (Metis `bg_fd20b1ed` B1: Plan 原列 2 个,补 7 个)

| 文件 | Phase | 改动 |
|------|:-----:|------|
| `test/test_dgpu_board_no_legacy_flags.cc` | C.6 | 整文件重写 — 当前第二 case 调 setter/pcie_path(),C.3 后**不编译**;改为 4 routing_enabled + PciePath 全删断言 + ctest grep gate |
| `test/test_minimal_soc_driver_visible_e2e.cc` | C.0 + C.3 | C.0: 3 处 BAR 断言 (143/148/154) 改 spec-compliant `lo==0,hi==0`,size 走 `PcieEndpointIP::bar_size()`; C.3: `:97-99` accessor 断言删 |
| `test/test_a3_mmio_power_gate.cc` | C.3 | `:108,126` `set_display_routing_enabled(true)` 改用 D3 power gate 直接构造 (无 facade 控制器 + `is_mmio_gated()` 返 -EIO) |
| `test/test_dgpu_board_framebuffer.cc` | C.3 | `:53-97` (5 处 setter+accessor) 改用 `attach_vram_for_testing` + 直接 framebuffer_ptr 验证 |
| `test/test_dgpu_pcie_device_perspective.cc` | C.3 | `:93,107,143` `set_display_routing_enabled(false)` 改用 PcieDisplayDevice attach/detach 切换 |
| `test/test_minimal_dgpu_soc_characterization.cc` | C.3 | `:52,87,97-99` accessor 断言改用 `has_*_device()` (PcieEndpointIP 已有) |
| `test/test_minimal_dgpu_soc_e2e.cc` | C.3 | `:147-149` accessor 断言删 |
| `test/test_pcie_memory_device_routing_characterization.cc` | C.3 | `:32` `memory_routing_enabled() == false` 改用 PcieMemoryDevice attach 切换 |
| `test/test_pcie_memory_device_routing_flag.cc` | C.3 | 整文件 11-29 行测 flag → 重写测 "PcieMemoryDevice attach/detach 决定 BAR2 路由" (root cause 4 防劫持语义) |

### 修改文件 — 文档 (docs/ openspec/ AGENTS.md)

| 文件 | Phase | 改动 |
|------|:-----:|------|
| `AGENTS.md` | C.0 | baseline 实测修正 (实测数字,不是估的 79401/1694); 删 "memory_routing_enabled_ 路由开关 (D2 v1.1 修订)" + "framebuffer 自动分配" KEY INVARIANTS 条目 (flag 删后过时) |
| `docs/designs/dgpu-soc/architecture.md` §2.4.2 | C.2 | 同 commit 同步重写 — gate 删除状态 + 分发迁移文档 + null-controller fallback 描述 |
| `openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md` | C.2 | **同 commit 同步重写** (不是 C.3!) — 修复 spec.md:62-68 `grep == 2` 与 G-C6 `== 0` 矛盾 + spec.md:89-105 "Strangler gate 保留 std::abort 保护" 场景过时; 加 Change B facade wiring 终态描述 |
| `docs/superpowers/plans/2026-10-08-dgpu-realip-pin-contract-resplit.md` | — | §Change C 节替换 (done 2026-10-08) |

### 新文件

| 文件 | Phase | 职责 |
|------|:-----:|------|
| `test/try_compile_legacy_flags_removed.cc` | C.5 | 已存在 (scripts/test/ 或 test/), 接入 CTest WILL_FAIL — 验证引用 4 routing_enabled + PciePath + attach_profile 的 TU 编译失败 |
| `docs/superpowers/plans/2026-10-08-dgpu-realip-pin-contract-change-c-legacy-flag-retirement.md` | — | 本 plan 文件 (新建) |

### Frozen surface (0 修改, Change C scope)

- `include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h` + `include/tlm/gpu/pcie_display_device.hh` + `include/bundles/pcie_bundles_tlm.hh` — 0 diff (G-C8)
- `PcieControllerIP::match_bar` 签名 / `BarTarget` enum — 0 变化 (whole-BAR 锁定, 继承 G-B7)
- 23 ABI — 0 新增
- `controller_ip_` / `set_pcie_controller_ip()` / `controller_ip()` 三件套 — **保留** (P2 PR-A facade 命脉, C 阶段分发依赖)

### 新文件

| 文件 | Phase | 职责 |
|------|:-----:|------|
| `test/try_compile_legacy_flags_removed.cc` | C.5 | 已存在 (scripts/test/ 或 test/), 接入 CTest WILL_FAIL — 验证引用 `display_routing_enabled_` 的 TU 编译失败 |
| `docs/superpowers/plans/2026-10-08-dgpu-realip-pin-contract-change-c-legacy-flag-retirement.md` | — | 本 plan 文件 (新建) |

### Frozen surface (0 修改, Change C scope)

- `include/abi/cpptlm_emulator.h` + `include/tlm/gpu/pcie_endpoint_tlm.h` + `include/tlm/gpu/pcie_display_device.hh` + `include/bundles/pcie_bundles_tlm.hh` — 0 diff (G-C8)
- `PcieControllerIP::match_bar` 签名 / `BarTarget` enum — 0 变化 (whole-BAR 锁定, 继承 G-B7)
- 23 ABI — 0 新增

---

## Architecture Decisions (Change C — Metis `bg_fd20b1ed` 修订版)

### AC-C1: 分发迁移必须先于 flag 删除 + 11 处 taxonomy 校正

**Oracle bg_a3f78cd9 核心发现 + Metis `bg_fd20b1ed` A1 校正**: 当前分发 **11 处** flag-keyed 分支 (Plan 原写 "9 处" 错数;列了 11 个行号但总述写 9)。修正 taxonomy:

| 路径 | flag-keyed 分支数 | 行号 |
|------|:----------------:|------|
| `mmio_read` | **3** | 366 (display) / 375 (storage) / 384 (memory) |
| `mmio_write` | **4** (不是 3) | 485 (display) / 496 (gmmu) / 508 (memory) / 563 (storage) |
| `backdoor_read` | **2** (不是 3) | 613 (display) / 621 (memory) — 634 storage 已 presence-based,作迁移模板 |
| `backdoor_write` | **2** (不是 3) | 669 (display) / 677 (memory) — 689 storage 已 presence-based |
| **合计** | **11** | (删 flag 会抽掉 gating 输入 → 编译即断,**必须 C.1 先迁移**) |

**决议**: C.1 把 11 处分支改为 `switch (ctrl->match_bar(...)) + null-controller fallback`:
```cpp
BarTarget target = BarTarget::Unknown;
uint64_t sub_off = 0;
const bool matched = controller_ip_ && controller_ip_->is_ep_attached()
    ? controller_ip_->match_bar(bar, offset, target, sub_off) : false;
// null-controller fallback (AC-C2.5): 用 mirror 逻辑 (facade verdict 的朴素计算)
// 注: 即使有 controller 但 ep_ 未 attach, match_bar(bar1) 恒 Unknown → 路由丢失,需 guard
switch (target) {
    case BarTarget::Bar0Mmio:
        if (ep->has_display_device()) { ... }     // display first (现状)
        else if (gmmu_ && offset < 0x100 && len >= 4) { ... }  // GMMU sub-window (AC-C2.4)
        break;
    case BarTarget::Bar1FbFastPath:
        if (framebuffer_ptr_) { ... }              // storage presence-based
        break;
    case BarTarget::Bar2VramAperture:
        if (ep->has_memory_device()) { ... }
        break;
    default: // Unknown (doorbell / OOB / no ctrl)
        // 仍需 mmio_regs_ store + inject_q_ push + doorbell handler (B.2)
        break;
}
```

### AC-C2: 保留 5 条非 facade 语义 (Metis bg_fd20b1ed A7 扩展)

**决议**: C.1 迁移时**必须保留**:
1. **INV-A D3 power gate 最前** (D1 v1.1.1 不变量, `is_mmio_gated()` 先检查;**backdoor 不查 power gate**, 仅 mmio_read/write 查)
2. **kBar1DoorbellOffset 排除** (板级合成偏移 0x10010000, 非 facade 职责 — G-B6 已 pin;**保留板级 handler 不动**, Change C 不修 doorbell 偏移)
3. **target-presence 谓词**: `has_display_device()` / `has_memory_device()` / `framebuffer_ptr_` / `gmmu_` / `soc_` (PcieEndpointIP 已有方法,无需新增)
4. **GMMU write 子窗 + display-first 优先级**: `offset<0x100 && len>=4`; display 命中即 return (现状保留); gmmu **不 return** (设置 regs 后 fall-through 到 mmio_regs_ + inject_q_); gmmu/display 互斥由 config 级保证 (从不共存),但显式注释
5. **🆕 null-controller fallback (Metis `bg_fd20b1ed` A3)**: `controller_ip_ == nullptr` 或 `controller_ip_->is_ep_attached() == false` 时,用 mirror 朴素计算 (bar==0→Bar0Mmio / bar==1→offset<bar_size(1)?FbFastPath:Unknown / bar==2→Bar2VramAperture / else Unknown) 作 fallback,**不静默丢所有路由** (否则 `test_a3_mmio_power_gate` `dgpu_mini_cfg()`、`test_pcie_bypass_tlp_data_path` 等无 controller 测试全 FAIL)

### AC-C3: Strangler gate 终态删除 + DEBUG-only assert 保留

**决议** (Metis `bg_fd20b1ed` A6 修订): 原计划说 "C.1 后 mirror 与 match_bar 同源 → gate 变 vacuous" **事实错误** — mirror (`dgpu_board_shell.cc:343-352, 456-464`) 是**独立** bar/offset + `pcie_ep()->bar_size(1)` 朴素计算,**不是 match_bar 输出** (match_bar 用 controller 的 `ep_->bar_size(1)`, mirror 用 board 的 `pcie_ep()`)。即使 C.1 后, controller `ep_` 与 board `pcie_ep()` 不对称时 mirror 与 match_bar 仍可能分歧。删 gate = 删 runtime invariant pin。

**修订**: C.2 删除 `std::abort` 路径与 expected mirror 计算,但**保留** DEBUG-only assert (DCHECK 或 NDEBUG-conditional `assert`):
- `DEBUG-only assert(target == naive_classify(bar, offset, pcie_ep()))` — 仅 debug 构建检查,Release 不开销
- 回归锁 = B.3' `[realip][facade-wiring]` characterization suite 8/8 (测试时 pin)
- **C.6 gate 字符串目标从 "=2" 改为 "=0"** (release 字符串删除)
- **DEBUG 构建**保留 "Strangler gate" 字符串(用于 assert 触发时的错误消息)

### AC-C4: 路由语义替换 (flag → presence) + 等价矩阵验证

**决议**: flag 删除后, "是否启用某路由" 由 **config 派生的 target 存在性** 决定:
- display 路由 = `ep->has_display_device()`
- memory 路由 = `ep->has_memory_device()` (但 with_pcie_ip.json `memory_routing_enabled=false` 防劫持 root cause 4 → presence 开启 = hijack 风险, **必须明确** with_pcie_ip 的 memory device 不 attach 或保留 fallback gate)
- storage 路由 = `framebuffer_ptr_ != nullptr`
- gmmu 路由 = `gmmu_ != nullptr`

**等价矩阵验证 (Metis `bg_fd20b1ed` A4)** — 必须逐个验证:

| config | flag | device-present | 旧行为 (flag) | 新行为 (presence) | 等价? |
|--------|------|:--------------:|----------------|-------------------|:-----:|
| minimal_v1 | `display_routing_enabled=false` | display 未 attach | display 不路由 | display 不路由 (presence = false) | ✅ |
| minimal_v1 | `memory_routing_enabled=true` | memory device attach | memory 路由 | memory 路由 | ✅ |
| timing_v1 | `gmmu_routing_enabled=true` | gmmu attach | gmmu 路由 | gmmu 路由 | ✅ |
| with_pcie_ip | `display_routing_enabled=true` | display attach | display 路由 | display 路由 | ✅ |
| **with_pcie_ip** | `memory_routing_enabled=false` | **memory device attach?** | **memory 不路由** (防劫持) | **memory 路由** (hijack 风险) | ⚠️ **不等价** |

**with_pcie_ip 行为反转风险**: 若 memory device 在 EP 中 attach (per `bind_memory_backings`),presence-based 开启 BAR2 → PcieMemoryDevice 路由,**撕开 ADR-DGPU-10 防劫持 root cause 4 锁**。

**处理**: (a) **C.1.5 新增 Task** — 验证 with_pcie_ip 的 PcieEndpointIP 不 attach memory device (或在 attach_composition 时检查); 或 (b) **引入 config-level `routing_profile`** 替代 flag 语义,presence 仅在 profile 启用时生效。**默认 (a)**,验证失败则降级 (b)。

### AC-C5: JSON 退役与 flag 删除同序列

**决议**: C.4 与 C.3 **同一提交序列** — 否则 loader 对残留 `*_routing_enabled` 字段静默忽略 = silent drop。loader 加一次性 deprecation WARN (非静默)。

### AC-C6: 测试覆盖用源码 grep, 勿复活 SFINAE

**决议**: C.6 用**源码 grep 断言** (`grep -rE "routing_enabled|pcie_path_|PciePath|attach_profile" include/ src/ test/ | wc -l` = 0) 覆盖 4 routing flag + PciePath + attach_profile。原 SFINAE 探测 (`std::is_same_v<...>`) 已被 Oracle `bg_14b6dd83` C3 REJECT (因 chstream 端口非模板类无法探测), **勿复活**。

### AC-C7: mmio_regs_ 决策式 defer (不再 "条件性")

**决议** (Metis `bg_fd20b1ed` A10 修订): `dgpu_board_shell.cc:544` doorbell handler 从 `mmio_regs_` 读回 doorbell 负载再转发 SDMA;`test_dgpu_board_doorbell_routing.cc` / `test_pcie_board_routing_characterization.cc` / `test_pcie_bypass_tlp_data_path.cc:95` / `test_pcie_display_device_e2e.cc:124` 共 4+ 文件依赖。**结论**: mmio_regs_ **不能硬删**,defer 到 Change D。C.7 改为 "验证 doorbell 依赖 + 文档化 deferral" — 不在 Change C 触碰 mmio_regs_。

---

## Acceptance Criteria (Change C DoD — 8 gates per Oracle bg_a3f78cd9 + Metis bg_fd20b1ed 修订)

- [ ] **G-C1**: `openspec validate dgpu-realip-pin-contract --strict` PASS (含 spec.md:62-68/89-105/114 同步重写消除 ==2/==0 矛盾)
- [ ] **G-C2**: `grep -rE "routing_enabled|pcie_path_|PciePath|attach_profile" include/ src/ test/` = 0 (排除 docs/archive,覆盖 4 routing flag + PciePath + attach_profile)
- [ ] **G-C3**: G-B4 六标签 0 回归复跑 — **实测数**: C.3 重写 9 个测试文件后,各 tag case 数会变;commit 时实测并记录实际数(不再锁 12/69+37/12024+...+24/69+5/21 硬值)
- [ ] **G-C4**: try_compile WILL_FAIL ctest PASS (引用 4 `routing_enabled_*_` + `pcie_path_` + `attach_profile` 任一的 TU 编译失败 = ctest PASS via WILL_FAIL)
- [ ] **G-C5**: `test_dgpu_board_no_legacy_flags` ACTIVE 5-API 覆盖 PASS (4 routing flag + PciePath,grep 断言;原 XFAIL 第 2 case 已重写) — `[realip][pin-contract]` 实测 case 数
- [ ] **G-C6**: `strings build/bin/cpptlm_tests | grep -c "Strangler gate"` = **0** (Release 字符串删除) + `[realip][facade-wiring]` characterization suite 8/8 绿 + DEBUG-only assert 保留 (防止 controller `ep_` vs board `pcie_ep()` 不对称静默漂移,per AC-C3 Metis 修订)
- [ ] **G-C7**: `[driver_visible]` 2/2 PASS (`test_minimal_soc_driver_visible_e2e:143+148+154` 全 6 个 BAR dword 改 spec-compliant `lo==0,hi==0`,size 走 `PcieEndpointIP::bar_size()` accessor) + AGENTS.md baseline 实测更新
- [ ] **G-C8**: 4 frozen header 0 diff + 23 ABI 0 new (继承 G-B10); + `controller_ip_`/`set_pcie_controller_ip`/`controller_ip()` 三件套保留 (P2 PR-A facade 命脉)

---

## Phases / Tasks

### Phase C.0: Pre-flight (Quick — 修 Change B 遗留 e2e + baseline 实测)

**Refs:** Oracle `bg_a3f78cd9` Part 1 note + Part 3 C.0 + Metis `bg_fd20b1ed` A2/B9 (e2e 修不全)+ B8 (baseline 实测)

**Files:**
- Modify: `test/test_minimal_soc_driver_visible_e2e.cc` (3 处 BAR 断言: `:143, :148, :154`; +`:97-99` accessor 断言待 C.3 处理)
- Modify: `AGENTS.md` (baseline 实测数 + 删 2 条过时 KEY INVARIANTS)

#### Task 0.1: 修 e2e 3 处 BAR 断言 (spec-compliant 全 6 dword)

- [ ] **Step 1: 读当前断言 + 确认 install_bar_registers 行为**

```bash
sed -n '135,160p' test/test_minimal_soc_driver_visible_e2e.cc
# 当前: BAR0 :143 lo==4096 / BAR1 :148 lo==16777216 / BAR2 :154 lo==0,hi==2 — 3 处全错
# 期望: per pcie_endpoint_ip.cc:116-124 install_bar_registers 对全部 6 个 BAR dword 写 0
grep -n "install_bar_registers\|cfg_pf.write" src/tlm/pcie/pcie_endpoint_ip.cc | head -10
```

- [ ] **Step 2: 改 3 处断言为 spec-compliant + 用 bar_size() accessor 验证 size**

```cpp
// per pcie_endpoint_ip.cc:116-124: BAR register = 0 (base addr placeholder)
// size 在 bar_sizes_[] 单独字段 (bar_size() accessor, 非 round-trip BAR)
// Oracle bg_a3f78cd9 C.0 + Metis bg_fd20b1ed A2/B9 修订: 全 6 dword 修复
// BAR0 @ 0x10/0x14
REQUIRE(board.pcie_config_read(0x10, 4, &lo) == 0);
REQUIRE(board.pcie_config_read(0x14, 4, &hi) == 0);
REQUIRE(lo == 0u);  // BAR0 base addr placeholder
REQUIRE(hi == 0u);
// size 验证走 PcieEndpointIP::bar_size() accessor (新增 per Change A.7)
REQUIRE(board.pcie_ep()->bar_size(0) == 4096u);  // 或 board.controller_ip()->ep()->bar_size(0)

// BAR1 @ 0x18/0x1C
REQUIRE(board.pcie_config_read(0x18, 4, &lo) == 0);
REQUIRE(board.pcie_config_read(0x1C, 4, &hi) == 0);
REQUIRE(lo == 0u);  // BAR1 base addr placeholder (per install_bar_registers 全 0 写)
REQUIRE(hi == 0u);
REQUIRE(board.pcie_ep()->bar_size(1) == 16777216u);

// BAR2 @ 0x20/0x24
REQUIRE(board.pcie_config_read(0x20, 4, &lo) == 0);
REQUIRE(board.pcie_config_read(0x24, 4, &hi) == 0);
REQUIRE(lo == 0u);  // BAR2 base addr placeholder (per install_bar_registers 全 0 写)
REQUIRE(hi == 0u);  // 旧测试 hi==2 是 64-bit 双 dword 编码 (size>>32), spec-compliant 应 0
REQUIRE(board.pcie_ep()->bar_size(2) == 8589934592ULL);
```

- [ ] **Step 3: 编译 + 跑 `[driver_visible]` → 2/2 PASS**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[driver_visible]" 2>&1 | tail -3
# 期望: 2 cases PASS (G-C7)
```

- [ ] **Step 4: 实测 baseline (非手算,per Metis B8)**

```bash
./build/bin/cpptlm_tests > /tmp/cpptlm_full.log 2>&1
echo "Exit code: $?"
grep "test cases:" /tmp/cpptlm_full.log | tail -1
# 记录实际数 (Change B 后 baseline)
# 当前预期: 1694 cases / 79401 assertions (per Change B 完成态)
# Change C 后实际数会变 (C.0 不删测试, 仅修断言 — 数字应不变)
```

- [ ] **Step 5: 更新 AGENTS.md baseline (实测数) + 删 2 条过时 KEY INVARIANTS**

```markdown
- **测试状态**: **实测值 (C.0 Step 4 实测, 不是估的)** test cases 全绿 (含 Change B facade-wiring + [driver_visible] 修复后; Change C 完成时再实测更新)
```

**同时删 2 条过时 KEY INVARIANTS** (C.3 删 flag 后过时):
- 删 `memory_routing_enabled_ 路由开关 (D2 v1.1 修订, 2027-09-26)` 条目 — flag 退役
- 删 `framebuffer 自动分配` 条目中的 "memory_routing_enabled=true" 提及 (per AC-C4 presence 语义)

- [ ] **Step 6: Commit**

```bash
git add test/test_minimal_soc_driver_visible_e2e.cc AGENTS.md
git commit -m "fix(e2e): align 3 BAR config reads to spec-compliant placeholder + baseline (C.0)"
```

---

### Phase C.1: 分发迁移 (核心 — **11 处** flag-keyed 分支 key 到 match_bar + null-controller fallback)

**Refs:** Oracle `bg_a3f78cd9` Part 3 C.1 + Metis `bg_fd20b1ed` A1/A3 + AC-C1/AC-C2

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc`

**11 处分支 taxonomy (Metis A1 校正,Plan 原 "9 处" 错数)**:
- mmio_read 3 处: 366 (display) / 375 (storage) / 384 (memory)
- mmio_write **4** 处 (不是 3): 485 (display) / 496 (gmmu) / 508 (memory) / 563 (storage)
- backdoor_read **2** 处 (不是 3; storage @ 634 已 presence-based): 613 (display) / 621 (memory)
- backdoor_write **2** 处 (不是 3; storage @ 689 已 presence-based): 669 (display) / 677 (memory)

#### Task 1.1: mmio_read 3 处分支迁移 (366/375/384)

- [ ] **Step 1: 读当前 mmio_read 分发** (`:360-420`)

```bash
sed -n '360,420p' src/tlm/gpu/dgpu_board_shell.cc
# 当前 3 处 flag-keyed: display/storage/memory
```

- [ ] **Step 2: 改为 switch (match_bar verdict) + null-controller fallback (Task 1.4 抽出的 helper)**

```cpp
// C.1 (per Oracle bg_a3f78cd9 + Metis bg_fd20b1ed A3): 
// dispatch keyed to match_bar verdict, NOT legacy flags
// null-controller fallback (AC-C2.5) via mirror logic
BarTarget target = BarTarget::Unknown;
uint64_t sub_off = 0;
if (controller_ip_ && controller_ip_->is_ep_attached()) {
    controller_ip_->match_bar(bar, offset, target, sub_off);
} else {
    // Fallback: naive bar/offset classification (mirror of Strangler gate expected)
    classify_naive(bar, offset, pcie_ep(), target);  // Task 1.4 helper
}
switch (target) {
    case BarTarget::Bar0Mmio: {
        // display first (现状保留, gmmu 不与之共存于同一 config — AGENTS.md 不变量)
        if (ep->has_display_device()) { /* display path */ return; }
        // GMMU sub-window (no return — fall-through 写 mmio_regs_)
        if (gmmu_ && offset < 0x100 && len >= 4) { gmmu_->set(*(uint32_t*)(buf+sub_off)); /* no return */ }
        break;
    }
    case BarTarget::Bar1FbFastPath:
        if (framebuffer_ptr_ && offset < framebuffer_size_) { memcpy(framebuffer_ptr_+offset, buf, len); return; }
        break;
    case BarTarget::Bar2VramAperture:
        if (ep->has_memory_device()) { ep->memory_device().memory_write(offset, buf, len); return; }
        break;
    default: // Unknown (doorbell / OOB / no ctrl / no match)
        // 仍需 mmio_regs_ store + inject_q_ push (保留 roundtrip 语义)
        break;
}
```

- [ ] **Step 3: 保留 INV-A power gate 最前 (:321 `is_mmio_gated()`) + doorbell 排除 (Unknown 路径) + presence 谓词**

- [ ] **Step 4: 编译 + 跑 G-B4 六标签 0 回归**

```bash
cmake --build build -j$(nproc)
for tag in "[minimal_dgpu_soc]" "[dgpu_soc_timing]" "[pcie]" "[realip][p3]" "[pcie-memory]" "[realip][pin-contract]"; do
  echo "$tag: $(./build/bin/cpptlm_tests "$tag" 2>&1 | tail -2 | head -1)"
done
```

- [ ] **Step 5: 跑 characterization suite**

```bash
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -3
# 期望: 8/8 PASS (行为逐位一致)
```

- [ ] **Step 6: Commit (per-branch, 可独立 revert per Metis F38)**

```bash
git add src/tlm/gpu/dgpu_board_shell.cc
git commit -m "refactor(facade): migrate mmio_read 3 branches to match_bar verdict (C.1.1)"
```

#### Task 1.2: mmio_write 4 处分支迁移 (485/496/508/563)

- [ ] **Step 1: 读当前 mmio_write 分发** (`:480-570`)

```bash
sed -n '480,570p' src/tlm/gpu/dgpu_board_shell.cc
# 当前 4 处 flag-keyed: display/gmmu/memory/storage (原 Task 1.2 错写 3 处)
```

- [ ] **Step 2: 同样 Task 1.1 switch 模式 + GMMU 子窗保留 (`offset<0x100 && len>=4` + gmmu **不 return** + display-first)**

- [ ] **Step 3: 编译 + G-B4 回归 + characterization suite**

- [ ] **Step 4: Commit**

```bash
git commit -m "refactor(facade): migrate mmio_write 4 branches to match_bar verdict (C.1.2)"
```

#### Task 1.3: backdoor 4 处分支迁移 (613/621/669/677)

- [ ] **Step 1: 读 backdoor 路径** (`:605-680`)

```bash
sed -n '605,680p' src/tlm/gpu/dgpu_board_shell.cc
# 当前 4 处 flag-keyed: display/memory × read/write
# storage @ 634/689 已 presence-based (framebuffer_ptr_ && framebuffer_size_) — 迁移模板
# 注意: backdoor 不查 is_mmio_gated() (与 mmio_read/write 不同)
```

- [ ] **Step 2: 同样 Task 1.1 switch 模式 + 复用 634/689 storage 模板 (presence-based, 不需 match_bar)**

- [ ] **Step 3: 编译 + 全回归 (含 `[a3]` power gate + `[pcie-memory]` 防劫持 + backdoor ABI smoke)**

- [ ] **Step 4: Commit**

```bash
git commit -m "refactor(facade): migrate backdoor 4 branches to match_bar verdict (C.1.3)"
```

#### Task 1.4 (🆕 Metis A3): null-controller fallback helper `classify_naive()`

- [ ] **Step 1: 抽 helper 函数** (从 Strangler gate mirror 343-352/456-464 抽出,作为 facade 缺失时的朴素分类)

```cpp
// per Metis bg_fd20b1ed A3: 无 controller_ip_ 或 controller->is_ep_attached()==false 时,
// match_bar 不可用, 路由不能静默丢. 抽 helper 用 board 的 pcie_ep() 做朴素 bar/offset 分类.
static void classify_naive(uint8_t bar, uint64_t offset,
                          PcieEndpointIP* ep,
                          BarTarget& out_target) {
    if (bar == 0) out_target = BarTarget::Bar0Mmio;
    else if (bar == 1) {
        const uint64_t sz = ep ? ep->bar_size(1) : 0;
        out_target = offset < sz ? BarTarget::Bar1FbFastPath : BarTarget::Unknown;
    } else if (bar == 2) {
        out_target = BarTarget::Bar2VramAperture;
    } else {
        out_target = BarTarget::Unknown;
    }
}
```

- [ ] **Step 2: 在 Task 1.1/1.2/1.3 的 switch 前调用 (替代 inline 计算)**

- [ ] **Step 3: 编译 + 跑无 controller 测试 (`test_a3_mmio_power_gate` `dgpu_mini_cfg()` + `test_pcie_bypass_tlp_data_path`)**

```bash
./build/bin/cpptlm_tests "[a3]" 2>&1 | tail -3   # power gate 仍触发 -EIO
./build/bin/cpptlm_tests "[pcie-bypass-tlp]" 2>&1 | tail -3   # data path 行为不变
```

- [ ] **Step 4: Commit**

```bash
git commit -m "refactor(facade): add classify_naive() helper for null-controller fallback (C.1.4)"
```

#### Task 1.4 (🆕 Metis A3): null-controller fallback helper `classify_naive()`

- [ ] **Step 1: 抽 helper 函数** (从 Strangler gate mirror 343-352/456-464 抽出,作为 facade 缺失时的朴素分类)

```cpp
// per Metis bg_fd20b1ed A3: 无 controller_ip_ 或 controller->is_ep_attached()==false 时,
// match_bar 不可用, 路由不能静默丢. 抽 helper 用 board 的 pcie_ep() 做朴素 bar/offset 分类.
static void classify_naive(uint8_t bar, uint64_t offset,
                          PcieEndpointIP* ep,
                          BarTarget& out_target) {
    if (bar == 0) out_target = BarTarget::Bar0Mmio;
    else if (bar == 1) {
        const uint64_t sz = ep ? ep->bar_size(1) : 0;
        out_target = offset < sz ? BarTarget::Bar1FbFastPath : BarTarget::Unknown;
    } else if (bar == 2) {
        out_target = BarTarget::Bar2VramAperture;
    } else {
        out_target = BarTarget::Unknown;
    }
}
```

- [ ] **Step 2: 在 Task 1.1/1.2/1.3 的 switch 前调用 (替代 inline 计算)**

- [ ] **Step 3: 编译 + 跑无 controller 测试 (`test_a3_mmio_power_gate` `dgpu_mini_cfg()` + `test_pcie_bypass_tlp_data_path`)**

```bash
./build/bin/cpptlm_tests "[a3]" 2>&1 | tail -3   # power gate 仍触发 -EIO
./build/bin/cpptlm_tests "[pcie-bypass-tlp]" 2>&1 | tail -3   # data path 行为不变
```

- [ ] **Step 4: Commit**

```bash
git commit -m "refactor(facade): add classify_naive() helper for null-controller fallback (C.1.4)"
```

---

### Phase C.2: 删除 Strangler gate + expected mirror + DEBUG-only assert + spec.md 同步 (终态)

**Refs:** Oracle `bg_a3f78cd9` Part 3 C.2 + Metis `bg_fd20b1ed` A6/B5/B10 + AC-C3

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc` (删 gate + mirror, 保留 DEBUG-only assert)
- Modify: `docs/designs/dgpu-soc/architecture.md` §2.4.2 (同 commit)
- Modify: `openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md` (同 commit — Metis B10)

#### Task 2.1: 删 gate + mirror + 加 DEBUG-only assert (Metis A6/B5)

- [ ] **Step 1: 找 Strangler gate 2 处 + abort 路径** (`:337-361, :440-465`)

```bash
grep -n "Strangler gate\|std::abort\|expected\|facade verdict" src/tlm/gpu/dgpu_board_shell.cc | head -20
```

- [ ] **Step 2: 删除 gate 分支 (controller_ip_ 检查 + match_bar 比对 + abort), 加 DEBUG-only assert 保留 runtime invariant (Metis B5)**

```cpp
// C.2 (per Metis bg_fd20b1ed A6/B5): mirror 与 match_bar 是独立计算源 (mirror 用 board 的 pcie_ep(),
// match_bar 用 controller 的 ep_), 删 gate = 删 runtime invariant pin. 保留 DEBUG-only assert:
//   1. Release build (NDEBUG): assert 消失, gate 字符串计数 = 0
//   2. Debug build: assert 检查 controller->ep_ vs board->pcie_ep() 一致性 (防止 ep_ 不对称静默漂移)

#ifndef NDEBUG
    if (controller_ip_ && controller_ip_->is_ep_attached()) {
        BarTarget facade_verdict = BarTarget::Unknown;
        uint64_t dummy = 0;
        controller_ip_->match_bar(bar, offset, facade_verdict, dummy);
        BarTarget naive_verdict = BarTarget::Unknown;
        classify_naive(bar, offset, pcie_ep(), naive_verdict);
        assert(facade_verdict == naive_verdict && "Strangler gate: facade verdict diverges from naive classification (ep_ asymmetry?)");
    }
#endif
    // (注意: 仅 debug 构建, release 无字符串 + 无开销)
```

- [ ] **Step 3: 编译 + 全回归 (debug + release 都验)**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)
./build/bin/cpptlm_tests 2>&1 | tail -3
# 期望: 全 PASS (debug 也有 DEBUG assert 仍 work)
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
./build/bin/cpptlm_tests 2>&1 | tail -3
# 期望: 全 PASS
```

- [ ] **Step 4: G-C6 验证 gate 字符串 = 0 (Release) + characterization 8/8 绿 (两种 build)**

```bash
# Release build
strings build/bin/cpptlm_tests | grep -c "Strangler gate"  # 期望: 0
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -3  # 8/8 绿
# Debug build (assert 字符串保留, 期望 > 0)
strings build/bin/cpptlm_tests | grep -c "Strangler gate"  # 期望: > 0
```

- [ ] **Step 5: 更新 architecture.md §2.4.2 (同 commit — Metis B10: spec.md 矛盾 + 文档同步)**

```markdown
### 2.4.2 Facade Production Wiring (Change B + Change C, 2026-10-08)

Per `openspec/.../spec.md`:

| Source | Dst | Action |
|--------|-----|--------|
| `DGpuBoard::init()` 公共路径 | `PcieControllerIP::attach_endpoint(ep)` | 程序化注入 EP 引用 |
| `DGpuBoard::init()` | `DGpuBoard::set_pcie_controller_ip(ctrl)` | 激活 facade |

**Dispatch (Change C)**: mmio_read/write + backdoor 共 11 处 flag-keyed 分支 key 到 `match_bar` verdict,
保留 4 条非 facade 语义 (power gate / doorbell / target-presence / GMMU 子窗+互斥)。
null-controller fallback 通过 `classify_naive()` helper 实现 (mirror 抽出)。

**Strangler gate (Change C 终态)**: Release build 删除 `std::abort` 路径 + expected mirror 计算;
Debug build 保留 `assert(facade_verdict == naive_verdict)` 防止 controller `ep_` 不对称静默漂移。
Release 二进制 `strings | grep "Strangler gate"` = 0 (G-C6)。
```

- [ ] **Step 6: spec.md 同 commit 同步重写** (Metis B10: 修复 spec.md:62-68 vs G-C6 "==0" 矛盾)

```bash
# 1. spec.md:62-68 scenario "Strangler gate 字符串仍存在" 改写:
#    改 "Strangler gate 字符串仍存在" → "Strangler gate 删除 (Release build 字符串=0); 
#    回归锁由 [realip][facade-wiring] characterization suite 8/8 承接"

# 2. spec.md:89-105 scenario "Strangler gate 保留作 facade verdict 与 expected mirror 一致性的 std::abort 保护" 
#    改写为 "Strangler gate 在 Change C 删除; DEBUG-only assert 保留 invariant pin; 
#    回归锁: characterization suite 行为逐位一致"

# 3. spec.md:114 "Strangler gate 保留 (per ADR-DGPU-10)" 改写为 "Strangler gate 删除 (Change C 终态); 
#    controller->ep_ 与 board->pcie_ep() 一致性由 DEBUG-only assert 保护"
```

- [ ] **Step 7: Commit (同 commit 含 spec.md 同步, 避免 docs-code 不一致窗口)**

```bash
git add src/tlm/gpu/dgpu_board_shell.cc \
        docs/designs/dgpu-soc/architecture.md \
        openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md
git commit -m "refactor(facade): remove Strangler gate (Release) + DEBUG-only assert + spec sync (C.2)"
```

---

### Phase C.3: 删 4 flag + PciePath enum + attach_profile + **重写 9 个测试文件** (原 C.1)

**Refs:** Oracle `bg_a3f78cd9` Part 3 C.3 + Metis `bg_fd20b1ed` B1/A5/A8/A9 + AC-C4

**Files:**
- Modify: `include/tlm/gpu/dgpu_board_shell.hh` (删 API 声明)
- Modify: `src/tlm/gpu/dgpu_board_shell.cc` (删实现 + 调用点)
- Modify: 9 个测试文件 (Metis B1 — 防止编译断裂)

**实际删除清单 (Metis A5 校正)**: 不是 "5 flag + PciePath" 而是:
- 4 `routing_enabled_*_` 成员: `display_routing_enabled_` / `storage_routing_enabled_` / `gmmu_routing_enabled_` / `memory_routing_enabled_`
- 4 setter (per flag)
- 4 accessor (per flag)
- `PciePath` enum + `pcie_path_` 成员 + `pcie_path()` accessor + **`attach_profile()` 方法 (死代码, grep 全仓零调用方, 必同步删)**

#### Task 3.0 (🆕 pre-C.3): 重写 9 个测试文件防止编译断裂 (Metis B1)

> **重要**: 这些重写必须**先于 C.3 主体** (或与 C.3 同 commit),否则 C.3 删 API 后断编译。

- [ ] **Step 1: 识别全部 flag API 调用点**

```bash
grep -rn "set_display_routing_enabled\|set_storage_routing_enabled\|set_gmmu_routing_enabled\|set_memory_routing_enabled\|display_routing_enabled()\|storage_routing_enabled()\|gmmu_routing_enabled()\|memory_routing_enabled()\|pcie_path()\|pcie_path_\|attach_profile\|PciePath::" test/ src/ | head -40
```

- [ ] **Step 2: 重写 9 个测试文件** (presence 语义 + D3 power gate 直接构造)

| # | 文件 | 改写策略 |
|---|------|----------|
| 1 | `test_a3_mmio_power_gate.cc` | `:108,126` `set_display_routing_enabled(true)` → 不设 flag;直接测 D3 power gate (构造 board + `is_mmio_gated()` 返 -EIO) |
| 2 | `test_dgpu_board_framebuffer.cc` | `:53-97` (5 处 setter+accessor) → 用 `attach_vram_for_testing` + 直接 `framebuffer_ptr_` 验证 (不再用 flag 启用 storage) |
| 3 | `test_dgpu_pcie_device_perspective.cc` | `:93,107,143` `set_display_routing_enabled(false)` → PcieDisplayDevice attach/detach 切换 presence |
| 4 | `test_minimal_dgpu_soc_characterization.cc` | `:52,87,97-99` accessor 断言 → `has_*_device()` (PcieEndpointIP 已有方法) |
| 5 | `test_minimal_dgpu_soc_e2e.cc` | `:147-149` accessor 断言 → 直接验证 device 行为 |
| 6 | `test_minimal_soc_driver_visible_e2e.cc` | `:97-99` accessor 断言 (C.0 已修 :143/:148/:154) → 删 |
| 7 | `test_pcie_memory_device_routing_characterization.cc` | `:32` `memory_routing_enabled() == false` → PcieMemoryDevice attach 切换 (root cause 4 防劫持语义保留) |
| 8 | `test_pcie_memory_device_routing_flag.cc` | 整文件 11-29 行测 flag → 重写测 "PcieMemoryDevice attach/detach 决定 BAR2 路由" (root cause 4 防劫持语义) |
| 9 | `test_dgpu_board_no_legacy_flags.cc` | 整文件重写 — 当前第 2 case 调 setter/pcie_path, C.3 后**不编译**;改为 4 routing_enabled + PciePath + attach_profile 全删断言 (CTest grep gate) |

- [ ] **Step 3: 编译验证 9 文件不依赖 flag API**

```bash
cmake --build build -j$(nproc)
# 期望: 编译通过 (9 文件已重写)
```

- [ ] **Step 4: 跑原 flag API 测试套件 (应全 PASS) + G-C3 [pcie-memory] 复跑**

```bash
for tag in "[a3]" "[pcie]" "[pcie-memory]" "[realip][pin-contract]"; do
  echo "$tag: $(./build/bin/cpptlm_tests "$tag" 2>&1 | tail -2 | head -1)"
done
# 期望: 全 PASS (新 presence 语义 + 新测试)
```

- [ ] **Step 5: Commit (per-file 或聚合, 推荐聚合 1 commit 便于回滚)**

```bash
git add test/test_a3_mmio_power_gate.cc \
        test/test_dgpu_board_framebuffer.cc \
        test/test_dgpu_pcie_device_perspective.cc \
        test/test_minimal_dgpu_soc_characterization.cc \
        test/test_minimal_dgpu_soc_e2e.cc \
        test/test_minimal_soc_driver_visible_e2e.cc \
        test/test_pcie_memory_device_routing_characterization.cc \
        test/test_pcie_memory_device_routing_flag.cc \
        test/test_dgpu_board_no_legacy_flags.cc
git commit -m "test(legacy): rewrite 9 files for flag/PciePath/attach_profile removal (C.3.0)"
```

#### Task 3.1: 删头文件声明 (4 routing flag + PciePath + attach_profile)

- [ ] **Step 1: grep 找所有声明**

```bash
grep -n "PciePath\|routing_enabled_\|attach_profile\|pcie_path_" include/tlm/gpu/dgpu_board_shell.hh | head -20
```

- [ ] **Step 2: 删声明 (1 个 enum + 4 成员 + 4 setter + 4 accessor + 1 pcie_path_ + 1 pcie_path() + 1 attach_profile)**

- [ ] **Step 3: 删实现 + 调用点 (`:31-42` `attach_profile` 定义, `:83-104` JSON 加载, 各分支引用)**

- [ ] **Step 4: G-C2 验证 (4 routing + PciePath + attach_profile 全 0)**

```bash
grep -rE "routing_enabled|pcie_path_|PciePath|attach_profile" include/ src/ | wc -l  # 期望: 0
```

- [ ] **Step 5: 编译 + G-B4 复跑 + [pcie-memory] (presence 语义等价验证)**

```bash
cmake --build build -j$(nproc)
for tag in "[minimal_dgpu_soc]" "[dgpu_soc_timing]" "[pcie]" "[realip][p3]" "[pcie-memory]" "[realip][pin-contract]"; do
  echo "$tag: $(./build/bin/cpptlm_tests "$tag" 2>&1 | tail -2 | head -1)"
done
# 期望: 全 PASS (presence 语义 + 9 文件重写后)
```

- [ ] **Step 6: Commit**

```bash
git add include/tlm/gpu/dgpu_board_shell.hh src/tlm/gpu/dgpu_board_shell.cc
git commit -m "refactor(facade): delete 4 routing flags + PciePath + attach_profile (C.3.1)"
```

---

### Phase C.4: JSON 字段退役 (原 C.3, 与 C.3 同序列) + with_pcie_ip memory device attach 验证 (AC-C4 行为反转风险)

**Refs:** Oracle `bg_a3f78cd9` Part 3 C.4 + Metis `bg_fd20b1ed` A4 (with_pcie_ip memory=false 行为反转) + AC-C5

**Files:**
- Modify: `configs/dgpu_soc_minimal_v1.json` / `dgpu_soc_timing_v1.json` / `examples/dgpu_soc_with_pcie_ip.json`
- Modify: `src/tlm/gpu/dgpu_board_shell.cc` (loader deprecation WARN)

#### Task 4.0 (🆕 Metis A4): 验证 with_pcie_ip 不 attach memory device (防 hijack 风险回归)

- [ ] **Step 1: grep 验证 with_pcie_ip.json 不实例化 PcieMemoryDevice**

```bash
grep -i "pcie_memory\|PcieMemoryDevice" examples/dgpu_soc_with_pcie_ip.json
# 期望: 0 hits (with_pcie_ip 无 PcieMemoryDevice 实例, presence-based memory 路由不会开启 → hijack 风险 0)
# 若命中 → 风险: C.1/C.3 实施后 BAR2 → PcieMemoryDevice 自动路由,撕开 ADR-DGPU-10 root cause 4
# 解决: 移除 PcieMemoryDevice 节点 或 引入 config-level routing_profile gate
```

- [ ] **Step 2: 记录结论到 commit message**

#### Task 4.1: 删 JSON 字段 + loader WARN

- [ ] **Step 1: 删 3 个 JSON 顶层 `*_routing_enabled` 字段** (minimal_v1: display/storage/gmmu/memory; timing_v1: memory/storage/gmmu; with_pcie_ip: display/memory + comment 字段)

- [ ] **Step 2: loader 加一次性 deprecation WARN** (field 存在时 DPRINTF 非静默)

- [ ] **Step 3: 编译 + validateConfig + G-B4 回归 + description 文本更新**

- [ ] **Step 4: Commit** (与 C.3 同提交序列或紧随)

```bash
git commit -m "refactor(config): retire routing_enabled JSON fields + deprecation WARN (C.4)"
```

---

### Phase C.5: try_compile EXPECT-FAIL 接入 CTest (原 C.4) — 扩到 4+PciePath+attach_profile

**Refs:** Oracle `bg_a3f78cd9` Part 3 C.5 + Metis `bg_fd20b1ed` B3 (harness 未接) + AC-C6

**Files:**
- Modify: `test/CMakeLists.txt` (或 scripts/test/)

#### Task 5.1: 接入 CTest WILL_FAIL — 覆盖 4 routing_enabled + PciePath + attach_profile

- [ ] **Step 1: 找已写好的 probe** (`scripts/test/try_compile_legacy_flags_removed.cc` 或 test/)

- [ ] **Step 2: 验证 probe 内容覆盖 6 个 API (不只是 4 routing flag)**

```bash
grep -E "display_routing_enabled|memory_routing_enabled|storage_routing_enabled|gmmu_routing_enabled|pcie_path|attach_profile|PciePath" scripts/test/try_compile_legacy_flags_removed.cc
# 期望: 6 个 API 都有引用
# 若缺 → 补 probe
```

- [ ] **Step 3: CMake 加 WILL_FAIL ctest (Metis B3: 从零建 harness, probe 必须先验 well-formed)**

```cmake
# 先验: 临时启 will_fail=false 跑一次, 确认 probe 编译成功(well-formed)
add_executable(try_compile_legacy_flags_removed scripts/test/try_compile_legacy_flags_removed.cc)
target_link_libraries(try_compile_legacy_flags_removed cpptlm_core)

# 然后设 will_fail=true (probe 在 C.3 删 API 后会编译失败, WILL_FAIL = PASS)
add_test(NAME try_compile_legacy_flags_removed
         COMMAND ${CMAKE_COMMAND} --build . --target try_compile_legacy_flags_removed)
set_tests_properties(try_compile_legacy_flags_removed PROPERTIES WILL_FAIL TRUE)
```

- [ ] **Step 4: 验证 G-C4** (先 well-formed, 后 WILL_FAIL)

```bash
# Phase A: 验证 well-formed (在 C.3 删 API 前)
cmake --build build --target try_compile_legacy_flags_removed 2>&1 | tail -3
# 期望: 编译成功 (因为 API 还在)
ctest --test-dir build --output-on-failure -R try_compile_legacy_flags_removed
# 期望: FAIL (此时 WILL_FAIL=true 但编译成功, ctest 视为失败)

# Phase B: C.3 删 API 后, 重跑
cmake --build build --target try_compile_legacy_flags_removed 2>&1 | tail -3
# 期望: 编译失败 (API 已删)
ctest --test-dir build --output-on-failure -R try_compile_legacy_flags_removed
# 期望: PASS (编译失败 = ctest PASS via WILL_FAIL)
```

- [ ] **Step 5: Commit**

```bash
git add test/CMakeLists.txt scripts/test/try_compile_legacy_flags_removed.cc
git commit -m "test(try-compile): wire WILL_FAIL ctest for 4+PciePath+attach_profile (C.5)"
```

---

### Phase C.6: no_legacy_flags XFAIL→ACTIVE (原 C.5) — 覆盖 4 routing + PciePath + attach_profile

**Refs:** Oracle `bg_a3f78cd9` Part 3 C.6 + Metis `bg_fd20b1ed` B4 (整文件重写, 不是 "activate") + AC-C6

**Files:**
- Modify: `test/test_dgpu_board_no_legacy_flags.cc` (整文件重写)

#### Task 6.1: 整文件重写 (C.3 已重写一遍, 本任务是终态 ACTIVE 验证)

- [ ] **Step 1: 读 C.3 Task 3.0 Step 2 重写后的 test_dgpu_board_no_legacy_flags.cc**

```bash
grep -n "WARN\|SUCCEED\|XFAIL\|routing_enabled\|PciePath\|attach_profile\|cTest" test/test_dgpu_board_no_legacy_flags.cc
```

- [ ] **Step 2: 改 ACTIVE — 加 5-API 覆盖 (4 routing + PciePath) + ctest grep gate**

```cpp
// C.6 ACTIVE 终态 (per Metis B4): source-grep assertion for 5 API names
// (SFINAE 探测已被 Oracle bg_14b6dd83 C3 REJECT — chstream 端口非模板类无法探测)
// 注意: 当前 TU 编译成功即"自身 API 已删" (因为上一 commit C.3 已删), Catch2 test 只做 grep
//   string assertion (4 routing + PciePath 全不在 dgpu_board_shell.hh 源码)

TEST_CASE("DGpuBoard: 4 routing_enabled + PciePath retired (C.6 ACTIVE)", "[realip][pin-contract]") {
    // 自检: 本 TU 编译成功 = API 已删
    SUCCEED("compile success = API deleted");
}
```

**配合 ctest grep gate** (在 `test/CMakeLists.txt` 加):

```cmake
# Source-grep gate: 验证 5 API name 在 dgpu_board_shell.hh 源码 = 0 命中
add_test(NAME grep_no_legacy_api_in_board_header
         COMMAND sh -c "! grep -E 'routing_enabled_|pcie_path_|attach_profile|PciePath' ${CMAKE_SOURCE_DIR}/include/tlm/gpu/dgpu_board_shell.hh")
```

- [ ] **Step 3: G-C5 验证**

```bash
./build/bin/cpptlm_tests "[realip][pin-contract]" 2>&1 | tail -3
# 期望: 5 cases PASS (3 from realip_pin_contract + 1 from no_legacy_flags ACTIVE + 1 from grep gate)
ctest --test-dir build --output-on-failure -R "realip|pin-contract|no_legacy|grep_no_legacy"
```

- [ ] **Step 4: Commit**

```bash
git add test/test_dgpu_board_no_legacy_flags.cc test/CMakeLists.txt
git commit -m "test(no-legacy): ACTIVE 5-API + ctest grep gate (C.6)"
```

---

### Phase C.7: mmio_regs_ defer 决策 (决策式, 非条件性)

**Refs:** Oracle `bg_a3f78cd9` Part 3 C.7 + Metis `bg_fd20b1ed` A10 + AC-C7

**Files:**
- (无源文件改动 — 仅决策记录)

#### Task 7.1: 决策记录 + 文档化 deferral

- [ ] **Step 1: 验证 doorbell handler 依赖 mmio_regs_**

```bash
grep -A 3 "kBar1DoorbellOffset" src/tlm/gpu/dgpu_board_shell.cc | head -10
# 期望: :544 doorbell handler 从 mmio_regs_ 读回 doorbell 负载
# 结论: mmio_regs_ 不能硬删, doorbell 链路依赖

grep -rn "mmio_regs_\[" src/tlm/pcie/pcie_endpoint_ip.cc | head -5
# 验证 PcieEndpointIP 也依赖 (e.g., doorbell 写入, BAR register 等)
```

- [ ] **Step 2: 验证 4+ 测试文件依赖 mmio_regs_**

```bash
grep -rln "mmio_regs_\|PciePath::Legacy" test/ | head -10
# 期望: test_dgpu_board_doorbell_routing / test_pcie_board_routing_characterization /
#        test_pcie_bypass_tlp_data_path / test_pcie_display_device_e2e 等
```

- [ ] **Step 3: 决策记录**

> **决策**: mmio_regs_ 物理清理不在 Change C 范围 (defer 到 Change D), 因为:
> 1. `dgpu_board_shell.cc:544` doorbell handler 读 `mmio_regs_[...]` 转发 SDMA
> 2. 4+ 测试文件依赖
> 3. Change C 标题"legacy flag retirement"≠ legacy path 完整退役

- [ ] **Step 4: 写入 docs/superpowers/plans/2026-10-08-dgpu-realip-pin-contract-resplit.md §Change D**

```bash
# 不在本 commit, 在 resplit plan §Change D 段加:
#   D.5 (延后自 C.7): mmio_regs_ 物理清理 — doorbell handler 依赖, 需先重构为独立路径
```

---

### Phase C.8: 全 8 AC gates 验证 + tag

**Refs:** Oracle `bg_a3f78cd9` Part 3 G-C1~G-C8 + Metis `bg_fd20b1ed` G-C3/G-C5 实测修订

#### Task 8.1: 跑 8 gates (实测数, 不锁硬值)

- [ ] **G-C1**: `openspec validate dgpu-realip-pin-contract --strict` PASS (含 spec.md 重写)
- [ ] **G-C2**: `grep -rE "routing_enabled|pcie_path_|PciePath|attach_profile" include/ src/ test/ | wc -l` = 0
- [ ] **G-C3**: G-B4 六标签 0 回归复跑 — **实测数** (C.3 重写 9 文件后 case 数会变, commit 时实测)
- [ ] **G-C4**: try_compile WILL_FAIL ctest PASS (per Phase A/B 验证)
- [ ] **G-C5**: `[realip][pin-contract]` 实测 + grep_no_legacy_api_in_board_header ctest PASS
- [ ] **G-C6**: Release build `strings | grep -c "Strangler gate"` = 0 + `[realip][facade-wiring]` 8/8 绿 + DEBUG-only assert 保留
- [ ] **G-C7**: `[driver_visible]` 2/2 PASS + AGENTS.md 实测 baseline 更新
- [ ] **G-C8**: 4 frozen header 0 diff + 23 ABI 0 new + `controller_ip_`/setter/accessor 三件套保留

#### Task 8.2: Apply tag

```bash
git tag -a dgpu-realip-pin-contract-change-c-completion -m "Change C 完成: legacy flag 退役 + dispatch 迁移到 facade verdict + 9 测试文件重写..."
```

---

## Risk & Commit Order (per Oracle bg_a3f78cd9 + Metis bg_fd20b1ed 修订)

| Task | 风险 | 理由 | Commit |
|------|:----:|------|:------:|
| C.0 pre-flight | 🟢 LOW | e2e 3 处 BAR 断言 + baseline 实测 | 1 |
| C.1.1 mmio_read 3 处 | 🟡 MED | switch + null-controller fallback (Task 1.4) | 2 |
| C.1.2 mmio_write 4 处 | 🔴 HIGH | GMMU 不 return + display-first 优先级 | 3 |
| C.1.3 backdoor 4 处 | 🟡 MED | 复用 634/689 storage 模板 (presence-based) | 4 |
| C.1.4 classify_naive() | 🟢 LOW | 抽出 mirror 逻辑, helper function | 5 |
| C.2 删 gate + DEBUG assert | 🟡 MED | 依赖 C.1 完成 + spec.md 同 commit | 6 |
| C.3.0 重写 9 测试文件 | 🔴 HIGH | 防 C.3 删 API 后断编译;AGENTS.md KEY INVARIANTS 同步 | 7-8 (per-file 或聚合) |
| C.3.1 删 4 flag + PciePath + attach_profile | 🟡 MED | 死代码 (grep=0 门禁); 9 文件已重写 | 9 |
| C.4 JSON 退役 + WARN + with_pcie_ip 验证 | 🟡 MED | with_pcie_ip 行为反转风险 (AC-C4) | 10 |
| C.5 try_compile WILL_FAIL | 🟢 LOW | probe 已写好, harness 接入 CTest | 11 |
| C.6 no_legacy_flags ACTIVE + grep gate | 🟢 LOW | 整文件已重写 (C.3.0), 终态 ACTIVE 验证 | 12 |
| C.7 mmio_regs_ defer | 🟢 LOW | 决策式 (doorbell 依赖, defer Change D) | (无 commit) |

**是否拆分独立 change**: 不拆。Change C 是 Change B 的自然终态 (facade verdict 成为 single source of truth 后, legacy flag 是 dead weight)。mmio_regs_ 物理清理如遇 backdoor 消费方, 决策式 defer 到 Change D (C.7 决策), 不在本 change 硬删。

**per-branch commit 策略 (Metis F38)**: C.1 四个 task (1.1/1.2/1.3/1.4) 各自独立 commit, 便于单分支可独立 revert。若 C.1.2 mmio_write (4 处) 失败, 可单独 revert 该 commit 而不丢 C.1.1 进度。

---

## Out-of-Scope (deferred to Change D)

### Change D: P2 Cleanup (parallel, independent)
- D.1: ModuleFactory strict_types=true 模式
- D.2: ConnectionResolver 对未解析命名端口打 WARN
- D.3: ParamResolver hook on PcieControllerIP
- D.4: VramControllerTLM 1GHz 隐含时钟文档化 + **bandwidth 字段正名** (per Oracle bg_a3f78cd9: 32 GB/s@1GHz 实为 32 B/cyc, 字段名误导)
- **D.5 (延后自 C.7): mmio_regs_ 物理清理** — doorbell handler 依赖, 需先重构为独立路径或保留 legacy path 兼容层
- D.6 (条件性, per AC-C4): with_pcie_ip 行为反转风险 mitigation — 若 PcieMemoryDevice attach 验证失败, 引入 config-level `routing_profile` 替代 flag 语义

### D-AXI v1.4 遗留议题 (Change C 不动)
- D1 Display FB: with_pcie_ip.json display=true → presence-based routing = `has_display_device()` (bind_display_backings attach 后 true) → 行为保持
- doorbell 0x10010000: 保持 Unknown + board handler (535-558 独立于 flag), Change C 不动
- MemoryTLM capacity_gb=1: 与 Change C 无关, 不动

### ArchForge 跨仓 review 触发 (per Metis F37)
- Change C 不触发 (§2.4 4-IP 边界表在 Change C 只是文字更新, 无新 IP 边界)
- 触发时机: Change E (D3 seam VramController 插入) 或 P3 RTL team pin-contract 对照实现启动时

---

## 实施时间线 (per Metis bg_fd20b1ed D27 + D40 修订: 4-5d→6d)

| Task | 内容 | 估时 | 风险 |
|:----:|------|:----:|:----:|
| C.0 | e2e 3 处 BAR 断言 + AGENTS.md 实测 baseline | 0.5h | 🟢 |
| C.1.1 | mmio_read 3 处迁移 + null-controller fallback (Task 1.4 helper) | 1.5h | 🟡 |
| C.1.2 | mmio_write 4 处迁移 (GMMU 不 return + display-first) | 2h | 🔴 |
| C.1.3 | backdoor 4 处迁移 (复用 634/689 storage 模板) | 1.5h | 🟡 |
| C.1.4 | classify_naive() helper (mirror 抽出) | 0.5h | 🟢 |
| C.2 | 删 Strangler gate + DEBUG-only assert + architecture.md + spec.md 同 commit | 1h | 🟡 |
| C.3.0 | 重写 9 个测试文件 (防编译断裂) | 3-4h | 🔴 |
| C.3.1 | 删 4 flag + PciePath + attach_profile + AGENTS.md KEY INVARIANTS 删 2 条 | 0.5h | 🟡 |
| C.4 | JSON 退役 + WARN + with_pcie_ip memory device attach 验证 | 1h | 🟡 |
| C.5 | try_compile WILL_FAIL ctest 接入 (含 Phase A/B 验证) | 0.5h | 🟢 |
| C.6 | no_legacy_flags ACTIVE 终态 + ctest grep gate | 0.5h | 🟢 |
| C.7 | mmio_regs_ defer 决策 (无 commit, 仅记录) | 0.25h | 🟢 |
| C.8 | 8 AC gates 验证 + tag | 1h | 🟡 |
| **总计** | — | **~13-14h (~6 天含回归验证 + 9 文件重写 + 8 AC gates 实测)** | |

**per Metis D27 + D40**: 估时系统性乐观 1.5x。4-5 天 → 6 天更现实。**C.1.2 (mmio_write 4 处)** 和 **C.3.0 (9 文件重写)** 是两个最大风险点, 各估 1.5-2h。

---

## Review Log (2026-10-08 pre-implementation)

### Oracle `bg_a3f78cd9` (4m 复评, 2026-10-08)
- Change B 总结: ✅ PASS-WITH-NOTES
- 4-IP 对齐差距: PcieControllerIP whole-BAR ✅; PcieAxiBridgeIP lazy_init ✅; GpuInternalFabric 4-port ✅; VramControllerTLM 简化 ⚠️ (bandwidth 字段名误导)
- Change C 修订: **C.2 顺序反转** (C.1 分发迁移 FIRST → C.2 删 flag → C.3 删 Strangler gate)
- 8 AC gates G-C1~G-C8, 估时 3-4d

### Metis `bg_fd20b1ed` (9m 复评, 2026-10-08, 跨 Oracle 复核)
- 10+10+5+10 详尽 items, ⛔ **halt-pending-revision** (3 硬伤 + 4 次要项)
- **3 硬伤**:
  1. 分支 taxonomy 9→11 (mmio_write 4 / backdoor 4, 11 不是 9); e2e 修 143 不全 (148/154 同样错)
  2. 测试文件冲击清单缺失 (Plan 列 2, 实际 9 个, C.3 删 API 后断编译 7 个未列)
  3. null-controller fallback 未定义 + spec.md vs G-C6 矛盾
- **4 次要项**: AC-C4 presence 等价矩阵; "5 routing flag" 错数 (4+PciePath+attach_profile); baseline 实测; 估时 4-5d→6d
- **10 Oracle 可能遗漏** (F31-F40): 7 条建议回 Oracle 复核

### Metis 修订落地 (本版 plan)
- ✅ 硬伤 1: 11 处 taxonomy 校正 (Phase C.1 注释 + 4 task 明确); e2e C.0 Step 2 扩到 143+148+154 全 6 dword
- ✅ 硬伤 2: File Structure 新增 7 测试文件; Phase C.3.0 Task 重写 9 文件
- ✅ 硬伤 3: AC-C2.5 null-controller fallback; Task 1.4 classify_naive() helper; C.2 spec.md 同 commit 同步; DEBUG-only assert 保留
- ✅ 次要 1: AC-C4 等价矩阵 (with_pcie_ip memory device attach 验证)
- ✅ 次要 2: 删 4 flag + PciePath + attach_profile (不是 5 flag + 5 setter)
- ✅ 次要 3: C.0 Step 4 baseline 实测
- ✅ 次要 4: Timeline 4-5d→6d

---

**Owner**: CppTLM Team (Sisyphus)
**起点**: `dgpu-realip-pin-contract-change-b-completion` tag
**目标 tag (Change C)**: `dgpu-realip-pin-contract-change-c-completion` (落 G-C1 ~ G-C8)
**后续**: Change D (P2 Cleanup: D.1-D.4 + D.5 mmio_regs_ defer + D.6 conditional routing_profile)
**ArchForge 跨仓 review**: 不在 Change C 完成后触发 (无新 IP 边界), 留待 Change E 或 P3 RTL team handoff
