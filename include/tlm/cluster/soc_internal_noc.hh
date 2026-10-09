// include/tlm/cluster/soc_internal_noc.hh
// SocInternalNoc: SoC 内部 NoC (functional routing + interface compat)
//
// cpptlm-dgpu-soc-internal-noc-minimal (2026-10-08, New OpenSpec change)
// 范围: "功能路由 + 接口兼容" 最小实现 — 对齐 Arteris FlexNoC / Custom Mesh IP
// 端口契约 (NoCFlitBundle 18 字段),不模拟 multi-VC/mesh/QoS (留 ArchForge scope)
//
// 物理对应 (per architecture.md §2.4):
//   - 真实芯片: Arteris FlexNoC / Custom Mesh IP
//   - 职责: 在 PcieControllerIP / SdmaEngineTLM / GmmuTLM / VramControllerTLM
//     之间路由 NoCFlitBundle (mesh 寻址, 4 NODE_ID 唯一)
//
// 与 GpuInternalFabric 关系 (per design.md §5.1):
//   - GpuInternalFabric 继续承担 VRAM bridge (Fabric2VramCtrlBundle, 现有)
//   - SocInternalNoc 新增, 承担 SoC 范围 4 target 路由 (本期为路由骨架,
//     实际数据流经既有 DGpuBoard::mmio_read/write 直调路径)
//
// Frozen surface (0 修改):
//   - include/bundles/noc_bundles_tlm.hh (NoCFlitBundle 既有, 18 字段)
//   - include/bundles/dgpu_fabric_bundles_tlm.hh (Fabric2VramCtrlBundle 既有)
//   - include/abi/cpptlm_emulator.h (23 ABI 冻结)
//   - 4 target 模块现有 wire format (本期不切)
//
// 作者: CppTLM Team / 日期: 2026-10-08
#ifndef CPPTLM_SOC_INTERNAL_NOC_H
#define CPPTLM_SOC_INTERNAL_NOC_H

#include "bundles/noc_bundles_tlm.hh"      // NoCFlitBundle (18 字段 wire format)
#include "core/sim_module.hh"              // SimModule 基类
#include "core/event_queue.hh"

#include <cstdint>

// Forward-decl 4 target 模块 (避免循环 include; per Fabric2VramCtrlBundle
// precedent in dgpu_fabric_bundles_tlm.hh A.3 注释):
//   tlm::pcie::PcieControllerIP  (PcieControllerIP facade)
//   tlm::gpu::SdmaEngineTLM      (SDMA engine)
//   tlm::gpu::GmmuTLM            (GMMU page walker)
//   ::VramControllerTLM          (VRAM controller)
namespace tlm::pcie { class PcieControllerIP; }
namespace tlm::gpu  { class SdmaEngineTLM; }
namespace tlm::gpu  { class GmmuTLM; }
class VramControllerTLM;

namespace tlm::cluster {

// SoC 内部 NoC: stateless 路由骨架 (per design.md §2.2)
class SocInternalNoc : public ::SimModule {
public:
    explicit SocInternalNoc(const std::string& name, EventQueue* eq)
        : ::SimModule(name, eq) {}

    ~SocInternalNoc() override = default;

    std::string get_module_type() const override { return "SocInternalNoc"; }

    // ── 4 Node ID 常量 (per architecture.md §2.4 IP 表格顺序) ──
    // 唯一标识 4 target 模块的 mesh 节点 ID,用于 flit.dst_node 路由查表
    static constexpr uint32_t NODE_ID_PCIE = 0;  // PcieControllerIP
    static constexpr uint32_t NODE_ID_SDMA = 1;  // SdmaEngineTLM
    static constexpr uint32_t NODE_ID_GMMU = 2;  // GmmuTLM
    static constexpr uint32_t NODE_ID_VRAM = 3;  // VramControllerTLM

    // ── 4 Target Setters (per GpuInternalFabric::set_vram_target pattern) ──
    // Programmatic bridge 模式, 避免 JSON 声明式端口被 §14.2.3 静默丢弃
    // (per Phase 8 HB/RC tick-forwarding precedent + cpptlm-p2-integration-unblock D7)
    void set_pcie_target(tlm::pcie::PcieControllerIP* target) noexcept {
        pcie_ = target;
    }
    void set_sdma_target(tlm::gpu::SdmaEngineTLM* target) noexcept {
        sdma_ = target;
    }
    void set_gmmu_target(tlm::gpu::GmmuTLM* target) noexcept {
        gmmu_ = target;
    }
    void set_vram_target(::VramControllerTLM* target) noexcept {
        vram_ = target;
    }

    // ── 4 Target Accessors (nullptr-safe, per A.3 修订) ──
    tlm::pcie::PcieControllerIP* pcie_target() const noexcept { return pcie_; }
    tlm::gpu::SdmaEngineTLM*    sdma_target() const noexcept { return sdma_; }
    tlm::gpu::GmmuTLM*         gmmu_target() const noexcept { return gmmu_; }
    ::VramControllerTLM*        vram_target() const noexcept { return vram_; }

    // ── 路由入口 (per design.md §2.3) ──
    // 读 flit.dst_node, switch 4 cases, 增相应统计
    // 未知 dst_node (>3) → unknown_dst_count++ + 0 crash (D.2 优雅降级)
    // nullptr target + 路由触发 → 0 crash (D.2 优雅降级)
    void route_flit(const bundles::NoCFlitBundle& flit) noexcept {
        const uint32_t dst = flit.dst_node.read();
        switch (dst) {
        case NODE_ID_PCIE: ++flits_routed_; break;
        case NODE_ID_SDMA: ++flits_routed_; break;
        case NODE_ID_GMMU: ++flits_routed_; break;
        case NODE_ID_VRAM: ++flits_routed_; break;
        default: ++unknown_dst_count_; break;
        }
        // 未来 E.1: 实际把 flit 推到 target->accept(flit)
    }

    // ── 统计 ──
    uint64_t flits_routed() const noexcept { return flits_routed_; }
    uint64_t unknown_dst_count() const noexcept { return unknown_dst_count_; }

    void do_reset(const ResetConfig& config) override {
        ::SimModule::do_reset(config);
        flits_routed_ = 0;
        unknown_dst_count_ = 0;
    }

private:
    // 4 target raw 指针 (non-owning, 生命周期由 DGpuBoard 管理)
    tlm::pcie::PcieControllerIP* pcie_ = nullptr;
    tlm::gpu::SdmaEngineTLM*    sdma_ = nullptr;
    tlm::gpu::GmmuTLM*         gmmu_ = nullptr;
    ::VramControllerTLM*        vram_ = nullptr;

    // 统计 (per D.2 优雅降级 + 测试可观察)
    uint64_t flits_routed_ = 0;
    uint64_t unknown_dst_count_ = 0;
};

} // namespace tlm::cluster

#endif // CPPTLM_SOC_INTERNAL_NOC_H
