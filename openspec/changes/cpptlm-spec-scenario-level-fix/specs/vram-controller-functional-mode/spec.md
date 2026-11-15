## MODIFIED Requirements

### Requirement: vc-functional-mode-data-path (新)

`PcieMemoryDevice::handle_slave_port(port=0)` SHALL 在 `simulation_mode == "functional"` 且 `vram_controller_ != nullptr` 时, **委托 SDMA MEM_WRITE 流量给 VramControllerTLM 的 functional-mode 接口** (`VramControllerMode::FunctionalZeroDelay`), 而不是直 memcpy.

#### Scenario: SDMA H2D 经 VramController functional-mode 路径 (per §0.1 原则 4 目标态)

- **GIVEN** `dgpu_soc_minimal_v1.json` 含 `simulation_mode: "functional"` + 顶层 `vram_ctrl` 模块节点
- **AND** `DGpuBoard::bind_memory_backings()` 已注入 `vram_ctrl` (T23C 第 4 路激活) + 设置 mode = `FunctionalZeroDelay`
- **WHEN** SDMA H2D: `sdma.mem_out (AxiMemBundle MEM_WRITE)` 触发 `pcie_memory.handle_slave_port(port=0)`
- **THEN** 数据路径 = **`vram_ctrl.handle_functional_request(addr, size, is_write=true, data)`**
- **AND** 内部执行: controller-protocol-mapped path + 字节级 memcpy 到 `vram_storage_[addr..addr+size]`
- **AND** 返回 `MEM_WRITE_RESP` (resp.resp=0 OK) → `sdma.req_in[2]`
- **AND** controller latency = **0 cycles** (functional-mode 标识; 与 timing-mode `latency=kOperatingFrequencyHz × bandwidth_gbps^-1` 区分)

#### Scenario: handle_request 返回 SLVERR 传播 (per D-AXI v1.4 B9)

- **GIVEN** VramController functional-mode, `sdma.mem_out` 触发 handle_slave_port(port=0)
- **AND** `vram_ctrl.handle_functional_request()` 返回 `non-zero` rc (e.g., OOB, SLVERR)
- **WHEN** `handle_slave_port` 检查返回值
- **THEN** 置 `resp.resp.write(1)` (SLVERR) 而**不**静默 OK
- **AND** SDMA 收到 SLVERR → 触发 retry driver (per D-AXI v1.4 B22)
- **NOTE**: per §X 关键风险 R3 缓解; 测试用例 `[vram_controller][functional][slverr]` 必须注入 1 case 验证

#### Scenario: GMMU PTE fetch 改走 VramControllerTLM 专用 functional port (NVIDIA H100 dedicated memory client 模式, per v0.5 三锁叠加裁决)

- **GIVEN** `dgpu_soc_minimal_v1.json` functional-mode 配置 (含 `vram_ctrl`) + per §1.4.6 v0.5 修订 (GMMU dedicated memory client bus)
- **WHEN** GMMU 触发 PTE fetch: `gmmu.translate(iova, ...) → GMMU 异步状态机 → MasterPort(gmmu.req_out) → VramControllerTLM::handle_functional_request(pte_addr, 8, is_write=false, nullptr)`
- **THEN** 数据路径 = **GMMU dedicated memory client 直连 VramController functional port** (per NVIDIA H100 HBM Controller 通道); VramController 通过 `handle_functional_request` 拉 PTE 字节; 字节级 `memcpy(vram_storage_.get() + pte_addr, pte_buf, 8)`
- **NOTE**: 与 §0.1 Gap 矩阵 G2 + §1.4.3 强约束不矛盾 — GMMU 是 dedicated memory client (直连 VramController), **不经 pcie_memory, 不经 NOC mesh** (per v0.5 三锁叠加裁决)
- **NOTE**: 既有 D-AXI v1.4 PcieMemoryDevice `port=1` (PcieTlpBundle) **保留不修改** (per v1.0 原 H-B6 不变; 仅本 scope 不修改 pcie_memory), 仅 GMMU 这条路径走 VramController functional port (NEW dedicated client 通道)
