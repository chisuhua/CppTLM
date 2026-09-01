# cpptlm-dgpu-pcie-link-layer-and-fc: Phase 1 Tasks

> **配套**: [`proposal.md`](./proposal.md) · [`specs/link-layer-and-fc/spec.md`](./specs/link-layer-and-fc/spec.md)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)

---

## T-P1-1: PcieDllpBundle(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_dllp_bundle.cc`(字段序列化 + 6 种 kind 判定) |
| ✅ 2 | 跑 `ctest -R test_pcie_dllp_bundle` → **FAIL** |
| ✅ 3 | 创建 `include/bundles/pcie_dllp_bundles_tlm.hh`:`PcieDllpBundle`(kind=8 bits, vc_id=4, credit_P/NP/Cpl × 3 组 16 bits, seq_num=16 bits) + `PciePhyConfig` |
| ✅ 4 | 跑测试 → **PASS** |
| ✅ 5 | 提交:`feat(pcie-dllp): add PcieDllpBundle with 6 kinds + 3 credit groups` |

## T-P1-2: FcTokenBucket(Step 1: Write failing test,per Q2)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_link_layer_fc_token_bucket.cc`(consume/update/over-consume/单调性/per-VF 隔离/deadlock 防护) |
| ✅ 2 | 跑 `ctest -R test_pcie_link_layer_fc_token_bucket` → **FAIL** |
| ✅ 3 | 创建 `include/tlm/pcie/pcie_flow_control_token_bucket.hh` + `.cc`(`consume()` / `update()` / `can_send()`,**无 refill_rate**) |
| ✅ 4 | 跑测试 → **PASS** |
| ✅ 5 | 提交:`feat(pcie-fc): FcTokenBucket with P/NP/Cpl buckets, UpdateFC-driven` |

## T-P1-3: PcieLinkLayer DLLP gen/parse/dispatch(Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_link_layer_dllp.cc` |
| ✅ 2 | 跑 `ctest -R test_pcie_link_layer_dllp` → **FAIL** |
| ✅ 3 | 创建 `include/tlm/pcie/pcie_link_layer_tlm.hh` + `src/tlm/pcie/pcie_link_layer_tlm.cc`(Tx Path + DLLP dispatch) |
| ✅ 4 | 跑测试 → **PASS** |
| ✅ 5 | 提交:`feat(pcie-ll): PcieLinkLayer with DLLP gen/parse/dispatch` |

## T-P1-4: 链路错误注入接口(Q15,Phase 1 必需)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_link_layer_error_injector.cc` |
| ✅ 2 | 跑 `ctest -R test_pcie_link_layer_error_injector` → **FAIL** |
| ✅ 3 | 在 `PcieLinkLayer` 加 `link_error_injector_t`(3 个 inject 方法,默认 disable) |
| ✅ 4 | 跑测试 → **PASS** |
| ✅ 5 | 提交:`feat(pcie-ll): link_error_injector_t API for retry TDD` |

## T-P1-5: 下行(host→EP)Rx 链路层(Q17,Phase 1 必需)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_link_layer_downstream_rx.cc` |
| ✅ 2 | 跑 `ctest -R test_pcie_link_layer_downstream_rx` → **FAIL** |
| ✅ 3 | 在 `PcieLinkLayer` 加 Rx Path(DLLP/TLP 分流 + Rx ACK 生成 + 收 retry buffer) |
| ✅ 4 | 跑测试 → **PASS** |
| ✅ 5 | 提交:`feat(pcie-ll): bidirectional link layer with downstream Rx path` |

## T-P1-6: ACK/NAK 重传(累积确认,Step 1: Write failing test)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_link_layer_ack_nak.cc`(模拟丢包 + 重传验证,使用 error injector) |
| ✅ 2 | 跑测试 → **FAIL** |
| ✅ 3 | 在 `PcieLinkLayer` 内实现 Retry Buffer + 12-bit Sequence Number |
| ✅ 4 | ACK → 累积清到 ack seq;NAK → 重发 seq ≥ NakSeq 所有 TLP |
| ✅ 5 | 跑测试 → **PASS** |
| ✅ 6 | 提交:`feat(pcie-ll): ACK/NAK retry buffer with seq tracking` |

## T-P1-7: 集成到 `PcieEndpointTLM`(composition,不修改布局)

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 写 `test/test_pcie_endpoint_with_ll.cc` |
| ✅ 2 | 跑测试 → **FAIL** |
| ✅ 3 | 在 `src/tlm/gpu/pcie_endpoint_tlm.cc` composition 集成 `PcieLinkLayer`(不改 `pcie_endpoint_tlm.h`) |
| ✅ 4 | 跑测试 → **PASS** |
| ✅ 5 | 跑全量 `ctest -R pcie` 无回归 |
| ✅ 6 | 提交:`feat(pcie-endpoint): integrate PcieLinkLayer via composition` |

## T-P1-8: 23 ABI 兼容性边界锁定

| 子任务 | 详情 |
|---|---|
| ✅ 1 | 创建 `include/tlm/pcie/pcie_endpoint_ip.hh` 顶部加 `CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 宏(无 `#error`) |
| ✅ 2 | 不修改 `include/tlm/gpu/pcie_endpoint_tlm.h` |
| ✅ 3 | 不修改 `include/abi/cpptlm_emulator.h` |
| ✅ 4 | 提交:`chore(pcie-abi): lock ABI v2 with version macro on pcie_endpoint_ip.hh` |

## T-P1-9: CMakeLists 注册 + 全量构建

| 子任务 | 详情 |
|---|---|
| ✅ 1 | `src/CMakeLists.txt` 加 `CORE_SOURCES` 显式列出新源文件(禁 GLOB) |
| ✅ 2 | `test/CMakeLists.txt` 加 ctest 注册(7 个新测试) |
| ✅ 3 | `cmake --build build -j$(nproc)` PASS |
| ✅ 4 | `ctest -j$(nproc)` 全量 PASS(764 + 7 个新测试 = 771) |
| ✅ 5 | 提交:`build(pcie-ll): register Phase 1 sources in CMakeLists` |

## T-P1-10: Phase 1 Acceptance Gate 验证

| Gate | 验证 |
|---|---|
| **P1-G1** | 7 个新测试 PASS |
| **P1-G2-G5,G4b,G4c** | 各 ctest 命令 PASS |
| **P1-G6** | 23 ABI 静态检查 PASS(`cpptlm_emulator.h` 未修改 + 宏已加) |
| **P1-G7** | 全量 `ctest -j$(nproc)` 无回归 |
| **P1-G8** | `openspec validate` PASS |

## T-P1-11: Phase 1 Archive

| 子任务 | 详情 |
|---|---|
| ✅ 1 | `openspec archive 2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc` |
| ✅ 2 | 更新 umbrella `roadmap.md` Phase 1 标记为 ✅ |
| ✅ 3 | 触发 Phase 1 Oracle 评审 |
