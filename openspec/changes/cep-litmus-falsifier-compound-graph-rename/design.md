# design: cep-litmus-falsifier-compound-graph-rename —— 改名实施设计

> **来源**：`proposal.md`（同目录）· **裁定**：`.rddf/reviews/cep-litmus-falsifier-rename-adjudication-2026-11-24.md`
> **范围纪律**：⛔ 零决策变更 · 零判据语义变更 · 零 invariant 编号变更 · 零行为变更 · ⛔ 不改任何已签批 ADR 正文 · ⛔ 不动 V3 litmus 家族 · ⛔ 不动 `cep-core-contracts/spec.md`

---

## D-1 ~ D-6：关键实现决策

### D-1 归档形态 = **实施期 `git mv` + 完整 delta + `retire_capabilities: true`**

⚠️ openspec 的两个**实测**行为（探针验证，见 §附录）：

| 探针 | 结果 |
|---|---|
| delta 含 REMOVED(旧) + ADDED(新)，**实施期不动** main spec | 归档时旧 capability「最后一个 requirement 被 REMOVED」⇒ **openspec 拒绝归档**（`Aborted. No files were changed.`）⇒ 须 `retire_capabilities: true`；归档后新 spec 的 Purpose = **占位**（"TBD - created by archiving…"）⇒ 须**归档后补写** ⇒ 冻结链 **2 次** |
| delta 含 REMOVED(旧) + ADDED(新)，**实施期已 `git mv`** | 归档时旧路径不存在 ⇒ REMOVED 被 openspec **忽略**（警告，无害）；新 spec 已存在且内容一致 ⇒ **零改动**（"Specs already in sync"）⇒ 冻结清单**实施期更新 1 次**即可，且新 spec 的 Purpose **实施期即真实**（⛔ 非占位） |

⇒ 选**后者**：实施期 `git mv` + 改写 + 完整 delta。⚠️ 代价：delta 的 REMOVED 段会被 openspec 忽略并**打一条警告**（功能无害）。

### D-2 delta 的 ADDED 段**从实施期新 spec 提取生成**（单一来源）

⚠️ 风险 = delta 的 ADDED 措辞与实施期新 spec 漂移 ⇒ 归档时 openspec 会**追加/覆盖** ⇒ 产生非预期 diff。
⇒ **机械保证**：实施脚本从新 spec 提取 `### Requirement:` 段（含 4 个 Scenario），加 `## ADDED Requirements` 头 ⇒ 写入 delta。REMOVED 段同理从 `git show HEAD:<旧 spec>` 提取。

### D-3 `test/test_cep_combination_contract.cc:312` 的**单行等长替换**

⚠️ 该行是 `INV-CEP-13-IC1` 的**判定级载体**（硬编码语料路径 + `load_json` 对缺失文件 `REQUIRE_FALSE(text.empty())`）⇒ 语料改名后**必红** ⇒ 必须同批改。
⚠️ **必须单行等长替换**（只改路径字符串）⇒ 否则 `test/cep_invariant_assertion_map.json` 记录的载体**行号漂移** ⇒ 该 map 是冻结面？⇒ **不在** M1 清单（`test/` 不在）⇒ 可改，但**改 map** 需重生成 ⇒ 本次**不改 map** ⇒ 故**必须等长**。

### D-4 命名消歧闸的缺陷修复（**并入本 change**）

⚠️ 实测缺陷：扫描实现 `[ -f "$f" ] || continue` ⇒ 扫描集缺失 ⇒ **静默跳过 ⇒ 假绿**（实测：spec 路径改成不存在 ⇒ 闸仍 RC=0「全部判据通过」）。

| 情形 | 修复前 | 修复后 |
|---|---|---|
| HDR（被消歧主体）不存在 | 整闸 SKIP | **保持 SKIP**（消歧主体未落地是合法前置态） |
| HDR 在 · TESTF 或 SPEC 缺失 | **假绿** ❌ | **FAIL**（红因 `litmus-scan-set-missing`）❌→✅ |
| 扫描集齐全 | 绿 | 绿 |

⇒ 第④路影子 fixture：扫描集缺一成员 ⇒ 必须红（机检④ 的实证）。

### D-5 闸文件名与红因码**保留**（审计链）

命名消歧闸的**管辖违例类**是「litmus 能力冒称」⇒ 与工件名无关 ⇒ 改名后其使命不消失（读者仍可能把 `compound_graph_falsifier` 错述为 memory-model litmus）⇒ 它是本仓**唯一**该边界的机器判据。
⚠️ 红因码 `litmus-scope-claim` 被阶段 1 的归档 change（AC-2/AC-3 证据 + 验收报告）**逐字引用** ⇒ 改名即断**审计链** ⇒ 保留。
⇒ 故「必不改」登记（proposal 表 + 闸头注释）**必须落盘**，防后续「零误导清扫」据同名 intent 误改。

### D-6 时序钉（critic 硬约束 · 不可拆）

```
commit A  闸缺陷修复（缺失即 FAIL + 第④路 fixture）        ← 可独立
commit B  改名同批：spec git mv + 闸扫描集 3 路径 + probe include
                 + 语料 git mv + 既有闸 git mv + env var 两侧
                 + tag 族 + test_cep_combination_contract.cc:312 单行
                 + ctest 2 条目 + README 守门表 + 台账 Entry + 路径文档
commit C  冻结清单单独提交（--update --authorized-by "Entry #N"）
                 ⇒ 输出须为「新增 1 · 移出 1 · hash 变更 0」· artifact_count 91 → 91
commit D  无参复验 cep_freeze_check.sh（rc=0）
commit E  归档 openspec archive（delta REMOVED/ADDED ⇒ 零 spec 改动）
                 + 归档后验证「openspec/specs/cep-litmus-falsification/ 无残留空壳」
commit F  计数 / KPI 回填 + 台账 Entry 收口 + 审稿/验收报告
```

⚠️ **闸扫描集与 spec 改名必须同批（commit B）** —— 否则「新 FAIL 逻辑 + 指向未建的新 spec」⇒ 归档前全程红（critic 的硬约束）。

---

## 改动面（恰 13 文件 · 冻结面 2）

| # | 文件 | 动作 |
|:--:|---|---|
| 1 | `include/tlm/cep/litmus_falsifier.hh` ⇒ `compound_graph_falsifier.hh` | git mv + 3 符号 + include guard + 消歧段**逐字保留** |
| 2 | `test/test_cep_litmus_falsification.cc` ⇒ `test_cep_compound_graph_falsification.cc` | git mv + tag 族 + 符号 |
| 3 | `test/cep_litmus_corpus.json` ⇒ `test/cep_compound_graph_corpus.json` | git mv |
| 4 | `scripts/test/cep_litmus_falsification_check.sh` ⇒ `cep_compound_graph_falsification_check.sh` | git mv + env var / tag 过滤 / `CORPUS` 路径 |
| 5 | `scripts/test/cep_litmus_naming_check.sh` | **文件名保留** + 扫描集 3 路径 + probe include + 缺失即 FAIL + 第④路 fixture + 闸头保留理由 |
| 6 | **`test/test_cep_combination_contract.cc`** | **:312 单行等长替换**（critic BLOCKER 闭合项） |
| 7 | `test/CMakeLists.txt` | 2 个 add_test + `:802` 注释措辞 |
| 8 | `scripts/README.md` | 守门表 1 行 + §1 枚举 |
| 9 | **`openspec/specs/cep-litmus-falsification/spec.md` ⇒ `openspec/specs/cep-compound-graph-falsification/spec.md`** | git mv + Requirement 名 + Purpose/Scenario 四类边界 + 改名沿革行（**含真实 Purpose**） |
| 10 | `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json` | 由 `--update` 自动更新（移出 1 + 新增 1 ⇒ 91 不变） |
| 11 | `.rddf/roadmap/objectives/obj-cep-platform.md` | 台账 Entry + **冻结面移出声明行** |
| 12 | `.rddf/improvements/cep-memory-model-operational-trace.md` | 6 处旧路径引用修订（**planner 域**，SHOULD_FIX ③） |
| 13 | `openspec/changes/cep-litmus-falsifier-compound-graph-rename/**` | proposal / design / tasks + delta |

**零改动（硬查）**：任何 `docs/adr/*.md` 正文 · `docs/architecture/**` · `include/tlm/cep/protocol_instance*.json` · `test/cep_invariant_assertion_map.json` · `openspec/specs/cep-core-contracts/spec.md` · V3 litmus 家族。

---

## 判据（4 机检 + 2 人工）

| # | 判据 |
|:--:|---|
| 机检 ① | **改名完整性**：`grep -rn "cep_litmus\|litmus_falsifier" include/ test/ scripts/ openspec/specs/` 命中项 ⊆ 「必不改」登记表（命名消歧闸文件名 + 红因码 + `kFalsifierIsMemoryModelLitmus` + static_assert token + V3 家族 + `cep-core-contracts`）⇒ 其余**零命中** |
| 机检 ② | **零行为变更**：改名后 `[cep][compound-graph][falsification]` 全绿 + 既有闸两臂 rc=0 + `test_cep_combination_contract` 绿 |
| 机检 ③ | **冻结链**：清单 diff 恰 2 项 · `artifact_count` 91 → 91 · 无参复验 rc=0 |
| 机检 ④ | **闸缺陷**：扫描集缺一成员 ⇒ 闸必红（`litmus-scan-set-missing`）⇒ 反 fixture 实证 |
| 人工 ① | **口径诚实**：零能力高估表述；三处诚实边界逐字在场 |
| 人工 ② | **不变量对账**：`INV-CEP-13-IC1` / `INV-CEP-12-IC1` 载体未被破坏 |

---

## 附录：openspec 行为探针（2026-11-24 实测）

| 探针 | 形态 | 结果 |
|:--:|---|---|
| P-1 | delta = REMOVED(旧) + ADDED(新)；旧 spec 存在；**无** `retire_capabilities` | ❌ 归档**中止**：`Validation errors in rebuilt spec for old-cap`（"Spec must have at least one requirement"）⇒ `Aborted. No files were changed.` |
| P-2 | 同 P-1 + `retire_capabilities: true` | ✅ 归档成功 ⇒ 旧 spec **删除**（**无残留空壳**）· 新 spec 创建但 Purpose = **占位** |
| P-3 | delta = REMOVED(旧) + ADDED(新)；旧 spec **不存在**（实施期已 mv）· 新 spec 已存在 | ✅ 归档成功 ⇒ `old-cap: 1 REMOVED requirement(s) ignored`（**警告**，无害）· `new-cap: update` ⇒ `Totals: + 0, ~ 0, - 0` ⇒ **零改动**（"Specs already in sync"）⇒ **实施期写真的 Purpose 不被占位覆盖** |
| P-4 | 同 P-3，`openspec validate --strict` | ✅ `Change 'rename-test2' is valid` |

⇒ 结论见 D-1。
