// test/cep_test_protocol_engine.hh
// 测试用 ProtocolEngine 派生类(Phase 3 / Phase 4 共用)
//
// 功能:提供一个**最小的** `ProtocolEngineBase` 派生类,供
//       ① `test/test_cep_protocol_bridge_contracts.cc`(Phase 3)
//       ② `scripts/test/cep_chstream_bidirectional_macro_probe.cc`(Phase 4 反向 fixture)
//       共用 ⇒ 避免两份重复的 10 个 `on_*` 空实现。
// 作者: CppTLM Team / 日期: 2026-10-28
#ifndef CEP_TEST_PROTOCOL_ENGINE_HH
#define CEP_TEST_PROTOCOL_ENGINE_HH

#include "tlm/cep/protocol_engine_base.hh"

#include <cstdint>
#include <string>

// ⚠️ **必须与 `ProtocolEngineBase` 同命名空间族**(`tlm::cep`)——
//    `ProtocolEngineBase` 定义在 `tlm::cep`,而 `cpptlm::cep` 是**另一个**
//    命名空间(测试辅助 `read_file` / `repo_root` 所在)⇒ 放在 `cpptlm::cep::testing`
//    会**找不到**基类(实测:「expected template-name before '<' token」)。
namespace tlm::cep::testing {

/// 端口数固定 **2**(与 `REGISTER_CHSTREAM_BIDIRECTIONAL` 的正控 N=2 一致)
class TestProtocolEngine : public ProtocolEngineBase<2> {
public:
    using Base = ProtocolEngineBase<2>;

    TestProtocolEngine(const std::string& n, EventQueue* eq) : Base(n, eq) {}
    ~TestProtocolEngine() override = default;

    // ── canonical 10 流(全部 no-op;仅满足纯虚接口)──────────────────
    void on_request(std::uint64_t, Perm, std::uint64_t) override {}
    void on_completion(std::uint64_t, std::uint64_t, Perm, bool) override {}
    void on_snoop(std::uint64_t, SnoopType, std::uint64_t) override {}
    void on_snoop_resp(std::uint64_t, bool, LineState, bool, std::uint64_t,
                       std::uint64_t) override {}
    void on_writeback(std::uint64_t, std::uint64_t, bool) override {}
    void on_forward_data(std::uint64_t, std::uint64_t, std::uint64_t, Perm,
                         std::uint64_t) override {}
    void on_back_invalidate(std::uint64_t, SnoopType, std::uint64_t) override {}
    void on_address_resolve(std::uint64_t, std::uint64_t, AddressResolveResult& out) override {
        out.home_node = 0;
        out.coherent = false;
    }
    void on_barrier(std::uint64_t, std::uint64_t) override {}
    void on_snoop_completion_ack(std::uint64_t, std::uint64_t, std::uint64_t) override {}
};

}  // namespace tlm::cep::testing

#endif  // CEP_TEST_PROTOCOL_ENGINE_HH
