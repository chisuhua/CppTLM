// src/tlm/pcie/pcie_mock_ip.cc
// PcieMockIP 实现
// 作者 CppTLM Team / 日期 2027-09-17
#include "tlm/pcie/pcie_mock_ip.hh"

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_set>

namespace cpptlm::pcie {

// ==================== attach_composition ====================
// 从 JSON 加载 BAR 配置（复用 PcieBarRouter 数据驱动机制）
// 支持字段: bar0_registers, bar_sizes, msix_table_size
void PcieMockIP::attach_composition(const nlohmann::json& cfg) {
    // 重置状态 (幂等)
    bar_regs_.clear();
    bar_sizes_.fill(0);
    is_mmio_gated_ = false;
    intr_cb_ = nullptr;
    msix_pending_.clear();
    axi_adapter_.reset();

    // 解析 bar_sizes（6 个 BAR slots）
    if (cfg.contains("bar_sizes") && cfg["bar_sizes"].is_array()) {
        const auto& sizes = cfg["bar_sizes"];
        for (std::size_t i = 0; i < 6 && i < sizes.size(); ++i) {
            bar_sizes_[i] = sizes[i].get<uint64_t>();
        }
    }

    // bar_sizes 可能是对象 {bar0: ..., bar1: ...}
    if (cfg.contains("bar_sizes") && cfg["bar_sizes"].is_object()) {
        for (const auto& [key, val] : cfg["bar_sizes"].items()) {
            if (key.size() == 4 && key.rfind("bar", 0) == 0) {
                const int idx = std::stoi(key.substr(3));
                if (idx >= 0 && idx < 6) {
                    bar_sizes_[idx] = val.get<uint64_t>();
                }
            }
        }
    }

    // 解析 bar0_registers（复用 PcieBarRouter 数据驱动机制）
    if (cfg.contains("bar0_registers") && cfg["bar0_registers"].is_array()) {
        bar_router_.init(1);
        for (const auto& reg_json : cfg["bar0_registers"]) {
            const uint32_t offset = reg_json.value("offset", 0u);
            const std::string name = reg_json.value("name", std::string{});
            const std::string access_str = reg_json.value("access", std::string{"rw"});
            const std::string side_str = reg_json.value("side_effect", std::string{"none"});

            auto access = tlm::gpu::PcieBarRouter::Access::RW;
            if (access_str == "ro" || access_str == "RO") {
                access = tlm::gpu::PcieBarRouter::Access::RO;
            } else if (access_str == "wo" || access_str == "WO") {
                access = tlm::gpu::PcieBarRouter::Access::WO;
            }

            auto side = tlm::gpu::PcieBarRouter::SideEffect::NONE;
            if (side_str == "doorbell") {
                side = tlm::gpu::PcieBarRouter::SideEffect::DOORBELL;
            }

            const uint32_t stream_id = reg_json.value("stream_id", 0u);
            bar_router_.add_register(offset, name, access, side, stream_id);
        }
    }

    // 解析 msix_table_size
    if (cfg.contains("msix_table_size")) {
        msix_init(cfg["msix_table_size"].get<uint32_t>(), 0);
    }
}

// ==================== mmio_write ====================
// D3hot gate 时返 -EIO（所有 BAR，无 doorbell 例外 per V-5 决议）
int PcieMockIP::mmio_write(uint8_t bar, uint64_t offset, const void* data, std::size_t len) {
    if (is_mmio_gated_) {
        return kMockEio;  // D3hot 下所有 BAR 写拒绝
    }

    if (bar >= 6) {
        return -1;  // 无效 BAR
    }

    if (offset >= bar_sizes_[bar]) {
        return -1;  // 越界
    }

    // 4 字节粒度写入（对齐 PcieEndpointIP 行为）
    const uint64_t key = bar_key(bar, offset);
    uint32_t val = 0;
    if (data && len > 0) {
        std::memcpy(&val, data, std::min(len, sizeof(val)));
    }
    bar_regs_[key] = val;

    return 0;  // success
}

// ==================== mmio_read ====================
int PcieMockIP::mmio_read(uint8_t bar, uint64_t offset, void* buf, std::size_t len) {
    if (bar >= 6) {
        return -1;
    }

    if (offset >= bar_sizes_[bar]) {
        return -1;
    }

    // 4 字节粒度读取
    const uint64_t key = bar_key(bar, offset);
    const auto it = bar_regs_.find(key);
    uint32_t val = (it != bar_regs_.end()) ? static_cast<uint32_t>(it->second) : 0;

    if (buf && len > 0) {
        std::memset(buf, 0, len);
        std::memcpy(buf, &val, std::min(len, sizeof(val)));
    }

    return 0;  // success
}

// ==================== pcie_config_write ====================
// 仅处理 PMCSR (offset 0x44) 的 PowerState Write (低位 2bit)
int PcieMockIP::pcie_config_write(uint16_t offset, uint8_t width, uint32_t value) {
    if (offset == 0x44 && width >= 1) {
        // PMCSR 低 2 bit: PowerState (0=D0, 3=D3hot)
        // 忽略 D1(1)/D2(2) 保留值 (对齐 PcieEndpointIP::install_pm_capability)
        const uint8_t new_pws = value & 0x3;
        if (new_pws == 0) {
            is_mmio_gated_ = false;  // D0
        } else if (new_pws == 3) {
            is_mmio_gated_ = true;   // D3hot
        }
        // D1/D2: 忽略
        return 0;
    }

    // 其他 offset: 假设接受 (mock 不模拟完整 Config Space)
    return 0;
}

// ==================== pcie_config_read ====================
int PcieMockIP::pcie_config_read(uint16_t offset, uint8_t width, uint32_t& value) {
    value = 0;

    if (offset == 0x44 && width >= 1) {
        // PMCSR: 返回当前 power state
        value = is_mmio_gated_ ? 0x3 : 0x0;
        return 0;
    }

    // 其他 offset: 返 0
    return 0;
}

// ==================== MSI-X ====================
int PcieMockIP::msix_init(uint32_t table_size, uint32_t /*mask*/) {
    msix_pending_.resize(table_size, 0);
    return 0;
}

int PcieMockIP::msix_update_pending(uint32_t vector) {
    if (vector >= msix_pending_.size()) {
        return -1;  // 越界
    }

    msix_pending_[vector] = 1;

    // 直接回调（无 TLP 投递链）
    if (intr_cb_) {
        intr_cb_(vector, 0);
    }

    return 0;
}

int PcieMockIP::msix_clear_pending(uint32_t vector) {
    if (vector >= msix_pending_.size()) {
        return -1;
    }
    msix_pending_[vector] = 0;
    return 0;
}

void PcieMockIP::register_msi_callback(
    std::function<void(uint32_t vector, uint32_t trans_id)> cb) {
    intr_cb_ = std::move(cb);
}

// ==================== Device-Side AXI ====================
void PcieMockIP::device_axi_write(uint64_t addr, const void* data, std::size_t len) {
    bundles::Axi4Bundle req;
    req.awaddr.write(addr);
    req.awlen.write(static_cast<uint8_t>((len + 63) / 64));  // burst length
    req.awsize.write(3);  // 8 bytes (2^3)
    req.awburst.write(1);  // INCR
    req.awid.write(1);     // default ID

    // 写数据 (仅低 64 bit)
    uint64_t val = 0;
    if (data && len > 0) {
        std::memcpy(&val, data, std::min(len, sizeof(val)));
    }
    req.wdata.write(val);
    req.wstrb.write((len >= 8) ? 0xFFULL : ((1ULL << len) - 1));
    req.wlast.write(1);

    // 推入 Axi4StreamAdapter (master_req 方向: EP → SoC)
    axi_adapter_.master_req(req, true);
}

// ==================== 查询 API ====================
bool PcieMockIP::axi_master_req_valid() const {
    return axi_adapter_.master_req_valid();
}

const bundles::Axi4Bundle& PcieMockIP::axi_master_req_data() const {
    return axi_adapter_.master_req_data();
}

} // namespace cpptlm::pcie