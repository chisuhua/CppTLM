// include/tlm/pcie/pcie_root_complex_tlm.hh
// PcieRootComplexTLM: 可选 Root Complex 镜像模型（PCIe 枚举 + 配置访问）
// 功能描述：Phase 7 新组件（可选）。自研 RC 模型，镜像 PcieEndpointIP，
//           支持 PCIe 枚举（发现设备/功能）、配置空间读/写、BAR 分配与访问路由。
//           依赖 Phase 5 AXI 接口（Axi4StreamAdapter）桥接 EP。
// 作者 CppTLM Team / 日期 2027-01-19
// 参考: openspec/changes/2027-01-19-cpptlm-dgpu-pcie-host-bypass-and-rc/spec.md
//       openspec/changes/2026-11-03-cpptlm-dgpu-axi-stream-adapter/spec.md (Phase 5)
#ifndef TLM_PCIE_PCIE_ROOT_COMPLEX_TLM_HH
#define TLM_PCIE_PCIE_ROOT_COMPLEX_TLM_HH

#include "core/event_queue.hh"
#include "framework/axi4_stream_adapter.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace tlm::pcie {

class PcieEndpointIP;

/**
 * @brief PCIe 设备/功能信息（枚举结果）
 */
struct DiscoveredDevice {
    uint16_t device_id = 0;      // 设备号
    uint16_t function = 0;       // 功能号 (0..7)
    uint16_t vendor_id = 0;      // 来自 EP 配置空间
    uint16_t device_id_reg = 0;  // device_id 寄存器
    uint32_t class_code = 0;     // 类别代码
    uint8_t  revision_id = 0;    // 修订号
};

/**
 * @brief PcieRootComplexTLM：自研 Root Complex 模型（可选）
 *
 * 职责：
 *   - 执行 PCIe 枚举：发现 EP 设备/功能（PF0 + VF0..VF15）
 *   - 配置空间读/写（路由到 EP 的 PcieConfigSpace）
 *   - BAR 分配：在 EP 配置空间写入 BAR 基址/属性
 *   - BAR 访问路由：经 AXI master 通道将 BAR 空间读写转发到 EP
 *   - 持有 Axi4StreamAdapter（三端口），与 HostBypassTLM 类似
 *   - attach_to_endpoint() 挂接 PcieEndpointIP 引用；detach() 解绑
 *
 * 与 HostBypassTLM 的关系：两者都是 Host 侧组件，都桥接 AXI ↔ EP。
 * HostBypassTLM 面向软件 bring-up（跳过 RC BFM），PcieRootComplexTLM
 * 面向枚举/配置验证（镜像 RC 行为）。可根据场景单独或共同使用。
 */
class PcieRootComplexTLM {
public:
    PcieRootComplexTLM(const std::string& name, EventQueue* eq);
    ~PcieRootComplexTLM() = default;

    PcieRootComplexTLM(const PcieRootComplexTLM&) = delete;
    PcieRootComplexTLM& operator=(const PcieRootComplexTLM&) = delete;

    // 初始化（幂等）
    void init();

    // ===================== composition 接线 =====================
    // 挂接 PcieEndpointIP（RC 作为 Host 侧发起器访问 EP）
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

    // ===================== PCIe 枚举 =====================
    // 执行枚举：发现 EP 设备/功能。返回 true 表示成功。
    bool enumerate();
    // 已发现的设备列表（enumerate 后填充）
    const std::vector<DiscoveredDevice>& discovered_devices() const noexcept { return devices_; }

    // ===================== 配置空间访问 =====================
    // 配置空间读：返回 EP 配置空间值（device/function 指定 PF/VF）。
    // stream_id: 0=PF, 1..16=VF0..VF15（覆盖 device/function 选择）
    uint32_t config_read(uint16_t device, uint16_t function, uint16_t offset,
                         uint16_t stream_id = 0);
    // 配置空间写：写入 EP 配置空间。返回 false 表示失败（未挂接/越界/只读）。
    bool config_write(uint16_t device, uint16_t function, uint16_t offset,
                      uint32_t value, uint16_t stream_id = 0);

    // ===================== BAR 管理 =====================
    // 分配 BAR：在 EP 配置空间写入 BAR 基址+属性（64-bit prefetchable 等）。
    // bar_offset: 配置空间内 BAR 偏移（0x10=BAR0, 0x14=BAR1, 0x18=BAR2...）
    // value: BAR 值（含地址+属性位，如 0x10000008 表示 64-bit prefetchable @ 0x10000000）
    bool bar_allocate(uint16_t device, uint16_t function, uint16_t bar_offset,
                      uint32_t value);

    // ===================== BAR 空间访问（经 AXI master 路由到 EP）=====================
    // BAR 写：经 AXI master 通道写入 EP BAR 空间（bytes=1/2/4/8）。
    // 返回 false 表示未挂接 EP 或通道忙。
    bool bar_write(uint16_t device, uint64_t addr, uint64_t data, uint8_t bytes);
    // BAR 读：经 AXI master 通道从 EP BAR 空间读取，data 输出读回值。
    // 返回 false 表示未挂接 EP 或通道忙。
    bool bar_read(uint16_t device, uint64_t addr, uint64_t& data, uint8_t bytes);

    // ===================== AXI master 通道直接访问（高级用法）=====================
    // AXI master 请求/响应通道（与 HostBypassTLM 相同接口）
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

    // 每周期推进（转发至底层 Axi4StreamAdapter）
    void tick();
    // 全量复位
    void reset() { axi_.reset(); }

private:
    // 由 awsize（2^awsize 字节/拍）计算 burst 总字节数
    static uint8_t bytes_to_awsize(uint8_t bytes);

    std::string name_;
    EventQueue* eq_;
    PcieEndpointIP* ep_ = nullptr;
    cpptlm::Axi4StreamAdapter axi_;
    std::vector<DiscoveredDevice> devices_;
    // BAR 事务 ID 计数器
    uint16_t bar_tx_id_ = 0;
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_ROOT_COMPLEX_TLM_HH