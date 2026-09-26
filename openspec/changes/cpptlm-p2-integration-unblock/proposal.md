# cpptlm-p2-integration-unblock: P2 (CP 接入) 集成阻塞修复 (Metis + Oracle 2027-09-17 修订版)

> **状态**: 📋 Proposed — 2027-09-17 (修订)
> **目标**: Unblock Phase 9 P2 (CP 跨仓接入) 启动的 4 个集成断点
> **关联 P2 计划** (ArchForge): `docs/roadmap/phase9-p2-cp-attach-via-axi.md`
> **关联 EP 文档** (ArchForge): `docs/architecture/19-pcie-ip-microarchitecture.md` §3, §7-9
> **路线图位置**: Phase 9+ P0.5（介于 P0 收尾 + P1 SM Gate 验证 + P2 启动之间）
> **关键修订 (Metis + Oracle 2027-09-17 fact-check)**:
> - **D2 重写**: Axi4 16-bit ID 与 CacheReq 64-bit `transaction_id` **1:1 直通**, 去旧 8-bit 压缩 (基于不存在前提)
> - **D3 修正**: `set_host_bypass()` API 不存在, 用既有 `HostBypassTLM::attach_to_endpoint()`
> - **D8 新增**: framebuffer 单一真源契约 (P0.5-landing 集成)
> - **文档仓修正**: `docs/soc_arch/**` 已迁 ArchForge 仓, 本仓禁直接修改
> - **测试基线实测**: `[pcie]` 36,598 / 415 cases, `[chstream]` 184 / 46 cases

---

## Why

Phase 9 P2（CP 跨仓接入）在 P2-1 写完 CP 寄存器映射 spec 后，需要在 CppTLM 侧打通 4 个集成断点才能推进 P2-2/3/4/5。**这 4 个断点都是 Phase 8 整合交付时遗留的接缝未缝合问题**（P2 摸底期间由 3 个 explore 探查 + Oracle/Metis 2027-09-17 二次代码 fact-check 确认）。

> **Oracle + Metis 二次复评 2027-09-17** (修订版): 见 `design.md` D1-D8 + spec 8 个 MODIFIED Requirements
> - **D7** (扩展版): `dgpu_soc_with_pcie_ip.json` 中**所有** EP 外层 SimModule 端口 (`axi_slave_in/axi_master_out/msix_delivery_in/out` + `xbar[N]` 任何目标) 声明式接线都被静默丢弃 (per 19 §14.2.3, `test_axislavein_bridge_path_intact` 锁定), 全部走程序化桥接 (Phase 8 HB/RC 先例)
> - **D2 重写 (Metis fact-check)**: `CacheReqBundle` **无 `src_id`/`dst_id` 字段**, 实际字段是 `transaction_id[64]`. AXI 16-bit ID 直接装入 64-bit transaction_id, **不压缩**、**无双映射表**、**无 SLVERR 冲突**. 旧 design 8-bit 压缩基于不存在前提
> - **D3 API 修正 (Metis fact-check)**: `set_host_bypass()` API **不存在**于代码. 既有 API 是 `HostBypassTLM::attach_to_endpoint(EP*)` (HB-side, Phase 8 M1)
> - **D8 新增**: framebuffer 单一真源契约 (P0.5-landing 集成). Axi4CacheAdapter / SDMA / CompletionRing 出站写目标 = `DGpuBoard::framebuffer_` backing, **禁止**独立 mmap
> - **G6 基线实测**: `[pcie]` 36,598 / 415 cases, `[chstream]` 184 / 46 cases (实测, 旧 plan 写 36,454 / 155 是陈旧数据)
> - **G9 修订 (Metis)**: Catch2 标签语义 — 空格分隔 = **AND (交集)**, 逗号 = **OR**. spec Scenario 3 旧写"OR 逻辑"是事实错误, 已修订

**主因（按阻断优先级排序）**:

1. **协议不兼容（最关键, 误诊修正）**：Oracle 2027-09-17 代码核验 `pcie_endpoint_ip.cc` grep 无 `xbar`/`CacheReq` 引用 → **真实问题是 EP `axi_master_out` 到 xbar 之间压根没有任何桥接器**（不是"16-bit↔8-bit ID 不兼容"）。新增 `Axi4CacheAdapter` 做字段级转换, 1:1 直通 transaction_id.
2. **MSI-X 中断无连接**：EP `MsiXTable::irq_out` 端口无外部接线, MSI-X 投递 → Host 路径完全断.
3. **SDMA 模块未接入 `dgpu_soc_with_pcie_ip.json`**：`SdmaEngineTLM` 不在该 JSON 的 `modules` 列表中, 无法通过配置接线（仅 shell 层手动转发）.
4. **CompletionRing→EP irq_out 未接线**：4 端口 Ring 的 `done_out[3]` 端口无下游消费者.

**P2 任务依赖**：
- P2-2 (EP 侧 BAR decode 扩展：CP 区域转发) — **需 1 修复**
- P2-3 (`command_processor_mvp` 新增 reg file + doorbell→wake()) — **独立可启**
- P2-4 (Ring buffer 读取路径接入) — **需 4 修复**
- P2-5/6 (UE ByPass/Real PCIe 验证) — **需 1+2 修复**

**预期收益**：
- 4 个 EP↔SoC 集成断点全部闭合, E2E 数据流真正闭环
- P2-2/3/4/5/6/7 可在 1-2 周内顺利推进
- 新增 `Axi4CacheAdapter` 通用桥接组件 (~80 行, 比原 400+ 行简化 80%), 未来 GPGPU/SoC 集成可复用
- `[pcie-ep-soc-bridge]` 标签测试覆盖协议转换 + 中断 + JSON 接线 + framebuffer 契约

---

## What Changes

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/framework/axi4_cache_adapter.hh` | **新** (~80 行) | `Axi4CacheAdapter` 类声明 (transaction_id 直通, AxiErrorCode enum, Backpressure) |
| `src/framework/axi4_cache_adapter.cc` | **新** (~80 行) | 桥接实现 (AW/AR → CacheReq, CacheResp → R/B, fragment 多拍) |
| `include/chstream_register.hh` | 改 | 注册 `Axi4CacheAdapter` (`REGISTER_CHSTREAM`) |
| `include/framework/axi4_stream_adapter.hh` | 改 (暴露桥接点) | 新增 `axi_to_cache_out` / `cache_to_axi_in` 端口 (可选 set_cache_adapter setter) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 改 (D3/D4 + MSI-X, flag-gated) | msix_delivery_in/out ChPort + 3 setter (set_xbar_master/set_sdma_engine/set_completion_ring) + allocate_msix_vector() + flag 默认 disable |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | 改 (tick 扩展) | flag-gated MSI-X 聚合, 默认 disable 不破坏 Phase 8 M1 |
| `include/tlm/pcie/host_bypass_tlm.hh` | 改 (msix_delivery_in) | HB 侧新增 ingress 端口 (经既有 attach_to_endpoint 关联) |
| `src/tlm/pcie/host_bypass_tlm.cc` | 改 (tick MSI-X 投递) | HB 接收 EP push → 累加 msix_delivery_count_ |
| `include/tlm/gpu/sdma_engine_tlm.hh` | 改 (端口扩展) | 新增 `axi_slave_in` / `axi_master_out` (Axi4Bundle, 占位) + `trigger_doorbell()` API |
| `src/tlm/gpu/sdma_engine_tlm.cc` | 改 (doorbell 触发) | trigger_doorbell() 实现 + tick 既有逻辑不破坏 |
| `examples/dgpu_soc_with_pcie_ip.json` | 改 (sdma 声明) | 仅 sdma 模块声明 (无端口接线 per D7), metadata 注释程序化桥接 |
| `include/tlm/gpu/completion_ring_mvp.hh` | 改 (irq_out 注释) | header 注释改为 → pcie_ep.msix_delivery_in (per 19 §14.2.3) |
| `src/tlm/gpu/completion_ring_mvp.cc` | 改 (tick 投递) | 输出 MsiXDeliveryBundle (vector=CR_VECTOR 经 allocate_msix_vector) |
| `src/tlm/gpu/dgpu_board_shell.cc` | 改 (D8 init order assert + setter 顺序) | framebuffer_ptr_ == nullptr assert + set_sdma_engine + set_completion_ring + set_xbar_master 调用顺序 |
| `test/test_axi4_cache_adapter.cc` | **新** | `[pcie][pcie-ep-soc-bridge]` 双标签, transaction_id + fragment + OOO + Backpressure 用例 |
| `test/test_pcie_endpoint_ip_msix_path.cc` | **新** | `[pcie][pcie-ep-soc-bridge]` 双标签, EP→HB MSI-X 投递 4 用例 |
| `test/test_pcie_endpoint_ip_sdma_wiring.cc` | **新** | `[pcie][pcie-ep-soc-bridge]` 双标签, SDMA 程序化桥接 4 用例 |
| `test/test_pcie_endpoint_ip_completion_ring_wiring.cc` | **新** | `[pcie][pcie-ep-soc-bridge]` 双标签, CompletionRing 桥接 3 用例 |
| `test/CMakeLists.txt` | 改 | 添加 4 个新测试文件 (file GLOB 自动发现) |
| `ArchForge: docs/roadmap/phase9-p2-unblock.md` | **新** | P2 unblock 实施阶段文件 (跨仓 PR) |
| `ArchForge: docs/microarchitecture/dgpu-soc-pcie-slice.md` §9.4 | 改 | P2 unblock 状态 + 4 断点修复说明 (跨仓 PR) |
| `ArchForge: docs/architecture/19-pcie-ip-microarchitecture.md` 新 §13 | 改 | EP↔SoC 桥接章节 (含 Axi4CacheAdapter + MSI-X 双端口 + SDMA 程序化 + CompletionRing setter, 跨仓 PR) |
| `AGENTS.md` | 改 | STRUCTURE 节同步 (per G8) |

### 不变 (Frozen surface, 0 修改)

- **23 ABI 签名** (`include/abi/cpptlm_emulator.h`) — 0 修改 (frozen per ADR-088 §D5)
- **`MsiXDeliveryBundle` 定义** — 已在 `include/bundles/pcie_bundles_tlm.hh:119` 存在 (per Oracle 2027-09-17 fact-check, fields: `vector[16]`, `msg_data[32]`, `msg_addr[64]`, `trans_id[32]`), **不是新文件依赖**
- **现有 spec 不修改**: `pcie-ip-integration`, `pcie-axi-datapath-hardening`, `sdma-engine-tlm`, `host-bypass-and-rc`

## Scope

### IN-SCOPE

- **Axi4CacheAdapter 桥接组件** (新 C++ 类, ~80 行 hh + ~80 行 cc + 1 测试文件) — **D2 重写 (transaction_id 直通)**
- **MSI-X 中断路径**: EP `MsiXTable` → EP.msix_delivery_in → EP.msix_delivery_out → HB.msix_delivery_in (3 个新端口, 经 `attach_to_endpoint` 既有 API)
- **SDMA 模块接线**: `dgpu_soc_with_pcie_ip.json` 添加 sdma 模块声明 (无端口连接 per D7) + 程序化桥接
- **CompletionRing→EP irq_out 接线**: EP.set_completion_ring() 程序化桥接到 EP.msix_delivery_in
- **4 方向 AXI 流闭环**: Axi4CacheAdapter 出站 + EP.tick() flag-gated 默认 disable (D3/D4)
- **framebuffer 单一真源契约** (D8 新增): DGpuBoardShell init order assert + setter 拒收
- **新增 4 个测试文件**: `[pcie][pcie-ep-soc-bridge]` 双标签统一
- **跨仓 doc PR** (ArchForge): 19 §13 + dgpu-soc-pcie-slice §9.4 + phase9-p2-unblock.md

### OUT-OF-SCOPE

- CP (CommandProcessor) 寄存器映射设计 — P2-1 单独 change
- P2-2/3/4/5/6/7/8 实际任务执行 — 本 change 仅 unblock
- 真实 RTL 桥接 (CppHDL/HybridCache) — P13 单独
- 性能优化 (cycle-accurate 时序) — P4 范围
- PCIe 协议层增强 (FC, retry buffer) — Phase 1-3 已完成
- 任何新 ABI 函数追加 (P5 冻结)
- 真实驱动 (VFIO/IOMMUFD) 验证 — P2-5/6 范围
- **`set_host_bypass()` setter** (Oracle 修订): API 不存在, 用既有 `attach_to_endpoint()`
- **8-bit src_id 冲突检测** (Oracle 修订): 旧 design 基于不存在前提, 已废

## Acceptance Gate (9 项, Metis + Oracle 2027-09-17 修订版)

- [ ] **G1**: `openspec validate cpptlm-p2-integration-unblock --strict` PASS
- [ ] **G2**: `[pcie-ep-soc-bridge]` 标签测试 PASS (4 个新测试文件, **双标签 `[pcie][pcie-ep-soc-bridge]`** per G9)
- [ ] **G3**: 4 方向 AXI 流方向图全部 ✅ (D1/D2 既有 + D3/D4 新增 + MSI-X 双端口 + SDMA 程序化 + CompletionRing setter)
- [ ] **G4**: `Axi4CacheAdapter` 桥接 2 方向 round-trip PASS (D2 重写: transaction_id 直通, **不**含 8-bit 压缩)
- [ ] **G5**: `dgpu_soc_with_pcie_ip.json` `validate_topology` PASS (含 sdma 程序化桥接 + msix 路径, WARN 可接受 per D7)
- [ ] **G6**: 既有 `[pcie]` 测试零回归 (**实测基线 36,598 / 415 cases**, 双标签保证新增 ≥250)
- [ ] **G7**: 既有 `[chstream]` 测试零回归 (**实测基线 184 / 46 cases**)
- [ ] **G8**: ArchForge 仓 19 §13 + dgpu-soc-pcie-slice §9.4 + phase9-p2-unblock.md 同步; CppTLM AGENTS.md STRUCTURE 节同步
- [ ] **G9** (Oracle + Metis 修订): 4 个新测试文件**双标签** `[pcie][pcie-ep-soc-bridge]`, 验证 `./build/bin/cpptlm_tests "[pcie]"` 包含新测试 (**注意**: 空格 = AND, 逗号 = OR; 旧 plan "OR 逻辑"是事实错误)

## Capabilities

### New Capabilities

- `pcie-ep-soc-noc-axi-bridge`: PcieEndpointIP ↔ SoC NOC 的 AXI 协议桥接 + 中断 + SDMA + CompletionRing 完整闭环 (4 断点修复, D2/D3/D8 修订版)

### Modified Capabilities

无（现有 spec 不需要 requirement 级别修改. `pcie-axi-datapath-hardening` 关注 AXI 判别逻辑, `pcie-ip-integration` 关注 Phase 8 整合, `sdma-engine-tlm` 和 `host-bypass-and-rc` 是已存在 spec, 本 change 仅为 unblock, 不修改其 requirement 集）

## Impact

**新增代码** (~160 行):
- `Axi4CacheAdapter` 类 (~80 行 hh + ~80 行 cc, **比原 design 400+ 行简化 80%**)
- 4 个新测试文件 (~600 行总计)

**修改代码** (~30 行):
- `pcie_endpoint_ip.{hh,cc}`: tick 扩展 + 3 setter (flag-gated 默认 disable)
- `host_bypass_tlm.{hh,cc}`: msix_delivery_in 端口
- `sdma_engine_tlm.{hh,cc}`: axi_* 端口占位 + trigger_doorbell()
- `completion_ring_mvp.{hh,cc}`: irq_out header + tick 输出
- `dgpu_board_shell.cc`: D8 init order assert + setter 调用顺序
- `dgpu_soc_with_pcie_ip.json`: sdma 声明 (无端口 per D7)

**下游 unblock**:
- P2-2 (EP BAR decode 扩展) 可启动
- P2-3 (CP reg file + doorbell→wake) 可独立
- P2-4 (Ring buffer 读取) 可启动
- P2-5/6 (UE ByPass/Real PCIe 验证) 可启动
- P2-7/8 (运行时 config + cp_attach 真实化) 可启动

**测试覆盖 (Metis + Oracle 实测)**:

| 标签 | 实测基线 | 新增预期 | 合计 |
|------|---------|----------|------|
| `[pcie]` | **36,598 / 415 cases** | ≥250 (4 文件双标签) | ≥36,850 |
| `[chstream]` | **184 / 46 cases** | 0 (本 change 不改 chstream) | ≥184 |
| `[pcie-ep-soc-bridge]` | 0 (新标签) | ≥11 (4 文件双标签, 与 `[pcie]` floor 同) | ≥11 |
| `[sdma]` | 既有 | 0 (既有测试 0 回归) | — |

**风险** (Metis + Oracle 重写版):

| # | 风险 | 等级 | 缓解 |
|---|------|:----:|------|
| 1 | ~~Axi4CacheAdapter 状态机设计缺陷~~ | ~~中~~ | **已废 (D2 重写)**: transaction_id 直通消除 OOO/conflict 复杂度 |
| 2 | ~~AXI 16→8 压缩冲突~~ | ~~中~~ | **已废**: `CacheReqBundle.transaction_id` 是 64-bit, 直通无冲突 |
| 3 | Axi4CacheAdapter 实施错误 (transaction_id 错位 / fragment bug) | 中 | TDD 5 步先写测试, fragment 多拍 + 多 outstanding 并发 |
| 4 | EP tick() 扩展破坏 Phase 8 M1 | 中 | flag-gated 默认 disable (Commit 2), `[pcie]` 36,598 基线 + test_pcie_endpoint_ip_full_e2e gate |
| 5 | **D2 旧前提 8-bit 压缩** (历史) | **高** | **已废**: 实施前 P0.5-2 code review 必须确认无压缩逻辑 |
| 6 | JSON 接线被 19 §14.2.3 静默丢弃 | **高** | D7 程序化桥接强制, 范围扩展含 xbar 下游 |
| 7 | **framebuffer 单一真源破坏** (D8) | **高** | D8 init order assert + setter 拒收, spec 新增 3 Scenario 验证 |
| 8 | SDMA bundle 类型误标 | 中 | D4 锁定 5 端口全 PcieTlpBundle, header 注释明确 |
| 9 | MSI-X vector 命名空间冲突 | 中 | D3 修订: vector 由 EP `allocate_msix_vector()` 统一分配 |
| 10 | Catch2 标签语义误解 | 低 | spec Scenario 修订: 空格 = AND, 逗号 = OR |

---

## 关联

- **P2 主计划** (ArchForge): `docs/roadmap/phase9-p2-cp-attach-via-axi.md` (本 change 是其 unblock)
- **P0.5 unblock 阶段文件** (ArchForge, Commit 4 创建): `docs/roadmap/phase9-p2-unblock.md`
- **架构文档** (ArchForge, Commit 4 新 §13): `docs/architecture/19-pcie-ip-microarchitecture.md`
- **dgpu-soc-pcie-slice** (ArchForge, Commit 4 §9.4): `docs/microarchitecture/dgpu-soc-pcie-slice.md`
- **PCIe EP 集成 spec**: `openspec/specs/pcie-ip-integration/spec.md` (Phase 8 交付, 不改)
- **AXI Datapath Hardening spec**: `openspec/specs/pcie-axi-datapath-hardening/spec.md` (AXI 判别逻辑, 不改)
- **SDMA spec**: `openspec/specs/sdma-engine-tlm/spec.md` (SDMA 引擎, 不改)
- **HostBypass + RC spec**: `openspec/specs/host-bypass-and-rc/spec.md` (Host 桥接, 不改)
- **P0.5-landing 前置** (archived 2027-09-17): `openspec/changes/archive/2026-09-25-2027-09-17-cpptlm-minimal-dgpu-soc-v1-landing/`
  - 关键依赖: `DGpuBoard::load_soc_config` 自动消费 JSON `framebuffer_size_bytes` (size cap 64GB, override warning, reverse-order guard)
  - **D8 契约**: 本 change 新增 4 断点修复**不能破坏** framebuffer 单一真源
  - 零文件重叠, 可并行推进
- **AGENTS.md**: KEY INVARIANTS (framebuffer 自动分配条目 + OpenSpec Proposed ≤3 KPI) + DOC HYGIENE (`docs/soc_arch/` 已迁 ArchForge, 本仓禁直接修改)
- **本 change 自身**: `design.md` (D1-D8 决策 + 风险表) + `tasks.md` (4 commit 结构) + `specs/pcie-ep-soc-noc-axi-bridge/spec.md` (8 MODIFIED Requirements)