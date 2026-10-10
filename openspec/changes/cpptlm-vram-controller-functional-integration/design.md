# Design — cpptlm-vram-controller-functional-integration v1.0

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/vram-controller-functional-mode/spec.md`](specs/vram-controller-functional-mode/spec.md)
> **目标 tag**: `cpptlm-vram-controller-functional-integration-completion`
> **估时**: **3-5d** (TDD 5 阶段)
> **Frozen surface**: 23 ABI 签名零 diff + 4 frozen headers 零 diff (per ADR-DGPU-08 + AGENTS.md H3)
> **依赖**: 无 (`include/tlm/vram_controller_tlm.hh` 已存在)
>
> **🔗 Cross-Reference (per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3, 2026-10-10)**:
> 本 change 引用 `docs/designs/dgpu-soc/architecture.md §1.4.1` VramControllerTLM 描述已修订为 **"as Phase 0 Change A target state"** 限定词 (避免使用现在时误导读者认为 API 已存在; per Oracle finding 1, 修 O-7)。本 change v0.5.2 修订时**仅需维持 §1.4.1 限定词**, 无需修改 §1.4.1 文字层。详见 [cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md §O-7](../../cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md)。

---

## 总体设计

### 核心变更思路

**目标**: 在 minimal_v1 SoC 的 functional-mode 仿真模式下, SDMA 写 VRAM 的数据路径从 **"直 memcpy"** 改为 **"经 VramControllerTLM functional-mode 接口"**, 与 timing-mode (`dgpu_soc_timing_v1.json`) 的 8 模块拓扑保持一致（同一 VramControllerTLM 类，配置仅 `mode` 字段差异）.

**不变量 (Locked)**:
- 与 timing-mode **共用同一 `VramControllerTLM` 类** (per §0.1 原则 4 + ADR-DGPU-11 H2)
- 23 ABI 签名零变更
- 4 frozen headers 零 diff

### 数据路径示意 (functional-mode 实施后)

```
                    ┌─────────────────────────────────────────────┐
                    │ minimal_v1 functional-mode data path         │
                    │                                             │
   SDMA H2D:        │  sdma.mem_out (PcieTlpBundle MEM_WRITE per v1.8 H1)        │
                    │       │                                     │
                    │       ▼                                     │
                    │  pcie_memory.handle_slave_port(port=0)       │
                    │       │                                     │
                    │       │ detect simulation_mode == functional  │
                    │       │ detect vram_ctrl_ != nullptr          │
                    │       ▼                                     │
                    │  vram_ctrl->handle_functional_request(       │
                    │      addr, size, is_write, data)             │
                    │   (= VramController functional-zero-delay)   │
                    │       │                                     │
                    │       │ controller-protocol-mapped path      │
                    │       │ (per VramControllerMode::            │
                    │       │  functional_zero_delay)              │
                    │       ▼                                     │
                    │  memcpy(vram_storage_.get()+addr, data, sz) │
                    │       │                                     │
                    │       ▼                                     │
                    │  MEM_WRITE_RESP (data=OK or SLVERR)         │
                    │       │                                     │
                    │       ▼                                     │
                    │  sdma.req_in[2] (slot 2, PORT_MEM_OUT)       │
                    └─────────────────────────────────────────────┘
```

### 与 timing-mode 数据路径对比

```
minimal_v1 functional-mode  (本 change 实施后):
  sdma → pcie_memory → vram_ctrl (functional_zero_delay) → memcpy(vram_storage_)

dgpu_soc_timing_v1 timing-mode (per cpptlm-dgpu-soc-timing-mvp v0.2 已存在):
  sdma → pcie_memory → vram_ctrl (timing_cycle_approximate, latency=cycle) → memcpy(vram_storage_) + cycle accounting
```

唯一差异: `set_mode(VramControllerMode)` 参数 (1 个枚举字段), 内部实现路径 99% 复用.

---

## API 设计

### 1. VramControllerTLM::set_mode() (新增, backward compat)

**文件**: `include/tlm/vram_controller_tlm.hh`

```cpp
namespace tlm::vram {

// v1.0 新增: VramController 工作模式 (per §0.1 原则 4 + ADR-DGPU-11 H2 互斥)
enum class VramControllerMode : uint8_t {
    FunctionalZeroDelay       = 0,  // functional-mode: zero-delay 但协议真实
    TimingCycleApproximate   = 1   // timing-mode: cycle-approximate + bandwidth accounting
};

// 默认 mode = TimingCycleApproximate (与既有 timing-mode 行为保持 100% 一致)
class VramControllerTLM : public MemoryTLM {
public:
    // ... 既有 API 不变 ...
    
    // v1.0 新增: 设置模式 (per §0.1 §X 业务定位要求)
    void set_mode(VramControllerMode m) noexcept { mode_ = m; }
    VramControllerMode mode() const noexcept { return mode_; }
    
    // ... set_backing_view 等既有 API 不变 ...
private:
    VramControllerMode mode_ = VramControllerMode::TimingCycleApproximate;
};

}  // namespace tlm::vram
```

**接口铁律**:
- 仅加 **2 个新签名** (`set_mode()` + `mode()` access + enum)
- 不修改既有任何 `VramControllerTLM` 签名 (per §0.1 原则 4)
- 不动 `MemoryTLM` 基类
- 默认值 `TimingCycleApproximate` 保证 backward compat

### 2. dgpu_board_shell.cc::bind_memory_backings() 注入扩展

**文件**: `src/tlm/gpu/dgpu_board_shell.cc` 第 730-744 行 (T23C 第 4 路, 现有注释保留 + 加 mode 设置)

```cpp
// T23C (Phase C 2026-10-07) + Change A: 注入 vram_ctrl (per ADR-DGPU-05 单一真源 +
//                                                §0.1 原则 4 functional-mode 集成)
VramControllerTLM* vram_ctrl_seen = nullptr;
if (auto* vram_ctrl =
        dynamic_cast<VramControllerTLM*>(soc_->getInternalInstance("vram_ctrl"))) {
    vram_ctrl->set_backing_view(framebuffer_ptr_, framebuffer_size_);
    
    // [v1.0 NEW] 根据 minimal_v1 的 simulation_mode 设置 VRAM controller 工作模式
    // 默认为 functional_zero_delay (per §0.1 原则 4 + ADR-DGPU-11 H2)
    if (simulation_mode() == SimulationMode::Functional) {
        vram_ctrl->set_mode(tlm::vram::VramControllerMode::FunctionalZeroDelay);
    }
    // 否则保持默认 TimingCycleApproximate (timing-mode 拓扑)
    
    vram_ctrl_seen = vram_ctrl;
    DPRINTF(MODULE,
            "[DGpuBoard] T23C+A: vram_ctrl backing injected + mode=%s (ptr=%p, size=%llu)\n",
            vram_ctrl->mode() == tlm::vram::VramControllerMode::FunctionalZeroDelay 
                ? "FunctionalZeroDelay" : "TimingCycleApproximate",
            static_cast<void*>(framebuffer_ptr_),
            static_cast<unsigned long long>(framebuffer_size_));
}
```

**实现铁律**:
- 仅有 `vram_ctrl` 注入时加 `set_mode` 调用 (非功能模式默认)
- **0 新增 try/catch** (per existing T23C 错误处理)
- 保留 `vram_ctrl_seen` 流程 (后续变更可能引用)

### 3. PcieMemoryDevice::handle_slave_port() 内部集成 VramController

**文件**: `src/tlm/gpu/pcie_memory_device.cc::handle_slave_port()` (or header)

**注意**: 这部分在 D-AXI v1.4 中已经是 `memcpy(vram_storage_, ...)` 直调. 本 change 修改为 dynamic 注入模式:

```cpp
// handle_slave_port 内 (port=0 SDMA MEM_WRITE 路径) - v1.0 扩展
case PcieTlpBundle::MEM_WRITE: {
    if (vram_controller_) {
        // [v1.0 NEW] 委托给 VramController (functional-mode 路径)
        // vram_controller_ 由 set_vram_controller() 注入 (per design §3.4)
        tlm::vram::VramRequest req{
            .addr = pte_addr,
            .size = size,
            .is_write = true,
            .data = data_ptr,
        };
        auto [rc, latency] = vram_controller_->handle_functional_request(req);
        if (rc != 0) {
            // SLVERR propagation per D-AXI v1.4 B9
            resp.resp.write(1);  // SLVERR
        } else {
            resp.resp.write(0);  // OK
        }
    } else {
        // [既有] 直 memcpy fallback (per D-AXI v1.4 v1.0 兼容)
        std::memcpy(vram_storage_.get() + pte_addr, data_ptr, size);
        resp.resp.write(0);
    }
    break;
}
```

**实现铁律**:
- **保留 fallback 路径**: 若 `vram_controller_ == nullptr` (legacy minimal_v1 without vram_ctrl in JSON), 走原 memcpy (per D-AXI v1.4 backward compat)
- `set_vram_controller()` setter 由 `DGpuBoard::bind_memory_backings()` 调用 (新增 step)
- SLVERR propagation 必须保留 (per D-AXI v1.4 B9)
- **Port 1 (GMMU PTE 读) [v0.5.1 修订 per Oracle C1 协调 spec.md]**: GMMU 走 VramControllerTLM 专用 functional port (per §0.1 Gap I v0.5 裁决: NVIDIA H100 GMMU dedicated memory client 模式), 不再经 PcieMemoryDevice port 1 (PcieMemoryDevice port 1 保留但 GMMU 不使用)

### 4. JSON 顶层 schema 变化

**文件**: `configs/dgpu_soc_minimal_v1.json` line 9-62

```diff
 {
   "name": "dgpu_soc_minimal_v1",
   "description": "...",
+  "simulation_mode": "functional",   // [v1.0 NEW] per ADR-DGPU-11 H2 互斥
   "display_routing_enabled": false,
   "storage_routing_enabled": true,
   "gmmu_routing_enabled": true,
   "memory_routing_enabled": true,
   "framebuffer_size_bytes": 16777216,
   "modules": [
     {
       "name": "soc",
       "type": "DGpuSoc",
       "modules": [
+        {
+          "name": "vram_ctrl",                           // [v1.0 NEW] per ADR-DGPU-11 §6.4 E5
+          "type": "VramControllerTLM",
+          "params": {
+            "operating_frequency_hz": 1000000000,         // 1 GHz per docs/designs/dgpu-soc/timing-mode.md §2.4
+            "bandwidth_gbps": 32                         // bytes/cycle (per design.md §5.2 历史用法)
+            // [v0.5.1 A-H1 修订] dram_mode 字段删除 (dual source of truth 消除)
+            // mode 配置统一由 DGpuBoard::simulation_mode() 字段控制 (顶层 simulation_mode="functional")
+          }
+        },
         {
           "name": "pcie_ep",
           ...
         },
```

**注**:
- `simulation_mode: "functional"` 顶层 (per ADR-DGPU-11 §6.4 schema)
- `vram_ctrl` 与既有 `pcie_ep`/`pcie_ctrl`/`sdma`/`gmmu` 同级 (per §0.1 §2.1 6 模块拓扑 → 7 模块拓扑 additive)
- `dram_mode` 是 VramController 专属 mode 配置 (与 tlm 类 enum 字段一一对应)

### 5. JSON 注册表变化

**文件**: `include/modules.hh` 或 `include/modules_cluster.hh` (Phase 2 已注册)

确认 `VramControllerTLM` 已在 `REGISTER_MODULE` 注册 (per `cpptlm-dgpu-soc-timing-mvp` v0.2). 不需注册表改动.

### 6. JSON schema validator 变化

**文件**: `scripts/test/docs_sync_check.sh` 同步检查 + `cpptlm/topo/` Python validator

确认 `simulation_mode` + `vram_ctrl` 字段加 schema validator; 缺字段时启动 fail fast.

---

## Backward Compatibility 策略

### backward compat 表

| 场景 | 行为 |
|------|------|
| **既存 timing-mode JSON** (`dgpu_soc_timing_v1.json`) | ✅ 0 触动: 顶层不加 `simulation_mode`, `vram_ctrl` 已存在 (per timing-mvp Phase 4 T4); 默认 mode = TimingCycleApproximate → 行为不变 |
| **既存 minimal_v1 JSON** (本 change 实施后无 `vram_ctrl`) | ✅ backward compat: `vram_controller_` setter nullptr, handle_slave_port 走 legacy memcpy 路径; 测试不变 |
| **既存 minimal_v1 JSON** (本 change 实施后**有** `vram_ctrl` + `simulation_mode:"functional"`) | ✅ functional 数据路径激活, **新行为** |
| **既有 23 ABI 测试** | ✅ 0 触动 (序列化层在 abi 之后) |
| **既有 4 frozen headers** | ✅ 0 diff (VramControllerTLM 头文件是 isolated change, 不与 frozen surface 重叠) |

### 兼容性验证矩阵

| 测试套 | 行为预期 | Status |
|--------|---------|--------|
| `[minimal_dgpu_soc]` (12 cases / 66 assertions) | minored: 数据路径 internal routing 变化, 接口签名不变 | ✅ 保持 Pass |
| `[driver_visible]` (2 cases / 29 assertions) | 同上 | ✅ 保持 Pass |
| `[dgpu_soc_timing]` (37 cases / 12024 assertions) | timing-mode 独立 JSON 不变 | ✅ 保持 Pass |
| `[pcie-memory]` (17 cases / 48 assertions) | 直接 path 不变 (port 1 GMMU 不经 VramController) | ✅ 保持 Pass |
| `[realip]` (22 cases / 193 assertions) | per GlueLayer 测试, 不直接触及 vram 数据路径 | ✅ 保持 Pass |

---

## 关键设计不变式

- **H-B1 (Hard Invariant)**: VramControllerTLM 类不破坏既有 signature (除新增 2 个 enum/signature)
- **H-B2 (Hard Invariant)**: functional-mode 与 timing-mode 数据路径**共享同一类**, 仅 mode 字段差异 (per ADR-DGPU-11 H2)
- **H-B3 (Hard Invariant)**: 23 ABI 签名零变更 (per ADR-088 §D5)
- **H-B4 (Hard Invariant)**: 4 frozen headers 零 diff (per AGENTS.md H3)
- **H-B5 (Hard Invariant)**: SLVERR propagation 保留 (per D-AXI v1.4 B9)
- **H-B6 (Hard Invariant) [v0.5.1 修订 per Oracle C1 协调]**: GMMU PTE fetch 走 VramControllerTLM 专用 functional port (per §0.1 Gap I v0.5 裁决), handle_slave_port port 1 保留 (其他用) 但 GMMU PTE 不再走
- **H-B7 (Hard Invariant)**: `simulation_mode: "functional"` 字段缺省 = `functional` (向后兼容)

---

## 测试策略

### 测试分层 (per AGENTS.md 测试规范)

1. **单元测试 (`[vram_controller][functional]`)**:
   - `set_mode(FunctionalZeroDelay)` 后 handle_request latency = 0 cycles
   - `set_mode(TimingCycleApproximate)` 后 handle_request latency = kOperatingFrequencyHz × bandwidth_gbps^-1 cycles (per docs/designs/dgpu-soc/timing-mode.md §2.4)

2. **集成测试 (`[vram_controller][functional][integration]`)**:
   - SDMA H2D → VramController functional-mode → vram_storage_ 字节级一致性
   - GMMU PTE 读路径 port=1 **不**经过 VramController (per H-B6)
   - Port=0 SDMA 读路径 经过 VramController (per H-B2)

3. **E2E 测试 (`[minimal_dgpu_soc][vram_controller]`)**:
   - driver 闭环: BAR2 mmio_write(BAR2, pte_off) → vram_storage_ 字节写入
   - driver 经 BAR1 backdoor 读同一地址: byte-level match (per D-AXI v1.4 B7 不变)

### 关键测试 invocation

```bash
# Phase 0 T5 / T5 RED → GREEN
./build/bin/cpptlm_tests "[vram_controller][functional]" --reporter compact
./build/bin/cpptlm_tests "[vram_controller][functional][integration]" --reporter compact
./build/bin/cpptlm_tests "[minimal_dgpu_soc][vram_controller]" --reporter compact

# 回归锁 (per DoD)
./build/bin/cpptlm_tests "[minimal_dgpu_soc]" --reporter compact
./build/bin/cpptlm_tests "[driver_visible]" --reporter compact
./build/bin/cpptlm_tests "[dgpu_soc_timing]" --reporter compact
./build/bin/cpptlm_tests "[pcie-memory]" --reporter compact
./build/bin/cpptlm_tests "[realip]" --reporter compact
```

---

## 风险与缓解 (设计层)

### R1: VramController functional-mode 集成可能影响 SDMA 性能断言 (`[dgpu_soc_timing]`)

**影响**: 测试时间退化 (functional-mode 路径经 controller 比直 memcpy 略慢, 跨 cycle 同步可能 stall)

**缓解 (per §0.2 关键风险 + ADR-DGPU-11 H2 互斥)**:
- 通过 `simulation_mode` JSON 字段**互斥** (functional 与 timing 不同时存在)
- 本 change 仅在 `simulation_mode == "functional"` 的 minimal_v1 中生效
- timing-mode JSON (`dgpu_soc_timing_v1.json`) 不加 `simulation_mode` 字段, 默认走 TimingCycleApproximate (与既有一致)

### R2: VramController class 修改可能破坏既有 signature (per Frozen Surface)

**影响**: frozen header diff, AGENTS.md H3 violation

**缓解**:
- VramControllerTLM **不是** frozen header (per §0.1 §X 锁)
- 既有 4 frozen headers (`pcie_endpoint_tlm.h`/`cpptlm_emulator.h`/`pcie_bundles_tlm.hh`/`pcie_display_device.hh`) + 1 quasi (`pcie_endpoint_ip.hh`, additive only) **0 触动** (v0.5.1 与全文口径统一)
- 新增 enum + 2 个函数 signature 在 `vram_controller_tlm.hh` 内 (非 frozen)
- Change A 提交前**双重检查** `git diff HEAD -- include/abi/ include/tlm/pcie/pcie_endpoint_ip.hh include/tlm/gpu/pcie_endpoint_tlm.h include/tlm/pcie/pcie_bundles_tlm.hh include/tlm/gpu/pcie_display_device.hh` 必须为空

### R3: handle_slave_port 内部插桩可能误触发 SLVERR latch 导致 driver 读到垃圾

**影响**: 数据损坏, driver 静默错

**缓解 (per D-AXI v1.4 B9 + 测试断言)**:
- `handle_functional_request()` 返回值检查 (0=OK, ≠0=SLVERR)
- 失败置 `resp.resp.write(1)` (SLVERR latch 既存)
- 测试覆盖 (per 测试策略): `[vram_controller][functional][slverr]` 注入 1 case 验证 SLVERR propagation

### R4: JSON schema validator 缺 `simulation_mode` 字段检测

**影响**: 启动 fail 或默默退化

**缓解**:
- `simulation_mode` 字段**默认 = "functional"** (per backward compat H-B7)
- 显式注入字段 + 启动 validate (validator 脚本加字段必填检查)

### R5: Phase 4 T4 timing-mode VramController 既有测试 (`[dgpu_soc_timing]`) 与 functional-mode 路径冲突

**影响**: 测试间数据 race 或 SLVERR 串扰

**缓解**:
- simulation_mode 字段是 **top-level** (per ADR-DGPU-11 §6.4); 两种 mode **不会**同时加载
- 不同 test_case 启动不同 `dgpu_soc_*_v1.json` 配置 (test_setup_load_config())
- `dgpu_soc_timing_v1.json` 不加 `simulation_mode`, 默认 TimingCycleApproximate
- `dgpu_soc_minimal_v1.json` 实施后**显式**加 `simulation_mode: "functional"`

---

## 文件清单 (实施追踪)

| 文件 | 类型 | 估时 |
|------|------|------|
| `include/tlm/vram_controller_tlm.hh` | 修改 (+~30 行 enum + 2 signature) | 0.5d |
| `src/tlm/gpu/pcie_memory_device.cc` | 修改 (~30 行 handle_slave_port 内部集成) | 1d |
| `src/tlm/gpu/dgpu_board_shell.cc::bind_memory_backings()` | 修改 (~10 行 mode 设置 + setter 调用) | 0.5d |
| `configs/dgpu_soc_minimal_v1.json` | 修改 (+~10 行 vram_ctrl 节点 + simulation_mode) | 0.25d |
| `test/test_vram_controller_functional.cc` | 新建 (~150 行 `[vram_controller][functional]` 5-10 cases) | 1d |
| `scripts/test/docs_sync_check.sh` | 修改 (+JSON schema validator) | 0.25d |
| `docs/implementation/2026-10-09-vram-controller-functional-integration.md` | 新增 (~50 行 TDD 5-step 实施笔记) | 0.25d |
| `docs/designs/dgpu-soc/architecture.md` §0.2 Gap G3 行 ✅ 修订 | 文档同步 | 0.25d |
| `AGENTS.md` KEY INVARIANTS VramController functional-mode 锁定 | 文档同步 | 0.25d |
| **总计** | | **~3.5d** (含 1d buffer = 4.5d) |

---

## 相关文档

- 业务定位锁定: `docs/designs/dgpu-soc/architecture.md` §0.1
- 总体实施计划: `docs/designs/dgpu-soc/architecture.md` §0.2
- 修复路线图方向: `docs/designs/dgpu-soc/architecture.md` §0.1.1
- D-AXI 实施笔记: `docs/pcie/driver-visible-minimal-soc.md` §7 D3 seam
- timing-mode ADR: `docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md`
- Frozen surface 锁定: `AGENTS.md` H3 不动约束 + `ADR-DGPU-08` §D5
- VramController 既有实现: `include/tlm/vram_controller_tlm.hh` (per `cpptlm-dgpu-soc-timing-mvp` Phase 4 T4)

---

**Owner**: CppTLM Team (Sisyphus)
**最后更新**: 2026-10-09 (v1.0 起草, Phase 0 Change A 启动)
