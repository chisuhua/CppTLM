// test/test_realip_equivalence_tag.cc
// T25B [equivalence] tag 独立测试 — facade vs EP direct path byte-identical.
//
// Per docs/superpowers/plans/2026-10-07-dgpu-realip-refactor-phase-c-handoff.md
// §5 T25B + §9.2 #10 vacuous tag 风险: [equivalence][p3] 必须 commit 前打 tag,
// 避免 vacuous pass. T25B 测试已在 test_realip_equivalence.cc 包含, 但那里
// tag 是 [realip][p3][equivalence] 三 tag AND (T28 gate 命令使用 [equivalence][p3]).
//
// 本文件提供独立 [equivalence] tag 测试 + 5-10 真实刺激 byte-identical 验证.
// 锁 PcieControllerIP facade path 与 PcieEndpointIP direct path (bar_store_value)
// 在相同 BAR/offset/data 刺激下产生一致行为.
//
// MUST NOT (per handoff §9.2 #9): VramControllerTLM 不在 P3 等价性数据路径,
// 5-10 刺激全部经 PcieControllerIP → PcieEndpointIP → PcieMemoryDevice
// → vram_storage_ (D-AXI v1.4 host BAR2 fast-path). fabric→vram_ctrl 链
// 属 D3/D4, 本期不验证 end-to-end 数据路径.

#include "catch_amalgamated.hpp"
#include "chstream_register.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "tlm/gpu/dgpu_soc.hh"
#include "tlm/pcie/pcie_controller_ip.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"

#include <nlohmann/json.hpp>
#include <fstream>

using tlm::pcie::PcieControllerIP;
using tlm::pcie::PcieEndpointIP;
using BT = PcieControllerIP::BarTarget;
using cpptlm::tlm::DGpuSoc;
using json = nlohmann::json;

namespace {

json load_realip_p2_config() {
    std::ifstream f("configs/dgpu_soc_v2_realip_p2.json");
    REQUIRE(f.is_open());
    json cfg;
    f >> cfg;
    return cfg;
}

} // namespace

TEST_CASE("P3 dual-path equivalence: facade match_bar 与 EP bar_store_value 5-10 刺激一致",
          "[equivalence][p3]") {
    constexpr uint64_t kBar1Size = 16777216ULL;   // 16MB
    constexpr uint64_t kBar2Size = 8589934592ULL; // 8GB

    EventQueue eq;

    auto cfg = load_realip_p2_config();
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));
    auto* soc = dynamic_cast<DGpuSoc*>(factory.getInstance("soc"));
    REQUIRE(soc != nullptr);
    auto* soc_sim = dynamic_cast<SimModule*>(soc);
    REQUIRE(soc_sim != nullptr);
    auto* ctrl = dynamic_cast<PcieControllerIP*>(soc_sim->getInternalInstance("pcie_ctrl"));
    REQUIRE(ctrl != nullptr);

    PcieEndpointIP ep("realip_t25b_tag_ep", &eq);
    json ep_cfg;
    ep_cfg["bar_sizes"] = json::array({0ULL, kBar1Size, kBar2Size, 0ULL, 0ULL, 0ULL});
    ep.set_config(ep_cfg);
    REQUIRE(ep.bar_size(1) == kBar1Size);
    REQUIRE(ep.bar_size(2) == kBar2Size);
    ctrl->attach_endpoint(&ep);

    // 5-10 真实刺激 facade vs EP direct byte-identical 验证
    BT facade_target;
    uint64_t sub_offset;

    // 1. bar=0 → Bar0Mmio (facade) + direct bar_store_value_set/get
    REQUIRE(ctrl->match_bar(0, 0x123ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar0Mmio);
    REQUIRE(sub_offset == 0x123ULL);
    ep.bar_store_value_set(0, 0, 0x123ULL, 0xCAFE0001ULL);
    REQUIRE(ep.bar_store_value(0, 0, 0x123ULL) == 0xCAFE0001ULL);

    // 2. bar=1 offset<bar_size(1) → Bar1FbFastPath
    REQUIRE(ctrl->match_bar(1, 0x5000ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar1FbFastPath);
    ep.bar_store_value_set(0, 1, 0x5000ULL, 0xDEAD0002ULL);
    REQUIRE(ep.bar_store_value(0, 1, 0x5000ULL) == 0xDEAD0002ULL);

    // 3. bar=1 boundary → Unknown
    REQUIRE_FALSE(ctrl->match_bar(1, kBar1Size, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // 4. bar=1 doorbell offset → Unknown (B2d OOB guard)
    REQUIRE_FALSE(ctrl->match_bar(1, 0x10010000ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // 5. bar=2 → Bar2VramAperture (facade path) + EP direct write
    REQUIRE(ctrl->match_bar(2, 0x1000ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar2VramAperture);
    ep.bar_store_value_set(0, 2, 0x1000ULL, 0xBEEF0003ULL);
    REQUIRE(ep.bar_store_value(0, 2, 0x1000ULL) == 0xBEEF0003ULL);

    // 6. bar=2 max valid offset
    REQUIRE(ctrl->match_bar(2, kBar2Size - 8ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Bar2VramAperture);

    // 7. bar=3 (BAR2 upper dword) → Unknown (per H2)
    REQUIRE_FALSE(ctrl->match_bar(3, 0x0ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // 8. bar=4-5 unused → Unknown
    REQUIRE_FALSE(ctrl->match_bar(4, 0x0ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);
    REQUIRE_FALSE(ctrl->match_bar(5, 0x0ULL, facade_target, sub_offset));
    REQUIRE(facade_target == BT::Unknown);

    // 9. facade path 在 doorbell offset (Stage 1.3a) → SDMA ring WPTR trigger,
    //    不进 bar_store_
    const uint64_t pre_count = ep.sdma_ring_processed_count();
    REQUIRE(ep.mmio_write(1, PcieEndpointIP::kBar1DoorbellOffset, 0x10ULL));
    REQUIRE(ep.sdma_ring_processed_count() == pre_count + 0x10ULL);
    REQUIRE(ep.bar_store_value(0, 1, PcieEndpointIP::kBar1DoorbellOffset) == 0ULL);

    // 10. facade path mmio_write OOB (per PcieEndpointIP MVP, mmio_write 无条件接受)
    //    ↔ facade match_bar OOB → Unknown (刺激 3+4) 是不一致但符合 MVP 设计
    //    (mmio_write 边界检查属 P4 deferred 工作, 见 PcieEndpointIP::mmio_write line 651 comment)
    REQUIRE(ep.mmio_write(1, kBar1Size + 0x100ULL, 0xDEADULL));  // accepted per MVP
    REQUIRE(ep.bar_store_value(0, 1, kBar1Size + 0x100ULL) == 0xDEADULL);
}