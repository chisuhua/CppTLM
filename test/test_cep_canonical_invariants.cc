// test/test_cep_canonical_invariants.cc
// CEP Stage 0 — canonical 不变量契约测试
// per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
//     + ADR-CEP-14 §2 决策 2(C1~C10)+ ADR-CEP-NUMBER-MAP §3
// 作者: CppTLM Team / 日期: 2026-10-21
//
// 契约边界(per ADR-CEP-14 决策 1):本文件断言的是「**契约存在且自洽**」,
// **不**断言「实现满足契约」—— 后者由 Stage 1+ 的各 change AC 落地。
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

using cpptlm::cep::all_cep_adr_files;
using cpptlm::cep::all_defined_invariant_ids;
using cpptlm::cep::defined_invariant_ids;
using cpptlm::cep::is_defined;
using cpptlm::cep::is_invariant_id;
using cpptlm::cep::no_inv_adr_files;
using cpptlm::cep::owner_specs;
using cpptlm::cep::read_file;
using cpptlm::cep::read_truth_source_groups;
using cpptlm::cep::repo_root;

namespace {

    // ── 期望的 15 组 invariant ID(契约;**不**复制 invariant 文本) ──────
    // 组名必须与 `docs/architecture/07-coherence-exploration-platform.md §4` 的
    // 总表行名**逐字一致**(per ADR-CEP-14 IC1 的锚点完备要求)。
    struct GroupSpec {
        std::string name;
        std::vector<std::string> ids;
        // 该组内**永久跳空**、仅被提及而从未定义的编号
        std::vector<std::string> skipped;
    };

    const std::vector<GroupSpec>& expected_groups() {
        static const std::vector<GroupSpec> g = {
            {"Coherence Interface",
             {"CEN-Ib1", "CEN-Ib2", "CEN-Ib3", "CEN-Ib4", "CEN-Ib5", "CEN-Ib6", "CEN-Ib7",
              "CEN-Ib8", "CEN-Ib9", "CEN-Ib10", "CEN-Ib11"},
             {}},
            {"Stub Cache Core", // 组名逐字对齐 `07 号 §4` 总表行名
             {"INV-SC1", "INV-SC2", "INV-SC3", "INV-SC4", "INV-SC5", "INV-SC6", "INV-CEP-05-IC1"},
             {}},
            {"Sharer Tracker",
             {"INV-CEP-04-IC1", "INV-CEP-04-IC2", "INV-CEP-04-IC3", "INV-CEP-04-IC5",
              "INV-CEP-04-IC6", "INV-CEP-04-IC7"},
             {"INV-CEP-04-IC4"}},
            {"Experiment Protocol",
             {"INV-CEP-06-IC1", "INV-CEP-06-IC2", "INV-CEP-06-IC3", "INV-CEP-06-IC4",
              "INV-CEP-06-IC5"},
             {}},
            {"Simulation + Seam",
             {"INV-CEP-07-IC1", "INV-CEP-07-IC2", "INV-CEP-07-IC3", "INV-CEP-07-IC4",
              "INV-CEP-07-IC5", "INV-CEP-07-IC6", "INV-CEP-07-IC7"},
             {}},
            {"Numbering", {"INV-CEP-NUM-IC1", "INV-CEP-NUM-IC2", "INV-CEP-NUM-IC3"}, {}},
            {"Ordering Point", {"INV-CEP-08-IC1", "INV-CEP-08-IC2", "INV-CEP-08-IC3"}, {}},
            {"Multi-VC / Deadlock Freedom",
             {"INV-CEP-09-IC1", "INV-CEP-09-IC2", "INV-CEP-09-IC3"},
             {}},
            {"Protocol Version Locking",
             {"INV-CEP-11-IC1", "INV-CEP-11-IC2", "INV-CEP-11-IC3"},
             {}},
            {"Capability Map", {"INV-CEP-12-IC1", "INV-CEP-12-IC2", "INV-CEP-12-IC3"}, {}},
            {"Coherence Domain", {"INV-CEP-13-IC1", "INV-CEP-13-IC2", "INV-CEP-13-IC3"}, {}},
            {"Stub Contract Tests", {"INV-CEP-14-IC1", "INV-CEP-14-IC2", "INV-CEP-14-IC3"}, {}},
            {"CacheTLM Alignment", {"INV-CEP-15-IC1", "INV-CEP-15-IC2", "INV-CEP-15-IC3"}, {}},
            {"06 Evolution Mapping", {"INV-CEP-16-IC1", "INV-CEP-16-IC2", "INV-CEP-16-IC3"}, {}},
            {"06-bis Artifact Disposition",
             {"INV-CEP-07A-IC1", "INV-CEP-07A-IC2", "INV-CEP-07A-IC3"},
             {}},
        };
        return g;
    }

    std::size_t total_defined() {
        std::size_t n = 0;
        for (const auto& g : expected_groups())
            n += g.ids.size();
        return n;
    }

    std::size_t total_skipped() {
        std::size_t n = 0;
        for (const auto& g : expected_groups())
            n += g.skipped.size();
        return n;
    }

    // **全仓**已定义 invariant —— 扫描 docs/adr/ 下**每一份** ADR-CEP-*.md。
    //
    // ⚠️ 此前只扫 owner_specs() 的 15 份 → 其余 6 份(non-owner)完全不可见,
    //    「全仓」名不副实:实测往 ADR-CEP-17 注入 `- **INV-CEP-17-IC1**: …`
    //    → C++ 测试与 lint **双双全绿**(静默漏检)。
    //
    // **定义语义**:一个 ID 只有在**其 owner ADR 内**才算「被定义」;
    // 在其它 ADR 内出现的是**跨 ADR 引用**(实测 ADR-CEP-17:280-281 的
    // `| **INV-CEP-04-IC7** | … | ADR-CEP-04 §11 |` 是引用表,不是定义)。
    // 前缀**无 owner** 的定义形态 ID = **孤儿** → 契约违反(必须 fail)。
    inline std::set<std::string> whole_repo_defined_ids() {
        std::set<std::string> all;
        for (const auto& f : all_cep_adr_files()) {
            const auto text = read_file((repo_root() / "docs" / "adr" / f).string());
            for (const auto& id : defined_invariant_ids(text)) {
                if (cpptlm::cep::owner_of(id) == f)
                    all.insert(id);
            }
        }
        return all;
    }

    // 全仓的**孤儿**定义形态 ID —— 前缀无 owner(既不是已登记的 invariant,
    // 也不是任何 owner ADR 的定义) → 契约违反
    inline std::set<std::string> whole_repo_orphan_ids() {
        std::set<std::string> orphans;
        for (const auto& f : all_cep_adr_files()) {
            const auto text = read_file((repo_root() / "docs" / "adr" / f).string());
            for (const auto& id : defined_invariant_ids(text)) {
                if (cpptlm::cep::owner_of(id).empty())
                    orphans.insert(id);
            }
        }
        return orphans;
    }

    // 某份 ADR 内已定义的 invariant id 集合
    inline std::set<std::string> defined_in(const std::string& adr_file) {
        return defined_invariant_ids(read_file((repo_root() / "docs" / "adr" / adr_file).string()));
    }

} // namespace

TEST_CASE("[cep][canonical-invariant] 15 组 invariant 的 owner ADR 均已定义",
          "[cep][canonical-invariant]") {
    for (const auto& g : expected_groups()) {
        for (const auto& id : g.ids) {
            INFO("组 " << g.name << " 的 " << id << " 未在其 owner ADR 中定义");
            REQUIRE(is_defined(id));
        }
    }
}

TEST_CASE("[cep][canonical-invariant] 永久跳空编号确实从未被定义", "[cep][canonical-invariant]") {
    // per 2026-10-21 实测教训:跳空 ID 被多处**提及**但**从未定义**;
    // 计数脚本必须区分二者,否则会多算 1 条。
    for (const auto& g : expected_groups()) {
        for (const auto& id : g.skipped) {
            INFO("跳空编号 " << id << " 不应被定义");
            REQUIRE_FALSE(is_defined(id));
        }
    }
}

TEST_CASE("[cep][canonical-invariant] 全仓无未登记的已定义 invariant",
          "[cep][canonical-invariant]") {
    std::set<std::string> expected;
    for (const auto& g : expected_groups()) {
        expected.insert(g.ids.begin(), g.ids.end());
    }
    // 扫**全部** ADR-CEP-*.md(不止 owner 的 15 份)
    const auto actual = whole_repo_defined_ids();
    std::set<std::string> extra;
    for (const auto& id : actual) {
        if (expected.count(id) == 0)
            extra.insert(id);
    }
    INFO("多出的已定义 invariant: " << extra.size());
    for (const auto& id : extra)
        INFO("  多出: " << id);
    REQUIRE(extra.empty());
    // owner 口径与全仓口径必须一致(说明 non-owner ADR 确实一条未定义)
    REQUIRE(actual == all_defined_invariant_ids());
}

TEST_CASE("[cep][canonical-invariant] 全部 ADR 都被归类(owner 或 no-inv)",
          "[cep][canonical-invariant]") {
    // 完备性:目录里每份 ADR 必须 ∈ owner_specs ∪ no_inv_adr_files;
    // 否则其 invariant **不被计数**且静默(正是 Oracle N1/N2 的形态)。
    std::set<std::string> owners;
    for (const auto& o : owner_specs())
        owners.insert(o.adr_file);
    std::set<std::string> noinv(no_inv_adr_files().begin(), no_inv_adr_files().end());

    const auto ondisk = all_cep_adr_files();
    REQUIRE_FALSE(ondisk.empty());
    for (const auto& f : ondisk) {
        INFO("ADR 未归类(owner 或 no-inv): " << f);
        REQUIRE((owners.count(f) == 1 || noinv.count(f) == 1));
    }
    // 反向:owner / no-inv 表里不得有磁盘上不存在的文件
    for (const auto& f : owners) {
        INFO("owner 表指向不存在的 ADR: " << f);
        REQUIRE(std::filesystem::exists(repo_root() / "docs" / "adr" / f));
    }
    for (const auto& f : noinv) {
        INFO("no-inv 表指向不存在的 ADR: " << f);
        REQUIRE(std::filesystem::exists(repo_root() / "docs" / "adr" / f));
    }
}

TEST_CASE("[cep][canonical-invariant] no-inv 白名单的 ADR 一旦定义 invariant 即违反契约",
          "[cep][canonical-invariant]") {
    // 这 6 份 ADR 本就不定义 invariant;若某天开始定义(如 `- **INV-CEP-17-IC1**: …`),
    // 该 ID 前缀无 owner → 落入**孤儿**集合 → 下面那条用例必红。
    // 这里断言它们**不拥有**任何 invariant(跨 ADR 引用不算)。
    for (const auto& f : no_inv_adr_files()) {
        std::set<std::string> owned;
        for (const auto& id : defined_in(f)) {
            if (cpptlm::cep::owner_of(id) == f)
                owned.insert(id);
        }
        INFO("no-inv ADR 拥有已定义 invariant: " << f);
        for (const auto& id : owned)
            INFO("  " << id);
        REQUIRE(owned.empty());
    }
}

TEST_CASE("[cep][canonical-invariant] 全仓无孤儿定义(前缀无 owner 的 invariant)",
          "[cep][canonical-invariant]") {
    // 任何「定义形态」但**前缀无 owner**的 invariant id = 未登记的 invariant。
    // 这条同时覆盖:
    //   - 在任一 ADR 里新增 `INV-CEP-NN-ICx`(NN 不在 owner 表)
    //   - 在 no-inv ADR(如 ADR-CEP-17)里新增 `- **INV-CEP-17-IC1**: …`
    const auto orphans = whole_repo_orphan_ids();
    INFO("孤儿 invariant 数: " << orphans.size());
    for (const auto& id : orphans)
        INFO("  孤儿: " << id);
    REQUIRE(orphans.empty());
}

TEST_CASE("[cep][canonical-invariant] 实际定义数与编号位口径", "[cep][canonical-invariant]") {
    // 实际定义 = 各组之和;编号位 = 实际定义 + 永久跳空
    REQUIRE(total_defined() == 66);
    REQUIRE(total_skipped() == 1);
    REQUIRE(total_defined() + total_skipped() == 67);
    REQUIRE(all_defined_invariant_ids().size() == total_defined());
}

TEST_CASE("[cep][canonical-invariant] 07 号 §4 总表与契约一致", "[cep][canonical-invariant]") {
    const auto rows = read_truth_source_groups();
    REQUIRE_FALSE(rows.empty());

    std::map<std::string, int> truth;
    for (const auto& r : rows)
        truth[r.name] = r.declared_count;

    for (const auto& g : expected_groups()) {
        INFO("07 号 §4 缺少组 " << g.name);
        REQUIRE(truth.count(g.name) == 1);
        INFO("07 号 §4 的组 " << g.name << " 条数与契约不符");
        REQUIRE(truth[g.name] == static_cast<int>(g.ids.size()));
    }
    REQUIRE(truth.size() == expected_groups().size());
}

TEST_CASE("[cep][canonical-invariant] 07 号 §4 的 ID 区间列与契约**集合相等**",
          "[cep][canonical-invariant]") {
    // ⚠️ 此前只比「条数」→ 区间写错 / 组间互换 owner **不可见**。
    //    实测把 Ordering Point 行的 ID 区间改成 INV-CEP-09-IC1 ~ IC3 → 两个 gate 全绿。
    // 现在逐组做**集合相等**断言。
    const auto rows = read_truth_source_groups();
    std::map<std::string, std::set<std::string>> truth_ids;
    for (const auto& r : rows)
        truth_ids[r.name] = r.ids;

    std::set<std::string> union_truth;
    for (const auto& g : expected_groups()) {
        INFO("07 号 §4 缺少组 " << g.name);
        REQUIRE(truth_ids.count(g.name) == 1);
        const std::set<std::string> want(g.ids.begin(), g.ids.end());
        const auto& got = truth_ids[g.name];
        // 差集逐条报出(便于定位是"多写"还是"漏写")
        std::set<std::string> missing, surplus;
        for (const auto& id : want)
            if (!got.count(id))
                missing.insert(id);
        for (const auto& id : got)
            if (!want.count(id))
                surplus.insert(id);
        for (const auto& id : missing)
            INFO("  07 号缺: " << id);
        for (const auto& id : surplus)
            INFO("  07 号多: " << id);
        INFO("07 号 §4 的组 " << g.name << " ID 集合与契约不符");
        REQUIRE(missing.empty());
        REQUIRE(surplus.empty());
        union_truth.insert(got.begin(), got.end());
    }
    // 并集 == 全部已定义 id(防漏引 / 防重复引用同一 id 到多组)
    std::set<std::string> all_expected;
    for (const auto& g : expected_groups())
        all_expected.insert(g.ids.begin(), g.ids.end());
    REQUIRE(union_truth == all_expected);
    // 且每个 id 恰好被引用一次(不重不漏)
    std::size_t total_refs = 0;
    for (const auto& r : rows)
        total_refs += r.ids.size();
    REQUIRE(total_refs == all_expected.size());
}

TEST_CASE("[cep][canonical-invariant] C10 组合接缝检查项清单存在且含 E 态静默升级",
          "[cep][canonical-invariant]") {
    // per ADR-CEP-14 决策 4:Stage 0 只落**契约层** —— 断言
    //   ① 组合接缝的**检查项清单存在**(至少含 "E 态静默升级" 项);
    //   ② **组合检查器缺失该项时,其结果判定为 fail**(不得静默跳过)。
    // ⚠️ 2026-10-21 代码复审发现:本项此前**零覆盖**(全 test/ 目录 grep
    //    "E 态" / "静默升级" 均 0 命中)—— ADR 明确划给 Stage 0 却没做。
    const auto adr08 = read_file(
        (repo_root() / "docs" / "adr" / "ADR-CEP-08-ordering-point-portability.md").string());
    const auto adr13 = read_file(
        (repo_root() / "docs" / "adr" / "ADR-CEP-13-coherence-domain-ownership.md").string());
    const auto adr14 =
        read_file((repo_root() / "docs" / "adr" / "ADR-CEP-14-stub-contract-tests.md").string());

    // ① 检查项清单存在:「E 态静默升级」须在 Ordering Point ADR 的组合必查项中
    INFO("ADR-CEP-08 未声明 E 态静默升级为组合必查项");
    REQUIRE(adr08.find("E 态静默升级") != std::string::npos);
    REQUIRE(adr08.find("INV-CEP-08-IC2") != std::string::npos);
    REQUIRE(adr08.find("组合") != std::string::npos);
    // 跨域组合侧同样声明(per ADR-CEP-13 §判据 4)
    INFO("ADR-CEP-13 未声明 E 态静默升级为跨域组合必查项");
    REQUIRE(adr13.find("E 态静默升级") != std::string::npos);
    REQUIRE(adr13.find("INV-CEP-08-IC2") != std::string::npos);

    // ② 缺项 = fail(不得静默跳过)的规则必须被声明
    INFO("ADR-CEP-08 未声明「缺项 = fail」规则");
    REQUIRE(adr08.find("缺项 = fail") != std::string::npos);
    INFO("ADR-CEP-14 IC3 未声明「不得静默跳过」");
    REQUIRE(adr14.find("INV-CEP-14-IC3") != std::string::npos);
    REQUIRE(adr14.find("静默跳过") != std::string::npos);
    // 且 ADR-CEP-14 决策 4 明确把 Stage 0 的归属指向本文件
    INFO("ADR-CEP-14 决策 4 未把 Stage 0 归属指向 canonical_invariants 测试");
    REQUIRE(adr14.find("test_cep_canonical_invariants.cc") != std::string::npos);
}

TEST_CASE("[cep][canonical-invariant] invariant ID 形态合法", "[cep][canonical-invariant]") {
    for (const auto& g : expected_groups()) {
        for (const auto& id : g.ids) {
            INFO("非法 invariant id 形态: " << id);
            REQUIRE(is_invariant_id(id));
        }
        for (const auto& id : g.skipped) {
            INFO("非法跳空 id 形态: " << id);
            REQUIRE(is_invariant_id(id));
        }
    }
}

TEST_CASE("[cep][canonical-invariant] owner 前缀唯一且两两不互为前缀",
          "[cep][canonical-invariant]") {
    // ⚠️ 此前该用例名不副实:body 只做 expected_groups() 的 set 自洽,
    //    **完全没碰 owner 映射** → 给「owner 歧义已被守住」的错误信心。
    const auto m = cpptlm::cep::owner_prefix_map();
    REQUIRE_FALSE(m.empty());
    // 1) 前缀 → owner 唯一(owner_specs() 里同一前缀不得出现两次)
    for (const auto& s : owner_specs()) {
        for (const auto& p : s.id_prefixes) {
            INFO("owner 前缀重复登记: " << p);
            REQUIRE(m.count(p) == 1);
            REQUIRE(m.at(p) == s.adr_file);
        }
    }
    // 2) 前缀两两不互为前缀(否则 `INV-CEP-07-IC` 会吞掉 `INV-CEP-07A-IC1`)
    for (const auto& a : m) {
        for (const auto& b : m) {
            if (a.first == b.first)
                continue;
            INFO("owner 前缀互为前缀(有歧义): " << a.first << " / " << b.first);
            REQUIRE(b.first.rfind(a.first, 0) != 0);
        }
    }
    // 3) 每个已定义 id 恰有 1 个 owner
    for (const auto& id : whole_repo_defined_ids()) {
        int owners = 0;
        for (const auto& s : owner_specs()) {
            for (const auto& p : s.id_prefixes) {
                if (id.rfind(p, 0) == 0)
                    ++owners;
            }
        }
        INFO("invariant 的 owner 数不为 1: " << id);
        REQUIRE(owners == 1);
    }
}

TEST_CASE("[cep][canonical-invariant] ADR owner 映射覆盖全部已定义 ID",
          "[cep][canonical-invariant]") {
    // 每个 owner ADR 的前缀必须唯一地覆盖一个分组;否则 owner 判定会歧义
    std::set<std::string> all;
    for (const auto& g : expected_groups()) {
        for (const auto& id : g.ids) {
            INFO("invariant id 重复登记: " << id);
            REQUIRE(all.insert(id).second);
        }
    }
    REQUIRE(all.size() == total_defined());
}
