# R-4 验收报告 —— 体量数字机器守（`cpptlm-cep-volume-number-guard`）

> **判据来源**
> 1. 用户口令「继续推进后续任务」
> 2. `.rddf/improvements/cpptlm-cep-volume-number-guard.md` 的 **6 条 AC**
> 3. 台账 **Entry #388**（立项 + 前提实测）· **#389**（实施 + 路径订正）
>
> **验收轮次**：**第 1 轮** · **结论**：**PASS** —— 6 条 AC **全部满足**；**0 BLOCKER**；**2 条已登记的实施期订正**
> **角色边界**：本报告**未修改任何实现**（只读 + 运行验证命令；变异在临时副本上做并已还原，字节指纹已比对）。
> ⚠️ **路径形态**:本 change 走**零槽位批**路径（⛔ 未建 `openspec/changes/` ⇒ ⛔ 未占 KPI slot）—— 见 §五-①。

---

## 一 · 逐条判定（每条均有工具证据）

| AC | 判定 | 证据（命令 → 实测） |
|---|:--:|---|
| **AC-1** 断言存在 | ✅ | `cep_docs_sync_check.sh` 第 12 组含**断言 ⑨**;输出行「✓ 体量数字 == 实测(归档 140 / cep-* spec 40 / 闸 41;命中 9 处,跳过 0 处)」 |
| **AC-2** 当前态数字 == 实测 | ✅ | 命中 **9 处**、逐条 == 实测(归档 **140** · cep-* spec **40** · 闸 **41**)⇒ 闸 rc=0 |
| **AC-3** 决定性变异 | ✅ | `AGENTS.md` 的「140 份已归档」⇒「999 份已归档」⇒ 闸 **rc=1**,红因 **「体量数字漂移[归档 change 数]: 写 999,实测 140」**(含错值**与**实测值)⇒ 还原 ⇒ **rc=0** |
| **AC-4** 历史叙述不误伤 | ✅ | 扫描集**排除台账 obj**(其 §11 Entry 区是 append-only 历史)⇒ 台账内旧数(135/138/37/35 等)零误报;另加**动作宾语排除**⇒「22 Scenario **并入** 3 份 main spec」不判红 |
| **AC-5** 0 回归 | ✅ | `ctest` **175/175** · `cep_*.sh` **41/41** rc=0 · format / docs_sync / doc_ref / lint / cep_docs_sync / gate_selftest **全 rc=0** |
| **AC-6** 范围纪律 | ✅ | `docs/adr/` 变更 = **0**;冻结清单 **hash 变更 = 0**(⇒ 零冻结面、**不触发五步链**);`B3b-2` 未动;⛔ 未新增 capability(仅加断言 + 数字对齐) |

---

## 二 · 判据有牙性复核

| 判据 | 是否只靠 `rc≠0` | 反证 |
|---|:--:|---|
| 体量数字 | ❌ | 变异 ⇒ 红因含「**写 999,实测 140**」⇒ 错值与真值都被点名 |
| 断言自身 | ❌ | 移除断言 ⇒ 该检查项消失 ⇒ 变异将不再红 ⇒ 断言是唯一防线 |
| 误伤面 | ❌ | 台账历史 + 动作宾语两类真实误伤在实施期被抓出并修正(见 §五-②③)⇒ 判据经过**对抗性校准**而非首版即用 |

---

## 三 · 遗留清扫复核（本次范围内的自查项）

| 项 | 实测 |
|---|---|
| `TODO` / `FIXME` / `XXX` | **0** |
| 被注释掉的代码 / `skip` 的测试 / 临时日志 / 调试分支 | **0** |
| 临时文件残留 | **0**（变异备份在 `/tmp/opencode/`，已还原；仓内 0 改动） |

---

## 四 · 失败项与路由（**0 BLOCKER**）

**无失败项。**

---

## 五 · 已登记的实施期订正（可复核）

| # | 订正 | 登记处 |
|:--:|---|---|
| ① | **路径订正**:#388 写的「占 1 slot / 归档 / 两次 KPI 回填」**作废** —— 本 change 产物 = 闸断言 + 文档数字(⛔ 无契约面变更)⇒ per **C1 先例**走**零槽位批**、**不占 slot** ⇒ KPI 全程 **0/3** | Entry #389 ① |
| ② | **动作宾语排除**:首版把「22 Scenario **并入** 3 份 main spec」判红 ⇒ 该「3 份」是**动作宾语**(某次归档并入的份数)⇒ ⛔ 不是仓内总量 ⇒ 加「数字紧前 14 字内出现动作动词 ⇒ 跳过」 | Entry #389 ③ |
| ③ | **实测不可得 ⇒ 跳过(非 FAIL)**:闸的**回归 fixture**(`cep_gate_selftest.sh`)在**仓外副本**中运行 ⇒ `scripts/test/` 不在其中 ⇒ 闸数不可数 ⇒ 首版误判为漂移 ⇒ 改为 SKIP(与闸族既有语义一致)⇒ 断言在**真实仓**仍是硬判 | Entry #389 ③ |

---

## 六 · 已用轮次

| 轮 | 结论 | 说明 |
|:--:|---|---|
| **1（本轮）** | **PASS** | 6/6 AC 满足;0 BLOCKER |

**是否打回**：**否**。

---

## 七 · 复核命令清单

```bash
bash scripts/test/cep_docs_sync_check.sh --strict    # 含断言 ⑨

# 决定性变异(临时改 + 还原)
cp AGENTS.md /tmp/AGENTS.bak
sed -i 's/140 份已归档/999 份已归档/' AGENTS.md
bash scripts/test/cep_docs_sync_check.sh --strict | grep '体量数字漂移'   # 期望:写 999,实测 140
cp /tmp/AGENTS.bak AGENTS.md

ctest --test-dir build -j4
for g in scripts/test/cep_*.sh; do bash "$g" >/dev/null || echo "FAIL $g"; done
bash scripts/build/format.sh --check
bash scripts/test/docs_sync_check.sh --strict
bash scripts/test/cep_doc_ref_check.sh --strict
python3 scripts/test/agents_md_lint.py

# 范围纪律
git diff --name-only c1180e7b..HEAD -- docs/adr
git diff --numstat c1180e7b..HEAD -- docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json
```
