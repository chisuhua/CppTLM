// test/test_cep_contracts_crosscheck.cc
// CEP Stage 0 — 跨文档契约一致性测试
//
// 背景:2026-10-21 的 spec↔断言机械对照发现 4 个 requirement 的**关键 token 覆盖率 < 60%**
//       (cep-numbering / cep-protocol-version-locking / cep-evolution-route-mapping /
//        cep-06bis-artifact-disposition)—— spec 声称了契约,测试却没断言。
// 本文件补齐这 4 个 requirement 的断言。
//
// per openspec/changes/cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
// 作者: CppTLM Team / 日期: 2026-10-21
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    // 词边界包含判定(避免子串假阳性:如 "hierarchical" 含 "chi")
    bool contains_word(const std::string& hay, const std::string& needle) {
        if (needle.empty())
            return false;
        std::size_t pos = 0;
        while ((pos = hay.find(needle, pos)) != std::string::npos) {
            // `_` 不算词字符(与 test_cep_schema.cc 的 contains_word 保持一致)
            const bool left_ok =
                (pos == 0) || !std::isalnum(static_cast<unsigned char>(hay[pos - 1]));
            const std::size_t after = pos + needle.size();
            const bool right_ok =
                (after >= hay.size()) || !std::isalnum(static_cast<unsigned char>(hay[after]));
            if (left_ok && right_ok)
                return true;
            pos += 1;
        }
        return false;
    }

    std::string adr(const std::string& f) {
        return read_file((repo_root() / "docs" / "adr" / f).string());
    }

} // namespace

// ─────────────────────────────────────────────────────────────
// cep-numbering-contract
// ─────────────────────────────────────────────────────────────
TEST_CASE("[cep][contract] 编号表是唯一真源且无重号(IC1)", "[cep][contract]") {
    const auto num = adr("ADR-CEP-NUMBER-MAP.md");
    REQUIRE(num.find("唯一真源") != std::string::npos);

    // 每个 CEP-NN 编号在**决策表**(§2 Decision 1)内恰好一条表项。
    // 只取该表:文件其它位置(如依赖表)也会提到编号,不是「登记」。
    std::set<std::string> seen;
    int rows = 0;
    const std::size_t t0 = num.find("### 决策 1");
    const std::size_t t1 = num.find("### 决策 2");
    REQUIRE(t0 != std::string::npos);
    REQUIRE(t1 != std::string::npos);
    const std::string table = num.substr(t0, t1 - t0);
    std::size_t pos = 0;
    while ((pos = table.find("| **CEP-", pos)) != std::string::npos) {
        const std::size_t b = pos + 4;
        const std::size_t e = table.find('*', b);
        REQUIRE(e != std::string::npos);
        const std::string id = table.substr(b, e - b);
        // Catch2 无法分解 set::insert(...) 的返回值(非可比较表达式)→ 先取具名 bool
        const bool inserted = seen.insert(id).second;
        INFO("编号重复登记: " << id);
        REQUIRE(inserted);
        ++rows;
        pos = e;
    }
    REQUIRE(rows >= 18); // CEP-00 ~ CEP-18(跳空若干)
    REQUIRE(seen.count("CEP-08") == 1);
    REQUIRE(seen.count("CEP-13") == 1);
    REQUIRE(seen.count("CEP-18") == 1);
}

TEST_CASE("[cep][contract] 不变量 id 命名空间与 ADR 编号分离(IC2)", "[cep][contract]") {
    const auto num = adr("ADR-CEP-NUMBER-MAP.md");
    REQUIRE(num.find("INV-") != std::string::npos);
    REQUIRE(num.find("命名空间") != std::string::npos);
    // 跨 ADR 引用必须带 INV- 前缀
    REQUIRE(num.find("`INV-CEP-04-IC1`") != std::string::npos);
}

TEST_CASE("[cep][contract] 06 §15.4 口径冻结:不引 P6(IC3)", "[cep][contract]") {
    const auto num = adr("ADR-CEP-NUMBER-MAP.md");
    REQUIRE(num.find("§15.4") != std::string::npos);
    REQUIRE(num.find("P6") != std::string::npos);
    // 口径声明:以 §15.4 为准
    REQUIRE(num.find("以 §15.4 为准") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────
// cep-protocol-version-locking-contract
// ─────────────────────────────────────────────────────────────
TEST_CASE("[cep][contract] 锁定表是规范版本唯一真源(IC2)", "[cep][contract]") {
    const auto lock = adr("ADR-CEP-11-protocol-version-locking.md");
    REQUIRE(lock.find("唯一真源") != std::string::npos);
    REQUIRE(lock.find("只引用不复制") != std::string::npos);

    // 锁定表 8 个协议族(2026-10-22 per 台账 Entry #12:增 moesi_gpu / mesif)
    for (const char* fam :
         {"CHI", "CXL 2.0", "CXL 3.0", "TileLink-C", "NVLink", "UALink", "MOESI/GPU", "MESIF"}) {
        INFO("锁定表缺协议族: " << fam);
        REQUIRE(lock.find(fam) != std::string::npos);
    }
    // 且 schema / 代码里**不得**重复列出规范版本号(唯一真源)
    const auto schema = read_file(
        (repo_root() / "include" / "tlm" / "cep" / "protocol_instance.schema.json").string());
    for (const char* v : {"chi-r2", "cxl-2.0", "cxl-3.0", "tilelink-c-1.3", "nvlink-4.0",
                          "ualink-2.0", "moesi-gpu-1.0", "mesif-1.0"}) {
        INFO("schema 重复了锁定表的规范版本号: " << v);
        REQUIRE(schema.find(v) == std::string::npos);
    }
}

TEST_CASE("[cep][contract] 版本变更须留痕(IC3)", "[cep][contract]") {
    const auto lock = adr("ADR-CEP-11-protocol-version-locking.md");
    REQUIRE(lock.find("INV-CEP-11-IC3") != std::string::npos);
    REQUIRE(lock.find("scope-change") != std::string::npos);
    REQUIRE(lock.find("adr-amendment") != std::string::npos);
    // 三条升级路径
    REQUIRE(lock.find("仅锁表更新") != std::string::npos);
    REQUIRE(lock.find("语义适配") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────
// cep-evolution-route-mapping-contract
// ─────────────────────────────────────────────────────────────
TEST_CASE("[cep][contract] 每个 Stage 有 §15.4 承接声明(IC2)", "[cep][contract]") {
    const auto map = adr("ADR-CEP-16-06-evolution-route-mapping.md");
    REQUIRE(map.find("INV-CEP-16-IC2") != std::string::npos);
    // 6 个 §15.4 阶段逐项都有处置
    for (const char* st : {"P2.0", "P2.1", "P3", "P3.5", "P4", "P5"}) {
        INFO("§15.4 映射缺阶段: " << st);
        REQUIRE(map.find(st) != std::string::npos);
    }
    // 处置词齐备
    for (const char* w : {"supersede", "部分承接", "改判", "明确排除"}) {
        INFO("映射缺处置词: " << w);
        REQUIRE(map.find(w) != std::string::npos);
    }
}

TEST_CASE("[cep][contract] 协议语义项归 ArchForge(IC3)", "[cep][contract]") {
    const auto map = adr("ADR-CEP-16-06-evolution-route-mapping.md");
    REQUIRE(map.find("INV-CEP-16-IC3") != std::string::npos);
    for (const char* s : {"MesiGpuProtocol", "MesiThreeLevelProtocol", "translate_snoop"}) {
        INFO("映射未点名协议语义项: " << s);
        REQUIRE(map.find(s) != std::string::npos);
    }
    REQUIRE(map.find("ArchForge") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────
// cep-06bis-artifact-disposition-contract
// ─────────────────────────────────────────────────────────────
TEST_CASE("[cep][contract] 既有产物已废弃且被标注(IC1)", "[cep][contract]") {
    const auto dis = adr("ADR-CEP-07A-06bis-artifact-disposition.md");
    REQUIRE(dis.find("INV-CEP-07A-IC1") != std::string::npos);
    REQUIRE(dis.find("coherence_domain.hh") != std::string::npos);
    REQUIRE(dis.find("[[deprecated]]") != std::string::npos);
    // 处置:7 项产物逐项
    for (const char* a : {"coherence_domain.hh", "coherent_xbar_tlm.hh", "coherence_bundles_tlm.hh",
                          "coherence_domains", "validate_domain_boundary", "coherence_bridge",
                          "add_coherence_domain"}) {
        INFO("产物处置缺项: " << a);
        REQUIRE(dis.find(a) != std::string::npos);
    }
}

TEST_CASE("[cep][contract] 协议泄漏通用禁令:与协议无关的层无协议标识符(IC2)", "[cep][contract]") {
    const auto dis = adr("ADR-CEP-07A-06bis-artifact-disposition.md");
    REQUIRE(dis.find("INV-CEP-07A-IC2") != std::string::npos);
    REQUIRE(dis.find("协议泄漏通用禁令") != std::string::npos);

    // 实测:Stub Cache / 拓扑 Python 层不得含协议标识符
    // (既有两处违规已被 ADR-CEP-07A 登记:coherence_domain.hh 的 enum Protocol、
    //  layer.py 的 protocol= 参数 —— 本处校验的是 **Stub Cache** 与 **schema**)
    const auto stub = adr("ADR-CEP-05-stub-cache-core-interface.md");
    REQUIRE(stub.find("不感知") != std::string::npos);

    const auto schema = read_file(
        (repo_root() / "include" / "tlm" / "cep" / "protocol_instance.schema.json").string());
    std::string upper = schema;
    for (auto& c : upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (const char* proto : {"MESI", "MOESI", "CHI", "CXL", "TILELINK", "NVLINK", "UALINK"}) {
        INFO("schema 出现协议标识符: " << proto);
        REQUIRE_FALSE(contains_word(upper, proto));
    }
}

TEST_CASE("[cep][contract] coherence_domains 保留但改语义(IC3)", "[cep][contract]") {
    const auto dis = adr("ADR-CEP-07A-06bis-artifact-disposition.md");
    REQUIRE(dis.find("INV-CEP-07A-IC3") != std::string::npos);
    REQUIRE(dis.find("ADR-CEP-13") != std::string::npos);

    // 4 个既有 config 仍含该字段 → 字段不得删除
    const std::vector<std::string> cfgs = {
        "configs/apu_soc_full.json", "configs/apu_soc_phase7a.json", "configs/apu_soc_phase7b.json",
        "configs/hierarchy_tree_3level.json"};
    int with_field = 0;
    for (const auto& c : cfgs) {
        const auto p = repo_root() / c;
        if (!std::filesystem::exists(p))
            continue;
        const auto txt = read_file(p.string());
        if (txt.find("coherence_domains") != std::string::npos)
            ++with_field;
    }
    INFO("含 coherence_domains 的既有 config 数: " << with_field);
    REQUIRE(with_field >= 3);

    // 告警文案须指向 ADR-CEP-13(消费阶段)而非 ADR-X.14
    const auto parser = read_file((repo_root() / "src" / "core" / "topology_parser.cc").string());
    REQUIRE(parser.find("coherence_domains") != std::string::npos);
    REQUIRE(parser.find("ADR-CEP-13") != std::string::npos);
}
