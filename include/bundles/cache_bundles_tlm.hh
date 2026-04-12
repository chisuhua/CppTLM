// include/bundles/cache_bundles_tlm.hh
// Cache Bundle 定义（轻量级，不依赖 CppHDL AST）
// 功能描述：定义 Cache 请求/响应 Bundle，仅含 POD 数据字段
// 作者 CppTLM Team / 日期 2026-04-12
#ifndef BUNDLES_CACHE_BUNDLES_TLM_HH
#define BUNDLES_CACHE_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>
#include <string>

namespace bundles {

struct CacheReqBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;

    CacheReqBundle() = default;
    CacheReqBundle(uint64_t tid, uint64_t addr, uint8_t sz, bool wr, uint64_t d)
        : transaction_id(tid), address(addr), size(sz), is_write(wr), data(d) {}
};

struct CacheRespBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;

    CacheRespBundle() = default;
    CacheRespBundle(uint64_t tid, uint64_t d, bool hit, uint8_t err = 0)
        : transaction_id(tid), data(d), is_hit(hit), error_code(err) {}
};

} // namespace bundles

#endif // BUNDLES_CACHE_BUNDLES_TLM_HH
