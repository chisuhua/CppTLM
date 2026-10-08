# dgpu-realip-pin-contract: Design (legacy flag 删除 + chip-internal 端口契约代码化)

> **配套**: [`proposal.md`](proposal.md) · [`specs/dgpu-4ip-port-contract/spec.md`](specs/dgpu-4ip-port-contract/spec.md) · [`tasks.md`](tasks.md)
> **目标**: 收紧 Phase C 后 4-IP topology 边界,从"行规 flag 拼装"切到"端口契约 + compile-time port map"
> **前置**: `cpptlm-dgpu-realip-refactor` ✅ (Phase C tag `dgpu-realip-refactor-completion`, 2026-10-07, 7 子任务 + T28 gate 全 PASS)
> **修订**: 起点 7 个 legacy flag (per `dgpu_board_shell.hh:75-117,317-348`),新增 `Fabric2VramCtrlBundle` chip-internal wire-protocol

---

## 1. Context

### 1.1 当前状态 (Phase C 后基线, 代码事实)

Phase C 完成 4-IP topology 装配 (`dgpu_soc_v2_realip_p2.json` + 6 commits per `dgpu-realip-refactor-completion` tag)。代码事实:

**DGpuBoard 7 个 legacy flag** (`dgpu_board_shell.hh:75-117,317-348`):
```cpp
// 1. PciePath enum (Legacy/Tlp/AxiBypass/Mock, 4 values)
enum class PciePath : uint8_t { Legacy = 0, Tlp, AxiBypass, Mock };
PciePath pcie_path_ = PciePath::Legacy;
[[nodiscard]] PciePath pcie_path() const noexcept { return pcie_path_; }

// 2-5. 5 routing_enabled flags
bool display_routing_enabled_ = false;
bool memory_routing_enabled_ = false;
bool storage_routing_enabled_ = false;
bool gmmu_routing_enabled_ = false;
// + 5 setter + 5 accessor methods

// 6. mmio_self_drain_enabled (drain 优化, **保留**)
bool mmio_self_drain_enabled{true};

// 7. controller_ip_ (P2 PR-A facade 必要, **保留**)
tlm::pcie::PcieControllerIP* controller_ip_ = nullptr;
```

**DGpuBoard::mmio_read/write 6 个 if 分支** (`dgpu_board_shell.cc:308-422` mmio_read + `:433-466` mmio_write):
- Strangler gate (line 320-343 / `:433-466`): facade verdict 与 expected mirror 比对 → std::abort (锁 facade 路径)
- Display fast-path (line 349-355 / `:468-472`): `display_routing_enabled && bar==0` → `pcie_ep()->display_device().mmio_read()`
- Memory fast-path (line 367-371): `memory_routing_enabled && bar==2` → `pcie_ep()->memory_device().memory_read()`
- Storage fast-path (line 358-365): `storage_routing_enabled && bar==1` → `memcpy(framebuffer_ptr_ + offset, buf, len)`
- Self-drain (line 387): `mmio_self_drain_enabled` → `drain_injection_queue()`
- Legacy path: `mmio_regs_[{bar, offset}]` map fallback (line 372-417, 仅 `controller_ip_ == nullptr` 时)

**GpuInternalFabric / VramControllerTLM 4-IP 数据路径未代码化** (per handoff §9.2 #9 D3/D4 deferred work):
- `fabric.vram_ctrl_out` 端口: **不存在**
- `vram_ctrl.fabric_in` 端口: **不存在**
- `Fabric2VramCtrlBundle`: **不存在** (chip-internal wire-protocol 未定义)
- 数据路径仅存在于 `dgpu_soc_v2_realip_p2.json:_note` 注释中,编译期无校验

**JSON 顶层 4 个 routing_enabled 字段** (`dgpu_soc_v2_realip_p2.json:5-7`):
```json
"simulation_mode": "timing",      // timing-mode 保留 (per Phase 4 timing-mode MVP)
"memory_routing_enabled": true,   // D2 v1.1 防劫持, default=true
"storage_routing_enabled": true,
"gmmu_routing_enabled": true,
"framebuffer_size_bytes": 16777216,
"display_routing_enabled": true   // D1 v1.1.1 防劫持, default=true
```

DGpuBoard::load_soc_config (`dgpu_board_shell.cc:108-126`) 加载这些字段,但**实际数据路径已被 `PcieControllerIP::match_bar` + `DGpuBoard::set_pcie_controller_ip` 取代**(per PR-B Phase Gate B1/B2/B3 全部 RESOLVED)。Flag 保留仅是为"向后兼容 legacy path"测试,但 legacy path 不再被任何 JSON 引用。

### 1.2 端口契约未代码化 (root cause)

Phase 8 M1 Strangler gate (`dgpu_board_shell.cc:336-343` facade verdict divergence std::abort) 锁 facade 路径,但**协议契约没有静态保证**:
- `PcieControllerIP::match_bar` 返回 `BarTarget` enum (per `pcie_controller_ip.hh`)
- `PcieEndpointIP::bar_store_` 是 `unordered_map<BarStoreKey, uint64_t>` (per `pcie_endpoint_ip.hh:321`)
- 两者契约**只在 T25B 测试 + Strangler gate 中体现**,编译期无检查

`GpuInternalFabric::simulate_instantiate` (per T25A.1) 调 `attach_crossbar()` 自动 wire inner CrossbarTLM,但**fabric→vram_ctrl 数据路径未实例化**(per handoff §9.2 #9: "fabric→vram_ctrl 链属 D3/D4, 本期不验证 end-to-end 数据路径, 仅 T23C 验证对象级 read() 契约")。

### 1.3 DGpuBoard mmio_read/write 简化机会

**现状** (`dgpu_board_shell.cc:308-422` mmio_read):
```
mmio_read(bar, offset, buf, len):
  if (controller_ip_) { Strangler gate }           # 保留
  if (display_routing_enabled && bar==0) {...}    # 删 (D1 fast-path 不依赖 flag)
  if (storage_routing_enabled && bar==1) {...}     # 删 (per B2, framebuffer 直接 memcpy)
  if (memory_routing_enabled && bar==2) {...}      # 删 (D2 fast-path 不依赖 flag)
  if (mmio_self_drain_enabled) { drain }            # 保留
  // legacy path (PciePath::Legacy): mmio_regs_ fallback
```

**简化后** (~40 行):
```
mmio_read(bar, offset, buf, len):
  if (controller_ip_) { Strangler gate }
  // D1: bar==0 → pcie_ep()->display_device().mmio_read() (per P0.5-landing)
  // B2: bar==1 → memcpy(framebuffer_ptr_ + offset, buf, len)
  // D2: bar==2 → pcie_ep()->memory_device().memory_read()
  if (mmio_self_drain_enabled) { drain }
  // 移除 mmio_regs_ legacy path (后续 pin-contract 二次清理)
```

净删除 ~80 行 (6 if 分支 + 5 flag 加载 + 5 flag 成员 + 5 setter + 5 accessor + 1 enum + 1 accessor + 1 path_str 加载 = 24 行声明 + ~50 行 .cc 代码),DGpuBoard.cc 从 ~750 行 → ~620 行 (-17%)。

---

## 2. Goals / Non-Goals (Change A scope, 2026-10-08 resplit)

### 2.1 Goals (Change A in-scope)

- **G1**: `Fabric2VramCtrlBundle` chip-internal wire-protocol 代码化,字段对齐 chip-internal 数据路径 (8 字段, max width 64-bit)
- **G2**: `port_contract_asserts.hh` 提供 `BUNDLE_FIELD_WIDTH` macro (Momus 修订: 删 PORT_IS_INITIATOR/TARGET,基于不存在的 `chstream::ChStreamInitiatorPort<Bundle>` 模板 — 实际 `cpptlm::ChStreamInitiatorPort` 是非模板类)
- **G3**: `GpuInternalFabric::set_vram_target(VramControllerTLM*)` + `VramControllerTLM::set_fabric_source(GpuInternalFabric*)` 双向 setter 桥接 (Momus 修订: 不加 `ChStreamInitiatorPort<Bundle>` 成员,改 setter-only 模式,运行时 round-trip 验证)
- **G4**: `DGpuBoard::bind_memory_backings()` 末尾追加 programmatic bridge,通过 `soc_->getInternalInstance("fabric")` + `"vram_ctrl"` 装配 (Momus 修订: 实际 API 而非 `modules_by_name`;fabric 模块名 `"fabric"` 而非 `"gpu_internal_fabric"`, per `configs/dgpu_soc_v2_realip_p2.json:22`)
- **G5**: `ch_uint<W>` 加 `static constexpr unsigned width = W;` (per `include/bundles/cpphdl_types.hh` A.2 修订) — 让 `BUNDLE_FIELD_WIDTH` macro 可用 `decltype(&BundleType::Member)::width`
- **G6**: 3 个新测试用例 (TEST_CASE #1 字段宽度 + #2 accessors + #3 round-trip) **双标签** `[realip][pin-contract]` (per handoff §9.2 #10 防 vacuous pass)
- **G7**: 4 frozen header 零 diff + 23 ABI 零新增 (per AGENTS.md 冻结面, Change A 不动)
- **G8**: 编译期契约生效 (BUNDLE_FIELD_WIDTH static_assert 触发 + 测试 #1 8 字段宽度 PASS)

### 2.2 Out of Change A scope (deferred to Change B/C/D)

- **NG1**: DGpuBoard 7 个 legacy flag 删除 (PciePath + 5 routing_enabled + mmio_self_drain 保留) — **Change C** (Oracle C6 修复,前置 = Change B 完成且 acceptance 0 回归)
- **NG2**: JSON 顶层 4 个 `routing_enabled` 字段退役 — **Change C**
- **NG3**: `mmio_read/write` 简化 (Strangler gate 保留) — **Change C**
- **NG4**: DGpuBoard::init() 重构 — Change A 仅加 programmatic bridge, init() 流程不动
- **NG5**: `match_bar` BAR0 offset-aware 子窗口修复 (GMMU/display 区分) — **Change B** (Oracle C2 修复)
- **NG6**: `DGpuBoard::init` 装配 `attach_endpoint` + `set_pcie_controller_ip` (facade 接管数据路径) — **Change B** (Oracle C1 修复)
- **NG7**: `test_pcie_bypass_tlp_data_path.cc` 编译失败修复 — **Change B** (Oracle C5 修复)
- **NG8**: 真实 RTL 桥接 (CppHDL/HybridCache) — P13 单独 change
- **NG9**: PcieAxiAdapter/Axi4CacheAdapter 实施 — cpptlm-p2-integration-unblock 异步推进
- **NG10**: DGpuBoard 其他 setter (set_display_fb_size / set_dma_translate_callback / set_irq_callback 等) — 业务保留
- **NG11**: legacy `framebuffer_size_bytes` 顶层 override — P0.5-landing 保留
- **NG12**: 4 frozen header (cpptlm_emulator.h / pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh) — 0 修改 (per AGENTS.md 冻结面)
- **NG13**: 23 ABI 新增 — P5 冻结 (per ADR-088 §D5)
- **NG14**: ModuleFactory strict_types + ConnectionResolver WARN + ParamResolver hook — **Change D** (Oracle A1/A2/A3/A4 修复)

---

## 3. Decisions

### D1: Fabric2VramCtrlBundle 字段定义 — **对齐 cache_bundles_tlm.hh + 新增 chip_select[8]**

**决策**: `Fabric2VramCtrlBundle` 8 字段 (`include/bundles/dgpu_fabric_bundles_tlm.hh`, 新建):

| 字段 | 宽 | 语义 | 对齐 |
|------|-----|------|------|
| `address` | `[64]` | chip-internal 物理地址 | `CacheReqBundle.address[64]` |
| `size` | `[8]` | 字节数 (per TLP MRd/MWr size) | `CacheReqBundle.size[8]` |
| `is_write` | `[1]` | true=MWr, false=MRd | `CacheReqBundle.is_write` |
| `data` | `[64]` | 写数据 (MRd 不使用) | `CacheReqBundle.data[64]` |
| `transaction_id` | `[32]` | fabric→vram_ctrl 事务 ID (chip-internal) | 新增 (chip-internal namespace) |
| `resp_data` | `[64]` | 读响应数据 | `CacheRespBundle.data[64]` |
| `error_code` | `[8]` | 0=OK, 1=DECERR, 2=SLVERR | `CacheRespBundle.error_code[8]` |
| `chip_select` | `[8]` | chip select (per multi-die future, 当前 default 0) | 新增 |

**理由**:
- **复用 cache_bundles_tlm 字段语义**: 8 字段中 6 字段直接对齐 `CacheReqBundle`/`CacheRespBundle`,RTL team 1:1 对照硅片 wire
- **transaction_id[32] 而非 [64]**: chip-internal namespace 用 32-bit 足够 (chip 内部 2^32 事务),**不与 PCIe 16-bit transaction_id 混淆**
- **chip_select[8] 新增**: 未来 multi-die 扩展用,当前 default 0 (单 chip),**预留接口零开销**
- **resp_data 与 data 分离**: chip-internal wire-protocol 要求 read response 单独路径 (per AXI R channel, 数据 + resp_code)

**考虑过的方案**:
- ❌ 复用 `CacheReqBundle`/`CacheRespBundle`: 字段语义不同 (chip-internal vs PCIe),混淆调用方
- ❌ 复用 `Axi4Bundle` (16-bit AXI): chip-internal 与 AXI 协议不同 (AXI 是 PCIe 协议,chip-internal 是 SoC 内部), 不应暴露 AXI 复杂度
- ❌ 复用 `PcieTlpBundle`: PcieTlpBundle 是 PCIe TLP packet 编码 (header + data + ECRC),**不是 chip-internal wire**

### D2: port_contract_asserts.hh static_assert 框架 — **基于 std::is_base_of_v + sizeof 检测**

**决策**: 新建 `include/utils/port_contract_asserts.hh` (~80 行),提供 `BUNDLE_FIELD_WIDTH` macro (Momus 修订后):

```cpp
// 1. bundle 字段宽契约 (编译期校验, 唯一保留 macro)
#define BUNDLE_FIELD_WIDTH(bundle, field, expected_width) \
    static_assert(decltype(std::declval<bundle>().field)::width == expected_width, \
                  "bundle field width mismatch")
```

> **Momus 修订 (2026-10-08)**: 原计划设计 4 个 macro 含 `PORT_IS_INITIATOR` / `PORT_IS_TARGET` (基于 `chstream::ChStreamInitiatorPort<Bundle>` / `ChStreamTargetPort<Bundle>` 模板类型),经实证全仓**不存在**该模板 — 实际 `cpptlm::ChStreamInitiatorPort` 是非模板类(继承 `SimObject`),无 `<Bundle>` 模板参数。原 macro 编译失败,已删除。改用 `BUNDLE_FIELD_WIDTH` 单 macro + setter-only 桥接 + 运行时 round-trip 测试 (`test_realip_pin_contract.cc` TEST_CASE #3) 组合,达相同契约保证效果。

**理由**:
- **编译期检查**: `BUNDLE_FIELD_WIDTH` 在编译时强制 bundle 字段宽对齐,RTL team 改字段时编译失败暴露
- **零运行时开销**: 全部编译期 `static_assert`,无运行时分支
- **可复用**: 未来 GPGPU/SoC 集成需要同样契约时,直接 include 即可

**考虑过的方案**:
- ❌ runtime check (dynamic_cast): 运行时开销 + 错误延迟到部署
- ❌ 单元测试 + 运行时 assert: 编译期无法保证契约 (CI 漏过风险)
- ❌ C++20 `concept` (per T25A.1 设计提案): 编译期契约,**但 concept 需要 C++20**,目前 C++17 baseline

### D3: set_vram_target 程序化桥接 — **Phase 8 HB/RC 先例 + cpptlm-p2-integration-unblock D7**

**决策**: `GpuInternalFabric::set_vram_target(VramControllerTLM*)` setter 在 JSON 装配后程序化桥接 (不依赖 JSON 声明式端口):

```cpp
// GpuInternalFabric 扩展:
void GpuInternalFabric::set_vram_target(VramControllerTLM* vram) noexcept {
    vram_ctrl_ = vram;
}

void GpuInternalFabric::simulate_instantiate(const nlohmann::json& cfg) {
    SimModule::simulate_instantiate(cfg);  // base: instantiateAll + 递归子 SimModule
    // T25A.1: attach_crossbar (现有)
    if (crossbar_ != nullptr) return;
    if (cfg.contains("modules")) {
        for (const auto& child_cfg : cfg["modules"]) {
            if (child_cfg.value("name", "") == "crossbar") {
                attach_crossbar(dynamic_cast<CrossbarTLM*>(
                    internal_factory->getInstance("crossbar")));
                break;
            }
        }
    }
    // T23C NEW: program 化桥接 vram_ctrl_ (per cpptlm-p2-integration-unblock D7)
    // 不依赖 JSON 声明式 fabric.vram_ctrl_out → vram_ctrl.fabric_in 端口连接
    // (避免 19 §14.2.3 静默丢弃)
}
```

**DGpuBoard::bind_memory_backings (line 708-746) 扩展**:
```cpp
// 已有: if (auto* vram_ctrl = ...) vram_ctrl->set_backing_view(...)
// NEW: 在 vram_ctrl 注入后,program 化桥接到 fabric
if (auto* fabric = dynamic_cast<GpuInternalFabric*>(
        soc_->getInternalInstance("fabric"))) {
    fabric->set_vram_target(vram_ctrl);
}
```

**理由**:
- **Phase 8 HB/RC tick 转发先例**: `HostBypassTLM::tick()` 在内部程序化转发 EP↔HB AXI 流 (per `host_bypass_tlm.cc`),不依赖 JSON 声明式端口
- **cpptlm-p2-integration-unblock design D7**: 锁定 "JSON 接线被 19 §14.2.3 静默丢弃",程序化桥接是唯一可靠路径
- **idempotent**: 多次调 set_vram_target 安全 (vram_ctrl_ 指针覆盖,失效场景是 vram_ctrl 释放)

**考虑过的方案**:
- ❌ JSON 声明式 `fabric.vram_ctrl_out → vram_ctrl.fabric_in` 端口连接: 会被 19 §14.2.3 静默丢弃 (per cpptlm-p2-integration-unblock design D7 锁定)
- ❌ extend JSON schema 加 `programmatic_bridges` 字段: 增加 schema 复杂度,与 Phase 8 HB/RC 既有模式不一致
- ❌ 通过 ChStreamAdapterFactory 注入: 适配器用于 AXI 协议转换,不适用 chip-internal wire

### D4: DGpuBoard legacy flag 删除 — **PciePath enum + 5 routing_enabled 全删, controller_ip + mmio_self_drain 保留**

**决策**: 删除 7 个 legacy flag (`PciePath` enum + `pcie_path_` + 5 `routing_enabled_*_` 成员 + 5 setter + 5 accessor + 1 `pcie_path()` accessor),保留 3 个 (controller_ip_, mmio_self_drain_enabled, mmio_regs_ legacy path)。

**删除顺序** (per `dgpu_board_shell.hh:75-117` 行号):
1. **Line 75-77**: `PciePath` enum (`Legacy`/`Tlp`/`AxiBypass`/`Mock`) + line 86 `pcie_path()` accessor + line 317 `pcie_path_` 成员
2. **Line 103-108**: `set_display_routing_enabled` + `display_routing_enabled()` + `display_routing_enabled_`
3. **Line 111-116**: `set_memory_routing_enabled` + `memory_routing_enabled()` + `memory_routing_enabled_`
4. **Line 268-269**: `set_storage_routing_enabled` + `storage_routing_enabled()` + `storage_routing_enabled_`
5. **Line 270-271**: `set_gmmu_routing_enabled` + `gmmu_routing_enabled()` + `gmmu_routing_enabled_`
6. **Line 30-39** (`.cc`): `pcie_path_str` JSON 加载逻辑删除
7. **Line 108-126** (`.cc`): 5 `routing_enabled_*` JSON 加载删除

**mmio_read/write 简化** (`dgpu_board_shell.cc:308-422` mmio_read):
- 删除 line 349-355 (display fast-path `if` + `display_routing_enabled_ && bar==0` → 改为无条件 `if (bar==0)`)
- 删除 line 358-365 (storage fast-path `if` + `storage_routing_enabled_ && bar==1` → 改为无条件 `if (bar==1)`)
- 删除 line 367-371 (memory fast-path `if` + `memory_routing_enabled_ && bar==2` → 改为无条件 `if (bar==2)`)
- **保留** line 320-343 Strangler gate (`controller_ip_ != nullptr` if)
- **保留** line 387 self-drain (`mmio_self_drain_enabled` if)

**理由**:
- **PciePath enum 4 values 全是 legacy**: `Legacy` 路径已被 `controller_ip_->match_bar` facade 取代 (per PR-B Phase Gate),`Tlp`/`AxiBypass`/`Mock` 仅是配置选项,实际数据路径不存在
- **5 routing_enabled flag 是"防劫持"补丁** (per ADR-DGPU-10): 锁特定 path (display/memory/storage/gmmu),**已被 Strangler gate 锁的 facade verdict 取代**
- **mmio_self_drain_enabled 不是 legacy**: drain 性能优化,默认 true,**保留**
- **controller_ip_ 不是 legacy**: P2 PR-A facade 必要,**保留**

**考虑过的方案**:
- ❌ 全部保留 flag (向后兼容): 永久技术债 + RTL team 翻译成本
- ❌ 全部删除 + mmio_regs_ 也删: 范围过大,破坏 PciePath::Legacy 测试基线
- ❌ 删除 PciePath 但保留 routing_enabled: 半删,既不彻底也不清晰

### D5: JSON 顶层 routing_enabled 字段退役 — **4 字段全部移除**

**决策**: `dgpu_soc_v2_realip_p2.json` 顶层 4 个 `routing_enabled` 字段退役:

```json
{
  "name": "dgpu_soc_v2_realip_p2",
  "version": "2-realip-p3-pin",  // Phase C → Phase D 版本递增
  "_note": "T23B/T25A.1/T25B/T26 (Phase C) + dgpu-realip-pin-contract (Phase D): ...",
  "modules": [...]  // 无变化 (4-IP topology 已固化)
  // 删除: simulation_mode, memory_routing_enabled, storage_routing_enabled,
  //       gmmu_routing_enabled, framebuffer_size_bytes, display_routing_enabled
}
```

**保留**:
- `simulation_mode` (timing/functional): Phase 4 timing-mode MVP 字段,功能保留
- `framebuffer_size_bytes`: P0.5-landing 字段,DGpuBoard 自动消费,**保留**
- `params.device_id` / `params.quantum_cycles`: DGpuBoard 业务参数,**保留**

**理由**:
- **5 routing_enabled 字段已无 consumer**: DGpuBoard::load_soc_config line 108-126 加载后**仅供已删除的 fast-path if 分支使用**,删除 if 分支后字段 dead
- **JSON schema 简化**: 顶层字段从 ~10 个 → ~5 个,与 P0.5-landing `dgpu_soc_minimal_v1.json` 字段数对齐
- **`validateConfig` 无破坏**: `module_factory_validate.cc:200-217` 仅检查 `modules`/`connections` 顶层必填字段,routing_enabled 字段是 optional

**考虑过的方案**:
- ❌ 保留字段但加 deprecation 注释: 字段在 JSON 中仍存在 → Schema validator 仍消费 → 永久债务
- ❌ 移到 dgpu_soc_v2_realip_p2.json:_note 注释: 不消费字段,等同删除
- ❌ 引入新 `pin_contract_enabled` 字段: 与本次简化目标相反

### D6: 测试策略 — **2 个独立测试文件 + 双标签 + 编译期 static_assert**

**决策**:
- `test/test_realip_pin_contract.cc` — `Fabric2VramCtrlBundle` 字段定义 + `static_assert` 编译期检查 + `set_vram_target` 程序化桥接 round-trip + 4-IP topology 集成测试 (3 cases, 双标签)
- `test/test_dgpu_board_no_legacy_flags.cc` — DGpuBoard public API 编译期不含已删除 5 routing flag (SFINAE 探测) + Strangler gate 字符串存在 (1 case, 双标签)
- **所有 2 文件必须双标签** `[realip][pin-contract]` (per handoff §9.2 #10 防 vacuous pass)

**理由**:
- **职责单一**: test #1 验证端口契约 (forward), test #2 验证 legacy flag 删除 (backward)
- **双标签**: `[realip]` 过滤 Phase C 拓扑 + `[pin-contract]` 过滤本 change, G2/G6 floor 数学成立
- **static_assert 编译期**: DGpuBoard 误加回 `display_routing_enabled_` 时编译失败 (CI 早期发现)

### D7: Strangler gate 保留 — **flag 字符串 + facade verdict 与 expected mirror 双层保护**

**决策**: Strangler gate (per `dgpu_board_shell.cc:336-343` mmio_read + `:433-466` mmio_write) 保留,作为**P2 PR-A facade 编译期保护**。

**保留机制**:
- **`controller_ip_ != nullptr` 触发**: 不依赖任何 routing_enabled flag
- **`controller_ip_->match_bar(bar, offset, target, sub_offset)` 返回 `BarTarget`**: 与 expected mirror 比对 (`dgpu_board_shell.cc:320-343`)
- **divergence 时 std::abort**: `dgpu_board_shell.cc:342-343` 显式 abort,锁 facade 行为
- **编译期字符串存在**: `"Strangler gate"` 字符串保留 (per `strings build/bin/cpptlm_tests | grep -c "Strangler gate" == 2`)

**理由**:
- **facade verdict 是 P2 PR-A 唯一 single source of truth**: 删除 routing_enabled flag 后,Strangler gate 仍是编译期保护
- **divergence 行为可观察**: std::abort 是强烈信号,避免 facade 静默失效
- **不依赖任何 flag**: Strangler gate 自身不消费 `display_routing_enabled_` 等 (per `dgpu_board_shell.cc:320-343` 实际逻辑)

---

## 4. Risks / Trade-offs

| # | 风险 | 等级 | 缓解 |
|---|------|:----:|------|
| 1 | DGpuBoard 误删 `framebuffer_size_` / `attach_vram_for_testing` 等业务 setter (与 ADR-DGPU-05 冲突) | 🔴 高 | **保留清单显式**: `framebuffer_size_`/`vram_size_`/`vram_storage_`/`attach_vram_for_testing` 锁定,**只删 routing flag** |
| 2 | `Fabric2VramCtrlBundle` 字段定义错误,与 CrossbarTLM::CacheReqBundle 协议不一致 | 🟡 中 | 字段对齐 `cache_bundles_tlm.hh:30-46`,TDD 5 步先写测试,`[pin-contract]` 编译期 + 运行时验证 |
| 3 | DGpuBoard::mmio_read/write 简化破坏 Strangler gate | 🔴 高 | Strangler gate 保留**编译期** + `controller_ip_` 注入 + facade verdict 与 expected mirror 比对,`test_dgpu_board_no_legacy_flags.cc` 静态断言字符串存在 |
| 4 | JSON 顶层 `routing_enabled` 字段删除破坏既有 `dgpu_soc_with_pcie_ip.json` (per cpptlm-p2-integration-unblock base) | 🟡 中 | `routing_enabled` 字段已 **default=false** + ADR-DGPU-10 锁定 root cause 4 防劫持,删除后 JSON 顶层字段未消费,`validateConfig` 不会失败 |
| 5 | `set_vram_target` 程序化桥接失效 (Phase 8 HB/RC 先例不适用 4-IP composite) | 🟡 中 | TDD 5 步:`test_realip_pin_contract.cc` round-trip fabric→vram_ctrl (经 JSON 装配 + set_vram_target + tick),失败可回滚到 `[realip][p3]` GREEN 基线 |
| 6 | `static_assert` 编译期契约触发 DGpuBoard 编译失败 | 🟡 中 | `static_assert` 仅在 `port_contract_asserts.hh` include 时启用,旧测试不 include,0 回归 |
| 7 | frozen header 触碰 (per AGENTS.md `cpptlm_emulator.h` 冻结) | 🔴 高 | git diff 严格 verify: 4 frozen header 零 diff, 23 ABI 零新增, G8 显式 gate |
| 8 | OpenSpec Proposed 季度 KPI 触顶 (本 change 后 2/3, 仍合规) | 🟢 低 | 当前 baseline 1/3 (cpptlm-p2-integration-unblock),本 change 后 2/3, 2026 Q4 复审前合规 |
| 9 | DGpuBoard::init() 顺序 Inv-2 扩展 (per cpptlm-p2-integration-unblock D8): set_vram_target 必须在 framebuffer 分配后 | 🟡 中 | `bind_memory_backings()` 扩展:`vram_ctrl->set_backing_view()` → `fabric->set_vram_target(vram_ctrl)` 顺序,失败时 `runtime_error` |
| 10 | `mmio_self_drain_enabled` 是否也属 legacy (误删) | 🟢 低 | spec §3 Requirement #2 Scenario #3 显式锁定: drain 优化保留,不在删除范围 |
| 11 | 现有 `[pcie]` 433/36682 cases 回归风险 | 🔴 高 | TDD 5 步先测试:`[realip][p3]` 9/122 → `[pcie]` 433/36682 基线,任何回归立即 `git revert` + Phase D 二次 review |

---

## 5. Migration Plan

### 5.1 实施顺序 (4 commit 结构, per handoff §5 time estimate)

```
Commit 1: Fabric2VramCtrlBundle + port_contract_asserts + 端口扩展
  - include/bundles/dgpu_fabric_bundles_tlm.hh (新建 ~60 行)
  - include/utils/port_contract_asserts.hh (新建 ~80 行)
  - include/chstream_register.hh (注册 bundle)
  - include/tlm/cluster/gpu_internal_fabric.hh (vram_ctrl_out 端口声明 + set_vram_target setter)
  - include/tlm/vram_controller_tlm.hh (fabric_in 端口扩展)
  - 验证: cpptlm_core 编译 PASS (无新功能,仅类型扩展)
  
Commit 2: DGpuBoard legacy flag 删除 + mmio_read/write 简化
  - include/tlm/gpu/dgpu_board_shell.hh (删 7 flag + 5 setter + 5 accessor + 1 enum + 1 accessor)
  - src/tlm/gpu/dgpu_board_shell.cc (删 6 if 分支 + pcie_path 加载 + 5 routing_enabled 加载)
  - configs/dgpu_soc_v2_realip_p2.json (删 4 routing_enabled 字段 + version 字段递增)
  - docs/designs/dgpu-soc/architecture.md §2.4/§7.3 同步
  - 验证: [pcie] 433/36682 + [realip][p3] 9/122 + Strangler gate=2 + docs_sync 0 missing
  
Commit 3: 2 个新测试文件 + 双标签
  - test/test_realip_pin_contract.cc (3 cases, 双标签 [realip][pin-contract])
  - test/test_dgpu_board_no_legacy_flags.cc (1 case, 双标签 [realip][pin-contract])
  - 验证: [realip][pin-contract] ≥4 cases PASS + [pin-contract] ≥4 cases PASS
  
Commit 4: 文档同步 + AGENTS.md + openspec archive
  - AGENTS.md KEY INVARIANTS 新增 legacy flag 退役条目
  - docs/designs/dgpu-soc/architecture.md §2.4 端口契约文档 + §7.3 JSON 退役字段说明
  - openspec validate dgpu-realip-pin-contract --strict
  - openspec archive dgpu-realip-pin-contract --yes
  - git tag -a dgpu-realip-pin-contract-completion
```

**为什么 4 commit 而非 3**:
- Commit 1 (类型扩展) 独立可回滚 — 端口契约代码化是**新增**能力,失败可单独回退
- Commit 2 (legacy flag 删除) 是**修改**能力,依赖 Commit 1 的端口契约 (Strangler gate 用 controller_ip_)
- Commit 3 (测试) 独立可回滚 — 测试失败不回退代码
- Commit 4 (文档 + archive) 异步,失败不影响代码

### 5.2 回滚策略 (per cpptlm-p2-integration-unblock §5.2 flag-gated 纪律)

- **Commit 2 严格 discipline**: DGpuBoard 7 flag 删除**默认**完成,**逐个**删除 (per D4 删除顺序 #1-7),每步跑 `[pcie]` + `[realip][p3]` + Strangler gate 验证,任一失败立即 revert 当前 flag
- **mmio_regs_ 保留**: legacy path 物理清理 (per NG1) 推迟到未来二次 pin-contract,本次只删 routing flag

### 5.3 时间线重估 (per handoff §5 估时 8-12d)

| Commit | 内容 | 估时 |
|:------:|------|:----:|
| 1 | Fabric2VramCtrlBundle + port_contract_asserts + 端口扩展 | 3-4 天 |
| 2 | DGpuBoard 7 flag 删除 + mmio_read/write 简化 + JSON 退役 + 文档同步 | 2-3 天 |
| 3 | 2 新测试 + 双标签验证 | 2 天 |
| 4 | AGENTS.md + archive + openspec validate + tag | 1 天 |
| **总计** | — | **8-10 天 ≈ 2 周单人** (含 Phase C 后基线 + Strangler gate 保护 + frozen header 0 diff gate) |

---

## 6. Open Questions

- **Q1**: `Fabric2VramCtrlBundle::chip_select[8]` 字段宽是否合理? (未来 multi-die 8 chip 足够,但 16 chip 可能需要 [16])
  - **当前答案**: [8] 足够 (Phase 5 spec 单 chip + 8 multi-die roadmap),未来 16+ 升级独立 change
- **Q2**: `set_vram_target` 是否需要 unset_vram_target (解绑) API?
  - **当前答案**: 不需要 (DGpuBoard::shutdown() 析构 fabric + vram_ctrl 一起释放),显式解绑 YAGNI
- **Q3**: `port_contract_asserts.hh` 是否需要 vendor-specific 扩展 (per-instance assertion)?
  - **当前答案**: 不需要 (4-IP 拓扑锁定),未来 GPGPU 集成独立 change
- **Q4**: mmio_regs_ 物理清理 (per NG1) 是否需要在 Phase D 二次清理?
  - **当前答案**: **是**. 本 change 只删 routing flag, mmio_regs_ (legacy path 物理 map) 推迟到 Phase D-2 (二次 pin-contract),per NG1

---

## 7. 关联

- **proposal**: [`proposal.md`](proposal.md)
- **spec**: [`specs/dgpu-4ip-port-contract/spec.md`](specs/dgpu-4ip-port-contract/spec.md) (3 ADDED Requirements: fabric-vram-ctrl-port-contract + dgpu-board-legacy-flag-retirement + dgpu-4ip-port-contract-test-coverage + json-routing-enabled-retirement)
- **前置**: `cpptlm-dgpu-realip-refactor` (archived, Phase C tag `dgpu-realip-refactor-completion`, 7 子任务 + T28 gate)
- **并行**: `cpptlm-p2-integration-unblock` (已 Proposed, 异步推进) — 共享 `dgpu-4ip-port-contract` capability 上下游
- **session handoff** (archived): `docs/superpowers/plans/archive/2026-10-07-dgpu-realip-refactor-phase-c-handoff.md` §9.2 #9 (fabric→vram_ctrl 链 D3/D4 deferred) → 本 change 解锁
- **原始 plan**: `docs/superpowers/plans/2026-10-06-dgpu-realip-refactor.md` (Phase C Status Update 落)
- **架构文档**: `docs/designs/dgpu-soc/architecture.md` §2.4 (4-IP 边界表) + §7.3 (JSON 层级约束)
- **设计 ADR**:
  - `ADR-DGPU-05-vram-storage-ownership.md` (framebuffer_ 单一真源)
  - `ADR-DGPU-06-axi-mem-bundle-boundary.md` (AxiMemBundle vs PcieTlpBundle 边界)
  - `ADR-DGPU-07-minimal-soc-evolution-seam.md` (D3 seam 插入点)
  - `ADR-DGPU-08-abi-freeze-policy.md` (23 ABI 冻结)
  - `ADR-DGPU-10` (legacy flag 防劫持 root cause 4 锁定)
- **AGENTS.md**: KEY INVARIANTS (framebuffer 自动分配条目 + OpenSpec Proposed ≤3 KPI) + DOC HYGIENE (ArchForge 跨仓约束)
