# tasks: cpptlm-module-param-read-declares

> 决策源 = `docs/adr/ADR-X.17-module-param-read-declares.md`（v1.5）;实施形态见本 change 的 `design.md`（D-1~D-8）。
> 共 **T0~T7**;标 ⚠️ 的条目为立项期 gap 分析（metis）新增/改写。

## T0 — 前置（独立签收）

- [ ] T0.1 ⚠️ **B-1 的复核 SHALL 由非本 change 作者执行**（planner 或 verifier）;签收物 = `.rddf/reviews/ADR-X.17.md` 追加一段 `## B-1 复核（实施 change 期）`，逐条列出：`module_factory_validate.cc:306-307` / `:320-326` / `:327-330` 的 **NICTLM 保留证据** + `test_config_schema_validation.cc:122/138/219` 三条实测绿输出。⛔ 不得由实施者自查后打勾
- [ ] T0.2 复核 `test/test_config_schema_validation.cc:122/138/219` 当前为绿（AC-08① 基线）
- [ ] T0.3 复核 `flit_width` / `vc_count` 的去向：二者 `configs/` 零命中、实现零消费 ⇒ 随 `get_param_rules()` 删除**自然消失**，不进基线表（design D-7）

## T1 — 机制（新增，不动既有）

- [ ] T1.1 新增 `include/core/param_loader.hh`：`load_param<T>(cfg, key, target, lo, hi, def)` + `ParamLoadContext`（持有 `cfg_` 引用 + `consumed_` + `unknown_keys()` + `cfg_has_params_key()`）
- [ ] T1.2 ⚠️ **失败通道复用** `cpptlm::ParamValidationError`（`include/core/param_errors.hh:11-24`）;`detail` 含期望范围;⛔ 不新造异常类型
- [ ] T1.3 ⚠️ **数值边界逐条钉**：整型只接受 `is_number_integer()`（`8.0` / `1e20` / `true` 拒绝）;无符号另加 `>= 0`
- [ ] T1.4 新增 `include/core/param_admission.hh`：`is_param_admitted(std::string_view)`（**唯一 C++ 真源**）
- [ ] T1.5 新增**测试夹具模块**（仅测试用，`ParamFixtureTLM`）：2 个标量参数（其一带默认值）⇒ 先证明机制本身可红;⚠️ MUST ① 经 `REGISTER_CHSTREAM` 注册（否则 `instantiateAll` 无法按 `type` 创建）② **进准入表**（`is_param_admitted("ParamFixtureTLM") == true`，否则 AC-06 不可满足 —— design D-2/D-3）

## T2 — 装配接入

- [ ] T2.1 `SimObject` 持有 `std::unique_ptr<ParamLoadContext> ctx_`;`set_config` 内**在 `config_` 赋值之后**重建（design D-6）
- [ ] T2.2 ⚠️ **收窄版**「无 `params` 也加载」：仅当 `is_param_admitted(type())` 或 `params` 键存在时调用 `on_config_loaded()`;对「未准入且无 `params`」维持现状跳过（⛔ 不引入新的副作用触发面，design D-2）
- [ ] T2.3 未知键失败（**基类执行**，design D-3a）：`set_config` 在 `on_config_loaded()` 返回后判 `is_param_admitted(type()) && !unknown_keys().empty()` ⇒ 抛 `ParamValidationError`
- [ ] T2.4 legacy 扁平路径豁免（`cfg_has_params_key() == false` ⇒ 跳过未知键检查;⛔ 不得因此跳过必填）
- [ ] T2.5 加载失败 ⇒ `instantiateAll` 返回 `false`（复用现有 catch 通道 `module_factory.cc:416-423`）

## T3 — RouterTLM 试点（**单 commit 原子完成**）

- [ ] T3.1 `RouterTLM::on_config_loaded` 逐参数 `load_param`：`node_x` / `node_y` / `mesh_x` / `mesh_y`（必填，**不设范围**）/ `buffer_size`（1..64，默认 8）/ `credit_timeout`（`uint64_t`，无范围，默认 0）
- [ ] T3.2 ⛔ **不含 `vc_count`**（其值不影响运行时行为）;⛔ **不含 `flit_width`**（自然消失）
- [ ] T3.3 ⚠️ **credit 同源副作用显式保留**：`buffer_size` 赋值后重算 `downstream_credits_`，**仅在深度实际变化时**执行（`if (new_depth != buffer_depth_)`）—— 保证二次加载不刷新已消耗额度
- [ ] T3.4 删除 `RouterTLM` 的 `registerParamRules` 注册调用（`router_tlm.cc:617-620`）
- [ ] T3.5 删除 `RouterTLM::get_param_rules()` 本体（`router_tlm.cc:584-610` + `router_tlm.hh:208`）
- [ ] T3.6 ⚠️ 删除 **CFG-08 的全部 RouterTLM 引用**：`:304-305`（扁平回退）+ `:311-319`（逐键检查）+ **`:327` 的 `type == "RouterTLM"` 析取（改为 `else if (type == "NICTLM")`）**。验收：`grep -n 'RouterTLM' src/core/module_factory_validate.cc` 在 `:301-330` 段内**零命中**
- [ ] T3.7 `is_param_admitted("RouterTLM")` 置 `true`
- [ ] T3.8 处置 `test/test_d3_param_resolver_registry.cc`（RouterTLM 相关用例删除或整文件删除，理由见 design D-6 / ADR-X.17 M4）
- [ ] T3.9 ⛔ **NICTLM 的 CFG-08 分支不动**（`:306-307` / `:320-326` / `:327-330`）

## T4 — 基线台账（辅助）

- [ ] T4.1 新增 `docs/implementation/module-param-baseline.md`：**表 A**（已消费但不经 `load_param`，逐键标注消费方）· **表 B**（真未消费豁免 = F-14 的 5 类 / 14 键）
- [ ] T4.2 ⚠️ 文档明确标注「**人类可读台账，非任何闸的输入**」（机制判定由 AC-04 的运行时测试承担，design D-4）

## T5 — 验证

- [ ] T5.1 AC-01：夹具模块越界 / 类型错 ⇒ `instantiateAll` 返回 false 且错误信息含**模块名 + 键名 + 期望范围**
- [ ] T5.2 AC-02：夹具模块两个参数各有「两个不同合法值 ⇒ 可观测指标不同」
- [ ] T5.3 AC-03：① `git diff <base> -- test/ include/ src/` 中除新增测试与 `test_d3_param_resolver_registry.cc` 外，既有 `REQUIRE` / `CHECK` 行无删除/改值 ② `git diff <base> -- configs/` **为空** ③ 全标签套件全绿 ④ `[pcie]` / `[cep]` 绿（外部副作用零变化）
- [ ] T5.4 ⚠️ AC-04：**先跑基线**（改动前，对含已准入类型实例的 `configs/**/*.json` 逐个 `instantiateAll`，记录成功/失败清单 + 既存失败的豁免证据）→ 再写 **Catch2 运行时对拍**（同一清单中基线成功者仍成功 + `unknown_keys()` 为空）;⛔ 不用 grep 提取键名;⛔ 不静默 `try/catch` 吞失败
- [ ] T5.5 AC-05：`buffer_size` = `1` / `64` 通过;`0` / `65` 失败（测试内构造）
- [ ] T5.6 AC-06：无 `params` 键的**夹具**实例 ⇒ 默认值落成员
- [ ] T5.7 AC-07：RouterTLM 配 `params.buffer_sizes` ⇒ 返回 false 且错误含该键名;⚠️ 且 `grep -nE 'unknown|buffer_sizes' src/tlm/router_tlm.cc` 零命中（证明失败源是基类而非模块内手写 throw）
- [ ] T5.8 AC-08：① `test_config_schema_validation.cc:122/138/219` 绿 ② `grep -n 'RouterTLM' src/core/module_factory_validate.cc` **文件级零命中**
- [ ] T5.9 AC-09：① 存量 BSP-3 绿 ② 消耗 credit 后同值二次 `set_config` ⇒ 后续转发数不增加
- [ ] T5.10 AC-10：默认值**不写回** `cfg["modules"][n]["params"]`（夹具 + RouterTLM 各一条）
- [ ] T5.11 ⚠️ AC-11：`buffer_size` = `8.0` / `true` / `1e20` ⇒ 失败;`credit_timeout` = `-1` ⇒ 失败;`buffer_size` = `8` ⇒ 成功
- [ ] T5.12 ⚠️ AC-12：RouterTLM **扁平**形式（无 `params`，顶层 `node_x` 等）⇒ 装配成功且不因 `name`/`type` 报未知键;同形式**缺 `node_x`** ⇒ 失败;NICTLM 扁平形式（顶层 `node_id`）行为不变
- [ ] T5.13 ⚠️ AC-13：`grep -nE 'load_param[^;]*"vc_count"' src/tlm/router_tlm.cc` 零命中
- [ ] T5.14 护栏 G-1（幂等）/ G-2（嵌套 RouterTLM 缺 `node_x` ⇒ false，改动前后均成立）
- [ ] T5.15 `ctest -R cep` + `[router]` / `[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]` / `[config][schema]` 全绿

## T6 — spec 修订（**走 delta，不直接编辑 main spec**）

- [ ] T6.1 ⚠️ 新增 `openspec/changes/cpptlm-module-param-read-declares/specs/param-derivation/spec.md`：① 先 `grep -n "^### Requirement:" openspec/specs/param-derivation/spec.md` 取**准确 Requirement 名**（`REMOVED` / `MODIFIED` 段必须逐字匹配 main spec 名，否则 `openspec validate` 报错）② `## REMOVED Requirements`（移除 `get_param_rules()` 强制条款 + `flit_width` 派生条款）③ `## MODIFIED Requirements`（改为「**已准入**类型 SHALL 用 `load_param`;未准入类型 MAY 保留 `get_param_rules()`（逐类型退役）」）
- [ ] T6.2 ⚠️ 新增 `.../specs/param-validation/spec.md` 的 `## MODIFIED Requirements`：执行者从 ModuleFactory 改为模块内 `load_param`;`:38-45` 的「marked required」与 `def = nullopt` 术语对齐
- [ ] T6.3 ⚠️ 新增 `.../specs/param-type-conversion/spec.md` 的 `## MODIFIED Requirements`：转换真值点从 ModuleFactory 改为模块内
- [ ] T6.5 ⛔ **真源 1 文档同步剥离**（`docs/architecture/06` §9.4 / `10-topology-generation-management-system.md` §8.2-8.4）⇒ 登记为 doc-only 后续 change（proposal 已知残留）

## T7 — 收口

- [ ] T7.1 台账 Entry（实施记录 + 实测 + 改动面）
- [ ] T7.2 KPI 回填（6 处活口径）
- [ ] T7.3 归档（`openspec archive`，⛔ 不用 `--skip-specs`）⇒ **archive 自动生成** `openspec/specs/module-param-load-declares/spec.md`（⛔ 不得手工预建 —— 会与 archive 输出冲突）
- [ ] T7.3a 手写新 main spec 的 Purpose（archive 留 `TBD - created by archiving change …`）;且**不**回填 `openspec/specs/param-derivation/spec.md` 的 Purpose —— 该 spec 属「另案订正」（delta 无法表达 Purpose，见 proposal Out of Scope 与 AC 表外说明）
- [ ] T7.4 冻结清单核查（`cep_freeze_check` —— 预期不受影响：本 change 不产出 `cep-*` spec）
- [ ] T7.5 独立验收（切 verifier）
- [ ] T7.6 ⚠️ **残留清点**：`grep -rn 'get_param_rules' --include=*.cc --include=*.hh src/ include/ test/` 的结果只剩 `include/core/param_rules.hh` 与 `src/core/module_factory.cc` 的通用机制（**无任何模块实现**）;且 `grep -n 'RouterTLM' src/core/module_factory_validate.cc` 零命中（与 AC-08② 同一判据）
