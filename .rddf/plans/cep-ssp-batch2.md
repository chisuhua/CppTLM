# cep-ssp-batch2: 实施计划（TDD 5 步）

> **For agentic workers:** Implement this plan task-by-task, following the TDD 5-step structure below. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把批 ① 的「能声明」变成「**能装配 + 能实验**」——⑦ functional-only 解释器 ⑧ 装配闸新增 SSP 绑定维 ⑨ 掩码格扩展 ⑩ 实验 4 件套 + `ADR-CEP-14` C1~C10 实例化。

**Architecture:** 三条**互不越界**的改动面：① **contract 面**（`ssp.schema.json` 的 `transition` 单元扩为两形态 + loader 的 E5/E6 校验放宽为「可为掩码对象」）② **判据面**（`instance_assembly_gate.hh` 在既有 5 层判据之外加第 6 维「SSP 绑定」，归因 `[IAG-09]`）③ **运行面**（新头 `ssp_interpreter.hh` = 状态迁移 + perm 投影 + 违例计数；4 件套复用既有产出路径，⛔ 不新建平行管线）。契约面改动落 3 份 main spec（1 份 MODIFIED×2 + 1 份 ADDED）。

**Tech Stack:** C++20 · Catch2 v3.7.0 · nlohmann::json · JSON Schema（`ssp.schema.json`）· M1 冻结面五步链（`scripts/test/cep_freeze_check.sh`）· openspec v1.14

---

> **Change name**: `cep-ssp-batch2`（slug，**不带日期前缀**）
> **配套**: `openspec/changes/cep-ssp-batch2/`（proposal / design / specs×3 / tasks）
> **来源**: `.rddf/improvements/cep-ssp-batch2.md`（✅ 已批准·待立项）
> **生成时间**: 2026-11-27 · **生成方式**: builder 手写（TDD 5 步）
> **授权**: 台账 **Entry #557**（立项 + 硬前置已解除 + 五步链步 1）
> **执行模式**: 由 P1.5 判定（本 plan task 数 = 6，`Files:` 覆盖 ~9 文件 ⇒ 预期 worktree；**以 P1.5 实测为准**）

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/tlm/cep/ssp_interpreter.hh` | **新头**（header-only）——⑦ functional-only 解释器：SSP 声明表 → 状态迁移 + perm 投影 + **洞命中违例计数**（⛔ 不产出任何时长 / hop） |
| `include/tlm/cep/ssp.schema.json` | ⑨ `transition` 单元由 `string \| null` 扩为 `string \| null \| {target_mask}`（掩码形，对齐 `tilelink_state_spec.hh:131-139`）|
| `include/tlm/cep/ssp_loader.hh` | 掩码形的 **E5/E6** 校验放宽（单元可为掩码对象；`target_mask` 位宽 == 3）|
| `include/tlm/cep/instance_assembly_gate.hh` | ⑧ 判据面新增第 6 维「**SSP 绑定**」（三方解析到同一插件；归因 **`[IAG-09]`**）|

### Tests

| File | Responsibility |
|---|---|
| `test/test_cep_ssp_runtime.cc`（**新**）| ⑦ 解释器（状态迁移 / perm 投影 / 洞命中违例计数 / 瞬态态零泄漏）+ ⑨ 掩码格两形态逐位对账 |
| `test/test_cep_instance_assembly.cc` | ⑧ 装配闸 SSP 绑定维（正控 + `[IAG-09]` 反向 fixture）|
| `scripts/test/cep_ssp_runtime_check.sh`（**新**）| ⑩ 4 件套在 SSP 协议上齐备 + 证据**档位声明** + 合同测试 C1~C10 实例化 |
| `openspec/specs/cep-ssp-declaration/spec.md` | **M1 冻结面** —— ⑨ 逐格对账扩为两形态 |
| `openspec/specs/cep-instance-assembly/spec.md` | **M1 冻结面** —— ⑧ 装配判据扩 SSP 绑定维 |
| `openspec/specs/cep-ssp-runtime/spec.md`（**新 main spec**）| **M1 冻结集新增**（glob 自动扩）—— ⑦⑩ 运行时契约 |

---

## 关键约束（设计要点摘要）

见 `design.md` 的 D-1 ~ D-8。实施时必须守住：

| # | 约束 | 一句话 |
|:--:|---|---|
| D-1 | 解释器 **functional-only** | ⛔ 不产出任何时长 / 延迟 / hop 数 |
| D-2 | 洞命中 ⇒ **违例计数 + 该次实验判「不可判」** | ⛔ 不静默跳过 · ⛔ 不 panic |
| D-3 | `transition` 单元扩为**两形态** | `target_mask` 位宽 == 3 · ⛔ 不新增其他字段 |
| D-4 | 装配闸新 tag = **`[IAG-09]`** | ⛔ 不复用 `[IAG-08]`（已被占）· 两臂共用同一份判据 |
| D-5 | 4 件套**复用既有产出路径** | ⛔ 不新建平行管线 |
| D-6 | 合同测试沿用 **C1~C10** | ⛔ 不新建合同体系 |
| D-7 | `combination_contract` = **受限用户声明** | ⛔ 不改 13 必含个数 |
| D-8 | ⑨ 的 contract 面**未入 M1** | ⛔ 本 change 不擅自扩冻结集（architect 待裁 · Entry #557 ⑥）· 用合同测试兜底 |

---

## Work Units（TDD 5 步）

### Task 1: ⑨ 掩码格扩展（contract 面 · `ssp.schema.json` + loader 的 E5/E6）

**Files:**
- Modify: `include/tlm/cep/ssp.schema.json`（`definitions.ssp.properties.transition.items.items`）
- Modify: `include/tlm/cep/ssp_loader.hh`（E5/E6 校验）
- Test: `test/test_cep_ssp_runtime.cc`

- [ ] **Step 1: Write the failing test**

```cpp
// ⑨ 掩码格:单元可为 {target_mask:3位} · 对齐 tilelink_state_spec.hh:131-139
TEST_CASE("[cep][ssp][mask] transition 单元支持掩码形(target_mask 3 位)",
          "[cep][ssp][mask]") {
    // 掩码形:1 正 1 负(TileLink Cap 形:源不定 ⇒ 目标为集合)
    nlohmann::json doc = /* 批① 的 mesif_ssp 文档 */;
    // 把某个单元从 "E" 换成掩码对象
    doc["ssps"][0]["transition"][0][0] = {{"target_mask", 0x3}};
    const auto chk = cpptlm::cep::check_ssp_document(doc);
    // 掩码形**合法** ⇒ 不得因形态判红(红因码只可能来自 E5/E6 之外的真实违规)
    CHECK(chk.ok);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][ssp][mask]"`
Expected: **FAIL** —— 现 `transition` 单元类型为 `string | null` ⇒ 掩码对象触发 schema 校验失败（`check_ssp_document` 判红）

- [ ] **Step 3: Write minimal implementation**

`include/tlm/cep/ssp.schema.json` 的 `transition` 单元类型扩为：

```json
"items": {
  "type": ["string", "null", "object"],
  "properties": {
    "target_mask": {"type": "integer", "minimum": 0, "maximum": 7}
  },
  "required": ["target_mask"],
  "additionalProperties": false
}
```

`include/tlm/cep/ssp_loader.hh` 的 E5/E6 分支：单元为 **object** 时 ⇒ `target_mask` ∈ `[0,7]`（3 位）且 ⛔ **不得**同时出现其他键；单元为 string/null 时行为**零改动**。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][ssp][mask]"`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit；所有变更将在 archive 阶段统一提交。

---

### Task 2: ⑨ 的 **V-24-2 逐位对账**扩为两形态（killer 判据）

**Files:**
- Test: `test/test_cep_ssp_runtime.cc`

- [ ] **Step 1: Write the failing test**

```cpp
// V-24-2 killer 判据扩为两形态:掩码格须与手写面**逐位**相等
TEST_CASE("[cep][ssp][mask] 掩码形态与手写 state_spec 逐位相等(V-24-2 两形态)",
          "[cep][ssp][mask]") {
    // 掩码形:1 正 1 负 ⇒ 期望 target_mask == 0x3(目标集合 = 2 个状态)
    const auto mask = /* 从 ssps.json 读掩码单元 */;
    CHECK(mask == 0x3);
    // ⛔ 双向:手写面改一位 ⇒ 必红
    // (反向 fixture:把期望值改成 0x5 ⇒ 对账闸必红)
    CHECK(mask != 0x5);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][ssp][mask]"`
Expected: **FAIL** —— 批 ① 的对账闸只覆盖单目标形态 ⇒ 掩码单元无对账载体

- [ ] **Step 3: Write minimal implementation**

把批 ① 的逐格对账用例扩为**两形态**：单目标（既有 `mesif_ssp` × `mesif_state_spec.hh::kTable`）**+** 掩码（新增一条掩码形态的 SSP × 对应手写面）。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][ssp][mask]"`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 3: ⑦ functional-only 解释器（新头 `ssp_interpreter.hh`）

**Files:**
- Create: `include/tlm/cep/ssp_interpreter.hh`
- Test: `test/test_cep_ssp_runtime.cc`

- [ ] **Step 1: Write the failing test**

```cpp
// ⑦ 解释器:状态迁移 + perm 投影 + 洞命中违例计数(⛔ 零时序)
TEST_CASE("[cep][ssp][interp] 解释器做状态迁移 + perm 投影且不产出时长",
          "[cep][ssp][interp]") {
    const auto res = cpptlm::cep::interpret_ssp(/* ssp 文档 */, /* 事件序列 */);
    REQUIRE(res.ok);
    // 每步:目标状态 + perm 投影
    REQUIRE(res.steps.size() == 2u);
    CHECK(res.steps[0].target_state == "S");
    CHECK(res.steps[0].perm == "read");
    // ⛔ 零时序:输出结构**不得**含任何时长/延迟/hop 字段
    CHECK(res.steps[0].duration_ns.has_value() == false);
    CHECK(res.steps[0].hops.has_value() == false);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][ssp][interp]"`
Expected: **编译失败** —— `interpret_ssp` / `SspInterpretation` 未声明（`ssp_interpreter.hh` 不存在）

- [ ] **Step 3: Write minimal implementation**

新建 `include/tlm/cep/ssp_interpreter.hh`（header-only · 通用层**零协议名**）：

```cpp
namespace tlm::cep {

/// 一步解释结果(⛔ **零时序** —— 本结构**不承载**任何时长 / 延迟 / hop 数)
struct SspStep {
    std::string source_state;
    std::string target_state;   ///< 空 = 洞命中(0xFF)
    std::string perm;           ///< 4 值锁投影
};

/// 一次解释的完整结果
struct SspInterpretation {
    bool ok = false;
    std::vector<SspStep> steps;
    std::size_t violation_count = 0;  ///< 洞命中次数(⛔ 不静默跳过 · ⛔ 不 panic)
    bool judgeable = true;            ///< 洞命中 ⇒ false ⇒ 该次实验判「不可判」
};

/// functional-only 解释:声明表 → 状态迁移 + perm 投影
/// ⛔ **零时序**(不含 hop / 消息级延迟 / 链路时延)—— 属 S2 ADR-CEP-25
inline SspInterpretation interpret_ssp(const nlohmann::json& ssp_doc,
                                       const std::string& ssp_id,
                                       const std::vector<std::string>& event_seq);

}  // namespace tlm::cep
```

实现要点：读 `transition` 单元 ⇒ string ⇒ 正常迁移；**null（空洞）** ⇒ `target_state` 置空 + `violation_count++` + `judgeable = false`；**掩码对象** ⇒ 取 `target_mask` 的目标集合（3 位）⇒ 命中多个目标时取**声明顺序首个**且 ⛔ 不做时序推断。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][ssp][interp]"`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 4: ⑩ 实验 4 件套 + 档位声明（复用既有产出路径）

**Files:**
- Modify: `scripts/cep/parity/`（**仅当**既有路径不认 SSP 协议 ⇒ 加「协议来源 = SSP」分支；⛔ 不复制整条管线）
- Create: `scripts/test/cep_ssp_runtime_check.sh`

- [ ] **Step 1: Write the failing test**

```bash
# scripts/test/cep_ssp_runtime_check.sh 的判据(节选)
# ① 4 件套齐备
for f in simulation_config.json trace.h5 metrics.json report.html; do
  [ -s "$RUN_DIR/$f" ] || fail "4 件套缺 $f"
done
# ② 证据声明档位(ADR-CEP-20 决策 6)
grep -q 'verification_mode'   "$RUN_DIR/metrics.json" || fail "缺档位声明"
grep -q 'verification_scope'  "$RUN_DIR/metrics.json" || fail "缺档位声明"
# ③ ⛔ 档 0 活性禁冒充满档
! grep -q 'semantic_verified.*true' "$RUN_DIR/report.html" || fail "档 0 禁冒充满档"
# ④ 四件描述同一次运行
grep -q '"run_id"' "$RUN_DIR/simulation_config.json" || fail "缺 run_id(跨运行拼接禁)"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `bash scripts/test/cep_ssp_runtime_check.sh`
Expected: **FAIL** —— 既有 4 件套路径只认手写协议 ⇒ SSP 协议无产出

- [ ] **Step 3: Write minimal implementation**

**先实测**：跑既有 4 件套路径，确认它「不认 SSP 协议」的具体缺口（❓ 缺口形态待 P1 实测确定）⇒ **最小**接入：若缺口是「协议来源选择」⇒ 加一个来源分支；若是「SSP 协议无 adapter」⇒ 复用 ⑧ 的三方绑定产物。⛔ **不新建**平行管线（D-5）。

- [ ] **Step 4: Run test to verify it passes**

Run: `bash scripts/test/cep_ssp_runtime_check.sh`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 5: ⑧ 装配闸新增「SSP 绑定」维（`[IAG-09]`）

**Files:**
- Modify: `include/tlm/cep/instance_assembly_gate.hh`（第 6 维判据）
- Test: `test/test_cep_instance_assembly.cc`

- [ ] **Step 1: Write the failing test**

```cpp
// ⑧ 装配闸:SSP 绑定维 —— 三方(id + adapter + SSP)解析到同一插件
TEST_CASE("[cep][assembly] SSP 绑定缺失或无效 ⇒ 判红且归因 [IAG-09]",
          "[cep][assembly][iag]") {
    // 反向 fixture ①:实例声明了 SSP 绑定但该 ssp.id 未登记 ⇒ 必红 + tag 命中
    const auto rep = run_instance_assembly_gate(/* 构造 config */, /* inputs */);
    CHECK(rep.verdict == InstanceAssemblyVerdict::FAIL);
    CHECK(rep.has_error("[IAG-09]"));
    // 正控:绑定齐备 ⇒ 判 PASS
    // (对照 config:声明层 + 拓扑层 + 平面层 + SSP 绑定四者齐备)
    CHECK(/* 齐备 config */.verdict == InstanceAssemblyVerdict::PASS);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][assembly][iag]"`
Expected: **FAIL** —— 装配闸**不判** SSP 绑定 ⇒ 未登记的 `ssp.id` 仍判 PASS

- [ ] **Step 3: Write minimal implementation**

在 `run_instance_assembly_gate` 的既有 ⑤ 层判据之后加第 6 维：

```cpp
// ⑥ SSP 绑定维(⑧ 装配集成):id + adapter + SSP 三方解析到同一插件
//    ⚠️ 归因 tag = [IAG-09](⛔ 不复用 [IAG-08] —— 已被既有用例占用)
//    ⛔ 只覆盖形态 + 引用闭环(不承诺语义)
const std::string ssp_id = inst.value("ssp", "");
if (!ssp_id.empty()) {
    if (!ssp_declared(ssp_id)) {
        rep.errors.emplace_back("[IAG-09] ssp not declared: " + ssp_id);
    } else if (!ssp_id_matches_instance(ssp_id, id, adapter)) {
        rep.errors.emplace_back("[IAG-09] ssp binding mismatch: " + ssp_id);
    }
}
```

（`ssp_declared` / `ssp_id_matches_instance` 的**确切形态以 P1 实测的现有接口为准** —— ⛔ 不新增第二份「SSP 是否已登记」的判定实现。）

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][assembly][iag]"`
Expected: **PASS** · 且**执行臂**（装配路径）同步生效 —— 两臂共用同一份判据实现（既有 Scenario 零改动）

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 6: 契约面落 main spec + **三处冻结面五步链**

**Files:**
- Modify: `openspec/specs/cep-ssp-declaration/spec.md`（**M1 冻结面**）
- Modify: `openspec/specs/cep-instance-assembly/spec.md`（**M1 冻结面**）
- Create: `openspec/specs/cep-ssp-runtime/spec.md`（**新 main spec** ⇒ 冻结集自动扩）
- Modify: `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`（`--update` 产出）

- [ ] **Step 1: Write the failing test**

```bash
# 三处契约面落地 + 冻结面复验
grep -c "target_mask" openspec/specs/cep-ssp-declaration/spec.md      # 期望 ≥ 1
grep -c "IAG-09"     openspec/specs/cep-instance-assembly/spec.md      # 期望 ≥ 1
[ -f openspec/specs/cep-ssp-runtime/spec.md ] || fail "缺新 main spec"
bash scripts/test/cep_freeze_check.sh                                  # 期望 rc=0
openspec validate --specs --strict                                     # 期望全绿
```

- [ ] **Step 2: Run test to verify it fails**

Run: 上述 4 条
Expected: `grep` 计数 **0** · `cep_freeze_check.sh` **rc≠0**（hash 漂移）

- [ ] **Step 3: Write minimal implementation**

把 Task 1~5 的契约面结论落进 3 份 main spec（内容与本 change 的 3 份 delta **逐字一致** ⇒ 归档幂等）⇒ 走**五步链**（D-7 · 物理串行）：

1. **台账 Entry #557**（步 1 · 已落）
2. builder commit：3 份 main spec + 代码 + 测试（**一个**产物 commit）
3. `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #557"`（预期：新增 **1**（新 main spec）· 移出 0 · hash 变更 = 命中数）
4. **清单单独 commit**（⛔ 不得与产物同 commit）
5. **无参复验** `bash scripts/test/cep_freeze_check.sh` ⇒ **rc=0**（硬门）

- [ ] **Step 4: Run test to verify it passes**

Run: 上述 4 条
Expected: `grep` 计数 ≥ 1 · `cep_freeze_check.sh` **rc=0** · `openspec validate --specs --strict` 全绿

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit；所有变更将在 archive 阶段统一提交。

---

## Test Plan（验收清单）

| # | 命令 | 期望 |
|:--:|---|---|
| 1 | `./build/bin/cpptlm_tests "[cep][ssp]"` | 全绿 |
| 2 | `./build/bin/cpptlm_tests "[cep][assembly]"` | 全绿 |
| 3 | `bash scripts/test/cep_ssp_runtime_check.sh` | rc=0 |
| 4 | `ctest --test-dir build --output-on-failure -j4` | **全绿**（⛔ 不按 `-R` 过滤）|
| 5 | 全部 `scripts/test/cep_*.sh` | 全 rc=0 |
| 6 | `bash scripts/test/cep_freeze_check.sh` | **rc=0**（三处冻结面）|
| 7 | `openspec validate --specs --strict` / `--changes --strict` | 全绿 |
| 8 | `bash scripts/test/docs_sync_check.sh --strict` | rc=0 |
| 9 | `./scripts/build/format.sh --check` | rc=0（⛔ 存量 5 件除外，见交付说明）|

## Risk Mitigation（风险登记表 —— 本 change 子集）

见 `design.md` §4（R-1 ~ R-7）。核心三条：**R-1** 洞命中被静默吞掉（D-2 违例计数为一等输出 + 反向 fixture）· **R-2** 掩码格与手写面漂移（D-3 V-24-2 扩为两形态逐位对账 + 双向）· **R-5** ⑨ 的 contract 面无 M1 保护（D-8 用合同测试兜底 + architect 待裁）。

## Done Definition（archive 前）

- [ ] Task 1~6 全部完成，`tasks.md` 全部 `- [x]`
- [ ] Test Plan 9 条全绿（rc=0）
- [ ] 三处冻结面的**五步链 5 步**全部完成（Entry #557 · 清单单独 commit · 无参复验 rc=0）
- [ ] ⛔ **表述纪律**：交付说明中**不得**含「CEP 已支持用户自定义协议」；实验结论**不得**表述为「协议语义已真实链路验证」（D-1 / `ADR-CEP-20` 决策 6）
- [ ] ⛔ **不得**宣称 ⑨ 的 contract 面已受 M1 保护（D-8 · architect 待裁）
