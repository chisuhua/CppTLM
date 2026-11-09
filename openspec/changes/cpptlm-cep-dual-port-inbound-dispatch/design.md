# design —— `cpptlm-cep-dual-port-inbound-dispatch`

> **依据**:proposal.md 的 Why(根因表)· **角色**:builder · **日期**:2026-11-09

## 1 · 端口组 ↔ 物理端口的对应关系(修复的事实基础)

`src/core/module_factory.cc` Step 7 对**双端口**类型创建 `n_ports = 2` 组物理端口(`:697-707`):

| 端口组 | `req_out` | `resp_in` | `req_in` | `resp_out` | 绑定(`:737-739`) |
|---|---|---|---|---|---|
| **0 = PE 侧** | `req_out_vec[0]` | `resp_in_vec[0]` | `req_in_vec[0]` | `resp_out_vec[0]` | `bind_pe_ports(...)` |
| **1 = Net 侧** | `req_out_vec[1]` | `resp_in_vec[1]` | `req_in_vec[1]` | `resp_out_vec[1]` | `bind_net_ports(...)` |

每个 `ChStreamTargetPort`(`req_in[i]` / `resp_in[i]`)携带 **`port_idx_ = i`** ⇒ **入向包的下标 = 端口组下标**。

⚠️ `req_in[i]` 与 `resp_in[i]` **共享同一 `port_idx_`** ⇒ **下标只区分「组」,不区分「请求/响应通道」** ⇒ 路由**必须按组**,且**不能**按 `pkt->type` 反推组。

## 2 · 连接如何落到物理通道(`src/core/module_factory.cc:1063-1094`)

对连接 `src: "A.i" → dst: "B.j"`:

| 方向 | 物理通道 | 包的 `type` |
|---|---|---|
| 正向 | `ch_req_out[A][i]` ↔ `ch_req_in[B][j]` | `PKT_REQ`(发端 `send(port, PKT_REQ)`) |
| 反向 | `ch_resp_out[B][j]` ↔ `ch_resp_in[A][i]` | 由发端的 `send(port, <traits 决定>)` 决定 |

⇒ CEP 拓扑 A 的回程:`home` 的 `Completion` 按 `CohIfDirectionTraits::packet_type`(`include/tlm/cep/cohif_direction_traits.hh:34-39`)= **`PKT_RESP`**,但按 `egress_channel()`(`:45-56`)= **`Req`** ⇒ 打在 **`home.req_out[0]`** ⇒ 抵达 **`w` 的 `req_in[1]`** ⇒ **(组 1, PKT_RESP)**。

## 2b · 拓扑连接:回程腿需要**显式的反向连接**(实施期发现)

`src/core/module_factory.cc:1063-1094` 对连接 `src: "A.i" → dst: "B.j"` 建**两个** pair:

| 方向 | pair | 承载 |
|---|---|---|
| 正向 | `ch_req_out[A][i]` ↔ `ch_req_in[B][j]` | `A` 发起的请求 / CohIf 的**数据通道** |
| 反向 | `ch_resp_out[B][j]` ↔ `ch_resp_in[A][i]` | 传统「请求-响应」语义下的响应通道 |

⇒ CohIf 的 egress 走 **`req_out`** ⇒ 回程 `Completion` 从 **`home.req_out[0]`** 发出 ⇒ **必须**有一条 `home.0 → w.1` 的连接把它绑上;否则 `home.req_out[0]` 无 peer ⇒ `send()` 失败 ⇒ 回程发不出去。

| 拓扑 | 修复前 | 修复后 |
|---|---|---|
| A | `tg → w.0` · `w.1 → home.0` | `tg → w.0` · `w.1 → home.0` · **`home.0 → w.1`** |
| B | `w.1 → home.0`(lat 3)· `chi.0 → home.1` | `w.1 → home.0`(lat 3)· **`home.0 → w.1`(lat 3)**· `chi.0 → home.1` |

⇒ **实测证据**:仅做「适配器修复 + `emit_completions: true`」时 `home.completions_sent > 0` ∧ `w.net_completions_in == 0` ⇒ 回程仍不可达 ⇒ **第三条条件成立**。

## 3 · 修复后的路由表
| 到达位置 | `port_idx` | `pkt->type` | 目标通道 | 模块 API | 说明 |
|---|---|---|---|---|---|
| `req_in[0]` / `resp_in[0]` | 0 | 任意 | `pe_req_in()` | `InputStreamAdapter<PE_ReqBundleT>` | PE 侧业务请求入向(**既有行为不变**) |
| `req_in[1]` / `resp_in[1]` | 1 | 任意 | `net_resp_in()` | `InputStreamAdapter<Net_RespBundleT>` | Net 侧入向(**本次修复** ⇒ 原恒落 PE 侧) |

⇒ **按组路由**覆盖两个真实场景:
- **CEP**:`Completion` 以 `PKT_RESP` 落 net 组 ⇒ `net_resp_in()` ✓
- **NICTLM**(mesh 拓扑):路由器经 `resp_out` 回的 flit 落 net 组 ⇒ `net_resp_in()` ✓(与 `NICTLM` 的文档契约「`net_resp_in()` — 从 Router 接收」一致)

⚠️ **PE 组的响应入向**(组 0 + `PKT_RESP`)在双端口模块 API 中**无对应通道**(PE 侧只有 `pe_req_in` 入 + `pe_resp_out` 出)⇒ 维持既有投递(落 `pe_req_in()`)以避免引入**静默丢包**;该通道在现存拓扑中**无流量**(双端口模块从不向 PE 侧发请求 ⇒ 对端不会回响应)。

## 4 · 实现形态(约束驱动)

```cpp
// 端口组下标(与 `ModuleFactory` Step 7 的双端口绑定一致:组 0 = PE · 组 1 = Net)
static constexpr unsigned kPeGroupIdx = 0;
static constexpr unsigned kNetGroupIdx = 1;

void process_request_input(Packet* pkt) override {          // 既有单参入口:保持 PE 组语义
    process_request_input(pkt, kPeGroupIdx);
}

void process_request_input(Packet* pkt, unsigned port_idx) override {   // ★ 框架唯一入口
    if (!pkt || !pkt->payload) return;
    if (port_idx == kNetGroupIdx) process_net_response_input(pkt);   // net 组 ⇒ net 侧入向
    else                          process_pe_request_input(pkt);    // PE  组 ⇒ PE  侧入向
}
```

| 约束 | 处置 |
|---|---|
| `src/core/module_factory.cc:733` 的 `static_cast` 跨模板实例化 ⇒ **成员布局必须逐字节相同** | ⛔ **不新增成员变量**;`process_net_response_input()` 复用既有成员(零调用点 → 变活) |
| 基类双参版会回退单参版 | **必须** override 双参版(否则下标仍丢失) |
| `process_net_response_input()` 原本零调用点 | 复用它作为 net 组投递点 ⇒ AC-3「调用点 > 0」可直接观测 |

## 5 · 判据设计(有牙)

| 判据 | 载体 | 变异体(决定性) | 变异后 |
|---|---|---|---|
| **AC-1** net 组入向 ⇒ `net_resp_in()` | `test/test_dual_port_inbound_dispatch.cc` | 把路由改回「恒落 PE 组」(旧行为) | **必红**,且红因为「`net_resp_in()` 未收到」 |
| **AC-2** PE 组入向 ⇒ `pe_req_in()` | 同上 | 把路由改反 | 必红 |
| **AC-4** 拓扑级回程闭合 | `test/test_cep_stub_cache_e2e.cc` SCE-09 | config 回退 `emit_completions: false` | 必红(`completions_sent == 0`) |

⚠️ **不做「断言存在」式伪判据**:每条判据都断言**通道归属 + 计数器数值**,且都有**能使其变红**的反向路径。

## 6 · 影响面(blast radius)与回归要求

| 受影响使用者 | 现状 | 修复后 | 风险处置 |
|---|---|---|---|
| `StubCacheEndpointTLM`(CEP) | net 侧入向**不可达** | 闭合 | ✅ 目标收益 |
| `NICTLM`(mesh) | 路由器回的 flit **恒落 PE 组** ⇒ 被当 `CacheReqBundle` 读 ⇒ 类型混淆 | 落 `net_resp_in()`(与 `NICTLM` 文档契约一致) | ⚠️ **行为变化** ⇒ 必须跑全量 `ctest` 并逐条核对 mesh / phase6 用例 |
| `RouterTLM`(5 端口) | **不受本次改动影响**(走 `BidirectionalPortAdapter`) | 同左 | ⇒ 其同类缺陷登记 **R-5** |

⇒ **AC-5** 要求 `ctest` 用例数**不减**且全绿;若 mesh 用例因 `NICTLM` 行为变化而红 ⇒ **不得**回退修复,须逐条判定是「断言需更新」还是「真回归」。

## 7 · 残余登记

- **R-5**:`include/framework/bidirectional_port_adapter.hh:335` 的 `process_request_input(Packet*, std::size_t)` 与基类 `process_request_input(Packet*, unsigned)` **签名不匹配** ⇒ **不是 override**(该处亦无 `override` 关键字)⇒ 基类双参版回退到单参 ⇒ 该适配器**恒用端口下标 0** ⇒ `RouterTLM`(5 端口)与全部 CEP 引擎(2 端口)的**多端口入向路由失效**。**不在本 change 修**(影响面远大于 R-2)⇒ 另立 change + 单独验证。

## 8 · 冻结面纪律

| 面 | 本 change 是否触碰 | 处置 |
|---|---|---|
| `include/framework/dual_port_stream_adapter.hh` | ✅ 触碰 | **不在 M1 冻结集** ⇒ 无需 `--update` 授权;但属**共享框架层** ⇒ 已用 openspec change 立项 |
| `include/tlm/cep/cohif_responder.hh` | ✅ 仅注释 | **不在 M1 冻结集** ✓ |
| `configs/cep_chi_rnf_e2e_{a,b}.json` | ✅ | 不在冻结集 ✓ |
| `test/**` | ✅ | 不在冻结集 ✓ |
| `docs/architecture/07-*.md`(SU · 十六) | ⛔ 本 change **不写** | 随 commit 轮次走**五步授权链** |
| `docs/adr/ADR-CEP-*` | ⛔ 不动 | — |
