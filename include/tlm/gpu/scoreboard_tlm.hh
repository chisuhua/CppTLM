// include/tlm/gpu/scoreboard_tlm.hh
// ScoreboardTLM: IScoreboard 实现 — D1-Full P1 Phase 1 核心模块
// 功能: Global hazard table (64 warps × 8 in-flight × 3 dest, CAPACITY=2048),
//       O(1) allocate/release via unordered_map, cross-kernel reset()
// 作者 CppTLM Team / 日期 2026-07-18
// 参考:
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/design.md §2.1
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/spec.md §cpptlm-scoreboard
//   - openspec/changes/cpptlm-d1-p1-pipeline-scoreboard/tasks.md §1.1
//   - include/cudart/scoreboard_interface.h (vendor from PTX-EMU 8acfd2d1)
//
// Design Revision 3 (2026-07-18 Oracle P0 审查):
//   - 数据结构: std::array → std::unordered_map (O(1) allocate/release,
//     消除线性扫描 O(N) 时序失真)
//   - CAPACITY = 2048: 覆盖 64 warps × 8 in-flight × 3 dest × 1.33 margin
//   - 新增 reset(): 跨 kernel 清空所有 entries（非虚, CppTLM 特有）
//   - 内存优化: reserve(2048) 预分配防碎片
#ifndef TLM_GPU_SCOREBOARD_TLM_HH
#define TLM_GPU_SCOREBOARD_TLM_HH

#include "cudart/scoreboard_interface.h"  // IScoreboard (vendor, 零依赖)

#include <cstdint>
#include <unordered_map>

namespace tlm {

/// ScoreboardTLM: IScoreboard 的 CppTLM 端实现（直接继承, 无 Internal 层）
///
/// 设计原则:
///   - Global scoreboard 语义: 所有 warp 共享一个实例, (reg_id, warp_id) 二元组索引
///     依据: PTX-EMU sm_context.h:188 (scoreboard_ 是 SMContext 成员, 非 per-warp)
///   - CAPACITY = 2048: 覆盖最坏场景 (64 warps × 8 in-flight × 3 dest × 1.33 margin)
///   - O(1) allocate/release: 通过 unordered_map<(warp_id, reg_id)> 实现
///   - duplicate allocate 返回 false (rejects): 匹配 PTX-EMU rollback 逻辑 (sm_context.cpp:37-43)
///   - tick(): Phase 1 no-op — 死锁缓解由容量(2048) + rollback 完整性 + decrement_blocked_cycles 保障
///     (Phase 4 可加超时释放)
///   - reset(): 跨 kernel 清空 entries（Phase 4 PTX-EMU 在 kernel launch 前调用）
///   - 非线程安全: D1-Full 阶段单线程假设
class ScoreboardTLM : public IScoreboard {
public:
    /// 容量上限: 覆盖 64 warps × 8 in-flight × 3 dest regs × 1.33 margin
    static constexpr size_t CAPACITY = 2048;

    ScoreboardTLM();
    ~ScoreboardTLM() override = default;

    // IScoreboard 4 纯虚方法
    bool has_free_entry() const override;
    bool allocate(uint32_t reg_id, uint32_t warp_id) override;
    bool release(uint32_t reg_id, uint32_t warp_id) override;
    void tick() override;

    /// 跨 kernel 重置 — 清空所有 scoreboard entries
    /// 非虚 (CppTLM 特有, IScoreboard vendored 接口不含此方法)
    /// Phase 4 集成: PTX-EMU 在 kernel launch 前调用
    void reset();

private:
    /// 组合 reg_id + warp_id 为单键: (warp_id << 32) | reg_id
    static uint64_t make_key(uint32_t reg_id, uint32_t warp_id) {
        return (static_cast<uint64_t>(warp_id) << 32) | reg_id;
    }

    /// key → active flag (always true; false entries 不插入)
    std::unordered_map<uint64_t, bool> entries_;
};

}  // namespace tlm

#endif  // TLM_GPU_SCOREBOARD_TLM_HH
