# 验收报告 — cpptlm-module-param-migrate-batch2（阶段 C / C2）

- **验收角色**：verifier（只读 + 取证；**未修改任何实现**）
- **日期**：2026-11-19
- **被判对象**：commits `4c39a179` → `db51111e`（共 8 个；`git diff 32848d0e..HEAD` = 65 files / +2520 / −675）
- **判据来源（按优先级）**：
  1. `openspec/changes/cpptlm-module-param-migrate-batch2/proposal.md` 的 `## 验收标准`（AC-01~AC-10 + 回归护栏 G-1/G-2）
  2. 同 change 的 delta spec（3 份：`module-param-load-declares` / `param-derivation` / `param-validation`）
  3. `docs/adr/ADR-X.17-module-param-read-declares.md`（D-1/D-2/D-3/D-5/D-6 判据）
- **已用轮次**：**1 / 3**（`RDDF_VERIFIER_MAX_LOOPS`）
- **判决**：**FAIL —— 需返工**（3 项 `implementation_gap` + 2 项判别覆盖 gap;**0 项 `proposal_drift`**）

> ⚠️ **分流说明**：`rddf rdd-verify --dry-run` ⇒ `No eligible changes to verify (empty queue)`
> （P3 archive gate 未触发 —— 本仓无 `.rddf/state/iteration.json`）⇒ 走**本地验收**，不调用 `rdd-verifier` skill。
> 本地验收的额外门「任务完成」按 `tasks.md` 判定：实现类 task 全部勾选（T0~T7 + T8.1~T8.6 + T8.10），
> 未勾的 3 项为 T8.7（KPI 回填）/ T8.8（本验收）/ T8.9（归档）—— **收口序列**，须在验收通过后执行。

---

## 1. 逐条判定（每条均有工具证据）

| AC | 判定 | 证据 |
|:--:|---|---|
| **AC-01** 准入表全量化 | ❌ **FAIL** | 见 **F1**。机械对拍：design D-2 清单 20 行 − `PcieEndpointTLM`（显式排除）+ `RouterTLM`（C1 已迁）= 期望 20 类型;实际准入表 **19** ⇒ 缺失 `PcieEndpointIP`;多出 = **0**。`include/core/param_admission.hh` 的 `param_admission_table()` 逐字解析 |
| **AC-02** 逐类型行为判别（全部声明键） | ❌ **FAIL** | 见 **F2 / F3 / F4 / F5**。已声明键 68 个（19 类型，机械枚举 `load_param` / `load_param_opt` / `ignore_param` / `reject_unsupported_*` 调用点）;**双值判别齐备**的类型：`NICTLM` / `MemoryTLM` / `VramControllerTLM` / `GmmuTLM` / `TrafficGenTLM`（C1 判据）/ `CohIfResponderTLM` / `SdmaEngineTLM`(部分) / `CpuCluster` / `CacheCluster`(部分) / `MemoryCluster`(部分) / `ComputeCluster` / `TpcCluster` / `GpcCluster` / `GpuCluster` / `CepMeshInterconnect` / `GpuNoC` |
| **AC-03** 必填真值唯一 | ✅ PASS | ① 唯一必填来源 = `load_param` 无默认值（`src/tlm/nic_tlm.cc:33` 等）;② `awk '/^bool ModuleFactory::validateConfig/,/^}$/' src/core/module_factory_validate.cc \| grep -vE '^\s*(//\|\*\|/\*)' \| grep -cE 'type\s*==\s*"\|params_src\|ParamRules'` = **0**（per-type 硬编码分支零残留）;③ `getParamRulesRegistry` 全仓非注释命中 = **0**（机制已删 ⇒ 条件恒真） |
| **AC-04** ParamRules 机制退役 | ✅ PASS | 非注释命中全为 0：`param_rules.hh` / `validate_module_params` / `registerParamRules` / `getParamRulesRegistry` / `ParamRulesProvider` / `ParamRule\b`;`src/core/module_factory.cc` 的 `default_int\|rules_registry` 命中 = **0**（默认值注入循环已删）;`include/core/param_rules.hh` 文件已不存在;`ParamType` 迁入 `include/core/param_parser.hh`;⛔ 无「注释掉的死代码」形态残留（diff 内 `^\+\s*//\s*(if\|for\|while\|return)` = 0） |
| **AC-05** 嵌套失败传播 | ✅ PASS | 判据文件 `test/test_nested_failure_propagation.cc` ⇒ **9 assertions / 3 cases 全绿**;端到端复现：嵌套 `ApuSoC` 内 `NICTLM` 缺 `node_id` ⇒ `cpptlm_sim` **exit=1**（改动前会被吞） |
| **AC-06** 零回归 | ✅ PASS | 全量 `./build/bin/cpptlm_tests` ⇒ **All tests passed (883715 assertions in 2406 test cases)**（≥ 基线 883191 / 2371）;`ctest` **184/184**;`pytest` **232 passed**;`git diff 32848d0e -- configs/` 的 **3 份**配置经 **JSON 语义级**对比（逐模块 `params` 键集）⇒ **参数键增删 = 无**,仅顶层新增 `connections` 键 1 个/份（AC-06 例外段已登记） |
| **AC-07** 未知键检出扩面 | ✅ PASS（覆盖面有洞，见 F1） | `[batch2][admission]` ⇒ **95 assertions / 3 cases**;取证 **4 个不同类型**（`NICTLM.nodeid` / `MemoryTLM.capacit_gb` / `CpuCluster.num_cpu` / `SdmaEngineTLM.max_inflght`），断言 `ParamValidationError::param_name` 等于拼错键名;端到端：拼错键 ⇒ `cpptlm_sim` **exit=1**。⚠️ 但 `PcieEndpointIP` 上的拼错键 **exit=0**（仅 `warn_unconsumed` 告警）⇒ 该类型未被未知键检查覆盖（F1 的用户可见后果） |
| **AC-08** 板级直读键三态 | ✅ PASS | 20 个板级键逐个机械核对（`load_param` 声明? / `ignore_param` 登记?）：`MemoryTLM` 7 键 + `GmmuTLM` 2 键 + `PcieEndpointIP` 11 键 ⇒ **全部「非 `load_param` 声明 + 已 `ignore_param` 登记」** ⇒ 态② 合规，且**同一类型内无「`load_param` 声明 + 板级直读」双写** |
| **AC-09** 台账一致 | ⚠️ **部分 FAIL** | 表 A ↔ 实现：§1.1（20 键）/ §1.2（7 键）与 `ignore_param` 调用点逐条对应 ⇒ **零未登记项**;表 B 关键裁决与实现一致（B-6 `routing` 态③ / B-5 `type`·`mem_type` 态② / B-10 `memory_type` 态② / B-9 `operating_frequency_hz` 态② —— 四项逐条 grep 确认）。❌ 但 **Entry #489 宣称 `PcieEndpointIP`「已准入」与事实不符**（F1），且**表 B 缺 `translate_latency` 这条零效果键**（F2） |
| **AC-10** spec 一致 | ✅ PASS | `openspec validate --changes --strict` ⇒ `✓ change/cpptlm-module-param-migrate-batch2`（1 passed / 0 failed）;「mesh 范围 1-16」条款在 `param-validation` / `module-param-load-declares` main spec 中 **零命中**（已不存在）;`RouterTLM::on_config_loaded` 对 `node_x`/`node_y` **无范围约束**（`src/tlm/router_tlm.cc:72-75` 仅 `load_param` 必填）⇒ 与 `configs/mesh_4x4.json` 的 `node_x: 0` 事实一致 ⇒ 无残留矛盾 |
| **G-1** `RouterTLM` 行为零变化 | ✅ PASS | `[param],[router]` ⇒ **732 assertions / 105 cases** 全绿 |
| **G-2** 既有副作用零变化 | ✅ PASS | `[pcie],[cep],[config][schema]` ⇒ **49197 assertions / 1006 cases** 全绿;另 `ctest -R cep` ⇒ **100/100** |

**步骤 4（边界与错误路径）**：装配期失败路径（拼错键 / 缺必填 / 越界 / 类型不符 / 态②拒绝键）逐条实测 ⇒ 均为 **exit=1**（不静默通过）;嵌套失败传播端到端复现通过。
**步骤 5（遗留清扫复核）**：`git diff 32848d0e -- include/ src/ test/` 内 `TODO`/`FIXME`/`HACK` = **0**;`TRACE-` / `DBG-` 残留 = **0**;Catch2 `[!mayfail]` / `[!shouldfail]` = **0**;注释掉的代码块 = **0**;未替换的 mock = **0**。⇒ **清扫项通过**。

---

## 2. 发现清单（5 项）

### **F1 — `PcieEndpointIP` 未准入 ⇒ AC-01 失败 + 台账自相矛盾**（`implementation_gap` + **一处策略岔路**）

**事实**：
1. design D-2 清单 #7 列出 `PcieEndpointIP`;`tasks.md` 的 **T1.7** 条目写「**已准入**」;台账 **Entry #489** 亦写「已准入」——
   但 `include/core/param_admission.hh` 的准入表**没有** `PcieEndpointIP`（机械对拍：期望 20 / 实际 19）。
2. 用户可见后果：拼错键**不被检出** —— `cpptlm_sim` 对 `PcieEndpointIP` 的 `totally_bogus_key` **exit=0**，只有 `warn_unconsumed` 告警（`no consumer; deferred or out-of-scope`）。
3. **为什么当初没加**（不是随意的）：`PcieEndpointIP` 的 params 里还有一个键 **`resizable_bars`**（`configs/dgpu_soc_v2_realip_p2.json` 真实在用），
   它**无任何消费者** —— `attach_composition` 不读它，`warn_unconsumed` 明确报 `no consumer`；
   而 `ignore_param` 登记名单只有 11 个键，**不含** `resizable_bars` ⇒ **直接准入会让该真实配置装配失败**（违反 AC-06）。

**闭合路径（两条，需裁决选一条）**：
- **路径 A（建议 · 实现侧闭合）**：把 `resizable_bars` 按 `ADR-X.17` D-2 **态② 显式拒绝**（`reject_unsupported_param`）⇒ 清理 `dgpu_soc_v2_realip_p2.json` 里该块（**属参数面改动**）⇒ 再把 `PcieEndpointIP` 加入准入表。
  ⚠️ 技术上可行且已验证：`on_config_loaded()` → `simulate_instantiate()` → `attach_composition()` → `ignore_param(...)`
  **发生在 `end_param_load()` 的 enforce 之前**（`src/tlm/pcie/pcie_endpoint_ip.cc:179-182 / 161-177 / 369-387`）⇒ 登记来得及。
- **路径 B（planner 域）**：把 `PcieEndpointIP` 登记为**「不迁 / 显式排除」**（与 `PcieEndpointTLM` 同款）⇒ 需改 design D-2 #7 + AC-01 口径 + 台账 ⇒ **属 `proposal_drift` 口径修订**，回 planner。
- **无论走哪条**：Entry #489 / tasks T1.7 的「已准入」表述必须订正（当前台账**在撒谎**）。

### **F2 — `SdmaEngineTLM.translate_latency` 是「声明了却无效果」的键**（`implementation_gap`）

**事实**：`translate_latency_` 成员**只有写、没有读** —— 全仓 `grep -rn "translate_latency_\b" src/ include/`（排除赋值/声明/注释）只剩 **2 处**：
`src/tlm/gpu/sdma_engine_tlm.cc:191`（自读默认值）与 `include/tlm/gpu/sdma_engine_tlm.hh:194`（**本批新增的观测 getter**）。
运行期的翻译延迟实际来自 **callback 的出参 `lat`**（`sdma_engine_tlm.cc:254-257` 的 `translate_timing_cb_(..., lat)`），
**与 `translate_latency_` 无关** ⇒ 该键**零效果**。

⇒ 违反 AC-02 的「⛔ 不得有『声明了却无效果』的键」。
⇒ 且 **tasks T1.6 与 Entry #489 的表述与事实相反**（两处都写「`translate_latency` ⇒ 地址翻译的周期数」= 真实消费）⇒ 台账需订正。
⇒ 闭合方向（`ADR-X.17` D-2）：**态② 显式拒绝**（写了即失败）或**接真实消费**（把 `translate_latency_` 接到 timing 路径）。二者取其一即可消除「声明了却无效」。

### **F3 — `ApuSoC` 的两个键没有行为判别**（`implementation_gap`）

- `coherent_xbar_name`：`grep -rn "coherent_xbar_name" test/` ⇒ **0 命中** ⇒ 完全无判别。
- `gpu_topology`：`test/test_param_migrate_batch2.cc` 的 `[batch2][apusoc]` 只断言 `cpu_topology()`，`gpu_topology` 仅出现在配置里 ⇒ 无断言。
⇒ 违反 AC-02「覆盖其**全部**声明键」。

### **F4 — 8 个键只断言了**单一**合法值**（判别形式未达标）（`implementation_gap`）

| 类型 | 键 | 现状 |
|---|---|---|
| `CacheCluster` | `l1_size` / `l2_size` | 各只断言一个值（`"32KB"` / `"1MB"`） |
| `MemoryCluster` | `channel_size` | 只断言 `"2GB"` |
| `ComputeCluster` / `TpcCluster` / `GpcCluster` / `GpuCluster` | `cu_template` | 各只断言一个路径 |
| `ApuSoC` | `cpu_topology` | 只断言一个路径 |
| `CepMeshInterconnect` | `template` | 只断言一个路径 |
| `CohIfNICTLM` | `mesh_x` / `mesh_y` | 固定 `4,4`（`node_id` 有双值） |

⇒ AC-02 的形式要求是「**两个不同合法值 ⇒ 可观测指标不同**」；这些键目前只验了「值落进成员」。
（`ADR-X.17` D-6 明示「逐键核对是纪律而非机制」，W-1 已登记该风险 —— 但 AC-02 仍是**明文判据** ⇒ 判 FAIL。）

### **F5 — `RouterTLM.credit_timeout` 无双值判别**（`implementation_gap`，继承自 C1）

全仓 `credit_timeout` 在测试中只出现 **1 次**（`test/test_param_loader.cc:137-139` 的「负值 ⇒ 失败」）⇒ **无「两个不同合法值 ⇒ 可观测不同」**的判别。
（该键有 getter `credit_timeout()` 与运行期消费者 `src/tlm/router_tlm.cc:170` ⇒ **有效果**，只是判别缺失。）

---

## 3. 路由建议

| # | 分类 | 路由 | 说明 |
|:--:|---|---|---|
| **F1** | `implementation_gap`（含 1 处**策略岔路**） | **builder**（若选路径 B 则同时回 **planner** 改 D-2 #7 与 AC-01 口径） | 需要一次口径裁决：准入（并处置 `resizable_bars`）还是显式排除 |
| **F2** | `implementation_gap` | **builder** | 键的处置（D-2 态②拒绝 / 接真实消费）+ 订正 tasks T1.6 与 Entry #489 的表述 |
| **F3** | `implementation_gap` | **builder** | 补 `ApuSoC.coherent_xbar_name` / `gpu_topology` 的判别 |
| **F4** | `implementation_gap` | **builder** | 8 个键补双值判别（或按「该键合法值集合只有 1 个」显式论证豁免并登记） |
| **F5** | `implementation_gap` | **builder** | 补 `RouterTLM.credit_timeout` 的双值判别 |

**不通过 ⇒ 不得执行 T8.7（KPI 回填）与 T8.9（归档）**：按 `AGENTS.md` 的「✅ 已完成归档 = 能力已在契约内」语义，
当前状态下归档会把「准入表缺 1 个 D-2 类型」「1 个零效果键被宣称有效」写进 main spec ⇒ **归档动作本身在撒谎**。

**下一步**（按你的选择）：
- 切回 **builder** 修 F1~F5（其中 **F1 需要你拍板走路径 A 还是路径 B**）⇒ 修完再切 verifier 复验（第 2 轮）；
- 或先由 **planner** 就 F1 的口径（准入 vs 显式排除）出裁定，再回 builder。

---

## 4. 附：本次取证用到的命令（可复跑）

```bash
# 准入表 vs design D-2（机械对拍）
python3 - <<'PY'   # 解析 include/core/param_admission.hh 与 design.md 的 D-2 表
# CFG-08 残留 / 机制残留
awk '/^bool ModuleFactory::validateConfig/,/^}$/' src/core/module_factory_validate.cc \
  | grep -vE '^\s*(//|\*|/\*)' | grep -cE 'type\s*==\s*"|params_src|ParamRules'      # ⇒ 0
grep -rn "param_rules.hh\|validate_module_params\|registerParamRules\|getParamRulesRegistry" include/ src/ test/  # ⇒ 0
# 零效果键取证
grep -rn "translate_latency_\b" src/ include/ | grep -v "= \|uint32_t\|// "           # ⇒ 仅 2 处（无运行期读）
# 端到端失败传播
./build/bin/cpptlm_sim /tmp/opencode/nested_missing.json --cycles 1 ; echo $?           # ⇒ 1
./build/bin/cpptlm_sim /tmp/opencode/ep_typo.json --cycles 1 ; echo $?                 # ⇒ 0（F1 的后果）
```
