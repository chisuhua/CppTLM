// include/tlm/gpu/sub_core_slot.hh
// SubCoreSlot: GPU Compute Unit 内部 4-way sub-core slot 状态
// 作者 CppTLM Team / 日期 2026-06-30
#ifndef TLM_GPU_SUB_CORE_SLOT_HH
#define TLM_GPU_SUB_CORE_SLOT_HH

#include <cstdint>

namespace tlm {

struct SubCoreSlot {
    uint32_t warp_id = 0xFFFFFFFFu;  // 0xFFFFFFFF = idle
    uint32_t remaining_cycles = 0;
    bool busy = false;

    void occupy(uint32_t warp, uint32_t cycles) {
        warp_id = warp;
        remaining_cycles = cycles;
        busy = true;
    }

    void release() {
        warp_id = 0xFFFFFFFFu;
        remaining_cycles = 0;
        busy = false;
    }

    void tick() {
        if (busy && remaining_cycles > 0) {
            --remaining_cycles;
            if (remaining_cycles == 0) {
                busy = false;
            }
        }
    }
};

}  // namespace tlm

#endif  // TLM_GPU_SUB_CORE_SLOT_HH
