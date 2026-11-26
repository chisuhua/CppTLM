# design: cep-ssp-cp-q1b-landing —— 用户自定义协议的装配期入口（contract 面）

> **Change name**: `cep-ssp-cp-q1b-landing`（slug，**不带日期前缀**）
> **配套**: `openspec/changes/cep-ssp-cp-q1b-landing/`（proposal / design / specs / tasks）
> **来源**: `.rddf/improvements/cep-ssp-cp-q1b-landing.md`（✅ 已批准·待立项 · 用户批准 5 段设计）
> **架构依据**: `CP-Q-1b` 裁决（✅ 已 ratify · 用户批准 R1~R4 · oracle 2 轮 · critic 1 轮 0 BLOCKER）+ 3 份 ADR Status Update（`ADR-CEP-24` 第三则 / `ADR-CEP-11` / `ADR-CEP-12`，均 2026-11-26 落盘 · 台账 Entry #554）
> **生成时间**: 2026-11-26 · **生成方式**: builder 手写（design.md ⛔ **不由 rdd-builder 生成** —— per 该 skill 的 owns 边界声明）

---

## 0. 决策摘要（一句话）

让 `family` 有第 4 个可解析取值（`user-defined`）+ 让 `protocol_version` 有第二种可机判的合法形态（`user:<已登记 ssp-id>`），**只扩取值域、零新增能力面**，从而把批 1 的「声明面」接到「装配期」的门口。

## 1. 背景：为什么用户自定义协议在装配期必然 fail

批 1（`cep-ssp-declaration`）交付了「**能声明**」（SSP 声明面 + loader + V-24-2 逐格对账）。但用户写的自定义实例**过不了装配期**，三条独立的红：

| # | 红的判据 | 根因 |
|:--:|---|---|
| 1 | `[IAG-03]`「`family` 必须存在且可解析」 | `family` 不属于既有 3 值域（`industrial` / `ruby-slicc` / `academic`）|
| 2 | `ADR-CEP-11` 决策 3 / `INV-CEP-11-IC1` | `protocol_version` 不在 §3 锁定表内（用户协议**没有官方规范版本**）|
| 3 | schema `pattern` 层 | 现 pattern `^[a-z0-9]+(-[a-z0-9.]+)*$` **拒绝 `:`** ⇒ `user:<ssp-id>` 在 schema 层即被拒 |

⇒ 声明面**无法被任何装配路径消费** ⇒ 批 2（`cep-ssp-batch2`）的 ⑦⑧⑨⑩ **全部无法验证** ⇒ 本 change 是批 2 的**唯一硬前置**。

## 2. 关键决策

### D-1：两分支谓词落**单一** helper（⛔ 禁第二份拷贝）

**决策**：两分支谓词（`∈ 锁表` **或** `^user:<已登记 ssp-id>`）实现为**单一** helper，由 IC1 通用闸调用。

**理由**：`ADR-CEP-11` SU ③ 第 3 面 + `test_cep_schema.cc:552` 的 Entry #109 **G-2**（「不得多处各抄一份」）。⚠️ `instance_assembly_gate.hh` **不查** `protocol_version`（通用闸在**测试层** `test_cep_schema.cc:573`）⇒ ⛔ **禁**在 IAG 另写一份（否则两份谓词可能漂移，且 IAG 那份无反向 fixture ⇒ 静默假绿）。

**代价**：helper 落点须与既有 `protocol_version_in_lock_table` 同处（见 D-2），使「锁表判定」与「两分支判定」可共用底层谓词。

### D-2：helper 落点 = `test/cep_adr_parser.hh`（与既有锁表 helper **同处**，⛔ 零新文件）

**决策**：两分支 helper 落在 **`test/cep_adr_parser.hh`**（`cpptlm::cep` 命名空间），与既有 `parse_protocol_version_lock_table` / `protocol_version_in_lock_table` **同处**；⛔ **不新建**文件。

**理由**：
- **实测定位**（2026-11-26）：`parse_protocol_version_lock_table` 与 `protocol_version_in_lock_table` 的**唯一定义**在 `test/cep_adr_parser.hh:605` / `:644` ⇒ 锁表判定面**本就在测试层**（`include/tlm/cep/*.hh` **零** `protocol_version` 相关 helper）；
- 该文件自身的纪律注释即写着「锁表是 `protocol_version` 的**唯一真源**……**任何**『某取值是否合法』的判定都**必须从 ADR 解析**，不得在代码里再抄一份（2026-10-30 Entry #109 的 **G-2**）」⇒ **两分支谓词**（同属「某取值是否合法」判定）**必须与既有 helper 同处**，否则 G-2 被破；
- 分支 ② 需要「已登记 `ssp.id` 集合」⇒ 依赖批 1 的 `ssp_loader.hh`（`include/tlm/cep/ssp_loader.hh`）⇒ helper 需能读 `ssps.json`（见 D-4）；
- ⛔ **零新文件**：新建头会扩大 `include/tlm/cep/**` 的对账面（`docs_sync_check.sh` 路径闸 + `include/tlm/cep/AGENTS.md` 文件索引）且与「生产层无 `protocol_version` 判定面」的既有事实脱节（无生产代码消费它 ⇒ 放生产层即死代码）。

**代价**：分支 ② 的「已登记」判定**依赖测试层能读到 `ssps.json`** ⇒ 需在 helper 侧加一个「从 `ssps.json` 提取 `ssp.id` 集合」的入口（见 D-4）；且该判定**当前只被测试层消费**（`instance_assembly_gate.hh` **不查** `protocol_version`）⇒ 生产层的放行属批 2 的装配集成范围，本 change ⛔ 不越界。

### D-2b：⛔ **不改** `instance_assembly_gate.hh`（IAG 零 diff）

**决策**：本 change ⛔ **零改动** `include/tlm/cep/instance_assembly_gate.hh`。

**理由**：`ADR-CEP-11` SU ③ 第 3 面明写「⚠️ `instance_assembly_gate.hh` **不查** `protocol_version`（现通用闸在**测试层** `test_cep_schema.cc:573`）⇒ ⛔ **禁**在 IAG 另写一份」⇒ IAG 侧**零 diff** 正是该纪律的**正向**结果（不是遗漏）。

### D-3：schema `pattern` 改**两分支**（不新增字段、不改字段数）

**决策**：`protocol_version` 的 `pattern` 由单分支 `^[a-z0-9]+(-[a-z0-9.]+)*$` 改为**两分支**形态，容纳 `user:[a-z][a-z0-9_]*`；`family` 的 `enum` 追加 `user-defined`。

**理由**：`ADR-CEP-11` SU ③ 第 2 面「⛔ **必改**」+ `ADR-CEP-12` SU（`family` 域 **3 → 4 值**）。

**约束**：
- ⛔ `family` 的 **13 必含属性个数不变**（`INV-CEP-12-IC1` 的 13 不动）—— 只扩 `enum` 取值域；
- ⛔ **不新增** schema 字段（`additionalProperties: false` 不动）；
- ⛔ `pattern` **两分支可证不相交**（ADR SU ②）：锁表内全部官方版本均含 `-` 或 `.`，`ssp.id` 词表 `^[a-z][a-z0-9_]*$` 排除二者 ⇒ `user:<ssp-id>` 不可能字面等于任一官方版本 ⇒ 分支 ② **无法冒充分支 ①**；拼错的官方版本（如 `chi-r3`）不匹配 `^user:` ⇒ **仍**落分支 ① 且 fail ⇒ **判据未变弱**。

### D-4：`<ssp-id>` 的「已登记」判据 = `ssps.json` 的 `ssp.id` 集合（**唯一真源**）

**决策**：分支 ② 的「已登记」= `<ssp-id>` ∈ `include/tlm/cep/ssps.json` 中某条 SSP 的 `ssp.id`。

**理由**：批 1 的 `ssp.id` 词表 = `^[a-z][a-z0-9_]*$`（`ssp_loader.hh` **E2**）+ 唯一性由 E2 的 `ids.insert(sid).second` 判重保证 ⇒ `user:` 引用的 id 必定唯一 ⇒ **可寻址**。

**约束**：⛔ **不**在本 change 内新增版本字段（`ssp.version` 不存在 ⇒ `user:<ssp-id>` 的**单版本**语义当前成立）⇒ 若未来 SSP 引入版本字段，`user:` 语义须重裁（可能 `user:<ssp-id>@<ver>`）—— 已登记为 **revisit trigger**（`ADR-CEP-11` SU ④）。

### D-5：IC1 Scenario 两分支化 ⇒ **MODIFIED delta**（⛔ 不动其余 Scenario）

**决策**：change 内落 `specs/cep-core-contracts/spec.md` 的 `## MODIFIED Requirements` 块（requirement = `cep-protocol-version-locking-contract`），**整段复制** main spec 既有 requirement 正文（含 **IC1 / IC2 / IC3 三个 Scenario** 全在），**仅两分支化 IC1 的 `THEN`**。

**理由**：
- openspec 的 `MODIFIED` 块按 **Scenario 名精确匹配**，且 archive **拒绝丢弃**既有 Scenario 名（工具源码 `specs-apply.js:392-394`）⇒ delta 必须含 main spec 中该 requirement 的**全部**既有 Scenario；
- ⛔ **不得**借机改 IC2 / IC3 的文字（超出本 change 范围）⇒ 逐字复制。

**代价**：delta 与 main spec 存在**重复正文** ⇒ 归档时 `openspec archive` 会把 delta 并入 main spec ⇒ **两处**冻结面（`protocol_instance.schema.json` + `openspec/specs/cep-core-contracts/spec.md`）各需一次 **M1 五步链**（见 D-7）。

### D-6：既有 8 实例**零行为变更**（只扩取值域，不收窄）

**决策**：不触碰 `ADR-CEP-11` §3 锁定表本身（⛔ `INV-CEP-11-IC2` 零改动）⇒ 既有 8 实例的 `protocol_version` 全在锁表内 ⇒ 仍落分支 ① ⇒ 全部绿灯。

**约束**：⛔ 不把任何用户自创版本**写入**锁表（⛔ 不污染唯一真源）；⛔ 批 1 的 `mesif_ssp` 不受影响（SSP 不绑实例）。

### D-7：两处冻结面 ⇒ **两次**五步链（授权 → 产物 commit → `--update --authorized-by` → 清单单独 commit → 无参复验 rc=0）

**决策**：schema 与 main spec 是 **M1 冻结集成员**（`cep_freeze_check.sh:56` + `:113`）⇒ 改二者各须走一次完整五步链。

**顺序（物理串行）**：
1. 台账 Entry 授权（`obj-cep-platform §11`）；
2. 产物 commit（schema + main spec + 代码 + 测试）；
3. `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`；
4. 清单**单独** commit；
5. **无参复验** `bash scripts/test/cep_freeze_check.sh` ⇒ **rc=0**。

**约束**：⛔ 不得把「改冻结面」与「改清单」混进同一个 commit（清单 diff 必须可单独审）；⛔ 步骤 5 是**硬门**（rc≠0 ⇒ 不得进 archive）。

### D-8：表述纪律（本 change 完成后**仍不得**说「CEP 已支持用户自定义协议」）

**决策**：验收完成后**仍不得**宣称「CEP 已支持用户自定义协议」。

**理由**：本 change 只把**声明面接到装配期门口**；解释器 / 装配集成 / 掩码格 / 实验 4 件套仍属批 2 ⇒ 「形态已定」≠「能力已交付」。

## 3. 影响面

| 面 | 影响 | 冻结面 |
|---|---|:--:|
| `include/tlm/cep/protocol_instance.schema.json` | `family` enum +1 值；`protocol_version` pattern 改两分支 | ✅ 五步链 |
| `include/tlm/cep/protocol_family.hh` | 6 处 + 1 新增 `static_assert`；注释 3 值 → 4 值 | — |
| 共享 helper（`test/cep_adr_parser.hh`，见 D-2） | 两分支谓词单点实现 | — |
| `test/test_cep_schema.cc` | IC1 通用闸两分支化 + pattern 容纳性用例 + 4 条反向 fixture | — |
| `openspec/specs/cep-core-contracts/spec.md` | IC1 Scenario 两分支化（IC2/IC3 零改动）| ✅ 五步链 |
| `ADR-CEP-11` §3 锁定表 | **零改动**（`INV-CEP-11-IC2` 保持）| — |
| 新增 invariant | **0**（IC1 号不变，只扩取值域）| — |
| 既有 8 实例 | **零行为变更** | — |

## 4. 风险与缓解

| # | 风险 | 缓解 |
|:--:|---|---|
| R-1 | 两分支谓词被抄成两份 ⇒ 漂移 | 单一 helper（D-1）+ 反向 fixture 断言「唯一实现」（AC-5）|
| R-2 | `pattern` 两分支写错 ⇒ `user:` 形态被 schema 层拒 | 4 条反向 fixture 各自按**正确原因**红（AC-2）|
| R-3 | 冻结面改了但清单未更新 ⇒ `cep_freeze_check.sh` 红 | D-7 的 5 步物理串行 + 步骤 5 硬门（AC-6）|
| R-4 | delta 漏既有 Scenario ⇒ archive 报错 | D-5：delta 整段复制（3 Scenario 全在）+ `openspec validate --strict` 复核 |
| R-5 | 收窄既有判据 ⇒ 既有 8 实例回归红 | D-6：不碰锁表 + AC-3「既有 8 实例全绿」|

## 5. Verification（可勾选验收判据 · 与 proposal §验收标准 一致）

- [ ] `family` enum 含 **4 值**；`protocol_family_from_name("user-defined")` 为 `true`，`protocol_family_id(UserDefined) == 4`
- [ ] `protocol_version` **接受** `user:<已登记 ssp-id>`；**拒绝** `user:<未登记>` / `chi-r3` / `user:`（空 id）—— 4 条反向 fixture 各自按**正确原因**红
- [ ] **既有 8 实例全绿**；批 1 的 `mesif_ssp` 不受影响
- [ ] `openspec/specs/cep-core-contracts/spec.md` 的 IC1 Scenario 已两分支化（`grep "user:" <spec>` ≥ 1）
- [ ] 两分支谓词是**唯一**实现（共享 helper；⛔ 无第二份拷贝）
- [ ] 五步链完成：schema + spec **两处**冻结面 ⇒ `cep_freeze_check.sh` 无参 **rc=0**
- [ ] `test_cep_schema.cc` 的 `[cep][schema][ic1]` 用例全绿 + 新增 `user:` 用例
