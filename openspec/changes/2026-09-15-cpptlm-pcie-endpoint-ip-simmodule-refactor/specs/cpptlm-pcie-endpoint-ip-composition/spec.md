# Spec: cpptlm-pcie-endpoint-ip-composition — PcieEndpointIP SimModule 组合模式

> **状态**: 🔄 Proposed v1.1 (2026-09-15 — Oracle 复审修订:R4 决策 C + Phase 1 PHY tick 休眠 + composite 单一所有权 + CPPTLM_TESTING 修复)
> **Delta type**: MODIFIED + ADDED Requirements（delta spec 位于 change 内 `specs/cpptlm-pcie-endpoint-ip-composition/`，归档时由 `openspec archive` 合并至 main spec `pcie-ip-microarch`）
> **Baseline spec**: `openspec/specs/pcie-ip-microarch/spec.md`（Phase 4-8 SR-IOV 17 端口基线）

---

## MODIFIED Requirements

### Requirement: PcieEndpointIP Composition Architecture（替换 §Composition 部分）

The system **MUST** model `PcieEndpointIP` as a `SimModule` (derived from `SimModule`, not `ChStreamModuleBase`) that holds a single composite `ChStreamModuleBase` child `PcieLinkPhyMuxTLM` internally, encapsulating the three strongly-coupled PCIe sub-modules (Link Layer + PHY Digital Ctrl + Bypass Mux) which **MUST NOT** be connected via Bundle/Port due to zero-cycle synchronous call requirements (INV-B / rate switch / 10-step cleanup atomicity).

#### Scenario: composite child internal construction order
- **WHEN** `PcieEndpointIP` is instantiated via `ModuleFactory::instantiateAll` with type `"PcieEndpointIP"` resolved from `getModuleRegistry()` (not `getObjectRegistry()`)
- **THEN** `simulate_instantiate(cfg)` is called on the EP instance
- **AND** the EP creates a single internal child `PcieLinkPhyMuxTLM` instance named `<ep_name>_lpm`
- **AND** the composite's private members are constructed in declaration order: `PcieLinkLayer link_` → `PciePhyDigitalCtrl phy_` → `PcieBypassMux mux_` (mux's `PcieLinkLayer*` set at construction)
- **AND** immediately after construction, `phy_.link_layer(&link_)` and `phy_.set_link_up(true)` and `mux_.set_phy_initialized(true)` are invoked (INV-1, encapsulating `pcie_endpoint_ip.cc:129-130` semantics)

#### Scenario: composite 17 ports reachable programmatically (NOT via outer JSON wiring)
- **WHEN** EP's `simulate_instantiate(cfg)` completes and the internal `PcieLinkPhyMuxTLM` child is instantiated via `internal_factory->instantiateAll(wrap)`
- **THEN** the composite's 17 TLP ports are reachable programmatically via `PcieEndpointIP::getInternalOutputPort("pcie_ep_lpm.resp_out[i]")` / `getInternalInputPort("pcie_ep_lpm.req_in[i]")` for `i ∈ [0, 16]` (the internal_factory Step 7 mirror + `SimModule::getInternalInstance` lookup, `sim_module.hh:140-146`)
- **AND** the EP **MUST NOT** register exposed-port mappings via `addOutputConfig` (the only exposed-port API in `sim_module.hh`, and only under `#ifdef CPPTLM_TESTING` at line 183-196, unusable in production builds)
- **AND** outer JSON declarative wiring of the form `connections: [{ "src": "...", "dst": "pcie_ep.pf0" }]` is an **explicit non-goal** of this change (R4 decision C) — the current `connection_resolver.cc:47-98` performs only single-level exposed-port resolution against the outer factory's `object_instances`, which does not contain `pcie_ep_lpm` (the composite lives in EP's `internal_factory`); such connections would be silently dropped
- **AND** Phase 8's data path (429327d `HostBypassTLM/RC::tick()` programmatic AXI channel forwarding) continues to work unchanged; the existing `pcie_ep.axi_slave_in` / `pcie_ep.axi_master_out` connections in `examples/dgpu_soc_with_pcie_ip.json` become no-ops after Phase 2 (EP enters `module_instances`), and this behavior **MUST** be locked by a test (`test_axislavein_bridge_path_intact`) to prevent silent regression

#### Scenario: EP tick ordering override (INV-3)
- **WHEN** `PcieEndpointIP::tick()` is called per simulation cycle
- **THEN** in **Phase 1** the EP::tick path is **unchanged** (AXI slave → EP's own 17 adapters → `PcieLinkLayer::for_endpoint(ep_name)->tick()`); the composite's `tick()` (sequencing `phy_.tick() → link_.tick() → mux_` → 17 TLP adapters) exists as the Phase 2 contract but has **no caller in Phase 1** (Oracle decision a: zero behavior change, preserving the "14+ test files zero-modification" promise, including externally-driven `phy->tick()` fixtures such as `test_aspm.cc`)
- **AND** in **Phase 2** the tick order is **strictly**:
  1. AXI slave request processing via `PcieAxiAdapter::for_endpoint(ep_name)` (preserves `pcie_endpoint_ip.cc:255-364` cfg/BAR dispatch semantics)
  2. composite child `PcieLinkPhyMuxTLM::tick()` (which internally sequences `phy_.tick() → link_.tick() → mux_` → 17 TLP adapters)
- **AND** the order **MUST** be explicitly coded in the EP override; the system **MUST NOT** rely on `SimModule::tick()` default unordered_map iteration (`include/core/sim_module.hh:166`)
- **AND** Phase 2's PHY tick driver migration (from external test fixtures to EP::tick) **MUST** be budgeted as an explicit Phase 2 task (tasks 2.1.5: grep and update all external `phy->tick()` drivers)

#### Scenario: static registry API preserved as shim (INV-6)
- **WHEN** any caller invokes `PcieLinkLayer::for_endpoint(ep_name)`, `PciePhyDigitalCtrl::for_endpoint(ep_name)`, `PcieBypassMux::for_endpoint(ep_name)`, or `PcieAxiAdapter::for_endpoint(ep_name)`
- **THEN** the API returns a pointer to the corresponding sub-module **as if** the sub-module were registered through the per-sub-module `attach_to_endpoint` static registry
- **AND** for the 3 composite-owned sub-modules (LL/PHY/Mux), the lookup **MUST** first check `PcieLinkPhyMuxTLM::for_endpoint(ep_name)` and return the composite-owned sub-module reference; if no composite exists, fall back to the per-sub-module legacy registry (preserves frozen `PcieEndpointTLM` compatibility)
- **AND** `PcieLinkPhyMuxTLM::for_endpoint(ep_name)` **MUST** work in both phases: Phase 1 via the static `unordered_map<name, unique_ptr<PcieLinkPhyMuxTLM>>` registry (EP holds raw observer pointer only — single ownership); Phase 2 via `PcieEndpointIP::find_composite(ep_name)` scanning the static EP instances list + `internal_factory->getInstance(ep_name + "_lpm")`
- **AND** `PcieEndpointIP` **MUST** explicitly detach from the composite registry in its destructor (Phase 1) to prevent stale pointers across Catch2 TEST_CASEs

### Requirement: Composite Internal Coupling Boundaries（新增）

The system **MUST NOT** introduce Bundle/Port connections between `PcieLinkLayer` / `PciePhyDigitalCtrl` / `PcieBypassMux` (the three composite-owned sub-modules). Their interactions **MUST** remain as direct C++ pointer calls and `std::function` sinks, to preserve the following zero-cycle synchronous semantics:

#### Scenario: PHY rate-switch synchronous trigger
- **WHEN** `PciePhyDigitalCtrl::start_rate_switch(rate)` is called
- **THEN** `phy_.link_layer()->trigger_rate_switch(rate_, to)` is invoked synchronously in the same call stack (per `pcie_phy_digital_ctrl_tlm.hh:136-137`)
- **AND** immediately after, `rate_switching_` flag is set and `link_layer_->rate_switch_ready_ns_` is populated such that subsequent `try_pop_tx_tlp()` calls within `link_layer_` reject until ready_ns (per `pcie_link_layer_tlm.hh:299-303`)
- **AND** no Bundle indirection is permitted between PHY and LL, even at the cost of explicit INV-4 documentation

#### Scenario: BypassMux 10-step cleanup atomicity
- **WHEN** `PcieBypassMux::apply_mode(BypassMode::Partial)` is called
- **THEN** the 10-step cleanup (`notify_peer_mode_change → pause_link_layer → drain → clear_retry_buffer → reset_seq_counters → reset_fc_buckets → Partial guard → clear_msix_pending → commit mode → notify_peer_mode_complete → resume_link_layer`, per `pcie_bypass_mux.hh:130-138`) executes synchronously in a single call stack
- **AND** no step is allowed to be deferred to a Bundle hop or async callback; partial state mid-cleanup is forbidden

### Requirement: 23 ABI Boundary Preservation（新增）

The system **MUST** maintain the 23 ABI frozen headers without modification:
- `include/tlm/gpu/pcie_endpoint_tlm.h` (PcieEndpointTLM 4-port legacy, `[[deprecated]]`)
- `include/abi/cpptlm_emulator.h` (mmio_write ABI走 board shell, `cpptlm_emulator.cc:245` `emu->board->mmio_write`)

#### Scenario: ABI layout compatibility
- **WHEN** `PcieEndpointIP` base class changes from `ChStreamModuleBase` to `SimModule`
- **THEN** the `mmio_write(uint32_t bar, uint64_t offset, uint64_t data)` ABI signature (`pcie_endpoint_ip.hh:148`) remains unchanged
- **AND** the call path from `cpptlm_emulator.cc:245` `emu->board->mmio_write` reaches `PcieEndpointIP::mmio_write` without ABI surface change
- **AND** `CPPTLM_PCIE_ENDPOINT_ABI_VERSION` macro (`pcie_endpoint_ip.hh:15`, currently `=2`) is **NOT** bumped (base class change is not ABI-exposing)

---

## ADDED Requirements

### Requirement: PcieSriovVfPool Stays as Value Member（新增）

The system **MUST NOT** split `PcieSriovVfPool` into a separate `ChStreamModuleBase` child. `pool_` is a value member of `PcieEndpointIP` (`pcie_endpoint_ip.hh:172`), and its `config_of(uint16_t stream_id)`, `msix_of(uint16_t stream_id)`, `completions()`, and direct data-path access in `PcieEndpointIP::tick()` (`pcie_endpoint_ip.cc:280, 296, 336, 340`) **MUST** remain as direct C++ method calls.

#### Scenario: VfPool hot-path preservation
- **WHEN** `PcieEndpointIP::tick()` processes an AXI slave write request with `awaddr < pool_.config_of(0).config_size()`
- **THEN** `pool_.config_of(0).write(cfg_byte_off, wdata32)` is invoked as a direct method call (per `pcie_endpoint_ip.cc:296-297`), without Bundle indirection
- **AND** splitting VfPool into a separate ChStream child is explicitly **rejected** by this Requirement

### Requirement: AxiAdapter Split Deferred to Phase 3（新增）

The system **MUST** keep `PcieAxiAdapter` as a per-EP static-registry-owned object (NOT a ChStream child) in Phase 1 and Phase 2 of this refactor. Splitting AxiAdapter into a `ChStreamModuleBase` child is **explicitly deferred** to a future change (`cpptlm-pcie-endpoint-ip-axiadapter-split`) and **MUST** first migrate the AXI slave processing logic from `PcieEndpointIP::tick()` (`pcie_endpoint_ip.cc:248-364`) into the adapter itself.

#### Scenario: AxiAdapter remains static-registry-only in Phase 1+2
- **WHEN** `PcieEndpointIP::attach_composition()` processes `params.axi_adapter`
- **THEN** it calls `PcieAxiAdapter::attach_to_endpoint(ep_name, event_queue)` (per `pcie_endpoint_ip.cc:96-105`) returning the per-EP adapter pointer
- **AND** `PcieAxiAdapter::for_endpoint(ep_name)` continues to be invoked from `PcieEndpointIP::tick()` (`pcie_endpoint_ip.cc:255-364`) for AXI slave processing
- **AND** `PcieAxiAdapter` is **NOT** registered via `REGISTER_CHSTREAM` and **NOT** added as an `internal_factory` child of `PcieEndpointIP`
- **AND** the future change `cpptlm-pcie-endpoint-ip-axiadapter-split` is **explicitly out of scope** of this spec

---

## Out of Scope (明确边界)

| 项 | 排除理由 |
|----|---------|
| Phase 3：AxiAdapter 拆分为 ChStream child | 当前非 SimObject（`pcie_axi_adapter_tlm.hh:40` 无基类）；EP::tick `cc:248-364` AXI slave 业务逻辑住 EP 内；先迁逻辑再建模块是独立 Short change |
| VfPool 拆分为 ChStream child | Oracle 明确驳回；`pool_` 是 EP 值成员 + EP::tick 数据路径每拍访问；拆分为模块只会让所有热路径调用穿过跨模块指针，零收益纯成本 |
| LL↔PHY↔Mux 内部信号 Bundle 化 | 0-cycle 同步调用语义保护；Bundle 化必然破坏 INV-B / rate switch / 10-step cleanup 原子性 |
| PcieEndpointTLM（4 端口冻结 legacy）触碰 | 23 ABI 冻结头 `include/tlm/gpu/pcie_endpoint_tlm.h`；本 spec 不引入任何对该文件的修改 |
| 23 ABI header `include/abi/cpptlm_emulator.h` 触碰 | mmio_write ABI 走 board shell，不经 EP 类布局；EP 基类切换 ABI 安全 |
| JSON `params` 消费语义变更 | 7 类键保持原 `attach_composition` 消费逻辑；`on_config_loaded → simulate_instantiate` 是同语义接口切换 |
| **外层 JSON `pcie_ep.pf0`/`pcie_ep.vfN` 声明式接线**（R4 决策 C） | `connection_resolver.cc` 仅支持单层 exposed-port 解析，不支持两层下钻（composite 住 EP `internal_factory`，不在外层 `object_instances`）；Phase 8 数据路径靠 429327d 程序化桥接，外层 TLP 接线零消费者；扩展 resolver 属 Phase 3 独立 change |
| **`addOutputConfig` 调用** | 该 API 在 `sim_module.hh:183-196` 仅 `#ifdef CPPTLM_TESTING` 暴露，生产构建编译失败；本 change 不使用 |
| 17 端口扩展 | Phase 4 SR-IOV 冻结约束（1 PF + 16 VF），扩展需另行 spec |
| CHSTREAM 注册表全部取消 | Phase 1 / Phase 2 保留为兼容 shim；Phase 3+ 评估 |
| `bar_store_` / ResizableBar / INV-G 重构 | 归 `2026-09-10-cpptlm-stage-1-4-2-1`（BAR window model）独立 change |

---

## Cross-Reference

- **foundation spec**: `openspec/specs/pcie-ip-microarch/spec.md`（Phase 4-8 SR-IOV 基线，本 spec 是 MODIFIED delta）
- **proposal**: `openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/proposal.md`
- **design**: `openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/design.md`
- **tasks**: `openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/tasks.md`
- **upstream**: `cpptlm-dgpu-pcie-integration` (Phase 8 ship, commit `429327d`)
- **sibling**: `2027-02-09-cpptlm-pcie-endpoint-ip-json-config`（archive, 2027-02-09，JSON 配置扩展；本 change 不重叠）
- **future**: `cpptlm-pcie-endpoint-ip-axiadapter-split`（规划中, Phase 3）
- **cluster precedent**: `src/tlm/cluster/apu_soc.cc:47-82`（SimModule 持有 ChStream + SimModule 子模块的活样本）
- **nested mechanism**: `include/core/sim_module.hh:31-261`（MAX_DEPTH=8 + 默认 tick 不定序）
- **factory 8-step flow**: `src/core/module_factory.cc:141-843`（Step 7 line 615-712 ChStream adapter 注入；module registry 优先查找 line 266-269）
- **connection resolver**: `src/core/connection_resolver.cc:47-98`（SimModule exposed-port findInternalPath 支持）
- **Oracle 复评记录**: 任务输出 `ses_f5f27577effeJr1y4IGj1NSGVk`（2026-09-15，APPROVE_WITH_CONDITIONS）
