// test/test_pcie_endpoint_ip_simmodule_refactor.cc
// PcieLinkPhyMuxTLM composite + EP attach_composition 重构测试
// 功能：openspec 2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor Phase 1
//   - 任务 1.1: composite 构造顺序断言 (link→phy→mux + INV-1 成员绑定)
//   - 任务 1.2: composite->tick() 顺序断言 + EP::tick PHY 休眠锁定 (Phase 1 行为零变化)
//   - 任务 1.3: 静态注册表 shim (composite-first + legacy-fallback) + 7 字段 JSON 消费
// 作者 CppTLM Team / 日期 2026-09-15
// 参考: openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/design.md §1
#include "catch_amalgamated.hpp"
#include "core/event_queue.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_link_phy_mux_tlm.hh"

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace tlm::pcie;

// ─────────────────────────────────────────────────────────────────────────────
// Task 1.1: composite 构造顺序 + INV-1 (link_up 立即置位)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieLinkPhyMuxTLM: construction order + INV-1 invariants",
          "[pcie-simmodule-refactor][composite]") {
    EventQueue eq;
    auto* composite = PcieLinkPhyMuxTLM::attach_to_endpoint("ep_composite_ctor", &eq);
    REQUIRE(composite != nullptr);

    // 三个子模块访问器均非 null
    REQUIRE(&composite->link() != nullptr);
    REQUIRE(&composite->phy() != nullptr);
    REQUIRE(&composite->mux() != nullptr);

    // INV-1: phy 持 link_ 指针 + link_up 立即置位
    REQUIRE(composite->phy().link_layer() == &composite->link());
    REQUIRE(composite->phy().is_link_up() == true);
    // set_link_up(true) 后 LTSSM 停在 Detect（link_up 置位, 未走训练序列）
    REQUIRE(composite->phy().state() == LtState::Detect);

    // mux 持 link_ 指针 + phy_initialized=true
    REQUIRE(composite->mux().link_layer() == &composite->link());
    REQUIRE(composite->mux().phy_initialized() == true);
    REQUIRE(composite->mux().mode() == BypassMode::Full);

    PcieLinkPhyMuxTLM::detach_from_endpoint("ep_composite_ctor");
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1.2a: composite->tick() 顺序断言 (phy → link → adapters)
// Phase 1 休眠: 完整实现但 EP::tick 不调用 (设计 §1.3)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieLinkPhyMuxTLM: composite tick order (phy before link)",
          "[pcie-simmodule-refactor][tick-order]") {
    EventQueue eq;
    auto* composite = PcieLinkPhyMuxTLM::attach_to_endpoint("ep_composite_tick", &eq);
    REQUIRE(composite != nullptr);

    REQUIRE(composite->tick_counts(0) == 0u);
    REQUIRE(composite->tick_counts(1) == 0u);

    composite->tick();
    REQUIRE(composite->tick_counts(0) == 1u); // phy tick
    REQUIRE(composite->tick_counts(1) == 1u); // link tick
    REQUIRE(composite->tick_counts(2) == 1u); // adapters tick

    composite->tick();
    REQUIRE(composite->tick_counts(0) == 2u);
    REQUIRE(composite->tick_counts(1) == 2u);
    REQUIRE(composite->tick_counts(2) == 2u);

    PcieLinkPhyMuxTLM::detach_from_endpoint("ep_composite_tick");
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1.2b: Phase 1 PHY 休眠锁定 (EP::tick 不驱动 PHY; Oracle 决策 a)
//   Phase 2 翻转预期时仅需改本断言方向
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: ep.tick() does NOT drive PHY in Phase 1 (dormant)",
          "[pcie-simmodule-refactor][phy-dormant]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_phy_dormant", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg);

    // composite 经 attach_composition 惰性构造
    auto* composite = PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_phy_dormant");
    REQUIRE(composite != nullptr);
    auto* phy = PciePhyDigitalCtrl::for_endpoint("pcie_ep_phy_dormant");
    REQUIRE(phy != nullptr);
    REQUIRE(phy == &composite->phy());

    // 用 start_link_training 使 phy tick 有可见效果（advance_training 每 tick 推进一态）
    phy->start_link_training();
    REQUIRE(phy->state() == LtState::Detect);

    // Phase 1 断言: EP::tick 不调 PHY tick → LTSSM 保持 Detect
    for (int i = 0; i < 4; ++i) {
        ep.tick();
    }
    REQUIRE(phy->state() == LtState::Detect); // 未推进

    // 对照组: composite->tick() 一次后推进到 Polling
    composite->tick();
    REQUIRE(phy->state() == LtState::Polling);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1.3a: 静态注册表 shim (composite-first + legacy-fallback)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: static registry shim routes through composite",
          "[pcie-simmodule-refactor][shim]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_shim", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg);

    auto* composite = PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_shim");
    REQUIRE(composite != nullptr);

    // composite-first: for_endpoint 返回 composite 成员
    REQUIRE(PcieLinkLayer::for_endpoint("pcie_ep_shim") == &composite->link());
    REQUIRE(PciePhyDigitalCtrl::for_endpoint("pcie_ep_shim") == &composite->phy());
    REQUIRE(PcieBypassMux::for_endpoint("pcie_ep_shim") == &composite->mux());

    // EP 访问器也走 composite
    REQUIRE(ep.link_layer() == &composite->link());
    REQUIRE(ep.phy() == &composite->phy());
    REQUIRE(ep.bypass_mux() == &composite->mux());
}

TEST_CASE("PcieLinkPhyMuxTLM: detach + reattach returns fresh instance",
          "[pcie-simmodule-refactor][shim]") {
    EventQueue eq;
    const std::string name = "ep_shim_reattach";

    auto* c1 = PcieLinkPhyMuxTLM::attach_to_endpoint(name, &eq);
    REQUIRE(c1 != nullptr);
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint(name) == c1);

    c1->link().set_fc_capacity(111);
    REQUIRE(c1->link().config().fc_capacity == 111u);

    PcieLinkPhyMuxTLM::detach_from_endpoint(name);
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint(name) == nullptr);

    auto* c2 = PcieLinkPhyMuxTLM::attach_to_endpoint(name, &eq);
    REQUIRE(c2 != nullptr);
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint(name) == c2);
    REQUIRE(c2->link().config().fc_capacity == 256u);  // 默认 256,非 c1 的 111

    PcieLinkPhyMuxTLM::detach_from_endpoint(name);
}

TEST_CASE("PcieEndpointIP: legacy attach_to_endpoint fallback preserved",
          "[pcie-simmodule-refactor][shim]") {
    EventQueue eq;
    const std::string name = "ep_legacy_fallback";

    // legacy 路径: 无 composite 时 for_endpoint 回落到子模块自身注册表
    PcieLinkLayerConfig ll_cfg;
    auto* ll = PcieLinkLayer::attach_to_endpoint(name, &eq, ll_cfg);
    REQUIRE(ll != nullptr);
    REQUIRE(PcieLinkLayer::for_endpoint(name) == ll);

    PcieLinkLayer::detach_from_endpoint(name);
    REQUIRE(PcieLinkLayer::for_endpoint(name) == nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1.3b: 7 字段 JSON 消费 (fc_capacity / fc_init_*/retry/err_inj/bypass)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: JSON link_layer.* 7-field consumption via composite",
          "[pcie-simmodule-refactor][json-consumption]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_json7", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    cfg["link_layer"]["fc_token_bucket_capacity"] = 512;   // 非默认 256
    cfg["link_layer"]["fc_initial_credit_p"] = 8;
    cfg["link_layer"]["fc_initial_credit_np"] = 4;
    cfg["link_layer"]["fc_initial_credit_cpl"] = 2;
    cfg["link_layer"]["retry_buffer_size"] = 1024;         // 非默认 4096
    cfg["link_layer"]["link_error_injection_enabled"] = true;
    cfg["link_layer"]["bypass_mode"] = "Bypass";
    ep.set_config(cfg);

    auto* composite = PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_json7");
    REQUIRE(composite != nullptr);

    // fc capacity 经 set_fc_capacity 生效 (Oracle 复审条件 1)
    auto* ll = PcieLinkLayer::for_endpoint("pcie_ep_json7");
    REQUIRE(ll != nullptr);
    REQUIRE(ll == &composite->link());
    REQUIRE(ll->fc().bucket(0).capacity() == 512u);

    // fc initial credits (update_fc 路径)
    REQUIRE(ll->fc().bucket(0).token_count(FcTokenBucket::Type::Posted) == 8u);
    REQUIRE(ll->fc().bucket(0).token_count(FcTokenBucket::Type::NonPosted) == 4u);
    REQUIRE(ll->fc().bucket(0).token_count(FcTokenBucket::Type::Completion) == 2u);

    // retry buffer 配置记录 (cfg_.retry_buffer_size 可见性)
    REQUIRE(ll->config().retry_buffer_size == 1024u);

    // error injection 经 set_link_error_injection_enabled 生效
    REQUIRE(ll->error_injector().enabled == true);

    // bypass_mode 经 mux.apply_mode 生效
    REQUIRE(composite->mux().mode() == BypassMode::Bypass);
}

TEST_CASE("PcieEndpointIP: link_layer.enabled=false leaves for_endpoint null",
          "[pcie-simmodule-refactor][json-consumption]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_ll_off", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = false;
    cfg["link_layer"]["bypass_mode"] = "Bypass"; // R-A: 同现不崩溃
    ep.set_config(cfg);

    REQUIRE(PcieLinkLayer::for_endpoint("pcie_ep_ll_off") == nullptr);
    REQUIRE(PciePhyDigitalCtrl::for_endpoint("pcie_ep_ll_off") == nullptr);
    REQUIRE(PcieBypassMux::for_endpoint("pcie_ep_ll_off") == nullptr);
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_ll_off") == nullptr);
    REQUIRE(ep.link_layer() == nullptr);
    REQUIRE(ep.phy() == nullptr);
    REQUIRE(ep.bypass_mux() == nullptr);
}

TEST_CASE("PcieEndpointIP: no link_layer block leaves for_endpoint null",
          "[pcie-simmodule-refactor][json-consumption]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_no_ll", &eq);
    ep.init();

    json cfg; // 空 params — 无 link_layer 块
    ep.set_config(cfg);

    REQUIRE(PcieLinkLayer::for_endpoint("pcie_ep_no_ll") == nullptr);
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_no_ll") == nullptr);
}

TEST_CASE("PcieEndpointIP: reconfigure enabled=false detaches composite (R-B)",
          "[pcie-simmodule-refactor][json-consumption]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_recfg", &eq);
    ep.init();

    // 第一次配置: enabled=true → composite 构造
    json cfg1;
    cfg1["link_layer"]["enabled"] = true;
    ep.set_config(cfg1);
    auto* composite = PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_recfg");
    REQUIRE(composite != nullptr);
    REQUIRE(PcieLinkLayer::for_endpoint("pcie_ep_recfg") == &composite->link());

    // 第二次配置: enabled=false → composite 必须 detach (R-B 修补)
    json cfg2;
    cfg2["link_layer"]["enabled"] = false;
    ep.set_config(cfg2);
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_recfg") == nullptr);
    REQUIRE(PcieLinkLayer::for_endpoint("pcie_ep_recfg") == nullptr);
    REQUIRE(PciePhyDigitalCtrl::for_endpoint("pcie_ep_recfg") == nullptr);
    REQUIRE(PcieBypassMux::for_endpoint("pcie_ep_recfg") == nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 1.4 支撑: EP dtor 自动 detach (防 stale 指针跨 TEST_CASE)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: destructor detaches composite from registry",
          "[pcie-simmodule-refactor][shim]") {
    const std::string name = "pcie_ep_dtor_cleanup";
    {
        EventQueue eq;
        PcieEndpointIP ep(name, &eq);
        ep.init();
        json cfg;
        cfg["link_layer"]["enabled"] = true;
        ep.set_config(cfg);
        REQUIRE(PcieLinkPhyMuxTLM::for_endpoint(name) != nullptr);
    } // ep dtor → detach_from_endpoint
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint(name) == nullptr);
}
