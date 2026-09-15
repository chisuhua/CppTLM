// test/test_pcie_endpoint_ip_simmodule_refactor.cc
// PcieLinkPhyMuxTLM composite + EP attach_composition 重构测试
// 功能：openspec 2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor Phase 1+2
//   - 任务 1.1: composite 构造顺序断言 (link→phy→mux + INV-1 成员绑定)
//   - 任务 1.2: composite->tick() 顺序断言 + EP::tick PHY 驱动 (Phase 2 激活)
//   - 任务 1.3: 静态注册表 shim (composite-first + legacy-fallback) + 7 字段 JSON 消费
//   - 任务 2.1: EP 基类切换 SimModule + composite 单一所有权 (internal_factory)
//   - 任务 2.2: composite 17 端口内部可达性 (R4 决策 C, getInternalOutputPort)
//   - 任务 2.3: axislavein 桥接路径保留 (R4=C5 锁定)
// 作者 CppTLM Team / 日期 2026-09-15
// 参考: openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/{design,tasks}.md
#include "catch_amalgamated.hpp"
#include "bundles/axi4_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "core/sim_module.hh"
#include "framework/chstream_adapter_factory.hh"
#include "tlm/pcie/host_bypass_tlm.hh"
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/pcie/pcie_link_phy_mux_tlm.hh"

#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace tlm::pcie;
using namespace bundles;

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
// Task 2.1.5: Phase 2 PHY 驱动迁移 (ep.tick() 驱动 composite → PHY 推进)
//   Phase 1 断言方向已翻转 (Oracle 决策 a 落地)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: ep.tick() drives PHY in Phase 2 (composite activation)",
          "[pcie-simmodule-refactor][phy-migration]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_phy_dormant", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg);

    auto* composite = PcieEndpointIP::find_composite("pcie_ep_phy_dormant");
    REQUIRE(composite != nullptr);
    auto* phy = PciePhyDigitalCtrl::for_endpoint("pcie_ep_phy_dormant");
    REQUIRE(phy != nullptr);
    REQUIRE(phy == &composite->phy());

    // 用 start_link_training 使 phy tick 有可见效果（advance_training 每 tick 推进一态）
    phy->start_link_training();
    REQUIRE(phy->state() == LtState::Detect);

    // Phase 2 断言 (方向翻转): ep.tick() 经 composite->tick() 驱动 PHY → LTSSM 推进
    // (训练序列 Detect→Polling→Configuration→L0, 每 tick 一态)
    ep.tick();
    REQUIRE(phy->state() == LtState::Polling);
    ep.tick();
    REQUIRE(phy->state() == LtState::Configuration);
    ep.tick();
    REQUIRE(phy->state() == LtState::L0);
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

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2 注册 (测试二进制不自动触发 REGISTER_CHSTREAM; 幂等)
// ─────────────────────────────────────────────────────────────────────────────
static const int s_pcie_simmodule_refactor_registered = []() {
    ModuleFactory::registerObject<tlm::pcie::PcieLinkPhyMuxTLM>("PcieLinkPhyMuxTLM");
    ModuleFactory::registerModule<tlm::pcie::PcieEndpointIP>("PcieEndpointIP");
    ChStreamAdapterFactory::get()
        .registerMultiPortAdapter<tlm::pcie::PcieLinkPhyMuxTLM,
                                  bundles::PcieTlpBundle, bundles::PcieTlpBundle, 17>(
            "PcieLinkPhyMuxTLM");
    return 0;
}();

// ─────────────────────────────────────────────────────────────────────────────
// Task 2.1: EP 基类切换 (INV-3, R2 双注册清理)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: SimModule inheritance (Phase 2 base switch)",
          "[pcie-simmodule-refactor][inheritance]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_simmodule", &eq);
    auto* as_sim = dynamic_cast<SimModule*>(&ep);
    auto* as_chstrm = dynamic_cast<ChStreamModuleBase*>(&ep);
    REQUIRE(as_sim != nullptr);     // Phase 2: EP 是 SimModule
    REQUIRE(as_chstrm == nullptr);  // Phase 2: 不再是 ChStreamModuleBase

    // R2 双注册清理: module registry 命中, object registry 不含
    auto mods = ModuleFactory::getRegisteredModuleTypes();
    REQUIRE(std::find(mods.begin(), mods.end(), "PcieEndpointIP") != mods.end());
    auto objs = ModuleFactory::getRegisteredObjectTypes();
    REQUIRE(std::find(objs.begin(), objs.end(), "PcieEndpointIP") == objs.end());
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2.1: composite 单一所有权 (internal_factory, EP ctor/dtor 维护 instances_)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: composite owned by internal_factory (Phase 2)",
          "[pcie-simmodule-refactor][composite-ownership-single]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_own", &eq);
    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg);

    // composite 经 find_composite 扫描 EP instances_ + internal_factory 命中
    auto* composite = PcieEndpointIP::find_composite("pcie_ep_own");
    REQUIRE(composite != nullptr);

    // EP 实例静态列表含 ep
    bool found = false;
    for (auto* p : PcieEndpointIP::instances_for_test()) {
        if (p == &ep) { found = true; break; }
    }
    REQUIRE(found);

    // for_endpoint 走 internal_factory 路径命中同一 composite
    REQUIRE(PcieLinkPhyMuxTLM::for_endpoint("pcie_ep_own") == composite);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2.1.5: ep.tick() 驱动 composite tick (tick_counts 经 EP 推进)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: ep.tick() drives composite tick in Phase 2",
          "[pcie-simmodule-refactor][phy-migration]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_tick_phase2", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg);

    auto* composite = PcieEndpointIP::find_composite("pcie_ep_tick_phase2");
    REQUIRE(composite != nullptr);
    REQUIRE(composite->tick_counts(0) == 0u);

    ep.tick();
    REQUIRE(composite->tick_counts(0) == 1u);  // phy tick 经 EP::tick 推进
    REQUIRE(composite->tick_counts(1) == 1u);  // link tick
    REQUIRE(composite->tick_counts(2) == 1u);  // adapters tick
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2.2: composite 17 端口内部可达性 (R4 决策 C, getInternalOutputPort)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: composite 17-port internal reachability (Phase 2)",
          "[pcie-simmodule-refactor][composite-reachability]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_reach", &eq);
    ep.init();

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg);

    // 经 getInternalOutputPort/getInternalInputPort 命中 internal_factory + Step 7 mirror
    for (unsigned i = 0; i < 17; ++i) {
        const std::string idx = std::to_string(i);
        auto* master = ep.getInternalOutputPort("pcie_ep_reach_lpm.resp_out[" + idx + "]");
        auto* slave = ep.getInternalInputPort("pcie_ep_reach_lpm.req_in[" + idx + "]");
        REQUIRE(master != nullptr);  // per design §3.3 + R4=C
        REQUIRE(slave != nullptr);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2.3: axislavein 桥接路径保留 (R4=C5 锁定: 429327d 程序化桥接不依赖 JSON connection)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: axislavein bridge path intact (Phase 2 R4=C5)",
          "[pcie-simmodule-refactor][axislavein-bridge]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_bridge", &eq);
    HostBypassTLM hb("hb_bridge", &eq);
    ep.init();

    json cfg;
    cfg["axi_adapter"] = json::object();
    cfg["axi_adapter"]["axi4_mapper_inject"] = true;
    cfg["link_layer"]["enabled"] = true;
    cfg["link_layer"]["bypass_mode"] = "Bypass";
    ep.set_config(cfg);
    ep.on_config_loaded();
    hb.init();
    hb.attach_to_endpoint(&ep);

    // 构造 AXI cfg write 经 HostBypass 程序化桥接 (非 JSON connection)
    Axi4Bundle wreq;
    wreq.awid.write(0x10);
    wreq.awaddr.write(0x04);  // 配置空间偏移 (Command Register)
    wreq.awlen.write(0);
    wreq.awsize.write(2);
    wreq.awburst.write(1);
    wreq.wdata.write(0x0007);
    wreq.wstrb.write(0xF);
    wreq.wlast.write(1);
    REQUIRE(hb.axi_master_req(wreq) == true);
    hb.set_axi_master_ready(true);

    for (int i = 0; i < 100 && (hb.axi_master_req_valid() || hb.axi_outstanding_wr() > 0); ++i) {
        ep.tick();
        hb.tick();
    }

    // 桥接路径完整 (R4=C 不依赖 JSON connection): EP 真实消费请求
    REQUIRE(ep.vf_pool().config_of(0).read(0x04) == 0x0007u);
    REQUIRE(hb.axi_master_resp_valid() == true);
    REQUIRE(hb.axi_master_resp_data().bid.read() == 0x10u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2.1 (决策 1): EP 非虚兼容方法转发到 composite (14+ 测试零修改保证)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: non-virtual compat accessors forward to composite",
          "[pcie-simmodule-refactor][compat-forward]") {
    EventQueue eq;
    PcieEndpointIP ep("pcie_ep_fwd", &eq);
    ep.init();

    // 无 composite (无 link_layer 块): 转发返回空/默认, 不崩溃
    REQUIRE(ep.num_ports() == 17u);
    REQUIRE(ep.all_ports_have_adapter() == false);
    REQUIRE(ep.get_adapter(0) == nullptr);
    ep.set_stream_adapter(static_cast<cpptlm::StreamAdapterBase*>(nullptr));

    json cfg;
    cfg["link_layer"]["enabled"] = true;
    ep.set_config(cfg);

    // composite 就绪后转发到 composite 持有的 adapter 数组
    auto* composite = PcieEndpointIP::find_composite("pcie_ep_fwd");
    REQUIRE(composite != nullptr);
    REQUIRE(ep.num_ports() == PcieLinkPhyMuxTLM::NUM_TLP_PORTS);
    for (unsigned i = 0; i < 17; ++i) {
        REQUIRE(ep.get_adapter(i) == composite->get_adapter(i));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Task 2.1 (决策 3): simulate_instantiate 双格式 (Module entry / 直接 params)
// ─────────────────────────────────────────────────────────────────────────────
TEST_CASE("PcieEndpointIP: simulate_instantiate accepts both entry and params formats",
          "[pcie-simmodule-refactor][dual-format]") {
    SECTION("direct params format (on_config_loaded forwarding)") {
        EventQueue eq;
        PcieEndpointIP ep("pcie_ep_direct", &eq);
        ep.init();
        json params;
        params["link_layer"]["enabled"] = true;
        ep.set_config(params);
        REQUIRE(PcieEndpointIP::find_composite("pcie_ep_direct") != nullptr);
    }

    SECTION("module entry format (ModuleFactory Step 4.5)") {
        EventQueue eq;
        PcieEndpointIP ep("pcie_ep_entry", &eq);
        ep.init();
        json entry;
        entry["name"] = "pcie_ep_entry";
        entry["type"] = "PcieEndpointIP";
        entry["params"]["link_layer"]["enabled"] = true;
        ep.simulate_instantiate(entry);
        REQUIRE(PcieEndpointIP::find_composite("pcie_ep_entry") != nullptr);
        REQUIRE(ep.link_layer() != nullptr);
    }

    SECTION("no link_layer block leaves composite inactive") {
        EventQueue eq;
        PcieEndpointIP ep("pcie_ep_no_ll_entry", &eq);
        ep.init();
        json entry;
        entry["name"] = "pcie_ep_no_ll_entry";
        entry["type"] = "PcieEndpointIP";
        ep.simulate_instantiate(entry);
        REQUIRE(PcieEndpointIP::find_composite("pcie_ep_no_ll_entry") == nullptr);
    }
}
