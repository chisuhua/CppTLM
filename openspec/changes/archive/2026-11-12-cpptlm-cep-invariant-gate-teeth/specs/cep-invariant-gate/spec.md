# cep-invariant-gate Specification (delta)

## Purpose

CEP 治理线的**机制机器化**能力:把 `ADR-CEP-00` SU 2026-11-11 §⑤ 自认的两项「有登记、无牙齿」残留
—— **R-c**(「一决策一文档化」残余登记 + 3 条触发条件**无机器检查**)与 **R-d**(决策块计数口径
**三个互不相等的实现**)—— 落为**有牙齿的机器判据**,使「残留被静默删除」「口径被漂移」两类
**静默失效**在提交前即报红。

判据由**单一闸**承载(`scripts/test/cep_decision_block_check.{sh,py}`),含两个**独立段**:
**R-d 段**(决策块计数快照;期望值 = **闸外快照**)与 **R-c 段**(双向对账;期望值 = **闸内源码常量**)。
两段**互不耦合**(R-c ⛔ 不用决策块数);每条判据均配**决定性变异**(破坏 ⇒ 按**固定签名**红 ⇒ 还原 ⇒ 绿),
且影子树机制由**共享 helper**(`scripts/test/cep_test_shadow.sh`)承载 ⇒ ⛔ 不存在第二份实现。

> **演进方式**:本 spec 经由 OpenSpec delta(**ADDED / MODIFIED / REMOVED / RENAMED**)演进,**不直接改写**本文件
> **归档前形态**(被评审与冻结的对象):`openspec/changes/archive/2026-11-12-cpptlm-cep-invariant-gate-teeth/specs/cep-invariant-gate/spec.md`
> **规范载体**:决策块计数口径 = `docs/adr/ADR-CEP-00-platform-charter-and-governance.md` 的 `## Status Update 2026-11-12`;
> **canonical 实现** = 本闸的 Python 实现(`ADR-CEP-00:431` 的「标题口径 4」= 其文档镜像)

## ADDED Requirements

### Requirement: cep-invariant-gate-decision-block-and-residual-check

CEP SHALL 提供**机器化治理判据**，把 `ADR-CEP-00` SU 2026-11-11 §⑤ 自认的两项「有登记、无牙齿」残留
—— **R-c**（「一决策一文档化」残余登记 + 3 条触发条件**无机器检查**）与 **R-d**（决策块计数口径
**三个互不相等的实现**）—— 落为**有牙齿的判据**。

判据 SHALL 由**单一新闸**承载（`scripts/test/cep_decision_block_check.{sh,py}`），其 SHALL 包含两个
**独立段**：**R-d 段**（决策块计数快照）与 **R-c 段**（双向对账）。

#### Scenario: **R-d 段 SHALL 按四步口径计数，并与快照逐份相等**

- **WHEN** 对 `docs/adr/ADR-CEP-*.md` 下每份 ADR 执行 §2 决策块提取
- **THEN** 提取 SHALL 严格按四步且**顺序不可换**：① **先剥** ` ``` ` 围栏代码块 → ② **再取** §2 章范围
      （`^## 2` 至下一个 `^##\s`）→ ③ **仅** `###` 级 → ④ 匹配 `^###\s*决策\s*\d+(?:\.\d+)?`
- **AND** 块 ID SHALL 为编号串的**正则全匹配**（`1` / `2` / `2.1` / `10`）⇒ ⛔ 前缀 / 子串 / 整数归一比较
      SHALL NOT 被使用（`stoi("2.1")==2` 会漏计;`grep -c "决策 1"` 会双命中 `决策 10`）
- **AND** 逐份计数 SHALL 与 `test/cep_decision_block_snapshot.json` 的**对应键**相等
- **AND** 快照的 `total` SHALL == 各份计数之和（实测 **125**），`count` SHALL == 键数（实测 **22**）
- **AND** 任一份偏离 SHALL 判 **FAIL**，红因 SHALL 含稳定签名 `决策块计数快照偏离` 且携带
      **文件名 + 期望值 + 实得值 + 差值**
- **AND** 判据 SHALL 为「**逐份相等**」⇒ ⛔ SHALL NOT 退化为「≥1」（后者删 8 条仍绿）

> **锚点**：`test/cep_decision_block_snapshot.json` · `scripts/test/cep_decision_block_check.py`

#### Scenario: **R-d 段 SHALL 对非 `###` 级与非 ASCII 十进制编号显式判红**

- **WHEN** §2 范围内出现决策标题 `^#{2,6}\s*决策` 但**层级 ≠ `###`**
- **THEN** SHALL 判 **FAIL**，红因签名 SHALL 为 `决策块标题非 ### 级`
- **WHEN** §2 范围内出现决策标题而其编号**非 ASCII 十进制**（例 `### 决策 Ⅻ`）**或非全匹配**（例 `### 决策 1a`）
- **THEN** SHALL 判 **FAIL**，红因签名 SHALL 为 `决策块编号非法`
- **AND** 该等判定 SHALL **仅作用于决策标题行** ⇒ ⛔ 正文散文中的「决策块」等词 SHALL NOT 触发误红

#### Scenario: **R-d 段 SHALL 显式标记 §2 内含行首 `## ` 的围栏块**

- **WHEN** §2 范围内存在围栏代码块，其**内容**含行首 `## `
- **THEN** SHALL 判 **FAIL**，红因签名 SHALL 为 `围栏内含行首 ##`
- **AND** 该判定 SHALL **不改变任何决策块的计数结果**（仅标记**危险围栏**）
- **AND** 理由 SHALL 记录为：既有 C++ 消费者 `test/cep_adr_parser.hh` 的 `read_section` 在 `^##\s` 处截断
      且**不能**剥围栏 ⇒ 该围栏对既有消费者是**活雷**

#### Scenario: **R-c 段 SHALL 做双向对账且期望值来自闸内常量**

- **WHEN** 执行 R-c 正向检查（文档语料 = `--root`）
- **THEN** `ADR-CEP-01` / `ADR-CEP-02` / `ADR-CEP-03` 三份**精确 basename** SHALL 存在
- **AND** 语料内任一 basename 满足 `^ADR-CEP-0[123](?![0-9])` 而**不属**三份精确名 ⇒ SHALL 判 **FAIL**，
      红因签名 `存在同号拆出件`
- **WHEN** 执行 R-c 反向检查
- **THEN** `ADR-CEP-00` 新 SU 的 `### ⑥` 重述块 SHALL 被**定界**取出（`### ⑥` 标题至下一个 `^#{2,3}\s`）
- **AND** 该块 SHALL 恰含 **3** 条 bullet，且每条 SHALL 含**唯一一个**完整编号 token
- **AND** `ADR-CEP-01` / `ADR-CEP-02` / `ADR-CEP-03` SHALL **各出现 ≥1 次** ⇒ 否则 SHALL 判 **FAIL**
      （红因签名 `重述块缺项`）
- **AND** 期望值 SHALL 为**闸内源码常量** ⇒ ⛔ SHALL NOT 从被检对象自身或 `NUMBER-MAP` 派生（自证复燃）

> **锚点**：`ADR-CEP-00` 的 `## Status Update 2026-11-12` 的 `### ⑥` 块（**格式与解析器同批定稿**）

#### Scenario: **R-c 段 SHALL 校验 3 条触发条件可解析**

- **WHEN** 在 `ADR-CEP-00` 的 SU（标题含 `2026-11-11`）的 `### ②` 之下取**第一张**表
- **THEN** 表头 SHALL 含「判定方式」列，且数据行 SHALL **恰 3 行** ⇒ 否则 SHALL 判 **FAIL**
      （红因签名 `触发条件表结构不符`）
- **AND** 每行的「判定方式」列 SHALL 含 **≥1 个 inline code span**，且**至少一个可解析** ⇒ 否则 SHALL 判
      **FAIL**（红因签名 `引用解析缺失`）
- **AND** 可解析 SHALL 覆盖 **4 类**：(a) 仓内路径（`os.path.exists`）· (b) 台账 `Entry #N` ·
      (c) **ADR 短名**（glob `docs/adr/ADR-CEP-NN-*.md`）· (d) 符号（同格**最后一个**路径 + 文件内命中定义式）
- **AND** ⛔ 类别 (c) SHALL 存在 ⇒ 否则 T1 / T2 行**首日红** ⇒ 实施者极可能把判据 weaken 成接受简写

#### Scenario: **判据 SHALL 区分「文档语料」与「被指物存在性」两个根**

- **WHEN** 以 `--root <影子树>` 运行
- **THEN** **文档语料**（ADR 集合 / 重述块 / §2 计数）SHALL 取自 `--root`
- **AND** **被指物存在性**（仓内路径 / 台账 Entry / ADR 短名 glob / 符号定义式）SHALL 取自**真实仓根**
      （影子树不含台账与闸外文件 ⇒ 若在影子根解析则恒红）
- **AND** 快照读取 SHALL 取自**真实仓根**（快照是闸外的期望值载体，⛔ 不是语料）

#### Scenario: **每条判据 SHALL 有决定性变异，且变异 SHALL 按签名红、还原后绿**

- **WHEN** 执行任一变异
- **THEN** 变异 SHALL **只作用于影子树** ⇒ 真实仓 SHALL **零改动**
- **AND** 变异后 SHALL 判 **FAIL**，且输出 SHALL 含**该条变异的稳定签名** ⇒ ⛔ `rc ≠ 0` SHALL NOT 单独构成红因
- **AND** **基线正控**（变异前）SHALL 为绿
- **AND** 变异**还原后** SHALL 复归绿
- **AND** R-c 的「同号拆出件」变异 SHALL 同时使 `cep_adr_structural_check.sh` 判红 ⇒ 两条闸 SHALL **各报各的签名**
      （⛔ 不得互相掩盖）

#### Scenario: **影子树复用 SHALL 是具名机制，不得存在第二份实现**

- **WHEN** 任一闸需要影子树
- **THEN** SHALL `source scripts/test/cep_test_shadow.sh` 并调用 `reset <shadow> [extra_cp...]`
- **AND** 必复制集合 SHALL 为 `docs/adr`（单一真源），差异复制 SHALL 由 `[extra_cp...]` 参数化
- **AND** ⛔ 任一闸 SHALL NOT 自带影子树复制实现

#### Scenario: **判据 SHALL 声明其不可约残余**

- **WHEN** 交付本能力
- **THEN** SHALL 显式记录：① 判据只验「**引用良形 + 被指物存在**」，**不验**「被指物真实现了该判定」;
      ② 计数类判据**防不住**「块数不变但 §2 正文被原位改写」;③ **新编号拆出件不在 R-c 覆盖内**
      （由 `cep_adr_structural_check.sh` 的分区完整性 + `ADR-CEP-00 §3` supersede 表兜底）
- **AND** ⛔ SHALL NOT 把「不可判 / 不空转」表述为「已覆盖 / 已证明」

> **锚点**：`docs/adr/ADR-CEP-00` 的 `## Status Update 2026-11-12` · `docs/architecture/07-coherence-exploration-platform.md`
> 的 `## Status Update 2026-11-12` · `scripts/test/cep_decision_block_check.py` 头注的**受控重复登记**
