# L2 运行期操作迹采集 —— 架构裁决（立项门①）

> **性质**：架构裁决记录（**不是** ADR；不改任何已签批 ADR 正文）—— 满足 `.rddf/improvements/cep-memory-model-runtime-trace.md` 的 **What Changes ⑥ 立项门①**（「采集层三候选的选点结论 + 不产 run 纪律边界 + 指针回填疑点，合并为一份请求」）。
> **授权链**：`ADR-CEP-22` SU(2026-11-25) ⑥ 的 **L2** → 台账 Entry **#549**（improvement 立项）→ 本报告。
> **日期 / 作者**：2026-11-25 · architect
> **审查**：oracle → critic（见 §8 状态）

---

## 0. 问题陈述（立项门①的三问）

| # | 问 | 本报告的回答位置 |
|:--:|---|---|
| **Q1** | 采集点选在**哪一层**？（ChStream 流级 / 事务层 / NoC flit 级） | §2 实证 · §3.1 裁决 |
| **Q2** | 与「**不产 run**」纪律的边界是什么？ | §4 |
| **Q3** | `ADR-CEP-22:135` 的「指向**后续单元**」**是否需回填**？ | §5 |

---

## 1. 需求侧：采集产物必须满足什么（从判定面**倒推**，不是从采集面正推）

判定面的输入契约（`include/tlm/cep/memory_model_trace.hh:43-84`）：

```cpp
struct Op   { id, kind(Read|Write), thread, domain, addr, value };
struct Trace{ id, family, source_litmus, anchor_id, domains[{id,instance}],
              ops[], rf[(read_id, write_id)], co_choices[{addr, candidates[{id,writes[]}]}] };
```

**四条硬约束**（缺任一 ⇒ 迹不合法，与采集实现无关）：

| # | 约束 | 机检位置 | 后果 |
|:--:|---|---|---|
| **C1** | `ops[].thread` 必须能表达**程序序**（线程内 op 的先后） | 判定按 thread 内序列建 `ppo` | 无 thread ⇒ 无法建程序序边 |
| **C2** | `ops[].domain` **必填**且须解析到已声明实例 | `memory_model_trace.hh:241` 起 · `unbound-domain` / `no-domain-instance` | 无 domain ⇒ `invalid` |
| **C3** | **`rf` 是单值映射** `(read_id, write_id)` —— ⛔ **没有候选集语义** | `compound_graph_falsifier.hh:243-249`（缺/错 ⇒ `rf-target-missing`） | **rf 不唯一 ⇒ 无法产出合法迹** |
| **C4** | `anchor_id` 必须 ∈ `anchor_allowlist()`（5 项） | `memory_model_trace.hh:149-157` · `:262-269` | 越界 ⇒ `malformed-trace` |

⇒ **采集最小产出集** = `{thread, domain, addr, kind, value}` × 每个操作 **+ `rf`** **+ `co_choices`**。

⚠️ **C3 是本报告的核心**：`rf` 语义是「这次读**看到了哪一个写**」。它要求采集点**能观察到内存/缓存/目录的实际状态**，或能在事后由「读到的值 + 地址 + 写历史」**唯一**反推。**值匹配不唯一时（同地址同值多写），现有格式无法表达** ⇒ 这是运行期采集的真实瓶颈，**不是「能不能记录操作」**。

---

## 2. 三个候选层的**可采性实证**

（以下均为仓内实测，非推断；`path:line` 见各行）

### 2.1 ChStream 流级 —— ⛔ 无观测挂点

| 事实 | 证据 |
|---|---|
| 注入口只有 `set_stream_adapter()`（单/多端口）与 `attach_protocol_bridge()`（默认 false，仅协议引擎） | `include/core/chstream_module.hh:38` / `:45` / `:90` |
| `StreamAdapterBase` 接口**无**任何 trace callback / 监听者注册 / tap | `include/core/stream_adapter_base.hh:24-91` |
| `ChStreamTargetPort::recvReq` 是**写死路径**（交 adapter + 归还 `PacketPool`），无旁路 | `include/core/chstream_port.hh:67-74` |

### 2.2 事务层（端口） —— ⚠️ 唯一**统一**观测面，但是 `final`

| 事实 | 证据 |
|---|---|
| 全仓 `src/` **无** `b_transport` / `transport_dbg` / `nb_transport` ⇒ **不存在统一事务钩子** | 全仓 grep |
| **唯一统一观测面** = `MasterPort::send`（可 override） | `include/core/master_port.hh:57-65` |
| ⚠️ **`SlavePort::recv` 是 `final`（`:41`）· `recvReq` 是纯虚（`:47`）** ⇒ **无统一观测面**；`recvReq` 只支持**逐类 hook**（局部采集可用 · 统一 tap 不可） | `include/core/slave_port.hh:41` / `:47` |
| 现有唯一统一数据 = `PortStats` 计数（req_count / byte_count / credit） | `include/core/port_stats.hh:8` |
| 可挂数据的手段 = TLM extension（逐 payload 附加） | `include/ext/transaction_context_ext.hh:22/31/50` |
| ⚠️ `tlm_command` **只有 READ / WRITE**（无 ATOMIC）；`Packet.cmd` 同 | `include/core/packet.hh:16-20`（`CmdType` 枚举 `CMD_READ` / `CMD_WRITE` 只有两项）· `:44`（字段） |

### 2.3 NoC flit 级 —— ⛔ 字段不足

| 事实 | 证据 |
|---|---|
| `NoCFlitBundle` **16 字段**（`transaction_id / src_node / dst_node / address / data / size / vc_id / flit_type / flit_index / flit_count / hops / flit_category / src_port / is_write / is_ok / error_code`） | `include/bundles/noc_bundles_tlm.hh:26-50` |
| ⛔ **无 `thread` / `domain`** 字段 | 同上 |
| 已有 flit 级设施 = **聚合计数器**（无逐 flit 记录） | `include/tlm/router_tlm.hh:319-326` · `include/tlm/nic_tlm.hh:161-167` · `include/tlm/link_tlm.hh:87` · `include/tlm/cep/cohif_nic.hh:147-150` |

### 2.4 既有运行期设施（可否复用）

| 设施 | 粒度 | 能否承载操作级 | 证据 |
|---|---|---|---|
| `include/metrics/`（Scalar/Average/Distribution/StatGroup + StatsManager + Reporter） | **周期聚合快照** | ⛔ 不能（无逐事件） | `include/metrics/streaming_reporter.hh:47` |
| `TransactionTracker` / `TransactionRecord`（`type` 已含 `"READ"/"WRITE"/"ATOMIC"` · `record_hop` · extension `trace_log`） | **逐事务**（最接近） | ⚠️ 骨架可借，但 ① **`src/` 零调用**（只在 `examples/` `test/`）② `TransactionRecord` **无显式 `addr`**（只存 payload 指针） | `include/framework/transaction_tracker.hh:17-34` / `:131` / `:140` |
| `DebugTracker`（`StateSnapshot` / `record_state_transition`） | 错误 + 一致性状态迁移 | ⛔ 不是操作迹 | `include/framework/debug_tracker.hh:44` / `:179` |

### 2.5 可采性矩阵（**本报告的核心证据表**）

| 所需字段 | 流级 | 事务/端口层 | flit 级 | 有状态的存储/目录/tracker |
|---|:--:|:--:|:--:|:--:|
| `addr` / `kind` / `value` | ⚠️ 需 adapter 内插桩 | ✅ `payload` + `Packet` | ✅ `address` / `is_write` / `data` | ✅ |
| `thread`（程序序） | ⛔ 无身份概念 | ⛔ 无身份概念（需发起端打标） | ⛔ 无 | ⚠️ 需显式传入 |
| `domain` | ⛔ | ⛔（需发起端声明） | ⛔ | ⚠️ 需显式传入 |
| **`rf`（C3）** | ⛔ **看不到内存状态** | ⛔ 只能靠「响应 data + 值匹配」反推 ⇒ **不唯一时不可表达** | ⛔ | ✅ **唯一可靠来源** |
| 挂钩代价 | 改 adapter / 端口 | ⚠️ **`recv` 是 final ⇒ 改核心基类** | 改 NIC/Router | 逐组件加 hook（影响面可控） |

---

## 3. 裁决

### 3.1 **D1 —— 采集点 = 「CEP 关心路径上的少数有状态组件」的局部采集；⛔ 不做全仓统一 tap**

**理由**（按可采性矩阵）：

1. **只有有状态组件能产 `rf`** —— 而 `rf` 是 C3 的硬约束。流级 / flit 级 / 端口层都**看不到内存状态**，只能靠值匹配反推 ⇒ 违反 C3（不唯一时无法表达）。
2. **全仓统一 tap 要改核心框架** —— `SlavePort::recv` 是 `final`（`slave_port.hh:41`）⇒ **统一**采集只能改 `include/core/slave_port.hh` 基类，波及**全仓所有从端口**与 **209 条 ctest**；且即便采到「请求/响应」，仍缺 thread / domain / rf（矩阵见 §2.5）⇒ **代价高、收益不完整**。（⚠️ 注意：`recvReq` 是**纯虚**（`:47`）⇒ **局部**采集可在具体子类 override `recvReq` 加 hook，不需要改基类 —— 这正是 D1 可行而「统一 tap」不可行的分界。）
3. **局部采集的边界可枚举** —— 只覆盖 CEP 的 litmus 场景所需组件（协议引擎边界 / NIC / 目录或 tracker / 存储边界），影响面 = 这些组件自身，可逐步扩展。

⚠️ **D1 的承重前提（须显式登记 · 否则 D1 失效）**：**v1 范围内（`mesif` 切片先行）CEP litmus 的发起者为协议引擎实例**（引擎 = 该域的 Ordering Point 代理）—— 证据：现役 6 条迹的 `domain` 全部绑定协议实例（`test/cep_memory_model_traces.json:13/17` 等），且档 2 的 `mesif` 载体已用 `protocol-engine-observation` 形态（`docs/architecture/07-coherence-exploration-platform.md:870`）。⇒ **若未来出现非引擎发起者**（DMA 直连 / 流模块自发请求），本裁决的 D1 选点**不成立**，须重新裁决。

### 3.2 **D2 —— 三个**不可回避**的缺口（v1 必须显式限定，⛔ 不得静默降级）

| 缺口 | v1 限定 | 若限定的代价（诚实登记） |
|---|---|---|
| **thread 身份** | **一线程 = 一个发起组件实例**（组件 id 即 thread id） | 组件内多线程并发 ⇒ 程序序不可分 ⇒ 该场景**不可采**（跳过并计数，⛔ 不猜） |
| **domain 标注** | 由**发起端显式声明**（经 extension 或配置映射） | `ADR-CEP-22` 决策 1 的承重假设**仍在**（域标识忠实代表性无机器判据）；采集不消除它 |
| **`rf` 唯一性（C3）** | **三态判定（⛔ 不得合并）**：每次读的返回值在本采集器**写历史**中的候选数 —— **0 ⇒ 迹整体丢弃**（写历史不完备 ⇒ 该迹不可信，⛔ **不是**「跳过这个 rf」）· **≥2 ⇒ rf 不唯一 ⇒ 跳过并计数** · **==1 ⇒ 输出**。⛔ 不改迹格式、⛔ 不新增原因码 | 覆盖率下降；但**伪造单值 rf 会让判定面产出假结论**（比覆盖率低更严重） |

### 3.2bis **`rf` 正确性的完备性前提** —— 「**假唯一**」比「不唯一」更危险（MAJOR）

> ⚠️ **失败模式**：局部采集 ⇒ **写历史可能不完备** ⇒ 某次读的真实来源写**未入采** ⇒ 值匹配在**残缺历史**里「**恰好唯一**」⇒ 产出**错误的单值 `rf`** ⇒ falsifier 拿错 `rf` 判出**假结论**。
> ⇒ 这种失败**无法由「跳过不唯一」检测**（它看起来是唯一的）⇒ 必须在机制层堵。

| # | 纪律 | 理由 | 可机检 |
|:--:|---|---|---|
| **W1** | **CEP 关心路径内部**的写产生点必须**全** hook（⚠️ **⛔ 不是全仓** —— 与 D1「局部采集」一致：**局部 = 路径范围可枚举**，**完备 = 在该范围内不漏写**）；**读**路径可不全 | 写历史完备性是 `rf` 正确性的**承重前提**；缺它则「跳过不唯一」形同虚设 | 路径内写产生点清单 vs 已 hook 列表（可枚举对账） |
| **W2** | 候选数 **0 ⇒ 整迹丢弃**（⛔ 不是跳过单个 `rf`） | 0 候选 = 写历史不完备的**直接证据** ⇒ 该迹**任何** `rf` 都不可信 | 丢弃计数 |
| **W3** | 首件采集**必须报告**三个率：`输出率 / 不唯一跳过率 / 零候选丢弃率` | 让「假唯一 / 零候选」可观测（否则切片质量无法评估） | 报告文件存在 + 三数齐 |

### 3.3 **D3 —— ⛔ 否决项（逐条给理由）**

| 否决 | 理由 |
|---|---|
| **流级作主采集面** | 无观测挂点（§2.1）+ 采不到 `rf`（§2.5） |
| **flit 级作主采集面** | 无 `thread` / `domain` 字段（§2.3）+ 采不到 `rf`；且逐 flit 会引入与内存模型无关的传输噪声 |
| **全仓统一端口 tap** | `SlavePort::recv` 是 `final` ⇒ 侵入核心基类；且收益不完整（§2.5） |
| **复用 `TransactionTracker` 作为采集器本体** | `src/` 零调用（未接入）+ 无 `addr` 字段 ⇒ 只能作为**接口骨架参考**，不是现成载体 |
| **扩展 `Trace::rf` 为候选集** | 依赖单值 `rf` 的面太多：① `ADR-CEP-22` 决策 4 的输入面（`:82`「`rf`（读 → 它读到的写）」，**单值语义**）② 决策 6 双图折衷（ADR 不可变）③ `openspec/specs/cep-compound-graph-falsification/spec.md` 的 `fr` 派生规则（**M1 冻结面**）④ 6 条现役语料的兼容性 ⇒ 若确有必要**须另立裁决**（真实改动量 = ADR SU 取代 + spec 冻结链 + 语料迁移），⛔ 不在 L2 内夹带。⚠️ **⛔ 不得援引 `R-2e` 作依据** —— `R-2e` 锁的是 `combination_contract` 的 **schema 必填策略**（`.rddf/reviews/cep-invariant-batch2-adjudication-2026-11-21.md:290` §五），而 `Trace::rf` 是**迹格式**（C++ struct + 语料 JSON），**不在其字面范围** |

### 3.4 v1 切片的**最小可交付形态**（供立项）：

> 在**一个** CEP 关心路径上（建议从 `mesif` 族起步 —— 它已有 `protocol-engine-observation` 载体，见 `07:866-871` 的档 2 载体表（`mesif` 行在 `:870`）），加**显式 hook** 产出 `{ops, rf, co_choices, domains, anchor_id}`，`rf` 不唯一即跳过并计数；产物**经 falsifier 判定**，⛔ 不由采集器给结论。

---

## 4. Q2 —— 「不产 run」的纪律边界

**先纠正一处可能的误读**：两处「不产 run」**不是同一件事**。

| 出处 | 含义 | 约束对象 |
|---|---|---|
| `.rddf/improvements/cep-memory-model-operational-trace.md:19` | 「⛔ 不产 run」= **该单元不跑仿真**（只做手写迹判定） | 上游**单元**的范围 |
| `docs/architecture/07-coherence-exploration-platform.md:891` | 「档 2 ⭔ 不产 run」= **证据档位**纪律（`run_produced = false` / `has_verdict = false`） | **证据产物**的身份 |

### **D4 —— 边界裁决**

1. **运行期采集必然跑仿真** ⇒ 它**不能**是上游单元的延伸，**必须是新单元**（这一点 `ADR-CEP-22:135` 的「后续单元」与 improvement 的定位已一致）。
2. 采集产出的迹是 **数据**（判定面的输入），⛔ **不得**携带 `run_produced = true` / `has_verdict`。
3. 判定结论仍**只能**由 falsifier 产出 —— spec 的**四形态** = `falsified` / `not_falsified` / `undecided` / **`invalid`（拒收）**（`openspec/specs/cep-compound-graph-falsification/spec.md`）。⚠️ **其中 `invalid` 是「拒收」（输入非法），⛔ 不是判定态** —— `ADR-CEP-22:127` 明确「与『不可判』**必须区分**」（前者输入错，后者判不出）⇒ 口诀：**四形态 = 三判定 + 一拒收**（⛔ 实现方不得把「拒收」当作可输出的判定产物）⇒ 档 2 的「无通过态 / 不可判 ≠ 零违例」（`07` 的 SU ② 第 4、5 条）**逐字继续适用**。
4. ⇒ **「不产 run」不构成采集实施的阻断**，但它**限定了采集产物的身份**：迹是**输入**，不是**证据**。

---

## 5. Q3 —— `ADR-CEP-22:135` 的指针回填

**原文**（`:135`）：「本单元 v1 **不提供运行期载体**（轨迹 = **手写** litmus 迹）⇒ `V-13-4` 判读**不被本单元取代** ⇒ **显式 Out-of-Scope**，指向 I-6 后续单元」。

**问题**：其中的「I-6 后续单元」在仓内有两种读法 ——
- 读法 a：登记册 §6.4 **队列 2 的 I-6**（`cep-invariant-modeling-batch2`，**已完成**）；
- 读法 b：字面的「**I-6 之后的单元**」（不指名）。

⚠️ 读法 a 不成立：`ADR-CEP-13:314-318`（SU ③）已把 I-6 的载体定性为**声明面**；而 `.rddf/improvements/cep-memory-model-operational-trace.md:51-52` 明确「**为什么不能由 I-6 顺手做掉**」（I-6 的完成判据是 `⭭-unmodelled` 2→0 + schema 形态落定 ⇒ 载体是**声明面**；塞进运行期采集会让该单元同时承担**规格修订**与**新载体建模** ⇒ 审批链失真）。⚠️ **注**：`ADR-CEP-13:45` 是空行，**不含**该论断（本报告初稿引用有误，已更正）。

### **D5 —— 裁决：需回填，建议**追加一次性 SU**（⛔ 不改正文）

- **回填内容**：把「指向 I-6 后续单元」**消歧**为「指向运行期采集单元 `cep-memory-model-runtime-trace`（improvement 登记 2026-11-25 · 台账 Entry #549 · **未立项**）」。
- **形式**：`ADR-CEP-22` 文末**再追加**一段 SU（**追加式** · 正文零改动 · 走冻结面五步链）。
- **时机**：⛔ **不阻塞**本裁决 —— 指针回填属文档一致性，与采集实施正交；由 planner 排期。
- ⚠️ **不在本报告内改 ADR**（本报告不是 ADR，无落盘权改正文）。

---

## 6. 后果 / 代价 / 回滚

| 面 | 内容 |
|---|---|
| **正面** | 采集面从「三层任选」收敛为「有状态组件局部采集」⇒ 立项时不再需要在三种层间反复权衡；`rf` 瓶颈被显式化，避免做出一版「采得到操作、但产不出合法迹」的工装 |
| **负面** | 覆盖率受限（rf 不唯一即跳过）· thread 语义收缩为「组件即线程」· 仍需 `R-2e` 禁区外的格式约束 |
| **风险** | ① 若未来 litmus 场景需要「组件内多线程程序序」⇒ D2 的 thread 限定失效，须另立裁决；② 若 `rf` 唯一性在实践中过低（覆盖率不可用）⇒ 需回到「扩格式」路线（D3 已标为须另立） |
| **风险（补）** | ③ **非引擎发起者场景**（DMA 直连 / 流模块自发请求）⇒ D1 的「v1 范围内发起者 = 协议引擎实例」前提失效 ⇒ 须重新裁决（见 §3.1 的承重前提） |
| **v2 扩展方向（登记 · ⛔ 不在 v1）** | `include/tlm/cep/protocol_engine_base.hh`（**CEP 自有基类**，非 core）可作**统一观测面** —— 引擎天然带 thread（引擎 = 域代理）/ domain（引擎绑实例）/ rf（引擎见域内写历史），且档 2 的 `protocol-engine-observation` 已是先例（`07:870`）。⇒ v1 逐组件（`mesif` 先行）与之等价；若族数增长，v2 应上移到该基类，避免退化为「每族引擎各加 N 处 hook」 |
| **回滚** | 本报告是**裁决记录**，不产生代码/配置 ⇒ 回滚 = 不采纳（零成本）；已采纳的部分可由后续 SU 覆盖 |

---

## 7. 供 builder 立项的判据（可勾选 · 前瞻）

- [ ] **V-L2-1** 采集点落在**有状态组件**（能观察读命中哪个写），⛔ 不是流级 / flit 级 / 全仓端口基类
- [ ] **V-L2-2** 采集器**不**修改 `include/core/master_port.hh` / `slave_port.hh`（`recv` 为 `final` ⇒ 改基类即违反 D1）
- [ ] **V-L2-3** `rf` 不唯一时**跳过并计数**（⛔ 不伪造单值）· 计数可机检
- [ ] **V-L2-4** 采集产物**不含** `run_produced` / `has_verdict`（D4）
- [ ] **V-L2-5** 采集器**不给结论**，判定只经 falsifier（三态）
- [ ] **V-L2-6** `thread` 限定为「组件实例」时，须在 spec 显式登记该限制（⛔ 不静默）
- [ ] **V-L2-7** `anchor_id` ∈ `anchor_allowlist()`（5 项）· `domain` 必填且可解析
- [ ] **V-L2-8** 对既有 209 ctest **零影响**（采集默认关闭 / 显式开关）
- [ ] **V-L2-9** ⛔ 不扩 `Trace::rf` 为候选集、⛔ 不新增原因码（否则另立裁决 · 依据见 D3 第五条）
- [ ] **V-L2-10**（**well-formedness 前置** —— `ADR-CEP-22` 决策 5⑤ 末句）采集器须处理「**读读到程序序更晚的写**」（会造成 `rf` 与 `ppo_com` 的自反环）⇒ 纳入畸形集拒收 **或** 显式登记为限制，⛔ 不得静默产出 falsifier 无法按既有口径判定的迹
- [ ] **V-L2-11**（**首件报告**）首批采集须产出 `输出率 / 不唯一跳过率 / 零候选丢弃率` 三数（W3）⇒ 缺此项则「跳过机制是否有效」不可评估
- [ ] **V-L2-12** 写产生点**全 hook** 可枚举对账（W1）· 零候选迹**整迹丢弃**而非跳过单 `rf`（W2）

---

## 8. 未决 / 审查状态

**待 oracle 挑的口子（作者自认的三处薄弱）**：

1. **D1 的「局部采集」是否真能覆盖 litmus 场景** —— 本报告未枚举「CEP 场景里谁充当 master」。若场景需要「任意模块组合」，局部采集不够；若只需 `mesif` 一条路径，则够。**这一点我倾向「先做一条路径」但缺少场景证据**。
2. **D2 的 `rf` 唯一性假设** —— 我未量化「值匹配唯一」在实际 litmus 中的比例；若比例过低，D2 的切片等于不可用。
3. **D3「⛔ 扩 `rf` 为候选集」的依据强度** —— 我援引 `R-2e`（不改字段类型/取值域），但 `Trace` 是**迹格式**而非 `combination_contract` 的 schema 字段，`R-2e` 是否覆盖它，**需 oracle 判定**。

**审查状态**：

| 审查者 | 轮次 | 判定 | 结果 |
|---|---|---|---|
| **oracle**（先） | 1 轮 | **`NEEDS-REVISION`**（1 MAJOR + 7 MINOR · **不推翻 D1~D5**） | ✅ **全闭**：MAJOR-1「假唯一」⇒ 新增 **§3.2bis**（W1 写产生点全 hook / W2 零候选整迹丢弃 / W3 首件三率）+ V-L2-11/12 · MINOR-1 R-2e 依据更正 · MINOR-2 `recvReq` 纯虚的局部采集可行性 · MINOR-3 四形态含拒收 · MINOR-4 well-formedness（V-L2-10）· MINOR-5 引用改正（`:45` 空行 → upstream `:51-52`）· MINOR-6 master 恒为引擎 + §6 风险行 · MINOR-7 首件报告（W3）· 另采其「第三条路」建议 → §6 登记 `protocol_engine_base.hh` 为 v2 方向 |
| **critic**（后） | 1 轮 | **`APPROVE_WITH_CONDITIONS`** · `mergeable: yes` · `scope_exceeded: false` · **无 BLOCKER** | ✅ **1 SHOULD_FIX + 3 NITS 全闭**：① SHOULD_FIX —— `invalid` 并入「四形态」易被误读为「可输出的判定态」⇒ 补 `ADR-CEP-22:127` 的「**四形态 = 三判定 + 一拒收**」口诀；② NIT-1 档 2 载体表锚点 `:891` → **`:866-871`**（`:891` 是 Verification 行）；③ NIT-2 `CmdType` 枚举锚点 `:44`（字段）→ **`:16-20`**（枚举）；④ NIT-3 「恒为」→ **「v1 范围内（`mesif` 切片先行）」**（证据对手写迹是惯例证据，不作全称断言） |

⇒ ⛔ 本报告**在 critic 闭合前**不构成立项门①的闭合证据 ⇒ improvement `cep-memory-model-runtime-trace` **维持未立项**。
