---
name: cpptlm-cep-stub-cache-chstream-surface
priority: P0
phase: phase-10
category: core-impl
type: feature
状态: ✅ 已归档(2026-11-09,per 台账 Entry #305 —— ✅ 已完成归档语义:delta 已合并进 `openspec/specs/cep-stub-cache-chstream-surface/spec.md` + `openspec/specs/cep-stub-cache-core/spec.md` 的 `:56` 单条 MODIFIES;归档件 = `openspec/changes/archive/2026-11-09-cpptlm-cep-stub-cache-chstream-surface/`;tasks 52/52)
主题: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台
依赖: ADR-CEP-05, ADR-CEP-06, ADR-CEP-07, ADR-CEP-12, ADR-CEP-19
来源: ADR-CEP-19 决策 9（显式延期）+ 台账 Entry #291~#296
生成时间: 2026-11-08
---

# cpptlm-cep-stub-cache-chstream-surface

**主题**: Coherence Exploration Platform(CEP)— 协议中立一致性互联仿真平台

> **名称锁定**:本名字由 `ADR-CEP-19` 决策 9 **具名绑定** ⇒ ⛔ **不改名**(per 台账 Entry #282 C-4);⚠️ 名字字面只承诺「端口面」,而本 change 实际承载 **G1 端口面 + G2 两拓扑 + G3 判据 + G4′ metric/元数据 四面** ⇒ ⇒ **收编 scope 须显式声明**(下表 §范围)。

## 架构依据

- `ADR-CEP-19` **决策 9** 把「`StubCacheCore` 的 ChStream 端口面」判为**显式延期**(四条理由:spec 在 `FROZEN_GLOBS` / spec 的 mock 约束 / stub 是 testbench 纯类 / 本该 `registerObject`),并**具名绑定**本 change。
- 台账 **Entry #288** 把该延期项由「显式延期」转为「**实施态**」;`07 号` **SU · 十一 ~ 十四** 登记实施形态(组合式 wrapper / 独立类目 home 模型 / J-9 拓扑分档 / 声明面 = 甲 / retention = 丙)。
- `07 号` **SU · 七 ④** 诚实登记当前后果:现有**全部**事务级判据都是「**引擎 ↔ 引擎**同构对等」⇒ ⛔ 不能回答「该协议在**真实业务流量**下会怎样」⇒ ⇒ 本 change 补的正是这一面。

## 范围

- **In Scope(收编四面,per Entry #282 C-4 显式声明)**:
  - **G1 端口面** —— `StubCacheEndpointTLM`(组合式 wrapper,`StubCacheCore` **纯类零 diff**)+ `CohIfResponderTLM`(**真实 home 的简化模型**)+ 真实注册。
  - **G2 两拓扑** —— 拓扑 A(直连,全绿载体)/ 拓扑 B(带 latency 连接 + 桥投影语境),**两份独立 config**。
  - **G3 判据** —— J-1 ~ J-8 + **J-9 拓扑分档**;**载体 = Catch2 测试** `test/test_cep_stub_cache_e2e.cc`(标签 `[cep][e2e]`)。
  - **G4′ metric / 元数据** —— 台账元数据 **7 字段**(两处并存:台账散文段 + `test/cep_e2e_runs.jsonl`)+ **双向闸** + **首批 h3 升级 run 入库**(`metrics.json` + `simulation_config.json`)。
  - **契约面收敛** —— delta 含 `:56` **单条** MODIFIES + **豁免登记 4 项** + 「登记 ≠ 可复算」边界句 + 「不作为 T1–T4 条款级证据」声明句。
- **Out of Scope**:
  - ⛔ 家族级 / 条款级协议语义(credit · retry · ordering point · 多拍聚合 · Perm 规则 · 容量与队列模型)。
  - ⛔ `trace.h5` / `report.html` 入库(retention = 丙)· ⛔ 历史 127 run 回填(grandfather)。
  - ⛔ 改 `B3b-2` 计数(维持 **0/4**)· ⛔ flit 级网络路径(仓内不存在)· ⛔ `TRACE` 流量模式。

## Why

`obj §2.1` 的目标是把「用 CHI / CXL / UALink 的这个 SoC,coherence 开销 / 停顿 / 死锁风险如何」从**人脑推演**变成**可跑实验出数据**。而当前判据只能在**引擎之间对等互打** ⇒ 即便链路全绿,也⛔ **不是**「真实业务流量下的行为」⇒ ⇒ 目标层的「可实验性」缺一块。本 change 是把该缺口补到**可测**(⛔ 不承诺协议语义达成)。

## What Changes

1. **新增** `include/tlm/cep/stub_cache_endpoint.hh` + `.cc`:`StubCacheEndpointTLM`(`ChStreamModuleBase` 派生,内部持 `StubCacheCore` 实例;异构 dual 端口 `CacheReq/CacheResp ↔ CohIf/CohIf`;**公开观测 API** = per-txn issue/complete 周期戳 + in-flight 计数;`attach_protocol_bridge` override + **只读桥指针访问器**;映射含「命中 → 就地生成 `CacheRespBundle`」;⛔ 语义分层红线 = 映射代码**仅许**存在于本模块)。
2. **新增** `include/tlm/cep/cohif_responder.hh` + `.cc`:`CohIfResponderTLM`(**真实 home 的简化模型**)—— 对称 CohIf 端口 + 参数化延迟回 `Completion` + 按读请求可配发 `Snoop` + `Writeback` **接收即消费、零响应**;⛔ 不承载条款级语义;⛔ 不进 `protocol_instances.json`(已裁**甲**)。
3. **注册**:`registerObject<StubCacheEndpointTLM>` + `registerObject<CohIfResponderTLM>` + `registerDualPortAdapter<...>` + `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED`(⛔ 不发明宏形式,per `ADR-CEP-07` 决策 3)。
4. **`TrafficGenTLM`**:新增 **seed / 地址段 / mode setter**(现 `rng_(42)` 硬编码)⇒ 使「种子入 config 快照」有落点;快照记种子 + **git rev** + 工具链。
5. **拓扑 config(两份独立文件)**:`configs/cep_chi_rnf_e2e_a.json`(省略 `cep.bridges` ∧ 全部 `connections` `latency = 0`)与 `configs/cep_chi_rnf_e2e_b.json`(`cep.bridges` 恰好 1 条 / `modules` 恰好 3 / ≥1 条 `latency > 0`;entry 带**已声明**实例名以满足 `[IAG-02]`)。
6. **判据(Catch2,`test/test_cep_stub_cache_e2e.cc`)**:J-1 装配 · J-2 事务抵达 · J-3 端到端响应完整 · J-4 IC3 绿(三态)+ run-validity 绿 · J-5 **决定性变异**(按正确原因红)· J-6 指标自洽 · J-7 桥投影(测试级 `pack`/`unpack` + 逐模块 `bridge()->bridge_id()` 同值)· J-8 B 不承载全绿 · **J-9 拓扑分档**。
7. **台账元数据(两处并存)**:台账散文段记 7 字段 + `test/cep_e2e_runs.jsonl` 机器可读(仿 `test/cep_b3b2_d4_probe_runs.jsonl`)⇒ **双向闸**读 jsonl。
8. **契约面**:`openspec/specs/cep-stub-cache-core/spec.md` 的 **`:56` 单条** MODIFIES(另立 Entry + 两步授权)+ change delta 含豁免登记 4 项 / 「登记 ≠ 可复算」边界句 / 7 字段 / 「⛔ 不作为 T1–T4 条款级证据」声明句。

## Acceptance

- [ ] **A0 授权门**:另立台账 Entry 授权 `:56` 单条 MODIFIES,且两步授权链绿(provenance 复验通过)
- [ ] **A1/A2**:两模块可构造;`StubCacheEndpointTLM` 两侧 Bundle 类型**确为异构**;`StubCacheCore` **纯类零 diff**(`INV-SC1`~`INV-SC6` 与 14 个既有单测全绿)
- [ ] **A3**:`ModuleFactory::instantiateAll` 对两份 config 均成功
- [ ] **A4 复现门**:setter 存在 + 快照含种子/git rev;同种子两次 run 的计数**逐字段相等**
- [ ] **A5 / J-9**:拓扑 A 闸 `SKIP` ∧ 无 `cep.bridges` ∧ 无 `latency>0`;拓扑 B 闸 `PASS` ∧ 1 entry / 3 modules / ≥1 条 `latency>0` ∧ 三模块桥指针同 `bridge_id`
- [ ] **A6**:J-1 ~ J-9 逐条绿;J-5 的变异按**正确原因**红(⛔ 非「因错误的原因红」)
- [ ] **A7 可达门**:13 键**各有结论**(实测值 **或** `not-collected` + reason)+ IC3 **✓ 通过** + 双违例类真实 0;三键测量归属**显式落字**
- [ ] **A8 对账门**:台账条目 7 字段齐备 + verdict 用三态词表 + **双向闸**绿(仓内 `metrics.json` ↔ 台账条目,零 orphan)+ 首批 h3 run 仅含 2 件
- [ ] **A9 契约面**:`:56` MODIFIES 合并;delta 含豁免登记 4 项 / 边界句 / 7 字段 / 声明句;`07 号` 与 ADR 的**实施后**状态更新已追加
- [ ] **计数零变化**:`B3b-2` 仍 **0/4**;`test/cep_b3b2_deferrals.json` **零 diff**;`protocol_instances.json` / `protocol_instance.schema.json` **零 diff**

## Capabilities

- **端到端可测**:真实业务流量(TrafficGen)可驱动到 CEP 互联面,⛔ 不再局限于引擎对等互打。
- **双拓扑**:直连(全绿载体)+ 带延迟/投影(B 的独立价值 = 互联可达 + 三端点共挂一 entry + 测试级桥投影)。
- **home 简化模型**:补上「home 应答端」这一物理元素(承载**最小**应答语义,⛔ 非条款级、⛔ 不计入 B3b)。
- **台账可对账**:`metrics_sha256` ↔ 仓内 `metrics.json`(配合 retention 丙),使台账结论首次**可核**。
- **观测面**:per-txn 周期戳 / in-flight 计数(原始事实;聚合仍由 `test/` 的 harness 消费,⛔ 不升格进契约面)。

## Impact

- **新增文件**:2 个契约面模块(`stub_cache_endpoint.*` / `cohif_responder.*`)· 1 份判据 `test/test_cep_stub_cache_e2e.cc` · 2 份 topology config · 1 份 run-record(`test/cep_e2e_runs.jsonl`)。
- **改动文件**:`include/chstream_register.hh`(注册行)· `configs/`(2 份新配置)· `openspec/changes/<本 change>/**` · `openspec/specs/cep-stub-cache-core/spec.md`(**`:56` 单条**,须两步授权)· `.gitignore`(2 条例外 + 注释 provenance)· `obj §11`(台账元数据段)。
- **⛔ 不动**:`include/tlm/cep/stub_cache_core.hh`(纯类零 diff)· `include/tlm/cep/cohif_noc_bridge.hh`(桥 `pack`/`unpack` 冻结)· `include/framework/`(⛔ 不新增映射代码)· `protocol_instances.json` / `protocol_instance.schema.json`(甲已裁)· `experimental/preregistration.json`。
- **契约影响**:新增**第四类模块**(adapter / engine / bridge / **home 模型**)⇒ 两侧边界句(home 模型 ⛔ 作 basis 分档输入 / ⛔ 计入 B3b)须随 delta 落盘。
- **计数影响**:**零** —— `B3b-2` 维持 0/4;`KPI` 0/3 → 1/3(建 change 目录)⇒ 归档后回 0/3。
