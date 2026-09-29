# ADR-DGPU-06: AxiMemBundle vs PcieTlpBundle 边界严格分离

> **状态**: 📋 提案 (基于 D-AXI v1.3 B2 + v1.8 H1 wire-format 修正)
> **日期**: 2026-09-26
> **对应 D-AXI 修正**: **B1** (SDMA 字段名) + **B2** (SDMA 切型范围限定) + **H1** (v1.8 wire-format 写死全 PcieTlpBundle)
> **关联架构文档**: [../designs/dgpu-soc/architecture.md §3 AxiMemBundle 定义](../designs/dgpu-soc/architecture.md), [../designs/dgpu-driver/architecture.md §2 BAR 视角](../designs/dgpu-driver/architecture.md)
> **关联 OpenSpec**: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前, spec Requirement "AxiMemBundle wire-format (v1.8 H1 标 D3+ chip-internal 专用)")
> **配套**: D-AXI tasks.md v1.3 B1+B2, v1.7 P0.33 (H1), v1.8 N2 (H1 normative)

---

## 1. 背景

### 1.1 问题

Minimal DGpu SoC 涉及两类传输：

| 类别 | 传输域 | Wire-format | 典型场景 |
|------|--------|-------------|----------|
| **Chip-internal** | SoC 内部模块间 (SDMA ↔ GMMU ↔ PcieMemoryDevice ↔ MemoryTLM) | `AxiMemBundle` (4KB inline payload) | SDMA 写 VRAM、GMMU 读 PTE |
| **Board-level** | host ↔ board (PCIe 物理 BAR 边界) | `PcieTlpBundle` (TLP 级别) | BAR0/1/2 mmio_read/write、MSI-X vector |

**混用风险**：
- 把 `AxiMemBundle` 暴露给 host → driver 收到错误的 wire-format 解析失败
- 把 `PcieTlpBundle` 用于 chip-internal 端口 → StreamAdapter serialize 失败、事务处理开销过大
- 边界不清导致 D-AXI v1.0-v1.3 阶段 spec 内部矛盾（B2 修正源头）

### 1.2 Oracle/Metis 评审结论

D-AXI v1.0 → v1.8 5 轮评审中，wire-format 边界反复出现：

- **v1.3 B2**: SDMA 切型范围限定（仅 minimal_v1 chip-internal 切 mem_in/mem_out, board-level 端口保持）
- **v1.8 H1**: wire-format 矛盾（spec Requirement 显式声明 "AxiMemBundle SHALL NOT 出现在 host↔board 端口"）
- **v1.8 N2**: H1/H6 normative 文本应用（per tasks.md v1.8 P0.41）

### 1.3 现状约束

- `PcieTlpBundle` 已冻结（per ADR-DGPU-08 冻结面 4 头文件之一）
- `AxiMemBundle` 是 D-AXI 新建（per spec Requirement "AxiMemBundle wire-format"）
- SDMA 5 端口混合 wire-format（per v1.3 B2 修正）
- StreamAdapter round-trip 必须真实（非裸 serialize per N1 必测）

---

## 2. 决策

### 2.1 AxiMemBundle 边界 (chip-internal 专用)

**规则**：`AxiMemBundle` **SHALL NOT** 出现在 host↔board 端口

```cpp
// include/tlm/bundles/axi_mem_bundle.hh (新建)
class AxiMemBundle {
public:
    // 4KB inline payload (chip-internal AXI 协议载体)
    ch_uint<512> data;       // 真实宽度 64-bit (per ch_uint<512> 限制, 不阻塞)
    ch_uint<64>  address;
    ch_uint<8>   size;       // 1, 2, 4, 8 bytes (AXI burst len)
    ch_uint<1>   is_write;   // 0 = read, 1 = write
    ch_uint<8>   burst_type; // FIXED / INCR / WRAP
    ch_uint<4>   cache_attr; // DEVICE / NORMAL / WRITEBACK
    ch_uint<4>   prot;       // PRIVILEGE / SECURE
    // ... (chip-internal AXI 完整字段)
};
```

**使用范围**：
- ✅ SDMA `mem_in` / `mem_out` 端口
- ✅ GMMU PTE 读取（chip-internal AXI master port）
- ✅ PcieMemoryDevice `req_in` / `resp_out`（chip-internal slave port）
- ✅ MemoryTLM `req_in` / `resp_out`
- ❌ BAR0/1/2 mmio 端口（必须 PcieTlpBundle）
- ❌ MSI-X vector 端口

### 2.2 PcieTlpBundle 边界 (board-level 专用)

**规则**：`PcieTlpBundle` 仅用于 board-level 端口（per 冻结面约束）

```cpp
// include/tlm/gpu/pcie_bundles_tlm.hh (冻结面)
class PcieTlpBundle {
public:
    // TLP 级别（per PCIe spec）
    PcieTlpHeader  header;     // fmt/type/length/reqid/tag
    ch_uint<4096>  payload;    // 4KB TLP payload (最大)
    ch_uint<32>    tlp_length;
    ch_uint<1>     is_completion;
    // ... (PCIe TLP 完整字段)
};
```

**使用范围**：
- ✅ PcieEndpointIP ↔ host 端口（BAR MMIO + Config Space + MSI-X）
- ✅ PcieBarRouter 寄存器表访问
- ✅ SDMA `desc_in` / `done_out` / `host_out`（board-level 端口，minimal_v1 不实际接线）
- ❌ chip-internal 模块间（必须 AxiMemBundle）

### 2.3 SDMA 5 端口混合 wire-format（关键约束）

**per v1.3 B2 修正**：

```cpp
class SdmaEngineTLM {
public:
    // ── chip-internal 端口 (切型为 AxiMemBundle, per B2) ──
    SlavePort <AxiMemBundle>  mem_in;     // 接受 host bulk data (chip-internal)
    MasterPort<AxiMemBundle>  mem_out;    // 写 VRAM (chip-internal AXI to PcieMemoryDevice)

    // ── board-level 端口 (保持 PcieTlpBundle, per B2 + H1) ──
    SlavePort <PcieTlpBundle> desc_in;    // 接收 descriptor (minimal_v1 不接线, 经 BAR1 ring doorbell mmio 路径)
    MasterPort<PcieTlpBundle> done_out;   // 完成通知 (minimal_v1 不接线)
    MasterPort<PcieTlpBundle> host_out;   // host-pull (minimal_v1 不接线)
};
```

**约束**：
- chip-internal 端口切型为 AxiMemBundle（per B2 限定 minimal_v1 范围）
- board-level 端口保持 PcieTlpBundle（**不**切型）
- dual-mode 测试路径保留：`set_vram_backdoor()` 无条件接受 raw pointer（per v1.3 B6 dual-mode 兼容）

### 2.4 StreamAdapter round-trip 真实（非裸 serialize）

**per spec Requirement "AxiMemBundle 经真实 StreamAdapter round-trip (N1 必测, 不做裸 serialize)"**：

```cpp
// test/axi_mem_bundle_stream_roundtrip.cc
TEST_CASE("AxiMemBundle 经真实 StreamAdapter round-trip", "[axi][bundles][stream]") {
    AxiMemBundle orig;
    orig.data.write(0xDEADBEEFCAFEBABE);
    orig.address.write(0x10000);
    orig.size.write(8);
    orig.is_write.write(1);

    // 经 StreamAdapter 序列化 → 反序列化（非裸 memcpy）
    auto serialized = stream_adapter.serialize(orig);
    auto deserialized = stream_adapter.deserialize<AxiMemBundle>(serialized);

    REQUIRE(deserialized.data.read() == 0xDEADBEEFCAFEBABE);
    REQUIRE(deserialized.address.read() == 0x10000);
    REQUIRE(deserialized.size.read() == 8);
    REQUIRE(deserialized.is_write.read() == 1);
}
```

---

## 3. 关键不变性 (Invariants)

### Inv-1: AxiMemBundle 仅 chip-internal 端口

**Where**: `include/tlm/bundles/axi_mem_bundle.hh` + 所有 StreamAdapter 端口定义

**测试**:
```bash
# grep 检查 AxiMemBundle 不出现在 board-level 端口
grep -rn "AxiMemBundle" include/tlm/pcie/  # 期望: 零匹配
```

### Inv-2: PcieTlpBundle 仅 board-level 端口

**Where**: `include/tlm/gpu/pcie_bundles_tlm.hh`（冻结面）

**测试**:
```bash
# grep 检查 PcieTlpBundle 不出现在 chip-internal 端口
grep -rn "PcieTlpBundle" include/tlm/memory_tlm.hh include/tlm/gpu/gmmu_tlm.hh  # 期望: 零匹配
```

### Inv-3: 不可互换

**Where**: 编译期强制（类型不兼容即编译失败）

```cpp
// 编译失败: type mismatch
SlavePort<PcieTlpBundle> mem_in;  // 错误: chip-internal 端口不能用 PcieTlpBundle
```

### Inv-4: SDMA 5 端口混合 wire-format

**Where**: `include/tlm/gpu/sdma_engine_tlm.hh` 端口定义

**测试**:
```cpp
TEST_CASE("SDMA 5 端口 wire-format 配置", "[sdma][wire-format]") {
    REQUIRE(std::is_same_v<decltype(sdma_->mem_in)::bundle_type, AxiMemBundle>);
    REQUIRE(std::is_same_v<decltype(sdma_->mem_out)::bundle_type, AxiMemBundle>);
    REQUIRE(std::is_same_v<decltype(sdma_->desc_in)::bundle_type, PcieTlpBundle>);
    REQUIRE(std::is_same_v<decltype(sdma_->done_out)::bundle_type, PcieTlpBundle>);
    REQUIRE(std::is_same_v<decltype(sdma_->host_out)::bundle_type, PcieTlpBundle>);
}
```

---

## 4. 实施步骤

per D-AXI tasks.md **v1.3 P0.2 B2 + v1.7 P0.33 H1 + v1.8 P0.41 N2**:

| 步骤 | 任务 | 位置 |
|------|------|------|
| 1 | AxiMemBundle 定义 (4KB inline payload) | include/tlm/bundles/axi_mem_bundle.hh |
| 2 | SDMA 切型范围限定 (mem_in/mem_out 切 AxiMemBundle) | include/tlm/gpu/sdma_engine_tlm.hh |
| 3 | GMMU chip-internal AXI master port 切型 | include/tlm/gpu/gmmu_tlm.hh |
| 4 | PcieMemoryDevice chip-internal slave port 切型 | include/tlm/gpu/pcie_memory_device.hh |
| 5 | MemoryTLM chip-internal slave port 切型 | include/tlm/memory_tlm.hh |
| 6 | StreamAdapter round-trip 测试 (N1) | test/axi_mem_bundle_stream_roundtrip.cc |
| 7 | spec Requirement "AxiMemBundle wire-format" 落地 | OpenSpec spec.md |
| 8 | normative 文本应用 (per v1.8 N2) | OpenSpec spec.md |

**预计工时**: 3-4 工作日 (per t1.md Phase 0.2 + Step 3)

---

## 5. 测试策略

### 5.1 单元测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_axi_mem_bundle_stream_roundtrip | AxiMemBundle 经真实 StreamAdapter round-trip (N1) | `[axi][bundles][stream]` |
| test_axi_mem_bundle_slverr | AxiMemBundle MEM_READ 请求 + SLVERR | `[axi][bundles][slverr]` |
| test_pcie_tlp_bundle_no_chip_internal | PcieTlpBundle 不出现在 chip-internal 端口 | `[pcie][bundles][isolation]` |

### 5.2 集成测试

| 测试 | 验证内容 | 标签 |
|------|---------|------|
| test_sdma_5_ports_wire_format | SDMA 5 端口混合 wire-format 正确 | `[sdma][wire-format]` |
| test_minimal_v1_chip_internal_only | minimal_v1 仅 chip-internal 切型 | `[minimal_dgpu_soc][wire-format]` |
| test_h2d_via_mem_out | SDMA H2D 经 mem_out AxiMemBundle 写 PcieMemoryDevice | `[sdma][h2d][axi]` |

### 5.3 回归测试

- 既有 [sdma] dual-mode 测试零逻辑改动（per v1.3 B6 dual-mode 保留 set_vram_backdoor）
- `[pcie]` baseline 全绿

---

## 6. 兼容性

### 6.1 ABI 兼容性

- AxiMemBundle 是**新** Bundle，**不**影响 ABI（per ADR-DGPU-08）
- PcieTlpBundle 不变（冻结面 4 头文件之一）

### 6.2 Wire-format 兼容性

- chip-internal 与 board-level 边界明确，不混用
- StreamAdapter serialize/deserialize 行为确定

### 6.3 测试兼容性

- `[sdma]` 套件零逻辑改动（dual-mode 保留）
- `[pcie]` 套件 baseline 全绿

---

## 7. 风险与缓解

| 风险 | 严重性 | 缓解 |
|------|--------|------|
| 误用 AxiMemBundle 在 board-level 端口 | 🔴 H | 编译期类型检查 + grep 验证 |
| StreamAdapter serialize 误用导致数据损坏 | 🔴 H | N1 必测 round-trip + SLVERR 测试 |
| wire-format 边界漂移（v1.0-v1.3 spec 矛盾复现） | 🟡 M | v1.8 H1/N2 normative 文本固化 |
| dual-mode 测试破坏 | 🟡 M | `set_vram_backdoor()` 保留无条件 dual-mode API |

### Oracle/Metis 评审重点

- **Oracle v1.3 第五轮 B1+B2**: SDMA 切型范围限定 + 字段名
- **Oracle v1.7 H1**: wire-format 矛盾
- **Oracle v1.8 N2**: H1/H6 normative 文本应用

---

## 8. 参考

- D-AXI OpenSpec change: [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8)
- D-AXI spec.md Requirement "AxiMemBundle wire-format"
- D-AXI design.md §3 AxiMemBundle 定义 + §6 SDMA 5 端口换型
- D-AXI tasks.md v1.3 P0.2 B2 + v1.7 P0.33 H1 + v1.8 P0.41 N2
- 实施笔记: [../../pcie/driver-visible-minimal-soc.md §3.6](../../pcie/driver-visible-minimal-soc.md)
- 实施计划: [../superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md Phase 0.2 + Step 3](../superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md)
- [include/tlm/bundles/axi_mem_bundle.hh](../../include/tlm/bundles/axi_mem_bundle.hh)（新建）
- [include/tlm/gpu/pcie_bundles_tlm.hh](../../include/tlm/gpu/pcie_bundles_tlm.hh)（冻结面）

---

## Status Update

### 2027-02-09 — Phase 3 实施完成 (driver-visible-minimal-soc v1.8)

**实施 change**: `openspec/changes/cpptlm-driver-visible-minimal-soc` (Phase 3: T3-FIX + T4)

**v1.8 H1 不变性验证**:

| Invariant | 状态 | 验证 |
|-----------|------|------|
| SDMA 5 端口全部 PcieTlpBundle (desc_in/mem_in/mem_out/host_out/done_out) | ✅ | `sdma_engine_tlm.hh` 5 端口 `InputStreamAdapter<PcieTlpBundle>` / `OutputStreamAdapter<PcieTlpBundle>` 同构 |
| `AxiMemBundle` SHALL NOT 出现在 minimal_v1 任何实际线路 | ✅ | SDMA↔PcieMemoryDevice、GMMU↔PcieMemoryDevice 均走 PcieTlpBundle (MEM_READ/MEM_WRITE/CPLD) |
| PcieMemoryDevice 2 SlavePorts + 单 adapter (v1.3 B3) | ✅ | `pcie_memory_device.hh` NUM_PORTS=2, `MultiPortStreamAdapter` 单注入 |
| GMMU translate MasterPort PcieTlpBundle (H1) | ✅ | GMMU req_out → pcie_memory.1 经 PcieTlpBundle MEM_READ |

**验证结果**: `[sdma]` 56 cases / 12884 assertions + `[pcie-memory]` 24 cases + `[driver_visible]` 2 cases (29 assertions) + ctest 76/76 PASS, 0 regression; ABI 冻结面 0 diff。

