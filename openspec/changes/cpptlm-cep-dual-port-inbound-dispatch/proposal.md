# cpptlm-cep-dual-port-inbound-dispatch: 双端口适配器入向按端口组路由 ⇒ 拓扑级回程腿闭合

> **状态**:Proposed(Draft v0.1)· **立项**:2026-11-09 · **角色**:builder
> **触发**:台账 **Entry #305 ③ R-2**(残余登记)+ **Entry #307 ③**(实施顺序 R-2 置首)
> **依据**:`docs/architecture/07-coherence-exploration-platform.md` **SU · 十五 ①**「「全绿 run」目标未达成(残余缺口)」

## Why

### 根因(代码级,非推断)

`DualPortStreamAdapter` **只 override 单参版** `process_request_input(Packet*)`,而框架的**唯一入向入口**是**双参版**:

| 环节 | 位置 | 事实 |
|---|---|---|
| 框架入口 | `include/core/chstream_port.hh:70` | `ChStreamTargetPort::recvReq` 调 `adapter_->process_request_input(pkt, port_idx_)` —— **只有双参版** |
| 基类回退 | `include/core/stream_adapter_base.hh:82-84` | 双参版默认**回退到单参版** ⇒ 端口下标信息**在此丢失** |
| 适配器 | `include/framework/dual_port_stream_adapter.hh:189-199` | 只 override 单参版 ⇒ 恒调 `process_pe_request_input()` ⇒ **只填 `pe_req_in()`** |
| 旁证 | `include/framework/dual_port_stream_adapter.hh:204` | `process_net_response_input()` **零调用点**(全仓仅定义处)⇒ `net_resp_in()` 永不可达 |

⇒ **双端口模块的 net 侧入向恒落 PE 侧**。

### 后果链(CEP 拓扑级)

1. 拓扑 A/B 的 `home` 侧到期 `Completion` 从 `resp_out[0]` 发出 ⇒ 经 CohIf 的 egress 通道(`req_out`,per `include/tlm/cep/cohif_direction_traits.hh:45-56`)⇒ 抵达 **`w` 的 net 组 `req_in[1]`**;
2. 该包被当作**业务请求**读进 `w` 的 `pe_req_in()` ⇒ **产生垃圾请求**(原注释亦如实记载此现象);
3. ⇒ `w` 的 `net_resp_in()` 收不到 `Completion` ⇒ **回程腿在拓扑级不可达**;
4. ⇒ 延迟族 5 键 `not-collected` + `IC3 = ⚪ 不适用` ⇒ **「全绿 run」目标无法评估**(SU 十五 ① 已登记)。

### 为什么必须另立 change

`include/framework/` 是**共享框架层**(`DualPortStreamAdapter` 同时服务 `NICTLM` 与 `StubCacheEndpointTLM`)⇒ 影响面 ≥ 2 个模块 / 公共行为 ⇒ per AGENTS.md 反模式「架构性变更未走 OpenSpec」必须先提案;且 per Entry #305 ③ R-2 明确「另立 change」。

### ⚠️ 「双条件」不完整 —— 实测需**第三条**(实施期发现)

`07 号` **SU · 十五 ①** 登记的触发条件是**双条件**:① 修适配器的入向路由 + ② 改 config `emit_completions: true` 并重跑。
**实测**:只做 ①+② 时 `home->counters().completions_sent > 0` 但 `w->counters().net_completions_in == 0` ⇒ **回程仍不可达** ⇒ 双条件**不充分**。

**缺的那一条(③)**:CohIf 的 egress 走 **`req_out`**(`include/tlm/cep/cohif_direction_traits.hh` 的 `egress_channel()` = `EgressChannel::Req`)⇒ `home` 的回程 `Completion` 从 **`home.req_out[0]`** 发出;而原拓扑只有 `w.1 → home.0` 一条连接 ⇒ 其正向 pair 是 `w.req_out[1] ↔ home.req_in[0]` ⇒ **`home.req_out[0]` 未被任何连接绑定** ⇒ `send()` 返回 false ⇒ 回程**根本发不出去**。

⇒ 须在拓扑里补 **`home.0 → w.1`**(每个方向一条**显式**连接 —— 这正是 CohIf「断链即断达」模型的直接推论:断掉这条连接 ⇒ 回程不可达 ⇒ 判据有牙)。

⚠️ 这条**不改** `07 号` 的原文(SU · 十五 的「双条件」是**当时**的登记)⇒ 以本 change + `07 号` **SU · 十六** 追加闭合登记。

## What Changes

| # | 改动 | 文件 | 性质 |
|---|---|---|---|
| **C1** | `DualPortStreamAdapter` override **双参** `process_request_input(Packet*, unsigned port_idx)`,**按端口组**路由:组 0(PE)⇒ `pe_req_in()` · 组 1(Net)⇒ `net_resp_in()`;单参版改为**委托给组 0**(保持既有直接调用者的行为不变) | `include/framework/dual_port_stream_adapter.hh` | **框架行为修复** |
| **C2** | 拓扑 A/B 的 home 侧 `emit_completions: false` ⇒ `true`(+ `description` 同步) | `configs/cep_chi_rnf_e2e_{a,b}.json` | 配置 |
| **C2b** | 拓扑 A/B 补 **`home.0 → w.1`** 的**回程连接**(⚠️ **实施期实测发现的第三条条件** —— 详见 Why 的「双条件不完整」) | `configs/cep_chi_rnf_e2e_{a,b}.json` | 配置 |
| **C3** | 新增入向路由判据(**正控 + 反向 fixture**) | `test/test_dual_port_inbound_dispatch.cc`(新) | 测试 |
| **C4** | SCE-09 断言翻转(`completions_sent == 0` ⇒ `> 0`;`completions_suppressed > 0` ⇒ `== 0`)+ 追加**回程腿闭合**判据(`w` 的 `net_completions_in` / `cpu_responses` > 0) | `test/test_cep_stub_cache_e2e.cc` | 测试 |
| **C5** | `emit_completions` 的注释改写(原注释称「框架限制」——限制已解除) | `include/tlm/cep/cohif_responder.hh` | 注释 |

### 关键设计决定(为什么**按端口组**而不是按 `pkt->type`)

| 候选 | 判定 | 理由 |
|---|---|---|
| **按端口组**(本方案) | ✅ 采用 | 双端口模块的**物理端口下标 = 端口组下标**(组 0 = PE · 组 1 = Net,见 `src/core/module_factory.cc:697-707, 737-739`)⇒ 下标**直接**给出「来自 PE 还是 Net」这一**唯一被模块 API 表达**的信息 |
| 按 `pkt->type` | ❌ 否决 | CohIf 的 egress 走 `req_out` ⇒ 回程 `Completion` 以 **`PKT_RESP`** 打在**对端的 `req_in`** 上 ⇒ 按 type 分流会把「响应」投到「请求」通道,**反而更错** |
| 拆成 4 个通道(PE req/resp × Net req/resp) | ❌ 否决 | 超出模块 API(`pe_req_in` / `pe_resp_out` / `net_req_out` / `net_resp_in` 四通道契约)⇒ 需改 `NICTLM` + `StubCacheEndpointTLM` 的公开面 ⇒ 越界 |

⚠️ **不可新增成员变量**:`src/core/module_factory.cc:733` 把适配器 `static_cast` 成**另一模板实例化**(`ModuleT = ChStreamModuleBase`)⇒ 两个实例的**成员布局必须逐字节相同** ⇒ 新增成员会让该 cast 写入错误偏移 ⇒ 计数器只能用 `static inline`(不进对象布局)。

## 验收标准(Acceptance Criteria)

- [ ] **AC-1** 双端口适配器 **net 组入向** ⇒ 落 `net_resp_in()` 且 **`pe_req_in()` 保持无效**(反向 fixture:改回旧行为 ⇒ 按该原因红)
- [ ] **AC-2** 双端口适配器 **PE 组入向** ⇒ 落 `pe_req_in()`(既有行为不变 ⇒ 无回归)
- [ ] **AC-3** `process_net_response_input()` 的**调用点 > 0**(原为零调用点)
- [ ] **AC-4** 拓扑 A:`home->counters().completions_sent > 0` ∧ `completions_suppressed == 0` ∧ `w->counters().net_completions_in > 0` ∧ `w->counters().cpu_responses > 0` ⇒ **回程腿在拓扑级闭合**
- [ ] **AC-5** `ctest` **全绿**且**用例数不减**(含 `[dualport]` / `[cep]` / mesh 相关用例)⇒ 框架改动**无回归**
- [ ] **AC-6** `B3b-2` **维持 0/4**;`test/cep_b3b2_deferrals.json` / `protocol_instances.json` / `protocol_instance.schema.json` **零 diff**;`stub_cache_core.hh` / `cohif_noc_bridge.hh` **零 diff**;⛔ **不**把「回程腿闭合」写成「协议语义达成」

## Out of Scope

- ⛔ **`BidirectionalPortAdapter` 的同类缺陷**(`include/framework/bidirectional_port_adapter.hh:335` 的 `process_request_input(Packet*, std::size_t)` **与基类的 `unsigned` 签名不匹配** ⇒ 不是 override ⇒ 端口下标同样丢失 ⇒ 恒落 `req_in()[0]`)。**影响面**(RouterTLM 5 端口 + 全部 CEP 引擎 2 端口)**远大于**本 change ⇒ **不在本 change 修**,登记为残余 **R-5**(含实测证据)。
- ⛔ **`07 号` SU · 十六**(残余缺口闭合登记)—— `docs/architecture/07-*.md` 在 **M1 冻结集**内 ⇒ 须走**五步授权链**(台账 Entry → 产物 commit → `--update --authorized-by` → 清单单独 commit → 无参复验)⇒ 随本 change 的 commit 轮次执行。
- ⛔ **拓扑级「全绿 run」**:per SU 十五 ①,`IC3` 仍为 ⚪ 不适用(违例类 `memory_violations` 无样本)⇒ 本 change 只让**延迟族 5 键出数值**,⛔ 不得据此宣称「全绿」。
- ⛔ `B3b-2` 计数 / `protocol_instances.json` / `protocol_instance.schema.json` / `stub_cache_core.hh` / `cohif_noc_bridge.hh` / 任何**已签发 ADR 的决策内容**。

## 关联

- 台账 **Entry #307**(4 项裁决 + 实施顺序)· **#305 ③ R-2**(残余登记)· **#304**(A9.2 SU)
- `docs/architecture/07-coherence-exploration-platform.md` **SU · 十五 ①**(双重成因与双条件)· `docs/adr/ADR-CEP-19` · `docs/adr/ADR-CEP-06`
- `include/framework/dual_port_stream_adapter.hh` · `include/core/stream_adapter_base.hh:82-84` · `include/core/chstream_port.hh:70` · `src/core/module_factory.cc:697-707,733-740`
- `.rddf/reviews/cep-next-session-handover-2026-11-09.md` §二 R-2 · §七.1 E-7/E-8
