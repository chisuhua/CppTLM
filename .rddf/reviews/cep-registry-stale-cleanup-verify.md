# 验收报告 · `cep-registry-stale-cleanup`（登记册队列 2 的 **I-1**）

- **验收对象**：登记册 §6.4 队列 2 的 **I-1** —— 口径 stale 集 a/c 清理（缺口 S2-6a / S2-6c）
- **验收角色**：builder 侧验收
- **日期**：2026-11-20
- **已用轮次**：**1/3**
- **判据来源**：登记册 §6.4 I-1 行「完成判据」列（a / c 两条 grep 判据 + freeze 闸 rc 不变）
- **分流判定**：无 `openspec/changes/<name>/`（本单元零 OpenSpec change ⇒ KPI 0/3）· P3 archive gate 未触发 ⇒ 不走 `rdd-verifier` skill ⇒ 本地验收

## 一、逐条判定

| 项 | 判据 | 判定 | 证据 |
|---|---|:--:|---|
| **a** | `grep -cF 'ADR 归因**(18 条)' docs/guide/cep-usage.md` == **0** | ✅ | → **0**；并**加严复验**（⛔ 不止「旧串消失」）：`grep -cF 'ADR 归因**(19 条)'` → **1**；行内分项和 **2+10+2+5 = 19 == 总数 19**；四项取值与 `test/cep_adr_structural_deferrals.json` 的 `invariant_attribution_dispositions`（**19 条**：`carrier_buildable_id_blocked` 10 · `doc_layer_manual_review` **5** · `condition_unmet` 2 · `carrier_present_id_absent` 2）**逐项完全一致** |
| **c** | `grep -c '当前 \`docs/architecture/cep/\` 不存在' scripts/test/cep_freeze_check.sh` == **0**；`cep_freeze_check.sh` rc 不变 | ✅ | → **0**；`bash scripts/test/cep_freeze_check.sh` **rc=0**（与改动前基线一致）；`bash -n` 通过；闸输出 `✓ 冻结范围一致性:脚本 23 条 glob == 清单记录` · `✓ 89 个产物 hash 全部一致` 不变 |

**⚠️ 判据加严说明（非判据变更，是验收侧补强）**：I-1 的原判据只有「旧串 == 0」——单靠它会放过「把整行删掉」或「把数字改成任意别的值」。本轮额外用「新串命中 1 ∧ 分项和自洽 ∧ 与 JSON 真值逐项一致」三重核对，确保订正是**向真值收敛**而不是「消掉旧串」。⛔ 未修改 I-1 的登记判据本身（那属 planner 域）。

## 二、事实核对（订正的依据）

```
a · docs/guide/cep-usage.md:220（改动前）
| **ADR 归因**(18 条) | condition_unmet 2 · carrier_buildable_id_blocked 10 · carrier_present_id_absent 2 · doc_layer_manual_review 4 | ...

真值（python 实测 test/cep_adr_structural_deferrals.json）
invariant_attribution_dispositions = 19 条
  carrier_buildable_id_blocked 10 · carrier_present_id_absent 2
  condition_unmet 2 · doc_layer_manual_review 5

c · scripts/test/cep_freeze_check.sh（改动前注释）
「⚠️ 当前 `docs/architecture/cep/` 不存在 ⇒ 本 glob 现 0 匹配,冻结产物数 69 → 69 不变」
实测：ls docs/architecture/cep/ → 9 篇 .md（目录已建）；manifest artifact_count = 89（frozen_at 2026-11-19）
```

## 三、改动清单（两个 commit，按角色可追溯）

| commit | 文件 | 归属 | 改动 |
|---|---|---|---|
| `8dc55e93` | `scripts/test/cep_freeze_check.sh` | builder | 注释 3 行：删「不存在 / 0 匹配 / 69 → 69」，改为「目录已建 ⇒ glob 有匹配，子文档自动入集；总数以 manifest 的 `artifact_count` 为准（⛔ 不写死数字）」|
| `31bbf096` | `docs/guide/cep-usage.md` | ⚠️ 该路径在 `arch-docs.json` 的 `arch_paths` 内（architect 写集）→ 按登记册 I-1 的「拆成两个小 commit **或指定单一执行者**」取后者，单独成 commit | 1 行：`18 条` → `19 条`，`doc_layer_manual_review` `4` → `5` |

**收尾状态**：`scripts/test/rddf_ac_carrier.tsv` 的 `I-1` → `done`（前驱 I-0 已 done ⇒ 不触发 P-2a 跳序）。

## 四、回归

`ctest` 前次全量 **191/191 passed**（本单元改的是注释 + 文档，无编译单元）· `cep_dod_reading_check` / `cep_adr_structural_check` / `cep_docs_sync_check` / `cep_doc_ref_check` / `docs_sync_check --strict` / `rddf_stage_gate`（默认 · `--phase queue2` · `--self-test`）**全 rc=0**。

## 五、遗留清扫

| 检查项 | 结果 |
|---|---|
| 新增行中的 TODO / FIXME / 临时标记 | **0** |
| 是否产生新的「写死数字」 | **无** —— c 项刻意不写死（89 / 9 都未落进注释），a 项的 19 是**当前真值**且被 JSON 机判锚定（下游若变，`cep_adr_structural_check.py` 侧可见）|
| 范围外历史遗留 | §1.5 表格 2 处既有断裂（`git diff` 0 命中，非本轮引入）⇒ ⛔ 未顺手清理 |

## 六、建议（⛔ 本轮不做，交用户决定）

a 项的根因是「文档里的计数没有机器闸校验」—— 任何一次 JSON 变动都会让这行再次腐化。彻底修法是给 `cep_adr_structural_check.py` 加一条「`cep-usage.md` 归因行 ↔ JSON 逐项一致」的断言（≈10 行）。⛔ 本次未做：I-1 的判据只要求旧串消失，加闸属**新增判据面**，应单独立项。

## 七、已用轮次

**1/3**（无失败返工）。
