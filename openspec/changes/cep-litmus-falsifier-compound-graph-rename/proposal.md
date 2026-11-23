# proposal: cep-litmus-falsifier-compound-graph-rename —— 「litmus 证伪工装」⇒「复合图证伪工装」改名

> **来源**：裁定记录 `.rddf/reviews/cep-litmus-falsifier-rename-adjudication-2026-11-24.md`（architect · oracle 1 轮 APPROVE + critic 1 轮 mergeable:no→BLOCKER 已闭合）
> **上游**：`.rddf/improvements/cep-litmus-falsifier-naming-disambiguation.md`（I2 · **阶段 2**）· `.rddf/roadmap/cep-gen-family-gap-closure.md`（gap 分析 G6）
> **定位**：消除「据文件名/符号名**高估能力**」的最后一道口子。⛔ **零决策变更** · 零判据语义变更 · 零 invariant 编号变更 · 零行为变更。

## Why

| 事实 | 一手出处 |
|---|---|
| 本仓工装检验**装配期通道依赖图**上的**环存在性**（顶点 `(instance, network, vc)`） | `include/tlm/cep/litmus_falsifier.hh` |
| 论文侧 `herd7` **memory-model litmus** 检验**执行迹的允许结果**（运行期语义） | Oswald 博士论文 §8.7.2 / HeteroGen §VII |
| ⇒ **同名不同物** | 阶段 1 已在头文件 / 测试 / spec 三处登记该消歧 |

阶段 1（已归档）完成**声明面**消歧（消歧常量 + 诚实边界段 + 禁用表述闸），但**活代码面与活契约面的文件名/符号名仍带「litmus」** ⇒ 读者仍可能据文件名**高估能力**（G6 的 C1 后果未闭合）。

**改名口径**（per `ADR-CEP-07A-06bis-artifact-disposition.md`：改名须「给理由」）：
- 理由 = 与论文侧 `herd7` memory-model litmus **同名不同物** ⇒ 文件名/符号名构成**能力高估**风险源
- 代价 = 活代码面/活契约面符号更名（**零语义**）· 冻结面 1 处（spec 退役 + 新增）· 命名消歧闸**保留原名**（其管辖违例类不随工件改名过期）
- **判定语义零变更** —— 4 个 Scenario 的结构与全部否定句**逐字保留**

## What Changes

| # | 变更 | 形态 / 约束 |
|:--:|---|---|
| 1 | **活代码面改名** | `include/tlm/cep/litmus_falsifier.hh` ⇒ `include/tlm/cep/compound_graph_falsifier.hh`（include guard 同步）· `LitmusVerdict` ⇒ `CompoundGraphVerdict` · `evaluate_litmus_falsification` ⇒ `evaluate_compound_graph_falsification` · `load_litmus_corpus` ⇒ `load_compound_graph_corpus` |
| 2 | **测试/语料/闸改名** | `test/test_cep_litmus_falsification.cc` ⇒ `test/test_cep_compound_graph_falsification.cc`（Catch2 tag 族 `[cep][litmus][falsification][*]` ⇒ `[cep][compound-graph][falsification][*]`）· `test/cep_litmus_corpus.json` ⇒ `test/cep_compound_graph_corpus.json` · `scripts/test/cep_litmus_falsification_check.sh` ⇒ `scripts/test/cep_compound_graph_falsification_check.sh`（env var `CPPTLM_LITMUS_{CORPUS,REPORT}` ⇒ `CPPTLM_COMPOUND_GRAPH_{CORPUS,REPORT}`，**生产/消费两侧同批**） |
| 3 | **命名消歧闸：文件名保留 + 修判据缺陷** | `scripts/test/cep_litmus_naming_check.sh` —— 扫描集 3 路径同步 + probe include 同步 + **「扫描集缺失 ⇒ 静默跳过 ⇒ 假绿」缺陷修复**（HDR 在而 TESTF/SPEC 缺失 ⇒ **FAIL**，红因 `litmus-scan-set-missing`）+ 第④路影子 fixture；⛔ **文件名与红因码 `litmus-scope-claim` 保留**（见「必不改」表） |
| 4 | **契约面退役 + 新增**（冻结面） | `openspec/specs/cep-litmus-falsification/spec.md` ⇒ `openspec/specs/cep-compound-graph-falsification/spec.md`（Requirement 名同改）· Purpose 按「四类措辞边界」改写（见下）· `.openspec.yaml` 声明 `retire_capabilities: true` |
| 5 | **invariant 载体对账**（critic BLOCKER 闭合项） | `test/test_cep_combination_contract.cc:312` 的语料路径**单行等长替换**（只改路径字符串 ⇒ 保持 `test/cep_invariant_assertion_map.json` 记录的载体行号有效） |

### 「必不改」登记（防后续「零误导清扫」误改）

| 项 | 理由 |
|---|---|
| 命名消歧闸**文件名** `cep_litmus_naming_check.sh` | 闸名命名的是它的**管辖违例类**（「litmus 能力冒称」），**不是**被守护工装的能力名；论文侧 litmus 是**正确名字且继续存在** ⇒ 混淆风险不随工件改名消失 ⇒ 它是本仓**唯一**该边界的机器判据，废弃即失防 |
| 红因码 `litmus-scope-claim` | 负空间违例类标识；阶段 1 归档 change 的 AC 证据与闸头均引用它 ⇒ 改名即断**审计链** |
| `kFalsifierIsMemoryModelLitmus` | 阶段 1 `design.md` 显式论证的**否定式命名**（负空间标记），改弱其义 |
| static_assert 签名 token `litmus-scope` | 闸内 grep 它（红因码一致性） |
| 头文件/测试/spec 内一切**带否定标记**的消歧句 | 闸的 claim pattern **要求它们存在** |
| **V3 barrier 分类 litmus 家族**（`collective_plane.hh` 16 处 · `nvlink_adapter.hh` · `cep-nvlink-sharp/spec.md` · `test_cep_nvlink_sharp*.cc` · `docs/architecture/07:175`） | **不同义项**（判别性探针，`ADR-CEP-01 §3.1` 冻结锚定）⇒ **范围外** |
| `openspec/specs/cep-core-contracts/spec.md:273`「litmus 语料」 | 冻结面；且冻结清单 diff 须**恰 2 项** ⇒ 动它即第 3 项 |
| 已签批 ADR 正文的历史字样 · `.rddf/**` 历史叙述 · 归档件 | **不可变历史**，非误导源 |

### 新 spec 的四类措辞边界

| 类 | 内容 |
|---|---|
| **逐字保留** | 「只做**证伪尝试**」·「⛔ **不判定公理成立**」(per `ADR-CEP-13` 决策 5 ②)·「**不可证伪**」是**真结果**、⛔ 不是「公理已成立」· 三形态（证伪/未证伪/不可证伪）· 覆盖边界 `coverage_note` ·「⛔ SHALL NOT 表述为『已证明』」· 4 Scenario 的结构与全部否定句 · 来源行（历史 change 名 + Entry #513，历史不可变） |
| **必须改** | 「litmus 证伪工装」→「复合图证伪工装」·「litmus 语料」→「证伪语料（复合图语料）」· Requirement 名 ⇒ `cep-compound-graph-falsification` |
| **必须增** | Purpose 内一行**改名沿革**（per `ADR-CEP-07A` 改名须给理由）：由 `cep-litmus-falsification` 改名 · 理由（与 herd7 同名不同物）· **判定语义零变更** · per 台账 Entry #N |
| **⛔ 不得引入** | 任何新 capability 声言（边类语义 / 新 Scenario / 操作级判据 /「验证通过」类措辞） |

## 验收标准

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。

- [ ] **AC-1（改名完整性 · 机检）**：`grep -rn "cep_litmus\|litmus_falsifier" include/ test/ scripts/ openspec/specs/` 的命中项 **全部**落在「必不改」登记表内 ⇒ 其余**零命中**
- [ ] **AC-2（零行为变更 · 机检）**：改名后 `[cep][compound-graph][falsification]` 标签族全绿 + 既有闸两条臂（正控 + 反向 fixture）rc=0 + `test_cep_combination_contract` 绿（invariant 载体未被破坏）
- [ ] **AC-3（冻结面链 · 机检）**：冻结清单 diff **恰 2 项**（移出 `openspec/specs/cep-litmus-falsification/spec.md` + 新增 `openspec/specs/cep-compound-graph-falsification/spec.md`）· `artifact_count` **91 → 91** 不变 · `cep_freeze_check.sh` 无参 rc=0
- [ ] **AC-4（闸缺陷修复 · 机检 + 反 fixture）**：扫描集任一文件缺失 ⇒ 闸**必红**（红因 `litmus-scan-set-missing`）⇒ 反向 fixture 实证；修复前该形态**假绿**（已实测）
- [ ] **AC-5（口径诚实 · 人工评审）**：改名后活代码面/契约面**零**能力高估表述；三处诚实边界（「不可证伪」= 真结果 ·「⛔ 不判定公理成立」·「⛔ 不表述为已证明」）**逐字在场**
- [ ] **AC-6（零回归 · 机检）**：`ctest -R cep` 全绿（⛔ 不写死断言数）· 全部 `scripts/test/cep_*.sh` rc=0 · `cep_docs_sync_check.sh --strict` rc=0

## Capabilities

| capability | 关系 | 说明 |
|---|---|---|
| `cep-litmus-falsification` | **退役**（REMOVED 最后一个 requirement ⇒ openspec 需 `retire_capabilities: true`） | 改名后的同型能力 |
| `cep-compound-graph-falsification` | **新增**（ADDED） | **判定语义与旧能力逐字等价**（4 Scenario 结构与否定句保留） |

## Impact

| 面 | 影响 |
|---|---|
| **冻结面** | **1 处**（旧 spec 移出 + 新 spec 新增 ⇒ 清单 diff 恰 2 项 · `artifact_count` 91 → 91）⇒ **五步链 1 次** |
| **已签批 ADR 正文** | **零改动**；⚠️ `ADR-CEP-12:1163` 引用旧语料路径 ⇒ 改名后**悬空**（登记为后果面；⛔ 本 change 不改 ADR 正文） |
| **零改动（硬查）** | 任何 `docs/adr/*.md` 正文 · `docs/architecture/**` · `include/tlm/cep/protocol_instance.schema.json` · `include/tlm/cep/protocol_instances.json` · `test/cep_invariant_assertion_map.json` · `openspec/specs/cep-core-contracts/spec.md` · V3 litmus 家族 |
| **闸** | 新增 1 个（命名消歧闸的缺陷修复与第④路 fixture）· 闸文件名 1 改 1 留 · `cep_*.sh` 计数 **50 → 51**（新增）+ 改名不计增减 ⇒ **51** |
| **测试** | `test_cep_*.cc` 计数 **78 不变**（改名，非新增） |
| **KPI** | **doc-only 形态**（零函数签名/逻辑/构建改动）⇒ **不占** OpenSpec Proposed slot；但本 change **有**冻结面 spec 变更 ⇒ 需立项（走 openspec change）⇒ 实施期占 1 slot（活口径 1/3），归档后释放回 0/3 |
| **时序钉（不可拆）** | spec 改名 · 闸扫描集改路径 · 语料改名 · `test_cep_combination_contract.cc:312` 单行替换 ⇒ **同批 commit**；闸缺陷修复可独立成前置 commit；归档后验证「`openspec/specs/cep-litmus-falsification/` **无残留空壳**」 |
| **风险** | ① 闸扫描集与 spec 改名不同批 ⇒ 新 FAIL 逻辑在归档前全程红 ⇒ 缓解：同批 commit ② `test_cep_combination_contract.cc:312` 漏改 ⇒ `INV-CEP-13-IC1` 载体变红 ⇒ 缓解：AC-2 覆盖 ③ delta 与实施期新 spec 措辞漂移 ⇒ 缓解：delta 的 ADDED 段**从实施期新 spec 提取**生成（单一来源） |

## 关联

裁定记录 `.rddf/reviews/cep-litmus-falsifier-rename-adjudication-2026-11-24.md` · I2 阶段 1 `.rddf/improvements/cep-litmus-falsifier-naming-disambiguation.md` + 归档 change `2026-11-23-cep-litmus-falsifier-naming-disambiguation` · 实施路径 `.rddf/roadmap/cep-gen-family-gap-closure.md` · `ADR-CEP-13`（决策 2 判据 / 决策 5 ②）· `ADR-CEP-17 §2 Decision 4` · 台账 Entry #538 / #539
