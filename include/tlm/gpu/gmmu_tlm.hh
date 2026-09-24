// include/tlm/gpu/gmmu_tlm.hh
// GmmuTLM: 一级页表翻译 (per openspec/changes/cpptlm-minimal-dgpu-soc-v1 A3)
// 镜像 gem5 PhysicalMemory/AbstractMemory 角色: SDMA 通过 translate_cb 注入,
// backing 是同一份 framebuffer_, 直读直写页表项。
//
// 功能: 一级页表 iova → paddr 翻译 (无 TLB, 每次即时翻译)
// 约束: 4KB 固定页; 跨页 DMA 返回 -EIO (v1.0 不支持); 不缓存
// 作者: CppTLM Team · 日期: 2027-02-09
#ifndef CPPTLM_GMMU_TLM_H
#define CPPTLM_GMMU_TLM_H

#include "core/sim_module.hh"
#include "core/sim_object.hh"
#include <cstdint>
#include <cstring>
#include <cerrno>

namespace tlm::gpu {

class GmmuTLM : public ::SimModule {
public:
    explicit GmmuTLM(const std::string& n, EventQueue* eq)
        : ::SimModule(n, eq) {}
    ~GmmuTLM() override = default;

    std::string get_module_type() const override { return "GmmuTLM"; }

    // ── 寄存器接口 (DGpuBoard BAR0 hook 调用) ──
    void set_pt_base_lo(uint32_t lo) noexcept { pt_base_lo_ = lo; }
    void set_pt_base_hi(uint32_t hi) noexcept { pt_base_hi_ = hi; }
    void set_enabled(bool en) noexcept { enabled_ = en; }
    uint64_t pt_base() const noexcept {
        return (static_cast<uint64_t>(pt_base_hi_) << 32) |
               static_cast<uint64_t>(pt_base_lo_);
    }

    // ── backing 注入 (DGpuBoard::bind_memory_backings 调用) ──
    void set_backing(uint8_t* ptr, uint64_t sz) noexcept {
        backing_ = ptr;
        backing_size_ = sz;
    }

    // ── 翻译 API (DmaTranslateCb 签名严格匹配) ──
    // 返回 0 成功, -EIO 失败; out_paddr 仅在成功时写入
    int translate(uint64_t iova, uint32_t size, uint64_t& out_paddr) {
        if (!enabled_ || !backing_ || pt_base() == 0) return -EIO;

        constexpr uint64_t page_size = 4096;
        constexpr uint64_t page_mask = page_size - 1;

        // 页跨界检查 (v1.0 不支持跨页描述符, per design D7)
        if ((iova & ~page_mask) != ((iova + size - 1) & ~page_mask)) {
            return -EIO;
        }

        const uint64_t idx = iova >> 12;
        const uint64_t pte_addr = pt_base() + idx * 8;

        if (pte_addr + 8 > backing_size_) return -EIO;

        uint64_t pte = 0;
        std::memcpy(&pte, backing_ + pte_addr, sizeof(pte));

        const bool valid = (pte & 1ULL) != 0;
        const uint64_t paddr_base = pte & ~page_mask;

        if (!valid) return -EIO;

        out_paddr = paddr_base | (iova & page_mask);
        return 0;
    }

    // on_config_loaded: v1.0 读 page_size_bytes (固定 4096); v1.1 扩展支持 2MB/1GB
    void on_config_loaded() override {
        // v1.0: page_size 固定 4096, 参数保留以备 v1.1 扩展
        // 当前实现: 不动内部状态, 由调用方按需重新 set_backing / set_enabled
    }

private:
    uint32_t pt_base_lo_ = 0;
    uint32_t pt_base_hi_ = 0;
    bool enabled_ = false;
    uint8_t* backing_ = nullptr;
    uint64_t backing_size_ = 0;
};

} // namespace tlm::gpu

#endif // CPPTLM_GMMU_TLM_H
