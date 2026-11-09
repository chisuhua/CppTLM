# tasks —— `cpptlm-cep-dual-port-inbound-dispatch`

> 顺序:**T1(框架修复)→ T2(判据)→ T3(配置)→ T4(拓扑级判据)→ T5(注释)→ T6(验证)→ T7(台账/残余)→ T8(冻结面 SU · 十六,随 commit 轮次)**
> 门:**G1 = T2**(框架修复有牙)· **G2 = T4**(拓扑级回程闭合)· **G3 = T6**(无回归)

## T1 · 框架修复(`include/framework/dual_port_stream_adapter.hh`)

- [x] T1.1 新增端口组常量 `kPeGroupIdx = 0` / `kNetGroupIdx = 1`(`static constexpr` ⇒ **不进对象布局**)
- [x] T1.2 **override 双参** `process_request_input(Packet*, unsigned port_idx)`:组 1 ⇒ `process_net_response_input(pkt)` · 组 0 ⇒ `process_pe_request_input(pkt)`
- [x] T1.3 单参 `process_request_input(Packet*)` 改为**委托给组 0**(既有直接调用者行为不变)
- [x] T1.4 ⛔ **不新增成员变量**(per design §4:`module_factory.cc:733` 的跨实例 `static_cast` 要求布局逐字节相同)
- [x] T1.5 注释写明**为什么按端口组而不是按 `pkt->type`**(CohIf egress 走 `req_out` ⇒ 回程 `Completion` 以 `PKT_RESP` 打在 `req_in` 上)

## T2 · 判据(`test/test_dual_port_inbound_dispatch.cc`,新文件)

- [x] T2.1 **AC-1** net 组入向 ⇒ `net_resp_in()` 收到 ∧ `pe_req_in()` **保持无效**
- [x] T2.2 **AC-2** PE 组入向 ⇒ `pe_req_in()` 收到(既有行为不变)
- [x] T2.3 **AC-3** `process_net_response_input()` 调用点 > 0(原零调用点)
- [x] T2.4 **反向 fixture**:路由改回「恒落 PE 组」⇒ **必红**且红因为「`net_resp_in()` 未收到」
- [x] T2.5 载体形态:**发真包**(经 `ChStreamTargetPort::recvReq` ⇒ 走框架真实入口)⇒ ⛔ 不用「直接写 adapter 成员」冒充入向判据

## T3 · 配置(`configs/cep_chi_rnf_e2e_{a,b}.json`)

- [x] T3.1 `emit_completions: false` ⇒ `true`(A / B 两份)
- [x] T3.2 ⚠️ **补回程连接** `home.0 → w.1`(A / B 两份;B 的 latency 与业务腿一致 = 3)—— CohIf egress 走 `req_out` ⇒ home 的 `req_out[0]` 必须有 peer(实施期实测发现的**第三条条件**)
- [x] T3.3 `description` 同步(**不再**宣称「回程被抑制 / 框架限制」;并登记「仍不承载全绿 run」的 IC3 边界)
- [x] T3.4 ⚠️ 既有 run 的 `config_sha256` 指纹**不覆盖** ⇒ 重跑**追加新 run_id**

## T4 · 拓扑级判据(`test/test_cep_stub_cache_e2e.cc`)

- [x] T4.1 **AC-4** SCE-09 断言翻转:`completions_sent == 0` ⇒ `> 0`;`completions_suppressed > 0` ⇒ `== 0`
- [x] T4.2 追加回程腿闭合判据:`w->counters().net_completions_in > 0` ∧ `w->counters().cpu_responses > 0`
- [x] T4.3 **反向 fixture**:config 回退 `emit_completions: false` ⇒ 必红

## T5 · 注释(`include/tlm/cep/cohif_responder.hh`)

- [x] T5.1 `Config::emit_completions` 注释改写:原注释称「dual-port 适配器的入向路由恒落 PE 侧(框架限制)」⇒ 已由本 change 解除 ⇒ 改为说明该开关**仍可用于**把回程腿留在模块级判据(不删功能)
- [x] T5.2 `completions_suppressed` 计数器的注释同步

## T6 · 验证

- [x] T6.1 构建 rc=0
- [x] T6.2 **AC-5** `ctest` 全绿且**用例数不减**(基线 166)⇒ 逐条核对 `[dualport]` / `[cep]` / mesh 相关用例
- [x] T6.3 `./test.sh --mode off`(Python + Catch2 + TLM E2E)绿
- [x] T6.4 **AC-6** `B3b-2` 维持 0/4;`test/cep_b3b2_deferrals.json` / `protocol_instances.json` / `protocol_instance.schema.json` / `stub_cache_core.hh` / `cohif_noc_bridge.hh` **零 diff**
- [x] T6.5 `openspec validate <change> --strict` 绿

## T7 · 台账与残余

- [x] T7.1 台账 **Entry #308**(实施回执:改动清单 + 验证证据 + 关联)
- [x] T7.2 **R-5** 残余登记(`bidirectional_port_adapter.hh:335` 签名不匹配 ⇒ 恒用端口 0)⇒ 含实测证据
- [x] T7.3 台账 **Entry #307 ② 的 E-7 勘误**更正:交接件原文「入向按端口路由」**正确**;先前的「按包类型」判断**错误**(CohIf egress 走 `req_out` ⇒ 按 type 会把响应投到请求通道)

## T8 · 冻结面 SU · 十六(随 commit 轮次)

- [ ] T8.1 `docs/architecture/07-coherence-exploration-platform.md` 追加 **SU · 十六**(残余缺口闭合登记 + 新残余 R-5)
- [ ] T8.2 **五步授权链**:台账 Entry 授权 → 产物 commit → `cep_freeze_check.sh --update --authorized-by "Entry #N"` → 清单单独 commit → 无参复验
- [ ] T8.3 ⛔ 不改写 SU · 一 ~ 十五 的任何一段(append-only)
