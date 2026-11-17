# 独立验收报告 — `cpptlm-router-local-port-credit-exhaustion`

- **日期**: 2026-11-17
- **验收者**: verifier（独立于实施者）
- **对象**: change `openspec/changes/cpptlm-router-local-port-credit-exhaustion/`（active，tasks **28/34**）
- **决策源**: `docs/adr/ADR-X.16-router-local-port-terminal-credit.md`（Accepted · 方案 A）
- **已用轮次**: 1 / 3
- **分流**: `rddf rdd-verify --dry-run` → `No eligible changes to verify (empty queue)`；且 `.rddf/state/iteration.json` 不存在 ⇒ **P3 archive gate 未触发** ⇒ 未走 `rdd-verifier` skill，走**本地验收路径**（1–5 步）

## 判据来源（按优先级）

1. 用户本次指令：「继续」⇒ 执行 tasks **T5.4** 独立验收（未给具体判据）
2. `openspec/changes/cpptlm-router-local-port-credit-exhaustion/proposal.md` 的 `## Acceptance`（**AC-01 ~ AC-07** + guard G-1/G-2）—— **主判据**
3. 本次变更自身声明：commit `68a2bf01`（实现）+ 台账 Entry **#463**（实施记录）
4. `ADR-X.16` 的 `## Verification`（**V-1 ~ V-6**，经 AC 映射对应）

## 验证顺序（早失败早停）

1. 编译/静态检查 ✅ `cmake --build build` 无 error
2. 单元测试 ✅（新用例 + 具名回归集）
3. 集成/端到端 ✅（`[mesh]` / `[cep]` / `[e2e]` / `[pcie]` / `[exposed]`）
4. 边界与错误路径 ✅（AC-01/02/03 的边界判据 + 非法 VC 不被短路放行）
5. 遗留清扫 ✅（见末节）

## 逐条判定 + 证据

| AC | 判定 | 证据（命令 + 结果） |
|:--:|---|---|
| **AC-01** LOCAL 单 VC 40 flit 全到（无 8 平台期） | **PASS** | `RLC-1 (AC-01)`（`test/test_router_local_port_credit.cc`）：裸 `RouterTLM` 自环注入 40 flit（全局 1 flit/cycle，循环上限 400 cycles ≥ design D-2 要求的 120）⇒ `REQUIRE(drained == 40)`；旧行为下第 9 个 flit 起永停（credit 不归还）⇒ 本用例即判别力载体。`[router][credit][local]` → `All tests passed (12 assertions in 4 test cases)` |
| **AC-02** 4 VC 80 flit 全到 | **PASS** | `RLC-2 (AC-02)`：`vc = injected % NUM_VCS` 手写 `vc_id` 0..3（`out_vc` 由 `allocate_vc` 决定），累计 80 flit ⇒ `REQUIRE(drained == 80)`（全部到达 ⇒ 无 flit 滞留） |
| **AC-03** stall 可观测 + 反向判据 + `credit_timeout_` 默认 0 | **PASS**（含 1 处判据粒度差异，见 F-1） | `RLC-3 (AC-03)`：裸 router（`adapter_ == nullptr` ⇒ credit 永不归还）向 **mesh 向**（WEST→EAST，非 LOCAL）注入 ⇒ `REQUIRE(stall_cycles >= 16)`；**反向判据** —— 每周期归还 1 个 credit 后 `REQUIRE(stall_cycles(router) == before)`（**严格相等**，非「增速下降」）⇒ 对「每周期无脑 +1」的假实现有真实判别力。`credit_timeout_` 默认值：`include/tlm/router_tlm.hh:316` `uint64_t credit_timeout_ = 0;  // 默认关闭 (>0 时启用)` ✅ 未被改动 |
| **AC-04a** 具名回归集全绿 + mesh router 计数逐项不变 | **PASS** | 回归集：`[mesh]` `251 assertions in 19 test cases` · `[cep]` `12429/539` · `[e2e]` `7451/150` · `[pcie]` `36779/452` · `[exposed]` `33/10` —— 全绿。**「逐项不变」的 diff 级直接证据**：`git diff 68a2bf01^ 68a2bf01 -- src/tlm/router_tlm.cc` 的 hunk 仅 4 处 —— `:49`（构造函数加标量）/ `:226`（VA 拒绝分支 +1 行）/ `:266`（SA 拒绝分支 +1 行）/ `:452`（`has_credit`+`consume_credit`+`note_credit_stall`）⇒ **转发路径函数（`stage_route_computation` / `stage_switch_traversal` / `stage_link_traversal`）与 `stats_flits_forwarded_` / `pending_link_.push` / hops 累加全部零改动** ⇒ router 的 flits/packets/hops 观测值必然不变 |
| **AC-04b** 数值变化须逐条重推导 | **PASS（空集）** | 实测**零既有断言变化**：改动前（Entry #463 记录）与改动后（本次实测）`[mesh]` 同为 `251/19`、其余标签同值；**未触碰任何既有 `REQUIRE` 常量**（`git diff` 显示既有测试文件零改动）。结论已回写 `design.md` §D-4「实测结论」段 |
| **AC-05** 死状态显式处置（可 grep） | **PASS** | `include/tlm/router_tlm.hh:286` 起：`⚠️ downstream_credits_[LOCAL][*] 在 ADR-X.16 **D1** 之后为**死状态**：… 保留数组形状（不把 LOCAL 拆出）以免改动既有索引与 credit_safety_reset 的遍历面` ⇒ 处置 = **方案 (a) 注释说明**，grep 可验 |
| **AC-06** 真源同步 | **PASS（delta 已备，合并动作属 T3.3）** | 2 份 delta spec 齐备且内容正确：① `specs/router-local-port-flow-control/spec.md` 新建 2 条 Requirement（`router-local-port-terminal-semantics` 含「`out_port == LOCAL` 时 SHALL 恒判定为有 credit」+「对 LOCAL SHALL NOT 扣减 `downstream_credits_[LOCAL][*]`」+「`port-type-system` 的 `LOCAL` `role = BI_DIRECTIONAL` SHALL 保持不变」；`router-mesh-credit-stall-observability` 含锚点口径「**不是** `waiting_for_credit` 状态位——后者恒 false」）；② `specs/cep-cohif-network-adapter/spec.md` 的「已知限制」句已改为「已由 capability `router-local-port-flow-control` 承接并解决」+ 保留「本模块**仍不**归还 credit ⇒ ingress 仍丢弃 credit flit 并计入 `credits_dropped()`」。**合并由 `openspec archive` 执行（T3.3）** |
| **AC-07** LOCAL 侧不误报（stall 保持 0） | **PASS** | `RLC-1` 与 `RLC-2` 均 `REQUIRE(stall_cycles(router) == 0)`（LOCAL 注入场景全程零停滞）⇒ D1 后 `has_credit(LOCAL,·)` 恒真 ⇒ 两个 `!has_credit` 拒绝分支对 LOCAL **永不成立**（结构上不可能误触发） |
| **G-1** `LOCAL↔router` 结构性测试仍绿 | **PASS** | `[exposed]` → `All tests passed (33 assertions in 10 test cases)`（含 EXP-1/EXP-2）；`test/test_cep_mesh_e2e.cc` 的 `MESH-6` 仍在（grep 命中 1） |
| **G-2** `RouterPort::LOCAL` 注释已声明终端语义 | **PASS** | `include/tlm/router_tlm.hh:30-35`：注释明写「终端端口…⚠️ 不参与 downstream credit 流控（per ADR-X.16 D1）：`has_credit(LOCAL, vc)` 恒真、`consume_credit(LOCAL, vc)` 不扣减 —— 反压由 ChStream 的 `valid/ready` 承担」+ 误用失败模式（下游 `input_buffer_` 静默丢 flit） |

## 失败项与路由建议

| # | 分类 | 内容 | 路由 |
|:--:|---|---|---|
| **F-1** | `proposal_drift`（轻） | **AC-03 的反向判据**原文写「挂上 `BidirectionalPortAdapter` 后**停止增长**」，实现（`RLC-3` 阶段 B）用的是「每周期直接调 `router.receive_credit(kEast, 0)` 归还 credit」。功能等价（`BidirectionalPortAdapter` 归还 credit 的唯一入口即 `router.receive_credit()`），且 `credit_timeout_` 默认 0 ⇒ `credit_safety_reset` 不会掩盖 ⇒ 但**未实际装配 adapter** 验证该路径 | planner（收敛 AC-03 文本为「credit 归还后停止增长」）或 builder（补一个挂 adapter 的对照用例）。**不阻断** |
| **F-2** | `proposal_drift`（轻） | AC-06 的**执行动作**（delta 合并进 main spec）在 T3.3 归档时完成，当前仅「delta 已备」 | 随归档闭环 |

> 本轮**无 implementation_gap**：全部差异均为「判据文本粒度 / 归档动作归属」层面，功能实现本身通过全部功能性判据。

## 遗留清扫（verifier 复核实施者的自查）

| 项 | 结果 |
|---|---|
| TODO/FIXME/XXX（本次触及文件：`src/tlm/router_tlm.cc` / `include/tlm/router_tlm.hh` / `test/test_router_local_port_credit.cc`） | 无 |
| 临时调试残留（`ZZ*` / `MF_*` / `test_zz_*` / `printf` / `std::cout`；`DPRINTF` 属正常诊断日志） | 无 |
| 被 skip / disabled 的测试 | 无（无 `.disabled`，无 `.skip` / `SKIP(` / `DISABLED`） |
| 未跟踪的仓库残渣 | 无（`git status --porcelain` 无 `??`） |
| 注释掉的代码块 | 0（diff 中 `^\+\s*//.*[;{]` 计数 = 0） |
| 未替换的 mock | 0（新增测试未引入 mock） |

## 结论

**PASS** —— AC-01 ~ AC-07 全部通过（G-1/G-2 通过），**无 implementation_gap**。功能面证据链闭合：实现 diff 零触碰转发路径 ⇒ 既有观测值零变化；新用例覆盖 LOCAL 无平台期、4 VC 并发、mesh 向停滞可观测与反向判据、LOCAL 不误报四类边界。

**2 项非阻断 drift**（F-1 判据粒度 / F-2 归档动作）交由后续闭环，不阻断归档。

**已用轮次**: 1 / 3（`RDDF_VERIFIER_MAX_LOOPS=3` 对齐）

**证据边界**：V-4 的「逐项不变」以 **diff 级零触碰转发路径** 直接证明（而非物化基线工作树重跑）—— 依据 §verifier 产出契约「不得 stash / checkout / 新建 worktree」；既有断言全绿 + 断言数不变构成交叉验证。
