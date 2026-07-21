// include/tlm/gpu/kernel_launch_tlm.hh
// KernelLaunchTLM: AQL 简化 dispatcher + D1-Full P1 PTX-EMU 驱动层（IPtxEmuDriver 窄接口）
// 功能:
//   Phase 8.A: 按 interval 周期向 ComputeCluster 发 KernelDesc (简化版 AQL packet)
//   P0 F12b-LD: 接 MemoryBridge, tick() 调 synchronize_stream(0) + 循环 exe_once
//   Phase 4 Wave 1: 升级为 IPtxEmuDriver 窄接口, advance() 推进 PTX-EMU 执行
// 作者 CppTLM Team / 日期 2026-06-24 (Phase 8.A) + 2026-07-16 (P0 扩展) + 2026-07-18 (Phase 4 P1)
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.4
//       openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/design.md §8
#ifndef TLM_GPU_KERNEL_LAUNCH_TLM_HH
#define TLM_GPU_KERNEL_LAUNCH_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <deque>

namespace tlm {

class MemoryBridge;   // forward declare (定义在 memory_bridge.hh)
class IPtxEmuDriver;  // forward declare (定义在 ptx_emu_driver.hh, 窄接口)

/**
 * @brief Kernel launch 请求数据结构 (MemoryBridge → KernelLaunchTLM FIFO)
 *
 * 字段与 CppTLMBridge::submit_kernel 参数对齐, kernel_name 为非所有权 char*
 * (PTX-EMU func2name 表长期存储, 无需拷贝)。func_ptr 当前 P0 未使用,
 * Phase 9+ PTX-EMU 真实集成时传入 kernel 函数指针。
 */
struct KernelLaunchRequest {
    uint64_t kernel_id = 0;
    uint64_t stream_id = 0;
    const char* kernel_name = nullptr;
    uint32_t grid_x = 0, grid_y = 0, grid_z = 0;
    uint32_t block_x = 0, block_y = 0, block_z = 0;
    size_t shared_mem = 0;
    void* func_ptr = nullptr;
};

/**
 * @brief AQL 简化 dispatcher (按 ADR-SOC-04 黑盒决策) + D1-Full P1 PTX-EMU 驱动
 *
 * Phase 8.A: tick() 按 interval 周期 launch kernel (简化 AQL model)
 * Phase 4 Wave 1: IPtxEmuDriver 窄接口模式:
 *   1. bridge_->synchronize_stream(0) 清空默认 stream 已完成 kernel
 *   2. driver_->advance(max, actual) 推进 PTX-EMU GPUContext::exe_once()
 *   3. AdvanceResult switch: Error 记录日志 + 终止, Executed/KernelComplete 检查 pending
 * driver_ == nullptr 时退化回 Phase 8.A 行为 (零回归)
 */
class KernelLaunchTLM : public ChStreamModuleBase {
public:
    KernelLaunchTLM(const std::string& name, EventQueue* eq);
    ~KernelLaunchTLM() override = default;

    std::string get_module_type() const override { return "KernelLaunchTLM"; }

    // ChStreamModuleBase required override
    void set_stream_adapter(cpptlm::StreamAdapterBase* adapter) override {
        adapter_ = adapter;
    }

    // === 程序化 setter (JSON 解析在 Phase 7.B+ 完整实现) ===
    void set_kernel_id(uint32_t id) { kernel_id_ = id; }
    void set_workgroup_size(uint32_t sz) { workgroup_size_ = sz; }
    void set_grid_size(uint32_t sz) { grid_size_ = sz; }
    void set_kernel_launch_interval(uint32_t cyc) { interval_ = cyc; }

    uint32_t get_kernel_id() const { return kernel_id_; }
    uint32_t get_workgroup_size() const { return workgroup_size_; }
    uint32_t get_grid_size() const { return grid_size_; }
    uint32_t get_kernel_launch_interval() const { return interval_; }

    /// 统计: 已 launch kernel 数 (测试用)
    uint64_t kernels_launched() const { return kernels_launched_; }

    // === D1-Full P0 扩展 APIs ===

    /// 注入 MemoryBridge (非所有权)。tick() 中检查 bridge_ != nullptr 决定走 P0/P1 还是 Phase 8.A 路径
    void setMemoryBridge(MemoryBridge* bridge) { bridge_ = bridge; }

    /// 注入 PTX-EMU 驱动接口 (Phase 4 Wave 1: 替代 P0 的 void* ptx_emu_context_)
    /// 非所有权, 由调用方管理生命周期
    void set_ptx_emu_driver(IPtxEmuDriver* driver) { driver_ = driver; }

    /// 获取 PTX-EMU 驱动接口 (S2 Phase 1.1: poll_kernel 查询 kernel 完成状态)
    IPtxEmuDriver* get_ptx_emu_driver() const { return driver_; }

    /// MemoryBridge::submit_kernel 调用: push KernelLaunchRequest 到 FIFO pending_
    void submit(KernelLaunchRequest&& req) {
        pending_.push_back(std::move(req));
    }

    /// 统计: FIFO 中 pending kernel 数 (P0 测试用)
    size_t pending_count() const { return pending_.size(); }

    void tick() override;

private:
    // Phase 8.A 状态 (bridge_ == nullptr 路径)
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    uint32_t kernel_id_ = 0;
    uint32_t workgroup_size_ = 64;
    uint32_t grid_size_ = 1;
    uint32_t interval_ = 1000;
    uint64_t cycle_counter_ = 0;
    uint64_t kernels_launched_ = 0;

    // D1-Full P1 状态 (driver_ != nullptr 路径)
    MemoryBridge* bridge_ = nullptr;             // 非所有权
    IPtxEmuDriver* driver_ = nullptr;            // 非所有权 (P1: 替代 void* ptx_emu_context_)
    std::deque<KernelLaunchRequest> pending_;    // FIFO kernel 队列

    /// Phase 4 Wave 1: 每 tick 调用 advance() 的最大步数, 1:1 映射满足 G-D3 ≤1 cycle
    static constexpr uint32_t MAX_PTX_STEPS_PER_TICK = 1;
};

}  // namespace tlm

#endif  // TLM_GPU_KERNEL_LAUNCH_TLM_HH