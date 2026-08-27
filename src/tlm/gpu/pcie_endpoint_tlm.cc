// src/tlm/gpu/pcie_endpoint_tlm.cc
// PcieEndpointTLM 实现：4 端口 PCIe slave 模型
// 作者 CppTLM Team / 日期 2026-08-26

#include "tlm/gpu/pcie_endpoint_tlm.h"

#include "bundles/pcie_bundles_tlm.hh"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace tlm::gpu {

    namespace {
        // JSON → Access 转换
        PcieBarRouter::Access parse_access(const std::string& s) {
            if (s == "RO" || s == "ro")
                return PcieBarRouter::Access::RO;
            if (s == "WO" || s == "wo")
                return PcieBarRouter::Access::WO;
            return PcieBarRouter::Access::RW; // 默认
        }

        // JSON → SideEffect 转换
        PcieBarRouter::SideEffect parse_side_effect(const std::string& s) {
            if (s == "doorbell")
                return PcieBarRouter::SideEffect::DOORBELL;
            return PcieBarRouter::SideEffect::NONE;
        }
    } // namespace

    PcieEndpointTLM::PcieEndpointTLM(const std::string& name, EventQueue* eq)
        : ChStreamModuleBase(name, eq), cfg_space_(std::make_unique<PcieConfigSpace>()),
          bar_router_(std::make_unique<PcieBarRouter>()), msix_(std::make_unique<MsiXTable>()) {
    }

    PcieEndpointTLM::~PcieEndpointTLM() = default;

    void PcieEndpointTLM::init() {
        cfg_space_->init();
        bar_router_->init();
        msix_->init();
    }

    void PcieEndpointTLM::do_reset(const ResetConfig&) {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            req_in[i].reset();
            resp_out[i].reset();
        }
    }

    void PcieEndpointTLM::set_stream_adapter(cpptlm::StreamAdapterBase* a) {
        // 单端口回退（向后兼容）：仅处理 port 0
        adapters_[0] = a;
    }

    void PcieEndpointTLM::set_stream_adapter(cpptlm::StreamAdapterBase* adapters[]) {
        // 4 端口多端口注入（per spec.md Scenario "All 4 ports receive non-null StreamAdapter"）
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            adapters_[i] = adapters[i];
        }
    }

    bool PcieEndpointTLM::all_ports_have_adapter() const {
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            if (adapters_[i] == nullptr)
                return false;
        }
        return true;
    }

    void PcieEndpointTLM::on_config_loaded() {
        // params 从 sim_object set_config 注入
        const auto& cfg = get_config();
        if (!cfg.is_object())
            return;

        // config_size 参数化 PcieConfigSpace
        if (cfg.contains("config_size")) {
            const std::size_t sz = cfg.value("config_size", 4096u);
            if (sz != 256 && sz != 4096) {
                throw std::invalid_argument("PcieEndpointTLM: config_size must be 256 or 4096");
            }
            cfg_space_ = std::make_unique<PcieConfigSpace>(sz);
        }

        // msix_num_vectors 参数化 MsiXTable
        if (cfg.contains("msix_num_vectors")) {
            const uint16_t n = cfg.value("msix_num_vectors", 16);
            msix_ = std::make_unique<MsiXTable>(n);
        }

        // 重新初始化子组件
        cfg_space_->init();
        bar_router_->init();
        msix_->init();

        // 应用 capabilities（chain 声明）
        if (cfg.contains("capabilities")) {
            apply_capabilities_config(cfg);
        }

        // 应用 bar0_registers（数据化寄存器表）
        if (cfg.contains("bar0_registers")) {
            apply_bar0_registers_config(cfg);
        }
    }

    void PcieEndpointTLM::apply_capabilities_config(const nlohmann::json& cfg) {
        for (const auto& cap : cfg["capabilities"]) {
            const uint8_t id = cap.value("id", uint8_t{0});
            const uint8_t offset = cap.value("offset", uint8_t{0});
            const uint8_t next = cap.value("next", uint8_t{0});
            const uint16_t control = cap.value("control", uint16_t{0});
            if (!cfg_space_->add_capability(id, offset, next, control)) {
                // 失败时静默（per spec.md：capability 重叠不应 crash）
            }
        }
    }

    void PcieEndpointTLM::apply_bar0_registers_config(const nlohmann::json& cfg) {
        for (const auto& reg : cfg["bar0_registers"]) {
            const uint32_t offset = reg.value("offset", uint32_t{0});
            const std::string name = reg.value("name", std::string{"REG"});
            const std::string access_s = reg.value("access", std::string{"RW"});
            const std::string effect_s = reg.value("side_effect", std::string{"none"});
            const uint32_t stream_id = reg.value("stream_id", uint32_t{0});
            if (!bar_router_->add_register(offset, name, parse_access(access_s),
                                           parse_side_effect(effect_s), stream_id)) {
                // 失败静默（offset 越界/未对齐/重叠）
            }
        }
    }

    void PcieEndpointTLM::handle_slave_in_tlp() {
        if (!req_in[PORT_SLAVE_IN].valid())
            return;
        const auto& req = req_in[PORT_SLAVE_IN].data();
        const uint8_t kind = req.kind.read();

        bundles::PcieTlpBundle resp;
        resp.bar_index.write(req.bar_index.read());
        resp.offset.write(req.offset.read());
        resp.size.write(req.size.read());
        resp.requester_id.write(req.requester_id.read());
        resp.trans_id.write(req.trans_id.read());

        switch (kind) {
        case bundles::PcieTlpBundle::CFG_READ: {
            const uint16_t cfg_offset = static_cast<uint16_t>(req.offset.read());
            resp.data.write(cfg_space_->read(cfg_offset));
            resp.kind.write(bundles::PcieTlpBundle::CFG_READ);
            resp_out[PORT_MMIO_OUT].write(resp);
            break;
        }
        case bundles::PcieTlpBundle::CFG_WRITE: {
            const uint16_t cfg_offset = static_cast<uint16_t>(req.offset.read());
            cfg_space_->write(cfg_offset, static_cast<uint32_t>(req.data.read()));
            resp.kind.write(bundles::PcieTlpBundle::CFG_WRITE);
            resp_out[PORT_MMIO_OUT].write(resp);
            break;
        }
        case bundles::PcieTlpBundle::MMIO_READ: {
            const uint32_t off = static_cast<uint32_t>(req.offset.read());
            resp.data.write(bar_router_->mmio_read(off));
            resp.kind.write(bundles::PcieTlpBundle::MMIO_READ);
            resp_out[PORT_MMIO_OUT].write(resp);
            break;
        }
        case bundles::PcieTlpBundle::MMIO_WRITE: {
            const uint32_t off = static_cast<uint32_t>(req.offset.read());
            const uint32_t val = static_cast<uint32_t>(req.data.read());
            bar_router_->mmio_write(off, val, req.trans_id.read());
            resp.kind.write(bundles::PcieTlpBundle::MMIO_WRITE);
            resp_out[PORT_MMIO_OUT].write(resp);
            break;
        }
        case bundles::PcieTlpBundle::MEM_READ:
        case bundles::PcieTlpBundle::MEM_WRITE: {
            // BAR1 MEM 转发：descriptor-only, size > 8 时 data=0 (per design.md §2.3)
            if (req.size.read() > 8) {
                resp.data.write(0);
            } else {
                // size <= 8: 内联 data 直接转发
                resp.data.write(req.data.read());
            }
            resp.kind.write(kind);
            resp_out[PORT_MEM_OUT].write(resp);
            break;
        }
        default:
            break; // 未知 kind 静默丢弃
        }

        req_in[PORT_SLAVE_IN].consume();
    }

    void PcieEndpointTLM::tick() {
        // 1. 处理 slave_in 入口
        handle_slave_in_tlp();

        // 2. 推进 bar_router_ 周期（完成 doorbell 强序写）
        bar_router_->tick();

        // 3. mmio_out 端口：把 bar_router_ 中到期的 doorbell 事件 consume
        // (resp_out[PORT_MMIO_OUT] 已经在 handle_slave_in_tlp 中写入了响应,
        //  这里只负责清空 doorbell 副作用队列)
        while (bar_router_->try_pop_doorbell_out()) {
            bar_router_->consume_doorbell_out();
        }

        // 4. irq_out 端口：从 MsiXTable pending 拉取并写出（PcieTlpBundle{IRQ_DELIVERY}）
        while (const auto* evt = msix_->try_pop_irq_out()) {
            bundles::PcieTlpBundle irq_tlp;
            irq_tlp.kind.write(bundles::PcieTlpBundle::IRQ_DELIVERY);
            irq_tlp.offset.write(static_cast<uint64_t>(evt->vector)); // vector 编码到 offset
            irq_tlp.size.write(evt->msg_data);                        // msg_data 编码到 size
            irq_tlp.data.write(evt->msg_addr);                        // msg_addr 编码到 data
            irq_tlp.trans_id.write(evt->trans_id);
            resp_out[PORT_IRQ_OUT].write(irq_tlp);
            msix_->consume_irq_out();
        }

        // 5. 调用各 adapter 的 tick()（如需要）
        for (unsigned i = 0; i < NUM_PORTS; i++) {
            if (adapters_[i])
                adapters_[i]->tick();
        }
    }

} // namespace tlm::gpu