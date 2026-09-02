// include/tlm/pcie/pcie_axi_adapter_tlm.hh
// PcieAxiAdapter: PCIe Endpoint IP 的 AXI 事务边界适配器
// 功能描述：在 PcieEndpointIP 与 SoC 之间建立 AXI 事务边界。
//           持有 Axi4StreamAdapter（axi_master_out / axi_slave_in / cfg_slave_in
//           三端口），并提供 64-byte burst 写序列化（awlen/awsize 驱动的多拍
//           W 通道 + wlast）。
// 作者 CppTLM Team / 日期 2026-11-03
// 参考: openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/spec.md
//       openspec/changes/2026-09-01-cpptlm-dgpu-pcie-ip-microarch/design.md §6
#ifndef TLM_PCIE_PCIE_AXI_ADAPTER_TLM_HH
#define TLM_PCIE_PCIE_AXI_ADAPTER_TLM_HH

#include "core/event_queue.hh"
#include "framework/axi4_stream_adapter.hh"

#include <cstdint>
#include <deque>
#include <string>

namespace tlm::pcie {

class PcieEndpointIP;

/**
 * @brief PcieEndpointIP 的 AXI 事务边界适配器
 *
 * 职责：
 *   - 持有 Axi4StreamAdapter（axi_master_out / axi_slave_in / cfg_slave_in）
 *   - 将 EP 发起的 64-byte burst 写序列化为多拍 W 通道（wlast 最后一拍置位）
 *   - 总传输字节 = (awlen+1) × 2^awsize（per spec.md Scenario "64-byte burst"）
 *   - 绑定 PcieEndpointIP（endpoint() 访问器），后续 composition 接线到
 *     PcieEndpointIP 数据路径（T-P5-6）
 */
class PcieAxiAdapter {
public:
    PcieAxiAdapter(PcieEndpointIP* ep, EventQueue* eq);
    PcieAxiAdapter(const PcieAxiAdapter&) = delete;
    PcieAxiAdapter& operator=(const PcieAxiAdapter&) = delete;

    // ===================== composition 接线（静态注册表）=====================
    // 按 EP 模块名挂接/查找/detach（与 PcieLinkLayer 同模式；.h 布局冻结时用）。
    // 由 PcieEndpointTLM/PcieEndpointIP 的 on_config_loaded() 在 JSON 含
    // "axi_adapter" 时调用。
    static PcieAxiAdapter* attach_to_endpoint(const std::string& endpoint_name,
                                              EventQueue* eq);
    static PcieAxiAdapter* for_endpoint(const std::string& endpoint_name) noexcept;
    static void detach_from_endpoint(const std::string& endpoint_name) noexcept;

    // 绑定的 Endpoint
    PcieEndpointIP* endpoint() const noexcept { return ep_; }
    EventQueue* event_queue() const noexcept { return eq_; }
    // composition 挂接后绑定真实 EP（attach_to_endpoint 先注册, 再 set_endpoint 回填）
    void set_endpoint(PcieEndpointIP* ep) noexcept { ep_ = ep; }

    // 底层 AXI Stream Adapter（三端口访问）
    cpptlm::Axi4StreamAdapter& axi() noexcept { return axi_; }
    const cpptlm::Axi4StreamAdapter& axi() const noexcept { return axi_; }

    // ===================== 64-byte burst 写序列化 =====================
    // 启动一个 burst 写事务（登记 awlen/awsize/awaddr/awid，计算总字节）。
    // 返回 false 表示已有 burst 在途（backpressure）。
    bool master_write_burst(const bundles::Axi4Bundle& req);

    // 当前 burst 的拍数信息（测试断言）
    uint8_t awlen() const noexcept { return awlen_; }
    uint8_t awsize() const noexcept { return awsize_; }
    uint32_t total_bytes() const noexcept { return total_bytes_; }

    // 写入一拍数据（beat_index 内校验）。返回 false 表示无 burst 或越界。
    bool write_beat(uint64_t data, uint64_t strb);

    // 当前拍是否为最后一拍（wlast）
    bool current_beat_is_last() const noexcept;

    // 把当前拍推送到下游 Axi4StreamAdapter（master_req）。成功则推进 beat。
    bool push_beat_to_downstream();

    // 已发送到下游的拍数
    std::size_t beats_sent() const noexcept { return beats_sent_; }

    // burst 是否已完成（全部拍已推送）
    bool burst_complete() const noexcept;

    // 复位 burst 状态机（不触碰 axi_ 通道状态）
    void reset_burst();

private:
    PcieEndpointIP* ep_ = nullptr;
    EventQueue* eq_ = nullptr;
    cpptlm::Axi4StreamAdapter axi_;

    // ---- burst 状态 ----
    bool burst_active_ = false;
    uint8_t awid_ = 0;
    uint64_t awaddr_ = 0;
    uint8_t awlen_ = 0;
    uint8_t awsize_ = 0;
    uint8_t awburst_ = 0;
    uint32_t total_bytes_ = 0;
    std::size_t num_beats_ = 0;
    std::size_t beat_index_ = 0;
    std::size_t beats_sent_ = 0;

    // 当前拍暂存
    uint64_t cur_data_ = 0;
    uint64_t cur_strb_ = 0;
    bool cur_written_ = false;

    // burst 写 outstanding ID（仅首拍登记，整体作为一个事务）
    std::deque<uint16_t> outstanding_wr_ids_;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_AXI_ADAPTER_TLM_HH
