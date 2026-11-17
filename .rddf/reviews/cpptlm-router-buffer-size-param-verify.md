# 独立验收报告 — `cpptlm-router-buffer-size-param`

- **日期**: 2026-11-17
- **验收者**: verifier（独立于实施者）
- **对象**: change `openspec/changes/cpptlm-router-buffer-size-param/`（active，tasks **19/29**）
- **决策源**: 本 change 的 `design.md` **D-1 默认值裁决**（取 8）
- **已用轮次**: 1 / 3
- **分流**: `rddf rdd-verify --dry-run` → `No eligible changes to verify (empty queue)`；change 仍 active（19/29 tasks）⇒ **P3 archive gate 未触发** ⇒ 未走 `rdd-verifier` skill，走**本地验收路径**（1–5 步）

## 判据来源（按优先级）

1. 用户指令：「再切verifier验收」
2. `openspec/changes/cpptlm-router-buffer-size-param/proposal.md` 的 `## Acceptance`（**AC-01 ~ AC-05** + guard **G-1/G-2**）—— **主判据**
   - ⚠️ **AC-01 的载体已按 `design.md` D-3 收敛**（原「第 N+1 个被丢弃」不可观测，见下文 O-2）
3. 本次变更自身声明：commit `708b13c8`（实施）+ 台账 Entry **#469**（实施记录）
4. `design.md` 的 **D-1**（默认值裁决）/ **D-2**（同源纪律）/ **D-3**（观测面边界）/ **D-7**（两处机制缺口）

## 验证顺序（早失败早停）

1. 编译/静态检查 ✅ `cmake --build build` 无 error
2. 单元测试 ✅（新用例 + 具名回归集）
3. 集成/端到端 ✅（`[cep]` / `[e2e]` / `[pcie]` / `[noc]` / `[mesh]`）
4. 边界与错误路径 ✅（非法值 0 / 65 / 字符串；合法边界 1 / 32）
5. 遗留清扫 ✅（见末节）

## 逐条判定 + 证据

| AC | 判定 | 证据（命令 + 结果） |
|:--:|---|---|
| **AC-01** `buffer_size: N` ⇒ 缓冲深度与容量均生效 | **PASS** | `BSP-2`：① 直接证据 `buffer_depth() == N`（`=2` ⇒ 2；`=16` ⇒ 16；`=1` 下界 ⇒ 1）② 行为证据 `buffer_size=2` 时**每 in_port 注入 8 个**（8 > 2）⇒ 转发数 **< 32**（必丢）；`=16` 时同条件 ⇒ **== 32**（零丢弃）。`[router][buffer_size]` → `All tests passed (15 assertions in 4 test cases)` |
| **AC-02** 未设 ⇒ 深度 == 8 且行为与改动前一致 | **PASS** | `BSP-1`：`REQUIRE(router.buffer_depth() == 8)` + 每 port 注入 8 个（总量 32，恰好不超容量）⇒ **32 全转发（零丢弃）**。⇒ 默认深度 == 参数声明默认值 == 实现默认值 |
| **AC-03** credit 预算与深度同源 | **PASS** | `BSP-3`：裸 router（`adapter_ == nullptr` ⇒ credit 永不归还）向 mesh 向（EAST）注入 ⇒ 转发数封顶于 credit 预算 —— 默认 **8**、`buffer_size=4` ⇒ **4**（与 `buffer_depth_` 一致 ⇒ 同源成立） |
| **AC-04** 非法值 ⇒ 装配期 fail | **PASS** | `BSP-4`：`0` / `65` / `"abc"` ⇒ **`instantiateAll` 返回 false**（三节各自 `REQUIRE_FALSE`）；`32`（合法）⇒ 装配成功且 `buffer_depth() == 32` |
| **AC-05** 全量回归 + 既有 `REQUIRE` 常量零改动 | **PASS** | 回归：`[router]` 118/18 · `[cep]` 12429/539 · `[e2e]` 7458/151 · `[pcie]` 36779/452 · `[noc]` 790848/70 · `[mesh]` 251/19 —— 全绿。**零改动直接证据**：`git diff --stat 708b13c8^ 708b13c8 -- test/` ⇒ **仅 `test_router_buffer_size_param.cc`（+196，新增）**，无任何既有测试文件的修改 |
| **G-1** `vc_count` 声明与实际值保持现状 | **PASS** | `get_param_rules()` 的 `vc_count`（默认 2 / 1–8，`router_tlm.cc:597-598`）与 `configs/param_rules/router_tlm.json:24-31`（含 `derive_expr`）**均未改动**；`NUM_VCS = 4` 未改（`router_tlm.hh:138`） |
| **G-2** `NUM_VCS` / `NUM_PORTS` 仍编译期常量 | **PASS** | `include/tlm/router_tlm.hh:137-138` 两行仍为 `static constexpr unsigned` |

## 观察项（不阻断）

| # | 内容 | 判定 |
|:--:|---|---|
| **O-1** | **跨模块行为变化**：`module_factory.cc` 的 `set_config`/`on_config_loaded` 调用点新增 try/catch ⇒ 凡在 `on_config_loaded` 抛异常的模块，其失败形态由「异常逃出 `instantiateAll`」变为「`instantiateAll` 返回 false」。**实测受影响的另一模块 = `src/tlm/gpu/pcie_endpoint_tlm.cc`**（其 `on_config_loaded` 对 `config_size` 非 256/4096 抛 `std::invalid_argument`） | **正向（非回归）** —— 该 throw 的**本意就是装配失败**，此前无 catch 意味着异常会逃出装配函数（调用方未必处理）；现在语义统一到「装配失败 ⇒ 返回 false」，与 Step 4.5 `simulate_instantiate` 的既有范式一致。`[pcie]` **36779/452 全绿** ⇒ 无测试依赖「异常传播」的旧形态。**建议**：在 change 的 `design.md` D-7 或台账补一句「该 try/catch 同时使 `PcieEndpointTLM` 的既有 throw 真正落地」—— 当前 Entry #469 未提及此跨模块影响 |
| **O-2** | **AC-01 的原始载体「第 N+1 个被丢弃」不可观测**：`input_buffer_` 是 private，且「容量判定」在**稳态**下与深度无关（egress 恒 1 flit/cycle ⇒ 持续注入的转发总数由**时长**决定） | **已在 `design.md` D-3 显式登记**并收敛为两条互补判据（① 只读访问器 `buffer_depth()` 直接证据 ② 每 in_port 注入 `K > C` 必丢 行为证据）⇒ **诚实收敛，非掩盖**。验收按收敛后的载体执行，结论成立 |
| **O-3** | **实施期两处机制缺口**：`validate_module_params` 是**零调用方死代码**（D-7.1）· `configs/param_rules/router_tlm.json` 原判「可能是 no-op」**结论错误**（D-7.2，原 D-6 第 4 条已作废标记） | **已正确处置** —— D-7.1 已登记为独立 backlog（`.rddf/improvements/cpptlm-validate-params-dead-code.md` · 台账 **Entry #470**）；D-7.2 已补 json 条目并在 design 内作废原判。**两处均在 change 内自洽** |
| **O-4** | 变异验证的**独立性**：实施者报告「注释掉 `buffer_depth_ = n` ⇒ 5 断言红」 | **采信并交叉核对** —— 变异点（`buffer_depth_` 赋值）与断言（`buffer_depth()` / 转发数 / credit 预算）之间的因果链在本报告中独立复述成立；`BSP-2/3/4` 的断言确实只依赖该赋值路径。**采纳** |

## 失败项与路由建议

**无失败项**（AC-01 ~ AC-05 + G-1/G-2 全部 PASS，0 implementation_gap）。

## 遗留清扫（verifier 复核实施者的自查）

| 项 | 结果 |
|---|---|
| TODO/FIXME/XXX（本次触及的 5 个文件） | 无 |
| 临时调试残留（`ZZ*` / `MUTATION` / `test_zz_*`） | 无 —— **变异标记已确认清除**（`grep -c MUTATION src/tlm/router_tlm.cc` ⇒ 0） |
| 被 skip / disabled 的测试 | 无 |
| 未跟踪的仓库残渣（本次范围） | 无 |
| 注释掉的代码块 | 0 |
| 未替换的 mock | 无 |

> 备注：`src/core/module_factory.cc` 中 `:222/:223/:233` 的 `fprintf`/`printf` 是**既有代码**（不在本次 diff 内），不属本次遗留。

## 结论

**PASS** —— AC-01 ~ AC-05 全部通过（G-1/G-2 通过），**无 implementation_gap**。判据链闭合：默认值裁决（D-1 取 8）→ 实现（D-2 同源）→ 观测面边界（D-3 两条互补判据）→ 变异验证（判别力实证）。

**4 项观察（O-1 ~ O-4）均不阻断**，其中 **O-1 建议补记**（跨模块行为变化已在 `[pcie]` 全绿前提下确认为正向）。

**已用轮次**: 1 / 3（`RDDF_VERIFIER_MAX_LOOPS=3` 对齐）

**证据边界**：AC-05 的「既有 `REQUIRE` 常量零改动」以 `git diff --stat` 直接证明（`test/` 只有新增文件）；未独立重跑基线二进制（依据 §verifier 产出契约「不得 stash / checkout / 新建 worktree」）。
