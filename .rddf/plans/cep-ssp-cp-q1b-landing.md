# cep-ssp-cp-q1b-landing: 实施计划（TDD 5 步）

> **For agentic workers:** Implement this plan task-by-task, following the TDD 5-step structure below. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让用户自定义协议**过得了装配期** —— `family` 扩第 4 值 `user-defined` + `protocol_version` 增第二种合法形态 `user:<已登记 ssp-id>`（两分支化）。

**Architecture:** 纯 **contract 面**扩值域（零新增能力面）。三处改动同批落地：① **schema**（`family` enum +1 值 · `protocol_version` pattern 改两分支）② **共享 helper**（两分支谓词落**单一**实现，落 `test/cep_adr_parser.hh` 与既有锁表 helper 同处）③ **IC1 两分支化**（`test/test_cep_schema.cc` 通用闸 + `openspec/specs/cep-core-contracts/spec.md` 的 IC1 Scenario）。⛔ 零解释器 / 零装配集成 / 零掩码格 / 零实验 4 件套。

**Tech Stack:** C++20 · Catch2 v3.7.0 · nlohmann::json · `std::regex` · JSON Schema（`protocol_instance.schema.json`）· M1 冻结面五步链（`scripts/test/cep_freeze_check.sh`）

---

> **Change name**: `cep-ssp-cp-q1b-landing`（slug，**不带日期前缀**）
> **配套**: `openspec/changes/cep-ssp-cp-q1b-landing/`（proposal / design / specs / tasks）
> **来源**: `.rddf/improvements/cep-ssp-cp-q1b-landing.md`（✅ 已批准·待立项）
> **生成时间**: 2026-11-26 · **生成方式**: builder 手写（TDD 5 步）
> **执行模式**: 由 P1.5 判定（本 plan task 数 = 6，`Files:` 覆盖 5 文件 ⇒ 预期 lightweight；**以 P1.5 实测为准**）

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/tlm/cep/protocol_instance.schema.json` | **M1 冻结面** —— `family` enum 追加 `user-defined`；`protocol_version` 的 `pattern` 改两分支（容纳 `user:[a-z][a-z0-9_]*`）。⛔ 字段数 / `required` 13 项**零改动** |
| `include/tlm/cep/protocol_family.hh` | `ProtocolFamily` 加 `UserDefined = 4` · `kProtocolFamilyCount` 3→4 · `kProtocolFamilies[]` 加条目 · `protocol_family_name` 的 `switch` 加 case · 新增 `static_assert(protocol_family_id(UserDefined) == 4)` · 既有 `static_assert(kProtocolFamilyCount == 3)` → 4 · 注释「恰好 3 值」→ 4 值 |

### Tests

| File | Responsibility |
|---|---|
| `test/cep_adr_parser.hh` | **共享 helper 落点** —— 与既有 `parse_protocol_version_lock_table` / `protocol_version_in_lock_table` 同处；新增两分支谓词 + `ssps.json` 的 `ssp.id` 集合提取入口（⛔ **零新文件**）|
| `test/test_cep_schema.cc` | IC1 通用闸两分支化（`:573`）+ `protocol_version` pattern 容纳性用例纳入 `user:` 形态（`:512`）+ 4 条反向 fixture（各自按**正确原因**红）|
| `openspec/specs/cep-core-contracts/spec.md` | **M1 冻结面** —— `cep-protocol-version-locking-contract` 的 IC1 Scenario 两分支化（IC2 / IC3 **逐字零改动**）|
| `openspec/changes/cep-ssp-cp-q1b-landing/specs/cep-core-contracts/spec.md` | MODIFIED delta（已落盘，3 Scenario 全保留）—— 归档时并入 main spec |

---

## 关键约束（设计要点摘要）

见 `design.md` 的 D-1 ~ D-8。实施时必须守住：

| # | 约束 | 一句话 |
|:--:|---|---|
| D-1 | 两分支谓词落**单一** helper | ⛔ 禁第二份拷贝（Entry #109 G-2）|
| D-2 | helper 落 `test/cep_adr_parser.hh` | 与既有锁表 helper 同处 · 零新文件 |
| D-2b | ⛔ `instance_assembly_gate.hh` **零 diff** | IAG **不查** `protocol_version`（ADR SU ③ 第 3 面）|
| D-3 | schema pattern 改**两分支** | ⛔ 不新增字段 · ⛔ 13 必含个数不变 |
| D-4 | 「已登记」= `ssps.json` 的 `ssp.id` 集合 | 唯一真源 · ⛔ 不写锁表 |
| D-5 | IC1 两分支化落 **MODIFIED delta** | 3 Scenario 全在 · IC2/IC3 逐字零改动 |
| D-6 | 既有 8 实例**零行为变更** | ⛔ 不碰 `ADR-CEP-11` §3 锁表 |
| D-7 | 两处冻结面 ⇒ **两次**五步链 | 5 步物理串行 · 第 5 步 rc=0 是硬门 |
| D-8 | 表述纪律 | 本 change 完成后**仍不得**说「CEP 已支持用户自定义协议」|

---

## Work Units（TDD 5 步）

### Task 1: `protocol_family.hh` 加 `UserDefined = 4`（6 处 + 1 新增断言）

**Files:**
- Modify: `include/tlm/cep/protocol_family.hh:43-53`（`enum class ProtocolFamily` + `kProtocolFamilyCount`）
- Modify: `include/tlm/cep/protocol_family.hh:56-60`（`kProtocolFamilies[]`）
- Modify: `include/tlm/cep/protocol_family.hh:79-89`（`protocol_family_name` 的 `switch`）
- Modify: `include/tlm/cep/protocol_family.hh:106-113`（编译期锁）

- [ ] **Step 1: Write the failing test**

在 `test/test_cep_schema.cc` 追加（`[cep][schema][ic1]` 标签）：

```cpp
// INV-CEP-12-IC1:family 域 4 值(per ADR-CEP-12 SU(2026-11-26)· ADR-CEP-24 V-24-13)
TEST_CASE("[cep][schema] family 域必须含 4 值且 user-defined 可解析",
          "[cep][schema][ic1]") {
    using namespace cpptlm::tlm::cep;
    REQUIRE(kProtocolFamilyCount == 4u);
    // 既有 3 值逐字不变(零回归)
    ProtocolFamily f{};
    CHECK(protocol_family_from_name("industrial", f));
    CHECK(f == ProtocolFamily::Industrial);
    CHECK(protocol_family_from_name("ruby-slicc", f));
    CHECK(f == ProtocolFamily::RubySlicc);
    CHECK(protocol_family_from_name("academic", f));
    CHECK(f == ProtocolFamily::Academic);
    // 新增第 4 值
    CHECK(protocol_family_from_name("user-defined", f));
    CHECK(f == ProtocolFamily::UserDefined);
    CHECK(protocol_family_id(ProtocolFamily::UserDefined) == 4u);
    CHECK(std::string(protocol_family_name(ProtocolFamily::UserDefined)) == "user-defined");
    // 未知字符串仍返回 false(不静默兜底)
    CHECK_FALSE(protocol_family_from_name("no-such-family", f));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **编译失败** —— `ProtocolFamily::UserDefined` / `kProtocolFamilyCount == 4u` 不存在（符号未声明）

- [ ] **Step 3: Write minimal implementation**

改 `include/tlm/cep/protocol_family.hh`（6 处 + 1 新增断言 + 1 注释）：

```cpp
enum class ProtocolFamily : std::uint8_t {
    Industrial = 1,   ///< 工业协议族(per `07 号 §5.1` 口径)
    RubySlicc = 2,    ///< Ruby SLICC 族(per `07 号 §5.2` 口径)
    Academic = 3,     ///< 学术协议族(per `07 号 §5.3` 口径)
    UserDefined = 4,  ///< 用户自定义协议族(per ADR-CEP-12 SU(2026-11-26)· ADR-CEP-24 V-24-13)
};

/// 来源族总数(per `ADR-CEP-12` 的 `family` 域 —— 恰好 4 值)
inline constexpr std::size_t kProtocolFamilyCount = 4;

/// 全部来源族(顺序 = 编号顺序)—— **唯一真源**
inline constexpr ProtocolFamily kProtocolFamilies[kProtocolFamilyCount] = {
    ProtocolFamily::Industrial,
    ProtocolFamily::RubySlicc,
    ProtocolFamily::Academic,
    ProtocolFamily::UserDefined,
};
```

`protocol_family_name` 的 `switch` 加 case：

```cpp
        case ProtocolFamily::Academic:
            return "academic";
        case ProtocolFamily::UserDefined:
            return "user-defined";
    }
```

编译期锁（新增 1 条 + 改 1 条）：

```cpp
static_assert(protocol_family_id(ProtocolFamily::UserDefined) == 4,
              "ProtocolFamily 编号漂移:UserDefined");
static_assert(kProtocolFamilyCount == 4, "来源族应为 4 值(per ADR-CEP-12 的 family 域)");
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit；所有变更将在 archive 阶段统一提交。

---

### Task 2: schema `family` enum 加 `user-defined` + `protocol_version` pattern 改两分支

**Files:**
- Modify: `include/tlm/cep/protocol_instance.schema.json:30-37`（`family` enum）
- Modify: `include/tlm/cep/protocol_instance.schema.json:51-55`（`protocol_version` pattern）

- [ ] **Step 1: Write the failing test**

在 `test/test_cep_schema.cc` 的既有 `[cep][schema] protocol_version 值域必须容纳 ADR-CEP-11 锁定表全部取值` 用例（`:512`）**追加** `user:` 形态断言：

```cpp
    // ── user: 形态必须被 pattern 容纳(per ADR-CEP-11 SU(2026-11-26)③ 第 2 面)──
    // ⚠️ ssp.id 词表 = ^[a-z][a-z0-9_]*$(per ssp_loader.hh E2)⇒ 只含小写字母/数字/下划线
    CHECK(std::regex_match("user:mesif_ssp", re));
    CHECK(std::regex_match("user:a1", re));
    // ⛔ 反向:大写 / 数字开头 / 含 `-` 或 `.` / 空 id ⇒ pattern 必须拒
    CHECK_FALSE(std::regex_match("user:Mesif", re));
    CHECK_FALSE(std::regex_match("user:1abc", re));
    CHECK_FALSE(std::regex_match("user:a-b", re));
    CHECK_FALSE(std::regex_match("user:", re));
```

并在同一文件新增一个 `family` enum 的 schema 面断言（与 Task 1 的 C++ 面**互为独立**——schema 侧单独验）：

```cpp
TEST_CASE("[cep][schema] family enum 必须含 4 值且必含属性个数仍为 13",
          "[cep][schema][ic1]") {
    const json s = load_schema();
    const auto fam = s.at("properties").at("family").at("enum");
    REQUIRE(fam.size() == 4u);
    const std::set<std::string> vals(fam.begin(), fam.end());
    CHECK(vals.count("industrial") == 1u);
    CHECK(vals.count("ruby-slicc") == 1u);
    CHECK(vals.count("academic") == 1u);
    CHECK(vals.count("user-defined") == 1u);
    // ⛔ INV-CEP-12-IC1:13 必含属性个数零改动(只扩 enum 取值域)
    CHECK(s.at("required").size() == 13u);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **FAIL** —— `family` enum 只有 3 值（`fam.size() == 4u` 断言红）；`std::regex_match("user:mesif_ssp", re)` 红（现 pattern `^[a-z0-9]+(-[a-z0-9.]+)*$` **拒绝 `:`**）

- [ ] **Step 3: Write minimal implementation**

改 `include/tlm/cep/protocol_instance.schema.json` 两处：

```json
    "family": {
      "description": "来源族(per 07 号 §5.1 口径:族 = 来源集合;per ADR-CEP-12 SU(2026-11-26) 扩第 4 值 user-defined)",
      "enum": [
        "industrial",
        "ruby-slicc",
        "academic",
        "user-defined"
      ]
    },
```

```json
    "protocol_version": {
      "description": "规范版本(per ADR-CEP-11 §3 锁定表;adapter 实现版本由 git 承载,不入本 schema;per ADR-CEP-11 SU(2026-11-26) 增 user:<ssp-id> 形态)",
      "type": "string",
      "pattern": "^([a-z0-9]+(-[a-z0-9.]+)*|user:[a-z][a-z0-9_]*)$"
    },
```

⚠️ **pattern 的两分支可证不相交**（ADR SU ②）：分支 ① 内全部官方版本均含 `-` 或 `.`；分支 ② 的 `ssp.id` 词表 `^[a-z][a-z0-9_]*$` **排除** `-` 与 `.` ⇒ `user:<ssp-id>` 不可能字面等于任一官方版本 ⇒ 分支 ② **无法冒充分支 ①**；`chi-r3` 不匹配 `^user:` ⇒ **仍**落分支 ① 且 fail ⇒ 判据未变弱。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 3: 共享 helper —— 两分支谓词单点实现（⛔ 零新文件）

**Files:**
- Modify: `test/cep_adr_parser.hh`（`cpptlm::cep` 命名空间，紧随 `protocol_version_in_lock_table` 之后）

- [ ] **Step 1: Write the failing test**

在 `test/test_cep_schema.cc` 追加（helper 契约面）：

```cpp
// ADR-CEP-11 SU(2026-11-26)①:两分支谓词(Entry #109 G-2:单一实现,禁多处各抄一份)
TEST_CASE("[cep][schema] 两分支谓词必须接受 user:<已登记 ssp-id> 且拒绝其余",
          "[cep][schema][ic1]") {
    const auto lock = read_file(
        (repo_root() / "docs" / "adr" / "ADR-CEP-11-protocol-version-locking.md").string());
    REQUIRE_FALSE(lock.empty());
    const std::set<std::string> table = cpptlm::cep::parse_protocol_version_lock_table(lock);
    REQUIRE(table.size() == 8u);

    const std::set<std::string> ssp_ids = cpptlm::cep::parse_registered_ssp_ids(
        read_file((repo_root() / "include" / "tlm" / "cep" / "ssps.json").string()));
    REQUIRE(ssp_ids.count("mesif_ssp") == 1u);

    // ── 分支 ①:锁表内 ⇒ 真 ─────────────────────────────────────────
    CHECK(cpptlm::cep::protocol_version_allowed("chi-r2", table, ssp_ids));
    CHECK(cpptlm::cep::protocol_version_allowed("mesif-1.0", table, ssp_ids));
    // ── 分支 ②:user:<已登记 id> ⇒ 真 ────────────────────────────────
    CHECK(cpptlm::cep::protocol_version_allowed("user:mesif_ssp", table, ssp_ids));
    // ── 反向 fixture:4 条各自按**正确原因**红 ───────────────────────
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("user:no_such_ssp", table, ssp_ids)); // ② id 未登记
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("chi-r3", table, ssp_ids));           // ① 拼错的官方版本
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("user:", table, ssp_ids));            // ② 空 id
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("", table, ssp_ids));                 // 空串
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **编译失败** —— `parse_registered_ssp_ids` / `protocol_version_allowed` 未声明

- [ ] **Step 3: Write minimal implementation**

在 `test/cep_adr_parser.hh` 的 `protocol_version_in_lock_table` 之后追加：

```cpp
        /// 从 `ssps.json` 提取**已登记**的 `ssp.id` 集合
        /// (per ADR-CEP-11 SU(2026-11-26)① 分支 ② —— 「已登记」的唯一真源;
        ///  词表 ^[a-z][a-z0-9_]*$ 由 ssp_loader.hh 的 E2 保证)
        inline std::set<std::string>
        parse_registered_ssp_ids(const std::string& ssps_json_text) {
            std::set<std::string> out;
            // 取 `"id"` 字段值 —— ssps.json 的每条 SSP 恰有一个 id 键
            const std::string key = "\"id\"";
            std::size_t pos = 0;
            while ((pos = ssps_json_text.find(key, pos)) != std::string::npos) {
                std::size_t colon = ssps_json_text.find(':', pos + key.size());
                if (colon == std::string::npos)
                    break;
                std::size_t q1 = ssps_json_text.find('"', colon + 1);
                if (q1 == std::string::npos)
                    break;
                std::size_t q2 = ssps_json_text.find('"', q1 + 1);
                if (q2 == std::string::npos)
                    break;
                out.insert(ssps_json_text.substr(q1 + 1, q2 - q1 - 1));
                pos = q2 + 1;
            }
            return out;
        }

        /// 两分支谓词:`protocol_version` 是否**合法**
        /// (per ADR-CEP-11 SU(2026-11-26)① —— 决策 3 / INV-CEP-11-IC1 两分支化)
        ///   分支 ① —— 在锁表内
        ///   分支 ② —— 匹配 ^user:([a-z][a-z0-9_]*)$ 且 <ssp-id> ∈ 已登记 ssp.id 集合
        /// ⛔ 两者皆不满足 ⇒ 装配期 fail(不得静默降级)
        /// ⚠️ **单一实现**(Entry #109 G-2)—— ⛔ 禁在别处另抄一份
        inline bool protocol_version_allowed(const std::string& version,
                                             const std::set<std::string>& table,
                                             const std::set<std::string>& ssp_ids) {
            if (version.empty())
                return false;
            // 分支 ①
            if (table.count(version) == 1)
                return true;
            // 分支 ②
            static const std::regex re(R"(^user:([a-z][a-z0-9_]*)$)");
            std::smatch m;
            if (!std::regex_match(version, m, re))
                return false;
            return ssp_ids.count(m[1].str()) == 1;
        }
```

⚠️ 需确认该头已 `#include <regex>` 与 `<set>`；若无则补上（既有 `protocol_version_in_lock_table` 已用 `std::set` ⇒ `<set>` 应已在）。

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 4: IC1 通用闸两分支化（`:573`）

**Files:**
- Modify: `test/test_cep_schema.cc:573-625`（IC1 通用闸用例）

- [ ] **Step 1: Write the failing test**

把既有 IC1 通用闸的判定从**单分支**改为**两分支**（在同一用例内改断言）：

```cpp
    const std::set<std::string> ssp_ids = cpptlm::cep::parse_registered_ssp_ids(
        read_file((repo_root() / "include" / "tlm" / "cep" / "ssps.json").string()));
    REQUIRE(ssp_ids.count("mesif_ssp") == 1u);
    ...
        CHECK(cpptlm::cep::protocol_version_allowed(ver, table, ssp_ids));   // ← 两分支
    ...
    // 反向 fixture ①:合成**表外**取值 ⇒ 谓词必假(鉴别力证明)
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("chi-r9", table, ssp_ids));
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("mesif-9.9", table, ssp_ids));
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("", table, ssp_ids));
    // 反向 fixture ②:user:<未登记 id> ⇒ 红(分支 ② 的判别力)
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("user:no_such_ssp", table, ssp_ids));
    // 反向 fixture ③:user:<空 id> ⇒ 红
    CHECK_FALSE(cpptlm::cep::protocol_version_allowed("user:", table, ssp_ids));
    // 正控:表内取值 ⇒ 谓词为真(否则上面的反控可能因谓词恒假而恒绿)
    CHECK(cpptlm::cep::protocol_version_allowed("chi-r2", table, ssp_ids));
    CHECK(cpptlm::cep::protocol_version_allowed("mesif-1.0", table, ssp_ids));
    // 正控:user:<已登记 id> ⇒ 谓词为真(分支 ② 可达性)
    CHECK(cpptlm::cep::protocol_version_allowed("user:mesif_ssp", table, ssp_ids));
```

「把**真实声明**改成表外值 ⇒ 通用闸必红」的变异块（既有 fixture ②）同步改用两分支谓词。

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **PASS**（因为 Task 3 已实现 helper）—— ⚠️ 本 task 的 RED 来自**语义**而非编译：若 Task 3 的 helper 未接进本用例，则断言仍走单分支 ⇒ `user:` 形态**未被覆盖** ⇒ 需靠 Task 5 的「唯一实现」反向 fixture 证明。**执行顺序**：Task 3 → Task 4 → Task 5，Task 4 的正确性由 Task 5 的反向 fixture 兜底。

- [ ] **Step 3: Write minimal implementation**

（Step 1 已给出全部改动 —— 本 task 只改**测试调用点**，不改 helper。）

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **PASS** · 且既有 8 实例（实际 3 个声明实例）全绿

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 5: 「两分支谓词是唯一实现」反向 fixture（AC-5）

**Files:**
- Test: `test/test_cep_schema.cc`（新增用例）

- [ ] **Step 1: Write the failing test**

```cpp
// Entry #109 G-2 的**机器化**:两分支谓词必须只有一份实现 —— ⛔ 无第二份拷贝
TEST_CASE("[cep][schema] 两分支谓词只有唯一实现(禁第二份拷贝)",
          "[cep][schema][ic1]") {
    // ① helper 存在且可调用(单一真源)
    const std::set<std::string> table{"chi-r2"};
    const std::set<std::string> ids{"mesif_ssp"};
    CHECK(cpptlm::cep::protocol_version_allowed("user:mesif_ssp", table, ids));
    // ② ⛔ 仓内**不得**出现第二份「user:」两分支判定的字面拷贝
    //    —— 扫描 CEP 实现面,凡出现 `user:` 形态判定的**另一处**即红
    const auto hits = grep_repo_files({"include/tlm/cep", "src", "test"}, "user:");
    // 允许的落点:helper 本体(cep_adr_parser.hh) + 本测试文件 + schema/文档
    for (const auto& h : hits) {
        INFO("命中 user: 判定的文件: " << h.path);
        const bool allowed = (h.path.find("cep_adr_parser.hh") != std::string::npos) ||
                             (h.path.find("test_cep_schema.cc") != std::string::npos);
        CHECK(allowed);
    }
}
```

> ⚠️ `grep_repo_files` 若仓内无同名 helper，则在本用例内用**最小自实现**（`std::filesystem::recursive_directory_iterator` + 逐文件读文本）替代，⛔ 不新建公共工具函数（避免扩大对账面）。

- [ ] **Step 2: Run test to verify it fails**

Run: `./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **PASS**（本用例是**守门型** —— 若实现里真出现了第二份拷贝则红；实现正确时首次即绿）

- [ ] **Step 3: Write minimal implementation**

（Step 1 已给出全部内容 —— 本 task 是**纯新增守门用例**，无生产代码改动。）

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build -j$(nproc) && ./build/bin/cpptlm_tests "[cep][schema][ic1]"`
Expected: **PASS**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit。

---

### Task 6: main spec 的 IC1 Scenario 两分支化 + **两处五步链**（⛔ 冻结面）

**Files:**
- Modify: `openspec/specs/cep-core-contracts/spec.md:329-340`（`cep-protocol-version-locking-contract` 的 IC1 Scenario）
- Modify: `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`（`--update` 产出）

- [ ] **Step 1: Write the failing test**

```bash
# IC1 Scenario 两分支化的机判
grep -c "user:" openspec/specs/cep-core-contracts/spec.md   # 期望 ≥ 1
grep -c "两分支" openspec/specs/cep-core-contracts/spec.md   # 期望 ≥ 1
# 冻结面复验(改动后必须 rc=0)
bash scripts/test/cep_freeze_check.sh                        # 期望 rc=0
```

- [ ] **Step 2: Run test to verify it fails**

Run: 上述 3 条
Expected: `grep "user:"` 计数 **0**（Scenario 未两分支化）；`cep_freeze_check.sh` **rc≠0**（hash 漂移）

- [ ] **Step 3: Write minimal implementation**

把 `openspec/specs/cep-core-contracts/spec.md` 的 IC1 Scenario 的 `THEN` 段改为（**IC2 / IC3 逐字零改动**）：

```markdown
#### Scenario: 版本取值必须落在锁定表内(IC1)

- **WHEN** 契约测试校验 `ADR-CEP-11 §3` 的锁定表与 schema 的 `protocol_version`
- **THEN** 锁定表**恰好 8 个协议族**:CHI / CXL 2.0 非对称 / CXL 3.0 对称 / TileLink-C / NVLink / UALink / MOESI/GPU / MESIF
  (2026-10-22 per 台账 Entry #12 —— 增 `moesi_gpu` / `mesif`,二者均为 ArchForge 贡献且 `family` 落 `industrial`)
- **AND** 每个协议实例的 `protocol_version` **必须**满足**两分支之一**:
  - **分支 ①** —— **在** `ADR-CEP-11 §3` 锁定表**内**
  - **分支 ②** —— 匹配 `^user:([a-z][a-z0-9_]*)$` **且** 捕获组 `<ssp-id>` ∈ `include/tlm/cep/ssps.json` 的**已登记** `ssp.id`
- **AND** 两分支皆不满足 ⇒ **装配期 fail**(不得静默降级)
- **AND** ⚠️ **分支 ② 是「表外但合法」** —— ⛔ **不改** `INV-CEP-11-IC1` 的**立法意图**(仍禁「配置层错误静默降级」); ⛔ **不改** `INV-CEP-11-IC2`(§3 锁定表仍是规范版本的**唯一真源**,只允许**表外一种可机判形态**)
- **AND** 两分支**可证不相交**:锁定表内**全部**官方版本**均含** `-` 或 `.`,而 `ssp.id` 词表 = `^[a-z][a-z0-9_]*$`(`include/tlm/cep/ssp_loader.hh` **E2**)**排除** `-` 与 `.` ⇒ `user:<ssp-id>` **不可能**与任一官方版本字面相同 ⇒ 分支 ② **无法冒充分支 ①**
```

⚠️ 该 main spec 的内容须与 `openspec/changes/cep-ssp-cp-q1b-landing/specs/cep-core-contracts/spec.md` 的 delta **逐字一致**（D-5）⇒ 归档时 `openspec archive` 的 MODIFIED 应用是幂等的。

**五步链（D-7 · 物理串行 · ⛔ 不得把「改冻结面」与「改清单」混进同一个 commit）**：

1. **台账 Entry 授权** —— 在 `.rddf/roadmap/objectives/obj-cep-platform.md` §11 追加一条 Entry（`scope-change` 形态），写明本 change 的两处冻结面授权；
2. **产物 commit** —— 提交 schema + main spec + 代码 + 测试；
3. `bash scripts/test/cep_freeze_check.sh --update --authorized-by "Entry #N"`（`N` = 第 1 步的 Entry 号）；
4. **清单单独 commit** —— 只提交 `docs/implementation/2026-10-21-cep-stage0-freeze-manifest.json`；
5. **无参复验** —— `bash scripts/test/cep_freeze_check.sh` ⇒ **rc=0**（硬门）。

- [ ] **Step 4: Run test to verify it passes**

Run:
```bash
grep -c "user:" openspec/specs/cep-core-contracts/spec.md
bash scripts/test/cep_freeze_check.sh
openspec validate cep-ssp-cp-q1b-landing --strict
```
Expected: `grep` 计数 **≥ 1** · `cep_freeze_check.sh` **rc=0** · `openspec validate` **rc=0**

- [ ] **Step 5: Defer commit**

按仓库约定，execute 阶段不逐任务 commit；所有变更将在 archive 阶段统一提交。

---

## Test Plan（验收清单）

| # | 命令 | 期望 |
|:--:|---|---|
| 1 | `./build/bin/cpptlm_tests "[cep][schema][ic1]"` | 全绿（含 4 条反向 fixture 各自按正确原因红）|
| 2 | `./build/bin/cpptlm_tests "[cep][schema]"` | 全绿（既有 schema 用例零回归）|
| 3 | `ctest --test-dir build --output-on-failure -j4` | 全绿 |
| 4 | `bash scripts/test/cep_freeze_check.sh` | **rc=0**（两处冻结面）|
| 5 | `openspec validate cep-ssp-cp-q1b-landing --strict` | rc=0 |
| 6 | `openspec validate --changes --strict` | 全绿 |
| 7 | `bash scripts/test/docs_sync_check.sh --strict` | rc=0（路径未漂移）|
| 8 | `./scripts/build/format.sh --check` | rc=0（C++ 改动格式）|

## Risk Mitigation（本 change 子集）

见 `design.md` §4（R-1 ~ R-5）。核心三条：R-1 两分支谓词唯一实现（Task 5 守门）· R-2 pattern 两分支写错（4 条反向 fixture）· R-3 冻结面清单未更新（五步链第 5 步硬门）。

## Done Definition（archive 前）

- [ ] Task 1~6 全部完成，`tasks.md` 全部 `- [x]`
- [ ] Test Plan 8 条命令全绿（rc=0）
- [ ] 两处冻结面的**五步链 5 步**全部完成（台账 Entry 已落 + 清单单独 commit + 无参复验 rc=0）
- [ ] `openspec validate --changes --strict` 全绿
- [ ] ⛔ **表述纪律**：交付说明中**不得**宣称「CEP 已支持用户自定义协议」（D-8）
