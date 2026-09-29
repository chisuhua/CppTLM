# cpptlm-dgpu-soc-timing-mvp: Timing-Mode (AT) DGpu SoC 性能建模 MVP

> **状态**: 📋 提案 — 2027-02-09 (**v0.2**,Oracle 二轮 H1-H6 + 三轮 12 处残余 + 四轮 7 处新识别已全部落地)
> **作者**: CppTLM Team
> **优先级**: 🟡 P1 (minimal_v1 functional-mode 已交付后,timing-mode 填补性能建模空白)
> **工期**: **11.5-12 工作日** (T0=1 + T1=1 + T2=1 + **T3=0.5** + T4=1 + **T5=2** + T6=1 + T7=2 + T8=0.5 + buffer 1.5)
> **目标**: 在 CppTLM dGPU SoC 中新增 **timing-mode (AT) SoC**,与 functional-mode minimal_v1 **并列共存**;提供 cycle-approximate 性能建模能力(driver 性能回归 + 带宽分析),**不**替换 functional-mode。
> **关联文档**: [docs/designs/dgpu-soc/timing-mode.md](../../docs/designs/dgpu-soc/timing-mode.md) (第 1 层架构视图)
> **配套 ADR** (✅ **已签发** 2027-02-09, T0 阶段): [ADR-DGPU-11-timing-mode-soc-scope.md](../../../docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md) (Oracle 八轮复评 ✅ PASS, 21/21 硬伤落盘)
> **基础 ADR**: ADR-DGPU-05 (单一 VRAM) + ADR-DGPU-06 (AxiMemBundle 边界) + ADR-DGPU-07 (演进 seam)

## Why

### 上下文：minimal_v1 functional-mode 已交付,缺 timing-mode

| 已交付 | 状态 |
|--------|------|
| **functional-mode minimal_v1** (LT, zero-delay) | ✅ Oracle 二轮复审通过,66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路) + D-AXI v1.4 实施真相 (commit `429327d`) |
| **driver 闭环正确性** (UE driver 8 步 ABI) | ✅ P0.5-landing 验证,28 assertions + 41 E2E cases 全绿 |
| **timing-mode (AT) 性能建模** | ❌ **缺失** — driver 端**仅**能验证"是否对",**不能**验证"多快" |

### 4 个用户决策 + 架构动机

| # | 决策来源 | 动机 |
|---|---------|------|
| U1 | 用户 Q: "能否把 SoC 改成 timing?" (2027-02-09) | 性能回归需要;workload 分析需要 |
| U2 | Oracle 5 步解锁链 Step 1 | functional-mode 5 项简化是**设计意图**,不是技术债 |
| U3 | gem5 `--mem-type=timing` vs `--cpu-type=atomic` | timing/functional 是**并列**两套模型,非"哪个更好" |
| U4 | ADR-DGPU-07 演进 seam | D3 VramControllerTLM 提前到 v0.1 是 seam 自然演进 |

### 为什么需要独立 timing-mode SoC

| 候选方案 | 优点 | 缺点 | 决策 |
|----------|------|------|------|
| **A. 改造 minimal_v1 为 timing** | 单一 SoC | 违反 6 条 Invariants,破坏 66951 assertions (per AGENTS.md baseline; 含 [minimal_dgpu_soc] 41 + [pcie-memory] 24 + [abi][minimal_dgpu_soc] 28 + D2 memory 67 + Phase 1-8 全链路),driver 闭环重测 | ❌ |
| **B. 新增 timing-mode 并列 (本提案)** | functional 不变,timing 独立配置 + 独立测试 | 双套模块需双套测试套件 | ✅ |
| **C. v2.0 完全重写** | 单一 spec | 等价于重新设计 MVP,失去 driver 闭环已交付价值 | ❌ |

**采用方案 B**。镜像 gem5 `simulate.py` 的 `--mem-type`/`--cpu-type` 选择模式,本提案:
- minimal_v1 (functional, LT) = `dgpu_soc_minimal_v1.json` (**已交付**,不变)
- timing-mode (AT, cycle-approximate) = `dgpu_soc_timing_v1.json` (**🆕 本提案**)
- 同一 DGpuBoard 框架,通过 `simulation_mode` 字段切换

### 预期收益

1. **性能回归能力** — driver 端能做 workload-level profiling(H2D/D2H throughput、fence 周期)
2. **带宽分析** — CrossbarTLM 暴露 `stats_port_utilization_`,识别 NoC 瓶颈
3. **模块验证** — TLB hit rate、cycle accounting 准确性 (经 SdmaEngineTLM `last_descriptor_complete_cycle_`)、cache line hit/miss 真实测试 (**v0.2 已删除 outstanding, per H3, "outstanding efficiency" 收益项作废**)
4. **D3/D4 演进 seam** — `VramControllerTLM` 提前到 v0.1 是 ADR-DGPU-07 演进路径自然落地
5. **架构清洁** — functional/timing 两模式独立 spec + 独立测试,互不污染
6. **后续可演进** — Phase 10+ 真实硬件校准 / 多通道 HBM / 多级页表都有基础

### 非目标 (Non-Goals)

- **NG1**: 不替换 minimal_v1 functional-mode (两模式并列)
- **NG2**: 不做多级页表硬实现(本版本 GMMU 仍一级,TLB miss 走 N-cycle 模拟延迟)
- **NG3**: 不改 23 ABI 签名 (per ADR-088 §D5)
- **NG4**: 不改 SDMA 既有 5 端口语义 (v0.2);timing-mode **不**新增 AXI master port、**不**新增 outstanding (per H3 Oracle 反馈)
- **NG5**: 不做多通道 HBM 仿真(单通道 HBM timing,D4 后续)
- **NG6**: 不引入第三方依赖(Cycle-accurate NoC 用既有 `crossbar_tlm.hh` + `link_tlm.hh`)
- **NG7**: 不做 cycle-approximate RTL 级精度(本提案是 cycle-approximate,非 cycle-approximate)
- **NG8**: 不与具体 GPU SKU 时序数字对齐(cycles 是仿真建模值)

## What Changes

### §1 范围: Timing-Mode SoC MVP 实施

**功能边界**(8 模块,2 新建,3 扩展):

| 模块/文件 | 改动类型 | 改动 |
|----------|----------|------|
| `include/tlm/memory_tlm.hh` | 🔧 扩展 | 新增 `defer_response()` + 5 个时序参数 setter + `pending_resps_` 队列 + `current_cycle_` 成员 |
| `include/tlm/gpu/gmmu_tlm.{hh,cc}` | 🔧 扩展 | 新增 `translate_timing()` (含 latency 输出) + 32-entry TLB + `TranslateStats` |
| `include/tlm/gpu/sdma_engine_tlm.{hh,cc}` | 🔧 扩展 | cycle accounting (沿用既有 5-port + backdoor,per H2/H3 v0.2 简化) + `last_descriptor_complete_cycle_` 记录 + `set_cycle_accounting_enabled()` setter (per H3) |
| **`include/tlm/vram_controller_tlm.{hh,cc}`** | 🆕 新建 | 继承 MemoryTLM 行为 + 行缓冲 hit/miss + bandwidth 限制 + bandwidth 上限模拟 |
| `include/tlm/crossbar_tlm.hh` (复用) | 🔧 扩展 | 4-port loopback (NUM_PORTS=4, **不**改下游拓扑, per H2) + `set_arb_latency()` + `stats_*` (per M4) |
| `include/tlm/gpu/dgpu_board_shell.{hh,cc}` | 🔧 扩展 | 新增 `simulation_mode_` 字段 + `init_timing_mode()` 分支 + `current_cycle_` 统一 advance |
| **`configs/dgpu_soc_timing_v1.json`** | 🆕 新建 | timing-mode SoC JSON 配置 (7-8 模块, **0 new connections**, 沿用 minimal_v1 拓扑) |
| `include/chstream_register.hh:46` | 🔧 扩展 | 宏体内追加 `registerObject<VramControllerTLM>` (per H1 + M10) |
| `test/test_dgpu_soc_timing_*.cc` (≥ 30 cases) | 🆕 新建 | 单元 + E2E 性能回归测试套件,新增 `[dgpu_soc_timing]` Catch2 标签 |
| **`docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md`** | 🆕 新建 | timing-mode SoC 范围 + functional/timing 共存边界 |
| `docs/designs/dgpu-soc/timing-mode.md` | 🆕 新建 | 第 1 层架构视图 (本 change 配套) |
| `docs/designs/dgpu-soc/README.md` | 🔧 扩展 | 添加 timing-mode 入口 + 仿真模式对照表 |
| `docs/designs/README.md` | 🔧 扩展 | dgpu-soc/ 子目录索引更新(并列 entry) |
| `AGENTS.md` (D-AXI 章节) | 🔧 追加 | 状态看板追加 timing-mode 行 |

### §2 不在范围内 (Non-Goals 详细版)

- **NG1**: minimal_v1 functional-mode 行为零修改 — 所有 `[minimal_dgpu_soc]` 测试零回归
- **NG2**: 多级页表 — v0.1 GMMU 仍一级,TLB miss 走 4-level walk 模拟(每级 N cycles)
- **NG3**: 23 ABI 签名 — 完全冻结;`simulation_mode` 是 DGpuBoard 内部字段,不暴露 ABI
- **NG4**: SDMA 既有 5 端口 — **零**新增端口 (per H2/H3);沿用 `submit_descriptor()` + backdoor path
- **NG5**: 多通道 HBM — 单 channel VramControllerTLM;D4 替换为 MemoryClusterTLM
- **NG6**: 第三方依赖 — 仅用既有 CppTLM 模块(CrossbarTLM/MemoryTLM/StreamAdapter)
- **NG7**: cycle-approximate RTL 精度 — 仿真建模值,仅用于**性能回归对比**
- **NG8**: 真实硬件校准 — cycles 数字不与 NVIDIA/AMD GPU SKU 对齐

### §3 关联变更

| 项 | 关系 |
|---|------|
| **D-AXI v1.4** (functional-mode 实施真相) | 本 change **不**破坏;沿用 vram_storage_ + 23 ABI 冻结 |
| **ADR-DGPU-05** (单一 VRAM 所有权) | 沿用 — timing-mode 与 functional-mode 共享同一 vram_storage_ |
| **ADR-DGPU-06** (AxiMemBundle 边界) | 沿用 — chip-internal 边界不变 |
| **ADR-DGPU-07** (演进 seam) | **触发 D3 提前** — `VramControllerTLM` 是 seam 占用 |
| **ADR-DGPU-10** (backing 命名约定) | 沿用 — MemoryTLM 仍用 `backing_view_` 注入 |
| **ADR-DGPU-11** (✅ 已签发 2027-02-09, Oracle 八轮 PASS) | timing-mode SoC 范围 + functional/timing 共存边界 (见 [docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md](../../docs/adr/ADR-DGPU-11-timing-mode-soc-scope.md)) |
| **Phase 6 AXI4Mapper** | pattern 借鉴 — v0.2 已删除 SDMA outstanding (per H3), 无调用 |
| **Phase 5 PcieAxiAdapter** | 直接复用 — PCIe TLP 时序已建模 (2 cyc TLP propagation) |
| **D2 PcieMemoryDevice** | 沿用 — BAR2 路由基础 |
| **GMMU MVP proposal** (D3/D4) | 关联 — 多级页表 + 多通道 HBM 是 v0.1 backlog,非本期实施 |

### §4 仿真模式切换机制

```json
{
  "name": "dgpu_soc_minimal_v1",    // functional-mode (✅ 已交付)
  "simulation_mode": "functional",  // 默认
  "modules": [...]
}
```

vs.

```json
{
  "name": "dgpu_soc_timing_v1",     // timing-mode (🆕 本提案)
  "simulation_mode": "timing",      // 🆕
  "modules": [
    // 6 functional 模块 + 2 新建 (CrossbarTLM + VramControllerTLM)
    // 3 扩展模块 (MemoryTLM/GmmuTLM/SDMA 加 cycle 参数)
  ]
}
```

`DGpuBoard::init()` 根据顶层 `simulation_mode` 字段:
- `"functional"` (默认) → 既有 functional-mode 路径(零 diff)
- `"timing"` → `init_timing_mode()` 分支,实例化 7-8 模块 (VramControllerTLM **或** MemoryTLM 二选一, per H2 互斥, per 六轮再确认)

### §5 关键架构决策

#### 决策 1: 不破坏 functional-mode 6 条 Invariants

minimal_v1 §8 6 条 Invariants(单一 backing 真源 / 指针稳定性 / BAR1 优先级 / GMMU PT_BASE 原子性 / translate_cb 签名 / 零时路径)**完全保留**。timing-mode 引入新 6 条 Invariants(共享 vram_storage_ / 同一 cycle 内 ordering / cycle advance 对齐 / backdoor 禁用 / BAR 路由优先级 / translate_cb 签名兼容),两套 Invariants **无冲突**。

#### 决策 2: v0.2 简化 — SDMA **不**新增 outstanding/AXI master port (per H3 Oracle 反馈)

SDMA 沿用既有 5-port + `set_vram_backdoor()` 路径 (functional-mode 流程),**不**新增 `axi_master_out`/`axi_master_in` 端口、**不**拆分 descriptor、**不**接 Crossbar 仲裁、**不**接 VramCtrl cascade (per H2)。Phase 6 AXI4Mapper (`include/framework/axi4_mapper.{hh,cc}`) 的 `OutstandingTracker` 设计模式作**参考**,**不**直接 include(后者绑定 `Axi4Bundle` awid/arid,与 `CacheReqBundle` 不兼容)。

SDMA cycle accounting 仅在 `translate_cb_` 回调中累加 GMMU TLB miss latency(`GmmuTLM::translate_timing` 返回 `lat_cycles`,SDMA `current_cycle_ += lat`),`last_descriptor_complete_cycle_` 成员记录最近完成 cycle 供外部测试断言。

#### 决策 3: VramControllerTLM 继承 MemoryTLM

不重写 MemoryTLM 行为,而是 `class VramControllerTLM : public MemoryTLM`,**复用** tick() + backing_view_ + stats_,**仅**添加:
- row buffer hit/miss tracking
- bandwidth 上限模拟(`bandwidth_gbps`)
- D3 seam(per ADR-DGPU-07)

#### 决策 4: cycle advance 统一由 DGpuBoard 推进

`MemoryTLM::current_cycle_` 由 `DGpuBoard::tick()` 统一 advance,避免跨模块 cycle 漂移(per TInv-3)。`GmmuTLM::current_cycle_` 同理。

#### 决策 5: backdoor 禁用于性能测试

timing-mode 测试**禁止**用 `DGpuBoard::backdoor_read/write` 验证数据(per TInv-4)。backdoor 仅用于 correctness sanity;性能回归必须用 cycle-approximate 路径 + `inflight_count()==0` 条件。

## Status

_(本节将在 change 实施 + Oracle 评审后追加)_