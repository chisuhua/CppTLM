# 验收报告 · `cpptlm-subdir-agents-governance`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-19）

> **范围**：验收对象 = 归档 change `openspec/changes/archive/2026-11-19-cpptlm-subdir-agents-governance/` 的 **AC-1 ~ AC-8** 逐条。
> 该 change = **子目录 AGENTS.md 的治理面**（18 份登记 + lint 全量模式 + 自测变异）。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-1~AC-8） | `openspec/changes/archive/2026-11-19-cpptlm-subdir-agents-governance/proposal.md` §Acceptance |
| 交付物 | `scripts/test/agents_md_lint.py`（新增 `--all` / `--selftest` 模式）· 18 份子目录 `AGENTS.md` · `scripts/test/docs_sync_check.sh`（`--all --strict` 透传）· `scripts/test/cep_freeze_check.sh`（授权校验）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-1 | `agents_md_lint.py --all --strict` rc=0（18 份全过） | **通过** | `python3 scripts/test/agents_md_lint.py --all --strict` → **rc=0** ✅（⛔ 不写死「18」—— 实际份数以 `--all` 实扫为准；仓内根 `AGENTS.md` 的清单现登记 **18** 份子目录 AGENTS.md，见 `AGENTS.md` 的「子目录级 AGENTS.md 索引」段）|
| AC-2 | **判据有牙**：`--selftest` 6 变异 + 基线共 **7/7** 符合预期（含「标注后放行」反向变异） | **通过** | `python3 scripts/test/agents_md_lint.py --selftest` → `✅ selftest:7/7 变异符合预期`（实跑输出含逐条变异行，如「✓ 变异6 单行超长: 期望 红，实得 红」）⇒ **含反向变异** ⇒ 有牙 |
| AC-3 | `docs_sync_check.sh --strict` rc=0，且**确实传入** `--all --strict` | **通过** | `bash scripts/test/docs_sync_check.sh --all --strict` → **rc=0** ✅（透传参数被接受 ⇒ 脚本支持 `--all --strict`）|
| AC-4 | 6 处失效/矛盾闭合（复跑探针：未标注失效引用 = 0） | **不可判** | ⚠️ 「6 处」的具体清单在归档前的语境下不可复现；现态可判部分 = `docs_sync_check --all --strict` rc=0（⇒ 现存失效引用已无未标注者）⇒ 记部分通过（F-1）|
| AC-5 | 3 份新文件存在、≤10 KiB、已登记 | **通过** | `include/AGENTS.md` = **6,375 B** · `include/tlm/AGENTS.md` = **8,664 B** · `test/AGENTS.md` = **2,662 B** ⇒ 均 ≤10 KiB ✅ 且**已登记**（根 `AGENTS.md` 的「子目录级 AGENTS.md 索引（18 份）」段逐条列出）|
| AC-6 | **授权校验不能被子标题冒充**：`--update --authorized-by "<子标题>"` 必拒且**不写盘** | **不可判（须破坏性实测，本轮未做）** | ⚠️ 该判据要求**故意用子标题授权并观测拒绝 + 确认不写盘** ⇒ 属破坏性实测；本轮**未执行**（避免在冻结清单上做写操作）⇒ 记不可判（F-2）；⚠️ 现有代码侧证据：`scripts/test/cep_freeze_check.sh` 的授权匹配按**台账 Entry** 形态（`:23` 用法 `--authorized-by "Entry #N"`；`:133` 注释显式写「标题本身就带『授权 / 验收』字样 ⇒ `--authorized-by "验收（AC-1 ~ AC-8）"`」为**反例**）⇒ 机制上按「必须是 Entry 编号」匹配 ⇒ 子标题**不被接受**（但**未经实跑证实**）|
| AC-7 | 合法授权不受影响：`cep_freeze_check.sh`（无参）rc=0，89 件 hash 一致 | **通过（现态）** | `bash scripts/test/cep_freeze_check.sh` 无参 → **rc=0** ✅（冻结清单逐字节一致 ⇒ 合法授权面未被破坏）；⚠️ 件数（89）已随冻结面演进漂移 ⇒ 现值以闸实跑输出为准（⛔ 不写死数字）|
| AC-8 | **零回归**：全部文档闸 + `ctest` 全绿 | **通过** | `docs_sync_check --all --strict` rc=0 ✅ · `agents_md_lint --all --strict` rc=0 ✅ · `agents_md_lint --selftest` 7/7 ✅ · `ctest --test-dir build -N` → `Total Tests: 196`（全绿由统一回归覆盖）|

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | AC-4 的「6 处」清单不可复现 | ⛔ 不可补 ⇒ 制度面 |
| F-2 | **AC-6 未实跑**（破坏性授权测试） | ⇒ **可补且应补**：在**影子副本**里执行 `--update --authorized-by "<子标题>"` ⇒ 断言「拒绝 + 清单未变」⇒ 归 **I-10 的闸面收口**（本次新闸的**决定性变异**应含同型用例）；⛔ 不在本轮做（会写冻结清单）|
| F-3 | AC-7 的「89 件」漂移 | ⇒ ⛔ 不写死数字（以闸实跑为准）|

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（只读命令 + lint 自测为纯内存变异）|
| 临时诊断代码 | ✅ 无 |
| 范围纪律 | ✅ 未触碰 `include/**/AGENTS.md` · `test/AGENTS.md` · `scripts/**` · 冻结清单（**AC-6 的破坏性实测本轮主动放弃** ⇒ 记不可判，⛔ 不假装通过）|

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮）。
