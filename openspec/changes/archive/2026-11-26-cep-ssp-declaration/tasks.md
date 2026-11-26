## Tasks

> **判据纪律**：每条 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」；⛔ 「rc≠0」不构成红因。
> **回归口径**：**零回归 = 全量 ctest（⛔ 不按 `-R` 过滤）+ 全部 `cep_*.sh` + 非 `cep` 只读闸 + `docs_sync --strict`**。
> **⚠️ 交付边界**：本 change **只做批 1**（声明面 + loader + 逐格对账）。⛔ 解释器 / 装配 / 掩码格 / 实验 4 件套 ⇒ 批 2 另立。

### Task 0: 授权（先于改冻结面）

- [x] 台账 Entry **#553**（立项 + 冻结面授权 · 前置 · 改动面 · 五步链预测 · 计数面 · 不做项）
- [x] `openspec/changes/cep-ssp-declaration/proposal.md`（planner 域 · A-1~A-14）
- [x] `.openspec.yaml` + `design.md`（builder 域 · D-1~D-6）

### Task 1: 事件字母表契约（`include/tlm/cep/event_alphabet.hh`）

- [x] `CoreAnchor` 枚举（**14** 核心锚）
- [x] `AnchorInfo { anchor, name, flow, remote }` + `kCoreAnchors[14]`（逐行 flow 映射）
- [x] `kCoreAnchorCount = 14` + 两条 `static_assert`（`size == 14` · `kCount == 14`）
- [x] `is_core_anchor_name` / `find_core_anchor` / `flow_of`（fail-closed：未命中 ⇒ `nullptr`）
- [x] 3 个诚实边界常量（`kFlowMappingIsPartial` / `kCoreAnchorSetIsClosed` / `kPrivateEventsNotModelledHere`）
- [x] 通用层**零协议名**（⛔ 无 `MESIF` / `TileLink` / `CXL` … 字面）

### Task 2: SSP 声明面（`ssp.schema.json` + `ssps.json`）

- [x] `ssp.schema.json`（draft-07 · `event` 的 anchor/flow/architect_approval 条件 · `perm` 4 值枚举 · basis 行）
- [x] `ssps.json` 的 `mesif_ssp`（**逐格转写** `mesif_state_spec.hh` 的 `kTable`：5×6 transition + perm + 35 格 basis 的 kind/pending）
- [x] `JSON + 同名 schema` 成对（`AGENTS.md` 铁律 3）

### Task 3: loader（`include/tlm/cep/ssp_loader.hh`）

- [x] `check_ssp_document`（fail-closed · 红因码 `E1`~`E12`）
- [x] `kSspSchemaLiteral` / `kSspFlowNames[10]` / `kSspPermNames[4]` / `kSspBasisKindNames[4]`
- [x] ⛔ 不含协议名（通用层）

### Task 4: 测试（`test/test_cep_ssp_declaration.cc`）

- [x] **V-24-2 killer**：逐格 diff（state/event 数 · transition · perm · basis.kind/pending · 投影 basis）
- [x] **反向见证**：JSON 侧改一格 ⇒ 与 `kTable` 不等（判别力）
- [x] 14 锚封闭 + 逐字名 + flow 部分映射（10 有 flow / 4 无 flow）
- [x] loader fail-closed：`E1`~`E12` **抽样 13 个 SECTION**（每节断言**具体红因码**）
- [x] 结果：**235 assertions / 5 test cases 全绿**

### Task 5: 闸（`scripts/test/cep_ssp_check.sh`）

- [x] 9 组判据（A-4 / A-6 / A-9 / A-11 / A-12a / A-12b / A-14 / D-1 / D-2+D-3 / A-10 复述）· 12 项
- [x] `--root`（拷贝根）+ `--selftest`（3 条反向 fixture · 各按签名红）
- [x] ⚠️ 修 `pipefail` 陷阱（内层闸 rc=1 是预期的红 ⇒ 须先捕获输出再 grep）

### Task 6: 主 spec + delta

- [x] `openspec/specs/cep-ssp-declaration/spec.md`（3 个 Requirement · 8 个 Scenario · 含 3/6 与 2/6）
- [x] delta = 主 spec 的 Requirements 段提取生成（`## ADDED Requirements`）
- [x] `openspec validate --type spec --strict` / `--type change --strict` **双绿**

### Task 7: 计数同步

- [x] `test/CMakeLists.txt`（`cep_ssp_declaration` + `cep_ssp_check` 两条 ctest）
- [x] `scripts/README.md`（闸族清单 +1）
- [x] `include/tlm/cep/AGENTS.md`（文件索引：4 个新文件）
- [x] 登记册 §0.1（闸 55→56 · `test_cep_*.cc` 80→81 · spec `cep-*` 50→51）

### Task 8: 全量验证 + 五步链

- [x] 全量 ctest（⛔ 无过滤）—— **211/211**
- [x] 全部 `cep_*.sh`（56 个）—— **56/56**
- [x] `docs_sync --strict` / `agents_md_lint --all` / `cep_docs_sync_check` / `docs_adr_index_check` —— 全 ✅
- [x] 五步链：产物 commit → `--update --authorized-by "Entry #553"`（预期：新增 1（新 spec）· 移出 0 · hash 变更 0）→ 清单单独 commit → 无参复验 rc=0

### Task 9: verifier 轮 1 FAIL 的修复（`implementation_gap`）

- [x] **F-1**：`ssp_loader.hh` 增 **`E13`**（`transition` 全为空洞 ⇒ fail-closed · `ADR-CEP-24` §4 E1）
- [x] **F-2**：补 SECTION「E2 空 ssps 数组（退化）」
- [x] **F-3**：补 SECTION「E3 空 states 数组（退化）」
- [x] 回归：`[cep][ssp]` **244 assertions** / 5 cases · 56/56 闸 · 211/211 ctest · 验收报告轮 2 **PASS**（`.rddf/reviews/cep-ssp-declaration-verify.md`）
