# R-1 验收报告 —— `cpptlm-cep-b3b2-archive-form`

> **判据来源（按优先级）**
> 1. 用户口令「继续推进后续任务」（+ 对 MUST-FIX-3 两个开放点的拍板）
> 2. `openspec/changes/archive/2026-11-13-cpptlm-cep-b3b2-archive-form/proposal.md` 的 **8 条 AC**
> 3. architect 决策 `.rddf/reviews/cep-mustfix-3-4-archive-form-decision.md`（6 项定义 + 回滚路径 + V1~V7）
> 4. 台账 **Entry #384**（立项 + 拍板登记）· **#385**（实施）· **#386**（归档）
>
> **验收轮次**：**第 1 轮**（`RDDF_VERIFIER_MAX_LOOPS=3`）
> **结论**：**PASS** —— 8 条 AC **全部满足**；**0 BLOCKER**；**1 条 `proposal_drift`（非阻断，判据字面过宽）**
> **角色边界**：本报告**未修改任何实现**（只读 + 运行验证命令）。

---

## 一 · 逐条判定（每条均有工具证据）

| AC | 判定 | 证据（命令 → 实测） |
|---|:--:|---|
| **AC-1** 六项形态齐备 | ✅ | 新闸 **V1** ⇒ 台账 `## §14 · 归档态` 段存在 + **§14.1~§14.7 七个子标题**齐备;删任一项 ⇒ 签名 `归档形态定义缺项` |
| **AC-2** 锚解析已升级且未降级 | ✅ | 闸代码含 `startswith('### ' + le)`（**行首标题匹配**）;新闸 **V2** 绿;**M2** 把影子树内的闸改回全文子串 ⇒ **V2 对影子树判红**(签名 `锚解析降级`)⇒ 证明该判据确实能抓住降级(⛔ 对真实仓跑无鉴别力 ⇒ M2 必须对影子树跑) |
| **AC-3** 重定向表 | ✅ | `test/cep_b3b2_ledger_redirect.json` 存在;`roots[0]` = `.rddf/roadmap/objectives/obj-cep-platform.md` ⇒ 新闸 **V3** 绿;改错 ⇒ 签名 `重定向表首根不符` |
| **AC-4** `07 号` 维护态单向指针 | ✅ | `07 号` §12 含「维护态的 B3b-2 判定规则」指针句 ⇒ 新闸 **V4** 绿;⛔ 该句**只写指针**、未复写规则(真源仍 = JSON + 闸) |
| **AC-5** 回滚形态成对可判 | ✅ | `obj §14.7` 含「撤回标记(成对可判)」+ `#### §14.7.1` 撤回登记表(列 = 被撤回的 SU 标识 / 撤回 Entry 号 / 理由 / 生效日)⇒ 新闸 **V5** 绿;触发条件 **3 条**齐备;⚠️ 当前为**空占位**(尚未触发任何回滚条件) |
| **AC-6** 闸行为有牙 + 既有 4 条仍绿 | ✅ | **M1**:影子树内把 4 条 `ledger_entry` 改为 `Entry #999999` ⇒ 闸 **rc=1** 且输出含 **`ledger_entry`** ⇒ 还原 ⇒ 绿;`cep_b3b2_deferral_check.sh` 对真实仓 **2 PASS / 0 FAIL**;`test/cep_b3b2_deferrals.json` **改动行数 = 0**(⇒ 行首匹配兼容现有 `Entry #249`) |
| **AC-7** 0 回归 + 冻结链 | ✅ | `ctest -N` = **175**;`cep_*.sh` = **41**;冻结面 **81 产物** · `cep_freeze_check.sh` 无参 **rc=0**;format / docs_sync / doc_ref / lint / cep_docs_sync 全 **rc=0**;`cep_index_position_check` 绿(§12 追加 2 行 ⇒ 索引行号位移 ⇒ **机重写** 5 删 5 增) |
| **AC-8** 范围纪律 | ✅ | `docs/adr/` 变更文件数 = **0**;`B3b-2` 行仍在(1 命中,仍 **0/4**);`§4.0` 区间**逐行相等** = True;deferrals JSON **零 diff**;⚠️ 「未执行归档」实测:§14 段内「已归档」**唯一出现处是否定句**(「⇒ 本段的存在**不表示** objective 已归档」)⇒ 实质满足(见 §四 唯一 finding) |

---

## 二 · 判据有牙性复核（不只看 rc）

| 判据 | 是否只靠 `rc≠0` | 反证 |
|---|:--:|---|
| 形态齐备 | ❌ | V1 按**标题**逐项断言,缺项 ⇒ 签名 `归档形态定义缺项` |
| 锚未降级 | ❌ | **M2** 降级 ⇒ V2 对影子树判红 ⇒ 判据对「降级」敏感 |
| 闸有牙 | ❌ | **M1** 破坏 ⇒ 含 `ledger_entry` 签名 ⇒ 还原 ⇒ 绿 |
| 隔离 | ❌ | 真实仓 5 文件**字节指纹前后相等** ⇒ 变异只在影子树 |

---

## 三 · 遗留清扫复核（本次范围内的自查项）

| 项 | 实测 |
|---|---|
| `TODO` / `FIXME` / `XXX`（2 个新增文件） | **0** |
| 被注释掉的代码 | **0** |
| `skip` / 跳过的测试 | **0** |
| 临时日志 / 调试输出残留 | **0**（影子树在 `/tmp/opencode/`;破坏 JSON 在影子树内) |
| 临时调试分支 | **0** |

---

## 四 · 失败项与路由（**0 BLOCKER**）

| # | 项 | 分类 | 路由 | 说明 |
|:--:|---|---|---|---|
| 1 | **AC-8** 的字面判据「§14 段**不含**「已归档」表述」**过宽** | `proposal_drift` | **planner**(后续订正 AC 文本,⛔ 不阻断) | 该字面会把**防误读声明**(「⇒ 本段的存在**不表示** objective 已归档」)判红 ⇒ 替代表述应是「不含『objective **已**归档』的**肯定**表述」;**实现本身正确**(实测:§14 段内「已归档」唯一出现处为否定句)⇒ ⛔ 不入 build 返工队列 |

---

## 五 · 已登记的实施期事实（可复核）

| # | 事实 | 登记处 |
|:--:|---|---|
| ① | `07 号` §12 追加 2 行 ⇒ **索引 A/B 行号位移** ⇒ `cep_index_position_check` 曾红 ⇒ **机重写**索引(5 删 5 增,纯机械) | Entry #386 ③ |
| ② | 归档目录前缀 = **`2026-11-13-`**(系统日期已跨日;实施在 11-12 深夜、归档在 11-13)⇒ 标识与实际**一致** | Entry #386 ① |
| ③ | MUST-FIX-3 ⑥ 的载体**已存在**(`07 号` §12 已是入口指针)⇒ **无需新建载体** ⇒ 实施面比裁决预估小 | Entry #384 ③ |

---

## 六 · 已用轮次

| 轮 | 结论 | 说明 |
|:--:|---|---|
| **1（本轮）** | **PASS** | 8/8 AC 满足;0 BLOCKER;1 条 `proposal_drift` 注 |

**是否打回**：**否**。

---

## 七 · 复核命令清单（可原样重跑）

```bash
bash scripts/test/cep_b3b2_archive_form_check.sh      # V1~V6 + M1/M2
bash scripts/test/cep_b3b2_deferral_check.sh          # 既有 4 条 + 真源交叉校验

ctest --test-dir build -j4
for g in scripts/test/cep_*.sh; do bash "$g" >/dev/null || echo "FAIL $g"; done
bash scripts/test/cep_freeze_check.sh
bash scripts/test/cep_index_position_check.sh
bash scripts/build/format.sh --check
bash scripts/test/docs_sync_check.sh --strict
bash scripts/test/cep_doc_ref_check.sh --strict
python3 scripts/test/agents_md_lint.py

# 范围纪律
git diff --numstat 18d19d56..HEAD -- test/cep_b3b2_deferrals.json
git diff --name-only 18d19d56..HEAD -- docs/adr
```
