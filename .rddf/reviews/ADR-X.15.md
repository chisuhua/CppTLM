# 审稿报告 — ADR-X.15: SimModule 暴露端口的真实数据通路径接入

- **日期**: 2026-11-16
- **架构产物**: `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`（Status: Proposed）
- **审查者**: oracle（先行）→ critic（arch 档，第二审）
- **评审轮次**: 本 ADR 第 1 轮（oracle 1 + critic 1），无 BLOCKER ⇒ 无需复审

## 意图检查点（oracle 前置）

- **意图**: 让容器型 `SimModule`（CepMeshInterconnect）的 exposed 端口承载真实数据流；范围仅限 `module_factory.cc` 连接解析（新增 Step 7c）；纯 ChStream 路径零回归；仅单层 exposed。
- **判定**: 意图明确、无 ≥2 种合理解读 ⇒ 未触发用户 `question`；歧义/边界已由代码读实枚举充分，未单独调 metis（intent checkpoint 的 metis 为可选）。
- **直接进入 oracle**。

## oracle（判定 APPROVE）

- 事实 F-1~F-6 全部读码核验为真（Step 7b 实际块边界 `module_factory.cc:1093-1185`，已订正进 ADR F-4）。
- 目标场景接线对象级等价：Step 7c 取得的内部端口 = 内部工厂 Step 7 mirror 的**同一对象** ⇒ `(ch_req_out[nic][1], r00.req_in[4])` 与扁平 7b 等价；LOCAL=4 无内部占用，无 clobber。
- 3 项 must-fix（已全部并入 ADR）：
  1. **方向按内部端口后缀推导**（反例 `CpuCluster::cpu0_to_bus` 是 req_out 型 exposed output，label 会骗人）→ ADR 决策 2 方向表。
  2. **`isBound()` 硬校验** 防 PortPair 静默覆盖 → ADR 决策 2b。
  3. **失败语义硬错误**（含嵌套容器暴露一律报错）→ ADR 决策 6（四类）。
  - 附带：⑤ processed_connections 键空间不对称已登记（ADR 决策 9）。
- 拒绝的替代方案 F/G/H/B/C/D 均确认更差；7c 是 7b 之后的最小落点。

## critic（判定 APPROVE_WITH_CONDITIONS · mergeable: yes · scope_exceeded: false · 0 BLOCKER）

- 与既有契约无硬冲突：Step 7c 只调 SimModule 既有公开 API（F-3 证明 mirror 就是为此预留）；不触双注册表；ADR-X.10 无交集；ADR-X.9 属「未声明的豁口」非直接矛盾。
- 向后兼容实测：6 份 exposed config 顶层 connections 为空或不引用 exposed 标签；8 份 cep 点对点 config 无 exposed 声明；`cep_mesh_e2e.json:79-82` 是唯一目标场景，Step 6/7b 均不写入其去重集 ⇒ 7c 输入集无既有占用、双连面为空。
- 决策完整性：方向表四行自洽；exposed-exposed、嵌套容器显式排除，非伪决策。

### SHOULD_FIX 处置（均已并入，未触发返工复审）

- **SHOULD_FIX-1**（7c 是否套用 ADR-X.9 L1/L2/L3 检查未声明）→ ADR 决策 7：显式声明「7c 豁免 L1/L2/L3，理由=无 port_specs，代价=由方向表+isBound+硬错误兜底」。
- **SHOULD_FIX-2**（内部端口后缀非四类如 `sdma.host_out` 行为未定义）→ ADR 决策 6 增第 ④ 类硬错误。

### NITS 处置

- latency 透传 → ADR 决策 8。
- `include/core/AGENTS.md` 同步 7c 成员说明 → 交实施阶段（非 ADR 签批阻塞项）。

## 结论

oracle + critic 双审完成，0 BLOCKER，SHOULD_FIX 已并入。ADR 四要素齐（元数据 / 选项对比 ≥3 / 独立后果章 / Verification 前瞻判据）。Status = Proposed（实施待立项 `backlog: cpptlm-simmodule-exposed-port-wiring`）。

## 遗留（交实施阶段）

- 立项实施 change 时回填 AC 编号、把 V-3 改写为「建 pair 前两侧 isBound()==false 校验通过」。
- 同步订正 `openspec/changes/cpptlm-cep-mesh-interconnect-submodule/design.md` D-3 表 4-7 行（与落地 config 相反，config 正确）+ 其 D-5 边界改为「依赖本 ADR」。
- `include/core/AGENTS.md` 补 Step 7c。

---

## 第二轮审查（2026-11-16 · 实施后追加段）— oracle 判定 APPROVE

**改动形态**：描述性追加 + 状态推进（`git diff` 实测 13 insertions / 1 deletion），范围与声明一致 —— ① `## Status` 行 `Proposed` → `Accepted`；② `## Status Update` 末尾追加 4 条（真根因 / 范围声明 / V-5 口径收敛 / 已证伪假设登记）。未动 Context / Decision / 选项对比 / Consequences / Verification / 关联 任何正文。

**审查路径**：按判例「Accepted ADR 的描述性措辞修订 ⇒ 只 oracle」（不触发 critic 复审）。

**oracle 结论（4 问逐条）**：

| # | 问题 | 结论 |
|:--:|---|---|
| 1 | 与正文既有条目冲突？ | **无冲突**。Decision 6 管装配期硬错误，追加段描述运行时链路层缺陷，不同层/不同生命周期；与 V-4 零回归也不冲突（credit 泄漏是既有行为，扁平基线同样受影响） |
| 2 | 「声明 V-5 不可用但不改原文」成立？ | **成立**。V-5 章自述为前瞻判据且复选框全未勾选（非已决断言）；等价性判据**已决**（mesh 级观测，已在实施 change 落地）、credit 修复**明确未决**（backlog 指针）；ADR 不可变原则下追加段是唯一合规裁决机制 —— 它消除而非制造矛盾 |
| 3 | 属过程日志混入？ | **不属于**。追加段主体（根因机理 + V-5 裁决）是决策性内容；第 4 条（已证伪假设）仅 3 句、有明确防复发价值且标注「不成立」 |
| 4 | Status → Accepted 恰当？ | **恰当**。首段 Status Update 已登记同日的「Proposed → Accepted」，头部对齐是标准形态；保持 Proposed 才是原矛盾 |

**行为 A/B 复核（oracle 逐条读码，全部属实）**：`fill_credit` 写 `CATEGORY_CREDIT` 且不写 `dst_node`/`hops`（`:132-136`，`credit_bundle{}` 值初始化）；经 `resp_out_port_` 同一通道（`:319`/`:323`）；`reverse_port(LOCAL)==LOCAL`（`router_tlm.hh:229`）；三处 `return_credit_to_upstream`（`router_tlm.cc:323/359/383`）；`handle_net_ingress` 无条件计数 + 覆写 `last_recv_*` + 投影全零 Completion（`cohif_nic.hh:260-271`）；证伪依据 `pack_flit` 一律 `FLIT_HEAD_TAIL`（`cohif_nic.hh:211`）+ `is_head()` 对 `FLIT_HEAD_TAIL` 返 true（`noc_bundles_tlm.hh:67-69`）+ `router_tlm.cc:177`（HEAD 走 `computeRoute`）vs `:190-199`（BODY/TAIL 查表分支）。

**已排除的替代方案**：改 V-5 原文（违反 ADR 不可变）/ 只留 improvement 指针（backlog 存活期短，读者无从裁决）/ Status 保持 Proposed（即原矛盾）/ 改 Deprecated-Superseded（决策经验证，无被取代迹象）。

**非阻断残留（交后续）**：
1. 旧 Status 行携带的评审出处（oracle APPROVE + critic APPROVE_WITH_CONDITIONS）未保留 —— 实施已完成、条件已落实，长留 interim 评审态是合理默认。
2. `cpptlm-router-credit-flit-leak-to-consumer` 立项并落地后，建议在本 ADR 再追加一条 Status Update 交叉引用（届时 V-5 字面判据的适用性可再评估），保持记录链闭合。
3. oracle 受限未能独立复核 `test/test_simmodule_exposed_port_wiring.cc` 的 mesh 级判据落地（实施验收已由 verifier 独立执行，属交叉验证缺口，不阻断）。

**判定行**：`判定: APPROVE`
