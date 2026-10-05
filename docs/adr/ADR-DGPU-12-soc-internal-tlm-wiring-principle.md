# ADR-DGPU-12: SoC 内部 wiring 原则 (SoC 内部 = TLM, SoC ↔ Board = 方法注入)

> **状态**: 📋 提案 (基于 D-AXI v1.4 N8 spec 决策 + 季度复审 2026-09-30 产物)
> **日期**: 2026-09-30
> **关联架构文档**: [../designs/dgpu-soc/soc-internal-tlm-design-target.md](../designs/dgpu-soc/soc-internal-tlm-design-target.md) (5 R-WIRE-* 形式化规则 + v2.0 roadmap), [../designs/dgpu-soc/d3-evolution-roadmap.md](../designs/dgpu-soc/d3-evolution-roadmap.md) (D3a-1 触发条件), [../designs/dgpu-soc/data-flow-and-topology-comparison.md](../designs/dgpu-soc/data-flow-and-topology-comparison.md) (3-way 代次对照)
> **关联 OpenSpec**: [cpptlm-dgpu-d3a1-display-fb-homogenization](../../openspec/changes/cpptlm-dgpu-d3a1-display-fb-homogenization/) (D3a-1 实施, 1-2d, **首个**符合 R-WIRE-* 全部规则的新增注入点), [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) (v1.8 当前, spec §6 行 1 遗留议题 D3a 收编计划), [cpptlm-dgpu-soc-timing-mvp](../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/) (timing-mode 沿用)
> **配套 ADR**: [ADR-DGPU-05](ADR-DGPU-05-vram-storage-ownership.md) (单一 VRAM 真源 vram_storage_, **沿用**), [ADR-DGPU-06](ADR-DGPU-06-axi-mem-bundle-boundary.md) (AxiMemBundle vs PcieTlpBundle wire-format 边界, **沿用**), [ADR-DGPU-07](ADR-DGPU-07-minimal-soc-evolution-seam.md) (D3-D5 演进 seam `handle_slave_port ↔ backing_ptr_`, **沿用并扩展**), [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md) (23 ABI 冻结, **不动摇**), [ADR-DGPU-10](ADR-DGPU-10-backing-naming-convention.md) (backing owner/injected 两级命名, **沿用**)

---

## 1. 背景

### 1.1 问题

D-AXI v1.4 (commit `429327d`) 完成时, SoC 内部 wiring 存在两类不同的机制:
- **TLM StreamAdapter 连接** (JSON `connections` 表达): sdma → pcie_memory, gmmu → pcie_memory (functional-mode + timing-mode 共 2 条)
- **方法注入** (C++ code 层 `setXxx()` 调用): `ep->set_memory_device(pcie_mem)`, `sdma->set_translate_cb(lambda)`, `sdma->set_vram_backdoor(ptr, size)`, `pcie_mem->set_backing_view(ptr, size)` 等

**核心问题**: SoC 内部模块 (`pcie_ep`) 与 SoC 内部模块 (`pcie_memory`) 之间用方法注入而非 TLM 连接, 这导致:
1. **配置层与代码层不一致**: JSON 配置只表达 2 条 TLM 连接, 但 C++ 代码隐藏 4 处方法注入
2. **新 SoC 变体复用困难**: 如果将来有 GPU 计算路径接入 (D3b-3 GMMU PoC + D3b-4 SM), 这些模块都通过方法注入组装, 配置层无法表达完整拓扑
3. **测试隔离性差**: 方法注入在 init 期由 DGpuBoard 单点完成, 测试无法替换中间模块 (mock testing 困难)

### 1.2 Oracle N8 spec 决策 (当前 baseline)

D-AXI v1.4 spec §B12 + Requirement "PcieEndpointIP 持有 PcieMemoryDevice raw pointer (N8 修订: 不再双 tick)" 显式声明:

> **N8 修订**: `tick()` 不再调 `memory_device_->tick()` (避免与 soc::tick 递归双 tick)

**为什么 N8 选择方法注入而非 TLM 连接**:
- TLM StreamAdapter 连接会**触发 tick 链**——`EP.tick()` 调 `pcie_memory.tick()` → `cycle_counter_` 2N 错误 (per `pcie_endpoint_ip.cc:357-360` 注释)
- 当时 TLM 2.0 `b_transport` 接口未被使用 (无 framework 约束), 方法注入是**最小改动**方案
- 这是 Oracle + Metis 多轮评审的**共识决策**, 不是 ad-hoc

**当前 1 处不合规点 (per R-WIRE-4)**:
| 位置 | 类型 | 注入内容 | 违反规则 | 当前理由 |
|------|------|----------|---------|----------|
| `pcie_endpoint_ip.hh:190` | `set_memory_device()` | EP ↔ pcie_memory raw pointer | **R-WIRE-4** | N8 避免双 tick |

### 1.3 现状约束

- **23 ABI 签名字节级冻结** (per ADR-DGPU-08 §D5): 任何 wiring 改动**不**改变 ABI 含义
- **D-AXI v1.4 已交付 79046 assertions 全绿**: 包括 `[minimal_dgpu_soc]` 12 cases / 69 assertions + `[pcie-memory]` 24 cases / 69 assertions + `[abi][minimal_dgpu_soc]` 10 cases / 28 assertions
- **5 消费者共享 vram_storage_** (per ADR-DGPU-05): backdoor / BAR1 fast-path / BAR2 via pcie_memory / MemoryTLM backing / SDMA-GMMU legacy ptr
- **tick() 链路 SHALL NOT 成环** (per `pcie_endpoint_ip.cc:357-360`): 当前 ModuleFactory::startAllTicks() 统一调度, 子模块各自 tick
- **D3-D5 演进 seam** (`handle_slave_port ↔ backing_ptr_`, per ADR-DGPU-07 §7): **必须保留** 作为 D3b-1 VramControllerTLM 插入点

---

## 2. 决策

### 2.1 决策 1: 5 R-WIRE-* wiring 原则正式化

| Rule ID | 触发 | SHALL/MUST 行为 |
|---------|------|----------------|
| **R-WIRE-1** | SoC 内部模块 A → SoC 内部模块 B 数据流 | **SHALL** 用 TLM StreamAdapter 连接 (JSON `connections` 表达) |
| **R-WIRE-2** | SoC 内部模块 ↔ board-level backing (`vram_storage_`) | **MUST** 用方法注入 (`setXxx()`), 因 board-level 不是 TLM 模块 |
| **R-WIRE-3** | SoC 内部模块 ↔ board-level callback (如 translate_cb) | **MAY** 用方法注入 (lambda capture), 性能优于 TLM port |
| **R-WIRE-4** | SoC 内部方法注入 (如 `ep->set_memory_device(pcie_mem)`) | **DEPRECATED** in v2.0, 应替换为 TLM `b_transport` port |
| **R-WIRE-5** | tick() 链路 (含 ChStreamModuleBase) | **SHALL NOT** 形成环 (避免 `cycle_counter_` 2N 错误) |

**为什么是这 5 条原则**:
- **R-WIRE-1** = 数据平面声明 (TLM 事务流)
- **R-WIRE-2** = 控制平面 + backing 共享 (因 board-level 真源不是 TLM 模块)
- **R-WIRE-3** = callback 灵活性 (lambda capture 比 TLM port 简单)
- **R-WIRE-4** = 演进方向 (v2.0 全 TLM 化目标)
- **R-WIRE-5** = tick 调度不变量 (per ChStreamModuleBase 设计)

### 2.2 决策 2: 当前 1 处违反 (ep↔pcie_memory) 保留为 N8 spec baseline

**ep↔pcie_memory 的方法注入 (per `pcie_endpoint_ip.hh:190 set_memory_device`) 暂时保留**, 理由:
1. Oracle N8 spec 决策的**故意妥协** (避免双 tick)
2. D-AXI v1.4 已交付, 0 regressions
3. 替换需要新增 TLM `b_transport` / `transport_dbg` port, 涉及:
   - PcieEndpointIP 新增 API (冻结面审计 per ADR-DGPU-08)
   - PcieMemoryDevice 新增 API
   - JSON connection 新增 (`{"src": "pcie_ep.X", "dst": "pcie_memory.Y", "transport_type": "b_transport"}`)
   - 51 cases 回归 + TDD 5 步分批实施
   - Oracle 评审 (冻结面审计)

**保留期间**:
- 3 个 config (`dgpu_soc_minimal_v1.json` / `dgpu_soc_timing_v1.json` / `dgpu_board_v1.json`) 的 description 段已显式声明 R-WIRE-* 状态
- `dgpu_soc_minimal_v1.json:8` / `dgpu_soc_timing_v1.json:8` 引用本文档 + spec N8
- `dgpu_board_v1.json:3` 显式声明 out-of-scope (早期代次)

### 2.3 决策 3: D-AXI v2.0 启动条件

D-AXI v2.0 (SoC 内部全 TLM 化) **不是本季度目标**, 仅在以下 AND 条件全部满足时启动:

1. **D-AXI v1.6 (D3a + D3b) 实施完成 + Oracle 评审通过**
2. **v1.7+ 累积 ≥ 3 处 wiring 改进** (说明 wiring 设计活跃)
3. **性能 benchmark 显示 TLM 化后 BAR2 fast-path 损耗 < 5%** (per `data-flow-and-topology-comparison.md` §4 时序分析)
4. **冻结面审计通过** (per ADR-DGPU-08 §D5): 加新 TLM socket 不算新增 ABI, 或新增 ADR-DGPU-12a 解释
5. **Oracle 评审同意双 tick 解决方案**: PcieEndpointIP 新增 `b_transport` 或 `transport_dbg` 接口, 不传播 tick

### 2.4 决策 4: D3a-1 作为首个符合 R-WIRE-* 全部规则的实施

D3a-1 (Display FB 同构, 1-2d) 是**首个**符合 R-WIRE-* 全部规则的新增注入点:
- **R-WIRE-2 合规**: DGpuBoard 持有 `display_fb_storage_` (新增 backing, 类比 `vram_storage_`), PcieDisplayDevice 通过 `set_backing_store(ptr, size)` 接收 (per ADR-DGPU-10 owner/injected 两级)
- **R-WIRE-4 合规**: D3a-1 **不**引入新的 SoC 内部方法注入, 仅扩展 R-WIRE-2 模式 (SoC↔Board backing 共享)
- **冻结面保护**: `pcie_display_device.hh` 零修改 (per ADR-DGPU-07 演进 seam)

D3a-1 的 OpenSpec change `cpptlm-dgpu-d3a1-display-fb-homogenization` 将作为本 ADR 的**首个验证用例**。

---

## 3. 实施

### 3.1 已完成 (季度复审 2026-09-30)

| # | 动作 | 文档/文件 | 状态 |
|---|------|----------|------|
| 1 | 起草设计目标声明 | [../designs/dgpu-soc/soc-internal-tlm-design-target.md](../designs/dgpu-soc/soc-internal-tlm-design-target.md) | ✅ v0.2 DRAFT |
| 2 | 起草 D3 演进 roadmap | [../designs/dgpu-soc/d3-evolution-roadmap.md](../designs/dgpu-soc/d3-evolution-roadmap.md) | ✅ v0.1 DRAFT |
| 3 | 起草数据流 + 拓扑 3-way 对比 | [../designs/dgpu-soc/data-flow-and-topology-comparison.md](../designs/dgpu-soc/data-flow-and-topology-comparison.md) | ✅ v1.0 |
| 4 | 3 个 config description 引用 R-WIRE-* | `configs/dgpu_soc_minimal_v1.json` + `dgpu_soc_timing_v1.json` + `dgpu_board_v1.json` | ✅ |
| 5 | AGENTS.md §仿真框架 vs 应用设计 + KPI 段 | `AGENTS.md` | ✅ |

### 3.2 D3a-1 实施 (本 ADR 首个验证用例)

详见 [OpenSpec change: cpptlm-dgpu-d3a1-display-fb-homogenization](../../openspec/changes/cpptlm-dgpu-d3a1-display-fb-homogenization/).

**关键路径**:
1. DGpuBoard 新增 `display_fb_storage_` (类比 `vram_storage_`)
2. PcieDisplayDevice 新增 `set_backing_store(ptr, size)` (per ADR-DGPU-10 owner/injected 两级)
3. DGpuBoard::bind_display_backings() 在 init 期注入
4. JSON config 可选字段: `display_fb_size_bytes` (顶层 override `pcie_ep.params.bar_sizes[1]` 派生)
5. 测试: `[pcie][pcie-display]` 标签下新增 `display_fb_owner_is_board_` 测试家族

**冻结面影响**: 0 (PcieDisplayDevice 新增 `set_backing_store` 不破坏现有 API, 仅扩展)

### 3.3 D-AXI v2.0 实施 (远期, 触发后启动)

**步骤**:
1. Oracle 评审 PcieEndpointIP 新增 `b_transport` 接口设计
2. ADR-DGPU-08 审计: 加新 TLM socket 是否算新增 ABI?
3. PcieEndpointIP + PcieMemoryDevice 新增 TLM `b_transport` port
4. JSON `connections` 新增 `pcie_ep → pcie_memory` (transport_type=b_transport)
5. 删除 `set_memory_device()` (R-WIRE-4 违反解除)
6. TDD 5 步分批: 14 + 37 cases 回归
7. 性能 benchmark: BAR2 fast-path 损耗 < 5%

**风险**:
- 冻结面审计失败 → 加新 TLM socket 视为新增 ABI, 需 ADR-DGPU-08 例外或修订
- 双 tick 复现 → `b_transport` 设计需严格 no-tick-propagation
- 性能损耗超阈值 → 退而求其次保留方法注入, 仅文档化 R-WIRE-4 现状

---

## 4. 不动摇原则 (Invariants)

| # | 原则 | 来源 |
|---|------|------|
| I-1 | **23 ABI 签名字节级冻结 + 0 新增** | ADR-DGPU-08 §D5 + Oracle v2.0.2 P0-3e |
| I-2 | **单一 VRAM 真源归 DGpuBoard** | ADR-DGPU-05 |
| I-3 | **chip-internal vs board-level wire-format 严格分离** | ADR-DGPU-06 |
| I-4 | **D3-D5 演进 seam (`handle_slave_port ↔ backing_ptr_`) 必须保留** | ADR-DGPU-07 |
| I-5 | **tick() 链路 SHALL NOT 成环** | per `pcie_endpoint_ip.cc:357-360` |
| I-6 | **owner/injected 两级 backing 命名约定** | ADR-DGPU-10 |
| I-7 | **本 ADR R-WIRE-* 5 条原则** | 本 ADR §2.1 |
| I-8 | **当前 1 处违反 (ep↔pcie_memory) 保留为 N8 baseline** | 本 ADR §2.2 |

---

## 5. 演进路径 (Roadmap)

```
Phase 9 (当前)        Phase 9+ (Q4 2026)        Phase 10+ (2027 H1)        Phase 10+ (2027 H2)
───────────────    ──────────────────────    ───────────────────────    ─────────────────────
│                  │                          │                          │
▼                  ▼                          ▼                          ▼
┌─────────┐    ┌────────────────┐    ┌────────────────┐    ┌────────────────┐
│ ADR-12  │───▶│ D3a-1 OpenSpec │───▶│ D3b-1 OpenSpec │───▶│ D-AXI v2.0     │
│ 签发    │    │ Display FB     │    │ VramController│    │ SoC 内部全     │
│ 📋 提案  │    │ 同构           │    │ TLM 插入      │    │ TLM 化         │
│         │    │ 1-2d          │    │ 5-7d          │    │ 远期目标       │
│         │    │ ✅ R-WIRE-2    │    │ ✅ R-WIRE-2    │    │ ❌ 解除 R-WIRE-4│
│         │    │ 全新合规       │    │ 沿用 seam     │    │ 需 Oracle 评审 │
└─────────┘    └────────────────┘    └────────────────┘    └────────────────┘
   │                  │                          │                          │
   ▼                  ▼                          ▼                          ▼
 设计目标声明    D3a-1 spec/tests        D3b-1 spec/tests         v2.0 spec/tests
 + 3 config        ✅ 合规                ✅ 合规                  待 Oracle 触发
   reference
```

**关键时间点**:
- **2026-09-30**: ADR-DGPU-12 起草 (本 ADR), 季度复审产物
- **2026-Q4 (W40-W42)**: D3a-1 实施 (per [d3-evolution-roadmap.md §2.1](../designs/dgpu-soc/d3-evolution-roadmap.md))
- **2027-Q1**: D3b-1 启动 (per [d3-evolution-roadmap.md §2.2](../designs/dgpu-soc/d3-evolution-roadmap.md))
- **2027-Q2+**: D-AXI v2.0 启动条件评估 (per 本 ADR §2.3 5 条 AND 条件)

---

## 6. 关联文档

### 6.1 架构文档 (Layer 1)

| 文档 | 关系 |
|------|------|
| [../designs/dgpu-soc/soc-internal-tlm-design-target.md](../designs/dgpu-soc/soc-internal-tlm-design-target.md) | **本 ADR 的设计目标声明源头** (5 R-WIRE-* 规则) |
| [../designs/dgpu-soc/d3-evolution-roadmap.md](../designs/dgpu-soc/d3-evolution-roadmap.md) | D3a/D3b 实施路径 |
| [../designs/dgpu-soc/data-flow-and-topology-comparison.md](../designs/dgpu-soc/data-flow-and-topology-comparison.md) | 3-way 代次对照 + §4 时序分析 (D-AXI v2.0 性能 benchmark 基础) |
| [../designs/dgpu-soc/architecture.md](../designs/dgpu-soc/architecture.md) | functional-mode minimal_v1 业务架构视图 |
| [../designs/dgpu-soc/timing-mode.md](../designs/dgpu-soc/timing-mode.md) | timing-mode SoC 业务架构视图 |
| [../designs/dgpu-board/architecture.md](../designs/dgpu-board/architecture.md) | DGpuBoard 5 层 + 5 组件 |

### 6.2 实施笔记

| 文档 | 关系 |
|------|------|
| [../pcie/driver-visible-minimal-soc.md](../pcie/driver-visible-minimal-soc.md) | D-AXI v1.4 §6-7 遗留议题 + D3 演进 seam 列表 |
| [../pcie/display-device-mvp.md](../pcie/display-device-mvp.md) | D1 PcieDisplayDevice MVP (v1.1 实施完成) |

### 6.3 OpenSpec changes (Layer 3)

| 文档 | 关系 |
|------|------|
| [cpptlm-dgpu-d3a1-display-fb-homogenization](../../openspec/changes/cpptlm-dgpu-d3a1-display-fb-homogenization/) | **本 ADR 首个验证用例** (D3a-1 实施) |
| [cpptlm-driver-visible-minimal-soc](../../openspec/changes/cpptlm-driver-visible-minimal-soc/) | v1.8 当前, spec §B12 N8 决策 |
| [cpptlm-dgpu-soc-timing-mvp](../../openspec/changes/cpptlm-dgpu-soc-timing-mvp/) | timing-mode 沿用 R-WIRE-* |

### 6.4 实施跟踪 (Layer 4)

| 文档 | 关系 |
|------|------|
| [../superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md](../superpowers/plans/2026-09-27-cpptlm-driver-visible-minimal-soc-t1.md) | D-AXI v1.4 实施跟踪 (D3a 进度) |

### 6.5 配套 ADR (Layer 2)

| ADR | 关系 |
|-----|------|
| [ADR-DGPU-05](ADR-DGPU-05-vram-storage-ownership.md) | 单一 VRAM 真源 (沿用) |
| [ADR-DGPU-06](ADR-DGPU-06-axi-mem-bundle-boundary.md) | wire-format 边界 (沿用) |
| [ADR-DGPU-07](ADR-DGPU-07-minimal-soc-evolution-seam.md) | D3-D5 演进 seam (沿用 + 扩展) |
| [ADR-DGPU-08](ADR-DGPU-08-abi-freeze-policy.md) | 23 ABI 冻结 (不动摇) |
| [ADR-DGPU-09](ADR-DGPU-09-driver-visible-minimal-soc-scope.md) | Driver-Visible 范围 (沿用) |
| [ADR-DGPU-10](ADR-DGPU-10-backing-naming-convention.md) | backing 命名 (沿用, D3a-1 复用) |
| [ADR-DGPU-11](ADR-DGPU-11-timing-mode-soc-scope.md) | timing-mode 范围 (沿用) |

---

## 7. 维护记录 (Maintenance Log)

| 日期 | 事件 | 操作人 |
|------|------|--------|
| 2026-09-30 | 📋 提案起草, 季度复审 2026-09-30 产物 | CppTLM Team |
| _TBD_ | Oracle 评审 → ✅ 已签发 | 待启动 |
| _TBD_ | D3a-1 实施完成 → §3.2 验证用例通过 | 待 D3a-1 启动 |
| _TBD_ | D-AXI v2.0 启动条件满足 → §2.3 评估 | 待 v1.6 完成后 |

---

**维护**: CppTLM Team · **版本**: v0.1 DRAFT (2026-09-30) · **状态**: 📋 提案, 待 Oracle 评审
**评审入口**: §2.3 D-AXI v2.0 启动条件 + §3.2 D3a-1 实施


### 2026-10-05 — D3a-2 实施完成 (D3a-1 Path A, supersede 原 D3a-1 浅层方案)

**实施 change**: [cpptlm-dgpu-d3a1-display-injection-refactor](../../openspec/changes/cpptlm-dgpu-d3a1-display-injection-refactor/) (D3a-2, 已实施待归档)

**D3a-2 vs §3.2 D3a-1 设计差异**:

| 项 | §3.2 D3a-1 设计 (Path 浅层) | D3a-2 实施 (Path A 彻底) |
|----|---------------------------|--------------------------|
| PcieDisplayDevice backing | 仅加旁路 (保留 framebuffer_) | **删自有 framebuffer_ + kFbSize + ensure_framebuffer_allocated** |
| PcieEndpointIP 持有模式 | 不动 (unique_ptr 持有) | 改 raw ptr + set_display_device() |
| soc 内访问方式 | ChStreamModule 注册 (`getInternalInstance`) | value 对象 + `display_device()` accessor (PcieDisplayDevice 是纯类, 非 SimObject) |
| 命名约定 | `set_backing_store()` (旧名, 违反 ADR-DGPU-10) | `set_backing_view()` (per ADR-DGPU-10 §2.2 命名) |
| 冻结面影响 | 0 (仅扩展) | 例外 (per ADR-DGPU-08 §Status Update v1.1, 删除私有字段 + 改 backdoor 行为) |

**实施结果**:
- 工作区 12 文件 M + 2 新增 (test/test_display_device_injection.cc + ADR-DGPU-12)
- `[d3a2]` 7 cases / 38 assertions PASS
- 同步更新 `[pcie][display][basic]` 13 cases + `[pcie][display][e2e]` 8 cases (零回归)
- `[pcie-memory]` 24 cases 0 影响 (D2 路径独立)
- `[abi][minimal_dgpu_soc]` 12 cases 全绿 (ABI 零修改)
- `[minimal_dgpu_soc]` 12 cases 全绿
- 23 ABI 零 diff
- docs_sync_check --strict PASS
- openspec validate --strict 3/3 PASS

**R-WIRE-* 状态修正**:

| Rule | D3a-1 设计 | D3a-2 实施 | 评估 |
|------|-----------|-----------|------|
| R-WIRE-1 | ✅ | ✅ (不变) | SoC 内部数据流 |
| R-WIRE-2 | ✅ | ✅ | DGpuBoard 持 `display_fb_storage_`, PcieDisplayDevice 经 `set_backing_view()` 注入 |
| R-WIRE-3 | ✅ | ✅ (不变) | callback 路径 |
| R-WIRE-4 | ❌ (D3a-1 不引入新方法注入) | ❌ (D3a-2 同不引入新方法注入, 仅扩展 R-WIRE-2 模式) | N8 baseline 维持 |
| R-WIRE-5 | ✅ | ✅ | tick 无环 (display_device tick 由 EP::tick() 单调用, 不递归) |

**冻结面例外增补**: [ADR-DGPU-08 §Status Update v1.1](../../../../docs/adr/ADR-DGPU-08-abi-freeze-policy.md) (新例外: pcie_display_device.hh 删私有字段 + 改 backdoor 行为; 公 API 零签名 diff + 23 ABI 零 diff; 先例: D-AXI v1.4 B7 同模式)

**归档时序**:
- 2026-10-05: 实施完成 (working tree)
- 2026-10-05: D3a-1 已 archive (KPI 从 3/3 → 2/3)
- 待 archive: D3a-2 (本 change, Phase 5 在 T5.1)

