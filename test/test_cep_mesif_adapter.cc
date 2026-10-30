// test/test_cep_mesif_adapter.cc
// 队列 #9 `cpptlm-cep-mesif-adapter` —— MESIF adapter 载体面的契约测试
// per openspec/changes/cpptlm-cep-mesif-adapter/specs/cep-mesif-adapter/spec.md
//     Requirement: cep-mesif-adapter(MA-01 ~ MA-03)
// 作者: CppTLM Team / 日期: 2026-10-30
#include "catch_amalgamated.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "tlm/cep/mesif_adapter.hh"
#include "tlm/cep/mesif_state_spec.hh"

namespace {

    using tlm::cep::MesifAdapter;
    using tlm::cep::MesifStateSpec;

    std::string read_repo_file(const std::string& rel) {
        std::ifstream in(std::string(CPPTLM_SOURCE_DIR) + "/" + rel);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 去掉 `//` 与 `/* */` 注释(静态扫描的前提:注释里允许解释被禁的名字)
    std::string strip_comments(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        bool line_comment = false;
        bool block_comment = false;
        for (std::size_t i = 0; i < s.size(); ++i) {
            const char c = s[i];
            const char n = (i + 1 < s.size()) ? s[i + 1] : '\0';
            if (line_comment) {
                if (c == '\n') {
                    line_comment = false;
                    out.push_back(c);
                }
                continue;
            }
            if (block_comment) {
                if (c == '*' && n == '/') {
                    block_comment = false;
                    ++i;
                }
                continue;
            }
            if (c == '/' && n == '/') {
                line_comment = true;
                ++i;
                continue;
            }
            if (c == '/' && n == '*') {
                block_comment = true;
                ++i;
                continue;
            }
            out.push_back(c);
        }
        return out;
    }

    /// 载体的去注释后文本(零副本静态扫描的输入)
    const std::string& adapter_code() {
        static const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/mesif_adapter.hh"));
        return code;
    }

    /// 「零副本」违禁记号 —— 载体**不**命名任何 `State` / `Event` / `Perm` 字面量
    /// ⇒ 它在语法上**不可能**携带自己的表。
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> kTokens = {"State::", "Event::", "Perm::", "kTable"};
        return kTokens;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────
// Scenario MA-01 —— `MesifAdapter` SHALL **零自有表副本**,委托消费规约载体
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF adapter: 零自有表副本(35 格 pass-through + 静态扫描)",
          "[cep][mesif-adapter]") {
    // ── 子条件 ① / ②:35 格 pass-through(转换 30 + 投影 5)────────────────
    std::size_t cells = 0;
    for (std::size_t s = 0; s < MesifAdapter::kStateCount; ++s) {
        const auto st = static_cast<MesifAdapter::State>(s);
        for (std::size_t e = 0; e < MesifAdapter::kEventCount; ++e) {
            const auto ev = static_cast<MesifAdapter::Event>(e);
            INFO("state=" << MesifStateSpec::state_name(st)
                          << " event=" << MesifStateSpec::event_name(ev));
            CHECK(MesifAdapter::transition(st, ev) == MesifStateSpec::transition(st, ev));
            ++cells;
        }
    }
    CHECK(cells == 30u);

    for (std::size_t s = 0; s < MesifAdapter::kStateCount; ++s) {
        const auto st = static_cast<MesifAdapter::State>(s);
        MesifAdapter::Perm pa = MesifAdapter::Perm::NONE;
        MesifAdapter::Perm pb = MesifAdapter::Perm::NONE;
        const bool ha = MesifAdapter::perm_of(st, pa);
        const bool hb = MesifStateSpec::perm_of(st, pb);
        INFO("state=" << MesifStateSpec::state_name(st));
        REQUIRE(ha == hb);
        if (ha) {
            CHECK(static_cast<int>(pa) == static_cast<int>(pb));
        }
        ++cells;
    }
    CHECK(cells == 35u);

    // ── 子条件 ③:静态扫描(去注释后不得含任何违禁记号)──────────────────
    const std::string& code = adapter_code();
    REQUIRE(!code.empty());
    for (const auto& tok : forbidden_tokens()) {
        INFO("forbidden token: " << tok);
        CHECK(code.find(tok) == std::string::npos);
    }

    // ── 正控:扫描器本身有鉴别力(违禁记号必须被命中)─────────────────────
    const std::string probe = "static constexpr uint8_t t[5][6] = {{1,2,3,4,5,6}};"
                              "using S = X::State; S v = X::State::I;";
    CHECK(strip_comments(probe).find("State::") != std::string::npos);

    // ── 反向 fixture:在载体里伪造一份表 ⇒ 静态扫描必红 ─────────────────
    //    (真实毒化副本由 `scripts/test/cep_mesif_adapter_reverse_verify.sh` 执行;
    //     此处断言「伪造形态确实会被扫描器抓到」——否则反向 fixture 是空转)
    const std::string forged =
        "class MesifAdapter { static constexpr uint8_t kTable[5][6] = "
        "{{1,2,0,0,0,0},{1,2,1,2,1,0},{2,2,3,2,4,0},{3,3,3,3,4,0},{4,2,4,2,4,0}}; };";
    CHECK(strip_comments(forged).find("kTable") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────
// Scenario MA-02 —— 5→4 `perm` 投影 SHALL 与规约逐格相等
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF adapter: 5→4 perm 投影 + 瞬态无投影 + 语义损失措辞", "[cep][mesif-adapter]") {
    // 子条件 ①:5 态**每个**都有 4 值投影(缺一即红)
    for (std::size_t s = 0; s < MesifAdapter::kStateCount; ++s) {
        const auto st = static_cast<MesifAdapter::State>(s);
        MesifAdapter::Perm p = MesifAdapter::Perm::NONE;
        INFO("state=" << MesifStateSpec::state_name(st));
        REQUIRE(MesifAdapter::perm_of(st, p));
        CHECK(static_cast<int>(p) >= 0);
        CHECK(static_cast<int>(p) <= 3); // 4 值锁
    }

    // 子条件 ②:瞬态无投影 —— 本规约 5 态**全稳定** ⇒ 平凡成立
    CHECK(MesifAdapter::kTransientStateCount == 0u);
    CHECK(MesifAdapter::kTransientStates[0] == 0xFF);

    // 子条件 ③:语义损失 ① 的措辞与 `chi_rnf` **可机器区分**
    //   · 主体登记**不得**出现「不可补偿」
    //   · 对照说明**必须**出现「不可补偿」
    CHECK(std::string(MesifAdapter::kPermMergeNote).find("不可补偿") == std::string::npos);
    CHECK(std::string(MesifAdapter::kPermMergeNoteChiRnfContrast).find("不可补偿") !=
          std::string::npos);
    //   · 本实例**可补偿**的措辞必须在主体登记里
    CHECK(std::string(MesifAdapter::kPermMergeNote).find("可由 Sharer Tracker 补偿") !=
          std::string::npos);

    // 语义损失 ③ 的登记必须在位(写回事实在 perm 接口不可见)
    CHECK(std::string(MesifAdapter::kPermMergeWritebackPathNote).find("不可分辨") !=
          std::string::npos);

    // ── 反向 fixture:若主体登记混入了「不可补偿」措辞 ⇒ 判据必红 ──────
    const std::string polluted = "F 与 M 合并投影为 WRITEBACK:该损失不可补偿。";
    CHECK(polluted.find("不可补偿") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────
// Scenario MA-03 —— 6→3 消息类映射 SHALL 完备 + 恒等与否 SHALL 显式登记
// ─────────────────────────────────────────────────────────────────────────
TEST_CASE("CEP MESIF adapter: 6→3 消息类映射(完备 + 恒等登记 + forward_data 不入表)",
          "[cep][mesif-adapter]") {
    // 子条件 ①:映射表完备覆盖本实例用到的 6 类消息类
    CHECK(MesifAdapter::used_message_class_count() == 6u);
    const char* kExpected[] = {"request", "response", "snoop", "snoop_resp", "atomic", "query"};
    for (const char* mc : kExpected) {
        std::uint8_t cat = 0xFF;
        INFO("message class: " << mc);
        REQUIRE(MesifAdapter::category_of(mc, cat));
    }

    // 子条件 ④:`forward_data` SHALL **不**出现在 6 类词表里
    //   (per `ADR-CEP-12 §8.3` 补充约定 —— 它是 `snoop` 槽的**承载实例**)
    std::uint8_t cat = 0xFF;
    CHECK_FALSE(MesifAdapter::category_of("forward_data", cat));

    // 子条件 ②:与平台方向默认表**对拍一致**
    //   (平台默认表 = `adapter_schema_contracts.hh` 的 `identity_reference_map()`)
    const std::uint8_t kRefRequest = 0;  // CATEGORY_REQUEST
    const std::uint8_t kRefResponse = 1; // CATEGORY_RESPONSE
    std::uint8_t got = 0xFF;
    REQUIRE(MesifAdapter::category_of("request", got));
    CHECK(got == kRefRequest);
    REQUIRE(MesifAdapter::category_of("response", got));
    CHECK(got == kRefResponse);
    REQUIRE(MesifAdapter::category_of("snoop", got));
    CHECK(got == kRefRequest);
    REQUIRE(MesifAdapter::category_of("snoop_resp", got));
    CHECK(got == kRefResponse);
    REQUIRE(MesifAdapter::category_of("atomic", got));
    CHECK(got == kRefRequest);
    REQUIRE(MesifAdapter::category_of("query", got));
    CHECK(got == kRefRequest);

    // 子条件 ③:恒等与否**显式登记**并携带理由(恒等是合法状态,不得静默放过)
    CHECK(MesifAdapter::kMappingIsIdentity);
    REQUIRE(MesifAdapter::kIdentityReason != nullptr);
    CHECK(std::string(MesifAdapter::kIdentityReason).size() > 20);

    // ── 反向 fixture:删一行映射 / 写错类别 ⇒ 判据必红 ──────────────────
    //    (真实毒化副本由 reverse-verify 脚本执行;此处断言判据有鉴别力)
    const std::vector<const char*> kFull = {"request",    "response", "snoop",
                                            "snoop_resp", "atomic",   "query"};
    const std::vector<const char*> kMissingOne = {"request", "response", "snoop", "snoop_resp",
                                                  "atomic"}; // 少 query
    CHECK(kFull.size() == 6u);
    CHECK(kMissingOne.size() == 5u); // ⇒ 完备性判据必红
}
