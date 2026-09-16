// test/test_pcie_endpoint_ip_tick_dispatch.cc
// PcieEndpointIP::tick() TLP 入方向三态分派测试 (T-P10-3)
// 功能：验证 dispatch_tlp_entry 按 Bypass Mux mode 分派：
//   - Full 模式: TLP 经 LL FC check + ACK 生成 → set_tlp_sink → CompleterEngine
//   - Bypass 模式: TLP 跳过 LL FC → 直送 set_tlp_sink → CompleterEngine (无 FC 检查)
//   - Partial 模式: 同 Full (保留 LL FC)
// 作者 CppTLM Team / 日期 2026-09-17
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P10-3
#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_bypass_mux.hh"

#include <cstdint>
#include <nlohmann/json.hpp>

using namespace bundles;
using namespace tlm::pcie;
using json = nlohmann::json;

// ========== 测试框架: 创建带 link_layer 的 PcieEndpointIP ==========
namespace {

    struct TickDispatchFixture {
        EventQueue eq;
        PcieEndpointIP ep;

        TickDispatchFixture(const std::string& name, const std::string& bypass_mode)
            : ep(name, &eq)
        {
            ep.init();
            json cfg;
            cfg["link_layer"]["enabled"] = true;
            cfg["link_layer"]["bypass_mode"] = bypass_mode;
            ep.set_config(cfg);
            ep.on_config_loaded();
            // 写 BAR0 寄存器 (offset 0x10): 32-bit memory BAR, 基地址 0x10000000
            ep.config_space().write(0x10, 0x10000000);
        }

        // 构造 MMIO_WRITE TLP
        static PcieTlpBundle make_mmio_write(uint64_t offset, uint64_t data,
                                              uint8_t bar = 0)
        {
            return PcieTlpBundle(
                PcieTlpBundle::MMIO_WRITE, bar, offset, 4,
                data, 0x0100 /* requester_id */, 0x42 /* trans_id */
            );
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// TEST_CASE 1: Full 模式 TLP 经 LL FC check 后到达 CompleterEngine
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP tick dispatch: Full mode LL FC check then CompleterEngine",
          "[pcie][tick-dispatch][pcie-t10-3]")
{
    TickDispatchFixture f("ep_full_dispatch", "Full");
    // Full mode 默认: bypass_mux_mode_ = Full (由 JSON bypass_mode = "Full" 设置)

    // 注入 MMIO_WRITE TLP: BAR0+0x1000, data=0xDEADBEEF
    auto mwr = TickDispatchFixture::make_mmio_write(0x1000, 0xDEADBEEF);
    f.ep.inject_tlp_from_host_for_test(mwr);

    // 验证 bar_store_ 落盘: (BDF=0, BAR=0, addr=0x1000) = 0xDEADBEEF
    REQUIRE(f.ep.bar_store_value(0, 0, 0x1000) == 0xDEADBEEF);

    // Full mode: LL FC 应消耗 (credit 减少). 初始 credit = 256 per P/NP/Cpl
    // MMIO_WRITE = Posted, 消耗 1 个 P credit
    // 注入后 FC 应 < 256
    auto fc = f.ep.link_layer_fc_snapshot_for_test();
    REQUIRE(std::get<0>(fc) < 256);  // P credit consumed
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST_CASE 2: Bypass 模式 TLP 跳过 LL FC 直达 CompleterEngine
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP tick dispatch: Bypass mode skips LL FC",
          "[pcie][tick-dispatch][pcie-t10-3]")
{
    TickDispatchFixture f("ep_bypass_dispatch", "Bypass");

    // 记录初始 FC 快照
    auto fc_initial = f.ep.link_layer_fc_snapshot_for_test();

    // 注入 MMIO_WRITE TLP: BAR0+0x1000, data=0xCAFEBABE
    auto mwr = TickDispatchFixture::make_mmio_write(0x1000, 0xCAFEBABE);
    f.ep.inject_tlp_from_host_for_test(mwr);

    // Bypass: FC 不变 (不经 LL FC)
    auto fc_after = f.ep.link_layer_fc_snapshot_for_test();
    REQUIRE(fc_after == fc_initial);

    // 验证 bar_store_ 仍落盘 (CompleterEngine 处理)
    REQUIRE(f.ep.bar_store_value(0, 0, 0x1000) == 0xCAFEBABE);
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST_CASE 3: Partial 模式同 Full (保留 LL FC)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP tick dispatch: Partial mode same as Full (LL FC retained)",
          "[pcie][tick-dispatch][pcie-t10-3]")
{
    TickDispatchFixture f("ep_partial_dispatch", "Partial");

    // 记录初始 FC
    auto fc_initial = f.ep.link_layer_fc_snapshot_for_test();

    // 注入 MMIO_WRITE TLP: BAR0+0x100, data=0xFEEDFACE
    auto mwr = TickDispatchFixture::make_mmio_write(0x100, 0xFEEDFACE);
    f.ep.inject_tlp_from_host_for_test(mwr);

    // Partial: FC 应消耗 (同 Full, 保留 LL FC)
    auto fc_after = f.ep.link_layer_fc_snapshot_for_test();
    REQUIRE(std::get<0>(fc_after) < std::get<0>(fc_initial));  // P credit consumed

    // 验证 bar_store_ 落盘
    REQUIRE(f.ep.bar_store_value(0, 0, 0x100) == 0xFEEDFACE);
}