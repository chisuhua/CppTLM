// test/test_cep_chstream_bidirectional_macro.cc
// `REGISTER_CHSTREAM_BIDIRECTIONAL` 宏的**运行期**契约测试(Phase 4 · 判据 CC-04 ③)
//
// per openspec/changes/cpptlm-cep-cohif-carrier/tasks.md task 4.1 ~ 4.3
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ⚠️ **与编译期判据的分工**(per 判据纪律「编译期锁不得只以『编译红』充当 ctest 判据」):
//   · **运行期**(本文件):断言宏「**两件都做**」—— `registerObject` 让 JSON 顶层能
//     实例化该类型;`registerBidirectionalPortAdapter` 让 adapter 工厂能查到它。
//   · **编译期**(`scripts/test/cep_chstream_bidirectional_macro_reverse_verify.sh`):
//     断言 `static_assert(n > 1)` 有牙(N=1 ⇒ 编译失败且原因命中该断言)。
#include "catch_amalgamated.hpp"

// ⚠️ 宏在 `chstream_register.hh`(与既有 `REGISTER_CHSTREAM` / `REGISTER_CHSTREAM_DUAL` 同处)
#include "chstream_register.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "framework/chstream_adapter_factory.hh"
#include "tlm/cep/cohif_noc_bridge.hh"
#include "tlm/cep/protocol_engine_base.hh"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

using json = nlohmann::json;

namespace {

// ── 本文件专属的 PE 派生类(避免与其它测试的类型名/注册表项冲突)──
class MacroProbePE : public tlm::cep::ProtocolEngineBase<2> {
public:
    using Base = tlm::cep::ProtocolEngineBase<2>;
    MacroProbePE(const std::string& n, EventQueue* eq) : Base(n, eq) {}
    void on_request(std::uint64_t, tlm::cep::Perm, std::uint64_t) override {}
    void on_completion(std::uint64_t, std::uint64_t, tlm::cep::Perm, bool) override {}
    void on_snoop(std::uint64_t, tlm::cep::SnoopType, std::uint64_t) override {}
    void on_snoop_resp(std::uint64_t, bool, tlm::cep::LineState, bool, std::uint64_t,
                       std::uint64_t) override {}
    void on_writeback(std::uint64_t, std::uint64_t, bool) override {}
    void on_forward_data(std::uint64_t, std::uint64_t, std::uint64_t, tlm::cep::Perm,
                         std::uint64_t) override {}
    void on_back_invalidate(std::uint64_t, tlm::cep::SnoopType, std::uint64_t) override {}
    void on_address_resolve(std::uint64_t, std::uint64_t,
                            tlm::cep::AddressResolveResult& out) override {
        out.home_node = 0;
        out.coherent = false;
    }
    void on_barrier(std::uint64_t, std::uint64_t) override {}
    void on_snoop_completion_ack(std::uint64_t, std::uint64_t, std::uint64_t) override {}
};

constexpr const char* kProbeType = "MacroProbePE";

void register_probe() {
    REGISTER_CHSTREAM_BIDIRECTIONAL(MacroProbePE, bundles::CohIfBundle, 2);
}

}  // namespace

TEST_CASE("CC-04 ③ 宏的 adapter 注册生效(多端口 + 端口数一致)", "[cep][chstream-macro]") {
    register_probe();

    auto& factory = ChStreamAdapterFactory::get();
    // ⚠️ `isMultiPort` 要求 `port_count > 1` ⇒ N=2 ⇒ true
    //    (N=1 会是 false ⇒ ModuleFactory 落 single 分支 ⇒ `bind_ports` 空 shim)
    REQUIRE(factory.isMultiPort(kProbeType));
    REQUIRE(factory.getPortCount(kProbeType) == 2u);
}

TEST_CASE("CC-04 ③ 宏的 registerObject 生效(JSON 顶层可实例化该类型)", "[cep][chstream-macro]") {
    register_probe();

    json cfg;
    cfg["modules"] = json::array();
    cfg["connections"] = json::array();
    cfg["modules"].push_back({{"name", "probe0"}, {"type", kProbeType}});

    EventQueue eq;
    ModuleFactory factory(&eq);
    REQUIRE(factory.instantiateAll(cfg));
}

TEST_CASE("CC-04 ③ 宏的端口数与模块的端口数一致(宏参数 N = ProtocolEngineBase 的 N)",
          "[cep][chstream-macro]") {
    register_probe();

    auto& factory = ChStreamAdapterFactory::get();
    REQUIRE(factory.getPortCount(kProbeType) == MacroProbePE::kPortCount);
    REQUIRE(MacroProbePE::kPortCount == 2u);
}

TEST_CASE("CohIfBundle 的方向映射与 5→3 消息类映射同源(不漂移)", "[cep][chstream-macro]") {
    // `BundleDirectionTraits<CohIfBundle>::packet_type`(PKT_REQ/PKT_RESP)必须与
    // `CohIfNoCProtocolBridge::category_of`(CATEGORY_REQUEST/CATEGORY_RESPONSE)**同源**
    // ⇒ 否则「网络类别」与「Packet 方向」会不一致(per `cohif_direction_traits.hh` 的口径)
    using Traits = cpptlm::BundleDirectionTraits<bundles::CohIfBundle>;

    const std::pair<bundles::CohIfFlowId, bool> resp_flows[] = {
        {bundles::CohIfFlowId::Completion, true}, {bundles::CohIfFlowId::SnoopResp, true},
        {bundles::CohIfFlowId::Request, false},   {bundles::CohIfFlowId::Snoop, false},
        {bundles::CohIfFlowId::Writeback, false}};

    for (const auto& [flow, is_resp] : resp_flows) {
        bundles::CohIfBundle b;
        b.flow_id = flow;
        const bool traits_says_resp = (Traits::packet_type(b) == PKT_RESP);
        INFO("flow 方向判定与预期不符");
        REQUIRE(traits_says_resp == is_resp);

        // 与桥的 5→3 映射对照
        const std::uint8_t cat = tlm::cep::CohIfNoCProtocolBridge::category_of(flow);
        const bool map_says_resp =
            (cat == bundles::NoCFlitBundle::CATEGORY_RESPONSE);
        INFO("方向 trait 与 5→3 消息类映射漂移");
        REQUIRE(map_says_resp == is_resp);
    }
}
