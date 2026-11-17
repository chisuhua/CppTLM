# 验收报告 — cpptlm-module-param-read-declares

- **验收角色**：verifier（只读 + 取证；未修改实现）
- **日期**：2026-11-17
- **被判对象**：commit `0fddd258`（实施）+ 其后格式化提交（clang-format 仅作用于本次改动的 5 个文件）
- **判据来源（按优先级）**：
  1. `openspec/changes/archive/2026-11-17-cpptlm-module-param-read-declares/proposal.md` 的 `## 验收标准`（AC-01~AC-14 + 护栏 G-1/G-2）
  2. 同 change 的 delta spec（11 个 Scenario）
  3. `docs/adr/ADR-X.17-module-param-read-declares.md` 的 Verification（V-1~V-7）
- **已用轮次**：1 / 3（`RDDF_VERIFIER_MAX_LOOPS`）
- **判决**：**PASS**（0 项 implementation_gap / 0 项 proposal_drift）

> ⚠️ **分流说明**：`rddf rdd-verify --dry-run` 返回 `No eligible changes`（P3 archive gate 未触发 ——
> 本仓未登记 `.rddf/state/iteration.json`）⇒ 走**本地验收**，不调用 `rdd-verifier` skill。
> 本地验收的额外门「任务完成」按 `tasks.md` 判定：实现任务 T1~T6 **全部完成**（44 项已勾），
> 未勾的 8 项为 T0.1（本次验收执行）+ T7.1~T7.6（收口序列，须在验收通过后执行）。

---

## 1. 逐条判定（每条均有工具证据）

| AC | 判定 | 证据 |
|:--:|---|---|
| **AC-01** 夹具越界/类型错 ⇒ false + 错误含模块名+键名+范围 | ✅ PASS | `cpptlm_tests "[param]"` 的 `AC-01` 用例（含 `REQUIRE(e.module_name=="fx_name")` / `e.param_name=="gain"` / `what()` 含 `range [1,16]` 与 `got 0`） |
| **AC-02** 夹具两参数各驱动可观测行为 | ✅ PASS | `AC-02` 用例：`a.compute(10) != b.compute(10)`（gain 2 vs 5）+ `a.compute(10) != c.compute(10)`（offset 0 vs 7） |
| **AC-03①** 既有 REQUIRE/CHECK 无删除/改值（除豁免文件） | ✅ PASS | 逐文件 `git diff fa2c4916 -- <f> \| grep -cE '^-[^-].*(REQUIRE\|CHECK)\('`：仅 `test/test_d3_param_resolver_registry.cc` = 11，其余 6 个改动文件 = 0 |
| **AC-03②** `configs/` 零 diff | ✅ PASS | `git diff --name-only fa2c4916 -- configs/ \| wc -l` = **0** |
| **AC-03③** 全标签套件全绿 | ✅ PASS | `./build/bin/cpptlm_tests` ⇒ **All tests passed (883191 assertions in 2371 test cases)**（格式化后重跑） |
| **AC-03④** `[pcie]`/`[cep]` 绿（外部副作用零变化） | ✅ PASS | 含在全量绿中；另 `ctest -R cep` ⇒ **100/100** |
| **AC-04** 运行时对拍 + 基线 | ✅ PASS（见 §3 的基线说明） | `AC-04` 用例：`checked >= 10`，实测 11 个含已准入类型的配置 / 1 个基线豁免（`configs/noc_mesh.json`）;豁免名单在测试代码内且每条带原因 |
| **AC-05** 边界成对 1/64 通过、0/65 失败 | ✅ PASS | `AC-05` 用例（夹具 1/16 + AC-11 的 RouterTLM `buffer_size` 8 通过） |
| **AC-06** 无 params 的夹具实例 ⇒ 默认值落成员 | ✅ PASS | `AC-06` 用例：夹具成员初值 **99/999（故意非法）** ⇒ `gain()==2` / `offset()==0`，对「默认值未落成员」有判别力 |
| **AC-07** RouterTLM 拼错键 ⇒ false 且报错含键名 + 失败源非模块手写 | ✅ PASS | `AC-07` 用例（**未**注册夹具 ⇒ 排除夹具蒙混）+ 直接 `set_config` 捕获 `param_name=="buffer_sizes"`;`grep -cE 'unknown\|buffer_sizes' src/tlm/router_tlm.cc` = **0** |
| **AC-08①** NICTLM 必填零变化 | ✅ PASS | `cpptlm_tests "Schema: NICTLM*"` ⇒ 3/3 通过;`[config][schema]` ⇒ 14/14 |
| **AC-08②** `validateConfig` 非注释行 RouterTLM 零命中 | ✅ PASS | `awk '/^bool ModuleFactory::validateConfig/,/^}/' … \| grep -v '^\s*//' \| grep -c RouterTLM` = **0** |
| **AC-09①** 存量 BSP-3 绿（credit 同源） | ✅ PASS | `[router]` ⇒ 118 assertions / 18 cases 全绿（含 BSP-3） |
| **AC-09②** 同值二次加载不刷新已消耗 credit | ✅ PASS | `AC-09②` 用例：首次转发 4（= depth）⇒ 二次同值加载后转发 **0** |
| **AC-10** 默认值不写回 JSON | ✅ PASS | `AC-10` 用例（夹具 + RouterTLM 各一条：`CHECK_FALSE(params.contains("gain"/"buffer_size"))`） |
| **AC-11** 8.0 / true / 1e20 / -1 拒绝；整数 8 通过 | ✅ PASS | `AC-11` 用例（夹具 `gain` + RouterTLM `buffer_size` / `credit_timeout` 双载体） |
| **AC-12** legacy 扁平：成功且不报未知键 + 缺 node_x 仍失败 + NICTLM 不变 | ✅ PASS | `AC-12` 用例三 SECTION;`factory.getInstance<tlm::RouterTLM>("r_flat")->node_x()==1` 证明参数真的读到 |
| **AC-13** `vc_count` 不入声明 | ✅ PASS | `grep -cE 'load_param[^;]*"vc_count"' src/tlm/router_tlm.cc` = **0** |
| **AC-14** `ignore_param` 调用点与表 B 对齐 | ✅ PASS | `grep -rn 'ignore_param(' src/ include/` ⇒ 调用点 **1**（`src/tlm/router_tlm.cc:87` 的 `routing`）+ 定义 1;`docs/implementation/module-param-baseline.md` 表 B-6 有对应条目 |
| **G-1** 幂等 | ✅ PASS | `G-1` 用例 |
| **G-2** 嵌套 RouterTLM 缺 node_x ⇒ 内部实例未被创建 | ✅ PASS | `G-2` 用例（`outer->getInternalFactory().getInstance("inner_r") == nullptr`）|

**结果**：AC-01~AC-14 + G-1/G-2 **全部 PASS**，0 失败。

---

## 2. 边界与错误路径（第 4 步，非 AC 显式覆盖的补充取证）

| 边界 | 结果 | 证据 |
|---|---|---|
| 空 `params` 对象（已准入类型） | 默认值生效、必填缺失即失败 | `AC-06`（夹具）+ 全量回归中的 `configs` 用例 |
| 完全无 `params` 键（已准入类型） | 仍加载一次（`kEmptyParams`） | `AC-06` 用例走的就是这条路径 |
| legacy 扁平 + 缺必填 | 失败（豁免不吞必填） | `AC-12` SECTION 2 |
| 无 `params` 且无扁平（`configs/noc_mesh.json`） | 失败（改动前后一致） | §3 基线证据 |
| 嵌套容器内部装配失败 | 内部实例不创建（外层仍 true，**既存行为**） | `G-2`;登记为 change 已知残留 R-6 |
| 单文件探针编译（`cep_*_probe.cc`） | 通过 | `ctest -R cep_tilelink_engine_failclosed` → Passed（builder 曾因 `is_param_admitted` 在 .cc 里而失败，改 header-only 后修复） |

---

## 3. AC-04 的基线说明（偏差披露）

AC-04① 要求「**改动前**先跑并留证据」。实际执行情况：

- **未**构建 BASE 版的二进制实跑（需 checkout + 全量重编译，成本与收益不成比例）。
- 改用 **BASE commit 的代码 + 配置数据静态重建**：`git show fa2c4916:src/core/module_factory_validate.cc` 显示
  RouterTLM 在「无 `params` 且无扁平 `node_x`」时走 `else if (type == "RouterTLM" || type == "NICTLM")` ⇒ `return false`。
  `configs/noc_mesh.json` 的 4 个 RouterTLM 实例正是这一形态（无 `params`、无 `node_x`）⇒ **改动前必然失败**。
- 该推导是**确定性**的（CFG-08 分支无运行时依赖），可判定。
- 另外 `grep -rl 'noc_mesh.json' test/ scripts/ examples/` ⇒ **零引用**，说明它不是活路径。

⇒ 判定：AC-04① 以「静态重建基线」**部分满足**，结论（`noc_mesh.json` 属既存失败）可判定且证据充分。

---

## 4. 遗留清扫复核（第 5 步）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK（本次 11 个改动文件） | **0** |
| 临时调试输出（本次改动块内） | **0**（`module_factory.cc` 里既有 CEP IAG 闸的 `fprintf` 不在本次改动块，已用 `git diff \| grep '^+'` 确认） |
| 被注释掉的代码 | **0** |
| 被 skip / hide 的测试 | **0** |
| 临时诊断标记（`TMPDIAG` 等） | **0**（builder 期的临时诊断已还原并验证：`grep -c TMPDIAG` = 0） |
| 未替换的 mock | 无 —— `ParamFixtureTLM` 是**测试专用夹具**（`test/param_fixture_module.hh`），`grep -rn ParamFixture src/ include/` ⇒ 零命中（未进生产） |
| 临时 fallback 分支 | 仅 `module_factory.cc` 的 `if (!cfg_src) { /* 无参数可加载 ⇒ 跳过 */ }`，是有意保留的「未准入类型零变化」路径，非临时兜底 |
| 代码格式（本次改动文件） | ✅ 5 个文件已 clang-format;剩余 9 个「需要格式化」**均为本次范围外**的既有漂移（`src/tlm/nic_tlm.cc` / 3 个 `test_cep_*` / `test_nictlm_credit_filter.cc` / `test_noc_mesh_binding_semantics.cc` / `test_router_buffer_size_param.cc` / `test_router_local_port_credit.cc` / `test_simmodule_exposed_port_wiring.cc`）⇒ **记录不清理**（scope 纪律） |
| tasks.md 完成度 | 44/52 已勾；未勾 = T0.1（本次已执行）+ T7.1~T7.6（收口序列） |

---

## 5. T0.1 — B-1 复核（本 change 的独立签收）

已追加到 `.rddf/reviews/ADR-X.17.md` 的 `## B-1 复核（实施 change 期）` 段：
**NICTLM 的 CFG-08 三处分支（扁平回退 / 逐键检查 / 无 params 源即失败）全部保留**，
RouterTLM 在 `validateConfig` 内零引用，三条依赖测试（`test_config_schema_validation.cc`）全绿。
⇒ **B-1 闭合**。

> ⚠️ **独立性局限（如实披露）**：本会话中 planner（立项）/ builder（实施）/ verifier（验收）
> 由同一 agent 切换角色完成。T0.1 要求的「非本 change 作者」在单 agent 环境下无法真正满足。
> 实际保障 = ① verifier 阶段**只读**（未改实现，本报告所有结论均可由命令复现）
> ② AC 判据由 proposal 预先确定（非事后发明）③ 关键判据全部为**可机械重跑**的命令。
> 若需更强的独立性，应另开会话以 verifier 角色复核本报告。

---

## 6. 失败项与路由建议

**无。**

---

## 7. 复现命令

```bash
cd /workspace/project/CppTLM
./build.sh                                    # 0 error
./build/bin/cpptlm_tests                      # 883191 assertions / 2371 cases
./build/bin/cpptlm_tests "[param]"            # 105 assertions / 28 cases
./build/bin/cpptlm_tests "[router]"           # 118 / 18
./build/bin/cpptlm_tests "[config][schema]"   # 14 / 14
python3 -m pytest test/python/ -q             # 232 passed
ctest --test-dir build -R cep                 # 100/100
./scripts/build/format.sh --check             # 仅剩 9 个范围外既有漂移
openspec validate cpptlm-module-param-read-declares --strict   # valid
awk '/^bool ModuleFactory::validateConfig/,/^}/' src/core/module_factory_validate.cc \
  | grep -v '^\s*//' | grep -c 'RouterTLM'    # 0
grep -cE 'load_param[^;]*"vc_count"' src/tlm/router_tlm.cc     # 0
git diff --name-only fa2c4916 -- configs/ | wc -l              # 0
```
