// test/test_noc_bundles.cc
// NoCFlitBundle 单元测试
// 功能描述：验证 NoCFlitBundle 字段读写、类型常量、辅助方法
// 作者 CppTLM Team / 日期 2026-04-23
#include <catch2/catch_all.hpp>
#include "bundles/noc_bundles_tlm.hh"

using namespace bundles;

TEST_CASE("NoCFlitBundle can be default constructed", "[noc][bundle]") {
    NoCFlitBundle flit;
    // 默认值应为 0
    REQUIRE(flit.transaction_id.read() == 0);
    REQUIRE(flit.src_node.read() == 0);
    REQUIRE(flit.dst_node.read() == 0);
    REQUIRE(flit.vc_id.read() == 0);
    REQUIRE(flit.flit_type.read() == 0);
    REQUIRE(flit.flit_category.read() == 0);
}

TEST_CASE("NoCFlitBundle fields can be read and written", "[noc][bundle]") {
    NoCFlitBundle flit;

    flit.transaction_id.write(0x123456789ABCDEEFULL);
    flit.src_node.write(5);
    flit.dst_node.write(10);
    flit.address.write(0x1000);
    flit.data.write(0xDEADBEEFCAFEBABEULL);
    flit.size.write(32);
    flit.vc_id.write(2);
    flit.flit_type.write(NoCFlitBundle::FLIT_BODY);
    flit.flit_index.write(1);
    flit.flit_count.write(4);
    flit.hops.write(3);
    flit.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    flit.is_write.write(true);
    flit.is_ok.write(true);
    flit.error_code.write(0);

    REQUIRE(flit.transaction_id.read() == 0x123456789ABCDEEFULL);
    REQUIRE(flit.src_node.read() == 5);
    REQUIRE(flit.dst_node.read() == 10);
    REQUIRE(flit.address.read() == 0x1000);
    REQUIRE(flit.data.read() == 0xDEADBEEFCAFEBABEULL);
    REQUIRE(flit.size.read() == 32);
    REQUIRE(flit.vc_id.read() == 2);
    REQUIRE(flit.flit_type.read() == NoCFlitBundle::FLIT_BODY);
    REQUIRE(flit.flit_index.read() == 1);
    REQUIRE(flit.flit_count.read() == 4);
    REQUIRE(flit.hops.read() == 3);
    REQUIRE(flit.flit_category.read() == NoCFlitBundle::CATEGORY_REQUEST);
    REQUIRE(flit.is_write.read() == true);
    REQUIRE(flit.is_ok.read() == true);
    REQUIRE(flit.error_code.read() == 0);
}

TEST_CASE("NoCFlitBundle FlitType constants are correct", "[noc][bundle]") {
    REQUIRE(NoCFlitBundle::FLIT_HEAD == 0);
    REQUIRE(NoCFlitBundle::FLIT_BODY == 1);
    REQUIRE(NoCFlitBundle::FLIT_TAIL == 2);
    REQUIRE(NoCFlitBundle::FLIT_HEAD_TAIL == 3);
}

TEST_CASE("NoCFlitBundle FlitCategory constants are correct", "[noc][bundle]") {
    REQUIRE(NoCFlitBundle::CATEGORY_REQUEST == 0);
    REQUIRE(NoCFlitBundle::CATEGORY_RESPONSE == 1);
}

TEST_CASE("NoCFlitBundle is_head() works correctly", "[noc][bundle]") {
    NoCFlitBundle flit;

    flit.flit_type.write(NoCFlitBundle::FLIT_HEAD);
    REQUIRE(flit.is_head() == true);

    flit.flit_type.write(NoCFlitBundle::FLIT_HEAD_TAIL);
    REQUIRE(flit.is_head() == true);

    flit.flit_type.write(NoCFlitBundle::FLIT_BODY);
    REQUIRE(flit.is_head() == false);

    flit.flit_type.write(NoCFlitBundle::FLIT_TAIL);
    REQUIRE(flit.is_head() == false);
}

TEST_CASE("NoCFlitBundle is_tail() works correctly", "[noc][bundle]") {
    NoCFlitBundle flit;

    flit.flit_type.write(NoCFlitBundle::FLIT_TAIL);
    REQUIRE(flit.is_tail() == true);

    flit.flit_type.write(NoCFlitBundle::FLIT_HEAD_TAIL);
    REQUIRE(flit.is_tail() == true);

    flit.flit_type.write(NoCFlitBundle::FLIT_BODY);
    REQUIRE(flit.is_tail() == false);

    flit.flit_type.write(NoCFlitBundle::FLIT_HEAD);
    REQUIRE(flit.is_tail() == false);
}

TEST_CASE("NoCFlitBundle is_request() and is_response() work correctly", "[noc][bundle]") {
    NoCFlitBundle flit;

    flit.flit_category.write(NoCFlitBundle::CATEGORY_REQUEST);
    REQUIRE(flit.is_request() == true);
    REQUIRE(flit.is_response() == false);

    flit.flit_category.write(NoCFlitBundle::CATEGORY_RESPONSE);
    REQUIRE(flit.is_request() == false);
    REQUIRE(flit.is_response() == true);
}

TEST_CASE("NoCFlitBundle make_head() factory creates valid HEAD flit", "[noc][bundle]") {
    NoCFlitBundle flit = NoCFlitBundle::make_head(
        0x1234ULL,     // transaction_id
        1,             // src_node
        5,             // dst_node
        0x1000ULL,     // address
        2,             // vc_id
        3,             // total_flits
        true,          // is_write
        0xDEADBEEFULL  // data_word
    );

    REQUIRE(flit.transaction_id.read() == 0x1234ULL);
    REQUIRE(flit.src_node.read() == 1);
    REQUIRE(flit.dst_node.read() == 5);
    REQUIRE(flit.address.read() == 0x1000ULL);
    REQUIRE(flit.vc_id.read() == 2);
    REQUIRE(flit.flit_index.read() == 0);  // HEAD 总是 index 0
    REQUIRE(flit.flit_count.read() == 3);
    REQUIRE(flit.hops.read() == 0);
    REQUIRE(flit.flit_category.read() == NoCFlitBundle::CATEGORY_REQUEST);
    REQUIRE(flit.is_write.read() == true);
    REQUIRE(flit.data.read() == 0xDEADBEEFULL);
    REQUIRE(flit.flit_type.read() == NoCFlitBundle::FLIT_HEAD);
}

TEST_CASE("NoCFlitBundle make_head() with single flit creates HEAD_TAIL", "[noc][bundle]") {
    NoCFlitBundle flit = NoCFlitBundle::make_head(
        0x1234ULL, 1, 5, 0x1000ULL, 2, 1, false, 0xDEADBEEFULL);

    REQUIRE(flit.flit_type.read() == NoCFlitBundle::FLIT_HEAD_TAIL);
    REQUIRE(flit.is_head() == true);
    REQUIRE(flit.is_tail() == true);
}

TEST_CASE("NoCFlitBundle make_resp_head() factory creates valid RESPONSE HEAD flit", "[noc][bundle]") {
    NoCFlitBundle flit = NoCFlitBundle::make_resp_head(
        0x5678ULL,     // transaction_id
        5,             // src_node (Memory)
        1,             // dst_node (NICTLM)
        3,             // vc_id
        2,             // total_flits
        true,          // is_ok
        0             // error_code
    );

    REQUIRE(flit.transaction_id.read() == 0x5678ULL);
    REQUIRE(flit.src_node.read() == 5);
    REQUIRE(flit.dst_node.read() == 1);
    REQUIRE(flit.vc_id.read() == 3);
    REQUIRE(flit.flit_index.read() == 0);
    REQUIRE(flit.flit_count.read() == 2);
    REQUIRE(flit.hops.read() == 0);
    REQUIRE(flit.flit_category.read() == NoCFlitBundle::CATEGORY_RESPONSE);
    REQUIRE(flit.is_ok.read() == true);
    REQUIRE(flit.error_code.read() == 0);
    REQUIRE(flit.flit_type.read() == NoCFlitBundle::FLIT_HEAD);
    REQUIRE(flit.is_request() == false);
    REQUIRE(flit.is_response() == true);
}

TEST_CASE("NoCFlitBundle is standard layout (memcpy safe)", "[noc][bundle]") {
    // 验证 bundle_base 是空基类，NoCFlitBundle 保持标准布局
    static_assert(std::is_standard_layout<NoCFlitBundle>::value,
                  "NoCFlitBundle must be standard layout for memcpy safety");

    NoCFlitBundle original;
    original.transaction_id.write(0x1234ULL);
    original.src_node.write(1);
    original.dst_node.write(2);
    original.vc_id.write(3);
    original.flit_type.write(NoCFlitBundle::FLIT_HEAD_TAIL);
    original.data.write(0xDEADBEEFULL);

    // 使用 memcpy 复制（证明是标准布局）
    NoCFlitBundle copied;
    memcpy(&copied, &original, sizeof(NoCFlitBundle));

    REQUIRE(copied.transaction_id.read() == 0x1234ULL);
    REQUIRE(copied.src_node.read() == 1);
    REQUIRE(copied.dst_node.read() == 2);
    REQUIRE(copied.vc_id.read() == 3);
    REQUIRE(copied.flit_type.read() == NoCFlitBundle::FLIT_HEAD_TAIL);
    REQUIRE(copied.data.read() == 0xDEADBEEFULL);
}

TEST_CASE("NoCFlitBundle ch_bool type works correctly", "[noc][bundle]") {
    NoCFlitBundle flit;

    // 测试 is_write
    flit.is_write.write(true);
    REQUIRE(flit.is_write.read() == true);

    flit.is_write.write(false);
    REQUIRE(flit.is_write.read() == false);

    // 测试 is_ok
    flit.is_ok.write(true);
    REQUIRE(flit.is_ok.read() == true);

    flit.is_ok.write(false);
    REQUIRE(flit.is_ok.read() == false);
}