// include/bundles/dma_bundles_tlm.hh
// DMA Bundle 定义（轻量级 TLM 侧，DmaDescriptorBundle + CompletionBundle）
// 功能描述：定义 SdmaEngineTLM 5 端口组件使用的 POD Bundle
//           - DmaDescriptorBundle：DMA 描述符（desc_in 入口）
//           - CompletionBundle：完成/错误通知（done_out 出口）
//           该文件为 TLM 侧 Bundle，与 PcieTlpBundle（pcie_bundles_tlm.hh）严格分离。
// 作者 CppTLM Team / 日期 2026-08-26
// 参考: openspec/changes/2026-08-26-cpptlm-dgpu-sdma-engine/design.md §2
//       ADR-SOC-07 D3 (PCIe master 归属 SOC)
//       ADR-088 §D3.8 (cpptlm_dma_translate_cb 同步签名)
#ifndef BUNDLES_DMA_BUNDLES_TLM_HH
#define BUNDLES_DMA_BUNDLES_TLM_HH

#include "bundles/cpphdl_types.hh"
#include <cstdint>

namespace bundles {

/**
 * @brief DMA 描述符 Bundle（轻量级 TLM 侧，POD）
 *
 * 字段（per design.md §2）：
 *   - dir         : 传输方向 (0=H2D, 1=D2H)
 *   - host_iova   : host 侧 IOVA（64-bit）
 *   - vram_offset : SOC VRAM 内偏移（64-bit）
 *   - size        : 字节数（32-bit）
 *   - tag         : 完成关联 ID（32-bit）
 *
 * 设计原则（per design.md §2）：
 *   - 轻量级：仅 POD 字段，无 CppHDL AST 依赖
 *   - C++17 兼容：可在 cpptlm_core（C++17 静态库）中使用
 *   - POD 惯例：不携带 shared_ptr/inline buffer 等堆指针
 *
 * 范围冻结（per design.md §2 + proposal.md §1.2）：
 *   - 本文件由 cpptlm-dgpu-sdma-engine 独立创建
 *   - 不修改 include/bundles/pcie_bundles_tlm.hh（change A 交付）
 *   - 与 cache/noc/compute_bundles_tlm.hh 目录惯例一致（按能力域分文件）
 *   - git merge 与 change A 零冲突
 */
struct DmaDescriptorBundle : public bundle_base {
    // Dir 编码（与 DmaDescriptor::Dir 一致）
    static constexpr uint8_t DIR_H2D = 0;  // host→device
    static constexpr uint8_t DIR_D2H = 1;  // device→host

    ch_uint<8>  dir;             // 传输方向 (0=H2D, 1=D2H)
    ch_uint<64> host_iova;       // host 侧 IOVA（经 IOMMU translate 为 PA）
    ch_uint<64> vram_offset;     // SOC VRAM 内偏移
    ch_uint<32> size;            // 字节数（0 表示空描述符，触发拒绝）
    ch_uint<32> tag;             // 完成关联 ID

    DmaDescriptorBundle() = default;

    DmaDescriptorBundle(uint8_t d, uint64_t iova, uint64_t vram_off, uint32_t sz, uint32_t t)
        : dir(d), host_iova(iova), vram_offset(vram_off), size(sz), tag(t) {}

    // 谓词：是否为 H2D 方向
    bool is_h2d() const {
        return dir.read() == DIR_H2D;
    }

    // 谓词：是否为 D2H 方向
    bool is_d2h() const {
        return dir.read() == DIR_D2H;
    }
};

/**
 * @brief 完成通知 Bundle（轻量级 TLM 侧，POD）
 *
 * 字段（per design.md §2）：
 *   - task_id : 任务 ID（与 DMA 描述符的 tag 关联，但语义上独立——task_id
 *               用于 CompletionRing 侧匹配，tag 用于 SOC 内部完成通知）
 *   - status  : 0=OK；<0=error（per errno 语义）
 *               典型错误：
 *                 -EINVAL (-22): size==0 / vram_offset 越界
 *                 -EIO (-5): translate callback 失败 / PCIe RequesterCompleterAbort
 *   - tag     : 完成关联 ID（与 DmaDescriptorBundle.tag 对齐）
 *
 * 所有权声明（per design.md §2 Ownership + tasks.md T-sd-1 验证项）：
 *   - 本 change (cpptlm-dgpu-sdma-engine) 是 `bundles::CompletionBundle` 的
 *     唯一所有者
 *   - board-soc-split change (T-bs-2) 必须复用本类型，不得在其
 *     dgpu_bundles_tlm.hh 中重复定义
 *   - 如需字段扩展，先在本文件改并通知 board-soc-split 跟进
 *   - 验证方法：`grep -c "CompletionBundle 所有权" design.md ≥ 1`
 */
struct CompletionBundle : public bundle_base {
    ch_uint<32> task_id;     // 任务 ID（用于 CompletionRing 匹配）
    ch_uint<32> status;      // 完成状态（0=OK, <0=errno）
    ch_uint<32> tag;         // 完成关联 ID（与 DmaDescriptorBundle.tag 对齐）

    CompletionBundle() = default;

    CompletionBundle(uint32_t tid, int32_t st, uint32_t t)
        : task_id(tid), status(static_cast<uint32_t>(st)), tag(t) {}

    // 谓词：是否成功
    bool is_ok() const {
        return static_cast<int32_t>(status.read()) == 0;
    }

    // 谓词：是否错误
    bool is_error() const {
        return static_cast<int32_t>(status.read()) < 0;
    }
};

} // namespace bundles

#endif // BUNDLES_DMA_BUNDLES_TLM_HH
