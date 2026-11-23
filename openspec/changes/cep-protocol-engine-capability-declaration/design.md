# design: cep-protocol-engine-capability-declaration

> **定位**：本 change 的**实施指导**（how）。**判据本体**在 proposal 的「验收标准」段；**决策依据**在 `.rddf/improvements/cep-protocol-engine-capability-declaration.md` 与 `.rddf/roadmap/cep-gen-family-gap-closure.md`。
> **⛔ 本文件不重述动机**，只回答「怎么做、为什么这么做」。

---

## D-1 分层：通用层放判据，协议层放类型

**约束**：`include/tlm/cep/AGENTS.md` 铁律 2 —— 通用层（`include/tlm/cep/*.hh` 根目录）**不得**出现具体协议名；协议差异只允许出现在 `*_adapter.hh` / `*_state_spec.hh` / `*_protocol_engine.hh`。

**决策**：

| 内容 | 落点 | 理由 |
|---|---|---|
| 枚举（`EgressMode` / `InFlightModeling`）· `EngineCapability` 结构 · `kEngineCapabilities[8]` · `kEgressHasMachineCheck` · `EngineTypeTraits<T>` 主模板 · `HasInFlightApi<T>` · 自洽谓词 | **通用层** `include/tlm/cep/engine_capabilities.hh` | 均为协议中立；实例 id 出现在通用层有既有先例（`channel_dependency_graph.hh` 的 `DeclaredInstance`） |
| `EngineTypeTraits<T>` 的**特化** | **5 个协议头**（各引擎头内） | 特化必须引用具体引擎类型 ⇒ 协议相关 |

**为什么不用第 4 类新文件**：铁律 2 白名单是闭合的（只有那三类文件可承载协议差异）⇒ 新建第 4 类会破白名单。

---

## D-2 反射，不 grep

**决策**：`HasInFlightApi<T>` 用 SFINAE 探测 `T::txn_in_flight`（`std::void_t` + `decltype`），**⛔ 不 grep 源码**。

**理由**：
- `grep "txn_in_flight"` 会命中**注释**（如本 change 的 proposal 与 improvement 自身）与**历史行** ⇒ 假绿；
- SFINAE 探测的是**类型真实暴露的公开 API** ⇒ 引擎删掉该方法 ⇒ 编译直接失败（这正是我们要的「补了必红 / 删了也必红」）。

**探测点选择依据**：`chi_rnf` 的 `txn_in_flight(std::uint64_t) const` / `in_flight_count() const` / `pending_egress() const` 是**唯一**在 8 个引擎中公开暴露在飞事务 API 的实现（`include/tlm/cep/chi_rnf_protocol_engine.hh`）。其余 4 个引擎均无同名 API（实测）。

**实现骨架**：

```cpp
template <class T, class = void>
struct HasInFlightApi : std::false_type {};

template <class T>
struct HasInFlightApi<T, std::void_t<decltype(std::declval<const T&>().txn_in_flight(
                          std::uint64_t{}))>> : std::true_type {};
```

---

## D-3 零 JSON（**相对初次设计的修订**）

**初次设计**：新建 `engine_modes.json` + 同名 schema + loader。
**修订后**：**零 JSON**，`kEngineCapabilities` 为 C++ `inline constexpr` 表。

**理由**：
1. 仓内先例证明 C++ constexpr 表可作**唯一真源**（`MesifProtocolEngine::kEngineDecisions[]` / `kTrackerTraits`）；
2. JSON 镜像会形成**第二真源** ⇒ 需额外闸做逐字段比对；
3. JSON 方案额外引入 schema + loader + 闸 + 镜像**四件套**，而本单元只有 8 行 × 3 字段 ⇒ 收益不抵成本；
4. **零 JSON ⇒ 零冻结面触碰** ⇒ 无五步链成本。

**代价（诚实登记）**：
- 闸无法用 JSON schema 校验 ⇒ 改为「抽取初始化区段 + 谓词」；
- 闸只能 grep **数据行**（⛔ 不 grep API / 类名）⇒ 该 grep 是**数据提取**而非语义判定，可接受。

---

## D-4 无 per-row `pending` 字段（**相对初次设计的修订**）

**初次设计**：每行带 `pending: bool`（仿 `EngineDecision`）。
**修订后**：**删除 per-row `pending`**，改为文件级常量 `kEgressHasMachineCheck = false`。

**理由**：本 change 中「egress 无机器判据」是**全体行的共同属性** ⇒ per-row `pending` 会是**恒真字段**（零判别力）。把诚实性升到**文件级单条常量**，既保留「不可伪装成有牙」的登记，又不引入恒真字段。

---

## D-5 `EngineTypeTraits` 不带 `instance_id`

**理由**（metis 1 轮指出）：类型与实例 1:1 时携带 id 无害，但这是把**实例命名权烧进类型系统**——若未来一个引擎类型服务两个实例，trait 里的 id 就变成谎言。

**现状不变式**：**1 引擎类型 : 1 实例**（本 change 记录该不变式；若未来打破 ⇒ 须改设计并走评审）。

**⇒ 实例 id 只存在于通用层表**；类型级 trait 只带 `in_flight` + `basis`。测试 TU 负责建立「实例 id → 引擎类型」的显式映射并做交叉核对（测试 TU 可以 include 协议头 ⇒ 允许有协议名）。

---

## D-6 `EngineTypeTraits<T>` 主模板用 dependent-false

**问题**：3 个无引擎实例**永远没有特化**；若主模板「故意不定义」，未来有人写「对全部 8 实例统一应用 J-1」的泛型代码时会撞到不可读的 `incomplete type` 报错。

**决策**：

```cpp
template <class T>
struct EngineTypeTraits {
    static_assert(!sizeof(T*), "EngineTypeTraits: <T> 无能力特化；"
                              "若无引擎实例请走 kEngineCapabilities 行级声明");
};
```

`!sizeof(T*)` 是 dependent-false 惯用法 ⇒ 保留「未特化即错」，同时给出**可读归因**。

---

## D-7 J-3 的覆盖范围：只对持字段的 adapter

**实测**：`kTransientStateCount` 只存在于 `chi_rnf_adapter.hh` / `cxl_cache_adapter.hh` / `cxl_state_spec.hh` / `mesif_adapter.hh` / `mesif_state_spec.hh` / `moesi_gpu_adapter.hh` / `tilelink_c_adapter.hh`；**`ualink_adapter.hh` / `nvlink_adapter.hh` 无该字段**。

⇒ J-3 的 `static_assert` 只对 **5 个持有者**（`chi_rnf` / `mesif` / `moesi_gpu` / `tilelink_c` / `cxl_cache`）落；对 `ualink_4` / `nvlink_4` 引用该字段**即编译错** ⇒ 必须显式排除。

**阈值语义**：`kMaxTransientStatesPerInstance = 1` 是「**现状锁**」而非「设计容量」；提升 = architect 裁决事项（衔接 `.rddf/roadmap/cep-gen-family-gap-closure.md` §3 Stage C）。

---

## D-8 反向存在性机制（AC-8）

**机制**（不依赖 grep 类名语义，只依赖**文件清单**）：

1. 闸内维护「已登记引擎头集合」= 5 个路径（`chi_rnf` / `mesif` / `moesi_gpu` / `ualink` / `tilelink`）；
2. 实测 `include/tlm/cep/*_protocol_engine.hh` 的文件集 **==** 该集合 ⇒ PASS；否则 FAIL（红因码 `engine-cap-new-engine-detected` 或 `engine-cap-engine-header-missing`）；
3. 反向 fixture：`touch include/tlm/cep/fake_protocol_engine.hh` ⇒ 必红。

**已知局限（必须随报告一起读）**：该机制**无法**区分共用命名前缀的两个实例（`cxl_2_0` 与 `cxl_3_0_symmetric` 都映射到 `cxl_*`）⇒ 它只守「**新引擎出现**」这一个真实风险，⛔ **不假装能逐实例绑定**。
**为什么不做得更强**：把「实例 id → 引擎类名」的映射放进闸 = 在闸里重建一份声明面 = 第二真源；而放进通用层 = 违反铁律 2。⇒ **诚实的半自动判据**优于虚假的全自动判据。

---

## D-9 编译失败型判据的验证范式（AC-3 / AC-4）

沿用 `scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh` 的 **probe TU 模式**：

1. 把目标头复制到影子目录（`scripts/test/cep_test_shadow.sh` 的既有机制，若有；否则用 `mktemp -d`）；
2. 在副本上施加变异（改特化 / 加瞬态项）；
3. 编译一个只 include 副本的 probe TU；
4. 断言：**编译失败** **且** stderr 命中 `static_assert` 的**字面量签名**（含实例 id）；
5. ⛔ 不接受「只看 `rc≠0`」（判据纪律明禁）。

---

## 判据 → 载体映射

| 判据 | 载体 | 形态 |
|---|---|---|
| J-1 在飞建模一致 | `test/test_cep_engine_capabilities.cc` 的逐实例 `static_assert` | 编译期 |
| J-2 双向零 orphan | 同上（运行期） + `scripts/test/cep_engine_capabilities_check.sh` | 运行期 + 闸 |
| J-2b 声明自洽 | `engine_capabilities_are_consistent()` + 测试的变异副本负控 | 运行期 |
| J-3 瞬态态策略 | 测试的逐 adapter `static_assert` | 编译期 |
| AC-8 反向存在性 | 闸的文件清单对账 | 闸 |
| AC-5 `kEgressHasMachineCheck` | 测试的 `static_assert` + 闸 | 编译期 + 闸 |

---

## 风险与权衡

| 风险 | 权衡决定 |
|---|---|
| 反射探测名（`txn_in_flight`）与未来实现不匹配 | 接受：探测名写进判据并登记 ⇒ 改名必然触发判据复核（这正是「可检出」的目的） |
| 3 个无引擎实例是**编译期飞地** | 接受 + 用 AC-8 的**文件集反向存在性**部分补偿；⛔ 不伪造编译期绑定 |
| `egress` 无机器判据 | 接受 + `kEgressHasMachineCheck=false` 诚实登记 + AC-5② 强制「引入机检必须同时翻标志」 |
| 闸靠 grep 数据行提取实例 id | 接受：grep **数据**（非 API/类名）；且 J-2 在 C++ 侧另有运行期核对 ⇒ 双重 |
| 阈值 1 被误读为设计容量 | 接受 + 文档明写「现状锁，提升归 architect」 |

---

## 不做什么（避免范围蔓延）

- ⛔ 不引入 JSON / schema / loader
- ⛔ 不改任何协议引擎的**行为**
- ⛔ 不改 `protocol_instances.json` / `protocol_instance.schema.json`
- ⛔ 不新增 invariant 编号
- ⛔ 不动冻结面（`docs/adr/ADR-CEP-*.md` · `docs/architecture/cep/*.md` · 既有 `openspec/specs/cep-*`）
