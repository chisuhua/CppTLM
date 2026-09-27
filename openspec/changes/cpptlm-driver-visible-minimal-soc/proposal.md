# cpptlm-driver-visible-minimal-soc: 让驱动看到完整最小设备 (v1.3 — 6 P0 阻塞修正)

> **状态**: 🔄 v1.3 提案 — 2026-09-26 (P0 修正)
> **v1.0 提案**: 2026-09-26（基于 5 澄清 + Oracle R1-R7 + Metis M1-M5）
> **v1.1 P0 修订**: 2026-09-26（应用 Oracle 落地包）
> **v1.2 P1 修订**: 2026-09-26（应用 Oracle 二次审查 8 个 must-fix，N1-N12 全部落地）
> **v1.3 修订**: 2026-09-26（应用 Metis/Oracle/Librarian 三方交叉审查发现的 6 P0 阻塞错误 B1-B6）
> **作废参考**: `2026-09-20-cpptlm-pcie-memory-device-mvp` (D2 v1.1，已 archive)
> **优先级**: 🟡 P1
> **工期**: 9.5-11 工作日（v1.3 修正 0.5-1d + 原 9d + buffer 1d）
> **目标**: 在 CppTLM dGPU SoC 中实现 **完整的最小设备**（minimal SoC），让 UsrLinuxEmu 端驱动通过标准 PCIe BAR + 内部 chip-internal AXI 总线看到真实、可驱动验证的最小 dGPU SoC。
> **v1.3 阻塞修正清单**: B1 字段名 (`host_iova`/`size`/`vram_offset`) + B2 SDMA 切型范围限定 (minimal_v1 仅 chip-internal) + B3 撤 N4 `adapters_[2]` (存单 adapter) + B4 改 lazy+mutex (非预分配) + B5 64-bit 双 dword BAR + B6 双注册 (object + adapter)

## Why

### 上下文：D2 v1.1 已交付，但未达 driver 视角完整

D2（`PcieMemoryDevice` MVP）已通过 Oracle 评审并实施（commit `14814627`），但存在**架构局限**（详见三次审查报告）。

### 用户决策（5 个澄清）+ 二次审查 must-fix

| # | 用户澄清 / Oracle must-fix | D-AXI 落点 |
|---|------|-----------|
| U1 | coherent/non-coherent 是 UsrLinuxEmu 端模拟 | spec 新增 "Coherence 域边界" Requirement |
| U2 | PcieTlpBundle 是 board-level 通信 | 新建 `AxiMemBundle` (chip-internal)；5 阶段 PCIe EP 共享 wire-format 零 diff |
| U3 | PTE 通过 BAR 空间写入 | minimal_v1 启用 BAR2 + `memory_routing_enabled=true` |
| U4 | device_id 来自 EP pcie 配置空间 | spec DoD 修正为 `PcieConfigSpace` (0x10DE/0x1234 已源码验证) |
| U5 | Oracle 给修订方案 | Oracle R1-R7 + Metis M1-M5 全部应用 |
| **N1** | **AxiMemBundle 4KB 撞 PacketPool 256B 上限** | design §3 显式声明需框架 payload 扩容（2 行）+ T0.2 改真实 StreamAdapter round-trip |
| **N2** | **GMMU COMPLETE 无 iova 匹配** | design §5 加 `pending_iova_` 匹配检查；spec Scenario 补断言 |
| **N3** | **SDMA retry driver 未设计** | design §6 补 inflight retry 伪码 + in-order 论证 |
| **N4** | **PcieMemoryDevice adapters[1] 丢** ~~（**v1.3 B3 撤销**：基于错误前提，事实 module_factory 对 multi-port 注入单个 MultiPortStreamAdapter）~~ | design §4 存**单 `adapter_`**（非 `adapters_[2]`）+ tick 一次 |
| **N5** | **"零测试改动"为伪命题** | tasks 显式列"既有 [sdma]/[pcie-memory] 测试机械迁移"任务 + +0.5–1d |
| **N6** | **EP BAR 寄存器生成不存在** | tasks T0.4 立项 EP BAR 实现（+0.5–1d）；删除"可完整枚举"过强断言 |
| **N7** | **SDMA resp slot-2 错配** | design §6/spec 改为 "resp 从 `req_in[PORT_MEM_OUT]` (slot 2) 消费" |
| **N8** | **EP 双 tick 隐患** | tasks T1.4 显式删除 EP `memory_device_->tick()` 转发 |

### 预期收益

1. **驱动视角完整** — driver 通过 15 ABI 看到 config + BAR0/1/2 + chip-internal AXI
2. **架构清洁** — chip-internal AXI 隔离，PcieTlpBundle 不变，DGpuBoard backdoor 特权持有者
3. **可扩展性** — chip-internal AxiMemBundle 承载未来设备
4. **D1/D2 复用** — flag 模式 + dual-mode legacy
5. **测试覆盖完整** — driver-visible E2E + AXI + backdoor + dual-mode 全绿

## What Changes

### §1 范围：Driver-Visible Minimal SoC

**功能边界**（最小可行 + 完整可驱动）:

| 模块 | 角色 | 接入方式 |
|------|------|----------|
| **PcieEndpointIP** | PCIe 协议端点 | 4 端口冻结；持 `PcieMemoryDevice*` raw pointer；config_space 返回 0x10DE/0x1234；**T1.4 删除 `memory_device_->tick()` 转发**（防双 tick） |
| **PcieMemoryDevice** | **soc 顶层** 8GB memory backing + 2 SlavePorts | ChStreamModuleBase；wire-format AxiMemBundle (chip-internal only, v1.3 B2 限定)；**T1 持单 `adapter_`**（v1.3 B3 撤 N4 双 adapter）+ **lazy alloc + mutex**（v1.3 B4 改预分配）+ **双注册**（v1.3 B6 registerObject + registerMultiPortAdapter） |
| **PcieDisplayDevice** | 4KB MMIO + 32MB FB | 不变 |
| **SdmaEngineTLM** | DMA 引擎 | 5 端口切 AxiMemBundle；**resp 从 slot-2 (PORT_MEM_OUT) 消费**；**T3 加 inflight retry driver** |
| **GmmuTLM** | 一级页表翻译 | ChStreamModuleBase + 1 MasterPort + **COMPLETE/WAIT iova 匹配**；dual-mode legacy 同步路径保留 |
| **MemoryTLM** | 通用内存 backing | 不变 |
| **CompletionRingTLM** | 完成环 | 不变 |
| **DGpuBoard** | 顶层 board shell + host-side backdoor 特权 | framebuffer_storage_ 持有；新增 BAR2 mmio fast-path；**T0.4 条件注入 legacy 路径**（无 pcie_memory 时保留 framedoor 注入） |
| **StreamAdapter 框架** | T0.2 框架扩容 | **payload 按 `sizeof(BundleT)` 扩容 2 行**（N1 关键修复） |
| **PcieConfigSpace** | T0.4 BAR 寄存器生成 | **T0.4 显式立项**（当前 `bar_sizes` 参数识别但 BAR 寄存器无填充代码） |

### §2 拓扑改动

**前**（D2 v1.1）: 同前次

**后**（v1.2 P1 修订）:
```
DGpuBoard
  └─ soc_ (DGpuSoc)
       ├─ pcie_ep       (PcieEndpointIP, 4 端口冻结)
       │    ├─ memory_device_ (raw ptr, non-owning)
       │    └─ **T1.4: 不再 tick memory_device_** (避免双 tick)
       ├─ pcie_memory   (PcieMemoryDevice, ChStreamModuleBase, 2 SlavePorts)
       │    ├─ **单 adapter_** + **tick 一次** (v1.3 B3 撤 N4 双 adapter)
       │    ├─ port0 ← sdma.2 (mem_out)  [AxiMemBundle]
       │    ├─ port1 ← gmmu.0 (master)   [AxiMemBundle]
       │    └─ memory_backing_ 8GB (BAR2 暴露)
       ├─ sdma          (SdmaEngineTLM, 5 ports, 全部 AxiMemBundle)
       │    ├─ mem_out → pcie_memory.0
       │    ├─ **resp 从 req_in[2] (slot 2, PORT_MEM_OUT) 消费** (N7)
       │    ├─ **tick 先 FIFO 重试 inflight_ 再收新 desc** (N3)
       │    └─ set_vram_backdoor 保留 dual-mode
       ├─ gmmu          (GmmuTLM, ChStreamModuleBase, 1 MasterPort)
       │    ├─ req_out → pcie_memory.1
       │    ├─ **COMPLETE/WAIT 加 pending_iova_ 匹配** (N2)
       │    └─ set_backing 保留 dual-mode
       ├─ memory        (MemoryTLM)
       └─ completion    (CompletionRingTLM)

  chip-internal AXI Connections (AxiMemBundle):
    sdma.2 (mem_out)  → pcie_memory.req_in[0] (PORT_SDMA)
    gmmu.0 (master)   → pcie_memory.req_in[1] (PORT_GMMU)
    sdma.2 (resp)     ← pcie_memory.resp_out[0]
    gmmu.0 (resp)     ← pcie_memory.resp_out[1]

  board-level (PcieTlpBundle, 不变):
    HostBypass/RC ↔ pcie_ep
```

### §3 DGpuBoard 路由层（沿用 D1/D2 v1.1 + N6/N8/N12 修订）

**backdoor_read/write 4 层分流** (不变):
1. `display_routing_enabled_ && has_display_device()` → PcieDisplayDevice
2. `memory_routing_enabled_ && has_memory_device()` → PcieMemoryDevice
3. `framebuffer_ptr_ != nullptr` → 直读 framebuffer (host-side 特权)
4. `vram_segments_` map → fallback

**新增 mmio_read/write BAR2 fast-path**:
```cpp
if (memory_routing_enabled_ && bar == 2 && soc_) {
    if (auto* ep = pcie_ep(); ep && ep->has_memory_device()) {
        return ep->memory_device().memory_read(offset, buf, len);  // write 对称
    }
}
```

**T0.4 条件注入**（N12）: `bind_memory_backings()` 改为"若 soc 无 pcie_memory 则保留 legacy framebuffer 注入，否则走 pcie_memory 注入路径"。避免 `dgpu_board_v1.json` 等无 pcie_memory 配置的 [sdma][h2d]/SoC E2E 回归断。

### §4 文件清单（v1.2 P1 修订）

| 文件 | 变化 | 来源 |
|------|------|------|
| `include/bundles/axi_mem_bundles_tlm.hh` | **新** | — |
| `include/framework/stream_adapter.hh` (或相关) | **改** | **N1**: payload 扩容 2 行 |
| `include/tlm/gpu/pcie_memory_device.hh` + `src/.../pcie_memory_device.cc` | **改** | **v1.3 B3 (撤 N4 单 adapter)** + **v1.3 B4 (lazy+mutex)** + **v1.3 B6 (双注册)** + ch_uint API 修正 |
| `include/tlm/pcie/pcie_endpoint_ip.hh` + `src/.../pcie_endpoint_ip.cc` | **改** | N8 (删 memory_device_->tick()) + raw ptr |
| `include/tlm/pcie/pcie_config_space*.hh` + `*.cc` | **改** | **N6**: BAR 寄存器从 bar_sizes 生成 |
| `include/tlm/gpu/gmmu_tlm.hh` + `src/.../gmmu_tlm.cc` | **改** | N2 (iova 匹配) + 访问器 + 异步状态机 |
| `include/tlm/gpu/sdma_engine_tlm.hh` + `src/.../sdma_engine_tlm.cc` | **改** | N3 (retry driver) + N7 (slot-2) + bundle 切型 |
| `include/chstream_register.hh` | **改** | SDMA 注册换 AxiMemBundle；PcieMemoryDevice (2-port) + GmmuTLM (1-port) |
| `include/modules_cluster.hh` | **改** | 删 GmmuTLM 双 REGISTER_MODULE |
| `include/tlm/gpu/dgpu_board_shell.hh` + `src/.../dgpu_board_shell.cc` | **改** | BAR2 fast-path + bind_memory_backings 条件化 (N12) |
| `configs/dgpu_soc_minimal_v1.json` | **改** | pcie_memory + BAR2 + AXI connections |
| `test/test_axi_mem_bundle_stream_adapter_roundtrip.cc` | **新** (N1) | 经真实 StreamAdapter 收发 round-trip |
| `test/test_pcie_endpoint_ip_three_bar.cc` | **新** (N6) | EP BAR 寄存器从 bar_sizes 生成 |
| `test/test_pcie_endpoint_ip_tick_nesting.cc` | **新** (N8) | EP 不再双 tick |
| `test/test_gmmu_iova_match.cc` | **新** (N2) | GMMU COMPLETE/WAIT iova 匹配 |
| `test/test_sdma_retry_driver.cc` | **新** (N3) | SDMA inflight retry driver |
| `test/test_sdma_slot2_response.cc` | **新** (N7) | SDMA resp 从 slot-2 消费 |
| `test/test_dgpu_board_legacy_dual_mode.cc` | **新** (N12) | 无 pcie_memory 配置保留 legacy 注入 |
| `test/test_pcie_memory_device_basic.cc` (既有) | **改** (N5) | EP 默认 nullptr 适配 + 2-port 适配 |
| `test/test_pcie_memory_device_backing.cc` (既有) | **改** (N5) | 同上 |
| `test/test_pcie_memory_device_routing_*.cc` (既有) | **改** (N5) | 同上 |
| `test/test_sdma_*.cc` (既有 6+ 个) | **改** (N5) | bundle 类型从 PcieTlpBundle → AxiMemBundle |
| `test/test_pcie_memory_device_axi_*.cc` | **新** | chip-internal AXI 路径 |
| `test/test_minimal_soc_driver_visible_e2e.cc` | **新** | driver-visible 端到端 |
| `test/test_pcie_memory_device_topology_lifecycle.cc` | **改** | "互不解引用" 而非"销毁顺序保证" (N11) |
| `docs/pcie/driver-visible-minimal-soc.md` + `AGENTS.md` | **新/改** | 同步 |

**冻结面零触碰**: `include/abi/cpptlm_emulator.h` / `include/tlm/gpu/pcie_endpoint_tlm.h` / `include/tlm/gpu/pcie_display_device.hh` / `include/bundles/pcie_bundles_tlm.hh`

### §5 ABI 冻结: 0 新 ABI 函数

### §6 Coherence 域边界: 由 UsrLinuxEmu 端分配器模拟，CppTLM 不建模

### §7 不在范围: GPU 计算/D3、GMMU 多级页表、混合端口模板、`ch_uint<512>` 限制统一化、ArchForge 跨仓

### §8 验收标准（DoD）— v1.2 P1 修订

| 项 | 修订后标准 |
|----|-----------|
| **N1 验证** | AxiMemBundle 经真实 StreamAdapter 收发 round-trip 通过（不裸 serialize） |
| **N6 验证** | PcieEndpointIP 接受 3 元素 bar_sizes，config space BAR 寄存器正确生成 3 BAR 条目 |
| **N8 验证** | EP 不再 `memory_device_->tick()`，pcie_memory 单 tick 路径 |
| **N2 验证** | GMMU 乱序重试返 -EAGAIN（同 iova 仍 -EAGAIN；不同 iova 返 -EAGAIN 不消费） |
| **N3 验证** | SDMA inflight retry driver 多 tick 完成 in-order |
| **N4 验证** | PcieMemoryDevice 2 adapter 都 tick，resp 双向路由成功 |
| **N5 验证** | 既有 [sdma]/[pcie-memory] 测试迁移后全绿 |
| **N7 验证** | SDMA D2H 经 slot-2 输入消费 resp |
| **N10 验证** | BAR2 写预分配 memory_backing_ (8GB)，无 lazy resize race |
| **N12 验证** | 无 pcie_memory 配置（dgpu_board_v1.json）保留 legacy framebuffer 注入，回归绿 |
| **驱动视角** | `pcie_config_read` 返 0x123410DE + BAR2 mmio round-trip + BAR1 backdoor round-trip + driver 经 BAR2 写 PTE + GMMU AXI 读 PTE + SDMA H2D/D2H 经 AXI 完成 |
| **ABI 冻结** | `git diff HEAD -- include/abi/cpptlm_emulator.h` 空；15 fn + 4 typedef 不变 |
| **冻结面** | pcie_endpoint_tlm.h / pcie_display_device.hh / pcie_bundles_tlm.hh 零 diff |
| **回归基线** | 既有 [pcie-memory]/[sdma]/[gmmu] 套件迁移后全绿 |
| **新增 E2E** | [driver_visible] ≥1 + [axi][gmmu]/[axi][sdma] 各 ≥1 + [must-fix] 各项 ≥1 |

## Impact

### Who is affected

- **UsrLinuxEmu 端**: 驱动开发者获得完整最小设备视图（coherence 由 UE 端分配器类型模拟）
- **CppTLM 端**: soc 拓扑扩展 + AxiMemBundle 新增 + 框架 2 行扩容 + EP BAR 实现 + 测试机械迁移
- **现有测试**: 6+ [sdma] 文件 + 4 个 [pcie-memory] 文件需机械迁移

### Dependencies (不变)

PcieEndpointIP / PcieDisplayDevice / PcieMemoryDevice / DGpuBoard / MemoryTLM / SdmaEngineTLM / GmmuTLM / CompletionRingTLM (全部已交付)

### Risks（v1.2）

| 风险 | 等级 | 状态 |
|------|------|------|
| R1-R7 | 全部 | ✅ 二次审查消解 |
| M1-M5 | 全部 | ✅ 二次审查消解 |
| N1 | Critical | ✅ must-fix #1 |
| N2-N8 | High/High-Medium | ✅ must-fix #2-8 |
| N9 D2H/H2D 数据面 | Medium | 🟡 部分（仅 VRAM 侧 AXI 化；host 侧依赖 host_backdoor 注入，spec 显式声明） |
| N10 BAR2 线程安全 | Medium | 🟡 spec 要求 preallocate memory_backing_ |
| N11 tick/析构顺序 | Low-Medium | 🟡 spec 改为 "互不解引用" 而非 "销毁顺序" |
| N12 条件注入 | Medium | ✅ must-fix #5 (tasks 显式立) |

## Tasks 概要 (修订)

- **P1 修订**: 8 must-fix 写入 design/spec/tasks
- **T0**: characterization + AxiMemBundle StreamAdapter round-trip + EP 3-BAR + 既有 baseline
- **T1**: PcieMemoryDevice 2-port + **单 adapter_**（v1.3 B3 撤 N4）+ EP raw ptr + 删 tick 转发 + EP BAR 实现（v1.3 B5 64-bit 双 dword）+ 既有 [pcie-memory] 测试迁移
- **T2**: GMMU 异步 + iova 匹配 + 访问器 + 注册迁移
- **T3**: SDMA 5 端口切型 + retry driver + slot-2 resp + D2H 异步 + 既有 [sdma] 测试迁移
- **T4**: JSON + BAR2 fast-path + bind_memory_backings 条件化 + E2E + 文档

## 关联

- **作废参考**: `2026-09-20-cpptlm-pcie-memory-device-mvp` (D2 v1.1, archive)
- **上游参考**: `2026-09-20-cpptlm-pcie-display-io-mvp` (D1 v1.1.1)

---

**作者**: CppTLM Team (Sisyphus)
**触发对话**: 2026-09-26 brainstorming 5 轮澄清 + 3 次 Oracle 审查
**v1.0**: 5 澄清落点
**v1.1 P0**: Oracle R1-R7 + Metis M1-M5 修订
**v1.2 P1**: Oracle 二次审查 8 must-fix 全部应用 (N1-N12 全部落地)
**Oracle sessions**:
- `ses_f21e9e147ffeeuTmILtLvnBvTb` (P0 修订落地包)
- `ses_f21924eb3ffeb8SMW0SPMGLVf1` (P1 二次审查 12 漏洞)