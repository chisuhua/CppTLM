// include/framework/axi4_bundle_to_signal.hh
// AXI4Signals → Axi4Bundle 转换（信号接口到 TLM Bundle 反向）
// 功能描述：将原始 AXI 信号字段（平面结构体）转换为 Axi4Bundle
// 作者 CppTLM Team / 日期 2026-12-22
// 参考: openspec/changes/2026-12-22-cpptlm-dgpu-axi4-mapper/tasks.md T-P6-1

#ifndef FRAMEWORK_AXI4_BUNDLE_TO_SIGNAL_HH
#define FRAMEWORK_AXI4_BUNDLE_TO_SIGNAL_HH

#include "bundles/axi4_bundles_tlm.hh"
#include "framework/axi4_signal_to_bundle.hh"

#include <cstdint>

namespace cpptlm {

/**
 * @brief 将 AXI4Signals 转换为 Axi4Bundle
 * @param signals 源信号结构体（RTL/接口侧）
 * @param bundle 目标 Bundle（TLM 侧）
 */
void signal_to_bundle(const AXI4Signals& signals, bundles::Axi4Bundle& bundle);

} // namespace cpptlm

#endif // FRAMEWORK_AXI4_BUNDLE_TO_SIGNAL_HH