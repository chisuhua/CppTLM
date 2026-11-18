# 验收报告 —— cpptlm-nictlm-credit-flit-filter

> **判据来源**：`openspec/changes/cpptlm-nictlm-credit-flit-filter/proposal.md` 的 `## Acceptance` 段（AC-01 ~ AC-06 + 附加 guard G-1）
> **执行面**：本地验收（`cpptlm-module-param-migrate-batch2` 之外 · **未走 `rdd-verifier` skill** —— 分流条件 ② 不满足：`.rddf/state/iteration.json` **不存在** ⇒ rdd-builder P3 archive gate 未触发 ⇒ 走「verifier 产出契约」的 1~5 步本地流程）
> **已用轮次**：**1 / 3**（`RDDF_VERIFIER_MAX_LOOPS=3`;本轮即通过 ⇒ 无需打回 builder）
> **验收人**：build agent（角色内建 verifier 职责;**只验不改** —— 本轮零实现改动）
> **日期**：2026-11-18

---

## 0. 结论

**PASS** —— AC-01 ~ AC-06 **全部通过**（6/6），附加 guard G-1 通过，遗留清扫零命中。
⇒ 允许进入归档（`openspec archive`，⛔ 不用 `--skip-specs` —— 实现已落地）。

---

## 1. 逐条判定 + 工具证据

### AC-01 —— 注入 credit ⇒ 三面零变化（带判别力的序列）

| 项 | 判定 |
|---|---|
| 结论 | ✅ **通过** |

**证据**：`test/test_nictlm_credit_filter.cc:88` 的 `NIC-CREDIT-1: 注入 credit flit ⇒ 计数 / 重组 / 业务流三面零变化`（tag `[nictlm][credit][filter]`）。

```
$ ./build/bin/cpptlm_tests "[nictlm]"
All tests passed (40 assertions in 5 test cases)
```

**判别力核对**（AC 明文要求的「⛔ 不得写成空断言」）：该用例先注入**真响应 flit**（`FLIT_HEAD_TAIL` · `CATEGORY_RESPONSE` · `tid=T`）⇒ 断言 `flits_received==1` · `packets_received==1` · `pe_resp_out()` 有效且 `transaction_id==T` ⇒ `clear_valid()` ⇒ **再**注入同 `tid=T` 的 credit flit ⇒ 断言三面**保持** 1/1/无输出且 `credits_dropped()==1`。⇒ 基线**非零**，断言**有判别力**（不是「注入前后都是 0」的空断言形态）。

### AC-02 —— 端到端零 credit 污染（存在性 + 计数正确）

| 项 | 判定 |
|---|---|
| 结论 | ✅ **通过** |

**证据**：`test/test_nictlm_credit_filter.cc:252` 的 `NIC-CREDIT-4: 端到端 —— credit 抵达 NIC 且被丢弃，计数零污染`（tag `[nictlm][credit][filter][e2e]`）。

- ① `credits_dropped() > 0` —— 运行期实测输出 `[NIC-CREDIT] ni0.sent=4 ni0.flits_received=0 ni0.credits_dropped=4 ni1.flits_received=4` ⇒ **credit 确实抵达**该 NIC ⇒ 规避了 `0==0` 空断言;
- ② `flits_received` == 测试自建拓扑注入的响应 flit 数（`ni1.flits_received=4` 与 `ni0.sent=4` 一致）;
- ③ 拓扑为**测试自建**的最小「NIC → Router → NIC」带流量拓扑 ⇒ **未**使用 `mesh_2x2_tlm` / `mesh_4x4_tlm`（符合 AC 明文禁令）。

### AC-03 —— 既有 `NICTLM` 拓扑回归全绿

| 项 | 判定 |
|---|---|
| 结论 | ✅ **通过** |

**证据 ①（触达标签全量）**：

```
$ ./build/bin/cpptlm_tests "[e2e],[nic],[router],[noc]"
All tests passed (798397 assertions in 243 test cases)
```

**证据 ②（ctest 全量）**：

```
$ ctest --test-dir build -j4
100% tests passed, 0 tests failed out of 181
```

**证据 ③（点名文件均在集合内）**：`test_complex_topologies_from_config.cc` · `test_e2e_simulation.cc` · `test_nic_side_rules` · `test_noc_mesh_binding_semantics.cc` 全部落在 `[e2e]/[nic]/[router]/[noc]` 标签面内 ⇒ 已由证据 ① 覆盖。
⚠️ `[pcie]` 按 AC 明文**不列入**（与 `NICTLM` 零交集）。

### AC-04 —— 判据形态与 `CohIfNICTLM` 侧一致（可对拍）

| 项 | 判定 |
|---|---|
| 结论 | ✅ **通过** |

**证据**：`test/test_nictlm_credit_filter.cc:294` 的 `NIC-CREDIT-5: 与 CohIfNICTLM 侧同款判据可对拍`（tag `[nictlm][credit][filter][cohif]`）。

- 两侧**同一 credit flit** 注入 ⇒ 丢弃行为一致（都不计数 / 都不产出业务流 / 丢弃数 == 1）;
- ⛔ **逐字复制判据**（未抽共享代码）—— 符合 AC 明文要求与 design 的「明确不做」第 7/8 行。

### AC-05 —— 零 diff 面（可复现判据）

| 项 | 判定 |
|---|---|
| 结论 | ✅ **通过**（**2 项超出，已如实登记** —— 见下） |

**证据（可复现命令）**：

```
$ git show --name-only --format="" 9e3b945f
.rddf/roadmap/objectives/obj-cep-platform.md
include/tlm/nic_tlm.hh
openspec/changes/cpptlm-nictlm-credit-flit-filter/design.md
openspec/changes/cpptlm-nictlm-credit-flit-filter/tasks.md
src/tlm/nic_tlm.cc
test/CMakeLists.txt
test/test_nictlm_credit_filter.cc
```

- **行为面零 diff ✅** —— AC 点名的 7 个零 diff 目标（`src/tlm/router_tlm.cc` · `include/tlm/router_tlm.hh` · `include/framework/**` · `configs/**` · `include/abi/**` · `include/bundles/**` · `include/tlm/cep/cohif_nic.hh`）**全部不在**本 change 的改动集合内;
- **改动面 ⊆ 允许集 + 2 项载体 ✅（超项登记）** —— `test/CMakeLists.txt`（+6 行 = 新测试的 ctest 注册载体）与 `.rddf/roadmap/objectives/obj-cep-platform.md`（台账 Entry #468）**不在 AC 字面允许集内** ⇒ 二者均**非行为面**（前者是测试注册,后者是治理台账）⇒ 判**通过**,但**显式登记**以免下次复核误判;
- ⚠️ **基线口径**：本仓有**并发 agent** ⇒ 零 diff 面按「**本 change 的改动 commit 集合**」判定（`9e3b945f`），**不按** `base..HEAD` 整段区间 —— 区间内混有并发 session 的 `cpptlm-router-buffer-size-param` 改动（`include/tlm/router_tlm.hh` / `src/tlm/router_tlm.cc` 等），与本 change 无关（与 tasks T2.6 的原记录口径一致）。

### AC-06 —— 同 `tid` 在途包不被劫持

| 项 | 判定 |
|---|---|
| 结论 | ✅ **通过** |

**证据**：`test/test_nictlm_credit_filter.cc:158` 的 `NIC-CREDIT-3: 同 tid 的 credit flit 不劫持在途包状态`（tag `[nictlm][credit][filter]`）。

- 注入真 `FLIT_HEAD`（`tid=T`，多 flit 包）→ 注入 credit（`tid=T`）⇒ 断言 `pe_resp_out()` **无输出**且在途包状态未被 credit 改写 → 再注入同 `tid=T` 的 `FLIT_TAIL` ⇒ 断言**恰好一条**响应 ⇒ 三段式判据齐备。

### 附加 guard G-1 —— `credits_dropped()` 两侧观测语义一致

| 项 | 判定 |
|---|---|
| 结论 | ✅ **通过** |

**证据**：`NIC-CREDIT-5`（AC-04 同址）内含两侧同名同义观测面的对拍断言（同名 `credits_dropped` + 同判据形态）。

---

## 2. 边界与错误路径（verifier 顺序第 4 步）

| 边界 | 判据 | 证据 |
|---|---|---|
| credit flit 的**两种 `flit_type` 形状** | `FLIT_HEAD_TAIL`（真实 credit 形状）+ 默认 `FLIT_HEAD` ⇒ **都必须**被丢弃 | `test/test_nictlm_credit_filter.cc:124` `NIC-CREDIT-2` |
| credit flit 与**同 `tid` 的在途包**共存 | 不得劫持（AC-06） | `NIC-CREDIT-3` |
| credit flit 与**同 `tid` 的真响应 flit**共存 | 真响应不得被误丢（AC-01 带判别力序列） | `NIC-CREDIT-1` |

---

## 3. 遗留清扫（verifier 顺序第 5 步）

| 检查项 | 结果 | 证据 |
|---|---|---|
| TODO / FIXME / XXX / 临时诊断标记 | **零命中** | `grep -rn "TODO\|FIXME\|TMPDIAG\|XXX" src/tlm/nic_tlm.cc include/tlm/nic_tlm.hh test/test_nictlm_credit_filter.cc` ⇒ 空 |
| 被 skip 的测试 | **零命中** | `grep -rn "CATCH_SKIP\|\.skip" test/test_nictlm_credit_filter.cc` ⇒ 空 |
| 未替换的 mock / 临时 fallback | **零命中** | 同上三文件 grep ⇒ 空 |
| 未跟踪残渣 | **零命中** | 归档前工作树洁净（`git status --short` 仅本 change 自身文件） |

---

## 4. 失败项与路由建议

**无失败项** ⇒ 无需路由回 build / planner。

---

## 5. 归档前置条件核对

| 前置 | 状态 |
|---|---|
| AC 全部通过 | ✅ AC-01 ~ AC-06 + G-1（7/7） |
| 全量回归绿 | ✅ `cpptlm_tests` 全量 + `ctest` 181/181 |
| delta spec 已写 | ✅ `openspec/changes/cpptlm-nictlm-credit-flit-filter/specs/cep-nictlm-credit-filter/spec.md`（1 Requirement / 3 Scenario）—— tasks **T3.1** 的勾选**滞后**，实际已完成 |
| `port-type-system` spec 未改 | ✅ 待归档时复核（T3.2） |
| 冻结清单 | ⚠️ 归档会新增 main spec `openspec/specs/cep-nictlm-credit-filter/spec.md` ⇒ 命中既有 glob `openspec/specs/cep-*/spec.md` ⇒ **须** `cep_freeze_check.sh --update --authorized-by "Entry #NNN"`（T3.4）;⛔ `NICTLM` 源码**不在**冻结面 ⇒ **不得**因此新增冻结条目 |

---

## 6. 已用轮次

| 轮次 | 结果 |
|---|---|
| 1 / 3 | ✅ PASS（6/6 AC + G-1） |

**结论**：变更**可归档**（`openspec archive cpptlm-nictlm-credit-flit-filter`，⛔ 不用 `--skip-specs`）⇒ KPI 3/3 → **2/3**，释放 1 个 OpenSpec slot。

**关联**：台账 Entry #468（实施记录）· `openspec/changes/cpptlm-nictlm-credit-flit-filter/proposal.md`（`## Acceptance`）· `docs/adr/ADR-X.16-router-local-port-terminal-credit.md`
