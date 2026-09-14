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
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace tlm::pcie {

    PcieEndpointIP::PcieEndpointIP(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq) {
        pool_.init_all();
        // Stage 1.4 §1.1: 在 PF slot (0) 安装 PM Cap (id=0x01) + PMCSR 写回调
        // 让 driver 通过 CFG_WRITE 触发 INV-A 状态机 + MMIO gating
        auto& cfg_pf = pool_.config_pool().config_of(0);
        cfg_pf.add_capability(0x01, 0x40, /*next=*/0x00, /*control=*/0x0013);
        cfg_pf.set_pmcsr_write_cb([this](uint16_t new_pws) {
            set_power_state(static_cast<PciePowerState>(new_pws));
        });
    }

    void PcieEndpointIP::init() {
        ChStreamModuleBase::init();
        pool_.init_all();
    }

    void PcieEndpointIP::do_reset(const ResetConfig&) {
        pool_.init_all();
    }

    void PcieEndpointIP::set_stream_adapter(cpptlm::StreamAdapterBase* a) {
        if (a) {
            adapters_[0] = a;
        }
    }

    void PcieEndpointIP::set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
        if (!adapters) {
            return;
        }
        for (unsigned i = 0; i < NUM_PORTS; ++i) {
            adapters_[i] = adapters[i];
        }
    }

    bool PcieEndpointIP::all_ports_have_adapter() const {
        for (unsigned i = 0; i < NUM_PORTS; ++i) {
            if (!adapters_[i]) {
                return false;
            }
        }
        return true;
    }

    void PcieEndpointIP::on_config_loaded() {
        const auto& params = get_config(); // SimObject::get_config()
        attach_composition(params);
    }

    void PcieEndpointIP::attach_composition(const nlohmann::json& params) {
        // 重入清空, 避免重复 config load 累积陈旧 warning
        config_warnings_.clear();
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

        // link_layer 块: 决定 ll/phy/mux 是否 attach; 返回 phy 指针供后续块使用
        tlm::pcie::PciePhyDigitalCtrl* phy = nullptr;
        if (params.contains("link_layer")) {
            const auto& ll_json = params["link_layer"];
            const bool enabled = ll_json.value("enabled", true);
            if (!enabled) {
                PcieLinkLayer::detach_from_endpoint(getName());
                PciePhyDigitalCtrl::detach_from_endpoint(getName());
                PcieBypassMux::detach_from_endpoint(getName());
            } else {
                tlm::pcie::PcieLinkLayerConfig ll_cfg;
                ll_cfg.enabled = enabled;
                ll_cfg.fc_capacity = ll_json.value("fc_token_bucket_capacity", 256u);
                ll_cfg.fc_init_p = ll_json.value("fc_initial_credit_p", 256u);
                ll_cfg.fc_init_np = ll_json.value("fc_initial_credit_np", 256u);
                ll_cfg.fc_init_cpl = ll_json.value("fc_initial_credit_cpl", 256u);
                ll_cfg.retry_buffer_size = ll_json.value("retry_buffer_size", 4096u);

                auto* ll = PcieLinkLayer::attach_to_endpoint(getName(), event_queue, ll_cfg);
                phy = PciePhyDigitalCtrl::attach_to_endpoint(getName(), event_queue);
                if (phy) {
                    phy->link_layer(ll);
                    phy->set_link_up(true);
                }
                auto* mux = PcieBypassMux::attach_to_endpoint(getName(), ll);
                if (mux) {
                    mux->set_phy_initialized(phy != nullptr);
                    const std::string bypass_mode =
                        ll_json.value("bypass_mode", std::string("Full"));
                    if (bypass_mode == "Bypass") {
                        mux->apply_mode(BypassMode::Bypass);
                    } else if (bypass_mode == "Partial") {
                        mux->apply_mode(BypassMode::Partial);
                    }
                }
            }
        }

        // 不变量 INV-2: phy->set_config() 整结构覆盖, 必须 read-modify-write
        // (保留现有 max_speed / max_lanes / sr_iov_vf_pool_size)
        if (params.contains("phy_digital") && phy != nullptr) {
            const auto& pj = params["phy_digital"];
            auto cfg = phy->config();
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
            phy->set_config(cfg);
            warn_unconsumed_subkeys(pj, "phy_digital",
                {"preset_p", "preset_np", "preset_cpl", "hot_plug_supported"});
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

        warn_unconsumed(params,
            {"axi_adapter", "link_layer", "phy_digital",
             "sr_iov", "transaction_layer", "bypass_mode"});
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
                            static_cast<uint32_t>(bar_store_[key]); // 默认 0 if missing
                        const uint32_t new_val = (old_val & ~byte_mask) | (wdata32 & byte_mask);
                        bar_store_[key] = new_val;
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
                        const auto it = bar_store_.find(key);
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

        for (unsigned i = 0; i < NUM_PORTS; ++i) {
            if (adapters_[i]) {
                adapters_[i]->tick();
            }
        }
        if (auto* ll = PcieLinkLayer::for_endpoint(getName())) {
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

    PcieLinkLayer* PcieEndpointIP::link_layer() const noexcept {
        return PcieLinkLayer::for_endpoint(getName());
    }

    PciePhyDigitalCtrl* PcieEndpointIP::phy() const noexcept {
        return PciePhyDigitalCtrl::for_endpoint(getName());
    }

    PcieBypassMux* PcieEndpointIP::bypass_mux() const noexcept {
        return PcieBypassMux::for_endpoint(getName());
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
        // 其他 BAR 空间: 落 bar_store_ (与 AXI tick() 路径一致)
        bar_store_[offset & ~0x3ULL] = data;
        return true;
    }

} // namespace tlm::pcie
