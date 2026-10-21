# Spec: PcieTlpSerializer + TLP 解码层入口 (cpptlm-pcie-tlp-entry-layer v1.0)

> **配套**: [`../../proposal.md`](../../proposal.md) · [`../../design.md`](../../design.md) · [`../../tasks.md`](../../tasks.md)
> **版本**: v1.0 (per tasks.md Phase 1 Change D, 2026-10-09 lock)

---

## ADDED Requirements

### Requirement: pe-tlp-entry-serialization (Gap M 修复)

`DGpuBoard` 内部 SHALL 将 host API (`mmio_read/write`, `pcie_config_read/write`, `trigger_flr/resize_bar`) 改为生成 PCIe TLP 报文, 经 `PcieEndpointIP::submit_tlp()` 进入 chip, 由内部 `PcieTlpDecoder` decode + dispatch.

#### Scenario: DGpuBoard::mmio_write 经 TLP 入口 (per Gap M 修复)

- **GIVEN** `cpptlm_emulator_mmio_write(bar, off, buf, len)` 调用 (`cpptlm_emulator.h:23 ABI`)
- **WHEN** DGpuBoard 内部 `mmio_write` 改造后
- **THEN** 内部生成 `PcieTlpBundle (MWr)` 含 Fmt=010/Type=00000 + TC=0 + T=0 + TD=0 + EP=0 + Length=len + ReqId + Tag + Last/First DW BE + Address + Data
- **AND** 调 `pcie_ep()->submit_tlp(tlp)` 进入 PcieEndpointIP
- **AND** `PcieTlpDecoder::decode_mwr` → dispatch to BAR memory write → 字节级 byte write
- **NOTE**: 23 ABI 签名零变更 (per H-D1)

#### Scenario: DGpuBoard::mmio_read 经 TLP 入口 + CplD 同步返回 (per Gap M 修复 + PCIe Base Spec §3.2)

- **GIVEN** `cpptlm_emulator_mmio_read(bar, off, buf, len)` 调用
- **WHEN** DGpuBoard 内部 `mmio_read` 改造后
- **THEN** 内部生成 `PcieTlpBundle (MRd)` 投递 → submit_tlp → decoder → BAR memory read
- **AND** PcieTlpDecoder 生成 `PcieTlpBundle (CplD with Data)` Completion
- **AND** 同步等待 future 50ms (per Change B `kMmioWaitTimeout`)
- **AND** memcpy(buf, data, min(len, returned_size))
- **NOTE**: mmio_read 同步路径引入 1 sim tick 延迟 (vs 直调), `kMmioWaitTimeout` 50ms 仍 cover 路径

#### Scenario: PCIe Config Space 经 TLP 入口 (per Gap M 修复)

- **GIVEN** `pcie_config_read(off, width, &val)` 调用
- **WHEN** 内部生成 `PcieTlpBundle (CfgRd)` (Fmt=001/Type=01010) → submit_tlp → decoder → PcieConfigSpace.read(off)
- **AND** `pcie_config_write(off, width, val)` 类似 CfgWr (Fmt=001/Type=01011)
- **NOTE**: per PCIe Base Spec §2.4 CfgRd/CfgWr encoding

---

### Requirement: pe-tlp-encode-pcie-spec-compliance (PCIe Base Spec §2.2)

`PcieTlpSerializer::serialize_mmio_*` SHALL 严格按 PCIe Base Spec §2.2 TLP Common Header 编码 (4DW header).

#### Scenario: MWr header 字段验证 (per AC-PE-1.1)

- **GIVEN** `serialize_mmio_write(bar=1, off=0x1000, data="ABCD", len=4)` 调用
- **THEN** 输出 PcieTlpBundle:
  - `kind = MWr (010 00000)` (Fmt=010, Type=00000)
  - `TC = 0` (Traffic Class best-effort)
  - `T  = 0` (no forced ordering)
  - `TD = 0` (no TLP digest)
  - `EP = 0` (no error poisoning)
  - `Attr = 0` (no special attributes)
  - `Length = 1` (DW count, per spec: Length is number of remaining DW)
  - `ReqId = <next_req_id>`
  - `Tag = <next_tag>`
  - `LastDWBE = 0b1111`, `FirstDWBE = 0b1111` (len=4 完整写)
  - `Address = 0x1000` (BAR1 base + offset, per VramController+BAR fast-path)
  - `Data = "ABCD" (32b)`

#### Scenario: MRd Length 字段 (per PCIe Spec §2.2.6)

- **GIVEN** MRd encode for len=12 (3 DW)
- **THEN** `Length = 3` (number of DW, per spec)
- **AND** `data_len = 12` (字节数 via separate data 字段)

#### Scenario: CfgRd Register Number 字段 (per PCIe Spec §2.4)

- **GIVEN** `serialize_config_read(off=0x04, width=4)`
- **THEN** `kind = CfgRd (001 01010)`
- **AND** Register Number = 0x04
- **AND** 完成数 = width in DW (=1 for 32b read)

---

### Requirement: pe-pcie-endpoint-submit-tlp-additive

`PcieEndpointIP` SHALL 加 1 个新 API `submit_tlp(const PcieTlpBundle&) noexcept`. 既有 17 端口 + 既有 API **完全不修改** (准 frozen additive-only).

#### Scenario: PcieEndpointIP::submit_tlp API 可见

- **GIVEN** `include/tlm/pcie/pcie_endpoint_ip.hh` 编译
- **THEN** `ep->submit_tlp(tlp)` 是合法 call

#### Scenario: 既有 17 端口 API 零 diff (per H-D3)

- **GIVEN** 头文件 `pcie_endpoint_ip.hh` 17 端口
- **THEN** `git diff HEAD -- include/tlm/pcie/pcie_endpoint_ip.hh` 仅显示 `+submit_tlp` (1 行新增), 无删除/修改

---

### Requirement: pe-decoder-dispatch (TLP decode + dispatch)

`PcieTlpDecoder` SHALL decode PcieTlpBundle + 路由到 BAR memory / Config Space / DMA target.

#### Scenario: MRd/MWr Dispatch (per AC-PE-1.2)

- **GIVEN** MWr TLP 到达 (Fmt=010/Type=00000)
- **WHEN** `PcieTlpDecoder::decode_and_dispatch` 调用
- **THEN** `decode_mwr` → `dispatch_to_bar(bar_from_address, address, data, len)` → `ep->memory_device().memory_write(off, buf, len)`
- **AND** 返 0=OK, -EIO=malformed
- **AND** **未来扩展**: MultiPort adapter 支持 BAR dispatcher 需时添加 (per D-AXI v1.5 B25 沿袭)

#### Scenario: CfgRd/CfgWr Dispatch (per AC-PE-1.3)

- **GIVEN** CfgRd TLP (Fmt=001/Type=01010) 到达
- **WHEN** `decode_and_dispatch` 调用
- **THEN** `decode_cfgrd` → `ep->config_space().read(off, &val)` → CplD generate
- **AND** `decode_cfgwr` 类似 + `config_space().write(off, val)`

#### Scenario: Malformed TLP 拒绝

- **GIVEN** Invalid Fmt (e.g. Fmt=11 reserved)
- **WHEN** `decode_and_dispatch` 调用
- **THEN** 返回 -EIO, 0 crash, log warning

---

### Requirement: pe-frozen-surface-zero-diff (Hard Invariant)

本 change SHALL 保持 23 ABI 零 diff + 5 frozen headers 零 diff (per ADR-DGPU-08 + AGENTS.md H3). 修改范围**仅限于**: 新增 `include/tlm/pcie/pcie_tlp_serializer.{hh,cc}` + `include/tlm/pcie/pcie_tlp_decoder.{hh,cc}`; `pcie_endpoint_ip.hh` **仅**新增 1 个 `submit_tlp` API; `dgpu_board_shell.cc` mmio_*/config_* 内部改造.

#### Scenario: 23 ABI 签名零变更

- `git diff HEAD -- include/abi/` 输出为空 (per ADR-088 §D5)

#### Scenario: 5 frozen headers 仅 pcie_endpoint_ip.hh +1 行

- `include/abi/cpptlm_emulator.h`: 0 diff
- `include/tlm/gpu/pcie_endpoint_tlm.h`: 0 diff
- `include/tlm/gpu/pcie_display_device.hh`: 0 diff
- `include/tlm/pcie/pcie_bundles_tlm.hh`: 0 diff
- `include/tlm/pcie/pcie_endpoint_ip.hh`: 仅 `+submit_tlp` 1 行 additive

---

### Requirement: pe-test-coverage (新)

本 change SHALL 新增 `[pcie][tlp-entry]` 10 cases.

#### Scenario: 10 cases 覆盖 (per design 测试策略)

```cpp
TEST_CASE("[pcie][tlp-entry]") {
  // T1-T3: encode + decode roundtrip (MWr/MRd/CfgRd-CfgWr)
  // T4-T6: PCIe Spec 字段映射 (header fields + CplD data + malformed rejection)
  // T7-T8: Host 端到端 (mmio_* 经 TLP path vs 直调 byte-level match)
  // T9-T10: 协同 (HostPcieTestStub 调 path 一致性 + [pcie] smoke 7 cases 保持)
}
```

#### Scenario: 既有 5 套件 0 回归

- `[pcie]` 430 cases / 36672 assn: ✅
- `[minimal_dgpu_soc]` 12 cases / 66 assn: ✅
- `[driver_visible]` 2 cases / 29 assn: ✅
- `[pcie-memory]` 17 cases / 48 assn: ✅
- `[realip]` 22 cases / 193 assn: ✅
- `[pcie-host-test]` (flr-rebar v1.1): ✅

---

## TEST TAGS

新增:
- `[pcie][tlp-entry]` — TLP entry layer 测试 (覆盖 10 cases)

并行/回归锁:
- `[lwc]`, `[vram_controller][functional]`, `[noc]` (Change A/B/C 已实施)
- `[pcie]`, `[minimal_dgpu_soc]`, `[driver_visible]`, `[pcie-memory]`, `[realip]` (既有)
- `[pcie-host-test]` (flr-rebar v1.1)

---

**Spec 版本**: v1.0 (per tasks.md Phase 1 Change D)
