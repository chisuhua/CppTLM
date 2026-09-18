# cpptlm-p2-integration-unblock: P2 (CP 接入) 集成阻塞修复

> **状态**: 📋 Proposed — 2027-09-17
> **目标**: Unblock Phase 9 P2 (CP 跨仓接入) 启动的 4 个集成断点
> **关联 P2 计划**: `docs/soc_arch/roadmap/phase9-p2-cp-attach-via-axi.md`
> **关联 EP 文档**: `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` §3, §7-9
> **路线图位置**: Phase 9+ P0.5（介于 P0 收尾 + P1 SM Gate 验证 + P2 启动之间）

## Why

Phase 9 P2（CP 跨仓接入）在 P2-1 写完 CP 寄存器映射 spec 后，需要在 CppTLM 侧打通 4 个集成断点才能推进 P2-2/3/4/5。**这 4 个断点都是 Phase 8 整合交付时遗留的接缝未缝合问题**（P2 摸底期间由 3 个 explore 探查确认）：

> **Oracle 复评 2027-09-17** (修订版): 见 `design.md` §F P0-1~4 + 新增 D7 (JSON 接线约束)
> - **D7**: `dgpu_soc_with_pcie_ip.json` 中 `pcie_ep.axi_slave_in/axi_master_out` 声明式接线被静默丢弃（per 19 §14.2.3, `test_axislavein_bridge_path_intact` 锁定）, EP 外层 SimModule 端口 JSON 接线需走程序化桥接 (Phase 8 HB/RC 先例) 或 connection_resolver 递归解析扩展
> - **D3 修订**: EP MSI-X 路径需 **2 个独立端口** (`msix_delivery_in` ingress ←SDMA/CR + `msix_delivery_out` egress →HB), 原版命名混乱已修正
> - **D4 修订**: SDMA 现有 5 端口全 `PcieTlpBundle` (`desc_in` kind=DMA_DESC / `host_out` index=3 / `done_out` index=4), 新增 2 个 `axi_*` 端口用于 AXI 路径, 接线描述需按真实 bundle 类型重写
> - **G6 基线**: `[pcie]` 36,454 / `[chstream]` 155 已实测确认 (proposal G6/G7 数字正确, 19 §12 的 66,564 是全量总数, AGENTS.md 的 44,498 同为不同时点总数)
> - **新增 G9**: 新测试文件**双标签** `[pcie][pcie-ep-soc-bridge]` (Catch2 标签不嵌套, 避免 G6 增长数学失效)

**主因（按阻断优先级排序）**：

1. **协议不兼容（最关键）**：`PcieAxiAdapter` 用 `Axi4Bundle`（awaddr/awid/awlen/wdata/bid/rid），`CrossbarTLM` 用 `CacheReqBundle`/`CacheRespBundle`。EP→SoC 读响应路径 BROKEN — xbar 返回的 `CacheRespBundle` 无法直接映射回 EP 的 `axi_master_resp`。
2. **MSI-X 中断无连接**：EP `MsiXTable::irq_out` 端口无任何外部接线，MSI-X 投递 → Host 路径完全断。
3. **SDMA 模块未接入 `dgpu_soc_with_pcie_ip.json`**：`SdmaEngineTLM` 不在该 JSON 的 `modules` 列表中，无法通过配置接线（仅 shell 层手动转发）。
4. **CompletionRing→EP irq_out 未接线**：4 端口 Ring 的 `done_out[3]` 端口无下游消费者。

**P2 任务依赖**：
- P2-2 (EP 侧 BAR decode 扩展：CP 区域转发) — **需 1 修复**
- P2-3 (`command_processor_mvp` 新增 reg file + doorbell→wake()) — **独立可启**
- P2-4 (Ring buffer 读取路径接入) — **需 4 修复**
- P2-5/6 (UE ByPass/Real PCIe 验证) — **需 1+2 修复**

**预期收益**：
- 4 个 EP↔SoC 集成断点全部闭合，E2E 数据流真正闭环
- P2-2/3/4/5/6/7 可在 1-2 周内顺利推进
- 新增 `Axi4CacheAdapter` 通用桥接组件，未来 GPGPU/SoC 集成可复用
- `[pcie-ep-soc-bridge]` 标签测试覆盖协议转换 + 中断 + JSON 接线

## What Changes

| 文件 | 变化 | 说明 |
|------|------|------|
| `include/framework/axi4_cache_adapter.hh` | **新** | `Axi4CacheAdapter` 类声明 (Axi4Bundle ↔ CacheRespBundle 协议桥接) |
| `src/framework/axi4_cache_adapter.cc` | **新** | 桥接实现 (2 方向状态机: aw+ar → req, r+b → resp) |
| `include/chstream_register.hh` | 改 | 注册 `Axi4CacheAdapter` (`REGISTER_CHSTREAM`) |
| `include/framework/axi4_stream_adapter.hh` | 改 | 暴露 `axi_to_cache_out` / `cache_to_axi_in` 端口 (桥接点) |
| `include/tlm/pcie/pcie_endpoint_ip.hh` | 改 | `tick()` 增加 MSI-X 投递 → HostBypassTLM 路径; 增加 EP→SoC 读响应桥接点 |
| `src/tlm/pcie/pcie_endpoint_ip.cc` | 改 | 4 方向 AXI 流: Direction 3 补 SoC→EP 读响应桥接 |
| `include/tlm/pcie/host_bypass_tlm.hh` | 改 | 新增 `msix_delivery_in` 端口 (接收 EP MSI-X 投递) |
| `src/tlm/pcie/host_bypass_tlm.cc` | 改 | tick() 新增 MSI-X 投递处理 + 中断计数 |
| `include/tlm/gpu/sdma_engine_tlm.hh` | 改 | 新增 `axi_slave_in` / `axi_master_out` 端口 (JSON 可接线) |
| `src/tlm/gpu/sdma_engine_tlm.cc` | 改 | tick() 新增 doorbell → 触发; desc_in ↔ EP BAR 读取 |
| `examples/dgpu_soc_with_pcie_ip.json` | 改 | 添加 `sdma` 模块 (声明式接线受限, 详见 Oracle 风险 §F + design D7); 添加 MSI-X 投递路径 |
| `include/tlm/gpu/completion_ring_mvp.hh` | 改 | `irq_out[3]` 端口明确连接目标 (EP 或 MSI-X) |
| `src/tlm/gpu/completion_ring_mvp.cc` | 改 | tick() 输出中断信号到 EP 路径 |
| `test/test_axi4_cache_adapter.cc` | **新** | `[pcie-ep-soc-bridge]` 标签测试 (2 方向 round-trip, OOO, error) |
| `test/test_pcie_endpoint_ip_msix_path.cc` | **新** | MSI-X 投递 EP→HB E2E 测试 |
| `test/test_pcie_endpoint_ip_sdma_wiring.cc` | **新** | SDMA JSON 接线 E2E 测试 |
| `test/test_pcie_endpoint_ip_completion_ring_wiring.cc` | **新** | CompletionRing→EP irq_out E2E 测试 |
| `test/CMakeLists.txt` | 改 | 添加 4 个新测试文件 (file GLOB 自动发现) |
| `docs/soc_arch/roadmap/phase9-p2-unblock.md` | **新** | P2 unblock 实施阶段文件 |
| `docs/soc_arch/modules/dgpu-soc-pcie-slice.md` | 改 | §9.4 新增 P2 unblock 状态 + 4 断点修复说明 |

## Scope

### IN-SCOPE

- **Axi4CacheAdapter 桥接组件** (新 C++ 类, 1 个文件 + 1 测试文件)
- **MSI-X 中断路径**: EP `MsiXTable` → HostBypassTLM 新增 `msix_delivery_in` 端口
- **SDMA 模块接线**: `dgpu_soc_with_pcie_ip.json` 添加 sdma 模块 + EP↔SDMA↔VRAM 完整路径
- **CompletionRing→EP irq_out 接线**: 明确 4 端口 Ring 的 `done_out[3]` 下游
- **4 方向 AXI 流闭环**: PcieEndpointIP::tick() 补全 Direction 3 (SoC→EP 读响应)
- **JSON 配置文件**: `dgpu_soc_with_pcie_ip.json` 添加 sdma + msix 路径连接
- **新增 4 个测试文件**: `[pcie-ep-soc-bridge]` 标签统一
- **AGENTS.md** STRUCTURE 节 + `dgpu-soc-pcie-slice.md` §9.4 同步

### OUT-OF-SCOPE

- CP (CommandProcessor) 寄存器映射设计 — P2-1 单独 change
- P2-2/3/4/5/6/7/8 实际任务执行 — 本 change 仅 unblock, 不实现 P2 任务本身
- 真实 RTL 桥接 (CppHDL/HybridCache) — P13 单独
- 性能优化 (cycle-accurate 时序模型) — P4 范围
- PCIe 协议层增强 (FC, retry buffer) — Phase 1-3 已完成
- 任何新 ABI 函数追加 (P5 冻结)
- 真实驱动 (VFIO/IOMMUFD) 验证 — P2-5/6 范围

## Acceptance Gate (9 项, Oracle 修订版)

- [ ] **G1**: `openspec validate cpptlm-p2-integration-unblock --strict` PASS
- [ ] **G2**: `[pcie-ep-soc-bridge]` 标签测试 PASS (4 个新测试文件, **双标签 `[pcie][pcie-ep-soc-bridge]`** per G9)
- [ ] **G3**: 4 方向 AXI 流方向图全部 ✅ (Host→EP 写/读, EP→SoC 写/读, MSI-X 投递, SDMA, CompletionRing)
- [ ] **G4**: `Axi4CacheAdapter` 桥接 2 方向 round-trip PASS (Axi4Bundle→CacheReqBundle→CacheRespBundle→Axi4Bundle)
- [ ] **G5**: `dgpu_soc_with_pcie_ip.json` `validate_topology` PASS (含 sdma 程序化桥接 + msix 路径)
- [ ] **G6**: 既有 `[pcie]` 测试零回归 (≥36,454 assertions PASS, 实测基线 36,454)
- [ ] **G7**: 既有 `[chstream]` 测试零回归 (155 assertions PASS, 实测基线 155)
- [ ] **G8**: AGENTS.md STRUCTURE 节同步 + `dgpu-soc-pcie-slice.md` §9.4 + `19-pcie-ip-microarchitecture.md` 新增 §13 EP↔SoC 桥接 章节
- [ ] **G9** (Oracle 新增): 4 个新测试文件**双标签** `[pcie][pcie-ep-soc-bridge]`, 验证 `./build/bin/cpptlm_tests "[pcie]"` 包含新测试 (避免 Catch2 标签不嵌套导致 G6 增长数学失效)

## Capabilities

### New Capabilities

- `pcie-ep-soc-noc-axi-bridge`: PcieEndpointIP ↔ SoC NOC 的 AXI 协议桥接 + 中断 + SDMA 集成完整闭环 (4 断点修复)

### Modified Capabilities

无（现有 spec 不需要 requirement 级别修改。`pcie-axi-datapath-hardening` 关注 AXI 判别逻辑, `pcie-ip-integration` 关注 Phase 8 整合, `sdma-engine-tlm` 和 `host-bypass-and-rc` 是已存在 spec, 本 change 仅为 unblock, 不修改其 requirement 集）

## Impact

**新增代码**:
- `Axi4CacheAdapter` 类 (~150 行 hh + ~250 行 cc)
- 4 个新测试文件 (~600 行总计)

**修改代码**:
- `pcie_endpoint_ip.{hh,cc}`: tick() 逻辑扩展 (Direction 3 + MSI-X 投递)
- `host_bypass_tlm.{hh,cc}`: 新增 msix_delivery_in 端口
- `sdma_engine_tlm.{hh,cc}`: 端口扩展 + doorbell 触发
- `completion_ring_mvp.{hh,cc}`: irq_out 明确目标
- `axi4_stream_adapter.hh`: 桥接点暴露
- `chstream_register.hh`: 新组件注册
- `dgpu_soc_with_pcie_ip.json`: 添加 sdma + msix 路径

**下游 unblock**:
- P2-2 (EP BAR decode 扩展) 可启动
- P2-3 (CP reg file + doorbell→wake) 可独立
- P2-4 (Ring buffer 读取) 可启动
- P2-5/6 (UE ByPass/Real PCIe 验证) 可启动
- P2-7/8 (运行时 config + cp_attach 真实化) 可启动

**测试基线变化** (Oracle 修订, 双标签下两标签断言数相同):
- `[pcie]` 从 36,454 → 预期 ≥ 36,700 assertions (新增 4 文件 ~250 assertions, **双标签保证 `[pcie]` 过滤器包含新测试**)
- `[chstream]` 维持 155 assertions
- `[pcie-ep-soc-bridge]` 新标签, 与 `[pcie]` 同 250 assertions (双标签), G2 floor ≥30

**风险**:
- `Axi4CacheAdapter` 设计需谨慎 (2 方向状态机, OOO 响应匹配)
- EP tick() 扩展可能影响 Phase 8 M1 既有 4 方向闭环逻辑 — 需回归测试
- JSON 新增模块可能影响 `validate_topology` 既有检查

---

## 关联

- **P2 主计划**: `docs/soc_arch/roadmap/phase9-p2-cp-attach-via-axi.md` (本 change 是其 unblock)
- **架构文档**: `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md` (EP 内部数据流 + 4 方向 AXI)
- **PCIe EP 集成 spec**: `openspec/specs/pcie-ip-integration/spec.md` (Phase 8 交付)
- **AXI Datapath Hardening spec**: `openspec/specs/pcie-axi-datapath-hardening/spec.md` (AXI 判别逻辑)
- **SDMA spec**: `openspec/specs/sdma-engine-tlm/spec.md` (SDMA 引擎)
- **HostBypass + RC spec**: `openspec/specs/host-bypass-and-rc/spec.md` (Host 桥接)
- **AGENTS.md**: 项目结构 + 测试命令基线
