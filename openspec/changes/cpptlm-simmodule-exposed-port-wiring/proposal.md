# change: cpptlm-simmodule-exposed-port-wiring

**主题**：让 `SimModule` 容器的**暴露端口**（`outputs`/`inputs`）承载**真实数据流** —— 在 `ModuleFactory` 新增隔离的连接阶段（Step 7c），把「exposed 端点 ↔ 顶层 ChStream 端点」的连接建成真实 `PortPair`。

> **上游决策**: `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`（Status: Proposed · oracle APPROVE + critic APPROVE_WITH_CONDITIONS · 0 BLOCKER · 审稿报告 `.rddf/reviews/ADR-X.15.md`）
> **触发**: change `cpptlm-cep-mesh-interconnect-submodule`（层次化 mesh）的实施期发现 —— 路径 B（真子模块 + exposed 端口）的前提「exposed 端口能承载真实数据流」**实测不成立**。

---

## Why

### 第 1 条 —— exposed 端口此前**从未**承载过真实数据流。

`SimModule` 的 `outputs`/`inputs` 暴露端口是一套**已文档化**的机制（main spec `simmodule-nested`），且 `module_factory.cc:765-778` 的 mirror 逻辑注释明写目的是「让 `SimModule::getInternalOutputPort` 能查到 ChStream 端口」—— 即**设计意图是支持数据通路**。但：

| 路径 | 为何接不上 |
|---|---|
| Step 5.5 端口创建 | `ConnectionResolver` 把 exposed 解析成 `(owner=r00, port=req_in[4])`，但创建循环去**顶层** `object_instances` 找 `r00` —— `r00` 在容器**内部工厂**里 ⇒ 端口从未创建 |
| Step 6 端点展名 | `Wildcard::match("*", x)` 恒为真 ⇒ else 分支死代码；带索引端点被误判 regex ⇒ 展成空集 ⇒ 静默丢弃 |
| Step 7b 实际连线 | 只处理顶层 `ch_adapters` 的 ChStream 模块 ⇒ 容器（非 ChStream）恒 `continue`，且是**静默** continue |

⇒ 净效果：exposed 端口只能被「查到路径」，**接不通流量**。

### 第 2 条 —— 已有机制**齐备**，缺的只是一次接线。

- `findInternalPath`（标签→内部路径）+ `getInternalInputPort/getInternalOutputPort`（内部工厂取**真实 Port**）已存在（`sim_module.hh:132-161`）；
- Step 7 的 mirror 已把 ChStream 端口放进子模块 PortManager（`module_factory.cc:765-778`）；
- `PortPair` 机制与 Step 7b 同源。

⇒ 本 change **只补一次接线**，⛔ 不发明端口基础设施、⛔ 不改 SimModule / ConnectionResolver 的公开 API。

### 第 3 条 —— 实测基线（本次改动前的对照）。

| 实验 | 结果 |
|---|---|
| 扁平 `configs/cep_mesh_e2e.json`（9 模块 / 17 连接） | `[mesh]` **13 用例 / 158 断言全绿**（走 Step 7b 直连） |
| 层次化版（`fabric` 容器 + exposed 端口） | `nic_b_recv_dst_node != 3` ⇒ **流量没到** |

---

## What Changes

1. **改** `include/core/simple_port.hh`：新增 `bool isBound() const { return pair != nullptr; }` —— `PortPair` 构造**无条件 bind**（`simple_port.hh:46-49`），二次连接会**静默覆盖**（flit 被劫持且零报错）。
2. **改** `src/core/module_factory.cc`：新增 **Step 7c**（隔离在 Step 7b 之后），按 ADR-X.15 决策 1~9 处理 exposed 连接。
3. **改** `openspec/specs/simmodule-nested/spec.md`（delta）：修正两处**已过时**的条款（声称 ChStream 子模块的 `getInternal*Port` 返 `nullptr` 属「已知架构限制」、且 exposed 端口语义**仅**靠 `findInternalPath`/`isExposedPort` 验证），并 ADDED Step 7c 的数据通路契约。
4. **不改**：`SimModule` / `ConnectionResolver` / `PortManager` 的公开 API；Step 6 的 wildcard 语义；Step 7b 的纯 ChStream 路径。

---

## Acceptance

判据直接对应 ADR-X.15 的 Verification（V-1~V-8）：

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | exposed 连接建成真实 PortPair | 装配后 `port_pairs_` 中存在对应 Pair；`SimplePort::isBound()` 对两侧均为 `true` |
| **AC-02** | 方向按**内部端口后缀**推导 | `req_in[N]`↔对端 `req_out`；`resp_in[N]`↔`resp_out`；`req_out[N]`↔`req_in`；`resp_out[N]`↔`resp_in`（4 组各一例） |
| **AC-03** | 无静默覆盖 | 同端二次连接 ⇒ **装配期报错**（非静默覆盖） |
| **AC-04** | 四类失败硬错误 | ①未声明的 exposed 标签（在声明过 exposed 的容器上）②`getInternal*Port` 空（含嵌套容器路径）③索引越界 ④后缀非四类 ⇒ 装配期报错 |
| **AC-05** | 纯 ChStream 零回归 | 既有 `[cep]` / `[e2e]` / `[mesh]` / `[simmodule]` 全绿；扁平 `cep_mesh_e2e` 逐断言一致 |
| **AC-06** | Step 6 未改语义 | `git diff` 中 Step 6 wildcard 分支零改动 |
| **AC-07** | 层次化端到端可达 | `CepMeshInterconnect` 装配后 `nic_b_recv_dst_node==3`、`r*.flits>0`（= ADR V-5） |
| **AC-08** | 既有 SimModule 普通端口引用不受影响 | `pcie_ep.axi_slave_in` / `cpu0_l2cache.l1_0` 等**未声明 exposed** 的 SimModule 自身端口引用行为零变化（Phase 8 E2E 绿） |

---

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|---|
| **C-1** | 容器暴露端口承载真实数据流 | AC-01 / AC-02 / AC-07 | ❌ 0 |
| **C-2** | 失败显式、不静默 | AC-03 / AC-04 | ❌ 0 |
| **C-3** | 既有路径零回归 | AC-05 / AC-06 / AC-08 | ❌ 0 |

---

## Impact

| 面 | 影响 |
|---|---|
| **改动** | `src/core/module_factory.cc`（+Step 7c）· `include/core/simple_port.hh`（+`isBound`）· delta spec · change 四件套 · 台账 · ADR-X.15 状态回填 |
| **⛔ 不动** | `include/core/sim_module.hh` · `src/core/connection_resolver.cc` · `include/core/port_manager.hh` · Step 6 wildcard 分支 · `include/tlm/cep/cohif_nic.hh` · 8 份点对点 config · 既有 130 份 main spec（除 `simmodule-nested`） |
| **计数** | `INV-CEP-*` 新增 **0** · `B3b-2` 维持 **0/4** |

---

## ⚠️ 实施期需回填的两处（已发现，非本 change 的可选项）

1. **ADR-X.15 决策 6 ① 的可达性**：声明过 exposed 的容器上引用**未声明**的标签 ⇒ 与「SimModule 自身普通端口」引用（如 `pcie_ep.axi_slave_in`）**不可区分** ⇒ 硬错误只在「该 SimModule 声明过 exposed 标签」的前提下生效；未声明 exposed 的 SimModule 维持既有普通端口语义。
2. `openspec/changes/cpptlm-cep-mesh-interconnect-submodule/design.md` 的 D-3 表 4-7 行 in/out 与落地 config 相反（config 正确、表 stale）+ 其 D-5「不改 module_factory」被本 ADR 推翻 —— 交该 change 实施前订正。
