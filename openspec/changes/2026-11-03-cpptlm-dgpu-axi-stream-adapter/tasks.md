# cpptlm-dgpu-axi-stream-adapter: Phase 5 Tasks

> **配套**: [`proposal.md`](../proposal.md) · [`specs/axi-stream-adapter/spec.md`](../specs/axi-stream-adapter/spec.md)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **前置**: Phase 1-4 已完成（PCIe Link Layer / Encoding / PHY / SR-IOV）
> **Phase 5 在 Phase 4 Oracle 复评放行后启动**

---

## Phase 5 文件清单

| 文件 | 用途 |
|---|---|
| `include/bundles/axi4_bundles_tlm.hh` | **新**: Axi4Bundle / Axi4LiteBundle（含 `awid`/`arid`/`bid`/`rid`） |
| `include/framework/axi4_stream_adapter.hh` + `src/framework/axi4_stream_adapter.cc` | **新**: AXI4/AXI4-Lite 与 ChStream 适配器 |
| `include/tlm/pcie/pcie_axi_adapter_tlm.hh` + `.cc` | **新**: PcieEndpointIP 的 AXI 适配 |
| `src/tlm/gpu/pcie_endpoint_tlm.cc` | **改**: wire AXI 适配器（axi_master_out/slave_in/cfg_slave_in） |
| `test/test_axi4_bundle.cc` | **新**: Bundle 字段 / 序列化 / ID 关联 |
| `test/test_pcie_axi_adapter_basic.cc` | **新**: 基础读写与响应 |
| `test/test_pcie_axi_adapter_64byte_burst.cc` | **新**: 512-bit/64-byte burst |
| `test/test_pcie_axi_adapter_backpressure.cc` | **新**: valid/ready 反压不丢事务 |
| `test/test_pcie_axi_adapter_ids.cc` | **新**: outstanding 请求 ID 与响应关联 |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 显式注册新源与测试 |

---

## 严格 TDD 5 步（每个子任务）

### T-P5-1: Axi4Bundle / Axi4LiteBundle

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_axi4_bundle.cc`（字段全读回 + AWID/ARID/BID/RID 关联） |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 创建 `include/bundles/axi4_bundles_tlm.hh` + `.cc`（若需要） |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(axi): Axi4Bundle + Axi4LiteBundle with awid/arid/bid/rid` |

**测试场景**：
- ✅ 所有字段读回（awaddr/awlen/awsize/awburst/awid/wdata/wstrb/wlast/bid/bresp/araddr/arlen/arsize/arburst/arid/rid/rdata/rresp/rlast）
- ✅ AWID/ARID 读写独立 ID 空间
- ✅ 字段宽度匹配 spec（awaddr 64b, awid 16b, wdata 512b）

### T-P5-2: AXI4 Stream Adapter 框架

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_axi_adapter_basic.cc`（master/slave/cfg_slave 三端口，基础读写） |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 创建 `include/framework/axi4_stream_adapter.hh` + `src/framework/axi4_stream_adapter.cc` |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(axi): AXI4 Stream Adapter (master/slave/cfg_slave 3 端口)` |

### T-P5-3: PcieAxiAdapter（绑定到 PcieEndpointIP）

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_axi_adapter_64byte_burst.cc`（512-bit/64-byte burst） |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 创建 `include/tlm/pcie/pcie_axi_adapter_tlm.hh` + `src/tlm/pcie/pcie_axi_adapter_tlm.cc`（wire 到 PcieEndpointIP） |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(pcie-axi): PcieAxiAdapter bound to PcieEndpointIP` |

**测试场景**：
- ✅ `awlen=4` burst 写 — `wlast` 最后一拍置位
- ✅ 总传输字节 = `(len+1) × 2^awsize`
- ✅ 事务完整到达下游

### T-P5-4: backpressure 不丢事务

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_axi_adapter_backpressure.cc` |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 扩展 `axi4_stream_adapter` — backpressure 握手（valid/ready） |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`fix(axi): backpressure handshake in AXI4 Stream Adapter` |

**测试场景**：
- ✅ 下游 `ready=0` 时数据不丢失，`valid` 保持直到 `ready=1`
- ✅ 事务在 backpressure 周期内完整完成

### T-P5-5: AXI4 请求 ID 关联（awid→bid, arid→rid）

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_axi_adapter_ids.cc`（多 outstanding + 响应 ID 匹配） |
| ✅ 2 | 跑 → **FAIL** |
| ✅ 3 | 扩展 AXI4 Stream Adapter — outstanding 请求 ID 关联 |
| ✅ 4 | 跑 → **PASS** |
| ✅ 5 | 提交:`feat(axi): outstanding request ID association (awid→bid / arid→rid)` |

### T-P5-6: composition 接入 + CMake + 全量验证

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 修改 `src/tlm/gpu/pcie_endpoint_tlm.cc` — wire AXI 适配器 |
| ✅ 2 | 修改 `src/CMakeLists.txt` + `test/CMakeLists.txt` — 注册新源与测试 |
| ✅ 3 | `cmake --build build -j$(nproc)` |
| ✅ 4 | `ctest -R "axi"` + 全量 `[pcie]` 无回归 |
| ✅ 5 | 提交:`build(axi): register Phase 5 sources in CMakeLists` |

---

## Phase 5 Acceptance Gate

| Gate | 验证 |
|---|---|
| **P5-G1** | 5 个新测试 PASS（axi4_bundle / axi_adapter_basic / 64byte_burst / backpressure / ids） |
| **P5-G2** | 全量 `[pcie][axi]` ctest 无回归（Phase 1-4 + 5 全部 PASS） |
| **P5-G3** | `include/tlm/gpu/pcie_endpoint_tlm.h` 与 `include/abi/cpptlm_emulator.h` 零修改（23 ABI 保护） |
| **P5-G4** | `openspec validate 2026-11-03-cpptlm-dgpu-axi-stream-adapter --strict` PASS |

---

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: AWID/ARID 与 BID/RID 映射错误导致 OOO 匹配失败 | TDD 严格断言 per Phase 6 OOO 消费要求 |
| R2: backpressure 丢事务 | TDD 测试覆盖 `ready=0` + 后续 `ready=1` 完整到达 |
| R3: 64-byte burst 位宽计算错误 | TDD 测试断言总字节 = `(len+1) × 2^awsize` |
| R4: PcieAxiAdapter 未接入 PcieEndpointIP 数据路径 | TDD 端到端测试 + composition 集成 |

---

## 维护

**维护**: CppTLM Team (Sisyphus)
**状态**: ✅ Tasks — Phase 5 已完成（6 子任务全部 PASS，2026-11-03）
