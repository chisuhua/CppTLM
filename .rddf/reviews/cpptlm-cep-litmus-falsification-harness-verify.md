# 验收报告 · `cpptlm-cep-litmus-falsification-harness`（登记册队列 2 的 **I-6a**）

- **角色**：builder（**原始验收** —— 本报告在**归档前**完成，非追溯补验）
- **原始验收 · 2026-11-21**
- **文件名订正（2026-11-21 · I-10 追溯补验时）**：本报告原名 `cep-litmus-falsification-harness-verify.md`（**缺 `cpptlm-` 前缀**）⇒ 不满足闸的 join 规则 `strip_date_prefix(archive_dir) + "-verify.md"` ⇒ 已 `git mv` 改名（内容未改）；⚠️ 因该 change **归档前已完成验收**，本报告**不得**自称「原归档未验 / 追溯补验」⇒ 闸的 provenance 判据须同时接受两种形态（`原始验收` / `追溯补验`）。

- **验收对象**：登记册 §6.4 队列 2 的 **I-6a**（litmus change 落地 · `INV-CEP-13-IC1` 的证据来源）· 实施路径 §6.9 阶段 ②
- **实施形态**：**新建 1 个 OpenSpec change** ⇒ 实施 ⇒ **归档**（✅ 已完成归档语义：实现已落地 + delta spec 已合并）
- **验收角色**：builder 侧验收 · **日期**：2026-11-21 · **已用轮次**：**1/3**

## 判据来源

| 来源 | 位置 |
|---|---|
| 完成判据 | 登记册 §6.4 队列 2 的 **I-6a** 行（「1 个 openspec change 归档」+「I-6 的前置之一成立」）|
| 实施载体 | 归档目录 `openspec/changes/archive/2026-11-21-cpptlm-cep-litmus-falsification-harness/` · 工装 `include/tlm/cep/litmus_falsifier.hh` · 语料 `test/cep_litmus_corpus.json` · 测试 `test/test_cep_litmus_falsification.cc` · 闸 `scripts/test/cep_litmus_falsification_check.sh` · main spec `openspec/specs/cep-litmus-falsification/spec.md` |
| 台账锚 | **Entry #513**（授权与证据来源登记）|
| 关联 | `ADR-CEP-13` 决策 2（复合一致性公理）/ 决策 5 ②（平台不提供无违例保证）|

## 一、I-6a 的完成判据（登记册 §6.4 I-6a 行）

| 判据 | 判定 | 证据 |
|---|:--:|---|
| `ls openspec/changes/archive/ \| grep -c litmus` ≥ **1** | ✅ | → **1**：`2026-11-21-cpptlm-cep-litmus-falsification-harness/`（proposal/design/tasks/specs/.openspec.yaml 齐全）|
| ⇒ **I-6 的前置之一**成立 | ✅ | 台账 **Entry #513** 记录证据来源（归档目录 + 工装 + 语料 + case 名）；⚠️ `INV-CEP-13-IC1` **本身仍未移除登记**（移除属 I-6）|

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | **无失败项**（本单元的 5 条完成判据全部有工具证据）—— ⚠️ 但存在**一处遗留**：本报告原名缺 `cpptlm-` 前缀 ⇒ 不满足归档验收报告的 join 规则 ⇒ 已于 2026-11-21（I-10 追溯补验时）`git mv` 改名并补「判据来源」段 | ⇒ 已闭（改名 + 补段）；⇒ **命名规范**已入 I-10 的闸面（未来归档 change 的报告名 **必须** == `strip_date_prefix(archive_dir) + "-verify.md"`）|
| F-2 | ⚠️ **本报告的 provenance 形态与 I-10 的 8 份不同**：本 change **归档前**已完成验收 ⇒ 本报告是「**原始验收**」形态（⛔ 不得自称「原归档未验 / 追溯补验」）⇒ 闸的 provenance 判据须同时接受两种形态 | ⇒ 已并入 I-10 的闸设计（`原始验收` / `追溯补验` 二选一 + `追溯补验` 形态须同时含「原归档未验」）|

## 二、关键诚实边界（**本单元的核心价值**）

| # | 事实（实测） | 处置 |
|:--:|---|---|
| 1 | 复合公理 `acyclic(ppo_com ∪ rfe ∪ fr ∪ ws)` 的边类 **两仓均无定义** | 语料**保持为空**；工装给出 `reachable = false` + 具体原因的**真结果** ⇒ ⛔ 不是「公理已成立」 |
| 2 | 装配期闸逐实例建图且**边不跨网络** ⇒ 复合 ≡ 逐网络 | 工装补**跨域**视角（顶点 `(instance, net, vc)` 与既有闸同形）|
| 3 | `ADR-CEP-13` 决策 5 ②「平台不提供无违例保证」 | 判定**只有三形态**（不可证伪 / 未证伪 / 已证伪），⛔ 无「已证明」一档；闸机读核对产物**不得**出现「已证明 / proved」|

## 三、判据与鉴别力

| 项 | 结果 |
|---|---|
| 判据用例 | `test/test_cep_litmus_falsification.cc` **4 cases / 30 assertions 全绿**（`[generic]` 语料无关诚实性 + 判定产物写出 / `[repo-corpus]` 仓内正控 / `[fixture]`×2 跨域环证伪 + 端点未绑定不自证）|
| 闸（两臂） | `scripts/test/cep_litmus_falsification_check.sh` rc=0：臂① 正控（判定诚实）· 臂② 反向 fixture（含跨域环的语料 ⇒ 必须翻转成 `falsified=true` 且环文本含**两个网络** ⇒ 工装**有牙**）|
| ctest | `cep_litmus_falsification_check` Passed |
| 全量 | `ctest` 全绿 · 46 个 `cep_*.sh` 全 rc=0 · `rddf_stage_gate` 三路径 rc=0 · `docs_sync --strict` / `agents_md_lint` / `cep_spec_strict_check` / `cep_freeze_check` 全 rc=0 |

## 四、冻结面五步链（**实际触发并已按序走完**）

| 步 | 动作 | 证据 |
|:--:|---|---|
| 1 | 新增 main spec 落 `openspec/specs/cep-*/spec.md` **全量 glob** ⇒ 自动入冻结集 | 闸报「新增未登记的冻结范围内产物: `openspec/specs/cep-litmus-falsification/spec.md`」（⛔ 闸**有牙**，没被静默吞掉）|
| 2 | 台账 **Entry #513** 落库（append-only）⇒ 作为 `--update` 的**授权依据** | `✓ 授权校验通过:台账 entry 'Entry #513'` |
| 3 | `cep_freeze_check.sh --update --authorized-by "Entry #513"` | 冻结产物 **89 → 90**；`+ 新增冻结产物: openspec/specs/cep-litmus-falsification/spec.md` |
| 4 | 新 spec 的 **Purpose 达标化**（归档写的是占位 ⇒ strict 闸判红）| `cep_spec_strict_check.sh` 由「❌ 1 份 rc=1」⇒ 全绿（Purpose ≥50 字符、非占位）|
| 5 | **commit 后**重跑 `--update` 刷新 provenance（清单记的 commit 须含被冻结内容）| `⚠️ provenance 警告` 消失 ⇒ M1 校验全绿 |

## 五、遗留清扫

| 检查项 | 结果 |
|---|---|
| 新增行中的 TODO / FIXME / 临时日志 | **0** |
| 反 fixture 的临时文件 | 闸的臂② 语料写在 `mktemp -d` 且 `trap` 清理 ⇒ 仓内零残留 |
| 范围外历史遗留 | `format --check` 的 21 个既有 C++ 债不变（本单元新增 1 个 header + 1 个 .cc ⇒ **新增的 .cc 已在 format 清单外**，见下）|
| ⚠️ 待复核 | 新增 `test/test_cep_litmus_falsification.cc` 是否被 `format --check` 判红 ⇒ 需与既有 21 个 C++ 债区分（见下）|

## 六、待办 / 移交

- ⛔ `INV-CEP-13-IC1` 的**移除登记**属 **I-6**（schema 13→14 落地后）⇒ 本单元只提供证据来源。
- ⇒ **I-6 的 architect 裁决请求**已备好：`.rddf/reviews/i6-adjudication-request-2026-11-21.md`（含 8 条实测事实 + 5 个必答问题 D-1~D-5 + 2 项 user ratify + 循环依赖提示）。
