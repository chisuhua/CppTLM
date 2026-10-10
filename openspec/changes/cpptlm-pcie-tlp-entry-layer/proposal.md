# cpptlm-pcie-tlp-entry-layer: PcieTlpSerializer + TLP 解码层 (修复 Gap M)

> **状态**: 🟡 Proposed (Phase 1, Change D, 2026-10-09 lock)
> **作者**: CppTLM Team
> **版本**: v0.5.2 (Tranche 1 修订: serializer 边界复用既有 `include/utils/pcie_tlp_serializer.hh`, 避免 design duplication debt; 初版 v1.0 协同 flr-rebar v1.1 + p2-integration-unblock 落地)
> **关联 OpenSpec**:
>   - ✅ `cpptlm-pcie-host-test-flr-rebar` v1.1 archived (HostPcieTestStub in-process C++ 仿真测试桩 + FLR/ReBAR 接口)
>   - ✅ `cpptlm-p2-integration-unblock` (Proposed — CP 跨仓接入 unblock, 23 ABI 冻结 + 0 新增)
>   - **业务定位锁定**: `docs/designs/dgpu-soc/architecture.md` §0.1 Gap 矩阵 + §0.2 总体实施计划 (Change D)
>
> **🔗 Cross-Reference (per `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3, 2026-10-10)**:
> 本 Change D PcieTlpSerializer 与 `cpptlm-pcie-host-test-flr-rebar` v1.1 HostPcieTestStub 关系: HostPcieTestStub 直接调既有 `PcieEndpointIP::flr_pf()/flr_vf() + enable_resizable_bar()/on_bar_resize()` API (per `pcie_endpoint_ip.hh:201-202` + `pcie_endpoint_ip.cc:670-694`), **未定义 PcieTlpSerializer 关系** (per Metis M-8);**v0.5.2 已落地**: PcieTlpSerializer **复用既有** `include/utils/pcie_tlp_serializer.hh` (HostPcieTestStub 已在用, 5596B, per `host_pcie_test_stub.hh:26,202`), 本 change **不新建** tlm/pcie 域重复类 (避免 design duplication debt); 仅**扩展**既有类新增 `encode_trigger_flr() + encode_resize_bar()` (~80 行) — 作为 HostPcieTestStub 与 PcieEndpointIP 之间的串行化/解串行化层 (经 `cpptlm-p2-integration-unblock` 跨仓接入), **不绕过既有 EP API**。详见 [cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md §M-8](../../cpptlm-dgpu-soc-arch-truth-cleanup/proposal.md)。Change D PCIe Base Spec Cfg TLP Fmt/Type 错误不在本 change 范围,需 Change D author 另起 v0.5.2 返工。
> **修复 Gap**: **M** (MED) — §0.1 Gap 矩阵 16 行中 1 行
> **优先级**: **P2** (Phase 1, 与 Change C 并行)
> **估时**: **7-10d** (含框架扩展 + 23 ABI 零 diff + 与 flr-rebar 协同)
> **依赖**: flr-rebar v1.1 ✅ archived (HostPcieTestStub 接口已锁) + Change B ✅ + Change C ✅
> **目标 tag**: `cpptlm-pcie-tlp-entry-layer-completion`

---

## Why

§0.1 Gap M 锁定 PCIe 入口 TLP 序列化层缺失. 当前 `DGpuBoard::mmio_read/write/config_read/write/trigger_flr/resize_bar` 全部是直调函数 (host→chip-internal 直跳, 绕开 PCIe TLP 协议), 与真实 RTL 不符 (real host 通过 TLP 报文 PCIe EP 解码).

### Gap M 详情

- **现状**: `DGpuBoard::mmio_*` 直调方法, 0 个 TLP serialization 层:
  ```cpp
  // 当前 (Gap M violation)
  int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
      // 直调 ep->memory_device().memory_write(off, buf, len)
      // 无 TLP encode, 无 EP TLP decode, 无 CplD 生成
  }
  ```
- **真实 RTL 模式**:
  - Host 发 MRd/MWr TLP (4DW header + data + ECRC + 1DW optional)
  - PCIe Controller 收 TLP, 解码 (TC, attr, length, req_id, 等等)
  - decode 后路由到 BAR / Cfg / DMA / etc.
  - Read 返回 CplD TLP (completion with data)
- **本 change 修复路径 (v0.5.2)**: 在 DGpuBoard 之上扩展既有 **`PcieTlpSerializer`** (`include/utils/pcie_tlp_serializer.hh`, host_pcie_test_stub 已在用) 新增 `encode_trigger_flr()/encode_resize_bar()` (host-side TLP encode) + PcieEndpointIP 接收端**TLP decode**, DGpuBoard::mmio_* 内部改为先生成 TLP.

### 协同关系

- **`cpptlm-pcie-host-test-flr-rebar` v1.1**: HostPcieTestStub in-process C++ 仿真测试桩, 已锁构造签名 `(DGpuBoard* board, const std::string& config_path)`. 本 change 让 HostPcieTestStub 调 DGpuBoard::trigger_flr/resize_bar/mmio_*/config_* 通过**真实 TLP 序列化层**, 与 host API 解耦
- **`cpptlm-p2-integration-unblock`**: CP 跨仓接入 unblock, 协同集成 (GAP M 修复后, CP 系统通过 TLP 序列化层与 minimal_v1 SoC 通信)

### 业务定位同步 (per §0.1 §X + flr-rebar v1.1 §X)

本 change 是 Phase 1 末段 change, 完成 Gap M 后 §0.1 Gap 矩阵 Phase 1 范围 (Gap I + G2 + G1 + M) 全部修复.

---

## What Changes

### 扩展现有文件 + 新增文件 (4 个, ~580 行)

| 文件 | 行数 | 内容 |
|------|:----:|------|
| `include/utils/pcie_tlp_serializer.hh` (扩展) | **+~80** | **新增方法**: `encode_trigger_flr()` (per FLR protocol) + `encode_resize_bar()` (per Resizable BAR §7.8.6 64-bit encoding)；复用既有 `PcieTlpSerializer` 类（host_pcie_test_stub.hh 已在用） |
| `include/tlm/pcie/pcie_tlp_decoder.hh` | **~100** | `PcieTlpDecoder` 在 PcieEndpointIP 内: decode PcieTlpBundle → dispatch 到 BAR / Cfg / DMA |
| `include/tlm/pcie/pcie_tlp_decoder.cc` | **~150** | TLP decode + dispatch 实现 (PCIe Base Spec §2.4~§2.6 decode) |
| `test/test_pcie_tlp_entry_layer.cc` | **~250** | 10 cases: encode/decode roundtrip + host→board→ep→bar 端到端 + `[pcie][tlp-entry]` |

### 修改文件 (4 个, ~150 行)

| 文件 | 改动 | 备注 |
|------|------|------|
| `src/tlm/gpu/dgpu_board_shell.cc` (mmio_write + mmio_read + pcie_config_read/write) | 改为内部生成 PcieTlpBundle + 注入 PcieEndpointIP (TLP serial loop) | 关键 invasive change (~80 行) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 新增 `submit_tlp(const PcieTlpBundle&)` API (TLP 入口) | 准 frozen (additive only) |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | PcieEndpointIP::submit_tlp 内部调 TLP decode + dispatch (~50 行) | 关键新增 |
| `test/test_cpptlm_emulator_minimal_soc_smoke.cc` | 加新 cases 验证 TLP entry 路径 (既有 7 cases 保持) | 0 回归锁 |

### 实施铁律 (per §0.1 + flr-rebar v1.1 集成)

1. **23 ABI 签名零变更 (Hard Invariant)**: `cpptlm_emulator.h` 0 修改 (per ADR-088 §D5 + flr-rebar v1.1 §X)
2. **TLP encode 严格按 PCIe Base Spec §2**: 4DW header fields (Fmt/Type + TC + attr + TH + EP + TD + length + ReqId + Tag + LastDWBE + FirstDWBE + Address) 等
3. **TLP decode 严格按 PCIe Base Spec §2.4**: Decode Fmt + Type, 路由到 MRd→Memory Read / MWr→Memory Write / CfgRd→Config Read / CfgWr→Config Write
4. **`pcie_tlp_serializer` 协同 Change B (v0.5.2 边界锁定)**: **复用既有** `include/utils/pcie_tlp_serializer.hh` (host_pcie_test_stub.hh 已在用, 5596B), 仅扩展新增 `encode_trigger_flr()/encode_resize_bar()`; **不新建** tlm/pcie 域重复类 (避免 design duplication debt). PcieTlpSerializer 跟 LWC 不冲突 (LWC 是 chip-internal, TLP 是 host↔chip 边界)
5. **协同 HostPcieTestStub**: TLP entry layer 让 HostPcieTestStub::send_mmio_read() 通过 TLP 序列化 → PcieEndpointIP::submit_tlp (真实路径), 而非绕过 TLP 直调 DGpuBoard
6. **completion 流程**: Read TLP 返回 CplD (Completion with Data TLP), 沿 PCIe spec §3.2
7. **时序兼容**: TLP entry 与 timing-mode 独立 (functional/timing mode 都能工作)

### Frozen Surface (0 修改, 守恒严格)

- `include/abi/cpptlm_emulator.h` (per ADR-088 §D5, 23 ABI 冻结)
- `include/tlm/gpu/pcie_endpoint_tlm.h`
- `include/tlm/gpu/pcie_display_device.hh`
- `include/tlm/pcie/pcie_bundles_tlm.hh`
- `include/tlm/pcie/pcie_endpoint_ip.hh` (准 frozen, 本 change **仅新增** `submit_tlp()` 1 个 API)

---

## Impact

### 影响范围

| 受影响模块/文件 | 影响 | 兼容性 |
|---------------|------|---------|
| `DGpuBoard::mmio_read/write/pcie_config_read/write` | 改为内部 TLP encode → `PcieEndpointIP::submit_tlp()` | 23 ABI 签名零变, 行为不变 (TLP 经同一路径); 既有测试 0 回归 |
| `PcieEndpointIP::submit_tlp` | 新增入口, TLP decode + dispatch | 准 frozen additive API; 既有 17 端口保持 |
| `HostPcieTestStub` | 直接调 `DGpuBoard::*` C++ 方法 (per flr-rebar v1.1 design §3.1), **不**调 TLP API | 0 兼容性影响 |
| `cpptlm-p2-integration-unblock` | CP 跨仓接入通过 std::function 或 callback 调 `DGpuBoard::*`, 走 TLP 序列化层 | 集成延迟 1-2 sim tick (TLP decode overhead) |

### 受影响测试

| 测试 | 当前 | 实施后 |
|------|------|--------|
| `[pcie]` (430 cases / 36672 assn) | ✅ Pass | ✅ **保持 Pass** (TLP entry 不破坏 PCIe 行为) |
| `[pcie-host-test-flr-rebar]` (flr-rebar 新) | Pass | ✅ Pass (HostPcieTestStub 调 `DGpuBoard::*` 不变) |
| `[minimal_dgpu_soc]` (12 cases / 66 assn) | Pass | ✅ Pass (driver 视角行为不变) |
| `[driver_visible]` (2 cases / 29 assn) | Pass | ✅ Pass |
| **`[pcie][tlp-entry]`** (本 change 新增) | 0 cases | ✅ **10 cases / 30+ assertions** (encode/decode roundtrip + 端到端) |

---

## Acceptance (per §0.2 DoD)

- [ ] **Gap M 修复**: `DGpuBoard::mmio_*` 内部走 PcieTlpBundle 经 PcieEndpointIP::submit_tlp, **0 直调** chip-internal
- [ ] TLP encode/decode 严格按 PCIe Base Spec §2 (validate via spec compliance test)
- [ ] 23 ABI 签名零变更 (`git diff HEAD -- include/abi/` 空)
- [ ] **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面: `cpptlm_emulator.h` + `pcie_endpoint_tlm.h` + `pcie_display_device.hh` + `pcie_bundles_tlm.hh`; quasi = `pcie_endpoint_ip.hh` **仅 + submit_tlp + submit_tlp_async** 2 个 API, 0 改既有 17 端口 signature)
- [ ] `[pcie]` 430 cases / 36672 assertions 保持全绿 (0 回归)
- [ ] `[minimal_dgpu_soc]` 12 cases / 66 assertions 保持全绿
- [ ] `[pcie][tlp-entry]` 10 新 cases 全绿
- [ ] TLP round-trip test: host 发 MRd→board 派发→bar 返回数据→CplD back to host, byte-level match
- [ ] `cpptlm-p2-integration-unblock` 集成 (CP 跨仓接入) 0 阻断
- [ ] `openspec validate --strict` PASS
- [ ] 文档同步: §0.2 Gap M 行 ✅ 修订 + AGENTS.md KEY INVARIANTS 加 TLP entry layer 锁定

### Commit 策略 (per D-AXI commit 风格 + flr-rebar v1.1)

预计 **11 commits**, 按 Phase 顺序:

```
1.  test(pcie-tlp): RED 10 cases (Phase 1.1)
2.  feat(tlp): 扩展 `include/utils/pcie_tlp_serializer.hh` encode_trigger_flr/encode_resize_bar (Phase 2.1)
3.  feat(tlp): PcieTlpDecoder decode + dispatch (Phase 2.2)
4.  feat(pcie-ep): PcieEndpointIP::submit_tlp() API (Phase 2.3)
5.  feat(board): DGpuBoard::mmio_* 走 PcieTlpSerializer (Phase 3.1)
6.  feat(board): DGpuBoard::pcie_config_* 走 PcieTlpSerializer (Phase 3.2)
7.  test(pcie): GREEN 10 cases TLP entry round-trip (Phase 4.1)
8.  test(regress): [pcie] + [minimal_dgpu_soc] 0 回归 (Phase 4.2)
9.  docs(implementation): 2026-10-09-pcie-tlp-entry-layer 实施笔记 (Phase 5.1)
10. docs(architecture): §0.2 Gap M ✅ 修订 + AGENTS.md KEY INVARIANTS (Phase 5.2)
11. chore(openspec): archive cpptlm-pcie-tlp-entry-layer (Phase 5.3)
```

---

## OpenSpec KPI 状态

- **实施前**: 4/3 (cpptlm-p2-integration-unblock + 3 起草完毕 change). KPI 超限 1 个
- **本 change 期间**: 5/3 (起草中)
- **A archive 后**: KPI → 3/3 (但已超限, **需立即 archive A/B 任一个**)
- **本 change archive 后**: KPI 清 1 槽

---

## 相关变更

| Change | 关系 |
|--------|------|
| `cpptlm-pcie-host-test-flr-rebar` v1.1 ✅ archived | **直接前置**: HostPcieTestStub 调 `DGpuBoard::*` 路径经本 change 引入的 TLP 序列化层 |
| `cpptlm-p2-integration-unblock` ✅ Proposed | **直接协作**: CP 跨仓接入 unblock 后通过 `DGpuBoard::*` → TLP path → minimal_v1 SoC |
| `cpptlm-lwc-bus-hub-mvp` ✅ Phase 0 | 间接协同: TLP entry 不直接用 LWC, 但与 BAR0/1/2 路由到 PcieEndpointIP 编码层有接口协同 |
| `cpptlm-gmmu-pte-bus-correction` ✅ Phase 1 | **并行 (Phase 1)**: 0 直接依赖; 同时实施修复 Gap I + G2 + G1 |

---

**Owner**: CppTLM Team (Sisyphus)
**最后更新**: 2026-10-09 (v1.0 起草, Phase 1 Change D 启动)
