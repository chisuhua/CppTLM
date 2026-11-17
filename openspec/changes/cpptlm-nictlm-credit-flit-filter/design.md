# design: cpptlm-nictlm-credit-flit-filter

> 配套 `proposal.md`（Why / What Changes / Acceptance / Capabilities / Out of Scope）。
> 决策源：无独立 ADR —— `ADR-X.16` 的 `## 关联` 章「相邻独立缺陷」第 2 条 + `ADR-X.15` 的 `## Status Update` 2026-11-17 追加段第 5 条。
> 同根因已修的一侧：`cpptlm-router-credit-flit-leak-to-consumer`（**已归档**）—— 本 change 是它的 `NICTLM` 侧对偶。

## D-1 — 实现形态

| 落点 | 改动 |
|---|---|
| `src/tlm/nic_tlm.cc` · `NICTLM::handle_net_resp` | 在 `consume()` 之后、`++stats_flits_received_` 之前插入 credit 判定：`flit.flit_category.read() == bundles::NoCFlitBundle::CATEGORY_CREDIT` ⇒ `++stats_credits_dropped_` 并 **`return true;`**（flit **已被消费** ⇒ 返回值须与既有 `:94` 语义一致；⛔ 不得写 `return false` —— 将来若有人按返回值做节拍统计即成 bug） |
| `include/tlm/nic_tlm.hh` · 统计面 | 新增 `tlm_stats::Scalar& stats_credits_dropped_`（标量名 `credits_dropped`，unit/description 与 `CohIfNICTLM` 侧**逐字一致**）+ 访问器 `std::uint64_t credits_dropped() const`（**对称**于 `CohIfNICTLM::credits_dropped()`） |

**判据形态必须与 `CohIfNICTLM` 侧逐字一致**（AC-04）：两者都判 **bundle 字段** `flit_category == CATEGORY_CREDIT`，⛔ **不用** `Packet::type`（该字段在 `InputStreamAdapter<NoCFlitBundle>` 路径上不一定可取 —— `process()` 只把 payload 反序列化进 bundle，`Packet*` 在模块层不可见）。

> ⚠️ **「同款」= 逐字复制判据，⛔ 不是共享代码**：
> - ⛔ 不在 `bundles::NoCFlitBundle` 增 `is_credit()`（动 `include/bundles/**` ⇒ 传输契约，零 diff 面）
> - ⛔ 不在 `framework/DualPortStreamAdapter` 做过滤（那是 ingress 汇聚点，可一处覆盖全部消费者，但会**改变 `CohIfNICTLM` 与未来消费者的边界** ⇒ 须另立 change）
> - ⛔ 不为「对拍方便」补 `flits_received()` / `packets_received()` 访问器 —— 本 change 的**唯一**新增公开 API 是 `credits_dropped()`；其余统计继续经 `get_stats_group()` 标量读取


**零配置面变更**：不动 JSON schema、不动 `ModuleFactory`、不动任何 `configs/*.json`、不动 `include/bundles/**`、不动 `include/abi/**`。

## D-2 — 测试装置与观测面

| 项 | 规定 | 依据 |
|---|---|---|
| **装置（单元）** | **裸 `NICTLM`** + 直接向 `net_resp_in()` 注入：`nic.net_resp_in().data() = flit; nic.net_resp_in().set_valid(true);` 然后 `nic.tick()` | `NICTLM::net_resp_in()` 是 `cpptlm::InputStreamAdapter<NoCFlitBundle>`（`include/tlm/nic_tlm.hh:119`）⇒ 无需新拓扑、无需新 sink |
| **注入 flit（真实形状）** | `flit_category = CATEGORY_CREDIT` · `flit_type = FLIT_HEAD_TAIL`（与 `fill_credit` 产出**同形**）· `dst_node = hops = 0`（credit flit 的默认值）· `transaction_id` **复用前一个真 flit 的 tid** | ⛔ **不是**「其余字段为默认值」—— 默认 `flit_type` 是 `0 = FLIT_HEAD`，与真实 credit 的 `FLIT_HEAD_TAIL` 在 `reassemble` 里走**不同分支**（`FLIT_HEAD` ⇒ `is_head()` ⇒ 新建 PendingPacket 且 `pending_packets_` 留一条**永不清除**的条目；`FLIT_HEAD_TAIL` ⇒ `is_tail()` ⇒ 写后 `erase`） |
| **两种形状都测** | 除真实形状（`FLIT_HEAD_TAIL`）外，**另测**默认形状（`FLIT_HEAD`）⇒ 两者**都必须**被丢弃 | 防实现只处理单一分支 |
| **观测（计数）** | 读 `get_stats_group()` 里的标量：`flits_received` / `packets_received` / `credits_dropped`（标量名断言见下） | `src/tlm/nic_tlm.cc:14-20` 的 `addScalar` 命名；读取手法照既有 `test_noc_mesh_binding_semantics.cc` 的 `scalar_of()` |
| **观测（标量名）** | `get_stats_group()->stats().count("credits_dropped") > 0` | G-1 的「**同名**」靠这个**字符串**成立（不是靠访问器）⇒ 必须有机器锚点 |
| **观测（业务流）** | `nic.pe_resp_out().valid()` ⇒ 若为真则读 `data()` 并 `clear_valid()` | `OutputStreamAdapter::write()` **不清** valid（`include/framework/stream_adapter.hh:111-126`）⇒ 必须显式 `clear_valid()` |
| **观测（丢弃数）** | `nic.credits_dropped()`（新增访问器） | 与 `CohIfNICTLM` 侧对称 |
| **端到端（AC-02）** | **测试自建**最小「NIC → Router → NIC」带流量拓扑（**⛔ 不用** `mesh_2x2_tlm` / `mesh_4x4_tlm`：其 4 个 NIC **全无 `address_regions`** 且**无 memory 模块** ⇒ `lookup_node()` 恒返回 0 ⇒「真实事务数」不可定义；该 config 仅作**装载回归**） | 避免 `0==0` 空断言：须断言 `credits_dropped() > 0`（credit 确实抵达） |
| **对拍（AC-04 / G-1）** | 同一 credit flit 分别注入 `NICTLM` 与 `CohIfNICTLM` ⇒ 两侧「不计数 + 不产出业务流 + 丢弃数 == 1」**一致** | 两侧判据同款 ⇒ 行为应当一致。⚠️ 一侧读**访问器**、另一侧读**标量**（`NICTLM` 无 `flits_received()` 访问器）⇒ 对拍断言按各自可得的面写 |

**注入节拍**：单元用例每周期**最多**注入 1 个 flit（`net_resp_in().valid()` 为真时不重复注入）⇒ 不会因超速而静默丢弃。


## D-3 — 数值变化的处理纪律（AC-03 的反向）

修复会改变**既有**断言的数值（`flits_received` / `packets_received` 虚增被消除 ⇒ 数值**变小**）。处理纪律：

1. 任何 `flits_received` / `packets_received` / `recv` 的变化**必须**在 `design.md` / tasks 里逐条列出「**旧值 → 新值 → 为何是本修复的必然结果**」；
2. ⛔ **禁止就地改 `REQUIRE` 常量**（禁止静默重基线）；
3. 若既有断言的**语义**依赖「计数包含 credit」（即断言本身就是基于缺陷写的），该断言**必须改判据**而非改常量 —— 并在 change 内显式登记。

**核对责任（⛔ 纪律须有锚点，否则是空文）**：D-3 第 1 条的「旧值 → 新值」清单**由 builder 在归档前执行并贴进 change**，证据形式 = 改动前后的标签级计数对照命令与输出（例如 `./build/bin/cpptlm_tests "[e2e][nic]" | tail -1` 的前后两值），**不**接受「读码确认」这类自我声明。⚠️ 既有 `test_noc_mesh_binding_semantics.cc` 的 `ni1.flits_received` 只是 **dump**（无断言）⇒ 该处**无机器锚点** ⇒ 必须**新增**断言，不能靠既有 dump 充当证据。

## D-4 — spec 落点与真源同步

| 动作 | 内容 |
|---|---|
| **新建** capability | `openspec/specs/cep-nictlm-credit-filter/` —— Requirement `nictlm-credit-flit-filter`，含 3 个 Scenario：丢弃 credit flit / 丢弃不得影响计数与业务流 / 观测面暴露丢弃计数 |
| **⛔ 不改** 既有 spec | `openspec/specs/port-type-system/spec.md` —— 本 change **不**改变 `NICTLM` 的端口角色（`PE` / `NETWORK`，bundle `CACHE_REQ` / `NOC_FLIT`）⇒ 无需 MODIFIED |
| **⛔ 不改** `cep-cohif-network-adapter` | 该 spec 的「已知限制」句已由 `router-local-port-flow-control` 承接（Entry #464 归档时合并）⇒ 与本 change 无关 |
| **对齐** | `openspec/specs/cep-cohif-network-adapter/spec.md` 的「网络 ingress SHALL 丢弃 credit flit」Scenario 与本 spec 的 Scenario **同款措辞**（bundle 字段判据）⇒ 两 capability 可对拍 |

## D-5 — 已知限制（不在本 change 解决）

1. **`buffer_size` param 不生效**（`ADR-X.16` 关联章第 1 条）—— 独立缺陷，`constexpr` 数组维度问题，成本远高于本项 ⇒ 另立 change。
2. **credit 仍会回流**（router 侧语义不变）—— 本 change 只保证**消费者不被污染**；credit 本身仍占用链路带宽。
3. **`NICTLM` 仍不归还 credit** —— 它不是 credit-aware 实体（单向 `InputStreamAdapter`）⇒ 若未来有「LOCAL 挂 router」的拓扑，其 credit 账仍会失衡（per `ADR-X.16` D5 的演进 seam）。
4. **`ADR-CEP-11` G-3**（IC3 变更留痕无自动闸）—— 治理面，另立 change。
5. **adapter 层静默丢包（既有缺陷，本 change 看不到）** —— `DualPortStreamAdapter::process_net_response_input` 在 `resp_in.valid()` 为真时**整包静默丢弃且 `Packet` 不 release**（`include/framework/dual_port_stream_adapter.hh:237-243`）⇒ 那部分 credit **永远进不了** `NICTLM` ⇒ `credits_dropped` **看不到**它，且存在既有 `Packet` 泄漏。⛔ 不在本 change 修（动 `framework/` 超出授权）。
6. **`MAX_PENDING_PACKETS = 16` 声明未被强制**（`include/tlm/nic_tlm.hh` 的常量 + `src/tlm/nic_tlm.cc` 的 `reassemble` 路径不校验上界）⇒ credit 泄漏**曾**加剧在途包表增长；本 change 减少其触发源，但**不**修该上界。
7. **`ComputeUnitTLM`（`apu_soc_full`）未覆盖** —— 该 config 因 `ComputeUnitTLM` 未注册而**不可装载**，且无反向 credit 路由 ⇒ 跳过（但它是「补完两个消费者 ≠ 根治」的第三个候选，见 proposal 的限界段）。
8. **`MemoryTLM`** —— 仅 Python fixture ⇒ 跳过。

### D-5.1 — 明确**不做**的边界情况（理论存在 / 结构上不可达）

| 情形 | 为何不做 |
|---|---|
| `vc_id` 非法 | `NICTLM` 全程**不读** `vc_id`（只有 `packetize` 写、`reassemble` 从不读）⇒ 无行为可断言 |
| `net_resp_in_.ready()` 为假 | 裸 `NICTLM` 的 `ready()` **恒真**（`stream_adapter.hh` 默认 `true`；`process()` 每次也置 `true`）⇒ 无消费者会置假 ⇒ 该分支在现行实现下**不可达**（判据沿用既有 `:81`） |
| 同周期 credit 在 data 之后 | `NICTLM::tick()` 每周期只 `handle_net_resp()` 一次；adapter 层在 `valid()` 为真时**直接丢包**（见 D-5 第 5 条）⇒ 同周期第二个 flit **到不了模块层** ⇒ 该用例实际测的是 adapter，**不在本 change 范围** |

