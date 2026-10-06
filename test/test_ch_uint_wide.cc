// test/test_ch_uint_wide.cc
// Phase P0: ch_uint wide-type specialization layout-identity + bit-slice tests
// Author: Architecture WG / Date: 2026-10-06
// Per docs/superpowers/specs/2026-10-06-dgpu-realip-refactor-design.md §5.1
#include "bundles/cpphdl_types.hh"

#include <catch_amalgamated.hpp>

#include <type_traits>

using namespace bundles;

// Layout-identity: ch_uint_storage<W> must be sizeof + standard-layout + trivially-copyable
// identical to pre-change ch_uint<W> for W in {1,2,4,8,16,32,64}
TEMPLATE_TEST_CASE("ch_uint_storage W<=64 layout identity", "[ch_uint][p0]",
    (uint8_t), (uint16_t), (uint32_t), (uint64_t)) {
    using W = TestType;
    constexpr unsigned Bits = sizeof(W) * 8;

    STATIC_REQUIRE(std::is_standard_layout_v<ch_uint_storage<Bits>>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<ch_uint_storage<Bits>>);
    STATIC_REQUIRE(sizeof(ch_uint_storage<Bits>) == sizeof(uint64_t));
}

// ch_uint<W> dispatch must inherit all constructors from base
TEST_CASE("ch_uint ctor inheritance preserves ch_uint<8>(k) call sites", "[ch_uint][p0]") {
    ch_uint<8> x(42);
    REQUIRE(static_cast<uint64_t>(x) == 42);

    ch_uint<32> y(0xDEADBEEF);
    REQUIRE(static_cast<uint64_t>(y) == 0xDEADBEEF);

    ch_uint<64> z;
    REQUIRE(static_cast<uint64_t>(z) == 0);
}

// ch_uint_wide<W> must exist for W>64 and expose slice/set_slice/low
TEST_CASE("ch_uint_wide W=128 layout + bit-slice round-trip", "[ch_uint][p0][wide]") {
    STATIC_REQUIRE(std::is_standard_layout_v<ch_uint_wide<128>>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<ch_uint_wide<128>>);

    ch_uint<128> x;
    x.template set_slice<63, 0>(0xCAFEBABEDEADBEEFULL);
    REQUIRE(x.template slice<63, 0>() == 0xCAFEBABEDEADBEEFULL);
    REQUIRE(x.low() == 0xCAFEBABEDEADBEEFULL);

    ch_uint<128> y;
    y.template set_slice<127, 64>(0x123456789ABCDEF0ULL);
    REQUIRE(y.template slice<127, 64>() == 0x123456789ABCDEF0ULL);
}

// ch_uint_wide<W> for 256 / 512 must also satisfy layout constraints
// (used by axi4_*/pcie_axi_adapter_tlm/pcie_endpoint_ip in production)
TEST_CASE("ch_uint_wide W=256/512 layout", "[ch_uint][p0][wide]") {
    using W256 = ch_uint_wide<256>;
    using W512 = ch_uint_wide<512>;
    STATIC_REQUIRE(std::is_standard_layout_v<W256>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<W256>);
    STATIC_REQUIRE(sizeof(W256) == 4 * sizeof(uint64_t));
    STATIC_REQUIRE(std::is_standard_layout_v<W512>);
    STATIC_REQUIRE(std::is_trivially_copyable_v<W512>);
    STATIC_REQUIRE(sizeof(W512) == 8 * sizeof(uint64_t));
}