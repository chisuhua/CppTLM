// include/tlm/pcie/host_bypass_tlm.hh
// HostBypassTLM: 独立 Host 侧桥接组件（软件 bring-up 跳过 RC BFM）
// 功能描述：Phase 7 新组件。软件 bring-up 阶段跳过 PCIe Root Complex BFM，
//           直接在 Host 侧与 PcieEndpointIP 之间建立 AXI 桥接（依赖 Phase 5
//           Axi4StreamAdapter 三端口：axi_master_out / axi_slave_in / cfg_slave_in）。
//           持有 Axi4StreamAdapter 提供 valid/ready 反压语义（不丢事务），
//           并持有 PcieEndpointIP* 引用（attach_to_endpoint / detach）。
// 作者 CppTLM Team / 日期 2027-01-19
// 参考: openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/spec.md
//       openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/spec.md (Phase 5)
#ifndef TLM_PCIE_HOST_BYPASS_TLM_HH
#define TLM_PCIE_HOST_BYPASS_TLM_HH

#include "core/event_queue.hh"
#include "framework/axi4_stream_adapter.hh"

#include <cstdint>
#include <string>

namespace tlm::pcie {

class PcieEndpointIP;

/**
 * @brief HostBypassTLM：软件 bring-up 的 Host 侧 AXI 桥接组件
 *
 * 职责：
 *   - 在软件（Host）与 PcieEndpointIP 之间建立 AXI 事务边界
 *   - 持有 Axi4StreamAdapter（axi_master_out / axi_slave_in / cfg_slave_in 三端口）
 *   - attach_to_endpoint() 挂接 PcieEndpointIP 引用；detach() 解绑
 *   - tick() 转发至底层 Axi4StreamAdapter（valid/ready 反压，不丢事务）
 *
 * 与 PcieAxiAdapter 的关系：PcieAxiAdapter 是 EP 侧 AXI 边界（挂在
 * PcieEndpointIP 内），HostBypassTLM 是 Host 侧独立组件，二者通过 AXI
 * 事务语义对接，形成完整的 Host ↔ EP 桥接路径。
 */
class HostBypassTLM {
public:
    HostBypassTLM(const std::string& name, EventQueue* eq);
    ~HostBypassTLM() = default;

    HostBypassTLM(const HostBypassTLM&) = delete;
    HostBypassTLM& operator=(const HostBypassTLM&) = delete;

    // 初始化（幂等）
    void init();

    // ===================== composition 接线 =====================
    // 挂接 PcieEndpointIP（HostBypassTLM 作为 Host 侧发起器访问 EP）
    void attach_to_endpoint(PcieEndpointIP* ep) noexcept;
    // 解绑 EP 引用
    void detach() noexcept;
    // 是否已挂接 EP
    bool is_attached() const noexcept { return ep_ != nullptr; }
    // 绑定的 Endpoint
    PcieEndpointIP* endpoint() const noexcept { return ep_; }
    // EventQueue（周期推进）
    EventQueue* event_queue() const noexcept { return eq_; }

    // 底层 AXI Stream Adapter（三端口访问，Host 侧视图）
    cpptlm::Axi4StreamAdapter& axi() noexcept { return axi_; }
    const cpptlm::Axi4StreamAdapter& axi() const noexcept { return axi_; }

    // ===================== axi_master_out（Host 发起 SoC 访问）=====================
    bool axi_master_req(const bundles::Axi4Bundle& req) { return axi_.master_req(req); }
    void set_axi_master_ready(bool r) { axi_.set_master_ready(r); }
    bool axi_master_req_valid() const { return axi_.master_req_valid(); }
    const bundles::Axi4Bundle& axi_master_req_data() const { return axi_.master_req_data(); }
    void axi_master_req_consume() { axi_.master_req_consume(); }
    bool axi_master_resp(const bundles::Axi4Bundle& resp) { return axi_.master_resp(resp); }
    bool axi_master_resp_valid() const { return axi_.master_resp_valid(); }
    const bundles::Axi4Bundle& axi_master_resp_data() const { return axi_.master_resp_data(); }
    void axi_master_resp_consume() { axi_.master_resp_consume(); }
    std::size_t axi_outstanding_wr() const { return axi_.outstanding_wr(); }
    std::size_t axi_outstanding_rd() const { return axi_.outstanding_rd(); }

    // ===================== axi_slave_in（Host 发起进入 Endpoint 的事务）=====================
    bool axi_slave_req(const bundles::Axi4Bundle& req) { return axi_.slave_req(req); }
    void set_axi_slave_ready(bool r) { axi_.set_slave_ready(r); }
    bool axi_slave_req_valid() const { return axi_.slave_req_valid(); }
    const bundles::Axi4Bundle& axi_slave_req_data() const { return axi_.slave_req_data(); }
    void axi_slave_req_consume() { axi_.slave_req_consume(); }
    bool axi_slave_resp(const bundles::Axi4Bundle& resp) { return axi_.slave_resp(resp); }
    bool axi_slave_resp_valid() const { return axi_.slave_resp_valid(); }
    const bundles::Axi4Bundle& axi_slave_resp_data() const { return axi_.slave_resp_data(); }
    void axi_slave_resp_consume() { axi_.slave_resp_consume(); }

    // ===================== cfg_slave_in（AXI4-Lite 配置访问）=====================
    bool axi_cfg_req(const bundles::Axi4LiteBundle& req) { return axi_.cfg_req(req); }
    void set_axi_cfg_ready(bool r) { axi_.set_cfg_ready(r); }
    bool axi_cfg_req_valid() const { return axi_.cfg_req_valid(); }
    const bundles::Axi4LiteBundle& axi_cfg_req_data() const { return axi_.cfg_req_data(); }
    void axi_cfg_req_consume() { axi_.cfg_req_consume(); }
    bool axi_cfg_resp(const bundles::Axi4LiteBundle& resp) { return axi_.cfg_resp(resp); }
    bool axi_cfg_resp_valid() const { return axi_.cfg_resp_valid(); }
    const bundles::Axi4LiteBundle& axi_cfg_resp_data() const { return axi_.cfg_resp_data(); }
    void axi_cfg_resp_consume() { axi_.cfg_resp_consume(); }

    // ===================== 软件 bring-up API (T-P7-2) =====================
    // 配置空间写：经 AXI 边界直达 EP 配置空间（stream_id 0=PF, 1..16=VF0..VF15）。
    // 返回 false 表示未挂接 EP 或 EP 不可达。
    bool config_write(uint16_t offset, uint32_t value, uint16_t stream_id = 0);
    // 配置空间读：返回 EP 配置空间实际存储值（未挂接 EP 时返回 0xFFFFFFFF）。
    uint32_t config_read(uint16_t offset, uint16_t stream_id = 0);
    // BAR 空间写：经 AXI master 通道路由到 EP（bytes=1/2/4/8）。
    // 返回 false 表示未挂接 EP 或通道忙。
    bool bar_write(uint64_t addr, uint64_t data, uint8_t bytes);
    // BAR 空间读：经 AXI master 通道路由到 EP，data 输出读回值。
    // 返回 false 表示未挂接 EP 或通道忙。
    bool bar_read(uint64_t addr, uint64_t& data, uint8_t bytes);

    // 每周期推进（转发至底层 Axi4StreamAdapter）
    void tick() { axi_.tick(); }
    // 全量复位
    void reset() { axi_.reset(); }

private:
    // 由 awsize（2^awsize 字节/拍）计算 burst 总字节数
    static uint8_t bytes_to_awsize(uint8_t bytes);

    std::string name_;
    EventQueue* eq_;
    PcieEndpointIP* ep_ = nullptr;
    cpptlm::Axi4StreamAdapter axi_;
    // BAR 事务 ID 计数器（每次 bar_write/bar_read 自增）
    uint16_t bar_tx_id_ = 0;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_HOST_BYPASS_TLM_HH