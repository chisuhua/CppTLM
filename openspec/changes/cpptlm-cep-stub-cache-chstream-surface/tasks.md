# tasks —— `cpptlm-cep-stub-cache-chstream-surface`

> 顺序 = **A0 → (A1 ∥ A2) → A1b(收尾) → A5(含 A3.2) → W4 → A6 → A7 → W2 → A8 + W3 → A9 → T9**;
> **A4 与 A5 并行**;**W1 独立**;**B1/B2 不在本 change(roadmap 独立项)**。
> 门:**G1 = A0** · **G2 = A6** · **G3 = A4** · **G4 = A8**;里程碑:**M1 接线 → M2 拓扑 B 分档 → M3 首件全绿 run → M4 归档**。

## A0 · 授权门(G1)

- [x] A0.1 另立台账 Entry 授权 `openspec/specs/cep-stub-cache-core/spec.md` 的 **`:56` 单条** MODIFIES(Entry #297;delta 仅该行差异)
- [ ] A0.2 两步授权链随 **T9.3 归档**一次执行(产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #297"` → 清单单独 commit → 无参复验)

## A1 · `StubCacheEndpointTLM`(端口面)

- [x] A1.1 `include/tlm/cep/stub_cache_endpoint.hh`(**header-only**;组合式 wrapper;异构 dual 端口)
- [x] A1.2 公开观测 API(per-txn issue/complete 周期戳 + `id_space` + in-flight + `cpu_resp_backlog`)
- [x] A1.3 映射:op 编码 / 同步 `AccessResult` ⇒ 异步 `CacheRespBundle`(**命中/拒绝亦回 CPU 侧响应**)/ net_side 5 流编码
- [x] A1.4 `attach_protocol_bridge` override + **只读桥指针访问器**
- [x] A1.5 `stub_cache_core.hh` **零 diff** + `INV-SC1`~`INV-SC6` 与 14 单测全绿

## A1b · A1/A2 收尾(双审发现)

- [x] F-A1 `CohIfResponderTLM` 的 `Completion` **回到请求的 ingress 端口**(按端口分桶,慢端口不阻塞其它端口);SCE-04 同步改
- [x] F-A2 CPU 侧响应**背压排队不丢**(`cpu_responses_enqueued` vs `cpu_responses` 分账)
- [x] F-A3 `evict()` 的 Writeback txn 用**独立段**(`kEvictTxnBase`),与 core txn 不重叠
- [x] F-A4 delta 增**限定句**「驱动者 = home 简化模型,⛔ 不是协议引擎」(ADR 原文逐字不动)
- [x] F-A5 proposal/design 改为 **header-only**;`tasks.md` 重排 + 勾选 + 本表
- [x] F-A6 删 `static_assert(true,"")`;Snoop txn 用独立段并**显式登记「配对不建模」**
- [x] F-A7 core 拒绝路径:⛔ **不伪造** `transaction_id=0` 的 Request;回显式 reject 响应(`error_code=1`)+ `cpu_rejected`
- [x] B9 `TxnStamp.id_space ∈ {CPU, CORE}`;J-3 不得跨空间配对

## A2 · `CohIfResponderTLM`(home 简化模型)

- [x] A2.1 `include/tlm/cep/cohif_responder.hh`(**header-only**;对称 CohIf 端口)
- [x] A2.2 Request ⇒ **参数化延迟** Completion;按读**可配发** Snoop;Writeback 接收即消费
- [x] A2.3 `attach_protocol_bridge` override + 只读桥指针访问器
- [x] A2.4 ⛔ 不进 `protocol_instances.json`;⛔ 不承载条款级语义(负向断言)

## A3 · 注册

- [x] A3.1 `registerObject` × 2 + `registerDualPortAdapter` × 1 + `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED` × 1
- [ ] A3.2 **并入 A5**:`instantiateAll` 仅作前置;**装配判据 = 首发事务行为断言**(⛔ `instantiateAll==true` 不算)

## A4 · 复现门(G3 · 与 A5 并行)

- [ ] A4.1 `TrafficGenTLM` 新增 **seed / 地址段** setter(`set_mode` 已有);`set_seed` 必须 `rng_.seed()`,`set_addr_range` 必须**重建** `addr_dist_`
- [ ] A4.2 快照记 **种子 + git rev + 工具链**(provenance 与配置指纹分离,⛔ 污染 `config_sha256`)
- [ ] A4.3 同种子**新建实例**两次 run,**固定 tick 预算、同一 binary**:比较集合 = 13 键 + `{cpu_requests, cpu_responses, net_requests_out, net_completions_in, in_flight}` 逐字段相等;并登记「`reset()` 后重跑**不相等**」为反例

## A5 · 两拓扑 + J-9

- [ ] A5.1 `configs/cep_chi_rnf_e2e_a.json`(省略 `cep.bridges` ∧ 全部 connection `latency = 0`)
- [ ] A5.2 `configs/cep_chi_rnf_e2e_b.json`(1 entry / 3 modules / ≥1 latency>0 / **显式 `bridge_id`** / entry 带已声明 `protocol_instance`)
- [ ] A5.3 A3.2 行为判据:两份 config 各跑首拍 ⇒ `wrapper.counters().cpu_requests > 0` ∧ 发射 > 0

## W4 · 桥单槽兜底

- [x] W4.1 **`[bridge-single-slot]`**(⛔ 不复用已被 `test_cep_instance_assembly.cc:185` 占用的 `IAG-08`):`instance_assembly_gate.hh` 增 **`<BSS-01>`** 判据(同一模块属多个 bridge entry ⇒ FAIL)+ 反向 fixture(SCE-12);文档写明「**只测不拦**」(进 Step 7.5 需另裁 —— 动 `module_factory.cc` ⇒ 红线)

## A6 · 判据 J-1 ~ J-9(G2 可达门)

- [x] A6.1 **J-1 / J-2**(SCE-13):装配**行为**判据(⛔ 不止 `instantiateAll`)+ 业务流量抵达 wrapper + 事务抵达对端且**地址一致**(`home.last_request_addr()`)
- [x] A6.2 **J-3**(SCE-14):一请求一响应 + 周期戳闭合 + ⛔ 不跨 `id_space` 配对
- [x] A6.3 **J-5 决定性变异**(SCE-15):判定函数 `j3_response_complete` + **3 个变异体**(丢响应 / 未闭合 / 空真)⇒ 判据**有区分力**
- [x] A6.4 **J-6**(SCE-16):`well_formed()` 正控 + 破坏自洽的变异体按正确原因红 + 「实测 0 ≠ 未测量」渲染断言
- [x] A6.5 **J-7**(SCE-17):测试级 `pack`/`unpack` 4 个共有字段无损 + `flow_id` **降维不可逆** + 丢弃清单 + 三模块同 `bridge_id`
- [x] A6.6 **J-8**(SCE-18):B 的「全绿」**不来自协议语义面** ⇒ 声明面**不含** home 简化模型 / wrapper
- [x] A6.7 **J-9 分档**(SCE-11):A ⇒ 闸 SKIP ∧ 无 `cep.bridges` ∧ 无 latency>0;B ⇒ 闸 PASS ∧ 1 entry / 3 modules / ≥1 latency>0 ∧ 无 error
- [ ] A6.8 **J-4 的 IC3 三态**(✓ 通过 / ❌ 违例 fail / ⚪ 不适用)—— ⏳ **依赖 A8 的 `ic3_verdict` 产出者**(本批只落 **run-validity** 半条)

## A7 · harness 接入 + 三键归属

- [ ] A7.1 `LatencyCollector` / `SwmrDataValueDetector` / `ProgressWatchdog` 接入
- [ ] A7.2 **三键测量归属**(`coherence_stalls` / `collective_stalls` / `directory_misses` 的来源与「真实 0」口径)显式落字

## A8 · 元数据与入库(G4 对账门)

- [ ] W2 `.gitignore`:**`!experimental/run-*/` + `experimental/run-*/*` + 两条白名单**;验收 = `git check-ignore -v experimental/run-x/metrics.json` 无输出;嵌套 run 明示不入库
- [ ] A8.1 台账元数据 **7 字段**(散文段)+ `test/cep_e2e_runs.jsonl`(机器可读)
- [ ] A8.2 **M2 run 定义**:必须由 `configs/cep_chi_rnf_e2e_a.json` 驱动且 `modules` 含 `StubCacheEndpointTLM`(⛔ 不用既有 h3 合成模型 run 顶替)
- [ ] A8.3 `ic3_verdict` **产出者钉死**(脚本 + 两个键 + 三态优先级 `⚪` > `✓`)并**实测**该 run 的落点
- [ ] W3 **双向闸**:零 orphan + `metrics_sha256` 相等(定义覆盖 grandfather 127 run 的豁免边界)
- [ ] A8.4 首批 h3 升级 run 入库(仅 `metrics.json` + `simulation_config.json`)

## A9 · 契约面(合并随 T9.3)

- [ ] A9.1 delta 终稿(含 F-A4 限定句 + 豁免登记 4 项 + 「登记 ≠ 可复算」边界句 + 7 字段 + 「⛔ 不作为 T1–T4 条款级证据」声明句)
- [ ] A9.2 `07 号` / ADR 的**实施后**状态更新(architect 域 · 追加 SU)
- [ ] A9.3 ⚠️ **不在此执行合并**:`:56` 合并 + 冻结链随 **T9.3 归档**一次完成

## W1 · 索引位置闸(独立,不阻塞 T9)

- [ ] W1.1 `scripts/test/cep_index_position_check.sh`(断言索引段后**零个** `## `)+ ctest 注册
- [ ] W1.2 建成后**移除** `cep_doc_ref_check.sh` 的 `VIRTUAL_PATHS` 豁免

## 收尾

- [ ] T9.1 `openspec validate cpptlm-cep-stub-cache-chstream-surface --strict`
- [ ] T9.2 `./test.sh --mode off` 全绿 + 冻结面 41 产物零 drift
- [ ] T9.3 归档(合并 delta + 两步授权链 + KPI 1/3 → 0/3)

> **B1 / B2(ADR 逐批签发 / ADR 结构闸)** 属治理域,**不在本 change DoD** ⇒ 见 `obj §10`,不占 AC、不阻塞归档。
