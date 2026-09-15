// test/test_dgpu_board_v1_uses_pcie_endpoint_ip.cc
// A-1 Path A: 验证 configs/dgpu_board_v1.json 的 pcie_ep 类型切换 + 4 条 dangling connection 处置。
//
// TDD 5 步位置: 1/5 Write test (RED)
//
// 本测试仅验证 A-1（profile 切换）的 JSON 层面效果，不验证 ABI 通路。
// ABI 通路（动态_cast → IP config space）由 A-2 单独测试覆盖，避免单一测试跨多个 task。
//
// 参考: openspec/changes/2026-09-15-cpptlm-stage-1-4-2-1-ue-extensions-unblock/v1.2 design.md §A-1
#include <cerrno>
#include <fstream>
#include "catch_amalgamated.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static json load_board_json(const std::string& path) {
    std::ifstream ifs(path);
    REQUIRE(ifs.is_open());
    return json::parse(ifs);
}

TEST_CASE("dgpu_board_v1.json: pcie_ep type switched to PcieEndpointIP",
          "[dgpu][pcie][unblock][a1]") {
    json cfg = load_board_json("configs/dgpu_board_v1.json");

    // Locate pcie_ep module (嵌套于 soc.modules)
    const auto& soc_mods = cfg["modules"][0]["modules"];
    auto it = std::find_if(soc_mods.begin(), soc_mods.end(),
        [](const json& m) { return m.value("name", "") == "pcie_ep"; });
    REQUIRE(it != soc_mods.end());

    // A-1 主断言: type 切换为 PcieEndpointIP
    REQUIRE(it->value("type", "") == "PcieEndpointIP");
}

TEST_CASE("dgpu_board_v1.json: no TLM-only connections (mmio_out/mem_out/irq_out/slave_in)",
          "[dgpu][pcie][unblock][a1]") {
    json cfg = load_board_json("configs/dgpu_board_v1.json");
    const auto& soc = cfg["modules"][0];

    // Oracle R9: 这 4 条 connection 引用 PcieEndpointTLM 专属端口名；PcieEndpointIP 无
    // 这些端口名，connection_resolver fail-soft 无告警，SoC instantiate 静默失败。
    // A-1 必须删除或改写这 4 条。

    const auto& connections = soc.value("connections", json::array());
    for (const auto& c : connections) {
        std::string src = c.value("src", "");
        REQUIRE(src.find("pcie_ep.mmio_out") == std::string::npos);
        REQUIRE(src.find("pcie_ep.mem_out") == std::string::npos);
    }

    const auto& outputs = soc.value("outputs", json::array());
    for (const auto& o : outputs) {
        std::string internal = o.value("internal", "");
        REQUIRE(internal.find("pcie_ep.irq_out") == std::string::npos);
    }

    const auto& inputs = soc.value("inputs", json::array());
    for (const auto& i : inputs) {
        std::string internal = i.value("internal", "");
        REQUIRE(internal.find("pcie_ep.slave_in") == std::string::npos);
    }
}

TEST_CASE("dgpu_board_v1.json: bar_sizes is numeric array (not strings)",
          "[dgpu][pcie][unblock][a1]") {
    // 防止 type_error 静默全灭: bar_sizes 必须是 uint64_t 数值数组
    json cfg = load_board_json("configs/dgpu_board_v1.json");
    const auto& soc_mods = cfg["modules"][0]["modules"];
    auto it = std::find_if(soc_mods.begin(), soc_mods.end(),
        [](const json& m) { return m.value("name", "") == "pcie_ep"; });
    REQUIRE(it != soc_mods.end());

    const auto& bar_sizes = (*it)["params"].value("bar_sizes", json::array());
    REQUIRE(bar_sizes.is_array());
    REQUIRE_FALSE(bar_sizes.empty());
    for (const auto& bs : bar_sizes) {
        REQUIRE(bs.is_number_integer());
        REQUIRE(bs.get<uint64_t>() > 0);
    }
}

// Spec Scenario 4 (A-2 Path A): grep dynamic_cast<PcieEndpointTLM*> 在 dgpu_board_shell 中应 0 hits
// (7+1 处 cast 站点已全部替换为 PcieEndpointIP*)
TEST_CASE("dgpu_board_shell: 0 dynamic_cast<PcieEndpointTLM*> remain (Scenario 4)",
          "[dgpu][pcie][unblock][a2]") {
    std::ifstream src_ifs("src/tlm/gpu/dgpu_board_shell.cc");
    REQUIRE(src_ifs.is_open());
    std::ifstream hdr_ifs("include/tlm/gpu/dgpu_board_shell.hh");
    REQUIRE(hdr_ifs.is_open());

    auto count_matches = [](std::ifstream& ifs) -> std::size_t {
        std::string line;
        std::size_t count = 0;
        while (std::getline(ifs, line)) {
            if (line.find("dynamic_cast<PcieEndpointTLM") != std::string::npos) {
                ++count;
            }
        }
        return count;
    };

    REQUIRE(count_matches(src_ifs) == 0);
    REQUIRE(count_matches(hdr_ifs) == 0);
}
