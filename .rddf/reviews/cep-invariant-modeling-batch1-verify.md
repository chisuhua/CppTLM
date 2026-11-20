# 验收报告 · `cep-invariant-modeling-batch1`（登记册队列 2 的 **I-5**）

- **验收对象**：登记册 §6.4 队列 2 的 **I-5** —— invariant 建模批 1（缺口 S1-1 批 1）· 判据文件 = 裁决记录 §2.4（Q-4）+ §8.2 C-1 / §9.2 SF-1~SF-3
- **验收角色**：builder 侧验收
- **日期**：2026-11-20
- **已用轮次**：**1/3**
- **判据来源**：单元 `## Acceptance` 的 **AC-1 ~ AC-12**
- **分流判定**：本单元**不创建** OpenSpec change（KPI 0/3）· P3 archive gate 未触发 ⇒ 不走 `rdd-verifier` skill ⇒ 本地验收

## 一、逐条判定

| AC | 判据 | 判定 | 证据 |
|---|---|:--:|---|
| **AC-1** | 闸 rc=0 且 `⭕-unmodelled` 5 → **2**；deferrals 长度 == 2 且 id == `INV-CEP-13-IC1` / `INV-CEP-13-IC3` | ✅ | `python3 scripts/test/cep_invariant_map_check.py --check` → `✓ 映射表闸通过: 66/66 = 64 项有牙载体 + 2 项 ⭕-unmodelled + 0 项 ⭕-pending`，rc=0；`deferrals` == `['INV-CEP-13-IC1','INV-CEP-13-IC3']` |
| **AC-2** | 映射表是生成物（重跑脚本后文件不变） | ✅ | 连续两次 `--generate` → sha256 **相同**（`7dd225fc…`）⇒ 文件 == 生成器输出，无手改残留 |
| **AC-3** | 3 个载体**各** 1 条决定性变异 ⇒ 按签名红 ⇒ 还原 ⇒ 绿 | ✅ | ① **CEN-Ib8**：禁用 engine 侧终结点记录 ⇒ `bad.recorded` **0 == 3** 失败 · `bad.violations` **4 == 1** 失败（8 passed / 4 failed）⇒ 还原后 12/12 passed<br>② **IC1**：把 J3 结果的 `instance` 归因改写 ⇒ `j3_for_point` **0 == 1** 失败 ⇒ 还原后 `[cep][ordering-point]` 76 assertions 全绿<br>③ **IC2**：影子副本植入含 `vc_arbitrat` 的桩 ⇒ 闸 rc=**1**（命中 2 处）⇒ 真实仓 rc=0 |
| **AC-4** | 台账锚真实存在且正文含 3 条 id；反 fixture 改 id ⇒ 红 | ✅ | **新增闸** `scripts/test/cep_invariant_batch1_ledger_check.py`（正控 rc=0：`Entry #512` 含 3 条 id + 4 个载体落点）<br>⚠️ **为什么必须新增闸**：映射表闸的判据 ④ 只校验**仍在 deferrals 里**的条目，而本批 3 条已按判据 ③（载体 ∩ ⭕ = ∅）移出 ⇒ ④ 对它们不再生效 ⇒ 无本闸则「台账已记录」只是自述<br>反 fixture：影子台账把 `Entry #512` 段内的 `INV-CEP-09-IC2` 全部改写 ⇒ rc=**1**（暴露出最新匹配 Entry 变成历史 `#447` 且不含载体落点）|
| **AC-5** | 零新增 invariant；66 集合仍 66；`INV-CEP-04-IC4` 仍不在载体中 | ✅ | 新增载体 id = `CEN-Ib8` / `INV-CEP-08-IC1` / `INV-CEP-09-IC2`；`test_cep_canonical_invariants.cc` 的 66 集合成员数仍 **66**；`INV-CEP-04-IC4` 在映射表中**不出现** |
| **AC-6** | deferrals 的改动**只含删除** | ✅ | `git diff --numstat` → **0 增 / 21 删**；`git diff -U0` 的非删除行数 == **0** |
| **AC-7** | 零越权 | ✅ | `git diff --name-only` 的禁改面命中 **0**（`docs/adr/` · `docs/architecture/` · `openspec/specs/` · `sharer_tracker.hh` · `protocol_engine_base.hh` 全空）|
| **AC-8** | 0 回归 | ✅ | `ctest` **194/194 passed** · `scripts/test/cep_*.sh` **46 个逐个 rc=0**（数量实测）· `rddf_stage_gate`（默认 / `--phase queue2` / `--self-test`）· `docs_sync --strict` · `agents_md_lint` · `config_type_registration_check` · `cep_freeze_check` 全 rc=0 · 工作区干净（提交后）|
| **AC-9** | 13 键仓内 golden + **双向**比对（单改一侧即红） | ✅ | `test/cep_instance_golden.json`（`moesi_gpu` / `mesif` × 13 键，从 `protocol_instances.json` 派生）+ `scripts/test/cep_instance_golden_check.py`（ctest 注册）<br>正控 → `✓ 13 键 golden ↔ 真源 双向一致: 2 实例 / 26 键`<br>反 fixture ①：只改 golden 的 `mesif.protocol_version` ⇒ `[golden→源] … 取值不等` rc=1<br>反 fixture ②：影子副本只改真源的 `moesi_gpu.status` ⇒ `[golden→源] … 取值不等` rc=1<br>⚠️ `_scope` 只含 `moesi_gpu` / `mesif`（二者**不在** `repo_owned_instance_ids`）⇒ ⛔ 未把 ArchForge 权威的 6 个纳入（纳入 = 造第二真源）|
| **AC-10** | 不改映射闸脚本、不引入第三态；偏离落点 = 载体注释 + 台账 | ✅ | `git diff` 对 `scripts/test/cep_invariant_map_check.py` == **0**；disposition 词表仍只有两态（脚本未动）；偏离「集合外不被覆盖」写在 `test/test_cep_ordering_point_checker.cc` 的载体注释 + `Entry #512` 正文（由 AC-4 闸的「⛔ 不得表述为已建模」子判据兜底）|
| **AC-11** | ⛔ 不给 `EventObservation` 加字段；用既有 `Kind`/`Origin` | ✅ | `include/tlm/cep/protocol_engine_base.hh` **零 diff**；engine 侧用 `record_transition(Origin::LocalRequest, …)`（`Kind = Transition`），`from == to`（不改状态）|
| **AC-12** | IC2 的 grep 模式写死且 ⛔ 不用 `Arbiter` | ✅ | 脚本内 `PATTERN='多 VC 仲裁\|vc_arbitrat\|VcArbitrat'`，扫描 `include/ src/ test/`；反 fixture 见 AC-3 ③<br>⚠️ 实现中踩到一次**自指误红**：`test/CMakeLists.txt` 注释里写了模式词 ⇒ 闸自红（1 处）；已改注释措辞（⛔ 未放宽扫描面）、并在注释里显式写明「本注释刻意回避模式词」|

## 二、处置的诚实边界（⚠️ 三条**不同质**，⛔ 均不得写成「已建模」）

| id | 终态 | 说明 |
|---|---|---|
| `CEN-Ib8` | **真建模** | engine 侧落终结点记录 + test 侧判定器（正控 3 次 / 负控精确 1 违例）|
| `INV-CEP-08-IC1` | **部分覆盖** | 只覆盖**已枚举**的 Ordering Point 集合；J3 通用形式在结构上不可静态判定（`ordering_point_checker.hh:11-12`）⇒ **集合外不被覆盖**，偏离已登记 |
| `INV-CEP-09-IC2` | **移出 ⭕ 登记，语义未达成** | 存在性反断言把「无可断言对象」变成可机检；一旦出现仲裁实现即红 ⇒ 强制重评。**不是达成** |

## 三、改动清单

| 文件 | 性质 | 改动 |
|---|---|---|
| `include/tlm/cep/mesif_protocol_engine.hh` | 改 | `on_address_resolve` 增 5 行：观测面开启时落 `Kind=Transition` + `Origin=LocalRequest` 的终结点记录（⛔ 不加字段）|
| `test/test_cep_invariant_cen_ib8.cc` | 新增 | `CEN-Ib8` 载体（判定器 + 正控/负控）|
| `test/test_cep_ordering_point_checker.cc` | 改 | 追加 `INV-CEP-08-IC1` 载体（枚举集合内逐点 J3 恰一次 + 多点退化 SKIP）|
| `scripts/test/cep_invariant_ic2_absent_check.sh` | 新增 | `INV-CEP-09-IC2` 存在性反断言载体 |
| `test/cep_invariant_assertion_map.json` | 改 | 脚本**重生成**（61 → 64 个 id）|
| `test/cep_invariant_deferrals.json` | 改 | 只删 3 条（0 增）|
| `test/cep_instance_golden.json` | 新增 | 13 键 golden（`moesi_gpu` / `mesif`）|
| `scripts/test/cep_instance_golden_check.py` | 新增 | 双向比对闸 |
| `scripts/test/cep_invariant_batch1_ledger_check.py` | 新增 | 台账锚可解析闸（AC-4 的反 fixture 需要）|
| `test/CMakeLists.txt` | 改 | 3 个 new ctest 注册 |
| `.rddf/roadmap/objectives/obj-cep-platform.md` | 改 | **append** `Entry #512` |

## 四、遗留清扫

| 检查项 | 结果 |
|---|---|
| 变异/反 fixture 的临时改动是否全部还原 | ✅ 3 处：`mesif_protocol_engine.hh`（`if (false)` → 0 命中）· `ordering_point_checker.hh`（`_mutated` → 0 命中）· 影子目录均在 `/tmp/opencode/` |
| ⚠️ **还原后是否重建** | ✅ 本轮踩到过一次：还原 `ordering_point_checker.hh` 后**未重建**即跑全量 ⇒ `cep_all` 假红（`j3_for_point 0 == 1`）；重建后 `[cep]` **542 cases / 12431 assertions 全绿** ⇒ 记此教训：**fixture 还原必须伴随重建** |
| 新增行中 TODO / FIXME / 临时日志 | **0** |
| 范围外历史遗留 | `format --check` 的 **21 个既有 C++ 文件**债仍在（与本单元开工前**同数**）；本单元**新增**的 `test/test_cep_invariant_cen_ib8.cc` 初次检查被判需格式化 ⇒ **已用 `clang-format -i -style=file` 格式化**，复查该文件为 `✓`、总数回到 **21**（= 既有债，非本单元引入）|

## 五、待办 / 风险

1. ✅ **已闭合**：`test/test_cep_invariant_cen_ib8.cc` 的格式化已在本轮完成（`format --check` 回复到 21 个**既有**债，该文件判 `✓`），格式化后重建 + 全量 ctest **194/194** 复验通过。
2. ⚠️ `CEP-09-IC2` 的存在性反断言会扫描 `test/` ⇒ 未来任何**注释/文档**里写该模式词都会自红；已在 CMake 注释里留警示。
3. 本单元**未**改 `test/cep_invariant_deferrals.json` 的 `_note`（仍写「⏕-pending 达成宣称必须为 0」等通用说明，不涉及条数）⇒ 无需改。

## 六、载体表 `state` 处置（**未标 done** · 与 I-9 同因）

I-5 按用户指示**乱序**执行（队列原序 `I-0 → I-1 → … → I-5`）。P-2a 是**严格串行**判据（任一已 done 项的前驱未 done ⇒ 红），而 `I-2`/`I-3`/`I-4` 仍未完成 ⇒ 当时标 `done` 会让 `--phase queue2` 与 `--self-test` 双红 ⇒ 暂留 `pending`。

### ✅ 已裁决（2026-11-20 · 用户选**路径 A**）

队列 2 的 canonical 顺序已重排为 `I-0 → I-1 → I-9 → I-5 → I-2 → …`（已完成项构成前缀）
+ 同步 `QUEUE2` 数组 ⇒ `I-5` 的载体表 `state` 已置 **`done`**，P-2a 下一项推进为 **I-2**，三路径全 rc=0。
详见 `.rddf/reviews/cep-archforge-ci-failclosed-verify.md` §六「已裁决」。

## 七、已用轮次

**1/3**（AC-3/AC-4/AC-9/AC-12 的反 fixture 均在轮 1 内完成并还原，无返工轮）。
