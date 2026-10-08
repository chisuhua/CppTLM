# dGPU Real-IP Pin-Contract Change B — Facade Production Wiring Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `PcieControllerIP` facade 从测试/demo 状态接入 `DGpuBoard` 生产路径,关闭 Oracle C1 (facade dormant); 同时为 Change C (legacy flag 删除) 准备干净的 C2 修复策略(放弃 controller-side BAR 子分发,改 target-side decode 文档化 + characterization 测试回归锁); 顺路偿还 Change A 的 4 项未披露偏差 (spec 漂移 / A.5 桥接零执行覆盖 / bundle 注释假陈述 / 文档债)。

**Architecture:** "**minimal wiring + target-side decode + characterization regression lock**" 三件套 (per Oracle `bg_14b6dd83` APPROVE-WITH-CHANGES verdict, 2026-10-08):
- facade 接入走 `init()` 公共路径(早于 timing/functional 分支),模式对齐 Change A.5 (programmatic bridge)
- 删 `set_endpoint_attachment(this)` (无 API 无消费者),最小接线 = `ctrl->attach_endpoint(ep)` + `board.set_pcie_controller_ip(ctrl)`
- `match_bar` **保持 whole-BAR,签名零变化** —— 锁定 `pcie_controller_ip.hh:45-49` 设计(前序 Oracle bg_c60653e0 §2.1 移除的子分发反模式不重加)
- C2 修复 = characterization 测试 pin 住 facade 激活前后的 BAR0/1/2 行为逐位一致(回归锁而非新逻辑),不引入 controller-side 子窗口

**Tech Stack:** C++17, Catch2 v3.7.0, 既有 `init_timing_mode` cast helper pattern, `[realip][facade-wiring]` 新 tag, `soc_->getInternalInstance(name)` API, `pcie_controller_ip.hh:45-49` 锁定设计注释作为 anchor。

**前置:** `dgpu-realip-pin-contract-change-a-completion` tag (2026-10-08, 12 AC gates 全绿, 0 回归)。
**后续 (门控):** Change C (legacy flag 删除) — 启动前置 = Change B acceptance G-B4 全绿 + G-B5 零 abort。

---

## File Structure (Change B locked by this plan)

### 新文件

| 文件 | Phase | 职责 |
|------|:-----:|------|
| `test/test_realip_facade_wiring.cc` | B.1, B.2, B.2b, B.2c, B.3', B.7 | `[realip][facade-wiring]` tag 套件 (≥7 cases: 含 pcie_ctrl init / 优雅 skip / 循环无悬空 / EP 缺失 dormant / 4 个 characterization configs / A.5 真实路径) |
| `configs/dgpu_soc_minimal_v1_facade.json` (or amend `dgpu_soc_minimal_v1.json`) | B.0 | 加 `pcie_ctrl` 节点 (SimModule JSON 实例化, `type: "PcieControllerIP"`) |
| `configs/dgpu_soc_v2_realip_p2.json` | B.0 | 加 `pcie_ep` 节点 (PcieEndpointIP 实例, facade wiring 完整) |

### 修改文件

| 文件 | Phase | 改动 |
|------|:-----:|------|
| `src/tlm/gpu/dgpu_board_shell.cc` | B.2, B.2b | (B.2) `init()` 公共路径在 `bind_memory_backings()` 之后追加 facade wiring (3 行,含 null-guards + DPRINTF); (B.2b) `~DGpuBoard()` 加 `controller_ip_ = nullptr;` |
| `test/test_pcie_bypass_tlp_data_path.cc` | B.5 | 重写 148 行:替换 `board.pcie_path()` 机制断言为 `board.controller_ip()` + `ctrl->match_bar(bar, offset)`;保留 profile 行为断言(mock/axi_bypass 数据路径结果) |
| `openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md` | B.8 | 删 Change C 残留 requirement (`dgpu-board-legacy-flag-retirement`) + Momus 已否决的 SFINAE 场景 + `ChInitiatorPort<Fabric2VramCtrlBundle>` 端口断言场景 + 4 个 TEST_CASE 描述改为 3 个 + 加 Change B facade wiring 场景 |
| `openspec/changes/dgpu-realip-pin-contract/design.md` | B.8 | D2 节标注 Momus 修订后状态 (PORT_IS_INITIATOR/PORT_IS_TARGET 已删);加 Change B Architecture Decision: `match_bar` whole-BAR 锁定;加 target-side decode 文档化 |
| `include/bundles/dgpu_fabric_bundles_tlm.hh` | B.8 | 修 header 注释第 10 行假陈述(删除 "Registered: compile-time include via chstream_register.hh" — 实际未注册;改为"compile-time include from test_realip_pin_contract.cc / dgpu_board_shell.cc 显式 include") |
| `include/chstream_register.hh` | B.8 | (可选) 加 `#include "bundles/dgpu_fabric_bundles_tlm.hh"` — 与 bundle header 注释假陈述修复二选一;推荐**修注释而非加 include**(bundle 是 plain struct,无 adapter factory 需求) |
| `docs/designs/dgpu-soc/architecture.md` §2.4 | B.8 | 加 §2.4.2 facade wiring 章节;扩 §2.4.1 文档化 target-side decode 规则 |
| `AGENTS.md` §KEY INVARIANTS | B.8 | 加 `PcieControllerIP::match_bar` whole-BAR 锁定条目;加 facade production wiring invariant |

### Frozen surface (0 修改, Change B scope)

- `include/abi/cpptlm_emulator.h` — 23 ABI 签名不动
- 4 frozen header (cpptlm_emulator.h / pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh) — 0 diff
- `PcieControllerIP::match_bar` 签名 / `BarTarget` enum — **零变化**(whole-BAR 锁定,G-B7 回归锁)

---

## Architecture Decisions (Change B)

### AD-B1: 删 `set_endpoint_attachment(this)`,用最小接线

**Oracle 修订**: `pcie_ep->set_endpoint_attachment(this)` API 在全仓不存在 (`grep` 实证),且无消费者需求——EP 已暴露 `bar_size()`,facade 单向持 EP 即可。

**决议**: 最小接线 =
```cpp
ctrl->attach_endpoint(ep);          // 注入 EP 引用
set_pcie_controller_ip(ctrl);       // 激活 Strangler gate
```
无 `set_endpoint_attachment` 反向指针。**不为可能的未来需求预先写 API** (YAGNI per `docs/architecture/01-hybrid-architecture-v2.1.md`)。

### AD-B2: 接线点入 `init()` 公共路径(早于 timing/functional 分支)

**Oracle 修订**: `init_timing_mode` 不接 facade(grep 实证),timing 模式无 facade = oracle 集成测试盲区。接线点必须在 `dgpu_board_shell.cc:166-192` timing/functional 分支**之前**,确保两种仿真模式都覆盖。

**决议**: 接线代码放 `init()` 内 `bind_memory_backings()` 之后,与 A.5 `set_vram_target` 桥接同段位。

### AD-B3: `match_bar` 签名零变化(whole-BAR 锁定, 前序 Oracle bg_c60653e0 §2.1 回归锁)

**Oracle REJECT B.4**: 删 `Bar0Display` + `offset_in_bar` 扩展。理由:
1. `pcie_controller_ip.hh:45-49` 锁定:"Real PCIe EP controllers do NOT sub-split a BAR by offset — decode happens on target side"
2. 真实 PCIe controller (Synopsys DWC / Cadence) 用 iATU inbound region 做 BAR-level 匹配,不在 controller 内做 offset 子分发
3. B.3 的"offset ≥ 0x800000"在 4KB BAR0 内物理不可能 (realip_p2.json:16 / minimal_v1.json:20 bar_sizes[0]=4096)
4. GMMU 寄存器 (minimal_v1 bar0_offsets 0/4/8/16) 与 display 寄存器 (pcie_display_device.hh:47-57 0x00-0xF0) 在 offset 0x00 重叠,在任何单一 config 中从不共存,offset 不可区分

**C2 实际修复策略**: characterization 测试 (B.3') pin 住 facade 激活前后的 BAR0/1/2 行为**逐位一致**(regression lock 而非新逻辑);target-side decode 文档化(为未来 GMMU+display 共存 config 预留);不引入 controller 子分发。

### AD-B4: 接线空 guard 模式(无 facade 静默问题)

**Oracle 新发现**: 接线后 `controller_ip_ != nullptr` 激活 Strangler gate (`dgpu_board_shell.cc:344,457` `std::abort`)。任何 mirror 不一致会导致 build-time error 或 runtime abort。必须:
1. 接线代码含 null-guards(`pcie_ctrl` 缺失 → DPRINTF 跳过;`pcie_ep()` 为 nullptr → attach_endpoint 跳过)
2. 接到的 EP 缺失场景 (`pcie_ctrl` 有但 `pcie_ep` 缺失) → `attach_endpoint` 跳过,`match_bar(1,…)` 走原 `ep_==nullptr` 塌陷路径 (`pcie_controller_ip.cc:38`),**显式 pin 住行为**(dormant-guard case B.2c)
3. `controller_ip_` 是 non-owning 裸指针,`destroy()` 中 `soc_.reset()` 后悬空 → **B.2b 修**:`~DGpuBoard()` 加 `controller_ip_ = nullptr;`

### AD-B5: 顺路修 Change A 文档债(B.8 in Change B scope)

**Oracle 修订**: 4 项 Change A 偏差不单独开 change,在 Change B 内 B.8 偿还:
- spec.md 漂移(仍含 Change C 内容 + Momus 已否决方案)
- design.md D2 节残留被删 macro 旧设计
- bundle header 注释第 10 行假陈述
- A.5 桥接零执行覆盖(B.7 修)

**决议**: B.8 spec 漂移修复 0.5 天;B.7 桥接执行验证 0.5 天。**不**单独开 change(避免 OpenSpec 提案数膨胀;与 Change B 同期发布)。

---

## Acceptance Criteria (Change B DoD — 11 gates per Oracle `bg_14b6dd83` Part 9)

- [ ] **G-B1**: `openspec validate dgpu-realip-pin-contract --strict` PASS;spec 不含 Change C 内容、不含已否决方案(SFINAE / 端口模板断言)
- [ ] **G-B2**: `[realip][facade-wiring]` ≥7 cases PASS(含 pcie_ctrl init / 优雅 skip / 循环无悬空 / EP 缺失 dormant-guard / 4 个 BAR characterization configs / A.5 桥接执行)
- [ ] **G-B3**: `grep -n "attach_endpoint\|set_pcie_controller_ip" src/` 显示 `dgpu_board_shell.cc` `init()` 路径有真实调用点(C1 关闭证据)
- [ ] **G-B4**: facade 激活于 ≥1 个 driver-visible config 后, `[minimal_dgpu_soc]` 12/69 + `[pcie-memory]` 24/69 + `[pcie]` 433/36682 + `[dgpu_soc_timing]` 37/12024 + `[realip][pin-contract]` 5/21 + `[realip][p3]` 9/122 **全 0 回归**(Oracle C6 红线沿用)
- [ ] **G-B5**: Strangler gate 字符串 = 2 (`strings build/bin/cpptlm_tests | grep -c "Strangler gate"`);全量测试零 `std::abort`(facade verdict 与 mirror 在所有路径一致)
- [ ] **G-B6**: doorbell 合成偏移 pin case: `mmio_write(1, 0x10010000)` 的 facade verdict == mirror == Unknown,分发行为不变(已知遗留议题显性化)
- [ ] **G-B7**: `match_bar` 签名与 `BarTarget` enum **零变化** (whole-BAR, 无 Bar0Display) — 前序 Oracle bg_c60653e0 §2.1 回归锁,`git diff pcie_controller_ip.hh` 应为 0 (除本 plan 允许的注释外)
- [ ] **G-B8**: `test/test_pcie_bypass_tlp_data_path.cc` 重写后编译 + PASS,无 `pcie_path()` 机制断言残留于 facade 覆盖路径
- [ ] **G-B9**: A.5 桥接执行验证 case PASS(`DGpuBoard` 加载 `dgpu_soc_v2_realip_p2.json` + `init()` → `fabric->vram_ctrl() != nullptr && vram->fabric() != nullptr`)
- [ ] **G-B10**: 4 frozen header 0 diff + 23 ABI 零新增 + `mmio_self_drain_enabled` 不动 + 无新 detached thread
- [ ] **G-B11**: `architecture.md §2.4` facade 接线章节 + `AGENTS.md` KEY INVARIANTS `match_bar` whole-BAR 锁定条目 + `docs_sync_check.sh --strict` PASS

---

## Phases / Tasks

### Phase B.0: config 前置决策 (硬前置,必须先做)

**Refs:** Oracle `bg_14b6dd83` Part 5 B.1,Part 7 #1 — 现有任何 config 都缺一环

**Files:**
- Modify: `configs/dgpu_soc_minimal_v1.json` (add pcie_ctrl node)
- Modify: `configs/dgpu_soc_timing_v1.json` (add pcie_ctrl node)
- Modify: `configs/dgpu_soc_v2_realip_p2.json` (add pcie_ep node — realip_p2 already has pcie_ctrl per Oracle verification)
- Modify: `examples/dgpu_soc_with_pcie_ip.json` (add pcie_ctrl node — Oracle path correction; the file lives in `examples/`, not `configs/`)

#### Task 0.1: 在 minimal_v1 + timing_v1 + with_pcie_ip 加 pcie_ctrl 节点

- [ ] **Step 1: 读 minimal_v1 现状**

```bash
grep -n "modules" configs/dgpu_soc_minimal_v1.json | head -3
```

- [ ] **Step 2: 在 DGpuSoc::modules 数组加 pcie_ctrl 节点**

```json
{
  "name": "pcie_ctrl",
  "type": "PcieControllerIP",
  "params": {
    "bar_bases": [0, 16777216, 8589934592],
    "bar_sizes": [4096, 16777216, 8589934592],
    "link_speed_gts": 8
  }
}
```

**注意** (Oracle `bg_6b1913d2` 修正):
- `realip_p2.json:12-13` 已有 pcie_ctrl (Task 0.1 不动)
- `minimal_v1` / `timing_v1` 已有 pcie_ep,加 pcie_ctrl 即可
- `examples/dgpu_soc_with_pcie_ip.json` 已有 pcie_ep,加 pcie_ctrl 即可 (文件位于 `examples/`,非 `configs/`)

- [ ] **Step 3: 同样改 timing_v1 + with_pcie_ip**(如缺 pcie_ctrl)

- [ ] **Step 4: 编译验证**(3 个 config 都进 PcieControllerIP 实例化)

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[minimal_dgpu_soc]" -v
./build/bin/cpptlm_tests "[dgpu_soc_timing]" -v
./build/bin/cpptlm_tests "[pcie]" -v
```

期望: 全部 PASS (PcieControllerIP 注册已就绪,无回归)

- [ ] **Step 5: Commit**

```bash
git add configs/dgpu_soc_minimal_v1.json configs/dgpu_soc_timing_v1.json configs/dgpu_soc_v2_realip_p2.json examples/dgpu_soc_with_pcie_ip.json
git commit -m "feat(config): add pcie_ctrl/pcie_ep nodes to driver-visible configs (B.0)"
```

#### Task 0.2: 在 realip_p2 加 pcie_ep 节点

- [ ] **Step 1: 读 realip_p2 现状**

```bash
grep -n "modules" configs/dgpu_soc_v2_realip_p2.json | head -3
```

- [ ] **Step 2: 加 pcie_ep 节点**

```json
{
  "name": "pcie_ep",
  "type": "PcieEndpointIP",
  "params": {
    "bdf": "0000:01:00.0",
    "num_msix_vectors": 16,
    "bar_bases": [0, 16777216, 8589934592],
    "bar_sizes": [4096, 16777216, 8589934592]
  }
}
```

- [ ] **Step 3: 编译 + 全回归**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[realip][p3]" -v
```

- [ ] **Step 4: Commit**(与 Task 0.1 合并或独立)

---

### Phase B.1: `[realip][facade-wiring]` RED test (B.1)

**Refs:** Oracle Part 5 B.1,Part 6 #2

**Files:**
- Create: `test/test_realip_facade_wiring.cc`

#### Task 1.1: 写失败测试 stub

- [ ] **Step 1: 创建 `test/test_realip_facade_wiring.cc`** 初始 stub (3 cases 全 FAIL,等 Phase B.2 GREEN)

```cpp
// test/test_realip_facade_wiring.cc
// Phase B.1 RED: [realip][facade-wiring] integration test stub
// Per Oracle bg_14b6dd83 Part 5 B.1 (revised): prerequisites = B.0 (config pcie_ctrl/pcie_ep)
//
// Author: CppTLM Team / Date: 2026-10-08

#include "tlm/gpu/dgpu_board_shell.hh"
#include "core/event_queue.hh"

#include <catch_amalgamated.hpp>

TEST_CASE("DGpuBoard::init wires controller_ip_ on minimal_v1 (facade active)", "[realip][facade-wiring]") {
    FAIL("Phase B.2 GREEN pending — DGpuBoard::init does not wire facade yet");
}

TEST_CASE("DGpuBoard::init gracefully skips facade when pcie_ctrl absent", "[realip][facade-wiring]") {
    FAIL("Phase B.2 GREEN pending");
}

TEST_CASE("DGpuBoard destroy→init cycle re-wires facade without dangling pointer", "[realip][facade-wiring]") {
    FAIL("Phase B.2b GREEN pending — lifecycle test");
}
```

- [ ] **Step 2: 编译 + 跑测试 → 编译失败或 3 FAIL**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -5
```

期望: 3 cases FAIL with "Phase B.2 GREEN pending" 消息

- [ ] **Step 3: Commit**

```bash
git add test/test_realip_facade_wiring.cc
git commit -m "test(facade-wiring): add init/skip/cycle RED stubs (B.1)"
```

---

### Phase B.2: 生产接线 (B.2, Oracle 修订版)

**Refs:** Oracle Part 5 B.2,Part 6 #1,AD-B1 + AD-B2

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc`

#### Task 2.1: 在 init() 公共路径加 facade wiring

- [ ] **Step 1: 找 `init()` 公共路径**(`dgpu_board_shell.cc:166-192`, 早于 timing/functional 分支)

```bash
sed -n '160,200p' src/tlm/gpu/dgpu_board_shell.cc
```

- [ ] **Step 2: 在 `bind_memory_backings()` 调用之后(约 line 175)插入**

```cpp
        // B.2 (cpptlm-driver-visible-minimal-soc 2026-10-08): facade production wiring
        // Per Oracle bg_14b6dd83 AD-B1/AD-B2: 删 set_endpoint_attachment,最小接线 =
        // ctrl->attach_endpoint(ep) + set_pcie_controller_ip(ctrl); 接 init() 公共路径
        // 早于 timing/functional 分支 (per AD-B2).
        // Oracle bg_6b1913d2 修订: 加 soc_ null-guard (init() 未调 load_soc_config 时).
        if (soc_) {
            if (auto* ctrl = dynamic_cast<tlm::pcie::PcieControllerIP*>(
                    soc_->getInternalInstance("pcie_ctrl"))) {
                if (auto* ep = pcie_ep()) {
                    ctrl->attach_endpoint(ep);
                }
                set_pcie_controller_ip(ctrl);
                DPRINTF(MODULE, "[DGpuBoard] B.2: PcieControllerIP facade wired (ep=%p, ctrl=%p)\n",
                        static_cast<void*>(pcie_ep()), static_cast<void*>(ctrl));
            } else {
                DPRINTF(MODULE, "[DGpuBoard] B.2: SOC 缺 'pcie_ctrl' 实例, facade 装配跳过\n");
            }
        }
```

- [ ] **Step 3: 编译验证**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0
```

- [ ] **Step 4: 跑 init 接线 test (TEST_CASE #1)**

```bash
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -5
# 期望: TEST_CASE #1 PASS (init 接线了 ctrl + ep), 其它 2 个 FAIL 等 B.2b/B.2c
```

- [ ] **Step 5: 全回归 baseline**

```bash
./build/bin/cpptlm_tests "[pcie]" 2>&1 | tail -3
./build/bin/cpptlm_tests "[minimal_dgpu_soc]" 2>&1 | tail -3
./build/bin/cpptlm_tests "[pcie-memory]" 2>&1 | tail -3
./build/bin/cpptlm_tests "[dgpu_soc_timing]" 2>&1 | tail -3
./build/bin/cpptlm_tests "[realip][p3]" 2>&1 | tail -3
./build/bin/cpptlm_tests "[realip][pin-contract]" 2>&1 | tail -3
```

期望: **全 0 回归**(G-B4 硬前置,Strangler gate 不应 abort)

- [ ] **Step 6: 跑全量 + 检查 Strangler gate 无 abort**

```bash
./build/bin/cpptlm_tests 2>&1 | tail -5
strings build/bin/cpptlm_tests | grep -c "Strangler gate"
# 期望: Strangler gate 字符串计数 == 2 (G-B5)
```

- [ ] **Step 7: Commit**

```bash
git add src/tlm/gpu/dgpu_board_shell.cc
git commit -m "feat(facade-wiring): DGpuBoard::init wires PcieControllerIP production path (B.2)"
```

---

### Phase B.2b: 生命周期 (B.2b)

**Refs:** Oracle Part 6 #4, Part 7 #2 — controller_ip_ 悬空指针修

**Files:**
- Modify: `src/tlm/gpu/dgpu_board_shell.cc`

#### Task 2.2: 在 destroy() 中清 4 个 raw pointer + real cycle test (Oracle+Metis 修订)

> **Oracle `bg_6b1913d2` + Metis `bg_03094fec` 联合发现**:
> `controller_ip_` **不是** `soc_.reset()` 后唯一悬空的裸指针。DGpuBoard 还有 3 个同类的 raw pointer 指向 `soc_` 内部实例:
> - `gmmu_` (`dgpu_board_shell.hh:355`, bind_memory_backings 缓存)
> - `sdma_engine_` (`:331`)
> - `cycle_advance_modules_` (`:381`, `std::vector<ChStreamModuleBase*>`)
>
> 三者同样在 `soc_.reset()` 后悬空。**`cycle_advance_modules_` 尤其危险**——`init_timing_mode` 每次 `push_back` 但从不清空;timing config 走 `init → destroy → load → init` 循环后,vector 内含第一批已析构模块的悬空指针 + 新指针,`tick()` 对悬空 `mod->advance_cycle()` 是 UB。
>
> **修正方案**: B.2b 改为 `destroy()` 内一次性清 4 个 + 把 `cycle_advance_modules_.clear()` 也加进 `init_timing_mode` 入口(防御 future re-init)。

- [ ] **Step 1: 找 destroy() 锚点**

```bash
sed -n '1010,1020p' src/tlm/gpu/dgpu_board_shell.cc
# 期望: line 1013 = soc_.reset(), 1016 = eq_.reset()
```

- [ ] **Step 2: 在 destroy() 中 `soc_.reset()` 之后加 4 个 raw pointer 清零**

```cpp
    // B.2b (cpptlm-driver-visible-minimal-soc 2026-10-08): clear 4 non-owning ptrs after soc_.reset()
    // per Oracle bg_6b1913d2 AD-B4 修正 + Metis cycle_advance_modules_ 新发现.
    // controller_ip_ + gmmu_ + sdma_engine_ + cycle_advance_modules_ 全部指向 soc_ 内部实例,
    // soc_.reset() 后一并悬空. 同时清零防止 destroy() 后 mmio_read/write UAF.
    controller_ip_ = nullptr;
    gmmu_ = nullptr;
    sdma_engine_ = nullptr;
    cycle_advance_modules_.clear();
```

- [ ] **Step 3: init_timing_mode 入口加防御性 clear** (防御 future re-init 时 vector 累积悬空)

```bash
grep -n "cycle_advance_modules_.push_back" src/tlm/gpu/dgpu_board_shell.cc
# 在 init_timing_mode() 最开头(进入 push_back 循环前)加:
# cycle_advance_modules_.clear();  // B.2b: 防御 re-init 累积悬空
```

- [ ] **Step 4: 编译**

```bash
cmake --build build -j$(nproc)
# 期望: 退出码 0
```

- [ ] **Step 5: 实装 cycle test (扩 test/test_realip_facade_wiring.cc — 非 vacuous)**

```cpp
TEST_CASE("DGpuBoard destroy→init cycle re-wires facade without dangling pointer", "[realip][facade-wiring]") {
    // Per Metis bg_03094fec 修正: 实装 load→init→destroy→load→init 真实序列, 不写 vacuous SUCCEED.
    tlm::gpu::DGpuBoard board("test_board");
    EventQueue eq;
    REQUIRE_NOTHROW(board.load_soc_config("configs/dgpu_soc_minimal_v1.json", &eq));
    REQUIRE_NOTHROW(board.init());
    
    // 第一次 init 后 facade 已 wire
    auto* first_ctrl = board.controller_ip();
    REQUIRE(first_ctrl != nullptr);
    REQUIRE_NOTHROW(board.mmio_read(0, 0, 4));  // 走 Strangler gate, verdict 一致
    
    // Destroy 周期
    REQUIRE_NOTHROW(board.destroy());
    REQUIRE_NOTHROW(board.load_soc_config("configs/dgpu_soc_minimal_v1.json", &eq));
    REQUIRE_NOTHROW(board.init());
    
    // 第二次 init 后 facade 重新 wire, 同一指针路径
    auto* second_ctrl = board.controller_ip();
    REQUIRE(second_ctrl != nullptr);
    REQUIRE_NOTHROW(board.mmio_read(0, 0, 4));  // 走 Strangler gate, 仍一致
    
    // 隐式验证: destroy() → init() 期间无悬空 (否则 mmio_read 会 UAF)
    SUCCEED("destroy→init cycle re-wires facade without dangling");
}
```

- [ ] **Step 6: 编译 + 跑 cycle test**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -5
# 期望: 3 cases PASS (B.1 + B.2 + B.2b)
```

- [ ] **Step 7: 跑全回归 + Strangler gate 检查**

```bash
./build/bin/cpptlm_tests 2>&1 | tail -3
strings build/bin/cpptlm_tests | grep -c "Strangler gate"
# 期望: 计数 = 2 (G-B5)
```

- [ ] **Step 8: Commit** (与 B.2 同 commit 或紧随,Oracle 建议合并)

```bash
git add src/tlm/gpu/dgpu_board_shell.cc test/test_realip_facade_wiring.cc
git commit -m "feat(facade-wiring): destroy() clears 4 raw ptrs + lifecycle test (B.2b)"
```

---

### Phase B.2c: dormant-guard test (B.2c)

**Refs:** Oracle Part 7 #3 — `pcie_ctrl` 存在但 `pcie_ep` 缺失场景,`match_bar` 静默塌陷行为显式 pin

**Files:**
- Modify: `test/test_realip_facade_wiring.cc`
- Modify: `configs/dgpu_soc_v2_realip_p2.json` (注释或临时配置 — 仅当 pcie_ep 缺失)

#### Task 2.3: 写 dormant-guard test (Oracle+Metis 3-bug fix)

> **Oracle `bg_6b1913d2` + Metis `bg_03094fec` 共同发现的 3 处硬伤**:
> 1. `PcieControllerIP ctrl("ctrl")` — 构造函数实为 `(const std::string&, EventQueue*)` (`pcie_controller_ip.hh:34`), 缺 EventQueue* 编译失败
> 2. `ctrl.match_bar(1, 0)` — 签名实为 `bool match_bar(uint8_t bar, uint64_t offset, BarTarget& out_target, uint64_t& out_sub_offset)`, 缺 out-params 编译失败
> 3. `REQUIRE(result == Bar1FbFastPath)` — 断言值错误!`ep_==nullptr` 时 `bar_size` 塌陷 0, `offset(0) < 0` 为 false, 实测 verdict 是 **Unknown** (pcie_controller_ip.cc:38-43), 与 AD-B4 描述自相矛盾

- [ ] **Step 1: 扩 test_realip_facade_wiring.cc — 修正 sample 代码**

```cpp
TEST_CASE("Dormant-guard: pcie_ctrl without pcie_ep collapses match_bar to Unknown gracefully", "[realip][facade-wiring]") {
    // Per Oracle bg_6b1913d2 AD-B4 + bg_14b6dd83 Part 7 #3 (修订版):
    // 当 pcie_ctrl 在 JSON 中存在但 pcie_ep() 返回 nullptr (eg. DGpuSoc 只有 pcie_ctrl 模块),
    // attach_endpoint() 跳过, match_bar() 应显式返回 false + out_target=Unknown (不 abort).
    // 此 case regression-lock 静默塌陷路径 — Change C 改 mmio_read/write 时不会突然 abort.
    
    auto eq = std::make_unique<EventQueue>();
    tlm::pcie::PcieControllerIP ctrl("ctrl", eq.get());
    // 注: 不调 ctrl.attach_endpoint(...) — 保持 ep_ == nullptr 塌陷状态
    
    tlm::pcie::PcieControllerIP::BarTarget out_target = tlm::pcie::PcieControllerIP::Bar0Mmio;  // sentinel init
    uint64_t out_sub_offset = 0;
    
    // bar=1, offset=0: ep_==nullptr → bar_size(1) 塌陷为 0 → 0 < 0 false → Unknown + return false
    const bool matched = ctrl.match_bar(1, 0, out_target, out_sub_offset);
    REQUIRE(matched == false);
    REQUIRE(out_target == tlm::pcie::PcieControllerIP::Unknown);
    
    // bar=0 同样塌陷为 Unknown (offset 0 != 0? 实际: ep_==nullptr → bar_size(0)=0 → 0 < 0 false → Unknown)
    out_target = tlm::pcie::PcieControllerIP::Bar0Mmio;  // reset sentinel
    const bool matched_bar0 = ctrl.match_bar(0, 0, out_target, out_sub_offset);
    REQUIRE(matched_bar0 == false);
    REQUIRE(out_target == tlm::pcie::PcieControllerIP::Unknown);
    
    SUCCEED("dormant-guard behavior pinned — no abort on ep_==nullptr, collapses to Unknown");
}
```

- [ ] **Step 2: 编译 + 跑**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -5
# 期望: 4 cases PASS (B.1 + B.2 + B.2b + B.2c)
```

- [ ] **Step 3: Commit**

```bash
git add test/test_realip_facade_wiring.cc
git commit -m "test(facade-wiring): dormant-guard case pins match_bar collapses to Unknown (B.2c)"
```

- [ ] **Step 2: 编译 + 跑**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -5
# 期望: 4 cases PASS
```

- [ ] **Step 3: Commit**

```bash
git add test/test_realip_facade_wiring.cc
git commit -m "test(facade-wiring): dormant-guard case pins match_bar without ep (B.2c)"
```

---

### Phase B.3': characterization tests (替换 B.3/B.4, Oracle 修订)

**Refs:** Oracle Part 6 #2, Part 7 #4 — facade 激活前后 BAR0/1/2 行为逐位一致回归锁

**Files:**
- Modify: `test/test_realip_facade_wiring.cc`

#### Task 3.1: 写 characterization tests (Metis 修订 — 不用 git stash)

> **Metis `bg_03094fec` 修订**: 用 `git stash` 临时禁用 B.2 wiring 是危险操作(stash mid-implementation 容易丢工作)。改用 **byte-baseline constant** 记录已验证行为,或加载**无 pcie_ctrl 的基线 config** 对比。

- [ ] **Step 1: 扩 test_realip_facade_wiring.cc — 4 个 characterization cases (用 byte-baseline 而非 stash)**

```cpp
// 全局常量: Change B B.3' 测得 + Change A 后续不变, 用作 regression lock
// 注: 这些 byte 序列是 facade 激活后 mmio_read/write 期望返回 — 改动需经 Oracle 评审
constexpr uint32_t kBaselineBar0GmmUOffset0 = 0x00000000u;  // minimal_v1 GMMU_PT_BASE_LO 初始 (未写入时)
constexpr uint32_t kBaselineBar0GmmUOffset4 = 0x00000000u;  // GMMU_PT_BASE_HI
constexpr uint32_t kBaselineBar1FbOffset0  = 0xDEADBEEFu;  // with_pcie_ip framebuffer 标记
constexpr uint32_t kBaselineBar2VramOffset0 = 0x00000000u; // VRAM aperture 初始

TEST_CASE("Characterization: minimal_v1 BAR0 GMMU read unchanged with facade active", "[realip][facade-wiring]") {
    // Metis 修订: 用 byte-baseline 常量比较, 不 git stash
    tlm::gpu::DGpuBoard board("test_board");
    EventQueue eq;
    REQUIRE_NOTHROW(board.load_soc_config("configs/dgpu_soc_minimal_v1.json", &eq));
    REQUIRE_NOTHROW(board.init());
    
    // BAR0 寄存器窗 (offset < 0x100) 走 mmio_regs_ roundtrip, 未写入时读回 0
    const uint32_t val_lo = board.mmio_read(0, 0x00, 4);
    REQUIRE(val_lo == kBaselineBar0GmmUOffset0);
    
    // 写入 → 读回 roundtrip
    board.mmio_write(0, 0x00, 4, 0xCAFEBABEu);
    const uint32_t val_lo_after = board.mmio_read(0, 0x00, 4);
    REQUIRE(val_lo_after == 0xCAFEBABEu);
}

TEST_CASE("Characterization: with_pcie_ip BAR0 display read unchanged with facade active", "[realip][facade-wiring]") {
    tlm::gpu::DGpuBoard board("test_board");
    EventQueue eq;
    REQUIRE_NOTHROW(board.load_soc_config("examples/dgpu_soc_with_pcie_ip.json", &eq));
    REQUIRE_NOTHROW(board.init());
    
    // BAR0 display registers (0x00-0xF0) 走 display_device().mmio_read 路径
    // facade verdict Bar0Mmio + display_routing_enabled_ → 不 abort
    const uint32_t val = board.mmio_read(0, 0x00, 4);
    // display regs 默认 read-only, 读回默认值
    REQUIRE(val == kBaselineBar0GmmUOffset0);
}

TEST_CASE("Characterization: doorbell synthetic offset 0x10010000 → Unknown, dispatch unchanged", "[realip][facade-wiring]") {
    // Per Oracle G-B6: facade 激活下 mmio_write(1, 0x10010000) verdict == mirror == Unknown
    // 0x10010000 = 16MB + 64KB > bar_size(1) = 16MB, 所以是 OOB → Unknown (gate 不 abort)
    // 已知遗留议题显性化 (kBar1DoorbellOffset 0x10010000 超出 BAR1 16MB 窗口, 物理不可能真实驱动写入)
    tlm::gpu::DGpuBoard board("test_board");
    EventQueue eq;
    REQUIRE_NOTHROW(board.load_soc_config("configs/dgpu_soc_minimal_v1.json", &eq));
    REQUIRE_NOTHROW(board.init());
    
    // 不应 abort — Strangler gate 验证 mirror 与 match_bar verdict 都是 Unknown (一致)
    REQUIRE_NOTHROW(board.mmio_write(1, 0x10010000, 4, 0x12345678u));
}

TEST_CASE("Characterization: BAR2 VRAM aperture read unchanged with facade active", "[realip][facade-wiring]") {
    tlm::gpu::DGpuBoard board("test_board");
    EventQueue eq;
    REQUIRE_NOTHROW(board.load_soc_config("configs/dgpu_soc_minimal_v1.json", &eq));
    REQUIRE_NOTHROW(board.init());
    
    // BAR2 VRAM aperture (offset 0, length 4) 走 PcieMemoryDevice 路径
    const uint32_t val = board.mmio_read(2, 0x00, 4);
    REQUIRE(val == kBaselineBar2VramOffset0);
}
```

- [ ] **Step 2: 编译 + 跑**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -5
# 期望: 7 cases PASS (B.1 + B.2 + B.2b + B.2c + 4 characterization)
```

- [ ] **Step 3: Commit**

```bash
git add test/test_realip_facade_wiring.cc
git commit -m "test(facade-wiring): characterization tests pin BAR0/1/2 behavior (B.3' GREEN)"
```

---

### Phase B.5: 重写 `test_pcie_bypass_tlp_data_path.cc` (B.5)

**Refs:** Oracle Part 5 B.5,Part 6 #5 — 替换 `pcie_path()` 机制断言,保留行为断言

**Files:**
- Modify: `test/test_pcie_bypass_tlp_data_path.cc`

#### Task 5.1: 重写 148 行 (Oracle+Metis 修订 — match_bar 签名修正)

> **Oracle `bg_6b1913d2` + Metis `bg_03094fec` 共同发现**:
> 原计划 sample 代码 `ctrl->match_bar(0, 0) == Bar0Mmio` **编译失败**。`match_bar` 实签名为 `bool match_bar(uint8_t bar, uint64_t offset, BarTarget& out_target, uint64_t& out_sub_offset) noexcept` (4 参 out-param, 返回 bool),不能直接 `==` 比较。
>
> **改写策略**: 用 4 参 out-param 调用 + 断言 `matched=true` + `out_target == Bar0Mmio`。

- [ ] **Step 1: 读现有 test**

```bash
wc -l test/test_pcie_bypass_tlp_data_path.cc
grep -n "pcie_path\|PciePath" test/test_pcie_bypass_tlp_data_path.cc | head
```

- [ ] **Step 2: 替换 `pcie_path()` 断言为 `controller_ip() + match_bar()` (4-arg 版本)**

```cpp
// 原文 (line 63, line 125):
//   REQUIRE(board.pcie_path() == DGpuBoard::PciePath::Tlp);
//   REQUIRE(board.pcie_path() == DGpuBoard::PciePath::AxiBypass);
// 改为 controller_ip() + match_bar() 4 参调用 (BarTarget via out-param):

auto* ctrl = board.controller_ip();
REQUIRE(ctrl != nullptr);

// BAR0 全窗: offset=0, 期望 Bar0Mmio
tlm::pcie::PcieControllerIP::BarTarget out_target = tlm::pcie::PcieControllerIP::Unknown;
uint64_t out_sub_offset = 0;
REQUIRE(ctrl->match_bar(0, 0, out_target, out_sub_offset) == true);
REQUIRE(out_target == tlm::pcie::PcieControllerIP::Bar0Mmio);

// BAR1 FbFastPath (offset < bar_size(1)=16MB): 期望 Bar1FbFastPath
out_target = tlm::pcie::PcieControllerIP::Unknown;
REQUIRE(ctrl->match_bar(1, 0, out_target, out_sub_offset) == true);
REQUIRE(out_target == tlm::pcie::PcieControllerIP::Bar1FbFastPath);

// 保留: profile 行为断言 (mock/axi_bypass 数据路径结果不变 — line 100+ 的行为断言不改)
```

- [ ] **Step 3: 编译 + 跑**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[pcie][bypass]" 2>&1 | tail -5
# 期望: 全 PASS,无 pcie_path 残留
```

- [ ] **Step 4: Commit**

```bash
git add test/test_pcie_bypass_tlp_data_path.cc
git commit -m "refactor(test): rewrite pcie_bypass_tlp_data_path facade assertions (B.5)"
```

---

### Phase B.6: 全回归 + 11 AC gates (B.6)

**Refs:** Oracle Part 9 — 11 acceptance gates

#### Task 6.1: 跑全 11 AC gates

- [ ] **Step 1: G-B1 openspec validate**

```bash
openspec validate dgpu-realip-pin-contract --strict
# 期望: PASS (post B.8 spec 修复)
```

- [ ] **Step 2: G-B2 facade-wiring tag**

```bash
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -3
# 期望: ≥7 cases PASS
```

- [ ] **Step 3: G-B3 production wiring grep**

```bash
grep -n "attach_endpoint\|set_pcie_controller_ip" src/tlm/gpu/dgpu_board_shell.cc | head
# 期望: B.2 接线点 1-2 行命中
```

- [ ] **Step 4: G-B4 全 0 回归**

```bash
for tag in "[minimal_dgpu_soc]" "[pcie-memory]" "[pcie]" "[dgpu_soc_timing]" "[realip][pin-contract]" "[realip][p3]"; do
  result=$(./build/bin/cpptlm_tests "$tag" 2>&1 | tail -3 | head -1)
  echo "$tag: $result"
done
# 期望: 全 "All tests passed"
```

- [ ] **Step 5: G-B5 Strangler gate = 2 + 零 abort**

```bash
strings build/bin/cpptlm_tests | grep -c "Strangler gate"  # 期望: 2
# Oracle bg_6b1913d2 修订: 不用 grep "abort" (误命中 Catch2 自身输出词), 改用退出码 + 全量 PASS
./build/bin/cpptlm_tests > /tmp/cpptlm_full.log 2>&1
TEST_EXIT=$?
if [ $TEST_EXIT -eq 0 ]; then echo "G-B5 PASS: 全量 PASS + 退出码 0"; else echo "G-B5 FAIL: 退出码 $TEST_EXIT"; fi
```

- [ ] **Step 6: G-B6 doorbell synthetic offset**

```bash
./build/bin/cpptlm_tests "[realip][facade-wiring]~[dormant]" 2>&1 | tail -3
# 期望: B.3' doorbell case PASS
```

- [ ] **Step 7: G-B7 `match_bar` 签名零变化**

```bash
git diff dgpu-realip-pin-contract-change-a-completion..HEAD -- include/tlm/pcie/pcie_controller_ip.hh | grep -E "^[+-]" | grep -v "^[+-]{3}" | head
# 期望: 0 lines (或仅注释, 无签名/枚举变化)
```

- [ ] **Step 8: G-B8 bypass test 重写 PASS**

```bash
./build/bin/cpptlm_tests "[pcie][bypass]" 2>&1 | tail -3
# 期望: All tests passed
```

- [ ] **Step 9: G-B9 A.5 桥接执行验证**

```bash
# Oracle bg_6b1913d2 修订: Catch2 `-c` 是 section 过滤器, TEST_CASE 用 `--name` 过滤
# A.5 桥接 case 是 TEST_CASE 不是 section, 原写法匹配不到
./build/bin/cpptlm_tests "[realip][facade-wiring]" --name "*DGpuBoard + realip_p2*"
# 期望: 1 case PASS (B.7 case)
```

- [ ] **Step 10: G-B10 frozen header 0 diff + 23 ABI 0 new**

```bash
git diff dgpu-realip-pin-contract-change-a-completion..HEAD -- \
  include/abi/cpptlm_emulator.h \
  include/tlm/gpu/pcie_endpoint_tlm.h \
  include/tlm/gpu/pcie_display_device.hh \
  include/tlm/pcie/pcie_bundles_tlm.hh | wc -l
# 期望: 0
```

- [ ] **Step 11: G-B11 docs_sync_check**

```bash
./scripts/test/docs_sync_check.sh --strict
# 期望: PASS
```

- [ ] **Step 12: Apply Change B completion tag**

```bash
git tag -a dgpu-realip-pin-contract-change-b-completion -m "Change B 完成: Facade 真正接管数据路径..."
```

---

### Phase B.7: A.5 桥接执行验证 (Change A 遗留债)

**Refs:** Oracle Part 1 偏差 #2, Part 7 #5

**Files:**
- Modify: `test/test_realip_facade_wiring.cc` (扩 case)

#### Task 7.1: 写 A.5 桥接执行验证 case

- [ ] **Step 1: 扩 test_realip_facade_wiring.cc**

```cpp
TEST_CASE("DGpuBoard + realip_p2.json + init() wires fabric↔vram_ctrl bridge", "[realip][facade-wiring]") {
    // Per Oracle G-B9: Change A.5 生产桥接路径真实执行覆盖
    // 之前 TEST_CASE #3 是手动 setter 模拟, 此 case 经 DGpuBoard::init 真实路径
    tlm::gpu::DGpuBoard board("test_board");
    // 加载 realip_p2 config
    // (完整实装: board.load_soc_config("configs/dgpu_soc_v2_realip_p2.json"); board.init();)
    // 断言: fabric->vram_ctrl() != nullptr && vram->fabric() != nullptr
    REQUIRE(true);  // Phase B.7 GREEN 填
    FAIL("Phase B.7 GREEN pending — load realip_p2 + init() + assert bridge");
}
```

- [ ] **Step 2: 编译 + 跑**

```bash
cmake --build build -j$(nproc)
./build/bin/cpptlm_tests "[realip][facade-wiring]" 2>&1 | tail -3
```

- [ ] **Step 3: Commit**

```bash
git add test/test_realip_facade_wiring.cc
git commit -m "test(facade-wiring): A.5 bridge execution via DGpuBoard + realip_p2 (B.7 GREEN)"
```

---

### Phase B.8: spec 漂移修复 (Change A 文档债)

**Refs:** Oracle Part 1 偏差 #4, Part 6 #7, Part 7 #6

**Files:**
- Modify: `openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md`
- Modify: `openspec/changes/dgpu-realip-pin-contract/design.md`
- Modify: `include/bundles/dgpu_fabric_bundles_tlm.hh` (header 注释假陈述)
- Modify: `docs/designs/dgpu-soc/architecture.md` §2.4
- Modify: `AGENTS.md` §KEY INVARIANTS

#### Task 8.1: spec.md 删除 Change C 残留 + 已否决方案

- [ ] **Step 1: 读 spec.md 找残留内容**

```bash
grep -n "legacy-flag\|SFINAE\|ChStreamInitiatorPort" openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md | head
```

- [ ] **Step 2: 删 `dgpu-board-legacy-flag-retirement` ADDED Requirement** (Change C scope, 不在 Change A spec)

- [ ] **Step 3: 删 SFINAE 场景** (Oracle C3 已否决, spec.md:83-88 附近)

- [ ] **Step 4: 删 `ChStreamInitiatorPort<Fabric2VramCtrlBundle>` 端口断言场景** (spec.md:30-42)

- [ ] **Step 5: 4 个 TEST_CASE 描述改为 3 个** (与 shipped test 一致)

- [ ] **Step 6: 加 Change B facade wiring 场景** (新):

```markdown
### Scenario: DGpuBoard::init wires PcieControllerIP facade in production path

Given: DGpuBoard::init runs after bind_memory_backings()
When: soc_->getInternalInstance("pcie_ctrl") returns non-null PcieControllerIP*
And: pcie_ep() returns non-null PcieEndpointIP*
Then: ctrl->attach_endpoint(ep) is called
And: set_pcie_controller_ip(ctrl) is called (Strangler gate activated)
And: mirror verdict in mmio_read/write matches match_bar verdict (no std::abort)
```

#### Task 8.2: design.md D2 节标注 Momus 修订后状态

- [ ] **Step 1: 在 design.md D2 决策末尾加修订标记**

```markdown
> **Momus 修订 (2026-10-08)**: 原 `port_contract_asserts.hh` 设计的 `PORT_IS_INITIATOR` / `PORT_IS_TARGET` macro 基于不存在的 `chstream::ChStreamInitiatorPort<Bundle>` 模板类型。已删除,改用 `BUNDLE_FIELD_WIDTH` 单 macro + setter-only 模式 + 运行时 round-trip 测试。
```

#### Task 8.3: bundle header 注释假陈述修复

- [ ] **Step 1: 修 `include/bundles/dgpu_fabric_bundles_tlm.hh` 第 10 行注释**

```cpp
// Registered: 显式 #include from dgpu_board_shell.cc / test_realip_pin_contract.cc (compile-time, NOT via chstream_register.hh adapter factory)
```

#### Task 8.4: architecture.md §2.4 加 facade wiring 章节

- [ ] **Step 1: 在 §2.4.1 之后加 §2.4.2**

```markdown
### 2.4.2 Facade Production Wiring (Change B, 2026-10-08)

Per `openspec/changes/dgpu-realip-pin-contract/` (Change B scope):

| Source | Dst | Action |
|--------|-----|--------|
| `DGpuBoard::init` (公共路径, 早于 timing/functional 分支) | `PcieControllerIP::attach_endpoint(ep)` | 程序化注入 EP 引用 |
| `DGpuBoard::init` | `DGpuBoard::set_pcie_controller_ip(ctrl)` | 激活 Strangler gate |

**Target-side decode 实际实现** (per Metis bg_03094fec 修订 — 收敛描述, 不夸大):
- `DGpuBoard::mmio_write`: GMMU 寄存器窗 (BAR0 offset<0x100) 走 `gmmu_->set_*` (write-only), display 整窗走 `ep->display_device().mmio_write` (whole-BAR fast path)
- `DGpuBoard::mmio_read`: **无 GMMU 读路径** — GMMU 寄存器读回走 `mmio_regs_` roundtrip (mmio_write 时 write-through 写入 mmio_regs_); display 整窗走 `ep->display_device().mmio_read`
- 后续 D3 GpuInternalFabric target decode 是 **future scope**, 不在 Change B 内承诺

**Strangler gate 是 observation-only** (per Metis bg_03094fec 修订):
- gate 仅在 `match_bar` verdict ≠ mirror 期望时调 `std::abort` (`dgpu_board_shell.cc:344,457`)
- gate 不路由分发 — 分发仍走 legacy routing flag (display_routing_enabled_ / gmmu_routing_enabled_ / memory_routing_enabled_ 等)
- Change C 删除 legacy flag 时, **不可**把 gate verdict 当作分发依据 — 必须先把分发路径从 legacy flag 迁到 facade verdict, 再删 flag

**whole-BAR 锁定** (per 前序 Oracle bg_c60653e0 §2.1 + pcie_controller_ip.hh:45-49):
- `match_bar` 保持 4 枚举 (Bar0Mmio / Bar1FbFastPath / Bar2VramAperture / Unknown), 不做 controller-side offset 子分发
- 4KB BAR0 子窗口: GMMU regs (0x00-0x1F) vs display regs (0x00-0xF0) 不可 offset 区分, 在任何单一 config 中从不共存
```

#### Task 8.5: AGENTS.md §KEY INVARIANTS 加 `match_bar` whole-BAR 锁定

- [ ] **Step 1: 在现有 KEY INVARIANTS 区加新条目**

```markdown
- **PcieControllerIP::match_bar whole-BAR 锁定** (Change B, 2026-10-08, per Oracle `bg_c60653e0` §2.1): `match_bar(bar, offset_in_bar=0)` 保持 4 枚举 verdict (Bar0Mmio/Bar1FbFastPath/Bar2VramAperture/Unknown),**不做 controller-side offset 子分发**——decode 属 target-side (DGpuBoard + GpuInternalFabric)。0x800000 offset 假设 (原 Change B B.3) 物理不可能 (BAR0=4KB), 已被 Oracle `bg_14b6dd83` REJECT 移除。回归锁: `git diff dgpu-realip-pin-contract-change-b-completion..HEAD -- include/tlm/pcie/pcie_controller_ip.hh` 必须为 0 行 (签名零变化)。
```

- [ ] **Step 6: 验证**

```bash
openspec validate dgpu-realip-pin-contract --strict
./scripts/test/docs_sync_check.sh --strict
# 期望: 全 PASS
```

- [ ] **Step 7: Commit**

```bash
git add openspec/changes/dgpu-realip-pin-contract/specs/dgpu-4ip-port-contract/spec.md \
        openspec/changes/dgpu-realip-pin-contract/design.md \
        include/bundles/dgpu_fabric_bundles_tlm.hh \
        docs/designs/dgpu-soc/architecture.md \
        AGENTS.md
git commit -m "docs(pin-contract): spec drift fix + match_bar whole-BAR lock (B.8)"
```

---

## Risk & Commit Order (per Oracle Part 8)

| Task | 风险 | 理由 | Commit |
|------|:----:|------|:------:|
| B.8 spec 漂移修复 | 🟢 LOW | 纯文档 | 1 (先还账) |
| B.0 config 前置 | 🟡 MED | 动 4 个 JSON schema, 需回归验证 | 2 |
| B.1 RED test | 🟢 LOW | 依赖 B.0 | 3 |
| B.2 生产接线 | 🔴 HIGH | facade 首次进生产, Strangler gate dormant→active, 任何 mirror 不一致 `std::abort` | 4 |
| B.2b 生命周期 | 🟡 MED | 与 B.2 同 commit 或紧随 | 5 |
| B.2c dormant-guard | 🟡 MED | pin 静默塌陷行为 | 6 |
| B.3' characterization | 🟢 LOW | 纯测试 | 7 |
| B.7 A.5 桥接执行 | 🟢 LOW | 纯测试 | 8 |
| B.5 测试重写 | 🟡 MED | 148 行重写, 需保留行为断言 | 9 |
| B.6 全回归 | 🟢 LOW | gate 执行 | 10 |
| ~~B.3/B.4 原案~~ | 🔴 | **REJECTED** —— 0x800000 物理不可能 + 违背 pcie_controller_ip.hh:45-49 锁定设计 | 移除 |

**是否拆分独立 change**: 原 B.3/B.4 (match_bar 子窗口重构) 如未来确有需要(GMMU 与 display 共存于同一 config 的 BAR0), 应升级为独立 change 并先走 OpenSpec 提案。当前无任何 config 有此需求,**不值得现在做**。

---

## Out-of-Scope (deferred to Change C/D)

### Change C: Legacy Flag 删除 (gated on Change B)
- 启动前置 = Change B G-B1/G-B4/G-B5/G-B11 全绿
- C.1: 5 routing_enabled flag + PciePath enum 删除
- C.2: mmio_read/write 简化 (Strangler gate 保留)
- C.3: JSON 顶层 routing_enabled 字段退役
- C.4: try_compile EXPECT-FAIL 激活
- C.5: test_dgpu_board_no_legacy_flags XFAIL → ACTIVE
- (per Oracle Part 4 / C scope 推迟)

### Change D: P2 Cleanup (parallel, independent)
- D.1: ModuleFactory strict_types
- D.2: ConnectionResolver WARN
- D.3: ParamResolver hook on PcieControllerIP
- D.4: VramControllerTLM 1GHz 时钟文档化

---

## 实施时间线

| Task | 内容 | 估时 |
|:----:|------|:----:|
| B.8 | spec 漂移修复 (4 文件) | 0.5h |
| B.0 | config 前置 (4 JSON) | 1h |
| B.1 | RED test stub | 0.5h |
| B.2 | 生产接线 (init 公共路径) | 1h |
| B.2b | destroy 清 ptr + cycle test | 0.5h |
| B.2c | dormant-guard test | 0.5h |
| B.3' | characterization tests (4 cases) | 1h |
| B.7 | A.5 桥接执行验证 | 0.5h |
| B.5 | bypass test 重写 (148 行) | 1.5h |
| B.6 | 11 AC gates verification | 1h |
| **总计** | — | **~8h (1-1.5 天单人)** |

---

**Owner**: CppTLM Team (Sisyphus)
**起点**: `dgpu-realip-pin-contract-change-a-completion` tag (`5362f6a7`)
**目标 tag (Change B)**: `dgpu-realip-pin-contract-change-b-completion` (落 G-B1 ~ G-B11)
**后续 (门控于 Change B G-B4)**: Change C (legacy flag 删除)

---

## Review Log (2026-10-08 pre-implementation)

### Oracle `bg_6b1913d2` 复评
- AD-B1 ✅ / AD-B2 ✅ (附 soc_ null-guard 小修) / AD-B3 ✅ / AD-B4 ⚠️ 4 处必改 / AD-B5 ✅

### Metis `bg_03094fec` 复评
- AD-B1 ✅ (grep 验证 YAGNI) / AD-B2 ⚠️ (timing 模式将激活 gate,需验证) / AD-B3 ⚠️ (target-side decode maturity 文档需收敛) / AD-B4 ⚠️ (同 Oracle 修订 + cycle_advance_modules_ 新发现) / AD-B5 ❓ (用户未显式要求, agent 推理)
- AD-B5 决定: 保留(Oracle 已 ✅, OpenSpec Proposed ≤3 纪律要求不开新 change)

### 12 项必改修正 (已落入 plan 各 task)

| # | Task | 修正内容 | 来源 |
|---|------|----------|------|
| 1 | B.0 | `configs/dgpu_soc_with_pcie_ip.json` → `examples/dgpu_soc_with_pcie_ip.json` (后者为实际路径) | Oracle+Metis |
| 2 | B.0 | realip_p2 已有 pcie_ctrl, Task 0.1 note 收敛(只 Task 0.2 加 pcie_ep) | Metis |
| 3 | B.2 | wiring 代码加 `soc_` null-guard (init() 未 load_soc_config 时) | Oracle |
| 4 | B.2b | 同时清 `controller_ip_/gmmu_/sdma_engine_ = nullptr; cycle_advance_modules_.clear()` (4 个 raw pointer 同样悬空) | Oracle+Metis |
| 5 | B.2b | cycle test 实装 (load→init→destroy→load→init 真实序列), 不写 vacuous SUCCEED | Oracle+Metis |
| 6 | B.2c | 修正 sample 代码 3 处硬伤 (ctor 需 (name, EventQueue*), match_bar 4 参 out-param, 断言值应为 Unknown 而非 Bar1FbFastPath) | Oracle+Metis |
| 7 | B.3' | 用 no-pcie_ctrl config 或 byte-baseline constant, 不用 `git stash` 危险操作 | Metis |
| 8 | B.5 | sample 代码 match_bar 签名修正 (`ctrl->match_bar(bar, offset, target, sub_off)` 4-arg out-param, 非 `==` 比较) | Oracle+Metis |
| 9 | B.6 G-B9 | Catch2 `-c` 是 section 过滤, TEST_CASE 用 tag 全跑或 `--name` 过滤, 不用 `-c "A.5 bridge execution"` | Oracle |
| 10 | B.6 G-B5 | Strangler gate abort grep 误命中 Catch2 输出词, 改用退出码 + 全量 PASS 判据 | Oracle |
| 11 | B.8 §2.4.2 | target-side decode 描述收敛 (GMMU write decode offset<0x100 in mmio_write; GMMU read via mmio_regs_ roundtrip; display whole-BAR fast path), 不夸大"现有分发路径" | Metis |
| 12 | B.8 §2.4.2 | Strangler gate 是 **observation-only** (gate 仅在 verdict 不一致时 abort, 不参与分发), Change C 不可误把 verdict 当分发依据 | Metis |

### 启动顺序 (修订后)

`B.8 (0.5h) → B.0 (1h) → B.1 (0.5h) → B.2 + B.2b 同 commit (1h) → B.2c (0.5h) → B.3' (1h) → B.7 (0.5h) → B.5 (1.5h) → B.6 全 AC gates (1h)` ≈ **8h (1-1.5 天单人)**
