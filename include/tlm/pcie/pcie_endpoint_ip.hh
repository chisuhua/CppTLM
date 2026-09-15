// include/tlm/pcie/pcie_endpoint_ip.hh
// PcieEndpointIP: SR-IOV PCIe Endpoint IP 模型 (17 端口 = 1 PF + 16 VF)
// 功能描述：Phase 4 新类，独立于 PcieEndpointTLM (4 端口冻结布局)。
//           - 17 端口: port[0]=PF, port[1..16]=VF0..VF15 (Phase 2 起由 composite 持有)
//           - 内置 PcieSriovVfPool (per-VF Config Space / MSI-X / FC / seq#)
//           - 内置 CompletionTracker (NP↔CplD trans_id 关联, per Q12)
//           - Phase 2: 基类 SimModule; 17 端口 + LL/PHY/Mux 归 internal_factory 内的
//             PcieLinkPhyMuxTLM composite 持有 (单一所有权), EP 不再持 adapter 数组
//           - FLR: flr_pf() 全复位 / flr_vf(vfx) 仅对应 VF
// 作者 CppTLM Team / 日期 2026-10-13 (Phase 2 基类切换 2026-09-16)
// 参考: openspec/changes/2026-10-13-cpptlm-dgpu-pcie-sriov-vf-pool/proposal.md T-P4-7
//       openspec/changes/2026-09-15-cpptlm-pcie-endpoint-ip-simmodule-refactor/design.md §3
// ⚠️ 不修改 include/abi/cpptlm_emulator.h（23 ABI 冻结边界，AD-088）
// ⚠️ 不修改 include/tlm/gpu/pcie_endpoint_tlm.h（PcieEndpointTLM 4 端口冻结）
#ifndef CPPTLM_PCIE_ENDPOINT_ABI_VERSION
#define CPPTLM_PCIE_ENDPOINT_ABI_VERSION 2  // v2 = 链路层 + PHY 数字 + SR-IOV
#endif

#ifndef TLM_PCIE_PCIE_ENDPOINT_IP_HH
#define TLM_PCIE_PCIE_ENDPOINT_IP_HH

#include "core/sim_module.hh"
#include "core/sim_object.hh"
#include "tlm/pcie/pcie_completion_tracker_tlm.hh"
#include "tlm/pcie/pcie_link_phy_mux_tlm.hh"
#include "tlm/pcie/pcie_resizable_bar.hh"
#include "tlm/pcie/pcie_sriov_vf_pool_tlm.hh"

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tlm::pcie {

/**
 * @brief PcieEndpointIP：SR-IOV PCIe Endpoint IP（17 端口）
 *
 * 与 PcieEndpointTLM 的差异：
 *   - 端口数 17（0=PF, 1..16=VF0..VF15）vs 4（冻结）
 *   - per-VF Config Space / MSI-X / FC / seq# 独立（PcieSriovVfPool）
 *   - Completion tracking（trans_id 关联, per Q12）
 *   - FLR: flr_pf() 全复位 / flr_vf() 仅对应 VF
 *
 * Phase 2 SimModule 派生：17 端口 + LL/PHY/Mux 由 internal_factory 内的
 * PcieLinkPhyMuxTLM composite 持有（单一所有权），EP::tick() 显式定序调用。
 */
class PcieEndpointIP : public SimModule {
public:
    static constexpr unsigned NUM_PORTS = 17;  // 0=PF, 1..16=VF0..VF15

    PcieEndpointIP(const std::string& name, EventQueue* eq);
    ~PcieEndpointIP() override;

    PcieEndpointIP(const PcieEndpointIP&) = delete;
    PcieEndpointIP& operator=(const PcieEndpointIP&) = delete;
    PcieEndpointIP(PcieEndpointIP&&) = delete;
    PcieEndpointIP& operator=(PcieEndpointIP&&) = delete;

    std::string get_module_type() const override {
        return "PcieEndpointIP";
    }

    // Phase 2: 构造期入口 (R4 决策 C — 不做外层 JSON TLP 接线)
    void simulate_instantiate(const json& cfg) override;

    void init() override;
    // INV-3: 显式定序 (AXI slave → composite 子模块 tick), 禁依赖 SimModule 默认迭代
    void tick() override;
    void do_reset(const ResetConfig&) override;
    void on_config_loaded() override;

    // Phase 2 static 辅助: EP 实例列表 (ctor/dtor 维护) + composite 查找
    static std::vector<PcieEndpointIP*>& instances_for_test() noexcept;
    static PcieLinkPhyMuxTLM* find_composite(const std::string& ep_name) noexcept;

    PcieSriovVfPool& vf_pool() noexcept { return pool_; }
    const PcieSriovVfPool& vf_pool() const noexcept { return pool_; }
    // completion 状态单一真源在 PcieSriovVfPool（生产路径 dispatch_tlp 登记到
    // pool_.completions()）；此处委托而非另持副本，避免双份 outstanding 失配。
    CompletionTracker& completions() noexcept { return pool_.completions(); }
    const CompletionTracker& completions() const noexcept { return pool_.completions(); }

    tlm::gpu::PcieConfigSpace& config_of(uint16_t stream_id) {
        return pool_.config_of(stream_id);
    }
    tlm::gpu::MsiXTable& msix_of(uint16_t stream_id) {
        return pool_.msix_of(stream_id);
    }

    void flr_pf() noexcept;
    void flr_vf(uint16_t vf_id) noexcept;

    // Stage 1.4-followups §4: PM Cap 安装 (init_all 后重装, init() 会 wipe)
    void install_pm_capability();

    // Stage 1.4-followups §3: ResizableBar 集成 (6 BAR slots)
    tlm::pcie::ResizableBar& resizable_bar(unsigned bar_idx) noexcept;
    // enable wrapper: enable 成功后触发 on_bar_resize (INV-G 越界校验)
    bool enable_resizable_bar(unsigned bar_idx) noexcept;

    // Stage 1.4 §1.3: Power state machine (INV-A MMIO gating)
    enum class PciePowerState : uint8_t { D0 = 0, D3hot = 3 };
    void set_power_state(PciePowerState s) noexcept;
    [[nodiscard]] PciePowerState power_state() const noexcept {
        return power_state_;
    }
    [[nodiscard]] bool mmio_gated() const noexcept { return mmio_gated_; }

    // 测试 helper: 读取 BAR backing store 验证 MMIO gate 是否阻止写入
    [[nodiscard]] uint64_t bar_store_value(uint64_t key) const noexcept {
        const auto it = bar_store_.find(key);
        return (it != bar_store_.end()) ? it->second : 0;
    }

    // Phase 2: 访问器转发到 internal_factory 内的 composite (inactive 时返回 nullptr)
    PcieLinkLayer* link_layer() const noexcept;
    PciePhyDigitalCtrl* phy() const noexcept;
    PcieBypassMux* bypass_mux() const noexcept;

    // PcieEndpointIP JSON 配置扩展 (Phase A1) — 未消费键 warning 列表
    // 配合 attach_composition 内的 std::cerr 同步输出 (per design.md §4)
    // 不抛异常, 不致命; 测试可通过 getter 断言 (Catch2 无法可移植捕获 stderr)
    const std::vector<std::string>& config_warnings() const noexcept {
        return config_warnings_;
    }

    // Stage 1.3a: SDMA Ring Buffer Doorbell 路由 (per openspec/.../2026-09-10-...)
    //   BAR1+0x10010000 offset 写入 WPTR → SDMA ring consumption trigger
    //   其他 BAR/offset → bar_store_ (无 ring 触发)
    static constexpr uint64_t kBar1DoorbellOffset = 0x10010000ULL;
    static constexpr uint64_t bar1_doorbell_offset() noexcept {
        return kBar1DoorbellOffset;
    }

    // UE 进程 ABI cpptlm_emulator_mmio_write 路径 (per Phase 8 ABI 表)
    //   返回 true = write accepted (无论 ring 触发与否)
    //   当 (bar==1 && offset==0x10010000) → sdma_ring_processed_count_ += data (WPTR)
    bool mmio_write(uint32_t bar, uint64_t offset, uint64_t data);

    // Stage 1.3c: GART/IOMMU 翻译模式开关 (per openspec/.../2026-09-10-... §1.3c)
    //   - C++ 端不实施 4 级页表 walk (UE 进程实现, 通过 dma_translate_cb 接口注入)
    //   - 这里只暴露 mode 开关 + iommu cb 转发路径
    //   - 默认 Mode::IDENTITY → cb(iova) 直接返 pa=iova (identity)
    //   - Mode::IOMMU → cb 内部走 4 级页表 walk (UE 实现)
    enum class DmaTranslateMode : uint8_t {
        IDENTITY = 0,  // pa = iova (Stage 1.3c 默认)
        IOMMU = 1,     // 4 级页表 walk (UE 进程实现)
    };
    void set_dma_translate_mode(DmaTranslateMode m) noexcept {
        dma_translate_mode_ = m;
    }
    DmaTranslateMode dma_translate_mode() const noexcept {
        return dma_translate_mode_;
    }

    // SDMA ring 已处理的 entry 数 (累计 WPTR, 测试断言用)
    uint32_t sdma_ring_processed_count() const noexcept {
        return sdma_ring_processed_count_;
    }

private:
    PcieSriovVfPool pool_;
    // BAR 空间 backing store（Phase 8 M1: AXI slave 写经地址路由落写/读回真实值）
    std::unordered_map<uint64_t, uint64_t> bar_store_;
    // Stage 1.4-followups §3: 6 个 ResizableBar (对应 6 个 BAR slots)
    std::array<tlm::pcie::ResizableBar, 6> resizable_bars_;
    void on_bar_resize(unsigned bar_idx);  // enable() 后触发, INV-G 越界校验
    // Stage 1.3a: SDMA ring 已处理的 entry 数 (累计 WPTR via doorbell writes)
    uint32_t sdma_ring_processed_count_ = 0;
    // Stage 1.3c: DMA 翻译模式 (identity / IOMMU)
    DmaTranslateMode dma_translate_mode_ = DmaTranslateMode::IDENTITY;
    // Stage 1.4 §1.3: Power state (D0/D3hot) + INV-A MMIO gate
    PciePowerState power_state_ = PciePowerState::D0;
    bool mmio_gated_ = false;
    // Phase 2: composite 单实例 owned by internal_factory; 本 flag 表达
    // "link_layer.enabled" 的可见性语义 (disabled 时 for_endpoint→nullptr)
    bool composite_active_ = false;
    std::string composite_name() const;
    PcieLinkPhyMuxTLM* composite() const noexcept;
    void ensure_composite_instantiated(const nlohmann::json& params);
    void attach_composition(const nlohmann::json& params);

    // PcieEndpointIP JSON 配置扩展 (Phase A1) — 未消费键 warning 收集
    // 配合 std::cerr 同步输出 (per design.md §4.1 + LINT005 precedent)
    std::vector<std::string> config_warnings_;
    void warn_unconsumed(const nlohmann::json& params,
                         const std::vector<std::string>& known);
    void warn_unconsumed_subkeys(const nlohmann::json& group,
                                 const std::string& group_name,
                                 const std::vector<std::string>& known_subkeys);
};

} // namespace tlm::pcie

#endif // TLM_PCIE_PCIE_ENDPOINT_IP_HH
