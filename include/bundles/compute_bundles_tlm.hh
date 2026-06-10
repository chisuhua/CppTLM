// include/bundles/compute_bundles_tlm.hh
// Compute Bundle 定义（GPU 发起请求 / 响应，轻量级 TLM 侧）
// 功能描述：定义 GPU Compute 请求/响应 Bundle，在 CacheReq/Resp 字段基础上
//           扩展 GPU 维度字段（kernel_id / workgroup_id / wavefront_id /
//           coalescing_factor），支持 Phase7.A 端到端 GPU 消息流通验证。
//           沿用 bundles::bundle_base 基类，POD 兼容 bundle_serialization.hh。
// 作者 CppTLM Team / 日期 2026-06-11
// 参考：docs/superpowers/specs/2026-06-11-phase7a-gpu-infra-design.md §2
//      gem5 src/gpu-compute/ComputeUnit.py (字段语义对位)
#ifndef BUNDLES_COMPUTE_BUNDLES_TLM_HH
#define BUNDLES_COMPUTE_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief GPU Compute 请求 Bundle（轻量级 TLM 侧）
 *
 * 字段分为两部分：
 *   - 继承自 CacheReqBundle 风格的 8 个核心字段（transaction 元数据 + 地址/数据）
 *   - GPU 特定字段：kernel_id / workgroup_id / wavefront_id / coalescing_factor
 *
 * 与 CacheReqBundle 的差异：
 *   - 字段集为 CacheReqBundle 的超集（组合而非继承，保持 POD 兼容 memcpy）
 *   - GPU 字段对位 gem5：kernel_id ≈ AQL dispatch_id，workgroup_id ≈ ComputeUnit
 *     dispWorkgroup(wg_id)，wavefront_id ≈ Wavefront.wfSlotId，coalescing_factor
 *     抽象 VIPERCoalescer::coalesce_factor
 */
struct ComputeReqBundle : public bundle_base {
    // === 核心字段（与 CacheReqBundle 对位）===
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;        // 0 = 根事务
    ch_uint<8>  fragment_id;      // 当前拍序号（0-based）
    ch_uint<8>  fragment_total;   // 总拍数（1 = 不分片）
    ch_uint<64> address;
    ch_uint<8>  size;
    ch_bool     is_write;
    ch_uint<64> data;

    // === GPU 特定字段 ===
    ch_uint<32> kernel_id;
    ch_uint<32> workgroup_id;
    ch_uint<32> wavefront_id;
    ch_uint<32> coalescing_factor;

    ComputeReqBundle() = default;

    ComputeReqBundle(uint64_t tid, uint32_t kid, uint32_t wgid, uint32_t wfid,
                     uint64_t addr, uint8_t sz, bool wr, uint64_t d,
                     uint32_t cf = 1)
        : transaction_id(tid), parent_id(0), fragment_id(0), fragment_total(1)
        , address(addr), size(sz), is_write(wr), data(d)
        , kernel_id(kid), workgroup_id(wgid), wavefront_id(wfid)
        , coalescing_factor(cf) {}

    bool is_first_fragment() const { return fragment_id.read() == 0; }
    bool is_last_fragment() const {
        return fragment_id.read() + 1 >= fragment_total.read();
    }
    bool is_root() const {
        return parent_id.read() == 0 && fragment_total.read() == 1;
    }
};

/**
 * @brief GPU Compute 响应 Bundle（轻量级 TLM 侧）
 *
 * 字段：8 个核心字段 + 3 个 GPU 维度字段（kernel_id / workgroup_id / wavefront_id）。
 * 注：响应侧不需要 coalescing_factor（合并是请求侧的事）。
 */
struct ComputeRespBundle : public bundle_base {
    ch_uint<64> transaction_id;
    ch_uint<64> parent_id;
    ch_uint<8>  fragment_id;
    ch_uint<8>  fragment_total;
    ch_uint<64> data;
    ch_bool     is_hit;
    ch_uint<8>  error_code;
    ch_bool     first;
    ch_bool     last;

    ch_uint<32> kernel_id;
    ch_uint<32> workgroup_id;
    ch_uint<32> wavefront_id;

    ComputeRespBundle() = default;

    ComputeRespBundle(uint64_t tid, uint32_t kid, uint32_t wgid, uint32_t wfid,
                      uint64_t d, bool hit, uint8_t err = 0)
        : transaction_id(tid), parent_id(0), fragment_id(0), fragment_total(1)
        , data(d), is_hit(hit), error_code(err), first(true), last(true)
        , kernel_id(kid), workgroup_id(wgid), wavefront_id(wfid) {}

    bool is_first_fragment() const {
        return fragment_id.read() == 0 || first.read();
    }
    bool is_last_fragment() const {
        return fragment_id.read() + 1 >= fragment_total.read() || last.read();
    }
};

} // namespace bundles

#endif // BUNDLES_COMPUTE_BUNDLES_TLM_HH