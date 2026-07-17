// =====================================================================
// CppTLM ↔ PTX-EMU ABI 真值源 (vendored, DO NOT MODIFY)
// =====================================================================
// Source         : github.com/chisuhua/PTX-EMU @ commit 8acfd2d1
// Source path    : include/ptxsim/scoreboard_interface.h (16 lines)
// Vendor method  : `git show 8acfd2d1:include/ptxsim/scoreboard_interface.h`
// Vendor SHA-256 : 见 include/cudart/AGENTS.md 验收检查表 (每次 rebase 后更新)
//
// 任何修改必须先在 PTX-EMU 端提交新 commit，再同步 rebase 此 vendor 文件。
// 修改流程参考 include/cudart/AGENTS.md "Sync policy"。
// =====================================================================

#ifndef PTXSIM_SCOREBOARD_INTERFACE_H
#define PTXSIM_SCOREBOARD_INTERFACE_H
#include <cstdint>

/// Pure virtual interface for CppTLM Scoreboard injection.
/// Zero external dependencies (only <cstdint>).
/// Ref: ADR-0020, CppTLM RFC-P1-001 §3.1
class IScoreboard {
public:
    virtual ~IScoreboard() = default;
    virtual bool has_free_entry() const = 0;
    virtual bool allocate(uint32_t reg_id, uint32_t warp_id) = 0;
    virtual bool release(uint32_t reg_id, uint32_t warp_id) = 0;
    virtual void tick() = 0;
};
#endif
