// src/tlm/pcie/pcie_endpoint_ip.cc
// PcieEndpointIP 实现 (T-P4-7)
// 作者 CppTLM Team / 日期 2026-10-13
#include "tlm/pcie/pcie_endpoint_ip.hh"
#include "tlm/gpu/pcie_config_space_mvp.hh"
#include "tlm/pcie/pcie_config_space_per_vf_tlm.hh"
#include "tlm/pcie/pcie_sriov_vf_pool_tlm.hh"
#include "tlm/pcie/pcie_msix_per_vf_tlm.hh"
#include "tlm/pcie/pcie_ari_router_tlm.hh"
#include "tlm/pcie/pcie_phy_digital_ctrl_tlm.hh"
#include "tlm/pcie/pcie_axi_adapter_tlm.hh"
#include "tlm/pcie/pcie_link_phy_mux_tlm.hh"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace tlm::pcie {

    PcieEndpointIP::PcieEndpointIP(const std::string& name, EventQueue* eq)
        : SimModule(name, eq) {
        pool_.init_all();
        install_pm_capability();
        install_capabilities();
        instances_for_test().push_back(this);
    }

    void PcieEndpointIP::init() {
        SimObject::init();
        pool_.init_all();
        install_pm_capability();
        install_capabilities();
    }

    void PcieEndpointIP::do_reset(const ResetConfig&) {
        pool_.init_all();
        install_pm_capability();
        install_capabilities();
    }

    void PcieEndpointIP::install_pm_capability() {
        // Stage 1.4 §1.1: 在 PF slot (0) 安装 PM Cap (id=0x01) + PMCSR 写回调
        // 让 driver 通过 CFG_WRITE 触发 INV-A 状态机 + MMIO gating
        // Stage 1.4-followups §6: PWS=1/2 (D1/D2 保留值) 忽略 — PCI PM spec
        // 不支持状态写入应保持当前态, 避免非法枚举值
        auto& cfg_pf = pool_.config_pool().config_of(0);
        cfg_pf.add_capability(0x01, 0x40, /*next=*/0x00, /*control=*/0x0013);
        cfg_pf.set_pmcsr_write_cb([this](uint16_t new_pws) {
            if (new_pws != 0 && new_pws != 3) {
                return;  // D1/D2 保留值: 忽略, 保持当前状态
            }
            set_power_state(static_cast<PciePowerState>(new_pws));
        });
    }

    void PcieEndpointIP::install_lnkctl_callback() {
        auto& cfg_pf = pool_.config_pool().config_of(0);
        cfg_pf.set_lnkctl_write_cb([this](uint16_t new_lnkctl) {
            // PCI-SIG LNKCTL ASPM bits[1:0]: 01=L0s, 10=L1
            if (new_lnkctl & 0x0001) {
                if (auto* p = phy()) p->enable_aspm(::tlm::pcie::PciePhyDigitalCtrl::AspmLevel::L0s);
            }
            if (new_lnkctl & 0x0002) {
                if (auto* p = phy()) p->enable_aspm(::tlm::pcie::PciePhyDigitalCtrl::AspmLevel::L1);
            }
        });
    }

    void PcieEndpointIP::install_capabilities() {
        auto& cfg_pf = pool_.config_pool().config_of(0);

        // A-4: PCIe Cap (0x10) @0x50, control = 0x0002 (PCIe Cap v2)
        cfg_pf.add_capability(0x10, 0x50, /*next=*/0x00, /*control=*/0x0002);

        // A-4: LNKCTL @0x60 (cap+0x10), LNKSTA @0x62 (cap+0x12)
        // LNKCTL default = 0x0010 (per PCI-SIG spec; bit 4 = Link Active)
        cfg_pf.add_extended_register(0x60, 2, 0x0010);
        cfg_pf.add_extended_register(0x62, 2, 0x0000);

        // A-5: ACS Ext Cap (id=0x000D) @0x100
        cfg_pf.add_extended_capability(0x000D, 0x100, /*next=*/0x140);

        // A-5: ReBAR Ext Cap (id=0x0015, per PCI-SIG) @0x140
        cfg_pf.add_extended_capability(0x0015, 0x140, /*next=*/0x00);

        // A-5: ReBAR Cap header @0x140 + Control @0x148
        // Header per PCI-SIG: bits[15:0]=id(0x0015), bits[19:16]=version(1), bits[31:20]=next(0)
        // → 0x00010015
        cfg_pf.add_extended_register(0x140, 4, 0x00010015);
        // Control: BAR index bits[3:0]=0, BAR Size bits[12:8]=8 (256MB, per log2(bytes)-20),
        //          Number of Resizable BARs bits[7:4]=1
        // → 0x00080810
        cfg_pf.add_extended_register(0x148, 4, 0x00080810);

        install_lnkctl_callback();
    }

    void PcieEndpointIP::simulate_instantiate(const json& cfg) {
        // 决策 3: 双格式 — Module entry {"name","type","params"} (Step 4.5) 或直接 params
        const json params = cfg.contains("params") ? cfg["params"] : cfg;
        if (internal_factory && internal_factory->getAllInstances().empty()) {
            // 惰性构造 composite child (仅 enabled=true, 决策 4)
            ensure_composite_instantiated(params);
        }
        // 始终消费 (幂等): 支撑 reconfigure (R-B) 与 Step 2 双调 on_config_loaded
        attach_composition(params);
    }

    void PcieEndpointIP::on_config_loaded() {
        // 决策 2: 直接构造路径 (tests / demo ep.set_config(cfg)) 归一到 simulate_instantiate
        simulate_instantiate(get_config()); // SimObject::get_config()
    }

    PcieEndpointIP::~PcieEndpointIP() {
        composite_active_ = false;
        auto& v = instances_for_test();
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }

    void PcieEndpointIP::attach_composition(const nlohmann::json& params) {
        // 重入清空, 避免重复 config load 累积陈旧 warning
        config_warnings_.clear();
        // Stage 1.4-followups §4: PM Cap control word JSON-driven
        // 走已有 update_capability_control (构造器已安装 cap; 勘误:
        // 不移入 attach_composition 避免 init_all wipe + 重复 add_capability)
        if (params.contains("pm_cap_control")) {
            const auto ctrl = params.value("pm_cap_control", static_cast<uint16_t>(0x0013));
            pool_.config_pool().config_of(0).update_capability_control(0, ctrl);
        }
        // AXI Stream Adapter 独立挂接（per Phase 5 T-P5-6, 不依赖 link_layer 分支）
        if (params.contains("axi_adapter")) {
            auto* ax = PcieAxiAdapter::attach_to_endpoint(getName(), event_queue);
            if (ax) {
                ax->set_endpoint(this);
                const auto& axi_json = params["axi_adapter"];
                const bool mapper_inject = axi_json.value("axi4_mapper_inject", false);
                ax->set_mapper_injected(mapper_inject);
            }
        } else {
            PcieAxiAdapter::detach_from_endpoint(getName());
        }

        // link_layer 块: 决定 composite (LL+PHY+Mux) 是否构造 (Phase 2 单一所有权)
        // 惰性构造: 仅 enabled=true 时经 internal_factory 构造, enabled=false 时
        // composite_active_=false (for_endpoint→nullptr 语义保持, Oracle 复审条件 2 + R-B)
        if (params.contains("link_layer")) {
            const auto& ll_json = params["link_layer"];
            const bool enabled = ll_json.value("enabled", true);
            if (!enabled) {
                composite_active_ = false;
                PcieLinkLayer::detach_from_endpoint(getName());
                PciePhyDigitalCtrl::detach_from_endpoint(getName());
                PcieBypassMux::detach_from_endpoint(getName());
            } else {
                ensure_composite_instantiated(params);
                composite_active_ = true;
                if (auto* lpm = this->composite()) {
                    lpm->link().set_fc_capacity(
                        ll_json.value("fc_token_bucket_capacity", 256u));
                    lpm->link().set_retry_buffer_size(
                        ll_json.value("retry_buffer_size", 4096u));
                    lpm->link().set_link_error_injection_enabled(
                        ll_json.value("link_error_injection_enabled", false));
                    if (ll_json.contains("bypass_mode")) {
                        const std::string bypass_mode =
                            ll_json["bypass_mode"].get<std::string>();
                        if (bypass_mode == "Bypass") {
                            lpm->mux().apply_mode(BypassMode::Bypass);
                        } else if (bypass_mode == "Partial") {
                            lpm->mux().apply_mode(BypassMode::Partial);
                        } else {
                            lpm->mux().apply_mode(BypassMode::Full);
                        }
                    }
                    lpm->link().set_fc_initial_credits(
                        ll_json.value("fc_initial_credit_p", 256u),
                        ll_json.value("fc_initial_credit_np", 256u),
                        ll_json.value("fc_initial_credit_cpl", 256u));
                }
            }
        }

        // 不变量 INV-2: phy->set_config() 整结构覆盖, 必须 read-modify-write
        // (保留现有 max_speed / max_lanes / sr_iov_vf_pool_size)
        if (params.contains("phy_digital")) {
            if (auto* lpm = this->composite()) {
                const auto& pj = params["phy_digital"];
                auto cfg = lpm->phy().config();
                if (pj.contains("preset_p")) {
                    cfg.preset_P = pj.value("preset_p", cfg.preset_P);
                }
                if (pj.contains("preset_np")) {
                    cfg.preset_NP = pj.value("preset_np", cfg.preset_NP);
                }
                if (pj.contains("preset_cpl")) {
                    cfg.preset_Cpl = pj.value("preset_cpl", cfg.preset_Cpl);
                }
                if (pj.contains("hot_plug_supported")) {
                    cfg.hot_plug_supported =
                        pj.value("hot_plug_supported", cfg.hot_plug_supported);
                }
                lpm->phy().set_config(cfg);
                warn_unconsumed_subkeys(pj, "phy_digital",
                    {"preset_p", "preset_np", "preset_cpl", "hot_plug_supported"});
            }
        }

        // 不变量 INV-1: configure_vectors 用 placement-new 销毁 pending IRQ;
        // 本函数仅 composition-time 可调用, 严禁在 tick 中重入
        if (params.contains("sr_iov")) {
            const auto& sio = params["sr_iov"];
            if (sio.contains("ari_capable")) {
                pool_.ari_router().set_ari_enabled(
                    sio.value("ari_capable", false));
            }
            if (sio.contains("vf_msix_vectors")) {
                const uint16_t n = sio.value("vf_msix_vectors", 4u);
                for (uint16_t vf = 1; vf < PcieMsixTablePerVf::NUM_SLOTS; ++vf) {
                    pool_.msix_pool().configure_vectors(vf, n);
                }
            }
            warn_unconsumed_subkeys(sio, "sr_iov",
                {"ari_capable", "vf_msix_vectors"});
        }

        if (params.contains("transaction_layer")) {
            const auto& tl = params["transaction_layer"];
            if (tl.contains("config_size")) {
                const std::size_t sz = tl.value("config_size", 4096u);
                const std::size_t applied = (sz == 256 || sz == 4096) ? sz : 4096;
                if (applied != sz) {
                    const std::string msg =
                        "transaction_layer.config_size must be 256 or 4096; got " +
                        std::to_string(sz) + ", falling back to 4096";
                    config_warnings_.push_back(msg);
                    std::cerr << "[CPPTLM-WARN] PcieEndpointIP::attach_composition: "
                              << msg << "\n";
                }
                pool_.config_pool().set_config_size_all(applied);
            }
            if (tl.contains("msix_num_vectors")) {
                const uint16_t n = tl.value("msix_num_vectors", 16u);
                pool_.msix_pool().configure_vectors(0, n);
            }
            warn_unconsumed_subkeys(tl, "transaction_layer",
                {"config_size", "msix_num_vectors"});
        }

        if (params.contains("bar0_registers") && params["bar0_registers"].is_array()) {
            bar_router_.init(1);
            for (const auto& reg_json : params["bar0_registers"]) {
                const uint32_t offset = reg_json.value("offset", 0u);
                const std::string name = reg_json.value("name", std::string{});
                const std::string access_str = reg_json.value("access", std::string{"rw"});
                const std::string side_str = reg_json.value("side_effect", std::string{"none"});

                tlm::gpu::PcieBarRouter::Access access = tlm::gpu::PcieBarRouter::Access::RW;
                if (access_str == "ro" || access_str == "RO") access = tlm::gpu::PcieBarRouter::Access::RO;
                else if (access_str == "wo" || access_str == "WO") access = tlm::gpu::PcieBarRouter::Access::WO;

                tlm::gpu::PcieBarRouter::SideEffect side = tlm::gpu::PcieBarRouter::SideEffect::NONE;
                if (side_str == "doorbell") side = tlm::gpu::PcieBarRouter::SideEffect::DOORBELL;

                const uint32_t stream_id = reg_json.value("stream_id", 0u);
                bar_router_.add_register(offset, name, access, side, stream_id);
            }
        }

        warn_unconsumed(params,
            {"axi_adapter", "link_layer", "phy_digital",
             "sr_iov", "transaction_layer", "bypass_mode", "pm_cap_control",
             "bar0_registers", "bar_sizes", "config_size", "num_msix_vectors"});
    }

    void PcieEndpointIP::warn_unconsumed(const nlohmann::json& params,
                                         const std::vector<std::string>& known) {
        std::unordered_set<std::string> known_set(known.begin(), known.end());
        for (auto it = params.begin(); it != params.end(); ++it) {
            if (known_set.count(it.key()) == 0) {
                const std::string msg =
                    "unrecognized JSON key '" + it.key() +
                    "' (no consumer; deferred or out-of-scope)";
                config_warnings_.push_back(msg);
                std::cerr << "[CPPTLM-WARN] PcieEndpointIP::attach_composition: "
                          << msg << "\n";
            }
        }
    }

    void PcieEndpointIP::warn_unconsumed_subkeys(
            const nlohmann::json& group,
            const std::string& group_name,
            const std::vector<std::string>& known_subkeys) {
        if (!group.is_object()) return;
        std::unordered_set<std::string> known(known_subkeys.begin(),
                                              known_subkeys.end());
        for (auto it = group.begin(); it != group.end(); ++it) {
            if (known.count(it.key()) == 0) {
                const std::string msg =
                    "unrecognized JSON subkey '" + it.key() +
                    "' in " + group_name;
                config_warnings_.push_back(msg);
                std::cerr << "[CPPTLM-WARN] PcieEndpointIP::attach_composition: "
                          << msg << "\n";
            }
        }
    }

    void PcieEndpointIP::tick() {
        // Phase 8 M1: 真实 AXI 数据路径接线 — PcieEndpointIP::tick() 驱动
        // PcieAxiAdapter 消费 slave_in 请求，EP 内部真实处理并产生真实响应。
        // HostBypass/RC (Host 侧 master) ↔ PcieAxiAdapter (EP 侧 slave) 双向闭环。
        // Stage 1.4-followups §1 INV-A 收窄: gate 移到 cfg/BAR 判别后,
        // 仅 BAR 路径 gate + DECERR 响应; cfg 路径保留 D3hot 访问 (PCIe spec,
        // INV-E: driver 可经 cfg 写 PMCSR 回 D0)
        if (auto* ax = PcieAxiAdapter::for_endpoint(getName())) {
            cpptlm::Axi4StreamAdapter& axi = ax->axi();

            if (axi.slave_req_valid()) {
                const bundles::Axi4Bundle& req = axi.slave_req_data();

                // 写/读判别: 使用 Axi4Bundle::is_write_request() 谓词 (per
                // include/bundles/axi4_bundles_tlm.hh:86-88)。该谓词判定"是否
                // 写请求",替代原有启发式 (awid!=0||awaddr!=0||awlen!=0) 三字段判别。
                // 启发式在 awid=0,awaddr=0,awlen=0 时会把任何含 wlast=1,wdata!=0
                // 的合法写请求误判为读,丢失写数据。
                //
                // 注: 当前 Axi4StreamAdapter 把 AW/W 打包为一个 Axi4Bundle 投递,
                // AR 通道在另一拍投递。所以"is_write_request"用 awlen/awid/awaddr
                // 任一非 0 作为写请求信号,语义与启发式一致但更稳健(未来如需扩展
                // 流式接口,只需在此谓词点扩展,不涉及消费者)。
                if (req.is_write_request()) {
                    // 写请求: 配置空间偏移 (< config_size) vs BAR 空间
                    // PCIe 规范 (cfg 路径):
                    //   - awaddr 低 2 bit [1:0] 为对齐保留位,请求方保证 = 00
                    //   - dword offset = awaddr >> 2,byte offset = (awaddr >> 2) << 2
                    // 范围判定用原始 awaddr (无屏蔽) 以正确区分 cfg vs BAR 空间。
                    const uint64_t awaddr = req.awaddr.read();
                    const uint16_t bid = static_cast<uint16_t>(req.awid.read());

                    const bool is_cfg = awaddr < pool_.config_of(0).config_size();

                    // INV-A 收窄: 仅 BAR 路径 gate; cfg 路径保留 D3hot 访问 (PCIe spec)
                    // 响应 DECERR (bresp=3; 2=SLVERR) 而非静默丢弃 (避免 host 挂起)
                    if (mmio_gated_ && !is_cfg) {
                        bundles::Axi4Bundle err_resp;
                        err_resp.bid.write(bid);
                        err_resp.bresp.write(3u);  // DECERR
                        axi.slave_resp(err_resp);
                        axi.slave_req_consume();
                        return;
                    }

                    if (is_cfg) {
                        // cfg 路径: 屏蔽低 2 bit 后右移得到 byte offset
                        const uint16_t cfg_byte_off = static_cast<uint16_t>(awaddr & ~0x3ULL);
                        pool_.config_of(0).write(cfg_byte_off,
                                                 static_cast<uint32_t>(req.wdata.read()));
                    } else {
                        // BAR 空间: 4B 粒度 key + 按 wstrb 字节 mask 部分写
                        // (per Oracle P1-2 报告,原 8B 对齐 key + 忽略 wstrb 建模失真)
                        //
                        // BAR slot 模型: 32-bit 寄存器(对应真实 PCIe BAR 4B 寄存器),
                        // wdata 低 32 bit + wstrb 低 4 bit mask 字节粒度。
                        // ch_uint<512>::wdata/wstrb 实际是 64-bit 存储,仅用低 32/4 bit。
                        //
                        // wstrb 字节语义 (per AXI/PCIe 规范):
                        //   wstrb[i]=1 表示 byte[i] 有效 (要写)
                        //   例: wstrb=0xE = 0b1110 → byte[0]不写, byte[1,2,3]写
                        //       字节 mask = 0xFFFFFF00 (byte[0]=0x00, byte[1..3]=0xFF)
                        const uint64_t key = awaddr & ~0x3ULL;
                        const uint32_t wdata32 = static_cast<uint32_t>(req.wdata.read());
                        const uint32_t wstrb4 = static_cast<uint32_t>(req.wstrb.read() & 0xFULL);
                        uint32_t byte_mask = 0;
                        for (unsigned i = 0; i < 4; ++i) {
                            if (wstrb4 & (1u << i)) {
                                byte_mask |= (0xFFu << (i * 8));
                            }
                        }
                        const uint32_t old_val =
                            static_cast<uint32_t>(bar_store_[BarStoreKey{bdf_, 0, key}]); // 默认 0 if missing
                        const uint32_t new_val = (old_val & ~byte_mask) | (wdata32 & byte_mask);
                        bar_store_[BarStoreKey{bdf_, 0, key}] = new_val;
                    }

                    bundles::Axi4Bundle wresp;
                    wresp.bid.write(bid);
                    wresp.bresp.write(0);
                    axi.slave_resp(wresp);
                } else {
                    // 读请求: 配置空间偏移 (< config_size) vs BAR 空间
                    // PCIe 规范解码同上
                    const uint64_t araddr = req.araddr.read();
                    const uint16_t rid = static_cast<uint16_t>(req.arid.read());
                    uint64_t rdata = 0;

                    const bool is_cfg = araddr < pool_.config_of(0).config_size();

                    if (is_cfg) {
                        const uint16_t cfg_byte_off = static_cast<uint16_t>(araddr & ~0x3ULL);
                        rdata = pool_.config_of(0).read(cfg_byte_off);
                    } else {
                        // BAR 空间: 4B 粒度 key 读取
                        const uint64_t key = araddr & ~0x3ULL;
                        const auto it = bar_store_.find(BarStoreKey{bdf_, 0, key});
                        if (it != bar_store_.end()) {
                            // BAR slot 是 32-bit 寄存器,高 32 bit 总是 0
                            rdata = static_cast<uint32_t>(it->second);
                        }
                    }

                    bundles::Axi4Bundle rresp;
                    rresp.rid.write(rid);
                    rresp.rdata.write(rdata);
                    rresp.rresp.write(0);
                    rresp.rlast.write(1);
                    axi.slave_resp(rresp);
                }
                axi.slave_req_consume();
            }

            // 注意：此处不调用 axi.tick()。slave_resp 通道由 HostBypass/RootComplex
            // 在桥接转发完成后调用 ep_axi.tick() 推进（避免响应被同周期 self-clear，
            // 保证 HostBypass/RC tick 能读到 EP 产生的真实响应）。Phase 8 M1 修复。
        }

        // Phase 2 (INV-3 显式定序): AXI slave 处理 → composite 子模块 tick
        // composite::tick() 内部定序 phy → link → 17 adapters (Oracle R3 顺序契约)
        // [Phase 2 行为变更]: PHY tick 驱动权从测试外部迁移到 EP 此处 (tasks 2.1.5)
        if (auto* lpm = this->composite()) {
            lpm->tick();
        } else if (auto* ll = PcieLinkLayer::for_endpoint(getName())) {
            // legacy fallback: composite inactive 时保持 LL tick (frozen EP 路径兼容)
            ll->tick();
        }
    }

    void PcieEndpointIP::set_power_state(PciePowerState s) noexcept {
        power_state_ = s;
        mmio_gated_ = (s == PciePowerState::D3hot);
    }

    void PcieEndpointIP::flr_pf() noexcept {
        pool_.flr_pf();
    }

    void PcieEndpointIP::flr_vf(uint16_t vf_id) noexcept {
        pool_.flr_vf(vf_id);
    }

    std::string PcieEndpointIP::composite_name() const {
        return getName() + "_lpm";
    }

    PcieLinkPhyMuxTLM* PcieEndpointIP::composite() const noexcept {
        if (!composite_active_ || !internal_factory) {
            return nullptr;
        }
        return dynamic_cast<PcieLinkPhyMuxTLM*>(
            internal_factory->getInstance(composite_name()));
    }

    void PcieEndpointIP::ensure_composite_instantiated(const nlohmann::json& params) {
        if (!params.contains("link_layer") ||
            !params["link_layer"].value("enabled", true)) {
            return;  // 惰性构造: enabled=true 才建 (for_endpoint→nullptr 语义保持)
        }
        if (!internal_factory || !internal_factory->getAllInstances().empty()) {
            composite_active_ = true;
            return;
        }
        nlohmann::json wrap;
        wrap["modules"] = nlohmann::json::array();
        wrap["connections"] = nlohmann::json::array();
        nlohmann::json composite_cfg;
        // 17 端口 multiport adapter 由 ChStreamAdapterFactory 注册 (chstream_register.hh)
        composite_cfg["name"] = composite_name();
        composite_cfg["type"] = "PcieLinkPhyMuxTLM";
        composite_cfg["params"] = params.value("params", nlohmann::json::object());
        wrap["modules"].push_back(composite_cfg);
        internal_factory->instantiateAll(wrap);
        composite_active_ = true;
    }

    PcieLinkLayer* PcieEndpointIP::link_layer() const noexcept {
        // Phase 2 composite 优先 (internal_factory 单一所有权); legacy fallback 经 shim
        // 处理 PcieEndpointTLM 冻结路径(composite 名字不同, 自然 miss)
        if (auto* composite = this->composite()) {
            return &composite->link();
        }
        return PcieLinkLayer::for_endpoint(getName());
    }

    PciePhyDigitalCtrl* PcieEndpointIP::phy() const noexcept {
        if (auto* composite = this->composite()) {
            return &composite->phy();
        }
        return PciePhyDigitalCtrl::for_endpoint(getName());
    }

    PcieBypassMux* PcieEndpointIP::bypass_mux() const noexcept {
        if (auto* composite = this->composite()) {
            return &composite->mux();
        }
        return PcieBypassMux::for_endpoint(getName());
    }

    std::vector<PcieEndpointIP*>& PcieEndpointIP::instances_for_test() noexcept {
        static std::vector<PcieEndpointIP*> instances;
        return instances;
    }

    PcieLinkPhyMuxTLM* PcieEndpointIP::find_composite(const std::string& ep_name) noexcept {
        for (auto* ep : instances_for_test()) {
            if (!ep || ep->getName() != ep_name) {
                continue;
            }
            if (auto* composite = ep->composite()) {
                return composite;
            }
        }
        return nullptr;
    }

    // ========== Phase 2 非虚兼容方法: 17 端口 + adapter 转发到 composite (决策 1) ==========

    void PcieEndpointIP::set_stream_adapter(cpptlm::StreamAdapterBase* a) {
        if (auto* lpm = composite()) {
            lpm->set_stream_adapter(a);
        }
    }

    void PcieEndpointIP::set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
        if (!adapters) {
            return;
        }
        if (auto* lpm = composite()) {
            lpm->set_stream_adapter(adapters);
        }
    }

    bool PcieEndpointIP::all_ports_have_adapter() const {
        auto* lpm = composite();
        if (!lpm) {
            return false;
        }
        for (unsigned i = 0; i < NUM_PORTS; ++i) {
            if (!lpm->get_adapter(i)) {
                return false;
            }
        }
        return true;
    }

    cpptlm::StreamAdapterBase* PcieEndpointIP::get_adapter(unsigned idx) const {
        auto* lpm = composite();
        return lpm ? lpm->get_adapter(idx) : nullptr;
    }

    // Stage 1.3a: UE ABI cpptlm_emulator_mmio_write 入口 (per spec.md Scenario
    // "Doorbell write triggers" + openspec/changes/2026-09-10-...)
    //   - BAR1+0x10010000 → SDMA ring consumption (WPTR 累加)
    //   - 其他 BAR/offset → bar_store_ (无 ring 触发)
    // 返回 true: write accepted (无条件; test 不校验拒绝路径, MVP)
    bool PcieEndpointIP::mmio_write(uint32_t bar, uint64_t offset, uint64_t data) {
        if (bar == 1 && offset == kBar1DoorbellOffset) {
            // SDMA ring doorbell: data 是 WPTR 累加值
            // spec: "WPTR 写入 BAR1+0x10010000 → ring 消费 RPTR..WPTR"
            // 累加语义: 多次 doorbell 累加 (per WPTR 累计)
            sdma_ring_processed_count_ += static_cast<uint32_t>(data);
            return true;
        }
        // 其他 BAR 空间: 落 bar_store_ (与 AXI tick() 路径一致, 带 bar 维度)
        bar_store_[BarStoreKey{bdf_, static_cast<uint8_t>(bar), offset & ~0x3ULL}] = data;
        return true;
    }

    // Stage 1.4-followups §3: ResizableBar 集成 (6 BAR slots + INV-G)
    tlm::pcie::ResizableBar& PcieEndpointIP::resizable_bar(unsigned bar_idx) noexcept {
        return resizable_bars_[bar_idx % 6];
    }

    bool PcieEndpointIP::enable_resizable_bar(unsigned bar_idx) noexcept {
        if (!resizable_bars_[bar_idx % 6].enable()) {
            return false;
        }
        on_bar_resize(bar_idx % 6);
        return true;
    }

    void PcieEndpointIP::on_bar_resize(unsigned bar_idx) {
        const uint32_t size = resizable_bars_[bar_idx].size_bytes();
        // 清 bar_store_ 中与 (bdf_, bar_idx) 匹配且 addr >= size 的越界 entry
        // 使用 erase_all_for_bar 可复用, 但需推送警告, 因此内联循环
        for (auto it = bar_store_.begin(); it != bar_store_.end();) {
            const auto& [k_bdf, k_bar, k_addr] = it->first;
            if (k_bdf == bdf_ && k_bar == bar_idx && k_addr >= size) {
                config_warnings_.push_back(
                    "BAR" + std::to_string(bar_idx) +
                    " resized to " + std::to_string(size) +
                    ", key " + std::to_string(k_addr) + " out of bounds (cleared)");
                it = bar_store_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void PcieEndpointIP::erase_all_for_bar(uint16_t bdf, uint8_t bar, uint64_t new_size) noexcept {
        // 清除 (bdf, bar) 下 addr >= new_size 的越界 entry
        // 本 helper 无 warning 侧效; on_bar_resize 调用时自行推送 config_warnings_
        for (auto it = bar_store_.begin(); it != bar_store_.end();) {
            const auto& [k_bdf, k_bar, k_addr] = it->first;
            if (k_bdf == bdf && k_bar == bar && k_addr >= new_size) {
                it = bar_store_.erase(it);
            } else {
                ++it;
            }
        }
    }

} // namespace tlm::pcie
