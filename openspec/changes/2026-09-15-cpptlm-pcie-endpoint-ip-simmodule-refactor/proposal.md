# Proposal: cpptlm-pcie-endpoint-ip-simmodule-refactor — PcieEndpointIP 升级为 SimModule（内部子模块分层组合）

> **状态**: 🔄 Proposed — 2026-09-15
> **工期**: 1.5 周（Phase 1 + Phase 2；Phase 3 AxiAdapter 拆分独立提案；R4 决策 C 后实际 ~2 工作日）
> **优先级**: P1
> **前置依赖**: `cpptlm-dgpu-pcie-ip-integration`（Phase 8 ✅ ship, commit `429327d`）+ `2027-02-09-cpptlm-pcie-endpoint-ip-json-config`（archive, 2027-02-09）
> **Oracle 复评**: ✅ **APPROVE_WITH_CONDITIONS**（5 项条件已落入本提案与 design.md）

---

## Why

`PcieEndpointIP`（Phase 4 SR-IOV 17 端口：1 PF + 16 VF）当前是 **`ChStreamModuleBase` 单类 + 4 个静态注册表外挂子模块** 的混合体（`include/tlm/pcie/pcie_endpoint_ip.hh:50`）：

| 子模块 | 连接机制 | 代码证据 |
|--------|---------|---------|
| `PcieLinkLayer` | 静态 `for_endpoint(name)` 查表 + 同步指针调用 | `pcie_link_layer_tlm.hh:82-308` + `pcie_endpoint_ip.cc:371-373` |
| `PciePhyDigitalCtrl` | 持 `PcieLinkLayer*` + `PcieBypassMux*` 双裸指针 | `pcie_phy_digital_ctrl_tlm.hh:212-213` |
| `PcieBypassMux` | 构造即持 `PcieLinkLayer*`；10 步清理直调 LL 成员 | `pcie_bypass_mux.hh:63, 119, 132-137` |
| `PcieAxiAdapter` | 静态注册表 + EP::tick 直接操作其 Axi4StreamAdapter | `pcie_axi_adapter_tlm.hh:40-129` + `pcie_endpoint_ip.cc:255-364` |
| `PcieSriovVfPool` | EP 的**值成员**（`pool_`），被 EP::tick 每拍直接访问 | `pcie_endpoint_ip.hh:172` + `cc:280, 296, 336, 340` |

**问题**：

1. **无法参与 SimModule 多层嵌套声明式组合**：ApuSoC（顶层 cluster）通过 `internal_factory->instantiateAll(wrap)` 持有 ChStream 子模块（CoherentXBarTLM）+ SimModule 子模块（CpuCluster/GpuCluster），整树 JSON 编排（`src/tlm/cluster/apu_soc.cc:47-82`）。PcieEndpointIP 顶层虽注册为 ChStream，但内部 4 子模块不在工厂视野内，只能硬编码在 `attach_composition`。
2. **静态注册表语义模糊**：4 个独立 `attach_to_endpoint(name)` 静态表（每个文件函数内 static `unique_ptr<map>`）让生命周期归各文件而非统一工厂；冻结的 `PcieEndpointTLM` 也共用这套注册表（`src/tlm/gpu/pcie_endpoint_tlm.cc`），互相影响。
3. **PHY ↔ LL 强耦合紧贴 EP 边界**：PHY 直接调 `link_layer_->trigger_rate_switch()`（`phy_digital_ctrl_tlm.hh:136-137`），Mux 调 `link_layer_->clear_retry_buffer()` 等（`pcie_bypass_mux.hh:132-137`）—— 这些是**同周期 0-cycle 原子调用**，Bundle 化必然破坏 INV-B ASPM exit latency。

## What Changes

### 方案 C 核心：EP 升级为 SimModule + 强耦合子模块合并为单一 composite ChStream child

```
现状 (ChStreamModuleBase + 静态注册表):
┌────────────────────────────────────────────────────┐
│ PcieEndpointIP : ChStreamModuleBase               │
│   ├─ 17 TLP 端口 (req_in/resp_out)                 │
│   ├─ pool_ : PcieSriovVfPool (值成员)             │
│   └─ attach_composition 硬编码 attach 4 子模块    │
└────────────────────────────────────────────────────┘

目标 (SimModule + composite child):
┌────────────────────────────────────────────────────┐
│ PcieEndpointIP : SimModule                        │
│   ├─ internal_factory (ModuleFactory 实例)       │
│   │   ├─ ChStream child: PcieLinkPhyMuxTLM        │
│   │   │     ├─ 持有 PcieLinkLayer / Phy / Mux  │
│   │   │     ├─ 17 TLP 端口 (multiport adapter)   │
│   │   │     └─ 成员声明序 LL→PHY→Mux            │
│   │   └─ pool_ : PcieSriovVfPool (仍为值成员)   │
│   ├─ inputs/outputs 配置 (17 端口暴露)           │
│   └─ tick() override 显式定序                    │
└────────────────────────────────────────────────────┘
```

### §1 Phase 1 — 引入 composite，EP 基类不动

- 新建 `include/tlm/pcie/pcie_link_phy_mux_tlm.{hh,cc}` — `class PcieLinkPhyMuxTLM : public ChStreamModuleBase`
  - 私有成员：`PcieLinkLayer link_`、`PciePhyDigitalCtrl phy_`、`PcieBypassMux mux_`
  - 构造函数按声明顺序构造 LL→PHY→Mux，构造后立即 `phy_.link_layer(&link_)` + `phy_.set_link_up(true)` + `mux_.set_link_layer(&link_)`（封装 `pcie_endpoint_ip.cc:125-141` 的组合逻辑）
  - `tick()` 显式定序：`phy_.tick() → link_.tick() → mux_.tick()`（**EP::tick 当前不调 PHY**，Phase 1 引入 PHY tick 时定义顺序）
- EP `attach_composition` 改为构造 composite 子模块（`composite_(name+"_lpm", event_queue)`）；`link_layer()/phy()/bypass_mux()` 访问器转发到 `composite_.link_/phy_/mux_`
- **静态注册表保留为 shim**：`PcieLinkLayer::for_endpoint(name)` 等继续可用，但内部委托到 composite 成员；保证 14+ 测试文件 + 冻结 PcieEndpointTLM 零修改通过
- Phase 1 不动 EP 基类（仍是 `ChStreamModuleBase`），不改 REGISTER_CHSTREAM

### §2 Phase 2 — EP 基类切换 `ChStreamModuleBase` → `SimModule`

- `pcie_endpoint_ip.hh:50` 改继承为 `: public SimModule`
- `chstream_register.hh:82` 删除 `registerObject<tlm::pcie::PcieEndpointIP>("PcieEndpointIP")`（**避免双注册残留**——module registry 优先查找，`module_factory.cc:266-269`，但 stale object 条目会让 `getRegisteredObjectTypes()` 说谎）
- `include/modules_cluster.hh` 新增 `REGISTER_MODULE(PcieEndpointIP)`
- EP 实现 `simulate_instantiate(const json& cfg)` override：构造 composite 子模块 + 注册到 internal_factory
- EP 实现 `tick()` override 显式定序：**AXI slave 处理 → composite 子模块 tick → 17 adapters tick**（保留 `pcie_endpoint_ip.cc:248-374` 当前语义，**禁用 SimModule 默认 unordered_map 迭代** `sim_module.hh:166`）
- `chstream_register.hh:130-131` 的 `registerMultiPortAdapter<PcieEndpointIP, ..., 17>` 改为对应 `PcieLinkPhyMuxTLM, ..., 17`（**新 composite 接管 17 端口**；composite 通过 `internal_factory->instantiateAll(wrap)` 在 EP 内部工厂内构造,Step 7 mirror 由**内部工厂**执行,composite 17 端口经 `EP::getInternalOutputPort("pcie_ep_lpm.resp_out[i]")` 程序化可达）

> **R4 决策落地（Oracle 复评 C 选项）**：本 change **不引入**外层 JSON 声明式 `pcie_ep.pf0` 接线。当前 `connection_resolver.cc:47-98` 对外层 `ep.<label>` → 内部 `<composite_name>.<port>` 的两层下钻**无递归 `getInternalOutputPort` 解析支持**——composite 住在 EP 内部工厂,不在外层 `object_instances`,外层连接解析会静默丢弃。Phase 8 实际数据路径是 429327d 的 `HostBypassTLM/RC::tick()` 程序化桥接,外层 TLP 接线无消费者。`ep.pf0`/`ep.vfN` 暴露留待 Phase 3 (AxiAdapter 拆分) 有真实消费者时与 AXI 端口一并设计。

### §3 严格 Out of Scope（Oracle 条件）

| 项 | 排除理由 |
|----|---------|
| **Phase 3：AxiAdapter 拆分为独立 ChStream child** | AxiAdapter 当前非 SimObject（`pcie_axi_adapter_tlm.hh:40` 无基类），且 EP::tick `cc:248-364` 的 AXI slave 业务逻辑住在 EP 里而非 adapter；先迁逻辑再建模块是独立 Short 提案 |
| **VfPool 拆分为 ChStream child** | `pool_` 是 EP 值成员 + EP::tick 数据路径每拍访问（`cc:280,296,336,340`），拆分为模块只会让所有热路径调用穿过跨模块指针，零收益纯成本（Oracle 明确驳回） |
| **PcieEndpointTLM（4 端口冻结 legacy）触碰** | 23 ABI 冻结头 `include/tlm/gpu/pcie_endpoint_tlm.h`；本 change 不引入任何对该文件的修改 |
| **23 ABI header `include/abi/cpptlm_emulator.h` 触碰** | mmio_write ABI 走 board shell (`cpptlm_emulator.cc:245` `emu->board->mmio_write`)，不经 EP 类布局，**EP 基类切换 ABI 安全** |
| **LL↔PHY↔Mux 内部信号 Bundle 化** | rate switch / 10-step cleanup / INV-B ASPM exit 是 0-cycle 同步调用（`pcie_phy_digital_ctrl_tlm.hh:136-137` `link_layer_->trigger_rate_switch`），Bundle 化必然引入 ≥1 cycle 延迟 |
| **CHSTREAM 注册表全部取消** | Phase 1 保留为兼容 shim；Phase 3+ 再评估 |
| **JSON 配置字段语义变更** | `params.{axi_adapter,link_layer,phy_digital,sr_iov,transaction_layer}` 7 类键保持原 `attach_composition` 消费逻辑不变 |
| **外层 JSON 声明式 `pcie_ep.pf0`/`pcie_ep.vfN` 接线** | Oracle R4 决策：当前 `connection_resolver.cc` 不支持两层下钻；composite 17 端口经 `getInternalOutputPort` 程序化可达已足够,Phase 3 再统一暴露机制 |

## Impact

### 修改源文件
- `include/tlm/pcie/pcie_endpoint_ip.hh`（基类切换 + 内部成员结构；Phase 2 移除 `composite_` 成员,改为通过 `internal_factory` 访问）
- `src/tlm/pcie/pcie_endpoint_ip.cc`（`on_config_loaded` → `simulate_instantiate`，`tick()` override 显式定序）
- `include/chstream_register.hh`（删 line 82 + line 130-131 改为 `PcieLinkPhyMuxTLM` 注册）
- 4 个子模块 `.cc` 的 `for_endpoint` 实现改造（composite-first + legacy-fallback,`pcie_link_layer_tlm.cc` / `pcie_phy_digital_ctrl_tlm.cc` / `pcie_bypass_mux.cc` / `pcie_axi_adapter_tlm.cc` 仅 .cc 修改,.hh 布局冻结）

### 新增源文件
- `include/tlm/pcie/pcie_link_phy_mux_tlm.hh`
- `src/tlm/pcie/pcie_link_phy_mux_tlm.cc`
- `test/test_pcie_endpoint_ip_simmodule_refactor.cc`（Catch2，6+ TEST_CASE）

### 修改示例 / 配置
- `examples/dgpu_soc_with_pcie_ip.json` **保留不变**（R4 决策 C：Phase 2 不引入外层 `pcie_ep.pf0` 暴露,现有 `pcie_ep.axi_slave_in`/`pcie_ep.axi_master_out` 接线行为由 Phase 2 task 2.3 E2E gate 监控;若发现丢弃导致 E2E 失败,回退方案为 AxiAdapter 路径桥接加固,非本 change 范围）

### 新增 OpenSpec 制品
- `openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/{proposal.md, design.md, tasks.md}`
- `openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/specs/cpptlm-pcie-endpoint-ip-composition/spec.md`

### 不改（强约束）
- `include/tlm/gpu/pcie_endpoint_tlm.h`（23 ABI 冻结头）
- `include/abi/cpptlm_emulator.h`（23 ABI 冻结头）
- `include/tlm/pcie/pcie_phy_digital_ctrl_tlm.hh` / `pcie_bypass_mux.hh` / `pcie_axi_adapter_tlm.hh`（子模块 .h 布局冻结,仅在 composite 内组合；.cc 实现可改）
- 4 个子模块的静态注册表 API（保留为 shim，仅内部委托实现变化）

### Impact 例外（精确定义）
- **`include/tlm/pcie/pcie_link_layer_tlm.hh`**：允许**仅新增** public setter 方法（`set_fc_capacity`, `set_retry_buffer_size`, `set_link_error_injection_enabled`）以支持 Phase 1 composite 注入 `link_layer.*` JSON 字段。**禁止**：修改现有 public 成员签名/类型、virtual 函数 override、`protected`/`private` 数据成员、ABI 暴露成员。**理由**：composite 在构造后逐字段写入需 setter；`FcTokenBucket::capacity_` 仅构造期可设（`pcie_flow_control_token_bucket.hh:100`，`update_fc` 按 PCIe 语义只改 credit 不改 capacity）,不增加 `set_fc_capacity` 则 `fc_token_bucket_capacity` / `retry_buffer_size` / `link_error_injection_enabled` 字段被静默 drop,违反 "7 类 JSON 消费语义保持不变" 承诺。

### 下游 / 跨 change 边界
- 与 `2027-02-09-cpptlm-pcie-endpoint-ip-json-config`（archive）边界 — 本 change 不动 JSON 消费语义；`attach_composition` → `simulate_instantiate` 是同语义接口切换
- 与未来 `cpptlm-pcie-endpoint-ip-axiadapter-split`（计划中）边界 — Phase 3 独立 change
- 与 `2026-09-10-cpptlm-stage-1-4-2-1`（Proposed v1.1，BAR window）边界 — 不重叠（BAR 窗口归对方）

## Oracle 复评

### Verdict
**APPROVE_WITH_CONDITIONS**（见 task output `ses_f5f27577effeJr1y4IGj1NSGVk`，2026-09-15）

### 5 项条件（已落入本提案 / design.md）
1. **CONDITION 1**：VfPool 保持值成员，**不**拆分为 ChStream child — 已落入 §3 Out of Scope
2. **CONDITION 2**：4 个子模块静态注册表保留为 shim — 已落入 §1 Phase 1
3. **CONDITION 3**：EP override `tick()` 显式定序；**禁用** SimModule 默认 unordered_map 迭代 — 已落入 §2 Phase 2
4. **CONDITION 4**：LL↔PHY↔Mux 内部信号**禁止** Bundle 化 — 已落入 §3 Out of Scope
5. **CONDITION 5**：Phase 1 / Phase 2 各阶段全量回归（44498 assertions）+ 14 个注册表依赖测试零修改通过 — 已落入 tasks.md §5 验证 gate

### 5 项关键风险（design.md §0.6 展开）
- **R1 注册表兼容**：`for_endpoint` 被 src/tlm/gpu/pcie_endpoint_tlm.cc（冻结 EP）+ 14+ 测试文件依赖；composite 内部可改写实现，`for_endpoint(name)` 返回值语义不变
- **R2 双注册残留**：必须删除 `chstream_register.hh:82` object 注册，否则 `getRegisteredObjectTypes()` 返回 stale
- **R3 tick 顺序漂移**：SimModule 默认 tick 遍历 unordered_map（`sim_module.hh:166`）非确定序；EP 必须 override tick；**Phase 1 不引入 PHY tick 激活**（composite->tick() Phase 1 无调用者,行为零变化）；Phase 2 EP tick override 时同步迁移 PHY tick 驱动权（design §1.3）
- **R4 已规避（决策 C）**：本 change 不引入外层声明式 `pcie_ep.pf0` 接线；composite 17 端口经 `getInternalOutputPort`/`getInternalInputPort` 程序化可达（`sim_module.hh:140-146` 命中 internal_factory + Step 7 mirror）；Phase 8 数据路径靠 429327d 程序化桥接,本 change 不破坏；Phase 3 AxiAdapter 拆分时再统一暴露机制
- **R5 延迟不变量**：保持 LL↔PHY↔Mux 直接指针 → 零新增 cycle；风险仅在后续"顺手" Bundle 化（spec 中显式列 non-goal）

### 复评证据清单（实施后须提交）
1. `git diff --stat` 范围限于上述修改文件 + 新增 composite + 新增测试 + 4 个 OpenSpec 制品
2. `./build/bin/cpptlm_tests "[pcie]"` 全绿（44498 assertions，含 14+ 注册表依赖文件）
3. `./build/bin/cpptlm_tests "[pcie-simmodule-refactor]"` 6+ 新增 TEST_CASE 全绿
4. `python3 examples/demo_pcie_full_e2e.py` 通过
5. `openspec validate 2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor --strict` PASS
6. `strings build/bin/cpptlm_tests | grep -c "PcieLinkPhyMuxTLM"` ≥ 1（composite 在 binary 内）
7. `grep -rn "static FILE\* diag" include/ src/` 输出空（无诊断残留）

## refs

- **主 spec**：`openspec/specs/pcie-ip-microarch/spec.md`（MODIFIED — `pcie-endpoint-ip-composition` 新增 Requirement）
- **架构文档**：`docs/architecture/14-pcie-ip-microarchitecture.md` §8-§10（Phase 7 M2 标注，Phase 8 集成；本 change 更新 §14 章节）
- **cluster precedent**：`src/tlm/cluster/apu_soc.cc:47-82`（SimModule 持有 ChStream 子模块 + SimModule 子模块）
- **嵌套机制**：`include/core/sim_module.hh:31-261`（MAX_DEPTH=8 + 默认 tick 不定序 + incorporate_parent hook）
- **工厂 8 步流程**：`src/core/module_factory.cc:141-843`（Step 7 ChStream adapter 注入 line 615-712；module registry 优先查找 line 266-269）
- **连接解析**：`src/core/connection_resolver.cc:47-98`（SimModule 单层 exposed-port 解析；两层下钻不在本 change 范围 —— 见 R4 决策 C）
- **上游**：`cpptlm-dgpu-pcie-integration`（Phase 8 ship, commit `429327d`）
- **下游 / 跨 change**：`2027-02-09-cpptlm-pcie-endpoint-ip-json-config`（archive, 2027-02-09，JSON 消费语义）；`cpptlm-pcie-endpoint-ip-axiadapter-split`（规划中，Phase 3）
- **ABI**：`CPPTLM_PCIE_ENDPOINT_ABI_VERSION=2` 保持；基类切换是 ABI 安全的非虚、非 ABI 暴露成员变化

## Timeline

| 子阶段 | 工作量 | 累计 |
|--------|--------|------|
| §1 OpenSpec 脚手架（本文档 + design + specs + tasks） | 1h | 1h |
| §2 Phase 1 — Composite 骨架 + 静态注册表 shim（任务 1.1-1.3） | 4h | 5h |
| §3 Phase 1 — 测试骨架 + TDD 红（任务 1.4-1.5） | 2h | 7h |
| §4 Phase 2 — EP 基类切换 + REGISTER_MODULE 迁移（任务 2.1-2.2） | 3h | 10h |
| §5 Phase 2 — composite 17 端口程序化可达性验证 + E2E 验证（任务 2.2-2.4） | 1h | 11h |
| §6 文档同步 + Oracle 复评（任务 3.1-3.2） | 2h | 13h |
| §7 buffer + review | 4h | 17h |

总计：约 **2 工作日**（密集执行 13h + 缓冲 4h），按 1.5 周排期（R4 决策 C 缩减 Phase 2 task 2.2 工时从 4h 到 1h）。

## Appendix

### A. 4 子模块分解判定矩阵（Oracle R 输出 §2）

| 子模块 | 判定 | 强耦合证据 |
|--------|------|----------|
| PcieLinkLayer | 进 composite（不可独立） | PHY 直接调 `link_layer_->trigger_rate_switch()`（phy hh:136-137）；Mux 调 `clear_retry_buffer/reset_seq_counters/reset_fc_buckets`（ll hh:189-197）；EP 按名查 `for_endpoint` 后 `ll->tick()`（ep cc:371-373） |
| PciePhyDigitalCtrl | 进 composite（不可独立） | 持 `link_layer_` + `mux_` 双裸指针（phy hh:212-213）；rate switch 原子性要求"调用即拒收发"（ll hh:299-303） |
| PcieBypassMux | 进 composite（不可独立） | 构造即持 `PcieLinkLayer*`（mux hh:63）；`apply_mode` 10 步全是对 LL 成员的直接方法调用（mux hh:130-138） |
| PcieSriovVfPool | **保持值成员**（Oracle 驳回拆分） | `pool_` 是 EP 值成员（ep hh:172）；EP::tick 数据路径每拍访问（cc:280,296,336,340）；8+ 公开访问器透传（ep hh:80-92）；无端口/无 tick/无独立生命周期 |
| PcieAxiAdapter | **Phase 3 独立提案** | 当前非 SimObject（axi hh:40 无基类）；EP::tick `cc:255-364` 的 AXI slave 业务逻辑住在 EP 里 |

### B. 不变量（强约束，写入 design.md §0.2）

- **INV-1**：`link_up(true)` 由 composite 构造后立即调用，EP 不再负责（封装 `pcie_endpoint_ip.cc:129` 语义）
- **INV-2**：`attach_composition` 仅 composition-time 可调用（`configure_vectors` placement-new 销毁 pending IRQ，运行期调用会丢中断）
- **INV-3**：EP `tick()` 必须 override 且显式定序 AXI→composite→adapters；禁止依赖 SimModule 默认 unordered_map 迭代
- **INV-4**：LL↔PHY↔Mux 内部信号保持直接指针 + std::function sink；禁止 Bundle 化
- **INV-5**：23 ABI header（`pcie_endpoint_tlm.h` / `cpptlm_emulator.h`）零触碰
- **INV-6**：4 子模块静态注册表 `attach_to_endpoint/for_endpoint/detach_from_endpoint` API 行为保留
