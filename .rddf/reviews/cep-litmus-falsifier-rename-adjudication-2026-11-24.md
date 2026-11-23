# 裁定记录 · **I2 阶段 2**（「litmus」工装改名 + 命名消歧闸缺陷修复）

> **日期**：2026-11-24 · **角色**：architect
> **对象**：CEP「litmus 证伪工装」→ **「复合图证伪工装」** 的全面改名（活代码面 + 活契约面）
> **关联**：I2 阶段 1 = change `cep-litmus-falsifier-naming-disambiguation`（已归档 · 台账 Entry #538）· 阶段 1 的 `design.md` 已预定新名 `compound_graph_falsifier`
> **审查轨迹**：**metis 1 轮**（gap 分析）→ **oracle 1 轮**（审查域 = 其他审查 · 判定 **APPROVE** · Q1~Q4 可判定裁决）→ **critic 1 轮**（review_profile = `plan, build` · 判定 **mergeable: no** · 1 BLOCKER + 3 SHOULD_FIX + 2 NITS）⇒ **BLOCKER 已闭合**（范围补入，见 §4）
> **改名的授权依据**：阶段 1 `design.md` 已登记「阶段 2 · 另行立项」；`ADR-CEP-07A-06bis-artifact-disposition.md` 确立仓内改名口径（「采用 / 改名 / 废弃」三选一 + **每项必须给理由**）

---

## 一、为什么要改名（G6 的最后一环）

| 事实 | 一手出处 |
|---|---|
| 本仓工装检验**装配期通道依赖图**上的**环存在性**（顶点 `(instance, network, vc)`） | `include/tlm/cep/litmus_falsifier.hh` |
| 论文侧 `herd7` **memory-model litmus** 检验**执行迹的允许结果**（运行期语义） | Oswald 博士论文 §8.7.2 / HeteroGen §VII |
| ⇒ **同名不同物** | 阶段 1 已在头文件/测试/spec 三处登记该消歧 |

阶段 1 完成了**声明面**（消歧常量 + 诚实边界段 + 禁用表述闸）；**活代码面与活契约面的文件名/符号名仍带「litmus」** ⇒ 读者仍可能据文件名**高估能力**（G6 的 C1 后果未闭合）。

**改名的边界（明确不做）**：⛔ 不改任何**已签批 ADR 正文**（历史字样是不可变历史，非误导源）· ⛔ 不改 `.rddf/**` 的历史叙述 · ⛔ 不改归档件 · ⛔ 零一致性判据语义变更 · ⛔ 零 invariant 编号变更 · ⛔ 零行为变更。

---

## 二、oracle 裁决（Q1~Q4 · 逐条可判定）

### Q1 改名范围（行级清单 · 封闭）

| # | 项 | 裁决 | 理由 |
|:--:|---|:--:|---|
| ① | 头文件名 ⇒ `compound_graph_falsifier.hh` | **必改** | 活代码面核心误导源；阶段 1 `design.md` 已预定 |
| ② | `LitmusVerdict` ⇒ `CompoundGraphVerdict` | **必改** | 同上（3 个符号之一） |
| ③ | `evaluate_litmus_falsification` ⇒ `evaluate_compound_graph_falsification` | **必改** | 同上 |
| ④ | 测试文件名 ⇒ `test_cep_compound_graph_falsification.cc` | **必改** | 活代码面；CMake `file(GLOB test_*.cc)` 自动拾取 |
| ⑤ | 既有闸文件名 ⇒ `cep_compound_graph_falsification_check.sh` | **必改** | 闸名描述**工装能力**，能力已改名则名字 stale；内部同改 tag 过滤串 / env var 名 / `CORPUS` 路径 |
| ⑥ | **命名消歧闸文件名** `cep_litmus_naming_check.sh` | **必不改** | 闸名命名的是它的**管辖违例类**（「litmus 能力冒称」= `litmus-scope-claim`），**不是**被守护工装的能力名；论文侧 litmus 是**正确名字且继续存在** ⇒ 混淆风险不随工装改名消失 |
| ⑦ | ctest 条目名 | **部分改** | 镜像脚本文件名：`cep_litmus_falsification_check` 随⑤改 · `cep_litmus_naming_check` **保留**（同⑥） |
| ⑧ | **capability 名 + spec 目录** ⇒ `cep-compound-graph-falsification` | **必改** | 契约面消歧是本次意图核心；冻结链走 C-1b（台账 Entry 须含**移出声明行**） |
| ⑨ | **红因码** `litmus-scope-claim` | **必不改** | 负空间违例类标识；阶段 1 归档 AC 证据与闸头均引用它 ⇒ 改名即断审计链 |
| ⑩ | 闸扫描集内 spec 路径 | **必改** | 事实 E 的直接要求；`HDR`/`TESTF` 两路径 + probe `#include` + sed 影子路径同改 |

**oracle 补入调用方清单漏列的 4 处**（漏掉任一处 ⇒ 残留误导字样或闸/测试断链）：

| # | 漏列项 | 改动 |
|:--:|---|---|
| ④b | **第三个符号** `load_litmus_corpus` | ⇒ `load_compound_graph_corpus`（消费者 = 测试文件 3 处） |
| ⑤b | env var `CPPTLM_LITMUS_CORPUS` / `CPPTLM_LITMUS_REPORT` | ⇒ `CPPTLM_COMPOUND_GRAPH_{CORPUS,REPORT}`（**生产/消费两侧同 commit**，否则闸断） |
| ⑤c | 语料文件 `test/cep_litmus_corpus.json` | ⇒ `test/cep_compound_graph_corpus.json`（3 处引用同改） |
| ④c | Catch2 tag 族 `[cep][litmus][falsification][*]` | ⇒ `[cep][compound-graph][falsification][*]`（闸内过滤串同改） |

### Q1 附：**必须保留「litmus」字样的活面**（防后续误改）

| 保留项 | 理由 |
|---|---|
| `kFalsifierIsMemoryModelLitmus` | 阶段 1 `design.md` 显式论证的**否定式命名**（负空间标记），改弱其义 |
| `kFalsifierScope` | 名/值均无 litmus |
| static_assert 签名 token `litmus-scope` | 闸 `:194` grep 它（红因码一致性） |
| 头文件/测试/spec 内一切**带否定标记**的消歧句 | 闸的 claim pattern **要求它们存在** |
| **V3 barrier 分类 litmus 家族**（`collective_plane.hh` 16 处 · `nvlink_adapter.hh` · `cep-nvlink-sharp/spec.md` · `test_cep_nvlink_sharp*.cc` · `docs/architecture/07:175`） | **不同义项**（判别性探针，`ADR-CEP-01 §3.1` 冻结锚定）⇒ **范围外** |
| `openspec/specs/cep-core-contracts/spec.md:273`「litmus 语料」 | 冻结面；且机检③要求 manifest diff **恰 2 项** ⇒ 动它即第 3 项 |
| `scripts/test/rddf_ac_carrier.tsv:68` · `AGENTS.md:313` | 历史流水 |

### Q2 命名消歧闸的存续形态

**保留** `cep_litmus_naming_check.sh`（本仓**唯一**「禁止把 memory-model litmus 指涉本仓工装」的机器判据 ⇒ 废弃即失防，无替代）。
**必改**：扫描集 3 路径 + probe include + 闸头注释 + `scripts/README.md` 守门表行描述（改为「守护**已改名**的复合图证伪工装」）。
**必留**：文件名 · 红因码 · claim patterns · 否定词表（**不含「区别」**）· SKIP 语义。

### Q3 新 spec 的四类措辞边界

| 类 | 内容 |
|---|---|
| **逐字保留** | 「只做**证伪尝试**」·「⛔ **不判定公理成立**」(per `ADR-CEP-13` 决策 5 ②)·「**不可证伪**」是**真结果**、⛔ 不是「公理已成立」· 三形态（证伪/未证伪/不可证伪）· 覆盖边界 `coverage_note` ·「⛔ SHALL NOT 表述为『已证明』」· 4 Scenario 的结构与全部否定句 · 来源行（历史 change 名 + Entry #513，历史不可变） |
| **必须改** | 「litmus 证伪工装」→「复合图证伪工装」·「litmus 语料」→「证伪语料（复合图语料）」· Requirement 名 ⇒ `cep-compound-graph-falsification` |
| **必须增** | Purpose 内一行**改名沿革**（per `ADR-CEP-07A` 改名须给理由）：由 `cep-litmus-falsification` 改名 · 理由（与 herd7 同名不同物）· **判定语义零变更** · per 台账 Entry #N |
| **⛔ 不得引入** | 任何新 capability 声言（边类语义 / 新 Scenario / 操作级判据 /「验证通过」类措辞） |

### Q4 闸缺陷修复的归属

**(a) 并入本 change** —— ① 改名**直接触发**该缺陷（spec 路径一变、不改扫描集 ⇒ 闸永久假绿；事实 E 实测的正是这个机理）② 修复面 ≈10 行，与改名共用同一验收批次 ③ 单独立项留下「改名先落地、牙后补」的**失检窗口期**。
**验收口径**：SKIP **仅**保留「HDR 不存在 ⇒ 整闸 SKIP」（消歧主体未落地是合法前置态）；「HDR 在而 TESTF/SPEC 任一缺失 ⇒ **FAIL**」（红因 `litmus-scan-set-missing`）；臂②新增第④路影子 fixture（扫描集缺一成员 ⇒ 必须红）。

---

## 三、critic 裁决（BLOCKER 已闭合）

| 项 | 裁决 | 处置 |
|---|---|---|
| **BLOCKER** | `test/test_cep_combination_contract.cc:312` 硬编码 `test/cep_litmus_corpus.json`（`load_json` 对缺失文件 `REQUIRE_FALSE(text.empty())`）⇒ **语料改名后 `INV-CEP-13-IC1` 的判定级载体变红**，而该文件不在拟改动清单 ⇒ 「不破坏」结论错误 + 违反「零行为变更」 | ✅ **已闭合**：该文件（**仅 :312 单行等长替换**）**补入 change 范围 + tasks**；⚠️ 必须**单行等长替换**（只改路径字符串）⇒ 否则 `test/cep_invariant_assertion_map.json` 记录的载体行号漂移 |
| SHOULD_FIX ① | 保留侧（⑥⑦⑨）须显式登记，否则后续「零误导清扫」会据同名 intent 误改 | ✅ 落 §二「Q1 附：必须保留」表 + proposal「必不改」表 + 闸头注释 |
| SHOULD_FIX ② | `ADR-CEP-12:1163`（**已签批 ADR 正文**）引用 `test/cep_litmus_corpus.json` ⇒ 改名后悬空；ADR 正文不可改 | ✅ 登记为**后果面**（proposal 后果段）；⛔ 本 change **不改** ADR 正文（若走 SU 追加须单独授权） |
| SHOULD_FIX ③ | `.rddf/improvements/cep-memory-model-operational-trace.md` 的 6 处旧路径引用（含 AC-12 的 `grep -c … openspec/specs/cep-litmus-falsification/spec.md == 0` 判据）⇒ 改名后该草稿的多条 AC 判据失效 | ✅ **planner 域**修订（该文件属 planner 写集）⇒ 与本次改名**同批** |
| SHOULD_FIX ④ | 时序钉：spec 改名 / 闸扫描集 / 语料改名 / `test_cep_combination_contract.cc:312` **同批 commit**；并加「归档后**无残留空壳** spec」验证项 | ✅ 写入 tasks（见 §四） |
| NIT ① | `.rddf/roadmap/cep-gen-family-gap-closure.md:42`（F5 真值表）引用旧头文件 + 旧语料 ⇒ 历史 roadmap 悬空 | ✅ 登记（历史文档，不改） |
| NIT ② | `test/CMakeLists.txt:802` 注释行仍写「litmus 证伪工装闸」 | ✅ 同批归入「必须改」类 |

**critic 的时序硬约束（源自闸缺陷修复本身）**：修复后「HDR 在而 TESTF/SPEC 任一缺失 ⇒ FAIL」与扫描集指向新 spec 路径组合 ⇒ **spec 改名、闸扫描集改路径、语料改名必须同一 commit 批次落地**；若闸先指向新 spec 而文件未建，新 FAIL 逻辑在归档前全程红。

**critic 的冻结链形态提醒**：`artifact_count` 不变 + 清单 diff 恰 2 项的前提是**旧 spec 文件物理删除**（git mv）⇒ 若残留空壳 `spec.md`，glob `cep-*/spec.md` 仍会收它 ⇒ 归档时再触发一次 hash-changed 型更新。

---

## 四、最终裁定（实施口径 · 冻结）

### 4.1 改动面（**恰 13 个文件**，冻结面 **2** 个）

| # | 文件 | 动作 | 冻结面 |
|:--:|---|---|:--:|
| 1 | `include/tlm/cep/litmus_falsifier.hh` ⇒ `include/tlm/cep/compound_graph_falsifier.hh` | git mv + 3 符号改名 + include guard 改名 + 消歧段**逐字保留** | 否 |
| 2 | `test/test_cep_litmus_falsification.cc` ⇒ `test/test_cep_compound_graph_falsification.cc` | git mv + tag 族改名 + 符号更新 | 否 |
| 3 | `test/cep_litmus_corpus.json` ⇒ `test/cep_compound_graph_corpus.json` | git mv | 否 |
| 4 | `scripts/test/cep_litmus_falsification_check.sh` ⇒ `scripts/test/cep_compound_graph_falsification_check.sh` | git mv + env var / tag 过滤 / `CORPUS` 路径 + 闸头 | 否 |
| 5 | `scripts/test/cep_litmus_naming_check.sh` | **文件名保留** + 扫描集 3 路径 + probe include + **缺失即 FAIL 修复** + 第④路 fixture + 闸头保留理由 | 否 |
| 6 | **`test/test_cep_combination_contract.cc`** | **:312 单行等长替换**（语料路径）⇒ **critic BLOCKER 的闭合项** | 否 |
| 7 | `test/CMakeLists.txt` | 2 个 add_test 条目 + `:802` 注释措辞 | 否 |
| 8 | `scripts/README.md` | 守门表 1 行 + §1 枚举 | 否 |
| 9 | **`openspec/specs/cep-litmus-falsification/spec.md` ⇒ `openspec/specs/cep-compound-graph-falsification/spec.md`** | git mv + Requirement 名 + Purpose/Scenario 按四类边界改写 + 改名沿革行 | **是** |
| 10 | `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json` | 由 `--update` 自动更新（移出 1 + 新增 1 ⇒ 91 不变） | **是** |
| 11 | `.rddf/roadmap/objectives/obj-cep-platform.md` | 台账 Entry + **冻结面移出声明行** | 否 |
| 12 | `.rddf/improvements/cep-memory-model-operational-trace.md` | 6 处旧路径引用修订（**planner 域**，SHOULD_FIX ③） | 否 |
| 13 | `openspec/changes/cep-litmus-falsifier-compound-graph-rename/**` | proposal / design / tasks + delta（REMOVED 旧 capability + ADDED 新 capability） | 否（归档件不入库） |

### 4.2 时序钉（critic SHOULD_FIX ④ · 不可拆）

```
① builder commit A：闸缺陷修复（缺失即 FAIL + 第④路 fixture）—— 可独立成 commit
② builder commit B：改名同批（①②③④⑤⑥⑦⑧⑨⑩⑪⑬ 全部落地）
③ builder commit C：冻结清单单独提交（--update --authorized-by "Entry #N"）
   ⇒ 输出须为「新增 1 · 移出 1 · hash 变更 0」· artifact_count 91 → 91
④ builder：无参复验 cep_freeze_check.sh（rc=0）
⑤ verifier：本地验收（机检 ①②③④ + 人工 ①②）
⑥ builder：openspec archive（delta REMOVED/ADDED）⇒ 若合并结果与 commit B 的文件
   **逐字不一致** ⇒ 需再 --update 一次（同一 Entry 授权）
⑦ builder：归档后验证「openspec/specs/cep-litmus-falsification/ 无残留空壳」
```

### 4.3 判据（4 机检 + 2 人工）

| # | 判据 |
|:--:|---|
| 机检 ① | **改名完整性**：`grep -rn "cep_litmus\|litmus_falsifier" include/ test/ scripts/ openspec/specs/` ⇒ 命中数 == §二「Q1 附：必须保留」表的**例外项**（命名消歧闸文件名 + 红因码 + `kFalsifierIsMemoryModelLitmus` + static_assert token + V3 家族 + `cep-core-contracts`）⇒ 其余**零命中** |
| 机检 ② | **零行为变更**：改名后 `[cep][compound-graph][falsification]` 标签族全绿 + 既有闸两条臂（正控 + 反向 fixture）rc=0 + `test_cep_combination_contract` 绿 |
| 机检 ③ | **冻结链**：清单 diff 恰 2 项（移出旧 spec 路径 + 新增新 spec 路径）· `artifact_count` 91 → 91 |
| 机检 ④ | **闸缺陷**：扫描集任一文件缺失 ⇒ 闸**必红**（红因 `litmus-scan-set-missing`）⇒ 反向 fixture 实证 |
| 人工 ① | **口径诚实**：改名后活代码面/契约面**零**能力高估表述；三处诚实边界**逐字在场** |
| 人工 ② | **不变量对账**：`INV-CEP-13-IC1` / `INV-CEP-12-IC1` 载体**未被破坏**（含 critic BLOCKER 的 `:312` 单行替换 ⇒ 载体仍绿） |

### 4.4 明确不做

⛔ 不改任何已签批 ADR 正文（`ADR-CEP-12:1163` 的悬空引用**只登记**）· ⛔ 不删任何 invariant · ⛔ 不改判据语义 · ⛔ 不动 V3 litmus 家族 · ⛔ 不动 `cep-core-contracts/spec.md` · ⛔ 不立项 G2（`cep-memory-model-operational-trace` 的**实施**另立，本轮只修其路径引用）。

---

## 五、后续（不在本裁定范围）

| # | 项 | 归属 |
|:--:|---|---|
| 1 | **G2 实施**：`cep-memory-model-operational-trace` ⇒ 开工前置 = **architect 裁「每域模型来源 + 域间边构造规则」**（新建 ADR）→ **用户 ratify** → planner 立 change → builder 实施 | 待 architect + 用户 |
| 2 | **G2 阶段 1 已由本裁定铺路**：本仓活代码面不再有「litmus 工装」⇒ G2 的操作级模型直接落在 `compound_graph_falsifier` 名下 ⇒ **无二次改名成本** | — |
| 3 | `ADR-CEP-12:1163` 的悬空引用 ⇒ 若需消解，走 **Status Update 追加**（须单独授权 + 五步链） | 后续 |
