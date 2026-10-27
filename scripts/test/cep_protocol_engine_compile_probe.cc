// scripts/test/cep_protocol_engine_compile_probe.cc
// ProtocolEngine 端口面**编译期锁**探针 —— 被编译的 TU 必须因毒化副本而编译失败
//
// 用途(per `cpptlm-cep-cohif-carrier` task 2.6 / 2.7 的**反向 fixture**):
//   · 「10 流覆盖矩阵缺 1 流」的毒化副本 ⇒ **覆盖完整性** `static_assert` 触发
//   · 「某流语义退化为 no-op」的毒化副本 ⇒ **逐流行为** `static_assert` 触发
//
// ⚠️ **编译期锁不得只以「编译红」充当 ctest 判据** —— 本探针只提供「编译会红」这
//    一事实;真正的 ctest 判据是**调用方断言「编译确实失败了」**
//    (见 `scripts/test/cep_protocol_engine_reverse_verify.sh` 与 Phase 6 的用例)。
//    ⇒ 若有人把两条 `static_assert` 删掉,本探针会**编译成功** ⇒ 调用方必须判红
//      ⇒ 回归被钉住。
//
// 编译:`g++ -std=c++17 -fsyntax-only -I <include 路径> <probe.cc>`
// 运行(生成毒化副本):
//   `probe --poison <kind> <in 路径> <out 路径>`
//     kind ∈ {matrix_drop_one, behavior_degrade}
#include "tlm/cep/protocol_engine_base.hh"

#include "cep_protocol_engine_contract.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>

namespace {

    cpptlm::cep::ProtocolEnginePoison parse_poison(const std::string& name) {
        using P = cpptlm::cep::ProtocolEnginePoison;
        if (name == "matrix_drop_one") return P::MatrixDropOne;
        if (name == "behavior_degrade") return P::BehaviorDegrade;
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

// 覆盖完整性 + 逐流行为两条锁的**显式再断言**
// (头文件内已有 `static_assert`;此处再断言一次 ⇒ 头内锁被删时本 TU 也红)
static_assert(tlm::cep::null_flow_matrix_is_complete(),
              "CC/INV-CEP-07-IC2: 10 流覆盖矩阵必须恰好覆盖 canonical 10 流");
static_assert(tlm::cep::null_flow_matrix_behaviors_are_expected(),
              "INV-CEP-07-IC2: 逐流行为必须与 07 号 §6.4 口径一致");

// canonical 10 流名与 schema `flow_support` 的 10 个 required key 的一致性
// (编译期可查的部分:名表大小与 10 个流枚举一一对应)
static_assert(tlm::cep::kCanonicalFlowCount == 10, "canonical 10 流");
static_assert(tlm::cep::kNullFlowMatrixSize == 10, "Null 覆盖矩阵 10 项");

// 端口面基类的类型不变量
static_assert(tlm::cep::kCohIfFlowCount == 5, "CohIf 承载 5 条 RN-side 流");

int main(int argc, char** argv) {
    // ── 模式 A:生成毒化副本 ──────────────────────────────────────
    if (argc >= 2 && std::string(argv[1]) == "--poison") {
        if (argc < 5) {
            std::cerr << "usage: cep_protocol_engine_compile_probe --poison <kind> <in> <out>\n";
            return 2;
        }
        try {
            const auto kind = parse_poison(argv[2]);
            const auto poisoned =
                cpptlm::cep::poison_protocol_engine_base(slurp(argv[3]), kind);
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
    return 0;
}
