# Tasks — cpptlm-pcie-tlp-entry-layer v1.0 (Phase 1 Change D, TDD 5-step)

> **配套**: [`proposal.md`](proposal.md) · [`design.md`](design.md) · [`specs/pcie-tlp-entry/spec.md`](specs/pcie-tlp-entry/spec.md)
> **目标 tag**: `cpptlm-pcie-tlp-entry-completion`
> **估时**: **~8d** (TDD 5-step + TLP encode/decode + DGpuBoard invasive)
> **依赖**: flr-rebar v1.1 ✅ + p2-integration-unblock ✅ + Change B/C ✅

---

## 总体时间线 (5 阶段, ~8d)

| 阶段 | 内容 | 估时 |
|------|------|:----:|
| Phase 1 | TDD RED (10 failing cases) | 1d |
| Phase 2 | GREEN: PcieTlpSerializer + PcieTlpDecoder + submit_tlp API | 3d |
| Phase 3 | GREEN: DGpuBoard::mmio_*/config_* 经 TLP 改造 | 1.5d |
| Phase 4 | 0 回归锁 (5 套件) + TLP round-trip 端到端 | 1.5d |
| Phase 5 | 文档 + archive | 1d |

---

## Phase 1: TDD RED — 10 failing cases (1d)

### Task 1.1: Write failing tests for TLP encode/decode roundtrip (RED)

**Files**: Create `test/test_pcie_tlp_entry_layer.cc`

- [ ] T1: encode_mwr → decode_mwr → byte-level match (data[4])
- [ ] T2: encode_mrd → decode_mrd → dispatch to BAR
- [ ] T3: encode_cfgrd/cfgwr → decode → dispatch to PcieConfigSpace
- [ ] T4: TLP header fields validate (Fmt + Type + TC + Length + ReqId + Tag + BE + Address)
- [ ] T5: MRd CplD data integrity (完整 payload)
- [ ] T6: MWr Completion 生成
- [ ] T7: malformed TLP reject (-EIO)
- [ ] T8: PCIe Spec §2.2 field mapping (encode 与 reference 字段集 match)
- [ ] T9: 端到端 mmio_read/write 经 TLP path vs 直调 byte-level match
- [ ] T10: 协同 [pcie] smoke 7 cases 保持全绿

**验证**: `cmake --build build` RED
**Commit**: `test(pcie-tlp): RED 10 cases TLP entry (Phase 1.1)`

---

## Phase 2: GREEN — PcieTlpSerializer + PcieTlpDecoder + submit_tlp (3d)

### Task 2.1: Implement PcieTlpSerializer encode (GREEN)

**Files**: Create `include/tlm/pcie/pcie_tlp_serializer.{hh,cc}` (~350 行)

- [ ] Step 1: header 4DW encode (per PCIe Spec §2.2.1)
- [ ] Step 2: serialize_mmio_write: MWr (per spec §2.2.6)
- [ ] Step 3: serialize_mmio_read: MRd
- [ ] Step 4: serialize_config_read: CfgRd (per spec §2.4)
- [ ] Step 5: serialize_config_write: CfgWr
- [ ] Step 6: deserialize_completion: CplD parse
- [ ] Step 7: req_id + tag allocator (atomic, 16-bit + 8-bit)
- [ ] Step 8: 单元测试: encode/decode roundtrip (T1-T3)

**验证**: T1-T3 GREEN
**Commit**: `feat(tlp): PcieTlpSerializer encode + decode (Phase 2.1)`

### Task 2.2: Implement PcieTlpDecoder dispatch (GREEN)

**Files**: Create `include/tlm/pcie/pcie_tlp_decoder.{hh,cc}` (~250 行)

- [ ] Step 1: handle_mwr → dispatch to BAR
- [ ] Step 2: handle_mrd → dispatch to BAR + generate CplD
- [ ] Step 3: handle_cfgrd → dispatch to PcieConfigSpace + CplD
- [ ] Step 4: handle_cfgwr → dispatch to PcieConfigSpace
- [ ] Step 5: malformed Fmt reject
- [ ] Step 6: 单元测试 (T4-T7)

**验证**: T4-T7 GREEN
**Commit**: `feat(tlp): PcieTlpDecoder dispatch + CplD generate (Phase 2.2)`

### Task 2.3: Add PcieEndpointIP::submit_tlp API + integrate decoder (GREEN)

**Files**: Modify `include/tlm/pcie/pcie_endpoint_ip.hh` (+1 行) + `src/tlm/pcie/pcie_endpoint_ip.cc` (~50 行)

- [ ] Step 1: 头文件加 `int submit_tlp(const PcieTlpBundle& tlp);` API (additive)
- [ ] Step 2: 内部实例化 `PcieTlpDecoder` 
- [ ] Step 3: submit_tlp 内部调 `decoder_->decode_and_dispatch(tlp)`
- [ ] Step 4: 单元测试 T8 (PCIe Spec compliance)

**验证**: T8 GREEN
**Commit**: `feat(pcie-ep): submit_tlp() API + Decoder integration (Phase 2.3)`

---

## Phase 3: GREEN — DGpuBoard::mmio_*/config_* 经 TLP 改造 (1.5d)

### Task 3.1: mmio_write 改造 (GREEN, Gap M 修复)

**Files**: Modify `src/tlm/gpu/dgpu_board_shell.cc` (~50 行)

- [ ] Step 1: DGpuBoard 持 `tlm::pcie::PcieTlpSerializer tlp_serializer_` 实例
- [ ] Step 2: `mmio_write` 改造: 内部生成 MWr TLP → ep->submit_tlp → 0 ep 直调
- [ ] Step 3: 保留 lifecycle guard + power gate (mmio_gated) 不变
- [ ] Step 4: 23 ABI 签名 0 变 (per H-D1)
- [ ] Step 5: 单元测试 T9 (端到端 byte-level match)

**验证**: T9 GREEN
**Commit**: `feat(board): mmio_write 经 PcieTlpSerializer (Gap M 修复, Phase 3.1)`

### Task 3.2: mmio_read + config_* 改造 (GREEN)

**Files**: Modify `src/tlm/gpu/dgpu_board_shell.cc` (~30 行)

- [ ] Step 1: mmio_read 改造: 内部生成 MRd TLP → ep->submit_tlp_async → CplD → return
- [ ] Step 2: submit_tlp_async 返回 `std::future<CplD>` + 50ms timeout (per Change B)
- [ ] Step 3: pcie_config_read/write 改造: CfgRd/CfgWr TLP encode
- [ ] Step 4: 单元测试 (T10 + smoke 7 cases 保持)

**验证**: T10 + smoke 7 cases GREEN
**Commit**: `feat(board): mmio_read + config_* 经 TLP (Gap M 修复, Phase 3.2)`

---

## Phase 4: 0 回归锁 (5 套件) + TLP round-trip 端到端 (1.5d)

### Task 4.1: 5 套件 0 回归锁

**验证**:
- [ ] `[pcie]` 430 cases / 36672 assn: ✅ Pass
- [ ] `[minimal_dgpu_soc]` 12 cases / 66 assn: ✅ Pass
- [ ] `[driver_visible]` 2 cases / 29 assn: ✅ Pass
- [ ] `[pcie-memory]` 17 cases / 48 assn: ✅ Pass
- [ ] `[realip]` 22 cases / 193 assn: ✅ Pass
- [ ] `[pcie-host-test]` (flr-rebar v1.1): ✅ Pass (HostPcieTestStub 调 DGpuBoard::trigger_flr/resize_bar/mmio_*/config_* 通过 TLP path)

**Commit**: `test(regress): 5 套件 0 回归锁 (Phase 4.1)`

### Task 4.2: frozen surface 验证

**验证**:
- [ ] `git diff HEAD -- include/abi/` 空
- [ ] `git diff HEAD -- include/tlm/gpu/pcie_endpoint_tlm.h pcie_display_device.hh include/tlm/pcie/{pcie_bundles_tlm,pcie_endpoint_ip}.hh` 仅 `+submit_tlp` 1 行

---

## Phase 5: 文档 + archive (1d)

### Task 5.1: 实施笔记

**Files**: Create `docs/implementation/2026-10-09-pcie-tlp-entry-layer.md` (~50 行)

**Commit**: `docs(implementation): 实施笔记 (Phase 5.1)`

### Task 5.2: architecture.md §0.2 Gap M ✅ 修订 + AGENTS.md KEY INVARIANTS

- [ ] Gap M 行 ✅ 已达
- [ ] AGENTS.md KEY INVARIANTS: "TLP entry layer (Gap M 修复)..."

**Commit**: `docs(architecture): §0.2 Gap M ✅ + AGENTS.md (Phase 5.2)`

### Task 5.3: archive + validate

**Commit**: `chore(openspec): archive cpptlm-pcie-tlp-entry-layer (Phase 5.3)`

---

## Acceptance (per §0.2 DoD)

- [ ] **Gap M 修复**: DGpuBoard::mmio_*/config_* 内部走 TLP 序列化 + ep->submit_tlp, **0 直调** chip-internal (per H-D4)
- [ ] TLP encode 严格按 PCIe Base Spec §2.2 (verify via 10 cases 测试)
- [ ] 23 ABI 签名零变更 (`git diff HEAD -- include/abi/` 空)
- [ ] **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面: `cpptlm_emulator.h` + `pcie_endpoint_tlm.h` + `pcie_display_device.hh` + `pcie_bundles_tlm.hh`; quasi = `pcie_endpoint_ip.hh` 仅 + submit_tlp + submit_tlp_async 2 个 API)
- [ ] 11 commits 全 GREEN
- [ ] `[pcie][tlp-entry]` 10 new cases PASS
- [ ] 既有 5 套件 (`[pcie]`/`[minimal_dgpu_soc]`/`[driver_visible]`/`[pcie-memory]`/`[realip]`) 0 回归
- [ ] `[pcie-host-test]` (flr-rebar v1.1) 0 回归
- [ ] `openspec validate --strict` PASS

### 总 commits 数

预计 **13 commits** (per proposal):

```
1.  test(pcie-tlp): RED 10 cases (Phase 1.1)
2.  feat(tlp): PcieTlpSerializer encode + decode (Phase 2.1)
3.  feat(tlp): PcieTlpDecoder dispatch + CplD (Phase 2.2)
4.  feat(pcie-ep): submit_tlp() + Decoder integration (Phase 2.3)
5.  feat(board): mmio_write 经 PcieTlpSerializer (Phase 3.1)
6.  feat(board): mmio_read + config_* 经 TLP (Phase 3.2)
7.  test(regress): 5 套件 0 回归 (Phase 4.1)
8.  test(frozen): 23 ABI + **4 frozen + 1 quasi-frozen** headers 验证 (Phase 4.2)
9.  docs(implementation): 实施笔记 (Phase 5.1)
10. docs(architecture): §0.2 Gap M ✅ (Phase 5.2)
11. docs(agents): KEY INVARIANTS TLP entry (Phase 5.2 扩展)
12. chore(openspec): archive (Phase 5.3)
```

实际 12 commits.

---

## 后续动作

- D archive 后, KPI 清 1 槽 → 给 Phase 2 Change E (`cpptlm-msix-generator-ip`) 留 KPI slot
- Change E 启动: Gap L 修复 (MSI-X 直调 + CompletionRing dormant)

---

**最后更新**: 2026-10-09 (v1.0 起草)
**Owner**: CppTLM Team (Sisyphus)
