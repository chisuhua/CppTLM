# change: cpptlm-cep-mesh-interconnect-submodule

**主题**: 把 CEP 的互联拓扑抽成**真子模块**（`CepMeshInterconnect : SimModule`），使互联层与业务层在**拓扑文件**与**命名空间**两个维度上同时分离，且同一份互联蓝图可被多个 e2e 复用。

> **上游**: 用户 2026-11-16 指令「把 `cep_mesh_e2e.json` 改进成互联拓扑独立的层次化结构…互联相关的放在独立拓扑里，业务接口外的模块放在上层」+ 选型 = **路径 B（真子模块）**
> **前序**: change `cpptlm-cep-mesh-e2e-carrier`（已在 `2026-11-16` 归档 · 本期改造它的拓扑组织，**不改行为**）

---

## Why

### 第 1 条 —— 当前互联层与业务层混在同一份扁平 JSON 里。

`configs/cep_mesh_e2e.json` 现状：9 个模块（4 router + 2 NIC + tg/w/home）与 17 条 connection **同处一个扁平数组** ⇒ 后果：
- router 名字（`r00`..`r11`）与业务模块名字（`tg`/`w`/`home`）**同一命名空间** ⇒ 换网就撞名；
- **换互联形态**（2×2 → 3×3）必须改主 config 的 `modules` + 8~18 条 connection ⇒ 互联形态与业务意图耦合。

### 第 2 条 —— 仓内已有两道现成机制，无需发明。

| 机制 | 现状 | 位置 |
|---|---|---|
| `SimModule` 子模块（`modules` + `connections` + `outputs`/`inputs` 暴露端口） | ✅ 9 个容器类在用（`CpuCluster` / `GpuCluster` / `ApuSoC` / `GpuNoC` 等） | `include/core/sim_module.hh:66-175` |
| **外部 JSON 蓝图加载**（`params.<x>_template` → `JsonIncluder::loadAndInclude` → `internal_factory->instantiateAll`） | ✅ `ComputeCluster::cu_template` 先例 | `src/tlm/cluster/compute_cluster.cc:28-60` |

⇒ 本 change **只加一个容器类**，把这两道机制组合起来，**⛔ 不发明任何新机制**。

### 第 3 条 —— 关键可行性已实测确认。

- `SimModule(n, eq)` 的 `internal_factory` **共用同一个 `EventQueue`**（`include/core/sim_module.hh:66-67`）⇒ 子模块与外部**时钟同步**（这是子模块方案成立的前提）
- `outputs`/`inputs` 的 `internal` 路径按**第一个 dot** 拆（`parsePortSpec`，`sim_module.hh:240-246`）⇒ 支持 `<子模块>.<端口名>`；端口名可带索引（`module_factory.cc:766-770` 把多端口 mirror 成 `req_out[i]` / `req_in[i]`）
- `RouterTLM` 的 LOCAL 口 = 索引 4（NORTH=0/EAST=1/SOUTH=2/WEST=3/LOCAL=4）

---

## What Changes

1. **新增** `include/tlm/cep/cep_mesh_interconnect.hh`（header-only）：`CepMeshInterconnect : SimModule`
   - `params.template` ⇒ 加载外部互联蓝图 JSON（照 `ComputeCluster::cu_template` 的既有模式）
   - **⛔ 不写端口暴露代码** —— `outputs`/`inputs` 由**主 config 声明**，走 `SimModule` 的通用机制
   - **⛔ 不承载任何业务语义**（无 NIC、无 coherence、无地址映射）
2. **注册**：`modules_cluster.hh` 加 `REGISTER_MODULE(CepMeshInterconnect)`
3. **新增** `configs/interconnect/cep_mesh_2x2.json`：2×2 mesh 蓝图（4 `RouterTLM` + 8 条环边 connection）
4. **改造** `configs/cep_mesh_e2e.json`：`fabric` 子模块 + 业务模块 + 仅**跨层** connection
5. **新增** `test/test_cep_mesh_interconnect.cc`：子模块装配 / 端口暴露 / 层次命名空间 / **行为等价性**（改造前后端到端观测一致）
6. **改** `scripts/CMakeLists.txt` 的 `CPPTLM_CEP_VALIDATION_CONFIGS`（若主 config 路径不变则无需改；新增蓝图**不**进 IAG 清单 —— 它是蓝图不是顶层拓扑）

---

## Acceptance

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | 容器可注册可装配 | `ModuleFactory` 注册表含 `CepMeshInterconnect`；`params.template` 指向的蓝图被加载；内部 4 个 router 实例在位 |
| **AC-02** | **层次命名空间成立** | 内部实例可经 `fabric` 内部访问；`findInternalPath("node0_resp_out") == "r00.resp_out[4]"`（8 个暴露标签各自映射到对应内部端口） |
| **AC-03** | **端口暴露可用** | 主 config 的 8 条 exposed 端口（4 out + 4 in）全部可解析；外部 connection 经 `fabric.portN_*` 生效 |
| **AC-04** | **行为等价（mesh 级）** | 同一次运行内比对「内嵌平铺基线」：4 个 router 的 `flits_forwarded` / `packets_forwarded` / `total_hops` **逐项相等** + 对端 NIC 收到的 flit 数相等（⚠️ **不断言** `last_recv_*` 顺序敏感计数器，理由见 design.md D-7） |
| **AC-05** | **蓝图可复用** | 同一份 `interconnect/cep_mesh_2x2.json` 被装配成 `fabric` 后，其内部模块数 / 连接数与蓝图逐项相等（判据比对蓝图与 `getInternalFactory()` 实例清单） |
| **AC-06** | **零回归** | `[cep]` / `[e2e]` / `ctest -R cep` 全绿；`CohIfNICTLM` **零 diff**；既有 8 份点对点 config **零 diff** |
| **AC-07** | **换形态只改蓝图** | 新增一份 1×2 蓝图（`cep_mesh_1x2.json`）⇒ 只改 `fabric.params.template` 即可装配（**不改主 config 的 modules/connections**） |

---

## Capabilities

| # | 能力 | 判据落点 | 新增 `INV-CEP-*` |
|:--:|---|---|---|
| **C-1** | 互联层与业务层在**文件**与**命名空间**两个维度分离 | AC-02 / AC-03 | ❌ 0 |
| **C-2** | 同一互联蓝图可复用 / 可替换 | AC-05 / AC-07 | ❌ 0 |
| **C-3** | 改造**零行为变化** | AC-04 / AC-06 | ❌ 0 |

---

## Impact

| 面 | 影响 |
|---|---|
| **新增** | `include/tlm/cep/cep_mesh_interconnect.hh` · `configs/interconnect/cep_mesh_2x2.json` · `test/test_cep_mesh_interconnect.cc` · delta spec |
| **改动** | `include/modules_cluster.hh`（+1 注册）· `configs/cep_mesh_e2e.json`（改造）· change 四件套 · improvement · 台账 |
| **⛔ 不动** | `include/tlm/cep/cohif_nic.hh`（`CohIfNICTLM` 零 diff）· `include/core/sim_module.hh` · `include/core/module_factory.cc` · `include/utils/json_includer.hh` · **8 份点对点 config**（用户裁决：无互联层，不动）· 既有 44 份 `cep-*` main spec |
| **计数** | `INV-CEP-*` 新增 **0** · `B3b-2` 维持 **0/4** |

---

## ⚠️ 明确**不做**（用户裁决）

| # | 项 | 理由 |
|:--:|---|---|
| **N-1** | 8 份点对点 config（`cep_*_pe_minimal.json` × 6 + `cep_chi_rnf_e2e_a|_b.json`）改成 include/子模块形式 | 实测：它们 **2~3 模块 / 2~3 条直连 / 无 router** ⇒ 无互联层可抽 ⇒ 强行抽会造**零复用价值的间接层** |
| **N-2** | 改 `JsonIncluder` 的数组合并规则（路径 A） | 用户选型 = 路径 B ⇒ 不需要 |
