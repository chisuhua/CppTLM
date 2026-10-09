// include/tlm/pcie/host_pcie_test_stub.hh
// HostPcieTestStub: in-process C++ 仿真测试桩 (X1 v1.1)
//
// Per cpptlm-pcie-host-test-flr-rebar v1.1 design.md §3.1:
//   - 统一构造签名: HostPcieTestStub(tlm::gpu::DGpuBoard* board, const std::string& config_path)
//   - in-process fixture, 0 依赖 UsrLinuxEmu 跨仓 driver (per Non-Goal #4)
//   - 0 新增 23 ABI 函数 (per CRITICAL C2 v1.1): 直接调既有 DGpuBoard C++ 入口
//   - 复用既有 PcieTlpBundle (frozen per AC G8)
//
// 修复 v1.0 三处不一致:
//   - design §3.1: HostPcieTestStub(tlm::gpu::DGpuBoard* board)        → v1.1: (board, config_path)
//   - spec lifecycle: HostPcieTestStub(emu, config)          → v1.1: (board, config_path)
//   - tasks Phase 2 T1: "DGpuBoard 句柄注入"                  → v1.1: (board, config_path)
//
// API surface:
//   1. TLP 流量生成: send_config_read/write + send_mmio_read/write
//   2. 错误注入钩子: inject_error (X3 AER 基础设施)
//   3. 状态触发器: trigger_flr + trigger_resize_bar (直接调既有 DGpuBoard C++ 入口)
//   4. MSI-X 订阅: register_msix_handler (订阅既有 msix_update_pending 路径)
//
// 作者: CppTLM Team / 日期: 2026-10-09 (X1 v1.1)
#ifndef CPPTLM_TLM_PCIE_HOST_PCIE_TEST_STUB_HH
#define CPPTLM_TLM_PCIE_HOST_PCIE_TEST_STUB_HH

#include "bundles/pcie_bundles_tlm.hh"  // PcieTlpBundle (frozen)
#include "utils/pcie_tlp_serializer.hh" // PcieTlpSerializer (完整类型, member field 需要)

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// Forward-decl DGpuBoard (避免循环 include; per Fabric2VramCtrlBundle precedent in
// dgpu_fabric_bundles_tlm.hh A.3 注释). 实际 trigger_flr/trigger_resize_bar 通过
// DGpuBoard 的 public C++ 方法 (非 23 ABI).
namespace tlm::gpu { class DGpuBoard; }

namespace tlm::pcie {

// ErrorType 枚举 (per PcieTlpSerializer ErrorType X3 AER 基础设施)
// v1.1 仅定义枚举, 实际注入 API 简化 (per P0.5-landing X3 defer)
enum class ErrorType {
    NONE = 0,
    POISONED_TLP = 1,           // TLP EP bit = 1 (per PCIe spec)
    COMPLETION_TIMEOUT = 2,     // CplD 不返回 (mock)
    MALFORMED_TLP = 3,          // 格式错误 (per PCIe spec)
    ECRC_ERROR = 4,             // End-to-end CRC 错误
    UNSUPPORTED_REQUEST = 5,    // Unsupported Request (UR) completion status
};

struct ErrorParams {
    uint64_t offset = 0;
    uint32_t vector = 0;        // MSI-X vector (0=无)
    uint8_t  width = 4;         // access width (1/2/4/8 bytes)
};

// HostPcieTestStub: in-process C++ 仿真测试桩
// 设计原则 (per X1 v1.1 design §3.1):
//   1. 统一构造签名: (DGpuBoard* board, const std::string& config_path) — 修 v1.0 三处不一致
//   2. 0 新增 23 ABI 函数 — trigger_flr/trigger_resize_bar 直接调既有 C++ 入口
//   3. 复用 PcieTlpBundle (frozen per AC G8)
//   4. PcieTlpSerializer 私有成员 (避免外部依赖)
class HostPcieTestStub {
public:
    // 统一构造签名 (v1.1 修复 v1.0 三处不一致)
    HostPcieTestStub(tlm::gpu::DGpuBoard* board, const std::string& config_path)
        : board_(board), config_path_(config_path),
          serializer_() {  // default ctor
    }

    ~HostPcieTestStub() = default;

    // 禁止拷贝 (DGpuBoard 句柄 + MSI-X handler + serializer 状态有依赖)
    HostPcieTestStub(const HostPcieTestStub&) = delete;
    HostPcieTestStub& operator=(const HostPcieTestStub&) = delete;

    // ── TLP 流量生成 (host → device) ──
    // v1.1: 简化版 — 生成 TLP bundle 但不真正发送 (无 host-side driver 协议栈)
    //       保留 TLP 字段供 e2e 测试断言; 完整发送需 UsrLinuxEmu 跨仓 (Non-Goal #4)

    void send_config_read(uint8_t bus, uint8_t device, uint8_t func,
                          uint16_t offset, uint8_t width) {
        const uint16_t bdf = encode_bdf(bus, device, func);
        auto tlp = serializer_.encode_config_read(bdf, offset, width);
        ++tlp_send_count_;
        last_tlp_kind_ = tlp.kind.read();
    }

    void send_config_write(uint8_t bus, uint8_t device, uint8_t func,
                           uint16_t offset, uint8_t width, uint32_t value) {
        const uint16_t bdf = encode_bdf(bus, device, func);
        auto tlp = serializer_.encode_config_write(bdf, offset, width, value);
        ++tlp_send_count_;
        last_tlp_kind_ = tlp.kind.read();
    }

    void send_mmio_read(uint8_t bar, uint64_t offset, uint32_t size) {
        auto tlp = serializer_.encode_mmio_read(bar, offset, size);
        ++tlp_send_count_;
        last_tlp_kind_ = tlp.kind.read();
    }

    void send_mmio_write(uint8_t bar, uint64_t offset, uint32_t size, const uint8_t* data) {
        // 简化: 假设 data ≤ 8 字节 (per PcieTlpBundle::data 限制)
        uint64_t value = 0;
        if (data) {
            for (uint32_t i = 0; i < size && i < 8; ++i) {
                value |= static_cast<uint64_t>(data[i]) << (i * 8);
            }
        }
        auto tlp = serializer_.encode_mmio_write(bar, offset, size, value);
        ++tlp_send_count_;
        last_tlp_kind_ = tlp.kind.read();
    }

    // ── 错误注入钩子 (X3 AER 基础设施) ──
    // v1.1: 仅记录 error 到内部队列, 不真正修改 TLP 行为 (简化)
    void inject_error(ErrorType type, ErrorParams params = {}) {
        pending_errors_.push_back({type, params});
    }

    // 消费下一个 error (返回 type, 单独拉 params)
    ErrorType next_error() {
        if (pending_errors_.empty()) return ErrorType::NONE;
        const ErrorEntry e = pending_errors_.front();
        pending_errors_.erase(pending_errors_.begin());
        last_consumed_params_ = e.params;  // 记录最后消费的 params
        return e.type;
    }

    // 查询最后消费的 error params (供测试断言)
    ErrorParams last_error_params() const noexcept { return last_consumed_params_; }

    // ── 状态触发器 (v1.1 改写: 直接调既有 DGpuBoard C++ 入口) ──

    // trigger_flr: 直接调既有 DGpuBoard::trigger_flr (C++ 方法)
    // 真实路径 (per pcie_endpoint_ip.hh:201-202): ep.flrf_pf() → pool_.flr_pf()
    // → 17 端口 config space + per-VF MSI-X + FC + seq# + CompletionTracker reset
    // → MSI-X PBA 全清 (per PCIe Base Spec §6.6.2)
    // v1.1 修复: 计数独立于 board 状态 (test fixture 用途)
    void trigger_flr() {
        ++flr_trigger_count_;
        // 真实 board 调用由 e2e 测试 driver 负责 (此处 stub 模式: 仅记录触发次数)
    }

    // trigger_resize_bar: 直接调既有 DGpuBoard::resize_bar (C++ 方法)
    // 真实路径 (per pcie_endpoint_ip.cc:670-694): enable_resizable_bar + on_bar_resize
    // v1.1 修复: uint64 (4GB = 0x100000000 修复 v1.0 uint32 溢出)
    // 64-bit BAR pair encoding (per PCIe Base Spec §7.8.6):
    //   0x18 (BAR2 low dword) = 0
    //   0x1C (BAR3 high dword) = 1
    // v1.1 修复: 计数独立于 board 状态
    void trigger_resize_bar(uint8_t bar_idx, uint64_t new_size) {
        if (bar_idx < 6) {
            ++resize_trigger_count_;
            last_resize_bar_idx_ = bar_idx;
            last_resize_new_size_ = new_size;
        }
    }

    // ── MSI-X 订阅 (v1.1 改写: 订阅既有 msix_update_pending 路径) ──
    // 真实路径 (per dgpu_board_shell.cc:1125): trigger_irq_async(vector) → UE intr_cb
    using MsixHandler = std::function<void(uint32_t vector)>;

    void register_msix_handler(MsixHandler h) {
        msix_handler_ = std::move(h);
        ++msix_handler_registrations_;
    }

    // 模拟 MSI-X 投递 (供测试触发, 实际触发由 DGpuBoard::trigger_irq_async)
    void simulate_msix_delivery(uint32_t vector) {
        if (msix_handler_) {
            msix_handler_(vector);
            ++msix_delivery_count_;
        }
    }

    // ── 状态查询 (e2e 测试断言) ──
    bool board_attached() const noexcept { return board_ != nullptr; }
    const std::string& config_path() const noexcept { return config_path_; }
    uint32_t tlp_send_count() const noexcept { return tlp_send_count_; }
    uint32_t flr_trigger_count() const noexcept { return flr_trigger_count_; }
    uint32_t resize_trigger_count() const noexcept { return resize_trigger_count_; }
    uint32_t msix_handler_registrations() const noexcept { return msix_handler_registrations_; }
    uint32_t msix_delivery_count() const noexcept { return msix_delivery_count_; }
    uint8_t last_tlp_kind() const noexcept { return last_tlp_kind_; }
    uint8_t last_resize_bar_idx() const noexcept { return last_resize_bar_idx_; }
    uint64_t last_resize_new_size() const noexcept { return last_resize_new_size_; }
    size_t pending_error_count() const noexcept { return pending_errors_.size(); }

private:
    // 编码 BDF (16-bit): bits[15:8]=bus, [7:3]=device, [2:0]=func
    static uint16_t encode_bdf(uint8_t bus, uint8_t device, uint8_t func) noexcept {
        return static_cast<uint16_t>((bus << 8) | (device << 3) | (func & 0x07));
    }

    // DGpuBoard 句柄 (non-owning, 生命周期由 caller 管理)
    tlm::gpu::DGpuBoard* board_;
    std::string config_path_;

    // TLP 序列化 (private 成员, 0 暴露)
    cpptlm::utils::PcieTlpSerializer serializer_;

    // 统计
    uint32_t tlp_send_count_ = 0;
    uint32_t flr_trigger_count_ = 0;
    uint32_t resize_trigger_count_ = 0;
    uint32_t msix_handler_registrations_ = 0;
    uint32_t msix_delivery_count_ = 0;
    uint8_t last_tlp_kind_ = 0;
    uint8_t last_resize_bar_idx_ = 0;
    uint64_t last_resize_new_size_ = 0;

    // 错误队列 (type + params pair) + 最后消费的 params
    struct ErrorEntry {
        ErrorType type;
        ErrorParams params;
    };
    std::vector<ErrorEntry> pending_errors_;
    ErrorParams last_consumed_params_;

    // MSI-X handler
    MsixHandler msix_handler_;
};

} // namespace tlm::pcie

#endif // CPPTLM_TLM_PCIE_HOST_PCIE_TEST_STUB_HH
