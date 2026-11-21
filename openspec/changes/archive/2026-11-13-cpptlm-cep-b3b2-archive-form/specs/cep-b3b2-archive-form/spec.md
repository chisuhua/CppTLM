# cep-b3b2-archive-form Specification (delta)

## Purpose

B3b-2(受限族 **0/4**)的**归档形态**(MUST-FIX-3)与**回滚路径**(MUST-FIX-4)此前只存在于裁决文本,
仓内**无载体** ⇒ 终局 Ⅰ / Ⅳ(依赖归档形态)**不可执行**。

本 capability 把「形态已定义 + 判据未被降级 + 指针闭合 + 撤回成对」这一**契约面**化,
并使其实例由**单一闸**机器守住。

本 capability **只约束形态**,⛔ **不执行归档**(终局未触发)⇒ 任何载体都**不得**把它读作归档事件。

> **演进方式**:本 spec 经由 OpenSpec delta(**ADDED**)演进,**不直接改写**本文件。
> **规范载体**:`.rddf/roadmap/objectives/obj-cep-platform.md` 的 `## §14 · 归档态`
> **canonical 实现**:`scripts/test/cep_b3b2_archive_form_check.sh` + `scripts/test/cep_b3b2_deferral_check.sh`

## ADDED Requirements

### Requirement: cep-b3b2-archive-form

B3b-2 的归档形态与回滚路径 SHALL 有**唯一权威载体**,其形态 SHALL 由机器判据守住。

#### Scenario: MUST-FIX-3 的六项形态 SHALL 齐备且可机器判

- **WHEN** 校验归档形态定义
- **THEN** 权威载体 SHALL 是台账的 `## §14 · 归档态` 段,且 SHALL 含 **§14.1~§14.7** 七个子项
- **AND** 六项 SHALL 分别覆盖:① 归档路径与文件存续形态 ② 归档语义选用规则(按终局分支自动选用)
      ③ 义务转移载体 ④ 闸的 `LEDGER` 路径重定向规则 ⑤ 逐族锚的解析规则 ⑥ `07 号` 维护态段
- **AND** 形态定义 SHALL **明确声明**「⛔ 不是归档已发生」

#### Scenario: 台账的锚解析 SHALL 只认行首标题

- **WHEN** 校验逐族锚的解析规则
- **THEN** 判据 SHALL 为「存在以 `### <ref>` **开头**的行」⇒ **引述不得**命中
- **AND** 台账根集合 SHALL 来自重定向表(唯一真源)+ 环境变量 + 缺省 ⇒ 台账归档时 SHALL **只改表、不改闸代码**
- **AND** ⛔ SHALL NOT 用「排除自身路径」等消音式实现

#### Scenario: 规则 SHALL 只有一个真源

- **WHEN** 校验义务转移后的载体
- **THEN** 判定规则 SHALL 只在 `test/cep_b3b2_deferrals.json` + 闸 `cep_b3b2_deferral_check.sh`
- **AND** `07 号` 与手册 SHALL **只写指针** ⇒ ⛔ SHALL NOT 复写规则

#### Scenario: 回滚路径 SHALL 成对可判且不得靠删除

- **WHEN** 校验 MUST-FIX-4
- **THEN** 回滚 SHALL 由 ① `07 号` **新 SU 显式撤销** + ② 台账 **append-only 撤回标记** 双载体承载
- **AND** 撤回标记 SHALL **成对**记录「被撤回的 SU 标识」+「撤回 Entry 号」⇒ 缺一即红
- **AND** 触发条件 SHALL 含 ① 用户不 ratify 口径 ④ ② N = 1 登记出错 ③ 三处重裁被证明不可行
- **AND** 默认 SHALL **只撤回「口径 ④ 的登记形态」** ⇒ ⛔ SHALL NOT 撤销禁令作用域重裁(除非触发条件 ③ 明确指向)

#### Scenario: 判据 SHALL 声明其不可约残余

- **WHEN** 交付本能力
- **THEN** SHALL 显式记录:① 形态定义**不是**归档事件 ② 季度复审节奏**不变**(定义 ≠ 触发)
      ③ 回滚触发条件含「用户 / 实测」⇒ ⛔ agent 不得自行宣告回滚已执行
- **AND** ⛔ SHALL NOT 把「不可判 / 不空转」表述为「已覆盖 / 已证明」
