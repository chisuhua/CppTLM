# tasks —— `cpptlm-cep-dod-reading-consistency`

> 顺序 = **A0(授权门) → A1(锚文本提取器) → A2(闸实现) → A3(10 条变异) → A4(ctest + 守门表) → A5(`07 号` 新 SU + 索引重生成) → A6(手册 + 台账) → A7(冻结链四拍) → A8(delta) → A9(0 回归) → A10(归档 + 第二次 `--update`) → A11(KPI 回填)**。
> 门:**G1 = A0** · **G2 = A3**(闸有牙)· **G3 = A5**(冻结件 SU 落盘)· **G4 = A6**(指针 + append-only)· **G5 = A7**(冻结链闭合)· **G6 = A9**(0 回归)· **G7 = A10**(归档)。
> 里程碑:**M1 锚单源就位 → M2 闸有牙 → M3 冻结件落盘 → M4 指针闭合 → M5 冻结链闭合 → M6 归档**。

> ⚠️ **锚文本单源纪律**(design §D-2 / §D-8 / 铁律 1):`07 号` 新 SU 正文、手册新行、台账 3 条 bullet、闸的 4 条常量**一律由脚本从 `design.md` 的 fenced block 提取后注入** ⇒ ⛔ 手抄(`A2` 长 168 字,手抄必然漂移 ⇒ AC-6 首日红)。
> ⚠️ **AC 逐条映射见 `proposal.md`「验收标准」段**(15 条 · 与 improvement 同源);本文件只记**步骤与取证**,⛔ 不重述判据。
> ⚠️ **实施顺序的依赖倒置**:闸断言的是**最终文本** ⇒ 必须先落文本(A5/A6)、再以文本为准提取常量(A1/A2)。故 A1 的「提取器」先就位(它读 design.md,不读目标文件),闸在 A5/A6 之后才可能绿。

---

## A0 · 授权门(G1)

- [ ] A0.1 台账 Entry 授权本 change 的实施(占 **1 个 Proposed 槽位**,KPI **0/3 → 1/3**)
      > ⚠️ **授权号 = 本 change 的 builder Entry**（A7.3 的 `--authorized-by` 指向它）⇒ ⛔ 不用 `#372`（那是 P0a 立项）
- [ ] A0.2 `.rddf/improvements/cpptlm-cep-dod-reading-consistency.md` 已落盘 + **HARD-GATE rc=0**（per Entry #372 ①）
- [ ] A0.3 `.rddf/state/.planner-handoff.json` 已由 **canonical 写者**产出（`awaiting_builder=["add-cpptlm-cep-dod-reading-consistency"]` / `recommended_route=complex`）
- [ ] A0.4 基线与 design §D-3 的「变更前」列**逐项相符**（A1=1 / A2=A3=A4=0 / A3_old=1 / 外部 grep=**13**(见 design §D-3 的实测订正)）
- [ ] A0.5 `git status` 仅本 change 的产物 + A0.1 的台账 Entry

## A1 · 锚文本提取器（per design §D-2 / §D-8 / 铁律 1）

- [ ] A1.1 写一次性提取脚本（`/tmp/opencode/` 内，⛔ 不入仓）：按 `<!-- C4-A1 -->` / `C4-A2` / `C4-A3` / `C4-A3-OLD` / `C4-A4` / `C4-SU` / `C4-OBJ45` / `C4-OBJ46` / `C4-OBJ62` 九个 marker 取**紧随其后的 fenced block**（fence 内逐行原样，含前导空格）
- [ ] A1.2 提取结果**逐条断言**：A1/A2/A3/A3_old/A4 **各恰 1 行**；SU / 3 条 bullet 非空；提取出的 A1 与 `obj` 现行第 `§6.2` 禁令行**逐字节相等**（⇒ 证明 design 里的 A1 确实是既有行，⛔ 不是新写的）
- [ ] A1.3 提取结果落 `/tmp/opencode/c4_anchors/`（`a1.txt` … `obj62.txt`）供 A2 / A5 / A6 复用

## A2 · 闸实现（per design §D-1 / §D-4 / §D-5）

- [ ] A2.1 `scripts/test/cep_dod_reading_check.py`：`--root` 解析（默认 = 自身位置推导的仓根）+ 语料扫描（`docs/**.md` + `.rddf/roadmap/**.md` + `test/**` + `scripts/**`，扩展集见 design §D-3）+ 头注写明「期望值载体不是语料」
- [ ] A2.2 常量注入：A1 / A2 / A3 / A3_old / A4 **逐字节**来自 A1.3 的提取结果（⛔ 手抄；形式 = `NAME = "..."` ⇒ 整行 ≠ 锚文本，不构成第二命中）
- [ ] A2.3 **S1**（A3_old 零命中）· **S2**（4 锚唯一性）· **S3**（缺失归因：`append-only 违规` / `作用域子句缺失` / `新文本缺失`）
- [ ] A2.4 **S4**（作用域 5 要素）· **S5**（绝对禁令复活）· **S6**（5 条禁表述）· **S7**（指针目标 + 取代指针）
- [ ] A2.5 输出契约：`FAIL <check_id> <签名> :: <细节>`；退出码 `0/1/2` 三态（`2` = 语料缺失，⛔ 与 `1` 混用）
- [ ] A2.6 `scripts/test/cep_dod_reading_check.sh`：`source cep_test_shadow.sh` + `cep_shadow_reset "$SHADOW" <3 份语料>` ⇒ 复用既有 helper，⛔ 自造复制逻辑
- [ ] A2.7 **基线正控**（AC-1）：`bash scripts/test/cep_dod_reading_check.sh` **rc=0**（真实仓）

## A3 · 10 条决定性变异（per design §D-6 · AC-7 / AC-8③）

- [ ] A3.1 **M1**（A2 复制到第 2 文件）⇒ 签名 `新文本非唯一命中` ⇒ 还原 ⇒ 绿
- [ ] A3.2 **M2**（手册还原 A3_old）⇒ 签名 `旧文本仍存在` ⇒ 还原 ⇒ 绿
- [ ] A3.3 **M3**（删 SU ② 的 A2 行）⇒ 签名 `作用域子句缺失` ⇒ 还原 ⇒ 绿
- [ ] A3.4 **M4**（植入无作用域的绝对禁令）⇒ 签名 `绝对禁令复活` ⇒ 还原 ⇒ 绿
- [ ] A3.5 **M5**（A1 行内追加）⇒ 签名 `append-only 违规` ⇒ 还原 ⇒ 绿
- [ ] A3.6 **M6~M10**（5 条禁表述各 1 条反例）⇒ 签名 `禁表述复活` ×5 ⇒ 逐条还原 ⇒ 绿
- [ ] A3.7 全部变异**只作用于影子树**；跑闸前后真实仓 `git status --porcelain` 无变化
- [ ] A3.8 断言方式 = **签名匹配**（⛔ 只断言 `rc≠0`）

## A4 · ctest 注册 + 守门表（per design §D-1 · Impact）

- [ ] A4.1 `test/CMakeLists.txt` 加 `add_test(NAME cep_dod_reading_check COMMAND bash .../cep_dod_reading_check.sh)` ⇒ `ctest -N` 用例 **171 → 172** 且见新名
- [ ] A4.2 `scripts/README.md` 守门表 **+1 行**（`cep_dod_reading_check.sh` + 其 4 锚/10 变异的一句话摘要）

## A5 · `07 号` 新 SU + 索引重生成（G3 · AC-5 · 冻结件）

- [ ] A5.1 从 `design.md` 的 `<!-- C4-SU -->` 提取 SU 全文，**逐字节**插入 `07 号` 的 `## §末 文档索引` **之前**
      > ⛔ 不改任何既有正文 / 既有 SU；⛔ 不含 `### 决策 ` 标题（本 change 不动 ADR ⇒ ADR 计数不受影响）
- [ ] A5.2 `python3 scripts/cep/generate_doc_index.py --write` ⇒ `--check` **rc=0**
- [ ] A5.3 `bash scripts/test/cep_index_position_check.sh` **rc=0** + 自述由 **29/16** 变 **30/17**
- [ ] A5.4 AC-15 自查：新 SU 内台账节号引用**全部带路径前缀**；`bash scripts/test/cep_doc_ref_check.sh` rc=0

## A6 · 手册 + 台账（G4 · AC-2 / AC-3 / AC-4）

- [ ] A6.1 `docs/guide/cep-usage.md` 的「四条禁写」第 3 条**原地替换**为 A3（`tasks.md` 的 A6 段逐字登记旧/新文供 AC-2 核对）
- [ ] A6.2 **逐字未变**断言：同段 1/2/4 三条 + 该条内「⛔ 不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文」与「口径 ④ **未 ratify**」
- [ ] A6.3 台账 `§4.5` 尾**新增** `C4-OBJ45` bullet；`§4.6` 尾**新增** `C4-OBJ46` bullet（⛔ 标题与值行逐字不动）
- [ ] A6.4 台账 `§6.2` 禁令行之后**新增子 bullet** `C4-OBJ62`（⛔ 不碰 A1 行）
- [ ] A6.5 AC-3 取证：`git diff -U0 -- .rddf/roadmap/objectives/obj-cep-platform.md` **零 `-` 行**
- [ ] A6.6 AC-4 取证：`§4.0` 字段级 diff **0 行**；`2030-Q1|2028-Q4` 外部 grep **仍 13 行**；新文本内 `2030-Q1` 字面 **0 次**

### A6 逐字登记（AC-2 的判据载体 · ⛔ 本节即「逐字可核」的证据）

**旧文**（= `docs/guide/cep-usage.md` 第 3 条**变更前**逐字，来自 design `C4-A3-OLD`）：

```text
3. ⛔ **不得**把 B3b-2 整行改写为「已达成」—— 「DoD 降格后仍宣称达成」是明确禁止的失败模式。⚠️ **本条作用域待重裁**(per 台账 Entry #332 ⑧ MUST-FIX-2 ①):重裁后禁的是「**不带 N/M 分解与逐族锚点的 ✅**」;**重裁落地前按现行绝对禁令执行** ⇒ ⛔ 重裁前**不得**以任何形式登记为达成。⛔ **本条最终口径以「口径 ④ 本体新 SU」(Entry #332 ⑪ 落点)为准**;新 SU 落地后本条**改为指针**,⛔ **不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文**(per 台账 Entry #264 / Entry #284 的先例形态)
```

**新文**（= 同一条**变更后**逐字，来自 design `C4-A3`）：

```text
3. ⛔ **不得**把 B3b-2 整行改写为「已达成」—— 「DoD 降格后仍宣称达成」是明确禁止的失败模式。📌 **本条作用域已重裁**(per 台账 Entry #373):作用域与例外**见 `07 号` 的 `Status Update 2026-11-13` ②**(⛔ 本行不复制作用域定义)。⚠️ 现行登记口径仍 = 口径 ②(挂起);口径 ④ **未 ratify**。⛔ **不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文**(per 台账 Entry #264 / Entry #284 的先例形态)
```

**逐字 diff 的结论**：① 前 66 字（禁令句 + 「DoD 降格后仍宣称达成」）**逐字相同**；② 「⚠️ 本条作用域待重裁 … 重裁落地前按现行绝对禁令执行 …」整段**被指针句取代**（该段正是「待重裁」状态标记 ⇒ 落地后必须消失）；③ 末句「⛔ 不回改 `07 号` 的 `Status Update 2026-11-04` ① 段原文」**逐字保留**；④ 新增「口径 ④ **未 ratify**」半句（保留原「未 ratify」事实，⛔ 不新增任何「已生效」表述）；⑤ 悬空指针「口径 ④ 本体新 SU（Entry #332 ⑪ 落点）」**被删除**（该 SU 从未落地）。

## A7 · 冻结链四拍（G5 · AC-11）

- [ ] A7.1 拍 0：落 **builder 自己的 C4 台账 Entry**（含 A0.4 基线、A3 的 10 条变异取证、A5/A6 的落盘记录）⇒ **commit**
- [ ] A7.2 拍 1+2：`07 号` 已是 A5 产物 ⇒ **commit 产物**（`docs/architecture/07-*.md` + 手册 + test/scripts 若同批）
- [ ] A7.3 拍 3：`bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`（**同一次**;⛔ 分多次）
- [ ] A7.4 拍 4：**commit 清单**（与产物 commit **分离**）
- [ ] A7.5 **无参**复验：`bash scripts/test/cep_freeze_check.sh` **rc=0**；`authorized_by` 精确匹配；`artifact_count` **仍 = 79**

## A8 · 契约 delta（AC-12 前半）

- [ ] A8.1 `specs/cep-dod-reading-consistency/spec.md`：**ADDED** capability，**仅 1 条文本 Requirement**，且**只写指针**（⛔ 不复写作用域句 —— 复写会让权威句从 1 处变多处，与 G6 本体相反）
- [ ] A8.2 `openspec validate cpptlm-cep-dod-reading-consistency --strict` **rc=0**（单份 strict-valid）

## A9 · 0 回归（G6 · AC-10）

- [ ] A9.1 `ctest --test-dir build -j4` **172/172**
- [ ] A9.2 **39** 个 `scripts/test/cep_*.sh` 逐个 rc=0
- [ ] A9.3 `./scripts/build/format.sh --check` rc=0
- [ ] A9.4 `bash scripts/test/docs_sync_check.sh --strict` rc=0
- [ ] A9.5 `python3 scripts/test/agents_md_lint.py` rc=0

## A10 · 归档 + 第二次 `--update`（G7 · AC-12）

- [ ] A10.1 `openspec archive cpptlm-cep-dod-reading-consistency`（✅ **已完成归档**语义 —— 实现已落地 ⇒ delta **合并**进 `openspec/specs/`）
- [ ] A10.2 **无条件**第二次 `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"` ⇒ `artifact_count` **79 → 80** + commit 清单
- [ ] A10.3 无参复验 rc=0
- [ ] A10.4 归档件 `tasks.md` 回填：**逐项核对实际文本**后再勾（⚠️ C3 踩过：`old` 文本与实际不符 ⇒ assert 中止 ⇒ 整体未生效）

## A11 · KPI 回填（AC-13 · 枚举 7 处）

- [ ] A11.1 `AGENTS.md` KPI 基线行（**1/3 → 0/3** + 归档日期 + C4 名）
- [ ] A11.2 `.rddf/roadmap.md`
- [ ] A11.3 `.rddf/roadmap/phases/phase-10.md`
- [ ] A11.4 `obj §5.2` 活口径段
- [ ] A11.5 `.rddf/improvements/cpptlm-cep-umbrella.md`
- [ ] A11.6 `.rddf/improvements/cpptlm-cep-nvlink-sharp.md`（**2 处**）
- [ ] A11.7 `bash scripts/test/cep_docs_sync_check.sh --strict` **rc=0**（KPI 断言一致）

---

## ⛔ 明确不做

- ⛔ MUST-FIX-3（objective 归档形态）/ MUST-FIX-4（回滚路径）—— 待用户 ratify
- ⛔ 改 `B3b-2` 计数 / 缩分母 / 写「✅(⏭)」/ 用「永久冻结」措辞
- ⛔ 新建 ADR / 改任何 ADR 正文
- ⛔ 改 `§4.0` 排期值 ⇒ 不触发「五处同改」
- ⛔ 回改 `Entry #249` ⑤ 五选项表 / `07 号` 的 `Status Update 2026-11-04` ① 段原文
- ⛔ 改台账 §13.1~§13.8
- ⛔ 顺手修 `§4.0` 自检 grep ① 的预存漂移（基线 9 行已脏，只作相对快照）
