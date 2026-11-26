# tasks: cep-ssp-cp-q1b-landing

> **前置**：`CP-Q-1b` 裁决已 ratify（用户批准 R1~R4）· 3 份 ADR SU 同批落盘（`ADR-CEP-24` 第三则 / `ADR-CEP-11` / `ADR-CEP-12` · 台账 Entry #554）
> **⛔ 顺序不可颠倒**（design.md D-7）：台账 Entry（授权）→ 改产物 → commit → `--update` → **清单单独 commit** → 无参复验 rc=0
> **⚠️ 交付边界**：本 change **只做 contract 面**（`family` 4 值域 + `protocol_version` 两分支）。⛔ 解释器 / 装配集成 / 掩码格 / 实验 4 件套 ⇒ `cep-ssp-batch2` 另立
> **⚠️ 表述纪律**（design.md D-8）：本 change 完成后**仍不得**宣称「CEP 已支持用户自定义协议」

## Task 0: 授权（先于改冻结面）

- [ ] T0.1 台账 Entry（立项 + **两处冻结面**授权 · 改动面 · 五步链预测 · 计数面 · 不做项）落 `.rddf/roadmap/objectives/obj-cep-platform.md` §11
- [ ] T0.2 `openspec/changes/cep-ssp-cp-q1b-landing/proposal.md`（planner 域素材 + builder 定稿）
- [ ] T0.3 `.openspec.yaml` + `design.md`（builder 域 · D-1~D-8）

## Task 1: `protocol_family.hh` 加 `UserDefined = 4`（6 处 + 1 新增断言）

- [ ] T1.1 RED：新增 `[cep][schema][ic1]` 用例断言 `kProtocolFamilyCount == 4u` + `user-defined` 可解析 + 既有 3 值逐字不变 + 未知字符串仍返 false
- [ ] T1.2 RED 验证：编译失败（`ProtocolFamily::UserDefined` 未声明）
- [ ] T1.3 GREEN：`enum class ProtocolFamily` 加 `UserDefined = 4` + `kProtocolFamilyCount` 3→4 + `kProtocolFamilies[]` 加条目 + `protocol_family_name` 的 `switch` 加 case
- [ ] T1.4 GREEN：编译期锁加 `static_assert(protocol_family_id(UserDefined) == 4)` + 既有 `static_assert(kProtocolFamilyCount == 3)` → 4 + 注释「恰好 3 值」→ 4 值
- [ ] T1.5 GREEN 验证：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` 全绿

## Task 2: schema `family` enum 加 `user-defined` + `protocol_version` pattern 改两分支（⛔ M1 冻结面）

- [ ] T2.1 RED：既有 `:512` 用例追加 `user:` 形态断言（正控 2 条 + 反向 4 条）+ 新增 `family` enum 4 值 + `required` 仍 13 的 schema 面断言
- [ ] T2.2 RED 验证：`fam.size() == 4u` 红 + `std::regex_match("user:mesif_ssp", re)` 红（现 pattern 拒绝 `:`）
- [ ] T2.3 GREEN：`protocol_instance.schema.json` 的 `family` enum 追加 `user-defined` + `protocol_version` 的 `pattern` 改 `^([a-z0-9]+(-[a-z0-9.]+)*|user:[a-z][a-z0-9_]*)$`
- [ ] T2.4 GREEN 验证：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` 全绿
- [ ] T2.5 ⛔ 不改字段数 / `required` 13 项 / `additionalProperties`

## Task 3: 共享 helper —— 两分支谓词单点实现（⛔ 零新文件）

- [ ] T3.1 RED：新增用例断言两分支谓词（分支 ① 锁表内 2 条绿 + 分支 ② `user:mesif_ssp` 绿 + 反向 4 条按正确原因红）
- [ ] T3.2 RED 验证：编译失败（`parse_registered_ssp_ids` / `protocol_version_allowed` 未声明）
- [ ] T3.3 GREEN：`test/cep_adr_parser.hh` 加 `parse_registered_ssp_ids`（从 `ssps.json` 提取已登记 `ssp.id`）+ `protocol_version_allowed`（两分支谓词 · 单一实现）
- [ ] T3.4 GREEN 验证：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` 全绿
- [ ] T3.5 ⛔ helper 落 `test/cep_adr_parser.hh`（与既有锁表 helper 同处）· ⛔ **零新文件** · ⛔ **不改** `instance_assembly_gate.hh`

## Task 4: IC1 通用闸两分支化（`test/test_cep_schema.cc:573`）

- [ ] T4.1 GREEN：IC1 通用闸的逐实例判定 + 反向 fixture ① + 变异块全部改用 `protocol_version_allowed`（两分支）
- [ ] T4.2 GREEN：追加反向 fixture ②（`user:no_such_ssp` 红）+ ③（`user:` 空 id 红）+ 正控（`user:mesif_ssp` 绿 ⇒ 分支 ② 可达性）
- [ ] T4.3 验证：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` 全绿 + **既有 8 实例零回归**
- [ ] T4.4 ⛔ 不碰 `ADR-CEP-11` §3 锁表（`INV-CEP-11-IC2` 保持）

## Task 5: 「两分支谓词是唯一实现」反向 fixture（AC-5）

- [ ] T5.1 GREEN：新增守门用例 —— 扫仓内 `user:` 判定落点，除 helper 本体 + 本测试文件外**任何第二处**即红
- [ ] T5.2 验证：`./build/bin/cpptlm_tests "[cep][schema][ic1]"` 全绿
- [ ] T5.3 ⛔ 不新建公共工具函数（扫描器在本用例内最小自实现）

## Task 6: main spec 的 IC1 Scenario 两分支化 + **两处五步链**（⛔ M1 冻结面）

- [ ] T6.1 RED：`grep -c "user:" openspec/specs/cep-core-contracts/spec.md` == 0 + `cep_freeze_check.sh` rc≠0
- [ ] T6.2 GREEN：`openspec/specs/cep-core-contracts/spec.md` 的 IC1 Scenario `THEN` 两分支化（**IC2 / IC3 逐字零改动**）· 与 change delta **逐字一致**
- [ ] T6.3 五步链①：台账 Entry 授权（T0.1 落的那条）
- [ ] T6.4 五步链②：产物 commit（schema + main spec + 代码 + 测试）
- [ ] T6.5 五步链③：`bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`
- [ ] T6.6 五步链④：清单**单独** commit（⛔ 不得与产物同 commit）
- [ ] T6.7 五步链⑤：`bash scripts/test/cep_freeze_check.sh` 无参复验 **rc=0**（硬门）
- [ ] T6.8 验证：`openspec validate cep-ssp-cp-q1b-landing --strict` rc=0 + `openspec validate --changes --strict` 全绿

## Task 7: 收口验证（archive 前）

- [ ] T7.1 `./build/bin/cpptlm_tests "[cep][schema][ic1]"` 全绿
- [ ] T7.2 `./build/bin/cpptlm_tests "[cep][schema]"` 全绿（零回归）
- [ ] T7.3 `ctest --test-dir build --output-on-failure -j4` 全绿（**零回归 = 全量**）
- [ ] T7.4 `bash scripts/test/cep_freeze_check.sh` **rc=0**
- [ ] T7.5 `bash scripts/test/docs_sync_check.sh --strict` rc=0
- [ ] T7.6 `./scripts/build/format.sh --check` rc=0
- [ ] T7.7 ⛔ 交付说明**不得**宣称「CEP 已支持用户自定义协议」（D-8）
