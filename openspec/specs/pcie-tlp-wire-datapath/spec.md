# pcie-tlp-wire-datapath Specification

## Purpose
TBD - created by archiving change 2026-09-16-cpptlm-pcie-tlp-wire-datapath. Update Purpose after archive.
## Requirements
### Requirement: tlp-codec-wire-format

CppTLM **SHALL** 提供 wire-format TLP 编码器，支持 PCIe Base Specification 定义的 3DW/4DW header、ECRC (CRC-32, poly 0x04C11DB7)、LCRC-32、DLLP CRC-16 计算与校验。

理由: 既有 `PcieTlpBundle` 是 6-kind 描述符（kind/bar_index/offset/size/data/requester_id/trans_id），**缺 Fmt/Type/Length/DW0/ECRC/LCRC** 报文级字段；全仓库零 CRC 实现。完整 TLP 链路闭合需要报文级编解码。

范围:
- 编码：3DW/4DW header (Fmt/Type/TC/Attr/TD/Length/Requester ID/Tag/Last BE/First BE/Address) + payload + ECRC
- 解码：parser 抽取 header 字段 + 校验 LCRC + 校验 ECRC（TD=1）
- DLLP 编码（ACK/NAK/InitFC/UpdateFC/NOP）+ CRC-16
- Malformed TLP 判别（header 非法 Fmt/Type 组合、Length=0 等）
- **wire→descriptor 转换点**：转换发生在 **`DGpuBoard::mmio_write` 内部**（具体路径：`mmio_write` → 构造 `PcieTlpWireBundle` → `PcieTlpEncoder` 编码 → 在 Encoder 内部或紧接 Encoder 出口处将 `PcieTlpWireBundle` 转为既有 `PcieTlpBundle`（descriptor）→ 送 `PcieLinkLayer::rx_tlp_from_host`）。LL 接口签名 `const PcieTlpBundle&`（`pcie_link_layer_tlm.hh:160`）保持不变，**LL 接口绝不修改**。wire 格式仅用于：(a) golden 快照测试、(b) CRC 校验、(c) 错误注入点位。**不修改 LL 任何接口签名**（会破坏既有 14+ Phase 1-4 测试）
- wire-format 快照测试：golden hex dump 比对

#### CRC 参数表

| 字段 | 多项式 | 初值 | 反射 | Residue | 位置 |
|------|--------|------|------|---------|------|
| ECRC (PCIe §2.7) | 0x04C11DB7 | 0xFFFFFFFF | Yes (LSB-first) | non-zero | TLP 末尾 4 字节,仅当 TD bit=1 |
| LCRC (PCIe §2.7) | 0x04C11DB7 | 0xFFFFFFFF | Yes (LSB-first) | non-zero | TLP 末尾 4 字节(ECRC 之后或替代) |
| DLLP CRC-16 (PCIe §3.4) | 0x100B | 0xFFFF | Yes (LSB-first) | non-zero | DLLP 末尾 2 字节 |

实现位置:
- `include/tlm/pcie/pcie_tlp_codec.hh` + `src/tlm/pcie/pcie_tlp_codec.cc` (新)
- `include/bundles/pcie_bundles_tlm.hh` (追加 `PcieTlpWireBundle`，保留既有 `PcieTlpBundle` descriptor)
- `test/test_pcie_tlp_codec_wire_format.cc` (新)

#### Scenario: 编码 MWr TLP

- **WHEN** 调用 `TlpCodec::encode_mwr(bdf, bar, offset, data, len)` 构造 4KB MWr TLP
- **THEN** 返回 `PcieTlpWireBundle` 包含：3DW header (Fmt=`010`, Type=`00000` **MWr32**) 或 4DW header (Fmt=`011`, Type=`00000` **MWr64**) + payload + ECRC
- **AND** header.Length 字段按 `(len + 3) / 4` (DW 单位) 填充
- **AND** ECRC 字段值通过 CRC-32 计算验证（poly 0x04C11DB7）

#### Scenario: 编码 CplD TLP

- **WHEN** 调用 `TlpCodec::encode_cpld(completer_id, requester_id, tag, byte_count, data)` 构造 64B CplD
- **THEN** 返回 `PcieTlpWireBundle` 包含 3DW header (Fmt=010, Type=01010 CplD) + payload + ECRC
- **AND** Requester ID 回显原始 MRd 的源 ID，Tag 回显原始 MRd 的 Tag

#### Scenario: 校验 TLP wire-format

- **WHEN** 解码 `PcieTlpWireBundle` 验证 LCRC
- **THEN** 若 LCRC 不匹配返回 false（Malformed TLP）
- **AND** 若 TD=1 且 ECRC 不匹配返回 false（CRC Error）

#### Scenario: golden hex dump 比对

- **WHEN** 测试运行 `test_pcie_tlp_codec_wire_format.cc`
- **THEN** golden 文件（与真实 PCIe spec 4KB MWr hex dump 比对）全 PASS
- **AND** 跨平台一致（little-endian / big-endian 字节序文档化）

---

### Requirement: completer-engine-full

CppTLM **SHALL** 提供 `PcieCompleterEngine` 替换 `PcieSriovVfPool::dispatch_tlp` 的 `default: return true` 占位，实现 CFGrd/CFGwr/MRdr/MWr 全分支 + CplD 回发 + BDF→BAR 路由表。

理由: 既有 `dispatch_tlp` 仅处理 CFGrd（占位 register_np）和 CFGwr（真实写），MMIO_READ/WRITE/MEM_READ/WRITE 落入 default no-op。**Completer 严重不完整**，生产链路无法闭合。

范围:
- CFG_READ: 真实读取 Config Space → 生成 CplD 经 `tx_tlp` 回 host（替代 register_np 占位）
- CFG_WRITE: 真实写 Config Space（既有实现保留）
- MMIO_READ: BDF→BAR 路由 → bar_store_ 读 → 生成 CplD 回 host
- MMIO_WRITE: BDF→BAR 路由 → bar_store_ 写（4B 寄存器粒度 + wstrb 字节 mask）
- MEM_READ: MEM 区域判定 → BAR1 VRAM 转发 + CplD
- MEM_WRITE: MEM 区域判定 → BAR1 VRAM 写
- BDF→BAR 路由表：从 config space BAR 寄存器动态构建（监听 BAR 写事件）

实现位置:
- `include/tlm/pcie/pcie_completer_engine.hh` + `src/tlm/pcie/pcie_completer_engine.cc` (新)
- `src/tlm/pcie/pcie_sriov_vf_pool_tlm.cc` (改: `dispatch_tlp` 委托 `PcieCompleterEngine`)
- `src/tlm/pcie/pcie_endpoint_ip.cc` (改: `set_tlp_sink` 接到生产)
- `test/test_pcie_completer_engine_full.cc` (新)

#### Scenario: MRd → CplD 回发

- **WHEN** host 发起 MRd → LL rx_tlp_from_host → CompleterEngine MRd 分支 → bar_store_ 读 → 生成 CplD → tx_tlp → host
- **THEN** host 在 1-2ms 内（per 1 cycle=1ns 虚拟时钟）收到 CplD
- **AND** CplD 的 Requester ID / Tag 回显原始 MRd
- **AND** CplD 的 payload 与 bar_store_ 一致

#### Scenario: BDF 路由命中

- **WHEN** CompleterEngine 收到 MMIO_WRITE (bdf=PF, bar=1, offset=0x10010000)
- **THEN** BDF→BAR 路由表命中 BAR1 doorbell offset
- **AND** 触发 sdma_ring_processed_count_ += wptr (per Stage 1.3a)

#### Scenario: bar_store_ 三维 key

- **WHEN** CompleterEngine 写 bar_store_
- **THEN** key = `(bdf, bar, addr & ~3)` 三维而非全局裸地址
- **AND** 既有测试用 BAR0 单 BAR 场景下 (bdf, 0, key) 命中相同值，零回归

---

### Requirement: requester-engine-outgoing

CppTLM **SHALL** 提供 `PcieRequesterEngine` 让 EP 主动发起 MRd/MWr（TLP 编码 + tx_tlp 调度 + tag 分配 + CompletionTracker 超时）。

理由: 既有 PcieEndpointIP::tick() **只消费 AXI slave 请求**，内部**无任何代码调用 `link_layer()->tx_tlp()`** 发起 MRd/MWr。SDMA 上行 MEM 读、MSI-X 投递链断裂。

范围:
- SDMA MEM_READ：上行读 host 内存（接 cpptlm_dma_translate_cb 翻译 IOVA → PA）
- MSI-X pending → MWr 投递链：读 MSI-X table (msg_addr/msg_data) → 构造 MWr TLP → tx_tlp
- tag 分配 (12-bit, per-VF 独立) + CompletionTracker::register_np 超时（虚拟 ns 计时器）
- 错误注入：CRC 错误 / Malformed / Poisoned (EP=1)（P13 可选）

实现位置:
- `include/tlm/pcie/pcie_requester_engine.hh` + `src/tlm/pcie/pcie_requester_engine.cc` (新)
- `src/tlm/pcie/pcie_link_layer_tlm.cc` (改: `tx_tlp` 接入 RequesterEngine)
- `src/tlm/gpu/sdma_engine_tlm.cc` (改: SDMA 上行 MEM 读出口接 RequesterEngine)
- `test/test_pcie_requester_engine_basic.cc` (新)

#### Scenario: SDMA 上行 MEM 读

- **WHEN** SDMA 处理 H2D DMA 描述符（IOVA=0x1000, len=4096）
- **THEN** RequesterEngine 构造 MRd TLP (bdf, 0, IOVA, len) → tx_tlp → host
- **AND** 经 cpptlm_dma_translate_cb 翻译 IOVA→PA（per ADR-088 §D3.8）
- **AND** 收到 CplD 后写 VRAM + 触发 completion ring（per `sdma_engine_tlm.cc` 既有 1.3a 链）

#### Scenario: MSI-X 投递链

- **WHEN** `dispatch_msix(stream_id, vector)` 被调用（pending 置位）
- **THEN** RequesterEngine 在下次 tick 时读 MSI-X table (msg_addr/msg_data)
- **AND** 构造 MWr TLP (target=msg_addr, data=msg_data) → tx_tlp → host
- **AND** CP interrupt_cb（per `cpptlm_intr_deliver_cb_t` ABI 回调）

#### Scenario: Completion 超时

- **WHEN** RequesterEngine 发起 MRd 后 N μs（虚拟 ns）未收到 CplD
- **THEN** 触发 `cpptlm_error_cb_t` 回调（per ABI 4 callbacks）
- **AND** 释放 tag（避免 outstanding 永久占用）

---

### Requirement: bypass-tlp-data-path

CppTLM **SHALL** 支持"跳过 PCIe EP IP 内部 TLP 逻辑，直接发起 SOC 内部 AXI 读写"通路（**Bypass TLP 链路**），由 `HostBypassTLM::bar_write` 实现。

理由: 用户目标"支持跳过 PCIe EP IP 的逻辑，直接发起 SOC 内部 AXI 读写"。既有 `HostBypassTLM::bar_write` 完整覆盖此场景（Phase 7 + Phase 8 M1 桥接修复 commit `429327d`）。

范围:
- profile JSON `"pcie_path": "axi_bypass"` 启用
- `HostBypassTLM::bar_write` → Axi4Bundle → 桥接 → `PcieAxiAdapter::slave_req` → `PcieEndpointIP::tick()` → bar_store_
- **不**经过 `PcieLinkLayer::rx_tlp_from_host` / `dispatch_tlp`
- **不**新增 Bypass Mux 模式（3 态已存在，仅 Full 启用 PHY+LL+TL，axi_bypass 是 HostBypass 拓扑变体）

实现位置:
- 既有 `src/tlm/pcie/host_bypass_tlm.cc`（Phase 7 已就绪）
- `configs/dgpu_soc_with_pcie_ip.json`（追加 `"pcie_path": "axi_bypass"` profile 示例）
- `test/test_pcie_bypass_tlp_data_path.cc` (新)

#### Scenario: Bypass TLP 路径

- **WHEN** 用户设置 `profile.pcie_path = "axi_bypass"` 并启动仿真
- **THEN** UsrLinuxEmu ABI 路径绕过 22 个函数 + 4 callback typedef 翻译层，直接走 `HostBypassTLM::bar_write`
- **AND** 测试断言 `bar_store_` 真实落盘
- **AND** 测试断言 PcieLinkLayer 状态不变（FC bucket、retry buffer 未变化）

---

### Requirement: pcie-mock-ip

CppTLM **SHALL** 提供独立 `PcieMockIP` 组件，简化 BAR 空间模拟 + MSI-X + D3hot gate + AXI payload 生成（**无 TLP 协议栈、无 LL/PHY/Mux、无 17 端口 SR-IOV**），profile `"pcie_path": "mock"` 启用。

理由: 用户目标"如果配置是跳过 TLP 链，那么就跳过 PCIe EP 的内部逻辑，只需要基本的 bar 空间模拟并支持创建 AXI payload 发到 AXI 接口"。既有 Bypass 路径（`HostBypassTLM::bar_write`）依赖 `PcieEndpointIP::tick()` 内部 BAR 处理，仍有完整 17 端口 SR-IOV 状态机负担。`PcieMockIP` 仿 gem5 架构，提供**目标 ~500 行（上限 800 行）**的简化 PCIe 端点，专用于"无协议栈精度"仿真场景（per V-5 决议 Fz-4：避免行数倒逼设计劣化）。

范围:
- **BAR 空间**：与 `PcieEndpointIP` 等价语义，复用 `PcieBarRouter` 数据驱动机制（JSON `bar0_registers` 填充）。**Host 端 mmio_read/write → 直接写/读本地 `bar_regs_`（绕开 TLP 协议栈）**
- **MSI-X**：简化版 `msix_update_pending(vector)` 直接调 `cpptlm_intr_deliver_cb_t` ABI 回调（无 TLP 投递链）
- **D3hot gate**：**对齐 PcieEndpointIP 的 AXI slave 路径**（`pcie_endpoint_ip.cc:348-355`）—— D3hot 下**所有** BAR 写 DECERR（含 doorbell），**无 doorbell 例外**（per V-5 决议：mock 应反映真实硬件语义，doorbell 在 D3hot 下也应被 gate）
- **AXI payload 生成（device-side only）**：EP 内部设备发起 SOC 输出时（如 SDMA 完成写 host、DPC、doorbell 中断）直接构造 `Axi4Bundle` 并推入内部 `Axi4StreamAdapter`，由 SOC AXI 互联消费。**Host 端 mmio_write 不产生 AXI payload**（仅写本地 bar_regs_）
- **不模拟**：TLP 编码/FC/ACK-NAK/PHY/LL/Config Space 完整语义；仅保留 BAR 空间 + MSI-X + D3hot gate + device-side AXI payload
- **不依赖 `PcieEndpointIP` 或 `PcieSriovVfPool`**：独立组件
- profile JSON `"pcie_path": "mock"` 启用；与 `tlp`/`axi_bypass`/`legacy` 三态正交

实现位置:
- **`include/tlm/pcie/pcie_mock_ip.hh`** + **`src/tlm/pcie/pcie_mock_ip.cc`** (新)
- **`docs/soc_arch/adr/ADR-SOC-17-pcie-mock-ip.md`** (新：架构决策仿 gem5 风格)
- **`test/test_pcie_mock_ip_basic.cc`** (新)
- `configs/dgpu_soc_with_pcie_ip.json` (改: 追加 `"pcie_path": "mock"` profile 示例)

#### Scenario: Mock IP BAR 读写

- **WHEN** profile `"pcie_path": "mock"` 启动，`cpptlm_emulator_mmio_write` 写 BAR0 offset 0x1000
- **THEN** `PcieMockIP::mmio_write` 直接写 `bar_regs_[(0, 0x1000)] = data`（绕过任何 TLP/LL 协议栈）
- **AND** **不**经 `PcieLinkLayer::rx_tlp_from_host` / `PcieAxiAdapter`
- **AND** 测试断言 BAR 读写语义与 `PcieEndpointIP` 等价（同样 key 命中相同值）

#### Scenario: Mock IP MSI-X 中断

- **WHEN** profile `"pcie_path": "mock"`，设备内部触发 MSI-X pending (vector=0)
- **THEN** `PcieMockIP::msix_update_pending(0)` 直接调 `cpptlm_intr_deliver_cb_t` ABI 回调
- **AND** **不**经过 TLP 编码（MWr + tx_tlp + LL 链路）
- **AND** 测试断言 Host driver 立即收到中断回调（虚拟 ns 内）

#### Scenario: Mock IP D3hot gate

- **WHEN** profile `"pcie_path": "mock"`，通过 `cpptlm_emulator_pcie_config_write(emu, 0x44, 1, 0x3)` 写 PMCSR（Power Management Control/Status Register）置 D3hot（**注：cpptlm_emulator_set_power_state 不存在于 22 个函数 ABI 中——头文件实测无此函数，D3hot 通过 cfg write PMCSR 真实路径实现**）
- **THEN** `PcieMockIP::is_mmio_gated()` 返回 true（INV-A 等价，对齐 PcieEndpointIP AXI slave 路径）
- **AND** `mmio_write` **所有** BAR 写（含 doorbell）返 -EIO（**无 doorbell 例外**——per V-5 决议，对齐真实硬件语义）

#### Scenario: Mock IP AXI payload 生成

- **WHEN** profile `"pcie_path": "mock"`，内部设备发起 SOC 侧输出（device-to-host AXI）
- **THEN** `PcieMockIP` 直接构造 `Axi4Bundle` 并推入内部 `Axi4StreamAdapter`
- **AND** **不**调用 `PcieAxiAdapter`，**不**经 17 端口 SR-IOV 状态机
- **AND** 测试断言 `axi_master_req_valid()` 立即 true（无虚拟 ns 时序延迟）

#### Scenario: Mock IP ↔ PcieEndpointIP 行为对齐矩阵

- **WHEN** profile `"pcie_path": "mock"` 启动
- **THEN** 行为对齐矩阵（4 维度）：

| 行为 | PcieEndpointIP AXI 路径 | PcieMockIP | 对齐状态 | 量化阈值 | 度量方法 |
|------|---------------------|------------|---------|----------|----------|
| BAR0 读写 | ✅ `bar_store_[key]` | ✅ `bar_regs_[(bar, offset)]` | ✅ 等价语义 | `data` 字段逐字节相同 (cycle-independent) | 同一 key 命中后 read-back,断言 `data` 全部字节相等 |
| D3hot gate | ✅ 全 BAR DECERR | ✅ 全 BAR -EIO | ✅ 对齐 | 返回 `int: -EIO` (即 -5) | `cpptlm_emulator_mmio_write` 返回值断言 `== -5` |
| D3hot doorbell | ❌ 被 gate | ❌ 被 gate | ✅ 对齐（V-5 决议） | 返回 `int: -EIO` (即 -5) | D3hot 下 doorbell 写返回码断言 |
| MSI-X | `dispatch_msix` → `tx_tlp` → 投递 | `msix_update_pending` → `cpptlm_intr_deliver_cb_t` | 🟡 路径不同 | host 回调到达 ≤1000 ticks (Endpoint) / ≤1 tick (Mock) | `cpptlm_emulator_tick()` 计数器 ≤ 阈值后断言回调已被调 |

- **AND** Mock IP 与 PcieEndpointIP 在 BAR 读写 key 命中相同值（既保证测试可对齐验证）

#### Scenario: Mock IP 行数约束

- **WHEN** 实现 `PcieMockIP` 后
- **THEN** 代码行数目标 **~500 行**，**上限 800 行**（per V-5 决议 Fz-4 放宽，避免行数倒逼设计劣化）

---

### Requirement: profile-pcie-path-routing

CppTLM **SHALL** 通过 profile JSON `"pcie_path"` 字段实现 **4 态选路**，配合 **ABI 精简（22 → 18 函数；26 → 22 符号）**共同维护跨仓契约面（per ADR-088 §D5 Status Update 与 Hub 侧协调同意）。

理由: 22 函数 + 4 callback typedef 是跨仓契约（UsrLinuxEmu ↔ CppTLM，按 `cpptlm_emulator.h` 行 64-115 实测），精简后 18 函数 + 4 callback typedef 配合 profile JSON 选路（4 态：tlp / axi_bypass / mock / legacy）保持冻结边界。`backdoor_*` 4 个冗余函数移出 C 表面，backdoor 行为通过 `mock` 路径向驱动隐藏。

范围:
- `cpptlm_emulator_create(profile_path)` 加载 profile JSON
- `pcie_path`: `"legacy"`（默认）/ `"axi_bypass"` / `"tlp"`（opt-in 启用 TLP 协议栈链路）/ **`"mock"`**（opt-in 启用独立 PcieMockIP，绕过 PCIe EP 全部协议栈）
- `mmio_write` 入口按 profile 分流
- `pcie_config_read/write` / `mmio_read` 同源
- **同步 ABI 读路径 tick 泵环**（tlp profile 下）：`DGpuBoard::mmio_read` 是同步 C ABI，调用线程在 `cpptlm_emulator_mmio_read` 内循环驱动 `eq_->run(...)` 最多 N 个虚拟周期（默认 N=1000，per design.md §2.5 quantum 边界），等待 host 侧读路径（Encoder 出 + RequesterEngine MRd + 收到 CplD 后）写回 `pending_data_`；到达或超时后回填 buf 返回。超时按既有 `mmio_regs_` 回退路径降级（**注：超时降级路径不再走 backdoor ABI**——backdoor 已移出 C 表面，是内部 C++ `DGpuBoard::backdoor_*` API），保证 ABI 同步语义不被破坏
- **ABI 精简（22 → 18 函数；26 → 22 符号）**：
  - 删除：`cpptlm_emulator_backdoor_read` / `_backdoor_write` / `_register_backdoor_cb` / `_lookup_register`
  - 保留（按 `cpptlm_emulator.h` 行 64-115 实测）：18 个驱动核心函数 — `get_version` / `get_device_count` / `get_device_info` / `create` / `create_by_id` / `destroy` / `mmio_write` / `mmio_read` / `pcie_config_write` / `pcie_config_read` / `msix_init` / `msix_update_pending` / `msix_clear_pending` / `register_callbacks` / `register_dma_translate_cb` / `open` / `close` / `get_adapter_info`（**注：删除清单中的 `set/get_power_state` 和 `set/get_user_context` 不存在于 22 个函数头文件中**；电源状态走 cfg write PMCSR，用户上下文通过 SDK 直接持有 emulator 指针）
  - backdoor 行为通过 `profile.pcie_path = "mock"` 切换（驱动不可见）
  - 需要 ADR-088 Status Update 与 Hub 侧协调同意

实现位置:
- `include/tlm/gpu/dgpu_board_shell.hh` (改: 暴露 profile 字段 + 路径分发器)
- `src/tlm/gpu/dgpu_board_shell.cc` (改: `mmio_write` 按 profile 分流 + tlp profile 下 `mmio_read` 循环驱动 eq_)
- `src/abi/cpptlm_emulator.cc` (改: 透传 profile 路径)
- `configs/dgpu_soc_with_pcie_ip.json` (改: 追加 `"pcie_path"` 字段)

#### Scenario: tlp profile 启用

- **WHEN** 配置 `"pcie_path": "tlp"`
- **THEN** `DGpuBoard::mmio_write` 构造 PcieTlpWireBundle → PcieTlpEncoder 编码 → PcieLinkLayer::rx_tlp_from_host → CompleterEngine 处理

#### Scenario: axi_bypass profile 启用

- **WHEN** 配置 `"pcie_path": "axi_bypass"`
- **THEN** `DGpuBoard::mmio_write` 直转 HostBypassTLM::bar_write (跳过 TLP 协议栈)

#### Scenario: mock profile 启用（独立 PcieMockIP）

- **WHEN** 配置 `"pcie_path": "mock"`
- **THEN** `DGpuBoard::mmio_write` 路由到 `PcieMockIP::mmio_write`（**不走** PcieEndpointIP / LL/PHY/Mux / 17 端口 SR-IOV / TLP 协议栈）
- **AND** MSI-X 中断直接调 `cpptlm_intr_deliver_cb_t` ABI 回调（无 TLP 投递链）
- **AND** D3hot gate 行为与 PcieEndpointIP AXI slave 路径严格对齐（**全 BAR 写 DECERR，无 doorbell 例外**）
- **AND** 测试断言 `bar_store_` 真实落盘（与 PcieEndpointIP 等价 key 命中）

#### Scenario: legacy profile 兼容

- **WHEN** 配置 `"pcie_path": "legacy"` 或缺失
- **THEN** 既有行为保留：仿真器本地 `mmio_regs_` 存储
- **AND** 既有 `[dgpu][shell][mmio]` 测试零回归

#### Scenario: tlp profile 同步读泵环

- **WHEN** `cpptlm_emulator_mmio_read(emu, 0, 0x1000, buf, 8)` 在 `pcie_path=tlp` 下被调用
- **THEN** `DGpuBoard::mmio_read` 内部循环驱动 `eq_->run(...)`（上限 1000 虚拟周期）
- **AND** CplD 到达后由 host 侧读路径（Encoder 出 + RequesterEngine MRd + 收到 CplD）写 `pending_data_[trans_id]` → `mmio_read` 拷贝到 buf 返回
- **AND** 超时降级到 `mmio_regs_` 回退路径（per 修复 #5 既有语义；**注：超时不再走 backdoor ABI——backdoor 已移出 C 表面**）
- **AND** ABI 同步语义保持（调用方无需感知泵环）

#### Scenario: 读泵环线程安全与超时一致性

- **WHEN** `pcie_path="tlp"` profile 下,ABI 调用线程与 sim 线程并发驱动 `eq_->run(...)`
- **THEN** `pending_data_` 由 `std::mutex` 保护(abi_call_mutex_); CplD 到达与 ABI `mmio_read` 读取均需 lock/unlock
- **AND** 超时降级(>1000 虚拟周期)后:
  - `pending_data_` 同步擦除(避免下次 mmio_read 返回陈旧数据)
  - spin 循环**永久** fallback 到 `mmio_regs_` 路径(per 修复 #5 既有语义)
  - 不再 retry 泵环(防止 ABI 调用线程阻塞)
- **AND** `DGpuBoard::mmio_read` 调用方无需感知泵环, ABI 同步语义保持

#### Scenario: ABI 精简（22 → 18 删除 4）

- **WHEN** 检查 `include/abi/cpptlm_emulator.h`
- **THEN** **删除 4 个冗余函数**：`cpptlm_emulator_backdoor_read` / `_backdoor_write` / `_register_backdoor_cb` / `_lookup_register`
- **AND** 保留 18 个驱动核心函数（按 `cpptlm_emulator.h` 行 64-115 实测）：`get_version` / `get_device_count` / `get_device_info` / `create` / `create_by_id` / `destroy` / `mmio_write` / `mmio_read` / `pcie_config_write` / `pcie_config_read` / `msix_init` / `msix_update_pending` / `msix_clear_pending` / `register_callbacks` / `register_dma_translate_cb` / `open` / `close` / `get_adapter_info` 签名零修改
- **AND** `cpptlm_emulator_t` 结构体零修改
- **AND** 4 个 callback typedef 零修改
- **AND** ADR-088 Status Update 与 Hub 侧（UsrLinuxEmu）协调同意

---

### Requirement: pcie-endpoint-tick-tlp-dispatch

`PcieEndpointIP::tick()` **SHALL** 接入 `dispatch_tlp_entry` 三态分派（参考既有 `pcie_endpoint_tlm.cc:61-73` 逻辑），让 Bypass 模式短路 LL FC 检查，Full/Partial 走 `rx_tlp_from_host`。

理由: 既有 PcieEndpointIP::tick()（Phase 8 M1）只从 `PcieAxiAdapter::slave_req` 消费 AXI，**未经过 `dispatch_tlp_entry` 三态分派**。`set_tlp_sink` 在生产代码中零调用。

范围:
- **入方向触发**：当 host 侧 TLP 入口到达（profile=`tlp` 路径的 `PcieTlpEncoder` 输出 / adapter ingress），按 Bypass Mux mode 三态分派：
  - **Full/Partial**：经 `PcieLinkLayer::rx_tlp_from_host`（FC check + ACK 生成）→ 通过后 `tlp_sink_(tlp)` 调 `PcieCompleterEngine`
  - **Bypass**：跳过 LL FC 检查，直接 `tlp_sink_(tlp)` 调 `PcieCompleterEngine`
- 接线时机：在 `attach_composition` 内调 `PcieLinkLayer::set_tlp_sink(...)` 注册 `PcieCompleterEngine` 入口回调（参考既有 `pcie_endpoint_tlm.cc:70` 的 `dispatch_tlp_entry` 同步路径）
- 既有 AXI slave 消费分支保留（向后兼容）

实现位置:
- `src/tlm/pcie/pcie_endpoint_ip.cc` (改: `tick()` 增加 `dispatch_tlp_entry` 逻辑)
- `include/tlm/pcie/pcie_endpoint_ip.hh` (改: 持有 `PcieCompleterEngine` 引用)

#### Scenario: Full 模式过 LL

- **WHEN** Bypass Mux mode = Full
- **THEN** TLP 经 `PcieLinkLayer::rx_tlp_from_host` (FC check + ACK 生成)
- **AND** 通过后到达 `set_tlp_sink` → `PcieCompleterEngine`

#### Scenario: Bypass 模式短路 LL

- **WHEN** Bypass Mux mode = Bypass
- **THEN** TLP 跳过 LL FC 检查，直接送达 `set_tlp_sink`
- **AND** 仅保留事务层（dispatch + bar_store_ 写）

#### Scenario: Partial 模式保留 LL FC

- **WHEN** Bypass Mux mode = Partial
- **THEN** 同 Full：过 LL FC 检查 + ACK 生成
- **AND** 跳过 PHY（与 Full 区别）

---

### Requirement: end-to-end-tlp-path

测试 `test_pcie_endpoint_ip_tlp_path_e2e.cc` **SHALL** 验证 **UsrLinuxEmu ABI 路径**真实产生 TLP（不是测试手写），经过 LL，到达 Completer，写 `bar_store_`，CplD 回发，最终 ABI `mmio_read` 返回真实数据。

理由: 既有 Phase 8 E2E (`test_pcie_endpoint_ip_full_e2e.cc`) 仅验证 AXI 数据路径闭合，**不验证完整 TLP 链路**（无 TLP 编码、无 LL FC、无 Completer CplD 回发）。

范围:
- 测试经 `cpptlm_emulator_mmio_write` (C ABI) 入口
- 验证 ABI 路径真实产生 TLP（`PcieTlpWireBundle` 长度 > 0）
- 验证 LL 真实消费（FC bucket credit 消耗）
- 验证 Completer 真实处理（bar_store_ 落盘）
- 验证 CplD 真实生成（ResponseKind=CplD, tag 回显）
- 验证 ABI `cpptlm_emulator_mmio_read` 返回真实数据（非 0 占位）

实现位置:
- `test/test_pcie_endpoint_ip_tlp_path_e2e.cc` (新)

#### Scenario: TLP 链路 e2e 闭环（MWr 写路径）

- **WHEN** `cpptlm_emulator_mmio_write(emu, 0, 0x1000, data, 8)` 被调用 (BAR0 offset 0x1000 写 8 字节)
- **THEN** profile `"pcie_path": "tlp"` 下，链路真实产生：
  - `PcieTlpWireBundle`（MWr TLP，length=2 DW）进入 LL
  - LL FC bucket 消耗 P credit（Posted 流量）
  - LL 生成 ACK DLLP（虚拟 ns 内）
  - Completer 解析 MWr → BDF→BAR 路由 → `bar_store_[(bdf, bar, key)] = data`
  - **MWr 是 Posted 事务，PCIe 规范无 Completion**（per PCIe Base Spec §2.4）；数据落盘即视为完成
- **AND** **后续** `cpptlm_emulator_mmio_read(emu, 0, 0x1000, buf, 8)` 触发 MRd 路径（见下条 Scenario），验证 bar_store_ 数据
- **AND** 测试断言 LL P credit 通过 UpdateFC 恢复

#### Scenario: 真实 TLP 产生断言钩子点

G3 验收要求 "**UsrLinuxEmu ABI 路径真实产生 TLP**",通过以下 3 个钩子点量化验证:

**钩子 1 — `PcieLinkLayer` 入口计数**: 在 `PcieLinkLayer::rx_tlp_from_host` 入口处增加原子计数器 `tlp_rx_count_` (类型 `std::atomic<uint64_t>`)。每次入口自增 1。
- 测试断言: ABI `mmio_write` 之后后, `tlp_rx_count_` ≥ 1

**钩子 2 — `set_tlp_sink` lambda 捕获**: 测试通过 `set_tlp_sink` 注册 lambda,在 lambda 内捕获 `PcieTlpBundle` 参数:
```cpp
PcieTlpBundle captured_tlp;
link_layer->set_tlp_sink([&](const PcieTlpBundle& tlp) {
    captured_tlp = tlp;
});
mmio_write(0, 0x1000, data, 8);
ASSERT(captured_tlp.kind == MMIO_WRITE);
assert(captured_tlp.bar == 0);
assert(captured_tlp.offset == 0x1000);
```

**钩子 3 — `PcieTlpEncoder` 出口原子计数器**: 在 `PcieTlpEncoder::encode_mwr(...)` 返回前增加 `wire_tlp_generated_count_` 原子自增。
- 测试断言: `wire_tlp_generated_count_` 在 ABI 调用前后差 ≥ 1

**Hook 失败语义**: 任一钩子点断言失败 → 测试 fail 并打印 "真实 TLP 未从 ABI 路径产生" 错误信息, 与 mock 测试显式区分。

#### Scenario: TLP 链路 e2e 闭环（MRd→CplD 读路径）

- **WHEN** `cpptlm_emulator_mmio_read(emu, 0, 0x1000, buf, 8)` 在前述 MWr 之后被调用 (BAR0 offset 0x1000 读 8 字节)
- **THEN** profile `"pcie_path": "tlp"` 下，链路真实产生：
  - `PcieTlpWireBundle`（MRd TLP，length=2 DW，对应 8 字节读请求）进入 LL
  - LL FC bucket 消耗 NP credit（Non-Posted 流量）
  - Completer 解析 MRd → 读 `bar_store_[(bdf, bar, key)]` → 生成 `CplD TLP`（Fmt=`010` Type=`01010`）→ 写入 `tx_tlp_out_` → host
  - LL 收到 CplD 后回填 `pending_data_` → ABI `mmio_read` 同步返回 buf
- **AND** 返回数据与 MWr 写入的 data 字节一致
- **AND** 测试断言 LL NP credit 通过 UpdateFC 恢复

---

