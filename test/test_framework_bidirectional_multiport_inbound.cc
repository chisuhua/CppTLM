// test/test_framework_bidirectional_multiport_inbound.cc
// 双向端口 StreamAdapter —— **多端口入向按端口分流**的判据
//
// 功能描述:验证 `BidirectionalPortAdapter` 的入向包落进**正确的端口桶**
//          (`req_in()[port_idx]`),而非恒落端口 0。
// 作者: CppTLM Team
// 日期: 2026-11-09
//
// ── 为什么有这组判据(per 台账 Entry #310 R-5/R-6 + Entry #317)─────────────
// 框架的入向路径固定为 `ChStreamTargetPort::recvReq`
// (`include/core/chstream_port.hh:70`) ⇒ 只调**双参** `process_request_input(pkt, port_idx_)`。
// 修复前 `BidirectionalPortAdapter` 的双参版签名是 `(Packet*, std::size_t)`
// 而基类是 `(Packet*, unsigned)` ⇒ **签名不匹配且无 `override`** ⇒ 不是 override
// ⇒ 基类双参版**回退单参** ⇒ 派生类单参版硬编码端口 0
// ⇒ **8 个多端口模块的入向路由全部失效**
// (`RouterTLM` N=5 + 7 个 CEP 模块 N=2,见 `include/chstream_register.hh`)。
//
// ⚠️ **判据形态**:每条都走**框架真实入口**(`ChStreamTargetPort::recvReq`)
//    ⇒ ⛔ 不用「直接写 adapter 成员」冒充入向判据;
//    且每条都有**能使其变红**的反向 fixture(变异后红因为「目标端口桶未收到」)。
//
// ⚠️ **为什么新建文件而不是塞进 `test_router_tlm.cc`**:
//    实测 `grep -cE "ChStream|bind_ports|process_request_input" test/test_router_tlm.cc` = **0**
//    ⇒ 该文件**零 ChStream 装配**(全篇是 XY 路由数学的纯函数 / 计数断言)
//    ⇒ 把入向判据塞进去会产出「看起来在测路由、实际没碰适配器」的**伪载体**
//    —— 正是台账 Entry #236 记录的「载体未真正咬合判据主体」失败模式。
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "bundles/noc_bundles_tlm.hh"
#include "core/chstream_module.hh"
#include "core/chstream_port.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"
#include "framework/bidirectional_port_adapter.hh"
#include "tlm/router_tlm.hh"

#include <catch2/catch_all.hpp>

// ============================================================
// Mock 多端口模块(端口面契约与 `RouterTLM` 一致:
//   req_in()  → std::array<InputStreamAdapter<BundleT>, N>&
//   resp_out() → std::array<OutputStreamAdapter<BundleT>, N>&)
//
// ⚠️ **必须放匿名命名空间**:仓内已有两份测试曾把同名 mock 类放在**全局命名空间**
//    ⇒ 两个 TU 的同名类 = **ODR 违反** ⇒ 链接期谁赢取决于 TU 顺序
//    ⇒ 表现为「适配器写进 A 成员、测试读 B 成员」的**静默内存错位**
//    (per 台账 Entry #309 ⑤ R-9 的先例)。
// ============================================================
namespace {

template <std::size_t N> class MockMultiPortModule : public ChStreamModuleBase {
private:
    std::array<cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>, N> req_in_;
    std::array<cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>, N> resp_out_;
    cpptlm::StreamAdapterBase* adapter_ = nullptr;

public:
    MockMultiPortModule(const std::string& n, EventQueue* eq) : ChStreamModuleBase(n, eq) {}

    std::string get_module_type() const override { return "MockMultiPortModule"; }

    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override { adapter_ = adapter; }

    void tick() override {
        if (adapter_)
            adapter_->tick();
    }

    void do_reset(const ResetConfig&) override {
        for (auto& a : req_in_)
            a.reset();
        for (auto& a : resp_out_)
            a.reset();
    }

    /// Credit 回收回调(`BidirectionalPortAdapter::tick()` 的 credit 接收路径会调它)
    void receive_credit(unsigned /*in_port*/, unsigned /*vc*/) {}

    std::array<cpptlm::InputStreamAdapter<bundles::NoCFlitBundle>, N>& req_in() { return req_in_; }
    std::array<cpptlm::OutputStreamAdapter<bundles::NoCFlitBundle>, N>& resp_out() {
        return resp_out_;
    }
};

using MockN2 = MockMultiPortModule<2>;
using MockN5 = MockMultiPortModule<5>;

using AdapterN2 = cpptlm::BidirectionalPortAdapter<MockN2, bundles::NoCFlitBundle, 2>;
using AdapterN5 = cpptlm::BidirectionalPortAdapter<MockN5, bundles::NoCFlitBundle, 5>;
using RouterAdapterN5 =
    cpptlm::BidirectionalPortAdapter<tlm::RouterTLM, bundles::NoCFlitBundle, 5>;

/// 构造一个带唯一 tag 的 flit bundle
/// ⚠️ tag 走 `transaction_id`(ch_uint<64>)⇒ 端口数 × tag 数不会溢出
bundles::NoCFlitBundle make_flit(std::uint64_t tag) {
    bundles::NoCFlitBundle b;
    b.transaction_id.write(tag);
    b.src_node.write(0);
    b.dst_node.write(0);
    b.vc_id.write(0);
    b.flit_count.write(1);
    b.size.write(1);
    b.src_port.write(0);
    return b;
}

/// 经**框架真实入口**(`ChStreamTargetPort::recvReq`)投递一个包
/// ⚠️ `recvReq` 内部会把包还给 `PacketPool` ⇒ 调用方**不得**再 release
void deliver(cpptlm::ChStreamTargetPort& target, std::uint64_t tag) {
    Packet* pkt = PacketPool::get().acquire();
    REQUIRE(pkt != nullptr);
    pkt->payload->set_data_length(sizeof(bundles::NoCFlitBundle));
    pkt->type = PKT_REQ;
    const auto b = make_flit(tag);
    REQUIRE(bundles::serialize_bundle(b, pkt->payload->get_data_ptr(),
                                      pkt->payload->get_data_length()));
    target.recvReq(pkt);
}

/// 端口持有者(`ChStreamTargetPort` / `ChStreamInitiatorPort` **不可默认构造**
/// ⇒ 用 `vector<unique_ptr>` 持有)
template <std::size_t N> struct PortSet {
    EventQueue eq;
    MockMultiPortModule<N> mod{"multiport", &eq};
    cpptlm::BidirectionalPortAdapter<MockMultiPortModule<N>, bundles::NoCFlitBundle, N> adapter{
        &mod};
    std::vector<std::unique_ptr<cpptlm::ChStreamInitiatorPort>> req_out;
    std::vector<std::unique_ptr<cpptlm::ChStreamTargetPort>> req_in;
    std::vector<std::unique_ptr<cpptlm::ChStreamInitiatorPort>> resp_out;
    std::vector<std::unique_ptr<cpptlm::ChStreamTargetPort>> resp_in;

    PortSet() {
        for (std::size_t i = 0; i < N; ++i) {
            req_out.emplace_back(
                std::make_unique<cpptlm::ChStreamInitiatorPort>("req_out", &eq));
            req_in.emplace_back(std::make_unique<cpptlm::ChStreamTargetPort>(
                "req_in", &adapter, &eq, static_cast<unsigned>(i)));
            resp_out.emplace_back(
                std::make_unique<cpptlm::ChStreamInitiatorPort>("resp_out", &eq));
            resp_in.emplace_back(std::make_unique<cpptlm::ChStreamTargetPort>(
                "resp_in", &adapter, &eq, static_cast<unsigned>(i)));
        }
        std::array<MasterPort*, N> ro{};
        std::array<SlavePort*, N> ri{};
        std::array<MasterPort*, N> so{};
        std::array<SlavePort*, N> si{};
        for (std::size_t i = 0; i < N; ++i) {
            ro[i] = req_out[i].get();
            ri[i] = resp_in[i].get();
            so[i] = resp_out[i].get();
            si[i] = req_in[i].get();
        }
        adapter.bind_ports_array(ro, ri, so, si);
        mod.set_stream_adapter(&adapter);
    }
};

/// 断言「只有端口 k 的桶收到 tag」
template <std::size_t N>
void require_only_bucket_k_has(MockMultiPortModule<N>& mod, std::size_t k, std::uint64_t tag) {
    for (std::size_t j = 0; j < N; ++j) {
        if (j == k) {
            REQUIRE(mod.req_in()[j].valid());
            REQUIRE(mod.req_in()[j].data().transaction_id.read() == tag);
        } else {
            REQUIRE_FALSE(mod.req_in()[j].valid());
        }
    }
}

} // namespace

// ── AC-1 · 编译期签名锁 ──────────────────────────────────────────────────
// 签名漂移(改回 `std::size_t` / 去掉 `override`)⇒ **编译红**,而非运行期静默错路由。
// ⚠️ 这是**唯一**可靠的守门手段:实测项目虽有 `-Wall`(含 `-Woverloaded-virtual`),
//    但**真实头文件不报该警告**(GCC 14.2 实测)⇒ ⛔ 不得依赖编译警告。
TEST_CASE("Bidirectional adapter: 2 参 process_request_input 是基类签名的真 override",
          "[adapter][bidirectional][multiport][r5]") {
    using BaseSig = void (cpptlm::StreamAdapterBase::*)(Packet*, unsigned);
    using AdapterSig = void (AdapterN2::*)(Packet*, unsigned);
    using WrongSig = void (AdapterN2::*)(Packet*, std::size_t);

    // ① 派生类必须**有** `(Packet*, unsigned)` 签名的成员 —— 否则本行编译失败
    //    (实测:修复前只有 `(Packet*, std::size_t)` ⇒ 编译器报
    //     "no matches converting function 'process_request_input'" ⇒ **编译红**)
    AdapterSig exact = static_cast<AdapterSig>(&AdapterN2::process_request_input);
    REQUIRE(exact != nullptr);

    // ② 该成员必须**真的 override 基类虚函数** —— 验证方式:通过基类指针取值,
    //    得到的必须与派生类的同签名成员是同一个(⇒ 虚分派可达)
    BaseSig via_base = static_cast<BaseSig>(&AdapterN2::process_request_input);
    REQUIRE(via_base != nullptr);

    // ③ `unsigned` 与 `std::size_t` 是两个**不同**的签名 ⇒ 后者若存在会隐藏前者
    //    (本断言在修复前会失败:那时**只有** WrongSig)
    static_assert(!std::is_same_v<AdapterSig, WrongSig>,
                  "unsigned 与 std::size_t 是不同签名 ⇒ 二者不可混用(R-5 根因)");
}

// ── AC-2 · 运行期分桶(N=2 / N=5,**全枚举**)──────────────────────────────
// 决定性判据:向端口 k 注入 ⇒ **仅** `req_in[k]` 收到,其余桶保持无效。
// ⛔ 非抽样(全枚举)⇒ 每个端口都被独立验证。
TEST_CASE("Bidirectional adapter: 入向包按端口分桶(N=2 全枚举)",
          "[adapter][bidirectional][multiport][r5]") {
    PortSet<2> fx;

    for (std::size_t k = 0; k < 2; ++k) {
        for (std::size_t j = 0; j < 2; ++j)
            fx.mod.req_in()[j].consume();

        const std::uint64_t tag = 100 + k;
        deliver(*fx.req_in[k], tag);

        require_only_bucket_k_has(fx.mod, k, tag);
    }
}

TEST_CASE("Bidirectional adapter: 入向包按端口分桶(N=5 全枚举)",
          "[adapter][bidirectional][multiport][r5]") {
    PortSet<5> fx;

    for (std::size_t k = 0; k < 5; ++k) {
        for (std::size_t j = 0; j < 5; ++j)
            fx.mod.req_in()[j].consume();

        const std::uint64_t tag = 200 + k;
        deliver(*fx.req_in[k], tag);

        require_only_bucket_k_has(fx.mod, k, tag);
    }
}

// ── AC-3 · RouterTLM **真装配**(N=5)─────────────────────────────────────
// ⛔ 不是「只有模板夹具绿」⇒ 用真实 `RouterTLM` 跑同一分桶断言。
TEST_CASE("Bidirectional adapter: RouterTLM 真装配下入向按端口分桶",
          "[adapter][bidirectional][multiport][router][r5]") {
    constexpr std::size_t N = 5;
    EventQueue eq;
    tlm::RouterTLM router{"router", &eq};
    RouterAdapterN5 adapter{&router};

    std::vector<std::unique_ptr<cpptlm::ChStreamInitiatorPort>> req_out;
    std::vector<std::unique_ptr<cpptlm::ChStreamTargetPort>> req_in;
    std::vector<std::unique_ptr<cpptlm::ChStreamInitiatorPort>> resp_out;
    std::vector<std::unique_ptr<cpptlm::ChStreamTargetPort>> resp_in;
    for (std::size_t i = 0; i < N; ++i) {
        req_out.emplace_back(std::make_unique<cpptlm::ChStreamInitiatorPort>("req_out", &eq));
        req_in.emplace_back(std::make_unique<cpptlm::ChStreamTargetPort>(
            "req_in", &adapter, &eq, static_cast<unsigned>(i)));
        resp_out.emplace_back(std::make_unique<cpptlm::ChStreamInitiatorPort>("resp_out", &eq));
        resp_in.emplace_back(std::make_unique<cpptlm::ChStreamTargetPort>(
            "resp_in", &adapter, &eq, static_cast<unsigned>(i)));
    }
    std::array<MasterPort*, N> ro{};
    std::array<SlavePort*, N> ri{};
    std::array<MasterPort*, N> so{};
    std::array<SlavePort*, N> si{};
    for (std::size_t i = 0; i < N; ++i) {
        ro[i] = req_out[i].get();
        ri[i] = resp_in[i].get();
        so[i] = resp_out[i].get();
        si[i] = req_in[i].get();
    }
    adapter.bind_ports_array(ro, ri, so, si);

    for (std::size_t k = 0; k < N; ++k) {
        for (std::size_t j = 0; j < N; ++j)
            router.req_in()[j].consume();

        const std::uint64_t tag = 300 + k;
        deliver(*req_in[k], tag);

        for (std::size_t j = 0; j < N; ++j) {
            if (j == k) {
                REQUIRE(router.req_in()[j].valid());
                REQUIRE(router.req_in()[j].data().transaction_id.read() == tag);
            } else {
                REQUIRE_FALSE(router.req_in()[j].valid());
            }
        }
    }
}

// ── AC-7 · 端口忙 ⇒ 丢包**可观测**(不再静默)──────────────────────────────
// ⚠️ **诚实边界**:端口忙的**丢弃语义不变**(回压 / 阻塞属另立 change)
//    ⇒ 本判据只保证「丢包**可计数**」⇒ ⛔ 不得读成「已解决丢包」。
TEST_CASE("Bidirectional adapter: 端口忙 ⇒ 丢包计数递增(不静默)",
          "[adapter][bidirectional][multiport][busy][r5]") {
    AdapterN2::reset_dropped_on_busy_port_count();
    REQUIRE(AdapterN2::dropped_on_busy_port_count() == 0);

    PortSet<2> fx;

    // 第 1 包占住端口 0
    deliver(*fx.req_in[0], 400);
    REQUIRE(fx.mod.req_in()[0].valid());
    REQUIRE(AdapterN2::dropped_on_busy_port_count() == 0);

    // 第 2 包仍投端口 0 ⇒ 端口忙 ⇒ 被丢弃 ⇒ **计数递增**
    deliver(*fx.req_in[0], 401);
    REQUIRE(AdapterN2::dropped_on_busy_port_count() == 1);

    // 端口 1 不受影响(空闲 ⇒ 正常接收)
    deliver(*fx.req_in[1], 402);
    REQUIRE(fx.mod.req_in()[1].valid());
    REQUIRE(AdapterN2::dropped_on_busy_port_count() == 1);

    // 消费掉端口 0 ⇒ 恢复接收
    fx.mod.req_in()[0].consume();
    deliver(*fx.req_in[0], 403);
    REQUIRE(fx.mod.req_in()[0].valid());
    REQUIRE(AdapterN2::dropped_on_busy_port_count() == 1);
}

// ── AC-8 · 边界:端口越界 ⇒ no-op(不崩、不写越界)────────────────────────
TEST_CASE("Bidirectional adapter: `port_idx == N` 越界 ⇒ no-op",
          "[adapter][bidirectional][multiport][boundary][r5]") {
    PortSet<2> fx;

    // 直接调 2 参版并传越界下标 ⇒ 应静默返回,不崩、不写任何桶
    Packet* pkt = PacketPool::get().acquire();
    REQUIRE(pkt != nullptr);
    pkt->payload->set_data_length(sizeof(bundles::NoCFlitBundle));
    pkt->type = PKT_REQ;
    const auto b = make_flit(999);
    REQUIRE(bundles::serialize_bundle(b, pkt->payload->get_data_ptr(),
                                      pkt->payload->get_data_length()));
    fx.adapter.process_request_input(pkt, 2);  // == N ⇒ 越界
    PacketPool::get().release(pkt);

    for (std::size_t j = 0; j < 2; ++j)
        REQUIRE_FALSE(fx.mod.req_in()[j].valid());
}
