# 审稿报告 · `07 号` SU 2026-11-06 · 四(T1 来源效力裁决)

- **对象**：`docs/architecture/07-coherence-exploration-platform.md` 定稿追加 SU `Status Update(2026-11-06 · 四, per 台账 Entry #249)`
- **审查顺序**：oracle → critic（architect 配对要求，顺序不可颠倒）
- **轮次**：oracle 1 轮 · critic 2 轮（第 1 轮 REJECT 1 BLOCKER → 第 2 轮 APPROVE）
- **结论**：**APPROVE · mergeable: yes · scope_exceeded: false**

## 一、oracle 轮（设计面：这个裁决会怎么坏）

关键发现（全部已在 v2/v3 吸收）：

| # | 发现 | 性质 | 处置 |
|---|---|---|---|
| 1 | D-1 与 `cxl_state_spec.hh` 的 `PublicDirect`（公开来源明确 ⇒ 强度 direct）**正面冲突**；且会对 22 格 `pending=false` 造成**追溯降级** | 否决级 | D-1 加**窄口径 + carve-out**（既有枚举语义不变、22 格由其自身 change 链授权、本条不 reopen） |
| 2 | CHI RN-F 的表是 `opcode → channel/flow`，**无逐格 basis/ref** ⇒ D-1 若一致套用会把计数从 2/6 打回 **1/6** | 否决级 | D-2 增**载体形态分类**（状态矩阵型 / opcode 事件表型；CHI 按表级 ref + 完备性锁成立，只登记分类不改计数） |
| 3 | 「来源类型」在载体中**无字段**（`Transition.ref` 为自由字符串）⇒ D-1 的判据不可机器判 | 否决级 | D-1 加**诚实声明**（不可自动判，只能人工抽样 + 来源注册表 backlog）；删除「grep 级判据」措辞 |
| 4 | ③ 漏掉 B2 的「分母不可缩」先例与 **DoD 条目拆分**形态 | 否决级 | §④ 五选项（甲/乙/丙/丁/**戊**），戊 = `B3b-1 2/2 ✅ + B3b-2 0/4 ⏭`（建议） |
| 5 | D-3 漏「**人类成员条款摘要**」路径（已有 spec 级先例）⇒「永久外部阻塞」前提错误 | 高 | D-4 增第 2 条路径 + 证据档位；「永久」→「外部条件 + 人力受限」 |
| 6 | 允许「≥2 来源交叉一致」直接标 `Direct` 是**错误的**（错误代价不对称：矩阵格错 ⇒ 引擎静默错误转移）；但应允许**影子矩阵**（弱档 + pending + 不消费、不计入）保留探索价值 | 中 | D-1 写入「探索载体可存在、不计入」+ 域边界（B3a 声明域 vs B3b 矩阵域） |
| 7 | `B3b` 状态单一标量 `2/6` 是**伪均匀分母**（「建 68 格后冻结」与「0 起步」等权） | 中 | D-3 改为**三元组**：`2 / 分母 6 / {1 冻结, 3 受限}`；⛔ 禁止 `2/2`（分母收缩） |
| 8 | 4 族 ⏭ 的 `reason` 沿用旧理由（「T1 未触发」）会与 D-1 的事实不符 | 中 | 💠 **判定结论不变、理由换**（写进 SU 的 Verification 勾选项） |

## 二、critic 轮（一致性：与既有 ADR / 契约是否冲突）

**第 1 轮 REJECT（1 BLOCKER + 2 SHOULD_FIX）**：

- **BLOCKER**：§0「⛔ 不改 `obj` 的 DoD 文本」与 §6 勾选项 1「`obj` 的 B3b 行改为三元组」**自相矛盾**（同一决策两个互不兼容答案），且勾选项 1 预设形态与 §4 未决选项耦合。
- **SHOULD_FIX ①**：D-1 的「`Direct` = 规范条款级」与 `cxl_state_spec` 的 `PublicDirect` 语义碰撞（需 carve-out 显式化）。
- **SHOULD_FIX ②**：D-1 的「非规范矩阵 fail-closed」与 `ADR-CEP-12` 的 `ualink_4`（非规范材料**已被机器消费**）立场不对称 ⇒ 缺**域边界**说明。

**第 2 轮（只验 BLOCKER 闭合 + 新 diff）→ APPROVE**：
- 勾选项 1 已圈定为「状态/达成度记账句（**不含 DoD 目标文本**）」，并写明 DoD 文本形态由 §④ 用户裁决决定 ⇒ 两对象域互斥，矛盾闭合；
- §5 负面句改全条件式（五选项并列），未决问题未被写成已决；
- NITS：文档标题的「修订 v2」版本标签滞后（过程追踪项，不影响决策）；另建议 verifier 在归档时确认「carve-out 不 reopen」落为审计证据而非口头承诺。

## 三、处置汇总

| 类别 | 条数 | 状态 |
|---|---:|---|
| BLOCKER（critic 第 1 轮） | 1 | ✅ 已闭合（critic 第 2 轮确认） |
| 否决级（oracle） | 4 | ✅ 全部吸收 |
| SHOULD_FIX | 2 | ✅ 全部吸收 |
| NITS | 2 | ✅ 吸收（版本标签 / verifier 核对建议） |
| oracle / critic 直接 BLOCKER（v3 定稿） | 0 | — |
