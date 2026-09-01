# cpptlm-dgpu-pcie-130b-encoding: 128b/130b Encoding 延迟建模

> **状态**: 📋 Proposed — 2026-09-29 · **日期**: 2026-09-29 · **Owner**: CppTLM Team (Sisyphus)
> **性质**: Phase 2 子 change(7 阶段路线图第二步,W5-W6)
> **父 change**: [`2026-09-01-cpptlm-dgpu-pcie-ip-microarch`](../2026-09-01-cpptlm-dgpu-pcie-ip-microarch/) (umbrella)
> **前置依赖**: [`2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc`](../2026-09-08-cpptlm-dgpu-pcie-link-layer-and-fc/) (Phase 1,已 archive)

---

## Why

Gen5 PCIe 使用 **128b/130b 编码**(非 FLIT,FLIT 是 Gen6 特性 — Librarian 修正)。SoC 仿真需要建模速率切换延迟与块传输延迟,但**不需要 bit-level 编码/解码**(per Oracle Q1)。

Phase 1 已交付 `PcieLinkLayer`(DLLP + ACK/NAK + 双向 Rx)。Phase 2 为其注入延迟模型,使带宽测试和反压仿真符合 Gen5 物理特性。

## What Changes

| 文件 | 用途 |
|---|---|
| `include/tlm/pcie/pcie_encoding_latency_model.hh` + `.cc` | **新**: 延迟建模器(Gen1-5 GT/s per-lane + 128B 块延迟 + 速率切换延迟) |
| `src/tlm/pcie/pcie_link_layer_tlm.cc` | **改**: Tx/Rx Path 注入延迟(不修改 Phase 1 ABI) |
| `test/test_pcie_encoding_latency.cc` | **新**: 各 Gen 速率延迟单元测试 |
| `test/test_pcie_endpoint_throughput.cc` | **新**: Gen5 速率下吞吐回归测试(>= 95% 理论带宽) |
| `src/CMakeLists.txt` + `test/CMakeLists.txt` | **改**: 注册新源文件 + ctest |

### 关键修订(per Oracle 二次评审 C2/C3)

- ⚠️ 速率单位: **GT/s per-lane per-direction**(不是 MB/s,不是 MT/s)
- `enum class Rate { GEN1=2, GEN2=5, GEN3=8, GEN4=16, GEN5=32 }` GT/s
- 块延迟(128B / 1024-bit per-lane): Gen5 ≈ 32ns, Gen4 ≈ 64ns, Gen3 ≈ 128ns
- 速率切换延迟: ~µs 级(包含 Gen3+ 均衡协商,**非 Gen5 独有**)

## 延迟计算公式

```
block_latency_ns = (block_size_bits / gen_rate_GT) = block_size_bytes × 8 / gen_rate
lane_factor      = 1 / active_lanes   # 多 lane 并行加速
total_latency    = block_latency_ns × lane_factor
```

Gen5 x1 lane, 128B block = 1024 bits / 32 GT/s = **32 ns**
Gen5 x16 lane, 128B block = 1024 bits / (32 × 16) GT/s = **2 ns**

## Acceptance Gate

| Gate | 验证 |
|---|---|
| **P2-G1** | `cmake --build build` 通过 + 2 个新测试 PASS |
| **P2-G2** | `ctest -R test_pcie_encoding_latency` PASS(各 Gen 速率正确) |
| **P2-G3** | `ctest -R test_pcie_endpoint_throughput` PASS(Gen5 ≥ 95% 理论带宽) |
| **P2-G4** | 全量 ctest 无回归(7 个 Phase 1 测试仍 PASS) |

## 风险与缓解

| Risk | 缓解 |
|---|---|
| R1: Gen1 枚举 2 vs 真值 2.5 GT/s 精度 | 文档注释 + 带宽计算按 2.5 用浮点常量 |
| R2: Phase 1 ABI 破坏 | 不修改 `pcie_link_layer_tlm.h` 公共接口,只往 .cc 注入延迟 |
| R3: 测试条件未定义 | P2-G3 明确:credit 充足 + 无拥塞 + 纯 memcpy 流 |