// scripts/test/cep_cohif_scan_probe.cc
// CohIf 载体契约**静态扫描**探针 —— 发现违反即以非 0 退出码结束
//
// 用途(per `cpptlm-cep-cohif-carrier` task 1.5 / 1.6 的**反向 fixture**):
//   把 `include/bundles/cep_coherence_bundles_tlm.hh` 的**毒化副本**喂给本探针
//   ⇒ 必须报红(退出码非 0);喂干净版本 ⇒ 必须报绿(退出码 0)。
//   ⇒ 「静态扫描用例必红」这条判据**有牙**,而非「声称有自动化、实则没有」。
//
// ⚠️ 本探针**不** include 被扫描的头文件 ——
//   ① 宽度 / 虚函数类毒化会让头文件**编译失败**,scan 逻辑必须仍能独立运行;
//   ② 扫描器自身若 include 目标头,会把目标的标识符喂进自己的禁用词集合。
//
// 编译:`g++ -std=c++17 -I <repo>/include -I <repo>/test -o probe <probe.cc>`
// 运行:
//   `probe <header 路径>`                                  —— 扫描该头文件
//   `probe --poison <kind> <in 路径> <out 路径>`            —— 生成毒化副本
//     kind ∈ {flow_count_6, network_field, width_plus_one, virtual_function}
#include "cep_cohif_contract.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

    cpptlm::cep::CohIfPoison parse_poison(const std::string& name) {
        using P = cpptlm::cep::CohIfPoison;
        if (name == "flow_count_6") return P::FlowCount6;
        if (name == "network_field") return P::NetworkField;
        if (name == "width_plus_one") return P::WidthPlusOne;
        if (name == "virtual_function") return P::VirtualFunction;
        throw std::runtime_error("unknown poison kind: " + name);
    }

    std::string slurp(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            throw std::runtime_error("cannot open " + path);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

}  // namespace

int main(int argc, char** argv) {
    // ── 模式 A:生成毒化副本 ──────────────────────────────────────
    if (argc >= 2 && std::string(argv[1]) == "--poison") {
        if (argc < 5) {
            std::cerr << "usage: cep_cohif_scan_probe --poison <kind> <in> <out>\n";
            return 2;
        }
        try {
            const auto kind = parse_poison(argv[2]);
            const auto poisoned =
                cpptlm::cep::poison_cohif_bundle(slurp(argv[3]), kind);
            std::ofstream out(argv[4], std::ios::binary | std::ios::trunc);
            if (!out) {
                std::cerr << "cannot write " << argv[4] << "\n";
                return 2;
            }
            out << poisoned;
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "poison generation failed: " << e.what() << "\n";
            return 2;
        }
    }

    // ── 模式 B:扫描 ──────────────────────────────────────────────
    if (argc < 2) {
        std::cerr << "usage: cep_cohif_scan_probe <header-path>\n";
        return 2;
    }
    std::string src;
    try {
        src = slurp(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 2;
    }

    // 扫描器自检(正控 / 反控)—— 扫描逻辑退化时必须报红,不得静默放行
    const auto self_bad = cpptlm::cep::cohif_scan_self_check();
    for (const auto& b : self_bad)
        std::cout << "SCANNER-SELFTEST-FAIL: " << b << "\n";

    const auto res = cpptlm::cep::scan_cohif_bundle(src);
    std::cout << res.render() << "\n";

    if (!self_bad.empty())
        return 1;  // 扫描器自身失效 ⇒ 判红(不得「假绿」)
    return res.ok() ? 0 : 1;
}
