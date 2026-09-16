// DGpuBoard - C++ shell(非数据面组件),承担 23 ABI 接口 + 5 职责 + 线程模型
// Per board-soc-split design §2 + §2.5 thread model + ADR-SOC-07 D1/D7
// Owner: CppTLM Team · Date: 2026-08-31
#ifndef CPPTLM_DGPU_BOARD_SHELL_H
#define CPPTLM_DGPU_BOARD_SHELL_H

#include "event_queue.hh"
#include "tlm/gpu/dgpu_soc.hh"  // DGpuSoc SimModule 容器
#include "tlm/gpu/pcie_endpoint_tlm.h"  // PcieEndpointTLM (legacy, 仅 frozen ABI 兼容)
#include "tlm/pcie/pcie_endpoint_ip.hh"  // PcieEndpointIP (A-2 Path A: pcie_ep accessor 返回类型)
#include "tlm/gpu/pcie_bar_router_mvp.hh"  // PcieBarRouter::RegisterEntry (lookup_register_entry)
#include "tlm/gpu/sdma_engine_tlm.hh"  // SdmaEngineTLM (P0 unblock Task 5+6: BAR1 doorbell wiring)
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <vector>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <nlohmann/json.hpp>

namespace tlm::gpu {

using cpptlm::tlm::DGpuSoc;  // 引入 DGpuSoc 类型

// PendingReq: host → sim 注入单元
struct PendingReq {
    uint8_t bar;
    uint64_t offset;
    std::vector<uint8_t> data;  // mmio_write payload
    uint64_t trans_id;          // 用于 future 关联
    std::promise<int32_t> resp; // mmio_read 用,mmio_write 无值
    bool is_backdoor = false;   // backdoor 标识(默认 false,mmio 路径不设)
    bool is_backdoor_read = false; // backdoor read/write 区分(SOC deferred 时 shell 本地处理)
    bool is_mmio_read = false;  // mmio read/write 区分(修复 #5: drain 时读路径需回填数据)
};

// DGpuBoard - 23 ABI 翻译 shell
// 设计原则(per ADR-SOC-07 D7):不继承 ChStreamModuleBase/SimModule;
// SOC deferred 期间 shell 本地持有 mmio_regs_/vram_segments_ 等回退存储(修复 #5/#6 确定性 roundtrip)
//
// T-P12-1: profile JSON "pcie_path" 4 态选路
//   "pcie_path": "tlp"       → TLP 链路完整路径 (Encoder → rx_tlp_from_host →
//                               CompleterEngine → bar_store_)
//   "pcie_path": "axi_bypass" → HostBypassTLM::bar_write (跳过 TLP 直接 AXI)
//   "pcie_path": "mock"       → PcieMockIP 直调 bar_regs_ (T-P9-3)
//   "pcie_path": "legacy"     → mmio_regs_ 既有行为 (向后兼容)
class DGpuBoard {
public:
    // mmio_read 同步等待窗口(修复 #5 flakiness: 1ms 硬超时 vs 加载主机 async drain 延迟;
    // 公开供调用方/测试覆写)
    static constexpr std::chrono::milliseconds kMmioWaitTimeout{50};
    // mmio_read 调用线程自 drain 回退: 默认开启, 保证成功路径确定性(不依赖 sim 线程被调度);
    // 测试超时路径时置 false, 仅依赖外部 drain 以稳定触发 -110 (公开供调用方/测试覆写)
    bool mmio_self_drain_enabled{true};

    // T-P12-1: 4 态 PCIe 路径选路
    enum class PciePath : uint8_t {
        Legacy = 0,     // "legacy" 或缺失 → mmio_regs_ 既有行为
        AxiBypass = 1,  // "axi_bypass" → HostBypassTLM::bar_write
        Tlp = 2,        // "tlp" → TLP 完整链路
        Mock = 3        // "mock" → PcieMockIP
    };

    // T-P12-1: profile 注入 (解析 pcie_path 字段)
    void attach_profile(const nlohmann::json& profile);

    // T-P12-1: 当前路径访问器 (测试用)
    PciePath pcie_path() const noexcept { return pcie_path_; }

    // T-P12-1: 测试 accessors
    // 验证 data ended up in endpoint bar_store_ (for axi_bypass/tlp paths)
    // 委托 pcie_ep()->bar_store_value()
    uint64_t endpoint_bar_store_value(uint8_t bar, uint16_t bdf, uint64_t offset) const;
    // 验证链路层无 TLP 发出 (axi_bypass 路径)
    size_t link_layer_tx_tlp_out_count() const;

    // 5 职责接口(per ADR-SOC-07 D1)
    explicit DGpuBoard(const std::string& name, EventQueue* eq = nullptr);
    ~DGpuBoard();

    // 1. SOC 装配
    bool load_soc_config(const nlohmann::json& board_cfg);
    bool init();
    void shutdown();

    // 2. ABI 翻译入口(被 23 ABI C 函数调用,定义在 abi-export change)
    //    本任务只声明接口,实现 deferred 到 T-bs-3b
    int mmio_read(uint8_t bar, uint64_t offset, void* buf, size_t len);
    int mmio_write(uint8_t bar, uint64_t offset, const void* buf, size_t len);
    int pcie_config_read(uint16_t offset, uint8_t width, uint32_t* val);
    int pcie_config_write(uint16_t offset, uint8_t width, uint32_t val);

    // 2.1 backdoor ABI(per design §2.5 #5 + ADR-SOC-07 Q3)
    //     走 inject_q 路径,不直接访问 VRAM
    int backdoor_read(uint64_t vram_offset, void* buf, size_t len);
    int backdoor_write(uint64_t vram_offset, const void* buf, size_t len);

    // 2.2 msix + lookup_register wrappers (per T-W3-3)
    //     soc_ null 或 ep 缺失时返 -ENOSYS; table_size > 2048 返 -EINVAL
    int msix_init(uint32_t table_size, uint32_t mask);
    int msix_update_pending(uint32_t vector);
    int msix_clear_pending(uint32_t vector);
    int lookup_register(uint32_t offset, uint32_t* value);

    // lookup_register_entry: returns full RegisterEntry for ABI metadata
    // (name/access/side_effect). nullptr when SOC null / unaligned / > BAR0 / miss.
    const PcieBarRouter::RegisterEntry* lookup_register_entry(uint32_t offset);

    // pcie_ep accessor: 返回 SOC 内 PcieEndpointIP 实例 (nullptr 当 SOC 未实例化/ep 缺失)
    // 只读, 供测试/工具直接访问底层 MSI-X / config space (如 unmask auto-deliver 验证)
    // A-2 Path A: 从 PcieEndpointTLM* → PcieEndpointIP* (生产 profile 已切 IP)
    tlm::pcie::PcieEndpointIP* pcie_ep() const {
        if (!soc_)
            return nullptr;
        return dynamic_cast<tlm::pcie::PcieEndpointIP*>(soc_->getInternalInstance("pcie_ep"));
    }

    // A-3: MMIO power-state gate — D3hot 时返 true (per INV-A MMIO gating)
    // 委托 pcie_ep->mmio_gated(); 非 IP 类型返 false
    [[nodiscard]] bool is_mmio_gated() const;

    // 3. 回调接线(non-blocking,per design §2.5 #4)
    using IrqCallback = std::function<void(uint32_t vector_id)>;
    using DmaTranslateCallback = std::function<uint64_t(uint64_t iova, size_t size)>;
    using ErrorCallback = std::function<void(int err_code, const std::string& msg)>;
    void set_irq_callback(IrqCallback cb) { irq_cb_ = std::move(cb); }
    void set_dma_translate_callback(DmaTranslateCallback cb) { dma_translate_cb_ = std::move(cb); }
    void set_error_callback(ErrorCallback cb) { error_cb_ = std::move(cb); }

    // Stage 1.3a integration (per docs/superpowers/specs/2026-09-13-ue-sdma-p0-unblock-design.md §3.2 目标 1):
    //   BAR1+0x10010000 doorbell 路由到 SOC SDMA ring 计数 (测试断言)
    uint32_t pcie_ep_doorbell_count() const noexcept {
        return pcie_ep_doorbell_count_;
    }

    // BAR1+0x10010000 doorbell offset (per 1.3a spec)
    static constexpr uint64_t kBar1DoorbellOffset = 0x10010000ULL;

    // Stage 1.3a integration (P0 unblock Task 5+6):
    //   Inject SdmaEngineTLM reference, so DGpuBoard::mmio_write BAR1 doorbell
    //   can dispatch to SdmaEngineTLM::mmio_write (1.3a 已 ship, BAR1 doorbell
    //   → ring consume 完整逻辑)
    void set_sdma_engine(::tlm::gpu::SdmaEngineTLM* sdma) noexcept {
        sdma_engine_ = sdma;
    }

    // Stage 1.3d: SDMA Fence → MSI-X vector 0 接线 (per openspec/.../2026-09-10-...)
    //   SdmaEngineTLM::process_fence_queue 调用此 API → DGpuBoard::msix_update_pending(0)
    //   → trigger_irq_async(0) → UE intr_cb(vector=0, payload=fence_id)
    void sdma_fence_complete(uint64_t fence_id);

    // 4. 设备枚举
    uint32_t device_id() const { return device_id_; }
    struct DeviceInfo {
        uint32_t vendor_id;
        uint32_t device_id;
        uint64_t bar_sizes[6];
        uint64_t visible_vram_size;
        uint64_t invisible_vram_size;
        uint64_t va_region_size;
        uint32_t gpu_id;
        uint16_t gfx_version;
        uint16_t bdf;
    };
    const DeviceInfo& device_info() const { return device_info_; }

    // 5. 生命周期
    void tick();  // 转发到 soc_->tick()(SimModule 递归)
    
    // StatsManager 多卡前缀(per design §2.5 #6)
    std::string get_stats_path(const std::string& module_name) const {
        // 格式: "<device_id>.<module_name>" 防止多卡 singleton 冲突
        return std::to_string(device_id_) + "." + module_name;
    }
    
    // 内部触发接口(供 SOC 组件调用,deferred T-bs-4 装配)
    void trigger_irq_async(uint32_t vector_id);
    void trigger_dma_translate_async(uint64_t iova, size_t size);
    void trigger_error_async(int err_code, const std::string& msg);

    // ── C3 (基础任务 1.2.3): MSI-X 中断合并 (interrupt coalescing) ──
    // 位于 msix_update_pending 与 trigger_irq_async 之间的唯一 choke point:
    // N 次投递 (threshold) 或首个投递后 timeout 窗口 → 合并为 1 次 intr_cb。
    // 默认: enabled=true, threshold=8, timeout=50us (per §2.5.2)。
    // 公开旋钮 (供测试 disable + 未来 tuning)。
    std::atomic<bool> msix_coalesce_enabled_{true};
    uint32_t msix_coalesce_threshold_ = 8;
    std::chrono::microseconds msix_coalesce_timeout_{50};

    // 线程安全访问器 (set 方法唤醒/重算 timer)
    void set_msix_coalesce_enabled(bool en) { msix_coalesce_enabled_.store(en); }
    bool msix_coalesce_enabled() const { return msix_coalesce_enabled_.load(); }
    void set_msix_coalesce_threshold(uint32_t threshold);
    uint32_t msix_coalesce_threshold() const { return msix_coalesce_threshold_; }
    void set_msix_coalesce_timeout(std::chrono::microseconds timeout);
    std::chrono::microseconds msix_coalesce_timeout() const { return msix_coalesce_timeout_; }

private:
    // ── 线程模型字段(per design §2.5) ──
    std::string name_;
    uint32_t device_id_ = 0;
    DeviceInfo device_info_{};
    std::unique_ptr<EventQueue> eq_;             // #1 每卡独立(non-thread-safe)
    std::unique_ptr<cpptlm::tlm::DGpuSoc> soc_;               // SimModule 容器
    std::thread sim_thread_;                     // 每卡独立仿真线程
    std::atomic<bool> stop_{false};              // #10 destroy 顺序第一段
    std::mutex inject_mu_;                       // #2 host→sim 注入互斥
    std::deque<PendingReq> inject_q_;            // #2 注入队列
    std::unordered_map<uint64_t, std::future<int32_t>> pending_resp_; // #3 future 关联
    uint64_t next_trans_id_ = 0;
    std::exception_ptr last_exception_;          // #8 跨线程异常传递
    static constexpr uint64_t kDefaultQuantumCycles = 1000;
    uint64_t quantum_cycles_ = kDefaultQuantumCycles;

    // T-P12-1: 4 态 PCIe 路径选路
    PciePath pcie_path_ = PciePath::Legacy;
    // T-P12-1: mmio_write 按路径分发
    void dispatch_mmio_to_pcie(uint8_t bar, uint64_t offset, const void* data, std::size_t len);

    // ── 回调(per #4 non-blocking) ──
    IrqCallback irq_cb_;
    DmaTranslateCallback dma_translate_cb_;
    ErrorCallback error_cb_;
    std::mutex callback_mu_;  // 保护 callback 指针(避免 host-sim race)

    // Stage 1.3a integration: BAR1+0x10010000 doorbell 路由计数器
    // (per docs/superpowers/specs/2026-09-13-ue-sdma-p0-unblock-design.md §3.2 目标 1)
    uint32_t pcie_ep_doorbell_count_ = 0;

    // Stage 1.3a integration (P0 unblock Task 5+6):
    //   SdmaEngineTLM 引用 (set_sdma_engine 注入), 转发 BAR1 doorbell 到 SDMA ring
    ::tlm::gpu::SdmaEngineTLM* sdma_engine_ = nullptr;

    // ── 内部方法 ──
    void sim_loop();                              // sim 线程主循环
    void drain_injection_queue();                 // #2/#5 inject_q 服务
    void destroy();                               // #10 严格顺序

    // backdoor VRAM 存储(SOC deferred 时 shell 本地处理 backdoor_read/write)
    std::map<uint64_t, std::vector<uint8_t>> vram_segments_;
    std::unordered_map<uint64_t, std::vector<uint8_t>> last_backdoor_reads_;
    // mmio 读响应 payload(SOC deferred 时 drain_injection_queue 回填, mmio_read 消费后清除)(修复 #5)
    std::unordered_map<uint64_t, std::vector<uint8_t>> pending_data_;
    // mmio 寄存器映射: (bar, offset) → 写入字节(SOC deferred 时 shell 本地, 确定性 roundtrip)(修复 #5)
    std::map<std::pair<uint8_t, uint64_t>, std::vector<uint8_t>> mmio_regs_;

    // ── C3 (基础任务 1.2.3): MSI-X 中断合并内部状态 ──
    // 全部由 coalesce_mu_ 保护 (除 coalesce_stop_ atomic); timer 线程 joinable,
    // lazy-start (首次 arm 时), shutdown/destroy 时 stop+notify+join (确定性析构)。
    std::mutex coalesce_mu_;                                   // 保护合并计数器/armed/deadline
    std::condition_variable coalesce_cv_;                      // timer 等待/唤醒
    std::atomic<bool> coalesce_stop_{false};                   // timer 线程停止标志
    std::thread coalesce_timer_;                               // 合并 timer 线程 (joinable)
    std::chrono::steady_clock::time_point coalesce_deadline_;  // 当前合并窗口截止 (armed 时有效)
    uint32_t coalesce_count_ = 0;                              // 窗口内已累计投递数
    uint32_t coalesce_rep_vector_ = 0;                         // 窗口内首个投递的 vector (代表)
    bool coalesce_armed_ = false;                              // timer 已武装 (有未决合并窗口)

    // C3 内部方法
    void coalesce_timer_loop();                   // timer 线程主循环 (wait_until deadline → drain)
    void coalesce_arm_or_drain(uint32_t vector);  // 合并投递: 累计/arm/阈值 drain 决策
    void coalesce_force_flush();                  // msix_init resize 时强制排空 armed 状态
};

} // namespace tlm::gpu

#endif // CPPTLM_DGPU_BOARD_SHELL_H