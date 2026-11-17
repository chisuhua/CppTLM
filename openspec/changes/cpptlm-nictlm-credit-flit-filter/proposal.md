# change: cpptlm-nictlm-credit-flit-filter

**主题**: 让 `NICTLM`（通用网卡）在网络 ingress **丢弃 in-band credit flit** —— 与已修复的 `CohIfNICTLM` 侧**同款判据、同款行为** ⇒ 消除「credit 被当成数据 flit 重组、并向业务侧灌入垃圾响应」的既有缺陷。

> **决策源**: 无独立 ADR —— 本 change 是 `docs/adr/ADR-X.16-router-local-port-terminal-credit.md` 的 `## 关联` 章「相邻独立缺陷」第 2 条（`NICTLM` 无 credit flit 过滤）的实现化，兼 `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md` 的 `## Status Update` **2026-11-17（追加）**段第 5 条（同族缺口登记）
> **上游**: 台账 Entry **#461**（登记本 backlog improvement）· **#453**（F-3 根因订正 = in-band credit 泄漏）· **#463** / **#464** / **#465**（同源另一面 `cpptlm-router-local-port-credit-exhaustion` 的实施 / 归档 / 验收）
> **improvement**: `.rddf/improvements/cpptlm-nictlm-credit-flit-filter.md`（5 段 + AC-01~AC-05 + G-1 guard）
> **同根因 · 已修复的一侧**: change `cpptlm-router-credit-flit-leak-to-consumer`（**已归档** `2026-11-16-…`）—— 修 `CohIfNICTLM` 侧；本 change 修 **`NICTLM`** 侧
> **⛔ 不因 `ADR-X.16` 而消解**: `ADR-X.16` 的 D1 管的是 router 的**下行** flow control（`has_credit(LOCAL,·)` 恒真）；本缺口的路径是 **router → LOCAL 上游**的**上行**（`return_credit_to_upstream` 把 credit 发回 `resp_out[LOCAL]`）⇒ **不受 D1 影响**，两者独立

---

## Why

### 第 1 条 —— 缺陷点：`NICTLM` 全文**零** credit 过滤（实测）

`src/tlm/nic_tlm.cc` **全文零 `credit` / `CATEGORY_CREDIT` 命中**（grep 实测）：

```cpp
// src/tlm/nic_tlm.cc:80-95
bool NICTLM::handle_net_resp() {
    if (!net_resp_in_.valid() || !net_resp_in_.ready()) { return false; }
    bundles::NoCFlitBundle flit = net_resp_in_.data();
    net_resp_in_.consume();

    ++stats_flits_received_;      // ← ① credit 也算「收到」

    if (reassemble(flit)) {       // ← ② credit 被当数据 flit 重组
        ++stats_packets_received_;
    }
    return true;
}
```

`reassemble` 在 `:182` 写 `pe_resp_out_.write(resp)` ⇒ credit flit 到达即产出**垃圾 `pe_resp_out`**。

### 第 2 条 —— 三面后果

① `flits_received` **虚增**；② `packets_received` **语义污染**；③ 向业务侧（`pe_resp_out_`）灌入**垃圾响应**（字段多为 0 / 默认值）。

### 第 3 条 —— 与 `CohIfNICTLM` 侧**同根因**

`RouterTLM` 对**每个**转发成功的入向 flit 回 credit，而 `reverse_port(LOCAL) == LOCAL`（`include/tlm/router_tlm.hh:229`）⇒ 经 LOCAL 口进入 router 的 flit，其 credit 沿 `resp_out[LOCAL]` **回流到挂在 LOCAL 上的下游**。挂在 LOCAL 上的终端消费者有两个：`CohIfNICTLM`（**已修**，见 `cpptlm-router-credit-flit-leak-to-consumer`）与 `NICTLM`（**未修** ⇒ 本 change）。

### 第 4 条 —— 受影响拓扑（`ADR-X.16` F-6 已列全 `NICTLM` 挂 LOCAL 的实例）

`mesh_2x2_tlm` / `mesh_4x4_tlm` / `mesh_4x4` / `nic_router_nic` / `test_e2e_simulation` / `test_nic_side_rules` / `test_noc_mesh_binding_semantics`（载体：`test/test_complex_topologies_from_config.cc` · `test/test_e2e_simulation.cc` · `test/test_noc_mesh_binding_semantics.cc`）。

### 第 5 条 —— 与 `ADR-X.16` **独立**（顺序依赖澄清）

`ADR-X.16` D1 = 「LOCAL 端口**不参与 downstream credit 流控**」⇒ 管 router 的**下行**（`has_credit(LOCAL, vc)` 恒真 / `consume_credit(LOCAL, vc)` 不扣减）。
本缺口的路径是 **router → LOCAL 上游**的**上行**（`return_credit_to_upstream` → `resp_out[LOCAL]`）⇒ **不受 D1 影响**。
⇒ ⛔ **不得**假定「`ADR-X.16` 落地后本缺口自动消失」；两者**独立**，可并行推进。

---

## What Changes

1. **消费者侧过滤（主修，与 `CohIfNICTLM` 同款）**：`NICTLM::handle_net_resp` 在消费 flit 前判 **`flit.flit_category.read() == bundles::NoCFlitBundle::CATEGORY_CREDIT`** ⇒ 丢弃该 flit。
   **判据必须用 bundle 字段**（传输无关；`Packet::type` 在 `InputStreamAdapter<NoCFlitBundle>` 路径上**不一定可取**）。
2. **丢弃的语义**：**不**计入 `stats_flits_received_`、**不**计入 `stats_packets_received_`、**不**进入 `reassemble`、**不**写 `pe_resp_out_`。
3. **观测面**：新增标量 `credits_dropped`（`tlm_stats::Scalar`）+ 访问器 `credits_dropped()`，与 `CohIfNICTLM` 侧**对称**（后者已有 `credits_dropped()` / `stats_credits_dropped_`）。

### ⛔ 明确不做

| # | 不做 | 理由 |
|:--:|---|---|
| 1 | **不改 router 侧**（`RouterTLM` / `router_tlm.hh` / `reverse_port` / credit 的产生与回传语义） | credit 的产生与回传属**链路层**；本 change 是「消费者必须健壮」的一般性要求，与已归档 change 的边界一致 |
| 2 | **不改 `include/bundles/**`** | bundle 是传输契约；`CATEGORY_CREDIT` 语义已存在且已被 `CohIfNICTLM` 侧使用 |
| 3 | **不改 `include/abi/**`**（23 ABI 冻结） | 本 change 不触及 driver 可见面 |
| 4 | **不改 `port-type-system` spec** | 本 change 只终止「credit 被当数据消费」，**不**改变 `NICTLM` 的端口角色（`PE` / `NETWORK`，bundle `CACHE_REQ` / `NOC_FLIT`） |
| 5 | **不改 `CohIfNICTLM`**（已修） | 本 change 只补 `NICTLM` 侧 |
| 6 | **不修 `buffer_size` param 不生效** | 同族但独立缺陷（`ADR-X.16` 关联章第 1 条）⇒ 另立 change；且它是 `constexpr` 数组维度问题，成本远高于本项 |
| 7 | **⛔ 不在 `bundles::NoCFlitBundle` 增 `is_credit()`** | 判据**逐字复制** `CohIfNICTLM` 的 3 行即可；抽到 bundle 层会动 `include/bundles/**`（传输契约，⛔ 零 diff 面）⇒ 超出本 change 授权 |
| 8 | **⛔ 不在 `framework/` 的 adapter 层做过滤** | `DualPortStreamAdapter::process_net_response_input` 虽是 ingress 汇聚点（可一处覆盖全部消费者），但那会**改变 `CohIfNICTLM` 与未来消费者的边界** ⇒ 超出本 change 授权，**须另立 change** |
| 9 | **⛔ 不为「对拍方便」补访问器** | `flits_received` / `packets_received` 继续经 `get_stats_group()` 标量读取（与既有 `test_noc_mesh_binding_semantics.cc` 的 `scalar_of()` 手法一致）⇒ 本 change 的**唯一**新增公开 API 是 `credits_dropped()` |

---

## Acceptance

| # | 判据 | 可验收点（机器可判） |
|:--:|---|---|
| **AC-01** | 注入 credit ⇒ 三面零变化（**带判别力的序列**） | 先注入 1 个真响应 flit（`flit_type=FLIT_HEAD_TAIL` · `flit_category=CATEGORY_RESPONSE` · `tid=T`）⇒ 断言 `flits_received==1` · `packets_received==1` · `pe_resp_out()` 有效且 `transaction_id==T` ⇒ `clear_valid()` ⇒ 再注入 credit flit（**同 `tid=T`** · `flit_type=FLIT_HEAD_TAIL` · `flit_category=CATEGORY_CREDIT`）⇒ 断言三面**保持** 1/1/无输出 且 `credits_dropped()==1`。<br>⚠️ **⛔ 不得写成「注入前计数为 0 ⇒ 注入后仍为 0」**（空断言陷阱，先例见 `test/test_cep_mesh_e2e.cc` 的明文教训） |
| **AC-02** | 端到端零 credit 污染（**存在性 + 计数正确**） | 在**测试自建**的最小「NIC → Router → NIC」带流量拓扑里断言 ① `credits_dropped() > 0`（credit 确实抵达该 NIC ⇒ 避免 `0==0` 空断言）；② `flits_received` == **测试自己注入的响应 flit 数**。<br>⚠️ **⛔ 不用 `mesh_2x2_tlm` / `mesh_4x4_tlm`** 作判据拓扑：其 4 个 NIC **全无 `address_regions`** 且**无 memory 模块** ⇒ `lookup_node()` 恒返回 0 ⇒「真实事务数」在该拓扑下**不可定义**（该 config 仅作**装载回归**） |
| **AC-03** | 既有 `NICTLM` 拓扑回归全绿 | `test_complex_topologies_from_config.cc` · `test_e2e_simulation.cc` 的 nic 用例 · `test_nic_side_rules` · `test_noc_mesh_binding_semantics.cc` · 本 change 触达标签 `[e2e][nic][router][noc]` 全量 + `ctest` 全量。<br>⚠️ `[pcie]`（PCIe EP 测试集）与 `NICTLM` **零交集** ⇒ **不列入** |
| **AC-04** | 判据形态与 `CohIfNICTLM` 侧一致（**可对拍**） | 同一 credit flit 注入两侧 ⇒ 丢弃行为一致（都不计数 / 都不产出业务流 / 丢弃数 == 1）。⚠️「同款」= **逐字复制判据**，⛔ **不是**共享代码（见「明确不做」第 7/8 行） |
| **AC-05** | 零 diff 面（**可复现判据**） | `git diff --name-only <base>..HEAD` 的结果集合 ⊆ `{src/tlm/nic_tlm.cc, include/tlm/nic_tlm.hh, test/test_nictlm_credit_filter.cc, openspec/changes/cpptlm-nictlm-credit-flit-filter/**}`。<br>零 diff 面 = `src/tlm/router_tlm.cc` · `include/tlm/router_tlm.hh` · **`include/framework/**`** · **`configs/**`** · `include/abi/**` · `include/bundles/**` · `include/tlm/cep/cohif_nic.hh`。<br>⚠️ `include/tlm/nic_tlm.hh` 的**允许改动仅限**：新增标量 `stats_credits_dropped_` + 新增访问器 `credits_dropped()`；**其余签名零改动**。<br>**执行时点**：由 builder 在归档前（归档前 review）执行并把命令输出贴进 change |
| **AC-06** | 同 `tid` 在途包**不被劫持** | `reassemble` 以 `transaction_id` 单键查 `pending_packets_` ⇒ credit flit 若被当作 HEAD 处理会**新建 / 污染在途包状态**（比「垃圾响应」更严重）。判据：注入真 `FLIT_HEAD`（`tid=T`，多 flit 包）→ 注入 credit（`tid=T`）⇒ 断言 `pe_resp_out()` **无输出** 且在途包状态未被 credit 改写 → 再注入同 `tid=T` 的 `FLIT_TAIL` ⇒ 断言**恰好一条**响应 |


**附加检查（guard，不占验收编号）**：G-1 `credits_dropped()` 与 `CohIfNICTLM::credits_dropped()` 的**观测语义一致**（同名同义，可直接对拍）。

---

## Capabilities

| # | 能力 | 新增 `INV-CEP-*` |
|:--:|---|:--:|
| C-1 | 通用网卡（`NICTLM`）不再被 in-band credit 污染（计数 + 重组 + 业务流 + **在途包状态**四面） | ❌ 0 |
| C-2 | 与 `CohIfNICTLM` 侧形成**同款可对拍**的 credit 过滤语义（同用 bundle 字段判据 + 同名观测面 `credits_dropped`） | ❌ 0 |

> **限界（⛔ 不得过度解读）**：本 change 只保证**消费者不被污染**；credit 仍按既有链路语义回流并占用带宽（`RouterTLM` 侧语义**不变**），`NICTLM` 仍**不归还** credit。另一面（router 下行流控）见 `ADR-X.16` 与 capability `router-local-port-flow-control`。**「补完两个消费者」不等于「credit 泄漏已根治」** —— `ADR-X.16` F-6 列出的第三个候选 `ComputeUnitTLM`（`apu_soc_full`）同样需要自我防御（该 config 当前不可装载，故本轮不覆盖）。


---

## Out of Scope

- **router 侧的 credit 产生 / 回传语义**（`RouterTLM` / `BidirectionalPortAdapter` / `reverse_port`）—— 链路层职责，本 change 只要求消费者健壮。
- **`buffer_size` param 不生效**（`ADR-X.16` 关联章第 1 条）—— 独立缺陷，另立 change。
- **`ADR-CEP-11` G-3**（IC3 变更留痕无自动闸）—— 治理面，另立 change。
- **`NICTLM` 的协议语义** —— `NICTLM` 仍是通用网卡，本 change 不给它加任何协议行为。
