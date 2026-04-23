// include/bundles/noc_bundles_tlm.hh
// NoC Flit Bundle 定义（统一类型，支持 REQUEST 和 RESPONSE）
// 功能描述：定义 NoC 统一 Flit Bundle，支持包化请求和响应复用同一类型
// 作者 CppTLM Team / 日期 2026-04-23
#ifndef BUNDLES_NOC_BUNDLES_TLM_HH
#define BUNDLES_NOC_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief NoC 统一 Flit Bundle (v2.1)
 *
 * 设计决策: 路由器端口统一使用 NoCFlitBundle 类型，而非分离的 ReqBundle/RespBundle。
 * 通过 flit_category 字段区分 REQUEST (0) 和 RESPONSE (1)。
 *
 * 字段设计参考:
 * - transaction_id: 关联原始 CacheReq/CacheResp
 * - src_node/dst_node: Mesh 节点坐标
 * - flit_type: HEAD/BODY/TAIL/HEAD_TAIL (wormhole 分片)
 * - flit_category: REQUEST/RESPONSE 区分
 * - vc_id: 虚拟通道，支持多 VC 流控
 */
struct NoCFlitBundle : public bundle_base {
    // ========== 事务元数据 ==========
    ch_uint<64> transaction_id;   // 关联原始请求/响应的 transaction_id
    ch_uint<32> src_node;        // 源节点 ID (NICTLM 节点号，Mesh XY 坐标)
    ch_uint<32> dst_node;        // 目标节点 ID

    // ========== 地址与数据 ==========
    ch_uint<64> address;          // 访问地址 (仅 REQUEST 使用)
    ch_uint<64> data;            // 数据载荷 (8 字节/拍)
    ch_uint<8>  size;            // 原始请求总大小 (字节)

    // ========== NoC 路由与流控 ==========
    ch_uint<8>  vc_id;           // 虚拟通道 ID (0-3)
    ch_uint<8>  flit_type;       // 0=HEAD, 1=BODY, 2=TAIL, 3=HEAD_TAIL
    ch_uint<8>  flit_index;      // 当前 flit 在分组中的索引 (0, 1, 2, ...)
    ch_uint<8>  flit_count;     // 总 flit 数量 (1, 2, 3, ...)
    ch_uint<8>  hops;             // 已跳数计数 (每 router 转发+1)

    // ========== 请求/响应区分 (v2.1 统一类型) ==========
    ch_uint<8>  flit_category;   // 0=REQUEST, 1=RESPONSE
    ch_bool     is_write;         // 读/写标记 (仅 REQUEST 使用)
    ch_bool     is_ok;            // 成功/失败 (仅 RESPONSE 使用)
    ch_uint<8>  error_code;      // 错误码 (0=OK)

    NoCFlitBundle() = default;

    // ========== Flit 类型常量 ==========
    static constexpr uint8_t FLIT_HEAD      = 0;
    static constexpr uint8_t FLIT_BODY      = 1;
    static constexpr uint8_t FLIT_TAIL     = 2;
    static constexpr uint8_t FLIT_HEAD_TAIL = 3;

    // ========== Flit 类别常量 (v2.1) ==========
    static constexpr uint8_t CATEGORY_REQUEST  = 0;
    static constexpr uint8_t CATEGORY_RESPONSE = 1;

    // ========== 辅助方法 ==========

    /** @brief 判断是否是 HEAD flit (包含路由信息) */
    bool is_head() const {
        return flit_type.read() == FLIT_HEAD || flit_type.read() == FLIT_HEAD_TAIL;
    }

    /** @brief 判断是否是 TAIL flit (分组结束) */
    bool is_tail() const {
        return flit_type.read() == FLIT_TAIL || flit_type.read() == FLIT_HEAD_TAIL;
    }

    /** @brief 判断是否是 REQUEST 类别 */
    bool is_request() const {
        return flit_category.read() == CATEGORY_REQUEST;
    }

    /** @brief 判断是否是 RESPONSE 类别 */
    bool is_response() const {
        return flit_category.read() == CATEGORY_RESPONSE;
    }

    /** @brief 创建 REQUEST HEAD flit 的便捷工厂方法 */
    static NoCFlitBundle make_head(uint64_t tid, uint32_t src, uint32_t dst,
                                    uint64_t addr, uint8_t vc, uint8_t total_flits,
                                    bool write, uint64_t data_word) {
        NoCFlitBundle flit;
        flit.transaction_id.write(tid);
        flit.src_node.write(src);
        flit.dst_node.write(dst);
        flit.address.write(addr);
        flit.vc_id.write(vc);
        flit.flit_index.write(0);
        flit.flit_count.write(total_flits);
        flit.hops.write(0);
        flit.flit_category.write(CATEGORY_REQUEST);
        flit.is_write.write(write);
        flit.data.write(data_word);

        if (total_flits == 1) {
            flit.flit_type.write(FLIT_HEAD_TAIL);
        } else {
            flit.flit_type.write(FLIT_HEAD);
        }
        return flit;
    }

    /** @brief 创建 RESPONSE HEAD flit 的便捷工厂方法 */
    static NoCFlitBundle make_resp_head(uint64_t tid, uint32_t src, uint32_t dst,
                                          uint8_t vc, uint8_t total_flits,
                                          bool ok = true, uint8_t err = 0) {
        NoCFlitBundle flit;
        flit.transaction_id.write(tid);
        flit.src_node.write(src);
        flit.dst_node.write(dst);
        flit.vc_id.write(vc);
        flit.flit_index.write(0);
        flit.flit_count.write(total_flits);
        flit.hops.write(0);
        flit.flit_category.write(CATEGORY_RESPONSE);
        flit.is_ok.write(ok);
        flit.error_code.write(err);
        flit.size.write(0);

        if (total_flits == 1) {
            flit.flit_type.write(FLIT_HEAD_TAIL);
        } else {
            flit.flit_type.write(FLIT_HEAD);
        }
        return flit;
    }
};

} // namespace bundles

#endif // BUNDLES_NOC_BUNDLES_TLM_HH