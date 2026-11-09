// test/test_dual_port_inbound_dispatch.cc
// 双端口 StreamAdapter —— **入向按端口组路由**的判据
//
// 功能描述:验证 `DualPortStreamAdapter` 的入向包按**端口组**投递
//          (组 0 = PE 侧 ⇒ `pe_req_in()` · 组 1 = Network 侧 ⇒ `net_resp_in()`)
// 作者: CppTLM Team
// 日期: 2026-11-09
//
// ── 为什么有这组判据(per 台账 Entry #305 ③ R-2 / Entry #307 ③)────────────
// 框架的入向路径固定为 `ChStreamTargetPort::recvReq`
// (`include/core/chstream_port.hh:70`) ⇒ 只调**双参** `process_request_input(pkt, port_idx_)`;
// 基类双参版(`include/core/stream_adapter_base.hh:82-84`)**默认回退单参版**。
// ⇒ 修复前只 override 单参版 ⇒ `port_idx` 丢失 ⇒ 入向包**恒落 PE 侧**
// ⇒ `net_resp_in()` 永不可达 ⇒ CEP 拓扑级回程腿不可达。
//
// ⚠️ **判据形态**:每条都走**框架真实入口**(`ChStreamTargetPort::recvReq`)
//    ⇒ ⛔ 不用「直接写 adapter 成员」冒充入向判据;
//    且每条都有**能使其变红**的反向 fixture(变异后红因为「net 侧未收到」)。
#include <string>

#include "bundles/cache_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/chstream_port.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "framework/dual_port_stream_adapter.hh"

#include <catch2/catch_all.hpp>

// ============================================================
// Mock 双端口模块(端口面契约与 `NICTLM` / `StubCacheEndpointTLM` 一致:
//   pe_req_in() · pe_resp_out() · net_req_out() · net_resp_in())
//
// ⚠️ **必须放匿名命名空间**:仓内另一份测试(`test/test_dual_port_adapter.cc`)曾把
//    **同名**的 mock 类放在**全局命名空间** ⇒ 两个 TU 的同名类 = **ODR 违反**
//    ⇒ 链接期谁赢取决于 TU 顺序 ⇒ 内联成员函数可能绑到**另一份**布局
//    ⇒ 表现为「适配器写进 A 成员、测试读 B 成员」的**静默内存错位**。
//    ⇒ 本文件的 mock 一律匿名命名空间(与仓库其它 mock 的隔离纪律一致)。
// ============================================================
namespace {

class MockDualPortModule : public ChStreamModuleBase {
private:
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> pe_req_in_;
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> pe_resp_out_;
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle> net_req_out_;
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle> net_resp_in_;

    cpptlm::StreamAdapterBase* adapter_ = nullptr;

public:
    MockDualPortModule(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}

    std::string get_module_type() const override { return "MockDualPortModule"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override { adapter_ = adapter; }

    void tick() override {
        if (adapter_)
            adapter_->tick();
    }

    void do_reset(const ResetConfig&) override {
        pe_req_in_.reset();
        pe_resp_out_.reset();
        net_req_out_.reset();
        net_resp_in_.reset();
    }

    // PE 侧
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle>& pe_req_in() { return pe_req_in_; }
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle>& pe_resp_out() { return pe_resp_out_; }

    // Network 侧
    cpptlm::OutputStreamAdapter<bundles::CacheReqBundle>& net_req_out() { return net_req_out_; }
    cpptlm::InputStreamAdapter<bundles::CacheRespBundle>& net_resp_in() { return net_resp_in_; }
};

using TestDualAdapter =
    cpptlm::DualPortStreamAdapter<MockDualPortModule, bundles::CacheReqBundle,
                                  bundles::CacheRespBundle, bundles::CacheReqBundle,
                                  bundles::CacheRespBundle>;

/// 构造「双端口适配器 + 两侧目标端口」的最小装配
struct DualPortFixture {
    EventQueue eq;
    MockDualPortModule mod{"dual_dispatch", &eq};
    TestDualAdapter adapter{&mod};

    cpptlm::ChStreamInitiatorPort pe_req_out{"pe_req_out", &eq};
    cpptlm::ChStreamTargetPort pe_resp_in{"pe_resp_in", &adapter, &eq, 0};
    cpptlm::ChStreamInitiatorPort pe_resp_out{"pe_resp_out", &eq};
    cpptlm::ChStreamTargetPort pe_req_in{"pe_req_in", &adapter, &eq, 0};

    cpptlm::ChStreamInitiatorPort net_req_out{"net_req_out", &eq};
    cpptlm::ChStreamTargetPort net_resp_in{"net_resp_in", &adapter, &eq, 1};
    cpptlm::ChStreamInitiatorPort net_resp_out{"net_resp_out", &eq};
    cpptlm::ChStreamTargetPort net_req_in{"net_req_in", &adapter, &eq, 1};

    DualPortFixture() {
        adapter.bind_pe_ports(&pe_req_out, &pe_resp_in, &pe_resp_out, &pe_req_in);
        adapter.bind_net_ports(&net_req_out, &net_resp_in, &net_resp_out, &net_req_in);
        mod.set_stream_adapter(&adapter);
    }
};

/// 经**框架真实入口**(`ChStreamTargetPort::recvReq`)投递一个包
/// ⚠️ `recvReq` 内部会把包还给 `PacketPool` ⇒ 调用方**不得**再 release
void deliver(cpptlm::ChStreamTargetPort& target, const bundles::CacheRespBundle& payload,
             PacketType type) {
    Packet* pkt = PacketPool::get().acquire();
    REQUIRE(pkt != nullptr);
    pkt->payload->set_data_length(sizeof(bundles::CacheRespBundle));
    pkt->type = type;
    REQUIRE(bundles::serialize_bundle(payload, pkt->payload->get_data_ptr(),
                                      pkt->payload->get_data_length()));
    target.recvReq(pkt);
}

bundles::CacheRespBundle make_resp(std::uint64_t txn, std::uint64_t data) {
    bundles::CacheRespBundle r;
    r.transaction_id.write(txn);
    r.data.write(data);
    r.is_hit.write(false);
    r.error_code.write(0);
    return r;
}

} // namespace

// ── AC-1 · Network 组入向 ⇒ 落 `net_resp_in()`(修复的核心)──────────────────
TEST_CASE("DualPort inbound: net 组入向 ⇒ `net_resp_in()` 收到 ∧ `pe_req_in()` 保持无效",
          "[dualport][inbound][r2]") {
    DualPortFixture fx;

    // net 组的目标端口(`port_idx_ = 1`)
    deliver(fx.net_resp_in, make_resp(200, 0xDEADBEEF), PKT_RESP);

    // ✅ 落 net 侧入向通道
    REQUIRE(fx.mod.net_resp_in().valid());
    REQUIRE(fx.mod.net_resp_in().data().transaction_id.read() == 200u);
    REQUIRE(fx.mod.net_resp_in().data().data.read() == 0xDEADBEEFu);

    // ⛔ **不得**被当成业务请求读进 PE 侧(修复前的现象)
    REQUIRE_FALSE(fx.mod.pe_req_in().valid());
}

// ── AC-1 变体 · ⛔ 按 `pkt->type` 分流是错的(Completion 以 `PKT_RESP` 打在 `req_in` 上)──
TEST_CASE("DualPort inbound: net 组入向**不按 `pkt->type` 分流**(PKT_REQ 也落 net 侧)",
          "[dualport][inbound][r2]") {
    DualPortFixture fx;

    // CohIf 的 `Completion` 走 `req_out` ⇒ 以 `PKT_RESP` 打在**对端的 `req_in`** 上;
    // 而 `RouterTLM` 语义下 net 组的 flit 可为 `PKT_REQ` ⇒ **两种 type 都必须落 net 侧**。
    deliver(fx.net_resp_in, make_resp(201, 0xCAFE), PKT_REQ);

    REQUIRE(fx.mod.net_resp_in().valid());
    REQUIRE(fx.mod.net_resp_in().data().transaction_id.read() == 201u);
    REQUIRE_FALSE(fx.mod.pe_req_in().valid());
}

// ── AC-2 · PE 组入向 ⇒ 落 `pe_req_in()`(既有行为不变 ⇒ 无回归)─────────────
TEST_CASE("DualPort inbound: PE 组入向 ⇒ `pe_req_in()` 收到(既有行为不变)",
          "[dualport][inbound][r2][pe]") {
    DualPortFixture fx;

    bundles::CacheReqBundle req;
    req.transaction_id.write(100);
    req.address.write(0x2000);
    req.is_write.write(false);
    req.data.write(0);
    req.size.write(8);

    Packet* pkt = PacketPool::get().acquire();
    REQUIRE(pkt != nullptr);
    pkt->payload->set_data_length(sizeof(bundles::CacheReqBundle));
    pkt->type = PKT_REQ;
    REQUIRE(bundles::serialize_bundle(req, pkt->payload->get_data_ptr(),
                                      pkt->payload->get_data_length()));
    fx.pe_req_in.recvReq(pkt);

    REQUIRE(fx.mod.pe_req_in().valid());
    REQUIRE(fx.mod.pe_req_in().data().transaction_id.read() == 100u);
    REQUIRE(fx.mod.pe_req_in().data().address.read() == 0x2000u);

    // PE 组入向**不得**污染 net 侧通道
    REQUIRE_FALSE(fx.mod.net_resp_in().valid());
}

// ── AC-3 · `process_net_response_input()` 的调用点 > 0(修复前 = 0)──────────
TEST_CASE("DualPort inbound: net 组入向必经 `process_net_response_input()`(原零调用点)",
          "[dualport][inbound][r2]") {
    DualPortFixture fx;

    // `process_net_response_input()` 是本适配器**唯一**写 `net_resp_in()` 的路径
    // ⇒ 「`net_resp_in()` 有效」⇔「该方法被调用过」⇒ 原「零调用点」状态已被打破。
    REQUIRE_FALSE(fx.mod.net_resp_in().valid());
    deliver(fx.net_resp_in, make_resp(300, 0x1234), PKT_RESP);
    REQUIRE(fx.mod.net_resp_in().valid());
}

// ── 反向 fixture · 把路由改回「恒落 PE 组」⇒ **必红**(红因为「net 侧未收到」)──
// 变异方式:把 `DualPortStreamAdapter::process_request_input(Packet*, unsigned)` 的
// 分派改回无条件走 `process_pe_request_input()` ⇒ 下方 AC-1 的
// `REQUIRE(fx.mod.net_resp_in().valid())` **必红**。
TEST_CASE("DualPort inbound 反向 fixture: 旧行为(恒落 PE 组)⇒ net 侧收不到",
          "[dualport][inbound][r2][reverse]") {
    DualPortFixture fx;

    // 模拟「旧行为」:直接走 PE 侧投递路径(等价于把分派改回单参版)
    bundles::CacheRespBundle resp = make_resp(400, 0xABCD);
    Packet* pkt = PacketPool::get().acquire();
    REQUIRE(pkt != nullptr);
    pkt->payload->set_data_length(sizeof(bundles::CacheRespBundle));
    pkt->type = PKT_RESP;
    REQUIRE(bundles::serialize_bundle(resp, pkt->payload->get_data_ptr(),
                                      pkt->payload->get_data_length()));
    fx.adapter.process_pe_request_input(pkt);

    // 旧行为下:PE 侧被污染,而 net 侧**收不到** ⇒ 这正是 AC-1 要判红的现象
    REQUIRE(fx.mod.pe_req_in().valid());
    REQUIRE_FALSE(fx.mod.net_resp_in().valid());
}
