# design: cpptlm-cep-mesh-interconnect-submodule

> **上游**: `proposal.md` · 用户 2026-11-16 选型 = 路径 B（真子模块）
> **事实基础**: 全部实测（行号见 D-1）

---

## D-1 · 机制实测清单（**防脑补**）

| # | 事实 | 位置 |
|:--:|---|---|
| F-1 | `SimModule(n, eq)` 的 `internal_factory` = `make_unique<ModuleFactory>(eq)` ⇒ **共用同一 `EventQueue`** | `include/core/sim_module.hh:66-67` |
| F-2 | `SimModule::simulate_instantiate(cfg)`：幂等守卫 → `internal_factory->instantiateAll(config)` → `parsePortConfigs(cfg)` → 递归激活子 SimModule | `include/core/sim_module.hh:81-109` |
| F-3 | `parsePortConfigs` 解析 `outputs[]` / `inputs[]`，每项 = `{internal, external}` | `include/core/sim_module.hh:209-235` |
| F-4 | `parsePortSpec` 按**第一个 dot** 拆 `module_name` / `port_name` | `include/core/sim_module.hh:240-246` |
| F-5 | `getInternalInputPort` → `portManager.getUpstreamPort(port_name)`；`getInternalOutputPort` → `getDownstreamPort(port_name)` | `include/core/sim_module.hh:132-143` |
| F-6 | 多端口模块被 mirror 成 `req_out[i]` / `resp_in[i]` / `req_in[i]` / `resp_out[i]` ⇒ **端口名可带索引** | `src/core/module_factory.cc:766-770` |
| F-7 | **外部 JSON 蓝图加载**的现成模式：`set_config` 读 `params.cu_template` → `simulate_instantiate` 里 `JsonIncluder::loadAndInclude(path)` → `internal_factory->instantiateAll(蓝图)` | `src/tlm/cluster/compute_cluster.cc:13-60` |
| F-8 | `router_tlm.hh` 的端口方向：NORTH=0 / EAST=1 / SOUTH=2 / WEST=3 / LOCAL=4 | `include/tlm/router_tlm.hh:25` |
| F-9 | `RouterTLM::num_ports()` = 5 ⇒ 会被 mirror 成 `req_out[0..4]` | `include/tlm/router_tlm.hh` + F-6 |
| F-10 | 主 config 的模块 `cfg`（含内联 `modules`/`connections`/`outputs`/`inputs`）传进 `SimModule::simulate_instantiate` | `configs/example_simmodule_nested_2level.json` 实证 |

---

## D-2 · `CepMeshInterconnect` 设计

### D-2.1 · 类形态（**照 `ComputeCluster` 的 template 模式，不发明机制**）

```cpp
class CepMeshInterconnect : public SimModule {
public:
    explicit CepMeshInterconnect(const std::string& n, EventQueue* eq)
        : SimModule(n, eq) {}
    void set_config(const nlohmann::json& params) override;   // 读 params.template
    std::string get_module_type() const override { return "CepMeshInterconnect"; }
    void simulate_instantiate(const nlohmann::json& cfg) override;

private:
    std::string template_path_;
};
```

**`simulate_instantiate` 的顺序（照 `ComputeCluster` 的实测顺序，⛔ 不可颠倒）**：

1. 幂等守卫：`if (!internal_factory->getAllInstances().empty()) return;`
2. **先** `SimModule::simulate_instantiate(cfg)` —— 用**主 config 的 cfg** 解析 `outputs`/`inputs`（F-3）⇒ 端口暴露表落在**本层**
3. `if (template_path_.empty()) return;`
4. `JsonIncluder::loadAndInclude(template_path_)` ⇒ 蓝图
5. 校验蓝图含 `modules` 数组（缺 ⇒ 抛 `std::runtime_error`，**显式失败**）
6. `internal_factory->instantiateAll(蓝图)` ⇒ 装配内部 router

⚠️ **顺序理由**：若先调 `internal_factory->instantiateAll(蓝图)` 再调基类，基类的 `parsePortConfigs(cfg)` 会用**主 cfg** 再解析一次（幂等、无害），但 `internal_factory->instantiateAll` 已幂等 ⇒ 真正必须的是**基类在前**以让 `output_configs` 用主 cfg 填充（蓝图里**没有** `outputs`/`inputs`）。⇒ 与 `ComputeCluster` 同序。

### D-2.2 · ⛔ 不写端口暴露代码

`SimModule` 的通用机制已足够：主 config 声明

```jsonc
"outputs": [
  {"internal": "r00.req_out[4]",  "external": "node0_req_out"},
  {"internal": "r00.resp_out[4]", "external": "node0_resp_out"}],
"inputs": [
  {"internal": "r00.req_in[4]",  "external": "node0_req_in"},
  {"internal": "r00.resp_in[4]", "external": "node0_resp_in"}]
```

⇒ `CepMeshInterconnect` **零端口代码**。⚠️ 这也意味着**接入点数由主 config 决定**（蓝图有几个 LOCAL 口就暴露几个）。

### D-2.3 · 蓝图契约（`configs/interconnect/*.json`）

| 约束 | 内容 |
|---|---|
| 必须含 `modules` 数组 | 缺 ⇒ `simulate_instantiate` **抛错**（⛔ 不静默空装配） |
| **必须**用固定命名 `r00`..`rNN` | 主 config 的 `outputs.internal` 要按名字指过去 ⇒ 名字是**蓝图与主 config 的接口契约** |
| ⛔ **不得**含 `outputs` / `inputs` | 端口暴露归**主 config**（否则两层都在定义端口 ⇒ 语义冲突） |
| ⛔ 不得含业务模块 | 蓝图只放互联件（router / link） |
| 可含内层 `connections` | 仅**互联内部**的边（router↔router） |

---

## D-3 · 拓扑改造（`configs/cep_mesh_e2e.json`）

**改造前**：9 模块 / 17 connection（全扁平）
**改造后**：6 个顶层模块（`fabric` + 业务 5 个）/ 9 条**跨层** connection；互联的 4 router + 8 条环边进蓝图。

| 层 | 内容 |
|---|---|
| `configs/interconnect/cep_mesh_2x2.json`（蓝图） | 4 × `RouterTLM`（`r00`/`r01`/`r10`/`r11`）+ 8 条环边（`r00↔r01` · `r01↔r11` · `r11↔r10` · `r10↔r00`，每条 2 个方向） |
| `configs/cep_mesh_e2e.json`（主） | 业务 5 模块（`tg`/`w`/`home`/`nic_a`/`nic_b`）+ `fabric`（`params.template` + 8 条 exposed 端口）+ 9 条跨层 connection |

**跨层 connection 清单（13 条：业务 5 + 每 NIC-节点链路 4 条）**：

> ⚠️ **为何每条链路 4 条**：平铺基线用 2 条对称 connection，经 Step 7b 的**自动镜像**为 router LOCAL 口的 4 个通道各建 1 条 PortPair；Step 7c 是**单向**语义（ADR-X.15 决策 2）⇒ 必须显式暴露 4 个通道 + 声明 4 条 connection 才能与平铺基线**逐条 pair 等价**。

| # | src | dst | 说明 |
|:--:|---|---|---|
| 1 | `tg` | `w.0` | 业务流量入 stub |
| 2 | `w.1` | `nic_a.0` | stub → 网络 |
| 3 | `nic_a.0` | `w.1` | 回程 |
| 4 | `nic_b.0` | `home.0` | 网络 → home |
| 5 | `home.0` | `nic_b.0` | home → 网络 |
| 6 | `nic_a.1` | `fabric.node0_req_in` | NIC → 互联(node0 req 入) |
| 7 | `fabric.node0_resp_out` | `nic_a.1` | 互联(node0 resp 出) → NIC |
| 8 | `fabric.node0_req_out` | `nic_a.1` | 互联(node0 req 出) → NIC |
| 9 | `nic_a.1` | `fabric.node0_resp_in` | NIC → 互联(node0 resp 入) |
| 10 | `nic_b.1` | `fabric.node3_req_in` | NIC → 互联(node3 req 入) |
| 11 | `fabric.node3_resp_out` | `nic_b.1` | 互联(node3 resp 出) → NIC |
| 12 | `fabric.node3_req_out` | `nic_b.1` | 互联(node3 req 出) → NIC |
| 13 | `nic_b.1` | `fabric.node3_resp_in` | NIC → 互联(node3 resp 入) |

| # | src | dst | 说明 |
|:--:|---|---|---|
| 1 | `tg` | `w.0` | 业务流量入 stub |
| 2 | `w.1` | `nic_a.0` | stub → 网络 |
| 3 | `nic_a.0` | `w.1` | 回程 |
| 4 | `nic_a.1` | `fabric.port0_out` | NIC → 互联(节点 0) |
| 5 | `fabric.port0_in` | `nic_a.1` | 互联 → NIC(回程) |
| 6 | `fabric.port3_in` | `nic_b.1` | 互联 → NIC(节点 3) |
| 7 | `nic_b.1` | `fabric.port3_out` | NIC → 互联(回程起点) |
| 8 | `nic_b.0` | `home.0` | 网络 → home |
| 9 | `home.0` | `nic_b.0` | home → 网络 |

⚠️ **端口方向必须实测确认**（照上一 change 的 T1.3 教训）：`portN_out` 映射 `rNN.resp_out[4]`（RTLM egress 走 `resp_out`），`portN_in` 映射 `rNN.req_in[4]`。⇒ T1.2 先用探针验证，**不得凭推断**。

---

## D-4 · 判据设计

| # | 判据 | 断言 |
|:--:|---|---|
| **J-1** | 装配 | `instantiateAll` rc=0；`fabric` + 5 业务模块在位；`fabric.getInternalFactory()` 有 4 个 router |
| **J-2** | 层次命名空间 | `findInternalPath("node0_resp_out") == "r00.resp_out[4]"`；`getInternalInstance("r00") != nullptr` |
| **J-3** | 端口暴露 | 8 条 exposed 端口全部可解析（`isExposedPort`） |
| **J-4** | **行为等价（mesh 级）** | 同一次运行内比对内嵌平铺基线：4 个 router 的 flits/packets/hops **逐项相等** + `nic_b.flits_received()` 相等（判据见 design D-7） |
| **J-5** | 蓝图可复用 | 蓝图内 `modules` 数 == `fabric.getInternalFactory().getAllInstances().size()`；蓝图 `connections` 数 == 内部 PortPair 数 |
| **J-6** | 换蓝图可复用（修正原「换形态只改蓝图」） | `cep_mesh_1x2.json` 换入 `params.template` ⇒ 装配成功、内部 router 数 **2**、按 1×2 的标签可接线；**不改容器类**。⚠️ 不含「主 config 零改」——标签按 router 名映射，命名/数量变化须同步标签（MESH-6 用独立最小主拓扑验证） |

---

## D-5 · 边界

1. `CepMeshInterconnect` **⛔ 不承载业务语义**（无 NIC / 无 coherence / 无地址映射）—— 它只是**互联拓扑的容器**。
2. 蓝图的 `rNN` 命名是**接口契约**（主 config 的 exposed 端口按名指过去）⇒ 改名 = 破坏契约。
3. 蓝图**不得**含 `outputs`/`inputs`（端口暴露归主 config）。
4. 本 change **⛔ 不改 `CohIfNICTLM`**、⛔ 不改 `SimModule` / `module_factory` / `JsonIncluder`。
5. 8 份点对点 config **不动**（用户裁决 · proposal N-1）。

---

## D-6 · 风险

| 风险 | 缓解 |
|---|---|
| `outputs.internal` 的 `resp_out[4]` 端口名在内部 factory 里查不到 | **T1.2 先用最小探针验证**（1 个 SimModule 内嵌 1 个 RouterTLM + 1 条 exposed 端口）⇒ 不通则**停**并回报 |
| 内部 `connections` 的端口索引语义与顶层不同 | 蓝图内连接照 `configs/cep_mesh_e2e.json` **改造前**的既有写法（已验证可通），不改语义 |
| `SimModule::simulate_instantiate` 的幂等守卫导致蓝图加载被跳过 | 照 `ComputeCluster` 的顺序：先基类（会进入幂等检查并 populate 空 modules）→ 再蓝图加载 ⇒ ⚠️ **须实测**：基类 `instantiateAll(cfg)` 在 `cfg` 无 `modules` 时是否会把 `getAllInstances()` 变为非空（若变为非空则第 4 步的守卫会拦） |
| 改造后行为漂移 | J-4 在**同一次运行内**比对内嵌平铺基线（mesh 级观测逐项相等） |


---

## D-7 · 实施记录与发现（2026-11-16）

- 落地：`configs/cep_mesh_e2e.json`（6 顶层模块 + 13 条连接，`fabric` 暴露 8 个标签）+ `configs/interconnect/cep_mesh_2x2.json`（4 router + 8 条环边）+ `include/tlm/cep/cep_mesh_interconnect.hh` + 注册 + `test/test_cep_mesh_e2e.cc`（MESH-1~5 全绿）。
- **暴露端口模型 = router LOCAL 口的 4 个通道全暴露**（req_in / req_out / resp_in / resp_out）：Step 7c 单向语义 ⇒ 4 标签 + 4 连接才与平铺基线逐条 pair 等价（pair dump 实测：34/34 完全一致）。
- **⚠️ 发现 1（时序/结构）**：层次化把容器子模块的 tick 收成**连续一组**（`SimModule::tick` 转发），与平铺时各模块按哈希序交织不同 ⇒ **NIC 侧顺序敏感计数器**（`last_recv_dst_node/_hops`、回声命中数）会变。**mesh 级观测（router flits/packets/hops）逐项不变**（实测 r00 15/15/29 · r01 8/8/16 · r10 7/7/14 · r11 15/15/31，两侧一致）。⇒ J-4 判据改用 mesh 级等同 + NIC 总收数，**不断言** `last_recv_*`。
- **⚠️ 发现 2（既有缺陷，⛔ 不在本 change 范围）**：`RouterTLM::routing_table_` 以 `transaction_id` 为键、**不含方向**；请求与其响应共用同一 tx id ⇒ 响应到达前若该 tx 的路由条目仍指向 `LOCAL`，后续同 tx 的 flit 会被误投 LOCAL（回声）。该缺陷**平铺基线同样存在**（只是命中数不同）。已登记为待立项 backlog，**本 change 不改 `RouterTLM`**。
- **⚠️ 发现 3（给容器作者）**：`SimModule` 内联 `modules` 要求模块条目**同时含 `connections` 数组**，否则内部工厂校验失败且被静默忽略（既有嵌套 config 均遵循）。
