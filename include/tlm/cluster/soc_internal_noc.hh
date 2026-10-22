// include/tlm/cluster/soc_internal_noc.hh
// SocInternalNoc: SoC 内部 NoC (DataNoC 物理保真, E.1 v1.1)
//
// E.1 v1.1 (per cpptlm-dgpu-soc-internal-noc-e1 v1.1 design.md, 2026-10-09):
//   - E.0 (2026-10-08) 提供 SocInternalNoc 路由骨架 + 4 NODE_ID + NoCFlitBundle 16 字段编译期契约
//   - E.1 v1.1 升级: route_flit 实际推送 + flits_dispatched_ 计数 + vc_counts_[4] 按 flit_category 分桶
//   - 新增 send_request/response API + accept_fn callback 注册机制
//
// 物理对应 (per architecture.md §2.4 + Oracle RTL 调研):
//   - 真实芯片: ARM CHI 4 通道 (REQ/RSP/DAT/SNP) + 4 独立 VC → 简化抽象为 flit_category + vc_counts_[4]
//   - E.1 v1.1 范围: DataNoC only (SDMA bulk DMA 路径); PT_BASE / doorbell / page walk 走独立通道 (defer to E.2/ArchForge)
//
// 与 GpuInternalFabric 关系:
//   - GpuInternalFabric 继续承担 VRAM bridge (Fabric2VramCtrlBundle, 现有)
//   - SocInternalNoc 仅承载 SDMA→VramControllerTLM bulk DMA 流量
//   - 0 触动既有 Fabric2VramCtrlBridge 路径
//
// Frozen surface (0 修改):
//   - include/bundles/noc_bundles_tlm.hh (NoCFlitBundle 既有, 16 字段 v2.1)
//   - include/bundles/dgpu_fabric_bundles_tlm.hh (Fabric2VramCtrlBundle 既有)
//   - include/abi/cpptlm_emulator.h (23 ABI 冻结, per ADR-DGPU-08 §2.3)
//   - 4 target 模块现有 wire format (v1.1 不切, 用 accept_fn callback 注册机制)
//
// 作者: CppTLM Team / 日期: 2026-10-08 (E.0) / 2026-10-09 (E.1 v1.1 升级)
#ifndef CPPTLM_SOC_INTERNAL_NOC_H
#define CPPTLM_SOC_INTERNAL_NOC_H

#include "bundles/noc_bundles_tlm.hh"      // NoCFlitBundle (16 字段 wire format, v2.1)
#include "core/sim_module.hh"              // SimModule 基类
#include "core/event_queue.hh"

#include <array>
#include <cstdint>
#include <functional>

// Forward-decl 4 target 模块 (避免循环 include; per Fabric2VramCtrlBundle
// precedent in dgpu_fabric_bundles_tlm.hh A.3 注释):
//   tlm::pcie::PcieControllerIP  (PcieControllerIP facade) — v1.1 不接 (per CRITICAL C1)
//   tlm::gpu::SdmaEngineTLM      (SDMA engine) — v1.1 唯一入向 (per e1 DataNoC only)
//   tlm::gpu::GmmuTLM            (GMMU page walker) — v1.1 不接 (per CRITICAL C2, 独立 memory client)
//   ::VramControllerTLM          (VRAM controller) — v1.1 唯一入向 + 出向
namespace tlm::pcie { class PcieControllerIP; }
namespace tlm::gpu  { class SdmaEngineTLM; }
namespace tlm::gpu  { class GmmuTLM; }
class VramControllerTLM;

namespace tlm::cluster {

// SoC 内部 NoC: E.1 v1.1 DataNoC only (per design §1.1)
class SocInternalNoc : public ::SimModule {
public:
    explicit SocInternalNoc(const std::string& name, EventQueue* eq)
        : ::SimModule(name, eq) {}

    ~SocInternalNoc() override = default;

    std::string get_module_type() const override { return "SocInternalNoc"; }

    // ── 4 Node ID 常量 (per architecture.md §2.4 IP 表格顺序) ──
    // 唯一标识 4 target 模块的 mesh 节点 ID,用于 flit.dst_node 路由查表
    static constexpr uint32_t NODE_ID_PCIE = 0;  // PcieControllerIP (v1.1 无入向流量, 仅 NODE_ID 保留对称性)
    static constexpr uint32_t NODE_ID_SDMA = 1;  // SdmaEngineTLM (v1.1 唯一入向)
    static constexpr uint32_t NODE_ID_GMMU = 2;  // GmmuTLM (v1.1 不接, per CRITICAL C2 独立 memory client)
    static constexpr uint32_t NODE_ID_VRAM = 3;  // VramControllerTLM (v1.1 唯一入向 + 出向)

    // ── 4 Virtual Channel IDs (per ARM CHI 4 VC 物理对应, e1 v1.1 design §3.4) ──
    // 按 flit_category **索引对齐**分桶 (per noc_bundles_tlm.hh:60-62):
    //   - VC_REQ = 0 (CATEGORY_REQUEST, e.g. SDMA bulk DMA write/read req)
    //   - VC_RSP = 1 (CATEGORY_RESPONSE, e.g. VRAM response to SDMA)
    //   - VC_DAT = 2 (CATEGORY_CREDIT —— **索引对齐**的副产物;当前 CATEGORY_* 无 data 类)
    //   - VC_SNP = 3 (当前无 CATEGORY_* 落入此桶 —— 为 snoop 类扩展预留)
    //
    // ⚠️ **索引对齐约束**(M-4,per docs/implementation/2026-10-21-cep-archforge-response-receipt.md §8.4.3):
    //    route_flit() 以 `cat = flit_category` **直接**做 vc_counts_ 的桶下标,
    //    故 CATEGORY_* 的值域必须 ⊆ [0, VC_COUNT) —— 否则越界的 cat 会被
    //    `(cat < VC_COUNT) ? cat : VC_SNP` 守卫**静默折叠**进 VC_SNP(无报错、无计数)。
    //    → CATEGORY_* 保持**传输层 3 类**;6 类一致性消息词表落在 **Coherence Interface 层**
    //      (per ADR-CEP-12 §8 IC2 ③ 分层纪律),**不**扩本层。
    //    → 本注释块与 noc_bundles_tlm.hh 的 CATEGORY_* 集合由 lint 第 10 组机器校验
    //      (scripts/test/cep_docs_sync_check.sh),防止再出现「幽灵类别」。
    static constexpr unsigned VC_REQ = 0;
    static constexpr unsigned VC_RSP = 1;
    static constexpr unsigned VC_DAT = 2;
    static constexpr unsigned VC_SNP = 3;
    static constexpr unsigned VC_COUNT = 4;

    // ── Target 接入回调类型 (E.1 v1.1 新增) ──
    // accept_fn callback 机制: 4 target setter 不必修改类定义
    // (避免 4 target 类继承 NocAcceptor 抽象基类)
    // SDMA/VRAM 通过 set_*_accept_fn 注册 (Phase 2.1/2.2 adapter 实现);
    // Pcie/GMMU 不注册 (per v1.1 scope 缩减).
    using AcceptFn = std::function<void(const bundles::NoCFlitBundle&)>;

    // ── 4 Target Setters (E.0 既有, 0 改动) ──
    // Programmatic bridge 模式, 避免 JSON 声明式端口被 §14.2.3 静默丢弃
    // (per Phase 8 HB/RC tick-forwarding precedent + cpptlm-p2-integration-unblock D7)
    void set_pcie_target(tlm::pcie::PcieControllerIP* target) noexcept { pcie_ = target; }
    void set_sdma_target(tlm::gpu::SdmaEngineTLM* target) noexcept   { sdma_ = target; }
    void set_gmmu_target(tlm::gpu::GmmuTLM* target) noexcept         { gmmu_ = target; }
    void set_vram_target(::VramControllerTLM* target) noexcept        { vram_ = target; }

    // ── 4 Target Accessors (E.0 既有, nullptr-safe) ──
    tlm::pcie::PcieControllerIP* pcie_target() const noexcept { return pcie_; }
    tlm::gpu::SdmaEngineTLM*    sdma_target() const noexcept { return sdma_; }
    tlm::gpu::GmmuTLM*         gmmu_target() const noexcept { return gmmu_; }
    ::VramControllerTLM*        vram_target() const noexcept { return vram_; }

    // ── 4 Accept Callback Registration (E.1 v1.1 新增) ──
    // 用于 SDMA/VRAM adapter 注册 accept_fn (Phase 2.1/2.2 实施).
    // AcceptFn 接受 NoCFlitBundle (含 src_node/dst_node/transaction_id/address/size/vc_id/flit_category).
    // nullptr (默认) = 优雅降级: route_flit 跳过推送, 仍增 flits_dispatched_ 计数.
    void set_pcie_accept_fn(AcceptFn fn) noexcept { pcie_accept_fn_ = std::move(fn); }
    void set_sdma_accept_fn(AcceptFn fn) noexcept { sdma_accept_fn_ = std::move(fn); }
    void set_gmmu_accept_fn(AcceptFn fn) noexcept { gmmu_accept_fn_ = std::move(fn); }
    void set_vram_accept_fn(AcceptFn fn) noexcept { vram_accept_fn_ = std::move(fn); }

    // ── 路由入口 (per E.1 v1.1 design §3.4) ──
    // 读 flit.dst_node, switch 4 cases, 实际推 flit 到 target->accept_fn(flit)
    //   + 增 flits_dispatched_ 计数 (替代 E.0 的 flits_routed_, 仅成功推送时增)
    //   + 增 vc_counts_[flit_category] 计数 (ARM CHI 4 VC 物理对应, 即使 dst 未知也增)
    // 未知 dst_node (>3) → unknown_dst_count++ + 0 crash (D.2 优雅降级, flits_dispatched_ 不增)
    // nullptr accept_fn + 路由触发 → 0 crash (D.2 优雅降级, flits_dispatched_ 仍增)
    void route_flit(const bundles::NoCFlitBundle& flit) noexcept {
        const uint32_t dst = flit.dst_node.read();
        const uint8_t  cat = flit.flit_category.read();
        const unsigned vc = (cat < VC_COUNT) ? cat : VC_SNP;  // 范围保护
        ++vc_counts_[vc];

        const AcceptFn* fn = nullptr;
        switch (dst) {
            case NODE_ID_PCIE: fn = &pcie_accept_fn_; break;
            case NODE_ID_SDMA: fn = &sdma_accept_fn_; break;
            case NODE_ID_GMMU: fn = &gmmu_accept_fn_; break;
            case NODE_ID_VRAM: fn = &vram_accept_fn_; break;
            default:
                // 未知 dst: 不增 flits_dispatched_, 仅 unknown_dst_count_ (per E.0 行为兼容)
                ++unknown_dst_count_;
                return;
        }

        // 已知 dst: 增 flits_dispatched_ (E.0 行为: 4 valid paths ++)
        ++flits_dispatched_;

        if (fn && *fn) {
            (*fn)(flit);  // 实际推送 (nullptr-safe 优雅降级, nullptr accept_fn 不影响计数)
        }
    }

    // ── Send API (E.1 v1.1 新增, per design §3.4) ──
    // 路由统一由 flit.dst_node 决定, 无 target_ptr 参数
    // (消除双路由源冗余, per Oracle 二轮附注)
    // 注: route_flit 已按 flit_category 增 vc_counts_[flit_category]
    // send_request/send_response 仅作语义标识 (API 显式性), 不重复计数
    void send_request(const bundles::NoCFlitBundle& req) noexcept {
        // 语义标识: send_request 期望 CATEGORY_REQUEST
        // VC 计数由 route_flit 内部完成 (避免双计)
        route_flit(req);
    }

    void send_response(const bundles::NoCFlitBundle& resp) noexcept {
        // 语义标识: send_response 期望 CATEGORY_RESPONSE
        // VC 计数由 route_flit 内部完成
        route_flit(resp);
    }

    // ── 统计 (E.0 既有 + E.1 v1.1 新增) ──
    // E.0 既有: 保持向后兼容 (既有 [realip][pin-contract][noc] 5 cases 依赖)
    uint64_t flits_routed() const noexcept { return flits_dispatched_; }  // 兼容别名
    uint64_t unknown_dst_count() const noexcept { return unknown_dst_count_; }

    // E.1 v1.1 新增:
    uint64_t flits_dispatched() const noexcept { return flits_dispatched_; }
    uint64_t vc_count(unsigned vc_id) const noexcept {
        return (vc_id < VC_COUNT) ? vc_counts_[vc_id] : 0;
    }
    uint64_t vc_req_count() const noexcept { return vc_counts_[VC_REQ]; }
    uint64_t vc_rsp_count() const noexcept { return vc_counts_[VC_RSP]; }
    uint64_t vc_dat_count() const noexcept { return vc_counts_[VC_DAT]; }
    // ⚠️ **语义遗留**:VC_DAT 桶当前由 CATEGORY_CREDIT 占据(索引对齐的副产物),
    //    故 vc_dat_count() 实际统计的是 **credit 流量**,不是数据流量。
    //    改映射需单独决策(per 台账 Entry #11 ④ 的遗留声明)。
    uint64_t vc_snp_count() const noexcept { return vc_counts_[VC_SNP]; }

    void do_reset(const ResetConfig& config) override {
        ::SimModule::do_reset(config);
        flits_dispatched_ = 0;  // E.1 v1.1: 替代 E.0 的 flits_routed_
        unknown_dst_count_ = 0;
        vc_counts_.fill(0);
        // 注意: pcie_/sdma_/gmmu_/vram_ raw 指针 与 accept_fn callback 不重置
        // (per E.0 设计: 生命周期由 DGpuBoard 管理, 跨 reset 保持)
    }

private:
    // 4 target raw 指针 (E.0 既有, non-owning, 生命周期由 DGpuBoard 管理)
    tlm::pcie::PcieControllerIP* pcie_ = nullptr;
    tlm::gpu::SdmaEngineTLM*    sdma_ = nullptr;
    tlm::gpu::GmmuTLM*         gmmu_ = nullptr;
    ::VramControllerTLM*        vram_ = nullptr;

    // 4 target accept callback (E.1 v1.1 新增)
    // nullptr = 优雅降级: route_flit 跳过推送, 仍增计数
    AcceptFn pcie_accept_fn_;
    AcceptFn sdma_accept_fn_;
    AcceptFn gmmu_accept_fn_;
    AcceptFn vram_accept_fn_;

    // 统计 (E.1 v1.1: flits_routed_ 改名 + 新增 vc_counts_)
    uint64_t flits_dispatched_ = 0;       // 实际推送总数 (替代 E.0 flits_routed_)
    uint64_t unknown_dst_count_ = 0;      // 未知 dst_node 计数
    std::array<uint64_t, VC_COUNT> vc_counts_{};  // 4 VC 桶 (REQ/RSP/DAT/SNP)
};

} // namespace tlm::cluster

#endif // CPPTLM_SOC_INTERNAL_NOC_H
