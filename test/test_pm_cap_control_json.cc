// test/test_pm_cap_control_json.cc
// PM Cap control word JSON-driven (followups §4)
// per openspec/changes/2027-02-09-cpptlm-stage-1-4-2-1-followups/design.md §4
//
// 标签: [pcie] [pm] [json]
//
// 作者 CppTLM Team / 日期 2027-02-09
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_endpoint_ip.hh"

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/param_errors.hh"

#include <nlohmann/json.hpp>

#include <atomic>
#include <string>

using json = nlohmann::json;
using tlm::pcie::PcieEndpointIP;

namespace {

    struct PmJsonFixture {
        EventQueue eq;
        std::unique_ptr<PcieEndpointIP> ep;
        std::string ep_name;

        PmJsonFixture() : ep_name("pcie_ep_pmjson_test_" + std::to_string(next_id())) {
            ep = std::make_unique<PcieEndpointIP>(ep_name, &eq);
            ep->init();
        }

        static int next_id() {
            static std::atomic<int> s{0};
            return s.fetch_add(1);
        }

        void apply(const json& cfg) {
            ep->set_config(cfg);
            ep->on_config_loaded();
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: default control = 0x0013 when key absent
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PM Cap control: default 0x0013 when pm_cap_control absent", "[pcie][pm][json]") {
    PmJsonFixture f;
    json cfg;
    cfg["link_layer"]["enabled"] = true;
    f.apply(cfg);
    // 构造器安装 control=0x0013; cap dword = control<<16 | next<<8 | id
    const uint32_t dword = f.ep->vf_pool().config_of(0).read(0x40);
    REQUIRE(((dword >> 16) & 0xFFFFu) == 0x0013u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: JSON pm_cap_control writes control field
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PM Cap control: JSON pm_cap_control applied via update_capability_control",
          "[pcie][pm][json]") {
    PmJsonFixture f;
    json cfg;
    cfg["pm_cap_control"] = 5251; // 0x1483
    f.apply(cfg);
    // read(0x40) 非 4 对齐错误示范: control 在 dword 0x40 高 16-bit (勘误)
    const uint32_t dword = f.ep->vf_pool().config_of(0).read(0x40);
    REQUIRE(((dword >> 16) & 0xFFFFu) == 0x1483u);
    // cap id/next 不被破坏
    REQUIRE((dword & 0xFFu) == 0x01u);        // PM Cap id
    REQUIRE(((dword >> 8) & 0xFFu) == 0x00u); // next = 0
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario: `pm_cap_control` 白名单生效；无关**顶层**键 ⇒ 装配失败
//
// ⚠️ **语义变更**（change `cpptlm-module-param-migrate-batch2`，verifier 验收 finding F1 返工）:
//   `PcieEndpointIP` 自本次起**已准入** ⇒ 未知**顶层**键不再「只告警」，而是
//   **装配失败**并报出键名（`ADR-X.17` D-3：⛔ 告警不失败 = 静默容忍）。
//   告警面（`warn_unconsumed`）仍覆盖**嵌套子键**（见
//   `test/test_pcie_endpoint_ip_json_config.cc` 的 `unknown subkey in phy_digital`）。
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PM Cap control: pm_cap_control 白名单生效；无关顶层键 ⇒ 装配失败",
          "[pcie][pm][json][warn]") {
    PmJsonFixture f;
    json cfg;
    cfg["pm_cap_control"] = 0x0013;
    f.apply(cfg);
    // pm_cap_control 已加入白名单, 不应产生 warning
    const auto& w = f.ep->config_warnings();
    const bool has_pm_cap_warn = std::any_of(w.begin(), w.end(), [](const std::string& s) {
        return s.find("pm_cap_control") != std::string::npos;
    });
    REQUIRE(has_pm_cap_warn == false);

    // 无关顶层键 ⇒ **装配失败**且错误含键名（准入后的未知键检出）
    json bad;
    bad["pm_cap_control"] = 0x0013;
    bad["foo_bar"] = 42;
    try {
        f.apply(bad);
        FAIL("expected cpptlm::ParamValidationError for key 'foo_bar'");
    } catch (const cpptlm::ParamValidationError& e) {
        CHECK(e.param_name == "foo_bar");
    }
}
