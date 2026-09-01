// include/tlm/pcie/pcie_encoding_latency_model.hh
// PcieEncodingLatencyModel: 128b/130b 编码延迟建模器 (Gen1-5)
// 功能描述：
//   - 块传输延迟: block_latency_ns = (block_bytes × 8) / gen_rate_GT/s / active_lanes
//     (多 lane 并行加速, per spec.md Scenario "Gen5 链路延迟(x16 lane)")
//   - 速率切换延迟: ~µs 级, 含 Gen3+ 均衡协商 (per spec.md Scenario "速率切换延迟")
//   - 仅建模延迟, 不实现 bit-level 130b 编码/解码 (per Oracle Q1)
// 单位修正(per Oracle 二次评审 C2/C3):
//   - 速率单位: GT/s per-lane per-direction (非 MB/s, 非 MT/s)
//   - Gen5 x1 128B 块 = 32ns; Gen5 x16 = 2ns; Gen3 x1 = 128ns
// 作者 CppTLM Team / 日期 2026-09-29
// 参考: openspec/changes/2026-09-29-cpptlm-dgpu-pcie-130b-encoding/
//       specs/130b-encoding/spec.md "Requirement: encoding-latency-model"
#ifndef TLM_PCIE_PCIE_ENCODING_LATENCY_MODEL_HH
#define TLM_PCIE_PCIE_ENCODING_LATENCY_MODEL_HH

#include <cstddef>
#include <cstdint>

namespace tlm::pcie {

/**
 * @brief 128b/130b 编码延迟建模器 (纯静态, 无状态)
 *
 * 提供两个延迟模型:
 *   - block_latency_ns : 128B (1024-bit) 块传输延迟, 单位 ns
 *   - rate_switch_delay_us : 速率切换延迟 (含均衡协商), 单位 µs
 *
 * 注意: 项目无 SystemC sc_time (USE_SYSTEMC_STUB 仅提供 TLM 2.0 桩),
 * 延迟以 uint64_t ns / µs 表示, 由调用方 (PcieLinkLayer Tx/Rx path) 折算。
 */
class PcieEncodingLatencyModel {
public:
    // GT/s per-lane per-direction (per Oracle C2/C3 单位修正)
    // 注: Gen1 真值为 2.5 GT/s, 枚举取整为 2 (per proposal.md Risk R1)
    enum class Rate : uint32_t {
        GEN1 = 2,   // 2.5 GT/s (取整)
        GEN2 = 5,
        GEN3 = 8,
        GEN4 = 16,
        GEN5 = 32
    };

    // 返回指定 Gen 速率 (GT/s per-lane per-direction)
    static uint32_t rate_gtps(Rate rate) noexcept {
        return static_cast<uint32_t>(rate);
    }

    // 块传输延迟 (ns): (block_bytes × 8) / rate_GT/s / active_lanes
    //   边界: active_lanes == 0 或 block_bytes == 0 → 0 (保护, 防除零)
    //   Gen5 x1 128B = 32ns; Gen5 x16 128B = 2ns; Gen3 x1 128B = 128ns
    static uint64_t block_latency_ns(Rate rate, std::size_t block_bytes,
                                     std::size_t active_lanes) noexcept;

    // 速率切换延迟 (µs): ~µs 级, 含 Gen3+ 均衡协商
    //   同速率 → 0µs (无协商); 跨 Gen → >= 1µs (链路重训练 + 均衡)
    static uint64_t rate_switch_delay_us(Rate from, Rate to) noexcept;

private:
    // Gen 档位索引 (GEN1=0 .. GEN5=4), 用于切换延迟分层
    static uint32_t gen_index(Rate rate) noexcept;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_ENCODING_LATENCY_MODEL_HH