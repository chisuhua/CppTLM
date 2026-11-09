---
name: cpptlm-cep-bidirectional-multiport-inbound
priority: P0
phase: phase-10
category: core-impl
type: fix
状态: ✅ 已归档(2026-11-09,per 台账 Entry #318 —— ✅ 已完成归档语义:**零 delta spec**(`skip_specs: true`,不改任何 main spec 的契约面);归档件 = `openspec/changes/archive/2026-11-09-cpptlm-cep-bidirectional-multiport-inbound/`;tasks **33/33**;两条决定性变异均按正确原因变红)
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖: ADR-CEP-19
来源: 台账 Entry #307(4 项 oracle 裁决)· #310(R-5 / R-6 残余登记 + `07 号` SU · 十六)· **#317**(全量盘点 + 实施路径 + metis 缺口分析)
生成时间: 2026-11-09
---

# cpptlm-cep-bidirectional-multiport-inbound

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **本 change 的问题一句话**:`BidirectionalPortAdapter` 的 **2 参 `process_request_input` 签名与基类不匹配且无 `override`** ⇒ 基类双参版回退单参 ⇒ **恒用端口下标 0** ⇒ **8 个多端口模块的入向路由全部失效**。
> ⚠️ **本 change 是「修复已交付能力的运行时真缺陷」**,⛔ **不改任何 main spec 的契约面** ⇒ 归档时**不合并 delta**(`skip_specs: true`,同 R-2 先例)。

## 架构依据

- **`include/framework/bidirectional_port_adapter.hh:335`** 声明 `void process_request_input(Packet* pkt, std::size_t port_idx)`;**基类** `include/core/stream_adapter_base.hh:82` 声明 `virtual void process_request_input(Packet* pkt, unsigned /*port_idx*/)`。二者**签名不匹配**(`std::size_t` = 64-bit `unsigned long` ≠ 32-bit `unsigned`)且该处**无 `override` 关键字** ⇒ **不是 override**。
- **调用链**:`include/core/chstream_port.hh:70` 的 `ChStreamTargetPort::recvReq` 调 `adapter_->process_request_input(pkt, port_idx_)` ⇒ 命中**基类**双参虚函数 ⇒ 其默认实现(`stream_adapter_base.hh:82-84`)**回退单参** ⇒ 命中派生类 `:331` 的单参 `override` ⇒ 该实现**硬编码 `process_request_input(pkt, 0)`** ⇒ **恒用端口下标 0**。
- **决定性证据(本轮实测 —— 编译期探针,非文本断言)**:取「基类签名」的成员指针时,编译器直接列出候选并**拒绝转换**:
  ```
  error: no matches converting function 'process_request_input' to type
         'void (class cpptlm::StreamAdapterBase::*)(class Packet*, unsigned int)'
  note: candidates are:
    'void BidirectionalPortAdapter<...>::process_request_input(Packet*, std::size_t)'  ← :335
    'void BidirectionalPortAdapter<...>::process_request_input(Packet*) override'      ← :331
  ```
  ⇒ 派生类**确实没有**基类 2 参签名的 override。
- **受影响面(实测全量 —— 8 个注册项)**:`include/chstream_register.hh` 中 `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED` 注册 **7 个 N=2** 模块(`CohIfProtocolEngine` / `ChiRnfProtocolEngine` / `MesifProtocolEngine` / `TileLinkProtocolEngine` / `MoesiGpuProtocolEngine` / `UalinkProtocolEngine` / `CohIfResponderTLM`)+ `registerBidirectionalPortAdapter<tlm::RouterTLM, ..., 5>`(**1 个 N=5**)⇒ **全部多端口入向路由失效**。
- **R-6 随之为真**(`07 号` SU · 十六 ② 登记):拓扑 B 的 `chi.0 → home.1` ⇒ 因 R-5 ⇒ chi 侧流量落 `home` 的 `req_in[0]`(与业务请求**同桶**)。
- **⛔ 与 R-2 的区别(避免混淆)**:`07 号` SU · 十五 ② ①(a) 曾称「`BidirectionalPortAdapter` 的双参路由**是对的**…**不是**本次受限点」⇒ 该陈述**仅对「R-2 的回程腿」成立**(回程腿走的是 **dual** 适配器 `DualPortStreamAdapter`,已在 `cpptlm-cep-dual-port-inbound-dispatch` 修复)⇒ **不成立于「该适配器无缺陷」**。`SU · 十六` 已用「订正二」调和,但**缺陷本身从未修复**。

## 范围

- **In Scope**:
  1. **签名修复** —— `include/framework/bidirectional_port_adapter.hh:335` ⇒ 改为 `unsigned port_idx` + **加 `override` 关键字**(编译期冻结签名)。
  2. **运行期分桶判据** —— 新建 `test/test_framework_bidirectional_multiport_inbound.cc`:夹具 N=2 与 N=5,**对 `k ∈ [0,N)` 全枚举**注入带唯一 tag 的 Packet ⇒ 断言**仅** `req_in[k]` 收到、其余为 null。
  3. **RouterTLM 真装配判据** —— 5 端口 `bind_ports_array` 后跑同一断言 ⇒ 证明不是「只有模板夹具绿」。
  4. **编译期签名锁** —— `static_assert` + 成员指针类型断言(见 Acceptance ①)⇒ 签名漂回即编译红。
  5. **同批处置:零调用点的 `process_response_input`** —— `:347` 全仓**零调用点**,且其实现写的是 `module_->req_in()[port_idx]`(**疑似 copy-paste**,语义应为 `resp_in`)⇒ 本 change **显式处置**(删除 / 或修正 + 补测试)⇒ ⛔ 不留「同类潜在缺陷」悬空。
  6. **同批裁决:端口忙时的行为** —— `:339-341` 的 `if (!req_adapter.valid()) process(pkt)` **无 `else`**;而 `chstream_port.hh:72` 的 `recvReq` **恒 `return true`** + **无条件 `PacketPool::release(pkt)`** ⇒ 端口忙 = **静默丢包**。修好多端口后流量分桶变真实 ⇒ 该分支从「隐藏」变「可观测」⇒ 本 change **必须裁决**(回压 / 丢弃 + 计数器)并落判据。
  7. **e2e baseline 前后对比 + 结论重验登记** —— R-5 修好 ⇒ **入向路由真实分桶** ⇒ 既有 e2e 的数值**可能变化** ⇒ 必须取 baseline、逐项对比、变化则登记「结论重验」或「结论不受影响」的理由。
- **Out of Scope(⛔ 逐条)**:
  - ⛔ **R-3 / Step 7.5 桥重挂检测** —— 另立 change(且 ⛔ **须先有 architect 裁决**「同一模块多桥挂载是否合法」)。R-3 与本 change **不合并**:R-3 动 `src/core/module_factory.cc` 装配主路径,风险面大一个量级 ⇒ 合并会让本 change 被更复杂的设计拖住。
  - ⛔ **不改任何 main spec** ⇒ `skip_specs: true`。
  - ⛔ **不改 `include/tlm/cep/**`** 与 M1 冻结清单(hash 须零变)。
  - ⛔ **不改 `attach_protocol_bridge` 的三处实现签名**(那是 R-3 的调用方面)。
  - ⛔ **不宣称「全绿 run」达成** —— IC3 仍 ⚪ 不适用(违例类 `memory_violations` 无样本)⇒ **R-5 修好 ≠ 全绿**。
  - ⛔ **`B3b-2` 维持 0/4**;不为 B3b-2 造内部替代品、不绕过 T1–T4。
  - ⛔ 不改任何**已签发 ADR** 的决策正文。

## Why

- **不是「文档债」而是「运行时真缺陷」** —— 8 个多端口模块的入向路由**当前完全不工作**:无论从哪个端口进来,包都落 `req_in[0]` ⇒ 端口 1..N-1 **永远收不到东西**。这意味着 `RouterTLM`(5 端口 mesh)的**多邻居入向**与全部 CEP 引擎的**第二端口**都是**哑的**。
- **为什么此前没被发现** —— 既有 e2e **只断言完成数,不断言分桶**(实测 `test/test_dual_port_inbound_dispatch.cc` 中 `req_in[0]` / `req_in[1]` 断言数 = **0**)⇒ ⛔ 「修完仍绿」**不能**证明修对了 —— 这正是台账 **Entry #236** 记录的「**载体未真正咬合判据的主体**」失败模式(该族已回归 3 次)⇒ 本 change 的判据**必须分桶断言**。
- **为什么现在做** —— 台账 **Entry #317**(全量盘点)把它排为 **P1 实施轨最高价值项**:它是剩余 backlog 里**唯一**修运行时真缺陷的项;`include/framework/` **不在** M1 冻结集(`cep_freeze_check.sh` 的 `FROZEN_GLOBS` 无此条)⇒ **无需**冻结面授权链。
- **为什么不能只改一行就收工** —— 修完之后,**三个新问题会立刻显形**:① 端口忙静默丢包(此前因全部流量挤在端口 0 而被掩盖)② `process_response_input` 的 copy-paste 缺陷(零调用点 ⇒ 从未被执行)③ 既有 e2e 结论的有效性(R-5 之前,那些 e2e 跑在**错误路由**上)⇒ 三者**必须同批处置**,否则是「留尾巴」。

## What Changes

1. **框架修复(1 处)**:`include/framework/bidirectional_port_adapter.hh:335` ⇒ `std::size_t` → `unsigned` + 加 `override`。
2. **同批处置(2 处)**:`process_response_input`(零调用点 + 疑似 copy-paste)⇒ 显式处置;端口忙行为 ⇒ 裁决 + 判据。
3. **判据(新建 1 个测试文件)**:`test/test_framework_bidirectional_multiport_inbound.cc` ⇒ N=2 / N=5 全枚举分桶 + RouterTLM 真装配 + 编译期签名锁。
4. **⛔ 不新增成员到 `BidirectionalPortAdapter`** —— 实测**无**跨实例 `static_cast` 到本类 ⇒ 布局约束不成立,但 ⛔ 仍**不加成员**(端口忙计数器若需要,走既有可观测面或另立 change)⇒ 保持「改动面最小」。

## Acceptance

- [ ] **① 编译期签名锁**:`test/test_framework_bidirectional_multiport_inbound.cc` 含 `static_assert` + 成员指针类型断言(`void (StreamAdapterBase::*)(Packet*, unsigned)`)⇒ 签名漂回 `std::size_t` 即**编译红**。
- [ ] **② 运行期分桶(决定性)**:N=2 与 N=5 夹具,**对 `k ∈ [0,N)` 全枚举**注入带唯一 tag 的 Packet ⇒ 断言**仅** `req_in[k]` 收到、其余为 null(⛔ 非抽样)。
- [ ] **③ RouterTLM 真装配**:5 端口 `bind_ports_array` 后跑同一断言 ⇒ 证明不是「只有模板夹具绿」。
- [ ] **④ 决定性变异证据(必附,不可省)**:提交里附**两条**变异结果 —— ① `unsigned → std::size_t` ⇒ 测试**必红**;② `port_idx` 硬编码 `0` ⇒ 测试**必红**。⛔ 只有「绿」不算闭合(per `Entry #236` 教训)。
- [ ] **⑤ e2e baseline 前后对比 + 结论重验**:先记录修正前 baseline(chi_rnf A/B · tilelink · lwc · minimal SoC),修正后逐项对比;**若数值变化** ⇒ 必须登记「结论重验」或「结论不受影响」的**理由**;⛔ 仅写「无回归」不足以交付。
- [ ] **⑥ `process_response_input` 显式处置**:删除**或**修正(写入 `resp_in`)+ 补测试 ⇒ 结论写进 commit message;⛔ 不得留「零调用点的疑似缺陷」悬空。
- [ ] **⑦ 端口忙行为裁决 + 判据**:裁决「回压(`recvReq` 返回 false)」**或**「丢弃 + 计数器」⇒ 落判据(断言忙时**不静默**丢包);⛔ 若裁决为回压 ⇒ 须评估对既有 `recvReq` 恒 `true` 语义的影响面。
- [ ] **⑧ 边界用例**:`port_idx == N`(越界)⇒ no-op(不崩、不写越界);`N=1` 的防护(宏路径已有 `static_assert(n > 1)`,直接实例化路径须确认)。
- [ ] **⑨ 零 diff 边界**:`include/tlm/cep/**` 与 M1 冻结清单 hash **零变化**;⛔ `B3b-2` 维持 **0/4**;⛔ **未宣称**「全绿 run」达成。
- [ ] **⑩ 回归**:`ctest` **167/167** 绿 + `./test.sh --mode off` **21/21** 绿 + `cep_freeze_check` 无参 rc=0 + `cep_docs_sync_check --strict` 绿。
- [ ] **⑪ 台账登记**:台账 Entry 追加本 change 的**实施回执**(含 ④ 的两条变异结果与 ⑤ 的 baseline 对比表)。

## Capabilities

- **C1 真实多端口入向路由** —— `BidirectionalPortAdapter` 的 2 参版成为**真 override** ⇒ `RouterTLM`(5 端口)与 7 个 N=2 模块的**分端口入向**首次可用。
- **C2 编译期签名冻结** —— 签名漂移**编译红**(而非运行期静默错路由)⇒ 该类缺陷**不可复发**。
- **C3 分桶级判据能力** —— 新测试文件提供「按端口分桶断言」的**可复用范式** ⇒ 后续任何多端口 adapter 的判据可照抄(⛔ 不得塞进 `test_router_tlm.cc` —— 实测其**零 ChStream 装配** ⇒ 会产出伪载体)。
- **C4 端口忙可观测** —— 裁决后端口忙**不再静默**。

## Impact

- **改动面**:1 个框架头 + 1 个新测试文件 + 台账 ⇒ **极小**;⛔ 不改 `src/core/**`、不改 `include/tlm/cep/**`、不改任何 main spec。
- **行为面**:多端口入向从「全部落端口 0」变为「**按端口分流**」⇒ ⚠️ **这是语义变更** ⇒ **既有 e2e 结论的有效性须重验**(Acceptance ⑤)⇒ ⛔ 不得以「无回归」代替。
- **风险面**:① 端口忙分支从隐藏变可观测 ⇒ 可能暴露既有拓扑的**隐性丢包** ⇒ 须在同批裁决并落判据(Acceptance ⑦);② 若既有 e2e 数值变化 ⇒ 需**回溯登记**(⚠️ 台账 append-only ⇒ 须开新 Entry,⛔ 不得改写既有结论)。
- **KPI**:本 change 占 **1 个 slot**(0/3 → 1/3)⇒ 与 AGENTS.md「Proposed ≤ 3」一致;CEP 系列内部并发 ≤ 1 ⇒ 本 change 期间**不并行**开第二个 CEP change。
- **冻结面**:⛔ **零冻结面改动**(`include/framework/` 不在 `FROZEN_GLOBS`)⇒ **无需**五步授权链;⛔ 但冻结清单 hash 须**零变化**(Acceptance ⑨)。
- **不做**:⛔ 不改 `B3b-2` 计数;⛔ 不宣称全绿;⛔ 不合并 R-3。

**关联**:台账 **Entry #317**(本 change 的盘点 + 路径 + metis 处置)· **#310**(R-5 / R-6 残余登记 + `07 号` SU · 十六)· **#307**(4 项 oracle 裁决)· **#264**(终态 DoD Go 挂起)· **#236**(载体评审教训 · 决定性变异必附)· `docs/architecture/07-coherence-exploration-platform.md` SU · 十五 ② ①(a)(**已被 SU · 十六 订正二调和**)· SU · 十六 ② · `include/framework/bidirectional_port_adapter.hh` · `include/core/stream_adapter_base.hh` · `include/core/chstream_port.hh` · `include/chstream_register.hh`
