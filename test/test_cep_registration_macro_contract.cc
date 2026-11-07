// test/test_cep_registration_macro_contract.cc
// `INV-CEP-07-IC4` —— CEP 模块注册走既有宏体系(`REGISTER_CHSTREAM` / `REGISTER_MODULE` /
//   `REGISTER_OBJECT`),**不发明宏形式**
//
// 判据来源:per `ADR-CEP-07 §2 决策 3`(「CEP 模块注册走**真实注册机制**,不发明宏形式」)
//   + §3 不变量 IC4
//
// 机器化口径:
//   ① **静态**:`include/chstream_register.hh` 里每个 `tlm::cep::` 类型的注册点,
//      其最近的注册形态**必须**是既有宏族 / 既有扩展钩子;
//   ② **反向**:CEP 头(`include/tlm/cep/*.hh`)内**不得**定义自己的注册宏 / 注册函数;
//   ③ **运行时**:注册**实际生效**(adapter 工厂能查到该短名且端口数正确)
//      —— 只「写了宏」而注册没生效 = 该 invariant 的实质失败。
//
// ⚠️ 变红条件(不是「ADR 里写着这句话」):
//   · 新注册点用了自造宏(如 `REGISTER_CEP_PLUGIN`)⇒ ① 红
//   · CEP 头里出现 `#define REGISTER_*` / 自造注册函数 ⇒ ② 红
//   · 注册未生效(工厂查不到)⇒ ③ 红
//
// 作者: CppTLM Team / 日期: 2026-11-05
#include "catch_amalgamated.hpp"

#include "cep_adr_parser.hh"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "chstream_register.hh"
#include "framework/chstream_adapter_factory.hh"

using cpptlm::cep::read_file;
using cpptlm::cep::repo_root;

namespace {

    // 既有注册形态白名单(per IC4 原文 + §2 决策 3 的既有先例)
    //   · `REGISTER_CHSTREAM` 覆盖 `_BIDIRECTIONAL` / `_BIDIRECTIONAL_NAMED` / `_DUAL` 变体
    //   · `REGISTER_MODULE` / `REGISTER_OBJECT` —— SimModule / SimObject 两条既有路径
    //   · `ModuleFactory::register_protocol_bridge` —— 既有协议桥扩展钩子(ADR-CEP-19 决策 5)
    bool is_whitelisted_form(const std::string& line) {
        static const char* kWhitelist[] = {"REGISTER_CHSTREAM", "REGISTER_MODULE",
                                           "REGISTER_OBJECT", "ModuleFactory::registerObject",
                                           "ModuleFactory::register_protocol_bridge"};
        for (const char* w : kWhitelist) {
            if (line.find(w) != std::string::npos)
                return true;
        }
        return false;
    }

    // 该行是否是「注册形态」行(既有或自造都算)
    bool looks_like_registration(const std::string& line) {
        return line.find("REGISTER_") != std::string::npos ||
               line.find("ModuleFactory::register") != std::string::npos;
    }

    struct ScanResult {
        bool ok = true;
        int refs = 0;
        std::string reason;
    };

    // ── 判定:每个 `tlm::cep::` 引用,其**最近的注册形态行**必须白名单化 ──
    ScanResult scan_cep_registration_sites(const std::vector<std::string>& lines,
                                           int lookback = 12) {
        static const std::string kNeedle = "tlm::cep::";
        ScanResult r;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].find(kNeedle) == std::string::npos)
                continue;
            ++r.refs;
            // 向前找最近的注册形态行(注册表达式可跨行,故给 12 行回看窗口)
            int found = -1;
            const int lo = std::max<int>(0, static_cast<int>(i) - lookback);
            for (int j = static_cast<int>(i); j >= lo; --j) {
                if (looks_like_registration(lines[static_cast<std::size_t>(j)])) {
                    found = j;
                    break;
                }
            }
            if (found < 0) {
                r.ok = false;
                r.reason = "第 " + std::to_string(i + 1) +
                           " 行的 tlm::cep:: 类型找不到注册形态行(可能经自造路径注册)";
                return r;
            }
            if (!is_whitelisted_form(lines[static_cast<std::size_t>(found)])) {
                r.ok = false;
                r.reason = "第 " + std::to_string(i + 1) + " 行的注册形态 **不是**既有宏体系: " +
                           lines[static_cast<std::size_t>(found)].substr(0, 80);
                return r;
            }
        }
        return r;
    }

    std::vector<std::string> split_lines(const std::string& text) {
        std::vector<std::string> out;
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line))
            out.push_back(line);
        return out;
    }

    // ── 判定:CEP 头内不得自造注册宏 / 注册函数 ─────────────────────────
    struct BespokeScan {
        bool ok = true;
        std::string where;
    };

    BespokeScan scan_bespoke_registration(const std::string& text) {
        for (const auto& line : split_lines(text)) {
            // 自造注册宏
            if (line.find("#define REGISTER_") != std::string::npos)
                return {false, "自造注册宏: " + line.substr(0, 80)};
            // 自造注册函数(CEP 侧直接调工厂注册 ⇒ 绕过既有宏体系)
            if (line.find("register_chstream_") != std::string::npos ||
                line.find("ModuleFactory::registerObject") != std::string::npos)
                return {false, "CEP 头内直接调工厂注册: " + line.substr(0, 80)};
        }
        return {};
    }

} // namespace

// INV-CARRIER: INV-CEP-07-IC4
// 变红理由(per ADR-CEP-06 SU#4 规则 B):CEP 注册点改用自造宏 / CEP 头自造注册路径 /
// 注册未生效(工厂查不到)⇒ 红
TEST_CASE("CEP Registration: IC4 —— CEP 模块注册走既有宏体系,不发明宏形式", "[cep][registration]") {
    // ── ① 静态:注册表里每个 tlm::cep:: 类型都经既有宏族 / 既有钩子注册 ──────
    const auto reg = read_file((repo_root() / "include" / "chstream_register.hh").string());
    REQUIRE(!reg.empty());
    const auto reg_scan = scan_cep_registration_sites(split_lines(reg));
    INFO("注册点不合规: " << reg_scan.reason);
    REQUIRE(reg_scan.ok);
    // 至少有 4 个 CEP 注册点(3 引擎 + 1 协议桥)—— 否则断言恒真
    REQUIRE(reg_scan.refs >= 4);

    // ── ② 反向:CEP 头内不得定义注册宏 / 直接调工厂注册 ──────────────────
    namespace fs = std::filesystem;
    const auto cep_dir = repo_root() / "include" / "tlm" / "cep";
    REQUIRE(fs::exists(cep_dir));
    std::size_t scanned = 0;
    for (const auto& e : fs::directory_iterator(cep_dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".hh")
            continue;
        ++scanned;
        const auto text = read_file(e.path().string());
        const auto b = scan_bespoke_registration(text);
        INFO("CEP 头自造注册路径: " << e.path().filename().string() << " ⇒ " << b.where);
        REQUIRE(b.ok);
    }
    REQUIRE(scanned > 10u); // 扫到的 CEP 头数量下界(防止目录读空导致恒真)

    // ── ③ 运行时:注册**实际生效**(adapter 工厂能查到短名且端口数正确)──────
    //     ⚠️ 需要先跑一次注册宏(与生产装配同一宏,不是测试专用路径)
    static const bool kRegistered = []() {
        REGISTER_CHSTREAM;
        return true;
    }();
    REQUIRE(kRegistered);
    // ⚠️ 引擎名清单 = **四元组**(2026-11-06 新增 `TileLinkProtocolEngine`,per 台账 Entry #243
    //    / change `cpptlm-cep-tilelink-runtime-engine` 的 P0-1 注册名连锁)——
    //    ⛔ 新增引擎名时**必须**同步本清单,否则既有断言会「因正确原因红」(注册未生效)。
    for (const char* name : {"CohIfProtocolEngine", "ChiRnfProtocolEngine", "MesifProtocolEngine",
                             "TileLinkProtocolEngine"}) {
        INFO("注册未生效(adapter 工厂查不到): " << name);
        REQUIRE(ChStreamAdapterFactory::get().isMultiPort(name));
        INFO("端口数错误: " << name);
        REQUIRE(ChStreamAdapterFactory::get().getPortCount(name) == 2u);
    }

    // ── ④ 反向 fixture(判别力证据)────────────────────────────────────────
    // ① 自造宏注册 ⇒ 必须判否
    {
        const std::vector<std::string> bad = {
            "REGISTER_CEP_PLUGIN(\"FooEngine\", tlm::cep::FooEngine, bundles::CohIfBundle, 2);"};
        const auto s = scan_cep_registration_sites(bad);
        INFO("反向 fixture 应判否,实际: " << s.reason);
        REQUIRE_FALSE(s.ok);
    }
    // ② 无任何注册形态行 ⇒ 必须判否
    {
        const std::vector<std::string> bad = {"auto* p = new tlm::cep::FooEngine();"};
        const auto s = scan_cep_registration_sites(bad);
        REQUIRE_FALSE(s.ok);
    }
    // ③ 合法形态 ⇒ 必须判否为「通过」(证明正向路径非恒假)
    {
        const std::vector<std::string> good = {
            "REGISTER_CHSTREAM_BIDIRECTIONAL_NAMED(\"FooEngine\",",
            "                                       tlm::cep::FooEngine,",
            "                                       bundles::CohIfBundle, 2);"};
        const auto s = scan_cep_registration_sites(good);
        INFO("正向 fixture 应通过,实际: " << s.reason);
        REQUIRE(s.ok);
        REQUIRE(s.refs == 1);
    }
    // ④ CEP 头自造注册宏 ⇒ 必须判否
    {
        const auto b = scan_bespoke_registration("#define REGISTER_CEP_PLUGIN(t, b, n) \\\n");
        REQUIRE_FALSE(b.ok);
    }
    // ⑤ CEP 头直接调工厂注册 ⇒ 必须判否
    {
        const auto b = scan_bespoke_registration(
            "    ModuleFactory::registerObject<FooEngine>(\"FooEngine\");");
        REQUIRE_FALSE(b.ok);
    }
}
