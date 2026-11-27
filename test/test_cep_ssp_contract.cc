// test/test_cep_ssp_contract.cc
// SSP 协议在 `ADR-CEP-14` **C1~C10 合同测试体系**上的**实例化**（批 2 `cep-ssp-batch2` · AC-7）
//
// ⚠️ **C1~C10 是什么**（`ADR-CEP-14` 决策 1 逐字）:
//    「合同测试断言的是『Stub 对 Coherence Interface 的投影行为是否符合契约』,
//     **不**断言『Stub 作为一个缓存是否行为正确』」
//    ⇒ 它是**协议无关**的投影契约;「在 SSP 协议上实例化」= 逐条给出
//    **SSP 声明面**在该条款上的**结论**(PASS / SKIP + 理由)。
//
// ⛔ **不得冒充**:C4~C8(Stub 的 outstanding / 延迟 / 配对 / 写回载荷 / hit 路径)
//    与 C10(组合接缝)**属 Stub / 组合器侧**,SSP 声明面**无此面** ⇒ 判 **SKIP**
//    (per `AdapterSchemaContracts` 的三态纪律:SKIP 不得记为 PASS)。
//
// 逐条结论(本文件的可执行载体):
//   C1  接口完备性         => **PASS(声明层)**:SSP 的**非空** `flow` 全在 canonical 10 流内
//   C2  折叠映射           => **PASS(声明层)**:`Cap`/`Grow`/`Prune`/`Report` 的 flow 为空(部分映射)
//   C3  权限状态值域       => **PASS**:SSP `perm` ⊆ 4 值锁,⛔ 不含瞬态 / token / 七态
//   C4  outstanding 纪律   => **SKIP**(Stub 侧)
//   C5  snoop 延迟参数化   => **SKIP**(Stub 侧)
//   C6  completion 配对     => **SKIP**(Stub 侧)
//   C7  writeback 载荷      => **SKIP**(Stub 侧)
//   C8  hit 零网络流量      => **SKIP**(Stub 侧)
//   C9  协议信息不泄漏      => **PASS**:通用层零协议名 + 解释器零瞬态态
//   C10 组合接缝检查项      => **SKIP**(组合器侧)
//
// 作者: CppTLM Team / 日期: 2026-11-27
#include "catch_amalgamated.hpp"

#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "tlm/cep/event_alphabet.hh"
#include "tlm/cep/protocol_engine_base.hh"  // kCanonicalFlowNames（C1 的 10 流真源）
#include "tlm/cep/ssp_interpreter.hh"
#include "tlm/cep/ssp_loader.hh"

namespace {

namespace ssp = tlm::cep::ssp;

nlohmann::json load_ssps_doc() {
    const std::string path = std::string(CPPTLM_SOURCE_DIR) + "/include/tlm/cep/ssps.json";
    std::ifstream in(path);
    REQUIRE(in.good());
    nlohmann::json j;
    in >> j;
    return j;
}

std::string read_src(const std::string& rel) {
    std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
    if (!in) return {};
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("[cep][ssp][contract] C1 接口完备性 —— SSP 的非空 flow 全在 canonical 10 流内",
          "[cep][ssp][contract]") {
    const auto doc = load_ssps_doc();
    std::set<std::string> canonical(tlm::cep::kCanonicalFlowNames.begin(),
                                    tlm::cep::kCanonicalFlowNames.end());
    REQUIRE(canonical.size() == 10u);

    std::size_t checked = 0;
    for (const auto& s : doc["ssps"]) {
        for (const auto& e : s["events"]) {
            const std::string name = e.value("name", std::string{});
            // ① 事件**自带**的 flow（可空 —— 部分映射见 C2）
            const std::string flow = e.value("flow", std::string{});
            if (!flow.empty()) {
                INFO("ssp=" << s.value("id", "") << " event=" << name << " flow=" << flow);
                REQUIRE(canonical.count(flow) == 1u);
                ++checked;
            }
            // ② 由 `anchor` **派生**的 flow（`ssps.json` 的事件全走这条路；
            //    `Cap`/`Grow`/`Prune`/`Report` 的锚 flow 为空 ⇒ 不计入，见 C2）
            const std::string anchor = e.value("anchor", std::string{});
            if (anchor.empty()) continue;
            const ssp::AnchorInfo* info = ssp::find_core_anchor(anchor);
            REQUIRE(info != nullptr);
            if (info->flow == nullptr) continue;
            INFO("ssp=" << s.value("id", "") << " event=" << name << " anchor=" << anchor
                        << " derived_flow=" << info->flow);
            REQUIRE(canonical.count(std::string(info->flow)) == 1u);
            ++checked;
        }
    }
    // ⚠️ 反 vacuous:PASS 必须有**实际被检查**的 flow 项（自带或锚派生）
    REQUIRE(checked > 0u);
}

TEST_CASE("[cep][ssp][contract] C2 折叠映射 —— Cap/Grow/Prune/Report 的 flow 为空(部分映射)",
          "[cep][ssp][contract]") {
    const auto doc = load_ssps_doc();
    // 这四个是**本地权限转移**语义 ⇒ ⛔ 不对应任何 canonical 流(诚实边界的部分映射)
    const std::set<std::string> local_perm_moves{"Cap", "Grow", "Prune", "Report"};

    std::size_t matched = 0;
    for (const auto& s : doc["ssps"]) {
        for (const auto& e : s["events"]) {
            const std::string anchor = e.value("anchor", std::string{});
            if (local_perm_moves.count(anchor) == 0) continue;
            INFO("ssp=" << s.value("id", "") << " anchor=" << anchor);
            REQUIRE(e.value("flow", std::string{}).empty());
            ++matched;
        }
    }
    // ⚠️ `tilelink_cap_ssp` 恰有这 4 个事件 ⇒ 必被覆盖(非 vacuous)
    REQUIRE(matched == 4u);
}

TEST_CASE("[cep][ssp][contract] C3 权限状态值域 —— SSP perm ⊆ 4 值锁且不含禁词表",
          "[cep][ssp][contract]") {
    const auto doc = load_ssps_doc();
    // 4 值锁(CEN-Ib11)· ⛔ 不含瞬态 / token / 七态
    const std::set<std::string> four{"none", "read", "unique", "writeback"};

    std::size_t total = 0;
    for (const auto& s : doc["ssps"]) {
        for (const auto& p : s["perm"]) {
            INFO("ssp=" << s.value("id", "") << " perm=" << p.get<std::string>());
            REQUIRE(four.count(p.get<std::string>()) == 1u);
            ++total;
        }
    }
    REQUIRE(total > 0u);
}

TEST_CASE("[cep][ssp][contract] C4~C8 -> SKIP（Stub 侧，SSP 声明面无此面）",
          "[cep][ssp][contract]") {
    // ⚠️ **SKIP 不得记为 PASS**（per AdapterSchemaContracts 三态纪律）:
    //    C4 outstanding / C5 snoop 延迟 / C6 completion 配对 / C7 writeback 载荷 /
    //    C8 hit 路径 —— 全部是 **Stub 缓存行为**的契约,SSP 声明面**不定义**这些面。
    //    本用例的可执行断言 = **SSP 声明面不含**这些面的任何表达(⛔ 无冒充字段)。
    const auto doc = load_ssps_doc();
    for (const auto& s : doc["ssps"]) {
        for (const char* banned : {"outstanding", "latency", "txn_id", "writeback_data"}) {
            INFO("ssp=" << s.value("id", "") << " banned=" << banned);
            REQUIRE_FALSE(s.contains(banned));
        }
    }
    SUCCEED("C4~C8 判 SKIP（Stub 侧）—— SSP 声明面无对应字段,不冒充");
}

TEST_CASE("[cep][ssp][contract] C9 协议信息不泄漏 —— 通用层零协议名 + 解释器零瞬态态",
          "[cep][ssp][contract]") {
    // ① 通用层（SSP loader / 解释器）**零协议名**（`ADR-CEP-24` 决策 2.1 第四类允许面）
    const char* proto[] = {"MESI", "MOESI", "MESIF", "CHI", "CXL", "TileLink", "NVLink", "UALink"};
    for (const char* rel : {"include/tlm/cep/ssp_loader.hh", "include/tlm/cep/ssp_interpreter.hh"}) {
        const std::string src = read_src(rel);
        REQUIRE_FALSE(src.empty());
        for (const char* p : proto) {
            INFO(rel << " 含协议名 " << p);
            REQUIRE(src.find(p) == std::string::npos);
        }
    }

    // ② 解释器**零瞬态态**（`INV-CEP-05-IC1`）:每步结果只承载稳定态名 + perm
    //    ⇒ 编译期零时序锁 + 运行期「目标态必为声明面里的态」双重保证
    const auto doc = load_ssps_doc();
    const auto r = ssp::interpret_ssp(doc, "mesif_ssp", "I",
                                       std::vector<std::string>{"Rd", "RdUnique"});
    REQUIRE(r.ok);
    REQUIRE(r.judgeable);
    for (const auto& st : r.steps) {
        INFO("step 目标态=" << st.target_state);
        REQUIRE_FALSE(st.target_state.empty());
        // 目标态必须是**声明面**里的稳定态（⛔ 不得是解释器私造的瞬态名）
        bool declared = false;
        for (const auto& s : doc["ssps"][0]["states"])
            if (s.get<std::string>() == st.target_state) declared = true;
        REQUIRE(declared);
    }
}

TEST_CASE("[cep][ssp][contract] C10 -> SKIP（组合器侧）",
          "[cep][ssp][contract]") {
    // C10 = 组合接缝检查项（`INV-CEP-07-IC7` + `INV-CEP-08-IC2`）⇒ 由 Ordering Point /
    // 跨域组合器承担。SSP 声明面**不定义**组合能力 ⇒ SKIP,⛔ 不冒充 PASS。
    const auto doc = load_ssps_doc();
    for (const auto& s : doc["ssps"]) {
        INFO("ssp=" << s.value("id", ""));
        REQUIRE_FALSE(s.contains("combination_capabilities"));
        REQUIRE_FALSE(s.contains("combination_contract"));
    }
    SUCCEED("C10 判 SKIP（组合器侧）—— SSP 声明面无组合接缝字段");
}
