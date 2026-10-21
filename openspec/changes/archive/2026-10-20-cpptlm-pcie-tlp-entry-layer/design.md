# Design — cpptlm-pcie-tlp-entry-layer v1.0 (Phase 1 Change D)

> **配套**: [`proposal.md`](proposal.md) · [`tasks.md`](tasks.md) · [`specs/pcie-tlp-entry/spec.md`](specs/pcie-tlp-entry/spec.md)
> **目标 tag**: `cpptlm-pcie-tlp-entry-completion`
> **估时**: **~8d** (TDD 5-step + TLP encode/decode + DGpuBoard invasive change)
> **Frozen surface**: 23 ABI + **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面)
> **依赖**: flr-rebar v1.1 ✅ + p2-integration-unblock ✅ + Change B ✅ + Change C ✅

---

## 总体设计

### 核心思路

**目标**: 修复 §0.1 Gap M (PCIe 入口 TLP 序列化层缺失) — 在 DGpuBoard 与 PcieEndpointIP 之间引入**TLP 序列化层**, 让 host API (`mmio_*`/`config_*`) 内部生成 PCIe TLP 报文, PcieEndpointIP 端 decoder dispatch.

**TLP encoding**: 严格按 **PCIe Base Spec 4.0 §2.2.1 + §2.2.5 + §2.4** TLP Common Header 格式 (**v0.5.1 重画 per Oracle BLOCKER B2 修订** — byte-level layout, Length/RequesterID/BE 位置正确, Type 值正确):

```
TLP 4DW Common Header (3DW 模式 for MRd/MWr 32-bit addr; 4DW for 64-bit addr or Cfg; 4DW for CplD):

DW0 (32 bits, byte-level layout per PCIe 4.0 §2.2.1):
  Byte 0 (bits [31:24]): Fmt[2:0] (3b) | Type[4:0] (5b)              =  8 bits
  Byte 1 (bits [23:16]): TC[2:0] (3b) | R (1b) | TH (1b) | TD (1b) | EP (1b) | Attr[2] (1b)  =  8 bits
  Byte 2 (bits [15:8]):   Attr[1:0] (2b) | AT[1:0] (2b) | Length[9:8] (2b) | R[1:0] (2b)  =  8 bits
  Byte 3 (bits [7:0]):    Length[7:0] (8b)                                =  8 bits
  Total: 32 bits = 3 + 5 + 3 + 1 + 1 + 1 + 1 + 1 + 2 + 2 + 2 + 10 = 31 bits (with 1 bit R reserved) ✓

DW1 (MRd/MWr/CfgRd/CfgWr, bits [63:32]):
  Requester ID[15:0] (16b) | Tag[7:0] (8b) | Last DW BE[3:0] (4b, for MWr) | First DW BE[3:0] (4b, for MWr)  = 32 bits

DW2 (MRd/MWr 3DW, Cfg, Cpl — depending on TLP type):
  - MRd/MWr 3DW (32-bit addr):  Address[31:0] (32b)
  - MRd 4DW (64-bit addr):       Address[63:32] (32b)
  - CfgRd0/CfgWr0:              Bus# (8b) | Device# (5b) | Function# (3b) | ExtReg# (4b) | Reg# (5b) | Rsv (7b)

DW3 (4DW only):
  - MRd 4DW:                    Address[31:0] (32b)
  - CfgWr0 4DW:                 Write Data[31:0] (32b)
  - CplD:                       [Byte0 Rsv(1b) | Status[2:0] (3b) | BCM (1b)] + Completer ID[15:0] (16b) + Requester ID[15:0] (16b) (DW3 = 32b) + Lower Address (DW4+ if completer expects)
  - Cpl (no data):              [Status+BCM+Reserved] + Completer ID + Requester ID
```

**v0.5.1 修正要点** (per Oracle Phase BLOCKER B2 + Metis Δ-3 + Phase E 全面审计):
- **Fmt 是 3 bit** (per PCIe Spec §2.2.1): `000`/`001`/`010`/`011` — 4 个有效值
- **Length 是 10 bit 在 DW0** (bits 9:0), 不是在 DW1 — 上一轮 v0.5 错放在第二行已修
- **Requester ID 在 DW1** (bits 31:16), 不是 DW0
- **Tag + Last/First DW BE 在 DW1** (bits 15:0), 不是 DW2/DW3
- **Cfg 请求 DW2 是 Bus/Device/Function/ExtReg/Reg**, 不是 "Message Class" (后者仅 Msg/MsgD TLP 存在)
- **Address 是 DW2 (3DW) 或 DW2-3 (4DW)**, 不是 "DW4+"
- **无伪字段 "Vendor (8b)" / "Rsv (4b)" / "DWORD BE (4b)"** (这是错误归并)

**FMT/Type 路由** (per PCIe Spec §2.4 + §2.2.4 — **v0.5.1 修正 CfgRd/CfgWr Type 值**):
- **MRd**: Fmt=`000` (3DW hdr) 或 `001` (4DW hdr with 64-bit addr) + **Type=`00000`**
- **MWr**: Fmt=`010` (3DW hdr with 1 DW data) 或 `011` (4DW hdr) + **Type=`00000`**
- **CfgRd0**: Fmt=`000` + **Type=`10100` (= 0x14)**
- **CfgRd1**: Fmt=`001` + **Type=`10101` (= 0x15)**
- **CfgWr0**: Fmt=`000` + **Type=`10110` (= 0x16)**
- **CfgWr1**: Fmt=`001` + **Type=`10111` (= 0x17)**
- **Cpl**: Fmt=`000` + **Type=`01010` (= 0x0A)**
- **CplD**: Fmt=`010` + **Type=`01010` (= 0x0A)**
- **注意**: 上一轮 v0.5 把 CfgRd Type 误写为 `01010` (与 Cpl 冲突); v0.5.1 修正 CfgRd Type=`10100`/`10101` 与 Cpl Type=`01010` 区分.

### 实施前后对比

**实施前** (Gap M violation):
```
Host API (DGpuBoard::mmio_*)
   │
   │ 直调函数 (sync)
   ▼
DGpuBoard::mmio_write/bind → ep->memory_device().memory_write(...) 直连
   │
   ▼
PcieMemoryDevice::memory_write → 字节级直写 (skip TLP)
```

**实施后** (Gap M 修复):
```
Host API (DGpuBoard::mmio_*) [23 ABI 不变]
   │
   │ 内部改造: 先生成 PcieTlpBundle (MWr with payload)
   ▼
PcieTlpSerializer::serialize_mmio_write(bar, off, data)
   │
   │ encode: Fmt=010 Type=00000 + TC=0 + T=0 + TD=0 + EP=0 + Length=N + ReqId + Tag + 4DW BE + Address/Off + Data
   ▼
PcieTlpBundle (per PcieEndpointIP 17 ports 已有)
   │
   │ ep->submit_tlp(bundle)
   ▼
PcieEndpointIP::submit_tlp() (新增 API, TLP 入口)
   │
   │ TLP decode (PcieTlpDecoder): Fmt/Type dispatch
   ├── MRd/MWr → BAR memory read/write (走 dgpu_board_shell 直调 chip-internal path)
   ├── CfgRd/CfgWr → PcieConfigSpace read/write
   ├── 完成 → CplD 生成 (read) 或 Cpl (write)
   ▼
return to DGpuBoard
```

### 关键设计不变式

- **D-H1**: 23 ABI 签名零变更 (per ADR-088 §D5)
- **D-H2**: **4 frozen** headers 零 diff (per AGENTS.md D-AXI 冻结面: `cpptlm_emulator.h`/`pcie_endpoint_tlm.h`/`pcie_display_device.hh`/`pcie_bundles_tlm.hh`) + **2 quasi-frozen** (`pcie_endpoint_ip.hh`/`pcie_axi_adapter_tlm.hh`, additive only, per v0.5 Change B 修订)
- **D-H3**: TLP encode 严格按 PCIe Base Spec §2.2 (避免系统误 bit flipping)
- **D-H4**: TLP decode 在 PcieEndpointIP 内部实施 (per design §总体设计), 不外移到 DGpuBoard
- **D-H5**: 协同 HostPcieTestStub — TLP path 是 host→board 的真实路径, 不绕过
- **D-H6**: 23 ABI 函数在 Board 层入口仍然可调; TLP encode 在 Board 内部自动调用 (这正是 0 直调 chip-internal 的体现)

### Data Flow

**MRd 路径** (Host 读 BAR):
```
1. cpptlm_emulator_mmio_read(bar, off, buf, len)
   └→ DGpuBoard::mmio_read(bar, off, buf, len)
      └→ serialize PcieTlpBundle(MRd, off, len, req_id)
      └→ ep->submit_tlp(bundle)
        └→ PcieTlpDecoder::decode_mrd → dispatch to BAR
          └→ ep->memory_device().memory_read(off, buf, len)
            └→ CplD TLP 生成 (Completion with Data)
        └→ return data to mmio_read
      └→ memcpy(buf, data, len); return 0
```

**MWr 路径** (Host 写 BAR):
```
1. cpptlm_emulator_mmio_write(bar, off, buf, len)
   └→ DGpuBoard::mmio_write → serialize MWr → ep->submit_tlp
      └→ PcieTlpDecoder::decode_mwr → dispatch to BAR
        └→ ep->memory_device().memory_write(off, buf, len)
   └→ return 0
```

**CfgRd/CfgWr 路径** (Host 读/写 PCIe Config Space):
```
- pcie_config_read(off, width, &val) → serialize CfgRd → ep->submit_tlp
  → PcieTlpDecoder::decode_cfgrd → dispatch to PcieConfigSpace
  → 完成 → CplD
- pcie_config_write(off, width, val) → serialize CfgWr → ep->submit_tlp
  → PcieTlpDecoder::decode_cfgwr → dispatch to PcieConfigSpace
```

---

## API 设计 (4 件套核心 API)

### 1. `PcieTlpSerializer` (新增, `include/tlm/pcie/pcie_tlp_serializer.hh`)

```cpp
namespace tlm::pcie {

// 工具类: 序列化 ABI 调用到 PcieTlpBundle
class PcieTlpSerializer {
public:
    explicit PcieTlpSerializer(uint16_t next_req_id_ = 1) : next_req_id_(next_req_id_) {}
    
    // encode mmio_write (MWr)
    bundles::PcieTlpBundle serialize_mmio_write(
        uint8_t bar, uint64_t offset, const void* data, size_t len);
    
    // encode mmio_read (MRd)
    bundles::PcieTlpBundle serialize_mmio_read(
        uint8_t bar, uint64_t offset, size_t len);
    
    // encode pcie_config_read/write (CfgRd/CfgWr)
    bundles::PcieTlpBundle serialize_config_read(uint16_t offset, uint8_t width);
    bundles::PcieTlpBundle serialize_config_write(uint16_t offset, uint8_t width, uint32_t value);
    
    // decode CplD (return data) (PcieTlpDecoder 共用)
    int deserialize_completion(const bundles::PcieTlpBundle& cpl,
                              void* buf, size_t len) noexcept;
    
private:
    uint16_t next_req_id_ = 1;
    uint8_t  next_tag_ = 0;
    
    uint16_t allocate_req_id() noexcept { return next_req_id_++; }
    uint8_t  allocate_tag() noexcept { return next_tag_++; }
};

}  // namespace tlm::pcie
```

### 2. `PcieEndpointIP::submit_tlp()` (新增, PcieEndpointIP 头文件 additive)

```cpp
// [v1.0 NEW + Phase C 补] TLP 入口 (per Gap M 修复)
// 既有 17 端口 signature 不变, 仅新增 2 个 API (sync + async, per Metis Δ-4 修订)
class PcieEndpointIP : public SimModule {
public:
    // 既有 API 完全不变 (signature 零 diff, quasi-frozen)

    // [v1.0 NEW] TLP entry sync API (write / non-posted w/ immediate Cpl on sync path)
    int submit_tlp(const bundles::PcieTlpBundle& tlp) noexcept;
    // 返 0=OK, -EAGAIN=pending read (走 sim 线程), -EIO=error

    // [Phase C 加] TLP entry async API (read with CplD, per Phase C Metis Δ-4 修订)
    std::future<bundles::PcieTlpBundle> submit_tlp_async(const bundles::PcieTlpBundle& tlp) noexcept;
    // 返 future<CplD>, 用户 mmio_read 在 host 线程 wait_for(50ms, per implicit PCIe completion timeout)
    // 实现: 内部 dispatch → CplD 生成 → std::promise.set_value
};
```

### 3. `PcieTlpDecoder` (新增, `include/tlm/pcie/pcie_tlp_decoder.hh`)

```cpp
namespace tlm::pcie {

// PcieTlpDecoder 在 PcieEndpointIP 内部 (per D-H4)
class PcieTlpDecoder {
public:
    explicit PcieTlpDecoder(PcieEndpointIP* ep) : ep_(ep) {}
    
    // decode + dispatch
    // 返 0=OK (write dispatched), -EAGAIN=pending read (走 sim 线程), -EIO=error
    int decode_and_dispatch(const bundles::PcieTlpBundle& tlp);
    
    // Cpl 生成
    bundles::PcieTlpBundle make_completion_with_data(
        uint16_t compl_status, uint16_t req_id, uint8_t tag,
        const void* data, size_t len);
    bundles::PcieTlpBundle make_completion(uint16_t compl_status, uint16_t req_id, uint8_t tag);
    
private:
    PcieEndpointIP* ep_;
    uint16_t compl_status_ = 0;
    
    int handle_mrd(const bundles::PcieTlpBundle& tlp);
    int handle_mwr(const bundles::PcieTlpBundle& tlp);
    int handle_cfgrd(const bundles::PcieTlpBundle& tlp);
    int handle_cfgwr(const bundles::PcieTlpBundle& tlp);
};

}  // namespace tlm::pcie
```

### 4. DGpuBoard 内部改造 (`src/tlm/gpu/dgpu_board_shell.cc`)

```cpp
// [v1.0 NEW] DGpuBoard hold PcieTlpSerializer
class DGpuBoard : public ChipObject {
private:
    tlm::pcie::PcieTlpSerializer tlp_serializer_;  // [v1.0 NEW]
    // ... 既有字段不变
};

// mmio_write 改造 (Gap M 修复):
int DGpuBoard::mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len) {
    // 既有 lifecycle guard + power gate 保留
    // ...
    
    // [v1.0 NEW] TLP 序列化
    auto tlp = tlp_serializer_.serialize_mmio_write(bar, offset, buf, len);
    
    // 既有 router (PcieControllerIP facade) 不变 — 仅把 chip-internal 直调改为 ep->submit_tlp
    auto* ep = pcie_ep();
    if (ep != nullptr) {
        return ep->submit_tlp(tlp, /*completion*/ std::nullopt);
    }
    return -ENODEV;
}

// mmio_read 改造:
int DGpuBoard::mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len) {
    // ...
    auto tlp = tlp_serializer_.serialize_mmio_read(bar, offset, len);
    auto* ep = pcie_ep();
    if (ep == nullptr) return -ENODEV;
    
    // 同步等待 CplD (per existing inject_q_/pending_resp_ 50ms timeout)
    // [v1.0 NEW] 改用 future + LWC cr_read() 模式 (per Change B 仿)
    auto fut = ep->submit_tlp_async(tlp);  // 返回 future<CplD>
    // wait_for(kMmioWaitTimeout) timeout (per Change B 50ms)
}
```

---

## Backward Compatibility

| 场景 | 行为 |
|------|------|
| 既有 `[pcie]` / `[minimal_dgpu_soc]` / `[driver_visible]` 测试 | 23 ABI 签名 0 变, 内部行为不变 (TLP 路径等价于直调 chip-internal, 但增加 1 sim tick 延迟); 测试保持全绿 |
| 既有 `ep->memory_device().memory_write(...)` 直调 | 内部 PcieTlpDecoder decode 后仍调此 API, 无 diff |
| HostPcieTestStub (flr-rebar v1.1) | 调 `DGpuBoard::trigger_flr/resize_bar/mmio_*/config_*` 不变 (per flr-rebar v1.1 §3.1), 走 TLP 序列化层 |
| `cpptlm-p2-integration-unblock` | CP 集成通过 `DGpuBoard::*` 走 TLP 路径, 集成延迟 1-2 sim tick |

---

## 关键不变式 (Hard Invariants)

- **H-D1**: 23 ABI 签名零变更 (per ADR-088 §D5)
- **H-D2 (v0.5.1 修订)**: **4 frozen headers 零 diff + 1 quasi-frozen (additive only)** (per AGENTS.md H3 D-AXI 冻结面: `cpptlm_emulator.h` + `pcie_endpoint_tlm.h` + `pcie_display_device.hh` + `pcie_bundles_tlm.hh`; quasi = `pcie_endpoint_ip.hh` **仅新增 submit_tlp + submit_tlp_async 2 个 API**; `pcie_axi_adapter_tlm.hh` 无任何 change 触碰, 不计入 quasi)
- **H-D3**: `pcie_endpoint_ip.hh` **仅新增 `submit_tlp` 1 个 API**, 不删不改既有 17 端口 signature
- **H-D4**: TLP encode 严格按 PCIe Base Spec §2 (validate via 测试)
- **H-D5**: 协同 HostPcieTestStub — TLP path 是 host→board 的真实路径, 不绕过

---

## 测试策略 (10 cases / 30+ assertions)

```cpp
TEST_CASE("[pcie][tlp-entry]") {
  // 1. encode + decode roundtrip (MWr encode → decode → verify match)
  // 2. encode + decode roundtrip (MRd encode → decode → verify match)
  // 3. encode + decode roundtrip (CfgRd/CfgWr)
  // 4. TLP Header field 校验 (Fmt + Type + TC + Length + ReqId + Tag + BE + Address)
  // 5. MRd CplD 数据完整性
  // 6. MWr 完成后 Cpl 生成
  // 7. 错误 TLP 拒绝 (-EIO, malformed header)
  // 8. PCIe Base Spec §2.2 字段映射测试
  // 9. cross-validation: 与既有 pcie_endpoint_ip_basic tests 协同 (smoke 7 cases 保持)
  // 10. 端到端: cpptlm_emulator_mmio_read/write 经 TLP 路径 vs 直调字节级 match
}
```

### 关键测试 invocation

```bash
# Phase 1 RED
./build/bin/cpptlm_tests "[pcie][tlp-entry]" --reporter compact

# Phase 5 0 回归锁
./build/bin/cpptlm_tests "[pcie]" --reporter compact  # 430 cases
./build/bin/cpptlm_tests "[minimal_dgpu_soc]" --reporter compact  # 12 cases
./build/bin/cpptlm_tests "[driver_visible]" --reporter compact  # 2 cases
./build/bin/cpptlm_tests "[pcie-memory]" --reporter compact  # 17 cases
./build/bin/cpptlm_tests "[realip]" --reporter compact  # 22 cases

# 协同 HostPcieTestStub (per flr-rebar v1.1)
./build/bin/cpptlm_tests "[pcie-host-test]" --reporter compact
```

---

## 风险与缓解

| Risk | Phase | Mitigation |
|------|-------|-----------|
| TLP encode/decode bit field 出错 → 数据 corruption | Phase 2 | spec compliance 测试 (10 cases) + cross-check 与 [pcie] 既有测试 |
| mmio_read 50ms timeout 触发频次增加 (TLP decode 比直调慢) | Phase 3 | 实测 R3 (per Change B); timeout 50ms 是否够? 必要时调整 |
| 23 ABI 签名破坏 → flr-rebar v1.1 / p2-integration-unblock 阻断 | Phase 3 | commits 前 `git diff HEAD -- include/abi/` 必须为空 (per D-H1) |
| mmio_read/write 内部 invasive change 破坏既有测试 | Phase 4 | 全 4 套件 0 回归锁 |
| 协同 Change C (GMMU wire-format) 可能影响 TLP path 测试 | Phase 5 | Change C 优先 archive 后 D 实施, 或并行 review |

---

## 文件清单

| 文件 | 类型 | 估时 |
|------|------|------|
| `include/tlm/pcie/pcie_tlp_serializer.{hh,cc}` | 新增 (~350 行) | 1.5d |
| `include/tlm/pcie/pcie_tlp_decoder.{hh,cc}` | 新增 (~250 行) | 1d |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 修改 (+1 行 `submit_tlp()`) | 0.05d |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | 修改 (~50 行 TLP decode integration) | 1d |
| `src/tlm/gpu/dgpu_board_shell.cc` | 修改 (~80 行 mmio_* + config_* 改造) | 1.5d |
| `test/test_pcie_tlp_entry_layer.cc` | 新增 (~250 行, 10 cases) | 2d |
| `test/test_cpptlm_emulator_minimal_soc_smoke.cc` | 修订 (+1 case 验 TLP entry) | 0.5d |
| `docs/implementation/2026-10-09-pcie-tlp-entry-layer.md` | 新增 (~50 行) | 0.25d |
| `docs/designs/dgpu-soc/architecture.md` §0.2 Gap M ✅ | 文档同步 | 0.1d |
| `AGENTS.md` KEY INVARIANTS | 文档同步 | 0.1d |
| **总计** | | **~8d** |

---

**Owner**: CppTLM Team (Sisyphus)
**最后更新**: 2026-10-09 (v1.0 起草, Phase 1 Change D 启动)
