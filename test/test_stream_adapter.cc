// test/test_stream_adapter.cc
// P1.8: StreamAdapter 单元测试 — InputStreamAdapter / OutputStreamAdapter

#include <catch2/catch_all.hpp>
#include "bundles/cache_bundles_tlm.hh"
#include "bundles/bundle_serialization.hh"
#include "framework/stream_adapter.hh"

#include <cstring>

TEST_CASE("cpptlm::ch_uint read/write roundtrip", "[chstream][types]") {
    bundles::ch_uint<64> val;
    REQUIRE(val.read() == 0);

    val.write(0xABCDEF1234567890ULL);
    REQUIRE(val.read() == 0xABCDEF1234567890ULL);

    bundles::ch_uint<8> small_val;
    small_val.write(255);
    REQUIRE(small_val.read() == 255);
}

TEST_CASE("cpptlm::ch_bool read/write roundtrip", "[chstream][types]") {
    bundles::ch_bool fl;
    REQUIRE(fl.read() == false);

    fl.write(1);
    REQUIRE(fl.read() == true);

    fl.write(0);
    REQUIRE(fl.read() == false);

    bundles::ch_bool t(true);
    REQUIRE(t);
}

TEST_CASE("CacheReqBundle field access", "[chstream][bundle]") {
    bundles::CacheReqBundle req;
    req.transaction_id.write(100);
    req.address.write(0x4000);
    req.is_write.write(1);
    req.data.write(0xDEADBEEF);
    req.size.write(8);

    REQUIRE(req.transaction_id.read() == 100);
    REQUIRE(req.address.read() == 0x4000);
    REQUIRE(req.is_write.read() == true);
    REQUIRE(req.data.read() == 0xDEADBEEF);
    REQUIRE(req.size.read() == 8);
}

TEST_CASE("CacheRespBundle field access", "[chstream][bundle]") {
    bundles::CacheRespBundle resp;
    resp.transaction_id.write(100);
    resp.data.write(0xCAFE);
    resp.is_hit.write(1);
    resp.error_code.write(0);

    REQUIRE(resp.transaction_id.read() == 100);
    REQUIRE(resp.data.read() == 0xCAFE);
    REQUIRE(resp.is_hit.read() == true);
    REQUIRE(resp.error_code.read() == 0);
}

TEST_CASE("Bundle memcpy serialization is standard layout safe", "[chstream][serialize]") {
    bundles::CacheReqBundle src;
    src.transaction_id.write(42);
    src.address.write(0x1000);
    src.is_write.write(0);
    src.data.write(0);
    src.size.write(4);

    char buf[1024];
    bool ok = bundles::serialize_bundle(src, buf, sizeof(buf));
    REQUIRE(ok);

    bundles::CacheReqBundle dst;
    ok = bundles::deserialize_bundle(buf, sizeof(buf), dst);
    REQUIRE(ok);

    REQUIRE(dst.transaction_id.read() == 42);
    REQUIRE(dst.address.read() == 0x1000);
    REQUIRE(dst.is_write.read() == false);
    REQUIRE(dst.data.read() == 0);
    REQUIRE(dst.size.read() == 4);
}

TEST_CASE("InputStreamAdapter valid/consume semantics", "[chstream][adapter]") {
    cpptlm::InputStreamAdapter<bundles::CacheReqBundle> adapter;
    REQUIRE_FALSE(adapter.valid());
    REQUIRE(adapter.ready());

    // Initially not valid
    adapter.reset();
    REQUIRE_FALSE(adapter.valid());
}

TEST_CASE("OutputStreamAdapter write/valid semantics", "[chstream][adapter]") {
    cpptlm::OutputStreamAdapter<bundles::CacheRespBundle> adapter;
    REQUIRE_FALSE(adapter.valid());

    bundles::CacheRespBundle resp;
    resp.data.write(0x1234);
    resp.is_hit.write(1);
    adapter.write(resp);

    REQUIRE(adapter.valid());
    REQUIRE(adapter.data().data.read() == 0x1234);
    REQUIRE(adapter.data().is_hit.read() == true);
}

TEST_CASE("bundle_base is standard layout (no vtable)", "[chstream][types]") {
    REQUIRE(std::is_standard_layout_v<bundles::CacheReqBundle>);
    REQUIRE(std::is_standard_layout_v<bundles::CacheRespBundle>);
    REQUIRE(std::is_standard_layout_v<bundles::bundle_base>);
}
