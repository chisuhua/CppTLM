# design: cpptlm-cep-mesh-e2e-carrier

> **上游**: `proposal.md` · 台账 **Entry #444** §2.2 类 B · §3 波 2
> **本设计的事实基础全部为 2026-11-16 实测**(每条给出行号)—— ⛔ **不采信台账措辞推断**

---

## D-1 · `CohIfNICTLM` 设计

### D-1.1 · 为什么必须是**新模块**而不是复用 `NICTLM` 或 `CohIfNoCProtocolBridge`

| 候选 | 为什么不行 |
|---|---|
| 复用 `NICTLM` | 它的 PE 侧端口是 `CacheReqBundle`(`include/tlm/nic_tlm.hh:72`)⇒ 与 `CohIfBundle` **类型不通** ⇒ 要复用就得给它加第二套端口面 ⇒ 改框架层 + 触 dGPU 侧 |
| 复用 `CohIfNoCProtocolBridge` | 它是**挂在 PE 上的类型转换器**(`ProtocolEngineBase` 持 `bridge_` 单槽),不是 ChStream 模块 ⇒ 没有自己的端口 ⇒ **无法插在 PE 与 Router 之间**;且 `pack` 明确「**不清零 `out` 的其余字段 —— 桥无权改写路由器负责的字段**」⇒ 桥**不填** `dst_node` ⇒ 路由不了 |
| **新增 `CohIfNICTLM`** | ✅ PE 侧 `CohIfBundle` + 网络侧 `NoCFlitBundle` + **自己填 `dst_node`** ⇒ 与 `NICTLM` 的职责同构(NIU 层),但 PE 侧 Bundle 类型是 CohIf ⇒ **不动框架层** |

### D-1.2 · 端口面(异构 dual-port · 4 端口)

```cpp
class CohIfNICTLM : public ChStreamModuleBase {
    // PE 侧(CohIf 业务语义)
    cpptlm::InputStreamAdapter<bundles::CohIfBundle>&  pe_req_in();   // 进
    cpptlm::OutputStreamAdapter<bundles::CohIfBundle>& pe_resp_out();  // 出
    // 网络侧(NoC 传输载体)
    cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>& net_req_out(); // 出
    cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>&  net_resp_in();  // 进
};
```

⛔ **不得用对称 `REGISTER_CHSTREAM_BIDIRECTIONAL`**(它要求两侧同 BundleT,`include/chstream_register.hh:85-93`)⇒ 走 `registerDualPortAdapter`(`include/framework/chstream_adapter_factory.hh:81-92`),范式照 `NICTLM` 与 `StubCacheEndpointTLM`(`include/chstream_register.hh:243-253`)。

### D-1.3 · 正向映射表(CohIf → NoCFlit)

| NoCFlitBundle 字段 | 取值 | 依据 |
|---|---|---|
| `transaction_id` | `in.transaction_id.read()` | 层 1 共有字段 |
| `address` | `in.address.read()` | 层 1 共有字段 |
| `data` | `in.data.read()` | 层 1 共有字段 |
| `size` | `in.size.read()` | 层 1 共有字段 |
| **`dst_node`** | **`lookup_node(in.address.read())`** | ⚠️ **本模块的核心职责** —— 桥不填(`cohif_noc_bridge.hh` 的「桥无权改写路由器负责的字段」)· `RouterTLM` 只从 `flit.bundle.dst_node.read()` 取目标(`src/tlm/router_tlm.cc:179`) |
| `src_node` | `node_id_`(配置 `node_id`) | 网络封装字段 |
| `vc_id` | `0`(默认) | `CohIfBundle` 无 `vc_id`(ADR-CEP-19 决策 3)⇒ 单 VC |
| `flit_type` | `FLIT_HEAD_TAIL` | `CohIfBundle.data` 是 **1 拍**载荷(ADR-CEP-02)⇒ **单 flit 足够** |
| `flit_index` / `flit_count` | `0` / `1` | 同上 |
| `hops` | `0` | 路由器负责 |
| `flit_category` | `Request`→`CATEGORY_REQUEST`;其余 4 流→`CATEGORY_RESPONSE` | ⚠️ **有损**(见 Q1)· 垂直切片内无歧义 |
| `is_write` | `in.perm.read() == PERM_UNIQUE \|\| PERM_WRITEBACK` | 写意图的可观测代理(⚠️ 近似,登记为已知边界) |

### D-1.4 · 反向映射表(NoCFlit → CohIf)

`transaction_id` / `address` / `data` / `size` 反向恢复;`flow_id` **不可恢复** ⇒ 由 `flit_category` 恢复为 `Completion`(CATEGORY_RESPONSE)或 `Request`(CATEGORY_REQUEST)⇒ ⚠️ **垂直切片内唯一响应是 Completion ⇒ 无歧义**。

### D-1.5 · 地址映射与**显式失败**

配置形态照 `NICTLM`(`src/tlm/nic_tlm.cc:36-40`):

```json
"params": { "node_id": 0, "mesh_x": 2, "mesh_y": 2,
            "address_regions": [ {"base": "0x1000", "size": "0x1000", "target_node": 3} ] }
```

⛔ **未命中 ⇒ 显式丢弃 + 计数**(`stats_dropped_unmapped_` ++)· **不**静默投 node 0。
⇒ **与 `NICTLM` 的差异是刻意的改进**:`NICTLM::lookup_node` 未命中**静默返回 0**(`include/tlm/nic_tlm.hh:44-51`)⇒ 在多节点 mesh 上会把包送到错误节点且无任何信号。本模块**不得**复制该缺陷。⚠️ `NICTLM` 自身的缺陷**不在本 change**(登记 Q3)。

### D-1.6 · 观测面(公开 · 原始事实)

**出站侧**(本模块自己打包时):
- `stats_flits_sent_` / `stats_flits_received_` / `stats_dropped_unmapped_`
- `last_dst_node()` —— 本模块**出站** flit 的查表结果

**入站侧**(本模块**收到**的 flit):
- `last_recv_dst_node()` —— 收到的 flit 的 `dst_node` ⇒ **「确实投给对端」的可观测量**
- `last_recv_hops()` —— 收到的 flit 的 `hops` ⇒ **「确实穿过了 mesh」的可观测量**

⚠️ **critic SHOULD_FIX-2/3 订正(2026-11-16)**:原设计只有 `last_dst_node()`(出站查表结果)⇒ 在垂直切片里 `nic_b` 的唯一出站 flit 是 Completion,而 `lookup_node(Completion.address)` 命中不了对端区间 ⇒ 该值恒为 0 ⇒ **J-2 必红或空转**;且 `hops` 是 flit 在 router 内的**瞬态**量(flit 出 router 后即离开)⇒ 无观测通道时 J-4 必然返工。
⇒ 故**入站侧另设两个可观测量**。⚠️ `last_recv_hops()` 的判别力依据:router 在**转发**时递增 `hops`(`src/tlm/router_tlm.cc:310`:`waiting_flit.bundle.hops.write(hops.read() + 1)`)⇒ 直连(不经 router)⇒ `hops == 0` ⇒ **可区分「过 mesh」与「直达」**。

⛔ **不**提供 latency 聚合 / 指标组装(per `cpptlm-cep-stub-cache-chstream-surface` 的「观测面只给原始事实」纪律)。

---

## D-2 · 联合拓扑设计

### D-2.0 · 连接绑定语义（**已由 T1.3 探针实测定稿** · critic SHOULD_FIX-1）

**探针**:`test/test_zz_mesh_probe.cc`(5 个用例 · 2026-11-16 实跑)⇒ 探针结论**已回写**本节。

**实测事实**:

| # | 事实 | 位置 |
|:--:|---|---|
| P-1 | 一条 connection 创建**两个** PortPair:req 对 `src.req_out[i] → dst.req_in[j]` + resp 对 `dst.resp_out[j] → src.req_in[i]` | `src/core/module_factory.cc:1167-1184` |
| P-2 | `RouterTLM` 的 flit **egress 走 `resp_out[p]`**(`NoCFlitBundle` 的 trait `egress_channel == Resp`);`resp_out_port_[p]` **为空时回退** `req_out_port_[p]` | `include/framework/bidirectional_port_adapter.hh:236-244` |
| P-3 | `DualPortStreamAdapter` 的 **Net 侧 egress 走 `req_out[1]`**(`net_req_out_port_`,**无回退**)· Net 侧 ingress 是 `req_in[1]` | `include/framework/dual_port_stream_adapter.hh:166-178` |
| P-4 | **`CohIfBundle` 的 egress_channel == `Req`** ⇒ CohIf 侧 egress 走 `req_out`(⛔ 不是 `resp_out`) | `include/tlm/cep/cohif_direction_traits.hh` |
| P-5 | `RouterTLM` 5 端口方向:NORTH=0 / EAST=1 / SOUTH=2 / WEST=3 / LOCAL=4 · `reverse_port` 在 `include/tlm/router_tlm.hh:223-232` | `include/tlm/router_tlm.hh:25` + `:223-232` |
| P-6 | XY 路由是 **X 优先**(先走到目标列,再走到目标行) | `src/tlm/router_tlm.cc:23-31` |
| P-7 | router 转发时 `hops += 1` | `src/tlm/router_tlm.cc:310` |

**探针实测结论（5 个用例的对照实验）**:

| 变体 | 实测输出 | 结论 |
|---|---|---|
| router↔router **1 条** | `ni1.packets_received=0` · `r0.flits=4` · `r1.flits=0` | ❌ **1 条不够** —— flit **停在源 router r0**,从不进 r1 |
| router↔router **2 条**(对称) | `ni1.packets_received=4` · `r0.flits=4` · `r1.flits=4` · `r1.hops=8` | ✅ **2 条才行** |
| router↔NIC 只给 **1 条** | `ni1.packets_received=0` · `r0.flits=4` | ❌ **确实需 2 条** |
| 2×2 mesh(去程) | `ni0.sent=4` · `ni1.recv=4` · `r00.hops=4` `r01.hops=8` `r11.hops=12` | ✅ **端到端可达** · hops 逐跳 1→2→3 |
| 2×2 mesh(**回程**,补 cpu1) | `ni1.sent=10` · `ni1.recv=14` · `ni0.recv=13` · `r10.flits=8` | ✅ **双向可达** · 回程**确实走 X-first 路径经 r10** · 4 个 router **全部参与** |

⇒ **定稿的连接规则**:

| 连接类型 | 条数 | 理由(实测支撑) |
|---|:---:|---|
| **router ↔ router** | **2 条**(对称) | 缺任一 ⇒ flit 停在源 router(P2 的回退路径只在 `resp_out` 未绑定时生效,而源 router 的 `resp_out[p]` **恰好已被对端 connection 绑定** ⇒ 两条才形成完整环) |
| **router ↔ NIC** | **2 条**(对称) | NIC 的 Net 侧 egress 走 `req_out[1]` 且**无回退**(P3)⇒ 必须有 `{src:"nic.1", dst:"router.p"}`;而 router→NIC 需 `{src:"router.p", dst:"nic.1"}` |
| **stub ↔ NIC**(CohIf 侧) | **1 条** | CohIf egress 走 `req_out`(P4)⇒ `{src:"w.1", dst:"nic_a.0"}` ⚠️ **此条未实测**(探针用的是 `NICTLM` 的 CacheReq 侧)⇒ **实施时须验** |

**2×2 mesh 的环结构(X-first ⇒ 去程与回程走不同边)**:

```
   node0=(0,0) ──X── node1=(1,0)
      │ r00            │ r01
      └──────Y─────────┘
   node2=(0,1) ──X── node3=(1,1)
      │ r10            │ r11
      └──────Y─────────┘
```
⇒ 去程 node0→node3 = `r00 →(X,E) r01 →(Y,N) r11`;回程 node3→node0 = `r11 →(X,W) r10 →(Y,S) r00` ⇒ **4 条 router-router 边构成环**,每条 2 个方向 = **8 条 connection** ⇒ **4 个 router 全部参与**(⛔ 不得有零连接的空闲 router)。

⚠️ **全仓先例的真相**:`configs/mesh_2x2_tlm.json` / `mesh_4x4_tlm.json` 的 NIC **均无 `address_regions`** ⇒ `NICTLM::lookup_node` 未命中**静默返回 0**(`include/tlm/nic_tlm.hh:44-51`)⇒ 那两个配置实为「**全流量 → node 0** 的单向生成树」;其 e2e 测试(`test/test_complex_topologies_from_config.cc:47-68`)**零 flit 流动断言** ⇒ **不能作为先例** —— 本节的探针是全仓**首次**对「JSON 装配下 router 间双向 flit 链路」的实测。

### D-2.1 · 拓扑（**已按探针实测定稿**）

`configs/cep_mesh_e2e.json` —— **2×2 mesh**(4 router)。

**模块清单(9 实例 / 6 类型)**:

| 名字 | 类型 | 角色 |
|---|---|---|
| `tg` | `TrafficGenTLM` | 业务流量源(产生 `CacheReqBundle`) |
| `w` | `StubCacheEndpointTLM` | stub 端点(CohIf 侧) |
| `home` | `CohIfResponderTLM` | home 简化模型(应答 Completion) |
| `nic_a` / `nic_b` | `CohIfNICTLM` | **新增** · 两个网络接口(node 0 / node 3) |
| `r00` / `r01` / `r10` / `r11` | `RouterTLM` | mesh 路由器 |

⚠️ **不含协议引擎**(critic SHOULD_FIX-4):`ADR-CEP-19` SU 2026-11-09 已登记拓扑 A 实施态 = **`tg→w→home` 三模块直连**,**引擎对未采纳**,理由 = 「RN-F **不服务 request**(只记 `foreign_event`、不回 Completion)⇒ 业务事务必须直连 home」⇒ 把 `chi` 列入清单会既不承载流量、又制造「协议在 mesh 环内」的**错误印象** ⇒ 本 change **删掉 `chi`**,边界句见 D-5 第 5 条。

**连接清单(14 条 · 探针实测定稿)**:

| # | src | dst | 语义 |
|:--:|---|---|---|
| 1 | `tg` | `w.0` | 业务流量入 stub |
| 2 | `w.1` | `nic_a.0` | stub → 网络(CohIf)⚠️ 此条实施时须验 |
| 3 | `nic_a.1` | `r00.4` | NIC → mesh(LOCAL) |
| 4 | `r00.4` | `nic_a.1` | mesh → NIC(回程) |
| 5 | `r00.1` | `r01.3` | 环边 X:E↔W(去程 r00→r01) |
| 6 | `r01.3` | `r00.1` | 同上反向 |
| 7 | `r01.0` | `r11.2` | 环边 Y:N↔S(去程 r01→r11) |
| 8 | `r11.2` | `r01.0` | 同上反向 |
| 9 | `r11.3` | `r10.1` | 环边 X:E↔W(回程 r11→r10) |
| 10 | `r10.1` | `r11.3` | 同上反向 |
| 11 | `r10.2` | `r00.0` | 环边 Y:N↔S(回程 r10→r00) |
| 12 | `r00.0` | `r10.2` | 同上反向 |
| 13 | `r11.4` | `nic_b.1` | mesh → NIC(去程终点) |
| 14 | `nic_b.1` | `r11.4` | NIC → mesh(回程起点) |
| 15 | `nic_b.0` | `home.0` | 网络 → home(CohIf) |
| 16 | `home.0` | `nic_b.0` | home → 网络(CohIf) |
| 17 | `nic_a.0` | `w.1` | 网络 → stub(CohIf 回程) |

⚠️ **`address_regions`**: `nic_a` 须把 `tg` 的地址段(`CPUTLM`/`TrafficGenTLM` 的 `0x1000~0x1100`)映射到 **node 3**;`nic_b` 须把同一区间映射回 **node 0**(回程)⇒ ⛔ **两处都要配**,否则回程的 `lookup_node` 未命中 ⇒ **显式丢弃**(本模块的显式失败,见 D-1.5)。

⚠️ **端到端跳数**:探针实测 2×2 去程 = **3 跳**(r00→r01→r11)⇒ J-4 的 `last_recv_hops() >= 1` 判据**有充分裕量**(实际应为 3)。

## D-3 · 判据设计(J-1 ~ J-6)

载体:`test/test_cep_mesh_e2e.cc` · 标签 `[cep][e2e][mesh]`。

| # | 判据 | 断言 | 关键点 |
|:--:|---|---|---|
| **J-1** | 装配 | `instantiateAll` rc=0 + **9 个实例**全部在位 | 装配失败 ⇒ 后续用例**无意义** ⇒ J-1 独立 |
| **J-2** | **正向抵达对端** | `nic_b` 的接收计数 == 1 **且** `nic_b.last_recv_dst_node() == 3` | ⚠️ **critic SHOULD_FIX-2 订正**:原写 `last_dst_node()`(那是**出站**查表结果)⇒ 在切片内 `nic_b` 唯一出站是 Completion ⇒ 该值恒为 0 ⇒ J-2 必红 ⇒ 改用**入站**可观测量 `last_recv_dst_node()`。必须断言**对端**(⛔ 只断言「计数>0」会被「回环到自己」骗过) |
| **J-3** | **反向抵达源端** | `w` 的 per-txn 观测:complete 周期戳存在 + in-flight 归零 | 用 `StubCacheEndpointTLM` **既有**公开 API(⛔ 不改它) |
| **J-4** | **确实穿过了 mesh** | 参与路径的 router 的 `flits_forwarded` > 0 **且** `packets_forwarded` > 0 **且** `nic_b.last_recv_hops() >= 1` | ⚠️ **AC-05 的核心** —— 「抵达」不足以证明「过了 mesh」(直达也能抵达)· ⚠️ **critic SHOULD_FIX-3 订正**:原写「至少一个 flit 的 `hops` ≥ 1」**没有命名可观测量**(flit 在 router 内是瞬态的,出 router 即离开)⇒ 改用 `nic_b.last_recv_hops()`(入站侧可观测量)。判别力依据:router 转发时 `hops += 1`(`src/tlm/router_tlm.cc:310`)⇒ 直连 ⇒ `hops == 0` ⇒ **可区分「过 mesh」与「直达」** |
| **J-5** | **决定性变异** | 断开 `r01→r11` 一条连接 ⇒ 重跑 ⇒ SHALL 红 **且** 失败原因 == 「未抵达」(⛔ **不是**「装配失败」) | ⚠️ **「按正确原因红」是硬要求**(per 台账 Entry #154 教训:变异「因错误的原因红」= 判据无牙)⇒ 须用 `ReachOutcome` 式**结局分类**枚举(照 `test/cep_cohif_assembly_helper.hh:35-40` 的既有范式),⛔ **不**用 `REQUIRE(!arrived)` |
| **J-6** | **既有面零回归** | 既有 `[cep]` / `[e2e]` 全绿 + 8 个既有文件零 diff | — |

---

## D-4 · 关键技术事实登记表（**防脑补** · 全部实测）

| # | 事实 | 位置 |
|:--:|---|---|
| F-1 | `CohIfBundle` 12 字段,**无** `dst_node`/`src_node`/`vc_id` | `include/bundles/cep_coherence_bundles_tlm.hh:94-114` |
| F-2 | `CohIfBundle.data` 是 **1 拍**载荷 ⇒ 单 flit 足够 | 同上 `:98` + ADR-CEP-02 |
| F-3 | `RouterTLM` 路由**只**从 `flit.bundle.dst_node.read()` 取目标 | `src/tlm/router_tlm.cc:179` |
| F-4 | `dst_node` 由 NIU 层填(`lookup_node(address)`) | `src/tlm/nic_tlm.cc:98,108` |
| F-5 | `NICTLM::lookup_node` 未命中**静默返回 0** | `include/tlm/nic_tlm.hh:44-51` |
| F-6 | `NoCFlitBundle` **无 payload 字段**(仅 8 字节 `data`)⇒ 层 2 字节等价在 NoC 上不可行 | `include/bundles/noc_bundles_tlm.hh:28-49` |
| F-7 | `flit_category` 仅 3 值(REQUEST/RESPONSE/CREDIT)⇒ 装不下 5 个 `flow_id` | `include/bundles/noc_bundles_tlm.hh:45` |
| F-8 | 桥**不填** `dst_node` | `include/tlm/cep/cohif_noc_bridge.hh` 文件头「桥无权改写路由器负责的字段」 |
| F-9 | `StubCacheEndpointTLM` 的 CohIf 侧端口 = `OutputStreamAdapter<CohIfBundle>` / `InputStreamAdapter<CohIfBundle>` ⇒ 与本模块 PE 侧**同类型可连** | `include/tlm/cep/stub_cache_endpoint.hh:96-97` |
| F-10 | `StubCacheEndpointTLM` 有 per-txn 观测 API(issue/complete 周期戳 + in-flight) | `include/tlm/cep/stub_cache_endpoint.hh` 公开 API 段 |
| F-11 | `registerDualPortAdapter` 模板参数 = (ModuleT, PE_Req, PE_Resp, Net_Req, Net_Resp) | `include/framework/chstream_adapter_factory.hh:81-92` |
| F-12 | `RouterTLM` 5 端口方向:NORTH=0/EAST=1/SOUTH=2/WEST=3/LOCAL=4 · `reverse_port` 在 `:223-232` | `include/tlm/router_tlm.hh:25` + `:223-232` |
| F-13 | 一条 connection 创建**两个** PortPair:req 对 `src.req_out[i]→dst.req_in[j]` + resp 对 `dst.resp_out[j]→src.req_in[i]` | `src/core/module_factory.cc:1167-1184` |
| F-14 | `NoCFlitBundle` 的 egress 走 `resp_out[p]`(trait `egress_channel == Resp`),`resp_out_port_[p]` 为空时**回退** `req_out_port_[p]` | `include/framework/bidirectional_port_adapter.hh:236-244` |
| F-15 | `DualPortStreamAdapter` 的 Net 侧 egress 走 `req_out[1]`(`net_req_out_port_`,**无回退**)· Net 侧 ingress 是 `req_in[1]` | `include/framework/dual_port_stream_adapter.hh:166-178` |
| F-16 | **`CohIfBundle` 的 egress_channel == `Req`** ⇒ CohIf 侧 egress 走 `req_out`(**不是** `resp_out`) | `include/tlm/cep/cohif_direction_traits.hh` |
| F-17 | XY 路由是 **X 优先**(先走到目标列,再走到目标行)⇒ 回程路径可能绕行 | `src/tlm/router_tlm.cc:23-31` |
| F-18 | router 转发时 `hops += 1` ⇒ 直连(不经 router)的 flit `hops == 0` | `src/tlm/router_tlm.cc:310` |
| F-19 | `mesh_2x2_tlm.json` / `mesh_4x4_tlm.json` 的 NIC **无 `address_regions`** ⇒ `lookup_node` 静默投 0 ⇒ 二者实为「**全流量 → node 0** 的单向生成树」;其 e2e 测试**零 flit 流动断言** ⇒ **全仓无「JSON 装配下 router 间双向 flit 链路」的实测先例** | `configs/mesh_2x2_tlm.json` · `test/test_complex_topologies_from_config.cc:47-68` |
| F-20 | `ADR-CEP-19` SU 2026-11-09 登记:拓扑 A 实施态 = `tg→w→home` 三模块直连,**引擎对未采纳**(RN-F 不服务 request ⇒ 业务事务必须直连 home) | `docs/adr/ADR-CEP-19*.md` SU 2026-11-09 |

---

## D-5 · 边界声明（落 delta spec）

1. `CohIfNICTLM` 是 CEP 的**第五类模块**(adapter / engine / bridge / home 模型 / **network adapter**)⇒ **不**进 `protocol_instances.json`(⛔ 它不是协议实例)· **不**计入 `B3b-2`(⛔ 未达「转移表真源」判定)· **不**作为 T1–T4 条款级证据。
2. 本模块的映射**仅**存在于本模块(⛔ 映射代码不得外溢到 `include/framework/` 或 `include/tlm/nic_tlm.hh`)。
3. `flit_category` 的 5→3 投影**有损** ⇒ 本模块**不**承诺多流无损(⚠️ Q1 待 architect 裁决)。
4. 本模块**不**承载协议语义(credit / retry / ordering point / 多拍聚合 / Perm 规则 / 容量与队列模型)。
5. ⚠️ **协议引擎不参与本切片的业务事务路径**(critic SHOULD_FIX-4):`ADR-CEP-19` SU 2026-11-09 已登记「RN-F **不服务 request**(只记 `foreign_event`、不回 Completion)⇒ 业务事务必须直连 home」⇒ 本切片验证的是「**stub ↔ home 事务**穿过真实 mesh」,**不是**「协议语义在 mesh 环内被验证」⇒ ⛔ 任何对外表述 SHALL NOT 把本切片读成「CEP 协议已在真实 mesh 上验证」。
6. **网络封装字段的归属**:`dst_node` / `src_node` / `vc_id` / `hops` / `src_port` / `flit_type` / `flit_index` / `flit_count` 由**本模块**或 **router** 写(`src_port` / `hops` 由 router 在转发时写,`src/tlm/router_tlm.cc:310-311`)⇒ ⛔ 本模块 SHALL **NOT** 在 flit 上写 `hops` / `src_port`(路由器职责);`is_ok` / `error_code` 属 RESPONSE 语义 ⇒ 本模块反向恢复 Completion 时写**默认值**(OK / 0)· ⛔ **不**承载错误传播语义。

---

## D-6 · 待 architect 裁决项（**不在本 change 实施**）

| # | 项 | 3 条出路 | 影响 |
|:--:|---|---|---|
| **Q1** | 多流无损 | ① `NoCFlitBundle` 加 `cohif_flow_id` 字段(ADR-CEP-19 决策 3「扩展走 OpenSpec change」⇒ 有合法路径,但触 dGPU 侧)② 新增 CEP 专用网络 Bundle(⇒ RouterTLM 须支持多 Bundle 类型 ⇒ 侵入 Router)③ 接受有损,「多流混跑」明确排除在 CEP 可实验性之外 | **不影响本 change 的 AC** —— 垂直切片语义无歧义 |
| **Q2** | CEP 的 mesh 拓扑声明面 | ① `fabric.json` 承接节点/链路/路由(⇒ 改派生副本 ⇒ 触 ArchForge 镜像纪律)② 维持「拓扑只在 config」现状并登记边界句 ③ 另立声明源 | **不影响本 change 的 AC** —— 拓扑由 config 表达 |
| **Q3** | `NICTLM` 的静默失败 | ① 改 `NICTLM` 为显式失败(触 dGPU 侧)② 登记为已知缺陷 ③ 加闸 | **不影响本 change 的 AC** —— 本模块已显式失败 |

---

## D-7 · 风险与缓解

| 风险 | 缓解 |
|---|---|
| ⚠️ **【已升级 · critic SHOULD_FIX-1】连接绑定语义本身未经实测** —— 原风险写「端口号语义不符」,实测后升级为「**src/dst 角色与路由路径选择本身**未经实测」:① `resp_out` 与 `req_out` **两条不同的 egress 路径**(F-14/F-15/F-16)② 一条 connection 产生**两个** PortPair(F-13)③ X-first 路由的绕行(F-17)⇒ **且全仓无先例**(F-19) | **T1.3 升级为硬前置探针**:先写**不依赖本 change** 的探针(2 router + 2 NIC + `address_regions`,用既有 `NICTLM`)实测 ① 每种 connection 需要几条 ② flit 实际走哪条 egress ③ `hops` 是否递增 ④ 回程路径覆盖 ⇒ 据实定稿 D-2.1 ⇒ ⛔ **不得凭推断实现** |
| `TrafficGenTLM` 的地址不落在 `address_regions` ⇒ 请求被丢 | 配置里把 `TrafficGen` 的地址段与 `address_regions` **显式对齐**;J-1 断言 `stats_dropped_unmapped_ == 0` |
| RouterTLM 的 VC / credit 机制需要 flit 先握手 ⇒ 首包被丢 | J-2 用「跑够周期后再断言」而非「跑 1 周期就断言」;具体周期数**实测**确定并写进判据注释 |
| 「抵达」被直达骗过 | J-4 的 `hops ≥ 1` + `flits_forwarded > 0` 双重保险 |
| 变异「因错误的原因红」 | J-5 用结局分类枚举,断言**具体结局** |
