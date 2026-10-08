# dgpu-realip-pin-contract: Tasks (4 commit 结构)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/dgpu-4ip-port-contract/spec.md`](specs/dgpu-4ip-port-contract/spec.md)
> **起点**: `dgpu-realip-refactor-completion` tag (Phase C, 2026-10-07)
> **目标 tag**: `dgpu-realip-pin-contract-completion` (落 DoD)
> **关键修订**: 起点 Phase C 后 4-IP topology 锁定,本 change 收紧 DGpuBoard + 端口契约

---

## 文件清单 (4 commit 结构)

| 文件 | 变化 | Commit |
|------|------|:-----:|
| `include/bundles/dgpu_fabric_bundles_tlm.hh` | **新** (~60 行) | Commit 1 |
| `include/utils/port_contract_asserts.hh` | **新** (~80 行) | Commit 1 |
| `include/chstream_register.hh` | 改 (注册 Fabric2VramCtrlBundle) | Commit 1 |
| `include/tlm/cluster/gpu_internal_fabric.hh` | 改 (vram_ctrl_out 端口 + set_vram_target setter) | Commit 1 |
| `include/tlm/vram_controller_tlm.hh` | 改 (fabric_in 端口扩展) | Commit 1 |
| `include/tlm/gpu/dgpu_board_shell.hh` | 改 (删 7 flag + 5 setter + 5 accessor + 1 enum + 1 accessor) | Commit 2 |
| `src/tlm/gpu/dgpu_board_shell.cc` | 改 (mmio_read/write 简化 + pcie_path_str 加载删除 + 5 routing_enabled 加载删除) | Commit 2 |
| `configs/dgpu_soc_v2_realip_p2.json` | 改 (删 4 routing_enabled 字段 + version 字段递增) | Commit 2 |
| `test/test_realip_pin_contract.cc` | **新** (3 cases, 双标签 `[realip][pin-contract]`) | Commit 3 |
| `test/test_dgpu_board_no_legacy_flags.cc` | **新** (1 case, 双标签 `[realip][pin-contract]`) | Commit 3 |
| `docs/designs/dgpu-soc/architecture.md` §2.4 | 改 (端口契约文档) | Commit 2 + 4 |
| `docs/designs/dgpu-soc/architecture.md` §7.3 | 改 (JSON 顶层 routing_enabled 字段退役说明) | Commit 2 + 4 |
| `AGENTS.md` KEY INVARIANTS | 改 (新增 legacy flag 退役条目) | Commit 4 |

**总文件数**: 14 (含 3 个新建 hh + 1 个新建 util + 2 个新建 test)

---

## Commit 1: Fabric2VramCtrlBundle + port_contract_asserts + 端口扩展 (TDD 5 步先测试后代码)

### 1.1 RED (写测试, 期望 FAIL)

- [ ] 1.1.1 创建 `test/test_realip_pin_contract.cc` 初始 stub (T23C stub, **双标签** `[realip][pin-contract]`):
  - TEST_CASE #1: `Fabric2VramCtrlBundle` 字段定义 (8 字段宽断言)
  - TEST_CASE #2: `GpuInternalFabric::vram_ctrl_out` 是 ChInitiatorPort
  - TEST_CASE #3: `set_vram_target` 程序化桥接 round-trip (经 JSON 装配)
- [ ] 1.1.2 跑 `cmake --build build -j$(nproc)` → **编译失败** (Fabric2VramCtrlBundle / vram_ctrl_out / fabric_in 不存在)
- [ ] 1.1.3 跑 `./build/bin/cpptlm_tests "[realip][pin-contract]"` → **No test cases matched** (新测试未编译进 binary)

### 1.2 GREEN (改代码, 期望 PASS)

- [ ] 1.2.1 创建 `include/bundles/dgpu_fabric_bundles_tlm.hh` (~60 行, per design §D1):
  - 8 字段: `address[64]` / `size[8]` / `is_write[1]` / `data[64]` / `transaction_id[32]` / `resp_data[64]` / `error_code[8]` / `chip_select[8]`
  - 用 `ch_uint<W>` 类型 (per `cpphdl_types.hh` 复用,per Phase 5 chstream_register 模式)
- [ ] 1.2.2 创建 `include/utils/port_contract_asserts.hh` (~80 行, per design §D2):
  - `PORT_IS_INITIATOR(port, bundle)` macro: `static_assert(std::is_base_of_v<ChStreamInitiatorPort<bundle>, decltype(port)>, ...)`
  - `PORT_IS_TARGET(port, bundle)` macro: 对称
  - `BUNDLE_FIELD_WIDTH(bundle, field, expected_width)` macro: `static_assert(decltype(std::declval<bundle>().field)::width == expected_width, ...)`
- [ ] 1.2.3 修改 `include/chstream_register.hh` (REGISTER_CHSTREAM 加 Fabric2VramCtrlBundle bundle 注册):
  - 注意: `Fabric2VramCtrlBundle` 是 bundle struct,不是 ChStream module,**走 `ChStreamAdapterFactory` 不适用**
  - 替代方案: 在 `chstream_register.hh` 添加 `#include "bundles/dgpu_fabric_bundles_tlm.hh"` (compile-time include)
- [ ] 1.2.4 修改 `include/tlm/cluster/gpu_internal_fabric.hh`:
  - 添加 `vram_ctrl_out` 端口声明: `ChStreamInitiatorPort<Fabric2VramCtrlBundle> vram_ctrl_out;`
  - 添加 `set_vram_target(VramControllerTLM*)` setter: 缓存 `VramControllerTLM* vram_ctrl_ = nullptr;`
  - 添加 `static_assert` 验证 `vram_ctrl_out` 是 ChStreamInitiatorPort (per `port_contract_asserts.hh`)
- [ ] 1.2.5 修改 `include/tlm/vram_controller_tlm.hh`:
  - 添加 `fabric_in` 端口声明: `ChStreamTargetPort<Fabric2VramCtrlBundle> fabric_in;`
  - `static_assert` 验证 `fabric_in` 是 ChStreamTargetPort
- [ ] 1.2.6 修改 `src/tlm/gpu/dgpu_board_shell.cc` `bind_memory_backings()`:
  - 在 `vram_ctrl->set_backing_view(...)` 之后,加 `fabric->set_vram_target(vram_ctrl)` (per design §D3)
- [ ] 1.2.7 跑 `cmake --build build -j$(nproc)` → 退出码 0 (所有 5 文件编译)
- [ ] 1.2.8 跑 `./build/bin/cpptlm_tests "[realip][pin-contract]"` → **3 cases PASS** (per Commit 1 GREEN)

### 1.3 REFACTOR (cleanup, 验证稳定性)

- [ ] 1.3.1 `Fabric2VramCtrlBundle` 字段定义 comment 文档化 (chip-internal wire-protocol)
- [ ] 1.3.2 `port_contract_asserts.hh` macro 添加 4-arg overload (端口名 + bundle 类型 + 期望方向 + 错误消息)
- [ ] 1.3.3 跑 5 次 `[realip][pin-contract]` → 5/5 stable (避免间歇性 test_local depth 累积问题,per Phase C 经验)
- [ ] 1.3.4 跑 `./build/bin/cpptlm_tests "[realip][p3]"` → ≥9 cases 0 回归 (Phase C 后基线)
- [ ] 1.3.5 跑 `./build/bin/cpptlm_tests "[fabric]"` → 7 cases 0 回归 (T22b attach_crossbar 锁定)
- [ ] 1.3.6 跑 `./build/bin/cpptlm_tests "[pcie]"` → ≥433 cases 0 回归 (Phase C 后基线)
- [ ] 1.3.7 commit: `feat(p3): Fabric2VramCtrlBundle + port_contract_asserts + 4-IP 端口扩展`

### Commit 1 验收 (per design §3 D1+D2+D3)

- [ ] `Fabric2VramCtrlBundle` 8 字段宽对齐 `ch_uint<W>` (per `cpphdl_types.hh`)
- [ ] `port_contract_asserts.hh` 提供 `PORT_IS_INITIATOR` / `PORT_IS_TARGET` / `BUNDLE_FIELD_WIDTH` 3 个 macro
- [ ] `GpuInternalFabric::vram_ctrl_out` 是 ChInitiatorPort<Fabric2VramCtrlBundle>
- [ ] `VramControllerTLM::fabric_in` 是 ChTargetPort<Fabric2VramCtrlBundle>
- [ ] `set_vram_target` 程序化桥接 round-trip PASS (经 JSON 装配 + DGpuBoard::init)
- [ ] `[realip][p3]` 9/122 0 回归 + `[fabric]` 7/13 0 回归 + `[pcie]` 433/36682 0 回归
- [ ] Strangler gate = 2 (per `strings build/bin/cpptlm_tests | grep -c "Strangler gate"`)

---

## Commit 2: DGpuBoard legacy flag 删除 + mmio_read/write 简化 + JSON 退役 (flag-gated 纪律)

### 2.1 顺序删除 (per design §D4 删除顺序 #1-7)

- [ ] 2.1.1 删除 `PciePath` enum + `pcie_path()` accessor + `pcie_path_` 成员 (`dgpu_board_shell.hh:75-77,86,317`)
- [ ] 2.1.2 删除 `set_display_routing_enabled` + `display_routing_enabled()` + `display_routing_enabled_` (`:103-108,340`)
- [ ] 2.1.3 删除 `set_memory_routing_enabled` + `memory_routing_enabled()` + `memory_routing_enabled_` (`:111-116,348`)
- [ ] 2.1.4 删除 `set_storage_routing_enabled` + `storage_routing_enabled()` + `storage_routing_enabled_` (`:268-269,345`)
- [ ] 2.1.5 删除 `set_gmmu_routing_enabled` + `gmmu_routing_enabled()` + `gmmu_routing_enabled_` (`:270-271,346`)
- [ ] 2.1.6 **保留** `controller_ip_` 成员 + `set_pcie_controller_ip()` setter + `controller_ip()` accessor (P2 PR-A facade 必要)
- [ ] 2.1.7 **保留** `mmio_self_drain_enabled` 成员 (drain 优化,非 legacy routing)

### 2.2 mmio_read/write 简化 (`dgpu_board_shell.cc:308-422` + `:433-466`)

- [ ] 2.2.1 删除 line 30-39 `pcie_path_str` JSON 加载逻辑 (5 行)
- [ ] 2.2.2 删除 line 108-126 5 `routing_enabled_*` JSON 加载逻辑 (~20 行)
- [ ] 2.2.3 mmio_read 简化 line 349-355 (display fast-path `if (display_routing_enabled_ && bar==0)` → `if (bar==0)`)
- [ ] 2.2.4 mmio_read 简化 line 358-365 (storage fast-path `if (storage_routing_enabled_ && bar==1)` → `if (bar==1)`)
- [ ] 2.2.5 mmio_read 简化 line 367-371 (memory fast-path `if (memory_routing_enabled_ && bar==2)` → `if (bar==2)`)
- [ ] 2.2.6 **保留** line 320-343 Strangler gate (`controller_ip_` if + facade verdict divergence std::abort)
- [ ] 2.2.7 **保留** line 387 self-drain (`mmio_self_drain_enabled` if)
- [ ] 2.2.8 mmio_write 镜像对称修改 (`:433-466` 同样 6 if 分支简化)

### 2.3 JSON 顶层 routing_enabled 字段退役 (`dgpu_soc_v2_realip_p2.json`)

- [ ] 2.3.1 删除 `memory_routing_enabled` 顶层字段 (`:7`)
- [ ] 2.3.2 删除 `storage_routing_enabled` 顶层字段 (`:7`)
- [ ] 2.3.3 删除 `gmmu_routing_enabled` 顶层字段 (`:7`)
- [ ] 2.3.4 删除 `display_routing_enabled` 顶层字段 (`:7`)
- [ ] 2.3.5 修改 `version` 字段 `"2-realip-p2-b"` → `"2-realip-p3-pin"` (Phase C → Phase D 版本递增)
- [ ] 2.3.6 更新 `_note` 字段: 引用本 change (Phase D pin contract) + ADR-DGPU-10 legacy flag 防劫持锁定

### 2.4 文档同步 (`docs/designs/dgpu-soc/architecture.md`)

- [ ] 2.4.1 §2.4 (4-IP 边界表) 新增 chip-internal port map: `fabric.vram_ctrl_out → vram_ctrl.fabric_in`,Fabric2VramCtrlBundle 字段定义引用 Commit 1
- [ ] 2.4.2 §7.3 (JSON 层级约束) 退役字段说明: 4 个 `routing_enabled` 字段移除 + Strangler gate 取代

### 2.5 验证

- [ ] 2.5.1 `cmake --build build -j$(nproc)` → 退出码 0 (DGpuBoard + JSON + doc 编译/解析 PASS)
- [ ] 2.5.2 `./build/bin/cpptlm_tests "[realip][p3]"` → ≥9 cases 0 回归
- [ ] 2.5.3 `./build/bin/cpptlm_tests "[fabric]"` → 7 cases 0 回归
- [ ] 2.5.4 `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` → 37 cases 0 回归 (MUST NOT 1 锚点)
- [ ] 2.5.5 `./build/bin/cpptlm_tests "[minimal_dgpu_soc]"` → 12 cases 0 回归
- [ ] 2.5.6 `./build/bin/cpptlm_tests "[pcie-memory]"` → 24 cases 0 回归
- [ ] 2.5.7 `./build/bin/cpptlm_tests "[realip][pin-contract]"` → ≥3 cases 0 回归
- [ ] 2.5.8 `./build/bin/cpptlm_tests "[pcie]"` → ≥433 cases 0 回归
- [ ] 2.5.9 `strings build/bin/cpptlm_tests | grep -c "Strangler gate"` = 2 (invariant)
- [ ] 2.5.10 `bash scripts/test/docs_sync_check.sh --strict` → 0 missing paths
- [ ] 2.5.11 commit: `feat(p3): DGpuBoard 7 legacy flag 删除 + JSON routing_enabled 退役`

### Commit 2 验收 (per design §3 D4+D5+D7)

- [ ] DGpuBoard public API 不含 7 已删除 flag/setter/accessor/enum (`test_dgpu_board_no_legacy_flags.cc` 编译期验证,Commit 3 实施)
- [ ] mmio_read/write 从 ~120 行 → ~40 行 (净删除 ~80 行)
- [ ] JSON 顶层 4 `routing_enabled` 字段全部移除
- [ ] Strangler gate 字符串仍存在 (`grep -c "Strangler gate"` = 2)
- [ ] `[pcie]` ≥433 cases 0 回归 + `[realip][p3]` ≥9 cases 0 回归 + `[realip][pin-contract]` ≥3 cases 0 回归
- [ ] docs_sync 0 missing

---

## Commit 3: 2 新测试 + 双标签防 vacuous pass (per handoff §9.2 #10)

### 3.1 test_dgpu_board_no_legacy_flags.cc (新增, 1 case)

- [ ] 3.1.1 创建 `test/test_dgpu_board_no_legacy_flags.cc` (1 case, **双标签** `[realip][pin-contract]`):
  - TEST_CASE #1: DGpuBoard public API 编译期不含已删除 5 routing flag (per design §D4 删除顺序 #2-5):
    - `static_assert` SFINAE 探测 `DGpuBoard::display_routing_enabled_` 成员不存在 (编译期 SFINAE)
    - `static_assert` 探测 `set_display_routing_enabled` setter 不存在
    - 同样 4 个: `memory_routing_enabled_` / `storage_routing_enabled_` / `gmmu_routing_enabled_` / `pcie_path_` + 5 个 setter
- [ ] 3.1.2 `static_assert` Strangler gate 字符串存在: `#include "src/tlm/gpu/dgpu_board_shell.cc"` (编译期 source grep)
- [ ] 3.1.3 TEST_CASE #1 包含 5 个 sub-assertion + Strangler gate 验证
- [ ] 3.1.4 跑 `cmake --build build -j$(nproc)` → 退出码 0 (DGpuBoard 误加回任一 flag 编译失败)
- [ ] 3.1.5 跑 `./build/bin/cpptlm_tests "[realip][pin-contract]"` → **1 case PASS**
- [ ] 3.1.6 跑 `./build/bin/cpptlm_tests "[pin-contract]"` → **1 case PASS** (双标签防 vacuous pass)
- [ ] 3.1.7 跑 `./build/bin/cpptlm_tests "[realip]"` → ≥433 cases 0 回归
- [ ] 3.1.8 commit: `test(p3): DGpuBoard public API 编译期不含 5 legacy flag (双标签 [realip][pin-contract])`

### 3.2 test_realip_pin_contract 扩展 (Commit 1 已写, Commit 3 加 1 case)

- [ ] 3.2.1 编辑 `test/test_realip_pin_contract.cc` 添加 TEST_CASE #4: VramControllerTLM::fabric_in 端口契约静态检查 (per spec §3 Scenario #2)
  - `static_assert` 验证 `fabric_in` 是 ChStreamTargetPort<Fabric2VramCtrlBundle>
  - `static_assert` 验证 `error_code[8]` 字段宽对齐 (per `vram_controller_tlm.hh:47-52` 4K row buffer)
- [ ] 3.2.2 跑 `./build/bin/cpptlm_tests "[realip][pin-contract]"` → **4 cases PASS** (Commit 1 #1-3 + Commit 3 #4)
- [ ] 3.2.3 跑 `./build/bin/cpptlm_tests "[pin-contract]"` → **4 cases PASS** (双标签)
- [ ] 3.2.4 跑 `./build/bin/cpptlm_tests "[realip]"` → ≥433 cases 0 回归

### Commit 3 验收 (per design §3 D6 + spec §3 Requirement #3)

- [ ] 2 个新测试文件**双标签** `[realip][pin-contract]` (per handoff §9.2 #10)
- [ ] `[realip][pin-contract]` ≥4 cases PASS (test_realip_pin_contract #1-4 + test_dgpu_board_no_legacy_flags #1)
- [ ] `[pin-contract]` ≥4 cases PASS (双标签防 vacuous pass, per cpptlm-p2-integration-unblock G9)
- [ ] `[realip]` ≥433 cases 0 回归 (双标签 G6 floor 数学成立)
- [ ] 全部 `static_assert` 编译期通过,无运行时开销

---

## Commit 4: 文档同步 + AGENTS.md + openspec archive + tag

### 4.1 AGENTS.md 同步

- [ ] 4.1.1 `AGENTS.md` KEY INVARIANTS 新增条目: "4-IP topology 不再依赖 routing flag (per Phase D pin-contract),端口契约代码化为 chip-internal 1:1 映射"
- [ ] 4.1.2 `AGENTS.md` STRUCTURE 节同步 `docs/designs/dgpu-soc/architecture.md` §2.4/§7.3 更新
- [ ] 4.1.3 跑 `bash scripts/test/docs_sync_check.sh --strict` → 0 missing paths

### 4.2 架构文档最终同步 (`docs/designs/dgpu-soc/architecture.md`)

- [ ] 4.2.1 §2.4 (4-IP 边界表) 端口契约表完整化: `fabric.vram_ctrl_out ↔ vram_ctrl.fabric_in`,Fabric2VramCtrlBundle 字段定义 + `set_vram_target` setter 桥接路径
- [ ] 4.2.2 §7.3 (JSON 层级约束) 退役字段说明完整化: 4 个 `routing_enabled` 字段移除 + Strangler gate 取代 + ADR-DGPU-10 引用

### 4.3 openspec validate + archive

- [ ] 4.3.1 `openspec validate dgpu-realip-pin-contract --strict` → PASS (G1)
- [ ] 4.3.2 `openspec validate --all` → 全部 spec PASS (Phase C 后 baseline + 新增 dgpu-4ip-port-contract)
- [ ] 4.3.3 `openspec archive dgpu-realip-pin-contract --yes` → change dir 移至 archive/

### 4.4 final tag

- [ ] 4.4.1 `git tag -a dgpu-realip-pin-contract-completion -m "..."` (per PR handoff)

### 4.5 全量验证

- [ ] 4.5.1 `cmake --build build -j$(nproc)` → 退出码 0 (14 文件全编译)
- [ ] 4.5.2 `ctest --test-dir build --output-on-failure -j4` → 全 PASS (含 dlopen_minimal_soc + ctest cases)
- [ ] 4.5.3 `./build/bin/cpptlm_tests "[realip][pin-contract]"` → ≥4 cases PASS (G2)
- [ ] 4.5.4 `./build/bin/cpptlm_tests "[pin-contract]"` → ≥4 cases PASS (双标签)
- [ ] 4.5.5 `./build/bin/cpptlm_tests "[realip][p3]"` → ≥9 cases PASS (G5)
- [ ] 4.5.6 `./build/bin/cpptlm_tests "[pcie]"` → ≥433 / 36682 cases PASS (G6)
- [ ] 4.5.7 `./build/bin/cpptlm_tests "[fabric]"` → 7 cases PASS (Phase C 后基线)
- [ ] 4.5.8 `./build/bin/cpptlm_tests "[dgpu_soc_timing]"` → 37 cases PASS (MUST NOT 1 锚点)
- [ ] 4.5.9 `strings build/bin/cpptlm_tests | grep -c "Strangler gate"` = 2 (G7 invariant)
- [ ] 4.5.10 4 frozen header 零 diff (`git diff --stat` 验证 `cpptlm_emulator.h` / `pcie_endpoint_tlm.h` / `pcie_display_device.hh` / `pcie_bundles_tlm.hh` 0 修改) (G8)
- [ ] 4.5.11 23 ABI 零新增 (`git diff --stat include/abi/cpptlm_emulator.h` 仅 frozen 0 diff)
- [ ] 4.5.12 commit: `docs(plan): dgpu-realip-pin-contract Status Update + archive`

### Commit 4 验收 (per spec §3 + G1-G8)

- [ ] AGENTS.md KEY INVARIANTS 新增 legacy flag 退役条目
- [ ] `docs/designs/dgpu-soc/architecture.md` §2.4/§7.3 完整化
- [ ] `openspec validate dgpu-realip-pin-contract --strict` PASS
- [ ] `openspec archive` 完成 (change dir 移至 archive/)
- [ ] `dgpu-realip-pin-contract-completion` tag 落 (commit 4 final)

---

## 8 项 Acceptance Gate (基线, per handoff §11)

| Gate | 验证 | 基线 |
|------|------|------|
| **G1** | `openspec validate dgpu-realip-pin-contract --strict` PASS | Commit 4 |
| **G2** | `[pin-contract]` 标签 ≥4 cases PASS (2 文件, **双标签** `[realip][pin-contract]` per handoff §9.2 #10) | Commit 3 |
| **G3** | DGpuBoard 7 legacy flag **编译期删除** (新测试 SFINAE 静态断言) | Commit 3 |
| **G4** | `Fabric2VramCtrlBundle` 端口契约代码化 (`static_assert` 编译期验证, per `port_contract_asserts.hh`) | Commit 1 + 3 |
| **G5** | 4-IP topology `[realip][p3]` ≥9 cases 0 回归 (per Phase C 后基线) | Commit 1/2/3 |
| **G6** | `[pcie]` 全链路 ≥433 / 36682 cases 0 回归 (per Phase C 后基线) | Commit 1/2/3 |
| **G7** | Strangler gate = 2 (per `strings build/bin/cpptlm_tests | grep -c "Strangler gate"`, dgpu_board_shell.cc:336-343 保留) | Commit 1/2 |
| **G8** | 4 frozen header 零 diff + 23 ABI 零新增 (per AGENTS.md 冻结面) | Commit 1/2/3 |

---

## 风险与缓解 (重写版)

| Risk | 等级 | 缓解 |
|------|:----:|------|
| DGpuBoard 误删 `framebuffer_size_`/`vram_storage_` 等业务 setter (与 ADR-DGPU-05 冲突) | 🔴 高 | **保留清单显式**: 锁定 framebuffer_size_/vram_size_/vram_storage_/attach_vram_for_testing,只删 routing flag |
| `Fabric2VramCtrlBundle` 字段定义错误,与 CrossbarTLM::CacheReqBundle 不一致 | 🟡 中 | 字段对齐 cache_bundles_tlm.hh:30-46, TDD 5 步先写测试, `[pin-contract]` 编译期 + 运行时验证 |
| DGpuBoard::mmio_read/write 简化破坏 Strangler gate | 🔴 高 | Strangler gate 保留编译期 + controller_ip_ 注入 + facade verdict 与 expected mirror 比对, test_dgpu_board_no_legacy_flags 静态断言字符串存在 |
| JSON 顶层 routing_enabled 字段删除破坏既有 dgpu_soc_with_pcie_ip.json (per cpptlm-p2-integration-unblock base) | 🟡 中 | routing_enabled 字段已 default=false + ADR-DGPU-10 锁定 root cause 4 防劫持, 删除后 JSON 顶层字段未消费, validateConfig 不会失败 |
| set_vram_target 程序化桥接失效 (Phase 8 HB/RC 先例不适用 4-IP composite) | 🟡 中 | TDD 5 步: test_realip_pin_contract round-trip fabric→vram_ctrl (经 JSON 装配 + set_vram_target + tick), 失败可回滚到 `[realip][p3]` GREEN 基线 |
| static_assert 编译期契约触发 DGpuBoard 编译失败 | 🟡 中 | static_assert 仅在 port_contract_asserts.hh include 时启用, 旧测试不 include, 0 回归 |
| frozen header 触碰 (per AGENTS.md cpptlm_emulator.h 冻结) | 🔴 高 | git diff 严格 verify: 4 frozen header 零 diff, 23 ABI 零新增, G8 显式 gate |
| OpenSpec Proposed 季度 KPI 触顶 (本 change 后 2/3, 仍合规) | 🟢 低 | 当前 baseline 1/3 (cpptlm-p2-integration-unblock),本 change 后 2/3, 2026 Q4 复审前合规 |
| DGpuBoard::init() 顺序 Inv-2 扩展 (per cpptlm-p2-integration-unblock D8): set_vram_target 必须在 framebuffer 分配后 | 🟡 中 | bind_memory_backings() 扩展: vram_ctrl->set_backing_view() → fabric->set_vram_target(vram_ctrl) 顺序, 失败时 runtime_error |
| mmio_self_drain_enabled 是否也属 legacy (误删) | 🟢 低 | spec §3 Requirement #2 Scenario #3 显式锁定: drain 优化保留, 不在删除范围 |
| 现有 `[pcie]` 433/36682 cases 回归风险 | 🔴 高 | TDD 5 步先测试: `[realip][p3]` 9/122 → `[pcie]` 433/36682 基线, 任何回归立即 git revert + Phase D 二次 review |

---

## 时间线 (修订)

| Commit | 内容 | 估时 |
|:------:|------|:----:|
| 1 | Fabric2VramCtrlBundle + port_contract_asserts + 端口扩展 | 3-4 天 |
| 2 | DGpuBoard 7 flag 删除 + mmio_read/write 简化 + JSON 退役 + 文档同步 | 2-3 天 |
| 3 | 2 新测试 + 双标签验证 | 2 天 |
| 4 | 文档同步 + AGENTS.md + archive + 验证 + tag | 1 天 |
| **总计** | — | **8-10 天 ≈ 2 周单人** (含 Phase C 后基线 + Strangler gate 保护 + frozen header 0 diff gate) |

---

## 维护

**Owner**: CppTLM Team (Sisyphus)
**状态**: 📋 Tasks — Phase D 边界收紧 (legacy flag 删除 + 端口契约代码化)
**起点**: `dgpu-realip-refactor-completion` tag (commit `26583da7`, 2026-10-07)
**目标 tag**: `dgpu-realip-pin-contract-completion`
**关键路径**:
1. Commit 1: 类型扩展 (Fabric2VramCtrlBundle + port_contract_asserts)
2. Commit 2: legacy flag 删除 (顺序, 每步验证回归)
3. Commit 3: 双标签测试 (防 vacuous pass)
4. Commit 4: 文档同步 + archive + 验证 + tag

---

## 关联

- **proposal**: [`proposal.md`](proposal.md)
- **design**: [`design.md`](design.md) (D1-D7 决策 + 风险表)
- **spec**: [`specs/dgpu-4ip-port-contract/spec.md`](specs/dgpu-4ip-port-contract/spec.md) (4 ADDED Requirements)
- **前置**: `cpptlm-dgpu-realip-refactor` (archived, Phase C 完成 2026-10-07)
- **并行**: `cpptlm-p2-integration-unblock` (已 Proposed, 异步推进) — 共享 dgpu-4ip-port-contract capability 上下游
- **P2 主计划** (ArchForge): `docs/roadmap/phase9-p2-cp-attach-via-axi.md`
- **架构文档**: `docs/designs/dgpu-soc/architecture.md` §2.4 (4-IP 边界表) + §7.3 (JSON 层级约束)
- **设计 ADR**: ADR-DGPU-05/06/07/08/10
- **AGENTS.md**: KEY INVARIANTS (framebuffer 自动分配 + OpenSpec Proposed ≤3 KPI) + DOC HYGIENE (ArchForge 跨仓约束)
