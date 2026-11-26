# tasks: cep-ssp-batch2

> **前置**：批 ① `cep-ssp-cp-q1b-landing` 已实施 + 验收（AC 7/7）+ 归档（Entry #556）⇒ 硬前置已解除 · 台账 **Entry #557**（立项 + 五步链步 1）
> **⚠️ 顺序不可颠倒**（design.md D-7）：台账 Entry（授权）→ 改产物 → commit → `--update` → **清单单独 commit** → 无参复验 rc=0
> **⚠️ 交付边界**：本 change = **S1 批 ②**（⑦ 解释器 + ⑧ 装配集成 + ⑨ 掩码格 + ⑩ 实验 4 件套 + 合同测试）。⛔ 时序 / hop ⇒ S2（`ADR-CEP-25`）· ⛔ 瞬态态生成器 ⇒ S3（`ADR-CEP-26`）
> **⚠️ 表述纪律**（design.md §6）：本 change 验收后**方可**说「CEP 已支持用户自定义协议」；实验结论**只**支撑「形态 + 引用闭环 + 路径延迟常数」档 ⇒ ⛔ 禁表述为「协议语义已真实链路验证」

## Task 0: 授权（先于改冻结面）

- [ ] T0.1 台账 Entry **#557**（立项 + **硬前置已解除** + 两处冻结面授权 · 五步链步 1 · 改动面 · 计数面 · 待裁项）—— ✅ **已落**（commit `d1a5a64e`）
- [ ] T0.2 `openspec/changes/cep-ssp-batch2/proposal.md`（house style · In Scope 4+1 · Capabilities 新增/修改声明）
- [ ] T0.3 `design.md`（D-1 ~ D-8）+ `.openspec.yaml` + `specs/` 三份 delta（MODIFIED×2 + ADDED×1）

## Task 1: ⑨ 掩码格扩展（contract 面）

- [ ] T1.1 RED：`transition` 单元支持掩码形 `{target_mask}`（3 位 · 1 正 1 负）的用例
- [ ] T1.2 RED 验证：现单元类型 `string | null` ⇒ 掩码对象判红
- [ ] T1.3 GREEN：`ssp.schema.json` 的 `transition` 单元类型扩为 `string | null | object`（`target_mask` ∈ [0,7] · `additionalProperties: false`）
- [ ] T1.4 GREEN：`ssp_loader.hh` 的 E5/E6 放宽（object 单元 ⇒ 掩码校验；string/null 行为**零改动**）
- [ ] T1.5 验证：`./build/bin/cpptlm_tests "[cep][ssp][mask]"` 全绿

## Task 2: ⑨ 的 V-24-2 逐位对账扩为两形态（killer 判据）

- [ ] T2.1 RED：掩码形态与手写 `state_spec` **逐位**相等的用例 + 双向反向 fixture
- [ ] T2.2 RED 验证：批 ① 对账闸只覆盖单目标形态 ⇒ 掩码单元无对账载体
- [ ] T2.3 GREEN：逐格对账用例扩为**两形态**（单目标 + 掩码）
- [ ] T2.4 验证：`./build/bin/cpptlm_tests "[cep][ssp][mask]"` 全绿

## Task 3: ⑦ functional-only 解释器（新头 `ssp_interpreter.hh`）

- [ ] T3.1 RED：状态迁移 + perm 投影 + ⛔ 输出结构**不含**时长 / hop 的用例
- [ ] T3.2 RED 验证：编译失败（`interpret_ssp` / `SspInterpretation` 未声明）
- [ ] T3.3 GREEN：新头落地（`SspStep` / `SspInterpretation` / `interpret_ssp`）· 洞命中 ⇒ `violation_count++` + `judgeable = false`
- [ ] T3.4 GREEN：⛔ 通用层**零协议名** · ⛔ 不产出任何时长 / 延迟 / hop 数
- [ ] T3.5 验证：`./build/bin/cpptlm_tests "[cep][ssp][interp]"` 全绿

## Task 4: ⑩ 实验 4 件套 + 档位声明（复用既有产出路径）

- [ ] T4.1 RED：`scripts/test/cep_ssp_runtime_check.sh`（4 件套齐备 + 档位声明 + 档 0 禁冒充满档 + 四件同一次运行）
- [ ] T4.2 RED 验证：既有 4 件套路径不认 SSP 协议 ⇒ 无产出
- [ ] T4.3 GREEN：**先实测**既有路径缺口 ⇒ 最小接入（⛔ **不新建**平行管线 · ⛔ 不复制整条链）
- [ ] T4.4 GREEN：证据声明 `verification_mode` + `verification_scope`（`ADR-CEP-20` 决策 6）· ⛔ 跨档聚合禁
- [ ] T4.5 验证：`bash scripts/test/cep_ssp_runtime_check.sh` rc=0

## Task 5: ⑧ 装配闸新增「SSP 绑定」维（`[IAG-09]`）

- [ ] T5.1 RED：SSP 绑定缺失 / 无效 ⇒ 判红且归因命中 `[IAG-09]` + 正控判 PASS 的用例
- [ ] T5.2 RED 验证：装配闸**不判** SSP 绑定 ⇒ 未登记 `ssp.id` 仍判 PASS
- [ ] T5.3 GREEN：`instance_assembly_gate.hh` 加第 6 维（`id` + `adapter` + SSP 三方解析到同一插件）· ⛔ **不复用 `[IAG-08]`**
- [ ] T5.4 GREEN：⛔ 不新增第二份「SSP 是否已登记」的判定实现 · 两臂共用同一份判据（既有 Scenario 零改动）
- [ ] T5.5 验证：`./build/bin/cpptlm_tests "[cep][assembly][iag]"` 全绿 + **执行臂**同步生效

## Task 6: 契约面落 main spec + **三处冻结面五步链**

- [ ] T6.1 RED：`grep -c "target_mask" openspec/specs/cep-ssp-declaration/spec.md` == 0 · `grep -c "IAG-09" openspec/specs/cep-instance-assembly/spec.md` == 0 · 缺 `openspec/specs/cep-ssp-runtime/spec.md` · `cep_freeze_check.sh` rc≠0
- [ ] T6.2 GREEN：3 份 main spec 落地（内容与本 change 的 3 份 delta **逐字一致** ⇒ 归档幂等）
- [ ] T6.3 五步链①：台账 Entry #557（步 1 · 已落）
- [ ] T6.4 五步链②：产物 commit（3 份 main spec + 代码 + 测试 · **一个** commit）
- [ ] T6.5 五步链③：`bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #557"`（预期：新增 **1**（新 main spec）· 移出 0 · hash 变更 = 命中数）
- [ ] T6.6 五步链④：清单**单独** commit（⛔ 不得与产物同 commit）
- [ ] T6.7 五步链⑤：`bash scripts/test/cep_freeze_check.sh` 无参复验 **rc=0**（硬门）
- [ ] T6.8 验证：`openspec validate --specs --strict` + `--changes --strict` 全绿

## Task 7: 收口验证（archive 前）

- [ ] T7.1 `./build/bin/cpptlm_tests "[cep][ssp]"` 全绿
- [ ] T7.2 `./build/bin/cpptlm_tests "[cep][assembly]"` 全绿
- [ ] T7.3 `./build/bin/cpptlm_tests "[cep]"` 全绿（**零回归**）
- [ ] T7.4 `ctest --test-dir build --output-on-failure -j4` **全绿**（⛔ 不按 `-R` 过滤）
- [ ] T7.5 全部 `scripts/test/cep_*.sh` 全 rc=0
- [ ] T7.6 `bash scripts/test/cep_freeze_check.sh` **rc=0**（三处冻结面）
- [ ] T7.7 `openspec validate --specs --strict` + `--changes --strict` 全绿
- [ ] T7.8 `bash scripts/test/docs_sync_check.sh --strict` rc=0
- [ ] T7.9 遗留清扫（TODO / 临时日志 / 注释掉的代码 / 被 skip 的测试 / `.cc.disabled` 新增 = 0）
- [ ] T7.10 ⛔ **表述纪律**：交付说明**不得**含「CEP 已支持用户自定义协议」；实验结论**不得**表述为「协议语义已真实链路验证」
- [ ] T7.11 ⛔ **不得**宣称 ⑨ 的 contract 面已受 M1 保护（D-8 · architect 待裁 · Entry #557 ⑥）
