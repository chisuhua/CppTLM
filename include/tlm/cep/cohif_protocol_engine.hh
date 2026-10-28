// include/tlm/cep/cohif_protocol_engine.hh
// CohIf 协议引擎的**可注册**形态
//
// 功能:为 `cpptlm-cep-cohif-carrier` Phase 5 提供一个能被 `ModuleFactory` 实例化的
//       CohIf 协议引擎(判据 CC-04 ①②③ 的装配载体)。
// 作者: CppTLM Team / 日期: 2026-10-28
//
// ── 为什么需要这个类 ────────────────────────────────────────────────
// `ProtocolEngineBase<N>` / `NullProtocolEngine<N>` 都是**模板** ⇒
// ① `#mod_type` 字符串化会得到 `"tlm::cep::NullProtocolEngine<2>"`(含尖括号)
//    —— 本仓注册表的惯例是**短名**(如 `"CoherentXBarTLM"`,见 `REGISTER_CHSTREAM`);
// ② `NullProtocolEngine` 的**语义**是「协议引擎的默认实现」(per `INV-CEP-07-IC2`),
//    它是**库内的实现细节**,不应直接作为 JSON 里的模块类型名。
//
// ⇒ 本类只做一件事:**给 `NullProtocolEngine<2>` 一个非模板的、可注册的、可观测的
//    外壳**(计数器在基类 ⇒ 端到端断言可查其收到的流)。
//
// ⚠️ **本类不引入任何新的协议语义** —— 10 流行为**完全继承** `NullProtocolEngine`
//    (per `07 号 §6.4` 的逐流口径)。⇒ **不是**重复实现。
//
// ⚠️ **端口数固定为 2** —— `ProtocolEngineBase<N>` 的 `N` 是**模板参数**
//    (per Phase 2 的取舍)⇒ 本类固定 `N = 2`(满足判据 CC-04 ③ 的「N ≥ 2」)。
#ifndef TLM_CEP_COHIF_PROTOCOL_ENGINE_HH
#define TLM_CEP_COHIF_PROTOCOL_ENGINE_HH

#include "tlm/cep/protocol_engine_base.hh"

#include <string>

namespace tlm::cep {

/// CohIf 协议引擎(**Null 语义**的可注册形态)
class CohIfNullProtocolEngine : public NullProtocolEngine<2> {
public:
    using Base = NullProtocolEngine<2>;

    CohIfNullProtocolEngine(const std::string& n, EventQueue* eq) : Base(n, eq) {}
    ~CohIfNullProtocolEngine() override = default;
};

}  // namespace tlm::cep

#endif  // TLM_CEP_COHIF_PROTOCOL_ENGINE_HH
