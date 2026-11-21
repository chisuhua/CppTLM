# tasks: cpptlm-cep-spec-strict-compliance

> **范围**: 12 份 spec 修到 `--strict` 0 红 + 防退闸 · **零实现层改动 · 零 ADR 改动 · 零归档件改动**
> **AC 映射**: proposal AC-01 ~ AC-18 · design D-1 ~ D-7
> ⚠️ **顺序不可换**：spec 先全绿 → 再跑 fixture → 最后注册 ctest（fixture 的基线前置要求全绿）

---

## Phase 0 · 立项态核对（planner 域已完成）

- [x] **T0.1** `openspec validate cpptlm-cep-spec-strict-compliance --strict` ⇒ rc=0
- [x] **T0.2** improvement v2（metis 10 MUST + 7 SHOULD + 6 NIT 全处置）+ change 目录 + proposal + **5 份 delta**
- [x] **T0.3** 台账 **Entry #434**（含回归认领）· KPI **0/3 → 1/3**
- [x] **T0.4** `design.md` + `tasks.md`（builder 域）
- [x] **T0.5** delta 自检：5 份既有 Scenario **0 丢失**，ualink 新增 4 条（`UGE-F/G/H/I`）

## Phase 1 · W1a 两份回归（纯删过程块）

- [x] **T1.1** `cep-sharer-tracker-base`：删「本次修订」3 行 → 1 行边界声明（604 → **206**）
- [x] **T1.2** `cep-invariant-gate`：删 3 条过程块 → 1 行限制披露（811 → **386**）
- [x] **T1.3** delta 内容 = 整块复制 + 定点删（由生成器产出，既有 Scenario 逐字保留）

## Phase 2 · W1b / W1c

- [x] **T2.1** `cep-collective-plane-contract`：删 `:29-30`（517 → **401**）
- [x] **T2.2** `cep-core-contracts`：删「订正(2026-11-15)」过程块，**保留**规范句 `SHALL NOT 在标题内嵌数量`（697 → **336**）

## Phase 3 · W1d `cep-ualink-coherence-engine`（最危险 —— 单独审查）

- [x] **T3.1** 去重 785（三分法表 387 + 4 条不变量 398 —— 已由 `UGE-A` 承载）
- [x] **T3.2** 下沉 3 条无场景覆盖的规范句 → `UGE-F`（txn_id 唯一键 + p2p_ordered 非假设）/ `UGE-G`（response handler 禁令）/ `UGE-H`（13 键 + B3b 登记）
- [x] **T3.3** 补 `UGE-I`（零 UPLI 通道级语义的操作定义：`ReqCmd` / 4 通道时序 / `transaction tag` 配对）
- [x] **T3.4** 裁后 **481** ≤ 500
- [x] **T3.5** critic（arch 档）逐条比对「删掉的是否都不是规范内容」

## Phase 4 · W4 八份 Purpose（直接改 main spec）

- [x] **T4.1** 8 份 Purpose 全部改写（83~121 字符，非占位、非 Requirement 首句复述、无过程叙述）
- [x] **T4.2** 7 份 Purpose-only 的 spec 已转绿（`--strict` rc=0）；`cep-ualink-coherence-engine` 待 delta 合并

## Phase 5 · W2 防退闸

- [x] **T5.1** `scripts/test/cep_spec_strict_check.sh`：① 零红（`--specs --json` 过滤 `cep-`）② 覆盖对账（129 / 43）③ 环境 exit 2 ④ 版本固化 ⑤ 反向 fixture
- [x] **T5.2** 闸实跑：① 正确报 5 份红（delta 未合并态）② 对账绿 ③ **前置不满足正确 exit 2**（基线不绿 ⇒ 毒化不可归因）
- [x] **T5.3** 归档后复跑：应 rc=0 且 fixture 5 项全绿
- [x] **T5.4** `--main-only` 快路径可用

## Phase 6 · W7 共享 helper 扩展 + 回归

- [x] **T6.1** `scripts/test/cep_test_shadow.sh`：`cp` → `cp -r`（对文件行为相同；支持目录 `extra_cp`）
- [x] **T6.2** 回归共用该 helper 的 2 闸：`cep_adr_structural_check.sh` / `cep_decision_block_check.sh`

## Phase 7 · W6 `removals.json`（AC-04 的载体）

- [x] **T7.1** 用「基线（`git show <base>:`)− 终态（归档后）」抽取 SHALL/MUST 句差集
- [x] **T7.2** 逐句分类 `kind` + `covered_by`（`process` / `interpretive` / `moved` / `reworded`）
- [x] **T7.3** 自检：`sentence_sha256` 集合 == 差集（**集合相等**）；非 `process` 类的 `covered_by` 可解析

## Phase 8 · W5 文档连带

- [x] **T8.1** `scripts/README.md` 守门表 +1 行
- [x] **T8.2** `test/CMakeLists.txt` 注册 `cep_spec_strict_check`（⚠️ 放最后一个产物 commit）
- [x] **T8.3** `AGENTS.md` 契约闸数 **44 → 45**

## Phase 9 · 归档（7 步，顺序固定）

- [x] **T9.1** ① 台账 Entry #434（planner 已落）
- [x] **T9.2** ② Purpose + delta 落盘
- [x] **T9.3** ③ 影子根预演 archive ⇒ `~ 5 modified` · 合并后 **43 份全绿 / 超长 0** · 既有场景 **0 丢失 0 改动** · 4 新增 · 仓内零改动
- [x] **T9.4** ④ 真仓 `openspec archive --yes` + 复验 0 红
- [x] **T9.5** ⑤ 产物 commit
- [x] **T9.6** ⑥ `cep_freeze_check.sh --update --authorized-by "Entry #434"` ⇒ 清单 diff = **12 份** hash + 3 元数据键；`artifact_count` 86 不变；`docs/adr/**` 与 `07-*.md` 零变化
- [x] **T9.7** ⑦ 清单单独 commit + 无参复验 rc=0 + `frozen_at_commit` == 产物 commit sha

## Phase 10 · 收口

- [x] **T10.1** 归档后 diff 只落目标段（5 份，AC-17）
- [x] **T10.2** KPI **1/3 → 0/3**（6 处活口径 + obj 并发句）+ 体量数字（归档 146 → **147**）
- [x] **T10.3** 台账闭合回执 Entry
- [ ] **T10.4** verifier 独立验收（AC-01 ~ AC-18 逐条 + 工具证据）
