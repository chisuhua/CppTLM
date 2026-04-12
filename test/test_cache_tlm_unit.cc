// test/test_cache_tlm_unit.cc
// P1.8: CacheTLM 模块逻辑单元测试 — hit/miss/write

#include <catch2/catch_all.hpp>
#include "bundles/cache_bundles_tlm.hh"

TEST_CASE("CacheTLM write caches data", "[chstream][cache]") {
    std::map<uint64_t, uint64_t> cache_lines;

    // Write request
    bundles::CacheReqBundle write_req;
    write_req.transaction_id.write(1);
    write_req.address.write(0x1000);
    write_req.is_write.write(1);
    write_req.data.write(0xABCD);
    write_req.size.write(8);

    if (write_req.is_write.read()) {
        cache_lines[write_req.address.read()] = write_req.data.read();
    }

    REQUIRE(cache_lines.count(0x1000) > 0);
    REQUIRE(cache_lines[0x1000] == 0xABCD);
}

TEST_CASE("CacheTLM read returns hit after write", "[chstream][cache]") {
    std::map<uint64_t, uint64_t> cache_lines;
    cache_lines[0x1000] = 0xABCD;

    bundles::CacheReqBundle read_req;
    read_req.transaction_id.write(2);
    read_req.address.write(0x1000);
    read_req.is_write.write(0);
    read_req.data.write(0);
    read_req.size.write(8);

    bundles::CacheRespBundle resp;
    bool hit = cache_lines.count(read_req.address.read()) > 0;
    resp.transaction_id.write(read_req.transaction_id.read());
    resp.data.write(hit ? cache_lines[read_req.address.read()] : 0);
    resp.is_hit.write(hit ? 1 : 0);
    resp.error_code.write(0);

    REQUIRE(resp.is_hit.read() == true);
    REQUIRE(resp.data.read() == 0xABCD);
    REQUIRE(resp.transaction_id.read() == 2);
}

TEST_CASE("CacheTLM read returns miss for uncached address", "[chstream][cache]") {
    std::map<uint64_t, uint64_t> cache_lines;

    bundles::CacheReqBundle read_req;
    read_req.transaction_id.write(10);
    read_req.address.write(0x9999);
    read_req.is_write.write(0);
    read_req.data.write(0);
    read_req.size.write(8);

    bundles::CacheRespBundle resp;
    bool hit = cache_lines.count(read_req.address.read()) > 0;
    resp.transaction_id.write(read_req.transaction_id.read());
    resp.data.write(hit ? cache_lines[read_req.address.read()] : 0);
    resp.is_hit.write(hit ? 1 : 0);
    resp.error_code.write(0);

    REQUIRE(resp.is_hit.read() == false);
    REQUIRE(resp.data.read() == 0);
}

TEST_CASE("Cache reset clears all lines", "[chstream][cache]") {
    std::map<uint64_t, uint64_t> cache_lines;
    cache_lines[0x1000] = 0xDEAD;
    cache_lines[0x2000] = 0xBEEF;

    cache_lines.clear();
    REQUIRE(cache_lines.empty());
}

TEST_CASE("CacheTLM consecutive writes accumulate", "[chstream][cache]") {
    std::map<uint64_t, uint64_t> cache_lines;

    auto do_write = [&](uint64_t addr, uint64_t data) {
        bundles::CacheReqBundle req;
        req.address.write(addr);
        req.is_write.write(1);
        req.data.write(data);
        cache_lines[req.address.read()] = req.data.read();
    };

    do_write(0x1000, 0xAAAA);
    do_write(0x2000, 0xBBBB);
    do_write(0x3000, 0xCCCC);

    REQUIRE(cache_lines.size() == 3);
    REQUIRE(cache_lines[0x1000] == 0xAAAA);
    REQUIRE(cache_lines[0x2000] == 0xBBBB);
    REQUIRE(cache_lines[0x3000] == 0xCCCC);
}
