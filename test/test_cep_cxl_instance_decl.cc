// test/test_cep_cxl_instance_decl.cc
//   —— `cxl_2_0` 实例声明面的契约测试
//
// 作者: CppTLM Team / 日期: 2026-11-01
// 配套: include/tlm/cep/protocol_instances.json · include/tlm/cep/perm_upgrades.json
//       include/tlm/cep/cxl_cache_adapter.hh · include/tlm/cep/cxl_state_spec.hh
//       openspec/changes/cpptlm-cep-cxl-nonsymmetric/specs/cep-cxl-nonsymmetric/spec.md
//       台账 obj-cep-platform.md Entry #122(口径)· #128(实现层转正)
//
// ── 判据纪律 ──────────────────────────────────────────────────────────────
// 纯负向断言 SHALL 有正控;每条判据 SHALL 有可执行反向 fixture 且**因正确原因红**。

#include "catch_amalgamated.hpp"

#include "tlm/cep/cxl_cache_adapter.hh"
#include "tlm/cep/cxl_state_spec.hh"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    /// 极简 JSON 取值:找 `"key": "value"` 或 `"key": <number|true|false>` 的**首次**出现。
    /// ⚠️ 本仓的声明文件是**本仓权威**(per `ADR-CEP-12 §11` 的实例级权威分区)⇒
    ///    本测试**不**依赖外部 JSON 库,只做**定点取值**。
    std::string json_scalar_after(const std::string& doc, const std::string& key) {
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = doc.find(needle);
        if (pos == std::string::npos)
            return {};
        pos = doc.find(':', pos + needle.size());
        if (pos == std::string::npos)
            return {};
        ++pos;
        while (pos < doc.size() && (doc[pos] == ' ' || doc[pos] == '\t' || doc[pos] == '\n'))
            ++pos;
        if (pos >= doc.size())
            return {};
        if (doc[pos] == '"') {
            const std::size_t e = doc.find('"', pos + 1);
            return (e == std::string::npos) ? std::string() : doc.substr(pos + 1, e - pos - 1);
        }
        std::size_t e = pos;
        while (e < doc.size() && doc[e] != ',' && doc[e] != '}' && doc[e] != '\n')
            ++e;
        std::string v = doc.substr(pos, e - pos);
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r'))
            v.pop_back();
        return v;
    }

    /// 本仓根
    std::string repo_root() {
#ifdef CPPTLM_SOURCE_DIR
        return std::string(CPPTLM_SOURCE_DIR);
#else
        return std::string(".");
#endif
    }

    std::string read_file(const std::string& rel) {
        std::ifstream in(repo_root() + "/" + rel, std::ios::binary);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// `cxl_2_0` 实例块原文(从声明文件里切出来)
    std::string cxl_instance_block(const std::string& doc) {
        const std::size_t start = doc.find("\"id\": \"cxl_2_0\"");
        if (start == std::string::npos)
            return {};
        const std::size_t id_open = doc.rfind('{', start);
        // 找匹配的右括号(实例块内无嵌套数组以外的复杂结构,但仍做**深度**计数)
        int depth = 0;
        for (std::size_t i = id_open; i < doc.size(); ++i) {
            if (doc[i] == '{')
                ++depth;
            else if (doc[i] == '}') {
                --depth;
                if (depth == 0)
                    return doc.substr(id_open, i - id_open + 1);
            }
        }
        return {};
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// CNA-03 —— CXL 实例声明 SHALL 登记,且 `sharer_tracker` SHALL 为「不参与」
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL 实例声明:13 必填键 + 不参与 tracker + 权威分区", "[cep][cxl][instance-decl]") {
    const std::string decl = read_file("include/tlm/cep/protocol_instances.json");
    REQUIRE(!decl.empty());
    const std::string blk = cxl_instance_block(decl);
    REQUIRE(!blk.empty());

    // ── 子条件 ①:13 个 required 键 SHALL 齐备(schema 驱动)───────────────
    for (const char* key : {"id", "family", "adapter", "status", "protocol_version", "flow_support",
                            "perm", "atomic", "consistency_labels", "ordering_assumption",
                            "vc_binding", "sharer_tracker", "combination_capabilities"}) {
        INFO("required key: " << key);
        REQUIRE(blk.find(std::string("\"") + key + "\"") != std::string::npos);
    }

    // ── 子条件 ②:`sharer_tracker.impl` SHALL 为「不参与」──────────────────
    //    (CXL.cache 的 peer 集**平凡为空** ⇒ 挂 SF 是**过度声明**)
    const std::size_t st_pos = blk.find("\"sharer_tracker\"");
    REQUIRE(st_pos != std::string::npos);
    const std::string st_blk = blk.substr(st_pos);
    REQUIRE(json_scalar_after(st_blk, "impl") == "不参与");
    // caps 与「不参与」先例同形(照 `chi_rnf`)
    REQUIRE(json_scalar_after(st_blk, "scope") == "NONE");
    REQUIRE(json_scalar_after(st_blk, "aggregation_at") == "NONE");

    // ── 子条件 ③:`cxl_2_0` SHALL 登记进 `repo_owned_instance_ids` ──────────
    //    (口径 ⑥:不镜像进 ArchForge `32 号 §6.1` ⇒ 本仓自有)
    REQUIRE(decl.find("\"repo_owned_instance_ids\"") != std::string::npos);
    REQUIRE(decl.find("\"cxl_2_0\"", decl.find("\"repo_owned_instance_ids\"")) !=
            std::string::npos);

    // ── 标识与 adapter 逐字一致(零漂移)───────────────────────────────────
    REQUIRE(json_scalar_after(blk, "id") == "cxl_2_0");
    REQUIRE(json_scalar_after(blk, "adapter") == "CxlCacheAdapter");
    REQUIRE(std::string(tlm::cep::CxlCacheAdapter::kInstanceId) == json_scalar_after(blk, "id"));
    REQUIRE(std::string(tlm::cep::CxlCacheAdapter::kAdapterName) ==
            json_scalar_after(blk, "adapter"));

    // ── `protocol_version` SHALL 与 `ADR-CEP-11 §3` 锁表的 `cxl-2.0` 一致 ──
    REQUIRE(json_scalar_after(blk, "protocol_version") == "cxl-2.0");

    // ── `perm` 恰 4 值(per `ADR-CEP-02` CEN-Ib11)──────────────────────────
    const std::size_t p_pos = blk.find("\"perm\"");
    REQUIRE(p_pos != std::string::npos);
    const std::string p_blk = blk.substr(p_pos, blk.find(']', p_pos) - p_pos);
    REQUIRE(p_blk.find("\"NONE\"") != std::string::npos);
    REQUIRE(p_blk.find("\"READ\"") != std::string::npos);
    REQUIRE(p_blk.find("\"UNIQUE\"") != std::string::npos);
    REQUIRE(p_blk.find("\"WRITEBACK\"") != std::string::npos);

    // ── `combination_capabilities.hierarchical` SHALL 为 true ─────────────
    //    ⚠️ **语义域**(实测踩坑,per Entry #131):该字段是**平台组合维度**
    //    (「本实例**能否**参与分层组合」),**不是**「本协议自身的层级结构」。
    //    口径 ③ 的「单一一致性点」说的是**后者** ⇒ 二者**不冲突**:
    //    CXL.cache 的设备侧是**叶节点**(peer 集平凡为空)⇒ 作为叶参与分层组合**成立**
    //    ⇒ 填 `false` 会撞 `INV-CEP-08-IC2`(声明 UNIQUE + 静默升级的实例
    //    **必须**声明 hierarchical 组合能力,否则全局 SWMR 可能被破坏)。
    REQUIRE(json_scalar_after(blk, "hierarchical") == "true");

    // ── 反向 fixture:误挂 SF ⇒ 必红(过度声明)─────────────────────────────
    {
        std::string forged = blk;
        const std::size_t ip = forged.find("\"impl\": \"不参与\"");
        REQUIRE(ip != std::string::npos);
        forged.replace(ip, std::string("\"impl\": \"不参与\"").size(),
                       "\"impl\": \"SparseSnoopFilter\"");
        REQUIRE(json_scalar_after(forged.substr(forged.find("\"sharer_tracker\"")), "impl") !=
                "不参与");
    }
}

TEST_CASE("CEP CXL 实例声明:静默升级登记 + 四实例齐备", "[cep][cxl][instance-decl]") {
    const std::string pu = read_file("include/tlm/cep/perm_upgrades.json");
    REQUIRE(!pu.empty());

    // `cxl_2_0` 的 UNIQUE → WRITEBACK 一行
    const std::size_t pos = pu.find("\"cxl_2_0\"");
    REQUIRE(pos != std::string::npos);
    const std::string row = pu.substr(pos, 120);
    REQUIRE(row.find("\"from\": \"UNIQUE\"") != std::string::npos);
    REQUIRE(row.find("\"to\": \"WRITEBACK\"") != std::string::npos);

    // 4 个实例齐备(moesi_gpu / chi_rnf / mesif / cxl_2_0)
    for (const char* id : {"moesi_gpu", "chi_rnf", "mesif", "cxl_2_0"}) {
        INFO("silent upgrade instance: " << id);
        REQUIRE(pu.find(std::string("\"") + id + "\"") != std::string::npos);
    }

    // ⚠️ 该格的 basis 是 `analogical` + `pending` ⇒ 散文**必须**登记该caveat
    REQUIRE(pu.find("public-analogical") != std::string::npos);
    REQUIRE(pu.find("pending") != std::string::npos);

    // 正控:`cxl_2_0` 的静默升级判定与规约载体逐格一致
    REQUIRE(tlm::cep::CxlCacheAdapter::is_silent_upgrade(tlm::cep::CxlCacheAdapter::State::E,
                                                         tlm::cep::CxlCacheAdapter::Event::ItoMWr));
}
