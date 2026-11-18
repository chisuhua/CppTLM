# 验收报告 —— cpptlm-module-param-effect-landing（阶段 A）

> **判据来源**：`openspec/changes/cpptlm-module-param-effect-landing/proposal.md` 的 `## 验收标准` 段（AC-01~AC-09 + G-1/G-2）
> **执行面**：**本地验收**（`verifier 产出契约` 的 1~5 步）—— 分流条件 ② 不满足：`.rddf/state/iteration.json` **不存在** ⇒ rdd-builder 的 P3 archive gate 未触发 ⇒ **不走 `rdd-verifier` skill**
> **已用轮次**：**2 / 3**（`RDDF_VERIFIER_MAX_LOOPS=3`）—— 第 1 轮 AC-01 失败（判据不可达）⇒ 改判后第 2 轮复验 **PASS**（见 §7）
> **验收人**：verifier（**只验不改** —— 本轮零实现改动）
> **日期**：2026-11-18

---

## 0. 结论（**第 2 轮已更新 —— 最终 PASS**）

> ⚠️ 本节是**第 1 轮**的原始结论，保留供追溯。**最终结论见 §7：PASS（AC-01 改判后全绿）**。

**第 1 轮：不通过（1 项失败）** —— AC-02 ~ AC-09 + G-1/G-2 **全部通过**;⛔ **AC-01 不通过**，且失败形态是**判据不可达**（不是实现缺失）。

| 项 | 判定 |
|---|---|
| AC-01 | ❌ **不通过**（`proposal_drift` —— 判据在当前实施形态下**恒不成立**） |
| AC-02 ~ AC-09 | ✅ 通过 |
| G-1 / G-2 | ✅ 通过 |

⇒ **路由建议：回 planner 修 AC-01 的判据**（⛔ 不是回 build 改代码 —— 实现是对的）。

---

## 1. AC-01 ❌ —— 判据不可达（核心发现）

**判据原文**（2026-11-18 改判版）：
> D-1 表登记的**每个类型**都能在盘点脚本输出中找到对应条目 —— 或在 `rows[].type` 里，或在 `undefined_types` 里。

**取证命令**：
```bash
python3 scripts/test/param_effect_inventory.py --json
```

**实测**：脚本 `rows` 只有 **25 个类型**;D-1 表中登记的 **8 个类型完全不在输出中**（既不在 `rows` 也不在 `undefined`）：

```
PcieControllerIP · PcieMemoryDevice · ArbiterTLM · SocInternalNoc
CrossbarTLM · StreamingMultiprocessorTLM · CoherentXBarTLM · GpuMeshNoC
```

**根因（可判定）**：**盘点的口径是「配置里的『类型 × 键』」** ⇒ 本 change 的处置**把键从配置里清掉了**（态② 停止产出）⇒ 配置里无键 ⇒ 脚本不输出该类型。

⇒ **「处置完成」与「D-1 表类型 ∈ 脚本输出」是互斥的** —— 判据在实施形态下**恒不成立**。这不是实现缺陷（12 键全部有落点，见 §2）。

**⚠️ 这是我（planner 侧）2026-11-18 改判时引入的疏漏**：改判时假设「D-1 表的类型仍在配置里」，但**处置本身会清空配置**。原判据（数字直比）与现判据（类型存在性）**都不可达**。

**建议修法**（供 planner 参考，verifier 不改）：
> AC-01 改为：D-1 表登记的**每个类型**，要么在脚本输出中（`rows[].type` ∪ `undefined_types`），要么在 **D-1.6 的处置记录**中有「**配置已清空 / 类型已归档**」的逐行登记（登记须含处置结论 + 载体位置）。
> 判据载体 = `design.md` D-1.6.2~D-1.6.6 的逐类型行 + 台账表 B 的对应条目。

或更简：**AC-01 只判「D-1 表的类型集合 ⊆ 本 change 处置过的类型集合」**（处置记录见 D-1.6 + 表 B）。

---

## 2. 其余 AC 逐条取证

### AC-02 逐键三态 ✅

| 证据 | 内容 |
|---|---|
| `design.md` D-1.6.2 ~ **D-1.6.6** | 逐批裁决表：B1（T1.1~T1.5）· B2 第一批（6 类型/9 键）· **D-1.6.6（A2a/A2b/A2c：7 键态② + 2 键态① + 3 键）** |
| 态③ | `CPUTLM.pattern` + `RouterTLM.routing` 均为唯一合法值 |
| ⛔ 「待定」态 | 零（D-2 明禁;本轮新增的 12 键全部落定） |

### AC-03 态① 的行为判别测试 ✅

```bash
$ ./build/bin/cpptlm_tests "[effect]"
All tests passed (207 assertions in 33 test cases)
```

态① 的判别测试（两个不同合法值 ⇒ 可观测指标不同）：
- `TrafficGenTLM`(8) / `MemoryTLM`(4) / `GmmuTLM`(1) / `LwcBusHub`(1) / `CacheTLM`(6) —— B1
- **`GpuMeshNoC.dim`/`hops_latency`** —— 观测量 = 既有 `last_transfer_latency_cycles()`（1 vs 4 ⇒ 延迟不同;dim 2 vs 8 ⇒ 最坏路径延迟不同）
- **`CPUTLM.start_addr`/`request_interval`** —— 观测量 = `req_out().data().address`（0x2000 vs 0x3000）/ `transaction_id`（interval 1 vs 10）

⛔ 未造「只为测试存在」的观测面 ✓（`ADR-X.17` D-2.1）

### AC-04 态② 的显式失败 ✅

`[effect]` 内含 AC-04 的 6 组 case（端口数类键 / SocInternalNoc / MemoryCluster / VramController / PcieMemoryDevice / **PcieControllerIP** / **StreamingMultiprocessorTLM** / **CoherentXBarTLM**），断言「抛 `ParamValidationError` 且 `param_name == 键名`」+ 空配置正向对照（防误伤）。

### AC-05 态③ 的显式拒绝 ✅

- `CPUTLM.pattern` ⇒ `AC-05` case：合法值通过 / 缺省通过 / **`RANDOM` ⇒ 失败且含键名**
- `RouterTLM.routing` ⇒ `AC-08` case：`"XY"` 通过 / 缺省通过 / **`"YX"` ⇒ 失败且含键名**

### AC-06 零回归 ✅

| 判据 | 实测 |
|---|---|
| `cpptlm_tests` ≥ 基线 **883191 / 2371** | **883279 / 2379** ✅（+88 断言 / +8 case） |
| `configs/` 每处变更对应一个「态② 停止产出」任务 | 5 份变更全部可对应：`apu_soc_full` / `apu_soc_phase7b`（生成物重跑：去 `protocol` / `read_latency` / `write_latency` / `type` / `snoop_filter` / `domain_protocol`）· `dgpu_soc_minimal_v1` / `dgpu_soc_timing_v1` / `dgpu_soc_v2_realip_p2`（`pcie_ctrl` 3 键 + `pcie_memory.capacity_gb`） ✅ |

### AC-07 台账一致 ✅

```bash
$ grep -rn "ignore_param(" src/ include/   # 排除定义处与注释
```
**5 处活调用**，全部在 `docs/implementation/module-param-baseline.md` **表 A** 登记（「已消费但**不经** `load_param`」，消费方逐键标注）：
- `TrafficGenTLM.start_addr`/`end_addr` —— 模块内 `read_addr_key()`（表 A `:21`）
- `TrafficGenTLM.hotspot_addrs`/`hotspot_weights` —— 模块内 `parse_hotspot()`（表 A `:22`）
- `GmmuTLM.tlb_size`/`tlb_miss_latency_cycles` —— **板级直读** `dgpu_board_shell.cc:1056-1057`（表 A `:23`）

⚠️ `RouterTLM.routing` 的 `ignore_param` **已移除**（改为态③）—— 台账 B-6 已同步。

### AC-08 `routing` 处置落地 ✅

| 子判据 | 证据 |
|---|---|
| 处置为态② 或 ③（⛔ 不停在 `ignore_param`） | **态③**：`src/tlm/router_tlm.cc` 读入 + 校验 ∈ {`"XY"`} + 非法值失败;`ignore_param("routing")` **已删** |
| `GpuNoC` 内部配置生成与该处置一致 | `GpuNoC::simulate_instantiate` 生成的值恒为 `"XY"`（合法）⇒ 一致 |
| `[simmodule][infra]` 的 GpuNoC 用例绿 | `All tests passed (23 assertions in 7 test cases)` ✅ |

### AC-09 未准入类型零变化 ✅

```bash
$ ./build/bin/cpptlm_tests "[param]"           → All tests passed (312 assertions in 61 test cases)
$ ./build/bin/cpptlm_tests "[router]"          → All tests passed (122 assertions in 19 test cases)
$ ./build/bin/cpptlm_tests "[config][schema]"  → All tests passed (14 assertions in 14 test cases)
$ ./build/bin/cpptlm_tests "[cep]"             → All tests passed (12413 assertions in 540 test cases)
```

### G-1 / G-2 ✅

| 护栏 | 实测 |
|---|---|
| G-1 已准入类型（`RouterTLM`）既有判据零变化 | `[param]` **61 cases**（原写 28，因本 change 扩充）全绿 · `[router]` 19 cases 全绿 |
| G-2 全量 + pytest + ctest | `cpptlm_tests` 883279/2379 · `pytest` **232 passed** · `ctest` **182/182**（含新增 `rddf_stage_gate`） |

---

## 3. 边界与错误路径（verifier 顺序第 4 步）

| 边界 | 判据 | 证据 |
|---|---|---|
| 空配置 ⇒ 不失败（防误伤） | 每个 AC-04/AC-05 case 都带一条正向对照 | `[effect]` 内 CHECK_NOTHROW 分支 |
| 态③ 的**缺省**（键缺失）⇒ 不失败 | `CPUTLM.pattern` / `RouterTLM.routing` 的 `none` 分支 | 同上 |
| 覆写基类时继承的拒绝**不得失效** | `CoherentXBarTLM` 覆写 `on_config_loaded` 后，继承自 `CrossbarTLM` 的 `ports` 拒绝仍生效 | `AC-04: CoherentXBarTLM 两键` 内含 `expect_param_error(..., "ports")` |
| 配置改了 `start_addr` ⇒ 游标必须同步 | 否则首请求仍用旧默认地址 | `AC-02: CPUTLM.start_addr` 断言首请求地址 = 配置值 |

---

## 4. 遗留清扫复核（verifier 顺序第 5 步）

| 检查项 | 结果 |
|---|---|
| TODO / FIXME / 临时诊断日志 | **零命中**（本 change 触达的 9 个源/测试文件） |
| 被 skip 的测试 | **零命中** |
| 未替换的 mock / 临时 fallback | **零命中** |
| `ignore_param` 活调用 | 5 处 —— **全部为表 A 合规登记**（非「长期掩盖」） |
| 生成器漂移 | **已修**：`generate_apu_soc.py` 不再 emit 已态② 的键（`CacheTLM.protocol` · `MemoryTLM.read_latency`/`write_latency`/`type` · `CoherentXBarTLM.snoop_filter`/`domain_protocol`） |

---

## 5. 失败项与路由

| # | 失败项 | 类别 | 路由 |
|:--:|---|---|---|
| 1 | **AC-01 判据不可达** —— 处置把键从配置清掉 ⇒ 脚本不再输出这些类型 ⇒ 「类型 ∈ 脚本输出」恒不成立 | **`proposal_drift`**（实现正确，判据错） | **回 planner 改 AC-01 判据**（建议见 §1）;⛔ **不回 build** |

---

## 6. 已用轮次

| 轮次 | 结果 |
|---|---|
| 1 / 3 | ❌ 不通过（AC-01 判据不可达，其余全绿） |

**结论**：**不可归档** —— AC-01 修好后需**重新验收**（第 2 轮只需复验 AC-01 + 确认其余未回退）。

**关联**：`design.md` D-1.2 / D-1.6.2~D-1.6.6 · 台账 `obj-cep-platform.md` Entry #484 · `scripts/test/rddf_ac_carrier.tsv`

---

## 7. 第 2 轮复验（2026-11-18）—— **PASS**

**触发**：第 1 轮判 AC-01 **判据不可达**（`proposal_drift`）⇒ planner 已按 §1 的建议**二次改判** AC-01（v1「类型 ∈ 脚本输出」→ **v2「类型在台账表 A/B/§2bis 至少一行登记」+ 键在 D-1.6 有裁决行**）。

### 7.1 AC-01（v2）取证 ✅

```bash
# D-1.2/1.3 的类型 → 台账登记
python3 - <<'EOF'  # 提取 D-1.2 段的类型名,逐个查台账
...
EOF
```
**实测**：D-1 段提取 **15 个类型**名，**台账无登记者 = 0** ⇒ **通过**。

| 类型 | 台账 | 类型 | 台账 |
|---|:--:|---|:--:|
| `ArbiterTLM` | ✓ | `MemoryTLM` | ✓ |
| `CPUTLM` | ✓ | `PcieControllerIP` | ✓ |
| `CacheTLM` | ✓ | `PcieMemoryDevice` | ✓ |
| `ComputeUnitTLM` | ✓（§2bis） | `SocInternalNoc` | ✓ |
| `CrossbarTLM` | ✓ | `StreamingMultiprocessorTLM` | ✓ |
| `GmmuTLM` | ✓ | `TCCTLM` | ✓（§2bis） |
| `LwcBusHub` | ✓ | `TrafficGenTLM` | ✓ |
| `routing`（D-1.3 键列） | ✓（B-6） | | |

**键的裁决行**：`design.md` D-1.6.2~**D-1.6.6** 共 9.4 KB 逐批裁决表 ⇒ 覆盖 B1 + B2 第一批 + **A2a/A2b/A2c** ✅

### 7.2 其余 AC 未回退 ✅

| 判据 | 实测 | 与第 1 轮对比 |
|---|---|---|
| `cpptlm_tests` | **883279 / 2379** | 持平 ⇒ 零回归 ✓ |
| `pytest` | **232 passed** | 持平 ✓ |
| `ctest` | **182 / 182** | 持平 ✓ |
| 五闸 | `agents_md_lint` · `docs_sync_check --strict` · `cep_docs_sync_check` · `cep_doc_ref_check` · `rddf_stage_gate`（**0 项跳过**）全绿 | 持平 ✓ |

### 7.3 最终结论

**PASS** —— AC-01（v2）~ AC-09 + G-1/G-2 **全部通过**;遗留清扫复核零命中;生成器漂移已修。

⇒ **允许归档**（`openspec archive`，⛔ 不用 `--skip-specs` —— 实现已落地）。

**⚠️ 归档时要做的三件**（不在验收范围，属 builder 的 T7.2/T7.3）：
1. KPI 回填 **7 处**（`3/3 → 2/3`，含 `AGENTS.md` 的 KPI 段）
2. 新增 main spec `openspec/specs/module-param-effect-landing/spec.md` 的 **Purpose 手写**（归档默认留 TBD）
3. ⚠️ **冻结面裁决**：新 main spec **非 `cep-` 前缀** ⇒ 按 `cep_freeze_check.sh` 的**逐路径显式清单**口径**默认不入** M1 冻结集 ⇒ 须在台账 Entry 里**显式裁定**「不入」。

**已用轮次**：2 / 3。
