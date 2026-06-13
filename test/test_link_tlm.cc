// test/test_link_tlm.cc
// SPDX-License-Identifier: Apache-2.0
// 作者 CppTLM Team / 日期 2026-05-09
#include "bundles/noc_bundles_tlm.hh"
#include "core/event_queue.hh"
#include "tlm/link_tlm.hh"
#include <catch2/catch_all.hpp>

using namespace tlm;
using namespace bundles;

static void inject_flit(LinkTLM* link, uint64_t tid, uint32_t src, uint32_t dst, uint8_t vc,
                        uint8_t flit_type) {
    bundles::NoCFlitBundle flit;
    flit.transaction_id.write(tid);
    flit.src_node.write(src);
    flit.dst_node.write(dst);
    flit.vc_id.write(vc);
    flit.flit_type.write(flit_type);
    flit.flit_category.write(bundles::NoCFlitBundle::CATEGORY_REQUEST);
    link->req_in().data() = flit;
    link->req_in().set_valid(true);
}

TEST_CASE("LinkTLM default latency is 1", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq);
    REQUIRE(link.latency() == 1);
}

TEST_CASE("LinkTLM latency can be configured", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq, 3);
    REQUIRE(link.latency() == 3);
    link.set_latency(5);
    REQUIRE(link.latency() == 5);
}

TEST_CASE("LinkTLM forwards flit with default latency", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq, 1);
    inject_flit(&link, 42, 0, 1, 0, bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    link.tick();
    REQUIRE(link.resp_out().valid() == false);
    link.tick();
    REQUIRE(link.resp_out().valid() == true);
    REQUIRE(link.resp_out().data().transaction_id.read() == 42);
    REQUIRE(link.stats().flits_forwarded == 1);
}

TEST_CASE("LinkTLM forwards flit with latency 3", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq, 3);
    inject_flit(&link, 100, 0, 1, 2, bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    link.tick();
    REQUIRE(link.resp_out().valid() == false);
    link.tick();
    REQUIRE(link.resp_out().valid() == false);
    link.tick();
    REQUIRE(link.resp_out().valid() == false);
    link.tick();
    REQUIRE(link.resp_out().valid() == true);
    REQUIRE(link.resp_out().data().transaction_id.read() == 100);
    REQUIRE(link.resp_out().data().vc_id.read() == 2);
    REQUIRE(link.stats().flits_forwarded == 1);
}

TEST_CASE("LinkTLM forwards multiple flits in order", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq, 1);
    inject_flit(&link, 1, 0, 1, 0, bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    link.tick();
    inject_flit(&link, 2, 0, 1, 0, bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    link.tick();
    REQUIRE(link.resp_out().valid() == true);
    REQUIRE(link.resp_out().data().transaction_id.read() == 1);
    REQUIRE(link.stats().flits_forwarded == 1);
    link.tick();
    REQUIRE(link.resp_out().valid() == true);
    REQUIRE(link.resp_out().data().transaction_id.read() == 2);
    REQUIRE(link.stats().flits_forwarded == 2);
}

TEST_CASE("LinkTLM credit return increments stats", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq, 1);
    REQUIRE(link.stats().credits_returned == 0);
    link.receive_credit(0, 1);
    link.tick();
    REQUIRE(link.stats().credits_returned == 1);
}

TEST_CASE("LinkTLM cycles_active when queue non-empty", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq, 2);
    REQUIRE(link.stats().cycles_active == 0);
    inject_flit(&link, 1, 0, 1, 0, bundles::NoCFlitBundle::FLIT_HEAD_TAIL);
    link.tick();
    REQUIRE(link.stats().cycles_active == 1);
    link.tick();
    REQUIRE(link.stats().cycles_active == 2);
    link.tick();
    REQUIRE(link.stats().cycles_active == 2);
    link.tick();
    REQUIRE(link.stats().cycles_active == 2);
}

TEST_CASE("LinkTLM forward with different flit types", "[link][tlm]") {
    EventQueue eq;
    LinkTLM link("link0", &eq, 1);
    inject_flit(&link, 10, 0, 1, 0, bundles::NoCFlitBundle::FLIT_HEAD);
    link.tick();
    link.tick();
    REQUIRE(link.resp_out().valid() == true);
    REQUIRE(link.resp_out().data().is_head() == true);
    link.tick();
    inject_flit(&link, 11, 0, 1, 0, bundles::NoCFlitBundle::FLIT_BODY);
    link.tick();
    link.tick();
    REQUIRE(link.resp_out().valid() == true);
    REQUIRE(link.resp_out().data().is_tail() == false);
    link.tick();
    inject_flit(&link, 12, 0, 1, 0, bundles::NoCFlitBundle::FLIT_TAIL);
    link.tick();
    link.tick();
    REQUIRE(link.resp_out().valid() == true);
    REQUIRE(link.resp_out().data().is_tail() == true);
}