# Spec: cpptlm-stage-1-4-2-1

> **状态**: 🔄 Proposed v1.1 (2027-02-09) — MODIFIED scenarios reflect MVP 现实
> **修订 (v1.0 → v1.1)**: 4 场景措辞调整为实现实际行为（spec.md 与 design.md §0.2 不变量对齐）

## ADDED Requirements

### Requirement: PCIe PM Capability (Stage 1.4)

The system MUST implement `PciePowerState` transitions (D0/D3hot) + PMCSR write intercept + INV-A MMIO gating for D3hot state.

#### Scenario: D0 to D3hot via PMCSR write
- **WHEN** driver writes `PMCSR[1:0] = 11b` (D3hot) on D0 device
- **THEN** device transitions to D3hot state
- **AND** subsequent tick() drops MMIO (cfg + BAR) writes (MVP: INV-A gate 范围; PCI spec D3hot cfg-access 要求放宽见 §follow-up)

#### Scenario: D0 PMCSR reserved values ignored
- **WHEN** driver writes `PMCSR[1:0] = 01b or 10b` (D1/D2 reserved)
- **THEN** device stays in D0 (PCIe spec: unsupported state writes ignored)

#### Scenario: ASPM L1 auto-entry after idle
- **WHEN** `enable_aspm(L1)` and idle >= 4000 cycles
- **THEN** link enters L1 state automatically

#### Scenario: ASPM L0s→L0 exit honors INV-B latency (on_traffic path)
- **WHEN** device in L0s and `on_traffic()` called
- **THEN** exit_pending_ set
- **AND** state returns to L0 after exactly L0S_EXIT_LATENCY (=4) cycles
- **AND** `exit_low_power()` direct call bypasses latency (pre-existing API; immediate transition)

### Requirement: P2P DMA Routing + ACS (Stage 2.1)

The system MUST support Peer-to-Peer DMA with explicit ACS rejection semantics (INV-D: never silent fallback).

#### Scenario: P2P DMA rejected by ACS (MVP default-deny)
- **WHEN** `p2p_dma_route(src_bdf, dst_bdf, addr, len)` with valid distinct BDFs
- **THEN** returns `P2PResult{BLOCKED_BY_ACS}` (MVP: ACS strict-deny; SUCCESS path deferred)

#### Scenario: P2P invalid BDF returns NO_ROUTE explicitly (INV-D)
- **WHEN** `src_bdf=0` or `dst_bdf=0` or `src_bdf==dst_bdf`
- **THEN** returns `P2PResult{NO_ROUTE}` (never silent; explicit error code)

### Requirement: Resizable BAR Capability (Stage 2.1)

The system MUST implement Resizable BAR state machine with INV-C sequence (disable → reprogram → enable).

#### Scenario: BAR size adjustment via disable→reprogram→enable
- **WHEN** driver calls `disable()` → `reprogram_size(new_size)` → `enable()`
- **THEN** state transitions Disabled → Programming → Enabled
- **AND** `size_bytes()` returns new_size after enable
- **AND** `reprogram_size` rejected when Enabled (INV-C)
- **AND** `enable` rejected without prior reprogram
- (NOTE: PcieEndpointIP integration + MMIO mapping update deferred to next PR; current impl provides standalone state machine)

## Out of Scope (明确边界 — MVP 限制)

| 项 | 排除理由 |
|----|---------|
| **D3cold 完整建模**（Vcc 物理移除） | TLM 不可模拟；spec.md 原"can wake to D0" 改为 "tick() drops cfg+BAR writes" |
| **P2P DMA SUCCESS 路径**（ACS-pass 时数据转发） | MVP: ACS 默认严格拒绝；SUCCESS 实现留待下一 PR |
| **Resizable BAR → MMIO 映射**（bar_store_ 更新） | 当前仅提供独立状态机；PcieEndpointIP 集成 deferred |
| **ASP exit-low-power direct call → 延迟倒计时** | 直调绕过倒计时（pre-existing API 行为）；仅 on_traffic() 路径走倒计时（INV-B） |
| **PcieEndpointIP D3hot cfg 写回 D0**（cfg 配置空间访问保留） | 当前 INV-A gate 含 cfg 路径，与 PCIe spec 略有偏离；下一 PR 修复 |
| **PM Cap 控制域 JSON-driven** | 当前 control=0x0013 写死 |
| **ACS Extended Cap (id=0x000D) 完整实现** | 当前用标准 cap id=0x0D 占位 |
| `sr_iov.{initial_vfs, total_vfs, num_vfs}` JSON 消费 | runtime NUM_PORTS 重构不在本 change 范围 |
| `transaction_layer.bar_sizes` / `sr_iov.vf_bar*_size` JSON 消费 | BAR window 模型后续 PR |

## Cross-References

- foundation spec: `openspec/specs/cpptlm-pcie-ep-foundation/spec.md`
- 2027-02-09 archive (predecessor): `openspec/specs/cpptlm-pcie-endpoint-ip-json-config/spec.md`
- 上游 `stage-1-3-sdma` (已 ship)
- 下游 UE: `ue-stage-1-4-2-1-extensions` (待启动)
- ABI: 不动；23 ABI 冻结边界 + pcie_endpoint_tlm.h 冻结
