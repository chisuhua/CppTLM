# 验收报告 · `cpptlm-cep-adr-verification-carriers`（追溯补验）

- **角色**：verifier
- **原归档未验**
- **追溯补验 · 2026-11-21**（归档日 2026-11-15；归档时 **T7.1 / T7.2 未勾** ⇒ 本报告为**独立追溯验收**，非原实现者自证）

> **范围**：验收对象 = 归档 change `openspec/changes/archive/2026-11-15-cpptlm-cep-adr-verification-carriers/` 的 **AC-01 ~ AC-10** 逐条 + **T7.1 / T7.2** 结论。
> ⚠️ **⏭ 本报告不判实现内容对错之外的语义**；⛔ 归档前的历史状态（当时的 `git diff` / 当时的计数）**不可复现** ⇒ 相关判据如实记为「不可判」。

## 判据来源

| 来源 | 位置 |
|---|---|
| 判据清单（AC-01~AC-10） | `openspec/changes/archive/2026-11-15-cpptlm-cep-adr-verification-carriers/proposal.md` §验收标准 |
| 未勾项（T7.1 / T7.2） | 同 change `tasks.md:82-83` |
| 交付物 | `scripts/test/cep_decision_block_check.py` · `scripts/test/cep_adr_verification_carriers_reverse_verify.sh` · `test/cep_decision_block_snapshot.json` · `include/tlm/cep/sharer_tracker.hh`（`kResponderMappedBits` / `kMappedResponderMask`）|
| 关联 ADR | `ADR-CEP-20`（决策块治理）· `ADR-CEP-21`（sharer bitmap 扩容，**阶段 2 未执行**）|

## 逐条判定

| # | 判据摘要 | 判定 | 工具证据 |
|:--:|---|:--:|---|
| AC-01 | 宽度常量被机器钉死；**反向**：毒化为 8 ⇒ 探针非零退出 | **通过（字面表述漂移）** | `include/tlm/cep/sharer_tracker.hh:92` `kResponderMappedBits = 4`；`:109` 现有 `static_assert(kResponderMappedBits <= 64)`。⚠️ 字面「`== 4u` 断言在位」**今日不在位**（只钉上界）；但**判别力由反向脚本的宽度毒化臂承担** ⇒ `bash scripts/test/cep_adr_verification_carriers_reverse_verify.sh` → `✅ ADR Verification 判据载体反向验证全部通过(8 项)`，rc=0 |
| AC-02 | 掩码数值锚有效（`kMappedResponderMask == 0xFu`）；毒化后实测 `mask=0xff` ⇒ 非零退出 | **通过** | 同上脚本的宽度/掩码毒化臂 rc=0；`sharer_tracker.hh:136` 掩码由 `kResponderMappedBits` 派生 |
| AC-03 | `ADR-CEP-20` 覆盖**可证**（非「零红」）：影子树删一个 `### 决策 N` 段 ⇒ 判红且归因含「决策块计数快照偏离」+「ADR-CEP-20」 | **通过** | reverse_verify 实跑输出：`✓ [覆盖毒化] 判红(退出码 1)+ 归因含「决策块计数快照偏离」与「ADR-CEP-20」`（已毒化 `### 决策 1 —— 拓扑级 E2E 装置的定位…`）|
| AC-04 | 快照自洽 + 授权存在：`count == len(files) == 23` · `total == sum == 131` · `files['ADR-CEP-20-…'] == 6` · 授权字段存在 | **通过** | `python3 -c "…test/cep_decision_block_snapshot.json"` ⇒ `len(files)=23 · total=131 · count=23 · files['ADR-CEP-20-topology-ic3-evidence-strength.md']=6 · authorized_by='Entry #431'` |
| AC-05 | `ADR-CEP-21` **不在**快照（负向断言） | **通过** | 同上 ⇒ `ADR-CEP-21 在快照内? False` |
| AC-06 | 持久 fixture 四态闭环 + **仓内零改动**：输出「未毒化绿 / 宽度毒化红 / 覆盖毒化红 / 恢复绿」；仓内文件未被改动 | **通过** | reverse_verify 实跑输出含 `✓ [恢复] 决策块闸绿` + `✓ include/tlm/cep/sharer_tracker.hh 未被改动` + `✓ test/cep_decision_block_snapshot.json 未被改动` + `✓ docs/adr/ADR-CEP-20-… 未被改动`；主闸 `python3 scripts/test/cep_decision_block_check.py` → `· 决策块计数合计 = 131(逐份 23 份)` + `✅ CEP 治理判据闸:全部判据通过`，rc=0 |
| AC-07 | 范围零改动（`git diff --name-only` 不含 `include/tlm/cep/` · `docs/adr/` · `cep_decision_block_check.py`） | **不可判** | ⚠️ change **已归档** ⇒ 「归档前的工作区 diff」**不可复现**（需 checkout 归档前 commit 才能取证）。当前只能确认**现状**：`sharer_tracker.hh` 现含 `kResponderMappedBits`（本 change 只承诺新增**判据载体**，不承诺不改该文件——正文 AC-07 的口径与归档后现状**不可直接比对**）|
| AC-08 | 0 回归：`[cep]` 用例数 **+1** · `ctest -R cep` 全绿 · ctest 条目数 **+1** · 43 个 `cep_*.sh` 全绿 | **部分通过 / 计数项不可判** | `ctest --test-dir build -N` → `Total Tests: 196`（AC 记的基线 43 个 `cep_*.sh` 已漂移为 **47** ⇒ **计数不可判**）；`ls scripts/test/cep_*.sh \| wc -l` = **47**；`bash scripts/test/cep_docs_sync_check.sh` rc=0 ✅ · `bash scripts/test/cep_spec_strict_check.sh` rc=0 ✅（闸套件现状绿）|
| AC-09 | 归档后逐份 `--strict` 不劣化 | **通过** | `openspec validate --specs --strict --json` ⇒ 总 items **138** · `cep-` 项 **47** · **`cep-` invalid = 0**（⚠️ 全仓非 `cep-` 有 55 份 invalid ⇒ 与本 change 无关，且本 change 未承诺修非 cep）|
| AC-10 | 归档五步链（Entry 先落 → 产物 commit → `--update --authorized-by` → 清单单独 commit → 无参复验 rc=0） | **通过（历史 commit 序列不可复现 ⇒ 部分不可判）** | `test/cep_decision_block_snapshot.json` 的 `authorized_by = "Entry #431"`（授权锚在位）；`bash scripts/test/cep_freeze_check.sh` 无参 rc=0（实测通过）⇒ **授权 + 无参复验**两端可证；「清单单独 commit」的历史形态不可复现 ⇒ 记不可判 |
| **T7.1** | verifier 独立验收（AC-01~AC-10 逐条 + 工具证据）⇒ 报告落 `.rddf/reviews/cpptlm-cep-adr-verification-carriers-verify.md` | **通过** | ✅ **本报告即 T7.1 的交付**；文件名与 `strip_date_prefix(archive_dir) + "-verify.md"` 逐字一致 |
| **T7.2** | 5 项残余状态确认 | **部分通过**（逐项见下） | ① F-1 全仓超长文档 ⇒ **已被别处收口**：`scripts/test/docs_sync_check.sh --strict` rc=0 + `scripts/test/agents_md_lint.py` rc=0（体量 ≤64 KiB + 单行 ≤1000 字符）⇒ 原「87/129 份超长」口径已过时 ② 镜像 20 值（ArchForge 侧）⇒ **不可判**（本环境无 ArchForge ⇒ `cep_decl_alignment_check.sh` 走 SKIP 块 `:38-42` ⇒ 零保护**确认仍成立**）③ 覆盖集合无自愈 ⇒ **确认仍成立**（`test/cep_decision_block_snapshot.json` 是**静态 fixture**，新增 ADR 决策块不会自动进快照 ⇒ 需手工更新）④ 编译期锁不可追 ⇒ **不追**（AC 明确「不追」）⑤ ArchForge 缺席零保护 ⇒ **确认仍成立**（同 ②）|

## 失败项与路由建议

| # | 失败 / 不可判项 | 路由 |
|:--:|---|---|
| F-1 | **AC-01 的「`== 4u` 编译期断言」今日不在位**（`sharer_tracker.hh:109` 只钉 `<= 64`）⇒ 「扩容到 8」时**编译期**不会拦，只有反向脚本会红 | ⛔ **不阻断本 change 的验收**（判别力由反向脚本承担）⇒ 建议**定向补**：在 `sharer_tracker.hh` 恢复 `static_assert(kResponderMappedBits == 4u, …)`（或把宽度钉死移进 `cep_decision_block_check.py` 的判据）—— 归 **I-11（spec 契约面 stale 引用修正）** 或新开 micro-ticket，⛔ **不并入本单元**（超出 AC 范围）|
| F-2 | AC-07（范围零改动）与 AC-08（计数 +1 / +1 / 43）在归档后**不可复现** | ⛔ 记为「不可判」⇒ **不可补**（历史状态不可回溯）⇒ 归入**制度面**：今后归档前须把「归档前的 diff 与计数基线」写进验收报告（**下一批归档 change 的报告模板**应含该两项）|
| F-3 | AC-01 的字面表述与现状漂移（见 F-1） | ⇒ **AC 措辞**与实现的漂移已被本报告记录；⛔ 不回改已归档 change 的 proposal（append-only 纪律）|

## 遗留清扫

| 项 | 结论 |
|---|---|
| 本次验收是否改动仓库 | ✅ **零改动**（全部取证为只读：`read` / `grep` / `shell` 只读命令；reverse_verify 脚本在**影子树**内毒化并自复原，且自带 sha256「仓内零改动」复核）|
| 临时诊断代码 | ✅ 无（未新增任何 `static FILE* diag` 之类） |
| 范围纪律 | ✅ 未触碰 `include/` · `docs/adr/` · `openspec/specs/` · 冻结清单 |

## 已用轮次

**1/3**（本 change 的追溯补验第 1 轮；无返工轮 —— 全部判据一次取证到位）。
