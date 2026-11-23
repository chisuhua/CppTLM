# cep-protocol-engine-capability-declaration: 实施计划（TDD 5 步）

> **Change name**: `cep-protocol-engine-capability-declaration`（slug，**不带日期前缀**）
> **配套**: `openspec/changes/cep-protocol-engine-capability-declaration/`（proposal / design / specs）
> **来源**: `.rddf/improvements/cep-protocol-engine-capability-declaration.md`（metis 1 轮已处置）
> **生成时间**: 2026-11-23 · **生成方式**: builder 手写（TDD 5 步）
> **执行模式**: 由 P1.5 判定（本 plan 的 task 数 = 8，`Files:` 覆盖 ~10 文件 ⇒ 预期 worktree；以 P1.5 实测为准）

---

## Goal（目标）

把「协议引擎承载能力（引擎存在性 / egress / 在飞事务建模 / 瞬态态现状）」从**引擎头里的散文**变成**编译期 + 闸双重保证的声明面**。

**完成定义（DoD）**：
1. `include/tlm/cep/engine_capabilities.hh` 落地：`EngineCapability` + `kEngineCapabilities[8]` + `kEgressHasMachineCheck=false` + `EngineTypeTraits<T>` 主模板（dependent-false）+ `HasInFlightApi<T>` + 自洽谓词
2. 5 个引擎头各追加 `EngineTypeTraits<T>` 特化（仅 `in_flight` + `basis`）
3. `test/test_cep_engine_capabilities.cc`：J-1 / J-2 / J-2b / J-3 全部有断言；变异副本负控
4. `scripts/test/cep_engine_capabilities_check.sh`：双向零 orphan + 引擎头文件集反向存在性 + 反向 fixture
5. probe TU 反向验证（AC-3 / AC-4）：编译失败 **且** 命中字面量签名
6. `cmake --build build` PASS · `ctest` 全绿 · 全部 `scripts/test/cep_*.sh` rc=0 · `cep_docs_sync_check.sh --strict` rc=0
7. `openspec validate cep-protocol-engine-capability-declaration --strict` rc=0

**范围纪律（硬约束）**：⛔ 不改 `protocol_instance.schema.json` / `protocol_instances.json` / 任何引擎行为 / 既有 ADR / 既有 CEP spec / 任何冻结面。

---

## 关键约束（设计要点摘要）

见 `design.md` 的 D-1 ~ D-9。实施时必须守住：

| # | 约束 | 一句话 |
|:--:|---|---|
| D-1 | 通用层零协议名 / 零 include 协议头 | 铁律 2 |
| D-2 | SFINAE 反射，不 grep | 备注会假绿 |
| D-3 | 零 JSON | C++ constexpr 表 = 唯一真源 |
| D-4 | 无 per-row `pending` | 改文件级 `kEgressHasMachineCheck` |
| D-5 | trait 不带 `instance_id` | 实例命名权不烧进类型 |
| D-6 | 主模板 dependent-false | 保留「未特化即错」+ 可读归因 |
| D-7 | J-3 只对 5 个持字段 adapter | ualink/nvlink 无该字段 |
| D-8 | 反向存在性 = 文件清单对账 | 诚实标注已知局限 |
| D-9 | 编译失败型判据用 probe TU | ⛔ 不接受裸 rc≠0 |

---

## Work Units（TDD 5 步）

### Task 1: 通用层骨架 + 自洽谓词（RED → GREEN）

**Files:** `include/tlm/cep/engine_capabilities.hh`, `test/test_cep_engine_capabilities.cc`

- **RED**：在测试里写 `TEST_CASE` 断言 `kEngineCapabilities` 存在且 8 行、`engine_capabilities_are_consistent` 可调用 ⇒ 编译失败（符号不存在）
- **GREEN**：新建头，落 `EgressMode` / `InFlightModeling` / `EngineCapability` / `kEngineCapabilities`（先全填占位值）/ `engine_capabilities_are_consistent`
- **REFACTOR**：谓词抽为纯函数模板（`(list, n)` 形参），不读全局

### Task 2: 8 行声明表填实值 + `basis` 非空

**Files:** `include/tlm/cep/engine_capabilities.hh`, `test/test_cep_engine_capabilities.cc`

- **RED**：测试断言「每行 `basis` 非空」+「`id` 唯一」+「`kEgressHasMachineCheck == false`」⇒ 占位行触发失败
- **GREEN**：按 design.md 的 egress 取值表填 8 行（`chi_rnf`/`mesif` = `ProtocolMessages`；其余 `None`），每行写 `basis` 一句话依据
- **REFACTOR**：`basis` 文本精简到一行可读

### Task 3: SFINAE 检测 + 主模板 dependent-false

**Files:** `include/tlm/cep/engine_capabilities.hh`, `test/test_cep_engine_capabilities.cc`

- **RED**：测试里 `static_assert(!HasInFlightApi<SomeTypeWithoutIt>::value)` + 一个反向（用 `ChiRnfProtocolEngine` 断言 true）⇒ 未实现则编译失败
- **GREEN**：落 `HasInFlightApi<T>`（`std::void_t` + `decltype(declval<const T&>().txn_in_flight(uint64_t{}))`）+ `EngineTypeTraits<T>` 主模板（`static_assert(!sizeof(T*), "...")`）
- **REFACTOR**：探测抽取为别名模板，便于阅读

### Task 4: 5 个引擎头 `EngineTypeTraits<T>` 特化 + J-1 编译期交叉核对

**Files:** `include/tlm/cep/engine_capabilities.hh`, `include/tlm/cep/chi_rnf_protocol_engine.hh`, `include/tlm/cep/mesif_protocol_engine.hh`, `include/tlm/cep/moesi_gpu_protocol_engine.hh`, `include/tlm/cep/ualink_protocol_engine.hh`, `include/tlm/cep/tilelink_protocol_engine.hh`, `test/test_cep_engine_capabilities.cc`

- **RED**：测试为 8 个实例建立「id → 类型」显式映射（无引擎实例映射为「无类型」），对 5 个有引擎的写 `static_assert(EngineTypeTraits<T>::in_flight == Modeled ⇔ HasInFlightApi<T>::value)` ⇒ 未特化则编译失败
- **GREEN**：5 个引擎头各追加特化（`chi_rnf` = `Modeled`，其余 4 个 = `NotModeled`），各带 `basis`
- **REFACTOR**：特化块统一放在各头文件尾部「能力声明」小节，加统一注释头

### Task 5: J-3 瞬态态策略（编译期）

**Files:** `include/tlm/cep/engine_capabilities.hh`, `test/test_cep_engine_capabilities.cc`

- **RED**：测试对 5 个持字段 adapter 写 `static_assert(Adapter::kTransientStateCount <= kMaxTransientStatesPerInstance)` ⇒ `kMaxTransientStatesPerInstance` 未定义则编译失败
- **GREEN**：在通用层头落 `inline constexpr std::size_t kMaxTransientStatesPerInstance = 1;`（注释明写「**现状锁**，提升归 architect」）；测试 include 5 个 adapter 并逐 `static_assert`
- **REFACTOR**：把 5 个 adapter 的 include 与断言集中到一个小节

### Task 6: 闸 `cep_engine_capabilities_check.sh`

**Files:** `scripts/test/cep_engine_capabilities_check.sh`

- **RED**：写闸骨架（解析 0 行 ⇒ `SKIP` + exit 0），跑一次 ⇒ 输出 `SKIP`（证明「解析不到 ≠ 通过」路径可达）
- **GREEN**：① 从 `kEngineCapabilities` 初始化区段 grep 数据行抽 `id` ② 与 `protocol_instances.json` 的 `id` 集双向对账 ③ 引擎头文件集对账（恰 5 个）④ `basis` 非空 ⑤ 三态 `SKIP`/`PASS`/`FAIL`
- **REFACTOR**：把「已登记引擎头清单」提为脚本顶部常量并加注释说明已知局限

### Task 7: probe TU 反向验证（AC-3 / AC-4）

**Files:** `scripts/test/cep_engine_capabilities_check.sh`

- **RED**：写 probe 模式骨架（复制头到临时目录 → 变异 → 编译 → 断言失败），先对**当前正确状态**跑 ⇒ 必须 `PASS`（不红）
- **GREEN**：落两条变异：① `chi_rnf` 特化改 `NotModeled` ⇒ 编译失败且输出含 `chi_rnf` ② `mesif` `kTransientStates` 加一项 ⇒ 编译失败且输出含 `mesif`；每条都断言「失败 + 签名命中」
- **REFACTOR**：抽公共函数 `run_probe_variant(mutation_sed, expect_signature)`

### Task 8: CMake 挂测试 + 全量回归

**Files:** `test/CMakeLists.txt`

- **RED**：不挂测试时 `ctest -R cep_engine_capabilities` 找不到用例
- **GREEN**：把 `test_cep_engine_capabilities.cc` 加入显式源列表（⛔ GLOB 例外仅 test/ 允许，但本仓 test/CMakeLists 用 GLOB）
- **REFACTOR**：无

---

## Test Plan（验收清单）

对应 proposal 的 AC-1 ~ AC-13。实施时逐条勾选，验收时 verifier 独立复核。

| AC | 载体 |
|---|---|
| AC-1 通用层零协议名 | 闸的静态字符串检查 + 反向 fixture |
| AC-2 双向零 orphan | C++ 运行期 + 闸 |
| AC-3 在飞建模一致 | 测试 `static_assert` + probe TU |
| AC-4 瞬态态策略 | 测试 `static_assert` + probe TU |
| AC-5 `basis` 非空 + egress 诚实登记 | 测试 + 闸 |
| AC-6 谓词变异负控 | 测试（传变异副本） |
| AC-7 SKIP/FAIL 可区分 | 闸骨架路径 |
| AC-8 引擎头文件集反向存在性 | 闸 + 反向 fixture |
| AC-9 单真源（字段不复制） | 闸 + 反向 fixture |
| AC-10 零回归 | `ctest` + 全部 `cep_*.sh` + docs_sync + doc_ref |
| AC-11 既有冻结面零变更 | `git diff --name-only` 对账 |
| AC-12 不新增 invariant 编号 | `cep_invariant_map_check.py` |
| AC-13 表述纪律 | 人工评审（verifier 报告） |

---

## 残余风险义务（⛔ 不可写成「已消除」）

| # | 义务 |
|:--:|---|
| R-1 | `egress` **无机器判据** ⇒ 仅 `kEgressHasMachineCheck=false` 登记。⛔ 不得表述为「egress 已核实」 |
| R-2 | 3 个无引擎实例是**编译期飞地** ⇒ 只有闸的文件集判据部分覆盖；⛔ 不得表述为「已逐实例绑定」 |
| R-3 | 闸靠 grep **数据行** 提取实例 id ⇒ 格式化表结构时可能失效；缓解 = C++ 侧另有运行期核对 |
| R-4 | J-3 阈值 1 是**现状锁**；提升须 architect 裁决 |
| R-5 | 反射探测名 `txn_in_flight` 若改名 ⇒ 判据必红（这是特性不是缺陷），须同步更新 |

---

## 判据纪律（本仓已固化）

1. 每条判据 = 「基线正控绿 ⇒ 破坏 ⇒ **按正确原因红（红因有签名）** ⇒ 还原 ⇒ **必须绿**」
2. ⛔ 「rc≠0」不构成红因
3. ⛔ 「声明 ≠ 判定」
4. 编译失败型判据必须用 probe TU + 签名命中

---

## 冻结面纪律

本 change **不触碰**任何冻结面（`docs/adr/ADR-CEP-*.md` / `docs/architecture/cep/*.md` / 既有 `openspec/specs/cep-*`）⇒ **无需五步授权链**。
引擎头**追加**特化不属冻结面变更（引擎头不在 M1 冻结清单）。

---

## Done Definition（归档前）

1. 全部 `### Task` 的 checkbox 勾选
2. `cmake --build build -j2` PASS（零 warning 或仅既有 warning）
3. `ctest --output-on-failure` 全绿
4. 全部 `scripts/test/cep_*.sh` rc=0
5. `bash scripts/test/cep_docs_sync_check.sh --strict` rc=0
6. `bash scripts/test/cep_doc_ref_check.sh` rc=0
7. `openspec validate cep-protocol-engine-capability-declaration --strict` rc=0
8. verifier 报告落 `.rddf/reviews/cep-protocol-engine-capability-declaration-verify.md`（含 AC-13 人工评审段）
