// include/tlm/gpu/kernel_launch_tlm.hh
// KernelLaunchTLM: AQL 简化 dispatcher (按 ADR-SOC-04 黑盒决策)
//
// 功能:
//   Phase 8.A: 按 interval 周期向 ComputeCluster 发 KernelDesc (简化版 AQL packet)
//   tick() 仅递增 cycle_counter_ + 按 interval 触发 kernels_launched_++
//
// PTX-EMU 集成路径 (HSK-8 后):
//   - 旧的 P0/P1 路径 (MemoryBridge + IPtxEmuDriver 窄接口) 已物理删除
//     (commit 369cf71, HSK-6 deprecate)
//   - 当前 PTX-EMU 集成走独立 facade: PtxEmuSubmoduleMVP + IPtxEmuDevice 公共 API
//     (per include/tlm/gpu/ptx_emu_submodule_mvp.hh, HSK-8 Phase 2 Step 4)
//   - 未来 Wave 2 接入: KernelLaunchTLM::tick() 应驱动 facade->exe_once()
//     (per docs/soc_arch/architecture/11-cdna-real-isa-integration.md 阶段 B)
//
// 作者 CppTLM Team / 日期 2026-06-24 (Phase 8.A) + 2026-07-16 (P0 扩展) + 2026-07-18 (Phase 4 P1)
// 参考: openspec/changes/2026-06-24-gpu-soc-phase8a-infra/design.md §3.4
//       docs/soc_arch/architecture/11-cdna-real-isa-integration.md §11.2.2
#ifndef TLM_GPU_KERNEL_LAUNCH_TLM_HH
#define TLM_GPU_KERNEL_LAUNCH_TLM_HH

#include "core/chstream_module.hh"
#include "framework/stream_adapter.hh"
#include <cstdint>
#include <deque>

namespace tlm {

// HSK-8 Phase 2 Step 4: MemoryBridge + IPtxEmuDriver 已物理删除 (HSK-6 deprecate by 369cf71).
// KernelLaunchTLM 的 P0/P1 路径已废弃, 仅保留 Phase 8.A 路径 (独立运行, 无桥接).

/**
 * @brief Kernel launch 请求数据结构 (KernelLaunchTLM FIFO 入口)
 *
 * 字段与原 CppTLMBridge::submit_kernel 参数对齐, kernel_name 为非所有权 char*
 * (PTX-EMU func2name 表长期存储, 无需拷贝)。func_ptr 当前未使用,
 * 未来 Phase 9+ PTX-EMU 真实集成时 (per ADR-SOC-15) 传入 kernel 函数指针。
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
 * @brief AQL 简化 dispatcher (按 ADR-SOC-04 黑盒决策)
 *
 * Phase 8.A: tick() 按 interval 周期 launch kernel (简化 AQL model)
 *
 * 历史 P0/P1 路径 (commit 369cf71 前): bridge_->synchronize_stream() + driver_->advance()
 * 已废弃。`driver_ == nullptr` 退化逻辑不再需要,本类当前不持有任何 PTX-EMU 驱动指针。
 *
 * 未来 Wave 2 接入 (per ADR-SOC-15 B.4): tick() 增加 `facade_->exe_once()` 调用。
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

    // === HSK-8 Phase 2 Step 4: setMemoryBridge() + set_ptx_emu_driver() 已废弃删除
    //     (MemoryBridge + IPtxEmuDriver 类已物理删除, HSK-6 deprecate by 369cf71)

    /// KernelLaunchTLM::submit 接口: push KernelLaunchRequest 到 FIFO pending_
    void submit(KernelLaunchRequest&& req) {
        pending_.push_back(std::move(req));
    }

    /// 统计: FIFO 中 pending kernel 数
    size_t pending_count() const { return pending_.size(); }

    void tick() override;

private:
    // Phase 8.A 状态 (仅保留此路径, P0/P1 已废弃)
    cpptlm::StreamAdapterBase* adapter_ = nullptr;
    uint32_t kernel_id_ = 0;
    uint32_t workgroup_size_ = 64;
    uint32_t grid_size_ = 1;
    uint32_t interval_ = 1000;
    uint64_t cycle_counter_ = 0;
    uint64_t kernels_launched_ = 0;

    std::deque<KernelLaunchRequest> pending_;    // FIFO kernel 队列
    bool tlm_objects_injected_ = false;          // Phase 2a: PipelineTLM/ScoreboardTLM/TensorCoreTLM 注入标志

    /// Phase 4 Wave 1: 每 tick 调用 advance() 的最大步数, 1:1 映射满足 G-D3 ≤1 cycle
    static constexpr uint32_t MAX_PTX_STEPS_PER_TICK = 1;
};

}  // namespace tlm

#endif  // TLM_GPU_KERNEL_LAUNCH_TLM_HH