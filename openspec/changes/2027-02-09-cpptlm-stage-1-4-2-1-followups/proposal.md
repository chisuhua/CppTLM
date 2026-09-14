# Proposal: cpptlm-stage-1-4-2-1-followups — PCIe 6 项 follow-up 推进

> **状态**: 🔄 Proposed (2027-02-09)
> **工期**: 1.0-1.5 周 (分阶段, 6 子任务)
> **优先级**: P1 (P2 中含 INV-A 严重功能 bug)
> **前置依赖**: `2026-09-10-cpptlm-stage-1-4-2-1` (已 archive ✅ 2026-09-14)
> **架构性变更**: ✅ 走 OpenSpec 流程 (per AGENTS.md anti-pattern "架构性变更未走 OpenSpec")

---

## Why

2026-09-10 stage-1-4-2-1 change 已实现 PM Cap / Power state / ASPM / P2P / ACS / Resizable BAR 等 6 个组件 + 4 不变量 (INV-A/B/C/D)。Oracle 复评指出 **3 项 Quick 阻断** 已在 archive 前修复（UAF re-fetch / test_aspm lifetime / design-spec 漂移）。

但复评同时识别 **6 项 follow-up 需后续 PR 推进**，从严重功能 bug 到可选扩展。当前零债务状态不可持续 — 这些是真实的产品债。一次性集中处理以避免多次小 change 引起散乱。

## What Changes

### 子任务 1: 🔴 INV-A gate 收窄到 BAR only（严重 PCIe spec 违反，0.5d）

当前 `PcieEndpointIP::tick()` line 233-235 在 mmio_gated=true 时早返回，**包括 cfg 写路径**。导致 D3hot 下 driver 无法经 AXI 数据路径写 PMCSR 回 D0（设备楔死至 FLR）。

**PCIe spec §5.4.2 D3hot**: 配置空间访问必须保留（这就是 "hot" 的含义）。

修复:
- `tick()` 内 cfg/BAR 判别前**先看地址**；仅 `is_cfg=false` 时 gate
- cfg 写经 PMCSR 写触发状态机回到 D0
- BAR 写返回 UR / Master Abort（而非静默丢弃）—— 通过 `Axi4StreamAdapter::slave_resp(wresp with bresp=2 (DECERR))` 通知 host

### 子任务 2: 🟡 P2P DMA SUCCESS 路径（功能缺失，0.3d）

当前 `p2p_dma_route` MVP 恒返 `BLOCKED_BY_ACS`。扩展为支持可选 ACS 策略：
- `PcieBypassMux` 持有全局 ACS 策略表（per BDF pair）
- 默认 `StrictACS=true` 维持当前 MVP 行为（向后兼容）
- 新增 `set_acs_strict(bdf_pair, false)` API 允许特定 BDF pair 通过 ACS

### 子任务 3: 🟡 ResizableBar → PcieEndpointIP 集成（孤立状态机，0.3d）

当前 `ResizableBar` 是独立 header-only 状态机。集成:
- `PcieEndpointIP` 持有 6 个 `ResizableBar` 实例（对应 6 个 BAR slots）
- `enable()` 时同步更新 `bar_store_` 关联区域大小校验（拒绝越界）
- 测试覆盖 BAR size 改变后越界访问拒绝

### 子任务 4: 🟢 PM Cap 控制域 JSON-driven（可选扩展，0.1d）

当前 `attach_composition()` line 28 hardcode `control=0x0013`（version 3 + D3hot）。扩展:
- `params.phy_digital.pm_cap_control` (default 0x0013) JSON-driven
- 与 2027-02-09 JSON config 模式一致

### 子任务 5: 🟢 ACS Extended Cap 完整实现（PCIe id=0x000D，0.2d）

当前用标准 cap id=0x0D 占位。完整实现 PCIe Extended Cap:
- Extended Cap header (next + version + id)
- ACS Cap Reg (8 bit 含义: V/B/R/UR/CR/EF)
- ACS Control Reg (对应 enable/disable bits)
- 单独 .cc 文件实现 Extended Cap walk 协议

### 子任务 6: 🟢 PMCSR PWS=1/2 保留值忽略（极小 bug，<0.1d）

当前 `set_power_state(static_cast<PciePowerState>(new_pws))` line 28 — PWS=1 (D1) 或 2 (D2) 产生非法枚举值。修复:
- mask: `if (new_pws == 0 || new_pws == 3) set_power_state(...); else /* ignore, keep current */`
- 已有 `last_pmcsr_pws_` sentinel `0xFFFFu` 自动保护（D1=1 会触发 callback 但幂等）

## Impact

- **修改**:
  - `src/tlm/pcie/pcie_endpoint_ip.cc` (INV-A gate 收窄 + ResizableBar 集成)
  - `src/tlm/pcie/pcie_bypass_mux_p2p.hh` (SUCCESS 路径, inline header 不变扩展为新函数)
  - `src/tlm/gpu/pcie_config_space_mvp.cc` (PMCSR mask + ACS Extended Cap)
  - `include/tlm/pcie/pcie_endpoint_ip.hh` (新成员 + set_acs_strict helper 暴露)
  - `include/tlm/pcie/pcie_resizable_bar.hh` (与 PcieEndpointIP 集成 hook)
  - `openspec/specs/cpptlm-stage-1-4-2-1/spec.md` (MODIFIED scenarios 反映修复)
- **不改**:
  - 23 ABI 冻结头
  - `pcie_endpoint_tlm.h` (deprecated 4-port)
  - JSON config 2027-02-09 archive 接口
- **新增**:
  - `src/tlm/pcie/pcie_acs_extended_cap.cc` (子任务 5)
  - `test/test_pcie_power_state_cfg_access.cc` (INV-A cfg 路径回归)
  - `test/test_p2p_dma_success.cc` (子任务 2 SUCCESS)
  - `test/test_resizable_bar_integration.cc` (子任务 3)
  - `test/test_pm_cap_control_json.cc` (子任务 4)
  - `test/test_acs_extended_cap.cc` (子任务 5)

## Oracle 复评

详见 `specs/cpptlm-stage-1-4-2-1-followups/spec.md` 的 6 个 Scenario。验收标准引用 2026-09-10 archive 的 INV-A/B/C/D 不变量，并扩展为 4 个 MODIFIED Scenarios。

复评证据清单:
1. `git diff --stat` 范围限于上述 6 修改 + 6 新增 + spec.md
2. 新增 5 测试文件全 PASS; 既有 `[pcie]` 全绿 (零回归)
3. `openspec validate cpptlm-stage-1-4-2-1-followups --strict` PASS
4. 全 [pcie] 回归 320+ cases / 19000+ assertions 全绿

## refs

- **主 spec**: `openspec/specs/cpptlm-stage-1-4-2-1/spec.md` (2026-09-14 archive, 13 follow-up items)
- **predecessor**: `openspec/changes/archive/2026-09-14-2026-09-10-cpptlm-stage-1-4-2-1/`
- **predecessor-predecessor**: `openspec/specs/cpptlm-pcie-endpoint-ip-json-config/spec.md` (2027-02-09 archive)
- **foundation**: `openspec/specs/cpptlm-pcie-ep-foundation/spec.md`
- **架构文档**: `docs/architecture/14-pcie-ip-microarchitecture.md`
- **ABI**: 不动

## Timeline

| 子任务 | 工作量 | 累计 | 紧急 |
|---|---|---|---|
| 1 INV-A gate 收窄 | 0.5d | 0.5d | 🔴 |
| 2 P2P SUCCESS 路径 | 0.3d | 0.8d | 🟡 |
| 3 ResizableBar 集成 | 0.3d | 1.1d | 🟡 |
| 4 PM Cap JSON control | 0.1d | 1.2d | 🟢 |
| 5 ACS Extended Cap | 0.2d | 1.4d | 🟢 |
| 6 PMCSR mask | <0.1d | 1.5d | 🟢 |
| 复评 + archive | 0.5d | 2.0d | — |

总计: 1.5-2.0 周（含文档与复评 buffer）。

## Appendix: 优先级排序理由

**子任务 1 (INV-A) 优先**: 现有 D3hot 路径会楔死设备（device stuck until FLR）。违反 PCIe spec D3hot 语义。一旦跑真实 dGPU 软件 bring-up 即触发。

**子任务 6 (PWS mask) 同步做**: 单行改动，工作量 < 0.1d。

**子任务 4 (PM Cap JSON) 与子任务 1 同步**: INV-A 修复涉及 cfg 路径，与 PM Cap JSON 配置自然关联。
