#include <catch2/catch_all.hpp>
#include "core/chstream_port.hh"
#include "core/packet.hh"
#include "core/packet_pool.hh"

TEST_CASE("ChStreamInitiatorPort stores and drains responses", "[chstream][port]") {
    EventQueue eq;
    cpptlm::ChStreamInitiatorPort port("test.initiator", &eq);

    REQUIRE_FALSE(port.hasResponse());
    REQUIRE(port.drainResponse() == nullptr);

    auto* pkt = PacketPool::get().acquire();
    pkt->set_transaction_id(42);
    port.recvResp(pkt);

    REQUIRE(port.hasResponse());
    auto* drained = port.drainResponse();
    REQUIRE(drained != nullptr);
    REQUIRE(drained->get_transaction_id() == 42);
    REQUIRE_FALSE(port.hasResponse());

    PacketPool::get().release(drained);
}

TEST_CASE("ChStreamInitiatorPort queues multiple responses", "[chstream][port]") {
    EventQueue eq;
    cpptlm::ChStreamInitiatorPort port("test.multi", &eq);

    for (int i = 0; i < 3; i++) {
        auto* pkt = PacketPool::get().acquire();
        pkt->set_transaction_id(i);
        port.recvResp(pkt);
    }

    REQUIRE(port.hasResponse());

    for (int i = 0; i < 3; i++) {
        auto* pkt = port.drainResponse();
        REQUIRE(pkt != nullptr);
        REQUIRE(pkt->get_transaction_id() == i);
        PacketPool::get().release(pkt);
    }

    REQUIRE_FALSE(port.hasResponse());
}

TEST_CASE("ChStreamTargetPort handles null adapter without crash", "[chstream][port]") {
    EventQueue eq;
    cpptlm::StreamAdapterBase* adapter = nullptr;

    auto* pkt = PacketPool::get().acquire();
    pkt->set_transaction_id(99);

    auto* port = new cpptlm::ChStreamTargetPort("test.target", adapter, &eq);

    auto result = port->recvReq(pkt);
    REQUIRE(result == true);

    delete port;
}

TEST_CASE("ChStreamTargetPort reports correct cycle", "[chstream][port]") {
    EventQueue eq;
    auto* port = new cpptlm::ChStreamTargetPort("test.cycle", nullptr, &eq);

    REQUIRE(port->getCurrentCycle() == 0);
    eq.run(1);
    REQUIRE(port->getCurrentCycle() == 1);

    delete port;
}

TEST_CASE("ChStreamInitiatorPort reports correct cycle", "[chstream][port]") {
    EventQueue eq;
    auto* port = new cpptlm::ChStreamInitiatorPort("test.init_cycle", &eq);

    REQUIRE(port->getCurrentCycle() == 0);
    eq.run(1);
    REQUIRE(port->getCurrentCycle() == 1);

    delete port;
}

TEST_CASE("ChStreamTargetPort cycle tracks after multiple runs", "[chstream][port]") {
    EventQueue eq;
    auto* port = new cpptlm::ChStreamTargetPort("test.cycle_multi", nullptr, &eq);

    REQUIRE(port->getCurrentCycle() == 0);
    eq.run(5);
    REQUIRE(port->getCurrentCycle() == 5);
    eq.run(10);
    REQUIRE(port->getCurrentCycle() == 15);

    delete port;
}

TEST_CASE("ChStreamPort getOwner returns nullptr", "[chstream][port]") {
    EventQueue eq;
    auto* init_port = new cpptlm::ChStreamInitiatorPort("test.owner_init", &eq);
    auto* target_port = new cpptlm::ChStreamTargetPort("test.owner_tgt", nullptr, &eq);

    REQUIRE(init_port->getOwner() == nullptr);
    REQUIRE(target_port->getOwner() == nullptr);

    delete init_port;
    delete target_port;
}
