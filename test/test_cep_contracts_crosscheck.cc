// test/test_cep_contracts_crosscheck.cc
// CEP Stage 0 — 跨文档契约一致性测试
//
// 背景:2026-10-21 的 spec↔断言机械对照发现 4 个 requirement 的**关键 token 覆盖率 < 60%**
//       (cep-numbering / cep-protocol-version-locking / cep-evolution-route-mapping /
//        cep-06bis-artifact-disposition)—— spec 声称了契约,测试却没断言。
// 本文件补齐这 4 个 requirement 的断言。
//
// per openspec/changes/archive/2026-10-23-cpptlm-cep-umbrella/specs/cep-core-contracts/spec.md
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

    // ─────────────────────────────────────────────────────────────────────
    // 2026-10-27 补齐(per 台账 Entry #83):C++ 侧「与协议无关的层」实测扫描
    //
    // 背景:`ADR-CEP-07A §4 验证口径`第 2 项声称「Stage 0 由
    //      `test_cep_canonical_invariants.cc` 的 C9 负向断言覆盖」——
    //      **实测不成立**(C9 不在 canonical_invariants.cc;实际在
    //      `test_cep_schema.cc`,且**只扫 schema JSON 一个文件**)。
    //      ⇒ C++ 侧(Stub / Directory & Ordering Plane)此前**零机器断言**。
    //      本次按 ADR-CEP-14 §2 决策 3 C9 的「源码 grep」口径补齐 C++ 侧。
    //
    // 扫描口径(**与 ADR 一致**):
    //   · **去注释后**扫(否则散文里的「禁止暴露 CHI」会被误判 = 假阳性;
    //     而把禁用词写进注释即可绕过 = 假阴性)—— per ADR-CEP-14 C9 的检验方式
    //   · 按**标识符词**判定(`chi_rnf` / `ChiRnfAdapter` 均命中;
    //     `chip_id` / `hierarchical` **不**命中 —— 子串匹配会把 `ARCHITECT` 误判为含 `CHI`)
    //   · 词集 = {MESI, MOESI, CHI, CXL, TILELINK, NVLINK, UALINK, MESIF}
    //     (与 `test_cep_schema.cc` 的 C9 集合对齐;**不含 TOKEN** ——
    //      token 是 MOESI_CMP 的**协议私有状态**概念(per ADR-CEP-17 决策 3),
    //      不是协议名;源码里 `token` 常见于词法解析局部变量 ⇒ 纳入即假阳性)
    // ─────────────────────────────────────────────────────────────────────
    const std::vector<std::string> protocol_neutral_layers = {
        "include/tlm/cep/stub_cache_core.hh",         // Stub Cache Core(ADR-CEP-05)
        "include/tlm/cep/sharer_tracker.hh",          // Directory(ADR-CEP-04)
        "include/tlm/cep/ordering_point_checker.hh",  // Ordering Plane(ADR-CEP-08)
        "include/tlm/cep/address_resolver.hh",        // Ordering Plane(ADR-CEP-08)
        "include/tlm/cep/channel_dependency_graph.hh",// Ordering Plane(ADR-CEP-09)
        // CohIf 载体(2026-10-27 per 台账 Entry #84 / 队列 #7 Phase 1)——
        // 跨模块共享的**业务** Bundle(真源 1 `06 号 §3.3` 第三类),字段语义取自
        // 平台级 canonical 流契约(`ADR-CEP-02`)⇒ **与协议无关** ⇒ 纳入本扫描。
        "include/bundles/cep_coherence_bundles_tlm.hh",
        // ProtocolEngine 端口面(2026-10-27 per 台账 Entry #85 / 队列 #7 Phase 2)——
        // 持**类型擦除**的 `ProtocolBridgeEr*`,不引入任何协议语义基类
        // (per `ADR-CEP-19` 决策 8)⇒ **与协议无关** ⇒ 纳入本扫描。
        "include/tlm/cep/protocol_engine_base.hh",
        // ProtocolBridge 特化桥 + 协议族编号表(2026-10-28 per 台账 Entry #86 /
        // 队列 #7 Phase 3)—— 类型擦除基类 + 模板子类 + CohIf↔NoC 桥,全部**不含**
        // 任何具体协议语义(per `ADR-CEP-19` 决策 8)⇒ **与协议无关** ⇒ 纳入本扫描。
        "include/framework/protocol_bridge.hh",
        "include/tlm/cep/protocol_family.hh",
        "include/tlm/cep/cohif_noc_bridge.hh",
    };
    for (const auto& rel : protocol_neutral_layers) {
        const std::string path = (repo_root() / rel).string();
        const std::string raw = read_file(path);
        REQUIRE_FALSE(raw.empty());
        const auto hits = cpptlm::cep::protocol_identifier_hits(cpptlm::cep::strip_comments(raw));
        for (const auto& h : hits) {
            INFO("与协议无关的层出现协议标识符: " << rel << " :: " << h);
            REQUIRE_FALSE(hits.count(h));
        }
    }

    // ADR-CEP-14 §2 决策 3 C9 第 4 条:Stub **不感知**协议侧的声明字段
    // (`ordering_assumption` / `vc_binding` / `consistency_labels` ——
    //  per ADR-CEP-07 IC7,它们是 ProtocolEngine 侧的声明)
    {
        const std::string raw = read_file(
            (repo_root() / "include" / "tlm" / "cep" / "stub_cache_core.hh").string());
        const std::string code = cpptlm::cep::strip_comments(raw);
        for (const char* field : {"ordering_assumption", "vc_binding", "consistency_labels",
                                  "sharer_tracker"}) {
            INFO("Stub 感知协议侧声明字段: " << field);
            REQUIRE_FALSE(cpptlm::cep::contains_identifier(code, field));
        }
        // 正控(判别力):同一机制必须能发现确实存在的标识符 ——
        //   否则「都没找到」可能只是比较逻辑恒真空(2026-10-27 变异测试实测捕获:
        //   首版误用 `identifier_words()`(大写 + 拆词)与小写原名比对 ⇒ 断言恒绿)。
        REQUIRE(cpptlm::cep::contains_identifier("int vc_binding = 0;", "vc_binding"));
        REQUIRE(cpptlm::cep::contains_identifier("int vcBinding = 0;", "vc_binding"));
        REQUIRE(cpptlm::cep::contains_identifier("int VC_Binding = 0;", "vc_binding"));
        REQUIRE(cpptlm::cep::contains_identifier("int ordering_assumption = 0;",
                                                 "ordering_assumption"));
        // 反控:不同标识符**不得**命中
        REQUIRE_FALSE(cpptlm::cep::contains_identifier("int vc_bindings = 0;", "vc_binding"));
        REQUIRE_FALSE(cpptlm::cep::contains_identifier("int vc = 0;", "vc_binding"));
    }

    // ── 正控(判别力)── 纯负向断言必须有正控,否则「都没找到」可能只是扫描本身失效
    //   (per `test_cep_stub_cache_core.cc` 判据纪律 ③)
    {
        const auto hits_of = [](const std::string& s) {
            return cpptlm::cep::protocol_identifier_hits(s);
        };
        REQUIRE(hits_of("struct ChiRnfAdapter {};").count("CHI") == 1);   // CamelCase
        REQUIRE(hits_of("chi_rnf").count("CHI") == 1);                    // 下划线形态
        REQUIRE(hits_of("tilelink_c").count("TILELINK") == 1);
        REQUIRE(hits_of("MESI_state").count("MESI") == 1);
        REQUIRE(hits_of("ualink_fabric").count("UALINK") == 1);
        // 反控:非协议标识符**不得**命中(否则扫描过宽 = 假阳性)
        REQUIRE(hits_of("chip_id").empty());          // 「chip」(芯片)≠「CHI」(协议)
        REQUIRE(hits_of("hierarchical").empty());
        REQUIRE(hits_of("mismatched").empty());
        REQUIRE(hits_of("no_such_word").empty());
        REQUIRE(hits_of("struct LineState { int x; };").empty());
    }

    // ── 拓扑 Python 层的**显式豁免登记**(不是漏检)──
    //   `cpptlm/topo/layer.py` 的 `protocol=` 参数与 `protocol="MESI"` 默认值
    //   是 ADR-CEP-07A §3 点名的**两处既有违规之一**(另一处是
    //   `coherence_domain.hh` 的 `enum Protocol`)。
    //   处置:per `ADR-CEP-07A` 决策 1 第 7 项 ③ —— **Stage 0~1 不改**
    //   (避免动既有 Python 测试),Stage 2 与 `ADR-CEP-13` 同步改。
    //   ⇒ 本断言**故意不覆盖**该层;此处登记豁免,使其不是「静默漏检」。
    {
        const std::string py = read_file((repo_root() / "cpptlm" / "topo" / "layer.py").string());
        REQUIRE_FALSE(py.empty());
        const bool has_protocol_param = py.find("protocol") != std::string::npos;
        REQUIRE(has_protocol_param); // 豁免的前提:该层确实仍有 protocol 参数
        // 豁免的**解除条件**登记:Stage 2 + ADR-CEP-13 落地后,本条须改为
        // 「拓扑 Python 层纳入扫描」并删除 layer.py 的 protocol 参数。
        // (per `ADR-CEP-07A` 决策 1 第 7 项 ③ / 决策 2 时间分期)
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
