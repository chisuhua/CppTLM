// test/cep_cohif_contract.hh
// CEP CohIf 载体契约的**共享**扫描器 + 反向 fixture 生成器
//
// 功能:把 `cpptlm-cep-cohif-carrier` 的 CC-01 / CC-02 判据实现为**可复用**的
//       纯函数(静态扫描)+ 毒化副本生成器,供两处消费:
//         ① `test/test_cep_cohif_carrier.cc`(Phase 6 的 Catch2 用例)
//         ② `scripts/test/cep_cohif_scan_probe.cc` + `cep_cohif_compile_probe.cc`
//            (Phase 1 的反向验证脚本 `cep_cohif_reverse_verify.sh`)
//       —— **同一份实现**,避免两份判据漂移。
// 作者: CppTLM Team / 日期: 2026-10-27
//
// ── 判据来源(per `openspec/changes/cpptlm-cep-cohif-carrier/specs/`)──
// · `CC-01 ①`:`flow_id` 域枚举**恰好 5 个**值
// · `CC-01 ②`:字段集**不含**任何网络层字段(`src_node`/`dst_node`/`flit_category`/`vc_id`)
// · `CC-01 ③`:`forward_data`/`back_invalidate`/`address_resolve`/`barrier`/
//             `snoop_completion_ack` **不可能**出现在 `flow_id` 域内
//             (⇒ 「对 stub 不可见」是**类型保证**,非运行期纪律)
//
// ── 扫描纪律(per 队列 #2 critic 7 轮提炼的「有牙」判据)──────────────
// ① 必须**去注释后**扫:否则头文件里解释「为何不含网络层字段」的中文散文会被喂进
//    扫描器(**假阳性**),而把禁用词写进注释即可绕过(**假阴性**)。
// ② 必须按**标识符**判定:子串匹配会把 `ARCHITECT` 误判为含 `CHI`。
// ③ **纯负向断言必须有正控**:正控见 `cohif_scan_self_check()`。
// ④ 扫描器自身**失效**必须报红(而非静默通过):域找不到 / 枚举项解析不了 ⇒ 违规。
#ifndef CEP_COHIF_CONTRACT_HH
#define CEP_COHIF_CONTRACT_HH

#include "cep_adr_parser.hh"

#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace cpptlm {
    namespace cep {

        // ── 契约常量(真源:ADR-CEP-02 §2 canonical 10 流表)─────────────

        /// `flow_id` 域的**期望**成员(恰好 5 值,顺序即源码顺序)
        inline const std::vector<std::string>& cohif_expected_flow_members() {
            static const std::vector<std::string> v = {"Request", "Completion", "Snoop", "SnoopResp",
                                                       "Writeback"};
            return v;
        }

        /// **禁止出现**的网络层字段(CC-01 ②)—— 归一化后比对
        inline const std::vector<std::string>& cohif_forbidden_network_fields() {
            static const std::vector<std::string> v = {"src_node", "dst_node", "flit_category",
                                                       "vc_id"};
            return v;
        }

        /// **不跨 Stub↔PE 边界**的 5 条流(CC-01 ③)—— 不得出现在 `flow_id` 域内
        inline const std::vector<std::string>& cohif_out_of_domain_flows() {
            static const std::vector<std::string> v = {"forward_data", "back_invalidate",
                                                       "address_resolve", "barrier",
                                                       "snoop_completion_ack"};
            return v;
        }

        // ── 扫描结果 ──────────────────────────────────────────────────

        struct CohIfScanResult {
            std::vector<std::string> violations;    // 非空 = 契约违反
            std::vector<std::string> flow_members;  // `flow_id` 域成员(源码顺序)

            bool ok() const { return violations.empty(); }

            std::string render() const {
                std::string s;
                for (const auto& v : violations)
                    s += "VIOLATION: " + v + "\n";
                s += "flow_members=" + std::to_string(flow_members.size()) +
                     " violations=" + std::to_string(violations.size());
                return s;
            }
        };

        /// 扫描 CohIf bundle 头文件**源码** —— CC-01 ①②③
        ///
        /// ⚠️ 输入应是**原始文件内容**(本函数内部自己去注释)。
        inline CohIfScanResult scan_cohif_bundle(const std::string& src) {
            CohIfScanResult res;
            const std::string code = strip_comments(src);

            // ── ① `flow_id` 域:恰好 5 值 ───────────────────────────────
            static const std::regex flow_re(
                R"(enum\s+class\s+CohIfFlowId\s*:\s*std::uint8_t\s*\{([^}]*)\})");
            std::smatch m;
            if (!std::regex_search(code, m, flow_re)) {
                // 扫描器失效**必须报红**(判据纪律 ④)—— 不得当作「0 个成员」放行
                res.violations.push_back(
                    "flow_id 域未找到(期望形态:enum class CohIfFlowId : std::uint8_t { … })");
            } else {
                const std::string body = m[1].str();
                size_t pos = 0;
                while (pos <= body.size()) {
                    const size_t comma = body.find(',', pos);
                    const std::string item =
                        body.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
                    pos = (comma == std::string::npos) ? body.size() + 1 : comma + 1;
                    // 取 '=' 前的标识符
                    static const std::regex item_re(R"(([A-Za-z_][A-Za-z0-9_]*)\s*(=|,|$))");
                    std::smatch im;
                    if (!std::regex_search(item, im, item_re)) {
                        if (item.find_first_not_of(" \t\r\n") != std::string::npos) {
                            res.violations.push_back("flow_id 域存在无法解析的枚举项: " + item);
                        }
                        continue;
                    }
                    res.flow_members.push_back(im[1].str());
                }
                const auto& expected = cohif_expected_flow_members();
                if (res.flow_members.size() != expected.size()) {
                    res.violations.push_back("flow_id 域大小 = " +
                                             std::to_string(res.flow_members.size()) + "(期望 " +
                                             std::to_string(expected.size()) + ")");
                } else if (res.flow_members != expected) {
                    res.violations.push_back("flow_id 域成员集与契约不符(期望 Request/Completion/"
                                             "Snoop/SnoopResp/Writeback)");
                }
            }

            // ── ② / ③:标识符级负向判定(归一化:小写 + 去 `_`)──────────
            const auto tokens = identifier_tokens(code);
            auto scan_forbidden = [&](const std::vector<std::string>& names,
                                      const char* kind) -> void {
                for (const auto& bad : names) {
                    const std::string want = normalize_identifier(bad);
                    for (const auto& t : tokens) {
                        if (normalize_identifier(t) == want) {
                            res.violations.push_back(std::string("出现") + kind + ":" + bad);
                            break;
                        }
                    }
                }
            };
            scan_forbidden(cohif_forbidden_network_fields(), "网络层字段");
            scan_forbidden(cohif_out_of_domain_flows(), "域外流名");

            return res;
        }

        // ── 反向 fixture:毒化副本生成器 ──────────────────────────────
        //
        // 4 种毒化各自对应一条判据的**失效形态**:
        //   · `FlowCount6`      ⇒ 破坏 CC-01 ①(域多 1 值)⇒ **静态扫描**必红
        //   · `NetworkField`    ⇒ 破坏 CC-01 ②(混入网络层字段)⇒ **静态扫描**必红
        //   · `WidthPlusOne`    ⇒ 破坏 CC-02 ③(宽度锁)⇒ **编译**必失败
        //   · `VirtualFunction` ⇒ 破坏 CC-02 ②(trivially copyable)⇒ **编译**必失败
        //
        // ⚠️ 生成器**不做**「找不到锚点就静默返回原文」—— 锚点缺失 = fixture 已失效,
        //    必须抛异常(否则反向验证会「假绿」)。

        enum class CohIfPoison {
            FlowCount6,
            NetworkField,
            WidthPlusOne,
            VirtualFunction,
        };

        inline const char* cohif_poison_name(CohIfPoison p) {
            switch (p) {
                case CohIfPoison::FlowCount6: return "flow_count_6";
                case CohIfPoison::NetworkField: return "network_field";
                case CohIfPoison::WidthPlusOne: return "width_plus_one";
                case CohIfPoison::VirtualFunction: return "virtual_function";
            }
            return "unknown";
        }

        namespace detail {

            inline void replace_first(std::string& text, const std::regex& re,
                                      const std::string& replacement, const char* what) {
                std::smatch m;
                if (!std::regex_search(text, m, re))
                    throw std::runtime_error(std::string("毒化副本生成失败 —— 锚点未找到:") + what);
                text.replace(m.position(0), m.length(0), replacement);
            }

        }  // namespace detail

        inline std::string poison_cohif_bundle(const std::string& src, CohIfPoison which) {
            std::string out = src;
            switch (which) {
                case CohIfPoison::FlowCount6:
                    detail::replace_first(out, std::regex(R"(\bWriteback\s*=\s*4\s*,)"),
                                          "Writeback  = 4,\n    Barrier    = 5,", "flow_id 末项");
                    break;
                case CohIfPoison::NetworkField:
                    detail::replace_first(out, std::regex(R"(ch_uint<64>\s+address\s*;)"),
                                          "ch_uint<64> address;\n    ch_uint<32> src_node;", "address 字段");
                    break;
                case CohIfPoison::WidthPlusOne:
                    detail::replace_first(out,
                                          std::regex(R"(ch_uint<64>\s+transaction_id\s*;)"),
                                          "ch_uint<65> transaction_id;", "transaction_id 字段");
                    break;
                case CohIfPoison::VirtualFunction:
                    detail::replace_first(out, std::regex(R"(CohIfBundle\(\)\s*=\s*default\s*;)"),
                                          "virtual void poison() {}\n    CohIfBundle() = default;",
                                          "CohIfBundle 默认构造");
                    break;
            }
            return out;
        }

        /// 全部毒化种类(供脚本 / 用例遍历)
        inline std::vector<CohIfPoison> cohif_all_poisons() {
            return {CohIfPoison::FlowCount6, CohIfPoison::NetworkField, CohIfPoison::WidthPlusOne,
                    CohIfPoison::VirtualFunction};
        }

        // ── CC-02 ③:`BUNDLE_FIELD_WIDTH` 覆盖表扫描 ────────────────────

        /// 一条宽度锁(`BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, <field>, <width>)`)
        struct CohIfWidthLock {
            std::string field;
            unsigned width;
        };

        /// `CohIfBundle` 的**期望**宽度锁表(12 字段,顺序即源码顺序)
        ///
        /// ⚠️ 这是 **CC-02 ③** 的判据真源 —— 「`BUNDLE_FIELD_WIDTH` 宏 SHALL 覆盖
        ///    `CohIfBundle` 的**全部**字段」⇒ 锁表**条数**与**字段集**都必须吻合。
        inline const std::vector<CohIfWidthLock>& cohif_expected_width_locks() {
            static const std::vector<CohIfWidthLock> v = {
                {"flow_id", 8},         {"transaction_id", 64}, {"address", 64}, {"data", 64},
                {"size", 8},            {"perm", 8},            {"snoop_type", 8},
                {"line_state", 8},      {"hit", 1},             {"dirty", 1},
                {"error", 1},           {"atomic", 1}};
            return v;
        }

        /// 扫描 `BUNDLE_FIELD_WIDTH(bundles::CohIfBundle, …)` 锁定表(CC-02 ③)
        ///
        /// ⚠️ 输入应是**原始文件内容**(注释里若出现该宏,不应被计入 ⇒ 内部去注释)。
        inline std::vector<CohIfWidthLock> scan_cohif_width_locks(const std::string& src) {
            std::vector<CohIfWidthLock> out;
            const std::string code = strip_comments(src);
            static const std::regex lock_re(
                R"(BUNDLE_FIELD_WIDTH\s*\(\s*bundles::CohIfBundle\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([0-9]+)\s*\))");
            for (std::sregex_iterator it(code.begin(), code.end(), lock_re), end; it != end; ++it) {
                out.push_back({(*it)[1].str(), static_cast<unsigned>(std::stoul((*it)[2].str()))});
            }
            return out;
        }

        /// 扫描器**正控 / 反控**自检 —— 证明扫描逻辑本身有判别力
        ///
        /// 返回违反项(应为空)。任一违反 ⇒ 扫描器退化(假绿)⇒ 调用方必须判红。
        inline std::vector<std::string> cohif_scan_self_check() {
            std::vector<std::string> bad;
            // 正控:确实存在的违规**必须**被抓到
            if (scan_cohif_bundle("enum class CohIfFlowId : std::uint8_t { A = 0 };")
                    .violations.empty())
                bad.push_back("正控失效:域大小 1 未被报出");
            if (scan_cohif_bundle("struct S { ch_uint<32> src_node; };").violations.empty())
                bad.push_back("正控失效:网络层字段未被报出");
            if (scan_cohif_bundle("enum class X { barrier = 0 };").violations.empty())
                bad.push_back("正控失效:域外流名未被报出");
            // 反控:合法形态**不得**被报出
            {
                std::string good =
                    "enum class CohIfFlowId : std::uint8_t { Request = 0, Completion = 1, Snoop = 2,"
                    " SnoopResp = 3, Writeback = 4 };";
                const auto r = scan_cohif_bundle(good);
                if (!r.violations.empty())
                    bad.push_back("反控失效:合法形态被误报:" + r.render());
            }
            return bad;
        }

    }  // namespace cep
}  // namespace cpptlm

#endif  // CEP_COHIF_CONTRACT_HH
