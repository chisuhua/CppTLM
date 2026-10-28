// test/cep_cohif_assembly_helper.hh
// CohIf **JSON 装配端到端**判据的共享实现
//
// 功能:把「一次 CohIf 事务**穿过** adapter 抵达对端」这条判据实现为**可复用**的
//       辅助函数 + 结局分类枚举,供两处消费:
//         ① `test/test_cep_cohif_json_assembly.cc`(Phase 5 的 JSON 装配用例)
//         ② `test/test_cep_cohif_carrier.cc`(Phase 6 的 CC-04 用例)
//       —— **同一份实现**,避免两份判据漂移。
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 判据纪律:结局必须**分类**,不能只返回 bool ──────────────────────
// 反向 fixture 若只断言「没抵达」,则「**装配被拒**」也会被判红 ⇒ fixture
// 「因错误的原因红」。⇒ 辅助函数返回 `ReachOutcome` 枚举,调用方断言
// **目标结局**(`NotArrived` / `Arrived`),而不是 `!= Arrived`。
#ifndef CEP_COHIF_ASSEMBLY_HELPER_HH
#define CEP_COHIF_ASSEMBLY_HELPER_HH

#include "cep_adr_parser.hh"

#include "core/event_queue.hh"
#include "core/module_factory.hh"
#include "tlm/cep/protocol_engine_base.hh"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <string>

namespace cpptlm {
namespace cep {
namespace testing {

/// 端到端判据的**结果分类**
enum class ReachOutcome {
    AssemblyFailed,   ///< 装配本身失败(配置被拒)—— **不是**端到端判据的目标
    NotInstantiated,  ///< 装配成功但拿不到模块实例
    NotArrived,       ///< 装配成功、模块在位,但事务**没有**抵达对端 ← 反向 fixture 期望
    Arrived,          ///< 事务**穿过** adapter 抵达对端 ← 正控期望
};

/// 造一个「待发送」的 CohIf 事务
inline bundles::CohIfBundle make_cohif_request() {
    bundles::CohIfBundle tx;
    tx.flow_id = bundles::CohIfFlowId::Request;
    tx.transaction_id.write(0xC0FFEE1234ULL);
    tx.address.write(0x1000);
    tx.data.write(0xABCD);
    tx.size.write(64);
    tx.perm.write(bundles::CohIfBundle::PERM_READ);
    return tx;
}

/// 加载最小 PE 拓扑配置 —— 并把 `protocol_instances` 的**相对路径**换成
/// **绝对路径**(配置的相对路径以「仓库根」为基准;测试的 CWD 不固定 ⇒ 必须绝对化)
inline nlohmann::json load_pe_topology() {
    const auto path = repo_root() / "configs" / "cep_cohif_pe_minimal.json";
    std::ifstream in(path);
    if (!in.good())
        throw std::runtime_error("无法打开最小 PE 拓扑配置: " + path.string());
    nlohmann::json cfg = nlohmann::json::parse(in);

    cfg["cep"]["protocol_instances"] =
        (repo_root() / "include" / "tlm" / "cep" / "protocol_instances.json").string();
    return cfg;
}

/// 端到端判据 —— 返回事务是否抵达对端(带失败原因分类)
///
/// ⚠️ 自建 `EventQueue` + `ModuleFactory`(`ModuleFactory` **不可拷贝赋值**
///    —— 内含 `unique_ptr`)⇒ 辅助函数必须**自持**装配上下文。
inline ReachOutcome cohif_transaction_reaches_peer(const nlohmann::json& cfg) {
    EventQueue eq;
    ModuleFactory factory(&eq);
    if (!factory.instantiateAll(cfg))
        return ReachOutcome::AssemblyFailed;

    using PE = tlm::cep::ProtocolEngineBase<2>;
    auto* pe0 = dynamic_cast<PE*>(factory.getInstanceForTesting("pe0"));
    auto* pe1 = dynamic_cast<PE*>(factory.getInstanceForTesting("pe1"));
    if (!pe0 || !pe1)
        return ReachOutcome::NotInstantiated;

    // pe0 产出一个 CohIf 事务(走 egress 通道)
    pe0->resp_out()[0].write(make_cohif_request());

    // ① pe0 的 tick:消费 ingress(空)+ 驱动 adapter(发出 egress)
    pe0->tick();
    // ② pe1 的 tick:消费 ingress ⇒ 分派到 canonical 流
    pe1->tick();

    auto* engine = dynamic_cast<tlm::cep::NullProtocolEngine<2>*>(pe1);
    if (!engine)
        return ReachOutcome::NotInstantiated;
    const auto& ctr = engine->counters();
    // ⚠️ 计数器索引是 **canonical 10 流**(`CanonicalFlow`),不是 `CohIfFlowId`
    const auto idx = static_cast<std::size_t>(tlm::cep::CanonicalFlow::Request);
    return ctr.per_flow[idx] == 1u ? ReachOutcome::Arrived : ReachOutcome::NotArrived;
}

}  // namespace testing
}  // namespace cep
}  // namespace cpptlm

#endif  // CEP_COHIF_ASSEMBLY_HELPER_HH
