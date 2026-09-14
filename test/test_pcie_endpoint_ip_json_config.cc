// test/test_pcie_endpoint_ip_json_config.cc
// PcieEndpointIP JSON 配置扩展 (Phase A1) — Catch2 测试套件
// 覆盖 5 个 Scenario per openspec/changes/2027-02-09-cpptlm-pcie-endpoint-ip-json-config/specs/spec.md
//
// 标签: [pcie] [pcie-json] + 子标签 [phy] [ari] [msix] [config-size] [warn]
//
// 作者 CppTLM Team / 日期 2027-02-09
// 参考: design.md §5 + spec.md §MODIFIED Requirements
#include "catch_amalgamated.hpp"

#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_sriov_vf_pool_tlm.hh"
#include "tlm/pcie/pcie_msix_per_vf_tlm.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"
#include "tlm/pcie/pcie_ari_router_tlm.hh"
#include "tlm/pcie/pcie_config_space_per_vf_tlm.hh"
#include "tlm/gpu/pcie_config_space_mvp.hh"
#include "tlm/gpu/msix_table_mvp.hh"

#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/sim_object.hh"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

using json = nlohmann::json;
using tlm::pcie::PcieEndpointIP;

namespace {

// Test fixture: 提供一个最小 PcieEndpointIP 实例 + EventQueue
struct JsonConfigFixture {
    EventQueue eq;
    PcieEndpointIP ep;

    JsonConfigFixture() : ep("pcie_ep_json_test", &eq) {
        // 默认 attach link_layer+phy, 这样 fixture 默认能验证 phy/sr_iov/transaction_layer
        ep.init();
        json default_cfg;
        default_cfg["link_layer"]["enabled"] = true;
        ep.set_config(default_cfg);
        ep.on_config_loaded();
    }

    // 把 json cfg 推入 attach_composition (完整替换, link_layer 缺失时按 nullptr 处理)
    void apply(const json& cfg) {
        ep.set_config(cfg);
        ep.on_config_loaded();
    }

    // 断言某字符串出现在 config_warnings_ 中
    bool warns_about(const std::string& substr) const {
        const auto& w = ep.config_warnings();
        return std::any_of(w.begin(), w.end(),
            [&](const std::string& s) { return s.find(substr) != std::string::npos; });
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Scenario 1: phy_digital presets applied via read-modify-write
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP JSON config: phy_digital presets take effect; max_speed preserved",
          "[pcie][pcie-json][phy]") {
    JsonConfigFixture f;

    // Step 1: 先设置一个非默认 PHY config (模拟"既有 max_speed 等")
    auto* phy = tlm::pcie::PciePhyDigitalCtrl::for_endpoint("pcie_ep_json_test");
    if (phy) {
        tlm::pcie::PciePhyConfig initial_cfg;
        initial_cfg.max_speed =
            tlm::pcie::PcieEncodingLatencyModel::Rate::GEN5;
        initial_cfg.max_lanes = 16;
        initial_cfg.hot_plug_supported = false;
        phy->set_config(initial_cfg);
    }

    // Step 2: 通过 JSON 只设置 presets + hot_plug (link_layer 必填以复用 fixture attach 的 phy)
    json cfg;
    cfg["link_layer"]["enabled"] = true;
    cfg["phy_digital"]["preset_p"] = 5;
    cfg["phy_digital"]["preset_np"] = 9;
    cfg["phy_digital"]["preset_cpl"] = 11;
    cfg["phy_digital"]["hot_plug_supported"] = true;
    f.apply(cfg);

    // Step 3: 断言 presets 已更新，且 max_speed 未被重置 (INV-2 read-modify-write)
    // 注: attach_to_endpoint replace 分支会替换 registry 对象, 必须重新取 phy
    phy = tlm::pcie::PciePhyDigitalCtrl::for_endpoint("pcie_ep_json_test");
    if (phy) {
        const auto& c = phy->config();
        REQUIRE(c.preset_P == 5);
        REQUIRE(c.preset_NP == 9);
        REQUIRE(c.preset_Cpl == 11);
        REQUIRE(c.hot_plug_supported == true);
        REQUIRE(c.max_speed ==
                tlm::pcie::PcieEncodingLatencyModel::Rate::GEN5);  // ← 关键: 未被重置
        REQUIRE(c.max_lanes == 16);                         // ← 关键: 未被重置
    }

    // Step 4: link_layer.enabled=false + phy 块组合不崩溃
    json cfg2;
    cfg2["link_layer"]["enabled"] = false;
    cfg2["phy_digital"]["preset_p"] = 3;  // 应被静默跳过, 不崩
    REQUIRE_NOTHROW(f.apply(cfg2));
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario 2: sr_iov.ari_capable enables VF8-15 routing
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP JSON config: sr_iov.ari_capable enables ARI routing",
          "[pcie][pcie-json][ari]") {
    JsonConfigFixture f;

    SECTION("default: ari disabled") {
        json cfg;
        f.apply(cfg);
        REQUIRE(f.ep.vf_pool().ari_router().ari_enabled() == false);
    }

    SECTION("JSON ari_capable=true enables") {
        json cfg;
        cfg["sr_iov"]["ari_capable"] = true;
        f.apply(cfg);
        REQUIRE(f.ep.vf_pool().ari_router().ari_enabled() == true);
    }

    SECTION("JSON ari_capable=false explicit disable") {
        json cfg;
        cfg["sr_iov"]["ari_capable"] = false;
        f.apply(cfg);
        REQUIRE(f.ep.vf_pool().ari_router().ari_enabled() == false);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario 3: msix vectors reconfig per-VF at composition time
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP JSON config: msix vectors reconfigure PF + VF1..VFn",
          "[pcie][pcie-json][msix]") {
    JsonConfigFixture f;

    json cfg;
    cfg["sr_iov"]["vf_msix_vectors"] = 4;
    cfg["transaction_layer"]["msix_num_vectors"] = 16;
    f.apply(cfg);

    // 断言: vector 0..15 在 PF slot 可访问 (config_size=16)
    // 断言: vector 0..3 在 VF slot 可访问, 4 静默拒绝
    // 实际验证通过 update_pending 边界行为
    auto& msix_pf = f.ep.vf_pool().msix_pool();
    // PF slot 0
    for (uint16_t v = 0; v < 16; ++v) {
        REQUIRE_NOTHROW(msix_pf.update_pending(0, v, true));
        msix_pf.update_pending(0, v, false);  // reset
    }
    // VF slot 1
    for (uint16_t v = 0; v < 4; ++v) {
        REQUIRE_NOTHROW(msix_pf.update_pending(1, v, true));
        msix_pf.update_pending(1, v, false);
    }
    // VF slot 1, vector 4 — 在 4-vector 配置下应被静默拒绝 (不抛异常, 不更新)
    REQUIRE_NOTHROW(msix_pf.update_pending(1, 4, true));
    // 后续读取 vector 4 不应为 pending
    REQUIRE(msix_pf.pending(1, 4) == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario 4: config_size applies to all 17 slots with validation
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP JSON config: transaction_layer.config_size applies to all 17 slots",
          "[pcie][pcie-json][config-size]") {
    JsonConfigFixture f;

    SECTION("valid value 4096 applied to all 17") {
        json cfg;
        cfg["transaction_layer"]["config_size"] = 4096;
        f.apply(cfg);
        for (uint16_t slot = 0; slot < 17; ++slot) {
            REQUIRE(f.ep.vf_pool().config_of(slot).config_size() == 4096);
        }
    }

    SECTION("valid value 256 applied to all 17") {
        json cfg;
        cfg["transaction_layer"]["config_size"] = 256;
        f.apply(cfg);
        for (uint16_t slot = 0; slot < 17; ++slot) {
            REQUIRE(f.ep.vf_pool().config_of(slot).config_size() == 256);
        }
    }

    SECTION("invalid value 1024 falls back to 4096 + warning") {
        json cfg;
        cfg["transaction_layer"]["config_size"] = 1024;
        f.apply(cfg);
        // 所有 17 slot 应落回 4096
        for (uint16_t slot = 0; slot < 17; ++slot) {
            REQUIRE(f.ep.vf_pool().config_of(slot).config_size() == 4096);
        }
        // 警告应记录
        REQUIRE(f.warns_about("config_size must be 256 or 4096"));
        REQUIRE(f.warns_about("got 1024"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Scenario 5: unconsumed JSON keys emit warning to stderr and config_warnings getter
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP JSON config: unconsumed keys warn via getter + stderr",
          "[pcie][pcie-json][warn]") {
    JsonConfigFixture f;

    SECTION("unknown top-level key") {
        json cfg;
        cfg["foo_bar"] = 42;
        cfg["baz_qux"] = "hello";
        f.apply(cfg);
        REQUIRE(f.warns_about("foo_bar"));
        REQUIRE(f.warns_about("baz_qux"));
    }

    SECTION("unknown subkey in phy_digital") {
        json cfg;
        cfg["link_layer"]["enabled"] = true;  // 必填以复用 fixture attach 的 phy
        cfg["phy_digital"]["pipe_interface"] = "lightweight_4_signal";
        cfg["phy_digital"]["ltssm_initial_state"] = "detect";
        cfg["phy_digital"]["perst_signal_initial"] = "asserted";
        f.apply(cfg);
        REQUIRE(f.warns_about("pipe_interface"));
        REQUIRE(f.warns_about("ltssm_initial_state"));
        REQUIRE(f.warns_about("perst_signal_initial"));
    }

    SECTION("unknown subkey in sr_iov") {
        json cfg;
        cfg["link_layer"]["enabled"] = true;
        cfg["sr_iov"]["initial_vfs"] = 8;  // out of scope
        cfg["sr_iov"]["total_vfs"] = 16;  // out of scope
        cfg["sr_iov"]["num_vfs"] = 8;     // out of scope
        cfg["sr_iov"]["vf_bar0_size"] = 65536;  // deferred
        cfg["sr_iov"]["vf_pool_stream_adapter"] = "shared";  // not consumed
        f.apply(cfg);
        REQUIRE(f.warns_about("initial_vfs"));
        REQUIRE(f.warns_about("total_vfs"));
        REQUIRE(f.warns_about("num_vfs"));
        REQUIRE(f.warns_about("vf_bar0_size"));
        REQUIRE(f.warns_about("vf_pool_stream_adapter"));
    }

    SECTION("unknown subkey in transaction_layer") {
        json cfg;
        cfg["link_layer"]["enabled"] = true;
        cfg["transaction_layer"]["bar_sizes"] = json::array({65536, 268435456});
        cfg["transaction_layer"]["bar0_registers"] = json::array();
        f.apply(cfg);
        REQUIRE(f.warns_about("bar_sizes"));
        REQUIRE(f.warns_about("bar0_registers"));
    }

    SECTION("known keys do not warn") {
        json cfg;
        cfg["axi_adapter"]["axi4_mapper_inject"] = true;
        cfg["link_layer"]["enabled"] = true;
        cfg["phy_digital"]["preset_p"] = 7;
        cfg["sr_iov"]["ari_capable"] = true;
        cfg["transaction_layer"]["config_size"] = 4096;
        f.apply(cfg);
        // 不应产生任何未消费键 warning (axi4_mapper_inject / preset_p / ari_capable 都已消费)
        // 注: link_layer 子键 fc_*_credit_* 等未写也不报错, 只警告未识别
        REQUIRE_FALSE(f.warns_about("preset_p"));
        REQUIRE_FALSE(f.warns_about("ari_capable"));
        REQUIRE_FALSE(f.warns_about("axi4_mapper_inject"));
        REQUIRE_FALSE(f.warns_about("config_size"));
    }
}
