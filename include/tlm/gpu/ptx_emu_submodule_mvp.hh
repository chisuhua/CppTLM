// include/tlm/gpu/ptx_emu_submodule_mvp.hh
// PtxEmuSubmoduleMVP: PTX functional facade (HSK-8 Phase 2 重构)
//
// 功能: CppTLM 唯一 include PTX-EMU 公共头 (`ptxemu/device_api.h` +
//       `ptxemu/ir/statement.h`) 的入口 — 编译防火墙 (Phase 2 收窄)。
//       暴露 IPtxEmuDevice 实际 13 个抽象方法的 1:1 转发 + WarpStatus
//       查询的 helper 派生方法 (mask / blocked_cycles / is_warp_finished)。
//
// 设计原则 (per openspec/changes/cpptlm-ptxemu-public-device-api/design.md):
//   - 编译防火墙 (cpp 不暴露): 唯一允许 include PTX-EMU 公共头的 .cc = facade.cc + adapter.cc
//   - PTX-EMU 内部实现头 (`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/`register/*.h`) 完全 PRIVATE
//   - 所有 facade 公共 API 对应 IPtxEmuDevice 实际签名, 无内部类型 (GPUContext* etc.) 泄漏
//   - register/memory 读写已被 PTX-EMU 端截断 (HSK-8 决策: 不暴露, 保持封装边界)
//
// HSK-8 spec 演进:
//   - S1 (c2038a93) facade 暴露 18 个方法, 部分依赖 PTX-EMU 内部实现
//   - HSK-8 (fcdad151) facade 收窄到 13 个 IPtxEmuDevice 方法 + 3 个 WarpStatus 派生 helper
//
// 作者 CppTLM Team / / 2026-08-24 (HSK-8 Phase 2, Sisyphus)
// 参考:
//   - openspec/changes/cpptlm-ptxemu-public-device-api/{proposal,design}.md
//   - HSK-8 spec: docs/superpowers/specs/2026-08-21-hsk-8-ptxemu-public-api.md
//   - PTX-EMU public device API: include/ptxemu/device_api.h (fcdad151)
#ifndef TLM_GPU_PTX_EMU_SUBMODULE_MVP_HH
#define TLM_GPU_PTX_EMU_SUBMODULE_MVP_HH

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// HSK-8 Phase 2: 仅 include PTX-EMU 公共头 (`ptxemu/device_api.h`).
// PTX-EMU 内部头 (`ptxsim/*.h`/`ptx_ir/*.h`/`memory/*.h`/`register/*.h`)
// 不可在此 include — 编译防火墙 (cpp 不暴露, per HSK-8 spec §3)。
// 注: WarpStatus/ThreadState/LaneStatus/DeviceConfig 等 DTO 来自 device_api.h,
//     Statement 来自 ptxemu/ir/statement.h (5 文件 ~1053 LOC + 2 .def X-Macro,
//     已 Phase 0 净化: 无 operand_phy_addr / 无 InstructionState state)。
#include "ptxemu/device_api.h"
#include "ptxemu/ir/statement.h"

namespace tlm {

/// 公开值类型 — 透传 PTX-EMU 命名空间 (per HSK-8 spec §CppTLM 端接受条件)
using DeviceConfig = ptxemu::DeviceConfig;
using WarpStatus = ptxemu::WarpStatus;
using LaneStatus = ptxemu::LaneStatus;
using ThreadState = ptxemu::ThreadState;
using StatementContext = ptxemu::ir::StatementContext;
constexpr uint32_t kWarpSize = 32;  // per PTX-EMU fcdad151 DeviceConfig.warp_size default

/// PtxEmuSubmoduleMVP — PTX functional facade (HSK-8 Phase 2)
///
/// 设计原则:
///   - 编译防火墙: 唯一允许 include PTX-EMU 公共头的 CppTLM .cc = 此 .cc + adapter.cc
///   - 所有公共方法对应 IPtxEmuDevice 实际抽象方法 (1:1 转发)
///   - 不暴露任何 PTX-EMU 内部类型 (GPUContext* / SMContext* / WarpContext*)
class PtxEmuSubmoduleMVP {
public:
    PtxEmuSubmoduleMVP();
    ~PtxEmuSubmoduleMVP();

    PtxEmuSubmoduleMVP(const PtxEmuSubmoduleMVP&) = delete;
    PtxEmuSubmoduleMVP& operator=(const PtxEmuSubmoduleMVP&) = delete;

    // =========================================================================
    // Lifecycle (IPtxEmuDevice::initialize / shutdown 转发)
    // =========================================================================
    bool init(const std::string& ptx_emu_root, const DeviceConfig& config);
    void shutdown();
    bool is_initialized() const { return initialized_; }

    // =========================================================================
    // Execution (IPtxEmuDevice::exe_once / sm_exe_once / warp_exe_once 转发)
    // =========================================================================
    /// 全局 exe_once (per HSK-8 spec: 驱动所有 SM)
    int exe_once();

    /// 单 SM exe_once (per SM 资源反压 — adapter 主要入口)
    int sm_exe_once(uint32_t sm_id);

    /// 单 warp exe_once (精细粒度, 一般不直接调用)
    int warp_exe_once(uint32_t sm_id, uint32_t warp_id);

    // =========================================================================
    // State Write (IPtxEmuDevice::set_* 转发)
    // =========================================================================
    bool set_scoreboard(uint32_t sm_id, uint32_t warp_id, uint64_t mask);
    bool set_active_mask(uint32_t sm_id, uint32_t warp_id, uint64_t mask);
    bool set_next_pc(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id, uint32_t pc);

    // =========================================================================
    // State Query (IPtxEmuDevice::get_thread_state / get_warp_status / is_finished 转发)
    // =========================================================================
    ThreadState get_thread_state(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id);
    WarpStatus get_warp_status(uint32_t sm_id, uint32_t warp_id);
    bool is_finished();

    // =========================================================================
    // HSK-4 注入 (IPtxEmuDevice::attach_timing 转发, 1 个方法 3→1 聚合)
    // =========================================================================
    /// 注入 HSK-4 vendored 三个接口 (IScoreboard + IPipelineLatencyProvider + ITensorCoreTiming)
    void attach_timing(void* scoreboard, void* pipeline, void* tensor_core);

    // =========================================================================
    // WarpStatus 派生 helpers (Phase 1 S1 方法的语义等价)
    // =========================================================================
    /// 32-bit active mask from WarpStatus.lanes[].state (★ FIX-H8/B.3 等价)
    /// 注: 实际实现可能仍需 IPtxEmuDevice::set_active_mask 配合;
    ///     当前 helper 只解析 lanes 状态推断 mask。
    uint32_t get_active_mask_from_warp_status(uint32_t sm_id, uint32_t warp_id);

    /// 单 lane blocked_cycles (从 WarpStatus.lanes[].state + LaneStatus 派生)
    int32_t get_blocked_cycles_from_warp_status(uint32_t sm_id, uint32_t warp_id, uint32_t lane_id);

    /// warp 是否所有 lane 都已退出 (从 WarpStatus.lanes[].state 推断)
    bool is_warp_finished_from_warp_status(uint32_t sm_id, uint32_t warp_id);

    // =========================================================================
    // Stub: PTX-EMU 端 fcdad151 未暴露的 S1 facade 方法 (待 PTX-EMU 扩展后实现)
    // =========================================================================
    // HSK-8 spec 演进注: register/memory 读写, PTXIR decode 自由函数,
    // functional_execute_warp (不增加 cycle 的 warp dispatch) 等 S1 方法
    // 在 PTX-EMU fcdad151 实际 API 中未暴露. CppTLM 端保留方法签名作为
    // forward declaration, 返回 error stub 值 (false / T{} / 0), 等待
    // PTX-EMU 端 HSK-9 扩展时补充.

    /// Stub: register read (PTX-EMU 端未暴露 register 读写 API)
    /// @return 总是 false, 配合 out 参数清零
    bool read_register_u32(uint32_t /*sm_id*/, uint32_t /*warp_id*/, uint32_t /*lane_id*/,
                            const char* /*reg_name*/, uint32_t& out_value);
    bool read_register_u64(uint32_t /*sm_id*/, uint32_t /*warp_id*/, uint32_t /*lane_id*/,
                            const char* /*reg_name*/, uint64_t& out_value);
    bool write_register_u32(uint32_t /*sm_id*/, uint32_t /*warp_id*/, uint32_t /*lane_id*/,
                             const char* /*reg_name*/, uint32_t /*value*/);
    bool write_register_u64(uint32_t /*sm_id*/, uint32_t /*warp_id*/, uint32_t /*lane_id*/,
                             const char* /*reg_name*/, uint64_t /*value*/);

    /// Stub: global memory read/write (PTX-EMU 端未暴露)
    bool read_global_u32(uint64_t /*addr*/, uint32_t& out_value);
    bool read_global_u64(uint64_t /*addr*/, uint64_t& out_value);
    bool write_global_u32(uint64_t /*addr*/, uint32_t /*value*/);
    bool write_global_u64(uint64_t /*addr*/, uint64_t /*value*/);

    /// Stub: PTXIR byte decode (PTX-EMU fcdad151 未暴露 decode_ptxir 自由函数)
    std::vector<StatementContext> decode_ptxir(const std::vector<uint8_t>& ptxir_bytes);

private:
    bool initialized_ = false;
    DeviceConfig config_{};
    std::string ptx_emu_root_;

    // HSK-8 Phase 2: 持有 IPtxEmuDevice unique_ptr (cpp 抽象边界)
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tlm

#endif  // TLM_GPU_PTX_EMU_SUBMODULE_MVP_HH