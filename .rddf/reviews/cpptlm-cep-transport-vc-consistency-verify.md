# C2 · 传输层 VC 语义一致性判据落地 —— verifier 验收报告

> **作者**: build agent（**verifier** mode）· 日期: **2026-11-12**
> **判据来源**: `openspec/changes/cpptlm-cep-transport-vc-consistency/proposal.md` 的 **17 条 AC**（§验收标准）
> **上游**: `.rddf/improvements/cpptlm-cep-transport-vc-consistency.md` · 台账 **Entry #344**（W0 + K-1/K-2）· **#345**（立项）· **#346**（metis gap 处置）· **#347**（A0 授权）· **#348**（A1）· **#349**（A2）· **#350**（A3）· **#351**（A4）· **#352**（A5/A7）· **#353**（A6）
> **分流判定**: ⛔ **未走 `rdd-verifier` skill** —— 其 canonical precondition ②(P3 archive gate:`.rddf/state/iteration.json` 的 `status ∈ {in_worktree, completed}`)**不满足**(该文件不存在)⇒ 走**内置本地验收**(角色矩阵「verifier 产出契约」1~5 步)
> **纪律**: ⛔ **verifier 只验不改** —— 本次发现的问题**不回改实现**,一律路由回 builder

---

## 〇 · 一句话结论

**17 / 17 AC 全部 PASS**(每条带工具证据);**1 项非 BLOCKER 发现**(新增探针文件未过 `format --check`);**0 BLOCKER** ⇒ **C2 可归档**。

---

## 一 · 判据来源与分流

| # | 项 | 内容 |
|:--:|---|---|
| 1 | **判据来源** | `proposal.md` §验收标准 的 **17 条 AC**(与 `.rddf/improvements/…` 的集合**双向差集为空**,实测) |
| 2 | **上游裁决** | `T-C2-⓪`(条件化口径)· `T-C2-①`(扩展 C-1′ 适用域)· `U-3`(不改声明)· `K-1`(R-1/R-2 落诚实边界段)· `K-2`(锚点拆两处 + tag `R1-b`/`R1-c`)—— **本次验收未重新讨论** |
| 3 | **skill 分流** | ① change 存在 + 有 `## 验收标准` 段 ✅ · ② P3 archive gate(`iteration.json`)❌ ⇒ ③ skill 可加载但**前置不满足** ⇒ **走内置 1~5 步** |

---

## 二 · 逐条 AC 判定(17 条 · 每条带工具证据)

### AC-1(决定性变异 · (a) 消息类)—— ✅ PASS

| 项 | 证据 |
|---|---|
| 判据 | scope **内**的 network 上改某实例某 VC 的消息类 ⇒ 必红,归因 `[CP-03-VC-CERTIFICATE/C-1′]` |
| 实测 | 既有 fixture `test/test_cep_ualink_inc.cc` 的 C-1′ 语义面反向 fixture ⇒ **PASS**(该断言逐字未变,见 §三) |
| **反向判据**(scope 外不得红) | **verifier 自跑**探针:`compute_noc` 上 **6 种**破坏(改 ordering 值 / 删条目 / 非法 key / 越界 key / 改 `vc_binding` / 显式 `null` 绑定)⇒ **全部 `verdict=0`** ⇒ `failures = 0` ⇒ **无 scope 泄漏** ✅ |

### AC-2(基线仍绿)—— ✅ PASS
`evaluate_shared_mode_certificate` 基线 ⇒ `verdict=PASS`,detail 前缀 `[CP-03-VC-CERTIFICATE]`(verifier 自跑探针输出)。

### AC-3(决定性变异 · (b) ordering)—— ✅ PASS
**verifier 独立复核**:毒化 `mesif.ualink_fabric.vc3: p2p_ordered → unordered` ⇒ `verdict=FAIL` + 前缀 **`[CP-03-VC-CERTIFICATE/R1-b]`**(逐字相符)。
**「旧绿 / 新红」并排**:同一毒化输入 ⇒ 旧实现(`0ceb0860`)**PASS** / 新实现 **FAIL** ⇒ 跨实现取证成立(探针源码 `test/probes/c2_oldnew_probe.cc`,**两版均编译成功** ⇒ 版本无关)。

### AC-4(决定性变异 · (b) VC 不完备)—— ✅ PASS
删某 entry 的 vc2 ⇒ `verdict=FAIL` + 前缀 **`R1-b`**(verifier 自跑)。红因文本含「未覆盖 vc2」。

### AC-5(决定性变异 · C-1′-S scope 完备性)—— ✅ PASS
`plane_binding` **插键** `chi_rnf → FabricCollectivePlane` ⇒ `verdict=FAIL` + 前缀 **`[CP-03-VC-CERTIFICATE/R1-c]`**。
**红因可区分性**:detail 含「**不在 `certificate_scope`** 内」且 **⛔ 不含**「plane 未登记」⇒ 与 s-2 的归因文本**可区分**(测试断言已钉)。

### AC-6(反 vacuous · 既有基线)—— ✅ PASS
`certificate_scope` 清空 ⇒ `verdict=FAIL`,前缀 `[CP-03-VC-CERTIFICATE]`(命中**既有** `:901-904` 空早退)⇒ 符合该 AC 的定位(⛔ 不得用它证明新判据有牙)。

### AC-7(文档同步)—— ✅ PASS
`docs/architecture/cep/fabric-transport.md` 的 R5 表「当前机器判定」列已更新(R1 行不再写「(b) 无载体」/「`compute_noc` 无判定」);R1 段含「作用域 = 条件化口径」框;**`:142` 有 `R1-a ≡ C-1′` 映射**;`:173` 的 `R1-c` 悬空引用已处置(改为「三条均已机器化」)。

### AC-8(0 回归 + 冻结链 · 3 处)—— ✅ PASS
| 面 | 实测 |
|---|---|
| `ctest --test-dir build -j4` | ✅ **169 / 169 passed, 0 failed**(128.06s;另**连跑 3 轮**均绿) |
| `[cep]` Catch2 | ✅ **All tests passed(11672 assertions in 512 test cases)** |
| **35 个** `cep_*.sh` 闸 | ✅ **35 / 35 rc=0**;✗ 总数 **1**(唯一 1 条在 `cep_freeze_reverse_verify.sh` 的「**故意破坏**」步内 ⇒ 反向验证的**预期证据**) |
| `docs_sync_check.sh --strict` | ✅ rc=0 |
| `agents_md_lint.py` | ✅ rc=0 |
| `cep_freeze_check.sh` 无参 | ✅ rc=0;`artifact_count` = **78** |
| 五步链 | ✅ ①Entry#352 → ②产物 `b03c693b` → ③`--update --authorized-by "Entry #347"` → ④清单 `f85502dd` → ⑤无参 rc=0 |

### AC-9((b) 的实例集合与缺失行为)—— ✅ PASS
实现的口径**逐字复刻** `per_network_mappings` 的 push 条件(`include/tlm/cep/collective_plane_contract.hh` 的 `if (!mapping.empty())`);快照正控实测 `per_network_orderings["ualink_fabric"].size() == 3` 且每条 key 集合 == `{0,1,2,3}`;测试含「两字段实例条目数逐 network **恒等**」断言。

### AC-10(越界 / 非规范 VC key)—— ✅ PASS
**verifier 独立复核三类**:① key `"abc"` ⇒ FAIL `R1-b`;② key `"04"` ⇒ FAIL `R1-b`(**前导零** —— schema 的 `^[0-9]+$` **挡不住**);③ key `"4"`(>= `vc_count`=4)⇒ FAIL `R1-b`。实现用 `std::from_chars`(⛔ 不用 `stoul`),且**先判越界再判缺失**。

### AC-11(C-1′-S 的 vacuous 探测)—— ✅ PASS
令绑 `FabricCollectivePlane` 的实例无任何 `vc_binding` ⇒ `verdict=FAIL` + 前缀 **`R1-c`**(verifier 自跑)。

### AC-12(C-1′-S 的「旧绿 / 新红」并排)—— ✅ PASS
**verifier 独立复跑**:同一毒化**声明**输入 ⇒ 旧实现 **PASS** / 新实现 **FAIL**(`R1-c`)。
**诚实边界已逐字写明**(`.rddf/reviews/cpptlm-cep-transport-vc-consistency-oldnew-evidence.md` §4):旧实现**根本不读** `decl.plane_binding`(M-2 模式下 V6-c 为 SKIP)⇒ 其「旧绿」是**平凡**的 ⛔ 未包装成「旧实现检查得更宽松」。

### AC-13(声明数据零改动)—— ✅ PASS
`git diff --exit-code 0ceb0860 -- <protocol_instances.json / collective_planes.json / fabric.json / protocol_instance.schema.json>` ⇒ **rc=0**;4 个 sha256 与 **Entry #347 ④** 记录**逐字一致**。

### AC-14(归因 tag 精确前缀不冲突 + 既有归因不变)—— ✅ PASS
**机器化核对**(verifier 自跑):旧版(`0ceb0860`)**8** 条前缀断言 vs 当前版 **10** 条 ⇒ **既有 8 条按相对顺序逐字保留**(子序列匹配,**缺失 0**);新增 2 条仅来自新用例自身的基线断言;分组计数 `1/2/1/2/2` == `1/2/1/2/2`。

### AC-15(真实 gate 路径可达)—— ✅ PASS
新增用例 `[cep][ualink-inc][real-gate]` ⇒ **17 assertions / 1 test case**;三场景(基线 / 影子 `protocol_instances` / 影子 `declaration`)**均**断言 `report.lines.size() == kContractRuleCount`(= 6)且用 `detail.find(prefix) != npos`(⛔ 未用 `rfind(...,0)` —— CP-03 的 detail 是拼接的);影子文件走 `temp_directory_path()` ⇒ ⛔ 未碰仓内 JSON。

### AC-16(`R-1` / `R-2` 登记的文字守门)—— ✅ PASS
`docs/architecture/cep/fabric-transport.md` 的诚实边界段实测:「已验证」= **0** 次 ·「已满足」= **0** 次;`R-1` / `R-2` **可 grep 命中**。
> ⚠️ 本 AC 为**措辞核对**(非红/绿闸)—— per **K-1** 与 **Entry #346 ⑨** 的 P0-4 裁决:⛔ 不扩 `UntestedRegistry`、⛔ 不新建登记面 ⇒ **无机器闸**。实现已把引用 C-4 守门的措辞由「未被冒充成**已满足**」改为「未被冒充**为达成**」⇒ 字面归零。

### AC-17(文档地位与映射)—— ✅ PASS
`R1-a ≡ C-1′` 映射在 `fabric-transport.md:142`;地位差异(`U-1`/`U-2` **有** C-4 机器守门 / `R-1`/`R-2` **没有**、不参与任何闸)在 `:138-139` ⇒ 逐字可核。

### 判定汇总

| 结果 | 条数 | AC |
|:--:|:--:|---|
| ✅ PASS | **17** | AC-1 ~ AC-17 全部 |
| ❌ FAIL | 0 | — |
| ⏭ 不适用 | 0 | — |

---

## 三 · 独立复核(不采信 builder 回执)

| # | 复核项 | 方法 | 结果 |
|:--:|---|---|---|
| 1 | **8 条决定性变异的红因** | verifier **自写探针**(`/tmp/opencode/verify_mutations.cc`)跑出每条的 verdict + **实际前缀** | ✅ 8/8 按**正确原因**红,前缀精确 |
| 2 | **AC-1 反向判据**(scope 泄漏) | verifier 自跑 6 变体探针 | ✅ `failures = 0` |
| 3 | **AC-3 / AC-12 并排取证** | verifier **独立复跑**旧/新两个探针 binary | ✅ 旧 PASS / 新 FAIL ×2 |
| 4 | **既有归因前缀不变** | verifier 自跑**子序列匹配**脚本 | ✅ 缺失 **0** |
| 5 | **声明数据零改动** | verifier 自跑 `git diff --exit-code` | ✅ rc=0 |
| 6 | **`kContractRuleCount`** | verifier 自查 | ✅ 仍 **6** |
| 7 | **AC-16 字面** | verifier 自跑「已验证/已满足」计数 | ✅ 0 / 0 |

---

## 四 · 边界与错误路径(验证步骤 4)

| # | 边界 | 实测 |
|:--:|---|---|
| 1 | **scope 外不得红** | ✅ 6/6 不红 |
| 2 | **scope 为空** | ✅ 命中既有空早退 ⇒ FAIL |
| 3 | **`plane_binding` 零键** | ✅ C-1′-S vacuous PASS(无主体)⇒ 与 V6-c 的「零键 ⇒ SKIP」**语义不同**(L-7 已登记) |
| 4 | **单实例 network** | ✅ 记为 **L-1**(空转无鉴别力);当前 `ualink_fabric` 有 3 实例 |
| 5 | **同一 VC 多类消息** | ✅ 记为 **L-2**((b) 只证跨实例一致) |
| 6 | **ordering 取值域非法** | ✅ **不在判据内**(既有 `cep_decl_alignment_check.sh` 的 `jsonschema` 已挡)⇒ **L-3**;⛔ 判据**不重复实现**(禁双实现) |
| 7 | **溢出 key** | ✅ `from_chars` 的 `errc` 分支 ⇒ 归「越界 FAIL」,⛔ 不抛异常 |
| 8 | **`id` 缺失 / 重复** | ✅ loader 置 `source_error`(⛔ 不静默排除) |

---

## 五 · 遗留清扫复核(验证步骤 5)

| 文件 | TODO/FIXME/XXX/HACK | skip 的用例 | 调试残留 |
|---|:--:|:--:|---|
| `include/tlm/cep/collective_plane_contract.hh` | **0** | **0** | **无** |
| `test/test_cep_ualink_inc.cc` | **0** | **0** | **无** |
| `test/probes/c2_oldnew_probe.cc` | **0** | **0** | **无**(仅 `std::cerr` 的**用法提示**与**错误路径**) |

- ⛔ **未替换的 mock / 临时 fallback / 注释掉的代码**:本次改动的 3 个文件内 **0 处**(`if (false)` / `#if 0` / 临时 `return true` 均无命中)。
- ✅ **临时文件不残留于仓内** —— 取证影子文件在 `/tmp/opencode/`(临时目录)⇒ 仓内 0 残留。
- ⚠️ **范围外的历史遗留**(仅记录,**不在本 change 顺手清理**):`scripts/build/format.sh --check` 报 **11** 个文件需格式化,其中 **9** 个**本次未改动**(`src/core/module_factory.cc` · `test/test_cep_bridge_reattach_guard.cc` · `test/test_cep_schema.cc` · `test/test_cep_simulation_mode_lock.cc` · `test/test_cep_stub_cache_e2e.cc` · `test/test_cep_stub_cache_emit.cc` · `test/test_dual_port_adapter.cc` · `test/test_dual_port_inbound_dispatch.cc` · `test/test_framework_bidirectional_multiport_inbound.cc`)⇒ **不属本 change 范围**。

---

## 六 · 发现(1 项 · **非 BLOCKER**)

### F-1 · [SHOULD_FIX] `test/probes/c2_oldnew_probe.cc` 未过 `clang-format --check`

| 项 | 内容 |
|---|---|
| **事实** | `bash scripts/build/format.sh --check` ⇒ 该文件列入「✗ 需要格式化」 |
| **范围判定** | ✅ **属本 change 范围** —— 该文件是本次**新增**(commit `400b9009`,per Entry #351)⇒ ⛔ 不是历史遗留 |
| **对照** | `test/test_cep_ualink_inc.cc` 同样被报,但实测**改动前(`0ceb0860`)就不合规** ⇒ **历史遗留**,⛔ 不属本 change |
| **影响** | ⛔ **不影响任何判据的正确性**(探针是**取证工具**,不入 ctest、不参与构建产物)⇒ **非 BLOCKER** |
| **建议** | 回 **builder**:`clang-format -i test/probes/c2_oldnew_probe.cc` ⇒ 单文件小修(⛔ 不得连带格式化那 9 个历史遗留文件) |
| **判定** | ✅ **可归档**(F-1 不阻断)—— 但建议**归档前**顺手修掉(1 条命令) |

> ⚠️ **verifier ⛔ 不改实现** ⇒ 本条**路由回 builder**。

---

## 七 · 边界说明

1. ⛔ **本次验收未重新讨论**已裁决事项(`T-C2-⓪` / `T-C2-①` / `U-3` / `K-1` / `K-2`)—— 全部按已签发口径验收。
2. ⛔ **未走 `rdd-verifier` skill** —— 其 canonical precondition ②(P3 archive gate)**不满足** ⇒ 走**内置 1~5 步**。
3. ⛔ **verifier 只验不改** —— F-1 **不回改**,路由回 builder。
4. ⚠️ **「旧绿」的平凡性已逐字登记** —— AC-12 的旧实现**根本不读** `decl.plane_binding` ⇒ 其「旧绿」是**平凡**的,⛔ 不得被引申为「旧实现也检查了但更宽松」。
5. ⚠️ **AC-6 命中的是既有空早退** ⇒ ⛔ 不得用它证明新判据有牙(其定位本就是回归基线)。
6. ⚠️ **9 个历史遗留的 format 问题**仅记录,**不在本 change 顺手清理**。

---

## 八 · 结论

| # | 项 | 结论 |
|:--:|---|---|
| 1 | **17 条 AC** | ✅ **17 / 17 PASS**(每条带工具证据) |
| 2 | **BLOCKER** | ✅ **0** |
| 3 | **SHOULD_FIX** | **1**(F-1:新增探针未过 `format --check` ⇒ ⛔ 不阻断归档,建议回 builder 单文件修) |
| 4 | **0 回归** | ✅ `ctest` **169/169** · `[cep]` **512 cases / 11672 assertions** · 35 闸 **35/35 rc=0** · 冻结链 5 步全绿 · `artifact_count` = **78** |
| 5 | **声明数据** | ✅ 4 个文件**零 diff** + sha256 与 Entry #347 逐字一致 |
| 6 | **既有归因** | ✅ 8 条前缀断言**逐字保留**(缺失 0) |
| 7 | **遗留清扫** | ✅ 本次范围内 **0** 处 |
| 8 | **总判定** | ✅ **PASS** —— **C2 可归档** |

**已用轮次:1 / 3**(本报告为第 1 轮;若 F-1 修完复验,为第 2 轮,仍在预算内)。

---

## 九 · 关联

台账 **Entry #353**(A6 冻结链)· **#352**(A5/A7)· **#351**(A4 并排取证)· **#350**(A3)· **#349**(A2)· **#348**(A1)· **#347**(A0 授权)· **#346**(metis gap 处置)· **#345**(立项)· **#344**(W0 + K-1/K-2)· `.rddf/reviews/cpptlm-cep-transport-vc-consistency-oldnew-evidence.md`(并排取证)· `openspec/changes/cpptlm-cep-transport-vc-consistency/`
