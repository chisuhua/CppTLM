# 验收报告 — cpptlm-module-param-migrate-batch2（阶段 C / C2）

- **验收角色**：verifier（只读 + 取证；**未修改任何实现**）
- **日期**：2026-11-19
- **被判对象**：commits `4c39a179` → `8a028a3c`（实施 8 个 + 验收返工 3 个 + ADR 签发 / 示例 2 个；`git diff 32848d0e..HEAD`）
- **判据来源（按优先级）**：
  1. `openspec/changes/cpptlm-module-param-migrate-batch2/proposal.md` 的 `## 验收标准`（AC-01~AC-10 + 回归护栏 G-1/G-2）
  2. 同 change 的 delta spec（3 份：`module-param-load-declares` / `param-derivation` / `param-validation`）
  3. `docs/adr/ADR-X.17-module-param-read-declares.md`（D-1/D-2/D-3/D-5/D-6）+ `docs/adr/ADR-DGPU-15-pcie-endpoint-ip-cpptlm-archforge-boundary.md`（`PcieEndpointIP` 三键的四态处置，Proposed 2026-11-19）
- **已用轮次**：**2 / 3**（`RDDF_VERIFIER_MAX_LOOPS`）
- **判决**：✅ **PASS** —— 第 1 轮 5 项 finding 全部闭合;**0 项未闭合 finding**;新增 1 项**非阻断**整改（N-1，纯格式）。⇒ 放行 T8.7（KPI 回填）/ T8.9（归档）

> ⚠️ **分流说明**：`rdd rdd-verify --dry-run` ⇒ `No eligible changes to verify`（P3 archive gate 未触发 —— 本仓无 `.rddf/state/iteration.json`）⇒ 走**本地验收**，不调用 `rdd-verifier` skill。
> 本地验收的额外门「任务完成」按 `tasks.md` 判定：实现类 task 全部勾选（54/57），未勾的 3 项为 T8.7（KPI 回填）/ T8.8（本验收）/ T8.9（归档）—— **收口序列**，须在验收通过后执行。

---

## 1. 逐条判定（第 2 轮实测，每条均有工具证据）

| AC | 判定 | 证据（第 2 轮实测） |
|:--:|---|---|
| **AC-01** 准入表全量化 | ✅ **PASS** | 机械对拍：design D-2 清单 20 行 − `PcieEndpointTLM`（显式排除）+ `RouterTLM`（C1 已迁）= 期望 **20**;`param_admission_table()` 实际 **20** ⇒ **逐项相等，差集 = ∅**。`[batch2][admission]` ⇒ 99 assertions / 3 cases 全绿 |
| **AC-02** 逐类型行为判别 | ✅ **PASS** | `[batch2]` ⇒ **426 assertions / 39 cases 全绿**;第 1 轮 F2~F5 的 4 处缺口已按双值判别补齐（`translate_latency` 接入 timing 路径：`sdma_engine_tlm.cc:192,291`;`[batch2][sdma][timing]` / `[batch2][apusoc][f3]` / `[batch2][f4]`×4 / `[batch2][f5][router]`）;`cu_template` 的「仓内仅 1 个合法 CU 蓝图」以「值落成员 + 子实例真实创建」替代双值，**显式论证豁免已登记**（tasks T8 表） |
| **AC-03** 必填真值唯一 | ✅ **PASS** | `validateConfig` 内 per-type 硬编码分支 **0**（`module_factory.cc:171` 仅一行历史注释）;`getParamRulesRegistry` / `registerParamRules` / `validate_module_params` 生产调用方 **0**（唯一命中为 `param_admission.hh:16` 的文档注释）;唯一必填来源 = 无默认值的 `load_param` |
| **AC-04** ParamRules 机制退役 | ✅ **PASS** | `include/core/param_rules.hh` **已不存在**;`param_rules.hh` 全仓命中仅 1 处**文档注释**（`param_parser.hh:12` 说明 `ParamType` 迁入）;无「注释掉的死代码」形态残留 |
| **AC-05** 嵌套失败传播 | ✅ **PASS** | `test/test_nested_failure_propagation.cc` ⇒ 3 cases（缺必填 ⇒ 外层 false / 齐备 ⇒ 外层 true / 失败后 `instantiate_succeeded()` = false）全绿（全量运行覆盖） |
| **AC-06** 零回归 | ✅ **PASS**（附 N-1） | 全量 `./build/bin/cpptlm_tests` ⇒ **All tests passed (883874 assertions in 2417 test cases)**（≥ 基线 883191 / 2371）;`ctest` ⇒ **184 / 184**;`pytest` ⇒ **232 passed**;`git diff 32848d0e -- configs/` = 5 份文件 ⇒ 仅 ① AC-06 例外段 1 的 `"connections": []` 新增（3 份）② 例外段 2 的 3 个键删除（`num_msix_vectors` ×4 / `resizable_bars` 块 ×1）⇒ **无其它参数键增删**。⚠️ 4 份配置的 `bar_sizes` 行缩进被写坏（见 **N-1**，不影响 JSON 语义与判据） |
| **AC-07** 未知键检出扩面 | ✅ **PASS** | `[batch2]` 内 4 种不同类型拼错键（`NICTLM` / `MemoryTLM` / `CpuCluster` / `SdmaEngineTLM`）+ `PcieEndpointIP` 拼错键 case，均断言 `ParamValidationError::param_name` 等于拼错键名;`[batch2][pcie-ep]` ⇒ 21 assertions / 4 cases 全绿（**F1 的「EP 拼错键 exit=0」后果已消失**） |
| **AC-08** 板级直读键三态 | ✅ **PASS** | `PcieEndpointIP` 的 `load_param` 实际调用 **0**（`grep` 的 2 处命中均为注释）⇒ 键面 = 11 `ignore_param`（含 `resizable_bars`）+ 2 `reject_unsupported_param` ⇒ **无「`load_param` 声明 + 板级直读」双写**;`MemoryTLM` / `VramControllerTLM` / `GmmuTLM` 同款（AC-08 case 覆盖，全绿） |
| **AC-09** 台账一致 | ✅ **PASS** | 表 B 三键与实现逐条一致：**B-21** `resizable_bars` = 「已登记 + 待修」↔ `pcie_endpoint_ip.cc:423` 的 `ignore_param`;**B-22** 两 MSI-X 键 = 「已显式拒绝」↔ `:197` 的 `reject_unsupported_param`;**B-23** `translate_latency` = 已消费 ↔ `sdma_engine_tlm.cc:192,291`;§1.1 摘要行（11 登记 + 2 拒绝）与 B-21/B-22 一致，**台账内部无互斥** |
| **AC-10** spec 一致 | ✅ **PASS** | `openspec validate --changes --strict` ⇒ `✓ change/cpptlm-module-param-migrate-batch2`（1 passed / 0 failed）;「mesh 范围 1-16」在 `openspec/specs/` **零命中**（条款已订正为 `node_x < mesh_x && node_y < mesh_y`）;`configs/mesh_4x4.json` 实测含 `node_x: 0` ⇒ 与订正后条款一致 |
| **G-1** `RouterTLM` 行为零变化 | ✅ **PASS** | `[param]` ⇒ 772 assertions / 98 cases;`[router]` ⇒ 136 assertions / 20 cases;全绿 |
| **G-2** 既有副作用零变化 | ✅ **PASS** | `[pcie]` ⇒ 36773 / 452;`[cep]` ⇒ 12411 / 540;`[config][schema]` ⇒ 14 / 14;全绿 |

**步骤 4（边界与错误路径）**：装配期失败路径（拼错键 / 缺必填 / 越界 / 态④ 拒绝键 / `resizable_bars` 登记后**不**失败）逐条由 `[batch2]` 与 `[batch2][pcie-ep]` 覆盖并全绿;`cpptlm_sim` 对 EP 最小示例 exit=0。

**步骤 5（遗留清扫复核）**：见 §4。

---

## 2. 第 1 轮 5 项 finding 的闭合核对

| # | 第 1 轮内容 | 第 2 轮核对 | 结论 |
|:--:|---|---|:--:|
| **F1** | `PcieEndpointIP` 未准入（期望 20 / 实际 19）;`resizable_bars` 无消费者 ⇒ 直接准入会让真实配置装配失败 | ① 准入表 20 == 20（AC-01）;② 三键处置**按 `ADR-DGPU-15` §2.2 四态表**改为：`num_msix_vectors` / `msix_num_vectors` = **态④ 显式拒绝**（`pcie_endpoint_ip.cc:196-205`）、**`resizable_bars` = 态③ `ignore_param` 登记 + 表 B「待修」**（`:420-423`，依 `ADR-X.17` D-3b：零效果但对应已定义实现契约的键**不得**使装配失败）;③ 4 份配置 + 12 个测试内联副本已清理;④ 判据 case 改写为「写了不失败」 | ✅ 闭合 |
| **F2** | `SdmaEngineTLM.translate_latency` 声明了却无效果 | `translate_latency_` 接入 timing 路径（`sdma_engine_tlm.cc:291` 的完成周期 = `current + tlb_latency + translate_latency + 4`）;`[batch2][sdma][timing]` 断言 0/10/25 ⇒ 4/14/29 cyc | ✅ 闭合 |
| **F3** | `ApuSoC.coherent_xbar_name` 零断言 / `gpu_topology` 无断言 | `[batch2][apusoc][f3]` 补双值判别 + 内层 `gpu` 实例存在作消费证据 | ✅ 闭合 |
| **F4** | 8 个键只断言单一合法值 | `[batch2][f4]` 4 case 补双值（`l1_size` / `l2_size` / `channel_size` / `cpu_topology` / `template` / `mesh_x`·`mesh_y`）;`cu_template` 以「值落成员 + 子实例创建」替代并**显式论证豁免** | ✅ 闭合 |
| **F5** | `RouterTLM.credit_timeout` 无双值判别 | `[batch2][f5][router]` 补 0 / 5 / 50 三值判别 | ✅ 闭合 |

> **F1 描述更新（本轮订正）**：第 1 轮写「`resizable_bars` 按 `ADR-X.17` D-2 **态② 显式拒绝**」——该处置已被 `ADR-DGPU-15` 推翻：`resizable_bars` **当前零消费但对应已定义实现契约**（`resizable_bars_` 值成员 + 运行期 API `enable_resizable_bar()` 存在，缺的只是 JSON 绑定）⇒ 属**态③**，须 `ignore_param` 登记 + 表 B「待修」，**不得**使装配失败。第 1 轮原报告的「态②」一词与 C2 `design.md` D-2 的「态②」（= 已消费但不经 `load_param`）**同号不同义**，已按 ADR 的「态④」术语订正。

---

## 3. 本轮新发现（1 项，非阻断）

### **N-1 — 4 份配置的 `bar_sizes` 行缩进被写坏**（格式缺陷，非 `implementation_gap`）

| 文件 | 行 | 现状 |
|---|:--:|---|
| `configs/dgpu_board_v1.json` | 25 | `bar_sizes` 行 24 空格缩进（同级键为 12） |
| `configs/dgpu_soc_minimal_v1.json` | 27 | 同上 |
| `configs/dgpu_soc_timing_v1.json` | 20 | 同上 |
| `configs/dgpu_soc_v2_realip_p2.json` | 33 | 同上 |

**成因**：F1 返工删除 `num_msix_vectors` 行时，`bar_sizes` 行被连带重写为错误缩进（`git diff 32848d0e -- configs/` 中表现为 `-"bar_sizes"` / `+"bar_sizes"` 的成对行）。

**影响面**：JSON 语义不受影响（`json.load` 通过），**不使任何 AC 失败**;但该文件是本 change 唯一触碰的配置面，缩进错乱会误导后续 diff 审阅。
**复现**：`grep -n '"bar_sizes"' configs/dgpu_*.json` ⇒ 4 处缩进均为 24 空格。
**路由**：**builder**（回 builder 做 T8.7 时顺带修正，然后重跑 `config_type_registration_check` + `ctest`）。

---

## 4. 遗留清扫复核（步骤 5）

| 项 | 结果 |
|---|---|
| `TODO` / `FIXME` / `HACK` | 改动面的 64 个现存源文件中 **0 命中** |
| 被 `skip` / `!mayfail` / `!shouldfail` 的测试 | **0** |
| 临时调试日志（`TRACE-` / `DBG-` / `/tmp` 直写） | **0** |
| 注释掉的代码块（`// if|for|while|return|auto|std::`） | **0**（命中均为散文注释：`vram_controller_tlm.hh:33` / `pcie_endpoint_ip.cc:150,152` / `test_dgpu_msix_throttle.cc:155`） |
| 未替换的 mock | **0** |
| 半截实现 | **0**（第 1 轮 5 项 finding 全部闭合） |

**范围外存量（登记不修，沿用第 1 轮结论）**：`configs/base.json` 单独运行段错误（零引用）、`test/stress_results/` 残留目录导致 `test_stress_framework.cc:448` 失败、`test_simmodule_nested.cc` 8 层 walk 用例随机顺序偶发 SIGSEGV。

**本次不做（已登记，非缺陷）**：`include/core/sim_object.hh:491,497` 注释沿用「态②」指拒绝 —— 注释级、无行为影响;该头触达约 330 TU，为注释触发全量重编译不成比例（`ADR-DGPU-15` 术语消歧段已登记「后续触碰该头时一并改」）。

---

## 5. 路由与下一步

- **判决 = PASS（2 / 3 轮）** ⇒ 放行收口：切回 **builder** 执行
  ① **N-1 缩进修正**（4 份配置）;
  ② **T8.7 KPI 回填**（活口径：`ls openspec/changes | grep -v archive | wc -l` 与归档数、全部活口径文件**同一 commit**）;
  ③ **T8.9 归档**（`openspec archive cpptlm-module-param-migrate-batch2`，⛔ **不用** `--skip-specs`）+ 新 main spec 的 Purpose 手写。
- **不通过项**：无 ⇒ 无 finding 需回退。
- **ADR-DGPU-15** 已于本轮前签发（`Proposed`，critic 4 轮末轮 APPROVE_WITH_CONDITIONS 无 BLOCKER）;其跨仓动作（键面契约 / §9.3 新旧分列 / `ADR-SOC-11` 推进 / 对账闸）登记为 backlog `cpptlm-ep-key-surface-reconciliation`，**待 planner 立项**，不阻断 C2 归档。

---

## 6. 附：本轮取证命令（可复跑）

```bash
# AC-01 准入表 vs design D-2（机械对拍：解析 param_admission_table() 与 design.md 的 D-2 表）
python3 - <<'PY'  # ⇒ 期望 20 / 实际 20 / 差集 ∅
# AC-03 / AC-04 机制残留
grep -rn "getParamRulesRegistry\|registerParamRules\|validate_module_params" src/ include/ | grep -v param_rules.hh   # ⇒ 仅注释
ls include/core/param_rules.hh                                                                                        # ⇒ No such file
# AC-06 配置面 diff
git diff 32848d0e -- configs/ | grep -E '^[+-]' | grep -vE '^(---|\+\+\+)'                                             # ⇒ 3 键删除 + connections
grep -n '"bar_sizes"' configs/dgpu_*.json                                                                             # ⇒ N-1（4 处缩进 24 空格）
# AC-08 / AC-09 键面与台账
grep -n "load_param" src/tlm/pcie/pcie_endpoint_ip.cc                                                                 # ⇒ 0 处实际调用
grep -n "resizable_bars\|num_msix_vectors" src/tlm/pcie/pcie_endpoint_ip.cc
grep -n "translate_latency" src/tlm/gpu/sdma_engine_tlm.cc
# 测试面
./build/bin/cpptlm_tests                          # ⇒ 883874 assertions / 2417 cases
./build/bin/cpptlm_tests "[batch2]"               # ⇒ 426 / 39
./build/bin/cpptlm_tests "[batch2][pcie-ep]"       # ⇒ 21 / 4
./build/bin/cpptlm_tests "[batch2][admission]"     # ⇒ 99 / 3
./build/bin/cpptlm_tests "[param]" "[router]"     # ⇒ G-1
./build/bin/cpptlm_tests "[pcie]" "[cep]" "[config][schema]"   # ⇒ G-2
ctest --test-dir build -j4                        # ⇒ 184/184
python3 -m pytest test/python/ -q                 # ⇒ 232 passed
openspec validate --changes --strict              # ⇒ 1 passed / 0 failed
```
