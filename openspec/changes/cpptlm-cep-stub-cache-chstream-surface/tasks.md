# tasks —— `cpptlm-cep-stub-cache-chstream-surface`

> 顺序 = **A0 → (A1 ∥ A2) → A3 → A5 → A6 → A7 → A8 → A9**;**A4 与 A6 并行**;**W1 独立**;**W2 在 A8 前 / W3 随 A8 / W4 随 A5**。

## A0 · 授权门(G1)

- [ ] A0.1 另立台账 Entry 授权 `openspec/specs/cep-stub-cache-core/spec.md` 的 **`:56` 单条** MODIFIES
- [ ] A0.2 两步授权链就绪:产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单单独 commit → 无参复验

## A1 · `StubCacheEndpointTLM`(端口面)

- [ ] A1.1 `include/tlm/cep/stub_cache_endpoint.hh` + `.cc`(组合式 wrapper;异构 dual 端口)
- [ ] A1.2 公开观测 API(per-txn issue/complete 周期戳 + in-flight 计数)
- [ ] A1.3 映射:op 编码 / 同步 `AccessResult` ⇒ 异步 `CacheRespBundle`(**命中也回 CPU 侧响应**)/ net_side 5 流编码
- [ ] A1.4 `attach_protocol_bridge` override + **只读桥指针访问器**
- [ ] A1.5 `stub_cache_core.hh` **零 diff** + `INV-SC1`~`INV-SC6` 与 14 单测全绿

## A2 · `CohIfResponderTLM`(home 简化模型)

- [ ] A2.1 `include/tlm/cep/cohif_responder.hh` + `.cc`(对称 CohIf 端口)
- [ ] A2.2 Request ⇒ **参数化延迟** Completion;按读**可配发** Snoop;Writeback 接收即消费
- [ ] A2.3 `attach_protocol_bridge` override + 只读桥指针访问器
- [ ] A2.4 ⛔ 不进 `protocol_instances.json`;⛔ 不承载条款级语义(负向断言)

## A3 · 注册

- [ ] A3.1 `registerObject` × 2 + `registerDualPortAdapter` × 1 + `REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED` × 1
- [ ] A3.2 `instantiateAll` 对 A / B 两份 config 均成功

## A4 · 复现门(G3)

- [ ] A4.1 `TrafficGenTLM` 新增 **seed / 地址段 / mode setter**
- [ ] A4.2 config 快照记 **种子 + git rev + 工具链**
- [ ] A4.3 同种子两次 run 计数**逐字段相等**

## A5 · 两拓扑 + J-9

- [ ] A5.1 `configs/cep_chi_rnf_e2e_a.json`(省略 `cep.bridges` ∧ latency=0)
- [ ] A5.2 `configs/cep_chi_rnf_e2e_b.json`(1 entry / 3 modules / ≥1 latency>0 / entry 带已声明 `protocol_instance`)
- [ ] A5.3 **W4** `[IAG-08]`:同一模块属多个 bridge entry ⇒ FAIL

## A6 · 判据 J-1 ~ J-9(G2 可达门)

- [ ] A6.1 `test/test_cep_stub_cache_e2e.cc`(`[cep][e2e]`):J-1 装配 · J-2 事务抵达 · J-3 端到端响应完整
- [ ] A6.2 J-4 IC3 绿(三态)+ run-validity 绿 · J-6 指标自洽
- [ ] A6.3 **J-5 决定性变异**(按正确原因红)
- [ ] A6.4 J-7 桥投影(测试级 `pack`/`unpack` + 逐模块同 `bridge_id`)· J-8 B 不承载全绿 · **J-9 分档**

## A7 · harness 接入 + 三键归属

- [ ] A7.1 `LatencyCollector` / `SwmrDataValueDetector` / `ProgressWatchdog` 接入
- [ ] A7.2 **三键测量归属**(`coherence_stalls` / `collective_stalls` / `directory_misses` 的来源与「真实 0」口径)显式落字

## A8 · 元数据与入库(G4 对账门)

- [ ] A8.1 **W2** `.gitignore` 增 2 条例外 + 注释 provenance 指向 Entry #295
- [ ] A8.2 台账元数据 **7 字段**(散文段)+ `test/cep_e2e_runs.jsonl`(机器可读)
- [ ] A8.3 **W3** 双向闸(零 orphan + sha256 相等)
- [ ] A8.4 首批 h3 升级 run 入库(仅 `metrics.json` + `simulation_config.json`)

## A9 · 契约面 + 实施后状态

- [ ] A9.1 `:56` MODIFIED 合并(两步授权)
- [ ] A9.2 delta 含 **豁免登记 4 项** + 「登记 ≠ 可复算」边界句 + 7 字段 + 「⛔ 不作为 T1–T4 条款级证据」声明句
- [ ] A9.3 `07 号` / ADR 的**实施后**状态更新(architect 域 · 追加 SU)

## W1 · 索引位置闸(独立)

- [ ] W1.1 `scripts/test/cep_index_position_check.sh`(断言索引段后**零个** `## `)+ ctest 注册
- [ ] W1.2 建成后**移除** `cep_doc_ref_check.sh` 的 `VIRTUAL_PATHS` 豁免

## 收尾

- [ ] T9.1 `openspec validate cpptlm-cep-stub-cache-chstream-surface --strict`
- [ ] T9.2 `./test.sh --mode off` 全绿 + 冻结面 41 产物零 drift
- [ ] T9.3 归档(KPI 1/3 → 0/3)
