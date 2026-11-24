# 验收报告：cep-hop-timing-capability-declaration（G10 · 协议语义层的 hop / 时序建模声明面）

> **角色**：verifier（角色内建验收 · 只验不改）· **日期**：2026-11-24
> **变更集**：`9d10eddc`（立项 commit）→ `57683fb2`（HEAD）· 3 个 commit：`6884f298` 产物 · `85382985` 冻结清单 · `57683fb2` 五步链回执
> **结论**：**PASS 17/17**（⛔ 0 项失败）· 另有 **1 条 SHOULD_FIX + 2 条 NIT**（均**不阻塞**归档）

---

## 0. 判据来源

| 优先级 | 来源 | 本次取用 |
|:--:|---|---|
| 1 | 用户本次明确给出的验收条件 | 无额外（用户指令为「继续推进任务1」+ 选定 verifier 验收） |
| 2 | `openspec/changes/cep-hop-timing-capability-declaration/proposal.md` 的 `## 验收标准` | **17 条 AC**（AC-1 ` AC-17）· **本报告的主判据** |
| 3 | 本次变更自己声明的行为 | commit message + `design.md` D-3 判据骨架（J-1 ` J-8） |
| 4 | ADR 的 Verification 章 | 本单元**零 ADR 正文变更** ⇒ 不适用 |

## 0.1 分流判定（为何走本地验收，⛔ 非「跳过」）

| 条件 | 判定 | 证据 |
|---|---|---|
| `openspec/changes/<name>/` 存在且 proposal 含 `## 验收标准` | ✅ 满足 | `proposal.md` 存在 · 含该段（17 条） |
| rdd-builder **P3 archive gate** 已触发（`iteration.json`：`status ∈ {in_worktree, completed}` ∧ `tasks_done == tasks_total > 0` ∧ 未归档） | ❌ **不满足** | `.rddf/state/iteration.json` **不存在** ⇒ 信号**不可检测** |
| `rdd-verifier` skill 可加载 | ✅ 满足（`rdd-skills.sh --json`：`installed=true` · `available[]` 含 `rdd-verifier`） | 但**前置条件 2 不成立** ⇒ **不调用** |

⇒ **走本地验收（契约 1–5 步）**。
⚠️ **本地验收额外门**（`iteration.json` 的 `tasks_done == tasks_total > 0`）在本项目**不适用**（文件缺失 ⇒ 无此状态面）⇒ 以 **change 的 `tasks.md` 实施段全勾 + 17 条 AC 全量取证**代替，并在 §5 如实标注该门 **N/A**（非「跳过」）。

---

## 1. 验证顺序与总结果

| # | 步 | 结果 | 关键证据 |
|:--:|---|---|---|
| 1 | 编译 / 静态检查 | ✅ | `ninja -j4` rc=0 · `agents_md_lint.py` rc=0 · `openspec change validate … --strict` = valid |
| 2 | 单元测试 | ✅ | `cpptlm_tests "[cep][hop_timing]"` = **92 assertions / 5 test cases** 全通过 |
| 3 | 集成 / 端到端 | ✅ | **全量 ctest 205/205 rc=0**（⛔ 无 `-R` 过滤）· **52/52** `cep_*.sh` rc=0 |
| 4 | 边界与错误路径 | ✅ | **3 条反向 fixture 各按自己的红因签名变红** + 编译期探针**正控/负控** + 编译期探针**突变实测**（§2 AC-5） |
| 5 | 遗留清扫复核 | ✅ | 三个新文件零 TODO / FIXME / 调试输出 / 注释掉的代码（§4） |

---

## 2. 17 条 AC 逐条判定

| AC | 判据 | 判定 | 证据（工具 + 输出摘要） |
|:--:|---|:--:|---|
| **AC-1** | 通用层零协议名 / 零 include 协议头 | ✅ | `grep -nE "ChiRnfProtocolEngine|…|#include.*(protocol_engine|_adapter)" include/tlm/cep/hop_timing_capabilities.hh` ⇒ **零命中**；闸面「✓ AC-1 零协议类名 / ✓ AC-1 零 include 协议头」 |
| **AC-2** | 实例集双向零 orphan | ✅ | 闸「✓ AC-2 正向 / ✓ AC-2 反向」；独立实测：JSON 实例 **8** ↔ 声明行 **8** · 漏声明 **无** · 多余声明 **无**；测试用例「CEP hop_timing: 实例集双向零 orphan（J-1）」 |
| **AC-3** | 协议语义层 hop / 时序**恒 None**（现状锁） | ✅ | 闸「✓ AC-3 现状锁」；测试逐行 `CHECK(protocol_hop == HopModel::None)` / `CHECK(protocol_timing == None)`；**反向**：fixture ③ ⇒ 红因 `hop-timing-protocol-model-present`（**命中自己的签名**）；谓词负控 SECTION「协议语义层 hop ≠ None ⇒ 必假」 |
| **AC-4** | 引擎层 hop / 时序 API **反向存在性** | ✅ | 闸 J-4「✓ AC-4 引擎层零 hop / 时序 API（去注释扫描）」；**反向**：fixture ②（引擎头注入 `last_hops()`）⇒ 红因 `hop-timing-engine-api-detected`；闸级模式词表**不含 `tick`**（步进钩子不误判，已实测 `chi_rnf`/`mesif` 有 `tick()` 仍绿） |
| **AC-5** | 编译期否定断言（5 引擎 × 2） | ✅ | `test_cep_hop_timing_capabilities.cc` 内 **10 条 `static_assert(!has_*_api_v<…>)`**（签名含**实例 id 字面量**）；**探针有牙（突变实测，verifier 侧）**：把 `chi_rnf_protocol_engine.hh` 复制到 `/tmp/opencode/g10probe/` 并注入 `public: inline int last_hops()` ⇒ `g++ -fsyntax-only` **rc=1** 且输出 `error: static assertion failed: chi_rnf: 引擎暴露了 hop 计数 API ⇒ 协议语义层已有 hop 模型 ⇒ 须走决策 + 更新声明`；**对照组**（同 probe、未注入）⇒ **rc=0**（不误报）；**正控/负控** `static_assert(has_hop_api_v<WithHop>)` / `!has_hop_api_v<WithNothing>` 在测试内 |
| **AC-6** | 载体 / 器件层 hop / 时延**存在性** | ✅ | 闸「✓ AC-6 载体 / 器件层 hop / 时延事实齐备」；独立实测：`cohif_nic.hh::hops` 6 处 · `last_recv_hops` 3 处 · `cohif_responder.hh::completion_latency_cycles` 4 处 · `stub_cache_core.hh::snoop_latency_cycles` 2 处；测试用例「载体 / 器件层既有 hop / 时延事实仍在（J-6）」 |
| **AC-7** | `basis` 非空 + id 唯一 | ✅ | 闸「✓ AC-7 basis 非空 / ✓ AC-7 id 唯一」；测试逐行 `REQUIRE(basis != nullptr)` + `REQUIRE(basis[0] != '\0')`；谓词负控 SECTION（basis 置空 / nullptr / id 重复 / id 置空 ⇒ 必假） |
| **AC-8** | 与 G1 正交（无 `egress` / `in_flight`） | ✅ | 闸「✓ AC-8 与 G1 正交」；独立实测：声明表 struct + init 区段内 `egress\|in_flight` 命中 **无**；`git diff 9d10eddc..HEAD -- include/tlm/cep/engine_capabilities.hh` ⇒ **零改动**（零真源重叠） |
| **AC-9** | `tick()` ≠ 时序模型（诚实边界） | ✅ | 头文件常量 `kTickIsSteppingHookNotTimingModel`；措辞「**步进**钩子 … ⛔ **不是**消息级时序模型」在位；闸「✓ AC-9 / AC-10 关键措辞在位（tick() ≠ 时序模型 · 仅对协议语义层成立）」 |
| **AC-10** | 仅对协议语义层成立（防误读） | ✅ | 头文件常量 `kHopTimingAbsenceIsProtocolSemanticsOnly` + 措辞「**仅对协议语义层**」；闸同条 ✓；**边界未越层**的独立佐证：全 `include/tlm/cep/*.hh`（50 份）去注释扫描 ⇒ 仅 **3 份载体/器件头**含 hop/latency token（`cohif_nic.hh` / `cohif_responder.hh` / `stub_cache_core.hh`），**无任何 adapter / state_spec** ⇒ 「协议语义层零 hop/时序」在**全域**成立，非仅引擎头成立 |
| **AC-11** | 三条反向 fixture · 各按自己的红因签名 | ✅ | `--selftest`：**基线拷贝根 rc=0** + 变异① `hop-timing-orphan-extra` + 变异② `hop-timing-engine-api-detected` + 变异③ `hop-timing-protocol-model-present` ⇒ **3/3 按预期红因变红**（⛔ 非「rc≠0」冒充红因） |
| **AC-12** | main spec + 五步链 | ✅ | `openspec/specs/cep-hop-timing-capability-declaration/spec.md` 在位（6 Requirement / 10 Scenario）；delta 由主 spec **提取生成**（`## ADDED Requirements`，零漂移）；`openspec change validate … --strict` = **valid**；`openspec validate --specs --strict` ⇒ `✓ spec/cep-hop-timing-capability-declaration`；冻结清单 `artifact_count` **92 → 93** · `frozen_at_commit=6884f2985ae0` · 含新 spec = **True**；五步链 commit 链 `6884f298` → `85382985`（清单**单独** commit，1 文件）→ `57683fb2`（回执）；无参 `cep_freeze_check.sh` **rc=0**（93 产物 hash 一致） |
| **AC-13** | 计数同步面 | ✅ | 实测：主 spec **140**（`cep-*` **49**）· `cep_*.sh` **52** + `cep_*.py` **14** = **66** · `test_cep_*.cc` **79** · ctest **205**；登记册 §0.1 三行取值逐字一致；`scripts/README.md` 守门表新增行（命中 1）· `include/tlm/cep/AGENTS.md` 索引含 `engine_capabilities.hh`（**补登 I1 漏项**）与 `hop_timing_capabilities.hh`；`cep_registry_baseline_check.py` rc=0（5 项取值 == 实测） |
| **AC-14** | 零回归 · **全量口径** | ✅ | `ctest --test-dir build -j4`（⛔ **不按 `-R` 过滤**）⇒ **205 / 205 passed, 0 failed**（rc=0）；**52 / 52** `cep_*.sh` rc=0（GATES PASS=52 FAIL=0）；**非 `cep` 只读闸 9 / 9 rc=0**（`archive_verify` / `config_type_registration` / `docs_adr_index` / `docs_index_consistency` / `markdown_link` / `openspec_version` / `test_sh_arg_parsing` / `rddf_stage_gate` / `cep_docs_sync_check --strict`） |
| **AC-15** | 零 schema 变更 / 零 ADR 正文变更 | ✅ | `git diff --name-only 9d10eddc..HEAD -- include/tlm/cep/protocol_instance.schema.json include/tlm/cep/protocol_instances.json include/tlm/cep/engine_capabilities.hh include/tlm/cep/*_protocol_engine.hh docs/adr/` ⇒ **空** |
| **AC-16** | 不新增 invariant 编号 | ✅ | `cep_invariant_map_check.py` ⇒「**66/66** = 66 项有牙载体 + 0 项 ⭭-unmodelled + 0 项 ⭭-pending」；`git diff 9d10eddc..HEAD -- test/cep_invariant_assertion_map.json` ⇒ **空** |
| **AC-17** | 诚实边界**双处**登记 | ✅ | 头文件：「本头**只登记现状**」+「⛔ 不补 hop 计数；⛔ 不补消息级时序模型」+「**仅对协议语义层**成立」；main spec Purpose：「本能力**只登记现状、只让缺口可检出**，⛔ **不补齐** hop / 时序模型（那属协议 FSM 语义设计，per 宪章归 ArchForge）」+ 三层区分段 ⇒ **双处齐备** |

**判定汇总：PASS 17 / 17（失败 0）**

---

## 3. 发现（均不阻塞归档）

| # | 级别 | 发现 | 证据 | 建议 / 路由 |
|:--:|:--:|---|---|---|
| **F-1** | SHOULD_FIX | **编译期探针只对 public 成员有牙**（C++ 访问检查参与 SFINAE ⇒ private 成员不可见）。当前注入 private `last_hops()` ⇒ 编译**通过**（未捕获）；注入 public `last_hops()` ⇒ `static_assert` 触发 | §2 AC-5 突变实测（两对照） | 与 gap 分析 R2「探测点必须是**公开可观察**的 API」一致 ⇒ **不是缺陷**；但头文件未写明该语义 ⇒ 建议 builder 在探针注释补一句「探针只看 **public** 成员；private 辅助函数由闸级 J-4 去注释扫描兜底」。**零行为变更**。路由：builder（注释级） |
| **F-2** | NIT | 闸 J-4 的模式词表按**token 子串**匹配（hop / latency / …）⇒ 将来引擎若引入含这些子串的**无关**标识符会误报 | 闸面模式词表 | 属**保守方向**（宁可误报不可漏报）· 当前实测零命中 ⇒ **不改**；若将来误报再按具体 token 收窄 |
| **F-3** | NIT | 无参 `cep_freeze_check.sh` 会提示 `frozen_at_commit ≠ 当前 HEAD`（因清单本身是后一 commit）⇒ **警告非红** | §2 AC-12 实测输出 | 既有机制 · 非本 change 引入 ⇒ **不改** |

---

## 4. 遗留清扫复核（本次范围 = 3 个新文件 + 1 个新 main spec + change 工件）

| 类别 | 检查 | 结果 |
|---|---|---|
| TODO / FIXME / XXX / HACK | 三个新文件全量 grep | **零命中** |
| 半截实现 | 声明面：枚举 / struct / 表 / 谓词 / 探针 / 常量 **均实现完整**（无空壳函数、无半截 TODO） | ✅ |
| 临时调试输出 | grep printf / std::cerr / DPRINTF | **零命中**（⛔ 遵守「不用 printf/stderr 诊断」铁律） |
| 注释掉的代码 | 行尾注释调用形态 + `#if 0` | **零命中** |
| 被 skip 的测试 | `[cep][hop_timing]` 5 个用例全部**实跑**（92 断言）· 无新建 `.disabled` | ✅ |
| 未替换的 mock | 本单元**零 mock** | ✅ |
| 临时 fallback 分支 | 闸的 `--selftest` 走**拷贝根**（`--root`）⇒ **零工作树污染**（既有自检以 sha256 前后相等守护） | ✅ |
| 范围外历史遗留 | **不清理**（本单元不趁机改动）· 仅记录 | ✅ |

---

## 5. 失败项与路由建议

- **失败项：0**（17/17 PASS）⇒ 无需路由回 builder 修实现。
- **建议路由（均不阻塞）**：F-1 走 builder 注释级补写（可并入下一个 CEP 文档维护窗口）；F-2 / F-3 记入 backlog，**本次不改**。
- **归档前置**：归档与计数回填属 **builder** 职责（`openspec archive` + KPI/计数回填 + 台账 Entry #543 收口）⇒ 需切回 builder 执行。

---

## 6. 已用轮次

| 轮次 | 内容 | 结果 |
|:--:|---|---|
| 1 | 本地验收（17 AC 全量取证 + 全量回归复核 + 遗留清扫） | **PASS 17/17** · 3 条非阻塞发现 |

**下一步（builder）**：`openspec archive cep-hop-timing-capability-declaration --yes` ⇒ 验证主 spec Purpose **非占位** ⇒ 计数 / KPI 回填（active 1→0 · KPI 1/3→0/3 · 归档 174→175 · CEP 58→59 · 主 spec 保持 140 / 闸 52 / 测试 79 / 冻结保持 93）⇒ 台账 Entry **#543** 收口（附本报告指针）⇒ 归档后**复跑** AC-13 / AC-14 的计数与回归口径（⛔ 回填本身会改 7 处活口径 ⇒ 必须复验）。

---

## 7. 复现命令（本次全部证据）

```bash
# 契约闸 + 反向 fixture
bash scripts/test/cep_hop_timing_capabilities_check.sh
bash scripts/test/cep_hop_timing_capabilities_check.sh --selftest
# 判据测试
./build/bin/cpptlm_tests "[cep][hop_timing]" -r compact
# 全量回归（⛔ 不按 -R 过滤）
for s in scripts/test/cep_*.sh; do bash "$s" >/dev/null 2>&1 || echo "FAIL $s"; done
ctest --test-dir build -j4
# 非 cep 只读闸（9 个）
for c in archive_verify_check.sh config_type_registration_check.sh docs_adr_index_check.sh \
         docs_index_consistency_check.sh markdown_link_check.sh openspec_version_check.sh \
         test_sh_arg_parsing.sh rddf_stage_gate.sh cep_docs_sync_check.sh; do bash scripts/test/$c; done
# 冻结链
bash scripts/test/cep_freeze_check.sh
# 零改动硬查
git diff --name-only 9d10eddc..HEAD -- docs/adr/ include/tlm/cep/protocol_instance.schema.json \
  include/tlm/cep/protocol_instances.json include/tlm/cep/engine_capabilities.hh
```
