// test/test_pcie_endpoint_ip_tlp_path_e2e.cc
// TLP 链路端到端 E2E 测试 (T-P12-2)
// 功能：验证 UsrLinuxEmu ABI 路径真实产生 TLP → LL → CompleterEngine → bar_store_
//       → CplD 回发 → ABI 返回 (per spec.md §end-to-end-tlp-path)
//       - MWr 写路径: TLP 真实产生, FC P credit 消耗, bar_store_ 落盘
//       - MRd→CplD 读路径: tag 关联, bar_store_ 读回, CplD 数据一致
//       - 钩子点: tlp_rx_count_ / set_tlp_sink lambda / FC 快照
// 作者 CppTLM Team / 日期 2027-02-09
// 参考: openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/tasks.md T-P12-2
//       openspec/changes/2026-09-16-cpptlm-pcie-tlp-wire-datapath/specs/pcie-tlp-wire-datapath/spec.md
#include "bundles/pcie_bundles_tlm.hh"
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_bypass_mux.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <cstdint>
#include <nlohmann/json.hpp>

using namespace bundles;
using namespace tlm::pcie;
using json = nlohmann::json;

namespace {

    // 带 link_layer 的 PcieEndpointIP 测试夹具, 支持 bypass_mode 参数化
    struct TlpE2EFixture {
        EventQueue eq;
        PcieEndpointIP ep;
        std::string mode_name;

        TlpE2EFixture(const std::string& name, const std::string& bypass_mode)
            : ep(name, &eq), mode_name(bypass_mode)
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
                PcieTlpBundle::MMIO_WRITE, bar, offset, 8,
                data, 0x0100 /* requester_id */, 0x42 /* trans_id */
            );
        }

        // 构造 MMIO_READ TLP
        static PcieTlpBundle make_mmio_read(uint64_t offset, uint8_t tag = 0x77)
        {
            return PcieTlpBundle(
                PcieTlpBundle::MMIO_READ, 0, offset, 8,
                0, 0x0100 /* requester_id */, tag /* trans_id = tag */
            );
        }
    };

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// TEST_CASE 1: MWr TLP 写路径 — TLP 真实产生, FC P credit 消耗, bar_store_ 落盘
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TLP E2E: MWr 写路径 — TLP 真实产生 + FC P credit 消耗 + bar_store_ 落盘",
          "[pcie][tlp-e2e][phase9]")
{
    TlpE2EFixture f("ep_e2e_mwr", "Full");

    // 初始 FC 快照
    auto [p_before, np_before, cpl_before] = f.ep.link_layer_fc_snapshot_for_test();
    REQUIRE(p_before == 256);
    REQUIRE(np_before == 256);
    REQUIRE(cpl_before == 256);

    // tlp_rx_count_ 初始为 0
    auto* ll = f.ep.link_layer();
    REQUIRE(ll != nullptr);
    uint64_t rx_before = ll->tlp_rx_count();

    // 注入 MMIO_WRITE TLP (Full 模式: 经 LL FC check + ACK 生成)
    constexpr uint64_t kWData = 0xDEADBEEFCAFEBABEULL;
    auto mwr = TlpE2EFixture::make_mmio_write(0x1000, kWData);
    f.ep.inject_tlp_from_host_for_test(mwr);

    // ── 验证 bar_store_ 落盘 ──
    REQUIRE(f.ep.bar_store_value(0, 0, 0x1000) == kWData);

    // ── 验证 FC P credit 消耗 ──
    auto [p_after, np_after, cpl_after] = f.ep.link_layer_fc_snapshot_for_test();
    // MWr = Posted, P credit 消耗
    REQUIRE(p_after < p_before);
    // NP 和 Cpl 不变 (MWr 不消耗 NP/Cpl)
    REQUIRE(np_after == np_before);
    REQUIRE(cpl_after == cpl_before);

    // ── 验证 tlp_rx_count_ 增加 (真实 TLP 经 LL 入口) ──
    REQUIRE(ll->tlp_rx_count() > rx_before);

    // ── 验证 MWr 是 Posted 事务 → 无 CplD 产生 ──
    // MWr 经 dispatch_tlp_entry 写 bar_store_ 并调 completer_engine.handle_tlp,
    // handle_mmio_write 返回空 bundle (非 CPLD), 故 tx_tlp_out 应为空
    PcieTlpBundle out;
    bool has_tlp = ll->try_pop_tx_tlp(out);
    REQUIRE(has_tlp == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST_CASE 2: MRd→CplD 读路径 — tag 关联 + CplD 数据一致 + FC NP credit 消耗
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TLP E2E: MRd→CplD 读路径 — tag 关联 + CplD 回发数据一致 + FC NP 消耗",
          "[pcie][tlp-e2e][phase9]")
{
    TlpE2EFixture f("ep_e2e_mrd", "Full");

    // 先通过 MWr 写入 bar_store_ (模拟 ABI mmio_write 真实路径)
    constexpr uint64_t kWData = 0xABCD12345678EEFFULL;
    auto mwr = TlpE2EFixture::make_mmio_write(0x2000, kWData);
    f.ep.inject_tlp_from_host_for_test(mwr);
    REQUIRE(f.ep.bar_store_value(0, 0, 0x2000) == kWData);

    // 记 FC NP 初始值
    auto [p0, np_before, cpl0] = f.ep.link_layer_fc_snapshot_for_test();

    // 注入 MRd TLP (tag=0x55, 经 LL FC check → dispatch → completer → CplD → tx_tlp)
    constexpr uint8_t kTag = 0x55;
    auto mrd = TlpE2EFixture::make_mmio_read(0x2000, kTag);
    f.ep.inject_tlp_from_host_for_test(mrd);

    // ── 验证 FC NP credit 消耗 (MRd = Non-Posted) ──
    auto [p1, np_after, cpl1] = f.ep.link_layer_fc_snapshot_for_test();
    REQUIRE(np_after < np_before);

    // ── 验证 CplD 已从 tx_tlp_out 弹出 ──
    auto* ll = f.ep.link_layer();
    REQUIRE(ll != nullptr);

    PcieTlpBundle cpld;
    bool got_cpld = ll->try_pop_tx_tlp(cpld);
    REQUIRE(got_cpld == true);

    // ── 验证 CplD 字段 ──
    // kind = CPLD (7)
    REQUIRE(static_cast<uint8_t>(cpld.kind.read()) == PcieTlpBundle::CPLD);
    // tag 回显
    REQUIRE(static_cast<uint8_t>(cpld.trans_id.read() & 0xFF) == kTag);
    // requester_id 回显
    REQUIRE(static_cast<uint16_t>(cpld.requester_id.read()) == 0x0100);
    // 数据 = bar_store_ 值
    REQUIRE(cpld.data.read() == kWData);
    // 大小 = 8 字节
    REQUIRE(static_cast<uint32_t>(cpld.size.read()) == 8);
}

// ─────────────────────────────────────────────────────────────────────────────
// TEST_CASE 3: 钩子点验证 — tlp_rx_count_ / set_tlp_sink lambda / FC 快照
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("TLP E2E: 钩子点验证 — tlp_rx_count_ / set_tlp_sink lambda / FC 快照",
          "[pcie][tlp-e2e][phase9]")
{
    // ── 钩子 1: PcieLinkLayer 入口计数器 ──
    // 使用独立的 Full 模式 EP 验证 tlp_rx_count_ 在 Full 模式下递增
    {
        TlpE2EFixture f("ep_e2e_hook1", "Full");
        auto* ll = f.ep.link_layer();
        REQUIRE(ll != nullptr);
        REQUIRE(ll->tlp_rx_count() == 0);

        // 注入 MMIO_WRITE (Full 模式: 经 rx_tlp_from_host 入口)
        auto mwr1 = TlpE2EFixture::make_mmio_write(0x3000, 0x1111);
        f.ep.inject_tlp_from_host_for_test(mwr1);
        REQUIRE(ll->tlp_rx_count() == 1);

        // 第二次注入 → 递增至 2
        auto mwr2 = TlpE2EFixture::make_mmio_write(0x3008, 0x2222);
        f.ep.inject_tlp_from_host_for_test(mwr2);
        REQUIRE(ll->tlp_rx_count() == 2);
    }

    // ── 钩子 2: set_tlp_sink lambda 捕获 (Bypass 模式) ──
    // Full 模式: inject_tlp_from_host_for_test 走 LL rx_tlp_from_host,
    // 调的是 LL 的 tlp_sink_ (dispatch_tlp_entry), 不是 EP 的 tlp_sink_.
    // Bypass 模式: inject_tlp_from_host_for_test 直接调 EP 的 tlp_sink_, lambda 被捕获.
    // bar_store_ 落盘已在 TEST_CASE 1 Full 模式下验证, 此处只测 lambda 捕获.
    {
        TlpE2EFixture f("ep_e2e_hook2", "Bypass");

        PcieTlpBundle captured_tlp;
        bool lambda_invoked = false;
        f.ep.set_tlp_sink([&](const PcieTlpBundle& tlp) {
            lambda_invoked = true;
            captured_tlp = tlp;
        });

        auto mwr = TlpE2EFixture::make_mmio_write(0x5000, 0xEEFF1122);
        f.ep.inject_tlp_from_host_for_test(mwr);

        // Bypass 模式: inject_tlp_from_host_for_test 直接调 tlp_sink_
        REQUIRE(lambda_invoked == true);
        REQUIRE(static_cast<uint8_t>(captured_tlp.kind.read()) == PcieTlpBundle::MMIO_WRITE);
        REQUIRE(static_cast<uint64_t>(captured_tlp.offset.read()) == 0x5000);
        REQUIRE(captured_tlp.data.read() == 0xEEFF1122);
    }

    // ── 钩子 3: FC 快照验证 — 确认 LL FC 桶真实变化 ──
    {
        TlpE2EFixture f("ep_e2e_hook3", "Full");
        auto* ll = f.ep.link_layer();
        REQUIRE(ll != nullptr);

        // 注入 2 个 MWr (Posted): P credit 应减 2
        auto fc0 = f.ep.link_layer_fc_snapshot_for_test();
        f.ep.inject_tlp_from_host_for_test(
            TlpE2EFixture::make_mmio_write(0x6000, 0x1111));
        f.ep.inject_tlp_from_host_for_test(
            TlpE2EFixture::make_mmio_write(0x6008, 0x2222));

        auto fc2 = f.ep.link_layer_fc_snapshot_for_test();
        // P credit 消耗 2
        REQUIRE(std::get<0>(fc2) == std::get<0>(fc0) - 2);
        // NP 和 Cpl 不变
        REQUIRE(std::get<1>(fc2) == std::get<1>(fc0));
        REQUIRE(std::get<2>(fc2) == std::get<2>(fc0));

        // 注入 1 个 MRd (Non-Posted): NP credit 应减 1
        f.ep.inject_tlp_from_host_for_test(
            TlpE2EFixture::make_mmio_read(0x6010, 0x33));

        auto fc3 = f.ep.link_layer_fc_snapshot_for_test();
        REQUIRE(std::get<0>(fc3) == std::get<0>(fc2));  // P 不变
        REQUIRE(std::get<1>(fc3) == std::get<1>(fc2) - 1);  // NP 减 1
    }
}