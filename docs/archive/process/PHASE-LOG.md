# PHASE-LOG — 交付流水（从 AGENTS.md §PHASE STATE 外移）

> **来源**：2026-11-19 由 `AGENTS.md` 的 §PHASE STATE 外移（change `cpptlm-agents-md-slimming` W2）。
> **原因**：AGENTS.md 注入**每个** agent 上下文，过程日志（commit 区间 / Oracle 评审轮次 / 归档 Entry #）按该文件 §DOC HYGIENE 的硬性规则**不得**留在其中。
> **当前态**见 [`../../../AGENTS.md`](../../../AGENTS.md) §PHASE STATE（只留终态表 + 实时计数命令）。
> **本文件是历史记录**：⛔ 不追改已发生的流水；新增流水追加到对应线的小节末尾。

---

> 本节只记**当前态**。历史评审过程 / finding 编号 / 归档流水见 OpenSpec `openspec/changes/archive/` 与 ADR `## Status Update` 段。

### ★ CEP 平台 (Coherence Exploration Platform) —— 当前主线

| 项 | 内容 |
|----|------|
| **治理** | `ADR-CEP-00`（跨仓归口 + 准入门槛）+ `ADR-CEP-00A`（真源章节锚点表） |
| **ADR** | 23 份 `ADR-CEP-*`（fabric / coherence / protocol / experiment / ownership 各域;⚠️ 文件数 24 = 23 + `NUMBER-MAP`） |
| **实现** | `include/tlm/cep/` 43 个 header-only 文件 + `test/test_cep_*.cc` |
| **OpenSpec** | 46 份 `cep-*` main spec；**0 active** change；162 份已归档 change（2026-11-19 实测） |
| **守门** | `scripts/test/cep_*.sh` 契约闸（数量以 `ls scripts/test/cep_*.sh` 为准）+ 17 个 `.cc` 编译探针（冻结面 / 实验 4 件套 / 预登记 drift / ArchForge 对齐 / 声明对齐 / 反向 fixture） |
| **测试** | `[cep]` —— 规模最大的测试集 |
| **状态** | 队列 #3~#14 已全部归档（2026-10-26 ~ 2026-11-04）; 当前 0 个 active change（`cpptlm-module-param-migrate-batch2` 于 2026-11-19 归档,per 台账 Entry #500;`cpptlm-config-type-registration-align` 于 2026-11-18 归档,per 台账 Entry #487;`cpptlm-module-param-effect-landing` 于 2026-11-18 归档,per 台账 Entry #485;`cpptlm-config-type-registration-align` 于 2026-11-18 立项,per 台账 Entry #480;`cpptlm-nictlm-credit-flit-filter` 于 2026-11-18 归档,per 台账 Entry #479;档 2 `cep-evidence-tier2-minimal-slice` 于 2026-11-14 归档,per 台账 Entry #412） |

### ★ PCIe EP 微架构 (2026-2027, 7 阶段全部交付)

| Phase | 内容 | commits | Oracle | 状态 |
|------|------|---------|--------|------|
| Umbrella | `openspec/changes/archive/2026-09-03-2026-09-01-cpptlm-dgpu-pcie-ip-microarch/` | — | — | ✅ 引导 |
| Phase 1 | PCIe Link Layer + FC | `e9b1b..` 等 | ✅ PASS | ✅ 完成 |
| Phase 2 | 128b/130b Encoding Latency | `8d1f1d5` `b21d290` | ✅ PASS | ✅ 完成 |
| Phase 3 | PHY Digital Ctrl + Bypass Mux | `05be913`..`bac9267` | ✅ 9/10 复评 PASS | ✅ 完成 |
| Phase 4 | SR-IOV VF Pool + PcieEndpointIP | `4e9564e` + `478cdd9` `a442b65` `536dbfc` `6ebbd7d` | ✅ PASS | ✅ 完成 |
| Phase 5 | AXI Stream Adapter | `710c734`..`dd8c44a` | ✅ M1/M2/M3 修复 + 复评 PASS | ✅ 完成 |
| Phase 6 | AXI4Mapper | `8b92bfb`..`fe4d745` | ✅ PASS | ✅ 完成 |
| Phase 7 | Host Bypass + RC | `ce50b05`..`45763fa` | ✅ 有条件 PASS (M1/M2) | ✅ 完成 |
| **Phase 8** | **整合交付** | **`e29defd`..`429327d`** | **—** | **✅完成** |

**Phase 8 整合交付 (W24 末)**:
- `docs/soc_arch/architecture/19-pcie-ip-microarchitecture.md`（从 umbrella design.md 迁移, **已迁 ArchForge**）
- `examples/dgpu_soc_with_pcie_ip.json` 完整 dGPU SoC + PCIe EP 配置（**硬件建模面示例**，语义以 ArchForge 规格为准）
- `examples/dgpu_soc_pcie_ep_framework_min.json` EP **最小框架验证示例**（只有 `axi_adapter` + `bar_sizes` ⇒ 拓扑装配 / StreamAdapter 挂接 / BAR 可见面；`cpptlm_sim` 可跑）
- `test/test_pcie_endpoint_ip_full_e2e.cc` 全链路 E2E (3 TEST_CASE, solve Phase 7 M1)
- `PcieEndpointTLM` 加 `[[deprecated]]` 标注（迁移提示, ABI 不动）
- 桥接修复 (429327d): HostBypassTLM/RC::tick() 自动转发 4 方向 AXI 通道, 让 EP 真实消费请求

**已知问题 (Minor, 不阻断)**:
- `ch_uint<512>` 既有 `.read()/.write()` 调用仍走低 64-bit 截断(向后兼容 shim,per P0 fix 2026-10-06);P1/P2 迁移到 `slice<>/set_slice<>` 后可移除 shim

### ★ D-AXI Driver-Visible Minimal SoC（跨 Phase 8 与 D-AXI change）

| Phase | 内容 | Oracle | 状态 |
|------|------|--------|------|
| v1.0 | 初版提案（5 澄清 + R1-R7 + M1-M5） | — | ✅ 引导 |
| v1.1 | P0 修订（5 澄清落点 + R1-R7 + M1-M5） | — | ✅ 完成 |
| v1.2 | Oracle 二次审查 8 must-fix (N1-N12 全部落地) | ✅ PASS | ✅ 完成 |
| **v1.4** | Metis/Oracle 二轮 + 用户质疑 + 单一 VRAM 所有权上提 (B7-B13 + 7 实施铁律 + 3 遗留议题延期) | ✅ 2 轮 PASS | ✅ **实施完成** |
| **v0.5 timing-mode (AT) SoC MVP** | Phase 4 T1-T8 完成 (MemoryTLM cycle-approximate + GmmuTLM TLB + SDMA cycle accounting + VramControllerTLM + `DGpuBoard::init_timing_mode` + `dgpu_soc_timing_v1.json` + 37 E2E cases) | ADR-DGPU-11 八轮 PASS | ✅ **实施完成** |

**v1.4 P0 修正 (B7-B13, 架构根因)**: B7 单一 VRAM backing 所有权归 DGpuBoard（8GB `unique_ptr<uint8_t[]>` default-init, Linux lazy commit, stable pointer）/ B8 bound = injected `backing_size_`（非 `kDefaultMemSize` 常量）/ B9 `handle_slave_port` 检查 `memory_read/write` 返回值（失败置 `resp.resp.write(1)` = SLVERR）/ B10 保留 `std::mutex backing_mutex_`（host/sim 并发保护）/ B11 双 size 字段拆分 `bar1_window_size_` vs `vram_size_` / B12 未注入 backing 时返 `-ENODEV` + `kRegMemSizeLo/Hi` 读 injected 值 / B13 24-case `[pcie-memory]` 机械迁移 + `[minimal_dgpu_soc]` 添加 BAR2 + 4 处 lazy alloc 行为删除。详见 `docs/pcie/driver-visible-minimal-soc.md` §4 + ADR-DGPU-05~09。

**7 条实施铁律**与**3 项遗留议题**（D1 Display FB / doorbell 0x10010000 / MemoryTLM capacity_gb=1）见 `docs/pcie/driver-visible-minimal-soc.md`。

**D3 演进 seam (v1.4 已就位, Phase 4 T4 实施完成)**：`handle_slave_port ↔ backing_ptr_` 之间已插入 VramControllerTLM (继承 MemoryTLM + 行缓冲 + bandwidth 上限)。

### ★ 已归档的 dGPU SoC 提案（不再 active）

| Change | 归档 | 状态 |
|--------|------|------|
| `cpptlm-dgpu-soc-internal-noc-e1` v1.1 | 2026-10-09 | ✅ 已归档（DataNoC only 范围缩减；详见 `openspec/changes/archive/2026-10-09-cpptlm-dgpu-soc-internal-noc-e1/`） |
| `cpptlm-pcie-host-test-flr-rebar` v1.1 | 2026-10-09 | ✅ 已归档（复用既有 `flr_pf()`/`flr_vf()` + Resizable BAR；详见 `openspec/changes/archive/2026-10-09-cpptlm-pcie-host-test-flr-rebar/`） |
| `cpptlm-dgpu-soc-arch-truth-cleanup` v1.3 | 2026-10-10 | ✅ 已归档（doc-only，19 项 finding 修复；不计入 Proposed KPI 上限） |

### ★ GPGPU / SoC 既有交付（2026 之前）

| 组件 | 内容 | 状态 |
|------|------|------|
| 9 类 SimModule (P2-P5) | CpuCluster / ComputeCluster / TpcCluster / GpcCluster / GpuCluster / CacheCluster / MemoryCluster / GpuNoC / ApuSoC | ✅ 完成 + Oracle 评审 |
| GPGPU 端 MVP | GPU CU / Warp / Vector RegFile / KernelLaunch / MeshNoC / SharedMemory / MemoryCluster | ✅ 完成 |
| SM 重构 (Task 1-14) | 12 ChStream 子模块 + 8 Bundle + IComputeDevice 15 方法; supersedes KernelLaunch/CudaCoreAdapter/PtxEmuSubmodule | ✅ 完成 |
| DMA / Doorbell / CommandProcessor / CompletionRing MVP | 端到端 SoC 集成路径 | ✅ 完成 |
| 顶层 ApuSoC | CPU侧 + GPGPU侧 + Crossbar 互联, `incorporate_parent` 钩子(借鉴 gem5 late-binding) | ✅ 完成 |
| dGPU Board Shell | `src/tlm/gpu/dgpu_board_shell.cc` 端到端板级集成 | ✅ 完成 |
| 配置文件 | `apu_soc_v1.json` / `apu_soc_full.json` / `dgpu_board_v1.json` / `dgpu_soc_minimal_v1.json` / `dgpu_soc_timing_v1.json` / `dgpu_soc_v2_realip_p2.json` | ✅ 完成 |
| 退役配置 | `configs/dgpu_soc_v1.json.in.disabled` (per 2026-10-09 季度复审, PcieEndpointTLM deprecated) | ⚠️ Deprecated |
