# design: cpptlm-simmodule-exposed-port-wiring

> **上游**: `proposal.md` · `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`（Proposed，0 BLOCKER）
> **事实基础**: 全部实测 / 读码（行号见 D-1）

---

## D-1 · 机制实测清单（**防脑补**）

| # | 事实 | 位置 |
|:--:|---|---|
| F-1 | `ChStreamInitiatorPort : MasterPort : SimplePort`；`ChStreamTargetPort : SlavePort : SimplePort` | `include/core/chstream_port.hh:17,56`；`include/core/master_port.hh:10`、`slave_port.hh:10` |
| F-2 | `PortPair(SimplePort* a, SimplePort* b)` 构造**无条件** `side_a->bind(this,0)` / `side_b->bind(this,1)` ⇒ 二次连接**静默覆盖** | `include/core/simple_port.hh:46-49` |
| F-3 | Step 7 把 ChStream 端口 mirror 进模块 PortManager（`req_out[i]`/`resp_in[i]`/`req_in[i]`/`resp_out[i]`） | `src/core/module_factory.cc:765-778` |
| F-4 | Step 7b 的连线来源：`ch_req_out` / `ch_resp_in` / `ch_req_in` / `ch_resp_out`（`unordered_map<string, vector<...>>`），索引按**数字**解析 | `src/core/module_factory.cc:700-704`、`1133-1185`（idx 解析 `1116-1136`） |
| F-5 | `getInternalInputPort` → 内部实例的 `PortManager::getUpstreamPort(port)`；`getInternalOutputPort` → `getDownstreamPort(port)` | `include/core/sim_module.hh:132-146` |
| F-6 | `parsePortSpec` 按**第一个点**切分 ⇒ 内部路径 `r00.req_in[4]` ⇒ (`r00`, `req_in[4]`)；嵌套容器路径 `child.r00.req_in[4]` ⇒ (`child`, `r00.req_in[4]`) ⇒ 取不到 | `include/core/sim_module.hh:237-243` |
| F-7 | `module_instances` = 顶层 SimModule 映射（`unordered_map<string, SimModule*>`） | `src/core/module_factory.cc:345`、`374` |
| F-8 | 既有 config 用 `<SimModule实例>.<port>` 引用**自身 PortManager 普通端口**：`pcie_ep.axi_slave_in` / `pcie_ep.axi_master_out`（Phase 8 E2E）· `cpu0_l2cache.l1_0` / `cpu0_l2cache.l1_1` | `examples/dgpu_soc_with_pcie_ip.json` · `configs/library_cpu_demo.json` |
| F-9 | 上述既有 SimModule 实例**均未**声明 `outputs`/`inputs` | 同上（实测 `keys`） |
| F-10 | `SimplePort::send` 在 `pair==nullptr` 时返回 `false` ⇒ 未接线的端口**静默丢包** | `include/core/simple_port.hh:52-58` |

---

## D-2 · Step 7c 算法（照 ADR-X.15 决策 1~9）

**位置**：`ModuleFactory::instantiateAll` 内，**Step 7b 之后**（Step 7b 结束于 `module_factory.cc:1185` 附近）。依赖 Step 7 产出的 `ch_*` 映射与 Step 4.5 触发的嵌套 `instantiateAll`（内部 mirror 已就位）—— 故**必须在 7b 之后**。

### D-2.1 · 端点分类（**关键：必须区分 exposed 标签与 SimModule 自身普通端口**）

```
端点 spec = "<module>.<port>"
  ├─ module 不在 module_instances（顶层 SimModule）        ⇒ 非 exposed 端点
  ├─ module 在 module_instances 但 findInternalPath(port) 非空 ⇒ **exposed 标签**
  └─ module 在 module_instances 且 findInternalPath(port) 为空
        ├─ 该 SimModule **声明过** exposed（outputs/inputs 非空）⇒ **硬错误 ①**（拼写错误）
        └─ 该 SimModule **未声明** exposed                    ⇒ 非 exposed 端点（既有普通端口语义，F-8/F-9 ⇒ 不动）
```

⚠️ F-8 是**回归红线**：既有 `pcie_ep.axi_slave_in` 走的是最后一条分支，行为必须**逐字节不变**。

### D-2.2 · 方向表（ADR 决策 2）

| 内部端口后缀 | 内部端口 role | 取端口方式 | 对端端口 | PortPair 顺序 (Master, Slave) |
|---|---|---|---|---|
| `req_in[N]` | Slave | `getInternalInputPort` | `ch_req_out[peer][idx]` | `(peer.req_out, internal)` |
| `resp_in[N]` | Slave | `getInternalInputPort` | `ch_resp_out[peer][idx]` | `(peer.resp_out, internal)` |
| `req_out[N]` | Master | `getInternalOutputPort` | `ch_req_in[peer][idx]` | `(internal, peer.req_in)` |
| `resp_out[N]` | Master | `getInternalOutputPort` | `ch_resp_in[peer][idx]` | `(internal, peer.resp_in)` |

**后缀识别**：取内部路径的**最后一段**（`internal_path.rfind('.')` 之后，如 `req_in[4]`）的前缀；**不匹配四类 ⇒ 硬错误 ④**。
⚠️ **必须取最后一段**：嵌套容器路径 `inner.r0.req_out[4]` 的首段是 `inner`，若按首段切分会把它误判为「后缀非四类」（错误 ④）而非「内部端口不可解析」（错误 ②）。
⚠️ **不得**用 exposed 标签字面（`portN_in`/`portN_out`）判方向 —— `CpuCluster::cpu0_to_bus` 的内部是 `req_out`（承载**请求**的 exposed output），字面会判反。

### D-2.3 · 对端解析

对端（非 exposed 一侧）必须是**顶层 ChStream 模块** + **数字索引**：

- `ch_adapters.count(peer)==0` ⇒ 硬错误（exposed 连接的对端必须是 ChStream 模块）
- 端口规格非纯数字 ⇒ 硬错误
- `idx >= ch_*[peer].size()` ⇒ 硬错误 ③

### D-2.4 · 建 pair 前守卫（ADR 决策 2b）

- 两侧 `isBound()==false` ⇒ 否则**硬错误**（防静默覆盖，F-2）。
- `PortPair(master, slave)` 后按 ADR 决策 8：`setDelay(latency)` 作用于 **initiator（Master）侧**。

### D-2.5 · 失败语义（ADR 决策 6，全部**硬错误** + 终止装配）

| 类 | 触发 | 归属 |
|---|---|---|
| ① | 声明过 exposed 的容器上引用**未声明**的标签（D-2.1 第 3 分支） | 拼写错误 |
| ② | `getInternal{Input,Output}Port` 返 `nullptr` —— 含**嵌套容器**路径（F-6） | 内部路径不可解析 |
| ③ | 对端索引越界 / 非数字 | 索引错误 |
| ④ | 内部端口后缀不属四类（如 `sdma.host_out`） | Step 7c 仅服务 ChStream mirror 端口 |
| ⑤ | **两端都是 exposed** | 本决策仅支持「一端 exposed + 一端顶层 ChStream」 |

⛔ 不得复用 Step 7b 的静默 `continue` 惯例。

### D-2.6 · 与既有阶段的边界

- **Step 6 不动**（ADR 决策 5）：其 wildcard 恒真分支对带索引端点恒丢弃 ⇒ 不会与 7c 争抢。
- **Step 7b 不动**（ADR 决策 4）：`src_ch && dst_ch` 判据对容器恒假 ⇒ 不会双连。
- **纯 ChStream 连接**：两端都非 exposed ⇒ 7c 直接 `continue` ⇒ 行为零变化（AC-05）。
- **去重**：`processed_connections` 共用；7c 只处理 exposed 连接，与 7b 输入集不相交。

---

## D-3 · `SimplePort::isBound()`

```cpp
// include/core/simple_port.hh（public 段）
bool isBound() const { return pair != nullptr; }
```

`pair` 是 `protected` 成员，`isBound()` 作为 const 成员函数在同类内访问 ⇒ 无需改可见性。零行为影响（纯新增查询）。

---

## D-4 · 兼容性豁免（ADR 决策 7）

Step 7c **不**套用 ADR-X.9 的 L1/L2/L3 兼容性检查（`check_port_compatibility`）：exposed 端点无 `port_specs` 声明面可查。代价 = 7c 路径不做层次兼容性校验，改由「方向表的 Master/Slave 角色匹配 + `isBound()` 硬校验 + 五类硬错误」兜底。

---

## D-5 · 判据落点

| 判据 | 落点 |
|:--:|---|
| AC-01 | `test/test_simmodule_exposed_port_wiring.cc`（新增）：装配后检查 PortPair 与 `isBound()` |
| AC-02 | 同文件：4 组后缀各一例 + `CpuCluster` 型 req_out 反例（正控/负控） |
| AC-03 | 同文件：同端二次连接 ⇒ 装配返回 false 且有错误输出 |
| AC-04 | 同文件：五类失败各一例 ⇒ 装配返回 false |
| AC-05 | 既有 `[cep]` / `[e2e]` / `[mesh]` / `[simmodule]` 全量 |
| AC-06 | `git diff src/core/module_factory.cc` 检 Step 6 段零改动 |
| AC-07 | `test/test_cep_mesh_e2e.cc`（层次化版，隶属另一 change）绿 |
| AC-08 | `examples/dgpu_soc_with_pcie_ip.json` 驱动的 Phase 8 E2E 绿 + `configs/library_cpu_demo.json` 可装配 |

---

## D-6 · 风险

| 风险 | 缓解 |
|---|---|
| 硬错误①误伤既有普通端口引用（F-8） | D-2.1 的三分支判定：**未声明 exposed 的 SimModule 不走硬错误**；AC-08 专项回归 |
| `PortPair` 静默覆盖 | `isBound()` 守卫 + AC-03 负例 |
| 对端 ChStream 模块的 req/resp 端口配对搞反 | 方向表 + AC-02 四组用例（含 `CpuCluster` 反例） |
| Step 7c 位置放错（早于 mirror / 晚于 7b 的去重） | 置于 7b 之后；AC-05 全量回归兜底 |
| 嵌套容器暴露 | 明确**不支持** ⇒ 硬错误 ②（而非静默失败） |


---

## D-7 · 实施记录（2026-11-16）

- 已实现 `src/core/module_factory.cc` Step 7c + `include/core/simple_port.hh::isBound()`；Step 6 / Step 7b 零改动（`git diff` 确认）。
- 判据 `test/test_simmodule_exposed_port_wiring.cc`：EXP-1 ~ EXP-10 全绿（33 断言 / 10 用例），五类硬错误各自命中精确报错。
- ⚠️ **给容器作者的实现发现**：`SimModule` 的**内联 `modules`** 能力要求模块条目**同时含 `connections` 数组**（`ModuleFactory::validateConfig` 要求顶层同时有 `modules` 与 `connections`）；否则内部工厂校验失败，且该失败被 `SimModule::simulate_instantiate` **静默忽略**（既有嵌套 config 均遵循此约定，见 `configs/example_simmodule_nested_2level.json`）。
- 回归：`[cep]` 534 · `[e2e]` 145 · `[simmodule]` 44 · `[mesh]` 14 · `[pcie]` 452 · `[realip]` 51 全绿；`examples/dgpu_soc_with_pcie_ip.json` 与 `configs/library_cpu_demo.json` 装配正常（未声明 exposed 的 SimModule 自身端口引用零变化）。
