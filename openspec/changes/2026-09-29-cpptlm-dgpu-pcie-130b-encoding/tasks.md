# cpptlm-dgpu-pcie-130b-encoding: Phase 2 Tasks

> **配套**: [`proposal.md`](../proposal.md) · [`specs/130b-encoding/spec.md`](../specs/130b-encoding/spec.md)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)

---

## T-P2-1: PcieEncodingLatencyModel(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_encoding_latency.cc`(各 Gen 速率延迟单元测试) |
| ⏳ 2 | 跑 `ctest -R test_pcie_encoding_latency` → **FAIL**(头不存在) |
| ⏳ 3 | 创建 `include/tlm/pcie/pcie_encoding_latency_model.hh` + `.cc`(`enum class Rate` + `block_latency(Rate, block_bytes, lanes)` + `rate_switch_delay(Rate from, Rate to)`) |
| ⏳ 4 | 跑测试 → **PASS** |
| ⏳ 5 | 提交:`feat(pcie-encoding): PcieEncodingLatencyModel with GT/s enum + 128B block latency` |

**测试场景覆盖**:
- ✅ Gen5 x1 lane 128B block → 32ns
- ✅ Gen5 x16 lane 128B block → 2ns
- ✅ Gen3 x1 128B block → 128ns
- ✅ 速率切换 GEN3 → GEN5 → µs 级
- ✅ 边界:0 lanes(保护)
- ✅ 边界:block_size = 0(返回 0)

## T-P2-2: LinkLayer 集成延迟(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | 写 `test/test_pcie_endpoint_throughput.cc`(Gen5 速率下吞吐回归) |
| ⏳ 2 | 跑 `ctest -R test_pcie_endpoint_throughput` → **FAIL**(无延迟注入时吞吐过高) |
| ⏳ 3 | 在 `src/tlm/pcie/pcie_link_layer_tlm.cc` Tx/Rx Path 注入 `PcieEncodingLatencyModel` 延迟 |
| ⏳ 4 | 跑测试 → **PASS**(吞吐 ≥ 95% 理论带宽) |
| ⏳ 5 | 跑全量 `ctest -R pcie` → Phase 1 7 个测试仍 PASS |
| ⏳ 6 | 提交:`feat(pcie-ll): integrate PcieEncodingLatencyModel into Tx/Rx paths` |

## T-P2-3: CMakeLists 注册 + 全量构建

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | `src/CMakeLists.txt` 加 `pcie_encoding_latency_model.cc` |
| ⏳ 2 | `test/CMakeLists.txt` 加 ctest 注册 2 个新测试 |
| ⏳ 3 | `cmake --build build -j$(nproc)` PASS |
| ⏳ 4 | 提交:`build(pcie-encoding): register Phase 2 sources in CMakeLists` |

## T-P2-4: Phase 2 Acceptance Gate

| Gate | 验证 |
|---|---|
| **P2-G1** | 2 个新测试 PASS |
| **P2-G2** | 各 Gen 速率延迟正确 |
| **P2-G3** | Gen5 吞吐 ≥ 95% 理论带宽 |
| **P2-G4** | 全量 ctest 无回归(Phase 1 7 测试仍 PASS) |

## T-P2-5: Phase 2 Archive

| 子任务 | 详情 |
|---|---|
| ⏳ 1 | `openspec archive 2026-09-29-cpptlm-dgpu-pcie-130b-encoding` |
| ⏳ 2 | 更新 umbrella `roadmap.md` Phase 2 标记为 ✅ |
| ⏳ 3 | 触发 Phase 2 Oracle 评审 |