// test/cep_protocol_engine_contract.hh
// ProtocolEngine 端口面契约的**共享**毒化副本生成器
//
// 功能:为 `cpptlm-cep-cohif-carrier` 的 task 2.6 / 2.7 提供**可执行**的反向 fixture:
//       · `MatrixDropOne`  —— 把 10 流覆盖矩阵改成**缺 1 流** ⇒ 覆盖完整性断言必红
//       · `BehaviorDegrade` —— 把某流的 `immediate-complete` 语义改成 **no-op**
//                              ⇒ 该流的**行为**断言必红
// 作者: CppTLM Team / 日期: 2026-10-27
//
// ── 为什么是**编译期**判据(而非运行期)──────────────────────────────
// 10 流覆盖矩阵的「完整性」与「逐流行为」都是**类型层**不变量 ——
//   · 覆盖完整性 ⇒ `null_flow_matrix_is_complete()` 的 `constexpr` 求值
//   · 逐流行为   ⇒ `null_flow_matrix_behaviors_are_expected()` 的 `constexpr` 求值
// 二者都以 `static_assert` 落在 `include/tlm/cep/protocol_engine_base.hh` 内 ⇒
// 毒化副本**编译不过** = 判据红了。
// ⚠️ **编译期锁不得只以「编译红」充当 ctest 判据** —— 本文件只提供「毒化副本」,
//    真正的 ctest 判据是**调用方断言「编译确实失败了」**
//    (见 `scripts/test/cep_protocol_engine_reverse_verify.sh` 与 Phase 6 的用例)。
//
// ── fixture 的「理由正确性」纪律 ───────────────────────────────────
// 两个 fixture 都必须**因判据而红**,而不是因「代码恰好编不过」而红:
//   · `MatrixDropOne`   ⇒ 命中「矩阵大小 ≠ 10」这一条**完整性**断言
//   · `BehaviorDegrade` ⇒ 命中「逐流行为 ≠ 期望」这一条**行为**断言
// ⇒ 矩阵用 **C 数组 + 显式 size**(非 `std::array<T,10>`)—— 否则「少一个元素」
//    会因**初始化器个数不足**而编不过(理由不对 ⇒ 判据被「错误的原因」满足)。
#ifndef CEP_PROTOCOL_ENGINE_CONTRACT_HH
#define CEP_PROTOCOL_ENGINE_CONTRACT_HH

#include <regex>
#include <stdexcept>
#include <string>

namespace cpptlm {
    namespace cep {

        /// ProtocolEngine 端口面头文件(相对仓根)
        inline constexpr const char* kProtocolEngineHeader =
            "include/tlm/cep/protocol_engine_base.hh";

        /// 毒化种类(per tasks 2.6 / 2.7)
        enum class ProtocolEnginePoison {
            MatrixDropOne,    // 10 流覆盖矩阵缺 1 流(删掉 barrier 项)
            BehaviorDegrade,  // barrier 流的 immediate-complete 语义退化为 no-op
        };

        inline const char* protocol_engine_poison_name(ProtocolEnginePoison p) {
            switch (p) {
                case ProtocolEnginePoison::MatrixDropOne: return "matrix_drop_one";
                case ProtocolEnginePoison::BehaviorDegrade: return "behavior_degrade";
            }
            return "unknown";
        }

        namespace detail {

            inline void replace_first_pe(std::string& text, const std::regex& re,
                                         const std::string& replacement, const char* what) {
                std::smatch m;
                if (!std::regex_search(text, m, re))
                    throw std::runtime_error(std::string("毒化副本生成失败 —— 锚点未找到:") + what);
                text.replace(m.position(0), m.length(0), replacement);
            }

            /// 删除匹配 re 的**整行**(含行尾换行)
            inline void erase_line(std::string& text, const std::regex& re, const char* what) {
                std::smatch m;
                if (!std::regex_search(text, m, re))
                    throw std::runtime_error(std::string("毒化副本生成失败 —— 锚点未找到:") + what);
                const std::size_t start = m.position(0);
                const std::size_t end = text.find('\n', start);
                const std::size_t stop = (end == std::string::npos) ? text.size() : end + 1;
                text.erase(start, stop - start);
            }

        }  // namespace detail

        /// 生成毒化副本 —— 锚点缺失**抛异常**(不得静默返回原文 ⇒ 否则反向验证假绿)
        inline std::string poison_protocol_engine_base(const std::string& src,
                                                       ProtocolEnginePoison which) {
            std::string out = src;
            switch (which) {
                case ProtocolEnginePoison::MatrixDropOne:
                    // 删掉 barrier 那一项 ⇒ 矩阵 size 变 9 ⇒ 覆盖完整性断言红
                    detail::erase_line(out,
                                       std::regex(R"(\{\s*CanonicalFlow::Barrier\s*,[^\n]*\n)"),
                                       "矩阵中的 Barrier 项");
                    break;
                case ProtocolEnginePoison::BehaviorDegrade:
                    // barrier 的 immediate-complete → no-op ⇒ 逐流行为断言红
                    detail::replace_first_pe(
                        out,
                        std::regex(R"(\{\s*CanonicalFlow::Barrier\s*,\s*NullFlowBehavior::ImmediateComplete\s*\})"),
                        "{CanonicalFlow::Barrier, NullFlowBehavior::NoOp}", "Barrier 的行为项");
                    break;
            }
            return out;
        }

        /// 全部毒化种类(供脚本 / 用例遍历)
        inline std::vector<ProtocolEnginePoison> protocol_engine_all_poisons() {
            return {ProtocolEnginePoison::MatrixDropOne, ProtocolEnginePoison::BehaviorDegrade};
        }

    }  // namespace cep
}  // namespace cpptlm

#endif  // CEP_PROTOCOL_ENGINE_CONTRACT_HH
