// probe_oldnew.cc —— C2 并排取证的**版本无关探针**(per metis P0-2)
//
// ⛔ **本文件 MUST NOT 引用 `per_network_orderings`** —— 旧 HEAD(`0ceb0860`)的
//    `VcBindingSnapshot` **没有**该字段 ⇒ 引用则旧 header **编译不过** ⇒
//    取证会退化为「同实现内自证」。
//
// ✅ **只允许调用两个 API**(两者在旧/新版本**签名完全相同**):
//    ① `cep::load_vc_binding_snapshot(影子 protocol_instances, 真实 fabric)`
//    ② `cep::evaluate_shared_mode_certificate(decl, snap, untested, detail)`
//
// 用法:`probe_oldnew <repo_root> <protocol_instances> <declaration>`
//   ⚠️ **两个输入显式给出**(⛔ 不做路径拼接 —— 拼接会在「只毒化一侧」的场景下拼错):
//     · AC-3:  <protocol_instances> = 影子(毒化 ordering) · <declaration> = 仓内真实
//     · AC-12: <protocol_instances> = 仓内真实 · <declaration> = 影子(毒化 plane_binding)
//
// 输出(机器可解析,供并排文档引用):
//   VERDICT=<PASS|FAIL|SKIP>
//   DETAIL=<detail 文本>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "tlm/cep/collective_plane.hh"
#include "tlm/cep/collective_plane_contract.hh"

namespace cep = tlm::cep;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const char* verdict_name(cep::ContractVerdict v) {
    switch (v) {
    case cep::ContractVerdict::PASS:
        return "PASS";
    case cep::ContractVerdict::FAIL:
        return "FAIL";
    case cep::ContractVerdict::SKIP:
        return "SKIP";
    }
    return "?";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: probe_oldnew <repo_root> <protocol_instances> <declaration>\n";
        return 2;
    }
    const std::string root = argv[1];
    const std::string proto_path = argv[2];
    const std::string decl_path = argv[3];

    const std::string decl_text = read_file(decl_path);
    if (decl_text.empty()) {
        std::cerr << "cannot read declaration: " << decl_path << "\n";
        return 3;
    }
    const auto decl = cep::parse_collective_plane_declaration(nlohmann::json::parse(decl_text));

    // ① 唯一允许的 loader 调用 —— fabric 始终用**真实**文件
    const auto snap = cep::load_vc_binding_snapshot(proto_path,
                                                    root + "/include/tlm/cep/fabric.json");
    if (snap.source_error) {
        std::cout << "SOURCE_ERROR=" << snap.source_error_detail << "\n";
        return 4;
    }

    // ② 唯一允许的判定调用 —— 签名在旧/新版本完全相同
    std::string detail;
    const auto v = cep::evaluate_shared_mode_certificate(decl, snap,
                                                         cep::default_untested_registry(), detail);
    std::cout << "VERDICT=" << verdict_name(v) << "\n";
    std::cout << "DETAIL=" << detail << "\n";
    return 0;
}
