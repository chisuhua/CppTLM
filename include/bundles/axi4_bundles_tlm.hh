// include/bundles/axi4_bundles_tlm.hh
// AXI4 / AXI4-Lite Bundle 定义（轻量级 TLM 侧）
// 功能描述：定义 dGPU SoC PCIe Endpoint IP 的 AXI Stream Adapter 使用的事务 Bundle
//           - Axi4Bundle   ：标准 AXI4 事务（写地址/写数据/写响应/读地址/读数据）
//           - Axi4LiteBundle：AXI4-Lite 配置访问（aw/ar/w/b/r 通道）
//           包含请求与响应 ID（awid/arid/bid/rid）以支持 Phase 6 out-of-order 匹配。
// 作者 CppTLM Team / 日期 2026-11-03
// 参考: openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/spec.md
//       openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §6.2/6.3
#ifndef BUNDLES_AXI4_BUNDLES_TLM_HH
#define BUNDLES_AXI4_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief AXI4 Bundle（轻量级 TLM 侧）
 *
 * 字段（per design.md §6.2/6.3，宽度全文档冻结）：
 *   写地址通道:
 *     - awaddr  : 64 bits 写地址
 *     - awlen   : burst length（-1，8-bit）
 *     - awsize  : 每拍字节数 2^awsize（3-bit）
 *     - awburst : burst 类型（FIXED/INCR/WRAP，2-bit）
 *     - awid    : 16 bits 写请求 ID
 *   写数据通道:
 *     - wdata   : 512 bits 写数据（Gen5 AXI 512-bit）
 *     - wstrb   : 写 strobe（512/8=64 bits）
 *     - wlast   : 最后一拍
 *   写响应通道:
 *     - bid     : 16 bits 写响应 ID（与 awid 关联）
 *     - bresp   : 写响应状态（2-bit）
 *   读地址通道:
 *     - araddr  : 64 bits 读地址
 *     - arlen   : burst length（-1，8-bit）
 *     - arsize  : 每拍字节数 2^arsize（3-bit）
 *     - arburst : burst 类型（2-bit）
 *     - arid    : 16 bits 读请求 ID
 *   读数据通道:
 *     - rid     : 16 bits 读响应 ID（与 arid 关联，Phase 6 OOO 匹配）
 *     - rdata   : 512 bits 读数据
 *     - rresp   : 读响应状态（2-bit）
 *     - rlast   : 最后一拍
 *
 * 设计原则：
 *   - 轻量级：仅 POD 字段，无 CppHDL AST 依赖
 *   - C++17 兼容：可在 cpptlm_core（C++17 静态库）中使用
 *   - POD 惯例：不携带 shared_ptr/inline buffer 等堆指针（per ADR-SOC-07 Status Update Q3）
 *   - ch_uint<512> 实际以 uint64_t 存储（仿真精度足够，见 cpphdl_types.hh）
 */
struct Axi4Bundle : public bundle_base {
    // ========== 写地址通道 ==========
    ch_uint<64> awaddr;   // 写地址
    ch_uint<8>  awlen;    // burst length (len = beats - 1)
    ch_uint<8>  awsize;   // 每拍字节数 2^awsize
    ch_uint<8>  awburst;  // burst 类型
    ch_uint<16> awid;     // 写请求 ID

    // ========== 写数据通道 ==========
    ch_uint<512> wdata;   // 写数据
    ch_uint<64>  wstrb;   // 写 strobe (512/8=64)
    ch_bool      wlast;   // 最后一拍

    // ========== 写响应通道 ==========
    ch_uint<16> bid;      // 写响应 ID（与 awid 关联）
    ch_uint<8>  bresp;    // 写响应状态 (OKAY=0)

    // ========== 读地址通道 ==========
    ch_uint<64> araddr;   // 读地址
    ch_uint<8>  arlen;    // burst length (len = beats - 1)
    ch_uint<8>  arsize;   // 每拍字节数 2^arsize
    ch_uint<8>  arburst;  // burst 类型
    ch_uint<16> arid;     // 读请求 ID

    // ========== 读数据通道 ==========
    ch_uint<16> rid;      // 读响应 ID（与 arid 关联）
    ch_uint<512> rdata;   // 读数据
    ch_uint<8>  rresp;    // 读响应状态 (OKAY=0)
    ch_bool     rlast;    // 最后一拍

    Axi4Bundle() = default;

    // 谓词：写请求是否有效（awid 非 0 或 awlen 非 0）
    bool is_write_request() const {
        return awlen.read() > 0 || awid.read() > 0 || awaddr.read() > 0;
    }
};

/**
 * @brief AXI4-Lite Bundle（轻量级 TLM 侧）
 *
 * 字段（per design.md §6.2，AXI4-Lite 单拍事务，无 burst）：
 *     - awaddr : 写地址（64-bit）
 *     - awid   : 写请求 ID（16-bit）
 *     - wdata  : 写数据（32-bit AXI4-Lite）
 *     - wstrb  : 写 strobe（4-bit）
 *     - bresp  : 写响应状态
 *     - araddr : 读地址（64-bit）
 *     - arid   : 读请求 ID（16-bit）
 *     - rdata  : 读数据（32-bit）
 *     - rresp  : 读响应状态
 *
 * 用途：cfg_slave_in 配置访问端口（AXI4-Lite）。
 */
struct Axi4LiteBundle : public bundle_base {
    ch_uint<64> awaddr;   // 写地址
    ch_uint<16> awid;     // 写请求 ID
    ch_uint<32> wdata;    // 写数据
    ch_uint<8>  wstrb;    // 写 strobe
    ch_uint<8>  bresp;    // 写响应状态
    ch_uint<64> araddr;   // 读地址
    ch_uint<16> arid;     // 读请求 ID
    ch_uint<32> rdata;    // 读数据
    ch_uint<8>  rresp;    // 读响应状态

    Axi4LiteBundle() = default;
};

} // namespace bundles

#endif // BUNDLES_AXI4_BUNDLES_TLM_HH
