// test/test_cep_cxl_cache_adapter.cc
//   —— `CxlCacheAdapter` 的契约测试 + 零副本扫描 + 反向 fixture
//
// 作者: CppTLM Team / 日期: 2026-11-01
// 配套: include/tlm/cep/cxl_cache_adapter.hh · include/tlm/cep/cxl_state_spec.hh
//       openspec/changes/cpptlm-cep-cxl-nonsymmetric/specs/cep-cxl-nonsymmetric/spec.md
//       台账 obj-cep-platform.md Entry #122(口径)· #127(规约层归档)· #128(实现层转正)
//
// ── 判据纪律(per 本仓 4 条铁律)────────────────────────────────────────────
// ① 纯负向断言 SHALL 有正控;② 每条判据 SHALL 有可执行反向 fixture 且**因正确原因红**;
// ③ 编译期锁 SHALL **不**只以「编译红」充当 ctest 判据;④ fixture 状态变迁后 SHALL 重新基线化。

#include "catch_amalgamated.hpp"

#include "tlm/cep/cxl_cache_adapter.hh"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

    using tlm::cep::CxlCacheAdapter;
    using tlm::cep::CxlStateSpec;

    // 类型别名取自 **adapter**(它是消费面)⇒ 顺带证明 adapter 的别名可用
    using CellKind = CxlCacheAdapter::CellKind;
    using Event = CxlCacheAdapter::Event;
    using Perm = CxlCacheAdapter::Perm;
    using State = CxlCacheAdapter::State;

    /// 读仓内文件
    std::string read_repo_file(const std::string& rel) {
#ifdef CPPTLM_SOURCE_DIR
        const std::string root = std::string(CPPTLM_SOURCE_DIR);
#else
        const std::string root = ".";
#endif
        std::ifstream in(root + "/" + rel, std::ios::binary);
        if (!in)
            return {};
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    /// 剥掉注释(行注释 + 块注释)⇒ 扫描的输入
    std::string strip_comments(std::string code) {
        std::string out;
        out.reserve(code.size());
        for (std::size_t i = 0; i < code.size();) {
            if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '/') {
                while (i < code.size() && code[i] != '\n')
                    ++i;
            } else if (code[i] == '/' && i + 1 < code.size() && code[i + 1] == '*') {
                i += 2;
                while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/'))
                    ++i;
                i = (i + 1 < code.size()) ? i + 2 : code.size();
            } else {
                out.push_back(code[i]);
                ++i;
            }
        }
        return out;
    }

    /// adapter 的去注释后文本(零副本静态扫描的输入)
    std::string adapter_code() {
        static const std::string code =
            strip_comments(read_repo_file("include/tlm/cep/cxl_cache_adapter.hh"));
        return code;
    }

    /// 「零副本」违禁记号 —— adapter **不**命名任何枚举字面量,**也不**指名任何表
    /// ⇒ 它在语法上**不可能**携带自己的表(只能调访问器)。
    const std::vector<std::string>& forbidden_tokens() {
        static const std::vector<std::string> kTokens = {
            "State::", "Event::", "Perm::", "kTable", "kTransitions", "kPermProjection"};
        return kTokens;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// CNA-01 —— 零自有表副本
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL adapter: 零自有表副本(68 格 pass-through + 静态扫描)", "[cep][cxl][adapter]") {
    // ── 子条件 ① / ②:68 格 pass-through(转换 kind + target 逐格)──────────
    std::size_t cells = 0;
    for (std::size_t s = 0; s < CxlCacheAdapter::kStateCount; ++s) {
        const auto st = static_cast<State>(s);
        for (std::size_t e = 0; e < CxlCacheAdapter::kEventCount; ++e) {
            const auto ev = static_cast<Event>(e);
            INFO("state=" << CxlStateSpec::state_name(st)
                          << " event=" << CxlStateSpec::event_name(ev));
            const auto& src = CxlStateSpec::transition(st, ev);
            REQUIRE(CxlCacheAdapter::transition_kind(st, ev) == src.kind);
            if (src.kind == CellKind::Known) {
                REQUIRE(CxlCacheAdapter::transition_target(st, ev) == src.target);
            }
            REQUIRE(CxlCacheAdapter::transition_is_known(st, ev) == (src.kind == CellKind::Known));
            REQUIRE(CxlCacheAdapter::transition_is_illegal(st, ev) ==
                    (src.kind == CellKind::Illegal));
            REQUIRE(CxlCacheAdapter::transition_is_unknown(st, ev) ==
                    (src.kind == CellKind::Unknown));
            ++cells;
        }
    }
    REQUIRE(cells == 68u);

    // 4 态投影逐格 pass-through
    for (std::size_t s = 0; s < CxlCacheAdapter::kStateCount; ++s) {
        const auto st = static_cast<State>(s);
        Perm pa = Perm::NONE;
        Perm pb = Perm::NONE;
        REQUIRE(CxlCacheAdapter::perm_of(st, pa));
        REQUIRE(CxlStateSpec::perm_of(st, pb));
        REQUIRE(static_cast<int>(pa) == static_cast<int>(pb));
    }

    // ── 子条件 ③:静态扫描(去注释后不得含任何违禁记号)──────────────────
    const std::string code = adapter_code();
    REQUIRE(!code.empty());
    for (const auto& tok : forbidden_tokens()) {
        INFO("forbidden token: " << tok);
        REQUIRE(code.find(tok) == std::string::npos);
    }

    // ── 正控:扫描器本身有鉴别力(违禁记号必须被命中)─────────────────────
    const std::string probe =
        "using S = X::State; S v = X::State::I; static constexpr int kTable[4][17] = {};";
    REQUIRE(strip_comments(probe).find("State::") != std::string::npos);
    REQUIRE(strip_comments(probe).find("kTable") != std::string::npos);
    // ⚠️ 注释里的违禁记号**不**算命中(否则扫描器会被注释骗过 ⇒ 失守)
    REQUIRE(strip_comments("// State:: Perm:: kTable\nint x;").find("State::") ==
            std::string::npos);

    // ── 反向 fixture:伪造一份表 ⇒ 静态扫描必红 ──────────────────────────
    const std::string forged =
        "class CxlCacheAdapter { static constexpr int kTransitions[4][17] = {}; "
        "static constexpr int kPermProjection[4] = {}; };";
    REQUIRE(strip_comments(forged).find("kTransitions") != std::string::npos);
    REQUIRE(strip_comments(forged).find("kPermProjection") != std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// CNA-02 —— 三态不被折叠
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL adapter: 三态不被折叠(Unknown ≠ Illegal)", "[cep][cxl][adapter]") {
    // ① 计数自洽:三态之和 == 68,且 `Illegal` 恒 0
    REQUIRE(CxlStateSpec::count_cells(CellKind::Known) == 26);
    REQUIRE(CxlStateSpec::count_cells(CellKind::Illegal) == 0);
    REQUIRE(CxlStateSpec::count_cells(CellKind::Unknown) == 42);

    // ② `Unknown` 格 SHALL NOT 被读成「非法」—— 逐格核对
    for (std::size_t s = 0; s < CxlCacheAdapter::kStateCount; ++s) {
        for (std::size_t e = 0; e < CxlCacheAdapter::kEventCount; ++e) {
            const auto st = static_cast<State>(s);
            const auto ev = static_cast<Event>(e);
            const auto k = CxlCacheAdapter::transition_kind(st, ev);
            REQUIRE((k == CellKind::Unknown) == CxlCacheAdapter::transition_is_unknown(st, ev));
            REQUIRE((k == CellKind::Illegal) == CxlCacheAdapter::transition_is_illegal(st, ev));
            REQUIRE((k == CellKind::Known) == CxlCacheAdapter::transition_is_known(st, ev));
        }
    }

    // ③ 抽样:42 个 `Unknown` 格中的代表 —— 写事件在 `E` 态 ⇒ `Unknown`(非 `Illegal`)
    REQUIRE(CxlCacheAdapter::transition_is_unknown(State::E, Event::ItoMWr));
    REQUIRE_FALSE(CxlCacheAdapter::transition_is_illegal(State::E, Event::ItoMWr));
    // 已确证的格 ⇒ `Known` + 目标正确
    REQUIRE(CxlCacheAdapter::transition_is_known(State::M, Event::SnpInv));
    REQUIRE(CxlCacheAdapter::transition_target(State::M, Event::SnpInv) ==
            CxlStateSpec::transition(State::M, Event::SnpInv).target);
}

// ─────────────────────────────────────────────────────────────────────────────
// CNA-05 —— `Unknown` 格的消费纪律 + 静默升级的 basis 诚实性
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("CEP CXL adapter: 静默升级 + 残余风险登记(委托,无双真源)", "[cep][cxl][adapter]") {
    // ① 静默升级判定逐格 pass-through
    for (std::size_t s = 0; s < CxlCacheAdapter::kStateCount; ++s) {
        for (std::size_t e = 0; e < CxlCacheAdapter::kEventCount; ++e) {
            const auto st = static_cast<State>(s);
            const auto ev = static_cast<Event>(e);
            REQUIRE(CxlCacheAdapter::is_silent_upgrade(st, ev) ==
                    CxlStateSpec::is_silent_upgrade(st, ev));
        }
    }
    // ⚠️ 该判据的 basis 是 `analogical` + `pending` ⇒ **不得**当已裁决事实
    REQUIRE(CxlCacheAdapter::is_silent_upgrade(State::E, Event::ItoMWr));
    REQUIRE(CxlCacheAdapter::is_silent_upgrade(State::E, Event::WrInv));
    REQUIRE_FALSE(CxlCacheAdapter::is_silent_upgrade(State::S, Event::ItoMWr));

    // ② 残余风险清单**委托**(散文无双真源)
    REQUIRE(CxlCacheAdapter::kResidualRiskCount == 8);
    REQUIRE(CxlCacheAdapter::kResidualRisks.size() == 8);
    REQUIRE(CxlCacheAdapter::kTransientStateCount == 0);

    // ③ 标识常量(逐字对齐 `protocol_instances.json`)
    REQUIRE(std::string(CxlCacheAdapter::kAdapterName) == "CxlCacheAdapter");
    REQUIRE(std::string(CxlCacheAdapter::kInstanceId) == "cxl_2_0");
}
