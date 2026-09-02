// include/framework/axi4_signal_to_bundle.hh
// AXI4 Signal → Axi4Bundle 转换（信号接口到 TLM Bundle）
// 功能描述：将原始 AXI 信号字段（平面结构体）转换为 Axi4Bundle
// 作者 CppTLM Team / 日期 2026-12-22
// 参考: openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/tasks.md T-P6-1

#ifndef FRAMEWORK_AXI4_SIGNAL_TO_BUNDLE_HH
#define FRAMEWORK_AXI4_SIGNAL_TO_BUNDLE_HH

#include "bundles/axi4_bundles_tlm.hh"

#include <cstdint>

namespace cpptlm {

/**
 * @brief AXI4 信号平面结构体（硬件接口视角）
 *
 * 这是与 RTL 交互的信号层面定义，仅包含纯数据字段，无封装。
 * 字段宽度与 Axi4Bundle 对应（per design.md §6.2/6.3）。
 */
struct AXI4Signals {
    // Write Address Channel
    uint64_t awaddr = 0;
    uint8_t  awlen = 0;
    uint8_t  awsize = 0;
    uint8_t  awburst = 0;
    uint16_t awid = 0;

    // Write Data Channel
    uint64_t wdata = 0;    // 512-bit (stored as uint64_t per cpphdl_types.hh)
    uint64_t wstrb = 0;    // 64-bit strobe
    uint8_t  wlast = 0;

    // Write Response Channel
    uint16_t bid = 0;
    uint8_t  bresp = 0;

    // Read Address Channel
    uint64_t araddr = 0;
    uint8_t  arlen = 0;
    uint8_t  arsize = 0;
    uint8_t  arburst = 0;
    uint16_t arid = 0;

    // Read Data Channel
    uint16_t rid = 0;
    uint64_t rdata = 0;    // 512-bit (stored as uint64_t)
    uint8_t  rresp = 0;
    uint8_t  rlast = 0;
};

/**
 * @brief 将 Axi4Bundle 转换为 AXI4Signals
 * @param bundle 源 Bundle（TLM 侧）
 * @param signals 目标信号结构体（RTL/接口侧）
 */
void bundle_to_signal(const bundles::Axi4Bundle& bundle, AXI4Signals& signals);

} // namespace cpptlm

#endif // FRAMEWORK_AXI4_SIGNAL_TO_BUNDLE_HH