# C4 验收报告 —— `cpptlm-cep-dod-reading-consistency`

> **判据来源（按优先级）**
> 1. 用户口令「按 C3 的同一条链继续」（= 实施 → 五步链 → 归档 → 切 verifier 验收）
> 2. `openspec/changes/archive/2026-11-12-cpptlm-cep-dod-reading-consistency/proposal.md` 的 **15 条 AC**（= improvement 同源转录）
> 3. 本 change 自述（`design.md` §D-1~§D-10 + 台账 Entry #373/#374/#375）
> 4. 台账 **Entry #340**（范围纪律）· **#332 ⑧**（MUST-FIX-1/2 原文）· **#359 ⑤**（判据四行）
>
> **验收轮次**：**第 1 轮**（`RDDF_VERIFIER_MAX_LOOPS=3`）
> **结论**：**PASS** —— 15 条 AC **全部满足**；**0 BLOCKER**；**1 条 `proposal_drift`（非阻断，见 §四）**；**5 条已登记的实测订正**（见 §五）
> **角色边界**：本报告**未修改任何实现**（只读 + 运行验证命令）。

---

## 一 · 逐条判定（每条均有工具证据）

### A 组 · 锚点与文本（AC-1 ~ AC-6）

| AC | 判定 | 证据（命令 → 实测） |
|---|:--:|---|
| **AC-1** 基线正控（两半） | ✅ | 基线 = `390076d0`（P0b，实施前）逐行全等比对：`a1`(obj)=**1** · `a2`/`a4`(07 号)=**0**/`0` · `a3`(手册)=**0** · `a3_old`(手册)=**1** ⇒ 与 design §D-3 的「变更前」列**逐项相符**；新文本变更前 0 命中 ⇒ 「变更后恰 1」不是撞既有文字 |
| **AC-2** 手册重裁 · 逐字可核 | ⚠️ **PASS（实质）** | 旧/新文**逐字**登记在 `tasks.md` 的「A6 逐字登记」段；同段 1/2/4 三条 + 末句「⛔ 不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文」**逐字未变**（old/new 双侧 `in` 判定全 ✔）。⚠️ **1 处 `proposal_drift`**：AC-2 括注的「口径 ④ **未 ratify**」半句**旧文并无此句** ⇒ 它是**新增**（per improvement In-Scope ② 的「保留」= 保留该事实、非逐字延续）⇒ 见 §四 |
| **AC-3** 台账取代指针 · append-only | ✅ | `§6.2` 禁令行**逐字保留**（A1 整行命中 1，行号 554）+ **紧随其后**为 C4 子 bullet（`lstrip().startswith('- 📌 **[C4 · MUST-FIX-2]')` = True）；全链 `git diff --numstat 136a3cd2..HEAD -- obj` = **167 增 / 2 删**，2 处删除**逐条可归属**：① KPI 活口径行（AC-13 授权）② `§13.9` 看板行（台账纪律「§13.9 可更新」）⇒ 口径细化已登记（Entry #375 §⑥'） |
| **AC-4** MUST-FIX-1 · 排期值不动 | ✅ | `§4.0` 区间（49 行）**逐行相等**（基线 vs 当前）；`§4.5` / `§4.6` 各 **+1 行 / −0 行**（纯新增 bullet）；新增文本内 `2030-Q1` 字面 **0 次**（5 个提取块逐一 `grep -c`）；外部 grep（`.rddf/` + `docs/` 排除 `obj` 与 `docs/implementation/`）= **13**，与基线 **13** 相等 ⇒ ⛔ 未触发「五处同改」 |
| **AC-5** SU 落位 + 索引 | ✅ | SU 标题行 **600** < 索引标题行 **637**（`## §末 文档索引`）⇒ 插在索引**之前**；`generate_doc_index.py --check` **rc=0**；`cep_index_position_check.sh` **rc=0**；自述 `30 个一级节 / 17 则 Status Update` **== 实际 30/17** |
| **AC-6** 「恰 1 命中」 | ✅ | 闸 S2 实测：A1(**obj**) / A2(**07 号**) / A3(**手册**) / A4(**07 号**) **各命中 1 处、文件数 1、行次数 1**；A3_OLD **0** 命中；判据表达 = **闸内常量**（由 design 的 fenced block 提取注入，⛔ 手抄） |

### B 组 · 闸有牙（AC-7 ~ AC-9）

| AC | 判定 | 证据 |
|---|:--:|---|
| **AC-7** 5 条决定性变异 | ✅ | `bash scripts/test/cep_dod_reading_check.sh` ⇒ **22/22**：M1 `新文本非唯一命中` · M2 `旧文本仍存在` · M3 `作用域子句缺失`（**保留禁令句**、只删作用域句）· M4 `绝对禁令复活` · M5 `append-only 违规`；每条**还原后绿**；末项「真实仓 3 份语料**字节指纹前后相等**」✅ |
| **AC-8** 禁表述 5 项 + 作用域要素 | ✅ | 闸 S6：SU 块 + 手册行 + 台账 `[C4 · …]` 行内 5 条禁表述 **0 命中**；S4：A2 行的 5 个作用域要素**全含**（`不带 N/M 分解与逐族锚点` / `N = 0` / `⏭` / `整行改写` / `不适用`）；5 条禁表述**各 1 条反例变异** ⇒ 全部 `禁表述复活` 红后还原绿 |
| **AC-9** 指针三元组 + 单向闭合 | ✅ | ① 目标标题 `## Status Update 2026-11-13` 在 `07 号` **恰 1**；② 手册第 3 条含该 token；③ 台账 `[C4 · MUST-FIX-2]` bullet 含该 token；④ 反向：`07 号` 全文无「口径 ④ 已生效」；⑤ 并列权威链检查：`07 号` 内 `不得改写为「已达成」` 仅 **1 行**（§③ 的片段引用行，含作用域 token）⇒ 无第二条并列权威链。⚠️ 原 AC-9 ⑤ 的前提（「`obj:552` 指向已失效的 `07 号:1262`」）**实测不成立**，见 §五-① |

### C 组 · 回归 / 冻结链 / 归档（AC-10 ~ AC-15）

| AC | 判定 | 证据 |
|---|:--:|---|
| **AC-10** 0 回归 | ✅ | `ctest -j4` **172/172**（新用例 `cep_dod_reading_check` **4.62s**）；`scripts/test/cep_*.sh` **39/39 rc=0**；`format.sh --check` rc=0；`docs_sync_check.sh --strict` rc=0；`agents_md_lint.py` rc=0 |
| **AC-11** 冻结链 · 1 件 + 提交粒度 | ✅ | 冻结面 **1 处**（`07 号`）；`--update --authorized-by "Entry #374"` ⇒ commit **`1724e505`** 的清单 = **79 产物** / `authorized_by = Entry #374`；无参复验 **rc=0**；提交粒度 = 台账 Entry(`15ec8d6f`) → 产物(`cede8643`) → 清单(`1724e505`) **三次分离**。⚠️ 见 §五-③ 的顺序告警 |
| **AC-12** 归档 + 第二次 `--update` | ✅ | `openspec archive --yes` rc=0（`+1 added` 合并进 `openspec/specs/cep-dod-reading-consistency/spec.md`）；归档后**无条件**第二次 `--update` ⇒ commit **`ba1ca3de`** 的清单 = **80 产物**（`openspec/specs/cep-*` glob **39** 份）；无参复验 **rc=0**（provenance = `ba1ca3de`） |
| **AC-13** KPI 回填（7 处） | ✅ | `openspec list` ⇒ **No active changes found**；7 处活口径全为 **0/3**：`AGENTS.md`(1) · `.rddf/roadmap.md`(1) · `phases/phase-10.md`(1) · `cep-umbrella.md`(1) · `cep-nvlink-sharp.md`(**2**) · `obj` 的 `§5.2` 活口径段(1)；`cep_docs_sync_check --strict` 的 KPI 断言 **rc=0** |
| **AC-14** 范围纪律 | ✅ | `git diff --stat 136a3cd2..HEAD` = **16 文件 / +713 / −25**（删除行全部可归属：7 处 KPI 行、1 行 §13.9、4 行 07 号索引结构、`role.json` 格式化、清单重算）；`docs/adr` **0 变更**（未新建/未改 ADR）；`B3b-2` 行**仍 0/4**；`Entry #249` ⑤ **仍存在**；`07 号` 内「口径 ④ 已 ratify」**0 命中** |
| **AC-15** 新 SU 引用形态 | ✅ | SU 段内 `§N` 引用 **6 处，全部**形如 台账 `objective-cep-platform` 的 §4.5/§4.6/§6.2（带路径前缀）；剥去该前缀后**残留裸 `§N` = 0**；`cep_doc_ref_check.sh --strict` **rc=0** |

---

## 二 · 判据有牙性复核（不只看 rc）

| 判据 | 是否只靠 `rc≠0` | 反证 |
|---|:--:|---|
| 4 条锚唯一性 | 否 | 变异 M1（复制到第 2 文件）⇒ 按 `新文本非唯一命中` 红；M5（行内追加）⇒ 按 `append-only 违规` 红 |
| 作用域被咬住 | 否 | M3（只删作用域句、保留禁令句）⇒ `作用域子句缺失`；M4（植入无作用域绝对禁令）⇒ `绝对禁令复活` ⇒ 证明闸咬的是**作用域**而非「整句存在」 |
| 禁表述 | 否 | 5 条各 1 条反例变异 ⇒ 全部 `禁表述复活` |

---

## 三 · 遗留清扫复核（本次范围内的自查项）

| 项 | 实测 |
|---|---|
| `TODO` / `FIXME` / `XXX`（2 个新增文件） | **0** |
| 被注释掉的代码 | **0** |
| `skip` / 跳过的测试 | **0** |
| 临时日志 / 调试输出残留 | **0**（`/tmp` 日志只出现在 `.sh` 的 `LOG=` 变量，属变异取证载具，非残留） |
| 临时调试分支（`pdb` / `breakpoint()`） | **0** |
| 未替换的 mock / 临时 fallback | **0**（闸无 mock；单根语义是**设计选择**，已在 design §D-7 登记） |

---

## 四 · 失败项与路由（**0 BLOCKER**）

| # | 项 | 分类 | 路由 | 说明 |
|:--:|---|---|---|---|
| 1 | **AC-2** 括注的「口径 ④ **未 ratify**」半句在旧文中**不存在**（是本次**新增**） | `proposal_drift` | **planner**（后续订正 AC 文本，⛔ 不阻断本次交付） | 实质要求（手册只写指针 + 保留未 ratify 事实 + 其余半句逐字未变）**已满足**；偏差在**判据描述**而非实现。⛔ 不在本 change 内改已归档 proposal（归档件不回改）⇒ 建议以 improvement 的下一版或 T3 批次订正 |

---

## 五 · 已登记的实测订正（⛔ 非缺陷掩盖，均可在仓内复核）

| # | 订正 | 登记处 |
|:--:|---|---|
| ① | **AC-9 ⑤ 的前提不成立**：`obj` §6.2 引用的是 `07 号` 的 `Status Update 2026-11-04 ①`，该 SU **存活**且其 ① 自述「现行载体即台账 §6.2」⇒ 该环**本来就闭合**；「改指」落地为「新增取代指针 + 断言环闭合」，⛔ 未改 A1 行 | design §D-10 第 6 条 · Entry #373 ⑤ |
| ② | **AC-4 ④ 的基线 9 → 13**：9 是 **P0a 落盘前**实测，improvement 文件落盘后自身贡献 7 行 ⇒ 真实基线 13；本 change 新增 **0 行** ⇒ 「相对快照相等」成立（判据**未放宽**，仅订正记录值） | design §D-3 / §D-10 第 7 条 · Entry #375 ③ |
| ③ | **冻结链顺序**：`--update` 在产物 commit **之前**跑会报「provenance 不成立」但**退出码仍为 0** ⇒ 易漏读；正确顺序 = 产物 commit → `--update` → 清单 commit | Entry #375 ② |
| ④ | **SU 日期标识**：`Status Update 2026-11-13` = **立项计划日**；实际落地/归档 = **2026-11-12**（`openspec` 归档目录前缀即 `2026-11-12-`）。⛔ 不回改标识（它是 A3 的逐字节组成 + 已提交 Entry/#AC 文本一致；台账 append-only 禁止回改 Entry）⇒ 登记而非改；真错（`role.json` 的未来时间戳）**已订正** | Entry #375 ④ |
| ⑤ | **AC-3 的删除行口径**：初稿「全文件零 `-` 行」是**过宽代理**（KPI 行与 §13.9 看板行按定义须就地更新）⇒ 细化为「只允许这两类，`§4.0`/`§4.5`/`§4.6`/`§6.2` 零删除」；禁改面的真正载体 A1 由闸的整行等值独立守住 | Entry #375 §⑥' · design(归档件) §D-9 |

---

## 六 · 已用轮次

| 轮 | 结论 | 说明 |
|:--:|---|---|
| **1（本轮）** | **PASS** | 15/15 AC 满足（AC-2 带 1 条 `proposal_drift` 注）；0 BLOCKER；无需返工 |

**是否打回**：**否**。唯一 finding 是 `proposal_drift`（判据描述偏差），按规则路由 planner，**不进 build 返工队列**。

---

## 七 · 复核命令清单（可原样重跑）

```bash
# 判据本体(7 组判据 + 10 条变异)
bash scripts/test/cep_dod_reading_check.sh

# 0 回归
ctest --test-dir build -j4
for g in scripts/test/cep_*.sh; do bash "$g" >/dev/null || echo "FAIL $g"; done
bash scripts/build/format.sh --check
bash scripts/test/docs_sync_check.sh --strict
python3 scripts/test/agents_md_lint.py

# 索引 / 引用 / 冻结面
python3 scripts/cep/generate_doc_index.py --check
bash scripts/test/cep_index_position_check.sh
bash scripts/test/cep_doc_ref_check.sh --strict
bash scripts/test/cep_freeze_check.sh

# 范围纪律(删除行归属)
git diff --numstat 136a3cd2..HEAD
git diff -U0 136a3cd2..HEAD -- .rddf/roadmap/objectives/obj-cep-platform.md | grep '^-[^-]'
```
