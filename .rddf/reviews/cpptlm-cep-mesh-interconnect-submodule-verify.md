# 独立验收报告 — `cpptlm-cep-mesh-interconnect-submodule`

- **日期**: 2026-11-16
- **验收者**: verifier（独立于实施者）
- **对象**: 已归档 change `openspec/changes/archive/2026-11-16-cpptlm-cep-mesh-interconnect-submodule/`
- **依赖**: `cpptlm-simmodule-exposed-port-wiring`（Step 7c，已归档 · 验收见 `.rddf/reviews/cpptlm-simmodule-exposed-port-wiring-verify.md`）
- **已用轮次**: 1 / 3
- **分流**: `rddf rdd-verify --dry-run` → `No eligible changes (empty queue)` ⇒ **本地验收路径**，未走 `rdd-verifier` skill

## 判据来源

1. `archive/…/cpptlm-cep-mesh-interconnect-submodule/proposal.md` 的 `## Acceptance`（AC-01 ~ AC-07）——**主判据**
2. `archive/…/design.md` 的 `## D-5 · 判据设计`（J-1 ~ J-6）
3. 用户指令：「最后切 verifier 验收」

## 逐条判定 + 证据

| AC | 判定 | 证据（命令 + 结果） |
|:--:|---|---|
| **AC-01** 容器可注册可装配（内部 4 router 在位） | **PASS** | `include/modules_cluster.hh:44` 显式短名注册 `ModuleFactory::registerModule<tlm::cep::CepMeshInterconnect>("CepMeshInterconnect")`；`MESH-1` 断言 6 顶层实例 + 顶层**不再**有 `r00..r11`；`MESH-2` 断言 `getInternalFactory().getAllInstances().size() == 4`（`test/test_cep_mesh_e2e.cc:249`）；蓝图 `configs/interconnect/cep_mesh_2x2.json` 实测 `4` 个 `RouterTLM` / `8` 条 connection |
| **AC-02** 层次命名空间成立（8 个标签映射） | **PASS** | `MESH-2` 逐条断言 8 个暴露标签的 `isExposedPort` 与 `findInternalPath`（`node0_req_in→r00.req_in[4]`、`node0_resp_out→r00.resp_out[4]`、`node3_*→r11.*` 等）；用例通过 |
| **AC-03** 端口暴露可用（外部 connection 生效） | **PASS**（文本标签名 stale，见 F-1） | `configs/cep_mesh_e2e.json` 实测 8 个 `"fabric.node[03]_*"` 端点各 1 次；`MESH-2` 断言全部可解析；`MESH-3` 端到端跑通（流量穿越 ⇒ 端口真实生效） |
| **AC-04** 行为等价（mesh 级） | **PASS** | `MESH-3`（`:275-300`）**同一次运行内**装配「内嵌平铺基线」（`FLAT_BASELINE` 字面量）并逐项比对：`hier.routers[i].flits/packets/hops == flat.routers[i].*`（`:281-283`）+ `hier.nic_b_recv == flat.nic_b_recv`；实测两侧 r00 15/15/29 · r01 8/8/16 · r10 7/7/14 · r11 15/15/31 **逐项相等** |
| **AC-05** 蓝图可复用（模块数/连接数与蓝图逐项相等） | **PASS（模块数）** / 见 F-2 | 模块数 ✅ `internal_router_count == 4`（`:297`）与蓝图 4 个 `RouterTLM` 相等；**连接数未单独断言**（蓝图 8 条 connection → 内部 PortPair 数未比对） |
| **AC-06** 零回归（`CohIfNICTLM` / 点对点 config 零 diff） | **PASS** | `git diff --stat e2dc3fca..HEAD -- include/tlm/cep/cohif_nic.hh` → **空**；9 份点对点 config（`cep_chi_rnf_e2e_{a,b}` + 6 份 `*_pe_minimal` + `cep_ualink_pe_minimal`）`git diff --stat` → **空**；`ctest -R cep` **100/100**；`[cep]` 12382/535 · `[e2e]` 7405/146 |
| **AC-07** 换蓝图可复用（不改容器类） | **PASS** | `MESH-6 (AC-07)`（`test/test_cep_mesh_e2e.cc` 末尾）：把 `params.template` 换成 `configs/interconnect/cep_mesh_1x2.json` ⇒ 装配成功、内部实例数 **2**、`r11` 不存在、按 1×2 标签可接线并 `isBound()`；`All tests passed (8 assertions)` |

## 失败项与路由建议

| # | 分类 | 内容 | 路由 |
|:--:|---|---|---|
| **F-1** | `proposal_drift`（轻） | **AC-03 文本写 `fabric.portN_*`**，实现为 `node0_*` / `node3_*`（8 个，4 通道 × 2 节点）⇒ AC 文本 stale（实现正确） | planner（订正 AC-03 文本） |
| **F-2** | `proposal_drift`（轻） | **AC-05 文本「模块数 / 连接数与蓝图逐项相等」**，实际只断言**模块数**；连接数无判据 | planner（AC-05 收敛为「模块数 + 逐条 PortPair 等价（由比对平铺基线间接覆盖）」，或补连接数断言） |
| **F-3** | `backlog`（记录）**—— 2026-11-16 根因订正** | 原判「`RouterTLM::routing_table_` 以 `transaction_id` 为键、不含方向」**已证伪**：`CohIfNICTLM::pack_flit` 一律写 `FLIT_HEAD_TAIL`，而 `is_head()` 对 `HEAD_TAIL` 返回 true ⇒ `stage_route_computation` **恒走 HEAD 分支**（`computeRoute`），BODY/TAIL 的查表分支**不可达**。**真根因 = in-band credit flit 泄漏给非 router 消费者**（`fill_credit` → 同一 `resp_out` 通道 → `reverse_port(LOCAL)==LOCAL` → `handle_net_ingress` 不辨 credit ⇒ 计数虚增 / `last_recv_*` 被覆写成 0/0 / 全零伪 Completion）。**已由 change `cpptlm-router-credit-flit-leak-to-consumer` 修复并归档 + 独立验收 PASS**（见 `.rddf/reviews/cpptlm-router-credit-flit-leak-to-consumer-verify.md`；根因订正过程见台账 Entry #453） |

> 本轮**无 implementation_gap**：所有差异均为「AC 文本/判据粒度」层面，实现本身通过全部功能性判据。

## 遗留清扫

| 项 | 结果 |
|---|---|
| TODO/FIXME/XXX（本次触及文件） | 无 |
| 临时调试残留 | 无 |
| 被 skip/disabled 的测试 | 无 |
| 未跟踪的仓库残渣 | 无 |
| 注释掉的代码块 | 0 |

## 结论

**PASS**（AC-01 ~ AC-04、AC-06、AC-07 完全通过；AC-05 模块数通过、连接数判据粒度偏窄）。**无 implementation_gap**；1 处 `backlog`（RouterTLM 路由表键缺陷）建议立项。

**证据边界**：AC-04 的「逐项相等」由测试**同一次运行内**比对平铺基线得出（不依赖硬编码数字），故对无关改动免疫；未独立重测 300 周期以外的时长。
