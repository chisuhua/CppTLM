# 验收报告：cep-litmus-falsifier-naming-disambiguation（阶段 1）

> **验收角色**：verifier（角色内建，非 subagent）· **日期**：2026-11-23
> **验收对象**：change `cep-litmus-falsifier-naming-disambiguation`（`skip_specs: true` · 零 main spec delta）
> **结论**：✅ **PASS**（无条件）—— 6/6 AC 全部通过，**每条均有工具证据**

---

## 1. 分流判定（为何走本地验收，而非 `rdd-verifier` skill）

| 条件 | 状态 | 证据 |
|---|:--:|---|
| ① change 有 `## 验收标准` 段 | ✅ | `openspec/changes/cep-litmus-falsifier-naming-disambiguation/proposal.md:49-56`（6 条 `- [ ] **AC-N**`） |
| ② P3 archive gate 触发（`iteration.json` 中 `status ∈ {in_worktree, completed}` ∧ `tasks_done == tasks_total > 0`） | ❌ | `.rddf/state/iteration.json` **不存在** ⇒ 条件不满足 |
| ③ `rdd-verifier` skill 可加载 | ✅ | `~/.config/opencode/scripts/rdd-skills.sh --json` → `available[]` 含 `rdd-verifier` |

⇒ **条件 2 不满足 ⇒ 走本地验收（1–5 步）**。本地验收额外门（`tasks_done == tasks_total > 0`）以 change 的 `tasks.md` 复选框为代理口径：**19/19 全勾选** ✅。

**判据来源**（按优先级）：① 用户本次明确条件（「选择A, 继续推进」= 实施 I2 阶段 1）② change `proposal.md` 的 `## 验收标准` 段（**6 条 AC**）③ 本 change 自身声明的行为 ④ ADR `Verification` 章 —— **本单元无新增 ADR ⇒ 来源 ④ 不适用**。

---

## 2. 逐条判定（6 条 AC）

### AC-1 消歧常量存在且取值正确 · 机检 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| `kFalsifierScope` 逐字 == `"channel-dependency-graph"` | `grep -n 'kFalsifierScope = "channel-dependency-graph"' include/tlm/cep/litmus_falsifier.hh` → `:61` | ✅ |
| `kFalsifierIsMemoryModelLitmus == false` | `grep -n 'kFalsifierIsMemoryModelLitmus = false' …` → `:67` | ✅ |
| 运行期断言 | `./build/bin/cpptlm_tests "[cep][litmus][falsification][disambiguation]"` → **All tests passed (7 assertions in 1 test case)** | ✅ |
| **反 fixture**：改 `true` ⇒ `static_assert` 编译失败（红因可见） | 闸内 probe TU：`bash scripts/test/cep_litmus_naming_check.sh` → 臂③「基线：probe TU 编译通过」+「变异：编译失败且命中签名『litmus-scope』」 | ✅ |

### AC-2 禁用表述闸 · 行为判据 · 三路 → ✅ PASS

**闸内自测三路**（影子副本 ⇒ 仓内零改动）：
```
✓ 臂②① 肯定句「本工装是 memory-model litmus」⇒ 必红（已红）
✓ 臂②② 否定句「本工装不是 memory-model litmus」⇒ 必须绿（已绿）
✓ 臂②③ 肯定句「X 与内存模型 litmus 的区别是 Y」⇒ 必红（已红）
```

**真实文件注入实证**（额外取证，非仅靠闸内自测）：向 `include/tlm/cep/litmus_falsifier.hh` 追加一行 `// 本工装是 memory-model litmus 工装。` ⇒
```
✗ litmus-scope-claim: 以下行指涉内存模型 litmus 但同段无否定语境:
[FAIL] cep_litmus_naming_check: 1 项失败      (rc=1)
```
还原后 `rc=0`（绿）⇒ **闸有牙已实证**（红因码 `litmus-scope-claim` 可见）。还原后 `git diff --stat` 为空 ⇒ 仓内零残留。

### AC-3 头文件诚实边界可引用 · 机检 + 人工评审 → ✅ PASS

- **机检**（闸臂④）：`✓ 臂④ 头文件含顶点形态 '(instance, network, vc)' ∧ 判据对象「环」 ∧ 否定标记`
- **人工评审**（语义准确性，⛔ 不写成 checkbox 判定）：边界段实际文本（`include/tlm/cep/litmus_falsifier.hh:23-38`）三要素**齐全且语义准确** ——
  ① 顶点形态 = `(instance, network, vc)`（通道依赖图）✅ ② 判据对象 = **装配期静态图上的环存在性** ✅ ③ 与内存模型 litmus 的区别 = 论文侧「检验执行迹的允许结果」✅
- 另核：段落明确写出「⛔ 不得据本工装的存在声称『CEP 已能证伪复合一致性公理』」⇒ 与 `ADR-CEP-13` 决策 5 ② 一致 ✅

### AC-4 逻辑等价 · 零行为变更 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| 既有断言段**逐字未变** | Python 比对：基线 `46c55728:test/test_cep_litmus_falsification.cc`（185 行）== 现文件**分隔行之前**内容（`pre.strip() == base.strip()` → `True`） | ✅ |
| 分隔行存在 | `// == disambiguation additions ==` 存在（`True`） | ✅ |
| 既有 4 个 `TEST_CASE` 仍在 | 4 个用例名逐个 `in cur` → `True` | ✅ |
| 既有闸 rc=0 | `bash scripts/test/cep_litmus_falsification_check.sh` → 正控 ✓ + 反向 fixture ✓ + 「litmus 证伪工装闸通过」 | ✅ |
| 全量 litmus 用例 | `./build/bin/cpptlm_tests "[cep][litmus][falsification]"` → **All tests passed (37 assertions in 5 test cases)**（既有 4 例 30 断言 + 新增 1 例 7 断言） | ✅ |

### AC-5 零冻结面 · 机检 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| `git diff --name-only` 不含 `openspec/specs/cep-litmus-falsification/spec.md` · `docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` | 相对立项前基线 `40940871` 的完整改动面（20 文件）逐项扫描 ⇒ **零命中** | ✅ |
| 冻结清单零变化 | `bash scripts/test/cep_freeze_check.sh` → 「✓ M1 产物冻结校验通过:91 个产物 hash 全部一致」 | ✅ |
| main spec 面零变化 | `openspec/specs/` 总数 **139** · `cep-*` **48**（= 基线值）⇒ `skip_specs: true` 生效，**归档不产 main spec** ⇒ **无需五步授权链** | ✅ |

### AC-6 零回归 → ✅ PASS

| 判据 | 证据 | 结果 |
|---|---|:--:|
| `ctest` 全绿（⛔ 不写死断言数） | `ctest --test-dir build -R cep -j2` ⇒ **111/111 Passed**（含本 change 新增条目 `cep_litmus_naming_check`） | ✅ |
| 全部 `scripts/test/cep_*.sh` rc=0 | 逐个跑 ⇒ **PASS=51 FAIL=0** | ✅ |
| `cep_docs_sync_check.sh --strict` rc=0 | rc=**0** | ✅ |
| 附加：`cep_doc_ref_check.sh` / `cep_spec_strict_check.sh` / `cep_registry_baseline_check.py` / `rddf_stage_gate.sh` P-5 | 全部 rc=0 / 「基线比对通过：5 项」/ 「✓ P-5 1 处活口径全部同步（锚定 7 处）」 | ✅ |

---

## 3. ⚠️ 验收过程中的一次**失败取证**及其归因（AC-6）

**现象**：首轮 `ctest -R cep -j4` 出现 **1 项红**：`135 - cep_parity_e2e (Failed)`。

**排查过程与证据**：

| # | 排查动作 | 结果 |
|:--:|---|---|
| 1 | 单跑 `ctest -R cep_parity_e2e --output-on-failure` | **Passed**（6.09s）⇒ 非确定性 |
| 2 | 该测试内容 = `bash scripts/cep/parity/run_parity.sh`（**gem5 探针对拍**）⇒ 与 litmus 零关联 | 排除逻辑关联 |
| 3 | `run_parity.sh` 与 `run_parity_linear.sh` 的 `OUT_DIR` **同为** `$ROOT/build/parity`（**固定目录 · 无 per-test 隔离**）⇒ 并发写冲突隐患 | **既有**隐患（非本 change 引入） |
| 4 | 排除法：`ctest -R cep -E cep_litmus_naming_check -j4`（摘除本 change 的新条目） | 110/110 绿（单样本，不足以定论） |
| 5 | 定向实验：`ctest -R "cep_parity_e2e$\|cep_parity_linear_e2e" -j2`（⛔ 不含本 change 的新闸） | 2/2 绿 |
| 6 | **仓内默认并发度依据**：`AGENTS.md:147`「`./build.sh --jobs 4` # 自定义并行度（默认 **-j2 OOM-safe**）」⇒ `-j4` **超出**仓内默认安全并发度 | 归因锚点 |
| 7 | 按**默认并发度**取证：`ctest --test-dir build -R cep -j2` | **111/111 全绿** |
| 8 | 复现尝试：`-j4` 连跑 **2 轮** | **两轮均绿**（2/2）⇒ 首次红为**偶发** |

**归因判定**：该失败是**超默认并发度（`-j4`）下的偶发资源竞争**（gem5 探针与并发重测试争抢资源 / 两个 parity e2e 共用 `build/parity` 固定目录），**非本 change 引入的判据失败**，亦**非**「取证不可用」。仓内默认并发度为 `-j2`（OOM-safe），在该口径下 AC-6 **全绿**。

⚠️ **登记为遗留观察**（不在本 change 范围，未修）：`run_parity*.sh` 系列共用 `$ROOT/build/parity` 固定目录 ⇒ `cep_parity_e2e` 与 `cep_parity_linear_e2e` 并发跑存在产物互相覆盖风险。⇒ 建议后续以 `PARITY_OUT_DIR` 按测试名隔离（该 env 钩子**已存在**，见 `run_parity.sh:19`）。

---

## 4. 遗留清扫复核（build 自查项 → verifier 确认）

| 清扫项 | 结果 | 证据 |
|---|:--:|---|
| TODO / FIXME / XXX / HACK / 临时调试输出 / 注释掉代码 | ✅ 0 | 对本 change 6 个产物逐文件 grep；唯一命中为 `scripts/test/cep_litmus_naming_check.sh:114` 的 `mktemp -d …XXXXXX` **模板串**（**非**遗留） |
| 被 skip 的测试 / 未替换的 mock | ✅ 0 | `git diff 40940871 HEAD -- test/` 内无 `SKIP`/`DISABLED`/`.disabled` 新增 |
| 临时文件残留（仓内） | ✅ 0 | `git status --short` 未跟踪项仅 `.rddf/state/*` 与 `*.lock`（仓内既有 noise），**无本 change 产物残留** |
| 临时文件残留（仓外） | ✅ | 反 fixture 用的影子副本均在 `mktemp -d` 临时目录内并 `trap` 清理；备份件在 `/tmp/opencode/`（仓外） |
| 半截实现 / 空壳交付 | ✅ 0 | 新闸 4 臂全部有实现且有实证；新用例 7 断言全部有实现 |

---

## 5. 表述纪律（人工评审项）

扫描 6 个产物中的夸大断言形态（「已证明 / 已验证 / 无违例 / 公理已成立 / 协议语义已验证」）：**全部命中项均为否定式用法** ——
- 「⛔ **不得**输出「已证明」」/「⛔ 不是「已证明」」
- 「平台**不提供**无违例保证」（引 `ADR-CEP-13` 决策 5 ②）
- 「⛔ **不得**被表述为「公理已成立」」

⇒ **无夸大断言** ✅（且本 change 自身新增的消歧段进一步强化了该纪律）。

---

## 6. 计数与口径复核（验收时实测）

| 面 | 验收时实测 | 与登记册 §0.1 一致 |
|---|---|:--:|
| `cep_*.sh` / `cep_*.py` / 闸族合计 | **51** / 14 / **65** | ✅（登记册已回填） |
| `test_cep_*.cc` | **78** | ✅（追加断言到**既有**文件 ⇒ 不新建测试文件） |
| 全仓主 spec / `cep-*` spec | **139** / **48** | ✅（零 delta ⇒ **不变**） |
| OpenSpec active | **1**（本 change 立项期） | ✅（KPI 活口径 1/3 · 7 处同步） |
| 归档 change 目录 / CEP 归档 | **171** / **55** | ✅（归档后 172 / 56） |
| M1 冻结清单 | **91** 产物 · hash 全一致 | ✅ **零变化** |

---

## 7. 判定

**PASS（无条件）** —— AC-1 ~ AC-6 **6/6 全部通过**，每条均有 ≥1 条工具证据；无取证不可用项；遗留清扫干净；表述纪律通过。

**归档前置条件**：✅ 满足（`skip_specs: true` ⇒ 归档**不产 main spec** ⇒ 冻结清单零变化 ⇒ **无需五步授权链**；KPI 活口径与登记册计数在归档后需同步回 0/3 与 172/56/65）。

**路由**：无失败项 ⇒ 无需回 builder。
