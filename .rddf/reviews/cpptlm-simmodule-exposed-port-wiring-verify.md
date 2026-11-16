# 独立验收报告 — `cpptlm-simmodule-exposed-port-wiring`

- **日期**: 2026-11-16
- **验收者**: verifier（独立于实施者）
- **对象**: 已归档 change `openspec/changes/archive/2026-11-16-cpptlm-simmodule-exposed-port-wiring/`
- **决策源**: `docs/adr/ADR-X.15-simmodule-exposed-port-data-path.md`（Accepted）
- **已用轮次**: 1 / 3
- **分流**: `rddf rdd-verify --dry-run` → `No eligible changes (empty queue)`（0 active）⇒ **本地验收路径**（1–5 步），未走 `rdd-verifier` skill

## 判据来源

1. `openspec/changes/archive/2026-11-16-cpptlm-simmodule-exposed-port-wiring/proposal.md` 的 `## Acceptance`（AC-01 ~ AC-08）——**主判据**
2. ADR-X.15 的 `## Verification`（V-1 ~ V-8）
3. 用户本次指令：「最后切 verifier 验收」

## 验证顺序（早失败早停）

1. 编译/静态检查 ✅ `cmake --build build` 无 error（仅既有 `-Wunused-parameter` 警告）
2. 单元测试 ✅（见下逐条）
3. 集成 ✅ `[cep]` / `[e2e]` / `[mesh]` / `[simmodule]` / `ctest -R cep`
4. 边界与错误路径 ✅（EXP-3 ~ EXP-9 全为负例）
5. 遗留清扫 ✅（见末节）

## 逐条判定 + 证据

| AC | 判定 | 证据（命令 + 结果） |
|:--:|---|---|
| **AC-01** exposed 连接建成真实 PortPair、两侧 `isBound()` | **PASS** | `./build/bin/cpptlm_tests "[exposed]"` → `All tests passed (33 assertions in 10 test cases)`；其中 `EXP-1 (AC-01)`（`test/test_simmodule_exposed_port_wiring.cc:118`）断言 `internal_in->isBound()` / `internal_out->isBound()` / 对端 `req_out[1]`、`req_in[1]` 均 `isBound()` |
| **AC-02** 方向按内部端口后缀推导（4 组） | **PASS** | `EXP-2 (AC-02)`（`:160`）：4 条 connection 覆盖 `req_in↔req_out` / `resp_in↔resp_out` / `req_out↔req_in` / `resp_out↔resp_in`，断言 peer 4 通道 + 容器 4 通道全部 `isBound()`；用例通过 |
| **AC-03** 同端二次连接 ⇒ 装配期报错 | **PASS** | `EXP-3 (AC-03)`（`:198`）`REQUIRE_FALSE(instantiateAll(cfg))`；运行输出含 `[ModuleFactory] 暴露端口连接 'peer.2 -> box.p_req_in' 的端口已被绑定，拒绝静默覆盖既有 PortPair` |
| **AC-04** 失败硬错误（提案列 4 类，实现实为 5 类） | **PASS**（实现**多于**提案） | `EXP-4`（未声明标签）→ `引用了未声明的暴露标签`；`EXP-5`（内部端口不可解析）→ `内部端口 'rX.req_out[4]' 不可解析`；`EXP-6`（嵌套容器）→ `'inner.r0.req_out[4]' 不可解析（嵌套容器暴露当前不支持）`；`EXP-7`（索引越界）→ `对端索引 99 越界`；`EXP-8`（后缀非四类）→ `custom_out 不属于 req_in/resp_in/req_out/resp_out 四类`；`EXP-9`（两端都 exposed）→ `连接两端都是暴露端口…仅支持「一端 exposed + 一端顶端 ChStream 端点」`。全部 `REQUIRE_FALSE` 且报错文案精确命中 |
| **AC-05** 纯 ChStream 零回归 | **PASS** | `[cep]` `12382 assertions in 535 test cases` · `[e2e]` `7405/146` · `[mesh]` `205/15` · `[simmodule]` `184/44` · `ctest -R cep` → **100% tests passed, 0 failed out of 100** |
| **AC-06** Step 6 wildcard 分支零改动 | **PASS** | `git show 0fa0d6c5 -- src/core/module_factory.cc \| grep -E "^[+-]" \| grep -E "isRegexPattern\|Wildcard::match\|src_names\|dst_names\|ModuleGroup::isGroupReference"` → **空输出** |
| **AC-07** 层次化端到端可达（`nic_b_recv_dst_node==3`、`r*.flits>0`） | **PARTIAL**（见 F-1） | `r*.flits>0` ✅ 有判据：`MESH-3` 断言 `hier.routers[0].flits > 0 && hier.routers[3].flits > 0 && hops > flits`。但 `nic_b_recv_dst_node==3` **无任何断言**：`grep -n "last_recv_dst_node" test/test_cep_mesh_e2e.cc test/test_simmodule_exposed_port_wiring.cc` → **空** |
| **AC-08** 未声明 exposed 的 SimModule 自身端口引用零变化 | **PASS**（含 1 处覆盖缺口，见 F-3） | `EXP-10 (AC-08)`（`:318`）断言未声明 exposed 的容器上 `box.r0.req_in[4]` 引用**不报错**且**不被接通**（`REQUIRE_FALSE(isBound())`）；Phase 8 全链路 E2E 绿（`[pcie]` `36779/452`） |

## 失败项与路由建议

| # | 分类 | 内容 | 路由 |
|:--:|---|---|---|
| **F-1** | `proposal_drift` | **AC-07 字面判据 `nic_b_recv_dst_node==3` 无覆盖且实测不成立**。根因（已记录于 `archive/…/design.md` D-7 + ADR-X.15 Status Update）：层次化把容器子模块 tick 收成连续一组 ⇒ NIC 侧**顺序敏感计数器**（`last_recv_dst_node/_hops`）会变；且 `RouterTLM::routing_table_` 以 `transaction_id` 为键不含方向（**既有缺陷**）导致回声，平铺基线同样存在。⇒ AC 应订正为 **mesh 级**判据（与 mesh change 的 AC-04 对齐：router flits/packets/hops 逐项相等 + `nic_b_recv>0`） | **planner**（改归档 proposal 的 AC-07 文本） |
| **F-2** | `proposal_drift`（轻） | **AC-04 文本写「四类」失败，实现实为五类**（多「两端都 exposed」）。实现**更严**，非缺陷 | planner（AC-04 文本补第 ⑤ 类） |
| **F-3** | 覆盖缺口（轻） | AC-08 括号里的 `configs/library_cpu_demo.json` 装配**未落持久判据**（实施期用临时探针验证过，未留测试） | 可留 backlog；不阻断（AC-08 主判据 Phase 8 E2E 有覆盖） |

## 遗留清扫（verifier 复核实施者的自查）

| 项 | 结果 |
|---|---|
| TODO/FIXME/XXX（本次触及文件） | 无 |
| 临时调试残留（`MF_EXP_DEBUG` / `MF_PAIR_DUMP` / `ZZ*` / `test_zz_*`） | 无 |
| 被 skip/disabled 的测试 | 无（无 `*.disabled`，无 `.skip`/`SKIP(`） |
| 未跟踪的仓库残渣 | 无（`git status --porcelain` 干净） |
| 注释掉的代码块 | 0 |
| 未替换的 mock | 无（无 mock 引入） |

## 结论

**功能面 PASS**（AC-01~AC-06、AC-08 全部通过，AC-07 的 `r*.flits>0` 子项通过）；**无 implementation_gap**。存在 **2 处 `proposal_drift`（AC 文本 stale）+ 1 处轻覆盖缺口**，均**不阻断**已验证的能力，交由 planner 订正 AC 文本。

**证据边界**：本报告未独立重测「`library_cpu_demo.json` 装配」（AC-08 的次级项，无持久判据）；除此外所有判定均有工具证据。
