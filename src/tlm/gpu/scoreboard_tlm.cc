// src/tlm/gpu/scoreboard_tlm.cc
// ScoreboardTLM: IScoreboard 实现 — O(1) global hazard table (CAPACITY=2048)
// 作者 CppTLM Team / 日期 2026-07-18
// 参考: include/tlm/gpu/scoreboard_tlm.hh
//
// Design Revision 3 (2026-07-18 Oracle P0):
//   - allocate/release: O(1) via unordered_map::emplace/erase
//   - reset(): 跨 kernel 清空所有 entries
//   - reserve(2048): 预分配防内存碎片
#include "tlm/gpu/scoreboard_tlm.hh"

namespace tlm {

    ScoreboardTLM::ScoreboardTLM() {
        entries_.reserve(CAPACITY); // 预分配，防频繁 rehash
    }

    bool ScoreboardTLM::has_free_entry() const {
        return entries_.size() < CAPACITY;
    }

    bool ScoreboardTLM::allocate(uint32_t reg_id, uint32_t warp_id) {
        if (entries_.size() >= CAPACITY) {
            return false; // table full
        }
        // emplace returns pair<iterator, bool> — second=false 表示 key 已存在 (duplicate)
        auto [it, inserted] = entries_.emplace(make_key(reg_id, warp_id), true);
        return inserted; // false = duplicate rejects (2026-07-18 决议, 见 design.md §2.1)
    }

    bool ScoreboardTLM::release(uint32_t reg_id, uint32_t warp_id) {
        return entries_.erase(make_key(reg_id, warp_id)) > 0;
    }

    void ScoreboardTLM::tick() {
        // Phase 1 no-op: 死锁缓解由容量 (2048) + PTX-EMU Step A rollback 完整性
        // + decrement_blocked_cycles 每 tick 执行保障。Phase 4 可加超时释放。
    }

    void ScoreboardTLM::reset() {
        entries_.clear();
        // reserve 在 clear() 后保留 — 避免下次 kernel 重新分配
        entries_.reserve(CAPACITY);
    }

} // namespace tlm
