// include/bundles/cache_bundles_tlm.hh
// Cache Bundle 定义（轻量级，不依赖 CppHDL AST）
// 功能描述：定义 Cache 请求/响应 Bundle，含分片（fragment）元数据字段。
//           该文件为 TLM 侧 Bundle，被 StreamAdapter 与 PIMPL Wrapper 共同使用。
//           与 RTL 侧 Bundle（cache_bundles_rtl.hh）通过 FragmentMapper 双向转换。
// 作者 CppTLM Team / 日期 2026-04-12
// 更新：2026-06-07 添加分片元数据（parent_id/fragment_id/fragment_total）
//                与谓词（is_first_fragment/is_last_fragment/is_root），
//                符合 v2.1 §4.1 + ADR-X.1 规范
#ifndef BUNDLES_CACHE_BUNDLES_TLM_HH
#define BUNDLES_CACHE_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>
#include <string>

namespace bundles {

/**
 * @brief Cache 请求 Bundle（轻量级 TLM 侧）
 *
 * 字段：
 *   - transaction_id   : 事务 ID（与 Packet::stream_id / TransactionContextExt 同步）
 *   - parent_id        : 父事务 ID（0 表示根事务，分片时为分组键）
 *   - fragment_id      : 当前拍序号（0-based）
 *   - fragment_total   : 总拍数（1 表示不分片）
 *   - address          : 请求地址（64-bit）
 *   - size             : 请求大小（字节，0-255）
 *   - is_write         : 写标志（true=write, false=read）
 *   - data             : 写数据首 8 字节
 *
 * 设计原则（v2.1 §4.1 + ADR-X.1）：
 *   - 轻量级：仅 POD 字段，无 CppHDL AST 依赖
 *   - C++17 兼容：可在 cpptlm_core（C++17 静态库）中使用
 *   - 字段语义与 TransactionContextExt 保持一致
 *   - 谓词方法对齐 TransactionContextExt::is_first_fragment/is_last_fragment/is_root
 */
struct CacheReqBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;        // 父事务 ID（0 = 根事务）
    ch_uint<8>  fragment_id;      // 当前拍序号（0-based）
    ch_uint<8>  fragment_total;   // 总拍数（1 = 不分片）
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;

    CacheReqBundle() = default;
    CacheReqBundle(uint64_t tid, uint64_t addr, uint8_t sz, bool wr, uint64_t d)
        : transaction_id(tid), parent_id(0), fragment_id(0), fragment_total(1)
        , address(addr), size(sz), is_write(wr), data(d) {}

    // 构造带分片元数据的版本（多拍请求场景）
    CacheReqBundle(uint64_t tid, uint64_t pid, uint8_t frag_id, uint8_t frag_total,
                   uint64_t addr, uint8_t sz, bool wr, uint64_t d)
        : transaction_id(tid), parent_id(pid), fragment_id(frag_id), fragment_total(frag_total)
        , address(addr), size(sz), is_write(wr), data(d) {}

    // 谓词（与 TransactionContextExt 对齐）
    bool is_first_fragment() const {
        return fragment_id.read() == 0;
    }
    bool is_last_fragment() const {
        return fragment_id.read() + 1 >= fragment_total.read();
    }
    bool is_root() const {
        return parent_id.read() == 0 && fragment_total.read() == 1;
    }
};

/**
 * @brief Cache 响应 Bundle（轻量级 TLM 侧）
 *
 * 字段：
 *   - transaction_id   : 响应事务 ID（与请求 ID 对齐）
 *   - parent_id        : 父事务 ID（与请求 parent_id 对齐）
 *   - fragment_id      : 当前拍序号
 *   - fragment_total   : 总拍数
 *   - data             : 读数据 / 写确认
 *   - is_hit           : 命中标志
 *   - error_code       : 错误码（0 = OK）
 *   - first            : 首拍（与 fragment_id==0 等价，但作为 Bundle 字段更显式）
 *   - last             : 末拍
 */
struct CacheRespBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;
    ch_uint<8>  fragment_id;
    ch_uint<8>  fragment_total;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;
    ch_bool     first;            // 显式 first 标志（与 fragment_id==0 等价）
    ch_bool     last;             // 显式 last 标志

    CacheRespBundle() = default;
    CacheRespBundle(uint64_t tid, uint64_t d, bool hit, uint8_t err = 0)
        : transaction_id(tid), parent_id(0), fragment_id(0), fragment_total(1)
        , data(d), is_hit(hit), error_code(err), first(true), last(true) {}

    // 谓词
    bool is_first_fragment() const {
        return fragment_id.read() == 0 || first.read();
    }
    bool is_last_fragment() const {
        return fragment_id.read() + 1 >= fragment_total.read() || last.read();
    }
};

} // namespace bundles

#endif // BUNDLES_CACHE_BUNDLES_TLM_HH
