// include/bundles/cache_bundles_rtl.hh
// Cache Bundle 定义（CppHDL 侧，与 cache_bundles_tlm.hh 对称）
// 功能描述：CppHDL ch_stream<BundleT> 使用的 Bundle 类型，
//           包含分片（fragment）元数据字段。
//           遵循 v2.1 §4.1 + example_rtl_modules.md canonical pattern。
// 作者 CppTLM Team / 日期 2026-06-07
#ifndef BUNDLES_CACHE_BUNDLES_RTL_HH
#define BUNDLES_CACHE_BUNDLES_RTL_HH

#include "ch.hpp"  // CppHDL 主入口
#include "core/bundle/bundle_base.h"  // bundle_base CRTP (ch.hpp未包含)
#include <cstdint>

namespace bundles {

/**
 * @brief Cache 请求 Bundle（CppHDL RTL 侧）
 *
 * 字段（与 cache_bundles_tlm.hh 对称 + CppHDL 类型）：
 *   - transaction_id : 事务 ID（ch_uint<64>）
 *   - parent_id      : 父事务 ID（0 = 根事务）
 *   - fragment_id    : 当前拍序号（0-based）
 *   - fragment_total : 总拍数（1 = 不分片）
 *   - address        : 请求地址
 *   - size           : 请求大小
 *   - is_write       : 写标志
 *   - data           : 写数据首 8 字节
 *
 * 继承 ch::core::bundle_base<CRTP>，使用 CH_BUNDLE_FIELDS_T 宏声明字段
 */
struct CacheReqBundleRTL : public ch::core::bundle_base<CacheReqBundleRTL> {
    using Self = CacheReqBundleRTL;

    ch::core::ch_uint<64> transaction_id;
    ch::core::ch_uint<64> parent_id;
    ch::core::ch_uint<8>  fragment_id;
    ch::core::ch_uint<8>  fragment_total;
    ch::core::ch_uint<64> address;
    ch::core::ch_uint<8>  size;
    ch::core::ch_bool     is_write;
    ch::core::ch_uint<64> data;

    CacheReqBundleRTL() = default;
    explicit CacheReqBundleRTL(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(transaction_id, parent_id, fragment_id, fragment_total,
                       address, size, is_write, data)

    void as_master_direction() {
        // Master: 输出请求信号
        this->make_output(transaction_id, parent_id, fragment_id, fragment_total,
                          address, size, is_write, data);
    }

    void as_slave_direction() {
        // Slave: 输入请求信号
        this->make_input(transaction_id, parent_id, fragment_id, fragment_total,
                         address, size, is_write, data);
    }
};

/**
 * @brief Cache 响应 Bundle（CppHDL RTL 侧）
 *
 * 字段：
 *   - transaction_id : 响应事务 ID
 *   - parent_id      : 父事务 ID
 *   - fragment_id    : 当前拍序号
 *   - fragment_total : 总拍数
 *   - data           : 读数据 / 写确认
 *   - is_hit         : 命中标志
 *   - error_code     : 错误码（0 = OK）
 *   - first          : 首拍标志
 *   - last           : 末拍标志
 */
struct CacheRespBundleRTL : public ch::core::bundle_base<CacheRespBundleRTL> {
    using Self = CacheRespBundleRTL;

    ch::core::ch_uint<64> transaction_id;
    ch::core::ch_uint<64> parent_id;
    ch::core::ch_uint<8>  fragment_id;
    ch::core::ch_uint<8>  fragment_total;
    ch::core::ch_uint<64> data;
    ch::core::ch_bool     is_hit;
    ch::core::ch_uint<8>  error_code;
    ch::core::ch_bool     first;
    ch::core::ch_bool     last;

    CacheRespBundleRTL() = default;
    explicit CacheRespBundleRTL(const std::string& prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(transaction_id, parent_id, fragment_id, fragment_total,
                       data, is_hit, error_code, first, last)

    void as_master_direction() {
        // Master: 输入响应信号
        this->make_input(transaction_id, parent_id, fragment_id, fragment_total,
                         data, is_hit, error_code, first, last);
    }

    void as_slave_direction() {
        // Slave: 输出响应信号
        this->make_output(transaction_id, parent_id, fragment_id, fragment_total,
                          data, is_hit, error_code, first, last);
    }
};

} // namespace bundles

#endif // BUNDLES_CACHE_BUNDLES_RTL_HH
